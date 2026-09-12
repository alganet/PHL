/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * PHP userland tokenizer surface: token_get_all(), token_name(), the T_*
 * constant family and the PhpToken class.
 *
 * This is a purpose-built, self-contained re-scan of the raw source bytes that
 * emits EVERY lexeme in source order — including T_WHITESPACE, T_COMMENT,
 * T_DOC_COMMENT, T_INLINE_HTML and the open/close tags — exactly as php's
 * tokenizer extension does. PHL's compile-time lexer (lex.c) drops whitespace
 * and routes comments to a trivia sidecar, so it cannot be reused for this.
 *
 * The T_* id VALUES are pinned to php 8.5's actual tokenizer constant ints so
 * that token_name() round-trips and consumers such as theseer/tokenizer (their
 * string->name MAP) work unchanged.
 */
#include "ph7int.h"

/*
 * php 8.5 tokenizer constant ids. Single source of truth for both the
 * registered T_* constants and token_name()'s reverse lookup.
 */
#define T_LNUMBER                                 260
#define T_DNUMBER                                 261
#define T_STRING                                  262
#define T_NAME_FULLY_QUALIFIED                    263
#define T_NAME_RELATIVE                           264
#define T_NAME_QUALIFIED                          265
#define T_VARIABLE                                266
#define T_INLINE_HTML                             267
#define T_ENCAPSED_AND_WHITESPACE                 268
#define T_CONSTANT_ENCAPSED_STRING                269
#define T_STRING_VARNAME                          270
#define T_NUM_STRING                              271
#define T_INCLUDE                                 272
#define T_INCLUDE_ONCE                            273
#define T_EVAL                                    274
#define T_REQUIRE                                 275
#define T_REQUIRE_ONCE                            276
#define T_LOGICAL_OR                              277
#define T_LOGICAL_XOR                             278
#define T_LOGICAL_AND                             279
#define T_PRINT                                   280
#define T_YIELD                                   281
#define T_YIELD_FROM                              282
#define T_INSTANCEOF                              283
#define T_NEW                                     284
#define T_CLONE                                   285
#define T_EXIT                                    286
#define T_IF                                      287
#define T_ELSEIF                                  288
#define T_ELSE                                    289
#define T_ENDIF                                   290
#define T_ECHO                                    291
#define T_DO                                      292
#define T_WHILE                                   293
#define T_ENDWHILE                                294
#define T_FOR                                     295
#define T_ENDFOR                                  296
#define T_FOREACH                                 297
#define T_ENDFOREACH                              298
#define T_DECLARE                                 299
#define T_ENDDECLARE                              300
#define T_AS                                      301
#define T_SWITCH                                  302
#define T_ENDSWITCH                               303
#define T_CASE                                    304
#define T_DEFAULT                                 305
#define T_MATCH                                   306
#define T_BREAK                                   307
#define T_CONTINUE                                308
#define T_GOTO                                    309
#define T_FUNCTION                                310
#define T_FN                                      311
#define T_CONST                                   312
#define T_RETURN                                  313
#define T_TRY                                     314
#define T_CATCH                                   315
#define T_FINALLY                                 316
#define T_THROW                                   317
#define T_USE                                     318
#define T_INSTEADOF                               319
#define T_GLOBAL                                  320
#define T_STATIC                                  321
#define T_ABSTRACT                                322
#define T_FINAL                                   323
#define T_PRIVATE                                 324
#define T_PROTECTED                               325
#define T_PUBLIC                                  326
#define T_PRIVATE_SET                             327
#define T_PROTECTED_SET                           328
#define T_PUBLIC_SET                              329
#define T_READONLY                                330
#define T_VAR                                     331
#define T_UNSET                                   332
#define T_ISSET                                   333
#define T_EMPTY                                   334
#define T_HALT_COMPILER                           335
#define T_CLASS                                   336
#define T_TRAIT                                   337
#define T_INTERFACE                               338
#define T_ENUM                                    339
#define T_EXTENDS                                 340
#define T_IMPLEMENTS                              341
#define T_NAMESPACE                               342
#define T_LIST                                    343
#define T_ARRAY                                   344
#define T_CALLABLE                                345
#define T_LINE                                    346
#define T_FILE                                    347
#define T_DIR                                     348
#define T_CLASS_C                                 349
#define T_TRAIT_C                                 350
#define T_METHOD_C                                351
#define T_FUNC_C                                  352
#define T_PROPERTY_C                              353
#define T_NS_C                                    354
#define T_ATTRIBUTE                               355
#define T_PLUS_EQUAL                              356
#define T_MINUS_EQUAL                             357
#define T_MUL_EQUAL                               358
#define T_DIV_EQUAL                               359
#define T_CONCAT_EQUAL                            360
#define T_MOD_EQUAL                               361
#define T_AND_EQUAL                               362
#define T_OR_EQUAL                                363
#define T_XOR_EQUAL                               364
#define T_SL_EQUAL                                365
#define T_SR_EQUAL                                366
#define T_COALESCE_EQUAL                          367
#define T_BOOLEAN_OR                              368
#define T_BOOLEAN_AND                             369
#define T_IS_EQUAL                                370
#define T_IS_NOT_EQUAL                            371
#define T_IS_IDENTICAL                            372
#define T_IS_NOT_IDENTICAL                        373
#define T_IS_SMALLER_OR_EQUAL                     374
#define T_IS_GREATER_OR_EQUAL                     375
#define T_SPACESHIP                               376
#define T_SL                                      377
#define T_SR                                      378
#define T_INC                                     379
#define T_DEC                                     380
#define T_INT_CAST                                381
#define T_DOUBLE_CAST                             382
#define T_STRING_CAST                             383
#define T_ARRAY_CAST                              384
#define T_OBJECT_CAST                             385
#define T_BOOL_CAST                               386
#define T_UNSET_CAST                              387
#define T_VOID_CAST                               388
#define T_OBJECT_OPERATOR                         389
#define T_NULLSAFE_OBJECT_OPERATOR                390
#define T_DOUBLE_ARROW                            391
#define T_COMMENT                                 392
#define T_DOC_COMMENT                             393
#define T_OPEN_TAG                                394
#define T_OPEN_TAG_WITH_ECHO                      395
#define T_CLOSE_TAG                               396
#define T_WHITESPACE                              397
#define T_START_HEREDOC                           398
#define T_END_HEREDOC                             399
#define T_DOLLAR_OPEN_CURLY_BRACES                400
#define T_CURLY_OPEN                              401
#define T_DOUBLE_COLON                            402
#define T_PAAMAYIM_NEKUDOTAYIM                    402
#define T_NS_SEPARATOR                            403
#define T_ELLIPSIS                                404
#define T_COALESCE                                405
#define T_POW                                     406
#define T_POW_EQUAL                               407
#define T_PIPE                                    408
#define T_AMPERSAND_FOLLOWED_BY_VAR_OR_VARARG     409
#define T_AMPERSAND_NOT_FOLLOWED_BY_VAR_OR_VARARG 410
#define T_BAD_CHARACTER                           411

/* Registered as a plain constant; here for token_name() completeness. */
#define TOK_TOKEN_PARSE                             1

typedef struct tok_const tok_const;
struct tok_const { const char *zName; int iId; };

/*
 * All tokenizer constants. token_name()'s reverse lookup uses this table too,
 * so the CANONICAL name for a shared id must come first (T_DOUBLE_COLON before
 * T_PAAMAYIM_NEKUDOTAYIM for id 402, matching php's token_name()).
 */
