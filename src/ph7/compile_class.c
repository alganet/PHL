/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#include "compile_int.h"
/* Forward declaration — deferred class declarations (defined with the deferral
 * helpers ahead of GenStateCompileClassEx; used by the interface/trait
 * compilers that precede them in this file). */
static int GenStateMaybeDeferClass(ph7_gen_state *pGen,sxi32 iFlags,int iSelfKind,sxi32 *pRc);
/*
 * Section:
 *    Class/OO compilation: classes, interfaces, traits, enums, anonymous
 *    classes, class constants, typed properties, property hooks and methods.
 * Status:
 *    Stable.
 */
static const char * GenStateClassKind(const ph7_class *pClass)
{
	if( pClass->iFlags & PH7_CLASS_INTERFACE ){ return "interface"; }
	if( pClass->iFlags & PH7_CLASS_TRAIT ){ return "trait"; }
	if( pClass->iFlags & PH7_CLASS_ENUM ){ return "enum"; }
	return "class";
}
/*
 * Guard a class/interface/trait/enum about to be installed. Returns SXERR_ABORT
 * (after emitting the fatal) if it redeclares an already-bound type; otherwise
 * marks it bound (when unconditional & top-level) and returns SXRET_OK.
 */
/*
 * Does this earlier declaration hold the name against the one being compiled?
 *
 * A class php EARLY-BINDS holds it from the moment the file compiles, so
 * anything else under that name is a redeclaration wherever it sits. One php
 * declares at RUN time (it implements an interface, uses a trait, is an enum,
 * or declares __toString and so implicitly implements Stringable) holds it only
 * once its own statement has run -- which, within one file, means only against
 * a declaration that comes AFTER it. That is the whole of the "polyfill" shape:
 *
 *     if (PHP_VERSION_ID >= 80000) { class T extends PhpToken {} return; }
 *     class T { public function __toString(): string { ... } }
 *
 * php runs the first branch, returns, and never reaches the second -- so the
 * second never takes the name. Reading the two declarations' ORDER is how this
 * compiler tells that apart without a runtime declaration of its own.
 */
static int GenStateDeclHoldsName(ph7_gen_state *pGen,ph7_class *pPrev,ph7_class *pClass)
{
	SyString *pFile;
	if( pPrev->iFlags & PH7_CLASS_BOUND ){
		return 1;   /* early-bound: the name is taken before anything runs */
	}
	if( (pPrev->iFlags & PH7_CLASS_TOPLEVEL) == 0 ){
		return 0;   /* conditional: it may never run at all */
	}
	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);
	if( pFile && pPrev->sFile.nByte == pFile->nByte
	 && SyMemcmp(pPrev->sFile.zString,pFile->zString,pFile->nByte) == 0 ){
		/* Same file: it holds the name only if it is written FIRST. */
		return pPrev->nLine <= pClass->nLine;
	}
	return 1;   /* another file, already loaded: it has run */
}
static sxi32 GenStateGuardClassRedeclaration(ph7_gen_state *pGen,ph7_class *pClass,
	int bEarlyBindable)
{
	SyHashEntry *pEntry;
	int bTopLevel = GenStateUnconditionalTopLevel(pGen);
	if( bTopLevel ){
		/* php RUNS this declaration whatever else the file holds, early bound or
		 * not -- so two of them under one name collide even when neither was. */
		pClass->iFlags |= PH7_CLASS_TOPLEVEL;
	}
	if( bTopLevel && !bEarlyBindable ){
		/* Not early-bound, but it RUNS: only another declaration that also runs
		 * unconditionally collides with it. */
		SyHashEntry *pTop = SyHashGet(&pGen->pVm->hClass,
			(const void *)pClass->sName.zString,pClass->sName.nByte);
		if( pTop ){
			ph7_class *pPrev = (ph7_class *)pTop->pUserData;
			while( pPrev ){
				if( GenStateDeclHoldsName(pGen,pPrev,pClass) ){
					pGen->iFatalTrace = PH7_FATAL_TRACE_RUNTIME;
					if( pPrev->sFile.nByte > 0 ){
						PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,
							"Cannot redeclare %s %z (previously declared in %.*s:%u)",
							GenStateClassKind(pPrev),&pClass->sName,
							pPrev->sFile.nByte,pPrev->sFile.zString,pPrev->nLine);
					}else{
						PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,
							"Cannot redeclare %s %z",GenStateClassKind(pPrev),&pClass->sName);
					}
					return SXERR_ABORT;
				}
				pPrev = pPrev->pNextName;
			}
		}
		return SXRET_OK;
	}
	if( !bTopLevel ){
		/* Conditional, nested -- or one php would not EARLY-BIND either, which
		 * is the same thing for this guard: such a declaration only takes effect
		 * when its statement runs, so another declaration of the name is not a
		 * redeclaration of it. php early-binds only a class it can link with
		 * nothing left over, so an implemented INTERFACE, a used TRAIT, an enum
		 * or a `__toString()` (which brings Stringable with it) all rule it out.
		 * Binding one of those at compile time made the polyfill shape above a
		 * redeclaration, and phpunit.phar died on it. */
		return SXRET_OK;
	}
	pClass->iFlags |= PH7_CLASS_BOUND;
	if( pGen->pVm->bCompilingBuiltin ){
		return SXRET_OK; /* the prelude installs each builtin exactly once */
	}
	pEntry = SyHashGet(&pGen->pVm->hClass,(const void *)pClass->sName.zString,pClass->sName.nByte);
	if( pEntry ){
		ph7_class *pPrev = (ph7_class *)pEntry->pUserData;
		while( pPrev ){
			if( GenStateDeclHoldsName(pGen,pPrev,pClass) ){
				/* php cannot early-bind a name it already holds, so THIS refusal comes
				 * from the DECLARE_CLASS opcode at run time -- and its stack trace
				 * carries the include/require that loaded the unit, where every other
				 * compile-time refusal's does not. */
				pGen->iFatalTrace = PH7_FATAL_TRACE_RUNTIME;
				/* php names the entity by the PREVIOUS declaration's kind and omits
				 * the "(previously declared in ...)" clause for internal symbols. */
				if( pPrev->sFile.nByte > 0 ){
					PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,
						"Cannot redeclare %s %z (previously declared in %.*s:%u)",
						GenStateClassKind(pPrev),&pClass->sName,
						pPrev->sFile.nByte,pPrev->sFile.zString,pPrev->nLine);
				}else{
					PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,
						"Cannot redeclare %s %z",GenStateClassKind(pPrev),&pClass->sName);
				}
				return SXERR_ABORT;
			}
			pPrev = pPrev->pNextName;
		}
	}
	return SXRET_OK;
}
/*
 * Extract the visibility level associated with a given keyword.
 * According to the PHP language reference manual
 *  Visibility:
 *  The visibility of a property or method can be defined by prefixing
 *  the declaration with the keywords public, protected or private.
 *  Class members declared public can be accessed everywhere.
 *  Members declared protected can be accessed only within the class
 *  itself and by inherited and parent classes. Members declared as private
 *  may only be accessed by the class that defines the member.
 */
static sxi32 GetProtectionLevel(sxi32 nKeyword)
{
	if( nKeyword == PH7_TKWRD_PRIVATE ){
		return PH7_CLASS_PROT_PRIVATE;
	}else if( nKeyword == PH7_TKWRD_PROTECTED ){
		return PH7_CLASS_PROT_PROTECTED;
	}
	/* Assume public by default */
	return PH7_CLASS_PROT_PUBLIC;
}
/*
 * Decide whether a typed class constant (PHP 8.3) declares a type before its
 * name. The classic untyped form is `const NAME = value` — a single name-like
 * token immediately followed by '='. Anything else with a leading type token
 * (`const int X`, `const ?int X`, `const A|B X`, `const \Ns\Foo X`) declares a
 * type. We only commit to the type-parse when the shape is unambiguous so the
 * untyped path never runs (and never trips the type parser's diagnostics).
 */
static int GenStateClassConstHasType(ph7_gen_state *pGen)
{
	SyToken *p0, *p1;
	if( pGen->pIn >= pGen->pEnd ){
		return 0;
	}
	p0 = pGen->pIn;
	/* A leading '\' (namespaced class type), '?' (nullable) or '(' (a DNF type's
	 * first intersection group) always starts a type -- none of the three can begin
	 * a constant NAME. */
	if( p0->nType & (PH7_TK_NSSEP|PH7_TK_LPAREN) ){
		return 1;
	}
	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){
		return 1;
	}
	/* A name-like first token begins a type only when followed by another
	 * name (the constant name), a union separator '|' or an intersection '&'.
	 * Followed by '=', ';' or ',' it is the constant name itself (untyped).
	 * Without the '&' a typed constant whose type is an INTERSECTION was read as
	 * an untyped one named after the first member, and refused with
	 * "Expected '=' after class constant Countable". */
	if( p0->nType & (PH7_TK_ID|PH7_TK_KEYWORD) ){
		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;
		if( p1 ){
			if( p1->nType & (PH7_TK_ID|PH7_TK_KEYWORD|PH7_TK_NSSEP|PH7_TK_AMPER) ){
				return 1;
			}
			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1
			 && (p1->sData.zString[0] == '|' || p1->sData.zString[0] == '&') ){
				return 1;
			}
		}
	}
	return 0;
}
/*
 * TRUE when the class-constant initializer starting at pGen->pIn is a bare real
 * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).
 * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a
 * whole-valued real MEMOBJ_REAL|MEMOBJ_INT, so the runtime flag test would wrongly
 * accept it as an int. The literal shape is the only reliable signal that separates
 * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).
 * Peek only; never consumes tokens.
 */
static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)
{
	SyToken *p = pGen->pIn;
	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1
		&& (p->sData.zString[0] == '-' || p->sData.zString[0] == '+') ){
		p++; /* skip leading unary sign(s) */
	}
	if( p >= pGen->pEnd || (p->nType & PH7_TK_REAL) == 0 ){
		return 0; /* not a real literal (int literal, cast, call, ...) */
	}
	p++;
	/* Must be the WHOLE initializer: the next token ends this constant. */
	return ( p >= pGen->pEnd || (p->nType & (PH7_TK_SEMI|PH7_TK_COMMA)) ) ? 1 : 0;
}
/*
 * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).
 * A `new` that immediately follows one of these is a member name (`A::new`,
 * `$o->new`), not a `new` expression.
 */
static int GenStateTokenIsMemberOp(const SyToken *p)
{
	sxi32 iOp;
	if( (p->nType & PH7_TK_OP) == 0 || p->pUserData == 0 ){
		return 0;
	}
	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;
	return ( iOp == EXPR_OP_DC || iOp == EXPR_OP_ARROW || iOp == EXPR_OP_NULLSAFE_ARROW );
}
/*
 * Skip the whole closure / arrow-function construct beginning at *pp, which is
 * positioned on the `function` / `fn` keyword. Its body is ordinary runtime code,
 * so none of the constant-expression rules below reach into it. *piDepth is the
 * caller's bracket depth: an arrow function has no braces of its own and ends at
 * a `,`/`;` or at a bracket closing an ENCLOSING group, so it shares that depth,
 * while a `function(){...}` body is brace-balanced and walks its own.
 */
static void GenStateInitSkipFuncConstruct(SyToken **pp,SyToken *pEnd,int *piDepth)
{
	SyToken *p = *pp;
	int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );
	int iBase = *piDepth;
	p++;
	if( bArrow ){
		/* fn(params) => expr : skip to the end of the current element. */
		while( p < pEnd ){
			if( p->nType & (PH7_TK_LPAREN|PH7_TK_OSB|PH7_TK_OCB) ){
				(*piDepth)++;
			}else if( p->nType & (PH7_TK_RPAREN|PH7_TK_CSB|PH7_TK_CCB) ){
				if( *piDepth <= iBase ){
					break; /* closes an enclosing group, not the fn's own */
				}
				(*piDepth)--;
			}else if( *piDepth <= iBase && (p->nType & (PH7_TK_SEMI|PH7_TK_COMMA)) ){
				break;
			}
			p++;
		}
	}else{
		/* function(params)[use(...)][: type] { body } : skip the signature up to the
		 * body '{' (a '{' at closure-local depth 0, so a `new class{}` default inside
		 * the parens is not mistaken for it), then the balanced brace block. */
		int iLocal = 0;
		while( p < pEnd ){
			if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){
				break; /* body brace */
			}
			if( p->nType & (PH7_TK_LPAREN|PH7_TK_OSB|PH7_TK_OCB) ){
				iLocal++;
			}else if( p->nType & (PH7_TK_RPAREN|PH7_TK_CSB|PH7_TK_CCB) ){
				if( iLocal > 0 ){
					iLocal--;
				}
			}
			p++;
		}
		if( p < pEnd ){
			int iBrace = 0; /* p is on the body '{' */
			while( p < pEnd ){
				if( p->nType & PH7_TK_OCB ){
					iBrace++;
				}else if( p->nType & PH7_TK_CCB ){
					iBrace--;
					if( iBrace == 0 ){
						p++;
						break;
					}
				}
				p++;
			}
		}
	}
	*pp = p;
}
/*
 * TRUE if *p is the `::` operator.
 */
static int GenStateTokenIsDoubleColon(const SyToken *p)
{
	return ( (p->nType & PH7_TK_OP) && p->pUserData
		&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_DC );
}
/*
 * Where the constant expression starting at *pStart ends: the first `,` or `;` at
 * bracket depth 0, or the first depth-0 `{`, which in a property declaration can
 * only open a PHP 8.4 hook list (`public T $x = default { get …; }`) and never
 * belongs to the default itself.
 */
static SyToken * GenStateConstExprEnd(SyToken *pStart,SyToken *pEnd)
{
	SyToken *p = pStart;
	int iDepth = 0;
	while( p < pEnd ){
		if( (p->nType & PH7_TK_KEYWORD) && (p->nType & PH7_TK_MEMBER_NAME) == 0
			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION
				|| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){
			/* A closure's BODY brace is not a hook list: skip the construct whole,
			 * or `$c = static function(){}` would end the expression at that brace
			 * and hide everything after it from the rules. */
			GenStateInitSkipFuncConstruct(&p,pEnd,&iDepth);
			continue;
		}
		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI|PH7_TK_COMMA|PH7_TK_OCB)) ){
			break;
		}
		if( p->nType & (PH7_TK_LPAREN|PH7_TK_OSB|PH7_TK_OCB) ){
			iDepth++;
		}else if( p->nType & (PH7_TK_RPAREN|PH7_TK_CSB|PH7_TK_CCB) ){
			if( iDepth > 0 ){
				iDepth--;
			}
		}
		p++;
	}
	return p;
}
/*
 * Decide a ternary CONDITION php would have folded: 1 truthy, 0 falsy, -1 "cannot
 * tell". Only a lone literal (with any number of redundant parens around it) is
 * decided here. php's own folder reaches much further -- it evaluates `1 === 1`,
 * `PHP_INT_SIZE === 8`, even `[1,2][0]` -- and everything it cannot fold at compile
 * time, a user constant among them, it leaves for the rule walk to refuse. The
 * caller answers -1 by leaving the whole expression alone, which is the safe
 * direction: an offender php would have refused stays accepted, and no valid
 * program is refused on a branch php would have dropped.
 */
static int GenStateConstExprTruth(SyToken *pStart,SyToken *pStop)
{
	SyToken *p = pStart, *q = pStop;
	while( p < q && (p->nType & PH7_TK_LPAREN) && (q[-1].nType & PH7_TK_RPAREN) ){
		p++;
		q--;
	}
	if( &p[1] != q ){
		return -1; /* not a lone token */
	}
	if( p->nType & PH7_TK_ID ){
		/* `true`/`false`/`null` are not lexer keywords here -- they arrive as plain
		 * identifiers and are recognised by name (php reserves all three, so no user
		 * constant can shadow one). Any OTHER name is a constant whose value this
		 * stage does not know, which is exactly where php stops folding too. */
		if( p->sData.nByte == 4 && SyStrnicmp(p->sData.zString,"true",4) == 0 ){
			return 1;
		}
		if( (p->sData.nByte == 5 && SyStrnicmp(p->sData.zString,"false",5) == 0)
			|| (p->sData.nByte == 4 && SyStrnicmp(p->sData.zString,"null",4) == 0) ){
			return 0;
		}
		return -1;
	}
	if( p->nType & (PH7_TK_INTEGER|PH7_TK_REAL) ){
		/* php's truthiness: 0 and 0.0 are false, everything else true. */
		const char *z = p->sData.zString;
		sxu32 n = p->sData.nByte, i;
		for( i = 0 ; i < n ; ++i ){
			if( z[i] != '0' && z[i] != '.' && z[i] != '+' && z[i] != '-' ){
				return 1;
			}
		}
		return 0;
	}
	if( p->nType & PH7_TK_DSTR ){
		/* A double-quoted literal may interpolate; only a single-quoted one is
		 * certainly its own text. Leave the rest undecided. */
		return -1;
	}
	if( p->nType & PH7_TK_SSTR ){
		return ( p->sData.nByte == 0
			|| (p->sData.nByte == 1 && p->sData.zString[0] == '0') ) ? 0 : 1;
	}
	return -1;
}
static const char * GenStateConstExprSpan(SyToken *pStart,SyToken *pStop,int bAllowNew,int nDepth);
/*
 * TRUE if a `?` appears anywhere in the span outside a closure body. Used only
 * after the top-level ternary has been ruled out: a ternary NESTED in a bracket
 * (`[true ? 1 : new X]`, `(true ? 1 : strlen('a')) + 1`) is folded by php just the
 * same, but the split below cannot say where its condition begins, so the span is
 * left alone rather than have a dropped branch refuse a valid program. A `?` in a
 * closure body is runtime code and does not count; `?->` and `??` are their own
 * operators and never land here.
 */
static int GenStateSpanHasNestedTernary(SyToken *pStart,SyToken *pStop)
{
	SyToken *p = pStart;
	int iDepth = 0;
	while( p < pStop ){
		if( (p->nType & PH7_TK_KEYWORD) && (p->nType & PH7_TK_MEMBER_NAME) == 0
			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION
				|| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){
			GenStateInitSkipFuncConstruct(&p,pStop,&iDepth);
			continue;
		}
		if( (p->nType & PH7_TK_OP) && p->pUserData
			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_QUESTY ){
			return 1;
		}
		p++;
	}
	return 0;
}
/*
 * Split a constant expression at its top-level ternary, the way php's constant
 * folder does: the condition decides which branch survives, and only the surviving
 * one is subject to the rules. `true ? 1 : new X` and `false ? new X : 1` are both
 * legal php for exactly this reason, and scanning the dropped branch refused valid
 * programs. Answers 1 when it handled the span (writing the verdict to *pzErr).
 */
static int GenStateConstExprTernary(SyToken *pStart,SyToken *pStop,int bAllowNew,
	int nDepth,const char **pzErr)
{
	SyToken *p = pStart, *pQ = 0, *pColon = 0;
	int iDepth = 0, iNest = 0, iTruth;
	while( p < pStop ){
		if( (p->nType & PH7_TK_KEYWORD) && (p->nType & PH7_TK_MEMBER_NAME) == 0
			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION
				|| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){
			/* A ternary inside a closure body -- or an ARROW function's whole body,
			 * which carries no braces to raise the depth -- is runtime code, not this
			 * expression's ternary. Reading `fn() => true ? 1 : 2` as one folded away
			 * the arrow that the rules exist to refuse. */
			GenStateInitSkipFuncConstruct(&p,pStop,&iDepth);
			continue;
		}
		if( p->nType & (PH7_TK_LPAREN|PH7_TK_OSB|PH7_TK_OCB) ){
			iDepth++;
		}else if( p->nType & (PH7_TK_RPAREN|PH7_TK_CSB|PH7_TK_CCB) ){
			if( iDepth > 0 ){
				iDepth--;
			}
		}else if( iDepth == 0 && (p->nType & PH7_TK_OP) && p->pUserData
			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_QUESTY ){
			if( pQ == 0 ){
				pQ = p;
			}
			iNest++;
		}else if( iDepth == 0 && pQ != 0 && (p->nType & PH7_TK_COLON) ){
			/* `::` is one DC operator token, not two colons, so it never lands here;
			 * a nested ternary's own colon is matched against its own `?`. */
			iNest--;
			if( iNest == 0 ){
				pColon = p;
				break;
			}
		}
		p++;
	}
	if( pQ == 0 ){
		return 0; /* no top-level ternary: the linear walk owns this span */
	}
	if( pColon == 0 ){
		*pzErr = 0; /* unbalanced (the parser will say so); nothing to rule on here */
		return 1;
	}
	iTruth = GenStateConstExprTruth(pStart,pQ);
	if( iTruth < 0 ){
		*pzErr = 0; /* php would fold what this cannot: leave the whole span alone */
		return 1;
	}
	*pzErr = GenStateConstExprSpan(pStart,pQ,bAllowNew,nDepth + 1);
	if( *pzErr == 0 ){
		/* `c ?: e` keeps the condition when it is truthy, so the then-span is empty. */
		*pzErr = iTruth
			? GenStateConstExprSpan(&pQ[1],pColon,bAllowNew,nDepth + 1)
			: GenStateConstExprSpan(&pColon[1],pStop,bAllowNew,nDepth + 1);
	}
	return 1;
}
/*
 * Every compile-time rule php applies to a CONSTANT EXPRESSION, over one token span,
 * as a single left-to-right walk that answers with the FIRST offender's sentence --
 * which is exactly the one php prints, because php walks the same expression in the
 * same order and stops at the first node it refuses to compile. Run as separate
 * passes, the rules got that order wrong whenever two kinds appeared together:
 * `[strlen('a'), function(){}]` said "Closures in constant expressions must be
 * static" where php says "Constant expression contains invalid operations".
 *
 * The rules, in the order a token can trigger them:
 *   - an ARROW function -- "Constant expression contains invalid operations". There
 *     is no static-`fn` escape: `static fn()=>1` is refused too;
 *   - an IMMEDIATELY INVOKED closure -- a call is what php names it, so it takes the
 *     call sentence and not the closure one;
 *   - a NON-STATIC closure -- "Closures in constant expressions must be static".
 *     `static function(){...}` is accepted and its body skipped, being runtime code;
 *   - a STATIC PROPERTY fetch (`C::$p`, `self::$p`, `static::$p`) -- php has no
 *     constant-expression node for one, so it is "invalid operations";
 *   - `static::` -- "\"static::\" is not allowed in compile-time constants", with the
 *     `static::class` face carrying php's own separate sentence. `self::`/`parent::`
 *     are fine: they name the DECLARING class, which is known where it is written;
 *   - a CALL -- "Constant expression contains invalid operations". A first-class
 *     callable (`strlen(...)`) is only an ellipsis in parens, and a constructor's
 *     argument list belongs to its `new`, so neither of those counts;
 *   - `new`, unless bAllowNew -- "New expressions are not supported in this context".
 */
static const char * GenStateConstExprSpan(SyToken *pStart,SyToken *pStop,int bAllowNew,int nDepth)
{
	SyToken *p = pStart;
	int iDepth = 0;
	const char *zTern = 0;
	if( nDepth > 32 ){
		return 0; /* pathological nesting: stop rather than recurse */
	}
	if( GenStateConstExprTernary(pStart,pStop,bAllowNew,nDepth,&zTern) ){
		return zTern;
	}
	if( GenStateSpanHasNestedTernary(pStart,pStop) ){
		return 0; /* php folds it and this cannot: leave the span alone */
	}
	while( p < pStop ){
		if( (p->nType & PH7_TK_KEYWORD) && (p->nType & PH7_TK_MEMBER_NAME) == 0 ){
			sxu32 nKw = (sxu32)SX_PTR_TO_INT(p->pUserData);
			if( nKw == PH7_TKWRD_FN ){
				return "Constant expression contains invalid operations";
			}
			if( nKw == PH7_TKWRD_FUNCTION ){
				int iScan = iDepth;
				SyToken *q = p;
				/* `(function(){...})()` is a CALL to php, and a call is what it names --
				 * the closure rule never gets a say. Look past the construct and the
				 * parens wrapping it for the argument list. */
				GenStateInitSkipFuncConstruct(&q,pStop,&iScan);
				while( q < pStop && (q->nType & PH7_TK_RPAREN) ){
					q++;
				}
				if( q < pStop && (q->nType & PH7_TK_LPAREN) ){
					return "Constant expression contains invalid operations";
				}
				/* The `static` modifier sits in the token immediately before. */
				if( !(p > pStart && (p[-1].nType & PH7_TK_KEYWORD)
					&& SX_PTR_TO_INT(p[-1].pUserData) == PH7_TKWRD_STATIC) ){
					return "Closures in constant expressions must be static";
				}
				GenStateInitSkipFuncConstruct(&p,pStop,&iDepth);
				continue;
			}
			if( nKw == PH7_TKWRD_STATIC && &p[1] < pStop
				&& GenStateTokenIsDoubleColon(&p[1])
				&& !(&p[2] < pStop && (p[2].nType & PH7_TK_DOLLAR)) ){
				/* `static::$p` is excluded above: it is a property fetch, which the
				 * `::` rule below words php's way. */
				if( &p[2] < pStop && (p[2].nType & PH7_TK_KEYWORD)
					&& SX_PTR_TO_INT(p[2].pUserData) == PH7_TKWRD_CLASS ){
					return "static::class cannot be used for compile-time class name resolution";
				}
				return "\"static::\" is not allowed in compile-time constants";
			}
		}
		if( GenStateTokenIsDoubleColon(p) && &p[1] < pStop && (p[1].nType & PH7_TK_DOLLAR) ){
			return "Constant expression contains invalid operations";
		}
		if( p->nType & (PH7_TK_LPAREN|PH7_TK_OSB|PH7_TK_OCB) ){
			/* A '(' directly after a NAME is a call. A name here is an identifier
			 * token; `new X(` is excluded by walking back to the `new` operator, and
			 * `f(...)` (first-class callable) by peeking for a lone ellipsis. */
			if( (p->nType & PH7_TK_LPAREN) && p > pStart
				&& (p[-1].nType & PH7_TK_ID)
				&& !((p[-1].nType & PH7_TK_OP) && p[-1].pUserData
					&& ((const ph7_expr_op *)p[-1].pUserData)->iOp == EXPR_OP_NEW) ){
				int bNewCtor = 0;
				SyToken *q = &p[-1];
				/* Walk back over a qualified name (A\B, A::b, $o->m) to a `new`. The name
				 * ALTERNATES -- segment, separator, segment -- so both steps have to be
				 * taken: stepping over the separator alone stopped on the segment before
				 * it, and every `new` whose class name carries a `\` then read as a CALL.
				 * `new Rule\A()` is the ordinary spelling in namespaced code, so an
				 * attribute argument, a global `const` and a parameter default all
				 * refused what php compiles (Respect\Validation's whole attribute
				 * suite is written that way). */
				while( q > pStart ){
					SyToken *pPrev = &q[-1];
					if( (pPrev->nType & PH7_TK_OP) && pPrev->pUserData
						&& ((const ph7_expr_op *)pPrev->pUserData)->iOp == EXPR_OP_NEW ){
						bNewCtor = 1;
						break;
					}
					if( GenStateTokenIsMemberOp(pPrev) || (pPrev->nType & PH7_TK_NSSEP) ){
						q--;   /* a separator: whatever precedes it continues the name */
						continue;
					}
					if( (pPrev->nType & PH7_TK_ID)
						&& (GenStateTokenIsMemberOp(q) || (q->nType & PH7_TK_NSSEP)) ){
						q--;   /* ...and a SEGMENT, but only across a separator */
						continue;
					}
					break;
				}
				if( !bNewCtor && !(&p[1] < pStop && (p[1].nType & PH7_TK_ELLIPSIS)) ){
					return "Constant expression contains invalid operations";
				}
			}
			iDepth++;
		}else if( p->nType & (PH7_TK_RPAREN|PH7_TK_CSB|PH7_TK_CCB) ){
			if( iDepth > 0 ){
				iDepth--;
			}
		}else if( !bAllowNew && (p->nType & PH7_TK_OP) && p->pUserData
			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){
			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID|PH7_TK_OP) whose
			 * pUserData is the operator instance, not a keyword id. Ignore a `new`
			 * used as a member name (`A::new` / `$o->new`). */
			if( p == pStart || !GenStateTokenIsMemberOp(&p[-1]) ){
				return "New expressions are not supported in this context";
			}
		}
		p++;
	}
	return 0;
}
/*
 * The constant-expression screen as the compiler's call sites use it: the rules of
 * GenStateConstExprSpan over the initializer that begins at the current token.
 *
 * bAllowNew states PHP 8.1's split: a global `const`, a parameter default and an
 * attribute argument take `new`; a class/interface constant, an enum case value and
 * a property default do not.
 *
 * Returns the sentence, or 0 when the expression is clean. Never consumes tokens.
 */
