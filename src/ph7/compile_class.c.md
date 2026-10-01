# src/ph7/compile_class.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3003/3641 lines (82.48%)

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
|      16 |   19 | `static const char * GenStateClassKind(const ph7_class *pClass)` |
|       4 |   20 | `{` |
|      20 |   21 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){ return "interface"; }` |
|      18 |   22 | `	if( pClass->iFlags & PH7_CLASS_TRAIT ){ return "trait"; }` |
|      15 |   23 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){ return "enum"; }` |
|      13 |   24 | `	return "class";` |
|      12 |   25 | `}` |
|       - |   26 | `/*` |
|       - |   27 | ` * Guard a class/interface/trait/enum about to be installed. Returns SXERR_ABORT` |
|       - |   28 | ` * (after emitting the fatal) if it redeclares an already-bound type; otherwise` |
|       - |   29 | ` * marks it bound (when unconditional & top-level) and returns SXRET_OK.` |
|       - |   30 | ` */` |
|       - |   31 | `/*` |
|       - |   32 | ` * Does this earlier declaration hold the name against the one being compiled?` |
|       - |   33 | ` *` |
|       - |   34 | ` * A class php EARLY-BINDS holds it from the moment the file compiles, so` |
|       - |   35 | ` * anything else under that name is a redeclaration wherever it sits. One php` |
|       - |   36 | ` * declares at RUN time (it implements an interface, uses a trait, is an enum,` |
|       - |   37 | ` * or declares __toString and so implicitly implements Stringable) holds it only` |
|       - |   38 | ` * once its own statement has run -- which, within one file, means only against` |
|       - |   39 | ` * a declaration that comes AFTER it. That is the whole of the "polyfill" shape:` |
|       - |   40 | ` *` |
|       - |   41 | ` *     if (PHP_VERSION_ID >= 80000) { class T extends PhpToken {} return; }` |
|       - |   42 | ` *     class T { public function __toString(): string { ... } }` |
|       - |   43 | ` *` |
|       - |   44 | ` * php runs the first branch, returns, and never reaches the second -- so the` |
|       - |   45 | ` * second never takes the name. Reading the two declarations' ORDER is how this` |
|       - |   46 | ` * compiler tells that apart without a runtime declaration of its own.` |
|       - |   47 | ` */` |
|      16 |   48 | `static int GenStateDeclHoldsName(ph7_gen_state *pGen,ph7_class *pPrev,ph7_class *pClass)` |
|       4 |   49 | `{` |
|       - |   50 | `	SyString *pFile;` |
|      20 |   51 | `	if( pPrev->iFlags & PH7_CLASS_BOUND ){` |
|      18 |   52 | `		return 1;   /* early-bound: the name is taken before anything runs */` |
|       - |   53 | `	}` |
|       3 |   54 | `	if( (pPrev->iFlags & PH7_CLASS_TOPLEVEL) == 0 ){` |
|     ! 0 |   55 | `		return 0;   /* conditional: it may never run at all */` |
|       - |   56 | `	}` |
|       3 |   57 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|       2 |   58 | `	if( pFile && pPrev->sFile.nByte == pFile->nByte` |
|       3 |   59 | `	 && SyMemcmp(pPrev->sFile.zString,pFile->zString,pFile->nByte) == 0 ){` |
|       - |   60 | `		/* Same file: it holds the name only if it is written FIRST. */` |
|       3 |   61 | `		return pPrev->nLine <= pClass->nLine;` |
|       - |   62 | `	}` |
|     ! 0 |   63 | `	return 1;   /* another file, already loaded: it has run */` |
|      12 |   64 | `}` |
|       - |   65 | `/*` |
|       - |   66 | `` * A `phl -l` compile PARSES; it does not BIND. php binds inheritance at run time,`` |
|       - |   67 | `` * so `php -l` says nothing about a parent, an interface or a trait it cannot see --`` |
|       - |   68 | ` * and under -l nothing autoloads, so it can see almost none of them. TRUE means` |
|       - |   69 | ` * "say nothing and carry on with no base", which is what leaves the BODY to be` |
|       - |   70 | ` * compiled: the whole point of the mode is that a syntax error in there is found.` |
|       - |   71 | ` */` |
|      20 |   72 | `static int GenStateLintSkipsBase(ph7_gen_state *pGen,ph7_class *pClass)` |
|     ! 0 |   73 | `{` |
|      20 |   74 | `	if( pGen->pVm->bSyntaxCheck == 0 ){` |
|     ! 0 |   75 | `		return 0;` |
|       - |   76 | `	}` |
|      20 |   77 | `	if( pClass ){` |
|      20 |   78 | `		pClass->iFlags \|= PH7_CLASS_LINT_UNBOUND;` |
|      10 |   79 | `	}` |
|      20 |   80 | `	return 1;` |
|      10 |   81 | `}` |
|    5534 |   82 | `static sxi32 GenStateGuardClassRedeclaration(ph7_gen_state *pGen,ph7_class *pClass,` |
|       - |   83 | `	int bEarlyBindable)` |
|       5 |   84 | `{` |
|       - |   85 | `	SyHashEntry *pEntry;` |
|    5539 |   86 | `	int bTopLevel = GenStateUnconditionalTopLevel(pGen);` |
|    5539 |   87 | `	if( pGen->pVm->bSyntaxCheck ){` |
|       - |   88 | ``		/* `php -l` reports a redeclared FUNCTION and not a redeclared CLASS: a`` |
|       - |   89 | `		 * class name is taken at the DECLARE_CLASS opcode, which lint never runs.` |
|       - |   90 | ``		 * So `class DateTime {}` and symfony/polyfill-php80's stub `final class`` |
|       - |   91 | ``		 * Attribute` -- a file composer only ever loads under php 7 -- both lint`` |
|       - |   92 | `		 * clean there, and this refused them. */` |
|      72 |   93 | `		return SXRET_OK;` |
|       - |   94 | `	}` |
|    5467 |   95 | `	if( bTopLevel ){` |
|       - |   96 | `		/* php RUNS this declaration whatever else the file holds, early bound or` |
|       - |   97 | `		 * not -- so two of them under one name collide even when neither was. */` |
|    5373 |   98 | `		pClass->iFlags \|= PH7_CLASS_TOPLEVEL;` |
|    2684 |   99 | `	}` |
|    5467 |  100 | `	if( bTopLevel && !bEarlyBindable ){` |
|       - |  101 | `		/* Not early-bound, but it RUNS: only another declaration that also runs` |
|       - |  102 | `		 * unconditionally collides with it. */` |
|    1457 |  103 | `		SyHashEntry *pTop = SyHashGet(&pGen->pVm->hClass,` |
|     968 |  104 | `			(const void *)pClass->sName.zString,pClass->sName.nByte);` |
|     973 |  105 | `		if( pTop ){` |
|       3 |  106 | `			ph7_class *pPrev = (ph7_class *)pTop->pUserData;` |
|       3 |  107 | `			while( pPrev ){` |
|       3 |  108 | `				if( GenStateDeclHoldsName(pGen,pPrev,pClass) ){` |
|       3 |  109 | `					pGen->iFatalTrace = PH7_FATAL_TRACE_RUNTIME;` |
|       3 |  110 | `					if( pPrev->sFile.nByte > 0 ){` |
|       4 |  111 | `						PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,` |
|       - |  112 | `							"Cannot redeclare %s %z (previously declared in %.*s:%u)",` |
|       1 |  113 | `							GenStateClassKind(pPrev),&pClass->sName,` |
|       1 |  114 | `							pPrev->sFile.nByte,pPrev->sFile.zString,pPrev->nLine);` |
|       2 |  115 | `					}else{` |
|     ! 0 |  116 | `						PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,` |
|     ! 0 |  117 | `							"Cannot redeclare %s %z",GenStateClassKind(pPrev),&pClass->sName);` |
|       - |  118 | `					}` |
|       3 |  119 | `					return SXERR_ABORT;` |
|       - |  120 | `				}` |
|     ! 0 |  121 | `				pPrev = pPrev->pNextName;` |
|     ! 0 |  122 | `			}` |
|     ! 0 |  123 | `		}` |
|     971 |  124 | `		return SXRET_OK;` |
|       - |  125 | `	}` |
|    4499 |  126 | `	if( !bTopLevel ){` |
|       - |  127 | `		/* Conditional, nested -- or one php would not EARLY-BIND either, which` |
|       - |  128 | `		 * is the same thing for this guard: such a declaration only takes effect` |
|       - |  129 | `		 * when its statement runs, so another declaration of the name is not a` |
|       - |  130 | `		 * redeclaration of it. php early-binds only a class it can link with` |
|       - |  131 | `		 * nothing left over, so an implemented INTERFACE, a used TRAIT, an enum` |
|       - |  132 | ``		 * or a `__toString()` (which brings Stringable with it) all rule it out.`` |
|       - |  133 | `		 * Binding one of those at compile time made the polyfill shape above a` |
|       - |  134 | `		 * redeclaration, and phpunit.phar died on it. */` |
|      98 |  135 | `		return SXRET_OK;` |
|       - |  136 | `	}` |
|    4405 |  137 | `	pClass->iFlags \|= PH7_CLASS_BOUND;` |
|    4405 |  138 | `	if( pGen->pVm->bCompilingBuiltin ){` |
|     ! 0 |  139 | `		return SXRET_OK; /* the prelude installs each builtin exactly once */` |
|       - |  140 | `	}` |
|    4405 |  141 | `	pEntry = SyHashGet(&pGen->pVm->hClass,(const void *)pClass->sName.zString,pClass->sName.nByte);` |
|    4405 |  142 | `	if( pEntry ){` |
|      18 |  143 | `		ph7_class *pPrev = (ph7_class *)pEntry->pUserData;` |
|      18 |  144 | `		while( pPrev ){` |
|      18 |  145 | `			if( GenStateDeclHoldsName(pGen,pPrev,pClass) ){` |
|       - |  146 | `				/* php cannot early-bind a name it already holds, so THIS refusal comes` |
|       - |  147 | `				 * from the DECLARE_CLASS opcode at run time -- and its stack trace` |
|       - |  148 | `				 * carries the include/require that loaded the unit, where every other` |
|       - |  149 | `				 * compile-time refusal's does not. */` |
|      18 |  150 | `				pGen->iFatalTrace = PH7_FATAL_TRACE_RUNTIME;` |
|       - |  151 | `				/* php names the entity by the PREVIOUS declaration's kind and omits` |
|       - |  152 | `				 * the "(previously declared in ...)" clause for internal symbols. */` |
|      18 |  153 | `				if( pPrev->sFile.nByte > 0 ){` |
|      22 |  154 | `					PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,` |
|       - |  155 | `						"Cannot redeclare %s %z (previously declared in %.*s:%u)",` |
|       6 |  156 | `						GenStateClassKind(pPrev),&pClass->sName,` |
|       6 |  157 | `						pPrev->sFile.nByte,pPrev->sFile.zString,pPrev->nLine);` |
|      10 |  158 | `				}else{` |
|       4 |  159 | `					PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,` |
|       1 |  160 | `						"Cannot redeclare %s %z",GenStateClassKind(pPrev),&pClass->sName);` |
|       - |  161 | `				}` |
|      18 |  162 | `				return SXERR_ABORT;` |
|       - |  163 | `			}` |
|     ! 0 |  164 | `			pPrev = pPrev->pNextName;` |
|     ! 0 |  165 | `		}` |
|     ! 0 |  166 | `	}` |
|    4391 |  167 | `	return SXRET_OK;` |
|    2772 |  168 | `}` |
|       - |  169 | `/*` |
|       - |  170 | ` * Extract the visibility level associated with a given keyword.` |
|       - |  171 | ` * According to the PHP language reference manual` |
|       - |  172 | ` *  Visibility:` |
|       - |  173 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - |  174 | ` *  the declaration with the keywords public, protected or private.` |
|       - |  175 | ` *  Class members declared public can be accessed everywhere.` |
|       - |  176 | ` *  Members declared protected can be accessed only within the class` |
|       - |  177 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - |  178 | ` *  may only be accessed by the class that defines the member.` |
|       - |  179 | ` */` |
|    8148 |  180 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       5 |  181 | `{` |
|    8153 |  182 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|     571 |  183 | `		return PH7_CLASS_PROT_PRIVATE;` |
|    7587 |  184 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|     239 |  185 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - |  186 | `	}` |
|       - |  187 | `	/* Assume public by default */` |
|    7353 |  188 | `	return PH7_CLASS_PROT_PUBLIC;` |
|    4079 |  189 | `}` |
|       - |  190 | `/*` |
|       - |  191 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|       - |  192 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|       - |  193 | ` * token immediately followed by '='. Anything else with a leading type token` |
|       - |  194 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|       - |  195 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|       - |  196 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|       - |  197 | ` */` |
|     664 |  198 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|       5 |  199 | `{` |
|       - |  200 | `	SyToken *p0, *p1;` |
|     669 |  201 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  202 | `		return 0;` |
|       - |  203 | `	}` |
|     669 |  204 | `	p0 = pGen->pIn;` |
|       - |  205 | `	/* A leading '\' (namespaced class type), '?' (nullable) or '(' (a DNF type's` |
|       - |  206 | `	 * first intersection group) always starts a type -- none of the three can begin` |
|       - |  207 | `	 * a constant NAME. */` |
|     669 |  208 | `	if( p0->nType & (PH7_TK_NSSEP\|PH7_TK_LPAREN) ){` |
|       3 |  209 | `		return 1;` |
|       - |  210 | `	}` |
|     667 |  211 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|       5 |  212 | `		return 1;` |
|       - |  213 | `	}` |
|       - |  214 | `	/* A name-like first token begins a type only when followed by another` |
|       - |  215 | `	 * name (the constant name), a union separator '\|' or an intersection '&'.` |
|       - |  216 | `	 * Followed by '=', ';' or ',' it is the constant name itself (untyped).` |
|       - |  217 | `	 * Without the '&' a typed constant whose type is an INTERSECTION was read as` |
|       - |  218 | `	 * an untyped one named after the first member, and refused with` |
|       - |  219 | `	 * "Expected '=' after class constant Countable". */` |
|     663 |  220 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     663 |  221 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|     663 |  222 | `		if( p1 ){` |
|     663 |  223 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP\|PH7_TK_AMPER) ){` |
|      53 |  224 | `				return 1;` |
|       - |  225 | `			}` |
|     610 |  226 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1` |
|     615 |  227 | `			 && (p1->sData.zString[0] == '\|' \|\| p1->sData.zString[0] == '&') ){` |
|       5 |  228 | `				return 1;` |
|       - |  229 | `			}` |
|     303 |  230 | `		}` |
|     303 |  231 | `	}` |
|     611 |  232 | `	return 0;` |
|     337 |  233 | `}` |
|       - |  234 | `/*` |
|       - |  235 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|       - |  236 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|       - |  237 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|       - |  238 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|       - |  239 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|       - |  240 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|       - |  241 | ` * Peek only; never consumes tokens.` |
|       - |  242 | ` */` |
|      36 |  243 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|       4 |  244 | `{` |
|      40 |  245 | `	SyToken *p = pGen->pIn;` |
|      57 |  246 | `	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      26 |  247 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|       3 |  248 | `		p++; /* skip leading unary sign(s) */` |
|       1 |  249 | `	}` |
|      40 |  250 | `	if( p >= pGen->pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|      36 |  251 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|       - |  252 | `	}` |
|       6 |  253 | `	p++;` |
|       - |  254 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|       6 |  255 | `	return ( p >= pGen->pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|      22 |  256 | `}` |
|       - |  257 | `/*` |
|       - |  258 | `` * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).`` |
|       - |  259 | `` * A `new` that immediately follows one of these is a member name (`A::new`,`` |
|       - |  260 | `` * `$o->new`), not a `new` expression.`` |
|       - |  261 | ` */` |
|     252 |  262 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|       5 |  263 | `{` |
|       - |  264 | `	sxi32 iOp;` |
|     257 |  265 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|      93 |  266 | `		return 0;` |
|       - |  267 | `	}` |
|     167 |  268 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|     167 |  269 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|     131 |  270 | `}` |
|       - |  271 | `/*` |
|       - |  272 | ` * Skip the whole closure / arrow-function construct beginning at *pp, which is` |
|       - |  273 | `` * positioned on the `function` / `fn` keyword. Its body is ordinary runtime code,`` |
|       - |  274 | ` * so none of the constant-expression rules below reach into it. *piDepth is the` |
|       - |  275 | ` * caller's bracket depth: an arrow function has no braces of its own and ends at` |
|       - |  276 | `` * a `,`/`;` or at a bracket closing an ENCLOSING group, so it shares that depth,`` |
|       - |  277 | `` * while a `function(){...}` body is brace-balanced and walks its own.`` |
|       - |  278 | ` */` |
|     192 |  279 | `static void GenStateInitSkipFuncConstruct(SyToken **pp,SyToken *pEnd,int *piDepth)` |
|       5 |  280 | `{` |
|     197 |  281 | `	SyToken *p = *pp;` |
|     197 |  282 | `	int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|     197 |  283 | `	int iBase = *piDepth;` |
|     197 |  284 | `	p++;` |
|     197 |  285 | `	if( bArrow ){` |
|       - |  286 | `		/* fn(params) => expr : skip to the end of the current element. */` |
|      86 |  287 | `		while( p < pEnd ){` |
|      78 |  288 | `			if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      14 |  289 | `				(*piDepth)++;` |
|      72 |  290 | `			}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      14 |  291 | `				if( *piDepth <= iBase ){` |
|     ! 0 |  292 | `					break; /* closes an enclosing group, not the fn's own */` |
|       - |  293 | `				}` |
|      14 |  294 | `				(*piDepth)--;` |
|      60 |  295 | `			}else if( *piDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|       6 |  296 | `				break;` |
|       - |  297 | `			}` |
|      74 |  298 | `			p++;` |
|       2 |  299 | `		}` |
|       8 |  300 | `	}else{` |
|       - |  301 | `		/* function(params)[use(...)][: type] { body } : skip the signature up to the` |
|       - |  302 | ``		 * body '{' (a '{' at closure-local depth 0, so a `new class{}` default inside`` |
|       - |  303 | `		 * the parens is not mistaken for it), then the balanced brace block. */` |
|     185 |  304 | `		int iLocal = 0;` |
|     545 |  305 | `		while( p < pEnd ){` |
|     545 |  306 | `			if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|     185 |  307 | `				break; /* body brace */` |
|       - |  308 | `			}` |
|     365 |  309 | `			if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     185 |  310 | `				iLocal++;` |
|     275 |  311 | `			}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     185 |  312 | `				if( iLocal > 0 ){` |
|     185 |  313 | `					iLocal--;` |
|      90 |  314 | `				}` |
|      90 |  315 | `			}` |
|     365 |  316 | `			p++;` |
|       5 |  317 | `		}` |
|     185 |  318 | `		if( p < pEnd ){` |
|     185 |  319 | `			int iBrace = 0; /* p is on the body '{' */` |
|    1217 |  320 | `			while( p < pEnd ){` |
|    1217 |  321 | `				if( p->nType & PH7_TK_OCB ){` |
|     205 |  322 | `					iBrace++;` |
|    1117 |  323 | `				}else if( p->nType & PH7_TK_CCB ){` |
|     205 |  324 | `					iBrace--;` |
|     205 |  325 | `					if( iBrace == 0 ){` |
|     185 |  326 | `						p++;` |
|     185 |  327 | `						break;` |
|       - |  328 | `					}` |
|      10 |  329 | `				}` |
|    1037 |  330 | `				p++;` |
|       5 |  331 | `			}` |
|      90 |  332 | `		}` |
|       - |  333 | `	}` |
|     197 |  334 | `	*pp = p;` |
|     197 |  335 | `}` |
|       - |  336 | `/*` |
|       - |  337 | `` * TRUE if *p is the `::` operator.`` |
|       - |  338 | ` */` |
|   59976 |  339 | `static int GenStateTokenIsDoubleColon(const SyToken *p)` |
|       5 |  340 | `{` |
|   38661 |  341 | `	return ( (p->nType & PH7_TK_OP) && p->pUserData` |
|   64324 |  342 | `		&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_DC );` |
|       5 |  343 | `}` |
|       - |  344 | `/*` |
|       - |  345 | `` * Where the constant expression starting at *pStart ends: the first `,` or `;` at`` |
|       - |  346 | `` * bracket depth 0, or the first depth-0 `{`, which in a property declaration can`` |
|       - |  347 | `` * only open a PHP 8.4 hook list (`public T $x = default { get …; }`) and never`` |
|       - |  348 | ` * belongs to the default itself.` |
|       - |  349 | ` */` |
|   44120 |  350 | `static SyToken * GenStateConstExprEnd(SyToken *pStart,SyToken *pEnd)` |
|       5 |  351 | `{` |
|   44125 |  352 | `	SyToken *p = pStart;` |
|   44125 |  353 | `	int iDepth = 0;` |
|  104347 |  354 | `	while( p < pEnd ){` |
|   63020 |  355 | `		if( (p->nType & PH7_TK_KEYWORD) && (p->nType & PH7_TK_MEMBER_NAME) == 0` |
|     235 |  356 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|     208 |  357 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|       - |  358 | `			/* A closure's BODY brace is not a hook list: skip the construct whole,` |
|       - |  359 | ``			 * or `$c = static function(){}` would end the expression at that brace`` |
|       - |  360 | `			 * and hide everything after it from the rules. */` |
|      53 |  361 | `			GenStateInitSkipFuncConstruct(&p,pEnd,&iDepth);` |
|      53 |  362 | `			continue;` |
|       - |  363 | `		}` |
|   62977 |  364 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA\|PH7_TK_OCB)) ){` |
|    2803 |  365 | `			break;` |
|       - |  366 | `		}` |
|   60179 |  367 | `		if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     471 |  368 | `			iDepth++;` |
|   59946 |  369 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     471 |  370 | `			if( iDepth > 0 ){` |
|     471 |  371 | `				iDepth--;` |
|     233 |  372 | `			}` |
|     233 |  373 | `		}` |
|   60179 |  374 | `		p++;` |
|       5 |  375 | `	}` |
|   44125 |  376 | `	return p;` |
|       5 |  377 | `}` |
|       - |  378 | `/*` |
|       - |  379 | ` * Decide a ternary CONDITION php would have folded: 1 truthy, 0 falsy, -1 "cannot` |
|       - |  380 | ` * tell". Only a lone literal (with any number of redundant parens around it) is` |
|       - |  381 | `` * decided here. php's own folder reaches much further -- it evaluates `1 === 1`,`` |
|       - |  382 | `` * `PHP_INT_SIZE === 8`, even `[1,2][0]` -- and everything it cannot fold at compile`` |
|       - |  383 | ` * time, a user constant among them, it leaves for the rule walk to refuse. The` |
|       - |  384 | ` * caller answers -1 by leaving the whole expression alone, which is the safe` |
|       - |  385 | ` * direction: an offender php would have refused stays accepted, and no valid` |
|       - |  386 | ` * program is refused on a branch php would have dropped.` |
|       - |  387 | ` */` |
|      22 |  388 | `static int GenStateConstExprTruth(SyToken *pStart,SyToken *pStop)` |
|       2 |  389 | `{` |
|      24 |  390 | `	SyToken *p = pStart, *q = pStop;` |
|      24 |  391 | `	while( p < q && (p->nType & PH7_TK_LPAREN) && (q[-1].nType & PH7_TK_RPAREN) ){` |
|     ! 0 |  392 | `		p++;` |
|     ! 0 |  393 | `		q--;` |
|     ! 0 |  394 | `	}` |
|      24 |  395 | `	if( &p[1] != q ){` |
|     ! 0 |  396 | `		return -1; /* not a lone token */` |
|       - |  397 | `	}` |
|      24 |  398 | `	if( p->nType & PH7_TK_ID ){` |
|       - |  399 | ``		/* `true`/`false`/`null` are not lexer keywords here -- they arrive as plain`` |
|       - |  400 | `		 * identifiers and are recognised by name (php reserves all three, so no user` |
|       - |  401 | `		 * constant can shadow one). Any OTHER name is a constant whose value this` |
|       - |  402 | `		 * stage does not know, which is exactly where php stops folding too. */` |
|      24 |  403 | `		if( p->sData.nByte == 4 && SyStrnicmp(p->sData.zString,"true",4) == 0 ){` |
|      16 |  404 | `			return 1;` |
|       - |  405 | `		}` |
|       8 |  406 | `		if( (p->sData.nByte == 5 && SyStrnicmp(p->sData.zString,"false",5) == 0)` |
|       5 |  407 | `			\|\| (p->sData.nByte == 4 && SyStrnicmp(p->sData.zString,"null",4) == 0) ){` |
|       9 |  408 | `			return 0;` |
|       - |  409 | `		}` |
|     ! 0 |  410 | `		return -1;` |
|       - |  411 | `	}` |
|     ! 0 |  412 | `	if( p->nType & (PH7_TK_INTEGER\|PH7_TK_REAL) ){` |
|       - |  413 | `		/* php's truthiness: 0 and 0.0 are false, everything else true. */` |
|     ! 0 |  414 | `		const char *z = p->sData.zString;` |
|     ! 0 |  415 | `		sxu32 n = p->sData.nByte, i;` |
|     ! 0 |  416 | `		for( i = 0 ; i < n ; ++i ){` |
|     ! 0 |  417 | `			if( z[i] != '0' && z[i] != '.' && z[i] != '+' && z[i] != '-' ){` |
|     ! 0 |  418 | `				return 1;` |
|       - |  419 | `			}` |
|     ! 0 |  420 | `		}` |
|     ! 0 |  421 | `		return 0;` |
|       - |  422 | `	}` |
|     ! 0 |  423 | `	if( p->nType & PH7_TK_DSTR ){` |
|       - |  424 | `		/* A double-quoted literal may interpolate; only a single-quoted one is` |
|       - |  425 | `		 * certainly its own text. Leave the rest undecided. */` |
|     ! 0 |  426 | `		return -1;` |
|       - |  427 | `	}` |
|     ! 0 |  428 | `	if( p->nType & PH7_TK_SSTR ){` |
|     ! 0 |  429 | `		return ( p->sData.nByte == 0` |
|     ! 0 |  430 | `			\|\| (p->sData.nByte == 1 && p->sData.zString[0] == '0') ) ? 0 : 1;` |
|       - |  431 | `	}` |
|     ! 0 |  432 | `	return -1;` |
|      13 |  433 | `}` |
|       - |  434 | `static const char * GenStateConstExprSpan(SyToken *pStart,SyToken *pStop,int bAllowNew,int nDepth);` |
|       - |  435 | `/*` |
|       - |  436 | `` * TRUE if a `?` appears anywhere in the span outside a closure body. Used only`` |
|       - |  437 | ` * after the top-level ternary has been ruled out: a ternary NESTED in a bracket` |
|       - |  438 | `` * (`[true ? 1 : new X]`, `(true ? 1 : strlen('a')) + 1`) is folded by php just the`` |
|       - |  439 | ` * same, but the split below cannot say where its condition begins, so the span is` |
|       - |  440 | `` * left alone rather than have a dropped branch refuse a valid program. A `?` in a`` |
|       - |  441 | `` * closure body is runtime code and does not count; `?->` and `??` are their own`` |
|       - |  442 | ` * operators and never land here.` |
|       - |  443 | ` */` |
|   44142 |  444 | `static int GenStateSpanHasNestedTernary(SyToken *pStart,SyToken *pStop)` |
|       5 |  445 | `{` |
|   44147 |  446 | `	SyToken *p = pStart;` |
|   44147 |  447 | `	int iDepth = 0;` |
|  104225 |  448 | `	while( p < pStop ){` |
|   60086 |  449 | `		if( (p->nType & PH7_TK_KEYWORD) && (p->nType & PH7_TK_MEMBER_NAME) == 0` |
|     223 |  450 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|     200 |  451 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|      45 |  452 | `			GenStateInitSkipFuncConstruct(&p,pStop,&iDepth);` |
|      45 |  453 | `			continue;` |
|       - |  454 | `		}` |
|   60046 |  455 | `		if( (p->nType & PH7_TK_OP) && p->pUserData` |
|    8715 |  456 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_QUESTY ){` |
|       9 |  457 | `			return 1;` |
|       - |  458 | `		}` |
|   60043 |  459 | `		p++;` |
|       5 |  460 | `	}` |
|   44139 |  461 | `	return 0;` |
|   22048 |  462 | `}` |
|       - |  463 | `/*` |
|       - |  464 | ` * Split a constant expression at its top-level ternary, the way php's constant` |
|       - |  465 | ` * folder does: the condition decides which branch survives, and only the surviving` |
|       - |  466 | `` * one is subject to the rules. `true ? 1 : new X` and `false ? new X : 1` are both`` |
|       - |  467 | ` * legal php for exactly this reason, and scanning the dropped branch refused valid` |
|       - |  468 | ` * programs. Answers 1 when it handled the span (writing the verdict to *pzErr).` |
|       - |  469 | ` */` |
|   44164 |  470 | `static int GenStateConstExprTernary(SyToken *pStart,SyToken *pStop,int bAllowNew,` |
|       - |  471 | `	int nDepth,const char **pzErr)` |
|       5 |  472 | `{` |
|   44169 |  473 | `	SyToken *p = pStart, *pQ = 0, *pColon = 0;` |
|   44169 |  474 | `	int iDepth = 0, iNest = 0, iTruth;` |
|  104377 |  475 | `	while( p < pStop ){` |
|   60230 |  476 | `		if( (p->nType & PH7_TK_KEYWORD) && (p->nType & PH7_TK_MEMBER_NAME) == 0` |
|     229 |  477 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|     204 |  478 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|       - |  479 | `			/* A ternary inside a closure body -- or an ARROW function's whole body,` |
|       - |  480 | `			 * which carries no braces to raise the depth -- is runtime code, not this` |
|       - |  481 | ``			 * expression's ternary. Reading `fn() => true ? 1 : 2` as one folded away`` |
|       - |  482 | `			 * the arrow that the rules exist to refuse. */` |
|      49 |  483 | `			GenStateInitSkipFuncConstruct(&p,pStop,&iDepth);` |
|      49 |  484 | `			continue;` |
|       - |  485 | `		}` |
|   60191 |  486 | `		if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     469 |  487 | `			iDepth++;` |
|   59959 |  488 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     469 |  489 | `			if( iDepth > 0 ){` |
|     469 |  490 | `				iDepth--;` |
|     232 |  491 | `			}` |
|   59495 |  492 | `		}else if( iDepth == 0 && (p->nType & PH7_TK_OP) && p->pUserData` |
|    2197 |  493 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_QUESTY ){` |
|      24 |  494 | `			if( pQ == 0 ){` |
|      24 |  495 | `				pQ = p;` |
|      11 |  496 | `			}` |
|      24 |  497 | `			iNest++;` |
|   59252 |  498 | `		}else if( iDepth == 0 && pQ != 0 && (p->nType & PH7_TK_COLON) ){` |
|       - |  499 | ``			/* `::` is one DC operator token, not two colons, so it never lands here;`` |
|       - |  500 | ``			 * a nested ternary's own colon is matched against its own `?`. */`` |
|      24 |  501 | `			iNest--;` |
|      24 |  502 | `			if( iNest == 0 ){` |
|      24 |  503 | `				pColon = p;` |
|      24 |  504 | `				break;` |
|       - |  505 | `			}` |
|     ! 0 |  506 | `		}` |
|   60169 |  507 | `		p++;` |
|       5 |  508 | `	}` |
|   44169 |  509 | `	if( pQ == 0 ){` |
|   44147 |  510 | `		return 0; /* no top-level ternary: the linear walk owns this span */` |
|       - |  511 | `	}` |
|      24 |  512 | `	if( pColon == 0 ){` |
|     ! 0 |  513 | `		*pzErr = 0; /* unbalanced (the parser will say so); nothing to rule on here */` |
|     ! 0 |  514 | `		return 1;` |
|       - |  515 | `	}` |
|      24 |  516 | `	iTruth = GenStateConstExprTruth(pStart,pQ);` |
|      24 |  517 | `	if( iTruth < 0 ){` |
|     ! 0 |  518 | `		*pzErr = 0; /* php would fold what this cannot: leave the whole span alone */` |
|     ! 0 |  519 | `		return 1;` |
|       - |  520 | `	}` |
|      24 |  521 | `	*pzErr = GenStateConstExprSpan(pStart,pQ,bAllowNew,nDepth + 1);` |
|      24 |  522 | `	if( *pzErr == 0 ){` |
|       - |  523 | ``		/* `c ?: e` keeps the condition when it is truthy, so the then-span is empty. */`` |
|      24 |  524 | `		*pzErr = iTruth` |
|      14 |  525 | `			? GenStateConstExprSpan(&pQ[1],pColon,bAllowNew,nDepth + 1)` |
|      15 |  526 | `			: GenStateConstExprSpan(&pColon[1],pStop,bAllowNew,nDepth + 1);` |
|      11 |  527 | `	}` |
|      24 |  528 | `	return 1;` |
|   22059 |  529 | `}` |
|       - |  530 | `/*` |
|       - |  531 | ` * Every compile-time rule php applies to a CONSTANT EXPRESSION, over one token span,` |
|       - |  532 | ` * as a single left-to-right walk that answers with the FIRST offender's sentence --` |
|       - |  533 | ` * which is exactly the one php prints, because php walks the same expression in the` |
|       - |  534 | ` * same order and stops at the first node it refuses to compile. Run as separate` |
|       - |  535 | ` * passes, the rules got that order wrong whenever two kinds appeared together:` |
|       - |  536 | `` * `[strlen('a'), function(){}]` said "Closures in constant expressions must be`` |
|       - |  537 | ` * static" where php says "Constant expression contains invalid operations".` |
|       - |  538 | ` *` |
|       - |  539 | ` * The rules, in the order a token can trigger them:` |
|       - |  540 | ` *   - an ARROW function -- "Constant expression contains invalid operations". There` |
|       - |  541 | `` *     is no static-`fn` escape: `static fn()=>1` is refused too;`` |
|       - |  542 | ` *   - an IMMEDIATELY INVOKED closure -- a call is what php names it, so it takes the` |
|       - |  543 | ` *     call sentence and not the closure one;` |
|       - |  544 | ` *   - a NON-STATIC closure -- "Closures in constant expressions must be static".` |
|       - |  545 | `` *     `static function(){...}` is accepted and its body skipped, being runtime code;`` |
|       - |  546 | `` *   - a STATIC PROPERTY fetch (`C::$p`, `self::$p`, `static::$p`) -- php has no`` |
|       - |  547 | ` *     constant-expression node for one, so it is "invalid operations";` |
|       - |  548 | `` *   - `static::` -- "\"static::\" is not allowed in compile-time constants", with the`` |
|       - |  549 | ``  *     `static::class` face carrying php's own separate sentence. `self::`/`parent::` `` |
|       - |  550 | ` *     are fine: they name the DECLARING class, which is known where it is written;` |
|       - |  551 | ` *   - a CALL -- "Constant expression contains invalid operations". A first-class` |
|       - |  552 | `` *     callable (`strlen(...)`) is only an ellipsis in parens, and a constructor's`` |
|       - |  553 | `` *     argument list belongs to its `new`, so neither of those counts;`` |
|       - |  554 | `` *   - `new`, unless bAllowNew -- "New expressions are not supported in this context".`` |
|       - |  555 | ` */` |
|   44164 |  556 | `static const char * GenStateConstExprSpan(SyToken *pStart,SyToken *pStop,int bAllowNew,int nDepth)` |
|       5 |  557 | `{` |
|   44169 |  558 | `	SyToken *p = pStart;` |
|   44169 |  559 | `	int iDepth = 0;` |
|   44169 |  560 | `	const char *zTern = 0;` |
|   44169 |  561 | `	if( nDepth > 32 ){` |
|     ! 0 |  562 | `		return 0; /* pathological nesting: stop rather than recurse */` |
|       - |  563 | `	}` |
|   44169 |  564 | `	if( GenStateConstExprTernary(pStart,pStop,bAllowNew,nDepth,&zTern) ){` |
|      24 |  565 | `		return zTern;` |
|       - |  566 | `	}` |
|   44147 |  567 | `	if( GenStateSpanHasNestedTernary(pStart,pStop) ){` |
|       9 |  568 | `		return 0; /* php folds it and this cannot: leave the span alone */` |
|       - |  569 | `	}` |
|  104085 |  570 | `	while( p < pStop ){` |
|   59989 |  571 | `		if( (p->nType & PH7_TK_KEYWORD) && (p->nType & PH7_TK_MEMBER_NAME) == 0 ){` |
|     219 |  572 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(p->pUserData);` |
|     219 |  573 | `			if( nKw == PH7_TKWRD_FN ){` |
|       6 |  574 | `				return "Constant expression contains invalid operations";` |
|       - |  575 | `			}` |
|     215 |  576 | `			if( nKw == PH7_TKWRD_FUNCTION ){` |
|      39 |  577 | `				int iScan = iDepth;` |
|      39 |  578 | `				SyToken *q = p;` |
|       - |  579 | ``				/* `(function(){...})()` is a CALL to php, and a call is what it names --`` |
|       - |  580 | `				 * the closure rule never gets a say. Look past the construct and the` |
|       - |  581 | `				 * parens wrapping it for the argument list. */` |
|      39 |  582 | `				GenStateInitSkipFuncConstruct(&q,pStop,&iScan);` |
|      41 |  583 | `				while( q < pStop && (q->nType & PH7_TK_RPAREN) ){` |
|       3 |  584 | `					q++;` |
|       1 |  585 | `				}` |
|      39 |  586 | `				if( q < pStop && (q->nType & PH7_TK_LPAREN) ){` |
|       6 |  587 | `					return "Constant expression contains invalid operations";` |
|       - |  588 | `				}` |
|       - |  589 | ``				/* The `static` modifier sits in the token immediately before. */`` |
|      37 |  590 | `				if( !(p > pStart && (p[-1].nType & PH7_TK_KEYWORD)` |
|      26 |  591 | `					&& SX_PTR_TO_INT(p[-1].pUserData) == PH7_TKWRD_STATIC) ){` |
|       9 |  592 | `					return "Closures in constant expressions must be static";` |
|       - |  593 | `				}` |
|      31 |  594 | `				GenStateInitSkipFuncConstruct(&p,pStop,&iDepth);` |
|      31 |  595 | `				continue;` |
|       - |  596 | `			}` |
|     176 |  597 | `			if( nKw == PH7_TKWRD_STATIC && &p[1] < pStop` |
|      34 |  598 | `				&& GenStateTokenIsDoubleColon(&p[1])` |
|      25 |  599 | `				&& !(&p[2] < pStop && (p[2].nType & PH7_TK_DOLLAR)) ){` |
|       - |  600 | ``				/* `static::$p` is excluded above: it is a property fetch, which the`` |
|       - |  601 | ``				 * `::` rule below words php's way. */`` |
|       4 |  602 | `				if( &p[2] < pStop && (p[2].nType & PH7_TK_KEYWORD)` |
|       5 |  603 | `					&& SX_PTR_TO_INT(p[2].pUserData) == PH7_TKWRD_CLASS ){` |
|       3 |  604 | `					return "static::class cannot be used for compile-time class name resolution";` |
|       - |  605 | `				}` |
|       3 |  606 | `				return "\"static::\" is not allowed in compile-time constants";` |
|       - |  607 | `			}` |
|      86 |  608 | `		}` |
|   59947 |  609 | `		if( GenStateTokenIsDoubleColon(p) && &p[1] < pStop && (p[1].nType & PH7_TK_DOLLAR) ){` |
|       3 |  610 | `			return "Constant expression contains invalid operations";` |
|       - |  611 | `		}` |
|   59945 |  612 | `		if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       - |  613 | `			/* A '(' directly after a NAME is a call. A name here is an identifier` |
|       - |  614 | ``			 * token; `new X(` is excluded by walking back to the `new` operator, and`` |
|       - |  615 | ``			 * `f(...)` (first-class callable) by peeking for a lone ellipsis. */`` |
|     442 |  616 | `			if( (p->nType & PH7_TK_LPAREN) && p > pStart` |
|      81 |  617 | `				&& (p[-1].nType & PH7_TK_ID)` |
|      70 |  618 | `				&& !((p[-1].nType & PH7_TK_OP) && p[-1].pUserData` |
|     ! 0 |  619 | `					&& ((const ph7_expr_op *)p[-1].pUserData)->iOp == EXPR_OP_NEW) ){` |
|      55 |  620 | `				int bNewCtor = 0;` |
|      55 |  621 | `				SyToken *q = &p[-1];` |
|       - |  622 | ``				/* Walk back over a qualified name (A\B, A::b, $o->m) to a `new`. The name`` |
|       - |  623 | `				 * ALTERNATES -- segment, separator, segment -- so both steps have to be` |
|       - |  624 | `				 * taken: stepping over the separator alone stopped on the segment before` |
|       - |  625 | ``				 * it, and every `new` whose class name carries a `\` then read as a CALL.`` |
|       - |  626 | ``				 * `new Rule\A()` is the ordinary spelling in namespaced code, so an`` |
|       - |  627 | ``				 * attribute argument, a global `const` and a parameter default all`` |
|       - |  628 | `				 * refused what php compiles (Respect\Validation's whole attribute` |
|       - |  629 | `				 * suite is written that way). */` |
|      97 |  630 | `				while( q > pStart ){` |
|      82 |  631 | `					SyToken *pPrev = &q[-1];` |
|      78 |  632 | `					if( (pPrev->nType & PH7_TK_OP) && pPrev->pUserData` |
|      40 |  633 | `						&& ((const ph7_expr_op *)pPrev->pUserData)->iOp == EXPR_OP_NEW ){` |
|      38 |  634 | `						bNewCtor = 1;` |
|      38 |  635 | `						break;` |
|       - |  636 | `					}` |
|      46 |  637 | `					if( GenStateTokenIsMemberOp(pPrev) \|\| (pPrev->nType & PH7_TK_NSSEP) ){` |
|      25 |  638 | `						q--;   /* a separator: whatever precedes it continues the name */` |
|      25 |  639 | `						continue;` |
|       - |  640 | `					}` |
|      20 |  641 | `					if( (pPrev->nType & PH7_TK_ID)` |
|      21 |  642 | `						&& (GenStateTokenIsMemberOp(q) \|\| (q->nType & PH7_TK_NSSEP)) ){` |
|      19 |  643 | `						q--;   /* ...and a SEGMENT, but only across a separator */` |
|      19 |  644 | `						continue;` |
|       - |  645 | `					}` |
|       3 |  646 | `					break;` |
|     ! 0 |  647 | `				}` |
|      55 |  648 | `				if( !bNewCtor && !(&p[1] < pStop && (p[1].nType & PH7_TK_ELLIPSIS)) ){` |
|      16 |  649 | `					return "Constant expression contains invalid operations";` |
|       - |  650 | `				}` |
|      19 |  651 | `			}` |
|     435 |  652 | `			iDepth++;` |
|   59718 |  653 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     429 |  654 | `			if( iDepth > 0 ){` |
|     429 |  655 | `				iDepth--;` |
|     212 |  656 | `			}` |
|   59291 |  657 | `		}else if( !bAllowNew && (p->nType & PH7_TK_OP) && p->pUserData` |
|    8077 |  658 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|       - |  659 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP) whose`` |
|       - |  660 | `` 			 * pUserData is the operator instance, not a keyword id. Ignore a `new` `` |
|       - |  661 | ``			 * used as a member name (`A::new` / `$o->new`). */`` |
|      12 |  662 | `			if( p == pStart \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|      12 |  663 | `				return "New expressions are not supported in this context";` |
|       - |  664 | `			}` |
|     ! 0 |  665 | `		}` |
|   59925 |  666 | `		p++;` |
|       5 |  667 | `	}` |
|   44101 |  668 | `	return 0;` |
|   22059 |  669 | `}` |
|       - |  670 | `/*` |
|       - |  671 | ` * The constant-expression screen as the compiler's call sites use it: the rules of` |
|       - |  672 | ` * GenStateConstExprSpan over the initializer that begins at the current token.` |
|       - |  673 | ` *` |
|       - |  674 | `` * bAllowNew states PHP 8.1's split: a global `const`, a parameter default and an`` |
|       - |  675 | `` * attribute argument take `new`; a class/interface constant, an enum case value and`` |
|       - |  676 | ` * a property default do not.` |
|       - |  677 | ` *` |
|       - |  678 | ` * Returns the sentence, or 0 when the expression is clean. Never consumes tokens.` |
|       - |  679 | ` */` |
|   44120 |  680 | `PH7_PRIVATE const char * PH7_GenStateConstExprError(ph7_gen_state *pGen,int bAllowNew)` |
|       5 |  681 | `{` |
|   66157 |  682 | `	return GenStateConstExprSpan(pGen->pIn,` |
|   22032 |  683 | `		GenStateConstExprEnd(pGen->pIn,pGen->pEnd),bAllowNew,0);` |
|       5 |  684 | `}` |
|       - |  685 | `/*` |
|       - |  686 | ` * php keeps class CONSTANTS and PROPERTIES in separate namespaces: a class may` |
|       - |  687 | `` * declare `const C` and `public $C` together, and `$obj->C` never resolves to the`` |
|       - |  688 | ` * constant. PHL stores each in its own table (constants in hConst, properties in` |
|       - |  689 | ` * hAttr), so these two lookups target the right namespace and never collide.` |
|       - |  690 | ` */` |
|     644 |  691 | `static ph7_class_attr * GenStateExtractConstant(ph7_class *pClass,SyString *pName)` |
|       5 |  692 | `{` |
|     649 |  693 | `	return PH7_ClassExtractConstant(pClass,pName->zString,pName->nByte);` |
|       5 |  694 | `}` |
|    2820 |  695 | `static ph7_class_attr * GenStateExtractProperty(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|       5 |  696 | `{` |
|    2825 |  697 | `	return PH7_ClassExtractAttribute(pClass,zName,nByte);` |
|       5 |  698 | `}` |
|       - |  699 | `/*` |
|       - |  700 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|       - |  701 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|       - |  702 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|       - |  703 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|       - |  704 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|       - |  705 | ` * share the same backing.` |
|       - |  706 | ` */` |
|     884 |  707 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|       - |  708 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|       5 |  709 | `{` |
|     889 |  710 | `	pAttr->nType = nType;` |
|     889 |  711 | `	pAttr->sClass = *pClass;` |
|     889 |  712 | `	pAttr->sTypeName = *pTypeName;` |
|     889 |  713 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  714 | `		sxu32 i;` |
|     153 |  715 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|     105 |  716 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|     105 |  717 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|      55 |  718 | `		}` |
|      24 |  719 | `	}` |
|     889 |  720 | `}` |
|     664 |  721 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  722 | `{` |
|     669 |  723 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  724 | `	SySet *pInstrContainer;` |
|       - |  725 | `	ph7_class_attr *pCons;` |
|       - |  726 | `	SyString *pName;` |
|       - |  727 | `	sxi32 rc;` |
|     669 |  728 | `	sxu32 nType = 0;` |
|       - |  729 | `	SyString sTypeClass;` |
|       - |  730 | `	SyString sTypeText;` |
|       - |  731 | `	SySet aUnionAlts;` |
|     669 |  732 | `	sxi32 iTypeFlags = 0;` |
|     669 |  733 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|     669 |  734 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|     669 |  735 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  736 | `	/* Extract visibility level */` |
|     669 |  737 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  738 | `	/* Mark as constant */` |
|     669 |  739 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|     669 |  740 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       - |  741 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|       - |  742 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|     698 |  743 | `	if( GenStateClassConstHasType(pGen) ){` |
|      92 |  744 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|       - |  745 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,` |
|      58 |  746 | `			/* bParamCtx */ 0,pGen->pIn->nLine);` |
|       - |  747 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|       - |  748 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|       - |  749 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|       - |  750 | `		 * and success paths release. */` |
|      63 |  751 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  752 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  753 | `			goto Synchronize;` |
|      63 |  754 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  755 | `			return SXERR_ABORT;` |
|      63 |  756 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  757 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  758 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|     ! 0 |  759 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  760 | `				return SXERR_ABORT;` |
|       - |  761 | `			}` |
|     ! 0 |  762 | `			goto Synchronize;` |
|       - |  763 | `		}` |
|      63 |  764 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      29 |  765 | `	}` |
|     332 |  766 | `loop:` |
|       - |  767 | ``	/* php 8 accepts EVERY reserved word as a class-constant name — `const list = 5`,`` |
|       - |  768 | ``	 * `const match`, `const function`, even `const true` — because a class constant is`` |
|       - |  769 | ``	 * addressed only through `C::name`, where no keyword can be ambiguous. The single`` |
|       - |  770 | ``	 * exception is `class`, reserved for `C::class`, and it gets its own message.`` |
|       - |  771 | `	 * (Method names already accept the whole set; this is the member-name side of the` |
|       - |  772 | ``	 * same rule. Global `const` is NOT the same rule: php rejects a reserved word there.)`` |
|       - |  773 | `	 * A keyword arrives as PH7_TK_KEYWORD, which this ID-only test rejected — so PHL` |
|       - |  774 | ``	 * accepted only the alpha-OPERATOR keywords (`const new`, `const and`), which the`` |
|       - |  775 | `	 * lexer marks PH7_TK_ID\|PH7_TK_OP, and that partial allow-list looked like a design. */` |
|     675 |  776 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  777 | `		/* Invalid constant name */` |
|     ! 0 |  778 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 |  779 | `		if( rc == SXERR_ABORT ){` |
|       - |  780 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  781 | `			return SXERR_ABORT;` |
|       - |  782 | `		}` |
|     ! 0 |  783 | `		goto Synchronize;` |
|       - |  784 | `	}` |
|       - |  785 | `	/* Peek constant name */` |
|     675 |  786 | `	pName = &pGen->pIn->sData;` |
|     670 |  787 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     376 |  788 | `		&& (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CLASS ){` |
|       3 |  789 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  790 | `			"A class constant must not be called 'class'; it is reserved for class name fetching");` |
|       3 |  791 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  792 | `			return SXERR_ABORT;` |
|       - |  793 | `		}` |
|       3 |  794 | `		goto Synchronize;` |
|       - |  795 | `	}` |
|       - |  796 | `	/* No reserved-CONSTANT check here: true/false/null are reserved GLOBAL constant` |
|       - |  797 | ``	 * names (compile_stmt.c still rejects `const true = 1`), but `C::true` addresses a`` |
|       - |  798 | `	 * class constant and php accepts the declaration like any other reserved word. The` |
|       - |  799 | `	 * member-name flag keeps the read from folding into the boolean literal. */` |
|     673 |  800 | `	if( (iFlags & PH7_CLASS_ATTR_FINAL) && iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       - |  801 | ``		/* `final` says "no subclass may replace this", and a PRIVATE constant is not`` |
|       - |  802 | `		 * visible to one -- so php refuses the pair, naming the constant. Same shape` |
|       - |  803 | `		 * as the private-final METHOD rule one member over, except php makes this one` |
|       - |  804 | `		 * a fatal rather than a warning. Reported per NAME, which is php's order for` |
|       - |  805 | `		 * a multi-declaration too. */` |
|       4 |  806 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  807 | `			"Private constant %z::%z cannot be final as it is not visible to other classes",` |
|       1 |  808 | `			&pClass->sName,pName);` |
|       3 |  809 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  810 | `			return SXERR_ABORT;` |
|       - |  811 | `		}` |
|       3 |  812 | `		goto Synchronize;` |
|       - |  813 | `	}` |
|       - |  814 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|     671 |  815 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      92 |  816 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|      58 |  817 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      29 |  818 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|      63 |  819 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  820 | `			return SXERR_ABORT;` |
|      63 |  821 | `		}else if( rc != SXRET_OK ){` |
|       3 |  822 | `			goto Synchronize;` |
|       - |  823 | `		}` |
|      28 |  824 | `	}` |
|       - |  825 | `	/* Advance the stream cursor */` |
|     669 |  826 | `	pGen->pIn++;` |
|     669 |  827 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  828 | `		/* Invalid declaration */` |
|     ! 0 |  829 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 |  830 | `		if( rc == SXERR_ABORT ){` |
|       - |  831 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  832 | `			return SXERR_ABORT;` |
|       - |  833 | `		}` |
|     ! 0 |  834 | `		goto Synchronize;` |
|       - |  835 | `	}` |
|     669 |  836 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - |  837 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|       - |  838 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|       - |  839 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|       - |  840 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|     664 |  841 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|      57 |  842 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|       8 |  843 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  844 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|       2 |  845 | `			&pClass->sName,pName,&sTypeText);` |
|       6 |  846 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  847 | `			return SXERR_ABORT;` |
|       - |  848 | `		}` |
|       6 |  849 | `		goto Synchronize;` |
|       - |  850 | `	}` |
|       - |  851 | `	/* php's constant-expression rules, first offender wins (see` |
|       - |  852 | ``	 * PH7_GenStateConstExprError). A class/interface constant takes no `new`. */`` |
|       - |  853 | `	{` |
|     665 |  854 | `		const char *zCErr = PH7_GenStateConstExprError(pGen,0);` |
|     665 |  855 | `		if( zCErr ){` |
|      20 |  856 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"%s",zCErr);` |
|      20 |  857 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  858 | `				return SXERR_ABORT;` |
|       - |  859 | `			}` |
|      20 |  860 | `			goto Synchronize;` |
|       - |  861 | `		}` |
|       - |  862 | `	}` |
|       - |  863 | `	/* php: a class constant may not be redefined in the same class body. The` |
|       - |  864 | `	 * property path already guarded this; the constant path did not, so` |
|       - |  865 | ``	 * `class C{const X=1; const X=2;}` silently kept one of them. */`` |
|       - |  866 | ``	/* php keeps constants and properties in SEPARATE namespaces, so `const C` and`` |
|       - |  867 | ``	 * `public $C` coexist. PHL now stores them in disjoint tables (hConst / hAttr),`` |
|       - |  868 | `	 * so no collision — only a genuine constant redefinition is rejected below. */` |
|     649 |  869 | `	if( GenStateExtractConstant(pClass,pName) != 0 ){` |
|     ! 0 |  870 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  871 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|     ! 0 |  872 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  873 | `			return SXERR_ABORT;` |
|       - |  874 | `		}` |
|     ! 0 |  875 | `		goto Synchronize;` |
|       - |  876 | `	}` |
|       - |  877 | `	/* Allocate a new class attribute */` |
|     649 |  878 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|     649 |  879 | `	if( pCons ){` |
|     649 |  880 | `		GenStateConsumeDoc(&(*pGen),&pCons->sDoc);` |
|     649 |  881 | `		if( GenStateConsumeAttrs(&(*pGen),&pCons->aAttrs) == SXERR_ABORT ){` |
|     ! 0 |  882 | `			return SXERR_ABORT;` |
|       - |  883 | `		}` |
|     649 |  884 | `		if( GenStateCheckAttrPlacement(&(*pGen),&pCons->aAttrs,16,16,0,0) == SXERR_ABORT ){` |
|     ! 0 |  885 | `			return SXERR_ABORT;` |
|       - |  886 | `		}` |
|     322 |  887 | `	}` |
|     649 |  888 | `	if( pCons == 0 ){` |
|     ! 0 |  889 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  890 | `		return SXERR_ABORT;` |
|       - |  891 | `	}` |
|     649 |  892 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      57 |  893 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      26 |  894 | `	}` |
|       - |  895 | `	/* Swap bytecode container */` |
|     649 |  896 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     649 |  897 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - |  898 | `	/* Compile constant value.` |
|       - |  899 | `	 */` |
|     649 |  900 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|     649 |  901 | `	if( rc == SXERR_EMPTY ){` |
|       3 |  902 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 |  903 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  904 | `			return SXERR_ABORT;` |
|       - |  905 | `		}` |
|       1 |  906 | `	}` |
|       - |  907 | `	/* Emit the done instruction */` |
|     649 |  908 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|     649 |  909 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     649 |  910 | `	if( rc == SXERR_ABORT ){` |
|       - |  911 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  912 | `		return SXERR_ABORT;` |
|       - |  913 | `	}` |
|       - |  914 | `	/* All done,install the constant */` |
|     649 |  915 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|     649 |  916 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  917 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  918 | `		return SXERR_ABORT;` |
|       - |  919 | `	}` |
|     649 |  920 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  921 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|       7 |  922 | `		pGen->pIn++; /* Jump the comma */` |
|       - |  923 | `		/* A reserved word is a valid name for EVERY constant in the declaration, not` |
|       - |  924 | ``		 * just the first (`const list = 1, match = 2`) — same allow-list as the head. */`` |
|       7 |  925 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  926 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  927 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  928 | `				pTok--;` |
|     ! 0 |  929 | `			}` |
|     ! 0 |  930 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|       - |  931 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 |  932 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  933 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  934 | `				return SXERR_ABORT;` |
|       - |  935 | `			}` |
|     ! 0 |  936 | `		}else{` |
|       7 |  937 | `			goto loop;` |
|       - |  938 | `		}` |
|     ! 0 |  939 | `	}` |
|     643 |  940 | `	SySetRelease(&aUnionAlts);` |
|     643 |  941 | `	return SXRET_OK;` |
|      13 |  942 | `Synchronize:` |
|      30 |  943 | `	SySetRelease(&aUnionAlts);` |
|       - |  944 | `	/* Synchronize with the first semi-colon */` |
|     156 |  945 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|     130 |  946 | `		pGen->pIn++;` |
|       4 |  947 | `	}` |
|      30 |  948 | `	return SXERR_CORRUPT;` |
|     337 |  949 | `}` |
|       - |  950 | `/*` |
|       - |  951 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - |  952 | ` * According to the PHP language reference manual` |
|       - |  953 | ` *  Properties` |
|       - |  954 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - |  955 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - |  956 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - |  957 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - |  958 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - |  959 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - |  960 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - |  961 | ` * Symisc eXtension.` |
|       - |  962 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - |  963 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  964 | ` *  Example:` |
|       - |  965 | ` *   class Test{` |
|       - |  966 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  967 | ` *   };` |
|       - |  968 | ` *   var_dump(TEST::myVar);` |
|       - |  969 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  970 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  971 | ` */` |
|       - |  972 | `/*` |
|       - |  973 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|       - |  974 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|       - |  975 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|       - |  976 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|       - |  977 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|       - |  978 | ` */` |
|    6366 |  979 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|       5 |  980 | `{` |
|    6371 |  981 | `	SyToken *p = pStart;` |
|    6371 |  982 | `	int bFirst = 1;` |
|    6371 |  983 | `	if( p >= pEnd ) return 0;` |
|       - |  984 | ``	/* Optional nullable `?` shorthand. */`` |
|    6371 |  985 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|      87 |  986 | `		p++;` |
|      87 |  987 | `		if( p >= pEnd ) return 0;` |
|      41 |  988 | `	}` |
|       - |  989 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|       - |  990 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|       - |  991 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|       - |  992 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|    3183 |  993 | `	for(;;){` |
|    6411 |  994 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|       - |  995 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|       3 |  996 | `			p++;` |
|       9 |  997 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|       3 |  998 | `			if( p >= pEnd ) return 0;` |
|       3 |  999 | `			p++; /* skip ')' */` |
|       2 | 1000 | `		}else{` |
|       - | 1001 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|       - | 1002 | ``			 * then any `&`-joined intersection members. */`` |
|    6409 | 1003 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|    6409 | 1004 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 1005 | `				return 0;` |
|       - | 1006 | `			}` |
|       - | 1007 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|       - | 1008 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|       - | 1009 | `			 * may still appear at the initial dispatch site). */` |
|    6409 | 1010 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|    6301 | 1011 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|    6296 | 1012 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    1115 | 1013 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|    5527 | 1014 | `					return 0;` |
|       - | 1015 | `				}` |
|     387 | 1016 | `			}` |
|     887 | 1017 | `			p++;` |
|     893 | 1018 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       7 | 1019 | `				p += 2;` |
|       1 | 1020 | `			}` |
|    1329 | 1021 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|     893 | 1022 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       6 | 1023 | `				p++; /* skip '&' */` |
|       6 | 1024 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|       6 | 1025 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|       6 | 1026 | `				p++;` |
|       6 | 1027 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     ! 0 | 1028 | `					p += 2;` |
|     ! 0 | 1029 | `				}` |
|       2 | 1030 | `			}` |
|       - | 1031 | `		}` |
|     889 | 1032 | `		bFirst = 0;` |
|     884 | 1033 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      45 | 1034 | `			&& p->sData.zString[0] == '\|' ){` |
|      45 | 1035 | ``			p++; /* next `\|`-separated part */`` |
|      45 | 1036 | `			continue;` |
|       - | 1037 | `		}` |
|     849 | 1038 | `		break;` |
|     ! 0 | 1039 | `	}` |
|     849 | 1040 | `	if( p >= pEnd ) return 0;` |
|     849 | 1041 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|    3188 | 1042 | `}` |
|       - | 1043 |  |
|       - | 1044 | `/*` |
|       - | 1045 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|       - | 1046 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|       - | 1047 | ` * if not). Recognized forms:` |
|       - | 1048 | ` *   ?Type, array, bool, int, float, string, object,` |
|       - | 1049 | ` *   self, parent, \Ns\ClassName, ClassName` |
|       - | 1050 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|       - | 1051 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|       - | 1052 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|       - | 1053 | ` * on unrecoverable error.` |
|       - | 1054 | ` *` |
|       - | 1055 | ` * When a type is parsed:` |
|       - | 1056 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|       - | 1057 | ` *   *pClass is set to the class name (for class types)` |
|       - | 1058 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|       - | 1059 | ` *   *pTypeText is set to the original text span of the type` |
|       - | 1060 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|       - | 1061 | ` */` |
|     836 | 1062 | `static sxi32 GenStateParsePropertyType(` |
|       - | 1063 | `	ph7_gen_state *pGen,` |
|       - | 1064 | `	sxu32 *pnType,` |
|       - | 1065 | `	SyString *pClass,` |
|       - | 1066 | `	sxi32 *piTypeFlags,` |
|       - | 1067 | `	SyString *pTypeText,` |
|       - | 1068 | `	SySet *pAlts` |
|       5 | 1069 | `){` |
|     841 | 1070 | `	sxi32 iFlags = 0;` |
|       - | 1071 | `	sxi32 rc;` |
|     841 | 1072 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 1073 | `		return SXRET_OK;` |
|       - | 1074 | `	}` |
|       - | 1075 | `	/* If the first token is '$', there's no type */` |
|     841 | 1076 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 | 1077 | `		return SXRET_OK;` |
|       - | 1078 | `	}` |
|     841 | 1079 | `	rc = GenStateParseUnionTypeDecl(` |
|     418 | 1080 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|       - | 1081 | `		PH7_CLASS_ATTR_NULLABLE,` |
|       - | 1082 | `		PH7_CLASS_ATTR_UNION,` |
|       - | 1083 | `		/* bAllowVoid */ 0,` |
|       - | 1084 | `		/* bParamCtx */ 0,` |
|     836 | 1085 | `		pGen->pIn->nLine);` |
|     841 | 1086 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1087 | `		return rc;` |
|       - | 1088 | `	}` |
|       - | 1089 | `	/* Verify next token is '$' (start of property name) */` |
|     841 | 1090 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 1091 | `		return SXERR_SYNTAX;` |
|       - | 1092 | `	}` |
|     841 | 1093 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|     841 | 1094 | `	return SXRET_OK;` |
|     423 | 1095 | `}` |
|       - | 1096 |  |
|       - | 1097 | `/*` |
|       - | 1098 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|       - | 1099 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|       - | 1100 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|       - | 1101 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|       - | 1102 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|       - | 1103 | ` * by the type parser itself before reaching here.` |
|       - | 1104 | ` *` |
|       - | 1105 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|       - | 1106 | ` * use in the error message.` |
|       - | 1107 | ` */` |
|    1146 | 1108 | `static int GenStateIsDisallowedPropertyAtom(` |
|       - | 1109 | `	sxu32 nType,` |
|       - | 1110 | `	const SyString *pClass,` |
|       - | 1111 | `	const char **pzName,` |
|       - | 1112 | `	sxu32 *pnName)` |
|       5 | 1113 | `{` |
|       - | 1114 | `	const char *z;` |
|       - | 1115 | `	sxu32 n;` |
|    1151 | 1116 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|    1011 | 1117 | `		return 0;` |
|       - | 1118 | `	}` |
|     145 | 1119 | `	z = pClass->zString;` |
|     145 | 1120 | `	n = pClass->nByte;` |
|     145 | 1121 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|       9 | 1122 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|       - | 1123 | `	}` |
|       - | 1124 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|       - | 1125 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|       - | 1126 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|     139 | 1127 | `	return 0;` |
|     578 | 1128 | `}` |
|       - | 1129 |  |
|       - | 1130 | `/*` |
|       - | 1131 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|       - | 1132 | ` * constant) — the main atom plus any union alternatives — against the` |
|       - | 1133 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|       - | 1134 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|       - | 1135 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|       - | 1136 | ` * type T" vs "Class constant C::X cannot have type T").` |
|       - | 1137 | ` *` |
|       - | 1138 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|       - | 1139 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|       - | 1140 | ` */` |
|    1030 | 1141 | `PH7_PRIVATE sxi32 GenStateValidateMemberType(` |
|       - | 1142 | `	ph7_gen_state *pGen,` |
|       - | 1143 | `	ph7_class *pClass,` |
|       - | 1144 | `	const SyString *pMemberName,` |
|       - | 1145 | `	sxu32 nType,` |
|       - | 1146 | `	const SyString *pTypeClass,` |
|       - | 1147 | `	const SyString *pTypeText,` |
|       - | 1148 | `	SySet *pUnionAlts,` |
|       - | 1149 | `	const char *zErrFmt,` |
|       - | 1150 | `	sxu32 nLine)` |
|       5 | 1151 | `{` |
|    1035 | 1152 | `	const char *zBad = 0;` |
|    1035 | 1153 | `	sxu32 nBad = 0;` |
|       - | 1154 | `	SyString sFallback;` |
|       - | 1155 | `	const SyString *pBad;` |
|       - | 1156 | `	sxi32 rc;` |
|    1035 | 1157 | `	int bDisallowed = 0;` |
|    1035 | 1158 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|       6 | 1159 | `		bDisallowed = 1;` |
|    1033 | 1160 | `	}else if( pUnionAlts ){` |
|       - | 1161 | `		sxu32 i;` |
|     175 | 1162 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|     121 | 1163 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|     121 | 1164 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|       3 | 1165 | `				bDisallowed = 1;` |
|       3 | 1166 | `				break;` |
|       - | 1167 | `			}` |
|      62 | 1168 | `		}` |
|      28 | 1169 | `	}` |
|    1035 | 1170 | `	if( !bDisallowed ){` |
|    1029 | 1171 | `		return SXRET_OK;` |
|       - | 1172 | `	}` |
|       - | 1173 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|       - | 1174 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|       - | 1175 | `	 * canonical spelling if the type text is unavailable. */` |
|       9 | 1176 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|       9 | 1177 | `		pBad = pTypeText;` |
|       6 | 1178 | `	}else{` |
|     ! 0 | 1179 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|     ! 0 | 1180 | `		pBad = &sFallback;` |
|       - | 1181 | `	}` |
|      12 | 1182 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       3 | 1183 | `		zErrFmt,` |
|       3 | 1184 | `		&pClass->sName,pMemberName,pBad);` |
|       9 | 1185 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 1186 | `		return SXERR_ABORT;` |
|       - | 1187 | `	}` |
|       9 | 1188 | `	return SXERR_SYNTAX;` |
|     520 | 1189 | `}` |
|       - | 1190 | `/*` |
|       - | 1191 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|       - | 1192 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|       - | 1193 | ` * matched as a plain identifier in the class-member modifier position rather` |
|       - | 1194 | ` * than promoted to a lexer keyword.` |
|       - | 1195 | ` */` |
| 7819836 | 1196 | `PH7_PRIVATE int GenStateIsReadonly(SyToken *pTok)` |
|       5 | 1197 | `{` |
| 8586970 | 1198 | `	return (pTok->nType & PH7_TK_ID)` |
| 4673260 | 1199 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
| 8590322 | 1200 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|       5 | 1201 | `}` |
|       - | 1202 | `/*` |
|       - | 1203 | ``  * Detect an asymmetric set-visibility modifier `public(set)` / `protected(set)` `` |
|       - | 1204 | `` * / `private(set)` (PHP 8.4) starting at pTok. Returns the visibility keyword id`` |
|       - | 1205 | ` * (PH7_TKWRD_*) and sets *pnTok to the 4 tokens consumed, or 0 when not present` |
|       - | 1206 | ` * (a bare visibility keyword is NOT a set-modifier; the '(' 'set' ')' run is).` |
|       - | 1207 | ` */` |
|  204143 | 1208 | `PH7_PRIVATE sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok)` |
|       5 | 1209 | `{` |
|  204148 | 1210 | `	*pnTok = 0;` |
|  204143 | 1211 | `	if( &pTok[3] < pEnd` |
|  160073 | 1212 | `	 && (pTok->nType & PH7_TK_KEYWORD)` |
|  116123 | 1213 | `	 && (pTok[1].nType & PH7_TK_LPAREN)` |
|   58010 | 1214 | `	 && (pTok[2].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|      48 | 1215 | `	 && pTok[2].sData.nByte == sizeof("set")-1` |
|      47 | 1216 | `	 && SyStrnicmp(pTok[2].sData.zString,"set",sizeof("set")-1) == 0` |
|      51 | 1217 | `	 && (pTok[3].nType & PH7_TK_RPAREN) ){` |
|      49 | 1218 | `		sxi32 nKw = SX_PTR_TO_INT(pTok->pUserData);` |
|      49 | 1219 | `		if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PRIVATE \|\| nKw == PH7_TKWRD_PROTECTED ){` |
|      49 | 1220 | `			*pnTok = 4;` |
|      49 | 1221 | `			return nKw;` |
|       - | 1222 | `		}` |
|     ! 0 | 1223 | `	}` |
|  204102 | 1224 | `	return 0;` |
|  101941 | 1225 | `}` |
|       - | 1226 | `/* Map a set-visibility keyword to its PH7_CLASS_ATTR_* flag. */` |
|      46 | 1227 | `PH7_PRIVATE sxi32 GenStateSetVisFlag(sxi32 nKw)` |
|       3 | 1228 | `{` |
|      49 | 1229 | `	if( nKw == PH7_TKWRD_PRIVATE ){` |
|      37 | 1230 | `		return PH7_CLASS_ATTR_PRIVATE_SET;` |
|       - | 1231 | `	}` |
|      13 | 1232 | `	if( nKw == PH7_TKWRD_PROTECTED ){` |
|      11 | 1233 | `		return PH7_CLASS_ATTR_PROTECTED_SET;` |
|       - | 1234 | `	}` |
|       3 | 1235 | `	return PH7_CLASS_ATTR_PUBLIC_SET;` |
|      26 | 1236 | `}` |
|    2652 | 1237 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 | 1238 | `{` |
|    2657 | 1239 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 1240 | `	ph7_class_attr *pAttr;` |
|       - | 1241 | `	SyString *pName;` |
|       - | 1242 | `	sxi32 rc;` |
|    2657 | 1243 | `	sxu32 nType = 0;` |
|       - | 1244 | `	SyString sTypeClass;` |
|       - | 1245 | `	SyString sTypeText;` |
|       - | 1246 | `	SySet aUnionAlts;` |
|    2657 | 1247 | `	sxi32 iTypeFlags = 0;` |
|    2657 | 1248 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|    2657 | 1249 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|    2657 | 1250 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - | 1251 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|       - | 1252 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|       - | 1253 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|    2657 | 1254 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|      26 | 1255 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|      11 | 1256 | `	}` |
|       - | 1257 | `	/* Extract visibility level */` |
|    2657 | 1258 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - | 1259 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|    3075 | 1260 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     841 | 1261 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|     841 | 1262 | `		if( rc == SXERR_CORRUPT ){` |
|       - | 1263 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 | 1264 | `			goto Synchronize;` |
|     841 | 1265 | `		}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 | 1266 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 1267 | `				"Invalid property type or declaration near '%z'",` |
|     ! 0 | 1268 | `				&pGen->pIn->sData);` |
|     ! 0 | 1269 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1270 | `				return SXERR_ABORT;` |
|       - | 1271 | `			}` |
|     ! 0 | 1272 | `			goto Synchronize;` |
|     841 | 1273 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 | 1274 | `			return SXERR_ABORT;` |
|       - | 1275 | `		}` |
|     418 | 1276 | `	}` |
|     ! 0 | 1277 | `loop:` |
|    2665 | 1278 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 1279 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Expected '$' at start of property name");` |
|     ! 0 | 1280 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1281 | `			return SXERR_ABORT;` |
|       - | 1282 | `		}` |
|     ! 0 | 1283 | `		goto Synchronize;` |
|       - | 1284 | `	}` |
|    2665 | 1285 | `	pGen->pIn++; /* Jump the dollar sign */` |
|    2665 | 1286 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - | 1287 | `		/* Invalid attribute name */` |
|     ! 0 | 1288 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 | 1289 | `		if( rc == SXERR_ABORT ){` |
|       - | 1290 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1291 | `			return SXERR_ABORT;` |
|       - | 1292 | `		}` |
|     ! 0 | 1293 | `		goto Synchronize;` |
|       - | 1294 | `	}` |
|       - | 1295 | `	/* Peek attribute name */` |
|    2665 | 1296 | `	pName = &pGen->pIn->sData;` |
|       - | 1297 | `	/* Advance the stream cursor */` |
|    2665 | 1298 | `	pGen->pIn++;` |
|    2665 | 1299 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/\|PH7_TK_OCB/*'{' hooks*/)) == 0 ){` |
|       - | 1300 | `		/* Invalid declaration */` |
|       - | 1301 | `		/* php reports the offending token here, expecting "," or ";". */` |
|       3 | 1302 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\",\" or \";\"");` |
|       3 | 1303 | `		if( rc == SXERR_ABORT ){` |
|       - | 1304 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1305 | `			return SXERR_ABORT;` |
|       - | 1306 | `		}` |
|       3 | 1307 | `		goto Synchronize;` |
|       - | 1308 | `	}` |
|       - | 1309 | `	/* Asymmetric-visibility rules (PHP 8.4): the property must be typed, and` |
|       - | 1310 | `	 * the read visibility must not be narrower than the set visibility. */` |
|    2663 | 1311 | `	if( iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET\|PH7_CLASS_ATTR_PUBLIC_SET) ){` |
|      42 | 1312 | `		const char *zAvErr = 0;` |
|      62 | 1313 | `		sxi32 iSetLevel = (iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? PH7_CLASS_PROT_PRIVATE` |
|      32 | 1314 | `			: (iFlags & PH7_CLASS_ATTR_PROTECTED_SET) ? PH7_CLASS_PROT_PROTECTED` |
|       6 | 1315 | `			: PH7_CLASS_PROT_PUBLIC;` |
|      42 | 1316 | `		if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|     ! 0 | 1317 | `			zAvErr = "Property with asymmetric visibility %z::$%z must have type";` |
|      42 | 1318 | `		}else if( iProtection > iSetLevel ){` |
|     ! 0 | 1319 | `			zAvErr = "Visibility of property %z::$%z must not be weaker than set visibility";` |
|     ! 0 | 1320 | `		}` |
|      42 | 1321 | `		if( zAvErr ){` |
|     ! 0 | 1322 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zAvErr,&pClass->sName,pName);` |
|     ! 0 | 1323 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1324 | `				return SXERR_ABORT;` |
|       - | 1325 | `			}` |
|     ! 0 | 1326 | `			goto Synchronize;` |
|       - | 1327 | `		}` |
|      20 | 1328 | `	}` |
|       - | 1329 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|       - | 1330 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|    2663 | 1331 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|      97 | 1332 | `		const char *zRoErr = 0;` |
|      97 | 1333 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|       3 | 1334 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|      96 | 1335 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       6 | 1336 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|      93 | 1337 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|       6 | 1338 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|       2 | 1339 | `		}` |
|      97 | 1340 | `		if( zRoErr ){` |
|      13 | 1341 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|      13 | 1342 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1343 | `				return SXERR_ABORT;` |
|       - | 1344 | `			}` |
|      13 | 1345 | `			goto Synchronize;` |
|       - | 1346 | `		}` |
|      41 | 1347 | `	}` |
|       - | 1348 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|       - | 1349 | `	 * type atom or any union alternative. void/never are already rejected` |
|       - | 1350 | `	 * by the type parser. */` |
|    2653 | 1351 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|    1256 | 1352 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|       - | 1353 | `			&sTypeText,` |
|     834 | 1354 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|     417 | 1355 | `			"Property %z::$%z cannot have type %z",nLine);` |
|     839 | 1356 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1357 | `			return SXERR_ABORT;` |
|     839 | 1358 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 | 1359 | `			goto Synchronize;` |
|       - | 1360 | `		}` |
|     417 | 1361 | `	}` |
|       - | 1362 | `	/* Reject redeclaration (catches clash with an earlier promoted property).` |
|       - | 1363 | `	 * A same-name class CONSTANT is NOT a clash — php's separate namespaces let` |
|       - | 1364 | ``	 * `const C` and `public $C` coexist (stored in disjoint hConst / hAttr tables). */`` |
|    2653 | 1365 | `	if( GenStateExtractProperty(pClass,pName->zString,pName->nByte) != 0 ){` |
|       4 | 1366 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 | 1367 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|       3 | 1368 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1369 | `			return SXERR_ABORT;` |
|       - | 1370 | `		}` |
|       3 | 1371 | `		goto Synchronize;` |
|       - | 1372 | `	}` |
|       - | 1373 | `	/* php's constant-expression rules, first offender wins. A property default takes` |
|       - | 1374 | ``	 * no `new`. pGen->pIn is still on the '=' (the scan skips it and reads the`` |
|       - | 1375 | `	 * initializer non-destructively); no '=' means no default at all, and the scan` |
|       - | 1376 | `	 * then stops at the ';'/',' with nothing to report. */` |
|    2651 | 1377 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|    1845 | 1378 | `		const char *zCErr = PH7_GenStateConstExprError(pGen,0);` |
|    1845 | 1379 | `		if( zCErr ){` |
|       9 | 1380 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"%s",zCErr);` |
|       9 | 1381 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1382 | `				return SXERR_ABORT;` |
|       - | 1383 | `			}` |
|       9 | 1384 | `			goto Synchronize;` |
|       - | 1385 | `		}` |
|     917 | 1386 | `	}` |
|       - | 1387 | `	/* Allocate a new class attribute */` |
|    2645 | 1388 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|    2645 | 1389 | `	if( pAttr ){` |
|    2645 | 1390 | `		GenStateConsumeDoc(&(*pGen),&pAttr->sDoc);` |
|    2645 | 1391 | `		if( GenStateConsumeAttrs(&(*pGen),&pAttr->aAttrs) == SXERR_ABORT ){` |
|     ! 0 | 1392 | `			return SXERR_ABORT;` |
|       - | 1393 | `		}` |
|    2645 | 1394 | `		if( GenStateCheckAttrPlacement(&(*pGen),&pAttr->aAttrs,8,8,0,0) == SXERR_ABORT ){` |
|     ! 0 | 1395 | `			return SXERR_ABORT;` |
|       - | 1396 | `		}` |
|    1320 | 1397 | `	}` |
|    2645 | 1398 | `	if( pAttr == 0 ){` |
|     ! 0 | 1399 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1400 | `		return SXERR_ABORT;` |
|       - | 1401 | `	}` |
|    2645 | 1402 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     837 | 1403 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|     416 | 1404 | `	}` |
|    2645 | 1405 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - | 1406 | `		SySet *pInstrContainer;` |
|    1839 | 1407 | `		SyToken *pSavedDefEnd = pGen->pEnd;` |
|    1839 | 1408 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - | 1409 | `		{` |
|       - | 1410 | `			/* Delimit the default expression: it ends at the declaration's` |
|       - | 1411 | `			 * ';'/',' or at a top-level '{' opening a PHP 8.4 hook list` |
|       - | 1412 | ``			 * (`public string $w = "init" { get => …; }`) — the expression`` |
|       - | 1413 | `			 * compiler would otherwise run into the hook tokens. */` |
|    1839 | 1414 | `			SyToken *pScan = pGen->pIn;` |
|    1839 | 1415 | `			sxi32 iNest = 0;` |
|    1839 | 1416 | ``			int bFuncSeen = 0; /* a `function` keyword stands at depth 0 */`` |
|   12595 | 1417 | `			while( pScan < pGen->pEnd ){` |
|   12590 | 1418 | `				if( (pScan->nType & PH7_TK_KEYWORD) && iNest <= 0` |
|      67 | 1419 | `					&& SX_PTR_TO_INT(pScan->pUserData) == PH7_TKWRD_FUNCTION ){` |
|       - | 1420 | `					/* The next depth-0 '{' is this CLOSURE's body, not a hook list:` |
|       - | 1421 | ``					 * `public $p = static function(){ … };` is php-legal (a static`` |
|       - | 1422 | `					 * closure is a constant expression) and its brace was read as` |
|       - | 1423 | `					 * the hook-list opener, so the default was truncated at` |
|       - | 1424 | ``					 * `static function()` and the declaration died three errors`` |
|       - | 1425 | `					 * deep. A class CONSTANT never showed it — only the property` |
|       - | 1426 | `					 * path carries a hook list at all. */` |
|       9 | 1427 | `					bFuncSeen = 1;` |
|   12591 | 1428 | `				}else if( pScan->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     289 | 1429 | `					iNest++;` |
|   12445 | 1430 | `				}else if( pScan->nType & (PH7_TK_RPAREN\|PH7_TK_CSB) ){` |
|     289 | 1431 | `					iNest--;` |
|   12161 | 1432 | `				}else if( iNest <= 0 && (pScan->nType & PH7_TK_OCB) ){` |
|      65 | 1433 | `					if( !bFuncSeen ){` |
|      57 | 1434 | `						break; /* the hook list */` |
|       - | 1435 | `					}` |
|       9 | 1436 | `					bFuncSeen = 0;` |
|       9 | 1437 | `					pScan++;` |
|       9 | 1438 | `					PH7_DelimitNestedTokens(pScan,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pScan);` |
|       9 | 1439 | `					if( pScan >= pGen->pEnd ){` |
|     ! 0 | 1440 | `						break;` |
|       1 | 1441 | `					}` |
|       - | 1442 | `					/* land on the closing '}', the loop's pScan++ steps past it */` |
|   11961 | 1443 | `				}else if( iNest <= 0 && (pScan->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|    1785 | 1444 | `					break;` |
|       - | 1445 | `				}` |
|   10761 | 1446 | `				pScan++;` |
|       5 | 1447 | `			}` |
|    1839 | 1448 | `			pGen->pEnd = pScan;` |
|       - | 1449 | `		}` |
|       - | 1450 | `		/* Swap bytecode container */` |
|    1839 | 1451 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    1839 | 1452 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - | 1453 | `		/* Compile attribute value. The default is a const-expression belonging to` |
|       - | 1454 | `		 * pClass (see iInMemberDefault) — __TRAIT__ in it reads pCurClass. */` |
|    1839 | 1455 | `		pGen->iInMemberDefault++;` |
|    1839 | 1456 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    1839 | 1457 | `		pGen->iInMemberDefault--;` |
|    1839 | 1458 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 | 1459 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 | 1460 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1461 | `				return SXERR_ABORT;` |
|       - | 1462 | `			}` |
|     ! 0 | 1463 | `		}` |
|       - | 1464 | `		/* Emit the done instruction */` |
|    1839 | 1465 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    1839 | 1466 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    1839 | 1467 | `		pGen->pIn = pGen->pEnd;   /* land exactly on the delimiter */` |
|    1839 | 1468 | `		pGen->pEnd = pSavedDefEnd;` |
|     917 | 1469 | `	}` |
|       - | 1470 | `	/* All done,install the attribute */` |
|    2645 | 1471 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|    2645 | 1472 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1473 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 1474 | `		return SXERR_ABORT;` |
|       - | 1475 | `	}` |
|    2645 | 1476 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) ){` |
|       - | 1477 | ``		/* PHP 8.4 property hooks: `public [T] $x [= default] { get ...; set ...; }`.`` |
|       - | 1478 | `		 * The list ends the declaration at '}' — no trailing ';', no comma list. */` |
|     185 | 1479 | `		if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|       - | 1480 | ``			/* `readonly` promises one write, a hook decides what a write MEANS, and`` |
|       - | 1481 | `			 * php will not have both -- a rule the declaration screen never had, so` |
|       - | 1482 | ``			 * `public readonly int $p { get => 1; }` compiled here and does not in`` |
|       - | 1483 | `			 * php (in a readonly CLASS too, where the modifier is implied). */` |
|       3 | 1484 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 1485 | `				"Hooked properties cannot be readonly");` |
|       3 | 1486 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1487 | `				return SXERR_ABORT;` |
|       - | 1488 | `			}` |
|       3 | 1489 | `			goto Synchronize;` |
|       - | 1490 | `		}` |
|     183 | 1491 | `		rc = GenStateCompilePropertyHooks(&(*pGen),pClass,pAttr);` |
|     183 | 1492 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1493 | `			return SXERR_ABORT;` |
|       - | 1494 | `		}` |
|     183 | 1495 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1496 | `			goto Synchronize;` |
|       - | 1497 | `		}` |
|     183 | 1498 | `		SySetRelease(&aUnionAlts);` |
|     183 | 1499 | `		return SXRET_OK;` |
|       - | 1500 | `	}` |
|    2465 | 1501 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - | 1502 | ``		/* php 8.4: `abstract` on a property requires a hook list (php's exact`` |
|       - | 1503 | `		 * wording differs per declaration site) */` |
|     ! 0 | 1504 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 1505 | `			(pClass->iFlags & PH7_CLASS_INTERFACE)` |
|       - | 1506 | `				? "Interfaces may only include hooked properties"` |
|       - | 1507 | `				: "Only hooked properties may be declared abstract");` |
|     ! 0 | 1508 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1509 | `			return SXERR_ABORT;` |
|       - | 1510 | `		}` |
|     ! 0 | 1511 | `		goto Synchronize;` |
|       - | 1512 | `	}` |
|    2465 | 1513 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 1514 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|       9 | 1515 | `		pGen->pIn++; /* Jump the comma */` |
|       9 | 1516 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 | 1517 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 1518 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 1519 | `				pTok--;` |
|     ! 0 | 1520 | `			}` |
|     ! 0 | 1521 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|       - | 1522 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 | 1523 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 1524 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1525 | `				return SXERR_ABORT;` |
|       - | 1526 | `			}` |
|     ! 0 | 1527 | `		}else{` |
|       9 | 1528 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       9 | 1529 | `				goto loop;` |
|       - | 1530 | `			}` |
|       - | 1531 | `		}` |
|     ! 0 | 1532 | `	}` |
|    2457 | 1533 | `	SySetRelease(&aUnionAlts);` |
|    2457 | 1534 | `	return SXRET_OK;` |
|      11 | 1535 | `Synchronize:` |
|       - | 1536 | `	/* Synchronize with the first semi-colon */` |
|      76 | 1537 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      54 | 1538 | `		pGen->pIn++;` |
|       4 | 1539 | `	}` |
|      26 | 1540 | `	SySetRelease(&aUnionAlts);` |
|      26 | 1541 | `	return SXERR_CORRUPT;` |
|    1331 | 1542 | `}` |
|       - | 1543 | `/*` |
|       - | 1544 | ` * php validates a magic method's DECLARATION at compile time` |
|       - | 1545 | ` * (zend_check_magic_method_implementation): the ENGINE builds the arguments and` |
|       - | 1546 | ` * calls these methods on its own, so a wrong shape is rejected where it is` |
|       - | 1547 | ` * written rather than discovered — or silently tolerated — at the dispatch.` |
|       - | 1548 | ` *` |
|       - | 1549 | ` * One row per magic name; its fields are the checks php makes for that name.` |
|       - | 1550 | ` * Arity is the first of them, in php's order — which is what decides the message` |
|       - | 1551 | ` * when a declaration breaks more than one of php's rules at once.` |
|       - | 1552 | ` */` |
|       - | 1553 | `typedef struct MagicMethodRule MagicMethodRule;` |
|       - | 1554 | `struct MagicMethodRule` |
|       - | 1555 | `{` |
|       - | 1556 | `	const char *zName; /* Magic method name */` |
|       - | 1557 | `	sxu32 nName;       /* Its length */` |
|       - | 1558 | `	int nArgs;         /* Declared arguments php requires, -1 when it does not check */` |
|       - | 1559 | `	int bStatic;       /* TRUE: must be static · FALSE: must NOT be static */` |
|       - | 1560 | `	int bPublic;       /* TRUE: must be public — a WARNING, and dispatched anyway */` |
|       - | 1561 | `	int bNoReturnType; /* TRUE: declaring ANY return type is a fatal */` |
|       - | 1562 | `};` |
|       - | 1563 | `#define MAGIC_METHOD_ROW(N,A,S,P,R) { N, sizeof(N)-1, A, S, P, R }` |
|       - | 1564 | `static const MagicMethodRule aMagicMethod[] = {` |
|       - | 1565 | `	MAGIC_METHOD_ROW("__construct",  -1, FALSE, FALSE, TRUE),` |
|       - | 1566 | `	MAGIC_METHOD_ROW("__destruct",    0, FALSE, FALSE, TRUE),` |
|       - | 1567 | `	MAGIC_METHOD_ROW("__clone",       0, FALSE, FALSE, FALSE),` |
|       - | 1568 | `	MAGIC_METHOD_ROW("__get",         1, FALSE, TRUE,  FALSE),` |
|       - | 1569 | `	MAGIC_METHOD_ROW("__set",         2, FALSE, TRUE,  FALSE),` |
|       - | 1570 | `	MAGIC_METHOD_ROW("__isset",       1, FALSE, TRUE,  FALSE),` |
|       - | 1571 | `	MAGIC_METHOD_ROW("__unset",       1, FALSE, TRUE,  FALSE),` |
|       - | 1572 | `	MAGIC_METHOD_ROW("__call",        2, FALSE, TRUE,  FALSE),` |
|       - | 1573 | `	MAGIC_METHOD_ROW("__callStatic",  2, TRUE,  TRUE,  FALSE),` |
|       - | 1574 | `	MAGIC_METHOD_ROW("__toString",    0, FALSE, TRUE,  FALSE),` |
|       - | 1575 | `	MAGIC_METHOD_ROW("__invoke",     -1, FALSE, TRUE,  FALSE),` |
|       - | 1576 | `	MAGIC_METHOD_ROW("__debugInfo",   0, FALSE, TRUE,  FALSE),` |
|       - | 1577 | `	MAGIC_METHOD_ROW("__serialize",   0, FALSE, TRUE,  FALSE),` |
|       - | 1578 | `	MAGIC_METHOD_ROW("__unserialize", 1, FALSE, TRUE,  FALSE),` |
|       - | 1579 | `	MAGIC_METHOD_ROW("__sleep",       0, FALSE, TRUE,  FALSE),` |
|       - | 1580 | `	MAGIC_METHOD_ROW("__wakeup",      0, FALSE, TRUE,  FALSE),` |
|       - | 1581 | `	MAGIC_METHOD_ROW("__set_state",   1, TRUE,  TRUE,  FALSE)` |
|       - | 1582 | `};` |
|       - | 1583 | `#undef MAGIC_METHOD_ROW` |
|       - | 1584 | `/*` |
|       - | 1585 | ` * Find the rule for a declared method name, or 0 when the name is not magic.` |
|       - | 1586 | `` * php matches method names case-insensitively everywhere, so `__GET` is `__get`;`` |
|       - | 1587 | ` * the two-underscore prefix test is php's own cheap reject.` |
|       - | 1588 | ` */` |
|  109858 | 1589 | `static const MagicMethodRule * GenStateMagicMethodRule(const SyString *pName)` |
|       5 | 1590 | `{` |
|       - | 1591 | `	sxu32 n;` |
|  109863 | 1592 | `	if( pName->nByte < sizeof("__x")-1 \|\| pName->zString[0] != '_' \|\| pName->zString[1] != '_' ){` |
|    3597 | 1593 | `		return 0;` |
|       - | 1594 | `	}` |
| 1160377 | 1595 | `	for( n = 0 ; n < SX_ARRAYSIZE(aMagicMethod) ; ++n ){` |
| 1160364 | 1596 | `		if( pName->nByte == aMagicMethod[n].nName` |
|  633885 | 1597 | `		 && SyStrnicmp(pName->zString,aMagicMethod[n].zName,aMagicMethod[n].nName) == 0 ){` |
|  106263 | 1598 | `			return &aMagicMethod[n];` |
|       - | 1599 | `		}` |
|  527058 | 1600 | `	}` |
|      10 | 1601 | `	return 0;` |
|   54934 | 1602 | `}` |
|       - | 1603 | `/*` |
|       - | 1604 | ` * TRUE when php requires this magic method to be PUBLIC: the rows it merely` |
|       - | 1605 | ` * WARNS about at the declaration and then dispatches regardless of what the` |
|       - | 1606 | ` * declaration said. The runtime's visibility gate reads this to let an` |
|       - | 1607 | ` * engine-built call through — a call the user WROTE stays denied.` |
|       - | 1608 | ` *` |
|       - | 1609 | `` * `__construct`/`__destruct`/`__clone` are deliberately not in the set: a`` |
|       - | 1610 | ` * private constructor is the singleton idiom, and php enforces those three at` |
|       - | 1611 | ` * the call like any other method.` |
|       - | 1612 | ` */` |
|  105030 | 1613 | `PH7_PRIVATE int PH7_MagicMethodMustBePublic(const SyString *pName)` |
|       5 | 1614 | `{` |
|  105035 | 1615 | `	const MagicMethodRule *pRule = GenStateMagicMethodRule(pName);` |
|  105035 | 1616 | `	return pRule != 0 && pRule->bPublic;` |
|       5 | 1617 | `}` |
|       - | 1618 | `/*` |
|       - | 1619 | ` * Enforce the rules of pRule against the declaration just parsed. pName is the` |
|       - | 1620 | ` * name AS WRITTEN — php quotes that spelling, not the canonical one.` |
|       - | 1621 | ` *` |
|       - | 1622 | ` * The diagnostic is FORMATTED, not reported: php decides these rules while the` |
|       - | 1623 | ` * signature is in hand (so the arity beats __toString's return-type rule) but` |
|       - | 1624 | ` * raises them only once the declaration has cleared the checks php makes` |
|       - | 1625 | ` * first — the redeclaration and abstract-placement rules, and any parse error` |
|       - | 1626 | ` * in the body php has already read. The caller reports the buffer at that` |
|       - | 1627 | ` * point.` |
|       - | 1628 | ` *` |
|       - | 1629 | ` * Returns the severity it wrote: E_ERROR, E_WARNING, or 0 for a clean` |
|       - | 1630 | ` * declaration.` |
|       - | 1631 | ` */` |
|    4828 | 1632 | `static sxi32 GenStateCheckMagicMethod(` |
|       - | 1633 | `	ph7_class *pClass,` |
|       - | 1634 | `	const SyString *pName,` |
|       - | 1635 | `	ph7_class_method *pMeth,` |
|       - | 1636 | `	char *zErr,` |
|       - | 1637 | `	int nErrBuf` |
|       - | 1638 | `	)` |
|       5 | 1639 | `{` |
|    4833 | 1640 | `	const MagicMethodRule *pRule = GenStateMagicMethodRule(pName);` |
|    4833 | 1641 | `	if( pRule == 0 ){` |
|    3605 | 1642 | `		return 0;` |
|       - | 1643 | `	}` |
|    1233 | 1644 | `	if( pRule->nArgs >= 0 ){` |
|       - | 1645 | `		/* php counts DECLARED parameters — an optional one counts` |
|       - | 1646 | ``		 * (`__destruct($a = null)` is rejected) and the variadic tail does not`` |
|       - | 1647 | ``		 * (`__clone(...$a)` declares zero and passes, `__get(...$a)` declares`` |
|       - | 1648 | `		 * zero where one is required and does not). */` |
|     595 | 1649 | `		sxu32 nDecl = SySetUsed(&pMeth->sFunc.aArgs);` |
|     595 | 1650 | `		sxu32 nGiven = 0;` |
|       - | 1651 | `		sxu32 n;` |
|    1035 | 1652 | `		for( n = 0 ; n < nDecl ; ++n ){` |
|     445 | 1653 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,n);` |
|     445 | 1654 | `			if( pArg && (pArg->iFlags & VM_FUNC_ARG_VARIADIC) == 0 ){` |
|     443 | 1655 | `				nGiven++;` |
|     219 | 1656 | `			}` |
|     225 | 1657 | `		}` |
|     595 | 1658 | `		if( nGiven != (sxu32)pRule->nArgs ){` |
|       9 | 1659 | `			if( pRule->nArgs == 0 ){` |
|       4 | 1660 | `				SyBufferFormat(zErr,nErrBuf,"Method %z::%z() cannot take arguments",` |
|       1 | 1661 | `					&pClass->sName,pName);` |
|       2 | 1662 | `			}else{` |
|       8 | 1663 | `				SyBufferFormat(zErr,nErrBuf,"Method %z::%z() must take exactly %d argument%s",` |
|       6 | 1664 | `					&pClass->sName,pName,pRule->nArgs,pRule->nArgs == 1 ? "" : "s");` |
|       - | 1665 | `			}` |
|       9 | 1666 | `			return E_ERROR;` |
|       - | 1667 | `		}` |
|       - | 1668 | `		/* None of the arguments the engine builds may be by-reference — there is` |
|       - | 1669 | `		 * no caller variable to write back to. php checks as many arguments as` |
|       - | 1670 | `		 * the rule counts, and the count above already skipped variadics, so` |
|       - | 1671 | `		 * this walk skips them the same way. */` |
|    1019 | 1672 | `		for( n = 0 ; n < nDecl ; ++n ){` |
|     436 | 1673 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,n);` |
|     436 | 1674 | `			if( pArg == 0 \|\| (pArg->iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|       3 | 1675 | `				continue;` |
|       - | 1676 | `			}` |
|     434 | 1677 | `			if( pArg->iFlags & VM_FUNC_ARG_BY_REF ){` |
|       4 | 1678 | `				SyBufferFormat(zErr,nErrBuf,"Method %z::%z() cannot take arguments by reference",` |
|       1 | 1679 | `					&pClass->sName,pName);` |
|       3 | 1680 | `				return E_ERROR;` |
|       - | 1681 | `			}` |
|     218 | 1682 | `		}` |
|     291 | 1683 | `	}` |
|       - | 1684 | `	/* Static-ness. Whether the engine has a receiver for a magic method is not` |
|       - | 1685 | ``	 * the declaration's to choose: `__callStatic` and `__set_state` are reached`` |
|       - | 1686 | `	 * with a class and nothing else, every other row is reached through an` |
|       - | 1687 | `	 * object. PHL took the declaration at its word and then dispatched the` |
|       - | 1688 | ``	 * method anyway — a `static function __get()` ran with no `$this` at all,`` |
|       - | 1689 | `	 * so a hook property table or a lazy-loading accessor read whatever the` |
|       - | 1690 | `	 * unbound scope happened to hold. php checks this after the arity, which is` |
|       - | 1691 | ``	 * why `static function __get($a,$b)` reports the count first. */`` |
|    1225 | 1692 | `	if( pRule->bStatic != ((pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0) ){` |
|       8 | 1693 | `		SyBufferFormat(zErr,nErrBuf,"Method %z::%z() %s be static",` |
|       4 | 1694 | `			&pClass->sName,pName,pRule->bStatic ? "must" : "cannot");` |
|       6 | 1695 | `		return E_ERROR;` |
|       - | 1696 | `	}` |
|       - | 1697 | `	/* Visibility. This one is a WARNING: php names the declaration and then` |
|       - | 1698 | ``	 * dispatches the method anyway, because the engine calling `__get` is not`` |
|       - | 1699 | `	 * the outside world reaching for a private member. PHL was silent at the` |
|       - | 1700 | ``	 * declaration and threw `Call to private method C::__get()` at the ACCESS —`` |
|       - | 1701 | `	 * the one rule of this family that changed what a program php RUNS does,` |
|       - | 1702 | `	 * and it killed the script. The dispatch half is` |
|       - | 1703 | `	 * PH7_MagicMethodMustBePublic, read by the runtime visibility gate. */` |
|    1221 | 1704 | `	if( pRule->bPublic && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|      51 | 1705 | `		SyBufferFormat(zErr,nErrBuf,"The magic method %z::%z() must have public visibility",` |
|      16 | 1706 | `			&pClass->sName,pName);` |
|      35 | 1707 | `		return E_WARNING;` |
|       - | 1708 | `	}` |
|       - | 1709 | ``	/* A return type on the two methods that have no return VALUE. `new C` is the`` |
|       - | 1710 | `	 * instance, never whatever __construct returned, and __destruct is called by` |
|       - | 1711 | `	 * the engine at a point with nowhere to put an answer — so php rejects any` |
|       - | 1712 | ``	 * declared type on either, `void` and `never` included, rather than let a`` |
|       - | 1713 | ``	 * declaration promise something no caller can read. (`__clone` is NOT in`` |
|       - | 1714 | ``	 * this row: `: void` on it is valid php.) PHL enforced the declared type at`` |
|       - | 1715 | ``	 * runtime instead, so `__construct(): int` raised a TypeError at every`` |
|       - | 1716 | `	 * instantiation — a diagnostic on the CALL for a mistake in the` |
|       - | 1717 | `	 * declaration. */` |
|    1184 | 1718 | `	if( pRule->bNoReturnType` |
|    1185 | 1719 | `	 && (pMeth->sFunc.nReturnType > 0` |
|     588 | 1720 | `	  \|\| SyStringLength(&pMeth->sFunc.sReturnClass) > 0` |
|     586 | 1721 | `	  \|\| SySetUsed(&pMeth->sFunc.aReturnUnion) > 0) ){` |
|       8 | 1722 | `		SyBufferFormat(zErr,nErrBuf,"Method %z::%z() cannot declare a return type",` |
|       2 | 1723 | `			&pClass->sName,pName);` |
|       6 | 1724 | `		return E_ERROR;` |
|       - | 1725 | `	}` |
|    1185 | 1726 | `	return 0;` |
|    2419 | 1727 | `}` |
|       - | 1728 | `/*` |
|       - | 1729 | ` * Raise the declaration diagnostic parked above (GenStateCheckMagicMethod's magic-method` |
|       - | 1730 | ` * rules, or the final-private one beside its call), once, and disarm it.` |
|       - | 1731 | ` * Suppressed when this declaration has already reported a fatal php decides` |
|       - | 1732 | ` * FIRST — a redeclaration, an abstract method in a non-abstract class, a parse` |
|       - | 1733 | ` * error in the body — since php stops at its own first fatal.` |
|       - | 1734 | ` */` |
|    4804 | 1735 | `static sxi32 GenStateRaiseMagicDiag(` |
|       - | 1736 | `	ph7_gen_state *pGen,` |
|       - | 1737 | `	sxi32 *pnSeverity,   /* IN/OUT: the parked severity, zeroed here */` |
|       - | 1738 | `	const char *zErr,` |
|       - | 1739 | `	sxu32 nLine,` |
|       - | 1740 | `	sxu32 nErrEntry      /* pGen->nErr when this declaration started */` |
|       - | 1741 | `	)` |
|       5 | 1742 | `{` |
|    4809 | 1743 | `	sxi32 rc = SXRET_OK;` |
|    4809 | 1744 | `	if( *pnSeverity != 0 ){` |
|      54 | 1745 | `		if( pGen->nErr == nErrEntry ){` |
|      54 | 1746 | `			rc = PH7_GenCompileError(pGen,*pnSeverity,nLine,"%s",zErr);` |
|      25 | 1747 | `		}` |
|      54 | 1748 | `		*pnSeverity = 0;` |
|      25 | 1749 | `	}` |
|    4809 | 1750 | `	return rc;` |
|       5 | 1751 | `}` |
|       - | 1752 | `/*` |
|       - | 1753 | ` * Compile a class method.` |
|       - | 1754 | ` *` |
|       - | 1755 | ` * Refer to the official documentation for more information` |
|       - | 1756 | ` * on the powerful extension introduced by the PH7 engine` |
|       - | 1757 | ` * to the OO subsystem such as full type hinting,method` |
|       - | 1758 | ` * overloading and many more.` |
|       - | 1759 | ` */` |
|    4832 | 1760 | `static sxi32 GenStateCompileClassMethod(` |
|       - | 1761 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 1762 | `	sxi32 iProtection,   /* Visibility level */` |
|       - | 1763 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - | 1764 | `	int doBody,          /* TRUE to process method body */` |
|       - | 1765 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - | 1766 | `	)` |
|       5 | 1767 | `{` |
|    4837 | 1768 | `	sxu32 nLine = pGen->pIn->nLine;` |
|    4837 | 1769 | `	sxu32 nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|    4837 | 1770 | `	sxu32 nErrEntry = pGen->nErr; /* Errors already reported when this declaration started */` |
|       - | 1771 | `	char zMagicErr[256];          /* Pending magic-method rule violation, reported at the end */` |
|    4837 | 1772 | `	sxi32 nMagicSeverity = 0;     /* E_ERROR / E_WARNING while zMagicErr is still unreported */` |
|    4837 | 1773 | `	int bMagicFatal = FALSE;      /* The parked diagnostic was a fatal: do not install the method */` |
|       - | 1774 | `	ph7_class_method *pMeth;` |
|       - | 1775 | `	sxi32 iFuncFlags;` |
|       - | 1776 | `	SyString *pName;` |
|       - | 1777 | `	SyToken *pEnd;` |
|       - | 1778 | `	sxi32 rc;` |
|       - | 1779 | `	/* Extract visibility level */` |
|    4837 | 1780 | `	iProtection = GetProtectionLevel(iProtection);` |
|    4837 | 1781 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|    4837 | 1782 | `	iFuncFlags = 0;` |
|    4837 | 1783 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 1784 | `		/* Invalid method name */` |
|     ! 0 | 1785 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 1786 | `		if( rc == SXERR_ABORT ){` |
|       - | 1787 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1788 | `			return SXERR_ABORT;` |
|       - | 1789 | `		}` |
|     ! 0 | 1790 | `		goto Synchronize;` |
|       - | 1791 | `	}` |
|    4837 | 1792 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 1793 | `		/* Return by reference,remember that */` |
|       6 | 1794 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 1795 | `		/* Jump the '&' token */` |
|       6 | 1796 | `		pGen->pIn++;` |
|       2 | 1797 | `	}` |
|    4837 | 1798 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 1799 | `		/* Invalid method name */` |
|     ! 0 | 1800 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 1801 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1802 | `			return SXERR_ABORT;` |
|       - | 1803 | `		}` |
|     ! 0 | 1804 | `		goto Synchronize;` |
|       - | 1805 | `	}` |
|       - | 1806 | `	/* Peek method name */` |
|    4837 | 1807 | `	pName = &pGen->pIn->sData;` |
|    4837 | 1808 | `	nLine = pGen->pIn->nLine;` |
|       - | 1809 | `	/* Jump the method name */` |
|    4837 | 1810 | `	pGen->pIn++;` |
|    4837 | 1811 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - | 1812 | `		/* Abstract method. php has THREE answers here and this had one:` |
|       - | 1813 | `		 *` |
|       - | 1814 | `		 *  - an ENUM may not declare an abstract method at all, whatever its` |
|       - | 1815 | `		 *    visibility ("Enum method E::m() must not be abstract");` |
|       - | 1816 | `		 *  - a TRAIT may declare a PRIVATE one -- php 8.0 allowed it, and` |
|       - | 1817 | `		 *    symfony/messenger's BatchHandlerTrait is written that way, so a` |
|       - | 1818 | `		 *    refusal here stops a real component from compiling;` |
|       - | 1819 | ``		 *  - a class (abstract or not) refuses it, in php's own words: `Abstract`` |
|       - | 1820 | ``		 *    function C::m() cannot be declared private`, not this file's older`` |
|       - | 1821 | `		 *    "Access type for abstract method" sentence, which php keeps for an` |
|       - | 1822 | `		 *    INTERFACE member (and which the interface path already spells). */` |
|     227 | 1823 | `		if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|     ! 0 | 1824 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 1825 | `				"Enum method %z::%z() must not be abstract",&pClass->sName,pName);` |
|     ! 0 | 1826 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1827 | `				return SXERR_ABORT;` |
|       - | 1828 | `			}` |
|     222 | 1829 | `		}else if( iProtection == PH7_CLASS_PROT_PRIVATE` |
|     117 | 1830 | `		       && (pClass->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 1831 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 1832 | `				"Abstract function %z::%z() cannot be declared private",` |
|     ! 0 | 1833 | `				&pClass->sName,pName);` |
|     ! 0 | 1834 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1835 | `				return SXERR_ABORT;` |
|       - | 1836 | `			}` |
|     ! 0 | 1837 | `		}` |
|       - | 1838 | `		/* Assemble method signature only */` |
|     227 | 1839 | `		doBody = FALSE;` |
|     111 | 1840 | `	}` |
|    4837 | 1841 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 1842 | `		/* Syntax error */` |
|     ! 0 | 1843 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 | 1844 | `		if( rc == SXERR_ABORT ){` |
|       - | 1845 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1846 | `			return SXERR_ABORT;` |
|       - | 1847 | `		}` |
|     ! 0 | 1848 | `		goto Synchronize;` |
|       - | 1849 | `	}` |
|       - | 1850 | `	/* Allocate a new class_method instance */` |
|    4837 | 1851 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|    4837 | 1852 | `	if( pMeth == 0 ){` |
|     ! 0 | 1853 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 1854 | `		return SXERR_ABORT;` |
|       - | 1855 | `	}` |
|    4837 | 1856 | `	pMeth->sFunc.nLine = nKwLine;` |
|    4837 | 1857 | `	GenStateConsumeDoc(&(*pGen),&pMeth->sFunc.sDoc);` |
|    4837 | 1858 | `	if( GenStateConsumeAttrs(&(*pGen),&pMeth->sFunc.aAttrs) == SXERR_ABORT ){` |
|     ! 0 | 1859 | `		return SXERR_ABORT;` |
|       - | 1860 | `	}` |
|    4837 | 1861 | `	if( GenStateCheckAttrPlacement(&(*pGen),&pMeth->sFunc.aAttrs,4,4,0,0) == SXERR_ABORT ){` |
|     ! 0 | 1862 | `		return SXERR_ABORT;` |
|       - | 1863 | `	}` |
|       - | 1864 | `	/* Jump the left parenthesis '(' */` |
|    4837 | 1865 | `	pGen->pIn++;` |
|    4837 | 1866 | `	pEnd = 0; /* cc warning */` |
|       - | 1867 | `	/* Delimit the method signature */` |
|    4837 | 1868 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    4837 | 1869 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 1870 | `		/* Syntax error */` |
|       3 | 1871 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 | 1872 | `		if( rc == SXERR_ABORT ){` |
|       - | 1873 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1874 | `			return SXERR_ABORT;` |
|       - | 1875 | `		}` |
|       3 | 1876 | `		goto Synchronize;` |
|       - | 1877 | `	}` |
|       - | 1878 | `	{` |
|    4835 | 1879 | `		int bIsCtor = 0;` |
|    4835 | 1880 | `		int bAbstractCtor = 0;` |
|       - | 1881 | `		/* Only __construct is the constructor (PHP-4 class-name constructors removed` |
|       - | 1882 | `		 * in 8.0): a method named like the class is a plain method, so promoted` |
|       - | 1883 | `		 * properties in it are rejected exactly as php does elsewhere. */` |
|    4830 | 1884 | `		if( pName->nByte == sizeof("__construct") - 1` |
|    2841 | 1885 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0 ){` |
|     545 | 1886 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      15 | 1887 | `				bAbstractCtor = 1;` |
|       9 | 1888 | `			}else{` |
|     533 | 1889 | `				bIsCtor = 1;` |
|       - | 1890 | `			}` |
|     270 | 1891 | `		}` |
|    4835 | 1892 | `		if( pGen->pIn < pEnd ){` |
|       - | 1893 | `			/* Collect method arguments */` |
|    1803 | 1894 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|    1803 | 1895 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1896 | `				return SXERR_ABORT;` |
|       - | 1897 | `			}` |
|     899 | 1898 | `		}` |
|       - | 1899 | `	}` |
|       - | 1900 | `	/* Point past ')' and parse optional return type ': type' */` |
|    4835 | 1901 | `	pGen->pIn = &pEnd[1];` |
|       - | 1902 | `	{` |
|    4835 | 1903 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|    4835 | 1904 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 | 1905 | `			return SXERR_ABORT;` |
|    4835 | 1906 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       2 | 1907 | `			goto Synchronize;` |
|       - | 1908 | `		}` |
|       - | 1909 | `	}` |
|       - | 1910 | `	/* php's #[\NoDiscard] declaration rules, which want the return type. */` |
|    5249 | 1911 | `	if( GenStateApplyNoDiscard(&(*pGen),&pMeth->sFunc,pClass,` |
|    4828 | 1912 | `			pName->nByte == sizeof("__construct")-1` |
|    2835 | 1913 | `			 && SyStrnicmp(pName->zString,"__construct",sizeof("__construct")-1) == 0)` |
|    2419 | 1914 | `		== SXERR_ABORT ){` |
|     ! 0 | 1915 | `		return SXERR_ABORT;` |
|       - | 1916 | `	}` |
|       - | 1917 | `	/* php's compile-time magic-method declaration rules, DECIDED here — with the` |
|       - | 1918 | `	 * signature in hand and before the __toString return-type rule below, which` |
|       - | 1919 | ``	 * is php's own order (`static function __toString($a): int` reports the`` |
|       - | 1920 | `	 * arity). Reported at the end of this function; see zMagicErr there. */` |
|    4833 | 1921 | `	nMagicSeverity = GenStateCheckMagicMethod(pClass,pName,pMeth,zMagicErr,(int)sizeof(zMagicErr));` |
|    4828 | 1922 | `	if( nMagicSeverity == 0` |
|    4804 | 1923 | `	 && (pMeth->iFlags & PH7_CLASS_ATTR_FINAL)` |
|    2399 | 1924 | `	 && pMeth->iProtection == PH7_CLASS_PROT_PRIVATE` |
|      17 | 1925 | `	 && !(pName->nByte == sizeof("__construct")-1` |
|       3 | 1926 | `	   && SyStrnicmp(pName->zString,"__construct",sizeof("__construct")-1) == 0) ){` |
|       - | 1927 | `		/* Not a magic rule, but the same KIND of rule and the same parking: php` |
|       - | 1928 | `		 * checks a declaration php itself decides the meaning of. A private method` |
|       - | 1929 | ``		 * is never overridden, so `final` on one says nothing — php WARNS here (8.0+)`` |
|       - | 1930 | `		 * and compiles the class. PHL was silent at the declaration and then fataled` |
|       - | 1931 | `		 * at the SUBCLASS that reused the name ("Cannot override final method"), a` |
|       - | 1932 | `		 * class php accepts; the inheritance half is in PH7_ClassInherit.` |
|       - | 1933 | `		 *` |
|       - | 1934 | `		 * The CONSTRUCTOR is php's one exemption, and it is a deliberate one:` |
|       - | 1935 | ``		 * `final private function __construct()` is the singleton idiom -- private`` |
|       - | 1936 | ``		 * to stop `new`, final to stop a subclass widening it back to public -- so`` |
|       - | 1937 | `		 * the modifier does say something there. Every other private method warns,` |
|       - | 1938 | ``		 * `__destruct`, `__clone` and a static one included. PHPUnit's TestSuite`` |
|       - | 1939 | `		 * declares exactly this and drew the warning on every single run. */` |
|       3 | 1940 | `		SyBufferFormat(zMagicErr,sizeof(zMagicErr),` |
|       - | 1941 | `			"Private methods cannot be final as they are never overridden by other classes");` |
|       3 | 1942 | `		nMagicSeverity = E_WARNING;` |
|       1 | 1943 | `	}` |
|    4833 | 1944 | `	if( nMagicSeverity == E_ERROR ){` |
|       - | 1945 | `		/* Suppress the __toString rule below: php never reaches it on a` |
|       - | 1946 | `		 * declaration the magic rules already rejected. */` |
|      20 | 1947 | `		bMagicFatal = TRUE;` |
|      20 | 1948 | `		goto SkipToStringType;` |
|       - | 1949 | `	}` |
|       - | 1950 | `	/*` |
|       - | 1951 | ``	 * php gives __toString() an IMPLICIT `string` return type. That is what makes`` |
|       - | 1952 | ``	 * `return 42` coerce to "42" and `return null` / an array / an object / falling`` |
|       - | 1953 | `	 * off the end raise` |
|       - | 1954 | `	 *   C::__toString(): Return value must be of type string, X returned` |
|       - | 1955 | `	 * PHL enforced DECLARED return types only, so an undeclared __toString could` |
|       - | 1956 | `	 * answer anything and MemObjStringValue fell back to the "Object" placeholder` |
|       - | 1957 | `	 * for whatever was not a non-empty string. Installing the type here reuses the` |
|       - | 1958 | `	 * enforcement that already matches php byte for byte.` |
|       - | 1959 | `	 *` |
|       - | 1960 | `	 * sReturnTypeName is filled in as well, for two reasons: reflection reports the` |
|       - | 1961 | `	 * implicit type exactly as php does (hasReturnType() TRUE, getReturnType()` |
|       - | 1962 | `	 * "string" for an undeclared __toString), and the generator-return-type fatal` |
|       - | 1963 | ``	 * renders from it — so a __toString with a `yield` in it now reports php's`` |
|       - | 1964 | `	 * "Generator return type must be a supertype of Generator, string given".` |
|       - | 1965 | `	 *` |
|       - | 1966 | ``	 * Declaring any OTHER return type is php's own compile fatal, `?string`, a`` |
|       - | 1967 | ``	 * union, `mixed`, `static` and `void` included. (php checks`` |
|       - | 1968 | ``	 * "A void method must not return a value" FIRST when a `: void` __toString also`` |
|       - | 1969 | `	 * returns a value; PHL has no such check yet, so it reports this one instead —` |
|       - | 1970 | `	 * both reject, on doubly-invalid input only.)` |
|       - | 1971 | `	 */` |
|    4812 | 1972 | `	if( pName->nByte == sizeof("__toString")-1` |
|    2662 | 1973 | `	 && SyStrnicmp(pName->zString,"__toString",sizeof("__toString")-1) == 0 ){` |
|     181 | 1974 | `		ph7_vm_func *pTsFunc = &pMeth->sFunc;` |
|     181 | 1975 | `		int bTsDeclared = pTsFunc->nReturnType > 0 \|\| SySetUsed(&pTsFunc->aReturnUnion) > 0;` |
|     181 | 1976 | `		if( bTsDeclared ){` |
|     104 | 1977 | `			if( pTsFunc->nReturnType != MEMOBJ_STRING` |
|     103 | 1978 | `			 \|\| SySetUsed(&pTsFunc->aReturnUnion) > 0` |
|     107 | 1979 | `			 \|\| (pTsFunc->iFlags & VM_FUNC_RETURN_NULLABLE) ){` |
|       - | 1980 | `				/* php raises this one AFTER the magic rules, so a parked` |
|       - | 1981 | `				 * visibility warning is php's first line here rather than a` |
|       - | 1982 | `				 * casualty of the fatal about to be counted. */` |
|       6 | 1983 | `				if( GenStateRaiseMagicDiag(pGen,&nMagicSeverity,zMagicErr,` |
|       6 | 1984 | `						nKwLine,nErrEntry) == SXERR_ABORT ){` |
|     ! 0 | 1985 | `					return SXERR_ABORT;` |
|       - | 1986 | `				}` |
|       8 | 1987 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 1988 | `					"%z::%z(): Return type must be string when declared",` |
|       2 | 1989 | `					&pClass->sName,pName);` |
|       6 | 1990 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1991 | `					return SXERR_ABORT;` |
|       - | 1992 | `				}` |
|       6 | 1993 | `				goto Synchronize;` |
|       - | 1994 | `			}` |
|      55 | 1995 | `		}else{` |
|      77 | 1996 | `			char *zTsType = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       - | 1997 | `				"string",sizeof("string")-1);` |
|      77 | 1998 | `			pTsFunc->nReturnType = MEMOBJ_STRING;` |
|      77 | 1999 | `			if( zTsType ){` |
|      77 | 2000 | `				SyStringInitFromBuf(&pTsFunc->sReturnTypeName,zTsType,sizeof("string")-1);` |
|      36 | 2001 | `			}` |
|       - | 2002 | `		}` |
|      86 | 2003 | `	}` |
|     ! 0 | 2004 | `SkipToStringType:` |
|       - | 2005 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|       - | 2006 | `	 * property init/typecheck is handled by the generic typed-property path` |
|       - | 2007 | `	 * since we mint real ph7_class_attr entries. */` |
|       - | 2008 | `	{` |
|    4829 | 2009 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|       - | 2010 | `		sxu32 i;` |
|    7359 | 2011 | `		for( i = 0; i < nArg; i++ ){` |
|    2545 | 2012 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|       - | 2013 | `			ph7_class_attr *pAttr;` |
|    2545 | 2014 | `			sxi32 iAttrFlags = 0;` |
|       - | 2015 | `			int bArgTyped;` |
|    2545 | 2016 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|    2367 | 2017 | `				continue;` |
|       - | 2018 | `			}` |
|       - | 2019 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|       - | 2020 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|       - | 2021 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|     140 | 2022 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|     201 | 2023 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|     183 | 2024 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       3 | 2025 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 2026 | `					"Cannot declare variadic promoted property");` |
|       3 | 2027 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 2028 | `					return SXERR_ABORT;` |
|       - | 2029 | `				}` |
|       3 | 2030 | `				goto Synchronize;` |
|       - | 2031 | `			}` |
|       - | 2032 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|       - | 2033 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|       - | 2034 | `			 * appear as an alternative of a union type. */` |
|     181 | 2035 | `			if( bArgTyped ){` |
|     212 | 2036 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|     138 | 2037 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|     138 | 2038 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|      69 | 2039 | `					"Property %z::$%z cannot have type %z",nLine);` |
|     143 | 2040 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 2041 | `					return SXERR_ABORT;` |
|     143 | 2042 | `				}else if( rc != SXRET_OK ){` |
|       6 | 2043 | `					goto Synchronize;` |
|       - | 2044 | `				}` |
|      67 | 2045 | `			}` |
|       - | 2046 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|     177 | 2047 | `			if( GenStateExtractProperty(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|       4 | 2048 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 | 2049 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|       3 | 2050 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 2051 | `					return SXERR_ABORT;` |
|       - | 2052 | `				}` |
|       3 | 2053 | `				goto Synchronize;` |
|       - | 2054 | `			}` |
|     175 | 2055 | `			if( bArgTyped ){` |
|     137 | 2056 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      66 | 2057 | `			}` |
|     175 | 2058 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|       3 | 2059 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|       1 | 2060 | `			}` |
|     175 | 2061 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|       8 | 2062 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|       3 | 2063 | `			}` |
|     175 | 2064 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|       - | 2065 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|       - | 2066 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|      45 | 2067 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       4 | 2068 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 | 2069 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|       3 | 2070 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 2071 | `						return SXERR_ABORT;` |
|       - | 2072 | `					}` |
|       3 | 2073 | `					goto Synchronize;` |
|       - | 2074 | `				}` |
|      43 | 2075 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|      19 | 2076 | `			}` |
|     173 | 2077 | `			if( pArg->iFlags & VM_FUNC_ARG_FINAL ){` |
|       - | 2078 | ``				/* PHP 8.4's `final` on a promoted property. No "final and private"`` |
|       - | 2079 | `				 * screen here: php refuses that pair in a CLASS BODY and accepts it` |
|       - | 2080 | ``				 * on a promoted parameter (`final private int $p` reflects as`` |
|       - | 2081 | `				 * modifiers 36), which is php's own asymmetry, not a gap. */` |
|       7 | 2082 | `				iAttrFlags \|= PH7_CLASS_ATTR_FINAL;` |
|       3 | 2083 | `			}` |
|     173 | 2084 | `			if( pArg->iFlags & (VM_FUNC_ARG_PRIV_SET\|VM_FUNC_ARG_PROT_SET) ){` |
|       - | 2085 | `				/* Asymmetric set-visibility on a promoted property (PHP 8.4) */` |
|       5 | 2086 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|     ! 0 | 2087 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 2088 | `						"Property with asymmetric visibility %z::$%z must have type",` |
|     ! 0 | 2089 | `						&pClass->sName,&pArg->sName);` |
|     ! 0 | 2090 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 2091 | `						return SXERR_ABORT;` |
|       - | 2092 | `					}` |
|     ! 0 | 2093 | `					goto Synchronize;` |
|       - | 2094 | `				}` |
|       5 | 2095 | `				iAttrFlags \|= (pArg->iFlags & VM_FUNC_ARG_PRIV_SET)` |
|       2 | 2096 | `					? PH7_CLASS_ATTR_PRIVATE_SET : PH7_CLASS_ATTR_PROTECTED_SET;` |
|       2 | 2097 | `			}` |
|     173 | 2098 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|     173 | 2099 | `			if( pAttr == 0 ){` |
|     ! 0 | 2100 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2101 | `				return SXERR_ABORT;` |
|       - | 2102 | `			}` |
|     173 | 2103 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|     137 | 2104 | `				pAttr->nType = pArg->nType;` |
|     137 | 2105 | `				pAttr->sClass = pArg->sClass;` |
|     137 | 2106 | `				pAttr->sTypeName = pArg->sTypeName;` |
|     137 | 2107 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|       - | 2108 | `					sxu32 k;` |
|      20 | 2109 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|      14 | 2110 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|      14 | 2111 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|       8 | 2112 | `					}` |
|       3 | 2113 | `				}` |
|      66 | 2114 | `			}` |
|       - | 2115 | ``			/* A promoted parameter's `#[...]` belongs to BOTH members in php: the`` |
|       - | 2116 | `			 * ReflectionParameter reports it and so does the ReflectionProperty,` |
|       - | 2117 | ``			 * which is what makes `#[\Override] public $p` in a constructor`` |
|       - | 2118 | `			 * signature a PROPERTY claim. The records are shared, not copied --` |
|       - | 2119 | `			 * the parameter owns them for the VM's lifetime. */` |
|       - | 2120 | `			{` |
|     173 | 2121 | `				ph7_attribute *aSrc = (ph7_attribute *)SySetBasePtr(&pArg->aAttrs);` |
|       - | 2122 | `				sxu32 k;` |
|     187 | 2123 | `				for( k = 0 ; k < SySetUsed(&pArg->aAttrs) ; k++ ){` |
|      17 | 2124 | `					SySetPut(&pAttr->aAttrs,(const void *)&aSrc[k]);` |
|      10 | 2125 | `				}` |
|       - | 2126 | `			}` |
|     173 | 2127 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|     173 | 2128 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 2129 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2130 | `				return SXERR_ABORT;` |
|       - | 2131 | `			}` |
|      89 | 2132 | `		}` |
|       - | 2133 | `	}` |
|    4819 | 2134 | `	if( doBody ){` |
|       - | 2135 | `		/* Compile method body */` |
|    4599 | 2136 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|    4599 | 2137 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2138 | `			return SXERR_ABORT;` |
|       - | 2139 | `		}` |
|       - | 2140 | `		/* The cursor sits just past the body's closing brace */` |
|    4599 | 2141 | `		pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|    2302 | 2142 | `	}else{` |
|       - | 2143 | `		/* Abstract/interface method: declaration ends at the ';' */` |
|     225 | 2144 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) ){` |
|     219 | 2145 | `			pMeth->sFunc.nEndLine = pGen->pIn->nLine;` |
|     107 | 2146 | `		}` |
|       - | 2147 | `		/* Only method signature is allowed */` |
|     225 | 2148 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|       - | 2149 | `			/* php words this as the declaration's problem rather than a missing` |
|       - | 2150 | `			 * token -- an abstract method (an interface's included, which is` |
|       - | 2151 | `			 * abstract by being one) is a promise, and a body makes it something` |
|       - | 2152 | `			 * else. The two kinds get the two nouns php uses. */` |
|       8 | 2153 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      11 | 2154 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 2155 | `					"%s function %z::%z() cannot contain body",` |
|       6 | 2156 | `					(pClass->iFlags & PH7_CLASS_INTERFACE) ? "Interface" : "Abstract",` |
|       3 | 2157 | `					&pClass->sName,pName);` |
|       8 | 2158 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 2159 | `					return SXERR_ABORT;` |
|       - | 2160 | `				}` |
|       8 | 2161 | `				return SXERR_CORRUPT;` |
|       - | 2162 | `			}` |
|     ! 0 | 2163 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|     ! 0 | 2164 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 | 2165 | `				if( rc == SXERR_ABORT ){` |
|       - | 2166 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 2167 | `					return SXERR_ABORT;` |
|       - | 2168 | `				}` |
|     ! 0 | 2169 | `				return SXERR_CORRUPT;` |
|       - | 2170 | `			}` |
|       - | 2171 | `	}` |
|       - | 2172 | `	/* php: an abstract method in a class that was not DECLARED abstract is a fatal,` |
|       - | 2173 | `	 * and it names the method -- distinct from the "contains N abstract methods"` |
|       - | 2174 | `	 * wording php uses for an unimplemented INHERITED one, which` |
|       - | 2175 | `	 * GenStateCheckAbstractMethods already handles. Interfaces and traits may carry` |
|       - | 2176 | `	 * abstract methods freely. */` |
|    4808 | 2177 | `	if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT)` |
|    2516 | 2178 | `	 && (pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0 ){` |
|       4 | 2179 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 2180 | `			"Class %z declares abstract method %z() and must therefore be declared abstract",` |
|       1 | 2181 | `			&pClass->sName,pName);` |
|       3 | 2182 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2183 | `			return SXERR_ABORT;` |
|       - | 2184 | `		}` |
|       3 | 2185 | `		return SXRET_OK;` |
|       - | 2186 | `	}` |
|       - | 2187 | `	/* php: two methods of the same name in one class body is a fatal, and the` |
|       - | 2188 | ``	 * comparison is case-INSENSITIVE (hMethod now matches that way), so `f` and`` |
|       - | 2189 | ``	 * `F` collide. php names the offending declaration with the spelling used at`` |
|       - | 2190 | `	 * the SECOND site. */` |
|    4811 | 2191 | `	if( PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte) != 0 ){` |
|      11 | 2192 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       3 | 2193 | `			"Cannot redeclare %z::%z()",&pClass->sName,pName);` |
|       8 | 2194 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2195 | `			return SXERR_ABORT;` |
|       - | 2196 | `		}` |
|       8 | 2197 | `		return SXRET_OK;` |
|       - | 2198 | `	}` |
|       - | 2199 | `	/* The magic-method rule this declaration broke (decided above, with the` |
|       - | 2200 | `	 * signature in hand). It is raised HERE because php raises it last of the` |
|       - | 2201 | `	 * declaration's fatals: a redeclaration, an abstract method in a class that` |
|       - | 2202 | `	 * is not abstract, and any parse error inside the body php has already read` |
|       - | 2203 | `	 * all win — and each of them has, by now, either returned or bumped nErr.` |
|       - | 2204 | `	 * php stops at its first fatal, so one is all this declaration reports. The` |
|       - | 2205 | ``	 * line is the `function` KEYWORD's, which is where php points once a`` |
|       - | 2206 | `	 * signature wraps across lines. */` |
|    4805 | 2207 | `	if( GenStateRaiseMagicDiag(pGen,&nMagicSeverity,zMagicErr,nKwLine,nErrEntry) == SXERR_ABORT ){` |
|     ! 0 | 2208 | `		return SXERR_ABORT;` |
|       - | 2209 | `	}` |
|    4805 | 2210 | `	if( bMagicFatal ){` |
|       - | 2211 | `		/* Never install a method php refused to declare. A WARNING falls through:` |
|       - | 2212 | `		 * php keeps the method and calls it. */` |
|      20 | 2213 | `		return SXRET_OK;` |
|       - | 2214 | `	}` |
|       - | 2215 | `	/* All done,install the method */` |
|    4789 | 2216 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|    4789 | 2217 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2218 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2219 | `		return SXERR_ABORT;` |
|       - | 2220 | `	}` |
|    4789 | 2221 | `	return SXRET_OK;` |
|       9 | 2222 | `Synchronize:` |
|       - | 2223 | `	/* Synchronize with the first semi-colon */` |
|      58 | 2224 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      40 | 2225 | `		pGen->pIn++;` |
|       4 | 2226 | `	}` |
|      22 | 2227 | `	return SXERR_CORRUPT;` |
|    2421 | 2228 | `}` |
|       - | 2229 | `/*` |
|       - | 2230 | ` * php's member-modifier RUN and the screens on it.` |
|       - | 2231 | ` *` |
|       - | 2232 | ` * Everything a class/trait/interface member may carry in front of the` |
|       - | 2233 | ``  * declaration it modifies -- the read visibility, an asymmetric `(set)` `` |
|       - | 2234 | ``  * visibility, `static`, `abstract`, `final` and the context-sensitive `readonly` `` |
|       - | 2235 | ` * -- in ANY order and each at most once. They are a SET in php's grammar, so` |
|       - | 2236 | `` * `final public static int $p` and `static final public $p` are one declaration`` |
|       - | 2237 | ` * each.` |
|       - | 2238 | ` *` |
|       - | 2239 | ` * The three body loops used to read ONE modifier per branch and then re-enter` |
|       - | 2240 | ` * their keyword ladder, which works only while every branch happens to be` |
|       - | 2241 | `` * reachable from every other: `static final public $p` fell out of the chain`` |
|       - | 2242 | `` * with "Unexpected token 'final'", and the `final` branch only ever led to a`` |
|       - | 2243 | ` * method or a constant -- so PHP 8.4's final PROPERTY was a parse error in every` |
|       - | 2244 | ` * one of its spellings, promoted parameter included. Reading the whole run in` |
|       - | 2245 | ` * one place makes the order irrelevant and gives the duplicate/combination rules` |
|       - | 2246 | `` * a single home; the caller dispatches on the token the run stops at (`const`,`` |
|       - | 2247 | `` * `function`, `var`, a type, a `$name`).`` |
|       - | 2248 | ` */` |
|       - | 2249 | ``#define GEN_MEMBER_PROP   0  /* `[type] $name`  */`` |
|       - | 2250 | ``#define GEN_MEMBER_CONST  1  /* `const NAME`    */`` |
|       - | 2251 | ``#define GEN_MEMBER_METHOD 2  /* `function name` */`` |
|       - | 2252 | ``#define GEN_MEMBER_VAR    3  /* the pre-5.0 `var $name` spelling */`` |
|       - | 2253 | `typedef struct GenMemberMods GenMemberMods;` |
|       - | 2254 | `struct GenMemberMods` |
|       - | 2255 | `{` |
|       - | 2256 | `	sxi32 iProtection;  /* read-visibility keyword; php's default is public */` |
|       - | 2257 | `	sxi32 iFlags;       /* PH7_CLASS_ATTR_* collected from the run */` |
|       - | 2258 | ``	sxi32 nSetVis;      /* the `(set)` visibility keyword, when one was written */`` |
|       - | 2259 | `	sxu32 nLine;        /* line the run starts on -- where php reports its refusals */` |
|       - | 2260 | `	int bAny;           /* TRUE once anything at all was consumed */` |
|       - | 2261 | `	int bVis,bSetVis,bStatic,bAbstract,bFinal,bReadonly;` |
|       - | 2262 | `};` |
|       - | 2263 | `/*` |
|       - | 2264 | ` * Read the run. A modifier written twice is php's own compile-time fatal, worded` |
|       - | 2265 | ` * per modifier -- and the two VISIBILITY kinds share one sentence, which is also` |
|       - | 2266 | `` * what php says for two DIFFERENT ones (`public private $p`).`` |
|       - | 2267 | ` *` |
|       - | 2268 | ` * Returns SXRET_OK, SXERR_SYNTAX when a rule above was reported (the caller` |
|       - | 2269 | ` * abandons the member), or SXERR_ABORT when the error budget is spent.` |
|       - | 2270 | ` */` |
|    8186 | 2271 | `static sxi32 GenStateReadMemberMods(ph7_gen_state *pGen,GenMemberMods *pMods)` |
|       5 | 2272 | `{` |
|    8191 | 2273 | `	pMods->iProtection = PH7_TKWRD_PUBLIC;` |
|    8191 | 2274 | `	pMods->iFlags = 0;` |
|    8191 | 2275 | `	pMods->nSetVis = 0;` |
|    8191 | 2276 | `	pMods->nLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0;` |
|    8191 | 2277 | `	pMods->bAny = pMods->bVis = pMods->bSetVis = 0;` |
|    8191 | 2278 | `	pMods->bStatic = pMods->bAbstract = pMods->bFinal = pMods->bReadonly = 0;` |
|   16391 | 2279 | `	while( pGen->pIn < pGen->pEnd ){` |
|   16391 | 2280 | `		const char *zTwice = 0;  /* the modifier php names; "" = the access-type sentence */` |
|   16391 | 2281 | `		int nSetTok = 0;` |
|       - | 2282 | `		sxi32 nSetVis;` |
|   16391 | 2283 | `		if( GenStateIsReadonly(pGen->pIn) ){` |
|       - | 2284 | ``			/* `readonly` is not a reserved word, so it arrives as a plain ID; at`` |
|       - | 2285 | `			 * modifier position it is always the modifier -- which is why php's` |
|       - | 2286 | ``			 * answer to `public readonly readonly $x` is the duplicate rule and`` |
|       - | 2287 | ``			 * not a property typed `readonly`. */`` |
|      81 | 2288 | `			if( pMods->bReadonly ){` |
|       3 | 2289 | `				zTwice = "readonly";` |
|       1 | 2290 | `			}` |
|      81 | 2291 | `			pMods->bReadonly = 1;` |
|      81 | 2292 | `			pMods->iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|      81 | 2293 | `			pGen->pIn++;` |
|   16353 | 2294 | `		}else if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|    5067 | 2295 | `			break;` |
|   14367 | 2296 | `		}else if( (nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok)) != 0 ){` |
|      45 | 2297 | `			if( pMods->bSetVis ){` |
|     ! 0 | 2298 | `				zTwice = "";` |
|     ! 0 | 2299 | `			}` |
|      45 | 2300 | `			pMods->bSetVis = 1;` |
|      45 | 2301 | `			pMods->nSetVis = nSetVis;` |
|      45 | 2302 | `			pMods->iFlags \|= GenStateSetVisFlag(nSetVis);` |
|      45 | 2303 | `			pGen->pIn += nSetTok;` |
|      24 | 2304 | `		}else{` |
|   14325 | 2305 | `			sxi32 nKw = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   14320 | 2306 | `			if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PRIVATE` |
|    7800 | 2307 | `			 \|\| nKw == PH7_TKWRD_PROTECTED ){` |
|    7055 | 2308 | `				if( pMods->bVis ){` |
|       3 | 2309 | `					zTwice = "";` |
|       1 | 2310 | `				}` |
|    7055 | 2311 | `				pMods->bVis = 1;` |
|    7055 | 2312 | `				pMods->iProtection = nKw;` |
|   10800 | 2313 | `			}else if( nKw == PH7_TKWRD_STATIC ){` |
|     855 | 2314 | `				if( pMods->bStatic ){` |
|       3 | 2315 | `					zTwice = "static";` |
|       1 | 2316 | `				}` |
|     855 | 2317 | `				pMods->bStatic = 1;` |
|     855 | 2318 | `				pMods->iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|    6850 | 2319 | `			}else if( nKw == PH7_TKWRD_ABSTRACT ){` |
|     105 | 2320 | `				if( pMods->bAbstract ){` |
|       3 | 2321 | `					zTwice = "abstract";` |
|       1 | 2322 | `				}` |
|     105 | 2323 | `				pMods->bAbstract = 1;` |
|     105 | 2324 | `				pMods->iFlags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|    6375 | 2325 | `			}else if( nKw == PH7_TKWRD_FINAL ){` |
|      97 | 2326 | `				if( pMods->bFinal ){` |
|       3 | 2327 | `					zTwice = "final";` |
|       1 | 2328 | `				}` |
|      97 | 2329 | `				pMods->bFinal = 1;` |
|      97 | 2330 | `				pMods->iFlags \|= PH7_CLASS_ATTR_FINAL;` |
|      51 | 2331 | `			}else{` |
|    6233 | 2332 | `				break; /* not a modifier -- the member itself starts here */` |
|       - | 2333 | `			}` |
|    8097 | 2334 | `			pGen->pIn++;` |
|       - | 2335 | `		}` |
|    8215 | 2336 | `		pMods->bAny = 1;` |
|    8215 | 2337 | `		if( zTwice ){` |
|       - | 2338 | `			sxi32 rc;` |
|      14 | 2339 | `			pGen->iFatalTrace = PH7_FATAL_TRACE_NONE;` |
|      19 | 2340 | `			rc = zTwice[0]` |
|      12 | 2341 | `				? PH7_GenCompileError(pGen,E_ERROR,pMods->nLine,` |
|       4 | 2342 | `					"Multiple %s modifiers are not allowed",zTwice)` |
|       6 | 2343 | `				: PH7_GenCompileError(pGen,E_ERROR,pMods->nLine,` |
|       - | 2344 | `					"Multiple access type modifiers are not allowed");` |
|      14 | 2345 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - | 2346 | `		}` |
|       5 | 2347 | `	}` |
|    8181 | 2348 | `	return SXRET_OK;` |
|    4098 | 2349 | `}` |
|       - | 2350 | ``/* php's name for a `(set)` visibility, as its refusals spell it. */`` |
|       2 | 2351 | `static const char * GenStateSetVisWord(sxi32 nKw)` |
|       1 | 2352 | `{` |
|       3 | 2353 | `	if( nKw == PH7_TKWRD_PRIVATE ){` |
|       3 | 2354 | `		return "private(set)";` |
|       - | 2355 | `	}` |
|     ! 0 | 2356 | `	return (nKw == PH7_TKWRD_PROTECTED) ? "protected(set)" : "public(set)";` |
|       2 | 2357 | `}` |
|       - | 2358 | `/*` |
|       - | 2359 | ` * The run, judged against the KIND of declaration it turned out to modify. php` |
|       - | 2360 | ` * refuses each combination with its own sentence, and the order tested below is` |
|       - | 2361 | ` * the order php reports them in when one declaration breaks several` |
|       - | 2362 | ``  * (`static abstract const` is the static rule, `public private(set) static const` `` |
|       - | 2363 | ` * the private(set) one).` |
|       - | 2364 | ` */` |
|    8168 | 2365 | `static sxi32 GenStateScreenMemberMods(ph7_gen_state *pGen,const GenMemberMods *pMods,` |
|       - | 2366 | `	int iKind,ph7_class *pClass)` |
|       5 | 2367 | `{` |
|    8173 | 2368 | `	const char *zBad = 0;   /* the modifier php names */` |
|    8173 | 2369 | `	const char *zWhere = 0; /* ...and what it was written on */` |
|       - | 2370 | `	sxi32 rc;` |
|    8173 | 2371 | `	if( iKind == GEN_MEMBER_CONST ){` |
|     677 | 2372 | `		zWhere = "a class constant";` |
|     677 | 2373 | `		if( pMods->bReadonly ){` |
|     ! 0 | 2374 | `			zBad = "readonly";` |
|     677 | 2375 | `		}else if( pMods->bSetVis ){` |
|       3 | 2376 | `			zBad = GenStateSetVisWord(pMods->nSetVis);` |
|     676 | 2377 | `		}else if( pMods->bStatic ){` |
|       3 | 2378 | `			zBad = "static";` |
|     674 | 2379 | `		}else if( pMods->bAbstract ){` |
|       3 | 2380 | `			zBad = "abstract";` |
|       6 | 2381 | `		}` |
|    7837 | 2382 | `	}else if( iKind == GEN_MEMBER_METHOD ){` |
|    4847 | 2383 | `		zWhere = "a method";` |
|    4847 | 2384 | `		if( pMods->bReadonly ){` |
|       3 | 2385 | `			zBad = "readonly";` |
|    4846 | 2386 | `		}else if( pMods->bSetVis ){` |
|     ! 0 | 2387 | `			zBad = GenStateSetVisWord(pMods->nSetVis);` |
|    4845 | 2388 | `		}else if( pMods->bFinal && pMods->bAbstract ){` |
|       3 | 2389 | `			zBad = "final";` |
|       3 | 2390 | `			zWhere = "an abstract method";` |
|       1 | 2391 | `		}` |
|    2426 | 2392 | `	}else{` |
|    2659 | 2393 | `		if( pMods->bFinal && pMods->bAbstract ){` |
|       3 | 2394 | `			zBad = "final";` |
|       3 | 2395 | `			zWhere = "an abstract property";` |
|    2658 | 2396 | `		}else if( pMods->bFinal && (pClass->iFlags & PH7_CLASS_INTERFACE) ){` |
|       - | 2397 | `			/* php words the interface case as the PROPERTY's problem rather than` |
|       - | 2398 | `			 * the modifier's: an interface property is a hooked REQUIREMENT, and a` |
|       - | 2399 | `			 * requirement no implementor may restate cannot be one. */` |
|       3 | 2400 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pMods->nLine,` |
|       - | 2401 | `				"Property in interface cannot be final");` |
|       3 | 2402 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|    2655 | 2403 | `		}else if( pMods->bFinal && pMods->iProtection == PH7_TKWRD_PRIVATE ){` |
|       - | 2404 | ``			/* A private property is invisible to a subclass, so `final` on one says`` |
|       - | 2405 | `			 * nothing php can honour. This is the PROPERTY rule only: php accepts` |
|       - | 2406 | `			 * the same pair on a PROMOTED constructor parameter (modifiers 36` |
|       - | 2407 | `			 * there) -- an asymmetry of php's own, reproduced rather than smoothed` |
|       - | 2408 | `			 * over. */` |
|       3 | 2409 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pMods->nLine,` |
|       - | 2410 | `				"Property cannot be both final and private");` |
|       3 | 2411 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - | 2412 | `		}` |
|       - | 2413 | `	}` |
|    8169 | 2414 | `	if( zBad == 0 ){` |
|    8157 | 2415 | `		return SXRET_OK;` |
|       - | 2416 | `	}` |
|      15 | 2417 | `	pGen->iFatalTrace = PH7_FATAL_TRACE_NONE;` |
|      21 | 2418 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pMods->nLine,` |
|       6 | 2419 | `		"Cannot use the %s modifier on %s",zBad,zWhere);` |
|      15 | 2420 | `	return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|    4089 | 2421 | `}` |
|       - | 2422 | `/*` |
|       - | 2423 | `` * Peek the NAME a `const`/`function` keyword introduces, for the diagnostics php`` |
|       - | 2424 | ` * words with it. Returns 0 when the declaration is malformed enough that there` |
|       - | 2425 | ` * is no name to show (the caller then falls back to a nameless sentence).` |
|       - | 2426 | ` */` |
|       8 | 2427 | `static SyString * GenStateMemberNamePeek(ph7_gen_state *pGen)` |
|       3 | 2428 | `{` |
|      11 | 2429 | `	SyToken *p = pGen->pIn + 1;` |
|      11 | 2430 | `	if( p < pGen->pEnd && (p->nType & PH7_TK_AMPER) ){` |
|     ! 0 | 2431 | ``		p++; /* a by-reference method: `function &f()` */`` |
|     ! 0 | 2432 | `	}` |
|      11 | 2433 | `	if( p < pGen->pEnd && (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      11 | 2434 | `		return &p->sData;` |
|       - | 2435 | `	}` |
|     ! 0 | 2436 | `	return 0;` |
|       7 | 2437 | `}` |
|       - | 2438 | `/*` |
|       - | 2439 | ` * One MEMBER of a class/trait/interface body: the modifier run, php's screens on` |
|       - | 2440 | ` * it, and the declaration it turned out to modify. Shared by the three body` |
|       - | 2441 | ` * loops, which used to carry three near-identical modifier ladders -- and three` |
|       - | 2442 | ` * different sets of gaps.` |
|       - | 2443 | ` *` |
|       - | 2444 | `` * The enum `case` and the trait `use` statement are NOT members and never reach`` |
|       - | 2445 | ` * here: they take no modifiers, and their loops consume them first.` |
|       - | 2446 | ` *` |
|       - | 2447 | ` * Returns SXRET_OK, SXERR_SYNTAX when a refusal was reported (the caller` |
|       - | 2448 | ` * abandons the body), or SXERR_ABORT when the error budget is spent.` |
|       - | 2449 | ` */` |
|    8186 | 2450 | `static sxi32 GenStateCompileMember(ph7_gen_state *pGen,ph7_class *pClass,const char *zBody)` |
|       5 | 2451 | `{` |
|    8191 | 2452 | `	SyString *pName = &pClass->sName;` |
|       - | 2453 | `	GenMemberMods sMods;` |
|       - | 2454 | `	int iKind;` |
|       - | 2455 | `	sxi32 rc;` |
|    8191 | 2456 | `	rc = GenStateReadMemberMods(&(*pGen),&sMods);` |
|    8191 | 2457 | `	if( rc != SXRET_OK ){` |
|      14 | 2458 | `		return rc;` |
|       - | 2459 | `	}` |
|    8181 | 2460 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 2461 | `		rc = PH7_GenCompileError(pGen,E_PARSE,sMods.nLine,` |
|     ! 0 | 2462 | `			"Expecting member declaration inside %s '%z'",zBody,pName);` |
|     ! 0 | 2463 | `		return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - | 2464 | `	}` |
|       - | 2465 | `	/* What the run modifies. A '$' -- or a TYPE followed by one -- is a property;` |
|       - | 2466 | `	 * the three keywords are each their own declaration. */` |
|    8176 | 2467 | `	if( (pGen->pIn->nType & PH7_TK_DOLLAR)` |
|    7276 | 2468 | `	 \|\| GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|    2659 | 2469 | `		iKind = GEN_MEMBER_PROP;` |
|    6854 | 2470 | `	}else if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 2471 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|       - | 2472 | `			"Unexpected token '%z'. Expecting member declaration inside %s '%z'",` |
|     ! 0 | 2473 | `			&pGen->pIn->sData,zBody,pName);` |
|     ! 0 | 2474 | `		return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|     ! 0 | 2475 | `	}else{` |
|    5527 | 2476 | `		sxi32 nKw = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    5527 | 2477 | `		if( nKw == PH7_TKWRD_CONST ){` |
|     677 | 2478 | `			iKind = GEN_MEMBER_CONST;` |
|    5191 | 2479 | `		}else if( nKw == PH7_TKWRD_FUNCTION ){` |
|    4847 | 2480 | `			iKind = GEN_MEMBER_METHOD;` |
|    2431 | 2481 | `		}else if( nKw == PH7_TKWRD_VAR ){` |
|      10 | 2482 | `			iKind = GEN_MEMBER_VAR;` |
|       6 | 2483 | `		}else{` |
|     ! 0 | 2484 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|       - | 2485 | `				"Unexpected token '%z'. Expecting member declaration inside %s '%z'",` |
|     ! 0 | 2486 | `				&pGen->pIn->sData,zBody,pName);` |
|     ! 0 | 2487 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - | 2488 | `		}` |
|       - | 2489 | `	}` |
|    8181 | 2490 | `	if( iKind == GEN_MEMBER_VAR ){` |
|       - | 2491 | ``		/* `var $x` is the pre-5.0 spelling of `public $x` and takes NO other`` |
|       - | 2492 | `		 * modifier: php's parser is looking for a VARIABLE where the modifier run` |
|       - | 2493 | ``		 * left off, so `public var $x` and `final var $x` are parse errors naming`` |
|       - | 2494 | `		 * the token that is not one. */` |
|      10 | 2495 | `		if( sMods.bAny ){` |
|       3 | 2496 | `			rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,"variable");` |
|       3 | 2497 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - | 2498 | `		}` |
|       7 | 2499 | `		pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 | 2500 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 2501 | `			rc = PH7_GenSyntaxError(&(*pGen),` |
|     ! 0 | 2502 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn : 0,"variable");` |
|     ! 0 | 2503 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - | 2504 | `		}` |
|       7 | 2505 | `		iKind = GEN_MEMBER_PROP;` |
|       4 | 2506 | `	}else{` |
|    8173 | 2507 | `		rc = GenStateScreenMemberMods(&(*pGen),&sMods,iKind,pClass);` |
|    8173 | 2508 | `		if( rc != SXRET_OK ){` |
|      19 | 2509 | `			return rc;` |
|       - | 2510 | `		}` |
|       - | 2511 | `	}` |
|    8163 | 2512 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|       - | 2513 | `		/* An interface member is public by declaration, and its methods are` |
|       - | 2514 | ``		 * implicitly abstract -- so php refuses both `abstract` written out and`` |
|       - | 2515 | ``		 * `final`, naming the method in each. */`` |
|     201 | 2516 | `		if( sMods.iProtection != PH7_TKWRD_PUBLIC ){` |
|       8 | 2517 | `			SyString *pMember = (iKind == GEN_MEMBER_PROP) ? 0 : GenStateMemberNamePeek(&(*pGen));` |
|       8 | 2518 | `			if( iKind == GEN_MEMBER_PROP ){` |
|       - | 2519 | `				/* php words the PROPERTY case as the property's problem, and names` |
|       - | 2520 | `				 * neither of the two visibilities it refuses. */` |
|       3 | 2521 | `				rc = PH7_GenCompileError(pGen,E_ERROR,sMods.nLine,` |
|       - | 2522 | `					"Property in interface cannot be protected or private");` |
|       6 | 2523 | `			}else if( pMember ){` |
|       7 | 2524 | `				rc = PH7_GenCompileError(pGen,E_ERROR,sMods.nLine,` |
|       - | 2525 | `					"Access type for interface %s %z::%z%s must be public",` |
|       2 | 2526 | `					(iKind == GEN_MEMBER_CONST) ? "constant" : "method",pName,pMember,` |
|       2 | 2527 | `					(iKind == GEN_MEMBER_CONST) ? "" : "()");` |
|       3 | 2528 | `			}else{` |
|     ! 0 | 2529 | `				rc = PH7_GenCompileError(pGen,E_ERROR,sMods.nLine,` |
|       - | 2530 | `					"Access type for interface %s must be public",` |
|     ! 0 | 2531 | `					(iKind == GEN_MEMBER_CONST) ? "constant" : "method");` |
|       - | 2532 | `			}` |
|       8 | 2533 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - | 2534 | `		}` |
|     195 | 2535 | `		if( iKind == GEN_MEMBER_METHOD && (sMods.bFinal \|\| sMods.bAbstract) ){` |
|       6 | 2536 | `			SyString *pMember = GenStateMemberNamePeek(&(*pGen));` |
|       6 | 2537 | `			const char *zWhat = sMods.bFinal ? "final" : "abstract";` |
|       6 | 2538 | `			if( pMember ){` |
|       8 | 2539 | `				rc = PH7_GenCompileError(pGen,E_ERROR,sMods.nLine,` |
|       2 | 2540 | `					"Interface method %z::%z() must not be %s",pName,pMember,zWhat);` |
|       4 | 2541 | `			}else{` |
|     ! 0 | 2542 | `				rc = PH7_GenCompileError(pGen,E_ERROR,sMods.nLine,` |
|     ! 0 | 2543 | `					"Interface method must not be %s",zWhat);` |
|       - | 2544 | `			}` |
|       6 | 2545 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - | 2546 | `		}` |
|       - | 2547 | `		/* Both remaining kinds are abstract in an interface: a method has no body` |
|       - | 2548 | `		 * to compile, and a property is a HOOKED requirement (a plain one is` |
|       - | 2549 | `		 * GenStateCompileClassAttr's own "Interfaces may only include hooked` |
|       - | 2550 | `		 * properties"). */` |
|     191 | 2551 | `		if( iKind != GEN_MEMBER_CONST ){` |
|     151 | 2552 | `			sMods.iFlags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|      73 | 2553 | `		}` |
|      93 | 2554 | `	}` |
|    8153 | 2555 | `	if( iKind == GEN_MEMBER_CONST ){` |
|     669 | 2556 | `		return GenStateCompileClassConstant(&(*pGen),sMods.iProtection,sMods.iFlags,pClass);` |
|       - | 2557 | `	}` |
|    7489 | 2558 | `	if( iKind == GEN_MEMBER_METHOD ){` |
|       - | 2559 | `		/* An interface method is a SIGNATURE: it has no body to compile. */` |
|    7253 | 2560 | `		return GenStateCompileClassMethod(&(*pGen),sMods.iProtection,sMods.iFlags,` |
|    4832 | 2561 | `			(pClass->iFlags & PH7_CLASS_INTERFACE) ? FALSE : TRUE,pClass);` |
|       - | 2562 | `	}` |
|    2657 | 2563 | `	return GenStateCompileClassAttr(&(*pGen),sMods.iProtection,sMods.iFlags,pClass);` |
|    4098 | 2564 | `}` |
|       - | 2565 | `/*` |
|       - | 2566 | `` * Compile a PHP 8.4 property-hook list `{ get ...; set ...; }` following a`` |
|       - | 2567 | ` * property declaration. Each hook body is synthesized into a hidden public` |
|       - | 2568 | ` * class method (__phl_hook_get_NAME / __phl_hook_set_NAME) so inheritance,` |
|       - | 2569 | ` * $this binding, and dispatch ride the ordinary method machinery; OP_MEMBER /` |
|       - | 2570 | ` * OP_STORE route reads and plain writes through them (a per-instance guard` |
|       - | 2571 | ` * makes $this->NAME inside a hook body address the raw backing slot — php's` |
|       - | 2572 | `` * rule that hooks see the backing store). `get => expr;` compiles as an`` |
|       - | 2573 | `` * implicit return (the arrow-fn pattern); `set => expr;` compiles the same`` |
|       - | 2574 | ` * and is flagged VM_FUNC_HOOK_SET_EXPR — the dispatcher assigns its return` |
|       - | 2575 | `` * value to the backing slot. A `set` without a parameter list receives the`` |
|       - | 2576 | `` * implicit `$value` formal.`` |
|       - | 2577 | ` * On entry pGen->pIn sits on '{'; on success it sits just past '}'.` |
|       - | 2578 | ` */` |
|       - | 2579 | `/*` |
|       - | 2580 | `` * Whether any token in [pStart, pEnd) spells `$this->NAME` (this property's own`` |
|       - | 2581 | `` * name; `?->` and `::` member ops count too). php 8.4's virtual-vs-backed rule:`` |
|       - | 2582 | ` * a hooked property is BACKED iff any of its OWN hook bodies references it by` |
|       - | 2583 | ` * name through $this — otherwise it is VIRTUAL: no backing store, no default` |
|       - | 2584 | ` * allowed, excluded from the raw object surfaces.` |
|       - | 2585 | ` */` |
|     186 | 2586 | `static int GenStateHookBodyRefsProp(SyToken *pStart,SyToken *pEnd,const SyString *pName)` |
|       5 | 2587 | `{` |
|       - | 2588 | `	SyToken *p;` |
|     835 | 2589 | `	for( p = pStart ; p + 1 < pEnd ; p++ ){` |
|     715 | 2590 | `		if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|     583 | 2591 | `			continue;` |
|       - | 2592 | `		}` |
|       - | 2593 | ``		/* `$this->NAME` (also `?->`/`::`) */`` |
|     132 | 2594 | `		if( p + 3 < pEnd` |
|     132 | 2595 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|     132 | 2596 | `		 && p[1].sData.nByte == sizeof("this")-1` |
|     115 | 2597 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)"this",sizeof("this")-1) == 0` |
|      98 | 2598 | `		 && GenStateTokenIsMemberOp(&p[2])` |
|      98 | 2599 | `		 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|      98 | 2600 | `		 && p[3].sData.nByte == pName->nByte` |
|      91 | 2601 | `		 && SyMemcmp((const void *)p[3].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|      66 | 2602 | `			return 1;` |
|       - | 2603 | `		}` |
|       - | 2604 | ``		/* `parent::$NAME` (the parent::$x::get() hook-call form): the parent`` |
|       - | 2605 | `		 * hook operates on the shared per-instance backing store, so the` |
|       - | 2606 | `		 * property is backed (php compiles a default alongside it). */` |
|      70 | 2607 | `		if( p > pStart` |
|      66 | 2608 | `		 && GenStateTokenIsMemberOp(&p[-1])` |
|      33 | 2609 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       4 | 2610 | `		 && p[1].sData.nByte == pName->nByte` |
|       8 | 2611 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|       6 | 2612 | `			return 1;` |
|       - | 2613 | `		}` |
|      36 | 2614 | `	}` |
|     125 | 2615 | `	return 0;` |
|      98 | 2616 | `}` |
|       - | 2617 | `/*` |
|       - | 2618 | ` * True when p opens php 8.4's parent-hook call form` |
|       - | 2619 | `` * `parent :: $ NAME :: get\|set (` (7 tokens through the '(').`` |
|       - | 2620 | ` */` |
|    1452 | 2621 | `static int GenStateIsParentHookCallAt(SyToken *p,SyToken *pEnd)` |
|       5 | 2622 | `{` |
|    1678 | 2623 | `	return p + 6 < pEnd` |
|     947 | 2624 | `	 && (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|     317 | 2625 | `	 && p->sData.nByte == sizeof("parent")-1` |
|     110 | 2626 | `	 && SyMemcmp((const void *)p->sData.zString,(const void *)"parent",sizeof("parent")-1) == 0` |
|      20 | 2627 | `	 && GenStateTokenIsMemberOp(&p[1])` |
|      12 | 2628 | `	 && (p[2].nType & PH7_TK_DOLLAR) != 0` |
|      12 | 2629 | `	 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|      12 | 2630 | `	 && GenStateTokenIsMemberOp(&p[4])` |
|      12 | 2631 | `	 && (p[5].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|      12 | 2632 | `	 && p[5].sData.nByte == 3` |
|      12 | 2633 | `	 && (SyMemcmp((const void *)p[5].sData.zString,(const void *)"get",3) == 0` |
|       8 | 2634 | `	  \|\| SyMemcmp((const void *)p[5].sData.zString,(const void *)"set",3) == 0)` |
|    1673 | 2635 | `	 && (p[6].nType & PH7_TK_LPAREN) != 0;` |
|       5 | 2636 | `}` |
|       - | 2637 | `/*` |
|       - | 2638 | `` * Rewrite php 8.4 `parent::$x::get(...)` / `parent::$x::set(...)` calls in a`` |
|       - | 2639 | ` * hook body into calls of the parent class's synthesized hook method` |
|       - | 2640 | `` * (`parent::__phl_hook_get_x(...)`). Builds a token COPY into pCopy (only`` |
|       - | 2641 | ` * called when GenStateIsParentHookCallAt matched somewhere in the range);` |
|       - | 2642 | ` * copied tokens keep pointing at source-owned lexeme storage, and the` |
|       - | 2643 | ` * synthesized method-name lexemes are VM-allocator owned. Returns SXRET_OK` |
|       - | 2644 | ` * or SXERR_MEM.` |
|       - | 2645 | ` */` |
|       6 | 2646 | `static sxi32 GenStateRewriteParentHookCalls(ph7_gen_state *pGen,SySet *pCopy,` |
|       - | 2647 | `	SyToken *pStart,SyToken *pEnd)` |
|       2 | 2648 | `{` |
|       8 | 2649 | `	SyToken *p = pStart;` |
|      56 | 2650 | `	while( p < pEnd ){` |
|      50 | 2651 | `		if( GenStateIsParentHookCallAt(p,pEnd) ){` |
|       - | 2652 | `			SyToken sTok;` |
|       - | 2653 | `			char zName[384];` |
|       - | 2654 | `			sxu32 nName;` |
|       - | 2655 | `			char *zDup;` |
|       - | 2656 | ``			/* `parent` `::` */`` |
|       8 | 2657 | `			SySetPut(pCopy,(const void *)&p[0]);` |
|       8 | 2658 | `			SySetPut(pCopy,(const void *)&p[1]);` |
|      11 | 2659 | `			nName = SyBufferFormat(zName,sizeof(zName),"__phl_hook_%.3s_%.*s",` |
|       6 | 2660 | `				p[5].sData.zString,(int)p[3].sData.nByte,p[3].sData.zString);` |
|       8 | 2661 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|       8 | 2662 | `			if( zDup == 0 ){` |
|     ! 0 | 2663 | `				return SXERR_MEM;` |
|       - | 2664 | `			}` |
|       8 | 2665 | `			sTok = p[3]; /* keep the line info of the property name */` |
|       8 | 2666 | `			sTok.nType = PH7_TK_ID;` |
|       8 | 2667 | `			SyStringInitFromBuf(&sTok.sData,zDup,nName);` |
|       8 | 2668 | `			sTok.pUserData = 0;` |
|       8 | 2669 | `			SySetPut(pCopy,(const void *)&sTok);` |
|       8 | 2670 | `			p += 6; /* continue at the '(' — arguments copy through unchanged */` |
|       8 | 2671 | `			continue;` |
|       - | 2672 | `		}` |
|      44 | 2673 | `		SySetPut(pCopy,(const void *)p);` |
|      44 | 2674 | `		p++;` |
|       2 | 2675 | `	}` |
|       8 | 2676 | `	return SXRET_OK;` |
|       5 | 2677 | `}` |
|       - | 2678 | `/*` |
|       - | 2679 | `` * A `get` hook's return type IS the property's declared type — php never lets a`` |
|       - | 2680 | ` * hook declare one, so there is nothing else it could be, and that is what makes` |
|       - | 2681 | `` * `public int $p { get { return "5"; } }` answer int(5) and a `get` returning`` |
|       - | 2682 | `` * "x" raise `C::$p::get(): Return value must be of type int, string returned`.`` |
|       - | 2683 | ` * Installing it on the synthesized method reuses the return enforcement that` |
|       - | 2684 | ` * already matches php byte for byte (the same move the __toString implicit` |
|       - | 2685 | `` * `string` type made), and lets the compile-time bare-`return;` check see the`` |
|       - | 2686 | ` * hook as the typed function php treats it as.` |
|       - | 2687 | ` *` |
|       - | 2688 | ` * The union alternatives are SHARED, not copied: their class-name SyStrings are` |
|       - | 2689 | ` * VM-allocator owned and outlive both records, which is the same contract` |
|       - | 2690 | ` * GenStateCopyTypeToAttr relies on.` |
|       - | 2691 | ` */` |
|     146 | 2692 | `static void GenStateHookGetReturnType(ph7_vm_func *pFunc,ph7_class_attr *pAttr)` |
|       5 | 2693 | `{` |
|     151 | 2694 | `	if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      20 | 2695 | `		return; /* untyped property: the hook is untyped too */` |
|       - | 2696 | `	}` |
|     135 | 2697 | `	pFunc->nReturnType = pAttr->nType;` |
|     135 | 2698 | `	pFunc->sReturnClass = pAttr->sClass;` |
|     135 | 2699 | `	pFunc->sReturnTypeName = pAttr->sTypeName;` |
|     135 | 2700 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|      14 | 2701 | `		pFunc->iFlags \|= VM_FUNC_RETURN_NULLABLE;` |
|       6 | 2702 | `	}` |
|     135 | 2703 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       - | 2704 | `		sxu32 i;` |
|     ! 0 | 2705 | `		for( i = 0 ; i < SySetUsed(&pAttr->aUnionAlts) ; i++ ){` |
|     ! 0 | 2706 | `			SySetPut(&pFunc->aReturnUnion,SySetAt(&pAttr->aUnionAlts,i));` |
|     ! 0 | 2707 | `		}` |
|     ! 0 | 2708 | `	}` |
|      78 | 2709 | `}` |
|       - | 2710 | `/*` |
|       - | 2711 | `` * The mirror for a `set` hook. php gives it two implicit pieces of signature:`` |
|       - | 2712 | `` * the implicit `$value` formal carries the PROPERTY's declared type (so`` |
|       - | 2713 | `` * `public int $p { set { ... } }` coerces `$o->p = "7"` to int(7) and rejects`` |
|       - | 2714 | `` * "abc" with `C::$p::set(): Argument #1 ($value) must be of type int, string`` |
|       - | 2715 | `` * given`), and the hook itself returns `void` — a set hook that returns a value`` |
|       - | 2716 | `` * is php's `A void method must not return a value`, on an untyped property too.`` |
|       - | 2717 | `` * An EXPLICIT `set(T $v)` keeps its own declared type; only the implicit formal`` |
|       - | 2718 | ` * is typed from the property, which is why the caller passes pValueArg only` |
|       - | 2719 | ` * when it synthesized one.` |
|       - | 2720 | ` */` |
|      96 | 2721 | `static void GenStateHookSetSignature(ph7_gen_state *pGen,ph7_vm_func *pFunc,` |
|       - | 2722 | `	ph7_class_attr *pAttr,ph7_vm_func_arg *pValueArg)` |
|       4 | 2723 | `{` |
|       - | 2724 | `	char *zVoid;` |
|     100 | 2725 | `	if( pValueArg && (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|      69 | 2726 | `		pValueArg->nType = pAttr->nType;` |
|      69 | 2727 | `		pValueArg->sClass = pAttr->sClass;` |
|      69 | 2728 | `		pValueArg->sTypeName = pAttr->sTypeName;` |
|      69 | 2729 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|      14 | 2730 | `			pValueArg->iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|       6 | 2731 | `		}` |
|      69 | 2732 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       - | 2733 | `			sxu32 i;` |
|       3 | 2734 | `			pValueArg->iFlags \|= VM_FUNC_ARG_UNION;` |
|       7 | 2735 | `			for( i = 0 ; i < SySetUsed(&pAttr->aUnionAlts) ; i++ ){` |
|       5 | 2736 | `				SySetPut(&pValueArg->aUnionAlts,SySetAt(&pAttr->aUnionAlts,i));` |
|       3 | 2737 | `			}` |
|       1 | 2738 | `		}` |
|      33 | 2739 | `	}` |
|     100 | 2740 | `	pFunc->nReturnType = MEMOBJ_VOID;` |
|     100 | 2741 | `	zVoid = SyMemBackendStrDup(&pGen->pVm->sAllocator,"void",sizeof("void")-1);` |
|     100 | 2742 | `	if( zVoid ){` |
|     100 | 2743 | `		SyStringInitFromBuf(&pFunc->sReturnTypeName,zVoid,sizeof("void")-1);` |
|      48 | 2744 | `	}` |
|     100 | 2745 | `}` |
|     178 | 2746 | `PH7_PRIVATE sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr)` |
|       5 | 2747 | `{` |
|     183 | 2748 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 2749 | `	sxi32 rc;` |
|     183 | 2750 | `	int bRefsSelf = 0;` |
|     183 | 2751 | `	pGen->pIn++; /* Jump '{' */` |
|     441 | 2752 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|       - | 2753 | `		char zHook[384];` |
|       - | 2754 | `		SyString sHookName;` |
|       - | 2755 | `		ph7_class_method *pMeth;` |
|       - | 2756 | `		int bGet;` |
|     263 | 2757 | `		sxu32 nHLine = pGen->pIn->nLine;` |
|     263 | 2758 | `		if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|      18 | 2759 | `			pGen->pIn++; /* stray ';' between hooks */` |
|      26 | 2760 | `			continue;` |
|       - | 2761 | `		}` |
|     247 | 2762 | `		if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - | 2763 | `			/* by-reference get hook: not modeled (loud, recorded) */` |
|     ! 0 | 2764 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|       - | 2765 | `				"By-reference property hooks are not supported for %z::$%z",` |
|     ! 0 | 2766 | `				&pClass->sName,&pAttr->sName);` |
|     ! 0 | 2767 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2768 | `				return SXERR_ABORT;` |
|       - | 2769 | `			}` |
|     ! 0 | 2770 | `			return SXERR_CORRUPT;` |
|       - | 2771 | `		}` |
|     247 | 2772 | `		if( (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 2773 | `			goto HookSyntax;` |
|       - | 2774 | `		}` |
|     242 | 2775 | `		if( pGen->pIn->sData.nByte == 3` |
|     247 | 2776 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"get",3) == 0 ){` |
|     151 | 2777 | `			bGet = 1;` |
|     174 | 2778 | `		}else if( pGen->pIn->sData.nByte == 3` |
|     100 | 2779 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"set",3) == 0 ){` |
|     100 | 2780 | `			bGet = 0;` |
|      52 | 2781 | `		}else{` |
|     ! 0 | 2782 | `			goto HookSyntax;` |
|       - | 2783 | `		}` |
|     247 | 2784 | `		pGen->pIn++; /* Jump 'get'/'set' */` |
|     247 | 2785 | `		sHookName.zString = zHook;` |
|     368 | 2786 | `		sHookName.nByte = SyBufferFormat(zHook,sizeof(zHook),"__phl_hook_%s_%z",` |
|     121 | 2787 | `			bGet ? "get" : "set",&pAttr->sName);` |
|     247 | 2788 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_CCB)) ){` |
|       - | 2789 | ``			/* Bare `get;` / `set;` — an ABSTRACT hook declaration (php 8.4):`` |
|       - | 2790 | ``			 * legal only on an `abstract` property or inside an interface. The`` |
|       - | 2791 | `			 * synthesized method carries PH7_CLASS_ATTR_ABSTRACT and rides the` |
|       - | 2792 | `			 * existing must-implement machinery; a concrete hook override (or a` |
|       - | 2793 | `			 * plain property, see GenStateCheckAbstractMethods) satisfies it. */` |
|      16 | 2794 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0` |
|      10 | 2795 | `			 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 2796 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|       - | 2797 | `					"Non-abstract property hook must have a body");` |
|     ! 0 | 2798 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 2799 | `					return SXERR_ABORT;` |
|       - | 2800 | `				}` |
|     ! 0 | 2801 | `				return SXERR_CORRUPT;` |
|       - | 2802 | `			}` |
|      18 | 2803 | `			pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|       - | 2804 | `				PH7_CLASS_PROT_PUBLIC,PH7_CLASS_ATTR_ABSTRACT,0);` |
|      18 | 2805 | `			if( pMeth == 0 ){` |
|     ! 0 | 2806 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2807 | `				return SXERR_ABORT;` |
|       - | 2808 | `			}` |
|      18 | 2809 | `			pMeth->sFunc.nLine = nHLine;` |
|      18 | 2810 | `			if( bGet ){` |
|      12 | 2811 | `				GenStateHookGetReturnType(&pMeth->sFunc,pAttr);` |
|       5 | 2812 | `			}` |
|      18 | 2813 | `			if( !bGet ){` |
|       - | 2814 | ``				/* The implicit `$value` formal keeps the stub's signature`` |
|       - | 2815 | `				 * compatible with concrete set-hook implementations (which` |
|       - | 2816 | `				 * always carry one parameter). It takes the PROPERTY's declared` |
|       - | 2817 | `				 * type (php: the abstract set's parameter type IS the property` |
|       - | 2818 | `				 * type), so the override contravariance check accepts a typed` |
|       - | 2819 | ``				 * `set(int $v)` implementation on an `int $x` requirement. */`` |
|       - | 2820 | `				ph7_vm_func_arg sVArg;` |
|       7 | 2821 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|       7 | 2822 | `				if( zVName == 0 ){` |
|     ! 0 | 2823 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2824 | `					return SXERR_ABORT;` |
|       - | 2825 | `				}` |
|       7 | 2826 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|       7 | 2827 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|       7 | 2828 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       7 | 2829 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       7 | 2830 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|       7 | 2831 | `				GenStateHookSetSignature(&(*pGen),&pMeth->sFunc,pAttr,&sVArg);` |
|       7 | 2832 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|       3 | 2833 | `			}` |
|      18 | 2834 | `			rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|      18 | 2835 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 2836 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2837 | `				return SXERR_ABORT;` |
|       - | 2838 | `			}` |
|      18 | 2839 | `			pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|      18 | 2840 | `			continue; /* the loop consumes the ';' as a stray separator */` |
|       - | 2841 | `		}` |
|     226 | 2842 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0` |
|     231 | 2843 | `		 \|\| (pClass->iFlags & PH7_CLASS_INTERFACE) != 0 ){` |
|       - | 2844 | `			/* php: an abstract/interface property hook cannot carry a body */` |
|     ! 0 | 2845 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|       - | 2846 | `				"Abstract property hook cannot have body");` |
|     ! 0 | 2847 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2848 | `				return SXERR_ABORT;` |
|       - | 2849 | `			}` |
|     ! 0 | 2850 | `			return SXERR_CORRUPT;` |
|       - | 2851 | `		}` |
|     231 | 2852 | `		pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|       - | 2853 | `			PH7_CLASS_PROT_PUBLIC,0,0);` |
|     231 | 2854 | `		if( pMeth == 0 ){` |
|     ! 0 | 2855 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2856 | `			return SXERR_ABORT;` |
|       - | 2857 | `		}` |
|     231 | 2858 | `		pMeth->sFunc.nLine = nHLine;` |
|     231 | 2859 | `		if( bGet ){` |
|     141 | 2860 | `			GenStateHookGetReturnType(&pMeth->sFunc,pAttr);` |
|      68 | 2861 | `		}` |
|     231 | 2862 | `		if( !bGet ){` |
|       - | 2863 | ``			/* Parameter list: explicit `set(Type $v)` or the implicit `$value` */`` |
|      94 | 2864 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|      26 | 2865 | `				SyToken *pRp = 0;` |
|      26 | 2866 | `				pGen->pIn++;` |
|      26 | 2867 | `				PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pRp);` |
|      26 | 2868 | `				if( pRp >= pGen->pEnd ){` |
|     ! 0 | 2869 | `					goto HookSyntax;` |
|       - | 2870 | `				}` |
|      26 | 2871 | `				if( pGen->pIn < pRp ){` |
|      26 | 2872 | `					rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pRp,0,0);` |
|      26 | 2873 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 2874 | `						return SXERR_ABORT;` |
|       - | 2875 | `					}` |
|      12 | 2876 | `				}` |
|      26 | 2877 | `				pGen->pIn = &pRp[1];` |
|      12 | 2878 | `			}` |
|      94 | 2879 | `			if( SySetUsed(&pMeth->sFunc.aArgs) < 1 ){` |
|       - | 2880 | `				/* Implicit $value formal */` |
|       - | 2881 | `				ph7_vm_func_arg sVArg;` |
|      70 | 2882 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|      70 | 2883 | `				if( zVName == 0 ){` |
|     ! 0 | 2884 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2885 | `					return SXERR_ABORT;` |
|       - | 2886 | `				}` |
|      70 | 2887 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|      70 | 2888 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|      70 | 2889 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      70 | 2890 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|      70 | 2891 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|      70 | 2892 | `				SyStringInitFromBuf(&sVArg.sTypeName,0,0);` |
|      70 | 2893 | `				GenStateHookSetSignature(&(*pGen),&pMeth->sFunc,pAttr,&sVArg);` |
|      70 | 2894 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|      37 | 2895 | `			}else{` |
|       - | 2896 | ``				/* An EXPLICIT `set(T $v)` keeps its own parameter type; only the`` |
|       - | 2897 | `				 * void return is implicit. */` |
|      26 | 2898 | `				GenStateHookSetSignature(&(*pGen),&pMeth->sFunc,pAttr,0);` |
|       - | 2899 | `			}` |
|      45 | 2900 | `		}` |
|     295 | 2901 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - | 2902 | `			/* Block body */` |
|     133 | 2903 | `			SyToken *pBodyStart = pGen->pIn;` |
|     133 | 2904 | `			SyToken *pCloser = 0;` |
|     133 | 2905 | `			int bParentCall = 0;` |
|     133 | 2906 | `			PH7_DelimitNestedTokens(&pBodyStart[1],pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pCloser);` |
|     133 | 2907 | `			if( pCloser < pGen->pEnd ){` |
|       - | 2908 | `				SyToken *pScan;` |
|    1143 | 2909 | `				for( pScan = &pBodyStart[1] ; pScan < pCloser ; pScan++ ){` |
|    1019 | 2910 | `					if( GenStateIsParentHookCallAt(pScan,pCloser) ){` |
|       6 | 2911 | `						bParentCall = 1;` |
|       6 | 2912 | `						break;` |
|       - | 2913 | `					}` |
|     510 | 2914 | `				}` |
|      64 | 2915 | `			}` |
|     133 | 2916 | `			if( bParentCall ){` |
|       - | 2917 | ``				/* `parent::$x::get()` inside the body: compile a REWRITTEN copy`` |
|       - | 2918 | `				 * of the body tokens (the call becomes the parent's synthesized` |
|       - | 2919 | `				 * hook method), then continue past the original body. */` |
|       - | 2920 | `				SySet sBody;` |
|       6 | 2921 | `				SyToken *pSavedEnd = pGen->pEnd;` |
|       6 | 2922 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       6 | 2923 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,&pCloser[1]);` |
|       6 | 2924 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 2925 | `					SySetRelease(&sBody);` |
|     ! 0 | 2926 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2927 | `					return SXERR_ABORT;` |
|       - | 2928 | `				}` |
|       6 | 2929 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|       6 | 2930 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|       6 | 2931 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|       6 | 2932 | `				pGen->pIn = &pCloser[1];` |
|       6 | 2933 | `				pGen->pEnd = pSavedEnd;` |
|       6 | 2934 | `				SySetRelease(&sBody);` |
|       6 | 2935 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 2936 | `					return SXERR_ABORT;` |
|       - | 2937 | `				}` |
|       6 | 2938 | `				pMeth->sFunc.nEndLine = pCloser->nLine;` |
|       4 | 2939 | `			}else{` |
|     129 | 2940 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|     129 | 2941 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 2942 | `					return SXERR_ABORT;` |
|       - | 2943 | `				}` |
|     129 | 2944 | `				pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|       - | 2945 | `			}` |
|     133 | 2946 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|      25 | 2947 | `				bRefsSelf = 1;` |
|      16 | 2948 | `			}` |
|     216 | 2949 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ARRAY_OP) ){` |
|       - | 2950 | ``			/* `=> expr;` — implicit-return body (the arrow-fn pattern) */`` |
|       - | 2951 | `			GenBlock *pBlock;` |
|       - | 2952 | `			SySet *pInstrContainer;` |
|       - | 2953 | `			SyToken *pBodyStart;` |
|       - | 2954 | `			SyToken *pExprEnd;` |
|     103 | 2955 | `			SyToken *pSavedEnd = 0;` |
|       - | 2956 | `			SySet sBody;` |
|     103 | 2957 | `			int bParentCall = 0;` |
|     103 | 2958 | `			pGen->pIn++; /* Jump '=>' */` |
|     103 | 2959 | `			pBodyStart = pGen->pIn;` |
|       - | 2960 | `			/* Delimit the expression (first top-level ';', or a closer that` |
|       - | 2961 | `			 * would end the enclosing hook list) and rewrite any` |
|       - | 2962 | ``			 * `parent::$x::get()` calls into the parent's synthesized hook`` |
|       - | 2963 | `			 * method on a token copy. */` |
|       - | 2964 | `			{` |
|     103 | 2965 | `				sxi32 iNest = 0;` |
|     103 | 2966 | `				pExprEnd = pBodyStart;` |
|     511 | 2967 | `				while( pExprEnd < pGen->pEnd ){` |
|     511 | 2968 | `					if( pExprEnd->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      19 | 2969 | `						iNest++;` |
|     503 | 2970 | `					}else if( pExprEnd->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      19 | 2971 | `						if( iNest <= 0 ){` |
|     ! 0 | 2972 | `							break;` |
|       - | 2973 | `						}` |
|      19 | 2974 | `						iNest--;` |
|     487 | 2975 | `					}else if( iNest <= 0 && (pExprEnd->nType & PH7_TK_SEMI) ){` |
|     103 | 2976 | `						break;` |
|       - | 2977 | `					}` |
|     413 | 2978 | `					pExprEnd++;` |
|       5 | 2979 | `				}` |
|       - | 2980 | `			}` |
|       - | 2981 | `			{` |
|       - | 2982 | `				SyToken *pScan;` |
|     491 | 2983 | `				for( pScan = pBodyStart ; pScan < pExprEnd ; pScan++ ){` |
|     395 | 2984 | `					if( GenStateIsParentHookCallAt(pScan,pExprEnd) ){` |
|       3 | 2985 | `						bParentCall = 1;` |
|       3 | 2986 | `						break;` |
|       - | 2987 | `					}` |
|     199 | 2988 | `				}` |
|       - | 2989 | `			}` |
|     103 | 2990 | `			if( bParentCall ){` |
|       3 | 2991 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       3 | 2992 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,pExprEnd);` |
|       3 | 2993 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 2994 | `					SySetRelease(&sBody);` |
|     ! 0 | 2995 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2996 | `					return SXERR_ABORT;` |
|       - | 2997 | `				}` |
|       3 | 2998 | `				pSavedEnd = pGen->pEnd;` |
|       3 | 2999 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|       3 | 3000 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|       1 | 3001 | `			}` |
|     152 | 3002 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      98 | 3003 | `				PH7_VmInstrLength(pGen->pVm),&pMeth->sFunc,&pBlock);` |
|     103 | 3004 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 3005 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"PH7 engine is running out-of-memory");` |
|     ! 0 | 3006 | `				return SXERR_ABORT;` |
|       - | 3007 | `			}` |
|     103 | 3008 | `			pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     103 | 3009 | `			PH7_VmSetByteCodeContainer(pGen->pVm,&pMeth->sFunc.aByteCode);` |
|     103 | 3010 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|     103 | 3011 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     103 | 3012 | `			GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     103 | 3013 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     103 | 3014 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     103 | 3015 | `			GenStateLeaveBlock(&(*pGen),0);` |
|     103 | 3016 | `			if( bParentCall ){` |
|       3 | 3017 | `				pGen->pIn = pExprEnd; /* land on the original ';' */` |
|       3 | 3018 | `				pGen->pEnd = pSavedEnd;` |
|       3 | 3019 | `				SySetRelease(&sBody);` |
|       1 | 3020 | `			}` |
|     103 | 3021 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3022 | `				return SXERR_ABORT;` |
|       - | 3023 | `			}` |
|     103 | 3024 | `			pMeth->sFunc.nEndLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nHLine;` |
|     103 | 3025 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|      47 | 3026 | `				bRefsSelf = 1;` |
|      22 | 3027 | `			}` |
|     103 | 3028 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     103 | 3029 | `				pGen->pIn++; /* Jump ';' */` |
|      49 | 3030 | `			}` |
|     103 | 3031 | `			if( !bGet ){` |
|       - | 3032 | ``				/* `set => expr` assigns the expression to the backing store:`` |
|       - | 3033 | `				 * the dispatcher consumes the implicit return value — which` |
|       - | 3034 | `				 * also makes the property BACKED (php: the shorthand is sugar` |
|       - | 3035 | ``				 * for `$this->NAME = expr`). */`` |
|       8 | 3036 | `				pMeth->sFunc.iFlags \|= VM_FUNC_HOOK_SET_EXPR;` |
|       8 | 3037 | `				bRefsSelf = 1;` |
|       3 | 3038 | `			}` |
|      54 | 3039 | `		}else{` |
|     ! 0 | 3040 | `			goto HookSyntax;` |
|       - | 3041 | `		}` |
|     231 | 3042 | `		rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|     231 | 3043 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3044 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 3045 | `			return SXERR_ABORT;` |
|       - | 3046 | `		}` |
|     231 | 3047 | `		pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|       5 | 3048 | `	}` |
|     183 | 3049 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|     ! 0 | 3050 | `		goto HookSyntax;` |
|       - | 3051 | `	}` |
|     183 | 3052 | `	pGen->pIn++; /* Jump '}' */` |
|     183 | 3053 | `	if( !bRefsSelf ){` |
|       - | 3054 | ``		/* php 8.4 virtual-vs-backed: no hook body referenced `$this->NAME`, so`` |
|       - | 3055 | `		 * this property is VIRTUAL — php gives it no backing store and forbids` |
|       - | 3056 | `		 * a default value (compile fatal, php's exact wording). */` |
|     115 | 3057 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_HOOK_VIRTUAL;` |
|     115 | 3058 | `		if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|     ! 0 | 3059 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3060 | `				"Cannot specify default value for virtual hooked property %z::$%z",` |
|     ! 0 | 3061 | `				&pClass->sName,&pAttr->sName);` |
|     ! 0 | 3062 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3063 | `				return SXERR_ABORT;` |
|       - | 3064 | `			}` |
|     ! 0 | 3065 | `			return SXERR_CORRUPT;` |
|       - | 3066 | `		}` |
|      55 | 3067 | `	}` |
|     183 | 3068 | `	return SXRET_OK;` |
|     ! 0 | 3069 | `HookSyntax:` |
|     ! 0 | 3070 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3071 | `		"Invalid property hook declaration for %z::$%z: expecting 'get' or 'set'",` |
|     ! 0 | 3072 | `		&pClass->sName,&pAttr->sName);` |
|     ! 0 | 3073 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 3074 | `		return SXERR_ABORT;` |
|       - | 3075 | `	}` |
|     ! 0 | 3076 | `	return SXERR_CORRUPT;` |
|      94 | 3077 | `}` |
|       - | 3078 | `/* php's #[\Override] verification, defined with the rest of the class-link` |
|       - | 3079 | ` * checks below; both compilers (class and interface) drive the same pair. */` |
|       - | 3080 | `static sxi32 GenStateCollectOverrides(ph7_gen_state *pGen,ph7_class *pClass,` |
|       - | 3081 | `	SySet *pMeths,SySet *pProps);` |
|       - | 3082 | `static sxi32 GenStateCheckOverrides(ph7_gen_state *pGen,ph7_class *pClass,` |
|       - | 3083 | `	SySet *pMeths,SySet *pProps);` |
|       - | 3084 | `/*` |
|       - | 3085 | ` * Compile an object interface.` |
|       - | 3086 | ` *  According to the PHP language reference manual` |
|       - | 3087 | ` *   Object Interfaces:` |
|       - | 3088 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - | 3089 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - | 3090 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - | 3091 | ` *   class, but without any of the methods having their contents defined.` |
|       - | 3092 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - | 3093 | ` */` |
|     332 | 3094 | `PH7_PRIVATE sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       5 | 3095 | `{` |
|     337 | 3096 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 3097 | `	ph7_class *pClass,*pBase;` |
|     337 | 3098 | `	ph7_class *pSavedCurClass = pGen->pCurClass; /* restored at 'done' */` |
|     337 | 3099 | `	ph7_class *pSavedCurBase = pGen->pCurBase;   /* ...and its base, see pCurBase */` |
|       - | 3100 | `	SyToken *pEnd,*pTmp;` |
|       - | 3101 | `	SyString *pName;` |
|       - | 3102 | `	SySet aOvMeth,aOvProp;   /* the #[\Override] claims this interface DECLARED */` |
|       - | 3103 | ``	SySet aExtraParents;     /* `extends A, S, T`: every parent after the first */`` |
|     337 | 3104 | `	sxu32 nErrEntry = pGen->nErr; /* errors already reported when this one started */` |
|       - | 3105 | `	sxi32 nKwrd;` |
|       - | 3106 | `	sxi32 rc;` |
|       - | 3107 | `	{` |
|       - | 3108 | `		/* Deferral gate: parent interfaces may need an autoloader` |
|       - | 3109 | `		 * that has not run yet. */` |
|       - | 3110 | `		sxi32 rcDefer;` |
|     337 | 3111 | `		if( GenStateMaybeDeferClass(pGen,0,PH7_DEFER_KIND_INTERFACE,&rcDefer) ){` |
|      15 | 3112 | `			return rcDefer == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - | 3113 | `		}` |
|       - | 3114 | `	}` |
|       - | 3115 | `	/* Jump the 'interface' keyword */` |
|     325 | 3116 | `	pGen->pIn++;` |
|       - | 3117 | `	/* Extract interface name */` |
|     325 | 3118 | `	pName = &pGen->pIn->sData;` |
|       - | 3119 | `	/* Advance the stream cursor */` |
|     325 | 3120 | `	pGen->pIn++;` |
|       - | 3121 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 3122 | `		SyBlob sFQN;` |
|       - | 3123 | `		SyString sFQNStr;` |
|     325 | 3124 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     325 | 3125 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|     325 | 3126 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|       - | 3127 | ``		/* php refuses a declaration whose short name a local `use` already took. */`` |
|     325 | 3128 | `		if( GenStateGuardImportRedeclare(pGen,0,pName,&sFQNStr,nLine) == SXERR_ABORT ){` |
|     ! 0 | 3129 | `			SyBlobRelease(&sFQN);` |
|     ! 0 | 3130 | `			return SXERR_ABORT;` |
|       - | 3131 | `		}` |
|     325 | 3132 | `		GenStateRecordDeclaredName(pGen,0,&sFQNStr);` |
|     325 | 3133 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|     325 | 3134 | `		SyBlobRelease(&sFQN);` |
|       - | 3135 | `	}` |
|     325 | 3136 | `	if( pClass == 0 ){` |
|     ! 0 | 3137 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 3138 | `		return SXERR_ABORT;` |
|       - | 3139 | `	}` |
|     325 | 3140 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|     325 | 3141 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|     ! 0 | 3142 | `		return SXERR_ABORT;` |
|       - | 3143 | `	}` |
|       - | 3144 | `	/* Mark as an interface (PH7_NewRawClass may have set INTERNAL) */` |
|     325 | 3145 | `	pClass->iFlags \|= PH7_CLASS_INTERFACE;` |
|     480 | 3146 | `	if( GenStateCheckAttrPlacement(&(*pGen),&pClass->aAttrs,1,1,` |
|     485 | 3147 | `			&pClass->sName,pClass->iFlags) == SXERR_ABORT ){` |
|     ! 0 | 3148 | `		return SXERR_ABORT;` |
|       - | 3149 | `	}` |
|       - | 3150 | `	/* Assume no base class is given */` |
|     325 | 3151 | `	pBase = 0;` |
|     325 | 3152 | `	SySetInit(&aExtraParents,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|     325 | 3153 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      54 | 3154 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      54 | 3155 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a, c, … */ ){` |
|       - | 3156 | `			/* php lets an interface extend SEVERAL parent interfaces. The first` |
|       - | 3157 | `			 * becomes pBase (single-inheritance chain, hDerived); every extra one` |
|       - | 3158 | `			 * is recorded via PH7_ClassImplement so it lands in aInterface — the` |
|       - | 3159 | `			 * runtime subtype walk (VmInterfaceReaches) follows both. */` |
|      54 | 3160 | `			pGen->pIn++;` |
|      33 | 3161 | `			for(;;){` |
|       - | 3162 | `				SyBlob sResolved;` |
|       - | 3163 | `				SyString sBaseName;` |
|       - | 3164 | `				sxu32 nRefLine;` |
|       - | 3165 | `				ph7_class *pParent;` |
|      70 | 3166 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|      70 | 3167 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      70 | 3168 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 | 3169 | `					SyBlobRelease(&sResolved);` |
|     ! 0 | 3170 | `					rc = PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|       - | 3171 | `						"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 | 3172 | `						pName);` |
|     ! 0 | 3173 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 3174 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 3175 | `						return SXERR_ABORT;` |
|       - | 3176 | `					}` |
|     ! 0 | 3177 | `					return SXRET_OK;` |
|       - | 3178 | `				}` |
|     136 | 3179 | `				pParent = PH7_VmExtractClass(pGen->pVm,` |
|      66 | 3180 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      70 | 3181 | `				SyStringInitFromBuf(&sBaseName,` |
|       - | 3182 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - | 3183 | `				/* Only interfaces is allowed */` |
|      70 | 3184 | `				while( pParent && (pParent->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 3185 | `					pParent = pParent->pNextName;` |
|     ! 0 | 3186 | `				}` |
|      70 | 3187 | `				if( pParent == 0 ){` |
|       2 | 3188 | `					if( !GenStateLintSkipsBase(pGen,pClass) ){` |
|     ! 0 | 3189 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - | 3190 | `							"Nonexistent base interface '%z'",&sBaseName);` |
|     ! 0 | 3191 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 3192 | `							SyBlobRelease(&sResolved);` |
|     ! 0 | 3193 | `							return SXERR_ABORT;` |
|       - | 3194 | `						}` |
|     ! 0 | 3195 | `					}` |
|      69 | 3196 | `				}else if( pBase == 0 ){` |
|       - | 3197 | `					/* First parent → single-inheritance base */` |
|      52 | 3198 | `					pBase = pParent;` |
|      28 | 3199 | `				}else{` |
|       - | 3200 | `					/* Additional parent → COLLECTED, and applied after the body like` |
|       - | 3201 | `					 * the first one is. Copying its members here put them in hMethod` |
|       - | 3202 | `					 * and hConst before the body was read, so the interface's own` |
|       - | 3203 | ``					 * `public function g();` collided with the very name it was`` |
|       - | 3204 | ``					 * restating: `interface B extends A, S` could redeclare A's`` |
|       - | 3205 | `					 * members and not S's ("Cannot redeclare B::g()"), which is a` |
|       - | 3206 | `					 * declaration php accepts and php-di writes. */` |
|      19 | 3207 | `					SySetPut(&aExtraParents,(const void *)&pParent);` |
|       - | 3208 | `				}` |
|      70 | 3209 | `				SyBlobRelease(&sResolved);` |
|       - | 3210 | `				/* Continue on a comma-separated list */` |
|      70 | 3211 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|      19 | 3212 | `					pGen->pIn++;` |
|      19 | 3213 | `					continue;` |
|       - | 3214 | `				}` |
|      54 | 3215 | `				break;` |
|     ! 0 | 3216 | `			}` |
|      25 | 3217 | `		}` |
|      25 | 3218 | `	}` |
|     325 | 3219 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 3220 | `		/* Syntax error */` |
|     ! 0 | 3221 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 | 3222 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 3223 | `		if( rc == SXERR_ABORT ){` |
|       - | 3224 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 3225 | `			return SXERR_ABORT;` |
|       - | 3226 | `		}` |
|     ! 0 | 3227 | `		return SXRET_OK;` |
|       - | 3228 | `	}` |
|     325 | 3229 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|     325 | 3230 | `	pEnd = 0; /* cc warning */` |
|       - | 3231 | `	/* Delimit the interface body */` |
|     325 | 3232 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|     325 | 3233 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 3234 | `		/* Syntax error */` |
|     ! 0 | 3235 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 | 3236 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 3237 | `		if( rc == SXERR_ABORT ){` |
|       - | 3238 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 3239 | `			return SXERR_ABORT;` |
|       - | 3240 | `		}` |
|     ! 0 | 3241 | `		return SXRET_OK;` |
|       - | 3242 | `	}` |
|       - | 3243 | `	/* The delimiter token is the interface body's closing brace */` |
|     325 | 3244 | `	pClass->nEndLine = pEnd->nLine;` |
|       - | 3245 | `	/* Swap token stream */` |
|     325 | 3246 | `	pTmp = pGen->pEnd;` |
|     325 | 3247 | `	pGen->pEnd = pEnd;` |
|       - | 3248 | `	/* This interface is now the lexical class for its body (see pCurClass) — a` |
|       - | 3249 | `	 * const default here is not a trait, so __TRAIT__ stays "". */` |
|     325 | 3250 | `	pGen->pCurClass = pClass;` |
|     325 | 3251 | ``	pGen->pCurBase = 0; /* php gives an interface no `parent`, however many it extends */`` |
|       - | 3252 | `	/* Start the parse process` |
|       - | 3253 | `	 * Note (According to the PHP reference manual):` |
|       - | 3254 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - | 3255 | `	 *  Only 'public' visibility is allowed.` |
|       - | 3256 | `	 */` |
|     250 | 3257 | `	for(;;){` |
|       - | 3258 | `		/* Jump leading/trailing semi-colons */` |
|     681 | 3259 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|     181 | 3260 | `			pGen->pIn++;` |
|       5 | 3261 | `		}` |
|     505 | 3262 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 3263 | `			/* End of interface body */` |
|     307 | 3264 | `			break;` |
|       - | 3265 | `		}` |
|       - | 3266 | `		/* Bind a directly-preceding docblock to this member */` |
|     203 | 3267 | `		GenStateSetPendingDoc(&(*pGen));` |
|     198 | 3268 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     104 | 3269 | `			&& !GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 | 3270 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|       - | 3271 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 | 3272 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 3273 | `			if( rc == SXERR_ABORT ){` |
|       - | 3274 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 3275 | `				return SXERR_ABORT;` |
|       - | 3276 | `			}` |
|     ! 0 | 3277 | `			goto done;` |
|       - | 3278 | `		}` |
|       - | 3279 | `		/* The member, through the shared modifier run -- which is where an` |
|       - | 3280 | ``		 * interface's own rules live now (public-only, no `final`, no written`` |
|       - | 3281 | ``		 * `abstract`, and a property that may only be a hooked requirement). */`` |
|     203 | 3282 | `		rc = GenStateCompileMember(&(*pGen),pClass,"interface");` |
|     203 | 3283 | `		if( rc != SXRET_OK ){` |
|      22 | 3284 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3285 | `				return SXERR_ABORT;` |
|       - | 3286 | `			}` |
|      22 | 3287 | `			goto done;` |
|       - | 3288 | `		}` |
|       5 | 3289 | `	}` |
|       - | 3290 | `	/* An interface method may claim #[\Override] too, against the interfaces this` |
|       - | 3291 | `	 * one extends -- collected before the inherit copies theirs in. */` |
|     307 | 3292 | `	GenStateCollectOverrides(&(*pGen),pClass,&aOvMeth,&aOvProp);` |
|       - | 3293 | `	/* Reject a php-fatal redeclaration before hoisting the interface. An` |
|       - | 3294 | `	 * interface that EXTENDS another is not early-bound, exactly as a class` |
|       - | 3295 | `	 * that implements one is not. */` |
|     556 | 3296 | `	if( GenStateGuardClassRedeclaration(pGen,pClass,` |
|     283 | 3297 | `			pBase == 0 && SySetUsed(&aExtraParents) == 0) == SXERR_ABORT ){` |
|       3 | 3298 | `		SySetRelease(&aOvMeth);` |
|       3 | 3299 | `		SySetRelease(&aOvProp);` |
|       3 | 3300 | `		return SXERR_ABORT;` |
|       - | 3301 | `	}` |
|       - | 3302 | `	/* Every method this interface DECLARES, judged against the same name in each` |
|       - | 3303 | `	 * parent -- hMethod holds only its own declarations until the inherits below` |
|       - | 3304 | `	 * run, so this is the one moment the two sets are separable. php makes the` |
|       - | 3305 | `	 * check for a restated method whichever parent it came from; PHL made it for` |
|       - | 3306 | ``	 * none of them, so `interface B extends A { public function f(): int; }` over`` |
|       - | 3307 | ``	 * `A::f(): string` compiled in silence. */`` |
|     305 | 3308 | `	if( pGen->nErr == nErrEntry ){` |
|     299 | 3309 | `		if( pBase && PH7_ClassInterfaceCheckRedeclare(&(*pGen),pClass,pBase) == SXERR_ABORT ){` |
|     ! 0 | 3310 | `			SySetRelease(&aOvMeth);` |
|     ! 0 | 3311 | `			SySetRelease(&aOvProp);` |
|     ! 0 | 3312 | `			SySetRelease(&aExtraParents);` |
|     ! 0 | 3313 | `			return SXERR_ABORT;` |
|       - | 3314 | `		}` |
|       - | 3315 | `		{` |
|     299 | 3316 | `			ph7_class **apExtra = (ph7_class **)SySetBasePtr(&aExtraParents);` |
|       - | 3317 | `			sxu32 nExtra;` |
|     315 | 3318 | `			for( nExtra = 0 ; nExtra < SySetUsed(&aExtraParents) ; ++nExtra ){` |
|      16 | 3319 | `				if( PH7_ClassInterfaceCheckRedeclare(&(*pGen),pClass,apExtra[nExtra])` |
|      11 | 3320 | `					== SXERR_ABORT ){` |
|     ! 0 | 3321 | `					SySetRelease(&aOvMeth);` |
|     ! 0 | 3322 | `					SySetRelease(&aOvProp);` |
|     ! 0 | 3323 | `					SySetRelease(&aExtraParents);` |
|     ! 0 | 3324 | `					return SXERR_ABORT;` |
|       - | 3325 | `				}` |
|      11 | 3326 | `			}` |
|       - | 3327 | `		}` |
|     147 | 3328 | `	}` |
|       - | 3329 | `	/* Install the interface */` |
|     305 | 3330 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|     305 | 3331 | `	if( rc == SXRET_OK && pBase ){` |
|       - | 3332 | `		/* Inherit from the base interface */` |
|      52 | 3333 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|      24 | 3334 | `	}` |
|     305 | 3335 | `	if( rc == SXRET_OK ){` |
|       - | 3336 | `		/* ...and from every parent after the first, whose members are copied only` |
|       - | 3337 | `		 * where this interface declared none of its own. */` |
|     305 | 3338 | `		ph7_class **apExtra = (ph7_class **)SySetBasePtr(&aExtraParents);` |
|       - | 3339 | `		sxu32 nExtra;` |
|     321 | 3340 | `		for( nExtra = 0 ; rc == SXRET_OK && nExtra < SySetUsed(&aExtraParents) ; ++nExtra ){` |
|      19 | 3341 | `			rc = PH7_ClassImplement(pClass,apExtra[nExtra]);` |
|      11 | 3342 | `		}` |
|     150 | 3343 | `	}` |
|     305 | 3344 | `	SySetRelease(&aExtraParents);` |
|     300 | 3345 | `	if( rc == SXRET_OK && pGen->nErr == nErrEntry` |
|     301 | 3346 | `	 && GenStateCheckOverrides(&(*pGen),pClass,&aOvMeth,&aOvProp) == SXERR_ABORT ){` |
|     ! 0 | 3347 | `		SySetRelease(&aOvMeth);` |
|     ! 0 | 3348 | `		SySetRelease(&aOvProp);` |
|     ! 0 | 3349 | `		return SXERR_ABORT;` |
|       - | 3350 | `	}` |
|     305 | 3351 | `	SySetRelease(&aOvMeth);` |
|     305 | 3352 | `	SySetRelease(&aOvProp);` |
|     305 | 3353 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3354 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 3355 | `		return SXERR_ABORT;` |
|       - | 3356 | `	}` |
|     150 | 3357 | `done:` |
|     323 | 3358 | `	SySetRelease(&aExtraParents);` |
|     323 | 3359 | `	pGen->pCurClass = pSavedCurClass;` |
|     323 | 3360 | `	pGen->pCurBase = pSavedCurBase;` |
|       - | 3361 | `	/* Point beyond the interface body */` |
|     323 | 3362 | `	pGen->pIn  = &pEnd[1];` |
|     323 | 3363 | `	pGen->pEnd = pTmp;` |
|     323 | 3364 | `	return PH7_OK;` |
|     171 | 3365 | `}` |
|       - | 3366 | `/*` |
|       - | 3367 | ` * Compile a user-defined class.` |
|       - | 3368 | ` * According to the PHP language reference manual` |
|       - | 3369 | ` *  class` |
|       - | 3370 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - | 3371 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - | 3372 | ` *  of the properties and methods belonging to the class.` |
|       - | 3373 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - | 3374 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - | 3375 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - | 3376 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 3377 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - | 3378 | ` *  (called "methods").` |
|       - | 3379 | ` */` |
|       - | 3380 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - | 3381 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - | 3382 | `struct TraitUseEntry {` |
|       - | 3383 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - | 3384 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - | 3385 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - | 3386 | `};` |
|       - | 3387 | `/*` |
|       - | 3388 | ` * Validate that methods implementing interface contracts have compatible` |
|       - | 3389 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - | 3390 | ` */` |
|    4904 | 3391 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 | 3392 | `{` |
|       - | 3393 | `	ph7_class **apIface;` |
|       - | 3394 | `	sxu32 nIface,i;` |
|       - | 3395 | `	sxi32 rc;` |
|    4909 | 3396 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 | 3397 | `		return SXRET_OK;` |
|       - | 3398 | `	}` |
|    4909 | 3399 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    4909 | 3400 | `	nIface = SySetUsed(&pClass->aInterface);` |
|    5835 | 3401 | `	for(i = 0; i < nIface; i++){` |
|     931 | 3402 | `		ph7_class *pIface = apIface[i];` |
|       - | 3403 | `		SyHashEntry *pEntry;` |
|     931 | 3404 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|    2373 | 3405 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|    1447 | 3406 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - | 3407 | `			ph7_class_method *pImplMeth;` |
|    1447 | 3408 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - | 3409 | `			/* Find the implementing method in the class */` |
|    1447 | 3410 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|    1447 | 3411 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      37 | 3412 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - | 3413 | `			}` |
|       - | 3414 | `			/* Check visibility: interface methods must be implemented as public */` |
|    1415 | 3415 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 | 3416 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - | 3417 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 | 3418 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 | 3419 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 3420 | `					return SXERR_ABORT;` |
|       - | 3421 | `				}` |
|       1 | 3422 | `			}` |
|       - | 3423 | `			/* Signature compatibility. php checks an implementation against the` |
|       - | 3424 | `			 * interface's declaration with the very rule it checks an override by --` |
|       - | 3425 | `			 * one shared body, so the variance half is not the class hierarchy's` |
|       - | 3426 | `			 * alone and the fatal reads the same. This site used to count PARAMETERS` |
|       - | 3427 | ``			 * only, and word the two declarations as bare `$name` lists.`` |
|       - | 3428 | `			 *` |
|       - | 3429 | `			 * A CONSTRUCTOR is not exempt here: php exempts an INHERITED one from` |
|       - | 3430 | ``			 * variance, but an interface that declares `__construct` constrains every`` |
|       - | 3431 | `			 * implementor's. */` |
|    1410 | 3432 | `			if( PH7_ClassCheckOverrideCompat(&(*pGen),pIface,pClass,pIfaceMeth,pImplMeth,0)` |
|     710 | 3433 | `				== SXERR_ABORT ){` |
|     ! 0 | 3434 | `				return SXERR_ABORT;` |
|       - | 3435 | `			}` |
|       5 | 3436 | `		}` |
|     468 | 3437 | `	}` |
|    4909 | 3438 | `	return SXRET_OK;` |
|    2457 | 3439 | `}` |
|       - | 3440 | `/*` |
|       - | 3441 | ` * An abstract property-hook stub (__phl_hook_{get,set}_NAME) is satisfied by` |
|       - | 3442 | ` * the class declaring a PLAIN (non-abstract, non-hooked) property NAME: php` |
|       - | 3443 | `` * lets a plain property implement `{ get; set; }` requirements — its raw`` |
|       - | 3444 | ` * read/write IS the default get/set. A concrete hook override replaced the` |
|       - | 3445 | ` * stub in hMethod already, so a surviving stub next to a HOOKED property` |
|       - | 3446 | ` * means that specific hook is still missing.` |
|       - | 3447 | ` */` |
|      50 | 3448 | `static int GenStateAbstractHookSatisfied(ph7_class *pClass,const SyString *pMName)` |
|       5 | 3449 | `{` |
|       - | 3450 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|       - | 3451 | `	ph7_class_attr *pProp;` |
|      50 | 3452 | `	if( pMName->nByte <= nPfx` |
|      33 | 3453 | `	 \|\| (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) != 0` |
|       4 | 3454 | `	  && SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) != 0) ){` |
|      48 | 3455 | `		return 0; /* not a hook stub */` |
|       - | 3456 | `	}` |
|       7 | 3457 | `	pProp = PH7_ClassExtractAttribute(pClass,&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|       7 | 3458 | `	return pProp != 0` |
|       6 | 3459 | `		&& (pProp->iFlags & (PH7_CLASS_ATTR_ABSTRACT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|       3 | 3460 | `			\|PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) == 0;` |
|      30 | 3461 | `}` |
|       - | 3462 | `/*` |
|       - | 3463 | ` * Append an abstract member's display name to the message blob, translating a` |
|       - | 3464 | `` * property-hook stub (__phl_hook_get_x) to php's `$x::get` form.`` |
|       - | 3465 | ` */` |
|      22 | 3466 | `static void GenStateAppendAbstractMemberName(SyBlob *pMsg,const SyString *pMName)` |
|       4 | 3467 | `{` |
|       - | 3468 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|      22 | 3469 | `	if( pMName->nByte > nPfx` |
|      15 | 3470 | `	 && (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) == 0` |
|     ! 0 | 3471 | `	  \|\| SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) == 0) ){` |
|     ! 0 | 3472 | `		SyBlobAppend(pMsg,"$",1);` |
|     ! 0 | 3473 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|     ! 0 | 3474 | `		SyBlobAppend(pMsg,"::",2);` |
|     ! 0 | 3475 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[sizeof("__phl_hook_")-1],3);` |
|     ! 0 | 3476 | `		return;` |
|       - | 3477 | `	}` |
|      26 | 3478 | `	SyBlobAppend(pMsg,(const void *)pMName->zString,pMName->nByte);` |
|      15 | 3479 | `}` |
|       - | 3480 | `/*` |
|       - | 3481 | ` * ---------------------------------------------------------------------------` |
|       - | 3482 | `` * php's `#[\Override]` (8.3, widened to properties in 8.5).`` |
|       - | 3483 | ` *` |
|       - | 3484 | ` * The attribute is a CLAIM the engine checks where the member is written: the` |
|       - | 3485 | ` * name must already exist above, so a typo, a renamed parent method or a base` |
|       - | 3486 | ` * class that dropped one is a fatal at the declaration instead of a method` |
|       - | 3487 | ` * nobody ever calls. php runs it at class LINK time, after inheritance, and its` |
|       - | 3488 | ` * rules are the inheritance rules rather than a name search:` |
|       - | 3489 | ` *` |
|       - | 3490 | ` *   - a PRIVATE parent member is not inherited, so it is not something to` |
|       - | 3491 | ` *     override;` |
|       - | 3492 | ` *   - the CONSTRUCTOR is exempt from php's inheritance signature check unless it` |
|       - | 3493 | ` *     is abstract (or an interface's), and #[\Override] follows that exemption —` |
|       - | 3494 | `` *     a concrete parent `__construct` does NOT satisfy the claim while an`` |
|       - | 3495 | ` *     abstract one does;` |
|       - | 3496 | ` *   - a method matches CASE-INSENSITIVELY and a property case-SENSITIVELY, which` |
|       - | 3497 | ` *     is php's rule for the two namespaces everywhere else;` |
|       - | 3498 | ` *   - an interface counts for a method, at any depth and through any ancestor;` |
|       - | 3499 | ` *   - a TRAIT used by this very class does not: its method is the using class's` |
|       - | 3500 | ` *     own, and php reports the USING class's name when the claim fails.` |
|       - | 3501 | ` * ---------------------------------------------------------------------------` |
|       - | 3502 | ` */` |
|       - | 3503 | `#define GEN_OVERRIDE_ATTR "Override"` |
|       - | 3504 | ``/* Does this member carry `#[\Override]`? The compiler resolves an attribute name`` |
|       - | 3505 | ` * to its fully-qualified spelling and class names are case-insensitive, so one` |
|       - | 3506 | ` * case-folded compare against the whole set is the test. */` |
|    8148 | 3507 | `static int GenStateHasOverrideAttr(SySet *pAttrs)` |
|       5 | 3508 | `{` |
|    8153 | 3509 | `	ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|       - | 3510 | `	sxu32 n;` |
|    8285 | 3511 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; ++n ){` |
|     212 | 3512 | `		if( aAttr[n].sName.nByte == sizeof(GEN_OVERRIDE_ATTR)-1` |
|     151 | 3513 | `		 && SyStrnicmp(aAttr[n].sName.zString,GEN_OVERRIDE_ATTR,` |
|      40 | 3514 | `			sizeof(GEN_OVERRIDE_ATTR)-1) == 0 ){` |
|      82 | 3515 | `			return 1;` |
|       - | 3516 | `		}` |
|      71 | 3517 | `	}` |
|    8073 | 3518 | `	return 0;` |
|    4079 | 3519 | `}` |
|       - | 3520 | `/* Does an interface reachable from pClass -- its own, or any ancestor's --` |
|       - | 3521 | ` * declare this method? An interface that extends others already carries their` |
|       - | 3522 | ` * stubs in its own table, so one level of lookup per interface is enough. */` |
|      30 | 3523 | `static int GenStateIfaceDeclaresMethod(ph7_class *pClass,const SyString *pName)` |
|       1 | 3524 | `{` |
|      67 | 3525 | `	for( ; pClass ; pClass = pClass->pBase ){` |
|      45 | 3526 | `		ph7_class **apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|       - | 3527 | `		sxu32 n;` |
|      47 | 3528 | `		for( n = 0 ; n < SySetUsed(&pClass->aInterface) ; ++n ){` |
|      10 | 3529 | `			if( apIface[n]` |
|      11 | 3530 | `			 && PH7_ClassExtractMethod(apIface[n],pName->zString,pName->nByte) ){` |
|       9 | 3531 | `				return 1;` |
|       - | 3532 | `			}` |
|       2 | 3533 | `		}` |
|      19 | 3534 | `	}` |
|      23 | 3535 | `	return 0;` |
|      16 | 3536 | `}` |
|       - | 3537 | `/* Is there a parent METHOD this one may claim to override? */` |
|      50 | 3538 | `static int GenStateOverridesMethod(ph7_class *pClass,const SyString *pName)` |
|       1 | 3539 | `{` |
|      79 | 3540 | `	int bCtor = pName->nByte == sizeof("__construct")-1` |
|      50 | 3541 | `		&& SyStrnicmp(pName->zString,"__construct",sizeof("__construct")-1) == 0;` |
|       - | 3542 | `	ph7_class *pWalk;` |
|      65 | 3543 | `	for( pWalk = pClass->pBase ; pWalk ; pWalk = pWalk->pBase ){` |
|      35 | 3544 | `		ph7_class_method *pMeth = PH7_ClassExtractMethod(pWalk,pName->zString,pName->nByte);` |
|      35 | 3545 | `		if( pMeth == 0 \|\| pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|      11 | 3546 | `			continue;` |
|       - | 3547 | `		}` |
|      25 | 3548 | `		if( bCtor && (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       5 | 3549 | `			continue;   /* php exempts a concrete parent constructor */` |
|       - | 3550 | `		}` |
|      21 | 3551 | `		return 1;` |
|     ! 0 | 3552 | `	}` |
|      31 | 3553 | `	return GenStateIfaceDeclaresMethod(pClass,pName);` |
|      26 | 3554 | `}` |
|       - | 3555 | `/* Is there a parent PROPERTY this one may claim to override? Interfaces declare` |
|       - | 3556 | ` * none, so this is the base chain alone. */` |
|      22 | 3557 | `static int GenStateOverridesProp(ph7_class *pClass,const SyString *pName)` |
|       2 | 3558 | `{` |
|       - | 3559 | `	ph7_class *pWalk;` |
|      32 | 3560 | `	for( pWalk = pClass->pBase ; pWalk ; pWalk = pWalk->pBase ){` |
|      22 | 3561 | `		ph7_class_attr *pAttr = PH7_ClassExtractAttribute(pWalk,pName->zString,pName->nByte);` |
|      22 | 3562 | `		if( pAttr && pAttr->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
|      14 | 3563 | `			return 1;` |
|       - | 3564 | `		}` |
|       5 | 3565 | `	}` |
|      11 | 3566 | `	return 0;` |
|      13 | 3567 | `}` |
|       - | 3568 | `/*` |
|       - | 3569 | `` * Verify every `#[\Override]` the class DECLARED, in php's order: the methods`` |
|       - | 3570 | ` * first and then the properties, each in declaration order, and the first` |
|       - | 3571 | ` * failure is the whole diagnostic (it is a fatal).` |
|       - | 3572 | ` *` |
|       - | 3573 | ` * The two sets are collected BEFORE inheritance runs -- an inherited method` |
|       - | 3574 | ` * keeps the parent's attribute record and php does not re-check it there -- and` |
|       - | 3575 | ` * verified after, which is when the answer exists.` |
|       - | 3576 | ` */` |
|    5218 | 3577 | `static sxi32 GenStateCollectOverrides(ph7_gen_state *pGen,ph7_class *pClass,` |
|       - | 3578 | `	SySet *pMeths,SySet *pProps)` |
|       5 | 3579 | `{` |
|       - | 3580 | `	static const sxu32 nHookPfx = sizeof("__phl_hook_get_")-1;` |
|       - | 3581 | `	SyHashEntry *pEntry;` |
|    5223 | 3582 | `	SySetInit(pMeths,&pGen->pVm->sAllocator,sizeof(ph7_class_method *));` |
|    5223 | 3583 | `	SySetInit(pProps,&pGen->pVm->sAllocator,sizeof(ph7_class_attr *));` |
|    5223 | 3584 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   10625 | 3585 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|    5407 | 3586 | `		ph7_class_method *pMeth = (ph7_class_method *)pEntry->pUserData;` |
|    5407 | 3587 | `		SyString *pName = &pMeth->sFunc.sName;` |
|       - | 3588 | `		/* A property hook is compiled to a method here and is a PROPERTY in php,` |
|       - | 3589 | `		 * so the property arm below owns its claim. */` |
|    5402 | 3590 | `		if( pName->nByte > nHookPfx` |
|    2850 | 3591 | `		 && SyMemcmp((const void *)pName->zString,(const void *)"__phl_hook_",` |
|     144 | 3592 | `			sizeof("__phl_hook_")-1) == 0 ){` |
|     247 | 3593 | `			continue;` |
|       - | 3594 | `		}` |
|    5165 | 3595 | `		if( GenStateHasOverrideAttr(&pMeth->sFunc.aAttrs) ){` |
|      57 | 3596 | `			SySetPut(pMeths,(const void *)&pMeth);` |
|      28 | 3597 | `		}` |
|       5 | 3598 | `	}` |
|    5223 | 3599 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|    8211 | 3600 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|    2993 | 3601 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|    2993 | 3602 | `		if( GenStateHasOverrideAttr(&pAttr->aAttrs) ){` |
|      26 | 3603 | `			SySetPut(pProps,(const void *)&pAttr);` |
|      12 | 3604 | `		}` |
|       5 | 3605 | `	}` |
|    5223 | 3606 | `	return SXRET_OK;` |
|       5 | 3607 | `}` |
|    4958 | 3608 | `static sxi32 GenStateCheckOverrides(ph7_gen_state *pGen,ph7_class *pClass,` |
|       - | 3609 | `	SySet *pMeths,SySet *pProps)` |
|       5 | 3610 | `{` |
|    4963 | 3611 | `	ph7_class_method **apMeth = (ph7_class_method **)SySetBasePtr(pMeths);` |
|    4963 | 3612 | `	ph7_class_attr **apProp = (ph7_class_attr **)SySetBasePtr(pProps);` |
|       - | 3613 | `	sxu32 n;` |
|    4963 | 3614 | `	if( pClass->iFlags & PH7_CLASS_LINT_UNBOUND ){` |
|       - | 3615 | `		/* A base this lint could not see may well DECLARE the member; php reports` |
|       - | 3616 | `		 * an #[\Override] mismatch only where it early-binds, and it early-binds` |
|       - | 3617 | `		 * nothing it cannot link. */` |
|       8 | 3618 | `		return SXRET_OK;` |
|       - | 3619 | `	}` |
|       - | 3620 | `	/* hMethod is a LIFO iteration list (SyHashInsert), so the collected order is` |
|       - | 3621 | `	 * the REVERSE of the declaration order; hAttr is a FIFO one` |
|       - | 3622 | `	 * (SyHashInsertTail) and needs no such turn. php reports the first member it` |
|       - | 3623 | `	 * finds in declaration order and stops. */` |
|    4983 | 3624 | `	for( n = SySetUsed(pMeths) ; n > 0 ; --n ){` |
|      51 | 3625 | `		ph7_class_method *pMeth = apMeth[n - 1];` |
|      51 | 3626 | `		if( !GenStateOverridesMethod(pClass,&pMeth->sFunc.sName) ){` |
|      34 | 3627 | `			return PH7_GenCompileError(&(*pGen),E_ERROR,pMeth->sFunc.nLine,` |
|       - | 3628 | `				"%z::%z() has #[\\Override] attribute, but no matching parent method exists",` |
|      11 | 3629 | `				&pClass->sName,&pMeth->sFunc.sName);` |
|       - | 3630 | `		}` |
|      15 | 3631 | `	}` |
|    4945 | 3632 | `	for( n = 0 ; n < SySetUsed(pProps) ; ++n ){` |
|      24 | 3633 | `		if( !GenStateOverridesProp(pClass,&apProp[n]->sName) ){` |
|       - | 3634 | `			/* php reports the CLASS's line for a property, not the property's. */` |
|      16 | 3635 | `			return PH7_GenCompileError(&(*pGen),E_ERROR,pClass->nLine,` |
|       - | 3636 | `				"%z::$%z has #[\\Override] attribute, but no matching parent property exists",` |
|      10 | 3637 | `				&pClass->sName,&apProp[n]->sName);` |
|       - | 3638 | `		}` |
|       8 | 3639 | `	}` |
|    4923 | 3640 | `	return SXRET_OK;` |
|    2484 | 3641 | `}` |
|       - | 3642 | `/*` |
|       - | 3643 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - | 3644 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - | 3645 | ` */` |
|       - | 3646 | `/*` |
|       - | 3647 | ` * The interface FURTHEST up that declares this method name. php attributes an` |
|       - | 3648 | ` * unimplemented method to the interface that first ASKED for it, and an` |
|       - | 3649 | ` * interface reaches its parents through two containers: pBase (the first name` |
|       - | 3650 | `` * after `extends`) and aInterface (every one after that). Following only pBase`` |
|       - | 3651 | `` * stopped at the restating interface, so `interface B extends A, S` reported`` |
|       - | 3652 | `` * `B::g` where php reports `S::g`.`` |
|       - | 3653 | ` *` |
|       - | 3654 | ` * Depth-bounded like the Throwable walk beside it: an interface graph cannot` |
|       - | 3655 | ` * cycle (every parent is already compiled), and the bound costs nothing.` |
|       - | 3656 | ` */` |
|      34 | 3657 | `static ph7_class * GenStateIfaceDeclaringAt(ph7_class *pIface,const SyString *pMName,int iDepth)` |
|       4 | 3658 | `{` |
|      38 | 3659 | `	ph7_class *pDeepest = 0;` |
|       - | 3660 | `	ph7_class **apUp;` |
|       - | 3661 | `	sxu32 i;` |
|      38 | 3662 | `	if( pIface == 0 \|\| iDepth > 32 ){` |
|      20 | 3663 | `		return 0;` |
|       - | 3664 | `	}` |
|      22 | 3665 | `	if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      20 | 3666 | `		pDeepest = pIface;` |
|       8 | 3667 | `	}` |
|       - | 3668 | `	{` |
|      22 | 3669 | `		ph7_class *pUp = GenStateIfaceDeclaringAt(pIface->pBase,pMName,iDepth + 1);` |
|      22 | 3670 | `		if( pUp ){` |
|     ! 0 | 3671 | `			pDeepest = pUp;` |
|     ! 0 | 3672 | `		}` |
|       - | 3673 | `	}` |
|      22 | 3674 | `	apUp = (ph7_class **)SySetBasePtr(&pIface->aInterface);` |
|      24 | 3675 | `	for( i = 0 ; i < SySetUsed(&pIface->aInterface) ; ++i ){` |
|       3 | 3676 | `		ph7_class *pUp = GenStateIfaceDeclaringAt(apUp[i],pMName,iDepth + 1);` |
|       3 | 3677 | `		if( pUp ){` |
|       3 | 3678 | `			pDeepest = pUp;` |
|       1 | 3679 | `		}` |
|       2 | 3680 | `	}` |
|      22 | 3681 | `	return pDeepest;` |
|      21 | 3682 | `}` |
|    4904 | 3683 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 | 3684 | `{` |
|       - | 3685 | `	ph7_class_method *pMeth;` |
|       - | 3686 | `	SyHashEntry *pEntry;` |
|       - | 3687 | `	sxu32 nAbstract;` |
|       - | 3688 | `	SyBlob sMsg;` |
|       - | 3689 | `	sxi32 rc;` |
|       - | 3690 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|    4909 | 3691 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     119 | 3692 | `		return SXRET_OK;` |
|       - | 3693 | `	}` |
|    4795 | 3694 | `	if( pClass->iFlags & PH7_CLASS_LINT_UNBOUND ){` |
|       - | 3695 | `		/* A trait this lint could not see is exactly where the implementations` |
|       - | 3696 | ``		 * usually are (`class C implements ArrayAccess { use HasDataTrait; }`),`` |
|       - | 3697 | `		 * so the count would be of methods the class does have. */` |
|      18 | 3698 | `		return SXRET_OK;` |
|       - | 3699 | `	}` |
|       - | 3700 | `	/* Count abstract methods */` |
|    4777 | 3701 | `	nAbstract = 0;` |
|    4777 | 3702 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   20937 | 3703 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   13779 | 3704 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   13779 | 3705 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      33 | 3706 | `			if( GenStateAbstractHookSatisfied(pClass,&pMeth->sFunc.sName) ){` |
|       7 | 3707 | `				continue; /* hook requirement met by a plain property (php) */` |
|       - | 3708 | `			}` |
|      26 | 3709 | `			nAbstract++;` |
|      11 | 3710 | `		}` |
|       5 | 3711 | `	}` |
|    4777 | 3712 | `	if( nAbstract == 0 ){` |
|    4757 | 3713 | `		return SXRET_OK;` |
|       - | 3714 | `	}` |
|       - | 3715 | `	/* Build the error message listing all abstract methods with origins */` |
|      24 | 3716 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      24 | 3717 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - | 3718 | `		"be declared abstract or implement the remaining method%s (",` |
|      10 | 3719 | `		&pClass->sName,nAbstract,` |
|      10 | 3720 | `		(nAbstract > 1 ? "s" : ""),` |
|      10 | 3721 | `		(nAbstract > 1 ? "s" : ""));` |
|       - | 3722 | `	/* Second pass: list methods with origins */` |
|       - | 3723 | `	{` |
|      24 | 3724 | `		sxu32 nListed = 0;` |
|      24 | 3725 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      52 | 3726 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      32 | 3727 | `			ph7_class *pOrigin = 0;` |
|       - | 3728 | `			SyString *pMName;` |
|      32 | 3729 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      32 | 3730 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       8 | 3731 | `				continue;` |
|       - | 3732 | `			}` |
|      26 | 3733 | `			pMName = &pMeth->sFunc.sName;` |
|      26 | 3734 | `			if( GenStateAbstractHookSatisfied(pClass,pMName) ){` |
|     ! 0 | 3735 | `				continue; /* hook requirement met by a plain property (php) */` |
|       - | 3736 | `			}` |
|      26 | 3737 | `			if( nListed > 0 ){` |
|       3 | 3738 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 | 3739 | `			}` |
|       - | 3740 | `			/* Find the origin of this abstract method.` |
|       - | 3741 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - | 3742 | `			 * inheritance chains) take precedence for interface-declared` |
|       - | 3743 | `			 * methods. Abstract class methods only win when the class` |
|       - | 3744 | `			 * itself declared the abstract method (not inherited from` |
|       - | 3745 | `			 * an interface). Trait methods are adopted into the using` |
|       - | 3746 | `			 * class's namespace.` |
|       - | 3747 | `			 */` |
|       - | 3748 | `			{` |
|       - | 3749 | `				ph7_class **apIface;` |
|       - | 3750 | `				ph7_class **apTrait;` |
|       - | 3751 | `				ph7_class *pWalk;` |
|       - | 3752 | `				sxu32 i;` |
|       - | 3753 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - | 3754 | `				 * (one that was written in the class body, not inherited from an` |
|       - | 3755 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - | 3756 | `				 */` |
|      26 | 3757 | `				if( pClass->pBase ){` |
|      14 | 3758 | `					pWalk = pClass->pBase;` |
|      22 | 3759 | `					while( pWalk ){` |
|       - | 3760 | `						ph7_class_method *pParentMeth;` |
|      16 | 3761 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      16 | 3762 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - | 3763 | `							/* Exclude methods that came from an interface anywhere` |
|       - | 3764 | `							 * in this class's ancestor chain.` |
|       - | 3765 | `							 */` |
|      16 | 3766 | `							int fromIface = 0;` |
|      16 | 3767 | `							ph7_class *pAnc = pWalk;` |
|      24 | 3768 | `							while( pAnc ){` |
|       - | 3769 | `								ph7_class **apPI;` |
|       - | 3770 | `								sxu32 j;` |
|      18 | 3771 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      18 | 3772 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|      10 | 3773 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|      10 | 3774 | `										fromIface = 1;` |
|      10 | 3775 | `										break;` |
|       - | 3776 | `									}` |
|     ! 0 | 3777 | `								}` |
|      18 | 3778 | `								if( fromIface ) break;` |
|      10 | 3779 | `								pAnc = pAnc->pBase;` |
|       2 | 3780 | `							}` |
|      16 | 3781 | `							if( !fromIface ){` |
|       7 | 3782 | `								pOrigin = pWalk;` |
|       7 | 3783 | `								break;` |
|       - | 3784 | `							}` |
|       4 | 3785 | `						}` |
|      10 | 3786 | `						pWalk = pWalk->pBase;` |
|       2 | 3787 | `					}` |
|       6 | 3788 | `				}` |
|       - | 3789 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - | 3790 | `				 * each interface's own parent chain for the deepest origin.` |
|       - | 3791 | `				 */` |
|      26 | 3792 | `				if( !pOrigin ){` |
|      20 | 3793 | `					pWalk = pClass;` |
|      44 | 3794 | `					while( pWalk && !pOrigin ){` |
|      28 | 3795 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      28 | 3796 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      18 | 3797 | `							ph7_class *pDeepest = GenStateIfaceDeclaringAt(apIface[i],pMName,0);` |
|      18 | 3798 | `							if( pDeepest ){` |
|      18 | 3799 | `								pOrigin = pDeepest;` |
|      18 | 3800 | `								break;` |
|       - | 3801 | `							}` |
|     ! 0 | 3802 | `						}` |
|      28 | 3803 | `						pWalk = pWalk->pBase;` |
|       4 | 3804 | `					}` |
|       8 | 3805 | `				}` |
|       - | 3806 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      26 | 3807 | `				if( !pOrigin ){` |
|       3 | 3808 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 | 3809 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 | 3810 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 | 3811 | `							pOrigin = pClass;` |
|       3 | 3812 | `							break;` |
|       - | 3813 | `						}` |
|     ! 0 | 3814 | `					}` |
|       1 | 3815 | `				}` |
|       - | 3816 | `			}` |
|      26 | 3817 | `			if( pOrigin ){` |
|      26 | 3818 | `				SyBlobFormat(&sMsg,"%z::",&pOrigin->sName);` |
|      15 | 3819 | `			}else{` |
|       - | 3820 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 | 3821 | `				SyBlobFormat(&sMsg,"%z::",&pClass->sName);` |
|       - | 3822 | `			}` |
|      26 | 3823 | `			GenStateAppendAbstractMemberName(&sMsg,pMName);` |
|      26 | 3824 | `			nListed++;` |
|       4 | 3825 | `		}` |
|       - | 3826 | `	}` |
|      24 | 3827 | `	SyBlobAppend(&sMsg,")",1);` |
|      34 | 3828 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      20 | 3829 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      24 | 3830 | `	SyBlobRelease(&sMsg);` |
|      24 | 3831 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 3832 | `		return SXERR_ABORT;` |
|       - | 3833 | `	}` |
|      24 | 3834 | `	return SXRET_OK;` |
|    2457 | 3835 | `}` |
|       - | 3836 | `/*` |
|       - | 3837 | ` * Parse a class/interface name reference from the current token stream.` |
|       - | 3838 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|       - | 3839 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|       - | 3840 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|       - | 3841 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|       - | 3842 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|       - | 3843 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|       - | 3844 | ` */` |
|    8668 | 3845 | `PH7_PRIVATE sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|       5 | 3846 | `{` |
|    8673 | 3847 | `	int isAbsolute = 0;` |
|    8673 | 3848 | `	SyToken *pStart = pGen->pIn;` |
|       - | 3849 | `	SyBlob sName;` |
|    8673 | 3850 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|    1035 | 3851 | `		isAbsolute = 1;` |
|    1035 | 3852 | `		pGen->pIn++;` |
|     513 | 3853 | `	}` |
|    8673 | 3854 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|       - | 3855 | ``	/* `namespace\X` names the CURRENT namespace and is fully qualified from there. */`` |
|    8673 | 3856 | `	if( !isAbsolute && GenStateNsRelPrefix(pGen,&pGen->pIn,pGen->pEnd,&sName) ){` |
|      17 | 3857 | `		isAbsolute = 1;` |
|       8 | 3858 | `	}` |
|    8673 | 3859 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      10 | 3860 | `		SyBlobRelease(&sName);` |
|      10 | 3861 | `		pGen->pIn = pStart;` |
|      10 | 3862 | `		return SXERR_INVALID;` |
|       - | 3863 | `	}` |
|    8665 | 3864 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|    8665 | 3865 | `	pGen->pIn++;` |
|   13243 | 3866 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|    4588 | 3867 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     171 | 3868 | `		SyBlobAppend(&sName,"\\",1);` |
|     171 | 3869 | `		pGen->pIn++;` |
|     171 | 3870 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|     171 | 3871 | `		pGen->pIn++;` |
|       5 | 3872 | `	}` |
|    8665 | 3873 | `	if( isAbsolute ){` |
|    1047 | 3874 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|     524 | 3875 | `	}else{` |
|       - | 3876 | `		SyString sRaw;` |
|    7623 | 3877 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|    7623 | 3878 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|       - | 3879 | `	}` |
|    8665 | 3880 | `	SyBlobRelease(&sName);` |
|    8665 | 3881 | `	return SXRET_OK;` |
|    4335 | 3882 | `}` |
|       - | 3883 | `/*` |
|       - | 3884 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|       - | 3885 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|       - | 3886 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|       - | 3887 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|       - | 3888 | ` * either direction cannot run unbounded.` |
|       - | 3889 | ` */` |
|       - | 3890 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|     454 | 3891 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|       5 | 3892 | `{` |
|       - | 3893 | `	ph7_class **apParent;` |
|       - | 3894 | `	sxu32 n;` |
|    1017 | 3895 | `	while( pInterface ){` |
|     573 | 3896 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|     ! 0 | 3897 | `			return FALSE;` |
|       - | 3898 | `		}` |
|     601 | 3899 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|      56 | 3900 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|      14 | 3901 | `			return TRUE;` |
|       - | 3902 | `		}` |
|     563 | 3903 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|     579 | 3904 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|      18 | 3905 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|     ! 0 | 3906 | `				return TRUE;` |
|       - | 3907 | `			}` |
|      10 | 3908 | `		}` |
|     563 | 3909 | `		pInterface = pInterface->pBase;` |
|     563 | 3910 | `		iDepth++;` |
|       5 | 3911 | `	}` |
|     449 | 3912 | `	return FALSE;` |
|     232 | 3913 | `}` |
|     438 | 3914 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|       5 | 3915 | `{` |
|     443 | 3916 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|       5 | 3917 | `}` |
|       - | 3918 | `/*` |
|       - | 3919 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|       - | 3920 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|       - | 3921 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|       - | 3922 | ` */` |
|      10 | 3923 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|       4 | 3924 | `{` |
|      18 | 3925 | `	while( pBase ){` |
|      10 | 3926 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|       2 | 3927 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|       3 | 3928 | `			return TRUE;` |
|       - | 3929 | `		}` |
|      10 | 3930 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|       6 | 3931 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|       3 | 3932 | `			return TRUE;` |
|       - | 3933 | `		}` |
|       5 | 3934 | `		pBase = pBase->pBase;` |
|       1 | 3935 | `	}` |
|       9 | 3936 | `	return FALSE;` |
|       9 | 3937 | `}` |
|       - | 3938 | `/*` |
|       - | 3939 | `` * Compile a single `case NAME [= value];` member of an enum body (PHP 8.1).`` |
|       - | 3940 | ` * A case is stored as a class constant (PH7_CLASS_ATTR_CONSTANT\|ENUMCASE) whose` |
|       - | 3941 | ` * aByteCode holds the BACKING value expression for backed enums (empty for pure` |
|       - | 3942 | ` * enums). The case's runtime value — the singleton instance — is materialized` |
|       - | 3943 | ` * lazily on first access (VmEnumMaterialize, vm.c), matching PHP's lazy` |
|       - | 3944 | ` * backing-value type/duplicate checks. Declaration order is recorded in` |
|       - | 3945 | ` * pClass->aEnumCases for cases().` |
|       - | 3946 | ` */` |
|     168 | 3947 | `static sxi32 GenStateCompileEnumCase(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 | 3948 | `{` |
|     173 | 3949 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 3950 | `	SySet *pInstrContainer;` |
|       - | 3951 | `	ph7_class_attr *pCase;` |
|       - | 3952 | `	SyString *pName;` |
|       - | 3953 | `	sxi32 rc;` |
|     173 | 3954 | `	pGen->pIn++; /* Jump the 'case' keyword */` |
|     173 | 3955 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 3956 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 3957 | `			"Invalid enum case name inside enum '%z'",&pClass->sName);` |
|     ! 0 | 3958 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3959 | `			return SXERR_ABORT;` |
|       - | 3960 | `		}` |
|     ! 0 | 3961 | `		goto Synchronize;` |
|       - | 3962 | `	}` |
|     173 | 3963 | `	pName = &pGen->pIn->sData;` |
|       - | 3964 | `	/* Cases share the class-constant namespace (php: "Cannot redefine class constant") */` |
|     173 | 3965 | `	if( SyHashGet(&pClass->hConst,(const void *)pName->zString,pName->nByte) != 0 ){` |
|     ! 0 | 3966 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 3967 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|     ! 0 | 3968 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3969 | `			return SXERR_ABORT;` |
|       - | 3970 | `		}` |
|     ! 0 | 3971 | `		goto Synchronize;` |
|       - | 3972 | `	}` |
|     173 | 3973 | `	pCase = PH7_NewClassAttr(pGen->pVm,pName,pGen->pIn->nLine,PH7_CLASS_PROT_PUBLIC,` |
|       - | 3974 | `		PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|     173 | 3975 | `	if( pCase == 0 ){` |
|     ! 0 | 3976 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 3977 | `		return SXERR_ABORT;` |
|       - | 3978 | `	}` |
|     173 | 3979 | `	GenStateConsumeDoc(&(*pGen),&pCase->sDoc);` |
|     173 | 3980 | `	if( GenStateConsumeAttrs(&(*pGen),&pCase->aAttrs) == SXERR_ABORT ){` |
|     ! 0 | 3981 | `		return SXERR_ABORT;` |
|       - | 3982 | `	}` |
|     173 | 3983 | `	if( GenStateCheckAttrPlacement(&(*pGen),&pCase->aAttrs,16,16,0,0) == SXERR_ABORT ){` |
|     ! 0 | 3984 | `		return SXERR_ABORT;` |
|       - | 3985 | `	}` |
|     173 | 3986 | `	pGen->pIn++; /* Jump the case name */` |
|     173 | 3987 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) ){` |
|     119 | 3988 | `		if( pClass->nEnumBacking == 0 ){` |
|       8 | 3989 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       2 | 3990 | `				"Case %z of non-backed enum %z must not have a value",pName,&pClass->sName);` |
|       6 | 3991 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3992 | `				return SXERR_ABORT;` |
|       - | 3993 | `			}` |
|       6 | 3994 | `			goto Synchronize;` |
|       - | 3995 | `		}` |
|     115 | 3996 | `		pGen->pIn++; /* Jump the equal sign */` |
|       - | 3997 | `		/* A backing value is a constant expression like any other: same rules, same` |
|       - | 3998 | ``		 * first-offender sentence, and no `new` (it is stored as a class constant). */`` |
|       - | 3999 | `		{` |
|     115 | 4000 | `			const char *zCErr = PH7_GenStateConstExprError(pGen,0);` |
|     115 | 4001 | `			if( zCErr ){` |
|       3 | 4002 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"%s",zCErr);` |
|       3 | 4003 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 4004 | `					return SXERR_ABORT;` |
|       - | 4005 | `				}` |
|       3 | 4006 | `				goto Synchronize;` |
|       - | 4007 | `			}` |
|       - | 4008 | `		}` |
|       - | 4009 | `		/* Compile the backing value expression into the case's own container` |
|       - | 4010 | `		 * (same technique as class constants). */` |
|     113 | 4011 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     113 | 4012 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pCase->aByteCode);` |
|     113 | 4013 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|     113 | 4014 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 | 4015 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 4016 | `				"Empty value for enum case %z::%z",&pClass->sName,pName);` |
|     ! 0 | 4017 | `		}` |
|     113 | 4018 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|     113 | 4019 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     113 | 4020 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4021 | `			return SXERR_ABORT;` |
|       - | 4022 | `		}` |
|      59 | 4023 | `	}else{` |
|      59 | 4024 | `		if( pClass->nEnumBacking != 0 ){` |
|     ! 0 | 4025 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 4026 | `				"Case %z of backed enum %z must have a value",pName,&pClass->sName);` |
|     ! 0 | 4027 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4028 | `				return SXERR_ABORT;` |
|       - | 4029 | `			}` |
|     ! 0 | 4030 | `			goto Synchronize;` |
|       - | 4031 | `		}` |
|       - | 4032 | `	}` |
|     167 | 4033 | `	rc = PH7_ClassInstallAttr(pClass,pCase);` |
|     167 | 4034 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4035 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4036 | `		return SXERR_ABORT;` |
|       - | 4037 | `	}` |
|     167 | 4038 | `	SySetPut(&pClass->aEnumCases,(const void *)&pCase);` |
|     167 | 4039 | `	return SXRET_OK;` |
|       3 | 4040 | `Synchronize:` |
|       - | 4041 | `	/* Synchronize with the first semi-colon */` |
|      25 | 4042 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|      19 | 4043 | `		pGen->pIn++;` |
|       3 | 4044 | `	}` |
|       9 | 4045 | `	return SXERR_CORRUPT;` |
|      89 | 4046 | `}` |
|       - | 4047 | `/*` |
|       - | 4048 | ` * Install the enum interface methods (PHP 8.1): cases() for every enum, plus` |
|       - | 4049 | ` * from()/tryFrom() for backed ones. They are NATIVE methods — the very same C` |
|       - | 4050 | ` * bodies an enum declared from C gets — because php's are internal: it reports` |
|       - | 4051 | `` * them as `<internal, prototype BackedEnum>` with no file and no line, and`` |
|       - | 4052 | `` * declares `from(string\|int $value): static` on the prototype rather than the`` |
|       - | 4053 | ` * enum's own backing type.` |
|       - | 4054 | ` *` |
|       - | 4055 | ` * This used to synthesize PHP source forwarding to three global` |
|       - | 4056 | `` * `__phl_enum_*` thunks, which put those names in php's namespace and reported`` |
|       - | 4057 | `` * every enum's three methods as `<user>` at the enum's own line.`` |
|       - | 4058 | ` */` |
|     128 | 4059 | `static sxi32 GenStateCompileEnumMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 | 4060 | `{` |
|     133 | 4061 | `	if( PH7_InstallEnumInterfaceMethods(pGen->pVm,pClass) != SXRET_OK ){` |
|     ! 0 | 4062 | `		PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4063 | `		return SXERR_ABORT;` |
|       - | 4064 | `	}` |
|     133 | 4065 | `	return SXRET_OK;` |
|      69 | 4066 | `}` |
|       - | 4067 | `/*` |
|       - | 4068 | ` * Magic methods an enum may not declare (php 8.1, zend_enum.c list —` |
|       - | 4069 | ` * __call/__callStatic/__invoke stay allowed).` |
|       - | 4070 | ` */` |
|       - | 4071 | `static const char *azEnumBannedMagic[] = {` |
|       - | 4072 | `	"__construct","__destruct","__clone","__get","__set","__isset","__unset",` |
|       - | 4073 | `	"__toString","__sleep","__wakeup","__serialize","__unserialize","__set_state"` |
|       - | 4074 | `};` |
|       - | 4075 | `/*` |
|       - | 4076 | ` * Enum post-body validation + synthesis: reject declared properties (including` |
|       - | 4077 | ``  * trait-imported ones) and banned magic methods, install the readonly `name` `` |
|       - | 4078 | `` * (and, for backed enums, `value`) instance properties the case singletons`` |
|       - | 4079 | ` * carry, and synthesize cases()/from()/tryFrom(). Runs after trait application` |
|       - | 4080 | ` * and before the class is installed.` |
|       - | 4081 | ` */` |
|     128 | 4082 | `static sxi32 GenStateEnumFinalize(ph7_gen_state *pGen,ph7_class *pClass,sxu32 nLine)` |
|       5 | 4083 | `{` |
|       - | 4084 | `	SyHashEntry *pEntry;` |
|       - | 4085 | `	sxi32 rc;` |
|       - | 4086 | `	sxu32 n;` |
|       - | 4087 | `	/* php: "Enum %s cannot include properties" */` |
|     133 | 4088 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     133 | 4089 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|       3 | 4090 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|       3 | 4091 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       3 | 4092 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine ? pAttr->nLine : nLine,` |
|       1 | 4093 | `				"Enum %z cannot include properties",&pClass->sName);` |
|       3 | 4094 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4095 | `				return SXERR_ABORT;` |
|       - | 4096 | `			}` |
|       3 | 4097 | `			break;` |
|       - | 4098 | `		}` |
|     ! 0 | 4099 | `	}` |
|       - | 4100 | `	/* php: "Enum %s cannot include magic method %s" */` |
|    1797 | 4101 | `	for( n = 0 ; n < SX_ARRAYSIZE(azEnumBannedMagic) ; n++ ){` |
|    2496 | 4102 | `		if( SyHashGet(&pClass->hMethod,(const void *)azEnumBannedMagic[n],` |
|    1669 | 4103 | `			SyStrlen(azEnumBannedMagic[n])) != 0 ){` |
|     ! 0 | 4104 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 4105 | `				"Enum %z cannot include magic method %s",&pClass->sName,azEnumBannedMagic[n]);` |
|     ! 0 | 4106 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4107 | `				return SXERR_ABORT;` |
|       - | 4108 | `			}` |
|     ! 0 | 4109 | `		}` |
|     837 | 4110 | `	}` |
|       - | 4111 | ``	/* Install the case-singleton instance properties: readonly `name` (every`` |
|       - | 4112 | ``	 * enum) and `value` (backed only). Materialization (vm.c) fills them and`` |
|       - | 4113 | `	 * clears the readonly write-once latch; user writes then raise php's` |
|       - | 4114 | `	 * "Cannot modify readonly property" through the normal store path. */` |
|       - | 4115 | `	{` |
|       - | 4116 | `		static const SyString sNameProp = { "name",sizeof("name")-1 };` |
|       - | 4117 | `		static const SyString sValueProp = { "value",sizeof("value")-1 };` |
|       - | 4118 | `		ph7_class_attr *pAttr;` |
|     133 | 4119 | `		pAttr = PH7_NewClassAttr(pGen->pVm,&sNameProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|       - | 4120 | `			PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|     133 | 4121 | `		if( pAttr == 0 ){` |
|     ! 0 | 4122 | `			PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4123 | `			return SXERR_ABORT;` |
|       - | 4124 | `		}` |
|     133 | 4125 | `		pAttr->nType = MEMOBJ_STRING;` |
|     133 | 4126 | `		SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|     133 | 4127 | `		PH7_ClassInstallAttr(pClass,pAttr);` |
|     133 | 4128 | `		if( pClass->nEnumBacking != 0 ){` |
|      69 | 4129 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&sValueProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|       - | 4130 | `				PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      69 | 4131 | `			if( pAttr == 0 ){` |
|     ! 0 | 4132 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4133 | `				return SXERR_ABORT;` |
|       - | 4134 | `			}` |
|      69 | 4135 | `			pAttr->nType = pClass->nEnumBacking;` |
|      69 | 4136 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|      25 | 4137 | `				SyStringInitFromBuf(&pAttr->sTypeName,"int",sizeof("int")-1);` |
|      14 | 4138 | `			}else{` |
|      47 | 4139 | `				SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|       - | 4140 | `			}` |
|      69 | 4141 | `			PH7_ClassInstallAttr(pClass,pAttr);` |
|      32 | 4142 | `		}` |
|       - | 4143 | `	}` |
|     133 | 4144 | `	return GenStateCompileEnumMethods(&(*pGen),pClass);` |
|      69 | 4145 | `}` |
|       - | 4146 | `/*` |
|       - | 4147 | ` * Deferred class declarations (class/anonymous-class extending an` |
|       - | 4148 | ` * autoloaded parent).` |
|       - | 4149 | ` *` |
|       - | 4150 | ` * A class declaration compiles INLINE while its enclosing file compiles, so a` |
|       - | 4151 | ` * parent/interface/trait that an autoloader would provide is unreachable when` |
|       - | 4152 | ` * the autoloader's own spl_autoload_register() statement has not EXECUTED yet` |
|       - | 4153 | `` * (same-file registration, or `new class extends \App\Child {}` anywhere).`` |
|       - | 4154 | ` * php's model has no such problem: a declaration with unresolved dependencies` |
|       - | 4155 | ` * is declared at its EXECUTION point, in statement order, not hoisted.` |
|       - | 4156 | ` *` |
|       - | 4157 | ` * These helpers reproduce that: before compiling a declaration, scan its` |
|       - | 4158 | `` * header (extends/implements) and body (depth-1 trait `use`) for referenced`` |
|       - | 4159 | ` * names and try to resolve each (firing autoload exactly where the normal` |
|       - | 4160 | ` * compile would). If any name is still missing, the WHOLE declaration is` |
|       - | 4161 | `` * captured as re-compilable source — a reconstructed `namespace`/`use`-import/`` |
|       - | 4162 | ` * doc/attribute/modifier prefix plus the declaration's raw text — recorded in` |
|       - | 4163 | ` * a VmDeferredClass, and OP_CLASS_DEFER is emitted at the declaration site.` |
|       - | 4164 | ` * At runtime (VmExecDeferredClass, vm_include.c) the autoloader is live: each` |
|       - | 4165 | `` * recorded name resolves or throws php's catchable `... not found` Error, and`` |
|       - | 4166 | ` * the chunk re-compiles through VmEvalChunk. An anonymous class re-compiles` |
|       - | 4167 | `` * inside `if (false) { new ... }` (installing the class without instantiating`` |
|       - | 4168 | ` * it) under its original synthesized name via pVm->sDeferAnonName; the site's` |
|       - | 4169 | ` * own OP_NEW then instantiates it with the site-compiled arguments.` |
|       - | 4170 | ` *` |
|       - | 4171 | ` * Behavior shifts only for declarations that previously died with the` |
|       - | 4172 | ` * compile-time "Nonexistent base class" fatal: they now follow php — succeed` |
|       - | 4173 | ` * when the autoloader is registered first, or throw php's catchable` |
|       - | 4174 | `` * `Class/Interface/Trait "X" not found` Error at the declaration point.`` |
|       - | 4175 | ` * A deferred declaration's OTHER compile errors (a body syntax error) shift` |
|       - | 4176 | ` * from file-compile time to the declaration's execution — still loud, timing` |
|       - | 4177 | ` * differs from php (recorded).` |
|       - | 4178 | ` */` |
|     462 | 4179 | `static void GenStateDeferEmitUses(SyBlob *pOut,SyHash *pTable,const char *zKind)` |
|       5 | 4180 | `{` |
|       - | 4181 | `	SyHashEntry *pEntry;` |
|     467 | 4182 | `	SyHashResetLoopCursor(pTable);` |
|     782 | 4183 | `	while( (pEntry = SyHashGetNextEntry(pTable)) != 0 ){` |
|      86 | 4184 | `		const char *zFqn = (const char *)pEntry->pUserData;` |
|      86 | 4185 | `		if( zFqn ){` |
|     128 | 4186 | `			SyBlobFormat(pOut,"use %s%s as %.*s;\n",zKind,zFqn,` |
|      84 | 4187 | `				(int)pEntry->nKeyLen,(const char *)pEntry->pKey);` |
|      42 | 4188 | `		}` |
|       2 | 4189 | `	}` |
|     467 | 4190 | `}` |
|       - | 4191 | `/*` |
|       - | 4192 | ` * Parse one class reference at *ppCur (bounded by pEnd) with the SAME` |
|       - | 4193 | ` * namespace/import resolution the real compile uses, and append it to pNames.` |
|       - | 4194 | ` * Advances *ppCur past the reference. Returns SXERR_INVALID on a malformed` |
|       - | 4195 | ` * reference (caller bails out of deferral and lets the normal path report).` |
|       - | 4196 | ` */` |
|    1950 | 4197 | `static sxi32 GenStateDeferRecordRef(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd,` |
|       - | 4198 | `	sxu8 cKind,SySet *pNames)` |
|       5 | 4199 | `{` |
|    1955 | 4200 | `	SyToken *pSavedIn = pGen->pIn;` |
|    1955 | 4201 | `	SyToken *pSavedEnd = pGen->pEnd;` |
|       - | 4202 | `	SyBlob sFqn;` |
|       - | 4203 | `	VmDeferredReq sReq;` |
|       - | 4204 | `	char *zDup;` |
|       - | 4205 | `	sxi32 rc;` |
|    1955 | 4206 | `	SyBlobInit(&sFqn,&pGen->pVm->sAllocator);` |
|    1955 | 4207 | `	pGen->pIn = *ppCur;` |
|    1955 | 4208 | `	pGen->pEnd = pEnd;` |
|    1955 | 4209 | `	rc = GenStateParseClassReference(pGen,&sFqn);` |
|    1955 | 4210 | `	*ppCur = pGen->pIn;` |
|    1955 | 4211 | `	pGen->pIn = pSavedIn;` |
|    1955 | 4212 | `	pGen->pEnd = pSavedEnd;` |
|    1955 | 4213 | `	if( rc != SXRET_OK \|\| SyBlobLength(&sFqn) < 1 ){` |
|       3 | 4214 | `		SyBlobRelease(&sFqn);` |
|       3 | 4215 | `		return SXERR_INVALID;` |
|       - | 4216 | `	}` |
|    2927 | 4217 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    1948 | 4218 | `		(const char *)SyBlobData(&sFqn),SyBlobLength(&sFqn));` |
|    1953 | 4219 | `	if( zDup == 0 ){` |
|     ! 0 | 4220 | `		SyBlobRelease(&sFqn);` |
|     ! 0 | 4221 | `		return SXERR_INVALID;` |
|       - | 4222 | `	}` |
|    1953 | 4223 | `	SyStringInitFromBuf(&sReq.sName,zDup,SyBlobLength(&sFqn));` |
|    1953 | 4224 | `	sReq.cKind = cKind;` |
|    1953 | 4225 | `	SySetPut(pNames,(const void *)&sReq);` |
|    1953 | 4226 | `	SyBlobRelease(&sFqn);` |
|    1953 | 4227 | `	return SXRET_OK;` |
|     980 | 4228 | `}` |
|       - | 4229 | `/*` |
|       - | 4230 | ` * Look a class/interface/trait name up WITHOUT asking the autoloader: is this` |
|       - | 4231 | ` * name declared right now? php's early binding asks exactly this question --` |
|       - | 4232 | ` * zend_try_early_binding does a plain class-table lookup and gives up if the` |
|       - | 4233 | ` * parent is not there yet, because a compile-time autoload would run user code` |
|       - | 4234 | ` * in the middle of compiling a file.` |
|       - | 4235 | ` */` |
|      34 | 4236 | `static ph7_class * GenStateFindDeclaredClass(ph7_vm *pVm,const char *zName,sxu32 nByte)` |
|       3 | 4237 | `{` |
|       - | 4238 | `	SyHashEntry *pEntry;` |
|      37 | 4239 | `	PH7_VmClassNameAnchor(&zName,&nByte);` |
|      37 | 4240 | `	if( nByte < 1 ){` |
|     ! 0 | 4241 | `		return 0;` |
|       - | 4242 | `	}` |
|      37 | 4243 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|      37 | 4244 | `	return pEntry ? (ph7_class *)pEntry->pUserData : 0;` |
|      20 | 4245 | `}` |
|       - | 4246 | `/*` |
|       - | 4247 | ` * Scan the declaration whose keyword pGen->pIn sits on (class/enum/interface/` |
|       - | 4248 | `` * trait, or an anonymous `class(args)`) WITHOUT consuming tokens. Collects`` |
|       - | 4249 | ` * every referenced dependency name, locates the body braces, and filters the` |
|       - | 4250 | ` * collected names down to the UNRESOLVABLE ones. SXRET_OK with an empty` |
|       - | 4251 | ` * pMissing set means "compile normally"; a non-empty set means "defer". Any` |
|       - | 4252 | ` * structural surprise returns SXERR_INVALID so the normal compile reports it.` |
|       - | 4253 | ` *` |
|       - | 4254 | ` * bNoAutoload picks which question the filter asks -- see its use below.` |
|       - | 4255 | ` */` |
|    5736 | 4256 | `static sxi32 GenStateScanDeferDeps(ph7_gen_state *pGen,int bAnon,int iSelfKind,` |
|       - | 4257 | `	SySet *pMissing,SyToken **ppBody,SyToken **ppBodyEnd,SyBlob *pSelfFqn,int bNoAutoload)` |
|       5 | 4258 | `{` |
|    5741 | 4259 | `	SyToken *pCur = pGen->pIn; /* on the declaration keyword */` |
|    5741 | 4260 | `	SyToken *pEnd = pGen->pEnd;` |
|       - | 4261 | `	SySet aNames;` |
|    5741 | 4262 | `	sxi32 rc = SXRET_OK;` |
|    5741 | 4263 | `	*ppBody = *ppBodyEnd = 0;` |
|    5741 | 4264 | `	SySetInit(&aNames,&pGen->pVm->sAllocator,sizeof(VmDeferredReq));` |
|    5741 | 4265 | `	pCur++; /* Jump the keyword */` |
|    5741 | 4266 | `	if( bAnon ){` |
|     146 | 4267 | `		if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|      71 | 4268 | `			SyToken *pClose = 0;` |
|      71 | 4269 | `			pCur++;` |
|      71 | 4270 | `			PH7_DelimitNestedTokens(pCur,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|      71 | 4271 | `			if( pClose == 0 \|\| pClose >= pEnd ){` |
|     ! 0 | 4272 | `				SySetRelease(&aNames);` |
|     ! 0 | 4273 | `				return SXERR_INVALID;` |
|       - | 4274 | `			}` |
|      71 | 4275 | `			pCur = &pClose[1];` |
|      34 | 4276 | `		}` |
|      75 | 4277 | `	}else{` |
|    5599 | 4278 | `		if( pCur >= pEnd \|\| !PH7_IsClassNameToken(pCur) ){` |
|       - | 4279 | `			/* Same name test the declaration compiler uses: a word php lets name a` |
|       - | 4280 | ``			 * class may be one of PHL's KEYWORD tokens (`class Integer …`), and`` |
|       - | 4281 | `			 * demanding a plain ID here sent such a declaration down the` |
|       - | 4282 | `			 * non-deferring path, where a not-yet-loaded parent is a fatal. */` |
|     ! 0 | 4283 | `			SySetRelease(&aNames);` |
|     ! 0 | 4284 | `			return SXERR_INVALID;` |
|       - | 4285 | `		}` |
|    5599 | 4286 | `		GenStateBuildFQN(pGen,&pCur->sData,pSelfFqn);` |
|    5599 | 4287 | `		pCur++;` |
|       - | 4288 | `	}` |
|       - | 4289 | ``	/* Header: extends/implements lists up to the '{' (an enum's `: int` backing`` |
|       - | 4290 | `	 * and any stray tokens pass through; malformed headers bail to the normal` |
|       - | 4291 | `	 * path's diagnostics). */` |
|    7395 | 4292 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_OCB) == 0 ){` |
|    1661 | 4293 | `		int iKind = -1;` |
|    1661 | 4294 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    1591 | 4295 | `			sxi32 nKw = SX_PTR_TO_INT(pCur->pUserData);` |
|    1591 | 4296 | `			if( nKw == PH7_TKWRD_EXTENDS ){` |
|    1107 | 4297 | `				iKind = (iSelfKind == PH7_DEFER_KIND_INTERFACE)` |
|     551 | 4298 | `					? PH7_DEFER_KIND_INTERFACE : PH7_DEFER_KIND_CLASS;` |
|    1040 | 4299 | `			}else if( nKw == PH7_TKWRD_IMPLEMENTS ){` |
|     419 | 4300 | `				iKind = PH7_DEFER_KIND_INTERFACE;` |
|     207 | 4301 | `			}` |
|     793 | 4302 | `		}` |
|    1661 | 4303 | `		if( iKind < 0 ){` |
|     145 | 4304 | `			pCur++;` |
|     145 | 4305 | `			continue;` |
|       - | 4306 | `		}` |
|    1521 | 4307 | `		pCur++; /* Jump extends/implements */` |
|     758 | 4308 | `		for(;;){` |
|    1569 | 4309 | `			if( GenStateDeferRecordRef(pGen,&pCur,pEnd,(sxu8)iKind,&aNames) != SXRET_OK ){` |
|       3 | 4310 | `				SySetRelease(&aNames);` |
|       3 | 4311 | `				return SXERR_INVALID;` |
|       - | 4312 | `			}` |
|    1567 | 4313 | `			if( pCur < pEnd && (pCur->nType & PH7_TK_COMMA) ){` |
|      53 | 4314 | `				pCur++;` |
|      53 | 4315 | `				continue;` |
|       - | 4316 | `			}` |
|    1519 | 4317 | `			break;` |
|     ! 0 | 4318 | `		}` |
|       5 | 4319 | `	}` |
|    5739 | 4320 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 | 4321 | `		SySetRelease(&aNames);` |
|     ! 0 | 4322 | `		return SXERR_INVALID;` |
|       - | 4323 | `	}` |
|    5739 | 4324 | `	*ppBody = pCur;` |
|       - | 4325 | `	{` |
|    5739 | 4326 | `		SyToken *pClose = 0;` |
|    5739 | 4327 | `		PH7_DelimitNestedTokens(&pCur[1],pEnd,PH7_TK_OCB,PH7_TK_CCB,&pClose);` |
|    5739 | 4328 | `		if( pClose == 0 \|\| pClose >= pEnd ){` |
|     ! 0 | 4329 | `			SySetRelease(&aNames);` |
|     ! 0 | 4330 | `			return SXERR_INVALID;` |
|       - | 4331 | `		}` |
|    5739 | 4332 | `		*ppBodyEnd = pClose;` |
|       - | 4333 | `	}` |
|       - | 4334 | ``	/* Body: depth-1 trait `use Name[, Name]` statements. Statement position only`` |
|       - | 4335 | ``	 * (previous token one of '{' '}' ';'), so a closure's `use ($x)` — which`` |
|       - | 4336 | `	 * follows a ')' — never matches. */` |
|       - | 4337 | `	{` |
|    5739 | 4338 | `		SyToken *p = &(*ppBody)[1];` |
|    5739 | 4339 | `		int bStmtPos = 1;` |
|    5739 | 4340 | `		sxi32 iDepth = 1;` |
|  123727 | 4341 | `		while( p < *ppBodyEnd ){` |
|  117993 | 4342 | `			if( p->nType & PH7_TK_OCB ){` |
|    5409 | 4343 | `				iDepth++;` |
|    5409 | 4344 | `				bStmtPos = 1;` |
|    5409 | 4345 | `				p++;` |
|    5409 | 4346 | `				continue;` |
|       - | 4347 | `			}` |
|  112589 | 4348 | `			if( p->nType & PH7_TK_CCB ){` |
|    5409 | 4349 | `				iDepth--;` |
|    5409 | 4350 | `				bStmtPos = 1;` |
|    5409 | 4351 | `				p++;` |
|    5409 | 4352 | `				continue;` |
|       - | 4353 | `			}` |
|  107185 | 4354 | `			if( p->nType & PH7_TK_SEMI ){` |
|    9089 | 4355 | `				bStmtPos = 1;` |
|    9089 | 4356 | `				p++;` |
|    9089 | 4357 | `				continue;` |
|       - | 4358 | `			}` |
|   98096 | 4359 | `			if( iDepth == 1 && bStmtPos && (p->nType & PH7_TK_KEYWORD)` |
|    8701 | 4360 | `			 && SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_USE ){` |
|     341 | 4361 | `				p++;` |
|     168 | 4362 | `				for(;;){` |
|     391 | 4363 | `					if( GenStateDeferRecordRef(pGen,&p,*ppBodyEnd,PH7_DEFER_KIND_TRAIT,&aNames) != SXRET_OK ){` |
|     ! 0 | 4364 | `						SySetRelease(&aNames);` |
|     ! 0 | 4365 | `						return SXERR_INVALID;` |
|       - | 4366 | `					}` |
|     391 | 4367 | `					if( p < *ppBodyEnd && (p->nType & PH7_TK_COMMA) ){` |
|      55 | 4368 | `						p++;` |
|      55 | 4369 | `						continue;` |
|       - | 4370 | `					}` |
|     341 | 4371 | `					break;` |
|     ! 0 | 4372 | `				}` |
|     341 | 4373 | `				continue;` |
|       - | 4374 | `			}` |
|   97765 | 4375 | `			bStmtPos = 0;` |
|   97765 | 4376 | `			p++;` |
|       5 | 4377 | `		}` |
|       - | 4378 | `	}` |
|       - | 4379 | `	/* Filter: keep only the names that do NOT resolve. For a declaration that` |
|       - | 4380 | `	 * will be compiled where it stands, the lookup fires the autoloader exactly` |
|       - | 4381 | `	 * where the replaced compile would. For a CONDITIONAL one it must not: the` |
|       - | 4382 | `	 * declaration is deferred whatever this answers, and php never resolves a` |
|       - | 4383 | ``	 * parent it has not reached. `if (false) { class C extends B {} }` is the`` |
|       - | 4384 | `	 * shape that shows it -- nikic/php-parser's own class aliases are written` |
|       - | 4385 | ``	 * that way, with a `require` of the parent's file BEFORE the dead block, so`` |
|       - | 4386 | `	 * an autoload here loaded that file first and the require then declared` |
|       - | 4387 | `	 * everything in it a second time. */` |
|       - | 4388 | `	{` |
|    5739 | 4389 | `		VmDeferredReq *aReq = (VmDeferredReq *)SySetBasePtr(&aNames);` |
|       - | 4390 | `		sxu32 n;` |
|    7687 | 4391 | `		for( n = 0 ; n < SySetUsed(&aNames) ; ++n ){` |
|    1953 | 4392 | `			ph7_class *pFound = bNoAutoload` |
|      34 | 4393 | `				? GenStateFindDeclaredClass(pGen->pVm,aReq[n].sName.zString,aReq[n].sName.nByte)` |
|    1931 | 4394 | `				: PH7_VmExtractClass(pGen->pVm,aReq[n].sName.zString,aReq[n].sName.nByte,FALSE,0);` |
|    1953 | 4395 | `			if( pFound == 0 ){` |
|      69 | 4396 | `				SySetPut(pMissing,(const void *)&aReq[n]);` |
|      32 | 4397 | `			}` |
|     979 | 4398 | `		}` |
|       - | 4399 | `	}` |
|    5739 | 4400 | `	SySetRelease(&aNames);` |
|    5739 | 4401 | `	return rc;` |
|    2873 | 4402 | `}` |
|       - | 4403 | `/*` |
|       - | 4404 | ` * Capture the declaration as a re-compilable chunk, record it, and emit` |
|       - | 4405 | ` * OP_CLASS_DEFER at the current emission point. On return the statement` |
|       - | 4406 | ` * cursor sits past the declaration's closing '}'. pMissing's entries are` |
|       - | 4407 | ` * COPIED into the record (their name bytes are already allocator-owned).` |
|       - | 4408 | ` */` |
|     154 | 4409 | `static sxi32 GenStateEmitDeferredClass(ph7_gen_state *pGen,sxi32 iFlags,int bAnon,` |
|       - | 4410 | `	SySet *pMissing,SyToken *pBodyEnd,SyBlob *pSelfFqn,const SyString *pAnonName)` |
|       5 | 4411 | `{` |
|     159 | 4412 | `	SyToken *pKw = pGen->pIn; /* the declaration keyword */` |
|       - | 4413 | `	VmDeferredClass *pDefer;` |
|       - | 4414 | `	SyBlob sChunk;` |
|       - | 4415 | `	const char *zFrom;` |
|       - | 4416 | `	const char *zTo;` |
|       - | 4417 | `	char *zDup;` |
|     159 | 4418 | `	SyBlobInit(&sChunk,&pGen->pVm->sAllocator);` |
|       - | 4419 | `	/* The declaration site's compile context is replayed as literal statements:` |
|       - | 4420 | `	 * strict_types first (it must open the chunk), then namespace and the` |
|       - | 4421 | `	 * use-import tables — the runtime re-compile starts in a fresh scope. */` |
|     159 | 4422 | `	if( pGen->bStrictTypes ){` |
|     ! 0 | 4423 | `		SyBlobAppend(&sChunk,"declare(strict_types=1);\n",sizeof("declare(strict_types=1);\n")-1);` |
|     ! 0 | 4424 | `	}` |
|     159 | 4425 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      86 | 4426 | `		SyBlobFormat(&sChunk,"namespace %.*s;\n",` |
|      82 | 4427 | `			(int)SyBlobLength(&pGen->sNamespace),(const char *)SyBlobData(&pGen->sNamespace));` |
|      41 | 4428 | `	}` |
|     159 | 4429 | `	GenStateDeferEmitUses(&sChunk,&pGen->hUseImports,"");` |
|     159 | 4430 | `	GenStateDeferEmitUses(&sChunk,&pGen->hUseFuncImports,"function ");` |
|     159 | 4431 | `	GenStateDeferEmitUses(&sChunk,&pGen->hUseConstImports,"const ");` |
|       - | 4432 | `	/* Doc-comment and attribute groups precede the keyword in the raw source,` |
|       - | 4433 | `	 * outside the captured span — re-emit them from the trivia sidecar. */` |
|     159 | 4434 | `	if( !bAnon && pGen->sPendingDoc.nByte > 0 ){` |
|     ! 0 | 4435 | `		SyBlobAppend(&sChunk,pGen->sPendingDoc.zString,pGen->sPendingDoc.nByte);` |
|     ! 0 | 4436 | `		SyBlobAppend(&sChunk,"\n",1);` |
|     ! 0 | 4437 | `	}` |
|       - | 4438 | `	{` |
|       - | 4439 | `		ph7_trivia *aT;` |
|       - | 4440 | `		sxu32 nT,n;` |
|     159 | 4441 | `		if( bAnon ){` |
|       - | 4442 | ``			/* `new #[A] class` trivia is keyed to the 'class' token */`` |
|      10 | 4443 | `			SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|      10 | 4444 | `			aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|      10 | 4445 | `			nT = SySetUsed(&pGen->aTrivia);` |
|      10 | 4446 | `			if( pGen->pTokenSet && pKw >= pBase && pKw < &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|      10 | 4447 | `				sxu32 nIdx = (sxu32)(pKw - pBase);` |
|      10 | 4448 | `				for( n = 0 ; n < nT ; ++n ){` |
|     ! 0 | 4449 | `					if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|     ! 0 | 4450 | `						SyBlobFormat(&sChunk,"#[%.*s]\n",(int)aT[n].sText.nByte,aT[n].sText.zString);` |
|     ! 0 | 4451 | `					}` |
|     ! 0 | 4452 | `				}` |
|       4 | 4453 | `			}` |
|       6 | 4454 | `		}else{` |
|     151 | 4455 | `			aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|     151 | 4456 | `			nT = SySetUsed(&pGen->aPendingAttrs);` |
|     171 | 4457 | `			for( n = 0 ; n < nT ; ++n ){` |
|      21 | 4458 | `				if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|      21 | 4459 | `					SyBlobFormat(&sChunk,"#[%.*s]\n",(int)aT[n].sText.nByte,aT[n].sText.zString);` |
|      10 | 4460 | `				}` |
|      11 | 4461 | `			}` |
|       - | 4462 | `		}` |
|       - | 4463 | `	}` |
|       - | 4464 | `	/* Pad the prefix with newlines so the declaration keyword sits on its` |
|       - | 4465 | `	 * ORIGINAL line inside the chunk — runtime diagnostics from the deferred` |
|       - | 4466 | `	 * compile then report the source's real line. Best-effort: a prefix` |
|       - | 4467 | `	 * already longer than the declaration line skips the padding. */` |
|       - | 4468 | `	{` |
|     159 | 4469 | `		const char *zScan = (const char *)SyBlobData(&sChunk);` |
|     159 | 4470 | `		sxu32 nHave = 0;` |
|       - | 4471 | `		sxu32 nScan;` |
|    4741 | 4472 | `		for( nScan = 0 ; nScan < SyBlobLength(&sChunk) ; ++nScan ){` |
|    4586 | 4473 | `			if( zScan[nScan] == '\n' ){` |
|     190 | 4474 | `				nHave++;` |
|      93 | 4475 | `			}` |
|    2295 | 4476 | `		}` |
|    1837 | 4477 | `		while( nHave + 1 < pKw->nLine ){` |
|    1683 | 4478 | `			SyBlobAppend(&sChunk,"\n",1);` |
|    1683 | 4479 | `			nHave++;` |
|       5 | 4480 | `		}` |
|       - | 4481 | `	}` |
|     159 | 4482 | `	if( bAnon ){` |
|       - | 4483 | ``		/* `if (false) { new class <header-minus-args> { body } ; }` — installs`` |
|       - | 4484 | `		 * the class at the chunk's compile, never instantiates it. */` |
|      10 | 4485 | `		SyToken *pAfterArgs = &pKw[1];` |
|      10 | 4486 | `		SyBlobAppend(&sChunk,"if (false) { new ",sizeof("if (false) { new ")-1);` |
|      10 | 4487 | `		SyBlobAppend(&sChunk,pKw->sData.zString,pKw->sData.nByte);` |
|      10 | 4488 | `		if( pAfterArgs < pGen->pEnd && (pAfterArgs->nType & PH7_TK_LPAREN) ){` |
|       3 | 4489 | `			SyToken *pClose = 0;` |
|       3 | 4490 | `			PH7_DelimitNestedTokens(&pAfterArgs[1],pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|       3 | 4491 | `			if( pClose == 0 \|\| pClose >= pGen->pEnd ){` |
|     ! 0 | 4492 | `				SyBlobRelease(&sChunk);` |
|     ! 0 | 4493 | `				return SXERR_INVALID;` |
|       - | 4494 | `			}` |
|       3 | 4495 | `			pAfterArgs = &pClose[1];` |
|       1 | 4496 | `		}` |
|      10 | 4497 | `		if( pAfterArgs < pBodyEnd ){` |
|      10 | 4498 | `			SyBlobAppend(&sChunk," ",1);` |
|      10 | 4499 | `			zFrom = pAfterArgs->sData.zString;` |
|      10 | 4500 | `			zTo = pBodyEnd->sData.zString + pBodyEnd->sData.nByte;` |
|      10 | 4501 | `			SyBlobAppend(&sChunk,zFrom,(sxu32)(zTo - zFrom));` |
|       4 | 4502 | `		}` |
|      10 | 4503 | `		SyBlobAppend(&sChunk,"; }",sizeof("; }")-1);` |
|       6 | 4504 | `	}else{` |
|       - | 4505 | `		/* Modifiers were consumed before this compiler ran; reconstruct them` |
|       - | 4506 | ``		 * (an enum's implicit `final` must NOT be spelled out). */`` |
|     146 | 4507 | `		if( (iFlags & PH7_CLASS_ENUM) == 0` |
|     145 | 4508 | `		 && (pKw->nType & PH7_TK_KEYWORD)` |
|     149 | 4509 | `		 && SX_PTR_TO_INT(pKw->pUserData) == PH7_TKWRD_CLASS ){` |
|     121 | 4510 | `			if( iFlags & PH7_CLASS_FINAL ){` |
|       3 | 4511 | `				SyBlobAppend(&sChunk,"final ",sizeof("final ")-1);` |
|       1 | 4512 | `			}` |
|     121 | 4513 | `			if( iFlags & PH7_CLASS_ABSTRACT ){` |
|     ! 0 | 4514 | `				SyBlobAppend(&sChunk,"abstract ",sizeof("abstract ")-1);` |
|     ! 0 | 4515 | `			}` |
|     121 | 4516 | `			if( iFlags & PH7_CLASS_READONLY ){` |
|     ! 0 | 4517 | `				SyBlobAppend(&sChunk,"readonly ",sizeof("readonly ")-1);` |
|     ! 0 | 4518 | `			}` |
|      58 | 4519 | `		}` |
|     151 | 4520 | `		zFrom = pKw->sData.zString;` |
|     151 | 4521 | `		zTo = pBodyEnd->sData.zString + pBodyEnd->sData.nByte;` |
|     151 | 4522 | `		SyBlobAppend(&sChunk,zFrom,(sxu32)(zTo - zFrom));` |
|       - | 4523 | `	}` |
|     159 | 4524 | `	pDefer = (VmDeferredClass *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmDeferredClass));` |
|     159 | 4525 | `	if( pDefer == 0 ){` |
|     ! 0 | 4526 | `		SyBlobRelease(&sChunk);` |
|     ! 0 | 4527 | `		PH7_GenCompileError(pGen,E_ERROR,pKw->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4528 | `		return SXERR_ABORT;` |
|       - | 4529 | `	}` |
|     159 | 4530 | `	SyZero(pDefer,sizeof(VmDeferredClass));` |
|     236 | 4531 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     154 | 4532 | `		(const char *)SyBlobData(&sChunk),SyBlobLength(&sChunk));` |
|     159 | 4533 | `	SyBlobRelease(&sChunk);` |
|     159 | 4534 | `	if( zDup == 0 ){` |
|     ! 0 | 4535 | `		PH7_GenCompileError(pGen,E_ERROR,pKw->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4536 | `		return SXERR_ABORT;` |
|       - | 4537 | `	}` |
|     159 | 4538 | `	SyStringInitFromBuf(&pDefer->sText,zDup,SyStrlen(zDup));` |
|     159 | 4539 | `	if( bAnon ){` |
|      10 | 4540 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pAnonName->zString,pAnonName->nByte);` |
|      10 | 4541 | `		if( zDup == 0 ){` |
|     ! 0 | 4542 | `			PH7_GenCompileError(pGen,E_ERROR,pKw->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4543 | `			return SXERR_ABORT;` |
|       - | 4544 | `		}` |
|      10 | 4545 | `		SyStringInitFromBuf(&pDefer->sAnonName,zDup,pAnonName->nByte);` |
|      10 | 4546 | `		pDefer->sSelfName = pDefer->sAnonName;` |
|       6 | 4547 | `	}else{` |
|     224 | 4548 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     146 | 4549 | `			(const char *)SyBlobData(pSelfFqn),SyBlobLength(pSelfFqn));` |
|     151 | 4550 | `		if( zDup == 0 ){` |
|     ! 0 | 4551 | `			PH7_GenCompileError(pGen,E_ERROR,pKw->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4552 | `			return SXERR_ABORT;` |
|       - | 4553 | `		}` |
|     151 | 4554 | `		SyStringInitFromBuf(&pDefer->sSelfName,zDup,SyBlobLength(pSelfFqn));` |
|       - | 4555 | `	}` |
|     159 | 4556 | `	SySetInit(&pDefer->aRequired,&pGen->pVm->sAllocator,sizeof(VmDeferredReq));` |
|       - | 4557 | `	{` |
|     159 | 4558 | `		VmDeferredReq *aReq = (VmDeferredReq *)SySetBasePtr(pMissing);` |
|       - | 4559 | `		sxu32 n;` |
|     221 | 4560 | `		for( n = 0 ; n < SySetUsed(pMissing) ; ++n ){` |
|      67 | 4561 | `			SySetPut(&pDefer->aRequired,(const void *)&aReq[n]);` |
|      36 | 4562 | `		}` |
|       - | 4563 | `	}` |
|     159 | 4564 | `	pDefer->nLine = pKw->nLine;` |
|     159 | 4565 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLASS_DEFER,0,0,(void *)pDefer,0);` |
|       - | 4566 | `	/* Skip the declaration: the statement cursor lands past its '}' */` |
|     159 | 4567 | `	pGen->pIn = &pBodyEnd[1];` |
|     159 | 4568 | `	return SXRET_OK;` |
|      82 | 4569 | `}` |
|       - | 4570 | `/*` |
|       - | 4571 | ` * Deferral gate shared by the named-declaration compilers: scan the` |
|       - | 4572 | ` * declaration at pGen->pIn; when a dependency is missing, capture + emit the` |
|       - | 4573 | ` * deferred record and return TRUE (the caller returns immediately — the` |
|       - | 4574 | ` * declaration compiles at execution time). FALSE means compile normally.` |
|       - | 4575 | ` * *pRc carries SXERR_ABORT out of the capture path.` |
|       - | 4576 | ` */` |
|    5666 | 4577 | `static int GenStateMaybeDeferClass(ph7_gen_state *pGen,sxi32 iFlags,int iSelfKind,sxi32 *pRc)` |
|       5 | 4578 | `{` |
|       - | 4579 | `	SySet aMissing;` |
|    5671 | 4580 | `	SyToken *pBody = 0;` |
|    5671 | 4581 | `	SyToken *pBodyEnd = 0;` |
|       - | 4582 | `	SyBlob sSelfFqn;` |
|    5671 | 4583 | `	int bDefer = 0;` |
|       - | 4584 | `	/* php's binding rule, decided before anything is resolved: a declaration that` |
|       - | 4585 | `	 * is not at a unit's top level is bound when execution REACHES it, so it is` |
|       - | 4586 | `	 * deferred whatever its dependencies look like -- and nothing about it may be` |
|       - | 4587 | `	 * resolved here. */` |
|    5671 | 4588 | `	int bCond = GenStateDeclIsConditional(&(*pGen));` |
|    5671 | 4589 | `	*pRc = SXRET_OK;` |
|    5671 | 4590 | `	if( pGen->pVm->bSyntaxCheck ){` |
|       - | 4591 | ``		/* `phl -l`: the declaration is never EXECUTED, so there is nothing to`` |
|       - | 4592 | `		 * defer it to -- and deferring captures the body as raw text that no one` |
|       - | 4593 | ``		 * ever parses, which is how a file with `$x = ;` inside a class extending`` |
|       - | 4594 | `		 * an autoloaded base linted CLEAN. Nothing autoloads under -l either, so` |
|       - | 4595 | `		 * this is the common case rather than the rare one. php's own lint parses` |
|       - | 4596 | `		 * every body and binds nothing. */` |
|      72 | 4597 | `		return 0;` |
|       - | 4598 | `	}` |
|    5599 | 4599 | `	SySetInit(&aMissing,&pGen->pVm->sAllocator,sizeof(VmDeferredReq));` |
|    5599 | 4600 | `	SyBlobInit(&sSelfFqn,&pGen->pVm->sAllocator);` |
|    5594 | 4601 | `	if( GenStateScanDeferDeps(pGen,0,iSelfKind,&aMissing,&pBody,&pBodyEnd,&sSelfFqn,bCond) == SXRET_OK` |
|    5598 | 4602 | `	 && (SySetUsed(&aMissing) > 0 \|\| bCond) ){` |
|       - | 4603 | `		/* Two reasons to compile this declaration where it RUNS rather than here.` |
|       - | 4604 | `		 * The first is a missing dependency (the autoloader that resolves it has` |
|       - | 4605 | `		 * not been registered yet). The second is php's binding rule: a class` |
|       - | 4606 | ``		 * written inside an `if`, a loop or a function body is declared when`` |
|       - | 4607 | ``		 * execution reaches it, so `if (!class_exists('DateTime')) { class`` |
|       - | 4608 | ``		 * DateTime {} }` -- how symfony/polyfill-php8x ships its back-ports -- must`` |
|       - | 4609 | `		 * not REPLACE the engine's own class in a tree that has one. */` |
|     151 | 4610 | `		*pRc = GenStateEmitDeferredClass(pGen,iFlags,0,&aMissing,pBodyEnd,&sSelfFqn,0);` |
|     151 | 4611 | `		bDefer = 1;` |
|      73 | 4612 | `	}` |
|    5599 | 4613 | `	SySetRelease(&aMissing);` |
|    5599 | 4614 | `	SyBlobRelease(&sSelfFqn);` |
|    5599 | 4615 | `	return bDefer;` |
|    2838 | 4616 | `}` |
|       - | 4617 | `/*` |
|       - | 4618 | ``  * Apply a declaration body's collected `use Trait[, Trait] [{ resolution }]` `` |
|       - | 4619 | ` * entries to pClass — plain application when no resolution block is present,` |
|       - | 4620 | ` * otherwise the two-pass insteadof/as machinery. Shared by the CLASS body and` |
|       - | 4621 | ` * (since the adaptation-block port) the TRAIT body compiler. Returns the last` |
|       - | 4622 | ` * application status (non-OK = out of memory at a copy site).` |
|       - | 4623 | ` */` |
|    5232 | 4624 | `static sxi32 GenStateApplyTraitUses(ph7_gen_state *pGen,ph7_class *pClass,SySet *pUseEntries)` |
|       5 | 4625 | `{` |
|    5237 | 4626 | `	sxi32 rc = SXRET_OK;` |
|       - | 4627 | `	{` |
|       - | 4628 | `		TraitUseEntry *apUse;` |
|       - | 4629 | `		sxu32 nU;` |
|    5237 | 4630 | `		apUse = (TraitUseEntry *)SySetBasePtr(pUseEntries);` |
|    5555 | 4631 | `		for( nU = 0 ; nU < SySetUsed(pUseEntries) ; nU++ ){` |
|     323 | 4632 | `			TraitUseEntry *pUse = &apUse[nU];` |
|     323 | 4633 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|     323 | 4634 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|     323 | 4635 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - | 4636 | `			sxu32 nT;` |
|     323 | 4637 | `			if( !hasResolution ){` |
|       - | 4638 | `				/* No conflict resolution block: use standard trait application */` |
|     585 | 4639 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|     311 | 4640 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|     311 | 4641 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 4642 | `						break;` |
|       - | 4643 | `					}` |
|     158 | 4644 | `				}` |
|     142 | 4645 | `			}else{` |
|       - | 4646 | `				/* With resolution block: copy attributes, record traits,` |
|       - | 4647 | `				 * then use the block to resolve method conflicts.` |
|       - | 4648 | `				 */` |
|       - | 4649 | `				SyToken *pR;` |
|     105 | 4650 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      61 | 4651 | `					ph7_class *pTR = apTrait[nT];` |
|       - | 4652 | `					ph7_class_attr *pAR;` |
|       - | 4653 | `					SyHashEntry *pER;` |
|       - | 4654 | `					SyString *pNR;` |
|      61 | 4655 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      93 | 4656 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|       5 | 4657 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|       5 | 4658 | `						pNR = &pAR->sName;` |
|       5 | 4659 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|       5 | 4660 | `							SyHashInsertTail(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|       5 | 4661 | `							PH7_ClassNotePrivateName(pClass,pAR);` |
|       2 | 4662 | `						}` |
|       1 | 4663 | `					}` |
|       - | 4664 | `					/* Trait constants (PHP 8.2) live in the separate hConst namespace */` |
|      61 | 4665 | `					SyHashResetLoopCursor(&pTR->hConst);` |
|      89 | 4666 | `					while((pER = SyHashGetNextEntry(&pTR->hConst)) != 0 ){` |
|     ! 0 | 4667 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 | 4668 | `						pNR = &pAR->sName;` |
|     ! 0 | 4669 | `						if( SyHashGet(&pClass->hConst,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 | 4670 | `							SyHashInsertTail(&pClass->hConst,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 | 4671 | `						}` |
|     ! 0 | 4672 | `					}` |
|      61 | 4673 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|      33 | 4674 | `				}` |
|       - | 4675 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|      49 | 4676 | `				pR = pUse->pResolvStart;` |
|     121 | 4677 | `				while( pR < pUse->pResolvEnd ){` |
|       - | 4678 | `					SyString sTrait,sMethod;` |
|       - | 4679 | `					ph7_class *pSrcTrait;` |
|       - | 4680 | `					ph7_class_method *pMeth;` |
|       - | 4681 | `					sxi32 nRKwrd;` |
|     193 | 4682 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|     121 | 4683 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      77 | 4684 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      77 | 4685 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      77 | 4686 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      77 | 4687 | `					sMethod = pR->sData;` |
|      77 | 4688 | `					pR++;` |
|      77 | 4689 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|      31 | 4690 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|      31 | 4691 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|      31 | 4692 | `							sTrait = sMethod;` |
|      31 | 4693 | `							pR++;` |
|      31 | 4694 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|      31 | 4695 | `							sMethod = pR->sData;` |
|      31 | 4696 | `							pR++;` |
|      14 | 4697 | `						}` |
|      14 | 4698 | `					}` |
|      77 | 4699 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 4700 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 | 4701 | `						continue;` |
|       - | 4702 | `					}` |
|      77 | 4703 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      77 | 4704 | `					pR++;` |
|      77 | 4705 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|      17 | 4706 | `						pSrcTrait = 0;` |
|      21 | 4707 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      21 | 4708 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      30 | 4709 | `							if( pTN->nByte >= sTrait.nByte &&` |
|      18 | 4710 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|      17 | 4711 | `								pSrcTrait = apTrait[nT];` |
|      17 | 4712 | `								break;` |
|       - | 4713 | `							}` |
|       4 | 4714 | `						}` |
|      17 | 4715 | `						if( pSrcTrait ){` |
|      17 | 4716 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|      17 | 4717 | `							if( pMeth ){` |
|      17 | 4718 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|      17 | 4719 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|      17 | 4720 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       7 | 4721 | `								}` |
|       7 | 4722 | `							}` |
|       7 | 4723 | `						}` |
|       7 | 4724 | `					}` |
|     169 | 4725 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       5 | 4726 | `				}` |
|       - | 4727 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|     105 | 4728 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - | 4729 | `					ph7_class_method *pMR;` |
|       - | 4730 | `					SyHashEntry *pER;` |
|       - | 4731 | `					SyString *pNR;` |
|      61 | 4732 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|     187 | 4733 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|     103 | 4734 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|     103 | 4735 | `						pNR = &pMR->sFunc.sName;` |
|     103 | 4736 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      75 | 4737 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|      35 | 4738 | `						}` |
|       5 | 4739 | `					}` |
|      33 | 4740 | `				}` |
|       - | 4741 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|      49 | 4742 | `				pR = pUse->pResolvStart;` |
|     121 | 4743 | `				while( pR < pUse->pResolvEnd ){` |
|       - | 4744 | `					SyString sTrait,sMethod,sAlias;` |
|       - | 4745 | `					ph7_class *pSrcTrait;` |
|       - | 4746 | `					ph7_class_method *pMeth;` |
|     121 | 4747 | `					int hasQual = 0;` |
|       - | 4748 | `					sxi32 nRKwrd;` |
|     193 | 4749 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|     121 | 4750 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      77 | 4751 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      77 | 4752 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      77 | 4753 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      77 | 4754 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      77 | 4755 | `					sMethod = pR->sData;` |
|      77 | 4756 | `					pR++;` |
|      77 | 4757 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|      31 | 4758 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|      31 | 4759 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|      31 | 4760 | `							sTrait = sMethod;` |
|      31 | 4761 | `							hasQual = 1;` |
|      31 | 4762 | `							pR++;` |
|      31 | 4763 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|      31 | 4764 | `							sMethod = pR->sData;` |
|      31 | 4765 | `							pR++;` |
|      14 | 4766 | `						}` |
|      14 | 4767 | `					}` |
|      77 | 4768 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 4769 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 | 4770 | `						continue;` |
|       - | 4771 | `					}` |
|      77 | 4772 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      77 | 4773 | `					pR++;` |
|      77 | 4774 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|      63 | 4775 | `						sxi32 iNewVis = -1;` |
|      63 | 4776 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|      26 | 4777 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|      26 | 4778 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|      26 | 4779 | `								iNewVis = nAK;` |
|      26 | 4780 | `								pR++;` |
|      11 | 4781 | `							}` |
|      11 | 4782 | `						}` |
|      63 | 4783 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|      60 | 4784 | `							sAlias = pR->sData;` |
|      60 | 4785 | `							pR++;` |
|      28 | 4786 | `						}` |
|      63 | 4787 | `						pMeth = 0;` |
|      63 | 4788 | `						if( hasQual ){` |
|      17 | 4789 | `							pSrcTrait = 0;` |
|      27 | 4790 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      27 | 4791 | `								SyString *pTN = &apTrait[nT]->sName;` |
|      39 | 4792 | `								if( pTN->nByte >= sTrait.nByte &&` |
|      24 | 4793 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|      17 | 4794 | `									pSrcTrait = apTrait[nT];` |
|      17 | 4795 | `									break;` |
|       - | 4796 | `								}` |
|       8 | 4797 | `							}` |
|      17 | 4798 | `							if( pSrcTrait ){` |
|      17 | 4799 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       7 | 4800 | `							}` |
|      10 | 4801 | `						}else{` |
|      49 | 4802 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - | 4803 | `						}` |
|      63 | 4804 | `						if( pMeth ){` |
|       - | 4805 | `							/* php: a method declared in the class BODY wins over a trait alias` |
|       - | 4806 | ``							 * of the same name (e.g. an explicit __construct over `init as`` |
|       - | 4807 | ``							 * __construct`). If pClass already declares sAlias ITSELF — an own`` |
|       - | 4808 | `							 * method, sFunc.pUserData == pClass — keep it: SyHashInsert is LIFO,` |
|       - | 4809 | `							 * so an unconditional insert would shadow the class method at lookup` |
|       - | 4810 | ``							 * and `new` would run the alias. A name held only by another trait is`` |
|       - | 4811 | `							 * a genuine conflict resolved by the insteadof pass above. */` |
|      63 | 4812 | `							int bClassWins = 0;` |
|      63 | 4813 | `							if( sAlias.nByte > 0 ){` |
|      60 | 4814 | `								ph7_class_method *pOwn = PH7_ClassExtractMethod(pClass,sAlias.zString,sAlias.nByte);` |
|      60 | 4815 | `								bClassWins = (pOwn && pOwn->sFunc.pUserData == pClass);` |
|      28 | 4816 | `							}` |
|      88 | 4817 | `							if( sAlias.nByte > 0 && !bClassWins ){` |
|       - | 4818 | `								/* Create a shallow copy of the method struct for the alias` |
|       - | 4819 | `								 * so it can carry its own visibility without affecting the original.` |
|       - | 4820 | `								 */` |
|       - | 4821 | `								ph7_class_method *pAlias;` |
|       - | 4822 | `								char *zAliasDup;` |
|      54 | 4823 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|      54 | 4824 | `								if( pAlias ){` |
|      54 | 4825 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|      54 | 4826 | `									if( iNewVis >= 0 ){` |
|      23 | 4827 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|      19 | 4828 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|       7 | 4829 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|      10 | 4830 | `									}` |
|      54 | 4831 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      54 | 4832 | `									if( zAliasDup ){` |
|      54 | 4833 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|      25 | 4834 | `									}` |
|      29 | 4835 | `								}` |
|      35 | 4836 | `							}else if( sAlias.nByte == 0 && iNewVis >= 0 ){` |
|       - | 4837 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - | 4838 | `								ph7_class_method *pCopy;` |
|       3 | 4839 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 | 4840 | `								if( pCopy ){` |
|       3 | 4841 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 | 4842 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 | 4843 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 | 4844 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 | 4845 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - | 4846 | `									/* Replace the method in the class hash */` |
|       3 | 4847 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 | 4848 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 | 4849 | `								}` |
|       1 | 4850 | `							}` |
|      29 | 4851 | `						}` |
|      29 | 4852 | `						SXUNUSED(hasQual);` |
|      29 | 4853 | `					}` |
|      91 | 4854 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       5 | 4855 | `				}` |
|       - | 4856 | `			}` |
|     323 | 4857 | `			SySetRelease(&pUse->aTraits);` |
|     164 | 4858 | `		}` |
|       - | 4859 | `	}` |
|    5237 | 4860 | `	return rc;` |
|       5 | 4861 | `}` |
|       - | 4862 | `/*` |
|       - | 4863 | ` * Compile a class declaration, named or anonymous.` |
|       - | 4864 | ` *` |
|       - | 4865 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|       - | 4866 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|       - | 4867 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|       - | 4868 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|       - | 4869 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|       - | 4870 | ` * implements, body, install) is shared by both paths.` |
|       - | 4871 | ` */` |
|    5136 | 4872 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|       - | 4873 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|       5 | 4874 | `{` |
|    5141 | 4875 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4876 | `	ph7_class *pClass,*pBase;` |
|    5141 | 4877 | `	ph7_class *pSavedCurClass = pGen->pCurClass; /* restored at 'done' (enclosing class for a nested anon) */` |
|    5141 | 4878 | `	ph7_class *pSavedCurBase = pGen->pCurBase;   /* ...and its base, see pCurBase */` |
|       - | 4879 | `	SyToken *pEnd,*pTmp;` |
|       - | 4880 | `	SySet aInterfaces;` |
|       - | 4881 | `	SySet aUseEntries;` |
|       - | 4882 | `	SySet aOvMeth,aOvProp;   /* the #[\Override] claims this class DECLARED */` |
|    5141 | 4883 | `	sxu32 nErrEntry = pGen->nErr; /* errors already reported when this class started */` |
|       - | 4884 | `	SyString *pName;` |
|       - | 4885 | `	sxi32 nKwrd;` |
|       - | 4886 | `	sxi32 rc;` |
|    5141 | 4887 | `	if( pAnonName == 0 ){` |
|       - | 4888 | `		/* Deferral gate: an unresolvable parent/interface/trait —` |
|       - | 4889 | `		 * its autoloader has not RUN yet — re-compiles this declaration at its` |
|       - | 4890 | `		 * execution point instead of dying on "Nonexistent base class". */` |
|       - | 4891 | `		sxi32 rcDefer;` |
|    5007 | 4892 | `		if( GenStateMaybeDeferClass(pGen,iFlags,PH7_DEFER_KIND_CLASS,&rcDefer) ){` |
|     123 | 4893 | `			return rcDefer == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - | 4894 | `		}` |
|    2442 | 4895 | `	}` |
|       - | 4896 | `	/* Jump the 'class' keyword */` |
|    5023 | 4897 | `	pGen->pIn++;` |
|    5023 | 4898 | `	if( pAnonName ){` |
|       - | 4899 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|       - | 4900 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|       - | 4901 | `		 * then use the synthesized name. */` |
|     138 | 4902 | `		*ppArgStart = *ppArgEnd = 0;` |
|     138 | 4903 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|      69 | 4904 | `			pGen->pIn++; /* Jump '(' */` |
|      69 | 4905 | `			*ppArgStart = pGen->pIn;` |
|     102 | 4906 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|      33 | 4907 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|      69 | 4908 | `			pGen->pIn = *ppArgEnd;` |
|      69 | 4909 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|      33 | 4910 | `		}` |
|     138 | 4911 | `		pName = pAnonName;` |
|     138 | 4912 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|      71 | 4913 | `	}else{` |
|    4889 | 4914 | `		if( pGen->pIn >= pGen->pEnd \|\| !PH7_IsClassNameToken(pGen->pIn) ){` |
|       - | 4915 | `			/* Syntax error */` |
|     ! 0 | 4916 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 | 4917 | `			if( rc == SXERR_ABORT ){` |
|       - | 4918 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 4919 | `				return SXERR_ABORT;` |
|       - | 4920 | `			}` |
|       - | 4921 | `			/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 | 4922 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 | 4923 | `				pGen->pIn++;` |
|     ! 0 | 4924 | `			}` |
|     ! 0 | 4925 | `			return SXRET_OK;` |
|       - | 4926 | `		}` |
|       - | 4927 | `		/* Extract class name */` |
|    4889 | 4928 | `		pName = &pGen->pIn->sData;` |
|    4889 | 4929 | `		if( PH7_IsReservedClassName(pName) ){` |
|       - | 4930 | `			/* php's compiler-side screen (zend_is_reserved_class_name): the scanner` |
|       - | 4931 | `			 * hands these over as identifiers and the compiler refuses them, quoting` |
|       - | 4932 | ``			 * the name as WRITTEN. `class Null {}` and `class void {}` used to`` |
|       - | 4933 | `			 * compile here and are fatals in php. */` |
|       4 | 4934 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 | 4935 | `				"Cannot use \"%z\" as a class name as it is reserved",pName);` |
|       3 | 4936 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4937 | `				return SXERR_ABORT;` |
|       - | 4938 | `			}` |
|       4 | 4939 | `			while( pGen->pIn < pGen->pEnd` |
|       5 | 4940 | `			    && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|       3 | 4941 | `				pGen->pIn++;` |
|       1 | 4942 | `			}` |
|       3 | 4943 | `			return SXRET_OK;` |
|       - | 4944 | `		}` |
|       - | 4945 | `		/* Advance the stream cursor */` |
|    4887 | 4946 | `		pGen->pIn++;` |
|       - | 4947 | `		/* Build FQN and obtain a raw class */ {` |
|       - | 4948 | `			SyBlob sFQN;` |
|       - | 4949 | `			SyString sFQNStr;` |
|    4887 | 4950 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    4887 | 4951 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|    4887 | 4952 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|       - | 4953 | ``			/* php refuses a declaration whose short name a local `use` already took. */`` |
|    4887 | 4954 | `			if( GenStateGuardImportRedeclare(pGen,0,pName,&sFQNStr,nLine) == SXERR_ABORT ){` |
|     ! 0 | 4955 | `				SyBlobRelease(&sFQN);` |
|     ! 0 | 4956 | `				return SXERR_ABORT;` |
|       - | 4957 | `			}` |
|    4887 | 4958 | `			GenStateRecordDeclaredName(pGen,0,&sFQNStr);` |
|    4887 | 4959 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    4887 | 4960 | `			SyBlobRelease(&sFQN);` |
|       - | 4961 | `		}` |
|       - | 4962 | `	}` |
|    5021 | 4963 | `	if( pClass == 0 ){` |
|     ! 0 | 4964 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4965 | `		return SXERR_ABORT;` |
|       - | 4966 | `	}` |
|    5016 | 4967 | `	if( (iFlags & PH7_CLASS_ENUM) && pGen->pIn < pGen->pEnd` |
|     139 | 4968 | `		&& (pGen->pIn->nType & PH7_TK_COLON /* ':' */) ){` |
|       - | 4969 | ``		/* Backed enum: `enum Name: int\|string` (PHP 8.1) */`` |
|      73 | 4970 | `		pGen->pIn++; /* Jump ':' */` |
|      68 | 4971 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      73 | 4972 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_INT ){` |
|      27 | 4973 | `			pClass->nEnumBacking = MEMOBJ_INT;` |
|      27 | 4974 | `			pGen->pIn++;` |
|      59 | 4975 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      49 | 4976 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STRING ){` |
|      47 | 4977 | `			pClass->nEnumBacking = MEMOBJ_STRING;` |
|      47 | 4978 | `			pGen->pIn++;` |
|      26 | 4979 | `		}else{` |
|       3 | 4980 | `			SyToken *pTok = pGen->pIn;` |
|       3 | 4981 | `			if( pTok >= pGen->pEnd ){ pTok--; }` |
|       4 | 4982 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pTok->nLine,` |
|       1 | 4983 | `				"Enum backing type must be int or string, %z given",&pTok->sData);` |
|       3 | 4984 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4985 | `				return SXERR_ABORT;` |
|       - | 4986 | `			}` |
|       3 | 4987 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|       3 | 4988 | `				pGen->pIn++; /* Skip the bogus type token */` |
|       1 | 4989 | `			}` |
|       - | 4990 | `		}` |
|      34 | 4991 | `	}` |
|    5021 | 4992 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|    5021 | 4993 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|     ! 0 | 4994 | `		return SXERR_ABORT;` |
|       - | 4995 | `	}` |
|       - | 4996 | `	/* implemented interfaces and per-use-statement trait containers */` |
|    5021 | 4997 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|    5021 | 4998 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - | 4999 | `	/* Assume a standalone class */` |
|    5021 | 5000 | `	pBase = 0;` |
|    5021 | 5001 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|    1413 | 5002 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    1413 | 5003 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|       - | 5004 | `			SyBlob sResolved;` |
|       - | 5005 | `			SyString sBaseName;` |
|       - | 5006 | `			sxu32 nRefLine;` |
|    1043 | 5007 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|       - | 5008 | `				/* php parse-fatals here (enums have no inheritance) */` |
|     ! 0 | 5009 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5010 | `					"Enum %z cannot extend a class",&pClass->sName);` |
|     ! 0 | 5011 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5012 | `					return SXERR_ABORT;` |
|       - | 5013 | `				}` |
|     ! 0 | 5014 | `			}` |
|    1043 | 5015 | `			pGen->pIn++; /* Advance past 'extends' */` |
|    1043 | 5016 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    1043 | 5017 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    1043 | 5018 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       3 | 5019 | `				SyBlobRelease(&sResolved);` |
|       4 | 5020 | `				rc = PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|       - | 5021 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|       1 | 5022 | `					pName);` |
|       3 | 5023 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       3 | 5024 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5025 | `					return SXERR_ABORT;` |
|       - | 5026 | `				}` |
|       3 | 5027 | `				return SXRET_OK;` |
|       - | 5028 | `			}` |
|    1559 | 5029 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|    1036 | 5030 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    1041 | 5031 | `			SyStringInitFromBuf(&sBaseName,` |
|       - | 5032 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - | 5033 | `			/* Interfaces are not allowed */` |
|    1041 | 5034 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 | 5035 | `				pBase = pBase->pNextName;` |
|     ! 0 | 5036 | `			}` |
|    1041 | 5037 | `			if( pBase == 0 ){` |
|      12 | 5038 | `				if( !GenStateLintSkipsBase(pGen,pClass) ){` |
|     ! 0 | 5039 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - | 5040 | `						"Nonexistent base class '%z'",&sBaseName);` |
|     ! 0 | 5041 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5042 | `						SyBlobRelease(&sResolved);` |
|     ! 0 | 5043 | `						return SXERR_ABORT;` |
|       - | 5044 | `					}` |
|     ! 0 | 5045 | `				}` |
|       6 | 5046 | `			}else{` |
|    1029 | 5047 | `				if( pBase->iFlags & PH7_CLASS_ENUM ){` |
|       4 | 5048 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 | 5049 | `						"Class %z cannot extend enum %z",pName,&pBase->sName);` |
|       3 | 5050 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5051 | `						SyBlobRelease(&sResolved);` |
|     ! 0 | 5052 | `						return SXERR_ABORT;` |
|       - | 5053 | `					}` |
|       3 | 5054 | `					pBase = 0; /* Never inherit from an enum */` |
|    1028 | 5055 | `				}else if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|       8 | 5056 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5057 | `						/* php's wording, unquoted: "Class B cannot extend final class A". */` |
|       2 | 5058 | `						"Class %z cannot extend final class %z",pName,&pBase->sName);` |
|       6 | 5059 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5060 | `						SyBlobRelease(&sResolved);` |
|     ! 0 | 5061 | `						return SXERR_ABORT;` |
|       - | 5062 | `					}` |
|       2 | 5063 | `				}` |
|       - | 5064 | `			}` |
|    1041 | 5065 | `			SyBlobRelease(&sResolved);` |
|    1041 | 5066 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|     ! 0 | 5067 | `				pBase = 0; /* Error already reported: enums have no base class */` |
|     ! 0 | 5068 | `			}` |
|     518 | 5069 | `		}` |
|    1411 | 5070 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - | 5071 | `			ph7_class *pInterface;` |
|       - | 5072 | `			/* Interface implementation */` |
|     413 | 5073 | `			pGen->pIn++; /* Advance the stream cursor */` |
|     236 | 5074 | `			for(;;){` |
|       - | 5075 | `				SyBlob sResolved;` |
|       - | 5076 | `				SyString sIntName;` |
|       - | 5077 | `				sxu32 nRefLine;` |
|     445 | 5078 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|     445 | 5079 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     445 | 5080 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 | 5081 | `					SyBlobRelease(&sResolved);` |
|     ! 0 | 5082 | `					rc = PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|       - | 5083 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 | 5084 | `						pName);` |
|     ! 0 | 5085 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5086 | `						return SXERR_ABORT;` |
|       - | 5087 | `					}` |
|     ! 0 | 5088 | `					break;` |
|       - | 5089 | `				}` |
|     885 | 5090 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|     440 | 5091 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     445 | 5092 | `				SyStringInitFromBuf(&sIntName,` |
|       - | 5093 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - | 5094 | `				/* Only interfaces are allowed */` |
|     445 | 5095 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 5096 | `					pInterface = pInterface->pNextName;` |
|     ! 0 | 5097 | `				}` |
|     445 | 5098 | `				if( pInterface == 0 ){` |
|       2 | 5099 | `					if( !GenStateLintSkipsBase(pGen,pClass) ){` |
|     ! 0 | 5100 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - | 5101 | `							"Nonexistent base interface '%z'",&sIntName);` |
|     ! 0 | 5102 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5103 | `							SyBlobRelease(&sResolved);` |
|     ! 0 | 5104 | `							return SXERR_ABORT;` |
|       - | 5105 | `						}` |
|     ! 0 | 5106 | `					}` |
|       1 | 5107 | `				}else{` |
|       - | 5108 | `					/* Reject user classes that try to implement Throwable` |
|       - | 5109 | `					 * directly (or via an interface that extends Throwable)` |
|       - | 5110 | `					 * unless they already extend Exception or Error.` |
|       - | 5111 | `					 * Exception and Error themselves are compiled from the` |
|       - | 5112 | `					 * built-in library and are exempt by FQN — a namespaced` |
|       - | 5113 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|     443 | 5114 | `					SyString *pFqn = &pClass->sName;` |
|     443 | 5115 | `					int bIsExceptionOrError =` |
|     228 | 5116 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|     666 | 5117 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|     450 | 5118 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|      24 | 5119 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|     443 | 5120 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|      18 | 5121 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|       3 | 5122 | `						!bIsExceptionOrError ){` |
|      12 | 5123 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5124 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|       3 | 5125 | `							&pClass->sName);` |
|       9 | 5126 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5127 | `							SyBlobRelease(&sResolved);` |
|     ! 0 | 5128 | `							return SXERR_ABORT;` |
|       - | 5129 | `						}` |
|       - | 5130 | `						/* Skip registration so the follow-up abstract-method` |
|       - | 5131 | `						 * check does not produce a duplicate fatal. */` |
|       6 | 5132 | `					}else{` |
|     437 | 5133 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - | 5134 | `					}` |
|       - | 5135 | `				}` |
|     445 | 5136 | `				SyBlobRelease(&sResolved);` |
|     445 | 5137 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     209 | 5138 | `					break;` |
|       - | 5139 | `				}` |
|      37 | 5140 | `				pGen->pIn++;/* Jump the comma */` |
|       5 | 5141 | `			}` |
|     204 | 5142 | `		}` |
|     703 | 5143 | `	}` |
|    5019 | 5144 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 5145 | `		/* Syntax error */` |
|     ! 0 | 5146 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 | 5147 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5148 | `		if( rc == SXERR_ABORT ){` |
|       - | 5149 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5150 | `			return SXERR_ABORT;` |
|       - | 5151 | `		}` |
|     ! 0 | 5152 | `		return SXRET_OK;` |
|       - | 5153 | `	}` |
|    5019 | 5154 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    5019 | 5155 | `	pEnd = 0; /* cc warning */` |
|       - | 5156 | `	/* Delimit the class body */` |
|    5019 | 5157 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    5019 | 5158 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 5159 | `		/* Syntax error */` |
|     ! 0 | 5160 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 | 5161 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5162 | `		if( rc == SXERR_ABORT ){` |
|       - | 5163 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5164 | `			return SXERR_ABORT;` |
|       - | 5165 | `		}` |
|     ! 0 | 5166 | `		return SXRET_OK;` |
|       - | 5167 | `	}` |
|       - | 5168 | `	/* The delimiter token is the class body's closing brace */` |
|    5019 | 5169 | `	pClass->nEndLine = pEnd->nLine;` |
|       - | 5170 | `	/* Swap token stream */` |
|    5019 | 5171 | `	pTmp = pGen->pEnd;` |
|    5019 | 5172 | `	pGen->pEnd = pEnd;` |
|       - | 5173 | `	/* Merge the inherited flags (PH7_NewRawClass may have set INTERNAL) */` |
|    5019 | 5174 | `	pClass->iFlags \|= iFlags;` |
|    5019 | 5175 | `	if( pAnonName ){` |
|       - | 5176 | `` 		/* `new class {...}`: the name is synthesized, which is what makes `self` `` |
|       - | 5177 | `		 * inside it unusable in an intersection type (see PH7_CLASS_ANON). */` |
|     138 | 5178 | `		pClass->iFlags \|= PH7_CLASS_ANON;` |
|      67 | 5179 | `	}` |
|       - | 5180 | ``	/* ...which is what php's own attribute validators judge: `#[\Attribute]` on an`` |
|       - | 5181 | ``	 * abstract class, `#[\AllowDynamicProperties]` on a readonly one or an enum. */`` |
|    7521 | 5182 | `	if( GenStateCheckAttrPlacement(&(*pGen),&pClass->aAttrs,1,1,` |
|    7526 | 5183 | `			&pClass->sName,pClass->iFlags) == SXERR_ABORT ){` |
|     ! 0 | 5184 | `		return SXERR_ABORT;` |
|       - | 5185 | `	}` |
|       - | 5186 | `	/* This class/enum is now the lexical class for its body — see pCurClass. */` |
|    5019 | 5187 | `	pGen->pCurClass = pClass;` |
|    5019 | 5188 | `	pGen->pCurBase = pBase;` |
|       - | 5189 | `	/* Start the parse process */` |
|    6209 | 5190 | `	for(;;){` |
|       - | 5191 | `		/* Jump leading/trailing semi-colons */` |
|   16269 | 5192 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|    3403 | 5193 | `			pGen->pIn++;` |
|       5 | 5194 | `		}` |
|   12871 | 5195 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5196 | `			/* End of class body */` |
|    4921 | 5197 | `			break;` |
|       - | 5198 | `		}` |
|       - | 5199 | `		/* Bind a directly-preceding docblock to this member */` |
|    7955 | 5200 | `		GenStateSetPendingDoc(&(*pGen));` |
|    7950 | 5201 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|    3981 | 5202 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|     ! 0 | 5203 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|       - | 5204 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 5205 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 5206 | `			if( rc == SXERR_ABORT ){` |
|       - | 5207 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 5208 | `				return SXERR_ABORT;` |
|       - | 5209 | `			}` |
|     ! 0 | 5210 | `			goto done;` |
|       - | 5211 | `		}` |
|    7955 | 5212 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 5213 | `			/* Extract the current keyword */` |
|    7953 | 5214 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    7953 | 5215 | `			if( nKwrd == PH7_TKWRD_CASE && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|       - | 5216 | ``				/* Enum case declaration: `case NAME [= value];` */`` |
|     173 | 5217 | `				rc = GenStateCompileEnumCase(&(*pGen),pClass);` |
|     173 | 5218 | `				if( rc != SXRET_OK ){` |
|       9 | 5219 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5220 | `						return SXERR_ABORT;` |
|       - | 5221 | `					}` |
|       9 | 5222 | `					goto done;` |
|       - | 5223 | `				}` |
|     167 | 5224 | `				continue;` |
|       - | 5225 | `			}` |
|    7785 | 5226 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 5227 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - | 5228 | `				TraitUseEntry sUse;` |
|     291 | 5229 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|     291 | 5230 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|     291 | 5231 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|     187 | 5232 | `				for(;;){` |
|       - | 5233 | `					ph7_class *pTrait;` |
|       - | 5234 | `					SyBlob sResolved;` |
|       - | 5235 | `					SyString sTraitName;` |
|     335 | 5236 | `					sxu32 nUseLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|       - | 5237 | `					/* A trait name is a full class reference: it may be qualified or` |
|       - | 5238 | ``					 * fully-qualified (`use Foo\Bar\T;`, `use \Foo\Bar\T;`) — the generated`` |
|       - | 5239 | `					 * PHPUnit mocks name their traits absolutely. Parse it with the shared` |
|       - | 5240 | `					 * class-reference reader (handles the leading '\', every '\'-segment and` |
|       - | 5241 | `					 * namespace/import resolution) instead of a single-identifier read, which` |
|       - | 5242 | `					 * choked on the first '\'. */` |
|     335 | 5243 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     335 | 5244 | `					if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 | 5245 | `						SyBlobRelease(&sResolved);` |
|     ! 0 | 5246 | `						rc = PH7_GenCompileError(pGen,E_PARSE,nUseLine,` |
|     ! 0 | 5247 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 | 5248 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5249 | `							return SXERR_ABORT;` |
|       - | 5250 | `						}` |
|     ! 0 | 5251 | `						break;` |
|       - | 5252 | `					}` |
|     665 | 5253 | `					pTrait = PH7_VmExtractClass(pGen->pVm,` |
|     330 | 5254 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     335 | 5255 | `					SyStringInitFromBuf(&sTraitName,` |
|       - | 5256 | `						(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - | 5257 | `					/* Only traits are allowed */` |
|     335 | 5258 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 5259 | `						pTrait = pTrait->pNextName;` |
|     ! 0 | 5260 | `					}` |
|     335 | 5261 | `					if( pTrait == 0 ){` |
|       4 | 5262 | `						if( !GenStateLintSkipsBase(pGen,pClass) ){` |
|     ! 0 | 5263 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|       - | 5264 | `								"'%z' is not a trait",&sTraitName);` |
|     ! 0 | 5265 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 5266 | `								SyBlobRelease(&sResolved);` |
|     ! 0 | 5267 | `								return SXERR_ABORT;` |
|       - | 5268 | `							}` |
|     ! 0 | 5269 | `						}` |
|       2 | 5270 | `					}else{` |
|     331 | 5271 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - | 5272 | `					}` |
|     335 | 5273 | `					SyBlobRelease(&sResolved);` |
|       - | 5274 | `					/* GenStateParseClassReference already advanced past the whole name —` |
|       - | 5275 | `					 * continue only across a comma-separated trait list. */` |
|     335 | 5276 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     148 | 5277 | `						break;` |
|       - | 5278 | `					}` |
|      48 | 5279 | `					pGen->pIn++; /* Jump the comma */` |
|       4 | 5280 | `				}` |
|       - | 5281 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|     291 | 5282 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - | 5283 | `					SyToken *pBlock;` |
|      41 | 5284 | `					pGen->pIn++; /* Jump '{' */` |
|      41 | 5285 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|      41 | 5286 | `					sUse.pResolvStart = pGen->pIn;` |
|      41 | 5287 | `					sUse.pResolvEnd = pBlock;` |
|      41 | 5288 | `					if( pBlock < pGen->pEnd ){` |
|      41 | 5289 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|      23 | 5290 | `					}else{` |
|     ! 0 | 5291 | `						pGen->pIn = pGen->pEnd;` |
|       - | 5292 | `					}` |
|      18 | 5293 | `				}` |
|     291 | 5294 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - | 5295 | `				/* The semicolon will be consumed by the outer loop */` |
|     291 | 5296 | `				continue;` |
|       - | 5297 | `			}` |
|    3747 | 5298 | `		}` |
|       - | 5299 | `		/* Everything else is a MEMBER: its modifier run and the declaration it` |
|       - | 5300 | `		 * modifies. */` |
|    7501 | 5301 | `		rc = GenStateCompileMember(&(*pGen),pClass,"class");` |
|    7501 | 5302 | `		if( rc != SXRET_OK ){` |
|      96 | 5303 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5304 | `				return SXERR_ABORT;` |
|       - | 5305 | `			}` |
|      96 | 5306 | `			goto done;` |
|       - | 5307 | `		}` |
|       5 | 5308 | `	}` |
|       - | 5309 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - | 5310 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - | 5311 | `	 */` |
|    4921 | 5312 | `	rc = GenStateApplyTraitUses(&(*pGen),pClass,&aUseEntries);` |
|    4921 | 5313 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 5314 | `		SySetRelease(&aUseEntries);` |
|     ! 0 | 5315 | `		SySetRelease(&aInterfaces);` |
|     ! 0 | 5316 | `		return SXERR_ABORT;` |
|       - | 5317 | `	}` |
|    4921 | 5318 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|       - | 5319 | `		/* Enum validation + name/value props + cases()/from()/tryFrom() synthesis.` |
|       - | 5320 | `		 * Runs after trait application so trait-imported properties are caught. */` |
|     133 | 5321 | `		rc = GenStateEnumFinalize(&(*pGen),pClass,nLine);` |
|     133 | 5322 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5323 | `			SySetRelease(&aUseEntries);` |
|     ! 0 | 5324 | `			SySetRelease(&aInterfaces);` |
|     ! 0 | 5325 | `			return SXERR_ABORT;` |
|       - | 5326 | `		}` |
|      64 | 5327 | `	}` |
|       - | 5328 | `	/* The members this class DECLARES that claim #[\Override] -- recorded here,` |
|       - | 5329 | `	 * before inheritance copies the base's records in beside them, and verified` |
|       - | 5330 | `	 * once the answer exists. */` |
|    4921 | 5331 | `	GenStateCollectOverrides(&(*pGen),pClass,&aOvMeth,&aOvProp);` |
|       - | 5332 | `	/* Reject a php-fatal redeclaration before hoisting the class. An ENUM is` |
|       - | 5333 | `	 * never early-bound either: php gives every one of them UnitEnum. */` |
|    7174 | 5334 | `	if( GenStateGuardClassRedeclaration(pGen,pClass,` |
|    4916 | 5335 | `			SySetUsed(&aInterfaces) == 0 && SySetUsed(&aUseEntries) == 0` |
|    4388 | 5336 | `			&& (pClass->iFlags & PH7_CLASS_ENUM) == 0` |
|    4528 | 5337 | `			&& SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) == 0)` |
|    2463 | 5338 | `		== SXERR_ABORT ){` |
|      15 | 5339 | `		SySetRelease(&aOvMeth);` |
|      15 | 5340 | `		SySetRelease(&aOvProp);` |
|      15 | 5341 | `		return SXERR_ABORT;` |
|       - | 5342 | `	}` |
|       - | 5343 | `	/* Install the class */` |
|    4909 | 5344 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    4909 | 5345 | `	if( rc == SXRET_OK ){` |
|       - | 5346 | `		ph7_class **apInterface;` |
|       - | 5347 | `		sxu32 n;` |
|    4909 | 5348 | `		if( pBase ){` |
|       - | 5349 | `			/* Inherit from base class and mark as a subclass */` |
|    1027 | 5350 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|     511 | 5351 | `		}` |
|    4909 | 5352 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|    5341 | 5353 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - | 5354 | `			/* Implements one or more interface */` |
|     437 | 5355 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|     437 | 5356 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5357 | `				break;` |
|       - | 5358 | `			}` |
|     221 | 5359 | `		}` |
|       - | 5360 | `		/* Auto-implement UnitEnum (and BackedEnum for backed enums) — php 8.1:` |
|       - | 5361 | ``		 * every enum satisfies `instanceof UnitEnum` implicitly. */`` |
|    4909 | 5362 | `		if( rc == SXRET_OK && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|     131 | 5363 | `			ph7_class *pIntf = PH7_VmExtractClass(pGen->pVm,"UnitEnum",sizeof("UnitEnum")-1,FALSE,0);` |
|     131 | 5364 | `			while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 5365 | `				pIntf = pIntf->pNextName;` |
|     ! 0 | 5366 | `			}` |
|     131 | 5367 | `			if( pIntf ){` |
|     131 | 5368 | `				PH7_ClassImplement(pClass,pIntf);` |
|      63 | 5369 | `			}` |
|     131 | 5370 | `			if( pClass->nEnumBacking != 0 ){` |
|      69 | 5371 | `				pIntf = PH7_VmExtractClass(pGen->pVm,"BackedEnum",sizeof("BackedEnum")-1,FALSE,0);` |
|      69 | 5372 | `				while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 5373 | `					pIntf = pIntf->pNextName;` |
|     ! 0 | 5374 | `				}` |
|      69 | 5375 | `				if( pIntf ){` |
|      69 | 5376 | `					PH7_ClassImplement(pClass,pIntf);` |
|      32 | 5377 | `				}` |
|      32 | 5378 | `			}` |
|      63 | 5379 | `		}` |
|       - | 5380 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|       - | 5381 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|    4904 | 5382 | `		if( rc == SXRET_OK` |
|    4904 | 5383 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|    4909 | 5384 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|     311 | 5385 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|       - | 5386 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|     311 | 5387 | `			if( pStringable ){` |
|     311 | 5388 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|     311 | 5389 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|       - | 5390 | `				sxu32 i;` |
|     311 | 5391 | `				int bAlready = 0;` |
|     317 | 5392 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|      10 | 5393 | `					if( apImpl[i] == pStringable ){` |
|       3 | 5394 | `						bAlready = 1;` |
|       3 | 5395 | `						break;` |
|       - | 5396 | `					}` |
|       4 | 5397 | `				}` |
|     311 | 5398 | `				if( !bAlready ){` |
|     309 | 5399 | `					PH7_ClassImplement(pClass,pStringable);` |
|     152 | 5400 | `				}` |
|     153 | 5401 | `			}` |
|     153 | 5402 | `		}` |
|       - | 5403 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|    4909 | 5404 | `		if( rc == SXRET_OK ){` |
|    4909 | 5405 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|    4909 | 5406 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 5407 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 5408 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 5409 | `				SySetRelease(&aOvMeth);` |
|     ! 0 | 5410 | `				SySetRelease(&aOvProp);` |
|     ! 0 | 5411 | `				return SXERR_ABORT;` |
|       - | 5412 | `			}` |
|    2452 | 5413 | `		}` |
|       - | 5414 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|    4909 | 5415 | `		if( rc == SXRET_OK ){` |
|    4909 | 5416 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|    4909 | 5417 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 5418 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 5419 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 5420 | `				SySetRelease(&aOvMeth);` |
|     ! 0 | 5421 | `				SySetRelease(&aOvProp);` |
|     ! 0 | 5422 | `				return SXERR_ABORT;` |
|       - | 5423 | `			}` |
|    2452 | 5424 | `		}` |
|       - | 5425 | `		/* ...and the #[\Override] claims LAST: php reports an unimplemented` |
|       - | 5426 | `		 * abstract method and an inheritance visibility clash before this one,` |
|       - | 5427 | `		 * and stops there — php's E_COMPILE_ERROR does not return, so a` |
|       - | 5428 | `		 * declaration that already failed says nothing more. */` |
|    4909 | 5429 | `		if( rc == SXRET_OK && pGen->nErr == nErrEntry ){` |
|    4671 | 5430 | `			sxi32 rcCheck = GenStateCheckOverrides(&(*pGen),pClass,&aOvMeth,&aOvProp);` |
|    4671 | 5431 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 5432 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 5433 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 5434 | `				SySetRelease(&aOvMeth);` |
|     ! 0 | 5435 | `				SySetRelease(&aOvProp);` |
|     ! 0 | 5436 | `				return SXERR_ABORT;` |
|       - | 5437 | `			}` |
|    2333 | 5438 | `		}` |
|    2452 | 5439 | `	}` |
|    4909 | 5440 | `	SySetRelease(&aUseEntries);` |
|    4909 | 5441 | `	SySetRelease(&aInterfaces);` |
|    4909 | 5442 | `	SySetRelease(&aOvMeth);` |
|    4909 | 5443 | `	SySetRelease(&aOvProp);` |
|    4909 | 5444 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5445 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5446 | `		return SXERR_ABORT;` |
|       - | 5447 | `	}` |
|    2452 | 5448 | `done:` |
|    5007 | 5449 | `	pGen->pCurClass = pSavedCurClass;` |
|    5007 | 5450 | `	pGen->pCurBase = pSavedCurBase;` |
|       - | 5451 | `	/* Point beyond the class body */` |
|    5007 | 5452 | `	pGen->pIn = &pEnd[1];` |
|    5007 | 5453 | `	pGen->pEnd = pTmp;` |
|    5007 | 5454 | `	return PH7_OK;` |
|    2573 | 5455 | `}` |
|       - | 5456 | `/* Compile a named class declaration (the common case). */` |
|    5002 | 5457 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       5 | 5458 | `{` |
|    5007 | 5459 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|       5 | 5460 | `}` |
|       - | 5461 | `/*` |
|       - | 5462 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|       - | 5463 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|       - | 5464 | ` * compile + install the class body once (at compile time, like every other` |
|       - | 5465 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|       - | 5466 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|       - | 5467 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|       - | 5468 | ` */` |
|     142 | 5469 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 | 5470 | `{` |
|       - | 5471 | `	char zName[128];         /* Synthesized class name */` |
|       - | 5472 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|       - | 5473 | `	SyString sName;` |
|       - | 5474 | `	SyToken *pArgStart,*pArgEnd;` |
|       - | 5475 | `	SyToken *pTokKw;` |
|     146 | 5476 | `	sxi32 iAnonFlags = 0;` |
|       - | 5477 | `	ph7_value *pObj;` |
|     146 | 5478 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5479 | `	sxu32 nIdx,nLen;` |
|       - | 5480 | `	sxi32 nArg,rc;` |
|      71 | 5481 | `	SXUNUSED(iCompileFlag);` |
|     146 | 5482 | `	if( GenStateIsReadonly(pGen->pIn) && &pGen->pIn[1] < pGen->pEnd ){` |
|       - | 5483 | ``		/* `new readonly class …` (PHP 8.3). Step over the modifier so everything`` |
|       - | 5484 | ``		 * below sees the cursor on `class`, where it has always been, and carry`` |
|       - | 5485 | `		 * the flag into the class body — which is what makes every property` |
|       - | 5486 | `		 * readonly and refuses a non-readonly base. */` |
|       5 | 5487 | `		iAnonFlags = PH7_CLASS_READONLY;` |
|       5 | 5488 | `		pGen->pIn++;` |
|       2 | 5489 | `	}` |
|     146 | 5490 | ``	pTokKw = pGen->pIn; /* Attribute-sidecar key: `new #[A] class` trivia`` |
|       - | 5491 | `	                     * is keyed to this 'class' token */` |
|     146 | 5492 | `	if( pGen->pVm->sDeferAnonName.nByte > 0 ){` |
|       - | 5493 | `		/* Deferred re-compile (VmExecDeferredClass): install under the SAME` |
|       - | 5494 | `		 * synthesized name the original site's OP_NEW loads. One-shot. */` |
|       8 | 5495 | `		sName = pGen->pVm->sDeferAnonName;` |
|       8 | 5496 | `		nLen = sName.nByte;` |
|       8 | 5497 | `		pGen->pVm->sDeferAnonName.zString = 0;` |
|       8 | 5498 | `		pGen->pVm->sDeferAnonName.nByte = 0;` |
|       5 | 5499 | `	}else{` |
|       - | 5500 | `		/* Generate a unique anonymous-class name (collision-checked) */` |
|     140 | 5501 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|     140 | 5502 | `		while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 | 5503 | `			nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|     ! 0 | 5504 | `		}` |
|     140 | 5505 | `		SyStringInitFromBuf(&sName,zName,nLen);` |
|       - | 5506 | `	}` |
|       - | 5507 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|       - | 5508 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|       - | 5509 | `	 * delimited construct; GenStateCompileClassEx restores both on success.` |
|       - | 5510 | ``	 * Deferral gate: `new class extends \App\Child {}` where the`` |
|       - | 5511 | `	 * parent's autoloader has not RUN yet — capture the class for a runtime` |
|       - | 5512 | `	 * re-compile and keep only the site's argument/OP_NEW emission here. */` |
|     146 | 5513 | `	pArgStart = pArgEnd = 0;` |
|       - | 5514 | `	{` |
|       - | 5515 | `		SySet aMissing;` |
|     146 | 5516 | `		SyToken *pBody = 0;` |
|     146 | 5517 | `		SyToken *pBodyEnd = 0;` |
|       - | 5518 | `		SyBlob sSelfFqn;` |
|     146 | 5519 | `		int bDeferred = 0;` |
|     146 | 5520 | `		SySetInit(&aMissing,&pGen->pVm->sAllocator,sizeof(VmDeferredReq));` |
|     146 | 5521 | `		SyBlobInit(&sSelfFqn,&pGen->pVm->sAllocator);` |
|       - | 5522 | `		/* An anonymous class is an EXPRESSION: it is always compiled where it runs,` |
|       - | 5523 | `		 * so resolving its parent here is resolving it at its execution point --` |
|       - | 5524 | `		 * the autoload belongs. */` |
|     142 | 5525 | `		if( GenStateScanDeferDeps(pGen,1,PH7_DEFER_KIND_CLASS,&aMissing,&pBody,&pBodyEnd,&sSelfFqn,0) == SXRET_OK` |
|     146 | 5526 | `		 && SySetUsed(&aMissing) > 0 && !pGen->pVm->bSyntaxCheck ){` |
|      10 | 5527 | `			if( &pTokKw[1] < pGen->pEnd && (pTokKw[1].nType & PH7_TK_LPAREN) ){` |
|       3 | 5528 | `				SyToken *pClose = 0;` |
|       3 | 5529 | `				PH7_DelimitNestedTokens(&pTokKw[2],pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|       3 | 5530 | `				if( pClose && pClose < pGen->pEnd ){` |
|       3 | 5531 | `					pArgStart = &pTokKw[2];` |
|       3 | 5532 | `					pArgEnd = pClose;` |
|       1 | 5533 | `				}` |
|       1 | 5534 | `			}` |
|      10 | 5535 | `			rc = GenStateEmitDeferredClass(pGen,0,1,&aMissing,pBodyEnd,0,&sName);` |
|      10 | 5536 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5537 | `				SySetRelease(&aMissing);` |
|     ! 0 | 5538 | `				SyBlobRelease(&sSelfFqn);` |
|     ! 0 | 5539 | `				return SXERR_ABORT;` |
|       - | 5540 | `			}` |
|      10 | 5541 | `			bDeferred = ( rc == SXRET_OK );` |
|       4 | 5542 | `		}` |
|     146 | 5543 | `		SySetRelease(&aMissing);` |
|     146 | 5544 | `		SyBlobRelease(&sSelfFqn);` |
|     146 | 5545 | `		if( !bDeferred ){` |
|     138 | 5546 | `			rc = GenStateCompileClassEx(pGen,iAnonFlags,&sName,&pArgStart,&pArgEnd);` |
|     138 | 5547 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5548 | `				return rc;` |
|       - | 5549 | `			}` |
|       - | 5550 | `			{` |
|       - | 5551 | ``				/* Expression-position attributes (`new #[A] class {…}`) */`` |
|     138 | 5552 | `				ph7_class *pAnonClass = PH7_VmExtractClass(pGen->pVm,sName.zString,nLen,FALSE,0);` |
|     134 | 5553 | `				if( pAnonClass` |
|     138 | 5554 | `				 && GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnonClass->aAttrs) == SXERR_ABORT ){` |
|     ! 0 | 5555 | `					return SXERR_ABORT;` |
|       - | 5556 | `				}` |
|       - | 5557 | `			}` |
|      67 | 5558 | `		}` |
|       - | 5559 | `	}` |
|       - | 5560 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|       - | 5561 | `	 * with the constructor arguments beneath it, so push the args first.` |
|       - | 5562 | `	 *` |
|       - | 5563 | `	 * This argument list is compiled from RAW TOKENS rather than through the` |
|       - | 5564 | `	 * expression parser's argument machinery (the class body sits between the` |
|       - | 5565 | `	 * parentheses and the rest of the expression), so the two forms that machinery` |
|       - | 5566 | ``	 * recognizes have to be recognized here too — `...$args` and `name: $v`. They`` |
|       - | 5567 | `	 * were not: a spread was compiled as one ordinary argument, so` |
|       - | 5568 | ``	 * `new class(...$a) {}` passed the ARRAY where php passes its elements, and a`` |
|       - | 5569 | ``	 * named argument was a `Syntax error: Unexpected token ':'` on source php`` |
|       - | 5570 | `	 * compiles. */` |
|     146 | 5571 | `	nArg = 0;` |
|       - | 5572 | `	{` |
|       - | 5573 | `	SySet aArgName;              /* one SyString per argument; {0,0} == positional */` |
|     146 | 5574 | `	int hasNamed = 0, hasSpread = 0;` |
|       - | 5575 | `	void *p3;` |
|     146 | 5576 | `	SySetInit(&aArgName,&pGen->pVm->sAllocator,sizeof(SyString));` |
|     146 | 5577 | `	if( pArgStart < pArgEnd ){` |
|      69 | 5578 | `		SyToken *pSavedIn = pGen->pIn;` |
|      69 | 5579 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|       - | 5580 | `		SyToken *pArgNext;` |
|      69 | 5581 | `		const char *zOrder = 0;   /* set when this argument's POSITION or shape is refused */` |
|       - | 5582 | ``		SyString sOrderName;      /* nByte > 0: zOrder is a `'%z:'` format, E_PARSE */`` |
|      69 | 5583 | `		SyZero(&sOrderName,sizeof(sOrderName));` |
|      69 | 5584 | `		pGen->pIn = pArgStart;` |
|      69 | 5585 | `		pGen->pEnd = pArgEnd;` |
|     145 | 5586 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|      83 | 5587 | `			SyToken *pArgIn = pGen->pIn;` |
|       - | 5588 | `			SyString sArgName;` |
|      83 | 5589 | `			int bSpread = 0;` |
|      83 | 5590 | `			SyZero(&sArgName,sizeof(sArgName));` |
|      83 | 5591 | `			if( pArgIn < pArgNext && (pArgIn->nType & PH7_TK_ELLIPSIS) ){` |
|      26 | 5592 | `				bSpread = 1;` |
|      26 | 5593 | `				pArgIn++;` |
|      26 | 5594 | `				if( hasNamed ){` |
|       3 | 5595 | `					zOrder = "Cannot use argument unpacking after named arguments";` |
|       1 | 5596 | `				}` |
|      70 | 5597 | `			}else if( &pArgIn[1] < pArgNext` |
|      48 | 5598 | `			 && (pArgIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|      39 | 5599 | `			 && (pArgIn[1].nType & PH7_TK_COLON) ){` |
|       - | 5600 | ``				/* `name: value`. php accepts a reserved word as a parameter name, and`` |
|       - | 5601 | ``				 * `::` lexes as its own operator, so an ID/KEYWORD followed by a SINGLE`` |
|       - | 5602 | `				 * colon at the head of an argument can only be this. */` |
|      24 | 5603 | `				sArgName = pArgIn->sData;` |
|      24 | 5604 | `				hasNamed = 1;` |
|      24 | 5605 | `				pArgIn += 2;` |
|      24 | 5606 | `				if( pArgIn >= pArgNext ){` |
|       - | 5607 | ``					/* `new class(a:) {}` — a name with no value. Spelled exactly as the`` |
|       - | 5608 | `					 * ordinary argument path spells it (php names the token that stopped` |
|       - | 5609 | `					 * it instead; that wording gap belongs to the parse-error family and` |
|       - | 5610 | `					 * is now one gap in both places rather than silence in this one). */` |
|       3 | 5611 | `					zOrder = "syntax error, expected expression after named argument '%z:'";` |
|       3 | 5612 | `					sOrderName = sArgName;` |
|      23 | 5613 | `				}else if( pArgIn->nType & PH7_TK_ELLIPSIS ){` |
|     ! 0 | 5614 | `					zOrder = "syntax error, unexpected token \"...\"";` |
|       2 | 5615 | `				}` |
|      47 | 5616 | `			}else if( hasNamed ){` |
|     ! 0 | 5617 | `				zOrder = "Cannot use positional argument after named argument";` |
|      36 | 5618 | `			}else if( hasSpread ){` |
|     ! 0 | 5619 | `				zOrder = "Cannot use positional argument after argument unpacking";` |
|     ! 0 | 5620 | `			}` |
|      83 | 5621 | `			if( zOrder ){` |
|       - | 5622 | `				/* The same four rules the ordinary call path enforces at COMPILE time` |
|       - | 5623 | `				 * (GenStateEmitCallArgs); an anonymous class's list is parsed here and` |
|       - | 5624 | `				 * so had none of them. */` |
|       5 | 5625 | `				sxu32 nErrLine = pArgNext > pArgStart ? pArgNext[-1].nLine : nLine;` |
|       5 | 5626 | `				pGen->pIn = pSavedIn;` |
|       5 | 5627 | `				pGen->pEnd = pSavedEnd;` |
|       5 | 5628 | `				SySetRelease(&aArgName);` |
|       5 | 5629 | `				if( sOrderName.nByte > 0 ){` |
|       3 | 5630 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,nErrLine,zOrder,&sOrderName);` |
|       2 | 5631 | `				}else{` |
|       3 | 5632 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,nErrLine,"%s",zOrder);` |
|       - | 5633 | `				}` |
|       5 | 5634 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - | 5635 | `			}` |
|      79 | 5636 | `			if( pArgIn < pArgNext ){` |
|      79 | 5637 | `				rc = GenStateCompileArrayEntry(pGen,pArgIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|      79 | 5638 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5639 | `					pGen->pIn = pSavedIn;` |
|     ! 0 | 5640 | `					pGen->pEnd = pSavedEnd;` |
|     ! 0 | 5641 | `					SySetRelease(&aArgName);` |
|     ! 0 | 5642 | `					return SXERR_ABORT;` |
|       - | 5643 | `				}` |
|      79 | 5644 | `				if( bSpread ){` |
|       - | 5645 | `					/* iP1 marks a source php unpacks BY REFERENCE: only a plain` |
|       - | 5646 | ``					 * `$var`, which is exactly two tokens here. */`` |
|      28 | 5647 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_SPREAD,` |
|      27 | 5648 | `						(&pArgIn[2] == pArgNext && (pArgIn->nType & PH7_TK_DOLLAR)` |
|      10 | 5649 | `						 && (pArgIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))) ? 1 : 0,` |
|       - | 5650 | `						0,0,0);` |
|      23 | 5651 | `					hasSpread = 1;` |
|      11 | 5652 | `				}` |
|      79 | 5653 | `				SySetPut(&aArgName,(const void *)&sArgName);` |
|      79 | 5654 | `				nArg++;` |
|      38 | 5655 | `			}` |
|      79 | 5656 | `			pGen->pIn = &pArgNext[1];` |
|       3 | 5657 | `		}` |
|      64 | 5658 | `		pGen->pIn = pSavedIn;` |
|      64 | 5659 | `		pGen->pEnd = pSavedEnd;` |
|      31 | 5660 | `	}` |
|       - | 5661 | `	/* Load the synthesized class name */` |
|     142 | 5662 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     142 | 5663 | `	if( pObj == 0 ){` |
|     ! 0 | 5664 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 5665 | `		SySetRelease(&aArgName);` |
|     ! 0 | 5666 | `		return SXERR_ABORT;` |
|       - | 5667 | `	}` |
|     142 | 5668 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     142 | 5669 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 5670 | `	/* The names ride on the instruction as a VmCallArgMap, deep-copied out of the` |
|       - | 5671 | `	 * token stream (which is freed before the code runs) exactly as the ordinary` |
|       - | 5672 | `	 * call path copies them. */` |
|     142 | 5673 | `	p3 = 0;` |
|     142 | 5674 | `	if( hasNamed ){` |
|      15 | 5675 | `		SyString *aName = (SyString *)SySetBasePtr(&aArgName);` |
|      15 | 5676 | `		sxu32 n, nStrBytes = 0;` |
|      35 | 5677 | `		for( n = 0 ; n < (sxu32)nArg ; ++n ){` |
|      21 | 5678 | `			nStrBytes += aName[n].nByte;` |
|      11 | 5679 | `		}` |
|       - | 5680 | `		{` |
|      15 | 5681 | `		sxu32 mapSize = sizeof(VmCallArgMap) + (sxu32)nArg * sizeof(SyString) + nStrBytes;` |
|      15 | 5682 | `		VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,mapSize);` |
|      15 | 5683 | `		if( pMap ){` |
|       - | 5684 | `			char *zBuf;` |
|      15 | 5685 | `			SyZero(pMap,mapSize);` |
|      15 | 5686 | `			pMap->bHasNamed = 1;` |
|      15 | 5687 | `			pMap->nTotal = (sxu32)nArg;` |
|      15 | 5688 | `			pMap->aNames = (SyString *)&pMap[1];` |
|      15 | 5689 | `			zBuf = (char *)&pMap->aNames[nArg];` |
|      35 | 5690 | `			for( n = 0 ; n < (sxu32)nArg ; ++n ){` |
|      21 | 5691 | `				if( aName[n].nByte > 0 ){` |
|      19 | 5692 | `					SyMemcpy(aName[n].zString,zBuf,aName[n].nByte);` |
|      19 | 5693 | `					SyStringInitFromBuf(&pMap->aNames[n],zBuf,aName[n].nByte);` |
|      19 | 5694 | `					zBuf += aName[n].nByte;` |
|       9 | 5695 | `				}` |
|      11 | 5696 | `			}` |
|      15 | 5697 | `			p3 = (void *)pMap;` |
|       7 | 5698 | `		}` |
|       - | 5699 | `		}` |
|       7 | 5700 | `	}` |
|     142 | 5701 | `	SySetRelease(&aArgName);` |
|       - | 5702 | `	/* Instantiate: pops the name + nArg arguments, runs __construct. iP2 is the` |
|       - | 5703 | `	 * spread flag the effective-argument-map builder keys on. */` |
|     211 | 5704 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,hasSpread ? 1 : 0,` |
|      69 | 5705 | `		GenStateAttachStrictFlag(pGen,p3),0);` |
|       - | 5706 | `	}` |
|     142 | 5707 | `	return SXRET_OK;` |
|      75 | 5708 | `}` |
|       - | 5709 | `/*` |
|       - | 5710 | ` * Compile a user-defined abstract class.` |
|       - | 5711 | ` *  According to the PHP language reference manual` |
|       - | 5712 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - | 5713 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - | 5714 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - | 5715 | ` *   the method's signature - they cannot define the implementation.` |
|       - | 5716 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - | 5717 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - | 5718 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - | 5719 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - | 5720 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - | 5721 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - | 5722 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - | 5723 | ` *   could differ.` |
|       - | 5724 | ` */` |
|       - | 5725 | `/*` |
|       - | 5726 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|       - | 5727 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|       - | 5728 | ` * receives the corresponding PH7_CLASS_* bit.` |
|       - | 5729 | ` */` |
| 2057284 | 5730 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|       5 | 5731 | `{` |
| 2057289 | 5732 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
| 1218403 | 5733 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
| 1218403 | 5734 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
| 1218287 | 5735 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|  608174 | 5736 | `	}` |
| 2056917 | 5737 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
| 2056831 | 5738 | `	return FALSE;` |
| 1027034 | 5739 | `}` |
|       - | 5740 | `/*` |
|       - | 5741 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|       - | 5742 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|       - | 5743 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|       - | 5744 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|       - | 5745 | ` */` |
| 2056826 | 5746 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|       5 | 5747 | `{` |
| 2056831 | 5748 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
| 2056831 | 5749 | `	sxi32 iFlags = 0,iFlag;` |
| 2057289 | 5750 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|     463 | 5751 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|       5 | 5752 | `			pDup = pIn;` |
|       2 | 5753 | `		}` |
|     463 | 5754 | `		iFlags \|= iFlag;` |
|     463 | 5755 | `		pIn++;` |
|       5 | 5756 | `	}` |
| 2056831 | 5757 | `	*ppIn = pIn;` |
| 2056831 | 5758 | `	if( ppDup ){ *ppDup = pDup; }` |
| 2056831 | 5759 | `	return iFlags;` |
|       5 | 5760 | `}` |
|       - | 5761 | `/*` |
|       - | 5762 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|       - | 5763 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|       - | 5764 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|       - | 5765 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|       - | 5766 | `` * `readonly`) to their existing handlers.`` |
|       - | 5767 | ` */` |
| 2056618 | 5768 | `PH7_PRIVATE int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|       5 | 5769 | `{` |
| 2056623 | 5770 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
| 1026921 | 5771 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
| 2056729 | 5772 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|       5 | 5773 | `}` |
|       - | 5774 | `/*` |
|       - | 5775 | ``  * Return TRUE when the token stream starts `readonly class …` in a `new` `` |
|       - | 5776 | `` * operand — PHP 8.3's readonly ANONYMOUS class. `readonly` is the only modifier`` |
|       - | 5777 | `` * php admits there: `new final class {}` and `new abstract class {}` are parse`` |
|       - | 5778 | ` * errors, so this deliberately does NOT reuse GenStateScanClassModifiers.` |
|       - | 5779 | ` */` |
| 5520583 | 5780 | `PH7_PRIVATE int GenStateStartsReadonlyAnonClass(SyToken *pIn,SyToken *pEnd)` |
|       5 | 5781 | `{` |
| 5520588 | 5782 | `	if( !GenStateIsReadonly(pIn) \|\| &pIn[1] >= pEnd ){` |
| 5520580 | 5783 | `		return 0;` |
|       - | 5784 | `	}` |
|      10 | 5785 | `	pIn++;` |
|       8 | 5786 | `	if( (pIn->nType & PH7_TK_KEYWORD) == 0` |
|       6 | 5787 | `	 \|\| (sxu32)SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_CLASS` |
|       6 | 5788 | `	 \|\| &pIn[1] >= pEnd ){` |
|       6 | 5789 | `		return 0;` |
|       - | 5790 | `	}` |
|       - | 5791 | `` 	/* Same shape test the bare `new class` branch makes, so a stray `readonly` `` |
|       - | 5792 | ``	 * followed by the `::class` constant is left to the literal path. */`` |
|       5 | 5793 | `	pIn++;` |
|       5 | 5794 | `	return (pIn->nType & (PH7_TK_OCB\|PH7_TK_LPAREN)) != 0` |
|       4 | 5795 | `		\|\| ( (pIn->nType & PH7_TK_KEYWORD)` |
|     ! 0 | 5796 | `		  && ( (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_EXTENDS` |
|     ! 0 | 5797 | `		    \|\| (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ) );` |
| 2754926 | 5798 | `}` |
|       - | 5799 | `/*` |
|       - | 5800 | ` * Compile a class declaration carrying one or more leading modifiers` |
|       - | 5801 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|       - | 5802 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|       - | 5803 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|       - | 5804 | `` * `abstract`+`final` pair, like PHP.`` |
|       - | 5805 | ` */` |
|     208 | 5806 | `PH7_PRIVATE sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|       5 | 5807 | `{` |
|       - | 5808 | `	SyToken *pDup;` |
|     213 | 5809 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|       - | 5810 | `	sxi32 rc;` |
|     213 | 5811 | `	if( pDup ){` |
|       4 | 5812 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|       2 | 5813 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|       3 | 5814 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5815 | `			return SXERR_ABORT;` |
|       - | 5816 | `		}` |
|       1 | 5817 | `	}` |
|     208 | 5818 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|     109 | 5819 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|       6 | 5820 | `		pGen->iFatalTrace = PH7_FATAL_TRACE_NONE;` |
|       6 | 5821 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5822 | `			"Cannot use the final modifier on an abstract class");` |
|       6 | 5823 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5824 | `			return SXERR_ABORT;` |
|       - | 5825 | `		}` |
|       2 | 5826 | `	}` |
|     213 | 5827 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|     109 | 5828 | `}` |
|       - | 5829 | `/*` |
|       - | 5830 | ` * Compile a user-defined trait.` |
|       - | 5831 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - | 5832 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - | 5833 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - | 5834 | ` */` |
|     332 | 5835 | `PH7_PRIVATE sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       5 | 5836 | `{` |
|     337 | 5837 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5838 | `	ph7_class *pClass;` |
|     337 | 5839 | `	ph7_class *pSavedCurClass = pGen->pCurClass; /* restored at 'done' */` |
|     337 | 5840 | `	ph7_class *pSavedCurBase = pGen->pCurBase;   /* ...and its base, see pCurBase */` |
|       - | 5841 | `	SyToken *pEnd,*pTmp;` |
|       - | 5842 | ``	SySet aUseEntries; /* trait-body `use` statements (incl. adaptation blocks) */`` |
|       - | 5843 | `	SyString *pName;` |
|       - | 5844 | `	sxi32 nKwrd;` |
|       - | 5845 | `	sxi32 rc;` |
|       - | 5846 | `	{` |
|       - | 5847 | `		/* Deferral gate: a used trait may need an autoloader that` |
|       - | 5848 | `		 * has not run yet. */` |
|       - | 5849 | `		sxi32 rcDefer;` |
|     337 | 5850 | `		if( GenStateMaybeDeferClass(pGen,0,PH7_DEFER_KIND_TRAIT,&rcDefer) ){` |
|      19 | 5851 | `			return rcDefer == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - | 5852 | `		}` |
|       - | 5853 | `	}` |
|     321 | 5854 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - | 5855 | `	/* Jump the 'trait' keyword */` |
|     321 | 5856 | `	pGen->pIn++;` |
|     321 | 5857 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 5858 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 | 5859 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5860 | `			return SXERR_ABORT;` |
|       - | 5861 | `		}` |
|     ! 0 | 5862 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 | 5863 | `			pGen->pIn++;` |
|     ! 0 | 5864 | `		}` |
|     ! 0 | 5865 | `		return SXRET_OK;` |
|       - | 5866 | `	}` |
|       - | 5867 | `	/* Extract trait name */` |
|     321 | 5868 | `	pName = &pGen->pIn->sData;` |
|     321 | 5869 | `	pGen->pIn++;` |
|       - | 5870 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 5871 | `		SyBlob sFQN;` |
|       - | 5872 | `		SyString sFQNStr;` |
|     321 | 5873 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     321 | 5874 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|     321 | 5875 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|       - | 5876 | ``		/* php refuses a declaration whose short name a local `use` already took. */`` |
|     321 | 5877 | `		if( GenStateGuardImportRedeclare(pGen,0,pName,&sFQNStr,nLine) == SXERR_ABORT ){` |
|     ! 0 | 5878 | `			SyBlobRelease(&sFQN);` |
|     ! 0 | 5879 | `			return SXERR_ABORT;` |
|       - | 5880 | `		}` |
|     321 | 5881 | `		GenStateRecordDeclaredName(pGen,0,&sFQNStr);` |
|     321 | 5882 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|     321 | 5883 | `		SyBlobRelease(&sFQN);` |
|       - | 5884 | `	}` |
|     321 | 5885 | `	if( pClass == 0 ){` |
|     ! 0 | 5886 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5887 | `		return SXERR_ABORT;` |
|       - | 5888 | `	}` |
|     321 | 5889 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|     321 | 5890 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|     ! 0 | 5891 | `		return SXERR_ABORT;` |
|       - | 5892 | `	}` |
|       - | 5893 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|     321 | 5894 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 | 5895 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 | 5896 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5897 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5898 | `			return SXERR_ABORT;` |
|       - | 5899 | `		}` |
|     ! 0 | 5900 | `		return SXRET_OK;` |
|       - | 5901 | `	}` |
|     321 | 5902 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|     321 | 5903 | `	pEnd = 0;` |
|     321 | 5904 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|     321 | 5905 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 5906 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 | 5907 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5908 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5909 | `			return SXERR_ABORT;` |
|       - | 5910 | `		}` |
|     ! 0 | 5911 | `		return SXRET_OK;` |
|       - | 5912 | `	}` |
|       - | 5913 | `	/* The delimiter token is the trait body's closing brace */` |
|     321 | 5914 | `	pClass->nEndLine = pEnd->nLine;` |
|       - | 5915 | `	/* Swap token stream */` |
|     321 | 5916 | `	pTmp = pGen->pEnd;` |
|     321 | 5917 | `	pGen->pEnd = pEnd;` |
|       - | 5918 | `	/* Mark as trait (PH7_NewRawClass may have set INTERNAL) */` |
|     321 | 5919 | `	pClass->iFlags \|= PH7_CLASS_TRAIT;` |
|     474 | 5920 | `	if( GenStateCheckAttrPlacement(&(*pGen),&pClass->aAttrs,1,1,` |
|     479 | 5921 | `			&pClass->sName,pClass->iFlags) == SXERR_ABORT ){` |
|     ! 0 | 5922 | `		return SXERR_ABORT;` |
|       - | 5923 | `	}` |
|       - | 5924 | `	/* This trait is now the lexical class for its body, so a property/parameter` |
|       - | 5925 | `	 * default here resolves __TRAIT__ to it (see pCurClass). */` |
|     321 | 5926 | `	pGen->pCurClass = pClass;` |
|     321 | 5927 | ``	pGen->pCurBase = 0; /* a trait has no base; its `parent` is deferred to composition */`` |
|       - | 5928 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|     404 | 5929 | `	for(;;){` |
|    1011 | 5930 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     171 | 5931 | `			pGen->pIn++;` |
|       5 | 5932 | `		}` |
|     845 | 5933 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     321 | 5934 | `			break;` |
|       - | 5935 | `		}` |
|       - | 5936 | `		/* Bind a directly-preceding docblock to this member */` |
|     529 | 5937 | `		GenStateSetPendingDoc(&(*pGen));` |
|     524 | 5938 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|     267 | 5939 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|     ! 0 | 5940 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|       - | 5941 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 5942 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 5943 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5944 | `				return SXERR_ABORT;` |
|       - | 5945 | `			}` |
|     ! 0 | 5946 | `			goto done;` |
|       - | 5947 | `		}` |
|     529 | 5948 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|     529 | 5949 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     529 | 5950 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 5951 | `				/* Trait uses another trait: use T[, T2] [{ resolution }]; A trait` |
|       - | 5952 | `				 * name is a full class reference — qualified or fully-qualified` |
|       - | 5953 | ``				 * (`use Foo\T;`, `use \Foo\T;`) — so parse it with the shared`` |
|       - | 5954 | `				 * class-reference reader like the CLASS body's trait-use does` |
|       - | 5955 | `				 * (the old single-identifier read choked on the leading '\'),` |
|       - | 5956 | `				 * and collect a TraitUseEntry so an adaptation block` |
|       - | 5957 | `				 * (insteadof/as) applies through the same shared machinery. */` |
|       - | 5958 | `				TraitUseEntry sUse;` |
|      36 | 5959 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      36 | 5960 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      36 | 5961 | `				pGen->pIn++; /* Jump 'use' */` |
|      20 | 5962 | `				for(;;){` |
|       - | 5963 | `					ph7_class *pUsedTrait;` |
|       - | 5964 | `					SyBlob sResolved;` |
|       - | 5965 | `					SyString sUsedName;` |
|      40 | 5966 | `					sxu32 nUseLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|      40 | 5967 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      40 | 5968 | `					if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 | 5969 | `						SyBlobRelease(&sResolved);` |
|     ! 0 | 5970 | `						rc = PH7_GenCompileError(pGen,E_PARSE,nUseLine,` |
|     ! 0 | 5971 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 | 5972 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5973 | `							return SXERR_ABORT;` |
|       - | 5974 | `						}` |
|     ! 0 | 5975 | `						break;` |
|       - | 5976 | `					}` |
|      76 | 5977 | `					pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|      36 | 5978 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      40 | 5979 | `					SyStringInitFromBuf(&sUsedName,` |
|       - | 5980 | `						(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|      40 | 5981 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 5982 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 | 5983 | `					}` |
|      40 | 5984 | `					if( pUsedTrait == 0 ){` |
|     ! 0 | 5985 | `						if( !GenStateLintSkipsBase(pGen,pClass) ){` |
|     ! 0 | 5986 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|       - | 5987 | `								"'%z' is not a trait",&sUsedName);` |
|     ! 0 | 5988 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 5989 | `								SyBlobRelease(&sResolved);` |
|     ! 0 | 5990 | `								return SXERR_ABORT;` |
|       - | 5991 | `							}` |
|     ! 0 | 5992 | `						}` |
|     ! 0 | 5993 | `					}else{` |
|      40 | 5994 | `						SySetPut(&sUse.aTraits,(const void *)&pUsedTrait);` |
|       - | 5995 | `					}` |
|      40 | 5996 | `					SyBlobRelease(&sResolved);` |
|      40 | 5997 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      20 | 5998 | `						break;` |
|       - | 5999 | `					}` |
|       6 | 6000 | `					pGen->pIn++;` |
|       2 | 6001 | `				}` |
|       - | 6002 | `				/* Optional adaptation block (conflict resolution) */` |
|      36 | 6003 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - | 6004 | `					SyToken *pBlock;` |
|      10 | 6005 | `					pGen->pIn++; /* Jump '{' */` |
|      10 | 6006 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|      10 | 6007 | `					sUse.pResolvStart = pGen->pIn;` |
|      10 | 6008 | `					sUse.pResolvEnd = pBlock;` |
|      10 | 6009 | `					if( pBlock < pGen->pEnd ){` |
|      10 | 6010 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       6 | 6011 | `					}else{` |
|     ! 0 | 6012 | `						pGen->pIn = pGen->pEnd;` |
|       - | 6013 | `					}` |
|       4 | 6014 | `				}` |
|      36 | 6015 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|      36 | 6016 | `				continue;` |
|       - | 6017 | `			}` |
|     246 | 6018 | `		}` |
|       - | 6019 | `		/* Everything else is a MEMBER: its modifier run and the declaration it` |
|       - | 6020 | `		 * modifies, read by the same code a class body uses. */` |
|     497 | 6021 | `		rc = GenStateCompileMember(&(*pGen),pClass,"trait");` |
|     497 | 6022 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 6023 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6024 | `				return SXERR_ABORT;` |
|       - | 6025 | `			}` |
|     ! 0 | 6026 | `			goto done;` |
|       - | 6027 | `		}` |
|       5 | 6028 | `	}` |
|       - | 6029 | ``	/* Apply the collected `use` entries (incl. adaptation blocks) through the`` |
|       - | 6030 | `	 * machinery shared with the class-body compiler. */` |
|     321 | 6031 | `	rc = GenStateApplyTraitUses(&(*pGen),pClass,&aUseEntries);` |
|     321 | 6032 | `	SySetRelease(&aUseEntries);` |
|     321 | 6033 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 6034 | `		return SXERR_ABORT;` |
|       - | 6035 | `	}` |
|       - | 6036 | `	/* Reject a php-fatal redeclaration before hoisting the trait. php early-binds` |
|       - | 6037 | `	 * a trait like a plain class -- it has nothing left to link. */` |
|     321 | 6038 | `	if( GenStateGuardClassRedeclaration(pGen,pClass,1) == SXERR_ABORT ){` |
|       3 | 6039 | `		return SXERR_ABORT;` |
|       - | 6040 | `	}` |
|       - | 6041 | `	/* Install the trait */` |
|     319 | 6042 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|     319 | 6043 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6044 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6045 | `		return SXERR_ABORT;` |
|       - | 6046 | `	}` |
|     157 | 6047 | `done:` |
|     319 | 6048 | `	pGen->pCurClass = pSavedCurClass;` |
|     319 | 6049 | `	pGen->pCurBase = pSavedCurBase;` |
|       - | 6050 | `	/* Point beyond the trait body */` |
|     319 | 6051 | `	pGen->pIn = &pEnd[1];` |
|     319 | 6052 | `	pGen->pEnd = pTmp;` |
|     319 | 6053 | `	return PH7_OK;` |
|     171 | 6054 | `}` |
|       - | 6055 | `/*` |
|       - | 6056 | ` * Compile a user-defined class.` |
|       - | 6057 | ` *  According to the PHP language reference manual` |
|       - | 6058 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - | 6059 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - | 6060 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - | 6061 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - | 6062 | ` *   and functions (called "methods").` |
|       - | 6063 | ` */` |
|    4658 | 6064 | `PH7_PRIVATE sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       5 | 6065 | `{` |
|       - | 6066 | `	sxi32 rc;` |
|    4663 | 6067 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|    4663 | 6068 | `	return rc;` |
|       5 | 6069 | `}` |
|       - | 6070 | `/*` |
|       - | 6071 | ` * Return TRUE if the token stream starts an enum declaration (PHP 8.1):` |
|       - | 6072 | `` * the context-sensitive identifier `enum` (not a reserved word — it stays`` |
|       - | 6073 | `` * valid as a function/constant name, like `readonly`) directly followed by`` |
|       - | 6074 | `` * an identifier. `enum(...)`/`enum;`/`$enum` all keep their expression`` |
|       - | 6075 | `` * meaning; `enum Name` can never start a valid expression.`` |
|       - | 6076 | ` */` |
| 2056400 | 6077 | `PH7_PRIVATE int GenStateStartsEnumDecl(SyToken *pIn,SyToken *pEnd)` |
|       5 | 6078 | `{` |
| 2120802 | 6079 | `	return (pIn->nType & PH7_TK_ID)` |
| 1091532 | 6080 | `		&& pIn->sData.nByte == sizeof("enum")-1` |
|   78671 | 6081 | `		&& SyStrnicmp(pIn->sData.zString,"enum",sizeof("enum")-1) == 0` |
| 2121345 | 6082 | `		&& &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_ID);` |
|       5 | 6083 | `}` |
|       - | 6084 | `/*` |
|       - | 6085 | ` * Return TRUE when the token stream starts a CLOSURE EXPRESSION at statement` |
|       - | 6086 | `` * position: `function () {…};` or `fn (…) => …;`, with an optional `&` for a`` |
|       - | 6087 | ` * by-reference return.` |
|       - | 6088 | ` *` |
|       - | 6089 | ` * php compiles both as ordinary expression statements — the closure is built and` |
|       - | 6090 | ` * discarded — and 176 files across the vendor trees write one. PHL sent the bare` |
|       - | 6091 | `` * `function` keyword to PH7_CompileFunction, which demands a NAME and answered`` |
|       - | 6092 | `` * `syntax error, unexpected token "(", expecting "("`; `fn` was not a statement`` |
|       - | 6093 | `` * keyword at all and answered `Unexpected keyword 'fn'`. The `static` forms`` |
|       - | 6094 | `` * (`static function () {};`, `static fn () => 1;`) already worked, which is what`` |
|       - | 6095 | ` * made this look narrower than it was.` |
|       - | 6096 | ` *` |
|       - | 6097 | `` * The NAMED declaration is what must not be caught here, and the `(` is what`` |
|       - | 6098 | `` * tells them apart: `function foo(` has an identifier where a closure has its`` |
|       - | 6099 | `` * parameter list. php refuses an immediately-invoked `function () {}()` at`` |
|       - | 6100 | ` * statement position too, so nothing here tries to accept one.` |
|       - | 6101 | ` */` |
| 2055890 | 6102 | `PH7_PRIVATE int GenStateStartsClosureExpr(SyToken *pIn,SyToken *pEnd)` |
|       5 | 6103 | `{` |
| 2055895 | 6104 | `	if( (pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|  838659 | 6105 | `		return 0;` |
|       - | 6106 | `	}` |
|       - | 6107 | `	{` |
| 1217241 | 6108 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
| 1217241 | 6109 | `		if( nKw != PH7_TKWRD_FUNCTION && nKw != PH7_TKWRD_FN ){` |
| 1065451 | 6110 | `			return 0;` |
|       - | 6111 | `		}` |
|       - | 6112 | `	}` |
|  151795 | 6113 | `	pIn++;` |
|  151795 | 6114 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_AMPER) ){` |
|      38 | 6115 | ``		pIn++;   /* `function &() {…}` returns by reference */`` |
|      17 | 6116 | `	}` |
|  151795 | 6117 | `	return pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) != 0;` |
| 1026337 | 6118 | `}` |
|       - | 6119 | `/*` |
|       - | 6120 | ` * Compile an enum declaration (PHP 8.1). An enum is a final class carrying` |
|       - | 6121 | `` * PH7_CLASS_ENUM: `case` members become lazily-materialized singleton`` |
|       - | 6122 | ` * constants, cases()/from()/tryFrom() are synthesized, and UnitEnum/BackedEnum` |
|       - | 6123 | ` * are implemented implicitly (GenStateCompileClassEx handles the specifics).` |
|       - | 6124 | ` */` |
|     136 | 6125 | `PH7_PRIVATE sxi32 PH7_CompileEnum(ph7_gen_state *pGen)` |
|       5 | 6126 | `{` |
|     141 | 6127 | `	return GenStateCompileClass(&(*pGen),PH7_CLASS_ENUM\|PH7_CLASS_FINAL);` |
|       5 | 6128 | `}` |
|       - | 6129 |  |