static const tok_const aTokConst[] = {
	{ "T_LNUMBER", T_LNUMBER }, { "T_DNUMBER", T_DNUMBER }, { "T_STRING", T_STRING },
	{ "T_NAME_FULLY_QUALIFIED", T_NAME_FULLY_QUALIFIED }, { "T_NAME_RELATIVE", T_NAME_RELATIVE },
	{ "T_NAME_QUALIFIED", T_NAME_QUALIFIED }, { "T_VARIABLE", T_VARIABLE }, { "T_INLINE_HTML", T_INLINE_HTML },
	{ "T_ENCAPSED_AND_WHITESPACE", T_ENCAPSED_AND_WHITESPACE },
	{ "T_CONSTANT_ENCAPSED_STRING", T_CONSTANT_ENCAPSED_STRING }, { "T_STRING_VARNAME", T_STRING_VARNAME },
	{ "T_NUM_STRING", T_NUM_STRING }, { "T_INCLUDE", T_INCLUDE }, { "T_INCLUDE_ONCE", T_INCLUDE_ONCE },
	{ "T_EVAL", T_EVAL }, { "T_REQUIRE", T_REQUIRE }, { "T_REQUIRE_ONCE", T_REQUIRE_ONCE },
	{ "T_LOGICAL_OR", T_LOGICAL_OR }, { "T_LOGICAL_XOR", T_LOGICAL_XOR }, { "T_LOGICAL_AND", T_LOGICAL_AND },
	{ "T_PRINT", T_PRINT }, { "T_YIELD", T_YIELD }, { "T_YIELD_FROM", T_YIELD_FROM },
	{ "T_INSTANCEOF", T_INSTANCEOF }, { "T_NEW", T_NEW }, { "T_CLONE", T_CLONE }, { "T_EXIT", T_EXIT },
	{ "T_IF", T_IF }, { "T_ELSEIF", T_ELSEIF }, { "T_ELSE", T_ELSE }, { "T_ENDIF", T_ENDIF },
	{ "T_ECHO", T_ECHO }, { "T_DO", T_DO }, { "T_WHILE", T_WHILE }, { "T_ENDWHILE", T_ENDWHILE },
	{ "T_FOR", T_FOR }, { "T_ENDFOR", T_ENDFOR }, { "T_FOREACH", T_FOREACH }, { "T_ENDFOREACH", T_ENDFOREACH },
	{ "T_DECLARE", T_DECLARE }, { "T_ENDDECLARE", T_ENDDECLARE }, { "T_AS", T_AS }, { "T_SWITCH", T_SWITCH },
	{ "T_ENDSWITCH", T_ENDSWITCH }, { "T_CASE", T_CASE }, { "T_DEFAULT", T_DEFAULT }, { "T_MATCH", T_MATCH },
	{ "T_BREAK", T_BREAK }, { "T_CONTINUE", T_CONTINUE }, { "T_GOTO", T_GOTO }, { "T_FUNCTION", T_FUNCTION },
	{ "T_FN", T_FN }, { "T_CONST", T_CONST }, { "T_RETURN", T_RETURN }, { "T_TRY", T_TRY },
	{ "T_CATCH", T_CATCH }, { "T_FINALLY", T_FINALLY }, { "T_THROW", T_THROW }, { "T_USE", T_USE },
	{ "T_INSTEADOF", T_INSTEADOF }, { "T_GLOBAL", T_GLOBAL }, { "T_STATIC", T_STATIC },
	{ "T_ABSTRACT", T_ABSTRACT }, { "T_FINAL", T_FINAL }, { "T_PRIVATE", T_PRIVATE },
	{ "T_PROTECTED", T_PROTECTED }, { "T_PUBLIC", T_PUBLIC }, { "T_PRIVATE_SET", T_PRIVATE_SET },
	{ "T_PROTECTED_SET", T_PROTECTED_SET }, { "T_PUBLIC_SET", T_PUBLIC_SET }, { "T_READONLY", T_READONLY },
	{ "T_VAR", T_VAR }, { "T_UNSET", T_UNSET }, { "T_ISSET", T_ISSET }, { "T_EMPTY", T_EMPTY },
	{ "T_HALT_COMPILER", T_HALT_COMPILER }, { "T_CLASS", T_CLASS }, { "T_TRAIT", T_TRAIT },
	{ "T_INTERFACE", T_INTERFACE }, { "T_ENUM", T_ENUM }, { "T_EXTENDS", T_EXTENDS },
	{ "T_IMPLEMENTS", T_IMPLEMENTS }, { "T_NAMESPACE", T_NAMESPACE }, { "T_LIST", T_LIST },
	{ "T_ARRAY", T_ARRAY }, { "T_CALLABLE", T_CALLABLE }, { "T_LINE", T_LINE }, { "T_FILE", T_FILE },
	{ "T_DIR", T_DIR }, { "T_CLASS_C", T_CLASS_C }, { "T_TRAIT_C", T_TRAIT_C }, { "T_METHOD_C", T_METHOD_C },
	{ "T_FUNC_C", T_FUNC_C }, { "T_PROPERTY_C", T_PROPERTY_C }, { "T_NS_C", T_NS_C },
	{ "T_ATTRIBUTE", T_ATTRIBUTE }, { "T_PLUS_EQUAL", T_PLUS_EQUAL }, { "T_MINUS_EQUAL", T_MINUS_EQUAL },
	{ "T_MUL_EQUAL", T_MUL_EQUAL }, { "T_DIV_EQUAL", T_DIV_EQUAL }, { "T_CONCAT_EQUAL", T_CONCAT_EQUAL },
	{ "T_MOD_EQUAL", T_MOD_EQUAL }, { "T_AND_EQUAL", T_AND_EQUAL }, { "T_OR_EQUAL", T_OR_EQUAL },
	{ "T_XOR_EQUAL", T_XOR_EQUAL }, { "T_SL_EQUAL", T_SL_EQUAL }, { "T_SR_EQUAL", T_SR_EQUAL },
	{ "T_COALESCE_EQUAL", T_COALESCE_EQUAL }, { "T_BOOLEAN_OR", T_BOOLEAN_OR },
	{ "T_BOOLEAN_AND", T_BOOLEAN_AND }, { "T_IS_EQUAL", T_IS_EQUAL }, { "T_IS_NOT_EQUAL", T_IS_NOT_EQUAL },
	{ "T_IS_IDENTICAL", T_IS_IDENTICAL }, { "T_IS_NOT_IDENTICAL", T_IS_NOT_IDENTICAL },
	{ "T_IS_SMALLER_OR_EQUAL", T_IS_SMALLER_OR_EQUAL }, { "T_IS_GREATER_OR_EQUAL", T_IS_GREATER_OR_EQUAL },
	{ "T_SPACESHIP", T_SPACESHIP }, { "T_SL", T_SL }, { "T_SR", T_SR }, { "T_INC", T_INC }, { "T_DEC", T_DEC },
	{ "T_INT_CAST", T_INT_CAST }, { "T_DOUBLE_CAST", T_DOUBLE_CAST }, { "T_STRING_CAST", T_STRING_CAST },
	{ "T_ARRAY_CAST", T_ARRAY_CAST }, { "T_OBJECT_CAST", T_OBJECT_CAST }, { "T_BOOL_CAST", T_BOOL_CAST },
	{ "T_UNSET_CAST", T_UNSET_CAST }, { "T_VOID_CAST", T_VOID_CAST }, { "T_OBJECT_OPERATOR", T_OBJECT_OPERATOR },
	{ "T_NULLSAFE_OBJECT_OPERATOR", T_NULLSAFE_OBJECT_OPERATOR }, { "T_DOUBLE_ARROW", T_DOUBLE_ARROW },
	{ "T_COMMENT", T_COMMENT }, { "T_DOC_COMMENT", T_DOC_COMMENT }, { "T_OPEN_TAG", T_OPEN_TAG },
	{ "T_OPEN_TAG_WITH_ECHO", T_OPEN_TAG_WITH_ECHO }, { "T_CLOSE_TAG", T_CLOSE_TAG },
	{ "T_WHITESPACE", T_WHITESPACE }, { "T_START_HEREDOC", T_START_HEREDOC }, { "T_END_HEREDOC", T_END_HEREDOC },
	{ "T_DOLLAR_OPEN_CURLY_BRACES", T_DOLLAR_OPEN_CURLY_BRACES }, { "T_CURLY_OPEN", T_CURLY_OPEN },
	{ "T_DOUBLE_COLON", T_DOUBLE_COLON }, { "T_PAAMAYIM_NEKUDOTAYIM", T_PAAMAYIM_NEKUDOTAYIM },
	{ "T_NS_SEPARATOR", T_NS_SEPARATOR }, { "T_ELLIPSIS", T_ELLIPSIS }, { "T_COALESCE", T_COALESCE },
	{ "T_POW", T_POW }, { "T_POW_EQUAL", T_POW_EQUAL }, { "T_PIPE", T_PIPE },
	{ "T_AMPERSAND_FOLLOWED_BY_VAR_OR_VARARG", T_AMPERSAND_FOLLOWED_BY_VAR_OR_VARARG },
	{ "T_AMPERSAND_NOT_FOLLOWED_BY_VAR_OR_VARARG", T_AMPERSAND_NOT_FOLLOWED_BY_VAR_OR_VARARG },
	{ "T_BAD_CHARACTER", T_BAD_CHARACTER },
	{ "TOKEN_PARSE", TOK_TOKEN_PARSE },
};

/*
 * The expander shared by every registered T_* constant: pUserData carries the
 * integer value directly (SX_INT_TO_PTR at registration time).
 */
static void TokConstExpand(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,SX_PTR_TO_INT(pUserData));
}