PH7_PRIVATE const char * PH7_GenStateConstExprError(ph7_gen_state *pGen,int bAllowNew)
{
	return GenStateConstExprSpan(pGen->pIn,
		GenStateConstExprEnd(pGen->pIn,pGen->pEnd),bAllowNew,0);
}
/*
 * php keeps class CONSTANTS and PROPERTIES in separate namespaces: a class may
 * declare `const C` and `public $C` together, and `$obj->C` never resolves to the
 * constant. PHL stores each in its own table (constants in hConst, properties in
 * hAttr), so these two lookups target the right namespace and never collide.
 */
static ph7_class_attr * GenStateExtractConstant(ph7_class *pClass,SyString *pName)
{
	return PH7_ClassExtractConstant(pClass,pName->zString,pName->nByte);
}
static ph7_class_attr * GenStateExtractProperty(ph7_class *pClass,const char *zName,sxu32 nByte)
{
	return PH7_ClassExtractAttribute(pClass,zName,nByte);
}
/*
 * Copy a parsed declared type onto a freshly created class attribute (property,
 * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come
 * straight from GenStateParseUnionTypeDecl; for a union the alternatives are
 * shared from pAlts — their class-name SyStrings are VM-allocator owned and
 * outlive the temporary set, so multiple attrs in a multi-declaration chain may
 * share the same backing.
 */
static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,
	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)
{
	pAttr->nType = nType;
	pAttr->sClass = *pClass;
	pAttr->sTypeName = *pTypeName;
	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){
		sxu32 i;
		for( i = 0; i < SySetUsed(pAlts); i++ ){
			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);
			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);
		}
	}
}
static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)
{
	sxu32 nLine = pGen->pIn->nLine;
	SySet *pInstrContainer;
	ph7_class_attr *pCons;
	SyString *pName;
	sxi32 rc;
	sxu32 nType = 0;
	SyString sTypeClass;
	SyString sTypeText;
	SySet aUnionAlts;
	sxi32 iTypeFlags = 0;
	SyStringInitFromBuf(&sTypeClass,0,0);
	SyStringInitFromBuf(&sTypeText,0,0);
	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));
	/* Extract visibility level */
	iProtection = GetProtectionLevel(iProtection);
	/* Mark as constant */
	iFlags |= PH7_CLASS_ATTR_CONSTANT;
	pGen->pIn++; /* Jump the 'const' keyword */
	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and
	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */
	if( GenStateClassConstHasType(pGen) ){
		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,
			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,
			/* bParamCtx */ 0,pGen->pIn->nLine);
		/* On abort the whole compilation tears down and the VM allocator (which
		 * backs aUnionAlts) is released, so abort paths below don't free it —
		 * matching the rest of this function; only the recoverable Synchronize
		 * and success paths release. */
		if( rc == SXERR_CORRUPT ){
			/* Error already reported by GenStateParseUnionTypeDecl */
			goto Synchronize;
		}else if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}else if( rc != SXRET_OK ){
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
				"Invalid type for class constant inside class '%z'",&pClass->sName);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto Synchronize;
		}
		iTypeFlags |= PH7_CLASS_ATTR_TYPED;
	}
loop:
	/* php 8 accepts EVERY reserved word as a class-constant name — `const list = 5`,
	 * `const match`, `const function`, even `const true` — because a class constant is
	 * addressed only through `C::name`, where no keyword can be ambiguous. The single
	 * exception is `class`, reserved for `C::class`, and it gets its own message.
	 * (Method names already accept the whole set; this is the member-name side of the
	 * same rule. Global `const` is NOT the same rule: php rejects a reserved word there.)
	 * A keyword arrives as PH7_TK_KEYWORD, which this ID-only test rejected — so PHL
	 * accepted only the alpha-OPERATOR keywords (`const new`, `const and`), which the
	 * lexer marks PH7_TK_ID|PH7_TK_OP, and that partial allow-list looked like a design. */
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
		/* Invalid constant name */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Peek constant name */
	pName = &pGen->pIn->sData;
	if( (pGen->pIn->nType & PH7_TK_KEYWORD)
		&& (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CLASS ){
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
			"A class constant must not be called 'class'; it is reserved for class name fetching");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* No reserved-CONSTANT check here: true/false/null are reserved GLOBAL constant
	 * names (compile_stmt.c still rejects `const true = 1`), but `C::true` addresses a
	 * class constant and php accepts the declaration like any other reserved word. The
	 * member-name flag keeps the read from folding into the boolean literal. */
	if( (iFlags & PH7_CLASS_ATTR_FINAL) && iProtection == PH7_CLASS_PROT_PRIVATE ){
		/* `final` says "no subclass may replace this", and a PRIVATE constant is not
		 * visible to one -- so php refuses the pair, naming the constant. Same shape
		 * as the private-final METHOD rule one member over, except php makes this one
		 * a fatal rather than a warning. Reported per NAME, which is php's order for
		 * a multi-declaration too. */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
			"Private constant %z::%z cannot be final as it is not visible to other classes",
			&pClass->sName,pName);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */
	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){
		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,
			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,
			"Class constant %z::%z cannot have type %z",nLine);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}else if( rc != SXRET_OK ){
			goto Synchronize;
		}
	}
	/* Advance the stream cursor */
	pGen->pIn++;
	if(pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){
		/* Invalid declaration */
		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Expected '=' after class constant %z'",pName);
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	pGen->pIn++; /* Jump the equal sign */
	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant
	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid
	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the
	 * literal shape here, at definition time, matching PHP's eager fatal. */
	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)
		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
			"Cannot use float as value for class constant %z::%z of type %z",
			&pClass->sName,pName,&sTypeText);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* php's constant-expression rules, first offender wins (see
	 * PH7_GenStateConstExprError). A class/interface constant takes no `new`. */
	{
		const char *zCErr = PH7_GenStateConstExprError(pGen,0);
		if( zCErr ){
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"%s",zCErr);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto Synchronize;
		}
	}
	/* php: a class constant may not be redefined in the same class body. The
	 * property path already guarded this; the constant path did not, so
	 * `class C{const X=1; const X=2;}` silently kept one of them. */
	/* php keeps constants and properties in SEPARATE namespaces, so `const C` and
	 * `public $C` coexist. PHL now stores them in disjoint tables (hConst / hAttr),
	 * so no collision — only a genuine constant redefinition is rejected below. */
	if( GenStateExtractConstant(pClass,pName) != 0 ){
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
			"Cannot redefine class constant %z::%z",&pClass->sName,pName);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Allocate a new class attribute */
	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags|iTypeFlags);
	if( pCons ){
		GenStateConsumeDoc(&(*pGen),&pCons->sDoc);
		if( GenStateConsumeAttrs(&(*pGen),&pCons->aAttrs) == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		if( GenStateCheckAttrPlacement(&(*pGen),&pCons->aAttrs,16,16,0,0) == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	if( pCons == 0 ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){
		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);
	}
	/* Swap bytecode container */
	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);
	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);
	/* Compile constant value.
	 */
	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);
	if( rc == SXERR_EMPTY ){
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	/* Emit the done instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);
	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
	if( rc == SXERR_ABORT ){
		/* Don't worry about freeing memory, everything will be released shortly */
		return SXERR_ABORT;
	}
	/* All done,install the constant */
	rc = PH7_ClassInstallAttr(pClass,pCons);
	if( rc != SXRET_OK ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){
		/* Multiple constants declarations [i.e: const min=-1,max = 10] */
		pGen->pIn++; /* Jump the comma */
		/* A reserved word is a valid name for EVERY constant in the declaration, not
		 * just the first (`const list = 1, match = 2`) — same allow-list as the head. */
		if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
			SyToken *pTok = pGen->pIn;
			if( pTok >= pGen->pEnd ){
				pTok--;
			}
			rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,
				"Unexpected token '%z',expecting constant declaration inside class '%z'",
				&pTok->sData,&pClass->sName);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}else{
			goto loop;
		}
	}
	SySetRelease(&aUnionAlts);
	return SXRET_OK;
Synchronize:
	SySetRelease(&aUnionAlts);
	/* Synchronize with the first semi-colon */
	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){
		pGen->pIn++;
	}
	return SXERR_CORRUPT;
}
/*
 * complie a class attribute or Properties in the PHP jargon.
 * According to the PHP language reference manual
 *  Properties
 *  Class member variables are called "properties". You may also see them referred
 *  to using other terms such as "attributes" or "fields", but for the purposes
 *  of this reference we will use "properties". They are defined by using one
 *  of the keywords public, protected, or private, followed by a normal variable
 *  declaration. This declaration may include an initialization, but this initialization
 *  must be a constant value--that is, it must be able to be evaluated at compile time
 *  and must not depend on run-time information in order to be evaluated.
 * Symisc eXtension.
 *  PH7 allow any complex expression to be associated with the attribute while
 *  the zend engine would allow only simple scalar value.
 *  Example:
 *   class Test{
 *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call
 *   };
 *   var_dump(TEST::myVar);
 *   Refer to the official documentation for more information on the powerful extension
 *   introduced by the PH7 engine to the OO subsystem.
 */
/*
 * Lookahead: return TRUE if the tokens starting at pStart look like a typed
 * property declaration — i.e. an optional '?', optional '\', one or more
 * ID/keyword tokens (possibly separated by '\' for namespace paths), followed
 * by a '$'. This is used by the class-body dispatcher to decide whether to
 * route into the typed-attribute path vs. fall through to method/const/etc.
 */
static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)
{
	SyToken *p = pStart;
	int bFirst = 1;
	if( p >= pEnd ) return 0;
	/* Optional nullable `?` shorthand. */
	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){
		p++;
		if( p >= pEnd ) return 0;
	}
	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.
	 * One or more `|`-separated parts; each part is either a parenthesized
	 * intersection `( … )` or an atom optionally followed by a bare `&`
	 * intersection. We only need to land on the `$` to classify the member. */
	for(;;){
		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){
			/* Parenthesized DNF group — skip to the matching `)`. */
			p++;
			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }
			if( p >= pEnd ) return 0;
			p++; /* skip ')' */
		}else{
			/* A type atom: optional `\`, an identifier/keyword, namespace path,
			 * then any `&`-joined intersection members. */
			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }
			if( p >= pEnd || (p->nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
				return 0;
			}
			/* Reject class-body modifier keywords that aren't types (only on the
			 * first atom; visibility is already consumed, but static/final/abstract
			 * may still appear at the initial dispatch site). */
			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){
				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));
				if( k == PH7_TKWRD_FUNCTION || k == PH7_TKWRD_VAR || k == PH7_TKWRD_CONST
				 || k == PH7_TKWRD_STATIC || k == PH7_TKWRD_FINAL || k == PH7_TKWRD_ABSTRACT ){
					return 0;
				}
			}
			p++;
			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID|PH7_TK_KEYWORD)) ){
				p += 2;
			}
			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)
				&& (p[1].nType & (PH7_TK_NSSEP|PH7_TK_ID|PH7_TK_KEYWORD)) ){
				p++; /* skip '&' */
				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }
				if( p >= pEnd || (p->nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ) return 0;
				p++;
				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID|PH7_TK_KEYWORD)) ){
					p += 2;
				}
			}
		}
		bFirst = 0;
		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1
			&& p->sData.zString[0] == '|' ){
			p++; /* next `|`-separated part */
			continue;
		}
		break;
	}
	if( p >= pEnd ) return 0;
	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;
}

/*
 * Parse an optional property type hint starting at pGen->pIn. On return,
 * pGen->pIn points at the '$' token if a type was present (or is unchanged
 * if not). Recognized forms:
 *   ?Type, array, bool, int, float, string, object,
 *   self, parent, \Ns\ClassName, ClassName
 * The 'iterable' pseudo-type is not yet supported and is rejected earlier
 * by GenStateCompileClassAttr along with void/never/mixed/callable.
 * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX
 * on unrecoverable error.
 *
 * When a type is parsed:
 *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)
 *   *pClass is set to the class name (for class types)
 *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE
 *   *pTypeText is set to the original text span of the type
 * Otherwise they are left unchanged (so multi-decl reuse works).
 */
static sxi32 GenStateParsePropertyType(
	ph7_gen_state *pGen,
	sxu32 *pnType,
	SyString *pClass,
	sxi32 *piTypeFlags,
	SyString *pTypeText,
	SySet *pAlts
){
	sxi32 iFlags = 0;
	sxi32 rc;
	if( pGen->pIn >= pGen->pEnd ){
		return SXRET_OK;
	}
	/* If the first token is '$', there's no type */
	if( pGen->pIn->nType & PH7_TK_DOLLAR ){
		return SXRET_OK;
	}
	rc = GenStateParseUnionTypeDecl(
		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,
		PH7_CLASS_ATTR_NULLABLE,
		PH7_CLASS_ATTR_UNION,
		/* bAllowVoid */ 0,
		/* bParamCtx */ 0,
		pGen->pIn->nLine);
	if( rc != SXRET_OK ){
		return rc;
	}
	/* Verify next token is '$' (start of property name) */
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){
		return SXERR_SYNTAX;
	}
	*piTypeFlags = iFlags | PH7_CLASS_ATTR_TYPED;
	return SXRET_OK;
}

/*
 * Return TRUE if a parsed type atom — identified by (nType, sClass) as
 * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP
 * forbids on properties. `callable`, `mixed`, and `iterable` are parsed
 * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they
 * are not recognized scalar keywords; `void` and `never` are rejected
 * by the type parser itself before reaching here.
 *
 * On TRUE, *pzName / *pnName point at a static canonical spelling for
 * use in the error message.
 */
static int GenStateIsDisallowedPropertyAtom(
	sxu32 nType,
	const SyString *pClass,
	const char **pzName,
	sxu32 *pnName)
{
	const char *z;
	sxu32 n;
	if( nType != SXU32_HIGH || pClass == 0 || pClass->nByte == 0 ){
		return 0;
	}
	z = pClass->zString;
	n = pClass->nByte;
	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){
		*pzName = "callable"; *pnName = 8; return 1;
	}
	/* `mixed` (any value) and `iterable` (= array|Traversable) are valid PHP
	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via
	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */
	return 0;
}

/*
 * Validate a parsed class-member type (property, promoted parameter or class
 * constant) — the main atom plus any union alternatives — against the
 * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string
 * taking three %z arguments (class name, member name, full canonical type text),
 * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have
 * type T" vs "Class constant C::X cannot have type T").
 *
 * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection
 * (error already emitted), or SXERR_ABORT on error-count overflow.
 */
PH7_PRIVATE sxi32 GenStateValidateMemberType(
	ph7_gen_state *pGen,
	ph7_class *pClass,
	const SyString *pMemberName,
	sxu32 nType,
	const SyString *pTypeClass,
	const SyString *pTypeText,
	SySet *pUnionAlts,
	const char *zErrFmt,
	sxu32 nLine)
{
	const char *zBad = 0;
	sxu32 nBad = 0;
	SyString sFallback;
	const SyString *pBad;
	sxi32 rc;
	int bDisallowed = 0;
	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){
		bDisallowed = 1;
	}else if( pUnionAlts ){
		sxu32 i;
		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){
			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);
			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){
				bDisallowed = 1;
				break;
			}
		}
	}
	if( !bDisallowed ){
		return SXRET_OK;
	}
	/* Prefer the full canonical type text (PHP prints `callable|int` for
	 * a union, not just the offending atom). Fall back to the atom's own
	 * canonical spelling if the type text is unavailable. */
	if( pTypeText && SyStringLength(pTypeText) > 0 ){
		pBad = pTypeText;
	}else{
		SyStringInitFromBuf(&sFallback,zBad,nBad);
		pBad = &sFallback;
	}
	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
		zErrFmt,
		&pClass->sName,pMemberName,pBad);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	return SXERR_SYNTAX;
}
/*
 * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not
 * reserve `readonly` (it remains valid as a method/function name), so it is
 * matched as a plain identifier in the class-member modifier position rather
 * than promoted to a lexer keyword.
 */
PH7_PRIVATE int GenStateIsReadonly(SyToken *pTok)
{
	return (pTok->nType & PH7_TK_ID)
		&& pTok->sData.nByte == sizeof("readonly")-1
		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;
}
/*
 * Detect an asymmetric set-visibility modifier `public(set)` / `protected(set)`
 * / `private(set)` (PHP 8.4) starting at pTok. Returns the visibility keyword id
 * (PH7_TKWRD_*) and sets *pnTok to the 4 tokens consumed, or 0 when not present
 * (a bare visibility keyword is NOT a set-modifier; the '(' 'set' ')' run is).
 */
PH7_PRIVATE sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok)
{
	*pnTok = 0;
	if( &pTok[3] < pEnd
	 && (pTok->nType & PH7_TK_KEYWORD)
	 && (pTok[1].nType & PH7_TK_LPAREN)
	 && (pTok[2].nType & (PH7_TK_ID|PH7_TK_KEYWORD))
	 && pTok[2].sData.nByte == sizeof("set")-1
	 && SyStrnicmp(pTok[2].sData.zString,"set",sizeof("set")-1) == 0
	 && (pTok[3].nType & PH7_TK_RPAREN) ){
		sxi32 nKw = SX_PTR_TO_INT(pTok->pUserData);
		if( nKw == PH7_TKWRD_PUBLIC || nKw == PH7_TKWRD_PRIVATE || nKw == PH7_TKWRD_PROTECTED ){
			*pnTok = 4;
			return nKw;
		}
	}
	return 0;
}
/* Map a set-visibility keyword to its PH7_CLASS_ATTR_* flag. */
PH7_PRIVATE sxi32 GenStateSetVisFlag(sxi32 nKw)
{
	if( nKw == PH7_TKWRD_PRIVATE ){
		return PH7_CLASS_ATTR_PRIVATE_SET;
	}
	if( nKw == PH7_TKWRD_PROTECTED ){
		return PH7_CLASS_ATTR_PROTECTED_SET;
	}
	return PH7_CLASS_ATTR_PUBLIC_SET;
}
static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)
{
	sxu32 nLine = pGen->pIn->nLine;
	ph7_class_attr *pAttr;
	SyString *pName;
	sxi32 rc;
	sxu32 nType = 0;
	SyString sTypeClass;
	SyString sTypeText;
	SySet aUnionAlts;
	sxi32 iTypeFlags = 0;
	SyStringInitFromBuf(&sTypeClass,0,0);
	SyStringInitFromBuf(&sTypeText,0,0);
	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));
	/* In a readonly class (PHP 8.2) every declared instance property is readonly;
	 * the per-property readonly rules below then apply uniformly (a static or
	 * untyped property, or one with a default, raises the same PHP-exact fatal). */
	if( pClass->iFlags & PH7_CLASS_READONLY ){
		iFlags |= PH7_CLASS_ATTR_READONLY;
	}
	/* Extract visibility level */
	iProtection = GetProtectionLevel(iProtection);
	/* Parse optional type hint (typed properties, PHP 7.4+) */
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){
		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);
		if( rc == SXERR_CORRUPT ){
			/* Error already reported by GenStateParseUnionTypeDecl */
			goto Synchronize;
		}else if( rc == SXERR_SYNTAX ){
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
				"Invalid property type or declaration near '%z'",
				&pGen->pIn->sData);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto Synchronize;
		}else if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
loop:
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){
		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Expected '$' at start of property name");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	pGen->pIn++; /* Jump the dollar sign */
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & (PH7_TK_KEYWORD|PH7_TK_ID)) == 0 ){
		/* Invalid attribute name */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Peek attribute name */
	pName = &pGen->pIn->sData;
	/* Advance the stream cursor */
	pGen->pIn++;
	if(pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/|PH7_TK_SEMI/*';'*/|PH7_TK_COMMA/*','*/|PH7_TK_OCB/*'{' hooks*/)) == 0 ){
		/* Invalid declaration */
		/* php reports the offending token here, expecting "," or ";". */
		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\",\" or \";\"");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Asymmetric-visibility rules (PHP 8.4): the property must be typed, and
	 * the read visibility must not be narrower than the set visibility. */
	if( iFlags & (PH7_CLASS_ATTR_PRIVATE_SET|PH7_CLASS_ATTR_PROTECTED_SET|PH7_CLASS_ATTR_PUBLIC_SET) ){
		const char *zAvErr = 0;
		sxi32 iSetLevel = (iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? PH7_CLASS_PROT_PRIVATE
			: (iFlags & PH7_CLASS_ATTR_PROTECTED_SET) ? PH7_CLASS_PROT_PROTECTED
			: PH7_CLASS_PROT_PUBLIC;
		if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){
			zAvErr = "Property with asymmetric visibility %z::$%z must have type";
		}else if( iProtection > iSetLevel ){
			zAvErr = "Visibility of property %z::$%z must not be weaker than set visibility";
		}
		if( zAvErr ){
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zAvErr,&pClass->sName,pName);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto Synchronize;
		}
	}
	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and
	 * cannot carry a default value. PHP-exact diagnostics. */
	if( iFlags & PH7_CLASS_ATTR_READONLY ){
		const char *zRoErr = 0;
		if( iFlags & PH7_CLASS_ATTR_STATIC ){
			zRoErr = "Static property %z::$%z cannot be readonly";
		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){
			zRoErr = "Readonly property %z::$%z must have type";
		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){
			zRoErr = "Readonly property %z::$%z cannot have default value";
		}
		if( zRoErr ){
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto Synchronize;
		}
	}
	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main
	 * type atom or any union alternative. void/never are already rejected
	 * by the type parser. */
	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){
		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,
			&sTypeText,
			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,
			"Property %z::$%z cannot have type %z",nLine);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}else if( rc != SXRET_OK ){
			goto Synchronize;
		}
	}
	/* Reject redeclaration (catches clash with an earlier promoted property).
	 * A same-name class CONSTANT is NOT a clash — php's separate namespaces let
	 * `const C` and `public $C` coexist (stored in disjoint hConst / hAttr tables). */
	if( GenStateExtractProperty(pClass,pName->zString,pName->nByte) != 0 ){
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
			"Cannot redeclare %z::$%z",&pClass->sName,pName);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* php's constant-expression rules, first offender wins. A property default takes
	 * no `new`. pGen->pIn is still on the '=' (the scan skips it and reads the
	 * initializer non-destructively); no '=' means no default at all, and the scan
	 * then stops at the ';'/',' with nothing to report. */
	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){
		const char *zCErr = PH7_GenStateConstExprError(pGen,0);
		if( zCErr ){
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"%s",zCErr);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto Synchronize;
		}
	}
	/* Allocate a new class attribute */
	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags|iTypeFlags);
	if( pAttr ){
		GenStateConsumeDoc(&(*pGen),&pAttr->sDoc);
		if( GenStateConsumeAttrs(&(*pGen),&pAttr->aAttrs) == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		if( GenStateCheckAttrPlacement(&(*pGen),&pAttr->aAttrs,8,8,0,0) == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	if( pAttr == 0 ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");
		return SXERR_ABORT;
	}
	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){
		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);
	}
	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){
		SySet *pInstrContainer;
		SyToken *pSavedDefEnd = pGen->pEnd;
		pGen->pIn++; /*Jump the equal sign */
		{
			/* Delimit the default expression: it ends at the declaration's
			 * ';'/',' or at a top-level '{' opening a PHP 8.4 hook list
			 * (`public string $w = "init" { get => …; }`) — the expression
			 * compiler would otherwise run into the hook tokens. */
			SyToken *pScan = pGen->pIn;
			sxi32 iNest = 0;
			int bFuncSeen = 0; /* a `function` keyword stands at depth 0 */
			while( pScan < pGen->pEnd ){
				if( (pScan->nType & PH7_TK_KEYWORD) && iNest <= 0
					&& SX_PTR_TO_INT(pScan->pUserData) == PH7_TKWRD_FUNCTION ){
					/* The next depth-0 '{' is this CLOSURE's body, not a hook list:
					 * `public $p = static function(){ … };` is php-legal (a static
					 * closure is a constant expression) and its brace was read as
					 * the hook-list opener, so the default was truncated at
					 * `static function()` and the declaration died three errors
					 * deep. A class CONSTANT never showed it — only the property
					 * path carries a hook list at all. */
					bFuncSeen = 1;
				}else if( pScan->nType & (PH7_TK_LPAREN|PH7_TK_OSB) ){
					iNest++;
				}else if( pScan->nType & (PH7_TK_RPAREN|PH7_TK_CSB) ){
					iNest--;
				}else if( iNest <= 0 && (pScan->nType & PH7_TK_OCB) ){
					if( !bFuncSeen ){
						break; /* the hook list */
					}
					bFuncSeen = 0;
					pScan++;
					PH7_DelimitNestedTokens(pScan,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pScan);
					if( pScan >= pGen->pEnd ){
						break;
					}
					/* land on the closing '}', the loop's pScan++ steps past it */
				}else if( iNest <= 0 && (pScan->nType & (PH7_TK_SEMI|PH7_TK_COMMA)) ){
					break;
				}
				pScan++;
			}
			pGen->pEnd = pScan;
		}
		/* Swap bytecode container */
		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);
		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);
		/* Compile attribute value. The default is a const-expression belonging to
		 * pClass (see iInMemberDefault) — __TRAIT__ in it reads pCurClass. */
		pGen->iInMemberDefault++;
		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);
		pGen->iInMemberDefault--;
		if( rc == SXERR_EMPTY ){
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}
		/* Emit the done instruction */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);
		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
		pGen->pIn = pGen->pEnd;   /* land exactly on the delimiter */
		pGen->pEnd = pSavedDefEnd;
	}
	/* All done,install the attribute */
	rc = PH7_ClassInstallAttr(pClass,pAttr);
	if( rc != SXRET_OK ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) ){
		/* PHP 8.4 property hooks: `public [T] $x [= default] { get ...; set ...; }`.
		 * The list ends the declaration at '}' — no trailing ';', no comma list. */
		if( iFlags & PH7_CLASS_ATTR_READONLY ){
			/* `readonly` promises one write, a hook decides what a write MEANS, and
			 * php will not have both -- a rule the declaration screen never had, so
			 * `public readonly int $p { get => 1; }` compiled here and does not in
			 * php (in a readonly CLASS too, where the modifier is implied). */
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
				"Hooked properties cannot be readonly");
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto Synchronize;
		}
		rc = GenStateCompilePropertyHooks(&(*pGen),pClass,pAttr);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		if( rc != SXRET_OK ){
			goto Synchronize;
		}
		SySetRelease(&aUnionAlts);
		return SXRET_OK;
	}
	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){
		/* php 8.4: `abstract` on a property requires a hook list (php's exact
		 * wording differs per declaration site) */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
			(pClass->iFlags & PH7_CLASS_INTERFACE)
				? "Interfaces may only include hooked properties"
				: "Only hooked properties may be declared abstract");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){
		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */
		pGen->pIn++; /* Jump the comma */
		if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){
			SyToken *pTok = pGen->pIn;
			if( pTok >= pGen->pEnd ){
				pTok--;
			}
			rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,
				"Unexpected token '%z',expecting attribute declaration inside class '%z'",
				&pTok->sData,&pClass->sName);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}else{
			if( pGen->pIn->nType & PH7_TK_DOLLAR ){
				goto loop;
			}
		}
	}
	SySetRelease(&aUnionAlts);
	return SXRET_OK;
Synchronize:
	/* Synchronize with the first semi-colon */
	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){
		pGen->pIn++;
	}
	SySetRelease(&aUnionAlts);
	return SXERR_CORRUPT;
}
/*
 * php validates a magic method's DECLARATION at compile time
 * (zend_check_magic_method_implementation): the ENGINE builds the arguments and
 * calls these methods on its own, so a wrong shape is rejected where it is
 * written rather than discovered — or silently tolerated — at the dispatch.
 *
 * One row per magic name; its fields are the checks php makes for that name.
 * Arity is the first of them, in php's order — which is what decides the message
 * when a declaration breaks more than one of php's rules at once.
 */
