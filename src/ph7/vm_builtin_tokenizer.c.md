# src/ph7/vm_builtin_tokenizer.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 370/936 lines (39.53%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    4 | ` *` |
|      - |    5 | ` * PHP userland tokenizer surface: token_get_all(), token_name(), the T_*` |
|      - |    6 | ` * constant family and the PhpToken class.` |
|      - |    7 | ` *` |
|      - |    8 | ` * This is a purpose-built, self-contained re-scan of the raw source bytes that` |
|      - |    9 | ` * emits EVERY lexeme in source order — including T_WHITESPACE, T_COMMENT,` |
|      - |   10 | ` * T_DOC_COMMENT, T_INLINE_HTML and the open/close tags — exactly as php's` |
|      - |   11 | ` * tokenizer extension does. PHL's compile-time lexer (lex.c) drops whitespace` |
|      - |   12 | ` * and routes comments to a trivia sidecar, so it cannot be reused for this.` |
|      - |   13 | ` *` |
|      - |   14 | ` * The T_* id VALUES are pinned to php 8.5's actual tokenizer constant ints so` |
|      - |   15 | ` * that token_name() round-trips and consumers such as theseer/tokenizer (their` |
|      - |   16 | ` * string->name MAP) work unchanged.` |
|      - |   17 | ` */` |
|      - |   18 | `#include "ph7int.h"` |
|      - |   19 |  |
|      - |   20 | `/*` |
|      - |   21 | ` * php 8.5 tokenizer constant ids. Single source of truth for both the` |
|      - |   22 | ` * registered T_* constants and token_name()'s reverse lookup.` |
|      - |   23 | ` */` |
|      - |   24 | `#define T_LNUMBER                                 260` |
|      - |   25 | `#define T_DNUMBER                                 261` |
|      - |   26 | `#define T_STRING                                  262` |
|      - |   27 | `#define T_NAME_FULLY_QUALIFIED                    263` |
|      - |   28 | `#define T_NAME_RELATIVE                           264` |
|      - |   29 | `#define T_NAME_QUALIFIED                          265` |
|      - |   30 | `#define T_VARIABLE                                266` |
|      - |   31 | `#define T_INLINE_HTML                             267` |
|      - |   32 | `#define T_ENCAPSED_AND_WHITESPACE                 268` |
|      - |   33 | `#define T_CONSTANT_ENCAPSED_STRING                269` |
|      - |   34 | `#define T_STRING_VARNAME                          270` |
|      - |   35 | `#define T_NUM_STRING                              271` |
|      - |   36 | `#define T_INCLUDE                                 272` |
|      - |   37 | `#define T_INCLUDE_ONCE                            273` |
|      - |   38 | `#define T_EVAL                                    274` |
|      - |   39 | `#define T_REQUIRE                                 275` |
|      - |   40 | `#define T_REQUIRE_ONCE                            276` |
|      - |   41 | `#define T_LOGICAL_OR                              277` |
|      - |   42 | `#define T_LOGICAL_XOR                             278` |
|      - |   43 | `#define T_LOGICAL_AND                             279` |
|      - |   44 | `#define T_PRINT                                   280` |
|      - |   45 | `#define T_YIELD                                   281` |
|      - |   46 | `#define T_YIELD_FROM                              282` |
|      - |   47 | `#define T_INSTANCEOF                              283` |
|      - |   48 | `#define T_NEW                                     284` |
|      - |   49 | `#define T_CLONE                                   285` |
|      - |   50 | `#define T_EXIT                                    286` |
|      - |   51 | `#define T_IF                                      287` |
|      - |   52 | `#define T_ELSEIF                                  288` |
|      - |   53 | `#define T_ELSE                                    289` |
|      - |   54 | `#define T_ENDIF                                   290` |
|      - |   55 | `#define T_ECHO                                    291` |
|      - |   56 | `#define T_DO                                      292` |
|      - |   57 | `#define T_WHILE                                   293` |
|      - |   58 | `#define T_ENDWHILE                                294` |
|      - |   59 | `#define T_FOR                                     295` |
|      - |   60 | `#define T_ENDFOR                                  296` |
|      - |   61 | `#define T_FOREACH                                 297` |
|      - |   62 | `#define T_ENDFOREACH                              298` |
|      - |   63 | `#define T_DECLARE                                 299` |
|      - |   64 | `#define T_ENDDECLARE                              300` |
|      - |   65 | `#define T_AS                                      301` |
|      - |   66 | `#define T_SWITCH                                  302` |
|      - |   67 | `#define T_ENDSWITCH                               303` |
|      - |   68 | `#define T_CASE                                    304` |
|      - |   69 | `#define T_DEFAULT                                 305` |
|      - |   70 | `#define T_MATCH                                   306` |
|      - |   71 | `#define T_BREAK                                   307` |
|      - |   72 | `#define T_CONTINUE                                308` |
|      - |   73 | `#define T_GOTO                                    309` |
|      - |   74 | `#define T_FUNCTION                                310` |
|      - |   75 | `#define T_FN                                      311` |
|      - |   76 | `#define T_CONST                                   312` |
|      - |   77 | `#define T_RETURN                                  313` |
|      - |   78 | `#define T_TRY                                     314` |
|      - |   79 | `#define T_CATCH                                   315` |
|      - |   80 | `#define T_FINALLY                                 316` |
|      - |   81 | `#define T_THROW                                   317` |
|      - |   82 | `#define T_USE                                     318` |
|      - |   83 | `#define T_INSTEADOF                               319` |
|      - |   84 | `#define T_GLOBAL                                  320` |
|      - |   85 | `#define T_STATIC                                  321` |
|      - |   86 | `#define T_ABSTRACT                                322` |
|      - |   87 | `#define T_FINAL                                   323` |
|      - |   88 | `#define T_PRIVATE                                 324` |
|      - |   89 | `#define T_PROTECTED                               325` |
|      - |   90 | `#define T_PUBLIC                                  326` |
|      - |   91 | `#define T_PRIVATE_SET                             327` |
|      - |   92 | `#define T_PROTECTED_SET                           328` |
|      - |   93 | `#define T_PUBLIC_SET                              329` |
|      - |   94 | `#define T_READONLY                                330` |
|      - |   95 | `#define T_VAR                                     331` |
|      - |   96 | `#define T_UNSET                                   332` |
|      - |   97 | `#define T_ISSET                                   333` |
|      - |   98 | `#define T_EMPTY                                   334` |
|      - |   99 | `#define T_HALT_COMPILER                           335` |
|      - |  100 | `#define T_CLASS                                   336` |
|      - |  101 | `#define T_TRAIT                                   337` |
|      - |  102 | `#define T_INTERFACE                               338` |
|      - |  103 | `#define T_ENUM                                    339` |
|      - |  104 | `#define T_EXTENDS                                 340` |
|      - |  105 | `#define T_IMPLEMENTS                              341` |
|      - |  106 | `#define T_NAMESPACE                               342` |
|      - |  107 | `#define T_LIST                                    343` |
|      - |  108 | `#define T_ARRAY                                   344` |
|      - |  109 | `#define T_CALLABLE                                345` |
|      - |  110 | `#define T_LINE                                    346` |
|      - |  111 | `#define T_FILE                                    347` |
|      - |  112 | `#define T_DIR                                     348` |
|      - |  113 | `#define T_CLASS_C                                 349` |
|      - |  114 | `#define T_TRAIT_C                                 350` |
|      - |  115 | `#define T_METHOD_C                                351` |
|      - |  116 | `#define T_FUNC_C                                  352` |
|      - |  117 | `#define T_PROPERTY_C                              353` |
|      - |  118 | `#define T_NS_C                                    354` |
|      - |  119 | `#define T_ATTRIBUTE                               355` |
|      - |  120 | `#define T_PLUS_EQUAL                              356` |
|      - |  121 | `#define T_MINUS_EQUAL                             357` |
|      - |  122 | `#define T_MUL_EQUAL                               358` |
|      - |  123 | `#define T_DIV_EQUAL                               359` |
|      - |  124 | `#define T_CONCAT_EQUAL                            360` |
|      - |  125 | `#define T_MOD_EQUAL                               361` |
|      - |  126 | `#define T_AND_EQUAL                               362` |
|      - |  127 | `#define T_OR_EQUAL                                363` |
|      - |  128 | `#define T_XOR_EQUAL                               364` |
|      - |  129 | `#define T_SL_EQUAL                                365` |
|      - |  130 | `#define T_SR_EQUAL                                366` |
|      - |  131 | `#define T_COALESCE_EQUAL                          367` |
|      - |  132 | `#define T_BOOLEAN_OR                              368` |
|      - |  133 | `#define T_BOOLEAN_AND                             369` |
|      - |  134 | `#define T_IS_EQUAL                                370` |
|      - |  135 | `#define T_IS_NOT_EQUAL                            371` |
|      - |  136 | `#define T_IS_IDENTICAL                            372` |
|      - |  137 | `#define T_IS_NOT_IDENTICAL                        373` |
|      - |  138 | `#define T_IS_SMALLER_OR_EQUAL                     374` |
|      - |  139 | `#define T_IS_GREATER_OR_EQUAL                     375` |
|      - |  140 | `#define T_SPACESHIP                               376` |
|      - |  141 | `#define T_SL                                      377` |
|      - |  142 | `#define T_SR                                      378` |
|      - |  143 | `#define T_INC                                     379` |
|      - |  144 | `#define T_DEC                                     380` |
|      - |  145 | `#define T_INT_CAST                                381` |
|      - |  146 | `#define T_DOUBLE_CAST                             382` |
|      - |  147 | `#define T_STRING_CAST                             383` |
|      - |  148 | `#define T_ARRAY_CAST                              384` |
|      - |  149 | `#define T_OBJECT_CAST                             385` |
|      - |  150 | `#define T_BOOL_CAST                               386` |
|      - |  151 | `#define T_UNSET_CAST                              387` |
|      - |  152 | `#define T_VOID_CAST                               388` |
|      - |  153 | `#define T_OBJECT_OPERATOR                         389` |
|      - |  154 | `#define T_NULLSAFE_OBJECT_OPERATOR                390` |
|      - |  155 | `#define T_DOUBLE_ARROW                            391` |
|      - |  156 | `#define T_COMMENT                                 392` |
|      - |  157 | `#define T_DOC_COMMENT                             393` |
|      - |  158 | `#define T_OPEN_TAG                                394` |
|      - |  159 | `#define T_OPEN_TAG_WITH_ECHO                      395` |
|      - |  160 | `#define T_CLOSE_TAG                               396` |
|      - |  161 | `#define T_WHITESPACE                              397` |
|      - |  162 | `#define T_START_HEREDOC                           398` |
|      - |  163 | `#define T_END_HEREDOC                             399` |
|      - |  164 | `#define T_DOLLAR_OPEN_CURLY_BRACES                400` |
|      - |  165 | `#define T_CURLY_OPEN                              401` |
|      - |  166 | `#define T_DOUBLE_COLON                            402` |
|      - |  167 | `#define T_PAAMAYIM_NEKUDOTAYIM                    402` |
|      - |  168 | `#define T_NS_SEPARATOR                            403` |
|      - |  169 | `#define T_ELLIPSIS                                404` |
|      - |  170 | `#define T_COALESCE                                405` |
|      - |  171 | `#define T_POW                                     406` |
|      - |  172 | `#define T_POW_EQUAL                               407` |
|      - |  173 | `#define T_PIPE                                    408` |
|      - |  174 | `#define T_AMPERSAND_FOLLOWED_BY_VAR_OR_VARARG     409` |
|      - |  175 | `#define T_AMPERSAND_NOT_FOLLOWED_BY_VAR_OR_VARARG 410` |
|      - |  176 | `#define T_BAD_CHARACTER                           411` |
|      - |  177 |  |
|      - |  178 | `/* Registered as a plain constant; here for token_name() completeness. */` |
|      - |  179 | `#define TOK_TOKEN_PARSE                             1` |
|      - |  180 |  |
|      - |  181 | `typedef struct tok_const tok_const;` |
|      - |  182 | `struct tok_const { const char *zName; int iId; };` |
|      - |  183 |  |
|      - |  184 | `/*` |
|      - |  185 | ` * All tokenizer constants. token_name()'s reverse lookup uses this table too,` |
|      - |  186 | ` * so the CANONICAL name for a shared id must come first (T_DOUBLE_COLON before` |
|      - |  187 | ` * T_PAAMAYIM_NEKUDOTAYIM for id 402, matching php's token_name()).` |
|      - |  188 | ` */` |
|      - |  189 | `static const tok_const aTokConst[] = {` |
|      - |  190 | `	{ "T_LNUMBER", T_LNUMBER }, { "T_DNUMBER", T_DNUMBER }, { "T_STRING", T_STRING },` |
|      - |  191 | `	{ "T_NAME_FULLY_QUALIFIED", T_NAME_FULLY_QUALIFIED }, { "T_NAME_RELATIVE", T_NAME_RELATIVE },` |
|      - |  192 | `	{ "T_NAME_QUALIFIED", T_NAME_QUALIFIED }, { "T_VARIABLE", T_VARIABLE }, { "T_INLINE_HTML", T_INLINE_HTML },` |
|      - |  193 | `	{ "T_ENCAPSED_AND_WHITESPACE", T_ENCAPSED_AND_WHITESPACE },` |
|      - |  194 | `	{ "T_CONSTANT_ENCAPSED_STRING", T_CONSTANT_ENCAPSED_STRING }, { "T_STRING_VARNAME", T_STRING_VARNAME },` |
|      - |  195 | `	{ "T_NUM_STRING", T_NUM_STRING }, { "T_INCLUDE", T_INCLUDE }, { "T_INCLUDE_ONCE", T_INCLUDE_ONCE },` |
|      - |  196 | `	{ "T_EVAL", T_EVAL }, { "T_REQUIRE", T_REQUIRE }, { "T_REQUIRE_ONCE", T_REQUIRE_ONCE },` |
|      - |  197 | `	{ "T_LOGICAL_OR", T_LOGICAL_OR }, { "T_LOGICAL_XOR", T_LOGICAL_XOR }, { "T_LOGICAL_AND", T_LOGICAL_AND },` |
|      - |  198 | `	{ "T_PRINT", T_PRINT }, { "T_YIELD", T_YIELD }, { "T_YIELD_FROM", T_YIELD_FROM },` |
|      - |  199 | `	{ "T_INSTANCEOF", T_INSTANCEOF }, { "T_NEW", T_NEW }, { "T_CLONE", T_CLONE }, { "T_EXIT", T_EXIT },` |
|      - |  200 | `	{ "T_IF", T_IF }, { "T_ELSEIF", T_ELSEIF }, { "T_ELSE", T_ELSE }, { "T_ENDIF", T_ENDIF },` |
|      - |  201 | `	{ "T_ECHO", T_ECHO }, { "T_DO", T_DO }, { "T_WHILE", T_WHILE }, { "T_ENDWHILE", T_ENDWHILE },` |
|      - |  202 | `	{ "T_FOR", T_FOR }, { "T_ENDFOR", T_ENDFOR }, { "T_FOREACH", T_FOREACH }, { "T_ENDFOREACH", T_ENDFOREACH },` |
|      - |  203 | `	{ "T_DECLARE", T_DECLARE }, { "T_ENDDECLARE", T_ENDDECLARE }, { "T_AS", T_AS }, { "T_SWITCH", T_SWITCH },` |
|      - |  204 | `	{ "T_ENDSWITCH", T_ENDSWITCH }, { "T_CASE", T_CASE }, { "T_DEFAULT", T_DEFAULT }, { "T_MATCH", T_MATCH },` |
|      - |  205 | `	{ "T_BREAK", T_BREAK }, { "T_CONTINUE", T_CONTINUE }, { "T_GOTO", T_GOTO }, { "T_FUNCTION", T_FUNCTION },` |
|      - |  206 | `	{ "T_FN", T_FN }, { "T_CONST", T_CONST }, { "T_RETURN", T_RETURN }, { "T_TRY", T_TRY },` |
|      - |  207 | `	{ "T_CATCH", T_CATCH }, { "T_FINALLY", T_FINALLY }, { "T_THROW", T_THROW }, { "T_USE", T_USE },` |
|      - |  208 | `	{ "T_INSTEADOF", T_INSTEADOF }, { "T_GLOBAL", T_GLOBAL }, { "T_STATIC", T_STATIC },` |
|      - |  209 | `	{ "T_ABSTRACT", T_ABSTRACT }, { "T_FINAL", T_FINAL }, { "T_PRIVATE", T_PRIVATE },` |
|      - |  210 | `	{ "T_PROTECTED", T_PROTECTED }, { "T_PUBLIC", T_PUBLIC }, { "T_PRIVATE_SET", T_PRIVATE_SET },` |
|      - |  211 | `	{ "T_PROTECTED_SET", T_PROTECTED_SET }, { "T_PUBLIC_SET", T_PUBLIC_SET }, { "T_READONLY", T_READONLY },` |
|      - |  212 | `	{ "T_VAR", T_VAR }, { "T_UNSET", T_UNSET }, { "T_ISSET", T_ISSET }, { "T_EMPTY", T_EMPTY },` |
|      - |  213 | `	{ "T_HALT_COMPILER", T_HALT_COMPILER }, { "T_CLASS", T_CLASS }, { "T_TRAIT", T_TRAIT },` |
|      - |  214 | `	{ "T_INTERFACE", T_INTERFACE }, { "T_ENUM", T_ENUM }, { "T_EXTENDS", T_EXTENDS },` |
|      - |  215 | `	{ "T_IMPLEMENTS", T_IMPLEMENTS }, { "T_NAMESPACE", T_NAMESPACE }, { "T_LIST", T_LIST },` |
|      - |  216 | `	{ "T_ARRAY", T_ARRAY }, { "T_CALLABLE", T_CALLABLE }, { "T_LINE", T_LINE }, { "T_FILE", T_FILE },` |
|      - |  217 | `	{ "T_DIR", T_DIR }, { "T_CLASS_C", T_CLASS_C }, { "T_TRAIT_C", T_TRAIT_C }, { "T_METHOD_C", T_METHOD_C },` |
|      - |  218 | `	{ "T_FUNC_C", T_FUNC_C }, { "T_PROPERTY_C", T_PROPERTY_C }, { "T_NS_C", T_NS_C },` |
|      - |  219 | `	{ "T_ATTRIBUTE", T_ATTRIBUTE }, { "T_PLUS_EQUAL", T_PLUS_EQUAL }, { "T_MINUS_EQUAL", T_MINUS_EQUAL },` |
|      - |  220 | `	{ "T_MUL_EQUAL", T_MUL_EQUAL }, { "T_DIV_EQUAL", T_DIV_EQUAL }, { "T_CONCAT_EQUAL", T_CONCAT_EQUAL },` |
|      - |  221 | `	{ "T_MOD_EQUAL", T_MOD_EQUAL }, { "T_AND_EQUAL", T_AND_EQUAL }, { "T_OR_EQUAL", T_OR_EQUAL },` |
|      - |  222 | `	{ "T_XOR_EQUAL", T_XOR_EQUAL }, { "T_SL_EQUAL", T_SL_EQUAL }, { "T_SR_EQUAL", T_SR_EQUAL },` |
|      - |  223 | `	{ "T_COALESCE_EQUAL", T_COALESCE_EQUAL }, { "T_BOOLEAN_OR", T_BOOLEAN_OR },` |
|      - |  224 | `	{ "T_BOOLEAN_AND", T_BOOLEAN_AND }, { "T_IS_EQUAL", T_IS_EQUAL }, { "T_IS_NOT_EQUAL", T_IS_NOT_EQUAL },` |
|      - |  225 | `	{ "T_IS_IDENTICAL", T_IS_IDENTICAL }, { "T_IS_NOT_IDENTICAL", T_IS_NOT_IDENTICAL },` |
|      - |  226 | `	{ "T_IS_SMALLER_OR_EQUAL", T_IS_SMALLER_OR_EQUAL }, { "T_IS_GREATER_OR_EQUAL", T_IS_GREATER_OR_EQUAL },` |
|      - |  227 | `	{ "T_SPACESHIP", T_SPACESHIP }, { "T_SL", T_SL }, { "T_SR", T_SR }, { "T_INC", T_INC }, { "T_DEC", T_DEC },` |
|      - |  228 | `	{ "T_INT_CAST", T_INT_CAST }, { "T_DOUBLE_CAST", T_DOUBLE_CAST }, { "T_STRING_CAST", T_STRING_CAST },` |
|      - |  229 | `	{ "T_ARRAY_CAST", T_ARRAY_CAST }, { "T_OBJECT_CAST", T_OBJECT_CAST }, { "T_BOOL_CAST", T_BOOL_CAST },` |
|      - |  230 | `	{ "T_UNSET_CAST", T_UNSET_CAST }, { "T_VOID_CAST", T_VOID_CAST }, { "T_OBJECT_OPERATOR", T_OBJECT_OPERATOR },` |
|      - |  231 | `	{ "T_NULLSAFE_OBJECT_OPERATOR", T_NULLSAFE_OBJECT_OPERATOR }, { "T_DOUBLE_ARROW", T_DOUBLE_ARROW },` |
|      - |  232 | `	{ "T_COMMENT", T_COMMENT }, { "T_DOC_COMMENT", T_DOC_COMMENT }, { "T_OPEN_TAG", T_OPEN_TAG },` |
|      - |  233 | `	{ "T_OPEN_TAG_WITH_ECHO", T_OPEN_TAG_WITH_ECHO }, { "T_CLOSE_TAG", T_CLOSE_TAG },` |
|      - |  234 | `	{ "T_WHITESPACE", T_WHITESPACE }, { "T_START_HEREDOC", T_START_HEREDOC }, { "T_END_HEREDOC", T_END_HEREDOC },` |
|      - |  235 | `	{ "T_DOLLAR_OPEN_CURLY_BRACES", T_DOLLAR_OPEN_CURLY_BRACES }, { "T_CURLY_OPEN", T_CURLY_OPEN },` |
|      - |  236 | `	{ "T_DOUBLE_COLON", T_DOUBLE_COLON }, { "T_PAAMAYIM_NEKUDOTAYIM", T_PAAMAYIM_NEKUDOTAYIM },` |
|      - |  237 | `	{ "T_NS_SEPARATOR", T_NS_SEPARATOR }, { "T_ELLIPSIS", T_ELLIPSIS }, { "T_COALESCE", T_COALESCE },` |
|      - |  238 | `	{ "T_POW", T_POW }, { "T_POW_EQUAL", T_POW_EQUAL }, { "T_PIPE", T_PIPE },` |
|      - |  239 | `	{ "T_AMPERSAND_FOLLOWED_BY_VAR_OR_VARARG", T_AMPERSAND_FOLLOWED_BY_VAR_OR_VARARG },` |
|      - |  240 | `	{ "T_AMPERSAND_NOT_FOLLOWED_BY_VAR_OR_VARARG", T_AMPERSAND_NOT_FOLLOWED_BY_VAR_OR_VARARG },` |
|      - |  241 | `	{ "T_BAD_CHARACTER", T_BAD_CHARACTER },` |
|      - |  242 | `	{ "TOKEN_PARSE", TOK_TOKEN_PARSE },` |
|      - |  243 | `};` |
|      - |  244 |  |
|      - |  245 | `/*` |
|      - |  246 | ` * The expander shared by every registered T_* constant: pUserData carries the` |
|      - |  247 | ` * integer value directly (SX_INT_TO_PTR at registration time).` |
|      - |  248 | ` */` |
|      8 |  249 | `static void TokConstExpand(ph7_value *pVal,void *pUserData)` |
|      1 |  250 | `{` |
|      9 |  251 | `	ph7_value_int(pVal,SX_PTR_TO_INT(pUserData));` |
|      9 |  252 | `}` |
|      - |  253 |  |
|   4076 |  254 | `PH7_PRIVATE void PH7_RegisterTokenizerConstants(ph7_vm *pVm)` |
|      5 |  255 | `{` |
|      - |  256 | `	sxu32 n;` |
| 631785 |  257 | `	for( n = 0 ; n < SX_ARRAYSIZE(aTokConst) ; ++n ){` |
| 941561 |  258 | `		ph7_create_constant(&(*pVm),aTokConst[n].zName,TokConstExpand,` |
| 627704 |  259 | `			SX_INT_TO_PTR(aTokConst[n].iId));` |
| 313857 |  260 | `	}` |
|   4081 |  261 | `}` |
|      - |  262 |  |
|      - |  263 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |  264 |  |
|      - |  265 | `/*` |
|      - |  266 | ` * Reverse lookup for token_name(): the canonical php name for an id, or 0 for` |
|      - |  267 | ` * unknown/out-of-range ids (token_name() then returns "UNKNOWN").` |
|      - |  268 | ` */` |
|     12 |  269 | `static const char * TokConstName(int iId)` |
|      1 |  270 | `{` |
|      - |  271 | `	sxu32 n;` |
|     13 |  272 | `	if( iId < 256 ){` |
|    ! 0 |  273 | `		return 0; /* single-char and TOKEN_PARSE ids are "UNKNOWN" in token_name() */` |
|      - |  274 | `	}` |
|   1115 |  275 | `	for( n = 0 ; n < SX_ARRAYSIZE(aTokConst) ; ++n ){` |
|   1115 |  276 | `		if( aTokConst[n].iId == iId ){` |
|     13 |  277 | `			return aTokConst[n].zName;` |
|      - |  278 | `		}` |
|    552 |  279 | `	}` |
|    ! 0 |  280 | `	return 0;` |
|      7 |  281 | `}` |
|      - |  282 |  |
|      - |  283 | `/*` |
|      - |  284 | ` * Case-insensitive identifier -> keyword id. A lone identifier (no backslash)` |
|      - |  285 | ` * matching one of these becomes the mapped T_* token; anything else is` |
|      - |  286 | ` * T_STRING. 'enum' is deliberately absent — it is contextual (see the scanner).` |
|      - |  287 | ` */` |
|      - |  288 | `typedef struct tok_kw tok_kw;` |
|      - |  289 | `struct tok_kw { const char *zName; int iId; };` |
|      - |  290 | `static const tok_kw aKeyword[] = {` |
|      - |  291 | `	{ "include", T_INCLUDE }, { "include_once", T_INCLUDE_ONCE }, { "eval", T_EVAL },` |
|      - |  292 | `	{ "require", T_REQUIRE }, { "require_once", T_REQUIRE_ONCE }, { "or", T_LOGICAL_OR },` |
|      - |  293 | `	{ "xor", T_LOGICAL_XOR }, { "and", T_LOGICAL_AND }, { "print", T_PRINT }, { "yield", T_YIELD },` |
|      - |  294 | `	{ "instanceof", T_INSTANCEOF }, { "new", T_NEW }, { "clone", T_CLONE }, { "exit", T_EXIT },` |
|      - |  295 | `	{ "die", T_EXIT }, { "if", T_IF }, { "elseif", T_ELSEIF }, { "else", T_ELSE }, { "endif", T_ENDIF },` |
|      - |  296 | `	{ "echo", T_ECHO }, { "do", T_DO }, { "while", T_WHILE }, { "endwhile", T_ENDWHILE }, { "for", T_FOR },` |
|      - |  297 | `	{ "endfor", T_ENDFOR }, { "foreach", T_FOREACH }, { "endforeach", T_ENDFOREACH }, { "declare", T_DECLARE },` |
|      - |  298 | `	{ "enddeclare", T_ENDDECLARE }, { "as", T_AS }, { "switch", T_SWITCH }, { "endswitch", T_ENDSWITCH },` |
|      - |  299 | `	{ "case", T_CASE }, { "default", T_DEFAULT }, { "match", T_MATCH }, { "break", T_BREAK },` |
|      - |  300 | `	{ "continue", T_CONTINUE }, { "goto", T_GOTO }, { "function", T_FUNCTION }, { "fn", T_FN },` |
|      - |  301 | `	{ "const", T_CONST }, { "return", T_RETURN }, { "try", T_TRY }, { "catch", T_CATCH },` |
|      - |  302 | `	{ "finally", T_FINALLY }, { "throw", T_THROW }, { "use", T_USE }, { "insteadof", T_INSTEADOF },` |
|      - |  303 | `	{ "global", T_GLOBAL }, { "static", T_STATIC }, { "abstract", T_ABSTRACT }, { "final", T_FINAL },` |
|      - |  304 | `	{ "private", T_PRIVATE }, { "protected", T_PROTECTED }, { "public", T_PUBLIC }, { "readonly", T_READONLY },` |
|      - |  305 | `	{ "var", T_VAR }, { "unset", T_UNSET }, { "isset", T_ISSET }, { "empty", T_EMPTY },` |
|      - |  306 | `	{ "__halt_compiler", T_HALT_COMPILER }, { "class", T_CLASS }, { "trait", T_TRAIT },` |
|      - |  307 | `	{ "interface", T_INTERFACE }, { "extends", T_EXTENDS }, { "implements", T_IMPLEMENTS },` |
|      - |  308 | `	{ "namespace", T_NAMESPACE }, { "list", T_LIST }, { "array", T_ARRAY }, { "callable", T_CALLABLE },` |
|      - |  309 | `	{ "__line__", T_LINE }, { "__file__", T_FILE }, { "__dir__", T_DIR }, { "__class__", T_CLASS_C },` |
|      - |  310 | `	{ "__trait__", T_TRAIT_C }, { "__method__", T_METHOD_C }, { "__function__", T_FUNC_C },` |
|      - |  311 | `	{ "__namespace__", T_NS_C }, { "__property__", T_PROPERTY_C },` |
|      - |  312 | `};` |
|      - |  313 |  |
|      - |  314 | `/* Tokenizing state carried through the whole scan. */` |
|      - |  315 | `typedef struct tok_state tok_state;` |
|      - |  316 | `struct tok_state {` |
|      - |  317 | `	ph7_context *pCtx;` |
|      - |  318 | `	ph7_value   *pArray;   /* output array being built */` |
|      - |  319 | `	ph7_class   *pTokClass;/* PhpToken::tokenize(): build INSTANCES of this class instead of` |
|      - |  320 | `	                        * php's [id,text,line] tuples (php's own add_token branch) */` |
|      - |  321 | ``	int          iPos;     /* byte offset of the next token, php's `text - yy_start` */`` |
|      - |  322 | `	ph7_value   *pS;       /* reusable scalar for plain single-char string tokens */` |
|      - |  323 | `	ph7_value   *pId;      /* reusable scalar: tuple field [0] */` |
|      - |  324 | `	ph7_value   *pText;    /* reusable scalar: tuple field [1] */` |
|      - |  325 | `	ph7_value   *pLine;    /* reusable scalar: tuple field [2] */` |
|      - |  326 | `	const unsigned char *z;      /* cursor */` |
|      - |  327 | `	const unsigned char *zEnd;   /* one past end */` |
|      - |  328 | `	int          iLine;    /* current 1-based line at the cursor */` |
|      - |  329 | `	int          bProp;    /* one-shot: next identifier is a member name -> T_STRING */` |
|      - |  330 | `	int          bCaseName;/* one-shot: next identifier may be an enum case name (after 'case') */` |
|      - |  331 | `	int          bParse;   /* TOKEN_PARSE flag: apply php's semi-reserved re-tagging */` |
|      - |  332 | `	int          bStop;    /* __halt_compiler seen: stop scanning entirely */` |
|      - |  333 | `	int          bOOM;     /* memory failure flag */` |
|      - |  334 | `};` |
|      - |  335 |  |
|      - |  336 | `/* --- character classes (byte-level, UTF-8 lead bytes count as label chars) --- */` |
|     97 |  337 | `static int tok_is_label_start(int c){` |
|     97 |  338 | `	return c=='_' \|\| (c>='a'&&c<='z') \|\| (c>='A'&&c<='Z') \|\| c>=0x80;` |
|      1 |  339 | `}` |
|     33 |  340 | `static int tok_is_label(int c){` |
|     33 |  341 | `	return tok_is_label_start(c) \|\| (c>='0'&&c<='9');` |
|      1 |  342 | `}` |
|    241 |  343 | `static int tok_is_ws(int c){` |
|      - |  344 | `	/* php's tokenizer whitespace is exactly [ \t\r\n]; \v and \f are T_BAD_CHARACTER. */` |
|    241 |  345 | `	return c==' '\|\|c=='\t'\|\|c=='\n'\|\|c=='\r';` |
|      1 |  346 | `}` |
|     97 |  347 | `static int tok_lower(int c){` |
|     97 |  348 | `	return (c>='A'&&c<='Z') ? c+32 : c;` |
|      1 |  349 | `}` |
|    ! 0 |  350 | `static int tok_ci_eq(const char *z,int n,const char *zKw){` |
|      - |  351 | `	int i;` |
|    ! 0 |  352 | `	for( i = 0 ; i < n ; ++i ){` |
|    ! 0 |  353 | `		if( zKw[i]==0 \|\| tok_lower((unsigned char)z[i]) != (unsigned char)zKw[i] ){` |
|    ! 0 |  354 | `			return 0;` |
|      - |  355 | `		}` |
|    ! 0 |  356 | `	}` |
|    ! 0 |  357 | `	return zKw[n]==0;` |
|    ! 0 |  358 | `}` |
|      - |  359 |  |
|      - |  360 | `/* php counts a line ending as any of "\n", "\r", or "\r\n" (each = one line). */` |
|     65 |  361 | `static int tok_at_nl(const unsigned char *p,const unsigned char *zEnd){` |
|     65 |  362 | `	if( *p=='\n' ){ return 1; }` |
|     49 |  363 | `	if( *p=='\r' && (p+1 >= zEnd \|\| p[1]!='\n') ){ return 1; }` |
|     49 |  364 | `	return 0;` |
|     33 |  365 | `}` |
|      - |  366 |  |
|      - |  367 | `/* Count line endings in [z,zEnd) and advance the running line counter. */` |
|    ! 0 |  368 | `static void tok_bump_lines(tok_state *ts,const unsigned char *z,const unsigned char *zEnd){` |
|    ! 0 |  369 | `	while( z < zEnd ){` |
|    ! 0 |  370 | `		if( tok_at_nl(z,zEnd) ){ ts->iLine++; }` |
|    ! 0 |  371 | `		z++;` |
|    ! 0 |  372 | `	}` |
|    ! 0 |  373 | `}` |
|      - |  374 |  |
|      - |  375 | `/* Emit a plain-string token (single-char / operator returned as a bare string). */` |
|      - |  376 | `/*` |
|      - |  377 | ` * One PhpToken (php's add_token with a token_class): the four slots are written` |
|      - |  378 | ` * DIRECTLY, without running the constructor -- which php can do because the` |
|      - |  379 | ` * constructor is final, and which is why a subclass keeps its own declared` |
|      - |  380 | ` * defaults (the instance starts from the class's property table). The line is` |
|      - |  381 | ` * the scanner's, and the position is the running byte offset, php's` |
|      - |  382 | `` * `text - yy_start`.`` |
|      - |  383 | ` */` |
|    128 |  384 | `static void tok_object(tok_state *ts,int iId,const char *z,int n,int iLine)` |
|      1 |  385 | `{` |
|    129 |  386 | `	ph7_vm *pVm = ts->pCtx->pVm;` |
|    129 |  387 | `	ph7_class_instance *pObj = PH7_NewClassInstance(pVm,ts->pTokClass);` |
|      - |  388 | `	ph7_value sVal;` |
|    129 |  389 | `	if( pObj == 0 ){` |
|    ! 0 |  390 | `		ts->bOOM = 1;` |
|    ! 0 |  391 | `		return;` |
|      - |  392 | `	}` |
|    129 |  393 | `	pObj->iRef++;` |
|    129 |  394 | `	PH7_NativeSetAttrInt(pVm,pObj,"id",iId);` |
|    129 |  395 | `	PH7_NativeSetAttrStr(pVm,pObj,"text",z,n);` |
|    129 |  396 | `	PH7_NativeSetAttrInt(pVm,pObj,"line",iLine);` |
|    129 |  397 | `	PH7_NativeSetAttrInt(pVm,pObj,"pos",ts->iPos);` |
|    129 |  398 | `	PH7_MemObjInit(pVm,&sVal);` |
|    129 |  399 | `	sVal.x.pOther = pObj;` |
|    129 |  400 | `	MemObjSetType(&sVal,MEMOBJ_OBJ);` |
|    129 |  401 | `	if( ph7_array_add_elem(ts->pArray,0,&sVal) != SXRET_OK ){   /* takes its OWN reference */` |
|    ! 0 |  402 | `		ts->bOOM = 1;` |
|    ! 0 |  403 | `	}` |
|    129 |  404 | `	PH7_MemObjRelease(&sVal);` |
|    129 |  405 | `	PH7_ClassInstanceUnref(pObj);   /* drop the creation reference (rule 16) */` |
|     65 |  406 | `}` |
|      - |  407 | `/* Every emitted lexeme advances the byte offset; the stream covers the source` |
|      - |  408 | ` * contiguously, which is what makes the running count equal php's pointer` |
|      - |  409 | ` * arithmetic (asserted by the text-roundtrip probe). */` |
|     33 |  410 | `static void tok_plain(tok_state *ts,const char *z,int n){` |
|     33 |  411 | `	if( ts->bOOM ){ return; }` |
|     33 |  412 | `	if( ts->pTokClass ){` |
|     33 |  413 | `		tok_object(ts,(unsigned char)z[0],z,n,ts->iLine);` |
|     33 |  414 | `		ts->iPos += n;` |
|     33 |  415 | `		return;` |
|      - |  416 | `	}` |
|    ! 0 |  417 | `	ph7_value_string(ts->pS,z,n);` |
|    ! 0 |  418 | `	if( ph7_array_add_elem(ts->pArray,0,ts->pS) != SXRET_OK ){ ts->bOOM = 1; }` |
|    ! 0 |  419 | `	ph7_value_reset_string_cursor(ts->pS);` |
|    ! 0 |  420 | `	ts->iPos += n;` |
|     17 |  421 | `}` |
|      - |  422 |  |
|      - |  423 | `/* Emit a [id, text, line] token. */` |
|     97 |  424 | `static void tok_tok(tok_state *ts,int iId,const char *z,int n,int iLine){` |
|      - |  425 | `	ph7_value *pInner;` |
|     97 |  426 | `	if( ts->bOOM ){ return; }` |
|     97 |  427 | `	if( ts->pTokClass ){` |
|     97 |  428 | `		tok_object(ts,iId,z,n,iLine);` |
|     97 |  429 | `		ts->iPos += n;` |
|     97 |  430 | `		return;` |
|      - |  431 | `	}` |
|    ! 0 |  432 | `	ts->iPos += n;` |
|    ! 0 |  433 | `	pInner = ph7_context_new_array(ts->pCtx);` |
|    ! 0 |  434 | `	if( pInner == 0 ){ ts->bOOM = 1; return; }` |
|    ! 0 |  435 | `	ph7_value_int(ts->pId,iId);` |
|    ! 0 |  436 | `	ph7_value_string(ts->pText,z,n);` |
|    ! 0 |  437 | `	ph7_value_int(ts->pLine,iLine);` |
|    ! 0 |  438 | `	ph7_array_add_elem(pInner,0,ts->pId);` |
|    ! 0 |  439 | `	ph7_array_add_elem(pInner,0,ts->pText);` |
|    ! 0 |  440 | `	ph7_array_add_elem(pInner,0,ts->pLine);` |
|    ! 0 |  441 | `	if( ph7_array_add_elem(ts->pArray,0,pInner) != SXRET_OK ){ ts->bOOM = 1; }` |
|    ! 0 |  442 | `	ph7_context_release_value(ts->pCtx,pInner);` |
|    ! 0 |  443 | `	ph7_value_reset_string_cursor(ts->pText);` |
|     49 |  444 | `}` |
|      - |  445 |  |
|      - |  446 | `/* Emit an encapsed-and-whitespace run [zStart, z) if non-empty, at iLine. */` |
|    ! 0 |  447 | `static void tok_encaps(tok_state *ts,const unsigned char *zStart,const unsigned char *z,int iLine){` |
|    ! 0 |  448 | `	if( z > zStart ){` |
|    ! 0 |  449 | `		tok_tok(ts,T_ENCAPSED_AND_WHITESPACE,(const char *)zStart,(int)(z-zStart),iLine);` |
|    ! 0 |  450 | `	}` |
|    ! 0 |  451 | `}` |
|      - |  452 |  |
|      - |  453 | `/* Does the integer literal (digits between z and zEnd, given radix) overflow` |
|      - |  454 | ` * ZEND_LONG_MAX? Underscores are ignored. */` |
|     17 |  455 | `static int tok_int_overflows(const unsigned char *z,const unsigned char *zEnd,int radix){` |
|     17 |  456 | `	sxu64 val = 0;` |
|     17 |  457 | `	const sxu64 max = (sxu64)0x7FFFFFFFFFFFFFFF; /* PHP_INT_MAX */` |
|     33 |  458 | `	while( z < zEnd ){` |
|     17 |  459 | `		int c = *z++;` |
|      - |  460 | `		int d;` |
|     17 |  461 | `		if( c=='_' ){ continue; }` |
|     17 |  462 | `		if( c>='0'&&c<='9' ){ d = c-'0'; }` |
|    ! 0 |  463 | `		else if( c>='a'&&c<='f' ){ d = c-'a'+10; }` |
|    ! 0 |  464 | `		else if( c>='A'&&c<='F' ){ d = c-'A'+10; }` |
|    ! 0 |  465 | `		else { break; }` |
|     17 |  466 | `		if( d >= radix ){ break; }` |
|     17 |  467 | `		if( val > (max - (sxu64)d)/(sxu64)radix ){` |
|    ! 0 |  468 | `			return 1;` |
|      - |  469 | `		}` |
|     17 |  470 | `		val = val*(sxu64)radix + (sxu64)d;` |
|      1 |  471 | `	}` |
|     17 |  472 | `	return 0;` |
|      9 |  473 | `}` |
|      - |  474 |  |
|      - |  475 | `/* Forward decls for the mutually-recursive string / expression scanners. */` |
|      - |  476 | `static int  tok_lex_one(tok_state *ts);` |
|      - |  477 | `static void tok_scan_curly(tok_state *ts,int bVarname);` |
|      - |  478 |  |
|      - |  479 | `/*` |
|      - |  480 | ` * After a T_HALT_COMPILER token: php emits the "();" that follows as normal` |
|      - |  481 | ` * tokens and then the entire remainder of the source as a single T_INLINE_HTML.` |
|      - |  482 | ` */` |
|    ! 0 |  483 | `static void tok_halt_tail(tok_state *ts){` |
|    ! 0 |  484 | `	for(;;){` |
|    ! 0 |  485 | `		if( ts->z >= ts->zEnd ){ break; }` |
|    ! 0 |  486 | `		if( tok_is_ws(*ts->z) ){` |
|    ! 0 |  487 | `			const unsigned char *z0 = ts->z;` |
|    ! 0 |  488 | `			int iLine = ts->iLine;` |
|    ! 0 |  489 | `			while( ts->z < ts->zEnd && tok_is_ws(*ts->z) ){` |
|    ! 0 |  490 | `				if( tok_at_nl(ts->z,ts->zEnd) ){ ts->iLine++; }` |
|    ! 0 |  491 | `				ts->z++;` |
|    ! 0 |  492 | `			}` |
|    ! 0 |  493 | `			tok_tok(ts,T_WHITESPACE,(const char *)z0,(int)(ts->z-z0),iLine);` |
|    ! 0 |  494 | `			continue;` |
|      - |  495 | `		}` |
|    ! 0 |  496 | `		if( *ts->z=='(' \|\| *ts->z==')' ){` |
|    ! 0 |  497 | `			char ch = (char)*ts->z;` |
|    ! 0 |  498 | `			tok_plain(ts,&ch,1);` |
|    ! 0 |  499 | `			ts->z++;` |
|    ! 0 |  500 | `			continue;` |
|      - |  501 | `		}` |
|    ! 0 |  502 | `		if( *ts->z==';' ){` |
|    ! 0 |  503 | `			tok_plain(ts,";",1);` |
|    ! 0 |  504 | `			ts->z++;` |
|    ! 0 |  505 | `			break;` |
|      - |  506 | `		}` |
|    ! 0 |  507 | `		break;` |
|    ! 0 |  508 | `	}` |
|    ! 0 |  509 | `	if( ts->z < ts->zEnd ){` |
|    ! 0 |  510 | `		int iLine = ts->iLine;` |
|    ! 0 |  511 | `		tok_tok(ts,T_INLINE_HTML,(const char *)ts->z,(int)(ts->zEnd-ts->z),iLine);` |
|    ! 0 |  512 | `		tok_bump_lines(ts,ts->z,ts->zEnd);` |
|    ! 0 |  513 | `		ts->z = ts->zEnd;` |
|    ! 0 |  514 | `	}` |
|    ! 0 |  515 | `	ts->bStop = 1;` |
|    ! 0 |  516 | `}` |
|      - |  517 |  |
|      - |  518 | `/*` |
|      - |  519 | ` * Handle a possible interpolation construct at the cursor inside a double-quoted` |
|      - |  520 | ` * string or heredoc body. Returns 1 if it consumed one (emitting tokens), else 0` |
|      - |  521 | ` * (the caller keeps the char as literal). *pFlush is the start of the pending` |
|      - |  522 | ` * literal run and its line; on a hit we flush it first.` |
|      - |  523 | ` */` |
|    ! 0 |  524 | `static int tok_try_interp(tok_state *ts,const unsigned char **pLitStart,int *pLitLine){` |
|    ! 0 |  525 | `	const unsigned char *z = ts->z;` |
|    ! 0 |  526 | `	if( *z=='$' && z+1 < ts->zEnd && tok_is_label_start(z[1]) ){` |
|    ! 0 |  527 | `		const unsigned char *zVar = z+1;` |
|    ! 0 |  528 | `		int iLine = ts->iLine;` |
|    ! 0 |  529 | `		while( zVar < ts->zEnd && tok_is_label(*zVar) ){ zVar++; }` |
|    ! 0 |  530 | `		tok_encaps(ts,*pLitStart,z,*pLitLine);` |
|    ! 0 |  531 | `		tok_tok(ts,T_VARIABLE,(const char *)z,(int)(zVar-z),iLine);` |
|    ! 0 |  532 | `		ts->z = zVar;` |
|      - |  533 | `		/* one optional simple offset [...] or ->prop */` |
|    ! 0 |  534 | `		if( ts->z < ts->zEnd && *ts->z=='[' ){` |
|    ! 0 |  535 | `			tok_plain(ts,"[",1);` |
|    ! 0 |  536 | `			ts->z++;` |
|    ! 0 |  537 | `			if( ts->z < ts->zEnd && *ts->z=='$' && ts->z+1 < ts->zEnd && tok_is_label_start(ts->z[1]) ){` |
|    ! 0 |  538 | `				const unsigned char *v = ts->z+1;` |
|    ! 0 |  539 | `				while( v < ts->zEnd && tok_is_label(*v) ){ v++; }` |
|    ! 0 |  540 | `				tok_tok(ts,T_VARIABLE,(const char *)ts->z,(int)(v-ts->z),ts->iLine);` |
|    ! 0 |  541 | `				ts->z = v;` |
|    ! 0 |  542 | `			}else{` |
|    ! 0 |  543 | `				const unsigned char *n0 = ts->z;` |
|    ! 0 |  544 | `				if( ts->z < ts->zEnd && *ts->z=='-' ){` |
|    ! 0 |  545 | `					tok_plain(ts,"-",1);` |
|    ! 0 |  546 | `					ts->z++;` |
|    ! 0 |  547 | `					n0 = ts->z;` |
|    ! 0 |  548 | `				}` |
|    ! 0 |  549 | `				if( ts->z < ts->zEnd && *ts->z>='0' && *ts->z<='9' ){` |
|    ! 0 |  550 | `					while( ts->z < ts->zEnd && *ts->z>='0' && *ts->z<='9' ){ ts->z++; }` |
|    ! 0 |  551 | `					tok_tok(ts,T_NUM_STRING,(const char *)n0,(int)(ts->z-n0),ts->iLine);` |
|    ! 0 |  552 | `				}else if( ts->z < ts->zEnd && tok_is_label_start(*ts->z) ){` |
|    ! 0 |  553 | `					const unsigned char *l = ts->z;` |
|    ! 0 |  554 | `					while( l < ts->zEnd && tok_is_label(*l) ){ l++; }` |
|    ! 0 |  555 | `					tok_tok(ts,T_STRING,(const char *)ts->z,(int)(l-ts->z),ts->iLine);` |
|    ! 0 |  556 | `					ts->z = l;` |
|    ! 0 |  557 | `				}` |
|      - |  558 | `			}` |
|    ! 0 |  559 | `			if( ts->z < ts->zEnd && *ts->z==']' ){ tok_plain(ts,"]",1); ts->z++; }` |
|    ! 0 |  560 | `		}else if( ts->z+2 < ts->zEnd && ts->z[0]=='-' && ts->z[1]=='>' && tok_is_label_start(ts->z[2]) ){` |
|      - |  561 | `			const unsigned char *l;` |
|    ! 0 |  562 | `			tok_tok(ts,T_OBJECT_OPERATOR,"->",2,ts->iLine);` |
|    ! 0 |  563 | `			ts->z += 2;` |
|    ! 0 |  564 | `			l = ts->z;` |
|    ! 0 |  565 | `			while( l < ts->zEnd && tok_is_label(*l) ){ l++; }` |
|    ! 0 |  566 | `			tok_tok(ts,T_STRING,(const char *)ts->z,(int)(l-ts->z),ts->iLine);` |
|    ! 0 |  567 | `			ts->z = l;` |
|    ! 0 |  568 | `		}` |
|    ! 0 |  569 | `		*pLitStart = ts->z;` |
|    ! 0 |  570 | `		*pLitLine  = ts->iLine;` |
|    ! 0 |  571 | `		return 1;` |
|      - |  572 | `	}` |
|    ! 0 |  573 | `	if( *z=='{' && z+1 < ts->zEnd && z[1]=='$' ){` |
|    ! 0 |  574 | `		tok_encaps(ts,*pLitStart,z,*pLitLine);` |
|    ! 0 |  575 | `		tok_tok(ts,T_CURLY_OPEN,"{",1,ts->iLine);` |
|    ! 0 |  576 | `		ts->z = z+1;` |
|    ! 0 |  577 | `		tok_scan_curly(ts,0);` |
|    ! 0 |  578 | `		*pLitStart = ts->z;` |
|    ! 0 |  579 | `		*pLitLine  = ts->iLine;` |
|    ! 0 |  580 | `		return 1;` |
|      - |  581 | `	}` |
|    ! 0 |  582 | `	if( *z=='$' && z+1 < ts->zEnd && z[1]=='{' ){` |
|    ! 0 |  583 | `		tok_encaps(ts,*pLitStart,z,*pLitLine);` |
|    ! 0 |  584 | `		tok_tok(ts,T_DOLLAR_OPEN_CURLY_BRACES,"${",2,ts->iLine);` |
|    ! 0 |  585 | `		ts->z = z+2;` |
|    ! 0 |  586 | `		tok_scan_curly(ts,1);` |
|    ! 0 |  587 | `		*pLitStart = ts->z;` |
|    ! 0 |  588 | `		*pLitLine  = ts->iLine;` |
|    ! 0 |  589 | `		return 1;` |
|      - |  590 | `	}` |
|    ! 0 |  591 | `	return 0;` |
|    ! 0 |  592 | `}` |
|      - |  593 |  |
|      - |  594 | `/*` |
|      - |  595 | ` * Scan a brace-delimited expression inside a string ("{$...}" or "${...}").` |
|      - |  596 | ` * The opening brace token was already emitted and ts->z points just past it.` |
|      - |  597 | ` * Emits PHP tokens until the matching close brace, which it emits as a plain` |
|      - |  598 | ` * "}" string token. bVarname: the first label (if immediately followed by '}'` |
|      - |  599 | ` * or '[') is a T_STRING_VARNAME (the "${name}" form).` |
|      - |  600 | ` */` |
|    ! 0 |  601 | `static void tok_scan_curly(tok_state *ts,int bVarname){` |
|    ! 0 |  602 | `	int depth = 1;` |
|    ! 0 |  603 | `	if( bVarname && ts->z < ts->zEnd && tok_is_label_start(*ts->z) ){` |
|    ! 0 |  604 | `		const unsigned char *l = ts->z;` |
|    ! 0 |  605 | `		while( l < ts->zEnd && tok_is_label(*l) ){ l++; }` |
|    ! 0 |  606 | `		if( l < ts->zEnd && (*l=='}' \|\| *l=='[') ){` |
|    ! 0 |  607 | `			tok_tok(ts,T_STRING_VARNAME,(const char *)ts->z,(int)(l-ts->z),ts->iLine);` |
|    ! 0 |  608 | `			ts->z = l;` |
|    ! 0 |  609 | `		}` |
|    ! 0 |  610 | `	}` |
|    ! 0 |  611 | `	while( ts->z < ts->zEnd && depth > 0 && !ts->bOOM ){` |
|      - |  612 | `		int eff;` |
|    ! 0 |  613 | `		if( *ts->z=='}' ){` |
|    ! 0 |  614 | `			depth--;` |
|    ! 0 |  615 | `			if( depth == 0 ){` |
|    ! 0 |  616 | `				tok_plain(ts,"}",1);` |
|    ! 0 |  617 | `				ts->z++;` |
|    ! 0 |  618 | `				return;` |
|      - |  619 | `			}` |
|    ! 0 |  620 | `		}` |
|    ! 0 |  621 | `		eff = tok_lex_one(ts);` |
|    ! 0 |  622 | `		if( eff == '{' ){ depth++; }` |
|    ! 0 |  623 | `		else if( eff == '}' ){ depth--; if( depth==0 ){ return; } }` |
|    ! 0 |  624 | `		else if( eff < 0 ){ return; } /* close tag / EOF safety */` |
|    ! 0 |  625 | `	}` |
|    ! 0 |  626 | `}` |
|      - |  627 |  |
|      - |  628 | `/*` |
|      - |  629 | ` * Scan a double-quoted string or backtick string starting at the opening` |
|      - |  630 | ` * delimiter (ts->z points at it). If double-quoted with no interpolation the` |
|      - |  631 | ` * whole literal is one T_CONSTANT_ENCAPSED_STRING; otherwise it splits into the` |
|      - |  632 | ` * delimiter, encapsed runs and interpolation tokens.` |
|      - |  633 | ` */` |
|    ! 0 |  634 | `static void tok_scan_dquote(tok_state *ts,int chDelim){` |
|    ! 0 |  635 | `	const unsigned char *zOpen = ts->z;` |
|    ! 0 |  636 | `	int iOpenLine = ts->iLine;` |
|    ! 0 |  637 | `	const unsigned char *zScan = ts->z+1;` |
|    ! 0 |  638 | `	int bInterp = 0;` |
|    ! 0 |  639 | `	int bClosed = 0;` |
|      - |  640 | `	/* Peek for interpolation to decide constant-vs-split (only for '"'). */` |
|    ! 0 |  641 | `	while( zScan < ts->zEnd ){` |
|    ! 0 |  642 | `		int c = *zScan;` |
|    ! 0 |  643 | `		if( c=='\\' ){ zScan += 2; continue; }` |
|    ! 0 |  644 | `		if( c==chDelim ){ bClosed = 1; break; }` |
|    ! 0 |  645 | `		if( c=='$' && zScan+1 < ts->zEnd && (tok_is_label_start(zScan[1])\|\|zScan[1]=='{') ){ bInterp = 1; break; }` |
|    ! 0 |  646 | `		if( c=='{' && zScan+1 < ts->zEnd && zScan[1]=='$' ){ bInterp = 1; break; }` |
|    ! 0 |  647 | `		zScan++;` |
|    ! 0 |  648 | `	}` |
|    ! 0 |  649 | `	if( chDelim=='"' && !bInterp && bClosed ){` |
|      - |  650 | `		/* Whole constant string. Find the real closing quote. */` |
|    ! 0 |  651 | `		const unsigned char *z = ts->z+1;` |
|    ! 0 |  652 | `		while( z < ts->zEnd ){` |
|    ! 0 |  653 | `			if( *z=='\\' && z+1 < ts->zEnd ){ z += 2; continue; }` |
|    ! 0 |  654 | `			if( *z=='"' ){ break; }` |
|    ! 0 |  655 | `			z++;` |
|    ! 0 |  656 | `		}` |
|    ! 0 |  657 | `		if( z < ts->zEnd ){ z++; } /* include closing quote */` |
|    ! 0 |  658 | `		tok_tok(ts,T_CONSTANT_ENCAPSED_STRING,(const char *)zOpen,(int)(z-zOpen),iOpenLine);` |
|    ! 0 |  659 | `		tok_bump_lines(ts,ts->z,z);` |
|    ! 0 |  660 | `		ts->z = z;` |
|    ! 0 |  661 | `		return;` |
|      - |  662 | `	}` |
|      - |  663 | `	/* Split form: opening delimiter, then body, then closing delimiter. */` |
|      - |  664 | `	{` |
|    ! 0 |  665 | `		char d = (char)chDelim;` |
|      - |  666 | `		const unsigned char *litStart;` |
|      - |  667 | `		int litLine;` |
|    ! 0 |  668 | `		tok_plain(ts,&d,1);` |
|    ! 0 |  669 | `		ts->z++;` |
|    ! 0 |  670 | `		litStart = ts->z;` |
|    ! 0 |  671 | `		litLine  = ts->iLine;` |
|    ! 0 |  672 | `		while( ts->z < ts->zEnd && !ts->bOOM ){` |
|    ! 0 |  673 | `			int c = *ts->z;` |
|    ! 0 |  674 | `			if( c=='\\' && ts->z+1 < ts->zEnd ){` |
|    ! 0 |  675 | `				if( ts->z[1]=='\n' ){ ts->iLine++; } /* escaped newline still advances the line */` |
|    ! 0 |  676 | `				ts->z += 2;` |
|    ! 0 |  677 | `				continue;` |
|      - |  678 | `			}` |
|    ! 0 |  679 | `			if( c==chDelim ){` |
|    ! 0 |  680 | `				tok_encaps(ts,litStart,ts->z,litLine);` |
|    ! 0 |  681 | `				tok_plain(ts,&d,1);` |
|    ! 0 |  682 | `				ts->z++;` |
|    ! 0 |  683 | `				return;` |
|      - |  684 | `			}` |
|    ! 0 |  685 | `			if( c=='$' \|\| c=='{' ){` |
|    ! 0 |  686 | `				if( tok_try_interp(ts,&litStart,&litLine) ){ continue; }` |
|    ! 0 |  687 | `			}` |
|    ! 0 |  688 | `			if( tok_at_nl(ts->z,ts->zEnd) ){ ts->iLine++; }` |
|    ! 0 |  689 | `			ts->z++;` |
|    ! 0 |  690 | `		}` |
|      - |  691 | `		/* Unterminated: flush what we have. */` |
|    ! 0 |  692 | `		tok_encaps(ts,litStart,ts->z,litLine);` |
|      - |  693 | `	}` |
|    ! 0 |  694 | `}` |
|      - |  695 |  |
|      - |  696 | `/* Does the line at z (already at line start) begin the heredoc closing marker` |
|      - |  697 | ` * for zLabel? Returns the end of the label (past it) if so, else 0. */` |
|    ! 0 |  698 | `static const unsigned char * tok_heredoc_close(tok_state *ts,const unsigned char *z,` |
|    ! 0 |  699 | `		const char *zLabel,int nLabel){` |
|    ! 0 |  700 | `	const unsigned char *p = z;` |
|      - |  701 | `	int i;` |
|    ! 0 |  702 | `	while( p < ts->zEnd && (*p==' '\|\|*p=='\t') ){ p++; }` |
|    ! 0 |  703 | `	if( p + nLabel > ts->zEnd ){ return 0; } /* not enough bytes left for the label */` |
|    ! 0 |  704 | `	for( i = 0 ; i < nLabel ; ++i ){` |
|    ! 0 |  705 | `		if( p[i] != (unsigned char)zLabel[i] ){ return 0; }` |
|    ! 0 |  706 | `	}` |
|    ! 0 |  707 | `	p += nLabel;` |
|    ! 0 |  708 | `	if( p < ts->zEnd && tok_is_label(*p) ){ return 0; } /* label is a prefix of a longer word */` |
|    ! 0 |  709 | `	return p;` |
|    ! 0 |  710 | `}` |
|      - |  711 |  |
|      - |  712 | `/*` |
|      - |  713 | ` * Scan a heredoc/nowdoc starting at "<<<". ts->z points at the first '<'.` |
|      - |  714 | ` * Returns 1 if a valid heredoc was consumed, 0 if this "<<<" is not a valid` |
|      - |  715 | ` * heredoc start (the caller then falls through to operator lexing: T_SL + '<').` |
|      - |  716 | ` * A valid start is: "<<<" [ws]* ["\|']? LABEL ["\|']? immediately followed by` |
|      - |  717 | ` * an optional '\r' and a '\n'.` |
|      - |  718 | ` */` |
|    ! 0 |  719 | `static int tok_scan_heredoc(tok_state *ts){` |
|    ! 0 |  720 | `	const unsigned char *zStart = ts->z;` |
|    ! 0 |  721 | `	int iStartLine = ts->iLine;` |
|    ! 0 |  722 | `	const unsigned char *z = ts->z+3;` |
|    ! 0 |  723 | `	int bNowdoc = 0;` |
|    ! 0 |  724 | `	int chQuote = 0;` |
|      - |  725 | `	const unsigned char *zLabel;` |
|      - |  726 | `	int nLabel;` |
|      - |  727 | `	const unsigned char *litStart;` |
|      - |  728 | `	int litLine;` |
|      - |  729 | `	/* optional spaces/tabs after <<< */` |
|    ! 0 |  730 | `	while( z < ts->zEnd && (*z==' '\|\|*z=='\t') ){ z++; }` |
|    ! 0 |  731 | `	if( z < ts->zEnd && (*z=='"' \|\| *z=='\'') ){` |
|    ! 0 |  732 | `		chQuote = *z;` |
|    ! 0 |  733 | `		bNowdoc = (*z=='\'');` |
|    ! 0 |  734 | `		z++;` |
|    ! 0 |  735 | `	}` |
|    ! 0 |  736 | `	zLabel = z;` |
|    ! 0 |  737 | `	while( z < ts->zEnd && tok_is_label(*z) ){ z++; }` |
|    ! 0 |  738 | `	nLabel = (int)(z - zLabel);` |
|    ! 0 |  739 | `	if( nLabel < 1 ){ return 0; } /* no label -> not a heredoc */` |
|    ! 0 |  740 | `	if( chQuote ){` |
|    ! 0 |  741 | `		if( z < ts->zEnd && *z==chQuote ){ z++; }` |
|    ! 0 |  742 | `		else { return 0; } /* unbalanced quote -> not a heredoc */` |
|    ! 0 |  743 | `	}` |
|      - |  744 | `	/* The label must be immediately followed by an optional '\r' then '\n'. */` |
|    ! 0 |  745 | `	if( z < ts->zEnd && *z=='\r' && z+1 < ts->zEnd && z[1]=='\n' ){ z += 2; }` |
|    ! 0 |  746 | `	else if( z < ts->zEnd && *z=='\n' ){ z += 1; }` |
|    ! 0 |  747 | `	else { return 0; } /* junk or EOF after the marker -> not a heredoc */` |
|      - |  748 | `	/* Emit T_START_HEREDOC (delimiters + trailing newline). */` |
|    ! 0 |  749 | `	tok_tok(ts,T_START_HEREDOC,(const char *)zStart,(int)(z-zStart),iStartLine);` |
|    ! 0 |  750 | `	tok_bump_lines(ts,zStart,z);` |
|    ! 0 |  751 | `	ts->z = z;` |
|      - |  752 | `	/* Scan body line by line until the closing marker. */` |
|    ! 0 |  753 | `	litStart = ts->z;` |
|    ! 0 |  754 | `	litLine  = ts->iLine;` |
|    ! 0 |  755 | `	while( ts->z < ts->zEnd && !ts->bOOM ){` |
|      - |  756 | `		/* At a line start, test for the closing marker. */` |
|    ! 0 |  757 | `		const unsigned char *pClose = tok_heredoc_close(ts,ts->z,(const char *)zLabel,nLabel);` |
|    ! 0 |  758 | `		if( pClose ){` |
|      - |  759 | `			int iCloseLine;` |
|    ! 0 |  760 | `			tok_encaps(ts,litStart,ts->z,litLine);` |
|    ! 0 |  761 | `			iCloseLine = ts->iLine;` |
|    ! 0 |  762 | `			tok_tok(ts,T_END_HEREDOC,(const char *)ts->z,(int)(pClose-ts->z),iCloseLine);` |
|    ! 0 |  763 | `			ts->z = pClose;` |
|    ! 0 |  764 | `			return 1;` |
|      - |  765 | `		}` |
|      - |  766 | `		/* Process one line's content. */` |
|    ! 0 |  767 | `		while( ts->z < ts->zEnd ){` |
|    ! 0 |  768 | `			int c = *ts->z;` |
|    ! 0 |  769 | `			if( c=='\n' ){ ts->iLine++; ts->z++; break; }` |
|    ! 0 |  770 | `			if( c=='\r' && (ts->z+1>=ts->zEnd \|\| ts->z[1]!='\n') ){ ts->iLine++; ts->z++; continue; }` |
|    ! 0 |  771 | `			if( !bNowdoc && (c=='\\') && ts->z+1 < ts->zEnd ){` |
|    ! 0 |  772 | `				if( ts->z[1]=='\n' ){ ts->iLine++; } /* escaped newline still advances the line */` |
|    ! 0 |  773 | `				ts->z += 2;` |
|    ! 0 |  774 | `				continue;` |
|      - |  775 | `			}` |
|    ! 0 |  776 | `			if( !bNowdoc && (c=='$' \|\| c=='{') ){` |
|    ! 0 |  777 | `				if( tok_try_interp(ts,&litStart,&litLine) ){ continue; }` |
|    ! 0 |  778 | `			}` |
|    ! 0 |  779 | `			ts->z++;` |
|    ! 0 |  780 | `		}` |
|    ! 0 |  781 | `	}` |
|    ! 0 |  782 | `	tok_encaps(ts,litStart,ts->z,litLine);` |
|    ! 0 |  783 | `	return 1;` |
|    ! 0 |  784 | `}` |
|      - |  785 |  |
|      - |  786 | `/* One cast keyword between the parens, e.g. "int". Returns the cast T_* id or 0. */` |
|    ! 0 |  787 | `static int tok_cast_id(const char *z,int n){` |
|    ! 0 |  788 | `	if( tok_ci_eq(z,n,"int") \|\| tok_ci_eq(z,n,"integer") ) return T_INT_CAST;` |
|    ! 0 |  789 | `	if( tok_ci_eq(z,n,"float") \|\| tok_ci_eq(z,n,"double") \|\| tok_ci_eq(z,n,"real") ) return T_DOUBLE_CAST;` |
|    ! 0 |  790 | `	if( tok_ci_eq(z,n,"string") \|\| tok_ci_eq(z,n,"binary") ) return T_STRING_CAST;` |
|    ! 0 |  791 | `	if( tok_ci_eq(z,n,"array") ) return T_ARRAY_CAST;` |
|    ! 0 |  792 | `	if( tok_ci_eq(z,n,"object") ) return T_OBJECT_CAST;` |
|    ! 0 |  793 | `	if( tok_ci_eq(z,n,"bool") \|\| tok_ci_eq(z,n,"boolean") ) return T_BOOL_CAST;` |
|    ! 0 |  794 | `	if( tok_ci_eq(z,n,"unset") ) return T_UNSET_CAST;` |
|    ! 0 |  795 | `	return 0;` |
|    ! 0 |  796 | `}` |
|      - |  797 |  |
|      - |  798 | `/*` |
|      - |  799 | ` * Try to lex "( <ws>? castword <ws>? )" at ts->z (pointing at '('). On success` |
|      - |  800 | ` * emits the cast token and returns 1; otherwise returns 0 and consumes nothing.` |
|      - |  801 | ` */` |
|    ! 0 |  802 | `static int tok_try_cast(tok_state *ts){` |
|    ! 0 |  803 | `	const unsigned char *p = ts->z+1;` |
|      - |  804 | `	const unsigned char *w0,*w1;` |
|      - |  805 | `	int id;` |
|    ! 0 |  806 | `	while( p < ts->zEnd && (*p==' '\|\|*p=='\t') ){ p++; }` |
|    ! 0 |  807 | `	w0 = p;` |
|    ! 0 |  808 | `	while( p < ts->zEnd && tok_is_label(*p) ){ p++; }` |
|    ! 0 |  809 | `	w1 = p;` |
|    ! 0 |  810 | `	if( w1 == w0 ){ return 0; }` |
|    ! 0 |  811 | `	id = tok_cast_id((const char *)w0,(int)(w1-w0));` |
|    ! 0 |  812 | `	if( id == 0 ){ return 0; }` |
|    ! 0 |  813 | `	while( p < ts->zEnd && (*p==' '\|\|*p=='\t') ){ p++; }` |
|    ! 0 |  814 | `	if( p >= ts->zEnd \|\| *p != ')' ){ return 0; }` |
|    ! 0 |  815 | `	p++;` |
|    ! 0 |  816 | `	tok_tok(ts,id,(const char *)ts->z,(int)(p-ts->z),ts->iLine);` |
|    ! 0 |  817 | `	ts->z = p;` |
|    ! 0 |  818 | `	return 1;` |
|    ! 0 |  819 | `}` |
|      - |  820 |  |
|      - |  821 | `/*` |
|      - |  822 | ` * Lex exactly one PHP-mode token and emit it. Returns:` |
|      - |  823 | ` *   0   normal token` |
|      - |  824 | ` *  '{' / '}'  when it emitted a bare '{' / '}' (for curly-expr brace tracking)` |
|      - |  825 | ` *  -1  when it emitted a close tag or hit EOF (leave PHP mode)` |
|      - |  826 | ` * Whitespace and comments preserve ts->bProp; every other token consumes it.` |
|      - |  827 | ` */` |
|    113 |  828 | `static int tok_lex_one(tok_state *ts){` |
|    113 |  829 | `	const unsigned char *z = ts->z;` |
|      - |  830 | `	int c;` |
|      - |  831 | `	int bWasProp;` |
|      - |  832 | `	int bWasCase;` |
|    113 |  833 | `	if( z >= ts->zEnd ){ return -1; }` |
|    113 |  834 | `	c = *z;` |
|      - |  835 | `	/* Whitespace run (preserves property state). */` |
|    113 |  836 | `	if( tok_is_ws(c) ){` |
|     49 |  837 | `		const unsigned char *z0 = z;` |
|     49 |  838 | `		int iLine = ts->iLine;` |
|     97 |  839 | `		while( ts->z < ts->zEnd && tok_is_ws(*ts->z) ){` |
|     49 |  840 | `			if( tok_at_nl(ts->z,ts->zEnd) ){ ts->iLine++; }` |
|     49 |  841 | `			ts->z++;` |
|      1 |  842 | `		}` |
|     49 |  843 | `		tok_tok(ts,T_WHITESPACE,(const char *)z0,(int)(ts->z-z0),iLine);` |
|     49 |  844 | `		return 0;` |
|      - |  845 | `	}` |
|      - |  846 | `	/* Comments: hash/slash-slash to EOL-or-close-tag, and block comments. Preserve property state. */` |
|     65 |  847 | `	if( c=='#' && !(z+1 < ts->zEnd && z[1]=='[') ){` |
|    ! 0 |  848 | `		const unsigned char *z0 = z;` |
|    ! 0 |  849 | `		int iLine = ts->iLine;` |
|    ! 0 |  850 | `		ts->z++;` |
|    ! 0 |  851 | `		while( ts->z < ts->zEnd && *ts->z!='\n' ){` |
|    ! 0 |  852 | `			if( *ts->z=='?' && ts->z+1 < ts->zEnd && ts->z[1]=='>' ){ break; }` |
|    ! 0 |  853 | `			ts->z++;` |
|    ! 0 |  854 | `		}` |
|    ! 0 |  855 | `		tok_tok(ts,T_COMMENT,(const char *)z0,(int)(ts->z-z0),iLine);` |
|    ! 0 |  856 | `		return 0;` |
|      - |  857 | `	}` |
|     65 |  858 | `	if( c=='/' && z+1 < ts->zEnd && z[1]=='/' ){` |
|    ! 0 |  859 | `		const unsigned char *z0 = z;` |
|    ! 0 |  860 | `		int iLine = ts->iLine;` |
|    ! 0 |  861 | `		ts->z += 2;` |
|    ! 0 |  862 | `		while( ts->z < ts->zEnd && *ts->z!='\n' ){` |
|    ! 0 |  863 | `			if( *ts->z=='?' && ts->z+1 < ts->zEnd && ts->z[1]=='>' ){ break; }` |
|    ! 0 |  864 | `			ts->z++;` |
|    ! 0 |  865 | `		}` |
|    ! 0 |  866 | `		tok_tok(ts,T_COMMENT,(const char *)z0,(int)(ts->z-z0),iLine);` |
|    ! 0 |  867 | `		return 0;` |
|      - |  868 | `	}` |
|     65 |  869 | `	if( c=='/' && z+1 < ts->zEnd && z[1]=='*' ){` |
|    ! 0 |  870 | `		const unsigned char *z0 = z;` |
|    ! 0 |  871 | `		int iLine = ts->iLine;` |
|      - |  872 | `		/* A doc comment is slash-star-star followed by a whitespace char (php's` |
|      - |  873 | `		 * rule): the third char must be '*' and the fourth must be whitespace. */` |
|    ! 0 |  874 | `		int bDoc = ( z+2 < ts->zEnd && z[2]=='*' && z+3 < ts->zEnd && tok_is_ws(z[3]) );` |
|    ! 0 |  875 | `		ts->z += 2;` |
|    ! 0 |  876 | `		while( ts->z < ts->zEnd ){` |
|    ! 0 |  877 | `			if( *ts->z=='*' && ts->z+1 < ts->zEnd && ts->z[1]=='/' ){ ts->z += 2; break; }` |
|    ! 0 |  878 | `			if( tok_at_nl(ts->z,ts->zEnd) ){ ts->iLine++; }` |
|    ! 0 |  879 | `			ts->z++;` |
|    ! 0 |  880 | `		}` |
|    ! 0 |  881 | `		tok_tok(ts,bDoc ? T_DOC_COMMENT : T_COMMENT,(const char *)z0,(int)(ts->z-z0),iLine);` |
|    ! 0 |  882 | `		return 0;` |
|      - |  883 | `	}` |
|      - |  884 | `	/* From here the token consumes property/case state. */` |
|     65 |  885 | `	bWasProp = ts->bProp;` |
|     65 |  886 | `	bWasCase = ts->bCaseName;` |
|     65 |  887 | `	ts->bProp = 0;` |
|     65 |  888 | `	ts->bCaseName = 0;` |
|      - |  889 | `	/* Close tag. */` |
|     65 |  890 | `	if( c=='?' && z+1 < ts->zEnd && z[1]=='>' ){` |
|    ! 0 |  891 | `		const unsigned char *z0 = z;` |
|    ! 0 |  892 | `		int iLine = ts->iLine;` |
|    ! 0 |  893 | `		ts->z += 2;` |
|      - |  894 | `		/* php swallows one trailing newline (\n or \r\n) into the close tag */` |
|    ! 0 |  895 | `		if( ts->z < ts->zEnd && *ts->z=='\r' && ts->z+1 < ts->zEnd && ts->z[1]=='\n' ){ ts->z += 2; ts->iLine++; }` |
|    ! 0 |  896 | `		else if( ts->z < ts->zEnd && *ts->z=='\n' ){ ts->z++; ts->iLine++; }` |
|    ! 0 |  897 | `		tok_tok(ts,T_CLOSE_TAG,(const char *)z0,(int)(ts->z-z0),iLine);` |
|    ! 0 |  898 | `		return -1;` |
|      - |  899 | `	}` |
|      - |  900 | `	/* #[ attribute open. */` |
|     65 |  901 | `	if( c=='#' && z+1 < ts->zEnd && z[1]=='[' ){` |
|    ! 0 |  902 | `		tok_tok(ts,T_ATTRIBUTE,"#[",2,ts->iLine);` |
|    ! 0 |  903 | `		ts->z += 2;` |
|    ! 0 |  904 | `		return 0;` |
|      - |  905 | `	}` |
|      - |  906 | `	/* Variable. */` |
|     65 |  907 | `	if( c=='$' && z+1 < ts->zEnd && tok_is_label_start(z[1]) ){` |
|     17 |  908 | `		const unsigned char *v = z+1;` |
|     33 |  909 | `		while( v < ts->zEnd && tok_is_label(*v) ){ v++; }` |
|     17 |  910 | `		tok_tok(ts,T_VARIABLE,(const char *)z,(int)(v-z),ts->iLine);` |
|     17 |  911 | `		ts->z = v;` |
|     17 |  912 | `		return 0;` |
|      - |  913 | `	}` |
|      - |  914 | `	/* Namespaced name / identifier / keyword. */` |
|     49 |  915 | `	if( tok_is_label_start(c) \|\| (c=='\\' && z+1 < ts->zEnd && tok_is_label_start(z[1])) ){` |
|    ! 0 |  916 | `		const unsigned char *p = z;` |
|    ! 0 |  917 | `		int bLeadBackslash = (c=='\\');` |
|    ! 0 |  918 | `		int bInner = 0;                 /* saw an internal backslash */` |
|      - |  919 | `		const unsigned char *firstLabelStart, *firstLabelEnd;` |
|    ! 0 |  920 | `		if( bLeadBackslash ){ p++; }` |
|    ! 0 |  921 | `		firstLabelStart = p;` |
|    ! 0 |  922 | `		while( p < ts->zEnd && tok_is_label(*p) ){ p++; }` |
|    ! 0 |  923 | `		firstLabelEnd = p;` |
|      - |  924 | `		/* Consume further \label segments. */` |
|    ! 0 |  925 | `		while( p+1 < ts->zEnd && *p=='\\' && tok_is_label_start(p[1]) ){` |
|    ! 0 |  926 | `			bInner = 1;` |
|    ! 0 |  927 | `			p++;` |
|    ! 0 |  928 | `			while( p < ts->zEnd && tok_is_label(*p) ){ p++; }` |
|    ! 0 |  929 | `		}` |
|    ! 0 |  930 | `		if( bLeadBackslash ){` |
|    ! 0 |  931 | `			tok_tok(ts,T_NAME_FULLY_QUALIFIED,(const char *)z,(int)(p-z),ts->iLine);` |
|    ! 0 |  932 | `			ts->z = p;` |
|    ! 0 |  933 | `			return 0;` |
|      - |  934 | `		}` |
|    ! 0 |  935 | `		if( bInner ){` |
|    ! 0 |  936 | `			int nFirst = (int)(firstLabelEnd - firstLabelStart);` |
|    ! 0 |  937 | `			int id = T_NAME_QUALIFIED;` |
|    ! 0 |  938 | `			if( tok_ci_eq((const char *)firstLabelStart,nFirst,"namespace") ){ id = T_NAME_RELATIVE; }` |
|    ! 0 |  939 | `			tok_tok(ts,id,(const char *)z,(int)(p-z),ts->iLine);` |
|    ! 0 |  940 | `			ts->z = p;` |
|    ! 0 |  941 | `			return 0;` |
|      - |  942 | `		}` |
|      - |  943 | `		/* Lone identifier: keyword, contextual, or T_STRING. */` |
|      - |  944 | `		{` |
|    ! 0 |  945 | `			int n = (int)(firstLabelEnd - z);` |
|    ! 0 |  946 | `			int id = T_STRING;` |
|      - |  947 | `			sxu32 k;` |
|    ! 0 |  948 | `			if( bWasProp ){` |
|    ! 0 |  949 | `				id = T_STRING;              /* property access after -> / ?-> */` |
|    ! 0 |  950 | `			}else if( tok_ci_eq((const char *)z,n,"enum") ){` |
|      - |  951 | `				/* Contextual: T_ENUM only when followed by <ws>+ then a label-start. */` |
|    ! 0 |  952 | `				const unsigned char *q = firstLabelEnd;` |
|    ! 0 |  953 | `				int sawWs = 0;` |
|    ! 0 |  954 | `				while( q < ts->zEnd && (*q==' '\|\|*q=='\t'\|\|*q=='\n'\|\|*q=='\r') ){ q++; sawWs = 1; }` |
|    ! 0 |  955 | `				if( sawWs && q < ts->zEnd && tok_is_label_start(*q) ){ id = T_ENUM; }` |
|    ! 0 |  956 | `			}else if( tok_ci_eq((const char *)z,n,"yield") ){` |
|      - |  957 | `				/* Contextual: "yield from" collapses to one T_YIELD_FROM. */` |
|    ! 0 |  958 | `				const unsigned char *q = firstLabelEnd;` |
|    ! 0 |  959 | `				const unsigned char *ws = q;` |
|    ! 0 |  960 | `				while( q < ts->zEnd && (*q==' '\|\|*q=='\t'\|\|*q=='\n'\|\|*q=='\r'\|\|*q=='\v'\|\|*q=='\f') ){ q++; }` |
|    ! 0 |  961 | `				if( q > ws && q+4 <= ts->zEnd && tok_ci_eq((const char *)q,4,"from")` |
|    ! 0 |  962 | `					&& (q+4 >= ts->zEnd \|\| !tok_is_label(q[4])) ){` |
|    ! 0 |  963 | `					const unsigned char *e = q+4;` |
|    ! 0 |  964 | `					tok_tok(ts,T_YIELD_FROM,(const char *)z,(int)(e-z),ts->iLine);` |
|    ! 0 |  965 | `					tok_bump_lines(ts,firstLabelEnd,e);` |
|    ! 0 |  966 | `					ts->z = e;` |
|    ! 0 |  967 | `					return 0;` |
|      - |  968 | `				}` |
|    ! 0 |  969 | `				id = T_YIELD;` |
|    ! 0 |  970 | `			}else{` |
|    ! 0 |  971 | `				for( k = 0 ; k < SX_ARRAYSIZE(aKeyword) ; ++k ){` |
|    ! 0 |  972 | `					if( tok_ci_eq((const char *)z,n,aKeyword[k].zName) ){` |
|    ! 0 |  973 | `						id = aKeyword[k].iId;` |
|    ! 0 |  974 | `						break;` |
|      - |  975 | `					}` |
|    ! 0 |  976 | `				}` |
|      - |  977 | `			}` |
|      - |  978 | `			/* Under TOKEN_PARSE, a reserved word right after 'case' that names an` |
|      - |  979 | `			 * enum case (followed by ';' or '=') is T_STRING; a switch 'case` |
|      - |  980 | `			 * array(...)'/'case expr:' keeps the keyword. */` |
|    ! 0 |  981 | `			if( bWasCase && id != T_STRING ){` |
|    ! 0 |  982 | `				const unsigned char *q = firstLabelEnd;` |
|    ! 0 |  983 | `				while( q < ts->zEnd && tok_is_ws(*q) ){ q++; }` |
|      - |  984 | `				/* ';' ends a pure case, a lone '=' (not '=='/'=>') starts a backed` |
|      - |  985 | `				 * case value; both mark an enum case name. */` |
|    ! 0 |  986 | `				if( q < ts->zEnd && (*q==';' \|\|` |
|    ! 0 |  987 | `					(*q=='=' && (q+1>=ts->zEnd \|\| (q[1]!='=' && q[1]!='>')))) ){` |
|    ! 0 |  988 | `					id = T_STRING;` |
|    ! 0 |  989 | `				}` |
|    ! 0 |  990 | `			}` |
|    ! 0 |  991 | `			if( id == T_STRING ){` |
|    ! 0 |  992 | `				tok_tok(ts,T_STRING,(const char *)z,n,ts->iLine);` |
|    ! 0 |  993 | `			}else{` |
|    ! 0 |  994 | `				tok_tok(ts,id,(const char *)z,n,ts->iLine);` |
|      - |  995 | `			}` |
|    ! 0 |  996 | `			ts->z = firstLabelEnd;` |
|    ! 0 |  997 | `			if( id == T_HALT_COMPILER ){` |
|    ! 0 |  998 | `				tok_halt_tail(ts);` |
|    ! 0 |  999 | `				return -1;` |
|      - | 1000 | `			}` |
|      - | 1001 | `			/* Under TOKEN_PARSE, a reserved word naming a function/method or a` |
|      - | 1002 | `			 * class constant becomes T_STRING (semi-reserved words); force the` |
|      - | 1003 | `			 * next identifier to T_STRING like a member name. 'case' arms a` |
|      - | 1004 | `			 * conditional retag (enum case names only — see the identifier path). */` |
|    ! 0 | 1005 | `			if( ts->bParse && (id == T_FUNCTION \|\| id == T_CONST) ){` |
|    ! 0 | 1006 | `				ts->bProp = 1;` |
|    ! 0 | 1007 | `			}` |
|    ! 0 | 1008 | `			if( ts->bParse && id == T_CASE ){` |
|    ! 0 | 1009 | `				ts->bCaseName = 1;` |
|    ! 0 | 1010 | `			}` |
|    ! 0 | 1011 | `			return 0;` |
|      - | 1012 | `		}` |
|      - | 1013 | `	}` |
|      - | 1014 | `	/* Lone backslash -> namespace separator. */` |
|     49 | 1015 | `	if( c=='\\' ){` |
|    ! 0 | 1016 | `		tok_tok(ts,T_NS_SEPARATOR,"\\",1,ts->iLine);` |
|    ! 0 | 1017 | `		ts->z++;` |
|    ! 0 | 1018 | `		return 0;` |
|      - | 1019 | `	}` |
|      - | 1020 | `	/* Number. */` |
|     49 | 1021 | `	if( (c>='0'&&c<='9') \|\| (c=='.' && z+1 < ts->zEnd && z[1]>='0' && z[1]<='9') ){` |
|     17 | 1022 | `		const unsigned char *p = z;` |
|     17 | 1023 | `		int isFloat = 0;` |
|      - | 1024 | `		int id;` |
|     16 | 1025 | `		if( c=='0' && p+2 < ts->zEnd && (p[1]=='x'\|\|p[1]=='X')` |
|      1 | 1026 | `				&& ((p[2]>='0'&&p[2]<='9')\|\|(p[2]>='a'&&p[2]<='f')\|\|(p[2]>='A'&&p[2]<='F')) ){` |
|      - | 1027 | `			const unsigned char *d0;` |
|    ! 0 | 1028 | `			p += 2; d0 = p;` |
|    ! 0 | 1029 | `			while( p < ts->zEnd && ((*p>='0'&&*p<='9')\|\|(*p>='a'&&*p<='f')\|\|(*p>='A'&&*p<='F')\|\|*p=='_') ){ p++; }` |
|    ! 0 | 1030 | `			id = tok_int_overflows(d0,p,16) ? T_DNUMBER : T_LNUMBER;` |
|     17 | 1031 | `		}else if( c=='0' && p+2 < ts->zEnd && (p[1]=='b'\|\|p[1]=='B') && (p[2]=='0'\|\|p[2]=='1') ){` |
|      - | 1032 | `			const unsigned char *d0;` |
|    ! 0 | 1033 | `			p += 2; d0 = p;` |
|    ! 0 | 1034 | `			while( p < ts->zEnd && (*p=='0'\|\|*p=='1'\|\|*p=='_') ){ p++; }` |
|    ! 0 | 1035 | `			id = tok_int_overflows(d0,p,2) ? T_DNUMBER : T_LNUMBER;` |
|     17 | 1036 | `		}else if( c=='0' && p+2 < ts->zEnd && (p[1]=='o'\|\|p[1]=='O') && (p[2]>='0'&&p[2]<='7') ){` |
|      - | 1037 | `			const unsigned char *d0;` |
|    ! 0 | 1038 | `			p += 2; d0 = p;` |
|    ! 0 | 1039 | `			while( p < ts->zEnd && ((*p>='0'&&*p<='7')\|\|*p=='_') ){ p++; }` |
|    ! 0 | 1040 | `			id = tok_int_overflows(d0,p,8) ? T_DNUMBER : T_LNUMBER;` |
|    ! 0 | 1041 | `		}else{` |
|     17 | 1042 | `			const unsigned char *intStart = p;` |
|     17 | 1043 | `			int allOctalDigits = 1;` |
|     49 | 1044 | `			while( p < ts->zEnd && ((*p>='0'&&*p<='9')\|\|*p=='_') ){` |
|     17 | 1045 | `				if( *p>'7' ){ allOctalDigits = 0; }` |
|     17 | 1046 | `				p++;` |
|      1 | 1047 | `			}` |
|     17 | 1048 | `			if( p < ts->zEnd && *p=='.' && !(c=='.') ){` |
|      - | 1049 | `				/* fractional part (unless the token itself started with '.') */` |
|    ! 0 | 1050 | `				isFloat = 1;` |
|    ! 0 | 1051 | `				p++;` |
|    ! 0 | 1052 | `				while( p < ts->zEnd && ((*p>='0'&&*p<='9')\|\|*p=='_') ){ p++; }` |
|     16 | 1053 | `			}else if( c=='.' ){` |
|    ! 0 | 1054 | `				isFloat = 1; /* .5 style: leading dot already consumed below */` |
|    ! 0 | 1055 | `			}` |
|     17 | 1056 | `			if( c=='.' ){` |
|      - | 1057 | `				/* token began with '.': consume the dot + digits here */` |
|    ! 0 | 1058 | `				p = z+1;` |
|    ! 0 | 1059 | `				while( p < ts->zEnd && ((*p>='0'&&*p<='9')\|\|*p=='_') ){ p++; }` |
|    ! 0 | 1060 | `				isFloat = 1;` |
|    ! 0 | 1061 | `			}` |
|     17 | 1062 | `			if( p < ts->zEnd && (*p=='e'\|\|*p=='E') ){` |
|    ! 0 | 1063 | `				const unsigned char *e = p+1;` |
|    ! 0 | 1064 | `				if( e < ts->zEnd && (*e=='+'\|\|*e=='-') ){ e++; }` |
|    ! 0 | 1065 | `				if( e < ts->zEnd && *e>='0' && *e<='9' ){` |
|    ! 0 | 1066 | `					isFloat = 1;` |
|    ! 0 | 1067 | `					p = e;` |
|    ! 0 | 1068 | `					while( p < ts->zEnd && ((*p>='0'&&*p<='9')\|\|*p=='_') ){ p++; }` |
|    ! 0 | 1069 | `				}` |
|    ! 0 | 1070 | `			}` |
|     17 | 1071 | `			if( isFloat ){` |
|    ! 0 | 1072 | `				id = T_DNUMBER;` |
|     17 | 1073 | `			}else if( intStart < ts->zEnd && *intStart=='0' && (p-intStart) > 1 && allOctalDigits ){` |
|      - | 1074 | `				/* legacy octal 0NNN */` |
|    ! 0 | 1075 | `				id = tok_int_overflows(intStart+1,p,8) ? T_DNUMBER : T_LNUMBER;` |
|    ! 0 | 1076 | `			}else{` |
|     17 | 1077 | `				id = tok_int_overflows(intStart,p,10) ? T_DNUMBER : T_LNUMBER;` |
|      - | 1078 | `			}` |
|      - | 1079 | `		}` |
|     17 | 1080 | `		tok_tok(ts,id,(const char *)z,(int)(p-z),ts->iLine);` |
|     17 | 1081 | `		ts->z = p;` |
|     17 | 1082 | `		return 0;` |
|      - | 1083 | `	}` |
|      - | 1084 | `	/* Strings. */` |
|     33 | 1085 | `	if( c=='\'' ){` |
|    ! 0 | 1086 | `		const unsigned char *p = z+1;` |
|    ! 0 | 1087 | `		int bClosed = 0;` |
|    ! 0 | 1088 | `		while( p < ts->zEnd ){` |
|    ! 0 | 1089 | `			if( *p=='\\' && p+1 < ts->zEnd ){ p += 2; continue; }` |
|    ! 0 | 1090 | `			if( *p=='\'' ){ p++; bClosed = 1; break; }` |
|    ! 0 | 1091 | `			p++;` |
|    ! 0 | 1092 | `		}` |
|      - | 1093 | `		/* An unterminated single-quoted string is one T_ENCAPSED_AND_WHITESPACE in php. */` |
|    ! 0 | 1094 | `		tok_tok(ts,bClosed ? T_CONSTANT_ENCAPSED_STRING : T_ENCAPSED_AND_WHITESPACE,` |
|    ! 0 | 1095 | `			(const char *)z,(int)(p-z),ts->iLine);` |
|    ! 0 | 1096 | `		tok_bump_lines(ts,z,p);` |
|    ! 0 | 1097 | `		ts->z = p;` |
|    ! 0 | 1098 | `		return 0;` |
|      - | 1099 | `	}` |
|     33 | 1100 | `	if( c=='"' ){ tok_scan_dquote(ts,'"'); return 0; }` |
|     33 | 1101 | ``	if( c=='`' ){ tok_scan_dquote(ts,'`'); return 0; }`` |
|      - | 1102 | `	/* Heredoc / nowdoc (falls through to operators if not a valid start). */` |
|     33 | 1103 | `	if( c=='<' && z+2 < ts->zEnd && z[1]=='<' && z[2]=='<' ){` |
|    ! 0 | 1104 | `		if( tok_scan_heredoc(ts) ){ return 0; }` |
|    ! 0 | 1105 | `	}` |
|      - | 1106 | `	/* Cast operators. */` |
|     33 | 1107 | `	if( c=='(' ){` |
|    ! 0 | 1108 | `		if( tok_try_cast(ts) ){ return 0; }` |
|    ! 0 | 1109 | `	}` |
|      - | 1110 | `	/* Object operators set the one-shot property state (next identifier -> T_STRING). */` |
|     33 | 1111 | `	if( c=='?' && z+2 < ts->zEnd && z[1]=='-' && z[2]=='>' ){` |
|    ! 0 | 1112 | `		tok_tok(ts,T_NULLSAFE_OBJECT_OPERATOR,"?->",3,ts->iLine);` |
|    ! 0 | 1113 | `		ts->z += 3;` |
|    ! 0 | 1114 | `		ts->bProp = 1;` |
|    ! 0 | 1115 | `		return 0;` |
|      - | 1116 | `	}` |
|     33 | 1117 | `	if( c=='-' && z+1 < ts->zEnd && z[1]=='>' ){` |
|    ! 0 | 1118 | `		tok_tok(ts,T_OBJECT_OPERATOR,"->",2,ts->iLine);` |
|    ! 0 | 1119 | `		ts->z += 2;` |
|    ! 0 | 1120 | `		ts->bProp = 1;` |
|    ! 0 | 1121 | `		return 0;` |
|      - | 1122 | `	}` |
|      - | 1123 | `	/* Under TOKEN_PARSE, php re-tags a reserved word used as a member name after` |
|      - | 1124 | `	 * "::" as T_STRING (e.g. Foo::class, Foo::empty). Mirror that with bProp. */` |
|     33 | 1125 | `	if( c==':' && z+1 < ts->zEnd && z[1]==':' ){` |
|    ! 0 | 1126 | `		tok_tok(ts,T_DOUBLE_COLON,"::",2,ts->iLine);` |
|    ! 0 | 1127 | `		ts->z += 2;` |
|    ! 0 | 1128 | `		if( ts->bParse ){ ts->bProp = 1; }` |
|    ! 0 | 1129 | `		return 0;` |
|      - | 1130 | `	}` |
|      - | 1131 | `	/* Multi-char and single-char operators (longest match first). */` |
|      - | 1132 | `	{` |
|     33 | 1133 | `		const unsigned char *e = ts->zEnd;` |
|     33 | 1134 | `		int r0 = c;` |
|     33 | 1135 | `		int r1 = (z+1<e)?z[1]:-1;` |
|     33 | 1136 | `		int r2 = (z+2<e)?z[2]:-1;` |
|      - | 1137 | `		#define TK3(a,b,cc,id) if(r0==(a)&&r1==(b)&&r2==(cc)){ tok_tok(ts,id,(const char*)z,3,ts->iLine); ts->z+=3; return 0; }` |
|      - | 1138 | `		#define TK2(a,b,id)    if(r0==(a)&&r1==(b)){ tok_tok(ts,id,(const char*)z,2,ts->iLine); ts->z+=2; return 0; }` |
|     33 | 1139 | `		TK3('=','=','=',T_IS_IDENTICAL)` |
|     33 | 1140 | `		TK3('!','=','=',T_IS_NOT_IDENTICAL)` |
|     33 | 1141 | `		TK3('<','=','>',T_SPACESHIP)` |
|     33 | 1142 | `		TK3('*','*','=',T_POW_EQUAL)` |
|     33 | 1143 | `		TK3('.','.','.',T_ELLIPSIS)` |
|     33 | 1144 | `		TK3('<','<','=',T_SL_EQUAL)` |
|     33 | 1145 | `		TK3('>','>','=',T_SR_EQUAL)` |
|     33 | 1146 | `		TK3('?','?','=',T_COALESCE_EQUAL)` |
|     33 | 1147 | `		TK2('=','=',T_IS_EQUAL)` |
|     33 | 1148 | `		TK2('!','=',T_IS_NOT_EQUAL)` |
|     33 | 1149 | `		TK2('<','>',T_IS_NOT_EQUAL)` |
|     33 | 1150 | `		TK2('<','=',T_IS_SMALLER_OR_EQUAL)` |
|     33 | 1151 | `		TK2('>','=',T_IS_GREATER_OR_EQUAL)` |
|     33 | 1152 | `		TK2('&','&',T_BOOLEAN_AND)` |
|     33 | 1153 | `		TK2('\|','\|',T_BOOLEAN_OR)` |
|     33 | 1154 | `		TK2('\|','>',T_PIPE)` |
|     33 | 1155 | `		TK2('+','+',T_INC)` |
|     33 | 1156 | `		TK2('-','-',T_DEC)` |
|     33 | 1157 | `		TK2('=','>',T_DOUBLE_ARROW)` |
|     33 | 1158 | `		TK2('<','<',T_SL)` |
|     33 | 1159 | `		TK2('>','>',T_SR)` |
|     33 | 1160 | `		TK2('*','*',T_POW)` |
|     33 | 1161 | `		TK2('?','?',T_COALESCE)` |
|     33 | 1162 | `		TK2('+','=',T_PLUS_EQUAL)` |
|     33 | 1163 | `		TK2('-','=',T_MINUS_EQUAL)` |
|     33 | 1164 | `		TK2('*','=',T_MUL_EQUAL)` |
|     33 | 1165 | `		TK2('/','=',T_DIV_EQUAL)` |
|     33 | 1166 | `		TK2('.','=',T_CONCAT_EQUAL)` |
|     33 | 1167 | `		TK2('%','=',T_MOD_EQUAL)` |
|     33 | 1168 | `		TK2('&','=',T_AND_EQUAL)` |
|     33 | 1169 | `		TK2('\|','=',T_OR_EQUAL)` |
|     33 | 1170 | `		TK2('^','=',T_XOR_EQUAL)` |
|      - | 1171 | `		#undef TK3` |
|      - | 1172 | `		#undef TK2` |
|      - | 1173 | `	}` |
|      - | 1174 | `	/* Ampersand: FOLLOWED (by var / vararg) vs NOT, looking past whitespace. */` |
|     33 | 1175 | `	if( c=='&' ){` |
|    ! 0 | 1176 | `		const unsigned char *p = z+1;` |
|      - | 1177 | `		int id;` |
|    ! 0 | 1178 | `		while( p < ts->zEnd && tok_is_ws(*p) ){ p++; }` |
|    ! 0 | 1179 | `		if( (p < ts->zEnd && *p=='$') \|\|` |
|    ! 0 | 1180 | `			(p+2 < ts->zEnd && p[0]=='.' && p[1]=='.' && p[2]=='.') ){` |
|    ! 0 | 1181 | `			id = T_AMPERSAND_FOLLOWED_BY_VAR_OR_VARARG;` |
|    ! 0 | 1182 | `		}else{` |
|    ! 0 | 1183 | `			id = T_AMPERSAND_NOT_FOLLOWED_BY_VAR_OR_VARARG;` |
|      - | 1184 | `		}` |
|    ! 0 | 1185 | `		tok_tok(ts,id,"&",1,ts->iLine);` |
|    ! 0 | 1186 | `		ts->z++;` |
|    ! 0 | 1187 | `		return 0;` |
|      - | 1188 | `	}` |
|      - | 1189 | `	/* Control bytes that begin no token are T_BAD_CHARACTER in php. */` |
|     33 | 1190 | `	if( c < 0x20 \|\| c == 0x7f ){` |
|    ! 0 | 1191 | `		char ch = (char)c;` |
|    ! 0 | 1192 | `		tok_tok(ts,T_BAD_CHARACTER,&ch,1,ts->iLine);` |
|    ! 0 | 1193 | `		ts->z++;` |
|    ! 0 | 1194 | `		return 0;` |
|      - | 1195 | `	}` |
|      - | 1196 | `	/* Single-char token, returned as a bare string. */` |
|      - | 1197 | `	{` |
|     33 | 1198 | `		char ch = (char)c;` |
|     33 | 1199 | `		tok_plain(ts,&ch,1);` |
|     33 | 1200 | `		ts->z++;` |
|     33 | 1201 | `		if( c=='{' ) return '{';` |
|     33 | 1202 | `		if( c=='}' ) return '}';` |
|     33 | 1203 | `		return 0;` |
|      - | 1204 | `	}` |
|     57 | 1205 | `}` |
|      - | 1206 |  |
|      - | 1207 | `/* Emit T_OPEN_TAG/T_OPEN_TAG_WITH_ECHO at ts->z; returns 1 if a tag was found. */` |
|     17 | 1208 | `static int tok_open_tag(tok_state *ts){` |
|     17 | 1209 | `	const unsigned char *z = ts->z;` |
|     17 | 1210 | `	if( z+2 < ts->zEnd && z[0]=='<' && z[1]=='?' && z[2]=='=' ){` |
|    ! 0 | 1211 | `		tok_tok(ts,T_OPEN_TAG_WITH_ECHO,"<?=",3,ts->iLine);` |
|    ! 0 | 1212 | `		ts->z = z+3;` |
|    ! 0 | 1213 | `		return 1;` |
|      - | 1214 | `	}` |
|     16 | 1215 | `	if( z+4 <= ts->zEnd && z[0]=='<' && z[1]=='?'` |
|     17 | 1216 | `		&& tok_lower(z[2])=='p' && tok_lower(z[3])=='h' && z+5 <= ts->zEnd && tok_lower(z[4])=='p' ){` |
|      - | 1217 | `		/* Require <?php to be followed by whitespace or EOF. */` |
|     17 | 1218 | `		const unsigned char *p = z+5;` |
|     17 | 1219 | `		if( p >= ts->zEnd \|\| tok_is_ws(*p) ){` |
|     17 | 1220 | `			const unsigned char *e = z+5;` |
|     17 | 1221 | `			int iLine = ts->iLine;` |
|     17 | 1222 | `			if( e < ts->zEnd && tok_is_ws(*e) ){` |
|     17 | 1223 | `				if( tok_at_nl(e,ts->zEnd) ){ ts->iLine++; }` |
|     17 | 1224 | `				e++;                 /* one trailing whitespace char joins the tag */` |
|      8 | 1225 | `			}` |
|     17 | 1226 | `			tok_tok(ts,T_OPEN_TAG,(const char *)z,(int)(e-z),iLine);` |
|     17 | 1227 | `			ts->z = e;` |
|     17 | 1228 | `			return 1;` |
|      - | 1229 | `		}` |
|    ! 0 | 1230 | `	}` |
|    ! 0 | 1231 | `	return 0;` |
|      9 | 1232 | `}` |
|      - | 1233 |  |
|      - | 1234 | `/* The scanning driver: alternate inline-HTML and PHP modes over the source. */` |
|     17 | 1235 | `static void tok_run(tok_state *ts){` |
|     33 | 1236 | `	while( ts->z < ts->zEnd && !ts->bOOM && !ts->bStop ){` |
|      - | 1237 | `		/* Inline HTML until the next open tag. */` |
|     17 | 1238 | `		const unsigned char *zHtml = ts->z;` |
|     17 | 1239 | `		int iHtmlLine = ts->iLine;` |
|     17 | 1240 | `		while( ts->z < ts->zEnd ){` |
|     17 | 1241 | `			if( ts->z[0]=='<' && ts->z+1 < ts->zEnd && ts->z[1]=='?' ){` |
|      - | 1242 | `				/* Only <?php and <?= are recognised (short tags off). */` |
|     17 | 1243 | `				const unsigned char *s = ts->z;` |
|     17 | 1244 | `				int bTag = 0;` |
|     17 | 1245 | `				if( s+2 < ts->zEnd && s[2]=='=' ){ bTag = 1; }` |
|     16 | 1246 | `				else if( s+4 <= ts->zEnd && tok_lower(s[2])=='p' && tok_lower(s[3])=='h'` |
|     16 | 1247 | `					&& s+5 <= ts->zEnd && tok_lower(s[4])=='p'` |
|     17 | 1248 | `					&& (s+5 >= ts->zEnd \|\| tok_is_ws(s[5])) ){ bTag = 1; }` |
|     17 | 1249 | `				if( bTag ){ break; }` |
|    ! 0 | 1250 | `			}` |
|    ! 0 | 1251 | `			if( tok_at_nl(ts->z,ts->zEnd) ){ ts->iLine++; }` |
|    ! 0 | 1252 | `			ts->z++;` |
|    ! 0 | 1253 | `		}` |
|     17 | 1254 | `		if( ts->z > zHtml ){` |
|    ! 0 | 1255 | `			tok_tok(ts,T_INLINE_HTML,(const char *)zHtml,(int)(ts->z-zHtml),iHtmlLine);` |
|    ! 0 | 1256 | `		}` |
|     17 | 1257 | `		if( ts->z >= ts->zEnd ){ break; }` |
|     17 | 1258 | `		if( !tok_open_tag(ts) ){` |
|      - | 1259 | `			/* Not actually a tag (shouldn't happen given the check above). */` |
|    ! 0 | 1260 | `			if( tok_at_nl(ts->z,ts->zEnd) ){ ts->iLine++; }` |
|    ! 0 | 1261 | `			ts->z++;` |
|    ! 0 | 1262 | `			continue;` |
|      - | 1263 | `		}` |
|      - | 1264 | `		/* PHP mode until a close tag or EOF. */` |
|     17 | 1265 | `		ts->bProp = 0;` |
|    129 | 1266 | `		while( ts->z < ts->zEnd && !ts->bOOM ){` |
|    113 | 1267 | `			int eff = tok_lex_one(ts);` |
|    113 | 1268 | `			if( eff < 0 ){ break; }` |
|      1 | 1269 | `		}` |
|      1 | 1270 | `	}` |
|     17 | 1271 | `}` |
|      - | 1272 |  |
|      - | 1273 | `/*` |
|      - | 1274 | ` * array token_get_all(string $source [, int $flags = 0 ])` |
|      - | 1275 | ` */` |
|    ! 0 | 1276 | `static int PH7_builtin_token_get_all(ph7_context *pCtx,int nArg,ph7_value **apArg){` |
|      - | 1277 | `	tok_state ts;` |
|      - | 1278 | `	const char *zSrc;` |
|    ! 0 | 1279 | `	int nSrc = 0;` |
|    ! 0 | 1280 | `	if( nArg < 1 ){` |
|    ! 0 | 1281 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 1282 | `			"token_get_all() expects at least 1 argument, %d given",nArg);` |
|      - | 1283 | `	}` |
|    ! 0 | 1284 | `	zSrc = ph7_value_to_string(apArg[0],&nSrc);` |
|    ! 0 | 1285 | `	SyZero(&ts,sizeof(ts));` |
|    ! 0 | 1286 | `	ts.pCtx  = pCtx;` |
|    ! 0 | 1287 | `	ts.pArray = ph7_context_new_array(pCtx);` |
|    ! 0 | 1288 | `	ts.pS    = ph7_context_new_scalar(pCtx);` |
|    ! 0 | 1289 | `	ts.pId   = ph7_context_new_scalar(pCtx);` |
|    ! 0 | 1290 | `	ts.pText = ph7_context_new_scalar(pCtx);` |
|    ! 0 | 1291 | `	ts.pLine = ph7_context_new_scalar(pCtx);` |
|    ! 0 | 1292 | `	if( ts.pArray==0 \|\| ts.pS==0 \|\| ts.pId==0 \|\| ts.pText==0 \|\| ts.pLine==0 ){` |
|    ! 0 | 1293 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1294 | `	}` |
|    ! 0 | 1295 | `	ts.z    = (const unsigned char *)zSrc;` |
|    ! 0 | 1296 | `	ts.zEnd = ts.z + (nSrc > 0 ? (sxu32)nSrc : 0);` |
|    ! 0 | 1297 | `	ts.iLine = 1;` |
|    ! 0 | 1298 | `	if( nArg > 1 && (ph7_value_to_int(apArg[1]) & TOK_TOKEN_PARSE) ){` |
|    ! 0 | 1299 | `		ts.bParse = 1;` |
|    ! 0 | 1300 | `	}` |
|    ! 0 | 1301 | `	tok_run(&ts);` |
|    ! 0 | 1302 | `	if( ts.bOOM ){` |
|    ! 0 | 1303 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1304 | `	}` |
|    ! 0 | 1305 | `	ph7_result_value(pCtx,ts.pArray);` |
|    ! 0 | 1306 | `	return PH7_OK;` |
|    ! 0 | 1307 | `}` |
|      - | 1308 |  |
|      - | 1309 | `/*` |
|      - | 1310 | ` * string token_name(int $id)` |
|      - | 1311 | ` */` |
|    ! 0 | 1312 | `static int PH7_builtin_token_name(ph7_context *pCtx,int nArg,ph7_value **apArg){` |
|      - | 1313 | `	int iId;` |
|      - | 1314 | `	const char *zName;` |
|    ! 0 | 1315 | `	if( nArg < 1 ){` |
|    ! 0 | 1316 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 1317 | `			"token_name() expects exactly 1 argument, %d given",nArg);` |
|      - | 1318 | `	}` |
|    ! 0 | 1319 | `	iId = ph7_value_to_int(apArg[0]);` |
|    ! 0 | 1320 | `	zName = TokConstName(iId);` |
|    ! 0 | 1321 | `	if( zName == 0 ){` |
|    ! 0 | 1322 | `		ph7_result_string(pCtx,"UNKNOWN",(int)sizeof("UNKNOWN")-1);` |
|    ! 0 | 1323 | `	}else{` |
|    ! 0 | 1324 | `		ph7_result_string(pCtx,zName,-1/*SyStrlen*/);` |
|      - | 1325 | `	}` |
|    ! 0 | 1326 | `	return PH7_OK;` |
|    ! 0 | 1327 | `}` |
|      - | 1328 |  |
|      - | 1329 | `/*` |
|      - | 1330 | ` * ---------------------------------------------------------------------------` |
|      - | 1331 | ` * The PhpToken class, declared and bodied in C.` |
|      - | 1332 | ` *` |
|      - | 1333 | `` * php's is NOT final and `tokenize()` returns `static[]` -- subclassing is the`` |
|      - | 1334 | ` * documented way to attach behaviour to a token stream, and the embedded PHP` |
|      - | 1335 | `` * declared it `final`, which made `class MyToken extends PhpToken` a fatal here`` |
|      - | 1336 | ` * and works in php. Its CONSTRUCTOR is what php marks final instead, which is` |
|      - | 1337 | `` * why php can (and does) skip it: `tokenize()` builds each instance and writes`` |
|      - | 1338 | ` * the four slots directly, so a subclass's own declared properties keep their` |
|      - | 1339 | ` * defaults.` |
|      - | 1340 | ` *` |
|      - | 1341 | ` * The four properties are php's typed-and-UNINITIALIZED shape` |
|      - | 1342 | `` * (`public int $id;` -- no default at all), so a read before construction is`` |
|      - | 1343 | ` * php's "must not be accessed before initialization" Error, and the four` |
|      - | 1344 | ` * methods that need a slot say so with php's own text rather than reading a` |
|      - | 1345 | ` * zero.` |
|      - | 1346 | ` * ---------------------------------------------------------------------------` |
|      - | 1347 | ` */` |
|      - | 1348 | `/* php's php_token_get_id / php_token_get_text: the slot, or the Error php` |
|      - | 1349 | ` * raises for an object that was never constructed. */` |
|     40 | 1350 | `static ph7_value * TokSlot(ph7_context *pCtx,const char *zName,sxi32 *pRc)` |
|      1 | 1351 | `{` |
|     41 | 1352 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     41 | 1353 | `	ph7_value *pVal = pThis ? PH7_NativeAttr(pThis,zName) : 0;` |
|     41 | 1354 | `	*pRc = PH7_OK;` |
|     41 | 1355 | `	if( pVal == 0 \|\| PH7_NativeAttrIsUninit(pThis,zName) ){` |
|     13 | 1356 | `		*pRc = PH7_VmThrowException(pCtx,"Error",` |
|      4 | 1357 | `			"Typed property PhpToken::$%s must not be accessed before initialization",zName);` |
|      9 | 1358 | `		return 0;` |
|      - | 1359 | `	}` |
|     33 | 1360 | `	return pVal;` |
|     21 | 1361 | `}` |
|      - | 1362 | `/* PhpToken::__construct(int $id, string $text, int $line = -1, int $pos = -1) */` |
|      2 | 1363 | `static int vm_builtin_PhpToken_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1364 | `{` |
|      3 | 1365 | `	ph7_vm *pVm = pCtx->pVm;` |
|      3 | 1366 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 1367 | `	const char *zText;` |
|      3 | 1368 | `	int nText = 0;` |
|      3 | 1369 | `	if( pThis == 0 \|\| nArg < 2 ){` |
|    ! 0 | 1370 | `		return PH7_OK;` |
|      - | 1371 | `	}` |
|      3 | 1372 | `	zText = ph7_value_to_string(apArg[1],&nText);` |
|      3 | 1373 | `	PH7_NativeSetAttrInt(pVm,pThis,"id",ph7_value_to_int64(apArg[0]));` |
|      3 | 1374 | `	PH7_NativeSetAttrStr(pVm,pThis,"text",zText,nText);` |
|      3 | 1375 | `	PH7_NativeSetAttrInt(pVm,pThis,"line",nArg > 2 ? ph7_value_to_int64(apArg[2]) : -1);` |
|      3 | 1376 | `	PH7_NativeSetAttrInt(pVm,pThis,"pos",nArg > 3 ? ph7_value_to_int64(apArg[3]) : -1);` |
|      3 | 1377 | `	return PH7_OK;` |
|      2 | 1378 | `}` |
|      - | 1379 | `/*` |
|      - | 1380 | ` * PhpToken::tokenize(string $code, int $flags = 0): static[]` |
|      - | 1381 | ` *` |
|      - | 1382 | ` * The scanner token_get_all() drives, told to emit INSTANCES of the CALLED` |
|      - | 1383 | `` * class -- php's own `token_class` branch, and the reason the position and the`` |
|      - | 1384 | ` * line are the scanner's own rather than a running total recomputed in PHP.` |
|      - | 1385 | ` */` |
|     18 | 1386 | `static int vm_builtin_PhpToken_tokenize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1387 | `{` |
|     19 | 1388 | `	ph7_class *pClass = PH7_ContextCalledClass(pCtx);` |
|      - | 1389 | `	tok_state ts;` |
|      - | 1390 | `	const char *zSrc;` |
|     19 | 1391 | `	int nSrc = 0;` |
|     19 | 1392 | `	if( pClass == 0 ){` |
|    ! 0 | 1393 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1394 | `	}` |
|     19 | 1395 | `	if( pClass->iFlags & PH7_CLASS_ABSTRACT ){` |
|      - | 1396 | `		/* php checks the construction precondition ONCE, before scanning. */` |
|      4 | 1397 | `		return PH7_VmThrowException(pCtx,"Error","Cannot instantiate abstract class %z",` |
|      1 | 1398 | `			&pClass->sName);` |
|      - | 1399 | `	}` |
|     17 | 1400 | `	zSrc = nArg > 0 ? ph7_value_to_string(apArg[0],&nSrc) : "";` |
|     17 | 1401 | `	SyZero(&ts,sizeof(ts));` |
|     17 | 1402 | `	ts.pCtx = pCtx;` |
|     17 | 1403 | `	ts.pArray = ph7_context_new_array(pCtx);` |
|     17 | 1404 | `	ts.pS    = ph7_context_new_scalar(pCtx);` |
|     17 | 1405 | `	ts.pId   = ph7_context_new_scalar(pCtx);` |
|     17 | 1406 | `	ts.pText = ph7_context_new_scalar(pCtx);` |
|     17 | 1407 | `	ts.pLine = ph7_context_new_scalar(pCtx);` |
|     17 | 1408 | `	if( ts.pArray == 0 \|\| ts.pS == 0 \|\| ts.pId == 0 \|\| ts.pText == 0 \|\| ts.pLine == 0 ){` |
|    ! 0 | 1409 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1410 | `	}` |
|     17 | 1411 | `	ts.pTokClass = pClass;` |
|     17 | 1412 | `	ts.z    = (const unsigned char *)zSrc;` |
|     17 | 1413 | `	ts.zEnd = ts.z + (nSrc > 0 ? (sxu32)nSrc : 0);` |
|     17 | 1414 | `	ts.iLine = 1;` |
|     17 | 1415 | `	if( nArg > 1 && (ph7_value_to_int(apArg[1]) & TOK_TOKEN_PARSE) ){` |
|    ! 0 | 1416 | `		ts.bParse = 1;` |
|    ! 0 | 1417 | `	}` |
|     17 | 1418 | `	tok_run(&ts);` |
|     17 | 1419 | `	if( ts.bOOM ){` |
|    ! 0 | 1420 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1421 | `	}` |
|     17 | 1422 | `	ph7_result_value(pCtx,ts.pArray);` |
|     17 | 1423 | `	return PH7_OK;` |
|     10 | 1424 | `}` |
|      - | 1425 | `/* One arm of is(): an int matches the id, a string matches the text. */` |
|     24 | 1426 | `static int TokMatch(ph7_context *pCtx,ph7_value *pKind,int *pbYes,sxi32 *pRc)` |
|      1 | 1427 | `{` |
|      - | 1428 | `	ph7_value *pSlot;` |
|     25 | 1429 | `	*pbYes = 0;` |
|     25 | 1430 | `	*pRc = PH7_OK;` |
|     25 | 1431 | `	if( pKind->iFlags & MEMOBJ_INT ){` |
|     11 | 1432 | `		pSlot = TokSlot(pCtx,"id",pRc);` |
|     11 | 1433 | `		if( pSlot == 0 ){` |
|      3 | 1434 | `			return 0;` |
|      - | 1435 | `		}` |
|      9 | 1436 | `		*pbYes = ph7_value_to_int64(pSlot) == pKind->x.iVal;` |
|      9 | 1437 | `		return 1;` |
|      - | 1438 | `	}` |
|     15 | 1439 | `	if( pKind->iFlags & MEMOBJ_STRING ){` |
|      9 | 1440 | `		int nText = 0,nKind = 0;` |
|      9 | 1441 | `		const char *zKind = ph7_value_to_string(pKind,&nKind);` |
|      - | 1442 | `		const char *zText;` |
|      9 | 1443 | `		pSlot = TokSlot(pCtx,"text",pRc);` |
|      9 | 1444 | `		if( pSlot == 0 ){` |
|    ! 0 | 1445 | `			return 0;` |
|      - | 1446 | `		}` |
|      9 | 1447 | `		zText = ph7_value_to_string(pSlot,&nText);` |
|     12 | 1448 | `		*pbYes = nText == nKind && (nText < 1 \|\| SyMemcmp(zText,zKind,(sxu32)nText) == 0);` |
|      9 | 1449 | `		return 1;` |
|      - | 1450 | `	}` |
|      7 | 1451 | `	return -1;   /* neither: the caller words php's TypeError */` |
|     13 | 1452 | `}` |
|      - | 1453 | `/*` |
|      - | 1454 | ` * PhpToken::is(int\|string\|array $kind): bool` |
|      - | 1455 | ` *` |
|      - | 1456 | ` * php screens the argument ITSELF (the parameter is untyped, so the shared ZPP` |
|      - | 1457 | ` * cannot), and words two different refusals: one for the argument, one for an` |
|      - | 1458 | ` * ELEMENT of an array argument. The embedded PHP screened neither and answered` |
|      - | 1459 | ` * false for a float, a null and a bad element alike.` |
|      - | 1460 | ` */` |
|     22 | 1461 | `static int vm_builtin_PhpToken_is(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1462 | `{` |
|     23 | 1463 | `	ph7_value *pKind = nArg > 0 ? apArg[0] : 0;` |
|     23 | 1464 | `	int bYes = 0;` |
|      - | 1465 | `	sxi32 rc;` |
|     23 | 1466 | `	if( pKind == 0 ){` |
|    ! 0 | 1467 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1468 | `		return PH7_OK;` |
|      - | 1469 | `	}` |
|     23 | 1470 | `	if( pKind->iFlags & MEMOBJ_HASHMAP ){` |
|     11 | 1471 | `		ph7_hashmap *pMap = (ph7_hashmap *)pKind->x.pOther;` |
|      - | 1472 | `		ph7_hashmap_node *pNode;` |
|      - | 1473 | `		sxu32 n;` |
|      - | 1474 | `		/* Insertion order is pFirst then the pPrev chain (rule 12), walked` |
|      - | 1475 | `		 * directly rather than through the shared loop cursor -- this is the` |
|      - | 1476 | `		 * CALLER's array and php's iteration does not move its pointer. */` |
|     17 | 1477 | `		for( n = 0, pNode = pMap->pFirst ; pNode && n < pMap->nEntry ;` |
|      7 | 1478 | `			 ++n, pNode = pNode->pPrev ){` |
|      - | 1479 | `			ph7_value sVal;` |
|      - | 1480 | `			int iRc;` |
|     13 | 1481 | `			PH7_MemObjInit(pCtx->pVm,&sVal);` |
|     13 | 1482 | `			PH7_HashmapExtractNodeValue(pNode,&sVal,FALSE);` |
|     13 | 1483 | `			iRc = TokMatch(pCtx,&sVal,&bYes,&rc);` |
|     13 | 1484 | `			if( iRc < 0 ){` |
|      - | 1485 | `				char zGiven[64];` |
|    ! 0 | 1486 | `				sxi32 rcT = PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1487 | `					"PhpToken::is(): Argument #1 ($kind) must only have elements of type "` |
|    ! 0 | 1488 | `					"string\|int, %s given",VmValueGivenName(&sVal,zGiven,sizeof(zGiven)));` |
|    ! 0 | 1489 | `				PH7_MemObjRelease(&sVal);` |
|    ! 0 | 1490 | `				return rcT;` |
|      - | 1491 | `			}` |
|     13 | 1492 | `			PH7_MemObjRelease(&sVal);` |
|     13 | 1493 | `			if( iRc == 0 ){` |
|    ! 0 | 1494 | `				return rc;` |
|      - | 1495 | `			}` |
|     13 | 1496 | `			if( bYes ){` |
|      7 | 1497 | `				ph7_result_bool(pCtx,1);` |
|      7 | 1498 | `				return PH7_OK;` |
|      - | 1499 | `			}` |
|      4 | 1500 | `		}` |
|      5 | 1501 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1502 | `		return PH7_OK;` |
|      - | 1503 | `	}` |
|      - | 1504 | `	{` |
|      - | 1505 | `		char zGiven[64];` |
|     13 | 1506 | `		int iRc = TokMatch(pCtx,pKind,&bYes,&rc);` |
|     13 | 1507 | `		if( iRc < 0 ){` |
|     11 | 1508 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1509 | `				"PhpToken::is(): Argument #1 ($kind) must be of type string\|int\|array, %s given",` |
|      3 | 1510 | `				VmValueGivenName(pKind,zGiven,sizeof(zGiven)));` |
|      - | 1511 | `		}` |
|      7 | 1512 | `		if( iRc == 0 ){` |
|      3 | 1513 | `			return rc;` |
|      - | 1514 | `		}` |
|      - | 1515 | `	}` |
|      5 | 1516 | `	ph7_result_bool(pCtx,bYes);` |
|      5 | 1517 | `	return PH7_OK;` |
|     12 | 1518 | `}` |
|      - | 1519 | `/* PhpToken::isIgnorable(): bool — php's four "not part of the program" ids. */` |
|      2 | 1520 | `static int vm_builtin_PhpToken_isIgnorable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1521 | `{` |
|      - | 1522 | `	sxi32 rc;` |
|      3 | 1523 | `	ph7_value *pSlot = TokSlot(pCtx,"id",&rc);` |
|      - | 1524 | `	sxi64 iId;` |
|      1 | 1525 | `	SXUNUSED(nArg);` |
|      1 | 1526 | `	SXUNUSED(apArg);` |
|      3 | 1527 | `	if( pSlot == 0 ){` |
|      3 | 1528 | `		return rc;` |
|      - | 1529 | `	}` |
|    ! 0 | 1530 | `	iId = ph7_value_to_int64(pSlot);` |
|    ! 0 | 1531 | `	ph7_result_bool(pCtx,iId == T_WHITESPACE \|\| iId == T_COMMENT` |
|    ! 0 | 1532 | `		\|\| iId == T_DOC_COMMENT \|\| iId == T_OPEN_TAG);` |
|    ! 0 | 1533 | `	return PH7_OK;` |
|      2 | 1534 | `}` |
|      - | 1535 | `/* PhpToken::getTokenName(): ?string — the CHARACTER for a single-byte token,` |
|      - | 1536 | ` * the T_* name for a known id, and null for anything else. */` |
|     18 | 1537 | `static int vm_builtin_PhpToken_getTokenName(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1538 | `{` |
|      - | 1539 | `	sxi32 rc;` |
|     19 | 1540 | `	ph7_value *pSlot = TokSlot(pCtx,"id",&rc);` |
|      - | 1541 | `	const char *zName;` |
|      - | 1542 | `	sxi64 iId;` |
|      9 | 1543 | `	SXUNUSED(nArg);` |
|      9 | 1544 | `	SXUNUSED(apArg);` |
|     19 | 1545 | `	if( pSlot == 0 ){` |
|      3 | 1546 | `		return rc;` |
|      - | 1547 | `	}` |
|     17 | 1548 | `	iId = ph7_value_to_int64(pSlot);` |
|     17 | 1549 | `	if( iId < 256 ){` |
|      5 | 1550 | `		char c = (char)iId;` |
|      5 | 1551 | `		ph7_result_string(pCtx,&c,1);` |
|      5 | 1552 | `		return PH7_OK;` |
|      - | 1553 | `	}` |
|     13 | 1554 | `	zName = TokConstName((int)iId);` |
|     13 | 1555 | `	if( zName == 0 ){` |
|    ! 0 | 1556 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1557 | `		return PH7_OK;` |
|      - | 1558 | `	}` |
|     13 | 1559 | `	ph7_result_string(pCtx,zName,-1/*SyStrlen*/);` |
|     13 | 1560 | `	return PH7_OK;` |
|     10 | 1561 | `}` |
|      2 | 1562 | `static int vm_builtin_PhpToken_toString(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1563 | `{` |
|      - | 1564 | `	sxi32 rc;` |
|      3 | 1565 | `	ph7_value *pSlot = TokSlot(pCtx,"text",&rc);` |
|      - | 1566 | `	const char *zText;` |
|      3 | 1567 | `	int nText = 0;` |
|      1 | 1568 | `	SXUNUSED(nArg);` |
|      1 | 1569 | `	SXUNUSED(apArg);` |
|      3 | 1570 | `	if( pSlot == 0 ){` |
|      3 | 1571 | `		return rc;` |
|      - | 1572 | `	}` |
|    ! 0 | 1573 | `	zText = ph7_value_to_string(pSlot,&nText);` |
|    ! 0 | 1574 | `	ph7_result_string(pCtx,zText,nText);` |
|    ! 0 | 1575 | `	return PH7_OK;` |
|      2 | 1576 | `}` |
|      - | 1577 | `/*` |
|      - | 1578 | ` * The declaration. Method ORDER is tokenizer.stub.php's — tokenize() first,` |
|      - | 1579 | ` * then the final constructor — and the four properties are declared with NO` |
|      - | 1580 | ` * default, which is what makes them php's uninitialized typed slots.` |
|      - | 1581 | ` */` |
|   4670 | 1582 | `static sxi32 VmInstallPhpToken(ph7_vm *pVm)` |
|      5 | 1583 | `{` |
|      - | 1584 | `	static const PH7_NativePropDef aTokProp[] = {` |
|      - | 1585 | `		{ "id",   PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|      - | 1586 | `		{ "text", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - | 1587 | `		{ "line", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|      - | 1588 | `		{ "pos",  PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|      - | 1589 | `	};` |
|      - | 1590 | `	static const PH7_NativeMethodDef aTokMethod[] = {` |
|      - | 1591 | `		{ "tokenize",     PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "string $code, int $flags = 0", "array",` |
|      - | 1592 | `		  vm_builtin_PhpToken_tokenize },` |
|      - | 1593 | `		{ "__construct",  PH7_MOD_PUBLIC\|PH7_MOD_FINAL,` |
|      - | 1594 | `		  "int $id, string $text, int $line = -1, int $pos = -1", 0,` |
|      - | 1595 | `		  vm_builtin_PhpToken_construct },` |
|      - | 1596 | `		{ "is",           PH7_MOD_PUBLIC, "$kind", "bool", vm_builtin_PhpToken_is },` |
|      - | 1597 | `		{ "isIgnorable",  PH7_MOD_PUBLIC, "", "bool", vm_builtin_PhpToken_isIgnorable },` |
|      - | 1598 | `		{ "getTokenName", PH7_MOD_PUBLIC, "", "?string", vm_builtin_PhpToken_getTokenName },` |
|      - | 1599 | `		{ "__toString",   PH7_MOD_PUBLIC, "", "string", vm_builtin_PhpToken_toString },` |
|      - | 1600 | `	};` |
|      - | 1601 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - | 1602 | `		{ "PhpToken", 0, "Stringable", 0,` |
|      - | 1603 | `		  aTokMethod, SX_ARRAYSIZE(aTokMethod), 0, 0,` |
|      - | 1604 | `		  aTokProp, SX_ARRAYSIZE(aTokProp), 0, 0, 0 },` |
|      - | 1605 | `	};` |
|   4675 | 1606 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|      5 | 1607 | `}` |
|      - | 1608 |  |
|   4675 | 1609 | `PH7_PRIVATE sxi32 PH7_VmInstallTokenizer(ph7_vm *pVm){` |
|   4675 | 1610 | `	ph7_create_function(&(*pVm),"token_get_all",PH7_builtin_token_get_all,0);` |
|   4675 | 1611 | `	ph7_create_function(&(*pVm),"token_name",PH7_builtin_token_name,0);` |
|   4675 | 1612 | `	return VmInstallPhpToken(&(*pVm));` |
|      5 | 1613 | `}` |
|      - | 1614 |  |
|      - | 1615 | `#else /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 1616 |  |
|      - | 1617 | `/* Tiny build: no tokenizer builtins/class (the whole builtin layer is off). */` |
|      - | 1618 | `PH7_PRIVATE sxi32 PH7_VmInstallTokenizer(ph7_vm *pVm){ (void)pVm; return SXRET_OK; }` |
|      - | 1619 |  |
|      - | 1620 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 1621 |  |
