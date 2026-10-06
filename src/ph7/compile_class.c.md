# src/ph7/compile_class.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3486/4108 lines (84.86%)

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
|        3 |   20 | `{` |
|       19 |   21 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){ return "interface"; }` |
|       17 |   22 | `	if( pClass->iFlags & PH7_CLASS_TRAIT ){ return "trait"; }` |
|       15 |   23 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){ return "enum"; }` |
|       13 |   24 | `	return "class";` |
|       11 |   25 | `}` |
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
|        3 |   49 | `{` |
|        - |   50 | `	SyString *pFile;` |
|       19 |   51 | `	if( pPrev->iFlags & PH7_CLASS_BOUND ){` |
|       17 |   52 | `		return 1;   /* early-bound: the name is taken before anything runs */` |
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
|       11 |   64 | `}` |
|        - |   65 | `/*` |
|        - |   66 | `` * A `phl -l` compile PARSES; it does not BIND. php binds inheritance at run time,`` |
|        - |   67 | `` * so `php -l` says nothing about a parent, an interface or a trait it cannot see --`` |
|        - |   68 | ` * and under -l nothing autoloads, so it can see almost none of them. TRUE means` |
|        - |   69 | ` * "say nothing and carry on with no base", which is what leaves the BODY to be` |
|        - |   70 | ` * compiled: the whole point of the mode is that a syntax error in there is found.` |
|        - |   71 | ` */` |
|      160 |   72 | `static int GenStateLintSkipsBase(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 |   73 | `{` |
|      165 |   74 | `	if( pGen->pVm->bSyntaxCheck == 0 && pGen->bDeclCheck == 0 ){` |
|      ! 0 |   75 | `		return 0;` |
|        - |   76 | `	}` |
|      165 |   77 | `	if( pClass ){` |
|      165 |   78 | `		pClass->iFlags \|= PH7_CLASS_LINT_UNBOUND;` |
|       80 |   79 | `	}` |
|      165 |   80 | `	return 1;` |
|       85 |   81 | `}` |
|        - |   82 | `/*` |
|        - |   83 | ` * Look up the parent, interface or trait a declaration names, to LINK it. A` |
|        - |   84 | ` * deferred declaration's check compile links nothing and asks no autoloader:` |
|        - |   85 | ` * php binds a conditional class only when its statement runs, so an` |
|        - |   86 | `` * `if (PHP_VERSION_ID < 80000)` polyfill whose signatures no longer match its`` |
|        - |   87 | ` * interface is not refused while it is never reached -- and nikic/php-parser's` |
|        - |   88 | ``  * `require` of a parent followed by `if (false) { class Alias extends Parent {} }` `` |
|        - |   89 | ` * must not have the parent's file loaded by the lookup before its own require.` |
|        - |   90 | ` */` |
|     2646 |   91 | `static ph7_class * GenStateLinkClass(ph7_gen_state *pGen,SyBlob *pName)` |
|        5 |   92 | `{` |
|     2651 |   93 | `	if( pGen->bDeclCheck ){` |
|      143 |   94 | `		return 0;` |
|        - |   95 | `	}` |
|     3767 |   96 | `	return PH7_VmExtractClass(pGen->pVm,` |
|     2508 |   97 | `		(const char *)SyBlobData(pName),(sxu32)SyBlobLength(pName),FALSE,0);` |
|     1328 |   98 | `}` |
|     7204 |   99 | `static sxi32 GenStateGuardClassRedeclaration(ph7_gen_state *pGen,ph7_class *pClass,` |
|        - |  100 | `	int bEarlyBindable)` |
|        5 |  101 | `{` |
|        - |  102 | `	SyHashEntry *pEntry;` |
|     7209 |  103 | `	int bTopLevel = GenStateUnconditionalTopLevel(pGen);` |
|     7209 |  104 | `	if( pGen->pVm->bSyntaxCheck \|\| pGen->bDeclCheck ){` |
|        - |  105 | ``		/* `php -l` reports a redeclared FUNCTION and not a redeclared CLASS: a`` |
|        - |  106 | `		 * class name is taken at the DECLARE_CLASS opcode, which lint never runs.` |
|        - |  107 | ``		 * So `class DateTime {}` and symfony/polyfill-php80's stub `final class`` |
|        - |  108 | ``		 * Attribute` -- a file composer only ever loads under php 7 -- both lint`` |
|        - |  109 | `		 * clean there, and this refused them. */` |
|      297 |  110 | `		return SXRET_OK;` |
|        - |  111 | `	}` |
|     6917 |  112 | `	if( bTopLevel ){` |
|        - |  113 | `		/* php RUNS this declaration whatever else the file holds, early bound or` |
|        - |  114 | `		 * not -- so two of them under one name collide even when neither was. */` |
|     6775 |  115 | `		pClass->iFlags \|= PH7_CLASS_TOPLEVEL;` |
|     3385 |  116 | `	}` |
|     6917 |  117 | `	if( bTopLevel && !bEarlyBindable ){` |
|        - |  118 | `		/* Not early-bound, but it RUNS: only another declaration that also runs` |
|        - |  119 | `		 * unconditionally collides with it. */` |
|     1748 |  120 | `		SyHashEntry *pTop = SyHashGet(&pGen->pVm->hClass,` |
|     1162 |  121 | `			(const void *)pClass->sName.zString,pClass->sName.nByte);` |
|     1167 |  122 | `		if( pTop ){` |
|        3 |  123 | `			ph7_class *pPrev = (ph7_class *)pTop->pUserData;` |
|        3 |  124 | `			while( pPrev ){` |
|        3 |  125 | `				if( (pPrev->iFlags & PH7_CLASS_HIDDEN) == 0 && GenStateDeclHoldsName(pGen,pPrev,pClass) ){` |
|        3 |  126 | `					pGen->iFatalTrace = PH7_FATAL_TRACE_RUNTIME;` |
|        3 |  127 | `					if( pPrev->sFile.nByte > 0 ){` |
|        4 |  128 | `						PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,` |
|        - |  129 | `							"Cannot redeclare %s %z (previously declared in %.*s:%u)",` |
|        1 |  130 | `							GenStateClassKind(pPrev),&pClass->sDisp,` |
|        1 |  131 | `							pPrev->sFile.nByte,pPrev->sFile.zString,pPrev->nLine);` |
|        2 |  132 | `					}else{` |
|      ! 0 |  133 | `						PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,` |
|      ! 0 |  134 | `							"Cannot redeclare %s %z",GenStateClassKind(pPrev),&pClass->sDisp);` |
|        - |  135 | `					}` |
|        3 |  136 | `					return SXERR_ABORT;` |
|        - |  137 | `				}` |
|      ! 0 |  138 | `				pPrev = pPrev->pNextName;` |
|      ! 0 |  139 | `			}` |
|      ! 0 |  140 | `		}` |
|     1165 |  141 | `		return SXRET_OK;` |
|        - |  142 | `	}` |
|     5755 |  143 | `	if( !bTopLevel ){` |
|        - |  144 | `		/* Conditional, nested -- or one php would not EARLY-BIND either, which` |
|        - |  145 | `		 * is the same thing for this guard: such a declaration only takes effect` |
|        - |  146 | `		 * when its statement runs, so another declaration of the name is not a` |
|        - |  147 | `		 * redeclaration of it. php early-binds only a class it can link with` |
|        - |  148 | `		 * nothing left over, so an implemented INTERFACE, a used TRAIT, an enum` |
|        - |  149 | ``		 * or a `__toString()` (which brings Stringable with it) all rule it out.`` |
|        - |  150 | `		 * Binding one of those at compile time made the polyfill shape above a` |
|        - |  151 | `		 * redeclaration, and phpunit.phar died on it. */` |
|      147 |  152 | `		return SXRET_OK;` |
|        - |  153 | `	}` |
|     5613 |  154 | `	pClass->iFlags \|= PH7_CLASS_BOUND;` |
|     5613 |  155 | `	if( pGen->pVm->bCompilingBuiltin ){` |
|      ! 0 |  156 | `		return SXRET_OK; /* the prelude installs each builtin exactly once */` |
|        - |  157 | `	}` |
|     5613 |  158 | `	pEntry = SyHashGet(&pGen->pVm->hClass,(const void *)pClass->sName.zString,pClass->sName.nByte);` |
|     5613 |  159 | `	if( pEntry ){` |
|       19 |  160 | `		ph7_class *pPrev = (ph7_class *)pEntry->pUserData;` |
|       21 |  161 | `		while( pPrev ){` |
|       19 |  162 | `			if( (pPrev->iFlags & PH7_CLASS_HIDDEN) == 0 && GenStateDeclHoldsName(pGen,pPrev,pClass) ){` |
|        - |  163 | `				/* php cannot early-bind a name it already holds, so THIS refusal comes` |
|        - |  164 | `				 * from the DECLARE_CLASS opcode at run time -- and its stack trace` |
|        - |  165 | `				 * carries the include/require that loaded the unit, where every other` |
|        - |  166 | `				 * compile-time refusal's does not. */` |
|       17 |  167 | `				pGen->iFatalTrace = PH7_FATAL_TRACE_RUNTIME;` |
|        - |  168 | `				/* php names the entity by the PREVIOUS declaration's kind and omits` |
|        - |  169 | `				 * the "(previously declared in ...)" clause for internal symbols. */` |
|       17 |  170 | `				if( pPrev->sFile.nByte > 0 ){` |
|       21 |  171 | `					PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,` |
|        - |  172 | `						"Cannot redeclare %s %z (previously declared in %.*s:%u)",` |
|        6 |  173 | `						GenStateClassKind(pPrev),&pClass->sDisp,` |
|        6 |  174 | `						pPrev->sFile.nByte,pPrev->sFile.zString,pPrev->nLine);` |
|        9 |  175 | `				}else{` |
|        4 |  176 | `					PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,` |
|        1 |  177 | `						"Cannot redeclare %s %z",GenStateClassKind(pPrev),&pClass->sDisp);` |
|        - |  178 | `				}` |
|       17 |  179 | `				return SXERR_ABORT;` |
|        - |  180 | `			}` |
|        3 |  181 | `			pPrev = pPrev->pNextName;` |
|        1 |  182 | `		}` |
|        1 |  183 | `	}` |
|     5599 |  184 | `	return SXRET_OK;` |
|     3607 |  185 | `}` |
|        - |  186 | `/*` |
|        - |  187 | ` * php early-binds only a declaration it can link whole at compile time: a class` |
|        - |  188 | ` * with an interface, a trait, a variance pair left open or a parent the file` |
|        - |  189 | ` * itself declares that way, and an interface that extends another, are declared` |
|        - |  190 | ` * where the statement RUNS, and nothing above it finds them. Such a class is` |
|        - |  191 | ` * hidden once its unit has compiled (PH7_VmHideClasses) and declared by the` |
|        - |  192 | ` * PH7_OP_CLASS_DECLARE emitted here, at the statement.` |
|        - |  193 | ` */` |
|     1288 |  194 | `static void GenStateHideUntilDeclared(ph7_gen_state *pGen,ph7_class *pClass,sxu32 nLine)` |
|        5 |  195 | `{` |
|        - |  196 | `	sxu32 nIdx;` |
|     1293 |  197 | `	if( SySetPut(&pGen->pVm->aHiddenClass,(const void *)&pClass) != SXRET_OK ){` |
|      ! 0 |  198 | `		return;` |
|        - |  199 | `	}` |
|     1293 |  200 | `	pClass->iFlags \|= PH7_CLASS_LATEBIND;` |
|     1288 |  201 | `	if( PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLASS_DECLARE,0,0,(void *)pClass,&nIdx) == SXRET_OK` |
|     1293 |  202 | `	 && nLine > 0 ){` |
|     1293 |  203 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm,nIdx);` |
|     1293 |  204 | `		if( pInstr ){` |
|     1293 |  205 | `			pInstr->nLine = nLine;` |
|      644 |  206 | `		}` |
|      644 |  207 | `	}` |
|      649 |  208 | `}` |
|        - |  209 | `/* Is this freshly compiled top-level declaration one php declares only where it runs? */` |
|     6480 |  210 | `static int GenStateDeclaredWhereRun(ph7_gen_state *pGen,ph7_class *pClass,int bOpenPair)` |
|        5 |  211 | `{` |
|     6480 |  212 | `	if( pGen->nErr > 0 \|\| pGen->pVm->bCompilingBuiltin` |
|     6059 |  213 | `	 \|\| (pClass->iFlags & PH7_CLASS_TOPLEVEL) == 0 ){` |
|      681 |  214 | `		return 0;` |
|        - |  215 | `	}` |
|     7660 |  216 | `	return (pClass->iFlags & PH7_CLASS_BOUND) == 0 \|\| bOpenPair` |
|     8185 |  217 | `		\|\| (pClass->pBase && (pClass->pBase->iFlags & PH7_CLASS_LATEBIND));` |
|     3245 |  218 | `}` |
|        - |  219 | `/*` |
|        - |  220 | ` * Extract the visibility level associated with a given keyword.` |
|        - |  221 | ` * According to the PHP language reference manual` |
|        - |  222 | ` *  Visibility:` |
|        - |  223 | ` *  The visibility of a property or method can be defined by prefixing` |
|        - |  224 | ` *  the declaration with the keywords public, protected or private.` |
|        - |  225 | ` *  Class members declared public can be accessed everywhere.` |
|        - |  226 | ` *  Members declared protected can be accessed only within the class` |
|        - |  227 | ` *  itself and by inherited and parent classes. Members declared as private` |
|        - |  228 | ` *  may only be accessed by the class that defines the member.` |
|        - |  229 | ` */` |
|    10110 |  230 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|        5 |  231 | `{` |
|    10115 |  232 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|      683 |  233 | `		return PH7_CLASS_PROT_PRIVATE;` |
|     9437 |  234 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|      281 |  235 | `		return PH7_CLASS_PROT_PROTECTED;` |
|        - |  236 | `	}` |
|        - |  237 | `	/* Assume public by default */` |
|     9161 |  238 | `	return PH7_CLASS_PROT_PUBLIC;` |
|     5060 |  239 | `}` |
|        - |  240 | `/*` |
|        - |  241 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|        - |  242 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|        - |  243 | ` * token immediately followed by '='. Anything else with a leading type token` |
|        - |  244 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|        - |  245 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|        - |  246 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|        - |  247 | ` */` |
|      724 |  248 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|        5 |  249 | `{` |
|        - |  250 | `	SyToken *p0, *p1;` |
|      729 |  251 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  252 | `		return 0;` |
|        - |  253 | `	}` |
|      729 |  254 | `	p0 = pGen->pIn;` |
|        - |  255 | `	/* A leading '\' (namespaced class type), '?' (nullable) or '(' (a DNF type's` |
|        - |  256 | `	 * first intersection group) always starts a type -- none of the three can begin` |
|        - |  257 | `	 * a constant NAME. */` |
|      729 |  258 | `	if( p0->nType & (PH7_TK_NSSEP\|PH7_TK_LPAREN) ){` |
|        3 |  259 | `		return 1;` |
|        - |  260 | `	}` |
|      727 |  261 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|       10 |  262 | `		return 1;` |
|        - |  263 | `	}` |
|        - |  264 | `	/* A name-like first token begins a type only when followed by another` |
|        - |  265 | `	 * name (the constant name), a union separator '\|' or an intersection '&'.` |
|        - |  266 | `	 * Followed by '=', ';' or ',' it is the constant name itself (untyped).` |
|        - |  267 | `	 * Without the '&' a typed constant whose type is an INTERSECTION was read as` |
|        - |  268 | `	 * an untyped one named after the first member, and refused with` |
|        - |  269 | `	 * "Expected '=' after class constant Countable". */` |
|      719 |  270 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      719 |  271 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|      719 |  272 | `		if( p1 ){` |
|      719 |  273 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP\|PH7_TK_AMPER) ){` |
|       67 |  274 | `				return 1;` |
|        - |  275 | `			}` |
|      652 |  276 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1` |
|      657 |  277 | `			 && (p1->sData.zString[0] == '\|' \|\| p1->sData.zString[0] == '&') ){` |
|       15 |  278 | `				return 1;` |
|        - |  279 | `			}` |
|      320 |  280 | `		}` |
|      320 |  281 | `	}` |
|      645 |  282 | `	return 0;` |
|      367 |  283 | `}` |
|        - |  284 | `/*` |
|        - |  285 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|        - |  286 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|        - |  287 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|        - |  288 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|        - |  289 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|        - |  290 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|        - |  291 | ` * Peek only; never consumes tokens. A parameter default asks it of its own span.` |
|        - |  292 | ` */` |
|     2314 |  293 | `PH7_PRIVATE int PH7_GenStateIsRealLiteral(const SyToken *p,const SyToken *pEnd)` |
|        5 |  294 | `{` |
|     3613 |  295 | `	while( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|     1435 |  296 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|       11 |  297 | `		p++; /* skip leading unary sign(s) */` |
|        3 |  298 | `	}` |
|     2319 |  299 | `	if( p >= pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|     2271 |  300 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|        - |  301 | `	}` |
|       53 |  302 | `	p++;` |
|        - |  303 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|       53 |  304 | `	return ( p >= pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|     1162 |  305 | `}` |
|     2306 |  306 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|        5 |  307 | `{` |
|     2311 |  308 | `	return PH7_GenStateIsRealLiteral(pGen->pIn,pGen->pEnd);` |
|        5 |  309 | `}` |
|        - |  310 | `/*` |
|        - |  311 | `` * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).`` |
|        - |  312 | `` * A `new` that immediately follows one of these is a member name (`A::new`,`` |
|        - |  313 | `` * `$o->new`), not a `new` expression.`` |
|        - |  314 | ` */` |
|      258 |  315 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|        5 |  316 | `{` |
|        - |  317 | `	sxi32 iOp;` |
|      263 |  318 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|       93 |  319 | `		return 0;` |
|        - |  320 | `	}` |
|      171 |  321 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|      171 |  322 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|      134 |  323 | `}` |
|        - |  324 | `/*` |
|        - |  325 | ` * Skip the whole closure / arrow-function construct beginning at *pp, which is` |
|        - |  326 | `` * positioned on the `function` / `fn` keyword. Its body is ordinary runtime code,`` |
|        - |  327 | ` * so none of the constant-expression rules below reach into it. *piDepth is the` |
|        - |  328 | ` * caller's bracket depth: an arrow function has no braces of its own and ends at` |
|        - |  329 | `` * a `,`/`;` or at a bracket closing an ENCLOSING group, so it shares that depth,`` |
|        - |  330 | `` * while a `function(){...}` body is brace-balanced and walks its own.`` |
|        - |  331 | ` */` |
|      192 |  332 | `static void GenStateInitSkipFuncConstruct(SyToken **pp,SyToken *pEnd,int *piDepth)` |
|        5 |  333 | `{` |
|      197 |  334 | `	SyToken *p = *pp;` |
|      197 |  335 | `	int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|      197 |  336 | `	int iBase = *piDepth;` |
|      197 |  337 | `	p++;` |
|      197 |  338 | `	if( bArrow ){` |
|        - |  339 | `		/* fn(params) => expr : skip to the end of the current element. */` |
|       86 |  340 | `		while( p < pEnd ){` |
|       78 |  341 | `			if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       14 |  342 | `				(*piDepth)++;` |
|       72 |  343 | `			}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       14 |  344 | `				if( *piDepth <= iBase ){` |
|      ! 0 |  345 | `					break; /* closes an enclosing group, not the fn's own */` |
|        - |  346 | `				}` |
|       14 |  347 | `				(*piDepth)--;` |
|       60 |  348 | `			}else if( *piDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|        6 |  349 | `				break;` |
|        - |  350 | `			}` |
|       74 |  351 | `			p++;` |
|        2 |  352 | `		}` |
|        8 |  353 | `	}else{` |
|        - |  354 | `		/* function(params)[use(...)][: type] { body } : skip the signature up to the` |
|        - |  355 | ``		 * body '{' (a '{' at closure-local depth 0, so a `new class{}` default inside`` |
|        - |  356 | `		 * the parens is not mistaken for it), then the balanced brace block. */` |
|      185 |  357 | `		int iLocal = 0;` |
|      545 |  358 | `		while( p < pEnd ){` |
|      545 |  359 | `			if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|      185 |  360 | `				break; /* body brace */` |
|        - |  361 | `			}` |
|      365 |  362 | `			if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      185 |  363 | `				iLocal++;` |
|      275 |  364 | `			}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      185 |  365 | `				if( iLocal > 0 ){` |
|      185 |  366 | `					iLocal--;` |
|       90 |  367 | `				}` |
|       90 |  368 | `			}` |
|      365 |  369 | `			p++;` |
|        5 |  370 | `		}` |
|      185 |  371 | `		if( p < pEnd ){` |
|      185 |  372 | `			int iBrace = 0; /* p is on the body '{' */` |
|     1217 |  373 | `			while( p < pEnd ){` |
|     1217 |  374 | `				if( p->nType & PH7_TK_OCB ){` |
|      205 |  375 | `					iBrace++;` |
|     1117 |  376 | `				}else if( p->nType & PH7_TK_CCB ){` |
|      205 |  377 | `					iBrace--;` |
|      205 |  378 | `					if( iBrace == 0 ){` |
|      185 |  379 | `						p++;` |
|      185 |  380 | `						break;` |
|        - |  381 | `					}` |
|       10 |  382 | `				}` |
|     1037 |  383 | `				p++;` |
|        5 |  384 | `			}` |
|       90 |  385 | `		}` |
|        - |  386 | `	}` |
|      197 |  387 | `	*pp = p;` |
|      197 |  388 | `}` |
|        - |  389 | `/*` |
|        - |  390 | `` * TRUE if *p is the `::` operator.`` |
|        - |  391 | ` */` |
|    72166 |  392 | `static int GenStateTokenIsDoubleColon(const SyToken *p)` |
|        5 |  393 | `{` |
|    45324 |  394 | `	return ( (p->nType & PH7_TK_OP) && p->pUserData` |
|    76801 |  395 | `		&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_DC );` |
|        5 |  396 | `}` |
|        - |  397 | `/*` |
|        - |  398 | `` * Where the constant expression starting at *pStart ends: the first `,` or `;` at`` |
|        - |  399 | `` * bracket depth 0, or the first depth-0 `{`, which in a property declaration can`` |
|        - |  400 | `` * only open a PHP 8.4 hook list (`public T $x = default { get …; }`) and never`` |
|        - |  401 | ` * belongs to the default itself.` |
|        - |  402 | ` */` |
|    55546 |  403 | `static SyToken * GenStateConstExprEnd(SyToken *pStart,SyToken *pEnd)` |
|        5 |  404 | `{` |
|    55551 |  405 | `	SyToken *p = pStart;` |
|    55551 |  406 | `	int iDepth = 0;` |
|   127985 |  407 | `	while( p < pEnd ){` |
|    75820 |  408 | `		if( (p->nType & PH7_TK_KEYWORD) && (p->nType & PH7_TK_MEMBER_NAME) == 0` |
|      243 |  409 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|      216 |  410 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|        - |  411 | `			/* A closure's BODY brace is not a hook list: skip the construct whole,` |
|        - |  412 | ``			 * or `$c = static function(){}` would end the expression at that brace`` |
|        - |  413 | `			 * and hide everything after it from the rules. */` |
|       53 |  414 | `			GenStateInitSkipFuncConstruct(&p,pEnd,&iDepth);` |
|       53 |  415 | `			continue;` |
|        - |  416 | `		}` |
|    75777 |  417 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA\|PH7_TK_OCB)) ){` |
|     3391 |  418 | `			break;` |
|        - |  419 | `		}` |
|    72391 |  420 | `		if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      509 |  421 | `			iDepth++;` |
|    72139 |  422 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      509 |  423 | `			if( iDepth > 0 ){` |
|      509 |  424 | `				iDepth--;` |
|      252 |  425 | `			}` |
|      252 |  426 | `		}` |
|    72391 |  427 | `		p++;` |
|        5 |  428 | `	}` |
|    55551 |  429 | `	return p;` |
|        5 |  430 | `}` |
|        - |  431 | `/*` |
|        - |  432 | ` * Decide a ternary CONDITION php would have folded: 1 truthy, 0 falsy, -1 "cannot` |
|        - |  433 | ` * tell". Only a lone literal (with any number of redundant parens around it) is` |
|        - |  434 | `` * decided here. php's own folder reaches much further -- it evaluates `1 === 1`,`` |
|        - |  435 | `` * `PHP_INT_SIZE === 8`, even `[1,2][0]` -- and everything it cannot fold at compile`` |
|        - |  436 | ` * time, a user constant among them, it leaves for the rule walk to refuse. The` |
|        - |  437 | ` * caller answers -1 by leaving the whole expression alone, which is the safe` |
|        - |  438 | ` * direction: an offender php would have refused stays accepted, and no valid` |
|        - |  439 | ` * program is refused on a branch php would have dropped.` |
|        - |  440 | ` */` |
|       28 |  441 | `static int GenStateConstExprTruth(SyToken *pStart,SyToken *pStop)` |
|        4 |  442 | `{` |
|       32 |  443 | `	SyToken *p = pStart, *q = pStop;` |
|       32 |  444 | `	while( p < q && (p->nType & PH7_TK_LPAREN) && (q[-1].nType & PH7_TK_RPAREN) ){` |
|      ! 0 |  445 | `		p++;` |
|      ! 0 |  446 | `		q--;` |
|      ! 0 |  447 | `	}` |
|       32 |  448 | `	if( &p[1] != q ){` |
|      ! 0 |  449 | `		return -1; /* not a lone token */` |
|        - |  450 | `	}` |
|       32 |  451 | `	if( p->nType & PH7_TK_ID ){` |
|        - |  452 | ``		/* `true`/`false`/`null` are not lexer keywords here -- they arrive as plain`` |
|        - |  453 | `		 * identifiers and are recognised by name (php reserves all three, so no user` |
|        - |  454 | `		 * constant can shadow one). Any OTHER name is a constant whose value this` |
|        - |  455 | `		 * stage does not know, which is exactly where php stops folding too. */` |
|       27 |  456 | `		if( p->sData.nByte == 4 && SyStrnicmp(p->sData.zString,"true",4) == 0 ){` |
|       19 |  457 | `			return 1;` |
|        - |  458 | `		}` |
|        8 |  459 | `		if( (p->sData.nByte == 5 && SyStrnicmp(p->sData.zString,"false",5) == 0)` |
|        5 |  460 | `			\|\| (p->sData.nByte == 4 && SyStrnicmp(p->sData.zString,"null",4) == 0) ){` |
|        9 |  461 | `			return 0;` |
|        - |  462 | `		}` |
|      ! 0 |  463 | `		return -1;` |
|        - |  464 | `	}` |
|        6 |  465 | `	if( p->nType & (PH7_TK_INTEGER\|PH7_TK_REAL) ){` |
|        - |  466 | `		/* php's truthiness: 0 and 0.0 are false, everything else true. */` |
|        6 |  467 | `		const char *z = p->sData.zString;` |
|        6 |  468 | `		sxu32 n = p->sData.nByte, i;` |
|        6 |  469 | `		for( i = 0 ; i < n ; ++i ){` |
|        6 |  470 | `			if( z[i] != '0' && z[i] != '.' && z[i] != '+' && z[i] != '-' ){` |
|        6 |  471 | `				return 1;` |
|        - |  472 | `			}` |
|      ! 0 |  473 | `		}` |
|      ! 0 |  474 | `		return 0;` |
|        - |  475 | `	}` |
|      ! 0 |  476 | `	if( p->nType & PH7_TK_DSTR ){` |
|        - |  477 | `		/* A double-quoted literal may interpolate; only a single-quoted one is` |
|        - |  478 | `		 * certainly its own text. Leave the rest undecided. */` |
|      ! 0 |  479 | `		return -1;` |
|        - |  480 | `	}` |
|      ! 0 |  481 | `	if( p->nType & PH7_TK_SSTR ){` |
|      ! 0 |  482 | `		return ( p->sData.nByte == 0` |
|      ! 0 |  483 | `			\|\| (p->sData.nByte == 1 && p->sData.zString[0] == '0') ) ? 0 : 1;` |
|        - |  484 | `	}` |
|      ! 0 |  485 | `	return -1;` |
|       18 |  486 | `}` |
|        - |  487 | `static const char * GenStateConstExprSpan(SyToken *pStart,SyToken *pStop,int bAllowNew,int nDepth);` |
|        - |  488 | `/*` |
|        - |  489 | `` * TRUE if a `?` appears anywhere in the span outside a closure body. Used only`` |
|        - |  490 | ` * after the top-level ternary has been ruled out: a ternary NESTED in a bracket` |
|        - |  491 | `` * (`[true ? 1 : new X]`, `(true ? 1 : strlen('a')) + 1`) is folded by php just the`` |
|        - |  492 | ` * same, but the split below cannot say where its condition begins, so the span is` |
|        - |  493 | `` * left alone rather than have a dropped branch refuse a valid program. A `?` in a`` |
|        - |  494 | `` * closure body is runtime code and does not count; `?->` and `??` are their own`` |
|        - |  495 | ` * operators and never land here.` |
|        - |  496 | ` */` |
|    55574 |  497 | `static int GenStateSpanHasNestedTernary(SyToken *pStart,SyToken *pStop)` |
|        5 |  498 | `{` |
|    55579 |  499 | `	SyToken *p = pStart;` |
|    55579 |  500 | `	int iDepth = 0;` |
|   127851 |  501 | `	while( p < pStop ){` |
|    72280 |  502 | `		if( (p->nType & PH7_TK_KEYWORD) && (p->nType & PH7_TK_MEMBER_NAME) == 0` |
|      231 |  503 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|      208 |  504 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|       45 |  505 | `			GenStateInitSkipFuncConstruct(&p,pStop,&iDepth);` |
|       45 |  506 | `			continue;` |
|        - |  507 | `		}` |
|    72240 |  508 | `		if( (p->nType & PH7_TK_OP) && p->pUserData` |
|     9289 |  509 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_QUESTY ){` |
|        9 |  510 | `			return 1;` |
|        - |  511 | `		}` |
|    72237 |  512 | `		p++;` |
|        5 |  513 | `	}` |
|    55571 |  514 | `	return 0;` |
|    27758 |  515 | `}` |
|        - |  516 | `/*` |
|        - |  517 | ` * Split a constant expression at its top-level ternary, the way php's constant` |
|        - |  518 | ` * folder does: the condition decides which branch survives, and only the surviving` |
|        - |  519 | `` * one is subject to the rules. `true ? 1 : new X` and `false ? new X : 1` are both`` |
|        - |  520 | ` * legal php for exactly this reason, and scanning the dropped branch refused valid` |
|        - |  521 | ` * programs. Answers 1 when it handled the span (writing the verdict to *pzErr).` |
|        - |  522 | ` */` |
|    55602 |  523 | `static int GenStateConstExprTernary(SyToken *pStart,SyToken *pStop,int bAllowNew,` |
|        - |  524 | `	int nDepth,const char **pzErr)` |
|        5 |  525 | `{` |
|    55607 |  526 | `	SyToken *p = pStart, *pQ = 0, *pColon = 0;` |
|    55607 |  527 | `	int iDepth = 0, iNest = 0, iTruth;` |
|   128027 |  528 | `	while( p < pStop ){` |
|    72448 |  529 | `		if( (p->nType & PH7_TK_KEYWORD) && (p->nType & PH7_TK_MEMBER_NAME) == 0` |
|      237 |  530 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|      212 |  531 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|        - |  532 | `			/* A ternary inside a closure body -- or an ARROW function's whole body,` |
|        - |  533 | `			 * which carries no braces to raise the depth -- is runtime code, not this` |
|        - |  534 | ``			 * expression's ternary. Reading `fn() => true ? 1 : 2` as one folded away`` |
|        - |  535 | `			 * the arrow that the rules exist to refuse. */` |
|       49 |  536 | `			GenStateInitSkipFuncConstruct(&p,pStop,&iDepth);` |
|       49 |  537 | `			continue;` |
|        - |  538 | `		}` |
|    72409 |  539 | `		if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      507 |  540 | `			iDepth++;` |
|    72158 |  541 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      507 |  542 | `			if( iDepth > 0 ){` |
|      507 |  543 | `				iDepth--;` |
|      251 |  544 | `			}` |
|    71656 |  545 | `		}else if( iDepth == 0 && (p->nType & PH7_TK_OP) && p->pUserData` |
|     2729 |  546 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_QUESTY ){` |
|       32 |  547 | `			if( pQ == 0 ){` |
|       32 |  548 | `				pQ = p;` |
|       14 |  549 | `			}` |
|       32 |  550 | `			iNest++;` |
|    71391 |  551 | `		}else if( iDepth == 0 && pQ != 0 && (p->nType & PH7_TK_COLON) ){` |
|        - |  552 | ``			/* `::` is one DC operator token, not two colons, so it never lands here;`` |
|        - |  553 | ``			 * a nested ternary's own colon is matched against its own `?`. */`` |
|       32 |  554 | `			iNest--;` |
|       32 |  555 | `			if( iNest == 0 ){` |
|       32 |  556 | `				pColon = p;` |
|       32 |  557 | `				break;` |
|        - |  558 | `			}` |
|      ! 0 |  559 | `		}` |
|    72381 |  560 | `		p++;` |
|        5 |  561 | `	}` |
|    55607 |  562 | `	if( pQ == 0 ){` |
|    55579 |  563 | `		return 0; /* no top-level ternary: the linear walk owns this span */` |
|        - |  564 | `	}` |
|       32 |  565 | `	if( pColon == 0 ){` |
|      ! 0 |  566 | `		*pzErr = 0; /* unbalanced (the parser will say so); nothing to rule on here */` |
|      ! 0 |  567 | `		return 1;` |
|        - |  568 | `	}` |
|       32 |  569 | `	iTruth = GenStateConstExprTruth(pStart,pQ);` |
|       32 |  570 | `	if( iTruth < 0 ){` |
|      ! 0 |  571 | `		*pzErr = 0; /* php would fold what this cannot: leave the whole span alone */` |
|      ! 0 |  572 | `		return 1;` |
|        - |  573 | `	}` |
|       32 |  574 | `	*pzErr = GenStateConstExprSpan(pStart,pQ,bAllowNew,nDepth + 1);` |
|       32 |  575 | `	if( *pzErr == 0 ){` |
|        - |  576 | ``		/* `c ?: e` keeps the condition when it is truthy, so the then-span is empty. */`` |
|       32 |  577 | `		*pzErr = iTruth` |
|       20 |  578 | `			? GenStateConstExprSpan(&pQ[1],pColon,bAllowNew,nDepth + 1)` |
|       18 |  579 | `			: GenStateConstExprSpan(&pColon[1],pStop,bAllowNew,nDepth + 1);` |
|       14 |  580 | `	}` |
|       32 |  581 | `	return 1;` |
|    27772 |  582 | `}` |
|        - |  583 | `/*` |
|        - |  584 | ` * Every compile-time rule php applies to a CONSTANT EXPRESSION, over one token span,` |
|        - |  585 | ` * as a single left-to-right walk that answers with the FIRST offender's sentence --` |
|        - |  586 | ` * which is exactly the one php prints, because php walks the same expression in the` |
|        - |  587 | ` * same order and stops at the first node it refuses to compile. Run as separate` |
|        - |  588 | ` * passes, the rules got that order wrong whenever two kinds appeared together:` |
|        - |  589 | `` * `[strlen('a'), function(){}]` said "Closures in constant expressions must be`` |
|        - |  590 | ` * static" where php says "Constant expression contains invalid operations".` |
|        - |  591 | ` *` |
|        - |  592 | ` * The rules, in the order a token can trigger them:` |
|        - |  593 | ` *   - an ARROW function -- "Constant expression contains invalid operations". There` |
|        - |  594 | `` *     is no static-`fn` escape: `static fn()=>1` is refused too;`` |
|        - |  595 | ` *   - an IMMEDIATELY INVOKED closure -- a call is what php names it, so it takes the` |
|        - |  596 | ` *     call sentence and not the closure one;` |
|        - |  597 | ` *   - a NON-STATIC closure -- "Closures in constant expressions must be static".` |
|        - |  598 | `` *     `static function(){...}` is accepted and its body skipped, being runtime code;`` |
|        - |  599 | `` *   - a STATIC PROPERTY fetch (`C::$p`, `self::$p`, `static::$p`) -- php has no`` |
|        - |  600 | ` *     constant-expression node for one, so it is "invalid operations";` |
|        - |  601 | `` *   - `static::` -- "\"static::\" is not allowed in compile-time constants", with the`` |
|        - |  602 | ``  *     `static::class` face carrying php's own separate sentence. `self::`/`parent::` `` |
|        - |  603 | ` *     are fine: they name the DECLARING class, which is known where it is written;` |
|        - |  604 | ` *   - a CALL -- "Constant expression contains invalid operations". A first-class` |
|        - |  605 | `` *     callable (`strlen(...)`) is only an ellipsis in parens, and a constructor's`` |
|        - |  606 | `` *     argument list belongs to its `new`, so neither of those counts;`` |
|        - |  607 | `` *   - `new`, unless bAllowNew -- "New expressions are not supported in this context".`` |
|        - |  608 | ` */` |
|    55602 |  609 | `static const char * GenStateConstExprSpan(SyToken *pStart,SyToken *pStop,int bAllowNew,int nDepth)` |
|        5 |  610 | `{` |
|    55607 |  611 | `	SyToken *p = pStart;` |
|    55607 |  612 | `	int iDepth = 0;` |
|    55607 |  613 | `	const char *zTern = 0;` |
|    55607 |  614 | `	if( nDepth > 32 ){` |
|      ! 0 |  615 | `		return 0; /* pathological nesting: stop rather than recurse */` |
|        - |  616 | `	}` |
|    55607 |  617 | `	if( GenStateConstExprTernary(pStart,pStop,bAllowNew,nDepth,&zTern) ){` |
|       32 |  618 | `		return zTern;` |
|        - |  619 | `	}` |
|    55579 |  620 | `	if( GenStateSpanHasNestedTernary(pStart,pStop) ){` |
|        9 |  621 | `		return 0; /* php folds it and this cannot: leave the span alone */` |
|        - |  622 | `	}` |
|   127705 |  623 | `	while( p < pStop ){` |
|    72179 |  624 | `		if( (p->nType & PH7_TK_KEYWORD) && (p->nType & PH7_TK_MEMBER_NAME) == 0 ){` |
|      227 |  625 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(p->pUserData);` |
|      227 |  626 | `			if( nKw == PH7_TKWRD_FN ){` |
|        6 |  627 | `				return "Constant expression contains invalid operations";` |
|        - |  628 | `			}` |
|      223 |  629 | `			if( nKw == PH7_TKWRD_FUNCTION ){` |
|       38 |  630 | `				int iScan = iDepth;` |
|       38 |  631 | `				SyToken *q = p;` |
|        - |  632 | ``				/* `(function(){...})()` is a CALL to php, and a call is what it names --`` |
|        - |  633 | `				 * the closure rule never gets a say. Look past the construct and the` |
|        - |  634 | `				 * parens wrapping it for the argument list. */` |
|       38 |  635 | `				GenStateInitSkipFuncConstruct(&q,pStop,&iScan);` |
|       40 |  636 | `				while( q < pStop && (q->nType & PH7_TK_RPAREN) ){` |
|        3 |  637 | `					q++;` |
|        1 |  638 | `				}` |
|       38 |  639 | `				if( q < pStop && (q->nType & PH7_TK_LPAREN) ){` |
|        6 |  640 | `					return "Constant expression contains invalid operations";` |
|        - |  641 | `				}` |
|        - |  642 | ``				/* The `static` modifier sits in the token immediately before. */`` |
|       36 |  643 | `				if( !(p > pStart && (p[-1].nType & PH7_TK_KEYWORD)` |
|       26 |  644 | `					&& SX_PTR_TO_INT(p[-1].pUserData) == PH7_TKWRD_STATIC) ){` |
|        8 |  645 | `					return "Closures in constant expressions must be static";` |
|        - |  646 | `				}` |
|       30 |  647 | `				GenStateInitSkipFuncConstruct(&p,pStop,&iDepth);` |
|       30 |  648 | `				continue;` |
|        - |  649 | `			}` |
|      184 |  650 | `			if( nKw == PH7_TKWRD_STATIC && &p[1] < pStop` |
|       34 |  651 | `				&& GenStateTokenIsDoubleColon(&p[1])` |
|       25 |  652 | `				&& !(&p[2] < pStop && (p[2].nType & PH7_TK_DOLLAR)) ){` |
|        - |  653 | ``				/* `static::$p` is excluded above: it is a property fetch, which the`` |
|        - |  654 | ``				 * `::` rule below words php's way. */`` |
|        4 |  655 | `				if( &p[2] < pStop && (p[2].nType & PH7_TK_KEYWORD)` |
|        4 |  656 | `					&& SX_PTR_TO_INT(p[2].pUserData) == PH7_TKWRD_CLASS ){` |
|        3 |  657 | `					return "static::class cannot be used for compile-time class name resolution";` |
|        - |  658 | `				}` |
|        3 |  659 | `				return "\"static::\" is not allowed in compile-time constants";` |
|        - |  660 | `			}` |
|       90 |  661 | `		}` |
|    72137 |  662 | `		if( GenStateTokenIsDoubleColon(p) && &p[1] < pStop && (p[1].nType & PH7_TK_DOLLAR) ){` |
|        3 |  663 | `			return "Constant expression contains invalid operations";` |
|        - |  664 | `		}` |
|    72135 |  665 | `		if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        - |  666 | `			/* A '(' directly after a NAME is a call. A name here is an identifier` |
|        - |  667 | ``			 * token; `new X(` is excluded by walking back to the `new` operator, and`` |
|        - |  668 | ``			 * `f(...)` (first-class callable) by peeking for a lone ellipsis. */`` |
|      480 |  669 | `			if( (p->nType & PH7_TK_LPAREN) && p > pStart` |
|       93 |  670 | `				&& (p[-1].nType & PH7_TK_ID)` |
|       82 |  671 | `				&& !((p[-1].nType & PH7_TK_OP) && p[-1].pUserData` |
|      ! 0 |  672 | `					&& ((const ph7_expr_op *)p[-1].pUserData)->iOp == EXPR_OP_NEW) ){` |
|       67 |  673 | `				int bNewCtor = 0;` |
|       67 |  674 | `				SyToken *q = &p[-1];` |
|        - |  675 | ``				/* Walk back over a qualified name (A\B, A::b, $o->m) to a `new`. The name`` |
|        - |  676 | `				 * ALTERNATES -- segment, separator, segment -- so both steps have to be` |
|        - |  677 | `				 * taken: stepping over the separator alone stopped on the segment before` |
|        - |  678 | ``				 * it, and every `new` whose class name carries a `\` then read as a CALL.`` |
|        - |  679 | ``				 * `new Rule\A()` is the ordinary spelling in namespaced code, so an`` |
|        - |  680 | ``				 * attribute argument, a global `const` and a parameter default all`` |
|        - |  681 | `				 * refused what php compiles (Respect\Validation's whole attribute` |
|        - |  682 | `				 * suite is written that way). */` |
|      131 |  683 | `				while( q > pStart ){` |
|      115 |  684 | `					SyToken *pPrev = &q[-1];` |
|      110 |  685 | `					if( (pPrev->nType & PH7_TK_OP) && pPrev->pUserData` |
|       51 |  686 | `						&& ((const ph7_expr_op *)pPrev->pUserData)->iOp == EXPR_OP_NEW ){` |
|       49 |  687 | `						bNewCtor = 1;` |
|       49 |  688 | `						break;` |
|        - |  689 | `					}` |
|       68 |  690 | `					if( GenStateTokenIsMemberOp(pPrev) \|\| (pPrev->nType & PH7_TK_NSSEP) ){` |
|       37 |  691 | `						q--;   /* a separator: whatever precedes it continues the name */` |
|       37 |  692 | `						continue;` |
|        - |  693 | `					}` |
|       30 |  694 | `					if( (pPrev->nType & PH7_TK_ID)` |
|       31 |  695 | `						&& (GenStateTokenIsMemberOp(q) \|\| (q->nType & PH7_TK_NSSEP)) ){` |
|       29 |  696 | `						q--;   /* ...and a SEGMENT, but only across a separator */` |
|       29 |  697 | `						continue;` |
|        - |  698 | `					}` |
|        3 |  699 | `					break;` |
|      ! 0 |  700 | `				}` |
|       67 |  701 | `				if( !bNewCtor && !(&p[1] < pStop && (p[1].nType & PH7_TK_ELLIPSIS)) ){` |
|       17 |  702 | `					return "Constant expression contains invalid operations";` |
|        - |  703 | `				}` |
|       24 |  704 | `			}` |
|      471 |  705 | `			iDepth++;` |
|    71888 |  706 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      465 |  707 | `			if( iDepth > 0 ){` |
|      465 |  708 | `				iDepth--;` |
|      230 |  709 | `			}` |
|    71425 |  710 | `		}else if( !bAllowNew && (p->nType & PH7_TK_OP) && p->pUserData` |
|     8559 |  711 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|        - |  712 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP) whose`` |
|        - |  713 | `` 			 * pUserData is the operator instance, not a keyword id. Ignore a `new` `` |
|        - |  714 | ``			 * used as a member name (`A::new` / `$o->new`). */`` |
|       10 |  715 | `			if( p == pStart \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|       10 |  716 | `				return "New expressions are not supported in this context";` |
|        - |  717 | `			}` |
|      ! 0 |  718 | `		}` |
|    72113 |  719 | `		p++;` |
|        5 |  720 | `	}` |
|    55531 |  721 | `	return 0;` |
|    27772 |  722 | `}` |
|        - |  723 | `/*` |
|        - |  724 | ` * The constant-expression screen as the compiler's call sites use it: the rules of` |
|        - |  725 | ` * GenStateConstExprSpan over the initializer that begins at the current token.` |
|        - |  726 | ` *` |
|        - |  727 | `` * bAllowNew states PHP 8.1's split: a global `const`, a parameter default and an`` |
|        - |  728 | `` * attribute argument take `new`; a class/interface constant, an enum case value and`` |
|        - |  729 | ` * a property default do not.` |
|        - |  730 | ` *` |
|        - |  731 | ` * Returns the sentence, or 0 when the expression is clean. Never consumes tokens.` |
|        - |  732 | ` */` |
|    55546 |  733 | `PH7_PRIVATE const char * PH7_GenStateConstExprError(ph7_gen_state *pGen,int bAllowNew)` |
|        5 |  734 | `{` |
|    83290 |  735 | `	return GenStateConstExprSpan(pGen->pIn,` |
|    27739 |  736 | `		GenStateConstExprEnd(pGen->pIn,pGen->pEnd),bAllowNew,0);` |
|        5 |  737 | `}` |
|        - |  738 | `/*` |
|        - |  739 | ` * php keeps class CONSTANTS and PROPERTIES in separate namespaces: a class may` |
|        - |  740 | `` * declare `const C` and `public $C` together, and `$obj->C` never resolves to the`` |
|        - |  741 | ` * constant. PHL stores each in its own table (constants in hConst, properties in` |
|        - |  742 | ` * hAttr), so these two lookups target the right namespace and never collide.` |
|        - |  743 | ` */` |
|      706 |  744 | `static ph7_class_attr * GenStateExtractConstant(ph7_class *pClass,SyString *pName)` |
|        5 |  745 | `{` |
|      711 |  746 | `	return PH7_ClassExtractConstant(pClass,pName->zString,pName->nByte);` |
|        5 |  747 | `}` |
|     3620 |  748 | `static ph7_class_attr * GenStateExtractProperty(ph7_class *pClass,const char *zName,sxu32 nByte)` |
|        5 |  749 | `{` |
|     3625 |  750 | `	return PH7_ClassExtractAttribute(pClass,zName,nByte);` |
|        5 |  751 | `}` |
|        - |  752 | `/*` |
|        - |  753 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|        - |  754 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|        - |  755 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|        - |  756 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|        - |  757 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|        - |  758 | ` * share the same backing.` |
|        - |  759 | ` */` |
|     1422 |  760 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|        - |  761 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|        5 |  762 | `{` |
|     1427 |  763 | `	pAttr->nType = nType;` |
|     1427 |  764 | `	pAttr->sClass = *pClass;` |
|     1427 |  765 | `	pAttr->sTypeName = *pTypeName;` |
|     1427 |  766 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|        - |  767 | `		sxu32 i;` |
|      301 |  768 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      205 |  769 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|      205 |  770 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|      105 |  771 | `		}` |
|       48 |  772 | `	}` |
|     1427 |  773 | `}` |
|      724 |  774 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|        5 |  775 | `{` |
|      729 |  776 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  777 | `	sxu32 nNameLine;      /* php's line for the group: its first name's */` |
|        - |  778 | `	SySet *pInstrContainer;` |
|        - |  779 | `	ph7_class_attr *pCons;` |
|        - |  780 | `	SyString *pName;` |
|        - |  781 | `	sxi32 rc;` |
|      729 |  782 | `	sxu32 nType = 0;` |
|        - |  783 | `	SyString sTypeClass;` |
|        - |  784 | `	SyString sTypeText;` |
|        - |  785 | `	SySet aUnionAlts;` |
|      729 |  786 | `	sxi32 iTypeFlags = 0;` |
|      729 |  787 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|      729 |  788 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|      729 |  789 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        - |  790 | `	/* Extract visibility level */` |
|      729 |  791 | `	iProtection = GetProtectionLevel(iProtection);` |
|        - |  792 | `	/* Mark as constant */` |
|      729 |  793 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      729 |  794 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|        - |  795 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|        - |  796 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|      771 |  797 | `	if( GenStateClassConstHasType(pGen) ){` |
|      131 |  798 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|        - |  799 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,` |
|       84 |  800 | `			/* bParamCtx */ 0,pGen->pIn->nLine);` |
|        - |  801 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|        - |  802 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|        - |  803 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|        - |  804 | `		 * and success paths release. */` |
|       89 |  805 | `		if( rc == SXERR_CORRUPT ){` |
|        - |  806 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|      ! 0 |  807 | `			goto Synchronize;` |
|       89 |  808 | `		}else if( rc == SXERR_ABORT ){` |
|      ! 0 |  809 | `			return SXERR_ABORT;` |
|       89 |  810 | `		}else if( rc != SXRET_OK ){` |
|      ! 0 |  811 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  812 | `				"Invalid type for class constant inside class '%z'",&pClass->sDisp);` |
|      ! 0 |  813 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  814 | `				return SXERR_ABORT;` |
|        - |  815 | `			}` |
|      ! 0 |  816 | `			goto Synchronize;` |
|        - |  817 | `		}` |
|       89 |  818 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|       42 |  819 | `	}` |
|      362 |  820 | `loop:` |
|        - |  821 | ``	/* php 8 accepts EVERY reserved word as a class-constant name — `const list = 5`,`` |
|        - |  822 | ``	 * `const match`, `const function`, even `const true` — because a class constant is`` |
|        - |  823 | ``	 * addressed only through `C::name`, where no keyword can be ambiguous. The single`` |
|        - |  824 | ``	 * exception is `class`, reserved for `C::class`, and it gets its own message.`` |
|        - |  825 | `	 * (Method names already accept the whole set; this is the member-name side of the` |
|        - |  826 | ``	 * same rule. Global `const` is NOT the same rule: php rejects a reserved word there.)`` |
|        - |  827 | `	 * A keyword arrives as PH7_TK_KEYWORD, which this ID-only test rejected — so PHL` |
|        - |  828 | ``	 * accepted only the alpha-OPERATOR keywords (`const new`, `const and`), which the`` |
|        - |  829 | `	 * lexer marks PH7_TK_ID\|PH7_TK_OP, and that partial allow-list looked like a design. */` |
|      737 |  830 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  831 | `		/* Invalid constant name */` |
|      ! 0 |  832 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|      ! 0 |  833 | `		if( rc == SXERR_ABORT ){` |
|        - |  834 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  835 | `			return SXERR_ABORT;` |
|        - |  836 | `		}` |
|      ! 0 |  837 | `		goto Synchronize;` |
|        - |  838 | `	}` |
|        - |  839 | `	/* Peek constant name */` |
|      737 |  840 | `	pName = &pGen->pIn->sData;` |
|      737 |  841 | `	nNameLine = pGen->pIn->nLine;` |
|      732 |  842 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      408 |  843 | `		&& (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CLASS ){` |
|        3 |  844 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  845 | `			"A class constant must not be called 'class'; it is reserved for class name fetching");` |
|        3 |  846 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  847 | `			return SXERR_ABORT;` |
|        - |  848 | `		}` |
|        3 |  849 | `		goto Synchronize;` |
|        - |  850 | `	}` |
|        - |  851 | `	/* No reserved-CONSTANT check here: true/false/null are reserved GLOBAL constant` |
|        - |  852 | ``	 * names (compile_stmt.c still rejects `const true = 1`), but `C::true` addresses a`` |
|        - |  853 | `	 * class constant and php accepts the declaration like any other reserved word. The` |
|        - |  854 | `	 * member-name flag keeps the read from folding into the boolean literal. */` |
|      735 |  855 | `	if( (iFlags & PH7_CLASS_ATTR_FINAL) && iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|        - |  856 | ``		/* `final` says "no subclass may replace this", and a PRIVATE constant is not`` |
|        - |  857 | `		 * visible to one -- so php refuses the pair, naming the constant. Same shape` |
|        - |  858 | `		 * as the private-final METHOD rule one member over, except php makes this one` |
|        - |  859 | `		 * a fatal rather than a warning. Reported per NAME, which is php's order for` |
|        - |  860 | `		 * a multi-declaration too. */` |
|        4 |  861 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  862 | `			"Private constant %z::%z cannot be final as it is not visible to other classes",` |
|        1 |  863 | `			&pClass->sDisp,pName);` |
|        3 |  864 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  865 | `			return SXERR_ABORT;` |
|        - |  866 | `		}` |
|        3 |  867 | `		goto Synchronize;` |
|        - |  868 | `	}` |
|        - |  869 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|      733 |  870 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      134 |  871 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|       86 |  872 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|       43 |  873 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|       91 |  874 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  875 | `			return SXERR_ABORT;` |
|       91 |  876 | `		}else if( rc != SXRET_OK ){` |
|        3 |  877 | `			goto Synchronize;` |
|        - |  878 | `		}` |
|       42 |  879 | `	}` |
|        - |  880 | `	/* Advance the stream cursor */` |
|      731 |  881 | `	pGen->pIn++;` |
|      731 |  882 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|        - |  883 | `		/* Invalid declaration */` |
|      ! 0 |  884 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Expected '=' after class constant %z'",pName);` |
|      ! 0 |  885 | `		if( rc == SXERR_ABORT ){` |
|        - |  886 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  887 | `			return SXERR_ABORT;` |
|        - |  888 | `		}` |
|      ! 0 |  889 | `		goto Synchronize;` |
|        - |  890 | `	}` |
|      731 |  891 | `	pGen->pIn++; /* Jump the equal sign */` |
|        - |  892 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|        - |  893 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|        - |  894 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|        - |  895 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|      726 |  896 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|       81 |  897 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|        8 |  898 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  899 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|        2 |  900 | `			&pClass->sDisp,pName,&sTypeText);` |
|        6 |  901 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  902 | `			return SXERR_ABORT;` |
|        - |  903 | `		}` |
|        6 |  904 | `		goto Synchronize;` |
|        - |  905 | `	}` |
|        - |  906 | `	/* php's constant-expression rules, first offender wins (see` |
|        - |  907 | ``	 * PH7_GenStateConstExprError). A class/interface constant takes no `new`. */`` |
|        - |  908 | `	{` |
|      727 |  909 | `		const char *zCErr = PH7_GenStateConstExprError(pGen,0);` |
|      727 |  910 | `		if( zCErr ){` |
|       20 |  911 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"%s",zCErr);` |
|       20 |  912 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  913 | `				return SXERR_ABORT;` |
|        - |  914 | `			}` |
|       20 |  915 | `			goto Synchronize;` |
|        - |  916 | `		}` |
|        - |  917 | `	}` |
|        - |  918 | `	/* php: a class constant may not be redefined in the same class body. The` |
|        - |  919 | `	 * property path already guarded this; the constant path did not, so` |
|        - |  920 | ``	 * `class C{const X=1; const X=2;}` silently kept one of them. */`` |
|        - |  921 | ``	/* php keeps constants and properties in SEPARATE namespaces, so `const C` and`` |
|        - |  922 | ``	 * `public $C` coexist. PHL now stores them in disjoint tables (hConst / hAttr),`` |
|        - |  923 | `	 * so no collision — only a genuine constant redefinition is rejected below. */` |
|      711 |  924 | `	if( GenStateExtractConstant(pClass,pName) != 0 ){` |
|      ! 0 |  925 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  926 | `			"Cannot redefine class constant %z::%z",&pClass->sDisp,pName);` |
|      ! 0 |  927 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  928 | `			return SXERR_ABORT;` |
|        - |  929 | `		}` |
|      ! 0 |  930 | `		goto Synchronize;` |
|        - |  931 | `	}` |
|        - |  932 | `	/* Allocate a new class attribute */` |
|      711 |  933 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|      711 |  934 | `	if( pCons ){` |
|      711 |  935 | `		GenStateConsumeDoc(&(*pGen),&pCons->sDoc);` |
|      711 |  936 | `		if( GenStateConsumeAttrs(&(*pGen),&pCons->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  937 | `			return SXERR_ABORT;` |
|        - |  938 | `		}` |
|      711 |  939 | `		if( GenStateCheckAttrPlacement(&(*pGen),&pCons->aAttrs,nNameLine,16,16,0,0) == SXERR_ABORT ){` |
|      ! 0 |  940 | `			return SXERR_ABORT;` |
|        - |  941 | `		}` |
|      353 |  942 | `	}` |
|      711 |  943 | `	if( pCons == 0 ){` |
|      ! 0 |  944 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  945 | `		return SXERR_ABORT;` |
|        - |  946 | `	}` |
|      711 |  947 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       85 |  948 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|       40 |  949 | `	}` |
|        - |  950 | `	/* Swap bytecode container */` |
|      711 |  951 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      711 |  952 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|        - |  953 | `	/* Compile constant value.` |
|        - |  954 | `	 */` |
|      711 |  955 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      711 |  956 | `	if( rc == SXERR_EMPTY ){` |
|        3 |  957 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|        3 |  958 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  959 | `			return SXERR_ABORT;` |
|        - |  960 | `		}` |
|        1 |  961 | `	}` |
|        - |  962 | `	/* Emit the done instruction */` |
|      711 |  963 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      711 |  964 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      711 |  965 | `	if( rc == SXERR_ABORT ){` |
|        - |  966 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  967 | `		return SXERR_ABORT;` |
|        - |  968 | `	}` |
|        - |  969 | `	/* All done,install the constant */` |
|      711 |  970 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      711 |  971 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  972 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  973 | `		return SXERR_ABORT;` |
|        - |  974 | `	}` |
|      711 |  975 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - |  976 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|       10 |  977 | `		pGen->pIn++; /* Jump the comma */` |
|        - |  978 | `		/* A reserved word is a valid name for EVERY constant in the declaration, not` |
|        - |  979 | ``		 * just the first (`const list = 1, match = 2`) — same allow-list as the head. */`` |
|       10 |  980 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  981 | `			SyToken *pTok = pGen->pIn;` |
|      ! 0 |  982 | `			if( pTok >= pGen->pEnd ){` |
|      ! 0 |  983 | `				pTok--;` |
|      ! 0 |  984 | `			}` |
|      ! 0 |  985 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|        - |  986 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|      ! 0 |  987 | `				&pTok->sData,&pClass->sDisp);` |
|      ! 0 |  988 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  989 | `				return SXERR_ABORT;` |
|        - |  990 | `			}` |
|      ! 0 |  991 | `		}else{` |
|       10 |  992 | `			goto loop;` |
|        - |  993 | `		}` |
|      ! 0 |  994 | `	}` |
|      703 |  995 | `	SySetRelease(&aUnionAlts);` |
|      703 |  996 | `	return SXRET_OK;` |
|       13 |  997 | `Synchronize:` |
|       30 |  998 | `	SySetRelease(&aUnionAlts);` |
|        - |  999 | `	/* Synchronize with the first semi-colon */` |
|      156 | 1000 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      130 | 1001 | `		pGen->pIn++;` |
|        4 | 1002 | `	}` |
|       30 | 1003 | `	return SXERR_CORRUPT;` |
|      367 | 1004 | `}` |
|        - | 1005 | `/*` |
|        - | 1006 | ` * complie a class attribute or Properties in the PHP jargon.` |
|        - | 1007 | ` * According to the PHP language reference manual` |
|        - | 1008 | ` *  Properties` |
|        - | 1009 | ` *  Class member variables are called "properties". You may also see them referred` |
|        - | 1010 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|        - | 1011 | ` *  of this reference we will use "properties". They are defined by using one` |
|        - | 1012 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|        - | 1013 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|        - | 1014 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|        - | 1015 | ` *  and must not depend on run-time information in order to be evaluated.` |
|        - | 1016 | ` * Symisc eXtension.` |
|        - | 1017 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|        - | 1018 | ` *  the zend engine would allow only simple scalar value.` |
|        - | 1019 | ` *  Example:` |
|        - | 1020 | ` *   class Test{` |
|        - | 1021 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|        - | 1022 | ` *   };` |
|        - | 1023 | ` *   var_dump(TEST::myVar);` |
|        - | 1024 | ` *   Refer to the official documentation for more information on the powerful extension` |
|        - | 1025 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|        - | 1026 | ` */` |
|        - | 1027 | `/*` |
|        - | 1028 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|        - | 1029 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|        - | 1030 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|        - | 1031 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|        - | 1032 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|        - | 1033 | ` */` |
|     8092 | 1034 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|        5 | 1035 | `{` |
|     8097 | 1036 | `	SyToken *p = pStart;` |
|     8097 | 1037 | `	int bFirst = 1;` |
|     8097 | 1038 | `	if( p >= pEnd ) return 0;` |
|        - | 1039 | ``	/* Optional nullable `?` shorthand. */`` |
|     8097 | 1040 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|      147 | 1041 | `		p++;` |
|      147 | 1042 | `		if( p >= pEnd ) return 0;` |
|       71 | 1043 | `	}` |
|        - | 1044 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|        - | 1045 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|        - | 1046 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|        - | 1047 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|     4046 | 1048 | `	for(;;){` |
|     8193 | 1049 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|        - | 1050 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|        3 | 1051 | `			p++;` |
|        9 | 1052 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|        3 | 1053 | `			if( p >= pEnd ) return 0;` |
|        3 | 1054 | `			p++; /* skip ')' */` |
|        2 | 1055 | `		}else{` |
|        - | 1056 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|        - | 1057 | ``			 * then any `&`-joined intersection members. */`` |
|     8191 | 1058 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|     8191 | 1059 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 1060 | `				return 0;` |
|        - | 1061 | `			}` |
|        - | 1062 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|        - | 1063 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|        - | 1064 | `			 * may still appear at the initial dispatch site). */` |
|     8191 | 1065 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|     7945 | 1066 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|     7940 | 1067 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|     1569 | 1068 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|     6747 | 1069 | `					return 0;` |
|        - | 1070 | `				}` |
|      599 | 1071 | `			}` |
|     1449 | 1072 | `			p++;` |
|     1459 | 1073 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       11 | 1074 | `				p += 2;` |
|        1 | 1075 | `			}` |
|     2172 | 1076 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|     1455 | 1077 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        6 | 1078 | `				p++; /* skip '&' */` |
|        6 | 1079 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|        6 | 1080 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|        6 | 1081 | `				p++;` |
|        6 | 1082 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      ! 0 | 1083 | `					p += 2;` |
|      ! 0 | 1084 | `				}` |
|        2 | 1085 | `			}` |
|        - | 1086 | `		}` |
|     1451 | 1087 | `		bFirst = 0;` |
|     1446 | 1088 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      101 | 1089 | `			&& p->sData.zString[0] == '\|' ){` |
|      101 | 1090 | ``			p++; /* next `\|`-separated part */`` |
|      101 | 1091 | `			continue;` |
|        - | 1092 | `		}` |
|     1355 | 1093 | `		break;` |
|      ! 0 | 1094 | `	}` |
|     1355 | 1095 | `	if( p >= pEnd ) return 0;` |
|     1355 | 1096 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|     4051 | 1097 | `}` |
|        - | 1098 |  |
|        - | 1099 | `/*` |
|        - | 1100 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|        - | 1101 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|        - | 1102 | ` * if not). Recognized forms:` |
|        - | 1103 | ` *   ?Type, array, bool, int, float, string, object,` |
|        - | 1104 | ` *   self, parent, \Ns\ClassName, ClassName` |
|        - | 1105 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|        - | 1106 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|        - | 1107 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|        - | 1108 | ` * on unrecoverable error.` |
|        - | 1109 | ` *` |
|        - | 1110 | ` * When a type is parsed:` |
|        - | 1111 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|        - | 1112 | ` *   *pClass is set to the class name (for class types)` |
|        - | 1113 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|        - | 1114 | ` *   *pTypeText is set to the original text span of the type` |
|        - | 1115 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|        - | 1116 | ` */` |
|     1342 | 1117 | `static sxi32 GenStateParsePropertyType(` |
|        - | 1118 | `	ph7_gen_state *pGen,` |
|        - | 1119 | `	sxu32 *pnType,` |
|        - | 1120 | `	SyString *pClass,` |
|        - | 1121 | `	sxi32 *piTypeFlags,` |
|        - | 1122 | `	SyString *pTypeText,` |
|        - | 1123 | `	SySet *pAlts` |
|        5 | 1124 | `){` |
|     1347 | 1125 | `	sxi32 iFlags = 0;` |
|        - | 1126 | `	sxi32 rc;` |
|     1347 | 1127 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 1128 | `		return SXRET_OK;` |
|        - | 1129 | `	}` |
|        - | 1130 | `	/* If the first token is '$', there's no type */` |
|     1347 | 1131 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      ! 0 | 1132 | `		return SXRET_OK;` |
|        - | 1133 | `	}` |
|     1347 | 1134 | `	rc = GenStateParseUnionTypeDecl(` |
|      671 | 1135 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|        - | 1136 | `		PH7_CLASS_ATTR_NULLABLE,` |
|        - | 1137 | `		PH7_CLASS_ATTR_UNION,` |
|        - | 1138 | `		/* bAllowVoid */ 0,` |
|        - | 1139 | `		/* bParamCtx */ 0,` |
|     1342 | 1140 | `		pGen->pIn->nLine);` |
|     1347 | 1141 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1142 | `		return rc;` |
|        - | 1143 | `	}` |
|        - | 1144 | `	/* Verify next token is '$' (start of property name) */` |
|     1347 | 1145 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 | 1146 | `		return SXERR_SYNTAX;` |
|        - | 1147 | `	}` |
|     1347 | 1148 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|     1347 | 1149 | `	return SXRET_OK;` |
|      676 | 1150 | `}` |
|        - | 1151 |  |
|        - | 1152 | `/*` |
|        - | 1153 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|        - | 1154 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|        - | 1155 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|        - | 1156 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|        - | 1157 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|        - | 1158 | ` * by the type parser itself before reaching here.` |
|        - | 1159 | ` *` |
|        - | 1160 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|        - | 1161 | ` * use in the error message.` |
|        - | 1162 | ` */` |
|     1812 | 1163 | `static int GenStateIsDisallowedPropertyAtom(` |
|        - | 1164 | `	sxu32 nType,` |
|        - | 1165 | `	const SyString *pClass,` |
|        - | 1166 | `	const char **pzName,` |
|        - | 1167 | `	sxu32 *pnName)` |
|        5 | 1168 | `{` |
|        - | 1169 | `	const char *z;` |
|        - | 1170 | `	sxu32 n;` |
|     1817 | 1171 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|     1583 | 1172 | `		return 0;` |
|        - | 1173 | `	}` |
|      239 | 1174 | `	z = pClass->zString;` |
|      239 | 1175 | `	n = pClass->nByte;` |
|      239 | 1176 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|        8 | 1177 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|        - | 1178 | `	}` |
|        - | 1179 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|        - | 1180 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|        - | 1181 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|      233 | 1182 | `	return 0;` |
|      911 | 1183 | `}` |
|        - | 1184 |  |
|        - | 1185 | `/*` |
|        - | 1186 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|        - | 1187 | ` * constant) — the main atom plus any union alternatives — against the` |
|        - | 1188 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|        - | 1189 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|        - | 1190 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|        - | 1191 | ` * type T" vs "Class constant C::X cannot have type T").` |
|        - | 1192 | ` *` |
|        - | 1193 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|        - | 1194 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|        - | 1195 | ` */` |
|     1592 | 1196 | `PH7_PRIVATE sxi32 GenStateValidateMemberType(` |
|        - | 1197 | `	ph7_gen_state *pGen,` |
|        - | 1198 | `	ph7_class *pClass,` |
|        - | 1199 | `	const SyString *pMemberName,` |
|        - | 1200 | `	sxu32 nType,` |
|        - | 1201 | `	const SyString *pTypeClass,` |
|        - | 1202 | `	const SyString *pTypeText,` |
|        - | 1203 | `	SySet *pUnionAlts,` |
|        - | 1204 | `	const char *zErrFmt,` |
|        - | 1205 | `	sxu32 nLine)` |
|        5 | 1206 | `{` |
|     1597 | 1207 | `	const char *zBad = 0;` |
|     1597 | 1208 | `	sxu32 nBad = 0;` |
|        - | 1209 | `	SyString sFallback;` |
|        - | 1210 | `	const SyString *pBad;` |
|        - | 1211 | `	sxi32 rc;` |
|     1597 | 1212 | `	int bDisallowed = 0;` |
|     1597 | 1213 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|        6 | 1214 | `		bDisallowed = 1;` |
|     1595 | 1215 | `	}else if( pUnionAlts ){` |
|        - | 1216 | `		sxu32 i;` |
|      329 | 1217 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|      225 | 1218 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|      225 | 1219 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|        3 | 1220 | `				bDisallowed = 1;` |
|        3 | 1221 | `				break;` |
|        - | 1222 | `			}` |
|      114 | 1223 | `		}` |
|       53 | 1224 | `	}` |
|     1597 | 1225 | `	if( !bDisallowed ){` |
|     1591 | 1226 | `		return SXRET_OK;` |
|        - | 1227 | `	}` |
|        - | 1228 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|        - | 1229 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|        - | 1230 | `	 * canonical spelling if the type text is unavailable. */` |
|        8 | 1231 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|        8 | 1232 | `		pBad = pTypeText;` |
|        5 | 1233 | `	}else{` |
|      ! 0 | 1234 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|      ! 0 | 1235 | `		pBad = &sFallback;` |
|        - | 1236 | `	}` |
|       11 | 1237 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        3 | 1238 | `		zErrFmt,` |
|        3 | 1239 | `		&pClass->sDisp,pMemberName,pBad);` |
|        8 | 1240 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 1241 | `		return SXERR_ABORT;` |
|        - | 1242 | `	}` |
|        8 | 1243 | `	return SXERR_SYNTAX;` |
|      801 | 1244 | `}` |
|        - | 1245 | `/*` |
|        - | 1246 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|        - | 1247 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|        - | 1248 | ` * matched as a plain identifier in the class-member modifier position rather` |
|        - | 1249 | ` * than promoted to a lexer keyword.` |
|        - | 1250 | ` */` |
|  9994467 | 1251 | `PH7_PRIVATE int GenStateIsReadonly(SyToken *pTok)` |
|        5 | 1252 | `{` |
| 10971715 | 1253 | `	return (pTok->nType & PH7_TK_ID)` |
|  5969935 | 1254 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
| 10975567 | 1255 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|        5 | 1256 | `}` |
|        - | 1257 | `/*` |
|        - | 1258 | ``  * Detect an asymmetric set-visibility modifier `public(set)` / `protected(set)` `` |
|        - | 1259 | `` * / `private(set)` (PHP 8.4) starting at pTok. Returns the visibility keyword id`` |
|        - | 1260 | ` * (PH7_TKWRD_*) and sets *pnTok to the 4 tokens consumed, or 0 when not present` |
|        - | 1261 | ` * (a bare visibility keyword is NOT a set-modifier; the '(' 'set' ')' run is).` |
|        - | 1262 | ` */` |
|   256291 | 1263 | `PH7_PRIVATE sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok)` |
|        5 | 1264 | `{` |
|   256296 | 1265 | `	*pnTok = 0;` |
|   256291 | 1266 | `	if( &pTok[3] < pEnd` |
|   200908 | 1267 | `	 && (pTok->nType & PH7_TK_KEYWORD)` |
|   145671 | 1268 | `	 && (pTok[1].nType & PH7_TK_LPAREN)` |
|    72782 | 1269 | `	 && (pTok[2].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|       74 | 1270 | `	 && pTok[2].sData.nByte == sizeof("set")-1` |
|       73 | 1271 | `	 && SyStrnicmp(pTok[2].sData.zString,"set",sizeof("set")-1) == 0` |
|       77 | 1272 | `	 && (pTok[3].nType & PH7_TK_RPAREN) ){` |
|       77 | 1273 | `		sxi32 nKw = SX_PTR_TO_INT(pTok->pUserData);` |
|       77 | 1274 | `		if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PRIVATE \|\| nKw == PH7_TKWRD_PROTECTED ){` |
|       77 | 1275 | `			*pnTok = 4;` |
|       77 | 1276 | `			return nKw;` |
|        - | 1277 | `		}` |
|      ! 0 | 1278 | `	}` |
|   256224 | 1279 | `	return 0;` |
|   127987 | 1280 | `}` |
|        - | 1281 | `/* Map a set-visibility keyword to its PH7_CLASS_ATTR_* flag. */` |
|       70 | 1282 | `PH7_PRIVATE sxi32 GenStateSetVisFlag(sxi32 nKw)` |
|        5 | 1283 | `{` |
|       75 | 1284 | `	if( nKw == PH7_TKWRD_PRIVATE ){` |
|       53 | 1285 | `		return PH7_CLASS_ATTR_PRIVATE_SET;` |
|        - | 1286 | `	}` |
|       24 | 1287 | `	if( nKw == PH7_TKWRD_PROTECTED ){` |
|       22 | 1288 | `		return PH7_CLASS_ATTR_PROTECTED_SET;` |
|        - | 1289 | `	}` |
|        3 | 1290 | `	return PH7_CLASS_ATTR_PUBLIC_SET;` |
|       40 | 1291 | `}` |
|     3396 | 1292 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|        5 | 1293 | `{` |
|     3401 | 1294 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 1295 | `	ph7_class_attr *pAttr;` |
|        - | 1296 | `	SyString *pName;` |
|        - | 1297 | `	sxi32 rc;` |
|     3401 | 1298 | `	sxu32 nType = 0;` |
|        - | 1299 | `	SyString sTypeClass;` |
|        - | 1300 | `	SyString sTypeText;` |
|        - | 1301 | `	SySet aUnionAlts;` |
|     3401 | 1302 | `	sxi32 iTypeFlags = 0;` |
|     3401 | 1303 | `	int bRealLit = 0;` |
|     3401 | 1304 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|     3401 | 1305 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|     3401 | 1306 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        - | 1307 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|        - | 1308 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|        - | 1309 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|     3401 | 1310 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|       26 | 1311 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       11 | 1312 | `	}` |
|        - | 1313 | `	/* Extract visibility level */` |
|     3401 | 1314 | `	iProtection = GetProtectionLevel(iProtection);` |
|        - | 1315 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|     4072 | 1316 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     1347 | 1317 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|     1347 | 1318 | `		if( rc == SXERR_CORRUPT ){` |
|        - | 1319 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|      ! 0 | 1320 | `			goto Synchronize;` |
|     1347 | 1321 | `		}else if( rc == SXERR_SYNTAX ){` |
|      ! 0 | 1322 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1323 | `				"Invalid property type or declaration near '%z'",` |
|      ! 0 | 1324 | `				&pGen->pIn->sData);` |
|      ! 0 | 1325 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1326 | `				return SXERR_ABORT;` |
|        - | 1327 | `			}` |
|      ! 0 | 1328 | `			goto Synchronize;` |
|     1347 | 1329 | `		}else if( rc == SXERR_ABORT ){` |
|      ! 0 | 1330 | `			return SXERR_ABORT;` |
|        - | 1331 | `		}` |
|      671 | 1332 | `	}` |
|      ! 0 | 1333 | `loop:` |
|     3413 | 1334 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 | 1335 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Expected '$' at start of property name");` |
|      ! 0 | 1336 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1337 | `			return SXERR_ABORT;` |
|        - | 1338 | `		}` |
|      ! 0 | 1339 | `		goto Synchronize;` |
|        - | 1340 | `	}` |
|     3413 | 1341 | `	pGen->pIn++; /* Jump the dollar sign */` |
|     3413 | 1342 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|        - | 1343 | `		/* Invalid attribute name */` |
|      ! 0 | 1344 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|      ! 0 | 1345 | `		if( rc == SXERR_ABORT ){` |
|        - | 1346 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1347 | `			return SXERR_ABORT;` |
|        - | 1348 | `		}` |
|      ! 0 | 1349 | `		goto Synchronize;` |
|        - | 1350 | `	}` |
|        - | 1351 | `	/* Peek attribute name */` |
|     3413 | 1352 | `	pName = &pGen->pIn->sData;` |
|        - | 1353 | `	/* Advance the stream cursor */` |
|     3413 | 1354 | `	pGen->pIn++;` |
|     3413 | 1355 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/\|PH7_TK_OCB/*'{' hooks*/)) == 0 ){` |
|        - | 1356 | `		/* Invalid declaration */` |
|        - | 1357 | `		/* php reports the offending token here, expecting "," or ";". */` |
|        3 | 1358 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\",\" or \";\"");` |
|        3 | 1359 | `		if( rc == SXERR_ABORT ){` |
|        - | 1360 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1361 | `			return SXERR_ABORT;` |
|        - | 1362 | `		}` |
|        3 | 1363 | `		goto Synchronize;` |
|        - | 1364 | `	}` |
|        - | 1365 | `	/* Asymmetric-visibility rules (PHP 8.4): the property must be typed, and` |
|        - | 1366 | `	 * the read visibility must not be narrower than the set visibility. */` |
|     3411 | 1367 | `	if( iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET\|PH7_CLASS_ATTR_PUBLIC_SET) ){` |
|       69 | 1368 | `		const char *zAvErr = 0;` |
|      101 | 1369 | `		sxi32 iSetLevel = (iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? PH7_CLASS_PROT_PRIVATE` |
|       54 | 1370 | `			: (iFlags & PH7_CLASS_ATTR_PROTECTED_SET) ? PH7_CLASS_PROT_PROTECTED` |
|       11 | 1371 | `			: PH7_CLASS_PROT_PUBLIC;` |
|       69 | 1372 | `		if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 | 1373 | `			zAvErr = "Property with asymmetric visibility %z::$%z must have type";` |
|       69 | 1374 | `		}else if( iProtection > iSetLevel ){` |
|      ! 0 | 1375 | `			zAvErr = "Visibility of property %z::$%z must not be weaker than set visibility";` |
|      ! 0 | 1376 | `		}` |
|       69 | 1377 | `		if( zAvErr ){` |
|      ! 0 | 1378 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zAvErr,&pClass->sDisp,pName);` |
|      ! 0 | 1379 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1380 | `				return SXERR_ABORT;` |
|        - | 1381 | `			}` |
|      ! 0 | 1382 | `			goto Synchronize;` |
|        - | 1383 | `		}` |
|        - | 1384 | `		/* A set visibility equal to the read one is no asymmetry: php drops it, so` |
|        - | 1385 | ``		 * `private private(set)` is neither private(set) nor implicitly final.`` |
|        - | 1386 | `		 * public(set) stays -- it is what keeps a public readonly property from` |
|        - | 1387 | `		 * php's implicit protected(set). */` |
|       69 | 1388 | `		if( iSetLevel == iProtection && iSetLevel != PH7_CLASS_PROT_PUBLIC ){` |
|        5 | 1389 | `			iFlags &= ~(PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET);` |
|        2 | 1390 | `		}` |
|       32 | 1391 | `	}` |
|        - | 1392 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|        - | 1393 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|     3411 | 1394 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|      115 | 1395 | `		const char *zRoErr = 0;` |
|      115 | 1396 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        3 | 1397 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|      114 | 1398 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|        6 | 1399 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|      111 | 1400 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|        6 | 1401 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|        2 | 1402 | `		}` |
|      115 | 1403 | `		if( zRoErr ){` |
|       13 | 1404 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sDisp,pName);` |
|       13 | 1405 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1406 | `				return SXERR_ABORT;` |
|        - | 1407 | `			}` |
|       13 | 1408 | `			goto Synchronize;` |
|        - | 1409 | `		}` |
|       50 | 1410 | `	}` |
|        - | 1411 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|        - | 1412 | `	 * type atom or any union alternative. void/never are already rejected` |
|        - | 1413 | `	 * by the type parser. */` |
|     3401 | 1414 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     2021 | 1415 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|        - | 1416 | `			&sTypeText,` |
|     1344 | 1417 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      672 | 1418 | `			"Property %z::$%z cannot have type %z",nLine);` |
|     1349 | 1419 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1420 | `			return SXERR_ABORT;` |
|     1349 | 1421 | `		}else if( rc != SXRET_OK ){` |
|      ! 0 | 1422 | `			goto Synchronize;` |
|        - | 1423 | `		}` |
|      672 | 1424 | `	}` |
|        - | 1425 | `	/* Reject redeclaration (catches clash with an earlier promoted property).` |
|        - | 1426 | `	 * A same-name class CONSTANT is NOT a clash — php's separate namespaces let` |
|        - | 1427 | ``	 * `const C` and `public $C` coexist (stored in disjoint hConst / hAttr tables). */`` |
|     3401 | 1428 | `	if( GenStateExtractProperty(pClass,pName->zString,pName->nByte) != 0 ){` |
|        4 | 1429 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 1430 | `			"Cannot redeclare %z::$%z",&pClass->sDisp,pName);` |
|        3 | 1431 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1432 | `			return SXERR_ABORT;` |
|        - | 1433 | `		}` |
|        3 | 1434 | `		goto Synchronize;` |
|        - | 1435 | `	}` |
|        - | 1436 | `	/* php's constant-expression rules, first offender wins. A property default takes` |
|        - | 1437 | ``	 * no `new`. pGen->pIn is still on the '=' (the scan skips it and reads the`` |
|        - | 1438 | `	 * initializer non-destructively); no '=' means no default at all, and the scan` |
|        - | 1439 | `	 * then stops at the ';'/',' with nothing to report. */` |
|     3399 | 1440 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|     2277 | 1441 | `		const char *zCErr = PH7_GenStateConstExprError(pGen,0);` |
|     2277 | 1442 | `		if( zCErr ){` |
|        8 | 1443 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"%s",zCErr);` |
|        8 | 1444 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1445 | `				return SXERR_ABORT;` |
|        - | 1446 | `			}` |
|        8 | 1447 | `			goto Synchronize;` |
|        - | 1448 | `		}` |
|     1133 | 1449 | `	}` |
|        - | 1450 | `	/* Allocate a new class attribute */` |
|     3393 | 1451 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|     3393 | 1452 | `	if( pAttr ){` |
|     3393 | 1453 | `		GenStateConsumeDoc(&(*pGen),&pAttr->sDoc);` |
|     3393 | 1454 | `		if( GenStateConsumeAttrs(&(*pGen),&pAttr->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 1455 | `			return SXERR_ABORT;` |
|        - | 1456 | `		}` |
|        - | 1457 | `		/* Their placement is judged once the declaration has compiled: php` |
|        - | 1458 | `		 * validates a property's attributes LAST, after its hooks, and blames` |
|        - | 1459 | `		 * whatever line the hooks left behind (GenStateCompilePropertyHooks). */` |
|     1694 | 1460 | `	}` |
|     3393 | 1461 | `	if( pAttr == 0 ){` |
|      ! 0 | 1462 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1463 | `		return SXERR_ABORT;` |
|        - | 1464 | `	}` |
|     3393 | 1465 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     1347 | 1466 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      671 | 1467 | `	}` |
|     3393 | 1468 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|        - | 1469 | `		SySet *pInstrContainer;` |
|     2271 | 1470 | `		SyToken *pSavedDefEnd = pGen->pEnd;` |
|     2271 | 1471 | `		pGen->pIn++; /*Jump the equal sign */` |
|        - | 1472 | `		{` |
|        - | 1473 | `			/* Delimit the default expression: it ends at the declaration's` |
|        - | 1474 | `			 * ';'/',' or at a top-level '{' opening a PHP 8.4 hook list` |
|        - | 1475 | ``			 * (`public string $w = "init" { get => …; }`) — the expression`` |
|        - | 1476 | `			 * compiler would otherwise run into the hook tokens. */` |
|     2271 | 1477 | `			SyToken *pScan = pGen->pIn;` |
|     2271 | 1478 | `			sxi32 iNest = 0;` |
|     2271 | 1479 | ``			int bFuncSeen = 0; /* a `function` keyword stands at depth 0 */`` |
|    13579 | 1480 | `			while( pScan < pGen->pEnd ){` |
|    13574 | 1481 | `				if( (pScan->nType & PH7_TK_KEYWORD) && iNest <= 0` |
|       69 | 1482 | `					&& SX_PTR_TO_INT(pScan->pUserData) == PH7_TKWRD_FUNCTION ){` |
|        - | 1483 | `					/* The next depth-0 '{' is this CLOSURE's body, not a hook list:` |
|        - | 1484 | ``					 * `public $p = static function(){ … };` is php-legal (a static`` |
|        - | 1485 | `					 * closure is a constant expression) and its brace was read as` |
|        - | 1486 | `					 * the hook-list opener, so the default was truncated at` |
|        - | 1487 | ``					 * `static function()` and the declaration died three errors`` |
|        - | 1488 | `					 * deep. A class CONSTANT never showed it — only the property` |
|        - | 1489 | `					 * path carries a hook list at all. */` |
|        9 | 1490 | `					bFuncSeen = 1;` |
|    13575 | 1491 | `				}else if( pScan->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|      307 | 1492 | `					iNest++;` |
|    13420 | 1493 | `				}else if( pScan->nType & (PH7_TK_RPAREN\|PH7_TK_CSB) ){` |
|      307 | 1494 | `					iNest--;` |
|    13118 | 1495 | `				}else if( iNest <= 0 && (pScan->nType & PH7_TK_OCB) ){` |
|       97 | 1496 | `					if( !bFuncSeen ){` |
|       89 | 1497 | `						break; /* the hook list */` |
|        - | 1498 | `					}` |
|        9 | 1499 | `					bFuncSeen = 0;` |
|        9 | 1500 | `					pScan++;` |
|        9 | 1501 | `					PH7_DelimitNestedTokens(pScan,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pScan);` |
|        9 | 1502 | `					if( pScan >= pGen->pEnd ){` |
|      ! 0 | 1503 | `						break;` |
|        1 | 1504 | `					}` |
|        - | 1505 | `					/* land on the closing '}', the loop's pScan++ steps past it */` |
|    12879 | 1506 | `				}else if( iNest <= 0 && (pScan->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|     2187 | 1507 | `					break;` |
|        - | 1508 | `				}` |
|    11313 | 1509 | `				pScan++;` |
|        5 | 1510 | `			}` |
|     2271 | 1511 | `			pGen->pEnd = pScan;` |
|        - | 1512 | `		}` |
|     2271 | 1513 | `		bRealLit = GenStateConstInitIsRealLiteral(pGen);` |
|        - | 1514 | `		/* Swap bytecode container */` |
|     2271 | 1515 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     2271 | 1516 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|        - | 1517 | `		/* Compile attribute value. The default is a const-expression belonging to` |
|        - | 1518 | `		 * pClass (see iInMemberDefault) — __TRAIT__ in it reads pCurClass. */` |
|     2271 | 1519 | `		pGen->iInMemberDefault++;` |
|     2271 | 1520 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|     2271 | 1521 | `		pGen->iInMemberDefault--;` |
|     2271 | 1522 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 | 1523 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|      ! 0 | 1524 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1525 | `				return SXERR_ABORT;` |
|        - | 1526 | `			}` |
|      ! 0 | 1527 | `		}` |
|        - | 1528 | `		/* Emit the done instruction */` |
|     2271 | 1529 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|     2271 | 1530 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     2271 | 1531 | `		pGen->pIn = pGen->pEnd;   /* land exactly on the delimiter */` |
|     2271 | 1532 | `		pGen->pEnd = pSavedDefEnd;` |
|     1133 | 1533 | `	}` |
|        - | 1534 | `	/* All done,install the attribute */` |
|     3393 | 1535 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|     3393 | 1536 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1537 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1538 | `		return SXERR_ABORT;` |
|        - | 1539 | `	}` |
|     3393 | 1540 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - | 1541 | `		/* php holds a default its compiler FOLDED to the declared type at compile` |
|        - | 1542 | `		 * time, before the class is ever instantiated. A whole-valued real literal` |
|        - | 1543 | ``		 * (`= 1.0`) folds dual-flagged here; it is a float to php, so it is judged`` |
|        - | 1544 | `		 * as one -- the literal shape is the only signal that separates it from a` |
|        - | 1545 | ``		 * computed `4/2`, which php folds to an int. */`` |
|        - | 1546 | `		ph7_value sVal;` |
|      725 | 1547 | `		PH7_MemObjInit(pGen->pVm,&sVal);` |
|      725 | 1548 | `		if( PH7_ClassFoldDefault(pGen->pVm,&pAttr->aByteCode,&sVal) ){` |
|        - | 1549 | `			SyBlob sMsg;` |
|        - | 1550 | `			int bWasInt;` |
|      627 | 1551 | `			if( bRealLit && (sVal.iFlags & MEMOBJ_REAL) ){` |
|       37 | 1552 | `				sVal.iFlags &= ~MEMOBJ_INT;` |
|       16 | 1553 | `			}` |
|      627 | 1554 | `			bWasInt = (sVal.iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL)) == MEMOBJ_INT;` |
|      627 | 1555 | `			SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      627 | 1556 | `			if( VmTypedDefaultRefusal(pGen->pVm,pClass,pAttr,&sVal,&sMsg) ){` |
|       12 | 1557 | `				SyBlobNullAppend(&sMsg);` |
|       12 | 1558 | `				PH7_MemObjRelease(&sVal);` |
|       12 | 1559 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|       12 | 1560 | `				SyBlobRelease(&sMsg);` |
|       12 | 1561 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1562 | `					return SXERR_ABORT;` |
|        - | 1563 | `				}` |
|       12 | 1564 | `				goto Synchronize;` |
|        - | 1565 | `			}` |
|      619 | 1566 | `			SyBlobRelease(&sMsg);` |
|      619 | 1567 | `			if( bWasInt && (sVal.iFlags & MEMOBJ_REAL) ){` |
|        - | 1568 | ``				/* The check let an int into a float (`float $x = 1`) and php`` |
|        - | 1569 | `				 * STORES that conversion, so every reader of the default --` |
|        - | 1570 | `				 * reflection, get_class_vars(), the export -- sees float(1):` |
|        - | 1571 | `				 * cast it ahead of the trailing DONE. */` |
|       40 | 1572 | `				VmInstr *pDone = (VmInstr *)SySetPeek(&pAttr->aByteCode);` |
|       40 | 1573 | `				if( pDone && pDone->iOp == PH7_OP_DONE ){` |
|       40 | 1574 | `					VmInstr sDone = *pDone;` |
|       40 | 1575 | `					pDone->iOp = PH7_OP_CVT_REAL;` |
|       40 | 1576 | `					pDone->iP1 = 0;` |
|       40 | 1577 | `					pDone->iP2 = 0;` |
|       40 | 1578 | `					pDone->p3 = 0;` |
|       40 | 1579 | `					pDone->nAux = 0;` |
|       40 | 1580 | `					pDone->nSite = 0;` |
|       40 | 1581 | `					SySetPut(&pAttr->aByteCode,(const void *)&sDone);` |
|       18 | 1582 | `				}` |
|       18 | 1583 | `			}` |
|      307 | 1584 | `		}` |
|      717 | 1585 | `		PH7_MemObjRelease(&sVal);` |
|      356 | 1586 | `	}` |
|     3385 | 1587 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) ){` |
|        - | 1588 | ``		/* PHP 8.4 property hooks: `public [T] $x [= default] { get ...; set ...; }`.`` |
|        - | 1589 | `		 * The list ends the declaration at '}' — no trailing ';', no comma list. */` |
|      371 | 1590 | `		if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|        - | 1591 | ``			/* `readonly` promises one write, a hook decides what a write MEANS, and`` |
|        - | 1592 | `			 * php will not have both -- a rule the declaration screen never had, so` |
|        - | 1593 | ``			 * `public readonly int $p { get => 1; }` compiled here and does not in`` |
|        - | 1594 | `			 * php (in a readonly CLASS too, where the modifier is implied). */` |
|        5 | 1595 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1596 | `				"Hooked properties cannot be readonly");` |
|        5 | 1597 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1598 | `				return SXERR_ABORT;` |
|        - | 1599 | `			}` |
|        5 | 1600 | `			goto Synchronize;` |
|        - | 1601 | `		}` |
|      367 | 1602 | `		rc = GenStateCompilePropertyHooks(&(*pGen),pClass,pAttr);` |
|      367 | 1603 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1604 | `			return SXERR_ABORT;` |
|        - | 1605 | `		}` |
|      367 | 1606 | `		if( rc != SXRET_OK ){` |
|       52 | 1607 | `			goto Synchronize;` |
|        - | 1608 | `		}` |
|      317 | 1609 | `		SySetRelease(&aUnionAlts);` |
|      317 | 1610 | `		return SXRET_OK;` |
|        - | 1611 | `	}` |
|     3019 | 1612 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        - | 1613 | ``		/* php 8.4: `abstract` on a property requires a hook list (php's exact`` |
|        - | 1614 | `		 * wording differs per declaration site) */` |
|        3 | 1615 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        2 | 1616 | `			(pClass->iFlags & PH7_CLASS_INTERFACE)` |
|        - | 1617 | `				? "Interfaces may only include hooked properties"` |
|        - | 1618 | `				: "Only hooked properties may be declared abstract");` |
|        3 | 1619 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1620 | `			return SXERR_ABORT;` |
|        - | 1621 | `		}` |
|        3 | 1622 | `		goto Synchronize;` |
|        - | 1623 | `	}` |
|     3017 | 1624 | `	if( GenStateCheckAttrPlacement(&(*pGen),&pAttr->aAttrs,nLine,8,8,0,0) == SXERR_ABORT ){` |
|      ! 0 | 1625 | `		return SXERR_ABORT;` |
|        - | 1626 | `	}` |
|     3017 | 1627 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - | 1628 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|       15 | 1629 | `		pGen->pIn++; /* Jump the comma */` |
|       15 | 1630 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|      ! 0 | 1631 | `			SyToken *pTok = pGen->pIn;` |
|      ! 0 | 1632 | `			if( pTok >= pGen->pEnd ){` |
|      ! 0 | 1633 | `				pTok--;` |
|      ! 0 | 1634 | `			}` |
|      ! 0 | 1635 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|        - | 1636 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|      ! 0 | 1637 | `				&pTok->sData,&pClass->sDisp);` |
|      ! 0 | 1638 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1639 | `				return SXERR_ABORT;` |
|        - | 1640 | `			}` |
|      ! 0 | 1641 | `		}else{` |
|       15 | 1642 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       15 | 1643 | `				goto loop;` |
|        - | 1644 | `			}` |
|        - | 1645 | `		}` |
|      ! 0 | 1646 | `	}` |
|     3005 | 1647 | `	SySetRelease(&aUnionAlts);` |
|     3005 | 1648 | `	return SXRET_OK;` |
|       42 | 1649 | `Synchronize:` |
|        - | 1650 | `	/* Synchronize with the first semi-colon */` |
|      274 | 1651 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      190 | 1652 | `		pGen->pIn++;` |
|        4 | 1653 | `	}` |
|       88 | 1654 | `	SySetRelease(&aUnionAlts);` |
|       88 | 1655 | `	return SXERR_CORRUPT;` |
|     1703 | 1656 | `}` |
|        - | 1657 | `/*` |
|        - | 1658 | ` * php validates a magic method's DECLARATION at compile time` |
|        - | 1659 | ` * (zend_check_magic_method_implementation): the ENGINE builds the arguments and` |
|        - | 1660 | ` * calls these methods on its own, so a wrong shape is rejected where it is` |
|        - | 1661 | ` * written rather than discovered — or silently tolerated — at the dispatch.` |
|        - | 1662 | ` *` |
|        - | 1663 | ` * One row per magic name; its fields are the checks php makes for that name.` |
|        - | 1664 | ` * Arity is the first of them, in php's order — which is what decides the message` |
|        - | 1665 | ` * when a declaration breaks more than one of php's rules at once.` |
|        - | 1666 | ` */` |
|        - | 1667 | `typedef struct MagicMethodRule MagicMethodRule;` |
|        - | 1668 | `struct MagicMethodRule` |
|        - | 1669 | `{` |
|        - | 1670 | `	const char *zName; /* Magic method name */` |
|        - | 1671 | `	sxu32 nName;       /* Its length */` |
|        - | 1672 | `	int nArgs;         /* Declared arguments php requires, -1 when it does not check */` |
|        - | 1673 | `	int bStatic;       /* TRUE: must be static · FALSE: must NOT be static */` |
|        - | 1674 | `	int bPublic;       /* TRUE: must be public — a WARNING, and dispatched anyway */` |
|        - | 1675 | `	int bNoReturnType; /* TRUE: declaring ANY return type is a fatal */` |
|        - | 1676 | `};` |
|        - | 1677 | `#define MAGIC_METHOD_ROW(N,A,S,P,R) { N, sizeof(N)-1, A, S, P, R }` |
|        - | 1678 | `static const MagicMethodRule aMagicMethod[] = {` |
|        - | 1679 | `	MAGIC_METHOD_ROW("__construct",  -1, FALSE, FALSE, TRUE),` |
|        - | 1680 | `	MAGIC_METHOD_ROW("__destruct",    0, FALSE, FALSE, TRUE),` |
|        - | 1681 | `	MAGIC_METHOD_ROW("__clone",       0, FALSE, FALSE, FALSE),` |
|        - | 1682 | `	MAGIC_METHOD_ROW("__get",         1, FALSE, TRUE,  FALSE),` |
|        - | 1683 | `	MAGIC_METHOD_ROW("__set",         2, FALSE, TRUE,  FALSE),` |
|        - | 1684 | `	MAGIC_METHOD_ROW("__isset",       1, FALSE, TRUE,  FALSE),` |
|        - | 1685 | `	MAGIC_METHOD_ROW("__unset",       1, FALSE, TRUE,  FALSE),` |
|        - | 1686 | `	MAGIC_METHOD_ROW("__call",        2, FALSE, TRUE,  FALSE),` |
|        - | 1687 | `	MAGIC_METHOD_ROW("__callStatic",  2, TRUE,  TRUE,  FALSE),` |
|        - | 1688 | `	MAGIC_METHOD_ROW("__toString",    0, FALSE, TRUE,  FALSE),` |
|        - | 1689 | `	MAGIC_METHOD_ROW("__invoke",     -1, FALSE, TRUE,  FALSE),` |
|        - | 1690 | `	MAGIC_METHOD_ROW("__debugInfo",   0, FALSE, TRUE,  FALSE),` |
|        - | 1691 | `	MAGIC_METHOD_ROW("__serialize",   0, FALSE, TRUE,  FALSE),` |
|        - | 1692 | `	MAGIC_METHOD_ROW("__unserialize", 1, FALSE, TRUE,  FALSE),` |
|        - | 1693 | `	MAGIC_METHOD_ROW("__sleep",       0, FALSE, TRUE,  FALSE),` |
|        - | 1694 | `	MAGIC_METHOD_ROW("__wakeup",      0, FALSE, TRUE,  FALSE),` |
|        - | 1695 | `	MAGIC_METHOD_ROW("__set_state",   1, TRUE,  TRUE,  FALSE)` |
|        - | 1696 | `};` |
|        - | 1697 | `#undef MAGIC_METHOD_ROW` |
|        - | 1698 | `/*` |
|        - | 1699 | ` * Find the rule for a declared method name, or 0 when the name is not magic.` |
|        - | 1700 | `` * php matches method names case-insensitively everywhere, so `__GET` is `__get`;`` |
|        - | 1701 | ` * the two-underscore prefix test is php's own cheap reject.` |
|        - | 1702 | ` */` |
|   111372 | 1703 | `static const MagicMethodRule * GenStateMagicMethodRule(const SyString *pName)` |
|        5 | 1704 | `{` |
|        - | 1705 | `	sxu32 n;` |
|   111377 | 1706 | `	if( pName->nByte < sizeof("__x")-1 \|\| pName->zString[0] != '_' \|\| pName->zString[1] != '_' ){` |
|     4437 | 1707 | `		return 0;` |
|        - | 1708 | `	}` |
|  1164821 | 1709 | `	for( n = 0 ; n < SX_ARRAYSIZE(aMagicMethod) ; ++n ){` |
|  1164808 | 1710 | `		if( pName->nByte == aMagicMethod[n].nName` |
|   636482 | 1711 | `		 && SyStrnicmp(pName->zString,aMagicMethod[n].zName,aMagicMethod[n].nName) == 0 ){` |
|   106937 | 1712 | `			return &aMagicMethod[n];` |
|        - | 1713 | `		}` |
|   528943 | 1714 | `	}` |
|       10 | 1715 | `	return 0;` |
|    55691 | 1716 | `}` |
|        - | 1717 | `/*` |
|        - | 1718 | ` * TRUE when php requires this magic method to be PUBLIC: the rows it merely` |
|        - | 1719 | ` * WARNS about at the declaration and then dispatches regardless of what the` |
|        - | 1720 | ` * declaration said. The runtime's visibility gate reads this to let an` |
|        - | 1721 | ` * engine-built call through — a call the user WROTE stays denied.` |
|        - | 1722 | ` *` |
|        - | 1723 | `` * `__construct`/`__destruct`/`__clone` are deliberately not in the set: a`` |
|        - | 1724 | ` * private constructor is the singleton idiom, and php enforces those three at` |
|        - | 1725 | ` * the call like any other method.` |
|        - | 1726 | ` */` |
|   105392 | 1727 | `PH7_PRIVATE int PH7_MagicMethodMustBePublic(const SyString *pName)` |
|        5 | 1728 | `{` |
|   105397 | 1729 | `	const MagicMethodRule *pRule = GenStateMagicMethodRule(pName);` |
|   105397 | 1730 | `	return pRule != 0 && pRule->bPublic;` |
|        5 | 1731 | `}` |
|        - | 1732 | `/*` |
|        - | 1733 | ` * Enforce the rules of pRule against the declaration just parsed. pName is the` |
|        - | 1734 | ` * name AS WRITTEN — php quotes that spelling, not the canonical one.` |
|        - | 1735 | ` *` |
|        - | 1736 | ` * The diagnostic is FORMATTED, not reported: php decides these rules while the` |
|        - | 1737 | ` * signature is in hand (so the arity beats __toString's return-type rule) but` |
|        - | 1738 | ` * raises them only once the declaration has cleared the checks php makes` |
|        - | 1739 | ` * first — the redeclaration and abstract-placement rules, and any parse error` |
|        - | 1740 | ` * in the body php has already read. The caller reports the buffer at that` |
|        - | 1741 | ` * point.` |
|        - | 1742 | ` *` |
|        - | 1743 | ` * Returns the severity it wrote: E_ERROR, E_WARNING, or 0 for a clean` |
|        - | 1744 | ` * declaration.` |
|        - | 1745 | ` */` |
|     5980 | 1746 | `static sxi32 GenStateCheckMagicMethod(` |
|        - | 1747 | `	ph7_class *pClass,` |
|        - | 1748 | `	const SyString *pName,` |
|        - | 1749 | `	ph7_class_method *pMeth,` |
|        - | 1750 | `	char *zErr,` |
|        - | 1751 | `	int nErrBuf` |
|        - | 1752 | `	)` |
|        5 | 1753 | `{` |
|     5985 | 1754 | `	const MagicMethodRule *pRule = GenStateMagicMethodRule(pName);` |
|     5985 | 1755 | `	if( pRule == 0 ){` |
|     4445 | 1756 | `		return 0;` |
|        - | 1757 | `	}` |
|     1545 | 1758 | `	if( pRule->nArgs >= 0 ){` |
|        - | 1759 | `		/* php counts DECLARED parameters — an optional one counts` |
|        - | 1760 | ``		 * (`__destruct($a = null)` is rejected) and the variadic tail does not`` |
|        - | 1761 | ``		 * (`__clone(...$a)` declares zero and passes, `__get(...$a)` declares`` |
|        - | 1762 | `		 * zero where one is required and does not). */` |
|      751 | 1763 | `		sxu32 nDecl = SySetUsed(&pMeth->sFunc.aArgs);` |
|      751 | 1764 | `		sxu32 nGiven = 0;` |
|        - | 1765 | `		sxu32 n;` |
|     1395 | 1766 | `		for( n = 0 ; n < nDecl ; ++n ){` |
|      649 | 1767 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,n);` |
|      649 | 1768 | `			if( pArg && (pArg->iFlags & VM_FUNC_ARG_VARIADIC) == 0 ){` |
|      647 | 1769 | `				nGiven++;` |
|      321 | 1770 | `			}` |
|      327 | 1771 | `		}` |
|      751 | 1772 | `		if( nGiven != (sxu32)pRule->nArgs ){` |
|        9 | 1773 | `			if( pRule->nArgs == 0 ){` |
|        4 | 1774 | `				SyBufferFormat(zErr,nErrBuf,"Method %z::%z() cannot take arguments",` |
|        1 | 1775 | `					&pClass->sDisp,pName);` |
|        2 | 1776 | `			}else{` |
|        8 | 1777 | `				SyBufferFormat(zErr,nErrBuf,"Method %z::%z() must take exactly %d argument%s",` |
|        6 | 1778 | `					&pClass->sDisp,pName,pRule->nArgs,pRule->nArgs == 1 ? "" : "s");` |
|        - | 1779 | `			}` |
|        9 | 1780 | `			return E_ERROR;` |
|        - | 1781 | `		}` |
|        - | 1782 | `		/* None of the arguments the engine builds may be by-reference — there is` |
|        - | 1783 | `		 * no caller variable to write back to. php checks as many arguments as` |
|        - | 1784 | `		 * the rule counts, and the count above already skipped variadics, so` |
|        - | 1785 | `		 * this walk skips them the same way. */` |
|     1379 | 1786 | `		for( n = 0 ; n < nDecl ; ++n ){` |
|      641 | 1787 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,n);` |
|      641 | 1788 | `			if( pArg == 0 \|\| (pArg->iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        3 | 1789 | `				continue;` |
|        - | 1790 | `			}` |
|      639 | 1791 | `			if( pArg->iFlags & VM_FUNC_ARG_BY_REF ){` |
|        4 | 1792 | `				SyBufferFormat(zErr,nErrBuf,"Method %z::%z() cannot take arguments by reference",` |
|        1 | 1793 | `					&pClass->sDisp,pName);` |
|        3 | 1794 | `				return E_ERROR;` |
|        - | 1795 | `			}` |
|      321 | 1796 | `		}` |
|      369 | 1797 | `	}` |
|        - | 1798 | `	/* Static-ness. Whether the engine has a receiver for a magic method is not` |
|        - | 1799 | ``	 * the declaration's to choose: `__callStatic` and `__set_state` are reached`` |
|        - | 1800 | `	 * with a class and nothing else, every other row is reached through an` |
|        - | 1801 | `	 * object. PHL took the declaration at its word and then dispatched the` |
|        - | 1802 | ``	 * method anyway — a `static function __get()` ran with no `$this` at all,`` |
|        - | 1803 | `	 * so a hook property table or a lazy-loading accessor read whatever the` |
|        - | 1804 | `	 * unbound scope happened to hold. php checks this after the arity, which is` |
|        - | 1805 | ``	 * why `static function __get($a,$b)` reports the count first. */`` |
|     1537 | 1806 | `	if( pRule->bStatic != ((pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0) ){` |
|        8 | 1807 | `		SyBufferFormat(zErr,nErrBuf,"Method %z::%z() %s be static",` |
|        4 | 1808 | `			&pClass->sDisp,pName,pRule->bStatic ? "must" : "cannot");` |
|        6 | 1809 | `		return E_ERROR;` |
|        - | 1810 | `	}` |
|        - | 1811 | `	/* Visibility. This one is a WARNING: php names the declaration and then` |
|        - | 1812 | ``	 * dispatches the method anyway, because the engine calling `__get` is not`` |
|        - | 1813 | `	 * the outside world reaching for a private member. PHL was silent at the` |
|        - | 1814 | ``	 * declaration and threw `Call to private method C::__get()` at the ACCESS —`` |
|        - | 1815 | `	 * the one rule of this family that changed what a program php RUNS does,` |
|        - | 1816 | `	 * and it killed the script. The dispatch half is` |
|        - | 1817 | `	 * PH7_MagicMethodMustBePublic, read by the runtime visibility gate. */` |
|     1533 | 1818 | `	if( pRule->bPublic && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       66 | 1819 | `		SyBufferFormat(zErr,nErrBuf,"The magic method %z::%z() must have public visibility",` |
|       21 | 1820 | `			&pClass->sDisp,pName);` |
|       45 | 1821 | `		return E_WARNING;` |
|        - | 1822 | `	}` |
|        - | 1823 | ``	/* A return type on the two methods that have no return VALUE. `new C` is the`` |
|        - | 1824 | `	 * instance, never whatever __construct returned, and __destruct is called by` |
|        - | 1825 | `	 * the engine at a point with nowhere to put an answer — so php rejects any` |
|        - | 1826 | ``	 * declared type on either, `void` and `never` included, rather than let a`` |
|        - | 1827 | ``	 * declaration promise something no caller can read. (`__clone` is NOT in`` |
|        - | 1828 | ``	 * this row: `: void` on it is valid php.) PHL enforced the declared type at`` |
|        - | 1829 | ``	 * runtime instead, so `__construct(): int` raised a TypeError at every`` |
|        - | 1830 | `	 * instantiation — a diagnostic on the CALL for a mistake in the` |
|        - | 1831 | `	 * declaration. */` |
|     1486 | 1832 | `	if( pRule->bNoReturnType` |
|     1488 | 1833 | `	 && (pMeth->sFunc.nReturnType > 0` |
|      740 | 1834 | `	  \|\| SyStringLength(&pMeth->sFunc.sReturnClass) > 0` |
|      738 | 1835 | `	  \|\| SySetUsed(&pMeth->sFunc.aReturnUnion) > 0) ){` |
|        8 | 1836 | `		SyBufferFormat(zErr,nErrBuf,"Method %z::%z() cannot declare a return type",` |
|        2 | 1837 | `			&pClass->sDisp,pName);` |
|        6 | 1838 | `		return E_ERROR;` |
|        - | 1839 | `	}` |
|     1487 | 1840 | `	return 0;` |
|     2995 | 1841 | `}` |
|        - | 1842 | `/*` |
|        - | 1843 | ` * Raise the declaration diagnostic parked above (GenStateCheckMagicMethod's magic-method` |
|        - | 1844 | ` * rules, or the final-private one beside its call), once, and disarm it.` |
|        - | 1845 | ` * Suppressed when this declaration has already reported a fatal php decides` |
|        - | 1846 | ` * FIRST — a redeclaration, an abstract method in a non-abstract class, a parse` |
|        - | 1847 | ` * error in the body — since php stops at its own first fatal.` |
|        - | 1848 | ` */` |
|     5954 | 1849 | `static sxi32 GenStateRaiseMagicDiag(` |
|        - | 1850 | `	ph7_gen_state *pGen,` |
|        - | 1851 | `	sxi32 *pnSeverity,   /* IN/OUT: the parked severity, zeroed here */` |
|        - | 1852 | `	const char *zErr,` |
|        - | 1853 | `	sxu32 nLine,` |
|        - | 1854 | `	sxu32 nErrEntry      /* pGen->nErr when this declaration started */` |
|        - | 1855 | `	)` |
|        5 | 1856 | `{` |
|     5959 | 1857 | `	sxi32 rc = SXRET_OK;` |
|     5959 | 1858 | `	if( *pnSeverity != 0 ){` |
|       76 | 1859 | `		if( pGen->nErr == nErrEntry ){` |
|       76 | 1860 | `			rc = PH7_GenCompileError(pGen,*pnSeverity,nLine,"%s",zErr);` |
|       36 | 1861 | `		}` |
|       76 | 1862 | `		*pnSeverity = 0;` |
|       36 | 1863 | `	}` |
|     5959 | 1864 | `	return rc;` |
|        5 | 1865 | `}` |
|        - | 1866 | `/*` |
|        - | 1867 | ` * Compile a class method.` |
|        - | 1868 | ` *` |
|        - | 1869 | ` * Refer to the official documentation for more information` |
|        - | 1870 | ` * on the powerful extension introduced by the PH7 engine` |
|        - | 1871 | ` * to the OO subsystem such as full type hinting,method` |
|        - | 1872 | ` * overloading and many more.` |
|        - | 1873 | ` */` |
|     5990 | 1874 | `static sxi32 GenStateCompileClassMethod(` |
|        - | 1875 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 1876 | `	sxi32 iProtection,   /* Visibility level */` |
|        - | 1877 | `	sxi32 iFlags,        /* Configuration flags */` |
|        - | 1878 | `	int doBody,          /* TRUE to process method body */` |
|        - | 1879 | `	ph7_class *pClass    /* Class this method belongs */` |
|        - | 1880 | `	)` |
|        5 | 1881 | `{` |
|     5995 | 1882 | `	sxu32 nLine = pGen->pIn->nLine;` |
|     5995 | 1883 | `	sxu32 nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|     5995 | 1884 | `	sxu32 nErrEntry = pGen->nErr; /* Errors already reported when this declaration started */` |
|        - | 1885 | `	char zMagicErr[256];          /* Pending magic-method rule violation, reported at the end */` |
|     5995 | 1886 | `	sxi32 nMagicSeverity = 0;     /* E_ERROR / E_WARNING while zMagicErr is still unreported */` |
|     5995 | 1887 | `	int bMagicFatal = FALSE;      /* The parked diagnostic was a fatal: do not install the method */` |
|        - | 1888 | `	ph7_class_method *pMeth;` |
|        - | 1889 | `	sxi32 iFuncFlags;` |
|        - | 1890 | `	SyString *pName;` |
|        - | 1891 | `	SyToken *pEnd;` |
|        - | 1892 | `	sxi32 rc;` |
|        - | 1893 | `	/* Extract visibility level */` |
|     5995 | 1894 | `	iProtection = GetProtectionLevel(iProtection);` |
|     5995 | 1895 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     5995 | 1896 | `	iFuncFlags = 0;` |
|     5995 | 1897 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - | 1898 | `		/* Invalid method name */` |
|      ! 0 | 1899 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|      ! 0 | 1900 | `		if( rc == SXERR_ABORT ){` |
|        - | 1901 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1902 | `			return SXERR_ABORT;` |
|        - | 1903 | `		}` |
|      ! 0 | 1904 | `		goto Synchronize;` |
|        - | 1905 | `	}` |
|     5995 | 1906 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        - | 1907 | `		/* Return by reference,remember that */` |
|        6 | 1908 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|        - | 1909 | `		/* Jump the '&' token */` |
|        6 | 1910 | `		pGen->pIn++;` |
|        2 | 1911 | `	}` |
|     5995 | 1912 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - | 1913 | `		/* Invalid method name */` |
|      ! 0 | 1914 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|      ! 0 | 1915 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1916 | `			return SXERR_ABORT;` |
|        - | 1917 | `		}` |
|      ! 0 | 1918 | `		goto Synchronize;` |
|        - | 1919 | `	}` |
|        - | 1920 | `	/* Peek method name */` |
|     5995 | 1921 | `	pName = &pGen->pIn->sData;` |
|     5995 | 1922 | `	nLine = pGen->pIn->nLine;` |
|        - | 1923 | `	/* Jump the method name */` |
|     5995 | 1924 | `	pGen->pIn++;` |
|     5995 | 1925 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        - | 1926 | `		/* Abstract method. php has THREE answers here and this had one:` |
|        - | 1927 | `		 *` |
|        - | 1928 | `		 *  - an ENUM may not declare an abstract method at all, whatever its` |
|        - | 1929 | `		 *    visibility ("Enum method E::m() must not be abstract");` |
|        - | 1930 | `		 *  - a TRAIT may declare a PRIVATE one -- php 8.0 allowed it, and` |
|        - | 1931 | `		 *    symfony/messenger's BatchHandlerTrait is written that way, so a` |
|        - | 1932 | `		 *    refusal here stops a real component from compiling;` |
|        - | 1933 | ``		 *  - a class (abstract or not) refuses it, in php's own words: `Abstract`` |
|        - | 1934 | ``		 *    function C::m() cannot be declared private`, not this file's older`` |
|        - | 1935 | `		 *    "Access type for abstract method" sentence, which php keeps for an` |
|        - | 1936 | `		 *    INTERFACE member (and which the interface path already spells). */` |
|      271 | 1937 | `		if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|      ! 0 | 1938 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 1939 | `				"Enum method %z::%z() must not be abstract",&pClass->sDisp,pName);` |
|      ! 0 | 1940 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1941 | `				return SXERR_ABORT;` |
|        - | 1942 | `			}` |
|      266 | 1943 | `		}else if( iProtection == PH7_CLASS_PROT_PRIVATE` |
|      139 | 1944 | `		       && (pClass->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|      ! 0 | 1945 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1946 | `				"Abstract function %z::%z() cannot be declared private",` |
|      ! 0 | 1947 | `				&pClass->sDisp,pName);` |
|      ! 0 | 1948 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1949 | `				return SXERR_ABORT;` |
|        - | 1950 | `			}` |
|      ! 0 | 1951 | `		}` |
|        - | 1952 | `		/* Assemble method signature only */` |
|      271 | 1953 | `		doBody = FALSE;` |
|      133 | 1954 | `	}` |
|     5995 | 1955 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1956 | `		/* Syntax error */` |
|      ! 0 | 1957 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Expected '(' after method name '%z'",pName);` |
|      ! 0 | 1958 | `		if( rc == SXERR_ABORT ){` |
|        - | 1959 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1960 | `			return SXERR_ABORT;` |
|        - | 1961 | `		}` |
|      ! 0 | 1962 | `		goto Synchronize;` |
|        - | 1963 | `	}` |
|        - | 1964 | `	/* Allocate a new class_method instance */` |
|     5995 | 1965 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|     5995 | 1966 | `	if( pMeth == 0 ){` |
|      ! 0 | 1967 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 1968 | `		return SXERR_ABORT;` |
|        - | 1969 | `	}` |
|     5995 | 1970 | `	pMeth->sFunc.nLine = nKwLine;` |
|     5995 | 1971 | `	GenStateConsumeDoc(&(*pGen),&pMeth->sFunc.sDoc);` |
|     5995 | 1972 | `	if( GenStateConsumeAttrs(&(*pGen),&pMeth->sFunc.aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 1973 | `		return SXERR_ABORT;` |
|        - | 1974 | `	}` |
|     5995 | 1975 | `	if( GenStateCheckAttrPlacement(&(*pGen),&pMeth->sFunc.aAttrs,nKwLine,4,4,0,0) == SXERR_ABORT ){` |
|      ! 0 | 1976 | `		return SXERR_ABORT;` |
|        - | 1977 | `	}` |
|        - | 1978 | `	/* Jump the left parenthesis '(' */` |
|     5995 | 1979 | `	pGen->pIn++;` |
|     5995 | 1980 | `	pEnd = 0; /* cc warning */` |
|        - | 1981 | `	/* Delimit the method signature */` |
|     5995 | 1982 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     5995 | 1983 | `	if( pEnd >= pGen->pEnd ){` |
|        - | 1984 | `		/* Syntax error */` |
|        3 | 1985 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Missing ')' after method '%z' declaration",pName);` |
|        3 | 1986 | `		if( rc == SXERR_ABORT ){` |
|        - | 1987 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1988 | `			return SXERR_ABORT;` |
|        - | 1989 | `		}` |
|        3 | 1990 | `		goto Synchronize;` |
|        - | 1991 | `	}` |
|        - | 1992 | `	{` |
|     5993 | 1993 | `		int bIsCtor = 0;` |
|     5993 | 1994 | `		int bAbstractCtor = 0;` |
|        - | 1995 | `		/* Only __construct is the constructor (PHP-4 class-name constructors removed` |
|        - | 1996 | `		 * in 8.0): a method named like the class is a plain method, so promoted` |
|        - | 1997 | `		 * properties in it are rejected exactly as php does elsewhere. */` |
|     5988 | 1998 | `		if( pName->nByte == sizeof("__construct") - 1` |
|     3504 | 1999 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0 ){` |
|      697 | 2000 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       19 | 2001 | `				bAbstractCtor = 1;` |
|       11 | 2002 | `			}else{` |
|      681 | 2003 | `				bIsCtor = 1;` |
|        - | 2004 | `			}` |
|      346 | 2005 | `		}` |
|     5993 | 2006 | `		if( pGen->pIn < pEnd ){` |
|        - | 2007 | `			/* Collect method arguments */` |
|     2271 | 2008 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|     2271 | 2009 | `			if( rc == SXERR_ABORT ){` |
|        8 | 2010 | `				return SXERR_ABORT;` |
|        - | 2011 | `			}` |
|     1130 | 2012 | `		}` |
|        - | 2013 | `	}` |
|        - | 2014 | `	/* Point past ')' and parse optional return type ': type' */` |
|     5987 | 2015 | `	pGen->pIn = &pEnd[1];` |
|        - | 2016 | `	{` |
|     5987 | 2017 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|     5987 | 2018 | `		if( rcRt == SXERR_ABORT ){` |
|      ! 0 | 2019 | `			return SXERR_ABORT;` |
|     5987 | 2020 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|        2 | 2021 | `			goto Synchronize;` |
|        - | 2022 | `		}` |
|        - | 2023 | `	}` |
|        - | 2024 | `	/* php's #[\NoDiscard] declaration rules, which want the return type. */` |
|     6484 | 2025 | `	if( GenStateApplyNoDiscard(&(*pGen),&pMeth->sFunc,pClass,` |
|     5980 | 2026 | `			pName->nByte == sizeof("__construct")-1` |
|     3494 | 2027 | `			 && SyStrnicmp(pName->zString,"__construct",sizeof("__construct")-1) == 0)` |
|     2995 | 2028 | `		== SXERR_ABORT ){` |
|      ! 0 | 2029 | `		return SXERR_ABORT;` |
|        - | 2030 | `	}` |
|        - | 2031 | `	/* php's compile-time magic-method declaration rules, DECIDED here — with the` |
|        - | 2032 | `	 * signature in hand and before the __toString return-type rule below, which` |
|        - | 2033 | ``	 * is php's own order (`static function __toString($a): int` reports the`` |
|        - | 2034 | `	 * arity). Reported at the end of this function; see zMagicErr there. */` |
|     5985 | 2035 | `	nMagicSeverity = GenStateCheckMagicMethod(pClass,pName,pMeth,zMagicErr,(int)sizeof(zMagicErr));` |
|     5980 | 2036 | `	if( nMagicSeverity == 0` |
|     5951 | 2037 | `	 && (pMeth->iFlags & PH7_CLASS_ATTR_FINAL)` |
|     2976 | 2038 | `	 && pMeth->iProtection == PH7_CLASS_PROT_PRIVATE` |
|       29 | 2039 | `	 && !(pName->nByte == sizeof("__construct")-1` |
|        9 | 2040 | `	   && SyStrnicmp(pName->zString,"__construct",sizeof("__construct")-1) == 0) ){` |
|        - | 2041 | `		/* Not a magic rule, but the same KIND of rule and the same parking: php` |
|        - | 2042 | `		 * checks a declaration php itself decides the meaning of. A private method` |
|        - | 2043 | ``		 * is never overridden, so `final` on one says nothing — php WARNS here (8.0+)`` |
|        - | 2044 | `		 * and compiles the class. PHL was silent at the declaration and then fataled` |
|        - | 2045 | `		 * at the SUBCLASS that reused the name ("Cannot override final method"), a` |
|        - | 2046 | `		 * class php accepts; the inheritance half is in PH7_ClassInherit.` |
|        - | 2047 | `		 *` |
|        - | 2048 | `		 * The CONSTRUCTOR is php's one exemption, and it is a deliberate one:` |
|        - | 2049 | ``		 * `final private function __construct()` is the singleton idiom -- private`` |
|        - | 2050 | ``		 * to stop `new`, final to stop a subclass widening it back to public -- so`` |
|        - | 2051 | `		 * the modifier does say something there. Every other private method warns,` |
|        - | 2052 | ``		 * `__destruct`, `__clone` and a static one included. PHPUnit's TestSuite`` |
|        - | 2053 | `		 * declares exactly this and drew the warning on every single run. */` |
|       16 | 2054 | `		SyBufferFormat(zMagicErr,sizeof(zMagicErr),` |
|        - | 2055 | `			"Private methods cannot be final as they are never overridden by other classes");` |
|        - | 2056 | `		/* php's E_COMPILE_WARNING, not the E_WARNING the visibility rules above` |
|        - | 2057 | `		 * raise -- swept out of php 8.5, which passes the visibility ones to a` |
|        - | 2058 | `		 * user error handler and this one straight to default processing, and` |
|        - | 2059 | `		 * hides exactly one of the two under` |
|        - | 2060 | ``		 * `error_reporting(E_ALL & ~E_COMPILE_WARNING)`. */`` |
|       16 | 2061 | `		nMagicSeverity = 128 /* E_COMPILE_WARNING */;` |
|        7 | 2062 | `	}` |
|     5985 | 2063 | `	if( nMagicSeverity == E_ERROR ){` |
|        - | 2064 | `		/* Suppress the __toString rule below: php never reaches it on a` |
|        - | 2065 | `		 * declaration the magic rules already rejected. */` |
|       20 | 2066 | `		bMagicFatal = TRUE;` |
|       20 | 2067 | `		goto SkipToStringType;` |
|        - | 2068 | `	}` |
|        - | 2069 | `	/*` |
|        - | 2070 | ``	 * php gives __toString() an IMPLICIT `string` return type. That is what makes`` |
|        - | 2071 | ``	 * `return 42` coerce to "42" and `return null` / an array / an object / falling`` |
|        - | 2072 | `	 * off the end raise` |
|        - | 2073 | `	 *   C::__toString(): Return value must be of type string, X returned` |
|        - | 2074 | `	 * PHL enforced DECLARED return types only, so an undeclared __toString could` |
|        - | 2075 | `	 * answer anything and MemObjStringValue fell back to the "Object" placeholder` |
|        - | 2076 | `	 * for whatever was not a non-empty string. Installing the type here reuses the` |
|        - | 2077 | `	 * enforcement that already matches php byte for byte.` |
|        - | 2078 | `	 *` |
|        - | 2079 | `	 * sReturnTypeName is filled in as well, for two reasons: reflection reports the` |
|        - | 2080 | `	 * implicit type exactly as php does (hasReturnType() TRUE, getReturnType()` |
|        - | 2081 | `	 * "string" for an undeclared __toString), and the generator-return-type fatal` |
|        - | 2082 | ``	 * renders from it — so a __toString with a `yield` in it now reports php's`` |
|        - | 2083 | `	 * "Generator return type must be a supertype of Generator, string given".` |
|        - | 2084 | `	 *` |
|        - | 2085 | ``	 * Declaring any OTHER return type is php's own compile fatal, `?string`, a`` |
|        - | 2086 | ``	 * union, `mixed`, `static` and `void` included. (php checks`` |
|        - | 2087 | ``	 * "A void method must not return a value" FIRST when a `: void` __toString also`` |
|        - | 2088 | `	 * returns a value; PHL has no such check yet, so it reports this one instead —` |
|        - | 2089 | `	 * both reject, on doubly-invalid input only.)` |
|        - | 2090 | `	 */` |
|     5964 | 2091 | `	if( pName->nByte == sizeof("__toString")-1` |
|     3259 | 2092 | `	 && SyStrnicmp(pName->zString,"__toString",sizeof("__toString")-1) == 0 ){` |
|      197 | 2093 | `		ph7_vm_func *pTsFunc = &pMeth->sFunc;` |
|      197 | 2094 | `		int bTsDeclared = pTsFunc->nReturnType > 0 \|\| SySetUsed(&pTsFunc->aReturnUnion) > 0;` |
|      197 | 2095 | `		if( bTsDeclared ){` |
|      118 | 2096 | `			if( pTsFunc->nReturnType != MEMOBJ_STRING` |
|      117 | 2097 | `			 \|\| SySetUsed(&pTsFunc->aReturnUnion) > 0` |
|      121 | 2098 | `			 \|\| (pTsFunc->iFlags & VM_FUNC_RETURN_NULLABLE) ){` |
|        - | 2099 | `				/* php raises this one AFTER the magic rules, so a parked` |
|        - | 2100 | `				 * visibility warning is php's first line here rather than a` |
|        - | 2101 | `				 * casualty of the fatal about to be counted. */` |
|        6 | 2102 | `				if( GenStateRaiseMagicDiag(pGen,&nMagicSeverity,zMagicErr,` |
|        6 | 2103 | `						nKwLine,nErrEntry) == SXERR_ABORT ){` |
|      ! 0 | 2104 | `					return SXERR_ABORT;` |
|        - | 2105 | `				}` |
|        8 | 2106 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2107 | `					"%z::%z(): Return type must be string when declared",` |
|        2 | 2108 | `					&pClass->sDisp,pName);` |
|        6 | 2109 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2110 | `					return SXERR_ABORT;` |
|        - | 2111 | `				}` |
|        6 | 2112 | `				goto Synchronize;` |
|        - | 2113 | `			}` |
|       62 | 2114 | `		}else{` |
|       79 | 2115 | `			char *zTsType = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        - | 2116 | `				"string",sizeof("string")-1);` |
|       79 | 2117 | `			pTsFunc->nReturnType = MEMOBJ_STRING;` |
|       79 | 2118 | `			if( zTsType ){` |
|       79 | 2119 | `				SyStringInitFromBuf(&pTsFunc->sReturnTypeName,zTsType,sizeof("string")-1);` |
|       37 | 2120 | `			}` |
|        - | 2121 | `		}` |
|       94 | 2122 | `	}` |
|      ! 0 | 2123 | `SkipToStringType:` |
|        - | 2124 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|        - | 2125 | `	 * property init/typecheck is handled by the generic typed-property path` |
|        - | 2126 | `	 * since we mint real ph7_class_attr entries. */` |
|        - | 2127 | `	{` |
|     5981 | 2128 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|        - | 2129 | `		sxu32 i;` |
|     9165 | 2130 | `		for( i = 0; i < nArg; i++ ){` |
|     3199 | 2131 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|        - | 2132 | `			ph7_class_attr *pAttr;` |
|     3199 | 2133 | `			sxi32 iAttrFlags = 0;` |
|        - | 2134 | `			int bArgTyped;` |
|     3199 | 2135 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|     2969 | 2136 | `				continue;` |
|        - | 2137 | `			}` |
|        - | 2138 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|        - | 2139 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|        - | 2140 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|      196 | 2141 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|      268 | 2142 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|      235 | 2143 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        3 | 2144 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2145 | `					"Cannot declare variadic promoted property");` |
|        3 | 2146 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2147 | `					return SXERR_ABORT;` |
|        - | 2148 | `				}` |
|        3 | 2149 | `				goto Synchronize;` |
|        - | 2150 | `			}` |
|        - | 2151 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|        - | 2152 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|        - | 2153 | `			 * appear as an alternative of a union type. */` |
|      233 | 2154 | `			if( bArgTyped ){` |
|      248 | 2155 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|      162 | 2156 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|      162 | 2157 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|       81 | 2158 | `					"Property %z::$%z cannot have type %z",nLine);` |
|      167 | 2159 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2160 | `					return SXERR_ABORT;` |
|      167 | 2161 | `				}else if( rc != SXRET_OK ){` |
|        6 | 2162 | `					goto Synchronize;` |
|        - | 2163 | `				}` |
|       79 | 2164 | `			}` |
|        - | 2165 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|      229 | 2166 | `			if( GenStateExtractProperty(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|        4 | 2167 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 2168 | `					"Cannot redeclare %z::$%z",&pClass->sDisp,&pArg->sName);` |
|        3 | 2169 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2170 | `					return SXERR_ABORT;` |
|        - | 2171 | `				}` |
|        3 | 2172 | `				goto Synchronize;` |
|        - | 2173 | `			}` |
|      227 | 2174 | `			if( bArgTyped ){` |
|      161 | 2175 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|       78 | 2176 | `			}` |
|      227 | 2177 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|        3 | 2178 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|        1 | 2179 | `			}` |
|      227 | 2180 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|       11 | 2181 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|        4 | 2182 | `			}` |
|      227 | 2183 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|        - | 2184 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|        - | 2185 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|       50 | 2186 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|        4 | 2187 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 2188 | `						"Readonly property %z::$%z must have type",&pClass->sDisp,&pArg->sName);` |
|        3 | 2189 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 2190 | `						return SXERR_ABORT;` |
|        - | 2191 | `					}` |
|        3 | 2192 | `					goto Synchronize;` |
|        - | 2193 | `				}` |
|       48 | 2194 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       22 | 2195 | `			}` |
|      225 | 2196 | `			if( pArg->iFlags & VM_FUNC_ARG_FINAL ){` |
|        - | 2197 | ``				/* PHP 8.4's `final` on a promoted property. No "final and private"`` |
|        - | 2198 | `				 * screen here: php refuses that pair in a CLASS BODY and accepts it` |
|        - | 2199 | ``				 * on a promoted parameter (`final private int $p` reflects as`` |
|        - | 2200 | `				 * modifiers 36), which is php's own asymmetry, not a gap. */` |
|        7 | 2201 | `				iAttrFlags \|= PH7_CLASS_ATTR_FINAL;` |
|        3 | 2202 | `			}` |
|      225 | 2203 | `			if( pArg->iFlags & (VM_FUNC_ARG_PRIV_SET\|VM_FUNC_ARG_PROT_SET) ){` |
|        - | 2204 | `				/* Asymmetric set-visibility on a promoted property (PHP 8.4) */` |
|        5 | 2205 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 | 2206 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2207 | `						"Property with asymmetric visibility %z::$%z must have type",` |
|      ! 0 | 2208 | `						&pClass->sDisp,&pArg->sName);` |
|      ! 0 | 2209 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 2210 | `						return SXERR_ABORT;` |
|        - | 2211 | `					}` |
|      ! 0 | 2212 | `					goto Synchronize;` |
|        - | 2213 | `				}` |
|        - | 2214 | `				/* One equal to the read visibility is dropped, as in a class body. */` |
|        5 | 2215 | `				if( (pArg->iFlags & VM_FUNC_ARG_PRIV_SET) ){` |
|        5 | 2216 | `					if( pArg->iPromoteVis != PH7_CLASS_PROT_PRIVATE ){` |
|        5 | 2217 | `						iAttrFlags \|= PH7_CLASS_ATTR_PRIVATE_SET;` |
|        3 | 2218 | `					}` |
|        2 | 2219 | `				}else if( pArg->iPromoteVis != PH7_CLASS_PROT_PROTECTED ){` |
|      ! 0 | 2220 | `					iAttrFlags \|= PH7_CLASS_ATTR_PROTECTED_SET;` |
|      ! 0 | 2221 | `				}` |
|        2 | 2222 | `			}` |
|      225 | 2223 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|      225 | 2224 | `			if( pAttr == 0 ){` |
|      ! 0 | 2225 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2226 | `				return SXERR_ABORT;` |
|        - | 2227 | `			}` |
|      225 | 2228 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|      161 | 2229 | `				pAttr->nType = pArg->nType;` |
|      161 | 2230 | `				pAttr->sClass = pArg->sClass;` |
|      161 | 2231 | `				pAttr->sTypeName = pArg->sTypeName;` |
|      161 | 2232 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|        - | 2233 | `					sxu32 k;` |
|       27 | 2234 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|       19 | 2235 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|       19 | 2236 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|       11 | 2237 | `					}` |
|        4 | 2238 | `				}` |
|       78 | 2239 | `			}` |
|        - | 2240 | ``			/* A promoted parameter's `#[...]` belongs to BOTH members in php: the`` |
|        - | 2241 | `			 * ReflectionParameter reports it and so does the ReflectionProperty,` |
|        - | 2242 | ``			 * which is what makes `#[\Override] public $p` in a constructor`` |
|        - | 2243 | `			 * signature a PROPERTY claim. The records are shared, not copied --` |
|        - | 2244 | `			 * the parameter owns them for the VM's lifetime. */` |
|        - | 2245 | `			{` |
|      225 | 2246 | `				ph7_attribute *aSrc = (ph7_attribute *)SySetBasePtr(&pArg->aAttrs);` |
|        - | 2247 | `				sxu32 k;` |
|      243 | 2248 | `				for( k = 0 ; k < SySetUsed(&pArg->aAttrs) ; k++ ){` |
|       21 | 2249 | `					SySetPut(&pAttr->aAttrs,(const void *)&aSrc[k]);` |
|       12 | 2250 | `				}` |
|        - | 2251 | `			}` |
|      225 | 2252 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|      225 | 2253 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 2254 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2255 | `				return SXERR_ABORT;` |
|        - | 2256 | `			}` |
|      115 | 2257 | `		}` |
|        - | 2258 | `	}` |
|     5971 | 2259 | `	if( doBody ){` |
|        - | 2260 | `		/* Compile method body */` |
|     5707 | 2261 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|     5707 | 2262 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2263 | `			return SXERR_ABORT;` |
|        - | 2264 | `		}` |
|        - | 2265 | `		/* The cursor sits just past the body's closing brace */` |
|     5707 | 2266 | `		pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|     2856 | 2267 | `	}else{` |
|        - | 2268 | `		/* Abstract/interface method: declaration ends at the ';' */` |
|      269 | 2269 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) ){` |
|      263 | 2270 | `			pMeth->sFunc.nEndLine = pGen->pIn->nLine;` |
|      129 | 2271 | `		}` |
|        - | 2272 | `		{` |
|        - | 2273 | `			/* php compiles an abstract method to an EMPTY body, and two doors run it` |
|        - | 2274 | ``			 * rather than refusing: a literal `X::__construct()` naming an abstract`` |
|        - | 2275 | `			 * (or interface) constructor, and a ReflectionMethod::getClosure() closure.` |
|        - | 2276 | `			 * The parameters bind and check, and the call answers NULL. Every other` |
|        - | 2277 | `			 * door refuses on the flag before it would reach this. */` |
|      269 | 2278 | `			SySet *pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      269 | 2279 | `			PH7_VmSetByteCodeContainer(pGen->pVm,&pMeth->sFunc.aByteCode);` |
|      269 | 2280 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|      269 | 2281 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 2282 | `		}` |
|        - | 2283 | `		/* Only method signature is allowed */` |
|      269 | 2284 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|        - | 2285 | `			/* php words this as the declaration's problem rather than a missing` |
|        - | 2286 | `			 * token -- an abstract method (an interface's included, which is` |
|        - | 2287 | `			 * abstract by being one) is a promise, and a body makes it something` |
|        - | 2288 | `			 * else. The two kinds get the two nouns php uses. */` |
|        9 | 2289 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       12 | 2290 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2291 | `					"%s function %z::%z() cannot contain body",` |
|        6 | 2292 | `					(pClass->iFlags & PH7_CLASS_INTERFACE) ? "Interface" : "Abstract",` |
|        3 | 2293 | `					&pClass->sDisp,pName);` |
|        9 | 2294 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2295 | `					return SXERR_ABORT;` |
|        - | 2296 | `				}` |
|        9 | 2297 | `				return SXERR_CORRUPT;` |
|        - | 2298 | `			}` |
|      ! 0 | 2299 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|      ! 0 | 2300 | `				"Expected ';' after method signature '%z'",pName);` |
|      ! 0 | 2301 | `				if( rc == SXERR_ABORT ){` |
|        - | 2302 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 | 2303 | `					return SXERR_ABORT;` |
|        - | 2304 | `				}` |
|      ! 0 | 2305 | `				return SXERR_CORRUPT;` |
|        - | 2306 | `			}` |
|        - | 2307 | `	}` |
|        - | 2308 | `	/* php: an abstract method in a class that was not DECLARED abstract is a fatal,` |
|        - | 2309 | `	 * and it names the method -- distinct from the "contains N abstract methods"` |
|        - | 2310 | `	 * wording php uses for an unimplemented INHERITED one, which` |
|        - | 2311 | `	 * GenStateCheckAbstractMethods already handles. Interfaces and traits may carry` |
|        - | 2312 | `	 * abstract methods freely. */` |
|     5960 | 2313 | `	if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT)` |
|     3114 | 2314 | `	 && (pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0 ){` |
|        4 | 2315 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2316 | `			"Class %z declares abstract method %z() and must therefore be declared abstract",` |
|        1 | 2317 | `			&pClass->sDisp,pName);` |
|        3 | 2318 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2319 | `			return SXERR_ABORT;` |
|        - | 2320 | `		}` |
|        3 | 2321 | `		return SXRET_OK;` |
|        - | 2322 | `	}` |
|        - | 2323 | `	/* php: two methods of the same name in one class body is a fatal, and the` |
|        - | 2324 | ``	 * comparison is case-INSENSITIVE (hMethod now matches that way), so `f` and`` |
|        - | 2325 | ``	 * `F` collide. php names the offending declaration with the spelling used at`` |
|        - | 2326 | `	 * the SECOND site. */` |
|     5963 | 2327 | `	if( PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte) != 0 ){` |
|       14 | 2328 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        4 | 2329 | `			"Cannot redeclare %z::%z()",&pClass->sDisp,pName);` |
|       10 | 2330 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2331 | `			return SXERR_ABORT;` |
|        - | 2332 | `		}` |
|       10 | 2333 | `		return SXRET_OK;` |
|        - | 2334 | `	}` |
|        - | 2335 | `	/* The magic-method rule this declaration broke (decided above, with the` |
|        - | 2336 | `	 * signature in hand). It is raised HERE because php raises it last of the` |
|        - | 2337 | `	 * declaration's fatals: a redeclaration, an abstract method in a class that` |
|        - | 2338 | `	 * is not abstract, and any parse error inside the body php has already read` |
|        - | 2339 | `	 * all win — and each of them has, by now, either returned or bumped nErr.` |
|        - | 2340 | `	 * php stops at its first fatal, so one is all this declaration reports. The` |
|        - | 2341 | ``	 * line is the `function` KEYWORD's, which is where php points once a`` |
|        - | 2342 | `	 * signature wraps across lines. */` |
|     5955 | 2343 | `	if( GenStateRaiseMagicDiag(pGen,&nMagicSeverity,zMagicErr,nKwLine,nErrEntry) == SXERR_ABORT ){` |
|      ! 0 | 2344 | `		return SXERR_ABORT;` |
|        - | 2345 | `	}` |
|     5955 | 2346 | `	if( bMagicFatal ){` |
|        - | 2347 | `		/* Never install a method php refused to declare. A WARNING falls through:` |
|        - | 2348 | `		 * php keeps the method and calls it. */` |
|       20 | 2349 | `		return SXRET_OK;` |
|        - | 2350 | `	}` |
|        - | 2351 | `	/* All done,install the method */` |
|     5939 | 2352 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|     5939 | 2353 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 2354 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2355 | `		return SXERR_ABORT;` |
|        - | 2356 | `	}` |
|     5939 | 2357 | `	return SXRET_OK;` |
|        9 | 2358 | `Synchronize:` |
|        - | 2359 | `	/* Synchronize with the first semi-colon */` |
|       58 | 2360 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       40 | 2361 | `		pGen->pIn++;` |
|        4 | 2362 | `	}` |
|       22 | 2363 | `	return SXERR_CORRUPT;` |
|     3000 | 2364 | `}` |
|        - | 2365 | `/*` |
|        - | 2366 | ` * php's member-modifier RUN and the screens on it.` |
|        - | 2367 | ` *` |
|        - | 2368 | ` * Everything a class/trait/interface member may carry in front of the` |
|        - | 2369 | ``  * declaration it modifies -- the read visibility, an asymmetric `(set)` `` |
|        - | 2370 | ``  * visibility, `static`, `abstract`, `final` and the context-sensitive `readonly` `` |
|        - | 2371 | ` * -- in ANY order and each at most once. They are a SET in php's grammar, so` |
|        - | 2372 | `` * `final public static int $p` and `static final public $p` are one declaration`` |
|        - | 2373 | ` * each.` |
|        - | 2374 | ` *` |
|        - | 2375 | ` * The three body loops used to read ONE modifier per branch and then re-enter` |
|        - | 2376 | ` * their keyword ladder, which works only while every branch happens to be` |
|        - | 2377 | `` * reachable from every other: `static final public $p` fell out of the chain`` |
|        - | 2378 | `` * with "Unexpected token 'final'", and the `final` branch only ever led to a`` |
|        - | 2379 | ` * method or a constant -- so PHP 8.4's final PROPERTY was a parse error in every` |
|        - | 2380 | ` * one of its spellings, promoted parameter included. Reading the whole run in` |
|        - | 2381 | ` * one place makes the order irrelevant and gives the duplicate/combination rules` |
|        - | 2382 | `` * a single home; the caller dispatches on the token the run stops at (`const`,`` |
|        - | 2383 | `` * `function`, `var`, a type, a `$name`).`` |
|        - | 2384 | ` */` |
|        - | 2385 | ``#define GEN_MEMBER_PROP   0  /* `[type] $name`  */`` |
|        - | 2386 | ``#define GEN_MEMBER_CONST  1  /* `const NAME`    */`` |
|        - | 2387 | ``#define GEN_MEMBER_METHOD 2  /* `function name` */`` |
|        - | 2388 | ``#define GEN_MEMBER_VAR    3  /* the pre-5.0 `var $name` spelling */`` |
|        - | 2389 | `typedef struct GenMemberMods GenMemberMods;` |
|        - | 2390 | `struct GenMemberMods` |
|        - | 2391 | `{` |
|        - | 2392 | `	sxi32 iProtection;  /* read-visibility keyword; php's default is public */` |
|        - | 2393 | `	sxi32 iFlags;       /* PH7_CLASS_ATTR_* collected from the run */` |
|        - | 2394 | ``	sxi32 nSetVis;      /* the `(set)` visibility keyword, when one was written */`` |
|        - | 2395 | `	sxu32 nLine;        /* line the run starts on -- where php reports its refusals */` |
|        - | 2396 | `	int bAny;           /* TRUE once anything at all was consumed */` |
|        - | 2397 | `	int bVis,bSetVis,bStatic,bAbstract,bFinal,bReadonly;` |
|        - | 2398 | `};` |
|        - | 2399 | `/*` |
|        - | 2400 | ` * Read the run. A modifier written twice is php's own compile-time fatal, worded` |
|        - | 2401 | ` * per modifier -- and the two VISIBILITY kinds share one sentence, which is also` |
|        - | 2402 | `` * what php says for two DIFFERENT ones (`public private $p`).`` |
|        - | 2403 | ` *` |
|        - | 2404 | ` * Returns SXRET_OK, SXERR_SYNTAX when a rule above was reported (the caller` |
|        - | 2405 | ` * abandons the member), or SXERR_ABORT when the error budget is spent.` |
|        - | 2406 | ` */` |
|    10148 | 2407 | `static sxi32 GenStateReadMemberMods(ph7_gen_state *pGen,GenMemberMods *pMods)` |
|        5 | 2408 | `{` |
|    10153 | 2409 | `	pMods->iProtection = PH7_TKWRD_PUBLIC;` |
|    10153 | 2410 | `	pMods->iFlags = 0;` |
|    10153 | 2411 | `	pMods->nSetVis = 0;` |
|    10153 | 2412 | `	pMods->nLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0;` |
|    10153 | 2413 | `	pMods->bAny = pMods->bVis = pMods->bSetVis = 0;` |
|    10153 | 2414 | `	pMods->bStatic = pMods->bAbstract = pMods->bFinal = pMods->bReadonly = 0;` |
|    20063 | 2415 | `	while( pGen->pIn < pGen->pEnd ){` |
|    20063 | 2416 | `		const char *zTwice = 0;  /* the modifier php names; "" = the access-type sentence */` |
|    20063 | 2417 | `		int nSetTok = 0;` |
|        - | 2418 | `		sxi32 nSetVis;` |
|    20063 | 2419 | `		if( GenStateIsReadonly(pGen->pIn) ){` |
|        - | 2420 | ``			/* `readonly` is not a reserved word, so it arrives as a plain ID; at`` |
|        - | 2421 | `			 * modifier position it is always the modifier -- which is why php's` |
|        - | 2422 | ``			 * answer to `public readonly readonly $x` is the duplicate rule and`` |
|        - | 2423 | ``			 * not a property typed `readonly`. */`` |
|       99 | 2424 | `			if( pMods->bReadonly ){` |
|        3 | 2425 | `				zTwice = "readonly";` |
|        1 | 2426 | `			}` |
|       99 | 2427 | `			pMods->bReadonly = 1;` |
|       99 | 2428 | `			pMods->iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       99 | 2429 | `			pGen->pIn++;` |
|    20016 | 2430 | `		}else if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     6229 | 2431 | `			break;` |
|    17659 | 2432 | `		}else if( (nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok)) != 0 ){` |
|       71 | 2433 | `			if( pMods->bSetVis ){` |
|      ! 0 | 2434 | `				zTwice = "";` |
|      ! 0 | 2435 | `			}` |
|       71 | 2436 | `			pMods->bSetVis = 1;` |
|       71 | 2437 | `			pMods->nSetVis = nSetVis;` |
|       71 | 2438 | `			pMods->iFlags \|= GenStateSetVisFlag(nSetVis);` |
|       71 | 2439 | `			pGen->pIn += nSetTok;` |
|       38 | 2440 | `		}else{` |
|    17593 | 2441 | `			sxi32 nKw = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    17588 | 2442 | `			if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PRIVATE` |
|     9974 | 2443 | `			 \|\| nKw == PH7_TKWRD_PROTECTED ){` |
|     8247 | 2444 | `				if( pMods->bVis ){` |
|        3 | 2445 | `					zTwice = "";` |
|        1 | 2446 | `				}` |
|     8247 | 2447 | `				pMods->bVis = 1;` |
|     8247 | 2448 | `				pMods->iProtection = nKw;` |
|    13472 | 2449 | `			}else if( nKw == PH7_TKWRD_STATIC ){` |
|     1269 | 2450 | `				if( pMods->bStatic ){` |
|        3 | 2451 | `					zTwice = "static";` |
|        1 | 2452 | `				}` |
|     1269 | 2453 | `				pMods->bStatic = 1;` |
|     1269 | 2454 | `				pMods->iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|     8719 | 2455 | `			}else if( nKw == PH7_TKWRD_ABSTRACT ){` |
|      155 | 2456 | `				if( pMods->bAbstract ){` |
|        3 | 2457 | `					zTwice = "abstract";` |
|        1 | 2458 | `				}` |
|      155 | 2459 | `				pMods->bAbstract = 1;` |
|      155 | 2460 | `				pMods->iFlags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|     8012 | 2461 | `			}else if( nKw == PH7_TKWRD_FINAL ){` |
|      109 | 2462 | `				if( pMods->bFinal ){` |
|        3 | 2463 | `					zTwice = "final";` |
|        1 | 2464 | `				}` |
|      109 | 2465 | `				pMods->bFinal = 1;` |
|      109 | 2466 | `				pMods->iFlags \|= PH7_CLASS_ATTR_FINAL;` |
|       57 | 2467 | `			}else{` |
|     7833 | 2468 | `				break; /* not a modifier -- the member itself starts here */` |
|        - | 2469 | `			}` |
|     9765 | 2470 | `			pGen->pIn++;` |
|        - | 2471 | `		}` |
|     9925 | 2472 | `		pMods->bAny = 1;` |
|     9925 | 2473 | `		if( zTwice ){` |
|        - | 2474 | `			sxi32 rc;` |
|       14 | 2475 | `			pGen->iFatalTrace = PH7_FATAL_TRACE_NONE;` |
|       19 | 2476 | `			rc = zTwice[0]` |
|       12 | 2477 | `				? PH7_GenCompileError(pGen,E_ERROR,pMods->nLine,` |
|        4 | 2478 | `					"Multiple %s modifiers are not allowed",zTwice)` |
|        6 | 2479 | `				: PH7_GenCompileError(pGen,E_ERROR,pMods->nLine,` |
|        - | 2480 | `					"Multiple access type modifiers are not allowed");` |
|       14 | 2481 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 2482 | `		}` |
|        5 | 2483 | `	}` |
|    10143 | 2484 | `	return SXRET_OK;` |
|     5079 | 2485 | `}` |
|        - | 2486 | ``/* php's name for a `(set)` visibility, as its refusals spell it. */`` |
|        4 | 2487 | `static const char * GenStateSetVisWord(sxi32 nKw)` |
|        2 | 2488 | `{` |
|        6 | 2489 | `	if( nKw == PH7_TKWRD_PRIVATE ){` |
|        6 | 2490 | `		return "private(set)";` |
|        - | 2491 | `	}` |
|      ! 0 | 2492 | `	return (nKw == PH7_TKWRD_PROTECTED) ? "protected(set)" : "public(set)";` |
|        4 | 2493 | `}` |
|        - | 2494 | `/*` |
|        - | 2495 | ` * The run, judged against the KIND of declaration it turned out to modify. php` |
|        - | 2496 | ` * refuses each combination with its own sentence, and the order tested below is` |
|        - | 2497 | ` * the order php reports them in when one declaration breaks several` |
|        - | 2498 | ``  * (`static abstract const` is the static rule, `public private(set) static const` `` |
|        - | 2499 | ` * the private(set) one).` |
|        - | 2500 | ` */` |
|    10128 | 2501 | `static sxi32 GenStateScreenMemberMods(ph7_gen_state *pGen,const GenMemberMods *pMods,` |
|        - | 2502 | `	int iKind,ph7_class *pClass)` |
|        5 | 2503 | `{` |
|    10133 | 2504 | `	const char *zBad = 0;   /* the modifier php names */` |
|    10133 | 2505 | `	const char *zWhere = 0; /* ...and what it was written on */` |
|        - | 2506 | `	sxi32 rc;` |
|    10133 | 2507 | `	if( iKind == GEN_MEMBER_CONST ){` |
|      737 | 2508 | `		zWhere = "a class constant";` |
|      737 | 2509 | `		if( pMods->bReadonly ){` |
|      ! 0 | 2510 | `			zBad = "readonly";` |
|      737 | 2511 | `		}else if( pMods->bSetVis ){` |
|        3 | 2512 | `			zBad = GenStateSetVisWord(pMods->nSetVis);` |
|      736 | 2513 | `		}else if( pMods->bStatic ){` |
|        3 | 2514 | `			zBad = "static";` |
|      734 | 2515 | `		}else if( pMods->bAbstract ){` |
|        3 | 2516 | `			zBad = "abstract";` |
|        6 | 2517 | `		}` |
|     9767 | 2518 | `	}else if( iKind == GEN_MEMBER_METHOD ){` |
|     6005 | 2519 | `		zWhere = "a method";` |
|     6005 | 2520 | `		if( pMods->bReadonly ){` |
|        3 | 2521 | `			zBad = "readonly";` |
|     6004 | 2522 | `		}else if( pMods->bSetVis ){` |
|      ! 0 | 2523 | `			zBad = GenStateSetVisWord(pMods->nSetVis);` |
|     6003 | 2524 | `		}else if( pMods->bFinal && pMods->bAbstract ){` |
|        3 | 2525 | `			zBad = "final";` |
|        3 | 2526 | `			zWhere = "an abstract method";` |
|        1 | 2527 | `		}` |
|     3005 | 2528 | `	}else{` |
|     3401 | 2529 | `		if( pMods->bFinal && pMods->bAbstract ){` |
|        3 | 2530 | `			zBad = "final";` |
|        3 | 2531 | `			zWhere = "an abstract property";` |
|     3400 | 2532 | `		}else if( pMods->bFinal && (pClass->iFlags & PH7_CLASS_INTERFACE) ){` |
|        - | 2533 | `			/* php words the interface case as the PROPERTY's problem rather than` |
|        - | 2534 | `			 * the modifier's: an interface property is a hooked REQUIREMENT, and a` |
|        - | 2535 | `			 * requirement no implementor may restate cannot be one. */` |
|        3 | 2536 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pMods->nLine,` |
|        - | 2537 | `				"Property in interface cannot be final");` |
|        3 | 2538 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|     3397 | 2539 | `		}else if( pMods->bFinal && pMods->iProtection == PH7_TKWRD_PRIVATE ){` |
|        - | 2540 | ``			/* A private property is invisible to a subclass, so `final` on one says`` |
|        - | 2541 | `			 * nothing php can honour. This is the PROPERTY rule only: php accepts` |
|        - | 2542 | `			 * the same pair on a PROMOTED constructor parameter (modifiers 36` |
|        - | 2543 | `			 * there) -- an asymmetry of php's own, reproduced rather than smoothed` |
|        - | 2544 | `			 * over. */` |
|        3 | 2545 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pMods->nLine,` |
|        - | 2546 | `				"Property cannot be both final and private");` |
|        3 | 2547 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 2548 | `		}` |
|        - | 2549 | `	}` |
|    10129 | 2550 | `	if( zBad == 0 ){` |
|    10117 | 2551 | `		return SXRET_OK;` |
|        - | 2552 | `	}` |
|       15 | 2553 | `	pGen->iFatalTrace = PH7_FATAL_TRACE_NONE;` |
|       21 | 2554 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pMods->nLine,` |
|        6 | 2555 | `		"Cannot use the %s modifier on %s",zBad,zWhere);` |
|       15 | 2556 | `	return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|     5069 | 2557 | `}` |
|        - | 2558 | `/*` |
|        - | 2559 | `` * Peek the NAME a `const`/`function` keyword introduces, for the diagnostics php`` |
|        - | 2560 | ` * words with it. Returns 0 when the declaration is malformed enough that there` |
|        - | 2561 | ` * is no name to show (the caller then falls back to a nameless sentence).` |
|        - | 2562 | ` */` |
|        8 | 2563 | `static SyString * GenStateMemberNamePeek(ph7_gen_state *pGen)` |
|        3 | 2564 | `{` |
|       11 | 2565 | `	SyToken *p = pGen->pIn + 1;` |
|       11 | 2566 | `	if( p < pGen->pEnd && (p->nType & PH7_TK_AMPER) ){` |
|      ! 0 | 2567 | ``		p++; /* a by-reference method: `function &f()` */`` |
|      ! 0 | 2568 | `	}` |
|       11 | 2569 | `	if( p < pGen->pEnd && (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       11 | 2570 | `		return &p->sData;` |
|        - | 2571 | `	}` |
|      ! 0 | 2572 | `	return 0;` |
|        7 | 2573 | `}` |
|        - | 2574 | `/*` |
|        - | 2575 | ` * One MEMBER of a class/trait/interface body: the modifier run, php's screens on` |
|        - | 2576 | ` * it, and the declaration it turned out to modify. Shared by the three body` |
|        - | 2577 | ` * loops, which used to carry three near-identical modifier ladders -- and three` |
|        - | 2578 | ` * different sets of gaps.` |
|        - | 2579 | ` *` |
|        - | 2580 | `` * The enum `case` and the trait `use` statement are NOT members and never reach`` |
|        - | 2581 | ` * here: they take no modifiers, and their loops consume them first.` |
|        - | 2582 | ` *` |
|        - | 2583 | ` * Returns SXRET_OK, SXERR_SYNTAX when a refusal was reported (the caller` |
|        - | 2584 | ` * abandons the body), or SXERR_ABORT when the error budget is spent.` |
|        - | 2585 | ` */` |
|    10148 | 2586 | `static sxi32 GenStateCompileMember(ph7_gen_state *pGen,ph7_class *pClass,const char *zBody)` |
|        5 | 2587 | `{` |
|    10153 | 2588 | `	SyString *pName = &pClass->sName;` |
|        - | 2589 | `	GenMemberMods sMods;` |
|        - | 2590 | `	int iKind;` |
|        - | 2591 | `	sxi32 rc;` |
|    10153 | 2592 | `	rc = GenStateReadMemberMods(&(*pGen),&sMods);` |
|    10153 | 2593 | `	if( rc != SXRET_OK ){` |
|       14 | 2594 | `		return rc;` |
|        - | 2595 | `	}` |
|    10143 | 2596 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 2597 | `		rc = PH7_GenCompileError(pGen,E_PARSE,sMods.nLine,` |
|      ! 0 | 2598 | `			"Expecting member declaration inside %s '%z'",zBody,pName);` |
|      ! 0 | 2599 | `		return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 2600 | `	}` |
|        - | 2601 | `	/* What the run modifies. A '$' -- or a TYPE followed by one -- is a property;` |
|        - | 2602 | `	 * the three keywords are each their own declaration. */` |
|    10138 | 2603 | `	if( (pGen->pIn->nType & PH7_TK_DOLLAR)` |
|     9120 | 2604 | `	 \|\| GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|     3401 | 2605 | `		iKind = GEN_MEMBER_PROP;` |
|     8445 | 2606 | `	}else if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 2607 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|        - | 2608 | `			"Unexpected token '%z'. Expecting member declaration inside %s '%z'",` |
|      ! 0 | 2609 | `			&pGen->pIn->sData,zBody,pName);` |
|      ! 0 | 2610 | `		return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|      ! 0 | 2611 | `	}else{` |
|     6747 | 2612 | `		sxi32 nKw = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     6747 | 2613 | `		if( nKw == PH7_TKWRD_CONST ){` |
|      737 | 2614 | `			iKind = GEN_MEMBER_CONST;` |
|     6381 | 2615 | `		}else if( nKw == PH7_TKWRD_FUNCTION ){` |
|     6005 | 2616 | `			iKind = GEN_MEMBER_METHOD;` |
|     3013 | 2617 | `		}else if( nKw == PH7_TKWRD_VAR ){` |
|       13 | 2618 | `			iKind = GEN_MEMBER_VAR;` |
|        8 | 2619 | `		}else{` |
|      ! 0 | 2620 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|        - | 2621 | `				"Unexpected token '%z'. Expecting member declaration inside %s '%z'",` |
|      ! 0 | 2622 | `				&pGen->pIn->sData,zBody,pName);` |
|      ! 0 | 2623 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 2624 | `		}` |
|        - | 2625 | `	}` |
|    10143 | 2626 | `	if( iKind == GEN_MEMBER_VAR ){` |
|        - | 2627 | ``		/* `var $x` is the pre-5.0 spelling of `public $x` and takes NO other`` |
|        - | 2628 | `		 * modifier: php's parser is looking for a VARIABLE where the modifier run` |
|        - | 2629 | ``		 * left off, so `public var $x` and `final var $x` are parse errors naming`` |
|        - | 2630 | `		 * the token that is not one. */` |
|       13 | 2631 | `		if( sMods.bAny ){` |
|        3 | 2632 | `			rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,"variable");` |
|        3 | 2633 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 2634 | `		}` |
|       10 | 2635 | `		pGen->pIn++; /* Jump the 'var' keyword */` |
|       10 | 2636 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 | 2637 | `			rc = PH7_GenSyntaxError(&(*pGen),` |
|      ! 0 | 2638 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn : 0,"variable");` |
|      ! 0 | 2639 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 2640 | `		}` |
|       10 | 2641 | `		iKind = GEN_MEMBER_PROP;` |
|        6 | 2642 | `	}else{` |
|    10133 | 2643 | `		rc = GenStateScreenMemberMods(&(*pGen),&sMods,iKind,pClass);` |
|    10133 | 2644 | `		if( rc != SXRET_OK ){` |
|       19 | 2645 | `			return rc;` |
|        - | 2646 | `		}` |
|        - | 2647 | `	}` |
|    10125 | 2648 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|        - | 2649 | `		/* An interface member is public by declaration, and its methods are` |
|        - | 2650 | ``		 * implicitly abstract -- so php refuses both `abstract` written out and`` |
|        - | 2651 | ``		 * `final`, naming the method in each. */`` |
|      265 | 2652 | `		if( sMods.iProtection != PH7_TKWRD_PUBLIC ){` |
|        8 | 2653 | `			SyString *pMember = (iKind == GEN_MEMBER_PROP) ? 0 : GenStateMemberNamePeek(&(*pGen));` |
|        8 | 2654 | `			if( iKind == GEN_MEMBER_PROP ){` |
|        - | 2655 | `				/* php words the PROPERTY case as the property's problem, and names` |
|        - | 2656 | `				 * neither of the two visibilities it refuses. */` |
|        3 | 2657 | `				rc = PH7_GenCompileError(pGen,E_ERROR,sMods.nLine,` |
|        - | 2658 | `					"Property in interface cannot be protected or private");` |
|        6 | 2659 | `			}else if( pMember ){` |
|        7 | 2660 | `				rc = PH7_GenCompileError(pGen,E_ERROR,sMods.nLine,` |
|        - | 2661 | `					"Access type for interface %s %z::%z%s must be public",` |
|        2 | 2662 | `					(iKind == GEN_MEMBER_CONST) ? "constant" : "method",pName,pMember,` |
|        2 | 2663 | `					(iKind == GEN_MEMBER_CONST) ? "" : "()");` |
|        3 | 2664 | `			}else{` |
|      ! 0 | 2665 | `				rc = PH7_GenCompileError(pGen,E_ERROR,sMods.nLine,` |
|        - | 2666 | `					"Access type for interface %s must be public",` |
|      ! 0 | 2667 | `					(iKind == GEN_MEMBER_CONST) ? "constant" : "method");` |
|        - | 2668 | `			}` |
|        8 | 2669 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 2670 | `		}` |
|      259 | 2671 | `		if( iKind == GEN_MEMBER_METHOD && (sMods.bFinal \|\| sMods.bAbstract) ){` |
|        6 | 2672 | `			SyString *pMember = GenStateMemberNamePeek(&(*pGen));` |
|        6 | 2673 | `			const char *zWhat = sMods.bFinal ? "final" : "abstract";` |
|        6 | 2674 | `			if( pMember ){` |
|        8 | 2675 | `				rc = PH7_GenCompileError(pGen,E_ERROR,sMods.nLine,` |
|        2 | 2676 | `					"Interface method %z::%z() must not be %s",pName,pMember,zWhat);` |
|        4 | 2677 | `			}else{` |
|      ! 0 | 2678 | `				rc = PH7_GenCompileError(pGen,E_ERROR,sMods.nLine,` |
|      ! 0 | 2679 | `					"Interface method must not be %s",zWhat);` |
|        - | 2680 | `			}` |
|        6 | 2681 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 2682 | `		}` |
|        - | 2683 | `		/* Both remaining kinds are abstract in an interface: a method has no body` |
|        - | 2684 | `		 * to compile, and a property is a HOOKED requirement (a plain one is` |
|        - | 2685 | `		 * GenStateCompileClassAttr's own "Interfaces may only include hooked` |
|        - | 2686 | `		 * properties"). */` |
|      255 | 2687 | `		if( iKind != GEN_MEMBER_CONST ){` |
|      213 | 2688 | `			sMods.iFlags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|      104 | 2689 | `		}` |
|      125 | 2690 | `	}` |
|    10115 | 2691 | `	if( iKind == GEN_MEMBER_CONST ){` |
|      729 | 2692 | `		return GenStateCompileClassConstant(&(*pGen),sMods.iProtection,sMods.iFlags,pClass);` |
|        - | 2693 | `	}` |
|     9391 | 2694 | `	if( iKind == GEN_MEMBER_METHOD ){` |
|        - | 2695 | `		/* An interface method is a SIGNATURE: it has no body to compile. */` |
|     8990 | 2696 | `		return GenStateCompileClassMethod(&(*pGen),sMods.iProtection,sMods.iFlags,` |
|     5990 | 2697 | `			(pClass->iFlags & PH7_CLASS_INTERFACE) ? FALSE : TRUE,pClass);` |
|        - | 2698 | `	}` |
|     3401 | 2699 | `	return GenStateCompileClassAttr(&(*pGen),sMods.iProtection,sMods.iFlags,pClass);` |
|     5079 | 2700 | `}` |
|        - | 2701 | `/*` |
|        - | 2702 | `` * Compile a PHP 8.4 property-hook list `{ get ...; set ...; }` following a`` |
|        - | 2703 | ` * property declaration. Each hook body is synthesized into a hidden public` |
|        - | 2704 | ` * class method (__phl_hook_get_NAME / __phl_hook_set_NAME) so inheritance,` |
|        - | 2705 | ` * $this binding, and dispatch ride the ordinary method machinery; OP_MEMBER /` |
|        - | 2706 | ` * OP_STORE route reads and plain writes through them (a per-instance guard` |
|        - | 2707 | ` * makes $this->NAME inside a hook body address the raw backing slot — php's` |
|        - | 2708 | `` * rule that hooks see the backing store). `get => expr;` compiles as an`` |
|        - | 2709 | `` * implicit return (the arrow-fn pattern); `set => expr;` compiles the same`` |
|        - | 2710 | ` * and is flagged VM_FUNC_HOOK_SET_EXPR — the dispatcher assigns its return` |
|        - | 2711 | `` * value to the backing slot. A `set` without a parameter list receives the`` |
|        - | 2712 | `` * implicit `$value` formal.`` |
|        - | 2713 | ` * On entry pGen->pIn sits on '{'; on success it sits just past '}'.` |
|        - | 2714 | ` */` |
|        - | 2715 | `/*` |
|        - | 2716 | `` * Whether any token in [pStart, pEnd) spells `$this->NAME` (this property's own`` |
|        - | 2717 | `` * name; `?->` and `::` member ops count too). php 8.4's virtual-vs-backed rule:`` |
|        - | 2718 | ` * a hooked property is BACKED iff any of its OWN hook bodies references it by` |
|        - | 2719 | ` * name through $this — otherwise it is VIRTUAL: no backing store, no default` |
|        - | 2720 | ` * allowed, excluded from the raw object surfaces.` |
|        - | 2721 | ` */` |
|      280 | 2722 | `static int GenStateHookBodyRefsProp(SyToken *pStart,SyToken *pEnd,const SyString *pName)` |
|        5 | 2723 | `{` |
|        - | 2724 | `	SyToken *p;` |
|     1113 | 2725 | `	for( p = pStart ; p + 1 < pEnd ; p++ ){` |
|      907 | 2726 | `		if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|      749 | 2727 | `			continue;` |
|        - | 2728 | `		}` |
|        - | 2729 | ``		/* `$this->NAME` (also `?->`/`::`) */`` |
|      158 | 2730 | `		if( p + 3 < pEnd` |
|      158 | 2731 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|      158 | 2732 | `		 && p[1].sData.nByte == sizeof("this")-1` |
|      134 | 2733 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)"this",sizeof("this")-1) == 0` |
|      110 | 2734 | `		 && GenStateTokenIsMemberOp(&p[2])` |
|      110 | 2735 | `		 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|      110 | 2736 | `		 && p[3].sData.nByte == pName->nByte` |
|      103 | 2737 | `		 && SyMemcmp((const void *)p[3].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|       79 | 2738 | `			return 1;` |
|        - | 2739 | `		}` |
|       45 | 2740 | `	}` |
|      211 | 2741 | `	return 0;` |
|      145 | 2742 | `}` |
|        - | 2743 | `/*` |
|        - | 2744 | ` * True when p opens php 8.4's parent-hook call form` |
|        - | 2745 | `` * `parent :: $ NAME :: get\|set (` (7 tokens through the '(').`` |
|        - | 2746 | ` */` |
|     1690 | 2747 | `static int GenStateIsParentHookCallAt(SyToken *p,SyToken *pEnd)` |
|        5 | 2748 | `{` |
|     1930 | 2749 | `	return p + 6 < pEnd` |
|     1080 | 2750 | `	 && (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|      340 | 2751 | `	 && p->sData.nByte == sizeof("parent")-1` |
|      125 | 2752 | `	 && SyMemcmp((const void *)p->sData.zString,(const void *)"parent",sizeof("parent")-1) == 0` |
|       32 | 2753 | `	 && GenStateTokenIsMemberOp(&p[1])` |
|       24 | 2754 | `	 && (p[2].nType & PH7_TK_DOLLAR) != 0` |
|       24 | 2755 | `	 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       24 | 2756 | `	 && GenStateTokenIsMemberOp(&p[4])` |
|       24 | 2757 | `	 && (p[5].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       24 | 2758 | `	 && p[5].sData.nByte == 3` |
|       24 | 2759 | `	 && (SyMemcmp((const void *)p[5].sData.zString,(const void *)"get",3) == 0` |
|       14 | 2760 | `	  \|\| SyMemcmp((const void *)p[5].sData.zString,(const void *)"set",3) == 0)` |
|     1925 | 2761 | `	 && (p[6].nType & PH7_TK_LPAREN) != 0;` |
|        5 | 2762 | `}` |
|        - | 2763 | `/*` |
|        - | 2764 | `` * Rewrite php 8.4 `parent::$x::get(...)` / `parent::$x::set(...)` calls in a`` |
|        - | 2765 | ` * hook body into calls of the parent class's synthesized hook method` |
|        - | 2766 | `` * (`parent::__phl_hook_get_x(...)`). Builds a token COPY into pCopy (only`` |
|        - | 2767 | ` * called when GenStateIsParentHookCallAt matched somewhere in the range);` |
|        - | 2768 | ` * copied tokens keep pointing at source-owned lexeme storage, and the` |
|        - | 2769 | ` * synthesized method-name lexemes are VM-allocator owned. Returns SXRET_OK` |
|        - | 2770 | ` * or SXERR_MEM.` |
|        - | 2771 | ` */` |
|       12 | 2772 | `static sxi32 GenStateRewriteParentHookCalls(ph7_gen_state *pGen,SySet *pCopy,` |
|        - | 2773 | `	SyToken *pStart,SyToken *pEnd)` |
|        3 | 2774 | `{` |
|       15 | 2775 | `	SyToken *p = pStart;` |
|       93 | 2776 | `	while( p < pEnd ){` |
|       81 | 2777 | `		if( GenStateIsParentHookCallAt(p,pEnd) ){` |
|        - | 2778 | `			SyToken sTok;` |
|        - | 2779 | `			char zName[384];` |
|        - | 2780 | `			sxu32 nName;` |
|        - | 2781 | `			char *zDup;` |
|        - | 2782 | ``			/* `parent` `::` */`` |
|       15 | 2783 | `			SySetPut(pCopy,(const void *)&p[0]);` |
|       15 | 2784 | `			SySetPut(pCopy,(const void *)&p[1]);` |
|       21 | 2785 | `			nName = SyBufferFormat(zName,sizeof(zName),"__phl_hook_%.3s_%.*s",` |
|       12 | 2786 | `				p[5].sData.zString,(int)p[3].sData.nByte,p[3].sData.zString);` |
|       15 | 2787 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|       15 | 2788 | `			if( zDup == 0 ){` |
|      ! 0 | 2789 | `				return SXERR_MEM;` |
|        - | 2790 | `			}` |
|       15 | 2791 | `			sTok = p[3]; /* keep the line info of the property name */` |
|       15 | 2792 | `			sTok.nType = PH7_TK_ID;` |
|       15 | 2793 | `			SyStringInitFromBuf(&sTok.sData,zDup,nName);` |
|       15 | 2794 | `			sTok.pUserData = 0;` |
|       15 | 2795 | `			SySetPut(pCopy,(const void *)&sTok);` |
|       15 | 2796 | `			p += 6; /* continue at the '(' — arguments copy through unchanged */` |
|       15 | 2797 | `			continue;` |
|        - | 2798 | `		}` |
|       69 | 2799 | `		SySetPut(pCopy,(const void *)p);` |
|       69 | 2800 | `		p++;` |
|        3 | 2801 | `	}` |
|       15 | 2802 | `	return SXRET_OK;` |
|        9 | 2803 | `}` |
|        - | 2804 | `/*` |
|        - | 2805 | `` * A `get` hook's return type IS the property's declared type — php never lets a`` |
|        - | 2806 | ` * hook declare one, so there is nothing else it could be, and that is what makes` |
|        - | 2807 | `` * `public int $p { get { return "5"; } }` answer int(5) and a `get` returning`` |
|        - | 2808 | `` * "x" raise `C::$p::get(): Return value must be of type int, string returned`.`` |
|        - | 2809 | ` * Installing it on the synthesized method reuses the return enforcement that` |
|        - | 2810 | ` * already matches php byte for byte (the same move the __toString implicit` |
|        - | 2811 | `` * `string` type made), and lets the compile-time bare-`return;` check see the`` |
|        - | 2812 | ` * hook as the typed function php treats it as.` |
|        - | 2813 | ` *` |
|        - | 2814 | ` * The union alternatives are SHARED, not copied: their class-name SyStrings are` |
|        - | 2815 | ` * VM-allocator owned and outlive both records, which is the same contract` |
|        - | 2816 | ` * GenStateCopyTypeToAttr relies on.` |
|        - | 2817 | ` */` |
|      270 | 2818 | `static void GenStateHookGetReturnType(ph7_vm_func *pFunc,ph7_class_attr *pAttr)` |
|        5 | 2819 | `{` |
|      275 | 2820 | `	if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       33 | 2821 | `		return; /* untyped property: the hook is untyped too */` |
|        - | 2822 | `	}` |
|      247 | 2823 | `	pFunc->nReturnType = pAttr->nType;` |
|      247 | 2824 | `	pFunc->sReturnClass = pAttr->sClass;` |
|      247 | 2825 | `	pFunc->sReturnTypeName = pAttr->sTypeName;` |
|      247 | 2826 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|       20 | 2827 | `		pFunc->iFlags \|= VM_FUNC_RETURN_NULLABLE;` |
|        8 | 2828 | `	}` |
|      247 | 2829 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|        - | 2830 | `		sxu32 i;` |
|      ! 0 | 2831 | `		for( i = 0 ; i < SySetUsed(&pAttr->aUnionAlts) ; i++ ){` |
|      ! 0 | 2832 | `			SySetPut(&pFunc->aReturnUnion,SySetAt(&pAttr->aUnionAlts,i));` |
|      ! 0 | 2833 | `		}` |
|      ! 0 | 2834 | `	}` |
|      140 | 2835 | `}` |
|        - | 2836 | `/*` |
|        - | 2837 | `` * The mirror for a `set` hook. php gives it two implicit pieces of signature:`` |
|        - | 2838 | `` * the implicit `$value` formal carries the PROPERTY's declared type (so`` |
|        - | 2839 | `` * `public int $p { set { ... } }` coerces `$o->p = "7"` to int(7) and rejects`` |
|        - | 2840 | `` * "abc" with `C::$p::set(): Argument #1 ($value) must be of type int, string`` |
|        - | 2841 | `` * given`), and the hook itself returns `void` — a set hook that returns a value`` |
|        - | 2842 | `` * is php's `A void method must not return a value`, on an untyped property too.`` |
|        - | 2843 | `` * An EXPLICIT `set(T $v)` keeps its own declared type; only the implicit formal`` |
|        - | 2844 | ` * is typed from the property, which is why the caller passes pValueArg only` |
|        - | 2845 | ` * when it synthesized one.` |
|        - | 2846 | ` */` |
|      138 | 2847 | `static void GenStateHookSetSignature(ph7_gen_state *pGen,ph7_vm_func *pFunc,` |
|        - | 2848 | `	ph7_class_attr *pAttr,ph7_vm_func_arg *pValueArg)` |
|        4 | 2849 | `{` |
|        - | 2850 | `	char *zVoid;` |
|      142 | 2851 | `	if( pValueArg && (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|      104 | 2852 | `		pValueArg->nType = pAttr->nType;` |
|      104 | 2853 | `		pValueArg->sClass = pAttr->sClass;` |
|      104 | 2854 | `		pValueArg->sTypeName = pAttr->sTypeName;` |
|      104 | 2855 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|       16 | 2856 | `			pValueArg->iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|        7 | 2857 | `		}` |
|      104 | 2858 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|        - | 2859 | `			sxu32 i;` |
|        6 | 2860 | `			pValueArg->iFlags \|= VM_FUNC_ARG_UNION;` |
|       14 | 2861 | `			for( i = 0 ; i < SySetUsed(&pAttr->aUnionAlts) ; i++ ){` |
|       10 | 2862 | `				SySetPut(&pValueArg->aUnionAlts,SySetAt(&pAttr->aUnionAlts,i));` |
|        6 | 2863 | `			}` |
|        2 | 2864 | `		}` |
|       50 | 2865 | `	}` |
|      142 | 2866 | `	pFunc->nReturnType = MEMOBJ_VOID;` |
|      142 | 2867 | `	zVoid = SyMemBackendStrDup(&pGen->pVm->sAllocator,"void",sizeof("void")-1);` |
|      142 | 2868 | `	if( zVoid ){` |
|      142 | 2869 | `		SyStringInitFromBuf(&pFunc->sReturnTypeName,zVoid,sizeof("void")-1);` |
|       69 | 2870 | `	}` |
|      142 | 2871 | `}` |
|        - | 2872 | `/*` |
|        - | 2873 | ` * A member modifier at pTok, as php's parser reads one before a hook's name` |
|        - | 2874 | ` * (its property_hook_modifiers is the class body's whole modifier run), with` |
|        - | 2875 | ` * the word php's refusal names it by. 0 when pTok is no modifier.` |
|        - | 2876 | ` */` |
|      920 | 2877 | `static const char * GenStateHookModifier(SyToken *pTok,SyToken *pEnd,int *pnTok)` |
|        5 | 2878 | `{` |
|        - | 2879 | `	sxi32 nSetVis;` |
|      925 | 2880 | `	*pnTok = 1;` |
|      925 | 2881 | `	if( pTok >= pEnd ){` |
|      ! 0 | 2882 | `		return 0;` |
|        - | 2883 | `	}` |
|      925 | 2884 | `	if( GenStateIsReadonly(pTok) ){` |
|      ! 0 | 2885 | `		return "readonly";` |
|        - | 2886 | `	}` |
|      925 | 2887 | `	if( (pTok->nType & PH7_TK_KEYWORD) == 0 ){` |
|      891 | 2888 | `		return 0;` |
|        - | 2889 | `	}` |
|       35 | 2890 | `	if( (nSetVis = GenStatePeekSetVisibility(pTok,pEnd,pnTok)) != 0 ){` |
|        3 | 2891 | `		return GenStateSetVisWord(nSetVis);` |
|        - | 2892 | `	}` |
|       33 | 2893 | `	*pnTok = 1;` |
|       33 | 2894 | `	switch( SX_PTR_TO_INT(pTok->pUserData) ){` |
|        3 | 2895 | `	case PH7_TKWRD_PUBLIC:    return "public";` |
|      ! 0 | 2896 | `	case PH7_TKWRD_PROTECTED: return "protected";` |
|      ! 0 | 2897 | `	case PH7_TKWRD_PRIVATE:   return "private";` |
|        3 | 2898 | `	case PH7_TKWRD_STATIC:    return "static";` |
|      ! 0 | 2899 | `	case PH7_TKWRD_ABSTRACT:  return "abstract";` |
|       29 | 2900 | `	case PH7_TKWRD_FINAL:     return "final";` |
|      ! 0 | 2901 | `	default: break;` |
|        - | 2902 | `	}` |
|      ! 0 | 2903 | `	return 0;` |
|      465 | 2904 | `}` |
|        - | 2905 | `/*` |
|        - | 2906 | ` * php's grammar for the hook list, judged before any hook compiles: every hook` |
|        - | 2907 | `` * is `[modifiers] [&] name [(params)]` and then `;`, a `{...}` body or`` |
|        - | 2908 | `` * `=> expr;`, and nothing else stands between two hooks -- a stray `;` is`` |
|        - | 2909 | ` * php's parse error, not a separator. The modifier run is judged in the` |
|        - | 2910 | `` * parser too, word by word: `final` is the only one a hook takes, and php`` |
|        - | 2911 | ` * names the first word it refuses at the token that ends the run. Being the` |
|        - | 2912 | ` * parser's, all of these outrank the per-hook rules the compile pass applies,` |
|        - | 2913 | ` * wherever in the list they sit.` |
|        - | 2914 | ` */` |
|      362 | 2915 | `static sxi32 GenStateScanHookList(ph7_gen_state *pGen,SyToken *pIn,SyToken *pEnd)` |
|        5 | 2916 | `{` |
|      367 | 2917 | `	SyToken *pErr = 0;          /* the token php names; 0 = the end of the input */` |
|      367 | 2918 | `	const char *zExpect = 0;` |
|        - | 2919 | `	sxi32 rc;` |
|      803 | 2920 | `	while( pIn < pEnd && (pIn->nType & PH7_TK_CCB) == 0 ){` |
|        - | 2921 | `		const char *zMod;` |
|      457 | 2922 | `		const char *zRefused = 0;` |
|      457 | 2923 | `		int bFinal = 0, bTwice = 0, nTok;` |
|      479 | 2924 | `		while( (zMod = GenStateHookModifier(pIn,pEnd,&nTok)) != 0 ){` |
|       23 | 2925 | `			if( zRefused == 0 && !bTwice ){` |
|       21 | 2926 | `				if( zMod[0] != 'f' ){` |
|        5 | 2927 | `					zRefused = zMod;` |
|       19 | 2928 | `				}else if( bFinal ){` |
|        3 | 2929 | `					bTwice = 1;` |
|        1 | 2930 | `				}` |
|       10 | 2931 | `			}` |
|       23 | 2932 | `			bFinal = 1;` |
|       23 | 2933 | `			pIn += nTok;` |
|        1 | 2934 | `		}` |
|      457 | 2935 | `		if( zRefused \|\| bTwice ){` |
|        7 | 2936 | `			sxu32 nAt = (pIn < pEnd) ? pIn->nLine : pIn[-1].nLine;` |
|        7 | 2937 | `			rc = zRefused` |
|        6 | 2938 | `				? PH7_GenCompileError(pGen,E_ERROR,nAt,` |
|        2 | 2939 | `					"Cannot use the %s modifier on a property hook",zRefused)` |
|        4 | 2940 | `				: PH7_GenCompileError(pGen,E_ERROR,nAt,` |
|        - | 2941 | `					"Multiple final modifiers are not allowed");` |
|        7 | 2942 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|        - | 2943 | `		}` |
|      451 | 2944 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_AMPER) ){` |
|      ! 0 | 2945 | `			pIn++;` |
|      ! 0 | 2946 | `		}` |
|      451 | 2947 | `		if( pIn >= pEnd \|\| (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        5 | 2948 | `			pErr = pIn;` |
|        5 | 2949 | `			zExpect = "identifier";` |
|        8 | 2950 | `			goto Syntax;` |
|        - | 2951 | `		}` |
|      447 | 2952 | `		pIn++; /* the hook's name */` |
|      447 | 2953 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|       48 | 2954 | `			SyToken *pRp = 0;` |
|       48 | 2955 | `			PH7_DelimitNestedTokens(&pIn[1],pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pRp);` |
|       48 | 2956 | `			if( pRp >= pEnd ){` |
|      ! 0 | 2957 | `				pIn = pEnd;` |
|      ! 0 | 2958 | `				goto Syntax;` |
|        - | 2959 | `			}` |
|       48 | 2960 | `			pIn = &pRp[1];` |
|       22 | 2961 | `		}` |
|      447 | 2962 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_SEMI) ){` |
|       97 | 2963 | `			pIn++;` |
|      401 | 2964 | `		}else if( pIn < pEnd && (pIn->nType & PH7_TK_OCB) ){` |
|      167 | 2965 | `			SyToken *pCloser = 0;` |
|      167 | 2966 | `			PH7_DelimitNestedTokens(&pIn[1],pEnd,PH7_TK_OCB,PH7_TK_CCB,&pCloser);` |
|      167 | 2967 | `			if( pCloser >= pEnd ){` |
|      ! 0 | 2968 | `				pIn = pEnd;` |
|      ! 0 | 2969 | `				goto Syntax;` |
|        - | 2970 | `			}` |
|      167 | 2971 | `			pIn = &pCloser[1];` |
|      274 | 2972 | `		}else if( pIn < pEnd && (pIn->nType & PH7_TK_ARRAY_OP) ){` |
|        - | 2973 | ``			/* `=> expr` runs to its own `;`: a closer first is php's bare`` |
|        - | 2974 | `			 * "unexpected" with nothing expected */` |
|      189 | 2975 | `			sxi32 iNest = 0;` |
|      771 | 2976 | `			for( pIn++ ; pIn < pEnd ; pIn++ ){` |
|      771 | 2977 | `				if( pIn->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       26 | 2978 | `					iNest++;` |
|      760 | 2979 | `				}else if( pIn->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       28 | 2980 | `					if( iNest <= 0 ){` |
|        3 | 2981 | `						break;` |
|        - | 2982 | `					}` |
|       26 | 2983 | `					iNest--;` |
|      736 | 2984 | `				}else if( iNest <= 0 && (pIn->nType & PH7_TK_SEMI) ){` |
|      187 | 2985 | `					break;` |
|        - | 2986 | `				}` |
|      296 | 2987 | `			}` |
|      189 | 2988 | `			if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        3 | 2989 | `				pErr = pIn;` |
|        3 | 2990 | `				goto Syntax;` |
|        - | 2991 | `			}` |
|      187 | 2992 | `			pIn++;` |
|       96 | 2993 | `		}else{` |
|        5 | 2994 | `			pErr = pIn;` |
|        5 | 2995 | `			zExpect = "\"=>\" or \";\" or \"{\"";` |
|        5 | 2996 | `			goto Syntax;` |
|        - | 2997 | `		}` |
|        5 | 2998 | `	}` |
|      351 | 2999 | `	return SXRET_OK;` |
|        5 | 3000 | `Syntax:` |
|       11 | 3001 | `	rc = PH7_GenSyntaxError(pGen,pErr < pEnd ? pErr : 0,zExpect);` |
|       11 | 3002 | `	return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|      186 | 3003 | `}` |
|        - | 3004 | `/* A refusal of php's zend_compile_property_hooks: a compile fatal that ends the list. */` |
|        - | 3005 | `#define GEN_HOOK_REFUSE(RC) return ((RC) == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT` |
|      362 | 3006 | `PH7_PRIVATE sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        5 | 3007 | `{` |
|      367 | 3008 | `	sxu32 nLine = pGen->pIn->nLine;` |
|      367 | 3009 | `	sxu32 nHookEnd = nLine;   /* the line that ends the last hook compiled */` |
|        - | 3010 | `	sxi32 rc;` |
|      367 | 3011 | `	int bRefsSelf = 0;` |
|      367 | 3012 | `	pGen->pIn++; /* Jump '{' */` |
|      367 | 3013 | `	rc = GenStateScanHookList(&(*pGen),pGen->pIn,pGen->pEnd);` |
|      367 | 3014 | `	if( rc != SXRET_OK ){` |
|       17 | 3015 | `		return rc;` |
|        - | 3016 | `	}` |
|      351 | 3017 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) ){` |
|        3 | 3018 | `		GEN_HOOK_REFUSE(PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine,` |
|        - | 3019 | `			"Property hook list must not be empty"));` |
|        - | 3020 | `	}` |
|      755 | 3021 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|        - | 3022 | `		char zHook[384];` |
|        - | 3023 | `		SyString sHookName;` |
|        - | 3024 | `		SyString *pWritten;      /* the hook's name as written: php's messages quote it */` |
|        - | 3025 | `		ph7_class_method *pMeth;` |
|      439 | 3026 | `		SyToken *pParams = 0;    /* an explicit parameter list's '(' */` |
|        - | 3027 | `		SyToken *pBody;          /* the token after the name and its parameters */` |
|      439 | 3028 | `		int bGet, bFinal = 0, bBodyless, nTok;` |
|        - | 3029 | `		sxu32 nHLine;` |
|        - | 3030 | ``		/* The scan above refused every modifier but a single `final`. */`` |
|      451 | 3031 | `		while( GenStateHookModifier(pGen->pIn,pGen->pEnd,&nTok) != 0 ){` |
|       13 | 3032 | `			bFinal = 1;` |
|       13 | 3033 | `			pGen->pIn += nTok;` |
|        1 | 3034 | `		}` |
|      439 | 3035 | `		nHLine = pGen->pIn->nLine;` |
|      439 | 3036 | `		if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|        - | 3037 | `			/* by-reference get hook: not modeled (loud, recorded) */` |
|      ! 0 | 3038 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|        - | 3039 | `				"By-reference property hooks are not supported for %z::$%z",` |
|      ! 0 | 3040 | `				&pClass->sDisp,&pAttr->sName);` |
|      ! 0 | 3041 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3042 | `				return SXERR_ABORT;` |
|        - | 3043 | `			}` |
|      ! 0 | 3044 | `			return SXERR_CORRUPT;` |
|        - | 3045 | `		}` |
|      439 | 3046 | `		pWritten = &pGen->pIn->sData;` |
|      439 | 3047 | `		if( pWritten->nByte == 3 && SyStrnicmp(pWritten->zString,"get",3) == 0 ){` |
|      285 | 3048 | `			bGet = 1;` |
|      298 | 3049 | `		}else if( pWritten->nByte == 3 && SyStrnicmp(pWritten->zString,"set",3) == 0 ){` |
|      154 | 3050 | `			bGet = 0;` |
|       79 | 3051 | `		}else{` |
|        5 | 3052 | `			bGet = -1;` |
|        - | 3053 | `		}` |
|      439 | 3054 | `		pGen->pIn++; /* Jump the name */` |
|      439 | 3055 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       46 | 3056 | `			pParams = pGen->pIn;` |
|       21 | 3057 | `		}` |
|      439 | 3058 | `		pBody = pGen->pIn;` |
|      439 | 3059 | `		if( pParams ){` |
|       46 | 3060 | `			SyToken *pRp = 0;` |
|       46 | 3061 | `			PH7_DelimitNestedTokens(&pParams[1],pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pRp);` |
|       46 | 3062 | `			pBody = &pRp[1];` |
|       21 | 3063 | `		}` |
|      439 | 3064 | `		bBodyless = (pBody->nType & PH7_TK_SEMI) != 0;` |
|        - | 3065 | `		/* php's zend_compile_property_hooks, in the order it asks: each rule is` |
|        - | 3066 | `		 * a compile fatal at the hook's name. An abstract property's hook WITH a` |
|        - | 3067 | `		 * body is an ordinary concrete hook; only an interface refuses one. */` |
|      439 | 3068 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        3 | 3069 | `			GEN_HOOK_REFUSE(PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|        - | 3070 | `				"Cannot declare hooks for static property"));` |
|        - | 3071 | `		}` |
|      437 | 3072 | `		if( bFinal && pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|        3 | 3073 | `			GEN_HOOK_REFUSE(PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|        - | 3074 | `				"Property hook cannot be both final and private"));` |
|        - | 3075 | `		}` |
|      430 | 3076 | `		if( (pClass->iFlags & PH7_CLASS_INTERFACE)` |
|      406 | 3077 | `		 \|\| ((pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) && bBodyless) ){` |
|       95 | 3078 | `			if( !bBodyless ){` |
|      ! 0 | 3079 | `				GEN_HOOK_REFUSE(PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|        - | 3080 | `					"Abstract property hook cannot have body"));` |
|        - | 3081 | `			}` |
|       95 | 3082 | `			if( bFinal ){` |
|        5 | 3083 | `				GEN_HOOK_REFUSE(PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|        - | 3084 | `					"Property hook cannot be both abstract and final"));` |
|        5 | 3085 | `			}` |
|      388 | 3086 | `		}else if( bBodyless ){` |
|        3 | 3087 | `			GEN_HOOK_REFUSE(PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|        - | 3088 | `				"Non-abstract property hook must have a body"));` |
|        - | 3089 | `		}` |
|      429 | 3090 | `		if( bGet < 0 ){` |
|        3 | 3091 | `			GEN_HOOK_REFUSE(PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|        - | 3092 | `				"Unknown hook \"%z\" for property %z::$%z, expected \"get\" or \"set\"",` |
|        - | 3093 | `				pWritten,&pClass->sDisp,&pAttr->sName));` |
|        - | 3094 | `		}` |
|      427 | 3095 | `		if( bGet && pParams ){` |
|        3 | 3096 | `			GEN_HOOK_REFUSE(PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|        - | 3097 | `				"get hook of property %z::$%z must not have a parameter list",` |
|        - | 3098 | `				&pClass->sDisp,&pAttr->sName));` |
|        - | 3099 | `		}` |
|      425 | 3100 | `		sHookName.zString = zHook;` |
|      635 | 3101 | `		sHookName.nByte = SyBufferFormat(zHook,sizeof(zHook),"__phl_hook_%s_%z",` |
|      210 | 3102 | `			bGet ? "get" : "set",&pAttr->sName);` |
|      425 | 3103 | `		if( bBodyless ){` |
|        - | 3104 | ``			/* Bare `get;` / `set;` — an ABSTRACT hook declaration (php 8.4):`` |
|        - | 3105 | ``			 * legal only on an `abstract` property or inside an interface. The`` |
|        - | 3106 | `			 * synthesized method carries PH7_CLASS_ATTR_ABSTRACT and rides the` |
|        - | 3107 | `			 * existing must-implement machinery; a concrete hook override (or a` |
|        - | 3108 | `			 * plain property, see GenStateCheckAbstractMethods) satisfies it. */` |
|       91 | 3109 | `			pGen->pIn = pBody;` |
|       91 | 3110 | `			pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|        - | 3111 | `				PH7_CLASS_PROT_PUBLIC,PH7_CLASS_ATTR_ABSTRACT,0);` |
|       91 | 3112 | `			if( pMeth == 0 ){` |
|      ! 0 | 3113 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 3114 | `				return SXERR_ABORT;` |
|        - | 3115 | `			}` |
|       91 | 3116 | `			pMeth->sFunc.nLine = nHLine;` |
|       91 | 3117 | `			if( bGet ){` |
|       67 | 3118 | `				GenStateHookGetReturnType(&pMeth->sFunc,pAttr);` |
|       31 | 3119 | `			}` |
|       91 | 3120 | `			if( !bGet ){` |
|        - | 3121 | ``				/* The implicit `$value` formal keeps the stub's signature`` |
|        - | 3122 | `				 * compatible with concrete set-hook implementations (which` |
|        - | 3123 | `				 * always carry one parameter). It takes the PROPERTY's declared` |
|        - | 3124 | `				 * type (php: the abstract set's parameter type IS the property` |
|        - | 3125 | `				 * type), so the override contravariance check accepts a typed` |
|        - | 3126 | ``				 * `set(int $v)` implementation on an `int $x` requirement. */`` |
|        - | 3127 | `				ph7_vm_func_arg sVArg;` |
|       28 | 3128 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|       28 | 3129 | `				if( zVName == 0 ){` |
|      ! 0 | 3130 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 3131 | `					return SXERR_ABORT;` |
|        - | 3132 | `				}` |
|       28 | 3133 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|       28 | 3134 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|       28 | 3135 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       28 | 3136 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       28 | 3137 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|       28 | 3138 | `				GenStateHookSetSignature(&(*pGen),&pMeth->sFunc,pAttr,&sVArg);` |
|       28 | 3139 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|       12 | 3140 | `			}` |
|       91 | 3141 | `			if( pAttr->iFlags & (bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET) ){` |
|      ! 0 | 3142 | `				GEN_HOOK_REFUSE(PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3143 | `					"Cannot redeclare property hook \"%z\"",pWritten));` |
|        - | 3144 | `			}` |
|       91 | 3145 | `			rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|       91 | 3146 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 3147 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 3148 | `				return SXERR_ABORT;` |
|        - | 3149 | `			}` |
|       91 | 3150 | `			pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|       91 | 3151 | `			nHookEnd = pGen->pIn->nLine;   /* the ';' */` |
|       91 | 3152 | `			pGen->pIn++; /* Jump ';' */` |
|       91 | 3153 | `			continue;` |
|        - | 3154 | `		}` |
|        - | 3155 | ``		/* `final` is the one modifier a hook keeps: a subclass may not override`` |
|        - | 3156 | `		 * it (see the final-method rule in the class link) */` |
|      506 | 3157 | `		pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|      167 | 3158 | `			PH7_CLASS_PROT_PUBLIC,bFinal ? PH7_CLASS_ATTR_FINAL : 0,0);` |
|      339 | 3159 | `		if( pMeth == 0 ){` |
|      ! 0 | 3160 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 3161 | `			return SXERR_ABORT;` |
|        - | 3162 | `		}` |
|      339 | 3163 | `		pMeth->sFunc.nLine = nHLine;` |
|      339 | 3164 | `		if( bGet ){` |
|      213 | 3165 | `			GenStateHookGetReturnType(&pMeth->sFunc,pAttr);` |
|      104 | 3166 | `		}` |
|      339 | 3167 | `		if( !bGet ){` |
|        - | 3168 | ``			/* Parameter list: explicit `set(Type $v)` or the implicit `$value` */`` |
|      130 | 3169 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       44 | 3170 | `				SyToken *pRp = 0;` |
|       44 | 3171 | `				pGen->pIn++;` |
|       44 | 3172 | `				PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pRp);` |
|       44 | 3173 | `				if( pGen->pIn < pRp ){` |
|       44 | 3174 | `					rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pRp,0,0);` |
|       44 | 3175 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3176 | `						return SXERR_ABORT;` |
|        - | 3177 | `					}` |
|       20 | 3178 | `				}` |
|       44 | 3179 | `				pGen->pIn = &pRp[1];` |
|        - | 3180 | `				/* php's rules for an explicit list: one plain parameter, typed` |
|        - | 3181 | `				 * exactly when the property is */` |
|       44 | 3182 | `				if( SySetUsed(&pMeth->sFunc.aArgs) != 1 ){` |
|        3 | 3183 | `					GEN_HOOK_REFUSE(PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|        - | 3184 | `						"%z hook of property %z::$%z must accept exactly one parameters",` |
|        - | 3185 | `						pWritten,&pClass->sDisp,&pAttr->sName));` |
|      ! 0 | 3186 | `				}else{` |
|       42 | 3187 | `					ph7_vm_func_arg *pV = (ph7_vm_func_arg *)SySetBasePtr(&pMeth->sFunc.aArgs);` |
|       42 | 3188 | `					const char *zWhy = 0;` |
|       42 | 3189 | `					if( pV->iFlags & VM_FUNC_ARG_BY_REF ){` |
|        3 | 3190 | `						zWhy = "must not be pass-by-reference";` |
|       41 | 3191 | `					}else if( pV->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        3 | 3192 | `						zWhy = "must not be variadic";` |
|       39 | 3193 | `					}else if( (pV->iFlags & VM_FUNC_ARG_HAS_DEF) \|\| SySetUsed(&pV->aByteCode) > 0 ){` |
|        3 | 3194 | `						zWhy = "must not have a default value";` |
|        1 | 3195 | `					}` |
|       42 | 3196 | `					if( zWhy ){` |
|        7 | 3197 | `						GEN_HOOK_REFUSE(PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|        - | 3198 | `							"Parameter $%z of %z hook %z::$%z %s",` |
|        - | 3199 | `							&pV->sName,pWritten,&pClass->sDisp,&pAttr->sName,zWhy));` |
|        - | 3200 | `					}` |
|       36 | 3201 | `					if( ((pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0) != (pV->sTypeName.nByte > 0) ){` |
|        5 | 3202 | `						GEN_HOOK_REFUSE(PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|        - | 3203 | `							"Type of parameter $%z of hook %z::$%z::set must be compatible with property type",` |
|        - | 3204 | `							&pV->sName,&pClass->sDisp,&pAttr->sName));` |
|        - | 3205 | `					}` |
|        - | 3206 | `				}` |
|       14 | 3207 | `			}` |
|      118 | 3208 | `			if( SySetUsed(&pMeth->sFunc.aArgs) < 1 ){` |
|        - | 3209 | `				/* Implicit $value formal */` |
|        - | 3210 | `				ph7_vm_func_arg sVArg;` |
|       90 | 3211 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|       90 | 3212 | `				if( zVName == 0 ){` |
|      ! 0 | 3213 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 3214 | `					return SXERR_ABORT;` |
|        - | 3215 | `				}` |
|       90 | 3216 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|       90 | 3217 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|       90 | 3218 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       90 | 3219 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       90 | 3220 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|       90 | 3221 | `				SyStringInitFromBuf(&sVArg.sTypeName,0,0);` |
|       90 | 3222 | `				GenStateHookSetSignature(&(*pGen),&pMeth->sFunc,pAttr,&sVArg);` |
|       90 | 3223 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|       47 | 3224 | `			}else{` |
|        - | 3225 | ``				/* An EXPLICIT `set(T $v)` keeps its own parameter type; only the`` |
|        - | 3226 | `				 * void return is implicit. */` |
|       31 | 3227 | `				GenStateHookSetSignature(&(*pGen),&pMeth->sFunc,pAttr,0);` |
|        - | 3228 | `			}` |
|       57 | 3229 | `		}` |
|      402 | 3230 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|        - | 3231 | `			/* Block body */` |
|      155 | 3232 | `			SyToken *pBodyStart = pGen->pIn;` |
|      155 | 3233 | `			SyToken *pCloser = 0;` |
|      155 | 3234 | `			int bParentCall = 0;` |
|      155 | 3235 | `			PH7_DelimitNestedTokens(&pBodyStart[1],pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pCloser);` |
|      155 | 3236 | `			if( pCloser < pGen->pEnd ){` |
|        - | 3237 | `				SyToken *pScan;` |
|     1265 | 3238 | `				for( pScan = &pBodyStart[1] ; pScan < pCloser ; pScan++ ){` |
|     1119 | 3239 | `					if( GenStateIsParentHookCallAt(pScan,pCloser) ){` |
|        6 | 3240 | `						bParentCall = 1;` |
|        6 | 3241 | `						break;` |
|        - | 3242 | `					}` |
|      560 | 3243 | `				}` |
|       75 | 3244 | `			}` |
|      155 | 3245 | `			if( bParentCall ){` |
|        - | 3246 | ``				/* `parent::$x::get()` inside the body: compile a REWRITTEN copy`` |
|        - | 3247 | `				 * of the body tokens (the call becomes the parent's synthesized` |
|        - | 3248 | `				 * hook method), then continue past the original body. */` |
|        - | 3249 | `				SySet sBody;` |
|        6 | 3250 | `				SyToken *pSavedEnd = pGen->pEnd;` |
|        6 | 3251 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        6 | 3252 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,&pCloser[1]);` |
|        6 | 3253 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 3254 | `					SySetRelease(&sBody);` |
|      ! 0 | 3255 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 3256 | `					return SXERR_ABORT;` |
|        - | 3257 | `				}` |
|        6 | 3258 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|        6 | 3259 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|        6 | 3260 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|        6 | 3261 | `				pGen->pIn = &pCloser[1];` |
|        6 | 3262 | `				pGen->pEnd = pSavedEnd;` |
|        6 | 3263 | `				SySetRelease(&sBody);` |
|        6 | 3264 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3265 | `					return SXERR_ABORT;` |
|        - | 3266 | `				}` |
|        6 | 3267 | `				pMeth->sFunc.nEndLine = pCloser->nLine;` |
|        4 | 3268 | `			}else{` |
|      151 | 3269 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|      151 | 3270 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3271 | `					return SXERR_ABORT;` |
|        - | 3272 | `				}` |
|      151 | 3273 | `				pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|        - | 3274 | `			}` |
|      155 | 3275 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|       32 | 3276 | `				bRefsSelf = 1;` |
|       19 | 3277 | `			}` |
|      338 | 3278 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ARRAY_OP) ){` |
|        - | 3279 | ``			/* `=> expr;` — implicit-return body (the arrow-fn pattern) */`` |
|        - | 3280 | `			GenBlock *pBlock;` |
|        - | 3281 | `			SySet *pInstrContainer;` |
|        - | 3282 | `			SyToken *pBodyStart;` |
|        - | 3283 | `			SyToken *pExprEnd;` |
|      177 | 3284 | `			SyToken *pSavedEnd = 0;` |
|        - | 3285 | `			SySet sBody;` |
|      177 | 3286 | `			int bParentCall = 0;` |
|      177 | 3287 | `			pGen->pIn++; /* Jump '=>' */` |
|      177 | 3288 | `			pBodyStart = pGen->pIn;` |
|        - | 3289 | `			/* Delimit the expression (first top-level ';', or a closer that` |
|        - | 3290 | `			 * would end the enclosing hook list) and rewrite any` |
|        - | 3291 | ``			 * `parent::$x::get()` calls into the parent's synthesized hook`` |
|        - | 3292 | `			 * method on a token copy. */` |
|        - | 3293 | `			{` |
|      177 | 3294 | `				sxi32 iNest = 0;` |
|      177 | 3295 | `				pExprEnd = pBodyStart;` |
|      747 | 3296 | `				while( pExprEnd < pGen->pEnd ){` |
|      747 | 3297 | `					if( pExprEnd->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       26 | 3298 | `						iNest++;` |
|      736 | 3299 | `					}else if( pExprEnd->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       26 | 3300 | `						if( iNest <= 0 ){` |
|      ! 0 | 3301 | `							break;` |
|        - | 3302 | `						}` |
|       26 | 3303 | `						iNest--;` |
|      714 | 3304 | `					}else if( iNest <= 0 && (pExprEnd->nType & PH7_TK_SEMI) ){` |
|      177 | 3305 | `						break;` |
|        - | 3306 | `					}` |
|      575 | 3307 | `					pExprEnd++;` |
|        5 | 3308 | `				}` |
|        - | 3309 | `			}` |
|        - | 3310 | `			{` |
|        - | 3311 | `				SyToken *pScan;` |
|      667 | 3312 | `				for( pScan = pBodyStart ; pScan < pExprEnd ; pScan++ ){` |
|      503 | 3313 | `					if( GenStateIsParentHookCallAt(pScan,pExprEnd) ){` |
|       10 | 3314 | `						bParentCall = 1;` |
|       10 | 3315 | `						break;` |
|        - | 3316 | `					}` |
|      250 | 3317 | `				}` |
|        - | 3318 | `			}` |
|      177 | 3319 | `			if( bParentCall ){` |
|       10 | 3320 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       10 | 3321 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,pExprEnd);` |
|       10 | 3322 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 3323 | `					SySetRelease(&sBody);` |
|      ! 0 | 3324 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 3325 | `					return SXERR_ABORT;` |
|        - | 3326 | `				}` |
|       10 | 3327 | `				pSavedEnd = pGen->pEnd;` |
|       10 | 3328 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|       10 | 3329 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|        4 | 3330 | `			}` |
|      263 | 3331 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      172 | 3332 | `				PH7_VmInstrLength(pGen->pVm),&pMeth->sFunc,&pBlock);` |
|      177 | 3333 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 3334 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"PH7 engine is running out-of-memory");` |
|      ! 0 | 3335 | `				return SXERR_ABORT;` |
|        - | 3336 | `			}` |
|      177 | 3337 | `			pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      177 | 3338 | `			PH7_VmSetByteCodeContainer(pGen->pVm,&pMeth->sFunc.aByteCode);` |
|      177 | 3339 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      177 | 3340 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      177 | 3341 | `			GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|      177 | 3342 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|      177 | 3343 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      177 | 3344 | `			GenStateLeaveBlock(&(*pGen),0);` |
|      177 | 3345 | `			if( bParentCall ){` |
|       10 | 3346 | `				pGen->pIn = pExprEnd; /* land on the original ';' */` |
|       10 | 3347 | `				pGen->pEnd = pSavedEnd;` |
|       10 | 3348 | `				SySetRelease(&sBody);` |
|        4 | 3349 | `			}` |
|      177 | 3350 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3351 | `				return SXERR_ABORT;` |
|        - | 3352 | `			}` |
|      177 | 3353 | `			pMeth->sFunc.nEndLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nHLine;` |
|      177 | 3354 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|       50 | 3355 | `				bRefsSelf = 1;` |
|       23 | 3356 | `			}` |
|      177 | 3357 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      177 | 3358 | `				pGen->pIn++; /* Jump ';' */` |
|       86 | 3359 | `			}` |
|      177 | 3360 | `			if( !bGet ){` |
|        - | 3361 | ``				/* `set => expr` assigns the expression to the backing store:`` |
|        - | 3362 | `				 * the dispatcher consumes the implicit return value — which` |
|        - | 3363 | `				 * also makes the property BACKED (php: the shorthand is sugar` |
|        - | 3364 | ``				 * for `$this->NAME = expr`). */`` |
|       14 | 3365 | `				pMeth->sFunc.iFlags \|= VM_FUNC_HOOK_SET_EXPR;` |
|       14 | 3366 | `				bRefsSelf = 1;` |
|        6 | 3367 | `			}` |
|       91 | 3368 | `		}else{` |
|      ! 0 | 3369 | `			goto HookSyntax;` |
|        - | 3370 | `		}` |
|      327 | 3371 | `		if( pAttr->iFlags & (bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET) ){` |
|        - | 3372 | `			/* php asks once the hook has compiled: its last line */` |
|        3 | 3373 | `			GEN_HOOK_REFUSE(PH7_GenCompileError(pGen,E_ERROR,pMeth->sFunc.nEndLine,` |
|        - | 3374 | `				"Cannot redeclare property hook \"%z\"",pWritten));` |
|        - | 3375 | `		}` |
|      325 | 3376 | `		rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|      325 | 3377 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 3378 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 3379 | `			return SXERR_ABORT;` |
|        - | 3380 | `		}` |
|      325 | 3381 | `		pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|      325 | 3382 | `		nHookEnd = pMeth->sFunc.nEndLine;   /* the body's '}' or the arrow's ';' */` |
|        5 | 3383 | `	}` |
|      321 | 3384 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|      ! 0 | 3385 | `		goto HookSyntax;` |
|        - | 3386 | `	}` |
|      321 | 3387 | `	pGen->pIn++; /* Jump '}' */` |
|        - | 3388 | `	/* php judges the property's attributes after compiling its hooks, at the` |
|        - | 3389 | `	 * line compiling them left behind: each hook is a function whose implicit` |
|        - | 3390 | `	 * return sits on its LAST line, so a refusal blames the end of the last` |
|        - | 3391 | `	 * hook -- its body's '}', or the ';' of an arrow or abstract one -- not the` |
|        - | 3392 | `	 * declaration. The virtual-default refusal below runs at the end of the` |
|        - | 3393 | `	 * hooks in a class with no parent, before the attributes, but waits for` |
|        - | 3394 | `	 * the class link in one that has a parent, after them. */` |
|      316 | 3395 | `	if( pGen->pCurBase` |
|      188 | 3396 | `	 && GenStateCheckAttrPlacement(&(*pGen),&pAttr->aAttrs,nHookEnd,8,8,0,0) == SXERR_ABORT ){` |
|      ! 0 | 3397 | `		return SXERR_ABORT;` |
|        - | 3398 | `	}` |
|      321 | 3399 | `	if( !bRefsSelf ){` |
|        - | 3400 | ``		/* php 8.4 virtual-vs-backed: no hook body referenced `$this->NAME`, so`` |
|        - | 3401 | `		 * this property is VIRTUAL — php gives it no backing store and forbids` |
|        - | 3402 | `` 		 * a default value (compile fatal, php's exact wording). A `parent::$x::get()` `` |
|        - | 3403 | `		 * call is not a reference: what makes such a child backed is a backed` |
|        - | 3404 | `		 * PARENT, which only the class link knows. */` |
|      241 | 3405 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_HOOK_VIRTUAL;` |
|      241 | 3406 | `		if( SySetUsed(&pAttr->aByteCode) > 0 && pGen->pCurBase == 0 ){` |
|        - | 3407 | `			/* ...at the end of the last hook; in a class with a parent the` |
|        - | 3408 | `			 * class link asks instead (GenStateCheckVirtualDefaults), because` |
|        - | 3409 | `			 * a backed parent property makes this one backed too. */` |
|        7 | 3410 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHookEnd,` |
|        - | 3411 | `				"Cannot specify default value for virtual hooked property %z::$%z",` |
|        2 | 3412 | `				&pClass->sDisp,&pAttr->sName);` |
|        5 | 3413 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3414 | `				return SXERR_ABORT;` |
|        - | 3415 | `			}` |
|        5 | 3416 | `			return SXERR_CORRUPT;` |
|        - | 3417 | `		}` |
|      116 | 3418 | `	}` |
|      312 | 3419 | `	if( pGen->pCurBase == 0` |
|      292 | 3420 | `	 && GenStateCheckAttrPlacement(&(*pGen),&pAttr->aAttrs,nHookEnd,8,8,0,0) == SXERR_ABORT ){` |
|      ! 0 | 3421 | `		return SXERR_ABORT;` |
|        - | 3422 | `	}` |
|      317 | 3423 | `	return SXRET_OK;` |
|      ! 0 | 3424 | `HookSyntax:` |
|      ! 0 | 3425 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 3426 | `		"Invalid property hook declaration for %z::$%z: expecting 'get' or 'set'",` |
|      ! 0 | 3427 | `		&pClass->sDisp,&pAttr->sName);` |
|      ! 0 | 3428 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 3429 | `		return SXERR_ABORT;` |
|        - | 3430 | `	}` |
|      ! 0 | 3431 | `	return SXERR_CORRUPT;` |
|      186 | 3432 | `}` |
|        - | 3433 | `/* php's #[\Override] verification, defined with the rest of the class-link` |
|        - | 3434 | ` * checks below; both compilers (class and interface) drive the same pair. */` |
|        - | 3435 | `static sxi32 GenStateCollectOverrides(ph7_gen_state *pGen,ph7_class *pClass,` |
|        - | 3436 | `	SySet *pMeths,SySet *pProps);` |
|        - | 3437 | `static sxi32 GenStateCheckOverrides(ph7_gen_state *pGen,ph7_class *pClass,` |
|        - | 3438 | `	SySet *pMeths,SySet *pProps);` |
|        - | 3439 | `/*` |
|        - | 3440 | ` * Compile an object interface.` |
|        - | 3441 | ` *  According to the PHP language reference manual` |
|        - | 3442 | ` *   Object Interfaces:` |
|        - | 3443 | ` *   Object interfaces allow you to create code which specifies which methods` |
|        - | 3444 | ` *   a class must implement, without having to define how these methods are handled.` |
|        - | 3445 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|        - | 3446 | ` *   class, but without any of the methods having their contents defined.` |
|        - | 3447 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|        - | 3448 | ` */` |
|      456 | 3449 | `PH7_PRIVATE sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|        5 | 3450 | `{` |
|      461 | 3451 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 3452 | `	ph7_class *pClass,*pBase;` |
|      461 | 3453 | `	ph7_class *pSavedCurClass = pGen->pCurClass; /* restored at 'done' */` |
|      461 | 3454 | `	GenBlock *pSavedCurClassBlock = pGen->pCurClassBlock;` |
|      461 | 3455 | `	ph7_class *pSavedCurBase = pGen->pCurBase;   /* ...and its base, see pCurBase */` |
|        - | 3456 | `	SyToken *pEnd,*pTmp;` |
|        - | 3457 | `	SyString *pName;` |
|        - | 3458 | `	SySet aOvMeth,aOvProp;   /* the #[\Override] claims this interface DECLARED */` |
|        - | 3459 | ``	SySet aExtraParents;     /* `extends A, S, T`: every parent after the first */`` |
|      461 | 3460 | `	sxu32 nErrEntry = pGen->nErr; /* errors already reported when this one started */` |
|        - | 3461 | `	sxi32 nKwrd;` |
|        - | 3462 | `	sxi32 rc;` |
|        - | 3463 | `	{` |
|        - | 3464 | `		/* Deferral gate: parent interfaces may need an autoloader` |
|        - | 3465 | `		 * that has not run yet. */` |
|        - | 3466 | `		sxi32 rcDefer;` |
|      461 | 3467 | `		if( GenStateMaybeDeferClass(pGen,0,PH7_DEFER_KIND_INTERFACE,&rcDefer) ){` |
|       19 | 3468 | `			return rcDefer == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 3469 | `		}` |
|        - | 3470 | `	}` |
|        - | 3471 | `	/* Jump the 'interface' keyword */` |
|      445 | 3472 | `	pGen->pIn++;` |
|        - | 3473 | `	/* Extract interface name */` |
|      445 | 3474 | `	pName = &pGen->pIn->sData;` |
|        - | 3475 | `	/* Advance the stream cursor */` |
|      445 | 3476 | `	pGen->pIn++;` |
|        - | 3477 | `	/* Build FQN and obtain a raw class */ {` |
|        - | 3478 | `		SyBlob sFQN;` |
|        - | 3479 | `		SyString sFQNStr;` |
|      445 | 3480 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      445 | 3481 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      445 | 3482 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|        - | 3483 | ``		/* php refuses a declaration whose short name a local `use` already took. */`` |
|      445 | 3484 | `		if( GenStateGuardImportRedeclare(pGen,0,pName,&sFQNStr,nLine) == SXERR_ABORT ){` |
|      ! 0 | 3485 | `			SyBlobRelease(&sFQN);` |
|      ! 0 | 3486 | `			return SXERR_ABORT;` |
|        - | 3487 | `		}` |
|      445 | 3488 | `		GenStateRecordDeclaredName(pGen,0,&sFQNStr);` |
|      445 | 3489 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      445 | 3490 | `		SyBlobRelease(&sFQN);` |
|        - | 3491 | `	}` |
|      445 | 3492 | `	if( pClass == 0 ){` |
|      ! 0 | 3493 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 3494 | `		return SXERR_ABORT;` |
|        - | 3495 | `	}` |
|      445 | 3496 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|      445 | 3497 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 3498 | `		return SXERR_ABORT;` |
|        - | 3499 | `	}` |
|        - | 3500 | `	/* Mark as an interface (PH7_NewRawClass may have set INTERNAL) */` |
|      445 | 3501 | `	pClass->iFlags \|= PH7_CLASS_INTERFACE;` |
|      660 | 3502 | `	if( GenStateCheckAttrPlacement(&(*pGen),&pClass->aAttrs,pClass->nLine,1,1,` |
|      665 | 3503 | `			&pClass->sName,pClass->iFlags) == SXERR_ABORT ){` |
|      ! 0 | 3504 | `		return SXERR_ABORT;` |
|        - | 3505 | `	}` |
|        - | 3506 | `	/* Assume no base class is given */` |
|      445 | 3507 | `	pBase = 0;` |
|      445 | 3508 | `	SySetInit(&aExtraParents,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      445 | 3509 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       75 | 3510 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       75 | 3511 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a, c, … */ ){` |
|        - | 3512 | `			/* php lets an interface extend SEVERAL parent interfaces. The first` |
|        - | 3513 | `			 * becomes pBase (single-inheritance chain, hDerived); every extra one` |
|        - | 3514 | `			 * is recorded via PH7_ClassImplement so it lands in aInterface — the` |
|        - | 3515 | `			 * runtime subtype walk (VmInterfaceReaches) follows both. */` |
|       75 | 3516 | `			pGen->pIn++;` |
|       45 | 3517 | `			for(;;){` |
|        - | 3518 | `				SyBlob sResolved;` |
|        - | 3519 | `				SyString sBaseName;` |
|        - | 3520 | `				sxu32 nRefLine;` |
|        - | 3521 | `				ph7_class *pParent;` |
|       95 | 3522 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|       95 | 3523 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       95 | 3524 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 3525 | `					SyBlobRelease(&sResolved);` |
|      ! 0 | 3526 | `					rc = PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        - | 3527 | `						"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|      ! 0 | 3528 | `						pName);` |
|      ! 0 | 3529 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 3530 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3531 | `						return SXERR_ABORT;` |
|        - | 3532 | `					}` |
|      ! 0 | 3533 | `					return SXRET_OK;` |
|        - | 3534 | `				}` |
|       95 | 3535 | `				pParent = GenStateLinkClass(pGen,&sResolved);` |
|       95 | 3536 | `				SyStringInitFromBuf(&sBaseName,` |
|        - | 3537 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - | 3538 | `				/* Only interfaces is allowed */` |
|       95 | 3539 | `				while( pParent && (pParent->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 3540 | `					pParent = pParent->pNextName;` |
|      ! 0 | 3541 | `				}` |
|       95 | 3542 | `				if( pParent == 0 ){` |
|       15 | 3543 | `					if( !GenStateLintSkipsBase(pGen,pClass) ){` |
|      ! 0 | 3544 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - | 3545 | `							"Nonexistent base interface '%z'",&sBaseName);` |
|      ! 0 | 3546 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 3547 | `							SyBlobRelease(&sResolved);` |
|      ! 0 | 3548 | `							return SXERR_ABORT;` |
|        - | 3549 | `						}` |
|        3 | 3550 | `					}` |
|       89 | 3551 | `				}else if( pBase == 0 ){` |
|        - | 3552 | `					/* First parent → single-inheritance base */` |
|       65 | 3553 | `					pBase = pParent;` |
|       35 | 3554 | `				}else{` |
|        - | 3555 | `					/* Additional parent → COLLECTED, and applied after the body like` |
|        - | 3556 | `					 * the first one is. Copying its members here put them in hMethod` |
|        - | 3557 | `					 * and hConst before the body was read, so the interface's own` |
|        - | 3558 | ``					 * `public function g();` collided with the very name it was`` |
|        - | 3559 | ``					 * restating: `interface B extends A, S` could redeclare A's`` |
|        - | 3560 | `					 * members and not S's ("Cannot redeclare B::g()"), which is a` |
|        - | 3561 | `					 * declaration php accepts and php-di writes. */` |
|       21 | 3562 | `					SySetPut(&aExtraParents,(const void *)&pParent);` |
|        - | 3563 | `				}` |
|       95 | 3564 | `				SyBlobRelease(&sResolved);` |
|        - | 3565 | `				/* Continue on a comma-separated list */` |
|       95 | 3566 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       23 | 3567 | `					pGen->pIn++;` |
|       23 | 3568 | `					continue;` |
|        - | 3569 | `				}` |
|       75 | 3570 | `				break;` |
|      ! 0 | 3571 | `			}` |
|       35 | 3572 | `		}` |
|       35 | 3573 | `	}` |
|      445 | 3574 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|        - | 3575 | `		/* Syntax error */` |
|      ! 0 | 3576 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Expected '{' after interface '%z' definition",pName);` |
|      ! 0 | 3577 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 3578 | `		if( rc == SXERR_ABORT ){` |
|        - | 3579 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3580 | `			return SXERR_ABORT;` |
|        - | 3581 | `		}` |
|      ! 0 | 3582 | `		return SXRET_OK;` |
|        - | 3583 | `	}` |
|      445 | 3584 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      445 | 3585 | `	pEnd = 0; /* cc warning */` |
|        - | 3586 | `	/* Delimit the interface body */` |
|      445 | 3587 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|      445 | 3588 | `	if( pEnd >= pGen->pEnd ){` |
|        - | 3589 | `		/* Syntax error */` |
|      ! 0 | 3590 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Missing '}' after interface '%z' definition",pName);` |
|      ! 0 | 3591 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 3592 | `		if( rc == SXERR_ABORT ){` |
|        - | 3593 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3594 | `			return SXERR_ABORT;` |
|        - | 3595 | `		}` |
|      ! 0 | 3596 | `		return SXRET_OK;` |
|        - | 3597 | `	}` |
|        - | 3598 | `	/* The delimiter token is the interface body's closing brace */` |
|      445 | 3599 | `	pClass->nEndLine = pEnd->nLine;` |
|        - | 3600 | `	/* Swap token stream */` |
|      445 | 3601 | `	pTmp = pGen->pEnd;` |
|      445 | 3602 | `	pGen->pEnd = pEnd;` |
|        - | 3603 | `	/* This interface is now the lexical class for its body (see pCurClass) — a` |
|        - | 3604 | `	 * const default here is not a trait, so __TRAIT__ stays "". */` |
|      445 | 3605 | `	pGen->pCurClass = pClass;` |
|      445 | 3606 | `	pGen->pCurClassBlock = pGen->pCurrent;` |
|      445 | 3607 | ``	pGen->pCurBase = 0; /* php gives an interface no `parent`, however many it extends */`` |
|        - | 3608 | `	/* Start the parse process` |
|        - | 3609 | `	 * Note (According to the PHP reference manual):` |
|        - | 3610 | `	 *  Only constants and function signatures(without body) are allowed.` |
|        - | 3611 | `	 *  Only 'public' visibility is allowed.` |
|        - | 3612 | `	 */` |
|      341 | 3613 | `	for(;;){` |
|        - | 3614 | `		/* Jump leading/trailing semi-colons */` |
|      879 | 3615 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|      197 | 3616 | `			pGen->pIn++;` |
|        5 | 3617 | `		}` |
|      687 | 3618 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 3619 | `			/* End of interface body */` |
|      425 | 3620 | `			break;` |
|        - | 3621 | `		}` |
|        - | 3622 | `		/* Bind a directly-preceding docblock to this member */` |
|      267 | 3623 | `		GenStateSetPendingDoc(&(*pGen));` |
|      262 | 3624 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      136 | 3625 | `			&& !GenStateIsReadonly(pGen->pIn) ){` |
|      ! 0 | 3626 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|        - | 3627 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|      ! 0 | 3628 | `				&pGen->pIn->sData,pName);` |
|      ! 0 | 3629 | `			if( rc == SXERR_ABORT ){` |
|        - | 3630 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 3631 | `				return SXERR_ABORT;` |
|        - | 3632 | `			}` |
|      ! 0 | 3633 | `			goto done;` |
|        - | 3634 | `		}` |
|        - | 3635 | `		/* The member, through the shared modifier run -- which is where an` |
|        - | 3636 | ``		 * interface's own rules live now (public-only, no `final`, no written`` |
|        - | 3637 | ``		 * `abstract`, and a property that may only be a hooked requirement). */`` |
|      267 | 3638 | `		rc = GenStateCompileMember(&(*pGen),pClass,"interface");` |
|      267 | 3639 | `		if( rc != SXRET_OK ){` |
|       24 | 3640 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3641 | `				return SXERR_ABORT;` |
|        - | 3642 | `			}` |
|       24 | 3643 | `			goto done;` |
|        - | 3644 | `		}` |
|        5 | 3645 | `	}` |
|        - | 3646 | `	/* An interface method may claim #[\Override] too, against the interfaces this` |
|        - | 3647 | `	 * one extends -- collected before the inherit copies theirs in. */` |
|      425 | 3648 | `	GenStateCollectOverrides(&(*pGen),pClass,&aOvMeth,&aOvProp);` |
|        - | 3649 | `	/* Reject a php-fatal redeclaration before hoisting the interface. An` |
|        - | 3650 | `	 * interface that EXTENDS another is not early-bound, exactly as a class` |
|        - | 3651 | `	 * that implements one is not. */` |
|      780 | 3652 | `	if( GenStateGuardClassRedeclaration(pGen,pClass,` |
|      395 | 3653 | `			pBase == 0 && SySetUsed(&aExtraParents) == 0) == SXERR_ABORT ){` |
|        3 | 3654 | `		SySetRelease(&aOvMeth);` |
|        3 | 3655 | `		SySetRelease(&aOvProp);` |
|        3 | 3656 | `		return SXERR_ABORT;` |
|        - | 3657 | `	}` |
|        - | 3658 | `	/* Every method this interface DECLARES, judged against the same name in each` |
|        - | 3659 | `	 * parent -- hMethod holds only its own declarations until the inherits below` |
|        - | 3660 | `	 * run, so this is the one moment the two sets are separable. php makes the` |
|        - | 3661 | `	 * check for a restated method whichever parent it came from; PHL made it for` |
|        - | 3662 | ``	 * none of them, so `interface B extends A { public function f(): int; }` over`` |
|        - | 3663 | ``	 * `A::f(): string` compiled in silence. */`` |
|      423 | 3664 | `	if( pGen->nErr == nErrEntry ){` |
|      411 | 3665 | `		if( pBase && PH7_ClassInterfaceCheckRedeclare(&(*pGen),pClass,pBase) == SXERR_ABORT ){` |
|      ! 0 | 3666 | `			SySetRelease(&aOvMeth);` |
|      ! 0 | 3667 | `			SySetRelease(&aOvProp);` |
|      ! 0 | 3668 | `			SySetRelease(&aExtraParents);` |
|      ! 0 | 3669 | `			return SXERR_ABORT;` |
|        - | 3670 | `		}` |
|        - | 3671 | `		{` |
|      411 | 3672 | `			ph7_class **apExtra = (ph7_class **)SySetBasePtr(&aExtraParents);` |
|        - | 3673 | `			sxu32 nExtra;` |
|      429 | 3674 | `			for( nExtra = 0 ; nExtra < SySetUsed(&aExtraParents) ; ++nExtra ){` |
|       18 | 3675 | `				if( PH7_ClassInterfaceCheckRedeclare(&(*pGen),pClass,apExtra[nExtra])` |
|       12 | 3676 | `					== SXERR_ABORT ){` |
|      ! 0 | 3677 | `					SySetRelease(&aOvMeth);` |
|      ! 0 | 3678 | `					SySetRelease(&aOvProp);` |
|      ! 0 | 3679 | `					SySetRelease(&aExtraParents);` |
|      ! 0 | 3680 | `					return SXERR_ABORT;` |
|        - | 3681 | `				}` |
|       12 | 3682 | `			}` |
|        - | 3683 | `		}` |
|      203 | 3684 | `	}` |
|        - | 3685 | `	/* Install the interface */` |
|      423 | 3686 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      423 | 3687 | `	if( rc == SXRET_OK && pBase ){` |
|        - | 3688 | `		/* Inherit from the base interface */` |
|       65 | 3689 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|       30 | 3690 | `	}` |
|      423 | 3691 | `	if( rc == SXRET_OK ){` |
|        - | 3692 | `		/* ...and from every parent after the first, whose members are copied only` |
|        - | 3693 | `		 * where this interface declared none of its own. */` |
|      423 | 3694 | `		ph7_class **apExtra = (ph7_class **)SySetBasePtr(&aExtraParents);` |
|        - | 3695 | `		sxu32 nExtra;` |
|      441 | 3696 | `		for( nExtra = 0 ; rc == SXRET_OK && nExtra < SySetUsed(&aExtraParents) ; ++nExtra ){` |
|       21 | 3697 | `			rc = PH7_ClassImplement(pClass,apExtra[nExtra]);` |
|       12 | 3698 | `		}` |
|      209 | 3699 | `	}` |
|      423 | 3700 | `	SySetRelease(&aExtraParents);` |
|      418 | 3701 | `	if( rc == SXRET_OK && pGen->nErr == nErrEntry` |
|      416 | 3702 | `	 && GenStateCheckOverrides(&(*pGen),pClass,&aOvMeth,&aOvProp) == SXERR_ABORT ){` |
|      ! 0 | 3703 | `		SySetRelease(&aOvMeth);` |
|      ! 0 | 3704 | `		SySetRelease(&aOvProp);` |
|      ! 0 | 3705 | `		return SXERR_ABORT;` |
|        - | 3706 | `	}` |
|      423 | 3707 | `	SySetRelease(&aOvMeth);` |
|      423 | 3708 | `	SySetRelease(&aOvProp);` |
|      423 | 3709 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3710 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 3711 | `		return SXERR_ABORT;` |
|        - | 3712 | `	}` |
|      452 | 3713 | `	if( GenStateDeclaredWhereRun(pGen,pClass,0) ){` |
|       63 | 3714 | `		GenStateHideUntilDeclared(pGen,pClass,nLine);` |
|       29 | 3715 | `	}` |
|      180 | 3716 | `done:` |
|      443 | 3717 | `	SySetRelease(&aExtraParents);` |
|      443 | 3718 | `	pGen->pCurClass = pSavedCurClass;` |
|      443 | 3719 | `	pGen->pCurBase = pSavedCurBase;` |
|      443 | 3720 | `	pGen->pCurClassBlock = pSavedCurClassBlock;` |
|        - | 3721 | `	/* Point beyond the interface body */` |
|      443 | 3722 | `	pGen->pIn  = &pEnd[1];` |
|      443 | 3723 | `	pGen->pEnd = pTmp;` |
|      443 | 3724 | `	return PH7_OK;` |
|      233 | 3725 | `}` |
|        - | 3726 | `/*` |
|        - | 3727 | ` * Compile a user-defined class.` |
|        - | 3728 | ` * According to the PHP language reference manual` |
|        - | 3729 | ` *  class` |
|        - | 3730 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|        - | 3731 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|        - | 3732 | ` *  of the properties and methods belonging to the class.` |
|        - | 3733 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|        - | 3734 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|        - | 3735 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|        - | 3736 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|        - | 3737 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|        - | 3738 | ` *  (called "methods").` |
|        - | 3739 | ` */` |
|        - | 3740 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|        - | 3741 | `typedef struct TraitUseEntry TraitUseEntry;` |
|        - | 3742 | `struct TraitUseEntry {` |
|        - | 3743 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|        - | 3744 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|        - | 3745 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|        - | 3746 | `};` |
|        - | 3747 | `/*` |
|        - | 3748 | ` * Validate that methods implementing interface contracts have compatible` |
|        - | 3749 | ` * signatures: public visibility and at least as many parameters as declared.` |
|        - | 3750 | ` */` |
|     6340 | 3751 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 | 3752 | `{` |
|        - | 3753 | `	ph7_class **apIface;` |
|        - | 3754 | `	sxu32 nIface,i;` |
|        - | 3755 | `	sxi32 rc;` |
|     6345 | 3756 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      ! 0 | 3757 | `		return SXRET_OK;` |
|        - | 3758 | `	}` |
|     6345 | 3759 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|     6345 | 3760 | `	nIface = SySetUsed(&pClass->aInterface);` |
|     7413 | 3761 | `	for(i = 0; i < nIface; i++){` |
|     1073 | 3762 | `		ph7_class *pIface = apIface[i];` |
|        - | 3763 | `		SyHashEntry *pEntry;` |
|     1073 | 3764 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|     2649 | 3765 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|     1581 | 3766 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - | 3767 | `			ph7_class_method *pImplMeth;` |
|     1581 | 3768 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|        - | 3769 | `			/* Find the implementing method in the class */` |
|     1581 | 3770 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|     1581 | 3771 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       97 | 3772 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|        - | 3773 | `			}` |
|        - | 3774 | `			/* Check visibility: interface methods must be implemented as public */` |
|     1489 | 3775 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        4 | 3776 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|        - | 3777 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|        1 | 3778 | `					&pClass->sDisp,pMName,&pIface->sDisp);` |
|        3 | 3779 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3780 | `					return SXERR_ABORT;` |
|        - | 3781 | `				}` |
|        1 | 3782 | `			}` |
|        - | 3783 | `			/* Signature compatibility. php checks an implementation against the` |
|        - | 3784 | `			 * interface's declaration with the very rule it checks an override by --` |
|        - | 3785 | `			 * one shared body, so the variance half is not the class hierarchy's` |
|        - | 3786 | `			 * alone and the fatal reads the same. This site used to count PARAMETERS` |
|        - | 3787 | ``			 * only, and word the two declarations as bare `$name` lists.`` |
|        - | 3788 | `			 *` |
|        - | 3789 | `			 * A CONSTRUCTOR is not exempt here: php exempts an INHERITED one from` |
|        - | 3790 | ``			 * variance, but an interface that declares `__construct` constrains every`` |
|        - | 3791 | `			 * implementor's. */` |
|     1484 | 3792 | `			if( PH7_ClassCheckOverrideCompat(&(*pGen),pIface,pClass,pIfaceMeth,pImplMeth,0)` |
|      747 | 3793 | `				== SXERR_ABORT ){` |
|      ! 0 | 3794 | `				return SXERR_ABORT;` |
|        - | 3795 | `			}` |
|        5 | 3796 | `		}` |
|      539 | 3797 | `	}` |
|     6345 | 3798 | `	return SXRET_OK;` |
|     3175 | 3799 | `}` |
|        - | 3800 | `/*` |
|        - | 3801 | ` * An abstract property-hook stub (__phl_hook_{get,set}_NAME) is satisfied by` |
|        - | 3802 | ` * the class declaring a BACKED (non-abstract) property NAME: php lets a` |
|        - | 3803 | `` * backed property implement `{ get; set; }` requirements -- its raw read IS`` |
|        - | 3804 | ` * the default get, and its raw write the default set unless it is readonly.` |
|        - | 3805 | `` * That holds for a backed property with hooks of its own, too: `public $x`` |
|        - | 3806 | `` * { set => ...; }` still reads its store, so it answers an abstract get. A`` |
|        - | 3807 | ` * concrete hook override replaced the stub in hMethod already, so a stub` |
|        - | 3808 | ` * surviving next to a VIRTUAL property means that hook is still missing.` |
|        - | 3809 | ` */` |
|      164 | 3810 | `static int GenStateAbstractHookSatisfied(ph7_class *pClass,const SyString *pMName)` |
|        5 | 3811 | `{` |
|        - | 3812 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|        - | 3813 | `	ph7_class_attr *pProp;` |
|      164 | 3814 | `	if( pMName->nByte <= nPfx` |
|      125 | 3815 | `	 \|\| (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) != 0` |
|       45 | 3816 | `	  && SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) != 0) ){` |
|       92 | 3817 | `		return 0; /* not a hook stub */` |
|        - | 3818 | `	}` |
|       81 | 3819 | `	pProp = PH7_ClassExtractAttribute(pClass,&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|       81 | 3820 | `	return pProp != 0` |
|       76 | 3821 | `		&& (pProp->iFlags & (PH7_CLASS_ATTR_ABSTRACT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|       38 | 3822 | `			\|PH7_CLASS_ATTR_HOOK_VIRTUAL)) == 0` |
|      155 | 3823 | `		&& (pMName->zString[sizeof("__phl_hook_")-1] == 'g'` |
|       41 | 3824 | `		 \|\| (pProp->iFlags & PH7_CLASS_ATTR_READONLY) == 0);` |
|       87 | 3825 | `}` |
|        - | 3826 | `/*` |
|        - | 3827 | ` * Append an abstract member's display name to the message blob, translating a` |
|        - | 3828 | `` * property-hook stub (__phl_hook_get_x) to php's `$x::get` form.`` |
|        - | 3829 | ` */` |
|       42 | 3830 | `static void GenStateAppendAbstractMemberName(SyBlob *pMsg,const SyString *pMName)` |
|        4 | 3831 | `{` |
|        - | 3832 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|       42 | 3833 | `	if( pMName->nByte > nPfx` |
|       26 | 3834 | `	 && (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) == 0` |
|        1 | 3835 | `	  \|\| SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) == 0) ){` |
|        3 | 3836 | `		SyBlobAppend(pMsg,"$",1);` |
|        3 | 3837 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|        3 | 3838 | `		SyBlobAppend(pMsg,"::",2);` |
|        3 | 3839 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[sizeof("__phl_hook_")-1],3);` |
|        3 | 3840 | `		return;` |
|        - | 3841 | `	}` |
|       44 | 3842 | `	SyBlobAppend(pMsg,(const void *)pMName->zString,pMName->nByte);` |
|       25 | 3843 | `}` |
|        - | 3844 | `/*` |
|        - | 3845 | ` * ---------------------------------------------------------------------------` |
|        - | 3846 | `` * php's `#[\Override]` (8.3, widened to properties in 8.5).`` |
|        - | 3847 | ` *` |
|        - | 3848 | ` * The attribute is a CLAIM the engine checks where the member is written: the` |
|        - | 3849 | ` * name must already exist above, so a typo, a renamed parent method or a base` |
|        - | 3850 | ` * class that dropped one is a fatal at the declaration instead of a method` |
|        - | 3851 | ` * nobody ever calls. php runs it at class LINK time, after inheritance, and its` |
|        - | 3852 | ` * rules are the inheritance rules rather than a name search:` |
|        - | 3853 | ` *` |
|        - | 3854 | ` *   - a PRIVATE parent member is not inherited, so it is not something to` |
|        - | 3855 | ` *     override;` |
|        - | 3856 | ` *   - the CONSTRUCTOR is exempt from php's inheritance signature check unless it` |
|        - | 3857 | ` *     is abstract (or an interface's), and #[\Override] follows that exemption —` |
|        - | 3858 | `` *     a concrete parent `__construct` does NOT satisfy the claim while an`` |
|        - | 3859 | ` *     abstract one does;` |
|        - | 3860 | ` *   - a method matches CASE-INSENSITIVELY and a property case-SENSITIVELY, which` |
|        - | 3861 | ` *     is php's rule for the two namespaces everywhere else;` |
|        - | 3862 | ` *   - an interface counts for a method, at any depth and through any ancestor;` |
|        - | 3863 | ` *   - a TRAIT used by this very class does not: its method is the using class's` |
|        - | 3864 | ` *     own, and php reports the USING class's name when the claim fails.` |
|        - | 3865 | ` * ---------------------------------------------------------------------------` |
|        - | 3866 | ` */` |
|        - | 3867 | `#define GEN_OVERRIDE_ATTR "Override"` |
|        - | 3868 | ``/* Does this member carry `#[\Override]`? The compiler resolves an attribute name`` |
|        - | 3869 | ` * to its fully-qualified spelling and class names are case-insensitive, so one` |
|        - | 3870 | ` * case-folded compare against the whole set is the test. */` |
|    10030 | 3871 | `static int GenStateHasOverrideAttr(SySet *pAttrs)` |
|        5 | 3872 | `{` |
|    10035 | 3873 | `	ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|        - | 3874 | `	sxu32 n;` |
|    10197 | 3875 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; ++n ){` |
|      246 | 3876 | `		if( aAttr[n].sName.nByte == sizeof(GEN_OVERRIDE_ATTR)-1` |
|      170 | 3877 | `		 && SyStrnicmp(aAttr[n].sName.zString,GEN_OVERRIDE_ATTR,` |
|       42 | 3878 | `			sizeof(GEN_OVERRIDE_ATTR)-1) == 0 ){` |
|       87 | 3879 | `			return 1;` |
|        - | 3880 | `		}` |
|       86 | 3881 | `	}` |
|     9951 | 3882 | `	return 0;` |
|     5020 | 3883 | `}` |
|        - | 3884 | `/* Does an interface reachable from pClass -- its own, or any ancestor's --` |
|        - | 3885 | ` * declare this method? An interface that extends others already carries their` |
|        - | 3886 | ` * stubs in its own table, so one level of lookup per interface is enough. */` |
|       30 | 3887 | `static int GenStateIfaceDeclaresMethod(ph7_class *pClass,const SyString *pName)` |
|        1 | 3888 | `{` |
|       67 | 3889 | `	for( ; pClass ; pClass = pClass->pBase ){` |
|       45 | 3890 | `		ph7_class **apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|        - | 3891 | `		sxu32 n;` |
|       47 | 3892 | `		for( n = 0 ; n < SySetUsed(&pClass->aInterface) ; ++n ){` |
|       10 | 3893 | `			if( apIface[n]` |
|       11 | 3894 | `			 && PH7_ClassExtractMethod(apIface[n],pName->zString,pName->nByte) ){` |
|        9 | 3895 | `				return 1;` |
|        - | 3896 | `			}` |
|        2 | 3897 | `		}` |
|       19 | 3898 | `	}` |
|       23 | 3899 | `	return 0;` |
|       16 | 3900 | `}` |
|        - | 3901 | `/* Is there a parent METHOD this one may claim to override? */` |
|       50 | 3902 | `static int GenStateOverridesMethod(ph7_class *pClass,const SyString *pName)` |
|        1 | 3903 | `{` |
|       79 | 3904 | `	int bCtor = pName->nByte == sizeof("__construct")-1` |
|       50 | 3905 | `		&& SyStrnicmp(pName->zString,"__construct",sizeof("__construct")-1) == 0;` |
|        - | 3906 | `	ph7_class *pWalk;` |
|       65 | 3907 | `	for( pWalk = pClass->pBase ; pWalk ; pWalk = pWalk->pBase ){` |
|       35 | 3908 | `		ph7_class_method *pMeth = PH7_ClassExtractMethod(pWalk,pName->zString,pName->nByte);` |
|       35 | 3909 | `		if( pMeth == 0 \|\| pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       11 | 3910 | `			continue;` |
|        - | 3911 | `		}` |
|       25 | 3912 | `		if( bCtor && (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|        5 | 3913 | `			continue;   /* php exempts a concrete parent constructor */` |
|        - | 3914 | `		}` |
|       21 | 3915 | `		return 1;` |
|      ! 0 | 3916 | `	}` |
|       31 | 3917 | `	return GenStateIfaceDeclaresMethod(pClass,pName);` |
|       26 | 3918 | `}` |
|        - | 3919 | `/* Is there a parent PROPERTY this one may claim to override? Interfaces declare` |
|        - | 3920 | ` * none, so this is the base chain alone. */` |
|       22 | 3921 | `static int GenStateOverridesProp(ph7_class *pClass,const SyString *pName)` |
|        2 | 3922 | `{` |
|        - | 3923 | `	ph7_class *pWalk;` |
|       32 | 3924 | `	for( pWalk = pClass->pBase ; pWalk ; pWalk = pWalk->pBase ){` |
|       22 | 3925 | `		ph7_class_attr *pAttr = PH7_ClassExtractAttribute(pWalk,pName->zString,pName->nByte);` |
|       22 | 3926 | `		if( pAttr && pAttr->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
|       14 | 3927 | `			return 1;` |
|        - | 3928 | `		}` |
|        5 | 3929 | `	}` |
|       11 | 3930 | `	return 0;` |
|       13 | 3931 | `}` |
|        - | 3932 | `/*` |
|        - | 3933 | `` * Verify every `#[\Override]` the class DECLARED, in php's order: the methods`` |
|        - | 3934 | ` * first and then the properties, each in declaration order, and the first` |
|        - | 3935 | ` * failure is the whole diagnostic (it is a fatal).` |
|        - | 3936 | ` *` |
|        - | 3937 | ` * The two sets are collected BEFORE inheritance runs -- an inherited method` |
|        - | 3938 | ` * keeps the parent's attribute record and php does not re-check it there -- and` |
|        - | 3939 | ` * verified after, which is when the answer exists.` |
|        - | 3940 | ` */` |
|     6772 | 3941 | `static sxi32 GenStateCollectOverrides(ph7_gen_state *pGen,ph7_class *pClass,` |
|        - | 3942 | `	SySet *pMeths,SySet *pProps)` |
|        5 | 3943 | `{` |
|        - | 3944 | `	static const sxu32 nHookPfx = sizeof("__phl_hook_get_")-1;` |
|        - | 3945 | `	SyHashEntry *pEntry;` |
|     6777 | 3946 | `	SySetInit(pMeths,&pGen->pVm->sAllocator,sizeof(ph7_class_method *));` |
|     6777 | 3947 | `	SySetInit(pProps,&pGen->pVm->sAllocator,sizeof(ph7_class_attr *));` |
|     6777 | 3948 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|    13501 | 3949 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|     6729 | 3950 | `		ph7_class_method *pMeth = (ph7_class_method *)pEntry->pUserData;` |
|     6729 | 3951 | `		SyString *pName = &pMeth->sFunc.sName;` |
|        - | 3952 | `		/* A property hook is compiled to a method here and is a PROPERTY in php,` |
|        - | 3953 | `		 * so the property arm below owns its claim. */` |
|     6724 | 3954 | `		if( pName->nByte > nHookPfx` |
|     3589 | 3955 | `		 && SyMemcmp((const void *)pName->zString,(const void *)"__phl_hook_",` |
|      222 | 3956 | `			sizeof("__phl_hook_")-1) == 0 ){` |
|      403 | 3957 | `			continue;` |
|        - | 3958 | `		}` |
|     6331 | 3959 | `		if( GenStateHasOverrideAttr(&pMeth->sFunc.aAttrs) ){` |
|       59 | 3960 | `			SySetPut(pMeths,(const void *)&pMeth);` |
|       29 | 3961 | `		}` |
|        5 | 3962 | `	}` |
|     6777 | 3963 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|    10481 | 3964 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|     3709 | 3965 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     3709 | 3966 | `		if( GenStateHasOverrideAttr(&pAttr->aAttrs) ){` |
|       29 | 3967 | `			SySetPut(pProps,(const void *)&pAttr);` |
|       13 | 3968 | `		}` |
|        5 | 3969 | `	}` |
|     6777 | 3970 | `	return SXRET_OK;` |
|        5 | 3971 | `}` |
|     6370 | 3972 | `static sxi32 GenStateCheckOverrides(ph7_gen_state *pGen,ph7_class *pClass,` |
|        - | 3973 | `	SySet *pMeths,SySet *pProps)` |
|        5 | 3974 | `{` |
|     6375 | 3975 | `	ph7_class_method **apMeth = (ph7_class_method **)SySetBasePtr(pMeths);` |
|     6375 | 3976 | `	ph7_class_attr **apProp = (ph7_class_attr **)SySetBasePtr(pProps);` |
|        - | 3977 | `	sxu32 n;` |
|     6375 | 3978 | `	if( (pClass->iFlags & PH7_CLASS_LINT_UNBOUND) \|\| pGen->bDeclCheck ){` |
|        - | 3979 | `		/* A base this lint could not see may well DECLARE the member; php reports` |
|        - | 3980 | `		 * an #[\Override] mismatch only where it early-binds, and it early-binds` |
|        - | 3981 | `		 * nothing it cannot link. */` |
|      195 | 3982 | `		return SXRET_OK;` |
|        - | 3983 | `	}` |
|        - | 3984 | `	/* hMethod is a LIFO iteration list (SyHashInsert), so the collected order is` |
|        - | 3985 | `	 * the REVERSE of the declaration order; hAttr is a FIFO one` |
|        - | 3986 | `	 * (SyHashInsertTail) and needs no such turn. php reports the first member it` |
|        - | 3987 | `	 * finds in declaration order and stops. */` |
|     6213 | 3988 | `	for( n = SySetUsed(pMeths) ; n > 0 ; --n ){` |
|       51 | 3989 | `		ph7_class_method *pMeth = apMeth[n - 1];` |
|       51 | 3990 | `		if( !GenStateOverridesMethod(pClass,&pMeth->sFunc.sName) ){` |
|       34 | 3991 | `			return PH7_GenCompileError(&(*pGen),E_ERROR,pMeth->sFunc.nLine,` |
|        - | 3992 | `				"%z::%z() has #[\\Override] attribute, but no matching parent method exists",` |
|       11 | 3993 | `				&pClass->sDisp,&pMeth->sFunc.sName);` |
|        - | 3994 | `		}` |
|       15 | 3995 | `	}` |
|     6175 | 3996 | `	for( n = 0 ; n < SySetUsed(pProps) ; ++n ){` |
|       24 | 3997 | `		if( !GenStateOverridesProp(pClass,&apProp[n]->sName) ){` |
|        - | 3998 | `			/* php reports the CLASS's line for a property, not the property's. */` |
|       16 | 3999 | `			return PH7_GenCompileError(&(*pGen),E_ERROR,pClass->nLine,` |
|        - | 4000 | `				"%z::$%z has #[\\Override] attribute, but no matching parent property exists",` |
|       10 | 4001 | `				&pClass->sDisp,&apProp[n]->sName);` |
|        - | 4002 | `		}` |
|        8 | 4003 | `	}` |
|     6153 | 4004 | `	return SXRET_OK;` |
|     3190 | 4005 | `}` |
|        - | 4006 | `/*` |
|        - | 4007 | ` * Check that a concrete class has no remaining abstract methods.` |
|        - | 4008 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|        - | 4009 | ` */` |
|        - | 4010 | `/*` |
|        - | 4011 | ` * The interface FURTHEST up that declares this method name. php attributes an` |
|        - | 4012 | ` * unimplemented method to the interface that first ASKED for it, and an` |
|        - | 4013 | ` * interface reaches its parents through two containers: pBase (the first name` |
|        - | 4014 | `` * after `extends`) and aInterface (every one after that). Following only pBase`` |
|        - | 4015 | `` * stopped at the restating interface, so `interface B extends A, S` reported`` |
|        - | 4016 | `` * `B::g` where php reports `S::g`.`` |
|        - | 4017 | ` *` |
|        - | 4018 | ` * Depth-bounded like the Throwable walk beside it: an interface graph cannot` |
|        - | 4019 | ` * cycle (every parent is already compiled), and the bound costs nothing.` |
|        - | 4020 | ` */` |
|       44 | 4021 | `static ph7_class * GenStateIfaceDeclaringAt(ph7_class *pIface,const SyString *pMName,int iDepth)` |
|        4 | 4022 | `{` |
|       48 | 4023 | `	ph7_class *pDeepest = 0;` |
|        - | 4024 | `	ph7_class **apUp;` |
|        - | 4025 | `	sxu32 i;` |
|       48 | 4026 | `	if( pIface == 0 \|\| iDepth > 32 ){` |
|       24 | 4027 | `		return 0;` |
|        - | 4028 | `	}` |
|       28 | 4029 | `	if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|       22 | 4030 | `		pDeepest = pIface;` |
|        9 | 4031 | `	}` |
|        - | 4032 | `	{` |
|       28 | 4033 | `		ph7_class *pUp = GenStateIfaceDeclaringAt(pIface->pBase,pMName,iDepth + 1);` |
|       28 | 4034 | `		if( pUp ){` |
|      ! 0 | 4035 | `			pDeepest = pUp;` |
|      ! 0 | 4036 | `		}` |
|        - | 4037 | `	}` |
|       28 | 4038 | `	apUp = (ph7_class **)SySetBasePtr(&pIface->aInterface);` |
|       30 | 4039 | `	for( i = 0 ; i < SySetUsed(&pIface->aInterface) ; ++i ){` |
|        3 | 4040 | `		ph7_class *pUp = GenStateIfaceDeclaringAt(apUp[i],pMName,iDepth + 1);` |
|        3 | 4041 | `		if( pUp ){` |
|        3 | 4042 | `			pDeepest = pUp;` |
|        1 | 4043 | `		}` |
|        2 | 4044 | `	}` |
|       28 | 4045 | `	return pDeepest;` |
|       26 | 4046 | `}` |
|        - | 4047 | `/*` |
|        - | 4048 | ` * Is this method-table name one of php's property HOOKS rather than a method?` |
|        - | 4049 | ` * php verifies a class's abstract METHODS first and its abstract property hooks` |
|        - | 4050 | ` * in a second pass, so the two groups do not interleave in the message however` |
|        - | 4051 | ` * the source is written.` |
|        - | 4052 | ` */` |
|      120 | 4053 | `static int GenStateIsHookName(const SyString *pMName)` |
|        4 | 4054 | `{` |
|        - | 4055 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|      128 | 4056 | `	return pMName->nByte > nPfx` |
|      124 | 4057 | `	 && (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) == 0` |
|        4 | 4058 | `	  \|\| SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) == 0);` |
|        4 | 4059 | `}` |
|        - | 4060 | `/*` |
|        - | 4061 | ` * php lists the unimplemented abstract methods in its own FUNCTION-TABLE order,` |
|        - | 4062 | ` * which is the order it LINKS a class in: the class's own declarations, then` |
|        - | 4063 | ` * inheritance, then the traits, then the interfaces -- each of those recursively.` |
|        - | 4064 | `` * So `class C extends P implements I { use T; }` reads `P::pa, C::ta, I::ia`,`` |
|        - | 4065 | `` * with the trait's ahead of the interface's though the `use` is written inside`` |
|        - | 4066 | `` * the body and the `implements` in the header; and `interface K extends J`` |
|        - | 4067 | `` * extends I` reads `K::ka, J::ja, I::ia`. A walk of this engine's own method hash`` |
|        - | 4068 | ` * reads none of that: it is one flat table, in an order the hash decides. The` |
|        - | 4069 | ` * sequence is rebuilt for the message rather than in hMethod itself, which` |
|        - | 4070 | ` * get_class_methods() and Reflection also read.` |
|        - | 4071 | ` *` |
|        - | 4072 | ` * Appends every method DECLARED by pSrc that is still an unimplemented abstract` |
|        - | 4073 | ` * of pClass and is not already in *pOrder, then recurses. bHooks selects which of` |
|        - | 4074 | ` * php's two passes this is.` |
|        - | 4075 | ` */` |
|      276 | 4076 | `static void GenStateOrderAbstractsFrom(ph7_class *pClass,ph7_class *pSrc,SySet *pOrder,` |
|        - | 4077 | `	int bHooks,int iDepth)` |
|        4 | 4078 | `{` |
|        - | 4079 | `	SyHashEntry *pEntry;` |
|        - | 4080 | `	ph7_class **apSrc;` |
|        - | 4081 | `	sxu32 n;` |
|      280 | 4082 | `	if( pSrc == 0 \|\| iDepth > 32 ){` |
|      112 | 4083 | `		return;` |
|        - | 4084 | `	}` |
|        - | 4085 | ``	/* Declaration order, not the hash's own LIFO walk: `abstract function pa();`` |
|        - | 4086 | ``	 * abstract function pb();` must list pa first. (And this is a nested walk of`` |
|        - | 4087 | `	 * another class's table -- SyHash carries a single shared loop cursor, so the` |
|        - | 4088 | `	 * cursor-based iterator could not be used here in any case.) */` |
|      432 | 4089 | `	for( pEntry = SyHashTailEntry(&pSrc->hMethod) ; pEntry ; pEntry = SyHashEntryPrev(pEntry) ){` |
|      264 | 4090 | `		ph7_class_method *pSrcMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - | 4091 | `		ph7_class_method *pMeth;` |
|        - | 4092 | `		ph7_class_method **apSeen;` |
|      264 | 4093 | `		int bSeen = 0;` |
|      264 | 4094 | `		if( pSrcMeth->sFunc.pUserData != (void *)pSrc ){` |
|        - | 4095 | `			/* Inherited into pSrc's table; the class that DECLARED it names it, and` |
|        - | 4096 | `			 * the recursion below reaches that one in php's own link order. */` |
|      180 | 4097 | `			continue;` |
|        - | 4098 | `		}` |
|      124 | 4099 | `		if( GenStateIsHookName(&pSrcMeth->sFunc.sName) != bHooks ){` |
|       64 | 4100 | `			continue;` |
|        - | 4101 | `		}` |
|       94 | 4102 | `		pMeth = PH7_ClassExtractMethod(pClass,` |
|       30 | 4103 | `			SyStringData(&pSrcMeth->sFunc.sName),SyStringLength(&pSrcMeth->sFunc.sName));` |
|       64 | 4104 | `		if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       15 | 4105 | `			continue;` |
|        - | 4106 | `		}` |
|       52 | 4107 | `		if( GenStateAbstractHookSatisfied(pClass,&pMeth->sFunc.sName) ){` |
|      ! 0 | 4108 | `			continue;` |
|        - | 4109 | `		}` |
|       52 | 4110 | `		apSeen = (ph7_class_method **)SySetBasePtr(pOrder);` |
|       92 | 4111 | `		for( n = 0 ; n < SySetUsed(pOrder) ; ++n ){` |
|       43 | 4112 | `			if( apSeen[n] == pMeth ){` |
|      ! 0 | 4113 | `				bSeen = 1;` |
|      ! 0 | 4114 | `				break;` |
|        - | 4115 | `			}` |
|       23 | 4116 | `		}` |
|       52 | 4117 | `		if( !bSeen ){` |
|       52 | 4118 | `			SySetPut(pOrder,(const void *)&pMeth);` |
|       24 | 4119 | `		}` |
|       28 | 4120 | `	}` |
|      172 | 4121 | `	GenStateOrderAbstractsFrom(pClass,pSrc->pBase,pOrder,bHooks,iDepth + 1);` |
|      172 | 4122 | `	apSrc = (ph7_class **)SySetBasePtr(&pSrc->aTrait);` |
|      184 | 4123 | `	for( n = 0 ; n < SySetUsed(&pSrc->aTrait) ; ++n ){` |
|       14 | 4124 | `		GenStateOrderAbstractsFrom(pClass,apSrc[n],pOrder,bHooks,iDepth + 1);` |
|        8 | 4125 | `	}` |
|      172 | 4126 | `	apSrc = (ph7_class **)SySetBasePtr(&pSrc->aInterface);` |
|      208 | 4127 | `	for( n = 0 ; n < SySetUsed(&pSrc->aInterface) ; ++n ){` |
|       40 | 4128 | `		GenStateOrderAbstractsFrom(pClass,apSrc[n],pOrder,bHooks,iDepth + 1);` |
|       22 | 4129 | `	}` |
|      142 | 4130 | `}` |
|        - | 4131 | `/*` |
|        - | 4132 | ` * php names at most THREE of them and then writes ", ..."; the COUNT in the` |
|        - | 4133 | ` * sentence is still the whole number.` |
|        - | 4134 | ` */` |
|        - | 4135 | `#define GEN_ABSTRACT_LIST_MAX 3` |
|        - | 4136 | `/*` |
|        - | 4137 | ` * Does [pClass] leave an abstract method unimplemented? Answers the COUNT, and` |
|        - | 4138 | ` * on a non-zero one appends php's sentence to *pMsg (which the caller owns).` |
|        - | 4139 | ` *` |
|        - | 4140 | ` * php asks this question at two different MOMENTS and the answer has to be the` |
|        - | 4141 | ` * same one, so it is a single routine: at link time for a named class, and when` |
|        - | 4142 | `` * the `new` EXECUTES for an anonymous one -- whose declaration is an expression,`` |
|        - | 4143 | `` * so a `new` on a branch nothing takes is never asked at all.`` |
|        - | 4144 | ` */` |
|     6570 | 4145 | `PH7_PRIVATE sxu32 PH7_ClassAbstractGap(ph7_vm *pVm,ph7_class *pClass,SyBlob *pMsg)` |
|        5 | 4146 | `{` |
|        - | 4147 | `	ph7_class_method *pMeth;` |
|        - | 4148 | `	SyHashEntry *pEntry;` |
|        - | 4149 | `	sxu32 nAbstract;` |
|        - | 4150 | `	SyBlob sMsg;` |
|        - | 4151 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|     6575 | 4152 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      169 | 4153 | `		return 0;` |
|        - | 4154 | `	}` |
|     6411 | 4155 | `	if( pClass->iFlags & PH7_CLASS_LINT_UNBOUND ){` |
|        - | 4156 | `		/* A trait this lint could not see is exactly where the implementations` |
|        - | 4157 | ``		 * usually are (`class C implements ArrayAccess { use HasDataTrait; }`),`` |
|        - | 4158 | `		 * so the count would be of methods the class does have. */` |
|       99 | 4159 | `		return 0;` |
|        - | 4160 | `	}` |
|        - | 4161 | `	/* Count abstract methods */` |
|     6317 | 4162 | `	nAbstract = 0;` |
|     6317 | 4163 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|    26683 | 4164 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|    17215 | 4165 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|    17215 | 4166 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      121 | 4167 | `			if( GenStateAbstractHookSatisfied(pClass,&pMeth->sFunc.sName) ){` |
|       73 | 4168 | `				continue; /* hook requirement met by a plain property (php) */` |
|        - | 4169 | `			}` |
|       52 | 4170 | `			nAbstract++;` |
|       24 | 4171 | `		}` |
|        5 | 4172 | `	}` |
|     6317 | 4173 | `	if( nAbstract == 0 ){` |
|     6287 | 4174 | `		return 0;` |
|        - | 4175 | `	}` |
|        - | 4176 | `	/* Build the error message listing all abstract methods with origins */` |
|       34 | 4177 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       34 | 4178 | `	if( pClass->sDisp.nByte != pClass->sName.nByte ){` |
|        - | 4179 | `		/* An ANONYMOUS class gets php's shorter wording -- it cannot be "declared` |
|        - | 4180 | `		 * abstract", there being no declaration to put the word on. */` |
|        6 | 4181 | `		SyBlobFormat(&sMsg,"Class %z must implement %u abstract method%s (",` |
|        2 | 4182 | `			&pClass->sDisp,nAbstract,(nAbstract > 1 ? "s" : ""));` |
|        4 | 4183 | `	}else{` |
|       30 | 4184 | `		SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|        - | 4185 | `			"be declared abstract or implement the remaining method%s (",` |
|       13 | 4186 | `			&pClass->sDisp,nAbstract,` |
|       13 | 4187 | `			(nAbstract > 1 ? "s" : ""),` |
|       13 | 4188 | `			(nAbstract > 1 ? "s" : ""));` |
|        - | 4189 | `	}` |
|        - | 4190 | `	/* Second pass: list methods with origins, in php's table order and capped */` |
|        - | 4191 | `	{` |
|       34 | 4192 | `		sxu32 nListed = 0;` |
|        - | 4193 | `		sxu32 nOrder;` |
|        - | 4194 | `		SySet aOrder; /* ph7_class_method * , php's function-table order */` |
|        - | 4195 | `		ph7_class_method **apOrder;` |
|       34 | 4196 | `		SySetInit(&aOrder,&pVm->sAllocator,sizeof(ph7_class_method *));` |
|        - | 4197 | `		/* Methods first, then property hooks: php's two verification passes. */` |
|       34 | 4198 | `		GenStateOrderAbstractsFrom(pClass,pClass,&aOrder,0,0);` |
|       34 | 4199 | `		GenStateOrderAbstractsFrom(pClass,pClass,&aOrder,1,0);` |
|       34 | 4200 | `		apOrder = (ph7_class_method **)SySetBasePtr(&aOrder);` |
|       76 | 4201 | `		for( nOrder = 0 ; nOrder < SySetUsed(&aOrder) ; ++nOrder ){` |
|       48 | 4202 | `			ph7_class *pOrigin = 0;` |
|        - | 4203 | `			SyString *pMName;` |
|       48 | 4204 | `			pMeth = apOrder[nOrder];` |
|       48 | 4205 | `			if( nListed >= GEN_ABSTRACT_LIST_MAX ){` |
|        3 | 4206 | `				SyBlobAppend(&sMsg,", ...",sizeof(", ...")-1);` |
|        3 | 4207 | `				break;` |
|        - | 4208 | `			}` |
|       46 | 4209 | `			pMName = &pMeth->sFunc.sName;` |
|       46 | 4210 | `			if( nListed > 0 ){` |
|       15 | 4211 | `				SyBlobAppend(&sMsg,", ",2);` |
|        6 | 4212 | `			}` |
|        - | 4213 | `			/* Find the origin of this abstract method.` |
|        - | 4214 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|        - | 4215 | `			 * inheritance chains) take precedence for interface-declared` |
|        - | 4216 | `			 * methods. Abstract class methods only win when the class` |
|        - | 4217 | `			 * itself declared the abstract method (not inherited from` |
|        - | 4218 | `			 * an interface). Trait methods are adopted into the using` |
|        - | 4219 | `			 * class's namespace.` |
|        - | 4220 | `			 */` |
|        - | 4221 | `			{` |
|        - | 4222 | `				ph7_class **apIface;` |
|        - | 4223 | `				ph7_class **apTrait;` |
|        - | 4224 | `				ph7_class *pWalk;` |
|        - | 4225 | `				sxu32 i;` |
|        - | 4226 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|        - | 4227 | `				 * (one that was written in the class body, not inherited from an` |
|        - | 4228 | `				 * interface). PHP attributes origin to the declaring class.` |
|        - | 4229 | `				 */` |
|       46 | 4230 | `				if( pClass->pBase ){` |
|       34 | 4231 | `					pWalk = pClass->pBase;` |
|       78 | 4232 | `					while( pWalk ){` |
|        - | 4233 | `						ph7_class_method *pParentMeth;` |
|       48 | 4234 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|       48 | 4235 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|        - | 4236 | `							/* Exclude methods that came from an interface anywhere` |
|        - | 4237 | `							 * in this class's ancestor chain.` |
|        - | 4238 | `							 */` |
|       36 | 4239 | `							int fromIface = 0;` |
|       36 | 4240 | `							ph7_class *pAnc = pWalk;` |
|       70 | 4241 | `							while( pAnc ){` |
|        - | 4242 | `								ph7_class **apPI;` |
|        - | 4243 | `								sxu32 j;` |
|       46 | 4244 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|       46 | 4245 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|       10 | 4246 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|       10 | 4247 | `										fromIface = 1;` |
|       10 | 4248 | `										break;` |
|        - | 4249 | `									}` |
|      ! 0 | 4250 | `								}` |
|       46 | 4251 | `								if( fromIface ) break;` |
|       38 | 4252 | `								pAnc = pAnc->pBase;` |
|        4 | 4253 | `							}` |
|       36 | 4254 | `							if( !fromIface ){` |
|        - | 4255 | `								/* php attributes the origin to the class that DECLARED the` |
|        - | 4256 | `								 * method, so keep climbing: breaking at the first ancestor` |
|        - | 4257 | `								 * that HAS it named the nearest one, and` |
|        - | 4258 | `` 								 * `abstract class G { abstract ga } abstract class P extends G` `` |
|        - | 4259 | ``								 * reported `P::ga` where php reports `G::ga`. */`` |
|       28 | 4260 | `								pOrigin = pWalk;` |
|       12 | 4261 | `							}` |
|       16 | 4262 | `						}` |
|       48 | 4263 | `						pWalk = pWalk->pBase;` |
|        4 | 4264 | `					}` |
|       15 | 4265 | `				}` |
|        - | 4266 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|        - | 4267 | `				 * each interface's own parent chain for the deepest origin.` |
|        - | 4268 | `				 */` |
|       46 | 4269 | `				if( !pOrigin ){` |
|       26 | 4270 | `					pWalk = pClass;` |
|       64 | 4271 | `					while( pWalk && !pOrigin ){` |
|       42 | 4272 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|       44 | 4273 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|       22 | 4274 | `							ph7_class *pDeepest = GenStateIfaceDeclaringAt(apIface[i],pMName,0);` |
|       22 | 4275 | `							if( pDeepest ){` |
|       20 | 4276 | `								pOrigin = pDeepest;` |
|       20 | 4277 | `								break;` |
|        - | 4278 | `							}` |
|        2 | 4279 | `						}` |
|       42 | 4280 | `						pWalk = pWalk->pBase;` |
|        4 | 4281 | `					}` |
|       11 | 4282 | `				}` |
|        - | 4283 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|       46 | 4284 | `				if( !pOrigin ){` |
|        8 | 4285 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|        8 | 4286 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|        8 | 4287 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|        8 | 4288 | `							pOrigin = pClass;` |
|        8 | 4289 | `							break;` |
|        - | 4290 | `						}` |
|      ! 0 | 4291 | `					}` |
|        3 | 4292 | `				}` |
|        - | 4293 | `			}` |
|       46 | 4294 | `			if( pOrigin ){` |
|       46 | 4295 | `				SyBlobFormat(&sMsg,"%z::",&pOrigin->sDisp);` |
|       25 | 4296 | `			}else{` |
|        - | 4297 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|      ! 0 | 4298 | `				SyBlobFormat(&sMsg,"%z::",&pClass->sDisp);` |
|        - | 4299 | `			}` |
|       46 | 4300 | `			GenStateAppendAbstractMemberName(&sMsg,pMName);` |
|       46 | 4301 | `			nListed++;` |
|       25 | 4302 | `		}` |
|       34 | 4303 | `		SySetRelease(&aOrder);` |
|        - | 4304 | `	}` |
|       34 | 4305 | `	SyBlobAppend(&sMsg,")",1);` |
|       34 | 4306 | `	SyBlobAppend(pMsg,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       34 | 4307 | `	SyBlobRelease(&sMsg);` |
|       34 | 4308 | `	return nAbstract;` |
|     3290 | 4309 | `}` |
|        - | 4310 | `/*` |
|        - | 4311 | ` * The link-time half: php refuses a NAMED class that leaves an abstract method` |
|        - | 4312 | ` * unimplemented where the declaration stands, whether or not anything ever` |
|        - | 4313 | ` * instantiates it. An ANONYMOUS one is not asked here -- see PH7_VmNewAnonAbstractGap,` |
|        - | 4314 | ` * the OP_NEW half.` |
|        - | 4315 | ` */` |
|     6340 | 4316 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 | 4317 | `{` |
|        - | 4318 | `	SyBlob sMsg;` |
|        - | 4319 | `	sxi32 rc;` |
|     6345 | 4320 | `	if( pClass->iFlags & PH7_CLASS_ANON ){` |
|      283 | 4321 | `		return SXRET_OK;` |
|        - | 4322 | `	}` |
|     6067 | 4323 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|     6067 | 4324 | `	if( PH7_ClassAbstractGap(pGen->pVm,pClass,&sMsg) == 0 ){` |
|     6041 | 4325 | `		SyBlobRelease(&sMsg);` |
|     6041 | 4326 | `		return SXRET_OK;` |
|        - | 4327 | `	}` |
|       43 | 4328 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|       26 | 4329 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|       30 | 4330 | `	SyBlobRelease(&sMsg);` |
|       30 | 4331 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 4332 | `		return SXERR_ABORT;` |
|        - | 4333 | `	}` |
|       30 | 4334 | `	return SXRET_OK;` |
|     3175 | 4335 | `}` |
|        - | 4336 | `/*` |
|        - | 4337 | ` * Parse a class/interface name reference from the current token stream.` |
|        - | 4338 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|        - | 4339 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|        - | 4340 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|        - | 4341 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|        - | 4342 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|        - | 4343 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|        - | 4344 | ` */` |
|    10912 | 4345 | `PH7_PRIVATE sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|        5 | 4346 | `{` |
|    10917 | 4347 | `	int isAbsolute = 0;` |
|    10917 | 4348 | `	SyToken *pStart = pGen->pIn;` |
|        - | 4349 | `	SyBlob sName;` |
|    10917 | 4350 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|     1195 | 4351 | `		isAbsolute = 1;` |
|     1195 | 4352 | `		pGen->pIn++;` |
|      593 | 4353 | `	}` |
|    10917 | 4354 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|        - | 4355 | ``	/* `namespace\X` names the CURRENT namespace and is fully qualified from there. */`` |
|    10917 | 4356 | `	if( !isAbsolute && GenStateNsRelPrefix(pGen,&pGen->pIn,pGen->pEnd,&sName) ){` |
|       19 | 4357 | `		isAbsolute = 1;` |
|        9 | 4358 | `	}` |
|    10917 | 4359 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       10 | 4360 | `		SyBlobRelease(&sName);` |
|       10 | 4361 | `		pGen->pIn = pStart;` |
|       10 | 4362 | `		return SXERR_INVALID;` |
|        - | 4363 | `	}` |
|    10909 | 4364 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|    10909 | 4365 | `	pGen->pIn++;` |
|    16732 | 4366 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|     5833 | 4367 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      253 | 4368 | `		SyBlobAppend(&sName,"\\",1);` |
|      253 | 4369 | `		pGen->pIn++;` |
|      253 | 4370 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      253 | 4371 | `		pGen->pIn++;` |
|        5 | 4372 | `	}` |
|    10909 | 4373 | `	if( isAbsolute ){` |
|     1213 | 4374 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      607 | 4375 | `	}else{` |
|        - | 4376 | `		SyString sRaw;` |
|     9701 | 4377 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|     9701 | 4378 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|        - | 4379 | `	}` |
|    10909 | 4380 | `	SyBlobRelease(&sName);` |
|    10909 | 4381 | `	return SXRET_OK;` |
|     5457 | 4382 | `}` |
|        - | 4383 | `/*` |
|        - | 4384 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|        - | 4385 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|        - | 4386 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|        - | 4387 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|        - | 4388 | ` * either direction cannot run unbounded.` |
|        - | 4389 | ` */` |
|        - | 4390 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|      550 | 4391 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|        5 | 4392 | `{` |
|        - | 4393 | `	ph7_class **apParent;` |
|        - | 4394 | `	sxu32 n;` |
|     1219 | 4395 | `	while( pInterface ){` |
|      679 | 4396 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|      ! 0 | 4397 | `			return FALSE;` |
|        - | 4398 | `		}` |
|      713 | 4399 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|       68 | 4400 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|       14 | 4401 | `			return TRUE;` |
|        - | 4402 | `		}` |
|      669 | 4403 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|      685 | 4404 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|       18 | 4405 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|      ! 0 | 4406 | `				return TRUE;` |
|        - | 4407 | `			}` |
|       10 | 4408 | `		}` |
|      669 | 4409 | `		pInterface = pInterface->pBase;` |
|      669 | 4410 | `		iDepth++;` |
|        5 | 4411 | `	}` |
|      545 | 4412 | `	return FALSE;` |
|      280 | 4413 | `}` |
|      534 | 4414 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|        5 | 4415 | `{` |
|      539 | 4416 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|        5 | 4417 | `}` |
|        - | 4418 | `/*` |
|        - | 4419 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|        - | 4420 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|        - | 4421 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|        - | 4422 | ` */` |
|       10 | 4423 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|        4 | 4424 | `{` |
|       18 | 4425 | `	while( pBase ){` |
|       10 | 4426 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|        2 | 4427 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|        3 | 4428 | `			return TRUE;` |
|        - | 4429 | `		}` |
|       10 | 4430 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|        6 | 4431 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|        3 | 4432 | `			return TRUE;` |
|        - | 4433 | `		}` |
|        5 | 4434 | `		pBase = pBase->pBase;` |
|        1 | 4435 | `	}` |
|        9 | 4436 | `	return FALSE;` |
|        9 | 4437 | `}` |
|        - | 4438 | `/*` |
|        - | 4439 | `` * Compile a single `case NAME [= value];` member of an enum body (PHP 8.1).`` |
|        - | 4440 | ` * A case is stored as a class constant (PH7_CLASS_ATTR_CONSTANT\|ENUMCASE) whose` |
|        - | 4441 | ` * aByteCode holds the BACKING value expression for backed enums (empty for pure` |
|        - | 4442 | ` * enums). The case's runtime value — the singleton instance — is materialized` |
|        - | 4443 | ` * lazily on first access (VmEnumMaterialize, vm.c), matching PHP's lazy` |
|        - | 4444 | ` * backing-value type/duplicate checks. Declaration order is recorded in` |
|        - | 4445 | ` * pClass->aEnumCases for cases().` |
|        - | 4446 | ` */` |
|      184 | 4447 | `static sxi32 GenStateCompileEnumCase(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 | 4448 | `{` |
|      189 | 4449 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 4450 | `	sxu32 nNameLine;      /* php's line for the case: its name's */` |
|        - | 4451 | `	SySet *pInstrContainer;` |
|        - | 4452 | `	ph7_class_attr *pCase;` |
|        - | 4453 | `	SyString *pName;` |
|        - | 4454 | `	sxi32 rc;` |
|      189 | 4455 | `	pGen->pIn++; /* Jump the 'case' keyword */` |
|      189 | 4456 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 4457 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 4458 | `			"Invalid enum case name inside enum '%z'",&pClass->sDisp);` |
|      ! 0 | 4459 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4460 | `			return SXERR_ABORT;` |
|        - | 4461 | `		}` |
|      ! 0 | 4462 | `		goto Synchronize;` |
|        - | 4463 | `	}` |
|      189 | 4464 | `	pName = &pGen->pIn->sData;` |
|      189 | 4465 | `	nNameLine = pGen->pIn->nLine;` |
|        - | 4466 | `	/* Cases share the class-constant namespace (php: "Cannot redefine class constant") */` |
|      189 | 4467 | `	if( SyHashGet(&pClass->hConst,(const void *)pName->zString,pName->nByte) != 0 ){` |
|      ! 0 | 4468 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 4469 | `			"Cannot redefine class constant %z::%z",&pClass->sDisp,pName);` |
|      ! 0 | 4470 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4471 | `			return SXERR_ABORT;` |
|        - | 4472 | `		}` |
|      ! 0 | 4473 | `		goto Synchronize;` |
|        - | 4474 | `	}` |
|      189 | 4475 | `	pCase = PH7_NewClassAttr(pGen->pVm,pName,pGen->pIn->nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - | 4476 | `		PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|      189 | 4477 | `	if( pCase == 0 ){` |
|      ! 0 | 4478 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 4479 | `		return SXERR_ABORT;` |
|        - | 4480 | `	}` |
|      189 | 4481 | `	GenStateConsumeDoc(&(*pGen),&pCase->sDoc);` |
|      189 | 4482 | `	if( GenStateConsumeAttrs(&(*pGen),&pCase->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 4483 | `		return SXERR_ABORT;` |
|        - | 4484 | `	}` |
|      189 | 4485 | `	if( GenStateCheckAttrPlacement(&(*pGen),&pCase->aAttrs,nNameLine,16,16,0,0) == SXERR_ABORT ){` |
|      ! 0 | 4486 | `		return SXERR_ABORT;` |
|        - | 4487 | `	}` |
|      189 | 4488 | `	pGen->pIn++; /* Jump the case name */` |
|      189 | 4489 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) ){` |
|      125 | 4490 | `		if( pClass->nEnumBacking == 0 ){` |
|        8 | 4491 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        2 | 4492 | `				"Case %z of non-backed enum %z must not have a value",pName,&pClass->sDisp);` |
|        6 | 4493 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 4494 | `				return SXERR_ABORT;` |
|        - | 4495 | `			}` |
|        6 | 4496 | `			goto Synchronize;` |
|        - | 4497 | `		}` |
|      121 | 4498 | `		pGen->pIn++; /* Jump the equal sign */` |
|        - | 4499 | `		/* A backing value is a constant expression like any other: same rules, same` |
|        - | 4500 | ``		 * first-offender sentence, and no `new` (it is stored as a class constant). */`` |
|        - | 4501 | `		{` |
|      121 | 4502 | `			const char *zCErr = PH7_GenStateConstExprError(pGen,0);` |
|      121 | 4503 | `			if( zCErr ){` |
|        3 | 4504 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"%s",zCErr);` |
|        3 | 4505 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 4506 | `					return SXERR_ABORT;` |
|        - | 4507 | `				}` |
|        3 | 4508 | `				goto Synchronize;` |
|        - | 4509 | `			}` |
|        - | 4510 | `		}` |
|        - | 4511 | `		/* Compile the backing value expression into the case's own container` |
|        - | 4512 | `		 * (same technique as class constants). */` |
|      119 | 4513 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      119 | 4514 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pCase->aByteCode);` |
|      119 | 4515 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      119 | 4516 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 | 4517 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 4518 | `				"Empty value for enum case %z::%z",&pClass->sDisp,pName);` |
|      ! 0 | 4519 | `		}` |
|      119 | 4520 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      119 | 4521 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      119 | 4522 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4523 | `			return SXERR_ABORT;` |
|        - | 4524 | `		}` |
|       62 | 4525 | `	}else{` |
|       69 | 4526 | `		if( pClass->nEnumBacking != 0 ){` |
|      ! 0 | 4527 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 4528 | `				"Case %z of backed enum %z must have a value",pName,&pClass->sDisp);` |
|      ! 0 | 4529 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 4530 | `				return SXERR_ABORT;` |
|        - | 4531 | `			}` |
|      ! 0 | 4532 | `			goto Synchronize;` |
|        - | 4533 | `		}` |
|        - | 4534 | `	}` |
|      183 | 4535 | `	rc = PH7_ClassInstallAttr(pClass,pCase);` |
|      183 | 4536 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 4537 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 4538 | `		return SXERR_ABORT;` |
|        - | 4539 | `	}` |
|      183 | 4540 | `	SySetPut(&pClass->aEnumCases,(const void *)&pCase);` |
|      183 | 4541 | `	return SXRET_OK;` |
|        3 | 4542 | `Synchronize:` |
|        - | 4543 | `	/* Synchronize with the first semi-colon */` |
|       25 | 4544 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|       19 | 4545 | `		pGen->pIn++;` |
|        3 | 4546 | `	}` |
|        9 | 4547 | `	return SXERR_CORRUPT;` |
|       97 | 4548 | `}` |
|        - | 4549 | `/*` |
|        - | 4550 | ` * Install the enum interface methods (PHP 8.1): cases() for every enum, plus` |
|        - | 4551 | ` * from()/tryFrom() for backed ones. They are NATIVE methods — the very same C` |
|        - | 4552 | ` * bodies an enum declared from C gets — because php's are internal: it reports` |
|        - | 4553 | `` * them as `<internal, prototype BackedEnum>` with no file and no line, and`` |
|        - | 4554 | `` * declares `from(string\|int $value): static` on the prototype rather than the`` |
|        - | 4555 | ` * enum's own backing type.` |
|        - | 4556 | ` *` |
|        - | 4557 | ` * This used to synthesize PHP source forwarding to three global` |
|        - | 4558 | `` * `__phl_enum_*` thunks, which put those names in php's namespace and reported`` |
|        - | 4559 | `` * every enum's three methods as `<user>` at the enum's own line.`` |
|        - | 4560 | ` */` |
|      146 | 4561 | `static sxi32 GenStateCompileEnumMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 | 4562 | `{` |
|      151 | 4563 | `	if( PH7_InstallEnumInterfaceMethods(pGen->pVm,pClass) != SXRET_OK ){` |
|      ! 0 | 4564 | `		PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 4565 | `		return SXERR_ABORT;` |
|        - | 4566 | `	}` |
|      151 | 4567 | `	return SXRET_OK;` |
|       78 | 4568 | `}` |
|        - | 4569 | `/*` |
|        - | 4570 | ` * Magic methods an enum may not declare (php 8.1, zend_enum.c list —` |
|        - | 4571 | ` * __call/__callStatic/__invoke stay allowed).` |
|        - | 4572 | ` */` |
|        - | 4573 | `static const char *azEnumBannedMagic[] = {` |
|        - | 4574 | `	"__construct","__destruct","__clone","__get","__set","__isset","__unset",` |
|        - | 4575 | `	"__toString","__sleep","__wakeup","__serialize","__unserialize","__set_state"` |
|        - | 4576 | `};` |
|        - | 4577 | `/*` |
|        - | 4578 | ` * Enum post-body validation + synthesis: reject declared properties (including` |
|        - | 4579 | ``  * trait-imported ones) and banned magic methods, install the readonly `name` `` |
|        - | 4580 | `` * (and, for backed enums, `value`) instance properties the case singletons`` |
|        - | 4581 | ` * carry, and synthesize cases()/from()/tryFrom(). Runs after trait application` |
|        - | 4582 | ` * and before the class is installed.` |
|        - | 4583 | ` */` |
|      146 | 4584 | `static sxi32 GenStateEnumFinalize(ph7_gen_state *pGen,ph7_class *pClass,sxu32 nLine)` |
|        5 | 4585 | `{` |
|        - | 4586 | `	SyHashEntry *pEntry;` |
|        - | 4587 | `	sxi32 rc;` |
|        - | 4588 | `	sxu32 n;` |
|        - | 4589 | `	/* php: "Enum %s cannot include properties" */` |
|      151 | 4590 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|      151 | 4591 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        3 | 4592 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|        3 | 4593 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        3 | 4594 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine ? pAttr->nLine : nLine,` |
|        1 | 4595 | `				"Enum %z cannot include properties",&pClass->sDisp);` |
|        3 | 4596 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 4597 | `				return SXERR_ABORT;` |
|        - | 4598 | `			}` |
|        3 | 4599 | `			break;` |
|        - | 4600 | `		}` |
|      ! 0 | 4601 | `	}` |
|        - | 4602 | `	/* php: "Enum %s cannot include magic method %s" */` |
|     2049 | 4603 | `	for( n = 0 ; n < SX_ARRAYSIZE(azEnumBannedMagic) ; n++ ){` |
|     2847 | 4604 | `		if( SyHashGet(&pClass->hMethod,(const void *)azEnumBannedMagic[n],` |
|     1903 | 4605 | `			SyStrlen(azEnumBannedMagic[n])) != 0 ){` |
|      ! 0 | 4606 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 4607 | `				"Enum %z cannot include magic method %s",&pClass->sDisp,azEnumBannedMagic[n]);` |
|      ! 0 | 4608 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 4609 | `				return SXERR_ABORT;` |
|        - | 4610 | `			}` |
|      ! 0 | 4611 | `		}` |
|      954 | 4612 | `	}` |
|        - | 4613 | ``	/* Install the case-singleton instance properties: readonly `name` (every`` |
|        - | 4614 | ``	 * enum) and `value` (backed only). Materialization (vm.c) fills them and`` |
|        - | 4615 | `	 * clears the readonly write-once latch; user writes then raise php's` |
|        - | 4616 | `	 * "Cannot modify readonly property" through the normal store path. */` |
|        - | 4617 | `	{` |
|        - | 4618 | `		static const SyString sNameProp = { "name",sizeof("name")-1 };` |
|        - | 4619 | `		static const SyString sValueProp = { "value",sizeof("value")-1 };` |
|        - | 4620 | `		ph7_class_attr *pAttr;` |
|      151 | 4621 | `		pAttr = PH7_NewClassAttr(pGen->pVm,&sNameProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - | 4622 | `			PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      151 | 4623 | `		if( pAttr == 0 ){` |
|      ! 0 | 4624 | `			PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 4625 | `			return SXERR_ABORT;` |
|        - | 4626 | `		}` |
|      151 | 4627 | `		pAttr->nType = MEMOBJ_STRING;` |
|      151 | 4628 | `		SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|      151 | 4629 | `		PH7_ClassInstallAttr(pClass,pAttr);` |
|      151 | 4630 | `		if( pClass->nEnumBacking != 0 ){` |
|       75 | 4631 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&sValueProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - | 4632 | `				PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|       75 | 4633 | `			if( pAttr == 0 ){` |
|      ! 0 | 4634 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 4635 | `				return SXERR_ABORT;` |
|        - | 4636 | `			}` |
|       75 | 4637 | `			pAttr->nType = pClass->nEnumBacking;` |
|       75 | 4638 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|       29 | 4639 | `				SyStringInitFromBuf(&pAttr->sTypeName,"int",sizeof("int")-1);` |
|       16 | 4640 | `			}else{` |
|       49 | 4641 | `				SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|        - | 4642 | `			}` |
|       75 | 4643 | `			PH7_ClassInstallAttr(pClass,pAttr);` |
|       35 | 4644 | `		}` |
|        - | 4645 | `	}` |
|      151 | 4646 | `	return GenStateCompileEnumMethods(&(*pGen),pClass);` |
|       78 | 4647 | `}` |
|        - | 4648 | `/*` |
|        - | 4649 | ` * Deferred class declarations (class/anonymous-class extending an` |
|        - | 4650 | ` * autoloaded parent).` |
|        - | 4651 | ` *` |
|        - | 4652 | ` * A class declaration compiles INLINE while its enclosing file compiles, so a` |
|        - | 4653 | ` * parent/interface/trait that an autoloader would provide is unreachable when` |
|        - | 4654 | ` * the autoloader's own spl_autoload_register() statement has not EXECUTED yet` |
|        - | 4655 | `` * (same-file registration, or `new class extends \App\Child {}` anywhere).`` |
|        - | 4656 | ` * php's model has no such problem: a declaration with unresolved dependencies` |
|        - | 4657 | ` * is declared at its EXECUTION point, in statement order, not hoisted.` |
|        - | 4658 | ` *` |
|        - | 4659 | ` * These helpers reproduce that: before compiling a declaration, scan its` |
|        - | 4660 | `` * header (extends/implements) and body (depth-1 trait `use`) for referenced`` |
|        - | 4661 | ` * names and try to resolve each (firing autoload exactly where the normal` |
|        - | 4662 | ` * compile would). If any name is still missing, the WHOLE declaration is` |
|        - | 4663 | `` * captured as re-compilable source — a reconstructed `namespace`/`use`-import/`` |
|        - | 4664 | ` * doc/attribute/modifier prefix plus the declaration's raw text — recorded in` |
|        - | 4665 | ` * a VmDeferredClass, and OP_CLASS_DEFER is emitted at the declaration site.` |
|        - | 4666 | ` * At runtime (VmExecDeferredClass, vm_include.c) the autoloader is live: each` |
|        - | 4667 | `` * recorded name resolves or throws php's catchable `... not found` Error, and`` |
|        - | 4668 | ` * the chunk re-compiles through VmEvalChunk. An anonymous class re-compiles` |
|        - | 4669 | `` * inside `if (false) { new ... }` (installing the class without instantiating`` |
|        - | 4670 | ` * it) under its original synthesized name via pVm->sDeferAnonName; the site's` |
|        - | 4671 | ` * own OP_NEW then instantiates it with the site-compiled arguments.` |
|        - | 4672 | ` *` |
|        - | 4673 | ` * Behavior shifts only for declarations that previously died with the` |
|        - | 4674 | ` * compile-time "Nonexistent base class" fatal: they now follow php — succeed` |
|        - | 4675 | ` * when the autoloader is registered first, or throw php's catchable` |
|        - | 4676 | `` * `Class/Interface/Trait "X" not found` Error at the declaration point.`` |
|        - | 4677 | ` * A deferred declaration's OTHER compile errors (a body syntax error) are` |
|        - | 4678 | ` * still raised with the file: GenStateCheckDeferredDecl compiles the body once` |
|        - | 4679 | ` * for them alone.` |
|        - | 4680 | ` */` |
|      648 | 4681 | `static void GenStateDeferEmitUses(SyBlob *pOut,SyHash *pTable,const char *zKind)` |
|        5 | 4682 | `{` |
|        - | 4683 | `	SyHashEntry *pEntry;` |
|      653 | 4684 | `	SyHashResetLoopCursor(pTable);` |
|     1061 | 4685 | `	while( (pEntry = SyHashGetNextEntry(pTable)) != 0 ){` |
|       86 | 4686 | `		const char *zFqn = (const char *)pEntry->pUserData;` |
|       86 | 4687 | `		if( zFqn ){` |
|      128 | 4688 | `			SyBlobFormat(pOut,"use %s%s as %.*s;\n",zKind,zFqn,` |
|       84 | 4689 | `				(int)pEntry->nKeyLen,(const char *)pEntry->pKey);` |
|       42 | 4690 | `		}` |
|        2 | 4691 | `	}` |
|      653 | 4692 | `}` |
|        - | 4693 | `/*` |
|        - | 4694 | ` * Parse one class reference at *ppCur (bounded by pEnd) with the SAME` |
|        - | 4695 | ` * namespace/import resolution the real compile uses, and append it to pNames.` |
|        - | 4696 | ` * Advances *ppCur past the reference. Returns SXERR_INVALID on a malformed` |
|        - | 4697 | ` * reference (caller bails out of deferral and lets the normal path report).` |
|        - | 4698 | ` */` |
|     2634 | 4699 | `static sxi32 GenStateDeferRecordRef(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd,` |
|        - | 4700 | `	sxu8 cKind,SySet *pNames)` |
|        5 | 4701 | `{` |
|     2639 | 4702 | `	SyToken *pSavedIn = pGen->pIn;` |
|     2639 | 4703 | `	SyToken *pSavedEnd = pGen->pEnd;` |
|        - | 4704 | `	SyBlob sFqn;` |
|        - | 4705 | `	VmDeferredReq sReq;` |
|        - | 4706 | `	char *zDup;` |
|        - | 4707 | `	sxi32 rc;` |
|     2639 | 4708 | `	SyBlobInit(&sFqn,&pGen->pVm->sAllocator);` |
|     2639 | 4709 | `	pGen->pIn = *ppCur;` |
|     2639 | 4710 | `	pGen->pEnd = pEnd;` |
|     2639 | 4711 | `	rc = GenStateParseClassReference(pGen,&sFqn);` |
|     2639 | 4712 | `	*ppCur = pGen->pIn;` |
|     2639 | 4713 | `	pGen->pIn = pSavedIn;` |
|     2639 | 4714 | `	pGen->pEnd = pSavedEnd;` |
|     2639 | 4715 | `	if( rc != SXRET_OK \|\| SyBlobLength(&sFqn) < 1 ){` |
|        3 | 4716 | `		SyBlobRelease(&sFqn);` |
|        3 | 4717 | `		return SXERR_INVALID;` |
|        - | 4718 | `	}` |
|     3953 | 4719 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     2632 | 4720 | `		(const char *)SyBlobData(&sFqn),SyBlobLength(&sFqn));` |
|     2637 | 4721 | `	if( zDup == 0 ){` |
|      ! 0 | 4722 | `		SyBlobRelease(&sFqn);` |
|      ! 0 | 4723 | `		return SXERR_INVALID;` |
|        - | 4724 | `	}` |
|     2637 | 4725 | `	SyStringInitFromBuf(&sReq.sName,zDup,SyBlobLength(&sFqn));` |
|     2637 | 4726 | `	sReq.cKind = cKind;` |
|     2637 | 4727 | `	SySetPut(pNames,(const void *)&sReq);` |
|     2637 | 4728 | `	SyBlobRelease(&sFqn);` |
|     2637 | 4729 | `	return SXRET_OK;` |
|     1322 | 4730 | `}` |
|        - | 4731 | `/*` |
|        - | 4732 | ` * Look a class/interface/trait name up WITHOUT asking the autoloader: is this` |
|        - | 4733 | ` * name declared right now? php's early binding asks exactly this question --` |
|        - | 4734 | ` * zend_try_early_binding does a plain class-table lookup and gives up if the` |
|        - | 4735 | ` * parent is not there yet, because a compile-time autoload would run user code` |
|        - | 4736 | ` * in the middle of compiling a file.` |
|        - | 4737 | ` */` |
|     2482 | 4738 | `static ph7_class * GenStateFindDeclaredClass(ph7_vm *pVm,const char *zName,sxu32 nByte)` |
|        5 | 4739 | `{` |
|        - | 4740 | `	SyHashEntry *pEntry;` |
|     2487 | 4741 | `	PH7_VmClassNameAnchor(&zName,&nByte);` |
|     2487 | 4742 | `	if( nByte < 1 ){` |
|      ! 0 | 4743 | `		return 0;` |
|        - | 4744 | `	}` |
|     2487 | 4745 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|     2487 | 4746 | `	return pEntry ? (ph7_class *)pEntry->pUserData : 0;` |
|     1246 | 4747 | `}` |
|        - | 4748 | `/*` |
|        - | 4749 | ` * Scan the declaration whose keyword pGen->pIn sits on (class/enum/interface/` |
|        - | 4750 | `` * trait, or an anonymous `class(args)`) WITHOUT consuming tokens. Collects`` |
|        - | 4751 | ` * every referenced dependency name, locates the body braces, and filters the` |
|        - | 4752 | ` * collected names down to the UNRESOLVABLE ones. SXRET_OK with an empty` |
|        - | 4753 | ` * pMissing set means "compile normally"; a non-empty set means "defer". Any` |
|        - | 4754 | ` * structural surprise returns SXERR_INVALID so the normal compile reports it.` |
|        - | 4755 | ` *` |
|        - | 4756 | ` * bNoAutoload picks which question the filter asks -- see its use below.` |
|        - | 4757 | ` */` |
|     7316 | 4758 | `static sxi32 GenStateScanDeferDeps(ph7_gen_state *pGen,int bAnon,int iSelfKind,` |
|        - | 4759 | `	SySet *pMissing,SyToken **ppBody,SyToken **ppBodyEnd,SyBlob *pSelfFqn,int bNoAutoload)` |
|        5 | 4760 | `{` |
|     7321 | 4761 | `	SyToken *pCur = pGen->pIn; /* on the declaration keyword */` |
|     7321 | 4762 | `	SyToken *pEnd = pGen->pEnd;` |
|        - | 4763 | `	SySet aNames;` |
|     7321 | 4764 | `	sxi32 rc = SXRET_OK;` |
|     7321 | 4765 | `	*ppBody = *ppBodyEnd = 0;` |
|     7321 | 4766 | `	SySetInit(&aNames,&pGen->pVm->sAllocator,sizeof(VmDeferredReq));` |
|     7321 | 4767 | `	pCur++; /* Jump the keyword */` |
|     7321 | 4768 | `	if( bAnon ){` |
|      293 | 4769 | `		if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|      114 | 4770 | `			SyToken *pClose = 0;` |
|      114 | 4771 | `			pCur++;` |
|      114 | 4772 | `			PH7_DelimitNestedTokens(pCur,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|      114 | 4773 | `			if( pClose == 0 \|\| pClose >= pEnd ){` |
|      ! 0 | 4774 | `				SySetRelease(&aNames);` |
|      ! 0 | 4775 | `				return SXERR_INVALID;` |
|        - | 4776 | `			}` |
|      114 | 4777 | `			pCur = &pClose[1];` |
|       55 | 4778 | `		}` |
|      149 | 4779 | `	}else{` |
|     7033 | 4780 | `		if( pCur >= pEnd \|\| !PH7_IsClassNameToken(pCur) ){` |
|        - | 4781 | `			/* Same name test the declaration compiler uses: a word php lets name a` |
|        - | 4782 | ``			 * class may be one of PHL's KEYWORD tokens (`class Integer …`), and`` |
|        - | 4783 | `			 * demanding a plain ID here sent such a declaration down the` |
|        - | 4784 | `			 * non-deferring path, where a not-yet-loaded parent is a fatal. */` |
|      ! 0 | 4785 | `			SySetRelease(&aNames);` |
|      ! 0 | 4786 | `			return SXERR_INVALID;` |
|        - | 4787 | `		}` |
|     7033 | 4788 | `		GenStateBuildFQN(pGen,&pCur->sData,pSelfFqn);` |
|     7033 | 4789 | `		pCur++;` |
|        - | 4790 | `	}` |
|        - | 4791 | ``	/* Header: extends/implements lists up to the '{' (an enum's `: int` backing`` |
|        - | 4792 | `	 * and any stray tokens pass through; malformed headers bail to the normal` |
|        - | 4793 | `	 * path's diagnostics). */` |
|     9539 | 4794 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_OCB) == 0 ){` |
|     2225 | 4795 | `		int iKind = -1;` |
|     2225 | 4796 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|     2151 | 4797 | `			sxi32 nKw = SX_PTR_TO_INT(pCur->pUserData);` |
|     2151 | 4798 | `			if( nKw == PH7_TKWRD_EXTENDS ){` |
|     1553 | 4799 | `				iKind = (iSelfKind == PH7_DEFER_KIND_INTERFACE)` |
|      774 | 4800 | `					? PH7_DEFER_KIND_INTERFACE : PH7_DEFER_KIND_CLASS;` |
|     1377 | 4801 | `			}else if( nKw == PH7_TKWRD_IMPLEMENTS ){` |
|      529 | 4802 | `				iKind = PH7_DEFER_KIND_INTERFACE;` |
|      262 | 4803 | `			}` |
|     1073 | 4804 | `		}` |
|     2225 | 4805 | `		if( iKind < 0 ){` |
|      153 | 4806 | `			pCur++;` |
|      153 | 4807 | `			continue;` |
|        - | 4808 | `		}` |
|     2077 | 4809 | `		pCur++; /* Jump extends/implements */` |
|     1036 | 4810 | `		for(;;){` |
|     2139 | 4811 | `			if( GenStateDeferRecordRef(pGen,&pCur,pEnd,(sxu8)iKind,&aNames) != SXRET_OK ){` |
|        3 | 4812 | `				SySetRelease(&aNames);` |
|        3 | 4813 | `				return SXERR_INVALID;` |
|        - | 4814 | `			}` |
|     2137 | 4815 | `			if( pCur < pEnd && (pCur->nType & PH7_TK_COMMA) ){` |
|       67 | 4816 | `				pCur++;` |
|       67 | 4817 | `				continue;` |
|        - | 4818 | `			}` |
|     2075 | 4819 | `			break;` |
|      ! 0 | 4820 | `		}` |
|        5 | 4821 | `	}` |
|     7319 | 4822 | `	if( pCur >= pEnd \|\| (pCur->nType & PH7_TK_OCB) == 0 ){` |
|      ! 0 | 4823 | `		SySetRelease(&aNames);` |
|      ! 0 | 4824 | `		return SXERR_INVALID;` |
|        - | 4825 | `	}` |
|     7319 | 4826 | `	*ppBody = pCur;` |
|        - | 4827 | `	{` |
|     7319 | 4828 | `		SyToken *pClose = 0;` |
|     7319 | 4829 | `		PH7_DelimitNestedTokens(&pCur[1],pEnd,PH7_TK_OCB,PH7_TK_CCB,&pClose);` |
|     7319 | 4830 | `		if( pClose == 0 \|\| pClose >= pEnd ){` |
|      ! 0 | 4831 | `			SySetRelease(&aNames);` |
|      ! 0 | 4832 | `			return SXERR_INVALID;` |
|        - | 4833 | `		}` |
|     7319 | 4834 | `		*ppBodyEnd = pClose;` |
|        - | 4835 | `	}` |
|        - | 4836 | ``	/* Body: depth-1 trait `use Name[, Name]` statements. Statement position only`` |
|        - | 4837 | ``	 * (previous token one of '{' '}' ';'), so a closure's `use ($x)` — which`` |
|        - | 4838 | `	 * follows a ')' — never matches. */` |
|        - | 4839 | `	{` |
|     7319 | 4840 | `		SyToken *p = &(*ppBody)[1];` |
|     7319 | 4841 | `		int bStmtPos = 1;` |
|     7319 | 4842 | `		sxi32 iDepth = 1;` |
|   158397 | 4843 | `		while( p < *ppBodyEnd ){` |
|   151083 | 4844 | `			if( p->nType & PH7_TK_OCB ){` |
|     6977 | 4845 | `				iDepth++;` |
|     6977 | 4846 | `				bStmtPos = 1;` |
|     6977 | 4847 | `				p++;` |
|     6977 | 4848 | `				continue;` |
|        - | 4849 | `			}` |
|   144111 | 4850 | `			if( p->nType & PH7_TK_CCB ){` |
|     6977 | 4851 | `				iDepth--;` |
|     6977 | 4852 | `				bStmtPos = 1;` |
|     6977 | 4853 | `				p++;` |
|     6977 | 4854 | `				continue;` |
|        - | 4855 | `			}` |
|   137139 | 4856 | `			if( p->nType & PH7_TK_SEMI ){` |
|    11573 | 4857 | `				bStmtPos = 1;` |
|    11573 | 4858 | `				p++;` |
|    11573 | 4859 | `				continue;` |
|        - | 4860 | `			}` |
|   125566 | 4861 | `			if( iDepth == 1 && bStmtPos && (p->nType & PH7_TK_KEYWORD)` |
|    10683 | 4862 | `			 && SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_USE ){` |
|      445 | 4863 | `				p++;` |
|      220 | 4864 | `				for(;;){` |
|      505 | 4865 | `					if( GenStateDeferRecordRef(pGen,&p,*ppBodyEnd,PH7_DEFER_KIND_TRAIT,&aNames) != SXRET_OK ){` |
|      ! 0 | 4866 | `						SySetRelease(&aNames);` |
|      ! 0 | 4867 | `						return SXERR_INVALID;` |
|        - | 4868 | `					}` |
|      505 | 4869 | `					if( p < *ppBodyEnd && (p->nType & PH7_TK_COMMA) ){` |
|       65 | 4870 | `						p++;` |
|       65 | 4871 | `						continue;` |
|        - | 4872 | `					}` |
|      445 | 4873 | `					break;` |
|      ! 0 | 4874 | `				}` |
|      445 | 4875 | `				continue;` |
|        - | 4876 | `			}` |
|   125131 | 4877 | `			bStmtPos = 0;` |
|   125131 | 4878 | `			p++;` |
|        5 | 4879 | `		}` |
|        - | 4880 | `	}` |
|        - | 4881 | `	/* Filter: keep only the names that do NOT resolve. Only an anonymous class,` |
|        - | 4882 | `	 * compiled where it runs, may fire the autoloader here; a named declaration` |
|        - | 4883 | `	 * asks only what is declared already. For a CONDITIONAL one that matters most:` |
|        - | 4884 | `	 * the declaration is deferred whatever this answers, and php never resolves a` |
|        - | 4885 | ``	 * parent it has not reached. `if (false) { class C extends B {} }` is the`` |
|        - | 4886 | `	 * shape that shows it -- nikic/php-parser's own class aliases are written` |
|        - | 4887 | ``	 * that way, with a `require` of the parent's file BEFORE the dead block, so`` |
|        - | 4888 | `	 * an autoload here loaded that file first and the require then declared` |
|        - | 4889 | `	 * everything in it a second time. */` |
|        - | 4890 | `	{` |
|        - | 4891 | `		/* php LINKS a declaration in one order and asks for each dependency as it` |
|        - | 4892 | `		 * links it: the parent, then the traits, then the interfaces -- so` |
|        - | 4893 | ``		 * `class C extends B implements I { use T; }` asks an autoloader for`` |
|        - | 4894 | ``		 * `B, T, I`, with the trait ahead of the interface though the `use` is`` |
|        - | 4895 | ``		 * written inside the body and the `implements` in the header. Reading the`` |
|        - | 4896 | ``		 * collected names in source order asked `B, I, T`, which is user-visible`` |
|        - | 4897 | `		 * the moment an autoloader has a side effect (a log line, a file read, a` |
|        - | 4898 | `		 * map lookup that fails differently) or when one dependency's loader` |
|        - | 4899 | `		 * declares another. The three kinds are walked in link order instead;` |
|        - | 4900 | `` 		 * within a kind the source order stands, and an interface's `extends` `` |
|        - | 4901 | `		 * list is recorded as interfaces, so it keeps its own written order. */` |
|        - | 4902 | `		static const sxu8 aLinkOrder[] = {` |
|        - | 4903 | `			PH7_DEFER_KIND_CLASS, PH7_DEFER_KIND_TRAIT, PH7_DEFER_KIND_INTERFACE` |
|        - | 4904 | `		};` |
|     7319 | 4905 | `		VmDeferredReq *aReq = (VmDeferredReq *)SySetBasePtr(&aNames);` |
|        - | 4906 | `		sxu32 n;` |
|        - | 4907 | `		int iPass;` |
|    29261 | 4908 | `		for( iPass = 0 ; iPass < (int)SX_ARRAYSIZE(aLinkOrder) ; ++iPass ){` |
|    29843 | 4909 | `			for( n = 0 ; n < SySetUsed(&aNames) ; ++n ){` |
|        - | 4910 | `				ph7_class *pFound;` |
|     7901 | 4911 | `				if( aReq[n].cKind != aLinkOrder[iPass] ){` |
|     5269 | 4912 | `					continue;` |
|        - | 4913 | `				}` |
|     2637 | 4914 | `				pFound = bNoAutoload` |
|     2482 | 4915 | `					? GenStateFindDeclaredClass(pGen->pVm,aReq[n].sName.zString,aReq[n].sName.nByte)` |
|     1391 | 4916 | `					: PH7_VmExtractClass(pGen->pVm,aReq[n].sName.zString,aReq[n].sName.nByte,FALSE,0);` |
|     2637 | 4917 | `				if( pFound == 0 ){` |
|      141 | 4918 | `					SySetPut(pMissing,(const void *)&aReq[n]);` |
|       68 | 4919 | `				}` |
|     1321 | 4920 | `			}` |
|    10976 | 4921 | `		}` |
|        - | 4922 | `	}` |
|     7319 | 4923 | `	SySetRelease(&aNames);` |
|     7319 | 4924 | `	return rc;` |
|     3663 | 4925 | `}` |
|        - | 4926 | `/*` |
|        - | 4927 | ` * Capture the declaration as a re-compilable chunk, record it, and emit` |
|        - | 4928 | ` * OP_CLASS_DEFER at the current emission point. On return the statement` |
|        - | 4929 | ` * cursor sits past the declaration's closing '}'. pMissing's entries are` |
|        - | 4930 | ` * COPIED into the record (their name bytes are already allocator-owned).` |
|        - | 4931 | ` */` |
|      216 | 4932 | `static sxi32 GenStateEmitDeferredClass(ph7_gen_state *pGen,sxi32 iFlags,int bAnon,` |
|        - | 4933 | `	SySet *pMissing,SyToken *pBodyEnd,SyBlob *pSelfFqn,const SyString *pAnonName)` |
|        5 | 4934 | `{` |
|      221 | 4935 | `	SyToken *pKw = pGen->pIn; /* the declaration keyword */` |
|        - | 4936 | `	VmDeferredClass *pDefer;` |
|        - | 4937 | `	SyBlob sChunk;` |
|        - | 4938 | `	const char *zFrom;` |
|        - | 4939 | `	const char *zTo;` |
|        - | 4940 | `	char *zDup;` |
|      221 | 4941 | `	SyBlobInit(&sChunk,&pGen->pVm->sAllocator);` |
|        - | 4942 | `	/* The declaration site's compile context is replayed as literal statements:` |
|        - | 4943 | `	 * strict_types first (it must open the chunk), then namespace and the` |
|        - | 4944 | `	 * use-import tables — the runtime re-compile starts in a fresh scope. */` |
|      221 | 4945 | `	if( pGen->bStrictTypes ){` |
|      ! 0 | 4946 | `		SyBlobAppend(&sChunk,"declare(strict_types=1);\n",sizeof("declare(strict_types=1);\n")-1);` |
|      ! 0 | 4947 | `	}` |
|      221 | 4948 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       91 | 4949 | `		SyBlobFormat(&sChunk,"namespace %.*s;\n",` |
|       86 | 4950 | `			(int)SyBlobLength(&pGen->sNamespace),(const char *)SyBlobData(&pGen->sNamespace));` |
|       43 | 4951 | `	}` |
|      221 | 4952 | `	GenStateDeferEmitUses(&sChunk,&pGen->hUseImports,"");` |
|      221 | 4953 | `	GenStateDeferEmitUses(&sChunk,&pGen->hUseFuncImports,"function ");` |
|      221 | 4954 | `	GenStateDeferEmitUses(&sChunk,&pGen->hUseConstImports,"const ");` |
|        - | 4955 | `	/* Doc-comment and attribute groups precede the keyword in the raw source,` |
|        - | 4956 | `	 * outside the captured span — re-emit them from the trivia sidecar. */` |
|      221 | 4957 | `	if( !bAnon && pGen->sPendingDoc.nByte > 0 ){` |
|      ! 0 | 4958 | `		SyBlobAppend(&sChunk,pGen->sPendingDoc.zString,pGen->sPendingDoc.nByte);` |
|      ! 0 | 4959 | `		SyBlobAppend(&sChunk,"\n",1);` |
|      ! 0 | 4960 | `	}` |
|        - | 4961 | `	{` |
|        - | 4962 | `		ph7_trivia *aT;` |
|        - | 4963 | `		sxu32 nT,n;` |
|      221 | 4964 | `		if( bAnon ){` |
|        - | 4965 | ``			/* `new #[A] class` trivia is keyed to the 'class' token */`` |
|       13 | 4966 | `			SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|       13 | 4967 | `			aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|       13 | 4968 | `			nT = SySetUsed(&pGen->aTrivia);` |
|       13 | 4969 | `			if( pGen->pTokenSet && pKw >= pBase && pKw < &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|       13 | 4970 | `				sxu32 nIdx = (sxu32)(pKw - pBase);` |
|       13 | 4971 | `				for( n = 0 ; n < nT ; ++n ){` |
|      ! 0 | 4972 | `					if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|      ! 0 | 4973 | `						SyBlobFormat(&sChunk,"#[%.*s]\n",(int)aT[n].sText.nByte,aT[n].sText.zString);` |
|      ! 0 | 4974 | `					}` |
|      ! 0 | 4975 | `				}` |
|        5 | 4976 | `			}` |
|        8 | 4977 | `		}else{` |
|      211 | 4978 | `			aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|      211 | 4979 | `			nT = SySetUsed(&pGen->aPendingAttrs);` |
|      231 | 4980 | `			for( n = 0 ; n < nT ; ++n ){` |
|       21 | 4981 | `				if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|       21 | 4982 | `					SyBlobFormat(&sChunk,"#[%.*s]\n",(int)aT[n].sText.nByte,aT[n].sText.zString);` |
|       10 | 4983 | `				}` |
|       11 | 4984 | `			}` |
|        - | 4985 | `		}` |
|        - | 4986 | `	}` |
|        - | 4987 | `	/* Pad the prefix with newlines so the declaration keyword sits on its` |
|        - | 4988 | `	 * ORIGINAL line inside the chunk — runtime diagnostics from the deferred` |
|        - | 4989 | `	 * compile then report the source's real line. Best-effort: a prefix` |
|        - | 4990 | `	 * already longer than the declaration line skips the padding. */` |
|        - | 4991 | `	{` |
|      221 | 4992 | `		const char *zScan = (const char *)SyBlobData(&sChunk);` |
|      221 | 4993 | `		sxu32 nHave = 0;` |
|        - | 4994 | `		sxu32 nScan;` |
|     4865 | 4995 | `		for( nScan = 0 ; nScan < SyBlobLength(&sChunk) ; ++nScan ){` |
|     4649 | 4996 | `			if( zScan[nScan] == '\n' ){` |
|      195 | 4997 | `				nHave++;` |
|       95 | 4998 | `			}` |
|     2327 | 4999 | `		}` |
|     2373 | 5000 | `		while( nHave + 1 < pKw->nLine ){` |
|     2157 | 5001 | `			SyBlobAppend(&sChunk,"\n",1);` |
|     2157 | 5002 | `			nHave++;` |
|        5 | 5003 | `		}` |
|        - | 5004 | `	}` |
|      221 | 5005 | `	if( bAnon ){` |
|        - | 5006 | ``		/* `if (false) { new class <header-minus-args> { body } ; }` — installs`` |
|        - | 5007 | `		 * the class at the chunk's compile, never instantiates it. */` |
|       13 | 5008 | `		SyToken *pAfterArgs = &pKw[1];` |
|       13 | 5009 | `		SyBlobAppend(&sChunk,"if (false) { new ",sizeof("if (false) { new ")-1);` |
|       13 | 5010 | `		SyBlobAppend(&sChunk,pKw->sData.zString,pKw->sData.nByte);` |
|       13 | 5011 | `		if( pAfterArgs < pGen->pEnd && (pAfterArgs->nType & PH7_TK_LPAREN) ){` |
|        6 | 5012 | `			SyToken *pClose = 0;` |
|        6 | 5013 | `			PH7_DelimitNestedTokens(&pAfterArgs[1],pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|        6 | 5014 | `			if( pClose == 0 \|\| pClose >= pGen->pEnd ){` |
|      ! 0 | 5015 | `				SyBlobRelease(&sChunk);` |
|      ! 0 | 5016 | `				return SXERR_INVALID;` |
|        - | 5017 | `			}` |
|        6 | 5018 | `			pAfterArgs = &pClose[1];` |
|        2 | 5019 | `		}` |
|       13 | 5020 | `		if( pAfterArgs < pBodyEnd ){` |
|       13 | 5021 | `			SyBlobAppend(&sChunk," ",1);` |
|       13 | 5022 | `			zFrom = pAfterArgs->sData.zString;` |
|       13 | 5023 | `			zTo = pBodyEnd->sData.zString + pBodyEnd->sData.nByte;` |
|       13 | 5024 | `			SyBlobAppend(&sChunk,zFrom,(sxu32)(zTo - zFrom));` |
|        5 | 5025 | `		}` |
|       13 | 5026 | `		SyBlobAppend(&sChunk,"; }",sizeof("; }")-1);` |
|        8 | 5027 | `	}else{` |
|        - | 5028 | `		/* Modifiers were consumed before this compiler ran; reconstruct them` |
|        - | 5029 | ``		 * (an enum's implicit `final` must NOT be spelled out). */`` |
|      206 | 5030 | `		if( (iFlags & PH7_CLASS_ENUM) == 0` |
|      204 | 5031 | `		 && (pKw->nType & PH7_TK_KEYWORD)` |
|      207 | 5032 | `		 && SX_PTR_TO_INT(pKw->pUserData) == PH7_TKWRD_CLASS ){` |
|      175 | 5033 | `			if( iFlags & PH7_CLASS_FINAL ){` |
|        3 | 5034 | `				SyBlobAppend(&sChunk,"final ",sizeof("final ")-1);` |
|        1 | 5035 | `			}` |
|      175 | 5036 | `			if( iFlags & PH7_CLASS_ABSTRACT ){` |
|        3 | 5037 | `				SyBlobAppend(&sChunk,"abstract ",sizeof("abstract ")-1);` |
|        1 | 5038 | `			}` |
|      175 | 5039 | `			if( iFlags & PH7_CLASS_READONLY ){` |
|      ! 0 | 5040 | `				SyBlobAppend(&sChunk,"readonly ",sizeof("readonly ")-1);` |
|      ! 0 | 5041 | `			}` |
|       85 | 5042 | `		}` |
|      211 | 5043 | `		zFrom = pKw->sData.zString;` |
|      211 | 5044 | `		zTo = pBodyEnd->sData.zString + pBodyEnd->sData.nByte;` |
|      211 | 5045 | `		SyBlobAppend(&sChunk,zFrom,(sxu32)(zTo - zFrom));` |
|        - | 5046 | `	}` |
|      221 | 5047 | `	pDefer = (VmDeferredClass *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmDeferredClass));` |
|      221 | 5048 | `	if( pDefer == 0 ){` |
|      ! 0 | 5049 | `		SyBlobRelease(&sChunk);` |
|      ! 0 | 5050 | `		PH7_GenCompileError(pGen,E_ERROR,pKw->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 5051 | `		return SXERR_ABORT;` |
|        - | 5052 | `	}` |
|      221 | 5053 | `	SyZero(pDefer,sizeof(VmDeferredClass));` |
|      329 | 5054 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      216 | 5055 | `		(const char *)SyBlobData(&sChunk),SyBlobLength(&sChunk));` |
|      221 | 5056 | `	SyBlobRelease(&sChunk);` |
|      221 | 5057 | `	if( zDup == 0 ){` |
|      ! 0 | 5058 | `		PH7_GenCompileError(pGen,E_ERROR,pKw->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 5059 | `		return SXERR_ABORT;` |
|        - | 5060 | `	}` |
|      221 | 5061 | `	SyStringInitFromBuf(&pDefer->sText,zDup,SyStrlen(zDup));` |
|      221 | 5062 | `	if( bAnon ){` |
|       13 | 5063 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pAnonName->zString,pAnonName->nByte);` |
|       13 | 5064 | `		if( zDup == 0 ){` |
|      ! 0 | 5065 | `			PH7_GenCompileError(pGen,E_ERROR,pKw->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 5066 | `			return SXERR_ABORT;` |
|        - | 5067 | `		}` |
|       13 | 5068 | `		SyStringInitFromBuf(&pDefer->sAnonName,zDup,pAnonName->nByte);` |
|       13 | 5069 | `		pDefer->sSelfName = pDefer->sAnonName;` |
|        8 | 5070 | `	}else{` |
|      314 | 5071 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      206 | 5072 | `			(const char *)SyBlobData(pSelfFqn),SyBlobLength(pSelfFqn));` |
|      211 | 5073 | `		if( zDup == 0 ){` |
|      ! 0 | 5074 | `			PH7_GenCompileError(pGen,E_ERROR,pKw->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 5075 | `			return SXERR_ABORT;` |
|        - | 5076 | `		}` |
|      211 | 5077 | `		SyStringInitFromBuf(&pDefer->sSelfName,zDup,SyBlobLength(pSelfFqn));` |
|        - | 5078 | `	}` |
|      221 | 5079 | `	SySetInit(&pDefer->aRequired,&pGen->pVm->sAllocator,sizeof(VmDeferredReq));` |
|        - | 5080 | `	{` |
|      221 | 5081 | `		VmDeferredReq *aReq = (VmDeferredReq *)SySetBasePtr(pMissing);` |
|        - | 5082 | `		sxu32 n;` |
|      355 | 5083 | `		for( n = 0 ; n < SySetUsed(pMissing) ; ++n ){` |
|      139 | 5084 | `			SySetPut(&pDefer->aRequired,(const void *)&aReq[n]);` |
|       72 | 5085 | `		}` |
|        - | 5086 | `	}` |
|      221 | 5087 | `	pDefer->nLine = pKw->nLine;` |
|      221 | 5088 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLASS_DEFER,0,0,(void *)pDefer,0);` |
|        - | 5089 | `	/* Skip the declaration: the statement cursor lands past its '}' */` |
|      221 | 5090 | `	pGen->pIn = &pBodyEnd[1];` |
|      221 | 5091 | `	return SXRET_OK;` |
|      113 | 5092 | `}` |
|        - | 5093 | `/*` |
|        - | 5094 | ` * Deferral gate shared by the named-declaration compilers: scan the` |
|        - | 5095 | ` * declaration at pGen->pIn; when a dependency is missing, capture + emit the` |
|        - | 5096 | ` * deferred record and return TRUE (the caller returns immediately — the` |
|        - | 5097 | ` * declaration compiles at execution time). FALSE means compile normally.` |
|        - | 5098 | ` * *pRc carries SXERR_ABORT out of the capture path.` |
|        - | 5099 | ` */` |
|        - | 5100 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|        - | 5101 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd);` |
|        - | 5102 | `/*` |
|        - | 5103 | ` * php compiles a class body where the FILE compiles, conditional or not, and only` |
|        - | 5104 | ` * declares it when the statement runs. So everything its compiler refuses -- a` |
|        - | 5105 | `` * syntax error in a method, `parent::` in a baseless class, a method declared`` |
|        - | 5106 | ` * twice, an abstract method in a concrete class -- stops the whole file before` |
|        - | 5107 | `` * a line of it runs, even inside `if (false)`. A deferred declaration is not`` |
|        - | 5108 | ` * compiled until its statement runs here, so compile it once now for those` |
|        - | 5109 | ` * refusals alone (bDeclCheck): nothing it names is linked, nothing is installed,` |
|        - | 5110 | ` * and what it emitted is discarded. pKw is the declaration keyword.` |
|        - | 5111 | ` */` |
|      206 | 5112 | `static sxi32 GenStateCheckDeferredDecl(ph7_gen_state *pGen,sxi32 iFlags,int iSelfKind,` |
|        - | 5113 | `	SyToken *pKw,VmDeferredClass *pDefer)` |
|        5 | 5114 | `{` |
|      211 | 5115 | `	SyToken *pSavedIn = pGen->pIn;` |
|      211 | 5116 | `	SyToken *pSavedEnd = pGen->pEnd;` |
|      211 | 5117 | `	sxu32 nInstr = PH7_VmInstrLength(pGen->pVm);` |
|      211 | 5118 | `	sxu32 nAnonSeq = pGen->pVm->nAnonSeq;` |
|        - | 5119 | `	sxi32 rc;` |
|      211 | 5120 | `	pGen->bDeclCheck = 1;` |
|      211 | 5121 | `	pGen->pIn = pKw;` |
|      211 | 5122 | `	if( iSelfKind == PH7_DEFER_KIND_INTERFACE ){` |
|       19 | 5123 | `		rc = PH7_CompileClassInterface(pGen);` |
|      203 | 5124 | `	}else if( iSelfKind == PH7_DEFER_KIND_TRAIT ){` |
|       20 | 5125 | `		rc = PH7_CompileTrait(pGen);` |
|       12 | 5126 | `	}else{` |
|      179 | 5127 | `		rc = GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|        - | 5128 | `	}` |
|      211 | 5129 | `	pGen->bDeclCheck = 0;` |
|      211 | 5130 | `	SySetTruncate(pGen->pVm->pByteContainer,nInstr);` |
|        - | 5131 | `	/* The real compile names its anonymous classes; this one minted none. */` |
|      211 | 5132 | `	pGen->pVm->nAnonSeq = nAnonSeq;` |
|      211 | 5133 | `	pGen->pIn = pSavedIn;` |
|      211 | 5134 | `	pGen->pEnd = pSavedEnd;` |
|      211 | 5135 | `	pDefer->bChecked = 1;` |
|      211 | 5136 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        5 | 5137 | `}` |
|     7324 | 5138 | `static int GenStateMaybeDeferClass(ph7_gen_state *pGen,sxi32 iFlags,int iSelfKind,sxi32 *pRc)` |
|        5 | 5139 | `{` |
|        - | 5140 | `	SySet aMissing;` |
|     7329 | 5141 | `	SyToken *pBody = 0;` |
|     7329 | 5142 | `	SyToken *pBodyEnd = 0;` |
|        - | 5143 | `	SyBlob sSelfFqn;` |
|     7329 | 5144 | `	int bDefer = 0;` |
|        - | 5145 | `	/* php's binding rule, decided before anything is resolved: a declaration that` |
|        - | 5146 | `	 * is not at a unit's top level is bound when execution REACHES it, so it is` |
|        - | 5147 | `	 * deferred whatever its dependencies look like -- and nothing about it may be` |
|        - | 5148 | `	 * resolved here. */` |
|     7329 | 5149 | `	int bCond = GenStateDeclIsConditional(&(*pGen));` |
|     7329 | 5150 | `	*pRc = SXRET_OK;` |
|     7329 | 5151 | `	if( pGen->bDeclCheck ){` |
|      211 | 5152 | `		return 0; /* the check compile of a declaration already deferred */` |
|        - | 5153 | `	}` |
|     7123 | 5154 | `	if( pGen->pVm->bSyntaxCheck ){` |
|        - | 5155 | ``		/* `phl -l`: the declaration is never EXECUTED, so there is nothing to`` |
|        - | 5156 | `		 * defer it to -- and deferring captures the body as raw text that no one` |
|        - | 5157 | ``		 * ever parses, which is how a file with `$x = ;` inside a class extending`` |
|        - | 5158 | `		 * an autoloaded base linted CLEAN. Nothing autoloads under -l either, so` |
|        - | 5159 | `		 * this is the common case rather than the rare one. php's own lint parses` |
|        - | 5160 | `		 * every body and binds nothing. */` |
|       92 | 5161 | `		return 0;` |
|        - | 5162 | `	}` |
|     7033 | 5163 | `	SySetInit(&aMissing,&pGen->pVm->sAllocator,sizeof(VmDeferredReq));` |
|     7033 | 5164 | `	SyBlobInit(&sSelfFqn,&pGen->pVm->sAllocator);` |
|        - | 5165 | `	/* A named declaration never asks the autoloader while it COMPILES, conditional` |
|        - | 5166 | `	 * or not: php's early binding takes a parent only if it is already declared,` |
|        - | 5167 | `	 * and leaves anything else to be bound when the statement runs. Asking here` |
|        - | 5168 | `	 * ran the autoloader ahead of the statements written above the declaration,` |
|        - | 5169 | `	 * for a dependency declared further down the same file, and for a unit` |
|        - | 5170 | `	 * that then failed to parse and so never ran at all. */` |
|     7028 | 5171 | `	if( GenStateScanDeferDeps(pGen,0,iSelfKind,&aMissing,&pBody,&pBodyEnd,&sSelfFqn,1) == SXRET_OK` |
|     7032 | 5172 | `	 && (SySetUsed(&aMissing) > 0 \|\| bCond) ){` |
|        - | 5173 | `		/* Two reasons to compile this declaration where it RUNS rather than here.` |
|        - | 5174 | `		 * The first is a missing dependency (the autoloader that resolves it has` |
|        - | 5175 | `		 * not been registered yet). The second is php's binding rule: a class` |
|        - | 5176 | ``		 * written inside an `if`, a loop or a function body is declared when`` |
|        - | 5177 | ``		 * execution reaches it, so `if (!class_exists('DateTime')) { class`` |
|        - | 5178 | ``		 * DateTime {} }` -- how symfony/polyfill-php8x ships its back-ports -- must`` |
|        - | 5179 | `		 * not REPLACE the engine's own class in a tree that has one. */` |
|      211 | 5180 | `		SyToken *pKw = pGen->pIn;` |
|      211 | 5181 | `		*pRc = GenStateEmitDeferredClass(pGen,iFlags,0,&aMissing,pBodyEnd,&sSelfFqn,0);` |
|      211 | 5182 | `		if( *pRc == SXRET_OK ){` |
|      211 | 5183 | `			VmInstr *pDeferInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      314 | 5184 | `			*pRc = GenStateCheckDeferredDecl(pGen,iFlags,iSelfKind,pKw,` |
|      206 | 5185 | `				(VmDeferredClass *)pDeferInstr->p3);` |
|      103 | 5186 | `		}` |
|      211 | 5187 | `		bDefer = 1;` |
|      103 | 5188 | `	}` |
|     7033 | 5189 | `	SySetRelease(&aMissing);` |
|     7033 | 5190 | `	SyBlobRelease(&sSelfFqn);` |
|     7033 | 5191 | `	return bDefer;` |
|     3667 | 5192 | `}` |
|        - | 5193 | `/*` |
|        - | 5194 | ``  * Apply a declaration body's collected `use Trait[, Trait] [{ resolution }]` `` |
|        - | 5195 | ` * entries to pClass — plain application when no resolution block is present,` |
|        - | 5196 | ` * otherwise the two-pass insteadof/as machinery. Shared by the CLASS body and` |
|        - | 5197 | ` * (since the adaptation-block port) the TRAIT body compiler. Returns the last` |
|        - | 5198 | ` * application status (non-OK = out of memory at a copy site).` |
|        - | 5199 | ` */` |
|     6784 | 5200 | `static sxi32 GenStateApplyTraitUses(ph7_gen_state *pGen,ph7_class *pClass,SySet *pUseEntries)` |
|        5 | 5201 | `{` |
|     6789 | 5202 | `	sxi32 rc = SXRET_OK;` |
|        - | 5203 | `	{` |
|        - | 5204 | `		TraitUseEntry *apUse;` |
|        - | 5205 | `		sxu32 nU;` |
|     6789 | 5206 | `		apUse = (TraitUseEntry *)SySetBasePtr(pUseEntries);` |
|     7233 | 5207 | `		for( nU = 0 ; nU < SySetUsed(pUseEntries) ; nU++ ){` |
|      449 | 5208 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      449 | 5209 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      449 | 5210 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      449 | 5211 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|        - | 5212 | `			sxu32 nT;` |
|      449 | 5213 | `			if( !hasResolution ){` |
|        - | 5214 | `				/* No conflict resolution block: use standard trait application */` |
|      805 | 5215 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      405 | 5216 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      405 | 5217 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 5218 | `						break;` |
|        - | 5219 | `					}` |
|      205 | 5220 | `				}` |
|      205 | 5221 | `			}else{` |
|        - | 5222 | `				/* With resolution block: copy attributes, record traits,` |
|        - | 5223 | `				 * then use the block to resolve method conflicts.` |
|        - | 5224 | `				 */` |
|        - | 5225 | `				SyToken *pR;` |
|      104 | 5226 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       60 | 5227 | `					ph7_class *pTR = apTrait[nT];` |
|        - | 5228 | `					ph7_class_attr *pAR;` |
|        - | 5229 | `					SyHashEntry *pER;` |
|        - | 5230 | `					SyString *pNR;` |
|       60 | 5231 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|       92 | 5232 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|        5 | 5233 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|        5 | 5234 | `						pNR = &pAR->sName;` |
|        5 | 5235 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|        5 | 5236 | `							SyHashInsertTail(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|        5 | 5237 | `							PH7_ClassNotePrivateName(pClass,pAR);` |
|        2 | 5238 | `						}` |
|        1 | 5239 | `					}` |
|        - | 5240 | `					/* Trait constants (PHP 8.2) live in the separate hConst namespace */` |
|       60 | 5241 | `					SyHashResetLoopCursor(&pTR->hConst);` |
|       88 | 5242 | `					while((pER = SyHashGetNextEntry(&pTR->hConst)) != 0 ){` |
|      ! 0 | 5243 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|      ! 0 | 5244 | `						pNR = &pAR->sName;` |
|      ! 0 | 5245 | `						if( SyHashGet(&pClass->hConst,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      ! 0 | 5246 | `							SyHashInsertTail(&pClass->hConst,(const void *)pNR->zString,pNR->nByte,pAR);` |
|      ! 0 | 5247 | `						}` |
|      ! 0 | 5248 | `					}` |
|       60 | 5249 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       32 | 5250 | `				}` |
|        - | 5251 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       48 | 5252 | `				pR = pUse->pResolvStart;` |
|      120 | 5253 | `				while( pR < pUse->pResolvEnd ){` |
|        - | 5254 | `					SyString sTrait,sMethod;` |
|        - | 5255 | `					ph7_class *pSrcTrait;` |
|        - | 5256 | `					ph7_class_method *pMeth;` |
|        - | 5257 | `					sxi32 nRKwrd;` |
|      192 | 5258 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      120 | 5259 | `					if( pR >= pUse->pResolvEnd ) break;` |
|       76 | 5260 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|       76 | 5261 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|       76 | 5262 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|       76 | 5263 | `					sMethod = pR->sData;` |
|       76 | 5264 | `					pR++;` |
|       76 | 5265 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       31 | 5266 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       31 | 5267 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       31 | 5268 | `							sTrait = sMethod;` |
|       31 | 5269 | `							pR++;` |
|       31 | 5270 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       31 | 5271 | `							sMethod = pR->sData;` |
|       31 | 5272 | `							pR++;` |
|       14 | 5273 | `						}` |
|       14 | 5274 | `					}` |
|       76 | 5275 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 5276 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|      ! 0 | 5277 | `						continue;` |
|        - | 5278 | `					}` |
|       76 | 5279 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|       76 | 5280 | `					pR++;` |
|       76 | 5281 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       17 | 5282 | `						pSrcTrait = 0;` |
|       21 | 5283 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       21 | 5284 | `							SyString *pTN = &apTrait[nT]->sName;` |
|       30 | 5285 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       18 | 5286 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       17 | 5287 | `								pSrcTrait = apTrait[nT];` |
|       17 | 5288 | `								break;` |
|        - | 5289 | `							}` |
|        4 | 5290 | `						}` |
|       17 | 5291 | `						if( pSrcTrait ){` |
|       17 | 5292 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       17 | 5293 | `							if( pMeth ){` |
|       17 | 5294 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       17 | 5295 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       17 | 5296 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|        7 | 5297 | `								}` |
|        7 | 5298 | `							}` |
|        7 | 5299 | `						}` |
|        7 | 5300 | `					}` |
|      168 | 5301 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|        4 | 5302 | `				}` |
|        - | 5303 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      104 | 5304 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        - | 5305 | `					ph7_class_method *pMR;` |
|        - | 5306 | `					SyHashEntry *pER;` |
|        - | 5307 | `					SyString *pNR;` |
|       60 | 5308 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      186 | 5309 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      102 | 5310 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      102 | 5311 | `						pNR = &pMR->sFunc.sName;` |
|      102 | 5312 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|       74 | 5313 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       35 | 5314 | `						}` |
|        4 | 5315 | `					}` |
|       32 | 5316 | `				}` |
|        - | 5317 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       48 | 5318 | `				pR = pUse->pResolvStart;` |
|      120 | 5319 | `				while( pR < pUse->pResolvEnd ){` |
|        - | 5320 | `					SyString sTrait,sMethod,sAlias;` |
|        - | 5321 | `					ph7_class *pSrcTrait;` |
|        - | 5322 | `					ph7_class_method *pMeth;` |
|      120 | 5323 | `					int hasQual = 0;` |
|        - | 5324 | `					sxi32 nRKwrd;` |
|      192 | 5325 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      120 | 5326 | `					if( pR >= pUse->pResolvEnd ) break;` |
|       76 | 5327 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|       76 | 5328 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|       76 | 5329 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|       76 | 5330 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|       76 | 5331 | `					sMethod = pR->sData;` |
|       76 | 5332 | `					pR++;` |
|       76 | 5333 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       31 | 5334 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       31 | 5335 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       31 | 5336 | `							sTrait = sMethod;` |
|       31 | 5337 | `							hasQual = 1;` |
|       31 | 5338 | `							pR++;` |
|       31 | 5339 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       31 | 5340 | `							sMethod = pR->sData;` |
|       31 | 5341 | `							pR++;` |
|       14 | 5342 | `						}` |
|       14 | 5343 | `					}` |
|       76 | 5344 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 5345 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|      ! 0 | 5346 | `						continue;` |
|        - | 5347 | `					}` |
|       76 | 5348 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|       76 | 5349 | `					pR++;` |
|       76 | 5350 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       62 | 5351 | `						sxi32 iNewVis = -1;` |
|       62 | 5352 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       25 | 5353 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       25 | 5354 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       25 | 5355 | `								iNewVis = nAK;` |
|       25 | 5356 | `								pR++;` |
|       11 | 5357 | `							}` |
|       11 | 5358 | `						}` |
|       62 | 5359 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       60 | 5360 | `							sAlias = pR->sData;` |
|       60 | 5361 | `							pR++;` |
|       28 | 5362 | `						}` |
|       62 | 5363 | `						pMeth = 0;` |
|       62 | 5364 | `						if( hasQual ){` |
|       17 | 5365 | `							pSrcTrait = 0;` |
|       27 | 5366 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       27 | 5367 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       39 | 5368 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       24 | 5369 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       17 | 5370 | `									pSrcTrait = apTrait[nT];` |
|       17 | 5371 | `									break;` |
|        - | 5372 | `								}` |
|        8 | 5373 | `							}` |
|       17 | 5374 | `							if( pSrcTrait ){` |
|       17 | 5375 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|        7 | 5376 | `							}` |
|       10 | 5377 | `						}else{` |
|       48 | 5378 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|        - | 5379 | `						}` |
|       62 | 5380 | `						if( pMeth ){` |
|        - | 5381 | `							/* php: a method declared in the class BODY wins over a trait alias` |
|        - | 5382 | ``							 * of the same name (e.g. an explicit __construct over `init as`` |
|        - | 5383 | ``							 * __construct`). If pClass already declares sAlias ITSELF — an own`` |
|        - | 5384 | `							 * method, sFunc.pUserData == pClass — keep it: SyHashInsert is LIFO,` |
|        - | 5385 | `							 * so an unconditional insert would shadow the class method at lookup` |
|        - | 5386 | ``							 * and `new` would run the alias. A name held only by another trait is`` |
|        - | 5387 | `							 * a genuine conflict resolved by the insteadof pass above. */` |
|       62 | 5388 | `							int bClassWins = 0;` |
|       62 | 5389 | `							if( sAlias.nByte > 0 ){` |
|       60 | 5390 | `								ph7_class_method *pOwn = PH7_ClassExtractMethod(pClass,sAlias.zString,sAlias.nByte);` |
|       60 | 5391 | `								bClassWins = (pOwn && pOwn->sFunc.pUserData == pClass);` |
|       28 | 5392 | `							}` |
|       87 | 5393 | `							if( sAlias.nByte > 0 && !bClassWins ){` |
|        - | 5394 | `								/* Create a shallow copy of the method struct for the alias` |
|        - | 5395 | `								 * so it can carry its own visibility without affecting the original.` |
|        - | 5396 | `								 */` |
|        - | 5397 | `								ph7_class_method *pAlias;` |
|        - | 5398 | `								char *zAliasDup;` |
|       54 | 5399 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       54 | 5400 | `								if( pAlias ){` |
|       54 | 5401 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       54 | 5402 | `									if( iNewVis >= 0 ){` |
|       23 | 5403 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       19 | 5404 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|        7 | 5405 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       10 | 5406 | `									}` |
|       54 | 5407 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       54 | 5408 | `									if( zAliasDup ){` |
|       54 | 5409 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       25 | 5410 | `									}` |
|       29 | 5411 | `								}` |
|       35 | 5412 | `							}else if( sAlias.nByte == 0 && iNewVis >= 0 ){` |
|        - | 5413 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|        - | 5414 | `								ph7_class_method *pCopy;` |
|        3 | 5415 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|        3 | 5416 | `								if( pCopy ){` |
|        3 | 5417 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|        3 | 5418 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|        3 | 5419 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|        3 | 5420 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|      ! 0 | 5421 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|        - | 5422 | `									/* Replace the method in the class hash */` |
|        3 | 5423 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|        3 | 5424 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|        1 | 5425 | `								}` |
|        1 | 5426 | `							}` |
|       29 | 5427 | `						}` |
|       29 | 5428 | `						SXUNUSED(hasQual);` |
|       29 | 5429 | `					}` |
|       90 | 5430 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|        4 | 5431 | `				}` |
|        - | 5432 | `			}` |
|      449 | 5433 | `			SySetRelease(&pUse->aTraits);` |
|      227 | 5434 | `		}` |
|        - | 5435 | `	}` |
|     6789 | 5436 | `	return rc;` |
|        5 | 5437 | `}` |
|        - | 5438 | `/*` |
|        - | 5439 | ` * Compile a class declaration, named or anonymous.` |
|        - | 5440 | ` *` |
|        - | 5441 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|        - | 5442 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|        - | 5443 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|        - | 5444 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|        - | 5445 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|        - | 5446 | ` * implements, body, install) is shared by both paths.` |
|        - | 5447 | ` */` |
|        - | 5448 | `/*` |
|        - | 5449 | ` * php's virtual-default refusal for a class with a parent, asked once the` |
|        - | 5450 | ` * class has linked: a hooked property whose own bodies never touch` |
|        - | 5451 | `` * `$this->NAME` is virtual only if the property it redeclares is virtual too`` |
|        - | 5452 | ` * (PH7_ClassInherit cleared the flag over a backed one), so a default is a` |
|        - | 5453 | ` * refusal only now -- blamed on the class's own line, as php's link blames it.` |
|        - | 5454 | ` */` |
|     1414 | 5455 | `static sxi32 GenStateCheckVirtualDefaults(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 | 5456 | `{` |
|        - | 5457 | `	SyHashEntry *pEntry;` |
|     1419 | 5458 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     6111 | 5459 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|     4707 | 5460 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     4702 | 5461 | `		if( pAttr->pDeclClass == pClass` |
|     2560 | 5462 | `		 && (pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL)` |
|      229 | 5463 | `		 && SySetUsed(&pAttr->aByteCode) > 0 ){` |
|       17 | 5464 | `			sxi32 rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,` |
|        - | 5465 | `				"Cannot specify default value for virtual hooked property %z::$%z",` |
|        5 | 5466 | `				&pClass->sDisp,&pAttr->sName);` |
|       12 | 5467 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_CORRUPT;` |
|        - | 5468 | `		}` |
|        5 | 5469 | `	}` |
|     1409 | 5470 | `	return SXRET_OK;` |
|      712 | 5471 | `}` |
|     6696 | 5472 | `static sxi32 GenStateCompileClassBody(ph7_gen_state *pGen,sxi32 iFlags,` |
|        - | 5473 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|        5 | 5474 | `{` |
|     6701 | 5475 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 5476 | `	ph7_class *pClass,*pBase;` |
|     6701 | 5477 | `	ph7_class *pSavedCurClass = pGen->pCurClass; /* restored at 'done' (enclosing class for a nested anon) */` |
|     6701 | 5478 | `	GenBlock *pSavedCurClassBlock = pGen->pCurClassBlock;` |
|     6701 | 5479 | `	ph7_class *pSavedCurBase = pGen->pCurBase;   /* ...and its base, see pCurBase */` |
|        - | 5480 | `	SyToken *pEnd,*pTmp;` |
|        - | 5481 | `	SySet aInterfaces;` |
|        - | 5482 | `	SySet aUseEntries;` |
|        - | 5483 | `	SySet aOvMeth,aOvProp;   /* the #[\Override] claims this class DECLARED */` |
|     6701 | 5484 | `	sxu32 nErrEntry = pGen->nErr; /* errors already reported when this class started */` |
|        - | 5485 | `	SyString *pName;` |
|        - | 5486 | `	sxi32 nKwrd;` |
|        - | 5487 | `	sxi32 rc;` |
|     6701 | 5488 | `	if( pAnonName == 0 ){` |
|        - | 5489 | `		/* Deferral gate: an unresolvable parent/interface/trait —` |
|        - | 5490 | `		 * its autoloader has not RUN yet — re-compiles this declaration at its` |
|        - | 5491 | `		 * execution point instead of dying on "Nonexistent base class". */` |
|        - | 5492 | `		sxi32 rcDefer;` |
|     6423 | 5493 | `		if( GenStateMaybeDeferClass(pGen,iFlags,PH7_DEFER_KIND_CLASS,&rcDefer) ){` |
|      179 | 5494 | `			return rcDefer == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 5495 | `		}` |
|     3122 | 5496 | `	}` |
|        - | 5497 | `	/* Jump the 'class' keyword */` |
|     6527 | 5498 | `	pGen->pIn++;` |
|     6527 | 5499 | `	if( pAnonName ){` |
|        - | 5500 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|        - | 5501 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|        - | 5502 | `		 * then use the synthesized name. */` |
|      283 | 5503 | `		*ppArgStart = *ppArgEnd = 0;` |
|      283 | 5504 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|      110 | 5505 | `			pGen->pIn++; /* Jump '(' */` |
|      110 | 5506 | `			*ppArgStart = pGen->pIn;` |
|      163 | 5507 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|       53 | 5508 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|      110 | 5509 | `			pGen->pIn = *ppArgEnd;` |
|      110 | 5510 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|       53 | 5511 | `		}` |
|      283 | 5512 | `		pName = pAnonName;` |
|      283 | 5513 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|      144 | 5514 | `	}else{` |
|     6249 | 5515 | `		if( pGen->pIn >= pGen->pEnd \|\| !PH7_IsClassNameToken(pGen->pIn) ){` |
|        - | 5516 | `			/* Syntax error */` |
|      ! 0 | 5517 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|      ! 0 | 5518 | `			if( rc == SXERR_ABORT ){` |
|        - | 5519 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 5520 | `				return SXERR_ABORT;` |
|        - | 5521 | `			}` |
|        - | 5522 | `			/* Synchronize with the first semi-colon or curly braces */` |
|      ! 0 | 5523 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|      ! 0 | 5524 | `				pGen->pIn++;` |
|      ! 0 | 5525 | `			}` |
|      ! 0 | 5526 | `			return SXRET_OK;` |
|        - | 5527 | `		}` |
|        - | 5528 | `		/* Extract class name */` |
|     6249 | 5529 | `		pName = &pGen->pIn->sData;` |
|     6249 | 5530 | `		if( PH7_IsReservedClassName(pName) ){` |
|        - | 5531 | `			/* php's compiler-side screen (zend_is_reserved_class_name): the scanner` |
|        - | 5532 | `			 * hands these over as identifiers and the compiler refuses them, quoting` |
|        - | 5533 | ``			 * the name as WRITTEN. `class Null {}` and `class void {}` used to`` |
|        - | 5534 | `			 * compile here and are fatals in php. */` |
|        4 | 5535 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        1 | 5536 | `				"Cannot use \"%z\" as a class name as it is reserved",pName);` |
|        3 | 5537 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 5538 | `				return SXERR_ABORT;` |
|        - | 5539 | `			}` |
|        4 | 5540 | `			while( pGen->pIn < pGen->pEnd` |
|        5 | 5541 | `			    && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|        3 | 5542 | `				pGen->pIn++;` |
|        1 | 5543 | `			}` |
|        3 | 5544 | `			return SXRET_OK;` |
|        - | 5545 | `		}` |
|        - | 5546 | `		/* Advance the stream cursor */` |
|     6247 | 5547 | `		pGen->pIn++;` |
|        - | 5548 | `		/* Build FQN and obtain a raw class */ {` |
|        - | 5549 | `			SyBlob sFQN;` |
|        - | 5550 | `			SyString sFQNStr;` |
|     6247 | 5551 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     6247 | 5552 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|     6247 | 5553 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|        - | 5554 | ``			/* php refuses a declaration whose short name a local `use` already took. */`` |
|     6247 | 5555 | `			if( GenStateGuardImportRedeclare(pGen,0,pName,&sFQNStr,nLine) == SXERR_ABORT ){` |
|      ! 0 | 5556 | `				SyBlobRelease(&sFQN);` |
|      ! 0 | 5557 | `				return SXERR_ABORT;` |
|        - | 5558 | `			}` |
|     6247 | 5559 | `			GenStateRecordDeclaredName(pGen,0,&sFQNStr);` |
|     6247 | 5560 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|     6247 | 5561 | `			SyBlobRelease(&sFQN);` |
|        - | 5562 | `		}` |
|        - | 5563 | `	}` |
|     6525 | 5564 | `	if( pClass == 0 ){` |
|      ! 0 | 5565 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 5566 | `		return SXERR_ABORT;` |
|        - | 5567 | `	}` |
|     6520 | 5568 | `	if( (iFlags & PH7_CLASS_ENUM) && pGen->pIn < pGen->pEnd` |
|      157 | 5569 | `		&& (pGen->pIn->nType & PH7_TK_COLON /* ':' */) ){` |
|        - | 5570 | ``		/* Backed enum: `enum Name: int\|string` (PHP 8.1) */`` |
|       79 | 5571 | `		pGen->pIn++; /* Jump ':' */` |
|       74 | 5572 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       79 | 5573 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_INT ){` |
|       31 | 5574 | `			pClass->nEnumBacking = MEMOBJ_INT;` |
|       31 | 5575 | `			pGen->pIn++;` |
|       63 | 5576 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       51 | 5577 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STRING ){` |
|       49 | 5578 | `			pClass->nEnumBacking = MEMOBJ_STRING;` |
|       49 | 5579 | `			pGen->pIn++;` |
|       27 | 5580 | `		}else{` |
|        3 | 5581 | `			SyToken *pTok = pGen->pIn;` |
|        3 | 5582 | `			if( pTok >= pGen->pEnd ){ pTok--; }` |
|        4 | 5583 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pTok->nLine,` |
|        1 | 5584 | `				"Enum backing type must be int or string, %z given",&pTok->sData);` |
|        3 | 5585 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 5586 | `				return SXERR_ABORT;` |
|        - | 5587 | `			}` |
|        3 | 5588 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|        3 | 5589 | `				pGen->pIn++; /* Skip the bogus type token */` |
|        1 | 5590 | `			}` |
|        - | 5591 | `		}` |
|       37 | 5592 | `	}` |
|     6525 | 5593 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|     6525 | 5594 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 5595 | `		return SXERR_ABORT;` |
|        - | 5596 | `	}` |
|        - | 5597 | `	/* implemented interfaces and per-use-statement trait containers */` |
|     6525 | 5598 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|     6525 | 5599 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|        - | 5600 | `	/* Assume a standalone class */` |
|     6525 | 5601 | `	pBase = 0;` |
|     6525 | 5602 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     1947 | 5603 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     1947 | 5604 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|        - | 5605 | `			SyBlob sResolved;` |
|        - | 5606 | `			SyString sBaseName;` |
|        - | 5607 | `			sxu32 nRefLine;` |
|     1489 | 5608 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|        - | 5609 | `				/* php parse-fatals here (enums have no inheritance) */` |
|      ! 0 | 5610 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 5611 | `					"Enum %z cannot extend a class",&pClass->sDisp);` |
|      ! 0 | 5612 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 5613 | `					return SXERR_ABORT;` |
|        - | 5614 | `				}` |
|      ! 0 | 5615 | `			}` |
|     1489 | 5616 | `			pGen->pIn++; /* Advance past 'extends' */` |
|     1489 | 5617 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|     1489 | 5618 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     1489 | 5619 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|        3 | 5620 | `				SyBlobRelease(&sResolved);` |
|        4 | 5621 | `				rc = PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        - | 5622 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|        1 | 5623 | `					pName);` |
|        3 | 5624 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|        3 | 5625 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 5626 | `					return SXERR_ABORT;` |
|        - | 5627 | `				}` |
|        3 | 5628 | `				return SXRET_OK;` |
|        - | 5629 | `			}` |
|     1487 | 5630 | `			pBase = GenStateLinkClass(pGen,&sResolved);` |
|     1487 | 5631 | `			SyStringInitFromBuf(&sBaseName,` |
|        - | 5632 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - | 5633 | `			/* Interfaces are not allowed */` |
|     1487 | 5634 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|      ! 0 | 5635 | `				pBase = pBase->pNextName;` |
|      ! 0 | 5636 | `			}` |
|     1487 | 5637 | `			if( pBase == 0 ){` |
|       69 | 5638 | `				if( !GenStateLintSkipsBase(pGen,pClass) ){` |
|      ! 0 | 5639 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - | 5640 | `						"Nonexistent base class '%z'",&sBaseName);` |
|      ! 0 | 5641 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 5642 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 5643 | `						return SXERR_ABORT;` |
|        - | 5644 | `					}` |
|      ! 0 | 5645 | `				}` |
|       37 | 5646 | `			}else{` |
|     1423 | 5647 | `				if( pBase->iFlags & PH7_CLASS_ENUM ){` |
|        4 | 5648 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 5649 | `						"Class %z cannot extend enum %z",pName,&pBase->sDisp);` |
|        3 | 5650 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 5651 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 5652 | `						return SXERR_ABORT;` |
|        - | 5653 | `					}` |
|        3 | 5654 | `					pBase = 0; /* Never inherit from an enum */` |
|     1422 | 5655 | `				}else if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|        8 | 5656 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 5657 | `						/* php's wording, unquoted: "Class B cannot extend final class A". */` |
|        2 | 5658 | `						"Class %z cannot extend final class %z",pName,&pBase->sDisp);` |
|        6 | 5659 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 5660 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 5661 | `						return SXERR_ABORT;` |
|        - | 5662 | `					}` |
|        2 | 5663 | `				}` |
|        - | 5664 | `			}` |
|     1487 | 5665 | `			SyBlobRelease(&sResolved);` |
|     1487 | 5666 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|      ! 0 | 5667 | `				pBase = 0; /* Error already reported: enums have no base class */` |
|      ! 0 | 5668 | `			}` |
|      741 | 5669 | `		}` |
|     1945 | 5670 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|        - | 5671 | `			ph7_class *pInterface;` |
|        - | 5672 | `			/* Interface implementation */` |
|      533 | 5673 | `			pGen->pIn++; /* Advance the stream cursor */` |
|      306 | 5674 | `			for(;;){` |
|        - | 5675 | `				SyBlob sResolved;` |
|        - | 5676 | `				SyString sIntName;` |
|        - | 5677 | `				sxu32 nRefLine;` |
|      575 | 5678 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|      575 | 5679 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      575 | 5680 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 5681 | `					SyBlobRelease(&sResolved);` |
|      ! 0 | 5682 | `					rc = PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        - | 5683 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|      ! 0 | 5684 | `						pName);` |
|      ! 0 | 5685 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 5686 | `						return SXERR_ABORT;` |
|        - | 5687 | `					}` |
|      ! 0 | 5688 | `					break;` |
|        - | 5689 | `				}` |
|      575 | 5690 | `				pInterface = GenStateLinkClass(pGen,&sResolved);` |
|      575 | 5691 | `				SyStringInitFromBuf(&sIntName,` |
|        - | 5692 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - | 5693 | `				/* Only interfaces are allowed */` |
|      575 | 5694 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 5695 | `					pInterface = pInterface->pNextName;` |
|      ! 0 | 5696 | `				}` |
|      575 | 5697 | `				if( pInterface == 0 ){` |
|       40 | 5698 | `					if( !GenStateLintSkipsBase(pGen,pClass) ){` |
|      ! 0 | 5699 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - | 5700 | `							"Nonexistent base interface '%z'",&sIntName);` |
|      ! 0 | 5701 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 5702 | `							SyBlobRelease(&sResolved);` |
|      ! 0 | 5703 | `							return SXERR_ABORT;` |
|        - | 5704 | `						}` |
|      ! 0 | 5705 | `					}` |
|       22 | 5706 | `				}else{` |
|        - | 5707 | `					/* Reject user classes that try to implement Throwable` |
|        - | 5708 | `					 * directly (or via an interface that extends Throwable)` |
|        - | 5709 | `					 * unless they already extend Exception or Error.` |
|        - | 5710 | `					 * Exception and Error themselves are compiled from the` |
|        - | 5711 | `					 * built-in library and are exempt by FQN — a namespaced` |
|        - | 5712 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|      539 | 5713 | `					SyString *pFqn = &pClass->sName;` |
|      539 | 5714 | `					int bIsExceptionOrError =` |
|      283 | 5715 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|      817 | 5716 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|      548 | 5717 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|       28 | 5718 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|      539 | 5719 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|       18 | 5720 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|        3 | 5721 | `						!bIsExceptionOrError ){` |
|       12 | 5722 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 5723 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|        3 | 5724 | `							&pClass->sDisp);` |
|        9 | 5725 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 5726 | `							SyBlobRelease(&sResolved);` |
|      ! 0 | 5727 | `							return SXERR_ABORT;` |
|        - | 5728 | `						}` |
|        - | 5729 | `						/* Skip registration so the follow-up abstract-method` |
|        - | 5730 | `						 * check does not produce a duplicate fatal. */` |
|        6 | 5731 | `					}else{` |
|      533 | 5732 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|        - | 5733 | `					}` |
|        - | 5734 | `				}` |
|      575 | 5735 | `				SyBlobRelease(&sResolved);` |
|      575 | 5736 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      269 | 5737 | `					break;` |
|        - | 5738 | `				}` |
|       47 | 5739 | `				pGen->pIn++;/* Jump the comma */` |
|        5 | 5740 | `			}` |
|      264 | 5741 | `		}` |
|      970 | 5742 | `	}` |
|     6523 | 5743 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|        - | 5744 | `		/* Syntax error */` |
|        3 | 5745 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Expected '{' after class '%z' declaration",pName);` |
|        3 | 5746 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|        3 | 5747 | `		if( rc == SXERR_ABORT ){` |
|        - | 5748 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 5749 | `			return SXERR_ABORT;` |
|        - | 5750 | `		}` |
|        3 | 5751 | `		return SXRET_OK;` |
|        - | 5752 | `	}` |
|     6521 | 5753 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|     6521 | 5754 | `	pEnd = 0; /* cc warning */` |
|        - | 5755 | `	/* Delimit the class body */` |
|     6521 | 5756 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|     6521 | 5757 | `	if( pEnd >= pGen->pEnd ){` |
|        - | 5758 | `		/* Syntax error */` |
|      ! 0 | 5759 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|      ! 0 | 5760 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 5761 | `		if( rc == SXERR_ABORT ){` |
|        - | 5762 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 5763 | `			return SXERR_ABORT;` |
|        - | 5764 | `		}` |
|      ! 0 | 5765 | `		return SXRET_OK;` |
|        - | 5766 | `	}` |
|        - | 5767 | `	/* The delimiter token is the class body's closing brace */` |
|     6521 | 5768 | `	pClass->nEndLine = pEnd->nLine;` |
|        - | 5769 | `	/* Swap token stream */` |
|     6521 | 5770 | `	pTmp = pGen->pEnd;` |
|     6521 | 5771 | `	pGen->pEnd = pEnd;` |
|        - | 5772 | `	/* Merge the inherited flags (PH7_NewRawClass may have set INTERNAL) */` |
|     6521 | 5773 | `	pClass->iFlags \|= iFlags;` |
|     6521 | 5774 | `	if( pAnonName ){` |
|        - | 5775 | `` 		/* `new class {...}`: the name is synthesized, which is what makes `self` `` |
|        - | 5776 | `		 * inside it unusable in an intersection type (see PH7_CLASS_ANON). */` |
|      283 | 5777 | `		pClass->iFlags \|= PH7_CLASS_ANON;` |
|      139 | 5778 | `	}` |
|        - | 5779 | ``	/* ...which is what php's own attribute validators judge: `#[\Attribute]` on an`` |
|        - | 5780 | ``	 * abstract class, `#[\AllowDynamicProperties]` on a readonly one or an enum. */`` |
|     9774 | 5781 | `	if( GenStateCheckAttrPlacement(&(*pGen),&pClass->aAttrs,pClass->nLine,1,1,` |
|     9779 | 5782 | `			&pClass->sName,pClass->iFlags) == SXERR_ABORT ){` |
|      ! 0 | 5783 | `		return SXERR_ABORT;` |
|        - | 5784 | `	}` |
|        - | 5785 | `	/* This class/enum is now the lexical class for its body — see pCurClass. */` |
|     6521 | 5786 | `	pGen->pCurClass = pClass;` |
|     6521 | 5787 | `	pGen->pCurClassBlock = pGen->pCurrent;` |
|     6521 | 5788 | `	pGen->pCurBase = pBase;` |
|        - | 5789 | `	/* Start the parse process */` |
|     7815 | 5790 | `	for(;;){` |
|        - | 5791 | `		/* Jump leading/trailing semi-colons */` |
|    20315 | 5792 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|     4101 | 5793 | `			pGen->pIn++;` |
|        5 | 5794 | `		}` |
|    16219 | 5795 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 5796 | `			/* End of class body */` |
|     6357 | 5797 | `			break;` |
|        - | 5798 | `		}` |
|        - | 5799 | `		/* Bind a directly-preceding docblock to this member */` |
|     9867 | 5800 | `		GenStateSetPendingDoc(&(*pGen));` |
|     9862 | 5801 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|     4938 | 5802 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|        4 | 5803 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|        - | 5804 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|        2 | 5805 | `				&pGen->pIn->sData,pName);` |
|        3 | 5806 | `			if( rc == SXERR_ABORT ){` |
|        - | 5807 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 5808 | `				return SXERR_ABORT;` |
|        - | 5809 | `			}` |
|        3 | 5810 | `			goto done;` |
|        - | 5811 | `		}` |
|     9865 | 5812 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - | 5813 | `			/* Extract the current keyword */` |
|     9863 | 5814 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     9863 | 5815 | `			if( nKwrd == PH7_TKWRD_CASE && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|        - | 5816 | ``				/* Enum case declaration: `case NAME [= value];` */`` |
|      189 | 5817 | `				rc = GenStateCompileEnumCase(&(*pGen),pClass);` |
|      189 | 5818 | `				if( rc != SXRET_OK ){` |
|        9 | 5819 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 5820 | `						return SXERR_ABORT;` |
|        - | 5821 | `					}` |
|        9 | 5822 | `					goto done;` |
|        - | 5823 | `				}` |
|      183 | 5824 | `				continue;` |
|        - | 5825 | `			}` |
|     9679 | 5826 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|        - | 5827 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|        - | 5828 | `				TraitUseEntry sUse;` |
|      411 | 5829 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      411 | 5830 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      411 | 5831 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      257 | 5832 | `				for(;;){` |
|        - | 5833 | `					ph7_class *pTrait;` |
|        - | 5834 | `					SyBlob sResolved;` |
|        - | 5835 | `					SyString sTraitName;` |
|      465 | 5836 | `					sxu32 nUseLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|        - | 5837 | `					/* A trait name is a full class reference: it may be qualified or` |
|        - | 5838 | ``					 * fully-qualified (`use Foo\Bar\T;`, `use \Foo\Bar\T;`) — the generated`` |
|        - | 5839 | `					 * PHPUnit mocks name their traits absolutely. Parse it with the shared` |
|        - | 5840 | `					 * class-reference reader (handles the leading '\', every '\'-segment and` |
|        - | 5841 | `					 * namespace/import resolution) instead of a single-identifier read, which` |
|        - | 5842 | `					 * choked on the first '\'. */` |
|      465 | 5843 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      465 | 5844 | `					if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 5845 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 5846 | `						rc = PH7_GenCompileError(pGen,E_PARSE,nUseLine,` |
|      ! 0 | 5847 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|      ! 0 | 5848 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 5849 | `							return SXERR_ABORT;` |
|        - | 5850 | `						}` |
|      ! 0 | 5851 | `						break;` |
|        - | 5852 | `					}` |
|      465 | 5853 | `					pTrait = GenStateLinkClass(pGen,&sResolved);` |
|      465 | 5854 | `					SyStringInitFromBuf(&sTraitName,` |
|        - | 5855 | `						(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - | 5856 | `					/* Only traits are allowed */` |
|      465 | 5857 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|      ! 0 | 5858 | `						pTrait = pTrait->pNextName;` |
|      ! 0 | 5859 | `					}` |
|      465 | 5860 | `					if( pTrait == 0 ){` |
|       45 | 5861 | `						if( !GenStateLintSkipsBase(pGen,pClass) ){` |
|      ! 0 | 5862 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|        - | 5863 | `								"'%z' is not a trait",&sTraitName);` |
|      ! 0 | 5864 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 5865 | `								SyBlobRelease(&sResolved);` |
|      ! 0 | 5866 | `								return SXERR_ABORT;` |
|        - | 5867 | `							}` |
|      ! 0 | 5868 | `						}` |
|       25 | 5869 | `					}else{` |
|      425 | 5870 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|        - | 5871 | `					}` |
|      465 | 5872 | `					SyBlobRelease(&sResolved);` |
|        - | 5873 | `					/* GenStateParseClassReference already advanced past the whole name —` |
|        - | 5874 | `					 * continue only across a comma-separated trait list. */` |
|      465 | 5875 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      208 | 5876 | `						break;` |
|        - | 5877 | `					}` |
|       59 | 5878 | `					pGen->pIn++; /* Jump the comma */` |
|        5 | 5879 | `				}` |
|        - | 5880 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      411 | 5881 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|        - | 5882 | `					SyToken *pBlock;` |
|       40 | 5883 | `					pGen->pIn++; /* Jump '{' */` |
|       40 | 5884 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       40 | 5885 | `					sUse.pResolvStart = pGen->pIn;` |
|       40 | 5886 | `					sUse.pResolvEnd = pBlock;` |
|       40 | 5887 | `					if( pBlock < pGen->pEnd ){` |
|       40 | 5888 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       22 | 5889 | `					}else{` |
|      ! 0 | 5890 | `						pGen->pIn = pGen->pEnd;` |
|        - | 5891 | `					}` |
|       18 | 5892 | `				}` |
|      411 | 5893 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|        - | 5894 | `				/* The semicolon will be consumed by the outer loop */` |
|      411 | 5895 | `				continue;` |
|        - | 5896 | `			}` |
|     4634 | 5897 | `		}` |
|        - | 5898 | `		/* Everything else is a MEMBER: its modifier run and the declaration it` |
|        - | 5899 | `		 * modifies. */` |
|     9275 | 5900 | `		rc = GenStateCompileMember(&(*pGen),pClass,"class");` |
|     9275 | 5901 | `		if( rc != SXRET_OK ){` |
|      160 | 5902 | `			if( rc == SXERR_ABORT ){` |
|        6 | 5903 | `				return SXERR_ABORT;` |
|        - | 5904 | `			}` |
|      156 | 5905 | `			goto done;` |
|        - | 5906 | `		}` |
|        5 | 5907 | `	}` |
|        - | 5908 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|        - | 5909 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|        - | 5910 | `	 */` |
|     6357 | 5911 | `	rc = GenStateApplyTraitUses(&(*pGen),pClass,&aUseEntries);` |
|     6357 | 5912 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 5913 | `		SySetRelease(&aUseEntries);` |
|      ! 0 | 5914 | `		SySetRelease(&aInterfaces);` |
|      ! 0 | 5915 | `		return SXERR_ABORT;` |
|        - | 5916 | `	}` |
|     6357 | 5917 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|        - | 5918 | `		/* Enum validation + name/value props + cases()/from()/tryFrom() synthesis.` |
|        - | 5919 | `		 * Runs after trait application so trait-imported properties are caught. */` |
|      151 | 5920 | `		rc = GenStateEnumFinalize(&(*pGen),pClass,nLine);` |
|      151 | 5921 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 5922 | `			SySetRelease(&aUseEntries);` |
|      ! 0 | 5923 | `			SySetRelease(&aInterfaces);` |
|      ! 0 | 5924 | `			return SXERR_ABORT;` |
|        - | 5925 | `		}` |
|       73 | 5926 | `	}` |
|        - | 5927 | `	/* The members this class DECLARES that claim #[\Override] -- recorded here,` |
|        - | 5928 | `	 * before inheritance copies the base's records in beside them, and verified` |
|        - | 5929 | `	 * once the answer exists. */` |
|     6357 | 5930 | `	GenStateCollectOverrides(&(*pGen),pClass,&aOvMeth,&aOvProp);` |
|        - | 5931 | `	/* Reject a php-fatal redeclaration before hoisting the class. An ENUM is` |
|        - | 5932 | `	 * never early-bound either: php gives every one of them UnitEnum. */` |
|     9283 | 5933 | `	if( GenStateGuardClassRedeclaration(pGen,pClass,` |
|     6352 | 5934 | `			SySetUsed(&aInterfaces) == 0 && SySetUsed(&aUseEntries) == 0` |
|     5684 | 5935 | `			&& (pClass->iFlags & PH7_CLASS_ENUM) == 0` |
|     5862 | 5936 | `			&& SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) == 0)` |
|     3181 | 5937 | `		== SXERR_ABORT ){` |
|       15 | 5938 | `		SySetRelease(&aOvMeth);` |
|       15 | 5939 | `		SySetRelease(&aOvProp);` |
|       15 | 5940 | `		return SXERR_ABORT;` |
|        - | 5941 | `	}` |
|        - | 5942 | `	/* Install the class */` |
|     6345 | 5943 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|     6345 | 5944 | `	if( rc == SXRET_OK ){` |
|        - | 5945 | `		ph7_class **apInterface;` |
|        - | 5946 | `		sxu32 n;` |
|     6345 | 5947 | `		if( pBase ){` |
|        - | 5948 | `			/* Inherit from base class and mark as a subclass */` |
|     1419 | 5949 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|     1419 | 5950 | `			if( rc == SXRET_OK && GenStateCheckVirtualDefaults(&(*pGen),pClass) == SXERR_ABORT ){` |
|      ! 0 | 5951 | `				SySetRelease(&aOvMeth);` |
|      ! 0 | 5952 | `				SySetRelease(&aOvProp);` |
|      ! 0 | 5953 | `				return SXERR_ABORT;` |
|        - | 5954 | `			}` |
|      707 | 5955 | `		}` |
|     6345 | 5956 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|     6873 | 5957 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|        - | 5958 | `			/* Implements one or more interface */` |
|      533 | 5959 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|      533 | 5960 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 5961 | `				break;` |
|        - | 5962 | `			}` |
|      269 | 5963 | `		}` |
|        - | 5964 | `		/* Auto-implement UnitEnum (and BackedEnum for backed enums) — php 8.1:` |
|        - | 5965 | ``		 * every enum satisfies `instanceof UnitEnum` implicitly. */`` |
|     6345 | 5966 | `		if( rc == SXRET_OK && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|      149 | 5967 | `			ph7_class *pIntf = PH7_VmExtractClass(pGen->pVm,"UnitEnum",sizeof("UnitEnum")-1,FALSE,0);` |
|      149 | 5968 | `			while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 5969 | `				pIntf = pIntf->pNextName;` |
|      ! 0 | 5970 | `			}` |
|      149 | 5971 | `			if( pIntf ){` |
|      149 | 5972 | `				PH7_ClassImplement(pClass,pIntf);` |
|       72 | 5973 | `			}` |
|      149 | 5974 | `			if( pClass->nEnumBacking != 0 ){` |
|       75 | 5975 | `				pIntf = PH7_VmExtractClass(pGen->pVm,"BackedEnum",sizeof("BackedEnum")-1,FALSE,0);` |
|       75 | 5976 | `				while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 5977 | `					pIntf = pIntf->pNextName;` |
|      ! 0 | 5978 | `				}` |
|       75 | 5979 | `				if( pIntf ){` |
|       75 | 5980 | `					PH7_ClassImplement(pClass,pIntf);` |
|       35 | 5981 | `				}` |
|       35 | 5982 | `			}` |
|       72 | 5983 | `		}` |
|        - | 5984 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|        - | 5985 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|     6340 | 5986 | `		if( rc == SXRET_OK` |
|     6340 | 5987 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|     6345 | 5988 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|      333 | 5989 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|        - | 5990 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|      333 | 5991 | `			if( pStringable ){` |
|      333 | 5992 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|      333 | 5993 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|        - | 5994 | `				sxu32 i;` |
|      333 | 5995 | `				int bAlready = 0;` |
|      339 | 5996 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|       10 | 5997 | `					if( apImpl[i] == pStringable ){` |
|        3 | 5998 | `						bAlready = 1;` |
|        3 | 5999 | `						break;` |
|        - | 6000 | `					}` |
|        4 | 6001 | `				}` |
|      333 | 6002 | `				if( !bAlready ){` |
|      331 | 6003 | `					PH7_ClassImplement(pClass,pStringable);` |
|      163 | 6004 | `				}` |
|      164 | 6005 | `			}` |
|      164 | 6006 | `		}` |
|        - | 6007 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|     6345 | 6008 | `		if( rc == SXRET_OK ){` |
|     6345 | 6009 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|     6345 | 6010 | `			if( rcCheck == SXERR_ABORT ){` |
|      ! 0 | 6011 | `				SySetRelease(&aUseEntries);` |
|      ! 0 | 6012 | `				SySetRelease(&aInterfaces);` |
|      ! 0 | 6013 | `				SySetRelease(&aOvMeth);` |
|      ! 0 | 6014 | `				SySetRelease(&aOvProp);` |
|      ! 0 | 6015 | `				return SXERR_ABORT;` |
|        - | 6016 | `			}` |
|     3170 | 6017 | `		}` |
|        - | 6018 | `		/* ...and every interface property against what the class holds by now */` |
|     6345 | 6019 | `		if( rc == SXRET_OK && PH7_ClassCheckInterfaceProps(&(*pGen),pClass) == SXERR_ABORT ){` |
|      ! 0 | 6020 | `			SySetRelease(&aUseEntries);` |
|      ! 0 | 6021 | `			SySetRelease(&aInterfaces);` |
|      ! 0 | 6022 | `			SySetRelease(&aOvMeth);` |
|      ! 0 | 6023 | `			SySetRelease(&aOvProp);` |
|      ! 0 | 6024 | `			return SXERR_ABORT;` |
|        - | 6025 | `		}` |
|        - | 6026 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|     6345 | 6027 | `		if( rc == SXRET_OK ){` |
|     6345 | 6028 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|     6345 | 6029 | `			if( rcCheck == SXERR_ABORT ){` |
|      ! 0 | 6030 | `				SySetRelease(&aUseEntries);` |
|      ! 0 | 6031 | `				SySetRelease(&aInterfaces);` |
|      ! 0 | 6032 | `				SySetRelease(&aOvMeth);` |
|      ! 0 | 6033 | `				SySetRelease(&aOvProp);` |
|      ! 0 | 6034 | `				return SXERR_ABORT;` |
|        - | 6035 | `			}` |
|     3170 | 6036 | `		}` |
|        - | 6037 | `		/* ...and the #[\Override] claims LAST: php reports an unimplemented` |
|        - | 6038 | `		 * abstract method and an inheritance visibility clash before this one,` |
|        - | 6039 | `		 * and stops there — php's E_COMPILE_ERROR does not return, so a` |
|        - | 6040 | `		 * declaration that already failed says nothing more. */` |
|     6345 | 6041 | `		if( rc == SXRET_OK && pGen->nErr == nErrEntry ){` |
|     5971 | 6042 | `			sxi32 rcCheck = GenStateCheckOverrides(&(*pGen),pClass,&aOvMeth,&aOvProp);` |
|     5971 | 6043 | `			if( rcCheck == SXERR_ABORT ){` |
|      ! 0 | 6044 | `				SySetRelease(&aUseEntries);` |
|      ! 0 | 6045 | `				SySetRelease(&aInterfaces);` |
|      ! 0 | 6046 | `				SySetRelease(&aOvMeth);` |
|      ! 0 | 6047 | `				SySetRelease(&aOvProp);` |
|      ! 0 | 6048 | `				return SXERR_ABORT;` |
|        - | 6049 | `			}` |
|     2983 | 6050 | `		}` |
|     3170 | 6051 | `	}` |
|     6345 | 6052 | `	SySetRelease(&aUseEntries);` |
|     6345 | 6053 | `	SySetRelease(&aInterfaces);` |
|     6345 | 6054 | `	SySetRelease(&aOvMeth);` |
|     6345 | 6055 | `	SySetRelease(&aOvProp);` |
|     6345 | 6056 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 6057 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 6058 | `		return SXERR_ABORT;` |
|        - | 6059 | `	}` |
|     6345 | 6060 | `	pGen->pDeclClass = pClass;` |
|     3250 | 6061 | `done:` |
|     6505 | 6062 | `	pGen->pCurClass = pSavedCurClass;` |
|     6505 | 6063 | `	pGen->pCurBase = pSavedCurBase;` |
|     6505 | 6064 | `	pGen->pCurClassBlock = pSavedCurClassBlock;` |
|        - | 6065 | `	/* Point beyond the class body */` |
|     6505 | 6066 | `	pGen->pIn = &pEnd[1];` |
|     6505 | 6067 | `	pGen->pEnd = pTmp;` |
|     6505 | 6068 | `	return PH7_OK;` |
|     3353 | 6069 | `}` |
|        - | 6070 | `/*` |
|        - | 6071 | ` * A class whose link leaves a variance pair only a class nothing has loaded can` |
|        - | 6072 | ` * decide (an interface's get-only property typed with a class declared further` |
|        - | 6073 | ` * down, a parameter naming one an autoloader supplies) is not refused or passed` |
|        - | 6074 | ` * here: php declares such a class where its statement RUNS and settles the pair` |
|        - | 6075 | ` * there. The pairs are collected while the body links, and PH7_OP_CLASS_OBLIGE` |
|        - | 6076 | ` * at the declaration's own position settles them in statement order.` |
|        - | 6077 | ` */` |
|     6696 | 6078 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|        - | 6079 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|        5 | 6080 | `{` |
|     6701 | 6081 | `	struct VmClassObligeSet *pSaved = pGen->pOblige;` |
|        - | 6082 | `	VmClassObligeSet sOblige;` |
|     6701 | 6083 | `	sxu32 nDeclLine = pGen->pIn ? pGen->pIn->nLine : 0;` |
|        - | 6084 | `	ph7_class *pDecl;` |
|     6701 | 6085 | `	int bHide = 0;` |
|        - | 6086 | `	sxi32 rc;` |
|     6701 | 6087 | `	SySetInit(&sOblige.aOblige,&pGen->pVm->sAllocator,sizeof(VmClassOblige));` |
|     6701 | 6088 | `	SySetInit(&sOblige.aName,&pGen->pVm->sAllocator,sizeof(SyString));` |
|     6701 | 6089 | `	sOblige.bDone = 0;` |
|        - | 6090 | ``	/* A check compile links nothing, and `phl -l` runs nothing to settle at. */`` |
|     6701 | 6091 | `	pGen->pOblige = (pGen->bDeclCheck \|\| pGen->pVm->bSyntaxCheck) ? 0 : &sOblige;` |
|     6701 | 6092 | `	pGen->pDeclClass = 0;` |
|     6701 | 6093 | `	rc = GenStateCompileClassBody(pGen,iFlags,pAnonName,ppArgStart,ppArgEnd);` |
|     6701 | 6094 | `	pGen->pOblige = pSaved;` |
|     6701 | 6095 | `	pDecl = pGen->pDeclClass;` |
|     6701 | 6096 | `	pGen->pDeclClass = 0;` |
|        - | 6097 | `	/* Decided before the settling is emitted, which runs first. */` |
|     9724 | 6098 | `	bHide = rc == SXRET_OK && pDecl && pAnonName == 0` |
|    10036 | 6099 | `		&& GenStateDeclaredWhereRun(pGen,pDecl,SySetUsed(&sOblige.aOblige) > 0);` |
|     6716 | 6100 | `	if( rc == SXRET_OK && SySetUsed(&sOblige.aOblige) > 0 ){` |
|       34 | 6101 | `		VmClassObligeSet *pSet = (VmClassObligeSet *)SyMemBackendAlloc(&pGen->pVm->sAllocator,` |
|        - | 6102 | `			sizeof(VmClassObligeSet));` |
|        - | 6103 | `		sxu32 nIdx;` |
|       34 | 6104 | `		if( pSet == 0 ){` |
|      ! 0 | 6105 | `			SySetRelease(&sOblige.aOblige);` |
|      ! 0 | 6106 | `			SySetRelease(&sOblige.aName);` |
|      ! 0 | 6107 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|        - | 6108 | `				"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 6109 | `			return SXERR_ABORT;` |
|        - | 6110 | `		}` |
|       34 | 6111 | `		*pSet = sOblige;` |
|       30 | 6112 | `		if( PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLASS_OBLIGE,0,0,(void *)pSet,&nIdx) == SXRET_OK` |
|       34 | 6113 | `		 && nDeclLine > 0 ){` |
|        - | 6114 | `			/* It runs AS the declaration: an autoload it triggers is called from,` |
|        - | 6115 | `			 * and a refusal reported at, the statement's line, not the token after` |
|        - | 6116 | `			 * the body. */` |
|       34 | 6117 | `			VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm,nIdx);` |
|       34 | 6118 | `			if( pInstr ){` |
|       34 | 6119 | `				pInstr->nLine = nDeclLine;` |
|       15 | 6120 | `			}` |
|       15 | 6121 | `		}` |
|       19 | 6122 | `	}else{` |
|     6671 | 6123 | `		SySetRelease(&sOblige.aOblige);` |
|     6671 | 6124 | `		SySetRelease(&sOblige.aName);` |
|        - | 6125 | `	}` |
|     6701 | 6126 | `	if( bHide ){` |
|     1107 | 6127 | `		GenStateHideUntilDeclared(pGen,pDecl,nDeclLine);` |
|      551 | 6128 | `	}` |
|     6701 | 6129 | `	return rc;` |
|     3353 | 6130 | `}` |
|        - | 6131 | `/* Compile a named class declaration (the common case). */` |
|     6244 | 6132 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|        5 | 6133 | `{` |
|     6249 | 6134 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|        5 | 6135 | `}` |
|        - | 6136 | `/*` |
|        - | 6137 | `` * The PREFIX php puts in front of an anonymous class's `@anonymous`: the resolved`` |
|        - | 6138 | ` * parent class name, else the resolved name of the FIRST implemented interface,` |
|        - | 6139 | ` * else the literal "class". Both are the name as WRITTEN, resolved through the` |
|        - | 6140 | `` * file's namespace and `use` imports -- not the name of whatever class the`` |
|        - | 6141 | ` * reference turns out to bind to, which at this point may not be declared yet.` |
|        - | 6142 | ` *` |
|        - | 6143 | `` * Peeks: the cursor sits on the `class` keyword when this runs and has to still`` |
|        - | 6144 | ` * sit there when it returns, because GenStateCompileClassEx parses the same` |
|        - | 6145 | ` * tokens for real straight afterwards.` |
|        - | 6146 | ` */` |
|      280 | 6147 | `static void GenStateAnonClassPrefix(ph7_gen_state *pGen,SyToken *pKw,SyBlob *pOut)` |
|        5 | 6148 | `{` |
|      285 | 6149 | `	SyToken *pSaveIn = pGen->pIn;` |
|      285 | 6150 | `	SyToken *pIn = pKw;` |
|      285 | 6151 | `	int bGot = 0;` |
|      285 | 6152 | `	pIn++; /* Step over 'class' */` |
|      285 | 6153 | `	if( pIn < pGen->pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|        - | 6154 | ``		/* `new class(args) extends B {}` -- the constructor list sits between the`` |
|        - | 6155 | `		 * keyword and the inheritance clause. */` |
|      114 | 6156 | `		SyToken *pClose = 0;` |
|      114 | 6157 | `		PH7_DelimitNestedTokens(&pIn[1],pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|      114 | 6158 | `		pIn = ( pClose && pClose < pGen->pEnd ) ? &pClose[1] : pGen->pEnd;` |
|       55 | 6159 | `	}` |
|      285 | 6160 | `	if( pIn < pGen->pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|      135 | 6161 | `		sxi32 nKwrd = SX_PTR_TO_INT(pIn->pUserData);` |
|      135 | 6162 | `		if( nKwrd == PH7_TKWRD_EXTENDS \|\| nKwrd == PH7_TKWRD_IMPLEMENTS ){` |
|      135 | 6163 | `			pGen->pIn = &pIn[1];` |
|      135 | 6164 | `			if( GenStateParseClassReference(pGen,pOut) == SXRET_OK ){` |
|      135 | 6165 | `				bGot = 1;` |
|       65 | 6166 | `			}` |
|       65 | 6167 | `		}` |
|       65 | 6168 | `	}` |
|      285 | 6169 | `	pGen->pIn = pSaveIn;` |
|      285 | 6170 | `	if( !bGot ){` |
|      154 | 6171 | `		SyBlobAppend(pOut,"class",sizeof("class")-1);` |
|       75 | 6172 | `	}` |
|      285 | 6173 | `}` |
|        - | 6174 | `/*` |
|        - | 6175 | ` * Synthesize php's name for an anonymous class into pOut:` |
|        - | 6176 | ` *` |
|        - | 6177 | ` *     <prefix>@anonymous \0 <file>:<line>$<hex>` |
|        - | 6178 | ` *` |
|        - | 6179 | ` * The NUL is php's, and it is the whole reason this name has two halves. php` |
|        - | 6180 | ` * builds it with one snprintf and then gets the short form everywhere for free,` |
|        - | 6181 | `` * because every diagnostic prints a class name with `%s` and stops there; only`` |
|        - | 6182 | ` * the surfaces that hand the name back as a VALUE -- get_class(), ::class,` |
|        - | 6183 | ` * __CLASS__, ReflectionClass::getName() -- carry the whole thing. Libraries read` |
|        - | 6184 | `` * it exactly that way: `strpos($class, "@anonymous\0") !== false` is monolog's`` |
|        - | 6185 | ` * Utils::getClass and symfony's idiom for "is this an anonymous class".` |
|        - | 6186 | ` *` |
|        - | 6187 | ` * The tail is what makes the name UNIQUE, which the prefix alone is not: two` |
|        - | 6188 | ` * anonymous classes written on the same line share a file, a line and usually a` |
|        - | 6189 | ` * parent, so php separates them with a request-wide counter in hex` |
|        - | 6190 | ` * (CG(rtd_key_counter), bumped in compile order).` |
|        - | 6191 | ` */` |
|      280 | 6192 | `static void GenStateAnonClassName(ph7_gen_state *pGen,SyToken *pKw,SyBlob *pOut)` |
|        5 | 6193 | `{` |
|      285 | 6194 | `	SyString *pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|      285 | 6195 | `	GenStateAnonClassPrefix(pGen,pKw,pOut);` |
|      285 | 6196 | `	SyBlobAppend(pOut,"@anonymous",sizeof("@anonymous")-1);` |
|      285 | 6197 | `	SyBlobAppend(pOut,"\0",1);` |
|      285 | 6198 | `	if( pFile && pFile->nByte > 0 ){` |
|      285 | 6199 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|      145 | 6200 | `	}else{` |
|        - | 6201 | `		/* No file on the include stack: an :MEMORY: chunk, which is what the magic` |
|        - | 6202 | `		 * __FILE__ constant answers in the same situation. */` |
|      ! 0 | 6203 | `		SyBlobAppend(pOut,":MEMORY:",sizeof(":MEMORY:")-1);` |
|        - | 6204 | `	}` |
|      285 | 6205 | `	SyBlobFormat(pOut,":%u$%x",pKw->nLine,pGen->pVm->nAnonSeq++);` |
|      285 | 6206 | `}` |
|        - | 6207 | `/*` |
|        - | 6208 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|        - | 6209 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|        - | 6210 | ` * compile + install the class body once (at compile time, like every other` |
|        - | 6211 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|        - | 6212 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|        - | 6213 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|        - | 6214 | ` */` |
|      288 | 6215 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 6216 | `{` |
|        - | 6217 | `	SyString sName;` |
|        - | 6218 | `	SyToken *pArgStart,*pArgEnd;` |
|        - | 6219 | `	SyToken *pTokKw;` |
|      293 | 6220 | `	sxi32 iAnonFlags = 0;` |
|      293 | 6221 | `	int bRecompile = 0;` |
|        - | 6222 | `	ph7_value *pObj;` |
|      293 | 6223 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 6224 | `	sxu32 nIdx,nLen;` |
|        - | 6225 | `	sxi32 nArg,rc;` |
|      144 | 6226 | `	SXUNUSED(iCompileFlag);` |
|      293 | 6227 | `	if( GenStateIsReadonly(pGen->pIn) && &pGen->pIn[1] < pGen->pEnd ){` |
|        - | 6228 | ``		/* `new readonly class …` (PHP 8.3). Step over the modifier so everything`` |
|        - | 6229 | ``		 * below sees the cursor on `class`, where it has always been, and carry`` |
|        - | 6230 | `		 * the flag into the class body — which is what makes every property` |
|        - | 6231 | `		 * readonly and refuses a non-readonly base. */` |
|        7 | 6232 | `		iAnonFlags = PH7_CLASS_READONLY;` |
|        7 | 6233 | `		pGen->pIn++;` |
|        3 | 6234 | `	}` |
|      293 | 6235 | ``	pTokKw = pGen->pIn; /* Attribute-sidecar key: `new #[A] class` trivia`` |
|        - | 6236 | `	                     * is keyed to this 'class' token */` |
|      293 | 6237 | `	if( pGen->pVm->sDeferAnonName.nByte > 0 ){` |
|        - | 6238 | `		/* Deferred re-compile (VmExecDeferredClass): install under the SAME` |
|        - | 6239 | `		 * synthesized name the original site's OP_NEW loads. One-shot. */` |
|       10 | 6240 | `		bRecompile = 1;` |
|       10 | 6241 | `		sName = pGen->pVm->sDeferAnonName;` |
|       10 | 6242 | `		nLen = sName.nByte;` |
|       10 | 6243 | `		pGen->pVm->sDeferAnonName.zString = 0;` |
|       10 | 6244 | `		pGen->pVm->sDeferAnonName.nByte = 0;` |
|        6 | 6245 | `	}else{` |
|        - | 6246 | `		/* Synthesize php's name. It carries a NUL and a path, so it does not fit a` |
|        - | 6247 | ``		 * stack buffer and cannot be built with the `%s` formatter; and it has to`` |
|        - | 6248 | `		 * outlive this function (the site's OP_NEW loads it as a literal), so it is` |
|        - | 6249 | `		 * duplicated into the VM allocator exactly as the deferred path's copy is. */` |
|        - | 6250 | `		SyBlob sAnon;` |
|        - | 6251 | `		char *zDup;` |
|      285 | 6252 | `		SyBlobInit(&sAnon,&pGen->pVm->sAllocator);` |
|      285 | 6253 | `		GenStateAnonClassName(pGen,pTokKw,&sAnon);` |
|      285 | 6254 | `		nLen = (sxu32)SyBlobLength(&sAnon);` |
|      285 | 6255 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sAnon),nLen);` |
|      285 | 6256 | `		SyBlobRelease(&sAnon);` |
|      285 | 6257 | `		if( zDup == 0 ){` |
|      ! 0 | 6258 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 6259 | `			return SXERR_ABORT;` |
|        - | 6260 | `		}` |
|      285 | 6261 | `		SyStringInitFromBuf(&sName,zDup,nLen);` |
|        - | 6262 | `	}` |
|        - | 6263 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|        - | 6264 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|        - | 6265 | `	 * delimited construct; GenStateCompileClassEx restores both on success.` |
|        - | 6266 | ``	 * Deferral gate: `new class extends \App\Child {}` where the`` |
|        - | 6267 | `	 * parent's autoloader has not RUN yet — capture the class for a runtime` |
|        - | 6268 | `	 * re-compile and keep only the site's argument/OP_NEW emission here. */` |
|      293 | 6269 | `	pArgStart = pArgEnd = 0;` |
|        - | 6270 | `	{` |
|        - | 6271 | `		SySet aMissing;` |
|      293 | 6272 | `		SyToken *pBody = 0;` |
|      293 | 6273 | `		SyToken *pBodyEnd = 0;` |
|        - | 6274 | `		SyBlob sSelfFqn;` |
|      293 | 6275 | `		int bDeferred = 0;` |
|      293 | 6276 | `		SySetInit(&aMissing,&pGen->pVm->sAllocator,sizeof(VmDeferredReq));` |
|      293 | 6277 | `		SyBlobInit(&sSelfFqn,&pGen->pVm->sAllocator);` |
|        - | 6278 | `		/* An anonymous class is an EXPRESSION: it is always compiled where it runs,` |
|        - | 6279 | `		 * so resolving its parent here is resolving it at its execution point --` |
|        - | 6280 | `		 * the autoload belongs. */` |
|      432 | 6281 | `		if( GenStateScanDeferDeps(pGen,1,PH7_DEFER_KIND_CLASS,&aMissing,&pBody,&pBodyEnd,&sSelfFqn,` |
|      288 | 6282 | `				pGen->bDeclCheck) == SXRET_OK` |
|      293 | 6283 | `		 && SySetUsed(&aMissing) > 0 && !pGen->pVm->bSyntaxCheck && !pGen->bDeclCheck ){` |
|       13 | 6284 | `			if( &pTokKw[1] < pGen->pEnd && (pTokKw[1].nType & PH7_TK_LPAREN) ){` |
|        6 | 6285 | `				SyToken *pClose = 0;` |
|        6 | 6286 | `				PH7_DelimitNestedTokens(&pTokKw[2],pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|        6 | 6287 | `				if( pClose && pClose < pGen->pEnd ){` |
|        6 | 6288 | `					pArgStart = &pTokKw[2];` |
|        6 | 6289 | `					pArgEnd = pClose;` |
|        2 | 6290 | `				}` |
|        2 | 6291 | `			}` |
|       13 | 6292 | `			rc = GenStateEmitDeferredClass(pGen,0,1,&aMissing,pBodyEnd,0,&sName);` |
|       13 | 6293 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 6294 | `				SySetRelease(&aMissing);` |
|      ! 0 | 6295 | `				SyBlobRelease(&sSelfFqn);` |
|      ! 0 | 6296 | `				return SXERR_ABORT;` |
|        - | 6297 | `			}` |
|       13 | 6298 | `			bDeferred = ( rc == SXRET_OK );` |
|        5 | 6299 | `		}` |
|      293 | 6300 | `		SySetRelease(&aMissing);` |
|      293 | 6301 | `		SyBlobRelease(&sSelfFqn);` |
|      293 | 6302 | `		if( !bDeferred ){` |
|      283 | 6303 | `			rc = GenStateCompileClassEx(pGen,iAnonFlags,&sName,&pArgStart,&pArgEnd);` |
|      283 | 6304 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 6305 | `				return rc;` |
|        - | 6306 | `			}` |
|        - | 6307 | `			{` |
|        - | 6308 | ``				/* Expression-position attributes (`new #[A] class {…}`) */`` |
|        - | 6309 | `				/* (A check compile installed none, and must not autoload its name.) */` |
|      422 | 6310 | `				ph7_class *pAnonClass = pGen->bDeclCheck ? 0` |
|      278 | 6311 | `					: PH7_VmExtractClass(pGen->pVm,sName.zString,nLen,FALSE,0);` |
|      278 | 6312 | `				if( pAnonClass` |
|      283 | 6313 | `				 && GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnonClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 6314 | `					return SXERR_ABORT;` |
|        - | 6315 | `				}` |
|        - | 6316 | `				/* ...judged by the same placement rules as a named class's, which` |
|        - | 6317 | ``				 * this door never asked: `new #[\Override] class {}` compiled. */`` |
|      422 | 6318 | `				if( pAnonClass && GenStateCheckAttrPlacement(&(*pGen),&pAnonClass->aAttrs,` |
|      417 | 6319 | `						pAnonClass->nLine,1,1,&pAnonClass->sDisp,pAnonClass->iFlags) == SXERR_ABORT ){` |
|      ! 0 | 6320 | `					return SXERR_ABORT;` |
|        - | 6321 | `				}` |
|      278 | 6322 | `				if( pAnonClass && !bRecompile && !pGen->pVm->bSyntaxCheck` |
|      269 | 6323 | `				 && !pGen->pVm->bCompilingBuiltin && pGen->nErr == 0` |
|      339 | 6324 | `				 && (pAnonClass->pBase \|\| SySetUsed(&pAnonClass->aInterface) > 0` |
|      154 | 6325 | `				  \|\| SySetUsed(&pAnonClass->aTrait) > 0` |
|      137 | 6326 | `				  \|\| SyHashGet(&pAnonClass->hMethod,"__toString",sizeof("__toString")-1)) ){` |
|        - | 6327 | `					/* php links an anonymous class at compile time only when it has` |
|        - | 6328 | `					 * no parent, no interface (__toString brings Stringable) and no` |
|        - | 6329 | `					 * trait; any other one is linked by its DECLARE_ANON_CLASS, and` |
|        - | 6330 | `					 * get_declared_classes() lists it only once that has run. A` |
|        - | 6331 | `					 * deferred re-compile already runs where the expression does. */` |
|      133 | 6332 | `					GenStateHideUntilDeclared(pGen,pAnonClass,nLine);` |
|       64 | 6333 | `				}` |
|      283 | 6334 | `				if( pAnonClass ){` |
|        - | 6335 | `					/* php's DECLARE_ANON_CLASS runs BEFORE the constructor arguments, and` |
|        - | 6336 | `					 * it is where an unimplemented abstract method is refused -- so` |
|        - | 6337 | ``					 * `new class(f()) extends Abs {}` never evaluates `f()`. The named`` |
|        - | 6338 | `					 * path spells that ordering as a screen OP_NEW ahead of its argument` |
|        - | 6339 | `					 * list; this list is emitted from raw tokens with the class name` |
|        - | 6340 | `					 * pushed after it, so the screen carries the class in p3 instead and` |
|        - | 6341 | `					 * touches no stack (iP1 == -2). The check itself is the same routine` |
|        - | 6342 | `					 * the instantiation runs, so the two can never disagree. */` |
|      283 | 6343 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,-2,0,(void *)pAnonClass,0);` |
|      139 | 6344 | `				}` |
|        - | 6345 | `			}` |
|      139 | 6346 | `		}` |
|        - | 6347 | `	}` |
|        - | 6348 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|        - | 6349 | `	 * with the constructor arguments beneath it, so push the args first.` |
|        - | 6350 | `	 *` |
|        - | 6351 | `	 * This argument list is compiled from RAW TOKENS rather than through the` |
|        - | 6352 | `	 * expression parser's argument machinery (the class body sits between the` |
|        - | 6353 | `	 * parentheses and the rest of the expression), so the two forms that machinery` |
|        - | 6354 | ``	 * recognizes have to be recognized here too — `...$args` and `name: $v`. They`` |
|        - | 6355 | `	 * were not: a spread was compiled as one ordinary argument, so` |
|        - | 6356 | ``	 * `new class(...$a) {}` passed the ARRAY where php passes its elements, and a`` |
|        - | 6357 | ``	 * named argument was a `Syntax error: Unexpected token ':'` on source php`` |
|        - | 6358 | `	 * compiles. */` |
|      293 | 6359 | `	nArg = 0;` |
|        - | 6360 | `	{` |
|        - | 6361 | `	SySet aArgName;              /* one SyString per argument; {0,0} == positional */` |
|      293 | 6362 | `	int hasNamed = 0, hasSpread = 0;` |
|        - | 6363 | `	sxu32 aNamedSend[16];        /* the PH7_OP_NAMED_SEND screens emitted, patched with the map */` |
|      293 | 6364 | `	sxu32 nNamedSend = 0, n;` |
|        - | 6365 | `	void *p3;` |
|      293 | 6366 | `	SySetInit(&aArgName,&pGen->pVm->sAllocator,sizeof(SyString));` |
|      293 | 6367 | `	if( pArgStart < pArgEnd ){` |
|      112 | 6368 | `		SyToken *pSavedIn = pGen->pIn;` |
|      112 | 6369 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|        - | 6370 | `		SyToken *pArgNext;` |
|      112 | 6371 | `		const char *zOrder = 0;   /* set when this argument's POSITION or shape is refused */` |
|        - | 6372 | ``		SyString sOrderName;      /* nByte > 0: zOrder is a `'%z:'` format, E_PARSE */`` |
|      112 | 6373 | `		SyZero(&sOrderName,sizeof(sOrderName));` |
|      112 | 6374 | `		pGen->pIn = pArgStart;` |
|      112 | 6375 | `		pGen->pEnd = pArgEnd;` |
|      264 | 6376 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|      160 | 6377 | `			SyToken *pArgIn = pGen->pIn;` |
|        - | 6378 | `			SyString sArgName;` |
|      160 | 6379 | `			int bSpread = 0;` |
|      160 | 6380 | `			SyZero(&sArgName,sizeof(sArgName));` |
|      160 | 6381 | `			if( pArgIn < pArgNext && (pArgIn->nType & PH7_TK_ELLIPSIS) ){` |
|       29 | 6382 | `				bSpread = 1;` |
|       29 | 6383 | `				pArgIn++;` |
|       29 | 6384 | `				if( hasNamed ){` |
|        3 | 6385 | `					zOrder = "Cannot use argument unpacking after named arguments";` |
|        1 | 6386 | `				}` |
|      146 | 6387 | `			}else if( &pArgIn[1] < pArgNext` |
|      117 | 6388 | `			 && (pArgIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|       98 | 6389 | `			 && (pArgIn[1].nType & PH7_TK_COLON) ){` |
|        - | 6390 | ``				/* `name: value`. php accepts a reserved word as a parameter name, and`` |
|        - | 6391 | ``				 * `::` lexes as its own operator, so an ID/KEYWORD followed by a SINGLE`` |
|        - | 6392 | `				 * colon at the head of an argument can only be this. */` |
|       74 | 6393 | `				sArgName = pArgIn->sData;` |
|       74 | 6394 | `				hasNamed = 1;` |
|       74 | 6395 | `				pArgIn += 2;` |
|       74 | 6396 | `				if( pArgIn >= pArgNext ){` |
|        - | 6397 | ``					/* `new class(a:) {}` — a name with no value. Spelled exactly as the`` |
|        - | 6398 | `					 * ordinary argument path spells it (php names the token that stopped` |
|        - | 6399 | `					 * it instead; that wording gap belongs to the parse-error family and` |
|        - | 6400 | `					 * is now one gap in both places rather than silence in this one). */` |
|        3 | 6401 | `					zOrder = "syntax error, expected expression after named argument '%z:'";` |
|        3 | 6402 | `					sOrderName = sArgName;` |
|       73 | 6403 | `				}else if( pArgIn->nType & PH7_TK_ELLIPSIS ){` |
|      ! 0 | 6404 | `					zOrder = "syntax error, unexpected token \"...\"";` |
|        4 | 6405 | `				}` |
|       99 | 6406 | `			}else if( hasNamed ){` |
|      ! 0 | 6407 | `				zOrder = "Cannot use positional argument after named argument";` |
|       64 | 6408 | `			}else if( hasSpread ){` |
|      ! 0 | 6409 | `				zOrder = "Cannot use positional argument after argument unpacking";` |
|      ! 0 | 6410 | `			}` |
|      160 | 6411 | `			if( zOrder ){` |
|        - | 6412 | `				/* The same four rules the ordinary call path enforces at COMPILE time` |
|        - | 6413 | `				 * (GenStateEmitCallArgs); an anonymous class's list is parsed here and` |
|        - | 6414 | `				 * so had none of them. */` |
|        6 | 6415 | `				sxu32 nErrLine = pArgNext > pArgStart ? pArgNext[-1].nLine : nLine;` |
|        6 | 6416 | `				pGen->pIn = pSavedIn;` |
|        6 | 6417 | `				pGen->pEnd = pSavedEnd;` |
|        6 | 6418 | `				SySetRelease(&aArgName);` |
|        6 | 6419 | `				if( sOrderName.nByte > 0 ){` |
|        3 | 6420 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,nErrLine,zOrder,&sOrderName);` |
|        2 | 6421 | `				}else{` |
|        3 | 6422 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,nErrLine,"%s",zOrder);` |
|        - | 6423 | `				}` |
|        6 | 6424 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 6425 | `			}` |
|      156 | 6426 | `			if( pArgIn < pArgNext ){` |
|      156 | 6427 | `				sxi32 iArgFlags = EXPR_FLAG_RDONLY_LOAD;` |
|      152 | 6428 | `				if( !bSpread && &pArgIn[2] == pArgNext && (pArgIn->nType & PH7_TK_DOLLAR)` |
|       34 | 6429 | `				 && (pArgIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        - | 6430 | ``					/* A plain `$var` is read by its SEND in php, against the formal it`` |
|        - | 6431 | `					 * binds to: the ordinary list's deferred load. A by-reference` |
|        - | 6432 | `					 * constructor parameter creates it, a by-value one warns once, and a` |
|        - | 6433 | `					 * named argument refused at its send never reads it at all. */` |
|       32 | 6434 | `					iArgFlags \|= EXPR_FLAG_DEFER_ARG;` |
|       15 | 6435 | `				}` |
|      156 | 6436 | `				rc = GenStateCompileArrayEntry(pGen,pArgIn,pArgNext,iArgFlags,0);` |
|      156 | 6437 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 6438 | `					pGen->pIn = pSavedIn;` |
|      ! 0 | 6439 | `					pGen->pEnd = pSavedEnd;` |
|      ! 0 | 6440 | `					SySetRelease(&aArgName);` |
|      ! 0 | 6441 | `					return SXERR_ABORT;` |
|        - | 6442 | `				}` |
|      156 | 6443 | `				if( bSpread ){` |
|        - | 6444 | `					/* iP1 marks a source php unpacks BY REFERENCE: only a plain` |
|        - | 6445 | ``					 * `$var`, which is exactly two tokens here. */`` |
|       32 | 6446 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_SPREAD,` |
|       30 | 6447 | `						(&pArgIn[2] == pArgNext && (pArgIn->nType & PH7_TK_DOLLAR)` |
|       12 | 6448 | `						 && (pArgIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))) ? 1 : 0,` |
|        - | 6449 | `						0,0,0);` |
|       26 | 6450 | `					hasSpread = 1;` |
|      144 | 6451 | `				}else if( sArgName.nByte > 0 && nNamedSend < sizeof(aNamedSend)/sizeof(aNamedSend[0]) ){` |
|        - | 6452 | `					/* php resolves a NAME at the send of its argument, against the` |
|        - | 6453 | ``					 * constructor of the class DECLARE_ANON_CLASS already declared: `new`` |
|        - | 6454 | ``					 * class(zz: $u, a: s()) {…}` is `Unknown named parameter $zz` without`` |
|        - | 6455 | ``					 * reading `$u` or running `s()`. The ordinary list's screen, told the`` |
|        - | 6456 | `					 * class through the map rather than the stack. */` |
|       72 | 6457 | `					aNamedSend[nNamedSend++] = PH7_VmInstrLength(pGen->pVm);` |
|      106 | 6458 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_NAMED_SEND,nArg,` |
|       34 | 6459 | `						(hasSpread ? PH7_ROT_SPREAD : 0) \| PH7_ROT_NEW \| PH7_ROT_ANON,0,0);` |
|       34 | 6460 | `				}` |
|      156 | 6461 | `				SySetPut(&aArgName,(const void *)&sArgName);` |
|      156 | 6462 | `				nArg++;` |
|       76 | 6463 | `			}` |
|      156 | 6464 | `			pGen->pIn = &pArgNext[1];` |
|        4 | 6465 | `		}` |
|      108 | 6466 | `		pGen->pIn = pSavedIn;` |
|      108 | 6467 | `		pGen->pEnd = pSavedEnd;` |
|       52 | 6468 | `	}` |
|        - | 6469 | `	/* Load the synthesized class name */` |
|      289 | 6470 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      289 | 6471 | `	if( pObj == 0 ){` |
|      ! 0 | 6472 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 6473 | `		SySetRelease(&aArgName);` |
|      ! 0 | 6474 | `		return SXERR_ABORT;` |
|        - | 6475 | `	}` |
|      289 | 6476 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      289 | 6477 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - | 6478 | `	/* The names ride on the instruction as a VmCallArgMap, deep-copied out of the` |
|        - | 6479 | `	 * token stream (which is freed before the code runs) exactly as the ordinary` |
|        - | 6480 | `	 * call path copies them. */` |
|      289 | 6481 | `	p3 = 0;` |
|      289 | 6482 | `	if( hasNamed ){` |
|       45 | 6483 | `		SyString *aName = (SyString *)SySetBasePtr(&aArgName);` |
|       45 | 6484 | `		sxu32 nStrBytes = sName.nByte;` |
|      123 | 6485 | `		for( n = 0 ; n < (sxu32)nArg ; ++n ){` |
|       81 | 6486 | `			nStrBytes += aName[n].nByte;` |
|       42 | 6487 | `		}` |
|        - | 6488 | `		{` |
|       45 | 6489 | `		sxu32 mapSize = sizeof(VmCallArgMap) + (sxu32)nArg * sizeof(SyString) + nStrBytes;` |
|       45 | 6490 | `		VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,mapSize);` |
|       45 | 6491 | `		if( pMap ){` |
|        - | 6492 | `			char *zBuf;` |
|       45 | 6493 | `			SyZero(pMap,mapSize);` |
|       45 | 6494 | `			pMap->bHasNamed = 1;` |
|       45 | 6495 | `			pMap->nTotal = (sxu32)nArg;` |
|       45 | 6496 | `			pMap->aNames = (SyString *)&pMap[1];` |
|       45 | 6497 | `			zBuf = (char *)&pMap->aNames[nArg];` |
|      123 | 6498 | `			for( n = 0 ; n < (sxu32)nArg ; ++n ){` |
|       81 | 6499 | `				if( aName[n].nByte > 0 ){` |
|       69 | 6500 | `					SyMemcpy(aName[n].zString,zBuf,aName[n].nByte);` |
|       69 | 6501 | `					SyStringInitFromBuf(&pMap->aNames[n],zBuf,aName[n].nByte);` |
|       69 | 6502 | `					zBuf += aName[n].nByte;` |
|       33 | 6503 | `				}` |
|       42 | 6504 | `			}` |
|       45 | 6505 | `			SyMemcpy(sName.zString,zBuf,sName.nByte);` |
|       45 | 6506 | `			SyStringInitFromBuf(&pMap->sNewAnon,zBuf,sName.nByte);` |
|       45 | 6507 | `			p3 = (void *)pMap;` |
|      111 | 6508 | `			for( n = 0 ; n < nNamedSend ; ++n ){` |
|       69 | 6509 | `				VmInstr *pSend = PH7_VmGetInstr(pGen->pVm,aNamedSend[n]);` |
|       69 | 6510 | `				if( pSend ){` |
|       69 | 6511 | `					pSend->p3 = (void *)pMap;` |
|       33 | 6512 | `				}` |
|       36 | 6513 | `			}` |
|       21 | 6514 | `		}` |
|        - | 6515 | `		}` |
|       21 | 6516 | `	}` |
|      289 | 6517 | `	SySetRelease(&aArgName);` |
|        - | 6518 | `	/* Instantiate: pops the name + nArg arguments, runs __construct. iP2 is the` |
|        - | 6519 | `	 * spread flag the effective-argument-map builder keys on. */` |
|      431 | 6520 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,hasSpread ? 1 : 0,` |
|      142 | 6521 | `		GenStateAttachStrictFlag(pGen,p3),0);` |
|        - | 6522 | `	}` |
|      289 | 6523 | `	return SXRET_OK;` |
|      149 | 6524 | `}` |
|        - | 6525 | `/*` |
|        - | 6526 | ` * Compile a user-defined abstract class.` |
|        - | 6527 | ` *  According to the PHP language reference manual` |
|        - | 6528 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|        - | 6529 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|        - | 6530 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|        - | 6531 | ` *   the method's signature - they cannot define the implementation.` |
|        - | 6532 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|        - | 6533 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|        - | 6534 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|        - | 6535 | ` *   method is defined as protected, the function implementation must be defined as either` |
|        - | 6536 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|        - | 6537 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|        - | 6538 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|        - | 6539 | ` *   could differ.` |
|        - | 6540 | ` */` |
|        - | 6541 | `/*` |
|        - | 6542 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|        - | 6543 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|        - | 6544 | ` * receives the corresponding PH7_CLASS_* bit.` |
|        - | 6545 | ` */` |
|  2582032 | 6546 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|        5 | 6547 | `{` |
|  2582037 | 6548 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|  1516381 | 6549 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|  1516381 | 6550 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|  1516253 | 6551 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|   756935 | 6552 | `	}` |
|  2581545 | 6553 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  2581459 | 6554 | `	return FALSE;` |
|  1289105 | 6555 | `}` |
|        - | 6556 | `/*` |
|        - | 6557 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|        - | 6558 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|        - | 6559 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|        - | 6560 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|        - | 6561 | ` */` |
|  2581454 | 6562 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|        5 | 6563 | `{` |
|  2581459 | 6564 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  2581459 | 6565 | `	sxi32 iFlags = 0,iFlag;` |
|  2582037 | 6566 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|      583 | 6567 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|        5 | 6568 | `			pDup = pIn;` |
|        2 | 6569 | `		}` |
|      583 | 6570 | `		iFlags \|= iFlag;` |
|      583 | 6571 | `		pIn++;` |
|        5 | 6572 | `	}` |
|  2581459 | 6573 | `	*ppIn = pIn;` |
|  2581459 | 6574 | `	if( ppDup ){ *ppDup = pDup; }` |
|  2581459 | 6575 | `	return iFlags;` |
|        5 | 6576 | `}` |
|        - | 6577 | `/*` |
|        - | 6578 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|        - | 6579 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|        - | 6580 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|        - | 6581 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|        - | 6582 | `` * `readonly`) to their existing handlers.`` |
|        - | 6583 | ` */` |
|  2581188 | 6584 | `PH7_PRIVATE int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|        5 | 6585 | `{` |
|  2581193 | 6586 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|  1288965 | 6587 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  2581330 | 6588 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|        5 | 6589 | `}` |
|        - | 6590 | `/*` |
|        - | 6591 | ``  * Return TRUE when the token stream starts `readonly class …` in a `new` `` |
|        - | 6592 | `` * operand — PHP 8.3's readonly ANONYMOUS class. `readonly` is the only modifier`` |
|        - | 6593 | `` * php admits there: `new final class {}` and `new abstract class {}` are parse`` |
|        - | 6594 | ` * errors, so this deliberately does NOT reuse GenStateScanClassModifiers.` |
|        - | 6595 | ` */` |
|  7115800 | 6596 | `PH7_PRIVATE int GenStateStartsReadonlyAnonClass(SyToken *pIn,SyToken *pEnd)` |
|        5 | 6597 | `{` |
|  7115805 | 6598 | `	if( !GenStateIsReadonly(pIn) \|\| &pIn[1] >= pEnd ){` |
|  7115795 | 6599 | `		return 0;` |
|        - | 6600 | `	}` |
|       12 | 6601 | `	pIn++;` |
|       10 | 6602 | `	if( (pIn->nType & PH7_TK_KEYWORD) == 0` |
|        8 | 6603 | `	 \|\| (sxu32)SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_CLASS` |
|        8 | 6604 | `	 \|\| &pIn[1] >= pEnd ){` |
|        6 | 6605 | `		return 0;` |
|        - | 6606 | `	}` |
|        - | 6607 | `` 	/* Same shape test the bare `new class` branch makes, so a stray `readonly` `` |
|        - | 6608 | ``	 * followed by the `::class` constant is left to the literal path. */`` |
|        7 | 6609 | `	pIn++;` |
|        7 | 6610 | `	return (pIn->nType & (PH7_TK_OCB\|PH7_TK_LPAREN)) != 0` |
|        6 | 6611 | `		\|\| ( (pIn->nType & PH7_TK_KEYWORD)` |
|      ! 0 | 6612 | `		  && ( (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_EXTENDS` |
|      ! 0 | 6613 | `		    \|\| (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ) );` |
|  3551610 | 6614 | `}` |
|        - | 6615 | `/*` |
|        - | 6616 | ` * Compile a class declaration carrying one or more leading modifiers` |
|        - | 6617 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|        - | 6618 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|        - | 6619 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|        - | 6620 | `` * `abstract`+`final` pair, like PHP.`` |
|        - | 6621 | ` */` |
|      266 | 6622 | `PH7_PRIVATE sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|        5 | 6623 | `{` |
|        - | 6624 | `	SyToken *pDup;` |
|      271 | 6625 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|        - | 6626 | `	sxi32 rc;` |
|      271 | 6627 | `	if( pDup ){` |
|        4 | 6628 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|        2 | 6629 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|        3 | 6630 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 6631 | `			return SXERR_ABORT;` |
|        - | 6632 | `		}` |
|        1 | 6633 | `	}` |
|      266 | 6634 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|      138 | 6635 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|        6 | 6636 | `		pGen->iFatalTrace = PH7_FATAL_TRACE_NONE;` |
|        6 | 6637 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 6638 | `			"Cannot use the final modifier on an abstract class");` |
|        6 | 6639 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 6640 | `			return SXERR_ABORT;` |
|        - | 6641 | `		}` |
|        2 | 6642 | `	}` |
|      271 | 6643 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|      138 | 6644 | `}` |
|        - | 6645 | `/*` |
|        - | 6646 | ` * Compile a user-defined trait.` |
|        - | 6647 | ` *  Traits are similar to classes, but only intended to group functionality` |
|        - | 6648 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|        - | 6649 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|        - | 6650 | ` */` |
|      450 | 6651 | `PH7_PRIVATE sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|        5 | 6652 | `{` |
|      455 | 6653 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 6654 | `	ph7_class *pClass;` |
|      455 | 6655 | `	ph7_class *pSavedCurClass = pGen->pCurClass; /* restored at 'done' */` |
|      455 | 6656 | `	GenBlock *pSavedCurClassBlock = pGen->pCurClassBlock;` |
|      455 | 6657 | `	ph7_class *pSavedCurBase = pGen->pCurBase;   /* ...and its base, see pCurBase */` |
|        - | 6658 | `	SyToken *pEnd,*pTmp;` |
|        - | 6659 | ``	SySet aUseEntries; /* trait-body `use` statements (incl. adaptation blocks) */`` |
|        - | 6660 | `	SyString *pName;` |
|        - | 6661 | `	sxi32 nKwrd;` |
|        - | 6662 | `	sxi32 rc;` |
|        - | 6663 | `	{` |
|        - | 6664 | `		/* Deferral gate: a used trait may need an autoloader that` |
|        - | 6665 | `		 * has not run yet. */` |
|        - | 6666 | `		sxi32 rcDefer;` |
|      455 | 6667 | `		if( GenStateMaybeDeferClass(pGen,0,PH7_DEFER_KIND_TRAIT,&rcDefer) ){` |
|       20 | 6668 | `			return rcDefer == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 6669 | `		}` |
|        - | 6670 | `	}` |
|      439 | 6671 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|        - | 6672 | `	/* Jump the 'trait' keyword */` |
|      439 | 6673 | `	pGen->pIn++;` |
|      439 | 6674 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 | 6675 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|      ! 0 | 6676 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 6677 | `			return SXERR_ABORT;` |
|        - | 6678 | `		}` |
|      ! 0 | 6679 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|      ! 0 | 6680 | `			pGen->pIn++;` |
|      ! 0 | 6681 | `		}` |
|      ! 0 | 6682 | `		return SXRET_OK;` |
|        - | 6683 | `	}` |
|        - | 6684 | `	/* Extract trait name */` |
|      439 | 6685 | `	pName = &pGen->pIn->sData;` |
|      439 | 6686 | `	pGen->pIn++;` |
|        - | 6687 | `	/* Build FQN and obtain a raw class */ {` |
|        - | 6688 | `		SyBlob sFQN;` |
|        - | 6689 | `		SyString sFQNStr;` |
|      439 | 6690 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      439 | 6691 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      439 | 6692 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|        - | 6693 | ``		/* php refuses a declaration whose short name a local `use` already took. */`` |
|      439 | 6694 | `		if( GenStateGuardImportRedeclare(pGen,0,pName,&sFQNStr,nLine) == SXERR_ABORT ){` |
|      ! 0 | 6695 | `			SyBlobRelease(&sFQN);` |
|      ! 0 | 6696 | `			return SXERR_ABORT;` |
|        - | 6697 | `		}` |
|      439 | 6698 | `		GenStateRecordDeclaredName(pGen,0,&sFQNStr);` |
|      439 | 6699 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      439 | 6700 | `		SyBlobRelease(&sFQN);` |
|        - | 6701 | `	}` |
|      439 | 6702 | `	if( pClass == 0 ){` |
|      ! 0 | 6703 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 6704 | `		return SXERR_ABORT;` |
|        - | 6705 | `	}` |
|      439 | 6706 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|      439 | 6707 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 6708 | `		return SXERR_ABORT;` |
|        - | 6709 | `	}` |
|        - | 6710 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      439 | 6711 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|      ! 0 | 6712 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|      ! 0 | 6713 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 6714 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 6715 | `			return SXERR_ABORT;` |
|        - | 6716 | `		}` |
|      ! 0 | 6717 | `		return SXRET_OK;` |
|        - | 6718 | `	}` |
|      439 | 6719 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      439 | 6720 | `	pEnd = 0;` |
|      439 | 6721 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      439 | 6722 | `	if( pEnd >= pGen->pEnd ){` |
|      ! 0 | 6723 | `		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|      ! 0 | 6724 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 6725 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 6726 | `			return SXERR_ABORT;` |
|        - | 6727 | `		}` |
|      ! 0 | 6728 | `		return SXRET_OK;` |
|        - | 6729 | `	}` |
|        - | 6730 | `	/* The delimiter token is the trait body's closing brace */` |
|      439 | 6731 | `	pClass->nEndLine = pEnd->nLine;` |
|        - | 6732 | `	/* Swap token stream */` |
|      439 | 6733 | `	pTmp = pGen->pEnd;` |
|      439 | 6734 | `	pGen->pEnd = pEnd;` |
|        - | 6735 | `	/* Mark as trait (PH7_NewRawClass may have set INTERNAL) */` |
|      439 | 6736 | `	pClass->iFlags \|= PH7_CLASS_TRAIT;` |
|      651 | 6737 | `	if( GenStateCheckAttrPlacement(&(*pGen),&pClass->aAttrs,pClass->nLine,1,1,` |
|      656 | 6738 | `			&pClass->sName,pClass->iFlags) == SXERR_ABORT ){` |
|      ! 0 | 6739 | `		return SXERR_ABORT;` |
|        - | 6740 | `	}` |
|        - | 6741 | `	/* This trait is now the lexical class for its body, so a property/parameter` |
|        - | 6742 | `	 * default here resolves __TRAIT__ to it (see pCurClass). */` |
|      439 | 6743 | `	pGen->pCurClass = pClass;` |
|      439 | 6744 | `	pGen->pCurClassBlock = pGen->pCurrent;` |
|      439 | 6745 | ``	pGen->pCurBase = 0; /* a trait has no base; its `parent` is deferred to composition */`` |
|        - | 6746 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      524 | 6747 | `	for(;;){` |
|     1339 | 6748 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      253 | 6749 | `			pGen->pIn++;` |
|        5 | 6750 | `		}` |
|     1091 | 6751 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      437 | 6752 | `			break;` |
|        - | 6753 | `		}` |
|        - | 6754 | `		/* Bind a directly-preceding docblock to this member */` |
|      659 | 6755 | `		GenStateSetPendingDoc(&(*pGen));` |
|      654 | 6756 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|      332 | 6757 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|      ! 0 | 6758 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|        - | 6759 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|      ! 0 | 6760 | `				&pGen->pIn->sData,pName);` |
|      ! 0 | 6761 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 6762 | `				return SXERR_ABORT;` |
|        - | 6763 | `			}` |
|      ! 0 | 6764 | `			goto done;` |
|        - | 6765 | `		}` |
|      659 | 6766 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      659 | 6767 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      659 | 6768 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|        - | 6769 | `				/* Trait uses another trait: use T[, T2] [{ resolution }]; A trait` |
|        - | 6770 | `				 * name is a full class reference — qualified or fully-qualified` |
|        - | 6771 | ``				 * (`use Foo\T;`, `use \Foo\T;`) — so parse it with the shared`` |
|        - | 6772 | `				 * class-reference reader like the CLASS body's trait-use does` |
|        - | 6773 | `				 * (the old single-identifier read choked on the leading '\'),` |
|        - | 6774 | `				 * and collect a TraitUseEntry so an adaptation block` |
|        - | 6775 | `				 * (insteadof/as) applies through the same shared machinery. */` |
|        - | 6776 | `				TraitUseEntry sUse;` |
|       42 | 6777 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|       42 | 6778 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|       42 | 6779 | `				pGen->pIn++; /* Jump 'use' */` |
|       25 | 6780 | `				for(;;){` |
|        - | 6781 | `					ph7_class *pUsedTrait;` |
|        - | 6782 | `					SyBlob sResolved;` |
|        - | 6783 | `					SyString sUsedName;` |
|       48 | 6784 | `					sxu32 nUseLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|       48 | 6785 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       48 | 6786 | `					if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 6787 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 6788 | `						rc = PH7_GenCompileError(pGen,E_PARSE,nUseLine,` |
|      ! 0 | 6789 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|      ! 0 | 6790 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 6791 | `							return SXERR_ABORT;` |
|        - | 6792 | `						}` |
|      ! 0 | 6793 | `						break;` |
|        - | 6794 | `					}` |
|       48 | 6795 | `					pUsedTrait = GenStateLinkClass(pGen,&sResolved);` |
|       48 | 6796 | `					SyStringInitFromBuf(&sUsedName,` |
|        - | 6797 | `						(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       48 | 6798 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|      ! 0 | 6799 | `						pUsedTrait = pUsedTrait->pNextName;` |
|      ! 0 | 6800 | `					}` |
|       48 | 6801 | `					if( pUsedTrait == 0 ){` |
|       11 | 6802 | `						if( !GenStateLintSkipsBase(pGen,pClass) ){` |
|      ! 0 | 6803 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|        - | 6804 | `								"'%z' is not a trait",&sUsedName);` |
|      ! 0 | 6805 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 6806 | `								SyBlobRelease(&sResolved);` |
|      ! 0 | 6807 | `								return SXERR_ABORT;` |
|        - | 6808 | `							}` |
|      ! 0 | 6809 | `						}` |
|        7 | 6810 | `					}else{` |
|       40 | 6811 | `						SySetPut(&sUse.aTraits,(const void *)&pUsedTrait);` |
|        - | 6812 | `					}` |
|       48 | 6813 | `					SyBlobRelease(&sResolved);` |
|       48 | 6814 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       23 | 6815 | `						break;` |
|        - | 6816 | `					}` |
|        8 | 6817 | `					pGen->pIn++;` |
|        2 | 6818 | `				}` |
|        - | 6819 | `				/* Optional adaptation block (conflict resolution) */` |
|       42 | 6820 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|        - | 6821 | `					SyToken *pBlock;` |
|       10 | 6822 | `					pGen->pIn++; /* Jump '{' */` |
|       10 | 6823 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       10 | 6824 | `					sUse.pResolvStart = pGen->pIn;` |
|       10 | 6825 | `					sUse.pResolvEnd = pBlock;` |
|       10 | 6826 | `					if( pBlock < pGen->pEnd ){` |
|       10 | 6827 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|        6 | 6828 | `					}else{` |
|      ! 0 | 6829 | `						pGen->pIn = pGen->pEnd;` |
|        - | 6830 | `					}` |
|        4 | 6831 | `				}` |
|       42 | 6832 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       42 | 6833 | `				continue;` |
|        - | 6834 | `			}` |
|      308 | 6835 | `		}` |
|        - | 6836 | `		/* Everything else is a MEMBER: its modifier run and the declaration it` |
|        - | 6837 | `		 * modifies, read by the same code a class body uses. */` |
|      621 | 6838 | `		rc = GenStateCompileMember(&(*pGen),pClass,"trait");` |
|      621 | 6839 | `		if( rc != SXRET_OK ){` |
|        3 | 6840 | `			if( rc == SXERR_ABORT ){` |
|        3 | 6841 | `				return SXERR_ABORT;` |
|        - | 6842 | `			}` |
|      ! 0 | 6843 | `			goto done;` |
|        - | 6844 | `		}` |
|        5 | 6845 | `	}` |
|        - | 6846 | ``	/* Apply the collected `use` entries (incl. adaptation blocks) through the`` |
|        - | 6847 | `	 * machinery shared with the class-body compiler. */` |
|      437 | 6848 | `	rc = GenStateApplyTraitUses(&(*pGen),pClass,&aUseEntries);` |
|      437 | 6849 | `	SySetRelease(&aUseEntries);` |
|      437 | 6850 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 6851 | `		return SXERR_ABORT;` |
|        - | 6852 | `	}` |
|        - | 6853 | `	/* Reject a php-fatal redeclaration before hoisting the trait. php early-binds` |
|        - | 6854 | `	 * a trait like a plain class -- it has nothing left to link. */` |
|      437 | 6855 | `	if( GenStateGuardClassRedeclaration(pGen,pClass,1) == SXERR_ABORT ){` |
|        3 | 6856 | `		return SXERR_ABORT;` |
|        - | 6857 | `	}` |
|        - | 6858 | `	/* Install the trait */` |
|      435 | 6859 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      435 | 6860 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 6861 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 6862 | `		return SXERR_ABORT;` |
|        - | 6863 | `	}` |
|      215 | 6864 | `done:` |
|      435 | 6865 | `	pGen->pCurClass = pSavedCurClass;` |
|      435 | 6866 | `	pGen->pCurBase = pSavedCurBase;` |
|      435 | 6867 | `	pGen->pCurClassBlock = pSavedCurClassBlock;` |
|        - | 6868 | `	/* Point beyond the trait body */` |
|      435 | 6869 | `	pGen->pIn = &pEnd[1];` |
|      435 | 6870 | `	pGen->pEnd = pTmp;` |
|      435 | 6871 | `	return PH7_OK;` |
|      230 | 6872 | `}` |
|        - | 6873 | `/*` |
|        - | 6874 | ` * Compile a user-defined class.` |
|        - | 6875 | ` *  According to the PHP language reference manual` |
|        - | 6876 | ` *   Basic class definitions begin with the keyword class, followed` |
|        - | 6877 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|        - | 6878 | ` *   the definitions of the properties and methods belonging to the class.` |
|        - | 6879 | ` *   A class may contain its own constants, variables (called "properties")` |
|        - | 6880 | ` *   and functions (called "methods").` |
|        - | 6881 | ` */` |
|     5826 | 6882 | `PH7_PRIVATE sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|        5 | 6883 | `{` |
|        - | 6884 | `	sxi32 rc;` |
|     5831 | 6885 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|     5831 | 6886 | `	return rc;` |
|        5 | 6887 | `}` |
|        - | 6888 | `/*` |
|        - | 6889 | ` * Return TRUE if the token stream starts an enum declaration (PHP 8.1):` |
|        - | 6890 | `` * the context-sensitive identifier `enum` (not a reserved word — it stays`` |
|        - | 6891 | `` * valid as a function/constant name, like `readonly`) directly followed by`` |
|        - | 6892 | `` * an identifier. `enum(...)`/`enum;`/`$enum` all keep their expression`` |
|        - | 6893 | `` * meaning; `enum Name` can never start a valid expression.`` |
|        - | 6894 | ` */` |
|  2580908 | 6895 | `PH7_PRIVATE int GenStateStartsEnumDecl(SyToken *pIn,SyToken *pEnd)` |
|        5 | 6896 | `{` |
|  2661453 | 6897 | `	return (pIn->nType & PH7_TK_ID)` |
|  1369663 | 6898 | `		&& pIn->sData.nByte == sizeof("enum")-1` |
|    98622 | 6899 | `		&& SyStrnicmp(pIn->sData.zString,"enum",sizeof("enum")-1) == 0` |
|  2662033 | 6900 | `		&& &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_ID);` |
|        5 | 6901 | `}` |
|        - | 6902 | `/*` |
|        - | 6903 | ` * Return TRUE when the token stream starts a CLOSURE EXPRESSION at statement` |
|        - | 6904 | `` * position: `function () {…};` or `fn (…) => …;`, with an optional `&` for a`` |
|        - | 6905 | ` * by-reference return.` |
|        - | 6906 | ` *` |
|        - | 6907 | ` * php compiles both as ordinary expression statements — the closure is built and` |
|        - | 6908 | ` * discarded — and 176 files across the vendor trees write one. PHL sent the bare` |
|        - | 6909 | `` * `function` keyword to PH7_CompileFunction, which demands a NAME and answered`` |
|        - | 6910 | `` * `syntax error, unexpected token "(", expecting "("`; `fn` was not a statement`` |
|        - | 6911 | `` * keyword at all and answered `Unexpected keyword 'fn'`. The `static` forms`` |
|        - | 6912 | `` * (`static function () {};`, `static fn () => 1;`) already worked, which is what`` |
|        - | 6913 | ` * made this look narrower than it was.` |
|        - | 6914 | ` *` |
|        - | 6915 | `` * The NAMED declaration is what must not be caught here, and the `(` is what`` |
|        - | 6916 | `` * tells them apart: `function foo(` has an identifier where a closure has its`` |
|        - | 6917 | `` * parameter list. php refuses an immediately-invoked `function () {}()` at`` |
|        - | 6918 | ` * statement position too, so nothing here tries to accept one.` |
|        - | 6919 | ` */` |
|  2580354 | 6920 | `PH7_PRIVATE int GenStateStartsClosureExpr(SyToken *pIn,SyToken *pEnd)` |
|        5 | 6921 | `{` |
|  2580359 | 6922 | `	if( (pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|  1065411 | 6923 | `		return 0;` |
|        - | 6924 | `	}` |
|        - | 6925 | `	{` |
|  1514953 | 6926 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|  1514953 | 6927 | `		if( nKw != PH7_TKWRD_FUNCTION && nKw != PH7_TKWRD_FN ){` |
|  1332592 | 6928 | `			return 0;` |
|        - | 6929 | `		}` |
|        - | 6930 | `	}` |
|   182366 | 6931 | `	pIn++;` |
|   182366 | 6932 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_AMPER) ){` |
|       43 | 6933 | ``		pIn++;   /* `function &() {…}` returns by reference */`` |
|       19 | 6934 | `	}` |
|   182366 | 6935 | `	return pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) != 0;` |
|  1288266 | 6936 | `}` |
|        - | 6937 | `/*` |
|        - | 6938 | ` * Compile an enum declaration (PHP 8.1). An enum is a final class carrying` |
|        - | 6939 | `` * PH7_CLASS_ENUM: `case` members become lazily-materialized singleton`` |
|        - | 6940 | ` * constants, cases()/from()/tryFrom() are synthesized, and UnitEnum/BackedEnum` |
|        - | 6941 | ` * are implemented implicitly (GenStateCompileClassEx handles the specifics).` |
|        - | 6942 | ` */` |
|      152 | 6943 | `PH7_PRIVATE sxi32 PH7_CompileEnum(ph7_gen_state *pGen)` |
|        5 | 6944 | `{` |
|      157 | 6945 | `	return GenStateCompileClass(&(*pGen),PH7_CLASS_ENUM\|PH7_CLASS_FINAL);` |
|        5 | 6946 | `}` |
|        - | 6947 |  |
