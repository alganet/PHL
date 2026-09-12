# src/ph7/vm_builtin_tokenizer.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 12/720 lines (1.67%)

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
|    ! 0 |  249 | `static void TokConstExpand(ph7_value *pVal,void *pUserData)` |
|    ! 0 |  250 | `{` |
|    ! 0 |  251 | `	ph7_value_int(pVal,SX_PTR_TO_INT(pUserData));` |
|    ! 0 |  252 | `}` |
|      - |  253 |  |
|   3414 |  254 | `PH7_PRIVATE void PH7_RegisterTokenizerConstants(ph7_vm *pVm)` |
|      5 |  255 | `{` |
|      - |  256 | `	sxu32 n;` |
| 529175 |  257 | `	for( n = 0 ; n < SX_ARRAYSIZE(aTokConst) ; ++n ){` |
| 788639 |  258 | `		ph7_create_constant(&(*pVm),aTokConst[n].zName,TokConstExpand,` |
| 525756 |  259 | `			SX_INT_TO_PTR(aTokConst[n].iId));` |
| 262883 |  260 | `	}` |
|   3419 |  261 | `}` |
|      - |  262 |  |
|      - |  263 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |  264 |  |
|      - |  265 | `/*` |
|      - |  266 | ` * Reverse lookup for token_name(): the canonical php name for an id, or 0 for` |
|      - |  267 | ` * unknown/out-of-range ids (token_name() then returns "UNKNOWN").` |
|      - |  268 | ` */` |
|    ! 0 |  269 | `static const char * TokConstName(int iId)` |
|    ! 0 |  270 | `{` |
|      - |  271 | `	sxu32 n;` |
|    ! 0 |  272 | `	if( iId < 256 ){` |
|    ! 0 |  273 | `		return 0; /* single-char and TOKEN_PARSE ids are "UNKNOWN" in token_name() */` |
|      - |  274 | `	}` |
|    ! 0 |  275 | `	for( n = 0 ; n < SX_ARRAYSIZE(aTokConst) ; ++n ){` |
|    ! 0 |  276 | `		if( aTokConst[n].iId == iId ){` |
|    ! 0 |  277 | `			return aTokConst[n].zName;` |
|      - |  278 | `		}` |
|    ! 0 |  279 | `	}` |
|    ! 0 |  280 | `	return 0;` |
|    ! 0 |  281 | `}` |
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
|      - |  319 | `	ph7_value   *pS;       /* reusable scalar for plain single-char string tokens */` |
|      - |  320 | `	ph7_value   *pId;      /* reusable scalar: tuple field [0] */` |
|      - |  321 | `	ph7_value   *pText;    /* reusable scalar: tuple field [1] */` |
|      - |  322 | `	ph7_value   *pLine;    /* reusable scalar: tuple field [2] */` |
|      - |  323 | `	const unsigned char *z;      /* cursor */` |
|      - |  324 | `	const unsigned char *zEnd;   /* one past end */` |
|      - |  325 | `	int          iLine;    /* current 1-based line at the cursor */` |
|      - |  326 | `	int          bProp;    /* one-shot: next identifier is a property -> T_STRING */` |
|      - |  327 | `	int          bStop;    /* __halt_compiler seen: stop scanning entirely */` |
|      - |  328 | `	int          bOOM;     /* memory failure flag */` |
|      - |  329 | `};` |
|      - |  330 |  |
|      - |  331 | `/* --- character classes (byte-level, UTF-8 lead bytes count as label chars) --- */` |
|    ! 0 |  332 | `static int tok_is_label_start(int c){` |
|    ! 0 |  333 | `	return c=='_' \|\| (c>='a'&&c<='z') \|\| (c>='A'&&c<='Z') \|\| c>=0x80;` |
|    ! 0 |  334 | `}` |
|    ! 0 |  335 | `static int tok_is_label(int c){` |
|    ! 0 |  336 | `	return tok_is_label_start(c) \|\| (c>='0'&&c<='9');` |
|    ! 0 |  337 | `}` |
|    ! 0 |  338 | `static int tok_is_ws(int c){` |
|      - |  339 | `	/* php's tokenizer whitespace is exactly [ \t\r\n]; \v and \f are T_BAD_CHARACTER. */` |
|    ! 0 |  340 | `	return c==' '\|\|c=='\t'\|\|c=='\n'\|\|c=='\r';` |
|    ! 0 |  341 | `}` |
|    ! 0 |  342 | `static int tok_lower(int c){` |
|    ! 0 |  343 | `	return (c>='A'&&c<='Z') ? c+32 : c;` |
|    ! 0 |  344 | `}` |
|    ! 0 |  345 | `static int tok_ci_eq(const char *z,int n,const char *zKw){` |
|      - |  346 | `	int i;` |
|    ! 0 |  347 | `	for( i = 0 ; i < n ; ++i ){` |
|    ! 0 |  348 | `		if( zKw[i]==0 \|\| tok_lower((unsigned char)z[i]) != (unsigned char)zKw[i] ){` |
|    ! 0 |  349 | `			return 0;` |
|      - |  350 | `		}` |
|    ! 0 |  351 | `	}` |
|    ! 0 |  352 | `	return zKw[n]==0;` |
|    ! 0 |  353 | `}` |
|      - |  354 |  |
|      - |  355 | `/* php counts a line ending as any of "\n", "\r", or "\r\n" (each = one line). */` |
|    ! 0 |  356 | `static int tok_at_nl(const unsigned char *p,const unsigned char *zEnd){` |
|    ! 0 |  357 | `	if( *p=='\n' ){ return 1; }` |
|    ! 0 |  358 | `	if( *p=='\r' && (p+1 >= zEnd \|\| p[1]!='\n') ){ return 1; }` |
|    ! 0 |  359 | `	return 0;` |
|    ! 0 |  360 | `}` |
|      - |  361 |  |
|      - |  362 | `/* Count line endings in [z,zEnd) and advance the running line counter. */` |
|    ! 0 |  363 | `static void tok_bump_lines(tok_state *ts,const unsigned char *z,const unsigned char *zEnd){` |
|    ! 0 |  364 | `	while( z < zEnd ){` |
|    ! 0 |  365 | `		if( tok_at_nl(z,zEnd) ){ ts->iLine++; }` |
|    ! 0 |  366 | `		z++;` |
|    ! 0 |  367 | `	}` |
|    ! 0 |  368 | `}` |
|      - |  369 |  |
|      - |  370 | `/* Emit a plain-string token (single-char / operator returned as a bare string). */` |
|    ! 0 |  371 | `static void tok_plain(tok_state *ts,const char *z,int n){` |
|    ! 0 |  372 | `	if( ts->bOOM ){ return; }` |
|    ! 0 |  373 | `	ph7_value_string(ts->pS,z,n);` |
|    ! 0 |  374 | `	if( ph7_array_add_elem(ts->pArray,0,ts->pS) != SXRET_OK ){ ts->bOOM = 1; }` |
|    ! 0 |  375 | `	ph7_value_reset_string_cursor(ts->pS);` |
|    ! 0 |  376 | `}` |
|      - |  377 |  |
|      - |  378 | `/* Emit a [id, text, line] token. */` |
|    ! 0 |  379 | `static void tok_tok(tok_state *ts,int iId,const char *z,int n,int iLine){` |
|      - |  380 | `	ph7_value *pInner;` |
|    ! 0 |  381 | `	if( ts->bOOM ){ return; }` |
|    ! 0 |  382 | `	pInner = ph7_context_new_array(ts->pCtx);` |
|    ! 0 |  383 | `	if( pInner == 0 ){ ts->bOOM = 1; return; }` |
|    ! 0 |  384 | `	ph7_value_int(ts->pId,iId);` |
|    ! 0 |  385 | `	ph7_value_string(ts->pText,z,n);` |
|    ! 0 |  386 | `	ph7_value_int(ts->pLine,iLine);` |
|    ! 0 |  387 | `	ph7_array_add_elem(pInner,0,ts->pId);` |
|    ! 0 |  388 | `	ph7_array_add_elem(pInner,0,ts->pText);` |
|    ! 0 |  389 | `	ph7_array_add_elem(pInner,0,ts->pLine);` |
|    ! 0 |  390 | `	if( ph7_array_add_elem(ts->pArray,0,pInner) != SXRET_OK ){ ts->bOOM = 1; }` |
|    ! 0 |  391 | `	ph7_context_release_value(ts->pCtx,pInner);` |
|    ! 0 |  392 | `	ph7_value_reset_string_cursor(ts->pText);` |
|    ! 0 |  393 | `}` |
|      - |  394 |  |
|      - |  395 | `/* Emit an encapsed-and-whitespace run [zStart, z) if non-empty, at iLine. */` |
|    ! 0 |  396 | `static void tok_encaps(tok_state *ts,const unsigned char *zStart,const unsigned char *z,int iLine){` |
|    ! 0 |  397 | `	if( z > zStart ){` |
|    ! 0 |  398 | `		tok_tok(ts,T_ENCAPSED_AND_WHITESPACE,(const char *)zStart,(int)(z-zStart),iLine);` |
|    ! 0 |  399 | `	}` |
|    ! 0 |  400 | `}` |
|      - |  401 |  |
|      - |  402 | `/* Does the integer literal (digits between z and zEnd, given radix) overflow` |
|      - |  403 | ` * ZEND_LONG_MAX? Underscores are ignored. */` |
|    ! 0 |  404 | `static int tok_int_overflows(const unsigned char *z,const unsigned char *zEnd,int radix){` |
|    ! 0 |  405 | `	sxu64 val = 0;` |
|    ! 0 |  406 | `	const sxu64 max = (sxu64)0x7FFFFFFFFFFFFFFF; /* PHP_INT_MAX */` |
|    ! 0 |  407 | `	while( z < zEnd ){` |
|    ! 0 |  408 | `		int c = *z++;` |
|      - |  409 | `		int d;` |
|    ! 0 |  410 | `		if( c=='_' ){ continue; }` |
|    ! 0 |  411 | `		if( c>='0'&&c<='9' ){ d = c-'0'; }` |
|    ! 0 |  412 | `		else if( c>='a'&&c<='f' ){ d = c-'a'+10; }` |
|    ! 0 |  413 | `		else if( c>='A'&&c<='F' ){ d = c-'A'+10; }` |
|    ! 0 |  414 | `		else { break; }` |
|    ! 0 |  415 | `		if( d >= radix ){ break; }` |
|    ! 0 |  416 | `		if( val > (max - (sxu64)d)/(sxu64)radix ){` |
|    ! 0 |  417 | `			return 1;` |
|      - |  418 | `		}` |
|    ! 0 |  419 | `		val = val*(sxu64)radix + (sxu64)d;` |
|    ! 0 |  420 | `	}` |
|    ! 0 |  421 | `	return 0;` |
|    ! 0 |  422 | `}` |
|      - |  423 |  |
|      - |  424 | `/* Forward decls for the mutually-recursive string / expression scanners. */` |
|      - |  425 | `static int  tok_lex_one(tok_state *ts);` |
|      - |  426 | `static void tok_scan_curly(tok_state *ts,int bVarname);` |
|      - |  427 |  |
|      - |  428 | `/*` |
|      - |  429 | ` * After a T_HALT_COMPILER token: php emits the "();" that follows as normal` |
|      - |  430 | ` * tokens and then the entire remainder of the source as a single T_INLINE_HTML.` |
|      - |  431 | ` */` |
|    ! 0 |  432 | `static void tok_halt_tail(tok_state *ts){` |
|    ! 0 |  433 | `	for(;;){` |
|    ! 0 |  434 | `		if( ts->z >= ts->zEnd ){ break; }` |
|    ! 0 |  435 | `		if( tok_is_ws(*ts->z) ){` |
|    ! 0 |  436 | `			const unsigned char *z0 = ts->z;` |
|    ! 0 |  437 | `			int iLine = ts->iLine;` |
|    ! 0 |  438 | `			while( ts->z < ts->zEnd && tok_is_ws(*ts->z) ){` |
|    ! 0 |  439 | `				if( tok_at_nl(ts->z,ts->zEnd) ){ ts->iLine++; }` |
|    ! 0 |  440 | `				ts->z++;` |
|    ! 0 |  441 | `			}` |
|    ! 0 |  442 | `			tok_tok(ts,T_WHITESPACE,(const char *)z0,(int)(ts->z-z0),iLine);` |
|    ! 0 |  443 | `			continue;` |
|      - |  444 | `		}` |
|    ! 0 |  445 | `		if( *ts->z=='(' \|\| *ts->z==')' ){` |
|    ! 0 |  446 | `			char ch = (char)*ts->z;` |
|    ! 0 |  447 | `			tok_plain(ts,&ch,1);` |
|    ! 0 |  448 | `			ts->z++;` |
|    ! 0 |  449 | `			continue;` |
|      - |  450 | `		}` |
|    ! 0 |  451 | `		if( *ts->z==';' ){` |
|    ! 0 |  452 | `			tok_plain(ts,";",1);` |
|    ! 0 |  453 | `			ts->z++;` |
|    ! 0 |  454 | `			break;` |
|      - |  455 | `		}` |
|    ! 0 |  456 | `		break;` |
|    ! 0 |  457 | `	}` |
|    ! 0 |  458 | `	if( ts->z < ts->zEnd ){` |
|    ! 0 |  459 | `		int iLine = ts->iLine;` |
|    ! 0 |  460 | `		tok_tok(ts,T_INLINE_HTML,(const char *)ts->z,(int)(ts->zEnd-ts->z),iLine);` |
|    ! 0 |  461 | `		tok_bump_lines(ts,ts->z,ts->zEnd);` |
|    ! 0 |  462 | `		ts->z = ts->zEnd;` |
|    ! 0 |  463 | `	}` |
|    ! 0 |  464 | `	ts->bStop = 1;` |
|    ! 0 |  465 | `}` |
|      - |  466 |  |
|      - |  467 | `/*` |
|      - |  468 | ` * Handle a possible interpolation construct at the cursor inside a double-quoted` |
|      - |  469 | ` * string or heredoc body. Returns 1 if it consumed one (emitting tokens), else 0` |
|      - |  470 | ` * (the caller keeps the char as literal). *pFlush is the start of the pending` |
|      - |  471 | ` * literal run and its line; on a hit we flush it first.` |
|      - |  472 | ` */` |
|    ! 0 |  473 | `static int tok_try_interp(tok_state *ts,const unsigned char **pLitStart,int *pLitLine){` |
|    ! 0 |  474 | `	const unsigned char *z = ts->z;` |
|    ! 0 |  475 | `	if( *z=='$' && z+1 < ts->zEnd && tok_is_label_start(z[1]) ){` |
|    ! 0 |  476 | `		const unsigned char *zVar = z+1;` |
|    ! 0 |  477 | `		int iLine = ts->iLine;` |
|    ! 0 |  478 | `		while( zVar < ts->zEnd && tok_is_label(*zVar) ){ zVar++; }` |
|    ! 0 |  479 | `		tok_encaps(ts,*pLitStart,z,*pLitLine);` |
|    ! 0 |  480 | `		tok_tok(ts,T_VARIABLE,(const char *)z,(int)(zVar-z),iLine);` |
|    ! 0 |  481 | `		ts->z = zVar;` |
|      - |  482 | `		/* one optional simple offset [...] or ->prop */` |
|    ! 0 |  483 | `		if( ts->z < ts->zEnd && *ts->z=='[' ){` |
|    ! 0 |  484 | `			tok_plain(ts,"[",1);` |
|    ! 0 |  485 | `			ts->z++;` |
|    ! 0 |  486 | `			if( ts->z < ts->zEnd && *ts->z=='$' && ts->z+1 < ts->zEnd && tok_is_label_start(ts->z[1]) ){` |
|    ! 0 |  487 | `				const unsigned char *v = ts->z+1;` |
|    ! 0 |  488 | `				while( v < ts->zEnd && tok_is_label(*v) ){ v++; }` |
|    ! 0 |  489 | `				tok_tok(ts,T_VARIABLE,(const char *)ts->z,(int)(v-ts->z),ts->iLine);` |
|    ! 0 |  490 | `				ts->z = v;` |
|    ! 0 |  491 | `			}else{` |
|    ! 0 |  492 | `				const unsigned char *n0 = ts->z;` |
|    ! 0 |  493 | `				if( ts->z < ts->zEnd && *ts->z=='-' ){` |
|    ! 0 |  494 | `					tok_plain(ts,"-",1);` |
|    ! 0 |  495 | `					ts->z++;` |
|    ! 0 |  496 | `					n0 = ts->z;` |
|    ! 0 |  497 | `				}` |
|    ! 0 |  498 | `				if( ts->z < ts->zEnd && *ts->z>='0' && *ts->z<='9' ){` |
|    ! 0 |  499 | `					while( ts->z < ts->zEnd && *ts->z>='0' && *ts->z<='9' ){ ts->z++; }` |
|    ! 0 |  500 | `					tok_tok(ts,T_NUM_STRING,(const char *)n0,(int)(ts->z-n0),ts->iLine);` |
|    ! 0 |  501 | `				}else if( ts->z < ts->zEnd && tok_is_label_start(*ts->z) ){` |
|    ! 0 |  502 | `					const unsigned char *l = ts->z;` |
|    ! 0 |  503 | `					while( l < ts->zEnd && tok_is_label(*l) ){ l++; }` |
|    ! 0 |  504 | `					tok_tok(ts,T_STRING,(const char *)ts->z,(int)(l-ts->z),ts->iLine);` |
|    ! 0 |  505 | `					ts->z = l;` |
|    ! 0 |  506 | `				}` |
|      - |  507 | `			}` |
|    ! 0 |  508 | `			if( ts->z < ts->zEnd && *ts->z==']' ){ tok_plain(ts,"]",1); ts->z++; }` |
|    ! 0 |  509 | `		}else if( ts->z+2 < ts->zEnd && ts->z[0]=='-' && ts->z[1]=='>' && tok_is_label_start(ts->z[2]) ){` |
|      - |  510 | `			const unsigned char *l;` |
|    ! 0 |  511 | `			tok_tok(ts,T_OBJECT_OPERATOR,"->",2,ts->iLine);` |
|    ! 0 |  512 | `			ts->z += 2;` |
|    ! 0 |  513 | `			l = ts->z;` |
|    ! 0 |  514 | `			while( l < ts->zEnd && tok_is_label(*l) ){ l++; }` |
|    ! 0 |  515 | `			tok_tok(ts,T_STRING,(const char *)ts->z,(int)(l-ts->z),ts->iLine);` |
|    ! 0 |  516 | `			ts->z = l;` |
|    ! 0 |  517 | `		}` |
|    ! 0 |  518 | `		*pLitStart = ts->z;` |
|    ! 0 |  519 | `		*pLitLine  = ts->iLine;` |
|    ! 0 |  520 | `		return 1;` |
|      - |  521 | `	}` |
|    ! 0 |  522 | `	if( *z=='{' && z+1 < ts->zEnd && z[1]=='$' ){` |
|    ! 0 |  523 | `		tok_encaps(ts,*pLitStart,z,*pLitLine);` |
|    ! 0 |  524 | `		tok_tok(ts,T_CURLY_OPEN,"{",1,ts->iLine);` |
|    ! 0 |  525 | `		ts->z = z+1;` |
|    ! 0 |  526 | `		tok_scan_curly(ts,0);` |
|    ! 0 |  527 | `		*pLitStart = ts->z;` |
|    ! 0 |  528 | `		*pLitLine  = ts->iLine;` |
|    ! 0 |  529 | `		return 1;` |
|      - |  530 | `	}` |
|    ! 0 |  531 | `	if( *z=='$' && z+1 < ts->zEnd && z[1]=='{' ){` |
|    ! 0 |  532 | `		tok_encaps(ts,*pLitStart,z,*pLitLine);` |
|    ! 0 |  533 | `		tok_tok(ts,T_DOLLAR_OPEN_CURLY_BRACES,"${",2,ts->iLine);` |
|    ! 0 |  534 | `		ts->z = z+2;` |
|    ! 0 |  535 | `		tok_scan_curly(ts,1);` |
|    ! 0 |  536 | `		*pLitStart = ts->z;` |
|    ! 0 |  537 | `		*pLitLine  = ts->iLine;` |
|    ! 0 |  538 | `		return 1;` |
|      - |  539 | `	}` |
|    ! 0 |  540 | `	return 0;` |
|    ! 0 |  541 | `}` |
|      - |  542 |  |
|      - |  543 | `/*` |
|      - |  544 | ` * Scan a brace-delimited expression inside a string ("{$...}" or "${...}").` |
|      - |  545 | ` * The opening brace token was already emitted and ts->z points just past it.` |
|      - |  546 | ` * Emits PHP tokens until the matching close brace, which it emits as a plain` |
|      - |  547 | ` * "}" string token. bVarname: the first label (if immediately followed by '}'` |
|      - |  548 | ` * or '[') is a T_STRING_VARNAME (the "${name}" form).` |
|      - |  549 | ` */` |
|    ! 0 |  550 | `static void tok_scan_curly(tok_state *ts,int bVarname){` |
|    ! 0 |  551 | `	int depth = 1;` |
|    ! 0 |  552 | `	if( bVarname && ts->z < ts->zEnd && tok_is_label_start(*ts->z) ){` |
|    ! 0 |  553 | `		const unsigned char *l = ts->z;` |
|    ! 0 |  554 | `		while( l < ts->zEnd && tok_is_label(*l) ){ l++; }` |
|    ! 0 |  555 | `		if( l < ts->zEnd && (*l=='}' \|\| *l=='[') ){` |
|    ! 0 |  556 | `			tok_tok(ts,T_STRING_VARNAME,(const char *)ts->z,(int)(l-ts->z),ts->iLine);` |
|    ! 0 |  557 | `			ts->z = l;` |
|    ! 0 |  558 | `		}` |
|    ! 0 |  559 | `	}` |
|    ! 0 |  560 | `	while( ts->z < ts->zEnd && depth > 0 && !ts->bOOM ){` |
|      - |  561 | `		int eff;` |
|    ! 0 |  562 | `		if( *ts->z=='}' ){` |
|    ! 0 |  563 | `			depth--;` |
|    ! 0 |  564 | `			if( depth == 0 ){` |
|    ! 0 |  565 | `				tok_plain(ts,"}",1);` |
|    ! 0 |  566 | `				ts->z++;` |
|    ! 0 |  567 | `				return;` |
|      - |  568 | `			}` |
|    ! 0 |  569 | `		}` |
|    ! 0 |  570 | `		eff = tok_lex_one(ts);` |
|    ! 0 |  571 | `		if( eff == '{' ){ depth++; }` |
|    ! 0 |  572 | `		else if( eff == '}' ){ depth--; if( depth==0 ){ return; } }` |
|    ! 0 |  573 | `		else if( eff < 0 ){ return; } /* close tag / EOF safety */` |
|    ! 0 |  574 | `	}` |
|    ! 0 |  575 | `}` |
|      - |  576 |  |
|      - |  577 | `/*` |
|      - |  578 | ` * Scan a double-quoted string or backtick string starting at the opening` |
|      - |  579 | ` * delimiter (ts->z points at it). If double-quoted with no interpolation the` |
|      - |  580 | ` * whole literal is one T_CONSTANT_ENCAPSED_STRING; otherwise it splits into the` |
|      - |  581 | ` * delimiter, encapsed runs and interpolation tokens.` |
|      - |  582 | ` */` |
|    ! 0 |  583 | `static void tok_scan_dquote(tok_state *ts,int chDelim){` |
|    ! 0 |  584 | `	const unsigned char *zOpen = ts->z;` |
|    ! 0 |  585 | `	int iOpenLine = ts->iLine;` |
|    ! 0 |  586 | `	const unsigned char *zScan = ts->z+1;` |
|    ! 0 |  587 | `	int bInterp = 0;` |
|    ! 0 |  588 | `	int bClosed = 0;` |
|      - |  589 | `	/* Peek for interpolation to decide constant-vs-split (only for '"'). */` |
|    ! 0 |  590 | `	while( zScan < ts->zEnd ){` |
|    ! 0 |  591 | `		int c = *zScan;` |
|    ! 0 |  592 | `		if( c=='\\' ){ zScan += 2; continue; }` |
|    ! 0 |  593 | `		if( c==chDelim ){ bClosed = 1; break; }` |
|    ! 0 |  594 | `		if( c=='$' && zScan+1 < ts->zEnd && (tok_is_label_start(zScan[1])\|\|zScan[1]=='{') ){ bInterp = 1; break; }` |
|    ! 0 |  595 | `		if( c=='{' && zScan+1 < ts->zEnd && zScan[1]=='$' ){ bInterp = 1; break; }` |
|    ! 0 |  596 | `		zScan++;` |
|    ! 0 |  597 | `	}` |
|    ! 0 |  598 | `	if( chDelim=='"' && !bInterp && bClosed ){` |
|      - |  599 | `		/* Whole constant string. Find the real closing quote. */` |
|    ! 0 |  600 | `		const unsigned char *z = ts->z+1;` |
|    ! 0 |  601 | `		while( z < ts->zEnd ){` |
|    ! 0 |  602 | `			if( *z=='\\' && z+1 < ts->zEnd ){ z += 2; continue; }` |
|    ! 0 |  603 | `			if( *z=='"' ){ break; }` |
|    ! 0 |  604 | `			z++;` |
|    ! 0 |  605 | `		}` |
|    ! 0 |  606 | `		if( z < ts->zEnd ){ z++; } /* include closing quote */` |
|    ! 0 |  607 | `		tok_tok(ts,T_CONSTANT_ENCAPSED_STRING,(const char *)zOpen,(int)(z-zOpen),iOpenLine);` |
|    ! 0 |  608 | `		tok_bump_lines(ts,ts->z,z);` |
|    ! 0 |  609 | `		ts->z = z;` |
|    ! 0 |  610 | `		return;` |
|      - |  611 | `	}` |
|      - |  612 | `	/* Split form: opening delimiter, then body, then closing delimiter. */` |
|      - |  613 | `	{` |
|    ! 0 |  614 | `		char d = (char)chDelim;` |
|      - |  615 | `		const unsigned char *litStart;` |
|      - |  616 | `		int litLine;` |
|    ! 0 |  617 | `		tok_plain(ts,&d,1);` |
|    ! 0 |  618 | `		ts->z++;` |
|    ! 0 |  619 | `		litStart = ts->z;` |
|    ! 0 |  620 | `		litLine  = ts->iLine;` |
|    ! 0 |  621 | `		while( ts->z < ts->zEnd && !ts->bOOM ){` |
|    ! 0 |  622 | `			int c = *ts->z;` |
|    ! 0 |  623 | `			if( c=='\\' && ts->z+1 < ts->zEnd ){` |
|    ! 0 |  624 | `				if( ts->z[1]=='\n' ){ ts->iLine++; } /* escaped newline still advances the line */` |
|    ! 0 |  625 | `				ts->z += 2;` |
|    ! 0 |  626 | `				continue;` |
|      - |  627 | `			}` |
|    ! 0 |  628 | `			if( c==chDelim ){` |
|    ! 0 |  629 | `				tok_encaps(ts,litStart,ts->z,litLine);` |
|    ! 0 |  630 | `				tok_plain(ts,&d,1);` |
|    ! 0 |  631 | `				ts->z++;` |
|    ! 0 |  632 | `				return;` |
|      - |  633 | `			}` |
|    ! 0 |  634 | `			if( c=='$' \|\| c=='{' ){` |
|    ! 0 |  635 | `				if( tok_try_interp(ts,&litStart,&litLine) ){ continue; }` |
|    ! 0 |  636 | `			}` |
|    ! 0 |  637 | `			if( tok_at_nl(ts->z,ts->zEnd) ){ ts->iLine++; }` |
|    ! 0 |  638 | `			ts->z++;` |
|    ! 0 |  639 | `		}` |
|      - |  640 | `		/* Unterminated: flush what we have. */` |
|    ! 0 |  641 | `		tok_encaps(ts,litStart,ts->z,litLine);` |
|      - |  642 | `	}` |
|    ! 0 |  643 | `}` |
|      - |  644 |  |
|      - |  645 | `/* Does the line at z (already at line start) begin the heredoc closing marker` |
|      - |  646 | ` * for zLabel? Returns the end of the label (past it) if so, else 0. */` |
|    ! 0 |  647 | `static const unsigned char * tok_heredoc_close(tok_state *ts,const unsigned char *z,` |
|    ! 0 |  648 | `		const char *zLabel,int nLabel){` |
|    ! 0 |  649 | `	const unsigned char *p = z;` |
|      - |  650 | `	int i;` |
|    ! 0 |  651 | `	while( p < ts->zEnd && (*p==' '\|\|*p=='\t') ){ p++; }` |
|    ! 0 |  652 | `	if( p + nLabel > ts->zEnd ){ return 0; } /* not enough bytes left for the label */` |
|    ! 0 |  653 | `	for( i = 0 ; i < nLabel ; ++i ){` |
|    ! 0 |  654 | `		if( p[i] != (unsigned char)zLabel[i] ){ return 0; }` |
|    ! 0 |  655 | `	}` |
|    ! 0 |  656 | `	p += nLabel;` |
|    ! 0 |  657 | `	if( p < ts->zEnd && tok_is_label(*p) ){ return 0; } /* label is a prefix of a longer word */` |
|    ! 0 |  658 | `	return p;` |
|    ! 0 |  659 | `}` |
|      - |  660 |  |
|      - |  661 | `/*` |
|      - |  662 | ` * Scan a heredoc/nowdoc starting at "<<<". ts->z points at the first '<'.` |
|      - |  663 | ` * Returns 1 if a valid heredoc was consumed, 0 if this "<<<" is not a valid` |
|      - |  664 | ` * heredoc start (the caller then falls through to operator lexing: T_SL + '<').` |
|      - |  665 | ` * A valid start is: "<<<" [ws]* ["\|']? LABEL ["\|']? immediately followed by` |
|      - |  666 | ` * an optional '\r' and a '\n'.` |
|      - |  667 | ` */` |
|    ! 0 |  668 | `static int tok_scan_heredoc(tok_state *ts){` |
|    ! 0 |  669 | `	const unsigned char *zStart = ts->z;` |
|    ! 0 |  670 | `	int iStartLine = ts->iLine;` |
|    ! 0 |  671 | `	const unsigned char *z = ts->z+3;` |
|    ! 0 |  672 | `	int bNowdoc = 0;` |
|    ! 0 |  673 | `	int chQuote = 0;` |
|      - |  674 | `	const unsigned char *zLabel;` |
|      - |  675 | `	int nLabel;` |
|      - |  676 | `	const unsigned char *litStart;` |
|      - |  677 | `	int litLine;` |
|      - |  678 | `	/* optional spaces/tabs after <<< */` |
|    ! 0 |  679 | `	while( z < ts->zEnd && (*z==' '\|\|*z=='\t') ){ z++; }` |
|    ! 0 |  680 | `	if( z < ts->zEnd && (*z=='"' \|\| *z=='\'') ){` |
|    ! 0 |  681 | `		chQuote = *z;` |
|    ! 0 |  682 | `		bNowdoc = (*z=='\'');` |
|    ! 0 |  683 | `		z++;` |
|    ! 0 |  684 | `	}` |
|    ! 0 |  685 | `	zLabel = z;` |
|    ! 0 |  686 | `	while( z < ts->zEnd && tok_is_label(*z) ){ z++; }` |
|    ! 0 |  687 | `	nLabel = (int)(z - zLabel);` |
|    ! 0 |  688 | `	if( nLabel < 1 ){ return 0; } /* no label -> not a heredoc */` |
|    ! 0 |  689 | `	if( chQuote ){` |
|    ! 0 |  690 | `		if( z < ts->zEnd && *z==chQuote ){ z++; }` |
|    ! 0 |  691 | `		else { return 0; } /* unbalanced quote -> not a heredoc */` |
|    ! 0 |  692 | `	}` |
|      - |  693 | `	/* The label must be immediately followed by an optional '\r' then '\n'. */` |
|    ! 0 |  694 | `	if( z < ts->zEnd && *z=='\r' && z+1 < ts->zEnd && z[1]=='\n' ){ z += 2; }` |
|    ! 0 |  695 | `	else if( z < ts->zEnd && *z=='\n' ){ z += 1; }` |
|    ! 0 |  696 | `	else { return 0; } /* junk or EOF after the marker -> not a heredoc */` |
|      - |  697 | `	/* Emit T_START_HEREDOC (delimiters + trailing newline). */` |
|    ! 0 |  698 | `	tok_tok(ts,T_START_HEREDOC,(const char *)zStart,(int)(z-zStart),iStartLine);` |
|    ! 0 |  699 | `	tok_bump_lines(ts,zStart,z);` |
|    ! 0 |  700 | `	ts->z = z;` |
|      - |  701 | `	/* Scan body line by line until the closing marker. */` |
|    ! 0 |  702 | `	litStart = ts->z;` |
|    ! 0 |  703 | `	litLine  = ts->iLine;` |
|    ! 0 |  704 | `	while( ts->z < ts->zEnd && !ts->bOOM ){` |
|      - |  705 | `		/* At a line start, test for the closing marker. */` |
|    ! 0 |  706 | `		const unsigned char *pClose = tok_heredoc_close(ts,ts->z,(const char *)zLabel,nLabel);` |
|    ! 0 |  707 | `		if( pClose ){` |
|      - |  708 | `			int iCloseLine;` |
|    ! 0 |  709 | `			tok_encaps(ts,litStart,ts->z,litLine);` |
|    ! 0 |  710 | `			iCloseLine = ts->iLine;` |
|    ! 0 |  711 | `			tok_tok(ts,T_END_HEREDOC,(const char *)ts->z,(int)(pClose-ts->z),iCloseLine);` |
|    ! 0 |  712 | `			ts->z = pClose;` |
|    ! 0 |  713 | `			return 1;` |
|      - |  714 | `		}` |
|      - |  715 | `		/* Process one line's content. */` |
|    ! 0 |  716 | `		while( ts->z < ts->zEnd ){` |
|    ! 0 |  717 | `			int c = *ts->z;` |
|    ! 0 |  718 | `			if( c=='\n' ){ ts->iLine++; ts->z++; break; }` |
|    ! 0 |  719 | `			if( c=='\r' && (ts->z+1>=ts->zEnd \|\| ts->z[1]!='\n') ){ ts->iLine++; ts->z++; continue; }` |
|    ! 0 |  720 | `			if( !bNowdoc && (c=='\\') && ts->z+1 < ts->zEnd ){` |
|    ! 0 |  721 | `				if( ts->z[1]=='\n' ){ ts->iLine++; } /* escaped newline still advances the line */` |
|    ! 0 |  722 | `				ts->z += 2;` |
|    ! 0 |  723 | `				continue;` |
|      - |  724 | `			}` |
|    ! 0 |  725 | `			if( !bNowdoc && (c=='$' \|\| c=='{') ){` |
|    ! 0 |  726 | `				if( tok_try_interp(ts,&litStart,&litLine) ){ continue; }` |
|    ! 0 |  727 | `			}` |
|    ! 0 |  728 | `			ts->z++;` |
|    ! 0 |  729 | `		}` |
|    ! 0 |  730 | `	}` |
|    ! 0 |  731 | `	tok_encaps(ts,litStart,ts->z,litLine);` |
|    ! 0 |  732 | `	return 1;` |
|    ! 0 |  733 | `}` |
|      - |  734 |  |
|      - |  735 | `/* One cast keyword between the parens, e.g. "int". Returns the cast T_* id or 0. */` |
|    ! 0 |  736 | `static int tok_cast_id(const char *z,int n){` |
|    ! 0 |  737 | `	if( tok_ci_eq(z,n,"int") \|\| tok_ci_eq(z,n,"integer") ) return T_INT_CAST;` |
|    ! 0 |  738 | `	if( tok_ci_eq(z,n,"float") \|\| tok_ci_eq(z,n,"double") \|\| tok_ci_eq(z,n,"real") ) return T_DOUBLE_CAST;` |
|    ! 0 |  739 | `	if( tok_ci_eq(z,n,"string") \|\| tok_ci_eq(z,n,"binary") ) return T_STRING_CAST;` |
|    ! 0 |  740 | `	if( tok_ci_eq(z,n,"array") ) return T_ARRAY_CAST;` |
|    ! 0 |  741 | `	if( tok_ci_eq(z,n,"object") ) return T_OBJECT_CAST;` |
|    ! 0 |  742 | `	if( tok_ci_eq(z,n,"bool") \|\| tok_ci_eq(z,n,"boolean") ) return T_BOOL_CAST;` |
|    ! 0 |  743 | `	if( tok_ci_eq(z,n,"unset") ) return T_UNSET_CAST;` |
|    ! 0 |  744 | `	return 0;` |
|    ! 0 |  745 | `}` |
|      - |  746 |  |
|      - |  747 | `/*` |
|      - |  748 | ` * Try to lex "( <ws>? castword <ws>? )" at ts->z (pointing at '('). On success` |
|      - |  749 | ` * emits the cast token and returns 1; otherwise returns 0 and consumes nothing.` |
|      - |  750 | ` */` |
|    ! 0 |  751 | `static int tok_try_cast(tok_state *ts){` |
|    ! 0 |  752 | `	const unsigned char *p = ts->z+1;` |
|      - |  753 | `	const unsigned char *w0,*w1;` |
|      - |  754 | `	int id;` |
|    ! 0 |  755 | `	while( p < ts->zEnd && (*p==' '\|\|*p=='\t') ){ p++; }` |
|    ! 0 |  756 | `	w0 = p;` |
|    ! 0 |  757 | `	while( p < ts->zEnd && tok_is_label(*p) ){ p++; }` |
|    ! 0 |  758 | `	w1 = p;` |
|    ! 0 |  759 | `	if( w1 == w0 ){ return 0; }` |
|    ! 0 |  760 | `	id = tok_cast_id((const char *)w0,(int)(w1-w0));` |
|    ! 0 |  761 | `	if( id == 0 ){ return 0; }` |
|    ! 0 |  762 | `	while( p < ts->zEnd && (*p==' '\|\|*p=='\t') ){ p++; }` |
|    ! 0 |  763 | `	if( p >= ts->zEnd \|\| *p != ')' ){ return 0; }` |
|    ! 0 |  764 | `	p++;` |
|    ! 0 |  765 | `	tok_tok(ts,id,(const char *)ts->z,(int)(p-ts->z),ts->iLine);` |
|    ! 0 |  766 | `	ts->z = p;` |
|    ! 0 |  767 | `	return 1;` |
|    ! 0 |  768 | `}` |
|      - |  769 |  |
|      - |  770 | `/*` |
|      - |  771 | ` * Lex exactly one PHP-mode token and emit it. Returns:` |
|      - |  772 | ` *   0   normal token` |
|      - |  773 | ` *  '{' / '}'  when it emitted a bare '{' / '}' (for curly-expr brace tracking)` |
|      - |  774 | ` *  -1  when it emitted a close tag or hit EOF (leave PHP mode)` |
|      - |  775 | ` * Whitespace and comments preserve ts->bProp; every other token consumes it.` |
|      - |  776 | ` */` |
|    ! 0 |  777 | `static int tok_lex_one(tok_state *ts){` |
|    ! 0 |  778 | `	const unsigned char *z = ts->z;` |
|      - |  779 | `	int c;` |
|      - |  780 | `	int bWasProp;` |
|    ! 0 |  781 | `	if( z >= ts->zEnd ){ return -1; }` |
|    ! 0 |  782 | `	c = *z;` |
|      - |  783 | `	/* Whitespace run (preserves property state). */` |
|    ! 0 |  784 | `	if( tok_is_ws(c) ){` |
|    ! 0 |  785 | `		const unsigned char *z0 = z;` |
|    ! 0 |  786 | `		int iLine = ts->iLine;` |
|    ! 0 |  787 | `		while( ts->z < ts->zEnd && tok_is_ws(*ts->z) ){` |
|    ! 0 |  788 | `			if( tok_at_nl(ts->z,ts->zEnd) ){ ts->iLine++; }` |
|    ! 0 |  789 | `			ts->z++;` |
|    ! 0 |  790 | `		}` |
|    ! 0 |  791 | `		tok_tok(ts,T_WHITESPACE,(const char *)z0,(int)(ts->z-z0),iLine);` |
|    ! 0 |  792 | `		return 0;` |
|      - |  793 | `	}` |
|      - |  794 | `	/* Comments: hash/slash-slash to EOL-or-close-tag, and block comments. Preserve property state. */` |
|    ! 0 |  795 | `	if( c=='#' && !(z+1 < ts->zEnd && z[1]=='[') ){` |
|    ! 0 |  796 | `		const unsigned char *z0 = z;` |
|    ! 0 |  797 | `		int iLine = ts->iLine;` |
|    ! 0 |  798 | `		ts->z++;` |
|    ! 0 |  799 | `		while( ts->z < ts->zEnd && *ts->z!='\n' ){` |
|    ! 0 |  800 | `			if( *ts->z=='?' && ts->z+1 < ts->zEnd && ts->z[1]=='>' ){ break; }` |
|    ! 0 |  801 | `			ts->z++;` |
|    ! 0 |  802 | `		}` |
|    ! 0 |  803 | `		tok_tok(ts,T_COMMENT,(const char *)z0,(int)(ts->z-z0),iLine);` |
|    ! 0 |  804 | `		return 0;` |
|      - |  805 | `	}` |
|    ! 0 |  806 | `	if( c=='/' && z+1 < ts->zEnd && z[1]=='/' ){` |
|    ! 0 |  807 | `		const unsigned char *z0 = z;` |
|    ! 0 |  808 | `		int iLine = ts->iLine;` |
|    ! 0 |  809 | `		ts->z += 2;` |
|    ! 0 |  810 | `		while( ts->z < ts->zEnd && *ts->z!='\n' ){` |
|    ! 0 |  811 | `			if( *ts->z=='?' && ts->z+1 < ts->zEnd && ts->z[1]=='>' ){ break; }` |
|    ! 0 |  812 | `			ts->z++;` |
|    ! 0 |  813 | `		}` |
|    ! 0 |  814 | `		tok_tok(ts,T_COMMENT,(const char *)z0,(int)(ts->z-z0),iLine);` |
|    ! 0 |  815 | `		return 0;` |
|      - |  816 | `	}` |
|    ! 0 |  817 | `	if( c=='/' && z+1 < ts->zEnd && z[1]=='*' ){` |
|    ! 0 |  818 | `		const unsigned char *z0 = z;` |
|    ! 0 |  819 | `		int iLine = ts->iLine;` |
|      - |  820 | `		/* A doc comment is slash-star-star followed by a whitespace char (php's` |
|      - |  821 | `		 * rule): the third char must be '*' and the fourth must be whitespace. */` |
|    ! 0 |  822 | `		int bDoc = ( z+2 < ts->zEnd && z[2]=='*' && z+3 < ts->zEnd && tok_is_ws(z[3]) );` |
|    ! 0 |  823 | `		ts->z += 2;` |
|    ! 0 |  824 | `		while( ts->z < ts->zEnd ){` |
|    ! 0 |  825 | `			if( *ts->z=='*' && ts->z+1 < ts->zEnd && ts->z[1]=='/' ){ ts->z += 2; break; }` |
|    ! 0 |  826 | `			if( tok_at_nl(ts->z,ts->zEnd) ){ ts->iLine++; }` |
|    ! 0 |  827 | `			ts->z++;` |
|    ! 0 |  828 | `		}` |
|    ! 0 |  829 | `		tok_tok(ts,bDoc ? T_DOC_COMMENT : T_COMMENT,(const char *)z0,(int)(ts->z-z0),iLine);` |
|    ! 0 |  830 | `		return 0;` |
|      - |  831 | `	}` |
|      - |  832 | `	/* From here the token consumes property state. */` |
|    ! 0 |  833 | `	bWasProp = ts->bProp;` |
|    ! 0 |  834 | `	ts->bProp = 0;` |
|      - |  835 | `	/* Close tag. */` |
|    ! 0 |  836 | `	if( c=='?' && z+1 < ts->zEnd && z[1]=='>' ){` |
|    ! 0 |  837 | `		const unsigned char *z0 = z;` |
|    ! 0 |  838 | `		int iLine = ts->iLine;` |
|    ! 0 |  839 | `		ts->z += 2;` |
|      - |  840 | `		/* php swallows one trailing newline (\n or \r\n) into the close tag */` |
|    ! 0 |  841 | `		if( ts->z < ts->zEnd && *ts->z=='\r' && ts->z+1 < ts->zEnd && ts->z[1]=='\n' ){ ts->z += 2; ts->iLine++; }` |
|    ! 0 |  842 | `		else if( ts->z < ts->zEnd && *ts->z=='\n' ){ ts->z++; ts->iLine++; }` |
|    ! 0 |  843 | `		tok_tok(ts,T_CLOSE_TAG,(const char *)z0,(int)(ts->z-z0),iLine);` |
|    ! 0 |  844 | `		return -1;` |
|      - |  845 | `	}` |
|      - |  846 | `	/* #[ attribute open. */` |
|    ! 0 |  847 | `	if( c=='#' && z+1 < ts->zEnd && z[1]=='[' ){` |
|    ! 0 |  848 | `		tok_tok(ts,T_ATTRIBUTE,"#[",2,ts->iLine);` |
|    ! 0 |  849 | `		ts->z += 2;` |
|    ! 0 |  850 | `		return 0;` |
|      - |  851 | `	}` |
|      - |  852 | `	/* Variable. */` |
|    ! 0 |  853 | `	if( c=='$' && z+1 < ts->zEnd && tok_is_label_start(z[1]) ){` |
|    ! 0 |  854 | `		const unsigned char *v = z+1;` |
|    ! 0 |  855 | `		while( v < ts->zEnd && tok_is_label(*v) ){ v++; }` |
|    ! 0 |  856 | `		tok_tok(ts,T_VARIABLE,(const char *)z,(int)(v-z),ts->iLine);` |
|    ! 0 |  857 | `		ts->z = v;` |
|    ! 0 |  858 | `		return 0;` |
|      - |  859 | `	}` |
|      - |  860 | `	/* Namespaced name / identifier / keyword. */` |
|    ! 0 |  861 | `	if( tok_is_label_start(c) \|\| (c=='\\' && z+1 < ts->zEnd && tok_is_label_start(z[1])) ){` |
|    ! 0 |  862 | `		const unsigned char *p = z;` |
|    ! 0 |  863 | `		int bLeadBackslash = (c=='\\');` |
|    ! 0 |  864 | `		int bInner = 0;                 /* saw an internal backslash */` |
|      - |  865 | `		const unsigned char *firstLabelStart, *firstLabelEnd;` |
|    ! 0 |  866 | `		if( bLeadBackslash ){ p++; }` |
|    ! 0 |  867 | `		firstLabelStart = p;` |
|    ! 0 |  868 | `		while( p < ts->zEnd && tok_is_label(*p) ){ p++; }` |
|    ! 0 |  869 | `		firstLabelEnd = p;` |
|      - |  870 | `		/* Consume further \label segments. */` |
|    ! 0 |  871 | `		while( p+1 < ts->zEnd && *p=='\\' && tok_is_label_start(p[1]) ){` |
|    ! 0 |  872 | `			bInner = 1;` |
|    ! 0 |  873 | `			p++;` |
|    ! 0 |  874 | `			while( p < ts->zEnd && tok_is_label(*p) ){ p++; }` |
|    ! 0 |  875 | `		}` |
|    ! 0 |  876 | `		if( bLeadBackslash ){` |
|    ! 0 |  877 | `			tok_tok(ts,T_NAME_FULLY_QUALIFIED,(const char *)z,(int)(p-z),ts->iLine);` |
|    ! 0 |  878 | `			ts->z = p;` |
|    ! 0 |  879 | `			return 0;` |
|      - |  880 | `		}` |
|    ! 0 |  881 | `		if( bInner ){` |
|    ! 0 |  882 | `			int nFirst = (int)(firstLabelEnd - firstLabelStart);` |
|    ! 0 |  883 | `			int id = T_NAME_QUALIFIED;` |
|    ! 0 |  884 | `			if( tok_ci_eq((const char *)firstLabelStart,nFirst,"namespace") ){ id = T_NAME_RELATIVE; }` |
|    ! 0 |  885 | `			tok_tok(ts,id,(const char *)z,(int)(p-z),ts->iLine);` |
|    ! 0 |  886 | `			ts->z = p;` |
|    ! 0 |  887 | `			return 0;` |
|      - |  888 | `		}` |
|      - |  889 | `		/* Lone identifier: keyword, contextual, or T_STRING. */` |
|      - |  890 | `		{` |
|    ! 0 |  891 | `			int n = (int)(firstLabelEnd - z);` |
|    ! 0 |  892 | `			int id = T_STRING;` |
|      - |  893 | `			sxu32 k;` |
|    ! 0 |  894 | `			if( bWasProp ){` |
|    ! 0 |  895 | `				id = T_STRING;              /* property access after -> / ?-> */` |
|    ! 0 |  896 | `			}else if( tok_ci_eq((const char *)z,n,"enum") ){` |
|      - |  897 | `				/* Contextual: T_ENUM only when followed by <ws>+ then a label-start. */` |
|    ! 0 |  898 | `				const unsigned char *q = firstLabelEnd;` |
|    ! 0 |  899 | `				int sawWs = 0;` |
|    ! 0 |  900 | `				while( q < ts->zEnd && (*q==' '\|\|*q=='\t'\|\|*q=='\n'\|\|*q=='\r') ){ q++; sawWs = 1; }` |
|    ! 0 |  901 | `				if( sawWs && q < ts->zEnd && tok_is_label_start(*q) ){ id = T_ENUM; }` |
|    ! 0 |  902 | `			}else if( tok_ci_eq((const char *)z,n,"yield") ){` |
|      - |  903 | `				/* Contextual: "yield from" collapses to one T_YIELD_FROM. */` |
|    ! 0 |  904 | `				const unsigned char *q = firstLabelEnd;` |
|    ! 0 |  905 | `				const unsigned char *ws = q;` |
|    ! 0 |  906 | `				while( q < ts->zEnd && (*q==' '\|\|*q=='\t'\|\|*q=='\n'\|\|*q=='\r'\|\|*q=='\v'\|\|*q=='\f') ){ q++; }` |
|    ! 0 |  907 | `				if( q > ws && q+4 <= ts->zEnd && tok_ci_eq((const char *)q,4,"from")` |
|    ! 0 |  908 | `					&& (q+4 >= ts->zEnd \|\| !tok_is_label(q[4])) ){` |
|    ! 0 |  909 | `					const unsigned char *e = q+4;` |
|    ! 0 |  910 | `					tok_tok(ts,T_YIELD_FROM,(const char *)z,(int)(e-z),ts->iLine);` |
|    ! 0 |  911 | `					tok_bump_lines(ts,firstLabelEnd,e);` |
|    ! 0 |  912 | `					ts->z = e;` |
|    ! 0 |  913 | `					return 0;` |
|      - |  914 | `				}` |
|    ! 0 |  915 | `				id = T_YIELD;` |
|    ! 0 |  916 | `			}else{` |
|    ! 0 |  917 | `				for( k = 0 ; k < SX_ARRAYSIZE(aKeyword) ; ++k ){` |
|    ! 0 |  918 | `					if( tok_ci_eq((const char *)z,n,aKeyword[k].zName) ){` |
|    ! 0 |  919 | `						id = aKeyword[k].iId;` |
|    ! 0 |  920 | `						break;` |
|      - |  921 | `					}` |
|    ! 0 |  922 | `				}` |
|      - |  923 | `			}` |
|    ! 0 |  924 | `			if( id == T_STRING ){` |
|    ! 0 |  925 | `				tok_tok(ts,T_STRING,(const char *)z,n,ts->iLine);` |
|    ! 0 |  926 | `			}else{` |
|    ! 0 |  927 | `				tok_tok(ts,id,(const char *)z,n,ts->iLine);` |
|      - |  928 | `			}` |
|    ! 0 |  929 | `			ts->z = firstLabelEnd;` |
|    ! 0 |  930 | `			if( id == T_HALT_COMPILER ){` |
|    ! 0 |  931 | `				tok_halt_tail(ts);` |
|    ! 0 |  932 | `				return -1;` |
|      - |  933 | `			}` |
|    ! 0 |  934 | `			return 0;` |
|      - |  935 | `		}` |
|      - |  936 | `	}` |
|      - |  937 | `	/* Lone backslash -> namespace separator. */` |
|    ! 0 |  938 | `	if( c=='\\' ){` |
|    ! 0 |  939 | `		tok_tok(ts,T_NS_SEPARATOR,"\\",1,ts->iLine);` |
|    ! 0 |  940 | `		ts->z++;` |
|    ! 0 |  941 | `		return 0;` |
|      - |  942 | `	}` |
|      - |  943 | `	/* Number. */` |
|    ! 0 |  944 | `	if( (c>='0'&&c<='9') \|\| (c=='.' && z+1 < ts->zEnd && z[1]>='0' && z[1]<='9') ){` |
|    ! 0 |  945 | `		const unsigned char *p = z;` |
|    ! 0 |  946 | `		int isFloat = 0;` |
|      - |  947 | `		int id;` |
|    ! 0 |  948 | `		if( c=='0' && p+2 < ts->zEnd && (p[1]=='x'\|\|p[1]=='X')` |
|    ! 0 |  949 | `				&& ((p[2]>='0'&&p[2]<='9')\|\|(p[2]>='a'&&p[2]<='f')\|\|(p[2]>='A'&&p[2]<='F')) ){` |
|      - |  950 | `			const unsigned char *d0;` |
|    ! 0 |  951 | `			p += 2; d0 = p;` |
|    ! 0 |  952 | `			while( p < ts->zEnd && ((*p>='0'&&*p<='9')\|\|(*p>='a'&&*p<='f')\|\|(*p>='A'&&*p<='F')\|\|*p=='_') ){ p++; }` |
|    ! 0 |  953 | `			id = tok_int_overflows(d0,p,16) ? T_DNUMBER : T_LNUMBER;` |
|    ! 0 |  954 | `		}else if( c=='0' && p+2 < ts->zEnd && (p[1]=='b'\|\|p[1]=='B') && (p[2]=='0'\|\|p[2]=='1') ){` |
|      - |  955 | `			const unsigned char *d0;` |
|    ! 0 |  956 | `			p += 2; d0 = p;` |
|    ! 0 |  957 | `			while( p < ts->zEnd && (*p=='0'\|\|*p=='1'\|\|*p=='_') ){ p++; }` |
|    ! 0 |  958 | `			id = tok_int_overflows(d0,p,2) ? T_DNUMBER : T_LNUMBER;` |
|    ! 0 |  959 | `		}else if( c=='0' && p+2 < ts->zEnd && (p[1]=='o'\|\|p[1]=='O') && (p[2]>='0'&&p[2]<='7') ){` |
|      - |  960 | `			const unsigned char *d0;` |
|    ! 0 |  961 | `			p += 2; d0 = p;` |
|    ! 0 |  962 | `			while( p < ts->zEnd && ((*p>='0'&&*p<='7')\|\|*p=='_') ){ p++; }` |
|    ! 0 |  963 | `			id = tok_int_overflows(d0,p,8) ? T_DNUMBER : T_LNUMBER;` |
|    ! 0 |  964 | `		}else{` |
|    ! 0 |  965 | `			const unsigned char *intStart = p;` |
|    ! 0 |  966 | `			int allOctalDigits = 1;` |
|    ! 0 |  967 | `			while( p < ts->zEnd && ((*p>='0'&&*p<='9')\|\|*p=='_') ){` |
|    ! 0 |  968 | `				if( *p>'7' ){ allOctalDigits = 0; }` |
|    ! 0 |  969 | `				p++;` |
|    ! 0 |  970 | `			}` |
|    ! 0 |  971 | `			if( p < ts->zEnd && *p=='.' && !(c=='.') ){` |
|      - |  972 | `				/* fractional part (unless the token itself started with '.') */` |
|    ! 0 |  973 | `				isFloat = 1;` |
|    ! 0 |  974 | `				p++;` |
|    ! 0 |  975 | `				while( p < ts->zEnd && ((*p>='0'&&*p<='9')\|\|*p=='_') ){ p++; }` |
|    ! 0 |  976 | `			}else if( c=='.' ){` |
|    ! 0 |  977 | `				isFloat = 1; /* .5 style: leading dot already consumed below */` |
|    ! 0 |  978 | `			}` |
|    ! 0 |  979 | `			if( c=='.' ){` |
|      - |  980 | `				/* token began with '.': consume the dot + digits here */` |
|    ! 0 |  981 | `				p = z+1;` |
|    ! 0 |  982 | `				while( p < ts->zEnd && ((*p>='0'&&*p<='9')\|\|*p=='_') ){ p++; }` |
|    ! 0 |  983 | `				isFloat = 1;` |
|    ! 0 |  984 | `			}` |
|    ! 0 |  985 | `			if( p < ts->zEnd && (*p=='e'\|\|*p=='E') ){` |
|    ! 0 |  986 | `				const unsigned char *e = p+1;` |
|    ! 0 |  987 | `				if( e < ts->zEnd && (*e=='+'\|\|*e=='-') ){ e++; }` |
|    ! 0 |  988 | `				if( e < ts->zEnd && *e>='0' && *e<='9' ){` |
|    ! 0 |  989 | `					isFloat = 1;` |
|    ! 0 |  990 | `					p = e;` |
|    ! 0 |  991 | `					while( p < ts->zEnd && ((*p>='0'&&*p<='9')\|\|*p=='_') ){ p++; }` |
|    ! 0 |  992 | `				}` |
|    ! 0 |  993 | `			}` |
|    ! 0 |  994 | `			if( isFloat ){` |
|    ! 0 |  995 | `				id = T_DNUMBER;` |
|    ! 0 |  996 | `			}else if( intStart < ts->zEnd && *intStart=='0' && (p-intStart) > 1 && allOctalDigits ){` |
|      - |  997 | `				/* legacy octal 0NNN */` |
|    ! 0 |  998 | `				id = tok_int_overflows(intStart+1,p,8) ? T_DNUMBER : T_LNUMBER;` |
|    ! 0 |  999 | `			}else{` |
|    ! 0 | 1000 | `				id = tok_int_overflows(intStart,p,10) ? T_DNUMBER : T_LNUMBER;` |
|      - | 1001 | `			}` |
|      - | 1002 | `		}` |
|    ! 0 | 1003 | `		tok_tok(ts,id,(const char *)z,(int)(p-z),ts->iLine);` |
|    ! 0 | 1004 | `		ts->z = p;` |
|    ! 0 | 1005 | `		return 0;` |
|      - | 1006 | `	}` |
|      - | 1007 | `	/* Strings. */` |
|    ! 0 | 1008 | `	if( c=='\'' ){` |
|    ! 0 | 1009 | `		const unsigned char *p = z+1;` |
|    ! 0 | 1010 | `		int bClosed = 0;` |
|    ! 0 | 1011 | `		while( p < ts->zEnd ){` |
|    ! 0 | 1012 | `			if( *p=='\\' && p+1 < ts->zEnd ){ p += 2; continue; }` |
|    ! 0 | 1013 | `			if( *p=='\'' ){ p++; bClosed = 1; break; }` |
|    ! 0 | 1014 | `			p++;` |
|    ! 0 | 1015 | `		}` |
|      - | 1016 | `		/* An unterminated single-quoted string is one T_ENCAPSED_AND_WHITESPACE in php. */` |
|    ! 0 | 1017 | `		tok_tok(ts,bClosed ? T_CONSTANT_ENCAPSED_STRING : T_ENCAPSED_AND_WHITESPACE,` |
|    ! 0 | 1018 | `			(const char *)z,(int)(p-z),ts->iLine);` |
|    ! 0 | 1019 | `		tok_bump_lines(ts,z,p);` |
|    ! 0 | 1020 | `		ts->z = p;` |
|    ! 0 | 1021 | `		return 0;` |
|      - | 1022 | `	}` |
|    ! 0 | 1023 | `	if( c=='"' ){ tok_scan_dquote(ts,'"'); return 0; }` |
|    ! 0 | 1024 | ``	if( c=='`' ){ tok_scan_dquote(ts,'`'); return 0; }`` |
|      - | 1025 | `	/* Heredoc / nowdoc (falls through to operators if not a valid start). */` |
|    ! 0 | 1026 | `	if( c=='<' && z+2 < ts->zEnd && z[1]=='<' && z[2]=='<' ){` |
|    ! 0 | 1027 | `		if( tok_scan_heredoc(ts) ){ return 0; }` |
|    ! 0 | 1028 | `	}` |
|      - | 1029 | `	/* Cast operators. */` |
|    ! 0 | 1030 | `	if( c=='(' ){` |
|    ! 0 | 1031 | `		if( tok_try_cast(ts) ){ return 0; }` |
|    ! 0 | 1032 | `	}` |
|      - | 1033 | `	/* Object operators set the one-shot property state (next identifier -> T_STRING). */` |
|    ! 0 | 1034 | `	if( c=='?' && z+2 < ts->zEnd && z[1]=='-' && z[2]=='>' ){` |
|    ! 0 | 1035 | `		tok_tok(ts,T_NULLSAFE_OBJECT_OPERATOR,"?->",3,ts->iLine);` |
|    ! 0 | 1036 | `		ts->z += 3;` |
|    ! 0 | 1037 | `		ts->bProp = 1;` |
|    ! 0 | 1038 | `		return 0;` |
|      - | 1039 | `	}` |
|    ! 0 | 1040 | `	if( c=='-' && z+1 < ts->zEnd && z[1]=='>' ){` |
|    ! 0 | 1041 | `		tok_tok(ts,T_OBJECT_OPERATOR,"->",2,ts->iLine);` |
|    ! 0 | 1042 | `		ts->z += 2;` |
|    ! 0 | 1043 | `		ts->bProp = 1;` |
|    ! 0 | 1044 | `		return 0;` |
|      - | 1045 | `	}` |
|      - | 1046 | `	/* Multi-char and single-char operators (longest match first). */` |
|      - | 1047 | `	{` |
|    ! 0 | 1048 | `		const unsigned char *e = ts->zEnd;` |
|    ! 0 | 1049 | `		int r0 = c;` |
|    ! 0 | 1050 | `		int r1 = (z+1<e)?z[1]:-1;` |
|    ! 0 | 1051 | `		int r2 = (z+2<e)?z[2]:-1;` |
|      - | 1052 | `		#define TK3(a,b,cc,id) if(r0==(a)&&r1==(b)&&r2==(cc)){ tok_tok(ts,id,(const char*)z,3,ts->iLine); ts->z+=3; return 0; }` |
|      - | 1053 | `		#define TK2(a,b,id)    if(r0==(a)&&r1==(b)){ tok_tok(ts,id,(const char*)z,2,ts->iLine); ts->z+=2; return 0; }` |
|    ! 0 | 1054 | `		TK3('=','=','=',T_IS_IDENTICAL)` |
|    ! 0 | 1055 | `		TK3('!','=','=',T_IS_NOT_IDENTICAL)` |
|    ! 0 | 1056 | `		TK3('<','=','>',T_SPACESHIP)` |
|    ! 0 | 1057 | `		TK3('*','*','=',T_POW_EQUAL)` |
|    ! 0 | 1058 | `		TK3('.','.','.',T_ELLIPSIS)` |
|    ! 0 | 1059 | `		TK3('<','<','=',T_SL_EQUAL)` |
|    ! 0 | 1060 | `		TK3('>','>','=',T_SR_EQUAL)` |
|    ! 0 | 1061 | `		TK3('?','?','=',T_COALESCE_EQUAL)` |
|    ! 0 | 1062 | `		TK2('=','=',T_IS_EQUAL)` |
|    ! 0 | 1063 | `		TK2('!','=',T_IS_NOT_EQUAL)` |
|    ! 0 | 1064 | `		TK2('<','>',T_IS_NOT_EQUAL)` |
|    ! 0 | 1065 | `		TK2('<','=',T_IS_SMALLER_OR_EQUAL)` |
|    ! 0 | 1066 | `		TK2('>','=',T_IS_GREATER_OR_EQUAL)` |
|    ! 0 | 1067 | `		TK2('&','&',T_BOOLEAN_AND)` |
|    ! 0 | 1068 | `		TK2('\|','\|',T_BOOLEAN_OR)` |
|    ! 0 | 1069 | `		TK2('\|','>',T_PIPE)` |
|    ! 0 | 1070 | `		TK2('+','+',T_INC)` |
|    ! 0 | 1071 | `		TK2('-','-',T_DEC)` |
|    ! 0 | 1072 | `		TK2('=','>',T_DOUBLE_ARROW)` |
|    ! 0 | 1073 | `		TK2(':',':',T_DOUBLE_COLON)` |
|    ! 0 | 1074 | `		TK2('<','<',T_SL)` |
|    ! 0 | 1075 | `		TK2('>','>',T_SR)` |
|    ! 0 | 1076 | `		TK2('*','*',T_POW)` |
|    ! 0 | 1077 | `		TK2('?','?',T_COALESCE)` |
|    ! 0 | 1078 | `		TK2('+','=',T_PLUS_EQUAL)` |
|    ! 0 | 1079 | `		TK2('-','=',T_MINUS_EQUAL)` |
|    ! 0 | 1080 | `		TK2('*','=',T_MUL_EQUAL)` |
|    ! 0 | 1081 | `		TK2('/','=',T_DIV_EQUAL)` |
|    ! 0 | 1082 | `		TK2('.','=',T_CONCAT_EQUAL)` |
|    ! 0 | 1083 | `		TK2('%','=',T_MOD_EQUAL)` |
|    ! 0 | 1084 | `		TK2('&','=',T_AND_EQUAL)` |
|    ! 0 | 1085 | `		TK2('\|','=',T_OR_EQUAL)` |
|    ! 0 | 1086 | `		TK2('^','=',T_XOR_EQUAL)` |
|      - | 1087 | `		#undef TK3` |
|      - | 1088 | `		#undef TK2` |
|      - | 1089 | `	}` |
|      - | 1090 | `	/* Ampersand: FOLLOWED (by var / vararg) vs NOT, looking past whitespace. */` |
|    ! 0 | 1091 | `	if( c=='&' ){` |
|    ! 0 | 1092 | `		const unsigned char *p = z+1;` |
|      - | 1093 | `		int id;` |
|    ! 0 | 1094 | `		while( p < ts->zEnd && tok_is_ws(*p) ){ p++; }` |
|    ! 0 | 1095 | `		if( (p < ts->zEnd && *p=='$') \|\|` |
|    ! 0 | 1096 | `			(p+2 < ts->zEnd && p[0]=='.' && p[1]=='.' && p[2]=='.') ){` |
|    ! 0 | 1097 | `			id = T_AMPERSAND_FOLLOWED_BY_VAR_OR_VARARG;` |
|    ! 0 | 1098 | `		}else{` |
|    ! 0 | 1099 | `			id = T_AMPERSAND_NOT_FOLLOWED_BY_VAR_OR_VARARG;` |
|      - | 1100 | `		}` |
|    ! 0 | 1101 | `		tok_tok(ts,id,"&",1,ts->iLine);` |
|    ! 0 | 1102 | `		ts->z++;` |
|    ! 0 | 1103 | `		return 0;` |
|      - | 1104 | `	}` |
|      - | 1105 | `	/* Control bytes that begin no token are T_BAD_CHARACTER in php. */` |
|    ! 0 | 1106 | `	if( c < 0x20 \|\| c == 0x7f ){` |
|    ! 0 | 1107 | `		char ch = (char)c;` |
|    ! 0 | 1108 | `		tok_tok(ts,T_BAD_CHARACTER,&ch,1,ts->iLine);` |
|    ! 0 | 1109 | `		ts->z++;` |
|    ! 0 | 1110 | `		return 0;` |
|      - | 1111 | `	}` |
|      - | 1112 | `	/* Single-char token, returned as a bare string. */` |
|      - | 1113 | `	{` |
|    ! 0 | 1114 | `		char ch = (char)c;` |
|    ! 0 | 1115 | `		tok_plain(ts,&ch,1);` |
|    ! 0 | 1116 | `		ts->z++;` |
|    ! 0 | 1117 | `		if( c=='{' ) return '{';` |
|    ! 0 | 1118 | `		if( c=='}' ) return '}';` |
|    ! 0 | 1119 | `		return 0;` |
|      - | 1120 | `	}` |
|    ! 0 | 1121 | `}` |
|      - | 1122 |  |
|      - | 1123 | `/* Emit T_OPEN_TAG/T_OPEN_TAG_WITH_ECHO at ts->z; returns 1 if a tag was found. */` |
|    ! 0 | 1124 | `static int tok_open_tag(tok_state *ts){` |
|    ! 0 | 1125 | `	const unsigned char *z = ts->z;` |
|    ! 0 | 1126 | `	if( z+2 < ts->zEnd && z[0]=='<' && z[1]=='?' && z[2]=='=' ){` |
|    ! 0 | 1127 | `		tok_tok(ts,T_OPEN_TAG_WITH_ECHO,"<?=",3,ts->iLine);` |
|    ! 0 | 1128 | `		ts->z = z+3;` |
|    ! 0 | 1129 | `		return 1;` |
|      - | 1130 | `	}` |
|    ! 0 | 1131 | `	if( z+4 <= ts->zEnd && z[0]=='<' && z[1]=='?'` |
|    ! 0 | 1132 | `		&& tok_lower(z[2])=='p' && tok_lower(z[3])=='h' && z+5 <= ts->zEnd && tok_lower(z[4])=='p' ){` |
|      - | 1133 | `		/* Require <?php to be followed by whitespace or EOF. */` |
|    ! 0 | 1134 | `		const unsigned char *p = z+5;` |
|    ! 0 | 1135 | `		if( p >= ts->zEnd \|\| tok_is_ws(*p) ){` |
|    ! 0 | 1136 | `			const unsigned char *e = z+5;` |
|    ! 0 | 1137 | `			int iLine = ts->iLine;` |
|    ! 0 | 1138 | `			if( e < ts->zEnd && tok_is_ws(*e) ){` |
|    ! 0 | 1139 | `				if( tok_at_nl(e,ts->zEnd) ){ ts->iLine++; }` |
|    ! 0 | 1140 | `				e++;                 /* one trailing whitespace char joins the tag */` |
|    ! 0 | 1141 | `			}` |
|    ! 0 | 1142 | `			tok_tok(ts,T_OPEN_TAG,(const char *)z,(int)(e-z),iLine);` |
|    ! 0 | 1143 | `			ts->z = e;` |
|    ! 0 | 1144 | `			return 1;` |
|      - | 1145 | `		}` |
|    ! 0 | 1146 | `	}` |
|    ! 0 | 1147 | `	return 0;` |
|    ! 0 | 1148 | `}` |
|      - | 1149 |  |
|      - | 1150 | `/* The scanning driver: alternate inline-HTML and PHP modes over the source. */` |
|    ! 0 | 1151 | `static void tok_run(tok_state *ts){` |
|    ! 0 | 1152 | `	while( ts->z < ts->zEnd && !ts->bOOM && !ts->bStop ){` |
|      - | 1153 | `		/* Inline HTML until the next open tag. */` |
|    ! 0 | 1154 | `		const unsigned char *zHtml = ts->z;` |
|    ! 0 | 1155 | `		int iHtmlLine = ts->iLine;` |
|    ! 0 | 1156 | `		while( ts->z < ts->zEnd ){` |
|    ! 0 | 1157 | `			if( ts->z[0]=='<' && ts->z+1 < ts->zEnd && ts->z[1]=='?' ){` |
|      - | 1158 | `				/* Only <?php and <?= are recognised (short tags off). */` |
|    ! 0 | 1159 | `				const unsigned char *s = ts->z;` |
|    ! 0 | 1160 | `				int bTag = 0;` |
|    ! 0 | 1161 | `				if( s+2 < ts->zEnd && s[2]=='=' ){ bTag = 1; }` |
|    ! 0 | 1162 | `				else if( s+4 <= ts->zEnd && tok_lower(s[2])=='p' && tok_lower(s[3])=='h'` |
|    ! 0 | 1163 | `					&& s+5 <= ts->zEnd && tok_lower(s[4])=='p'` |
|    ! 0 | 1164 | `					&& (s+5 >= ts->zEnd \|\| tok_is_ws(s[5])) ){ bTag = 1; }` |
|    ! 0 | 1165 | `				if( bTag ){ break; }` |
|    ! 0 | 1166 | `			}` |
|    ! 0 | 1167 | `			if( tok_at_nl(ts->z,ts->zEnd) ){ ts->iLine++; }` |
|    ! 0 | 1168 | `			ts->z++;` |
|    ! 0 | 1169 | `		}` |
|    ! 0 | 1170 | `		if( ts->z > zHtml ){` |
|    ! 0 | 1171 | `			tok_tok(ts,T_INLINE_HTML,(const char *)zHtml,(int)(ts->z-zHtml),iHtmlLine);` |
|    ! 0 | 1172 | `		}` |
|    ! 0 | 1173 | `		if( ts->z >= ts->zEnd ){ break; }` |
|    ! 0 | 1174 | `		if( !tok_open_tag(ts) ){` |
|      - | 1175 | `			/* Not actually a tag (shouldn't happen given the check above). */` |
|    ! 0 | 1176 | `			if( tok_at_nl(ts->z,ts->zEnd) ){ ts->iLine++; }` |
|    ! 0 | 1177 | `			ts->z++;` |
|    ! 0 | 1178 | `			continue;` |
|      - | 1179 | `		}` |
|      - | 1180 | `		/* PHP mode until a close tag or EOF. */` |
|    ! 0 | 1181 | `		ts->bProp = 0;` |
|    ! 0 | 1182 | `		while( ts->z < ts->zEnd && !ts->bOOM ){` |
|    ! 0 | 1183 | `			int eff = tok_lex_one(ts);` |
|    ! 0 | 1184 | `			if( eff < 0 ){ break; }` |
|    ! 0 | 1185 | `		}` |
|    ! 0 | 1186 | `	}` |
|    ! 0 | 1187 | `}` |
|      - | 1188 |  |
|      - | 1189 | `/*` |
|      - | 1190 | ` * array token_get_all(string $source [, int $flags = 0 ])` |
|      - | 1191 | ` */` |
|    ! 0 | 1192 | `static int PH7_builtin_token_get_all(ph7_context *pCtx,int nArg,ph7_value **apArg){` |
|      - | 1193 | `	tok_state ts;` |
|      - | 1194 | `	const char *zSrc;` |
|    ! 0 | 1195 | `	int nSrc = 0;` |
|    ! 0 | 1196 | `	if( nArg < 1 ){` |
|    ! 0 | 1197 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 1198 | `			"token_get_all() expects at least 1 argument, %d given",nArg);` |
|      - | 1199 | `	}` |
|    ! 0 | 1200 | `	zSrc = ph7_value_to_string(apArg[0],&nSrc);` |
|    ! 0 | 1201 | `	SyZero(&ts,sizeof(ts));` |
|    ! 0 | 1202 | `	ts.pCtx  = pCtx;` |
|    ! 0 | 1203 | `	ts.pArray = ph7_context_new_array(pCtx);` |
|    ! 0 | 1204 | `	ts.pS    = ph7_context_new_scalar(pCtx);` |
|    ! 0 | 1205 | `	ts.pId   = ph7_context_new_scalar(pCtx);` |
|    ! 0 | 1206 | `	ts.pText = ph7_context_new_scalar(pCtx);` |
|    ! 0 | 1207 | `	ts.pLine = ph7_context_new_scalar(pCtx);` |
|    ! 0 | 1208 | `	if( ts.pArray==0 \|\| ts.pS==0 \|\| ts.pId==0 \|\| ts.pText==0 \|\| ts.pLine==0 ){` |
|    ! 0 | 1209 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1210 | `	}` |
|    ! 0 | 1211 | `	ts.z    = (const unsigned char *)zSrc;` |
|    ! 0 | 1212 | `	ts.zEnd = ts.z + (nSrc > 0 ? (sxu32)nSrc : 0);` |
|    ! 0 | 1213 | `	ts.iLine = 1;` |
|    ! 0 | 1214 | `	tok_run(&ts);` |
|    ! 0 | 1215 | `	if( ts.bOOM ){` |
|    ! 0 | 1216 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1217 | `	}` |
|    ! 0 | 1218 | `	ph7_result_value(pCtx,ts.pArray);` |
|    ! 0 | 1219 | `	return PH7_OK;` |
|    ! 0 | 1220 | `}` |
|      - | 1221 |  |
|      - | 1222 | `/*` |
|      - | 1223 | ` * string token_name(int $id)` |
|      - | 1224 | ` */` |
|    ! 0 | 1225 | `static int PH7_builtin_token_name(ph7_context *pCtx,int nArg,ph7_value **apArg){` |
|      - | 1226 | `	int iId;` |
|      - | 1227 | `	const char *zName;` |
|    ! 0 | 1228 | `	if( nArg < 1 ){` |
|    ! 0 | 1229 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 1230 | `			"token_name() expects exactly 1 argument, %d given",nArg);` |
|      - | 1231 | `	}` |
|    ! 0 | 1232 | `	iId = ph7_value_to_int(apArg[0]);` |
|    ! 0 | 1233 | `	zName = TokConstName(iId);` |
|    ! 0 | 1234 | `	if( zName == 0 ){` |
|    ! 0 | 1235 | `		ph7_result_string(pCtx,"UNKNOWN",(int)sizeof("UNKNOWN")-1);` |
|    ! 0 | 1236 | `	}else{` |
|    ! 0 | 1237 | `		ph7_result_string(pCtx,zName,-1/*SyStrlen*/);` |
|      - | 1238 | `	}` |
|    ! 0 | 1239 | `	return PH7_OK;` |
|    ! 0 | 1240 | `}` |
|      - | 1241 |  |
|      - | 1242 | `/*` |
|      - | 1243 | ` * The PhpToken class: a thin userland wrapper over token_get_all(), tracking` |
|      - | 1244 | ` * byte offset (->pos) and line for the plain single-char tokens.` |
|      - | 1245 | ` */` |
|      - | 1246 | `static const char zPhpTokenClass[] = {` |
|      - | 1247 | `	"final class PhpToken implements Stringable {"` |
|      - | 1248 | `	" public int $id;"` |
|      - | 1249 | `	" public string $text;"` |
|      - | 1250 | `	" public int $line;"` |
|      - | 1251 | `	" public int $pos;"` |
|      - | 1252 | `	" public function __construct(int $id, string $text, int $line = -1, int $pos = -1){"` |
|      - | 1253 | `	"  $this->id = $id; $this->text = $text; $this->line = $line; $this->pos = $pos;"` |
|      - | 1254 | `	" }"` |
|      - | 1255 | `	" public static function tokenize(string $code, int $flags = 0): array {"` |
|      - | 1256 | `	"  $tokens = token_get_all($code, $flags);"` |
|      - | 1257 | `	"  $result = array(); $pos = 0; $line = 1;"` |
|      - | 1258 | `	"  foreach( $tokens as $tok ){"` |
|      - | 1259 | `	"   if( is_array($tok) ){ $id = $tok[0]; $text = $tok[1]; $ln = $tok[2]; }"` |
|      - | 1260 | `	"   else { $id = ord($tok); $text = $tok; $ln = $line; }"` |
|      - | 1261 | `	"   $result[] = new static($id, $text, $ln, $pos);"` |
|      - | 1262 | `	"   $pos += strlen($text);"` |
|      - | 1263 | `	"   $line += substr_count($text, \"\\n\");"` |
|      - | 1264 | `	"  }"` |
|      - | 1265 | `	"  return $result;"` |
|      - | 1266 | `	" }"` |
|      - | 1267 | `	" public function is($kind): bool {"` |
|      - | 1268 | `	"  if( is_array($kind) ){"` |
|      - | 1269 | `	"   foreach( $kind as $k ){"` |
|      - | 1270 | `	"    if( is_string($k) ){ if( $this->text === $k ){ return true; } }"` |
|      - | 1271 | `	"    elseif( $this->id === $k ){ return true; }"` |
|      - | 1272 | `	"   }"` |
|      - | 1273 | `	"   return false;"` |
|      - | 1274 | `	"  }"` |
|      - | 1275 | `	"  if( is_string($kind) ){ return $this->text === $kind; }"` |
|      - | 1276 | `	"  return $this->id === $kind;"` |
|      - | 1277 | `	" }"` |
|      - | 1278 | `	" public function isIgnorable(): bool {"` |
|      - | 1279 | `	"  return $this->id === T_WHITESPACE \|\| $this->id === T_COMMENT"` |
|      - | 1280 | `	"   \|\| $this->id === T_DOC_COMMENT \|\| $this->id === T_OPEN_TAG;"` |
|      - | 1281 | `	" }"` |
|      - | 1282 | `	" public function getTokenName(): ?string {"` |
|      - | 1283 | `	"  if( $this->id < 256 ){ return chr($this->id); }"` |
|      - | 1284 | `	"  $name = token_name($this->id);"` |
|      - | 1285 | `	"  if( $name === 'UNKNOWN' ){ return null; }"` |
|      - | 1286 | `	"  return $name;"` |
|      - | 1287 | `	" }"` |
|      - | 1288 | `	" public function __toString(): string { return (string)$this->text; }"` |
|      - | 1289 | `	"}"` |
|      - | 1290 | `};` |
|      - | 1291 |  |
|   3875 | 1292 | `PH7_PRIVATE sxi32 PH7_VmInstallTokenizer(ph7_vm *pVm){` |
|   3875 | 1293 | `	ph7_create_function(&(*pVm),"token_get_all",PH7_builtin_token_get_all,0);` |
|   3875 | 1294 | `	ph7_create_function(&(*pVm),"token_name",PH7_builtin_token_name,0);` |
|   3875 | 1295 | `	return PH7_VmEvalBuiltinChunk(&(*pVm),zPhpTokenClass,sizeof(zPhpTokenClass)-1);` |
|      5 | 1296 | `}` |
|      - | 1297 |  |
|      - | 1298 | `#else /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 1299 |  |
|      - | 1300 | `/* Tiny build: no tokenizer builtins/class (the whole builtin layer is off). */` |
|      - | 1301 | `PH7_PRIVATE sxi32 PH7_VmInstallTokenizer(ph7_vm *pVm){ (void)pVm; return SXRET_OK; }` |
|      - | 1302 |  |
|      - | 1303 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 1304 |  |