typedef struct MagicMethodRule MagicMethodRule;
struct MagicMethodRule
{
	const char *zName; /* Magic method name */
	sxu32 nName;       /* Its length */
	int nArgs;         /* Declared arguments php requires, -1 when it does not check */
	int bStatic;       /* TRUE: must be static · FALSE: must NOT be static */
	int bPublic;       /* TRUE: must be public — a WARNING, and dispatched anyway */
	int bNoReturnType; /* TRUE: declaring ANY return type is a fatal */
};
#define MAGIC_METHOD_ROW(N,A,S,P,R) { N, sizeof(N)-1, A, S, P, R }
static const MagicMethodRule aMagicMethod[] = {
	MAGIC_METHOD_ROW("__construct",  -1, FALSE, FALSE, TRUE),
	MAGIC_METHOD_ROW("__destruct",    0, FALSE, FALSE, TRUE),
	MAGIC_METHOD_ROW("__clone",       0, FALSE, FALSE, FALSE),
	MAGIC_METHOD_ROW("__get",         1, FALSE, TRUE,  FALSE),
	MAGIC_METHOD_ROW("__set",         2, FALSE, TRUE,  FALSE),
	MAGIC_METHOD_ROW("__isset",       1, FALSE, TRUE,  FALSE),
	MAGIC_METHOD_ROW("__unset",       1, FALSE, TRUE,  FALSE),
	MAGIC_METHOD_ROW("__call",        2, FALSE, TRUE,  FALSE),
	MAGIC_METHOD_ROW("__callStatic",  2, TRUE,  TRUE,  FALSE),
	MAGIC_METHOD_ROW("__toString",    0, FALSE, TRUE,  FALSE),
	MAGIC_METHOD_ROW("__invoke",     -1, FALSE, TRUE,  FALSE),
	MAGIC_METHOD_ROW("__debugInfo",   0, FALSE, TRUE,  FALSE),
	MAGIC_METHOD_ROW("__serialize",   0, FALSE, TRUE,  FALSE),
	MAGIC_METHOD_ROW("__unserialize", 1, FALSE, TRUE,  FALSE),
	MAGIC_METHOD_ROW("__sleep",       0, FALSE, TRUE,  FALSE),
	MAGIC_METHOD_ROW("__wakeup",      0, FALSE, TRUE,  FALSE),
	MAGIC_METHOD_ROW("__set_state",   1, TRUE,  TRUE,  FALSE)
};
#undef MAGIC_METHOD_ROW
/*
 * Find the rule for a declared method name, or 0 when the name is not magic.
 * php matches method names case-insensitively everywhere, so `__GET` is `__get`;
 * the two-underscore prefix test is php's own cheap reject.
 */
static const MagicMethodRule * GenStateMagicMethodRule(const SyString *pName)
{
	sxu32 n;
	if( pName->nByte < sizeof("__x")-1 || pName->zString[0] != '_' || pName->zString[1] != '_' ){
		return 0;
	}
	for( n = 0 ; n < SX_ARRAYSIZE(aMagicMethod) ; ++n ){
		if( pName->nByte == aMagicMethod[n].nName
		 && SyStrnicmp(pName->zString,aMagicMethod[n].zName,aMagicMethod[n].nName) == 0 ){
			return &aMagicMethod[n];
		}
	}
	return 0;
}
/*
 * TRUE when php requires this magic method to be PUBLIC: the rows it merely
 * WARNS about at the declaration and then dispatches regardless of what the
 * declaration said. The runtime's visibility gate reads this to let an
 * engine-built call through — a call the user WROTE stays denied.
 *
 * `__construct`/`__destruct`/`__clone` are deliberately not in the set: a
 * private constructor is the singleton idiom, and php enforces those three at
 * the call like any other method.
 */
PH7_PRIVATE int PH7_MagicMethodMustBePublic(const SyString *pName)
{
	const MagicMethodRule *pRule = GenStateMagicMethodRule(pName);
	return pRule != 0 && pRule->bPublic;
}
/*
 * Enforce the rules of pRule against the declaration just parsed. pName is the
 * name AS WRITTEN — php quotes that spelling, not the canonical one.
 *
 * The diagnostic is FORMATTED, not reported: php decides these rules while the
 * signature is in hand (so the arity beats __toString's return-type rule) but
 * raises them only once the declaration has cleared the checks php makes
 * first — the redeclaration and abstract-placement rules, and any parse error
 * in the body php has already read. The caller reports the buffer at that
 * point.
 *
 * Returns the severity it wrote: E_ERROR, E_WARNING, or 0 for a clean
 * declaration.
 */
static sxi32 GenStateCheckMagicMethod(
	ph7_class *pClass,
	const SyString *pName,
	ph7_class_method *pMeth,
	char *zErr,
	int nErrBuf
	)
{
	const MagicMethodRule *pRule = GenStateMagicMethodRule(pName);
	if( pRule == 0 ){
		return 0;
	}
	if( pRule->nArgs >= 0 ){
		/* php counts DECLARED parameters — an optional one counts
		 * (`__destruct($a = null)` is rejected) and the variadic tail does not
		 * (`__clone(...$a)` declares zero and passes, `__get(...$a)` declares
		 * zero where one is required and does not). */
		sxu32 nDecl = SySetUsed(&pMeth->sFunc.aArgs);
		sxu32 nGiven = 0;
		sxu32 n;
		for( n = 0 ; n < nDecl ; ++n ){
			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,n);
			if( pArg && (pArg->iFlags & VM_FUNC_ARG_VARIADIC) == 0 ){
				nGiven++;
			}
		}
		if( nGiven != (sxu32)pRule->nArgs ){
			if( pRule->nArgs == 0 ){
				SyBufferFormat(zErr,nErrBuf,"Method %z::%z() cannot take arguments",
					&pClass->sName,pName);
			}else{
				SyBufferFormat(zErr,nErrBuf,"Method %z::%z() must take exactly %d argument%s",
					&pClass->sName,pName,pRule->nArgs,pRule->nArgs == 1 ? "" : "s");
			}
			return E_ERROR;
		}
		/* None of the arguments the engine builds may be by-reference — there is
		 * no caller variable to write back to. php checks as many arguments as
		 * the rule counts, and the count above already skipped variadics, so
		 * this walk skips them the same way. */
		for( n = 0 ; n < nDecl ; ++n ){
			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,n);
			if( pArg == 0 || (pArg->iFlags & VM_FUNC_ARG_VARIADIC) ){
				continue;
			}
			if( pArg->iFlags & VM_FUNC_ARG_BY_REF ){
				SyBufferFormat(zErr,nErrBuf,"Method %z::%z() cannot take arguments by reference",
					&pClass->sName,pName);
				return E_ERROR;
			}
		}
	}
	/* Static-ness. Whether the engine has a receiver for a magic method is not
	 * the declaration's to choose: `__callStatic` and `__set_state` are reached
	 * with a class and nothing else, every other row is reached through an
	 * object. PHL took the declaration at its word and then dispatched the
	 * method anyway — a `static function __get()` ran with no `$this` at all,
	 * so a hook property table or a lazy-loading accessor read whatever the
	 * unbound scope happened to hold. php checks this after the arity, which is
	 * why `static function __get($a,$b)` reports the count first. */
	if( pRule->bStatic != ((pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0) ){
		SyBufferFormat(zErr,nErrBuf,"Method %z::%z() %s be static",
			&pClass->sName,pName,pRule->bStatic ? "must" : "cannot");
		return E_ERROR;
	}
	/* Visibility. This one is a WARNING: php names the declaration and then
	 * dispatches the method anyway, because the engine calling `__get` is not
	 * the outside world reaching for a private member. PHL was silent at the
	 * declaration and threw `Call to private method C::__get()` at the ACCESS —
	 * the one rule of this family that changed what a program php RUNS does,
	 * and it killed the script. The dispatch half is
	 * PH7_MagicMethodMustBePublic, read by the runtime visibility gate. */
	if( pRule->bPublic && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){
		SyBufferFormat(zErr,nErrBuf,"The magic method %z::%z() must have public visibility",
			&pClass->sName,pName);
		return E_WARNING;
	}
	/* A return type on the two methods that have no return VALUE. `new C` is the
	 * instance, never whatever __construct returned, and __destruct is called by
	 * the engine at a point with nowhere to put an answer — so php rejects any
	 * declared type on either, `void` and `never` included, rather than let a
	 * declaration promise something no caller can read. (`__clone` is NOT in
	 * this row: `: void` on it is valid php.) PHL enforced the declared type at
	 * runtime instead, so `__construct(): int` raised a TypeError at every
	 * instantiation — a diagnostic on the CALL for a mistake in the
	 * declaration. */
	if( pRule->bNoReturnType
	 && (pMeth->sFunc.nReturnType > 0
	  || SyStringLength(&pMeth->sFunc.sReturnClass) > 0
	  || SySetUsed(&pMeth->sFunc.aReturnUnion) > 0) ){
		SyBufferFormat(zErr,nErrBuf,"Method %z::%z() cannot declare a return type",
			&pClass->sName,pName);
		return E_ERROR;
	}
	return 0;
}
/*
 * Raise the declaration diagnostic parked above (GenStateCheckMagicMethod's magic-method
 * rules, or the final-private one beside its call), once, and disarm it.
 * Suppressed when this declaration has already reported a fatal php decides
 * FIRST — a redeclaration, an abstract method in a non-abstract class, a parse
 * error in the body — since php stops at its own first fatal.
 */
static sxi32 GenStateRaiseMagicDiag(
	ph7_gen_state *pGen,
	sxi32 *pnSeverity,   /* IN/OUT: the parked severity, zeroed here */
	const char *zErr,
	sxu32 nLine,
	sxu32 nErrEntry      /* pGen->nErr when this declaration started */
	)
{
	sxi32 rc = SXRET_OK;
	if( *pnSeverity != 0 ){
		if( pGen->nErr == nErrEntry ){
			rc = PH7_GenCompileError(pGen,*pnSeverity,nLine,"%s",zErr);
		}
		*pnSeverity = 0;
	}
	return rc;
}
/*
 * Compile a class method.
 *
 * Refer to the official documentation for more information
 * on the powerful extension introduced by the PH7 engine
 * to the OO subsystem such as full type hinting,method
 * overloading and many more.
 */
static sxi32 GenStateCompileClassMethod(
	ph7_gen_state *pGen, /* Code generator state */
	sxi32 iProtection,   /* Visibility level */
	sxi32 iFlags,        /* Configuration flags */
	int doBody,          /* TRUE to process method body */
	ph7_class *pClass    /* Class this method belongs */
	)
{
	sxu32 nLine = pGen->pIn->nLine;
	sxu32 nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */
	sxu32 nErrEntry = pGen->nErr; /* Errors already reported when this declaration started */
	char zMagicErr[256];          /* Pending magic-method rule violation, reported at the end */
	sxi32 nMagicSeverity = 0;     /* E_ERROR / E_WARNING while zMagicErr is still unreported */
	int bMagicFatal = FALSE;      /* The parked diagnostic was a fatal: do not install the method */
	ph7_class_method *pMeth;
	sxi32 iFuncFlags;
	SyString *pName;
	SyToken *pEnd;
	sxi32 rc;
	/* Extract visibility level */
	iProtection = GetProtectionLevel(iProtection);
	pGen->pIn++; /* Jump the 'function' keyword */
	iFuncFlags = 0;
	if( pGen->pIn >= pGen->pEnd ){
		/* Invalid method name */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){
		/* Return by reference,remember that */
		iFuncFlags |= VM_FUNC_REF_RETURN;
		/* Jump the '&' token */
		pGen->pIn++;
	}
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
		/* Invalid method name */
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Peek method name */
	pName = &pGen->pIn->sData;
	nLine = pGen->pIn->nLine;
	/* Jump the method name */
	pGen->pIn++;
	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){
		/* Abstract method */
		if( iProtection == PH7_CLASS_PROT_PRIVATE ){
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
				"Access type for abstract method '%z::%z' cannot be 'private'",
				&pClass->sName,pName);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}
		/* Assemble method signature only */
		doBody = FALSE;
	}
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Expected '(' after method name '%z'",pName);
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Allocate a new class_method instance */
	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);
	if( pMeth == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
	pMeth->sFunc.nLine = nKwLine;
	GenStateConsumeDoc(&(*pGen),&pMeth->sFunc.sDoc);
	if( GenStateConsumeAttrs(&(*pGen),&pMeth->sFunc.aAttrs) == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	if( GenStateCheckAttrPlacement(&(*pGen),&pMeth->sFunc.aAttrs,4,4,0,0) == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	/* Jump the left parenthesis '(' */
	pGen->pIn++;
	pEnd = 0; /* cc warning */
	/* Delimit the method signature */
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);
	if( pEnd >= pGen->pEnd ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Missing ')' after method '%z' declaration",pName);
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	{
		int bIsCtor = 0;
		int bAbstractCtor = 0;
		/* Only __construct is the constructor (PHP-4 class-name constructors removed
		 * in 8.0): a method named like the class is a plain method, so promoted
		 * properties in it are rejected exactly as php does elsewhere. */
		if( pName->nByte == sizeof("__construct") - 1
				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0 ){
			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){
				bAbstractCtor = 1;
			}else{
				bIsCtor = 1;
			}
		}
		if( pGen->pIn < pEnd ){
			/* Collect method arguments */
			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}
	}
	/* Point past ')' and parse optional return type ': type' */
	pGen->pIn = &pEnd[1];
	{
		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);
		if( rcRt == SXERR_ABORT ){
			return SXERR_ABORT;
		}else if( rcRt == SXERR_SYNTAX ){
			goto Synchronize;
		}
	}
	/* php's #[\NoDiscard] declaration rules, which want the return type. */
	if( GenStateApplyNoDiscard(&(*pGen),&pMeth->sFunc,pClass,
			pName->nByte == sizeof("__construct")-1
			 && SyStrnicmp(pName->zString,"__construct",sizeof("__construct")-1) == 0)
		== SXERR_ABORT ){
		return SXERR_ABORT;
	}
	/* php's compile-time magic-method declaration rules, DECIDED here — with the
	 * signature in hand and before the __toString return-type rule below, which
	 * is php's own order (`static function __toString($a): int` reports the
	 * arity). Reported at the end of this function; see zMagicErr there. */
	nMagicSeverity = GenStateCheckMagicMethod(pClass,pName,pMeth,zMagicErr,(int)sizeof(zMagicErr));
	if( nMagicSeverity == 0
	 && (pMeth->iFlags & PH7_CLASS_ATTR_FINAL)
	 && pMeth->iProtection == PH7_CLASS_PROT_PRIVATE
	 && !(pName->nByte == sizeof("__construct")-1
	   && SyStrnicmp(pName->zString,"__construct",sizeof("__construct")-1) == 0) ){
		/* Not a magic rule, but the same KIND of rule and the same parking: php
		 * checks a declaration php itself decides the meaning of. A private method
		 * is never overridden, so `final` on one says nothing — php WARNS here (8.0+)
		 * and compiles the class. PHL was silent at the declaration and then fataled
		 * at the SUBCLASS that reused the name ("Cannot override final method"), a
		 * class php accepts; the inheritance half is in PH7_ClassInherit.
		 *
		 * The CONSTRUCTOR is php's one exemption, and it is a deliberate one:
		 * `final private function __construct()` is the singleton idiom -- private
		 * to stop `new`, final to stop a subclass widening it back to public -- so
		 * the modifier does say something there. Every other private method warns,
		 * `__destruct`, `__clone` and a static one included. PHPUnit's TestSuite
		 * declares exactly this and drew the warning on every single run. */
		SyBufferFormat(zMagicErr,sizeof(zMagicErr),
			"Private methods cannot be final as they are never overridden by other classes");
		nMagicSeverity = E_WARNING;
	}
	if( nMagicSeverity == E_ERROR ){
		/* Suppress the __toString rule below: php never reaches it on a
		 * declaration the magic rules already rejected. */
		bMagicFatal = TRUE;
		goto SkipToStringType;
	}
	/*
	 * php gives __toString() an IMPLICIT `string` return type. That is what makes
	 * `return 42` coerce to "42" and `return null` / an array / an object / falling
	 * off the end raise
	 *   C::__toString(): Return value must be of type string, X returned
	 * PHL enforced DECLARED return types only, so an undeclared __toString could
	 * answer anything and MemObjStringValue fell back to the "Object" placeholder
	 * for whatever was not a non-empty string. Installing the type here reuses the
	 * enforcement that already matches php byte for byte.
	 *
	 * sReturnTypeName is filled in as well, for two reasons: reflection reports the
	 * implicit type exactly as php does (hasReturnType() TRUE, getReturnType()
	 * "string" for an undeclared __toString), and the generator-return-type fatal
	 * renders from it — so a __toString with a `yield` in it now reports php's
	 * "Generator return type must be a supertype of Generator, string given".
	 *
	 * Declaring any OTHER return type is php's own compile fatal, `?string`, a
	 * union, `mixed`, `static` and `void` included. (php checks
	 * "A void method must not return a value" FIRST when a `: void` __toString also
	 * returns a value; PHL has no such check yet, so it reports this one instead —
	 * both reject, on doubly-invalid input only.)
	 */
	if( pName->nByte == sizeof("__toString")-1
	 && SyStrnicmp(pName->zString,"__toString",sizeof("__toString")-1) == 0 ){
		ph7_vm_func *pTsFunc = &pMeth->sFunc;
		int bTsDeclared = pTsFunc->nReturnType > 0 || SySetUsed(&pTsFunc->aReturnUnion) > 0;
		if( bTsDeclared ){
			if( pTsFunc->nReturnType != MEMOBJ_STRING
			 || SySetUsed(&pTsFunc->aReturnUnion) > 0
			 || (pTsFunc->iFlags & VM_FUNC_RETURN_NULLABLE) ){
				/* php raises this one AFTER the magic rules, so a parked
				 * visibility warning is php's first line here rather than a
				 * casualty of the fatal about to be counted. */
				if( GenStateRaiseMagicDiag(pGen,&nMagicSeverity,zMagicErr,
						nKwLine,nErrEntry) == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
					"%z::%z(): Return type must be string when declared",
					&pClass->sName,pName);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				goto Synchronize;
			}
		}else{
			char *zTsType = SyMemBackendStrDup(&pGen->pVm->sAllocator,
				"string",sizeof("string")-1);
			pTsFunc->nReturnType = MEMOBJ_STRING;
			if( zTsType ){
				SyStringInitFromBuf(&pTsFunc->sReturnTypeName,zTsType,sizeof("string")-1);
			}
		}
	}
SkipToStringType:
	/* Install promoted constructor properties as class attributes. Runtime
	 * property init/typecheck is handled by the generic typed-property path
	 * since we mint real ph7_class_attr entries. */
	{
		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);
		sxu32 i;
		for( i = 0; i < nArg; i++ ){
			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);
			ph7_class_attr *pAttr;
			sxi32 iAttrFlags = 0;
			int bArgTyped;
			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){
				continue;
			}
			/* "typed" = a single type or class name, OR a union/intersection,
			 * which leaves nType=0 / empty sClass and stores its alts in
			 * aUnionAlts. Used both to validate the type and to mark the attr. */
			bArgTyped = pArg->nType > 0 || SyStringLength(&pArg->sClass) > 0
			         || (pArg->iFlags & VM_FUNC_ARG_UNION);
			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){
				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
					"Cannot declare variadic promoted property");
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				goto Synchronize;
			}
			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)
			 * that GenStateCompileClassAttr rejects — including when they
			 * appear as an alternative of a union type. */
			if( bArgTyped ){
				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,
					pArg->nType,&pArg->sClass,&pArg->sTypeName,
					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,
					"Property %z::$%z cannot have type %z",nLine);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}else if( rc != SXRET_OK ){
					goto Synchronize;
				}
			}
			/* Reject duplicate property (explicit property declared earlier with same name). */
			if( GenStateExtractProperty(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){
				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				goto Synchronize;
			}
			if( bArgTyped ){
				iAttrFlags |= PH7_CLASS_ATTR_TYPED;
			}
			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){
				iAttrFlags |= PH7_CLASS_ATTR_NULLABLE;
			}
			if( pArg->iFlags & VM_FUNC_ARG_UNION ){
				iAttrFlags |= PH7_CLASS_ATTR_UNION;
			}
			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) || (pClass->iFlags & PH7_CLASS_READONLY) ){
				/* A readonly promoted property must be typed (PHP 8.1); in a
				 * readonly class (8.2) every promoted property is readonly too. */
				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){
					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
					goto Synchronize;
				}
				iAttrFlags |= PH7_CLASS_ATTR_READONLY;
			}
			if( pArg->iFlags & VM_FUNC_ARG_FINAL ){
				/* PHP 8.4's `final` on a promoted property. No "final and private"
				 * screen here: php refuses that pair in a CLASS BODY and accepts it
				 * on a promoted parameter (`final private int $p` reflects as
				 * modifiers 36), which is php's own asymmetry, not a gap. */
				iAttrFlags |= PH7_CLASS_ATTR_FINAL;
			}
			if( pArg->iFlags & (VM_FUNC_ARG_PRIV_SET|VM_FUNC_ARG_PROT_SET) ){
				/* Asymmetric set-visibility on a promoted property (PHP 8.4) */
				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){
					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
						"Property with asymmetric visibility %z::$%z must have type",
						&pClass->sName,&pArg->sName);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
					goto Synchronize;
				}
				iAttrFlags |= (pArg->iFlags & VM_FUNC_ARG_PRIV_SET)
					? PH7_CLASS_ATTR_PRIVATE_SET : PH7_CLASS_ATTR_PROTECTED_SET;
			}
			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);
			if( pAttr == 0 ){
				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
				return SXERR_ABORT;
			}
			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){
				pAttr->nType = pArg->nType;
				pAttr->sClass = pArg->sClass;
				pAttr->sTypeName = pArg->sTypeName;
				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){
					sxu32 k;
					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){
						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);
						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);
					}
				}
			}
			/* A promoted parameter's `#[...]` belongs to BOTH members in php: the
			 * ReflectionParameter reports it and so does the ReflectionProperty,
			 * which is what makes `#[\Override] public $p` in a constructor
			 * signature a PROPERTY claim. The records are shared, not copied --
			 * the parameter owns them for the VM's lifetime. */
			{
				ph7_attribute *aSrc = (ph7_attribute *)SySetBasePtr(&pArg->aAttrs);
				sxu32 k;
				for( k = 0 ; k < SySetUsed(&pArg->aAttrs) ; k++ ){
					SySetPut(&pAttr->aAttrs,(const void *)&aSrc[k]);
				}
			}
			rc = PH7_ClassInstallAttr(pClass,pAttr);
			if( rc != SXRET_OK ){
				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
				return SXERR_ABORT;
			}
		}
	}
	if( doBody ){
		/* Compile method body */
		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		/* The cursor sits just past the body's closing brace */
		pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;
	}else{
		/* Abstract/interface method: declaration ends at the ';' */
		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) ){
			pMeth->sFunc.nEndLine = pGen->pIn->nLine;
		}
		/* Only method signature is allowed */
		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){
			/* php words this as the declaration's problem rather than a missing
			 * token -- an abstract method (an interface's included, which is
			 * abstract by being one) is a promise, and a body makes it something
			 * else. The two kinds get the two nouns php uses. */
			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){
				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
					"%s function %z::%z() cannot contain body",
					(pClass->iFlags & PH7_CLASS_INTERFACE) ? "Interface" : "Abstract",
					&pClass->sName,pName);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				return SXERR_CORRUPT;
			}
			rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,
				"Expected ';' after method signature '%z'",pName);
				if( rc == SXERR_ABORT ){
					/* Error count limit reached,abort immediately */
					return SXERR_ABORT;
				}
				return SXERR_CORRUPT;
			}
	}
	/* php: an abstract method in a class that was not DECLARED abstract is a fatal,
	 * and it names the method -- distinct from the "contains N abstract methods"
	 * wording php uses for an unimplemented INHERITED one, which
	 * GenStateCheckAbstractMethods already handles. Interfaces and traits may carry
	 * abstract methods freely. */
	if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT)
	 && (pClass->iFlags & (PH7_CLASS_ABSTRACT|PH7_CLASS_INTERFACE|PH7_CLASS_TRAIT)) == 0 ){
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
			"Class %z declares abstract method %z() and must therefore be declared abstract",
			&pClass->sName,pName);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	/* php: two methods of the same name in one class body is a fatal, and the
	 * comparison is case-INSENSITIVE (hMethod now matches that way), so `f` and
	 * `F` collide. php names the offending declaration with the spelling used at
	 * the SECOND site. */
	if( PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte) != 0 ){
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
			"Cannot redeclare %z::%z()",&pClass->sName,pName);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	/* The magic-method rule this declaration broke (decided above, with the
	 * signature in hand). It is raised HERE because php raises it last of the
	 * declaration's fatals: a redeclaration, an abstract method in a class that
	 * is not abstract, and any parse error inside the body php has already read
	 * all win — and each of them has, by now, either returned or bumped nErr.
	 * php stops at its first fatal, so one is all this declaration reports. The
	 * line is the `function` KEYWORD's, which is where php points once a
	 * signature wraps across lines. */
	if( GenStateRaiseMagicDiag(pGen,&nMagicSeverity,zMagicErr,nKwLine,nErrEntry) == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	if( bMagicFatal ){
		/* Never install a method php refused to declare. A WARNING falls through:
		 * php keeps the method and calls it. */
		return SXRET_OK;
	}
	/* All done,install the method */
	rc = PH7_ClassInstallMethod(pClass,pMeth);
	if( rc != SXRET_OK ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
	return SXRET_OK;
Synchronize:
	/* Synchronize with the first semi-colon */
	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){
		pGen->pIn++;
	}
	return SXERR_CORRUPT;
}
/*
 * php's member-modifier RUN and the screens on it.
 *
 * Everything a class/trait/interface member may carry in front of the
 * declaration it modifies -- the read visibility, an asymmetric `(set)`
 * visibility, `static`, `abstract`, `final` and the context-sensitive `readonly`
 * -- in ANY order and each at most once. They are a SET in php's grammar, so
 * `final public static int $p` and `static final public $p` are one declaration
 * each.
 *
 * The three body loops used to read ONE modifier per branch and then re-enter
 * their keyword ladder, which works only while every branch happens to be
 * reachable from every other: `static final public $p` fell out of the chain
 * with "Unexpected token 'final'", and the `final` branch only ever led to a
 * method or a constant -- so PHP 8.4's final PROPERTY was a parse error in every
 * one of its spellings, promoted parameter included. Reading the whole run in
 * one place makes the order irrelevant and gives the duplicate/combination rules
 * a single home; the caller dispatches on the token the run stops at (`const`,
 * `function`, `var`, a type, a `$name`).
 */
#define GEN_MEMBER_PROP   0  /* `[type] $name`  */
#define GEN_MEMBER_CONST  1  /* `const NAME`    */
#define GEN_MEMBER_METHOD 2  /* `function name` */
#define GEN_MEMBER_VAR    3  /* the pre-5.0 `var $name` spelling */
typedef struct GenMemberMods GenMemberMods;
struct GenMemberMods
{
	sxi32 iProtection;  /* read-visibility keyword; php's default is public */
	sxi32 iFlags;       /* PH7_CLASS_ATTR_* collected from the run */
	sxi32 nSetVis;      /* the `(set)` visibility keyword, when one was written */
	sxu32 nLine;        /* line the run starts on -- where php reports its refusals */
	int bAny;           /* TRUE once anything at all was consumed */
	int bVis,bSetVis,bStatic,bAbstract,bFinal,bReadonly;
};
/*
 * Read the run. A modifier written twice is php's own compile-time fatal, worded
 * per modifier -- and the two VISIBILITY kinds share one sentence, which is also
 * what php says for two DIFFERENT ones (`public private $p`).
 *
 * Returns SXRET_OK, SXERR_SYNTAX when a rule above was reported (the caller
 * abandons the member), or SXERR_ABORT when the error budget is spent.
 */