PH7_PRIVATE void PH7_RegisterTokenizerConstants(ph7_vm *pVm)
{
	sxu32 n;
	for( n = 0 ; n < SX_ARRAYSIZE(aTokConst) ; ++n ){
		ph7_create_constant(&(*pVm),aTokConst[n].zName,TokConstExpand,
			SX_INT_TO_PTR(aTokConst[n].iId));
	}
}

#ifndef PH7_DISABLE_BUILTIN_FUNC

/*
 * Reverse lookup for token_name(): the canonical php name for an id, or 0 for
 * unknown/out-of-range ids (token_name() then returns "UNKNOWN").
 */
static const char * TokConstName(int iId)
{
	sxu32 n;
	if( iId < 256 ){
		return 0; /* single-char and TOKEN_PARSE ids are "UNKNOWN" in token_name() */
	}
	for( n = 0 ; n < SX_ARRAYSIZE(aTokConst) ; ++n ){
		if( aTokConst[n].iId == iId ){
			return aTokConst[n].zName;
		}
	}
	return 0;
}

/*
 * Case-insensitive identifier -> keyword id. A lone identifier (no backslash)
 * matching one of these becomes the mapped T_* token; anything else is
 * T_STRING. 'enum' is deliberately absent — it is contextual (see the scanner).
 */
typedef struct tok_kw tok_kw;
struct tok_kw { const char *zName; int iId; };
static const tok_kw aKeyword[] = {
	{ "include", T_INCLUDE }, { "include_once", T_INCLUDE_ONCE }, { "eval", T_EVAL },
	{ "require", T_REQUIRE }, { "require_once", T_REQUIRE_ONCE }, { "or", T_LOGICAL_OR },
	{ "xor", T_LOGICAL_XOR }, { "and", T_LOGICAL_AND }, { "print", T_PRINT }, { "yield", T_YIELD },
	{ "instanceof", T_INSTANCEOF }, { "new", T_NEW }, { "clone", T_CLONE }, { "exit", T_EXIT },
	{ "die", T_EXIT }, { "if", T_IF }, { "elseif", T_ELSEIF }, { "else", T_ELSE }, { "endif", T_ENDIF },
	{ "echo", T_ECHO }, { "do", T_DO }, { "while", T_WHILE }, { "endwhile", T_ENDWHILE }, { "for", T_FOR },
	{ "endfor", T_ENDFOR }, { "foreach", T_FOREACH }, { "endforeach", T_ENDFOREACH }, { "declare", T_DECLARE },
	{ "enddeclare", T_ENDDECLARE }, { "as", T_AS }, { "switch", T_SWITCH }, { "endswitch", T_ENDSWITCH },
	{ "case", T_CASE }, { "default", T_DEFAULT }, { "match", T_MATCH }, { "break", T_BREAK },
	{ "continue", T_CONTINUE }, { "goto", T_GOTO }, { "function", T_FUNCTION }, { "fn", T_FN },
	{ "const", T_CONST }, { "return", T_RETURN }, { "try", T_TRY }, { "catch", T_CATCH },
	{ "finally", T_FINALLY }, { "throw", T_THROW }, { "use", T_USE }, { "insteadof", T_INSTEADOF },
	{ "global", T_GLOBAL }, { "static", T_STATIC }, { "abstract", T_ABSTRACT }, { "final", T_FINAL },
	{ "private", T_PRIVATE }, { "protected", T_PROTECTED }, { "public", T_PUBLIC }, { "readonly", T_READONLY },
	{ "var", T_VAR }, { "unset", T_UNSET }, { "isset", T_ISSET }, { "empty", T_EMPTY },
	{ "__halt_compiler", T_HALT_COMPILER }, { "class", T_CLASS }, { "trait", T_TRAIT },
	{ "interface", T_INTERFACE }, { "extends", T_EXTENDS }, { "implements", T_IMPLEMENTS },
	{ "namespace", T_NAMESPACE }, { "list", T_LIST }, { "array", T_ARRAY }, { "callable", T_CALLABLE },
	{ "__line__", T_LINE }, { "__file__", T_FILE }, { "__dir__", T_DIR }, { "__class__", T_CLASS_C },
	{ "__trait__", T_TRAIT_C }, { "__method__", T_METHOD_C }, { "__function__", T_FUNC_C },
	{ "__namespace__", T_NS_C }, { "__property__", T_PROPERTY_C },
};

/* Tokenizing state carried through the whole scan. */
typedef struct tok_state tok_state;
struct tok_state {
	ph7_context *pCtx;
	ph7_value   *pArray;   /* output array being built */
	ph7_value   *pS;       /* reusable scalar for plain single-char string tokens */
	ph7_value   *pId;      /* reusable scalar: tuple field [0] */
	ph7_value   *pText;    /* reusable scalar: tuple field [1] */
	ph7_value   *pLine;    /* reusable scalar: tuple field [2] */
	const unsigned char *z;      /* cursor */
	const unsigned char *zEnd;   /* one past end */
	int          iLine;    /* current 1-based line at the cursor */
	int          bProp;    /* one-shot: next identifier is a property -> T_STRING */
	int          bStop;    /* __halt_compiler seen: stop scanning entirely */
	int          bOOM;     /* memory failure flag */
};

/* --- character classes (byte-level, UTF-8 lead bytes count as label chars) --- */
static int tok_is_label_start(int c){
	return c=='_' || (c>='a'&&c<='z') || (c>='A'&&c<='Z') || c>=0x80;
}
static int tok_is_label(int c){
	return tok_is_label_start(c) || (c>='0'&&c<='9');
}
static int tok_is_ws(int c){
	/* php's tokenizer whitespace is exactly [ \t\r\n]; \v and \f are T_BAD_CHARACTER. */
	return c==' '||c=='\t'||c=='\n'||c=='\r';
}
static int tok_lower(int c){
	return (c>='A'&&c<='Z') ? c+32 : c;
}
static int tok_ci_eq(const char *z,int n,const char *zKw){
	int i;
	for( i = 0 ; i < n ; ++i ){
		if( zKw[i]==0 || tok_lower((unsigned char)z[i]) != (unsigned char)zKw[i] ){
			return 0;
		}
	}
	return zKw[n]==0;
}

/* php counts a line ending as any of "\n", "\r", or "\r\n" (each = one line). */
static int tok_at_nl(const unsigned char *p,const unsigned char *zEnd){
	if( *p=='\n' ){ return 1; }
	if( *p=='\r' && (p+1 >= zEnd || p[1]!='\n') ){ return 1; }
	return 0;
}

/* Count line endings in [z,zEnd) and advance the running line counter. */
static void tok_bump_lines(tok_state *ts,const unsigned char *z,const unsigned char *zEnd){
	while( z < zEnd ){
		if( tok_at_nl(z,zEnd) ){ ts->iLine++; }
		z++;
	}
}

/* Emit a plain-string token (single-char / operator returned as a bare string). */
static void tok_plain(tok_state *ts,const char *z,int n){
	if( ts->bOOM ){ return; }
	ph7_value_string(ts->pS,z,n);
	if( ph7_array_add_elem(ts->pArray,0,ts->pS) != SXRET_OK ){ ts->bOOM = 1; }
	ph7_value_reset_string_cursor(ts->pS);
}

/* Emit a [id, text, line] token. */
static void tok_tok(tok_state *ts,int iId,const char *z,int n,int iLine){
	ph7_value *pInner;
	if( ts->bOOM ){ return; }
	pInner = ph7_context_new_array(ts->pCtx);
	if( pInner == 0 ){ ts->bOOM = 1; return; }
	ph7_value_int(ts->pId,iId);
	ph7_value_string(ts->pText,z,n);
	ph7_value_int(ts->pLine,iLine);
	ph7_array_add_elem(pInner,0,ts->pId);
	ph7_array_add_elem(pInner,0,ts->pText);
	ph7_array_add_elem(pInner,0,ts->pLine);
	if( ph7_array_add_elem(ts->pArray,0,pInner) != SXRET_OK ){ ts->bOOM = 1; }
	ph7_context_release_value(ts->pCtx,pInner);
	ph7_value_reset_string_cursor(ts->pText);
}

/* Emit an encapsed-and-whitespace run [zStart, z) if non-empty, at iLine. */
static void tok_encaps(tok_state *ts,const unsigned char *zStart,const unsigned char *z,int iLine){
	if( z > zStart ){
		tok_tok(ts,T_ENCAPSED_AND_WHITESPACE,(const char *)zStart,(int)(z-zStart),iLine);
	}
}

/* Does the integer literal (digits between z and zEnd, given radix) overflow
 * ZEND_LONG_MAX? Underscores are ignored. */
