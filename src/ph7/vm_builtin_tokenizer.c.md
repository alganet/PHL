# src/ph7/vm_builtin_tokenizer.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 12/743 lines (1.62%)

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
|   3418 |  254 | `PH7_PRIVATE void PH7_RegisterTokenizerConstants(ph7_vm *pVm)` |
|      5 |  255 | `{` |
|      - |  256 | `	sxu32 n;` |
| 529795 |  257 | `	for( n = 0 ; n < SX_ARRAYSIZE(aTokConst) ; ++n ){` |
| 789563 |  258 | `		ph7_create_constant(&(*pVm),aTokConst[n].zName,TokConstExpand,` |
| 526372 |  259 | `			SX_INT_TO_PTR(aTokConst[n].iId));` |
| 263191 |  260 | `	}` |
|   3423 |  261 | `}` |
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
|      - |  326 | `	int          bProp;    /* one-shot: next identifier is a member name -> T_STRING */` |
|      - |  327 | `	int          bCaseName;/* one-shot: next identifier may be an enum case name (after 'case') */` |
|      - |  328 | `	int          bParse;   /* TOKEN_PARSE flag: apply php's semi-reserved re-tagging */` |
|      - |  329 | `	int          bStop;    /* __halt_compiler seen: stop scanning entirely */` |
|      - |  330 | `	int          bOOM;     /* memory failure flag */` |
|      - |  331 | `};` |
|      - |  332 |  |
|      - |  333 | `/* --- character classes (byte-level, UTF-8 lead bytes count as label chars) --- */` |
|    ! 0 |  334 | `static int tok_is_label_start(int c){` |
|    ! 0 |  335 | `	return c=='_' \|\| (c>='a'&&c<='z') \|\| (c>='A'&&c<='Z') \|\| c>=0x80;` |
|    ! 0 |  336 | `}` |
|    ! 0 |  337 | `static int tok_is_label(int c){` |
|    ! 0 |  338 | `	return tok_is_label_start(c) \|\| (c>='0'&&c<='9');` |
|    ! 0 |  339 | `}` |
|    ! 0 |  340 | `static int tok_is_ws(int c){` |
|      - |  341 | `	/* php's tokenizer whitespace is exactly [ \t\r\n]; \v and \f are T_BAD_CHARACTER. */` |
|    ! 0 |  342 | `	return c==' '\|\|c=='\t'\|\|c=='\n'\|\|c=='\r';` |
|    ! 0 |  343 | `}` |
|    ! 0 |  344 | `static int tok_lower(int c){` |
|    ! 0 |  345 | `	return (c>='A'&&c<='Z') ? c+32 : c;` |
|    ! 0 |  346 | `}` |
|    ! 0 |  347 | `static int tok_ci_eq(const char *z,int n,const char *zKw){` |
|      - |  348 | `	int i;` |
|    ! 0 |  349 | `	for( i = 0 ; i < n ; ++i ){` |
|    ! 0 |  350 | `		if( zKw[i]==0 \|\| tok_lower((unsigned char)z[i]) != (unsigned char)zKw[i] ){` |
|    ! 0 |  351 | `			return 0;` |
|      - |  352 | `		}` |
|    ! 0 |  353 | `	}` |
|    ! 0 |  354 | `	return zKw[n]==0;` |
|    ! 0 |  355 | `}` |
|      - |  356 |  |
|      - |  357 | `/* php counts a line ending as any of "\n", "\r", or "\r\n" (each = one line). */` |
|    ! 0 |  358 | `static int tok_at_nl(const unsigned char *p,const unsigned char *zEnd){` |
|    ! 0 |  359 | `	if( *p=='\n' ){ return 1; }` |
|    ! 0 |  360 | `	if( *p=='\r' && (p+1 >= zEnd \|\| p[1]!='\n') ){ return 1; }` |
|    ! 0 |  361 | `	return 0;` |
|    ! 0 |  362 | `}` |
|      - |  363 |  |
|      - |  364 | `/* Count line endings in [z,zEnd) and advance the running line counter. */` |
|    ! 0 |  365 | `static void tok_bump_lines(tok_state *ts,const unsigned char *z,const unsigned char *zEnd){` |
|    ! 0 |  366 | `	while( z < zEnd ){` |
|    ! 0 |  367 | `		if( tok_at_nl(z,zEnd) ){ ts->iLine++; }` |
|    ! 0 |  368 | `		z++;` |
|    ! 0 |  369 | `	}` |
|    ! 0 |  370 | `}` |
|      - |  371 |  |
|      - |  372 | `/* Emit a plain-string token (single-char / operator returned as a bare string). */` |
|    ! 0 |  373 | `static void tok_plain(tok_state *ts,const char *z,int n){` |
|    ! 0 |  374 | `	if( ts->bOOM ){ return; }` |
|    ! 0 |  375 | `	ph7_value_string(ts->pS,z,n);` |
|    ! 0 |  376 | `	if( ph7_array_add_elem(ts->pArray,0,ts->pS) != SXRET_OK ){ ts->bOOM = 1; }` |
|    ! 0 |  377 | `	ph7_value_reset_string_cursor(ts->pS);` |
|    ! 0 |  378 | `}` |
|      - |  379 |  |
|      - |  380 | `/* Emit a [id, text, line] token. */` |
|    ! 0 |  381 | `static void tok_tok(tok_state *ts,int iId,const char *z,int n,int iLine){` |
|      - |  382 | `	ph7_value *pInner;` |
|    ! 0 |  383 | `	if( ts->bOOM ){ return; }` |
|    ! 0 |  384 | `	pInner = ph7_context_new_array(ts->pCtx);` |
|    ! 0 |  385 | `	if( pInner == 0 ){ ts->bOOM = 1; return; }` |
|    ! 0 |  386 | `	ph7_value_int(ts->pId,iId);` |
|    ! 0 |  387 | `	ph7_value_string(ts->pText,z,n);` |
|    ! 0 |  388 | `	ph7_value_int(ts->pLine,iLine);` |
|    ! 0 |  389 | `	ph7_array_add_elem(pInner,0,ts->pId);` |
|    ! 0 |  390 | `	ph7_array_add_elem(pInner,0,ts->pText);` |
|    ! 0 |  391 | `	ph7_array_add_elem(pInner,0,ts->pLine);` |
|    ! 0 |  392 | `	if( ph7_array_add_elem(ts->pArray,0,pInner) != SXRET_OK ){ ts->bOOM = 1; }` |
|    ! 0 |  393 | `	ph7_context_release_value(ts->pCtx,pInner);` |
|    ! 0 |  394 | `	ph7_value_reset_string_cursor(ts->pText);` |
|    ! 0 |  395 | `}` |
|      - |  396 |  |
|      - |  397 | `/* Emit an encapsed-and-whitespace run [zStart, z) if non-empty, at iLine. */` |
|    ! 0 |  398 | `static void tok_encaps(tok_state *ts,const unsigned char *zStart,const unsigned char *z,int iLine){` |
|    ! 0 |  399 | `	if( z > zStart ){` |
|    ! 0 |  400 | `		tok_tok(ts,T_ENCAPSED_AND_WHITESPACE,(const char *)zStart,(int)(z-zStart),iLine);` |
|    ! 0 |  401 | `	}` |
|    ! 0 |  402 | `}` |
|      - |  403 |  |
|      - |  404 | `/* Does the integer literal (digits between z and zEnd, given radix) overflow` |
|      - |  405 | ` * ZEND_LONG_MAX? Underscores are ignored. */` |
|    ! 0 |  406 | `static int tok_int_overflows(const unsigned char *z,const unsigned char *zEnd,int radix){` |
|    ! 0 |  407 | `	sxu64 val = 0;` |
|    ! 0 |  408 | `	const sxu64 max = (sxu64)0x7FFFFFFFFFFFFFFF; /* PHP_INT_MAX */` |
|    ! 0 |  409 | `	while( z < zEnd ){` |
|    ! 0 |  410 | `		int c = *z++;` |
|      - |  411 | `		int d;` |
|    ! 0 |  412 | `		if( c=='_' ){ continue; }` |
|    ! 0 |  413 | `		if( c>='0'&&c<='9' ){ d = c-'0'; }` |
|    ! 0 |  414 | `		else if( c>='a'&&c<='f' ){ d = c-'a'+10; }` |
|    ! 0 |  415 | `		else if( c>='A'&&c<='F' ){ d = c-'A'+10; }` |
|    ! 0 |  416 | `		else { break; }` |
|    ! 0 |  417 | `		if( d >= radix ){ break; }` |
|    ! 0 |  418 | `		if( val > (max - (sxu64)d)/(sxu64)radix ){` |
|    ! 0 |  419 | `			return 1;` |
|      - |  420 | `		}` |
|    ! 0 |  421 | `		val = val*(sxu64)radix + (sxu64)d;` |
|    ! 0 |  422 | `	}` |
|    ! 0 |  423 | `	return 0;` |
|    ! 0 |  424 | `}` |
|      - |  425 |  |
|      - |  426 | `/* Forward decls for the mutually-recursive string / expression scanners. */` |
|      - |  427 | `static int  tok_lex_one(tok_state *ts);` |
|      - |  428 | `static void tok_scan_curly(tok_state *ts,int bVarname);` |
|      - |  429 |  |
|      - |  430 | `/*` |
|      - |  431 | ` * After a T_HALT_COMPILER token: php emits the "();" that follows as normal` |
|      - |  432 | ` * tokens and then the entire remainder of the source as a single T_INLINE_HTML.` |
|      - |  433 | ` */` |
|    ! 0 |  434 | `static void tok_halt_tail(tok_state *ts){` |
|    ! 0 |  435 | `	for(;;){` |
|    ! 0 |  436 | `		if( ts->z >= ts->zEnd ){ break; }` |
|    ! 0 |  437 | `		if( tok_is_ws(*ts->z) ){` |
|    ! 0 |  438 | `			const unsigned char *z0 = ts->z;` |
|    ! 0 |  439 | `			int iLine = ts->iLine;` |
|    ! 0 |  440 | `			while( ts->z < ts->zEnd && tok_is_ws(*ts->z) ){` |
|    ! 0 |  441 | `				if( tok_at_nl(ts->z,ts->zEnd) ){ ts->iLine++; }` |
|    ! 0 |  442 | `				ts->z++;` |
|    ! 0 |  443 | `			}` |
|    ! 0 |  444 | `			tok_tok(ts,T_WHITESPACE,(const char *)z0,(int)(ts->z-z0),iLine);` |
|    ! 0 |  445 | `			continue;` |
|      - |  446 | `		}` |
|    ! 0 |  447 | `		if( *ts->z=='(' \|\| *ts->z==')' ){` |
|    ! 0 |  448 | `			char ch = (char)*ts->z;` |
|    ! 0 |  449 | `			tok_plain(ts,&ch,1);` |
|    ! 0 |  450 | `			ts->z++;` |
|    ! 0 |  451 | `			continue;` |
|      - |  452 | `		}` |
|    ! 0 |  453 | `		if( *ts->z==';' ){` |
|    ! 0 |  454 | `			tok_plain(ts,";",1);` |
|    ! 0 |  455 | `			ts->z++;` |
|    ! 0 |  456 | `			break;` |
|      - |  457 | `		}` |
|    ! 0 |  458 | `		break;` |
|    ! 0 |  459 | `	}` |
|    ! 0 |  460 | `	if( ts->z < ts->zEnd ){` |
|    ! 0 |  461 | `		int iLine = ts->iLine;` |
|    ! 0 |  462 | `		tok_tok(ts,T_INLINE_HTML,(const char *)ts->z,(int)(ts->zEnd-ts->z),iLine);` |
|    ! 0 |  463 | `		tok_bump_lines(ts,ts->z,ts->zEnd);` |
|    ! 0 |  464 | `		ts->z = ts->zEnd;` |
|    ! 0 |  465 | `	}` |
|    ! 0 |  466 | `	ts->bStop = 1;` |
|    ! 0 |  467 | `}` |
|      - |  468 |  |
|      - |  469 | `/*` |
|      - |  470 | ` * Handle a possible interpolation construct at the cursor inside a double-quoted` |
|      - |  471 | ` * string or heredoc body. Returns 1 if it consumed one (emitting tokens), else 0` |
|      - |  472 | ` * (the caller keeps the char as literal). *pFlush is the start of the pending` |
|      - |  473 | ` * literal run and its line; on a hit we flush it first.` |
|      - |  474 | ` */` |
|    ! 0 |  475 | `static int tok_try_interp(tok_state *ts,const unsigned char **pLitStart,int *pLitLine){` |
|    ! 0 |  476 | `	const unsigned char *z = ts->z;` |
|    ! 0 |  477 | `	if( *z=='$' && z+1 < ts->zEnd && tok_is_label_start(z[1]) ){` |
|    ! 0 |  478 | `		const unsigned char *zVar = z+1;` |
|    ! 0 |  479 | `		int iLine = ts->iLine;` |
|    ! 0 |  480 | `		while( zVar < ts->zEnd && tok_is_label(*zVar) ){ zVar++; }` |
|    ! 0 |  481 | `		tok_encaps(ts,*pLitStart,z,*pLitLine);` |
|    ! 0 |  482 | `		tok_tok(ts,T_VARIABLE,(const char *)z,(int)(zVar-z),iLine);` |
|    ! 0 |  483 | `		ts->z = zVar;` |
|      - |  484 | `		/* one optional simple offset [...] or ->prop */` |
|    ! 0 |  485 | `		if( ts->z < ts->zEnd && *ts->z=='[' ){` |
|    ! 0 |  486 | `			tok_plain(ts,"[",1);` |
|    ! 0 |  487 | `			ts->z++;` |
|    ! 0 |  488 | `			if( ts->z < ts->zEnd && *ts->z=='$' && ts->z+1 < ts->zEnd && tok_is_label_start(ts->z[1]) ){` |
|    ! 0 |  489 | `				const unsigned char *v = ts->z+1;` |
|    ! 0 |  490 | `				while( v < ts->zEnd && tok_is_label(*v) ){ v++; }` |
|    ! 0 |  491 | `				tok_tok(ts,T_VARIABLE,(const char *)ts->z,(int)(v-ts->z),ts->iLine);` |
|    ! 0 |  492 | `				ts->z = v;` |
|    ! 0 |  493 | `			}else{` |
|    ! 0 |  494 | `				const unsigned char *n0 = ts->z;` |
|    ! 0 |  495 | `				if( ts->z < ts->zEnd && *ts->z=='-' ){` |
|    ! 0 |  496 | `					tok_plain(ts,"-",1);` |
|    ! 0 |  497 | `					ts->z++;` |
|    ! 0 |  498 | `					n0 = ts->z;` |
|    ! 0 |  499 | `				}` |
|    ! 0 |  500 | `				if( ts->z < ts->zEnd && *ts->z>='0' && *ts->z<='9' ){` |
|    ! 0 |  501 | `					while( ts->z < ts->zEnd && *ts->z>='0' && *ts->z<='9' ){ ts->z++; }` |
|    ! 0 |  502 | `					tok_tok(ts,T_NUM_STRING,(const char *)n0,(int)(ts->z-n0),ts->iLine);` |
|    ! 0 |  503 | `				}else if( ts->z < ts->zEnd && tok_is_label_start(*ts->z) ){` |
|    ! 0 |  504 | `					const unsigned char *l = ts->z;` |
|    ! 0 |  505 | `					while( l < ts->zEnd && tok_is_label(*l) ){ l++; }` |
|    ! 0 |  506 | `					tok_tok(ts,T_STRING,(const char *)ts->z,(int)(l-ts->z),ts->iLine);` |
|    ! 0 |  507 | `					ts->z = l;` |
|    ! 0 |  508 | `				}` |
|      - |  509 | `			}` |
|    ! 0 |  510 | `			if( ts->z < ts->zEnd && *ts->z==']' ){ tok_plain(ts,"]",1); ts->z++; }` |
|    ! 0 |  511 | `		}else if( ts->z+2 < ts->zEnd && ts->z[0]=='-' && ts->z[1]=='>' && tok_is_label_start(ts->z[2]) ){` |
|      - |  512 | `			const unsigned char *l;` |
|    ! 0 |  513 | `			tok_tok(ts,T_OBJECT_OPERATOR,"->",2,ts->iLine);` |
|    ! 0 |  514 | `			ts->z += 2;` |
|    ! 0 |  515 | `			l = ts->z;` |
|    ! 0 |  516 | `			while( l < ts->zEnd && tok_is_label(*l) ){ l++; }` |
|    ! 0 |  517 | `			tok_tok(ts,T_STRING,(const char *)ts->z,(int)(l-ts->z),ts->iLine);` |
|    ! 0 |  518 | `			ts->z = l;` |
|    ! 0 |  519 | `		}` |
|    ! 0 |  520 | `		*pLitStart = ts->z;` |
|    ! 0 |  521 | `		*pLitLine  = ts->iLine;` |
|    ! 0 |  522 | `		return 1;` |
|      - |  523 | `	}` |
|    ! 0 |  524 | `	if( *z=='{' && z+1 < ts->zEnd && z[1]=='$' ){` |
|    ! 0 |  525 | `		tok_encaps(ts,*pLitStart,z,*pLitLine);` |
|    ! 0 |  526 | `		tok_tok(ts,T_CURLY_OPEN,"{",1,ts->iLine);` |
|    ! 0 |  527 | `		ts->z = z+1;` |
|    ! 0 |  528 | `		tok_scan_curly(ts,0);` |
|    ! 0 |  529 | `		*pLitStart = ts->z;` |
|    ! 0 |  530 | `		*pLitLine  = ts->iLine;` |
|    ! 0 |  531 | `		return 1;` |
|      - |  532 | `	}` |
|    ! 0 |  533 | `	if( *z=='$' && z+1 < ts->zEnd && z[1]=='{' ){` |
|    ! 0 |  534 | `		tok_encaps(ts,*pLitStart,z,*pLitLine);` |
|    ! 0 |  535 | `		tok_tok(ts,T_DOLLAR_OPEN_CURLY_BRACES,"${",2,ts->iLine);` |
|    ! 0 |  536 | `		ts->z = z+2;` |
|    ! 0 |  537 | `		tok_scan_curly(ts,1);` |
|    ! 0 |  538 | `		*pLitStart = ts->z;` |
|    ! 0 |  539 | `		*pLitLine  = ts->iLine;` |
|    ! 0 |  540 | `		return 1;` |
|      - |  541 | `	}` |
|    ! 0 |  542 | `	return 0;` |
|    ! 0 |  543 | `}` |
|      - |  544 |  |
|      - |  545 | `/*` |
|      - |  546 | ` * Scan a brace-delimited expression inside a string ("{$...}" or "${...}").` |
|      - |  547 | ` * The opening brace token was already emitted and ts->z points just past it.` |
|      - |  548 | ` * Emits PHP tokens until the matching close brace, which it emits as a plain` |
|      - |  549 | ` * "}" string token. bVarname: the first label (if immediately followed by '}'` |
|      - |  550 | ` * or '[') is a T_STRING_VARNAME (the "${name}" form).` |
|      - |  551 | ` */` |
|    ! 0 |  552 | `static void tok_scan_curly(tok_state *ts,int bVarname){` |
|    ! 0 |  553 | `	int depth = 1;` |
|    ! 0 |  554 | `	if( bVarname && ts->z < ts->zEnd && tok_is_label_start(*ts->z) ){` |
|    ! 0 |  555 | `		const unsigned char *l = ts->z;` |
|    ! 0 |  556 | `		while( l < ts->zEnd && tok_is_label(*l) ){ l++; }` |
|    ! 0 |  557 | `		if( l < ts->zEnd && (*l=='}' \|\| *l=='[') ){` |
|    ! 0 |  558 | `			tok_tok(ts,T_STRING_VARNAME,(const char *)ts->z,(int)(l-ts->z),ts->iLine);` |
|    ! 0 |  559 | `			ts->z = l;` |
|    ! 0 |  560 | `		}` |
|    ! 0 |  561 | `	}` |
|    ! 0 |  562 | `	while( ts->z < ts->zEnd && depth > 0 && !ts->bOOM ){` |
|      - |  563 | `		int eff;` |
|    ! 0 |  564 | `		if( *ts->z=='}' ){` |
|    ! 0 |  565 | `			depth--;` |
|    ! 0 |  566 | `			if( depth == 0 ){` |
|    ! 0 |  567 | `				tok_plain(ts,"}",1);` |
|    ! 0 |  568 | `				ts->z++;` |
|    ! 0 |  569 | `				return;` |
|      - |  570 | `			}` |
|    ! 0 |  571 | `		}` |
|    ! 0 |  572 | `		eff = tok_lex_one(ts);` |
|    ! 0 |  573 | `		if( eff == '{' ){ depth++; }` |
|    ! 0 |  574 | `		else if( eff == '}' ){ depth--; if( depth==0 ){ return; } }` |
|    ! 0 |  575 | `		else if( eff < 0 ){ return; } /* close tag / EOF safety */` |
|    ! 0 |  576 | `	}` |
|    ! 0 |  577 | `}` |
|      - |  578 |  |
|      - |  579 | `/*` |
|      - |  580 | ` * Scan a double-quoted string or backtick string starting at the opening` |
|      - |  581 | ` * delimiter (ts->z points at it). If double-quoted with no interpolation the` |
|      - |  582 | ` * whole literal is one T_CONSTANT_ENCAPSED_STRING; otherwise it splits into the` |
|      - |  583 | ` * delimiter, encapsed runs and interpolation tokens.` |
|      - |  584 | ` */` |
|    ! 0 |  585 | `static void tok_scan_dquote(tok_state *ts,int chDelim){` |
|    ! 0 |  586 | `	const unsigned char *zOpen = ts->z;` |
|    ! 0 |  587 | `	int iOpenLine = ts->iLine;` |
|    ! 0 |  588 | `	const unsigned char *zScan = ts->z+1;` |
|    ! 0 |  589 | `	int bInterp = 0;` |
|    ! 0 |  590 | `	int bClosed = 0;` |
|      - |  591 | `	/* Peek for interpolation to decide constant-vs-split (only for '"'). */` |
|    ! 0 |  592 | `	while( zScan < ts->zEnd ){` |
|    ! 0 |  593 | `		int c = *zScan;` |
|    ! 0 |  594 | `		if( c=='\\' ){ zScan += 2; continue; }` |
|    ! 0 |  595 | `		if( c==chDelim ){ bClosed = 1; break; }` |
|    ! 0 |  596 | `		if( c=='$' && zScan+1 < ts->zEnd && (tok_is_label_start(zScan[1])\|\|zScan[1]=='{') ){ bInterp = 1; break; }` |
|    ! 0 |  597 | `		if( c=='{' && zScan+1 < ts->zEnd && zScan[1]=='$' ){ bInterp = 1; break; }` |
|    ! 0 |  598 | `		zScan++;` |
|    ! 0 |  599 | `	}` |
|    ! 0 |  600 | `	if( chDelim=='"' && !bInterp && bClosed ){` |
|      - |  601 | `		/* Whole constant string. Find the real closing quote. */` |
|    ! 0 |  602 | `		const unsigned char *z = ts->z+1;` |
|    ! 0 |  603 | `		while( z < ts->zEnd ){` |
|    ! 0 |  604 | `			if( *z=='\\' && z+1 < ts->zEnd ){ z += 2; continue; }` |
|    ! 0 |  605 | `			if( *z=='"' ){ break; }` |
|    ! 0 |  606 | `			z++;` |
|    ! 0 |  607 | `		}` |
|    ! 0 |  608 | `		if( z < ts->zEnd ){ z++; } /* include closing quote */` |
|    ! 0 |  609 | `		tok_tok(ts,T_CONSTANT_ENCAPSED_STRING,(const char *)zOpen,(int)(z-zOpen),iOpenLine);` |
|    ! 0 |  610 | `		tok_bump_lines(ts,ts->z,z);` |
|    ! 0 |  611 | `		ts->z = z;` |
|    ! 0 |  612 | `		return;` |
|      - |  613 | `	}` |
|      - |  614 | `	/* Split form: opening delimiter, then body, then closing delimiter. */` |
|      - |  615 | `	{` |
|    ! 0 |  616 | `		char d = (char)chDelim;` |
|      - |  617 | `		const unsigned char *litStart;` |
|      - |  618 | `		int litLine;` |
|    ! 0 |  619 | `		tok_plain(ts,&d,1);` |
|    ! 0 |  620 | `		ts->z++;` |
|    ! 0 |  621 | `		litStart = ts->z;` |
|    ! 0 |  622 | `		litLine  = ts->iLine;` |
|    ! 0 |  623 | `		while( ts->z < ts->zEnd && !ts->bOOM ){` |
|    ! 0 |  624 | `			int c = *ts->z;` |
|    ! 0 |  625 | `			if( c=='\\' && ts->z+1 < ts->zEnd ){` |
|    ! 0 |  626 | `				if( ts->z[1]=='\n' ){ ts->iLine++; } /* escaped newline still advances the line */` |
|    ! 0 |  627 | `				ts->z += 2;` |
|    ! 0 |  628 | `				continue;` |
|      - |  629 | `			}` |
|    ! 0 |  630 | `			if( c==chDelim ){` |
|    ! 0 |  631 | `				tok_encaps(ts,litStart,ts->z,litLine);` |
|    ! 0 |  632 | `				tok_plain(ts,&d,1);` |
|    ! 0 |  633 | `				ts->z++;` |
|    ! 0 |  634 | `				return;` |
|      - |  635 | `			}` |
|    ! 0 |  636 | `			if( c=='$' \|\| c=='{' ){` |
|    ! 0 |  637 | `				if( tok_try_interp(ts,&litStart,&litLine) ){ continue; }` |
|    ! 0 |  638 | `			}` |
|    ! 0 |  639 | `			if( tok_at_nl(ts->z,ts->zEnd) ){ ts->iLine++; }` |
|    ! 0 |  640 | `			ts->z++;` |
|    ! 0 |  641 | `		}` |
|      - |  642 | `		/* Unterminated: flush what we have. */` |
|    ! 0 |  643 | `		tok_encaps(ts,litStart,ts->z,litLine);` |
|      - |  644 | `	}` |
|    ! 0 |  645 | `}` |
|      - |  646 |  |
|      - |  647 | `/* Does the line at z (already at line start) begin the heredoc closing marker` |
|      - |  648 | ` * for zLabel? Returns the end of the label (past it) if so, else 0. */` |
|    ! 0 |  649 | `static const unsigned char * tok_heredoc_close(tok_state *ts,const unsigned char *z,` |
|    ! 0 |  650 | `		const char *zLabel,int nLabel){` |
|    ! 0 |  651 | `	const unsigned char *p = z;` |
|      - |  652 | `	int i;` |
|    ! 0 |  653 | `	while( p < ts->zEnd && (*p==' '\|\|*p=='\t') ){ p++; }` |
|    ! 0 |  654 | `	if( p + nLabel > ts->zEnd ){ return 0; } /* not enough bytes left for the label */` |
|    ! 0 |  655 | `	for( i = 0 ; i < nLabel ; ++i ){` |
|    ! 0 |  656 | `		if( p[i] != (unsigned char)zLabel[i] ){ return 0; }` |
|    ! 0 |  657 | `	}` |
|    ! 0 |  658 | `	p += nLabel;` |
|    ! 0 |  659 | `	if( p < ts->zEnd && tok_is_label(*p) ){ return 0; } /* label is a prefix of a longer word */` |
|    ! 0 |  660 | `	return p;` |
|    ! 0 |  661 | `}` |
|      - |  662 |  |
|      - |  663 | `/*` |
|      - |  664 | ` * Scan a heredoc/nowdoc starting at "<<<". ts->z points at the first '<'.` |
|      - |  665 | ` * Returns 1 if a valid heredoc was consumed, 0 if this "<<<" is not a valid` |
|      - |  666 | ` * heredoc start (the caller then falls through to operator lexing: T_SL + '<').` |
|      - |  667 | ` * A valid start is: "<<<" [ws]* ["\|']? LABEL ["\|']? immediately followed by` |
|      - |  668 | ` * an optional '\r' and a '\n'.` |
|      - |  669 | ` */` |
|    ! 0 |  670 | `static int tok_scan_heredoc(tok_state *ts){` |
|    ! 0 |  671 | `	const unsigned char *zStart = ts->z;` |
|    ! 0 |  672 | `	int iStartLine = ts->iLine;` |
|    ! 0 |  673 | `	const unsigned char *z = ts->z+3;` |
|    ! 0 |  674 | `	int bNowdoc = 0;` |
|    ! 0 |  675 | `	int chQuote = 0;` |
|      - |  676 | `	const unsigned char *zLabel;` |
|      - |  677 | `	int nLabel;` |
|      - |  678 | `	const unsigned char *litStart;` |
|      - |  679 | `	int litLine;` |
|      - |  680 | `	/* optional spaces/tabs after <<< */` |
|    ! 0 |  681 | `	while( z < ts->zEnd && (*z==' '\|\|*z=='\t') ){ z++; }` |
|    ! 0 |  682 | `	if( z < ts->zEnd && (*z=='"' \|\| *z=='\'') ){` |
|    ! 0 |  683 | `		chQuote = *z;` |
|    ! 0 |  684 | `		bNowdoc = (*z=='\'');` |
|    ! 0 |  685 | `		z++;` |
|    ! 0 |  686 | `	}` |
|    ! 0 |  687 | `	zLabel = z;` |
|    ! 0 |  688 | `	while( z < ts->zEnd && tok_is_label(*z) ){ z++; }` |
|    ! 0 |  689 | `	nLabel = (int)(z - zLabel);` |
|    ! 0 |  690 | `	if( nLabel < 1 ){ return 0; } /* no label -> not a heredoc */` |
|    ! 0 |  691 | `	if( chQuote ){` |
|    ! 0 |  692 | `		if( z < ts->zEnd && *z==chQuote ){ z++; }` |
|    ! 0 |  693 | `		else { return 0; } /* unbalanced quote -> not a heredoc */` |
|    ! 0 |  694 | `	}` |
|      - |  695 | `	/* The label must be immediately followed by an optional '\r' then '\n'. */` |
|    ! 0 |  696 | `	if( z < ts->zEnd && *z=='\r' && z+1 < ts->zEnd && z[1]=='\n' ){ z += 2; }` |
|    ! 0 |  697 | `	else if( z < ts->zEnd && *z=='\n' ){ z += 1; }` |
|    ! 0 |  698 | `	else { return 0; } /* junk or EOF after the marker -> not a heredoc */` |
|      - |  699 | `	/* Emit T_START_HEREDOC (delimiters + trailing newline). */` |
|    ! 0 |  700 | `	tok_tok(ts,T_START_HEREDOC,(const char *)zStart,(int)(z-zStart),iStartLine);` |
|    ! 0 |  701 | `	tok_bump_lines(ts,zStart,z);` |
|    ! 0 |  702 | `	ts->z = z;` |
|      - |  703 | `	/* Scan body line by line until the closing marker. */` |
|    ! 0 |  704 | `	litStart = ts->z;` |
|    ! 0 |  705 | `	litLine  = ts->iLine;` |
|    ! 0 |  706 | `	while( ts->z < ts->zEnd && !ts->bOOM ){` |
|      - |  707 | `		/* At a line start, test for the closing marker. */` |
|    ! 0 |  708 | `		const unsigned char *pClose = tok_heredoc_close(ts,ts->z,(const char *)zLabel,nLabel);` |
|    ! 0 |  709 | `		if( pClose ){` |
|      - |  710 | `			int iCloseLine;` |
|    ! 0 |  711 | `			tok_encaps(ts,litStart,ts->z,litLine);` |
|    ! 0 |  712 | `			iCloseLine = ts->iLine;` |
|    ! 0 |  713 | `			tok_tok(ts,T_END_HEREDOC,(const char *)ts->z,(int)(pClose-ts->z),iCloseLine);` |
|    ! 0 |  714 | `			ts->z = pClose;` |
|    ! 0 |  715 | `			return 1;` |
|      - |  716 | `		}` |
|      - |  717 | `		/* Process one line's content. */` |
|    ! 0 |  718 | `		while( ts->z < ts->zEnd ){` |
|    ! 0 |  719 | `			int c = *ts->z;` |
|    ! 0 |  720 | `			if( c=='\n' ){ ts->iLine++; ts->z++; break; }` |
|    ! 0 |  721 | `			if( c=='\r' && (ts->z+1>=ts->zEnd \|\| ts->z[1]!='\n') ){ ts->iLine++; ts->z++; continue; }` |
|    ! 0 |  722 | `			if( !bNowdoc && (c=='\\') && ts->z+1 < ts->zEnd ){` |
|    ! 0 |  723 | `				if( ts->z[1]=='\n' ){ ts->iLine++; } /* escaped newline still advances the line */` |
|    ! 0 |  724 | `				ts->z += 2;` |
|    ! 0 |  725 | `				continue;` |
|      - |  726 | `			}` |
|    ! 0 |  727 | `			if( !bNowdoc && (c=='$' \|\| c=='{') ){` |
|    ! 0 |  728 | `				if( tok_try_interp(ts,&litStart,&litLine) ){ continue; }` |
|    ! 0 |  729 | `			}` |
|    ! 0 |  730 | `			ts->z++;` |
|    ! 0 |  731 | `		}` |
|    ! 0 |  732 | `	}` |
|    ! 0 |  733 | `	tok_encaps(ts,litStart,ts->z,litLine);` |
|    ! 0 |  734 | `	return 1;` |
|    ! 0 |  735 | `}` |
|      - |  736 |  |
|      - |  737 | `/* One cast keyword between the parens, e.g. "int". Returns the cast T_* id or 0. */` |
|    ! 0 |  738 | `static int tok_cast_id(const char *z,int n){` |
|    ! 0 |  739 | `	if( tok_ci_eq(z,n,"int") \|\| tok_ci_eq(z,n,"integer") ) return T_INT_CAST;` |
|    ! 0 |  740 | `	if( tok_ci_eq(z,n,"float") \|\| tok_ci_eq(z,n,"double") \|\| tok_ci_eq(z,n,"real") ) return T_DOUBLE_CAST;` |
|    ! 0 |  741 | `	if( tok_ci_eq(z,n,"string") \|\| tok_ci_eq(z,n,"binary") ) return T_STRING_CAST;` |
|    ! 0 |  742 | `	if( tok_ci_eq(z,n,"array") ) return T_ARRAY_CAST;` |
|    ! 0 |  743 | `	if( tok_ci_eq(z,n,"object") ) return T_OBJECT_CAST;` |
|    ! 0 |  744 | `	if( tok_ci_eq(z,n,"bool") \|\| tok_ci_eq(z,n,"boolean") ) return T_BOOL_CAST;` |
|    ! 0 |  745 | `	if( tok_ci_eq(z,n,"unset") ) return T_UNSET_CAST;` |
|    ! 0 |  746 | `	return 0;` |
|    ! 0 |  747 | `}` |
|      - |  748 |  |
|      - |  749 | `/*` |
|      - |  750 | ` * Try to lex "( <ws>? castword <ws>? )" at ts->z (pointing at '('). On success` |
|      - |  751 | ` * emits the cast token and returns 1; otherwise returns 0 and consumes nothing.` |
|      - |  752 | ` */` |
|    ! 0 |  753 | `static int tok_try_cast(tok_state *ts){` |
|    ! 0 |  754 | `	const unsigned char *p = ts->z+1;` |
|      - |  755 | `	const unsigned char *w0,*w1;` |
|      - |  756 | `	int id;` |
|    ! 0 |  757 | `	while( p < ts->zEnd && (*p==' '\|\|*p=='\t') ){ p++; }` |
|    ! 0 |  758 | `	w0 = p;` |
|    ! 0 |  759 | `	while( p < ts->zEnd && tok_is_label(*p) ){ p++; }` |
|    ! 0 |  760 | `	w1 = p;` |
|    ! 0 |  761 | `	if( w1 == w0 ){ return 0; }` |
|    ! 0 |  762 | `	id = tok_cast_id((const char *)w0,(int)(w1-w0));` |
|    ! 0 |  763 | `	if( id == 0 ){ return 0; }` |
|    ! 0 |  764 | `	while( p < ts->zEnd && (*p==' '\|\|*p=='\t') ){ p++; }` |
|    ! 0 |  765 | `	if( p >= ts->zEnd \|\| *p != ')' ){ return 0; }` |
|    ! 0 |  766 | `	p++;` |
|    ! 0 |  767 | `	tok_tok(ts,id,(const char *)ts->z,(int)(p-ts->z),ts->iLine);` |
|    ! 0 |  768 | `	ts->z = p;` |
|    ! 0 |  769 | `	return 1;` |
|    ! 0 |  770 | `}` |
|      - |  771 |  |
|      - |  772 | `/*` |
|      - |  773 | ` * Lex exactly one PHP-mode token and emit it. Returns:` |
|      - |  774 | ` *   0   normal token` |
|      - |  775 | ` *  '{' / '}'  when it emitted a bare '{' / '}' (for curly-expr brace tracking)` |
|      - |  776 | ` *  -1  when it emitted a close tag or hit EOF (leave PHP mode)` |
|      - |  777 | ` * Whitespace and comments preserve ts->bProp; every other token consumes it.` |
|      - |  778 | ` */` |
|    ! 0 |  779 | `static int tok_lex_one(tok_state *ts){` |
|    ! 0 |  780 | `	const unsigned char *z = ts->z;` |
|      - |  781 | `	int c;` |
|      - |  782 | `	int bWasProp;` |
|      - |  783 | `	int bWasCase;` |
|    ! 0 |  784 | `	if( z >= ts->zEnd ){ return -1; }` |
|    ! 0 |  785 | `	c = *z;` |
|      - |  786 | `	/* Whitespace run (preserves property state). */` |
|    ! 0 |  787 | `	if( tok_is_ws(c) ){` |
|    ! 0 |  788 | `		const unsigned char *z0 = z;` |
|    ! 0 |  789 | `		int iLine = ts->iLine;` |
|    ! 0 |  790 | `		while( ts->z < ts->zEnd && tok_is_ws(*ts->z) ){` |
|    ! 0 |  791 | `			if( tok_at_nl(ts->z,ts->zEnd) ){ ts->iLine++; }` |
|    ! 0 |  792 | `			ts->z++;` |
|    ! 0 |  793 | `		}` |
|    ! 0 |  794 | `		tok_tok(ts,T_WHITESPACE,(const char *)z0,(int)(ts->z-z0),iLine);` |
|    ! 0 |  795 | `		return 0;` |
|      - |  796 | `	}` |
|      - |  797 | `	/* Comments: hash/slash-slash to EOL-or-close-tag, and block comments. Preserve property state. */` |
|    ! 0 |  798 | `	if( c=='#' && !(z+1 < ts->zEnd && z[1]=='[') ){` |
|    ! 0 |  799 | `		const unsigned char *z0 = z;` |
|    ! 0 |  800 | `		int iLine = ts->iLine;` |
|    ! 0 |  801 | `		ts->z++;` |
|    ! 0 |  802 | `		while( ts->z < ts->zEnd && *ts->z!='\n' ){` |
|    ! 0 |  803 | `			if( *ts->z=='?' && ts->z+1 < ts->zEnd && ts->z[1]=='>' ){ break; }` |
|    ! 0 |  804 | `			ts->z++;` |
|    ! 0 |  805 | `		}` |
|    ! 0 |  806 | `		tok_tok(ts,T_COMMENT,(const char *)z0,(int)(ts->z-z0),iLine);` |
|    ! 0 |  807 | `		return 0;` |
|      - |  808 | `	}` |
|    ! 0 |  809 | `	if( c=='/' && z+1 < ts->zEnd && z[1]=='/' ){` |
|    ! 0 |  810 | `		const unsigned char *z0 = z;` |
|    ! 0 |  811 | `		int iLine = ts->iLine;` |
|    ! 0 |  812 | `		ts->z += 2;` |
|    ! 0 |  813 | `		while( ts->z < ts->zEnd && *ts->z!='\n' ){` |
|    ! 0 |  814 | `			if( *ts->z=='?' && ts->z+1 < ts->zEnd && ts->z[1]=='>' ){ break; }` |
|    ! 0 |  815 | `			ts->z++;` |
|    ! 0 |  816 | `		}` |
|    ! 0 |  817 | `		tok_tok(ts,T_COMMENT,(const char *)z0,(int)(ts->z-z0),iLine);` |
|    ! 0 |  818 | `		return 0;` |
|      - |  819 | `	}` |
|    ! 0 |  820 | `	if( c=='/' && z+1 < ts->zEnd && z[1]=='*' ){` |
|    ! 0 |  821 | `		const unsigned char *z0 = z;` |
|    ! 0 |  822 | `		int iLine = ts->iLine;` |
|      - |  823 | `		/* A doc comment is slash-star-star followed by a whitespace char (php's` |
|      - |  824 | `		 * rule): the third char must be '*' and the fourth must be whitespace. */` |
|    ! 0 |  825 | `		int bDoc = ( z+2 < ts->zEnd && z[2]=='*' && z+3 < ts->zEnd && tok_is_ws(z[3]) );` |
|    ! 0 |  826 | `		ts->z += 2;` |
|    ! 0 |  827 | `		while( ts->z < ts->zEnd ){` |
|    ! 0 |  828 | `			if( *ts->z=='*' && ts->z+1 < ts->zEnd && ts->z[1]=='/' ){ ts->z += 2; break; }` |
|    ! 0 |  829 | `			if( tok_at_nl(ts->z,ts->zEnd) ){ ts->iLine++; }` |
|    ! 0 |  830 | `			ts->z++;` |
|    ! 0 |  831 | `		}` |
|    ! 0 |  832 | `		tok_tok(ts,bDoc ? T_DOC_COMMENT : T_COMMENT,(const char *)z0,(int)(ts->z-z0),iLine);` |
|    ! 0 |  833 | `		return 0;` |
|      - |  834 | `	}` |
|      - |  835 | `	/* From here the token consumes property/case state. */` |
|    ! 0 |  836 | `	bWasProp = ts->bProp;` |
|    ! 0 |  837 | `	bWasCase = ts->bCaseName;` |
|    ! 0 |  838 | `	ts->bProp = 0;` |
|    ! 0 |  839 | `	ts->bCaseName = 0;` |
|      - |  840 | `	/* Close tag. */` |
|    ! 0 |  841 | `	if( c=='?' && z+1 < ts->zEnd && z[1]=='>' ){` |
|    ! 0 |  842 | `		const unsigned char *z0 = z;` |
|    ! 0 |  843 | `		int iLine = ts->iLine;` |
|    ! 0 |  844 | `		ts->z += 2;` |
|      - |  845 | `		/* php swallows one trailing newline (\n or \r\n) into the close tag */` |
|    ! 0 |  846 | `		if( ts->z < ts->zEnd && *ts->z=='\r' && ts->z+1 < ts->zEnd && ts->z[1]=='\n' ){ ts->z += 2; ts->iLine++; }` |
|    ! 0 |  847 | `		else if( ts->z < ts->zEnd && *ts->z=='\n' ){ ts->z++; ts->iLine++; }` |
|    ! 0 |  848 | `		tok_tok(ts,T_CLOSE_TAG,(const char *)z0,(int)(ts->z-z0),iLine);` |
|    ! 0 |  849 | `		return -1;` |
|      - |  850 | `	}` |
|      - |  851 | `	/* #[ attribute open. */` |
|    ! 0 |  852 | `	if( c=='#' && z+1 < ts->zEnd && z[1]=='[' ){` |
|    ! 0 |  853 | `		tok_tok(ts,T_ATTRIBUTE,"#[",2,ts->iLine);` |
|    ! 0 |  854 | `		ts->z += 2;` |
|    ! 0 |  855 | `		return 0;` |
|      - |  856 | `	}` |
|      - |  857 | `	/* Variable. */` |
|    ! 0 |  858 | `	if( c=='$' && z+1 < ts->zEnd && tok_is_label_start(z[1]) ){` |
|    ! 0 |  859 | `		const unsigned char *v = z+1;` |
|    ! 0 |  860 | `		while( v < ts->zEnd && tok_is_label(*v) ){ v++; }` |
|    ! 0 |  861 | `		tok_tok(ts,T_VARIABLE,(const char *)z,(int)(v-z),ts->iLine);` |
|    ! 0 |  862 | `		ts->z = v;` |
|    ! 0 |  863 | `		return 0;` |
|      - |  864 | `	}` |
|      - |  865 | `	/* Namespaced name / identifier / keyword. */` |
|    ! 0 |  866 | `	if( tok_is_label_start(c) \|\| (c=='\\' && z+1 < ts->zEnd && tok_is_label_start(z[1])) ){` |
|    ! 0 |  867 | `		const unsigned char *p = z;` |
|    ! 0 |  868 | `		int bLeadBackslash = (c=='\\');` |
|    ! 0 |  869 | `		int bInner = 0;                 /* saw an internal backslash */` |
|      - |  870 | `		const unsigned char *firstLabelStart, *firstLabelEnd;` |
|    ! 0 |  871 | `		if( bLeadBackslash ){ p++; }` |
|    ! 0 |  872 | `		firstLabelStart = p;` |
|    ! 0 |  873 | `		while( p < ts->zEnd && tok_is_label(*p) ){ p++; }` |
|    ! 0 |  874 | `		firstLabelEnd = p;` |
|      - |  875 | `		/* Consume further \label segments. */` |
|    ! 0 |  876 | `		while( p+1 < ts->zEnd && *p=='\\' && tok_is_label_start(p[1]) ){` |
|    ! 0 |  877 | `			bInner = 1;` |
|    ! 0 |  878 | `			p++;` |
|    ! 0 |  879 | `			while( p < ts->zEnd && tok_is_label(*p) ){ p++; }` |
|    ! 0 |  880 | `		}` |
|    ! 0 |  881 | `		if( bLeadBackslash ){` |
|    ! 0 |  882 | `			tok_tok(ts,T_NAME_FULLY_QUALIFIED,(const char *)z,(int)(p-z),ts->iLine);` |
|    ! 0 |  883 | `			ts->z = p;` |
|    ! 0 |  884 | `			return 0;` |
|      - |  885 | `		}` |
|    ! 0 |  886 | `		if( bInner ){` |
|    ! 0 |  887 | `			int nFirst = (int)(firstLabelEnd - firstLabelStart);` |
|    ! 0 |  888 | `			int id = T_NAME_QUALIFIED;` |
|    ! 0 |  889 | `			if( tok_ci_eq((const char *)firstLabelStart,nFirst,"namespace") ){ id = T_NAME_RELATIVE; }` |
|    ! 0 |  890 | `			tok_tok(ts,id,(const char *)z,(int)(p-z),ts->iLine);` |
|    ! 0 |  891 | `			ts->z = p;` |
|    ! 0 |  892 | `			return 0;` |
|      - |  893 | `		}` |
|      - |  894 | `		/* Lone identifier: keyword, contextual, or T_STRING. */` |
|      - |  895 | `		{` |
|    ! 0 |  896 | `			int n = (int)(firstLabelEnd - z);` |
|    ! 0 |  897 | `			int id = T_STRING;` |
|      - |  898 | `			sxu32 k;` |
|    ! 0 |  899 | `			if( bWasProp ){` |
|    ! 0 |  900 | `				id = T_STRING;              /* property access after -> / ?-> */` |
|    ! 0 |  901 | `			}else if( tok_ci_eq((const char *)z,n,"enum") ){` |
|      - |  902 | `				/* Contextual: T_ENUM only when followed by <ws>+ then a label-start. */` |
|    ! 0 |  903 | `				const unsigned char *q = firstLabelEnd;` |
|    ! 0 |  904 | `				int sawWs = 0;` |
|    ! 0 |  905 | `				while( q < ts->zEnd && (*q==' '\|\|*q=='\t'\|\|*q=='\n'\|\|*q=='\r') ){ q++; sawWs = 1; }` |
|    ! 0 |  906 | `				if( sawWs && q < ts->zEnd && tok_is_label_start(*q) ){ id = T_ENUM; }` |
|    ! 0 |  907 | `			}else if( tok_ci_eq((const char *)z,n,"yield") ){` |
|      - |  908 | `				/* Contextual: "yield from" collapses to one T_YIELD_FROM. */` |
|    ! 0 |  909 | `				const unsigned char *q = firstLabelEnd;` |
|    ! 0 |  910 | `				const unsigned char *ws = q;` |
|    ! 0 |  911 | `				while( q < ts->zEnd && (*q==' '\|\|*q=='\t'\|\|*q=='\n'\|\|*q=='\r'\|\|*q=='\v'\|\|*q=='\f') ){ q++; }` |
|    ! 0 |  912 | `				if( q > ws && q+4 <= ts->zEnd && tok_ci_eq((const char *)q,4,"from")` |
|    ! 0 |  913 | `					&& (q+4 >= ts->zEnd \|\| !tok_is_label(q[4])) ){` |
|    ! 0 |  914 | `					const unsigned char *e = q+4;` |
|    ! 0 |  915 | `					tok_tok(ts,T_YIELD_FROM,(const char *)z,(int)(e-z),ts->iLine);` |
|    ! 0 |  916 | `					tok_bump_lines(ts,firstLabelEnd,e);` |
|    ! 0 |  917 | `					ts->z = e;` |
|    ! 0 |  918 | `					return 0;` |
|      - |  919 | `				}` |
|    ! 0 |  920 | `				id = T_YIELD;` |
|    ! 0 |  921 | `			}else{` |
|    ! 0 |  922 | `				for( k = 0 ; k < SX_ARRAYSIZE(aKeyword) ; ++k ){` |
|    ! 0 |  923 | `					if( tok_ci_eq((const char *)z,n,aKeyword[k].zName) ){` |
|    ! 0 |  924 | `						id = aKeyword[k].iId;` |
|    ! 0 |  925 | `						break;` |
|      - |  926 | `					}` |
|    ! 0 |  927 | `				}` |
|      - |  928 | `			}` |
|      - |  929 | `			/* Under TOKEN_PARSE, a reserved word right after 'case' that names an` |
|      - |  930 | `			 * enum case (followed by ';' or '=') is T_STRING; a switch 'case` |
|      - |  931 | `			 * array(...)'/'case expr:' keeps the keyword. */` |
|    ! 0 |  932 | `			if( bWasCase && id != T_STRING ){` |
|    ! 0 |  933 | `				const unsigned char *q = firstLabelEnd;` |
|    ! 0 |  934 | `				while( q < ts->zEnd && tok_is_ws(*q) ){ q++; }` |
|      - |  935 | `				/* ';' ends a pure case, a lone '=' (not '=='/'=>') starts a backed` |
|      - |  936 | `				 * case value; both mark an enum case name. */` |
|    ! 0 |  937 | `				if( q < ts->zEnd && (*q==';' \|\|` |
|    ! 0 |  938 | `					(*q=='=' && (q+1>=ts->zEnd \|\| (q[1]!='=' && q[1]!='>')))) ){` |
|    ! 0 |  939 | `					id = T_STRING;` |
|    ! 0 |  940 | `				}` |
|    ! 0 |  941 | `			}` |
|    ! 0 |  942 | `			if( id == T_STRING ){` |
|    ! 0 |  943 | `				tok_tok(ts,T_STRING,(const char *)z,n,ts->iLine);` |
|    ! 0 |  944 | `			}else{` |
|    ! 0 |  945 | `				tok_tok(ts,id,(const char *)z,n,ts->iLine);` |
|      - |  946 | `			}` |
|    ! 0 |  947 | `			ts->z = firstLabelEnd;` |
|    ! 0 |  948 | `			if( id == T_HALT_COMPILER ){` |
|    ! 0 |  949 | `				tok_halt_tail(ts);` |
|    ! 0 |  950 | `				return -1;` |
|      - |  951 | `			}` |
|      - |  952 | `			/* Under TOKEN_PARSE, a reserved word naming a function/method or a` |
|      - |  953 | `			 * class constant becomes T_STRING (semi-reserved words); force the` |
|      - |  954 | `			 * next identifier to T_STRING like a member name. 'case' arms a` |
|      - |  955 | `			 * conditional retag (enum case names only — see the identifier path). */` |
|    ! 0 |  956 | `			if( ts->bParse && (id == T_FUNCTION \|\| id == T_CONST) ){` |
|    ! 0 |  957 | `				ts->bProp = 1;` |
|    ! 0 |  958 | `			}` |
|    ! 0 |  959 | `			if( ts->bParse && id == T_CASE ){` |
|    ! 0 |  960 | `				ts->bCaseName = 1;` |
|    ! 0 |  961 | `			}` |
|    ! 0 |  962 | `			return 0;` |
|      - |  963 | `		}` |
|      - |  964 | `	}` |
|      - |  965 | `	/* Lone backslash -> namespace separator. */` |
|    ! 0 |  966 | `	if( c=='\\' ){` |
|    ! 0 |  967 | `		tok_tok(ts,T_NS_SEPARATOR,"\\",1,ts->iLine);` |
|    ! 0 |  968 | `		ts->z++;` |
|    ! 0 |  969 | `		return 0;` |
|      - |  970 | `	}` |
|      - |  971 | `	/* Number. */` |
|    ! 0 |  972 | `	if( (c>='0'&&c<='9') \|\| (c=='.' && z+1 < ts->zEnd && z[1]>='0' && z[1]<='9') ){` |
|    ! 0 |  973 | `		const unsigned char *p = z;` |
|    ! 0 |  974 | `		int isFloat = 0;` |
|      - |  975 | `		int id;` |
|    ! 0 |  976 | `		if( c=='0' && p+2 < ts->zEnd && (p[1]=='x'\|\|p[1]=='X')` |
|    ! 0 |  977 | `				&& ((p[2]>='0'&&p[2]<='9')\|\|(p[2]>='a'&&p[2]<='f')\|\|(p[2]>='A'&&p[2]<='F')) ){` |
|      - |  978 | `			const unsigned char *d0;` |
|    ! 0 |  979 | `			p += 2; d0 = p;` |
|    ! 0 |  980 | `			while( p < ts->zEnd && ((*p>='0'&&*p<='9')\|\|(*p>='a'&&*p<='f')\|\|(*p>='A'&&*p<='F')\|\|*p=='_') ){ p++; }` |
|    ! 0 |  981 | `			id = tok_int_overflows(d0,p,16) ? T_DNUMBER : T_LNUMBER;` |
|    ! 0 |  982 | `		}else if( c=='0' && p+2 < ts->zEnd && (p[1]=='b'\|\|p[1]=='B') && (p[2]=='0'\|\|p[2]=='1') ){` |
|      - |  983 | `			const unsigned char *d0;` |
|    ! 0 |  984 | `			p += 2; d0 = p;` |
|    ! 0 |  985 | `			while( p < ts->zEnd && (*p=='0'\|\|*p=='1'\|\|*p=='_') ){ p++; }` |
|    ! 0 |  986 | `			id = tok_int_overflows(d0,p,2) ? T_DNUMBER : T_LNUMBER;` |
|    ! 0 |  987 | `		}else if( c=='0' && p+2 < ts->zEnd && (p[1]=='o'\|\|p[1]=='O') && (p[2]>='0'&&p[2]<='7') ){` |
|      - |  988 | `			const unsigned char *d0;` |
|    ! 0 |  989 | `			p += 2; d0 = p;` |
|    ! 0 |  990 | `			while( p < ts->zEnd && ((*p>='0'&&*p<='7')\|\|*p=='_') ){ p++; }` |
|    ! 0 |  991 | `			id = tok_int_overflows(d0,p,8) ? T_DNUMBER : T_LNUMBER;` |
|    ! 0 |  992 | `		}else{` |
|    ! 0 |  993 | `			const unsigned char *intStart = p;` |
|    ! 0 |  994 | `			int allOctalDigits = 1;` |
|    ! 0 |  995 | `			while( p < ts->zEnd && ((*p>='0'&&*p<='9')\|\|*p=='_') ){` |
|    ! 0 |  996 | `				if( *p>'7' ){ allOctalDigits = 0; }` |
|    ! 0 |  997 | `				p++;` |
|    ! 0 |  998 | `			}` |
|    ! 0 |  999 | `			if( p < ts->zEnd && *p=='.' && !(c=='.') ){` |
|      - | 1000 | `				/* fractional part (unless the token itself started with '.') */` |
|    ! 0 | 1001 | `				isFloat = 1;` |
|    ! 0 | 1002 | `				p++;` |
|    ! 0 | 1003 | `				while( p < ts->zEnd && ((*p>='0'&&*p<='9')\|\|*p=='_') ){ p++; }` |
|    ! 0 | 1004 | `			}else if( c=='.' ){` |
|    ! 0 | 1005 | `				isFloat = 1; /* .5 style: leading dot already consumed below */` |
|    ! 0 | 1006 | `			}` |
|    ! 0 | 1007 | `			if( c=='.' ){` |
|      - | 1008 | `				/* token began with '.': consume the dot + digits here */` |
|    ! 0 | 1009 | `				p = z+1;` |
|    ! 0 | 1010 | `				while( p < ts->zEnd && ((*p>='0'&&*p<='9')\|\|*p=='_') ){ p++; }` |
|    ! 0 | 1011 | `				isFloat = 1;` |
|    ! 0 | 1012 | `			}` |
|    ! 0 | 1013 | `			if( p < ts->zEnd && (*p=='e'\|\|*p=='E') ){` |
|    ! 0 | 1014 | `				const unsigned char *e = p+1;` |
|    ! 0 | 1015 | `				if( e < ts->zEnd && (*e=='+'\|\|*e=='-') ){ e++; }` |
|    ! 0 | 1016 | `				if( e < ts->zEnd && *e>='0' && *e<='9' ){` |
|    ! 0 | 1017 | `					isFloat = 1;` |
|    ! 0 | 1018 | `					p = e;` |
|    ! 0 | 1019 | `					while( p < ts->zEnd && ((*p>='0'&&*p<='9')\|\|*p=='_') ){ p++; }` |
|    ! 0 | 1020 | `				}` |
|    ! 0 | 1021 | `			}` |
|    ! 0 | 1022 | `			if( isFloat ){` |
|    ! 0 | 1023 | `				id = T_DNUMBER;` |
|    ! 0 | 1024 | `			}else if( intStart < ts->zEnd && *intStart=='0' && (p-intStart) > 1 && allOctalDigits ){` |
|      - | 1025 | `				/* legacy octal 0NNN */` |
|    ! 0 | 1026 | `				id = tok_int_overflows(intStart+1,p,8) ? T_DNUMBER : T_LNUMBER;` |
|    ! 0 | 1027 | `			}else{` |
|    ! 0 | 1028 | `				id = tok_int_overflows(intStart,p,10) ? T_DNUMBER : T_LNUMBER;` |
|      - | 1029 | `			}` |
|      - | 1030 | `		}` |
|    ! 0 | 1031 | `		tok_tok(ts,id,(const char *)z,(int)(p-z),ts->iLine);` |
|    ! 0 | 1032 | `		ts->z = p;` |
|    ! 0 | 1033 | `		return 0;` |
|      - | 1034 | `	}` |
|      - | 1035 | `	/* Strings. */` |
|    ! 0 | 1036 | `	if( c=='\'' ){` |
|    ! 0 | 1037 | `		const unsigned char *p = z+1;` |
|    ! 0 | 1038 | `		int bClosed = 0;` |
|    ! 0 | 1039 | `		while( p < ts->zEnd ){` |
|    ! 0 | 1040 | `			if( *p=='\\' && p+1 < ts->zEnd ){ p += 2; continue; }` |
|    ! 0 | 1041 | `			if( *p=='\'' ){ p++; bClosed = 1; break; }` |
|    ! 0 | 1042 | `			p++;` |
|    ! 0 | 1043 | `		}` |
|      - | 1044 | `		/* An unterminated single-quoted string is one T_ENCAPSED_AND_WHITESPACE in php. */` |
|    ! 0 | 1045 | `		tok_tok(ts,bClosed ? T_CONSTANT_ENCAPSED_STRING : T_ENCAPSED_AND_WHITESPACE,` |
|    ! 0 | 1046 | `			(const char *)z,(int)(p-z),ts->iLine);` |
|    ! 0 | 1047 | `		tok_bump_lines(ts,z,p);` |
|    ! 0 | 1048 | `		ts->z = p;` |
|    ! 0 | 1049 | `		return 0;` |
|      - | 1050 | `	}` |
|    ! 0 | 1051 | `	if( c=='"' ){ tok_scan_dquote(ts,'"'); return 0; }` |
|    ! 0 | 1052 | ``	if( c=='`' ){ tok_scan_dquote(ts,'`'); return 0; }`` |
|      - | 1053 | `	/* Heredoc / nowdoc (falls through to operators if not a valid start). */` |
|    ! 0 | 1054 | `	if( c=='<' && z+2 < ts->zEnd && z[1]=='<' && z[2]=='<' ){` |
|    ! 0 | 1055 | `		if( tok_scan_heredoc(ts) ){ return 0; }` |
|    ! 0 | 1056 | `	}` |
|      - | 1057 | `	/* Cast operators. */` |
|    ! 0 | 1058 | `	if( c=='(' ){` |
|    ! 0 | 1059 | `		if( tok_try_cast(ts) ){ return 0; }` |
|    ! 0 | 1060 | `	}` |
|      - | 1061 | `	/* Object operators set the one-shot property state (next identifier -> T_STRING). */` |
|    ! 0 | 1062 | `	if( c=='?' && z+2 < ts->zEnd && z[1]=='-' && z[2]=='>' ){` |
|    ! 0 | 1063 | `		tok_tok(ts,T_NULLSAFE_OBJECT_OPERATOR,"?->",3,ts->iLine);` |
|    ! 0 | 1064 | `		ts->z += 3;` |
|    ! 0 | 1065 | `		ts->bProp = 1;` |
|    ! 0 | 1066 | `		return 0;` |
|      - | 1067 | `	}` |
|    ! 0 | 1068 | `	if( c=='-' && z+1 < ts->zEnd && z[1]=='>' ){` |
|    ! 0 | 1069 | `		tok_tok(ts,T_OBJECT_OPERATOR,"->",2,ts->iLine);` |
|    ! 0 | 1070 | `		ts->z += 2;` |
|    ! 0 | 1071 | `		ts->bProp = 1;` |
|    ! 0 | 1072 | `		return 0;` |
|      - | 1073 | `	}` |
|      - | 1074 | `	/* Under TOKEN_PARSE, php re-tags a reserved word used as a member name after` |
|      - | 1075 | `	 * "::" as T_STRING (e.g. Foo::class, Foo::empty). Mirror that with bProp. */` |
|    ! 0 | 1076 | `	if( c==':' && z+1 < ts->zEnd && z[1]==':' ){` |
|    ! 0 | 1077 | `		tok_tok(ts,T_DOUBLE_COLON,"::",2,ts->iLine);` |
|    ! 0 | 1078 | `		ts->z += 2;` |
|    ! 0 | 1079 | `		if( ts->bParse ){ ts->bProp = 1; }` |
|    ! 0 | 1080 | `		return 0;` |
|      - | 1081 | `	}` |
|      - | 1082 | `	/* Multi-char and single-char operators (longest match first). */` |
|      - | 1083 | `	{` |
|    ! 0 | 1084 | `		const unsigned char *e = ts->zEnd;` |
|    ! 0 | 1085 | `		int r0 = c;` |
|    ! 0 | 1086 | `		int r1 = (z+1<e)?z[1]:-1;` |
|    ! 0 | 1087 | `		int r2 = (z+2<e)?z[2]:-1;` |
|      - | 1088 | `		#define TK3(a,b,cc,id) if(r0==(a)&&r1==(b)&&r2==(cc)){ tok_tok(ts,id,(const char*)z,3,ts->iLine); ts->z+=3; return 0; }` |
|      - | 1089 | `		#define TK2(a,b,id)    if(r0==(a)&&r1==(b)){ tok_tok(ts,id,(const char*)z,2,ts->iLine); ts->z+=2; return 0; }` |
|    ! 0 | 1090 | `		TK3('=','=','=',T_IS_IDENTICAL)` |
|    ! 0 | 1091 | `		TK3('!','=','=',T_IS_NOT_IDENTICAL)` |
|    ! 0 | 1092 | `		TK3('<','=','>',T_SPACESHIP)` |
|    ! 0 | 1093 | `		TK3('*','*','=',T_POW_EQUAL)` |
|    ! 0 | 1094 | `		TK3('.','.','.',T_ELLIPSIS)` |
|    ! 0 | 1095 | `		TK3('<','<','=',T_SL_EQUAL)` |
|    ! 0 | 1096 | `		TK3('>','>','=',T_SR_EQUAL)` |
|    ! 0 | 1097 | `		TK3('?','?','=',T_COALESCE_EQUAL)` |
|    ! 0 | 1098 | `		TK2('=','=',T_IS_EQUAL)` |
|    ! 0 | 1099 | `		TK2('!','=',T_IS_NOT_EQUAL)` |
|    ! 0 | 1100 | `		TK2('<','>',T_IS_NOT_EQUAL)` |
|    ! 0 | 1101 | `		TK2('<','=',T_IS_SMALLER_OR_EQUAL)` |
|    ! 0 | 1102 | `		TK2('>','=',T_IS_GREATER_OR_EQUAL)` |
|    ! 0 | 1103 | `		TK2('&','&',T_BOOLEAN_AND)` |
|    ! 0 | 1104 | `		TK2('\|','\|',T_BOOLEAN_OR)` |
|    ! 0 | 1105 | `		TK2('\|','>',T_PIPE)` |
|    ! 0 | 1106 | `		TK2('+','+',T_INC)` |
|    ! 0 | 1107 | `		TK2('-','-',T_DEC)` |
|    ! 0 | 1108 | `		TK2('=','>',T_DOUBLE_ARROW)` |
|    ! 0 | 1109 | `		TK2('<','<',T_SL)` |
|    ! 0 | 1110 | `		TK2('>','>',T_SR)` |
|    ! 0 | 1111 | `		TK2('*','*',T_POW)` |
|    ! 0 | 1112 | `		TK2('?','?',T_COALESCE)` |
|    ! 0 | 1113 | `		TK2('+','=',T_PLUS_EQUAL)` |
|    ! 0 | 1114 | `		TK2('-','=',T_MINUS_EQUAL)` |
|    ! 0 | 1115 | `		TK2('*','=',T_MUL_EQUAL)` |
|    ! 0 | 1116 | `		TK2('/','=',T_DIV_EQUAL)` |
|    ! 0 | 1117 | `		TK2('.','=',T_CONCAT_EQUAL)` |
|    ! 0 | 1118 | `		TK2('%','=',T_MOD_EQUAL)` |
|    ! 0 | 1119 | `		TK2('&','=',T_AND_EQUAL)` |
|    ! 0 | 1120 | `		TK2('\|','=',T_OR_EQUAL)` |
|    ! 0 | 1121 | `		TK2('^','=',T_XOR_EQUAL)` |
|      - | 1122 | `		#undef TK3` |
|      - | 1123 | `		#undef TK2` |
|      - | 1124 | `	}` |
|      - | 1125 | `	/* Ampersand: FOLLOWED (by var / vararg) vs NOT, looking past whitespace. */` |
|    ! 0 | 1126 | `	if( c=='&' ){` |
|    ! 0 | 1127 | `		const unsigned char *p = z+1;` |
|      - | 1128 | `		int id;` |
|    ! 0 | 1129 | `		while( p < ts->zEnd && tok_is_ws(*p) ){ p++; }` |
|    ! 0 | 1130 | `		if( (p < ts->zEnd && *p=='$') \|\|` |
|    ! 0 | 1131 | `			(p+2 < ts->zEnd && p[0]=='.' && p[1]=='.' && p[2]=='.') ){` |
|    ! 0 | 1132 | `			id = T_AMPERSAND_FOLLOWED_BY_VAR_OR_VARARG;` |
|    ! 0 | 1133 | `		}else{` |
|    ! 0 | 1134 | `			id = T_AMPERSAND_NOT_FOLLOWED_BY_VAR_OR_VARARG;` |
|      - | 1135 | `		}` |
|    ! 0 | 1136 | `		tok_tok(ts,id,"&",1,ts->iLine);` |
|    ! 0 | 1137 | `		ts->z++;` |
|    ! 0 | 1138 | `		return 0;` |
|      - | 1139 | `	}` |
|      - | 1140 | `	/* Control bytes that begin no token are T_BAD_CHARACTER in php. */` |
|    ! 0 | 1141 | `	if( c < 0x20 \|\| c == 0x7f ){` |
|    ! 0 | 1142 | `		char ch = (char)c;` |
|    ! 0 | 1143 | `		tok_tok(ts,T_BAD_CHARACTER,&ch,1,ts->iLine);` |
|    ! 0 | 1144 | `		ts->z++;` |
|    ! 0 | 1145 | `		return 0;` |
|      - | 1146 | `	}` |
|      - | 1147 | `	/* Single-char token, returned as a bare string. */` |
|      - | 1148 | `	{` |
|    ! 0 | 1149 | `		char ch = (char)c;` |
|    ! 0 | 1150 | `		tok_plain(ts,&ch,1);` |
|    ! 0 | 1151 | `		ts->z++;` |
|    ! 0 | 1152 | `		if( c=='{' ) return '{';` |
|    ! 0 | 1153 | `		if( c=='}' ) return '}';` |
|    ! 0 | 1154 | `		return 0;` |
|      - | 1155 | `	}` |
|    ! 0 | 1156 | `}` |
|      - | 1157 |  |
|      - | 1158 | `/* Emit T_OPEN_TAG/T_OPEN_TAG_WITH_ECHO at ts->z; returns 1 if a tag was found. */` |
|    ! 0 | 1159 | `static int tok_open_tag(tok_state *ts){` |
|    ! 0 | 1160 | `	const unsigned char *z = ts->z;` |
|    ! 0 | 1161 | `	if( z+2 < ts->zEnd && z[0]=='<' && z[1]=='?' && z[2]=='=' ){` |
|    ! 0 | 1162 | `		tok_tok(ts,T_OPEN_TAG_WITH_ECHO,"<?=",3,ts->iLine);` |
|    ! 0 | 1163 | `		ts->z = z+3;` |
|    ! 0 | 1164 | `		return 1;` |
|      - | 1165 | `	}` |
|    ! 0 | 1166 | `	if( z+4 <= ts->zEnd && z[0]=='<' && z[1]=='?'` |
|    ! 0 | 1167 | `		&& tok_lower(z[2])=='p' && tok_lower(z[3])=='h' && z+5 <= ts->zEnd && tok_lower(z[4])=='p' ){` |
|      - | 1168 | `		/* Require <?php to be followed by whitespace or EOF. */` |
|    ! 0 | 1169 | `		const unsigned char *p = z+5;` |
|    ! 0 | 1170 | `		if( p >= ts->zEnd \|\| tok_is_ws(*p) ){` |
|    ! 0 | 1171 | `			const unsigned char *e = z+5;` |
|    ! 0 | 1172 | `			int iLine = ts->iLine;` |
|    ! 0 | 1173 | `			if( e < ts->zEnd && tok_is_ws(*e) ){` |
|    ! 0 | 1174 | `				if( tok_at_nl(e,ts->zEnd) ){ ts->iLine++; }` |
|    ! 0 | 1175 | `				e++;                 /* one trailing whitespace char joins the tag */` |
|    ! 0 | 1176 | `			}` |
|    ! 0 | 1177 | `			tok_tok(ts,T_OPEN_TAG,(const char *)z,(int)(e-z),iLine);` |
|    ! 0 | 1178 | `			ts->z = e;` |
|    ! 0 | 1179 | `			return 1;` |
|      - | 1180 | `		}` |
|    ! 0 | 1181 | `	}` |
|    ! 0 | 1182 | `	return 0;` |
|    ! 0 | 1183 | `}` |
|      - | 1184 |  |
|      - | 1185 | `/* The scanning driver: alternate inline-HTML and PHP modes over the source. */` |
|    ! 0 | 1186 | `static void tok_run(tok_state *ts){` |
|    ! 0 | 1187 | `	while( ts->z < ts->zEnd && !ts->bOOM && !ts->bStop ){` |
|      - | 1188 | `		/* Inline HTML until the next open tag. */` |
|    ! 0 | 1189 | `		const unsigned char *zHtml = ts->z;` |
|    ! 0 | 1190 | `		int iHtmlLine = ts->iLine;` |
|    ! 0 | 1191 | `		while( ts->z < ts->zEnd ){` |
|    ! 0 | 1192 | `			if( ts->z[0]=='<' && ts->z+1 < ts->zEnd && ts->z[1]=='?' ){` |
|      - | 1193 | `				/* Only <?php and <?= are recognised (short tags off). */` |
|    ! 0 | 1194 | `				const unsigned char *s = ts->z;` |
|    ! 0 | 1195 | `				int bTag = 0;` |
|    ! 0 | 1196 | `				if( s+2 < ts->zEnd && s[2]=='=' ){ bTag = 1; }` |
|    ! 0 | 1197 | `				else if( s+4 <= ts->zEnd && tok_lower(s[2])=='p' && tok_lower(s[3])=='h'` |
|    ! 0 | 1198 | `					&& s+5 <= ts->zEnd && tok_lower(s[4])=='p'` |
|    ! 0 | 1199 | `					&& (s+5 >= ts->zEnd \|\| tok_is_ws(s[5])) ){ bTag = 1; }` |
|    ! 0 | 1200 | `				if( bTag ){ break; }` |
|    ! 0 | 1201 | `			}` |
|    ! 0 | 1202 | `			if( tok_at_nl(ts->z,ts->zEnd) ){ ts->iLine++; }` |
|    ! 0 | 1203 | `			ts->z++;` |
|    ! 0 | 1204 | `		}` |
|    ! 0 | 1205 | `		if( ts->z > zHtml ){` |
|    ! 0 | 1206 | `			tok_tok(ts,T_INLINE_HTML,(const char *)zHtml,(int)(ts->z-zHtml),iHtmlLine);` |
|    ! 0 | 1207 | `		}` |
|    ! 0 | 1208 | `		if( ts->z >= ts->zEnd ){ break; }` |
|    ! 0 | 1209 | `		if( !tok_open_tag(ts) ){` |
|      - | 1210 | `			/* Not actually a tag (shouldn't happen given the check above). */` |
|    ! 0 | 1211 | `			if( tok_at_nl(ts->z,ts->zEnd) ){ ts->iLine++; }` |
|    ! 0 | 1212 | `			ts->z++;` |
|    ! 0 | 1213 | `			continue;` |
|      - | 1214 | `		}` |
|      - | 1215 | `		/* PHP mode until a close tag or EOF. */` |
|    ! 0 | 1216 | `		ts->bProp = 0;` |
|    ! 0 | 1217 | `		while( ts->z < ts->zEnd && !ts->bOOM ){` |
|    ! 0 | 1218 | `			int eff = tok_lex_one(ts);` |
|    ! 0 | 1219 | `			if( eff < 0 ){ break; }` |
|    ! 0 | 1220 | `		}` |
|    ! 0 | 1221 | `	}` |
|    ! 0 | 1222 | `}` |
|      - | 1223 |  |
|      - | 1224 | `/*` |
|      - | 1225 | ` * array token_get_all(string $source [, int $flags = 0 ])` |
|      - | 1226 | ` */` |
|    ! 0 | 1227 | `static int PH7_builtin_token_get_all(ph7_context *pCtx,int nArg,ph7_value **apArg){` |
|      - | 1228 | `	tok_state ts;` |
|      - | 1229 | `	const char *zSrc;` |
|    ! 0 | 1230 | `	int nSrc = 0;` |
|    ! 0 | 1231 | `	if( nArg < 1 ){` |
|    ! 0 | 1232 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 1233 | `			"token_get_all() expects at least 1 argument, %d given",nArg);` |
|      - | 1234 | `	}` |
|    ! 0 | 1235 | `	zSrc = ph7_value_to_string(apArg[0],&nSrc);` |
|    ! 0 | 1236 | `	SyZero(&ts,sizeof(ts));` |
|    ! 0 | 1237 | `	ts.pCtx  = pCtx;` |
|    ! 0 | 1238 | `	ts.pArray = ph7_context_new_array(pCtx);` |
|    ! 0 | 1239 | `	ts.pS    = ph7_context_new_scalar(pCtx);` |
|    ! 0 | 1240 | `	ts.pId   = ph7_context_new_scalar(pCtx);` |
|    ! 0 | 1241 | `	ts.pText = ph7_context_new_scalar(pCtx);` |
|    ! 0 | 1242 | `	ts.pLine = ph7_context_new_scalar(pCtx);` |
|    ! 0 | 1243 | `	if( ts.pArray==0 \|\| ts.pS==0 \|\| ts.pId==0 \|\| ts.pText==0 \|\| ts.pLine==0 ){` |
|    ! 0 | 1244 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1245 | `	}` |
|    ! 0 | 1246 | `	ts.z    = (const unsigned char *)zSrc;` |
|    ! 0 | 1247 | `	ts.zEnd = ts.z + (nSrc > 0 ? (sxu32)nSrc : 0);` |
|    ! 0 | 1248 | `	ts.iLine = 1;` |
|    ! 0 | 1249 | `	if( nArg > 1 && (ph7_value_to_int(apArg[1]) & TOK_TOKEN_PARSE) ){` |
|    ! 0 | 1250 | `		ts.bParse = 1;` |
|    ! 0 | 1251 | `	}` |
|    ! 0 | 1252 | `	tok_run(&ts);` |
|    ! 0 | 1253 | `	if( ts.bOOM ){` |
|    ! 0 | 1254 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1255 | `	}` |
|    ! 0 | 1256 | `	ph7_result_value(pCtx,ts.pArray);` |
|    ! 0 | 1257 | `	return PH7_OK;` |
|    ! 0 | 1258 | `}` |
|      - | 1259 |  |
|      - | 1260 | `/*` |
|      - | 1261 | ` * string token_name(int $id)` |
|      - | 1262 | ` */` |
|    ! 0 | 1263 | `static int PH7_builtin_token_name(ph7_context *pCtx,int nArg,ph7_value **apArg){` |
|      - | 1264 | `	int iId;` |
|      - | 1265 | `	const char *zName;` |
|    ! 0 | 1266 | `	if( nArg < 1 ){` |
|    ! 0 | 1267 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 1268 | `			"token_name() expects exactly 1 argument, %d given",nArg);` |
|      - | 1269 | `	}` |
|    ! 0 | 1270 | `	iId = ph7_value_to_int(apArg[0]);` |
|    ! 0 | 1271 | `	zName = TokConstName(iId);` |
|    ! 0 | 1272 | `	if( zName == 0 ){` |
|    ! 0 | 1273 | `		ph7_result_string(pCtx,"UNKNOWN",(int)sizeof("UNKNOWN")-1);` |
|    ! 0 | 1274 | `	}else{` |
|    ! 0 | 1275 | `		ph7_result_string(pCtx,zName,-1/*SyStrlen*/);` |
|      - | 1276 | `	}` |
|    ! 0 | 1277 | `	return PH7_OK;` |
|    ! 0 | 1278 | `}` |
|      - | 1279 |  |
|      - | 1280 | `/*` |
|      - | 1281 | ` * The PhpToken class: a thin userland wrapper over token_get_all(), tracking` |
|      - | 1282 | ` * byte offset (->pos) and line for the plain single-char tokens.` |
|      - | 1283 | ` */` |
|      - | 1284 | `static const char zPhpTokenClass[] = {` |
|      - | 1285 | `	"final class PhpToken implements Stringable {"` |
|      - | 1286 | `	" public int $id;"` |
|      - | 1287 | `	" public string $text;"` |
|      - | 1288 | `	" public int $line;"` |
|      - | 1289 | `	" public int $pos;"` |
|      - | 1290 | `	" public function __construct(int $id, string $text, int $line = -1, int $pos = -1){"` |
|      - | 1291 | `	"  $this->id = $id; $this->text = $text; $this->line = $line; $this->pos = $pos;"` |
|      - | 1292 | `	" }"` |
|      - | 1293 | `	" public static function tokenize(string $code, int $flags = 0): array {"` |
|      - | 1294 | `	"  $tokens = token_get_all($code, $flags);"` |
|      - | 1295 | `	"  $result = array(); $pos = 0; $line = 1;"` |
|      - | 1296 | `	"  foreach( $tokens as $tok ){"` |
|      - | 1297 | `	"   if( is_array($tok) ){ $id = $tok[0]; $text = $tok[1]; $ln = $tok[2]; }"` |
|      - | 1298 | `	"   else { $id = ord($tok); $text = $tok; $ln = $line; }"` |
|      - | 1299 | `	"   $result[] = new static($id, $text, $ln, $pos);"` |
|      - | 1300 | `	"   $pos += strlen($text);"` |
|      - | 1301 | `	"   $line += substr_count($text, \"\\n\");"` |
|      - | 1302 | `	"  }"` |
|      - | 1303 | `	"  return $result;"` |
|      - | 1304 | `	" }"` |
|      - | 1305 | `	" public function is($kind): bool {"` |
|      - | 1306 | `	"  if( is_array($kind) ){"` |
|      - | 1307 | `	"   foreach( $kind as $k ){"` |
|      - | 1308 | `	"    if( is_string($k) ){ if( $this->text === $k ){ return true; } }"` |
|      - | 1309 | `	"    elseif( $this->id === $k ){ return true; }"` |
|      - | 1310 | `	"   }"` |
|      - | 1311 | `	"   return false;"` |
|      - | 1312 | `	"  }"` |
|      - | 1313 | `	"  if( is_string($kind) ){ return $this->text === $kind; }"` |
|      - | 1314 | `	"  return $this->id === $kind;"` |
|      - | 1315 | `	" }"` |
|      - | 1316 | `	" public function isIgnorable(): bool {"` |
|      - | 1317 | `	"  return $this->id === T_WHITESPACE \|\| $this->id === T_COMMENT"` |
|      - | 1318 | `	"   \|\| $this->id === T_DOC_COMMENT \|\| $this->id === T_OPEN_TAG;"` |
|      - | 1319 | `	" }"` |
|      - | 1320 | `	" public function getTokenName(): ?string {"` |
|      - | 1321 | `	"  if( $this->id < 256 ){ return chr($this->id); }"` |
|      - | 1322 | `	"  $name = token_name($this->id);"` |
|      - | 1323 | `	"  if( $name === 'UNKNOWN' ){ return null; }"` |
|      - | 1324 | `	"  return $name;"` |
|      - | 1325 | `	" }"` |
|      - | 1326 | `	" public function __toString(): string { return (string)$this->text; }"` |
|      - | 1327 | `	"}"` |
|      - | 1328 | `};` |
|      - | 1329 |  |
|   3879 | 1330 | `PH7_PRIVATE sxi32 PH7_VmInstallTokenizer(ph7_vm *pVm){` |
|   3879 | 1331 | `	ph7_create_function(&(*pVm),"token_get_all",PH7_builtin_token_get_all,0);` |
|   3879 | 1332 | `	ph7_create_function(&(*pVm),"token_name",PH7_builtin_token_name,0);` |
|   3879 | 1333 | `	return PH7_VmEvalBuiltinChunk(&(*pVm),zPhpTokenClass,sizeof(zPhpTokenClass)-1);` |
|      5 | 1334 | `}` |
|      - | 1335 |  |
|      - | 1336 | `#else /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 1337 |  |
|      - | 1338 | `/* Tiny build: no tokenizer builtins/class (the whole builtin layer is off). */` |
|      - | 1339 | `PH7_PRIVATE sxi32 PH7_VmInstallTokenizer(ph7_vm *pVm){ (void)pVm; return SXRET_OK; }` |
|      - | 1340 |  |
|      - | 1341 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 1342 |  |