static sxi32 GenStateReadMemberMods(ph7_gen_state *pGen,GenMemberMods *pMods)
{
	pMods->iProtection = PH7_TKWRD_PUBLIC;
	pMods->iFlags = 0;
	pMods->nSetVis = 0;
	pMods->nLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0;
	pMods->bAny = pMods->bVis = pMods->bSetVis = 0;
	pMods->bStatic = pMods->bAbstract = pMods->bFinal = pMods->bReadonly = 0;
	while( pGen->pIn < pGen->pEnd ){
		const char *zTwice = 0;  /* the modifier php names; "" = the access-type sentence */
		int nSetTok = 0;
		sxi32 nSetVis;
		if( GenStateIsReadonly(pGen->pIn) ){
			/* `readonly` is not a reserved word, so it arrives as a plain ID; at
			 * modifier position it is always the modifier -- which is why php's
			 * answer to `public readonly readonly $x` is the duplicate rule and
			 * not a property typed `readonly`. */
			if( pMods->bReadonly ){
				zTwice = "readonly";
			}
			pMods->bReadonly = 1;
			pMods->iFlags |= PH7_CLASS_ATTR_READONLY;
			pGen->pIn++;
		}else if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){
			break;
		}else if( (nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok)) != 0 ){
			if( pMods->bSetVis ){
				zTwice = "";
			}
			pMods->bSetVis = 1;
			pMods->nSetVis = nSetVis;
			pMods->iFlags |= GenStateSetVisFlag(nSetVis);
			pGen->pIn += nSetTok;
		}else{
			sxi32 nKw = SX_PTR_TO_INT(pGen->pIn->pUserData);
			if( nKw == PH7_TKWRD_PUBLIC || nKw == PH7_TKWRD_PRIVATE
			 || nKw == PH7_TKWRD_PROTECTED ){
				if( pMods->bVis ){
					zTwice = "";
				}
				pMods->bVis = 1;
				pMods->iProtection = nKw;
			}else if( nKw == PH7_TKWRD_STATIC ){
				if( pMods->bStatic ){
					zTwice = "static";
				}
				pMods->bStatic = 1;
				pMods->iFlags |= PH7_CLASS_ATTR_STATIC;
			}else if( nKw == PH7_TKWRD_ABSTRACT ){
				if( pMods->bAbstract ){
					zTwice = "abstract";
				}
				pMods->bAbstract = 1;
				pMods->iFlags |= PH7_CLASS_ATTR_ABSTRACT;
			}else if( nKw == PH7_TKWRD_FINAL ){
				if( pMods->bFinal ){
					zTwice = "final";
				}
				pMods->bFinal = 1;
				pMods->iFlags |= PH7_CLASS_ATTR_FINAL;
			}else{
				break; /* not a modifier -- the member itself starts here */
			}
			pGen->pIn++;
		}
		pMods->bAny = 1;
		if( zTwice ){
			sxi32 rc;
			pGen->iFatalTrace = PH7_FATAL_TRACE_NONE;
			rc = zTwice[0]
				? PH7_GenCompileError(pGen,E_ERROR,pMods->nLine,
					"Multiple %s modifiers are not allowed",zTwice)
				: PH7_GenCompileError(pGen,E_ERROR,pMods->nLine,
					"Multiple access type modifiers are not allowed");
			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;
		}
	}
	return SXRET_OK;
}
/* php's name for a `(set)` visibility, as its refusals spell it. */
static const char * GenStateSetVisWord(sxi32 nKw)
{
	if( nKw == PH7_TKWRD_PRIVATE ){
		return "private(set)";
	}
	return (nKw == PH7_TKWRD_PROTECTED) ? "protected(set)" : "public(set)";
}
/*
 * The run, judged against the KIND of declaration it turned out to modify. php
 * refuses each combination with its own sentence, and the order tested below is
 * the order php reports them in when one declaration breaks several
 * (`static abstract const` is the static rule, `public private(set) static const`
 * the private(set) one).
 */
static sxi32 GenStateScreenMemberMods(ph7_gen_state *pGen,const GenMemberMods *pMods,
	int iKind,ph7_class *pClass)
{
	const char *zBad = 0;   /* the modifier php names */
	const char *zWhere = 0; /* ...and what it was written on */
	sxi32 rc;
	if( iKind == GEN_MEMBER_CONST ){
		zWhere = "a class constant";
		if( pMods->bReadonly ){
			zBad = "readonly";
		}else if( pMods->bSetVis ){
			zBad = GenStateSetVisWord(pMods->nSetVis);
		}else if( pMods->bStatic ){
			zBad = "static";
		}else if( pMods->bAbstract ){
			zBad = "abstract";
		}
	}else if( iKind == GEN_MEMBER_METHOD ){
		zWhere = "a method";
		if( pMods->bReadonly ){
			zBad = "readonly";
		}else if( pMods->bSetVis ){
			zBad = GenStateSetVisWord(pMods->nSetVis);
		}else if( pMods->bFinal && pMods->bAbstract ){
			zBad = "final";
			zWhere = "an abstract method";
		}
	}else{
		if( pMods->bFinal && pMods->bAbstract ){
			zBad = "final";
			zWhere = "an abstract property";
		}else if( pMods->bFinal && (pClass->iFlags & PH7_CLASS_INTERFACE) ){
			/* php words the interface case as the PROPERTY's problem rather than
			 * the modifier's: an interface property is a hooked REQUIREMENT, and a
			 * requirement no implementor may restate cannot be one. */
			rc = PH7_GenCompileError(pGen,E_ERROR,pMods->nLine,
				"Property in interface cannot be final");
			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;
		}else if( pMods->bFinal && pMods->iProtection == PH7_TKWRD_PRIVATE ){
			/* A private property is invisible to a subclass, so `final` on one says
			 * nothing php can honour. This is the PROPERTY rule only: php accepts
			 * the same pair on a PROMOTED constructor parameter (modifiers 36
			 * there) -- an asymmetry of php's own, reproduced rather than smoothed
			 * over. */
			rc = PH7_GenCompileError(pGen,E_ERROR,pMods->nLine,
				"Property cannot be both final and private");
			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;
		}
	}
	if( zBad == 0 ){
		return SXRET_OK;
	}
	pGen->iFatalTrace = PH7_FATAL_TRACE_NONE;
	rc = PH7_GenCompileError(pGen,E_ERROR,pMods->nLine,
		"Cannot use the %s modifier on %s",zBad,zWhere);
	return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;
}
/*
 * Peek the NAME a `const`/`function` keyword introduces, for the diagnostics php
 * words with it. Returns 0 when the declaration is malformed enough that there
 * is no name to show (the caller then falls back to a nameless sentence).
 */
static SyString * GenStateMemberNamePeek(ph7_gen_state *pGen)
{
	SyToken *p = pGen->pIn + 1;
	if( p < pGen->pEnd && (p->nType & PH7_TK_AMPER) ){
		p++; /* a by-reference method: `function &f()` */
	}
	if( p < pGen->pEnd && (p->nType & (PH7_TK_ID|PH7_TK_KEYWORD)) ){
		return &p->sData;
	}
	return 0;
}
/*
 * One MEMBER of a class/trait/interface body: the modifier run, php's screens on
 * it, and the declaration it turned out to modify. Shared by the three body
 * loops, which used to carry three near-identical modifier ladders -- and three
 * different sets of gaps.
 *
 * The enum `case` and the trait `use` statement are NOT members and never reach
 * here: they take no modifiers, and their loops consume them first.
 *
 * Returns SXRET_OK, SXERR_SYNTAX when a refusal was reported (the caller
 * abandons the body), or SXERR_ABORT when the error budget is spent.
 */
static sxi32 GenStateCompileMember(ph7_gen_state *pGen,ph7_class *pClass,const char *zBody)
{
	SyString *pName = &pClass->sName;
	GenMemberMods sMods;
	int iKind;
	sxi32 rc;
	rc = GenStateReadMemberMods(&(*pGen),&sMods);
	if( rc != SXRET_OK ){
		return rc;
	}
	if( pGen->pIn >= pGen->pEnd ){
		rc = PH7_GenCompileError(pGen,E_PARSE,sMods.nLine,
			"Expecting member declaration inside %s '%z'",zBody,pName);
		return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;
	}
	/* What the run modifies. A '$' -- or a TYPE followed by one -- is a property;
	 * the three keywords are each their own declaration. */
	if( (pGen->pIn->nType & PH7_TK_DOLLAR)
	 || GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){
		iKind = GEN_MEMBER_PROP;
	}else if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){
		rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,
			"Unexpected token '%z'. Expecting member declaration inside %s '%z'",
			&pGen->pIn->sData,zBody,pName);
		return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;
	}else{
		sxi32 nKw = SX_PTR_TO_INT(pGen->pIn->pUserData);
		if( nKw == PH7_TKWRD_CONST ){
			iKind = GEN_MEMBER_CONST;
		}else if( nKw == PH7_TKWRD_FUNCTION ){
			iKind = GEN_MEMBER_METHOD;
		}else if( nKw == PH7_TKWRD_VAR ){
			iKind = GEN_MEMBER_VAR;
		}else{
			rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,
				"Unexpected token '%z'. Expecting member declaration inside %s '%z'",
				&pGen->pIn->sData,zBody,pName);
			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;
		}
	}
	if( iKind == GEN_MEMBER_VAR ){
		/* `var $x` is the pre-5.0 spelling of `public $x` and takes NO other
		 * modifier: php's parser is looking for a VARIABLE where the modifier run
		 * left off, so `public var $x` and `final var $x` are parse errors naming
		 * the token that is not one. */
		if( sMods.bAny ){
			rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,"variable");
			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;
		}
		pGen->pIn++; /* Jump the 'var' keyword */
		if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){
			rc = PH7_GenSyntaxError(&(*pGen),
				(pGen->pIn < pGen->pEnd) ? pGen->pIn : 0,"variable");
			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;
		}
		iKind = GEN_MEMBER_PROP;
	}else{
		rc = GenStateScreenMemberMods(&(*pGen),&sMods,iKind,pClass);
		if( rc != SXRET_OK ){
			return rc;
		}
	}
	if( pClass->iFlags & PH7_CLASS_INTERFACE ){
		/* An interface member is public by declaration, and its methods are
		 * implicitly abstract -- so php refuses both `abstract` written out and
		 * `final`, naming the method in each. */
		if( sMods.iProtection != PH7_TKWRD_PUBLIC ){
			SyString *pMember = (iKind == GEN_MEMBER_PROP) ? 0 : GenStateMemberNamePeek(&(*pGen));
			if( iKind == GEN_MEMBER_PROP ){
				/* php words the PROPERTY case as the property's problem, and names
				 * neither of the two visibilities it refuses. */
				rc = PH7_GenCompileError(pGen,E_ERROR,sMods.nLine,
					"Property in interface cannot be protected or private");
			}else if( pMember ){
				rc = PH7_GenCompileError(pGen,E_ERROR,sMods.nLine,
					"Access type for interface %s %z::%z%s must be public",
					(iKind == GEN_MEMBER_CONST) ? "constant" : "method",pName,pMember,
					(iKind == GEN_MEMBER_CONST) ? "" : "()");
			}else{
				rc = PH7_GenCompileError(pGen,E_ERROR,sMods.nLine,
					"Access type for interface %s must be public",
					(iKind == GEN_MEMBER_CONST) ? "constant" : "method");
			}
			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;
		}
		if( iKind == GEN_MEMBER_METHOD && (sMods.bFinal || sMods.bAbstract) ){
			SyString *pMember = GenStateMemberNamePeek(&(*pGen));
			const char *zWhat = sMods.bFinal ? "final" : "abstract";
			if( pMember ){
				rc = PH7_GenCompileError(pGen,E_ERROR,sMods.nLine,
					"Interface method %z::%z() must not be %s",pName,pMember,zWhat);
			}else{
				rc = PH7_GenCompileError(pGen,E_ERROR,sMods.nLine,
					"Interface method must not be %s",zWhat);
			}
			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;
		}
		/* Both remaining kinds are abstract in an interface: a method has no body
		 * to compile, and a property is a HOOKED requirement (a plain one is
		 * GenStateCompileClassAttr's own "Interfaces may only include hooked
		 * properties"). */
		if( iKind != GEN_MEMBER_CONST ){
			sMods.iFlags |= PH7_CLASS_ATTR_ABSTRACT;
		}
	}
	if( iKind == GEN_MEMBER_CONST ){
		return GenStateCompileClassConstant(&(*pGen),sMods.iProtection,sMods.iFlags,pClass);
	}
	if( iKind == GEN_MEMBER_METHOD ){
		/* An interface method is a SIGNATURE: it has no body to compile. */
		return GenStateCompileClassMethod(&(*pGen),sMods.iProtection,sMods.iFlags,
			(pClass->iFlags & PH7_CLASS_INTERFACE) ? FALSE : TRUE,pClass);
	}
	return GenStateCompileClassAttr(&(*pGen),sMods.iProtection,sMods.iFlags,pClass);
}
/*
 * Compile a PHP 8.4 property-hook list `{ get ...; set ...; }` following a
 * property declaration. Each hook body is synthesized into a hidden public
 * class method (__phl_hook_get_NAME / __phl_hook_set_NAME) so inheritance,
 * $this binding, and dispatch ride the ordinary method machinery; OP_MEMBER /
 * OP_STORE route reads and plain writes through them (a per-instance guard
 * makes $this->NAME inside a hook body address the raw backing slot — php's
 * rule that hooks see the backing store). `get => expr;` compiles as an
 * implicit return (the arrow-fn pattern); `set => expr;` compiles the same
 * and is flagged VM_FUNC_HOOK_SET_EXPR — the dispatcher assigns its return
 * value to the backing slot. A `set` without a parameter list receives the
 * implicit `$value` formal.
 * On entry pGen->pIn sits on '{'; on success it sits just past '}'.
 */
/*
 * Whether any token in [pStart, pEnd) spells `$this->NAME` (this property's own
 * name; `?->` and `::` member ops count too). php 8.4's virtual-vs-backed rule:
 * a hooked property is BACKED iff any of its OWN hook bodies references it by
 * name through $this — otherwise it is VIRTUAL: no backing store, no default
 * allowed, excluded from the raw object surfaces.
 */
static int GenStateHookBodyRefsProp(SyToken *pStart,SyToken *pEnd,const SyString *pName)
{
	SyToken *p;
	for( p = pStart ; p + 1 < pEnd ; p++ ){
		if( (p->nType & PH7_TK_DOLLAR) == 0 ){
			continue;
		}
		/* `$this->NAME` (also `?->`/`::`) */
		if( p + 3 < pEnd
		 && (p[1].nType & (PH7_TK_ID|PH7_TK_KEYWORD)) != 0
		 && p[1].sData.nByte == sizeof("this")-1
		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)"this",sizeof("this")-1) == 0
		 && GenStateTokenIsMemberOp(&p[2])
		 && (p[3].nType & (PH7_TK_ID|PH7_TK_KEYWORD)) != 0
		 && p[3].sData.nByte == pName->nByte
		 && SyMemcmp((const void *)p[3].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){
			return 1;
		}
		/* `parent::$NAME` (the parent::$x::get() hook-call form): the parent
		 * hook operates on the shared per-instance backing store, so the
		 * property is backed (php compiles a default alongside it). */
		if( p > pStart
		 && GenStateTokenIsMemberOp(&p[-1])
		 && (p[1].nType & (PH7_TK_ID|PH7_TK_KEYWORD)) != 0
		 && p[1].sData.nByte == pName->nByte
		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){
			return 1;
		}
	}
	return 0;
}
/*
 * True when p opens php 8.4's parent-hook call form
 * `parent :: $ NAME :: get|set (` (7 tokens through the '(').
 */
static int GenStateIsParentHookCallAt(SyToken *p,SyToken *pEnd)
{
	return p + 6 < pEnd
	 && (p->nType & (PH7_TK_ID|PH7_TK_KEYWORD)) != 0
	 && p->sData.nByte == sizeof("parent")-1
	 && SyMemcmp((const void *)p->sData.zString,(const void *)"parent",sizeof("parent")-1) == 0
	 && GenStateTokenIsMemberOp(&p[1])
	 && (p[2].nType & PH7_TK_DOLLAR) != 0
	 && (p[3].nType & (PH7_TK_ID|PH7_TK_KEYWORD)) != 0
	 && GenStateTokenIsMemberOp(&p[4])
	 && (p[5].nType & (PH7_TK_ID|PH7_TK_KEYWORD)) != 0
	 && p[5].sData.nByte == 3
	 && (SyMemcmp((const void *)p[5].sData.zString,(const void *)"get",3) == 0
	  || SyMemcmp((const void *)p[5].sData.zString,(const void *)"set",3) == 0)
	 && (p[6].nType & PH7_TK_LPAREN) != 0;
}
/*
 * Rewrite php 8.4 `parent::$x::get(...)` / `parent::$x::set(...)` calls in a
 * hook body into calls of the parent class's synthesized hook method
 * (`parent::__phl_hook_get_x(...)`). Builds a token COPY into pCopy (only
 * called when GenStateIsParentHookCallAt matched somewhere in the range);
 * copied tokens keep pointing at source-owned lexeme storage, and the
 * synthesized method-name lexemes are VM-allocator owned. Returns SXRET_OK
 * or SXERR_MEM.
 */
static sxi32 GenStateRewriteParentHookCalls(ph7_gen_state *pGen,SySet *pCopy,
	SyToken *pStart,SyToken *pEnd)
{
	SyToken *p = pStart;
	while( p < pEnd ){
		if( GenStateIsParentHookCallAt(p,pEnd) ){
			SyToken sTok;
			char zName[384];
			sxu32 nName;
			char *zDup;
			/* `parent` `::` */
			SySetPut(pCopy,(const void *)&p[0]);
			SySetPut(pCopy,(const void *)&p[1]);
			nName = SyBufferFormat(zName,sizeof(zName),"__phl_hook_%.3s_%.*s",
				p[5].sData.zString,(int)p[3].sData.nByte,p[3].sData.zString);
			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);
			if( zDup == 0 ){
				return SXERR_MEM;
			}
			sTok = p[3]; /* keep the line info of the property name */
			sTok.nType = PH7_TK_ID;
			SyStringInitFromBuf(&sTok.sData,zDup,nName);
			sTok.pUserData = 0;
			SySetPut(pCopy,(const void *)&sTok);
			p += 6; /* continue at the '(' — arguments copy through unchanged */
			continue;
		}
		SySetPut(pCopy,(const void *)p);
		p++;
	}
	return SXRET_OK;
}
/*
 * A `get` hook's return type IS the property's declared type — php never lets a
 * hook declare one, so there is nothing else it could be, and that is what makes
 * `public int $p { get { return "5"; } }` answer int(5) and a `get` returning
 * "x" raise `C::$p::get(): Return value must be of type int, string returned`.
 * Installing it on the synthesized method reuses the return enforcement that
 * already matches php byte for byte (the same move the __toString implicit
 * `string` type made), and lets the compile-time bare-`return;` check see the
 * hook as the typed function php treats it as.
 *
 * The union alternatives are SHARED, not copied: their class-name SyStrings are
 * VM-allocator owned and outlive both records, which is the same contract
 * GenStateCopyTypeToAttr relies on.
 */
static void GenStateHookGetReturnType(ph7_vm_func *pFunc,ph7_class_attr *pAttr)
{
	if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){
		return; /* untyped property: the hook is untyped too */
	}
	pFunc->nReturnType = pAttr->nType;
	pFunc->sReturnClass = pAttr->sClass;
	pFunc->sReturnTypeName = pAttr->sTypeName;
	if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){
		pFunc->iFlags |= VM_FUNC_RETURN_NULLABLE;
	}
	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){
		sxu32 i;
		for( i = 0 ; i < SySetUsed(&pAttr->aUnionAlts) ; i++ ){
			SySetPut(&pFunc->aReturnUnion,SySetAt(&pAttr->aUnionAlts,i));
		}
	}
}
/*
 * The mirror for a `set` hook. php gives it two implicit pieces of signature:
 * the implicit `$value` formal carries the PROPERTY's declared type (so
 * `public int $p { set { ... } }` coerces `$o->p = "7"` to int(7) and rejects
 * "abc" with `C::$p::set(): Argument #1 ($value) must be of type int, string
 * given`), and the hook itself returns `void` — a set hook that returns a value
 * is php's `A void method must not return a value`, on an untyped property too.
 * An EXPLICIT `set(T $v)` keeps its own declared type; only the implicit formal
 * is typed from the property, which is why the caller passes pValueArg only
 * when it synthesized one.
 */
static void GenStateHookSetSignature(ph7_gen_state *pGen,ph7_vm_func *pFunc,
	ph7_class_attr *pAttr,ph7_vm_func_arg *pValueArg)
{
	char *zVoid;
	if( pValueArg && (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){
		pValueArg->nType = pAttr->nType;
		pValueArg->sClass = pAttr->sClass;
		pValueArg->sTypeName = pAttr->sTypeName;
		if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){
			pValueArg->iFlags |= VM_FUNC_ARG_NULLABLE;
		}
		if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){
			sxu32 i;
			pValueArg->iFlags |= VM_FUNC_ARG_UNION;
			for( i = 0 ; i < SySetUsed(&pAttr->aUnionAlts) ; i++ ){
				SySetPut(&pValueArg->aUnionAlts,SySetAt(&pAttr->aUnionAlts,i));
			}
		}
	}
	pFunc->nReturnType = MEMOBJ_VOID;
	zVoid = SyMemBackendStrDup(&pGen->pVm->sAllocator,"void",sizeof("void")-1);
	if( zVoid ){
		SyStringInitFromBuf(&pFunc->sReturnTypeName,zVoid,sizeof("void")-1);
	}
}
PH7_PRIVATE sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr)
{
	sxu32 nLine = pGen->pIn->nLine;
	sxi32 rc;
	int bRefsSelf = 0;
	pGen->pIn++; /* Jump '{' */
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) == 0 ){
		char zHook[384];
		SyString sHookName;
		ph7_class_method *pMeth;
		int bGet;
		sxu32 nHLine = pGen->pIn->nLine;
		if( pGen->pIn->nType & PH7_TK_SEMI ){
			pGen->pIn++; /* stray ';' between hooks */
			continue;
		}
		if( pGen->pIn->nType & PH7_TK_AMPER ){
			/* by-reference get hook: not modeled (loud, recorded) */
			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,
				"By-reference property hooks are not supported for %z::$%z",
				&pClass->sName,&pAttr->sName);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			return SXERR_CORRUPT;
		}
		if( (pGen->pIn->nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
			goto HookSyntax;
		}
		if( pGen->pIn->sData.nByte == 3
		 && SyStrnicmp(pGen->pIn->sData.zString,"get",3) == 0 ){
			bGet = 1;
		}else if( pGen->pIn->sData.nByte == 3
		 && SyStrnicmp(pGen->pIn->sData.zString,"set",3) == 0 ){
			bGet = 0;
		}else{
			goto HookSyntax;
		}
		pGen->pIn++; /* Jump 'get'/'set' */
		sHookName.zString = zHook;
		sHookName.nByte = SyBufferFormat(zHook,sizeof(zHook),"__phl_hook_%s_%z",
			bGet ? "get" : "set",&pAttr->sName);
		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI|PH7_TK_CCB)) ){
			/* Bare `get;` / `set;` — an ABSTRACT hook declaration (php 8.4):
			 * legal only on an `abstract` property or inside an interface. The
			 * synthesized method carries PH7_CLASS_ATTR_ABSTRACT and rides the
			 * existing must-implement machinery; a concrete hook override (or a
			 * plain property, see GenStateCheckAbstractMethods) satisfies it. */
			if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0
			 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){
				rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,
					"Non-abstract property hook must have a body");
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				return SXERR_CORRUPT;
			}
			pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,
				PH7_CLASS_PROT_PUBLIC,PH7_CLASS_ATTR_ABSTRACT,0);
			if( pMeth == 0 ){
				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");
				return SXERR_ABORT;
			}
			pMeth->sFunc.nLine = nHLine;
			if( bGet ){
				GenStateHookGetReturnType(&pMeth->sFunc,pAttr);
			}
			if( !bGet ){
				/* The implicit `$value` formal keeps the stub's signature
				 * compatible with concrete set-hook implementations (which
				 * always carry one parameter). It takes the PROPERTY's declared
				 * type (php: the abstract set's parameter type IS the property
				 * type), so the override contravariance check accepts a typed
				 * `set(int $v)` implementation on an `int $x` requirement. */
				ph7_vm_func_arg sVArg;
				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);
				if( zVName == 0 ){
					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");
					return SXERR_ABORT;
				}
				SyZero(&sVArg,sizeof(ph7_vm_func_arg));
				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);
				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));
				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));
				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));
				GenStateHookSetSignature(&(*pGen),&pMeth->sFunc,pAttr,&sVArg);
				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);
			}
			rc = PH7_ClassInstallMethod(pClass,pMeth);
			if( rc != SXRET_OK ){
				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");
				return SXERR_ABORT;
			}
			pAttr->iFlags |= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;
			continue; /* the loop consumes the ';' as a stray separator */
		}
		if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0
		 || (pClass->iFlags & PH7_CLASS_INTERFACE) != 0 ){
			/* php: an abstract/interface property hook cannot carry a body */
			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,
				"Abstract property hook cannot have body");
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			return SXERR_CORRUPT;
		}
		pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,
			PH7_CLASS_PROT_PUBLIC,0,0);
		if( pMeth == 0 ){
			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");
			return SXERR_ABORT;
		}
		pMeth->sFunc.nLine = nHLine;
		if( bGet ){
			GenStateHookGetReturnType(&pMeth->sFunc,pAttr);
		}
		if( !bGet ){
			/* Parameter list: explicit `set(Type $v)` or the implicit `$value` */
			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){
				SyToken *pRp = 0;
				pGen->pIn++;
				PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pRp);
				if( pRp >= pGen->pEnd ){
					goto HookSyntax;
				}
				if( pGen->pIn < pRp ){
					rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pRp,0,0);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
				}
				pGen->pIn = &pRp[1];
			}
			if( SySetUsed(&pMeth->sFunc.aArgs) < 1 ){
				/* Implicit $value formal */
				ph7_vm_func_arg sVArg;
				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);
				if( zVName == 0 ){
					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");
					return SXERR_ABORT;
				}
				SyZero(&sVArg,sizeof(ph7_vm_func_arg));
				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);
				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));
				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));
				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));
				SyStringInitFromBuf(&sVArg.sTypeName,0,0);
				GenStateHookSetSignature(&(*pGen),&pMeth->sFunc,pAttr,&sVArg);
				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);
			}else{
				/* An EXPLICIT `set(T $v)` keeps its own parameter type; only the
				 * void return is implicit. */
				GenStateHookSetSignature(&(*pGen),&pMeth->sFunc,pAttr,0);
			}
		}
		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){
			/* Block body */
			SyToken *pBodyStart = pGen->pIn;
			SyToken *pCloser = 0;
			int bParentCall = 0;
			PH7_DelimitNestedTokens(&pBodyStart[1],pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pCloser);
			if( pCloser < pGen->pEnd ){
				SyToken *pScan;
				for( pScan = &pBodyStart[1] ; pScan < pCloser ; pScan++ ){
					if( GenStateIsParentHookCallAt(pScan,pCloser) ){
						bParentCall = 1;
						break;
					}
				}
			}
			if( bParentCall ){
				/* `parent::$x::get()` inside the body: compile a REWRITTEN copy
				 * of the body tokens (the call becomes the parent's synthesized
				 * hook method), then continue past the original body. */
				SySet sBody;
				SyToken *pSavedEnd = pGen->pEnd;
				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));
				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,&pCloser[1]);
				if( rc != SXRET_OK ){
					SySetRelease(&sBody);
					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");
					return SXERR_ABORT;
				}
				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);
				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];
				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);
				pGen->pIn = &pCloser[1];
				pGen->pEnd = pSavedEnd;
				SySetRelease(&sBody);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				pMeth->sFunc.nEndLine = pCloser->nLine;
			}else{
				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;
			}
			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){
				bRefsSelf = 1;
			}
		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ARRAY_OP) ){
			/* `=> expr;` — implicit-return body (the arrow-fn pattern) */
			GenBlock *pBlock;
			SySet *pInstrContainer;
			SyToken *pBodyStart;
			SyToken *pExprEnd;
			SyToken *pSavedEnd = 0;
			SySet sBody;
			int bParentCall = 0;
			pGen->pIn++; /* Jump '=>' */
			pBodyStart = pGen->pIn;
			/* Delimit the expression (first top-level ';', or a closer that
			 * would end the enclosing hook list) and rewrite any
			 * `parent::$x::get()` calls into the parent's synthesized hook
			 * method on a token copy. */
			{
				sxi32 iNest = 0;
				pExprEnd = pBodyStart;
				while( pExprEnd < pGen->pEnd ){
					if( pExprEnd->nType & (PH7_TK_LPAREN|PH7_TK_OSB|PH7_TK_OCB) ){
						iNest++;
					}else if( pExprEnd->nType & (PH7_TK_RPAREN|PH7_TK_CSB|PH7_TK_CCB) ){
						if( iNest <= 0 ){
							break;
						}
						iNest--;
					}else if( iNest <= 0 && (pExprEnd->nType & PH7_TK_SEMI) ){
						break;
					}
					pExprEnd++;
				}
			}
			{
				SyToken *pScan;
				for( pScan = pBodyStart ; pScan < pExprEnd ; pScan++ ){
					if( GenStateIsParentHookCallAt(pScan,pExprEnd) ){
						bParentCall = 1;
						break;
					}
				}
			}
			if( bParentCall ){
				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));
				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,pExprEnd);
				if( rc != SXRET_OK ){
					SySetRelease(&sBody);
					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");
					return SXERR_ABORT;
				}
				pSavedEnd = pGen->pEnd;
				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);
				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];
			}
			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED|GEN_BLOCK_FUNC,
				PH7_VmInstrLength(pGen->pVm),&pMeth->sFunc,&pBlock);
			if( rc != SXRET_OK ){
				PH7_GenCompileError(pGen,E_ERROR,nHLine,"PH7 engine is running out-of-memory");
				return SXERR_ABORT;
			}
			pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);
			PH7_VmSetByteCodeContainer(pGen->pVm,&pMeth->sFunc.aByteCode);
			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);
			GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);
			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
			GenStateLeaveBlock(&(*pGen),0);
			if( bParentCall ){
				pGen->pIn = pExprEnd; /* land on the original ';' */
				pGen->pEnd = pSavedEnd;
				SySetRelease(&sBody);
			}
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			pMeth->sFunc.nEndLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nHLine;
			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){
				bRefsSelf = 1;
			}
			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){
				pGen->pIn++; /* Jump ';' */
			}
			if( !bGet ){
				/* `set => expr` assigns the expression to the backing store:
				 * the dispatcher consumes the implicit return value — which
				 * also makes the property BACKED (php: the shorthand is sugar
				 * for `$this->NAME = expr`). */
				pMeth->sFunc.iFlags |= VM_FUNC_HOOK_SET_EXPR;
				bRefsSelf = 1;
			}
		}else{
			goto HookSyntax;
		}
		rc = PH7_ClassInstallMethod(pClass,pMeth);
		if( rc != SXRET_OK ){
			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");
			return SXERR_ABORT;
		}
		pAttr->iFlags |= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;
	}
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_CCB) == 0 ){
		goto HookSyntax;
	}
	pGen->pIn++; /* Jump '}' */
	if( !bRefsSelf ){
		/* php 8.4 virtual-vs-backed: no hook body referenced `$this->NAME`, so
		 * this property is VIRTUAL — php gives it no backing store and forbids
		 * a default value (compile fatal, php's exact wording). */
		pAttr->iFlags |= PH7_CLASS_ATTR_HOOK_VIRTUAL;
		if( SySetUsed(&pAttr->aByteCode) > 0 ){
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
				"Cannot specify default value for virtual hooked property %z::$%z",
				&pClass->sName,&pAttr->sName);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			return SXERR_CORRUPT;
		}
	}
	return SXRET_OK;
