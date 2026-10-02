# src/ph7/compile_class.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3118/3751 lines (83.12%)

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
|        - |    8 | `/* Forward declaration — deferred class declarations (defined with the deferral` |
|        - |    9 | ` * helpers ahead of GenStateCompileClassEx; used by the interface/trait` |
|        - |   10 | ` * compilers that precede them in this file). */` |
|        - |   11 | `static int GenStateMaybeDeferClass(ph7_gen_state *pGen,sxi32 iFlags,int iSelfKind,sxi32 *pRc);` |
|        - |   12 | `/*` |
|        - |   13 | ` * Section:` |
|        - |   14 | ` *    Class/OO compilation: classes, interfaces, traits, enums, anonymous` |
|        - |   15 | ` *    classes, class constants, typed properties, property hooks and methods.` |
|        - |   16 | ` * Status:` |
|        - |   17 | ` *    Stable.` |
|        - |   18 | ` */` |
|       16 |   19 | `static const char * GenStateClassKind(const ph7_class *pClass)` |
|        4 |   20 | `{` |
|       20 |   21 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){ return "interface"; }` |
|       18 |   22 | `	if( pClass->iFlags & PH7_CLASS_TRAIT ){ return "trait"; }` |
|       16 |   23 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){ return "enum"; }` |
|       14 |   24 | `	return "class";` |
|       12 |   25 | `}` |
|        - |   26 | `/*` |
|        - |   27 | ` * Guard a class/interface/trait/enum about to be installed. Returns SXERR_ABORT` |
|        - |   28 | ` * (after emitting the fatal) if it redeclares an already-bound type; otherwise` |
|        - |   29 | ` * marks it bound (when unconditional & top-level) and returns SXRET_OK.` |
|        - |   30 | ` */` |
|        - |   31 | `/*` |
|        - |   32 | ` * Does this earlier declaration hold the name against the one being compiled?` |
|        - |   33 | ` *` |
|        - |   34 | ` * A class php EARLY-BINDS holds it from the moment the file compiles, so` |
|        - |   35 | ` * anything else under that name is a redeclaration wherever it sits. One php` |
|        - |   36 | ` * declares at RUN time (it implements an interface, uses a trait, is an enum,` |
|        - |   37 | ` * or declares __toString and so implicitly implements Stringable) holds it only` |
|        - |   38 | ` * once its own statement has run -- which, within one file, means only against` |
|        - |   39 | ` * a declaration that comes AFTER it. That is the whole of the "polyfill" shape:` |
|        - |   40 | ` *` |
|        - |   41 | ` *     if (PHP_VERSION_ID >= 80000) { class T extends PhpToken {} return; }` |
|        - |   42 | ` *     class T { public function __toString(): string { ... } }` |
|        - |   43 | ` *` |
|        - |   44 | ` * php runs the first branch, returns, and never reaches the second -- so the` |
|        - |   45 | ` * second never takes the name. Reading the two declarations' ORDER is how this` |
|        - |   46 | ` * compiler tells that apart without a runtime declaration of its own.` |
|        - |   47 | ` */` |
|       16 |   48 | `static int GenStateDeclHoldsName(ph7_gen_state *pGen,ph7_class *pPrev,ph7_class *pClass)` |
|        4 |   49 | `{` |
|        - |   50 | `	SyString *pFile;` |
|       20 |   51 | `	if( pPrev->iFlags & PH7_CLASS_BOUND ){` |
|       18 |   52 | `		return 1;   /* early-bound: the name is taken before anything runs */` |
|        - |   53 | `	}` |
|        3 |   54 | `	if( (pPrev->iFlags & PH7_CLASS_TOPLEVEL) == 0 ){` |
|      ! 0 |   55 | `		return 0;   /* conditional: it may never run at all */` |
|        - |   56 | `	}` |
|        3 |   57 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|        2 |   58 | `	if( pFile && pPrev->sFile.nByte == pFile->nByte` |
|        3 |   59 | `	 && SyMemcmp(pPrev->sFile.zString,pFile->zString,pFile->nByte) == 0 ){` |
|        - |   60 | `		/* Same file: it holds the name only if it is written FIRST. */` |
|        3 |   61 | `		return pPrev->nLine <= pClass->nLine;` |
|        - |   62 | `	}` |
|      ! 0 |   63 | `	return 1;   /* another file, already loaded: it has run */` |
|       12 |   64 | `}` |
|        - |   65 | `/*` |
|        - |   66 | `` * A `phl -l` compile PARSES; it does not BIND. php binds inheritance at run time,`` |
|        - |   67 | `` * so `php -l` says nothing about a parent, an interface or a trait it cannot see --`` |
|        - |   68 | ` * and under -l nothing autoloads, so it can see almost none of them. TRUE means` |
|        - |   69 | ` * "say nothing and carry on with no base", which is what leaves the BODY to be` |
|        - |   70 | ` * compiled: the whole point of the mode is that a syntax error in there is found.` |
|        - |   71 | ` */` |
|       22 |   72 | `static int GenStateLintSkipsBase(ph7_gen_state *pGen,ph7_class *pClass)` |
|        1 |   73 | `{` |
|       23 |   74 | `	if( pGen->pVm->bSyntaxCheck == 0 ){` |
|      ! 0 |   75 | `		return 0;` |
|        - |   76 | `	}` |
|       23 |   77 | `	if( pClass ){` |
|       23 |   78 | `		pClass->iFlags \|= PH7_CLASS_LINT_UNBOUND;` |
|       11 |   79 | `	}` |
|       23 |   80 | `	return 1;` |
|       12 |   81 | `}` |
|     5794 |   82 | `static sxi32 GenStateGuardClassRedeclaration(ph7_gen_state *pGen,ph7_class *pClass,` |
|        - |   83 | `	int bEarlyBindable)` |
|        5 |   84 | `{` |
|        - |   85 | `	SyHashEntry *pEntry;` |
|     5799 |   86 | `	int bTopLevel = GenStateUnconditionalTopLevel(pGen);` |
|     5799 |   87 | `	if( pGen->pVm->bSyntaxCheck ){` |
|        - |   88 | ``		/* `php -l` reports a redeclared FUNCTION and not a redeclared CLASS: a`` |
|        - |   89 | `		 * class name is taken at the DECLARE_CLASS opcode, which lint never runs.` |
|        - |   90 | ``		 * So `class DateTime {}` and symfony/polyfill-php80's stub `final class`` |
|        - |   91 | ``		 * Attribute` -- a file composer only ever loads under php 7 -- both lint`` |
|        - |   92 | `		 * clean there, and this refused them. */` |
|       90 |   93 | `		return SXRET_OK;` |
|        - |   94 | `	}` |
|     5711 |   95 | `	if( bTopLevel ){` |
|        - |   96 | `		/* php RUNS this declaration whatever else the file holds, early bound or` |
|        - |   97 | `		 * not -- so two of them under one name collide even when neither was. */` |
|     5609 |   98 | `		pClass->iFlags \|= PH7_CLASS_TOPLEVEL;` |
|     2802 |   99 | `	}` |
|     5711 |  100 | `	if( bTopLevel && !bEarlyBindable ){` |
|        - |  101 | `		/* Not early-bound, but it RUNS: only another declaration that also runs` |
|        - |  102 | `		 * unconditionally collides with it. */` |
|     1514 |  103 | `		SyHashEntry *pTop = SyHashGet(&pGen->pVm->hClass,` |
|     1006 |  104 | `			(const void *)pClass->sName.zString,pClass->sName.nByte);` |
|     1011 |  105 | `		if( pTop ){` |
|        3 |  106 | `			ph7_class *pPrev = (ph7_class *)pTop->pUserData;` |
|        3 |  107 | `			while( pPrev ){` |
|        3 |  108 | `				if( GenStateDeclHoldsName(pGen,pPrev,pClass) ){` |
|        3 |  109 | `					pGen->iFatalTrace = PH7_FATAL_TRACE_RUNTIME;` |
|        3 |  110 | `					if( pPrev->sFile.nByte > 0 ){` |
|        4 |  111 | `						PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,` |
|        - |  112 | `							"Cannot redeclare %s %z (previously declared in %.*s:%u)",` |
|        1 |  113 | `							GenStateClassKind(pPrev),&pClass->sDisp,` |
|        1 |  114 | `							pPrev->sFile.nByte,pPrev->sFile.zString,pPrev->nLine);` |
|        2 |  115 | `					}else{` |
|      ! 0 |  116 | `						PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,` |
|      ! 0 |  117 | `							"Cannot redeclare %s %z",GenStateClassKind(pPrev),&pClass->sDisp);` |
|        - |  118 | `					}` |
|        3 |  119 | `					return SXERR_ABORT;` |
|        - |  120 | `				}` |
|      ! 0 |  121 | `				pPrev = pPrev->pNextName;` |
|      ! 0 |  122 | `			}` |
|      ! 0 |  123 | `		}` |
|     1009 |  124 | `		return SXRET_OK;` |
|        - |  125 | `	}` |
|     4705 |  126 | `	if( !bTopLevel ){` |
|        - |  127 | `		/* Conditional, nested -- or one php would not EARLY-BIND either, which` |
|        - |  128 | `		 * is the same thing for this guard: such a declaration only takes effect` |
|        - |  129 | `		 * when its statement runs, so another declaration of the name is not a` |
|        - |  130 | `		 * redeclaration of it. php early-binds only a class it can link with` |
|        - |  131 | `		 * nothing left over, so an implemented INTERFACE, a used TRAIT, an enum` |
|        - |  132 | ``		 * or a `__toString()` (which brings Stringable with it) all rule it out.`` |
|        - |  133 | `		 * Binding one of those at compile time made the polyfill shape above a` |
|        - |  134 | `		 * redeclaration, and phpunit.phar died on it. */` |
|      107 |  135 | `		return SXRET_OK;` |
|        - |  136 | `	}` |
|     4603 |  137 | `	pClass->iFlags \|= PH7_CLASS_BOUND;` |
|     4603 |  138 | `	if( pGen->pVm->bCompilingBuiltin ){` |
|      ! 0 |  139 | `		return SXRET_OK; /* the prelude installs each builtin exactly once */` |
|        - |  140 | `	}` |
|     4603 |  141 | `	pEntry = SyHashGet(&pGen->pVm->hClass,(const void *)pClass->sName.zString,pClass->sName.nByte);` |
|     4603 |  142 | `	if( pEntry ){` |
|       18 |  143 | `		ph7_class *pPrev = (ph7_class *)pEntry->pUserData;` |
|       18 |  144 | `		while( pPrev ){` |
|       18 |  145 | `			if( GenStateDeclHoldsName(pGen,pPrev,pClass) ){` |
|        - |  146 | `				/* php cannot early-bind a name it already holds, so THIS refusal comes` |
|        - |  147 | `				 * from the DECLARE_CLASS opcode at run time -- and its stack trace` |
|        - |  148 | `				 * carries the include/require that loaded the unit, where every other` |
|        - |  149 | `				 * compile-time refusal's does not. */` |
|       18 |  150 | `				pGen->iFatalTrace = PH7_FATAL_TRACE_RUNTIME;` |
|        - |  151 | `				/* php names the entity by the PREVIOUS declaration's kind and omits` |
|        - |  152 | `				 * the "(previously declared in ...)" clause for internal symbols. */` |
|       18 |  153 | `				if( pPrev->sFile.nByte > 0 ){` |
|       22 |  154 | `					PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,` |
|        - |  155 | `						"Cannot redeclare %s %z (previously declared in %.*s:%u)",` |
|        6 |  156 | `						GenStateClassKind(pPrev),&pClass->sDisp,` |
|        6 |  157 | `						pPrev->sFile.nByte,pPrev->sFile.zString,pPrev->nLine);` |
|       10 |  158 | `				}else{` |
|        4 |  159 | `					PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,` |
|        1 |  160 | `						"Cannot redeclare %s %z",GenStateClassKind(pPrev),&pClass->sDisp);` |
|        - |  161 | `				}` |
|       18 |  162 | `				return SXERR_ABORT;` |
|        - |  163 | `			}` |
|      ! 0 |  164 | `			pPrev = pPrev->pNextName;` |
|      ! 0 |  165 | `		}` |
|      ! 0 |  166 | `	}` |
|     4589 |  167 | `	return SXRET_OK;` |
|     2902 |  168 | `}` |
|        - |  169 | `/*` |
|        - |  170 | ` * Extract the visibility level associated with a given keyword.` |
|        - |  171 | ` * According to the PHP language reference manual` |
|        - |  172 | ` *  Visibility:` |
|        - |  173 | ` *  The visibility of a property or method can be defined by prefixing` |
|        - |  174 | ` *  the declaration with the keywords public, protected or private.` |
|        - |  175 | ` *  Class members declared public can be accessed everywhere.` |
|        - |  176 | ` *  Members declared protected can be accessed only within the class` |
|        - |  177 | ` *  itself and by inherited and parent classes. Members declared as private` |
|        - |  178 | ` *  may only be accessed by the class that defines the member.` |
|        - |  179 | ` */` |
|     8432 |  180 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|        5 |  181 | `{` |
|     8437 |  182 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|      605 |  183 | `		return PH7_CLASS_PROT_PRIVATE;` |
|     7837 |  184 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|      249 |  185 | `		return PH7_CLASS_PROT_PROTECTED;` |
|        - |  186 | `	}` |
|        - |  187 | `	/* Assume public by default */` |
|     7593 |  188 | `	return PH7_CLASS_PROT_PUBLIC;` |
|     4221 |  189 | `}` |
|        - |  190 | `/*` |
|        - |  191 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|        - |  192 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|        - |  193 | ` * token immediately followed by '='. Anything else with a leading type token` |
|        - |  194 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|        - |  195 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|        - |  196 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|        - |  197 | ` */` |
|      668 |  198 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|        5 |  199 | `{` |
|        - |  200 | `	SyToken *p0, *p1;` |
|      673 |  201 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  202 | `		return 0;` |
|        - |  203 | `	}` |
|      673 |  204 | `	p0 = pGen->pIn;` |
|        - |  205 | `	/* A leading '\' (namespaced class type), '?' (nullable) or '(' (a DNF type's` |
|        - |  206 | `	 * first intersection group) always starts a type -- none of the three can begin` |
|        - |  207 | `	 * a constant NAME. */` |
|      673 |  208 | `	if( p0->nType & (PH7_TK_NSSEP\|PH7_TK_LPAREN) ){` |
|        3 |  209 | `		return 1;` |
|        - |  210 | `	}` |
|      671 |  211 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|        5 |  212 | `		return 1;` |
|        - |  213 | `	}` |
|        - |  214 | `	/* A name-like first token begins a type only when followed by another` |
|        - |  215 | `	 * name (the constant name), a union separator '\|' or an intersection '&'.` |
|        - |  216 | `	 * Followed by '=', ';' or ',' it is the constant name itself (untyped).` |
|        - |  217 | `	 * Without the '&' a typed constant whose type is an INTERSECTION was read as` |
|        - |  218 | `	 * an untyped one named after the first member, and refused with` |
|        - |  219 | `	 * "Expected '=' after class constant Countable". */` |
|      667 |  220 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      667 |  221 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|      667 |  222 | `		if( p1 ){` |
|      667 |  223 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP\|PH7_TK_AMPER) ){` |
|       53 |  224 | `				return 1;` |
|        - |  225 | `			}` |
|      614 |  226 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1` |
|      619 |  227 | `			 && (p1->sData.zString[0] == '\|' \|\| p1->sData.zString[0] == '&') ){` |
|        5 |  228 | `				return 1;` |
|        - |  229 | `			}` |
|      305 |  230 | `		}` |
|      305 |  231 | `	}` |
|      615 |  232 | `	return 0;` |
|      339 |  233 | `}` |
|        - |  234 | `/*` |
|        - |  235 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|        - |  236 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|        - |  237 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|        - |  238 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|        - |  239 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|        - |  240 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|        - |  241 | ` * Peek only; never consumes tokens.` |
|        - |  242 | ` */` |
|       36 |  243 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|        4 |  244 | `{` |
|       40 |  245 | `	SyToken *p = pGen->pIn;` |
|       57 |  246 | `	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|       26 |  247 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|        3 |  248 | `		p++; /* skip leading unary sign(s) */` |
|        1 |  249 | `	}` |
|       40 |  250 | `	if( p >= pGen->pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|       36 |  251 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|        - |  252 | `	}` |
|        6 |  253 | `	p++;` |
|        - |  254 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|        6 |  255 | `	return ( p >= pGen->pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|       22 |  256 | `}` |
|        - |  257 | `/*` |
|        - |  258 | `` * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).`` |
|        - |  259 | `` * A `new` that immediately follows one of these is a member name (`A::new`,`` |
|        - |  260 | `` * `$o->new`), not a `new` expression.`` |
|        - |  261 | ` */` |
|      252 |  262 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|        5 |  263 | `{` |
|        - |  264 | `	sxi32 iOp;` |
|      257 |  265 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|       92 |  266 | `		return 0;` |
|        - |  267 | `	}` |
|      167 |  268 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|      167 |  269 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|      131 |  270 | `}` |
|        - |  271 | `/*` |
|        - |  272 | ` * Skip the whole closure / arrow-function construct beginning at *pp, which is` |
|        - |  273 | `` * positioned on the `function` / `fn` keyword. Its body is ordinary runtime code,`` |
|        - |  274 | ` * so none of the constant-expression rules below reach into it. *piDepth is the` |
|        - |  275 | ` * caller's bracket depth: an arrow function has no braces of its own and ends at` |
|        - |  276 | `` * a `,`/`;` or at a bracket closing an ENCLOSING group, so it shares that depth,`` |
|        - |  277 | `` * while a `function(){...}` body is brace-balanced and walks its own.`` |
|        - |  278 | ` */` |
|      192 |  279 | `static void GenStateInitSkipFuncConstruct(SyToken **pp,SyToken *pEnd,int *piDepth)` |
|        5 |  280 | `{` |
|      197 |  281 | `	SyToken *p = *pp;` |
|      197 |  282 | `	int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|      197 |  283 | `	int iBase = *piDepth;` |
|      197 |  284 | `	p++;` |
|      197 |  285 | `	if( bArrow ){` |
|        - |  286 | `		/* fn(params) => expr : skip to the end of the current element. */` |
|       86 |  287 | `		while( p < pEnd ){` |
|       78 |  288 | `			if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       14 |  289 | `				(*piDepth)++;` |
|       72 |  290 | `			}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       14 |  291 | `				if( *piDepth <= iBase ){` |
|      ! 0 |  292 | `					break; /* closes an enclosing group, not the fn's own */` |
|        - |  293 | `				}` |
|       14 |  294 | `				(*piDepth)--;` |
|       60 |  295 | `			}else if( *piDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|        6 |  296 | `				break;` |
|        - |  297 | `			}` |
|       74 |  298 | `			p++;` |
|        2 |  299 | `		}` |
|        8 |  300 | `	}else{` |
|        - |  301 | `		/* function(params)[use(...)][: type] { body } : skip the signature up to the` |
|        - |  302 | ``		 * body '{' (a '{' at closure-local depth 0, so a `new class{}` default inside`` |
|        - |  303 | `		 * the parens is not mistaken for it), then the balanced brace block. */` |
|      185 |  304 | `		int iLocal = 0;` |
|      545 |  305 | `		while( p < pEnd ){` |
|      545 |  306 | `			if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|      185 |  307 | `				break; /* body brace */` |
|        - |  308 | `			}` |
|      365 |  309 | `			if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      185 |  310 | `				iLocal++;` |
|      275 |  311 | `			}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      185 |  312 | `				if( iLocal > 0 ){` |
|      185 |  313 | `					iLocal--;` |
|       90 |  314 | `				}` |
|       90 |  315 | `			}` |
|      365 |  316 | `			p++;` |
|        5 |  317 | `		}` |
|      185 |  318 | `		if( p < pEnd ){` |
|      185 |  319 | `			int iBrace = 0; /* p is on the body '{' */` |
|     1217 |  320 | `			while( p < pEnd ){` |
|     1217 |  321 | `				if( p->nType & PH7_TK_OCB ){` |
|      205 |  322 | `					iBrace++;` |
|     1117 |  323 | `				}else if( p->nType & PH7_TK_CCB ){` |
|      205 |  324 | `					iBrace--;` |
|      205 |  325 | `					if( iBrace == 0 ){` |
|      185 |  326 | `						p++;` |
|      185 |  327 | `						break;` |
|        - |  328 | `					}` |
|       10 |  329 | `				}` |
|     1037 |  330 | `				p++;` |
|        5 |  331 | `			}` |
|       90 |  332 | `		}` |
|        - |  333 | `	}` |
|      197 |  334 | `	*pp = p;` |
|      197 |  335 | `}` |
|        - |  336 | `/*` |
|        - |  337 | `` * TRUE if *p is the `::` operator.`` |
|        - |  338 | ` */` |
|    67506 |  339 | `static int GenStateTokenIsDoubleColon(const SyToken *p)` |
|        5 |  340 | `{` |
|    42498 |  341 | `	return ( (p->nType & PH7_TK_OP) && p->pUserData` |
|    71893 |  342 | `		&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_DC );` |
|        5 |  343 | `}` |
|        - |  344 | `/*` |
|        - |  345 | `` * Where the constant expression starting at *pStart ends: the first `,` or `;` at`` |
|        - |  346 | `` * bracket depth 0, or the first depth-0 `{`, which in a property declaration can`` |
|        - |  347 | `` * only open a PHP 8.4 hook list (`public T $x = default { get …; }`) and never`` |
|        - |  348 | ` * belongs to the default itself.` |
|        - |  349 | ` */` |
|    51572 |  350 | `static SyToken * GenStateConstExprEnd(SyToken *pStart,SyToken *pEnd)` |
|        5 |  351 | `{` |
|    51577 |  352 | `	SyToken *p = pStart;` |
|    51577 |  353 | `	int iDepth = 0;` |
|   119329 |  354 | `	while( p < pEnd ){` |
|    70632 |  355 | `		if( (p->nType & PH7_TK_KEYWORD) && (p->nType & PH7_TK_MEMBER_NAME) == 0` |
|      235 |  356 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|      208 |  357 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|        - |  358 | `			/* A closure's BODY brace is not a hook list: skip the construct whole,` |
|        - |  359 | ``			 * or `$c = static function(){}` would end the expression at that brace`` |
|        - |  360 | `			 * and hide everything after it from the rules. */` |
|       53 |  361 | `			GenStateInitSkipFuncConstruct(&p,pEnd,&iDepth);` |
|       53 |  362 | `			continue;` |
|        - |  363 | `		}` |
|    70589 |  364 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA\|PH7_TK_OCB)) ){` |
|     2885 |  365 | `			break;` |
|        - |  366 | `		}` |
|    67709 |  367 | `		if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      473 |  368 | `			iDepth++;` |
|    67475 |  369 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      473 |  370 | `			if( iDepth > 0 ){` |
|      473 |  371 | `				iDepth--;` |
|      234 |  372 | `			}` |
|      234 |  373 | `		}` |
|    67709 |  374 | `		p++;` |
|        5 |  375 | `	}` |
|    51577 |  376 | `	return p;` |
|        5 |  377 | `}` |
|        - |  378 | `/*` |
|        - |  379 | ` * Decide a ternary CONDITION php would have folded: 1 truthy, 0 falsy, -1 "cannot` |
|        - |  380 | ` * tell". Only a lone literal (with any number of redundant parens around it) is` |
|        - |  381 | `` * decided here. php's own folder reaches much further -- it evaluates `1 === 1`,`` |
|        - |  382 | `` * `PHP_INT_SIZE === 8`, even `[1,2][0]` -- and everything it cannot fold at compile`` |
|        - |  383 | ` * time, a user constant among them, it leaves for the rule walk to refuse. The` |
|        - |  384 | ` * caller answers -1 by leaving the whole expression alone, which is the safe` |
|        - |  385 | ` * direction: an offender php would have refused stays accepted, and no valid` |
|        - |  386 | ` * program is refused on a branch php would have dropped.` |
|        - |  387 | ` */` |
|       22 |  388 | `static int GenStateConstExprTruth(SyToken *pStart,SyToken *pStop)` |
|        2 |  389 | `{` |
|       24 |  390 | `	SyToken *p = pStart, *q = pStop;` |
|       24 |  391 | `	while( p < q && (p->nType & PH7_TK_LPAREN) && (q[-1].nType & PH7_TK_RPAREN) ){` |
|      ! 0 |  392 | `		p++;` |
|      ! 0 |  393 | `		q--;` |
|      ! 0 |  394 | `	}` |
|       24 |  395 | `	if( &p[1] != q ){` |
|      ! 0 |  396 | `		return -1; /* not a lone token */` |
|        - |  397 | `	}` |
|       24 |  398 | `	if( p->nType & PH7_TK_ID ){` |
|        - |  399 | ``		/* `true`/`false`/`null` are not lexer keywords here -- they arrive as plain`` |
|        - |  400 | `		 * identifiers and are recognised by name (php reserves all three, so no user` |
|        - |  401 | `		 * constant can shadow one). Any OTHER name is a constant whose value this` |
|        - |  402 | `		 * stage does not know, which is exactly where php stops folding too. */` |
|       24 |  403 | `		if( p->sData.nByte == 4 && SyStrnicmp(p->sData.zString,"true",4) == 0 ){` |
|       16 |  404 | `			return 1;` |
|        - |  405 | `		}` |
|        8 |  406 | `		if( (p->sData.nByte == 5 && SyStrnicmp(p->sData.zString,"false",5) == 0)` |
|        5 |  407 | `			\|\| (p->sData.nByte == 4 && SyStrnicmp(p->sData.zString,"null",4) == 0) ){` |
|        9 |  408 | `			return 0;` |
|        - |  409 | `		}` |
|      ! 0 |  410 | `		return -1;` |
|        - |  411 | `	}` |
|      ! 0 |  412 | `	if( p->nType & (PH7_TK_INTEGER\|PH7_TK_REAL) ){` |
|        - |  413 | `		/* php's truthiness: 0 and 0.0 are false, everything else true. */` |
|      ! 0 |  414 | `		const char *z = p->sData.zString;` |
|      ! 0 |  415 | `		sxu32 n = p->sData.nByte, i;` |
|      ! 0 |  416 | `		for( i = 0 ; i < n ; ++i ){` |
|      ! 0 |  417 | `			if( z[i] != '0' && z[i] != '.' && z[i] != '+' && z[i] != '-' ){` |
|      ! 0 |  418 | `				return 1;` |
|        - |  419 | `			}` |
|      ! 0 |  420 | `		}` |
|      ! 0 |  421 | `		return 0;` |
|        - |  422 | `	}` |
|      ! 0 |  423 | `	if( p->nType & PH7_TK_DSTR ){` |
|        - |  424 | `		/* A double-quoted literal may interpolate; only a single-quoted one is` |
|        - |  425 | `		 * certainly its own text. Leave the rest undecided. */` |
|      ! 0 |  426 | `		return -1;` |
|        - |  427 | `	}` |
|      ! 0 |  428 | `	if( p->nType & PH7_TK_SSTR ){` |
|      ! 0 |  429 | `		return ( p->sData.nByte == 0` |
|      ! 0 |  430 | `			\|\| (p->sData.nByte == 1 && p->sData.zString[0] == '0') ) ? 0 : 1;` |
|        - |  431 | `	}` |
|      ! 0 |  432 | `	return -1;` |
|       13 |  433 | `}` |
|        - |  434 | `static const char * GenStateConstExprSpan(SyToken *pStart,SyToken *pStop,int bAllowNew,int nDepth);` |
|        - |  435 | `/*` |
|        - |  436 | `` * TRUE if a `?` appears anywhere in the span outside a closure body. Used only`` |
|        - |  437 | ` * after the top-level ternary has been ruled out: a ternary NESTED in a bracket` |
|        - |  438 | `` * (`[true ? 1 : new X]`, `(true ? 1 : strlen('a')) + 1`) is folded by php just the`` |
|        - |  439 | ` * same, but the split below cannot say where its condition begins, so the span is` |
|        - |  440 | `` * left alone rather than have a dropped branch refuse a valid program. A `?` in a`` |
|        - |  441 | `` * closure body is runtime code and does not count; `?->` and `??` are their own`` |
|        - |  442 | ` * operators and never land here.` |
|        - |  443 | ` */` |
|    51594 |  444 | `static int GenStateSpanHasNestedTernary(SyToken *pStart,SyToken *pStop)` |
|        5 |  445 | `{` |
|    51599 |  446 | `	SyToken *p = pStart;` |
|    51599 |  447 | `	int iDepth = 0;` |
|   119207 |  448 | `	while( p < pStop ){` |
|    67616 |  449 | `		if( (p->nType & PH7_TK_KEYWORD) && (p->nType & PH7_TK_MEMBER_NAME) == 0` |
|      223 |  450 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|      200 |  451 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|       45 |  452 | `			GenStateInitSkipFuncConstruct(&p,pStop,&iDepth);` |
|       45 |  453 | `			continue;` |
|        - |  454 | `		}` |
|    67576 |  455 | `		if( (p->nType & PH7_TK_OP) && p->pUserData` |
|     8793 |  456 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_QUESTY ){` |
|        9 |  457 | `			return 1;` |
|        - |  458 | `		}` |
|    67573 |  459 | `		p++;` |
|        5 |  460 | `	}` |
|    51591 |  461 | `	return 0;` |
|    25768 |  462 | `}` |
|        - |  463 | `/*` |
|        - |  464 | ` * Split a constant expression at its top-level ternary, the way php's constant` |
|        - |  465 | ` * folder does: the condition decides which branch survives, and only the surviving` |
|        - |  466 | `` * one is subject to the rules. `true ? 1 : new X` and `false ? new X : 1` are both`` |
|        - |  467 | ` * legal php for exactly this reason, and scanning the dropped branch refused valid` |
|        - |  468 | ` * programs. Answers 1 when it handled the span (writing the verdict to *pzErr).` |
|        - |  469 | ` */` |
|    51616 |  470 | `static int GenStateConstExprTernary(SyToken *pStart,SyToken *pStop,int bAllowNew,` |
|        - |  471 | `	int nDepth,const char **pzErr)` |
|        5 |  472 | `{` |
|    51621 |  473 | `	SyToken *p = pStart, *pQ = 0, *pColon = 0;` |
|    51621 |  474 | `	int iDepth = 0, iNest = 0, iTruth;` |
|   119359 |  475 | `	while( p < pStop ){` |
|    67760 |  476 | `		if( (p->nType & PH7_TK_KEYWORD) && (p->nType & PH7_TK_MEMBER_NAME) == 0` |
|      229 |  477 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|      204 |  478 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|        - |  479 | `			/* A ternary inside a closure body -- or an ARROW function's whole body,` |
|        - |  480 | `			 * which carries no braces to raise the depth -- is runtime code, not this` |
|        - |  481 | ``			 * expression's ternary. Reading `fn() => true ? 1 : 2` as one folded away`` |
|        - |  482 | `			 * the arrow that the rules exist to refuse. */` |
|       49 |  483 | `			GenStateInitSkipFuncConstruct(&p,pStop,&iDepth);` |
|       49 |  484 | `			continue;` |
|        - |  485 | `		}` |
|    67721 |  486 | `		if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      471 |  487 | `			iDepth++;` |
|    67488 |  488 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      471 |  489 | `			if( iDepth > 0 ){` |
|      471 |  490 | `				iDepth--;` |
|      233 |  491 | `			}` |
|    67022 |  492 | `		}else if( iDepth == 0 && (p->nType & PH7_TK_OP) && p->pUserData` |
|     2273 |  493 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_QUESTY ){` |
|       24 |  494 | `			if( pQ == 0 ){` |
|       24 |  495 | `				pQ = p;` |
|       11 |  496 | `			}` |
|       24 |  497 | `			iNest++;` |
|    66778 |  498 | `		}else if( iDepth == 0 && pQ != 0 && (p->nType & PH7_TK_COLON) ){` |
|        - |  499 | ``			/* `::` is one DC operator token, not two colons, so it never lands here;`` |
|        - |  500 | ``			 * a nested ternary's own colon is matched against its own `?`. */`` |
|       24 |  501 | `			iNest--;` |
|       24 |  502 | `			if( iNest == 0 ){` |
|       24 |  503 | `				pColon = p;` |
|       24 |  504 | `				break;` |
|        - |  505 | `			}` |
|      ! 0 |  506 | `		}` |
|    67699 |  507 | `		p++;` |
|        5 |  508 | `	}` |
|    51621 |  509 | `	if( pQ == 0 ){` |
|    51599 |  510 | `		return 0; /* no top-level ternary: the linear walk owns this span */` |
|        - |  511 | `	}` |
|       24 |  512 | `	if( pColon == 0 ){` |
|      ! 0 |  513 | `		*pzErr = 0; /* unbalanced (the parser will say so); nothing to rule on here */` |
|      ! 0 |  514 | `		return 1;` |
|        - |  515 | `	}` |
|       24 |  516 | `	iTruth = GenStateConstExprTruth(pStart,pQ);` |
|       24 |  517 | `	if( iTruth < 0 ){` |
|      ! 0 |  518 | `		*pzErr = 0; /* php would fold what this cannot: leave the whole span alone */` |
|      ! 0 |  519 | `		return 1;` |
|        - |  520 | `	}` |
|       24 |  521 | `	*pzErr = GenStateConstExprSpan(pStart,pQ,bAllowNew,nDepth + 1);` |
|       24 |  522 | `	if( *pzErr == 0 ){` |
|        - |  523 | ``		/* `c ?: e` keeps the condition when it is truthy, so the then-span is empty. */`` |
|       24 |  524 | `		*pzErr = iTruth` |
|       14 |  525 | `			? GenStateConstExprSpan(&pQ[1],pColon,bAllowNew,nDepth + 1)` |
|       15 |  526 | `			: GenStateConstExprSpan(&pColon[1],pStop,bAllowNew,nDepth + 1);` |
|       11 |  527 | `	}` |
|       24 |  528 | `	return 1;` |
|    25779 |  529 | `}` |
|        - |  530 | `/*` |
|        - |  531 | ` * Every compile-time rule php applies to a CONSTANT EXPRESSION, over one token span,` |
|        - |  532 | ` * as a single left-to-right walk that answers with the FIRST offender's sentence --` |
|        - |  533 | ` * which is exactly the one php prints, because php walks the same expression in the` |
|        - |  534 | ` * same order and stops at the first node it refuses to compile. Run as separate` |
|        - |  535 | ` * passes, the rules got that order wrong whenever two kinds appeared together:` |
|        - |  536 | `` * `[strlen('a'), function(){}]` said "Closures in constant expressions must be`` |
|        - |  537 | ` * static" where php says "Constant expression contains invalid operations".` |
|        - |  538 | ` *` |
|        - |  539 | ` * The rules, in the order a token can trigger them:` |
|        - |  540 | ` *   - an ARROW function -- "Constant expression contains invalid operations". There` |
|        - |  541 | `` *     is no static-`fn` escape: `static fn()=>1` is refused too;`` |
|        - |  542 | ` *   - an IMMEDIATELY INVOKED closure -- a call is what php names it, so it takes the` |
|        - |  543 | ` *     call sentence and not the closure one;` |
|        - |  544 | ` *   - a NON-STATIC closure -- "Closures in constant expressions must be static".` |
|        - |  545 | `` *     `static function(){...}` is accepted and its body skipped, being runtime code;`` |
|        - |  546 | `` *   - a STATIC PROPERTY fetch (`C::$p`, `self::$p`, `static::$p`) -- php has no`` |
|        - |  547 | ` *     constant-expression node for one, so it is "invalid operations";` |
|        - |  548 | `` *   - `static::` -- "\"static::\" is not allowed in compile-time constants", with the`` |
|        - |  549 | ``  *     `static::class` face carrying php's own separate sentence. `self::`/`parent::` `` |
|        - |  550 | ` *     are fine: they name the DECLARING class, which is known where it is written;` |
|        - |  551 | ` *   - a CALL -- "Constant expression contains invalid operations". A first-class` |
|        - |  552 | `` *     callable (`strlen(...)`) is only an ellipsis in parens, and a constructor's`` |
|        - |  553 | `` *     argument list belongs to its `new`, so neither of those counts;`` |
|        - |  554 | `` *   - `new`, unless bAllowNew -- "New expressions are not supported in this context".`` |
|        - |  555 | ` */` |
|    51616 |  556 | `static const char * GenStateConstExprSpan(SyToken *pStart,SyToken *pStop,int bAllowNew,int nDepth)` |
|        5 |  557 | `{` |
|    51621 |  558 | `	SyToken *p = pStart;` |
|    51621 |  559 | `	int iDepth = 0;` |
|    51621 |  560 | `	const char *zTern = 0;` |
|    51621 |  561 | `	if( nDepth > 32 ){` |
|      ! 0 |  562 | `		return 0; /* pathological nesting: stop rather than recurse */` |
|        - |  563 | `	}` |
|    51621 |  564 | `	if( GenStateConstExprTernary(pStart,pStop,bAllowNew,nDepth,&zTern) ){` |
|       24 |  565 | `		return zTern;` |
|        - |  566 | `	}` |
|    51599 |  567 | `	if( GenStateSpanHasNestedTernary(pStart,pStop) ){` |
|        9 |  568 | `		return 0; /* php folds it and this cannot: leave the span alone */` |
|        - |  569 | `	}` |
|   119067 |  570 | `	while( p < pStop ){` |
|    67519 |  571 | `		if( (p->nType & PH7_TK_KEYWORD) && (p->nType & PH7_TK_MEMBER_NAME) == 0 ){` |
|      219 |  572 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(p->pUserData);` |
|      219 |  573 | `			if( nKw == PH7_TKWRD_FN ){` |
|        6 |  574 | `				return "Constant expression contains invalid operations";` |
|        - |  575 | `			}` |
|      215 |  576 | `			if( nKw == PH7_TKWRD_FUNCTION ){` |
|       38 |  577 | `				int iScan = iDepth;` |
|       38 |  578 | `				SyToken *q = p;` |
|        - |  579 | ``				/* `(function(){...})()` is a CALL to php, and a call is what it names --`` |
|        - |  580 | `				 * the closure rule never gets a say. Look past the construct and the` |
|        - |  581 | `				 * parens wrapping it for the argument list. */` |
|       38 |  582 | `				GenStateInitSkipFuncConstruct(&q,pStop,&iScan);` |
|       40 |  583 | `				while( q < pStop && (q->nType & PH7_TK_RPAREN) ){` |
|        3 |  584 | `					q++;` |
|        1 |  585 | `				}` |
|       38 |  586 | `				if( q < pStop && (q->nType & PH7_TK_LPAREN) ){` |
|        6 |  587 | `					return "Constant expression contains invalid operations";` |
|        - |  588 | `				}` |
|        - |  589 | ``				/* The `static` modifier sits in the token immediately before. */`` |
|       36 |  590 | `				if( !(p > pStart && (p[-1].nType & PH7_TK_KEYWORD)` |
|       26 |  591 | `					&& SX_PTR_TO_INT(p[-1].pUserData) == PH7_TKWRD_STATIC) ){` |
|        7 |  592 | `					return "Closures in constant expressions must be static";` |
|        - |  593 | `				}` |
|       30 |  594 | `				GenStateInitSkipFuncConstruct(&p,pStop,&iDepth);` |
|       30 |  595 | `				continue;` |
|        - |  596 | `			}` |
|      176 |  597 | `			if( nKw == PH7_TKWRD_STATIC && &p[1] < pStop` |
|       34 |  598 | `				&& GenStateTokenIsDoubleColon(&p[1])` |
|       25 |  599 | `				&& !(&p[2] < pStop && (p[2].nType & PH7_TK_DOLLAR)) ){` |
|        - |  600 | ``				/* `static::$p` is excluded above: it is a property fetch, which the`` |
|        - |  601 | ``				 * `::` rule below words php's way. */`` |
|        4 |  602 | `				if( &p[2] < pStop && (p[2].nType & PH7_TK_KEYWORD)` |
|        5 |  603 | `					&& SX_PTR_TO_INT(p[2].pUserData) == PH7_TKWRD_CLASS ){` |
|        3 |  604 | `					return "static::class cannot be used for compile-time class name resolution";` |
|        - |  605 | `				}` |
|        3 |  606 | `				return "\"static::\" is not allowed in compile-time constants";` |
|        - |  607 | `			}` |
|       86 |  608 | `		}` |
|    67477 |  609 | `		if( GenStateTokenIsDoubleColon(p) && &p[1] < pStop && (p[1].nType & PH7_TK_DOLLAR) ){` |
|        3 |  610 | `			return "Constant expression contains invalid operations";` |
|        - |  611 | `		}` |
|    67475 |  612 | `		if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        - |  613 | `			/* A '(' directly after a NAME is a call. A name here is an identifier` |
|        - |  614 | ``			 * token; `new X(` is excluded by walking back to the `new` operator, and`` |
|        - |  615 | ``			 * `f(...)` (first-class callable) by peeking for a lone ellipsis. */`` |
|      444 |  616 | `			if( (p->nType & PH7_TK_LPAREN) && p > pStart` |
|       81 |  617 | `				&& (p[-1].nType & PH7_TK_ID)` |
|       70 |  618 | `				&& !((p[-1].nType & PH7_TK_OP) && p[-1].pUserData` |
|      ! 0 |  619 | `					&& ((const ph7_expr_op *)p[-1].pUserData)->iOp == EXPR_OP_NEW) ){` |
|       55 |  620 | `				int bNewCtor = 0;` |
|       55 |  621 | `				SyToken *q = &p[-1];` |
|        - |  622 | ``				/* Walk back over a qualified name (A\B, A::b, $o->m) to a `new`. The name`` |
|        - |  623 | `				 * ALTERNATES -- segment, separator, segment -- so both steps have to be` |
|        - |  624 | `				 * taken: stepping over the separator alone stopped on the segment before` |
|        - |  625 | ``				 * it, and every `new` whose class name carries a `\` then read as a CALL.`` |
|        - |  626 | ``				 * `new Rule\A()` is the ordinary spelling in namespaced code, so an`` |
|        - |  627 | ``				 * attribute argument, a global `const` and a parameter default all`` |
|        - |  628 | `				 * refused what php compiles (Respect\Validation's whole attribute` |
|        - |  629 | `				 * suite is written that way). */` |
|       97 |  630 | `				while( q > pStart ){` |
|       83 |  631 | `					SyToken *pPrev = &q[-1];` |
|       78 |  632 | `					if( (pPrev->nType & PH7_TK_OP) && pPrev->pUserData` |
|       41 |  633 | `						&& ((const ph7_expr_op *)pPrev->pUserData)->iOp == EXPR_OP_NEW ){` |
|       39 |  634 | `						bNewCtor = 1;` |
|       39 |  635 | `						break;` |
|        - |  636 | `					}` |
|       46 |  637 | `					if( GenStateTokenIsMemberOp(pPrev) \|\| (pPrev->nType & PH7_TK_NSSEP) ){` |
|       25 |  638 | `						q--;   /* a separator: whatever precedes it continues the name */` |
|       25 |  639 | `						continue;` |
|        - |  640 | `					}` |
|       20 |  641 | `					if( (pPrev->nType & PH7_TK_ID)` |
|       21 |  642 | `						&& (GenStateTokenIsMemberOp(q) \|\| (q->nType & PH7_TK_NSSEP)) ){` |
|       19 |  643 | `						q--;   /* ...and a SEGMENT, but only across a separator */` |
|       19 |  644 | `						continue;` |
|        - |  645 | `					}` |
|        3 |  646 | `					break;` |
|      ! 0 |  647 | `				}` |
|       55 |  648 | `				if( !bNewCtor && !(&p[1] < pStop && (p[1].nType & PH7_TK_ELLIPSIS)) ){` |
|       15 |  649 | `					return "Constant expression contains invalid operations";` |
|        - |  650 | `				}` |
|       19 |  651 | `			}` |
|      437 |  652 | `			iDepth++;` |
|    67247 |  653 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      431 |  654 | `			if( iDepth > 0 ){` |
|      431 |  655 | `				iDepth--;` |
|      213 |  656 | `			}` |
|    66818 |  657 | `		}else if( !bAllowNew && (p->nType & PH7_TK_OP) && p->pUserData` |
|     8153 |  658 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|        - |  659 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP) whose`` |
|        - |  660 | `` 			 * pUserData is the operator instance, not a keyword id. Ignore a `new` `` |
|        - |  661 | ``			 * used as a member name (`A::new` / `$o->new`). */`` |
|       11 |  662 | `			if( p == pStart \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|       11 |  663 | `				return "New expressions are not supported in this context";` |
|        - |  664 | `			}` |
|      ! 0 |  665 | `		}` |
|    67455 |  666 | `		p++;` |
|        5 |  667 | `	}` |
|    51553 |  668 | `	return 0;` |
|    25779 |  669 | `}` |
|        - |  670 | `/*` |
|        - |  671 | ` * The constant-expression screen as the compiler's call sites use it: the rules of` |
|        - |  672 | ` * GenStateConstExprSpan over the initializer that begins at the current token.` |
|        - |  673 | ` *` |
|        - |  674 | `` * bAllowNew states PHP 8.1's split: a global `const`, a parameter default and an`` |
|        - |  675 | `` * attribute argument take `new`; a class/interface constant, an enum case value and`` |
|        - |  676 | ` * a property default do not.` |
|        - |  677 | ` *` |
|        - |  678 | ` * Returns the sentence, or 0 when the expression is clean. Never consumes tokens.` |
|        - |  679 | ` */` |
|    51572 |  680 | `PH7_PRIVATE const char * PH7_GenStateConstExprError(ph7_gen_state *pGen,int bAllowNew)` |
|        5 |  681 | `{` |
|    77329 |  682 | `	return GenStateConstExprSpan(pGen->pIn,` |
|    25752 |  683 | `		GenStateConstExprEnd(pGen->pIn,pGen->pEnd),bAllowNew,0);` |
|        5 |  684 | `}` |
|        - |  685 | `/*` |
|        - |  686 | ` * php keeps class CONSTANTS and PROPERTIES in separate namespaces: a class may` |
|        - |  687 | `` * declare `const C` and `public $C` together, and `$obj->C` never resolves to the`` |
|        - |  688 | ` * constant. PHL stores each in its own table (constants in hConst, properties in` |
|        - |  689 | ` * hAttr), so these two lookups target the right namespace and never collide.` |
|        - |  690 | ` */` |
|      648 |  691 | `static ph7_class_attr * GenStateExtractConstant(ph7_class *pClass,SyString *pName)` |
|        5 |  692 | `{` |
|      653 |  693 | `	return PH7_ClassExtractConstant(pClass,pName->zString,pName->nByte);` |
|        5 |  694 | `}` |
|     2922 |  695 | `static ph7_class_attr * GenStateExtractProperty(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|        5 |  696 | `{` |
|     2927 |  697 | `	return PH7_ClassExtractAttribute(pClass,zName,nByte);` |
|        5 |  698 | `}` |
|        - |  699 | `/*` |
|        - |  700 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|        - |  701 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|        - |  702 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|        - |  703 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|        - |  704 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|        - |  705 | ` * share the same backing.` |
|        - |  706 | ` */` |
|      894 |  707 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|        - |  708 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|        5 |  709 | `{` |
|      899 |  710 | `	pAttr->nType = nType;` |
|      899 |  711 | `	pAttr->sClass = *pClass;` |
|      899 |  712 | `	pAttr->sTypeName = *pTypeName;` |
|      899 |  713 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|        - |  714 | `		sxu32 i;` |
|      153 |  715 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      105 |  716 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|      105 |  717 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|       55 |  718 | `		}` |
|       24 |  719 | `	}` |
|      899 |  720 | `}` |
|      668 |  721 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|        5 |  722 | `{` |
|      673 |  723 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  724 | `	SySet *pInstrContainer;` |
|        - |  725 | `	ph7_class_attr *pCons;` |
|        - |  726 | `	SyString *pName;` |
|        - |  727 | `	sxi32 rc;` |
|      673 |  728 | `	sxu32 nType = 0;` |
|        - |  729 | `	SyString sTypeClass;` |
|        - |  730 | `	SyString sTypeText;` |
|        - |  731 | `	SySet aUnionAlts;` |
|      673 |  732 | `	sxi32 iTypeFlags = 0;` |
|      673 |  733 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|      673 |  734 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|      673 |  735 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        - |  736 | `	/* Extract visibility level */` |
|      673 |  737 | `	iProtection = GetProtectionLevel(iProtection);` |
|        - |  738 | `	/* Mark as constant */` |
|      673 |  739 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      673 |  740 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|        - |  741 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|        - |  742 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|      702 |  743 | `	if( GenStateClassConstHasType(pGen) ){` |
|       92 |  744 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|        - |  745 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,` |
|       58 |  746 | `			/* bParamCtx */ 0,pGen->pIn->nLine);` |
|        - |  747 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|        - |  748 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|        - |  749 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|        - |  750 | `		 * and success paths release. */` |
|       63 |  751 | `		if( rc == SXERR_CORRUPT ){` |
|        - |  752 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|      ! 0 |  753 | `			goto Synchronize;` |
|       63 |  754 | `		}else if( rc == SXERR_ABORT ){` |
|      ! 0 |  755 | `			return SXERR_ABORT;` |
|       63 |  756 | `		}else if( rc != SXRET_OK ){` |
|      ! 0 |  757 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  758 | `				"Invalid type for class constant inside class '%z'",&pClass->sDisp);` |
|      ! 0 |  759 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  760 | `				return SXERR_ABORT;` |
|        - |  761 | `			}` |
|      ! 0 |  762 | `			goto Synchronize;` |
|        - |  763 | `		}` |
|       63 |  764 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|       29 |  765 | `	}` |
|      334 |  766 | `loop:` |
|        - |  767 | ``	/* php 8 accepts EVERY reserved word as a class-constant name — `const list = 5`,`` |
|        - |  768 | ``	 * `const match`, `const function`, even `const true` — because a class constant is`` |
|        - |  769 | ``	 * addressed only through `C::name`, where no keyword can be ambiguous. The single`` |
|        - |  770 | ``	 * exception is `class`, reserved for `C::class`, and it gets its own message.`` |
|        - |  771 | `	 * (Method names already accept the whole set; this is the member-name side of the` |
|        - |  772 | ``	 * same rule. Global `const` is NOT the same rule: php rejects a reserved word there.)`` |
|        - |  773 | `	 * A keyword arrives as PH7_TK_KEYWORD, which this ID-only test rejected — so PHL` |
|        - |  774 | ``	 * accepted only the alpha-OPERATOR keywords (`const new`, `const and`), which the`` |
|        - |  775 | `	 * lexer marks PH7_TK_ID\|PH7_TK_OP, and that partial allow-list looked like a design. */` |
|      679 |  776 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  777 | `		/* Invalid constant name */` |
|      ! 0 |  778 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|      ! 0 |  779 | `		if( rc == SXERR_ABORT ){` |
|        - |  780 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  781 | `			return SXERR_ABORT;` |
|        - |  782 | `		}` |
|      ! 0 |  783 | `		goto Synchronize;` |
|        - |  784 | `	}` |
|        - |  785 | `	/* Peek constant name */` |
|      679 |  786 | `	pName = &pGen->pIn->sData;` |
|      674 |  787 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      379 |  788 | `		&& (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CLASS ){` |
|        3 |  789 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  790 | `			"A class constant must not be called 'class'; it is reserved for class name fetching");` |
|        3 |  791 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  792 | `			return SXERR_ABORT;` |
|        - |  793 | `		}` |
|        3 |  794 | `		goto Synchronize;` |
|        - |  795 | `	}` |
|        - |  796 | `	/* No reserved-CONSTANT check here: true/false/null are reserved GLOBAL constant` |
|        - |  797 | ``	 * names (compile_stmt.c still rejects `const true = 1`), but `C::true` addresses a`` |
|        - |  798 | `	 * class constant and php accepts the declaration like any other reserved word. The` |
|        - |  799 | `	 * member-name flag keeps the read from folding into the boolean literal. */` |
|      677 |  800 | `	if( (iFlags & PH7_CLASS_ATTR_FINAL) && iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|        - |  801 | ``		/* `final` says "no subclass may replace this", and a PRIVATE constant is not`` |
|        - |  802 | `		 * visible to one -- so php refuses the pair, naming the constant. Same shape` |
|        - |  803 | `		 * as the private-final METHOD rule one member over, except php makes this one` |
|        - |  804 | `		 * a fatal rather than a warning. Reported per NAME, which is php's order for` |
|        - |  805 | `		 * a multi-declaration too. */` |
|        4 |  806 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  807 | `			"Private constant %z::%z cannot be final as it is not visible to other classes",` |
|        1 |  808 | `			&pClass->sDisp,pName);` |
|        3 |  809 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  810 | `			return SXERR_ABORT;` |
|        - |  811 | `		}` |
|        3 |  812 | `		goto Synchronize;` |
|        - |  813 | `	}` |
|        - |  814 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|      675 |  815 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       92 |  816 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|       58 |  817 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|       29 |  818 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|       63 |  819 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  820 | `			return SXERR_ABORT;` |
|       63 |  821 | `		}else if( rc != SXRET_OK ){` |
|        3 |  822 | `			goto Synchronize;` |
|        - |  823 | `		}` |
|       28 |  824 | `	}` |
|        - |  825 | `	/* Advance the stream cursor */` |
|      673 |  826 | `	pGen->pIn++;` |
|      673 |  827 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|        - |  828 | `		/* Invalid declaration */` |
|      ! 0 |  829 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Expected '=' after class constant %z'",pName);` |
|      ! 0 |  830 | `		if( rc == SXERR_ABORT ){` |
|        - |  831 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  832 | `			return SXERR_ABORT;` |
|        - |  833 | `		}` |
|      ! 0 |  834 | `		goto Synchronize;` |
|        - |  835 | `	}` |
|      673 |  836 | `	pGen->pIn++; /* Jump the equal sign */` |
|        - |  837 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|        - |  838 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|        - |  839 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|        - |  840 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|      668 |  841 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|       57 |  842 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|        8 |  843 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  844 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|        2 |  845 | `			&pClass->sDisp,pName,&sTypeText);` |
|        6 |  846 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  847 | `			return SXERR_ABORT;` |
|        - |  848 | `		}` |
|        6 |  849 | `		goto Synchronize;` |
|        - |  850 | `	}` |
|        - |  851 | `	/* php's constant-expression rules, first offender wins (see` |
|        - |  852 | ``	 * PH7_GenStateConstExprError). A class/interface constant takes no `new`. */`` |
|        - |  853 | `	{` |
|      669 |  854 | `		const char *zCErr = PH7_GenStateConstExprError(pGen,0);` |
|      669 |  855 | `		if( zCErr ){` |
|       20 |  856 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"%s",zCErr);` |
|       20 |  857 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  858 | `				return SXERR_ABORT;` |
|        - |  859 | `			}` |
|       20 |  860 | `			goto Synchronize;` |
|        - |  861 | `		}` |
|        - |  862 | `	}` |
|        - |  863 | `	/* php: a class constant may not be redefined in the same class body. The` |
|        - |  864 | `	 * property path already guarded this; the constant path did not, so` |
|        - |  865 | ``	 * `class C{const X=1; const X=2;}` silently kept one of them. */`` |
|        - |  866 | ``	/* php keeps constants and properties in SEPARATE namespaces, so `const C` and`` |
|        - |  867 | ``	 * `public $C` coexist. PHL now stores them in disjoint tables (hConst / hAttr),`` |
|        - |  868 | `	 * so no collision — only a genuine constant redefinition is rejected below. */` |
|      653 |  869 | `	if( GenStateExtractConstant(pClass,pName) != 0 ){` |
|      ! 0 |  870 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  871 | `			"Cannot redefine class constant %z::%z",&pClass->sDisp,pName);` |
|      ! 0 |  872 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  873 | `			return SXERR_ABORT;` |
|        - |  874 | `		}` |
|      ! 0 |  875 | `		goto Synchronize;` |
|        - |  876 | `	}` |
|        - |  877 | `	/* Allocate a new class attribute */` |
|      653 |  878 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|      653 |  879 | `	if( pCons ){` |
|      653 |  880 | `		GenStateConsumeDoc(&(*pGen),&pCons->sDoc);` |
|      653 |  881 | `		if( GenStateConsumeAttrs(&(*pGen),&pCons->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  882 | `			return SXERR_ABORT;` |
|        - |  883 | `		}` |
|      653 |  884 | `		if( GenStateCheckAttrPlacement(&(*pGen),&pCons->aAttrs,16,16,0,0) == SXERR_ABORT ){` |
|      ! 0 |  885 | `			return SXERR_ABORT;` |
|        - |  886 | `		}` |
|      324 |  887 | `	}` |
|      653 |  888 | `	if( pCons == 0 ){` |
|      ! 0 |  889 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  890 | `		return SXERR_ABORT;` |
|        - |  891 | `	}` |
|      653 |  892 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       57 |  893 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|       26 |  894 | `	}` |
|        - |  895 | `	/* Swap bytecode container */` |
|      653 |  896 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      653 |  897 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|        - |  898 | `	/* Compile constant value.` |
|        - |  899 | `	 */` |
|      653 |  900 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      653 |  901 | `	if( rc == SXERR_EMPTY ){` |
|        3 |  902 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|        3 |  903 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  904 | `			return SXERR_ABORT;` |
|        - |  905 | `		}` |
|        1 |  906 | `	}` |
|        - |  907 | `	/* Emit the done instruction */` |
|      653 |  908 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      653 |  909 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      653 |  910 | `	if( rc == SXERR_ABORT ){` |
|        - |  911 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  912 | `		return SXERR_ABORT;` |
|        - |  913 | `	}` |
|        - |  914 | `	/* All done,install the constant */` |
|      653 |  915 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      653 |  916 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  917 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  918 | `		return SXERR_ABORT;` |
|        - |  919 | `	}` |
|      653 |  920 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - |  921 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|        7 |  922 | `		pGen->pIn++; /* Jump the comma */` |
|        - |  923 | `		/* A reserved word is a valid name for EVERY constant in the declaration, not` |
|        - |  924 | ``		 * just the first (`const list = 1, match = 2`) — same allow-list as the head. */`` |
|        7 |  925 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  926 | `			SyToken *pTok = pGen->pIn;` |
|      ! 0 |  927 | `			if( pTok >= pGen->pEnd ){` |
|      ! 0 |  928 | `				pTok--;` |
|      ! 0 |  929 | `			}` |
|      ! 0 |  930 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|        - |  931 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|      ! 0 |  932 | `				&pTok->sData,&pClass->sDisp);` |
|      ! 0 |  933 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  934 | `				return SXERR_ABORT;` |
|        - |  935 | `			}` |
|      ! 0 |  936 | `		}else{` |
|        7 |  937 | `			goto loop;` |
|        - |  938 | `		}` |
|      ! 0 |  939 | `	}` |
|      647 |  940 | `	SySetRelease(&aUnionAlts);` |
|      647 |  941 | `	return SXRET_OK;` |
|       13 |  942 | `Synchronize:` |
|       30 |  943 | `	SySetRelease(&aUnionAlts);` |
|        - |  944 | `	/* Synchronize with the first semi-colon */` |
|      156 |  945 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      130 |  946 | `		pGen->pIn++;` |
|        4 |  947 | `	}` |
|       30 |  948 | `	return SXERR_CORRUPT;` |
|      339 |  949 | `}` |
|        - |  950 | `/*` |
|        - |  951 | ` * complie a class attribute or Properties in the PHP jargon.` |
|        - |  952 | ` * According to the PHP language reference manual` |
|        - |  953 | ` *  Properties` |
|        - |  954 | ` *  Class member variables are called "properties". You may also see them referred` |
|        - |  955 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|        - |  956 | ` *  of this reference we will use "properties". They are defined by using one` |
|        - |  957 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|        - |  958 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|        - |  959 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|        - |  960 | ` *  and must not depend on run-time information in order to be evaluated.` |
|        - |  961 | ` * Symisc eXtension.` |
|        - |  962 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|        - |  963 | ` *  the zend engine would allow only simple scalar value.` |
|        - |  964 | ` *  Example:` |
|        - |  965 | ` *   class Test{` |
|        - |  966 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|        - |  967 | ` *   };` |
|        - |  968 | ` *   var_dump(TEST::myVar);` |
|        - |  969 | ` *   Refer to the official documentation for more information on the powerful extension` |
|        - |  970 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|        - |  971 | ` */` |
|        - |  972 | `/*` |
|        - |  973 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|        - |  974 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|        - |  975 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|        - |  976 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|        - |  977 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|        - |  978 | ` */` |
|     6562 |  979 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|        5 |  980 | `{` |
|     6567 |  981 | `	SyToken *p = pStart;` |
|     6567 |  982 | `	int bFirst = 1;` |
|     6567 |  983 | `	if( p >= pEnd ) return 0;` |
|        - |  984 | ``	/* Optional nullable `?` shorthand. */`` |
|     6567 |  985 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|       89 |  986 | `		p++;` |
|       89 |  987 | `		if( p >= pEnd ) return 0;` |
|       42 |  988 | `	}` |
|        - |  989 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|        - |  990 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|        - |  991 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|        - |  992 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|     3281 |  993 | `	for(;;){` |
|     6607 |  994 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|        - |  995 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|        3 |  996 | `			p++;` |
|        9 |  997 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|        3 |  998 | `			if( p >= pEnd ) return 0;` |
|        3 |  999 | `			p++; /* skip ')' */` |
|        2 | 1000 | `		}else{` |
|        - | 1001 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|        - | 1002 | ``			 * then any `&`-joined intersection members. */`` |
|     6605 | 1003 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|     6605 | 1004 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 1005 | `				return 0;` |
|        - | 1006 | `			}` |
|        - | 1007 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|        - | 1008 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|        - | 1009 | `			 * may still appear at the initial dispatch site). */` |
|     6605 | 1010 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|     6495 | 1011 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|     6490 | 1012 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|     1125 | 1013 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|     5713 | 1014 | `					return 0;` |
|        - | 1015 | `				}` |
|      391 | 1016 | `			}` |
|      897 | 1017 | `			p++;` |
|      903 | 1018 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        7 | 1019 | `				p += 2;` |
|        1 | 1020 | `			}` |
|     1344 | 1021 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|      903 | 1022 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        6 | 1023 | `				p++; /* skip '&' */` |
|        6 | 1024 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|        6 | 1025 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|        6 | 1026 | `				p++;` |
|        6 | 1027 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      ! 0 | 1028 | `					p += 2;` |
|      ! 0 | 1029 | `				}` |
|        2 | 1030 | `			}` |
|        - | 1031 | `		}` |
|      899 | 1032 | `		bFirst = 0;` |
|      894 | 1033 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|       45 | 1034 | `			&& p->sData.zString[0] == '\|' ){` |
|       45 | 1035 | ``			p++; /* next `\|`-separated part */`` |
|       45 | 1036 | `			continue;` |
|        - | 1037 | `		}` |
|      859 | 1038 | `		break;` |
|      ! 0 | 1039 | `	}` |
|      859 | 1040 | `	if( p >= pEnd ) return 0;` |
|      859 | 1041 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|     3286 | 1042 | `}` |
|        - | 1043 |  |
|        - | 1044 | `/*` |
|        - | 1045 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|        - | 1046 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|        - | 1047 | ` * if not). Recognized forms:` |
|        - | 1048 | ` *   ?Type, array, bool, int, float, string, object,` |
|        - | 1049 | ` *   self, parent, \Ns\ClassName, ClassName` |
|        - | 1050 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|        - | 1051 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|        - | 1052 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|        - | 1053 | ` * on unrecoverable error.` |
|        - | 1054 | ` *` |
|        - | 1055 | ` * When a type is parsed:` |
|        - | 1056 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|        - | 1057 | ` *   *pClass is set to the class name (for class types)` |
|        - | 1058 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|        - | 1059 | ` *   *pTypeText is set to the original text span of the type` |
|        - | 1060 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|        - | 1061 | ` */` |
|      846 | 1062 | `static sxi32 GenStateParsePropertyType(` |
|        - | 1063 | `	ph7_gen_state *pGen,` |
|        - | 1064 | `	sxu32 *pnType,` |
|        - | 1065 | `	SyString *pClass,` |
|        - | 1066 | `	sxi32 *piTypeFlags,` |
|        - | 1067 | `	SyString *pTypeText,` |
|        - | 1068 | `	SySet *pAlts` |
|        5 | 1069 | `){` |
|      851 | 1070 | `	sxi32 iFlags = 0;` |
|        - | 1071 | `	sxi32 rc;` |
|      851 | 1072 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 1073 | `		return SXRET_OK;` |
|        - | 1074 | `	}` |
|        - | 1075 | `	/* If the first token is '$', there's no type */` |
|      851 | 1076 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      ! 0 | 1077 | `		return SXRET_OK;` |
|        - | 1078 | `	}` |
|      851 | 1079 | `	rc = GenStateParseUnionTypeDecl(` |
|      423 | 1080 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|        - | 1081 | `		PH7_CLASS_ATTR_NULLABLE,` |
|        - | 1082 | `		PH7_CLASS_ATTR_UNION,` |
|        - | 1083 | `		/* bAllowVoid */ 0,` |
|        - | 1084 | `		/* bParamCtx */ 0,` |
|      846 | 1085 | `		pGen->pIn->nLine);` |
|      851 | 1086 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1087 | `		return rc;` |
|        - | 1088 | `	}` |
|        - | 1089 | `	/* Verify next token is '$' (start of property name) */` |
|      851 | 1090 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 | 1091 | `		return SXERR_SYNTAX;` |
|        - | 1092 | `	}` |
|      851 | 1093 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|      851 | 1094 | `	return SXRET_OK;` |
|      428 | 1095 | `}` |
|        - | 1096 |  |
|        - | 1097 | `/*` |
|        - | 1098 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|        - | 1099 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|        - | 1100 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|        - | 1101 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|        - | 1102 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|        - | 1103 | ` * by the type parser itself before reaching here.` |
|        - | 1104 | ` *` |
|        - | 1105 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|        - | 1106 | ` * use in the error message.` |
|        - | 1107 | ` */` |
|     1160 | 1108 | `static int GenStateIsDisallowedPropertyAtom(` |
|        - | 1109 | `	sxu32 nType,` |
|        - | 1110 | `	const SyString *pClass,` |
|        - | 1111 | `	const char **pzName,` |
|        - | 1112 | `	sxu32 *pnName)` |
|        5 | 1113 | `{` |
|        - | 1114 | `	const char *z;` |
|        - | 1115 | `	sxu32 n;` |
|     1165 | 1116 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|     1023 | 1117 | `		return 0;` |
|        - | 1118 | `	}` |
|      147 | 1119 | `	z = pClass->zString;` |
|      147 | 1120 | `	n = pClass->nByte;` |
|      147 | 1121 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|        9 | 1122 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|        - | 1123 | `	}` |
|        - | 1124 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|        - | 1125 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|        - | 1126 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|      141 | 1127 | `	return 0;` |
|      585 | 1128 | `}` |
|        - | 1129 |  |
|        - | 1130 | `/*` |
|        - | 1131 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|        - | 1132 | ` * constant) — the main atom plus any union alternatives — against the` |
|        - | 1133 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|        - | 1134 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|        - | 1135 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|        - | 1136 | ` * type T" vs "Class constant C::X cannot have type T").` |
|        - | 1137 | ` *` |
|        - | 1138 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|        - | 1139 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|        - | 1140 | ` */` |
|     1044 | 1141 | `PH7_PRIVATE sxi32 GenStateValidateMemberType(` |
|        - | 1142 | `	ph7_gen_state *pGen,` |
|        - | 1143 | `	ph7_class *pClass,` |
|        - | 1144 | `	const SyString *pMemberName,` |
|        - | 1145 | `	sxu32 nType,` |
|        - | 1146 | `	const SyString *pTypeClass,` |
|        - | 1147 | `	const SyString *pTypeText,` |
|        - | 1148 | `	SySet *pUnionAlts,` |
|        - | 1149 | `	const char *zErrFmt,` |
|        - | 1150 | `	sxu32 nLine)` |
|        5 | 1151 | `{` |
|     1049 | 1152 | `	const char *zBad = 0;` |
|     1049 | 1153 | `	sxu32 nBad = 0;` |
|        - | 1154 | `	SyString sFallback;` |
|        - | 1155 | `	const SyString *pBad;` |
|        - | 1156 | `	sxi32 rc;` |
|     1049 | 1157 | `	int bDisallowed = 0;` |
|     1049 | 1158 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|        6 | 1159 | `		bDisallowed = 1;` |
|     1047 | 1160 | `	}else if( pUnionAlts ){` |
|        - | 1161 | `		sxu32 i;` |
|      175 | 1162 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|      121 | 1163 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|      121 | 1164 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|        3 | 1165 | `				bDisallowed = 1;` |
|        3 | 1166 | `				break;` |
|        - | 1167 | `			}` |
|       62 | 1168 | `		}` |
|       28 | 1169 | `	}` |
|     1049 | 1170 | `	if( !bDisallowed ){` |
|     1043 | 1171 | `		return SXRET_OK;` |
|        - | 1172 | `	}` |
|        - | 1173 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|        - | 1174 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|        - | 1175 | `	 * canonical spelling if the type text is unavailable. */` |
|        9 | 1176 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|        9 | 1177 | `		pBad = pTypeText;` |
|        6 | 1178 | `	}else{` |
|      ! 0 | 1179 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|      ! 0 | 1180 | `		pBad = &sFallback;` |
|        - | 1181 | `	}` |
|       12 | 1182 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        3 | 1183 | `		zErrFmt,` |
|        3 | 1184 | `		&pClass->sDisp,pMemberName,pBad);` |
|        9 | 1185 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 1186 | `		return SXERR_ABORT;` |
|        - | 1187 | `	}` |
|        9 | 1188 | `	return SXERR_SYNTAX;` |
|      527 | 1189 | `}` |
|        - | 1190 | `/*` |
|        - | 1191 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|        - | 1192 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|        - | 1193 | ` * matched as a plain identifier in the class-member modifier position rather` |
|        - | 1194 | ` * than promoted to a lexer keyword.` |
|        - | 1195 | ` */` |
|  9357870 | 1196 | `PH7_PRIVATE int GenStateIsReadonly(SyToken *pTok)` |
|        5 | 1197 | `{` |
| 10275398 | 1198 | `	return (pTok->nType & PH7_TK_ID)` |
|  5591911 | 1199 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
| 10279261 | 1200 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|        5 | 1201 | `}` |
|        - | 1202 | `/*` |
|        - | 1203 | ``  * Detect an asymmetric set-visibility modifier `public(set)` / `protected(set)` `` |
|        - | 1204 | `` * / `private(set)` (PHP 8.4) starting at pTok. Returns the visibility keyword id`` |
|        - | 1205 | ` * (PH7_TKWRD_*) and sets *pnTok to the 4 tokens consumed, or 0 when not present` |
|        - | 1206 | ` * (a bare visibility keyword is NOT a set-modifier; the '(' 'set' ')' run is).` |
|        - | 1207 | ` */` |
|   238489 | 1208 | `PH7_PRIVATE sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok)` |
|        5 | 1209 | `{` |
|   238494 | 1210 | `	*pnTok = 0;` |
|   238489 | 1211 | `	if( &pTok[3] < pEnd` |
|   186557 | 1212 | `	 && (pTok->nType & PH7_TK_KEYWORD)` |
|   134771 | 1213 | `	 && (pTok[1].nType & PH7_TK_LPAREN)` |
|    67319 | 1214 | `	 && (pTok[2].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|       48 | 1215 | `	 && pTok[2].sData.nByte == sizeof("set")-1` |
|       47 | 1216 | `	 && SyStrnicmp(pTok[2].sData.zString,"set",sizeof("set")-1) == 0` |
|       51 | 1217 | `	 && (pTok[3].nType & PH7_TK_RPAREN) ){` |
|       49 | 1218 | `		sxi32 nKw = SX_PTR_TO_INT(pTok->pUserData);` |
|       49 | 1219 | `		if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PRIVATE \|\| nKw == PH7_TKWRD_PROTECTED ){` |
|       49 | 1220 | `			*pnTok = 4;` |
|       49 | 1221 | `			return nKw;` |
|        - | 1222 | `		}` |
|      ! 0 | 1223 | `	}` |
|   238448 | 1224 | `	return 0;` |
|   119086 | 1225 | `}` |
|        - | 1226 | `/* Map a set-visibility keyword to its PH7_CLASS_ATTR_* flag. */` |
|       46 | 1227 | `PH7_PRIVATE sxi32 GenStateSetVisFlag(sxi32 nKw)` |
|        3 | 1228 | `{` |
|       49 | 1229 | `	if( nKw == PH7_TKWRD_PRIVATE ){` |
|       37 | 1230 | `		return PH7_CLASS_ATTR_PRIVATE_SET;` |
|        - | 1231 | `	}` |
|       13 | 1232 | `	if( nKw == PH7_TKWRD_PROTECTED ){` |
|       11 | 1233 | `		return PH7_CLASS_ATTR_PROTECTED_SET;` |
|        - | 1234 | `	}` |
|        3 | 1235 | `	return PH7_CLASS_ATTR_PUBLIC_SET;` |
|       26 | 1236 | `}` |
|     2750 | 1237 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|        5 | 1238 | `{` |
|     2755 | 1239 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 1240 | `	ph7_class_attr *pAttr;` |
|        - | 1241 | `	SyString *pName;` |
|        - | 1242 | `	sxi32 rc;` |
|     2755 | 1243 | `	sxu32 nType = 0;` |
|        - | 1244 | `	SyString sTypeClass;` |
|        - | 1245 | `	SyString sTypeText;` |
|        - | 1246 | `	SySet aUnionAlts;` |
|     2755 | 1247 | `	sxi32 iTypeFlags = 0;` |
|     2755 | 1248 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|     2755 | 1249 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|     2755 | 1250 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        - | 1251 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|        - | 1252 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|        - | 1253 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|     2755 | 1254 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|       25 | 1255 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       11 | 1256 | `	}` |
|        - | 1257 | `	/* Extract visibility level */` |
|     2755 | 1258 | `	iProtection = GetProtectionLevel(iProtection);` |
|        - | 1259 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|     3178 | 1260 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      851 | 1261 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|      851 | 1262 | `		if( rc == SXERR_CORRUPT ){` |
|        - | 1263 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|      ! 0 | 1264 | `			goto Synchronize;` |
|      851 | 1265 | `		}else if( rc == SXERR_SYNTAX ){` |
|      ! 0 | 1266 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1267 | `				"Invalid property type or declaration near '%z'",` |
|      ! 0 | 1268 | `				&pGen->pIn->sData);` |
|      ! 0 | 1269 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1270 | `				return SXERR_ABORT;` |
|        - | 1271 | `			}` |
|      ! 0 | 1272 | `			goto Synchronize;` |
|      851 | 1273 | `		}else if( rc == SXERR_ABORT ){` |
|      ! 0 | 1274 | `			return SXERR_ABORT;` |
|        - | 1275 | `		}` |
|      423 | 1276 | `	}` |
|      ! 0 | 1277 | `loop:` |
|     2763 | 1278 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 | 1279 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Expected '$' at start of property name");` |
|      ! 0 | 1280 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1281 | `			return SXERR_ABORT;` |
|        - | 1282 | `		}` |
|      ! 0 | 1283 | `		goto Synchronize;` |
|        - | 1284 | `	}` |
|     2763 | 1285 | `	pGen->pIn++; /* Jump the dollar sign */` |
|     2763 | 1286 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|        - | 1287 | `		/* Invalid attribute name */` |
|      ! 0 | 1288 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|      ! 0 | 1289 | `		if( rc == SXERR_ABORT ){` |
|        - | 1290 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1291 | `			return SXERR_ABORT;` |
|        - | 1292 | `		}` |
|      ! 0 | 1293 | `		goto Synchronize;` |
|        - | 1294 | `	}` |
|        - | 1295 | `	/* Peek attribute name */` |
|     2763 | 1296 | `	pName = &pGen->pIn->sData;` |
|        - | 1297 | `	/* Advance the stream cursor */` |
|     2763 | 1298 | `	pGen->pIn++;` |
|     2763 | 1299 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/\|PH7_TK_OCB/*'{' hooks*/)) == 0 ){` |
|        - | 1300 | `		/* Invalid declaration */` |
|        - | 1301 | `		/* php reports the offending token here, expecting "," or ";". */` |
|        3 | 1302 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\",\" or \";\"");` |
|        3 | 1303 | `		if( rc == SXERR_ABORT ){` |
|        - | 1304 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1305 | `			return SXERR_ABORT;` |
|        - | 1306 | `		}` |
|        3 | 1307 | `		goto Synchronize;` |
|        - | 1308 | `	}` |
|        - | 1309 | `	/* Asymmetric-visibility rules (PHP 8.4): the property must be typed, and` |
|        - | 1310 | `	 * the read visibility must not be narrower than the set visibility. */` |
|     2761 | 1311 | `	if( iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET\|PH7_CLASS_ATTR_PUBLIC_SET) ){` |
|       42 | 1312 | `		const char *zAvErr = 0;` |
|       62 | 1313 | `		sxi32 iSetLevel = (iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? PH7_CLASS_PROT_PRIVATE` |
|       32 | 1314 | `			: (iFlags & PH7_CLASS_ATTR_PROTECTED_SET) ? PH7_CLASS_PROT_PROTECTED` |
|        6 | 1315 | `			: PH7_CLASS_PROT_PUBLIC;` |
|       42 | 1316 | `		if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 | 1317 | `			zAvErr = "Property with asymmetric visibility %z::$%z must have type";` |
|       42 | 1318 | `		}else if( iProtection > iSetLevel ){` |
|      ! 0 | 1319 | `			zAvErr = "Visibility of property %z::$%z must not be weaker than set visibility";` |
|      ! 0 | 1320 | `		}` |
|       42 | 1321 | `		if( zAvErr ){` |
|      ! 0 | 1322 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zAvErr,&pClass->sDisp,pName);` |
|      ! 0 | 1323 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1324 | `				return SXERR_ABORT;` |
|        - | 1325 | `			}` |
|      ! 0 | 1326 | `			goto Synchronize;` |
|        - | 1327 | `		}` |
|       20 | 1328 | `	}` |
|        - | 1329 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|        - | 1330 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|     2761 | 1331 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|       99 | 1332 | `		const char *zRoErr = 0;` |
|       99 | 1333 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        3 | 1334 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|       98 | 1335 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|        6 | 1336 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|       95 | 1337 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|        6 | 1338 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|        2 | 1339 | `		}` |
|       99 | 1340 | `		if( zRoErr ){` |
|       13 | 1341 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sDisp,pName);` |
|       13 | 1342 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1343 | `				return SXERR_ABORT;` |
|        - | 1344 | `			}` |
|       13 | 1345 | `			goto Synchronize;` |
|        - | 1346 | `		}` |
|       42 | 1347 | `	}` |
|        - | 1348 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|        - | 1349 | `	 * type atom or any union alternative. void/never are already rejected` |
|        - | 1350 | `	 * by the type parser. */` |
|     2751 | 1351 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     1271 | 1352 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|        - | 1353 | `			&sTypeText,` |
|      844 | 1354 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      422 | 1355 | `			"Property %z::$%z cannot have type %z",nLine);` |
|      849 | 1356 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1357 | `			return SXERR_ABORT;` |
|      849 | 1358 | `		}else if( rc != SXRET_OK ){` |
|      ! 0 | 1359 | `			goto Synchronize;` |
|        - | 1360 | `		}` |
|      422 | 1361 | `	}` |
|        - | 1362 | `	/* Reject redeclaration (catches clash with an earlier promoted property).` |
|        - | 1363 | `	 * A same-name class CONSTANT is NOT a clash — php's separate namespaces let` |
|        - | 1364 | ``	 * `const C` and `public $C` coexist (stored in disjoint hConst / hAttr tables). */`` |
|     2751 | 1365 | `	if( GenStateExtractProperty(pClass,pName->zString,pName->nByte) != 0 ){` |
|        4 | 1366 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 1367 | `			"Cannot redeclare %z::$%z",&pClass->sDisp,pName);` |
|        3 | 1368 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1369 | `			return SXERR_ABORT;` |
|        - | 1370 | `		}` |
|        3 | 1371 | `		goto Synchronize;` |
|        - | 1372 | `	}` |
|        - | 1373 | `	/* php's constant-expression rules, first offender wins. A property default takes` |
|        - | 1374 | ``	 * no `new`. pGen->pIn is still on the '=' (the scan skips it and reads the`` |
|        - | 1375 | `	 * initializer non-destructively); no '=' means no default at all, and the scan` |
|        - | 1376 | `	 * then stops at the ';'/',' with nothing to report. */` |
|     2749 | 1377 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|     1921 | 1378 | `		const char *zCErr = PH7_GenStateConstExprError(pGen,0);` |
|     1921 | 1379 | `		if( zCErr ){` |
|        9 | 1380 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"%s",zCErr);` |
|        9 | 1381 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1382 | `				return SXERR_ABORT;` |
|        - | 1383 | `			}` |
|        9 | 1384 | `			goto Synchronize;` |
|        - | 1385 | `		}` |
|      955 | 1386 | `	}` |
|        - | 1387 | `	/* Allocate a new class attribute */` |
|     2743 | 1388 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|     2743 | 1389 | `	if( pAttr ){` |
|     2743 | 1390 | `		GenStateConsumeDoc(&(*pGen),&pAttr->sDoc);` |
|     2743 | 1391 | `		if( GenStateConsumeAttrs(&(*pGen),&pAttr->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 1392 | `			return SXERR_ABORT;` |
|        - | 1393 | `		}` |
|     2743 | 1394 | `		if( GenStateCheckAttrPlacement(&(*pGen),&pAttr->aAttrs,8,8,0,0) == SXERR_ABORT ){` |
|      ! 0 | 1395 | `			return SXERR_ABORT;` |
|        - | 1396 | `		}` |
|     1369 | 1397 | `	}` |
|     2743 | 1398 | `	if( pAttr == 0 ){` |
|      ! 0 | 1399 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1400 | `		return SXERR_ABORT;` |
|        - | 1401 | `	}` |
|     2743 | 1402 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      847 | 1403 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      421 | 1404 | `	}` |
|     2743 | 1405 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|        - | 1406 | `		SySet *pInstrContainer;` |
|     1915 | 1407 | `		SyToken *pSavedDefEnd = pGen->pEnd;` |
|     1915 | 1408 | `		pGen->pIn++; /*Jump the equal sign */` |
|        - | 1409 | `		{` |
|        - | 1410 | `			/* Delimit the default expression: it ends at the declaration's` |
|        - | 1411 | `			 * ';'/',' or at a top-level '{' opening a PHP 8.4 hook list` |
|        - | 1412 | ``			 * (`public string $w = "init" { get => …; }`) — the expression`` |
|        - | 1413 | `			 * compiler would otherwise run into the hook tokens. */` |
|     1915 | 1414 | `			SyToken *pScan = pGen->pIn;` |
|     1915 | 1415 | `			sxi32 iNest = 0;` |
|     1915 | 1416 | ``			int bFuncSeen = 0; /* a `function` keyword stands at depth 0 */`` |
|    12749 | 1417 | `			while( pScan < pGen->pEnd ){` |
|    12744 | 1418 | `				if( (pScan->nType & PH7_TK_KEYWORD) && iNest <= 0` |
|       67 | 1419 | `					&& SX_PTR_TO_INT(pScan->pUserData) == PH7_TKWRD_FUNCTION ){` |
|        - | 1420 | `					/* The next depth-0 '{' is this CLOSURE's body, not a hook list:` |
|        - | 1421 | ``					 * `public $p = static function(){ … };` is php-legal (a static`` |
|        - | 1422 | `					 * closure is a constant expression) and its brace was read as` |
|        - | 1423 | `					 * the hook-list opener, so the default was truncated at` |
|        - | 1424 | ``					 * `static function()` and the declaration died three errors`` |
|        - | 1425 | `					 * deep. A class CONSTANT never showed it — only the property` |
|        - | 1426 | `					 * path carries a hook list at all. */` |
|        9 | 1427 | `					bFuncSeen = 1;` |
|    12745 | 1428 | `				}else if( pScan->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|      291 | 1429 | `					iNest++;` |
|    12598 | 1430 | `				}else if( pScan->nType & (PH7_TK_RPAREN\|PH7_TK_CSB) ){` |
|      291 | 1431 | `					iNest--;` |
|    12312 | 1432 | `				}else if( iNest <= 0 && (pScan->nType & PH7_TK_OCB) ){` |
|       65 | 1433 | `					if( !bFuncSeen ){` |
|       57 | 1434 | `						break; /* the hook list */` |
|        - | 1435 | `					}` |
|        9 | 1436 | `					bFuncSeen = 0;` |
|        9 | 1437 | `					pScan++;` |
|        9 | 1438 | `					PH7_DelimitNestedTokens(pScan,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pScan);` |
|        9 | 1439 | `					if( pScan >= pGen->pEnd ){` |
|      ! 0 | 1440 | `						break;` |
|        1 | 1441 | `					}` |
|        - | 1442 | `					/* land on the closing '}', the loop's pScan++ steps past it */` |
|    12111 | 1443 | `				}else if( iNest <= 0 && (pScan->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|     1861 | 1444 | `					break;` |
|        - | 1445 | `				}` |
|    10839 | 1446 | `				pScan++;` |
|        5 | 1447 | `			}` |
|     1915 | 1448 | `			pGen->pEnd = pScan;` |
|        - | 1449 | `		}` |
|        - | 1450 | `		/* Swap bytecode container */` |
|     1915 | 1451 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     1915 | 1452 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|        - | 1453 | `		/* Compile attribute value. The default is a const-expression belonging to` |
|        - | 1454 | `		 * pClass (see iInMemberDefault) — __TRAIT__ in it reads pCurClass. */` |
|     1915 | 1455 | `		pGen->iInMemberDefault++;` |
|     1915 | 1456 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|     1915 | 1457 | `		pGen->iInMemberDefault--;` |
|     1915 | 1458 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 | 1459 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|      ! 0 | 1460 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1461 | `				return SXERR_ABORT;` |
|        - | 1462 | `			}` |
|      ! 0 | 1463 | `		}` |
|        - | 1464 | `		/* Emit the done instruction */` |
|     1915 | 1465 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|     1915 | 1466 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     1915 | 1467 | `		pGen->pIn = pGen->pEnd;   /* land exactly on the delimiter */` |
|     1915 | 1468 | `		pGen->pEnd = pSavedDefEnd;` |
|      955 | 1469 | `	}` |
|        - | 1470 | `	/* All done,install the attribute */` |
|     2743 | 1471 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|     2743 | 1472 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1473 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1474 | `		return SXERR_ABORT;` |
|        - | 1475 | `	}` |
|     2743 | 1476 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) ){` |
|        - | 1477 | ``		/* PHP 8.4 property hooks: `public [T] $x [= default] { get ...; set ...; }`.`` |
|        - | 1478 | `		 * The list ends the declaration at '}' — no trailing ';', no comma list. */` |
|      187 | 1479 | `		if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|        - | 1480 | ``			/* `readonly` promises one write, a hook decides what a write MEANS, and`` |
|        - | 1481 | `			 * php will not have both -- a rule the declaration screen never had, so` |
|        - | 1482 | ``			 * `public readonly int $p { get => 1; }` compiled here and does not in`` |
|        - | 1483 | `			 * php (in a readonly CLASS too, where the modifier is implied). */` |
|        3 | 1484 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1485 | `				"Hooked properties cannot be readonly");` |
|        3 | 1486 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1487 | `				return SXERR_ABORT;` |
|        - | 1488 | `			}` |
|        3 | 1489 | `			goto Synchronize;` |
|        - | 1490 | `		}` |
|      185 | 1491 | `		rc = GenStateCompilePropertyHooks(&(*pGen),pClass,pAttr);` |
|      185 | 1492 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1493 | `			return SXERR_ABORT;` |
|        - | 1494 | `		}` |
|      185 | 1495 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1496 | `			goto Synchronize;` |
|        - | 1497 | `		}` |
|      185 | 1498 | `		SySetRelease(&aUnionAlts);` |
|      185 | 1499 | `		return SXRET_OK;` |
|        - | 1500 | `	}` |
|     2561 | 1501 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        - | 1502 | ``		/* php 8.4: `abstract` on a property requires a hook list (php's exact`` |
|        - | 1503 | `		 * wording differs per declaration site) */` |
|      ! 0 | 1504 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 1505 | `			(pClass->iFlags & PH7_CLASS_INTERFACE)` |
|        - | 1506 | `				? "Interfaces may only include hooked properties"` |
|        - | 1507 | `				: "Only hooked properties may be declared abstract");` |
|      ! 0 | 1508 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1509 | `			return SXERR_ABORT;` |
|        - | 1510 | `		}` |
|      ! 0 | 1511 | `		goto Synchronize;` |
|        - | 1512 | `	}` |
|     2561 | 1513 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - | 1514 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|        9 | 1515 | `		pGen->pIn++; /* Jump the comma */` |
|        9 | 1516 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|      ! 0 | 1517 | `			SyToken *pTok = pGen->pIn;` |
|      ! 0 | 1518 | `			if( pTok >= pGen->pEnd ){` |
|      ! 0 | 1519 | `				pTok--;` |
|      ! 0 | 1520 | `			}` |
|      ! 0 | 1521 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|        - | 1522 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|      ! 0 | 1523 | `				&pTok->sData,&pClass->sDisp);` |
|      ! 0 | 1524 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1525 | `				return SXERR_ABORT;` |
|        - | 1526 | `			}` |
|      ! 0 | 1527 | `		}else{` |
|        9 | 1528 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        9 | 1529 | `				goto loop;` |
|        - | 1530 | `			}` |
|        - | 1531 | `		}` |
|      ! 0 | 1532 | `	}` |
|     2553 | 1533 | `	SySetRelease(&aUnionAlts);` |
|     2553 | 1534 | `	return SXRET_OK;` |
|       11 | 1535 | `Synchronize:` |
|        - | 1536 | `	/* Synchronize with the first semi-colon */` |
|       76 | 1537 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       54 | 1538 | `		pGen->pIn++;` |
|        4 | 1539 | `	}` |
|       26 | 1540 | `	SySetRelease(&aUnionAlts);` |
|       26 | 1541 | `	return SXERR_CORRUPT;` |
|     1380 | 1542 | `}` |
|        - | 1543 | `/*` |
|        - | 1544 | ` * php validates a magic method's DECLARATION at compile time` |
|        - | 1545 | ` * (zend_check_magic_method_implementation): the ENGINE builds the arguments and` |
|        - | 1546 | ` * calls these methods on its own, so a wrong shape is rejected where it is` |
|        - | 1547 | ` * written rather than discovered — or silently tolerated — at the dispatch.` |
|        - | 1548 | ` *` |
|        - | 1549 | ` * One row per magic name; its fields are the checks php makes for that name.` |
|        - | 1550 | ` * Arity is the first of them, in php's order — which is what decides the message` |
|        - | 1551 | ` * when a declaration breaks more than one of php's rules at once.` |
|        - | 1552 | ` */` |
|        - | 1553 | `typedef struct MagicMethodRule MagicMethodRule;` |
|        - | 1554 | `struct MagicMethodRule` |
|        - | 1555 | `{` |
|        - | 1556 | `	const char *zName; /* Magic method name */` |
|        - | 1557 | `	sxu32 nName;       /* Its length */` |
|        - | 1558 | `	int nArgs;         /* Declared arguments php requires, -1 when it does not check */` |
|        - | 1559 | `	int bStatic;       /* TRUE: must be static · FALSE: must NOT be static */` |
|        - | 1560 | `	int bPublic;       /* TRUE: must be public — a WARNING, and dispatched anyway */` |
|        - | 1561 | `	int bNoReturnType; /* TRUE: declaring ANY return type is a fatal */` |
|        - | 1562 | `};` |
|        - | 1563 | `#define MAGIC_METHOD_ROW(N,A,S,P,R) { N, sizeof(N)-1, A, S, P, R }` |
|        - | 1564 | `static const MagicMethodRule aMagicMethod[] = {` |
|        - | 1565 | `	MAGIC_METHOD_ROW("__construct",  -1, FALSE, FALSE, TRUE),` |
|        - | 1566 | `	MAGIC_METHOD_ROW("__destruct",    0, FALSE, FALSE, TRUE),` |
|        - | 1567 | `	MAGIC_METHOD_ROW("__clone",       0, FALSE, FALSE, FALSE),` |
|        - | 1568 | `	MAGIC_METHOD_ROW("__get",         1, FALSE, TRUE,  FALSE),` |
|        - | 1569 | `	MAGIC_METHOD_ROW("__set",         2, FALSE, TRUE,  FALSE),` |
|        - | 1570 | `	MAGIC_METHOD_ROW("__isset",       1, FALSE, TRUE,  FALSE),` |
|        - | 1571 | `	MAGIC_METHOD_ROW("__unset",       1, FALSE, TRUE,  FALSE),` |
|        - | 1572 | `	MAGIC_METHOD_ROW("__call",        2, FALSE, TRUE,  FALSE),` |
|        - | 1573 | `	MAGIC_METHOD_ROW("__callStatic",  2, TRUE,  TRUE,  FALSE),` |
|        - | 1574 | `	MAGIC_METHOD_ROW("__toString",    0, FALSE, TRUE,  FALSE),` |
|        - | 1575 | `	MAGIC_METHOD_ROW("__invoke",     -1, FALSE, TRUE,  FALSE),` |
|        - | 1576 | `	MAGIC_METHOD_ROW("__debugInfo",   0, FALSE, TRUE,  FALSE),` |
|        - | 1577 | `	MAGIC_METHOD_ROW("__serialize",   0, FALSE, TRUE,  FALSE),` |
|        - | 1578 | `	MAGIC_METHOD_ROW("__unserialize", 1, FALSE, TRUE,  FALSE),` |
|        - | 1579 | `	MAGIC_METHOD_ROW("__sleep",       0, FALSE, TRUE,  FALSE),` |
|        - | 1580 | `	MAGIC_METHOD_ROW("__wakeup",      0, FALSE, TRUE,  FALSE),` |
|        - | 1581 | `	MAGIC_METHOD_ROW("__set_state",   1, TRUE,  TRUE,  FALSE)` |
|        - | 1582 | `};` |
|        - | 1583 | `#undef MAGIC_METHOD_ROW` |
|        - | 1584 | `/*` |
|        - | 1585 | ` * Find the rule for a declared method name, or 0 when the name is not magic.` |
|        - | 1586 | `` * php matches method names case-insensitively everywhere, so `__GET` is `__get`;`` |
|        - | 1587 | ` * the two-underscore prefix test is php's own cheap reject.` |
|        - | 1588 | ` */` |
|   110050 | 1589 | `static const MagicMethodRule * GenStateMagicMethodRule(const SyString *pName)` |
|        5 | 1590 | `{` |
|        - | 1591 | `	sxu32 n;` |
|   110055 | 1592 | `	if( pName->nByte < sizeof("__x")-1 \|\| pName->zString[0] != '_' \|\| pName->zString[1] != '_' ){` |
|     3741 | 1593 | `		return 0;` |
|        - | 1594 | `	}` |
|  1160635 | 1595 | `	for( n = 0 ; n < SX_ARRAYSIZE(aMagicMethod) ; ++n ){` |
|  1160622 | 1596 | `		if( pName->nByte == aMagicMethod[n].nName` |
|   634040 | 1597 | `		 && SyStrnicmp(pName->zString,aMagicMethod[n].zName,aMagicMethod[n].nName) == 0 ){` |
|   106311 | 1598 | `			return &aMagicMethod[n];` |
|        - | 1599 | `		}` |
|   527163 | 1600 | `	}` |
|       10 | 1601 | `	return 0;` |
|    55030 | 1602 | `}` |
|        - | 1603 | `/*` |
|        - | 1604 | ` * TRUE when php requires this magic method to be PUBLIC: the rows it merely` |
|        - | 1605 | ` * WARNS about at the declaration and then dispatches regardless of what the` |
|        - | 1606 | ` * declaration said. The runtime's visibility gate reads this to let an` |
|        - | 1607 | ` * engine-built call through — a call the user WROTE stays denied.` |
|        - | 1608 | ` *` |
|        - | 1609 | `` * `__construct`/`__destruct`/`__clone` are deliberately not in the set: a`` |
|        - | 1610 | ` * private constructor is the singleton idiom, and php enforces those three at` |
|        - | 1611 | ` * the call like any other method.` |
|        - | 1612 | ` */` |
|   105040 | 1613 | `PH7_PRIVATE int PH7_MagicMethodMustBePublic(const SyString *pName)` |
|        5 | 1614 | `{` |
|   105045 | 1615 | `	const MagicMethodRule *pRule = GenStateMagicMethodRule(pName);` |
|   105045 | 1616 | `	return pRule != 0 && pRule->bPublic;` |
|        5 | 1617 | `}` |
|        - | 1618 | `/*` |
|        - | 1619 | ` * Enforce the rules of pRule against the declaration just parsed. pName is the` |
|        - | 1620 | ` * name AS WRITTEN — php quotes that spelling, not the canonical one.` |
|        - | 1621 | ` *` |
|        - | 1622 | ` * The diagnostic is FORMATTED, not reported: php decides these rules while the` |
|        - | 1623 | ` * signature is in hand (so the arity beats __toString's return-type rule) but` |
|        - | 1624 | ` * raises them only once the declaration has cleared the checks php makes` |
|        - | 1625 | ` * first — the redeclaration and abstract-placement rules, and any parse error` |
|        - | 1626 | ` * in the body php has already read. The caller reports the buffer at that` |
|        - | 1627 | ` * point.` |
|        - | 1628 | ` *` |
|        - | 1629 | ` * Returns the severity it wrote: E_ERROR, E_WARNING, or 0 for a clean` |
|        - | 1630 | ` * declaration.` |
|        - | 1631 | ` */` |
|     5010 | 1632 | `static sxi32 GenStateCheckMagicMethod(` |
|        - | 1633 | `	ph7_class *pClass,` |
|        - | 1634 | `	const SyString *pName,` |
|        - | 1635 | `	ph7_class_method *pMeth,` |
|        - | 1636 | `	char *zErr,` |
|        - | 1637 | `	int nErrBuf` |
|        - | 1638 | `	)` |
|        5 | 1639 | `{` |
|     5015 | 1640 | `	const MagicMethodRule *pRule = GenStateMagicMethodRule(pName);` |
|     5015 | 1641 | `	if( pRule == 0 ){` |
|     3749 | 1642 | `		return 0;` |
|        - | 1643 | `	}` |
|     1271 | 1644 | `	if( pRule->nArgs >= 0 ){` |
|        - | 1645 | `		/* php counts DECLARED parameters — an optional one counts` |
|        - | 1646 | ``		 * (`__destruct($a = null)` is rejected) and the variadic tail does not`` |
|        - | 1647 | ``		 * (`__clone(...$a)` declares zero and passes, `__get(...$a)` declares`` |
|        - | 1648 | `		 * zero where one is required and does not). */` |
|      621 | 1649 | `		sxu32 nDecl = SySetUsed(&pMeth->sFunc.aArgs);` |
|      621 | 1650 | `		sxu32 nGiven = 0;` |
|        - | 1651 | `		sxu32 n;` |
|     1071 | 1652 | `		for( n = 0 ; n < nDecl ; ++n ){` |
|      455 | 1653 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,n);` |
|      455 | 1654 | `			if( pArg && (pArg->iFlags & VM_FUNC_ARG_VARIADIC) == 0 ){` |
|      453 | 1655 | `				nGiven++;` |
|      224 | 1656 | `			}` |
|      230 | 1657 | `		}` |
|      621 | 1658 | `		if( nGiven != (sxu32)pRule->nArgs ){` |
|        9 | 1659 | `			if( pRule->nArgs == 0 ){` |
|        4 | 1660 | `				SyBufferFormat(zErr,nErrBuf,"Method %z::%z() cannot take arguments",` |
|        1 | 1661 | `					&pClass->sDisp,pName);` |
|        2 | 1662 | `			}else{` |
|        8 | 1663 | `				SyBufferFormat(zErr,nErrBuf,"Method %z::%z() must take exactly %d argument%s",` |
|        6 | 1664 | `					&pClass->sDisp,pName,pRule->nArgs,pRule->nArgs == 1 ? "" : "s");` |
|        - | 1665 | `			}` |
|        9 | 1666 | `			return E_ERROR;` |
|        - | 1667 | `		}` |
|        - | 1668 | `		/* None of the arguments the engine builds may be by-reference — there is` |
|        - | 1669 | `		 * no caller variable to write back to. php checks as many arguments as` |
|        - | 1670 | `		 * the rule counts, and the count above already skipped variadics, so` |
|        - | 1671 | `		 * this walk skips them the same way. */` |
|     1055 | 1672 | `		for( n = 0 ; n < nDecl ; ++n ){` |
|      446 | 1673 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,n);` |
|      446 | 1674 | `			if( pArg == 0 \|\| (pArg->iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        3 | 1675 | `				continue;` |
|        - | 1676 | `			}` |
|      444 | 1677 | `			if( pArg->iFlags & VM_FUNC_ARG_BY_REF ){` |
|        4 | 1678 | `				SyBufferFormat(zErr,nErrBuf,"Method %z::%z() cannot take arguments by reference",` |
|        1 | 1679 | `					&pClass->sDisp,pName);` |
|        3 | 1680 | `				return E_ERROR;` |
|        - | 1681 | `			}` |
|      223 | 1682 | `		}` |
|      304 | 1683 | `	}` |
|        - | 1684 | `	/* Static-ness. Whether the engine has a receiver for a magic method is not` |
|        - | 1685 | ``	 * the declaration's to choose: `__callStatic` and `__set_state` are reached`` |
|        - | 1686 | `	 * with a class and nothing else, every other row is reached through an` |
|        - | 1687 | `	 * object. PHL took the declaration at its word and then dispatched the` |
|        - | 1688 | ``	 * method anyway — a `static function __get()` ran with no `$this` at all,`` |
|        - | 1689 | `	 * so a hook property table or a lazy-loading accessor read whatever the` |
|        - | 1690 | `	 * unbound scope happened to hold. php checks this after the arity, which is` |
|        - | 1691 | ``	 * why `static function __get($a,$b)` reports the count first. */`` |
|     1263 | 1692 | `	if( pRule->bStatic != ((pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0) ){` |
|        8 | 1693 | `		SyBufferFormat(zErr,nErrBuf,"Method %z::%z() %s be static",` |
|        4 | 1694 | `			&pClass->sDisp,pName,pRule->bStatic ? "must" : "cannot");` |
|        6 | 1695 | `		return E_ERROR;` |
|        - | 1696 | `	}` |
|        - | 1697 | `	/* Visibility. This one is a WARNING: php names the declaration and then` |
|        - | 1698 | ``	 * dispatches the method anyway, because the engine calling `__get` is not`` |
|        - | 1699 | `	 * the outside world reaching for a private member. PHL was silent at the` |
|        - | 1700 | ``	 * declaration and threw `Call to private method C::__get()` at the ACCESS —`` |
|        - | 1701 | `	 * the one rule of this family that changed what a program php RUNS does,` |
|        - | 1702 | `	 * and it killed the script. The dispatch half is` |
|        - | 1703 | `	 * PH7_MagicMethodMustBePublic, read by the runtime visibility gate. */` |
|     1259 | 1704 | `	if( pRule->bPublic && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       67 | 1705 | `		SyBufferFormat(zErr,nErrBuf,"The magic method %z::%z() must have public visibility",` |
|       21 | 1706 | `			&pClass->sDisp,pName);` |
|       46 | 1707 | `		return E_WARNING;` |
|        - | 1708 | `	}` |
|        - | 1709 | ``	/* A return type on the two methods that have no return VALUE. `new C` is the`` |
|        - | 1710 | `	 * instance, never whatever __construct returned, and __destruct is called by` |
|        - | 1711 | `	 * the engine at a point with nowhere to put an answer — so php rejects any` |
|        - | 1712 | ``	 * declared type on either, `void` and `never` included, rather than let a`` |
|        - | 1713 | ``	 * declaration promise something no caller can read. (`__clone` is NOT in`` |
|        - | 1714 | ``	 * this row: `: void` on it is valid php.) PHL enforced the declared type at`` |
|        - | 1715 | ``	 * runtime instead, so `__construct(): int` raised a TypeError at every`` |
|        - | 1716 | `	 * instantiation — a diagnostic on the CALL for a mistake in the` |
|        - | 1717 | `	 * declaration. */` |
|     1212 | 1718 | `	if( pRule->bNoReturnType` |
|     1209 | 1719 | `	 && (pMeth->sFunc.nReturnType > 0` |
|      598 | 1720 | `	  \|\| SyStringLength(&pMeth->sFunc.sReturnClass) > 0` |
|      596 | 1721 | `	  \|\| SySetUsed(&pMeth->sFunc.aReturnUnion) > 0) ){` |
|        8 | 1722 | `		SyBufferFormat(zErr,nErrBuf,"Method %z::%z() cannot declare a return type",` |
|        2 | 1723 | `			&pClass->sDisp,pName);` |
|        6 | 1724 | `		return E_ERROR;` |
|        - | 1725 | `	}` |
|     1213 | 1726 | `	return 0;` |
|     2510 | 1727 | `}` |
|        - | 1728 | `/*` |
|        - | 1729 | ` * Raise the declaration diagnostic parked above (GenStateCheckMagicMethod's magic-method` |
|        - | 1730 | ` * rules, or the final-private one beside its call), once, and disarm it.` |
|        - | 1731 | ` * Suppressed when this declaration has already reported a fatal php decides` |
|        - | 1732 | ` * FIRST — a redeclaration, an abstract method in a non-abstract class, a parse` |
|        - | 1733 | ` * error in the body — since php stops at its own first fatal.` |
|        - | 1734 | ` */` |
|     4986 | 1735 | `static sxi32 GenStateRaiseMagicDiag(` |
|        - | 1736 | `	ph7_gen_state *pGen,` |
|        - | 1737 | `	sxi32 *pnSeverity,   /* IN/OUT: the parked severity, zeroed here */` |
|        - | 1738 | `	const char *zErr,` |
|        - | 1739 | `	sxu32 nLine,` |
|        - | 1740 | `	sxu32 nErrEntry      /* pGen->nErr when this declaration started */` |
|        - | 1741 | `	)` |
|        5 | 1742 | `{` |
|     4991 | 1743 | `	sxi32 rc = SXRET_OK;` |
|     4991 | 1744 | `	if( *pnSeverity != 0 ){` |
|       76 | 1745 | `		if( pGen->nErr == nErrEntry ){` |
|       76 | 1746 | `			rc = PH7_GenCompileError(pGen,*pnSeverity,nLine,"%s",zErr);` |
|       36 | 1747 | `		}` |
|       76 | 1748 | `		*pnSeverity = 0;` |
|       36 | 1749 | `	}` |
|     4991 | 1750 | `	return rc;` |
|        5 | 1751 | `}` |
|        - | 1752 | `/*` |
|        - | 1753 | ` * Compile a class method.` |
|        - | 1754 | ` *` |
|        - | 1755 | ` * Refer to the official documentation for more information` |
|        - | 1756 | ` * on the powerful extension introduced by the PH7 engine` |
|        - | 1757 | ` * to the OO subsystem such as full type hinting,method` |
|        - | 1758 | ` * overloading and many more.` |
|        - | 1759 | ` */` |
|     5014 | 1760 | `static sxi32 GenStateCompileClassMethod(` |
|        - | 1761 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 1762 | `	sxi32 iProtection,   /* Visibility level */` |
|        - | 1763 | `	sxi32 iFlags,        /* Configuration flags */` |
|        - | 1764 | `	int doBody,          /* TRUE to process method body */` |
|        - | 1765 | `	ph7_class *pClass    /* Class this method belongs */` |
|        - | 1766 | `	)` |
|        5 | 1767 | `{` |
|     5019 | 1768 | `	sxu32 nLine = pGen->pIn->nLine;` |
|     5019 | 1769 | `	sxu32 nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|     5019 | 1770 | `	sxu32 nErrEntry = pGen->nErr; /* Errors already reported when this declaration started */` |
|        - | 1771 | `	char zMagicErr[256];          /* Pending magic-method rule violation, reported at the end */` |
|     5019 | 1772 | `	sxi32 nMagicSeverity = 0;     /* E_ERROR / E_WARNING while zMagicErr is still unreported */` |
|     5019 | 1773 | `	int bMagicFatal = FALSE;      /* The parked diagnostic was a fatal: do not install the method */` |
|        - | 1774 | `	ph7_class_method *pMeth;` |
|        - | 1775 | `	sxi32 iFuncFlags;` |
|        - | 1776 | `	SyString *pName;` |
|        - | 1777 | `	SyToken *pEnd;` |
|        - | 1778 | `	sxi32 rc;` |
|        - | 1779 | `	/* Extract visibility level */` |
|     5019 | 1780 | `	iProtection = GetProtectionLevel(iProtection);` |
|     5019 | 1781 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     5019 | 1782 | `	iFuncFlags = 0;` |
|     5019 | 1783 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - | 1784 | `		/* Invalid method name */` |
|      ! 0 | 1785 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|      ! 0 | 1786 | `		if( rc == SXERR_ABORT ){` |
|        - | 1787 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1788 | `			return SXERR_ABORT;` |
|        - | 1789 | `		}` |
|      ! 0 | 1790 | `		goto Synchronize;` |
|        - | 1791 | `	}` |
|     5019 | 1792 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        - | 1793 | `		/* Return by reference,remember that */` |
|        6 | 1794 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|        - | 1795 | `		/* Jump the '&' token */` |
|        6 | 1796 | `		pGen->pIn++;` |
|        2 | 1797 | `	}` |
|     5019 | 1798 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - | 1799 | `		/* Invalid method name */` |
|      ! 0 | 1800 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|      ! 0 | 1801 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1802 | `			return SXERR_ABORT;` |
|        - | 1803 | `		}` |
|      ! 0 | 1804 | `		goto Synchronize;` |
|        - | 1805 | `	}` |
|        - | 1806 | `	/* Peek method name */` |
|     5019 | 1807 | `	pName = &pGen->pIn->sData;` |
|     5019 | 1808 | `	nLine = pGen->pIn->nLine;` |
|        - | 1809 | `	/* Jump the method name */` |
|     5019 | 1810 | `	pGen->pIn++;` |
|     5019 | 1811 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        - | 1812 | `		/* Abstract method. php has THREE answers here and this had one:` |
|        - | 1813 | `		 *` |
|        - | 1814 | `		 *  - an ENUM may not declare an abstract method at all, whatever its` |
|        - | 1815 | `		 *    visibility ("Enum method E::m() must not be abstract");` |
|        - | 1816 | `		 *  - a TRAIT may declare a PRIVATE one -- php 8.0 allowed it, and` |
|        - | 1817 | `		 *    symfony/messenger's BatchHandlerTrait is written that way, so a` |
|        - | 1818 | `		 *    refusal here stops a real component from compiling;` |
|        - | 1819 | ``		 *  - a class (abstract or not) refuses it, in php's own words: `Abstract`` |
|        - | 1820 | ``		 *    function C::m() cannot be declared private`, not this file's older`` |
|        - | 1821 | `		 *    "Access type for abstract method" sentence, which php keeps for an` |
|        - | 1822 | `		 *    INTERFACE member (and which the interface path already spells). */` |
|      253 | 1823 | `		if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|      ! 0 | 1824 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 1825 | `				"Enum method %z::%z() must not be abstract",&pClass->sDisp,pName);` |
|      ! 0 | 1826 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1827 | `				return SXERR_ABORT;` |
|        - | 1828 | `			}` |
|      248 | 1829 | `		}else if( iProtection == PH7_CLASS_PROT_PRIVATE` |
|      130 | 1830 | `		       && (pClass->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|      ! 0 | 1831 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1832 | `				"Abstract function %z::%z() cannot be declared private",` |
|      ! 0 | 1833 | `				&pClass->sDisp,pName);` |
|      ! 0 | 1834 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1835 | `				return SXERR_ABORT;` |
|        - | 1836 | `			}` |
|      ! 0 | 1837 | `		}` |
|        - | 1838 | `		/* Assemble method signature only */` |
|      253 | 1839 | `		doBody = FALSE;` |
|      124 | 1840 | `	}` |
|     5019 | 1841 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1842 | `		/* Syntax error */` |
|      ! 0 | 1843 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Expected '(' after method name '%z'",pName);` |
|      ! 0 | 1844 | `		if( rc == SXERR_ABORT ){` |
|        - | 1845 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1846 | `			return SXERR_ABORT;` |
|        - | 1847 | `		}` |
|      ! 0 | 1848 | `		goto Synchronize;` |
|        - | 1849 | `	}` |
|        - | 1850 | `	/* Allocate a new class_method instance */` |
|     5019 | 1851 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|     5019 | 1852 | `	if( pMeth == 0 ){` |
|      ! 0 | 1853 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1854 | `		return SXERR_ABORT;` |
|        - | 1855 | `	}` |
|     5019 | 1856 | `	pMeth->sFunc.nLine = nKwLine;` |
|     5019 | 1857 | `	GenStateConsumeDoc(&(*pGen),&pMeth->sFunc.sDoc);` |
|     5019 | 1858 | `	if( GenStateConsumeAttrs(&(*pGen),&pMeth->sFunc.aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 1859 | `		return SXERR_ABORT;` |
|        - | 1860 | `	}` |
|     5019 | 1861 | `	if( GenStateCheckAttrPlacement(&(*pGen),&pMeth->sFunc.aAttrs,4,4,0,0) == SXERR_ABORT ){` |
|      ! 0 | 1862 | `		return SXERR_ABORT;` |
|        - | 1863 | `	}` |
|        - | 1864 | `	/* Jump the left parenthesis '(' */` |
|     5019 | 1865 | `	pGen->pIn++;` |
|     5019 | 1866 | `	pEnd = 0; /* cc warning */` |
|        - | 1867 | `	/* Delimit the method signature */` |
|     5019 | 1868 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     5019 | 1869 | `	if( pEnd >= pGen->pEnd ){` |
|        - | 1870 | `		/* Syntax error */` |
|        3 | 1871 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Missing ')' after method '%z' declaration",pName);` |
|        3 | 1872 | `		if( rc == SXERR_ABORT ){` |
|        - | 1873 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1874 | `			return SXERR_ABORT;` |
|        - | 1875 | `		}` |
|        3 | 1876 | `		goto Synchronize;` |
|        - | 1877 | `	}` |
|        - | 1878 | `	{` |
|     5017 | 1879 | `		int bIsCtor = 0;` |
|     5017 | 1880 | `		int bAbstractCtor = 0;` |
|        - | 1881 | `		/* Only __construct is the constructor (PHP-4 class-name constructors removed` |
|        - | 1882 | `		 * in 8.0): a method named like the class is a plain method, so promoted` |
|        - | 1883 | `		 * properties in it are rejected exactly as php does elsewhere. */` |
|     5012 | 1884 | `		if( pName->nByte == sizeof("__construct") - 1` |
|     2940 | 1885 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0 ){` |
|      555 | 1886 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       15 | 1887 | `				bAbstractCtor = 1;` |
|        9 | 1888 | `			}else{` |
|      543 | 1889 | `				bIsCtor = 1;` |
|        - | 1890 | `			}` |
|      275 | 1891 | `		}` |
|     5017 | 1892 | `		if( pGen->pIn < pEnd ){` |
|        - | 1893 | `			/* Collect method arguments */` |
|     1861 | 1894 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|     1861 | 1895 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1896 | `				return SXERR_ABORT;` |
|        - | 1897 | `			}` |
|      928 | 1898 | `		}` |
|        - | 1899 | `	}` |
|        - | 1900 | `	/* Point past ')' and parse optional return type ': type' */` |
|     5017 | 1901 | `	pGen->pIn = &pEnd[1];` |
|        - | 1902 | `	{` |
|     5017 | 1903 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|     5017 | 1904 | `		if( rcRt == SXERR_ABORT ){` |
|      ! 0 | 1905 | `			return SXERR_ABORT;` |
|     5017 | 1906 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|        2 | 1907 | `			goto Synchronize;` |
|        - | 1908 | `		}` |
|        - | 1909 | `	}` |
|        - | 1910 | `	/* php's #[\NoDiscard] declaration rules, which want the return type. */` |
|     5439 | 1911 | `	if( GenStateApplyNoDiscard(&(*pGen),&pMeth->sFunc,pClass,` |
|     5010 | 1912 | `			pName->nByte == sizeof("__construct")-1` |
|     2934 | 1913 | `			 && SyStrnicmp(pName->zString,"__construct",sizeof("__construct")-1) == 0)` |
|     2510 | 1914 | `		== SXERR_ABORT ){` |
|      ! 0 | 1915 | `		return SXERR_ABORT;` |
|        - | 1916 | `	}` |
|        - | 1917 | `	/* php's compile-time magic-method declaration rules, DECIDED here — with the` |
|        - | 1918 | `	 * signature in hand and before the __toString return-type rule below, which` |
|        - | 1919 | ``	 * is php's own order (`static function __toString($a): int` reports the`` |
|        - | 1920 | `	 * arity). Reported at the end of this function; see zMagicErr there. */` |
|     5015 | 1921 | `	nMagicSeverity = GenStateCheckMagicMethod(pClass,pName,pMeth,zMagicErr,(int)sizeof(zMagicErr));` |
|     5010 | 1922 | `	if( nMagicSeverity == 0` |
|     4981 | 1923 | `	 && (pMeth->iFlags & PH7_CLASS_ATTR_FINAL)` |
|     2491 | 1924 | `	 && pMeth->iProtection == PH7_CLASS_PROT_PRIVATE` |
|       29 | 1925 | `	 && !(pName->nByte == sizeof("__construct")-1` |
|        9 | 1926 | `	   && SyStrnicmp(pName->zString,"__construct",sizeof("__construct")-1) == 0) ){` |
|        - | 1927 | `		/* Not a magic rule, but the same KIND of rule and the same parking: php` |
|        - | 1928 | `		 * checks a declaration php itself decides the meaning of. A private method` |
|        - | 1929 | ``		 * is never overridden, so `final` on one says nothing — php WARNS here (8.0+)`` |
|        - | 1930 | `		 * and compiles the class. PHL was silent at the declaration and then fataled` |
|        - | 1931 | `		 * at the SUBCLASS that reused the name ("Cannot override final method"), a` |
|        - | 1932 | `		 * class php accepts; the inheritance half is in PH7_ClassInherit.` |
|        - | 1933 | `		 *` |
|        - | 1934 | `		 * The CONSTRUCTOR is php's one exemption, and it is a deliberate one:` |
|        - | 1935 | ``		 * `final private function __construct()` is the singleton idiom -- private`` |
|        - | 1936 | ``		 * to stop `new`, final to stop a subclass widening it back to public -- so`` |
|        - | 1937 | `		 * the modifier does say something there. Every other private method warns,` |
|        - | 1938 | ``		 * `__destruct`, `__clone` and a static one included. PHPUnit's TestSuite`` |
|        - | 1939 | `		 * declares exactly this and drew the warning on every single run. */` |
|       16 | 1940 | `		SyBufferFormat(zMagicErr,sizeof(zMagicErr),` |
|        - | 1941 | `			"Private methods cannot be final as they are never overridden by other classes");` |
|        - | 1942 | `		/* php's E_COMPILE_WARNING, not the E_WARNING the visibility rules above` |
|        - | 1943 | `		 * raise -- swept out of php 8.5, which passes the visibility ones to a` |
|        - | 1944 | `		 * user error handler and this one straight to default processing, and` |
|        - | 1945 | `		 * hides exactly one of the two under` |
|        - | 1946 | ``		 * `error_reporting(E_ALL & ~E_COMPILE_WARNING)`. */`` |
|       16 | 1947 | `		nMagicSeverity = 128 /* E_COMPILE_WARNING */;` |
|        7 | 1948 | `	}` |
|     5015 | 1949 | `	if( nMagicSeverity == E_ERROR ){` |
|        - | 1950 | `		/* Suppress the __toString rule below: php never reaches it on a` |
|        - | 1951 | `		 * declaration the magic rules already rejected. */` |
|       20 | 1952 | `		bMagicFatal = TRUE;` |
|       20 | 1953 | `		goto SkipToStringType;` |
|        - | 1954 | `	}` |
|        - | 1955 | `	/*` |
|        - | 1956 | ``	 * php gives __toString() an IMPLICIT `string` return type. That is what makes`` |
|        - | 1957 | ``	 * `return 42` coerce to "42" and `return null` / an array / an object / falling`` |
|        - | 1958 | `	 * off the end raise` |
|        - | 1959 | `	 *   C::__toString(): Return value must be of type string, X returned` |
|        - | 1960 | `	 * PHL enforced DECLARED return types only, so an undeclared __toString could` |
|        - | 1961 | `	 * answer anything and MemObjStringValue fell back to the "Object" placeholder` |
|        - | 1962 | `	 * for whatever was not a non-empty string. Installing the type here reuses the` |
|        - | 1963 | `	 * enforcement that already matches php byte for byte.` |
|        - | 1964 | `	 *` |
|        - | 1965 | `	 * sReturnTypeName is filled in as well, for two reasons: reflection reports the` |
|        - | 1966 | `	 * implicit type exactly as php does (hasReturnType() TRUE, getReturnType()` |
|        - | 1967 | `	 * "string" for an undeclared __toString), and the generator-return-type fatal` |
|        - | 1968 | ``	 * renders from it — so a __toString with a `yield` in it now reports php's`` |
|        - | 1969 | `	 * "Generator return type must be a supertype of Generator, string given".` |
|        - | 1970 | `	 *` |
|        - | 1971 | ``	 * Declaring any OTHER return type is php's own compile fatal, `?string`, a`` |
|        - | 1972 | ``	 * union, `mixed`, `static` and `void` included. (php checks`` |
|        - | 1973 | ``	 * "A void method must not return a value" FIRST when a `: void` __toString also`` |
|        - | 1974 | `	 * returns a value; PHL has no such check yet, so it reports this one instead —` |
|        - | 1975 | `	 * both reject, on doubly-invalid input only.)` |
|        - | 1976 | `	 */` |
|     4994 | 1977 | `	if( pName->nByte == sizeof("__toString")-1` |
|     2757 | 1978 | `	 && SyStrnicmp(pName->zString,"__toString",sizeof("__toString")-1) == 0 ){` |
|      185 | 1979 | `		ph7_vm_func *pTsFunc = &pMeth->sFunc;` |
|      185 | 1980 | `		int bTsDeclared = pTsFunc->nReturnType > 0 \|\| SySetUsed(&pTsFunc->aReturnUnion) > 0;` |
|      185 | 1981 | `		if( bTsDeclared ){` |
|      108 | 1982 | `			if( pTsFunc->nReturnType != MEMOBJ_STRING` |
|      107 | 1983 | `			 \|\| SySetUsed(&pTsFunc->aReturnUnion) > 0` |
|      111 | 1984 | `			 \|\| (pTsFunc->iFlags & VM_FUNC_RETURN_NULLABLE) ){` |
|        - | 1985 | `				/* php raises this one AFTER the magic rules, so a parked` |
|        - | 1986 | `				 * visibility warning is php's first line here rather than a` |
|        - | 1987 | `				 * casualty of the fatal about to be counted. */` |
|        6 | 1988 | `				if( GenStateRaiseMagicDiag(pGen,&nMagicSeverity,zMagicErr,` |
|        6 | 1989 | `						nKwLine,nErrEntry) == SXERR_ABORT ){` |
|      ! 0 | 1990 | `					return SXERR_ABORT;` |
|        - | 1991 | `				}` |
|        8 | 1992 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1993 | `					"%z::%z(): Return type must be string when declared",` |
|        2 | 1994 | `					&pClass->sDisp,pName);` |
|        6 | 1995 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1996 | `					return SXERR_ABORT;` |
|        - | 1997 | `				}` |
|        6 | 1998 | `				goto Synchronize;` |
|        - | 1999 | `			}` |
|       57 | 2000 | `		}else{` |
|       77 | 2001 | `			char *zTsType = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        - | 2002 | `				"string",sizeof("string")-1);` |
|       77 | 2003 | `			pTsFunc->nReturnType = MEMOBJ_STRING;` |
|       77 | 2004 | `			if( zTsType ){` |
|       77 | 2005 | `				SyStringInitFromBuf(&pTsFunc->sReturnTypeName,zTsType,sizeof("string")-1);` |
|       36 | 2006 | `			}` |
|        - | 2007 | `		}` |
|       88 | 2008 | `	}` |
|      ! 0 | 2009 | `SkipToStringType:` |
|        - | 2010 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|        - | 2011 | `	 * property init/typecheck is handled by the generic typed-property path` |
|        - | 2012 | `	 * since we mint real ph7_class_attr entries. */` |
|        - | 2013 | `	{` |
|     5011 | 2014 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|        - | 2015 | `		sxu32 i;` |
|     7629 | 2016 | `		for( i = 0; i < nArg; i++ ){` |
|     2633 | 2017 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|        - | 2018 | `			ph7_class_attr *pAttr;` |
|     2633 | 2019 | `			sxi32 iAttrFlags = 0;` |
|        - | 2020 | `			int bArgTyped;` |
|     2633 | 2021 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|     2451 | 2022 | `				continue;` |
|        - | 2023 | `			}` |
|        - | 2024 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|        - | 2025 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|        - | 2026 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|      142 | 2027 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|      205 | 2028 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|      187 | 2029 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        3 | 2030 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2031 | `					"Cannot declare variadic promoted property");` |
|        3 | 2032 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2033 | `					return SXERR_ABORT;` |
|        - | 2034 | `				}` |
|        3 | 2035 | `				goto Synchronize;` |
|        - | 2036 | `			}` |
|        - | 2037 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|        - | 2038 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|        - | 2039 | `			 * appear as an alternative of a union type. */` |
|      185 | 2040 | `			if( bArgTyped ){` |
|      218 | 2041 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|      142 | 2042 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|      142 | 2043 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|       71 | 2044 | `					"Property %z::$%z cannot have type %z",nLine);` |
|      147 | 2045 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2046 | `					return SXERR_ABORT;` |
|      147 | 2047 | `				}else if( rc != SXRET_OK ){` |
|        6 | 2048 | `					goto Synchronize;` |
|        - | 2049 | `				}` |
|       69 | 2050 | `			}` |
|        - | 2051 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|      181 | 2052 | `			if( GenStateExtractProperty(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|        4 | 2053 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 2054 | `					"Cannot redeclare %z::$%z",&pClass->sDisp,&pArg->sName);` |
|        3 | 2055 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2056 | `					return SXERR_ABORT;` |
|        - | 2057 | `				}` |
|        3 | 2058 | `				goto Synchronize;` |
|        - | 2059 | `			}` |
|      179 | 2060 | `			if( bArgTyped ){` |
|      141 | 2061 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|       68 | 2062 | `			}` |
|      179 | 2063 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|        3 | 2064 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|        1 | 2065 | `			}` |
|      179 | 2066 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|        8 | 2067 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|        3 | 2068 | `			}` |
|      179 | 2069 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|        - | 2070 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|        - | 2071 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|       45 | 2072 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|        4 | 2073 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 2074 | `						"Readonly property %z::$%z must have type",&pClass->sDisp,&pArg->sName);` |
|        3 | 2075 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 2076 | `						return SXERR_ABORT;` |
|        - | 2077 | `					}` |
|        3 | 2078 | `					goto Synchronize;` |
|        - | 2079 | `				}` |
|       43 | 2080 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       19 | 2081 | `			}` |
|      177 | 2082 | `			if( pArg->iFlags & VM_FUNC_ARG_FINAL ){` |
|        - | 2083 | ``				/* PHP 8.4's `final` on a promoted property. No "final and private"`` |
|        - | 2084 | `				 * screen here: php refuses that pair in a CLASS BODY and accepts it` |
|        - | 2085 | ``				 * on a promoted parameter (`final private int $p` reflects as`` |
|        - | 2086 | `				 * modifiers 36), which is php's own asymmetry, not a gap. */` |
|        7 | 2087 | `				iAttrFlags \|= PH7_CLASS_ATTR_FINAL;` |
|        3 | 2088 | `			}` |
|      177 | 2089 | `			if( pArg->iFlags & (VM_FUNC_ARG_PRIV_SET\|VM_FUNC_ARG_PROT_SET) ){` |
|        - | 2090 | `				/* Asymmetric set-visibility on a promoted property (PHP 8.4) */` |
|        5 | 2091 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 | 2092 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2093 | `						"Property with asymmetric visibility %z::$%z must have type",` |
|      ! 0 | 2094 | `						&pClass->sDisp,&pArg->sName);` |
|      ! 0 | 2095 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 2096 | `						return SXERR_ABORT;` |
|        - | 2097 | `					}` |
|      ! 0 | 2098 | `					goto Synchronize;` |
|        - | 2099 | `				}` |
|        5 | 2100 | `				iAttrFlags \|= (pArg->iFlags & VM_FUNC_ARG_PRIV_SET)` |
|        2 | 2101 | `					? PH7_CLASS_ATTR_PRIVATE_SET : PH7_CLASS_ATTR_PROTECTED_SET;` |
|        2 | 2102 | `			}` |
|      177 | 2103 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|      177 | 2104 | `			if( pAttr == 0 ){` |
|      ! 0 | 2105 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2106 | `				return SXERR_ABORT;` |
|        - | 2107 | `			}` |
|      177 | 2108 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|      141 | 2109 | `				pAttr->nType = pArg->nType;` |
|      141 | 2110 | `				pAttr->sClass = pArg->sClass;` |
|      141 | 2111 | `				pAttr->sTypeName = pArg->sTypeName;` |
|      141 | 2112 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|        - | 2113 | `					sxu32 k;` |
|       20 | 2114 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|       14 | 2115 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|       14 | 2116 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|        8 | 2117 | `					}` |
|        3 | 2118 | `				}` |
|       68 | 2119 | `			}` |
|        - | 2120 | ``			/* A promoted parameter's `#[...]` belongs to BOTH members in php: the`` |
|        - | 2121 | `			 * ReflectionParameter reports it and so does the ReflectionProperty,` |
|        - | 2122 | ``			 * which is what makes `#[\Override] public $p` in a constructor`` |
|        - | 2123 | `			 * signature a PROPERTY claim. The records are shared, not copied --` |
|        - | 2124 | `			 * the parameter owns them for the VM's lifetime. */` |
|        - | 2125 | `			{` |
|      177 | 2126 | `				ph7_attribute *aSrc = (ph7_attribute *)SySetBasePtr(&pArg->aAttrs);` |
|        - | 2127 | `				sxu32 k;` |
|      191 | 2128 | `				for( k = 0 ; k < SySetUsed(&pArg->aAttrs) ; k++ ){` |
|       16 | 2129 | `					SySetPut(&pAttr->aAttrs,(const void *)&aSrc[k]);` |
|        9 | 2130 | `				}` |
|        - | 2131 | `			}` |
|      177 | 2132 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|      177 | 2133 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 2134 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2135 | `				return SXERR_ABORT;` |
|        - | 2136 | `			}` |
|       91 | 2137 | `		}` |
|        - | 2138 | `	}` |
|     5001 | 2139 | `	if( doBody ){` |
|        - | 2140 | `		/* Compile method body */` |
|     4755 | 2141 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|     4755 | 2142 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2143 | `			return SXERR_ABORT;` |
|        - | 2144 | `		}` |
|        - | 2145 | `		/* The cursor sits just past the body's closing brace */` |
|     4755 | 2146 | `		pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|     2380 | 2147 | `	}else{` |
|        - | 2148 | `		/* Abstract/interface method: declaration ends at the ';' */` |
|      251 | 2149 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) ){` |
|      245 | 2150 | `			pMeth->sFunc.nEndLine = pGen->pIn->nLine;` |
|      120 | 2151 | `		}` |
|        - | 2152 | `		/* Only method signature is allowed */` |
|      251 | 2153 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|        - | 2154 | `			/* php words this as the declaration's problem rather than a missing` |
|        - | 2155 | `			 * token -- an abstract method (an interface's included, which is` |
|        - | 2156 | `			 * abstract by being one) is a promise, and a body makes it something` |
|        - | 2157 | `			 * else. The two kinds get the two nouns php uses. */` |
|        9 | 2158 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       12 | 2159 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2160 | `					"%s function %z::%z() cannot contain body",` |
|        6 | 2161 | `					(pClass->iFlags & PH7_CLASS_INTERFACE) ? "Interface" : "Abstract",` |
|        3 | 2162 | `					&pClass->sDisp,pName);` |
|        9 | 2163 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2164 | `					return SXERR_ABORT;` |
|        - | 2165 | `				}` |
|        9 | 2166 | `				return SXERR_CORRUPT;` |
|        - | 2167 | `			}` |
|      ! 0 | 2168 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|      ! 0 | 2169 | `				"Expected ';' after method signature '%z'",pName);` |
|      ! 0 | 2170 | `				if( rc == SXERR_ABORT ){` |
|        - | 2171 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 | 2172 | `					return SXERR_ABORT;` |
|        - | 2173 | `				}` |
|      ! 0 | 2174 | `				return SXERR_CORRUPT;` |
|        - | 2175 | `			}` |
|        - | 2176 | `	}` |
|        - | 2177 | `	/* php: an abstract method in a class that was not DECLARED abstract is a fatal,` |
|        - | 2178 | `	 * and it names the method -- distinct from the "contains N abstract methods"` |
|        - | 2179 | `	 * wording php uses for an unimplemented INHERITED one, which` |
|        - | 2180 | `	 * GenStateCheckAbstractMethods already handles. Interfaces and traits may carry` |
|        - | 2181 | `	 * abstract methods freely. */` |
|     4990 | 2182 | `	if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT)` |
|     2620 | 2183 | `	 && (pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0 ){` |
|        4 | 2184 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2185 | `			"Class %z declares abstract method %z() and must therefore be declared abstract",` |
|        1 | 2186 | `			&pClass->sDisp,pName);` |
|        3 | 2187 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2188 | `			return SXERR_ABORT;` |
|        - | 2189 | `		}` |
|        3 | 2190 | `		return SXRET_OK;` |
|        - | 2191 | `	}` |
|        - | 2192 | `	/* php: two methods of the same name in one class body is a fatal, and the` |
|        - | 2193 | ``	 * comparison is case-INSENSITIVE (hMethod now matches that way), so `f` and`` |
|        - | 2194 | ``	 * `F` collide. php names the offending declaration with the spelling used at`` |
|        - | 2195 | `	 * the SECOND site. */` |
|     4993 | 2196 | `	if( PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte) != 0 ){` |
|       11 | 2197 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        3 | 2198 | `			"Cannot redeclare %z::%z()",&pClass->sDisp,pName);` |
|        8 | 2199 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2200 | `			return SXERR_ABORT;` |
|        - | 2201 | `		}` |
|        8 | 2202 | `		return SXRET_OK;` |
|        - | 2203 | `	}` |
|        - | 2204 | `	/* The magic-method rule this declaration broke (decided above, with the` |
|        - | 2205 | `	 * signature in hand). It is raised HERE because php raises it last of the` |
|        - | 2206 | `	 * declaration's fatals: a redeclaration, an abstract method in a class that` |
|        - | 2207 | `	 * is not abstract, and any parse error inside the body php has already read` |
|        - | 2208 | `	 * all win — and each of them has, by now, either returned or bumped nErr.` |
|        - | 2209 | `	 * php stops at its first fatal, so one is all this declaration reports. The` |
|        - | 2210 | ``	 * line is the `function` KEYWORD's, which is where php points once a`` |
|        - | 2211 | `	 * signature wraps across lines. */` |
|     4987 | 2212 | `	if( GenStateRaiseMagicDiag(pGen,&nMagicSeverity,zMagicErr,nKwLine,nErrEntry) == SXERR_ABORT ){` |
|      ! 0 | 2213 | `		return SXERR_ABORT;` |
|        - | 2214 | `	}` |
|     4987 | 2215 | `	if( bMagicFatal ){` |
|        - | 2216 | `		/* Never install a method php refused to declare. A WARNING falls through:` |
|        - | 2217 | `		 * php keeps the method and calls it. */` |
|       20 | 2218 | `		return SXRET_OK;` |
|        - | 2219 | `	}` |
|        - | 2220 | `	/* All done,install the method */` |
|     4971 | 2221 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|     4971 | 2222 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 2223 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2224 | `		return SXERR_ABORT;` |
|        - | 2225 | `	}` |
|     4971 | 2226 | `	return SXRET_OK;` |
|        9 | 2227 | `Synchronize:` |
|        - | 2228 | `	/* Synchronize with the first semi-colon */` |
|       58 | 2229 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       40 | 2230 | `		pGen->pIn++;` |
|        4 | 2231 | `	}` |
|       22 | 2232 | `	return SXERR_CORRUPT;` |
|     2512 | 2233 | `}` |
|        - | 2234 | `/*` |
|        - | 2235 | ` * php's member-modifier RUN and the screens on it.` |
|        - | 2236 | ` *` |
|        - | 2237 | ` * Everything a class/trait/interface member may carry in front of the` |
|        - | 2238 | ``  * declaration it modifies -- the read visibility, an asymmetric `(set)` `` |
|        - | 2239 | ``  * visibility, `static`, `abstract`, `final` and the context-sensitive `readonly` `` |
|        - | 2240 | ` * -- in ANY order and each at most once. They are a SET in php's grammar, so` |
|        - | 2241 | `` * `final public static int $p` and `static final public $p` are one declaration`` |
|        - | 2242 | ` * each.` |
|        - | 2243 | ` *` |
|        - | 2244 | ` * The three body loops used to read ONE modifier per branch and then re-enter` |
|        - | 2245 | ` * their keyword ladder, which works only while every branch happens to be` |
|        - | 2246 | `` * reachable from every other: `static final public $p` fell out of the chain`` |
|        - | 2247 | `` * with "Unexpected token 'final'", and the `final` branch only ever led to a`` |
|        - | 2248 | ` * method or a constant -- so PHP 8.4's final PROPERTY was a parse error in every` |
|        - | 2249 | ` * one of its spellings, promoted parameter included. Reading the whole run in` |
|        - | 2250 | ` * one place makes the order irrelevant and gives the duplicate/combination rules` |
|        - | 2251 | `` * a single home; the caller dispatches on the token the run stops at (`const`,`` |
|        - | 2252 | `` * `function`, `var`, a type, a `$name`).`` |
|        - | 2253 | ` */` |
|        - | 2254 | ``#define GEN_MEMBER_PROP   0  /* `[type] $name`  */`` |
|        - | 2255 | ``#define GEN_MEMBER_CONST  1  /* `const NAME`    */`` |
|        - | 2256 | ``#define GEN_MEMBER_METHOD 2  /* `function name` */`` |
|        - | 2257 | ``#define GEN_MEMBER_VAR    3  /* the pre-5.0 `var $name` spelling */`` |
|        - | 2258 | `typedef struct GenMemberMods GenMemberMods;` |
|        - | 2259 | `struct GenMemberMods` |
|        - | 2260 | `{` |
|        - | 2261 | `	sxi32 iProtection;  /* read-visibility keyword; php's default is public */` |
|        - | 2262 | `	sxi32 iFlags;       /* PH7_CLASS_ATTR_* collected from the run */` |
|        - | 2263 | ``	sxi32 nSetVis;      /* the `(set)` visibility keyword, when one was written */`` |
|        - | 2264 | `	sxu32 nLine;        /* line the run starts on -- where php reports its refusals */` |
|        - | 2265 | `	int bAny;           /* TRUE once anything at all was consumed */` |
|        - | 2266 | `	int bVis,bSetVis,bStatic,bAbstract,bFinal,bReadonly;` |
|        - | 2267 | `};` |
|        - | 2268 | `/*` |
|        - | 2269 | ` * Read the run. A modifier written twice is php's own compile-time fatal, worded` |
|        - | 2270 | ` * per modifier -- and the two VISIBILITY kinds share one sentence, which is also` |
|        - | 2271 | `` * what php says for two DIFFERENT ones (`public private $p`).`` |
|        - | 2272 | ` *` |
|        - | 2273 | ` * Returns SXRET_OK, SXERR_SYNTAX when a rule above was reported (the caller` |
|        - | 2274 | ` * abandons the member), or SXERR_ABORT when the error budget is spent.` |
|        - | 2275 | ` */` |
|     8470 | 2276 | `static sxi32 GenStateReadMemberMods(ph7_gen_state *pGen,GenMemberMods *pMods)` |
|        5 | 2277 | `{` |
|     8475 | 2278 | `	pMods->iProtection = PH7_TKWRD_PUBLIC;` |
|     8475 | 2279 | `	pMods->iFlags = 0;` |
|     8475 | 2280 | `	pMods->nSetVis = 0;` |
|     8475 | 2281 | `	pMods->nLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0;` |
|     8475 | 2282 | `	pMods->bAny = pMods->bVis = pMods->bSetVis = 0;` |
|     8475 | 2283 | `	pMods->bStatic = pMods->bAbstract = pMods->bFinal = pMods->bReadonly = 0;` |
|    17015 | 2284 | `	while( pGen->pIn < pGen->pEnd ){` |
|    17015 | 2285 | `		const char *zTwice = 0;  /* the modifier php names; "" = the access-type sentence */` |
|    17015 | 2286 | `		int nSetTok = 0;` |
|        - | 2287 | `		sxi32 nSetVis;` |
|    17015 | 2288 | `		if( GenStateIsReadonly(pGen->pIn) ){` |
|        - | 2289 | ``			/* `readonly` is not a reserved word, so it arrives as a plain ID; at`` |
|        - | 2290 | `			 * modifier position it is always the modifier -- which is why php's` |
|        - | 2291 | ``			 * answer to `public readonly readonly $x` is the duplicate rule and`` |
|        - | 2292 | ``			 * not a property typed `readonly`. */`` |
|       83 | 2293 | `			if( pMods->bReadonly ){` |
|        3 | 2294 | `				zTwice = "readonly";` |
|        1 | 2295 | `			}` |
|       83 | 2296 | `			pMods->bReadonly = 1;` |
|       83 | 2297 | `			pMods->iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       83 | 2298 | `			pGen->pIn++;` |
|    16976 | 2299 | `		}else if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     5255 | 2300 | `			break;` |
|    14897 | 2301 | `		}else if( (nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok)) != 0 ){` |
|       45 | 2302 | `			if( pMods->bSetVis ){` |
|      ! 0 | 2303 | `				zTwice = "";` |
|      ! 0 | 2304 | `			}` |
|       45 | 2305 | `			pMods->bSetVis = 1;` |
|       45 | 2306 | `			pMods->nSetVis = nSetVis;` |
|       45 | 2307 | `			pMods->iFlags \|= GenStateSetVisFlag(nSetVis);` |
|       45 | 2308 | `			pGen->pIn += nSetTok;` |
|       24 | 2309 | `		}else{` |
|    14855 | 2310 | `			sxi32 nKw = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    14850 | 2311 | `			if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PRIVATE` |
|     8137 | 2312 | `			 \|\| nKw == PH7_TKWRD_PROTECTED ){` |
|     7275 | 2313 | `				if( pMods->bVis ){` |
|        3 | 2314 | `					zTwice = "";` |
|        1 | 2315 | `				}` |
|     7275 | 2316 | `				pMods->bVis = 1;` |
|     7275 | 2317 | `				pMods->iProtection = nKw;` |
|    11220 | 2318 | `			}else if( nKw == PH7_TKWRD_STATIC ){` |
|      939 | 2319 | `				if( pMods->bStatic ){` |
|        3 | 2320 | `					zTwice = "static";` |
|        1 | 2321 | `				}` |
|      939 | 2322 | `				pMods->bStatic = 1;` |
|      939 | 2323 | `				pMods->iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|     7118 | 2324 | `			}else if( nKw == PH7_TKWRD_ABSTRACT ){` |
|      127 | 2325 | `				if( pMods->bAbstract ){` |
|        3 | 2326 | `					zTwice = "abstract";` |
|        1 | 2327 | `				}` |
|      127 | 2328 | `				pMods->bAbstract = 1;` |
|      127 | 2329 | `				pMods->iFlags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|     6590 | 2330 | `			}else if( nKw == PH7_TKWRD_FINAL ){` |
|      109 | 2331 | `				if( pMods->bFinal ){` |
|        3 | 2332 | `					zTwice = "final";` |
|        1 | 2333 | `				}` |
|      109 | 2334 | `				pMods->bFinal = 1;` |
|      109 | 2335 | `				pMods->iFlags \|= PH7_CLASS_ATTR_FINAL;` |
|       57 | 2336 | `			}else{` |
|     6425 | 2337 | `				break; /* not a modifier -- the member itself starts here */` |
|        - | 2338 | `			}` |
|     8435 | 2339 | `			pGen->pIn++;` |
|        - | 2340 | `		}` |
|     8555 | 2341 | `		pMods->bAny = 1;` |
|     8555 | 2342 | `		if( zTwice ){` |
|        - | 2343 | `			sxi32 rc;` |
|       14 | 2344 | `			pGen->iFatalTrace = PH7_FATAL_TRACE_NONE;` |
|       19 | 2345 | `			rc = zTwice[0]` |
|       12 | 2346 | `				? PH7_GenCompileError(pGen,E_ERROR,pMods->nLine,` |
|        4 | 2347 | `					"Multiple %s modifiers are not allowed",zTwice)` |
|        6 | 2348 | `				: PH7_GenCompileError(pGen,E_ERROR,pMods->nLine,` |
|        - | 2349 | `					"Multiple access type modifiers are not allowed");` |
|       14 | 2350 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 2351 | `		}` |
|        5 | 2352 | `	}` |
|     8465 | 2353 | `	return SXRET_OK;` |
|     4240 | 2354 | `}` |
|        - | 2355 | ``/* php's name for a `(set)` visibility, as its refusals spell it. */`` |
|        2 | 2356 | `static const char * GenStateSetVisWord(sxi32 nKw)` |
|        1 | 2357 | `{` |
|        3 | 2358 | `	if( nKw == PH7_TKWRD_PRIVATE ){` |
|        3 | 2359 | `		return "private(set)";` |
|        - | 2360 | `	}` |
|      ! 0 | 2361 | `	return (nKw == PH7_TKWRD_PROTECTED) ? "protected(set)" : "public(set)";` |
|        2 | 2362 | `}` |
|        - | 2363 | `/*` |
|        - | 2364 | ` * The run, judged against the KIND of declaration it turned out to modify. php` |
|        - | 2365 | ` * refuses each combination with its own sentence, and the order tested below is` |
|        - | 2366 | ` * the order php reports them in when one declaration breaks several` |
|        - | 2367 | ``  * (`static abstract const` is the static rule, `public private(set) static const` `` |
|        - | 2368 | ` * the private(set) one).` |
|        - | 2369 | ` */` |
|     8452 | 2370 | `static sxi32 GenStateScreenMemberMods(ph7_gen_state *pGen,const GenMemberMods *pMods,` |
|        - | 2371 | `	int iKind,ph7_class *pClass)` |
|        5 | 2372 | `{` |
|     8457 | 2373 | `	const char *zBad = 0;   /* the modifier php names */` |
|     8457 | 2374 | `	const char *zWhere = 0; /* ...and what it was written on */` |
|        - | 2375 | `	sxi32 rc;` |
|     8457 | 2376 | `	if( iKind == GEN_MEMBER_CONST ){` |
|      681 | 2377 | `		zWhere = "a class constant";` |
|      681 | 2378 | `		if( pMods->bReadonly ){` |
|      ! 0 | 2379 | `			zBad = "readonly";` |
|      681 | 2380 | `		}else if( pMods->bSetVis ){` |
|        3 | 2381 | `			zBad = GenStateSetVisWord(pMods->nSetVis);` |
|      680 | 2382 | `		}else if( pMods->bStatic ){` |
|        3 | 2383 | `			zBad = "static";` |
|      678 | 2384 | `		}else if( pMods->bAbstract ){` |
|        3 | 2385 | `			zBad = "abstract";` |
|        6 | 2386 | `		}` |
|     8119 | 2387 | `	}else if( iKind == GEN_MEMBER_METHOD ){` |
|     5029 | 2388 | `		zWhere = "a method";` |
|     5029 | 2389 | `		if( pMods->bReadonly ){` |
|        3 | 2390 | `			zBad = "readonly";` |
|     5028 | 2391 | `		}else if( pMods->bSetVis ){` |
|      ! 0 | 2392 | `			zBad = GenStateSetVisWord(pMods->nSetVis);` |
|     5027 | 2393 | `		}else if( pMods->bFinal && pMods->bAbstract ){` |
|        3 | 2394 | `			zBad = "final";` |
|        3 | 2395 | `			zWhere = "an abstract method";` |
|        1 | 2396 | `		}` |
|     2517 | 2397 | `	}else{` |
|     2757 | 2398 | `		if( pMods->bFinal && pMods->bAbstract ){` |
|        3 | 2399 | `			zBad = "final";` |
|        3 | 2400 | `			zWhere = "an abstract property";` |
|     2756 | 2401 | `		}else if( pMods->bFinal && (pClass->iFlags & PH7_CLASS_INTERFACE) ){` |
|        - | 2402 | `			/* php words the interface case as the PROPERTY's problem rather than` |
|        - | 2403 | `			 * the modifier's: an interface property is a hooked REQUIREMENT, and a` |
|        - | 2404 | `			 * requirement no implementor may restate cannot be one. */` |
|        3 | 2405 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pMods->nLine,` |
|        - | 2406 | `				"Property in interface cannot be final");` |
|        3 | 2407 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|     2753 | 2408 | `		}else if( pMods->bFinal && pMods->iProtection == PH7_TKWRD_PRIVATE ){` |
|        - | 2409 | ``			/* A private property is invisible to a subclass, so `final` on one says`` |
|        - | 2410 | `			 * nothing php can honour. This is the PROPERTY rule only: php accepts` |
|        - | 2411 | `			 * the same pair on a PROMOTED constructor parameter (modifiers 36` |
|        - | 2412 | `			 * there) -- an asymmetry of php's own, reproduced rather than smoothed` |
|        - | 2413 | `			 * over. */` |
|        3 | 2414 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pMods->nLine,` |
|        - | 2415 | `				"Property cannot be both final and private");` |
|        3 | 2416 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 2417 | `		}` |
|        - | 2418 | `	}` |
|     8453 | 2419 | `	if( zBad == 0 ){` |
|     8441 | 2420 | `		return SXRET_OK;` |
|        - | 2421 | `	}` |
|       16 | 2422 | `	pGen->iFatalTrace = PH7_FATAL_TRACE_NONE;` |
|       22 | 2423 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pMods->nLine,` |
|        6 | 2424 | `		"Cannot use the %s modifier on %s",zBad,zWhere);` |
|       16 | 2425 | `	return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|     4231 | 2426 | `}` |
|        - | 2427 | `/*` |
|        - | 2428 | `` * Peek the NAME a `const`/`function` keyword introduces, for the diagnostics php`` |
|        - | 2429 | ` * words with it. Returns 0 when the declaration is malformed enough that there` |
|        - | 2430 | ` * is no name to show (the caller then falls back to a nameless sentence).` |
|        - | 2431 | ` */` |
|        8 | 2432 | `static SyString * GenStateMemberNamePeek(ph7_gen_state *pGen)` |
|        3 | 2433 | `{` |
|       11 | 2434 | `	SyToken *p = pGen->pIn + 1;` |
|       11 | 2435 | `	if( p < pGen->pEnd && (p->nType & PH7_TK_AMPER) ){` |
|      ! 0 | 2436 | ``		p++; /* a by-reference method: `function &f()` */`` |
|      ! 0 | 2437 | `	}` |
|       11 | 2438 | `	if( p < pGen->pEnd && (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       11 | 2439 | `		return &p->sData;` |
|        - | 2440 | `	}` |
|      ! 0 | 2441 | `	return 0;` |
|        7 | 2442 | `}` |
|        - | 2443 | `/*` |
|        - | 2444 | ` * One MEMBER of a class/trait/interface body: the modifier run, php's screens on` |
|        - | 2445 | ` * it, and the declaration it turned out to modify. Shared by the three body` |
|        - | 2446 | ` * loops, which used to carry three near-identical modifier ladders -- and three` |
|        - | 2447 | ` * different sets of gaps.` |
|        - | 2448 | ` *` |
|        - | 2449 | `` * The enum `case` and the trait `use` statement are NOT members and never reach`` |
|        - | 2450 | ` * here: they take no modifiers, and their loops consume them first.` |
|        - | 2451 | ` *` |
|        - | 2452 | ` * Returns SXRET_OK, SXERR_SYNTAX when a refusal was reported (the caller` |
|        - | 2453 | ` * abandons the body), or SXERR_ABORT when the error budget is spent.` |
|        - | 2454 | ` */` |
|     8470 | 2455 | `static sxi32 GenStateCompileMember(ph7_gen_state *pGen,ph7_class *pClass,const char *zBody)` |
|        5 | 2456 | `{` |
|     8475 | 2457 | `	SyString *pName = &pClass->sName;` |
|        - | 2458 | `	GenMemberMods sMods;` |
|        - | 2459 | `	int iKind;` |
|        - | 2460 | `	sxi32 rc;` |
|     8475 | 2461 | `	rc = GenStateReadMemberMods(&(*pGen),&sMods);` |
|     8475 | 2462 | `	if( rc != SXRET_OK ){` |
|       14 | 2463 | `		return rc;` |
|        - | 2464 | `	}` |
|     8465 | 2465 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 2466 | `		rc = PH7_GenCompileError(pGen,E_PARSE,sMods.nLine,` |
|      ! 0 | 2467 | `			"Expecting member declaration inside %s '%z'",zBody,pName);` |
|      ! 0 | 2468 | `		return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 2469 | `	}` |
|        - | 2470 | `	/* What the run modifies. A '$' -- or a TYPE followed by one -- is a property;` |
|        - | 2471 | `	 * the three keywords are each their own declaration. */` |
|     8460 | 2472 | `	if( (pGen->pIn->nType & PH7_TK_DOLLAR)` |
|     7516 | 2473 | `	 \|\| GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|     2757 | 2474 | `		iKind = GEN_MEMBER_PROP;` |
|     7089 | 2475 | `	}else if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 2476 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|        - | 2477 | `			"Unexpected token '%z'. Expecting member declaration inside %s '%z'",` |
|      ! 0 | 2478 | `			&pGen->pIn->sData,zBody,pName);` |
|      ! 0 | 2479 | `		return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|      ! 0 | 2480 | `	}else{` |
|     5713 | 2481 | `		sxi32 nKw = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     5713 | 2482 | `		if( nKw == PH7_TKWRD_CONST ){` |
|      681 | 2483 | `			iKind = GEN_MEMBER_CONST;` |
|     5375 | 2484 | `		}else if( nKw == PH7_TKWRD_FUNCTION ){` |
|     5029 | 2485 | `			iKind = GEN_MEMBER_METHOD;` |
|     2522 | 2486 | `		}else if( nKw == PH7_TKWRD_VAR ){` |
|       10 | 2487 | `			iKind = GEN_MEMBER_VAR;` |
|        6 | 2488 | `		}else{` |
|      ! 0 | 2489 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|        - | 2490 | `				"Unexpected token '%z'. Expecting member declaration inside %s '%z'",` |
|      ! 0 | 2491 | `				&pGen->pIn->sData,zBody,pName);` |
|      ! 0 | 2492 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 2493 | `		}` |
|        - | 2494 | `	}` |
|     8465 | 2495 | `	if( iKind == GEN_MEMBER_VAR ){` |
|        - | 2496 | ``		/* `var $x` is the pre-5.0 spelling of `public $x` and takes NO other`` |
|        - | 2497 | `		 * modifier: php's parser is looking for a VARIABLE where the modifier run` |
|        - | 2498 | ``		 * left off, so `public var $x` and `final var $x` are parse errors naming`` |
|        - | 2499 | `		 * the token that is not one. */` |
|       10 | 2500 | `		if( sMods.bAny ){` |
|        3 | 2501 | `			rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,"variable");` |
|        3 | 2502 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 2503 | `		}` |
|        7 | 2504 | `		pGen->pIn++; /* Jump the 'var' keyword */` |
|        7 | 2505 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 | 2506 | `			rc = PH7_GenSyntaxError(&(*pGen),` |
|      ! 0 | 2507 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn : 0,"variable");` |
|      ! 0 | 2508 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 2509 | `		}` |
|        7 | 2510 | `		iKind = GEN_MEMBER_PROP;` |
|        4 | 2511 | `	}else{` |
|     8457 | 2512 | `		rc = GenStateScreenMemberMods(&(*pGen),&sMods,iKind,pClass);` |
|     8457 | 2513 | `		if( rc != SXRET_OK ){` |
|       20 | 2514 | `			return rc;` |
|        - | 2515 | `		}` |
|        - | 2516 | `	}` |
|     8447 | 2517 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|        - | 2518 | `		/* An interface member is public by declaration, and its methods are` |
|        - | 2519 | ``		 * implicitly abstract -- so php refuses both `abstract` written out and`` |
|        - | 2520 | ``		 * `final`, naming the method in each. */`` |
|      209 | 2521 | `		if( sMods.iProtection != PH7_TKWRD_PUBLIC ){` |
|        8 | 2522 | `			SyString *pMember = (iKind == GEN_MEMBER_PROP) ? 0 : GenStateMemberNamePeek(&(*pGen));` |
|        8 | 2523 | `			if( iKind == GEN_MEMBER_PROP ){` |
|        - | 2524 | `				/* php words the PROPERTY case as the property's problem, and names` |
|        - | 2525 | `				 * neither of the two visibilities it refuses. */` |
|        3 | 2526 | `				rc = PH7_GenCompileError(pGen,E_ERROR,sMods.nLine,` |
|        - | 2527 | `					"Property in interface cannot be protected or private");` |
|        6 | 2528 | `			}else if( pMember ){` |
|        7 | 2529 | `				rc = PH7_GenCompileError(pGen,E_ERROR,sMods.nLine,` |
|        - | 2530 | `					"Access type for interface %s %z::%z%s must be public",` |
|        2 | 2531 | `					(iKind == GEN_MEMBER_CONST) ? "constant" : "method",pName,pMember,` |
|        2 | 2532 | `					(iKind == GEN_MEMBER_CONST) ? "" : "()");` |
|        3 | 2533 | `			}else{` |
|      ! 0 | 2534 | `				rc = PH7_GenCompileError(pGen,E_ERROR,sMods.nLine,` |
|        - | 2535 | `					"Access type for interface %s must be public",` |
|      ! 0 | 2536 | `					(iKind == GEN_MEMBER_CONST) ? "constant" : "method");` |
|        - | 2537 | `			}` |
|        8 | 2538 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 2539 | `		}` |
|      203 | 2540 | `		if( iKind == GEN_MEMBER_METHOD && (sMods.bFinal \|\| sMods.bAbstract) ){` |
|        6 | 2541 | `			SyString *pMember = GenStateMemberNamePeek(&(*pGen));` |
|        6 | 2542 | `			const char *zWhat = sMods.bFinal ? "final" : "abstract";` |
|        6 | 2543 | `			if( pMember ){` |
|        8 | 2544 | `				rc = PH7_GenCompileError(pGen,E_ERROR,sMods.nLine,` |
|        2 | 2545 | `					"Interface method %z::%z() must not be %s",pName,pMember,zWhat);` |
|        4 | 2546 | `			}else{` |
|      ! 0 | 2547 | `				rc = PH7_GenCompileError(pGen,E_ERROR,sMods.nLine,` |
|      ! 0 | 2548 | `					"Interface method must not be %s",zWhat);` |
|        - | 2549 | `			}` |
|        6 | 2550 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 2551 | `		}` |
|        - | 2552 | `		/* Both remaining kinds are abstract in an interface: a method has no body` |
|        - | 2553 | `		 * to compile, and a property is a HOOKED requirement (a plain one is` |
|        - | 2554 | `		 * GenStateCompileClassAttr's own "Interfaces may only include hooked` |
|        - | 2555 | `		 * properties"). */` |
|      199 | 2556 | `		if( iKind != GEN_MEMBER_CONST ){` |
|      157 | 2557 | `			sMods.iFlags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       76 | 2558 | `		}` |
|       97 | 2559 | `	}` |
|     8437 | 2560 | `	if( iKind == GEN_MEMBER_CONST ){` |
|      673 | 2561 | `		return GenStateCompileClassConstant(&(*pGen),sMods.iProtection,sMods.iFlags,pClass);` |
|        - | 2562 | `	}` |
|     7769 | 2563 | `	if( iKind == GEN_MEMBER_METHOD ){` |
|        - | 2564 | `		/* An interface method is a SIGNATURE: it has no body to compile. */` |
|     7526 | 2565 | `		return GenStateCompileClassMethod(&(*pGen),sMods.iProtection,sMods.iFlags,` |
|     5014 | 2566 | `			(pClass->iFlags & PH7_CLASS_INTERFACE) ? FALSE : TRUE,pClass);` |
|        - | 2567 | `	}` |
|     2755 | 2568 | `	return GenStateCompileClassAttr(&(*pGen),sMods.iProtection,sMods.iFlags,pClass);` |
|     4240 | 2569 | `}` |
|        - | 2570 | `/*` |
|        - | 2571 | `` * Compile a PHP 8.4 property-hook list `{ get ...; set ...; }` following a`` |
|        - | 2572 | ` * property declaration. Each hook body is synthesized into a hidden public` |
|        - | 2573 | ` * class method (__phl_hook_get_NAME / __phl_hook_set_NAME) so inheritance,` |
|        - | 2574 | ` * $this binding, and dispatch ride the ordinary method machinery; OP_MEMBER /` |
|        - | 2575 | ` * OP_STORE route reads and plain writes through them (a per-instance guard` |
|        - | 2576 | ` * makes $this->NAME inside a hook body address the raw backing slot — php's` |
|        - | 2577 | `` * rule that hooks see the backing store). `get => expr;` compiles as an`` |
|        - | 2578 | `` * implicit return (the arrow-fn pattern); `set => expr;` compiles the same`` |
|        - | 2579 | ` * and is flagged VM_FUNC_HOOK_SET_EXPR — the dispatcher assigns its return` |
|        - | 2580 | `` * value to the backing slot. A `set` without a parameter list receives the`` |
|        - | 2581 | `` * implicit `$value` formal.`` |
|        - | 2582 | ` * On entry pGen->pIn sits on '{'; on success it sits just past '}'.` |
|        - | 2583 | ` */` |
|        - | 2584 | `/*` |
|        - | 2585 | `` * Whether any token in [pStart, pEnd) spells `$this->NAME` (this property's own`` |
|        - | 2586 | `` * name; `?->` and `::` member ops count too). php 8.4's virtual-vs-backed rule:`` |
|        - | 2587 | ` * a hooked property is BACKED iff any of its OWN hook bodies references it by` |
|        - | 2588 | ` * name through $this — otherwise it is VIRTUAL: no backing store, no default` |
|        - | 2589 | ` * allowed, excluded from the raw object surfaces.` |
|        - | 2590 | ` */` |
|      186 | 2591 | `static int GenStateHookBodyRefsProp(SyToken *pStart,SyToken *pEnd,const SyString *pName)` |
|        5 | 2592 | `{` |
|        - | 2593 | `	SyToken *p;` |
|      835 | 2594 | `	for( p = pStart ; p + 1 < pEnd ; p++ ){` |
|      715 | 2595 | `		if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|      583 | 2596 | `			continue;` |
|        - | 2597 | `		}` |
|        - | 2598 | ``		/* `$this->NAME` (also `?->`/`::`) */`` |
|      132 | 2599 | `		if( p + 3 < pEnd` |
|      132 | 2600 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|      132 | 2601 | `		 && p[1].sData.nByte == sizeof("this")-1` |
|      115 | 2602 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)"this",sizeof("this")-1) == 0` |
|       98 | 2603 | `		 && GenStateTokenIsMemberOp(&p[2])` |
|       98 | 2604 | `		 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       98 | 2605 | `		 && p[3].sData.nByte == pName->nByte` |
|       91 | 2606 | `		 && SyMemcmp((const void *)p[3].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|       66 | 2607 | `			return 1;` |
|        - | 2608 | `		}` |
|        - | 2609 | ``		/* `parent::$NAME` (the parent::$x::get() hook-call form): the parent`` |
|        - | 2610 | `		 * hook operates on the shared per-instance backing store, so the` |
|        - | 2611 | `		 * property is backed (php compiles a default alongside it). */` |
|       70 | 2612 | `		if( p > pStart` |
|       66 | 2613 | `		 && GenStateTokenIsMemberOp(&p[-1])` |
|       33 | 2614 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        4 | 2615 | `		 && p[1].sData.nByte == pName->nByte` |
|        7 | 2616 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        6 | 2617 | `			return 1;` |
|        - | 2618 | `		}` |
|       35 | 2619 | `	}` |
|      125 | 2620 | `	return 0;` |
|       98 | 2621 | `}` |
|        - | 2622 | `/*` |
|        - | 2623 | ` * True when p opens php 8.4's parent-hook call form` |
|        - | 2624 | `` * `parent :: $ NAME :: get\|set (` (7 tokens through the '(').`` |
|        - | 2625 | ` */` |
|     1452 | 2626 | `static int GenStateIsParentHookCallAt(SyToken *p,SyToken *pEnd)` |
|        5 | 2627 | `{` |
|     1678 | 2628 | `	return p + 6 < pEnd` |
|      947 | 2629 | `	 && (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|      317 | 2630 | `	 && p->sData.nByte == sizeof("parent")-1` |
|      110 | 2631 | `	 && SyMemcmp((const void *)p->sData.zString,(const void *)"parent",sizeof("parent")-1) == 0` |
|       20 | 2632 | `	 && GenStateTokenIsMemberOp(&p[1])` |
|       12 | 2633 | `	 && (p[2].nType & PH7_TK_DOLLAR) != 0` |
|       12 | 2634 | `	 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       12 | 2635 | `	 && GenStateTokenIsMemberOp(&p[4])` |
|       12 | 2636 | `	 && (p[5].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       12 | 2637 | `	 && p[5].sData.nByte == 3` |
|       12 | 2638 | `	 && (SyMemcmp((const void *)p[5].sData.zString,(const void *)"get",3) == 0` |
|        8 | 2639 | `	  \|\| SyMemcmp((const void *)p[5].sData.zString,(const void *)"set",3) == 0)` |
|     1673 | 2640 | `	 && (p[6].nType & PH7_TK_LPAREN) != 0;` |
|        5 | 2641 | `}` |
|        - | 2642 | `/*` |
|        - | 2643 | `` * Rewrite php 8.4 `parent::$x::get(...)` / `parent::$x::set(...)` calls in a`` |
|        - | 2644 | ` * hook body into calls of the parent class's synthesized hook method` |
|        - | 2645 | `` * (`parent::__phl_hook_get_x(...)`). Builds a token COPY into pCopy (only`` |
|        - | 2646 | ` * called when GenStateIsParentHookCallAt matched somewhere in the range);` |
|        - | 2647 | ` * copied tokens keep pointing at source-owned lexeme storage, and the` |
|        - | 2648 | ` * synthesized method-name lexemes are VM-allocator owned. Returns SXRET_OK` |
|        - | 2649 | ` * or SXERR_MEM.` |
|        - | 2650 | ` */` |
|        6 | 2651 | `static sxi32 GenStateRewriteParentHookCalls(ph7_gen_state *pGen,SySet *pCopy,` |
|        - | 2652 | `	SyToken *pStart,SyToken *pEnd)` |
|        2 | 2653 | `{` |
|        8 | 2654 | `	SyToken *p = pStart;` |
|       56 | 2655 | `	while( p < pEnd ){` |
|       50 | 2656 | `		if( GenStateIsParentHookCallAt(p,pEnd) ){` |
|        - | 2657 | `			SyToken sTok;` |
|        - | 2658 | `			char zName[384];` |
|        - | 2659 | `			sxu32 nName;` |
|        - | 2660 | `			char *zDup;` |
|        - | 2661 | ``			/* `parent` `::` */`` |
|        8 | 2662 | `			SySetPut(pCopy,(const void *)&p[0]);` |
|        8 | 2663 | `			SySetPut(pCopy,(const void *)&p[1]);` |
|       11 | 2664 | `			nName = SyBufferFormat(zName,sizeof(zName),"__phl_hook_%.3s_%.*s",` |
|        6 | 2665 | `				p[5].sData.zString,(int)p[3].sData.nByte,p[3].sData.zString);` |
|        8 | 2666 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|        8 | 2667 | `			if( zDup == 0 ){` |
|      ! 0 | 2668 | `				return SXERR_MEM;` |
|        - | 2669 | `			}` |
|        8 | 2670 | `			sTok = p[3]; /* keep the line info of the property name */` |
|        8 | 2671 | `			sTok.nType = PH7_TK_ID;` |
|        8 | 2672 | `			SyStringInitFromBuf(&sTok.sData,zDup,nName);` |
|        8 | 2673 | `			sTok.pUserData = 0;` |
|        8 | 2674 | `			SySetPut(pCopy,(const void *)&sTok);` |
|        8 | 2675 | `			p += 6; /* continue at the '(' — arguments copy through unchanged */` |
|        8 | 2676 | `			continue;` |
|        - | 2677 | `		}` |
|       44 | 2678 | `		SySetPut(pCopy,(const void *)p);` |
|       44 | 2679 | `		p++;` |
|        2 | 2680 | `	}` |
|        8 | 2681 | `	return SXRET_OK;` |
|        5 | 2682 | `}` |
|        - | 2683 | `/*` |
|        - | 2684 | `` * A `get` hook's return type IS the property's declared type — php never lets a`` |
|        - | 2685 | ` * hook declare one, so there is nothing else it could be, and that is what makes` |
|        - | 2686 | `` * `public int $p { get { return "5"; } }` answer int(5) and a `get` returning`` |
|        - | 2687 | `` * "x" raise `C::$p::get(): Return value must be of type int, string returned`.`` |
|        - | 2688 | ` * Installing it on the synthesized method reuses the return enforcement that` |
|        - | 2689 | ` * already matches php byte for byte (the same move the __toString implicit` |
|        - | 2690 | `` * `string` type made), and lets the compile-time bare-`return;` check see the`` |
|        - | 2691 | ` * hook as the typed function php treats it as.` |
|        - | 2692 | ` *` |
|        - | 2693 | ` * The union alternatives are SHARED, not copied: their class-name SyStrings are` |
|        - | 2694 | ` * VM-allocator owned and outlive both records, which is the same contract` |
|        - | 2695 | ` * GenStateCopyTypeToAttr relies on.` |
|        - | 2696 | ` */` |
|      148 | 2697 | `static void GenStateHookGetReturnType(ph7_vm_func *pFunc,ph7_class_attr *pAttr)` |
|        5 | 2698 | `{` |
|      153 | 2699 | `	if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       20 | 2700 | `		return; /* untyped property: the hook is untyped too */` |
|        - | 2701 | `	}` |
|      137 | 2702 | `	pFunc->nReturnType = pAttr->nType;` |
|      137 | 2703 | `	pFunc->sReturnClass = pAttr->sClass;` |
|      137 | 2704 | `	pFunc->sReturnTypeName = pAttr->sTypeName;` |
|      137 | 2705 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|       14 | 2706 | `		pFunc->iFlags \|= VM_FUNC_RETURN_NULLABLE;` |
|        6 | 2707 | `	}` |
|      137 | 2708 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|        - | 2709 | `		sxu32 i;` |
|      ! 0 | 2710 | `		for( i = 0 ; i < SySetUsed(&pAttr->aUnionAlts) ; i++ ){` |
|      ! 0 | 2711 | `			SySetPut(&pFunc->aReturnUnion,SySetAt(&pAttr->aUnionAlts,i));` |
|      ! 0 | 2712 | `		}` |
|      ! 0 | 2713 | `	}` |
|       79 | 2714 | `}` |
|        - | 2715 | `/*` |
|        - | 2716 | `` * The mirror for a `set` hook. php gives it two implicit pieces of signature:`` |
|        - | 2717 | `` * the implicit `$value` formal carries the PROPERTY's declared type (so`` |
|        - | 2718 | `` * `public int $p { set { ... } }` coerces `$o->p = "7"` to int(7) and rejects`` |
|        - | 2719 | `` * "abc" with `C::$p::set(): Argument #1 ($value) must be of type int, string`` |
|        - | 2720 | `` * given`), and the hook itself returns `void` — a set hook that returns a value`` |
|        - | 2721 | `` * is php's `A void method must not return a value`, on an untyped property too.`` |
|        - | 2722 | `` * An EXPLICIT `set(T $v)` keeps its own declared type; only the implicit formal`` |
|        - | 2723 | ` * is typed from the property, which is why the caller passes pValueArg only` |
|        - | 2724 | ` * when it synthesized one.` |
|        - | 2725 | ` */` |
|       96 | 2726 | `static void GenStateHookSetSignature(ph7_gen_state *pGen,ph7_vm_func *pFunc,` |
|        - | 2727 | `	ph7_class_attr *pAttr,ph7_vm_func_arg *pValueArg)` |
|        4 | 2728 | `{` |
|        - | 2729 | `	char *zVoid;` |
|      100 | 2730 | `	if( pValueArg && (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|       68 | 2731 | `		pValueArg->nType = pAttr->nType;` |
|       68 | 2732 | `		pValueArg->sClass = pAttr->sClass;` |
|       68 | 2733 | `		pValueArg->sTypeName = pAttr->sTypeName;` |
|       68 | 2734 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|       14 | 2735 | `			pValueArg->iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|        6 | 2736 | `		}` |
|       68 | 2737 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|        - | 2738 | `			sxu32 i;` |
|        3 | 2739 | `			pValueArg->iFlags \|= VM_FUNC_ARG_UNION;` |
|        7 | 2740 | `			for( i = 0 ; i < SySetUsed(&pAttr->aUnionAlts) ; i++ ){` |
|        5 | 2741 | `				SySetPut(&pValueArg->aUnionAlts,SySetAt(&pAttr->aUnionAlts,i));` |
|        3 | 2742 | `			}` |
|        1 | 2743 | `		}` |
|       33 | 2744 | `	}` |
|      100 | 2745 | `	pFunc->nReturnType = MEMOBJ_VOID;` |
|      100 | 2746 | `	zVoid = SyMemBackendStrDup(&pGen->pVm->sAllocator,"void",sizeof("void")-1);` |
|      100 | 2747 | `	if( zVoid ){` |
|      100 | 2748 | `		SyStringInitFromBuf(&pFunc->sReturnTypeName,zVoid,sizeof("void")-1);` |
|       48 | 2749 | `	}` |
|      100 | 2750 | `}` |
|      180 | 2751 | `PH7_PRIVATE sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        5 | 2752 | `{` |
|      185 | 2753 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 2754 | `	sxi32 rc;` |
|      185 | 2755 | `	int bRefsSelf = 0;` |
|      185 | 2756 | `	pGen->pIn++; /* Jump '{' */` |
|      447 | 2757 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|        - | 2758 | `		char zHook[384];` |
|        - | 2759 | `		SyString sHookName;` |
|        - | 2760 | `		ph7_class_method *pMeth;` |
|        - | 2761 | `		int bGet;` |
|      267 | 2762 | `		sxu32 nHLine = pGen->pIn->nLine;` |
|      267 | 2763 | `		if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       21 | 2764 | `			pGen->pIn++; /* stray ';' between hooks */` |
|       30 | 2765 | `			continue;` |
|        - | 2766 | `		}` |
|      249 | 2767 | `		if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|        - | 2768 | `			/* by-reference get hook: not modeled (loud, recorded) */` |
|      ! 0 | 2769 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|        - | 2770 | `				"By-reference property hooks are not supported for %z::$%z",` |
|      ! 0 | 2771 | `				&pClass->sDisp,&pAttr->sName);` |
|      ! 0 | 2772 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2773 | `				return SXERR_ABORT;` |
|        - | 2774 | `			}` |
|      ! 0 | 2775 | `			return SXERR_CORRUPT;` |
|        - | 2776 | `		}` |
|      249 | 2777 | `		if( (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 2778 | `			goto HookSyntax;` |
|        - | 2779 | `		}` |
|      244 | 2780 | `		if( pGen->pIn->sData.nByte == 3` |
|      249 | 2781 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"get",3) == 0 ){` |
|      153 | 2782 | `			bGet = 1;` |
|      175 | 2783 | `		}else if( pGen->pIn->sData.nByte == 3` |
|      100 | 2784 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"set",3) == 0 ){` |
|      100 | 2785 | `			bGet = 0;` |
|       52 | 2786 | `		}else{` |
|      ! 0 | 2787 | `			goto HookSyntax;` |
|        - | 2788 | `		}` |
|      249 | 2789 | `		pGen->pIn++; /* Jump 'get'/'set' */` |
|      249 | 2790 | `		sHookName.zString = zHook;` |
|      371 | 2791 | `		sHookName.nByte = SyBufferFormat(zHook,sizeof(zHook),"__phl_hook_%s_%z",` |
|      122 | 2792 | `			bGet ? "get" : "set",&pAttr->sName);` |
|      249 | 2793 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_CCB)) ){` |
|        - | 2794 | ``			/* Bare `get;` / `set;` — an ABSTRACT hook declaration (php 8.4):`` |
|        - | 2795 | ``			 * legal only on an `abstract` property or inside an interface. The`` |
|        - | 2796 | `			 * synthesized method carries PH7_CLASS_ATTR_ABSTRACT and rides the` |
|        - | 2797 | `			 * existing must-implement machinery; a concrete hook override (or a` |
|        - | 2798 | `			 * plain property, see GenStateCheckAbstractMethods) satisfies it. */` |
|       18 | 2799 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0` |
|       12 | 2800 | `			 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 2801 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|        - | 2802 | `					"Non-abstract property hook must have a body");` |
|      ! 0 | 2803 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2804 | `					return SXERR_ABORT;` |
|        - | 2805 | `				}` |
|      ! 0 | 2806 | `				return SXERR_CORRUPT;` |
|        - | 2807 | `			}` |
|       21 | 2808 | `			pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|        - | 2809 | `				PH7_CLASS_PROT_PUBLIC,PH7_CLASS_ATTR_ABSTRACT,0);` |
|       21 | 2810 | `			if( pMeth == 0 ){` |
|      ! 0 | 2811 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2812 | `				return SXERR_ABORT;` |
|        - | 2813 | `			}` |
|       21 | 2814 | `			pMeth->sFunc.nLine = nHLine;` |
|       21 | 2815 | `			if( bGet ){` |
|       15 | 2816 | `				GenStateHookGetReturnType(&pMeth->sFunc,pAttr);` |
|        6 | 2817 | `			}` |
|       21 | 2818 | `			if( !bGet ){` |
|        - | 2819 | ``				/* The implicit `$value` formal keeps the stub's signature`` |
|        - | 2820 | `				 * compatible with concrete set-hook implementations (which` |
|        - | 2821 | `				 * always carry one parameter). It takes the PROPERTY's declared` |
|        - | 2822 | `				 * type (php: the abstract set's parameter type IS the property` |
|        - | 2823 | `				 * type), so the override contravariance check accepts a typed` |
|        - | 2824 | ``				 * `set(int $v)` implementation on an `int $x` requirement. */`` |
|        - | 2825 | `				ph7_vm_func_arg sVArg;` |
|        7 | 2826 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|        7 | 2827 | `				if( zVName == 0 ){` |
|      ! 0 | 2828 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2829 | `					return SXERR_ABORT;` |
|        - | 2830 | `				}` |
|        7 | 2831 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|        7 | 2832 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|        7 | 2833 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        7 | 2834 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        7 | 2835 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|        7 | 2836 | `				GenStateHookSetSignature(&(*pGen),&pMeth->sFunc,pAttr,&sVArg);` |
|        7 | 2837 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|        3 | 2838 | `			}` |
|       21 | 2839 | `			rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|       21 | 2840 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 2841 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2842 | `				return SXERR_ABORT;` |
|        - | 2843 | `			}` |
|       21 | 2844 | `			pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|       21 | 2845 | `			continue; /* the loop consumes the ';' as a stray separator */` |
|        - | 2846 | `		}` |
|      226 | 2847 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0` |
|      231 | 2848 | `		 \|\| (pClass->iFlags & PH7_CLASS_INTERFACE) != 0 ){` |
|        - | 2849 | `			/* php: an abstract/interface property hook cannot carry a body */` |
|      ! 0 | 2850 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|        - | 2851 | `				"Abstract property hook cannot have body");` |
|      ! 0 | 2852 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2853 | `				return SXERR_ABORT;` |
|        - | 2854 | `			}` |
|      ! 0 | 2855 | `			return SXERR_CORRUPT;` |
|        - | 2856 | `		}` |
|      231 | 2857 | `		pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|        - | 2858 | `			PH7_CLASS_PROT_PUBLIC,0,0);` |
|      231 | 2859 | `		if( pMeth == 0 ){` |
|      ! 0 | 2860 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2861 | `			return SXERR_ABORT;` |
|        - | 2862 | `		}` |
|      231 | 2863 | `		pMeth->sFunc.nLine = nHLine;` |
|      231 | 2864 | `		if( bGet ){` |
|      141 | 2865 | `			GenStateHookGetReturnType(&pMeth->sFunc,pAttr);` |
|       68 | 2866 | `		}` |
|      231 | 2867 | `		if( !bGet ){` |
|        - | 2868 | ``			/* Parameter list: explicit `set(Type $v)` or the implicit `$value` */`` |
|       94 | 2869 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       26 | 2870 | `				SyToken *pRp = 0;` |
|       26 | 2871 | `				pGen->pIn++;` |
|       26 | 2872 | `				PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pRp);` |
|       26 | 2873 | `				if( pRp >= pGen->pEnd ){` |
|      ! 0 | 2874 | `					goto HookSyntax;` |
|        - | 2875 | `				}` |
|       26 | 2876 | `				if( pGen->pIn < pRp ){` |
|       26 | 2877 | `					rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pRp,0,0);` |
|       26 | 2878 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 2879 | `						return SXERR_ABORT;` |
|        - | 2880 | `					}` |
|       12 | 2881 | `				}` |
|       26 | 2882 | `				pGen->pIn = &pRp[1];` |
|       12 | 2883 | `			}` |
|       94 | 2884 | `			if( SySetUsed(&pMeth->sFunc.aArgs) < 1 ){` |
|        - | 2885 | `				/* Implicit $value formal */` |
|        - | 2886 | `				ph7_vm_func_arg sVArg;` |
|       70 | 2887 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|       70 | 2888 | `				if( zVName == 0 ){` |
|      ! 0 | 2889 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2890 | `					return SXERR_ABORT;` |
|        - | 2891 | `				}` |
|       70 | 2892 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|       70 | 2893 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|       70 | 2894 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       70 | 2895 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       70 | 2896 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|       70 | 2897 | `				SyStringInitFromBuf(&sVArg.sTypeName,0,0);` |
|       70 | 2898 | `				GenStateHookSetSignature(&(*pGen),&pMeth->sFunc,pAttr,&sVArg);` |
|       70 | 2899 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|       37 | 2900 | `			}else{` |
|        - | 2901 | ``				/* An EXPLICIT `set(T $v)` keeps its own parameter type; only the`` |
|        - | 2902 | `				 * void return is implicit. */` |
|       26 | 2903 | `				GenStateHookSetSignature(&(*pGen),&pMeth->sFunc,pAttr,0);` |
|        - | 2904 | `			}` |
|       45 | 2905 | `		}` |
|      295 | 2906 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|        - | 2907 | `			/* Block body */` |
|      133 | 2908 | `			SyToken *pBodyStart = pGen->pIn;` |
|      133 | 2909 | `			SyToken *pCloser = 0;` |
|      133 | 2910 | `			int bParentCall = 0;` |
|      133 | 2911 | `			PH7_DelimitNestedTokens(&pBodyStart[1],pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pCloser);` |
|      133 | 2912 | `			if( pCloser < pGen->pEnd ){` |
|        - | 2913 | `				SyToken *pScan;` |
|     1143 | 2914 | `				for( pScan = &pBodyStart[1] ; pScan < pCloser ; pScan++ ){` |
|     1019 | 2915 | `					if( GenStateIsParentHookCallAt(pScan,pCloser) ){` |
|        6 | 2916 | `						bParentCall = 1;` |
|        6 | 2917 | `						break;` |
|        - | 2918 | `					}` |
|      510 | 2919 | `				}` |
|       64 | 2920 | `			}` |
|      133 | 2921 | `			if( bParentCall ){` |
|        - | 2922 | ``				/* `parent::$x::get()` inside the body: compile a REWRITTEN copy`` |
|        - | 2923 | `				 * of the body tokens (the call becomes the parent's synthesized` |
|        - | 2924 | `				 * hook method), then continue past the original body. */` |
|        - | 2925 | `				SySet sBody;` |
|        6 | 2926 | `				SyToken *pSavedEnd = pGen->pEnd;` |
|        6 | 2927 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        6 | 2928 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,&pCloser[1]);` |
|        6 | 2929 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 2930 | `					SySetRelease(&sBody);` |
|      ! 0 | 2931 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2932 | `					return SXERR_ABORT;` |
|        - | 2933 | `				}` |
|        6 | 2934 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|        6 | 2935 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|        6 | 2936 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|        6 | 2937 | `				pGen->pIn = &pCloser[1];` |
|        6 | 2938 | `				pGen->pEnd = pSavedEnd;` |
|        6 | 2939 | `				SySetRelease(&sBody);` |
|        6 | 2940 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2941 | `					return SXERR_ABORT;` |
|        - | 2942 | `				}` |
|        6 | 2943 | `				pMeth->sFunc.nEndLine = pCloser->nLine;` |
|        4 | 2944 | `			}else{` |
|      129 | 2945 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|      129 | 2946 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2947 | `					return SXERR_ABORT;` |
|        - | 2948 | `				}` |
|      129 | 2949 | `				pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|        - | 2950 | `			}` |
|      133 | 2951 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|       25 | 2952 | `				bRefsSelf = 1;` |
|       16 | 2953 | `			}` |
|      216 | 2954 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ARRAY_OP) ){` |
|        - | 2955 | ``			/* `=> expr;` — implicit-return body (the arrow-fn pattern) */`` |
|        - | 2956 | `			GenBlock *pBlock;` |
|        - | 2957 | `			SySet *pInstrContainer;` |
|        - | 2958 | `			SyToken *pBodyStart;` |
|        - | 2959 | `			SyToken *pExprEnd;` |
|      103 | 2960 | `			SyToken *pSavedEnd = 0;` |
|        - | 2961 | `			SySet sBody;` |
|      103 | 2962 | `			int bParentCall = 0;` |
|      103 | 2963 | `			pGen->pIn++; /* Jump '=>' */` |
|      103 | 2964 | `			pBodyStart = pGen->pIn;` |
|        - | 2965 | `			/* Delimit the expression (first top-level ';', or a closer that` |
|        - | 2966 | `			 * would end the enclosing hook list) and rewrite any` |
|        - | 2967 | ``			 * `parent::$x::get()` calls into the parent's synthesized hook`` |
|        - | 2968 | `			 * method on a token copy. */` |
|        - | 2969 | `			{` |
|      103 | 2970 | `				sxi32 iNest = 0;` |
|      103 | 2971 | `				pExprEnd = pBodyStart;` |
|      511 | 2972 | `				while( pExprEnd < pGen->pEnd ){` |
|      511 | 2973 | `					if( pExprEnd->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       19 | 2974 | `						iNest++;` |
|      503 | 2975 | `					}else if( pExprEnd->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       19 | 2976 | `						if( iNest <= 0 ){` |
|      ! 0 | 2977 | `							break;` |
|        - | 2978 | `						}` |
|       19 | 2979 | `						iNest--;` |
|      487 | 2980 | `					}else if( iNest <= 0 && (pExprEnd->nType & PH7_TK_SEMI) ){` |
|      103 | 2981 | `						break;` |
|        - | 2982 | `					}` |
|      413 | 2983 | `					pExprEnd++;` |
|        5 | 2984 | `				}` |
|        - | 2985 | `			}` |
|        - | 2986 | `			{` |
|        - | 2987 | `				SyToken *pScan;` |
|      491 | 2988 | `				for( pScan = pBodyStart ; pScan < pExprEnd ; pScan++ ){` |
|      395 | 2989 | `					if( GenStateIsParentHookCallAt(pScan,pExprEnd) ){` |
|        3 | 2990 | `						bParentCall = 1;` |
|        3 | 2991 | `						break;` |
|        - | 2992 | `					}` |
|      199 | 2993 | `				}` |
|        - | 2994 | `			}` |
|      103 | 2995 | `			if( bParentCall ){` |
|        3 | 2996 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        3 | 2997 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,pExprEnd);` |
|        3 | 2998 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 2999 | `					SySetRelease(&sBody);` |
|      ! 0 | 3000 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 3001 | `					return SXERR_ABORT;` |
|        - | 3002 | `				}` |
|        3 | 3003 | `				pSavedEnd = pGen->pEnd;` |
|        3 | 3004 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|        3 | 3005 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|        1 | 3006 | `			}` |
|      152 | 3007 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|       98 | 3008 | `				PH7_VmInstrLength(pGen->pVm),&pMeth->sFunc,&pBlock);` |
|      103 | 3009 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 3010 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"PH7 engine is running out-of-memory");` |
|      ! 0 | 3011 | `				return SXERR_ABORT;` |
|        - | 3012 | `			}` |
|      103 | 3013 | `			pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      103 | 3014 | `			PH7_VmSetByteCodeContainer(pGen->pVm,&pMeth->sFunc.aByteCode);` |
|      103 | 3015 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      103 | 3016 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      103 | 3017 | `			GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|      103 | 3018 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|      103 | 3019 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      103 | 3020 | `			GenStateLeaveBlock(&(*pGen),0);` |
|      103 | 3021 | `			if( bParentCall ){` |
|        3 | 3022 | `				pGen->pIn = pExprEnd; /* land on the original ';' */` |
|        3 | 3023 | `				pGen->pEnd = pSavedEnd;` |
|        3 | 3024 | `				SySetRelease(&sBody);` |
|        1 | 3025 | `			}` |
|      103 | 3026 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3027 | `				return SXERR_ABORT;` |
|        - | 3028 | `			}` |
|      103 | 3029 | `			pMeth->sFunc.nEndLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nHLine;` |
|      103 | 3030 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|       47 | 3031 | `				bRefsSelf = 1;` |
|       22 | 3032 | `			}` |
|      103 | 3033 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      103 | 3034 | `				pGen->pIn++; /* Jump ';' */` |
|       49 | 3035 | `			}` |
|      103 | 3036 | `			if( !bGet ){` |
|        - | 3037 | ``				/* `set => expr` assigns the expression to the backing store:`` |
|        - | 3038 | `				 * the dispatcher consumes the implicit return value — which` |
|        - | 3039 | `				 * also makes the property BACKED (php: the shorthand is sugar` |
|        - | 3040 | ``				 * for `$this->NAME = expr`). */`` |
|        8 | 3041 | `				pMeth->sFunc.iFlags \|= VM_FUNC_HOOK_SET_EXPR;` |
|        8 | 3042 | `				bRefsSelf = 1;` |
|        3 | 3043 | `			}` |
|       54 | 3044 | `		}else{` |
|      ! 0 | 3045 | `			goto HookSyntax;` |
|        - | 3046 | `		}` |
|      231 | 3047 | `		rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|      231 | 3048 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 3049 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 3050 | `			return SXERR_ABORT;` |
|        - | 3051 | `		}` |
|      231 | 3052 | `		pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|        5 | 3053 | `	}` |
|      185 | 3054 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|      ! 0 | 3055 | `		goto HookSyntax;` |
|        - | 3056 | `	}` |
|      185 | 3057 | `	pGen->pIn++; /* Jump '}' */` |
|      185 | 3058 | `	if( !bRefsSelf ){` |
|        - | 3059 | ``		/* php 8.4 virtual-vs-backed: no hook body referenced `$this->NAME`, so`` |
|        - | 3060 | `		 * this property is VIRTUAL — php gives it no backing store and forbids` |
|        - | 3061 | `		 * a default value (compile fatal, php's exact wording). */` |
|      117 | 3062 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_HOOK_VIRTUAL;` |
|      117 | 3063 | `		if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|      ! 0 | 3064 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 3065 | `				"Cannot specify default value for virtual hooked property %z::$%z",` |
|      ! 0 | 3066 | `				&pClass->sDisp,&pAttr->sName);` |
|      ! 0 | 3067 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3068 | `				return SXERR_ABORT;` |
|        - | 3069 | `			}` |
|      ! 0 | 3070 | `			return SXERR_CORRUPT;` |
|        - | 3071 | `		}` |
|       56 | 3072 | `	}` |
|      185 | 3073 | `	return SXRET_OK;` |
|      ! 0 | 3074 | `HookSyntax:` |
|      ! 0 | 3075 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 3076 | `		"Invalid property hook declaration for %z::$%z: expecting 'get' or 'set'",` |
|      ! 0 | 3077 | `		&pClass->sDisp,&pAttr->sName);` |
|      ! 0 | 3078 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 3079 | `		return SXERR_ABORT;` |
|        - | 3080 | `	}` |
|      ! 0 | 3081 | `	return SXERR_CORRUPT;` |
|       95 | 3082 | `}` |
|        - | 3083 | `/* php's #[\Override] verification, defined with the rest of the class-link` |
|        - | 3084 | ` * checks below; both compilers (class and interface) drive the same pair. */` |
|        - | 3085 | `static sxi32 GenStateCollectOverrides(ph7_gen_state *pGen,ph7_class *pClass,` |
|        - | 3086 | `	SySet *pMeths,SySet *pProps);` |
|        - | 3087 | `static sxi32 GenStateCheckOverrides(ph7_gen_state *pGen,ph7_class *pClass,` |
|        - | 3088 | `	SySet *pMeths,SySet *pProps);` |
|        - | 3089 | `/*` |
|        - | 3090 | ` * Compile an object interface.` |
|        - | 3091 | ` *  According to the PHP language reference manual` |
|        - | 3092 | ` *   Object Interfaces:` |
|        - | 3093 | ` *   Object interfaces allow you to create code which specifies which methods` |
|        - | 3094 | ` *   a class must implement, without having to define how these methods are handled.` |
|        - | 3095 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|        - | 3096 | ` *   class, but without any of the methods having their contents defined.` |
|        - | 3097 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|        - | 3098 | ` */` |
|      366 | 3099 | `PH7_PRIVATE sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|        5 | 3100 | `{` |
|      371 | 3101 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 3102 | `	ph7_class *pClass,*pBase;` |
|      371 | 3103 | `	ph7_class *pSavedCurClass = pGen->pCurClass; /* restored at 'done' */` |
|      371 | 3104 | `	ph7_class *pSavedCurBase = pGen->pCurBase;   /* ...and its base, see pCurBase */` |
|        - | 3105 | `	SyToken *pEnd,*pTmp;` |
|        - | 3106 | `	SyString *pName;` |
|        - | 3107 | `	SySet aOvMeth,aOvProp;   /* the #[\Override] claims this interface DECLARED */` |
|        - | 3108 | ``	SySet aExtraParents;     /* `extends A, S, T`: every parent after the first */`` |
|      371 | 3109 | `	sxu32 nErrEntry = pGen->nErr; /* errors already reported when this one started */` |
|        - | 3110 | `	sxi32 nKwrd;` |
|        - | 3111 | `	sxi32 rc;` |
|        - | 3112 | `	{` |
|        - | 3113 | `		/* Deferral gate: parent interfaces may need an autoloader` |
|        - | 3114 | `		 * that has not run yet. */` |
|        - | 3115 | `		sxi32 rcDefer;` |
|      371 | 3116 | `		if( GenStateMaybeDeferClass(pGen,0,PH7_DEFER_KIND_INTERFACE,&rcDefer) ){` |
|       17 | 3117 | `			return rcDefer == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 3118 | `		}` |
|        - | 3119 | `	}` |
|        - | 3120 | `	/* Jump the 'interface' keyword */` |
|      357 | 3121 | `	pGen->pIn++;` |
|        - | 3122 | `	/* Extract interface name */` |
|      357 | 3123 | `	pName = &pGen->pIn->sData;` |
|        - | 3124 | `	/* Advance the stream cursor */` |
|      357 | 3125 | `	pGen->pIn++;` |
|        - | 3126 | `	/* Build FQN and obtain a raw class */ {` |
|        - | 3127 | `		SyBlob sFQN;` |
|        - | 3128 | `		SyString sFQNStr;` |
|      357 | 3129 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      357 | 3130 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      357 | 3131 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|        - | 3132 | ``		/* php refuses a declaration whose short name a local `use` already took. */`` |
|      357 | 3133 | `		if( GenStateGuardImportRedeclare(pGen,0,pName,&sFQNStr,nLine) == SXERR_ABORT ){` |
|      ! 0 | 3134 | `			SyBlobRelease(&sFQN);` |
|      ! 0 | 3135 | `			return SXERR_ABORT;` |
|        - | 3136 | `		}` |
|      357 | 3137 | `		GenStateRecordDeclaredName(pGen,0,&sFQNStr);` |
|      357 | 3138 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      357 | 3139 | `		SyBlobRelease(&sFQN);` |
|        - | 3140 | `	}` |
|      357 | 3141 | `	if( pClass == 0 ){` |
|      ! 0 | 3142 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 3143 | `		return SXERR_ABORT;` |
|        - | 3144 | `	}` |
|      357 | 3145 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|      357 | 3146 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 3147 | `		return SXERR_ABORT;` |
|        - | 3148 | `	}` |
|        - | 3149 | `	/* Mark as an interface (PH7_NewRawClass may have set INTERNAL) */` |
|      357 | 3150 | `	pClass->iFlags \|= PH7_CLASS_INTERFACE;` |
|      528 | 3151 | `	if( GenStateCheckAttrPlacement(&(*pGen),&pClass->aAttrs,1,1,` |
|      533 | 3152 | `			&pClass->sName,pClass->iFlags) == SXERR_ABORT ){` |
|      ! 0 | 3153 | `		return SXERR_ABORT;` |
|        - | 3154 | `	}` |
|        - | 3155 | `	/* Assume no base class is given */` |
|      357 | 3156 | `	pBase = 0;` |
|      357 | 3157 | `	SySetInit(&aExtraParents,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      357 | 3158 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       58 | 3159 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       58 | 3160 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a, c, … */ ){` |
|        - | 3161 | `			/* php lets an interface extend SEVERAL parent interfaces. The first` |
|        - | 3162 | `			 * becomes pBase (single-inheritance chain, hDerived); every extra one` |
|        - | 3163 | `			 * is recorded via PH7_ClassImplement so it lands in aInterface — the` |
|        - | 3164 | `			 * runtime subtype walk (VmInterfaceReaches) follows both. */` |
|       58 | 3165 | `			pGen->pIn++;` |
|       36 | 3166 | `			for(;;){` |
|        - | 3167 | `				SyBlob sResolved;` |
|        - | 3168 | `				SyString sBaseName;` |
|        - | 3169 | `				sxu32 nRefLine;` |
|        - | 3170 | `				ph7_class *pParent;` |
|       76 | 3171 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|       76 | 3172 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       76 | 3173 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 3174 | `					SyBlobRelease(&sResolved);` |
|      ! 0 | 3175 | `					rc = PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        - | 3176 | `						"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|      ! 0 | 3177 | `						pName);` |
|      ! 0 | 3178 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 3179 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3180 | `						return SXERR_ABORT;` |
|        - | 3181 | `					}` |
|      ! 0 | 3182 | `					return SXRET_OK;` |
|        - | 3183 | `				}` |
|      148 | 3184 | `				pParent = PH7_VmExtractClass(pGen->pVm,` |
|       72 | 3185 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       76 | 3186 | `				SyStringInitFromBuf(&sBaseName,` |
|        - | 3187 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - | 3188 | `				/* Only interfaces is allowed */` |
|       76 | 3189 | `				while( pParent && (pParent->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 3190 | `					pParent = pParent->pNextName;` |
|      ! 0 | 3191 | `				}` |
|       76 | 3192 | `				if( pParent == 0 ){` |
|        2 | 3193 | `					if( !GenStateLintSkipsBase(pGen,pClass) ){` |
|      ! 0 | 3194 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - | 3195 | `							"Nonexistent base interface '%z'",&sBaseName);` |
|      ! 0 | 3196 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 3197 | `							SyBlobRelease(&sResolved);` |
|      ! 0 | 3198 | `							return SXERR_ABORT;` |
|        - | 3199 | `						}` |
|      ! 0 | 3200 | `					}` |
|       75 | 3201 | `				}else if( pBase == 0 ){` |
|        - | 3202 | `					/* First parent → single-inheritance base */` |
|       56 | 3203 | `					pBase = pParent;` |
|       30 | 3204 | `				}else{` |
|        - | 3205 | `					/* Additional parent → COLLECTED, and applied after the body like` |
|        - | 3206 | `					 * the first one is. Copying its members here put them in hMethod` |
|        - | 3207 | `					 * and hConst before the body was read, so the interface's own` |
|        - | 3208 | ``					 * `public function g();` collided with the very name it was`` |
|        - | 3209 | ``					 * restating: `interface B extends A, S` could redeclare A's`` |
|        - | 3210 | `					 * members and not S's ("Cannot redeclare B::g()"), which is a` |
|        - | 3211 | `					 * declaration php accepts and php-di writes. */` |
|       21 | 3212 | `					SySetPut(&aExtraParents,(const void *)&pParent);` |
|        - | 3213 | `				}` |
|       76 | 3214 | `				SyBlobRelease(&sResolved);` |
|        - | 3215 | `				/* Continue on a comma-separated list */` |
|       76 | 3216 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       21 | 3217 | `					pGen->pIn++;` |
|       21 | 3218 | `					continue;` |
|        - | 3219 | `				}` |
|       58 | 3220 | `				break;` |
|      ! 0 | 3221 | `			}` |
|       27 | 3222 | `		}` |
|       27 | 3223 | `	}` |
|      357 | 3224 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|        - | 3225 | `		/* Syntax error */` |
|      ! 0 | 3226 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Expected '{' after interface '%z' definition",pName);` |
|      ! 0 | 3227 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 3228 | `		if( rc == SXERR_ABORT ){` |
|        - | 3229 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3230 | `			return SXERR_ABORT;` |
|        - | 3231 | `		}` |
|      ! 0 | 3232 | `		return SXRET_OK;` |
|        - | 3233 | `	}` |
|      357 | 3234 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      357 | 3235 | `	pEnd = 0; /* cc warning */` |
|        - | 3236 | `	/* Delimit the interface body */` |
|      357 | 3237 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|      357 | 3238 | `	if( pEnd >= pGen->pEnd ){` |
|        - | 3239 | `		/* Syntax error */` |
|      ! 0 | 3240 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Missing '}' after interface '%z' definition",pName);` |
|      ! 0 | 3241 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 3242 | `		if( rc == SXERR_ABORT ){` |
|        - | 3243 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3244 | `			return SXERR_ABORT;` |
|        - | 3245 | `		}` |
|      ! 0 | 3246 | `		return SXRET_OK;` |
|        - | 3247 | `	}` |
|        - | 3248 | `	/* The delimiter token is the interface body's closing brace */` |
|      357 | 3249 | `	pClass->nEndLine = pEnd->nLine;` |
|        - | 3250 | `	/* Swap token stream */` |
|      357 | 3251 | `	pTmp = pGen->pEnd;` |
|      357 | 3252 | `	pGen->pEnd = pEnd;` |
|        - | 3253 | `	/* This interface is now the lexical class for its body (see pCurClass) — a` |
|        - | 3254 | `	 * const default here is not a trait, so __TRAIT__ stays "". */` |
|      357 | 3255 | `	pGen->pCurClass = pClass;` |
|      357 | 3256 | ``	pGen->pCurBase = 0; /* php gives an interface no `parent`, however many it extends */`` |
|        - | 3257 | `	/* Start the parse process` |
|        - | 3258 | `	 * Note (According to the PHP reference manual):` |
|        - | 3259 | `	 *  Only constants and function signatures(without body) are allowed.` |
|        - | 3260 | `	 *  Only 'public' visibility is allowed.` |
|        - | 3261 | `	 */` |
|      270 | 3262 | `	for(;;){` |
|        - | 3263 | `		/* Jump leading/trailing semi-colons */` |
|      729 | 3264 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|      189 | 3265 | `			pGen->pIn++;` |
|        5 | 3266 | `		}` |
|      545 | 3267 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 3268 | `			/* End of interface body */` |
|      339 | 3269 | `			break;` |
|        - | 3270 | `		}` |
|        - | 3271 | `		/* Bind a directly-preceding docblock to this member */` |
|      211 | 3272 | `		GenStateSetPendingDoc(&(*pGen));` |
|      206 | 3273 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      108 | 3274 | `			&& !GenStateIsReadonly(pGen->pIn) ){` |
|      ! 0 | 3275 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|        - | 3276 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|      ! 0 | 3277 | `				&pGen->pIn->sData,pName);` |
|      ! 0 | 3278 | `			if( rc == SXERR_ABORT ){` |
|        - | 3279 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 3280 | `				return SXERR_ABORT;` |
|        - | 3281 | `			}` |
|      ! 0 | 3282 | `			goto done;` |
|        - | 3283 | `		}` |
|        - | 3284 | `		/* The member, through the shared modifier run -- which is where an` |
|        - | 3285 | ``		 * interface's own rules live now (public-only, no `final`, no written`` |
|        - | 3286 | ``		 * `abstract`, and a property that may only be a hooked requirement). */`` |
|      211 | 3287 | `		rc = GenStateCompileMember(&(*pGen),pClass,"interface");` |
|      211 | 3288 | `		if( rc != SXRET_OK ){` |
|       22 | 3289 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3290 | `				return SXERR_ABORT;` |
|        - | 3291 | `			}` |
|       22 | 3292 | `			goto done;` |
|        - | 3293 | `		}` |
|        5 | 3294 | `	}` |
|        - | 3295 | `	/* An interface method may claim #[\Override] too, against the interfaces this` |
|        - | 3296 | `	 * one extends -- collected before the inherit copies theirs in. */` |
|      339 | 3297 | `	GenStateCollectOverrides(&(*pGen),pClass,&aOvMeth,&aOvProp);` |
|        - | 3298 | `	/* Reject a php-fatal redeclaration before hoisting the interface. An` |
|        - | 3299 | `	 * interface that EXTENDS another is not early-bound, exactly as a class` |
|        - | 3300 | `	 * that implements one is not. */` |
|      616 | 3301 | `	if( GenStateGuardClassRedeclaration(pGen,pClass,` |
|      313 | 3302 | `			pBase == 0 && SySetUsed(&aExtraParents) == 0) == SXERR_ABORT ){` |
|        3 | 3303 | `		SySetRelease(&aOvMeth);` |
|        3 | 3304 | `		SySetRelease(&aOvProp);` |
|        3 | 3305 | `		return SXERR_ABORT;` |
|        - | 3306 | `	}` |
|        - | 3307 | `	/* Every method this interface DECLARES, judged against the same name in each` |
|        - | 3308 | `	 * parent -- hMethod holds only its own declarations until the inherits below` |
|        - | 3309 | `	 * run, so this is the one moment the two sets are separable. php makes the` |
|        - | 3310 | `	 * check for a restated method whichever parent it came from; PHL made it for` |
|        - | 3311 | ``	 * none of them, so `interface B extends A { public function f(): int; }` over`` |
|        - | 3312 | ``	 * `A::f(): string` compiled in silence. */`` |
|      337 | 3313 | `	if( pGen->nErr == nErrEntry ){` |
|      329 | 3314 | `		if( pBase && PH7_ClassInterfaceCheckRedeclare(&(*pGen),pClass,pBase) == SXERR_ABORT ){` |
|      ! 0 | 3315 | `			SySetRelease(&aOvMeth);` |
|      ! 0 | 3316 | `			SySetRelease(&aOvProp);` |
|      ! 0 | 3317 | `			SySetRelease(&aExtraParents);` |
|      ! 0 | 3318 | `			return SXERR_ABORT;` |
|        - | 3319 | `		}` |
|        - | 3320 | `		{` |
|      329 | 3321 | `			ph7_class **apExtra = (ph7_class **)SySetBasePtr(&aExtraParents);` |
|        - | 3322 | `			sxu32 nExtra;` |
|      347 | 3323 | `			for( nExtra = 0 ; nExtra < SySetUsed(&aExtraParents) ; ++nExtra ){` |
|       18 | 3324 | `				if( PH7_ClassInterfaceCheckRedeclare(&(*pGen),pClass,apExtra[nExtra])` |
|       12 | 3325 | `					== SXERR_ABORT ){` |
|      ! 0 | 3326 | `					SySetRelease(&aOvMeth);` |
|      ! 0 | 3327 | `					SySetRelease(&aOvProp);` |
|      ! 0 | 3328 | `					SySetRelease(&aExtraParents);` |
|      ! 0 | 3329 | `					return SXERR_ABORT;` |
|        - | 3330 | `				}` |
|       12 | 3331 | `			}` |
|        - | 3332 | `		}` |
|      162 | 3333 | `	}` |
|        - | 3334 | `	/* Install the interface */` |
|      337 | 3335 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      337 | 3336 | `	if( rc == SXRET_OK && pBase ){` |
|        - | 3337 | `		/* Inherit from the base interface */` |
|       56 | 3338 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|       26 | 3339 | `	}` |
|      337 | 3340 | `	if( rc == SXRET_OK ){` |
|        - | 3341 | `		/* ...and from every parent after the first, whose members are copied only` |
|        - | 3342 | `		 * where this interface declared none of its own. */` |
|      337 | 3343 | `		ph7_class **apExtra = (ph7_class **)SySetBasePtr(&aExtraParents);` |
|        - | 3344 | `		sxu32 nExtra;` |
|      355 | 3345 | `		for( nExtra = 0 ; rc == SXRET_OK && nExtra < SySetUsed(&aExtraParents) ; ++nExtra ){` |
|       21 | 3346 | `			rc = PH7_ClassImplement(pClass,apExtra[nExtra]);` |
|       12 | 3347 | `		}` |
|      166 | 3348 | `	}` |
|      337 | 3349 | `	SySetRelease(&aExtraParents);` |
|      332 | 3350 | `	if( rc == SXRET_OK && pGen->nErr == nErrEntry` |
|      332 | 3351 | `	 && GenStateCheckOverrides(&(*pGen),pClass,&aOvMeth,&aOvProp) == SXERR_ABORT ){` |
|      ! 0 | 3352 | `		SySetRelease(&aOvMeth);` |
|      ! 0 | 3353 | `		SySetRelease(&aOvProp);` |
|      ! 0 | 3354 | `		return SXERR_ABORT;` |
|        - | 3355 | `	}` |
|      337 | 3356 | `	SySetRelease(&aOvMeth);` |
|      337 | 3357 | `	SySetRelease(&aOvProp);` |
|      337 | 3358 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3359 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 3360 | `		return SXERR_ABORT;` |
|        - | 3361 | `	}` |
|      166 | 3362 | `done:` |
|      355 | 3363 | `	SySetRelease(&aExtraParents);` |
|      355 | 3364 | `	pGen->pCurClass = pSavedCurClass;` |
|      355 | 3365 | `	pGen->pCurBase = pSavedCurBase;` |
|        - | 3366 | `	/* Point beyond the interface body */` |
|      355 | 3367 | `	pGen->pIn  = &pEnd[1];` |
|      355 | 3368 | `	pGen->pEnd = pTmp;` |
|      355 | 3369 | `	return PH7_OK;` |
|      188 | 3370 | `}` |
|        - | 3371 | `/*` |
|        - | 3372 | ` * Compile a user-defined class.` |
|        - | 3373 | ` * According to the PHP language reference manual` |
|        - | 3374 | ` *  class` |
|        - | 3375 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|        - | 3376 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|        - | 3377 | ` *  of the properties and methods belonging to the class.` |
|        - | 3378 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|        - | 3379 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|        - | 3380 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|        - | 3381 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|        - | 3382 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|        - | 3383 | ` *  (called "methods").` |
|        - | 3384 | ` */` |
|        - | 3385 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|        - | 3386 | `typedef struct TraitUseEntry TraitUseEntry;` |
|        - | 3387 | `struct TraitUseEntry {` |
|        - | 3388 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|        - | 3389 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|        - | 3390 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|        - | 3391 | `};` |
|        - | 3392 | `/*` |
|        - | 3393 | ` * Validate that methods implementing interface contracts have compatible` |
|        - | 3394 | ` * signatures: public visibility and at least as many parameters as declared.` |
|        - | 3395 | ` */` |
|     5108 | 3396 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 | 3397 | `{` |
|        - | 3398 | `	ph7_class **apIface;` |
|        - | 3399 | `	sxu32 nIface,i;` |
|        - | 3400 | `	sxi32 rc;` |
|     5113 | 3401 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      ! 0 | 3402 | `		return SXRET_OK;` |
|        - | 3403 | `	}` |
|     5113 | 3404 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|     5113 | 3405 | `	nIface = SySetUsed(&pClass->aInterface);` |
|     6073 | 3406 | `	for(i = 0; i < nIface; i++){` |
|      965 | 3407 | `		ph7_class *pIface = apIface[i];` |
|        - | 3408 | `		SyHashEntry *pEntry;` |
|      965 | 3409 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|     2425 | 3410 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|     1465 | 3411 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - | 3412 | `			ph7_class_method *pImplMeth;` |
|     1465 | 3413 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|        - | 3414 | `			/* Find the implementing method in the class */` |
|     1465 | 3415 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|     1465 | 3416 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       41 | 3417 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|        - | 3418 | `			}` |
|        - | 3419 | `			/* Check visibility: interface methods must be implemented as public */` |
|     1429 | 3420 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        4 | 3421 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|        - | 3422 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|        1 | 3423 | `					&pClass->sDisp,pMName,&pIface->sDisp);` |
|        3 | 3424 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3425 | `					return SXERR_ABORT;` |
|        - | 3426 | `				}` |
|        1 | 3427 | `			}` |
|        - | 3428 | `			/* Signature compatibility. php checks an implementation against the` |
|        - | 3429 | `			 * interface's declaration with the very rule it checks an override by --` |
|        - | 3430 | `			 * one shared body, so the variance half is not the class hierarchy's` |
|        - | 3431 | `			 * alone and the fatal reads the same. This site used to count PARAMETERS` |
|        - | 3432 | ``			 * only, and word the two declarations as bare `$name` lists.`` |
|        - | 3433 | `			 *` |
|        - | 3434 | `			 * A CONSTRUCTOR is not exempt here: php exempts an INHERITED one from` |
|        - | 3435 | ``			 * variance, but an interface that declares `__construct` constrains every`` |
|        - | 3436 | `			 * implementor's. */` |
|     1424 | 3437 | `			if( PH7_ClassCheckOverrideCompat(&(*pGen),pIface,pClass,pIfaceMeth,pImplMeth,0)` |
|      717 | 3438 | `				== SXERR_ABORT ){` |
|      ! 0 | 3439 | `				return SXERR_ABORT;` |
|        - | 3440 | `			}` |
|        5 | 3441 | `		}` |
|      485 | 3442 | `	}` |
|     5113 | 3443 | `	return SXRET_OK;` |
|     2559 | 3444 | `}` |
|        - | 3445 | `/*` |
|        - | 3446 | ` * An abstract property-hook stub (__phl_hook_{get,set}_NAME) is satisfied by` |
|        - | 3447 | ` * the class declaring a PLAIN (non-abstract, non-hooked) property NAME: php` |
|        - | 3448 | `` * lets a plain property implement `{ get; set; }` requirements — its raw`` |
|        - | 3449 | ` * read/write IS the default get/set. A concrete hook override replaced the` |
|        - | 3450 | ` * stub in hMethod already, so a surviving stub next to a HOOKED property` |
|        - | 3451 | ` * means that specific hook is still missing.` |
|        - | 3452 | ` */` |
|       98 | 3453 | `static int GenStateAbstractHookSatisfied(ph7_class *pClass,const SyString *pMName)` |
|        5 | 3454 | `{` |
|        - | 3455 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|        - | 3456 | `	ph7_class_attr *pProp;` |
|       98 | 3457 | `	if( pMName->nByte <= nPfx` |
|       59 | 3458 | `	 \|\| (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) != 0` |
|        6 | 3459 | `	  && SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) != 0) ){` |
|       92 | 3460 | `		return 0; /* not a hook stub */` |
|        - | 3461 | `	}` |
|       12 | 3462 | `	pProp = PH7_ClassExtractAttribute(pClass,&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|       12 | 3463 | `	return pProp != 0` |
|       10 | 3464 | `		&& (pProp->iFlags & (PH7_CLASS_ATTR_ABSTRACT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|        5 | 3465 | `			\|PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) == 0;` |
|       54 | 3466 | `}` |
|        - | 3467 | `/*` |
|        - | 3468 | ` * Append an abstract member's display name to the message blob, translating a` |
|        - | 3469 | `` * property-hook stub (__phl_hook_get_x) to php's `$x::get` form.`` |
|        - | 3470 | ` */` |
|       40 | 3471 | `static void GenStateAppendAbstractMemberName(SyBlob *pMsg,const SyString *pMName)` |
|        4 | 3472 | `{` |
|        - | 3473 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|       40 | 3474 | `	if( pMName->nByte > nPfx` |
|       24 | 3475 | `	 && (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) == 0` |
|      ! 0 | 3476 | `	  \|\| SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) == 0) ){` |
|      ! 0 | 3477 | `		SyBlobAppend(pMsg,"$",1);` |
|      ! 0 | 3478 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|      ! 0 | 3479 | `		SyBlobAppend(pMsg,"::",2);` |
|      ! 0 | 3480 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[sizeof("__phl_hook_")-1],3);` |
|      ! 0 | 3481 | `		return;` |
|        - | 3482 | `	}` |
|       44 | 3483 | `	SyBlobAppend(pMsg,(const void *)pMName->zString,pMName->nByte);` |
|       24 | 3484 | `}` |
|        - | 3485 | `/*` |
|        - | 3486 | ` * ---------------------------------------------------------------------------` |
|        - | 3487 | `` * php's `#[\Override]` (8.3, widened to properties in 8.5).`` |
|        - | 3488 | ` *` |
|        - | 3489 | ` * The attribute is a CLAIM the engine checks where the member is written: the` |
|        - | 3490 | ` * name must already exist above, so a typo, a renamed parent method or a base` |
|        - | 3491 | ` * class that dropped one is a fatal at the declaration instead of a method` |
|        - | 3492 | ` * nobody ever calls. php runs it at class LINK time, after inheritance, and its` |
|        - | 3493 | ` * rules are the inheritance rules rather than a name search:` |
|        - | 3494 | ` *` |
|        - | 3495 | ` *   - a PRIVATE parent member is not inherited, so it is not something to` |
|        - | 3496 | ` *     override;` |
|        - | 3497 | ` *   - the CONSTRUCTOR is exempt from php's inheritance signature check unless it` |
|        - | 3498 | ` *     is abstract (or an interface's), and #[\Override] follows that exemption —` |
|        - | 3499 | `` *     a concrete parent `__construct` does NOT satisfy the claim while an`` |
|        - | 3500 | ` *     abstract one does;` |
|        - | 3501 | ` *   - a method matches CASE-INSENSITIVELY and a property case-SENSITIVELY, which` |
|        - | 3502 | ` *     is php's rule for the two namespaces everywhere else;` |
|        - | 3503 | ` *   - an interface counts for a method, at any depth and through any ancestor;` |
|        - | 3504 | ` *   - a TRAIT used by this very class does not: its method is the using class's` |
|        - | 3505 | ` *     own, and php reports the USING class's name when the claim fails.` |
|        - | 3506 | ` * ---------------------------------------------------------------------------` |
|        - | 3507 | ` */` |
|        - | 3508 | `#define GEN_OVERRIDE_ATTR "Override"` |
|        - | 3509 | ``/* Does this member carry `#[\Override]`? The compiler resolves an attribute name`` |
|        - | 3510 | ` * to its fully-qualified spelling and class names are case-insensitive, so one` |
|        - | 3511 | ` * case-folded compare against the whole set is the test. */` |
|     8442 | 3512 | `static int GenStateHasOverrideAttr(SySet *pAttrs)` |
|        5 | 3513 | `{` |
|     8447 | 3514 | `	ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|        - | 3515 | `	sxu32 n;` |
|     8579 | 3516 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; ++n ){` |
|      212 | 3517 | `		if( aAttr[n].sName.nByte == sizeof(GEN_OVERRIDE_ATTR)-1` |
|      151 | 3518 | `		 && SyStrnicmp(aAttr[n].sName.zString,GEN_OVERRIDE_ATTR,` |
|       40 | 3519 | `			sizeof(GEN_OVERRIDE_ATTR)-1) == 0 ){` |
|       81 | 3520 | `			return 1;` |
|        - | 3521 | `		}` |
|       71 | 3522 | `	}` |
|     8367 | 3523 | `	return 0;` |
|     4226 | 3524 | `}` |
|        - | 3525 | `/* Does an interface reachable from pClass -- its own, or any ancestor's --` |
|        - | 3526 | ` * declare this method? An interface that extends others already carries their` |
|        - | 3527 | ` * stubs in its own table, so one level of lookup per interface is enough. */` |
|       30 | 3528 | `static int GenStateIfaceDeclaresMethod(ph7_class *pClass,const SyString *pName)` |
|        1 | 3529 | `{` |
|       67 | 3530 | `	for( ; pClass ; pClass = pClass->pBase ){` |
|       45 | 3531 | `		ph7_class **apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|        - | 3532 | `		sxu32 n;` |
|       47 | 3533 | `		for( n = 0 ; n < SySetUsed(&pClass->aInterface) ; ++n ){` |
|       10 | 3534 | `			if( apIface[n]` |
|       11 | 3535 | `			 && PH7_ClassExtractMethod(apIface[n],pName->zString,pName->nByte) ){` |
|        9 | 3536 | `				return 1;` |
|        - | 3537 | `			}` |
|        2 | 3538 | `		}` |
|       19 | 3539 | `	}` |
|       23 | 3540 | `	return 0;` |
|       16 | 3541 | `}` |
|        - | 3542 | `/* Is there a parent METHOD this one may claim to override? */` |
|       50 | 3543 | `static int GenStateOverridesMethod(ph7_class *pClass,const SyString *pName)` |
|        1 | 3544 | `{` |
|       79 | 3545 | `	int bCtor = pName->nByte == sizeof("__construct")-1` |
|       50 | 3546 | `		&& SyStrnicmp(pName->zString,"__construct",sizeof("__construct")-1) == 0;` |
|        - | 3547 | `	ph7_class *pWalk;` |
|       65 | 3548 | `	for( pWalk = pClass->pBase ; pWalk ; pWalk = pWalk->pBase ){` |
|       35 | 3549 | `		ph7_class_method *pMeth = PH7_ClassExtractMethod(pWalk,pName->zString,pName->nByte);` |
|       35 | 3550 | `		if( pMeth == 0 \|\| pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       11 | 3551 | `			continue;` |
|        - | 3552 | `		}` |
|       25 | 3553 | `		if( bCtor && (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|        5 | 3554 | `			continue;   /* php exempts a concrete parent constructor */` |
|        - | 3555 | `		}` |
|       21 | 3556 | `		return 1;` |
|      ! 0 | 3557 | `	}` |
|       31 | 3558 | `	return GenStateIfaceDeclaresMethod(pClass,pName);` |
|       26 | 3559 | `}` |
|        - | 3560 | `/* Is there a parent PROPERTY this one may claim to override? Interfaces declare` |
|        - | 3561 | ` * none, so this is the base chain alone. */` |
|       22 | 3562 | `static int GenStateOverridesProp(ph7_class *pClass,const SyString *pName)` |
|        1 | 3563 | `{` |
|        - | 3564 | `	ph7_class *pWalk;` |
|       31 | 3565 | `	for( pWalk = pClass->pBase ; pWalk ; pWalk = pWalk->pBase ){` |
|       21 | 3566 | `		ph7_class_attr *pAttr = PH7_ClassExtractAttribute(pWalk,pName->zString,pName->nByte);` |
|       21 | 3567 | `		if( pAttr && pAttr->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
|       13 | 3568 | `			return 1;` |
|        - | 3569 | `		}` |
|        5 | 3570 | `	}` |
|       11 | 3571 | `	return 0;` |
|       12 | 3572 | `}` |
|        - | 3573 | `/*` |
|        - | 3574 | `` * Verify every `#[\Override]` the class DECLARED, in php's order: the methods`` |
|        - | 3575 | ` * first and then the properties, each in declaration order, and the first` |
|        - | 3576 | ` * failure is the whole diagnostic (it is a fatal).` |
|        - | 3577 | ` *` |
|        - | 3578 | ` * The two sets are collected BEFORE inheritance runs -- an inherited method` |
|        - | 3579 | ` * keeps the parent's attribute record and php does not re-check it there -- and` |
|        - | 3580 | ` * verified after, which is when the answer exists.` |
|        - | 3581 | ` */` |
|     5454 | 3582 | `static sxi32 GenStateCollectOverrides(ph7_gen_state *pGen,ph7_class *pClass,` |
|        - | 3583 | `	SySet *pMeths,SySet *pProps)` |
|        5 | 3584 | `{` |
|        - | 3585 | `	static const sxu32 nHookPfx = sizeof("__phl_hook_get_")-1;` |
|        - | 3586 | `	SyHashEntry *pEntry;` |
|     5459 | 3587 | `	SySetInit(pMeths,&pGen->pVm->sAllocator,sizeof(ph7_class_method *));` |
|     5459 | 3588 | `	SySetInit(pProps,&pGen->pVm->sAllocator,sizeof(ph7_class_attr *));` |
|     5459 | 3589 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|    11051 | 3590 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|     5597 | 3591 | `		ph7_class_method *pMeth = (ph7_class_method *)pEntry->pUserData;` |
|     5597 | 3592 | `		SyString *pName = &pMeth->sFunc.sName;` |
|        - | 3593 | `		/* A property hook is compiled to a method here and is a PROPERTY in php,` |
|        - | 3594 | `		 * so the property arm below owns its claim. */` |
|     5592 | 3595 | `		if( pName->nByte > nHookPfx` |
|     2946 | 3596 | `		 && SyMemcmp((const void *)pName->zString,(const void *)"__phl_hook_",` |
|      145 | 3597 | `			sizeof("__phl_hook_")-1) == 0 ){` |
|      249 | 3598 | `			continue;` |
|        - | 3599 | `		}` |
|     5353 | 3600 | `		if( GenStateHasOverrideAttr(&pMeth->sFunc.aAttrs) ){` |
|       57 | 3601 | `			SySetPut(pMeths,(const void *)&pMeth);` |
|       28 | 3602 | `		}` |
|        5 | 3603 | `	}` |
|     5459 | 3604 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     8553 | 3605 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|     3099 | 3606 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     3099 | 3607 | `		if( GenStateHasOverrideAttr(&pAttr->aAttrs) ){` |
|       25 | 3608 | `			SySetPut(pProps,(const void *)&pAttr);` |
|       12 | 3609 | `		}` |
|        5 | 3610 | `	}` |
|     5459 | 3611 | `	return SXRET_OK;` |
|        5 | 3612 | `}` |
|     5182 | 3613 | `static sxi32 GenStateCheckOverrides(ph7_gen_state *pGen,ph7_class *pClass,` |
|        - | 3614 | `	SySet *pMeths,SySet *pProps)` |
|        5 | 3615 | `{` |
|     5187 | 3616 | `	ph7_class_method **apMeth = (ph7_class_method **)SySetBasePtr(pMeths);` |
|     5187 | 3617 | `	ph7_class_attr **apProp = (ph7_class_attr **)SySetBasePtr(pProps);` |
|        - | 3618 | `	sxu32 n;` |
|     5187 | 3619 | `	if( pClass->iFlags & PH7_CLASS_LINT_UNBOUND ){` |
|        - | 3620 | `		/* A base this lint could not see may well DECLARE the member; php reports` |
|        - | 3621 | `		 * an #[\Override] mismatch only where it early-binds, and it early-binds` |
|        - | 3622 | `		 * nothing it cannot link. */` |
|        8 | 3623 | `		return SXRET_OK;` |
|        - | 3624 | `	}` |
|        - | 3625 | `	/* hMethod is a LIFO iteration list (SyHashInsert), so the collected order is` |
|        - | 3626 | `	 * the REVERSE of the declaration order; hAttr is a FIFO one` |
|        - | 3627 | `	 * (SyHashInsertTail) and needs no such turn. php reports the first member it` |
|        - | 3628 | `	 * finds in declaration order and stops. */` |
|     5207 | 3629 | `	for( n = SySetUsed(pMeths) ; n > 0 ; --n ){` |
|       51 | 3630 | `		ph7_class_method *pMeth = apMeth[n - 1];` |
|       51 | 3631 | `		if( !GenStateOverridesMethod(pClass,&pMeth->sFunc.sName) ){` |
|       34 | 3632 | `			return PH7_GenCompileError(&(*pGen),E_ERROR,pMeth->sFunc.nLine,` |
|        - | 3633 | `				"%z::%z() has #[\\Override] attribute, but no matching parent method exists",` |
|       11 | 3634 | `				&pClass->sDisp,&pMeth->sFunc.sName);` |
|        - | 3635 | `		}` |
|       15 | 3636 | `	}` |
|     5169 | 3637 | `	for( n = 0 ; n < SySetUsed(pProps) ; ++n ){` |
|       23 | 3638 | `		if( !GenStateOverridesProp(pClass,&apProp[n]->sName) ){` |
|        - | 3639 | `			/* php reports the CLASS's line for a property, not the property's. */` |
|       16 | 3640 | `			return PH7_GenCompileError(&(*pGen),E_ERROR,pClass->nLine,` |
|        - | 3641 | `				"%z::$%z has #[\\Override] attribute, but no matching parent property exists",` |
|       10 | 3642 | `				&pClass->sDisp,&apProp[n]->sName);` |
|        - | 3643 | `		}` |
|        7 | 3644 | `	}` |
|     5147 | 3645 | `	return SXRET_OK;` |
|     2596 | 3646 | `}` |
|        - | 3647 | `/*` |
|        - | 3648 | ` * Check that a concrete class has no remaining abstract methods.` |
|        - | 3649 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|        - | 3650 | ` */` |
|        - | 3651 | `/*` |
|        - | 3652 | ` * The interface FURTHEST up that declares this method name. php attributes an` |
|        - | 3653 | ` * unimplemented method to the interface that first ASKED for it, and an` |
|        - | 3654 | ` * interface reaches its parents through two containers: pBase (the first name` |
|        - | 3655 | `` * after `extends`) and aInterface (every one after that). Following only pBase`` |
|        - | 3656 | `` * stopped at the restating interface, so `interface B extends A, S` reported`` |
|        - | 3657 | `` * `B::g` where php reports `S::g`.`` |
|        - | 3658 | ` *` |
|        - | 3659 | ` * Depth-bounded like the Throwable walk beside it: an interface graph cannot` |
|        - | 3660 | ` * cycle (every parent is already compiled), and the bound costs nothing.` |
|        - | 3661 | ` */` |
|       40 | 3662 | `static ph7_class * GenStateIfaceDeclaringAt(ph7_class *pIface,const SyString *pMName,int iDepth)` |
|        4 | 3663 | `{` |
|       44 | 3664 | `	ph7_class *pDeepest = 0;` |
|        - | 3665 | `	ph7_class **apUp;` |
|        - | 3666 | `	sxu32 i;` |
|       44 | 3667 | `	if( pIface == 0 \|\| iDepth > 32 ){` |
|       22 | 3668 | `		return 0;` |
|        - | 3669 | `	}` |
|       26 | 3670 | `	if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|       20 | 3671 | `		pDeepest = pIface;` |
|        8 | 3672 | `	}` |
|        - | 3673 | `	{` |
|       26 | 3674 | `		ph7_class *pUp = GenStateIfaceDeclaringAt(pIface->pBase,pMName,iDepth + 1);` |
|       26 | 3675 | `		if( pUp ){` |
|      ! 0 | 3676 | `			pDeepest = pUp;` |
|      ! 0 | 3677 | `		}` |
|        - | 3678 | `	}` |
|       26 | 3679 | `	apUp = (ph7_class **)SySetBasePtr(&pIface->aInterface);` |
|       28 | 3680 | `	for( i = 0 ; i < SySetUsed(&pIface->aInterface) ; ++i ){` |
|        3 | 3681 | `		ph7_class *pUp = GenStateIfaceDeclaringAt(apUp[i],pMName,iDepth + 1);` |
|        3 | 3682 | `		if( pUp ){` |
|        3 | 3683 | `			pDeepest = pUp;` |
|        1 | 3684 | `		}` |
|        2 | 3685 | `	}` |
|       26 | 3686 | `	return pDeepest;` |
|       24 | 3687 | `}` |
|        - | 3688 | `/*` |
|        - | 3689 | ` * Is this method-table name one of php's property HOOKS rather than a method?` |
|        - | 3690 | ` * php verifies a class's abstract METHODS first and its abstract property hooks` |
|        - | 3691 | ` * in a second pass, so the two groups do not interleave in the message however` |
|        - | 3692 | ` * the source is written.` |
|        - | 3693 | ` */` |
|      116 | 3694 | `static int GenStateIsHookName(const SyString *pMName)` |
|        4 | 3695 | `{` |
|        - | 3696 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|      122 | 3697 | `	return pMName->nByte > nPfx` |
|      118 | 3698 | `	 && (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) == 0` |
|        2 | 3699 | `	  \|\| SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) == 0);` |
|        4 | 3700 | `}` |
|        - | 3701 | `/*` |
|        - | 3702 | ` * php lists the unimplemented abstract methods in its own FUNCTION-TABLE order,` |
|        - | 3703 | ` * which is the order it LINKS a class in: the class's own declarations, then` |
|        - | 3704 | ` * inheritance, then the traits, then the interfaces -- each of those recursively.` |
|        - | 3705 | `` * So `class C extends P implements I { use T; }` reads `P::pa, C::ta, I::ia`,`` |
|        - | 3706 | `` * with the trait's ahead of the interface's though the `use` is written inside`` |
|        - | 3707 | `` * the body and the `implements` in the header; and `interface K extends J`` |
|        - | 3708 | `` * extends I` reads `K::ka, J::ja, I::ia`. A walk of this engine's own method hash`` |
|        - | 3709 | ` * reads none of that: it is one flat table, in an order the hash decides. The` |
|        - | 3710 | ` * sequence is rebuilt for the message rather than in hMethod itself, which` |
|        - | 3711 | ` * get_class_methods() and Reflection also read.` |
|        - | 3712 | ` *` |
|        - | 3713 | ` * Appends every method DECLARED by pSrc that is still an unimplemented abstract` |
|        - | 3714 | ` * of pClass and is not already in *pOrder, then recurses. bHooks selects which of` |
|        - | 3715 | ` * php's two passes this is.` |
|        - | 3716 | ` */` |
|      260 | 3717 | `static void GenStateOrderAbstractsFrom(ph7_class *pClass,ph7_class *pSrc,SySet *pOrder,` |
|        - | 3718 | `	int bHooks,int iDepth)` |
|        4 | 3719 | `{` |
|        - | 3720 | `	SyHashEntry *pEntry;` |
|        - | 3721 | `	ph7_class **apSrc;` |
|        - | 3722 | `	sxu32 n;` |
|      264 | 3723 | `	if( pSrc == 0 \|\| iDepth > 32 ){` |
|      104 | 3724 | `		return;` |
|        - | 3725 | `	}` |
|        - | 3726 | ``	/* Declaration order, not the hash's own LIFO walk: `abstract function pa();`` |
|        - | 3727 | ``	 * abstract function pb();` must list pa first. (And this is a nested walk of`` |
|        - | 3728 | `	 * another class's table -- SyHash carries a single shared loop cursor, so the` |
|        - | 3729 | `	 * cursor-based iterator could not be used here in any case.) */` |
|      416 | 3730 | `	for( pEntry = SyHashTailEntry(&pSrc->hMethod) ; pEntry ; pEntry = SyHashEntryPrev(pEntry) ){` |
|      256 | 3731 | `		ph7_class_method *pSrcMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - | 3732 | `		ph7_class_method *pMeth;` |
|        - | 3733 | `		ph7_class_method **apSeen;` |
|      256 | 3734 | `		int bSeen = 0;` |
|      256 | 3735 | `		if( pSrcMeth->sFunc.pUserData != (void *)pSrc ){` |
|        - | 3736 | `			/* Inherited into pSrc's table; the class that DECLARED it names it, and` |
|        - | 3737 | `			 * the recursion below reaches that one in php's own link order. */` |
|      175 | 3738 | `			continue;` |
|        - | 3739 | `		}` |
|      120 | 3740 | `		if( GenStateIsHookName(&pSrcMeth->sFunc.sName) != bHooks ){` |
|       62 | 3741 | `			continue;` |
|        - | 3742 | `		}` |
|       91 | 3743 | `		pMeth = PH7_ClassExtractMethod(pClass,` |
|       29 | 3744 | `			SyStringData(&pSrcMeth->sFunc.sName),SyStringLength(&pSrcMeth->sFunc.sName));` |
|       62 | 3745 | `		if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       14 | 3746 | `			continue;` |
|        - | 3747 | `		}` |
|       50 | 3748 | `		if( GenStateAbstractHookSatisfied(pClass,&pMeth->sFunc.sName) ){` |
|      ! 0 | 3749 | `			continue;` |
|        - | 3750 | `		}` |
|       50 | 3751 | `		apSeen = (ph7_class_method **)SySetBasePtr(pOrder);` |
|       90 | 3752 | `		for( n = 0 ; n < SySetUsed(pOrder) ; ++n ){` |
|       44 | 3753 | `			if( apSeen[n] == pMeth ){` |
|      ! 0 | 3754 | `				bSeen = 1;` |
|      ! 0 | 3755 | `				break;` |
|        - | 3756 | `			}` |
|       24 | 3757 | `		}` |
|       50 | 3758 | `		if( !bSeen ){` |
|       50 | 3759 | `			SySetPut(pOrder,(const void *)&pMeth);` |
|       23 | 3760 | `		}` |
|       27 | 3761 | `	}` |
|      164 | 3762 | `	GenStateOrderAbstractsFrom(pClass,pSrc->pBase,pOrder,bHooks,iDepth + 1);` |
|      164 | 3763 | `	apSrc = (ph7_class **)SySetBasePtr(&pSrc->aTrait);` |
|      176 | 3764 | `	for( n = 0 ; n < SySetUsed(&pSrc->aTrait) ; ++n ){` |
|       14 | 3765 | `		GenStateOrderAbstractsFrom(pClass,apSrc[n],pOrder,bHooks,iDepth + 1);` |
|        8 | 3766 | `	}` |
|      164 | 3767 | `	apSrc = (ph7_class **)SySetBasePtr(&pSrc->aInterface);` |
|      196 | 3768 | `	for( n = 0 ; n < SySetUsed(&pSrc->aInterface) ; ++n ){` |
|       36 | 3769 | `		GenStateOrderAbstractsFrom(pClass,apSrc[n],pOrder,bHooks,iDepth + 1);` |
|       20 | 3770 | `	}` |
|      134 | 3771 | `}` |
|        - | 3772 | `/*` |
|        - | 3773 | ` * php names at most THREE of them and then writes ", ..."; the COUNT in the` |
|        - | 3774 | ` * sentence is still the whole number.` |
|        - | 3775 | ` */` |
|        - | 3776 | `#define GEN_ABSTRACT_LIST_MAX 3` |
|        - | 3777 | `/*` |
|        - | 3778 | ` * Does [pClass] leave an abstract method unimplemented? Answers the COUNT, and` |
|        - | 3779 | ` * on a non-zero one appends php's sentence to *pMsg (which the caller owns).` |
|        - | 3780 | ` *` |
|        - | 3781 | ` * php asks this question at two different MOMENTS and the answer has to be the` |
|        - | 3782 | ` * same one, so it is a single routine: at link time for a named class, and when` |
|        - | 3783 | `` * the `new` EXECUTES for an anonymous one -- whose declaration is an expression,`` |
|        - | 3784 | `` * so a `new` on a branch nothing takes is never asked at all.`` |
|        - | 3785 | ` */` |
|     5272 | 3786 | `PH7_PRIVATE sxu32 PH7_ClassAbstractGap(ph7_vm *pVm,ph7_class *pClass,SyBlob *pMsg)` |
|        5 | 3787 | `{` |
|        - | 3788 | `	ph7_class_method *pMeth;` |
|        - | 3789 | `	SyHashEntry *pEntry;` |
|        - | 3790 | `	sxu32 nAbstract;` |
|        - | 3791 | `	SyBlob sMsg;` |
|        - | 3792 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|     5277 | 3793 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      137 | 3794 | `		return 0;` |
|        - | 3795 | `	}` |
|     5145 | 3796 | `	if( pClass->iFlags & PH7_CLASS_LINT_UNBOUND ){` |
|        - | 3797 | `		/* A trait this lint could not see is exactly where the implementations` |
|        - | 3798 | ``		 * usually are (`class C implements ArrayAccess { use HasDataTrait; }`),`` |
|        - | 3799 | `		 * so the count would be of methods the class does have. */` |
|       16 | 3800 | `		return 0;` |
|        - | 3801 | `	}` |
|        - | 3802 | `	/* Count abstract methods */` |
|     5129 | 3803 | `	nAbstract = 0;` |
|     5129 | 3804 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|    22151 | 3805 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|    14465 | 3806 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|    14465 | 3807 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       57 | 3808 | `			if( GenStateAbstractHookSatisfied(pClass,&pMeth->sFunc.sName) ){` |
|        7 | 3809 | `				continue; /* hook requirement met by a plain property (php) */` |
|        - | 3810 | `			}` |
|       50 | 3811 | `			nAbstract++;` |
|       23 | 3812 | `		}` |
|        5 | 3813 | `	}` |
|     5129 | 3814 | `	if( nAbstract == 0 ){` |
|     5101 | 3815 | `		return 0;` |
|        - | 3816 | `	}` |
|        - | 3817 | `	/* Build the error message listing all abstract methods with origins */` |
|       32 | 3818 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       32 | 3819 | `	if( pClass->sDisp.nByte != pClass->sName.nByte ){` |
|        - | 3820 | `		/* An ANONYMOUS class gets php's shorter wording -- it cannot be "declared` |
|        - | 3821 | `		 * abstract", there being no declaration to put the word on. */` |
|        6 | 3822 | `		SyBlobFormat(&sMsg,"Class %z must implement %u abstract method%s (",` |
|        2 | 3823 | `			&pClass->sDisp,nAbstract,(nAbstract > 1 ? "s" : ""));` |
|        4 | 3824 | `	}else{` |
|       28 | 3825 | `		SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|        - | 3826 | `			"be declared abstract or implement the remaining method%s (",` |
|       12 | 3827 | `			&pClass->sDisp,nAbstract,` |
|       12 | 3828 | `			(nAbstract > 1 ? "s" : ""),` |
|       12 | 3829 | `			(nAbstract > 1 ? "s" : ""));` |
|        - | 3830 | `	}` |
|        - | 3831 | `	/* Second pass: list methods with origins, in php's table order and capped */` |
|        - | 3832 | `	{` |
|       32 | 3833 | `		sxu32 nListed = 0;` |
|        - | 3834 | `		sxu32 nOrder;` |
|        - | 3835 | `		SySet aOrder; /* ph7_class_method * , php's function-table order */` |
|        - | 3836 | `		ph7_class_method **apOrder;` |
|       32 | 3837 | `		SySetInit(&aOrder,&pVm->sAllocator,sizeof(ph7_class_method *));` |
|        - | 3838 | `		/* Methods first, then property hooks: php's two verification passes. */` |
|       32 | 3839 | `		GenStateOrderAbstractsFrom(pClass,pClass,&aOrder,0,0);` |
|       32 | 3840 | `		GenStateOrderAbstractsFrom(pClass,pClass,&aOrder,1,0);` |
|       32 | 3841 | `		apOrder = (ph7_class_method **)SySetBasePtr(&aOrder);` |
|       72 | 3842 | `		for( nOrder = 0 ; nOrder < SySetUsed(&aOrder) ; ++nOrder ){` |
|       46 | 3843 | `			ph7_class *pOrigin = 0;` |
|        - | 3844 | `			SyString *pMName;` |
|       46 | 3845 | `			pMeth = apOrder[nOrder];` |
|       46 | 3846 | `			if( nListed >= GEN_ABSTRACT_LIST_MAX ){` |
|        3 | 3847 | `				SyBlobAppend(&sMsg,", ...",sizeof(", ...")-1);` |
|        3 | 3848 | `				break;` |
|        - | 3849 | `			}` |
|       44 | 3850 | `			pMName = &pMeth->sFunc.sName;` |
|       44 | 3851 | `			if( nListed > 0 ){` |
|       16 | 3852 | `				SyBlobAppend(&sMsg,", ",2);` |
|        6 | 3853 | `			}` |
|        - | 3854 | `			/* Find the origin of this abstract method.` |
|        - | 3855 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|        - | 3856 | `			 * inheritance chains) take precedence for interface-declared` |
|        - | 3857 | `			 * methods. Abstract class methods only win when the class` |
|        - | 3858 | `			 * itself declared the abstract method (not inherited from` |
|        - | 3859 | `			 * an interface). Trait methods are adopted into the using` |
|        - | 3860 | `			 * class's namespace.` |
|        - | 3861 | `			 */` |
|        - | 3862 | `			{` |
|        - | 3863 | `				ph7_class **apIface;` |
|        - | 3864 | `				ph7_class **apTrait;` |
|        - | 3865 | `				ph7_class *pWalk;` |
|        - | 3866 | `				sxu32 i;` |
|        - | 3867 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|        - | 3868 | `				 * (one that was written in the class body, not inherited from an` |
|        - | 3869 | `				 * interface). PHP attributes origin to the declaring class.` |
|        - | 3870 | `				 */` |
|       44 | 3871 | `				if( pClass->pBase ){` |
|       33 | 3872 | `					pWalk = pClass->pBase;` |
|       77 | 3873 | `					while( pWalk ){` |
|        - | 3874 | `						ph7_class_method *pParentMeth;` |
|       47 | 3875 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|       47 | 3876 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|        - | 3877 | `							/* Exclude methods that came from an interface anywhere` |
|        - | 3878 | `							 * in this class's ancestor chain.` |
|        - | 3879 | `							 */` |
|       35 | 3880 | `							int fromIface = 0;` |
|       35 | 3881 | `							ph7_class *pAnc = pWalk;` |
|       69 | 3882 | `							while( pAnc ){` |
|        - | 3883 | `								ph7_class **apPI;` |
|        - | 3884 | `								sxu32 j;` |
|       45 | 3885 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|       45 | 3886 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|       10 | 3887 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|       10 | 3888 | `										fromIface = 1;` |
|       10 | 3889 | `										break;` |
|        - | 3890 | `									}` |
|      ! 0 | 3891 | `								}` |
|       45 | 3892 | `								if( fromIface ) break;` |
|       37 | 3893 | `								pAnc = pAnc->pBase;` |
|        3 | 3894 | `							}` |
|       35 | 3895 | `							if( !fromIface ){` |
|        - | 3896 | `								/* php attributes the origin to the class that DECLARED the` |
|        - | 3897 | `								 * method, so keep climbing: breaking at the first ancestor` |
|        - | 3898 | `								 * that HAS it named the nearest one, and` |
|        - | 3899 | `` 								 * `abstract class G { abstract ga } abstract class P extends G` `` |
|        - | 3900 | ``								 * reported `P::ga` where php reports `G::ga`. */`` |
|       27 | 3901 | `								pOrigin = pWalk;` |
|       12 | 3902 | `							}` |
|       16 | 3903 | `						}` |
|       47 | 3904 | `						pWalk = pWalk->pBase;` |
|        3 | 3905 | `					}` |
|       15 | 3906 | `				}` |
|        - | 3907 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|        - | 3908 | `				 * each interface's own parent chain for the deepest origin.` |
|        - | 3909 | `				 */` |
|       44 | 3910 | `				if( !pOrigin ){` |
|       24 | 3911 | `					pWalk = pClass;` |
|       60 | 3912 | `					while( pWalk && !pOrigin ){` |
|       40 | 3913 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|       42 | 3914 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|       20 | 3915 | `							ph7_class *pDeepest = GenStateIfaceDeclaringAt(apIface[i],pMName,0);` |
|       20 | 3916 | `							if( pDeepest ){` |
|       18 | 3917 | `								pOrigin = pDeepest;` |
|       18 | 3918 | `								break;` |
|        - | 3919 | `							}` |
|        2 | 3920 | `						}` |
|       40 | 3921 | `						pWalk = pWalk->pBase;` |
|        4 | 3922 | `					}` |
|       10 | 3923 | `				}` |
|        - | 3924 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|       44 | 3925 | `				if( !pOrigin ){` |
|        8 | 3926 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|        8 | 3927 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|        8 | 3928 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|        8 | 3929 | `							pOrigin = pClass;` |
|        8 | 3930 | `							break;` |
|        - | 3931 | `						}` |
|      ! 0 | 3932 | `					}` |
|        3 | 3933 | `				}` |
|        - | 3934 | `			}` |
|       44 | 3935 | `			if( pOrigin ){` |
|       44 | 3936 | `				SyBlobFormat(&sMsg,"%z::",&pOrigin->sDisp);` |
|       24 | 3937 | `			}else{` |
|        - | 3938 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|      ! 0 | 3939 | `				SyBlobFormat(&sMsg,"%z::",&pClass->sDisp);` |
|        - | 3940 | `			}` |
|       44 | 3941 | `			GenStateAppendAbstractMemberName(&sMsg,pMName);` |
|       44 | 3942 | `			nListed++;` |
|       24 | 3943 | `		}` |
|       32 | 3944 | `		SySetRelease(&aOrder);` |
|        - | 3945 | `	}` |
|       32 | 3946 | `	SyBlobAppend(&sMsg,")",1);` |
|       32 | 3947 | `	SyBlobAppend(pMsg,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       32 | 3948 | `	SyBlobRelease(&sMsg);` |
|       32 | 3949 | `	return nAbstract;` |
|     2641 | 3950 | `}` |
|        - | 3951 | `/*` |
|        - | 3952 | ` * The link-time half: php refuses a NAMED class that leaves an abstract method` |
|        - | 3953 | ` * unimplemented where the declaration stands, whether or not anything ever` |
|        - | 3954 | ` * instantiates it. An ANONYMOUS one is not asked here -- see PH7_VmNewAnonAbstractGap,` |
|        - | 3955 | ` * the OP_NEW half.` |
|        - | 3956 | ` */` |
|     5108 | 3957 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 | 3958 | `{` |
|        - | 3959 | `	SyBlob sMsg;` |
|        - | 3960 | `	sxi32 rc;` |
|     5113 | 3961 | `	if( pClass->iFlags & PH7_CLASS_ANON ){` |
|      183 | 3962 | `		return SXRET_OK;` |
|        - | 3963 | `	}` |
|     4935 | 3964 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|     4935 | 3965 | `	if( PH7_ClassAbstractGap(pGen->pVm,pClass,&sMsg) == 0 ){` |
|     4911 | 3966 | `		SyBlobRelease(&sMsg);` |
|     4911 | 3967 | `		return SXRET_OK;` |
|        - | 3968 | `	}` |
|       40 | 3969 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|       24 | 3970 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|       28 | 3971 | `	SyBlobRelease(&sMsg);` |
|       28 | 3972 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 3973 | `		return SXERR_ABORT;` |
|        - | 3974 | `	}` |
|       28 | 3975 | `	return SXRET_OK;` |
|     2559 | 3976 | `}` |
|        - | 3977 | `/*` |
|        - | 3978 | ` * Parse a class/interface name reference from the current token stream.` |
|        - | 3979 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|        - | 3980 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|        - | 3981 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|        - | 3982 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|        - | 3983 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|        - | 3984 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|        - | 3985 | ` */` |
|     9238 | 3986 | `PH7_PRIVATE sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|        5 | 3987 | `{` |
|     9243 | 3988 | `	int isAbsolute = 0;` |
|     9243 | 3989 | `	SyToken *pStart = pGen->pIn;` |
|        - | 3990 | `	SyBlob sName;` |
|     9243 | 3991 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|     1057 | 3992 | `		isAbsolute = 1;` |
|     1057 | 3993 | `		pGen->pIn++;` |
|      524 | 3994 | `	}` |
|     9243 | 3995 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|        - | 3996 | ``	/* `namespace\X` names the CURRENT namespace and is fully qualified from there. */`` |
|     9243 | 3997 | `	if( !isAbsolute && GenStateNsRelPrefix(pGen,&pGen->pIn,pGen->pEnd,&sName) ){` |
|       19 | 3998 | `		isAbsolute = 1;` |
|        9 | 3999 | `	}` |
|     9243 | 4000 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       11 | 4001 | `		SyBlobRelease(&sName);` |
|       11 | 4002 | `		pGen->pIn = pStart;` |
|       11 | 4003 | `		return SXERR_INVALID;` |
|        - | 4004 | `	}` |
|     9235 | 4005 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|     9235 | 4006 | `	pGen->pIn++;` |
|    14107 | 4007 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|     4882 | 4008 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      177 | 4009 | `		SyBlobAppend(&sName,"\\",1);` |
|      177 | 4010 | `		pGen->pIn++;` |
|      177 | 4011 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      177 | 4012 | `		pGen->pIn++;` |
|        5 | 4013 | `	}` |
|     9235 | 4014 | `	if( isAbsolute ){` |
|     1075 | 4015 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      538 | 4016 | `	}else{` |
|        - | 4017 | `		SyString sRaw;` |
|     8165 | 4018 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|     8165 | 4019 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|        - | 4020 | `	}` |
|     9235 | 4021 | `	SyBlobRelease(&sName);` |
|     9235 | 4022 | `	return SXRET_OK;` |
|     4620 | 4023 | `}` |
|        - | 4024 | `/*` |
|        - | 4025 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|        - | 4026 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|        - | 4027 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|        - | 4028 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|        - | 4029 | ` * either direction cannot run unbounded.` |
|        - | 4030 | ` */` |
|        - | 4031 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|      480 | 4032 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|        5 | 4033 | `{` |
|        - | 4034 | `	ph7_class **apParent;` |
|        - | 4035 | `	sxu32 n;` |
|     1073 | 4036 | `	while( pInterface ){` |
|      603 | 4037 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|      ! 0 | 4038 | `			return FALSE;` |
|        - | 4039 | `		}` |
|      631 | 4040 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|       56 | 4041 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|       14 | 4042 | `			return TRUE;` |
|        - | 4043 | `		}` |
|      593 | 4044 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|      609 | 4045 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|       18 | 4046 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|      ! 0 | 4047 | `				return TRUE;` |
|        - | 4048 | `			}` |
|       10 | 4049 | `		}` |
|      593 | 4050 | `		pInterface = pInterface->pBase;` |
|      593 | 4051 | `		iDepth++;` |
|        5 | 4052 | `	}` |
|      475 | 4053 | `	return FALSE;` |
|      245 | 4054 | `}` |
|      464 | 4055 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|        5 | 4056 | `{` |
|      469 | 4057 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|        5 | 4058 | `}` |
|        - | 4059 | `/*` |
|        - | 4060 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|        - | 4061 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|        - | 4062 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|        - | 4063 | ` */` |
|       10 | 4064 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|        4 | 4065 | `{` |
|       18 | 4066 | `	while( pBase ){` |
|       10 | 4067 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|        2 | 4068 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|        3 | 4069 | `			return TRUE;` |
|        - | 4070 | `		}` |
|       10 | 4071 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|        6 | 4072 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|        3 | 4073 | `			return TRUE;` |
|        - | 4074 | `		}` |
|        5 | 4075 | `		pBase = pBase->pBase;` |
|        1 | 4076 | `	}` |
|        9 | 4077 | `	return FALSE;` |
|        9 | 4078 | `}` |
|        - | 4079 | `/*` |
|        - | 4080 | `` * Compile a single `case NAME [= value];` member of an enum body (PHP 8.1).`` |
|        - | 4081 | ` * A case is stored as a class constant (PH7_CLASS_ATTR_CONSTANT\|ENUMCASE) whose` |
|        - | 4082 | ` * aByteCode holds the BACKING value expression for backed enums (empty for pure` |
|        - | 4083 | ` * enums). The case's runtime value — the singleton instance — is materialized` |
|        - | 4084 | ` * lazily on first access (VmEnumMaterialize, vm.c), matching PHP's lazy` |
|        - | 4085 | ` * backing-value type/duplicate checks. Declaration order is recorded in` |
|        - | 4086 | ` * pClass->aEnumCases for cases().` |
|        - | 4087 | ` */` |
|      170 | 4088 | `static sxi32 GenStateCompileEnumCase(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 | 4089 | `{` |
|      175 | 4090 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 4091 | `	SySet *pInstrContainer;` |
|        - | 4092 | `	ph7_class_attr *pCase;` |
|        - | 4093 | `	SyString *pName;` |
|        - | 4094 | `	sxi32 rc;` |
|      175 | 4095 | `	pGen->pIn++; /* Jump the 'case' keyword */` |
|      175 | 4096 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 4097 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 4098 | `			"Invalid enum case name inside enum '%z'",&pClass->sDisp);` |
|      ! 0 | 4099 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4100 | `			return SXERR_ABORT;` |
|        - | 4101 | `		}` |
|      ! 0 | 4102 | `		goto Synchronize;` |
|        - | 4103 | `	}` |
|      175 | 4104 | `	pName = &pGen->pIn->sData;` |
|        - | 4105 | `	/* Cases share the class-constant namespace (php: "Cannot redefine class constant") */` |
|      175 | 4106 | `	if( SyHashGet(&pClass->hConst,(const void *)pName->zString,pName->nByte) != 0 ){` |
|      ! 0 | 4107 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 4108 | `			"Cannot redefine class constant %z::%z",&pClass->sDisp,pName);` |
|      ! 0 | 4109 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4110 | `			return SXERR_ABORT;` |
|        - | 4111 | `		}` |
|      ! 0 | 4112 | `		goto Synchronize;` |
|        - | 4113 | `	}` |
|      175 | 4114 | `	pCase = PH7_NewClassAttr(pGen->pVm,pName,pGen->pIn->nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - | 4115 | `		PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|      175 | 4116 | `	if( pCase == 0 ){` |
|      ! 0 | 4117 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 4118 | `		return SXERR_ABORT;` |
|        - | 4119 | `	}` |
|      175 | 4120 | `	GenStateConsumeDoc(&(*pGen),&pCase->sDoc);` |
|      175 | 4121 | `	if( GenStateConsumeAttrs(&(*pGen),&pCase->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 4122 | `		return SXERR_ABORT;` |
|        - | 4123 | `	}` |
|      175 | 4124 | `	if( GenStateCheckAttrPlacement(&(*pGen),&pCase->aAttrs,16,16,0,0) == SXERR_ABORT ){` |
|      ! 0 | 4125 | `		return SXERR_ABORT;` |
|        - | 4126 | `	}` |
|      175 | 4127 | `	pGen->pIn++; /* Jump the case name */` |
|      175 | 4128 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) ){` |
|      121 | 4129 | `		if( pClass->nEnumBacking == 0 ){` |
|        8 | 4130 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        2 | 4131 | `				"Case %z of non-backed enum %z must not have a value",pName,&pClass->sDisp);` |
|        6 | 4132 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 4133 | `				return SXERR_ABORT;` |
|        - | 4134 | `			}` |
|        6 | 4135 | `			goto Synchronize;` |
|        - | 4136 | `		}` |
|      116 | 4137 | `		pGen->pIn++; /* Jump the equal sign */` |
|        - | 4138 | `		/* A backing value is a constant expression like any other: same rules, same` |
|        - | 4139 | ``		 * first-offender sentence, and no `new` (it is stored as a class constant). */`` |
|        - | 4140 | `		{` |
|      116 | 4141 | `			const char *zCErr = PH7_GenStateConstExprError(pGen,0);` |
|      116 | 4142 | `			if( zCErr ){` |
|        3 | 4143 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"%s",zCErr);` |
|        3 | 4144 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 4145 | `					return SXERR_ABORT;` |
|        - | 4146 | `				}` |
|        3 | 4147 | `				goto Synchronize;` |
|        - | 4148 | `			}` |
|        - | 4149 | `		}` |
|        - | 4150 | `		/* Compile the backing value expression into the case's own container` |
|        - | 4151 | `		 * (same technique as class constants). */` |
|      114 | 4152 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      114 | 4153 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pCase->aByteCode);` |
|      114 | 4154 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      114 | 4155 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 | 4156 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 4157 | `				"Empty value for enum case %z::%z",&pClass->sDisp,pName);` |
|      ! 0 | 4158 | `		}` |
|      114 | 4159 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      114 | 4160 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      114 | 4161 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4162 | `			return SXERR_ABORT;` |
|        - | 4163 | `		}` |
|       59 | 4164 | `	}else{` |
|       59 | 4165 | `		if( pClass->nEnumBacking != 0 ){` |
|      ! 0 | 4166 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 4167 | `				"Case %z of backed enum %z must have a value",pName,&pClass->sDisp);` |
|      ! 0 | 4168 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 4169 | `				return SXERR_ABORT;` |
|        - | 4170 | `			}` |
|      ! 0 | 4171 | `			goto Synchronize;` |
|        - | 4172 | `		}` |
|        - | 4173 | `	}` |
|      169 | 4174 | `	rc = PH7_ClassInstallAttr(pClass,pCase);` |
|      169 | 4175 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 4176 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 4177 | `		return SXERR_ABORT;` |
|        - | 4178 | `	}` |
|      169 | 4179 | `	SySetPut(&pClass->aEnumCases,(const void *)&pCase);` |
|      169 | 4180 | `	return SXRET_OK;` |
|        3 | 4181 | `Synchronize:` |
|        - | 4182 | `	/* Synchronize with the first semi-colon */` |
|       25 | 4183 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|       19 | 4184 | `		pGen->pIn++;` |
|        3 | 4185 | `	}` |
|        9 | 4186 | `	return SXERR_CORRUPT;` |
|       90 | 4187 | `}` |
|        - | 4188 | `/*` |
|        - | 4189 | ` * Install the enum interface methods (PHP 8.1): cases() for every enum, plus` |
|        - | 4190 | ` * from()/tryFrom() for backed ones. They are NATIVE methods — the very same C` |
|        - | 4191 | ` * bodies an enum declared from C gets — because php's are internal: it reports` |
|        - | 4192 | `` * them as `<internal, prototype BackedEnum>` with no file and no line, and`` |
|        - | 4193 | `` * declares `from(string\|int $value): static` on the prototype rather than the`` |
|        - | 4194 | ` * enum's own backing type.` |
|        - | 4195 | ` *` |
|        - | 4196 | ` * This used to synthesize PHP source forwarding to three global` |
|        - | 4197 | `` * `__phl_enum_*` thunks, which put those names in php's namespace and reported`` |
|        - | 4198 | `` * every enum's three methods as `<user>` at the enum's own line.`` |
|        - | 4199 | ` */` |
|      130 | 4200 | `static sxi32 GenStateCompileEnumMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 | 4201 | `{` |
|      135 | 4202 | `	if( PH7_InstallEnumInterfaceMethods(pGen->pVm,pClass) != SXRET_OK ){` |
|      ! 0 | 4203 | `		PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 4204 | `		return SXERR_ABORT;` |
|        - | 4205 | `	}` |
|      135 | 4206 | `	return SXRET_OK;` |
|       70 | 4207 | `}` |
|        - | 4208 | `/*` |
|        - | 4209 | ` * Magic methods an enum may not declare (php 8.1, zend_enum.c list —` |
|        - | 4210 | ` * __call/__callStatic/__invoke stay allowed).` |
|        - | 4211 | ` */` |
|        - | 4212 | `static const char *azEnumBannedMagic[] = {` |
|        - | 4213 | `	"__construct","__destruct","__clone","__get","__set","__isset","__unset",` |
|        - | 4214 | `	"__toString","__sleep","__wakeup","__serialize","__unserialize","__set_state"` |
|        - | 4215 | `};` |
|        - | 4216 | `/*` |
|        - | 4217 | ` * Enum post-body validation + synthesis: reject declared properties (including` |
|        - | 4218 | ``  * trait-imported ones) and banned magic methods, install the readonly `name` `` |
|        - | 4219 | `` * (and, for backed enums, `value`) instance properties the case singletons`` |
|        - | 4220 | ` * carry, and synthesize cases()/from()/tryFrom(). Runs after trait application` |
|        - | 4221 | ` * and before the class is installed.` |
|        - | 4222 | ` */` |
|      130 | 4223 | `static sxi32 GenStateEnumFinalize(ph7_gen_state *pGen,ph7_class *pClass,sxu32 nLine)` |
|        5 | 4224 | `{` |
|        - | 4225 | `	SyHashEntry *pEntry;` |
|        - | 4226 | `	sxi32 rc;` |
|        - | 4227 | `	sxu32 n;` |
|        - | 4228 | `	/* php: "Enum %s cannot include properties" */` |
|      135 | 4229 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|      135 | 4230 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        3 | 4231 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|        3 | 4232 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        3 | 4233 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine ? pAttr->nLine : nLine,` |
|        1 | 4234 | `				"Enum %z cannot include properties",&pClass->sDisp);` |
|        3 | 4235 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 4236 | `				return SXERR_ABORT;` |
|        - | 4237 | `			}` |
|        3 | 4238 | `			break;` |
|        - | 4239 | `		}` |
|      ! 0 | 4240 | `	}` |
|        - | 4241 | `	/* php: "Enum %s cannot include magic method %s" */` |
|     1825 | 4242 | `	for( n = 0 ; n < SX_ARRAYSIZE(azEnumBannedMagic) ; n++ ){` |
|     2535 | 4243 | `		if( SyHashGet(&pClass->hMethod,(const void *)azEnumBannedMagic[n],` |
|     1695 | 4244 | `			SyStrlen(azEnumBannedMagic[n])) != 0 ){` |
|      ! 0 | 4245 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 4246 | `				"Enum %z cannot include magic method %s",&pClass->sDisp,azEnumBannedMagic[n]);` |
|      ! 0 | 4247 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 4248 | `				return SXERR_ABORT;` |
|        - | 4249 | `			}` |
|      ! 0 | 4250 | `		}` |
|      850 | 4251 | `	}` |
|        - | 4252 | ``	/* Install the case-singleton instance properties: readonly `name` (every`` |
|        - | 4253 | ``	 * enum) and `value` (backed only). Materialization (vm.c) fills them and`` |
|        - | 4254 | `	 * clears the readonly write-once latch; user writes then raise php's` |
|        - | 4255 | `	 * "Cannot modify readonly property" through the normal store path. */` |
|        - | 4256 | `	{` |
|        - | 4257 | `		static const SyString sNameProp = { "name",sizeof("name")-1 };` |
|        - | 4258 | `		static const SyString sValueProp = { "value",sizeof("value")-1 };` |
|        - | 4259 | `		ph7_class_attr *pAttr;` |
|      135 | 4260 | `		pAttr = PH7_NewClassAttr(pGen->pVm,&sNameProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - | 4261 | `			PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      135 | 4262 | `		if( pAttr == 0 ){` |
|      ! 0 | 4263 | `			PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 4264 | `			return SXERR_ABORT;` |
|        - | 4265 | `		}` |
|      135 | 4266 | `		pAttr->nType = MEMOBJ_STRING;` |
|      135 | 4267 | `		SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|      135 | 4268 | `		PH7_ClassInstallAttr(pClass,pAttr);` |
|      135 | 4269 | `		if( pClass->nEnumBacking != 0 ){` |
|       70 | 4270 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&sValueProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - | 4271 | `				PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|       70 | 4272 | `			if( pAttr == 0 ){` |
|      ! 0 | 4273 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 4274 | `				return SXERR_ABORT;` |
|        - | 4275 | `			}` |
|       70 | 4276 | `			pAttr->nType = pClass->nEnumBacking;` |
|       70 | 4277 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|       27 | 4278 | `				SyStringInitFromBuf(&pAttr->sTypeName,"int",sizeof("int")-1);` |
|       15 | 4279 | `			}else{` |
|       46 | 4280 | `				SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|        - | 4281 | `			}` |
|       70 | 4282 | `			PH7_ClassInstallAttr(pClass,pAttr);` |
|       33 | 4283 | `		}` |
|        - | 4284 | `	}` |
|      135 | 4285 | `	return GenStateCompileEnumMethods(&(*pGen),pClass);` |
|       70 | 4286 | `}` |
|        - | 4287 | `/*` |
|        - | 4288 | ` * Deferred class declarations (class/anonymous-class extending an` |
|        - | 4289 | ` * autoloaded parent).` |
|        - | 4290 | ` *` |
|        - | 4291 | ` * A class declaration compiles INLINE while its enclosing file compiles, so a` |
|        - | 4292 | ` * parent/interface/trait that an autoloader would provide is unreachable when` |
|        - | 4293 | ` * the autoloader's own spl_autoload_register() statement has not EXECUTED yet` |
|        - | 4294 | `` * (same-file registration, or `new class extends \App\Child {}` anywhere).`` |
|        - | 4295 | ` * php's model has no such problem: a declaration with unresolved dependencies` |
|        - | 4296 | ` * is declared at its EXECUTION point, in statement order, not hoisted.` |
|        - | 4297 | ` *` |
|        - | 4298 | ` * These helpers reproduce that: before compiling a declaration, scan its` |
|        - | 4299 | `` * header (extends/implements) and body (depth-1 trait `use`) for referenced`` |
|        - | 4300 | ` * names and try to resolve each (firing autoload exactly where the normal` |
|        - | 4301 | ` * compile would). If any name is still missing, the WHOLE declaration is` |
|        - | 4302 | `` * captured as re-compilable source — a reconstructed `namespace`/`use`-import/`` |
|        - | 4303 | ` * doc/attribute/modifier prefix plus the declaration's raw text — recorded in` |
|        - | 4304 | ` * a VmDeferredClass, and OP_CLASS_DEFER is emitted at the declaration site.` |
|        - | 4305 | ` * At runtime (VmExecDeferredClass, vm_include.c) the autoloader is live: each` |
|        - | 4306 | `` * recorded name resolves or throws php's catchable `... not found` Error, and`` |
|        - | 4307 | ` * the chunk re-compiles through VmEvalChunk. An anonymous class re-compiles` |
|        - | 4308 | `` * inside `if (false) { new ... }` (installing the class without instantiating`` |
|        - | 4309 | ` * it) under its original synthesized name via pVm->sDeferAnonName; the site's` |
|        - | 4310 | ` * own OP_NEW then instantiates it with the site-compiled arguments.` |
|        - | 4311 | ` *` |
|        - | 4312 | ` * Behavior shifts only for declarations that previously died with the` |
|        - | 4313 | ` * compile-time "Nonexistent base class" fatal: they now follow php — succeed` |
|        - | 4314 | ` * when the autoloader is registered first, or throw php's catchable` |
|        - | 4315 | `` * `Class/Interface/Trait "X" not found` Error at the declaration point.`` |
|        - | 4316 | ` * A deferred declaration's OTHER compile errors (a body syntax error) shift` |
|        - | 4317 | ` * from file-compile time to the declaration's execution — still loud, timing` |
|        - | 4318 | ` * differs from php (recorded).` |
|        - | 4319 | ` */` |
|      504 | 4320 | `static void GenStateDeferEmitUses(SyBlob *pOut,SyHash *pTable,const char *zKind)` |
|        5 | 4321 | `{` |
|        - | 4322 | `	SyHashEntry *pEntry;` |
|      509 | 4323 | `	SyHashResetLoopCursor(pTable);` |
|      845 | 4324 | `	while( (pEntry = SyHashGetNextEntry(pTable)) != 0 ){` |
|       86 | 4325 | `		const char *zFqn = (const char *)pEntry->pUserData;` |
|       86 | 4326 | `		if( zFqn ){` |
|      128 | 4327 | `			SyBlobFormat(pOut,"use %s%s as %.*s;\n",zKind,zFqn,` |
|       84 | 4328 | `				(int)pEntry->nKeyLen,(const char *)pEntry->pKey);` |
|       42 | 4329 | `		}` |
|        2 | 4330 | `	}` |
|      509 | 4331 | `}` |
|        - | 4332 | `/*` |
|        - | 4333 | ` * Parse one class reference at *ppCur (bounded by pEnd) with the SAME` |
|        - | 4334 | ` * namespace/import resolution the real compile uses, and append it to pNames.` |
|        - | 4335 | ` * Advances *ppCur past the reference. Returns SXERR_INVALID on a malformed` |
|        - | 4336 | ` * reference (caller bails out of deferral and lets the normal path report).` |
|        - | 4337 | ` */` |
|     2088 | 4338 | `static sxi32 GenStateDeferRecordRef(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd,` |
|        - | 4339 | `	sxu8 cKind,SySet *pNames)` |
|        5 | 4340 | `{` |
|     2093 | 4341 | `	SyToken *pSavedIn = pGen->pIn;` |
|     2093 | 4342 | `	SyToken *pSavedEnd = pGen->pEnd;` |
|        - | 4343 | `	SyBlob sFqn;` |
|        - | 4344 | `	VmDeferredReq sReq;` |
|        - | 4345 | `	char *zDup;` |
|        - | 4346 | `	sxi32 rc;` |
|     2093 | 4347 | `	SyBlobInit(&sFqn,&pGen->pVm->sAllocator);` |
|     2093 | 4348 | `	pGen->pIn = *ppCur;` |
|     2093 | 4349 | `	pGen->pEnd = pEnd;` |
|     2093 | 4350 | `	rc = GenStateParseClassReference(pGen,&sFqn);` |
|     2093 | 4351 | `	*ppCur = pGen->pIn;` |
|     2093 | 4352 | `	pGen->pIn = pSavedIn;` |
|     2093 | 4353 | `	pGen->pEnd = pSavedEnd;` |
|     2093 | 4354 | `	if( rc != SXRET_OK \|\| SyBlobLength(&sFqn) < 1 ){` |
|        3 | 4355 | `		SyBlobRelease(&sFqn);` |
|        3 | 4356 | `		return SXERR_INVALID;` |
|        - | 4357 | `	}` |
|     3134 | 4358 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     2086 | 4359 | `		(const char *)SyBlobData(&sFqn),SyBlobLength(&sFqn));` |
|     2091 | 4360 | `	if( zDup == 0 ){` |
|      ! 0 | 4361 | `		SyBlobRelease(&sFqn);` |
|      ! 0 | 4362 | `		return SXERR_INVALID;` |
|        - | 4363 | `	}` |
|     2091 | 4364 | `	SyStringInitFromBuf(&sReq.sName,zDup,SyBlobLength(&sFqn));` |
|     2091 | 4365 | `	sReq.cKind = cKind;` |
|     2091 | 4366 | `	SySetPut(pNames,(const void *)&sReq);` |
|     2091 | 4367 | `	SyBlobRelease(&sFqn);` |
|     2091 | 4368 | `	return SXRET_OK;` |
|     1049 | 4369 | `}` |
|        - | 4370 | `/*` |
|        - | 4371 | ` * Look a class/interface/trait name up WITHOUT asking the autoloader: is this` |
|        - | 4372 | ` * name declared right now? php's early binding asks exactly this question --` |
|        - | 4373 | ` * zend_try_early_binding does a plain class-table lookup and gives up if the` |
|        - | 4374 | ` * parent is not there yet, because a compile-time autoload would run user code` |
|        - | 4375 | ` * in the middle of compiling a file.` |
|        - | 4376 | ` */` |
|       34 | 4377 | `static ph7_class * GenStateFindDeclaredClass(ph7_vm *pVm,const char *zName,sxu32 nByte)` |
|        3 | 4378 | `{` |
|        - | 4379 | `	SyHashEntry *pEntry;` |
|       37 | 4380 | `	PH7_VmClassNameAnchor(&zName,&nByte);` |
|       37 | 4381 | `	if( nByte < 1 ){` |
|      ! 0 | 4382 | `		return 0;` |
|        - | 4383 | `	}` |
|       37 | 4384 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       37 | 4385 | `	return pEntry ? (ph7_class *)pEntry->pUserData : 0;` |
|       20 | 4386 | `}` |
|        - | 4387 | `/*` |
|        - | 4388 | ` * Scan the declaration whose keyword pGen->pIn sits on (class/enum/interface/` |
|        - | 4389 | `` * trait, or an anonymous `class(args)`) WITHOUT consuming tokens. Collects`` |
|        - | 4390 | ` * every referenced dependency name, locates the body braces, and filters the` |
|        - | 4391 | ` * collected names down to the UNRESOLVABLE ones. SXRET_OK with an empty` |
|        - | 4392 | ` * pMissing set means "compile normally"; a non-empty set means "defer". Any` |
|        - | 4393 | ` * structural surprise returns SXERR_INVALID so the normal compile reports it.` |
|        - | 4394 | ` *` |
|        - | 4395 | ` * bNoAutoload picks which question the filter asks -- see its use below.` |
|        - | 4396 | ` */` |
|     5994 | 4397 | `static sxi32 GenStateScanDeferDeps(ph7_gen_state *pGen,int bAnon,int iSelfKind,` |
|        - | 4398 | `	SySet *pMissing,SyToken **ppBody,SyToken **ppBodyEnd,SyBlob *pSelfFqn,int bNoAutoload)` |
|        5 | 4399 | `{` |
|     5999 | 4400 | `	SyToken *pCur = pGen->pIn; /* on the declaration keyword */` |
|     5999 | 4401 | `	SyToken *pEnd = pGen->pEnd;` |
|        - | 4402 | `	SySet aNames;` |
|     5999 | 4403 | `	sxi32 rc = SXRET_OK;` |
|     5999 | 4404 | `	*ppBody = *ppBodyEnd = 0;` |
|     5999 | 4405 | `	SySetInit(&aNames,&pGen->pVm->sAllocator,sizeof(VmDeferredReq));` |
|     5999 | 4406 | `	pCur++; /* Jump the keyword */` |
|     5999 | 4407 | `	if( bAnon ){` |
|      191 | 4408 | `		if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|       75 | 4409 | `			SyToken *pClose = 0;` |
|       75 | 4410 | `			pCur++;` |
|       75 | 4411 | `			PH7_DelimitNestedTokens(pCur,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|       75 | 4412 | `			if( pClose == 0 \|\| pClose >= pEnd ){` |
|      ! 0 | 4413 | `				SySetRelease(&aNames);` |
|      ! 0 | 4414 | `				return SXERR_INVALID;` |
|        - | 4415 | `			}` |
|       75 | 4416 | `			pCur = &pClose[1];` |
|       35 | 4417 | `		}` |
|       98 | 4418 | `	}else{` |
|     5813 | 4419 | `		if( pCur >= pEnd \|\| !PH7_IsClassNameToken(pCur) ){` |
|        - | 4420 | `			/* Same name test the declaration compiler uses: a word php lets name a` |
|        - | 4421 | ``			 * class may be one of PHL's KEYWORD tokens (`class Integer …`), and`` |
|        - | 4422 | `			 * demanding a plain ID here sent such a declaration down the` |
|        - | 4423 | `			 * non-deferring path, where a not-yet-loaded parent is a fatal. */` |
|      ! 0 | 4424 | `			SySetRelease(&aNames);` |
|      ! 0 | 4425 | `			return SXERR_INVALID;` |
|        - | 4426 | `		}` |
|     5813 | 4427 | `		GenStateBuildFQN(pGen,&pCur->sData,pSelfFqn);` |
|     5813 | 4428 | `		pCur++;` |
|        - | 4429 | `	}` |
|        - | 4430 | ``	/* Header: extends/implements lists up to the '{' (an enum's `: int` backing`` |
|        - | 4431 | `	 * and any stray tokens pass through; malformed headers bail to the normal` |
|        - | 4432 | `	 * path's diagnostics). */` |
|     7749 | 4433 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_OCB) == 0 ){` |
|     1757 | 4434 | `		int iKind = -1;` |
|     1757 | 4435 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|     1683 | 4436 | `			sxi32 nKw = SX_PTR_TO_INT(pCur->pUserData);` |
|     1683 | 4437 | `			if( nKw == PH7_TKWRD_EXTENDS ){` |
|     1163 | 4438 | `				iKind = (iSelfKind == PH7_DEFER_KIND_INTERFACE)` |
|      579 | 4439 | `					? PH7_DEFER_KIND_INTERFACE : PH7_DEFER_KIND_CLASS;` |
|     1104 | 4440 | `			}else if( nKw == PH7_TKWRD_IMPLEMENTS ){` |
|      451 | 4441 | `				iKind = PH7_DEFER_KIND_INTERFACE;` |
|      223 | 4442 | `			}` |
|      839 | 4443 | `		}` |
|     1757 | 4444 | `		if( iKind < 0 ){` |
|      152 | 4445 | `			pCur++;` |
|      152 | 4446 | `			continue;` |
|        - | 4447 | `		}` |
|     1609 | 4448 | `		pCur++; /* Jump extends/implements */` |
|      802 | 4449 | `		for(;;){` |
|     1667 | 4450 | `			if( GenStateDeferRecordRef(pGen,&pCur,pEnd,(sxu8)iKind,&aNames) != SXRET_OK ){` |
|        3 | 4451 | `				SySetRelease(&aNames);` |
|        3 | 4452 | `				return SXERR_INVALID;` |
|        - | 4453 | `			}` |
|     1665 | 4454 | `			if( pCur < pEnd && (pCur->nType & PH7_TK_COMMA) ){` |
|       63 | 4455 | `				pCur++;` |
|       63 | 4456 | `				continue;` |
|        - | 4457 | `			}` |
|     1607 | 4458 | `			break;` |
|      ! 0 | 4459 | `		}` |
|        5 | 4460 | `	}` |
|     5997 | 4461 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_OCB) == 0 ){` |
|      ! 0 | 4462 | `		SySetRelease(&aNames);` |
|      ! 0 | 4463 | `		return SXERR_INVALID;` |
|        - | 4464 | `	}` |
|     5997 | 4465 | `	*ppBody = pCur;` |
|        - | 4466 | `	{` |
|     5997 | 4467 | `		SyToken *pClose = 0;` |
|     5997 | 4468 | `		PH7_DelimitNestedTokens(&pCur[1],pEnd,PH7_TK_OCB,PH7_TK_CCB,&pClose);` |
|     5997 | 4469 | `		if( pClose == 0 \|\| pClose >= pEnd ){` |
|      ! 0 | 4470 | `			SySetRelease(&aNames);` |
|      ! 0 | 4471 | `			return SXERR_INVALID;` |
|        - | 4472 | `		}` |
|     5997 | 4473 | `		*ppBodyEnd = pClose;` |
|        - | 4474 | `	}` |
|        - | 4475 | ``	/* Body: depth-1 trait `use Name[, Name]` statements. Statement position only`` |
|        - | 4476 | ``	 * (previous token one of '{' '}' ';'), so a closure's `use ($x)` — which`` |
|        - | 4477 | `	 * follows a ')' — never matches. */` |
|        - | 4478 | `	{` |
|     5997 | 4479 | `		SyToken *p = &(*ppBody)[1];` |
|     5997 | 4480 | `		int bStmtPos = 1;` |
|     5997 | 4481 | `		sxi32 iDepth = 1;` |
|   128243 | 4482 | `		while( p < *ppBodyEnd ){` |
|   122251 | 4483 | `			if( p->nType & PH7_TK_OCB ){` |
|     5625 | 4484 | `				iDepth++;` |
|     5625 | 4485 | `				bStmtPos = 1;` |
|     5625 | 4486 | `				p++;` |
|     5625 | 4487 | `				continue;` |
|        - | 4488 | `			}` |
|   116631 | 4489 | `			if( p->nType & PH7_TK_CCB ){` |
|     5625 | 4490 | `				iDepth--;` |
|     5625 | 4491 | `				bStmtPos = 1;` |
|     5625 | 4492 | `				p++;` |
|     5625 | 4493 | `				continue;` |
|        - | 4494 | `			}` |
|   111011 | 4495 | `			if( p->nType & PH7_TK_SEMI ){` |
|     9455 | 4496 | `				bStmtPos = 1;` |
|     9455 | 4497 | `				p++;` |
|     9455 | 4498 | `				continue;` |
|        - | 4499 | `			}` |
|   101556 | 4500 | `			if( iDepth == 1 && bStmtPos && (p->nType & PH7_TK_KEYWORD)` |
|     9009 | 4501 | `			 && SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_USE ){` |
|      377 | 4502 | `				p++;` |
|      186 | 4503 | `				for(;;){` |
|      431 | 4504 | `					if( GenStateDeferRecordRef(pGen,&p,*ppBodyEnd,PH7_DEFER_KIND_TRAIT,&aNames) != SXRET_OK ){` |
|      ! 0 | 4505 | `						SySetRelease(&aNames);` |
|      ! 0 | 4506 | `						return SXERR_INVALID;` |
|        - | 4507 | `					}` |
|      431 | 4508 | `					if( p < *ppBodyEnd && (p->nType & PH7_TK_COMMA) ){` |
|       59 | 4509 | `						p++;` |
|       59 | 4510 | `						continue;` |
|        - | 4511 | `					}` |
|      377 | 4512 | `					break;` |
|      ! 0 | 4513 | `				}` |
|      377 | 4514 | `				continue;` |
|        - | 4515 | `			}` |
|   101189 | 4516 | `			bStmtPos = 0;` |
|   101189 | 4517 | `			p++;` |
|        5 | 4518 | `		}` |
|        - | 4519 | `	}` |
|        - | 4520 | `	/* Filter: keep only the names that do NOT resolve. For a declaration that` |
|        - | 4521 | `	 * will be compiled where it stands, the lookup fires the autoloader exactly` |
|        - | 4522 | `	 * where the replaced compile would. For a CONDITIONAL one it must not: the` |
|        - | 4523 | `	 * declaration is deferred whatever this answers, and php never resolves a` |
|        - | 4524 | ``	 * parent it has not reached. `if (false) { class C extends B {} }` is the`` |
|        - | 4525 | `	 * shape that shows it -- nikic/php-parser's own class aliases are written` |
|        - | 4526 | ``	 * that way, with a `require` of the parent's file BEFORE the dead block, so`` |
|        - | 4527 | `	 * an autoload here loaded that file first and the require then declared` |
|        - | 4528 | `	 * everything in it a second time. */` |
|        - | 4529 | `	{` |
|        - | 4530 | `		/* php LINKS a declaration in one order and asks for each dependency as it` |
|        - | 4531 | `		 * links it: the parent, then the traits, then the interfaces -- so` |
|        - | 4532 | ``		 * `class C extends B implements I { use T; }` asks an autoloader for`` |
|        - | 4533 | ``		 * `B, T, I`, with the trait ahead of the interface though the `use` is`` |
|        - | 4534 | ``		 * written inside the body and the `implements` in the header. Reading the`` |
|        - | 4535 | ``		 * collected names in source order asked `B, I, T`, which is user-visible`` |
|        - | 4536 | `		 * the moment an autoloader has a side effect (a log line, a file read, a` |
|        - | 4537 | `		 * map lookup that fails differently) or when one dependency's loader` |
|        - | 4538 | `		 * declares another. The three kinds are walked in link order instead;` |
|        - | 4539 | `` 		 * within a kind the source order stands, and an interface's `extends` `` |
|        - | 4540 | `		 * list is recorded as interfaces, so it keeps its own written order. */` |
|        - | 4541 | `		static const sxu8 aLinkOrder[] = {` |
|        - | 4542 | `			PH7_DEFER_KIND_CLASS, PH7_DEFER_KIND_TRAIT, PH7_DEFER_KIND_INTERFACE` |
|        - | 4543 | `		};` |
|     5997 | 4544 | `		VmDeferredReq *aReq = (VmDeferredReq *)SySetBasePtr(&aNames);` |
|        - | 4545 | `		sxu32 n;` |
|        - | 4546 | `		int iPass;` |
|    23973 | 4547 | `		for( iPass = 0 ; iPass < (int)SX_ARRAYSIZE(aLinkOrder) ; ++iPass ){` |
|    24239 | 4548 | `			for( n = 0 ; n < SySetUsed(&aNames) ; ++n ){` |
|        - | 4549 | `				ph7_class *pFound;` |
|     6263 | 4550 | `				if( aReq[n].cKind != aLinkOrder[iPass] ){` |
|     4177 | 4551 | `					continue;` |
|        - | 4552 | `				}` |
|     2091 | 4553 | `				pFound = bNoAutoload` |
|       34 | 4554 | `					? GenStateFindDeclaredClass(pGen->pVm,aReq[n].sName.zString,aReq[n].sName.nByte)` |
|     2069 | 4555 | `					: PH7_VmExtractClass(pGen->pVm,aReq[n].sName.zString,aReq[n].sName.nByte,FALSE,0);` |
|     2091 | 4556 | `				if( pFound == 0 ){` |
|      106 | 4557 | `					SySetPut(pMissing,(const void *)&aReq[n]);` |
|       51 | 4558 | `				}` |
|     1048 | 4559 | `			}` |
|     8993 | 4560 | `		}` |
|        - | 4561 | `	}` |
|     5997 | 4562 | `	SySetRelease(&aNames);` |
|     5997 | 4563 | `	return rc;` |
|     3002 | 4564 | `}` |
|        - | 4565 | `/*` |
|        - | 4566 | ` * Capture the declaration as a re-compilable chunk, record it, and emit` |
|        - | 4567 | ` * OP_CLASS_DEFER at the current emission point. On return the statement` |
|        - | 4568 | ` * cursor sits past the declaration's closing '}'. pMissing's entries are` |
|        - | 4569 | ` * COPIED into the record (their name bytes are already allocator-owned).` |
|        - | 4570 | ` */` |
|      168 | 4571 | `static sxi32 GenStateEmitDeferredClass(ph7_gen_state *pGen,sxi32 iFlags,int bAnon,` |
|        - | 4572 | `	SySet *pMissing,SyToken *pBodyEnd,SyBlob *pSelfFqn,const SyString *pAnonName)` |
|        5 | 4573 | `{` |
|      173 | 4574 | `	SyToken *pKw = pGen->pIn; /* the declaration keyword */` |
|        - | 4575 | `	VmDeferredClass *pDefer;` |
|        - | 4576 | `	SyBlob sChunk;` |
|        - | 4577 | `	const char *zFrom;` |
|        - | 4578 | `	const char *zTo;` |
|        - | 4579 | `	char *zDup;` |
|      173 | 4580 | `	SyBlobInit(&sChunk,&pGen->pVm->sAllocator);` |
|        - | 4581 | `	/* The declaration site's compile context is replayed as literal statements:` |
|        - | 4582 | `	 * strict_types first (it must open the chunk), then namespace and the` |
|        - | 4583 | `	 * use-import tables — the runtime re-compile starts in a fresh scope. */` |
|      173 | 4584 | `	if( pGen->bStrictTypes ){` |
|      ! 0 | 4585 | `		SyBlobAppend(&sChunk,"declare(strict_types=1);\n",sizeof("declare(strict_types=1);\n")-1);` |
|      ! 0 | 4586 | `	}` |
|      173 | 4587 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       86 | 4588 | `		SyBlobFormat(&sChunk,"namespace %.*s;\n",` |
|       82 | 4589 | `			(int)SyBlobLength(&pGen->sNamespace),(const char *)SyBlobData(&pGen->sNamespace));` |
|       41 | 4590 | `	}` |
|      173 | 4591 | `	GenStateDeferEmitUses(&sChunk,&pGen->hUseImports,"");` |
|      173 | 4592 | `	GenStateDeferEmitUses(&sChunk,&pGen->hUseFuncImports,"function ");` |
|      173 | 4593 | `	GenStateDeferEmitUses(&sChunk,&pGen->hUseConstImports,"const ");` |
|        - | 4594 | `	/* Doc-comment and attribute groups precede the keyword in the raw source,` |
|        - | 4595 | `	 * outside the captured span — re-emit them from the trivia sidecar. */` |
|      173 | 4596 | `	if( !bAnon && pGen->sPendingDoc.nByte > 0 ){` |
|      ! 0 | 4597 | `		SyBlobAppend(&sChunk,pGen->sPendingDoc.zString,pGen->sPendingDoc.nByte);` |
|      ! 0 | 4598 | `		SyBlobAppend(&sChunk,"\n",1);` |
|      ! 0 | 4599 | `	}` |
|        - | 4600 | `	{` |
|        - | 4601 | `		ph7_trivia *aT;` |
|        - | 4602 | `		sxu32 nT,n;` |
|      173 | 4603 | `		if( bAnon ){` |
|        - | 4604 | ``			/* `new #[A] class` trivia is keyed to the 'class' token */`` |
|       10 | 4605 | `			SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|       10 | 4606 | `			aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|       10 | 4607 | `			nT = SySetUsed(&pGen->aTrivia);` |
|       10 | 4608 | `			if( pGen->pTokenSet && pKw >= pBase && pKw < &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|       10 | 4609 | `				sxu32 nIdx = (sxu32)(pKw - pBase);` |
|       10 | 4610 | `				for( n = 0 ; n < nT ; ++n ){` |
|      ! 0 | 4611 | `					if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|      ! 0 | 4612 | `						SyBlobFormat(&sChunk,"#[%.*s]\n",(int)aT[n].sText.nByte,aT[n].sText.zString);` |
|      ! 0 | 4613 | `					}` |
|      ! 0 | 4614 | `				}` |
|        4 | 4615 | `			}` |
|        6 | 4616 | `		}else{` |
|      165 | 4617 | `			aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|      165 | 4618 | `			nT = SySetUsed(&pGen->aPendingAttrs);` |
|      185 | 4619 | `			for( n = 0 ; n < nT ; ++n ){` |
|       21 | 4620 | `				if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|       21 | 4621 | `					SyBlobFormat(&sChunk,"#[%.*s]\n",(int)aT[n].sText.nByte,aT[n].sText.zString);` |
|       10 | 4622 | `				}` |
|       11 | 4623 | `			}` |
|        - | 4624 | `		}` |
|        - | 4625 | `	}` |
|        - | 4626 | `	/* Pad the prefix with newlines so the declaration keyword sits on its` |
|        - | 4627 | `	 * ORIGINAL line inside the chunk — runtime diagnostics from the deferred` |
|        - | 4628 | `	 * compile then report the source's real line. Best-effort: a prefix` |
|        - | 4629 | `	 * already longer than the declaration line skips the padding. */` |
|        - | 4630 | `	{` |
|      173 | 4631 | `		const char *zScan = (const char *)SyBlobData(&sChunk);` |
|      173 | 4632 | `		sxu32 nHave = 0;` |
|        - | 4633 | `		sxu32 nScan;` |
|     4755 | 4634 | `		for( nScan = 0 ; nScan < SyBlobLength(&sChunk) ; ++nScan ){` |
|     4586 | 4635 | `			if( zScan[nScan] == '\n' ){` |
|      190 | 4636 | `				nHave++;` |
|       93 | 4637 | `			}` |
|     2295 | 4638 | `		}` |
|     2089 | 4639 | `		while( nHave + 1 < pKw->nLine ){` |
|     1921 | 4640 | `			SyBlobAppend(&sChunk,"\n",1);` |
|     1921 | 4641 | `			nHave++;` |
|        5 | 4642 | `		}` |
|        - | 4643 | `	}` |
|      173 | 4644 | `	if( bAnon ){` |
|        - | 4645 | ``		/* `if (false) { new class <header-minus-args> { body } ; }` — installs`` |
|        - | 4646 | `		 * the class at the chunk's compile, never instantiates it. */` |
|       10 | 4647 | `		SyToken *pAfterArgs = &pKw[1];` |
|       10 | 4648 | `		SyBlobAppend(&sChunk,"if (false) { new ",sizeof("if (false) { new ")-1);` |
|       10 | 4649 | `		SyBlobAppend(&sChunk,pKw->sData.zString,pKw->sData.nByte);` |
|       10 | 4650 | `		if( pAfterArgs < pGen->pEnd && (pAfterArgs->nType & PH7_TK_LPAREN) ){` |
|        3 | 4651 | `			SyToken *pClose = 0;` |
|        3 | 4652 | `			PH7_DelimitNestedTokens(&pAfterArgs[1],pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|        3 | 4653 | `			if( pClose == 0 \|\| pClose >= pGen->pEnd ){` |
|      ! 0 | 4654 | `				SyBlobRelease(&sChunk);` |
|      ! 0 | 4655 | `				return SXERR_INVALID;` |
|        - | 4656 | `			}` |
|        3 | 4657 | `			pAfterArgs = &pClose[1];` |
|        1 | 4658 | `		}` |
|       10 | 4659 | `		if( pAfterArgs < pBodyEnd ){` |
|       10 | 4660 | `			SyBlobAppend(&sChunk," ",1);` |
|       10 | 4661 | `			zFrom = pAfterArgs->sData.zString;` |
|       10 | 4662 | `			zTo = pBodyEnd->sData.zString + pBodyEnd->sData.nByte;` |
|       10 | 4663 | `			SyBlobAppend(&sChunk,zFrom,(sxu32)(zTo - zFrom));` |
|        4 | 4664 | `		}` |
|       10 | 4665 | `		SyBlobAppend(&sChunk,"; }",sizeof("; }")-1);` |
|        6 | 4666 | `	}else{` |
|        - | 4667 | `		/* Modifiers were consumed before this compiler ran; reconstruct them` |
|        - | 4668 | ``		 * (an enum's implicit `final` must NOT be spelled out). */`` |
|      160 | 4669 | `		if( (iFlags & PH7_CLASS_ENUM) == 0` |
|      158 | 4670 | `		 && (pKw->nType & PH7_TK_KEYWORD)` |
|      161 | 4671 | `		 && SX_PTR_TO_INT(pKw->pUserData) == PH7_TKWRD_CLASS ){` |
|      131 | 4672 | `			if( iFlags & PH7_CLASS_FINAL ){` |
|        3 | 4673 | `				SyBlobAppend(&sChunk,"final ",sizeof("final ")-1);` |
|        1 | 4674 | `			}` |
|      131 | 4675 | `			if( iFlags & PH7_CLASS_ABSTRACT ){` |
|        3 | 4676 | `				SyBlobAppend(&sChunk,"abstract ",sizeof("abstract ")-1);` |
|        1 | 4677 | `			}` |
|      131 | 4678 | `			if( iFlags & PH7_CLASS_READONLY ){` |
|      ! 0 | 4679 | `				SyBlobAppend(&sChunk,"readonly ",sizeof("readonly ")-1);` |
|      ! 0 | 4680 | `			}` |
|       63 | 4681 | `		}` |
|      165 | 4682 | `		zFrom = pKw->sData.zString;` |
|      165 | 4683 | `		zTo = pBodyEnd->sData.zString + pBodyEnd->sData.nByte;` |
|      165 | 4684 | `		SyBlobAppend(&sChunk,zFrom,(sxu32)(zTo - zFrom));` |
|        - | 4685 | `	}` |
|      173 | 4686 | `	pDefer = (VmDeferredClass *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmDeferredClass));` |
|      173 | 4687 | `	if( pDefer == 0 ){` |
|      ! 0 | 4688 | `		SyBlobRelease(&sChunk);` |
|      ! 0 | 4689 | `		PH7_GenCompileError(pGen,E_ERROR,pKw->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 4690 | `		return SXERR_ABORT;` |
|        - | 4691 | `	}` |
|      173 | 4692 | `	SyZero(pDefer,sizeof(VmDeferredClass));` |
|      257 | 4693 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      168 | 4694 | `		(const char *)SyBlobData(&sChunk),SyBlobLength(&sChunk));` |
|      173 | 4695 | `	SyBlobRelease(&sChunk);` |
|      173 | 4696 | `	if( zDup == 0 ){` |
|      ! 0 | 4697 | `		PH7_GenCompileError(pGen,E_ERROR,pKw->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 4698 | `		return SXERR_ABORT;` |
|        - | 4699 | `	}` |
|      173 | 4700 | `	SyStringInitFromBuf(&pDefer->sText,zDup,SyStrlen(zDup));` |
|      173 | 4701 | `	if( bAnon ){` |
|       10 | 4702 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pAnonName->zString,pAnonName->nByte);` |
|       10 | 4703 | `		if( zDup == 0 ){` |
|      ! 0 | 4704 | `			PH7_GenCompileError(pGen,E_ERROR,pKw->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 4705 | `			return SXERR_ABORT;` |
|        - | 4706 | `		}` |
|       10 | 4707 | `		SyStringInitFromBuf(&pDefer->sAnonName,zDup,pAnonName->nByte);` |
|       10 | 4708 | `		pDefer->sSelfName = pDefer->sAnonName;` |
|        6 | 4709 | `	}else{` |
|      245 | 4710 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      160 | 4711 | `			(const char *)SyBlobData(pSelfFqn),SyBlobLength(pSelfFqn));` |
|      165 | 4712 | `		if( zDup == 0 ){` |
|      ! 0 | 4713 | `			PH7_GenCompileError(pGen,E_ERROR,pKw->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 4714 | `			return SXERR_ABORT;` |
|        - | 4715 | `		}` |
|      165 | 4716 | `		SyStringInitFromBuf(&pDefer->sSelfName,zDup,SyBlobLength(pSelfFqn));` |
|        - | 4717 | `	}` |
|      173 | 4718 | `	SySetInit(&pDefer->aRequired,&pGen->pVm->sAllocator,sizeof(VmDeferredReq));` |
|        - | 4719 | `	{` |
|      173 | 4720 | `		VmDeferredReq *aReq = (VmDeferredReq *)SySetBasePtr(pMissing);` |
|        - | 4721 | `		sxu32 n;` |
|      273 | 4722 | `		for( n = 0 ; n < SySetUsed(pMissing) ; ++n ){` |
|      104 | 4723 | `			SySetPut(&pDefer->aRequired,(const void *)&aReq[n]);` |
|       54 | 4724 | `		}` |
|        - | 4725 | `	}` |
|      173 | 4726 | `	pDefer->nLine = pKw->nLine;` |
|      173 | 4727 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLASS_DEFER,0,0,(void *)pDefer,0);` |
|        - | 4728 | `	/* Skip the declaration: the statement cursor lands past its '}' */` |
|      173 | 4729 | `	pGen->pIn = &pBodyEnd[1];` |
|      173 | 4730 | `	return SXRET_OK;` |
|       89 | 4731 | `}` |
|        - | 4732 | `/*` |
|        - | 4733 | ` * Deferral gate shared by the named-declaration compilers: scan the` |
|        - | 4734 | ` * declaration at pGen->pIn; when a dependency is missing, capture + emit the` |
|        - | 4735 | ` * deferred record and return TRUE (the caller returns immediately — the` |
|        - | 4736 | ` * declaration compiles at execution time). FALSE means compile normally.` |
|        - | 4737 | ` * *pRc carries SXERR_ABORT out of the capture path.` |
|        - | 4738 | ` */` |
|     5898 | 4739 | `static int GenStateMaybeDeferClass(ph7_gen_state *pGen,sxi32 iFlags,int iSelfKind,sxi32 *pRc)` |
|        5 | 4740 | `{` |
|        - | 4741 | `	SySet aMissing;` |
|     5903 | 4742 | `	SyToken *pBody = 0;` |
|     5903 | 4743 | `	SyToken *pBodyEnd = 0;` |
|        - | 4744 | `	SyBlob sSelfFqn;` |
|     5903 | 4745 | `	int bDefer = 0;` |
|        - | 4746 | `	/* php's binding rule, decided before anything is resolved: a declaration that` |
|        - | 4747 | `	 * is not at a unit's top level is bound when execution REACHES it, so it is` |
|        - | 4748 | `	 * deferred whatever its dependencies look like -- and nothing about it may be` |
|        - | 4749 | `	 * resolved here. */` |
|     5903 | 4750 | `	int bCond = GenStateDeclIsConditional(&(*pGen));` |
|     5903 | 4751 | `	*pRc = SXRET_OK;` |
|     5903 | 4752 | `	if( pGen->pVm->bSyntaxCheck ){` |
|        - | 4753 | ``		/* `phl -l`: the declaration is never EXECUTED, so there is nothing to`` |
|        - | 4754 | `		 * defer it to -- and deferring captures the body as raw text that no one` |
|        - | 4755 | ``		 * ever parses, which is how a file with `$x = ;` inside a class extending`` |
|        - | 4756 | `		 * an autoloaded base linted CLEAN. Nothing autoloads under -l either, so` |
|        - | 4757 | `		 * this is the common case rather than the rare one. php's own lint parses` |
|        - | 4758 | `		 * every body and binds nothing. */` |
|       92 | 4759 | `		return 0;` |
|        - | 4760 | `	}` |
|     5813 | 4761 | `	SySetInit(&aMissing,&pGen->pVm->sAllocator,sizeof(VmDeferredReq));` |
|     5813 | 4762 | `	SyBlobInit(&sSelfFqn,&pGen->pVm->sAllocator);` |
|     5808 | 4763 | `	if( GenStateScanDeferDeps(pGen,0,iSelfKind,&aMissing,&pBody,&pBodyEnd,&sSelfFqn,bCond) == SXRET_OK` |
|     5812 | 4764 | `	 && (SySetUsed(&aMissing) > 0 \|\| bCond) ){` |
|        - | 4765 | `		/* Two reasons to compile this declaration where it RUNS rather than here.` |
|        - | 4766 | `		 * The first is a missing dependency (the autoloader that resolves it has` |
|        - | 4767 | `		 * not been registered yet). The second is php's binding rule: a class` |
|        - | 4768 | ``		 * written inside an `if`, a loop or a function body is declared when`` |
|        - | 4769 | ``		 * execution reaches it, so `if (!class_exists('DateTime')) { class`` |
|        - | 4770 | ``		 * DateTime {} }` -- how symfony/polyfill-php8x ships its back-ports -- must`` |
|        - | 4771 | `		 * not REPLACE the engine's own class in a tree that has one. */` |
|      165 | 4772 | `		*pRc = GenStateEmitDeferredClass(pGen,iFlags,0,&aMissing,pBodyEnd,&sSelfFqn,0);` |
|      165 | 4773 | `		bDefer = 1;` |
|       80 | 4774 | `	}` |
|     5813 | 4775 | `	SySetRelease(&aMissing);` |
|     5813 | 4776 | `	SyBlobRelease(&sSelfFqn);` |
|     5813 | 4777 | `	return bDefer;` |
|     2954 | 4778 | `}` |
|        - | 4779 | `/*` |
|        - | 4780 | ``  * Apply a declaration body's collected `use Trait[, Trait] [{ resolution }]` `` |
|        - | 4781 | ` * entries to pClass — plain application when no resolution block is present,` |
|        - | 4782 | ` * otherwise the two-pass insteadof/as machinery. Shared by the CLASS body and` |
|        - | 4783 | ` * (since the adaptation-block port) the TRAIT body compiler. Returns the last` |
|        - | 4784 | ` * application status (non-OK = out of memory at a copy site).` |
|        - | 4785 | ` */` |
|     5460 | 4786 | `static sxi32 GenStateApplyTraitUses(ph7_gen_state *pGen,ph7_class *pClass,SySet *pUseEntries)` |
|        5 | 4787 | `{` |
|     5465 | 4788 | `	sxi32 rc = SXRET_OK;` |
|        - | 4789 | `	{` |
|        - | 4790 | `		TraitUseEntry *apUse;` |
|        - | 4791 | `		sxu32 nU;` |
|     5465 | 4792 | `		apUse = (TraitUseEntry *)SySetBasePtr(pUseEntries);` |
|     5805 | 4793 | `		for( nU = 0 ; nU < SySetUsed(pUseEntries) ; nU++ ){` |
|      345 | 4794 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      345 | 4795 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      345 | 4796 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      345 | 4797 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|        - | 4798 | `			sxu32 nT;` |
|      345 | 4799 | `			if( !hasResolution ){` |
|        - | 4800 | `				/* No conflict resolution block: use standard trait application */` |
|      631 | 4801 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      335 | 4802 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      335 | 4803 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 4804 | `						break;` |
|        - | 4805 | `					}` |
|      170 | 4806 | `				}` |
|      153 | 4807 | `			}else{` |
|        - | 4808 | `				/* With resolution block: copy attributes, record traits,` |
|        - | 4809 | `				 * then use the block to resolve method conflicts.` |
|        - | 4810 | `				 */` |
|        - | 4811 | `				SyToken *pR;` |
|      105 | 4812 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       61 | 4813 | `					ph7_class *pTR = apTrait[nT];` |
|        - | 4814 | `					ph7_class_attr *pAR;` |
|        - | 4815 | `					SyHashEntry *pER;` |
|        - | 4816 | `					SyString *pNR;` |
|       61 | 4817 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|       93 | 4818 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|        5 | 4819 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|        5 | 4820 | `						pNR = &pAR->sName;` |
|        5 | 4821 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|        5 | 4822 | `							SyHashInsertTail(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|        5 | 4823 | `							PH7_ClassNotePrivateName(pClass,pAR);` |
|        2 | 4824 | `						}` |
|        1 | 4825 | `					}` |
|        - | 4826 | `					/* Trait constants (PHP 8.2) live in the separate hConst namespace */` |
|       61 | 4827 | `					SyHashResetLoopCursor(&pTR->hConst);` |
|       89 | 4828 | `					while((pER = SyHashGetNextEntry(&pTR->hConst)) != 0 ){` |
|      ! 0 | 4829 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|      ! 0 | 4830 | `						pNR = &pAR->sName;` |
|      ! 0 | 4831 | `						if( SyHashGet(&pClass->hConst,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      ! 0 | 4832 | `							SyHashInsertTail(&pClass->hConst,(const void *)pNR->zString,pNR->nByte,pAR);` |
|      ! 0 | 4833 | `						}` |
|      ! 0 | 4834 | `					}` |
|       61 | 4835 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       33 | 4836 | `				}` |
|        - | 4837 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       49 | 4838 | `				pR = pUse->pResolvStart;` |
|      121 | 4839 | `				while( pR < pUse->pResolvEnd ){` |
|        - | 4840 | `					SyString sTrait,sMethod;` |
|        - | 4841 | `					ph7_class *pSrcTrait;` |
|        - | 4842 | `					ph7_class_method *pMeth;` |
|        - | 4843 | `					sxi32 nRKwrd;` |
|      193 | 4844 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      121 | 4845 | `					if( pR >= pUse->pResolvEnd ) break;` |
|       77 | 4846 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|       77 | 4847 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|       77 | 4848 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|       77 | 4849 | `					sMethod = pR->sData;` |
|       77 | 4850 | `					pR++;` |
|       77 | 4851 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       31 | 4852 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       31 | 4853 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       31 | 4854 | `							sTrait = sMethod;` |
|       31 | 4855 | `							pR++;` |
|       31 | 4856 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       31 | 4857 | `							sMethod = pR->sData;` |
|       31 | 4858 | `							pR++;` |
|       14 | 4859 | `						}` |
|       14 | 4860 | `					}` |
|       77 | 4861 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 4862 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|      ! 0 | 4863 | `						continue;` |
|        - | 4864 | `					}` |
|       77 | 4865 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|       77 | 4866 | `					pR++;` |
|       77 | 4867 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       17 | 4868 | `						pSrcTrait = 0;` |
|       21 | 4869 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       21 | 4870 | `							SyString *pTN = &apTrait[nT]->sName;` |
|       30 | 4871 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       18 | 4872 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       17 | 4873 | `								pSrcTrait = apTrait[nT];` |
|       17 | 4874 | `								break;` |
|        - | 4875 | `							}` |
|        4 | 4876 | `						}` |
|       17 | 4877 | `						if( pSrcTrait ){` |
|       17 | 4878 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       17 | 4879 | `							if( pMeth ){` |
|       17 | 4880 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       17 | 4881 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       17 | 4882 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|        7 | 4883 | `								}` |
|        7 | 4884 | `							}` |
|        7 | 4885 | `						}` |
|        7 | 4886 | `					}` |
|      169 | 4887 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|        5 | 4888 | `				}` |
|        - | 4889 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      105 | 4890 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        - | 4891 | `					ph7_class_method *pMR;` |
|        - | 4892 | `					SyHashEntry *pER;` |
|        - | 4893 | `					SyString *pNR;` |
|       61 | 4894 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      187 | 4895 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      103 | 4896 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      103 | 4897 | `						pNR = &pMR->sFunc.sName;` |
|      103 | 4898 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|       75 | 4899 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       35 | 4900 | `						}` |
|        5 | 4901 | `					}` |
|       33 | 4902 | `				}` |
|        - | 4903 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       49 | 4904 | `				pR = pUse->pResolvStart;` |
|      121 | 4905 | `				while( pR < pUse->pResolvEnd ){` |
|        - | 4906 | `					SyString sTrait,sMethod,sAlias;` |
|        - | 4907 | `					ph7_class *pSrcTrait;` |
|        - | 4908 | `					ph7_class_method *pMeth;` |
|      121 | 4909 | `					int hasQual = 0;` |
|        - | 4910 | `					sxi32 nRKwrd;` |
|      193 | 4911 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      121 | 4912 | `					if( pR >= pUse->pResolvEnd ) break;` |
|       77 | 4913 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|       77 | 4914 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|       77 | 4915 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|       77 | 4916 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|       77 | 4917 | `					sMethod = pR->sData;` |
|       77 | 4918 | `					pR++;` |
|       77 | 4919 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       31 | 4920 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       31 | 4921 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       31 | 4922 | `							sTrait = sMethod;` |
|       31 | 4923 | `							hasQual = 1;` |
|       31 | 4924 | `							pR++;` |
|       31 | 4925 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       31 | 4926 | `							sMethod = pR->sData;` |
|       31 | 4927 | `							pR++;` |
|       14 | 4928 | `						}` |
|       14 | 4929 | `					}` |
|       77 | 4930 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 4931 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|      ! 0 | 4932 | `						continue;` |
|        - | 4933 | `					}` |
|       77 | 4934 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|       77 | 4935 | `					pR++;` |
|       77 | 4936 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       63 | 4937 | `						sxi32 iNewVis = -1;` |
|       63 | 4938 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       26 | 4939 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       26 | 4940 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       26 | 4941 | `								iNewVis = nAK;` |
|       26 | 4942 | `								pR++;` |
|       11 | 4943 | `							}` |
|       11 | 4944 | `						}` |
|       63 | 4945 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       60 | 4946 | `							sAlias = pR->sData;` |
|       60 | 4947 | `							pR++;` |
|       28 | 4948 | `						}` |
|       63 | 4949 | `						pMeth = 0;` |
|       63 | 4950 | `						if( hasQual ){` |
|       17 | 4951 | `							pSrcTrait = 0;` |
|       27 | 4952 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       27 | 4953 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       39 | 4954 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       24 | 4955 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       17 | 4956 | `									pSrcTrait = apTrait[nT];` |
|       17 | 4957 | `									break;` |
|        - | 4958 | `								}` |
|        8 | 4959 | `							}` |
|       17 | 4960 | `							if( pSrcTrait ){` |
|       17 | 4961 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|        7 | 4962 | `							}` |
|       10 | 4963 | `						}else{` |
|       49 | 4964 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|        - | 4965 | `						}` |
|       63 | 4966 | `						if( pMeth ){` |
|        - | 4967 | `							/* php: a method declared in the class BODY wins over a trait alias` |
|        - | 4968 | ``							 * of the same name (e.g. an explicit __construct over `init as`` |
|        - | 4969 | ``							 * __construct`). If pClass already declares sAlias ITSELF — an own`` |
|        - | 4970 | `							 * method, sFunc.pUserData == pClass — keep it: SyHashInsert is LIFO,` |
|        - | 4971 | `							 * so an unconditional insert would shadow the class method at lookup` |
|        - | 4972 | ``							 * and `new` would run the alias. A name held only by another trait is`` |
|        - | 4973 | `							 * a genuine conflict resolved by the insteadof pass above. */` |
|       63 | 4974 | `							int bClassWins = 0;` |
|       63 | 4975 | `							if( sAlias.nByte > 0 ){` |
|       60 | 4976 | `								ph7_class_method *pOwn = PH7_ClassExtractMethod(pClass,sAlias.zString,sAlias.nByte);` |
|       60 | 4977 | `								bClassWins = (pOwn && pOwn->sFunc.pUserData == pClass);` |
|       28 | 4978 | `							}` |
|       88 | 4979 | `							if( sAlias.nByte > 0 && !bClassWins ){` |
|        - | 4980 | `								/* Create a shallow copy of the method struct for the alias` |
|        - | 4981 | `								 * so it can carry its own visibility without affecting the original.` |
|        - | 4982 | `								 */` |
|        - | 4983 | `								ph7_class_method *pAlias;` |
|        - | 4984 | `								char *zAliasDup;` |
|       54 | 4985 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       54 | 4986 | `								if( pAlias ){` |
|       54 | 4987 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       54 | 4988 | `									if( iNewVis >= 0 ){` |
|       23 | 4989 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       19 | 4990 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|        7 | 4991 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       10 | 4992 | `									}` |
|       54 | 4993 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       54 | 4994 | `									if( zAliasDup ){` |
|       54 | 4995 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       25 | 4996 | `									}` |
|       29 | 4997 | `								}` |
|       35 | 4998 | `							}else if( sAlias.nByte == 0 && iNewVis >= 0 ){` |
|        - | 4999 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|        - | 5000 | `								ph7_class_method *pCopy;` |
|        3 | 5001 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|        3 | 5002 | `								if( pCopy ){` |
|        3 | 5003 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|        3 | 5004 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|        3 | 5005 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|        3 | 5006 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|      ! 0 | 5007 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|        - | 5008 | `									/* Replace the method in the class hash */` |
|        3 | 5009 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|        3 | 5010 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|        1 | 5011 | `								}` |
|        1 | 5012 | `							}` |
|       29 | 5013 | `						}` |
|       29 | 5014 | `						SXUNUSED(hasQual);` |
|       29 | 5015 | `					}` |
|       91 | 5016 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|        5 | 5017 | `				}` |
|        - | 5018 | `			}` |
|      345 | 5019 | `			SySetRelease(&pUse->aTraits);` |
|      175 | 5020 | `		}` |
|        - | 5021 | `	}` |
|     5465 | 5022 | `	return rc;` |
|        5 | 5023 | `}` |
|        - | 5024 | `/*` |
|        - | 5025 | ` * Compile a class declaration, named or anonymous.` |
|        - | 5026 | ` *` |
|        - | 5027 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|        - | 5028 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|        - | 5029 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|        - | 5030 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|        - | 5031 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|        - | 5032 | ` * implements, body, install) is shared by both paths.` |
|        - | 5033 | ` */` |
|     5354 | 5034 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|        - | 5035 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|        5 | 5036 | `{` |
|     5359 | 5037 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 5038 | `	ph7_class *pClass,*pBase;` |
|     5359 | 5039 | `	ph7_class *pSavedCurClass = pGen->pCurClass; /* restored at 'done' (enclosing class for a nested anon) */` |
|     5359 | 5040 | `	ph7_class *pSavedCurBase = pGen->pCurBase;   /* ...and its base, see pCurBase */` |
|        - | 5041 | `	SyToken *pEnd,*pTmp;` |
|        - | 5042 | `	SySet aInterfaces;` |
|        - | 5043 | `	SySet aUseEntries;` |
|        - | 5044 | `	SySet aOvMeth,aOvProp;   /* the #[\Override] claims this class DECLARED */` |
|     5359 | 5045 | `	sxu32 nErrEntry = pGen->nErr; /* errors already reported when this class started */` |
|        - | 5046 | `	SyString *pName;` |
|        - | 5047 | `	sxi32 nKwrd;` |
|        - | 5048 | `	sxi32 rc;` |
|     5359 | 5049 | `	if( pAnonName == 0 ){` |
|        - | 5050 | `		/* Deferral gate: an unresolvable parent/interface/trait —` |
|        - | 5051 | `		 * its autoloader has not RUN yet — re-compiles this declaration at its` |
|        - | 5052 | `		 * execution point instead of dying on "Nonexistent base class". */` |
|        - | 5053 | `		sxi32 rcDefer;` |
|     5181 | 5054 | `		if( GenStateMaybeDeferClass(pGen,iFlags,PH7_DEFER_KIND_CLASS,&rcDefer) ){` |
|      135 | 5055 | `			return rcDefer == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 5056 | `		}` |
|     2523 | 5057 | `	}` |
|        - | 5058 | `	/* Jump the 'class' keyword */` |
|     5229 | 5059 | `	pGen->pIn++;` |
|     5229 | 5060 | `	if( pAnonName ){` |
|        - | 5061 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|        - | 5062 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|        - | 5063 | `		 * then use the synthesized name. */` |
|      183 | 5064 | `		*ppArgStart = *ppArgEnd = 0;` |
|      183 | 5065 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       73 | 5066 | `			pGen->pIn++; /* Jump '(' */` |
|       73 | 5067 | `			*ppArgStart = pGen->pIn;` |
|      107 | 5068 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|       34 | 5069 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|       73 | 5070 | `			pGen->pIn = *ppArgEnd;` |
|       73 | 5071 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|       34 | 5072 | `		}` |
|      183 | 5073 | `		pName = pAnonName;` |
|      183 | 5074 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|       94 | 5075 | `	}else{` |
|     5051 | 5076 | `		if( pGen->pIn >= pGen->pEnd \|\| !PH7_IsClassNameToken(pGen->pIn) ){` |
|        - | 5077 | `			/* Syntax error */` |
|      ! 0 | 5078 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|      ! 0 | 5079 | `			if( rc == SXERR_ABORT ){` |
|        - | 5080 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 5081 | `				return SXERR_ABORT;` |
|        - | 5082 | `			}` |
|        - | 5083 | `			/* Synchronize with the first semi-colon or curly braces */` |
|      ! 0 | 5084 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|      ! 0 | 5085 | `				pGen->pIn++;` |
|      ! 0 | 5086 | `			}` |
|      ! 0 | 5087 | `			return SXRET_OK;` |
|        - | 5088 | `		}` |
|        - | 5089 | `		/* Extract class name */` |
|     5051 | 5090 | `		pName = &pGen->pIn->sData;` |
|     5051 | 5091 | `		if( PH7_IsReservedClassName(pName) ){` |
|        - | 5092 | `			/* php's compiler-side screen (zend_is_reserved_class_name): the scanner` |
|        - | 5093 | `			 * hands these over as identifiers and the compiler refuses them, quoting` |
|        - | 5094 | ``			 * the name as WRITTEN. `class Null {}` and `class void {}` used to`` |
|        - | 5095 | `			 * compile here and are fatals in php. */` |
|        4 | 5096 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        1 | 5097 | `				"Cannot use \"%z\" as a class name as it is reserved",pName);` |
|        3 | 5098 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 5099 | `				return SXERR_ABORT;` |
|        - | 5100 | `			}` |
|        4 | 5101 | `			while( pGen->pIn < pGen->pEnd` |
|        5 | 5102 | `			    && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|        3 | 5103 | `				pGen->pIn++;` |
|        1 | 5104 | `			}` |
|        3 | 5105 | `			return SXRET_OK;` |
|        - | 5106 | `		}` |
|        - | 5107 | `		/* Advance the stream cursor */` |
|     5049 | 5108 | `		pGen->pIn++;` |
|        - | 5109 | `		/* Build FQN and obtain a raw class */ {` |
|        - | 5110 | `			SyBlob sFQN;` |
|        - | 5111 | `			SyString sFQNStr;` |
|     5049 | 5112 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     5049 | 5113 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|     5049 | 5114 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|        - | 5115 | ``			/* php refuses a declaration whose short name a local `use` already took. */`` |
|     5049 | 5116 | `			if( GenStateGuardImportRedeclare(pGen,0,pName,&sFQNStr,nLine) == SXERR_ABORT ){` |
|      ! 0 | 5117 | `				SyBlobRelease(&sFQN);` |
|      ! 0 | 5118 | `				return SXERR_ABORT;` |
|        - | 5119 | `			}` |
|     5049 | 5120 | `			GenStateRecordDeclaredName(pGen,0,&sFQNStr);` |
|     5049 | 5121 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|     5049 | 5122 | `			SyBlobRelease(&sFQN);` |
|        - | 5123 | `		}` |
|        - | 5124 | `	}` |
|     5227 | 5125 | `	if( pClass == 0 ){` |
|      ! 0 | 5126 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 5127 | `		return SXERR_ABORT;` |
|        - | 5128 | `	}` |
|     5222 | 5129 | `	if( (iFlags & PH7_CLASS_ENUM) && pGen->pIn < pGen->pEnd` |
|      141 | 5130 | `		&& (pGen->pIn->nType & PH7_TK_COLON /* ':' */) ){` |
|        - | 5131 | ``		/* Backed enum: `enum Name: int\|string` (PHP 8.1) */`` |
|       74 | 5132 | `		pGen->pIn++; /* Jump ':' */` |
|       70 | 5133 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       74 | 5134 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_INT ){` |
|       29 | 5135 | `			pClass->nEnumBacking = MEMOBJ_INT;` |
|       29 | 5136 | `			pGen->pIn++;` |
|       60 | 5137 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       48 | 5138 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STRING ){` |
|       46 | 5139 | `			pClass->nEnumBacking = MEMOBJ_STRING;` |
|       46 | 5140 | `			pGen->pIn++;` |
|       25 | 5141 | `		}else{` |
|        3 | 5142 | `			SyToken *pTok = pGen->pIn;` |
|        3 | 5143 | `			if( pTok >= pGen->pEnd ){ pTok--; }` |
|        4 | 5144 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pTok->nLine,` |
|        1 | 5145 | `				"Enum backing type must be int or string, %z given",&pTok->sData);` |
|        3 | 5146 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 5147 | `				return SXERR_ABORT;` |
|        - | 5148 | `			}` |
|        3 | 5149 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|        3 | 5150 | `				pGen->pIn++; /* Skip the bogus type token */` |
|        1 | 5151 | `			}` |
|        - | 5152 | `		}` |
|       35 | 5153 | `	}` |
|     5227 | 5154 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|     5227 | 5155 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 5156 | `		return SXERR_ABORT;` |
|        - | 5157 | `	}` |
|        - | 5158 | `	/* implemented interfaces and per-use-statement trait containers */` |
|     5227 | 5159 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|     5227 | 5160 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|        - | 5161 | `	/* Assume a standalone class */` |
|     5227 | 5162 | `	pBase = 0;` |
|     5227 | 5163 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     1471 | 5164 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     1471 | 5165 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|        - | 5166 | `			SyBlob sResolved;` |
|        - | 5167 | `			SyString sBaseName;` |
|        - | 5168 | `			sxu32 nRefLine;` |
|     1089 | 5169 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|        - | 5170 | `				/* php parse-fatals here (enums have no inheritance) */` |
|      ! 0 | 5171 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 5172 | `					"Enum %z cannot extend a class",&pClass->sDisp);` |
|      ! 0 | 5173 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 5174 | `					return SXERR_ABORT;` |
|        - | 5175 | `				}` |
|      ! 0 | 5176 | `			}` |
|     1089 | 5177 | `			pGen->pIn++; /* Advance past 'extends' */` |
|     1089 | 5178 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|     1089 | 5179 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     1089 | 5180 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|        3 | 5181 | `				SyBlobRelease(&sResolved);` |
|        4 | 5182 | `				rc = PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        - | 5183 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|        1 | 5184 | `					pName);` |
|        3 | 5185 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|        3 | 5186 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 5187 | `					return SXERR_ABORT;` |
|        - | 5188 | `				}` |
|        3 | 5189 | `				return SXRET_OK;` |
|        - | 5190 | `			}` |
|     1628 | 5191 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|     1082 | 5192 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     1087 | 5193 | `			SyStringInitFromBuf(&sBaseName,` |
|        - | 5194 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - | 5195 | `			/* Interfaces are not allowed */` |
|     1087 | 5196 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|      ! 0 | 5197 | `				pBase = pBase->pNextName;` |
|      ! 0 | 5198 | `			}` |
|     1087 | 5199 | `			if( pBase == 0 ){` |
|       15 | 5200 | `				if( !GenStateLintSkipsBase(pGen,pClass) ){` |
|      ! 0 | 5201 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - | 5202 | `						"Nonexistent base class '%z'",&sBaseName);` |
|      ! 0 | 5203 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 5204 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 5205 | `						return SXERR_ABORT;` |
|        - | 5206 | `					}` |
|      ! 0 | 5207 | `				}` |
|        8 | 5208 | `			}else{` |
|     1073 | 5209 | `				if( pBase->iFlags & PH7_CLASS_ENUM ){` |
|        4 | 5210 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 5211 | `						"Class %z cannot extend enum %z",pName,&pBase->sDisp);` |
|        3 | 5212 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 5213 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 5214 | `						return SXERR_ABORT;` |
|        - | 5215 | `					}` |
|        3 | 5216 | `					pBase = 0; /* Never inherit from an enum */` |
|     1072 | 5217 | `				}else if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|        8 | 5218 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 5219 | `						/* php's wording, unquoted: "Class B cannot extend final class A". */` |
|        2 | 5220 | `						"Class %z cannot extend final class %z",pName,&pBase->sDisp);` |
|        6 | 5221 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 5222 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 5223 | `						return SXERR_ABORT;` |
|        - | 5224 | `					}` |
|        2 | 5225 | `				}` |
|        - | 5226 | `			}` |
|     1087 | 5227 | `			SyBlobRelease(&sResolved);` |
|     1087 | 5228 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|      ! 0 | 5229 | `				pBase = 0; /* Error already reported: enums have no base class */` |
|      ! 0 | 5230 | `			}` |
|      541 | 5231 | `		}` |
|     1469 | 5232 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|        - | 5233 | `			ph7_class *pInterface;` |
|        - | 5234 | `			/* Interface implementation */` |
|      435 | 5235 | `			pGen->pIn++; /* Advance the stream cursor */` |
|      251 | 5236 | `			for(;;){` |
|        - | 5237 | `				SyBlob sResolved;` |
|        - | 5238 | `				SyString sIntName;` |
|        - | 5239 | `				sxu32 nRefLine;` |
|      471 | 5240 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|      471 | 5241 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      471 | 5242 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 5243 | `					SyBlobRelease(&sResolved);` |
|      ! 0 | 5244 | `					rc = PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        - | 5245 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|      ! 0 | 5246 | `						pName);` |
|      ! 0 | 5247 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 5248 | `						return SXERR_ABORT;` |
|        - | 5249 | `					}` |
|      ! 0 | 5250 | `					break;` |
|        - | 5251 | `				}` |
|      937 | 5252 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|      466 | 5253 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      471 | 5254 | `				SyStringInitFromBuf(&sIntName,` |
|        - | 5255 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - | 5256 | `				/* Only interfaces are allowed */` |
|      471 | 5257 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 5258 | `					pInterface = pInterface->pNextName;` |
|      ! 0 | 5259 | `				}` |
|      471 | 5260 | `				if( pInterface == 0 ){` |
|        2 | 5261 | `					if( !GenStateLintSkipsBase(pGen,pClass) ){` |
|      ! 0 | 5262 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - | 5263 | `							"Nonexistent base interface '%z'",&sIntName);` |
|      ! 0 | 5264 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 5265 | `							SyBlobRelease(&sResolved);` |
|      ! 0 | 5266 | `							return SXERR_ABORT;` |
|        - | 5267 | `						}` |
|      ! 0 | 5268 | `					}` |
|        1 | 5269 | `				}else{` |
|        - | 5270 | `					/* Reject user classes that try to implement Throwable` |
|        - | 5271 | `					 * directly (or via an interface that extends Throwable)` |
|        - | 5272 | `					 * unless they already extend Exception or Error.` |
|        - | 5273 | `					 * Exception and Error themselves are compiled from the` |
|        - | 5274 | `					 * built-in library and are exempt by FQN — a namespaced` |
|        - | 5275 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|      469 | 5276 | `					SyString *pFqn = &pClass->sName;` |
|      469 | 5277 | `					int bIsExceptionOrError =` |
|      242 | 5278 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|      706 | 5279 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|      477 | 5280 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|       26 | 5281 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|      469 | 5282 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|       18 | 5283 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|        3 | 5284 | `						!bIsExceptionOrError ){` |
|       12 | 5285 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 5286 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|        3 | 5287 | `							&pClass->sDisp);` |
|        9 | 5288 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 5289 | `							SyBlobRelease(&sResolved);` |
|      ! 0 | 5290 | `							return SXERR_ABORT;` |
|        - | 5291 | `						}` |
|        - | 5292 | `						/* Skip registration so the follow-up abstract-method` |
|        - | 5293 | `						 * check does not produce a duplicate fatal. */` |
|        6 | 5294 | `					}else{` |
|      463 | 5295 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|        - | 5296 | `					}` |
|        - | 5297 | `				}` |
|      471 | 5298 | `				SyBlobRelease(&sResolved);` |
|      471 | 5299 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      220 | 5300 | `					break;` |
|        - | 5301 | `				}` |
|       41 | 5302 | `				pGen->pIn++;/* Jump the comma */` |
|        5 | 5303 | `			}` |
|      215 | 5304 | `		}` |
|      732 | 5305 | `	}` |
|     5225 | 5306 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|        - | 5307 | `		/* Syntax error */` |
|        3 | 5308 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Expected '{' after class '%z' declaration",pName);` |
|        3 | 5309 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|        3 | 5310 | `		if( rc == SXERR_ABORT ){` |
|        - | 5311 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 5312 | `			return SXERR_ABORT;` |
|        - | 5313 | `		}` |
|        3 | 5314 | `		return SXRET_OK;` |
|        - | 5315 | `	}` |
|     5223 | 5316 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|     5223 | 5317 | `	pEnd = 0; /* cc warning */` |
|        - | 5318 | `	/* Delimit the class body */` |
|     5223 | 5319 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|     5223 | 5320 | `	if( pEnd >= pGen->pEnd ){` |
|        - | 5321 | `		/* Syntax error */` |
|      ! 0 | 5322 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|      ! 0 | 5323 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 5324 | `		if( rc == SXERR_ABORT ){` |
|        - | 5325 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 5326 | `			return SXERR_ABORT;` |
|        - | 5327 | `		}` |
|      ! 0 | 5328 | `		return SXRET_OK;` |
|        - | 5329 | `	}` |
|        - | 5330 | `	/* The delimiter token is the class body's closing brace */` |
|     5223 | 5331 | `	pClass->nEndLine = pEnd->nLine;` |
|        - | 5332 | `	/* Swap token stream */` |
|     5223 | 5333 | `	pTmp = pGen->pEnd;` |
|     5223 | 5334 | `	pGen->pEnd = pEnd;` |
|        - | 5335 | `	/* Merge the inherited flags (PH7_NewRawClass may have set INTERNAL) */` |
|     5223 | 5336 | `	pClass->iFlags \|= iFlags;` |
|     5223 | 5337 | `	if( pAnonName ){` |
|        - | 5338 | `` 		/* `new class {...}`: the name is synthesized, which is what makes `self` `` |
|        - | 5339 | `		 * inside it unusable in an intersection type (see PH7_CLASS_ANON). */` |
|      183 | 5340 | `		pClass->iFlags \|= PH7_CLASS_ANON;` |
|       89 | 5341 | `	}` |
|        - | 5342 | ``	/* ...which is what php's own attribute validators judge: `#[\Attribute]` on an`` |
|        - | 5343 | ``	 * abstract class, `#[\AllowDynamicProperties]` on a readonly one or an enum. */`` |
|     7827 | 5344 | `	if( GenStateCheckAttrPlacement(&(*pGen),&pClass->aAttrs,1,1,` |
|     7832 | 5345 | `			&pClass->sName,pClass->iFlags) == SXERR_ABORT ){` |
|      ! 0 | 5346 | `		return SXERR_ABORT;` |
|        - | 5347 | `	}` |
|        - | 5348 | `	/* This class/enum is now the lexical class for its body — see pCurClass. */` |
|     5223 | 5349 | `	pGen->pCurClass = pClass;` |
|     5223 | 5350 | `	pGen->pCurBase = pBase;` |
|        - | 5351 | `	/* Start the parse process */` |
|     6445 | 5352 | `	for(;;){` |
|        - | 5353 | `		/* Jump leading/trailing semi-colons */` |
|    16901 | 5354 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|     3539 | 5355 | `			pGen->pIn++;` |
|        5 | 5356 | `		}` |
|    13367 | 5357 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 5358 | `			/* End of class body */` |
|     5125 | 5359 | `			break;` |
|        - | 5360 | `		}` |
|        - | 5361 | `		/* Bind a directly-preceding docblock to this member */` |
|     8247 | 5362 | `		GenStateSetPendingDoc(&(*pGen));` |
|     8242 | 5363 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|     4127 | 5364 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|      ! 0 | 5365 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|        - | 5366 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|      ! 0 | 5367 | `				&pGen->pIn->sData,pName);` |
|      ! 0 | 5368 | `			if( rc == SXERR_ABORT ){` |
|        - | 5369 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 5370 | `				return SXERR_ABORT;` |
|        - | 5371 | `			}` |
|      ! 0 | 5372 | `			goto done;` |
|        - | 5373 | `		}` |
|     8247 | 5374 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - | 5375 | `			/* Extract the current keyword */` |
|     8245 | 5376 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     8245 | 5377 | `			if( nKwrd == PH7_TKWRD_CASE && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|        - | 5378 | ``				/* Enum case declaration: `case NAME [= value];` */`` |
|      175 | 5379 | `				rc = GenStateCompileEnumCase(&(*pGen),pClass);` |
|      175 | 5380 | `				if( rc != SXRET_OK ){` |
|        9 | 5381 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 5382 | `						return SXERR_ABORT;` |
|        - | 5383 | `					}` |
|        9 | 5384 | `					goto done;` |
|        - | 5385 | `				}` |
|      169 | 5386 | `				continue;` |
|        - | 5387 | `			}` |
|     8075 | 5388 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|        - | 5389 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|        - | 5390 | `				TraitUseEntry sUse;` |
|      313 | 5391 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      313 | 5392 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      313 | 5393 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      200 | 5394 | `				for(;;){` |
|        - | 5395 | `					ph7_class *pTrait;` |
|        - | 5396 | `					SyBlob sResolved;` |
|        - | 5397 | `					SyString sTraitName;` |
|      359 | 5398 | `					sxu32 nUseLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|        - | 5399 | `					/* A trait name is a full class reference: it may be qualified or` |
|        - | 5400 | ``					 * fully-qualified (`use Foo\Bar\T;`, `use \Foo\Bar\T;`) — the generated`` |
|        - | 5401 | `					 * PHPUnit mocks name their traits absolutely. Parse it with the shared` |
|        - | 5402 | `					 * class-reference reader (handles the leading '\', every '\'-segment and` |
|        - | 5403 | `					 * namespace/import resolution) instead of a single-identifier read, which` |
|        - | 5404 | `					 * choked on the first '\'. */` |
|      359 | 5405 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      359 | 5406 | `					if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 5407 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 5408 | `						rc = PH7_GenCompileError(pGen,E_PARSE,nUseLine,` |
|      ! 0 | 5409 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|      ! 0 | 5410 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 5411 | `							return SXERR_ABORT;` |
|        - | 5412 | `						}` |
|      ! 0 | 5413 | `						break;` |
|        - | 5414 | `					}` |
|      713 | 5415 | `					pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      354 | 5416 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      359 | 5417 | `					SyStringInitFromBuf(&sTraitName,` |
|        - | 5418 | `						(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - | 5419 | `					/* Only traits are allowed */` |
|      359 | 5420 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|      ! 0 | 5421 | `						pTrait = pTrait->pNextName;` |
|      ! 0 | 5422 | `					}` |
|      359 | 5423 | `					if( pTrait == 0 ){` |
|        4 | 5424 | `						if( !GenStateLintSkipsBase(pGen,pClass) ){` |
|      ! 0 | 5425 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|        - | 5426 | `								"'%z' is not a trait",&sTraitName);` |
|      ! 0 | 5427 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 5428 | `								SyBlobRelease(&sResolved);` |
|      ! 0 | 5429 | `								return SXERR_ABORT;` |
|        - | 5430 | `							}` |
|      ! 0 | 5431 | `						}` |
|        2 | 5432 | `					}else{` |
|      355 | 5433 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|        - | 5434 | `					}` |
|      359 | 5435 | `					SyBlobRelease(&sResolved);` |
|        - | 5436 | `					/* GenStateParseClassReference already advanced past the whole name —` |
|        - | 5437 | `					 * continue only across a comma-separated trait list. */` |
|      359 | 5438 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      159 | 5439 | `						break;` |
|        - | 5440 | `					}` |
|       50 | 5441 | `					pGen->pIn++; /* Jump the comma */` |
|        4 | 5442 | `				}` |
|        - | 5443 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      313 | 5444 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|        - | 5445 | `					SyToken *pBlock;` |
|       41 | 5446 | `					pGen->pIn++; /* Jump '{' */` |
|       41 | 5447 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       41 | 5448 | `					sUse.pResolvStart = pGen->pIn;` |
|       41 | 5449 | `					sUse.pResolvEnd = pBlock;` |
|       41 | 5450 | `					if( pBlock < pGen->pEnd ){` |
|       41 | 5451 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       23 | 5452 | `					}else{` |
|      ! 0 | 5453 | `						pGen->pIn = pGen->pEnd;` |
|        - | 5454 | `					}` |
|       18 | 5455 | `				}` |
|      313 | 5456 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|        - | 5457 | `				/* The semicolon will be consumed by the outer loop */` |
|      313 | 5458 | `				continue;` |
|        - | 5459 | `			}` |
|     3881 | 5460 | `		}` |
|        - | 5461 | `		/* Everything else is a MEMBER: its modifier run and the declaration it` |
|        - | 5462 | `		 * modifies. */` |
|     7769 | 5463 | `		rc = GenStateCompileMember(&(*pGen),pClass,"class");` |
|     7769 | 5464 | `		if( rc != SXRET_OK ){` |
|       96 | 5465 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 5466 | `				return SXERR_ABORT;` |
|        - | 5467 | `			}` |
|       96 | 5468 | `			goto done;` |
|        - | 5469 | `		}` |
|        5 | 5470 | `	}` |
|        - | 5471 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|        - | 5472 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|        - | 5473 | `	 */` |
|     5125 | 5474 | `	rc = GenStateApplyTraitUses(&(*pGen),pClass,&aUseEntries);` |
|     5125 | 5475 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 5476 | `		SySetRelease(&aUseEntries);` |
|      ! 0 | 5477 | `		SySetRelease(&aInterfaces);` |
|      ! 0 | 5478 | `		return SXERR_ABORT;` |
|        - | 5479 | `	}` |
|     5125 | 5480 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|        - | 5481 | `		/* Enum validation + name/value props + cases()/from()/tryFrom() synthesis.` |
|        - | 5482 | `		 * Runs after trait application so trait-imported properties are caught. */` |
|      135 | 5483 | `		rc = GenStateEnumFinalize(&(*pGen),pClass,nLine);` |
|      135 | 5484 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 5485 | `			SySetRelease(&aUseEntries);` |
|      ! 0 | 5486 | `			SySetRelease(&aInterfaces);` |
|      ! 0 | 5487 | `			return SXERR_ABORT;` |
|        - | 5488 | `		}` |
|       65 | 5489 | `	}` |
|        - | 5490 | `	/* The members this class DECLARES that claim #[\Override] -- recorded here,` |
|        - | 5491 | `	 * before inheritance copies the base's records in beside them, and verified` |
|        - | 5492 | `	 * once the answer exists. */` |
|     5125 | 5493 | `	GenStateCollectOverrides(&(*pGen),pClass,&aOvMeth,&aOvProp);` |
|        - | 5494 | `	/* Reject a php-fatal redeclaration before hoisting the class. An ENUM is` |
|        - | 5495 | `	 * never early-bound either: php gives every one of them UnitEnum. */` |
|     7469 | 5496 | `	if( GenStateGuardClassRedeclaration(pGen,pClass,` |
|     5120 | 5497 | `			SySetUsed(&aInterfaces) == 0 && SySetUsed(&aUseEntries) == 0` |
|     4566 | 5498 | `			&& (pClass->iFlags & PH7_CLASS_ENUM) == 0` |
|     4717 | 5499 | `			&& SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) == 0)` |
|     2565 | 5500 | `		== SXERR_ABORT ){` |
|       16 | 5501 | `		SySetRelease(&aOvMeth);` |
|       16 | 5502 | `		SySetRelease(&aOvProp);` |
|       16 | 5503 | `		return SXERR_ABORT;` |
|        - | 5504 | `	}` |
|        - | 5505 | `	/* Install the class */` |
|     5113 | 5506 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|     5113 | 5507 | `	if( rc == SXRET_OK ){` |
|        - | 5508 | `		ph7_class **apInterface;` |
|        - | 5509 | `		sxu32 n;` |
|     5113 | 5510 | `		if( pBase ){` |
|        - | 5511 | `			/* Inherit from base class and mark as a subclass */` |
|     1071 | 5512 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|      533 | 5513 | `		}` |
|     5113 | 5514 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|     5571 | 5515 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|        - | 5516 | `			/* Implements one or more interface */` |
|      463 | 5517 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|      463 | 5518 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 5519 | `				break;` |
|        - | 5520 | `			}` |
|      234 | 5521 | `		}` |
|        - | 5522 | `		/* Auto-implement UnitEnum (and BackedEnum for backed enums) — php 8.1:` |
|        - | 5523 | ``		 * every enum satisfies `instanceof UnitEnum` implicitly. */`` |
|     5113 | 5524 | `		if( rc == SXRET_OK && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|      133 | 5525 | `			ph7_class *pIntf = PH7_VmExtractClass(pGen->pVm,"UnitEnum",sizeof("UnitEnum")-1,FALSE,0);` |
|      133 | 5526 | `			while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 5527 | `				pIntf = pIntf->pNextName;` |
|      ! 0 | 5528 | `			}` |
|      133 | 5529 | `			if( pIntf ){` |
|      133 | 5530 | `				PH7_ClassImplement(pClass,pIntf);` |
|       64 | 5531 | `			}` |
|      133 | 5532 | `			if( pClass->nEnumBacking != 0 ){` |
|       70 | 5533 | `				pIntf = PH7_VmExtractClass(pGen->pVm,"BackedEnum",sizeof("BackedEnum")-1,FALSE,0);` |
|       70 | 5534 | `				while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 5535 | `					pIntf = pIntf->pNextName;` |
|      ! 0 | 5536 | `				}` |
|       70 | 5537 | `				if( pIntf ){` |
|       70 | 5538 | `					PH7_ClassImplement(pClass,pIntf);` |
|       33 | 5539 | `				}` |
|       33 | 5540 | `			}` |
|       64 | 5541 | `		}` |
|        - | 5542 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|        - | 5543 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|     5108 | 5544 | `		if( rc == SXRET_OK` |
|     5108 | 5545 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|     5113 | 5546 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|      315 | 5547 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|        - | 5548 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|      315 | 5549 | `			if( pStringable ){` |
|      315 | 5550 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|      315 | 5551 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|        - | 5552 | `				sxu32 i;` |
|      315 | 5553 | `				int bAlready = 0;` |
|      321 | 5554 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|       10 | 5555 | `					if( apImpl[i] == pStringable ){` |
|        3 | 5556 | `						bAlready = 1;` |
|        3 | 5557 | `						break;` |
|        - | 5558 | `					}` |
|        4 | 5559 | `				}` |
|      315 | 5560 | `				if( !bAlready ){` |
|      313 | 5561 | `					PH7_ClassImplement(pClass,pStringable);` |
|      154 | 5562 | `				}` |
|      155 | 5563 | `			}` |
|      155 | 5564 | `		}` |
|        - | 5565 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|     5113 | 5566 | `		if( rc == SXRET_OK ){` |
|     5113 | 5567 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|     5113 | 5568 | `			if( rcCheck == SXERR_ABORT ){` |
|      ! 0 | 5569 | `				SySetRelease(&aUseEntries);` |
|      ! 0 | 5570 | `				SySetRelease(&aInterfaces);` |
|      ! 0 | 5571 | `				SySetRelease(&aOvMeth);` |
|      ! 0 | 5572 | `				SySetRelease(&aOvProp);` |
|      ! 0 | 5573 | `				return SXERR_ABORT;` |
|        - | 5574 | `			}` |
|     2554 | 5575 | `		}` |
|        - | 5576 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|     5113 | 5577 | `		if( rc == SXRET_OK ){` |
|     5113 | 5578 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|     5113 | 5579 | `			if( rcCheck == SXERR_ABORT ){` |
|      ! 0 | 5580 | `				SySetRelease(&aUseEntries);` |
|      ! 0 | 5581 | `				SySetRelease(&aInterfaces);` |
|      ! 0 | 5582 | `				SySetRelease(&aOvMeth);` |
|      ! 0 | 5583 | `				SySetRelease(&aOvProp);` |
|      ! 0 | 5584 | `				return SXERR_ABORT;` |
|        - | 5585 | `			}` |
|     2554 | 5586 | `		}` |
|        - | 5587 | `		/* ...and the #[\Override] claims LAST: php reports an unimplemented` |
|        - | 5588 | `		 * abstract method and an inheritance visibility clash before this one,` |
|        - | 5589 | `		 * and stops there — php's E_COMPILE_ERROR does not return, so a` |
|        - | 5590 | `		 * declaration that already failed says nothing more. */` |
|     5113 | 5591 | `		if( rc == SXRET_OK && pGen->nErr == nErrEntry ){` |
|     4865 | 5592 | `			sxi32 rcCheck = GenStateCheckOverrides(&(*pGen),pClass,&aOvMeth,&aOvProp);` |
|     4865 | 5593 | `			if( rcCheck == SXERR_ABORT ){` |
|      ! 0 | 5594 | `				SySetRelease(&aUseEntries);` |
|      ! 0 | 5595 | `				SySetRelease(&aInterfaces);` |
|      ! 0 | 5596 | `				SySetRelease(&aOvMeth);` |
|      ! 0 | 5597 | `				SySetRelease(&aOvProp);` |
|      ! 0 | 5598 | `				return SXERR_ABORT;` |
|        - | 5599 | `			}` |
|     2430 | 5600 | `		}` |
|     2554 | 5601 | `	}` |
|     5113 | 5602 | `	SySetRelease(&aUseEntries);` |
|     5113 | 5603 | `	SySetRelease(&aInterfaces);` |
|     5113 | 5604 | `	SySetRelease(&aOvMeth);` |
|     5113 | 5605 | `	SySetRelease(&aOvProp);` |
|     5113 | 5606 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 5607 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 5608 | `		return SXERR_ABORT;` |
|        - | 5609 | `	}` |
|     2554 | 5610 | `done:` |
|     5211 | 5611 | `	pGen->pCurClass = pSavedCurClass;` |
|     5211 | 5612 | `	pGen->pCurBase = pSavedCurBase;` |
|        - | 5613 | `	/* Point beyond the class body */` |
|     5211 | 5614 | `	pGen->pIn = &pEnd[1];` |
|     5211 | 5615 | `	pGen->pEnd = pTmp;` |
|     5211 | 5616 | `	return PH7_OK;` |
|     2682 | 5617 | `}` |
|        - | 5618 | `/* Compile a named class declaration (the common case). */` |
|     5176 | 5619 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|        5 | 5620 | `{` |
|     5181 | 5621 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|        5 | 5622 | `}` |
|        - | 5623 | `/*` |
|        - | 5624 | `` * The PREFIX php puts in front of an anonymous class's `@anonymous`: the resolved`` |
|        - | 5625 | ` * parent class name, else the resolved name of the FIRST implemented interface,` |
|        - | 5626 | ` * else the literal "class". Both are the name as WRITTEN, resolved through the` |
|        - | 5627 | `` * file's namespace and `use` imports -- not the name of whatever class the`` |
|        - | 5628 | ` * reference turns out to bind to, which at this point may not be declared yet.` |
|        - | 5629 | ` *` |
|        - | 5630 | `` * Peeks: the cursor sits on the `class` keyword when this runs and has to still`` |
|        - | 5631 | ` * sit there when it returns, because GenStateCompileClassEx parses the same` |
|        - | 5632 | ` * tokens for real straight afterwards.` |
|        - | 5633 | ` */` |
|      180 | 5634 | `static void GenStateAnonClassPrefix(ph7_gen_state *pGen,SyToken *pKw,SyBlob *pOut)` |
|        5 | 5635 | `{` |
|      185 | 5636 | `	SyToken *pSaveIn = pGen->pIn;` |
|      185 | 5637 | `	SyToken *pIn = pKw;` |
|      185 | 5638 | `	int bGot = 0;` |
|      185 | 5639 | `	pIn++; /* Step over 'class' */` |
|      185 | 5640 | `	if( pIn < pGen->pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|        - | 5641 | ``		/* `new class(args) extends B {}` -- the constructor list sits between the`` |
|        - | 5642 | `		 * keyword and the inheritance clause. */` |
|       75 | 5643 | `		SyToken *pClose = 0;` |
|       75 | 5644 | `		PH7_DelimitNestedTokens(&pIn[1],pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|       75 | 5645 | `		pIn = ( pClose && pClose < pGen->pEnd ) ? &pClose[1] : pGen->pEnd;` |
|       35 | 5646 | `	}` |
|      185 | 5647 | `	if( pIn < pGen->pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|      115 | 5648 | `		sxi32 nKwrd = SX_PTR_TO_INT(pIn->pUserData);` |
|      115 | 5649 | `		if( nKwrd == PH7_TKWRD_EXTENDS \|\| nKwrd == PH7_TKWRD_IMPLEMENTS ){` |
|      115 | 5650 | `			pGen->pIn = &pIn[1];` |
|      115 | 5651 | `			if( GenStateParseClassReference(pGen,pOut) == SXRET_OK ){` |
|      115 | 5652 | `				bGot = 1;` |
|       55 | 5653 | `			}` |
|       55 | 5654 | `		}` |
|       55 | 5655 | `	}` |
|      185 | 5656 | `	pGen->pIn = pSaveIn;` |
|      185 | 5657 | `	if( !bGot ){` |
|       75 | 5658 | `		SyBlobAppend(pOut,"class",sizeof("class")-1);` |
|       35 | 5659 | `	}` |
|      185 | 5660 | `}` |
|        - | 5661 | `/*` |
|        - | 5662 | ` * Synthesize php's name for an anonymous class into pOut:` |
|        - | 5663 | ` *` |
|        - | 5664 | ` *     <prefix>@anonymous \0 <file>:<line>$<hex>` |
|        - | 5665 | ` *` |
|        - | 5666 | ` * The NUL is php's, and it is the whole reason this name has two halves. php` |
|        - | 5667 | ` * builds it with one snprintf and then gets the short form everywhere for free,` |
|        - | 5668 | `` * because every diagnostic prints a class name with `%s` and stops there; only`` |
|        - | 5669 | ` * the surfaces that hand the name back as a VALUE -- get_class(), ::class,` |
|        - | 5670 | ` * __CLASS__, ReflectionClass::getName() -- carry the whole thing. Libraries read` |
|        - | 5671 | `` * it exactly that way: `strpos($class, "@anonymous\0") !== false` is monolog's`` |
|        - | 5672 | ` * Utils::getClass and symfony's idiom for "is this an anonymous class".` |
|        - | 5673 | ` *` |
|        - | 5674 | ` * The tail is what makes the name UNIQUE, which the prefix alone is not: two` |
|        - | 5675 | ` * anonymous classes written on the same line share a file, a line and usually a` |
|        - | 5676 | ` * parent, so php separates them with a request-wide counter in hex` |
|        - | 5677 | ` * (CG(rtd_key_counter), bumped in compile order).` |
|        - | 5678 | ` */` |
|      180 | 5679 | `static void GenStateAnonClassName(ph7_gen_state *pGen,SyToken *pKw,SyBlob *pOut)` |
|        5 | 5680 | `{` |
|      185 | 5681 | `	SyString *pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|      185 | 5682 | `	GenStateAnonClassPrefix(pGen,pKw,pOut);` |
|      185 | 5683 | `	SyBlobAppend(pOut,"@anonymous",sizeof("@anonymous")-1);` |
|      185 | 5684 | `	SyBlobAppend(pOut,"\0",1);` |
|      185 | 5685 | `	if( pFile && pFile->nByte > 0 ){` |
|      185 | 5686 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|       95 | 5687 | `	}else{` |
|        - | 5688 | `		/* No file on the include stack: an :MEMORY: chunk, which is what the magic` |
|        - | 5689 | `		 * __FILE__ constant answers in the same situation. */` |
|      ! 0 | 5690 | `		SyBlobAppend(pOut,":MEMORY:",sizeof(":MEMORY:")-1);` |
|        - | 5691 | `	}` |
|      185 | 5692 | `	SyBlobFormat(pOut,":%u$%x",pKw->nLine,pGen->pVm->nAnonSeq++);` |
|      185 | 5693 | `}` |
|        - | 5694 | `/*` |
|        - | 5695 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|        - | 5696 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|        - | 5697 | ` * compile + install the class body once (at compile time, like every other` |
|        - | 5698 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|        - | 5699 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|        - | 5700 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|        - | 5701 | ` */` |
|      186 | 5702 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 5703 | `{` |
|        - | 5704 | `	SyString sName;` |
|        - | 5705 | `	SyToken *pArgStart,*pArgEnd;` |
|        - | 5706 | `	SyToken *pTokKw;` |
|      191 | 5707 | `	sxi32 iAnonFlags = 0;` |
|        - | 5708 | `	ph7_value *pObj;` |
|      191 | 5709 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 5710 | `	sxu32 nIdx,nLen;` |
|        - | 5711 | `	sxi32 nArg,rc;` |
|       93 | 5712 | `	SXUNUSED(iCompileFlag);` |
|      191 | 5713 | `	if( GenStateIsReadonly(pGen->pIn) && &pGen->pIn[1] < pGen->pEnd ){` |
|        - | 5714 | ``		/* `new readonly class …` (PHP 8.3). Step over the modifier so everything`` |
|        - | 5715 | ``		 * below sees the cursor on `class`, where it has always been, and carry`` |
|        - | 5716 | `		 * the flag into the class body — which is what makes every property` |
|        - | 5717 | `		 * readonly and refuses a non-readonly base. */` |
|        5 | 5718 | `		iAnonFlags = PH7_CLASS_READONLY;` |
|        5 | 5719 | `		pGen->pIn++;` |
|        2 | 5720 | `	}` |
|      191 | 5721 | ``	pTokKw = pGen->pIn; /* Attribute-sidecar key: `new #[A] class` trivia`` |
|        - | 5722 | `	                     * is keyed to this 'class' token */` |
|      191 | 5723 | `	if( pGen->pVm->sDeferAnonName.nByte > 0 ){` |
|        - | 5724 | `		/* Deferred re-compile (VmExecDeferredClass): install under the SAME` |
|        - | 5725 | `		 * synthesized name the original site's OP_NEW loads. One-shot. */` |
|        8 | 5726 | `		sName = pGen->pVm->sDeferAnonName;` |
|        8 | 5727 | `		nLen = sName.nByte;` |
|        8 | 5728 | `		pGen->pVm->sDeferAnonName.zString = 0;` |
|        8 | 5729 | `		pGen->pVm->sDeferAnonName.nByte = 0;` |
|        5 | 5730 | `	}else{` |
|        - | 5731 | `		/* Synthesize php's name. It carries a NUL and a path, so it does not fit a` |
|        - | 5732 | ``		 * stack buffer and cannot be built with the `%s` formatter; and it has to`` |
|        - | 5733 | `		 * outlive this function (the site's OP_NEW loads it as a literal), so it is` |
|        - | 5734 | `		 * duplicated into the VM allocator exactly as the deferred path's copy is. */` |
|        - | 5735 | `		SyBlob sAnon;` |
|        - | 5736 | `		char *zDup;` |
|      185 | 5737 | `		SyBlobInit(&sAnon,&pGen->pVm->sAllocator);` |
|      185 | 5738 | `		GenStateAnonClassName(pGen,pTokKw,&sAnon);` |
|      185 | 5739 | `		nLen = (sxu32)SyBlobLength(&sAnon);` |
|      185 | 5740 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sAnon),nLen);` |
|      185 | 5741 | `		SyBlobRelease(&sAnon);` |
|      185 | 5742 | `		if( zDup == 0 ){` |
|      ! 0 | 5743 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 5744 | `			return SXERR_ABORT;` |
|        - | 5745 | `		}` |
|      185 | 5746 | `		SyStringInitFromBuf(&sName,zDup,nLen);` |
|        - | 5747 | `	}` |
|        - | 5748 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|        - | 5749 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|        - | 5750 | `	 * delimited construct; GenStateCompileClassEx restores both on success.` |
|        - | 5751 | ``	 * Deferral gate: `new class extends \App\Child {}` where the`` |
|        - | 5752 | `	 * parent's autoloader has not RUN yet — capture the class for a runtime` |
|        - | 5753 | `	 * re-compile and keep only the site's argument/OP_NEW emission here. */` |
|      191 | 5754 | `	pArgStart = pArgEnd = 0;` |
|        - | 5755 | `	{` |
|        - | 5756 | `		SySet aMissing;` |
|      191 | 5757 | `		SyToken *pBody = 0;` |
|      191 | 5758 | `		SyToken *pBodyEnd = 0;` |
|        - | 5759 | `		SyBlob sSelfFqn;` |
|      191 | 5760 | `		int bDeferred = 0;` |
|      191 | 5761 | `		SySetInit(&aMissing,&pGen->pVm->sAllocator,sizeof(VmDeferredReq));` |
|      191 | 5762 | `		SyBlobInit(&sSelfFqn,&pGen->pVm->sAllocator);` |
|        - | 5763 | `		/* An anonymous class is an EXPRESSION: it is always compiled where it runs,` |
|        - | 5764 | `		 * so resolving its parent here is resolving it at its execution point --` |
|        - | 5765 | `		 * the autoload belongs. */` |
|      186 | 5766 | `		if( GenStateScanDeferDeps(pGen,1,PH7_DEFER_KIND_CLASS,&aMissing,&pBody,&pBodyEnd,&sSelfFqn,0) == SXRET_OK` |
|      191 | 5767 | `		 && SySetUsed(&aMissing) > 0 && !pGen->pVm->bSyntaxCheck ){` |
|       10 | 5768 | `			if( &pTokKw[1] < pGen->pEnd && (pTokKw[1].nType & PH7_TK_LPAREN) ){` |
|        3 | 5769 | `				SyToken *pClose = 0;` |
|        3 | 5770 | `				PH7_DelimitNestedTokens(&pTokKw[2],pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|        3 | 5771 | `				if( pClose && pClose < pGen->pEnd ){` |
|        3 | 5772 | `					pArgStart = &pTokKw[2];` |
|        3 | 5773 | `					pArgEnd = pClose;` |
|        1 | 5774 | `				}` |
|        1 | 5775 | `			}` |
|       10 | 5776 | `			rc = GenStateEmitDeferredClass(pGen,0,1,&aMissing,pBodyEnd,0,&sName);` |
|       10 | 5777 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 5778 | `				SySetRelease(&aMissing);` |
|      ! 0 | 5779 | `				SyBlobRelease(&sSelfFqn);` |
|      ! 0 | 5780 | `				return SXERR_ABORT;` |
|        - | 5781 | `			}` |
|       10 | 5782 | `			bDeferred = ( rc == SXRET_OK );` |
|        4 | 5783 | `		}` |
|      191 | 5784 | `		SySetRelease(&aMissing);` |
|      191 | 5785 | `		SyBlobRelease(&sSelfFqn);` |
|      191 | 5786 | `		if( !bDeferred ){` |
|      183 | 5787 | `			rc = GenStateCompileClassEx(pGen,iAnonFlags,&sName,&pArgStart,&pArgEnd);` |
|      183 | 5788 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 5789 | `				return rc;` |
|        - | 5790 | `			}` |
|        - | 5791 | `			{` |
|        - | 5792 | ``				/* Expression-position attributes (`new #[A] class {…}`) */`` |
|      183 | 5793 | `				ph7_class *pAnonClass = PH7_VmExtractClass(pGen->pVm,sName.zString,nLen,FALSE,0);` |
|      178 | 5794 | `				if( pAnonClass` |
|      183 | 5795 | `				 && GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnonClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 5796 | `					return SXERR_ABORT;` |
|        - | 5797 | `				}` |
|      183 | 5798 | `				if( pAnonClass ){` |
|        - | 5799 | `					/* php's DECLARE_ANON_CLASS runs BEFORE the constructor arguments, and` |
|        - | 5800 | `					 * it is where an unimplemented abstract method is refused -- so` |
|        - | 5801 | ``					 * `new class(f()) extends Abs {}` never evaluates `f()`. The named`` |
|        - | 5802 | `					 * path spells that ordering as a screen OP_NEW ahead of its argument` |
|        - | 5803 | `					 * list; this list is emitted from raw tokens with the class name` |
|        - | 5804 | `					 * pushed after it, so the screen carries the class in p3 instead and` |
|        - | 5805 | `					 * touches no stack (iP1 == -2). The check itself is the same routine` |
|        - | 5806 | `					 * the instantiation runs, so the two can never disagree. */` |
|      183 | 5807 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,-2,0,(void *)pAnonClass,0);` |
|       89 | 5808 | `				}` |
|        - | 5809 | `			}` |
|       89 | 5810 | `		}` |
|        - | 5811 | `	}` |
|        - | 5812 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|        - | 5813 | `	 * with the constructor arguments beneath it, so push the args first.` |
|        - | 5814 | `	 *` |
|        - | 5815 | `	 * This argument list is compiled from RAW TOKENS rather than through the` |
|        - | 5816 | `	 * expression parser's argument machinery (the class body sits between the` |
|        - | 5817 | `	 * parentheses and the rest of the expression), so the two forms that machinery` |
|        - | 5818 | ``	 * recognizes have to be recognized here too — `...$args` and `name: $v`. They`` |
|        - | 5819 | `	 * were not: a spread was compiled as one ordinary argument, so` |
|        - | 5820 | ``	 * `new class(...$a) {}` passed the ARRAY where php passes its elements, and a`` |
|        - | 5821 | ``	 * named argument was a `Syntax error: Unexpected token ':'` on source php`` |
|        - | 5822 | `	 * compiles. */` |
|      191 | 5823 | `	nArg = 0;` |
|        - | 5824 | `	{` |
|        - | 5825 | `	SySet aArgName;              /* one SyString per argument; {0,0} == positional */` |
|      191 | 5826 | `	int hasNamed = 0, hasSpread = 0;` |
|        - | 5827 | `	void *p3;` |
|      191 | 5828 | `	SySetInit(&aArgName,&pGen->pVm->sAllocator,sizeof(SyString));` |
|      191 | 5829 | `	if( pArgStart < pArgEnd ){` |
|       73 | 5830 | `		SyToken *pSavedIn = pGen->pIn;` |
|       73 | 5831 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|        - | 5832 | `		SyToken *pArgNext;` |
|       73 | 5833 | `		const char *zOrder = 0;   /* set when this argument's POSITION or shape is refused */` |
|        - | 5834 | ``		SyString sOrderName;      /* nByte > 0: zOrder is a `'%z:'` format, E_PARSE */`` |
|       73 | 5835 | `		SyZero(&sOrderName,sizeof(sOrderName));` |
|       73 | 5836 | `		pGen->pIn = pArgStart;` |
|       73 | 5837 | `		pGen->pEnd = pArgEnd;` |
|      151 | 5838 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|       87 | 5839 | `			SyToken *pArgIn = pGen->pIn;` |
|        - | 5840 | `			SyString sArgName;` |
|       87 | 5841 | `			int bSpread = 0;` |
|       87 | 5842 | `			SyZero(&sArgName,sizeof(sArgName));` |
|       87 | 5843 | `			if( pArgIn < pArgNext && (pArgIn->nType & PH7_TK_ELLIPSIS) ){` |
|       26 | 5844 | `				bSpread = 1;` |
|       26 | 5845 | `				pArgIn++;` |
|       26 | 5846 | `				if( hasNamed ){` |
|        3 | 5847 | `					zOrder = "Cannot use argument unpacking after named arguments";` |
|        1 | 5848 | `				}` |
|       72 | 5849 | `			}else if( &pArgIn[1] < pArgNext` |
|       50 | 5850 | `			 && (pArgIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|       43 | 5851 | `			 && (pArgIn[1].nType & PH7_TK_COLON) ){` |
|        - | 5852 | ``				/* `name: value`. php accepts a reserved word as a parameter name, and`` |
|        - | 5853 | ``				 * `::` lexes as its own operator, so an ID/KEYWORD followed by a SINGLE`` |
|        - | 5854 | `				 * colon at the head of an argument can only be this. */` |
|       25 | 5855 | `				sArgName = pArgIn->sData;` |
|       25 | 5856 | `				hasNamed = 1;` |
|       25 | 5857 | `				pArgIn += 2;` |
|       25 | 5858 | `				if( pArgIn >= pArgNext ){` |
|        - | 5859 | ``					/* `new class(a:) {}` — a name with no value. Spelled exactly as the`` |
|        - | 5860 | `					 * ordinary argument path spells it (php names the token that stopped` |
|        - | 5861 | `					 * it instead; that wording gap belongs to the parse-error family and` |
|        - | 5862 | `					 * is now one gap in both places rather than silence in this one). */` |
|        3 | 5863 | `					zOrder = "syntax error, expected expression after named argument '%z:'";` |
|        3 | 5864 | `					sOrderName = sArgName;` |
|       23 | 5865 | `				}else if( pArgIn->nType & PH7_TK_ELLIPSIS ){` |
|      ! 0 | 5866 | `					zOrder = "syntax error, unexpected token \"...\"";` |
|        3 | 5867 | `				}` |
|       51 | 5868 | `			}else if( hasNamed ){` |
|      ! 0 | 5869 | `				zOrder = "Cannot use positional argument after named argument";` |
|       40 | 5870 | `			}else if( hasSpread ){` |
|      ! 0 | 5871 | `				zOrder = "Cannot use positional argument after argument unpacking";` |
|      ! 0 | 5872 | `			}` |
|       87 | 5873 | `			if( zOrder ){` |
|        - | 5874 | `				/* The same four rules the ordinary call path enforces at COMPILE time` |
|        - | 5875 | `				 * (GenStateEmitCallArgs); an anonymous class's list is parsed here and` |
|        - | 5876 | `				 * so had none of them. */` |
|        6 | 5877 | `				sxu32 nErrLine = pArgNext > pArgStart ? pArgNext[-1].nLine : nLine;` |
|        6 | 5878 | `				pGen->pIn = pSavedIn;` |
|        6 | 5879 | `				pGen->pEnd = pSavedEnd;` |
|        6 | 5880 | `				SySetRelease(&aArgName);` |
|        6 | 5881 | `				if( sOrderName.nByte > 0 ){` |
|        3 | 5882 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,nErrLine,zOrder,&sOrderName);` |
|        2 | 5883 | `				}else{` |
|        3 | 5884 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,nErrLine,"%s",zOrder);` |
|        - | 5885 | `				}` |
|        6 | 5886 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 5887 | `			}` |
|       83 | 5888 | `			if( pArgIn < pArgNext ){` |
|       83 | 5889 | `				rc = GenStateCompileArrayEntry(pGen,pArgIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|       83 | 5890 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 5891 | `					pGen->pIn = pSavedIn;` |
|      ! 0 | 5892 | `					pGen->pEnd = pSavedEnd;` |
|      ! 0 | 5893 | `					SySetRelease(&aArgName);` |
|      ! 0 | 5894 | `					return SXERR_ABORT;` |
|        - | 5895 | `				}` |
|       83 | 5896 | `				if( bSpread ){` |
|        - | 5897 | `					/* iP1 marks a source php unpacks BY REFERENCE: only a plain` |
|        - | 5898 | ``					 * `$var`, which is exactly two tokens here. */`` |
|       28 | 5899 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_SPREAD,` |
|       27 | 5900 | `						(&pArgIn[2] == pArgNext && (pArgIn->nType & PH7_TK_DOLLAR)` |
|       10 | 5901 | `						 && (pArgIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))) ? 1 : 0,` |
|        - | 5902 | `						0,0,0);` |
|       23 | 5903 | `					hasSpread = 1;` |
|       11 | 5904 | `				}` |
|       83 | 5905 | `				SySetPut(&aArgName,(const void *)&sArgName);` |
|       83 | 5906 | `				nArg++;` |
|       39 | 5907 | `			}` |
|       83 | 5908 | `			pGen->pIn = &pArgNext[1];` |
|        5 | 5909 | `		}` |
|       68 | 5910 | `		pGen->pIn = pSavedIn;` |
|       68 | 5911 | `		pGen->pEnd = pSavedEnd;` |
|       32 | 5912 | `	}` |
|        - | 5913 | `	/* Load the synthesized class name */` |
|      187 | 5914 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      187 | 5915 | `	if( pObj == 0 ){` |
|      ! 0 | 5916 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 5917 | `		SySetRelease(&aArgName);` |
|      ! 0 | 5918 | `		return SXERR_ABORT;` |
|        - | 5919 | `	}` |
|      187 | 5920 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      187 | 5921 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - | 5922 | `	/* The names ride on the instruction as a VmCallArgMap, deep-copied out of the` |
|        - | 5923 | `	 * token stream (which is freed before the code runs) exactly as the ordinary` |
|        - | 5924 | `	 * call path copies them. */` |
|      187 | 5925 | `	p3 = 0;` |
|      187 | 5926 | `	if( hasNamed ){` |
|       15 | 5927 | `		SyString *aName = (SyString *)SySetBasePtr(&aArgName);` |
|       15 | 5928 | `		sxu32 n, nStrBytes = 0;` |
|       35 | 5929 | `		for( n = 0 ; n < (sxu32)nArg ; ++n ){` |
|       21 | 5930 | `			nStrBytes += aName[n].nByte;` |
|       11 | 5931 | `		}` |
|        - | 5932 | `		{` |
|       15 | 5933 | `		sxu32 mapSize = sizeof(VmCallArgMap) + (sxu32)nArg * sizeof(SyString) + nStrBytes;` |
|       15 | 5934 | `		VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,mapSize);` |
|       15 | 5935 | `		if( pMap ){` |
|        - | 5936 | `			char *zBuf;` |
|       15 | 5937 | `			SyZero(pMap,mapSize);` |
|       15 | 5938 | `			pMap->bHasNamed = 1;` |
|       15 | 5939 | `			pMap->nTotal = (sxu32)nArg;` |
|       15 | 5940 | `			pMap->aNames = (SyString *)&pMap[1];` |
|       15 | 5941 | `			zBuf = (char *)&pMap->aNames[nArg];` |
|       35 | 5942 | `			for( n = 0 ; n < (sxu32)nArg ; ++n ){` |
|       21 | 5943 | `				if( aName[n].nByte > 0 ){` |
|       19 | 5944 | `					SyMemcpy(aName[n].zString,zBuf,aName[n].nByte);` |
|       19 | 5945 | `					SyStringInitFromBuf(&pMap->aNames[n],zBuf,aName[n].nByte);` |
|       19 | 5946 | `					zBuf += aName[n].nByte;` |
|        9 | 5947 | `				}` |
|       11 | 5948 | `			}` |
|       15 | 5949 | `			p3 = (void *)pMap;` |
|        7 | 5950 | `		}` |
|        - | 5951 | `		}` |
|        7 | 5952 | `	}` |
|      187 | 5953 | `	SySetRelease(&aArgName);` |
|        - | 5954 | `	/* Instantiate: pops the name + nArg arguments, runs __construct. iP2 is the` |
|        - | 5955 | `	 * spread flag the effective-argument-map builder keys on. */` |
|      278 | 5956 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,hasSpread ? 1 : 0,` |
|       91 | 5957 | `		GenStateAttachStrictFlag(pGen,p3),0);` |
|        - | 5958 | `	}` |
|      187 | 5959 | `	return SXRET_OK;` |
|       98 | 5960 | `}` |
|        - | 5961 | `/*` |
|        - | 5962 | ` * Compile a user-defined abstract class.` |
|        - | 5963 | ` *  According to the PHP language reference manual` |
|        - | 5964 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|        - | 5965 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|        - | 5966 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|        - | 5967 | ` *   the method's signature - they cannot define the implementation.` |
|        - | 5968 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|        - | 5969 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|        - | 5970 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|        - | 5971 | ` *   method is defined as protected, the function implementation must be defined as either` |
|        - | 5972 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|        - | 5973 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|        - | 5974 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|        - | 5975 | ` *   could differ.` |
|        - | 5976 | ` */` |
|        - | 5977 | `/*` |
|        - | 5978 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|        - | 5979 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|        - | 5980 | ` * receives the corresponding PH7_CLASS_* bit.` |
|        - | 5981 | ` */` |
|  2431834 | 5982 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|        5 | 5983 | `{` |
|  2431839 | 5984 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|  1435373 | 5985 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|  1435373 | 5986 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|  1435257 | 5987 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|   716460 | 5988 | `	}` |
|  2431427 | 5989 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  2431341 | 5990 | `	return FALSE;` |
|  1213995 | 5991 | `}` |
|        - | 5992 | `/*` |
|        - | 5993 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|        - | 5994 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|        - | 5995 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|        - | 5996 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|        - | 5997 | ` */` |
|  2431336 | 5998 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|        5 | 5999 | `{` |
|  2431341 | 6000 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  2431341 | 6001 | `	sxi32 iFlags = 0,iFlag;` |
|  2431839 | 6002 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|      503 | 6003 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|        5 | 6004 | `			pDup = pIn;` |
|        2 | 6005 | `		}` |
|      503 | 6006 | `		iFlags \|= iFlag;` |
|      503 | 6007 | `		pIn++;` |
|        5 | 6008 | `	}` |
|  2431341 | 6009 | `	*ppIn = pIn;` |
|  2431341 | 6010 | `	if( ppDup ){ *ppDup = pDup; }` |
|  2431341 | 6011 | `	return iFlags;` |
|        5 | 6012 | `}` |
|        - | 6013 | `/*` |
|        - | 6014 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|        - | 6015 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|        - | 6016 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|        - | 6017 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|        - | 6018 | `` * `readonly`) to their existing handlers.`` |
|        - | 6019 | ` */` |
|  2431108 | 6020 | `PH7_PRIVATE int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|        5 | 6021 | `{` |
|  2431113 | 6022 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|  1213872 | 6023 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  2431229 | 6024 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|        5 | 6025 | `}` |
|        - | 6026 | `/*` |
|        - | 6027 | ``  * Return TRUE when the token stream starts `readonly class …` in a `new` `` |
|        - | 6028 | `` * operand — PHP 8.3's readonly ANONYMOUS class. `readonly` is the only modifier`` |
|        - | 6029 | `` * php admits there: `new final class {}` and `new abstract class {}` are parse`` |
|        - | 6030 | ` * errors, so this deliberately does NOT reuse GenStateScanClassModifiers.` |
|        - | 6031 | ` */` |
|  6643822 | 6032 | `PH7_PRIVATE int GenStateStartsReadonlyAnonClass(SyToken *pIn,SyToken *pEnd)` |
|        5 | 6033 | `{` |
|  6643827 | 6034 | `	if( !GenStateIsReadonly(pIn) \|\| &pIn[1] >= pEnd ){` |
|  6643819 | 6035 | `		return 0;` |
|        - | 6036 | `	}` |
|       10 | 6037 | `	pIn++;` |
|        8 | 6038 | `	if( (pIn->nType & PH7_TK_KEYWORD) == 0` |
|        6 | 6039 | `	 \|\| (sxu32)SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_CLASS` |
|        6 | 6040 | `	 \|\| &pIn[1] >= pEnd ){` |
|        6 | 6041 | `		return 0;` |
|        - | 6042 | `	}` |
|        - | 6043 | `` 	/* Same shape test the bare `new class` branch makes, so a stray `readonly` `` |
|        - | 6044 | ``	 * followed by the `::class` constant is left to the literal path. */`` |
|        5 | 6045 | `	pIn++;` |
|        5 | 6046 | `	return (pIn->nType & (PH7_TK_OCB\|PH7_TK_LPAREN)) != 0` |
|        4 | 6047 | `		\|\| ( (pIn->nType & PH7_TK_KEYWORD)` |
|      ! 0 | 6048 | `		  && ( (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_EXTENDS` |
|      ! 0 | 6049 | `		    \|\| (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ) );` |
|  3315621 | 6050 | `}` |
|        - | 6051 | `/*` |
|        - | 6052 | ` * Compile a class declaration carrying one or more leading modifiers` |
|        - | 6053 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|        - | 6054 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|        - | 6055 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|        - | 6056 | `` * `abstract`+`final` pair, like PHP.`` |
|        - | 6057 | ` */` |
|      228 | 6058 | `PH7_PRIVATE sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|        5 | 6059 | `{` |
|        - | 6060 | `	SyToken *pDup;` |
|      233 | 6061 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|        - | 6062 | `	sxi32 rc;` |
|      233 | 6063 | `	if( pDup ){` |
|        4 | 6064 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|        2 | 6065 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|        3 | 6066 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 6067 | `			return SXERR_ABORT;` |
|        - | 6068 | `		}` |
|        1 | 6069 | `	}` |
|      228 | 6070 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|      119 | 6071 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|        6 | 6072 | `		pGen->iFatalTrace = PH7_FATAL_TRACE_NONE;` |
|        6 | 6073 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 6074 | `			"Cannot use the final modifier on an abstract class");` |
|        6 | 6075 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 6076 | `			return SXERR_ABORT;` |
|        - | 6077 | `		}` |
|        2 | 6078 | `	}` |
|      233 | 6079 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|      119 | 6080 | `}` |
|        - | 6081 | `/*` |
|        - | 6082 | ` * Compile a user-defined trait.` |
|        - | 6083 | ` *  Traits are similar to classes, but only intended to group functionality` |
|        - | 6084 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|        - | 6085 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|        - | 6086 | ` */` |
|      356 | 6087 | `PH7_PRIVATE sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|        5 | 6088 | `{` |
|      361 | 6089 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 6090 | `	ph7_class *pClass;` |
|      361 | 6091 | `	ph7_class *pSavedCurClass = pGen->pCurClass; /* restored at 'done' */` |
|      361 | 6092 | `	ph7_class *pSavedCurBase = pGen->pCurBase;   /* ...and its base, see pCurBase */` |
|        - | 6093 | `	SyToken *pEnd,*pTmp;` |
|        - | 6094 | ``	SySet aUseEntries; /* trait-body `use` statements (incl. adaptation blocks) */`` |
|        - | 6095 | `	SyString *pName;` |
|        - | 6096 | `	sxi32 nKwrd;` |
|        - | 6097 | `	sxi32 rc;` |
|        - | 6098 | `	{` |
|        - | 6099 | `		/* Deferral gate: a used trait may need an autoloader that` |
|        - | 6100 | `		 * has not run yet. */` |
|        - | 6101 | `		sxi32 rcDefer;` |
|      361 | 6102 | `		if( GenStateMaybeDeferClass(pGen,0,PH7_DEFER_KIND_TRAIT,&rcDefer) ){` |
|       19 | 6103 | `			return rcDefer == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 6104 | `		}` |
|        - | 6105 | `	}` |
|      345 | 6106 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|        - | 6107 | `	/* Jump the 'trait' keyword */` |
|      345 | 6108 | `	pGen->pIn++;` |
|      345 | 6109 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 | 6110 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|      ! 0 | 6111 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 6112 | `			return SXERR_ABORT;` |
|        - | 6113 | `		}` |
|      ! 0 | 6114 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|      ! 0 | 6115 | `			pGen->pIn++;` |
|      ! 0 | 6116 | `		}` |
|      ! 0 | 6117 | `		return SXRET_OK;` |
|        - | 6118 | `	}` |
|        - | 6119 | `	/* Extract trait name */` |
|      345 | 6120 | `	pName = &pGen->pIn->sData;` |
|      345 | 6121 | `	pGen->pIn++;` |
|        - | 6122 | `	/* Build FQN and obtain a raw class */ {` |
|        - | 6123 | `		SyBlob sFQN;` |
|        - | 6124 | `		SyString sFQNStr;` |
|      345 | 6125 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      345 | 6126 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      345 | 6127 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|        - | 6128 | ``		/* php refuses a declaration whose short name a local `use` already took. */`` |
|      345 | 6129 | `		if( GenStateGuardImportRedeclare(pGen,0,pName,&sFQNStr,nLine) == SXERR_ABORT ){` |
|      ! 0 | 6130 | `			SyBlobRelease(&sFQN);` |
|      ! 0 | 6131 | `			return SXERR_ABORT;` |
|        - | 6132 | `		}` |
|      345 | 6133 | `		GenStateRecordDeclaredName(pGen,0,&sFQNStr);` |
|      345 | 6134 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      345 | 6135 | `		SyBlobRelease(&sFQN);` |
|        - | 6136 | `	}` |
|      345 | 6137 | `	if( pClass == 0 ){` |
|      ! 0 | 6138 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 6139 | `		return SXERR_ABORT;` |
|        - | 6140 | `	}` |
|      345 | 6141 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|      345 | 6142 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 6143 | `		return SXERR_ABORT;` |
|        - | 6144 | `	}` |
|        - | 6145 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      345 | 6146 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|      ! 0 | 6147 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|      ! 0 | 6148 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 6149 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 6150 | `			return SXERR_ABORT;` |
|        - | 6151 | `		}` |
|      ! 0 | 6152 | `		return SXRET_OK;` |
|        - | 6153 | `	}` |
|      345 | 6154 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      345 | 6155 | `	pEnd = 0;` |
|      345 | 6156 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      345 | 6157 | `	if( pEnd >= pGen->pEnd ){` |
|      ! 0 | 6158 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|      ! 0 | 6159 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 6160 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 6161 | `			return SXERR_ABORT;` |
|        - | 6162 | `		}` |
|      ! 0 | 6163 | `		return SXRET_OK;` |
|        - | 6164 | `	}` |
|        - | 6165 | `	/* The delimiter token is the trait body's closing brace */` |
|      345 | 6166 | `	pClass->nEndLine = pEnd->nLine;` |
|        - | 6167 | `	/* Swap token stream */` |
|      345 | 6168 | `	pTmp = pGen->pEnd;` |
|      345 | 6169 | `	pGen->pEnd = pEnd;` |
|        - | 6170 | `	/* Mark as trait (PH7_NewRawClass may have set INTERNAL) */` |
|      345 | 6171 | `	pClass->iFlags \|= PH7_CLASS_TRAIT;` |
|      510 | 6172 | `	if( GenStateCheckAttrPlacement(&(*pGen),&pClass->aAttrs,1,1,` |
|      515 | 6173 | `			&pClass->sName,pClass->iFlags) == SXERR_ABORT ){` |
|      ! 0 | 6174 | `		return SXERR_ABORT;` |
|        - | 6175 | `	}` |
|        - | 6176 | `	/* This trait is now the lexical class for its body, so a property/parameter` |
|        - | 6177 | `	 * default here resolves __TRAIT__ to it (see pCurClass). */` |
|      345 | 6178 | `	pGen->pCurClass = pClass;` |
|      345 | 6179 | ``	pGen->pCurBase = 0; /* a trait has no base; its `parent` is deferred to composition */`` |
|        - | 6180 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      420 | 6181 | `	for(;;){` |
|     1049 | 6182 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      177 | 6183 | `			pGen->pIn++;` |
|        5 | 6184 | `		}` |
|      877 | 6185 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      345 | 6186 | `			break;` |
|        - | 6187 | `		}` |
|        - | 6188 | `		/* Bind a directly-preceding docblock to this member */` |
|      537 | 6189 | `		GenStateSetPendingDoc(&(*pGen));` |
|      532 | 6190 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|      271 | 6191 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|      ! 0 | 6192 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|        - | 6193 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|      ! 0 | 6194 | `				&pGen->pIn->sData,pName);` |
|      ! 0 | 6195 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 6196 | `				return SXERR_ABORT;` |
|        - | 6197 | `			}` |
|      ! 0 | 6198 | `			goto done;` |
|        - | 6199 | `		}` |
|      537 | 6200 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      537 | 6201 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      537 | 6202 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|        - | 6203 | `				/* Trait uses another trait: use T[, T2] [{ resolution }]; A trait` |
|        - | 6204 | `				 * name is a full class reference — qualified or fully-qualified` |
|        - | 6205 | ``				 * (`use Foo\T;`, `use \Foo\T;`) — so parse it with the shared`` |
|        - | 6206 | `				 * class-reference reader like the CLASS body's trait-use does` |
|        - | 6207 | `				 * (the old single-identifier read choked on the leading '\'),` |
|        - | 6208 | `				 * and collect a TraitUseEntry so an adaptation block` |
|        - | 6209 | `				 * (insteadof/as) applies through the same shared machinery. */` |
|        - | 6210 | `				TraitUseEntry sUse;` |
|       35 | 6211 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|       35 | 6212 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|       35 | 6213 | `				pGen->pIn++; /* Jump 'use' */` |
|       20 | 6214 | `				for(;;){` |
|        - | 6215 | `					ph7_class *pUsedTrait;` |
|        - | 6216 | `					SyBlob sResolved;` |
|        - | 6217 | `					SyString sUsedName;` |
|       39 | 6218 | `					sxu32 nUseLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|       39 | 6219 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       39 | 6220 | `					if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 6221 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 6222 | `						rc = PH7_GenCompileError(pGen,E_PARSE,nUseLine,` |
|      ! 0 | 6223 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|      ! 0 | 6224 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 6225 | `							return SXERR_ABORT;` |
|        - | 6226 | `						}` |
|      ! 0 | 6227 | `						break;` |
|        - | 6228 | `					}` |
|       75 | 6229 | `					pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       36 | 6230 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       39 | 6231 | `					SyStringInitFromBuf(&sUsedName,` |
|        - | 6232 | `						(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       39 | 6233 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|      ! 0 | 6234 | `						pUsedTrait = pUsedTrait->pNextName;` |
|      ! 0 | 6235 | `					}` |
|       39 | 6236 | `					if( pUsedTrait == 0 ){` |
|      ! 0 | 6237 | `						if( !GenStateLintSkipsBase(pGen,pClass) ){` |
|      ! 0 | 6238 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|        - | 6239 | `								"'%z' is not a trait",&sUsedName);` |
|      ! 0 | 6240 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 6241 | `								SyBlobRelease(&sResolved);` |
|      ! 0 | 6242 | `								return SXERR_ABORT;` |
|        - | 6243 | `							}` |
|      ! 0 | 6244 | `						}` |
|      ! 0 | 6245 | `					}else{` |
|       39 | 6246 | `						SySetPut(&sUse.aTraits,(const void *)&pUsedTrait);` |
|        - | 6247 | `					}` |
|       39 | 6248 | `					SyBlobRelease(&sResolved);` |
|       39 | 6249 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       19 | 6250 | `						break;` |
|        - | 6251 | `					}` |
|        6 | 6252 | `					pGen->pIn++;` |
|        2 | 6253 | `				}` |
|        - | 6254 | `				/* Optional adaptation block (conflict resolution) */` |
|       35 | 6255 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|        - | 6256 | `					SyToken *pBlock;` |
|       10 | 6257 | `					pGen->pIn++; /* Jump '{' */` |
|       10 | 6258 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       10 | 6259 | `					sUse.pResolvStart = pGen->pIn;` |
|       10 | 6260 | `					sUse.pResolvEnd = pBlock;` |
|       10 | 6261 | `					if( pBlock < pGen->pEnd ){` |
|       10 | 6262 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|        6 | 6263 | `					}else{` |
|      ! 0 | 6264 | `						pGen->pIn = pGen->pEnd;` |
|        - | 6265 | `					}` |
|        4 | 6266 | `				}` |
|       35 | 6267 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       35 | 6268 | `				continue;` |
|        - | 6269 | `			}` |
|      250 | 6270 | `		}` |
|        - | 6271 | `		/* Everything else is a MEMBER: its modifier run and the declaration it` |
|        - | 6272 | `		 * modifies, read by the same code a class body uses. */` |
|      505 | 6273 | `		rc = GenStateCompileMember(&(*pGen),pClass,"trait");` |
|      505 | 6274 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 6275 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 6276 | `				return SXERR_ABORT;` |
|        - | 6277 | `			}` |
|      ! 0 | 6278 | `			goto done;` |
|        - | 6279 | `		}` |
|        5 | 6280 | `	}` |
|        - | 6281 | ``	/* Apply the collected `use` entries (incl. adaptation blocks) through the`` |
|        - | 6282 | `	 * machinery shared with the class-body compiler. */` |
|      345 | 6283 | `	rc = GenStateApplyTraitUses(&(*pGen),pClass,&aUseEntries);` |
|      345 | 6284 | `	SySetRelease(&aUseEntries);` |
|      345 | 6285 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 6286 | `		return SXERR_ABORT;` |
|        - | 6287 | `	}` |
|        - | 6288 | `	/* Reject a php-fatal redeclaration before hoisting the trait. php early-binds` |
|        - | 6289 | `	 * a trait like a plain class -- it has nothing left to link. */` |
|      345 | 6290 | `	if( GenStateGuardClassRedeclaration(pGen,pClass,1) == SXERR_ABORT ){` |
|        3 | 6291 | `		return SXERR_ABORT;` |
|        - | 6292 | `	}` |
|        - | 6293 | `	/* Install the trait */` |
|      343 | 6294 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      343 | 6295 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 6296 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 6297 | `		return SXERR_ABORT;` |
|        - | 6298 | `	}` |
|      169 | 6299 | `done:` |
|      343 | 6300 | `	pGen->pCurClass = pSavedCurClass;` |
|      343 | 6301 | `	pGen->pCurBase = pSavedCurBase;` |
|        - | 6302 | `	/* Point beyond the trait body */` |
|      343 | 6303 | `	pGen->pIn = &pEnd[1];` |
|      343 | 6304 | `	pGen->pEnd = pTmp;` |
|      343 | 6305 | `	return PH7_OK;` |
|      183 | 6306 | `}` |
|        - | 6307 | `/*` |
|        - | 6308 | ` * Compile a user-defined class.` |
|        - | 6309 | ` *  According to the PHP language reference manual` |
|        - | 6310 | ` *   Basic class definitions begin with the keyword class, followed` |
|        - | 6311 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|        - | 6312 | ` *   the definitions of the properties and methods belonging to the class.` |
|        - | 6313 | ` *   A class may contain its own constants, variables (called "properties")` |
|        - | 6314 | ` *   and functions (called "methods").` |
|        - | 6315 | ` */` |
|     4808 | 6316 | `PH7_PRIVATE sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|        5 | 6317 | `{` |
|        - | 6318 | `	sxi32 rc;` |
|     4813 | 6319 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|     4813 | 6320 | `	return rc;` |
|        5 | 6321 | `}` |
|        - | 6322 | `/*` |
|        - | 6323 | ` * Return TRUE if the token stream starts an enum declaration (PHP 8.1):` |
|        - | 6324 | `` * the context-sensitive identifier `enum` (not a reserved word — it stays`` |
|        - | 6325 | `` * valid as a function/constant name, like `readonly`) directly followed by`` |
|        - | 6326 | `` * an identifier. `enum(...)`/`enum;`/`$enum` all keep their expression`` |
|        - | 6327 | `` * meaning; `enum Name` can never start a valid expression.`` |
|        - | 6328 | ` */` |
|  2430870 | 6329 | `PH7_PRIVATE int GenStateStartsEnumDecl(SyToken *pIn,SyToken *pEnd)` |
|        5 | 6330 | `{` |
|  2505561 | 6331 | `	return (pIn->nType & PH7_TK_ID)` |
|  1288779 | 6332 | `		&& pIn->sData.nByte == sizeof("enum")-1` |
|    91531 | 6333 | `		&& SyStrnicmp(pIn->sData.zString,"enum",sizeof("enum")-1) == 0` |
|  2506141 | 6334 | `		&& &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_ID);` |
|        5 | 6335 | `}` |
|        - | 6336 | `/*` |
|        - | 6337 | ` * Return TRUE when the token stream starts a CLOSURE EXPRESSION at statement` |
|        - | 6338 | `` * position: `function () {…};` or `fn (…) => …;`, with an optional `&` for a`` |
|        - | 6339 | ` * by-reference return.` |
|        - | 6340 | ` *` |
|        - | 6341 | ` * php compiles both as ordinary expression statements — the closure is built and` |
|        - | 6342 | ` * discarded — and 176 files across the vendor trees write one. PHL sent the bare` |
|        - | 6343 | `` * `function` keyword to PH7_CompileFunction, which demands a NAME and answered`` |
|        - | 6344 | `` * `syntax error, unexpected token "(", expecting "("`; `fn` was not a statement`` |
|        - | 6345 | `` * keyword at all and answered `Unexpected keyword 'fn'`. The `static` forms`` |
|        - | 6346 | `` * (`static function () {};`, `static fn () => 1;`) already worked, which is what`` |
|        - | 6347 | ` * made this look narrower than it was.` |
|        - | 6348 | ` *` |
|        - | 6349 | `` * The NAMED declaration is what must not be caught here, and the `(` is what`` |
|        - | 6350 | `` * tells them apart: `function foo(` has an identifier where a closure has its`` |
|        - | 6351 | `` * parameter list. php refuses an immediately-invoked `function () {}()` at`` |
|        - | 6352 | ` * statement position too, so nothing here tries to accept one.` |
|        - | 6353 | ` */` |
|  2430356 | 6354 | `PH7_PRIVATE int GenStateStartsClosureExpr(SyToken *pIn,SyToken *pEnd)` |
|        5 | 6355 | `{` |
|  2430361 | 6356 | `	if( (pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   996235 | 6357 | `		return 0;` |
|        - | 6358 | `	}` |
|        - | 6359 | `	{` |
|  1434131 | 6360 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|  1434131 | 6361 | `		if( nKw != PH7_TKWRD_FUNCTION && nKw != PH7_TKWRD_FN ){` |
|  1255431 | 6362 | `			return 0;` |
|        - | 6363 | `		}` |
|        - | 6364 | `	}` |
|   178705 | 6365 | `	pIn++;` |
|   178705 | 6366 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_AMPER) ){` |
|       38 | 6367 | ``		pIn++;   /* `function &() {…}` returns by reference */`` |
|       17 | 6368 | `	}` |
|   178705 | 6369 | `	return pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) != 0;` |
|  1213256 | 6370 | `}` |
|        - | 6371 | `/*` |
|        - | 6372 | ` * Compile an enum declaration (PHP 8.1). An enum is a final class carrying` |
|        - | 6373 | `` * PH7_CLASS_ENUM: `case` members become lazily-materialized singleton`` |
|        - | 6374 | ` * constants, cases()/from()/tryFrom() are synthesized, and UnitEnum/BackedEnum` |
|        - | 6375 | ` * are implemented implicitly (GenStateCompileClassEx handles the specifics).` |
|        - | 6376 | ` */` |
|      140 | 6377 | `PH7_PRIVATE sxi32 PH7_CompileEnum(ph7_gen_state *pGen)` |
|        5 | 6378 | `{` |
|      145 | 6379 | `	return GenStateCompileClass(&(*pGen),PH7_CLASS_ENUM\|PH7_CLASS_FINAL);` |
|        5 | 6380 | `}` |
|        - | 6381 |  |