static int tok_int_overflows(const unsigned char *z,const unsigned char *zEnd,int radix){
	sxu64 val = 0;
	const sxu64 max = (sxu64)0x7FFFFFFFFFFFFFFF; /* PHP_INT_MAX */
	while( z < zEnd ){
		int c = *z++;
		int d;
		if( c=='_' ){ continue; }
		if( c>='0'&&c<='9' ){ d = c-'0'; }
		else if( c>='a'&&c<='f' ){ d = c-'a'+10; }
		else if( c>='A'&&c<='F' ){ d = c-'A'+10; }
		else { break; }
		if( d >= radix ){ break; }
		if( val > (max - (sxu64)d)/(sxu64)radix ){
			return 1;
		}
		val = val*(sxu64)radix + (sxu64)d;
	}
	return 0;
}

/* Forward decls for the mutually-recursive string / expression scanners. */
static int  tok_lex_one(tok_state *ts);
static void tok_scan_curly(tok_state *ts,int bVarname);

/*
 * After a T_HALT_COMPILER token: php emits the "();" that follows as normal
 * tokens and then the entire remainder of the source as a single T_INLINE_HTML.
 */
static void tok_halt_tail(tok_state *ts){
	for(;;){
		if( ts->z >= ts->zEnd ){ break; }
		if( tok_is_ws(*ts->z) ){
			const unsigned char *z0 = ts->z;
			int iLine = ts->iLine;
			while( ts->z < ts->zEnd && tok_is_ws(*ts->z) ){
				if( tok_at_nl(ts->z,ts->zEnd) ){ ts->iLine++; }
				ts->z++;
			}
			tok_tok(ts,T_WHITESPACE,(const char *)z0,(int)(ts->z-z0),iLine);
			continue;
		}
		if( *ts->z=='(' || *ts->z==')' ){
			char ch = (char)*ts->z;
			tok_plain(ts,&ch,1);
			ts->z++;
			continue;
		}
		if( *ts->z==';' ){
			tok_plain(ts,";",1);
			ts->z++;
			break;
		}
		break;
	}
	if( ts->z < ts->zEnd ){
		int iLine = ts->iLine;
		tok_tok(ts,T_INLINE_HTML,(const char *)ts->z,(int)(ts->zEnd-ts->z),iLine);
		tok_bump_lines(ts,ts->z,ts->zEnd);
		ts->z = ts->zEnd;
	}
	ts->bStop = 1;
}

/*
 * Handle a possible interpolation construct at the cursor inside a double-quoted
 * string or heredoc body. Returns 1 if it consumed one (emitting tokens), else 0
 * (the caller keeps the char as literal). *pFlush is the start of the pending
 * literal run and its line; on a hit we flush it first.
 */
static int tok_try_interp(tok_state *ts,const unsigned char **pLitStart,int *pLitLine){
	const unsigned char *z = ts->z;
	if( *z=='$' && z+1 < ts->zEnd && tok_is_label_start(z[1]) ){
		const unsigned char *zVar = z+1;
		int iLine = ts->iLine;
		while( zVar < ts->zEnd && tok_is_label(*zVar) ){ zVar++; }
		tok_encaps(ts,*pLitStart,z,*pLitLine);
		tok_tok(ts,T_VARIABLE,(const char *)z,(int)(zVar-z),iLine);
		ts->z = zVar;
		/* one optional simple offset [...] or ->prop */
		if( ts->z < ts->zEnd && *ts->z=='[' ){
			tok_plain(ts,"[",1);
			ts->z++;
			if( ts->z < ts->zEnd && *ts->z=='$' && ts->z+1 < ts->zEnd && tok_is_label_start(ts->z[1]) ){
				const unsigned char *v = ts->z+1;
				while( v < ts->zEnd && tok_is_label(*v) ){ v++; }
				tok_tok(ts,T_VARIABLE,(const char *)ts->z,(int)(v-ts->z),ts->iLine);
				ts->z = v;
			}else{
				const unsigned char *n0 = ts->z;
				if( ts->z < ts->zEnd && *ts->z=='-' ){
					tok_plain(ts,"-",1);
					ts->z++;
					n0 = ts->z;
				}
				if( ts->z < ts->zEnd && *ts->z>='0' && *ts->z<='9' ){
					while( ts->z < ts->zEnd && *ts->z>='0' && *ts->z<='9' ){ ts->z++; }
					tok_tok(ts,T_NUM_STRING,(const char *)n0,(int)(ts->z-n0),ts->iLine);
				}else if( ts->z < ts->zEnd && tok_is_label_start(*ts->z) ){
					const unsigned char *l = ts->z;
					while( l < ts->zEnd && tok_is_label(*l) ){ l++; }
					tok_tok(ts,T_STRING,(const char *)ts->z,(int)(l-ts->z),ts->iLine);
					ts->z = l;
				}
			}
			if( ts->z < ts->zEnd && *ts->z==']' ){ tok_plain(ts,"]",1); ts->z++; }
		}else if( ts->z+2 < ts->zEnd && ts->z[0]=='-' && ts->z[1]=='>' && tok_is_label_start(ts->z[2]) ){
			const unsigned char *l;
			tok_tok(ts,T_OBJECT_OPERATOR,"->",2,ts->iLine);
			ts->z += 2;
			l = ts->z;
			while( l < ts->zEnd && tok_is_label(*l) ){ l++; }
			tok_tok(ts,T_STRING,(const char *)ts->z,(int)(l-ts->z),ts->iLine);
			ts->z = l;
		}
		*pLitStart = ts->z;
		*pLitLine  = ts->iLine;
		return 1;
	}
	if( *z=='{' && z+1 < ts->zEnd && z[1]=='$' ){
		tok_encaps(ts,*pLitStart,z,*pLitLine);
		tok_tok(ts,T_CURLY_OPEN,"{",1,ts->iLine);
		ts->z = z+1;
		tok_scan_curly(ts,0);
		*pLitStart = ts->z;
		*pLitLine  = ts->iLine;
		return 1;
	}
	if( *z=='$' && z+1 < ts->zEnd && z[1]=='{' ){
		tok_encaps(ts,*pLitStart,z,*pLitLine);
		tok_tok(ts,T_DOLLAR_OPEN_CURLY_BRACES,"${",2,ts->iLine);
		ts->z = z+2;
		tok_scan_curly(ts,1);
		*pLitStart = ts->z;
		*pLitLine  = ts->iLine;
		return 1;
	}
	return 0;
}

/*
 * Scan a brace-delimited expression inside a string ("{$...}" or "${...}").
 * The opening brace token was already emitted and ts->z points just past it.
 * Emits PHP tokens until the matching close brace, which it emits as a plain
 * "}" string token. bVarname: the first label (if immediately followed by '}'
 * or '[') is a T_STRING_VARNAME (the "${name}" form).
 */
static void tok_scan_curly(tok_state *ts,int bVarname){
	int depth = 1;
	if( bVarname && ts->z < ts->zEnd && tok_is_label_start(*ts->z) ){
		const unsigned char *l = ts->z;
		while( l < ts->zEnd && tok_is_label(*l) ){ l++; }
		if( l < ts->zEnd && (*l=='}' || *l=='[') ){
			tok_tok(ts,T_STRING_VARNAME,(const char *)ts->z,(int)(l-ts->z),ts->iLine);
			ts->z = l;
		}
	}
	while( ts->z < ts->zEnd && depth > 0 && !ts->bOOM ){
		int eff;
		if( *ts->z=='}' ){
			depth--;
			if( depth == 0 ){
				tok_plain(ts,"}",1);
				ts->z++;
				return;
			}
		}
		eff = tok_lex_one(ts);
		if( eff == '{' ){ depth++; }
		else if( eff == '}' ){ depth--; if( depth==0 ){ return; } }
		else if( eff < 0 ){ return; } /* close tag / EOF safety */
	}
}

/*
 * Scan a double-quoted string or backtick string starting at the opening
 * delimiter (ts->z points at it). If double-quoted with no interpolation the
 * whole literal is one T_CONSTANT_ENCAPSED_STRING; otherwise it splits into the
 * delimiter, encapsed runs and interpolation tokens.
 */