HookSyntax:
	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
		"Invalid property hook declaration for %z::$%z: expecting 'get' or 'set'",
		&pClass->sName,&pAttr->sName);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	return SXERR_CORRUPT;
}
/* php's #[\Override] verification, defined with the rest of the class-link
 * checks below; both compilers (class and interface) drive the same pair. */
static sxi32 GenStateCollectOverrides(ph7_gen_state *pGen,ph7_class *pClass,
	SySet *pMeths,SySet *pProps);
static sxi32 GenStateCheckOverrides(ph7_gen_state *pGen,ph7_class *pClass,
	SySet *pMeths,SySet *pProps);
/*
 * Compile an object interface.
 *  According to the PHP language reference manual
 *   Object Interfaces:
 *   Object interfaces allow you to create code which specifies which methods
 *   a class must implement, without having to define how these methods are handled.
 *   Interfaces are defined using the interface keyword, in the same way as a standard
 *   class, but without any of the methods having their contents defined.
 *   All methods declared in an interface must be public, this is the nature of an interface.
 */
PH7_PRIVATE sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)
{
	sxu32 nLine = pGen->pIn->nLine;
	ph7_class *pClass,*pBase;
	ph7_class *pSavedCurClass = pGen->pCurClass; /* restored at 'done' */
	ph7_class *pSavedCurBase = pGen->pCurBase;   /* ...and its base, see pCurBase */
	SyToken *pEnd,*pTmp;
	SyString *pName;
	SySet aOvMeth,aOvProp;   /* the #[\Override] claims this interface DECLARED */
	SySet aExtraParents;     /* `extends A, S, T`: every parent after the first */
	sxu32 nErrEntry = pGen->nErr; /* errors already reported when this one started */
	sxi32 nKwrd;
	sxi32 rc;
	{
		/* Deferral gate: parent interfaces may need an autoloader
		 * that has not run yet. */
		sxi32 rcDefer;
		if( GenStateMaybeDeferClass(pGen,0,PH7_DEFER_KIND_INTERFACE,&rcDefer) ){
			return rcDefer == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;
		}
	}
	/* Jump the 'interface' keyword */
	pGen->pIn++;
	/* Extract interface name */
	pName = &pGen->pIn->sData;
	/* Advance the stream cursor */
	pGen->pIn++;
	/* Build FQN and obtain a raw class */ {
		SyBlob sFQN;
		SyString sFQNStr;
		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);
		GenStateBuildFQN(pGen,pName,&sFQN);
		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));
		/* php refuses a declaration whose short name a local `use` already took. */
		if( GenStateGuardImportRedeclare(pGen,0,pName,&sFQNStr,nLine) == SXERR_ABORT ){
			SyBlobRelease(&sFQN);
			return SXERR_ABORT;
		}
		GenStateRecordDeclaredName(pGen,0,&sFQNStr);
		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);
		SyBlobRelease(&sFQN);
	}
	if( pClass == 0 ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);
	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	/* Mark as an interface (PH7_NewRawClass may have set INTERNAL) */
	pClass->iFlags |= PH7_CLASS_INTERFACE;
	if( GenStateCheckAttrPlacement(&(*pGen),&pClass->aAttrs,1,1,
			&pClass->sName,pClass->iFlags) == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	/* Assume no base class is given */
	pBase = 0;
	SySetInit(&aExtraParents,&pGen->pVm->sAllocator,sizeof(ph7_class *));
	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){
		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a, c, … */ ){
			/* php lets an interface extend SEVERAL parent interfaces. The first
			 * becomes pBase (single-inheritance chain, hDerived); every extra one
			 * is recorded via PH7_ClassImplement so it lands in aInterface — the
			 * runtime subtype walk (VmInterfaceReaches) follows both. */
			pGen->pIn++;
			for(;;){
				SyBlob sResolved;
				SyString sBaseName;
				sxu32 nRefLine;
				ph7_class *pParent;
				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;
				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);
				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){
					SyBlobRelease(&sResolved);
					rc = PH7_GenCompileError(pGen,E_PARSE,nLine,
						"Expected 'interface_name' after 'extends' keyword inside interface '%z'",
						pName);
					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
					return SXRET_OK;
				}
				pParent = PH7_VmExtractClass(pGen->pVm,
					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);
				SyStringInitFromBuf(&sBaseName,
					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));
				/* Only interfaces is allowed */
				while( pParent && (pParent->iFlags & PH7_CLASS_INTERFACE) == 0 ){
					pParent = pParent->pNextName;
				}
				if( pParent == 0 ){
					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,
						"Nonexistent base interface '%z'",&sBaseName);
					if( rc == SXERR_ABORT ){
						SyBlobRelease(&sResolved);
						return SXERR_ABORT;
					}
				}else if( pBase == 0 ){
					/* First parent → single-inheritance base */
					pBase = pParent;
				}else{
					/* Additional parent → COLLECTED, and applied after the body like
					 * the first one is. Copying its members here put them in hMethod
					 * and hConst before the body was read, so the interface's own
					 * `public function g();` collided with the very name it was
					 * restating: `interface B extends A, S` could redeclare A's
					 * members and not S's ("Cannot redeclare B::g()"), which is a
					 * declaration php accepts and php-di writes. */
					SySetPut(&aExtraParents,(const void *)&pParent);
				}
				SyBlobRelease(&sResolved);
				/* Continue on a comma-separated list */
				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){
					pGen->pIn++;
					continue;
				}
				break;
			}
		}
	}
	if( pGen->pIn >= pGen->pEnd  || (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Expected '{' after interface '%z' definition",pName);
		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	pGen->pIn++; /* Jump the leading curly brace */
	pEnd = 0; /* cc warning */
	/* Delimit the interface body */
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);
	if( pEnd >= pGen->pEnd ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Missing '}' after interface '%z' definition",pName);
		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	/* The delimiter token is the interface body's closing brace */
	pClass->nEndLine = pEnd->nLine;
	/* Swap token stream */
	pTmp = pGen->pEnd;
	pGen->pEnd = pEnd;
	/* This interface is now the lexical class for its body (see pCurClass) — a
	 * const default here is not a trait, so __TRAIT__ stays "". */
	pGen->pCurClass = pClass;
	pGen->pCurBase = 0; /* php gives an interface no `parent`, however many it extends */
	/* Start the parse process
	 * Note (According to the PHP reference manual):
	 *  Only constants and function signatures(without body) are allowed.
	 *  Only 'public' visibility is allowed.
	 */
	for(;;){
		/* Jump leading/trailing semi-colons */
		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){
			pGen->pIn++;
		}
		if( pGen->pIn >= pGen->pEnd ){
			/* End of interface body */
			break;
		}
		/* Bind a directly-preceding docblock to this member */
		GenStateSetPendingDoc(&(*pGen));
		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0
			&& !GenStateIsReadonly(pGen->pIn) ){
			rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,
				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",
				&pGen->pIn->sData,pName);
			if( rc == SXERR_ABORT ){
				/* Error count limit reached,abort immediately */
				return SXERR_ABORT;
			}
			goto done;
		}
		/* The member, through the shared modifier run -- which is where an
		 * interface's own rules live now (public-only, no `final`, no written
		 * `abstract`, and a property that may only be a hooked requirement). */
		rc = GenStateCompileMember(&(*pGen),pClass,"interface");
		if( rc != SXRET_OK ){
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto done;
		}
	}
	/* An interface method may claim #[\Override] too, against the interfaces this
	 * one extends -- collected before the inherit copies theirs in. */
	GenStateCollectOverrides(&(*pGen),pClass,&aOvMeth,&aOvProp);
	/* Reject a php-fatal redeclaration before hoisting the interface. An
	 * interface that EXTENDS another is not early-bound, exactly as a class
	 * that implements one is not. */
	if( GenStateGuardClassRedeclaration(pGen,pClass,
			pBase == 0 && SySetUsed(&aExtraParents) == 0) == SXERR_ABORT ){
		SySetRelease(&aOvMeth);
		SySetRelease(&aOvProp);
		return SXERR_ABORT;
	}
	/* Every method this interface DECLARES, judged against the same name in each
	 * parent -- hMethod holds only its own declarations until the inherits below
	 * run, so this is the one moment the two sets are separable. php makes the
	 * check for a restated method whichever parent it came from; PHL made it for
	 * none of them, so `interface B extends A { public function f(): int; }` over
	 * `A::f(): string` compiled in silence. */
	if( pGen->nErr == nErrEntry ){
		if( pBase && PH7_ClassInterfaceCheckRedeclare(&(*pGen),pClass,pBase) == SXERR_ABORT ){
			SySetRelease(&aOvMeth);
			SySetRelease(&aOvProp);
			SySetRelease(&aExtraParents);
			return SXERR_ABORT;
		}
		{
			ph7_class **apExtra = (ph7_class **)SySetBasePtr(&aExtraParents);
			sxu32 nExtra;
			for( nExtra = 0 ; nExtra < SySetUsed(&aExtraParents) ; ++nExtra ){
				if( PH7_ClassInterfaceCheckRedeclare(&(*pGen),pClass,apExtra[nExtra])
					== SXERR_ABORT ){
					SySetRelease(&aOvMeth);
					SySetRelease(&aOvProp);
					SySetRelease(&aExtraParents);
					return SXERR_ABORT;
				}
			}
		}
	}
	/* Install the interface */
	rc = PH7_VmInstallClass(pGen->pVm,pClass);
	if( rc == SXRET_OK && pBase ){
		/* Inherit from the base interface */
		rc = PH7_ClassInterfaceInherit(pClass,pBase);
	}
	if( rc == SXRET_OK ){
		/* ...and from every parent after the first, whose members are copied only
		 * where this interface declared none of its own. */
		ph7_class **apExtra = (ph7_class **)SySetBasePtr(&aExtraParents);
		sxu32 nExtra;
		for( nExtra = 0 ; rc == SXRET_OK && nExtra < SySetUsed(&aExtraParents) ; ++nExtra ){
			rc = PH7_ClassImplement(pClass,apExtra[nExtra]);
		}
	}
	SySetRelease(&aExtraParents);
	if( rc == SXRET_OK && pGen->nErr == nErrEntry
	 && GenStateCheckOverrides(&(*pGen),pClass,&aOvMeth,&aOvProp) == SXERR_ABORT ){
		SySetRelease(&aOvMeth);
		SySetRelease(&aOvProp);
		return SXERR_ABORT;
	}
	SySetRelease(&aOvMeth);
	SySetRelease(&aOvProp);
	if( rc != SXRET_OK ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
done:
	SySetRelease(&aExtraParents);
	pGen->pCurClass = pSavedCurClass;
	pGen->pCurBase = pSavedCurBase;
	/* Point beyond the interface body */
	pGen->pIn  = &pEnd[1];
	pGen->pEnd = pTmp;
	return PH7_OK;
}
/*
 * Compile a user-defined class.
 * According to the PHP language reference manual
 *  class
 *  Basic class definitions begin with the keyword class, followed by a class
 *  name, followed by a pair of curly braces which enclose the definitions
 *  of the properties and methods belonging to the class.
 *  The class name can be any valid label which is a not a PHP reserved word.
 *  A valid class name starts with a letter or underscore, followed by any number
 *  of letters, numbers, or underscores. As a regular expression, it would be expressed
 *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.
 *  A class may contain its own constants, variables (called "properties"), and functions
 *  (called "methods").
 */
/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */
typedef struct TraitUseEntry TraitUseEntry;
struct TraitUseEntry {
	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */
	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */
	SyToken *pResolvEnd;       /* End of resolution block tokens */
};
/*
 * Validate that methods implementing interface contracts have compatible
 * signatures: public visibility and at least as many parameters as declared.
 */
static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)
{
	ph7_class **apIface;
	sxu32 nIface,i;
	sxi32 rc;
	if( pClass->iFlags & (PH7_CLASS_INTERFACE|PH7_CLASS_TRAIT) ){
		return SXRET_OK;
	}
	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);
	nIface = SySetUsed(&pClass->aInterface);
	for(i = 0; i < nIface; i++){
		ph7_class *pIface = apIface[i];
		SyHashEntry *pEntry;
		SyHashResetLoopCursor(&pIface->hMethod);
		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){
			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;
			ph7_class_method *pImplMeth;
			SyString *pMName = &pIfaceMeth->sFunc.sName;
			/* Find the implementing method in the class */
			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);
			if( pImplMeth == 0 || (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){
				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */
			}
			/* Check visibility: interface methods must be implemented as public */
			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){
				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,
					"Access level to %z::%z() must be public (as in class %z)",
					&pClass->sName,pMName,&pIface->sName);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			/* Signature compatibility. php checks an implementation against the
			 * interface's declaration with the very rule it checks an override by --
			 * one shared body, so the variance half is not the class hierarchy's
			 * alone and the fatal reads the same. This site used to count PARAMETERS
			 * only, and word the two declarations as bare `$name` lists.
			 *
			 * A CONSTRUCTOR is not exempt here: php exempts an INHERITED one from
			 * variance, but an interface that declares `__construct` constrains every
			 * implementor's. */
			if( PH7_ClassCheckOverrideCompat(&(*pGen),pIface,pClass,pIfaceMeth,pImplMeth,0)
				== SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}
	}
	return SXRET_OK;
}
/*
 * An abstract property-hook stub (__phl_hook_{get,set}_NAME) is satisfied by
 * the class declaring a PLAIN (non-abstract, non-hooked) property NAME: php
 * lets a plain property implement `{ get; set; }` requirements — its raw
 * read/write IS the default get/set. A concrete hook override replaced the
 * stub in hMethod already, so a surviving stub next to a HOOKED property
 * means that specific hook is still missing.
 */
static int GenStateAbstractHookSatisfied(ph7_class *pClass,const SyString *pMName)
{
	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;
	ph7_class_attr *pProp;
	if( pMName->nByte <= nPfx
	 || (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) != 0
	  && SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) != 0) ){
		return 0; /* not a hook stub */
	}
	pProp = PH7_ClassExtractAttribute(pClass,&pMName->zString[nPfx],pMName->nByte - nPfx);
	return pProp != 0
		&& (pProp->iFlags & (PH7_CLASS_ATTR_ABSTRACT|PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT
			|PH7_CLASS_ATTR_HOOK_GET|PH7_CLASS_ATTR_HOOK_SET)) == 0;
}
/*
 * Append an abstract member's display name to the message blob, translating a
 * property-hook stub (__phl_hook_get_x) to php's `$x::get` form.
 */
static void GenStateAppendAbstractMemberName(SyBlob *pMsg,const SyString *pMName)
{
	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;
	if( pMName->nByte > nPfx
	 && (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) == 0
	  || SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) == 0) ){
		SyBlobAppend(pMsg,"$",1);
		SyBlobAppend(pMsg,(const void *)&pMName->zString[nPfx],pMName->nByte - nPfx);
		SyBlobAppend(pMsg,"::",2);
		SyBlobAppend(pMsg,(const void *)&pMName->zString[sizeof("__phl_hook_")-1],3);
		return;
	}
	SyBlobAppend(pMsg,(const void *)pMName->zString,pMName->nByte);
}
/*
 * ---------------------------------------------------------------------------
 * php's `#[\Override]` (8.3, widened to properties in 8.5).
 *
 * The attribute is a CLAIM the engine checks where the member is written: the
 * name must already exist above, so a typo, a renamed parent method or a base
 * class that dropped one is a fatal at the declaration instead of a method
 * nobody ever calls. php runs it at class LINK time, after inheritance, and its
 * rules are the inheritance rules rather than a name search:
 *
 *   - a PRIVATE parent member is not inherited, so it is not something to
 *     override;
 *   - the CONSTRUCTOR is exempt from php's inheritance signature check unless it
 *     is abstract (or an interface's), and #[\Override] follows that exemption —
 *     a concrete parent `__construct` does NOT satisfy the claim while an
 *     abstract one does;
 *   - a method matches CASE-INSENSITIVELY and a property case-SENSITIVELY, which
 *     is php's rule for the two namespaces everywhere else;
 *   - an interface counts for a method, at any depth and through any ancestor;
 *   - a TRAIT used by this very class does not: its method is the using class's
 *     own, and php reports the USING class's name when the claim fails.
 * ---------------------------------------------------------------------------
 */
#define GEN_OVERRIDE_ATTR "Override"
/* Does this member carry `#[\Override]`? The compiler resolves an attribute name
 * to its fully-qualified spelling and class names are case-insensitive, so one
 * case-folded compare against the whole set is the test. */
static int GenStateHasOverrideAttr(SySet *pAttrs)
{
	ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(pAttrs);
	sxu32 n;
	for( n = 0 ; n < SySetUsed(pAttrs) ; ++n ){
		if( aAttr[n].sName.nByte == sizeof(GEN_OVERRIDE_ATTR)-1
		 && SyStrnicmp(aAttr[n].sName.zString,GEN_OVERRIDE_ATTR,
			sizeof(GEN_OVERRIDE_ATTR)-1) == 0 ){
			return 1;
		}
	}
	return 0;
}
/* Does an interface reachable from pClass -- its own, or any ancestor's --
 * declare this method? An interface that extends others already carries their
 * stubs in its own table, so one level of lookup per interface is enough. */
static int GenStateIfaceDeclaresMethod(ph7_class *pClass,const SyString *pName)
{
	for( ; pClass ; pClass = pClass->pBase ){
		ph7_class **apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);
		sxu32 n;
		for( n = 0 ; n < SySetUsed(&pClass->aInterface) ; ++n ){
			if( apIface[n]
			 && PH7_ClassExtractMethod(apIface[n],pName->zString,pName->nByte) ){
				return 1;
			}
		}
	}
	return 0;
}
/* Is there a parent METHOD this one may claim to override? */
static int GenStateOverridesMethod(ph7_class *pClass,const SyString *pName)
{
	int bCtor = pName->nByte == sizeof("__construct")-1
		&& SyStrnicmp(pName->zString,"__construct",sizeof("__construct")-1) == 0;
	ph7_class *pWalk;
	for( pWalk = pClass->pBase ; pWalk ; pWalk = pWalk->pBase ){
		ph7_class_method *pMeth = PH7_ClassExtractMethod(pWalk,pName->zString,pName->nByte);
		if( pMeth == 0 || pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ){
			continue;
		}
		if( bCtor && (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){
			continue;   /* php exempts a concrete parent constructor */
		}
		return 1;
	}
	return GenStateIfaceDeclaresMethod(pClass,pName);
}
/* Is there a parent PROPERTY this one may claim to override? Interfaces declare
 * none, so this is the base chain alone. */
static int GenStateOverridesProp(ph7_class *pClass,const SyString *pName)
{
	ph7_class *pWalk;
	for( pWalk = pClass->pBase ; pWalk ; pWalk = pWalk->pBase ){
		ph7_class_attr *pAttr = PH7_ClassExtractAttribute(pWalk,pName->zString,pName->nByte);
		if( pAttr && pAttr->iProtection != PH7_CLASS_PROT_PRIVATE ){
			return 1;
		}
	}
	return 0;
}
/*
 * Verify every `#[\Override]` the class DECLARED, in php's order: the methods
 * first and then the properties, each in declaration order, and the first
 * failure is the whole diagnostic (it is a fatal).
 *
 * The two sets are collected BEFORE inheritance runs -- an inherited method
 * keeps the parent's attribute record and php does not re-check it there -- and
 * verified after, which is when the answer exists.
 */
static sxi32 GenStateCollectOverrides(ph7_gen_state *pGen,ph7_class *pClass,
	SySet *pMeths,SySet *pProps)
{
	static const sxu32 nHookPfx = sizeof("__phl_hook_get_")-1;
	SyHashEntry *pEntry;
	SySetInit(pMeths,&pGen->pVm->sAllocator,sizeof(ph7_class_method *));
	SySetInit(pProps,&pGen->pVm->sAllocator,sizeof(ph7_class_attr *));
	SyHashResetLoopCursor(&pClass->hMethod);
	while( (pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){
		ph7_class_method *pMeth = (ph7_class_method *)pEntry->pUserData;
		SyString *pName = &pMeth->sFunc.sName;
		/* A property hook is compiled to a method here and is a PROPERTY in php,
		 * so the property arm below owns its claim. */
		if( pName->nByte > nHookPfx
		 && SyMemcmp((const void *)pName->zString,(const void *)"__phl_hook_",
			sizeof("__phl_hook_")-1) == 0 ){
			continue;
		}
		if( GenStateHasOverrideAttr(&pMeth->sFunc.aAttrs) ){
			SySetPut(pMeths,(const void *)&pMeth);
		}
	}
	SyHashResetLoopCursor(&pClass->hAttr);
	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){
		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;
		if( GenStateHasOverrideAttr(&pAttr->aAttrs) ){
			SySetPut(pProps,(const void *)&pAttr);
		}
	}
	return SXRET_OK;
}
static sxi32 GenStateCheckOverrides(ph7_gen_state *pGen,ph7_class *pClass,
	SySet *pMeths,SySet *pProps)
{
	ph7_class_method **apMeth = (ph7_class_method **)SySetBasePtr(pMeths);
	ph7_class_attr **apProp = (ph7_class_attr **)SySetBasePtr(pProps);
	sxu32 n;
	/* hMethod is a LIFO iteration list (SyHashInsert), so the collected order is
	 * the REVERSE of the declaration order; hAttr is a FIFO one
	 * (SyHashInsertTail) and needs no such turn. php reports the first member it
	 * finds in declaration order and stops. */
	for( n = SySetUsed(pMeths) ; n > 0 ; --n ){
		ph7_class_method *pMeth = apMeth[n - 1];
		if( !GenStateOverridesMethod(pClass,&pMeth->sFunc.sName) ){
			return PH7_GenCompileError(&(*pGen),E_ERROR,pMeth->sFunc.nLine,
				"%z::%z() has #[\\Override] attribute, but no matching parent method exists",
				&pClass->sName,&pMeth->sFunc.sName);
		}
	}
	for( n = 0 ; n < SySetUsed(pProps) ; ++n ){
		if( !GenStateOverridesProp(pClass,&apProp[n]->sName) ){
			/* php reports the CLASS's line for a property, not the property's. */
			return PH7_GenCompileError(&(*pGen),E_ERROR,pClass->nLine,
				"%z::$%z has #[\\Override] attribute, but no matching parent property exists",
				&pClass->sName,&apProp[n]->sName);
		}
	}
	return SXRET_OK;
}
/*
 * Check that a concrete class has no remaining abstract methods.
 * If it does, emit a PHP-compatible fatal error listing them all.
 */
/*
 * The interface FURTHEST up that declares this method name. php attributes an
 * unimplemented method to the interface that first ASKED for it, and an
 * interface reaches its parents through two containers: pBase (the first name
 * after `extends`) and aInterface (every one after that). Following only pBase
 * stopped at the restating interface, so `interface B extends A, S` reported
 * `B::g` where php reports `S::g`.
 *
 * Depth-bounded like the Throwable walk beside it: an interface graph cannot
 * cycle (every parent is already compiled), and the bound costs nothing.
 */
static ph7_class * GenStateIfaceDeclaringAt(ph7_class *pIface,const SyString *pMName,int iDepth)
{
	ph7_class *pDeepest = 0;
	ph7_class **apUp;
	sxu32 i;
	if( pIface == 0 || iDepth > 32 ){
		return 0;
	}
	if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){
		pDeepest = pIface;
	}
	{
		ph7_class *pUp = GenStateIfaceDeclaringAt(pIface->pBase,pMName,iDepth + 1);
		if( pUp ){
			pDeepest = pUp;
		}
	}
	apUp = (ph7_class **)SySetBasePtr(&pIface->aInterface);
	for( i = 0 ; i < SySetUsed(&pIface->aInterface) ; ++i ){
		ph7_class *pUp = GenStateIfaceDeclaringAt(apUp[i],pMName,iDepth + 1);
		if( pUp ){
			pDeepest = pUp;
		}
	}
	return pDeepest;
}
static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)
{
	ph7_class_method *pMeth;
	SyHashEntry *pEntry;
	sxu32 nAbstract;
	SyBlob sMsg;
	sxi32 rc;
	/* Abstract classes, interfaces, and traits may have unimplemented methods */
	if( pClass->iFlags & (PH7_CLASS_ABSTRACT|PH7_CLASS_INTERFACE|PH7_CLASS_TRAIT) ){
		return SXRET_OK;
	}
	/* Count abstract methods */
	nAbstract = 0;
	SyHashResetLoopCursor(&pClass->hMethod);
	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){
		pMeth = (ph7_class_method *)pEntry->pUserData;
		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){
			if( GenStateAbstractHookSatisfied(pClass,&pMeth->sFunc.sName) ){
				continue; /* hook requirement met by a plain property (php) */
			}
			nAbstract++;
		}
	}
	if( nAbstract == 0 ){
		return SXRET_OK;
	}
	/* Build the error message listing all abstract methods with origins */
	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);
	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "
		"be declared abstract or implement the remaining method%s (",
		&pClass->sName,nAbstract,
		(nAbstract > 1 ? "s" : ""),
		(nAbstract > 1 ? "s" : ""));
	/* Second pass: list methods with origins */
	{
		sxu32 nListed = 0;
		SyHashResetLoopCursor(&pClass->hMethod);
		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){
			ph7_class *pOrigin = 0;
			SyString *pMName;
			pMeth = (ph7_class_method *)pEntry->pUserData;
			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){
				continue;
			}
			pMName = &pMeth->sFunc.sName;
			if( GenStateAbstractHookSatisfied(pClass,pMName) ){
				continue; /* hook requirement met by a plain property (php) */
			}
			if( nListed > 0 ){
				SyBlobAppend(&sMsg,", ",2);
			}
			/* Find the origin of this abstract method.
			 * PHP priority: interfaces (walking ancestors and interface
			 * inheritance chains) take precedence for interface-declared
			 * methods. Abstract class methods only win when the class
			 * itself declared the abstract method (not inherited from
			 * an interface). Trait methods are adopted into the using
			 * class's namespace.
			 */
			{
				ph7_class **apIface;
				ph7_class **apTrait;
				ph7_class *pWalk;
				sxu32 i;
				/* 1. Check parent chain for a natively-declared abstract method
				 * (one that was written in the class body, not inherited from an
				 * interface). PHP attributes origin to the declaring class.
				 */
				if( pClass->pBase ){
					pWalk = pClass->pBase;
					while( pWalk ){
						ph7_class_method *pParentMeth;
						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);
						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){
							/* Exclude methods that came from an interface anywhere
							 * in this class's ancestor chain.
							 */
							int fromIface = 0;
							ph7_class *pAnc = pWalk;
							while( pAnc ){
								ph7_class **apPI;
								sxu32 j;
								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);
								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){
									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){
										fromIface = 1;
										break;
									}
								}
								if( fromIface ) break;
								pAnc = pAnc->pBase;
							}
							if( !fromIface ){
								pOrigin = pWalk;
								break;
							}
						}
						pWalk = pWalk->pBase;
					}
				}
				/* 2. Check interfaces on class and all ancestors, walking
				 * each interface's own parent chain for the deepest origin.
				 */
				if( !pOrigin ){
					pWalk = pClass;
					while( pWalk && !pOrigin ){
						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);
						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){
							ph7_class *pDeepest = GenStateIfaceDeclaringAt(apIface[i],pMName,0);
							if( pDeepest ){
								pOrigin = pDeepest;
								break;
							}
						}
						pWalk = pWalk->pBase;
					}
				}
				/* 3. Trait methods are adopted into the class namespace in PHP */
				if( !pOrigin ){
					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);
					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){
						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){
							pOrigin = pClass;
							break;
						}
					}
				}
			}
			if( pOrigin ){
				SyBlobFormat(&sMsg,"%z::",&pOrigin->sName);
			}else{
				/* Origin is the class itself (trait method adopted into class namespace) */
				SyBlobFormat(&sMsg,"%z::",&pClass->sName);
			}
			GenStateAppendAbstractMemberName(&sMsg,pMName);
			nListed++;
		}
	}
	SyBlobAppend(&sMsg,")",1);
	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",
		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));
	SyBlobRelease(&sMsg);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	return SXRET_OK;
}
/*
 * Parse a class/interface name reference from the current token stream.
 * Handles an optional leading '\' (absolute) and multi-segment namespaced
 * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn
 * (which must be an initialized, empty SyBlob) and advances pGen->pIn past
 * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if
 * the stream has no valid name at the current position (pGen->pIn is left
 * untouched in that case so the caller can produce its own diagnostic).
 */
