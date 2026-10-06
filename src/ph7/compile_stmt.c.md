# src/ph7/compile_stmt.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2465/2874 lines (85.77%)

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
|        - |   10 | ` *    Statement compilation: const, continue/break, labels and goto, blocks,` |
|        - |   11 | ` *    loops (while/do/for/foreach), if, global, return, yield, halt, echo,` |
|        - |   12 | ` *    static, var, namespace/use/declare, throw, try/catch/finally, switch.` |
|        - |   13 | ` * Status:` |
|        - |   14 | ` *    Stable.` |
|        - |   15 | ` */` |
|        - |   16 | `/*` |
|        - |   17 | ` * Compile the 'const' statement.` |
|        - |   18 | ` * According to the PHP language reference` |
|        - |   19 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|        - |   20 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|        - |   21 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|        - |   22 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|        - |   23 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|        - |   24 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|        - |   25 | ` *  Syntax` |
|        - |   26 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|        - |   27 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|        - |   28 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|        - |   29 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|        - |   30 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|        - |   31 | ` *  to get a list of all defined constants.` |
|        - |   32 | ` *` |
|        - |   33 | ` * Symisc eXtension.` |
|        - |   34 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|        - |   35 | ` *  would allow only simple scalar value.` |
|        - |   36 | ` *  Example` |
|        - |   37 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|        - |   38 | ` *    Refer to the official documentation for more information on this feature.` |
|        - |   39 | ` */` |
|        - |   40 | `/*` |
|        - |   41 | ` * The keyword-shaped names php still accepts as a GLOBAL constant name: its lexer` |
|        - |   42 | `` * does not reserve the type words or the scope words, so `const int = 1;` and`` |
|        - |   43 | `` * `const self = 1;` parse there while every other keyword is a syntax error. Kept`` |
|        - |   44 | ` * as a list rather than a token-class test because the two engines' keyword TABLES` |
|        - |   45 | ` * are not the same set — PHL lexes some words php leaves as plain identifiers.` |
|        - |   46 | ` */` |
|       20 |   47 | `static int GenStateConstNameKeywordOk(SyString *pName)` |
|        3 |   48 | `{` |
|        - |   49 | ``	/* `integer` and `boolean` lex to the same tokens as `int` and `bool` here,`` |
|        - |   50 | `	 * but php has no reserved word under either spelling and takes both as a` |
|        - |   51 | `	 * constant name. */` |
|        - |   52 | `	static const char *azOk[] = { "bool", "boolean", "float", "int", "integer",` |
|        - |   53 | `	                              "object", "string", "self", "parent" };` |
|        - |   54 | `	sxu32 i;` |
|      113 |   55 | `	for( i = 0 ; i < SX_ARRAYSIZE(azOk) ; ++i ){` |
|      111 |   56 | `		sxu32 n = (sxu32)SyStrlen(azOk[i]);` |
|      111 |   57 | `		if( pName->nByte == n && SyStrnicmp(pName->zString,azOk[i],n) == 0 ){` |
|       20 |   58 | `			return 1;` |
|        - |   59 | `		}` |
|       48 |   60 | `	}` |
|        3 |   61 | `	return 0;` |
|       13 |   62 | `}` |
|        - |   63 | `/*` |
|        - |   64 | `` * php's grammar takes `const`, `use` and `namespace` as TOP statements only: at`` |
|        - |   65 | ` * file scope or directly inside a namespace's braces. In a function, a class` |
|        - |   66 | `` * method, a closure or any block -- a plain `{}` included -- each is a parse`` |
|        - |   67 | ` * error on the keyword itself, with whatever tail the enclosing block names.` |
|        - |   68 | ` * Answers TRUE when the statement at the cursor stands where php takes one.` |
|        - |   69 | ` */` |
|     1160 |   70 | `static int GenStateAtTopStatement(ph7_gen_state *pGen)` |
|        5 |   71 | `{` |
|     1256 |   72 | `	return pGen->pCurrent == &pGen->sGlobal` |
|     1160 |   73 | `		\|\| (pGen->bInNsBlock && pGen->pCurrent->pParent == &pGen->sGlobal);` |
|        5 |   74 | `}` |
|      332 |   75 | `PH7_PRIVATE sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|        5 |   76 | `{` |
|        - |   77 | `	sxu32 nLineLocal;` |
|        - |   78 | `	SyString *pName;` |
|        - |   79 | `	sxi32 rc;` |
|        - |   80 | `	/* php forbids attributes on a comma-separated const list. Snapshot whether the` |
|        - |   81 | `	 * statement carries any now, before the first constant consumes them. */` |
|      337 |   82 | `	int bHadAttrs = SySetUsed(&pGen->aPendingAttrs) > 0;` |
|      337 |   83 | `	SySet *pAttrs = 0;   /* the set the first constant took them into */` |
|      337 |   84 | `	int nDecl = 0;       /* constants this statement declares */` |
|        - |   85 | `	/* php attributes every const-statement diagnostic to the statement's line, not` |
|        - |   86 | ``	 * the offending list element's own (`const A=1,\nB=strlen()` blames line 1),`` |
|        - |   87 | ``	 * and the statement's line is its FIRST NAME's -- `const` on a line of its own`` |
|        - |   88 | `	 * moves nothing. Taken from the keyword here for the refusals before the name,` |
|        - |   89 | `	 * then from the name itself. */` |
|      337 |   90 | `	nLineLocal = pGen->pIn->nLine;` |
|        - |   91 | `	/* A top statement only: elsewhere this compiled it and declared the` |
|        - |   92 | `	 * constant when the statement ran. */` |
|      337 |   93 | `	if( !GenStateAtTopStatement(&(*pGen)) ){` |
|       57 |   94 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn,pGen->pCurrent->zInnerTail);` |
|       57 |   95 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |   96 | `			return SXERR_ABORT;` |
|        - |   97 | `		}` |
|       57 |   98 | `		goto Synchronize;` |
|        - |   99 | `	}` |
|      281 |  100 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|        - |  101 | ``	/* php allows a single `const` statement to declare several constants at once`` |
|        - |  102 | ``	 * (`const A = 1, B = 2;`). Loop over the comma-separated name = value pairs;`` |
|        - |  103 | `	 * PH7_CompileExpr(EXPR_FLAG_COMMA_STATEMENT) stops each value at the first` |
|        - |  104 | `	 * top-level comma so the next pair starts cleanly. */` |
|      149 |  105 | `Loop:` |
|      303 |  106 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  107 | `		/* Invalid constant name */` |
|        9 |  108 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"identifier");` |
|        9 |  109 | `		if( rc == SXERR_ABORT ){` |
|        - |  110 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  111 | `			return SXERR_ABORT;` |
|        - |  112 | `		}` |
|        9 |  113 | `		goto Synchronize;` |
|        - |  114 | `	}` |
|        - |  115 | `	/* Peek constant name */` |
|      297 |  116 | `	pName = &pGen->pIn->sData;` |
|      297 |  117 | `	if( nDecl++ == 0 ){` |
|      275 |  118 | `		nLineLocal = pGen->pIn->nLine;` |
|      135 |  119 | `	}` |
|        - |  120 | ``	/* php's global `const` takes an IDENTIFIER: a reserved word is a parse error`` |
|        - |  121 | `` 	 * there, and only a CLASS constant may carry one (`class C { const list = 5; }` `` |
|        - |  122 | ``	 * is php-legal, `const list = 5;` at file scope is not). PHL accepted both, so`` |
|        - |  123 | ``	 * `const LIST = 1;` compiled and READ back — source php refuses to parse.`` |
|        - |  124 | `	 * The words php still allows are the ones its lexer does not reserve: the type` |
|        - |  125 | ``	 * names and the scope words. The word OPERATORS (`and`, `or`, `xor`, `new`,`` |
|        - |  126 | ``	 * `clone`, `instanceof`) are reserved too — the lexer types those ID\|OP rather`` |
|        - |  127 | `	 * than KEYWORD, which is why the test reads both bits. */` |
|      292 |  128 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_OP))` |
|      161 |  129 | `	 && !GenStateConstNameKeywordOk(pName) ){` |
|        3 |  130 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn,"identifier");` |
|        3 |  131 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  132 | `			return SXERR_ABORT;` |
|        - |  133 | `		}` |
|        3 |  134 | `		goto Synchronize;` |
|        - |  135 | `	}` |
|        - |  136 | `	/* Make sure the constant name isn't reserved */` |
|      295 |  137 | `	if( GenStateIsReservedConstant(pName) ){` |
|        - |  138 | `		/* Reserved constant */` |
|       10 |  139 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Cannot redeclare constant '%z'",pName);` |
|       10 |  140 | `		if( rc == SXERR_ABORT ){` |
|        - |  141 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  142 | `			return SXERR_ABORT;` |
|        - |  143 | `		}` |
|       10 |  144 | `		goto Synchronize;` |
|        - |  145 | `	}` |
|      287 |  146 | `	pGen->pIn++;` |
|      287 |  147 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|        - |  148 | `		/* Invalid statement*/` |
|        6 |  149 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"=\"");` |
|        6 |  150 | `		if( rc == SXERR_ABORT ){` |
|        - |  151 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  152 | `			return SXERR_ABORT;` |
|        - |  153 | `		}` |
|        6 |  154 | `		goto Synchronize;` |
|        - |  155 | `	}` |
|      283 |  156 | `	pGen->pIn++; /*Jump the equal sign */` |
|        - |  157 | `	/* php's constant-expression rules, first offender wins (see` |
|        - |  158 | ``	 * PH7_GenStateConstExprError). A global `const` DOES take `new` (PHP 8.1's`` |
|        - |  159 | ``	 * "new in initializers"), so the `new` rule is off here. */`` |
|        - |  160 | `	{` |
|      283 |  161 | `		const char *zCErr = PH7_GenStateConstExprError(pGen,1);` |
|      283 |  162 | `		if( zCErr ){` |
|       14 |  163 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"%s",zCErr);` |
|       14 |  164 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  165 | `				return SXERR_ABORT;` |
|        - |  166 | `			}` |
|       14 |  167 | `			goto Synchronize;` |
|        - |  168 | `		}` |
|        - |  169 | `	}` |
|        - |  170 | `	/* The initializer is compiled INLINE and the declaration emitted after it: php` |
|        - |  171 | `	 * evaluates the value where the statement runs and only then binds the name` |
|        - |  172 | `	 * (ZEND_DECLARE_CONST), so a throw in the initializer is an ordinary throw at` |
|        - |  173 | `	 * this statement and a name the table already holds is refused there. Binding` |
|        - |  174 | `	 * at compile time made the name readable before its statement ran and let a` |
|        - |  175 | `	 * second declaration replace the first in silence.` |
|        - |  176 | `` 	 * php: a stray token after `const X = EXPR` is `... expecting "," or ";"` `` |
|        - |  177 | `	 * (const supports a comma-separated list). EXPR_FLAG_COMMA_STATEMENT stops this` |
|        - |  178 | `	 * value at the first top-level comma so a following declaration is left for the` |
|        - |  179 | `	 * loop below. */` |
|        - |  180 | `	{` |
|      273 |  181 | `		const char *zSaveConst = pGen->zClauseCloser;` |
|      273 |  182 | `		pGen->zClauseCloser = "\",\" or \";\"";` |
|      273 |  183 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      273 |  184 | `		pGen->zClauseCloser = zSaveConst;` |
|        - |  185 | `	}` |
|      273 |  186 | `	if( rc == SXERR_ABORT ){` |
|        - |  187 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  188 | `		return SXERR_ABORT;` |
|        - |  189 | `	}` |
|        - |  190 | ``	/* php parse-errors an empty initializer (`const A = ;`, or a `,B=` list hole);`` |
|        - |  191 | `	 * the class-const path rejects it too. Reject loudly rather than silently` |
|        - |  192 | `	 * defining a NULL constant. (php's error KIND -- a parse error -- differs from` |
|        - |  193 | `	 * PHL's compile fatal, the recursive-descent-vs-bison family; both refuse.) */` |
|      273 |  194 | `	if( rc == SXERR_EMPTY ){` |
|        2 |  195 | `		if( PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Empty constant '%z' value",pName)` |
|        2 |  196 | `				== SXERR_ABORT ){` |
|      ! 0 |  197 | `			return SXERR_ABORT;` |
|        - |  198 | `		}` |
|        3 |  199 | `		goto Next;` |
|        - |  200 | `	}` |
|        - |  201 | `	/* Declare the constant under its namespace-qualified name */` |
|        - |  202 | `	{` |
|        - |  203 | `		VmConstDecl *pDecl;` |
|        - |  204 | `		SyBlob sFQN;` |
|        - |  205 | `		SyString sFQNStr;` |
|      271 |  206 | `		pDecl = (VmConstDecl *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(VmConstDecl));` |
|      271 |  207 | `		if( pDecl == 0 ){` |
|      ! 0 |  208 | `			PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  209 | `			return SXERR_ABORT;` |
|        - |  210 | `		}` |
|      271 |  211 | `		SyZero(pDecl,sizeof(VmConstDecl));` |
|      271 |  212 | `		SySetInit(&pDecl->aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|      271 |  213 | `		pDecl->nLine = nLineLocal;` |
|      271 |  214 | `		if( SySetUsed(&pGen->pVm->aFiles) > 0 ){` |
|      271 |  215 | `			pDecl->sFile = *(SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|      133 |  216 | `		}` |
|      271 |  217 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      271 |  218 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      271 |  219 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|        - |  220 | ``		/* php refuses a `const` whose name a local `use const` already took. */`` |
|      271 |  221 | `		if( GenStateGuardImportRedeclare(pGen,2,pName,&sFQNStr,nLineLocal) == SXERR_ABORT ){` |
|      ! 0 |  222 | `			SyBlobRelease(&sFQN);` |
|      ! 0 |  223 | `			return SXERR_ABORT;` |
|        - |  224 | `		}` |
|        - |  225 | `		/* The instruction outlives this blob: keep the name in VM-lifetime storage. */` |
|        - |  226 | `		{` |
|      271 |  227 | `			char *zFQN = SyMemBackendStrDup(&pGen->pVm->sAllocator,sFQNStr.zString,sFQNStr.nByte);` |
|      271 |  228 | `			SyBlobRelease(&sFQN);` |
|      271 |  229 | `			if( zFQN == 0 ){` |
|      ! 0 |  230 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  231 | `				return SXERR_ABORT;` |
|        - |  232 | `			}` |
|      271 |  233 | `			SyStringInitFromBuf(&pDecl->sName,zFQN,sFQNStr.nByte);` |
|        - |  234 | `		}` |
|      271 |  235 | `		if( SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|        - |  236 | ``			/* php 8.5: attributes on `const` statements -- kept for the record the`` |
|        - |  237 | `			 * declaration installs, which is what Reflection reads. */` |
|       23 |  238 | `			if( GenStateConsumeAttrs(&(*pGen),&pDecl->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  239 | `				return SXERR_ABORT;` |
|        - |  240 | `			}` |
|       23 |  241 | `			pAttrs = &pDecl->aAttrs;` |
|       10 |  242 | `		}` |
|      271 |  243 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONST_DECL,0,0,(void *)pDecl,0);` |
|      133 |  244 | `	}` |
|      134 |  245 | `Next:` |
|        - |  246 | ``	/* Another declaration in the same statement: `const A = 1, B = 2;`. */`` |
|      273 |  247 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /* ',' */) ){` |
|       25 |  248 | `		pGen->pIn++; /* Jump the comma */` |
|       25 |  249 | `		goto Loop;` |
|        - |  250 | `	}` |
|        - |  251 | `	/* php judges the statement's attributes once every constant has compiled:` |
|        - |  252 | `	 * a list carrying any is refused outright, and only a single constant's set` |
|        - |  253 | `	 * reaches the placement rules. */` |
|      251 |  254 | `	if( bHadAttrs && nDecl > 1 ){` |
|        6 |  255 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|        - |  256 | `			"Cannot apply attributes to multiple constants at once");` |
|        6 |  257 | `		return ( rc == SXERR_ABORT ) ? SXERR_ABORT : SXRET_OK;` |
|        - |  258 | `	}` |
|      247 |  259 | `	if( pAttrs && GenStateCheckAttrPlacement(&(*pGen),pAttrs,nLineLocal,64,64,0,0) == SXERR_ABORT ){` |
|      ! 0 |  260 | `		return SXERR_ABORT;` |
|        - |  261 | `	}` |
|      247 |  262 | `	return SXRET_OK;` |
|       43 |  263 | `Synchronize:` |
|        - |  264 | `	/* Synchronize with the next top-level semicolon and avoid compiling this` |
|        - |  265 | ``	 * erroneous statement. BRACE-aware (only `{`...`}`): a rejected closure`` |
|        - |  266 | ``	 * initializer's body holds inner `;` that are not statement terminators, so`` |
|        - |  267 | ``	 * without this its `;` and `}` dangle into a spurious second error where php`` |
|        - |  268 | `	 * halts at the first fatal. Parentheses and brackets are deliberately NOT` |
|        - |  269 | ``	 * tracked -- a lone unbalanced `(`/`[` in erroneous input must not swallow the`` |
|        - |  270 | ``	 * following statements (which would drop later error reports); a `{` never`` |
|        - |  271 | `	 * appears in a valid global-const initializer except as a closure body. */` |
|        - |  272 | `	{` |
|       91 |  273 | `		int iBrace = 0;` |
|      411 |  274 | `		while( pGen->pIn < pGen->pEnd ){` |
|      411 |  275 | `			if( iBrace == 0 && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       91 |  276 | `				break;` |
|        - |  277 | `			}` |
|      325 |  278 | `			if( pGen->pIn->nType & PH7_TK_OCB ){` |
|        3 |  279 | `				iBrace++;` |
|      324 |  280 | `			}else if( pGen->pIn->nType & PH7_TK_CCB ){` |
|        3 |  281 | `				if( iBrace > 0 ){ iBrace--; }` |
|        1 |  282 | `			}` |
|      325 |  283 | `			pGen->pIn++;` |
|        5 |  284 | `		}` |
|        - |  285 | `	}` |
|       91 |  286 | `	return SXRET_OK;` |
|      171 |  287 | `}` |
|        - |  288 | `/*` |
|        - |  289 | ` * Compile the 'continue' statement.` |
|        - |  290 | ` * According to the PHP language reference` |
|        - |  291 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|        - |  292 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|        - |  293 | ` *  iteration.` |
|        - |  294 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|        - |  295 | ` *  the purposes of continue.` |
|        - |  296 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|        - |  297 | ` *  of enclosing loops it should skip to the end of.` |
|        - |  298 | ` *  Note:` |
|        - |  299 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|        - |  300 | ` */` |
|        - |  301 | `/*` |
|        - |  302 | `` * Pick the jump opcode for a `break`/`continue` targeting pLoop and fill in its iP1,`` |
|        - |  303 | ` * emitting the POP_EXCEPTIONs for the crossed trys this statement can resolve here and` |
|        - |  304 | `` * now. All the classification lives in GenStateJumpScope, which `goto` shares.`` |
|        - |  305 | ` */` |
|    59959 |  306 | `static sxi32 GenStateLoopJumpOp(ph7_gen_state *pGen,GenBlock *pLoop,sxi32 *piP1,` |
|        - |  307 | `	GenJumpScope *pCross)` |
|        5 |  308 | `{` |
|        - |  309 | `	/* The loop encloses the break by construction, so the walk always reaches it. Note the` |
|        - |  310 | `	 * walk EMITS the crossed trys' POP_EXCEPTIONs as it goes, before the caller can see` |
|        - |  311 | `	 * pCross->nFinally and reject: a statement about to be fatal therefore leaves a few` |
|        - |  312 | `	 * dead instructions behind. Harmless — the compile error stops the program from` |
|        - |  313 | `	 * running at all — and the alternative is walking the chain twice on every jump. */` |
|    59964 |  314 | `	GenStateJumpScope(&(*pGen),pGen->nCurScopeId,pLoop->nScopeId,TRUE,pCross);` |
|    59964 |  315 | `	return GenStateScopeJumpOp(pCross,piP1);` |
|        5 |  316 | `}` |
|        - |  317 | `/*` |
|        - |  318 | `` * php compile-rejects a `break`/`continue`/`goto` that leaves a `finally` body (a`` |
|        - |  319 | `` * `return` is fine). One wording, one place, for all three statements.`` |
|        - |  320 | ` */` |
|        - |  321 | `/* Whether the cursor has run past the LAST token of a chunk that met the end of` |
|        - |  322 | ` * the FILE -- the shared end-of-input question, asked here by the three statements` |
|        - |  323 | ` * that would otherwise report a complaint of their own first. */` |
|        2 |  324 | `static int GenStateAtChunkEofStmt(ph7_gen_state *pGen)` |
|      ! 0 |  325 | `{` |
|        - |  326 | `	SyToken *pBase;` |
|        2 |  327 | `	if( !pGen->bChunkAtEof \|\| pGen->pTokenSet == 0 ){` |
|      ! 0 |  328 | `		return 0;` |
|        - |  329 | `	}` |
|        2 |  330 | `	pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        2 |  331 | `	return pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)];` |
|        1 |  332 | `}` |
|       10 |  333 | `PH7_PRIVATE sxi32 GenStateJumpOutOfFinally(ph7_gen_state *pGen,sxu32 nLine)` |
|        4 |  334 | `{` |
|       14 |  335 | `	return PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  336 | `		"jump out of a finally block is disallowed");` |
|        4 |  337 | `}` |
|        - |  338 | `/*` |
|        - |  339 | `` * php's `break`/`continue` grammar is `KEYWORD optional_expr ";"`, and the level is`` |
|        - |  340 | ` * then screened as a compile-time VALUE rather than parsed as a number: an operand` |
|        - |  341 | `` * that is not a literal at all -- a constant, a variable, `-1`, `(1+0)` -- is`` |
|        - |  342 | `` * `'break' operator with non-integer operand is no longer supported`, and one that`` |
|        - |  343 | `` * IS a literal but not a positive integer -- `1.5`, `"1"`, `0` -- is`` |
|        - |  344 | `` * `'break' operator accepts only positive integers`. Parentheses are transparent`` |
|        - |  345 | `` * (`((1))` is 1) because they leave no node of their own, which is also why the`` |
|        - |  346 | ` * arithmetic inside them is not folded away first.` |
|        - |  347 | ` *` |
|        - |  348 | `` * PHL used to read a NUMBER token and ignore anything else, so `break 1.5;` broke`` |
|        - |  349 | `` * one level in silence, `break $x;` and `break foo;` compiled to a plain break with`` |
|        - |  350 | ` * a WARNING about the missing semicolon, and the program then RAN.` |
|        - |  351 | ` *` |
|        - |  352 | ` * Answers SXRET_OK with *piLevel set (1 when there is no operand), SXERR_ABORT to` |
|        - |  353 | ` * abort the compile, or SXERR_SYNTAX once a refusal has been reported.` |
|        - |  354 | ` */` |
|    60027 |  355 | `static sxi32 GenStateJumpLevelArg(ph7_gen_state *pGen,const char *zWhich,sxu32 nLine,sxi32 *piLevel)` |
|        5 |  356 | `{` |
|    60032 |  357 | `	SyToken *pStart = pGen->pIn,*pEnd = pGen->pEnd,*pAfter;` |
|        - |  358 | `	sxi32 rc;` |
|    60032 |  359 | `	*piLevel = 1;` |
|        - |  360 | `	/* The statement slice runs to the end of the enclosing block, so the operand` |
|        - |  361 | `	 * stops at its own terminator. */` |
|    60164 |  362 | `	for( pAfter = pStart ; pAfter < pEnd ; pAfter++ ){` |
|    60162 |  363 | `		if( pAfter->nType & PH7_TK_SEMI ){` |
|    60030 |  364 | `			pEnd = pAfter;` |
|    60030 |  365 | `			break;` |
|        - |  366 | `		}` |
|       71 |  367 | `	}` |
|    60032 |  368 | `	if( pStart >= pEnd ){` |
|    59950 |  369 | `		return SXRET_OK; /* No operand at all: one level */` |
|        - |  370 | `	}` |
|       87 |  371 | `	pGen->pIn = pEnd; /* The whole operand belongs to this statement either way */` |
|        - |  372 | `	/* Peel parenthesis pairs that wrap the WHOLE operand. */` |
|       51 |  373 | `	for(;;){` |
|        - |  374 | `		SyToken *pTok;` |
|       97 |  375 | `		sxi32 nDepth = 0;` |
|       97 |  376 | `		if( !(pStart->nType & PH7_TK_LPAREN) \|\| !(pEnd[-1].nType & PH7_TK_RPAREN) ){` |
|       46 |  377 | `			break;` |
|        - |  378 | `		}` |
|       36 |  379 | `		for( pTok = pStart ; pTok < pEnd ; pTok++ ){` |
|       36 |  380 | `			if( pTok->nType & PH7_TK_LPAREN ){` |
|       12 |  381 | `				nDepth++;` |
|       30 |  382 | `			}else if( pTok->nType & PH7_TK_RPAREN ){` |
|       12 |  383 | `				nDepth--;` |
|       12 |  384 | `				if( nDepth == 0 ){` |
|       10 |  385 | `					break;` |
|        - |  386 | `				}` |
|        1 |  387 | `			}` |
|       13 |  388 | `		}` |
|       10 |  389 | `		if( pTok != pEnd - 1 ){` |
|      ! 0 |  390 | `			break; /* The opening paren closes before the end: not a wrap */` |
|        - |  391 | `		}` |
|       10 |  392 | `		pStart++;` |
|       10 |  393 | `		pEnd--;` |
|      ! 0 |  394 | `	}` |
|       87 |  395 | `	if( pStart >= pEnd ){` |
|        - |  396 | ``		/* The parentheses held nothing (`break ();`): php's expression parser has`` |
|        - |  397 | `		 * no operand to read and names the closing one, which is where the peel` |
|        - |  398 | `		 * above left the cursor. */` |
|        2 |  399 | `		rc = PH7_GenSyntaxError(&(*pGen),pStart,0);` |
|        2 |  400 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  401 | `	}` |
|       80 |  402 | `	if( (pStart->nType & (PH7_TK_INTEGER\|PH7_TK_REAL\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_HEREDOC` |
|       45 |  403 | `	                     \|PH7_TK_NOWDOC\|PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|        - |  404 | `		/* An operator leads the operand, so it cannot be a literal node. */` |
|        4 |  405 | `		goto NonInteger;` |
|        - |  406 | `	}` |
|        - |  407 | ``	/* The primary this operand starts with -- `$name` is two tokens here. */`` |
|       81 |  408 | `	pAfter = &pStart[(pStart->nType & PH7_TK_DOLLAR) ? 2 : 1];` |
|       81 |  409 | `	if( pAfter < pEnd ){` |
|       18 |  410 | `		if( (pAfter->nType & (PH7_TK_OP\|PH7_TK_OSB\|PH7_TK_LPAREN))` |
|       14 |  411 | `		 && (pAfter->nType & PH7_TK_COMMA) == 0 ){` |
|        - |  412 | `			/* The primary continues into a larger expression php will not take.` |
|        - |  413 | `			 * A comma is typed as an operator here and is not one to php: the` |
|        - |  414 | `			 * expression has ENDED there, so the semicolon is what it wants. */` |
|        4 |  415 | `			goto NonInteger;` |
|        - |  416 | `		}` |
|        - |  417 | `		/* php read its expression and now wants the semicolon. */` |
|       16 |  418 | `		rc = PH7_GenSyntaxError(&(*pGen),pAfter,"\";\"");` |
|       16 |  419 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  420 | `	}` |
|       61 |  421 | `	if( pStart->nType & PH7_TK_INTEGER ){` |
|        - |  422 | `		char zScratch[GEN_NUM_SCRATCH];` |
|       49 |  423 | `		char *zAlloc = 0;` |
|        - |  424 | `		SyString sNum;` |
|       72 |  425 | `		if( SXRET_OK != GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|       46 |  426 | `				&pStart->sData,zScratch,sizeof(zScratch),&sNum,&zAlloc) ){` |
|      ! 0 |  427 | `			return SXERR_ABORT;` |
|        - |  428 | `		}` |
|       49 |  429 | `		*piLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|       49 |  430 | `		if( zAlloc ){` |
|      ! 0 |  431 | `			SyMemBackendFree(&pGen->pVm->sAllocator,zAlloc);` |
|      ! 0 |  432 | `		}` |
|       49 |  433 | `		if( *piLevel < 1 ){` |
|        3 |  434 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 |  435 | `				"'%s' operator accepts only positive integers",zWhich);` |
|        2 |  436 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  437 | `		}` |
|       47 |  438 | `		return SXRET_OK;` |
|        - |  439 | `	}` |
|       12 |  440 | `	if( pStart->nType & (PH7_TK_REAL\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){` |
|        9 |  441 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        3 |  442 | `			"'%s' operator accepts only positive integers",zWhich);` |
|        6 |  443 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  444 | `	}` |
|        3 |  445 | `NonInteger:` |
|       21 |  446 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        7 |  447 | `		"'%s' operator with non-integer operand is no longer supported",zWhich);` |
|       14 |  448 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|    29980 |  449 | `}` |
|    42473 |  450 | `PH7_PRIVATE sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|        5 |  451 | `{` |
|        - |  452 | `	GenBlock *pLoop; /* Target loop */` |
|        - |  453 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|        - |  454 | `	sxi32 iRawLevel; /* The level as WRITTEN, kept for php's diagnostics */` |
|        - |  455 | `	sxu32 nLineLocal;` |
|        - |  456 | `	sxi32 rc;` |
|    42478 |  457 | `	iRawLevel = 1;` |
|    42478 |  458 | `	nLineLocal = pGen->pIn->nLine;` |
|    42478 |  459 | `	iLevel = 0;` |
|        - |  460 | `	/* Jump the 'continue' keyword */` |
|    42478 |  461 | `	pGen->pIn++;` |
|    42478 |  462 | `	rc = GenStateJumpLevelArg(&(*pGen),"continue",nLineLocal,&iRawLevel);` |
|    42478 |  463 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  464 | `		return SXERR_ABORT;` |
|    42478 |  465 | `	}else if( rc != SXRET_OK ){` |
|        9 |  466 | `		return SXRET_OK; /* Refused and reported */` |
|        - |  467 | `	}` |
|    42470 |  468 | `	if( pGen->pIn >= pGen->pEnd && GenStateAtChunkEofStmt(pGen) ){` |
|        - |  469 | `		/* php's parser wants the semicolon before it ever asks where the jump` |
|        - |  470 | ``		 * lands, so a `continue` that ends the FILE is its parse error and not a`` |
|        - |  471 | `		 * sentence about the loop this one is not in. */` |
|      ! 0 |  472 | `		rc = PH7_GenSyntaxError(&(*pGen),0,"\";\"");` |
|      ! 0 |  473 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - |  474 | `	}` |
|    42470 |  475 | `	iLevel = iRawLevel < 2 ? 0 : iRawLevel;` |
|        - |  476 | `	/* Point to the target loop */` |
|    42470 |  477 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    42470 |  478 | `	if( pLoop == 0 ){` |
|        - |  479 | ``		/* Same split php makes for `break`: no loop at all keeps the not-in-context`` |
|        - |  480 | ``		 * wording, too FEW loops is `Cannot 'continue' N levels`. */`` |
|       12 |  481 | `		if( GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,0) != 0 ){` |
|      ! 0 |  482 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|      ! 0 |  483 | `				"Cannot 'continue' %d level%s",iRawLevel,iRawLevel == 1 ? "" : "s");` |
|      ! 0 |  484 | `		}else{` |
|       12 |  485 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"'continue' not in the 'loop' or 'switch' context");` |
|        - |  486 | `		}` |
|       12 |  487 | `		if( rc == SXERR_ABORT ){` |
|        - |  488 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  489 | `			return SXERR_ABORT;` |
|        - |  490 | `		}` |
|        7 |  491 | `	}else{` |
|    42460 |  492 | `		sxu32 nInstrIdx = 0;` |
|    42460 |  493 | `		sxi32 iP1 = 0;` |
|        - |  494 | `		GenJumpScope sCross;` |
|    42460 |  495 | `		sxi32 iJmpOp = GenStateLoopJumpOp(&(*pGen),pLoop,&iP1,&sCross);` |
|    42460 |  496 | `		if( sCross.nFinally > 0 ){` |
|        3 |  497 | `			if( GenStateJumpOutOfFinally(&(*pGen),nLineLocal) == SXERR_ABORT ){` |
|      ! 0 |  498 | `				return SXERR_ABORT;` |
|        1 |  499 | `			}` |
|    42459 |  500 | `		}else if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|        - |  501 | ``			/* `continue` inside a switch acts like `break` — which is almost never what`` |
|        - |  502 | `			 * the author meant, so php 7.3+ says so at compile time. The generated jump` |
|        - |  503 | `			 * is unchanged; only the diagnostic was missing. An explicit level` |
|        - |  504 | ``			 * (`continue 2`) targets the enclosing loop and stays silent. */`` |
|       18 |  505 | `			if( iLevel < 1 ){` |
|       18 |  506 | `				PH7_GenCompileError(&(*pGen),E_WARNING,nLineLocal,` |
|        - |  507 | `					"\"continue\" targeting switch is equivalent to \"break\"."` |
|        - |  508 | `					" Did you mean to use \"continue 2\"?");` |
|        8 |  509 | `			}` |
|       18 |  510 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,iP1,0,0,&nInstrIdx);` |
|       18 |  511 | `			if( rc == SXRET_OK ){` |
|       18 |  512 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|        8 |  513 | `			}` |
|       10 |  514 | `		}else{` |
|        - |  515 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    42442 |  516 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,iP1,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    42442 |  517 | `			if( pLoop->bPostContinue == TRUE ){` |
|        - |  518 | `				JumpFixup sJumpFix;` |
|        - |  519 | `				/* Post-continue */` |
|    16989 |  520 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|    16989 |  521 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|    16989 |  522 | `				sJumpFix.pContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    16989 |  523 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|     8481 |  524 | `			}` |
|        - |  525 | `		}` |
|        - |  526 | `	}` |
|    42465 |  527 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0` |
|    21210 |  528 | `	 && PH7_GenSyntaxError(&(*pGen),pGen->pIn,"\";\"") == SXERR_ABORT ){` |
|      ! 0 |  529 | `		return SXERR_ABORT;` |
|        - |  530 | `	}` |
|        - |  531 | `	/* Statement successfully compiled */` |
|    42470 |  532 | `	return SXRET_OK;` |
|    21214 |  533 | `}` |
|        - |  534 | `/*` |
|        - |  535 | ` * Compile the 'break' statement.` |
|        - |  536 | ` * According to the PHP language reference` |
|        - |  537 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|        - |  538 | ` *  structure.` |
|        - |  539 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|        - |  540 | ` *  enclosing structures are to be broken out of.` |
|        - |  541 | ` */` |
|    17554 |  542 | `PH7_PRIVATE sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|        5 |  543 | `{` |
|        - |  544 | `	GenBlock *pLoop; /* Target loop */` |
|        - |  545 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|        - |  546 | `	sxi32 iRawLevel; /* The level as WRITTEN, kept for php's diagnostics */` |
|        - |  547 | `	sxu32 nLineLocal;` |
|        - |  548 | `	sxi32 rc;` |
|    17559 |  549 | `	iLevel = 0;` |
|    17559 |  550 | `	iRawLevel = 1;` |
|    17559 |  551 | `	nLineLocal = pGen->pIn->nLine;` |
|        - |  552 | `	/* Jump the 'break' keyword */` |
|    17559 |  553 | `	pGen->pIn++;` |
|    17559 |  554 | `	rc = GenStateJumpLevelArg(&(*pGen),"break",nLineLocal,&iRawLevel);` |
|    17559 |  555 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  556 | `		return SXERR_ABORT;` |
|    17559 |  557 | `	}else if( rc != SXRET_OK ){` |
|       31 |  558 | `		return SXRET_OK; /* Refused and reported */` |
|        - |  559 | `	}` |
|    17529 |  560 | `	if( pGen->pIn >= pGen->pEnd && GenStateAtChunkEofStmt(pGen) ){` |
|        - |  561 | ``		/* As for `continue` above: the missing semicolon is what php's parser meets`` |
|        - |  562 | `		 * first, before it can ask which loop this breaks out of. */` |
|        2 |  563 | `		rc = PH7_GenSyntaxError(&(*pGen),0,"\";\"");` |
|        2 |  564 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - |  565 | `	}` |
|    17527 |  566 | `	iLevel = iRawLevel < 2 ? 0 : iRawLevel;` |
|        - |  567 | `	/* Extract the target loop */` |
|    17527 |  568 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    17527 |  569 | `	if( pLoop == 0 ){` |
|        - |  570 | `		/* php distinguishes "there is no loop at all here" from "there is one, but` |
|        - |  571 | `		 * not N of them": the first keeps the not-in-context wording, the second is` |
|        - |  572 | ``		 * `Cannot 'break' N levels`. */`` |
|       21 |  573 | `		if( GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,0) != 0 ){` |
|        5 |  574 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        4 |  575 | `				"Cannot 'break' %d level%s",iRawLevel,iRawLevel == 1 ? "" : "s");` |
|        3 |  576 | `		}else{` |
|       17 |  577 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"'break' not in the 'loop' or 'switch' context");` |
|        - |  578 | `		}` |
|       21 |  579 | `		if( rc == SXERR_ABORT ){` |
|        - |  580 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  581 | `			return SXERR_ABORT;` |
|        - |  582 | `		}` |
|       12 |  583 | `	}else{` |
|        - |  584 | `		sxu32 nInstrIdx;` |
|    17509 |  585 | `		sxi32 iP1 = 0;` |
|        - |  586 | `		GenJumpScope sCross;` |
|    17509 |  587 | `		sxi32 iJmpOp = GenStateLoopJumpOp(&(*pGen),pLoop,&iP1,&sCross);` |
|    17509 |  588 | `		if( sCross.nFinally > 0 ){` |
|        9 |  589 | `			if( GenStateJumpOutOfFinally(&(*pGen),nLineLocal) == SXERR_ABORT ){` |
|      ! 0 |  590 | `				return SXERR_ABORT;` |
|        - |  591 | `			}` |
|        6 |  592 | `		}else{` |
|    17503 |  593 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,iP1,0,0,&nInstrIdx);` |
|    17503 |  594 | `			if( rc == SXRET_OK ){` |
|        - |  595 | `				/* Fix the jump later when the jump destination is resolved */` |
|    17503 |  596 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|     8738 |  597 | `			}` |
|        - |  598 | `		}` |
|        - |  599 | `	}` |
|    17522 |  600 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0` |
|     8755 |  601 | `	 && PH7_GenSyntaxError(&(*pGen),pGen->pIn,"\";\"") == SXERR_ABORT ){` |
|      ! 0 |  602 | `		return SXERR_ABORT;` |
|        - |  603 | `	}` |
|        - |  604 | `	/* Statement successfully compiled */` |
|    17527 |  605 | `	return SXRET_OK;` |
|     8771 |  606 | `}` |
|        - |  607 | `/*` |
|        - |  608 | ` * The function body a goto or a label sits in, or NULL at file scope. A goto may not` |
|        - |  609 | ` * cross functions, so GenStateFixGoto pairs the two on this.` |
|        - |  610 | ` */` |
|      446 |  611 | `static ph7_vm_func * GenStateOwningFunc(ph7_gen_state *pGen)` |
|        5 |  612 | `{` |
|      451 |  613 | `	GenBlock *pBlock = pGen->pCurrent;` |
|     1137 |  614 | `	while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|      691 |  615 | `		pBlock = pBlock->pParent;` |
|        5 |  616 | `	}` |
|      451 |  617 | `	return pBlock ? (ph7_vm_func *)pBlock->pUserData : 0;` |
|        5 |  618 | `}` |
|        - |  619 | `/*` |
|        - |  620 | ` * Compile or record a label.` |
|        - |  621 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|        - |  622 | ` * Example` |
|        - |  623 | ` *  goto LABEL;` |
|        - |  624 | ` *   echo 'Foo';` |
|        - |  625 | ` *  LABEL:` |
|        - |  626 | ` *   echo 'Bar';` |
|        - |  627 | ` */` |
|      220 |  628 | `PH7_PRIVATE sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|        5 |  629 | `{` |
|        - |  630 | `	Label sLabel;` |
|        - |  631 | `	/* php places almost NO restriction on where a label may be DEFINED — inside a loop, a` |
|        - |  632 | `	 * switch or a try{} is all fine; the one rule is that a name may not be declared twice` |
|        - |  633 | `	 * in the same function (below). The rest is on the jump: you may not goto INTO a loop` |
|        - |  634 | `	 * or switch from outside it, which is checked once the labels are all known (see` |
|        - |  635 | `	 * GenStateFixJumps). Record the loop this label sits in so that check can run. */` |
|        - |  636 | `	{` |
|      225 |  637 | `		SyString *pTarget = &pGen->pIn->sData;` |
|      225 |  638 | `		ph7_vm_func *pFunc = GenStateOwningFunc(&(*pGen));` |
|        - |  639 | `		char *zDup;` |
|        - |  640 | `		/* One name, one destination: php compile-rejects a label its function already` |
|        - |  641 | ``		 * declares, wherever the two sit (`L: L:`, one per branch of an if, one in a loop`` |
|        - |  642 | `		 * and one after it). PHL used to accept the redeclaration and silently give every` |
|        - |  643 | `		 * goto the FIRST one. The owning function is part of the key, so the same name in` |
|        - |  644 | `		 * another function — or at file scope beside it — is untouched by this. On the` |
|        - |  645 | `		 * duplicate, keep the first declaration and record nothing: the compile has already` |
|        - |  646 | `		 * failed, and a second entry under the same key would only shadow it. */` |
|      225 |  647 | `		if( SXRET_OK == GenStateGetLabel(&(*pGen),pTarget,pFunc,0) ){` |
|        8 |  648 | `			if( SXERR_ABORT == PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        2 |  649 | `				"Label '%z' already defined",pTarget) ){` |
|      ! 0 |  650 | `				return SXERR_ABORT;` |
|        - |  651 | `			}` |
|        6 |  652 | `			pGen->pIn += 2; /* Jump the label name and the semi-colon */` |
|        6 |  653 | `			return SXRET_OK;` |
|        - |  654 | `		}` |
|        - |  655 | `		/* Initialize label fields */` |
|      221 |  656 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|        - |  657 | `		/* Duplicate label name */` |
|      221 |  658 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|      221 |  659 | `		if( zDup == 0 ){` |
|      ! 0 |  660 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  661 | `			return SXERR_ABORT;` |
|        - |  662 | `		}` |
|      221 |  663 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|      221 |  664 | `		sLabel.nLine = pGen->pIn->nLine;` |
|      221 |  665 | `		sLabel.nLoopId = pGen->nCurLoopId;` |
|        - |  666 | `		/* Where the label sits, so a goto from a detached catch/finally body can be told` |
|        - |  667 | `		 * what it has to cross to reach it (GenStateJumpScope). */` |
|      221 |  668 | `		sLabel.pContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      221 |  669 | `		sLabel.nScopeId = pGen->nCurScopeId;` |
|        - |  670 | `		/* The owning FUNCTION, matched against the goto's in GenStateFixGoto. This used` |
|        - |  671 | `		 * to stop at GEN_BLOCK_EXCEPTION as well, which attributed every label inside a` |
|        - |  672 | `		 * try or catch body to "no function" — so from anywhere in a function such a` |
|        - |  673 | `		 * label read as undefined, including from the very catch body declaring it.` |
|        - |  674 | `		 * Whether a label may be jumped TO is decided by its container, not by this. */` |
|      221 |  675 | `		sLabel.pFunc = pFunc;` |
|        - |  676 | `		/* Insert in label set */` |
|      221 |  677 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|        - |  678 | `	}` |
|      221 |  679 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|      221 |  680 | `	return SXRET_OK;` |
|      115 |  681 | `}` |
|        - |  682 | `/*` |
|        - |  683 | ` * Compile the so hated 'goto' statement.` |
|        - |  684 | ` * You've probably been taught that gotos are bad, but this sort` |
|        - |  685 | ` * of rewriting  happens all the time, in fact every time you run` |
|        - |  686 | ` * a compiler it has to do this.` |
|        - |  687 | ` * According to the PHP language reference manual` |
|        - |  688 | ` *   The goto operator can be used to jump to another section in the program.` |
|        - |  689 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|        - |  690 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|        - |  691 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|        - |  692 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|        - |  693 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|        - |  694 | ` *   of a multi-level break` |
|        - |  695 | ` */` |
|      230 |  696 | `PH7_PRIVATE sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|        5 |  697 | `{` |
|        - |  698 | `	JumpFixup sJump;` |
|        - |  699 | `	sxi32 rc;` |
|      235 |  700 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|      235 |  701 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  702 | `		/* Missing label */` |
|      ! 0 |  703 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|      ! 0 |  704 | `		if( rc == SXERR_ABORT ){` |
|        - |  705 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  706 | `			return SXERR_ABORT;` |
|        - |  707 | `		}` |
|      ! 0 |  708 | `		return SXRET_OK;` |
|        - |  709 | `	}` |
|      235 |  710 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|        6 |  711 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn,"identifier");` |
|        6 |  712 | `		if( rc == SXERR_ABORT ){` |
|        - |  713 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  714 | `			return SXERR_ABORT;` |
|        - |  715 | `		}` |
|        4 |  716 | `	}else{` |
|      231 |  717 | `		SyString *pTarget = &pGen->pIn->sData;` |
|        - |  718 | `		char *zDup;` |
|        - |  719 | `		/* Prepare the jump destination */` |
|      231 |  720 | `		sJump.nJumpType = PH7_OP_JMP;` |
|      231 |  721 | `		sJump.nLine = pGen->pIn->nLine;` |
|        - |  722 | `		/* Gotos resolve at end of compilation, well after any container swap. */` |
|      231 |  723 | `		sJump.pContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      231 |  724 | `		sJump.nScopeId = pGen->nCurScopeId;` |
|        - |  725 | `		/* Duplicate label name */` |
|      231 |  726 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|      231 |  727 | `		if( zDup == 0 ){` |
|      ! 0 |  728 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  729 | `			return SXERR_ABORT;` |
|        - |  730 | `		}` |
|      231 |  731 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|        - |  732 | `		/* The loop/switch this goto sits in, for the "goto into a loop" check later. */` |
|      231 |  733 | `		sJump.nLoopId = pGen->nCurLoopId;` |
|        - |  734 | `		/* A goto inside a try{}/catch{} is legal php (jumping OUT of the block is fine);` |
|        - |  735 | `		 * only the owning function matters here, since a goto may not cross functions. */` |
|      231 |  736 | `		sJump.pFunc = GenStateOwningFunc(&(*pGen));` |
|        - |  737 | `		/* Emit the unconditional jump. Inside a DETACHED catch/finally body the target may` |
|        - |  738 | `		 * lie in another bytecode array, which a plain OP_JMP cannot address; enclosing` |
|        - |  739 | `		 * trys likewise need their finally run on the way out, which a plain jump would` |
|        - |  740 | `		 * skip. Emit a structure-crossing jump whenever either is possible — the label is` |
|        - |  741 | `		 * not known yet, so GenStateFixGoto picks the final opcode (and may downgrade it` |
|        - |  742 | `		 * back to OP_JMP once the counts prove to cancel). */` |
|      344 |  743 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,` |
|      226 |  744 | `			sJump.nScopeId > 0 ? PH7_OP_CATCH_JMP : PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|      231 |  745 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      113 |  746 | `		}` |
|        - |  747 | `	}` |
|      235 |  748 | `	pGen->pIn++; /* Jump the label name */` |
|        - |  749 | ``	/* php reads `goto LABEL ;` and nothing else: a stray token there is its parse`` |
|        - |  750 | `	 * error naming the token, where this said so in a sentence of its own. */` |
|      230 |  751 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0` |
|      121 |  752 | `	 && PH7_GenSyntaxError(&(*pGen),pGen->pIn,"\";\"") == SXERR_ABORT ){` |
|      ! 0 |  753 | `		return SXERR_ABORT;` |
|        - |  754 | `	}` |
|        - |  755 | `	/* Statement successfully compiled */` |
|      235 |  756 | `	return SXRET_OK;` |
|      120 |  757 | `}` |
|        - |  758 | `/*` |
|        - |  759 | ` * Point to the next PHP chunk that will be processed shortly.` |
|        - |  760 | ` * Return SXRET_OK on success. Any other return value indicates` |
|        - |  761 | ` * failure.` |
|        - |  762 | ` */` |
|      190 |  763 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|        5 |  764 | `{` |
|        - |  765 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|        - |  766 | `	sxu32 nRawObj;` |
|       95 |  767 | `	sxu32 nObjIdx;` |
|        - |  768 | `	/* Consume raw chunks verbatim without any processing until we get` |
|        - |  769 | `	 * a PHP block.` |
|        - |  770 | `	 */` |
|       98 |  771 | `Consume:` |
|      201 |  772 | `	nRawObj = nObjIdx = 0;` |
|      271 |  773 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|       74 |  774 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|       74 |  775 | `		if( pRawObj == 0 ){` |
|      ! 0 |  776 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  777 | `			return SXERR_ABORT;` |
|        - |  778 | `		}` |
|        - |  779 | `		/* Mark as constant and emit the load constant instruction */` |
|       74 |  780 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|       74 |  781 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|       74 |  782 | `		++nRawObj;` |
|       74 |  783 | `		pGen->pRawIn++; /* Next chunk */` |
|        4 |  784 | `	}` |
|      201 |  785 | `	if( nRawObj > 0 ){` |
|        - |  786 | `		/* Emit the consume instruction */` |
|       74 |  787 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|       35 |  788 | `	}` |
|      201 |  789 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|       72 |  790 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       72 |  791 | `		PH7_GenCarryBraces(pGen);` |
|        - |  792 | `		/* Reset the token set (and its trivia sidecar) */` |
|       72 |  793 | `		SySetReset(pTokenSet);` |
|       72 |  794 | `		SySetReset(&pGen->aTrivia);` |
|        - |  795 | `		/* Tokenize input */` |
|      106 |  796 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|       68 |  797 | `			pGen->pRawIn->nLine,pTokenSet,&pGen->aTrivia);` |
|        - |  798 | `		/* Point to the fresh token stream */` |
|       72 |  799 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|       72 |  800 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|        - |  801 | `		/* Only the chunk that met the end of the FILE can leave a statement` |
|        - |  802 | `		 * unterminated, and that is as true of a later block as of the first:` |
|        - |  803 | ``		 * `<?php if (1): ?>A<?php endif` wants its `;` in php. */`` |
|       72 |  804 | `		pGen->bChunkAtEof = (sxi8)(SX_PTR_TO_INT(pGen->pRawIn->pUserData) == 0);` |
|       72 |  805 | `		pGen->bChunkLast = pGen->bChunkAtEof;` |
|        - |  806 | `		/* Advance the stream cursor */` |
|       72 |  807 | `		pGen->pRawIn++;` |
|        - |  808 | `		{` |
|        - |  809 | `			SyToken *pTok;` |
|      224 |  810 | `			for( pTok = pGen->pIn ; pTok < pGen->pEnd ; pTok++ ){` |
|      156 |  811 | `				if( pTok->nType & PH7_TK_OCB ){` |
|        5 |  812 | `					pGen->nBraceNet++;` |
|      154 |  813 | `				}else if( pTok->nType & PH7_TK_CCB ){` |
|       24 |  814 | `					pGen->nBraceNet--;` |
|       10 |  815 | `				}` |
|       80 |  816 | `			}` |
|        - |  817 | `		}` |
|       72 |  818 | `		if( pGen->nBraceNet > 0 ){` |
|        - |  819 | ``			/* A `{` still open at the end of the file is what php reports there, as`` |
|        - |  820 | `			 * in the first chunk: stand the end-of-input questions down. */` |
|       30 |  821 | `			pGen->bChunkAtEof = 0;` |
|       13 |  822 | `		}` |
|       72 |  823 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - |  824 | ``			/* The chunk held no TOKENS. `<?php // note ?>` is a whole PHP block that`` |
|        - |  825 | `			 * produces none, and so is a block holding only a comment of any kind --` |
|        - |  826 | `			 * the lexer emits the chunk (its bytes are not empty) and tokenizing it` |
|        - |  827 | `			 * yields nothing. Handing that empty stream back reads as END OF INPUT to` |
|        - |  828 | ``			 * every caller, so the enclosing block ended there and the real `}` two`` |
|        - |  829 | `			 * lines later was "Unmatched". A php TEMPLATE writes exactly this shape --` |
|        - |  830 | ``			 * `<?php } else { ?>` … `<?php // why ?>` … `<?php } ?>` is symfony's`` |
|        - |  831 | `			 * error-handler view -- so take the NEXT chunk instead. The cursor has` |
|        - |  832 | `			 * already advanced, so the loop always makes progress. */` |
|        8 |  833 | `			goto Consume;` |
|        - |  834 | `		}` |
|        - |  835 | `		/* TICKET 1433-011 */` |
|       66 |  836 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|        - |  837 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|        - |  838 | ``			/* The block opens with an `echo` statement and goes on after it,`` |
|        - |  839 | `			 * exactly as in the first chunk: hand it to the statement loop. */` |
|        8 |  840 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|        8 |  841 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|        8 |  842 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|        3 |  843 | `		}` |
|       35 |  844 | `	}else{` |
|        - |  845 | `		/* No more chunks to process */` |
|      132 |  846 | `		pGen->pIn = pGen->pEnd;` |
|      132 |  847 | `		return SXERR_EOF;` |
|        - |  848 | `	}` |
|       66 |  849 | `	return SXRET_OK;` |
|      100 |  850 | `}` |
|        - |  851 | `/*` |
|        - |  852 | `` * A `{` the input never closed. php's scanner says so where the input ENDS --`` |
|        - |  853 | ` * past a trailing newline -- and names the line the brace was opened on only` |
|        - |  854 | ` * when that is another line.` |
|        - |  855 | ` */` |
|       90 |  856 | `static sxi32 GenStateUnclosedBrace(ph7_gen_state *pGen,sxu32 nOpenLine)` |
|        4 |  857 | `{` |
|       94 |  858 | `	if( pGen->nChunkEofLine > nOpenLine ){` |
|       68 |  859 | `		return PH7_GenCompileError(&(*pGen),E_PARSE,pGen->nChunkEofLine,"Unclosed '{' on line %u",nOpenLine);` |
|        - |  860 | `	}` |
|       28 |  861 | `	return PH7_GenCompileError(&(*pGen),E_PARSE,nOpenLine,"Unclosed '{'");` |
|       49 |  862 | `}` |
|        - |  863 | `/*` |
|        - |  864 | `` * A statement head -- `if (`, `while (`, `switch (`, `for (`, `foreach (` --`` |
|        - |  865 | `` * whose `(` nothing closes. php has no sentence of its own for that: its scanner`` |
|        - |  866 | ` * and its parser go down the same tokens and the first to refuse speaks. The` |
|        - |  867 | `` * PARSER refuses a `;` or a `{` that cannot continue what the head holds, and`` |
|        - |  868 | ` * names the token; the SCANNER refuses a closer that does not match the innermost` |
|        - |  869 | ` * open bracket, and the end of the input while one is still open, and names the` |
|        - |  870 | ` * BRACKET.` |
|        - |  871 | ` *` |
|        - |  872 | ` * So walk the tokens the way the scanner does, with its nesting stack, and stop` |
|        - |  873 | `` * where either would. A `{` continues an expression only where a closure, a`` |
|        - |  874 | `` * match or an anonymous class owes its body, or behind `$`, `->` and `::`.`` |
|        - |  875 | ` *` |
|        - |  876 | ` * What the parser was still waiting for is part of its sentence and depends on` |
|        - |  877 | `` * the head: a `for` wants the `;` of its first two clauses and then its `)`, a`` |
|        - |  878 | `` * `foreach` target could still be dereferenced, an argument list or a subscript`` |
|        - |  879 | ` * wants its own closer, and a plain condition could be continued by too many` |
|        - |  880 | ` * tokens for php to list any.` |
|        - |  881 | ` */` |
|        - |  882 | `#define PHL_HEAD_COND    0 /* one expression */` |
|        - |  883 | ``#define PHL_HEAD_FOR     1 /* three clauses, `;` between them */`` |
|        - |  884 | ``#define PHL_HEAD_FOREACH 2 /* an expression, `as`, the targets */`` |
|        - |  885 | `#define PHL_HEAD_NEST    32` |
|      210 |  886 | `static int GenStateTokIsText(const SyToken *pTok,const char *zText,sxu32 nLen)` |
|        1 |  887 | `{` |
|      217 |  888 | `	return pTok->sData.nByte == nLen` |
|      210 |  889 | `	    && SyMemcmp((const void *)pTok->sData.zString,(const void *)zText,nLen) == 0;` |
|        1 |  890 | `}` |
|      230 |  891 | `static sxi32 GenStateUnclosedHead(ph7_gen_state *pGen,SyToken *pOpen,sxi32 iKind)` |
|        1 |  892 | `{` |
|        - |  893 | `	struct { char c; sxu32 nLine; } aNest[PHL_HEAD_NEST];` |
|      231 |  894 | `	SyToken *pLimit = pGen->pEnd; /* Where the scanner's walk ends */` |
|        - |  895 | `	SyToken *pStop;` |
|      231 |  896 | `	const char *zExpect = 0;` |
|      231 |  897 | ``	sxi32 nNest = 1;   /* The head's own `(` */`` |
|      231 |  898 | `	sxi32 nOwed = 0;   /* Bodies a closure, a match or a class has announced */` |
|      231 |  899 | ``	sxi32 nSemi = 0;   /* `for` clauses closed so far */`` |
|      231 |  900 | ``	int bAs = 0;       /* A `foreach` has reached its targets */`` |
|        - |  901 | `	sxi32 rc;` |
|      231 |  902 | `	if( pGen->pTokenSet ){` |
|        - |  903 | ``		/* A function body is compiled in a slice that stops at its `}`, and that`` |
|        - |  904 | ``		 * `}` is the closer php's scanner refuses. */`` |
|      231 |  905 | `		SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|      231 |  906 | `		SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|      231 |  907 | `		if( pGen->pEnd >= pBase && pGen->pEnd <= pStreamEnd ){` |
|      231 |  908 | `			pLimit = pStreamEnd;` |
|      115 |  909 | `		}` |
|      115 |  910 | `	}` |
|      231 |  911 | `	aNest[0].c = '(';` |
|      231 |  912 | `	aNest[0].nLine = pOpen->nLine;` |
|      725 |  913 | `	for( pStop = &pOpen[1] ; pStop < pLimit ; pStop++ ){` |
|      635 |  914 | `		sxu32 nType = pStop->nType;` |
|      635 |  915 | `		char cOpen = 0,cClose = 0;` |
|      635 |  916 | `		if( nType & PH7_TK_KEYWORD ){` |
|       41 |  917 | `			sxi32 nKey = SX_PTR_TO_INT(pStop->pUserData);` |
|       40 |  918 | `			if( nKey == PH7_TKWRD_FUNCTION \|\| nKey == PH7_TKWRD_MATCH` |
|       36 |  919 | `			 \|\| (nKey == PH7_TKWRD_CLASS && !GenStateTokIsText(&pStop[-1],"::",2)) ){` |
|        9 |  920 | `				nOwed++;` |
|       37 |  921 | `			}else if( nKey == PH7_TKWRD_AS ){` |
|        - |  922 | `				sxi32 n;` |
|       31 |  923 | `				for( n = 1 ; n < nNest && n < PHL_HEAD_NEST && aNest[n].c != '{' ; n++ );` |
|       27 |  924 | `				if( nNest > 1 && n >= nNest ){` |
|        - |  925 | ``					/* Inside a bracket that is not a closure's body no `as` is`` |
|        - |  926 | ``					 * the foreach's own: `foreach (([] as $v)`. */`` |
|        5 |  927 | `					break;` |
|        - |  928 | `				}` |
|       23 |  929 | `				bAs = (nNest == 1);` |
|       11 |  930 | `			}` |
|       37 |  931 | `			continue;` |
|        - |  932 | `		}` |
|      595 |  933 | `		if( nType & PH7_TK_LPAREN ){` |
|       35 |  934 | `			cOpen = '(';` |
|      578 |  935 | `		}else if( nType & PH7_TK_OSB ){` |
|       37 |  936 | `			cOpen = '[';` |
|      543 |  937 | `		}else if( nType & PH7_TK_OCB ){` |
|       81 |  938 | `			if( nOwed > 0 ){` |
|        9 |  939 | `				nOwed--;` |
|       77 |  940 | `			}else if( !(pStop[-1].nType & PH7_TK_DOLLAR)` |
|       71 |  941 | `			 && !GenStateTokIsText(&pStop[-1],"->",2)` |
|       69 |  942 | `			 && !GenStateTokIsText(&pStop[-1],"?->",3)` |
|       69 |  943 | `			 && !GenStateTokIsText(&pStop[-1],"::",2) ){` |
|       69 |  944 | `				break;` |
|        - |  945 | `			}` |
|       13 |  946 | `			cOpen = '{';` |
|      451 |  947 | `		}else if( nType & PH7_TK_RPAREN ){` |
|       21 |  948 | `			cClose = ')';` |
|      435 |  949 | `		}else if( nType & PH7_TK_CSB ){` |
|       49 |  950 | `			cClose = ']';` |
|      401 |  951 | `		}else if( nType & PH7_TK_CCB ){` |
|       33 |  952 | `			cClose = '}';` |
|      361 |  953 | `		}else if( nType & PH7_TK_SEMI ){` |
|       75 |  954 | `			if( nNest <= PHL_HEAD_NEST && aNest[nNest-1].c == '{' ){` |
|        - |  955 | `				/* A statement of a closure's body */` |
|        5 |  956 | `				continue;` |
|        - |  957 | `			}` |
|       71 |  958 | `			if( nNest == 1 && iKind == PHL_HEAD_FOR && nSemi < 2 ){` |
|       43 |  959 | `				nSemi++;` |
|       43 |  960 | `				continue;` |
|        - |  961 | `			}` |
|       29 |  962 | `			break;` |
|        - |  963 | `		}` |
|      453 |  964 | `		if( cOpen ){` |
|       83 |  965 | `			if( nNest < PHL_HEAD_NEST ){` |
|       83 |  966 | `				aNest[nNest].c = cOpen;` |
|       83 |  967 | `				aNest[nNest].nLine = pStop->nLine;` |
|       41 |  968 | `			}` |
|       83 |  969 | `			nNest++;` |
|      412 |  970 | `		}else if( cClose ){` |
|      101 |  971 | `			char cTop = nNest <= PHL_HEAD_NEST ? aNest[nNest-1].c : 0;` |
|      100 |  972 | `			if( cTop == 0 \|\| (cTop == '(' && cClose == ')') \|\| (cTop == '[' && cClose == ']')` |
|       69 |  973 | `			 \|\| (cTop == '{' && cClose == '}') ){` |
|        - |  974 | ``				/* The head's own `(` is never popped: nothing closes it, which is`` |
|        - |  975 | `				 * what brought us here. */` |
|       61 |  976 | `				nNest--;` |
|       61 |  977 | `				continue;` |
|        - |  978 | `			}` |
|        - |  979 | `			/* php's scanner: the innermost bracket, the line it was opened on when` |
|        - |  980 | `			 * that is another line, and the closer it met instead. */` |
|       41 |  981 | `			if( aNest[nNest-1].nLine != pStop->nLine ){` |
|       28 |  982 | `				return PH7_GenCompileError(&(*pGen),E_PARSE,pStop->nLine,` |
|       18 |  983 | `					"Unclosed '%c' on line %u does not match '%c'",cTop,aNest[nNest-1].nLine,cClose);` |
|        - |  984 | `			}` |
|       34 |  985 | `			return PH7_GenCompileError(&(*pGen),E_PARSE,pStop->nLine,` |
|       11 |  986 | `				"Unclosed '%c' does not match '%c'",cTop,cClose);` |
|        - |  987 | `		}` |
|      177 |  988 | `	}` |
|      191 |  989 | `	if( pStop >= pLimit && pGen->bChunkLast ){` |
|        - |  990 | `		/* The input ended with the bracket open. */` |
|       87 |  991 | `		char cTop = nNest <= PHL_HEAD_NEST ? aNest[nNest-1].c : '(';` |
|       87 |  992 | `		sxu32 nOpenLine = nNest <= PHL_HEAD_NEST ? aNest[nNest-1].nLine : pOpen->nLine;` |
|       87 |  993 | `		if( pGen->nChunkEofLine > nOpenLine ){` |
|       79 |  994 | `			return PH7_GenCompileError(&(*pGen),E_PARSE,pGen->nChunkEofLine,` |
|       26 |  995 | `				"Unclosed '%c' on line %u",cTop,nOpenLine);` |
|        - |  996 | `		}` |
|       35 |  997 | `		return PH7_GenCompileError(&(*pGen),E_PARSE,nOpenLine,"Unclosed '%c'",cTop);` |
|        - |  998 | `	}` |
|        - |  999 | `	/* The parser's refusal. What it was waiting for: */` |
|      105 | 1000 | `	if( nNest > 1 ){` |
|       13 | 1001 | `		if( nNest <= PHL_HEAD_NEST && pStop[-1].nType` |
|       12 | 1002 | `		     & (PH7_TK_INTEGER\|PH7_TK_REAL\|PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_RPAREN\|PH7_TK_CSB) ){` |
|        - | 1003 | `			/* An operand is complete inside an argument list or a subscript. */` |
|       11 | 1004 | `			SyToken *pInner = pStop;` |
|       11 | 1005 | `			sxi32 nBack = 0;` |
|       25 | 1006 | `			while( pInner > pOpen ){` |
|       25 | 1007 | `				pInner--;` |
|       25 | 1008 | `				if( pInner->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        5 | 1009 | `					nBack++;` |
|       23 | 1010 | `				}else if( pInner->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       15 | 1011 | `					if( nBack == 0 ){` |
|       11 | 1012 | `						break;` |
|        - | 1013 | `					}` |
|        5 | 1014 | `					nBack--;` |
|        2 | 1015 | `				}` |
|        1 | 1016 | `			}` |
|       10 | 1017 | `			if( pInner > pOpen && (pInner->nType & PH7_TK_OSB)` |
|        7 | 1018 | `			 && (pInner[-1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_RPAREN\|PH7_TK_CSB)) ){` |
|        3 | 1019 | `				zExpect = "\"]\"";` |
|       10 | 1020 | `			}else if( pInner > pOpen && (pInner->nType & PH7_TK_LPAREN)` |
|        8 | 1021 | `			 && (pInner[-1].nType & (PH7_TK_ID\|PH7_TK_RPAREN\|PH7_TK_CSB))` |
|        7 | 1022 | `			 && !(pInner[-1].nType & PH7_TK_KEYWORD) ){` |
|        5 | 1023 | `				zExpect = "\")\"";` |
|        2 | 1024 | `			}` |
|        6 | 1025 | `		}` |
|       99 | 1026 | `	}else if( iKind == PHL_HEAD_FOR ){` |
|       19 | 1027 | `		zExpect = nSemi < 2 ? "\";\"" : "\")\"";` |
|       84 | 1028 | `	}else if( iKind == PHL_HEAD_FOREACH ){` |
|       18 | 1029 | `		if( bAs && !(pStop[-1].nType & PH7_TK_ARRAY_OP)` |
|       12 | 1030 | `		 && !((pStop[-1].nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pStop[-1].pUserData) == PH7_TKWRD_AS) ){` |
|        9 | 1031 | `			zExpect = "\"->\" or \"?->\" or \"[\"";` |
|        5 | 1032 | `		}` |
|       66 | 1033 | `	}else if( pStop <= pGen->pEnd ){` |
|        - | 1034 | `		/* A condition is one expression, and whatever is wrong INSIDE it the parser` |
|        - | 1035 | ``		 * met first: `if (1 2 {` names the 2. */`` |
|       57 | 1036 | `		SyToken *pTmp = pGen->pEnd;` |
|       57 | 1037 | `		pGen->pIn = &pOpen[1];` |
|       57 | 1038 | `		pGen->pEnd = pStop;` |
|       57 | 1039 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       57 | 1040 | `		if( rc != SXERR_ABORT && pGen->pIn < pStop ){` |
|      ! 0 | 1041 | `			rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 | 1042 | `		}` |
|       57 | 1043 | `		pGen->pEnd = pTmp;` |
|       57 | 1044 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1045 | `			return SXERR_ABORT;` |
|        - | 1046 | `		}` |
|       28 | 1047 | `	}` |
|      105 | 1048 | `	if( pStop >= pLimit ){` |
|        - | 1049 | ``		/* The block was closed by a `?>`, which is php's `;` -- one a `for` still`` |
|        - | 1050 | `		 * owed takes it, and what php then refuses is the text behind the tag. */` |
|        5 | 1051 | `		if( zExpect && !(iKind == PHL_HEAD_FOR && nNest == 1 && nSemi < 2) ){` |
|        4 | 1052 | `			return PH7_GenCompileError(&(*pGen),E_PARSE,pStop[-1].nLine,` |
|        1 | 1053 | `				"syntax error, unexpected token \";\", expecting %s",zExpect);` |
|        - | 1054 | `		}` |
|        3 | 1055 | `		return PH7_GenCompileError(&(*pGen),E_PARSE,pStop[-1].nLine,` |
|        - | 1056 | `			"syntax error, unexpected token \";\"");` |
|        - | 1057 | `	}` |
|      101 | 1058 | `	return PH7_GenSyntaxError(&(*pGen),pStop,zExpect);` |
|      116 | 1059 | `}` |
|        - | 1060 | `/*` |
|        - | 1061 | `` * php's grammar closes an alternative-syntax body with the `end*` keyword AND a`` |
|        - | 1062 | `` * `;` -- `endwhile echo 2;` is `unexpected token "echo", expecting ";"` there.`` |
|        - | 1063 | `` * A closing tag counts as the `;`, and a chunk that stops right after the keyword`` |
|        - | 1064 | ` * stopped at one or at the end of the file, which the statement loop reports.` |
|        - | 1065 | ` */` |
|      162 | 1066 | `static sxi32 GenStateEndKeywordSemi(ph7_gen_state *pGen)` |
|        3 | 1067 | `{` |
|      165 | 1068 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      131 | 1069 | `		return SXRET_OK;` |
|        - | 1070 | `	}` |
|       35 | 1071 | `	if( pGen->pIn->nType & (PH7_TK_RPAREN\|PH7_TK_CSB) ){` |
|        - | 1072 | ``		/* Never matched here -- the body sits in a `{` or at the top -- so php's`` |
|        - | 1073 | `		 * scanner refuses it first, and the expression compiler says so. */` |
|        5 | 1074 | `		return SXRET_OK;` |
|        - | 1075 | `	}` |
|       31 | 1076 | `	if( pGen->pIn->nType & PH7_TK_CCB ){` |
|        - | 1077 | ``		/* Likewise a `}` with no `{` open around it. */`` |
|        7 | 1078 | `		SyToken *pTok = pGen->pIn;` |
|        7 | 1079 | `		SyToken *pStreamEnd = &((SyToken *)SySetBasePtr(pGen->pTokenSet))[SySetUsed(pGen->pTokenSet)];` |
|        7 | 1080 | `		sxi32 nOpen = pGen->nBraceNet;` |
|       13 | 1081 | `		for( ; pTok < pStreamEnd ; pTok++ ){` |
|        7 | 1082 | `			if( pTok->nType & PH7_TK_OCB ){` |
|      ! 0 | 1083 | `				nOpen--;` |
|        7 | 1084 | `			}else if( pTok->nType & PH7_TK_CCB ){` |
|        7 | 1085 | `				nOpen++;` |
|        3 | 1086 | `			}` |
|        4 | 1087 | `		}` |
|        7 | 1088 | `		if( nOpen <= 0 ){` |
|        3 | 1089 | `			return SXRET_OK;` |
|        - | 1090 | `		}` |
|        2 | 1091 | `	}` |
|       29 | 1092 | `	return PH7_GenSyntaxError(&(*pGen),pGen->pIn,"\";\"");` |
|       84 | 1093 | `}` |
|        - | 1094 | `/*` |
|        - | 1095 | ` * Compile a PHP block.` |
|        - | 1096 | ` * A block is simply one or more PHP statements and expressions to compile` |
|        - | 1097 | ` * optionally delimited by braces {}.` |
|        - | 1098 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|        - | 1099 | ` * and this function takes care of generating the appropriate error` |
|        - | 1100 | ` * message.` |
|        - | 1101 | ` */` |
|  1082223 | 1102 | `PH7_PRIVATE sxi32 PH7_CompileBlock(` |
|        - | 1103 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 1104 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|        - | 1105 | `	)` |
|        5 | 1106 | `{` |
|        - | 1107 | `	sxi32 rc;` |
|        - | 1108 | `	sxu32 nLine;` |
|  1082228 | 1109 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  1080308 | 1110 | `		nLine = pGen->pIn->nLine;` |
|  1080308 | 1111 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  1080308 | 1112 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1113 | `			return SXERR_ABORT;` |
|        - | 1114 | `		}` |
|  1080308 | 1115 | `		pGen->pIn++;` |
|        - | 1116 | `		/* Compile until we hit the closing braces '}' */` |
|  1673783 | 1117 | `		for(;;){` |
|  3352088 | 1118 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      115 | 1119 | `				rc = GenStateNextChunk(&(*pGen));` |
|      115 | 1120 | `				if (rc == SXERR_ABORT ){` |
|      ! 0 | 1121 | `			 	   return SXERR_ABORT;` |
|        - | 1122 | `				}` |
|      115 | 1123 | `				if( rc == SXERR_EOF ){` |
|        - | 1124 | `					/* No more token to process: the block was never closed. */` |
|       88 | 1125 | `					GenStateUnclosedBrace(&(*pGen),nLine);` |
|       88 | 1126 | `					break;` |
|        - | 1127 | `				}` |
|       13 | 1128 | `			}` |
|  3352004 | 1129 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|        - | 1130 | `				/* Closing braces found,break immediately*/` |
|  1080214 | 1131 | `				pGen->pIn++;` |
|  1080214 | 1132 | `				break;` |
|        - | 1133 | `			}` |
|        - | 1134 | `			/* Compile a single statement */` |
|  2271795 | 1135 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  2271795 | 1136 | `			if( rc == SXERR_ABORT ){` |
|       12 | 1137 | `				return SXERR_ABORT;` |
|        - | 1138 | `			}` |
|        5 | 1139 | `		}` |
|  1080298 | 1140 | `		GenStateLeaveBlock(&(*pGen),0);` |
|   541440 | 1141 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|        - | 1142 | `		/* An if/elseif body may still be followed by either branch, an else` |
|        - | 1143 | ``		 * body only by its `endif` -- php's parser names the three words for the`` |
|        - | 1144 | `		 * first and nothing for the second. The token before the ':' says which. */` |
|      236 | 1145 | `		int bIfBody = nKeywordEnd == PH7_TKWRD_ENDIF` |
|      196 | 1146 | `			&& !((pGen->pIn[-1].nType & PH7_TK_KEYWORD)` |
|       58 | 1147 | `				&& SX_PTR_TO_INT(pGen->pIn[-1].pUserData) == PH7_TKWRD_ELSE);` |
|      185 | 1148 | `		pGen->pIn++;` |
|      185 | 1149 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|      185 | 1150 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1151 | `			return SXERR_ABORT;` |
|        - | 1152 | `		}` |
|      185 | 1153 | `		if( bIfBody ){` |
|       91 | 1154 | `			pGen->pCurrent->zInnerTail = "\"elseif\" or \"else\" or \"endif\"";` |
|       44 | 1155 | `		}` |
|        - | 1156 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|      137 | 1157 | `		for(;;){` |
|      277 | 1158 | `			if( pGen->pIn >= pGen->pEnd ){` |
|       46 | 1159 | `				rc = GenStateNextChunk(&(*pGen));` |
|       46 | 1160 | `				if (rc == SXERR_ABORT ){` |
|      ! 0 | 1161 | `			 	   return SXERR_ABORT;` |
|        - | 1162 | `				}` |
|       46 | 1163 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|        - | 1164 | `					/* No more token to process */` |
|       29 | 1165 | `					if( rc == SXERR_EOF && pGen->nBraceNet <= 0 ){` |
|        - | 1166 | `						/* The input ended inside the body: php's parse error, naming` |
|        - | 1167 | ``						 * what an if/elseif body wanted. A `{` still open around it is`` |
|        - | 1168 | `						 * its scanner's refusal instead, which that block reports. */` |
|       25 | 1169 | `						rc = PH7_GenSyntaxError(&(*pGen),0,pGen->pCurrent->zInnerTail);` |
|       25 | 1170 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 1171 | `							return SXERR_ABORT;` |
|        - | 1172 | `						}` |
|       12 | 1173 | `					}` |
|       29 | 1174 | `					break;` |
|        - | 1175 | `				}` |
|        8 | 1176 | `			}` |
|      249 | 1177 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        - | 1178 | `				sxi32 nKwrd;` |
|        - | 1179 | `				/* Keyword found */` |
|      245 | 1180 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      273 | 1181 | `				if( nKwrd == nKeywordEnd \|\|` |
|       89 | 1182 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|        - | 1183 | `						/* Delimiter keyword found,break */` |
|      157 | 1184 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|      135 | 1185 | `							pGen->pIn++; /*  endif;endswitch... */` |
|      135 | 1186 | `							if( GenStateEndKeywordSemi(&(*pGen)) == SXERR_ABORT ){` |
|      ! 0 | 1187 | `								return SXERR_ABORT;` |
|        - | 1188 | `							}` |
|       66 | 1189 | `						}` |
|      157 | 1190 | `						break;` |
|        - | 1191 | `				}` |
|       44 | 1192 | `			}` |
|        - | 1193 | `			/* Compile a single statement */` |
|       94 | 1194 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|       94 | 1195 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1196 | `				return SXERR_ABORT;` |
|        - | 1197 | `			}` |
|        2 | 1198 | `		}` |
|      185 | 1199 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       94 | 1200 | `	}else{` |
|        - | 1201 | `		/* Compile a single statement */` |
|     1743 | 1202 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     1743 | 1203 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1204 | `			return SXERR_ABORT;` |
|        - | 1205 | `		}` |
|        - | 1206 | `	}` |
|        - | 1207 | `	/* Jump trailing semi-colons ';' */` |
|  1082294 | 1208 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       79 | 1209 | `		pGen->pIn++;` |
|        3 | 1210 | `	}` |
|  1082218 | 1211 | `	return SXRET_OK;` |
|   540394 | 1212 | `}` |
|        - | 1213 | `/*` |
|        - | 1214 | ` * Compile the gentle 'while' statement.` |
|        - | 1215 | ` * According to the PHP language reference` |
|        - | 1216 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|        - | 1217 | ` *  The basic form of a while statement is:` |
|        - | 1218 | ` *  while (expr)` |
|        - | 1219 | ` *   statement` |
|        - | 1220 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|        - | 1221 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|        - | 1222 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|        - | 1223 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|        - | 1224 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|        - | 1225 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|        - | 1226 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|        - | 1227 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|        - | 1228 | ` *  while (expr):` |
|        - | 1229 | ` *    statement` |
|        - | 1230 | ` *   endwhile;` |
|        - | 1231 | ` */` |
|    17534 | 1232 | `PH7_PRIVATE sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|        5 | 1233 | `{` |
|    17539 | 1234 | `	GenBlock *pWhileBlock = 0;` |
|    17539 | 1235 | `	SyToken *pTmp,*pEnd = 0;` |
|        - | 1236 | `	sxu32 nFalseJump;` |
|        - | 1237 | `	sxi32 rc;` |
|        - | 1238 | `	/* Jump the 'while' keyword */` |
|    17539 | 1239 | `	pGen->pIn++;` |
|    17539 | 1240 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1241 | `		/* php's parse error names the token it found instead */` |
|       11 | 1242 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|       11 | 1243 | `		if( rc == SXERR_ABORT ){` |
|        - | 1244 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1245 | `			return SXERR_ABORT;` |
|        - | 1246 | `		}` |
|       11 | 1247 | `		goto Synchronize;` |
|        - | 1248 | `	}` |
|        - | 1249 | `	/* Jump the left parenthesis '(' */` |
|    17529 | 1250 | `	pGen->pIn++;` |
|        - | 1251 | `	/* Delimit the condition */` |
|    17529 | 1252 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    17529 | 1253 | `	if( pEnd >= pGen->pEnd ){` |
|       25 | 1254 | `		rc = GenStateUnclosedHead(&(*pGen),&pGen->pIn[-1],PHL_HEAD_COND);` |
|       25 | 1255 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1256 | `			return SXERR_ABORT;` |
|        - | 1257 | `		}` |
|       25 | 1258 | `		goto Synchronize;` |
|        - | 1259 | `	}` |
|        - | 1260 | `	/* Create the loop block */` |
|    17505 | 1261 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|    17505 | 1262 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1263 | `		return SXERR_ABORT;` |
|        - | 1264 | `	}` |
|    17505 | 1265 | `	if( pGen->pIn == pEnd ){` |
|        - | 1266 | `		/* Empty condition. php reports the token that actually stopped it -- for` |
|        - | 1267 | ``		 * `while ()` that is the ')' -- not a hand-written "expected expression". */`` |
|        6 | 1268 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);` |
|        6 | 1269 | `		if( rc == SXERR_ABORT ){` |
|        - | 1270 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1271 | `			return SXERR_ABORT;` |
|        - | 1272 | `		}` |
|        2 | 1273 | `	}` |
|        - | 1274 | `	/* Swap token streams */` |
|    17505 | 1275 | `	pTmp = pGen->pEnd;` |
|    17505 | 1276 | `	pGen->pEnd = pEnd;` |
|        - | 1277 | `	/* Compile the expression */` |
|    17505 | 1278 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    17505 | 1279 | `	if( rc == SXERR_ABORT ){` |
|        - | 1280 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1281 | `		return SXERR_ABORT;` |
|        - | 1282 | `	}` |
|        - | 1283 | `	/* Update token stream */` |
|    17505 | 1284 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 | 1285 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 | 1286 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1287 | `			return SXERR_ABORT;` |
|        - | 1288 | `		}` |
|      ! 0 | 1289 | `		pGen->pIn++;` |
|      ! 0 | 1290 | `	}` |
|        - | 1291 | `	/* Synchronize pointers */` |
|    17505 | 1292 | `	pGen->pIn  = &pEnd[1];` |
|    17505 | 1293 | `	pGen->pEnd = pTmp;` |
|        - | 1294 | `	/* Emit the false jump */` |
|    17505 | 1295 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|        - | 1296 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    17505 | 1297 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|        - | 1298 | `	/* Compile the loop body */` |
|    17505 | 1299 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|    17505 | 1300 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 1301 | `		return SXERR_ABORT;` |
|        - | 1302 | `	}` |
|        - | 1303 | `	/* Emit the unconditional jump to the start of the loop */` |
|    17505 | 1304 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|        - | 1305 | `	/* Fix all jumps now the destination is resolved */` |
|    17505 | 1306 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 1307 | `	/* Release the loop block */` |
|    17505 | 1308 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 1309 | `	/* Statement successfully compiled */` |
|    17505 | 1310 | `	return SXRET_OK;` |
|       17 | 1311 | `Synchronize:` |
|        - | 1312 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - | 1313 | `	 * compiling this erroneous block.` |
|        - | 1314 | `	 */` |
|       59 | 1315 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       25 | 1316 | `		pGen->pIn++;` |
|        1 | 1317 | `	}` |
|       35 | 1318 | `	return SXRET_OK;` |
|     8761 | 1319 | `}` |
|        - | 1320 | `/*` |
|        - | 1321 | ` * Compile the ugly do..while() statement.` |
|        - | 1322 | ` * According to the PHP language reference` |
|        - | 1323 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|        - | 1324 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|        - | 1325 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|        - | 1326 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|        - | 1327 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|        - | 1328 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|        - | 1329 | ` *  would end immediately).` |
|        - | 1330 | ` *  There is just one syntax for do-while loops:` |
|        - | 1331 | ` *  <?php` |
|        - | 1332 | ` *  $i = 0;` |
|        - | 1333 | ` *  do {` |
|        - | 1334 | ` *   echo $i;` |
|        - | 1335 | ` *  } while ($i > 0);` |
|        - | 1336 | ` * ?>` |
|        - | 1337 | ` */` |
|       48 | 1338 | `PH7_PRIVATE sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|        3 | 1339 | `{` |
|       51 | 1340 | `	SyToken *pTmp,*pEnd = 0;` |
|       51 | 1341 | `	GenBlock *pDoBlock = 0;` |
|        - | 1342 | `	sxi32 rc;` |
|        - | 1343 | `	/* Jump the 'do' keyword */` |
|       51 | 1344 | `	pGen->pIn++;` |
|        - | 1345 | `	/* Create the loop block */` |
|       51 | 1346 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       51 | 1347 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1348 | `		return SXERR_ABORT;` |
|        - | 1349 | `	}` |
|        - | 1350 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       51 | 1351 | `	pDoBlock->bPostContinue = TRUE;` |
|       51 | 1352 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       51 | 1353 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 1354 | `		return SXERR_ABORT;` |
|        - | 1355 | `	}` |
|       51 | 1356 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|       46 | 1357 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|        - | 1358 | `			/* Missing 'while' statement */` |
|        - | 1359 | ``			/* php: `do {} ;` names the ';', while `do {}` at EOF is "unexpected end of`` |
|        - | 1360 | `			 * file". The do-block's own slice stops before its terminator, so look at` |
|        - | 1361 | `			 * the whole CHUNK stream: a token still there is the one php names; nothing` |
|        - | 1362 | `			 * left means end of file (NULL). */` |
|        - | 1363 | `			{` |
|        3 | 1364 | `				SyToken *pBad = pGen->pIn < pGen->pEnd ? pGen->pIn : 0;` |
|        3 | 1365 | `				if( pBad == 0 && pGen->pTokenSet ){` |
|        - | 1366 | `					/* The do-block consumed its terminator, so the token php names is` |
|        - | 1367 | `					 * the one just behind the cursor -- but only when it is a real` |
|        - | 1368 | ``					 * terminator (`do {} ;` -> the ';'). If the block simply ended on`` |
|        - | 1369 | `					 * its '}' with nothing after it, php reports end of file. */` |
|        3 | 1370 | `					SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        3 | 1371 | `					if( pGen->pIn > pBase && (pGen->pIn[-1].nType & PH7_TK_SEMI) ){` |
|      ! 0 | 1372 | `						pBad = &pGen->pIn[-1];` |
|      ! 0 | 1373 | `					}` |
|        1 | 1374 | `				}` |
|        3 | 1375 | `				rc = PH7_GenSyntaxError(pGen,pBad,"\"while\"");` |
|        - | 1376 | `			}` |
|        3 | 1377 | `			if( rc == SXERR_ABORT ){` |
|        - | 1378 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 1379 | `				return SXERR_ABORT;` |
|        - | 1380 | `			}` |
|        3 | 1381 | `			goto Synchronize;` |
|        - | 1382 | `	}` |
|        - | 1383 | `	/* Jump the 'while' keyword */` |
|       49 | 1384 | `	pGen->pIn++;` |
|       49 | 1385 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1386 | `		/* php's parse error names the token it found instead */` |
|       11 | 1387 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|       11 | 1388 | `		if( rc == SXERR_ABORT ){` |
|        - | 1389 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1390 | `			return SXERR_ABORT;` |
|        - | 1391 | `		}` |
|       11 | 1392 | `		goto Synchronize;` |
|        - | 1393 | `	}` |
|        - | 1394 | `	/* Jump the left parenthesis '(' */` |
|       39 | 1395 | `	pGen->pIn++;` |
|        - | 1396 | `	/* Delimit the condition */` |
|       39 | 1397 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|       39 | 1398 | `	if( pEnd >= pGen->pEnd ){` |
|       21 | 1399 | `		rc = GenStateUnclosedHead(&(*pGen),&pGen->pIn[-1],PHL_HEAD_COND);` |
|       21 | 1400 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1401 | `			return SXERR_ABORT;` |
|        - | 1402 | `		}` |
|       21 | 1403 | `		goto Synchronize;` |
|        - | 1404 | `	}` |
|       19 | 1405 | `	if( pGen->pIn == pEnd ){` |
|        - | 1406 | `		/* Empty condition. php reports the token that actually stopped it -- for` |
|        - | 1407 | ``		 * `while ()` that is the ')' -- not a hand-written "expected expression". */`` |
|        3 | 1408 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);` |
|        3 | 1409 | `		if( rc == SXERR_ABORT ){` |
|        - | 1410 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1411 | `			return SXERR_ABORT;` |
|        - | 1412 | `		}` |
|        3 | 1413 | `		goto Synchronize;` |
|        - | 1414 | `	}` |
|        - | 1415 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|       17 | 1416 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|        - | 1417 | `		JumpFixup *aPost;` |
|        - | 1418 | `		VmInstr *pInstr;` |
|        - | 1419 | `		sxu32 nJumpDest;` |
|        - | 1420 | `		sxu32 n;` |
|        3 | 1421 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|        3 | 1422 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|        5 | 1423 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|        3 | 1424 | `			pInstr = GenStateFixupInstr(&aPost[n]);` |
|        3 | 1425 | `			if( pInstr ){` |
|        - | 1426 | `				/* Fix */` |
|        3 | 1427 | `				pInstr->iP2 = nJumpDest;` |
|        1 | 1428 | `			}` |
|        2 | 1429 | `		}` |
|        1 | 1430 | `	}` |
|        - | 1431 | `	/* Swap token streams */` |
|       17 | 1432 | `	pTmp = pGen->pEnd;` |
|       17 | 1433 | `	pGen->pEnd = pEnd;` |
|        - | 1434 | `	/* Compile the expression */` |
|       17 | 1435 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       17 | 1436 | `	if( rc == SXERR_ABORT ){` |
|        - | 1437 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1438 | `		return SXERR_ABORT;` |
|        - | 1439 | `	}` |
|        - | 1440 | `	/* Update token stream */` |
|       17 | 1441 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 | 1442 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 | 1443 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1444 | `			return SXERR_ABORT;` |
|        - | 1445 | `		}` |
|      ! 0 | 1446 | `		pGen->pIn++;` |
|      ! 0 | 1447 | `	}` |
|       17 | 1448 | `	pGen->pIn  = &pEnd[1];` |
|       17 | 1449 | `	pGen->pEnd = pTmp;` |
|        - | 1450 | `	/* Emit the true jump to the beginning of the loop */` |
|       17 | 1451 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|        - | 1452 | `	/* Fix all jumps now the destination is resolved */` |
|       17 | 1453 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 1454 | `	/* Release the loop block */` |
|       17 | 1455 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 1456 | `	/* Statement successfully compiled */` |
|       17 | 1457 | `	return SXRET_OK;` |
|       17 | 1458 | `Synchronize:` |
|        - | 1459 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - | 1460 | `	 * compiling this erroneous block.` |
|        - | 1461 | `	 */` |
|       56 | 1462 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       21 | 1463 | `		pGen->pIn++;` |
|        1 | 1464 | `	}` |
|       36 | 1465 | `	return SXRET_OK;` |
|       27 | 1466 | `}` |
|        - | 1467 | `/*` |
|        - | 1468 | ` * Compile the complex and powerful 'for' statement.` |
|        - | 1469 | ` * According to the PHP language reference` |
|        - | 1470 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|        - | 1471 | ` *  The syntax of a for loop is:` |
|        - | 1472 | ` *  for (expr1; expr2; expr3)` |
|        - | 1473 | ` *   statement` |
|        - | 1474 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|        - | 1475 | ` *  the beginning of the loop.` |
|        - | 1476 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|        - | 1477 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|        - | 1478 | ` *  to FALSE, the execution of the loop ends.` |
|        - | 1479 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|        - | 1480 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|        - | 1481 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|        - | 1482 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|        - | 1483 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|        - | 1484 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|        - | 1485 | ` *  of using the for truth expression.` |
|        - | 1486 | ` */` |
|    51431 | 1487 | `PH7_PRIVATE sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|        5 | 1488 | `{` |
|    51436 | 1489 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|    51436 | 1490 | `	GenBlock *pForBlock = 0;` |
|        - | 1491 | `	sxu32 nFalseJump;` |
|        - | 1492 | `	sxi32 rc;` |
|        - | 1493 | `	/* Jump the 'for' keyword */` |
|    51436 | 1494 | `	pGen->pIn++;` |
|    51436 | 1495 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1496 | `		/* php's parse error names the token it found instead */` |
|       11 | 1497 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|       11 | 1498 | `		if( rc == SXERR_ABORT ){` |
|        - | 1499 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1500 | `			return SXERR_ABORT;` |
|        - | 1501 | `		}` |
|       11 | 1502 | `		return SXRET_OK;` |
|        - | 1503 | `	}` |
|        - | 1504 | `	/* Jump the left parenthesis '(' */` |
|    51426 | 1505 | `	pGen->pIn++;` |
|        - | 1506 | `	/* Delimit the init-expr;condition;post-expr */` |
|    51426 | 1507 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    51426 | 1508 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - | 1509 | ``		/* `for ()` wants the `;` of its first clause; a head nothing closes is`` |
|        - | 1510 | `		 * the scanner's or the parser's to refuse. */` |
|       55 | 1511 | `		rc = pEnd >= pGen->pEnd` |
|       34 | 1512 | `			? GenStateUnclosedHead(&(*pGen),&pGen->pIn[-1],PHL_HEAD_FOR)` |
|       19 | 1513 | `			: PH7_GenSyntaxError(&(*pGen),pEnd,"\";\"");` |
|       37 | 1514 | `		if( rc == SXERR_ABORT ){` |
|        - | 1515 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1516 | `			return SXERR_ABORT;` |
|        - | 1517 | `		}` |
|        - | 1518 | `		/* Synchronize */` |
|       37 | 1519 | `		pGen->pIn = pEnd;` |
|       37 | 1520 | `		if( pGen->pIn < pGen->pEnd ){` |
|        3 | 1521 | `			pGen->pIn++;` |
|        1 | 1522 | `		}` |
|       37 | 1523 | `		return SXRET_OK;` |
|        - | 1524 | `	}` |
|        - | 1525 | `	/* Swap token streams */` |
|    51390 | 1526 | `	pTmp = pGen->pEnd;` |
|    51390 | 1527 | `	pGen->pEnd = pEnd;` |
|        - | 1528 | `	/* for() clauses are the ONLY place php's grammar allows a comma-separated` |
|        - | 1529 | `	 * expression list, so the comma operator is permitted for their duration` |
|        - | 1530 | `	 * (see GenStateTreeHasComma). A closure body nested inside a clause is` |
|        - | 1531 | `	 * compiled through this same window — recorded as a known leniency. */` |
|    51390 | 1532 | `	pGen->nCommaExprOk++;` |
|    51390 | 1533 | `	pGen->zClauseCloser = "\";\""; /* init/condition clauses close on ';' */` |
|        - | 1534 | `` 	/* Compile initialization expressions if available. Every element of a `for` `` |
|        - | 1535 | ``	 * clause is a statement position in php, `(void)` cast included. */`` |
|    51390 | 1536 | `	GenStateEnableClauseVoidCasts(&(*pGen),1);` |
|    51390 | 1537 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 1538 | `	/* Pop operand lvalues */` |
|    51390 | 1539 | `	if( rc == SXERR_ABORT ){` |
|        - | 1540 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1541 | `		return SXERR_ABORT;` |
|    51390 | 1542 | `	}else if( rc != SXERR_EMPTY ){` |
|    51360 | 1543 | `		GenStateMarkDiscardedCall(&(*pGen));` |
|    51360 | 1544 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    25643 | 1545 | `	}` |
|    51390 | 1546 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 1547 | `		/* Syntax error */` |
|      ! 0 | 1548 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|      ! 0 | 1549 | `		if( rc == SXERR_ABORT ){` |
|        - | 1550 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1551 | `			return SXERR_ABORT;` |
|        - | 1552 | `		}` |
|      ! 0 | 1553 | `		return SXRET_OK;` |
|        - | 1554 | `	}` |
|        - | 1555 | `	/* Jump the trailing ';' */` |
|    51390 | 1556 | `	pGen->pIn++;` |
|        - | 1557 | `	/* Create the loop block */` |
|    51390 | 1558 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|    51390 | 1559 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1560 | `		return SXERR_ABORT;` |
|        - | 1561 | `	}` |
|        - | 1562 | `	/* Deffer continue jumps */` |
|    51390 | 1563 | `	pForBlock->bPostContinue = TRUE;` |
|        - | 1564 | `	/* Compile the condition */` |
|    51390 | 1565 | `	if( GenStateEnableClauseVoidCasts(&(*pGen),0) ){` |
|        - | 1566 | `		/* php names the clause terminator, not the cast, when the offending` |
|        - | 1567 | ``		 * `(void)` is on the element that has to BE the condition. */`` |
|        3 | 1568 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|        - | 1569 | `			"syntax error, unexpected token \";\", expecting \",\"");` |
|        3 | 1570 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1571 | `			return SXERR_ABORT;` |
|        - | 1572 | `		}` |
|        3 | 1573 | `		return SXRET_OK;` |
|        - | 1574 | `	}` |
|    51388 | 1575 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    51388 | 1576 | `	if( rc == SXERR_ABORT ){` |
|        - | 1577 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1578 | `		return SXERR_ABORT;` |
|    51388 | 1579 | `	}else if( rc != SXERR_EMPTY ){` |
|        - | 1580 | `		/* Emit the false jump */` |
|    51366 | 1581 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|        - | 1582 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    51366 | 1583 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    25646 | 1584 | `	}` |
|    51388 | 1585 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 1586 | `		/* Syntax error */` |
|        6 | 1587 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|        6 | 1588 | `		if( rc == SXERR_ABORT ){` |
|        - | 1589 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1590 | `			return SXERR_ABORT;` |
|        - | 1591 | `		}` |
|        6 | 1592 | `		return SXRET_OK;` |
|        - | 1593 | `	}` |
|        - | 1594 | `	/* Jump the trailing ';' */` |
|    51384 | 1595 | `	pGen->pIn++;` |
|        - | 1596 | `	/* Save the post condition stream */` |
|    51384 | 1597 | `	pPostStart = pGen->pIn;` |
|        - | 1598 | `	/* Compile the loop body — OUTSIDE the comma window (the body is ordinary` |
|        - | 1599 | ``	 * php, so `(1, 2)` inside it is the parse error it should be). */`` |
|    51384 | 1600 | `	pGen->nCommaExprOk--;` |
|    51384 | 1601 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|    51384 | 1602 | `	pGen->pEnd = pTmp;` |
|    51384 | 1603 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|    51384 | 1604 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 1605 | `		return SXERR_ABORT;` |
|        - | 1606 | `	}` |
|        - | 1607 | `	/* Fix post-continue jumps */` |
|    51384 | 1608 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|        - | 1609 | `		JumpFixup *aPost;` |
|        - | 1610 | `		VmInstr *pInstr;` |
|        - | 1611 | `		sxu32 nJumpDest;` |
|        - | 1612 | `		sxu32 n;` |
|    16933 | 1613 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|    16933 | 1614 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|    33915 | 1615 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|    16987 | 1616 | `			pInstr = GenStateFixupInstr(&aPost[n]);` |
|    16987 | 1617 | `			if( pInstr ){` |
|        - | 1618 | `				/* Fix jump */` |
|    16987 | 1619 | `				pInstr->iP2 = nJumpDest;` |
|     8480 | 1620 | `			}` |
|     8485 | 1621 | `		}` |
|     8453 | 1622 | `	}` |
|        - | 1623 | ``	/* compile the post-expressions if available. A `;` here is not skipped: the`` |
|        - | 1624 | ``	 * post clause closes on ')', so `for (;;;)` is php's parse error. */`` |
|    51384 | 1625 | `	if( pPostStart < pEnd ){` |
|        - | 1626 | `		SyToken *pTmpIn,*pTmpEnd;` |
|    51370 | 1627 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|    51370 | 1628 | `		pGen->nCommaExprOk++; /* post-expressions are a clause list again */` |
|    51370 | 1629 | `		pGen->zClauseCloser = "\")\""; /* the post clause closes on ')' */` |
|    51370 | 1630 | `		GenStateEnableClauseVoidCasts(&(*pGen),1);` |
|    51370 | 1631 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    51370 | 1632 | `		pGen->nCommaExprOk--;` |
|    51370 | 1633 | `		pGen->zClauseCloser = 0;` |
|    51370 | 1634 | `		if( pGen->pIn < pGen->pEnd ){` |
|        - | 1635 | `			/* php names the token and expects ')'; the post-clause runs to the ')'. */` |
|       17 | 1636 | `			rc = PH7_GenSyntaxError(pGen,pGen->pIn,"\")\"");` |
|       17 | 1637 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1638 | `				return SXERR_ABORT;` |
|        - | 1639 | `			}` |
|       17 | 1640 | `			return SXRET_OK;` |
|        - | 1641 | `		}` |
|    51354 | 1642 | `		RE_SWAP_DELIMITER(pGen);` |
|    51354 | 1643 | `		if( rc == SXERR_ABORT ){` |
|        - | 1644 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1645 | `			return SXERR_ABORT;` |
|    51354 | 1646 | `		}else if( rc != SXERR_EMPTY){` |
|    51354 | 1647 | `			GenStateMarkDiscardedCall(&(*pGen));` |
|        - | 1648 | `			/* Pop operand lvalue */` |
|    51354 | 1649 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    25640 | 1650 | `		}` |
|    25640 | 1651 | `	}` |
|        - | 1652 | `	/* Emit the unconditional jump to the start of the loop */` |
|    51368 | 1653 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|        - | 1654 | `	/* Fix all jumps now the destination is resolved */` |
|    51368 | 1655 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 1656 | `	/* Release the loop block */` |
|    51368 | 1657 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 1658 | `	/* Statement successfully compiled */` |
|    51368 | 1659 | `	return SXRET_OK;` |
|    25686 | 1660 | `}` |
|        - | 1661 | `/*` |
|        - | 1662 | ` * Is this keyword token part of a NAME rather than a keyword in its own right?` |
|        - | 1663 | ` *` |
|        - | 1664 | `` * The lexer marks `as` a keyword wherever it appears, and php lets it BE a name`` |
|        - | 1665 | ` * in three places the token stream reaches through a preceding operator: a` |
|        - | 1666 | ` * variable ($as lexes as PH7_TK_DOLLAR followed by the identifier), a property` |
|        - | 1667 | ` * ($o->as, $o?->as) and a class constant (A::as). foreach's separator scan has` |
|        - | 1668 | ` * to look at what PRECEDES the keyword, or it splits inside the name and hands` |
|        - | 1669 | `` * the subject compiler a bare `$`.`` |
|        - | 1670 | ` */` |
|    98420 | 1671 | `static int GenStateKeywordIsName(SyToken *pStart,SyToken *pCur)` |
|        5 | 1672 | `{` |
|        - | 1673 | `	SyToken *pPrev;` |
|    98425 | 1674 | `	if( pCur <= pStart ){` |
|      ! 0 | 1675 | `		return 0;` |
|        - | 1676 | `	}` |
|    98425 | 1677 | `	pPrev = pCur - 1;` |
|    98425 | 1678 | `	if( pPrev->nType & PH7_TK_DOLLAR ){` |
|        6 | 1679 | `		return 1;` |
|        - | 1680 | `	}` |
|    98421 | 1681 | `	if( (pPrev->nType & PH7_TK_OP) == 0 ){` |
|    98413 | 1682 | `		return 0;` |
|        - | 1683 | `	}` |
|       13 | 1684 | `	return (pPrev->sData.nByte == sizeof("->")-1` |
|        6 | 1685 | `			&& SyMemcmp(pPrev->sData.zString,"->",sizeof("->")-1) == 0)` |
|        7 | 1686 | `		\|\| (pPrev->sData.nByte == sizeof("::")-1` |
|        3 | 1687 | `			&& SyMemcmp(pPrev->sData.zString,"::",sizeof("::")-1) == 0)` |
|       16 | 1688 | `		\|\| (pPrev->sData.nByte == sizeof("?->")-1` |
|        4 | 1689 | `			&& SyMemcmp(pPrev->sData.zString,"?->",sizeof("?->")-1) == 0);` |
|    49143 | 1690 | `}` |
|        - | 1691 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|        - | 1692 | ` *` |
|        - | 1693 | `` * php's `as` target is any WRITABLE expression, not just a variable: a property`` |
|        - | 1694 | ` * ($o->p), a static property (C::$s), an array element ($a['k']) and an append` |
|        - | 1695 | ` * ($a[]) are all accepted, on the key side as much as on the value side. The` |
|        - | 1696 | ` * three shapes php rejects get php's own wording; everything else keeps PH7's.` |
|        - | 1697 | ` */` |
|   116151 | 1698 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 1699 | `{` |
|   116156 | 1700 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|   116156 | 1701 | `	const char *zMsg = 0;` |
|        - | 1702 | ``	/* A loop target is a write target: `as $this` and `as (new A)->p` are php's`` |
|        - | 1703 | `	 * own compile fatals. */` |
|   116156 | 1704 | `	rc = GenStateWriteTargetCheck(&(*pGen),pRoot,0);` |
|   116156 | 1705 | `	if( rc != SXRET_OK ){` |
|        3 | 1706 | `		return rc;` |
|        - | 1707 | `	}` |
|   116154 | 1708 | `	if( pRoot->pOp ){` |
|       40 | 1709 | `		switch( pRoot->pOp->iOp ){` |
|       17 | 1710 | `		case EXPR_OP_ARROW:     /* $o->p */` |
|        - | 1711 | `		case EXPR_OP_SUBSCRIPT: /* $a['k'], $a[] */` |
|       35 | 1712 | `			return SXRET_OK;` |
|        2 | 1713 | ``		case EXPR_OP_DC:        /* C::$s — but `as C::K` is php's parse error */`` |
|        6 | 1714 | `			if( !PH7_ExprNodeIsClassConst(pRoot) ){` |
|        3 | 1715 | `				return SXRET_OK;` |
|        - | 1716 | `			}` |
|        3 | 1717 | `			break;` |
|      ! 0 | 1718 | `		case EXPR_OP_NULLSAFE_ARROW:` |
|      ! 0 | 1719 | `			zMsg = "Can't use nullsafe operator in write context";` |
|      ! 0 | 1720 | `			break;` |
|        - | 1721 | ``		/* A CALL target (`as f()`) never reaches here — GenStateWriteTargetCheck`` |
|        - | 1722 | `		 * above already refused it, and with php's function/method distinction. */` |
|      ! 0 | 1723 | `		default:` |
|      ! 0 | 1724 | `			break;` |
|        - | 1725 | `		}` |
|   116113 | 1726 | `	}else if( pRoot->xCode == PH7_CompileVariable ){` |
|   116116 | 1727 | `		return SXRET_OK;` |
|        - | 1728 | `	}` |
|        3 | 1729 | `	if( zMsg == 0 ){` |
|        - | 1730 | ``		/* Not a `variable` in php's grammar: php's own syntax error, which names`` |
|        - | 1731 | `		 * the token that follows the target. */` |
|        3 | 1732 | `		return PH7_ExprOperandNotAVariable(&(*pGen),pRoot);` |
|        - | 1733 | `	}` |
|      ! 0 | 1734 | `	rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,zMsg);` |
|      ! 0 | 1735 | `	if( rc != SXERR_ABORT ){` |
|      ! 0 | 1736 | `		rc = SXERR_INVALID;` |
|      ! 0 | 1737 | `	}` |
|      ! 0 | 1738 | `	return rc;` |
|    57995 | 1739 | `}` |
|        - | 1740 | `/*` |
|        - | 1741 | `` * Is this `as` target the plain `$name` shape?`` |
|        - | 1742 | ` *` |
|        - | 1743 | ` * Only that shape can be installed by NAME the way ph7_foreach_info records it` |
|        - | 1744 | ` * (the step writes straight into the frame's symbol table). Every other writable` |
|        - | 1745 | ` * target — including a variable-variable, whose name php re-evaluates per step —` |
|        - | 1746 | ` * goes through a synthetic temporary plus a real store (GenStateForeachStoreTarget).` |
|        - | 1747 | ` */` |
|   116151 | 1748 | `static int GenStateForeachTargetIsPlainVar(SyToken *pStart,SyToken *pEnd)` |
|        5 | 1749 | `{` |
|   174126 | 1750 | `	return (pEnd == &pStart[2])` |
|   116131 | 1751 | `		&& (pStart[0].nType & PH7_TK_DOLLAR)` |
|   174292 | 1752 | `		&& (pStart[1].nType & PH7_TK_ID);` |
|        5 | 1753 | `}` |
|        - | 1754 | `/*` |
|        - | 1755 | `` * Reserve the synthetic temporary a complex `as` target's step value lands in.`` |
|        - | 1756 | ` * The bracketed name cannot collide with a user variable — the same trick the` |
|        - | 1757 | ` * list()/[...] destructuring path uses.` |
|        - | 1758 | ` */` |
|      128 | 1759 | `static sxi32 GenStateForeachTempName(ph7_gen_state *pGen,const char *zTag,SyString *pOut)` |
|        5 | 1760 | `{` |
|        - | 1761 | `	static int iForeachTargetCnt = 0;` |
|        - | 1762 | `	char zTmp[128];` |
|        - | 1763 | `	sxu32 nLen;` |
|        - | 1764 | `	char *zDup;` |
|      133 | 1765 | `	nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_%s_%d__]",zTag,iForeachTargetCnt++);` |
|      133 | 1766 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      133 | 1767 | `	if( zDup == 0 ){` |
|      ! 0 | 1768 | `		return SXERR_ABORT;` |
|        - | 1769 | `	}` |
|      133 | 1770 | `	SyStringInitFromBuf(pOut,zDup,nLen);` |
|      133 | 1771 | `	return SXRET_OK;` |
|       69 | 1772 | `}` |
|        - | 1773 | `/*` |
|        - | 1774 | `` * Emit `<target> = <temp>` (or `<target> =& <temp>` for a by-reference value) for`` |
|        - | 1775 | `` * one complex `as` target, at the top of the loop body — where php performs the`` |
|        - | 1776 | ` * assignment, once per step.` |
|        - | 1777 | ` *` |
|        - | 1778 | ` * The store is folded exactly as the assignment operator's own codegen folds it` |
|        - | 1779 | ` * (compile.c, precedence-18 site): a member LHS keeps its OP_MEMBER, a subscript` |
|        - | 1780 | ` * becomes STORE_IDX, and a plain name folds into the STORE's p3.` |
|        - | 1781 | ` */` |
|      128 | 1782 | `static sxi32 GenStateForeachStoreTarget(` |
|        - | 1783 | `	ph7_gen_state *pGen,` |
|        - | 1784 | `	SyString *pTemp,   /* Synthetic variable holding this step's value/key */` |
|        - | 1785 | `	SyToken *pStart,   /* Target expression token range */` |
|        - | 1786 | `	SyToken *pEnd,` |
|        - | 1787 | `	int bRef           /* True for a by-reference value target */` |
|        - | 1788 | `	)` |
|        5 | 1789 | `{` |
|      133 | 1790 | `	SyToken *pSavedIn = pGen->pIn;` |
|      133 | 1791 | `	SyToken *pSavedEnd = pGen->pEnd;` |
|      133 | 1792 | `	sxi32 iVmOp = bRef ? PH7_OP_STORE_REF : PH7_OP_STORE;` |
|        - | 1793 | `	VmInstr *pInstr;` |
|      133 | 1794 | `	sxi32 iP1 = 0;` |
|      133 | 1795 | `	sxi32 iP2 = 0;` |
|      133 | 1796 | `	void *p3 = 0;` |
|        - | 1797 | `	sxi32 rc;` |
|        - | 1798 | `	/* The value being stored, below the target — the operand order OP_STORE expects. */` |
|      133 | 1799 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(pTemp),0);` |
|      133 | 1800 | `	pGen->pIn = pStart;` |
|      133 | 1801 | `	pGen->pEnd = pEnd;` |
|      133 | 1802 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE,` |
|        - | 1803 | `		GenStateForEachNodeValidator);` |
|      133 | 1804 | `	pGen->pIn = pSavedIn;` |
|      133 | 1805 | `	pGen->pEnd = pSavedEnd;` |
|      133 | 1806 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 1807 | `		return SXERR_ABORT;` |
|      133 | 1808 | `	}else if( rc != SXRET_OK ){` |
|        - | 1809 | `		/* The validator already reported it; drop the pushed value and carry on. */` |
|      ! 0 | 1810 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      ! 0 | 1811 | `		return SXRET_OK;` |
|        - | 1812 | `	}` |
|      133 | 1813 | `	pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      133 | 1814 | `	if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|        - | 1815 | `		/* A member target resolves (and, for a reference, stashes) its own slot. */` |
|       22 | 1816 | `		if( bRef ){` |
|        3 | 1817 | `			pInstr->iP2 = PH7_MEMBER_REF_TARGET;` |
|        1 | 1818 | `		}` |
|       22 | 1819 | `		iP2 = 1;` |
|      123 | 1820 | `	}else if( pInstr ){` |
|      113 | 1821 | `		(void)PH7_VmPopInstr(pGen->pVm);` |
|      113 | 1822 | `		if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       19 | 1823 | `			iVmOp = bRef ? PH7_OP_STORE_IDX_REF : PH7_OP_STORE_IDX;` |
|       19 | 1824 | `			iP1 = pInstr->iP1;` |
|       19 | 1825 | `			if( bRef ){` |
|        3 | 1826 | `				iP2 = pInstr->iP2;` |
|        3 | 1827 | `				p3 = pInstr->p3;` |
|        1 | 1828 | `			}` |
|       10 | 1829 | `		}else{` |
|       95 | 1830 | `			p3 = pInstr->p3;` |
|        - | 1831 | `		}` |
|       54 | 1832 | `	}` |
|      133 | 1833 | `	PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|        - | 1834 | `	/* Discard the stored value the store leaves behind */` |
|      133 | 1835 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      133 | 1836 | `	if( bRef ){` |
|        - | 1837 | `		/* The target now holds the element; drop the temporary's own hold, or it` |
|        - | 1838 | `		 * would keep the element a REFERENCE for the rest of the script — an extra` |
|        - | 1839 | ``		 * holder no `unset()` the program can write is able to reach. Dropping the`` |
|        - | 1840 | `		 * NAME never releases the slot the target still refers to. */` |
|        5 | 1841 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UNSET_VAR,0,0,(void *)pTemp,0);` |
|        2 | 1842 | `	}` |
|      133 | 1843 | `	return SXRET_OK;` |
|       69 | 1844 | `}` |
|        - | 1845 | `/*` |
|        - | 1846 | ` * Compile the 'foreach' statement.` |
|        - | 1847 | ` * According to the PHP language reference` |
|        - | 1848 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|        - | 1849 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|        - | 1850 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|        - | 1851 | ` *  is a minor but useful extension of the first:` |
|        - | 1852 | ` *  foreach (array_expression as $value)` |
|        - | 1853 | ` *    statement` |
|        - | 1854 | ` *  foreach (array_expression as $key => $value)` |
|        - | 1855 | ` *   statement` |
|        - | 1856 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|        - | 1857 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|        - | 1858 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|        - | 1859 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|        - | 1860 | ` *  to the variable $key on each loop.` |
|        - | 1861 | ` *  Note:` |
|        - | 1862 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|        - | 1863 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|        - | 1864 | ` *  Note:` |
|        - | 1865 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|        - | 1866 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|        - | 1867 | ` *  or after the foreach without resetting it.` |
|        - | 1868 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|        - | 1869 | ` *  of copying the value.` |
|        - | 1870 | ` */` |
|    98456 | 1871 | `PH7_PRIVATE sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|        5 | 1872 | `{` |
|    98461 | 1873 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    98461 | 1874 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|        - | 1875 | ``	/* Token ranges of a KEY / VALUE target that is not a plain `$name`: it is`` |
|        - | 1876 | `	 * compiled as a real store at the top of the loop body (php assigns the value` |
|        - | 1877 | `	 * first, then the key), against a synthetic temporary the step writes. */` |
|    98461 | 1878 | `	SyToken *pKeyStart = 0,*pKeyEnd = 0;` |
|    98461 | 1879 | `	SyToken *pValStart = 0,*pValEnd = 0;` |
|    98461 | 1880 | `	GenBlock *pForeachBlock = 0;` |
|        - | 1881 | `	ph7_foreach_info *pInfo;` |
|        - | 1882 | `	sxu32 nFalseJump;` |
|    98461 | 1883 | `	sxu32 nContainerEnd = 0; /* instruction count just after the iterated expression */` |
|        - | 1884 | `	VmInstr *pInstr;` |
|        - | 1885 | `	sxu32 nLine;` |
|        - | 1886 | `	sxi32 rc;` |
|    98461 | 1887 | `	nLine = pGen->pIn->nLine;` |
|        - | 1888 | `	/* Jump the 'foreach' keyword */` |
|    98461 | 1889 | `	pGen->pIn++;` |
|    98461 | 1890 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1891 | `		/* php's parse error names the token it found instead */` |
|       11 | 1892 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|       11 | 1893 | `		if( rc == SXERR_ABORT ){` |
|        - | 1894 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1895 | `			return SXERR_ABORT;` |
|        - | 1896 | `		}` |
|       11 | 1897 | `		goto Synchronize;` |
|        - | 1898 | `	}` |
|        - | 1899 | `	/* Jump the left parenthesis '(' */` |
|    98451 | 1900 | `	pGen->pIn++;` |
|        - | 1901 | `	/* Create the loop block */` |
|    98451 | 1902 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    98451 | 1903 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1904 | `		return SXERR_ABORT;` |
|        - | 1905 | `	}` |
|        - | 1906 | `	/* Delimit the expression */` |
|    98451 | 1907 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    98451 | 1908 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - | 1909 | ``		/* `foreach ()` names the `)`; a head nothing closes is the scanner's or`` |
|        - | 1910 | `		 * the parser's to refuse. */` |
|       58 | 1911 | `		rc = pEnd >= pGen->pEnd` |
|       36 | 1912 | `			? GenStateUnclosedHead(&(*pGen),&pGen->pIn[-1],PHL_HEAD_FOREACH)` |
|       20 | 1913 | `			: PH7_GenSyntaxError(&(*pGen),pEnd,0);` |
|       39 | 1914 | `		if( rc == SXERR_ABORT ){` |
|        - | 1915 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1916 | `			return SXERR_ABORT;` |
|        - | 1917 | `		}` |
|        - | 1918 | `		/* Synchronize */` |
|       39 | 1919 | `		pGen->pIn = pEnd;` |
|       39 | 1920 | `		if( pGen->pIn < pGen->pEnd ){` |
|        3 | 1921 | `			pGen->pIn++;` |
|        1 | 1922 | `		}` |
|       39 | 1923 | `		return SXRET_OK;` |
|        - | 1924 | `	}` |
|        - | 1925 | `	/* Compile the array expression.` |
|        - | 1926 | `	 *` |
|        - | 1927 | ``	 * The separator is the first TOP-LEVEL `as`: one nested inside brackets, parens`` |
|        - | 1928 | `	 * or braces belongs to something else the iterated expression contains — a` |
|        - | 1929 | ``	 * closure with a `foreach` of its own is the shape that finds this, and cutting`` |
|        - | 1930 | ``	 * at its inner `as` left the outer expression with an unclosed bracket and made`` |
|        - | 1931 | ``	 * `foreach ([function(){ foreach ([1] as $k) … }] as $f)` a compile fatal on`` |
|        - | 1932 | ``	 * source php runs. An `as` that is part of a NAME ($as, $o->as, A::as) is not a`` |
|        - | 1933 | `	 * separator either. */` |
|    98413 | 1934 | `	pCur = pGen->pIn;` |
|        - | 1935 | `	{` |
|    98413 | 1936 | `		sxi32 iNest = 0;` |
|   831089 | 1937 | `		while( pCur < pEnd ){` |
|   831089 | 1938 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|    83733 | 1939 | `				iNest++;` |
|   789164 | 1940 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        - | 1941 | `				/* A mismatch here is the expression parser's to report. */` |
|    83733 | 1942 | `				iNest--;` |
|   705436 | 1943 | `			}else if( iNest <= 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|   115695 | 1944 | `				sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|   115695 | 1945 | `				if( nKeywrd == PH7_TKWRD_AS && !GenStateKeywordIsName(pGen->pIn,pCur) ){` |
|    98413 | 1946 | `					break;` |
|        - | 1947 | `				}` |
|     8630 | 1948 | `			}` |
|        - | 1949 | `			/* Advance the stream cursor */` |
|   732681 | 1950 | `			pCur++;` |
|        5 | 1951 | `		}` |
|        - | 1952 | `	}` |
|    98413 | 1953 | `	if( pCur <= pGen->pIn ){` |
|      ! 0 | 1954 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 1955 | `			"foreach: Missing array/object expression");` |
|      ! 0 | 1956 | `		if( rc == SXERR_ABORT ){` |
|        - | 1957 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1958 | `			return SXERR_ABORT;` |
|        - | 1959 | `		}` |
|      ! 0 | 1960 | `		goto Synchronize;` |
|        - | 1961 | `	}` |
|        - | 1962 | `	/* Swap token streams */` |
|    98413 | 1963 | `	pTmp = pGen->pEnd;` |
|    98413 | 1964 | `	pGen->pEnd = pCur;` |
|    98413 | 1965 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    98413 | 1966 | `	if( rc == SXERR_ABORT ){` |
|        - | 1967 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1968 | `		return SXERR_ABORT;` |
|        - | 1969 | `	}` |
|        - | 1970 | `	/* Remember where the iterated expression ends: whether it was fetched for` |
|        - | 1971 | ``	 * WRITING is only known once the `&` after `as` has been read, and a`` |
|        - | 1972 | `	 * by-reference walk fetches its container the way a reference bind does` |
|        - | 1973 | `	 * (php's BP_VAR_W) -- so a missing property is created, not warned about. */` |
|    98413 | 1974 | `	nContainerEnd = PH7_VmInstrLength(pGen->pVm);` |
|        - | 1975 | `	/* Update token stream */` |
|    98413 | 1976 | `	while(pGen->pIn < pCur ){` |
|      ! 0 | 1977 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 1978 | `		if( rc == SXERR_ABORT ){` |
|        - | 1979 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1980 | `			return SXERR_ABORT;` |
|        - | 1981 | `		}` |
|      ! 0 | 1982 | `		pGen->pIn++;` |
|      ! 0 | 1983 | `	}` |
|    98413 | 1984 | `	pCur++; /* Jump the 'as' keyword */` |
|    98413 | 1985 | `	pGen->pIn = pCur;` |
|    98413 | 1986 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 1987 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|      ! 0 | 1988 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1989 | `			return SXERR_ABORT;` |
|        - | 1990 | `		}` |
|      ! 0 | 1991 | `	}` |
|        - | 1992 | `	/* Create the foreach context */` |
|    98413 | 1993 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    98413 | 1994 | `	if( pInfo == 0 ){` |
|      ! 0 | 1995 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|      ! 0 | 1996 | `		return SXERR_ABORT;` |
|        - | 1997 | `	}` |
|        - | 1998 | `	/* Zero the structure */` |
|    98413 | 1999 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|        - | 2000 | `	/* Initialize structure fields */` |
|    98413 | 2001 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|        - | 2002 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|        - | 2003 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|        - | 2004 | `	 * '=>'. */` |
|    98413 | 2005 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|    98413 | 2006 | `	if( pCur < pEnd ){` |
|        - | 2007 | `		/* Compile the expression holding the key name */` |
|    18019 | 2008 | `		if( pGen->pIn >= pCur ){` |
|      ! 0 | 2009 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|      ! 0 | 2010 | `			if( rc == SXERR_ABORT ){` |
|        - | 2011 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 2012 | `				return SXERR_ABORT;` |
|      ! 0 | 2013 | `			}` |
|    18019 | 2014 | `		}else if( !GenStateForeachTargetIsPlainVar(pGen->pIn,pCur) ){` |
|        - | 2015 | `			/* A writable but non-name key target ($o->k, C::$s, $a['k'], $$n): the` |
|        - | 2016 | `			 * step lands in a temporary and the store runs in the loop body. */` |
|       13 | 2017 | `			pKeyStart = pGen->pIn;` |
|       13 | 2018 | `			pKeyEnd = pCur;` |
|       13 | 2019 | `			if( GenStateForeachTempName(&(*pGen),"key",&pInfo->sKey) != SXRET_OK ){` |
|      ! 0 | 2020 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 2021 | `				return SXERR_ABORT;` |
|        - | 2022 | `			}` |
|       13 | 2023 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|        7 | 2024 | `		}else{` |
|    18007 | 2025 | `			pGen->pEnd = pCur;` |
|    18007 | 2026 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    18007 | 2027 | `			if( rc == SXERR_ABORT ){` |
|        - | 2028 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 2029 | `				return SXERR_ABORT;` |
|        - | 2030 | `			}` |
|    18007 | 2031 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    18007 | 2032 | `			if( pInstr->p3 ){` |
|        - | 2033 | `				/* Record key name */` |
|    18007 | 2034 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|     8987 | 2035 | `			}` |
|    18007 | 2036 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|        - | 2037 | `		}` |
|    18019 | 2038 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|     8993 | 2039 | `	}` |
|    98413 | 2040 | `	pGen->pEnd = pEnd;` |
|    98413 | 2041 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 2042 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|      ! 0 | 2043 | `		if( rc == SXERR_ABORT ){` |
|        - | 2044 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 2045 | `			return SXERR_ABORT;` |
|        - | 2046 | `		}` |
|      ! 0 | 2047 | `		goto Synchronize;` |
|        - | 2048 | `	}` |
|    98413 | 2049 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|       88 | 2050 | `		pGen->pIn++;` |
|        - | 2051 | `		/* Pass by reference  */` |
|       88 | 2052 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       42 | 2053 | `	}` |
|        - | 2054 | `	/* Check if the value target is list() */` |
|    98413 | 2055 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        8 | 2056 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|        - | 2057 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|        - | 2058 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|        - | 2059 | `		 */` |
|        - | 2060 | `		static int iForeachListCnt = 0;` |
|        - | 2061 | `		char zTmp[128];` |
|        - | 2062 | `		sxu32 nLen;` |
|        - | 2063 | `		char *zDup;` |
|       10 | 2064 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|       10 | 2065 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       10 | 2066 | `		if( zDup == 0 ){` |
|      ! 0 | 2067 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 2068 | `			return SXERR_ABORT;` |
|        - | 2069 | `		}` |
|       10 | 2070 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|        - | 2071 | `		/* Save list() token boundaries */` |
|       10 | 2072 | `		pListStart = pGen->pIn;` |
|        - | 2073 | `		/* Advance past list(...) — validate parentheses */` |
|       10 | 2074 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|       10 | 2075 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 2076 | `			/* php: syntax error, unexpected variable "$x", expecting "(" */` |
|        3 | 2077 | `			rc = PH7_GenSyntaxError(pGen,pGen->pIn < pEnd ? pGen->pIn : 0,"\"(\"");` |
|        3 | 2078 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2079 | `				return SXERR_ABORT;` |
|        - | 2080 | `			}` |
|        3 | 2081 | `			goto Synchronize;` |
|        - | 2082 | `		}` |
|        7 | 2083 | `		pGen->pIn++; /* Jump '(' */` |
|        7 | 2084 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|        7 | 2085 | `		if( pListEnd >= pEnd ){` |
|      ! 0 | 2086 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2087 | `				"foreach: Missing closing ')' after list");` |
|      ! 0 | 2088 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2089 | `				return SXERR_ABORT;` |
|        - | 2090 | `			}` |
|      ! 0 | 2091 | `			goto Synchronize;` |
|        - | 2092 | `		}` |
|        7 | 2093 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|        7 | 2094 | `		pListEnd = pGen->pIn;` |
|        7 | 2095 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    98408 | 2096 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|        - | 2097 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|        - | 2098 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|        - | 2099 | `		 */` |
|        - | 2100 | `		static int iForeachShortListCnt = 0;` |
|        - | 2101 | `		char zTmp[128];` |
|        - | 2102 | `		sxu32 nLen;` |
|        - | 2103 | `		char *zDup;` |
|      268 | 2104 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|      268 | 2105 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      268 | 2106 | `		if( zDup == 0 ){` |
|      ! 0 | 2107 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 2108 | `			return SXERR_ABORT;` |
|        - | 2109 | `		}` |
|      268 | 2110 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|        - | 2111 | `		/* Save [...] token boundaries */` |
|      268 | 2112 | `		pListStart = pGen->pIn;` |
|        - | 2113 | `		/* Advance past [...] */` |
|      268 | 2114 | `		pGen->pIn++; /* Jump '[' */` |
|      268 | 2115 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|      268 | 2116 | `		if( pListEnd >= pEnd ){` |
|      ! 0 | 2117 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2118 | `				"foreach: Missing closing ']' after short list");` |
|      ! 0 | 2119 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2120 | `				return SXERR_ABORT;` |
|        - | 2121 | `			}` |
|      ! 0 | 2122 | `			goto Synchronize;` |
|        - | 2123 | `		}` |
|      268 | 2124 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|      268 | 2125 | `		pListEnd = pGen->pIn;` |
|      268 | 2126 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    98273 | 2127 | `	}else if( !GenStateForeachTargetIsPlainVar(pGen->pIn,pEnd) ){` |
|        - | 2128 | `		/* A writable but non-name value target — same treatment as the key above. */` |
|      121 | 2129 | `		pValStart = pGen->pIn;` |
|      121 | 2130 | `		pValEnd = pEnd;` |
|      121 | 2131 | `		if( GenStateForeachTempName(&(*pGen),"val",&pInfo->sValue) != SXRET_OK ){` |
|      ! 0 | 2132 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 2133 | `			return SXERR_ABORT;` |
|        - | 2134 | `		}` |
|       63 | 2135 | `	}else{` |
|        - | 2136 | `		/* Compile the expression holding the value name */` |
|    98026 | 2137 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    98026 | 2138 | `		if( rc == SXERR_ABORT ){` |
|        - | 2139 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 2140 | `			return SXERR_ABORT;` |
|        - | 2141 | `		}` |
|    98026 | 2142 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    98026 | 2143 | `		if( pInstr->p3 ){` |
|        - | 2144 | `			/* Record value name */` |
|    98026 | 2145 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    48939 | 2146 | `		}` |
|        - | 2147 | `	}` |
|    98406 | 2148 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd` |
|      274 | 2149 | `	 && PH7_GenStateListSpanHasRef(pListStart,pListEnd) ){` |
|        - | 2150 | `		/* The list binds by reference, so this step's value has to BE the array` |
|        - | 2151 | `		 * element rather than a copy of it. */` |
|        3 | 2152 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|        1 | 2153 | `	}` |
|    98411 | 2154 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_REF) && nContainerEnd > 0 ){` |
|       90 | 2155 | `		VmInstr *pContainer = PH7_VmGetInstr(pGen->pVm,nContainerEnd - 1);` |
|       86 | 2156 | `		if( pContainer && pContainer->iOp == PH7_OP_MEMBER` |
|       57 | 2157 | `		 && pContainer->iP2 == PH7_MEMBER_READ ){` |
|       22 | 2158 | `			pContainer->bRefSrc = 1;` |
|       10 | 2159 | `		}` |
|       43 | 2160 | `	}` |
|        - | 2161 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    98411 | 2162 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|        - | 2163 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    98411 | 2164 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|        - | 2165 | `	/* Record the first instruction to execute */` |
|    98411 | 2166 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|        - | 2167 | `	/* Emit the FOREACH_STEP instruction */` |
|    98411 | 2168 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|        - | 2169 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    98411 | 2170 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|        - | 2171 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    98411 | 2172 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|        - | 2173 | `		SyToken *pSavedIn,*pSavedEnd;` |
|        - | 2174 | `		/* Load the temporary variable holding the current value onto the stack.` |
|        - | 2175 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|        - | 2176 | `		 */` |
|      274 | 2177 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|        - | 2178 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|        - | 2179 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|        - | 2180 | `		 * picks up the delimiter and the variable names inside.` |
|        - | 2181 | `		 */` |
|      274 | 2182 | `		pSavedIn = pGen->pIn;` |
|      274 | 2183 | `		pSavedEnd = pGen->pEnd;` |
|      274 | 2184 | `		pGen->pIn = pListStart;` |
|      274 | 2185 | `		pGen->pEnd = pListEnd;` |
|      274 | 2186 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|      268 | 2187 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|      136 | 2188 | `		}else{` |
|        7 | 2189 | `			rc = PH7_CompileList(&(*pGen),0);` |
|        - | 2190 | `		}` |
|      274 | 2191 | `		pGen->pIn = pSavedIn;` |
|      274 | 2192 | `		pGen->pEnd = pSavedEnd;` |
|      274 | 2193 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2194 | `			return SXERR_ABORT;` |
|        - | 2195 | `		}` |
|        - | 2196 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|      274 | 2197 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      274 | 2198 | `		if( pInfo->iFlags & PH7_4EACH_STEP_REF ){` |
|        - | 2199 | `			/* The bind is done; drop the STEP's own hold on the element, exactly as` |
|        - | 2200 | `			 * the non-list by-ref target does. Left in place it would keep the row a` |
|        - | 2201 | `			 * REFERENCE for the rest of the script -- php marks the ELEMENT the` |
|        - | 2202 | `			 * target still aliases, never the row. */` |
|        3 | 2203 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_UNSET_VAR,0,0,(void *)&pInfo->sValue,0);` |
|        1 | 2204 | `		}` |
|      134 | 2205 | `	}` |
|        - | 2206 | `	/* Store this step's value and key into their non-name targets. php performs the` |
|        - | 2207 | `	 * VALUE assignment first — visible through a __set() pair, and the order the` |
|        - | 2208 | `	 * symbol table records the two locals in for the plain-name shape. */` |
|    98411 | 2209 | `	if( pValStart ){` |
|      179 | 2210 | `		rc = GenStateForeachStoreTarget(&(*pGen),&pInfo->sValue,pValStart,pValEnd,` |
|      116 | 2211 | `			(pInfo->iFlags & PH7_4EACH_STEP_REF) != 0);` |
|      121 | 2212 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2213 | `			return SXERR_ABORT;` |
|        - | 2214 | `		}` |
|       58 | 2215 | `	}` |
|    98411 | 2216 | `	if( pKeyStart ){` |
|       13 | 2217 | `		rc = GenStateForeachStoreTarget(&(*pGen),&pInfo->sKey,pKeyStart,pKeyEnd,0);` |
|       13 | 2218 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2219 | `			return SXERR_ABORT;` |
|        - | 2220 | `		}` |
|        6 | 2221 | `	}` |
|        - | 2222 | `	/* Compile the loop body */` |
|    98411 | 2223 | `	pGen->pIn = &pEnd[1];` |
|    98411 | 2224 | `	pGen->pEnd = pTmp;` |
|    98411 | 2225 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    98411 | 2226 | `	if( rc == SXERR_ABORT ){` |
|        - | 2227 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 2228 | `		return SXERR_ABORT;` |
|        - | 2229 | `	}` |
|        - | 2230 | `	/* Emit the unconditional jump to the start of the loop */` |
|    98411 | 2231 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|        - | 2232 | `	/* Fix all jumps now the destination is resolved */` |
|    98411 | 2233 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 2234 | `	/* Release the loop block */` |
|    98411 | 2235 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 2236 | `	/* Statement successfully compiled */` |
|    98411 | 2237 | `	return SXRET_OK;` |
|        6 | 2238 | `Synchronize:` |
|        - | 2239 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - | 2240 | `	 * compiling this erroneous block.` |
|        - | 2241 | `	 */` |
|       26 | 2242 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       13 | 2243 | `		pGen->pIn++;` |
|        1 | 2244 | `	}` |
|       14 | 2245 | `	return SXRET_OK;` |
|    49161 | 2246 | `}` |
|        - | 2247 | `/*` |
|        - | 2248 | ` * Compile the infamous if/elseif/else if/else statements.` |
|        - | 2249 | ` * According to the PHP language reference` |
|        - | 2250 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|        - | 2251 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|        - | 2252 | ` *  that is similar to that of C:` |
|        - | 2253 | ` *  if (expr)` |
|        - | 2254 | ` *   statement` |
|        - | 2255 | ` *  else construct:` |
|        - | 2256 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|        - | 2257 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|        - | 2258 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|        - | 2259 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|        - | 2260 | ` *   $b, and a is NOT greater than b otherwise.` |
|        - | 2261 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|        - | 2262 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|        - | 2263 | ` *  elseif` |
|        - | 2264 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|        - | 2265 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|        - | 2266 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|        - | 2267 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|        - | 2268 | ` *   than b, a equal to b or a is smaller than b:` |
|        - | 2269 | ` *   <?php` |
|        - | 2270 | ` *    if ($a > $b) {` |
|        - | 2271 | ` *     echo "a is bigger than b";` |
|        - | 2272 | ` *    } elseif ($a == $b) {` |
|        - | 2273 | ` *     echo "a is equal to b";` |
|        - | 2274 | ` *    } else {` |
|        - | 2275 | ` *     echo "a is smaller than b";` |
|        - | 2276 | ` *    }` |
|        - | 2277 | ` *    ?>` |
|        - | 2278 | ` */` |
|   579125 | 2279 | `PH7_PRIVATE sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|        5 | 2280 | `{` |
|   579130 | 2281 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|   579130 | 2282 | `	GenBlock *pCondBlock = 0;` |
|        - | 2283 | `	sxu32 nJumpIdx;` |
|        - | 2284 | `	sxu32 nKeyID;` |
|        - | 2285 | `	sxi32 rc;` |
|        - | 2286 | `	/* Jump the 'if' keyword */` |
|   579130 | 2287 | `	pGen->pIn++;` |
|   579130 | 2288 | `	pToken = pGen->pIn;` |
|        - | 2289 | `	/* Create the conditional block */` |
|   579130 | 2290 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|   579130 | 2291 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 2292 | `		return SXERR_ABORT;` |
|        - | 2293 | `	}` |
|        - | 2294 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   323111 | 2295 | `	for(;;){` |
|   647068 | 2296 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 2297 | `			/* php's parse error names the token it found instead */` |
|       31 | 2298 | `			rc = PH7_GenSyntaxError(&(*pGen),pToken < pGen->pEnd ? pToken : 0,"\"(\"");` |
|       31 | 2299 | `			if( rc == SXERR_ABORT ){` |
|        - | 2300 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 2301 | `				return SXERR_ABORT;` |
|        - | 2302 | `			}` |
|       31 | 2303 | `			goto Synchronize;` |
|        - | 2304 | `		}` |
|        - | 2305 | `		/* Jump the left parenthesis '(' */` |
|   647038 | 2306 | `		pToken++;` |
|        - | 2307 | `		/* Delimit the condition */` |
|   647038 | 2308 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   647038 | 2309 | `		if( pToken >= pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - | 2310 | ``			/* `if ()` names the `)`; a head nothing closes is the scanner's or the`` |
|        - | 2311 | `			 * parser's to refuse. Ask where the delimiter stopped, never what is` |
|        - | 2312 | `			 * there: past the end sits whatever a longer chunk left in the buffer,` |
|        - | 2313 | ``			 * and a stale `)` read as this head's closer. */`` |
|      154 | 2314 | `			rc = pEnd >= pGen->pEnd` |
|       96 | 2315 | `				? GenStateUnclosedHead(&(*pGen),&pToken[-1],PHL_HEAD_COND)` |
|       54 | 2316 | `				: PH7_GenSyntaxError(&(*pGen),pEnd,0);` |
|      103 | 2317 | `			if( rc == SXERR_ABORT ){` |
|        - | 2318 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 2319 | `				return SXERR_ABORT;` |
|        - | 2320 | `			}` |
|      103 | 2321 | `			goto Synchronize;` |
|        - | 2322 | `		}` |
|        - | 2323 | `		/* Swap token streams */` |
|   646936 | 2324 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|        - | 2325 | `		/* Compile the condition */` |
|   646936 | 2326 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 2327 | `		/* Update token stream */` |
|   646936 | 2328 | `		while(pGen->pIn < pEnd ){` |
|      ! 0 | 2329 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 | 2330 | `			pGen->pIn++;` |
|      ! 0 | 2331 | `		}` |
|   646936 | 2332 | `		pGen->pIn  = &pEnd[1];` |
|   646936 | 2333 | `		pGen->pEnd = pTmp;` |
|   646936 | 2334 | `		if( rc == SXERR_ABORT ){` |
|        - | 2335 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|        3 | 2336 | `			return SXERR_ABORT;` |
|        - | 2337 | `		}` |
|        - | 2338 | `		/* Emit the false jump */` |
|   646934 | 2339 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|        - | 2340 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   646934 | 2341 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|        - | 2342 | `		/* Compile the body */` |
|   646934 | 2343 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   646934 | 2344 | `		if( rc == SXERR_ABORT ){` |
|        5 | 2345 | `			return SXERR_ABORT;` |
|        - | 2346 | `		}` |
|   646930 | 2347 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   140638 | 2348 | `			break;` |
|        - | 2349 | `		}` |
|        - | 2350 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   365297 | 2351 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   365297 | 2352 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   237686 | 2353 | `			break;` |
|        - | 2354 | `		}` |
|        - | 2355 | `		/* Emit the unconditional jump */` |
|   127616 | 2356 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|        - | 2357 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   127616 | 2358 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   127616 | 2359 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|    85045 | 2360 | `			pToken = &pGen->pIn[1];` |
|    85045 | 2361 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    25409 | 2362 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    29803 | 2363 | `					break;` |
|        - | 2364 | `			}` |
|    25372 | 2365 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    12667 | 2366 | `		}` |
|    67943 | 2367 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|        - | 2368 | `		/* Synchronize cursors */` |
|    67943 | 2369 | `		pToken = pGen->pIn;` |
|        - | 2370 | `		/* Fix the false jump */` |
|    67943 | 2371 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|        5 | 2372 | `	} /* For(;;) */` |
|        - | 2373 | `	/* Fix the false jump */` |
|   578992 | 2374 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|   578992 | 2375 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   297354 | 2376 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|        - | 2377 | `			/* Compile the else block */` |
|    59678 | 2378 | `			pGen->pIn++;` |
|    59678 | 2379 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    59678 | 2380 | `			if( rc == SXERR_ABORT ){` |
|        - | 2381 |  |
|      ! 0 | 2382 | `				return SXERR_ABORT;` |
|        - | 2383 | `			}` |
|    29798 | 2384 | `	}` |
|   578992 | 2385 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|        - | 2386 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|   578992 | 2387 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|        - | 2388 | `	/* Release the conditional block */` |
|   578992 | 2389 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 2390 | `	/* Statement successfully compiled */` |
|   578992 | 2391 | `	return SXRET_OK;` |
|       66 | 2392 | `Synchronize:` |
|        - | 2393 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|        - | 2394 | `	 */` |
|      317 | 2395 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      185 | 2396 | `		pGen->pIn++;` |
|        1 | 2397 | `	}` |
|      133 | 2398 | `	return SXRET_OK;` |
|   289191 | 2399 | `}` |
|        - | 2400 | `/*` |
|        - | 2401 | ` * Compile the global construct.` |
|        - | 2402 | ` * According to the PHP language reference` |
|        - | 2403 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|        - | 2404 | ` *  to be used in that function.` |
|        - | 2405 | ` *  Example #1 Using global` |
|        - | 2406 | ` *  <?php` |
|        - | 2407 | ` *   $a = 1;` |
|        - | 2408 | ` *   $b = 2;` |
|        - | 2409 | ` *   function Sum()` |
|        - | 2410 | ` *   {` |
|        - | 2411 | ` *    global $a, $b;` |
|        - | 2412 | ` *    $b = $a + $b;` |
|        - | 2413 | ` *   }` |
|        - | 2414 | ` *   Sum();` |
|        - | 2415 | ` *   echo $b;` |
|        - | 2416 | ` *  ?>` |
|        - | 2417 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|        - | 2418 | ` *  all references to either variable will refer to the global version. There is no limit` |
|        - | 2419 | ` *  to the number of global variables that can be manipulated by a function.` |
|        - | 2420 | ` */` |
|      151 | 2421 | `PH7_PRIVATE sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|        5 | 2422 | `{` |
|      156 | 2423 | `	SyToken *pTmp,*pNext = 0;` |
|        - | 2424 | `	sxi32 nExpr;` |
|        - | 2425 | `	sxi32 rc;` |
|        - | 2426 | `	/* Jump the 'global' keyword */` |
|      156 | 2427 | `	pGen->pIn++;` |
|      156 | 2428 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        - | 2429 | `		/* Nothing to process */` |
|      ! 0 | 2430 | `		return SXRET_OK;` |
|        - | 2431 | `	}` |
|      156 | 2432 | `	pTmp = pGen->pEnd;` |
|      156 | 2433 | `	nExpr = 0;` |
|      347 | 2434 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      196 | 2435 | `		if( pGen->pIn < pNext ){` |
|      196 | 2436 | `			pGen->pEnd = pNext;` |
|      196 | 2437 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 | 2438 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|      ! 0 | 2439 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2440 | `					return SXERR_ABORT;` |
|        - | 2441 | `				}` |
|      191 | 2442 | `			}else if( &pGen->pIn[1] < pGen->pEnd` |
|      191 | 2443 | `			 && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|      186 | 2444 | `			 && pGen->pIn[1].sData.nByte == sizeof("this")-1` |
|      102 | 2445 | `			 && SyMemcmp((const void *)pGen->pIn[1].sData.zString,` |
|        7 | 2446 | `			             (const void *)"this",sizeof("this")-1) == 0 ){` |
|        - | 2447 | ``				/* php refuses `global $this;` at the declaration. */`` |
|        3 | 2448 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 2449 | `					"Cannot use $this as global variable");` |
|        3 | 2450 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2451 | `					return SXERR_ABORT;` |
|        - | 2452 | `				}` |
|        2 | 2453 | `			}else{` |
|      194 | 2454 | `				pGen->pIn++;` |
|      194 | 2455 | `				if( pGen->pIn >= pGen->pEnd ){` |
|        - | 2456 | `					/* Emit a warning */` |
|      ! 0 | 2457 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|      ! 0 | 2458 | `				}else{` |
|      194 | 2459 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      194 | 2460 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 2461 | `						return SXERR_ABORT;` |
|      194 | 2462 | `					}else if(rc != SXERR_EMPTY ){` |
|      194 | 2463 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      194 | 2464 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|        - | 2465 | `							/* Variable name, not a constant */` |
|      184 | 2466 | `							pLast->iP1 = 0;` |
|       89 | 2467 | `						}` |
|      194 | 2468 | `						nExpr++;` |
|       94 | 2469 | `					}` |
|        - | 2470 | `				}` |
|        - | 2471 | `			}` |
|       95 | 2472 | `		}` |
|        - | 2473 | `		/* Next expression in the stream */` |
|      196 | 2474 | `		pGen->pIn = pNext;` |
|        - | 2475 | `		/* Jump trailing commas */` |
|      236 | 2476 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       45 | 2477 | `			pGen->pIn++;` |
|        5 | 2478 | `		}` |
|        5 | 2479 | `	}` |
|        - | 2480 | `	/* Restore token stream */` |
|      156 | 2481 | `	pGen->pEnd = pTmp;` |
|      156 | 2482 | `	if( nExpr > 0 ){` |
|        - | 2483 | `		/* Emit the uplink instruction */` |
|      154 | 2484 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|       74 | 2485 | `	}` |
|      156 | 2486 | `	return SXRET_OK;` |
|       80 | 2487 | `}` |
|        - | 2488 | `/*` |
|        - | 2489 | ` * php's NOUN for a compile-time return diagnostic is the LEXICAL scope, not the` |
|        - | 2490 | `` * kind of function the `return` sits in: zend reads CG(active_class_entry), so a`` |
|        - | 2491 | ` * closure written inside a class body reports "method" and the very same closure` |
|        - | 2492 | ` * written at file scope reports "function". pCurClass is the compiler's exact` |
|        - | 2493 | ` * counterpart (the class/interface/trait/enum whose BODY is being compiled).` |
|        - | 2494 | ` */` |
|       26 | 2495 | `static const char * GenStateReturnNoun(ph7_gen_state *pGen)` |
|        4 | 2496 | `{` |
|       30 | 2497 | `	return pGen->pCurClass ? "method" : "function";` |
|        4 | 2498 | `}` |
|        - | 2499 | `/*` |
|        - | 2500 | ` * TRUE when a declared return type ACCEPTS null — the condition under which php` |
|        - | 2501 | `` * appends its `did you mean "return null;"` hint to the missing-value error.`` |
|        - | 2502 | ``  * That is every nullable declaration (`?T`, `T\|null`, and the standalone `null` `` |
|        - | 2503 | `` * type, all of which set VM_FUNC_RETURN_NULLABLE) plus `mixed`, which includes`` |
|        - | 2504 | ` * null but is stored as a pseudo-CLASS atom rather than through the flag.` |
|        - | 2505 | ` */` |
|       10 | 2506 | `static int GenStateReturnTypeAllowsNull(ph7_vm_func *pFunc)` |
|        4 | 2507 | `{` |
|        - | 2508 | `	SyString *pCls;` |
|       14 | 2509 | `	if( pFunc->iFlags & VM_FUNC_RETURN_NULLABLE ){` |
|        3 | 2510 | `		return 1;` |
|        - | 2511 | `	}` |
|       11 | 2512 | `	pCls = &pFunc->sReturnClass;` |
|        8 | 2513 | `	if( pFunc->nReturnType == SXU32_HIGH && pCls->nByte == sizeof("mixed")-1` |
|        3 | 2514 | `	 && SyStrnicmp(pCls->zString,"mixed",sizeof("mixed")-1) == 0 ){` |
|      ! 0 | 2515 | `		return 1;` |
|        - | 2516 | `	}` |
|       11 | 2517 | `	return 0;` |
|        9 | 2518 | `}` |
|        - | 2519 | `/*` |
|        - | 2520 | ` * Compile the return statement.` |
|        - | 2521 | ` * According to the PHP language reference` |
|        - | 2522 | ` *  If called from within a function, the return() statement immediately ends execution` |
|        - | 2523 | ` *  of the current function, and returns its argument as the value of the function call.` |
|        - | 2524 | ` *  return() will also end the execution of an eval() statement or script file.` |
|        - | 2525 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|        - | 2526 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|        - | 2527 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|        - | 2528 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|        - | 2529 | ` *  from within the main script file, then script execution end.` |
|        - | 2530 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|        - | 2531 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|        - | 2532 | ` *  should do so as PHP has less work to do in this case.` |
|        - | 2533 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|        - | 2534 | ` */` |
|   360027 | 2535 | `PH7_PRIVATE sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|        5 | 2536 | `{` |
|   360032 | 2537 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|        - | 2538 | `	sxi32 rc;` |
|   360032 | 2539 | `	sxu32 nLine = pGen->pIn->nLine;` |
|   360032 | 2540 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|        - | 2541 | `	ph7_vm_func *pFunc;` |
|        - | 2542 | `	sxu32 nInstrBefore;` |
|        - | 2543 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|        - | 2544 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|        - | 2545 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|        - | 2546 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|        - | 2547 | `	 * normally below so token processing stays consistent. */` |
|  1196573 | 2548 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|   836546 | 2549 | `		pFuncBlock = pFuncBlock->pParent;` |
|        5 | 2550 | `	}` |
|   360032 | 2551 | `	pFunc = pFuncBlock ? (ph7_vm_func *)pFuncBlock->pUserData : 0;` |
|   360032 | 2552 | `	if( pFunc && pFunc->nReturnType == MEMOBJ_NEVER ){` |
|        8 | 2553 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        2 | 2554 | `			"A never-returning %s must not return", GenStateReturnNoun(pGen));` |
|        6 | 2555 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2556 | `			return SXERR_ABORT;` |
|        - | 2557 | `		}` |
|        2 | 2558 | `	}` |
|        - | 2559 | `	/* Jump the 'return' keyword */` |
|   360032 | 2560 | `	pGen->pIn++;` |
|   360032 | 2561 | `	nInstrBefore = PH7_VmInstrLength(pGen->pVm);` |
|   360032 | 2562 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_VOID_CAST) ){` |
|        - | 2563 | ``		/* php's `(void)` cast is a STATEMENT prefix, so `return (void) f();` is a`` |
|        - | 2564 | ``		 * parse error there — and the expected-token set is the one `return` has,`` |
|        - | 2565 | ``		 * which is why php names `";"` here and nothing after `$x = (void)`. */`` |
|        3 | 2566 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|        - | 2567 | `			"syntax error, unexpected token \"(void)\", expecting \";\"");` |
|        3 | 2568 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2569 | `			return SXERR_ABORT;` |
|        - | 2570 | `		}` |
|        3 | 2571 | `		return SXRET_OK;` |
|        - | 2572 | `	}` |
|   360030 | 2573 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 2574 | ``		/* php: a stray token after `return EXPR` is `... expecting ";"`. */`` |
|   359834 | 2575 | `		const char *zSave = pGen->zClauseCloser;` |
|   359834 | 2576 | `		pGen->zClauseCloser = "\";\"";` |
|        - | 2577 | ``		/* A `return` READS its operand (the value is consumed), so compile it`` |
|        - | 2578 | ``		 * read-only: a lone undefined variable `return $z` must warn at the read`` |
|        - | 2579 | `		 * exactly like echo/interpolation, not be loaded quietly (the same quiet` |
|        - | 2580 | ``		 * load that correctly keeps a bare `$z;` statement silent). Matches the`` |
|        - | 2581 | `		 * arrow-fn implicit-return body fix. */` |
|   359834 | 2582 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|   359834 | 2583 | `		pGen->zClauseCloser = zSave;` |
|   359834 | 2584 | `		if( rc == SXERR_ABORT ){` |
|        3 | 2585 | `			return SXERR_ABORT;` |
|   359832 | 2586 | `		}else if(rc != SXERR_EMPTY ){` |
|   359826 | 2587 | `			nRet = 1;` |
|   179675 | 2588 | `		}` |
|   179678 | 2589 | `	}` |
|        - | 2590 | ``	/* A bare `return;` inside a function that DECLARES a return type is a php`` |
|        - | 2591 | `	 * COMPILE error, not the runtime TypeError PHL used to raise on the way out:` |
|        - | 2592 | `` 	 * php rejects the program before it runs. `void` (which is what `return;` `` |
|        - | 2593 | ``	 * means) and `never` (handled above) are the two declarations exempt from it,`` |
|        - | 2594 | ``	 * and a GENERATOR is exempt whatever it declares — there `return;` ends the`` |
|        - | 2595 | `	 * generator, and the declared type describes the Generator object the call` |
|        - | 2596 | `	 * produced, never the returned value. */` |
|   360023 | 2597 | `	if( nRet == 0 && pFunc && !pGen->bInGenerator && VmFuncHasReturnType(pFunc)` |
|       86 | 2598 | `	 && pFunc->nReturnType != MEMOBJ_VOID && pFunc->nReturnType != MEMOBJ_NEVER ){` |
|       19 | 2599 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 2600 | `			"A %s with return type must return a value%s",` |
|        5 | 2601 | `			GenStateReturnNoun(pGen),` |
|       10 | 2602 | `			GenStateReturnTypeAllowsNull(pFunc)` |
|        - | 2603 | `				? " (did you mean \"return null;\" instead of \"return;\"?)" : "");` |
|       14 | 2604 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2605 | `			return SXERR_ABORT;` |
|        - | 2606 | `		}` |
|        5 | 2607 | `	}` |
|        - | 2608 | ``	/* The mirror rule: a `void` function must not return a VALUE, and php stops`` |
|        - | 2609 | `	 * the program at the return statement rather than throwing on the way out.` |
|        - | 2610 | `	 * Generators keep their own diagnostic (a void generator is rejected as` |
|        - | 2611 | ``	 * `Generator return type must be a supertype of Generator`), so they are`` |
|        - | 2612 | `	 * skipped here exactly as above. php's hint fires when the operand is a` |
|        - | 2613 | ``	 * compile-time constant null; PHL folds the `null` KEYWORD (constant index 0,`` |
|        - | 2614 | `	 * emitted as a lone OP_LOADC and unchanged by any wrapping parens), which is` |
|        - | 2615 | `	 * every shape real code writes. */` |
|   360023 | 2616 | `	if( nRet != 0 && pFunc && !pGen->bInGenerator` |
|   350134 | 2617 | `	 && pFunc->nReturnType == MEMOBJ_VOID ){` |
|       16 | 2618 | `		VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|       22 | 2619 | `		int bNullLiteral = (PH7_VmInstrLength(pGen->pVm) == nInstrBefore + 1)` |
|       12 | 2620 | `			&& pLast && pLast->iOp == PH7_OP_LOADC` |
|       18 | 2621 | `			&& pLast->iP1 == 0 && pLast->iP2 == 0;` |
|       22 | 2622 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 2623 | `			"A void %s must not return a value%s",` |
|        6 | 2624 | `			GenStateReturnNoun(pGen),` |
|        6 | 2625 | `			bNullLiteral` |
|        - | 2626 | `				? " (did you mean \"return;\" instead of \"return null;\"?)" : "");` |
|       16 | 2627 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2628 | `			return SXERR_ABORT;` |
|        - | 2629 | `		}` |
|        6 | 2630 | `	}` |
|        - | 2631 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|        - | 2632 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|        - | 2633 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|        - | 2634 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|   360028 | 2635 | `	if( pGen->bInGenerator ){` |
|       52 | 2636 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|       52 | 2637 | `		return SXRET_OK;` |
|        - | 2638 | `	}` |
|        - | 2639 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|        - | 2640 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|        - | 2641 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|        - | 2642 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|        - | 2643 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|   359980 | 2644 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|   359980 | 2645 | `	return SXRET_OK;` |
|   179783 | 2646 | `}` |
|        - | 2647 | `/*` |
|        - | 2648 | ` * Compile a yield expression.` |
|        - | 2649 | ` * Called from the expression code generator when a yield node is encountered.` |
|        - | 2650 | ` * Handles: yield, yield $value, yield $key => $value` |
|        - | 2651 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|        - | 2652 | ` */` |
|      734 | 2653 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        5 | 2654 | `{` |
|        - | 2655 | `	SyToken *pTmp, *pSplit;` |
|      739 | 2656 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|      739 | 2657 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|        - | 2658 | `	sxi32 rc;` |
|      367 | 2659 | `	(void)iCompileFlag;` |
|        - | 2660 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|      739 | 2661 | `	pGen->pIn++;` |
|        - | 2662 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|        - | 2663 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|        - | 2664 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|        - | 2665 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|        - | 2666 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|      734 | 2667 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|      423 | 2668 | `		&& pGen->pIn->sData.nByte == 4` |
|      106 | 2669 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|       89 | 2670 | `		pGen->pIn++; /* Skip 'from' */` |
|       89 | 2671 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       89 | 2672 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2673 | `			return SXERR_ABORT;` |
|        - | 2674 | `		}` |
|       89 | 2675 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 | 2676 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|      ! 0 | 2677 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|        - | 2678 | `				"Missing expression after 'yield from'");` |
|      ! 0 | 2679 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2680 | `				return SXERR_ABORT;` |
|        - | 2681 | `			}` |
|      ! 0 | 2682 | `		}` |
|       89 | 2683 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|       89 | 2684 | `		return SXRET_OK;` |
|        - | 2685 | `	}` |
|      655 | 2686 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - | 2687 | `		/* Bare yield — no value */` |
|        8 | 2688 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|        8 | 2689 | `		return SXRET_OK;` |
|        - | 2690 | `	}` |
|        - | 2691 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax. The shared` |
|        - | 2692 | `	 * array-entry scanner is the one that already knows which '=>' are NOT` |
|        - | 2693 | ``	 * separators — an arrow function's body and a match arm's — so `yield fn($x)`` |
|        - | 2694 | `` 	 * => $x` and `yield match($k){ 1 => 2 }` stop being read as `key => value` `` |
|        - | 2695 | `	 * pairs (the first was a parse error, the second yielded the wrong pair). */` |
|      649 | 2696 | `	pSplit = GenStateFindTopLevelArrow(pGen->pIn,pGen->pEnd);` |
|      649 | 2697 | `	if( pSplit >= pGen->pEnd ){` |
|      601 | 2698 | `		pSplit = 0;` |
|      298 | 2699 | `	}` |
|      649 | 2700 | `	pTmp = pGen->pEnd;` |
|      649 | 2701 | `	if( pSplit ){` |
|        - | 2702 | `		/* yield $key => $value */` |
|       53 | 2703 | `		pGen->pEnd = pSplit;` |
|       53 | 2704 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       53 | 2705 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       53 | 2706 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|       53 | 2707 | `		pGen->pEnd = pTmp;` |
|       53 | 2708 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       53 | 2709 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       53 | 2710 | `		iP1 = 1;` |
|       53 | 2711 | `		iP2 = 1;` |
|       29 | 2712 | `	}else{` |
|        - | 2713 | `		/* yield $value */` |
|      601 | 2714 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      601 | 2715 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      601 | 2716 | `		if( rc != SXERR_EMPTY ){` |
|      601 | 2717 | `			iP1 = 1;` |
|      298 | 2718 | `		}` |
|        - | 2719 | `	}` |
|      649 | 2720 | `	pGen->pEnd = pTmp;` |
|      649 | 2721 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|      649 | 2722 | `	return SXRET_OK;` |
|      372 | 2723 | `}` |
|        - | 2724 | `/*` |
|        - | 2725 | ` * Compile the die/exit language construct.` |
|        - | 2726 | ` * The role of these constructs is to terminate execution of the script.` |
|        - | 2727 | ` * Shutdown functions will always be executed even if exit() is called.` |
|        - | 2728 | ` */` |
|      392 | 2729 | `PH7_PRIVATE sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|        5 | 2730 | `{` |
|      397 | 2731 | `	sxi32 nExpr = 0;` |
|        - | 2732 | `	sxi32 rc;` |
|        - | 2733 | `	/* Jump the die/exit keyword */` |
|      397 | 2734 | `	pGen->pIn++;` |
|      397 | 2735 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 2736 | `		/* Compile the expression */` |
|      397 | 2737 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      397 | 2738 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2739 | `			return SXERR_ABORT;` |
|      397 | 2740 | `		}else if(rc != SXERR_EMPTY ){` |
|      397 | 2741 | `			nExpr = 1;` |
|      196 | 2742 | `		}` |
|      196 | 2743 | `	}` |
|        - | 2744 | `	/* Emit the HALT instruction */` |
|      397 | 2745 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|      397 | 2746 | `	return SXRET_OK;` |
|      201 | 2747 | `}` |
|        - | 2748 | `/*` |
|        - | 2749 | ` * Compile the 'echo' language construct.` |
|        - | 2750 | ` */` |
|    35355 | 2751 | `PH7_PRIVATE sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|        5 | 2752 | `{` |
|    35360 | 2753 | `	SyToken *pTmp,*pNext = 0;` |
|    35360 | 2754 | `	sxu32 nLine = pGen->pIn->nLine;` |
|    35360 | 2755 | `	int nExpr = 0;      /* expressions actually compiled */` |
|    35360 | 2756 | `	int bExpectMore = 1;/* after 'echo' or a comma an expression is REQUIRED */` |
|        - | 2757 | `	sxi32 rc;` |
|        - | 2758 | `	/* Jump the 'echo' keyword */` |
|    35360 | 2759 | `	pGen->pIn++;` |
|        - | 2760 | `	/* Compile arguments one after one. php: a stray token in an echo list is` |
|        - | 2761 | ``	 * `... expecting "," or ";"` — set the closer for each argument's compile. */`` |
|    35360 | 2762 | `	pTmp = pGen->pEnd;` |
|        - | 2763 | `	{` |
|    35360 | 2764 | `	const char *zSaveEcho = pGen->zClauseCloser;` |
|   103124 | 2765 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|    67775 | 2766 | `		if( pGen->pIn < pNext ){` |
|    67775 | 2767 | `			pGen->pEnd = pNext;` |
|    67775 | 2768 | `			pGen->zClauseCloser = "\",\" or \";\"";` |
|    67775 | 2769 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|    67775 | 2770 | `			pGen->zClauseCloser = zSaveEcho;` |
|    67775 | 2771 | `			if( rc == SXERR_ABORT ){` |
|        6 | 2772 | `				return SXERR_ABORT;` |
|    67771 | 2773 | `			}else if( rc != SXERR_EMPTY ){` |
|        - | 2774 | `				/* Emit the consume instruction */` |
|    67679 | 2775 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    67679 | 2776 | `				nExpr++;` |
|    67679 | 2777 | `				bExpectMore = 0;` |
|    33791 | 2778 | `			}` |
|    33837 | 2779 | `		}` |
|        - | 2780 | `		/* Jump trailing commas (php: exactly one between expressions; a` |
|        - | 2781 | `		 * dangling or doubled comma is a parse error, enforced below) */` |
|   100220 | 2782 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    32456 | 2783 | `			if( bExpectMore ){` |
|        - | 2784 | `				/* two commas in a row */` |
|        3 | 2785 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,pNext->nLine,` |
|        - | 2786 | `					"syntax error, unexpected token \",\"");` |
|        3 | 2787 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 2788 | `			}` |
|    32454 | 2789 | `			bExpectMore = 1;` |
|    32454 | 2790 | `			pNext++;` |
|        5 | 2791 | `		}` |
|    67769 | 2792 | `		pGen->pIn = pNext;` |
|        5 | 2793 | `	}` |
|        - | 2794 | `	}` |
|        - | 2795 | `	/* Restore token stream */` |
|    35354 | 2796 | `	pGen->pEnd = pTmp;` |
|    35354 | 2797 | `	if( nExpr == 0 \|\| bExpectMore ){` |
|        - | 2798 | ``		/* `echo ;` or `echo expr, ;` — php rejects both, and names the end of the`` |
|        - | 2799 | ``		 * file when the list ran into it rather than into a `;` or a `?>`. */`` |
|       84 | 2800 | `		rc = (pGen->pIn >= pGen->pEnd && pGen->bChunkAtEof)` |
|       16 | 2801 | `			? PH7_GenSyntaxError(&(*pGen),0,0)` |
|      125 | 2802 | `			: PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - | 2803 | `				"syntax error, unexpected token \";\"");` |
|      129 | 2804 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 2805 | `	}` |
|    35230 | 2806 | `	return SXRET_OK;` |
|    17644 | 2807 | `}` |
|        - | 2808 | `/*` |
|        - | 2809 | ` * Compile the static statement.` |
|        - | 2810 | ` * According to the PHP language reference` |
|        - | 2811 | ` *  Another important feature of variable scoping is the static variable.` |
|        - | 2812 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|        - | 2813 | ` *  when program execution leaves this scope.` |
|        - | 2814 | ` *  Static variables also provide one way to deal with recursive functions.` |
|        - | 2815 | ` * Symisc eXtension.` |
|        - | 2816 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|        - | 2817 | ` *  the zend engine would allow only simple scalar value.` |
|        - | 2818 | ` *  Example` |
|        - | 2819 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|        - | 2820 | ` *    Refer to the official documentation for more information on this feature.` |
|        - | 2821 | ` */` |
|       82 | 2822 | `PH7_PRIVATE sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|        4 | 2823 | `{` |
|        - | 2824 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|        - | 2825 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|        - | 2826 | `	GenBlock *pBlock;` |
|        - | 2827 | `	SyString *pName;` |
|        - | 2828 | `	char *zDup;` |
|        - | 2829 | `	sxu32 nLine;` |
|        - | 2830 | `	sxi32 rc;` |
|        - | 2831 | ``	/* `static function () {}` / `static fn () =>` at statement position is an`` |
|        - | 2832 | `	 * EXPRESSION statement (a bare static closure), not a static-variable` |
|        - | 2833 | `	 * declaration — hand it to the expression compiler (php accepts it). */` |
|       82 | 2834 | `	if( &pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_KEYWORD)` |
|       47 | 2835 | `	 && (SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FUNCTION` |
|        2 | 2836 | `	  \|\| SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FN) ){` |
|        5 | 2837 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        5 | 2838 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2839 | `			return SXERR_ABORT;` |
|        5 | 2840 | `		}else if( rc != SXERR_EMPTY ){` |
|        5 | 2841 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        2 | 2842 | `		}` |
|        5 | 2843 | `		return SXRET_OK;` |
|        - | 2844 | `	}` |
|        - | 2845 | `	/* Jump the static keyword */` |
|       82 | 2846 | `	nLine = pGen->pIn->nLine;` |
|       82 | 2847 | `	pGen->pIn++;` |
|        - | 2848 | `	/* Extract the enclosing function if any */` |
|       82 | 2849 | `	pBlock = pGen->pCurrent;` |
|      160 | 2850 | `	while( pBlock ){` |
|      156 | 2851 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       78 | 2852 | `			break;` |
|        - | 2853 | `		}` |
|        - | 2854 | `		/* Point to the upper block */` |
|       82 | 2855 | `		pBlock = pBlock->pParent;` |
|        4 | 2856 | `	}` |
|       82 | 2857 | `	if( pBlock == 0 ){` |
|        - | 2858 | `		/* Static statement,called outside of a function body,treat it as a simple variable.` |
|        - | 2859 | `		 * php's list form applies here too, so each declarator is compiled on its own` |
|        - | 2860 | `		 * and the comma is stepped over rather than reaching the expression parser` |
|        - | 2861 | `		 * (which has no comma operator). */` |
|        3 | 2862 | `		for(;;){` |
|        7 | 2863 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|        - | 2864 | ``				/* php: `static FOO;` is a syntax error naming FOO and expecting "::"`` |
|        - | 2865 | ``				 * (the parser is still open to `static::` at that point). */`` |
|      ! 0 | 2866 | `				rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"::\"");` |
|      ! 0 | 2867 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2868 | `					return SXERR_ABORT;` |
|        - | 2869 | `				}` |
|      ! 0 | 2870 | `				goto Synchronize;` |
|        - | 2871 | `			}` |
|        - | 2872 | `			/* Compile the expression holding the variable */` |
|        7 | 2873 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|        7 | 2874 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2875 | `				return SXERR_ABORT;` |
|        7 | 2876 | `			}else if( rc != SXERR_EMPTY ){` |
|        - | 2877 | `				/* Emit the POP instruction */` |
|        7 | 2878 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        3 | 2879 | `			}` |
|        7 | 2880 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|        3 | 2881 | `				break;` |
|        - | 2882 | `			}` |
|        3 | 2883 | `			pGen->pIn++; /* Jump the comma and take the next declarator */` |
|        1 | 2884 | `		}` |
|        5 | 2885 | `		return SXRET_OK;` |
|        - | 2886 | `	}` |
|       78 | 2887 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        - | 2888 | ``	/* php declares a LIST here -- `static $a, $b = 2, $c;` -- each element its own`` |
|        - | 2889 | `	 * slot with its own optional initializer. PHL took the first declarator and` |
|        - | 2890 | ``	 * then refused the comma (`static: Unexpected token ','`), which is a fatal on`` |
|        - | 2891 | `	 * an everyday spelling: two counters in one statement is how the construct is` |
|        - | 2892 | `	 * usually written. */` |
|       40 | 2893 | `Declarator:` |
|        - | 2894 | `	/* Make sure we are dealing with a valid statement */` |
|       84 | 2895 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|       78 | 2896 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - | 2897 | ``			/* php: `static FOO;` is a syntax error naming FOO and expecting "::"`` |
|        - | 2898 | ``			 * (the parser is still open to `static::` at that point). */`` |
|        3 | 2899 | `			rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"::\"");` |
|        3 | 2900 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2901 | `				return SXERR_ABORT;` |
|        - | 2902 | `			}` |
|        3 | 2903 | `			goto Synchronize;` |
|        - | 2904 | `	}` |
|       82 | 2905 | `	pGen->pIn++;` |
|        - | 2906 | `	/* Extract variable name */` |
|       82 | 2907 | `	pName = &pGen->pIn->sData;` |
|        - | 2908 | ``	/* php refuses `static $this;` at the declaration — the name is not a slot a`` |
|        - | 2909 | `	 * function may own. */` |
|       78 | 2910 | `	if( pName->nByte == sizeof("this")-1` |
|       46 | 2911 | `	 && SyMemcmp((const void *)pName->zString,(const void *)"this",sizeof("this")-1) == 0 ){` |
|        3 | 2912 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 2913 | `			"Cannot use $this as static variable");` |
|        3 | 2914 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2915 | `			return SXERR_ABORT;` |
|        - | 2916 | `		}` |
|        3 | 2917 | `		goto Synchronize;` |
|        - | 2918 | `	}` |
|       80 | 2919 | `	pGen->pIn++; /* Jump the var name */` |
|       76 | 2920 | `	if( pGen->pIn < pGen->pEnd` |
|       80 | 2921 | `	 && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|      ! 0 | 2922 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 2923 | `		goto Synchronize;` |
|        - | 2924 | `	}` |
|        - | 2925 | `	/* Initialize the structure describing the static variable */` |
|       80 | 2926 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       80 | 2927 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|        - | 2928 | `	/* Duplicate variable name */` |
|       80 | 2929 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       80 | 2930 | `	if( zDup == 0 ){` |
|      ! 0 | 2931 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 2932 | `		return SXERR_ABORT;` |
|        - | 2933 | `	}` |
|       80 | 2934 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|        - | 2935 | `	/* Check if we have an expression to compile */` |
|       80 | 2936 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|        - | 2937 | `		SySet *pInstrContainer;` |
|        - | 2938 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|        - | 2939 | `		 * Static variable can take any complex expression including function` |
|        - | 2940 | `		 * call as their initialization value.` |
|        - | 2941 | `		 * Example:` |
|        - | 2942 | `		 *		static $var = foo(1,4+5,bar());` |
|        - | 2943 | `		 */` |
|       63 | 2944 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|        - | 2945 | `		/* Swap bytecode container */` |
|       63 | 2946 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       63 | 2947 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|        - | 2948 | `		/* Compile the expression. EXPR_FLAG_COMMA_STATEMENT stops it at the first` |
|        - | 2949 | `		 * top-level comma so the declarator after it is left for the loop. */` |
|       63 | 2950 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|        - | 2951 | `		/* Emit the done instruction */` |
|       63 | 2952 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        - | 2953 | `		/* Restore default bytecode container */` |
|       63 | 2954 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       30 | 2955 | `	}` |
|        - | 2956 | `	/* Finally save the compiled static variable in the appropriate container */` |
|       80 | 2957 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|       80 | 2958 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|        7 | 2959 | `		pGen->pIn++; /* Jump the comma and take the next declarator */` |
|        7 | 2960 | `		goto Declarator;` |
|        - | 2961 | `	}` |
|       74 | 2962 | `	return SXRET_OK;` |
|        2 | 2963 | `Synchronize:` |
|        - | 2964 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|        - | 2965 | `	 * statement.` |
|        - | 2966 | `	 */` |
|       10 | 2967 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|        6 | 2968 | `		pGen->pIn++;` |
|        2 | 2969 | `	}` |
|        6 | 2970 | `	return SXRET_OK;` |
|       45 | 2971 | `}` |
|        - | 2972 | `/*` |
|        - | 2973 | ` * Compile the var statement.` |
|        - | 2974 | ` * Symisc Extension:` |
|        - | 2975 | ` *      var statement can be used outside of a class definition.` |
|        - | 2976 | ` */` |
|        8 | 2977 | `PH7_PRIVATE sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|        2 | 2978 | `{` |
|        - | 2979 | `` 	/* `var` is a PROPERTY modifier and nothing else. A class body's `var $x;` `` |
|        - | 2980 | `	 * is compiled by compile_class.c, never here, so reaching this statement` |
|        - | 2981 | ``	 * handler means `var` appeared outside a class — which php rejects as a`` |
|        - | 2982 | `	 * parse error. PHL used to compile it as an ordinary expression statement,` |
|        - | 2983 | ``	 * so top-level `var $x = 1;` silently ran (a Symisc extension). */`` |
|       10 | 2984 | `	PH7_GenSyntaxError(&(*pGen),pGen->pIn,PH7_GenStrayStatementTail(&(*pGen)));` |
|       10 | 2985 | `	return SXERR_ABORT;` |
|        2 | 2986 | `}` |
|        - | 2987 | `/*` |
|        - | 2988 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|        - | 2989 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|        - | 2990 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|        - | 2991 | ` */` |
|        - | 2992 | `/*` |
|        - | 2993 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|        - | 2994 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|        - | 2995 | ` * hash and any shared references), this creates a new literal entry with the` |
|        - | 2996 | ` * qualified name and updates the instruction's operand index.` |
|        - | 2997 | ` *` |
|        - | 2998 | ` * Resolution order:` |
|        - | 2999 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|        - | 3000 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|        - | 3001 | ` *   3. Otherwise return the original literal index unchanged.` |
|        - | 3002 | ` *` |
|        - | 3003 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|        - | 3004 | ` * came from an import (step 1) and 0 otherwise.` |
|        - | 3005 | ` * Returns the (possibly new) literal index.` |
|        - | 3006 | ` */` |
|  1312992 | 3007 | `PH7_PRIVATE sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|        5 | 3008 | `{` |
|        - | 3009 | `	ph7_value *pLit;` |
|        - | 3010 | `	const char *zLit;` |
|        - | 3011 | `	SyString sQualified;` |
|        - | 3012 | `	sxu32 nLit;` |
|        - | 3013 | `	sxu32 k;` |
|        - | 3014 | `	sxu32 nNewIdx;` |
|        - | 3015 | `	int hasNsSep;` |
|        - | 3016 | `	SyHashEntry *pImport;` |
|        - | 3017 | `	ph7_value *pNew;` |
|  1312997 | 3018 | `	if( pFromImport ){` |
|  1167166 | 3019 | `		*pFromImport = 0;` |
|   582445 | 3020 | `	}` |
|  1312997 | 3021 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  1312997 | 3022 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|      ! 0 | 3023 | `		return nOrigIdx;` |
|        - | 3024 | `	}` |
|  1312997 | 3025 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  1312997 | 3026 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|        - | 3027 | `	/* Skip if already qualified (contains backslash) */` |
|  1312997 | 3028 | `	hasNsSep = 0;` |
| 11651089 | 3029 | `	for( k = 0; k < nLit; k++ ){` |
| 10339100 | 3030 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
|  5159249 | 3031 | `	}` |
|  1312997 | 3032 | `	if( hasNsSep ){` |
|     1008 | 3033 | `		return nOrigIdx;` |
|        - | 3034 | `	}` |
|        - | 3035 | `	/* Check use imports first (works even outside namespaces) */` |
|  1311994 | 3036 | `	SyBlobReset(&pGen->sWorker);` |
|  1311994 | 3037 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  1311994 | 3038 | `	if( pImport ){` |
|      279 | 3039 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      279 | 3040 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      279 | 3041 | `		if( pFromImport ){` |
|       42 | 3042 | `			*pFromImport = 1;` |
|       19 | 3043 | `		}` |
|      142 | 3044 | `	}else{` |
|  1311720 | 3045 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  1310672 | 3046 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|        - | 3047 | `		}` |
|        - | 3048 | `		/* Prepend current namespace */` |
|     1053 | 3049 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     1053 | 3050 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|     1053 | 3051 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|        - | 3052 | `	}` |
|        - | 3053 | `	/* Look up or create a new literal for the qualified name */` |
|     1327 | 3054 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     1327 | 3055 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      861 | 3056 | `		return nNewIdx; /* Already interned */` |
|        - | 3057 | `	}` |
|      471 | 3058 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      471 | 3059 | `	if( pNew == 0 ){` |
|      ! 0 | 3060 | `		return nOrigIdx; /* OOM, fall back to original */` |
|        - | 3061 | `	}` |
|      471 | 3062 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      471 | 3063 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      471 | 3064 | `	return nNewIdx;` |
|   655269 | 3065 | `}` |
|        - | 3066 | `/*` |
|        - | 3067 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|        - | 3068 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|        - | 3069 | ` */` |
|    10616 | 3070 | `PH7_PRIVATE void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 | 3071 | `{` |
|        - | 3072 | `	SyHashEntry *pImport;` |
|    10621 | 3073 | `	const char *zName = pName->zString;` |
|    10621 | 3074 | `	sxu32 nName = pName->nByte;` |
|    10621 | 3075 | `	sxu32 nFirst = 0;` |
|        - | 3076 | `	/* php resolves a name through use-imports on its LEADING segment: an` |
|        - | 3077 | ``	 * unqualified `C` maps the whole name (`use X\C;` -> X\C), while a QUALIFIED`` |
|        - | 3078 | ``	 * `A\B\C` maps just `A` (`use X\A;` -> X\A\B\C) and keeps the `\B\C` tail.`` |
|        - | 3079 | `	 * The old code looked up the whole qualified string (which never matches a` |
|        - | 3080 | `	 * single-segment import alias) and then blindly prefixed the current` |
|        - | 3081 | ``	 * namespace, so `use PHPUnit\Framework; extends Framework\TestCase` resolved`` |
|        - | 3082 | `	 * to Ns\Framework\TestCase. Mirror GenStateResolveNamespaceLiteral here. */` |
|    98213 | 3083 | `	while( nFirst < nName && zName[nFirst] != '\\' ){ nFirst++; }` |
|    10621 | 3084 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)zName,nFirst);` |
|    10621 | 3085 | `	if( pImport ){` |
|      137 | 3086 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      137 | 3087 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      137 | 3088 | `		SyBlobAppend(pOut,zName + nFirst,nName - nFirst); /* the \B\C tail, if any */` |
|      137 | 3089 | `		return;` |
|        - | 3090 | `	}` |
|        - | 3091 | `	/* Prepend current namespace if active */` |
|    10489 | 3092 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      100 | 3093 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      100 | 3094 | `		SyBlobAppend(pOut,"\\",1);` |
|       48 | 3095 | `	}` |
|    10489 | 3096 | `	SyBlobAppend(pOut,zName,nName);` |
|     5311 | 3097 | `}` |
|        - | 3098 | `/*` |
|        - | 3099 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|        - | 3100 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|        - | 3101 | ` * The caller must release pOut when done.` |
|        - | 3102 | ` */` |
|    14682 | 3103 | `PH7_PRIVATE void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 | 3104 | `{` |
|    14687 | 3105 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      983 | 3106 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      983 | 3107 | `		SyBlobAppend(pOut,"\\",1);` |
|      489 | 3108 | `	}` |
|    14687 | 3109 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    14687 | 3110 | `}` |
|        - | 3111 | `/*` |
|        - | 3112 | ` * TRUE when pB starts exactly where pA ends. The tokenizer drops whitespace, so` |
|        - | 3113 | ` * source offsets are the only record of it — and php's qualified-name token` |
|        - | 3114 | `` * (`{LABEL}("\\"{LABEL})+`) is matched by its LEXER, which means the `\` has to`` |
|        - | 3115 | `` * be glued: `private\Q` is one name, `private \Q` is a modifier and a type, and`` |
|        - | 3116 | ` * only this test tells the two apart.` |
|        - | 3117 | ` */` |
|      528 | 3118 | `PH7_PRIVATE int GenStateTokensGlued(SyToken *pA,SyToken *pB)` |
|        5 | 3119 | `{` |
|      533 | 3120 | `	return pA->sData.zString + pA->sData.nByte == pB->sData.zString;` |
|        5 | 3121 | `}` |
|        - | 3122 | `/*` |
|        - | 3123 | `` * php's `namespace\X` NAME OPERATOR (5.3): a leading `namespace` keyword glued to`` |
|        - | 3124 | `` * a `\` names the CURRENT namespace, and the whole name is then FULLY QUALIFIED —`` |
|        - | 3125 | `` * `namespace\X` inside `namespace B;` is `\B\X`, and plain `\X` at global scope.`` |
|        - | 3126 | `` * php's lexer matches it as one token (T_NAME_RELATIVE, `"namespace"("\\"{LABEL})+`,`` |
|        - | 3127 | `` * case-insensitively), so the `\` must be GLUED to the keyword: `namespace \X` is a`` |
|        - | 3128 | ` * php parse error, and this mirrors that by comparing source offsets.` |
|        - | 3129 | ` *` |
|        - | 3130 | ` * This predicate only RECOGNIZES the operator (it consumes nothing), which is what` |
|        - | 3131 | `` * the statement dispatcher needs to tell `namespace\X::m();` from a namespace`` |
|        - | 3132 | ` * DECLARATION; GenStateNsRelPrefix below is what the name collectors call.` |
|        - | 3133 | ` */` |
|  3055213 | 3134 | `PH7_PRIVATE int GenStateIsNsRelName(SyToken *pIn,SyToken *pEnd)` |
|        5 | 3135 | `{` |
|  3055213 | 3136 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
|  2481125 | 3137 | `	 \|\| (sxu32)SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_NAMESPACE ){` |
|  3054138 | 3138 | `		return 0;` |
|        - | 3139 | `	}` |
|     1085 | 3140 | `	if( &pIn[1] >= pEnd \|\| (pIn[1].nType & PH7_TK_NSSEP) == 0 ){` |
|      965 | 3141 | `		return 0;` |
|        - | 3142 | `	}` |
|      124 | 3143 | `	if( &pIn[2] >= pEnd \|\| (pIn[2].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 3144 | ``		return 0; /* php's T_NAME_RELATIVE needs at least one segment after the `\` */`` |
|        - | 3145 | `	}` |
|      124 | 3146 | `	return GenStateTokensGlued(pIn,&pIn[1]);` |
|  1525375 | 3147 | `}` |
|        - | 3148 | `/*` |
|        - | 3149 | `` * Consume a leading `namespace\` (see GenStateIsNsRelName) at *ppIn and seed pOut`` |
|        - | 3150 | ` * with the current namespace plus its separator — nothing at global scope, where` |
|        - | 3151 | ` * the bare name already IS the FQN. Returns TRUE when it fired, and the caller` |
|        - | 3152 | `` * must then treat the name it goes on to collect as ABSOLUTE: no `use` import may`` |
|        - | 3153 | ` * apply to it, and the current namespace is already in place.` |
|        - | 3154 | ` */` |
|   474043 | 3155 | `PH7_PRIVATE int GenStateNsRelPrefix(ph7_gen_state *pGen,SyToken **ppIn,SyToken *pEnd,SyBlob *pOut)` |
|        5 | 3156 | `{` |
|   474048 | 3157 | `	SyToken *pIn = *ppIn;` |
|   474048 | 3158 | `	if( !GenStateIsNsRelName(pIn,pEnd) ){` |
|   473954 | 3159 | `		return 0;` |
|        - | 3160 | `	}` |
|       98 | 3161 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       85 | 3162 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       85 | 3163 | `		SyBlobAppend(pOut,"\\",1);` |
|       41 | 3164 | `	}` |
|       98 | 3165 | `	*ppIn = &pIn[2];` |
|       98 | 3166 | `	return 1;` |
|   236706 | 3167 | `}` |
|        - | 3168 | `/*` |
|        - | 3169 | ` * Compile a namespace statement` |
|        - | 3170 | ` * According to the PHP language reference manual` |
|        - | 3171 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|        - | 3172 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|        - | 3173 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|        - | 3174 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|        - | 3175 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|        - | 3176 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|        - | 3177 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|        - | 3178 | ` *  programming world.` |
|        - | 3179 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|        - | 3180 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|        - | 3181 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|        - | 3182 | ` *  classes/functions/constants.` |
|        - | 3183 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|        - | 3184 | ` *  readability of source code.` |
|        - | 3185 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|        - | 3186 | ` *  Here is an example of namespace syntax in PHP:` |
|        - | 3187 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|        - | 3188 | ` *       class MyClass {}` |
|        - | 3189 | ` *       function myfunction() {}` |
|        - | 3190 | ` *       const MYCONST = 1;` |
|        - | 3191 | ` *       $a = new MyClass;` |
|        - | 3192 | ` *       $c = new \my\name\MyClass;` |
|        - | 3193 | ` *       $a = strlen('hi');` |
|        - | 3194 | ` *       $d = namespace\MYCONST;` |
|        - | 3195 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|        - | 3196 | ` *       echo constant($d);` |
|        - | 3197 | ` * NOTE` |
|        - | 3198 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|        - | 3199 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|        - | 3200 | ` */` |
|        - | 3201 | `/*` |
|        - | 3202 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|        - | 3203 | ` */` |
|       24 | 3204 | `PH7_PRIVATE const char * TokenTypeName(sxu32 nType)` |
|        3 | 3205 | `{` |
|       27 | 3206 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|       21 | 3207 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|       21 | 3208 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|        - | 3209 | `	/* php's parse errors call a reserved word a "token", never a "keyword" — the` |
|        - | 3210 | ``	 * word only ever appears in ITS vocabulary as `unexpected token "while"`. */`` |
|       21 | 3211 | `	if( nType & PH7_TK_KEYWORD ){ return "token"; }` |
|       21 | 3212 | `	if( nType & PH7_TK_FQNAME ){ return "fully qualified name"; }` |
|       19 | 3213 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|       19 | 3214 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       12 | 3215 | `	return "token";` |
|       15 | 3216 | `}` |
|      482 | 3217 | `PH7_PRIVATE sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|        5 | 3218 | `{` |
|        - | 3219 | `	SyBlob sName;` |
|        - | 3220 | `	sxu32 nLine;` |
|        - | 3221 | `	sxi32 rc;` |
|        - | 3222 | `	int bBracket;` |
|        - | 3223 | `	int bFirst;` |
|      487 | 3224 | `	nLine = pGen->pIn->nLine;` |
|        - | 3225 | `	/* A top statement only: elsewhere this reached the first-statement rule` |
|        - | 3226 | `	 * and compile-fataled where php fails to parse. */` |
|      487 | 3227 | `	if( !GenStateAtTopStatement(&(*pGen)) ){` |
|       17 | 3228 | `		int iBrace = 0;` |
|       17 | 3229 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,pGen->pCurrent->zInnerTail);` |
|       17 | 3230 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3231 | `			return SXERR_ABORT;` |
|        - | 3232 | `		}` |
|        - | 3233 | `		/* Recover past the whole declaration, a braced body included, and never` |
|        - | 3234 | `		 * past the brace that closes the enclosing block. */` |
|       43 | 3235 | `		for( pGen->pIn++ ; pGen->pIn < pGen->pEnd ; pGen->pIn++ ){` |
|       43 | 3236 | `			if( pGen->pIn->nType & PH7_TK_OCB ){` |
|        5 | 3237 | `				iBrace++;` |
|       41 | 3238 | `			}else if( pGen->pIn->nType & PH7_TK_CCB ){` |
|        5 | 3239 | `				if( iBrace == 0 ){` |
|      ! 0 | 3240 | `					break;` |
|        - | 3241 | `				}` |
|        5 | 3242 | `				if( --iBrace == 0 ){` |
|        5 | 3243 | `					pGen->pIn++;` |
|        5 | 3244 | `					break;` |
|      ! 0 | 3245 | `				}` |
|       35 | 3246 | `			}else if( iBrace == 0 && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       13 | 3247 | `				break;` |
|        - | 3248 | `			}` |
|       14 | 3249 | `		}` |
|       17 | 3250 | `		return SXRET_OK;` |
|        - | 3251 | `	}` |
|      471 | 3252 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|        - | 3253 | `` 	/* php's grammar has three shapes -- `namespace NAME;`, `namespace NAME { }` `` |
|        - | 3254 | ``	 * and `namespace { }` -- and a bare `namespace;` is none of them. */`` |
|      471 | 3255 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        3 | 3256 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"{\"");` |
|        3 | 3257 | `		return ( rc == SXERR_ABORT ) ? SXERR_ABORT : SXRET_OK;` |
|        - | 3258 | `	}` |
|      469 | 3259 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|      469 | 3260 | `	if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|        - | 3261 | ``		/* `namespace \A;` -- php lexed a fully-qualified name where its grammar`` |
|        - | 3262 | `		 * wants a plain one, and names the whole token. */` |
|        3 | 3263 | `		SyBlobAppend(&sName,"\\",1);` |
|        3 | 3264 | `		pGen->pIn++;` |
|        5 | 3265 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 | 3266 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      ! 0 | 3267 | `				SyBlobAppend(&sName,"\\",1);` |
|      ! 0 | 3268 | `			}else{` |
|        3 | 3269 | `				SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        - | 3270 | `			}` |
|        3 | 3271 | `			pGen->pIn++;` |
|        1 | 3272 | `		}` |
|        4 | 3273 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - | 3274 | `			"syntax error, unexpected fully qualified name \"%.*s\", expecting \"{\"",` |
|        2 | 3275 | `			(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName));` |
|        3 | 3276 | `		SyBlobRelease(&sName);` |
|        3 | 3277 | `		return ( rc == SXERR_ABORT ) ? SXERR_ABORT : SXRET_OK;` |
|        - | 3278 | `	}` |
|        - | 3279 | ``	/* Collect the namespace path: namespace Foo\Bar\Baz. A `\` that is not glued`` |
|        - | 3280 | `	 * to a following segment is not a separator token any more (the lexer hands` |
|        - | 3281 | ``	 * it over as php's bare T_NS_SEPARATOR), so `namespace A\ B;` and `A\;` stop`` |
|        - | 3282 | `	 * here and are named below the way php names them. */` |
|     1125 | 3283 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      663 | 3284 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      113 | 3285 | `			if( SyBlobLength(&sName) > 0 ){` |
|      113 | 3286 | `				SyBlobAppend(&sName,"\\",1);` |
|       54 | 3287 | `			}` |
|       59 | 3288 | `		}else{` |
|      555 | 3289 | `			SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        - | 3290 | `		}` |
|      663 | 3291 | `		pGen->pIn++;` |
|        5 | 3292 | `	}` |
|      467 | 3293 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       20 | 3294 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - | 3295 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       12 | 3296 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       14 | 3297 | `		SyBlobRelease(&sName);` |
|       14 | 3298 | `		return ( rc == SXERR_ABORT ) ? SXERR_ABORT : SXRET_OK;` |
|        - | 3299 | `	}` |
|      455 | 3300 | `	bBracket = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|        - | 3301 | `	/* php's zend_compile_namespace, in its order: the two forms never mix in one` |
|        - | 3302 | `	 * file, a block never nests, and the FIRST declaration of either form must be` |
|        - | 3303 | `	 * the first statement -- declares and empty statements aside. */` |
|      455 | 3304 | `	rc = SXRET_OK;` |
|      455 | 3305 | `	if( !pGen->bNsBracketed ){` |
|      401 | 3306 | `		if( pGen->bNsNamed && bBracket ){` |
|        3 | 3307 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 3308 | `				"Cannot mix bracketed namespace declarations with unbracketed namespace declarations");` |
|        6 | 3309 | `		}` |
|      256 | 3310 | `	}else if( !bBracket ){` |
|        3 | 3311 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 3312 | `			"Cannot mix bracketed namespace declarations with unbracketed namespace declarations");` |
|       57 | 3313 | `	}else if( pGen->bNsNamed \|\| pGen->bInNsBlock ){` |
|        3 | 3314 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Namespace declarations cannot be nested");` |
|        1 | 3315 | `	}` |
|      455 | 3316 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 3317 | `		SyBlobRelease(&sName);` |
|      ! 0 | 3318 | `		return SXERR_ABORT;` |
|        - | 3319 | `	}` |
|      455 | 3320 | `	bFirst = ( (!bBracket && !pGen->bNsNamed) \|\| (bBracket && !pGen->bNsBracketed) );` |
|      455 | 3321 | `	if( bFirst && pGen->bStrictTypesLocked ){` |
|       12 | 3322 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 3323 | `			"Namespace declaration statement has to be the very first statement or after any declare call in the script");` |
|       12 | 3324 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3325 | `			SyBlobRelease(&sName);` |
|      ! 0 | 3326 | `			return SXERR_ABORT;` |
|        - | 3327 | `		}` |
|        5 | 3328 | `	}` |
|        - | 3329 | `	/* Switch namespace and clear the previous imports */` |
|      455 | 3330 | `	SyBlobReset(&pGen->sNamespace);` |
|      455 | 3331 | `	GenStateResetUseImports(&(*pGen),pGen->pVm);` |
|      455 | 3332 | `	if( SyBlobLength(&sName) > 0 ){` |
|      437 | 3333 | `		SyBlobAppend(&pGen->sNamespace,SyBlobData(&sName),SyBlobLength(&sName));` |
|      216 | 3334 | `	}` |
|      455 | 3335 | `	pGen->bNsNamed = (sxi8)( SyBlobLength(&sName) > 0 );` |
|      455 | 3336 | `	SyBlobRelease(&sName);` |
|        - | 3337 | `	/* A namespace statement is code for what follows: a declare after it is late. */` |
|      455 | 3338 | `	pGen->bStrictTypesLocked = 1;` |
|      455 | 3339 | `	if( bBracket ){` |
|      117 | 3340 | `		pGen->bNsBracketed = 1;` |
|      117 | 3341 | `		pGen->bInNsBlock = 1;` |
|      117 | 3342 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      117 | 3343 | `		pGen->bInNsBlock = 0;` |
|        - | 3344 | `		/* php's zend_end_namespace: the block's close ends the namespace, and` |
|        - | 3345 | `		 * whatever comes after it outside a block is refused by the dispatcher. */` |
|      117 | 3346 | `		SyBlobReset(&pGen->sNamespace);` |
|      117 | 3347 | `		GenStateResetUseImports(&(*pGen),pGen->pVm);` |
|      117 | 3348 | `		pGen->bNsNamed = 0;` |
|      117 | 3349 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3350 | `			return SXERR_ABORT;` |
|        - | 3351 | `		}` |
|       56 | 3352 | `	}` |
|      455 | 3353 | `	return SXRET_OK;` |
|      246 | 3354 | `}` |
|        - | 3355 | `/*` |
|        - | 3356 | ` * Initialize the three use-import tables of a code generator.` |
|        - | 3357 | ` *` |
|        - | 3358 | ` * php resolves CLASS and FUNCTION imports case-INSENSITIVELY, like every other` |
|        - | 3359 | ``  * name in those two families: `use A\Cee;` then `CEE::K`, `use A\Cee as Alias;` `` |
|        - | 3360 | `` * then `ALIAS::K`, `use function A\eff;` then `EFF()`, and a wrong-case leading`` |
|        - | 3361 | `` * segment of an imported namespace (`use A\B;` then `b\Cee::K`) all resolve.`` |
|        - | 3362 | ` * So both tables fold through SyStrHash/SyStrnmicmp, exactly like hClass /` |
|        - | 3363 | ` * hMethod / hFunction.` |
|        - | 3364 | ` *` |
|        - | 3365 | ` * The CONST table stays BYTE-EXACT: php keeps constant names case-sensitive,` |
|        - | 3366 | `` * so `use const A\KAY;` followed by `kay` must remain an undefined constant.`` |
|        - | 3367 | ` * That asymmetry is why the three tables exist separately.` |
|        - | 3368 | ` */` |
|    55754 | 3369 | `PH7_PRIVATE void GenStateInitUseImports(ph7_gen_state *pGen,ph7_vm *pVm)` |
|        5 | 3370 | `{` |
|    55759 | 3371 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|    55759 | 3372 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|    55759 | 3373 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|    55759 | 3374 | `}` |
|        - | 3375 | `/*` |
|        - | 3376 | ` * Drop every import currently in scope and start a fresh set (a namespace` |
|        - | 3377 | ` * switch clears imports).  Keeps the case rules of GenStateInitUseImports.` |
|        - | 3378 | ` */` |
|    47303 | 3379 | `PH7_PRIVATE void GenStateResetUseImports(ph7_gen_state *pGen,ph7_vm *pVm)` |
|        5 | 3380 | `{` |
|    47308 | 3381 | `	SyHashRelease(&pGen->hUseImports);` |
|    47308 | 3382 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|    47308 | 3383 | `	SyHashRelease(&pGen->hUseConstImports);` |
|    47308 | 3384 | `	GenStateInitUseImports(&(*pGen),&(*pVm));` |
|    47308 | 3385 | `}` |
|        - | 3386 | `/*` |
|        - | 3387 | ` * The two DECLARED-name tables (classes and functions declared so far in this` |
|        - | 3388 | ` * compile unit). php refuses an import whose name a declaration already took, and` |
|        - | 3389 | ` * the check is per COMPILE UNIT and case-INSENSITIVE — a class declared by a file` |
|        - | 3390 | `` * this one later `require`s is invisible to it, because that file compiles after`` |
|        - | 3391 | ` * this one has finished. Both tables key on the FQN, so they survive a namespace` |
|        - | 3392 | ` * switch (which clears only the imports).` |
|        - | 3393 | ` */` |
|    55192 | 3394 | `PH7_PRIVATE void GenStateInitSeenSymbols(ph7_gen_state *pGen,ph7_vm *pVm)` |
|        5 | 3395 | `{` |
|    55197 | 3396 | `	SyHashInit(&pGen->hSeenClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|    55197 | 3397 | `	SyHashInit(&pGen->hSeenFunc,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|    55197 | 3398 | `}` |
|    46747 | 3399 | `PH7_PRIVATE void GenStateReleaseSeenSymbols(ph7_gen_state *pGen)` |
|        5 | 3400 | `{` |
|    46752 | 3401 | `	SyHashRelease(&pGen->hSeenClass);` |
|    46752 | 3402 | `	SyHashRelease(&pGen->hSeenFunc);` |
|    46752 | 3403 | `}` |
|    46741 | 3404 | `PH7_PRIVATE void GenStateResetSeenSymbols(ph7_gen_state *pGen,ph7_vm *pVm)` |
|        5 | 3405 | `{` |
|    46746 | 3406 | `	GenStateReleaseSeenSymbols(&(*pGen));` |
|    46746 | 3407 | `	GenStateInitSeenSymbols(&(*pGen),&(*pVm));` |
|    46746 | 3408 | `}` |
|        - | 3409 | `/*` |
|        - | 3410 | ` * Record one declared CLASS (bFunc = 0) or FUNCTION (bFunc = 1) FQN so a later` |
|        - | 3411 | `` * `use` in this compile unit can see that the name is taken.`` |
|        - | 3412 | ` */` |
|   189449 | 3413 | `PH7_PRIVATE void GenStateRecordDeclaredName(ph7_gen_state *pGen,int bFunc,const SyString *pFqn)` |
|        5 | 3414 | `{` |
|   189454 | 3415 | `	SyHash *pHash = bFunc ? &pGen->hSeenFunc : &pGen->hSeenClass;` |
|        - | 3416 | `	char *zDup;` |
|   189454 | 3417 | `	if( pFqn->nByte < 1 \|\| SyHashGet(pHash,pFqn->zString,pFqn->nByte) != 0 ){` |
|       35 | 3418 | `		return;` |
|        - | 3419 | `	}` |
|   189424 | 3420 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pFqn->zString,pFqn->nByte);` |
|   189424 | 3421 | `	if( zDup ){` |
|        - | 3422 | `		/* The blob the caller built is released on its way out, so the table owns` |
|        - | 3423 | `		 * a pool copy (freed in bulk with the VM, like the import FQNs). */` |
|   189424 | 3424 | `		SyHashInsert(pHash,zDup,pFqn->nByte,zDup);` |
|    94588 | 3425 | `	}` |
|    94608 | 3426 | `}` |
|        - | 3427 | `/*` |
|        - | 3428 | `` * php refuses a DECLARATION whose short name a local `use` import already took:`` |
|        - | 3429 | ` *` |
|        - | 3430 | ` *   use A\Cee;  class Cee {}    Cannot redeclare class B\Cee (previously declared as local import)` |
|        - | 3431 | ` *   use function A\eff;  function eff(){}` |
|        - | 3432 | ` *                               Cannot redeclare function B\eff() (previously declared as local import)` |
|        - | 3433 | ` *   use const A\KAY;  const KAY = 1;` |
|        - | 3434 | ` *                               Cannot declare const B\KAY because the name is already in use` |
|        - | 3435 | ` *` |
|        - | 3436 | `` * A SELF-import (`use B\Cee;` inside `namespace B;`) names this very declaration`` |
|        - | 3437 | ` * and is a no-op, so it is exempt. iKind: 0 = class family (interface/trait/enum` |
|        - | 3438 | ` * included — php says "class" for all four), 1 = function, 2 = const.` |
|        - | 3439 | ` */` |
|   189715 | 3440 | `PH7_PRIVATE sxi32 GenStateGuardImportRedeclare(ph7_gen_state *pGen,int iKind,` |
|        - | 3441 | `	const SyString *pShort,const SyString *pFqn,sxu32 nLine)` |
|        5 | 3442 | `{` |
|        - | 3443 | `	SyHash *pImports;` |
|        - | 3444 | `	SyHashEntry *pEntry;` |
|        - | 3445 | `	const char *zImported;` |
|        - | 3446 | `	sxu32 nImported;` |
|   189720 | 3447 | `	switch( iKind ){` |
|   182338 | 3448 | `		case 1:  pImports = &pGen->hUseFuncImports; break;` |
|      271 | 3449 | `		case 2:  pImports = &pGen->hUseConstImports; break;` |
|     7121 | 3450 | `		default: pImports = &pGen->hUseImports; break;` |
|        - | 3451 | `	}` |
|   189720 | 3452 | `	pEntry = SyHashGet(pImports,(const void *)pShort->zString,pShort->nByte);` |
|   189720 | 3453 | `	if( pEntry == 0 ){` |
|   189706 | 3454 | `		return SXRET_OK;` |
|        - | 3455 | `	}` |
|       18 | 3456 | `	zImported = (const char *)pEntry->pUserData;` |
|       18 | 3457 | `	nImported = SyStrlen(zImported);` |
|       14 | 3458 | `	if( nImported == pFqn->nByte` |
|       22 | 3459 | `	 && (iKind == 2 ? SyMemcmp((const void *)zImported,(const void *)pFqn->zString,nImported) == 0` |
|        8 | 3460 | `	                : SyStrnicmp(zImported,pFqn->zString,nImported) == 0) ){` |
|        7 | 3461 | `		return SXRET_OK; /* the import IS this declaration */` |
|        - | 3462 | `	}` |
|       13 | 3463 | `	if( iKind == 2 ){` |
|        4 | 3464 | `		return PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        1 | 3465 | `			"Cannot declare const %z because the name is already in use",pFqn);` |
|        - | 3466 | `	}` |
|        8 | 3467 | `	return PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        2 | 3468 | `		iKind == 1 ? "Cannot redeclare function %z() (previously declared as local import)"` |
|        2 | 3469 | `		           : "Cannot redeclare class %z (previously declared as local import)",pFqn);` |
|    94739 | 3470 | `}` |
|        - | 3471 | `/*` |
|        - | 3472 | ` * TRUE when pTok is a PHL KEYWORD token that php's lexer nevertheless hands back` |
|        - | 3473 | ` * as a plain T_STRING. php reserves fewer words than PHL's table does, and the` |
|        - | 3474 | `` * `as` clause of a `use` accepts a T_STRING and nothing else — so `use A\Q as`` |
|        - | 3475 | `` * integer;` is a legal (if odd) php import while `use A\Q as echo;` is a parse`` |
|        - | 3476 | ` * error. These are exactly the type-NAME keywords: php spells its scalar types` |
|        - | 3477 | `` * with ordinary labels, and `self`/`parent` too (only `static` is a real token).`` |
|        - | 3478 | ` */` |
|        2 | 3479 | `static int GenStateKeywordIsPhpLabel(SyToken *pTok)` |
|        1 | 3480 | `{` |
|        - | 3481 | `	sxu32 nKey;` |
|        3 | 3482 | `	if( (pTok->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 3483 | `		return 0;` |
|        - | 3484 | `	}` |
|        3 | 3485 | `	nKey = (sxu32)(SX_PTR_TO_INT(pTok->pUserData));` |
|        4 | 3486 | `	return nKey == PH7_TKWRD_BOOL \|\| nKey == PH7_TKWRD_INT \|\| nKey == PH7_TKWRD_FLOAT` |
|        2 | 3487 | `		\|\| nKey == PH7_TKWRD_STRING \|\| nKey == PH7_TKWRD_OBJECT` |
|        3 | 3488 | `		\|\| nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT;` |
|        2 | 3489 | `}` |
|        - | 3490 | `/*` |
|        - | 3491 | ` * TRUE for the names php refuses to let a CLASS import occupy — zend's reserved` |
|        - | 3492 | ` * class names. Distinct from compile_func.c's GenStateIsReservedTypeWord, which` |
|        - | 3493 | ` * answers "is this word a built-in TYPE rather than a class name": that one also` |
|        - | 3494 | `` * covers the `boolean`/`integer`/`double` aliases, and php imports those happily`` |
|        - | 3495 | `` * (`use A\boolean;` is accepted). Matched case-insensitively, like php.`` |
|        - | 3496 | ` */` |
|      234 | 3497 | `static int GenStateIsReservedClassName(const SyString *pName)` |
|        5 | 3498 | `{` |
|        - | 3499 | `	static const char *azWords[] = {` |
|        - | 3500 | `		"self","parent","static","int","float","string","bool","array","object",` |
|        - | 3501 | `		"null","false","true","void","iterable","mixed","never","callable"` |
|        - | 3502 | `	};` |
|        - | 3503 | `	sxu32 i;` |
|     4175 | 3504 | `	for( i = 0 ; i < SX_ARRAYSIZE(azWords) ; i++ ){` |
|     3945 | 3505 | `		sxu32 n = (sxu32)SyStrlen(azWords[i]);` |
|     3945 | 3506 | `		if( pName->nByte == n && SyStrnicmp(pName->zString,azWords[i],n) == 0 ){` |
|        6 | 3507 | `			return 1;` |
|        - | 3508 | `		}` |
|     1973 | 3509 | `	}` |
|      235 | 3510 | `	return 0;` |
|      122 | 3511 | `}` |
|        - | 3512 | `/*` |
|        - | 3513 | `` * Register one resolved `use` import: alias -> FQN, in the table its KIND owns`` |
|        - | 3514 | ` * (iUseType: 0 = class, 1 = function, 2 = const).  Shared by the plain form` |
|        - | 3515 | `` * (`use A\Cee;`) and by each member of a group (`use A\{Cee, Dee};`).`` |
|        - | 3516 | ` */` |
|      314 | 3517 | `static sxi32 GenStateAddImport(` |
|        - | 3518 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - | 3519 | `	int iUseType,         /* 0=class, 1=function, 2=const */` |
|        - | 3520 | `	SyBlob *pPath,        /* Fully qualified name being imported */` |
|        - | 3521 | `	SyString *pAlias,     /* Short name it is imported under */` |
|        - | 3522 | `	sxu32 nLine           /* Line of the 'use' keyword (for diagnostics) */` |
|        - | 3523 | `	)` |
|        5 | 3524 | `{` |
|        - | 3525 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|        - | 3526 | `	const char *zKind;  /* php's kind word in the "already in use" message */` |
|        - | 3527 | `	char *zDup;` |
|        - | 3528 | `	sxi32 rc;` |
|        - | 3529 | `	/* Select the target hash table based on import type. */` |
|      319 | 3530 | `	switch( iUseType ){` |
|       46 | 3531 | `		case 1:  pGenHash = &pGen->hUseFuncImports; break;` |
|       42 | 3532 | `		case 2:  pGenHash = &pGen->hUseConstImports; break;` |
|      239 | 3533 | `		default: pGenHash = &pGen->hUseImports; break;` |
|        - | 3534 | `	}` |
|        - | 3535 | `	/* php names the KIND of a non-class import in this message: "Cannot use` |
|        - | 3536 | `	 * function A\eff as eff …" / "Cannot use const A\KAY as KAY …". */` |
|      319 | 3537 | `	zKind = iUseType == 1 ? "function " : iUseType == 2 ? "const " : "";` |
|        - | 3538 | `	/* A CLASS import may not take a reserved class name, however it got there:` |
|        - | 3539 | ``	 * as the trailing segment (`use A\self;`) or as an explicit alias`` |
|        - | 3540 | ``	 * (`use A\Q as self;`). Only classes — `use function A\self;` and`` |
|        - | 3541 | ``	 * `use const A\self;` are both accepted by php. */`` |
|      319 | 3542 | `	if( iUseType == 0 && GenStateIsReservedClassName(pAlias) ){` |
|        8 | 3543 | `		return PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 3544 | `			"Cannot use %.*s as %z because '%z' is a special class name",` |
|        4 | 3545 | `			(int)SyBlobLength(pPath),(const char *)SyBlobData(pPath),pAlias,pAlias);` |
|        - | 3546 | `	}` |
|        - | 3547 | `	/* Check for duplicate import alias (per-type) */` |
|      315 | 3548 | `	if( SyHashGet(pGenHash,pAlias->zString,pAlias->nByte) != 0 ){` |
|       12 | 3549 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 3550 | `			"Cannot use %s%.*s as %z because the name is already in use",` |
|        6 | 3551 | `			zKind,(int)SyBlobLength(pPath),(const char *)SyBlobData(pPath),pAlias);` |
|        9 | 3552 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3553 | `			return SXERR_ABORT;` |
|        - | 3554 | `		}` |
|        3 | 3555 | `	}` |
|        - | 3556 | `	/* …and refuses one whose name a DECLARATION in this compile unit already took` |
|        - | 3557 | ``	 * (`class Cee {} use A\Cee;`), unless the import names that very declaration.`` |
|        - | 3558 | `	 * The name an import occupies is the alias in the CURRENT namespace, which is` |
|        - | 3559 | `	 * what the seen tables key on. php runs this check for classes and functions` |
|        - | 3560 | ``	 * only — a `const` declaration followed by its own `use const` is accepted. */`` |
|      315 | 3561 | `	if( iUseType != 2 ){` |
|        - | 3562 | `		SyBlob sTaken;` |
|      277 | 3563 | `		SyBlobInit(&sTaken,&pGen->pVm->sAllocator);` |
|      277 | 3564 | `		GenStateBuildFQN(&(*pGen),pAlias,&sTaken);` |
|      272 | 3565 | `		if( SyHashGet(iUseType == 1 ? &pGen->hSeenFunc : &pGen->hSeenClass,` |
|      408 | 3566 | `				SyBlobData(&sTaken),SyBlobLength(&sTaken)) != 0` |
|      143 | 3567 | `		 && (SyBlobLength(&sTaken) != SyBlobLength(pPath)` |
|        4 | 3568 | `			\|\| SyStrnicmp((const char *)SyBlobData(&sTaken),(const char *)SyBlobData(pPath),` |
|        6 | 3569 | `				(sxu32)SyBlobLength(&sTaken)) != 0) ){` |
|        8 | 3570 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 3571 | `				"Cannot use %s%.*s as %z because the name is already in use",` |
|        4 | 3572 | `				zKind,(int)SyBlobLength(pPath),(const char *)SyBlobData(pPath),pAlias);` |
|        6 | 3573 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3574 | `				SyBlobRelease(&sTaken);` |
|      ! 0 | 3575 | `				return SXERR_ABORT;` |
|        - | 3576 | `			}` |
|        2 | 3577 | `		}` |
|      277 | 3578 | `		SyBlobRelease(&sTaken);` |
|      136 | 3579 | `	}` |
|        - | 3580 | `	/* Register the import: alias -> FQN.` |
|        - | 3581 | `	 * Strings are allocated from the VM pool allocator and freed` |
|        - | 3582 | `	 * when the entire VM is released. SyHashRelease does not free` |
|        - | 3583 | `	 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|      470 | 3584 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      310 | 3585 | `		(const char *)SyBlobData(pPath),SyBlobLength(pPath));` |
|      315 | 3586 | `	if( zDup ){` |
|        - | 3587 | `		/* All three kinds resolve entirely at COMPILE time — a const import is read` |
|        - | 3588 | `		 * by the OP_LOADC candidate builder (compile_node.c), so no runtime table` |
|        - | 3589 | `		 * is needed for it either. */` |
|      315 | 3590 | `		SyHashInsert(pGenHash,pAlias->zString,pAlias->nByte,zDup);` |
|      155 | 3591 | `	}` |
|      315 | 3592 | `	return SXRET_OK;` |
|      162 | 3593 | `}` |
|        - | 3594 | `/*` |
|        - | 3595 | `` * Collect one `\`-separated name into pOut (appending to whatever it holds, with`` |
|        - | 3596 | ` * a separator when needed) and return its LAST segment token, or 0 when the` |
|        - | 3597 | ` * cursor is not on a name at all.` |
|        - | 3598 | ` */` |
|      336 | 3599 | `static SyToken * GenStateCollectNsPath(ph7_gen_state *pGen,SyBlob *pOut)` |
|        5 | 3600 | `{` |
|      341 | 3601 | `	SyToken *pLast = 0;` |
|      341 | 3602 | ``	int bAfterSep = 0;   /* the token just consumed was a `\` */`` |
|     1257 | 3603 | `	while( pGen->pIn < pGen->pEnd ){` |
|     1255 | 3604 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      589 | 3605 | `			bAfterSep = ( pGen->pIn + 1 < pGen->pEnd` |
|      292 | 3606 | `				&& GenStateTokensGlued(pGen->pIn,&pGen->pIn[1]) );` |
|      297 | 3607 | `			pGen->pIn++;` |
|      297 | 3608 | `			continue;` |
|        - | 3609 | `		}` |
|      963 | 3610 | `		if( (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|        - | 3611 | `			/* php lexes a QUALIFIED name as one T_NAME_QUALIFIED token before it` |
|        - | 3612 | `			 * consults the keyword table, so every reserved word is a legal SEGMENT` |
|        - | 3613 | ``			 * of it — `use A\Default\Q;`, `use Default\Q;`, `use A\as;` are all`` |
|        - | 3614 | `			 * accepted — while a BARE reserved word is not a name at all` |
|        - | 3615 | ``			 * (`use Default;` is a php parse error, and so is `use A\{Default};`).`` |
|        - | 3616 | ``			 * A `\` on one side or the other is exactly what separates the two, and`` |
|        - | 3617 | `` 			 * it must be the IMMEDIATE neighbour: the `as` of `use A\Cee as Baz;` `` |
|        - | 3618 | `			 * carries no separator and must still end the path. */` |
|      367 | 3619 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      187 | 3620 | `				break;` |
|        - | 3621 | `			}` |
|      180 | 3622 | `			if( !bAfterSep` |
|      170 | 3623 | `			 && !(pGen->pIn + 1 < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_NSSEP)` |
|       76 | 3624 | `				&& GenStateTokensGlued(pGen->pIn,&pGen->pIn[1])) ){` |
|       80 | 3625 | `				break;` |
|        - | 3626 | `			}` |
|       14 | 3627 | `		}` |
|      629 | 3628 | `		pLast = pGen->pIn;` |
|      629 | 3629 | `		if( SyBlobLength(pOut) > 0 ){` |
|      331 | 3630 | `			SyBlobAppend(pOut,"\\",1);` |
|      163 | 3631 | `		}` |
|      629 | 3632 | `		SyBlobAppend(pOut,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      629 | 3633 | `		bAfterSep = 0;` |
|      629 | 3634 | `		pGen->pIn++;` |
|        5 | 3635 | `	}` |
|      341 | 3636 | `	return pLast;` |
|        5 | 3637 | `}` |
|        - | 3638 | `/*` |
|        - | 3639 | `` * Park the cursor on the `;` that ends this declaration so a refused `use` does`` |
|        - | 3640 | ` * not leave its remaining tokens for the statement dispatcher to read as an` |
|        - | 3641 | ` * expression, which would pile a second diagnostic on the first.` |
|        - | 3642 | ` */` |
|       48 | 3643 | `static void GenStateSkipToStatementEnd(ph7_gen_state *pGen)` |
|        2 | 3644 | `{` |
|      196 | 3645 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      148 | 3646 | `		pGen->pIn++;` |
|        2 | 3647 | `	}` |
|       50 | 3648 | `}` |
|        - | 3649 | `/*` |
|        - | 3650 | `` * `namespace\X` lexes as php's T_NAME_RELATIVE, and a `use` statement takes a`` |
|        - | 3651 | ` * plain or fully-qualified name only. php refuses it and NAMES the token kind in` |
|        - | 3652 | ` * the message, a wording TokenTypeName cannot spell. Consumes the name, reports,` |
|        - | 3653 | ` * and answers TRUE when it fired; zExpecting is php's trailing clause (empty for` |
|        - | 3654 | ` * the plain form, the member list for a group).` |
|        - | 3655 | ` */` |
|      338 | 3656 | `static int GenStateUseRejectNsRelName(ph7_gen_state *pGen,sxu32 nLine,const char *zExpecting,sxi32 *pRc)` |
|        5 | 3657 | `{` |
|        - | 3658 | `	SyBlob sName;` |
|      343 | 3659 | `	if( !GenStateIsNsRelName(pGen->pIn,pGen->pEnd) ){` |
|      341 | 3660 | `		return 0;` |
|        - | 3661 | `	}` |
|        3 | 3662 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|        3 | 3663 | `	SyBlobAppend(&sName,"namespace",sizeof("namespace")-1);` |
|        3 | 3664 | ``	pGen->pIn++; /* the `namespace` keyword; the `\` and its segments follow */`` |
|        5 | 3665 | `	while( pGen->pIn + 1 < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP)` |
|        5 | 3666 | `		&& (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 | 3667 | `		SyBlobAppend(&sName,"\\",1);` |
|        3 | 3668 | `		SyBlobAppend(&sName,pGen->pIn[1].sData.zString,pGen->pIn[1].sData.nByte);` |
|        3 | 3669 | `		pGen->pIn += 2;` |
|        1 | 3670 | `	}` |
|        5 | 3671 | `	*pRc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - | 3672 | `		"syntax error, unexpected namespace-relative name \"%.*s\"%s",` |
|        2 | 3673 | `		(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName),zExpecting);` |
|        3 | 3674 | `	SyBlobRelease(&sName);` |
|        3 | 3675 | `	return 1;` |
|      174 | 3676 | `}` |
|        - | 3677 | `/*` |
|        - | 3678 | ` * TRUE when pTok is a PHL IDENTIFIER that php's lexer nevertheless reserves. The` |
|        - | 3679 | `` * alpha operators (`and`, `or`, `xor`, `new`, `clone`, `instanceof`) reach the`` |
|        - | 3680 | `` * parser here as PH7_TK_ID\|PH7_TK_OP, and `readonly`/`callable` are`` |
|        - | 3681 | ` * context-sensitive identifiers — php has a real token for every one of them, so` |
|        - | 3682 | ` * none may stand where its grammar asks for a T_STRING.` |
|        - | 3683 | ` */` |
|      150 | 3684 | `static int GenStateIdIsPhpKeyword(SyToken *pTok)` |
|        4 | 3685 | `{` |
|        - | 3686 | `	static const char *azWords[] = {` |
|        - | 3687 | `		"and","or","xor","new","clone","instanceof","readonly","callable"` |
|        - | 3688 | `	};` |
|        - | 3689 | `	sxu32 i;` |
|      154 | 3690 | `	if( (pTok->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 | 3691 | `		return 0;` |
|        - | 3692 | `	}` |
|     1354 | 3693 | `	for( i = 0 ; i < SX_ARRAYSIZE(azWords) ; i++ ){` |
|     1204 | 3694 | `		sxu32 n = (sxu32)SyStrlen(azWords[i]);` |
|     1204 | 3695 | `		if( pTok->sData.nByte == n && SyStrnicmp(pTok->sData.zString,azWords[i],n) == 0 ){` |
|      ! 0 | 3696 | `			return 1;` |
|        - | 3697 | `		}` |
|      604 | 3698 | `	}` |
|      154 | 3699 | `	return 0;` |
|       79 | 3700 | `}` |
|        - | 3701 | `/*` |
|        - | 3702 | `` * Consume the optional `as Alias` clause, leaving *pAlias untouched when absent.`` |
|        - | 3703 | ` * php's grammar takes a T_STRING there and nothing else, so a reserved word is a` |
|        - | 3704 | ` * parse error however PHL's lexer happens to have classified it. Returns TRUE` |
|        - | 3705 | ` * when it reported one, and the caller must then abandon the declaration.` |
|        - | 3706 | ` */` |
|      316 | 3707 | `static int GenStateCollectImportAlias(ph7_gen_state *pGen,SyString *pAlias,sxi32 *pRc)` |
|        5 | 3708 | `{` |
|      316 | 3709 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      238 | 3710 | `		&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      156 | 3711 | `		pGen->pIn++; /* Jump 'as' */` |
|      156 | 3712 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 3713 | `			return 0;` |
|        - | 3714 | `		}` |
|      156 | 3715 | `		if( (pGen->pIn->nType & PH7_TK_ID) && !GenStateIdIsPhpKeyword(pGen->pIn) ){` |
|      154 | 3716 | `			*pAlias = pGen->pIn->sData;` |
|      154 | 3717 | `			pGen->pIn++;` |
|      154 | 3718 | `			return 0;` |
|        - | 3719 | `		}` |
|        3 | 3720 | `		if( GenStateKeywordIsPhpLabel(pGen->pIn) ){` |
|        - | 3721 | ``			/* php spells its scalar types and `self`/`parent` with plain labels,`` |
|        - | 3722 | ``			 * so those ARE legal aliases — `use A\Q as integer;` compiles, and`` |
|        - | 3723 | ``			 * `use A\Q as self;` reaches the special-class-name check instead. */`` |
|      ! 0 | 3724 | `			*pAlias = pGen->pIn->sData;` |
|      ! 0 | 3725 | `			pGen->pIn++;` |
|      ! 0 | 3726 | `			return 0;` |
|        - | 3727 | `		}` |
|        - | 3728 | `		{` |
|        - | 3729 | `			/* php prints a reserved word LOWER-CASED in this message, whatever the` |
|        - | 3730 | ``			 * source spelled: `use A\Q as Default;` reads `unexpected token`` |
|        - | 3731 | ``			 * "default"`. Longest reserved word here is `include_once` (12). */`` |
|        - | 3732 | `			char zLower[16];` |
|        3 | 3733 | `			SyString sTok = pGen->pIn->sData;` |
|        - | 3734 | `			sxu32 i;` |
|        4 | 3735 | `			int bKeyword = ( (pGen->pIn->nType & PH7_TK_KEYWORD) != 0` |
|        2 | 3736 | `				\|\| GenStateIdIsPhpKeyword(pGen->pIn) );` |
|        5 | 3737 | `			int bWord = ( (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        2 | 3738 | `				&& sTok.nByte <= sizeof(zLower) );` |
|       17 | 3739 | `			for( i = 0 ; bWord && i < sTok.nByte ; i++ ){` |
|       15 | 3740 | `				unsigned char c = (unsigned char)sTok.zString[i];` |
|       15 | 3741 | `				zLower[i] = (char)((c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c);` |
|        8 | 3742 | `			}` |
|        3 | 3743 | `			if( bWord ){` |
|        3 | 3744 | `				SyStringInitFromBuf(&sTok,zLower,sTok.nByte);` |
|        1 | 3745 | `			}` |
|        - | 3746 | ``			/* `die` and `exit` are the same token to php, and it names it `exit`. */`` |
|        3 | 3747 | `			if( bWord && sTok.nByte == 3 && SyMemcmp(sTok.zString,"die",3) == 0 ){` |
|      ! 0 | 3748 | `				SyStringInitFromBuf(&sTok,"exit",sizeof("exit")-1);` |
|      ! 0 | 3749 | `			}` |
|        4 | 3750 | `			*pRc = PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|        - | 3751 | `				"syntax error, unexpected %s \"%z\", expecting identifier",` |
|        1 | 3752 | `				bKeyword ? "token" : TokenTypeName(pGen->pIn->nType),&sTok);` |
|        - | 3753 | `		}` |
|        3 | 3754 | `		return 1;` |
|        - | 3755 | `	}` |
|      169 | 3756 | `	return 0;` |
|      163 | 3757 | `}` |
|        - | 3758 | `/*` |
|        - | 3759 | `` * Park the cursor on whichever of `}` / `;` ends the group, so a refused member`` |
|        - | 3760 | ` * does not cascade into the ones after it.` |
|        - | 3761 | ` */` |
|      ! 0 | 3762 | `static void GenStateSkipToGroupEnd(ph7_gen_state *pGen)` |
|      ! 0 | 3763 | `{` |
|      ! 0 | 3764 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_CCB\|PH7_TK_SEMI)) == 0 ){` |
|      ! 0 | 3765 | `		pGen->pIn++;` |
|      ! 0 | 3766 | `	}` |
|      ! 0 | 3767 | `}` |
|        - | 3768 | `/*` |
|        - | 3769 | ` * Compile the members of a GROUP use declaration (php 7.0):` |
|        - | 3770 | ` *` |
|        - | 3771 | ` *      use A\{Cee, Dee as D2, Sub\Eee};` |
|        - | 3772 | ` *      use function A\{f, g as h};` |
|        - | 3773 | ` *      use A\{function f, const K, Cee};   // per-member kind, untyped group only` |
|        - | 3774 | ` *` |
|        - | 3775 | `` * pPrefix holds the path before the brace; the cursor sits on `{`.  Each member`` |
|        - | 3776 | `` * is the prefix, a `\`, and the member's own (possibly multi-segment) name.  A`` |
|        - | 3777 | ` * trailing comma is allowed, an empty group is not.` |
|        - | 3778 | ` */` |
|       16 | 3779 | `static sxi32 GenStateCompileGroupUse(ph7_gen_state *pGen,SyBlob *pPrefix,int iUseType,sxu32 nLine)` |
|        2 | 3780 | `{` |
|        - | 3781 | `	SyBlob sPath;` |
|       18 | 3782 | `	sxi32 rc = SXRET_OK;` |
|       18 | 3783 | `	pGen->pIn++; /* Jump '{' */` |
|       18 | 3784 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       17 | 3785 | `	for(;;){` |
|       36 | 3786 | `		int iMemberType = iUseType;` |
|        - | 3787 | `		SyString sAlias;` |
|        - | 3788 | `		SyToken *pLast;` |
|        - | 3789 | ``		/* `function`/`const` may qualify a single member, but only inside a`` |
|        - | 3790 | ``		 * group that is not itself typed (php rejects `use function A\{const C}`). */`` |
|       36 | 3791 | `		if( iUseType == 0 && pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        5 | 3792 | `			sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|        5 | 3793 | `			if( nKey == PH7_TKWRD_FUNCTION ){` |
|        3 | 3794 | `				iMemberType = 1;` |
|        3 | 3795 | `				pGen->pIn++;` |
|        4 | 3796 | `			}else if( nKey == PH7_TKWRD_CONST ){` |
|        3 | 3797 | `				iMemberType = 2;` |
|        3 | 3798 | `				pGen->pIn++;` |
|        1 | 3799 | `			}` |
|        2 | 3800 | `		}` |
|       36 | 3801 | `		SyBlobReset(&sPath);` |
|       36 | 3802 | `		SyBlobAppend(&sPath,SyBlobData(pPrefix),SyBlobLength(pPrefix));` |
|       36 | 3803 | `		if( GenStateUseRejectNsRelName(&(*pGen),nLine,` |
|        - | 3804 | `				", expecting identifier or namespaced name or \"function\" or \"const\"",&rc) ){` |
|      ! 0 | 3805 | `			GenStateSkipToGroupEnd(&(*pGen));` |
|      ! 0 | 3806 | `			break;` |
|        - | 3807 | `		}` |
|       36 | 3808 | `		pLast = GenStateCollectNsPath(pGen,&sPath);` |
|       36 | 3809 | `		if( pLast == 0 ){` |
|        - | 3810 | ``			/* No member name: `use A\{};` or a stray token.  Report once, then`` |
|        - | 3811 | `			 * skip to the end of the group so the statement does not cascade. */` |
|      ! 0 | 3812 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - | 3813 | `				"syntax error, unexpected %s \"%z\", expecting identifier",` |
|      ! 0 | 3814 | `				TokenTypeName(pGen->pIn < pGen->pEnd ? pGen->pIn->nType : 0),` |
|      ! 0 | 3815 | `				pGen->pIn < pGen->pEnd ? &pGen->pIn->sData : 0);` |
|      ! 0 | 3816 | `			GenStateSkipToGroupEnd(&(*pGen));` |
|      ! 0 | 3817 | `			break;` |
|        - | 3818 | `		}` |
|       36 | 3819 | `		sAlias = pLast->sData; /* Default alias is the member's last component */` |
|       36 | 3820 | `		if( GenStateCollectImportAlias(pGen,&sAlias,&rc) ){` |
|      ! 0 | 3821 | `			GenStateSkipToGroupEnd(&(*pGen));` |
|      ! 0 | 3822 | `			break;` |
|        - | 3823 | `		}` |
|       36 | 3824 | `		rc = GenStateAddImport(&(*pGen),iMemberType,&sPath,&sAlias,nLine);` |
|       36 | 3825 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3826 | `			break;` |
|        - | 3827 | `		}` |
|       36 | 3828 | `		rc = SXRET_OK;` |
|       36 | 3829 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       22 | 3830 | `			pGen->pIn++;` |
|       22 | 3831 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) ){` |
|        3 | 3832 | `				break; /* Trailing comma before the closing brace */` |
|        - | 3833 | `			}` |
|       20 | 3834 | `			continue;` |
|        - | 3835 | `		}` |
|       16 | 3836 | `		break;` |
|      ! 0 | 3837 | `	}` |
|       18 | 3838 | `	SyBlobRelease(&sPath);` |
|       18 | 3839 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 3840 | `		return SXERR_ABORT;` |
|        - | 3841 | `	}` |
|       18 | 3842 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) ){` |
|       18 | 3843 | `		pGen->pIn++; /* Jump '}' */` |
|        8 | 3844 | `	}` |
|       18 | 3845 | `	return SXRET_OK;` |
|       10 | 3846 | `}` |
|        - | 3847 | `/*` |
|        - | 3848 | ` * Compile the 'use' statement` |
|        - | 3849 | ` * According to the PHP language reference manual` |
|        - | 3850 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|        - | 3851 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|        - | 3852 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|        - | 3853 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|        - | 3854 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|        - | 3855 | ` *  a function or constant is not supported.` |
|        - | 3856 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|        - | 3857 | ` * NOTE` |
|        - | 3858 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|        - | 3859 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|        - | 3860 | ` */` |
|      346 | 3861 | `PH7_PRIVATE sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|        5 | 3862 | `{` |
|        - | 3863 | `	sxu32 nLine;` |
|        - | 3864 | `	sxi32 rc;` |
|        - | 3865 | `	SyBlob sPath;` |
|        - | 3866 | `	SyString sAlias;` |
|        - | 3867 | `	SyToken *pLast;` |
|        - | 3868 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|        - | 3869 | `	int bGroup;` |
|      351 | 3870 | `	nLine = pGen->pIn->nLine;` |
|        - | 3871 | `	/* A top statement only: elsewhere this imported the name for the rest of` |
|        - | 3872 | `	 * the file where php fails to parse. */` |
|      351 | 3873 | `	if( !GenStateAtTopStatement(&(*pGen)) ){` |
|       45 | 3874 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,pGen->pCurrent->zInnerTail);` |
|       45 | 3875 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3876 | `			return SXERR_ABORT;` |
|        - | 3877 | `		}` |
|       45 | 3878 | `		pGen->pIn++;` |
|       45 | 3879 | `		GenStateSkipToStatementEnd(&(*pGen));` |
|       45 | 3880 | `		return SXRET_OK;` |
|        - | 3881 | `	}` |
|      307 | 3882 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|        - | 3883 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|      307 | 3884 | `	iUseType = 0;` |
|      307 | 3885 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       78 | 3886 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|       78 | 3887 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|       42 | 3888 | `			iUseType = 1;` |
|       42 | 3889 | `			pGen->pIn++;` |
|       59 | 3890 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|       38 | 3891 | `			iUseType = 2;` |
|       38 | 3892 | `			pGen->pIn++;` |
|       17 | 3893 | `		}` |
|       37 | 3894 | `	}` |
|      307 | 3895 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|        - | 3896 | `	/* Process one or more use declarations separated by commas */` |
|      152 | 3897 | `	for(;;){` |
|      309 | 3898 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 3899 | `			break;` |
|        - | 3900 | `		}` |
|      309 | 3901 | `		SyBlobReset(&sPath);` |
|      309 | 3902 | `		if( GenStateUseRejectNsRelName(&(*pGen),nLine,"",&rc) ){` |
|        3 | 3903 | `			SyBlobRelease(&sPath);` |
|        3 | 3904 | `			GenStateSkipToStatementEnd(&(*pGen));` |
|        3 | 3905 | `			return ( rc == SXERR_ABORT ) ? SXERR_ABORT : SXRET_OK;` |
|        - | 3906 | `		}` |
|        - | 3907 | `		/* Collect the full namespace path */` |
|      307 | 3908 | `		pLast = GenStateCollectNsPath(pGen,&sPath);` |
|        - | 3909 | ``		/* php's group form is `NAME \ {`: the separator before the brace is the`` |
|        - | 3910 | `		 * one bare T_NS_SEPARATOR its grammar takes, a token of its own (spaces` |
|        - | 3911 | ``		 * on either side are fine), and `use A\B{C}` without it is refused. */`` |
|      307 | 3912 | `		bGroup = 0;` |
|      302 | 3913 | `		if( pGen->pIn + 1 < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OTHER)` |
|      155 | 3914 | `		 && pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\\'` |
|       21 | 3915 | `		 && (pGen->pIn[1].nType & PH7_TK_OCB) ){` |
|       18 | 3916 | `			pGen->pIn++;` |
|       18 | 3917 | `			bGroup = 1;` |
|      296 | 3918 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OTHER)` |
|      147 | 3919 | `		 && pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\\' ){` |
|        - | 3920 | ``			/* `use A\ B;` -- php's parser is past the separator and wants the brace,`` |
|        - | 3921 | `			 * so it names what stood there instead. */` |
|      ! 0 | 3922 | `			rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn + 1 < pGen->pEnd ? &pGen->pIn[1] : 0,"\"{\"");` |
|      ! 0 | 3923 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 3924 | `			GenStateSkipToStatementEnd(&(*pGen));` |
|      ! 0 | 3925 | `			return ( rc == SXERR_ABORT ) ? SXERR_ABORT : SXRET_OK;` |
|        - | 3926 | `		}` |
|      307 | 3927 | `		if( bGroup && SyBlobLength(&sPath) > 0 ){` |
|        - | 3928 | `			/* GROUP declaration: what was collected is the shared prefix.  php` |
|        - | 3929 | `			 * does not let a group be comma-combined with another declaration,` |
|        - | 3930 | `			 * so the members close the statement. */` |
|       18 | 3931 | `			rc = GenStateCompileGroupUse(&(*pGen),&sPath,iUseType,nLine);` |
|       18 | 3932 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3933 | `				SyBlobRelease(&sPath);` |
|      ! 0 | 3934 | `				return SXERR_ABORT;` |
|        - | 3935 | `			}` |
|       18 | 3936 | `			break;` |
|        - | 3937 | `		}` |
|      291 | 3938 | `		if( pLast == 0 ){` |
|        - | 3939 | `			/* Empty path */` |
|        5 | 3940 | `			break;` |
|        - | 3941 | `		}` |
|        - | 3942 | `		/* Default alias is the last component of the path */` |
|      287 | 3943 | `		sAlias = pLast->sData;` |
|        - | 3944 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      287 | 3945 | `		if( GenStateCollectImportAlias(pGen,&sAlias,&rc) ){` |
|        3 | 3946 | `			SyBlobRelease(&sPath);` |
|        3 | 3947 | `			GenStateSkipToStatementEnd(&(*pGen));` |
|        3 | 3948 | `			return ( rc == SXERR_ABORT ) ? SXERR_ABORT : SXRET_OK;` |
|        - | 3949 | `		}` |
|      285 | 3950 | `		rc = GenStateAddImport(&(*pGen),iUseType,&sPath,&sAlias,nLine);` |
|      285 | 3951 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3952 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 3953 | `			return SXERR_ABORT;` |
|        - | 3954 | `		}` |
|        - | 3955 | `		/* Check for comma (multiple use declarations) */` |
|      285 | 3956 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|        3 | 3957 | `			pGen->pIn++;` |
|        2 | 3958 | `		}else{` |
|      144 | 3959 | `			break;` |
|        - | 3960 | `		}` |
|        1 | 3961 | `	}` |
|      303 | 3962 | `	SyBlobRelease(&sPath);` |
|      303 | 3963 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        8 | 3964 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|        4 | 3965 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        6 | 3966 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3967 | `			return SXERR_ABORT;` |
|        - | 3968 | `		}` |
|        2 | 3969 | `	}` |
|      303 | 3970 | `	return SXRET_OK;` |
|      178 | 3971 | `}` |
|        - | 3972 | `/*` |
|        - | 3973 | ` * Compile the stupid 'declare' language construct.` |
|        - | 3974 | ` *` |
|        - | 3975 | ` * According to the PHP language reference manual.` |
|        - | 3976 | ` *  The declare construct is used to set execution directives for a block of code.` |
|        - | 3977 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|        - | 3978 | ` *  declare (directive)` |
|        - | 3979 | ` *   statement` |
|        - | 3980 | ` * The directive section allows the behavior of the declare block to be set.` |
|        - | 3981 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|        - | 3982 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|        - | 3983 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|        - | 3984 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|        - | 3985 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|        - | 3986 | ` * <?php` |
|        - | 3987 | ` * // these are the same:` |
|        - | 3988 | ` * // you can use this:` |
|        - | 3989 | ` * declare(ticks=1) {` |
|        - | 3990 | ` *   // entire script here` |
|        - | 3991 | ` * }` |
|        - | 3992 | ` * // or you can use this:` |
|        - | 3993 | ` * declare(ticks=1);` |
|        - | 3994 | ` * // entire script here` |
|        - | 3995 | ` * ?>` |
|        - | 3996 | ` *` |
|        - | 3997 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|        - | 3998 | ` */` |
|        - | 3999 | `/*` |
|        - | 4000 | ` * Match a directive name against a known literal (case-insensitive).` |
|        - | 4001 | ` */` |
|      248 | 4002 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|        5 | 4003 | `{` |
|      327 | 4004 | `	return SyStringLength(pName) == nWant` |
|      248 | 4005 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|        5 | 4006 | `}` |
|        - | 4007 |  |
|      114 | 4008 | `PH7_PRIVATE sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|        5 | 4009 | `{` |
|      119 | 4010 | `	sxu32 nLine = pGen->pIn->nLine;` |
|      119 | 4011 | `	SyToken *pBodyEnd = 0;` |
|        - | 4012 | `	SyToken *pBodyStart;` |
|        - | 4013 | `	SyToken *pCursor;` |
|        - | 4014 | `	int bHasStrictTypes;` |
|        - | 4015 | `	int bBlockForm;` |
|        - | 4016 | `	int bPlacementOk;` |
|        - | 4017 | `	sxi32 rc;` |
|      119 | 4018 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|      119 | 4019 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|        6 | 4020 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|        6 | 4021 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4022 | `			return SXERR_ABORT;` |
|        - | 4023 | `		}` |
|        6 | 4024 | `		goto Synchro;` |
|        - | 4025 | `	}` |
|      115 | 4026 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|      115 | 4027 | `	pBodyStart = pGen->pIn;` |
|        - | 4028 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|      115 | 4029 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|      115 | 4030 | `	if( pBodyEnd >= pGen->pEnd ){` |
|      ! 0 | 4031 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\")\"");` |
|      ! 0 | 4032 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4033 | `			return SXERR_ABORT;` |
|        - | 4034 | `		}` |
|      ! 0 | 4035 | `		return SXRET_OK;` |
|        - | 4036 | `	}` |
|        - | 4037 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|        - | 4038 | `	 * now delimits the comma-separated directive list. */` |
|      115 | 4039 | `	pGen->pIn = &pBodyEnd[1];` |
|      115 | 4040 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|      ! 0 | 4041 | `		if( pGen->pIn >= pGen->pEnd && GenStateAtChunkEofStmt(pGen) ){` |
|        - | 4042 | `			/* Ran out of input: php's parser has an unfinished statement and names` |
|        - | 4043 | ``			 * that, not a sentence about `declare` (the shared end-of-input check`` |
|        - | 4044 | `			 * in compile.c words every other statement the same way). */` |
|      ! 0 | 4045 | `			rc = PH7_GenSyntaxError(&(*pGen),0,0);` |
|      ! 0 | 4046 | `		}else{` |
|      ! 0 | 4047 | `			rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"declare: Expecting ';' or '{' after directive");` |
|        - | 4048 | `		}` |
|      ! 0 | 4049 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4050 | `			return SXERR_ABORT;` |
|        - | 4051 | `		}` |
|      ! 0 | 4052 | `	}` |
|        - | 4053 | ``	/* `declare(...): ... enddeclare;` is php's alternative syntax for the braced`` |
|        - | 4054 | `	 * body, and counts as block mode the same way. */` |
|      115 | 4055 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_COLON)) ) ? 1 : 0;` |
|      115 | 4056 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|      115 | 4057 | `	bHasStrictTypes = 0;` |
|        - | 4058 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|        - | 4059 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|        - | 4060 | `	 * directive appears anywhere in the list, before validating values. */` |
|      115 | 4061 | `	pCursor = pBodyStart;` |
|      217 | 4062 | `	while( pCursor < pBodyEnd ){` |
|      183 | 4063 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      115 | 4064 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|       81 | 4065 | `				bHasStrictTypes = 1;` |
|       81 | 4066 | `				break;` |
|        - | 4067 | `			}` |
|       17 | 4068 | `		}` |
|      106 | 4069 | `		pCursor++;` |
|        4 | 4070 | `	}` |
|      115 | 4071 | `	if( bHasStrictTypes && bBlockForm ){` |
|        3 | 4072 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 4073 | `			"strict_types declaration must not use block mode");` |
|        3 | 4074 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        3 | 4075 | `		return SXRET_OK;` |
|        - | 4076 | `	}` |
|      113 | 4077 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|        8 | 4078 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 4079 | `			"strict_types declaration must be the very first statement in the script");` |
|        8 | 4080 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        8 | 4081 | `		return SXRET_OK;` |
|        - | 4082 | `	}` |
|        - | 4083 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|      107 | 4084 | `	pCursor = pBodyStart;` |
|      209 | 4085 | `	while( pCursor < pBodyEnd ){` |
|        - | 4086 | `		SyToken *pNameTok;` |
|        - | 4087 | `		SyToken *pEqTok;` |
|        - | 4088 | `		SyToken *pValTok;` |
|        - | 4089 | `		SyString *pDirName;` |
|        - | 4090 | `		int bIsStrict;` |
|        - | 4091 | `		int iStrictValue;` |
|      109 | 4092 | `		pNameTok = pCursor;` |
|      109 | 4093 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 4094 | `			rc = PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        - | 4095 | `				"declare: Expecting a directive name");` |
|      ! 0 | 4096 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 4097 | `			return SXRET_OK;` |
|        - | 4098 | `		}` |
|      109 | 4099 | `		pEqTok = pNameTok + 1;` |
|      109 | 4100 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|      ! 0 | 4101 | `			rc = PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        - | 4102 | `				"declare: Expecting '=' after directive name");` |
|      ! 0 | 4103 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 4104 | `			return SXRET_OK;` |
|        - | 4105 | `		}` |
|      109 | 4106 | `		pValTok = pEqTok + 1;` |
|      109 | 4107 | `		if( pValTok >= pBodyEnd ){` |
|      ! 0 | 4108 | `			rc = PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        - | 4109 | `				"declare: Expecting value after '='");` |
|      ! 0 | 4110 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 4111 | `			return SXRET_OK;` |
|        - | 4112 | `		}` |
|      109 | 4113 | `		pDirName = &pNameTok->sData;` |
|      109 | 4114 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|      109 | 4115 | `		if( bIsStrict ){` |
|        - | 4116 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|        - | 4117 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|       75 | 4118 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      ! 0 | 4119 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 4120 | `					"declare(strict_types) value must be a literal");` |
|      ! 0 | 4121 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 4122 | `				return SXRET_OK;` |
|        - | 4123 | `			}` |
|       75 | 4124 | `			iStrictValue = -1;` |
|       75 | 4125 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|       75 | 4126 | `				const char *zv = SyStringData(&pValTok->sData);` |
|       75 | 4127 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|       75 | 4128 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|       73 | 4129 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|       35 | 4130 | `			}` |
|       75 | 4131 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|        3 | 4132 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 4133 | `					"strict_types declaration must have 0 or 1 as its value");` |
|        3 | 4134 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        3 | 4135 | `				return SXRET_OK;` |
|        - | 4136 | `			}` |
|       73 | 4137 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|       72 | 4138 | `		}else if( DeclareNameIs(pDirName, "encoding", sizeof("encoding")-1) ){` |
|        - | 4139 | `			/* php always ignores declare(encoding=...) unless it was built with` |
|        - | 4140 | `			 * Zend multibyte, and says so in these exact words. */` |
|        - | 4141 | `			/* php's E_COMPILE_WARNING (probe-verified against 8.5). */` |
|        3 | 4142 | `			PH7_GenCompileError(&(*pGen),128 /* E_COMPILE_WARNING */,nLine,` |
|        - | 4143 | `				"declare(encoding=...) ignored because Zend multibyte feature is turned off by settings");` |
|        1 | 4144 | `		}else{` |
|        - | 4145 | `			/* Other directives (ticks and friends) are accepted as no-ops.` |
|        - | 4146 | `			 * This used to emit a NOTICE naming the upstream engine and its` |
|        - | 4147 | `			 * version ("the declare construct is a no-op in the current release` |
|        - | 4148 | `			 * of the PH7(2.1.4) engine") — php prints nothing at all for` |
|        - | 4149 | ``			 * `declare(ticks=1)`, and leaking the old engine's branding into`` |
|        - | 4150 | `			 * user-visible diagnostics was wrong on its own. */` |
|        - | 4151 | `		}` |
|      107 | 4152 | `		pCursor = pValTok + 1;` |
|        - | 4153 | `		/* Consume separating comma (or end). */` |
|      107 | 4154 | `		if( pCursor < pBodyEnd ){` |
|        3 | 4155 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|      ! 0 | 4156 | `				rc = PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        - | 4157 | `					"declare: Expecting ',' or ')' after directive value");` |
|      ! 0 | 4158 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 4159 | `				return SXRET_OK;` |
|        - | 4160 | `			}` |
|        3 | 4161 | `			pCursor++;` |
|        1 | 4162 | `		}` |
|        5 | 4163 | `	}` |
|        - | 4164 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|        - | 4165 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|        - | 4166 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|      105 | 4167 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){` |
|        - | 4168 | `		/* The alternative-syntax body is compiled here, so the whole construct is` |
|        - | 4169 | `		 * one statement (a braced body is left to the caller as a plain block). */` |
|       23 | 4170 | `		return PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDDEC);` |
|        - | 4171 | `	}` |
|       83 | 4172 | `	return SXRET_OK;` |
|        2 | 4173 | `Synchro:` |
|        - | 4174 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|       16 | 4175 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       12 | 4176 | `		pGen->pIn++;` |
|        2 | 4177 | `	}` |
|        6 | 4178 | `	return SXRET_OK;` |
|       62 | 4179 | `}` |
|        - | 4180 | `/*` |
|        - | 4181 | ` * Compile a class constant.` |
|        - | 4182 | ` * According to the PHP language reference manual` |
|        - | 4183 | ` *  Class Constants` |
|        - | 4184 | ` *   It is possible to define constant values on a per-class basis remaining` |
|        - | 4185 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|        - | 4186 | ` *   you don't use the $ symbol to declare or use them.` |
|        - | 4187 | ` *   The value must be a constant expression, not (for example) a variable,` |
|        - | 4188 | ` *   a property, a result of a mathematical operation, or a function call.` |
|        - | 4189 | ` *   It's also possible for interfaces to have constants.` |
|        - | 4190 | ` * Symisc eXtension.` |
|        - | 4191 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|        - | 4192 | ` *  the zend engine would allow only simple scalar value.` |
|        - | 4193 | ` *  Example:` |
|        - | 4194 | ` *   class Test{` |
|        - | 4195 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|        - | 4196 | ` *   };` |
|        - | 4197 | ` *   var_dump(TEST::MyConst);` |
|        - | 4198 | ` *   Refer to the official documentation for more information on the powerful extension` |
|        - | 4199 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|        - | 4200 | ` */` |
|        - | 4201 | `/*` |
|        - | 4202 | ` * Exception handling.` |
|        - | 4203 | ` *  According to the PHP language reference manual` |
|        - | 4204 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|        - | 4205 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|        - | 4206 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|        - | 4207 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|        - | 4208 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|        - | 4209 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|        - | 4210 | ` *    (or re-thrown) within a catch block.` |
|        - | 4211 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|        - | 4212 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|        - | 4213 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|        - | 4214 | ` *    been defined with set_exception_handler().` |
|        - | 4215 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|        - | 4216 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|        - | 4217 | ` */` |
|        - | 4218 | `/*` |
|        - | 4219 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|        - | 4220 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|        - | 4221 | ` * indicates failure.` |
|        - | 4222 | ` */` |
|   110657 | 4223 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 4224 | `{` |
|        - | 4225 | ``	/* php decides throwability at RUNTIME: `throw 5` compiles and raises the`` |
|        - | 4226 | `	 * catchable Error "Can only throw objects" when it executes, and a` |
|        - | 4227 | `	 * non-Throwable object raises "Cannot throw objects that do not implement` |
|        - | 4228 | `	 * Throwable". This used to whitelist a handful of expression shapes and` |
|        - | 4229 | `	 * reject the rest at compile time as a "friendlier" error, which meant the` |
|        - | 4230 | `	 * program never ran and no catch could ever see it — and it was inconsistent` |
|        - | 4231 | ``	 * anyway, since `throw $v` (a variable holding an int) always compiled and`` |
|        - | 4232 | `	 * fell through to the same runtime path. OP_THROW now owns the whole rule,` |
|        - | 4233 | ``	 * so accept any expression here; `throw;` with no operand is still rejected`` |
|        - | 4234 | `	 * by PH7_CompileThrow's SXERR_EMPTY branch. */` |
|    55256 | 4235 | `	SXUNUSED(pGen);` |
|    55256 | 4236 | `	SXUNUSED(pRoot);` |
|   110662 | 4237 | `	return SXRET_OK;` |
|        5 | 4238 | `}` |
|        - | 4239 | `/*` |
|        - | 4240 | ` * Compile a 'throw' statement.` |
|        - | 4241 | ` * throw: This is how you trigger an exception.` |
|        - | 4242 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|        - | 4243 | ` */` |
|   110611 | 4244 | `PH7_PRIVATE sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|        5 | 4245 | `{` |
|   110616 | 4246 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 4247 | `	GenBlock *pBlock;` |
|        - | 4248 | `	sxu32 nIdx;` |
|        - | 4249 | `	sxi32 rc;` |
|   110616 | 4250 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|        - | 4251 | `	/* Compile the expression */` |
|   110616 | 4252 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   110616 | 4253 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 4254 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|      ! 0 | 4255 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4256 | `			return SXERR_ABORT;` |
|        - | 4257 | `		}` |
|      ! 0 | 4258 | `		return SXRET_OK;` |
|        - | 4259 | `	}` |
|   110616 | 4260 | `	pBlock = pGen->pCurrent;` |
|        - | 4261 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   508435 | 4262 | `	while(pBlock->pParent){` |
|   508409 | 4263 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   110590 | 4264 | `			break;` |
|        - | 4265 | `		}` |
|        - | 4266 | `		/* Point to the parent block */` |
|   397824 | 4267 | `		pBlock = pBlock->pParent;` |
|        5 | 4268 | `	}` |
|        - | 4269 | `	/* Emit the throw instruction */` |
|   110616 | 4270 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|        - | 4271 | `	/* Emit the jump */` |
|   110616 | 4272 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   110616 | 4273 | `	return SXRET_OK;` |
|    55238 | 4274 | `}` |
|        - | 4275 | `/*` |
|        - | 4276 | ` * Compile a PHP 8.0 'throw' expression.` |
|        - | 4277 | ` * Called from the expression code generator when a 'throw' keyword is` |
|        - | 4278 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|        - | 4279 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|        - | 4280 | ` * the validator guarantees the operand is a valid exception target.` |
|        - | 4281 | ` */` |
|       46 | 4282 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        5 | 4283 | `{` |
|       51 | 4284 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 4285 | `	GenBlock *pBlock;` |
|        - | 4286 | `	sxu32 nIdx;` |
|        - | 4287 | `	sxi32 rc;` |
|       23 | 4288 | `	(void)iCompileFlag;` |
|       51 | 4289 | `	pGen->pIn++; /* Skip 'throw' */` |
|       51 | 4290 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 4291 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 4292 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 4293 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4294 | `			return SXERR_ABORT;` |
|        - | 4295 | `		}` |
|      ! 0 | 4296 | `		return SXRET_OK;` |
|        - | 4297 | `	}` |
|       51 | 4298 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|       51 | 4299 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 4300 | `		return SXERR_ABORT;` |
|        - | 4301 | `	}` |
|       51 | 4302 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 4303 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 4304 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 4305 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4306 | `			return SXERR_ABORT;` |
|        - | 4307 | `		}` |
|      ! 0 | 4308 | `		return SXRET_OK;` |
|        - | 4309 | `	}` |
|        - | 4310 | `	/* Walk up to nearest exception/function block for the jump target */` |
|       51 | 4311 | `	pBlock = pGen->pCurrent;` |
|       77 | 4312 | `	while( pBlock->pParent ){` |
|       67 | 4313 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|       41 | 4314 | `			break;` |
|        - | 4315 | `		}` |
|       28 | 4316 | `		pBlock = pBlock->pParent;` |
|        2 | 4317 | `	}` |
|       51 | 4318 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       51 | 4319 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|       51 | 4320 | `	return SXRET_OK;` |
|       28 | 4321 | `}` |
|        - | 4322 | `/*` |
|        - | 4323 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|        - | 4324 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|        - | 4325 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|        - | 4326 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|        - | 4327 | ` * compile error propagated from the parser.` |
|        - | 4328 | ` */` |
|       78 | 4329 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|        5 | 4330 | `{` |
|        - | 4331 | `	SyString sClassName;` |
|        - | 4332 | `	SyToken *pToken;` |
|        - | 4333 | `	SyString *pName;` |
|        - | 4334 | `	char *zDup;` |
|        - | 4335 | `	sxi32 rc;` |
|       83 | 4336 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       83 | 4337 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|       83 | 4338 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|        - | 4339 | `	/* Inline catches compile into the function's own container; pByteCode stays NULL. */` |
|       83 | 4340 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 | 4341 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 4342 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 4343 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 4344 | `		return SXERR_INVALID;` |
|        - | 4345 | `	}` |
|       83 | 4346 | `	pGen->pIn++; /* '(' */` |
|       39 | 4347 | `	for(;;){` |
|        - | 4348 | `		SyBlob sResolved;` |
|       83 | 4349 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       83 | 4350 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 4351 | `			SyBlobRelease(&sResolved);` |
|      ! 0 | 4352 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 4353 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 4354 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 4355 | `			return SXERR_INVALID;` |
|        - | 4356 | `		}` |
|      122 | 4357 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       78 | 4358 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       83 | 4359 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|       83 | 4360 | `		SyBlobRelease(&sResolved);` |
|       83 | 4361 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|       83 | 4362 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|       83 | 4363 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       78 | 4364 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|        5 | 4365 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      ! 0 | 4366 | `			pGen->pIn++; continue;` |
|        - | 4367 | `		}` |
|       83 | 4368 | `		break;` |
|      ! 0 | 4369 | `	}` |
|        - | 4370 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|        - | 4371 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding. */` |
|       83 | 4372 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|        3 | 4373 | `		pGen->pIn++; /* ')' */` |
|        3 | 4374 | `		return SXRET_OK;` |
|        - | 4375 | `	}` |
|       76 | 4376 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|       81 | 4377 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 4378 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 4379 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 4380 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 4381 | `		return SXERR_INVALID;` |
|        - | 4382 | `	}` |
|       81 | 4383 | `	pGen->pIn++; /* '$' */` |
|       81 | 4384 | `	pName = &pGen->pIn->sData;` |
|       81 | 4385 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       81 | 4386 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|       81 | 4387 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|       81 | 4388 | `	pGen->pIn++;` |
|       81 | 4389 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 | 4390 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 4391 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 4392 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 4393 | `		return SXERR_INVALID;` |
|        - | 4394 | `	}` |
|       81 | 4395 | `	pGen->pIn++; /* ')' */` |
|       81 | 4396 | `	return SXRET_OK;` |
|       44 | 4397 | `}` |
|        - | 4398 | `/*` |
|        - | 4399 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|        - | 4400 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|        - | 4401 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|        - | 4402 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|        - | 4403 | ` * VmThrowException):` |
|        - | 4404 | ` *` |
|        - | 4405 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|        - | 4406 | ` *    <try body>` |
|        - | 4407 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|        - | 4408 | ` *    JMP  -> finally\|end` |
|        - | 4409 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|        - | 4410 | ` *    <catch body>` |
|        - | 4411 | ` *    JMP  -> finally\|end` |
|        - | 4412 | ` *    ... more catches ...` |
|        - | 4413 | ` *  Lfin: <finally body>` |
|        - | 4414 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|        - | 4415 | ` *  Lend:` |
|        - | 4416 | ` */` |
|      138 | 4417 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|        5 | 4418 | `{` |
|      143 | 4419 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 4420 | `	GenBlock *pTry;` |
|        - | 4421 | `	VmInstr *pInstr;` |
|      143 | 4422 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|        - | 4423 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|        - | 4424 | `	sxi32 rc;` |
|      143 | 4425 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|        - | 4426 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION; passed at` |
|        - | 4427 | `	 * ENTRY so GenStateEnterBlock can classify the scope with it) */` |
|      212 | 4428 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),` |
|       69 | 4429 | `		pException,&pTry);` |
|      143 | 4430 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      143 | 4431 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|      143 | 4432 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|      143 | 4433 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      143 | 4434 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      143 | 4435 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|      143 | 4436 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|        - | 4437 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|      143 | 4438 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|      143 | 4439 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|      143 | 4440 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|      143 | 4441 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 4442 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|      143 | 4443 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|        - | 4444 | `	/* Catch clauses (inline) */` |
|      143 | 4445 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      138 | 4446 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       83 | 4447 | `		sxu32 k = 0;` |
|      117 | 4448 | `		for(;;){` |
|        - | 4449 | `			ph7_exception_block sCatch;` |
|        - | 4450 | `			GenBlock *pCatchBlk;` |
|      161 | 4451 | `			sxu32 idxJmp = 0;` |
|      156 | 4452 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      147 | 4453 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|       44 | 4454 | `				break;` |
|        - | 4455 | `			}` |
|       83 | 4456 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|       83 | 4457 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       83 | 4458 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|       83 | 4459 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|       83 | 4460 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|        - | 4461 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|        - | 4462 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|        - | 4463 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump).` |
|        - | 4464 | `			 * Passed at ENTRY: GenStateEnterBlock reads it to classify the block's scope. */` |
|      122 | 4465 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),` |
|       39 | 4466 | `				pException,&pCatchBlk);` |
|       83 | 4467 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       83 | 4468 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|       83 | 4469 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       83 | 4470 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       83 | 4471 | `			GenStateLeaveBlock(&(*pGen),0);` |
|        - | 4472 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|        - | 4473 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|       83 | 4474 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       83 | 4475 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|       83 | 4476 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|       83 | 4477 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|       83 | 4478 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       83 | 4479 | `			k++;` |
|        5 | 4480 | `		}` |
|       39 | 4481 | `	}` |
|        - | 4482 | `	/* Finally (inline) */` |
|      143 | 4483 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      110 | 4484 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 4485 | `		GenBlock *pFinBlk;` |
|       71 | 4486 | `		pGen->pIn++; /* Jump 'finally' */` |
|       71 | 4487 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|      104 | 4488 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FINALLY,` |
|       33 | 4489 | `			PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|       71 | 4490 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       71 | 4491 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       71 | 4492 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       71 | 4493 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       71 | 4494 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       71 | 4495 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|       71 | 4496 | `		pException->iHasFinally = 1;` |
|       33 | 4497 | `	}` |
|      143 | 4498 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|      143 | 4499 | `	pException->iInlined = 1;` |
|        - | 4500 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|        - | 4501 | `	{` |
|      143 | 4502 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|        - | 4503 | `		sxu32 *aJ; sxu32 n;` |
|      143 | 4504 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|      143 | 4505 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|      143 | 4506 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|      221 | 4507 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|       83 | 4508 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|       83 | 4509 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|       44 | 4510 | `		}` |
|        - | 4511 | `	}` |
|      143 | 4512 | `	SySetRelease(&aCatchJmp);` |
|      143 | 4513 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|      ! 0 | 4514 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|      ! 0 | 4515 | `	}` |
|      143 | 4516 | `	return SXRET_OK;` |
|       74 | 4517 | `}` |
|        - | 4518 | `/*` |
|        - | 4519 | ` * Compile a 'catch' block.` |
|        - | 4520 | ` * Catch: A "catch" block retrieves an exception and creates` |
|        - | 4521 | ` * an object containing the exception information.` |
|        - | 4522 | ` */` |
|     5388 | 4523 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|        5 | 4524 | `{` |
|     5393 | 4525 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 4526 | `	ph7_exception_block sCatch;` |
|        - | 4527 | `	SySet *pInstrContainer;` |
|        - | 4528 | `	SyString sClassName;` |
|        - | 4529 | `	GenBlock *pCatch;` |
|        - | 4530 | `	SyToken *pToken;` |
|        - | 4531 | `	SyString *pName;` |
|        - | 4532 | `	char *zDup;` |
|        - | 4533 | `	sxi32 rc;` |
|     5393 | 4534 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|        - | 4535 | `	/* Zero the structure */` |
|     5393 | 4536 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|        - | 4537 | `	/* Initialize fields */` |
|     5393 | 4538 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|        - | 4539 | `	/* The catch body gets its own bytecode array, allocated (not embedded) so its address` |
|        - | 4540 | `	 * survives both this stack frame and any later growth of pException->sEntry — a` |
|        - | 4541 | `	 * break/continue inside the body records it in its JumpFixup (see JumpFixup). */` |
|     5393 | 4542 | `	sCatch.pByteCode = (SySet *)SyMemBackendAlloc(&pException->pVm->sAllocator,sizeof(SySet));` |
|     5393 | 4543 | `	if( sCatch.pByteCode == 0 ){` |
|      ! 0 | 4544 | `		goto Mem;` |
|        - | 4545 | `	}` |
|     5393 | 4546 | `	SySetInit(sCatch.pByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     5393 | 4547 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|        - | 4548 | `			/* Unexpected token,break immediately */` |
|      ! 0 | 4549 | `			pToken = pGen->pIn;` |
|      ! 0 | 4550 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 4551 | `				pToken--;` |
|      ! 0 | 4552 | `			}` |
|      ! 0 | 4553 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 4554 | `				"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 4555 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 4556 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 4557 | `				return SXERR_ABORT;` |
|        - | 4558 | `			}` |
|      ! 0 | 4559 | `			return SXERR_INVALID;` |
|        - | 4560 | `	}` |
|        - | 4561 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     5393 | 4562 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     2707 | 4563 | `	for(;;){` |
|        - | 4564 | `		SyBlob sResolved;` |
|     5427 | 4565 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     5427 | 4566 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|        6 | 4567 | `			SyBlobRelease(&sResolved);` |
|        6 | 4568 | `			pToken = pGen->pIn;` |
|        6 | 4569 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 4570 | `				pToken--;` |
|      ! 0 | 4571 | `			}` |
|        8 | 4572 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 4573 | `				"syntax error, unexpected %s \"%z\"",` |
|        2 | 4574 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|        6 | 4575 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 4576 | `				return SXERR_ABORT;` |
|        - | 4577 | `			}` |
|        6 | 4578 | `			return SXERR_INVALID;` |
|        - | 4579 | `		}` |
|        - | 4580 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|        - | 4581 | `		 * transient SyBlob allocation. */` |
|     8128 | 4582 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     5418 | 4583 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     5423 | 4584 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     5423 | 4585 | `		SyBlobRelease(&sResolved);` |
|     5423 | 4586 | `		if( zDup == 0 ){` |
|      ! 0 | 4587 | `			goto Mem;` |
|        - | 4588 | `		}` |
|     5423 | 4589 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     5423 | 4590 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 4591 | `			goto Mem;` |
|        - | 4592 | `		}` |
|        - | 4593 | `		/* Check for '\|' (multi-catch separator) */` |
|     5418 | 4594 | `		if( pGen->pIn < pGen->pEnd &&` |
|     5418 | 4595 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|       39 | 4596 | `			pGen->pIn->sData.nByte == 1 &&` |
|       34 | 4597 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|       38 | 4598 | `			pGen->pIn++; /* Consume the '\|' */` |
|       38 | 4599 | `			continue;` |
|        - | 4600 | `		}` |
|     5389 | 4601 | `		break;` |
|      ! 0 | 4602 | `	}` |
|        - | 4603 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|        - | 4604 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding;` |
|        - | 4605 | `	 * jump straight to compiling the block below. */` |
|     5389 | 4606 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) /*)*/ ){` |
|        7 | 4607 | `		goto CatchBody;` |
|        - | 4608 | `	}` |
|     5378 | 4609 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     5383 | 4610 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - | 4611 | `			/* Unexpected token,break immediately */` |
|      ! 0 | 4612 | `			pToken = pGen->pIn;` |
|      ! 0 | 4613 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 4614 | `				pToken--;` |
|      ! 0 | 4615 | `			}` |
|      ! 0 | 4616 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 4617 | `				"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 4618 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 4619 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 4620 | `				return SXERR_ABORT;` |
|        - | 4621 | `			}` |
|      ! 0 | 4622 | `			return SXERR_INVALID;` |
|        - | 4623 | `	}` |
|     5383 | 4624 | `	pGen->pIn++; /* Jump the dollar sign */` |
|        - | 4625 | `	/* Duplicate instance name */` |
|     5383 | 4626 | `	pName = &pGen->pIn->sData;` |
|     5383 | 4627 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     5383 | 4628 | `	if( zDup == 0 ){` |
|      ! 0 | 4629 | `		goto Mem;` |
|        - | 4630 | `	}` |
|     5383 | 4631 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     5383 | 4632 | `	pGen->pIn++;` |
|     2696 | 4633 | `CatchBody:` |
|     5389 | 4634 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|        - | 4635 | `		/* Unexpected token,break immediately */` |
|      ! 0 | 4636 | `		pToken = pGen->pIn;` |
|      ! 0 | 4637 | `		if( pToken >= pGen->pEnd ){` |
|      ! 0 | 4638 | `			pToken--;` |
|      ! 0 | 4639 | `		}` |
|      ! 0 | 4640 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 4641 | `			"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 4642 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 4643 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4644 | `			return SXERR_ABORT;` |
|        - | 4645 | `		}` |
|      ! 0 | 4646 | `		return SXERR_INVALID;` |
|        - | 4647 | `	}` |
|        - | 4648 | `	/* Compile the block */` |
|     5389 | 4649 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|        - | 4650 | `	/* Create the catch block. GEN_BLOCK_DETACHED: the body below compiles into` |
|        - | 4651 | `	 * sCatch.pByteCode, not into the enclosing function's array. */` |
|     5389 | 4652 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION\|GEN_BLOCK_DETACHED,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     5389 | 4653 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 4654 | `		return SXERR_ABORT;` |
|        - | 4655 | `	}` |
|        - | 4656 | `	/* Swap bytecode container */` |
|     5389 | 4657 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     5389 | 4658 | `	PH7_VmSetByteCodeContainer(pGen->pVm,sCatch.pByteCode);` |
|        - | 4659 | `	/* Compile the block */` |
|     5389 | 4660 | `	PH7_CompileBlock(&(*pGen),0);` |
|        - | 4661 | `	/* Fix forward jumps now the destination is resolved  */` |
|     5389 | 4662 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 4663 | `	/* Emit the DONE instruction */` |
|     5389 | 4664 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 4665 | `	/* Leave the block */` |
|     5389 | 4666 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 4667 | `	/* Restore the default container */` |
|     5389 | 4668 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 4669 | `	/* Install the catch block */` |
|     5389 | 4670 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     5389 | 4671 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 4672 | `		goto Mem;` |
|        - | 4673 | `	}` |
|     5389 | 4674 | `	return SXRET_OK;` |
|      ! 0 | 4675 | `Mem:` |
|      ! 0 | 4676 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 4677 | `	return SXERR_ABORT;` |
|     2695 | 4678 | `}` |
|        - | 4679 | `/*` |
|        - | 4680 | ` * Compile a 'try' block.` |
|        - | 4681 | ` * A function using an exception should be in a "try" block.` |
|        - | 4682 | ` * If the exception does not trigger, the code will continue` |
|        - | 4683 | ` * as normal. However if the exception triggers, an exception` |
|        - | 4684 | ` * is "thrown".` |
|        - | 4685 | ` */` |
|     5704 | 4686 | `PH7_PRIVATE sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|        5 | 4687 | `{` |
|        - | 4688 | `	ph7_exception *pException;` |
|     5709 | 4689 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 4690 | `	GenBlock *pTry;` |
|        - | 4691 | `	sxu32 nJmpIdx;` |
|        - | 4692 | `	sxi32 rc;` |
|        - | 4693 | `	/* Create the exception container */` |
|     5709 | 4694 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     5709 | 4695 | `	if( pException == 0 ){` |
|      ! 0 | 4696 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|      ! 0 | 4697 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 4698 | `		return SXERR_ABORT;` |
|        - | 4699 | `	}` |
|        - | 4700 | `	/* Zero the structure */` |
|     5709 | 4701 | `	SyZero(pException,sizeof(ph7_exception));` |
|        - | 4702 | `	/* Initialize fields */` |
|     5709 | 4703 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     5709 | 4704 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     5709 | 4705 | `	pException->iHasFinally = 0;` |
|     5709 | 4706 | `	pException->iFinallyDone = 0;` |
|     5709 | 4707 | `	pException->pVm = pGen->pVm;` |
|        - | 4708 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|        - | 4709 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the DETACHED path`` |
|        - | 4710 | `	 * below — deliberately, not pending migration: it is the proven one, and inlining was` |
|        - | 4711 | ``	 * scoped to generators so no other code path changed. `bInlineTryCatch` is 1 since the`` |
|        - | 4712 | ``	 * inline VM handlers landed, so `bInGenerator` is what actually selects here. */`` |
|     5709 | 4713 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|      143 | 4714 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|        - | 4715 | `	}` |
|        - | 4716 | `	/* Create the try block */` |
|        - | 4717 | `	/* pUserData is the exception context, passed at ENTRY (not assigned after) because` |
|        - | 4718 | `	 * GenStateEnterBlock reads it to classify the block's try/catch scope — see aScope.` |
|        - | 4719 | `	 * It is also what a break/continue crossing this try emits its POP_EXCEPTION with. */` |
|     8350 | 4720 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),` |
|     2779 | 4721 | `		pException,&pTry);` |
|     5571 | 4722 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 4723 | `		return SXERR_ABORT;` |
|        - | 4724 | `	}` |
|        - | 4725 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     5571 | 4726 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|        - | 4727 | `	/* Fix the jump later when the destination is resolved */` |
|     5571 | 4728 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     5571 | 4729 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|        - | 4730 | `	/* Compile the block */` |
|     5571 | 4731 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     5571 | 4732 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 4733 | `		return SXERR_ABORT;` |
|        - | 4734 | `	}` |
|        - | 4735 | `	/* Fix forward jumps now the destination is resolved */` |
|     5571 | 4736 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 4737 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     5571 | 4738 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|        - | 4739 | `	/* Leave the block */` |
|     5571 | 4740 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 4741 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     5571 | 4742 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     5564 | 4743 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|        - | 4744 | `		/* Compile one or more catch blocks */` |
|     5375 | 4745 | `		for(;;){` |
|    10766 | 4746 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     8900 | 4747 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     2690 | 4748 | `					break;` |
|        - | 4749 | `			}` |
|     5393 | 4750 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     5393 | 4751 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 4752 | `				return SXERR_ABORT;` |
|        - | 4753 | `			}` |
|        5 | 4754 | `		}` |
|     2685 | 4755 | `	}` |
|        - | 4756 | `	/* Compile optional finally block */` |
|     5571 | 4757 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     2618 | 4758 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 4759 | `		SySet *pInstrContainer;` |
|        - | 4760 | `		GenBlock *pFinBlock;` |
|      279 | 4761 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|        - | 4762 | `		/* Create the finally block for jump fixup bookkeeping (detached: the body` |
|        - | 4763 | `		 * compiles into pException->sFinally, see GEN_BLOCK_DETACHED). */` |
|      416 | 4764 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION\|GEN_BLOCK_DETACHED\|GEN_BLOCK_FINALLY,` |
|      137 | 4765 | `			PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      279 | 4766 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 4767 | `			return SXERR_ABORT;` |
|        - | 4768 | `		}` |
|        - | 4769 | `		/* Swap bytecode container */` |
|      279 | 4770 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      279 | 4771 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|        - | 4772 | `		/* Compile the finally body */` |
|      279 | 4773 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      279 | 4774 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4775 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 | 4776 | `			return SXERR_ABORT;` |
|        - | 4777 | `		}` |
|        - | 4778 | `		/* Fix forward jumps now the destination is resolved */` |
|      279 | 4779 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 4780 | `		/* Emit DONE to terminate the finally block */` |
|      279 | 4781 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 4782 | `		/* Leave the block */` |
|      279 | 4783 | `		GenStateLeaveBlock(&(*pGen),0);` |
|        - | 4784 | `		/* Restore the default container */` |
|      279 | 4785 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      279 | 4786 | `		pException->iHasFinally = 1;` |
|      137 | 4787 | `	}` |
|        - | 4788 | `	/* Must have at least one catch or finally */` |
|     5571 | 4789 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|        8 | 4790 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 4791 | `			"Cannot use try without catch or finally");` |
|        8 | 4792 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4793 | `			return SXERR_ABORT;` |
|        - | 4794 | `		}` |
|        3 | 4795 | `	}` |
|     5571 | 4796 | `	return SXRET_OK;` |
|     2853 | 4797 | `}` |
|        - | 4798 | `/*` |
|        - | 4799 | ` * Compile a switch block.` |
|        - | 4800 | ` *  (See block-comment below for more information)` |
|        - | 4801 | ` */` |
|      316 | 4802 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 *pBlockStart)` |
|        5 | 4803 | `{` |
|      321 | 4804 | `	sxi32 rc = SXRET_OK;` |
|      321 | 4805 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|        - | 4806 | `		/* Unexpected token */` |
|      ! 0 | 4807 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 | 4808 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4809 | `			return SXERR_ABORT;` |
|        - | 4810 | `		}` |
|      ! 0 | 4811 | `		pGen->pIn++;` |
|      ! 0 | 4812 | `	}` |
|      321 | 4813 | `	pGen->pIn++;` |
|        - | 4814 | `	/* First instruction to execute in this block. */` |
|      321 | 4815 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|        - | 4816 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|        - | 4817 | `	 * or the '}' token */` |
|      454 | 4818 | `	for(;;){` |
|      929 | 4819 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 4820 | ``			/* A `?>` ended the chunk -- php reads it as a `;` -- and the case body`` |
|        - | 4821 | `			 * goes on in the next one, the text between them included. At the end` |
|        - | 4822 | `			 * of the input the caller reports the switch left open. */` |
|       23 | 4823 | `			rc = GenStateNextChunk(&(*pGen));` |
|       23 | 4824 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 4825 | `				return SXERR_ABORT;` |
|        - | 4826 | `			}` |
|       23 | 4827 | `			if( rc == SXERR_EOF ){` |
|        7 | 4828 | `				break;` |
|        - | 4829 | `			}` |
|       17 | 4830 | `			continue;` |
|        - | 4831 | `		}` |
|      907 | 4832 | `		rc = SXRET_OK;` |
|      907 | 4833 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      225 | 4834 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|        - | 4835 | `				/* The end of the case list either way: the caller judges whether it` |
|        - | 4836 | `				 * is the right one. */` |
|      111 | 4837 | `				break;` |
|        - | 4838 | `			}` |
|       62 | 4839 | `		}else{` |
|        - | 4840 | `			sxi32 nKwrd;` |
|        - | 4841 | `			/* Extract the keyword */` |
|      687 | 4842 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      687 | 4843 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT \|\| nKwrd == PH7_TKWRD_ENDSWITCH ){` |
|      107 | 4844 | `				break;` |
|        - | 4845 | `			}` |
|        - | 4846 | `		}` |
|        - | 4847 | `		/* Compile block */` |
|      597 | 4848 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      597 | 4849 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4850 | `			return SXERR_ABORT;` |
|        - | 4851 | `		}` |
|        5 | 4852 | `	}` |
|      321 | 4853 | `	return SXRET_OK;` |
|      163 | 4854 | `}` |
|        - | 4855 | `/*` |
|        - | 4856 | ` * Compile a case eXpression.` |
|        - | 4857 | ` *  (See block-comment below for more information)` |
|        - | 4858 | ` */` |
|      254 | 4859 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|        5 | 4860 | `{` |
|        - | 4861 | `	SySet *pInstrContainer;` |
|        - | 4862 | `	SyToken *pEnd,*pTmp;` |
|      259 | 4863 | `	sxi32 iNest = 0;` |
|      259 | 4864 | ``	sxi32 iQuesty = 0;  /* `?`s opened in the case expression and still unclosed */`` |
|        - | 4865 | `	sxi32 rc;` |
|        - | 4866 | ``	/* Delimit the expression. The `:` that ends a case label is the one standing`` |
|        - | 4867 | `	 * outside every paren AND outside every open ternary: php reads the whole` |
|        - | 4868 | `` 	 * expression first, so `case \PHP_VERSION_ID < 80100 ? \T_CLASS : \T_ENUM:` `` |
|        - | 4869 | `	 * is one label with three colons' worth of punctuation in it. Stopping at the` |
|        - | 4870 | `` 	 * first colon cut that label at the ternary's, and the leftover `\T_ENUM:` `` |
|        - | 4871 | ``	 * came back as `syntax error, unexpected token ":"` -- it is how nette/utils`` |
|        - | 4872 | `	 * spells its token switch, so no phpstan run got past its own bootstrap. A` |
|        - | 4873 | ``	 * `?:` closes itself here (its two tokens are adjacent), and a named`` |
|        - | 4874 | ``	 * argument's `:` sits at iNest >= 1 where neither test can see it. */`` |
|      259 | 4875 | `	pEnd = pGen->pIn;` |
|      619 | 4876 | `	while( pEnd < pGen->pEnd ){` |
|      619 | 4877 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|        - | 4878 | `			/* Increment nesting level */` |
|       16 | 4879 | `			iNest++;` |
|      612 | 4880 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|        - | 4881 | `			/* Decrement nesting level */` |
|       16 | 4882 | `			iNest--;` |
|      595 | 4883 | `		}else if( iNest < 1 && (pEnd->nType & PH7_TK_OP)` |
|      286 | 4884 | `			&& pEnd->sData.nByte == 1 && pEnd->sData.zString[0] == '?' ){` |
|        - | 4885 | ``			/* `??` and `?->` are tokens of their own, so this cannot see them */`` |
|       11 | 4886 | `			iQuesty++;` |
|      586 | 4887 | `		}else if( (pEnd->nType & PH7_TK_COLON) && iNest < 1 ){` |
|      269 | 4888 | `			if( iQuesty < 1 ){` |
|      259 | 4889 | `				break;` |
|        - | 4890 | `			}` |
|       11 | 4891 | `			iQuesty--;` |
|      322 | 4892 | `		}else if( (pEnd->nType & PH7_TK_SEMI/*';'*/) && iNest < 1 ){` |
|      ! 0 | 4893 | `			break;` |
|        - | 4894 | `		}` |
|      365 | 4895 | `		pEnd++;` |
|        5 | 4896 | `	}` |
|      259 | 4897 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 4898 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|      ! 0 | 4899 | `		if( rc == SXERR_ABORT ){` |
|        - | 4900 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 4901 | `			return SXERR_ABORT;` |
|        - | 4902 | `		}` |
|      ! 0 | 4903 | `	}` |
|        - | 4904 | `	/* Swap token stream */` |
|      259 | 4905 | `	pTmp = pGen->pEnd;` |
|      259 | 4906 | `	pGen->pEnd = pEnd;` |
|      259 | 4907 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      259 | 4908 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      259 | 4909 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 4910 | `	/* Emit the done instruction */` |
|      259 | 4911 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      259 | 4912 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 4913 | `	/* Update token stream */` |
|      259 | 4914 | `	pGen->pIn  = pEnd;` |
|      259 | 4915 | `	pGen->pEnd = pTmp;` |
|      259 | 4916 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 4917 | `		return SXERR_ABORT;` |
|        - | 4918 | `	}` |
|      259 | 4919 | `	return SXRET_OK;` |
|      132 | 4920 | `}` |
|        - | 4921 | `/*` |
|        - | 4922 | ` * Compile the smart switch statement.` |
|        - | 4923 | ` * According to the PHP language reference manual` |
|        - | 4924 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|        - | 4925 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|        - | 4926 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|        - | 4927 | ` *  This is exactly what the switch statement is for.` |
|        - | 4928 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|        - | 4929 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|        - | 4930 | ` *  of the outer loop, use continue 2.` |
|        - | 4931 | ` *  Note that switch/case does loose comparision.` |
|        - | 4932 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|        - | 4933 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|        - | 4934 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|        - | 4935 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|        - | 4936 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|        - | 4937 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|        - | 4938 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|        - | 4939 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|        - | 4940 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|        - | 4941 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|        - | 4942 | ` *  list for the next case.` |
|        - | 4943 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|        - | 4944 | ` *  or floating-point numbers and strings.` |
|        - | 4945 | ` */` |
|      224 | 4946 | `PH7_PRIVATE sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|        5 | 4947 | `{` |
|        - | 4948 | `	GenBlock *pSwitchBlock;` |
|        - | 4949 | `	SyToken *pTmp,*pEnd;` |
|        - | 4950 | `	ph7_switch *pSwitch;` |
|        - | 4951 | `	sxu32 nToken;` |
|        - | 4952 | `	sxu32 nOpenLine;` |
|        - | 4953 | ``	int bHead;  /* No case seen yet: php's case list may open with ONE `;` */`` |
|        - | 4954 | `	int bSemi;  /* ...and this one has */` |
|        - | 4955 | `	sxi32 rc;` |
|        - | 4956 | `	/* Jump the 'switch' keyword */` |
|      229 | 4957 | `	pGen->pIn++;` |
|      229 | 4958 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 4959 | `		/* php's parse error names the token it found instead */` |
|       17 | 4960 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|       17 | 4961 | `		if( rc == SXERR_ABORT ){` |
|        - | 4962 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 4963 | `			return SXERR_ABORT;` |
|        - | 4964 | `		}` |
|       17 | 4965 | `		goto Synchronize;` |
|        - | 4966 | `	}` |
|        - | 4967 | `	/* Jump the left parenthesis '(' */` |
|      213 | 4968 | `	pGen->pIn++;` |
|      213 | 4969 | `	pEnd = 0; /* cc warning */` |
|        - | 4970 | `	/* Create the loop block */` |
|      317 | 4971 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      104 | 4972 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      213 | 4973 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 4974 | `		return SXERR_ABORT;` |
|        - | 4975 | `	}` |
|        - | 4976 | `	/* Delimit the condition */` |
|      213 | 4977 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      213 | 4978 | `	if( pGen->pIn == pEnd && pEnd < pGen->pEnd ){` |
|        - | 4979 | ``		/* `switch ()`: php names the ')' */`` |
|        5 | 4980 | `		rc = PH7_GenSyntaxError(&(*pGen),pEnd,0);` |
|        5 | 4981 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4982 | `			return SXERR_ABORT;` |
|        1 | 4983 | `		}` |
|      211 | 4984 | `	}else if( pEnd >= pGen->pEnd ){` |
|       21 | 4985 | `		rc = GenStateUnclosedHead(&(*pGen),&pGen->pIn[-1],PHL_HEAD_COND);` |
|       21 | 4986 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 4987 | `			return SXERR_ABORT;` |
|        - | 4988 | `		}` |
|       21 | 4989 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       21 | 4990 | `		goto Synchronize;` |
|        - | 4991 | `	}` |
|        - | 4992 | `	/* Swap token streams */` |
|      193 | 4993 | `	pTmp = pGen->pEnd;` |
|      193 | 4994 | `	pGen->pEnd = pEnd;` |
|        - | 4995 | `	/* Compile the expression */` |
|      193 | 4996 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      193 | 4997 | `	if( rc == SXERR_ABORT ){` |
|        - | 4998 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 4999 | `		return SXERR_ABORT;` |
|        - | 5000 | `	}` |
|        - | 5001 | `	/* Update token stream */` |
|      193 | 5002 | `	if( pGen->pIn < pEnd ){` |
|        - | 5003 | `		/* A token the condition left over: php's parse error names it */` |
|      ! 0 | 5004 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 | 5005 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 5006 | `			return SXERR_ABORT;` |
|        - | 5007 | `		}` |
|      ! 0 | 5008 | `	}` |
|      193 | 5009 | `	pGen->pIn  = &pEnd[1];` |
|      193 | 5010 | `	pGen->pEnd = pTmp;` |
|      193 | 5011 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|        9 | 5012 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\":\" or \"{\"");` |
|        9 | 5013 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 5014 | `			return SXERR_ABORT;` |
|        - | 5015 | `		}` |
|        9 | 5016 | `		goto Synchronize;` |
|        - | 5017 | `	}` |
|      185 | 5018 | `	nOpenLine = pGen->pIn->nLine;` |
|        - | 5019 | `	/* Set the delimiter token */` |
|      185 | 5020 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       48 | 5021 | `		nToken = PH7_TK_KEYWORD;` |
|        - | 5022 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       48 | 5023 | `		pGen->pCurrent->zInnerTail = "\"endswitch\" or \"case\" or \"default\"";` |
|       25 | 5024 | `	}else{` |
|      139 | 5025 | `		nToken = PH7_TK_CCB; /* '}' */` |
|      139 | 5026 | `		pGen->pCurrent->zInnerTail = "\"case\" or \"default\" or \"}\"";` |
|        - | 5027 | `	}` |
|      185 | 5028 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|        - | 5029 | `	/* Create the switch blocks container */` |
|      185 | 5030 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      185 | 5031 | `	if( pSwitch == 0 ){` |
|        - | 5032 | `		/* Abort compilation */` |
|      ! 0 | 5033 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 5034 | `		return SXERR_ABORT;` |
|        - | 5035 | `	}` |
|        - | 5036 | `	/* Zero the structure */` |
|      185 | 5037 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|        - | 5038 | `	/* Initialize fields */` |
|      185 | 5039 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|        - | 5040 | `	/* Emit the switch instruction */` |
|      185 | 5041 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|        - | 5042 | `	/* Compile case blocks */` |
|      185 | 5043 | `	bHead = 1;` |
|      185 | 5044 | `	bSemi = 0;` |
|      413 | 5045 | `	for(;;){` |
|        - | 5046 | `		sxu32 nKwrd;` |
|      515 | 5047 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       21 | 5048 | `			if( bHead ){` |
|        - | 5049 | ``				/* A `?>` before the first case is php's `;` there -- the one its`` |
|        - | 5050 | `				 * grammar takes -- and any text after it is a token it has no` |
|        - | 5051 | `				 * place for. */` |
|       15 | 5052 | `				if( bSemi ){` |
|        4 | 5053 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pEnd[-1].nLine,` |
|        2 | 5054 | `						"syntax error, unexpected token \";\", expecting %s",pGen->pCurrent->zInnerTail);` |
|        3 | 5055 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 5056 | `						return SXERR_ABORT;` |
|        - | 5057 | `					}` |
|        3 | 5058 | `					break;` |
|        - | 5059 | `				}` |
|       12 | 5060 | `				if( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP` |
|        9 | 5061 | `				 && pGen->pRawIn->sData.nByte > 0 ){` |
|        7 | 5062 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pRawIn->nLine,` |
|        - | 5063 | `						"syntax error, unexpected T_INLINE_HTML \"%z\", expecting %s",` |
|        4 | 5064 | `						&pGen->pRawIn->sData,pGen->pCurrent->zInnerTail);` |
|        5 | 5065 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 5066 | `						return SXERR_ABORT;` |
|        - | 5067 | `					}` |
|        5 | 5068 | `					break;` |
|        - | 5069 | `				}` |
|        9 | 5070 | `				bSemi = 1;` |
|        4 | 5071 | `			}` |
|        - | 5072 | `			/* The case list goes on in the next PHP chunk */` |
|       15 | 5073 | `			rc = GenStateNextChunk(&(*pGen));` |
|       15 | 5074 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 5075 | `				return SXERR_ABORT;` |
|        - | 5076 | `			}` |
|       15 | 5077 | `			if( rc == SXERR_EOF ){` |
|        - | 5078 | `				/* The input ended inside the switch */` |
|       11 | 5079 | `				if( nToken == PH7_TK_CCB ){` |
|        7 | 5080 | `					rc = GenStateUnclosedBrace(&(*pGen),nOpenLine);` |
|        4 | 5081 | `				}else{` |
|        7 | 5082 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,` |
|        4 | 5083 | `						pGen->nChunkEofLine > nOpenLine ? pGen->nChunkEofLine : nOpenLine,` |
|        4 | 5084 | `						"syntax error, unexpected end of file, expecting %s",pGen->pCurrent->zInnerTail);` |
|        - | 5085 | `				}` |
|       11 | 5086 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 5087 | `					return SXERR_ABORT;` |
|        - | 5088 | `				}` |
|       11 | 5089 | `				break;` |
|        - | 5090 | `			}` |
|        5 | 5091 | `			continue;` |
|        - | 5092 | `		}` |
|      495 | 5093 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      139 | 5094 | `			if( bHead && !bSemi && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       11 | 5095 | `				bSemi = 1;` |
|       11 | 5096 | `				pGen->pIn++;` |
|       11 | 5097 | `				continue;` |
|        - | 5098 | `			}` |
|      129 | 5099 | `			if( (pGen->pIn->nType & PH7_TK_CCB /*}*/) && nToken == PH7_TK_CCB ){` |
|        - | 5100 | `				/* Block compiled */` |
|      111 | 5101 | `				break;` |
|        - | 5102 | `			}` |
|       19 | 5103 | `			if( pGen->pIn->nType & PH7_TK_CCB ){` |
|        - | 5104 | ``				/* A `}` in an endswitch list. php's scanner refuses it as unmatched`` |
|        - | 5105 | ``				 * when no `{` is open around the switch; otherwise its parser`` |
|        - | 5106 | `				 * wanted the list to go on. */` |
|        7 | 5107 | `				SyToken *pTok = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        7 | 5108 | `				sxi32 nOpen = 0;` |
|       59 | 5109 | `				for( ; pTok < pGen->pIn ; pTok++ ){` |
|       53 | 5110 | `					if( pTok->nType & PH7_TK_OCB ){` |
|        3 | 5111 | `						nOpen++;` |
|       52 | 5112 | `					}else if( pTok->nType & PH7_TK_CCB ){` |
|      ! 0 | 5113 | `						nOpen--;` |
|      ! 0 | 5114 | `					}` |
|       27 | 5115 | `				}` |
|        7 | 5116 | `				if( nOpen <= 0 ){` |
|        5 | 5117 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,"Unmatched '}'");` |
|        5 | 5118 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 5119 | `						return SXERR_ABORT;` |
|        - | 5120 | `					}` |
|        5 | 5121 | `					break;` |
|        - | 5122 | `				}` |
|        1 | 5123 | `			}` |
|        - | 5124 | `			/* php's parse error, naming what the case list wanted */` |
|       15 | 5125 | `			rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,pGen->pCurrent->zInnerTail);` |
|       15 | 5126 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 5127 | `				return SXERR_ABORT;` |
|        - | 5128 | `			}` |
|       15 | 5129 | `			break;` |
|        - | 5130 | `		}` |
|        - | 5131 | `		/* Extract the keyword */` |
|      361 | 5132 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      361 | 5133 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       32 | 5134 | `			if( nToken != PH7_TK_KEYWORD ){` |
|        5 | 5135 | `				rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,pGen->pCurrent->zInnerTail);` |
|        5 | 5136 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 5137 | `					return SXERR_ABORT;` |
|        - | 5138 | `				}` |
|        2 | 5139 | `			}` |
|        - | 5140 | `			/* Block compiled */` |
|       32 | 5141 | `			break;` |
|        - | 5142 | `		}` |
|      331 | 5143 | `		bHead = 0;` |
|      331 | 5144 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|        - | 5145 | `			/*` |
|        - | 5146 | `			 * Accroding to the PHP language reference manual` |
|        - | 5147 | `			 *  A special case is the default case. This case matches anything` |
|        - | 5148 | `			 *  that wasn't matched by the other cases.` |
|        - | 5149 | `			 */` |
|       67 | 5150 | `			if( pSwitch->nDefault > 0 ){` |
|        - | 5151 | `				/* php refuses a second default when it compiles the switch */` |
|        3 | 5152 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 5153 | `					"Switch statements may only contain one default clause");` |
|        3 | 5154 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 5155 | `					return SXERR_ABORT;` |
|        - | 5156 | `				}` |
|        1 | 5157 | `			}` |
|       67 | 5158 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|        - | 5159 | `			/* Compile the default block */` |
|       67 | 5160 | `			rc = GenStateCompileSwitchBlock(pGen,&pSwitch->nDefault);` |
|       67 | 5161 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 5162 | `				return SXERR_ABORT;` |
|        5 | 5163 | `			}` |
|      300 | 5164 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|        - | 5165 | `			ph7_case_expr sCase;` |
|        - | 5166 | `			/* Standard case block */` |
|      259 | 5167 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|        - | 5168 | `			/* initialize the structure */` |
|      259 | 5169 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - | 5170 | `			/* Compile the case expression */` |
|      259 | 5171 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      259 | 5172 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 5173 | `				return SXERR_ABORT;` |
|        - | 5174 | `			}` |
|        - | 5175 | `			/* Compile the case block */` |
|      259 | 5176 | `			rc = GenStateCompileSwitchBlock(pGen,&sCase.nStart);` |
|        - | 5177 | `			/* Insert in the switch container */` |
|      259 | 5178 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      259 | 5179 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 5180 | `				return SXERR_ABORT;` |
|        - | 5181 | `			}` |
|      132 | 5182 | `		}else{` |
|        - | 5183 | `			/* php's parse error, naming what the case list wanted */` |
|       11 | 5184 | `			rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,pGen->pCurrent->zInnerTail);` |
|       11 | 5185 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 5186 | `				return SXERR_ABORT;` |
|        - | 5187 | `			}` |
|       11 | 5188 | `			break;` |
|        - | 5189 | `		}` |
|        5 | 5190 | `	}` |
|        - | 5191 | `	/* Fix all jumps now the destination is resolved */` |
|      185 | 5192 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      185 | 5193 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 5194 | `	/* Release the loop block */` |
|      185 | 5195 | `	GenStateLeaveBlock(pGen,0);` |
|      185 | 5196 | `	if( pGen->pIn < pGen->pEnd ){` |
|        - | 5197 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      271 | 5198 | `		int bEndKw = (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      164 | 5199 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_ENDSWITCH;` |
|      169 | 5200 | `		pGen->pIn++;` |
|      169 | 5201 | `		if( bEndKw && GenStateEndKeywordSemi(&(*pGen)) == SXERR_ABORT ){` |
|      ! 0 | 5202 | `			return SXERR_ABORT;` |
|        - | 5203 | `		}` |
|       82 | 5204 | `	}` |
|        - | 5205 | `	/* Statement successfully compiled */` |
|      185 | 5206 | `	return SXRET_OK;` |
|       22 | 5207 | `Synchronize:` |
|        - | 5208 | `	/* Synchronize with the first semi-colon */` |
|      117 | 5209 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       73 | 5210 | `		pGen->pIn++;` |
|        1 | 5211 | `	}` |
|       45 | 5212 | `	return SXRET_OK;` |
|      117 | 5213 | `}` |
|        - | 5214 |  |