static void tok_scan_dquote(tok_state *ts,int chDelim){
	const unsigned char *zOpen = ts->z;
	int iOpenLine = ts->iLine;
	const unsigned char *zScan = ts->z+1;
	int bInterp = 0;
	int bClosed = 0;
	/* Peek for interpolation to decide constant-vs-split (only for '"'). */
	while( zScan < ts->zEnd ){
		int c = *zScan;
		if( c=='\\' ){ zScan += 2; continue; }
		if( c==chDelim ){ bClosed = 1; break; }
		if( c=='$' && zScan+1 < ts->zEnd && (tok_is_label_start(zScan[1])||zScan[1]=='{') ){ bInterp = 1; break; }
		if( c=='{' && zScan+1 < ts->zEnd && zScan[1]=='$' ){ bInterp = 1; break; }
		zScan++;
	}
	if( chDelim=='"' && !bInterp && bClosed ){
		/* Whole constant string. Find the real closing quote. */
		const unsigned char *z = ts->z+1;
		while( z < ts->zEnd ){
			if( *z=='\\' && z+1 < ts->zEnd ){ z += 2; continue; }
			if( *z=='"' ){ break; }
			z++;
		}
		if( z < ts->zEnd ){ z++; } /* include closing quote */
		tok_tok(ts,T_CONSTANT_ENCAPSED_STRING,(const char *)zOpen,(int)(z-zOpen),iOpenLine);
		tok_bump_lines(ts,ts->z,z);
		ts->z = z;
		return;
	}
	/* Split form: opening delimiter, then body, then closing delimiter. */
	{
		char d = (char)chDelim;
		const unsigned char *litStart;
		int litLine;
		tok_plain(ts,&d,1);
		ts->z++;
		litStart = ts->z;
		litLine  = ts->iLine;
		while( ts->z < ts->zEnd && !ts->bOOM ){
			int c = *ts->z;
			if( c=='\\' && ts->z+1 < ts->zEnd ){
				if( ts->z[1]=='\n' ){ ts->iLine++; } /* escaped newline still advances the line */
				ts->z += 2;
				continue;
			}
			if( c==chDelim ){
				tok_encaps(ts,litStart,ts->z,litLine);
				tok_plain(ts,&d,1);
				ts->z++;
				return;
			}
			if( c=='$' || c=='{' ){
				if( tok_try_interp(ts,&litStart,&litLine) ){ continue; }
			}
			if( tok_at_nl(ts->z,ts->zEnd) ){ ts->iLine++; }
			ts->z++;
		}
		/* Unterminated: flush what we have. */
		tok_encaps(ts,litStart,ts->z,litLine);
	}
}

/* Does the line at z (already at line start) begin the heredoc closing marker
 * for zLabel? Returns the end of the label (past it) if so, else 0. */
static const unsigned char * tok_heredoc_close(tok_state *ts,const unsigned char *z,
		const char *zLabel,int nLabel){
	const unsigned char *p = z;
	int i;
	while( p < ts->zEnd && (*p==' '||*p=='\t') ){ p++; }
	if( p + nLabel > ts->zEnd ){ return 0; } /* not enough bytes left for the label */
	for( i = 0 ; i < nLabel ; ++i ){
		if( p[i] != (unsigned char)zLabel[i] ){ return 0; }
	}
	p += nLabel;
	if( p < ts->zEnd && tok_is_label(*p) ){ return 0; } /* label is a prefix of a longer word */
	return p;
}

/*
 * Scan a heredoc/nowdoc starting at "<<<". ts->z points at the first '<'.
 * Returns 1 if a valid heredoc was consumed, 0 if this "<<<" is not a valid
 * heredoc start (the caller then falls through to operator lexing: T_SL + '<').
 * A valid start is: "<<<" [ws]* ["|']? LABEL ["|']? immediately followed by
 * an optional '\r' and a '\n'.
 */
static int tok_scan_heredoc(tok_state *ts){
	const unsigned char *zStart = ts->z;
	int iStartLine = ts->iLine;
	const unsigned char *z = ts->z+3;
	int bNowdoc = 0;
	int chQuote = 0;
	const unsigned char *zLabel;
	int nLabel;
	const unsigned char *litStart;
	int litLine;
	/* optional spaces/tabs after <<< */
	while( z < ts->zEnd && (*z==' '||*z=='\t') ){ z++; }
	if( z < ts->zEnd && (*z=='"' || *z=='\'') ){
		chQuote = *z;
		bNowdoc = (*z=='\'');
		z++;
	}
	zLabel = z;
	while( z < ts->zEnd && tok_is_label(*z) ){ z++; }
	nLabel = (int)(z - zLabel);
	if( nLabel < 1 ){ return 0; } /* no label -> not a heredoc */
	if( chQuote ){
		if( z < ts->zEnd && *z==chQuote ){ z++; }
		else { return 0; } /* unbalanced quote -> not a heredoc */
	}
	/* The label must be immediately followed by an optional '\r' then '\n'. */
	if( z < ts->zEnd && *z=='\r' && z+1 < ts->zEnd && z[1]=='\n' ){ z += 2; }
	else if( z < ts->zEnd && *z=='\n' ){ z += 1; }
	else { return 0; } /* junk or EOF after the marker -> not a heredoc */
	/* Emit T_START_HEREDOC (delimiters + trailing newline). */
	tok_tok(ts,T_START_HEREDOC,(const char *)zStart,(int)(z-zStart),iStartLine);
	tok_bump_lines(ts,zStart,z);
	ts->z = z;
	/* Scan body line by line until the closing marker. */
	litStart = ts->z;
	litLine  = ts->iLine;
	while( ts->z < ts->zEnd && !ts->bOOM ){
		/* At a line start, test for the closing marker. */
		const unsigned char *pClose = tok_heredoc_close(ts,ts->z,(const char *)zLabel,nLabel);
		if( pClose ){
			int iCloseLine;
			tok_encaps(ts,litStart,ts->z,litLine);
			iCloseLine = ts->iLine;
			tok_tok(ts,T_END_HEREDOC,(const char *)ts->z,(int)(pClose-ts->z),iCloseLine);
			ts->z = pClose;
			return 1;
		}
		/* Process one line's content. */
		while( ts->z < ts->zEnd ){
			int c = *ts->z;
			if( c=='\n' ){ ts->iLine++; ts->z++; break; }
			if( c=='\r' && (ts->z+1>=ts->zEnd || ts->z[1]!='\n') ){ ts->iLine++; ts->z++; continue; }
			if( !bNowdoc && (c=='\\') && ts->z+1 < ts->zEnd ){
				if( ts->z[1]=='\n' ){ ts->iLine++; } /* escaped newline still advances the line */
				ts->z += 2;
				continue;
			}
			if( !bNowdoc && (c=='$' || c=='{') ){
				if( tok_try_interp(ts,&litStart,&litLine) ){ continue; }
			}
			ts->z++;
		}
	}
	tok_encaps(ts,litStart,ts->z,litLine);
	return 1;
}

/* One cast keyword between the parens, e.g. "int". Returns the cast T_* id or 0. */
static int tok_cast_id(const char *z,int n){
	if( tok_ci_eq(z,n,"int") || tok_ci_eq(z,n,"integer") ) return T_INT_CAST;
	if( tok_ci_eq(z,n,"float") || tok_ci_eq(z,n,"double") || tok_ci_eq(z,n,"real") ) return T_DOUBLE_CAST;
	if( tok_ci_eq(z,n,"string") || tok_ci_eq(z,n,"binary") ) return T_STRING_CAST;
	if( tok_ci_eq(z,n,"array") ) return T_ARRAY_CAST;
	if( tok_ci_eq(z,n,"object") ) return T_OBJECT_CAST;
	if( tok_ci_eq(z,n,"bool") || tok_ci_eq(z,n,"boolean") ) return T_BOOL_CAST;
	if( tok_ci_eq(z,n,"unset") ) return T_UNSET_CAST;
	return 0;
}

/*
 * Try to lex "( <ws>? castword <ws>? )" at ts->z (pointing at '('). On success
 * emits the cast token and returns 1; otherwise returns 0 and consumes nothing.
 */
static int tok_try_cast(tok_state *ts){
	const unsigned char *p = ts->z+1;
	const unsigned char *w0,*w1;
	int id;
	while( p < ts->zEnd && (*p==' '||*p=='\t') ){ p++; }
	w0 = p;
	while( p < ts->zEnd && tok_is_label(*p) ){ p++; }
	w1 = p;
	if( w1 == w0 ){ return 0; }
	id = tok_cast_id((const char *)w0,(int)(w1-w0));
	if( id == 0 ){ return 0; }
	while( p < ts->zEnd && (*p==' '||*p=='\t') ){ p++; }
	if( p >= ts->zEnd || *p != ')' ){ return 0; }
	p++;
	tok_tok(ts,id,(const char *)ts->z,(int)(p-ts->z),ts->iLine);
	ts->z = p;
	return 1;
}