PH7_PRIVATE sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)
{
	int isAbsolute = 0;
	SyToken *pStart = pGen->pIn;
	SyBlob sName;
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){
		isAbsolute = 1;
		pGen->pIn++;
	}
	SyBlobInit(&sName,&pGen->pVm->sAllocator);
	/* `namespace\X` names the CURRENT namespace and is fully qualified from there. */
	if( !isAbsolute && GenStateNsRelPrefix(pGen,&pGen->pIn,pGen->pEnd,&sName) ){
		isAbsolute = 1;
	}
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
		SyBlobRelease(&sName);
		pGen->pIn = pStart;
		return SXERR_INVALID;
	}
	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);
	pGen->pIn++;
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&
		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID|PH7_TK_KEYWORD)) ){
		SyBlobAppend(&sName,"\\",1);
		pGen->pIn++;
		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);
		pGen->pIn++;
	}
	if( isAbsolute ){
		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));
	}else{
		SyString sRaw;
		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));
		GenStateResolveName(pGen,&sRaw,pFqn);
	}
	SyBlobRelease(&sName);
	return SXRET_OK;
}
/*
 * Return TRUE if pInterface is Throwable or transitively extends Throwable.
 * Walks both the interface `extends` chain (pBase) and any parent-interface
 * set (aInterface). Depth is counted for every traversal step — recursion
 * through aInterface *and* sibling iteration through pBase — so a cycle in
 * either direction cannot run unbounded.
 */
#define PH7_THROWABLE_WALK_MAX_DEPTH 64
static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)
{
	ph7_class **apParent;
	sxu32 n;
	while( pInterface ){
		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){
			return FALSE;
		}
		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&
			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){
			return TRUE;
		}
		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);
		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){
			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){
				return TRUE;
			}
		}
		pInterface = pInterface->pBase;
		iDepth++;
	}
	return FALSE;
}
static int GenStateInterfaceIsThrowable(ph7_class *pInterface)
{
	return GenStateInterfaceIsThrowableAt(pInterface,0);
}
/*
 * Return TRUE if pBase is (or transitively extends) the Exception or Error
 * base class. Used to enforce that user classes can only acquire Throwable
 * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.
 */
static int GenStateClassIsExceptionOrError(ph7_class *pBase)
{
	while( pBase ){
		if( pBase->sName.nByte == sizeof("Exception")-1 &&
			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){
			return TRUE;
		}
		if( pBase->sName.nByte == sizeof("Error")-1 &&
			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){
			return TRUE;
		}
		pBase = pBase->pBase;
	}
	return FALSE;
}
/*
 * Compile a single `case NAME [= value];` member of an enum body (PHP 8.1).
 * A case is stored as a class constant (PH7_CLASS_ATTR_CONSTANT|ENUMCASE) whose
 * aByteCode holds the BACKING value expression for backed enums (empty for pure
 * enums). The case's runtime value — the singleton instance — is materialized
 * lazily on first access (VmEnumMaterialize, vm.c), matching PHP's lazy
 * backing-value type/duplicate checks. Declaration order is recorded in
 * pClass->aEnumCases for cases().
 */
static sxi32 GenStateCompileEnumCase(ph7_gen_state *pGen,ph7_class *pClass)
{
	sxu32 nLine = pGen->pIn->nLine;
	SySet *pInstrContainer;
	ph7_class_attr *pCase;
	SyString *pName;
	sxi32 rc;
	pGen->pIn++; /* Jump the 'case' keyword */
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
			"Invalid enum case name inside enum '%z'",&pClass->sName);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	pName = &pGen->pIn->sData;
	/* Cases share the class-constant namespace (php: "Cannot redefine class constant") */
	if( SyHashGet(&pClass->hConst,(const void *)pName->zString,pName->nByte) != 0 ){
		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
			"Cannot redefine class constant %z::%z",&pClass->sName,pName);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	pCase = PH7_NewClassAttr(pGen->pVm,pName,pGen->pIn->nLine,PH7_CLASS_PROT_PUBLIC,
		PH7_CLASS_ATTR_CONSTANT|PH7_CLASS_ATTR_ENUMCASE);
	if( pCase == 0 ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
	GenStateConsumeDoc(&(*pGen),&pCase->sDoc);
	if( GenStateConsumeAttrs(&(*pGen),&pCase->aAttrs) == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	if( GenStateCheckAttrPlacement(&(*pGen),&pCase->aAttrs,16,16,0,0) == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	pGen->pIn++; /* Jump the case name */
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) ){
		if( pClass->nEnumBacking == 0 ){
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
				"Case %z of non-backed enum %z must not have a value",pName,&pClass->sName);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto Synchronize;
		}
		pGen->pIn++; /* Jump the equal sign */
		/* A backing value is a constant expression like any other: same rules, same
		 * first-offender sentence, and no `new` (it is stored as a class constant). */
		{
			const char *zCErr = PH7_GenStateConstExprError(pGen,0);
			if( zCErr ){
				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"%s",zCErr);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				goto Synchronize;
			}
		}
		/* Compile the backing value expression into the case's own container
		 * (same technique as class constants). */
		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);
		PH7_VmSetByteCodeContainer(pGen->pVm,&pCase->aByteCode);
		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);
		if( rc == SXERR_EMPTY ){
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
				"Empty value for enum case %z::%z",&pClass->sName,pName);
		}
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);
		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}else{
		if( pClass->nEnumBacking != 0 ){
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
				"Case %z of backed enum %z must have a value",pName,&pClass->sName);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto Synchronize;
		}
	}
	rc = PH7_ClassInstallAttr(pClass,pCase);
	if( rc != SXRET_OK ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
	SySetPut(&pClass->aEnumCases,(const void *)&pCase);
	return SXRET_OK;
Synchronize:
	/* Synchronize with the first semi-colon */
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){
		pGen->pIn++;
	}
	return SXERR_CORRUPT;
}
/*
 * Install the enum interface methods (PHP 8.1): cases() for every enum, plus
 * from()/tryFrom() for backed ones. They are NATIVE methods — the very same C
 * bodies an enum declared from C gets — because php's are internal: it reports
 * them as `<internal, prototype BackedEnum>` with no file and no line, and
 * declares `from(string|int $value): static` on the prototype rather than the
 * enum's own backing type.
 *
 * This used to synthesize PHP source forwarding to three global
 * `__phl_enum_*` thunks, which put those names in php's namespace and reported
 * every enum's three methods as `<user>` at the enum's own line.
 */
static sxi32 GenStateCompileEnumMethods(ph7_gen_state *pGen,ph7_class *pClass)
{
	if( PH7_InstallEnumInterfaceMethods(pGen->pVm,pClass) != SXRET_OK ){
		PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
	return SXRET_OK;
}
/*
 * Magic methods an enum may not declare (php 8.1, zend_enum.c list —
 * __call/__callStatic/__invoke stay allowed).
 */
static const char *azEnumBannedMagic[] = {
	"__construct","__destruct","__clone","__get","__set","__isset","__unset",
	"__toString","__sleep","__wakeup","__serialize","__unserialize","__set_state"
};
/*
 * Enum post-body validation + synthesis: reject declared properties (including
 * trait-imported ones) and banned magic methods, install the readonly `name`
 * (and, for backed enums, `value`) instance properties the case singletons
 * carry, and synthesize cases()/from()/tryFrom(). Runs after trait application
 * and before the class is installed.
 */
static sxi32 GenStateEnumFinalize(ph7_gen_state *pGen,ph7_class *pClass,sxu32 nLine)
{
	SyHashEntry *pEntry;
	sxi32 rc;
	sxu32 n;
	/* php: "Enum %s cannot include properties" */
	SyHashResetLoopCursor(&pClass->hAttr);
	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){
		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;
		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){
			rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine ? pAttr->nLine : nLine,
				"Enum %z cannot include properties",&pClass->sName);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			break;
		}
	}
	/* php: "Enum %s cannot include magic method %s" */
	for( n = 0 ; n < SX_ARRAYSIZE(azEnumBannedMagic) ; n++ ){
		if( SyHashGet(&pClass->hMethod,(const void *)azEnumBannedMagic[n],
			SyStrlen(azEnumBannedMagic[n])) != 0 ){
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
				"Enum %z cannot include magic method %s",&pClass->sName,azEnumBannedMagic[n]);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}
	}
	/* Install the case-singleton instance properties: readonly `name` (every
	 * enum) and `value` (backed only). Materialization (vm.c) fills them and
	 * clears the readonly write-once latch; user writes then raise php's
	 * "Cannot modify readonly property" through the normal store path. */
	{
		static const SyString sNameProp = { "name",sizeof("name")-1 };
		static const SyString sValueProp = { "value",sizeof("value")-1 };
		ph7_class_attr *pAttr;
		pAttr = PH7_NewClassAttr(pGen->pVm,&sNameProp,nLine,PH7_CLASS_PROT_PUBLIC,
			PH7_CLASS_ATTR_READONLY|PH7_CLASS_ATTR_TYPED);
		if( pAttr == 0 ){
			PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
			return SXERR_ABORT;
		}
		pAttr->nType = MEMOBJ_STRING;
		SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);
		PH7_ClassInstallAttr(pClass,pAttr);
		if( pClass->nEnumBacking != 0 ){
			pAttr = PH7_NewClassAttr(pGen->pVm,&sValueProp,nLine,PH7_CLASS_PROT_PUBLIC,
				PH7_CLASS_ATTR_READONLY|PH7_CLASS_ATTR_TYPED);
			if( pAttr == 0 ){
				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
				return SXERR_ABORT;
			}
			pAttr->nType = pClass->nEnumBacking;
			if( pClass->nEnumBacking == MEMOBJ_INT ){
				SyStringInitFromBuf(&pAttr->sTypeName,"int",sizeof("int")-1);
			}else{
				SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);
			}
			PH7_ClassInstallAttr(pClass,pAttr);
		}
	}
	return GenStateCompileEnumMethods(&(*pGen),pClass);
}
/*
 * Deferred class declarations (class/anonymous-class extending an
 * autoloaded parent).
 *
 * A class declaration compiles INLINE while its enclosing file compiles, so a
 * parent/interface/trait that an autoloader would provide is unreachable when
 * the autoloader's own spl_autoload_register() statement has not EXECUTED yet
 * (same-file registration, or `new class extends \App\Child {}` anywhere).
 * php's model has no such problem: a declaration with unresolved dependencies
 * is declared at its EXECUTION point, in statement order, not hoisted.
 *
 * These helpers reproduce that: before compiling a declaration, scan its
 * header (extends/implements) and body (depth-1 trait `use`) for referenced
 * names and try to resolve each (firing autoload exactly where the normal
 * compile would). If any name is still missing, the WHOLE declaration is
 * captured as re-compilable source — a reconstructed `namespace`/`use`-import/
 * doc/attribute/modifier prefix plus the declaration's raw text — recorded in
 * a VmDeferredClass, and OP_CLASS_DEFER is emitted at the declaration site.
 * At runtime (VmExecDeferredClass, vm_include.c) the autoloader is live: each
 * recorded name resolves or throws php's catchable `... not found` Error, and
 * the chunk re-compiles through VmEvalChunk. An anonymous class re-compiles
 * inside `if (false) { new ... }` (installing the class without instantiating
 * it) under its original synthesized name via pVm->sDeferAnonName; the site's
 * own OP_NEW then instantiates it with the site-compiled arguments.
 *
 * Behavior shifts only for declarations that previously died with the
 * compile-time "Nonexistent base class" fatal: they now follow php — succeed
 * when the autoloader is registered first, or throw php's catchable
 * `Class/Interface/Trait "X" not found` Error at the declaration point.
 * A deferred declaration's OTHER compile errors (a body syntax error) shift
 * from file-compile time to the declaration's execution — still loud, timing
 * differs from php (recorded).
 */
static void GenStateDeferEmitUses(SyBlob *pOut,SyHash *pTable,const char *zKind)
{
	SyHashEntry *pEntry;
	SyHashResetLoopCursor(pTable);
	while( (pEntry = SyHashGetNextEntry(pTable)) != 0 ){
		const char *zFqn = (const char *)pEntry->pUserData;
		if( zFqn ){
			SyBlobFormat(pOut,"use %s%s as %.*s;\n",zKind,zFqn,
				(int)pEntry->nKeyLen,(const char *)pEntry->pKey);
		}
	}
}
/*
 * Parse one class reference at *ppCur (bounded by pEnd) with the SAME
 * namespace/import resolution the real compile uses, and append it to pNames.
 * Advances *ppCur past the reference. Returns SXERR_INVALID on a malformed
 * reference (caller bails out of deferral and lets the normal path report).
 */
static sxi32 GenStateDeferRecordRef(ph7_gen_state *pGen,SyToken **ppCur,SyToken *pEnd,
	sxu8 cKind,SySet *pNames)
{
	SyToken *pSavedIn = pGen->pIn;
	SyToken *pSavedEnd = pGen->pEnd;
	SyBlob sFqn;
	VmDeferredReq sReq;
	char *zDup;
	sxi32 rc;
	SyBlobInit(&sFqn,&pGen->pVm->sAllocator);
	pGen->pIn = *ppCur;
	pGen->pEnd = pEnd;
	rc = GenStateParseClassReference(pGen,&sFqn);
	*ppCur = pGen->pIn;
	pGen->pIn = pSavedIn;
	pGen->pEnd = pSavedEnd;
	if( rc != SXRET_OK || SyBlobLength(&sFqn) < 1 ){
		SyBlobRelease(&sFqn);
		return SXERR_INVALID;
	}
	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,
		(const char *)SyBlobData(&sFqn),SyBlobLength(&sFqn));
	if( zDup == 0 ){
		SyBlobRelease(&sFqn);
		return SXERR_INVALID;
	}
	SyStringInitFromBuf(&sReq.sName,zDup,SyBlobLength(&sFqn));
	sReq.cKind = cKind;
	SySetPut(pNames,(const void *)&sReq);
	SyBlobRelease(&sFqn);
	return SXRET_OK;
}
/*
 * Look a class/interface/trait name up WITHOUT asking the autoloader: is this
 * name declared right now? php's early binding asks exactly this question --
 * zend_try_early_binding does a plain class-table lookup and gives up if the
 * parent is not there yet, because a compile-time autoload would run user code
 * in the middle of compiling a file.
 */
static ph7_class * GenStateFindDeclaredClass(ph7_vm *pVm,const char *zName,sxu32 nByte)
{
	SyHashEntry *pEntry;
	PH7_VmClassNameAnchor(&zName,&nByte);
	if( nByte < 1 ){
		return 0;
	}
	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);
	return pEntry ? (ph7_class *)pEntry->pUserData : 0;
}
/*
 * Scan the declaration whose keyword pGen->pIn sits on (class/enum/interface/
 * trait, or an anonymous `class(args)`) WITHOUT consuming tokens. Collects
 * every referenced dependency name, locates the body braces, and filters the
 * collected names down to the UNRESOLVABLE ones. SXRET_OK with an empty
 * pMissing set means "compile normally"; a non-empty set means "defer". Any
 * structural surprise returns SXERR_INVALID so the normal compile reports it.
 *
 * bNoAutoload picks which question the filter asks -- see its use below.
 */
static sxi32 GenStateScanDeferDeps(ph7_gen_state *pGen,int bAnon,int iSelfKind,
	SySet *pMissing,SyToken **ppBody,SyToken **ppBodyEnd,SyBlob *pSelfFqn,int bNoAutoload)
{
	SyToken *pCur = pGen->pIn; /* on the declaration keyword */
	SyToken *pEnd = pGen->pEnd;
	SySet aNames;
	sxi32 rc = SXRET_OK;
	*ppBody = *ppBodyEnd = 0;
	SySetInit(&aNames,&pGen->pVm->sAllocator,sizeof(VmDeferredReq));
	pCur++; /* Jump the keyword */
	if( bAnon ){
		if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){
			SyToken *pClose = 0;
			pCur++;
			PH7_DelimitNestedTokens(pCur,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);
			if( pClose == 0 || pClose >= pEnd ){
				SySetRelease(&aNames);
				return SXERR_INVALID;
			}
			pCur = &pClose[1];
		}
	}else{
		if( pCur >= pEnd || (pCur->nType & PH7_TK_ID) == 0 ){
			SySetRelease(&aNames);
			return SXERR_INVALID;
		}
		GenStateBuildFQN(pGen,&pCur->sData,pSelfFqn);
		pCur++;
	}
	/* Header: extends/implements lists up to the '{' (an enum's `: int` backing
	 * and any stray tokens pass through; malformed headers bail to the normal
	 * path's diagnostics). */
	while( pCur < pEnd && (pCur->nType & PH7_TK_OCB) == 0 ){
		int iKind = -1;
		if( pCur->nType & PH7_TK_KEYWORD ){
			sxi32 nKw = SX_PTR_TO_INT(pCur->pUserData);
			if( nKw == PH7_TKWRD_EXTENDS ){
				iKind = (iSelfKind == PH7_DEFER_KIND_INTERFACE)
					? PH7_DEFER_KIND_INTERFACE : PH7_DEFER_KIND_CLASS;
			}else if( nKw == PH7_TKWRD_IMPLEMENTS ){
				iKind = PH7_DEFER_KIND_INTERFACE;
			}
		}
		if( iKind < 0 ){
			pCur++;
			continue;
		}
		pCur++; /* Jump extends/implements */
		for(;;){
			if( GenStateDeferRecordRef(pGen,&pCur,pEnd,(sxu8)iKind,&aNames) != SXRET_OK ){
				SySetRelease(&aNames);
				return SXERR_INVALID;
			}
			if( pCur < pEnd && (pCur->nType & PH7_TK_COMMA) ){
				pCur++;
				continue;
			}
			break;
		}
	}
	if( pCur >= pEnd || (pCur->nType & PH7_TK_OCB) == 0 ){
		SySetRelease(&aNames);
		return SXERR_INVALID;
	}
	*ppBody = pCur;
	{
		SyToken *pClose = 0;
		PH7_DelimitNestedTokens(&pCur[1],pEnd,PH7_TK_OCB,PH7_TK_CCB,&pClose);
		if( pClose == 0 || pClose >= pEnd ){
			SySetRelease(&aNames);
			return SXERR_INVALID;
		}
		*ppBodyEnd = pClose;
	}
	/* Body: depth-1 trait `use Name[, Name]` statements. Statement position only
	 * (previous token one of '{' '}' ';'), so a closure's `use ($x)` — which
	 * follows a ')' — never matches. */
	{
		SyToken *p = &(*ppBody)[1];
		int bStmtPos = 1;
		sxi32 iDepth = 1;
		while( p < *ppBodyEnd ){
			if( p->nType & PH7_TK_OCB ){
				iDepth++;
				bStmtPos = 1;
				p++;
				continue;
			}
			if( p->nType & PH7_TK_CCB ){
				iDepth--;
				bStmtPos = 1;
				p++;
				continue;
			}
			if( p->nType & PH7_TK_SEMI ){
				bStmtPos = 1;
				p++;
				continue;
			}
			if( iDepth == 1 && bStmtPos && (p->nType & PH7_TK_KEYWORD)
			 && SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_USE ){
				p++;
				for(;;){
					if( GenStateDeferRecordRef(pGen,&p,*ppBodyEnd,PH7_DEFER_KIND_TRAIT,&aNames) != SXRET_OK ){
						SySetRelease(&aNames);
						return SXERR_INVALID;
					}
					if( p < *ppBodyEnd && (p->nType & PH7_TK_COMMA) ){
						p++;
						continue;
					}
					break;
				}
				continue;
			}
			bStmtPos = 0;
			p++;
		}
	}
	/* Filter: keep only the names that do NOT resolve. For a declaration that
	 * will be compiled where it stands, the lookup fires the autoloader exactly
	 * where the replaced compile would. For a CONDITIONAL one it must not: the
	 * declaration is deferred whatever this answers, and php never resolves a
	 * parent it has not reached. `if (false) { class C extends B {} }` is the
	 * shape that shows it -- nikic/php-parser's own class aliases are written
	 * that way, with a `require` of the parent's file BEFORE the dead block, so
	 * an autoload here loaded that file first and the require then declared
	 * everything in it a second time. */
	{
		VmDeferredReq *aReq = (VmDeferredReq *)SySetBasePtr(&aNames);
		sxu32 n;
		for( n = 0 ; n < SySetUsed(&aNames) ; ++n ){
			ph7_class *pFound = bNoAutoload
				? GenStateFindDeclaredClass(pGen->pVm,aReq[n].sName.zString,aReq[n].sName.nByte)
				: PH7_VmExtractClass(pGen->pVm,aReq[n].sName.zString,aReq[n].sName.nByte,FALSE,0);
			if( pFound == 0 ){
				SySetPut(pMissing,(const void *)&aReq[n]);
			}
		}
	}
	SySetRelease(&aNames);
	return rc;
}
/*
 * Capture the declaration as a re-compilable chunk, record it, and emit
 * OP_CLASS_DEFER at the current emission point. On return the statement
 * cursor sits past the declaration's closing '}'. pMissing's entries are
 * COPIED into the record (their name bytes are already allocator-owned).
 */
