# src/ph7/vm_builtin_tokenizer.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1007/1185 lines (84.98%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    4 | ` *` |
|       - |    5 | ` * PHP userland tokenizer surface: token_get_all(), token_name(), the T_*` |
|       - |    6 | ` * constant family and the PhpToken class.` |
|       - |    7 | ` *` |
|       - |    8 | ` * This is a purpose-built, self-contained re-scan of the raw source bytes that` |
|       - |    9 | ` * emits EVERY lexeme in source order — including T_WHITESPACE, T_COMMENT,` |
|       - |   10 | ` * T_DOC_COMMENT, T_INLINE_HTML and the open/close tags — exactly as php's` |
|       - |   11 | ` * tokenizer extension does. PHL's compile-time lexer (lex.c) drops whitespace` |
|       - |   12 | ` * and routes comments to a trivia sidecar, so it cannot be reused for this.` |
|       - |   13 | ` *` |
|       - |   14 | ` * The T_* id VALUES are pinned to php 8.5's actual tokenizer constant ints so` |
|       - |   15 | ` * that token_name() round-trips and consumers such as theseer/tokenizer (their` |
|       - |   16 | ` * string->name MAP) work unchanged.` |
|       - |   17 | ` */` |
|       - |   18 | `#include "ph7int.h"` |
|       - |   19 |  |
|       - |   20 | `/*` |
|       - |   21 | ` * php 8.5 tokenizer constant ids. Single source of truth for both the` |
|       - |   22 | ` * registered T_* constants and token_name()'s reverse lookup.` |
|       - |   23 | ` */` |
|       - |   24 | `#define T_LNUMBER                                 260` |
|       - |   25 | `#define T_DNUMBER                                 261` |
|       - |   26 | `#define T_STRING                                  262` |
|       - |   27 | `#define T_NAME_FULLY_QUALIFIED                    263` |
|       - |   28 | `#define T_NAME_RELATIVE                           264` |
|       - |   29 | `#define T_NAME_QUALIFIED                          265` |
|       - |   30 | `#define T_VARIABLE                                266` |
|       - |   31 | `#define T_INLINE_HTML                             267` |
|       - |   32 | `#define T_ENCAPSED_AND_WHITESPACE                 268` |
|       - |   33 | `#define T_CONSTANT_ENCAPSED_STRING                269` |
|       - |   34 | `#define T_STRING_VARNAME                          270` |
|       - |   35 | `#define T_NUM_STRING                              271` |
|       - |   36 | `#define T_INCLUDE                                 272` |
|       - |   37 | `#define T_INCLUDE_ONCE                            273` |
|       - |   38 | `#define T_EVAL                                    274` |
|       - |   39 | `#define T_REQUIRE                                 275` |
|       - |   40 | `#define T_REQUIRE_ONCE                            276` |
|       - |   41 | `#define T_LOGICAL_OR                              277` |
|       - |   42 | `#define T_LOGICAL_XOR                             278` |
|       - |   43 | `#define T_LOGICAL_AND                             279` |
|       - |   44 | `#define T_PRINT                                   280` |
|       - |   45 | `#define T_YIELD                                   281` |
|       - |   46 | `#define T_YIELD_FROM                              282` |
|       - |   47 | `#define T_INSTANCEOF                              283` |
|       - |   48 | `#define T_NEW                                     284` |
|       - |   49 | `#define T_CLONE                                   285` |
|       - |   50 | `#define T_EXIT                                    286` |
|       - |   51 | `#define T_IF                                      287` |
|       - |   52 | `#define T_ELSEIF                                  288` |
|       - |   53 | `#define T_ELSE                                    289` |
|       - |   54 | `#define T_ENDIF                                   290` |
|       - |   55 | `#define T_ECHO                                    291` |
|       - |   56 | `#define T_DO                                      292` |
|       - |   57 | `#define T_WHILE                                   293` |
|       - |   58 | `#define T_ENDWHILE                                294` |
|       - |   59 | `#define T_FOR                                     295` |
|       - |   60 | `#define T_ENDFOR                                  296` |
|       - |   61 | `#define T_FOREACH                                 297` |
|       - |   62 | `#define T_ENDFOREACH                              298` |
|       - |   63 | `#define T_DECLARE                                 299` |
|       - |   64 | `#define T_ENDDECLARE                              300` |
|       - |   65 | `#define T_AS                                      301` |
|       - |   66 | `#define T_SWITCH                                  302` |
|       - |   67 | `#define T_ENDSWITCH                               303` |
|       - |   68 | `#define T_CASE                                    304` |
|       - |   69 | `#define T_DEFAULT                                 305` |
|       - |   70 | `#define T_MATCH                                   306` |
|       - |   71 | `#define T_BREAK                                   307` |
|       - |   72 | `#define T_CONTINUE                                308` |
|       - |   73 | `#define T_GOTO                                    309` |
|       - |   74 | `#define T_FUNCTION                                310` |
|       - |   75 | `#define T_FN                                      311` |
|       - |   76 | `#define T_CONST                                   312` |
|       - |   77 | `#define T_RETURN                                  313` |
|       - |   78 | `#define T_TRY                                     314` |
|       - |   79 | `#define T_CATCH                                   315` |
|       - |   80 | `#define T_FINALLY                                 316` |
|       - |   81 | `#define T_THROW                                   317` |
|       - |   82 | `#define T_USE                                     318` |
|       - |   83 | `#define T_INSTEADOF                               319` |
|       - |   84 | `#define T_GLOBAL                                  320` |
|       - |   85 | `#define T_STATIC                                  321` |
|       - |   86 | `#define T_ABSTRACT                                322` |
|       - |   87 | `#define T_FINAL                                   323` |
|       - |   88 | `#define T_PRIVATE                                 324` |
|       - |   89 | `#define T_PROTECTED                               325` |
|       - |   90 | `#define T_PUBLIC                                  326` |
|       - |   91 | `#define T_PRIVATE_SET                             327` |
|       - |   92 | `#define T_PROTECTED_SET                           328` |
|       - |   93 | `#define T_PUBLIC_SET                              329` |
|       - |   94 | `#define T_READONLY                                330` |
|       - |   95 | `#define T_VAR                                     331` |
|       - |   96 | `#define T_UNSET                                   332` |
|       - |   97 | `#define T_ISSET                                   333` |
|       - |   98 | `#define T_EMPTY                                   334` |
|       - |   99 | `#define T_HALT_COMPILER                           335` |
|       - |  100 | `#define T_CLASS                                   336` |
|       - |  101 | `#define T_TRAIT                                   337` |
|       - |  102 | `#define T_INTERFACE                               338` |
|       - |  103 | `#define T_ENUM                                    339` |
|       - |  104 | `#define T_EXTENDS                                 340` |
|       - |  105 | `#define T_IMPLEMENTS                              341` |
|       - |  106 | `#define T_NAMESPACE                               342` |
|       - |  107 | `#define T_LIST                                    343` |
|       - |  108 | `#define T_ARRAY                                   344` |
|       - |  109 | `#define T_CALLABLE                                345` |
|       - |  110 | `#define T_LINE                                    346` |
|       - |  111 | `#define T_FILE                                    347` |
|       - |  112 | `#define T_DIR                                     348` |
|       - |  113 | `#define T_CLASS_C                                 349` |
|       - |  114 | `#define T_TRAIT_C                                 350` |
|       - |  115 | `#define T_METHOD_C                                351` |
|       - |  116 | `#define T_FUNC_C                                  352` |
|       - |  117 | `#define T_PROPERTY_C                              353` |
|       - |  118 | `#define T_NS_C                                    354` |
|       - |  119 | `#define T_ATTRIBUTE                               355` |
|       - |  120 | `#define T_PLUS_EQUAL                              356` |
|       - |  121 | `#define T_MINUS_EQUAL                             357` |
|       - |  122 | `#define T_MUL_EQUAL                               358` |
|       - |  123 | `#define T_DIV_EQUAL                               359` |
|       - |  124 | `#define T_CONCAT_EQUAL                            360` |
|       - |  125 | `#define T_MOD_EQUAL                               361` |
|       - |  126 | `#define T_AND_EQUAL                               362` |
|       - |  127 | `#define T_OR_EQUAL                                363` |
|       - |  128 | `#define T_XOR_EQUAL                               364` |
|       - |  129 | `#define T_SL_EQUAL                                365` |
|       - |  130 | `#define T_SR_EQUAL                                366` |
|       - |  131 | `#define T_COALESCE_EQUAL                          367` |
|       - |  132 | `#define T_BOOLEAN_OR                              368` |
|       - |  133 | `#define T_BOOLEAN_AND                             369` |
|       - |  134 | `#define T_IS_EQUAL                                370` |
|       - |  135 | `#define T_IS_NOT_EQUAL                            371` |
|       - |  136 | `#define T_IS_IDENTICAL                            372` |
|       - |  137 | `#define T_IS_NOT_IDENTICAL                        373` |
|       - |  138 | `#define T_IS_SMALLER_OR_EQUAL                     374` |
|       - |  139 | `#define T_IS_GREATER_OR_EQUAL                     375` |
|       - |  140 | `#define T_SPACESHIP                               376` |
|       - |  141 | `#define T_SL                                      377` |
|       - |  142 | `#define T_SR                                      378` |
|       - |  143 | `#define T_INC                                     379` |
|       - |  144 | `#define T_DEC                                     380` |
|       - |  145 | `#define T_INT_CAST                                381` |
|       - |  146 | `#define T_DOUBLE_CAST                             382` |
|       - |  147 | `#define T_STRING_CAST                             383` |
|       - |  148 | `#define T_ARRAY_CAST                              384` |
|       - |  149 | `#define T_OBJECT_CAST                             385` |
|       - |  150 | `#define T_BOOL_CAST                               386` |
|       - |  151 | `#define T_UNSET_CAST                              387` |
|       - |  152 | `#define T_VOID_CAST                               388` |
|       - |  153 | `#define T_OBJECT_OPERATOR                         389` |
|       - |  154 | `#define T_NULLSAFE_OBJECT_OPERATOR                390` |
|       - |  155 | `#define T_DOUBLE_ARROW                            391` |
|       - |  156 | `#define T_COMMENT                                 392` |
|       - |  157 | `#define T_DOC_COMMENT                             393` |
|       - |  158 | `#define T_OPEN_TAG                                394` |
|       - |  159 | `#define T_OPEN_TAG_WITH_ECHO                      395` |
|       - |  160 | `#define T_CLOSE_TAG                               396` |
|       - |  161 | `#define T_WHITESPACE                              397` |
|       - |  162 | `#define T_START_HEREDOC                           398` |
|       - |  163 | `#define T_END_HEREDOC                             399` |
|       - |  164 | `#define T_DOLLAR_OPEN_CURLY_BRACES                400` |
|       - |  165 | `#define T_CURLY_OPEN                              401` |
|       - |  166 | `#define T_DOUBLE_COLON                            402` |
|       - |  167 | `#define T_PAAMAYIM_NEKUDOTAYIM                    402` |
|       - |  168 | `#define T_NS_SEPARATOR                            403` |
|       - |  169 | `#define T_ELLIPSIS                                404` |
|       - |  170 | `#define T_COALESCE                                405` |
|       - |  171 | `#define T_POW                                     406` |
|       - |  172 | `#define T_POW_EQUAL                               407` |
|       - |  173 | `#define T_PIPE                                    408` |
|       - |  174 | `#define T_AMPERSAND_FOLLOWED_BY_VAR_OR_VARARG     409` |
|       - |  175 | `#define T_AMPERSAND_NOT_FOLLOWED_BY_VAR_OR_VARARG 410` |
|       - |  176 | `#define T_BAD_CHARACTER                           411` |
|       - |  177 |  |
|       - |  178 | `/* Registered as a plain constant; here for token_name() completeness. */` |
|       - |  179 | `#define TOK_TOKEN_PARSE                             1` |
|       - |  180 |  |
|       - |  181 | `typedef struct tok_const tok_const;` |
|       - |  182 | `struct tok_const { const char *zName; int iId; };` |
|       - |  183 |  |
|       - |  184 | `/*` |
|       - |  185 | ` * All tokenizer constants. token_name()'s reverse lookup uses this table too,` |
|       - |  186 | ` * so the CANONICAL name for a shared id must come first (T_DOUBLE_COLON before` |
|       - |  187 | ` * T_PAAMAYIM_NEKUDOTAYIM for id 402, matching php's token_name()).` |
|       - |  188 | ` */` |
|       - |  189 | `static const tok_const aTokConst[] = {` |
|       - |  190 | `	{ "T_LNUMBER", T_LNUMBER }, { "T_DNUMBER", T_DNUMBER }, { "T_STRING", T_STRING },` |
|       - |  191 | `	{ "T_NAME_FULLY_QUALIFIED", T_NAME_FULLY_QUALIFIED }, { "T_NAME_RELATIVE", T_NAME_RELATIVE },` |
|       - |  192 | `	{ "T_NAME_QUALIFIED", T_NAME_QUALIFIED }, { "T_VARIABLE", T_VARIABLE }, { "T_INLINE_HTML", T_INLINE_HTML },` |
|       - |  193 | `	{ "T_ENCAPSED_AND_WHITESPACE", T_ENCAPSED_AND_WHITESPACE },` |
|       - |  194 | `	{ "T_CONSTANT_ENCAPSED_STRING", T_CONSTANT_ENCAPSED_STRING }, { "T_STRING_VARNAME", T_STRING_VARNAME },` |
|       - |  195 | `	{ "T_NUM_STRING", T_NUM_STRING }, { "T_INCLUDE", T_INCLUDE }, { "T_INCLUDE_ONCE", T_INCLUDE_ONCE },` |
|       - |  196 | `	{ "T_EVAL", T_EVAL }, { "T_REQUIRE", T_REQUIRE }, { "T_REQUIRE_ONCE", T_REQUIRE_ONCE },` |
|       - |  197 | `	{ "T_LOGICAL_OR", T_LOGICAL_OR }, { "T_LOGICAL_XOR", T_LOGICAL_XOR }, { "T_LOGICAL_AND", T_LOGICAL_AND },` |
|       - |  198 | `	{ "T_PRINT", T_PRINT }, { "T_YIELD", T_YIELD }, { "T_YIELD_FROM", T_YIELD_FROM },` |
|       - |  199 | `	{ "T_INSTANCEOF", T_INSTANCEOF }, { "T_NEW", T_NEW }, { "T_CLONE", T_CLONE }, { "T_EXIT", T_EXIT },` |
|       - |  200 | `	{ "T_IF", T_IF }, { "T_ELSEIF", T_ELSEIF }, { "T_ELSE", T_ELSE }, { "T_ENDIF", T_ENDIF },` |
|       - |  201 | `	{ "T_ECHO", T_ECHO }, { "T_DO", T_DO }, { "T_WHILE", T_WHILE }, { "T_ENDWHILE", T_ENDWHILE },` |
|       - |  202 | `	{ "T_FOR", T_FOR }, { "T_ENDFOR", T_ENDFOR }, { "T_FOREACH", T_FOREACH }, { "T_ENDFOREACH", T_ENDFOREACH },` |
|       - |  203 | `	{ "T_DECLARE", T_DECLARE }, { "T_ENDDECLARE", T_ENDDECLARE }, { "T_AS", T_AS }, { "T_SWITCH", T_SWITCH },` |
|       - |  204 | `	{ "T_ENDSWITCH", T_ENDSWITCH }, { "T_CASE", T_CASE }, { "T_DEFAULT", T_DEFAULT }, { "T_MATCH", T_MATCH },` |
|       - |  205 | `	{ "T_BREAK", T_BREAK }, { "T_CONTINUE", T_CONTINUE }, { "T_GOTO", T_GOTO }, { "T_FUNCTION", T_FUNCTION },` |
|       - |  206 | `	{ "T_FN", T_FN }, { "T_CONST", T_CONST }, { "T_RETURN", T_RETURN }, { "T_TRY", T_TRY },` |
|       - |  207 | `	{ "T_CATCH", T_CATCH }, { "T_FINALLY", T_FINALLY }, { "T_THROW", T_THROW }, { "T_USE", T_USE },` |
|       - |  208 | `	{ "T_INSTEADOF", T_INSTEADOF }, { "T_GLOBAL", T_GLOBAL }, { "T_STATIC", T_STATIC },` |
|       - |  209 | `	{ "T_ABSTRACT", T_ABSTRACT }, { "T_FINAL", T_FINAL }, { "T_PRIVATE", T_PRIVATE },` |
|       - |  210 | `	{ "T_PROTECTED", T_PROTECTED }, { "T_PUBLIC", T_PUBLIC }, { "T_PRIVATE_SET", T_PRIVATE_SET },` |
|       - |  211 | `	{ "T_PROTECTED_SET", T_PROTECTED_SET }, { "T_PUBLIC_SET", T_PUBLIC_SET }, { "T_READONLY", T_READONLY },` |
|       - |  212 | `	{ "T_VAR", T_VAR }, { "T_UNSET", T_UNSET }, { "T_ISSET", T_ISSET }, { "T_EMPTY", T_EMPTY },` |
|       - |  213 | `	{ "T_HALT_COMPILER", T_HALT_COMPILER }, { "T_CLASS", T_CLASS }, { "T_TRAIT", T_TRAIT },` |
|       - |  214 | `	{ "T_INTERFACE", T_INTERFACE }, { "T_ENUM", T_ENUM }, { "T_EXTENDS", T_EXTENDS },` |
|       - |  215 | `	{ "T_IMPLEMENTS", T_IMPLEMENTS }, { "T_NAMESPACE", T_NAMESPACE }, { "T_LIST", T_LIST },` |
|       - |  216 | `	{ "T_ARRAY", T_ARRAY }, { "T_CALLABLE", T_CALLABLE }, { "T_LINE", T_LINE }, { "T_FILE", T_FILE },` |
|       - |  217 | `	{ "T_DIR", T_DIR }, { "T_CLASS_C", T_CLASS_C }, { "T_TRAIT_C", T_TRAIT_C }, { "T_METHOD_C", T_METHOD_C },` |
|       - |  218 | `	{ "T_FUNC_C", T_FUNC_C }, { "T_PROPERTY_C", T_PROPERTY_C }, { "T_NS_C", T_NS_C },` |
|       - |  219 | `	{ "T_ATTRIBUTE", T_ATTRIBUTE }, { "T_PLUS_EQUAL", T_PLUS_EQUAL }, { "T_MINUS_EQUAL", T_MINUS_EQUAL },` |
|       - |  220 | `	{ "T_MUL_EQUAL", T_MUL_EQUAL }, { "T_DIV_EQUAL", T_DIV_EQUAL }, { "T_CONCAT_EQUAL", T_CONCAT_EQUAL },` |
|       - |  221 | `	{ "T_MOD_EQUAL", T_MOD_EQUAL }, { "T_AND_EQUAL", T_AND_EQUAL }, { "T_OR_EQUAL", T_OR_EQUAL },` |
|       - |  222 | `	{ "T_XOR_EQUAL", T_XOR_EQUAL }, { "T_SL_EQUAL", T_SL_EQUAL }, { "T_SR_EQUAL", T_SR_EQUAL },` |
|       - |  223 | `	{ "T_COALESCE_EQUAL", T_COALESCE_EQUAL }, { "T_BOOLEAN_OR", T_BOOLEAN_OR },` |
|       - |  224 | `	{ "T_BOOLEAN_AND", T_BOOLEAN_AND }, { "T_IS_EQUAL", T_IS_EQUAL }, { "T_IS_NOT_EQUAL", T_IS_NOT_EQUAL },` |
|       - |  225 | `	{ "T_IS_IDENTICAL", T_IS_IDENTICAL }, { "T_IS_NOT_IDENTICAL", T_IS_NOT_IDENTICAL },` |
|       - |  226 | `	{ "T_IS_SMALLER_OR_EQUAL", T_IS_SMALLER_OR_EQUAL }, { "T_IS_GREATER_OR_EQUAL", T_IS_GREATER_OR_EQUAL },` |
|       - |  227 | `	{ "T_SPACESHIP", T_SPACESHIP }, { "T_SL", T_SL }, { "T_SR", T_SR }, { "T_INC", T_INC }, { "T_DEC", T_DEC },` |
|       - |  228 | `	{ "T_INT_CAST", T_INT_CAST }, { "T_DOUBLE_CAST", T_DOUBLE_CAST }, { "T_STRING_CAST", T_STRING_CAST },` |
|       - |  229 | `	{ "T_ARRAY_CAST", T_ARRAY_CAST }, { "T_OBJECT_CAST", T_OBJECT_CAST }, { "T_BOOL_CAST", T_BOOL_CAST },` |
|       - |  230 | `	{ "T_UNSET_CAST", T_UNSET_CAST }, { "T_VOID_CAST", T_VOID_CAST }, { "T_OBJECT_OPERATOR", T_OBJECT_OPERATOR },` |
|       - |  231 | `	{ "T_NULLSAFE_OBJECT_OPERATOR", T_NULLSAFE_OBJECT_OPERATOR }, { "T_DOUBLE_ARROW", T_DOUBLE_ARROW },` |
|       - |  232 | `	{ "T_COMMENT", T_COMMENT }, { "T_DOC_COMMENT", T_DOC_COMMENT }, { "T_OPEN_TAG", T_OPEN_TAG },` |
|       - |  233 | `	{ "T_OPEN_TAG_WITH_ECHO", T_OPEN_TAG_WITH_ECHO }, { "T_CLOSE_TAG", T_CLOSE_TAG },` |
|       - |  234 | `	{ "T_WHITESPACE", T_WHITESPACE }, { "T_START_HEREDOC", T_START_HEREDOC }, { "T_END_HEREDOC", T_END_HEREDOC },` |
|       - |  235 | `	{ "T_DOLLAR_OPEN_CURLY_BRACES", T_DOLLAR_OPEN_CURLY_BRACES }, { "T_CURLY_OPEN", T_CURLY_OPEN },` |
|       - |  236 | `	{ "T_DOUBLE_COLON", T_DOUBLE_COLON }, { "T_PAAMAYIM_NEKUDOTAYIM", T_PAAMAYIM_NEKUDOTAYIM },` |
|       - |  237 | `	{ "T_NS_SEPARATOR", T_NS_SEPARATOR }, { "T_ELLIPSIS", T_ELLIPSIS }, { "T_COALESCE", T_COALESCE },` |
|       - |  238 | `	{ "T_POW", T_POW }, { "T_POW_EQUAL", T_POW_EQUAL }, { "T_PIPE", T_PIPE },` |
|       - |  239 | `	{ "T_AMPERSAND_FOLLOWED_BY_VAR_OR_VARARG", T_AMPERSAND_FOLLOWED_BY_VAR_OR_VARARG },` |
|       - |  240 | `	{ "T_AMPERSAND_NOT_FOLLOWED_BY_VAR_OR_VARARG", T_AMPERSAND_NOT_FOLLOWED_BY_VAR_OR_VARARG },` |
|       - |  241 | `	{ "T_BAD_CHARACTER", T_BAD_CHARACTER },` |
|       - |  242 | `	{ "TOKEN_PARSE", TOK_TOKEN_PARSE },` |
|       - |  243 | `};` |
|       - |  244 |  |
|       - |  245 | `/*` |
|       - |  246 | ` * The expander shared by every registered T_* constant: pUserData carries the` |
|       - |  247 | ` * integer value directly (SX_INT_TO_PTR at registration time).` |
|       - |  248 | ` */` |
|   11096 |  249 | `static void TokConstExpand(ph7_value *pVal,void *pUserData)` |
|       3 |  250 | `{` |
|   11099 |  251 | `	ph7_value_int(pVal,SX_PTR_TO_INT(pUserData));` |
|   11099 |  252 | `}` |
|       - |  253 |  |
|    5619 |  254 | `PH7_PRIVATE void PH7_RegisterTokenizerConstants(ph7_vm *pVm)` |
|       5 |  255 | `{` |
|       - |  256 | `	sxu32 n;` |
|  870950 |  257 | `	for( n = 0 ; n < SX_ARRAYSIZE(aTokConst) ; ++n ){` |
| 1297301 |  258 | `		ph7_create_constant(&(*pVm),aTokConst[n].zName,TokConstExpand,` |
|  865326 |  259 | `			SX_INT_TO_PTR(aTokConst[n].iId));` |
|  431975 |  260 | `	}` |
|    5624 |  261 | `}` |
|       - |  262 |  |
|       - |  263 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|       - |  264 |  |
|       - |  265 | `/*` |
|       - |  266 | ` * Reverse lookup for token_name(): the canonical php name for an id, or 0 for` |
|       - |  267 | ` * unknown/out-of-range ids (token_name() then returns "UNKNOWN").` |
|       - |  268 | ` */` |
|     570 |  269 | `static const char * TokConstName(int iId)` |
|       2 |  270 | `{` |
|       - |  271 | `	sxu32 n;` |
|     572 |  272 | `	if( iId < 256 ){` |
|     ! 0 |  273 | `		return 0; /* single-char and TOKEN_PARSE ids are "UNKNOWN" in token_name() */` |
|       - |  274 | `	}` |
|   43080 |  275 | `	for( n = 0 ; n < SX_ARRAYSIZE(aTokConst) ; ++n ){` |
|   43080 |  276 | `		if( aTokConst[n].iId == iId ){` |
|     572 |  277 | `			return aTokConst[n].zName;` |
|       - |  278 | `		}` |
|   21256 |  279 | `	}` |
|     ! 0 |  280 | `	return 0;` |
|     287 |  281 | `}` |
|       - |  282 |  |
|       - |  283 | `/*` |
|       - |  284 | ` * Case-insensitive identifier -> keyword id. A lone identifier (no backslash)` |
|       - |  285 | ` * matching one of these becomes the mapped T_* token; anything else is` |
|       - |  286 | ` * T_STRING. 'enum' is deliberately absent — it is contextual (see the scanner).` |
|       - |  287 | ` */` |
|       - |  288 | `typedef struct tok_kw tok_kw;` |
|       - |  289 | `struct tok_kw { const char *zName; int iId; };` |
|       - |  290 | `static const tok_kw aKeyword[] = {` |
|       - |  291 | `	{ "include", T_INCLUDE }, { "include_once", T_INCLUDE_ONCE }, { "eval", T_EVAL },` |
|       - |  292 | `	{ "require", T_REQUIRE }, { "require_once", T_REQUIRE_ONCE }, { "or", T_LOGICAL_OR },` |
|       - |  293 | `	{ "xor", T_LOGICAL_XOR }, { "and", T_LOGICAL_AND }, { "print", T_PRINT }, { "yield", T_YIELD },` |
|       - |  294 | `	{ "instanceof", T_INSTANCEOF }, { "new", T_NEW }, { "clone", T_CLONE }, { "exit", T_EXIT },` |
|       - |  295 | `	{ "die", T_EXIT }, { "if", T_IF }, { "elseif", T_ELSEIF }, { "else", T_ELSE }, { "endif", T_ENDIF },` |
|       - |  296 | `	{ "echo", T_ECHO }, { "do", T_DO }, { "while", T_WHILE }, { "endwhile", T_ENDWHILE }, { "for", T_FOR },` |
|       - |  297 | `	{ "endfor", T_ENDFOR }, { "foreach", T_FOREACH }, { "endforeach", T_ENDFOREACH }, { "declare", T_DECLARE },` |
|       - |  298 | `	{ "enddeclare", T_ENDDECLARE }, { "as", T_AS }, { "switch", T_SWITCH }, { "endswitch", T_ENDSWITCH },` |
|       - |  299 | `	{ "case", T_CASE }, { "default", T_DEFAULT }, { "match", T_MATCH }, { "break", T_BREAK },` |
|       - |  300 | `	{ "continue", T_CONTINUE }, { "goto", T_GOTO }, { "function", T_FUNCTION }, { "fn", T_FN },` |
|       - |  301 | `	{ "const", T_CONST }, { "return", T_RETURN }, { "try", T_TRY }, { "catch", T_CATCH },` |
|       - |  302 | `	{ "finally", T_FINALLY }, { "throw", T_THROW }, { "use", T_USE }, { "insteadof", T_INSTEADOF },` |
|       - |  303 | `	{ "global", T_GLOBAL }, { "static", T_STATIC }, { "abstract", T_ABSTRACT }, { "final", T_FINAL },` |
|       - |  304 | `	{ "private", T_PRIVATE }, { "protected", T_PROTECTED }, { "public", T_PUBLIC }, { "readonly", T_READONLY },` |
|       - |  305 | `	{ "var", T_VAR }, { "unset", T_UNSET }, { "isset", T_ISSET }, { "empty", T_EMPTY },` |
|       - |  306 | `	{ "__halt_compiler", T_HALT_COMPILER }, { "class", T_CLASS }, { "trait", T_TRAIT },` |
|       - |  307 | `	{ "interface", T_INTERFACE }, { "extends", T_EXTENDS }, { "implements", T_IMPLEMENTS },` |
|       - |  308 | `	{ "namespace", T_NAMESPACE }, { "list", T_LIST }, { "array", T_ARRAY }, { "callable", T_CALLABLE },` |
|       - |  309 | `	{ "__line__", T_LINE }, { "__file__", T_FILE }, { "__dir__", T_DIR }, { "__class__", T_CLASS_C },` |
|       - |  310 | `	{ "__trait__", T_TRAIT_C }, { "__method__", T_METHOD_C }, { "__function__", T_FUNC_C },` |
|       - |  311 | `	{ "__namespace__", T_NS_C }, { "__property__", T_PROPERTY_C },` |
|       - |  312 | `};` |
|       - |  313 |  |
|       - |  314 | `/* Tokenizing state carried through the whole scan. */` |
|       - |  315 | `typedef struct tok_state tok_state;` |
|       - |  316 | `struct tok_state {` |
|       - |  317 | `	ph7_context *pCtx;` |
|       - |  318 | `	ph7_value   *pArray;   /* output array being built */` |
|       - |  319 | `	ph7_class   *pTokClass;/* PhpToken::tokenize(): build INSTANCES of this class instead of` |
|       - |  320 | `	                        * php's [id,text,line] tuples (php's own add_token branch) */` |
|       - |  321 | ``	int          iPos;     /* byte offset of the next token, php's `text - yy_start` */`` |
|       - |  322 | `	ph7_value   *pS;       /* reusable scalar for plain single-char string tokens */` |
|       - |  323 | `	ph7_value   *pId;      /* reusable scalar: tuple field [0] */` |
|       - |  324 | `	ph7_value   *pText;    /* reusable scalar: tuple field [1] */` |
|       - |  325 | `	ph7_value   *pLine;    /* reusable scalar: tuple field [2] */` |
|       - |  326 | `	const unsigned char *z;      /* cursor */` |
|       - |  327 | `	const unsigned char *zEnd;   /* one past end */` |
|       - |  328 | `	int          iLine;    /* current 1-based line at the cursor */` |
|       - |  329 | `	int          bProp;    /* one-shot: next identifier is a member name -> T_STRING */` |
|       - |  330 | `	int          bCaseName;/* one-shot: next identifier may be an enum case name (after 'case') */` |
|       - |  331 | `	int          bParse;   /* TOKEN_PARSE flag: apply php's semi-reserved re-tagging */` |
|       - |  332 | `	int          bStop;    /* __halt_compiler seen: stop scanning entirely */` |
|       - |  333 | `	int          bOOM;     /* memory failure flag */` |
|       - |  334 | `	/* php_strip_whitespace(): write the stripped source HERE instead of building an` |
|       - |  335 | `	 * array. The three flags are zend_strip's own state -- see tok_strip(). */` |
|       - |  336 | `	SyBlob      *pStrip;` |
|       - |  337 | `	int          bStripPrevSpace;` |
|       - |  338 | `	int          bStripPendingNl;` |
|       - |  339 | `	int          bStripAfterHd;` |
|       - |  340 | `};` |
|       - |  341 |  |
|       - |  342 | `/* --- character classes (byte-level, UTF-8 lead bytes count as label chars) --- */` |
|    1808 |  343 | `static int tok_is_label_start(int c){` |
|    1808 |  344 | `	return c=='_' \|\| (c>='a'&&c<='z') \|\| (c>='A'&&c<='Z') \|\| c>=0x80;` |
|       2 |  345 | `}` |
|    1056 |  346 | `static int tok_is_label(int c){` |
|    1056 |  347 | `	return tok_is_label_start(c) \|\| (c>='0'&&c<='9');` |
|       2 |  348 | `}` |
|    2090 |  349 | `static int tok_is_ws(int c){` |
|       - |  350 | `	/* php's tokenizer whitespace is exactly [ \t\r\n]; \v and \f are T_BAD_CHARACTER. */` |
|    2090 |  351 | `	return c==' '\|\|c=='\t'\|\|c=='\n'\|\|c=='\r';` |
|       2 |  352 | `}` |
|    7410 |  353 | `static int tok_lower(int c){` |
|    7410 |  354 | `	return (c>='A'&&c<='Z') ? c+32 : c;` |
|       2 |  355 | `}` |
|    6030 |  356 | `static int tok_ci_eq(const char *z,int n,const char *zKw){` |
|       - |  357 | `	int i;` |
|    6954 |  358 | `	for( i = 0 ; i < n ; ++i ){` |
|    6794 |  359 | `		if( zKw[i]==0 \|\| tok_lower((unsigned char)z[i]) != (unsigned char)zKw[i] ){` |
|    5870 |  360 | `			return 0;` |
|       - |  361 | `		}` |
|     464 |  362 | `	}` |
|     162 |  363 | `	return zKw[n]==0;` |
|    3016 |  364 | `}` |
|       - |  365 |  |
|       - |  366 | `/* php counts a line ending as any of "\n", "\r", or "\r\n" (each = one line). */` |
|     894 |  367 | `static int tok_at_nl(const unsigned char *p,const unsigned char *zEnd){` |
|     894 |  368 | `	if( *p=='\n' ){ return 1; }` |
|     756 |  369 | `	if( *p=='\r' && (p+1 >= zEnd \|\| p[1]!='\n') ){ return 1; }` |
|     742 |  370 | `	return 0;` |
|     448 |  371 | `}` |
|       - |  372 |  |
|       - |  373 | `/* Count line endings in [z,zEnd) and advance the running line counter. */` |
|      52 |  374 | `static void tok_bump_lines(tok_state *ts,const unsigned char *z,const unsigned char *zEnd){` |
|     332 |  375 | `	while( z < zEnd ){` |
|     282 |  376 | `		if( tok_at_nl(z,zEnd) ){ ts->iLine++; }` |
|     282 |  377 | `		z++;` |
|       2 |  378 | `	}` |
|      52 |  379 | `}` |
|       - |  380 |  |
|       - |  381 | `/* Emit a plain-string token (single-char / operator returned as a bare string). */` |
|       - |  382 | `/*` |
|       - |  383 | ` * One PhpToken (php's add_token with a token_class): the four slots are written` |
|       - |  384 | ` * DIRECTLY, without running the constructor -- which php can do because the` |
|       - |  385 | ` * constructor is final, and which is why a subclass keeps its own declared` |
|       - |  386 | ` * defaults (the instance starts from the class's property table). The line is` |
|       - |  387 | ` * the scanner's, and the position is the running byte offset, php's` |
|       - |  388 | `` * `text - yy_start`.`` |
|       - |  389 | ` */` |
|     146 |  390 | `static void tok_object(tok_state *ts,int iId,const char *z,int n,int iLine)` |
|       2 |  391 | `{` |
|     148 |  392 | `	ph7_vm *pVm = ts->pCtx->pVm;` |
|     148 |  393 | `	ph7_class_instance *pObj = PH7_NewClassInstance(pVm,ts->pTokClass);` |
|       - |  394 | `	ph7_value sVal;` |
|     148 |  395 | `	if( pObj == 0 ){` |
|     ! 0 |  396 | `		ts->bOOM = 1;` |
|     ! 0 |  397 | `		return;` |
|       - |  398 | `	}` |
|     148 |  399 | `	pObj->iRef++;` |
|     148 |  400 | `	PH7_NativeSetAttrInt(pVm,pObj,"id",iId);` |
|     148 |  401 | `	PH7_NativeSetAttrStr(pVm,pObj,"text",z,n);` |
|     148 |  402 | `	PH7_NativeSetAttrInt(pVm,pObj,"line",iLine);` |
|     148 |  403 | `	PH7_NativeSetAttrInt(pVm,pObj,"pos",ts->iPos);` |
|     148 |  404 | `	PH7_MemObjInit(pVm,&sVal);` |
|     148 |  405 | `	sVal.x.pOther = pObj;` |
|     148 |  406 | `	MemObjSetType(&sVal,MEMOBJ_OBJ);` |
|     148 |  407 | `	if( ph7_array_add_elem(ts->pArray,0,&sVal) != SXRET_OK ){   /* takes its OWN reference */` |
|     ! 0 |  408 | `		ts->bOOM = 1;` |
|     ! 0 |  409 | `	}` |
|     148 |  410 | `	PH7_MemObjRelease(&sVal);` |
|     148 |  411 | `	PH7_ClassInstanceUnref(pObj);   /* drop the creation reference (rule 16) */` |
|      75 |  412 | `}` |
|       - |  413 | `/*` |
|       - |  414 | `` * php_strip_whitespace()'s sink: php's `zend_strip`, over the same token stream`` |
|       - |  415 | ` * token_get_all() walks. It writes` |
|       - |  416 | ` *   - a COMMENT (line, block or doc) as NOTHING at all;` |
|       - |  417 | ` *   - a WHITESPACE run as ONE space, and only when the last thing written was not` |
|       - |  418 | ` *     already one;` |
|       - |  419 | ` *   - everything else verbatim;` |
|       - |  420 | ` *   - and a NEWLINE just after a heredoc terminator: in place of the WHITESPACE` |
|       - |  421 | ` *     that follows it, and otherwise after the token that does -- which is what` |
|       - |  422 | `` *     makes `EOT;` come out as "EOT;\n", `EOT\n;` as "EOT\n; " and `EOT :` as`` |
|       - |  423 | ` *     "EOT\n: ".` |
|       - |  424 | ` * Derived from php 8.5.9 over the 15,000 vendor files of the four ECOSYSTEM.md` |
|       - |  425 | ` * projects: every one of them matches byte for byte.` |
|       - |  426 | ` */` |
|     440 |  427 | `static void tok_strip(tok_state *ts,int iId,const char *z,int n)` |
|       1 |  428 | `{` |
|     441 |  429 | `	int bWs = iId == T_WHITESPACE;` |
|     441 |  430 | `	if( ts->bStripAfterHd ){` |
|      15 |  431 | `		ts->bStripAfterHd = 0;` |
|      15 |  432 | `		if( bWs ){` |
|       - |  433 | `			/* WHITESPACE right after the terminator: the newline goes there, and` |
|       - |  434 | `			 * the run itself is swallowed with it. */` |
|       5 |  435 | `			SyBlobAppend(ts->pStrip,"\n",1);` |
|       5 |  436 | `			ts->bStripPrevSpace = 1;` |
|       5 |  437 | `			return;` |
|       - |  438 | `		}` |
|      11 |  439 | `		ts->bStripPendingNl = 1;` |
|       5 |  440 | `	}` |
|     437 |  441 | `	if( iId == T_COMMENT \|\| iId == T_DOC_COMMENT ){` |
|      23 |  442 | `		return;` |
|       - |  443 | `	}` |
|     415 |  444 | `	if( bWs ){` |
|     133 |  445 | `		if( !ts->bStripPrevSpace ){` |
|     121 |  446 | `			SyBlobAppend(ts->pStrip," ",1);` |
|     121 |  447 | `			ts->bStripPrevSpace = 1;` |
|      60 |  448 | `		}` |
|     133 |  449 | `		return;` |
|       - |  450 | `	}` |
|     283 |  451 | `	SyBlobAppend(ts->pStrip,z,(sxu32)n);` |
|     283 |  452 | `	ts->bStripPrevSpace = 0;` |
|     283 |  453 | `	if( ts->bStripPendingNl ){` |
|      11 |  454 | `		SyBlobAppend(ts->pStrip,"\n",1);` |
|      11 |  455 | `		ts->bStripPrevSpace = 1;` |
|      11 |  456 | `		ts->bStripPendingNl = 0;` |
|      11 |  457 | `		return;` |
|       - |  458 | `	}` |
|     273 |  459 | `	if( iId == T_END_HEREDOC ){` |
|      15 |  460 | `		ts->bStripAfterHd = 1;` |
|       7 |  461 | `	}` |
|     221 |  462 | `}` |
|       - |  463 | `/* Every emitted lexeme advances the byte offset; the stream covers the source` |
|       - |  464 | ` * contiguously, which is what makes the running count equal php's pointer` |
|       - |  465 | ` * arithmetic (asserted by the text-roundtrip probe). */` |
|     450 |  466 | `static void tok_plain(tok_state *ts,const char *z,int n){` |
|     450 |  467 | `	if( ts->bOOM ){ return; }` |
|     450 |  468 | `	if( ts->pStrip ){` |
|       - |  469 | `		/* A single-character token is never whitespace or a comment. */` |
|     103 |  470 | `		tok_strip(ts,0,z,n);` |
|     103 |  471 | `		ts->iPos += n;` |
|     103 |  472 | `		return;` |
|       - |  473 | `	}` |
|     348 |  474 | `	if( ts->pTokClass ){` |
|       - |  475 | `		/* php's PhpToken id for a single-character token is the character ITSELF,` |
|       - |  476 | ``		 * and for the two-byte binary-string opener `b"` that character is the`` |
|       - |  477 | `		 * QUOTE -- the last byte, not the prefix. (token_get_all's array form has` |
|       - |  478 | `		 * no id here at all, so only this branch can tell.) */` |
|      42 |  479 | `		tok_object(ts,(unsigned char)z[n-1],z,n,ts->iLine);` |
|      42 |  480 | `		ts->iPos += n;` |
|      42 |  481 | `		return;` |
|       - |  482 | `	}` |
|     308 |  483 | `	ph7_value_string(ts->pS,z,n);` |
|     308 |  484 | `	if( ph7_array_add_elem(ts->pArray,0,ts->pS) != SXRET_OK ){ ts->bOOM = 1; }` |
|     308 |  485 | `	ph7_value_reset_string_cursor(ts->pS);` |
|     308 |  486 | `	ts->iPos += n;` |
|     226 |  487 | `}` |
|       - |  488 |  |
|       - |  489 | `/* Emit a [id, text, line] token. */` |
|     994 |  490 | `static void tok_tok(tok_state *ts,int iId,const char *z,int n,int iLine){` |
|       - |  491 | `	ph7_value *pInner;` |
|     994 |  492 | `	if( ts->bOOM ){ return; }` |
|     994 |  493 | `	if( ts->pStrip ){` |
|     339 |  494 | `		tok_strip(ts,iId,z,n);` |
|     339 |  495 | `		ts->iPos += n;` |
|     339 |  496 | `		return;` |
|       - |  497 | `	}` |
|     656 |  498 | `	if( ts->pTokClass ){` |
|     108 |  499 | `		tok_object(ts,iId,z,n,iLine);` |
|     108 |  500 | `		ts->iPos += n;` |
|     108 |  501 | `		return;` |
|       - |  502 | `	}` |
|     550 |  503 | `	ts->iPos += n;` |
|     550 |  504 | `	pInner = ph7_context_new_array(ts->pCtx);` |
|     550 |  505 | `	if( pInner == 0 ){ ts->bOOM = 1; return; }` |
|     550 |  506 | `	ph7_value_int(ts->pId,iId);` |
|     550 |  507 | `	ph7_value_string(ts->pText,z,n);` |
|     550 |  508 | `	ph7_value_int(ts->pLine,iLine);` |
|     550 |  509 | `	ph7_array_add_elem(pInner,0,ts->pId);` |
|     550 |  510 | `	ph7_array_add_elem(pInner,0,ts->pText);` |
|     550 |  511 | `	ph7_array_add_elem(pInner,0,ts->pLine);` |
|     550 |  512 | `	if( ph7_array_add_elem(ts->pArray,0,pInner) != SXRET_OK ){ ts->bOOM = 1; }` |
|     550 |  513 | `	ph7_context_release_value(ts->pCtx,pInner);` |
|     550 |  514 | `	ph7_value_reset_string_cursor(ts->pText);` |
|     498 |  515 | `}` |
|       - |  516 |  |
|       - |  517 | `/* Emit an encapsed-and-whitespace run [zStart, z) if non-empty, at iLine. */` |
|     104 |  518 | `static void tok_encaps(tok_state *ts,const unsigned char *zStart,const unsigned char *z,int iLine){` |
|     104 |  519 | `	if( z > zStart ){` |
|      30 |  520 | `		tok_tok(ts,T_ENCAPSED_AND_WHITESPACE,(const char *)zStart,(int)(z-zStart),iLine);` |
|      14 |  521 | `	}` |
|     104 |  522 | `}` |
|       - |  523 |  |
|       - |  524 | `/* Does the integer literal (digits between z and zEnd, given radix) overflow` |
|       - |  525 | ` * ZEND_LONG_MAX? Underscores are ignored. */` |
|      86 |  526 | `static int tok_int_overflows(const unsigned char *z,const unsigned char *zEnd,int radix){` |
|      86 |  527 | `	sxu64 val = 0;` |
|      86 |  528 | `	const sxu64 max = (sxu64)0x7FFFFFFFFFFFFFFF; /* PHP_INT_MAX */` |
|     190 |  529 | `	while( z < zEnd ){` |
|     106 |  530 | `		int c = *z++;` |
|       - |  531 | `		int d;` |
|     106 |  532 | `		if( c=='_' ){ continue; }` |
|     104 |  533 | `		if( c>='0'&&c<='9' ){ d = c-'0'; }` |
|      11 |  534 | `		else if( c>='a'&&c<='f' ){ d = c-'a'+10; }` |
|      11 |  535 | `		else if( c>='A'&&c<='F' ){ d = c-'A'+10; }` |
|     ! 0 |  536 | `		else { break; }` |
|     104 |  537 | `		if( d >= radix ){ break; }` |
|     104 |  538 | `		if( val > (max - (sxu64)d)/(sxu64)radix ){` |
|     ! 0 |  539 | `			return 1;` |
|       - |  540 | `		}` |
|     104 |  541 | `		val = val*(sxu64)radix + (sxu64)d;` |
|       2 |  542 | `	}` |
|      86 |  543 | `	return 0;` |
|      44 |  544 | `}` |
|       - |  545 |  |
|       - |  546 | `/* Forward decls for the mutually-recursive string / expression scanners. */` |
|       - |  547 | `static int  tok_lex_one(tok_state *ts);` |
|       - |  548 | `static void tok_scan_curly(tok_state *ts,int bVarname);` |
|       - |  549 |  |
|       - |  550 | `/*` |
|       - |  551 | ` * php's {WHITESPACE_OR_COMMENTS}: the run of whitespace and complete comments a` |
|       - |  552 | ` * contextual keyword is allowed to look across. Returns the cursor past it and` |
|       - |  553 | `` * sets *pbAny when anything at all was skipped -- `enum Foo` needs a separator,`` |
|       - |  554 | `` * and a comment counts as one (a block comment between `enum` and the name`` |
|       - |  555 | ` * still leaves an enum declaration).` |
|       - |  556 | ` */` |
|       8 |  557 | `static const unsigned char * tok_skip_ws_comments(tok_state *ts,const unsigned char *z,int *pbAny)` |
|       1 |  558 | `{` |
|       9 |  559 | `	int bAny = 0;` |
|       4 |  560 | `	for(;;){` |
|      17 |  561 | `		if( z < ts->zEnd && tok_is_ws(*z) ){` |
|      17 |  562 | `			while( z < ts->zEnd && tok_is_ws(*z) ){ z++; }` |
|       9 |  563 | `			bAny = 1;` |
|       9 |  564 | `			continue;` |
|       - |  565 | `		}` |
|       9 |  566 | `		if( z+1 < ts->zEnd && z[0]=='/' && z[1]=='*' ){` |
|     ! 0 |  567 | `			const unsigned char *p = z+2;` |
|     ! 0 |  568 | `			while( p+1 < ts->zEnd && !(p[0]=='*' && p[1]=='/') ){ p++; }` |
|     ! 0 |  569 | `			if( p+1 >= ts->zEnd ){ break; }   /* unterminated: not a separator */` |
|     ! 0 |  570 | `			z = p+2;` |
|     ! 0 |  571 | `			bAny = 1;` |
|     ! 0 |  572 | `			continue;` |
|       - |  573 | `		}` |
|       9 |  574 | `		if( z+1 < ts->zEnd && z[0]=='/' && z[1]=='/' ){` |
|     ! 0 |  575 | `			while( z < ts->zEnd && *z!='\n' && *z!='\r' ){ z++; }` |
|     ! 0 |  576 | `			bAny = 1;` |
|     ! 0 |  577 | `			continue;` |
|       - |  578 | `		}` |
|       9 |  579 | `		if( z < ts->zEnd && *z=='#' && !(z+1 < ts->zEnd && z[1]=='[') ){` |
|     ! 0 |  580 | `			while( z < ts->zEnd && *z!='\n' && *z!='\r' ){ z++; }` |
|     ! 0 |  581 | `			bAny = 1;` |
|     ! 0 |  582 | `			continue;` |
|       - |  583 | `		}` |
|       9 |  584 | `		break;` |
|     ! 0 |  585 | `	}` |
|       9 |  586 | `	if( pbAny ){ *pbAny = bAny; }` |
|       9 |  587 | `	return z;` |
|       1 |  588 | `}` |
|       - |  589 |  |
|       - |  590 | `/*` |
|       - |  591 | ` * After a T_HALT_COMPILER token: php emits the "();" that follows as normal` |
|       - |  592 | ` * tokens and then the entire remainder of the source as a single T_INLINE_HTML.` |
|       - |  593 | ` */` |
|       7 |  594 | `static void tok_halt_tail(tok_state *ts){` |
|       3 |  595 | `	for(;;){` |
|      21 |  596 | `		if( ts->z >= ts->zEnd ){ break; }` |
|      21 |  597 | `		if( tok_is_ws(*ts->z) ){` |
|       3 |  598 | `			const unsigned char *z0 = ts->z;` |
|       3 |  599 | `			int iLine = ts->iLine;` |
|       5 |  600 | `			while( ts->z < ts->zEnd && tok_is_ws(*ts->z) ){` |
|       3 |  601 | `				if( tok_at_nl(ts->z,ts->zEnd) ){ ts->iLine++; }` |
|       3 |  602 | `				ts->z++;` |
|       1 |  603 | `			}` |
|       3 |  604 | `			tok_tok(ts,T_WHITESPACE,(const char *)z0,(int)(ts->z-z0),iLine);` |
|       3 |  605 | `			continue;` |
|       - |  606 | `		}` |
|      19 |  607 | `		if( *ts->z=='(' \|\| *ts->z==')' ){` |
|      13 |  608 | `			char ch = (char)*ts->z;` |
|      13 |  609 | `			tok_plain(ts,&ch,1);` |
|      13 |  610 | `			ts->z++;` |
|      13 |  611 | `			continue;` |
|       - |  612 | `		}` |
|       7 |  613 | `		if( *ts->z==';' ){` |
|       3 |  614 | `			tok_plain(ts,";",1);` |
|       3 |  615 | `			ts->z++;` |
|       3 |  616 | `			break;` |
|       - |  617 | `		}` |
|       5 |  618 | `		if( *ts->z=='?' && ts->z+1 < ts->zEnd && ts->z[1]=='>' ){` |
|       - |  619 | `			/* php's other statement terminator. The close tag is a TOKEN there, and` |
|       - |  620 | `			 * the remainder starts after it (and after the one newline the tag` |
|       - |  621 | ``			 * swallows); this swallowed the `?>` into the T_INLINE_HTML, so a`` |
|       - |  622 | ``			 * `__halt_compiler()` with no semicolon -- the shape every phar stub and`` |
|       - |  623 | `			 * every data-appended script uses -- lost its terminator. */` |
|       5 |  624 | `			const unsigned char *z0 = ts->z;` |
|       5 |  625 | `			int iLine = ts->iLine;` |
|       5 |  626 | `			ts->z += 2;` |
|       5 |  627 | `			if( ts->z < ts->zEnd && *ts->z=='\r' && ts->z+1 < ts->zEnd && ts->z[1]=='\n' ){ ts->z += 2; }` |
|       5 |  628 | `			else if( ts->z < ts->zEnd && (*ts->z=='\n' \|\| *ts->z=='\r') ){ ts->z++; }` |
|       - |  629 | `			/* The line does NOT advance for the newline this tag swallows, and that` |
|       - |  630 | `			 * is php's own asymmetry rather than an oversight: the ordinary close tag` |
|       - |  631 | `			 * hands the rest of the file to the INITIAL state, which counts it, while` |
|       - |  632 | `			 * the halt-compiler remainder is emitted here and keeps the tag's line. */` |
|       5 |  633 | `			tok_tok(ts,T_CLOSE_TAG,(const char *)z0,(int)(ts->z-z0),iLine);` |
|       5 |  634 | `			break;` |
|       - |  635 | `		}` |
|     ! 0 |  636 | `		break;` |
|     ! 0 |  637 | `	}` |
|       7 |  638 | `	if( ts->z < ts->zEnd ){` |
|       7 |  639 | `		int iLine = ts->iLine;` |
|       7 |  640 | `		tok_tok(ts,T_INLINE_HTML,(const char *)ts->z,(int)(ts->zEnd-ts->z),iLine);` |
|       7 |  641 | `		tok_bump_lines(ts,ts->z,ts->zEnd);` |
|       7 |  642 | `		ts->z = ts->zEnd;` |
|       3 |  643 | `	}` |
|       7 |  644 | `	ts->bStop = 1;` |
|       7 |  645 | `}` |
|       - |  646 |  |
|       - |  647 | `/* Which digits a run may hold. */` |
|       - |  648 | `#define TOK_DIG_DEC 10` |
|       - |  649 | `#define TOK_DIG_HEX 16` |
|       - |  650 | `#define TOK_DIG_BIN 2` |
|       - |  651 | `#define TOK_DIG_OCT 8` |
|     296 |  652 | `static int tok_is_digit_of(int c,int radix){` |
|     296 |  653 | `	if( c>='0' && c<='9' ){ return (c-'0') < radix; }` |
|     154 |  654 | `	if( radix == 16 ){ return (c>='a'&&c<='f') \|\| (c>='A'&&c<='F'); }` |
|     134 |  655 | `	return 0;` |
|     149 |  656 | `}` |
|       - |  657 | `/*` |
|       - |  658 | ` * One digit run in php's number grammar, whose separator rule is` |
|       - |  659 | `` * `{DIGIT}+(_{DIGIT}+)*` -- an underscore is legal only BETWEEN two digits of`` |
|       - |  660 | `` * the run's own class. Taking `_` as just another digit character (which is what`` |
|       - |  661 | `` * the loops here used to do) accepted `100_`, `1__1` and `1_.0`, all of which php`` |
|       - |  662 | ` * rejects: it stops the number at the first digit and lexes the rest as a LABEL,` |
|       - |  663 | `` * so `1__1` is the integer 1 followed by T_STRING `__1`. Advances *pz past the`` |
|       - |  664 | ` * run and answers how many digits it held.` |
|       - |  665 | ` */` |
|     122 |  666 | `static int tok_digit_run(tok_state *ts,const unsigned char **pz,int radix)` |
|       2 |  667 | `{` |
|     124 |  668 | `	const unsigned char *z = *pz;` |
|     124 |  669 | `	int n = 0;` |
|     252 |  670 | `	while( z < ts->zEnd && tok_is_digit_of(*z,radix) ){ z++; n++; }` |
|     124 |  671 | `	if( n == 0 ){ return 0; }` |
|      58 |  672 | `	for(;;){` |
|     128 |  673 | `		if( z+1 < ts->zEnd && *z=='_' && tok_is_digit_of(z[1],radix) ){` |
|      11 |  674 | `			z++;` |
|      27 |  675 | `			while( z < ts->zEnd && tok_is_digit_of(*z,radix) ){ z++; n++; }` |
|      11 |  676 | `			continue;` |
|       - |  677 | `		}` |
|     118 |  678 | `		break;` |
|     ! 0 |  679 | `	}` |
|     118 |  680 | `	*pz = z;` |
|     118 |  681 | `	return n;` |
|      63 |  682 | `}` |
|       - |  683 | `/*` |
|       - |  684 | `` * php's ST_VAR_OFFSET number: `{LNUM}\|{HNUM}\|{BNUM}\|{ONUM}` -- the SAME four`` |
|       - |  685 | `` * spellings a literal has, `_` separators included, not the plain digit run this`` |
|       - |  686 | `` * used to take. `"$a[0x0]"` is one T_NUM_STRING covering `0x0` in php (the VALUE`` |
|       - |  687 | ` * is still the string key "0x0"; the offset grammar decides the TOKEN, not the` |
|       - |  688 | `` * meaning), and splitting it left `x0]` as encapsed text and a parse error where`` |
|       - |  689 | ` * php has none. Advances *pz past the run and returns 1 on a match.` |
|       - |  690 | ` */` |
|      36 |  691 | `static int tok_offset_num(tok_state *ts,const unsigned char **pz)` |
|       1 |  692 | `{` |
|      37 |  693 | `	const unsigned char *z = *pz;` |
|      37 |  694 | `	if( z >= ts->zEnd ){ return 0; }` |
|      37 |  695 | `	if( *z=='0' && z+1 < ts->zEnd ){` |
|      15 |  696 | `		int c1 = z[1];` |
|      15 |  697 | `		const unsigned char *d = z+2;` |
|      17 |  698 | `		if( (c1=='x'\|\|c1=='X') && tok_digit_run(ts,&d,TOK_DIG_HEX) ){ *pz = d; return 1; }` |
|      13 |  699 | `		d = z+2;` |
|      13 |  700 | `		if( (c1=='b'\|\|c1=='B') && tok_digit_run(ts,&d,TOK_DIG_BIN) ){ *pz = d; return 1; }` |
|      11 |  701 | `		d = z+2;` |
|      11 |  702 | `		if( (c1=='o'\|\|c1=='O') && tok_digit_run(ts,&d,TOK_DIG_OCT) ){ *pz = d; return 1; }` |
|       4 |  703 | `	}` |
|      31 |  704 | `	if( *z>='0' && *z<='9' ){` |
|      17 |  705 | `		tok_digit_run(ts,&z,TOK_DIG_DEC);` |
|      17 |  706 | `		*pz = z;` |
|      17 |  707 | `		return 1;` |
|       - |  708 | `	}` |
|      15 |  709 | `	return 0;` |
|      19 |  710 | `}` |
|       - |  711 | `/*` |
|       - |  712 | ` * Handle a possible interpolation construct at the cursor inside a double-quoted` |
|       - |  713 | ` * string or heredoc body. Returns 1 if it consumed one (emitting tokens), else 0` |
|       - |  714 | ` * (the caller keeps the char as literal). *pFlush is the start of the pending` |
|       - |  715 | ` * literal run and its line; on a hit we flush it first.` |
|       - |  716 | ` */` |
|      41 |  717 | `static int tok_try_interp(tok_state *ts,const unsigned char **pLitStart,int *pLitLine){` |
|      41 |  718 | `	const unsigned char *z = ts->z;` |
|      41 |  719 | `	if( *z=='$' && z+1 < ts->zEnd && tok_is_label_start(z[1]) ){` |
|      39 |  720 | `		const unsigned char *zVar = z+1;` |
|      39 |  721 | `		int iLine = ts->iLine;` |
|      77 |  722 | `		while( zVar < ts->zEnd && tok_is_label(*zVar) ){ zVar++; }` |
|      39 |  723 | `		tok_encaps(ts,*pLitStart,z,*pLitLine);` |
|      39 |  724 | `		tok_tok(ts,T_VARIABLE,(const char *)z,(int)(zVar-z),iLine);` |
|      39 |  725 | `		ts->z = zVar;` |
|       - |  726 | `		/* one optional simple offset [...] or ->prop */` |
|      39 |  727 | `		if( ts->z < ts->zEnd && *ts->z=='[' ){` |
|       - |  728 | `			/* php's ST_VAR_OFFSET, which is a STATE and therefore a loop, not one` |
|       - |  729 | `			 * token: it keeps producing tokens until a ']' pops it (or until one of` |
|       - |  730 | `			 * six bytes gives the input back). This used to read at most a single` |
|       - |  731 | ``			 * offset token, so `"$a[0x]"` -- valid source whose key is the string`` |
|       - |  732 | ``			 * "0x" -- left `x]` as encapsed text and a parse error where php has a`` |
|       - |  733 | `			 * T_NUM_STRING and a T_STRING. */` |
|      29 |  734 | `			tok_plain(ts,"[",1);` |
|      29 |  735 | `			ts->z++;` |
|      38 |  736 | `			for(;;){` |
|       - |  737 | `				int co;` |
|      67 |  738 | `				if( ts->z >= ts->zEnd ){ break; }` |
|      67 |  739 | `				co = *ts->z;` |
|      67 |  740 | `				if( co==']' ){` |
|      25 |  741 | `					tok_plain(ts,"]",1);` |
|      25 |  742 | `					ts->z++;` |
|      25 |  743 | `					break;` |
|       - |  744 | `				}` |
|      43 |  745 | `				if( co==' '\|\|co=='\n'\|\|co=='\r'\|\|co=='\t'\|\|co=='\\'\|\|co=='\''\|\|co=='#' ){` |
|       - |  746 | `					/* php's one rule whose whole job is a better parse error: it gives` |
|       - |  747 | `					 * the input BACK (yyless(0)) and pops the state, so the offset` |
|       - |  748 | `					 * produces an EMPTY T_ENCAPSED_AND_WHITESPACE and the string` |
|       - |  749 | ``					 * scanner then reads the rest -- ` 0]` and all -- as one more`` |
|       - |  750 | `					 * encapsed run. The empty token is the position a parser reports` |
|       - |  751 | `					 * the error at, so it is part of the contract. */` |
|       5 |  752 | `					tok_tok(ts,T_ENCAPSED_AND_WHITESPACE,(const char *)ts->z,0,ts->iLine);` |
|       5 |  753 | `					break;` |
|       - |  754 | `				}` |
|      39 |  755 | `				if( co=='$' && ts->z+1 < ts->zEnd && tok_is_label_start(ts->z[1]) ){` |
|       3 |  756 | `					const unsigned char *v = ts->z+1;` |
|       5 |  757 | `					while( v < ts->zEnd && tok_is_label(*v) ){ v++; }` |
|       3 |  758 | `					tok_tok(ts,T_VARIABLE,(const char *)ts->z,(int)(v-ts->z),ts->iLine);` |
|       3 |  759 | `					ts->z = v;` |
|       3 |  760 | `					continue;` |
|       - |  761 | `				}` |
|       - |  762 | `				{` |
|      37 |  763 | `					const unsigned char *n0 = ts->z;` |
|      37 |  764 | `					if( tok_offset_num(ts,&ts->z) ){` |
|      23 |  765 | `						tok_tok(ts,T_NUM_STRING,(const char *)n0,(int)(ts->z-n0),ts->iLine);` |
|      23 |  766 | `						continue;` |
|       - |  767 | `					}` |
|       - |  768 | `				}` |
|      15 |  769 | `				if( tok_is_label_start(co) ){` |
|       5 |  770 | `					const unsigned char *l = ts->z;` |
|       9 |  771 | `					while( l < ts->zEnd && tok_is_label(*l) ){ l++; }` |
|       5 |  772 | `					tok_tok(ts,T_STRING,(const char *)ts->z,(int)(l-ts->z),ts->iLine);` |
|       5 |  773 | `					ts->z = l;` |
|       5 |  774 | `					continue;` |
|       - |  775 | `				}` |
|      11 |  776 | `				if( co < 0x20 \|\| co == 0x7f ){` |
|     ! 0 |  777 | `					char chBad = (char)co;` |
|     ! 0 |  778 | `					tok_tok(ts,T_BAD_CHARACTER,&chBad,1,ts->iLine);` |
|     ! 0 |  779 | `					ts->z++;` |
|     ! 0 |  780 | `					continue;` |
|       - |  781 | `				}` |
|       - |  782 | ``				/* Everything else is php's generic one-character rule -- `-`, `+`,`` |
|       - |  783 | ``				 * `,`, `@`, a brace, a quote: "only '[' or '-' can be valid, but`` |
|       - |  784 | `				 * returning other tokens allows a more explicit parse error". */` |
|       - |  785 | `				{` |
|      11 |  786 | `					char chTok = (char)co;` |
|      11 |  787 | `					tok_plain(ts,&chTok,1);` |
|      11 |  788 | `					ts->z++;` |
|       - |  789 | `				}` |
|       1 |  790 | `			}` |
|      25 |  791 | `		}else if( ts->z+2 < ts->zEnd && ts->z[0]=='-' && ts->z[1]=='>' && tok_is_label_start(ts->z[2]) ){` |
|       - |  792 | `			const unsigned char *l;` |
|     ! 0 |  793 | `			tok_tok(ts,T_OBJECT_OPERATOR,"->",2,ts->iLine);` |
|     ! 0 |  794 | `			ts->z += 2;` |
|     ! 0 |  795 | `			l = ts->z;` |
|     ! 0 |  796 | `			while( l < ts->zEnd && tok_is_label(*l) ){ l++; }` |
|     ! 0 |  797 | `			tok_tok(ts,T_STRING,(const char *)ts->z,(int)(l-ts->z),ts->iLine);` |
|     ! 0 |  798 | `			ts->z = l;` |
|      10 |  799 | `		}else if( ts->z+3 < ts->zEnd && ts->z[0]=='?' && ts->z[1]=='-' && ts->z[2]=='>'` |
|       7 |  800 | `			&& tok_is_label_start(ts->z[3]) ){` |
|       - |  801 | `			/* php 8.0 gave the simple syntax the NULLSAFE arrow too, on the same` |
|       - |  802 | `			 * terms as '->': one property name and no further accessor. Without it` |
|       - |  803 | ``			 * `"$a?->b"` was the variable followed by four literal bytes. */`` |
|       - |  804 | `			const unsigned char *l;` |
|       7 |  805 | `			tok_tok(ts,T_NULLSAFE_OBJECT_OPERATOR,"?->",3,ts->iLine);` |
|       7 |  806 | `			ts->z += 3;` |
|       7 |  807 | `			l = ts->z;` |
|      13 |  808 | `			while( l < ts->zEnd && tok_is_label(*l) ){ l++; }` |
|       7 |  809 | `			tok_tok(ts,T_STRING,(const char *)ts->z,(int)(l-ts->z),ts->iLine);` |
|       7 |  810 | `			ts->z = l;` |
|       3 |  811 | `		}` |
|      39 |  812 | `		*pLitStart = ts->z;` |
|      39 |  813 | `		*pLitLine  = ts->iLine;` |
|      39 |  814 | `		return 1;` |
|       - |  815 | `	}` |
|       3 |  816 | `	if( *z=='{' && z+1 < ts->zEnd && z[1]=='$' ){` |
|       3 |  817 | `		tok_encaps(ts,*pLitStart,z,*pLitLine);` |
|       3 |  818 | `		tok_tok(ts,T_CURLY_OPEN,"{",1,ts->iLine);` |
|       3 |  819 | `		ts->z = z+1;` |
|       3 |  820 | `		tok_scan_curly(ts,0);` |
|       3 |  821 | `		*pLitStart = ts->z;` |
|       3 |  822 | `		*pLitLine  = ts->iLine;` |
|       3 |  823 | `		return 1;` |
|       - |  824 | `	}` |
|     ! 0 |  825 | `	if( *z=='$' && z+1 < ts->zEnd && z[1]=='{' ){` |
|     ! 0 |  826 | `		tok_encaps(ts,*pLitStart,z,*pLitLine);` |
|     ! 0 |  827 | `		tok_tok(ts,T_DOLLAR_OPEN_CURLY_BRACES,"${",2,ts->iLine);` |
|     ! 0 |  828 | `		ts->z = z+2;` |
|     ! 0 |  829 | `		tok_scan_curly(ts,1);` |
|     ! 0 |  830 | `		*pLitStart = ts->z;` |
|     ! 0 |  831 | `		*pLitLine  = ts->iLine;` |
|     ! 0 |  832 | `		return 1;` |
|       - |  833 | `	}` |
|     ! 0 |  834 | `	return 0;` |
|      21 |  835 | `}` |
|       - |  836 |  |
|       - |  837 | `/*` |
|       - |  838 | ` * Scan a brace-delimited expression inside a string ("{$...}" or "${...}").` |
|       - |  839 | ` * The opening brace token was already emitted and ts->z points just past it.` |
|       - |  840 | ` * Emits PHP tokens until the matching close brace, which it emits as a plain` |
|       - |  841 | ` * "}" string token. bVarname: the first label (if immediately followed by '}'` |
|       - |  842 | ` * or '[') is a T_STRING_VARNAME (the "${name}" form).` |
|       - |  843 | ` */` |
|       3 |  844 | `static void tok_scan_curly(tok_state *ts,int bVarname){` |
|       3 |  845 | `	int depth = 1;` |
|       3 |  846 | `	if( bVarname && ts->z < ts->zEnd && tok_is_label_start(*ts->z) ){` |
|     ! 0 |  847 | `		const unsigned char *l = ts->z;` |
|     ! 0 |  848 | `		while( l < ts->zEnd && tok_is_label(*l) ){ l++; }` |
|     ! 0 |  849 | `		if( l < ts->zEnd && (*l=='}' \|\| *l=='[') ){` |
|     ! 0 |  850 | `			tok_tok(ts,T_STRING_VARNAME,(const char *)ts->z,(int)(l-ts->z),ts->iLine);` |
|     ! 0 |  851 | `			ts->z = l;` |
|     ! 0 |  852 | `		}` |
|     ! 0 |  853 | `	}` |
|       9 |  854 | `	while( ts->z < ts->zEnd && depth > 0 && !ts->bOOM ){` |
|       - |  855 | `		int eff;` |
|       9 |  856 | `		if( *ts->z=='}' ){` |
|       3 |  857 | `			depth--;` |
|       3 |  858 | `			if( depth == 0 ){` |
|       3 |  859 | `				tok_plain(ts,"}",1);` |
|       3 |  860 | `				ts->z++;` |
|       3 |  861 | `				return;` |
|       - |  862 | `			}` |
|     ! 0 |  863 | `		}` |
|       7 |  864 | `		eff = tok_lex_one(ts);` |
|       7 |  865 | `		if( eff == '{' ){ depth++; }` |
|       7 |  866 | `		else if( eff == '}' ){ depth--; if( depth==0 ){ return; } }` |
|       7 |  867 | `		else if( eff < 0 ){ return; } /* close tag / EOF safety */` |
|       1 |  868 | `	}` |
|       2 |  869 | `}` |
|       - |  870 |  |
|       - |  871 | `/*` |
|       - |  872 | ` * Scan a double-quoted string or backtick string starting at the opening` |
|       - |  873 | ` * delimiter (ts->z points at it). If double-quoted with no interpolation the` |
|       - |  874 | ` * whole literal is one T_CONSTANT_ENCAPSED_STRING; otherwise it splits into the` |
|       - |  875 | ` * delimiter, encapsed runs and interpolation tokens.` |
|       - |  876 | ` */` |
|      45 |  877 | `static void tok_scan_dquote(tok_state *ts,int chDelim){` |
|      45 |  878 | `	const unsigned char *zOpen = ts->z;` |
|      45 |  879 | `	int iOpenLine = ts->iLine;` |
|       - |  880 | ``	/* A leading `b`/`B` is php's binary-string prefix (see tok_lex_one): it belongs`` |
|       - |  881 | `	 * to the token, so the token starts at zOpen while the DELIMITER scan starts` |
|       - |  882 | `	 * one byte later. nPfx is 1 exactly when the prefix is there. */` |
|      45 |  883 | `	int nPfx = (*zOpen != (unsigned char)chDelim) ? 1 : 0;` |
|       - |  884 | `	const unsigned char *zScan;` |
|      45 |  885 | `	int bInterp = 0;` |
|      45 |  886 | `	int bClosed = 0;` |
|      45 |  887 | `	ts->z += nPfx;` |
|      45 |  888 | `	zScan = ts->z+1;` |
|       - |  889 | `	/* Peek for interpolation to decide constant-vs-split (only for '"'). */` |
|      63 |  890 | `	while( zScan < ts->zEnd ){` |
|      63 |  891 | `		int c = *zScan;` |
|      63 |  892 | `		if( c=='\\' ){ zScan += 2; continue; }` |
|      63 |  893 | `		if( c==chDelim ){ bClosed = 1; break; }` |
|      57 |  894 | `		if( c=='$' && zScan+1 < ts->zEnd && (tok_is_label_start(zScan[1])\|\|zScan[1]=='{') ){ bInterp = 1; break; }` |
|      21 |  895 | `		if( c=='{' && zScan+1 < ts->zEnd && zScan[1]=='$' ){ bInterp = 1; break; }` |
|      19 |  896 | `		zScan++;` |
|       1 |  897 | `	}` |
|      45 |  898 | `	if( chDelim=='"' && !bInterp && bClosed ){` |
|       - |  899 | `		/* Whole constant string. Find the real closing quote. */` |
|       7 |  900 | `		const unsigned char *z = ts->z+1;` |
|      21 |  901 | `		while( z < ts->zEnd ){` |
|      21 |  902 | `			if( *z=='\\' && z+1 < ts->zEnd ){ z += 2; continue; }` |
|      21 |  903 | `			if( *z=='"' ){ break; }` |
|      15 |  904 | `			z++;` |
|       1 |  905 | `		}` |
|       7 |  906 | `		if( z < ts->zEnd ){ z++; } /* include closing quote */` |
|       7 |  907 | `		tok_tok(ts,T_CONSTANT_ENCAPSED_STRING,(const char *)zOpen,(int)(z-zOpen),iOpenLine);` |
|       7 |  908 | `		tok_bump_lines(ts,ts->z,z);` |
|       7 |  909 | `		ts->z = z;` |
|       7 |  910 | `		return;` |
|       - |  911 | `	}` |
|       - |  912 | `	/* Split form: opening delimiter, then body, then closing delimiter. */` |
|       - |  913 | `	{` |
|       - |  914 | `		char aOpen[2];` |
|      39 |  915 | `		char d = (char)chDelim;` |
|       - |  916 | `		const unsigned char *litStart;` |
|       - |  917 | `		int litLine;` |
|       - |  918 | ``		/* The opening token carries the prefix: php emits `b"` as one two-byte`` |
|       - |  919 | ``		 * single-character-token, not `b` and then `"`. */`` |
|      39 |  920 | `		aOpen[0] = nPfx ? (char)zOpen[0] : d;` |
|      39 |  921 | `		aOpen[1] = d;` |
|      39 |  922 | `		tok_plain(ts,nPfx ? aOpen : &d,nPfx ? 2 : 1);` |
|      39 |  923 | `		ts->z++;` |
|      39 |  924 | `		litStart = ts->z;` |
|      39 |  925 | `		litLine  = ts->iLine;` |
|      99 |  926 | `		while( ts->z < ts->zEnd && !ts->bOOM ){` |
|      99 |  927 | `			int c = *ts->z;` |
|      99 |  928 | `			if( c=='\\' && ts->z+1 < ts->zEnd ){` |
|     ! 0 |  929 | `				if( ts->z[1]=='\n' ){ ts->iLine++; } /* escaped newline still advances the line */` |
|     ! 0 |  930 | `				ts->z += 2;` |
|     ! 0 |  931 | `				continue;` |
|       - |  932 | `			}` |
|      99 |  933 | `			if( c==chDelim ){` |
|      39 |  934 | `				tok_encaps(ts,litStart,ts->z,litLine);` |
|      39 |  935 | `				tok_plain(ts,&d,1);` |
|      39 |  936 | `				ts->z++;` |
|      39 |  937 | `				return;` |
|       - |  938 | `			}` |
|      61 |  939 | `			if( c=='$' \|\| c=='{' ){` |
|      39 |  940 | `				if( tok_try_interp(ts,&litStart,&litLine) ){ continue; }` |
|     ! 0 |  941 | `			}` |
|      23 |  942 | `			if( tok_at_nl(ts->z,ts->zEnd) ){ ts->iLine++; }` |
|      23 |  943 | `			ts->z++;` |
|       1 |  944 | `		}` |
|       - |  945 | `		/* Unterminated: flush what we have. */` |
|     ! 0 |  946 | `		tok_encaps(ts,litStart,ts->z,litLine);` |
|       - |  947 | `	}` |
|      23 |  948 | `}` |
|       - |  949 |  |
|       - |  950 | `/* Does the line at z (already at line start) begin the heredoc closing marker` |
|       - |  951 | ` * for zLabel? Returns the end of the label (past it) if so, else 0. */` |
|      40 |  952 | `static const unsigned char * tok_heredoc_close(tok_state *ts,const unsigned char *z,` |
|       2 |  953 | `		const char *zLabel,int nLabel){` |
|      42 |  954 | `	const unsigned char *p = z;` |
|       - |  955 | `	int i;` |
|      70 |  956 | `	while( p < ts->zEnd && (*p==' '\|\|*p=='\t') ){ p++; }` |
|      42 |  957 | `	if( p + nLabel > ts->zEnd ){ return 0; } /* not enough bytes left for the label */` |
|      94 |  958 | `	for( i = 0 ; i < nLabel ; ++i ){` |
|      70 |  959 | `		if( p[i] != (unsigned char)zLabel[i] ){ return 0; }` |
|      28 |  960 | `	}` |
|      26 |  961 | `	p += nLabel;` |
|      26 |  962 | `	if( p < ts->zEnd && tok_is_label(*p) ){ return 0; } /* label is a prefix of a longer word */` |
|      26 |  963 | `	return p;` |
|      22 |  964 | `}` |
|       - |  965 |  |
|       - |  966 | `/*` |
|       - |  967 | ` * Scan a heredoc/nowdoc starting at "<<<". ts->z points at the first '<'.` |
|       - |  968 | ` * Returns 1 if a valid heredoc was consumed, 0 if this "<<<" is not a valid` |
|       - |  969 | ` * heredoc start (the caller then falls through to operator lexing: T_SL + '<').` |
|       - |  970 | ` * A valid start is: "<<<" [ws]* ["\|']? LABEL ["\|']? immediately followed by` |
|       - |  971 | ` * an optional '\r' and a '\n'.` |
|       - |  972 | ` */` |
|      26 |  973 | `static int tok_scan_heredoc(tok_state *ts){` |
|      26 |  974 | `	const unsigned char *zStart = ts->z;` |
|      26 |  975 | `	int iStartLine = ts->iLine;` |
|       - |  976 | ``	/* `b<<<LABEL`: the binary-string prefix is part of T_START_HEREDOC. */`` |
|      26 |  977 | `	int nPfx = (*zStart != '<') ? 1 : 0;` |
|      26 |  978 | `	const unsigned char *z = ts->z+3+nPfx;` |
|      26 |  979 | `	int bNowdoc = 0;` |
|      26 |  980 | `	int chQuote = 0;` |
|       - |  981 | `	const unsigned char *zLabel;` |
|       - |  982 | `	int nLabel;` |
|       - |  983 | `	const unsigned char *litStart;` |
|       - |  984 | `	int litLine;` |
|       - |  985 | `	/* optional spaces/tabs after <<< */` |
|      38 |  986 | `	while( z < ts->zEnd && (*z==' '\|\|*z=='\t') ){ z++; }` |
|      26 |  987 | `	if( z < ts->zEnd && (*z=='"' \|\| *z=='\'') ){` |
|      10 |  988 | `		chQuote = *z;` |
|      10 |  989 | `		bNowdoc = (*z=='\'');` |
|      10 |  990 | `		z++;` |
|       4 |  991 | `	}` |
|      26 |  992 | `	zLabel = z;` |
|      78 |  993 | `	while( z < ts->zEnd && tok_is_label(*z) ){ z++; }` |
|      26 |  994 | `	nLabel = (int)(z - zLabel);` |
|      26 |  995 | `	if( nLabel < 1 ){ return 0; } /* no label -> not a heredoc */` |
|       - |  996 | `	/* Every failure below returns 0 with ts->z still on the first byte, so a` |
|       - |  997 | ``	 * `b<<<` that turns out not to open a heredoc can fall back to lexing the`` |
|       - |  998 | `	 * prefix as an ordinary label. */` |
|      26 |  999 | `	if( chQuote ){` |
|      10 | 1000 | `		if( z < ts->zEnd && *z==chQuote ){ z++; }` |
|     ! 0 | 1001 | `		else { return 0; } /* unbalanced quote -> not a heredoc */` |
|       4 | 1002 | `	}` |
|       - | 1003 | `	/* The label must be immediately followed by an optional '\r' then '\n'. */` |
|      26 | 1004 | `	if( z < ts->zEnd && *z=='\r' && z+1 < ts->zEnd && z[1]=='\n' ){ z += 2; }` |
|      26 | 1005 | `	else if( z < ts->zEnd && *z=='\n' ){ z += 1; }` |
|     ! 0 | 1006 | `	else { return 0; } /* junk or EOF after the marker -> not a heredoc */` |
|       - | 1007 | `	/* Emit T_START_HEREDOC (delimiters + trailing newline). */` |
|      26 | 1008 | `	tok_tok(ts,T_START_HEREDOC,(const char *)zStart,(int)(z-zStart),iStartLine);` |
|      26 | 1009 | `	tok_bump_lines(ts,zStart,z);` |
|      26 | 1010 | `	ts->z = z;` |
|       - | 1011 | `	/* Scan body line by line until the closing marker. */` |
|      26 | 1012 | `	litStart = ts->z;` |
|      26 | 1013 | `	litLine  = ts->iLine;` |
|      54 | 1014 | `	while( ts->z < ts->zEnd && !ts->bOOM ){` |
|       - | 1015 | `		/* At a line start, test for the closing marker. */` |
|      42 | 1016 | `		const unsigned char *pClose = tok_heredoc_close(ts,ts->z,(const char *)zLabel,nLabel);` |
|      42 | 1017 | `		if( pClose ){` |
|       - | 1018 | `			int iCloseLine;` |
|      26 | 1019 | `			tok_encaps(ts,litStart,ts->z,litLine);` |
|      26 | 1020 | `			iCloseLine = ts->iLine;` |
|      26 | 1021 | `			tok_tok(ts,T_END_HEREDOC,(const char *)ts->z,(int)(pClose-ts->z),iCloseLine);` |
|      26 | 1022 | `			ts->z = pClose;` |
|      26 | 1023 | `			return 1;` |
|       - | 1024 | `		}` |
|       - | 1025 | `		/* Process one line's content. */` |
|      76 | 1026 | `		while( ts->z < ts->zEnd ){` |
|      76 | 1027 | `			int c = *ts->z;` |
|      76 | 1028 | `			if( c=='\n' ){ ts->iLine++; ts->z++; break; }` |
|      60 | 1029 | `			if( c=='\r' && (ts->z+1>=ts->zEnd \|\| ts->z[1]!='\n') ){ ts->iLine++; ts->z++; continue; }` |
|      60 | 1030 | `			if( !bNowdoc && (c=='\\') && ts->z+1 < ts->zEnd ){` |
|     ! 0 | 1031 | `				if( ts->z[1]=='\n' ){ ts->iLine++; } /* escaped newline still advances the line */` |
|     ! 0 | 1032 | `				ts->z += 2;` |
|     ! 0 | 1033 | `				continue;` |
|       - | 1034 | `			}` |
|      60 | 1035 | `			if( !bNowdoc && (c=='$' \|\| c=='{') ){` |
|       3 | 1036 | `				if( tok_try_interp(ts,&litStart,&litLine) ){ continue; }` |
|     ! 0 | 1037 | `			}` |
|      57 | 1038 | `			ts->z++;` |
|       1 | 1039 | `		}` |
|       2 | 1040 | `	}` |
|     ! 0 | 1041 | `	tok_encaps(ts,litStart,ts->z,litLine);` |
|     ! 0 | 1042 | `	return 1;` |
|      14 | 1043 | `}` |
|       - | 1044 |  |
|       - | 1045 | `/* One cast keyword between the parens, e.g. "int". Returns the cast T_* id or 0. */` |
|      13 | 1046 | `static int tok_cast_id(const char *z,int n){` |
|      13 | 1047 | `	if( tok_ci_eq(z,n,"int") \|\| tok_ci_eq(z,n,"integer") ) return T_INT_CAST;` |
|      13 | 1048 | `	if( tok_ci_eq(z,n,"float") \|\| tok_ci_eq(z,n,"double") \|\| tok_ci_eq(z,n,"real") ) return T_DOUBLE_CAST;` |
|      13 | 1049 | `	if( tok_ci_eq(z,n,"string") \|\| tok_ci_eq(z,n,"binary") ) return T_STRING_CAST;` |
|      13 | 1050 | `	if( tok_ci_eq(z,n,"array") ) return T_ARRAY_CAST;` |
|      13 | 1051 | `	if( tok_ci_eq(z,n,"object") ) return T_OBJECT_CAST;` |
|      13 | 1052 | `	if( tok_ci_eq(z,n,"bool") \|\| tok_ci_eq(z,n,"boolean") ) return T_BOOL_CAST;` |
|      13 | 1053 | `	if( tok_ci_eq(z,n,"unset") ) return T_UNSET_CAST;` |
|       - | 1054 | ``	/* php 8.5's `(void)`. It converts nothing and the grammar takes it only at the`` |
|       - | 1055 | `	 * head of an expression statement, but the SCANNER produces it wherever the` |
|       - | 1056 | ``	 * spelling appears -- so `$x = (void);` is T_VOID_CAST followed by ';'. */`` |
|      13 | 1057 | `	if( tok_ci_eq(z,n,"void") ) return T_VOID_CAST;` |
|       5 | 1058 | `	return 0;` |
|       7 | 1059 | `}` |
|       - | 1060 |  |
|       - | 1061 | `/*` |
|       - | 1062 | ` * Try to lex "( <ws>? castword <ws>? )" at ts->z (pointing at '('). On success` |
|       - | 1063 | ` * emits the cast token and returns 1; otherwise returns 0 and consumes nothing.` |
|       - | 1064 | ` */` |
|      22 | 1065 | `static int tok_try_cast(tok_state *ts){` |
|      22 | 1066 | `	const unsigned char *p = ts->z+1;` |
|       - | 1067 | `	const unsigned char *w0,*w1;` |
|       - | 1068 | `	int id;` |
|      36 | 1069 | `	while( p < ts->zEnd && (*p==' '\|\|*p=='\t') ){ p++; }` |
|      22 | 1070 | `	w0 = p;` |
|      70 | 1071 | `	while( p < ts->zEnd && tok_is_label(*p) ){ p++; }` |
|      22 | 1072 | `	w1 = p;` |
|      22 | 1073 | `	if( w1 == w0 ){ return 0; }` |
|      13 | 1074 | `	id = tok_cast_id((const char *)w0,(int)(w1-w0));` |
|      13 | 1075 | `	if( id == 0 ){ return 0; }` |
|      17 | 1076 | `	while( p < ts->zEnd && (*p==' '\|\|*p=='\t') ){ p++; }` |
|       9 | 1077 | `	if( p >= ts->zEnd \|\| *p != ')' ){ return 0; }` |
|       9 | 1078 | `	p++;` |
|       9 | 1079 | `	tok_tok(ts,id,(const char *)ts->z,(int)(p-ts->z),ts->iLine);` |
|       9 | 1080 | `	ts->z = p;` |
|       9 | 1081 | `	return 1;` |
|      12 | 1082 | `}` |
|       - | 1083 |  |
|       - | 1084 | `/*` |
|       - | 1085 | ` * Lex exactly one PHP-mode token and emit it. Returns:` |
|       - | 1086 | ` *   0   normal token` |
|       - | 1087 | ` *  '{' / '}'  when it emitted a bare '{' / '}' (for curly-expr brace tracking)` |
|       - | 1088 | ` *  -1  when it emitted a close tag or hit EOF (leave PHP mode)` |
|       - | 1089 | ` * Whitespace and comments preserve ts->bProp; every other token consumes it.` |
|       - | 1090 | ` */` |
|    1058 | 1091 | `static int tok_lex_one(tok_state *ts){` |
|    1058 | 1092 | `	const unsigned char *z = ts->z;` |
|       - | 1093 | `	int c;` |
|       - | 1094 | `	int bWasProp;` |
|       - | 1095 | `	int bWasCase;` |
|    1058 | 1096 | `	if( z >= ts->zEnd ){ return -1; }` |
|    1058 | 1097 | `	c = *z;` |
|       - | 1098 | `	/* Whitespace run (preserves property state). */` |
|    1058 | 1099 | `	if( tok_is_ws(c) ){` |
|     356 | 1100 | `		const unsigned char *z0 = z;` |
|     356 | 1101 | `		int iLine = ts->iLine;` |
|     726 | 1102 | `		while( ts->z < ts->zEnd && tok_is_ws(*ts->z) ){` |
|     372 | 1103 | `			if( tok_at_nl(ts->z,ts->zEnd) ){ ts->iLine++; }` |
|     372 | 1104 | `			ts->z++;` |
|       2 | 1105 | `		}` |
|     356 | 1106 | `		tok_tok(ts,T_WHITESPACE,(const char *)z0,(int)(ts->z-z0),iLine);` |
|     356 | 1107 | `		return 0;` |
|       - | 1108 | `	}` |
|       - | 1109 | `	/* Comments: hash/slash-slash to EOL-or-close-tag, and block comments. Preserve property state.` |
|       - | 1110 | `	 * A LINE ending is any of "\n", "\r" and "\r\n" -- a bare CR ends a line comment in php too,` |
|       - | 1111 | `	 * so a CR-only file (old Mac endings) is a stream of comments and code there and was ONE` |
|       - | 1112 | `	 * comment running to the end of the file here. */` |
|     704 | 1113 | `	if( c=='#' && !(z+1 < ts->zEnd && z[1]=='[') ){` |
|       3 | 1114 | `		const unsigned char *z0 = z;` |
|       3 | 1115 | `		int iLine = ts->iLine;` |
|       3 | 1116 | `		ts->z++;` |
|       7 | 1117 | `		while( ts->z < ts->zEnd && *ts->z!='\n' && *ts->z!='\r' ){` |
|       5 | 1118 | `			if( *ts->z=='?' && ts->z+1 < ts->zEnd && ts->z[1]=='>' ){ break; }` |
|       5 | 1119 | `			ts->z++;` |
|       1 | 1120 | `		}` |
|       3 | 1121 | `		tok_tok(ts,T_COMMENT,(const char *)z0,(int)(ts->z-z0),iLine);` |
|       3 | 1122 | `		return 0;` |
|       - | 1123 | `	}` |
|     702 | 1124 | `	if( c=='/' && z+1 < ts->zEnd && z[1]=='/' ){` |
|      11 | 1125 | `		const unsigned char *z0 = z;` |
|      11 | 1126 | `		int iLine = ts->iLine;` |
|      11 | 1127 | `		ts->z += 2;` |
|      47 | 1128 | `		while( ts->z < ts->zEnd && *ts->z!='\n' && *ts->z!='\r' ){` |
|      37 | 1129 | `			if( *ts->z=='?' && ts->z+1 < ts->zEnd && ts->z[1]=='>' ){ break; }` |
|      37 | 1130 | `			ts->z++;` |
|       1 | 1131 | `		}` |
|      11 | 1132 | `		tok_tok(ts,T_COMMENT,(const char *)z0,(int)(ts->z-z0),iLine);` |
|      11 | 1133 | `		return 0;` |
|       - | 1134 | `	}` |
|     692 | 1135 | `	if( c=='/' && z+1 < ts->zEnd && z[1]=='*' ){` |
|      13 | 1136 | `		const unsigned char *z0 = z;` |
|      13 | 1137 | `		int iLine = ts->iLine;` |
|       - | 1138 | `		/* A doc comment is slash-star-star followed by a whitespace char (php's` |
|       - | 1139 | `		 * rule): the third char must be '*' and the fourth must be whitespace. */` |
|      13 | 1140 | `		int bDoc = ( z+2 < ts->zEnd && z[2]=='*' && z+3 < ts->zEnd && tok_is_ws(z[3]) );` |
|      13 | 1141 | `		ts->z += 2;` |
|      37 | 1142 | `		while( ts->z < ts->zEnd ){` |
|      37 | 1143 | `			if( *ts->z=='*' && ts->z+1 < ts->zEnd && ts->z[1]=='/' ){ ts->z += 2; break; }` |
|      25 | 1144 | `			if( tok_at_nl(ts->z,ts->zEnd) ){ ts->iLine++; }` |
|      25 | 1145 | `			ts->z++;` |
|       1 | 1146 | `		}` |
|      13 | 1147 | `		tok_tok(ts,bDoc ? T_DOC_COMMENT : T_COMMENT,(const char *)z0,(int)(ts->z-z0),iLine);` |
|      13 | 1148 | `		return 0;` |
|       - | 1149 | `	}` |
|       - | 1150 | `	/* From here the token consumes property/case state. */` |
|     680 | 1151 | `	bWasProp = ts->bProp;` |
|     680 | 1152 | `	bWasCase = ts->bCaseName;` |
|     680 | 1153 | `	ts->bProp = 0;` |
|     680 | 1154 | `	ts->bCaseName = 0;` |
|       - | 1155 | `	/* Close tag. */` |
|     680 | 1156 | `	if( c=='?' && z+1 < ts->zEnd && z[1]=='>' ){` |
|      14 | 1157 | `		const unsigned char *z0 = z;` |
|      14 | 1158 | `		int iLine = ts->iLine;` |
|      14 | 1159 | `		ts->z += 2;` |
|       - | 1160 | `		/* php swallows one trailing NEWLINE into the close tag, and its newline is` |
|       - | 1161 | ``		 * ("\r\n"\|"\n"\|"\r") -- a lone CR counts. `?>\r\r\n` therefore ends the tag`` |
|       - | 1162 | `		 * after the FIRST CR (the pair "\r\r" is not "\r\n"), and the inline HTML that` |
|       - | 1163 | `		 * follows starts a line later. */` |
|      14 | 1164 | `		if( ts->z < ts->zEnd && *ts->z=='\r' && ts->z+1 < ts->zEnd && ts->z[1]=='\n' ){ ts->z += 2; ts->iLine++; }` |
|      12 | 1165 | `		else if( ts->z < ts->zEnd && (*ts->z=='\n' \|\| *ts->z=='\r') ){ ts->z++; ts->iLine++; }` |
|      14 | 1166 | `		tok_tok(ts,T_CLOSE_TAG,(const char *)z0,(int)(ts->z-z0),iLine);` |
|      14 | 1167 | `		return -1;` |
|       - | 1168 | `	}` |
|       - | 1169 | `	/* #[ attribute open. */` |
|     668 | 1170 | `	if( c=='#' && z+1 < ts->zEnd && z[1]=='[' ){` |
|     ! 0 | 1171 | `		tok_tok(ts,T_ATTRIBUTE,"#[",2,ts->iLine);` |
|     ! 0 | 1172 | `		ts->z += 2;` |
|     ! 0 | 1173 | `		return 0;` |
|       - | 1174 | `	}` |
|       - | 1175 | `	/* Variable. */` |
|     668 | 1176 | `	if( c=='$' && z+1 < ts->zEnd && tok_is_label_start(z[1]) ){` |
|      78 | 1177 | `		const unsigned char *v = z+1;` |
|     154 | 1178 | `		while( v < ts->zEnd && tok_is_label(*v) ){ v++; }` |
|      78 | 1179 | `		tok_tok(ts,T_VARIABLE,(const char *)z,(int)(v-z),ts->iLine);` |
|      78 | 1180 | `		ts->z = v;` |
|      78 | 1181 | `		return 0;` |
|       - | 1182 | `	}` |
|       - | 1183 | `` 	/* php's BINARY-STRING PREFIX. A single `b`/`B` welded to a quote or to `<<<` `` |
|       - | 1184 | `	 * is not an identifier: it is part of the string TOKEN, and php's scanner says` |
|       - | 1185 | ``	 * so -- `b'foo'` is one T_CONSTANT_ENCAPSED_STRING six bytes long, an`` |
|       - | 1186 | ``	 * interpolating `b"foo$x"` opens with a two-byte `b"` and `b<<<'S'` is a`` |
|       - | 1187 | ``	 * T_START_HEREDOC carrying the `b`. It marks nothing at runtime (php has one`` |
|       - | 1188 | `	 * string type), which is exactly why it survives in real source. The prefix is` |
|       - | 1189 | ``	 * ADJACENT only: `b <<<'S'` and `bb'foo'` are an identifier and a string, and a`` |
|       - | 1190 | ``	 * `b` in php's LOOKING_FOR_PROPERTY state (right after `->`/`?->`) is a member`` |
|       - | 1191 | `	 * NAME, never a prefix. */` |
|     590 | 1192 | `	if( (c=='b' \|\| c=='B') && !bWasProp && z+1 < ts->zEnd` |
|      34 | 1193 | `	 && ( z[1]=='\'' \|\| z[1]=='"'` |
|      16 | 1194 | `	   \|\| (z[1]=='<' && z+3 < ts->zEnd && z[2]=='<' && z[3]=='<') ) ){` |
|      23 | 1195 | `		if( z[1]=='\'' ){` |
|      11 | 1196 | `			const unsigned char *p = z+2;` |
|      11 | 1197 | `			int bClosed = 0;` |
|      27 | 1198 | `			while( p < ts->zEnd ){` |
|      27 | 1199 | `				if( *p=='\\' && p+1 < ts->zEnd ){ p += 2; continue; }` |
|      27 | 1200 | `				if( *p=='\'' ){ p++; bClosed = 1; break; }` |
|      17 | 1201 | `				p++;` |
|       1 | 1202 | `			}` |
|      11 | 1203 | `			tok_tok(ts,bClosed ? T_CONSTANT_ENCAPSED_STRING : T_ENCAPSED_AND_WHITESPACE,` |
|      10 | 1204 | `				(const char *)z,(int)(p-z),ts->iLine);` |
|      11 | 1205 | `			tok_bump_lines(ts,z,p);` |
|      11 | 1206 | `			ts->z = p;` |
|      11 | 1207 | `			return 0;` |
|       - | 1208 | `		}` |
|      13 | 1209 | `		if( z[1]=='"' ){` |
|       9 | 1210 | `			tok_scan_dquote(ts,'"');` |
|       9 | 1211 | `			return 0;` |
|       - | 1212 | `		}` |
|       5 | 1213 | `		if( tok_scan_heredoc(ts) ){ return 0; }` |
|       - | 1214 | ``		/* Not a heredoc after all: fall through and lex the `b` as an identifier. */`` |
|     ! 0 | 1215 | `	}` |
|       - | 1216 | `	/* Namespaced name / identifier / keyword. */` |
|     570 | 1217 | `	if( tok_is_label_start(c) \|\| (c=='\\' && z+1 < ts->zEnd && tok_is_label_start(z[1])) ){` |
|     112 | 1218 | `		const unsigned char *p = z;` |
|     112 | 1219 | `		int bLeadBackslash = (c=='\\');` |
|     112 | 1220 | `		int bInner = 0;                 /* saw an internal backslash */` |
|       - | 1221 | `		const unsigned char *firstLabelStart, *firstLabelEnd;` |
|     112 | 1222 | `		if( bLeadBackslash ){ p++; }` |
|     112 | 1223 | `		firstLabelStart = p;` |
|     582 | 1224 | `		while( p < ts->zEnd && tok_is_label(*p) ){ p++; }` |
|     112 | 1225 | `		firstLabelEnd = p;` |
|       - | 1226 | `		/* Consume further \label segments. */` |
|     112 | 1227 | `		while( p+1 < ts->zEnd && *p=='\\' && tok_is_label_start(p[1]) ){` |
|     ! 0 | 1228 | `			bInner = 1;` |
|     ! 0 | 1229 | `			p++;` |
|     ! 0 | 1230 | `			while( p < ts->zEnd && tok_is_label(*p) ){ p++; }` |
|     ! 0 | 1231 | `		}` |
|     112 | 1232 | `		if( bLeadBackslash ){` |
|     ! 0 | 1233 | `			tok_tok(ts,T_NAME_FULLY_QUALIFIED,(const char *)z,(int)(p-z),ts->iLine);` |
|     ! 0 | 1234 | `			ts->z = p;` |
|     ! 0 | 1235 | `			return 0;` |
|       - | 1236 | `		}` |
|     112 | 1237 | `		if( bInner ){` |
|     ! 0 | 1238 | `			int nFirst = (int)(firstLabelEnd - firstLabelStart);` |
|     ! 0 | 1239 | `			int id = T_NAME_QUALIFIED;` |
|     ! 0 | 1240 | `			if( tok_ci_eq((const char *)firstLabelStart,nFirst,"namespace") ){ id = T_NAME_RELATIVE; }` |
|     ! 0 | 1241 | `			tok_tok(ts,id,(const char *)z,(int)(p-z),ts->iLine);` |
|     ! 0 | 1242 | `			ts->z = p;` |
|     ! 0 | 1243 | `			return 0;` |
|       - | 1244 | `		}` |
|       - | 1245 | `		/* Lone identifier: keyword, contextual, or T_STRING. */` |
|       - | 1246 | `		{` |
|     112 | 1247 | `			int n = (int)(firstLabelEnd - z);` |
|     112 | 1248 | `			int id = T_STRING;` |
|       - | 1249 | `			sxu32 k;` |
|     112 | 1250 | `			if( bWasProp ){` |
|       9 | 1251 | `				id = T_STRING;              /* property access after -> / ?-> */` |
|     108 | 1252 | `			}else if( tok_ci_eq((const char *)z,n,"enum") ){` |
|       - | 1253 | `				/* Contextual, and php has TWO rules for it, tried in this order:` |
|       - | 1254 | `				 *` |
|       - | 1255 | `				 *   "enum" WS_OR_COMMENTS ("extends"\|"implements") WS_OR_COMMENTS` |
|       - | 1256 | `				 *        -> T_STRING: this is a CLASS NAMED "enum", as in` |
|       - | 1257 | ``				 *           `class Enum extends X {}` -- which is ordinary source`` |
|       - | 1258 | `				 *           (nikic/php-parser's own emulation tests carry it);` |
|       - | 1259 | `				 *   "enum" WS_OR_COMMENTS LABEL-START` |
|       - | 1260 | `				 *        -> T_ENUM.` |
|       - | 1261 | `				 *` |
|       - | 1262 | `				 * The separator is php's WHITESPACE_OR_COMMENTS, not plain` |
|       - | 1263 | `				 * whitespace: a block comment between the two is a separator too. */` |
|       9 | 1264 | `				int sawWs = 0;` |
|       9 | 1265 | `				const unsigned char *q = tok_skip_ws_comments(ts,firstLabelEnd,&sawWs);` |
|       9 | 1266 | `				if( sawWs && q < ts->zEnd && tok_is_label_start(*q) ){` |
|       9 | 1267 | `					const unsigned char *w = q;` |
|       - | 1268 | `					int nw;` |
|      55 | 1269 | `					while( w < ts->zEnd && tok_is_label(*w) ){ w++; }` |
|       9 | 1270 | `					nw = (int)(w - q);` |
|       8 | 1271 | `					if( tok_ci_eq((const char *)q,nw,"extends")` |
|       8 | 1272 | `					 \|\| tok_ci_eq((const char *)q,nw,"implements") ){` |
|       5 | 1273 | `						id = T_STRING;` |
|       3 | 1274 | `					}else{` |
|       5 | 1275 | `						id = T_ENUM;` |
|       - | 1276 | `					}` |
|       5 | 1277 | `				}` |
|     100 | 1278 | `			}else if( tok_ci_eq((const char *)z,n,"yield") ){` |
|       - | 1279 | `				/* Contextual: "yield from" collapses to one T_YIELD_FROM. */` |
|     ! 0 | 1280 | `				const unsigned char *q = firstLabelEnd;` |
|     ! 0 | 1281 | `				const unsigned char *ws = q;` |
|     ! 0 | 1282 | `				while( q < ts->zEnd && (*q==' '\|\|*q=='\t'\|\|*q=='\n'\|\|*q=='\r'\|\|*q=='\v'\|\|*q=='\f') ){ q++; }` |
|     ! 0 | 1283 | `				if( q > ws && q+4 <= ts->zEnd && tok_ci_eq((const char *)q,4,"from")` |
|     ! 0 | 1284 | `					&& (q+4 >= ts->zEnd \|\| !tok_is_label(q[4])) ){` |
|     ! 0 | 1285 | `					const unsigned char *e = q+4;` |
|     ! 0 | 1286 | `					tok_tok(ts,T_YIELD_FROM,(const char *)z,(int)(e-z),ts->iLine);` |
|     ! 0 | 1287 | `					tok_bump_lines(ts,firstLabelEnd,e);` |
|     ! 0 | 1288 | `					ts->z = e;` |
|     ! 0 | 1289 | `					return 0;` |
|       - | 1290 | `				}` |
|     ! 0 | 1291 | `				id = T_YIELD;` |
|     ! 0 | 1292 | `			}else{` |
|    5712 | 1293 | `				for( k = 0 ; k < SX_ARRAYSIZE(aKeyword) ; ++k ){` |
|    5664 | 1294 | `					if( tok_ci_eq((const char *)z,n,aKeyword[k].zName) ){` |
|      48 | 1295 | `						id = aKeyword[k].iId;` |
|      48 | 1296 | `						break;` |
|       - | 1297 | `					}` |
|    2810 | 1298 | `				}` |
|       - | 1299 | `			}` |
|       - | 1300 | `			/* php 8.4's asymmetric-visibility modifiers are ONE token whose text is` |
|       - | 1301 | ``			 * the whole `private(set)` -- no whitespace and no comment anywhere`` |
|       - | 1302 | `			 * inside it, and both halves case-insensitive. Emitted here rather than` |
|       - | 1303 | `			 * from the keyword table because the spelling straddles a paren pair` |
|       - | 1304 | `			 * that would otherwise lex as three tokens. */` |
|     110 | 1305 | `			if( (id == T_PRIVATE \|\| id == T_PROTECTED \|\| id == T_PUBLIC)` |
|      59 | 1306 | `			 && firstLabelEnd + 5 <= ts->zEnd` |
|      59 | 1307 | `			 && firstLabelEnd[0]=='('` |
|       7 | 1308 | `			 && tok_lower(firstLabelEnd[1])=='s'` |
|       6 | 1309 | `			 && tok_lower(firstLabelEnd[2])=='e'` |
|       6 | 1310 | `			 && tok_lower(firstLabelEnd[3])=='t'` |
|       8 | 1311 | `			 && firstLabelEnd[4]==')' ){` |
|       7 | 1312 | `				int idSet = id == T_PRIVATE ? T_PRIVATE_SET` |
|       5 | 1313 | `				          : (id == T_PROTECTED ? T_PROTECTED_SET : T_PUBLIC_SET);` |
|       7 | 1314 | `				tok_tok(ts,idSet,(const char *)z,n+5,ts->iLine);` |
|       7 | 1315 | `				ts->z = firstLabelEnd + 5;` |
|       7 | 1316 | `				return 0;` |
|       - | 1317 | `			}` |
|       - | 1318 | `			/* Under TOKEN_PARSE, a reserved word right after 'case' that names an` |
|       - | 1319 | `			 * enum case (followed by ';' or '=') is T_STRING; a switch 'case` |
|       - | 1320 | `			 * array(...)'/'case expr:' keeps the keyword. */` |
|     106 | 1321 | `			if( bWasCase && id != T_STRING ){` |
|     ! 0 | 1322 | `				const unsigned char *q = firstLabelEnd;` |
|     ! 0 | 1323 | `				while( q < ts->zEnd && tok_is_ws(*q) ){ q++; }` |
|       - | 1324 | `				/* ';' ends a pure case, a lone '=' (not '=='/'=>') starts a backed` |
|       - | 1325 | `				 * case value; both mark an enum case name. */` |
|     ! 0 | 1326 | `				if( q < ts->zEnd && (*q==';' \|\|` |
|     ! 0 | 1327 | `					(*q=='=' && (q+1>=ts->zEnd \|\| (q[1]!='=' && q[1]!='>')))) ){` |
|     ! 0 | 1328 | `					id = T_STRING;` |
|     ! 0 | 1329 | `				}` |
|     ! 0 | 1330 | `			}` |
|     106 | 1331 | `			if( id == T_STRING ){` |
|      62 | 1332 | `				tok_tok(ts,T_STRING,(const char *)z,n,ts->iLine);` |
|      32 | 1333 | `			}else{` |
|      46 | 1334 | `				tok_tok(ts,id,(const char *)z,n,ts->iLine);` |
|       - | 1335 | `			}` |
|     106 | 1336 | `			ts->z = firstLabelEnd;` |
|     106 | 1337 | `			if( id == T_HALT_COMPILER ){` |
|       7 | 1338 | `				tok_halt_tail(ts);` |
|       7 | 1339 | `				return -1;` |
|       - | 1340 | `			}` |
|       - | 1341 | `			/* Under TOKEN_PARSE, a reserved word naming a function/method or a` |
|       - | 1342 | `			 * class constant becomes T_STRING (semi-reserved words); force the` |
|       - | 1343 | `			 * next identifier to T_STRING like a member name. 'case' arms a` |
|       - | 1344 | `			 * conditional retag (enum case names only — see the identifier path). */` |
|     100 | 1345 | `			if( ts->bParse && (id == T_FUNCTION \|\| id == T_CONST) ){` |
|     ! 0 | 1346 | `				ts->bProp = 1;` |
|     ! 0 | 1347 | `			}` |
|     100 | 1348 | `			if( ts->bParse && id == T_CASE ){` |
|     ! 0 | 1349 | `				ts->bCaseName = 1;` |
|     ! 0 | 1350 | `			}` |
|     100 | 1351 | `			return 0;` |
|       - | 1352 | `		}` |
|       - | 1353 | `	}` |
|       - | 1354 | `	/* Lone backslash -> namespace separator. */` |
|     468 | 1355 | `	if( c=='\\' ){` |
|     ! 0 | 1356 | `		tok_tok(ts,T_NS_SEPARATOR,"\\",1,ts->iLine);` |
|     ! 0 | 1357 | `		ts->z++;` |
|     ! 0 | 1358 | `		return 0;` |
|       - | 1359 | `	}` |
|       - | 1360 | `	/* Number. */` |
|     468 | 1361 | `	if( (c>='0'&&c<='9') \|\| (c=='.' && z+1 < ts->zEnd && z[1]>='0' && z[1]<='9') ){` |
|      96 | 1362 | `		const unsigned char *p = z;` |
|      96 | 1363 | `		int isFloat = 0;` |
|       - | 1364 | `		int id;` |
|      94 | 1365 | `		if( c=='0' && p+2 < ts->zEnd && (p[1]=='x'\|\|p[1]=='X')` |
|       7 | 1366 | `				&& ((p[2]>='0'&&p[2]<='9')\|\|(p[2]>='a'&&p[2]<='f')\|\|(p[2]>='A'&&p[2]<='F')) ){` |
|       - | 1367 | `			const unsigned char *d0;` |
|       3 | 1368 | `			p += 2; d0 = p;` |
|       3 | 1369 | `			tok_digit_run(ts,&p,TOK_DIG_HEX);` |
|       3 | 1370 | `			id = tok_int_overflows(d0,p,16) ? T_DNUMBER : T_LNUMBER;` |
|      93 | 1371 | `		}else if( c=='0' && p+2 < ts->zEnd && (p[1]=='b'\|\|p[1]=='B') && (p[2]=='0'\|\|p[2]=='1') ){` |
|       - | 1372 | `			const unsigned char *d0;` |
|       2 | 1373 | `			p += 2; d0 = p;` |
|       2 | 1374 | `			tok_digit_run(ts,&p,TOK_DIG_BIN);` |
|       2 | 1375 | `			id = tok_int_overflows(d0,p,2) ? T_DNUMBER : T_LNUMBER;` |
|      92 | 1376 | `		}else if( c=='0' && p+2 < ts->zEnd && (p[1]=='o'\|\|p[1]=='O') && (p[2]>='0'&&p[2]<='7') ){` |
|       - | 1377 | `			const unsigned char *d0;` |
|     ! 0 | 1378 | `			p += 2; d0 = p;` |
|     ! 0 | 1379 | `			tok_digit_run(ts,&p,TOK_DIG_OCT);` |
|     ! 0 | 1380 | `			id = tok_int_overflows(d0,p,8) ? T_DNUMBER : T_LNUMBER;` |
|     ! 0 | 1381 | `		}else{` |
|      90 | 1382 | `			const unsigned char *intStart = p;` |
|      90 | 1383 | `			int allOctalDigits = 1;` |
|       - | 1384 | `			const unsigned char *q;` |
|      90 | 1385 | `			tok_digit_run(ts,&p,TOK_DIG_DEC);` |
|     184 | 1386 | `			for( q = intStart ; q < p ; ++q ){` |
|      96 | 1387 | `				if( *q>'7' && *q<='9' ){ allOctalDigits = 0; }` |
|      49 | 1388 | `			}` |
|      90 | 1389 | `			if( p < ts->zEnd && *p=='.' && !(c=='.') ){` |
|       - | 1390 | `				/* fractional part (unless the token itself started with '.') */` |
|       5 | 1391 | `				isFloat = 1;` |
|       5 | 1392 | `				p++;` |
|       5 | 1393 | `				tok_digit_run(ts,&p,TOK_DIG_DEC);` |
|      88 | 1394 | `			}else if( c=='.' ){` |
|       3 | 1395 | `				isFloat = 1; /* .5 style: leading dot already consumed below */` |
|       1 | 1396 | `			}` |
|      90 | 1397 | `			if( c=='.' ){` |
|       - | 1398 | `				/* token began with '.': consume the dot + digits here */` |
|       3 | 1399 | `				p = z+1;` |
|       3 | 1400 | `				tok_digit_run(ts,&p,TOK_DIG_DEC);` |
|       3 | 1401 | `				isFloat = 1;` |
|       1 | 1402 | `			}` |
|      90 | 1403 | `			if( p < ts->zEnd && (*p=='e'\|\|*p=='E') ){` |
|       5 | 1404 | `				const unsigned char *e = p+1;` |
|       5 | 1405 | `				if( e < ts->zEnd && (*e=='+'\|\|*e=='-') ){ e++; }` |
|       5 | 1406 | `				if( e < ts->zEnd && *e>='0' && *e<='9' ){` |
|       3 | 1407 | `					isFloat = 1;` |
|       3 | 1408 | `					p = e;` |
|       3 | 1409 | `					tok_digit_run(ts,&p,TOK_DIG_DEC);` |
|       1 | 1410 | `				}` |
|       2 | 1411 | `			}` |
|      90 | 1412 | `			if( isFloat ){` |
|       7 | 1413 | `				id = T_DNUMBER;` |
|      87 | 1414 | `			}else if( intStart < ts->zEnd && *intStart=='0' && (p-intStart) > 1 && allOctalDigits ){` |
|       - | 1415 | `				/* legacy octal 0NNN */` |
|     ! 0 | 1416 | `				id = tok_int_overflows(intStart+1,p,8) ? T_DNUMBER : T_LNUMBER;` |
|     ! 0 | 1417 | `			}else{` |
|      84 | 1418 | `				id = tok_int_overflows(intStart,p,10) ? T_DNUMBER : T_LNUMBER;` |
|       - | 1419 | `			}` |
|       - | 1420 | `		}` |
|      92 | 1421 | `		tok_tok(ts,id,(const char *)z,(int)(p-z),ts->iLine);` |
|      92 | 1422 | `		ts->z = p;` |
|      92 | 1423 | `		return 0;` |
|       - | 1424 | `	}` |
|       - | 1425 | `	/* Strings. */` |
|     374 | 1426 | `	if( c=='\'' ){` |
|       5 | 1427 | `		const unsigned char *p = z+1;` |
|       5 | 1428 | `		int bClosed = 0;` |
|      13 | 1429 | `		while( p < ts->zEnd ){` |
|      13 | 1430 | `			if( *p=='\\' && p+1 < ts->zEnd ){ p += 2; continue; }` |
|      13 | 1431 | `			if( *p=='\'' ){ p++; bClosed = 1; break; }` |
|       9 | 1432 | `			p++;` |
|       1 | 1433 | `		}` |
|       - | 1434 | `		/* An unterminated single-quoted string is one T_ENCAPSED_AND_WHITESPACE in php. */` |
|       5 | 1435 | `		tok_tok(ts,bClosed ? T_CONSTANT_ENCAPSED_STRING : T_ENCAPSED_AND_WHITESPACE,` |
|       4 | 1436 | `			(const char *)z,(int)(p-z),ts->iLine);` |
|       5 | 1437 | `		tok_bump_lines(ts,z,p);` |
|       5 | 1438 | `		ts->z = p;` |
|       5 | 1439 | `		return 0;` |
|       - | 1440 | `	}` |
|     370 | 1441 | `	if( c=='"' ){ tok_scan_dquote(ts,'"'); return 0; }` |
|     334 | 1442 | ``	if( c=='`' ){ tok_scan_dquote(ts,'`'); return 0; }`` |
|       - | 1443 | `	/* Heredoc / nowdoc (falls through to operators if not a valid start). */` |
|     334 | 1444 | `	if( c=='<' && z+2 < ts->zEnd && z[1]=='<' && z[2]=='<' ){` |
|      22 | 1445 | `		if( tok_scan_heredoc(ts) ){ return 0; }` |
|     ! 0 | 1446 | `	}` |
|       - | 1447 | `	/* Cast operators. */` |
|     314 | 1448 | `	if( c=='(' ){` |
|      22 | 1449 | `		if( tok_try_cast(ts) ){ return 0; }` |
|       6 | 1450 | `	}` |
|       - | 1451 | `	/* Object operators set the one-shot property state (next identifier -> T_STRING). */` |
|     306 | 1452 | `	if( c=='?' && z+2 < ts->zEnd && z[1]=='-' && z[2]=='>' ){` |
|       5 | 1453 | `		tok_tok(ts,T_NULLSAFE_OBJECT_OPERATOR,"?->",3,ts->iLine);` |
|       5 | 1454 | `		ts->z += 3;` |
|       5 | 1455 | `		ts->bProp = 1;` |
|       5 | 1456 | `		return 0;` |
|       - | 1457 | `	}` |
|     302 | 1458 | `	if( c=='-' && z+1 < ts->zEnd && z[1]=='>' ){` |
|       5 | 1459 | `		tok_tok(ts,T_OBJECT_OPERATOR,"->",2,ts->iLine);` |
|       5 | 1460 | `		ts->z += 2;` |
|       5 | 1461 | `		ts->bProp = 1;` |
|       5 | 1462 | `		return 0;` |
|       - | 1463 | `	}` |
|       - | 1464 | `	/* Under TOKEN_PARSE, php re-tags a reserved word used as a member name after` |
|       - | 1465 | `	 * "::" as T_STRING (e.g. Foo::class, Foo::empty). Mirror that with bProp. */` |
|     298 | 1466 | `	if( c==':' && z+1 < ts->zEnd && z[1]==':' ){` |
|       3 | 1467 | `		tok_tok(ts,T_DOUBLE_COLON,"::",2,ts->iLine);` |
|       3 | 1468 | `		ts->z += 2;` |
|       3 | 1469 | `		if( ts->bParse ){ ts->bProp = 1; }` |
|       3 | 1470 | `		return 0;` |
|       - | 1471 | `	}` |
|       - | 1472 | `	/* Multi-char and single-char operators (longest match first). */` |
|       - | 1473 | `	{` |
|     296 | 1474 | `		const unsigned char *e = ts->zEnd;` |
|     296 | 1475 | `		int r0 = c;` |
|     296 | 1476 | `		int r1 = (z+1<e)?z[1]:-1;` |
|     296 | 1477 | `		int r2 = (z+2<e)?z[2]:-1;` |
|       - | 1478 | `		#define TK3(a,b,cc,id) if(r0==(a)&&r1==(b)&&r2==(cc)){ tok_tok(ts,id,(const char*)z,3,ts->iLine); ts->z+=3; return 0; }` |
|       - | 1479 | `		#define TK2(a,b,id)    if(r0==(a)&&r1==(b)){ tok_tok(ts,id,(const char*)z,2,ts->iLine); ts->z+=2; return 0; }` |
|     296 | 1480 | `		TK3('=','=','=',T_IS_IDENTICAL)` |
|     296 | 1481 | `		TK3('!','=','=',T_IS_NOT_IDENTICAL)` |
|     296 | 1482 | `		TK3('<','=','>',T_SPACESHIP)` |
|     296 | 1483 | `		TK3('*','*','=',T_POW_EQUAL)` |
|     296 | 1484 | `		TK3('.','.','.',T_ELLIPSIS)` |
|     296 | 1485 | `		TK3('<','<','=',T_SL_EQUAL)` |
|     296 | 1486 | `		TK3('>','>','=',T_SR_EQUAL)` |
|     296 | 1487 | `		TK3('?','?','=',T_COALESCE_EQUAL)` |
|     296 | 1488 | `		TK2('=','=',T_IS_EQUAL)` |
|     296 | 1489 | `		TK2('!','=',T_IS_NOT_EQUAL)` |
|     296 | 1490 | `		TK2('<','>',T_IS_NOT_EQUAL)` |
|     296 | 1491 | `		TK2('<','=',T_IS_SMALLER_OR_EQUAL)` |
|     296 | 1492 | `		TK2('>','=',T_IS_GREATER_OR_EQUAL)` |
|     296 | 1493 | `		TK2('&','&',T_BOOLEAN_AND)` |
|     296 | 1494 | `		TK2('\|','\|',T_BOOLEAN_OR)` |
|     296 | 1495 | `		TK2('\|','>',T_PIPE)` |
|     296 | 1496 | `		TK2('+','+',T_INC)` |
|     296 | 1497 | `		TK2('-','-',T_DEC)` |
|     296 | 1498 | `		TK2('=','>',T_DOUBLE_ARROW)` |
|     296 | 1499 | `		TK2('<','<',T_SL)` |
|     296 | 1500 | `		TK2('>','>',T_SR)` |
|     296 | 1501 | `		TK2('*','*',T_POW)` |
|     296 | 1502 | `		TK2('?','?',T_COALESCE)` |
|     296 | 1503 | `		TK2('+','=',T_PLUS_EQUAL)` |
|     296 | 1504 | `		TK2('-','=',T_MINUS_EQUAL)` |
|     296 | 1505 | `		TK2('*','=',T_MUL_EQUAL)` |
|     296 | 1506 | `		TK2('/','=',T_DIV_EQUAL)` |
|     296 | 1507 | `		TK2('.','=',T_CONCAT_EQUAL)` |
|     296 | 1508 | `		TK2('%','=',T_MOD_EQUAL)` |
|     296 | 1509 | `		TK2('&','=',T_AND_EQUAL)` |
|     296 | 1510 | `		TK2('\|','=',T_OR_EQUAL)` |
|     296 | 1511 | `		TK2('^','=',T_XOR_EQUAL)` |
|       - | 1512 | `		#undef TK3` |
|       - | 1513 | `		#undef TK2` |
|       - | 1514 | `	}` |
|       - | 1515 | `	/* Ampersand: FOLLOWED (by var / vararg) vs NOT, looking past whitespace. */` |
|     296 | 1516 | `	if( c=='&' ){` |
|     ! 0 | 1517 | `		const unsigned char *p = z+1;` |
|       - | 1518 | `		int id;` |
|     ! 0 | 1519 | `		while( p < ts->zEnd && tok_is_ws(*p) ){ p++; }` |
|     ! 0 | 1520 | `		if( (p < ts->zEnd && *p=='$') \|\|` |
|     ! 0 | 1521 | `			(p+2 < ts->zEnd && p[0]=='.' && p[1]=='.' && p[2]=='.') ){` |
|     ! 0 | 1522 | `			id = T_AMPERSAND_FOLLOWED_BY_VAR_OR_VARARG;` |
|     ! 0 | 1523 | `		}else{` |
|     ! 0 | 1524 | `			id = T_AMPERSAND_NOT_FOLLOWED_BY_VAR_OR_VARARG;` |
|       - | 1525 | `		}` |
|     ! 0 | 1526 | `		tok_tok(ts,id,"&",1,ts->iLine);` |
|     ! 0 | 1527 | `		ts->z++;` |
|     ! 0 | 1528 | `		return 0;` |
|       - | 1529 | `	}` |
|       - | 1530 | `	/* Control bytes that begin no token are T_BAD_CHARACTER in php. */` |
|     296 | 1531 | `	if( c < 0x20 \|\| c == 0x7f ){` |
|     ! 0 | 1532 | `		char ch = (char)c;` |
|     ! 0 | 1533 | `		tok_tok(ts,T_BAD_CHARACTER,&ch,1,ts->iLine);` |
|     ! 0 | 1534 | `		ts->z++;` |
|     ! 0 | 1535 | `		return 0;` |
|       - | 1536 | `	}` |
|       - | 1537 | `	/* Single-char token, returned as a bare string. */` |
|       - | 1538 | `	{` |
|     296 | 1539 | `		char ch = (char)c;` |
|     296 | 1540 | `		tok_plain(ts,&ch,1);` |
|     296 | 1541 | `		ts->z++;` |
|     296 | 1542 | `		if( c=='{' ) return '{';` |
|     286 | 1543 | `		if( c=='}' ) return '}';` |
|     276 | 1544 | `		return 0;` |
|       - | 1545 | `	}` |
|     532 | 1546 | `}` |
|       - | 1547 |  |
|       - | 1548 | `/* Emit T_OPEN_TAG/T_OPEN_TAG_WITH_ECHO at ts->z; returns 1 if a tag was found. */` |
|     102 | 1549 | `static int tok_open_tag(tok_state *ts){` |
|     102 | 1550 | `	const unsigned char *z = ts->z;` |
|     102 | 1551 | `	if( z+2 < ts->zEnd && z[0]=='<' && z[1]=='?' && z[2]=='=' ){` |
|     ! 0 | 1552 | `		tok_tok(ts,T_OPEN_TAG_WITH_ECHO,"<?=",3,ts->iLine);` |
|     ! 0 | 1553 | `		ts->z = z+3;` |
|     ! 0 | 1554 | `		return 1;` |
|       - | 1555 | `	}` |
|     100 | 1556 | `	if( z+4 <= ts->zEnd && z[0]=='<' && z[1]=='?'` |
|     102 | 1557 | `		&& tok_lower(z[2])=='p' && tok_lower(z[3])=='h' && z+5 <= ts->zEnd && tok_lower(z[4])=='p' ){` |
|       - | 1558 | `		/* Require <?php to be followed by whitespace or EOF. */` |
|     102 | 1559 | `		const unsigned char *p = z+5;` |
|     102 | 1560 | `		if( p >= ts->zEnd \|\| tok_is_ws(*p) ){` |
|     102 | 1561 | `			const unsigned char *e = z+5;` |
|     102 | 1562 | `			int iLine = ts->iLine;` |
|     102 | 1563 | `			if( e < ts->zEnd && tok_is_ws(*e) ){` |
|       - | 1564 | `				/* ONE trailing whitespace character joins the tag -- and a CRLF is` |
|       - | 1565 | `` 				 * one newline, not two characters: php's scanner eats `\r\n` `` |
|       - | 1566 | ``				 * whole, so `<?php\r\n` is a single T_OPEN_TAG there and was an`` |
|       - | 1567 | `				 * open tag plus a stray T_WHITESPACE("\n") here (every CRLF file's` |
|       - | 1568 | `				 * token stream, and php_strip_whitespace's answer with it). */` |
|     102 | 1569 | `				int bCrLf = *e == '\r' && e+1 < ts->zEnd && e[1] == '\n';` |
|     102 | 1570 | `				if( bCrLf \|\| tok_at_nl(e,ts->zEnd) ){ ts->iLine++; }` |
|     102 | 1571 | `				if( bCrLf ){` |
|       5 | 1572 | `					e++;` |
|       2 | 1573 | `				}` |
|     102 | 1574 | `				e++;` |
|      50 | 1575 | `			}` |
|     102 | 1576 | `			tok_tok(ts,T_OPEN_TAG,(const char *)z,(int)(e-z),iLine);` |
|     102 | 1577 | `			ts->z = e;` |
|     102 | 1578 | `			return 1;` |
|       - | 1579 | `		}` |
|     ! 0 | 1580 | `	}` |
|     ! 0 | 1581 | `	return 0;` |
|      52 | 1582 | `}` |
|       - | 1583 |  |
|       - | 1584 | `/* The scanning driver: alternate inline-HTML and PHP modes over the source. */` |
|     108 | 1585 | `static void tok_run(tok_state *ts){` |
|     208 | 1586 | `	while( ts->z < ts->zEnd && !ts->bOOM && !ts->bStop ){` |
|       - | 1587 | `		/* Inline HTML until the next open tag. */` |
|     116 | 1588 | `		const unsigned char *zHtml = ts->z;` |
|     116 | 1589 | `		int iHtmlLine = ts->iLine;` |
|     214 | 1590 | `		while( ts->z < ts->zEnd ){` |
|     200 | 1591 | `			if( ts->z[0]=='<' && ts->z+1 < ts->zEnd && ts->z[1]=='?' ){` |
|       - | 1592 | `				/* Only <?php and <?= are recognised (short tags off). */` |
|     102 | 1593 | `				const unsigned char *s = ts->z;` |
|     102 | 1594 | `				int bTag = 0;` |
|     102 | 1595 | `				if( s+2 < ts->zEnd && s[2]=='=' ){ bTag = 1; }` |
|     100 | 1596 | `				else if( s+4 <= ts->zEnd && tok_lower(s[2])=='p' && tok_lower(s[3])=='h'` |
|     100 | 1597 | `					&& s+5 <= ts->zEnd && tok_lower(s[4])=='p'` |
|     102 | 1598 | `					&& (s+5 >= ts->zEnd \|\| tok_is_ws(s[5])) ){ bTag = 1; }` |
|     102 | 1599 | `				if( bTag ){ break; }` |
|     ! 0 | 1600 | `			}` |
|     100 | 1601 | `			if( tok_at_nl(ts->z,ts->zEnd) ){ ts->iLine++; }` |
|     100 | 1602 | `			ts->z++;` |
|       2 | 1603 | `		}` |
|     116 | 1604 | `		if( ts->z > zHtml ){` |
|      18 | 1605 | `			tok_tok(ts,T_INLINE_HTML,(const char *)zHtml,(int)(ts->z-zHtml),iHtmlLine);` |
|       8 | 1606 | `		}` |
|     116 | 1607 | `		if( ts->z >= ts->zEnd ){ break; }` |
|     102 | 1608 | `		if( !tok_open_tag(ts) ){` |
|       - | 1609 | `			/* Not actually a tag (shouldn't happen given the check above). */` |
|     ! 0 | 1610 | `			if( tok_at_nl(ts->z,ts->zEnd) ){ ts->iLine++; }` |
|     ! 0 | 1611 | `			ts->z++;` |
|     ! 0 | 1612 | `			continue;` |
|       - | 1613 | `		}` |
|       - | 1614 | `		/* PHP mode until a close tag or EOF. */` |
|     102 | 1615 | `		ts->bProp = 0;` |
|    1138 | 1616 | `		while( ts->z < ts->zEnd && !ts->bOOM ){` |
|    1056 | 1617 | `			int eff = tok_lex_one(ts);` |
|    1056 | 1618 | `			if( eff < 0 ){ break; }` |
|       2 | 1619 | `		}` |
|       2 | 1620 | `	}` |
|     108 | 1621 | `}` |
|       - | 1622 |  |
|       - | 1623 | `/*` |
|       - | 1624 | ` * array token_get_all(string $source [, int $flags = 0 ])` |
|       - | 1625 | ` */` |
|      44 | 1626 | `static int PH7_builtin_token_get_all(ph7_context *pCtx,int nArg,ph7_value **apArg){` |
|       - | 1627 | `	tok_state ts;` |
|       - | 1628 | `	const char *zSrc;` |
|      44 | 1629 | `	int nSrc = 0;` |
|      44 | 1630 | `	if( nArg < 1 ){` |
|     ! 0 | 1631 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|     ! 0 | 1632 | `			"token_get_all() expects at least 1 argument, %d given",nArg);` |
|       - | 1633 | `	}` |
|      44 | 1634 | `	zSrc = ph7_value_to_string(apArg[0],&nSrc);` |
|      44 | 1635 | `	SyZero(&ts,sizeof(ts));` |
|      44 | 1636 | `	ts.pCtx  = pCtx;` |
|      44 | 1637 | `	ts.pArray = ph7_context_new_array(pCtx);` |
|      44 | 1638 | `	ts.pS    = ph7_context_new_scalar(pCtx);` |
|      44 | 1639 | `	ts.pId   = ph7_context_new_scalar(pCtx);` |
|      44 | 1640 | `	ts.pText = ph7_context_new_scalar(pCtx);` |
|      44 | 1641 | `	ts.pLine = ph7_context_new_scalar(pCtx);` |
|      44 | 1642 | `	if( ts.pArray==0 \|\| ts.pS==0 \|\| ts.pId==0 \|\| ts.pText==0 \|\| ts.pLine==0 ){` |
|     ! 0 | 1643 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1644 | `	}` |
|      44 | 1645 | `	ts.z    = (const unsigned char *)zSrc;` |
|      44 | 1646 | `	ts.zEnd = ts.z + (nSrc > 0 ? (sxu32)nSrc : 0);` |
|      44 | 1647 | `	ts.iLine = 1;` |
|      44 | 1648 | `	if( nArg > 1 && (ph7_value_to_int(apArg[1]) & TOK_TOKEN_PARSE) ){` |
|     ! 0 | 1649 | `		ts.bParse = 1;` |
|     ! 0 | 1650 | `	}` |
|      44 | 1651 | `	tok_run(&ts);` |
|      44 | 1652 | `	if( ts.bOOM ){` |
|     ! 0 | 1653 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1654 | `	}` |
|      44 | 1655 | `	ph7_result_value(pCtx,ts.pArray);` |
|      44 | 1656 | `	return PH7_OK;` |
|      23 | 1657 | `}` |
|       - | 1658 |  |
|       - | 1659 | `/*` |
|       - | 1660 | ` * string php_strip_whitespace(string $filename)` |
|       - | 1661 | ` *   The source of $filename with its comments and whitespace stripped.` |
|       - | 1662 | ` *` |
|       - | 1663 | `` * Composer's classmap generator is built on it -- `PhpFileParser` refuses to run`` |
|       - | 1664 | `` * without it and reports the absence as `disabled by the disable_functions`` |
|       - | 1665 | `` * directive` -- so `composer dump-autoload` stopped here. php implements it as`` |
|       - | 1666 | `` * `zend_strip` over its own scanner; PHL runs the tokenizer token_get_all() already`` |
|       - | 1667 | ` * uses, with tok_strip() as the sink.` |
|       - | 1668 | ` *` |
|       - | 1669 | ` * The file is opened through the stream layer, so a userland wrapper serves it and` |
|       - | 1670 | ``  * a failure is php's own `php_strip_whitespace(<path>): Failed to open stream: …` `` |
|       - | 1671 | `` * naming this function. A leading `#!` line is dropped before scanning, as php's`` |
|       - | 1672 | ` * open_file_for_scanning does.` |
|       - | 1673 | ` */` |
|      52 | 1674 | `static int PH7_builtin_php_strip_whitespace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1675 | `{` |
|       - | 1676 | `	const ph7_io_stream *pStream;` |
|       - | 1677 | `	const char *zFile;` |
|       - | 1678 | `	void *pHandle;` |
|       - | 1679 | `	SyBlob sSrc,sOut;` |
|       - | 1680 | `	tok_state ts;` |
|      53 | 1681 | `	int nLen = 0;` |
|      53 | 1682 | `	if( nArg < 1 ){` |
|     ! 0 | 1683 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|     ! 0 | 1684 | `			"php_strip_whitespace() expects exactly 1 argument, %d given",nArg);` |
|       - | 1685 | `	}` |
|      53 | 1686 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      53 | 1687 | `	if( PH7_VfsEmptyPathRefused(pCtx,nLen) ){` |
|       3 | 1688 | `		return PH7_OK;` |
|       - | 1689 | `	}` |
|      51 | 1690 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      51 | 1691 | `	if( pStream == 0 ){` |
|     ! 0 | 1692 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|     ! 0 | 1693 | `		ph7_result_string(pCtx,"",0);` |
|     ! 0 | 1694 | `		return PH7_OK;` |
|       - | 1695 | `	}` |
|       - | 1696 | `	{` |
|       - | 1697 | ``		/* php opens this one the way `include` does, and that opener refuses a`` |
|       - | 1698 | `		 * DIRECTORY -- with the same sentence a missing path gets. The plain` |
|       - | 1699 | `		 * files wrapper is the only one that has directories to refuse. */` |
|      51 | 1700 | `		const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;` |
|      50 | 1701 | `		if( pStream == pCtx->pVm->pDefStream && pVfs && pVfs->xIsdir` |
|      51 | 1702 | `		 && pVfs->xIsdir(zFile) == PH7_OK ){` |
|       3 | 1703 | `			int nAsked = 0;` |
|       3 | 1704 | `			const char *zAsked = ph7_value_to_string(apArg[0],&nAsked);` |
|       4 | 1705 | `			PH7_VmThrowWarningFmt(pCtx->pVm,` |
|       - | 1706 | `				"%s(%.*s): Failed to open stream: No such file or directory",` |
|       1 | 1707 | `				ph7_function_name(pCtx),nAsked,zAsked);` |
|       3 | 1708 | `			ph7_result_string(pCtx,"",0);` |
|       3 | 1709 | `			return PH7_OK;` |
|       - | 1710 | `		}` |
|       - | 1711 | `	}` |
|      73 | 1712 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0,` |
|      24 | 1713 | `		ph7_function_name(pCtx));` |
|      49 | 1714 | `	if( pHandle == 0 ){` |
|       3 | 1715 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|       3 | 1716 | `		ph7_result_string(pCtx,"",0);` |
|       3 | 1717 | `		return PH7_OK;` |
|       - | 1718 | `	}` |
|      47 | 1719 | `	SyBlobInit(&sSrc,&pCtx->pVm->sAllocator);` |
|      47 | 1720 | `	PH7_StreamReadWholeFile(pHandle,pStream,&sSrc);` |
|      47 | 1721 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      47 | 1722 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|       - | 1723 | `	{` |
|      47 | 1724 | `		const unsigned char *zIn = (const unsigned char *)SyBlobData(&sSrc);` |
|      47 | 1725 | `		sxu32 nIn = SyBlobLength(&sSrc);` |
|      47 | 1726 | `		if( nIn >= 2 && zIn[0] == '#' && zIn[1] == '!' ){` |
|       - | 1727 | `			/* php's scanner eats the shebang line before the first token. */` |
|       5 | 1728 | `			sxu32 i = 2;` |
|      51 | 1729 | `			while( i < nIn && zIn[i] != '\n' ){` |
|      47 | 1730 | `				i++;` |
|       1 | 1731 | `			}` |
|       5 | 1732 | `			if( i < nIn ){` |
|       5 | 1733 | `				i++;` |
|       2 | 1734 | `			}` |
|       5 | 1735 | `			zIn += i;` |
|       5 | 1736 | `			nIn -= i;` |
|       2 | 1737 | `		}` |
|      47 | 1738 | `		SyZero(&ts,sizeof(ts));` |
|      47 | 1739 | `		ts.pCtx = pCtx;` |
|      47 | 1740 | `		ts.pStrip = &sOut;` |
|      47 | 1741 | `		ts.z = zIn;` |
|      47 | 1742 | `		ts.zEnd = zIn + nIn;` |
|      47 | 1743 | `		ts.iLine = 1;` |
|      47 | 1744 | `		tok_run(&ts);` |
|       - | 1745 | `	}` |
|      47 | 1746 | `	if( ts.bOOM ){` |
|     ! 0 | 1747 | `		SyBlobRelease(&sSrc);` |
|     ! 0 | 1748 | `		SyBlobRelease(&sOut);` |
|     ! 0 | 1749 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1750 | `	}` |
|      47 | 1751 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|      47 | 1752 | `	SyBlobRelease(&sSrc);` |
|      47 | 1753 | `	SyBlobRelease(&sOut);` |
|      47 | 1754 | `	return PH7_OK;` |
|      27 | 1755 | `}` |
|       - | 1756 | `/*` |
|       - | 1757 | ` * string token_name(int $id)` |
|       - | 1758 | ` */` |
|     550 | 1759 | `static int PH7_builtin_token_name(ph7_context *pCtx,int nArg,ph7_value **apArg){` |
|       - | 1760 | `	int iId;` |
|       - | 1761 | `	const char *zName;` |
|     550 | 1762 | `	if( nArg < 1 ){` |
|     ! 0 | 1763 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|     ! 0 | 1764 | `			"token_name() expects exactly 1 argument, %d given",nArg);` |
|       - | 1765 | `	}` |
|     550 | 1766 | `	iId = ph7_value_to_int(apArg[0]);` |
|     550 | 1767 | `	zName = TokConstName(iId);` |
|     550 | 1768 | `	if( zName == 0 ){` |
|     ! 0 | 1769 | `		ph7_result_string(pCtx,"UNKNOWN",(int)sizeof("UNKNOWN")-1);` |
|     ! 0 | 1770 | `	}else{` |
|     550 | 1771 | `		ph7_result_string(pCtx,zName,-1/*SyStrlen*/);` |
|       - | 1772 | `	}` |
|     550 | 1773 | `	return PH7_OK;` |
|     276 | 1774 | `}` |
|       - | 1775 |  |
|       - | 1776 | `/*` |
|       - | 1777 | ` * ---------------------------------------------------------------------------` |
|       - | 1778 | ` * The PhpToken class, declared and bodied in C.` |
|       - | 1779 | ` *` |
|       - | 1780 | `` * php's is NOT final and `tokenize()` returns `static[]` -- subclassing is the`` |
|       - | 1781 | ` * documented way to attach behaviour to a token stream, and the embedded PHP` |
|       - | 1782 | `` * declared it `final`, which made `class MyToken extends PhpToken` a fatal here`` |
|       - | 1783 | ` * and works in php. Its CONSTRUCTOR is what php marks final instead, which is` |
|       - | 1784 | `` * why php can (and does) skip it: `tokenize()` builds each instance and writes`` |
|       - | 1785 | ` * the four slots directly, so a subclass's own declared properties keep their` |
|       - | 1786 | ` * defaults.` |
|       - | 1787 | ` *` |
|       - | 1788 | ` * The four properties are php's typed-and-UNINITIALIZED shape` |
|       - | 1789 | `` * (`public int $id;` -- no default at all), so a read before construction is`` |
|       - | 1790 | ` * php's "must not be accessed before initialization" Error, and the four` |
|       - | 1791 | ` * methods that need a slot say so with php's own text rather than reading a` |
|       - | 1792 | ` * zero.` |
|       - | 1793 | ` * ---------------------------------------------------------------------------` |
|       - | 1794 | ` */` |
|       - | 1795 | `/* php's php_token_get_id / php_token_get_text: the slot, or the Error php` |
|       - | 1796 | ` * raises for an object that was never constructed. */` |
|      58 | 1797 | `static ph7_value * TokSlot(ph7_context *pCtx,const char *zName,sxi32 *pRc)` |
|       2 | 1798 | `{` |
|      60 | 1799 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      60 | 1800 | `	ph7_value *pVal = pThis ? PH7_NativeAttr(pThis,zName) : 0;` |
|      60 | 1801 | `	*pRc = PH7_OK;` |
|      60 | 1802 | `	if( pVal == 0 \|\| PH7_NativeAttrIsUninit(pThis,zName) ){` |
|      13 | 1803 | `		*pRc = PH7_VmThrowException(pCtx,"Error",` |
|       4 | 1804 | `			"Typed property PhpToken::$%s must not be accessed before initialization",zName);` |
|       9 | 1805 | `		return 0;` |
|       - | 1806 | `	}` |
|      52 | 1807 | `	return pVal;` |
|      31 | 1808 | `}` |
|       - | 1809 | `/* PhpToken::__construct(int $id, string $text, int $line = -1, int $pos = -1) */` |
|       2 | 1810 | `static int vm_builtin_PhpToken_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1811 | `{` |
|       3 | 1812 | `	ph7_vm *pVm = pCtx->pVm;` |
|       3 | 1813 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       - | 1814 | `	const char *zText;` |
|       3 | 1815 | `	int nText = 0;` |
|       3 | 1816 | `	if( pThis == 0 \|\| nArg < 2 ){` |
|     ! 0 | 1817 | `		return PH7_OK;` |
|       - | 1818 | `	}` |
|       3 | 1819 | `	zText = ph7_value_to_string(apArg[1],&nText);` |
|       3 | 1820 | `	PH7_NativeSetAttrInt(pVm,pThis,"id",ph7_value_to_int64(apArg[0]));` |
|       3 | 1821 | `	PH7_NativeSetAttrStr(pVm,pThis,"text",zText,nText);` |
|       3 | 1822 | `	PH7_NativeSetAttrInt(pVm,pThis,"line",nArg > 2 ? ph7_value_to_int64(apArg[2]) : -1);` |
|       3 | 1823 | `	PH7_NativeSetAttrInt(pVm,pThis,"pos",nArg > 3 ? ph7_value_to_int64(apArg[3]) : -1);` |
|       3 | 1824 | `	return PH7_OK;` |
|       2 | 1825 | `}` |
|       - | 1826 | `/*` |
|       - | 1827 | ` * PhpToken::tokenize(string $code, int $flags = 0): static[]` |
|       - | 1828 | ` *` |
|       - | 1829 | ` * The scanner token_get_all() drives, told to emit INSTANCES of the CALLED` |
|       - | 1830 | `` * class -- php's own `token_class` branch, and the reason the position and the`` |
|       - | 1831 | ` * line are the scanner's own rather than a running total recomputed in PHP.` |
|       - | 1832 | ` */` |
|      20 | 1833 | `static int vm_builtin_PhpToken_tokenize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1834 | `{` |
|      22 | 1835 | `	ph7_class *pClass = PH7_ContextCalledClass(pCtx);` |
|       - | 1836 | `	tok_state ts;` |
|       - | 1837 | `	const char *zSrc;` |
|      22 | 1838 | `	int nSrc = 0;` |
|      22 | 1839 | `	if( pClass == 0 ){` |
|     ! 0 | 1840 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1841 | `	}` |
|      22 | 1842 | `	if( pClass->iFlags & PH7_CLASS_ABSTRACT ){` |
|       - | 1843 | `		/* php checks the construction precondition ONCE, before scanning. */` |
|       4 | 1844 | `		return PH7_VmThrowException(pCtx,"Error","Cannot instantiate abstract class %z",` |
|       1 | 1845 | `			&pClass->sName);` |
|       - | 1846 | `	}` |
|      20 | 1847 | `	zSrc = nArg > 0 ? ph7_value_to_string(apArg[0],&nSrc) : "";` |
|      20 | 1848 | `	SyZero(&ts,sizeof(ts));` |
|      20 | 1849 | `	ts.pCtx = pCtx;` |
|      20 | 1850 | `	ts.pArray = ph7_context_new_array(pCtx);` |
|      20 | 1851 | `	ts.pS    = ph7_context_new_scalar(pCtx);` |
|      20 | 1852 | `	ts.pId   = ph7_context_new_scalar(pCtx);` |
|      20 | 1853 | `	ts.pText = ph7_context_new_scalar(pCtx);` |
|      20 | 1854 | `	ts.pLine = ph7_context_new_scalar(pCtx);` |
|      20 | 1855 | `	if( ts.pArray == 0 \|\| ts.pS == 0 \|\| ts.pId == 0 \|\| ts.pText == 0 \|\| ts.pLine == 0 ){` |
|     ! 0 | 1856 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1857 | `	}` |
|      20 | 1858 | `	ts.pTokClass = pClass;` |
|      20 | 1859 | `	ts.z    = (const unsigned char *)zSrc;` |
|      20 | 1860 | `	ts.zEnd = ts.z + (nSrc > 0 ? (sxu32)nSrc : 0);` |
|      20 | 1861 | `	ts.iLine = 1;` |
|      20 | 1862 | `	if( nArg > 1 && (ph7_value_to_int(apArg[1]) & TOK_TOKEN_PARSE) ){` |
|     ! 0 | 1863 | `		ts.bParse = 1;` |
|     ! 0 | 1864 | `	}` |
|      20 | 1865 | `	tok_run(&ts);` |
|      20 | 1866 | `	if( ts.bOOM ){` |
|     ! 0 | 1867 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1868 | `	}` |
|      20 | 1869 | `	ph7_result_value(pCtx,ts.pArray);` |
|      20 | 1870 | `	return PH7_OK;` |
|      12 | 1871 | `}` |
|       - | 1872 | `/* One arm of is(): an int matches the id, a string matches the text. */` |
|      24 | 1873 | `static int TokMatch(ph7_context *pCtx,ph7_value *pKind,int *pbYes,sxi32 *pRc)` |
|       1 | 1874 | `{` |
|       - | 1875 | `	ph7_value *pSlot;` |
|      25 | 1876 | `	*pbYes = 0;` |
|      25 | 1877 | `	*pRc = PH7_OK;` |
|      25 | 1878 | `	if( pKind->iFlags & MEMOBJ_INT ){` |
|      11 | 1879 | `		pSlot = TokSlot(pCtx,"id",pRc);` |
|      11 | 1880 | `		if( pSlot == 0 ){` |
|       3 | 1881 | `			return 0;` |
|       - | 1882 | `		}` |
|       9 | 1883 | `		*pbYes = ph7_value_to_int64(pSlot) == pKind->x.iVal;` |
|       9 | 1884 | `		return 1;` |
|       - | 1885 | `	}` |
|      15 | 1886 | `	if( pKind->iFlags & MEMOBJ_STRING ){` |
|       9 | 1887 | `		int nText = 0,nKind = 0;` |
|       9 | 1888 | `		const char *zKind = ph7_value_to_string(pKind,&nKind);` |
|       - | 1889 | `		const char *zText;` |
|       9 | 1890 | `		pSlot = TokSlot(pCtx,"text",pRc);` |
|       9 | 1891 | `		if( pSlot == 0 ){` |
|     ! 0 | 1892 | `			return 0;` |
|       - | 1893 | `		}` |
|       9 | 1894 | `		zText = ph7_value_to_string(pSlot,&nText);` |
|      12 | 1895 | `		*pbYes = nText == nKind && (nText < 1 \|\| SyMemcmp(zText,zKind,(sxu32)nText) == 0);` |
|       9 | 1896 | `		return 1;` |
|       - | 1897 | `	}` |
|       7 | 1898 | `	return -1;   /* neither: the caller words php's TypeError */` |
|      13 | 1899 | `}` |
|       - | 1900 | `/*` |
|       - | 1901 | ` * PhpToken::is(int\|string\|array $kind): bool` |
|       - | 1902 | ` *` |
|       - | 1903 | ` * php screens the argument ITSELF (the parameter is untyped, so the shared ZPP` |
|       - | 1904 | ` * cannot), and words two different refusals: one for the argument, one for an` |
|       - | 1905 | ` * ELEMENT of an array argument. The embedded PHP screened neither and answered` |
|       - | 1906 | ` * false for a float, a null and a bad element alike.` |
|       - | 1907 | ` */` |
|      22 | 1908 | `static int vm_builtin_PhpToken_is(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1909 | `{` |
|      23 | 1910 | `	ph7_value *pKind = nArg > 0 ? apArg[0] : 0;` |
|      23 | 1911 | `	int bYes = 0;` |
|       - | 1912 | `	sxi32 rc;` |
|      23 | 1913 | `	if( pKind == 0 ){` |
|     ! 0 | 1914 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1915 | `		return PH7_OK;` |
|       - | 1916 | `	}` |
|      23 | 1917 | `	if( pKind->iFlags & MEMOBJ_HASHMAP ){` |
|      11 | 1918 | `		ph7_hashmap *pMap = (ph7_hashmap *)pKind->x.pOther;` |
|       - | 1919 | `		ph7_hashmap_node *pNode;` |
|       - | 1920 | `		sxu32 n;` |
|       - | 1921 | `		/* Insertion order is pFirst then the pPrev chain (rule 12), walked` |
|       - | 1922 | `		 * directly rather than through the shared loop cursor -- this is the` |
|       - | 1923 | `		 * CALLER's array and php's iteration does not move its pointer. */` |
|      17 | 1924 | `		for( n = 0, pNode = pMap->pFirst ; pNode && n < pMap->nEntry ;` |
|       7 | 1925 | `			 ++n, pNode = pNode->pPrev ){` |
|       - | 1926 | `			ph7_value sVal;` |
|       - | 1927 | `			int iRc;` |
|      13 | 1928 | `			PH7_MemObjInit(pCtx->pVm,&sVal);` |
|      13 | 1929 | `			PH7_HashmapExtractNodeValue(pNode,&sVal,FALSE);` |
|      13 | 1930 | `			iRc = TokMatch(pCtx,&sVal,&bYes,&rc);` |
|      13 | 1931 | `			if( iRc < 0 ){` |
|       - | 1932 | `				char zGiven[64];` |
|     ! 0 | 1933 | `				sxi32 rcT = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1934 | `					"PhpToken::is(): Argument #1 ($kind) must only have elements of type "` |
|     ! 0 | 1935 | `					"string\|int, %s given",VmValueGivenName(&sVal,zGiven,sizeof(zGiven)));` |
|     ! 0 | 1936 | `				PH7_MemObjRelease(&sVal);` |
|     ! 0 | 1937 | `				return rcT;` |
|       - | 1938 | `			}` |
|      13 | 1939 | `			PH7_MemObjRelease(&sVal);` |
|      13 | 1940 | `			if( iRc == 0 ){` |
|     ! 0 | 1941 | `				return rc;` |
|       - | 1942 | `			}` |
|      13 | 1943 | `			if( bYes ){` |
|       7 | 1944 | `				ph7_result_bool(pCtx,1);` |
|       7 | 1945 | `				return PH7_OK;` |
|       - | 1946 | `			}` |
|       4 | 1947 | `		}` |
|       5 | 1948 | `		ph7_result_bool(pCtx,0);` |
|       5 | 1949 | `		return PH7_OK;` |
|       - | 1950 | `	}` |
|       - | 1951 | `	{` |
|       - | 1952 | `		char zGiven[64];` |
|      13 | 1953 | `		int iRc = TokMatch(pCtx,pKind,&bYes,&rc);` |
|      13 | 1954 | `		if( iRc < 0 ){` |
|      11 | 1955 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1956 | `				"PhpToken::is(): Argument #1 ($kind) must be of type string\|int\|array, %s given",` |
|       3 | 1957 | `				VmValueGivenName(pKind,zGiven,sizeof(zGiven)));` |
|       - | 1958 | `		}` |
|       7 | 1959 | `		if( iRc == 0 ){` |
|       3 | 1960 | `			return rc;` |
|       - | 1961 | `		}` |
|       - | 1962 | `	}` |
|       5 | 1963 | `	ph7_result_bool(pCtx,bYes);` |
|       5 | 1964 | `	return PH7_OK;` |
|      12 | 1965 | `}` |
|       - | 1966 | `/* PhpToken::isIgnorable(): bool — php's four "not part of the program" ids. */` |
|       2 | 1967 | `static int vm_builtin_PhpToken_isIgnorable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1968 | `{` |
|       - | 1969 | `	sxi32 rc;` |
|       3 | 1970 | `	ph7_value *pSlot = TokSlot(pCtx,"id",&rc);` |
|       - | 1971 | `	sxi64 iId;` |
|       1 | 1972 | `	SXUNUSED(nArg);` |
|       1 | 1973 | `	SXUNUSED(apArg);` |
|       3 | 1974 | `	if( pSlot == 0 ){` |
|       3 | 1975 | `		return rc;` |
|       - | 1976 | `	}` |
|     ! 0 | 1977 | `	iId = ph7_value_to_int64(pSlot);` |
|     ! 0 | 1978 | `	ph7_result_bool(pCtx,iId == T_WHITESPACE \|\| iId == T_COMMENT` |
|     ! 0 | 1979 | `		\|\| iId == T_DOC_COMMENT \|\| iId == T_OPEN_TAG);` |
|     ! 0 | 1980 | `	return PH7_OK;` |
|       2 | 1981 | `}` |
|       - | 1982 | `/* PhpToken::getTokenName(): ?string — the CHARACTER for a single-byte token,` |
|       - | 1983 | ` * the T_* name for a known id, and null for anything else. */` |
|      36 | 1984 | `static int vm_builtin_PhpToken_getTokenName(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1985 | `{` |
|       - | 1986 | `	sxi32 rc;` |
|      38 | 1987 | `	ph7_value *pSlot = TokSlot(pCtx,"id",&rc);` |
|       - | 1988 | `	const char *zName;` |
|       - | 1989 | `	sxi64 iId;` |
|      18 | 1990 | `	SXUNUSED(nArg);` |
|      18 | 1991 | `	SXUNUSED(apArg);` |
|      38 | 1992 | `	if( pSlot == 0 ){` |
|       3 | 1993 | `		return rc;` |
|       - | 1994 | `	}` |
|      36 | 1995 | `	iId = ph7_value_to_int64(pSlot);` |
|      36 | 1996 | `	if( iId < 256 ){` |
|      14 | 1997 | `		char c = (char)iId;` |
|      14 | 1998 | `		ph7_result_string(pCtx,&c,1);` |
|      14 | 1999 | `		return PH7_OK;` |
|       - | 2000 | `	}` |
|      24 | 2001 | `	zName = TokConstName((int)iId);` |
|      24 | 2002 | `	if( zName == 0 ){` |
|     ! 0 | 2003 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2004 | `		return PH7_OK;` |
|       - | 2005 | `	}` |
|      24 | 2006 | `	ph7_result_string(pCtx,zName,-1/*SyStrlen*/);` |
|      24 | 2007 | `	return PH7_OK;` |
|      20 | 2008 | `}` |
|       2 | 2009 | `static int vm_builtin_PhpToken_toString(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2010 | `{` |
|       - | 2011 | `	sxi32 rc;` |
|       3 | 2012 | `	ph7_value *pSlot = TokSlot(pCtx,"text",&rc);` |
|       - | 2013 | `	const char *zText;` |
|       3 | 2014 | `	int nText = 0;` |
|       1 | 2015 | `	SXUNUSED(nArg);` |
|       1 | 2016 | `	SXUNUSED(apArg);` |
|       3 | 2017 | `	if( pSlot == 0 ){` |
|       3 | 2018 | `		return rc;` |
|       - | 2019 | `	}` |
|     ! 0 | 2020 | `	zText = ph7_value_to_string(pSlot,&nText);` |
|     ! 0 | 2021 | `	ph7_result_string(pCtx,zText,nText);` |
|     ! 0 | 2022 | `	return PH7_OK;` |
|       2 | 2023 | `}` |
|       - | 2024 | `/*` |
|       - | 2025 | ` * The declaration. Method ORDER is tokenizer.stub.php's — tokenize() first,` |
|       - | 2026 | ` * then the final constructor — and the four properties are declared with NO` |
|       - | 2027 | ` * default, which is what makes them php's uninitialized typed slots.` |
|       - | 2028 | ` */` |
|    6721 | 2029 | `static sxi32 VmInstallPhpToken(ph7_vm *pVm)` |
|       5 | 2030 | `{` |
|       - | 2031 | `	static const PH7_NativePropDef aTokProp[] = {` |
|       - | 2032 | `		{ "id",   PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|       - | 2033 | `		{ "text", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|       - | 2034 | `		{ "line", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|       - | 2035 | `		{ "pos",  PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|       - | 2036 | `	};` |
|       - | 2037 | `	static const PH7_NativeMethodDef aTokMethod[] = {` |
|       - | 2038 | `		{ "tokenize",     PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "string $code, int $flags = 0", "array",` |
|       - | 2039 | `		  vm_builtin_PhpToken_tokenize },` |
|       - | 2040 | `		{ "__construct",  PH7_MOD_PUBLIC\|PH7_MOD_FINAL,` |
|       - | 2041 | `		  "int $id, string $text, int $line = -1, int $pos = -1", 0,` |
|       - | 2042 | `		  vm_builtin_PhpToken_construct },` |
|       - | 2043 | `		{ "is",           PH7_MOD_PUBLIC, "$kind", "bool", vm_builtin_PhpToken_is },` |
|       - | 2044 | `		{ "isIgnorable",  PH7_MOD_PUBLIC, "", "bool", vm_builtin_PhpToken_isIgnorable },` |
|       - | 2045 | `		{ "getTokenName", PH7_MOD_PUBLIC, "", "?string", vm_builtin_PhpToken_getTokenName },` |
|       - | 2046 | `		{ "__toString",   PH7_MOD_PUBLIC, "", "string", vm_builtin_PhpToken_toString },` |
|       - | 2047 | `	};` |
|       - | 2048 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|       - | 2049 | `		{ "PhpToken", 0, "Stringable", 0,` |
|       - | 2050 | `		  aTokMethod, SX_ARRAYSIZE(aTokMethod), 0, 0,` |
|       - | 2051 | `		  aTokProp, SX_ARRAYSIZE(aTokProp), 0, 0, 0 },` |
|       - | 2052 | `	};` |
|    6726 | 2053 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|       5 | 2054 | `}` |
|       - | 2055 |  |
|    6726 | 2056 | `PH7_PRIVATE sxi32 PH7_VmInstallTokenizer(ph7_vm *pVm){` |
|    6726 | 2057 | `	ph7_create_function(&(*pVm),"token_get_all",PH7_builtin_token_get_all,0);` |
|    6726 | 2058 | `	ph7_create_function(&(*pVm),"php_strip_whitespace",PH7_builtin_php_strip_whitespace,0);` |
|    6726 | 2059 | `	ph7_create_function(&(*pVm),"token_name",PH7_builtin_token_name,0);` |
|    6726 | 2060 | `	return VmInstallPhpToken(&(*pVm));` |
|       5 | 2061 | `}` |
|       - | 2062 |  |
|       - | 2063 | `#else /* PH7_DISABLE_BUILTIN_FUNC */` |
|       - | 2064 |  |
|       - | 2065 | `/* Tiny build: no tokenizer builtins/class (the whole builtin layer is off). */` |
|       - | 2066 | `PH7_PRIVATE sxi32 PH7_VmInstallTokenizer(ph7_vm *pVm){ (void)pVm; return SXRET_OK; }` |
|       - | 2067 |  |
|       - | 2068 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|       - | 2069 |  |