/*
 * Lex exactly one PHP-mode token and emit it. Returns:
 *   0   normal token
 *  '{' / '}'  when it emitted a bare '{' / '}' (for curly-expr brace tracking)
 *  -1  when it emitted a close tag or hit EOF (leave PHP mode)
 * Whitespace and comments preserve ts->bProp; every other token consumes it.
 */
static int tok_lex_one(tok_state *ts){
	const unsigned char *z = ts->z;
	int c;
	int bWasProp;
	if( z >= ts->zEnd ){ return -1; }
	c = *z;
	/* Whitespace run (preserves property state). */
	if( tok_is_ws(c) ){
		const unsigned char *z0 = z;
		int iLine = ts->iLine;
		while( ts->z < ts->zEnd && tok_is_ws(*ts->z) ){
			if( tok_at_nl(ts->z,ts->zEnd) ){ ts->iLine++; }
			ts->z++;
		}
		tok_tok(ts,T_WHITESPACE,(const char *)z0,(int)(ts->z-z0),iLine);
		return 0;
	}
	/* Comments: hash/slash-slash to EOL-or-close-tag, and block comments. Preserve property state. */
	if( c=='#' && !(z+1 < ts->zEnd && z[1]=='[') ){
		const unsigned char *z0 = z;
		int iLine = ts->iLine;
		ts->z++;
		while( ts->z < ts->zEnd && *ts->z!='\n' ){
			if( *ts->z=='?' && ts->z+1 < ts->zEnd && ts->z[1]=='>' ){ break; }
			ts->z++;
		}
		tok_tok(ts,T_COMMENT,(const char *)z0,(int)(ts->z-z0),iLine);
		return 0;
	}
	if( c=='/' && z+1 < ts->zEnd && z[1]=='/' ){
		const unsigned char *z0 = z;
		int iLine = ts->iLine;
		ts->z += 2;
		while( ts->z < ts->zEnd && *ts->z!='\n' ){
			if( *ts->z=='?' && ts->z+1 < ts->zEnd && ts->z[1]=='>' ){ break; }
			ts->z++;
		}
		tok_tok(ts,T_COMMENT,(const char *)z0,(int)(ts->z-z0),iLine);
		return 0;
	}
	if( c=='/' && z+1 < ts->zEnd && z[1]=='*' ){
		const unsigned char *z0 = z;
		int iLine = ts->iLine;
		/* A doc comment is slash-star-star followed by a whitespace char (php's
		 * rule): the third char must be '*' and the fourth must be whitespace. */
		int bDoc = ( z+2 < ts->zEnd && z[2]=='*' && z+3 < ts->zEnd && tok_is_ws(z[3]) );
		ts->z += 2;
		while( ts->z < ts->zEnd ){
			if( *ts->z=='*' && ts->z+1 < ts->zEnd && ts->z[1]=='/' ){ ts->z += 2; break; }
			if( tok_at_nl(ts->z,ts->zEnd) ){ ts->iLine++; }
			ts->z++;
		}
		tok_tok(ts,bDoc ? T_DOC_COMMENT : T_COMMENT,(const char *)z0,(int)(ts->z-z0),iLine);
		return 0;
	}
	/* From here the token consumes property state. */
	bWasProp = ts->bProp;
	ts->bProp = 0;
	/* Close tag. */
	if( c=='?' && z+1 < ts->zEnd && z[1]=='>' ){
		const unsigned char *z0 = z;
		int iLine = ts->iLine;
		ts->z += 2;
		/* php swallows one trailing newline (\n or \r\n) into the close tag */
		if( ts->z < ts->zEnd && *ts->z=='\r' && ts->z+1 < ts->zEnd && ts->z[1]=='\n' ){ ts->z += 2; ts->iLine++; }
		else if( ts->z < ts->zEnd && *ts->z=='\n' ){ ts->z++; ts->iLine++; }
		tok_tok(ts,T_CLOSE_TAG,(const char *)z0,(int)(ts->z-z0),iLine);
		return -1;
	}
	/* #[ attribute open. */
	if( c=='#' && z+1 < ts->zEnd && z[1]=='[' ){
		tok_tok(ts,T_ATTRIBUTE,"#[",2,ts->iLine);
		ts->z += 2;
		return 0;
	}
	/* Variable. */
	if( c=='$' && z+1 < ts->zEnd && tok_is_label_start(z[1]) ){
		const unsigned char *v = z+1;
		while( v < ts->zEnd && tok_is_label(*v) ){ v++; }
		tok_tok(ts,T_VARIABLE,(const char *)z,(int)(v-z),ts->iLine);
		ts->z = v;
		return 0;
	}
	/* Namespaced name / identifier / keyword. */
	if( tok_is_label_start(c) || (c=='\\' && z+1 < ts->zEnd && tok_is_label_start(z[1])) ){
		const unsigned char *p = z;
		int bLeadBackslash = (c=='\\');
		int bInner = 0;                 /* saw an internal backslash */
		const unsigned char *firstLabelStart, *firstLabelEnd;
		if( bLeadBackslash ){ p++; }
		firstLabelStart = p;
		while( p < ts->zEnd && tok_is_label(*p) ){ p++; }
		firstLabelEnd = p;
		/* Consume further \label segments. */
		while( p+1 < ts->zEnd && *p=='\\' && tok_is_label_start(p[1]) ){
			bInner = 1;
			p++;
			while( p < ts->zEnd && tok_is_label(*p) ){ p++; }
		}
		if( bLeadBackslash ){
			tok_tok(ts,T_NAME_FULLY_QUALIFIED,(const char *)z,(int)(p-z),ts->iLine);
			ts->z = p;
			return 0;
		}
		if( bInner ){
			int nFirst = (int)(firstLabelEnd - firstLabelStart);
			int id = T_NAME_QUALIFIED;
			if( tok_ci_eq((const char *)firstLabelStart,nFirst,"namespace") ){ id = T_NAME_RELATIVE; }
			tok_tok(ts,id,(const char *)z,(int)(p-z),ts->iLine);
			ts->z = p;
			return 0;
		}
		/* Lone identifier: keyword, contextual, or T_STRING. */
		{
			int n = (int)(firstLabelEnd - z);
			int id = T_STRING;
			sxu32 k;
			if( bWasProp ){
				id = T_STRING;              /* property access after -> / ?-> */
			}else if( tok_ci_eq((const char *)z,n,"enum") ){
				/* Contextual: T_ENUM only when followed by <ws>+ then a label-start. */
				const unsigned char *q = firstLabelEnd;
				int sawWs = 0;
				while( q < ts->zEnd && (*q==' '||*q=='\t'||*q=='\n'||*q=='\r') ){ q++; sawWs = 1; }
				if( sawWs && q < ts->zEnd && tok_is_label_start(*q) ){ id = T_ENUM; }
			}else if( tok_ci_eq((const char *)z,n,"yield") ){
				/* Contextual: "yield from" collapses to one T_YIELD_FROM. */
				const unsigned char *q = firstLabelEnd;
				const unsigned char *ws = q;
				while( q < ts->zEnd && (*q==' '||*q=='\t'||*q=='\n'||*q=='\r'||*q=='\v'||*q=='\f') ){ q++; }
				if( q > ws && q+4 <= ts->zEnd && tok_ci_eq((const char *)q,4,"from")
					&& (q+4 >= ts->zEnd || !tok_is_label(q[4])) ){
					const unsigned char *e = q+4;
					tok_tok(ts,T_YIELD_FROM,(const char *)z,(int)(e-z),ts->iLine);
					tok_bump_lines(ts,firstLabelEnd,e);
					ts->z = e;
					return 0;
				}
				id = T_YIELD;
			}else{
				for( k = 0 ; k < SX_ARRAYSIZE(aKeyword) ; ++k ){
					if( tok_ci_eq((const char *)z,n,aKeyword[k].zName) ){
						id = aKeyword[k].iId;
						break;
					}
				}
			}
			if( id == T_STRING ){
				tok_tok(ts,T_STRING,(const char *)z,n,ts->iLine);
			}else{
				tok_tok(ts,id,(const char *)z,n,ts->iLine);
			}
			ts->z = firstLabelEnd;
			if( id == T_HALT_COMPILER ){
				tok_halt_tail(ts);
				return -1;
			}
			return 0;
		}
	}
	/* Lone backslash -> namespace separator. */
	if( c=='\\' ){
		tok_tok(ts,T_NS_SEPARATOR,"\\",1,ts->iLine);
		ts->z++;
		return 0;
	}
	/* Number. */
	if( (c>='0'&&c<='9') || (c=='.' && z+1 < ts->zEnd && z[1]>='0' && z[1]<='9') ){
		const unsigned char *p = z;
		int isFloat = 0;
		int id;
		if( c=='0' && p+2 < ts->zEnd && (p[1]=='x'||p[1]=='X')
				&& ((p[2]>='0'&&p[2]<='9')||(p[2]>='a'&&p[2]<='f')||(p[2]>='A'&&p[2]<='F')) ){
			const unsigned char *d0;
			p += 2; d0 = p;
			while( p < ts->zEnd && ((*p>='0'&&*p<='9')||(*p>='a'&&*p<='f')||(*p>='A'&&*p<='F')||*p=='_') ){ p++; }
			id = tok_int_overflows(d0,p,16) ? T_DNUMBER : T_LNUMBER;
		}else if( c=='0' && p+2 < ts->zEnd && (p[1]=='b'||p[1]=='B') && (p[2]=='0'||p[2]=='1') ){
			const unsigned char *d0;
			p += 2; d0 = p;
			while( p < ts->zEnd && (*p=='0'||*p=='1'||*p=='_') ){ p++; }
			id = tok_int_overflows(d0,p,2) ? T_DNUMBER : T_LNUMBER;
		}else if( c=='0' && p+2 < ts->zEnd && (p[1]=='o'||p[1]=='O') && (p[2]>='0'&&p[2]<='7') ){
			const unsigned char *d0;
			p += 2; d0 = p;
			while( p < ts->zEnd && ((*p>='0'&&*p<='7')||*p=='_') ){ p++; }
			id = tok_int_overflows(d0,p,8) ? T_DNUMBER : T_LNUMBER;
		}else{
			const unsigned char *intStart = p;
			int allOctalDigits = 1;
			while( p < ts->zEnd && ((*p>='0'&&*p<='9')||*p=='_') ){
				if( *p>'7' ){ allOctalDigits = 0; }
				p++;
			}
			if( p < ts->zEnd && *p=='.' && !(c=='.') ){
				/* fractional part (unless the token itself started with '.') */
				isFloat = 1;
				p++;
				while( p < ts->zEnd && ((*p>='0'&&*p<='9')||*p=='_') ){ p++; }
			}else if( c=='.' ){
				isFloat = 1; /* .5 style: leading dot already consumed below */
			}
			if( c=='.' ){
				/* token began with '.': consume the dot + digits here */
				p = z+1;
				while( p < ts->zEnd && ((*p>='0'&&*p<='9')||*p=='_') ){ p++; }
				isFloat = 1;
			}
			if( p < ts->zEnd && (*p=='e'||*p=='E') ){
				const unsigned char *e = p+1;
				if( e < ts->zEnd && (*e=='+'||*e=='-') ){ e++; }
				if( e < ts->zEnd && *e>='0' && *e<='9' ){
					isFloat = 1;
					p = e;
					while( p < ts->zEnd && ((*p>='0'&&*p<='9')||*p=='_') ){ p++; }
				}
			}
			if( isFloat ){
				id = T_DNUMBER;
			}else if( intStart < ts->zEnd && *intStart=='0' && (p-intStart) > 1 && allOctalDigits ){
				/* legacy octal 0NNN */
				id = tok_int_overflows(intStart+1,p,8) ? T_DNUMBER : T_LNUMBER;
			}else{
				id = tok_int_overflows(intStart,p,10) ? T_DNUMBER : T_LNUMBER;
			}
		}
		tok_tok(ts,id,(const char *)z,(int)(p-z),ts->iLine);
		ts->z = p;
		return 0;
	}
	/* Strings. */
	if( c=='\'' ){
		const unsigned char *p = z+1;
		int bClosed = 0;
		while( p < ts->zEnd ){
			if( *p=='\\' && p+1 < ts->zEnd ){ p += 2; continue; }
			if( *p=='\'' ){ p++; bClosed = 1; break; }
			p++;
		}
		/* An unterminated single-quoted string is one T_ENCAPSED_AND_WHITESPACE in php. */
		tok_tok(ts,bClosed ? T_CONSTANT_ENCAPSED_STRING : T_ENCAPSED_AND_WHITESPACE,
			(const char *)z,(int)(p-z),ts->iLine);
		tok_bump_lines(ts,z,p);
		ts->z = p;
		return 0;
	}
	if( c=='"' ){ tok_scan_dquote(ts,'"'); return 0; }
	if( c=='`' ){ tok_scan_dquote(ts,'`'); return 0; }
	/* Heredoc / nowdoc (falls through to operators if not a valid start). */
	if( c=='<' && z+2 < ts->zEnd && z[1]=='<' && z[2]=='<' ){
		if( tok_scan_heredoc(ts) ){ return 0; }
	}
	/* Cast operators. */
	if( c=='(' ){
		if( tok_try_cast(ts) ){ return 0; }
	}
	/* Object operators set the one-shot property state (next identifier -> T_STRING). */
	if( c=='?' && z+2 < ts->zEnd && z[1]=='-' && z[2]=='>' ){
		tok_tok(ts,T_NULLSAFE_OBJECT_OPERATOR,"?->",3,ts->iLine);
		ts->z += 3;
		ts->bProp = 1;
		return 0;
	}
	if( c=='-' && z+1 < ts->zEnd && z[1]=='>' ){
		tok_tok(ts,T_OBJECT_OPERATOR,"->",2,ts->iLine);
		ts->z += 2;
		ts->bProp = 1;
		return 0;
	}
	/* Multi-char and single-char operators (longest match first). */
	{
		const unsigned char *e = ts->zEnd;
		int r0 = c;
		int r1 = (z+1<e)?z[1]:-1;
		int r2 = (z+2<e)?z[2]:-1;
		#define TK3(a,b,cc,id) if(r0==(a)&&r1==(b)&&r2==(cc)){ tok_tok(ts,id,(const char*)z,3,ts->iLine); ts->z+=3; return 0; }
		#define TK2(a,b,id)    if(r0==(a)&&r1==(b)){ tok_tok(ts,id,(const char*)z,2,ts->iLine); ts->z+=2; return 0; }
		TK3('=','=','=',T_IS_IDENTICAL)
		TK3('!','=','=',T_IS_NOT_IDENTICAL)
		TK3('<','=','>',T_SPACESHIP)
		TK3('*','*','=',T_POW_EQUAL)
		TK3('.','.','.',T_ELLIPSIS)
		TK3('<','<','=',T_SL_EQUAL)
		TK3('>','>','=',T_SR_EQUAL)
		TK3('?','?','=',T_COALESCE_EQUAL)
		TK2('=','=',T_IS_EQUAL)
		TK2('!','=',T_IS_NOT_EQUAL)
		TK2('<','>',T_IS_NOT_EQUAL)
		TK2('<','=',T_IS_SMALLER_OR_EQUAL)
		TK2('>','=',T_IS_GREATER_OR_EQUAL)
		TK2('&','&',T_BOOLEAN_AND)
		TK2('|','|',T_BOOLEAN_OR)
		TK2('|','>',T_PIPE)
		TK2('+','+',T_INC)
		TK2('-','-',T_DEC)
		TK2('=','>',T_DOUBLE_ARROW)
		TK2(':',':',T_DOUBLE_COLON)
		TK2('<','<',T_SL)
		TK2('>','>',T_SR)
		TK2('*','*',T_POW)
		TK2('?','?',T_COALESCE)
		TK2('+','=',T_PLUS_EQUAL)
		TK2('-','=',T_MINUS_EQUAL)
		TK2('*','=',T_MUL_EQUAL)
		TK2('/','=',T_DIV_EQUAL)
		TK2('.','=',T_CONCAT_EQUAL)
		TK2('%','=',T_MOD_EQUAL)
		TK2('&','=',T_AND_EQUAL)
		TK2('|','=',T_OR_EQUAL)
		TK2('^','=',T_XOR_EQUAL)
		#undef TK3
		#undef TK2
	}
	/* Ampersand: FOLLOWED (by var / vararg) vs NOT, looking past whitespace. */
	if( c=='&' ){
		const unsigned char *p = z+1;
		int id;
		while( p < ts->zEnd && tok_is_ws(*p) ){ p++; }
		if( (p < ts->zEnd && *p=='$') ||
			(p+2 < ts->zEnd && p[0]=='.' && p[1]=='.' && p[2]=='.') ){
			id = T_AMPERSAND_FOLLOWED_BY_VAR_OR_VARARG;
		}else{
			id = T_AMPERSAND_NOT_FOLLOWED_BY_VAR_OR_VARARG;
		}
		tok_tok(ts,id,"&",1,ts->iLine);
		ts->z++;
		return 0;
	}
	/* Control bytes that begin no token are T_BAD_CHARACTER in php. */
	if( c < 0x20 || c == 0x7f ){
		char ch = (char)c;
		tok_tok(ts,T_BAD_CHARACTER,&ch,1,ts->iLine);
		ts->z++;
		return 0;
	}
	/* Single-char token, returned as a bare string. */
	{
		char ch = (char)c;
		tok_plain(ts,&ch,1);
		ts->z++;
		if( c=='{' ) return '{';
		if( c=='}' ) return '}';
		return 0;
	}
}