static sxi32 GenStateEmitDeferredClass(ph7_gen_state *pGen,sxi32 iFlags,int bAnon,
	SySet *pMissing,SyToken *pBodyEnd,SyBlob *pSelfFqn,const SyString *pAnonName)
{
	SyToken *pKw = pGen->pIn; /* the declaration keyword */
	VmDeferredClass *pDefer;
	SyBlob sChunk;
	const char *zFrom;
	const char *zTo;
	char *zDup;
	SyBlobInit(&sChunk,&pGen->pVm->sAllocator);
	/* The declaration site's compile context is replayed as literal statements:
	 * strict_types first (it must open the chunk), then namespace and the
	 * use-import tables — the runtime re-compile starts in a fresh scope. */
	if( pGen->bStrictTypes ){
		SyBlobAppend(&sChunk,"declare(strict_types=1);\n",sizeof("declare(strict_types=1);\n")-1);
	}
	if( SyBlobLength(&pGen->sNamespace) > 0 ){
		SyBlobFormat(&sChunk,"namespace %.*s;\n",
			(int)SyBlobLength(&pGen->sNamespace),(const char *)SyBlobData(&pGen->sNamespace));
	}
	GenStateDeferEmitUses(&sChunk,&pGen->hUseImports,"");
	GenStateDeferEmitUses(&sChunk,&pGen->hUseFuncImports,"function ");
	GenStateDeferEmitUses(&sChunk,&pGen->hUseConstImports,"const ");
	/* Doc-comment and attribute groups precede the keyword in the raw source,
	 * outside the captured span — re-emit them from the trivia sidecar. */
	if( !bAnon && pGen->sPendingDoc.nByte > 0 ){
		SyBlobAppend(&sChunk,pGen->sPendingDoc.zString,pGen->sPendingDoc.nByte);
		SyBlobAppend(&sChunk,"\n",1);
	}
	{
		ph7_trivia *aT;
		sxu32 nT,n;
		if( bAnon ){
			/* `new #[A] class` trivia is keyed to the 'class' token */
			SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);
			aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);
			nT = SySetUsed(&pGen->aTrivia);
			if( pGen->pTokenSet && pKw >= pBase && pKw < &pBase[SySetUsed(pGen->pTokenSet)] ){
				sxu32 nIdx = (sxu32)(pKw - pBase);
				for( n = 0 ; n < nT ; ++n ){
					if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){
						SyBlobFormat(&sChunk,"#[%.*s]\n",(int)aT[n].sText.nByte,aT[n].sText.zString);
					}
				}
			}
		}else{
			aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);
			nT = SySetUsed(&pGen->aPendingAttrs);
			for( n = 0 ; n < nT ; ++n ){
				if( aT[n].iKind == PH7_TRIVIA_ATTR ){
					SyBlobFormat(&sChunk,"#[%.*s]\n",(int)aT[n].sText.nByte,aT[n].sText.zString);
				}
			}
		}
	}
	/* Pad the prefix with newlines so the declaration keyword sits on its
	 * ORIGINAL line inside the chunk — runtime diagnostics from the deferred
	 * compile then report the source's real line. Best-effort: a prefix
	 * already longer than the declaration line skips the padding. */
	{
		const char *zScan = (const char *)SyBlobData(&sChunk);
		sxu32 nHave = 0;
		sxu32 nScan;
		for( nScan = 0 ; nScan < SyBlobLength(&sChunk) ; ++nScan ){
			if( zScan[nScan] == '\n' ){
				nHave++;
			}
		}
		while( nHave + 1 < pKw->nLine ){
			SyBlobAppend(&sChunk,"\n",1);
			nHave++;
		}
	}
	if( bAnon ){
		/* `if (false) { new class <header-minus-args> { body } ; }` — installs
		 * the class at the chunk's compile, never instantiates it. */
		SyToken *pAfterArgs = &pKw[1];
		SyBlobAppend(&sChunk,"if (false) { new ",sizeof("if (false) { new ")-1);
		SyBlobAppend(&sChunk,pKw->sData.zString,pKw->sData.nByte);
		if( pAfterArgs < pGen->pEnd && (pAfterArgs->nType & PH7_TK_LPAREN) ){
			SyToken *pClose = 0;
			PH7_DelimitNestedTokens(&pAfterArgs[1],pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);
			if( pClose == 0 || pClose >= pGen->pEnd ){
				SyBlobRelease(&sChunk);
				return SXERR_INVALID;
			}
			pAfterArgs = &pClose[1];
		}
		if( pAfterArgs < pBodyEnd ){
			SyBlobAppend(&sChunk," ",1);
			zFrom = pAfterArgs->sData.zString;
			zTo = pBodyEnd->sData.zString + pBodyEnd->sData.nByte;
			SyBlobAppend(&sChunk,zFrom,(sxu32)(zTo - zFrom));
		}
		SyBlobAppend(&sChunk,"; }",sizeof("; }")-1);
	}else{
		/* Modifiers were consumed before this compiler ran; reconstruct them
		 * (an enum's implicit `final` must NOT be spelled out). */
		if( (iFlags & PH7_CLASS_ENUM) == 0
		 && (pKw->nType & PH7_TK_KEYWORD)
		 && SX_PTR_TO_INT(pKw->pUserData) == PH7_TKWRD_CLASS ){
			if( iFlags & PH7_CLASS_FINAL ){
				SyBlobAppend(&sChunk,"final ",sizeof("final ")-1);
			}
			if( iFlags & PH7_CLASS_ABSTRACT ){
				SyBlobAppend(&sChunk,"abstract ",sizeof("abstract ")-1);
			}
			if( iFlags & PH7_CLASS_READONLY ){
				SyBlobAppend(&sChunk,"readonly ",sizeof("readonly ")-1);
			}
		}
		zFrom = pKw->sData.zString;
		zTo = pBodyEnd->sData.zString + pBodyEnd->sData.nByte;
		SyBlobAppend(&sChunk,zFrom,(sxu32)(zTo - zFrom));
	}
	pDefer = (VmDeferredClass *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmDeferredClass));
	if( pDefer == 0 ){
		SyBlobRelease(&sChunk);
		PH7_GenCompileError(pGen,E_ERROR,pKw->nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
	SyZero(pDefer,sizeof(VmDeferredClass));
	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,
		(const char *)SyBlobData(&sChunk),SyBlobLength(&sChunk));
	SyBlobRelease(&sChunk);
	if( zDup == 0 ){
		PH7_GenCompileError(pGen,E_ERROR,pKw->nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
	SyStringInitFromBuf(&pDefer->sText,zDup,SyStrlen(zDup));
	if( bAnon ){
		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pAnonName->zString,pAnonName->nByte);
		if( zDup == 0 ){
			PH7_GenCompileError(pGen,E_ERROR,pKw->nLine,"Fatal, PH7 is running out of memory");
			return SXERR_ABORT;
		}
		SyStringInitFromBuf(&pDefer->sAnonName,zDup,pAnonName->nByte);
		pDefer->sSelfName = pDefer->sAnonName;
	}else{
		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,
			(const char *)SyBlobData(pSelfFqn),SyBlobLength(pSelfFqn));
		if( zDup == 0 ){
			PH7_GenCompileError(pGen,E_ERROR,pKw->nLine,"Fatal, PH7 is running out of memory");
			return SXERR_ABORT;
		}
		SyStringInitFromBuf(&pDefer->sSelfName,zDup,SyBlobLength(pSelfFqn));
	}
	SySetInit(&pDefer->aRequired,&pGen->pVm->sAllocator,sizeof(VmDeferredReq));
	{
		VmDeferredReq *aReq = (VmDeferredReq *)SySetBasePtr(pMissing);
		sxu32 n;
		for( n = 0 ; n < SySetUsed(pMissing) ; ++n ){
			SySetPut(&pDefer->aRequired,(const void *)&aReq[n]);
		}
	}
	pDefer->nLine = pKw->nLine;
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLASS_DEFER,0,0,(void *)pDefer,0);
	/* Skip the declaration: the statement cursor lands past its '}' */
	pGen->pIn = &pBodyEnd[1];
	return SXRET_OK;
}
/*
 * Deferral gate shared by the named-declaration compilers: scan the
 * declaration at pGen->pIn; when a dependency is missing, capture + emit the
 * deferred record and return TRUE (the caller returns immediately — the
 * declaration compiles at execution time). FALSE means compile normally.
 * *pRc carries SXERR_ABORT out of the capture path.
 */
static int GenStateMaybeDeferClass(ph7_gen_state *pGen,sxi32 iFlags,int iSelfKind,sxi32 *pRc)
{
	SySet aMissing;
	SyToken *pBody = 0;
	SyToken *pBodyEnd = 0;
	SyBlob sSelfFqn;
	int bDefer = 0;
	/* php's binding rule, decided before anything is resolved: a declaration that
	 * is not at a unit's top level is bound when execution REACHES it, so it is
	 * deferred whatever its dependencies look like -- and nothing about it may be
	 * resolved here. */
	int bCond = GenStateDeclIsConditional(&(*pGen));
	*pRc = SXRET_OK;
	SySetInit(&aMissing,&pGen->pVm->sAllocator,sizeof(VmDeferredReq));
	SyBlobInit(&sSelfFqn,&pGen->pVm->sAllocator);
	if( GenStateScanDeferDeps(pGen,0,iSelfKind,&aMissing,&pBody,&pBodyEnd,&sSelfFqn,bCond) == SXRET_OK
	 && (SySetUsed(&aMissing) > 0 || bCond) ){
		/* Two reasons to compile this declaration where it RUNS rather than here.
		 * The first is a missing dependency (the autoloader that resolves it has
		 * not been registered yet). The second is php's binding rule: a class
		 * written inside an `if`, a loop or a function body is declared when
		 * execution reaches it, so `if (!class_exists('DateTime')) { class
		 * DateTime {} }` -- how symfony/polyfill-php8x ships its back-ports -- must
		 * not REPLACE the engine's own class in a tree that has one. */
		*pRc = GenStateEmitDeferredClass(pGen,iFlags,0,&aMissing,pBodyEnd,&sSelfFqn,0);
		bDefer = 1;
	}
	SySetRelease(&aMissing);
	SyBlobRelease(&sSelfFqn);
	return bDefer;
}
/*
 * Apply a declaration body's collected `use Trait[, Trait] [{ resolution }]`
 * entries to pClass — plain application when no resolution block is present,
 * otherwise the two-pass insteadof/as machinery. Shared by the CLASS body and
 * (since the adaptation-block port) the TRAIT body compiler. Returns the last
 * application status (non-OK = out of memory at a copy site).
 */
static sxi32 GenStateApplyTraitUses(ph7_gen_state *pGen,ph7_class *pClass,SySet *pUseEntries)
{
	sxi32 rc = SXRET_OK;
	{
		TraitUseEntry *apUse;
		sxu32 nU;
		apUse = (TraitUseEntry *)SySetBasePtr(pUseEntries);
		for( nU = 0 ; nU < SySetUsed(pUseEntries) ; nU++ ){
			TraitUseEntry *pUse = &apUse[nU];
			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);
			sxu32 nTraits = SySetUsed(&pUse->aTraits);
			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;
			sxu32 nT;
			if( !hasResolution ){
				/* No conflict resolution block: use standard trait application */
				for( nT = 0 ; nT < nTraits ; nT++ ){
					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);
					if( rc != SXRET_OK ){
						break;
					}
				}
			}else{
				/* With resolution block: copy attributes, record traits,
				 * then use the block to resolve method conflicts.
				 */
				SyToken *pR;
				for( nT = 0 ; nT < nTraits ; nT++ ){
					ph7_class *pTR = apTrait[nT];
					ph7_class_attr *pAR;
					SyHashEntry *pER;
					SyString *pNR;
					SyHashResetLoopCursor(&pTR->hAttr);
					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){
						pAR = (ph7_class_attr *)pER->pUserData;
						pNR = &pAR->sName;
						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){
							SyHashInsertTail(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);
						}
					}
					/* Trait constants (PHP 8.2) live in the separate hConst namespace */
					SyHashResetLoopCursor(&pTR->hConst);
					while((pER = SyHashGetNextEntry(&pTR->hConst)) != 0 ){
						pAR = (ph7_class_attr *)pER->pUserData;
						pNR = &pAR->sName;
						if( SyHashGet(&pClass->hConst,(const void *)pNR->zString,pNR->nByte) == 0 ){
							SyHashInsertTail(&pClass->hConst,(const void *)pNR->zString,pNR->nByte,pAR);
						}
					}
					SySetPut(&pClass->aTrait,(const void *)&pTR);
				}
				/* Pass 1: process insteadof rules to install winning methods */
				pR = pUse->pResolvStart;
				while( pR < pUse->pResolvEnd ){
					SyString sTrait,sMethod;
					ph7_class *pSrcTrait;
					ph7_class_method *pMeth;
					sxi32 nRKwrd;
					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }
					if( pR >= pUse->pResolvEnd ) break;
					SyStringInitFromBuf(&sTrait,"",0);
					SyStringInitFromBuf(&sMethod,"",0);
					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }
					sMethod = pR->sData;
					pR++;
					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){
						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;
						if( pOp && pOp->iOp == EXPR_OP_DC ){
							sTrait = sMethod;
							pR++;
							if( pR >= pUse->pResolvEnd || (pR->nType & PH7_TK_ID) == 0 ) break;
							sMethod = pR->sData;
							pR++;
						}
					}
					if( pR >= pUse->pResolvEnd || (pR->nType & PH7_TK_KEYWORD) == 0 ){
						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }
						continue;
					}
					nRKwrd = SX_PTR_TO_INT(pR->pUserData);
					pR++;
					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){
						pSrcTrait = 0;
						for( nT = 0 ; nT < nTraits ; nT++ ){
							SyString *pTN = &apTrait[nT]->sName;
							if( pTN->nByte >= sTrait.nByte &&
								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){
								pSrcTrait = apTrait[nT];
								break;
							}
						}
						if( pSrcTrait ){
							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);
							if( pMeth ){
								SyString *pMN = &pMeth->sFunc.sName;
								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){
									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);
								}
							}
						}
					}
					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }
				}
				/* Install remaining non-conflicting methods from this use's traits */
				for( nT = 0 ; nT < nTraits ; nT++ ){
					ph7_class_method *pMR;
					SyHashEntry *pER;
					SyString *pNR;
					SyHashResetLoopCursor(&apTrait[nT]->hMethod);
					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){
						pMR = (ph7_class_method *)pER->pUserData;
						pNR = &pMR->sFunc.sName;
						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){
							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);
						}
					}
				}
				/* Pass 2: process as rules (aliases and visibility changes) */
				pR = pUse->pResolvStart;
				while( pR < pUse->pResolvEnd ){
					SyString sTrait,sMethod,sAlias;
					ph7_class *pSrcTrait;
					ph7_class_method *pMeth;
					int hasQual = 0;
					sxi32 nRKwrd;
					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }
					if( pR >= pUse->pResolvEnd ) break;
					SyStringInitFromBuf(&sTrait,"",0);
					SyStringInitFromBuf(&sMethod,"",0);
					SyStringInitFromBuf(&sAlias,"",0);
					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }
					sMethod = pR->sData;
					pR++;
					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){
						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;
						if( pOp && pOp->iOp == EXPR_OP_DC ){
							sTrait = sMethod;
							hasQual = 1;
							pR++;
							if( pR >= pUse->pResolvEnd || (pR->nType & PH7_TK_ID) == 0 ) break;
							sMethod = pR->sData;
							pR++;
						}
					}
					if( pR >= pUse->pResolvEnd || (pR->nType & PH7_TK_KEYWORD) == 0 ){
						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }
						continue;
					}
					nRKwrd = SX_PTR_TO_INT(pR->pUserData);
					pR++;
					if( nRKwrd == PH7_TKWRD_AS ){
						sxi32 iNewVis = -1;
						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){
							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);
							if( nAK == PH7_TKWRD_PUBLIC || nAK == PH7_TKWRD_PROTECTED || nAK == PH7_TKWRD_PRIVATE ){
								iNewVis = nAK;
								pR++;
							}
						}
						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){
							sAlias = pR->sData;
							pR++;
						}
						pMeth = 0;
						if( hasQual ){
							pSrcTrait = 0;
							for( nT = 0 ; nT < nTraits ; nT++ ){
								SyString *pTN = &apTrait[nT]->sName;
								if( pTN->nByte >= sTrait.nByte &&
									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){
									pSrcTrait = apTrait[nT];
									break;
								}
							}
							if( pSrcTrait ){
								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);
							}
						}else{
							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);
						}
						if( pMeth ){
							/* php: a method declared in the class BODY wins over a trait alias
							 * of the same name (e.g. an explicit __construct over `init as
							 * __construct`). If pClass already declares sAlias ITSELF — an own
							 * method, sFunc.pUserData == pClass — keep it: SyHashInsert is LIFO,
							 * so an unconditional insert would shadow the class method at lookup
							 * and `new` would run the alias. A name held only by another trait is
							 * a genuine conflict resolved by the insteadof pass above. */
							int bClassWins = 0;
							if( sAlias.nByte > 0 ){
								ph7_class_method *pOwn = PH7_ClassExtractMethod(pClass,sAlias.zString,sAlias.nByte);
								bClassWins = (pOwn && pOwn->sFunc.pUserData == pClass);
							}
							if( sAlias.nByte > 0 && !bClassWins ){
								/* Create a shallow copy of the method struct for the alias
								 * so it can carry its own visibility without affecting the original.
								 */
								ph7_class_method *pAlias;
								char *zAliasDup;
								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));
								if( pAlias ){
									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));
									if( iNewVis >= 0 ){
										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;
										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;
										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;
									}
									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);
									if( zAliasDup ){
										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);
									}
								}
							}else if( sAlias.nByte == 0 && iNewVis >= 0 ){
								/* Visibility-only change (no alias name): also needs a copy */
								ph7_class_method *pCopy;
								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));
								if( pCopy ){
									SyString *pMN = &pMeth->sFunc.sName;
									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));
									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;
									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;
									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;
									/* Replace the method in the class hash */
									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);
									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);
								}
							}
						}
						SXUNUSED(hasQual);
					}
					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }
				}
			}
			SySetRelease(&pUse->aTraits);
		}
	}
	return rc;
}
/*
 * Compile a class declaration, named or anonymous.
 *
 * For a named class pAnonName is 0 and the class name is read from the token
 * stream. For an anonymous class (`new class(args) extends B implements I {…}`)
 * pAnonName carries the synthesized class name, the optional constructor
 * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to
 * compile, and no name token is expected. Everything after the header (extends/
 * implements, body, install) is shared by both paths.
 */
static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,
	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)
{
	sxu32 nLine = pGen->pIn->nLine;
	ph7_class *pClass,*pBase;
	ph7_class *pSavedCurClass = pGen->pCurClass; /* restored at 'done' (enclosing class for a nested anon) */
	ph7_class *pSavedCurBase = pGen->pCurBase;   /* ...and its base, see pCurBase */
	SyToken *pEnd,*pTmp;
	SySet aInterfaces;
	SySet aUseEntries;
	SySet aOvMeth,aOvProp;   /* the #[\Override] claims this class DECLARED */
	sxu32 nErrEntry = pGen->nErr; /* errors already reported when this class started */
	SyString *pName;
	sxi32 nKwrd;
	sxi32 rc;
	if( pAnonName == 0 ){
		/* Deferral gate: an unresolvable parent/interface/trait —
		 * its autoloader has not RUN yet — re-compiles this declaration at its
		 * execution point instead of dying on "Nonexistent base class". */
		sxi32 rcDefer;
		if( GenStateMaybeDeferClass(pGen,iFlags,PH7_DEFER_KIND_CLASS,&rcDefer) ){
			return rcDefer == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;
		}
	}
	/* Jump the 'class' keyword */
	pGen->pIn++;
	if( pAnonName ){
		/* Anonymous class: no name token. Capture the optional constructor
		 * '(args)' range for the caller (which always supplies the out-params),
		 * then use the synthesized name. */
		*ppArgStart = *ppArgEnd = 0;
		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){
			pGen->pIn++; /* Jump '(' */
			*ppArgStart = pGen->pIn;
			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,
				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);
			pGen->pIn = *ppArgEnd;
			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */
		}
		pName = pAnonName;
		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);
	}else{
		if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_ID) == 0 ){
			/* Syntax error */
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");
			if( rc == SXERR_ABORT ){
				/* Error count limit reached,abort immediately */
				return SXERR_ABORT;
			}
			/* Synchronize with the first semi-colon or curly braces */
			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/|PH7_TK_SEMI/*';'*/)) == 0 ){
				pGen->pIn++;
			}
			return SXRET_OK;
		}
		/* Extract class name */
		pName = &pGen->pIn->sData;
		/* Advance the stream cursor */
		pGen->pIn++;
		/* Build FQN and obtain a raw class */ {
			SyBlob sFQN;
			SyString sFQNStr;
			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);
			GenStateBuildFQN(pGen,pName,&sFQN);
			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));
			/* php refuses a declaration whose short name a local `use` already took. */
			if( GenStateGuardImportRedeclare(pGen,0,pName,&sFQNStr,nLine) == SXERR_ABORT ){
				SyBlobRelease(&sFQN);
				return SXERR_ABORT;
			}
			GenStateRecordDeclaredName(pGen,0,&sFQNStr);
			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);
			SyBlobRelease(&sFQN);
		}
	}
	if( pClass == 0 ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
	if( (iFlags & PH7_CLASS_ENUM) && pGen->pIn < pGen->pEnd
		&& (pGen->pIn->nType & PH7_TK_COLON /* ':' */) ){
		/* Backed enum: `enum Name: int|string` (PHP 8.1) */
		pGen->pIn++; /* Jump ':' */
		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)
			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_INT ){
			pClass->nEnumBacking = MEMOBJ_INT;
			pGen->pIn++;
		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)
			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STRING ){
			pClass->nEnumBacking = MEMOBJ_STRING;
			pGen->pIn++;
		}else{
			SyToken *pTok = pGen->pIn;
			if( pTok >= pGen->pEnd ){ pTok--; }
			rc = PH7_GenCompileError(pGen,E_ERROR,pTok->nLine,
				"Enum backing type must be int or string, %z given",&pTok->sData);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) == 0 ){
				pGen->pIn++; /* Skip the bogus type token */
			}
		}
	}
	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);
	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	/* implemented interfaces and per-use-statement trait containers */
	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));
	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));
	/* Assume a standalone class */
	pBase = 0;
	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){
		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){
			SyBlob sResolved;
			SyString sBaseName;
			sxu32 nRefLine;
			if( iFlags & PH7_CLASS_ENUM ){
				/* php parse-fatals here (enums have no inheritance) */
				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
					"Enum %z cannot extend a class",&pClass->sName);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			pGen->pIn++; /* Advance past 'extends' */
			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;
			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);
			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){
				SyBlobRelease(&sResolved);
				rc = PH7_GenCompileError(pGen,E_PARSE,nLine,
					"Expected 'class_name' after 'extends' keyword inside class '%z'",
					pName);
				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				return SXRET_OK;
			}
			pBase = PH7_VmExtractClass(pGen->pVm,
				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);
			SyStringInitFromBuf(&sBaseName,
				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));
			/* Interfaces are not allowed */
			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){
				pBase = pBase->pNextName;
			}
			if( pBase == 0 ){
				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,
					"Nonexistent base class '%z'",&sBaseName);
				if( rc == SXERR_ABORT ){
					SyBlobRelease(&sResolved);
					return SXERR_ABORT;
				}
			}else{
				if( pBase->iFlags & PH7_CLASS_ENUM ){
					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
						"Class %z cannot extend enum %z",pName,&pBase->sName);
					if( rc == SXERR_ABORT ){
						SyBlobRelease(&sResolved);
						return SXERR_ABORT;
					}
					pBase = 0; /* Never inherit from an enum */
				}else if( pBase->iFlags & PH7_CLASS_FINAL ){
					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
						/* php's wording, unquoted: "Class B cannot extend final class A". */
						"Class %z cannot extend final class %z",pName,&pBase->sName);
					if( rc == SXERR_ABORT ){
						SyBlobRelease(&sResolved);
						return SXERR_ABORT;
					}
				}
			}
			SyBlobRelease(&sResolved);
			if( iFlags & PH7_CLASS_ENUM ){
				pBase = 0; /* Error already reported: enums have no base class */
			}
		}
		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){
			ph7_class *pInterface;
			/* Interface implementation */
			pGen->pIn++; /* Advance the stream cursor */
			for(;;){
				SyBlob sResolved;
				SyString sIntName;
				sxu32 nRefLine;
				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;
				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);
				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){
					SyBlobRelease(&sResolved);
					rc = PH7_GenCompileError(pGen,E_PARSE,nLine,
						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",
						pName);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
					break;
				}
				pInterface = PH7_VmExtractClass(pGen->pVm,
					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);
				SyStringInitFromBuf(&sIntName,
					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));
				/* Only interfaces are allowed */
				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){
					pInterface = pInterface->pNextName;
				}
				if( pInterface == 0 ){
					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,
						"Nonexistent base interface '%z'",&sIntName);
					if( rc == SXERR_ABORT ){
						SyBlobRelease(&sResolved);
						return SXERR_ABORT;
					}
				}else{
					/* Reject user classes that try to implement Throwable
					 * directly (or via an interface that extends Throwable)
					 * unless they already extend Exception or Error.
					 * Exception and Error themselves are compiled from the
					 * built-in library and are exempt by FQN — a namespaced
					 * `Foo\Exception` is a different class and not exempt. */
					SyString *pFqn = &pClass->sName;
					int bIsExceptionOrError =
						(pFqn->nByte == sizeof("Exception")-1 &&
						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) ||
						(pFqn->nByte == sizeof("Error")-1 &&
						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);
					if( GenStateInterfaceIsThrowable(pInterface) &&
						!GenStateClassIsExceptionOrError(pBase) &&
						!bIsExceptionOrError ){
						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
							"Class %z cannot implement interface Throwable, extend Exception or Error instead",
							&pClass->sName);
						if( rc == SXERR_ABORT ){
							SyBlobRelease(&sResolved);
							return SXERR_ABORT;
						}
						/* Skip registration so the follow-up abstract-method
						 * check does not produce a duplicate fatal. */
					}else{
						SySetPut(&aInterfaces,(const void *)&pInterface);
					}
				}
				SyBlobRelease(&sResolved);
				if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){
					break;
				}
				pGen->pIn++;/* Jump the comma */
			}
		}
	}
	if( pGen->pIn >= pGen->pEnd  || (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Expected '{' after class '%z' declaration",pName);
		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	pGen->pIn++; /* Jump the leading curly brace */
	pEnd = 0; /* cc warning */
	/* Delimit the class body */
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);
	if( pEnd >= pGen->pEnd ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Missing closing braces'}' after class '%z' definition",pName);
		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	/* The delimiter token is the class body's closing brace */
	pClass->nEndLine = pEnd->nLine;
	/* Swap token stream */
	pTmp = pGen->pEnd;
	pGen->pEnd = pEnd;
	/* Merge the inherited flags (PH7_NewRawClass may have set INTERNAL) */
	pClass->iFlags |= iFlags;
	if( pAnonName ){
		/* `new class {...}`: the name is synthesized, which is what makes `self`
		 * inside it unusable in an intersection type (see PH7_CLASS_ANON). */
		pClass->iFlags |= PH7_CLASS_ANON;
	}
	/* ...which is what php's own attribute validators judge: `#[\Attribute]` on an
	 * abstract class, `#[\AllowDynamicProperties]` on a readonly one or an enum. */
	if( GenStateCheckAttrPlacement(&(*pGen),&pClass->aAttrs,1,1,
			&pClass->sName,pClass->iFlags) == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	/* This class/enum is now the lexical class for its body — see pCurClass. */
	pGen->pCurClass = pClass;
	pGen->pCurBase = pBase;
	/* Start the parse process */
	for(;;){
		/* Jump leading/trailing semi-colons */
		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){
			pGen->pIn++;
		}
		if( pGen->pIn >= pGen->pEnd ){
			/* End of class body */
			break;
		}
		/* Bind a directly-preceding docblock to this member */
		GenStateSetPendingDoc(&(*pGen));
		if( (pGen->pIn->nType & (PH7_TK_KEYWORD|PH7_TK_DOLLAR)) == 0
			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){
			rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,
				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",
				&pGen->pIn->sData,pName);
			if( rc == SXERR_ABORT ){
				/* Error count limit reached,abort immediately */
				return SXERR_ABORT;
			}
			goto done;
		}
		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){
			/* Extract the current keyword */
			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
			if( nKwrd == PH7_TKWRD_CASE && (pClass->iFlags & PH7_CLASS_ENUM) ){
				/* Enum case declaration: `case NAME [= value];` */
				rc = GenStateCompileEnumCase(&(*pGen),pClass);
				if( rc != SXRET_OK ){
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
					goto done;
				}
				continue;
			}
			if( nKwrd == PH7_TKWRD_USE ){
				/* Trait use: use TraitA, TraitB [{ ... }]; */
				TraitUseEntry sUse;
				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));
				sUse.pResolvStart = sUse.pResolvEnd = 0;
				pGen->pIn++; /* Jump the 'use' keyword */
				for(;;){
					ph7_class *pTrait;
					SyBlob sResolved;
					SyString sTraitName;
					sxu32 nUseLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;
					/* A trait name is a full class reference: it may be qualified or
					 * fully-qualified (`use Foo\Bar\T;`, `use \Foo\Bar\T;`) — the generated
					 * PHPUnit mocks name their traits absolutely. Parse it with the shared
					 * class-reference reader (handles the leading '\', every '\'-segment and
					 * namespace/import resolution) instead of a single-identifier read, which
					 * choked on the first '\'. */
					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);
					if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){
						SyBlobRelease(&sResolved);
						rc = PH7_GenCompileError(pGen,E_PARSE,nUseLine,
							"Expected trait name after 'use' inside class '%z'",pName);
						if( rc == SXERR_ABORT ){
							return SXERR_ABORT;
						}
						break;
					}
					pTrait = PH7_VmExtractClass(pGen->pVm,
						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);
					SyStringInitFromBuf(&sTraitName,
						(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));
					/* Only traits are allowed */
					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){
						pTrait = pTrait->pNextName;
					}
					if( pTrait == 0 ){
						rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,
							"'%z' is not a trait",&sTraitName);
						if( rc == SXERR_ABORT ){
							SyBlobRelease(&sResolved);
							return SXERR_ABORT;
						}
					}else{
						SySetPut(&sUse.aTraits,(const void *)&pTrait);
					}
					SyBlobRelease(&sResolved);
					/* GenStateParseClassReference already advanced past the whole name —
					 * continue only across a comma-separated trait list. */
					if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){
						break;
					}
					pGen->pIn++; /* Jump the comma */
				}
				/* Expect semicolon or opening brace (for conflict resolution) */
				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){
					SyToken *pBlock;
					pGen->pIn++; /* Jump '{' */
					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);
					sUse.pResolvStart = pGen->pIn;
					sUse.pResolvEnd = pBlock;
					if( pBlock < pGen->pEnd ){
						pGen->pIn = &pBlock[1]; /* Skip past '}' */
					}else{
						pGen->pIn = pGen->pEnd;
					}
				}
				SySetPut(&aUseEntries,(const void *)&sUse);
				/* The semicolon will be consumed by the outer loop */
				continue;
			}
		}
		/* Everything else is a MEMBER: its modifier run and the declaration it
		 * modifies. */
		rc = GenStateCompileMember(&(*pGen),pClass,"class");
		if( rc != SXRET_OK ){
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto done;
		}
	}
	/* Apply collected traits (per use-statement) before installing the class.
	 * Each use-statement carries its own set of traits and optional resolution block.
	 */
	rc = GenStateApplyTraitUses(&(*pGen),pClass,&aUseEntries);
	if( rc == SXERR_ABORT ){
		SySetRelease(&aUseEntries);
		SySetRelease(&aInterfaces);
		return SXERR_ABORT;
	}
	if( pClass->iFlags & PH7_CLASS_ENUM ){
		/* Enum validation + name/value props + cases()/from()/tryFrom() synthesis.
		 * Runs after trait application so trait-imported properties are caught. */
		rc = GenStateEnumFinalize(&(*pGen),pClass,nLine);
		if( rc == SXERR_ABORT ){
			SySetRelease(&aUseEntries);
			SySetRelease(&aInterfaces);
			return SXERR_ABORT;
		}
	}
	/* The members this class DECLARES that claim #[\Override] -- recorded here,
	 * before inheritance copies the base's records in beside them, and verified
	 * once the answer exists. */
	GenStateCollectOverrides(&(*pGen),pClass,&aOvMeth,&aOvProp);
	/* Reject a php-fatal redeclaration before hoisting the class. An ENUM is
	 * never early-bound either: php gives every one of them UnitEnum. */
	if( GenStateGuardClassRedeclaration(pGen,pClass,
			SySetUsed(&aInterfaces) == 0 && SySetUsed(&aUseEntries) == 0
			&& (pClass->iFlags & PH7_CLASS_ENUM) == 0
			&& SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) == 0)
		== SXERR_ABORT ){
		SySetRelease(&aOvMeth);
		SySetRelease(&aOvProp);
		return SXERR_ABORT;
	}
	/* Install the class */
	rc = PH7_VmInstallClass(pGen->pVm,pClass);
	if( rc == SXRET_OK ){
		ph7_class **apInterface;
		sxu32 n;
		if( pBase ){
			/* Inherit from base class and mark as a subclass */
			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);
		}
		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);
		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){
			/* Implements one or more interface */
			rc = PH7_ClassImplement(pClass,apInterface[n]);
			if( rc != SXRET_OK ){
				break;
			}
		}
		/* Auto-implement UnitEnum (and BackedEnum for backed enums) — php 8.1:
		 * every enum satisfies `instanceof UnitEnum` implicitly. */
		if( rc == SXRET_OK && (pClass->iFlags & PH7_CLASS_ENUM) ){
			ph7_class *pIntf = PH7_VmExtractClass(pGen->pVm,"UnitEnum",sizeof("UnitEnum")-1,FALSE,0);
			while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){
				pIntf = pIntf->pNextName;
			}
			if( pIntf ){
				PH7_ClassImplement(pClass,pIntf);
			}
			if( pClass->nEnumBacking != 0 ){
				pIntf = PH7_VmExtractClass(pGen->pVm,"BackedEnum",sizeof("BackedEnum")-1,FALSE,0);
				while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){
					pIntf = pIntf->pNextName;
				}
				if( pIntf ){
					PH7_ClassImplement(pClass,pIntf);
				}
			}
		}
		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).
		 * Skip interfaces/traits and classes that already implement it explicitly. */
		if( rc == SXRET_OK
		 && (pClass->iFlags & (PH7_CLASS_INTERFACE|PH7_CLASS_TRAIT)) == 0
		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){
			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,
				"Stringable",sizeof("Stringable")-1,FALSE,0);
			if( pStringable ){
				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);
				sxu32 nImpl = SySetUsed(&pClass->aInterface);
				sxu32 i;
				int bAlready = 0;
				for( i = 0 ; i < nImpl ; i++ ){
					if( apImpl[i] == pStringable ){
						bAlready = 1;
						break;
					}
				}
				if( !bAlready ){
					PH7_ClassImplement(pClass,pStringable);
				}
			}
		}
		/* Validate interface method signatures (visibility and parameter count) */
		if( rc == SXRET_OK ){
			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);
			if( rcCheck == SXERR_ABORT ){
				SySetRelease(&aUseEntries);
				SySetRelease(&aInterfaces);
				SySetRelease(&aOvMeth);
				SySetRelease(&aOvProp);
				return SXERR_ABORT;
			}
		}
		/* Check for unimplemented abstract methods in concrete classes */
		if( rc == SXRET_OK ){
			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);
			if( rcCheck == SXERR_ABORT ){
				SySetRelease(&aUseEntries);
				SySetRelease(&aInterfaces);
				SySetRelease(&aOvMeth);
				SySetRelease(&aOvProp);
				return SXERR_ABORT;
			}
		}
		/* ...and the #[\Override] claims LAST: php reports an unimplemented
		 * abstract method and an inheritance visibility clash before this one,
		 * and stops there — php's E_COMPILE_ERROR does not return, so a
		 * declaration that already failed says nothing more. */
		if( rc == SXRET_OK && pGen->nErr == nErrEntry ){
			sxi32 rcCheck = GenStateCheckOverrides(&(*pGen),pClass,&aOvMeth,&aOvProp);
			if( rcCheck == SXERR_ABORT ){
				SySetRelease(&aUseEntries);
				SySetRelease(&aInterfaces);
				SySetRelease(&aOvMeth);
				SySetRelease(&aOvProp);
				return SXERR_ABORT;
			}
		}
	}
	SySetRelease(&aUseEntries);
	SySetRelease(&aInterfaces);
	SySetRelease(&aOvMeth);
	SySetRelease(&aOvProp);
	if( rc != SXRET_OK ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
done:
	pGen->pCurClass = pSavedCurClass;
	pGen->pCurBase = pSavedCurBase;
	/* Point beyond the class body */
	pGen->pIn = &pEnd[1];
	pGen->pEnd = pTmp;
	return PH7_OK;
}
/* Compile a named class declaration (the common case). */
static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)
{
	return GenStateCompileClassEx(pGen,iFlags,0,0,0);
}
/*
 * Compile an anonymous class expression: `new class(args) extends B implements I
 * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,
 * compile + install the class body once (at compile time, like every other
 * class), then emit the instantiation — push the constructor arguments, load the
 * synthesized class name, and OP_NEW. The class is installed once per source
 * site, matching PHP's one-class-per-anonymous-site semantics.
 */
PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	char zName[128];         /* Synthesized class name */
	static int iCnt = 1;     /* Single-threaded compile: no locking needed */
	SyString sName;
	SyToken *pArgStart,*pArgEnd;
	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `new #[A] class` trivia
	                              * is keyed to this 'class' token */
	ph7_value *pObj;
	sxu32 nLine = pGen->pIn->nLine;
	sxu32 nIdx,nLen;
	sxi32 nArg,rc;
	SXUNUSED(iCompileFlag);
	if( pGen->pVm->sDeferAnonName.nByte > 0 ){
		/* Deferred re-compile (VmExecDeferredClass): install under the SAME
		 * synthesized name the original site's OP_NEW loads. One-shot. */
		sName = pGen->pVm->sDeferAnonName;
		nLen = sName.nByte;
		pGen->pVm->sDeferAnonName.zString = 0;
		pGen->pVm->sDeferAnonName.nByte = 0;
	}else{
		/* Generate a unique anonymous-class name (collision-checked) */
		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);
		while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){
			nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);
		}
		SyStringInitFromBuf(&sName,zName,nLen);
	}
	/* Compile + install the class body; capture the constructor '(args)' range.
	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the
	 * delimited construct; GenStateCompileClassEx restores both on success.
	 * Deferral gate: `new class extends \App\Child {}` where the
	 * parent's autoloader has not RUN yet — capture the class for a runtime
	 * re-compile and keep only the site's argument/OP_NEW emission here. */
	pArgStart = pArgEnd = 0;
	{
		SySet aMissing;
		SyToken *pBody = 0;
		SyToken *pBodyEnd = 0;
		SyBlob sSelfFqn;
		int bDeferred = 0;
		SySetInit(&aMissing,&pGen->pVm->sAllocator,sizeof(VmDeferredReq));
		SyBlobInit(&sSelfFqn,&pGen->pVm->sAllocator);
		/* An anonymous class is an EXPRESSION: it is always compiled where it runs,
		 * so resolving its parent here is resolving it at its execution point --
		 * the autoload belongs. */
		if( GenStateScanDeferDeps(pGen,1,PH7_DEFER_KIND_CLASS,&aMissing,&pBody,&pBodyEnd,&sSelfFqn,0) == SXRET_OK
		 && SySetUsed(&aMissing) > 0 ){
			if( &pTokKw[1] < pGen->pEnd && (pTokKw[1].nType & PH7_TK_LPAREN) ){
				SyToken *pClose = 0;
				PH7_DelimitNestedTokens(&pTokKw[2],pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);
				if( pClose && pClose < pGen->pEnd ){
					pArgStart = &pTokKw[2];
					pArgEnd = pClose;
				}
			}
			rc = GenStateEmitDeferredClass(pGen,0,1,&aMissing,pBodyEnd,0,&sName);
			if( rc == SXERR_ABORT ){
				SySetRelease(&aMissing);
				SyBlobRelease(&sSelfFqn);
				return SXERR_ABORT;
			}
			bDeferred = ( rc == SXRET_OK );
		}
		SySetRelease(&aMissing);
		SyBlobRelease(&sSelfFqn);
		if( !bDeferred ){
			rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);
			if( rc != SXRET_OK ){
				return rc;
			}
			{
				/* Expression-position attributes (`new #[A] class {…}`) */
				ph7_class *pAnonClass = PH7_VmExtractClass(pGen->pVm,sName.zString,nLen,FALSE,0);
				if( pAnonClass
				 && GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnonClass->aAttrs) == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
		}
	}
	/* Emit the instantiation. OP_NEW expects the class name on the stack top
	 * with the constructor arguments beneath it, so push the args first.
	 *
	 * This argument list is compiled from RAW TOKENS rather than through the
	 * expression parser's argument machinery (the class body sits between the
	 * parentheses and the rest of the expression), so the two forms that machinery
	 * recognizes have to be recognized here too — `...$args` and `name: $v`. They
	 * were not: a spread was compiled as one ordinary argument, so
	 * `new class(...$a) {}` passed the ARRAY where php passes its elements, and a
	 * named argument was a `Syntax error: Unexpected token ':'` on source php
	 * compiles. */
	nArg = 0;
	{
	SySet aArgName;              /* one SyString per argument; {0,0} == positional */
	int hasNamed = 0, hasSpread = 0;
	void *p3;
	SySetInit(&aArgName,&pGen->pVm->sAllocator,sizeof(SyString));
	if( pArgStart < pArgEnd ){
		SyToken *pSavedIn = pGen->pIn;
		SyToken *pSavedEnd = pGen->pEnd;
		SyToken *pArgNext;
		const char *zOrder = 0;   /* set when this argument's POSITION or shape is refused */
		SyString sOrderName;      /* nByte > 0: zOrder is a `'%z:'` format, E_PARSE */
		SyZero(&sOrderName,sizeof(sOrderName));
		pGen->pIn = pArgStart;
		pGen->pEnd = pArgEnd;
		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){
			SyToken *pArgIn = pGen->pIn;
			SyString sArgName;
			int bSpread = 0;
			SyZero(&sArgName,sizeof(sArgName));
			if( pArgIn < pArgNext && (pArgIn->nType & PH7_TK_ELLIPSIS) ){
				bSpread = 1;
				pArgIn++;
				if( hasNamed ){
					zOrder = "Cannot use argument unpacking after named arguments";
				}
			}else if( &pArgIn[1] < pArgNext
			 && (pArgIn->nType & (PH7_TK_ID|PH7_TK_KEYWORD))
			 && (pArgIn[1].nType & PH7_TK_COLON) ){
				/* `name: value`. php accepts a reserved word as a parameter name, and
				 * `::` lexes as its own operator, so an ID/KEYWORD followed by a SINGLE
				 * colon at the head of an argument can only be this. */
				sArgName = pArgIn->sData;
				hasNamed = 1;
				pArgIn += 2;
				if( pArgIn >= pArgNext ){
					/* `new class(a:) {}` — a name with no value. Spelled exactly as the
					 * ordinary argument path spells it (php names the token that stopped
					 * it instead; that wording gap belongs to the parse-error family and
					 * is now one gap in both places rather than silence in this one). */
					zOrder = "syntax error, expected expression after named argument '%z:'";
					sOrderName = sArgName;
				}else if( pArgIn->nType & PH7_TK_ELLIPSIS ){
					zOrder = "syntax error, unexpected token \"...\"";
				}
			}else if( hasNamed ){
				zOrder = "Cannot use positional argument after named argument";
			}else if( hasSpread ){
				zOrder = "Cannot use positional argument after argument unpacking";
			}
			if( zOrder ){
				/* The same four rules the ordinary call path enforces at COMPILE time
				 * (GenStateEmitCallArgs); an anonymous class's list is parsed here and
				 * so had none of them. */
				sxu32 nErrLine = pArgNext > pArgStart ? pArgNext[-1].nLine : nLine;
				pGen->pIn = pSavedIn;
				pGen->pEnd = pSavedEnd;
				SySetRelease(&aArgName);
				if( sOrderName.nByte > 0 ){
					rc = PH7_GenCompileError(&(*pGen),E_PARSE,nErrLine,zOrder,&sOrderName);
				}else{
					rc = PH7_GenCompileError(&(*pGen),E_ERROR,nErrLine,"%s",zOrder);
				}
				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;
			}
			if( pArgIn < pArgNext ){
				rc = GenStateCompileArrayEntry(pGen,pArgIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);
				if( rc == SXERR_ABORT ){
					pGen->pIn = pSavedIn;
					pGen->pEnd = pSavedEnd;
					SySetRelease(&aArgName);
					return SXERR_ABORT;
				}
				if( bSpread ){
					/* iP1 marks a source php unpacks BY REFERENCE: only a plain
					 * `$var`, which is exactly two tokens here. */
					PH7_VmEmitInstr(pGen->pVm,PH7_OP_SPREAD,
						(&pArgIn[2] == pArgNext && (pArgIn->nType & PH7_TK_DOLLAR)
						 && (pArgIn[1].nType & (PH7_TK_ID|PH7_TK_KEYWORD))) ? 1 : 0,
						0,0,0);
					hasSpread = 1;
				}
				SySetPut(&aArgName,(const void *)&sArgName);
				nArg++;
			}
			pGen->pIn = &pArgNext[1];
		}
		pGen->pIn = pSavedIn;
		pGen->pEnd = pSavedEnd;
	}
	/* Load the synthesized class name */
	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
	if( pObj == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");
		SySetRelease(&aArgName);
		return SXERR_ABORT;
	}
	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);
	/* The names ride on the instruction as a VmCallArgMap, deep-copied out of the
	 * token stream (which is freed before the code runs) exactly as the ordinary
	 * call path copies them. */
	p3 = 0;
	if( hasNamed ){
		SyString *aName = (SyString *)SySetBasePtr(&aArgName);
		sxu32 n, nStrBytes = 0;
		for( n = 0 ; n < (sxu32)nArg ; ++n ){
			nStrBytes += aName[n].nByte;
		}
		{
		sxu32 mapSize = sizeof(VmCallArgMap) + (sxu32)nArg * sizeof(SyString) + nStrBytes;
		VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,mapSize);
		if( pMap ){
			char *zBuf;
			SyZero(pMap,mapSize);
			pMap->bHasNamed = 1;
			pMap->nTotal = (sxu32)nArg;
			pMap->aNames = (SyString *)&pMap[1];
			zBuf = (char *)&pMap->aNames[nArg];
			for( n = 0 ; n < (sxu32)nArg ; ++n ){
				if( aName[n].nByte > 0 ){
					SyMemcpy(aName[n].zString,zBuf,aName[n].nByte);
					SyStringInitFromBuf(&pMap->aNames[n],zBuf,aName[n].nByte);
					zBuf += aName[n].nByte;
				}
			}
			p3 = (void *)pMap;
		}
		}
	}
	SySetRelease(&aArgName);
	/* Instantiate: pops the name + nArg arguments, runs __construct. iP2 is the
	 * spread flag the effective-argument-map builder keys on. */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,hasSpread ? 1 : 0,
		GenStateAttachStrictFlag(pGen,p3),0);
	}
	return SXRET_OK;
}
/*
 * Compile a user-defined abstract class.
 *  According to the PHP language reference manual
 *   PHP 5 introduces abstract classes and methods. Classes defined as abstract
 *   may not be instantiated, and any class that contains at least one abstract
 *   method must also be abstract. Methods defined as abstract simply declare
 *   the method's signature - they cannot define the implementation.
 *   When inheriting from an abstract class, all methods marked abstract in the parent's
 *   class declaration must be defined by the child; additionally, these methods must be
 *   defined with the same (or a less restricted) visibility. For example, if the abstract
 *   method is defined as protected, the function implementation must be defined as either
 *   protected or public, but not private. Furthermore the signatures of the methods must
 *   match, i.e. the type hints and the number of required arguments must be the same.
 *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures
 *   could differ.
 */
/*
 * Recognize a class-declaration modifier token: the `final`/`abstract` keywords
 * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag
 * receives the corresponding PH7_CLASS_* bit.
 */
static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)
{
	if( pTok->nType & PH7_TK_KEYWORD ){
		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);
		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }
		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }
	}
	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }
	return FALSE;
}
/*
 * Advance *ppIn over a leading run of class modifiers, returning the combined
 * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated
 * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.
 * This stays side-effect-free so it can be used for speculative look-ahead.
 */
static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)
{
	SyToken *pIn = *ppIn,*pDup = 0;
	sxi32 iFlags = 0,iFlag;
	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){
		if( (iFlags & iFlag) && pDup == 0 ){
			pDup = pIn;
		}
		iFlags |= iFlag;
		pIn++;
	}
	*ppIn = pIn;
	if( ppDup ){ *ppDup = pDup; }
	return iFlags;
}
/*
 * Test whether the token stream starts a *modified* class declaration: a run of
 * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated
 * by the `class` keyword. Requiring at least one modifier leaves a bare
 * `class`/`interface`/`trait` (and any expression that merely starts with
 * `readonly`) to their existing handlers.
 */
PH7_PRIVATE int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)
{
	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);
	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)
		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;
}
/*
 * Compile a class declaration carrying one or more leading modifiers
 * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving
 * the cursor on the `class` keyword for GenStateCompileClass, and rejects a
 * repeated modifier (`final final class`) or the mutually-exclusive
 * `abstract`+`final` pair, like PHP.
 */
PH7_PRIVATE sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)
{
	SyToken *pDup;
	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);
	sxi32 rc;
	if( pDup ){
		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,
			"Multiple %z modifiers are not allowed",&pDup->sData);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	if( (iFlags & (PH7_CLASS_FINAL|PH7_CLASS_ABSTRACT))
		== (PH7_CLASS_FINAL|PH7_CLASS_ABSTRACT) ){
		pGen->iFatalTrace = PH7_FATAL_TRACE_NONE;
		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
			"Cannot use the final modifier on an abstract class");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	return GenStateCompileClass(&(*pGen),iFlags);
}
/*
 * Compile a user-defined trait.
 *  Traits are similar to classes, but only intended to group functionality
 *  in a fine-grained and consistent way. It is not possible to instantiate
 *  a Trait on its own. Traits cannot extend or implement.
 */
PH7_PRIVATE sxi32 PH7_CompileTrait(ph7_gen_state *pGen)
{
	sxu32 nLine = pGen->pIn->nLine;
	ph7_class *pClass;
	ph7_class *pSavedCurClass = pGen->pCurClass; /* restored at 'done' */
	ph7_class *pSavedCurBase = pGen->pCurBase;   /* ...and its base, see pCurBase */
	SyToken *pEnd,*pTmp;
	SySet aUseEntries; /* trait-body `use` statements (incl. adaptation blocks) */
	SyString *pName;
	sxi32 nKwrd;
	sxi32 rc;
	{
		/* Deferral gate: a used trait may need an autoloader that
		 * has not run yet. */
		sxi32 rcDefer;
		if( GenStateMaybeDeferClass(pGen,0,PH7_DEFER_KIND_TRAIT,&rcDefer) ){
			return rcDefer == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;
		}
	}
	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));
	/* Jump the 'trait' keyword */
	pGen->pIn++;
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_ID) == 0 ){
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB|PH7_TK_SEMI)) == 0 ){
			pGen->pIn++;
		}
		return SXRET_OK;
	}
	/* Extract trait name */
	pName = &pGen->pIn->sData;
	pGen->pIn++;
	/* Build FQN and obtain a raw class */ {
		SyBlob sFQN;
		SyString sFQNStr;
		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);
		GenStateBuildFQN(pGen,pName,&sFQN);
		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));
		/* php refuses a declaration whose short name a local `use` already took. */
		if( GenStateGuardImportRedeclare(pGen,0,pName,&sFQNStr,nLine) == SXERR_ABORT ){
			SyBlobRelease(&sFQN);
			return SXERR_ABORT;
		}
		GenStateRecordDeclaredName(pGen,0,&sFQNStr);
		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);
		SyBlobRelease(&sFQN);
	}
	if( pClass == 0 ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);
	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	/* Traits cannot extend or implement; expect opening brace directly */
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_OCB) == 0 ){
		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Expected '{' after trait '%z' declaration",pName);
		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	pGen->pIn++; /* Jump the leading curly brace */
	pEnd = 0;
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);
	if( pEnd >= pGen->pEnd ){
		rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"Missing closing braces '}' after trait '%z' definition",pName);
		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	/* The delimiter token is the trait body's closing brace */
	pClass->nEndLine = pEnd->nLine;
	/* Swap token stream */
	pTmp = pGen->pEnd;
	pGen->pEnd = pEnd;
	/* Mark as trait (PH7_NewRawClass may have set INTERNAL) */
	pClass->iFlags |= PH7_CLASS_TRAIT;
	if( GenStateCheckAttrPlacement(&(*pGen),&pClass->aAttrs,1,1,
			&pClass->sName,pClass->iFlags) == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	/* This trait is now the lexical class for its body, so a property/parameter
	 * default here resolves __TRAIT__ to it (see pCurClass). */
	pGen->pCurClass = pClass;
	pGen->pCurBase = 0; /* a trait has no base; its `parent` is deferred to composition */
	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */
	for(;;){
		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){
			pGen->pIn++;
		}
		if( pGen->pIn >= pGen->pEnd ){
			break;
		}
		/* Bind a directly-preceding docblock to this member */
		GenStateSetPendingDoc(&(*pGen));
		if( (pGen->pIn->nType & (PH7_TK_KEYWORD|PH7_TK_DOLLAR)) == 0
			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){
			rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,
				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",
				&pGen->pIn->sData,pName);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto done;
		}
		if( pGen->pIn->nType & PH7_TK_KEYWORD ){
			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
			if( nKwrd == PH7_TKWRD_USE ){
				/* Trait uses another trait: use T[, T2] [{ resolution }]; A trait
				 * name is a full class reference — qualified or fully-qualified
				 * (`use Foo\T;`, `use \Foo\T;`) — so parse it with the shared
				 * class-reference reader like the CLASS body's trait-use does
				 * (the old single-identifier read choked on the leading '\'),
				 * and collect a TraitUseEntry so an adaptation block
				 * (insteadof/as) applies through the same shared machinery. */
				TraitUseEntry sUse;
				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));
				sUse.pResolvStart = sUse.pResolvEnd = 0;
				pGen->pIn++; /* Jump 'use' */
				for(;;){
					ph7_class *pUsedTrait;
					SyBlob sResolved;
					SyString sUsedName;
					sxu32 nUseLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;
					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);
					if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){
						SyBlobRelease(&sResolved);
						rc = PH7_GenCompileError(pGen,E_PARSE,nUseLine,
							"Expected trait name after 'use' inside trait '%z'",pName);
						if( rc == SXERR_ABORT ){
							return SXERR_ABORT;
						}
						break;
					}
					pUsedTrait = PH7_VmExtractClass(pGen->pVm,
						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);
					SyStringInitFromBuf(&sUsedName,
						(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));
					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){
						pUsedTrait = pUsedTrait->pNextName;
					}
					if( pUsedTrait == 0 ){
						rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,
							"'%z' is not a trait",&sUsedName);
						if( rc == SXERR_ABORT ){
							SyBlobRelease(&sResolved);
							return SXERR_ABORT;
						}
					}else{
						SySetPut(&sUse.aTraits,(const void *)&pUsedTrait);
					}
					SyBlobRelease(&sResolved);
					if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){
						break;
					}
					pGen->pIn++;
				}
				/* Optional adaptation block (conflict resolution) */
				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){
					SyToken *pBlock;
					pGen->pIn++; /* Jump '{' */
					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);
					sUse.pResolvStart = pGen->pIn;
					sUse.pResolvEnd = pBlock;
					if( pBlock < pGen->pEnd ){
						pGen->pIn = &pBlock[1]; /* Skip past '}' */
					}else{
						pGen->pIn = pGen->pEnd;
					}
				}
				SySetPut(&aUseEntries,(const void *)&sUse);
				continue;
			}
		}
		/* Everything else is a MEMBER: its modifier run and the declaration it
		 * modifies, read by the same code a class body uses. */
		rc = GenStateCompileMember(&(*pGen),pClass,"trait");
		if( rc != SXRET_OK ){
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto done;
		}
	}
	/* Apply the collected `use` entries (incl. adaptation blocks) through the
	 * machinery shared with the class-body compiler. */
	rc = GenStateApplyTraitUses(&(*pGen),pClass,&aUseEntries);
	SySetRelease(&aUseEntries);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	/* Reject a php-fatal redeclaration before hoisting the trait. php early-binds
	 * a trait like a plain class -- it has nothing left to link. */
	if( GenStateGuardClassRedeclaration(pGen,pClass,1) == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	/* Install the trait */
	rc = PH7_VmInstallClass(pGen->pVm,pClass);
	if( rc != SXRET_OK ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
done:
	pGen->pCurClass = pSavedCurClass;
	pGen->pCurBase = pSavedCurBase;
	/* Point beyond the trait body */
	pGen->pIn = &pEnd[1];
	pGen->pEnd = pTmp;
	return PH7_OK;
}
/*
 * Compile a user-defined class.
 *  According to the PHP language reference manual
 *   Basic class definitions begin with the keyword class, followed
 *   by a class name, followed by a pair of curly braces which enclose
 *   the definitions of the properties and methods belonging to the class.
 *   A class may contain its own constants, variables (called "properties")
 *   and functions (called "methods").
 */
PH7_PRIVATE sxi32 PH7_CompileClass(ph7_gen_state *pGen)
{
	sxi32 rc;
	rc = GenStateCompileClass(&(*pGen),0);
	return rc;
}
/*
 * Return TRUE if the token stream starts an enum declaration (PHP 8.1):
 * the context-sensitive identifier `enum` (not a reserved word — it stays
 * valid as a function/constant name, like `readonly`) directly followed by
 * an identifier. `enum(...)`/`enum;`/`$enum` all keep their expression
 * meaning; `enum Name` can never start a valid expression.
 */
PH7_PRIVATE int GenStateStartsEnumDecl(SyToken *pIn,SyToken *pEnd)
{
	return (pIn->nType & PH7_TK_ID)
		&& pIn->sData.nByte == sizeof("enum")-1
		&& SyStrnicmp(pIn->sData.zString,"enum",sizeof("enum")-1) == 0
		&& &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_ID);
}
/*
 * Compile an enum declaration (PHP 8.1). An enum is a final class carrying
 * PH7_CLASS_ENUM: `case` members become lazily-materialized singleton
 * constants, cases()/from()/tryFrom() are synthesized, and UnitEnum/BackedEnum
 * are implemented implicitly (GenStateCompileClassEx handles the specifics).
 */
PH7_PRIVATE sxi32 PH7_CompileEnum(ph7_gen_state *pGen)
{
	return GenStateCompileClass(&(*pGen),PH7_CLASS_ENUM|PH7_CLASS_FINAL);
}