/* Emit T_OPEN_TAG/T_OPEN_TAG_WITH_ECHO at ts->z; returns 1 if a tag was found. */
static int tok_open_tag(tok_state *ts){
	const unsigned char *z = ts->z;
	if( z+2 < ts->zEnd && z[0]=='<' && z[1]=='?' && z[2]=='=' ){
		tok_tok(ts,T_OPEN_TAG_WITH_ECHO,"<?=",3,ts->iLine);
		ts->z = z+3;
		return 1;
	}
	if( z+4 <= ts->zEnd && z[0]=='<' && z[1]=='?'
		&& tok_lower(z[2])=='p' && tok_lower(z[3])=='h' && z+5 <= ts->zEnd && tok_lower(z[4])=='p' ){
		/* Require <?php to be followed by whitespace or EOF. */
		const unsigned char *p = z+5;
		if( p >= ts->zEnd || tok_is_ws(*p) ){
			const unsigned char *e = z+5;
			int iLine = ts->iLine;
			if( e < ts->zEnd && tok_is_ws(*e) ){
				if( tok_at_nl(e,ts->zEnd) ){ ts->iLine++; }
				e++;                 /* one trailing whitespace char joins the tag */
			}
			tok_tok(ts,T_OPEN_TAG,(const char *)z,(int)(e-z),iLine);
			ts->z = e;
			return 1;
		}
	}
	return 0;
}

/* The scanning driver: alternate inline-HTML and PHP modes over the source. */
static void tok_run(tok_state *ts){
	while( ts->z < ts->zEnd && !ts->bOOM && !ts->bStop ){
		/* Inline HTML until the next open tag. */
		const unsigned char *zHtml = ts->z;
		int iHtmlLine = ts->iLine;
		while( ts->z < ts->zEnd ){
			if( ts->z[0]=='<' && ts->z+1 < ts->zEnd && ts->z[1]=='?' ){
				/* Only <?php and <?= are recognised (short tags off). */
				const unsigned char *s = ts->z;
				int bTag = 0;
				if( s+2 < ts->zEnd && s[2]=='=' ){ bTag = 1; }
				else if( s+4 <= ts->zEnd && tok_lower(s[2])=='p' && tok_lower(s[3])=='h'
					&& s+5 <= ts->zEnd && tok_lower(s[4])=='p'
					&& (s+5 >= ts->zEnd || tok_is_ws(s[5])) ){ bTag = 1; }
				if( bTag ){ break; }
			}
			if( tok_at_nl(ts->z,ts->zEnd) ){ ts->iLine++; }
			ts->z++;
		}
		if( ts->z > zHtml ){
			tok_tok(ts,T_INLINE_HTML,(const char *)zHtml,(int)(ts->z-zHtml),iHtmlLine);
		}
		if( ts->z >= ts->zEnd ){ break; }
		if( !tok_open_tag(ts) ){
			/* Not actually a tag (shouldn't happen given the check above). */
			if( tok_at_nl(ts->z,ts->zEnd) ){ ts->iLine++; }
			ts->z++;
			continue;
		}
		/* PHP mode until a close tag or EOF. */
		ts->bProp = 0;
		while( ts->z < ts->zEnd && !ts->bOOM ){
			int eff = tok_lex_one(ts);
			if( eff < 0 ){ break; }
		}
	}
}

/*
 * array token_get_all(string $source [, int $flags = 0 ])
 */
static int PH7_builtin_token_get_all(ph7_context *pCtx,int nArg,ph7_value **apArg){
	tok_state ts;
	const char *zSrc;
	int nSrc = 0;
	if( nArg < 1 ){
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"token_get_all() expects at least 1 argument, %d given",nArg);
	}
	zSrc = ph7_value_to_string(apArg[0],&nSrc);
	SyZero(&ts,sizeof(ts));
	ts.pCtx  = pCtx;
	ts.pArray = ph7_context_new_array(pCtx);
	ts.pS    = ph7_context_new_scalar(pCtx);
	ts.pId   = ph7_context_new_scalar(pCtx);
	ts.pText = ph7_context_new_scalar(pCtx);
	ts.pLine = ph7_context_new_scalar(pCtx);
	if( ts.pArray==0 || ts.pS==0 || ts.pId==0 || ts.pText==0 || ts.pLine==0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	ts.z    = (const unsigned char *)zSrc;
	ts.zEnd = ts.z + (nSrc > 0 ? (sxu32)nSrc : 0);
	ts.iLine = 1;
	tok_run(&ts);
	if( ts.bOOM ){
		return PH7_ContextMemoryError(pCtx);
	}
	ph7_result_value(pCtx,ts.pArray);
	return PH7_OK;
}

/*
 * string token_name(int $id)
 */
static int PH7_builtin_token_name(ph7_context *pCtx,int nArg,ph7_value **apArg){
	int iId;
	const char *zName;
	if( nArg < 1 ){
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"token_name() expects exactly 1 argument, %d given",nArg);
	}
	iId = ph7_value_to_int(apArg[0]);
	zName = TokConstName(iId);
	if( zName == 0 ){
		ph7_result_string(pCtx,"UNKNOWN",(int)sizeof("UNKNOWN")-1);
	}else{
		ph7_result_string(pCtx,zName,-1/*SyStrlen*/);
	}
	return PH7_OK;
}

/*
 * The PhpToken class: a thin userland wrapper over token_get_all(), tracking
 * byte offset (->pos) and line for the plain single-char tokens.
 */
static const char zPhpTokenClass[] = {
	"final class PhpToken implements Stringable {"
	" public int $id;"
	" public string $text;"
	" public int $line;"
	" public int $pos;"
	" public function __construct(int $id, string $text, int $line = -1, int $pos = -1){"
	"  $this->id = $id; $this->text = $text; $this->line = $line; $this->pos = $pos;"
	" }"
	" public static function tokenize(string $code, int $flags = 0): array {"
	"  $tokens = token_get_all($code, $flags);"
	"  $result = array(); $pos = 0; $line = 1;"
	"  foreach( $tokens as $tok ){"
	"   if( is_array($tok) ){ $id = $tok[0]; $text = $tok[1]; $ln = $tok[2]; }"
	"   else { $id = ord($tok); $text = $tok; $ln = $line; }"
	"   $result[] = new static($id, $text, $ln, $pos);"
	"   $pos += strlen($text);"
	"   $line += substr_count($text, \"\\n\");"
	"  }"
	"  return $result;"
	" }"
	" public function is($kind): bool {"
	"  if( is_array($kind) ){"
	"   foreach( $kind as $k ){"
	"    if( is_string($k) ){ if( $this->text === $k ){ return true; } }"
	"    elseif( $this->id === $k ){ return true; }"
	"   }"
	"   return false;"
	"  }"
	"  if( is_string($kind) ){ return $this->text === $kind; }"
	"  return $this->id === $kind;"
	" }"
	" public function isIgnorable(): bool {"
	"  return $this->id === T_WHITESPACE || $this->id === T_COMMENT"
	"   || $this->id === T_DOC_COMMENT || $this->id === T_OPEN_TAG;"
	" }"
	" public function getTokenName(): ?string {"
	"  if( $this->id < 256 ){ return chr($this->id); }"
	"  $name = token_name($this->id);"
	"  if( $name === 'UNKNOWN' ){ return null; }"
	"  return $name;"
	" }"
	" public function __toString(): string { return (string)$this->text; }"
	"}"
};

PH7_PRIVATE sxi32 PH7_VmInstallTokenizer(ph7_vm *pVm){
	ph7_create_function(&(*pVm),"token_get_all",PH7_builtin_token_get_all,0);
	ph7_create_function(&(*pVm),"token_name",PH7_builtin_token_name,0);
	return PH7_VmEvalBuiltinChunk(&(*pVm),zPhpTokenClass,sizeof(zPhpTokenClass)-1);
}

#else /* PH7_DISABLE_BUILTIN_FUNC */

/* Tiny build: no tokenizer builtins/class (the whole builtin layer is off). */
PH7_PRIVATE sxi32 PH7_VmInstallTokenizer(ph7_vm *pVm){ (void)pVm; return SXRET_OK; }

#endif /* PH7_DISABLE_BUILTIN_FUNC */
