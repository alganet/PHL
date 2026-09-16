# src/ph7/vm_arg_check.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 375/384 lines (97.66%)

[Root index](../../index.md) | [Directory index](index.md)

|     Hits | Line | Source |
| -------: | ---: | :--- |
|        - |    1 | `/**` |
|        - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|        - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|        - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|        - |    5 | ` */` |
|        - |    6 | `#include "ph7int.h"` |
|        - |    7 | `/*` |
|        - |    8 | ` * Section:` |
|        - |    9 | ` *    Builtin-function argument checking: the aBuiltinArity[] min-arity` |
|        - |   10 | ` *    overrides, the aBuiltinSig[] PHP-8.5 signature table (the single` |
|        - |   11 | ` *    source of truth for builtin arity/types and Reflection), ZPP type` |
|        - |   12 | ` *    enforcement for host functions and the deprecation-notice machinery.` |
|        - |   13 | ` * Status:` |
|        - |   14 | ` *    Stable.` |
|        - |   15 | ` */` |
|        - |   16 | `/*` |
|        - |   17 | ` * PHP-8 builtin minimum-arity table (band A #5, stage 1).` |
|        - |   18 | ` *` |
|        - |   19 | ` * Native builtins carry no formal-parameter signature, so historically each` |
|        - |   20 | ` * one self-validated its argument count (or, worse, silently degraded to a` |
|        - |   21 | ` * bogus false/-1/"" return on too few arguments — a PH7-ism that diverges from` |
|        - |   22 | ` * PHP 8, which throws a catchable ArgumentCountError). This table is the single` |
|        - |   23 | ` * source of truth for the required minimum: at VM init VmSetBuiltinArity()` |
|        - |   24 | ` * stamps nMinArg/bAtLeast onto the matching ph7_user_func, and the OP_CALL` |
|        - |   25 | ` * choke point throws ArgumentCountError before the C routine ever runs.` |
|        - |   26 | ` *` |
|        - |   27 | ` * bAtLeast mirrors PHP's ZPP wording: "expects exactly N" when the builtin has` |
|        - |   28 | ` * no optional/variadic parameters (min == max), "expects at least N" otherwise.` |
|        - |   29 | ` * Every entry's min count and wording is byte-verified against php 8.5.7.` |
|        - |   30 | ` *` |
|        - |   31 | ` * Only functions that currently mis-behave (silent wrong return) are listed;` |
|        - |   32 | ` * builtins that already self-throw the correct message are intentionally left` |
|        - |   33 | ` * out so there is no double-check / message drift. The batch-1 block below is` |
|        - |   34 | ` * the original 31-function seed; the batch-2 block that follows completes the` |
|        - |   35 | ` * sweep across every remaining silent-degrading builtin (verified against the` |
|        - |   36 | ` * php 8.5.7 oracle).` |
|        - |   37 | ` */` |
|        - |   38 | `static const struct VmBuiltinArity {` |
|        - |   39 | `	const char *zName;   /* Builtin name (short, unqualified) */` |
|        - |   40 | `	sxi16 nMin;          /* Minimum required arguments */` |
|        - |   41 | `	sxu8 bAtLeast;       /* 0 -> "exactly", 1 -> "at least" */` |
|        - |   42 | `} aBuiltinArity[] = {` |
|        - |   43 | `	/* String family */` |
|        - |   44 | `	{ "substr",       2, 1 }, { "substr_count",  2, 1 }, { "str_repeat",     2, 0 },` |
|        - |   45 | `	{ "str_pad",      2, 1 }, { "strpos",        2, 1 }, { "stripos",        2, 1 },` |
|        - |   46 | `	{ "strrpos",      2, 1 }, { "strripos",      2, 1 }, { "strstr",         2, 1 },` |
|        - |   47 | `	{ "stristr",      2, 1 }, { "strrchr",       2, 1 }, { "str_replace",    3, 1 },` |
|        - |   48 | `	{ "str_ireplace", 3, 1 }, { "strncmp",       3, 0 }, { "strncasecmp",    3, 0 },` |
|        - |   49 | `	{ "substr_compare",3,1 }, { "strpbrk",       2, 0 }, { "strspn",         2, 1 },` |
|        - |   50 | `	{ "strcspn",      2, 1 }, { "hexdec",        1, 0 }, { "octdec",         1, 0 },` |
|        - |   51 | `	{ "bindec",       1, 0 }, { "chunk_split",   1, 1 },` |
|        - |   52 | `	/* Math family (atan2/intdiv already self-throw the same ArgumentCountError,` |
|        - |   53 | `	 * so they stay off the table per the disjointness rule above). */` |
|        - |   54 | `	{ "pow",          2, 0 }, { "fmod",          2, 0 }, { "hypot",          2, 0 },` |
|        - |   55 | `	{ "log",          1, 1 },` |
|        - |   56 | `	/* Array family (str_split already self-throws — kept off the table). */` |
|        - |   57 | `	{ "in_array",     2, 1 }, { "range",         2, 1 },` |
|        - |   58 | `	{ "implode",      1, 1 }, { "join",          1, 1 },` |
|        - |   59 | `	/*` |
|        - |   60 | `	 * Batch 2 (band A #5 continuation) — a systematic sweep of every remaining` |
|        - |   61 | `	 * builtin that silently degraded on too-few arguments where php 8 throws` |
|        - |   62 | `	 * ArgumentCountError. Each row's minimum and "exactly"/"at least" wording was` |
|        - |   63 | `	 * extracted from php 8.5.7's own ArgumentCountError message at the argument` |
|        - |   64 | `	 * boundary (the message text is byte-identical to the one this table drives).` |
|        - |   65 | `	 * Functions that already self-throw the correct message are still excluded per` |
|        - |   66 | `	 * the disjointness rule above.` |
|        - |   67 | `	 */` |
|        - |   68 | `	/* String family */` |
|        - |   69 | `	{ "chop",                      1, 1 },` |
|        - |   70 | `	{ "explode",                   2, 1 },` |
|        - |   71 | `	{ "fprintf",                   2, 1 },` |
|        - |   72 | `	{ "html_entity_decode",        1, 1 },` |
|        - |   73 | `	{ "htmlentities",              1, 1 },` |
|        - |   74 | `	{ "htmlspecialchars",          1, 1 },` |
|        - |   75 | `	{ "htmlspecialchars_decode",   1, 1 },` |
|        - |   76 | `	{ "lcfirst",                   1, 0 },` |
|        - |   77 | `	{ "ltrim",                     1, 1 },` |
|        - |   78 | `	{ "mb_check_encoding",         1, 0 },` |
|        - |   79 | `	{ "mb_chr",                    1, 1 },` |
|        - |   80 | `	{ "mb_convert_case",           2, 1 },` |
|        - |   81 | `	{ "mb_detect_encoding",        1, 1 },` |
|        - |   82 | `	{ "mb_ord",                    1, 1 },` |
|        - |   83 | `	{ "mb_str_split",              1, 1 },` |
|        - |   84 | `	{ "mb_stripos",                2, 1 },` |
|        - |   85 | `	{ "mb_strlen",                 1, 1 },` |
|        - |   86 | `	{ "mb_strpos",                 2, 1 },` |
|        - |   87 | `	{ "mb_strrpos",                2, 1 },` |
|        - |   88 | `	{ "mb_strtolower",             1, 1 },` |
|        - |   89 | `	{ "mb_strtoupper",             1, 1 },` |
|        - |   90 | `	{ "mb_strwidth",               1, 1 },` |
|        - |   91 | `	{ "mb_substr",                 2, 1 },` |
|        - |   92 | `	{ "nl2br",                     1, 1 },` |
|        - |   93 | `	{ "printf",                    1, 1 },` |
|        - |   94 | `	{ "quotemeta",                 1, 0 },` |
|        - |   95 | `	{ "rtrim",                     1, 1 },` |
|        - |   96 | `	{ "soundex",                   1, 0 },` |
|        - |   97 | `	{ "sprintf",                   1, 1 },` |
|        - |   98 | `	{ "str_getcsv",                1, 1 },` |
|        - |   99 | `	{ "str_shuffle",               1, 0 },` |
|        - |  100 | `	{ "strcasecmp",                2, 0 },` |
|        - |  101 | `	{ "strchr",                    2, 1 },` |
|        - |  102 | `	{ "strcmp",                    2, 0 },` |
|        - |  103 | `	{ "strnatcasecmp",             2, 0 },` |
|        - |  104 | `	{ "strnatcmp",                 2, 0 },` |
|        - |  105 | `	{ "strcoll",                   2, 0 },` |
|        - |  106 | `	{ "strip_tags",                1, 1 },` |
|        - |  107 | `	{ "stripslashes",              1, 0 },` |
|        - |  108 | `	{ "strlen",                    1, 0 },` |
|        - |  109 | `	{ "strrev",                    1, 0 },` |
|        - |  110 | `	{ "strtok",                    1, 1 },` |
|        - |  111 | `	{ "strtolower",                1, 0 },` |
|        - |  112 | `	{ "strtoupper",                1, 0 },` |
|        - |  113 | `	{ "strtr",                     2, 0 },` |
|        - |  114 | `	{ "trim",                      1, 1 },` |
|        - |  115 | `	{ "ucfirst",                   1, 0 },` |
|        - |  116 | `	{ "ucwords",                   1, 1 },` |
|        - |  117 | `	{ "vfprintf",                  3, 0 },` |
|        - |  118 | `	{ "vprintf",                   2, 0 },` |
|        - |  119 | `	{ "vsprintf",                  2, 0 },` |
|        - |  120 | `	{ "wordwrap",                  1, 1 },` |
|        - |  121 | `	/* Ctype family */` |
|        - |  122 | `	{ "ctype_alnum",               1, 0 },` |
|        - |  123 | `	{ "ctype_alpha",               1, 0 },` |
|        - |  124 | `	{ "ctype_cntrl",               1, 0 },` |
|        - |  125 | `	{ "ctype_digit",               1, 0 },` |
|        - |  126 | `	{ "ctype_graph",               1, 0 },` |
|        - |  127 | `	{ "ctype_lower",               1, 0 },` |
|        - |  128 | `	{ "ctype_print",               1, 0 },` |
|        - |  129 | `	{ "ctype_punct",               1, 0 },` |
|        - |  130 | `	{ "ctype_space",               1, 0 },` |
|        - |  131 | `	{ "ctype_upper",               1, 0 },` |
|        - |  132 | `	{ "ctype_xdigit",              1, 0 },` |
|        - |  133 | `	/* Math family */` |
|        - |  134 | `	{ "base_convert",              3, 0 },` |
|        - |  135 | `	{ "cos",                       1, 0 },` |
|        - |  136 | `	{ "cosh",                      1, 0 },` |
|        - |  137 | `	{ "crc32",                     1, 0 },` |
|        - |  138 | `	{ "decbin",                    1, 0 },` |
|        - |  139 | `	{ "dechex",                    1, 0 },` |
|        - |  140 | `	{ "decoct",                    1, 0 },` |
|        - |  141 | `	{ "exp",                       1, 0 },` |
|        - |  142 | `	{ "log10",                     1, 0 },` |
|        - |  143 | `	{ "md5",                       1, 1 },` |
|        - |  144 | `	{ "round",                     1, 1 },` |
|        - |  145 | `	{ "sha1",                      1, 1 },` |
|        - |  146 | `	{ "sin",                       1, 0 },` |
|        - |  147 | `	{ "sinh",                      1, 0 },` |
|        - |  148 | `	{ "sqrt",                      1, 0 },` |
|        - |  149 | `	{ "tan",                       1, 0 },` |
|        - |  150 | `	{ "tanh",                      1, 0 },` |
|        - |  151 | `	/* Type/var family */` |
|        - |  152 | `	{ "floatval",                  1, 0 },` |
|        - |  153 | `	{ "get_resource_type",         1, 0 },` |
|        - |  154 | `	{ "gettype",                   1, 0 },` |
|        - |  155 | `	{ "intval",                    1, 1 },` |
|        - |  156 | `	{ "is_array",                  1, 0 },` |
|        - |  157 | `	{ "is_bool",                   1, 0 },` |
|        - |  158 | `	{ "is_callable",               1, 1 },` |
|        - |  159 | `	{ "is_double",                 1, 0 },` |
|        - |  160 | `	{ "is_float",                  1, 0 },` |
|        - |  161 | `	{ "is_int",                    1, 0 },` |
|        - |  162 | `	{ "is_integer",                1, 0 },` |
|        - |  163 | `	{ "is_long",                   1, 0 },` |
|        - |  164 | `	{ "is_null",                   1, 0 },` |
|        - |  165 | `	{ "is_numeric",                1, 0 },` |
|        - |  166 | `	{ "is_object",                 1, 0 },` |
|        - |  167 | `	{ "is_resource",               1, 0 },` |
|        - |  168 | `	{ "is_scalar",                 1, 0 },` |
|        - |  169 | `	{ "is_string",                 1, 0 },` |
|        - |  170 | `	{ "print_r",                   1, 1 },` |
|        - |  171 | `	{ "strval",                    1, 0 },` |
|        - |  172 | `	{ "var_dump",                  1, 1 },` |
|        - |  173 | `	{ "var_export",                1, 1 },` |
|        - |  174 | `	/* Array/iterator family */` |
|        - |  175 | `	{ "array_filter",              1, 1 },` |
|        - |  176 | `	{ "array_product",             1, 0 },` |
|        - |  177 | `	{ "array_rand",                1, 1 },` |
|        - |  178 | `	{ "compact",                   1, 1 },` |
|        - |  179 | `	{ "current",                   1, 0 },` |
|        - |  180 | `	{ "end",                       1, 0 },` |
|        - |  181 | `	{ "extract",                   1, 1 },` |
|        - |  182 | `	{ "iterator_apply",            2, 1 },` |
|        - |  183 | `	{ "iterator_count",            1, 0 },` |
|        - |  184 | `	{ "iterator_to_array",         1, 1 },` |
|        - |  185 | `	{ "key",                       1, 0 },` |
|        - |  186 | `	{ "krsort",                    1, 1 },` |
|        - |  187 | `	{ "ksort",                     1, 1 },` |
|        - |  188 | `	{ "next",                      1, 0 },` |
|        - |  189 | `	{ "pos",                       1, 0 },` |
|        - |  190 | `	{ "prev",                      1, 0 },` |
|        - |  191 | `	{ "reset",                     1, 0 },` |
|        - |  192 | `	{ "rsort",                     1, 1 },` |
|        - |  193 | `	{ "shuffle",                   1, 0 },` |
|        - |  194 | `	{ "sort",                      1, 1 },` |
|        - |  195 | `	{ "uasort",                    2, 0 },` |
|        - |  196 | `	{ "uksort",                    2, 0 },` |
|        - |  197 | `	{ "usort",                     2, 0 },` |
|        - |  198 | `	/* Class/reflection family */` |
|        - |  199 | `	{ "class_alias",               2, 1 },` |
|        - |  200 | `	{ "class_exists",              1, 1 },` |
|        - |  201 | `	{ "enum_exists",               1, 1 },` |
|        - |  202 | `	{ "get_class_methods",         1, 0 },` |
|        - |  203 | `	{ "get_class_vars",            1, 0 },` |
|        - |  204 | `	{ "get_object_vars",           1, 0 },` |
|        - |  205 | `	{ "interface_exists",          1, 1 },` |
|        - |  206 | `	{ "trait_exists",              1, 1 },` |
|        - |  207 | `	{ "is_a",                      2, 1 },` |
|        - |  208 | `	{ "is_subclass_of",            2, 1 },` |
|        - |  209 | `	{ "method_exists",             2, 0 },` |
|        - |  210 | `	{ "property_exists",           2, 0 },` |
|        - |  211 | `	{ "spl_autoload",              1, 1 },` |
|        - |  212 | `	{ "spl_autoload_unregister",   1, 0 },` |
|        - |  213 | `	{ "spl_object_hash",           1, 0 },` |
|        - |  214 | `	{ "spl_object_id",             1, 0 },` |
|        - |  215 | `	/* Filesystem/IO family */` |
|        - |  216 | `	{ "basename",                  1, 1 },` |
|        - |  217 | `	{ "chdir",                     1, 0 },` |
|        - |  218 | `	{ "chgrp",                     2, 0 },` |
|        - |  219 | `	{ "dirname",                   1, 1 },` |
|        - |  220 | `	{ "disk_free_space",           1, 0 },` |
|        - |  221 | `	{ "disk_total_space",          1, 0 },` |
|        - |  222 | `	{ "diskfreespace",             1, 0 },` |
|        - |  223 | `	{ "fclose",                    1, 0 },` |
|        - |  224 | `	{ "feof",                      1, 0 },` |
|        - |  225 | `	{ "fflush",                    1, 0 },` |
|        - |  226 | `	{ "fgetc",                     1, 0 },` |
|        - |  227 | `	{ "fgetcsv",                   1, 1 },` |
|        - |  228 | `	{ "file",                      1, 1 },` |
|        - |  229 | `	{ "file_exists",               1, 0 },` |
|        - |  230 | `	{ "fileatime",                 1, 0 },` |
|        - |  231 | `	{ "filectime",                 1, 0 },` |
|        - |  232 | `	{ "filemtime",                 1, 0 },` |
|        - |  233 | `	{ "filesize",                  1, 0 },` |
|        - |  234 | `	{ "filetype",                  1, 0 },` |
|        - |  235 | `	{ "flock",                     2, 1 },` |
|        - |  236 | `	{ "fpassthru",                 1, 0 },` |
|        - |  237 | `	{ "fputcsv",                   2, 1 },` |
|        - |  238 | `	{ "fputs",                     2, 1 },` |
|        - |  239 | `	{ "fseek",                     2, 1 },` |
|        - |  240 | `	{ "fstat",                     1, 0 },` |
|        - |  241 | `	{ "ftell",                     1, 0 },` |
|        - |  242 | `	{ "ftruncate",                 2, 0 },` |
|        - |  243 | `	{ "getopt",                    1, 1 },` |
|        - |  244 | `	{ "is_dir",                    1, 0 },` |
|        - |  245 | `	{ "is_executable",             1, 0 },` |
|        - |  246 | `	{ "is_file",                   1, 0 },` |
|        - |  247 | `	{ "is_link",                   1, 0 },` |
|        - |  248 | `	{ "is_readable",               1, 0 },` |
|        - |  249 | `	{ "is_writable",               1, 0 },` |
|        - |  250 | `	{ "lstat",                     1, 0 },` |
|        - |  251 | `	{ "md5_file",                  1, 1 },` |
|        - |  252 | `	{ "opendir",                   1, 1 },` |
|        - |  253 | `	{ "pathinfo",                  1, 1 },` |
|        - |  254 | `	{ "pclose",                    1, 0 },` |
|        - |  255 | `	{ "realpath",                  1, 0 },` |
|        - |  256 | `	{ "rewind",                    1, 0 },` |
|        - |  257 | `	{ "sha1_file",                 1, 1 },` |
|        - |  258 | `	{ "stat",                      1, 0 },` |
|        - |  259 | `	/* Date family */` |
|        - |  260 | `	{ "date",                      1, 1 },` |
|        - |  261 | `	{ "date_default_timezone_set", 1, 1 },` |
|        - |  262 | `	{ "gmdate",                    1, 1 },` |
|        - |  263 | `	{ "gmmktime",                  1, 1 },` |
|        - |  264 | `	{ "idate",                     1, 1 },` |
|        - |  265 | `	{ "mktime",                    1, 1 },` |
|        - |  266 | `	/* Encoding/URL family */` |
|        - |  267 | `	{ "base64_decode",             1, 1 },` |
|        - |  268 | `	{ "base64_encode",             1, 0 },` |
|        - |  269 | `	{ "convert_uudecode",          1, 0 },` |
|        - |  270 | `	{ "convert_uuencode",          1, 0 },` |
|        - |  271 | `	{ "parse_ini_file",            1, 1 },` |
|        - |  272 | `	{ "parse_ini_string",          1, 1 },` |
|        - |  273 | `	{ "parse_url",                 1, 1 },` |
|        - |  274 | `	{ "rawurldecode",              1, 0 },` |
|        - |  275 | `	{ "rawurlencode",              1, 0 },` |
|        - |  276 | `	{ "urldecode",                 1, 0 },` |
|        - |  277 | `	{ "urlencode",                 1, 0 },` |
|        - |  278 | `	/* JSON/serialize family */` |
|        - |  279 | `	{ "filter_var",                1, 1 },` |
|        - |  280 | `	{ "json_decode",               1, 1 },` |
|        - |  281 | `	{ "json_encode",               1, 1 },` |
|        - |  282 | `	{ "json_validate",             1, 1 },` |
|        - |  283 | `	{ "serialize",                 1, 0 },` |
|        - |  284 | `	{ "unserialize",               1, 1 },` |
|        - |  285 | `	/* PCRE family */` |
|        - |  286 | `	{ "preg_match",                2, 1 },` |
|        - |  287 | `	{ "preg_match_all",            2, 1 },` |
|        - |  288 | `	{ "preg_quote",                1, 1 },` |
|        - |  289 | `	{ "preg_replace",              3, 1 },` |
|        - |  290 | `	{ "preg_replace_callback",     3, 1 },` |
|        - |  291 | `	{ "preg_split",                2, 1 },` |
|        - |  292 | `	/* XML family */` |
|        - |  293 | `	/* Constants/misc family */` |
|        - |  294 | `	{ "call_user_func",            1, 1 },` |
|        - |  295 | `	{ "call_user_func_array",      2, 0 },` |
|        - |  296 | `	{ "constant",                  1, 0 },` |
|        - |  297 | `	{ "define",                    2, 1 },` |
|        - |  298 | `	{ "defined",                   1, 0 },` |
|        - |  299 | `	{ "error_log",                 1, 1 },` |
|        - |  300 | `	{ "fnmatch",                   2, 1 },` |
|        - |  301 | `	{ "forward_static_call",       1, 1 },` |
|        - |  302 | `	{ "forward_static_call_array", 2, 0 },` |
|        - |  303 | `	{ "func_get_arg",              1, 0 },` |
|        - |  304 | `	{ "function_exists",           1, 0 },` |
|        - |  305 | `	{ "header",                    1, 1 },` |
|        - |  306 | `	{ "password_get_info",         1, 0 },` |
|        - |  307 | `	{ "putenv",                    1, 0 },` |
|        - |  308 | `	{ "register_shutdown_function", 1, 1 },` |
|        - |  309 | `	{ "set_error_handler",         1, 1 },` |
|        - |  310 | `	{ "set_exception_handler",     1, 0 },` |
|        - |  311 | `	{ "setcookie",                 1, 1 },` |
|        - |  312 | `	{ "setrawcookie",              1, 1 },` |
|        - |  313 | `	{ "trigger_error",             1, 1 },` |
|        - |  314 | `	{ "user_error",                1, 1 },` |
|        - |  315 | `	/*` |
|        - |  316 | `	 * Overrides for signatures that under-report their own minimum: the callback` |
|        - |  317 | `	 * of these three hides inside the variadic tail ("array $array, ...$rest"),` |
|        - |  318 | `	 * so the derivation reads 1 where php requires 2.` |
|        - |  319 | `	 */` |
|        - |  320 | `	{ "array_udiff",               2, 1 },` |
|        - |  321 | `	{ "array_uintersect",          2, 1 },` |
|        - |  322 | `	{ "array_diff_uassoc",         2, 1 },` |
|        - |  323 | `};` |
|        - |  324 | `/*` |
|        - |  325 | ` * Stamp the minimum-arity metadata from aBuiltinArity[] onto the already` |
|        - |  326 | ` * registered host functions. Called once at VM init after every builtin family` |
|        - |  327 | ` * has been installed into hHostFunction. A name absent from the hash (e.g. a` |
|        - |  328 | ` * build without a given extension) is simply skipped.` |
|        - |  329 | ` */` |
|     3406 |  330 | `PH7_PRIVATE void VmSetBuiltinArity(ph7_vm *pVm)` |
|        5 |  331 | `{` |
|        - |  332 | `	sxu32 n;` |
|   916219 |  333 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinArity) ; ++n ){` |
|   912813 |  334 | `		const struct VmBuiltinArity *p = &aBuiltinArity[n];` |
|  1825621 |  335 | `		SyHashEntry *pEntry = SyHashGet(&pVm->hHostFunction,` |
|   912808 |  336 | `			(const void *)p->zName,SyStrlen(p->zName));` |
|   912813 |  337 | `		if( pEntry ){` |
|   912813 |  338 | `			ph7_user_func *pFunc = (ph7_user_func *)pEntry->pUserData;` |
|   912813 |  339 | `			pFunc->nMinArg  = p->nMin;` |
|   912813 |  340 | `			pFunc->bAtLeast = p->bAtLeast;` |
|   456404 |  341 | `		}` |
|   456409 |  342 | `	}` |
|     3411 |  343 | `}` |
|        - |  344 | `/*` |
|        - |  345 | ` * PHP 8.5 parameter signatures for the C builtins, generated offline from` |
|        - |  346 | ` * a real PHP 8.5 ReflectionFunction dump over PHL's registered function` |
|        - |  347 | ` * list (see the plan's Reflection section). "= ?" marks an optional` |
|        - |  348 | ` * parameter whose default is not representable as a short literal.` |
|        - |  349 | ` * Reflection parses these strings on demand; unlisted builtins degrade to` |
|        - |  350 | ` * the min-arity data.` |
|        - |  351 | ` */` |
|        - |  352 | `static const struct VmBuiltinSig {` |
|        - |  353 | `	const char *zName;` |
|        - |  354 | `	const char *zSig;` |
|        - |  355 | `	const char *zRet;` |
|        - |  356 | `} aBuiltinSig[] = {` |
|        - |  357 | `	{ "abs", "int\|float $num", "int\|float" },` |
|        - |  358 | `	{ "acos", "float $num", "float" },` |
|        - |  359 | `	{ "addcslashes", "string $string, string $characters", "string" },` |
|        - |  360 | `	{ "addslashes", "string $string", "string" },` |
|        - |  361 | `	{ "array_all", "array $array, callable $callback", "bool" },` |
|        - |  362 | `	{ "array_any", "array $array, callable $callback", "bool" },` |
|        - |  363 | `	{ "array_chunk", "array $array, int $length, bool $preserve_keys = false", "array" },` |
|        - |  364 | `	{ "array_column", "array $array, string\|int\|null $column_key, string\|int\|null $index_key = NULL", "array" },` |
|        - |  365 | `	{ "array_combine", "array $keys, array $values", "array" },` |
|        - |  366 | `	{ "array_diff", "array $array, array ...$arrays = ?", "array" },` |
|        - |  367 | `	{ "array_diff_assoc", "array $array, array ...$arrays = ?", "array" },` |
|        - |  368 | `	{ "array_diff_key", "array $array, array ...$arrays = ?", "array" },` |
|        - |  369 | `	{ "array_diff_uassoc", "array $array, ...$rest = ?", "array" },` |
|        - |  370 | `	{ "array_fill", "int $start_index, int $count, mixed $value", "array" },` |
|        - |  371 | `	{ "array_fill_keys", "array $keys, mixed $value", "array" },` |
|        - |  372 | `	{ "array_filter", "array $array, ?callable $callback = NULL, int $mode = 0", "array" },` |
|        - |  373 | `	{ "array_find", "array $array, callable $callback", "mixed" },` |
|        - |  374 | `	{ "array_find_key", "array $array, callable $callback", "mixed" },` |
|        - |  375 | `	{ "array_first", "array $array", "mixed" },` |
|        - |  376 | `	{ "array_flip", "array $array", "array" },` |
|        - |  377 | `	{ "array_intersect", "array $array, array ...$arrays = ?", "array" },` |
|        - |  378 | `	{ "array_intersect_assoc", "array $array, array ...$arrays = ?", "array" },` |
|        - |  379 | `	{ "array_intersect_key", "array $array, array ...$arrays = ?", "array" },` |
|        - |  380 | `	{ "array_is_list", "array $array", "bool" },` |
|        - |  381 | `	{ "array_key_exists", "$key, array $array", "bool" },` |
|        - |  382 | `	{ "array_key_first", "array $array", "string\|int\|null" },` |
|        - |  383 | `	{ "array_key_last", "array $array", "string\|int\|null" },` |
|        - |  384 | `	{ "array_keys", "array $array, mixed $filter_value = ?, bool $strict = false", "array" },` |
|        - |  385 | `	{ "array_last", "array $array", "mixed" },` |
|        - |  386 | `	{ "array_map", "?callable $callback, array $array, array ...$arrays = ?", "array" },` |
|        - |  387 | `	{ "array_merge", "array ...$arrays = ?", "array" },` |
|        - |  388 | `	{ "array_pad", "array $array, int $length, mixed $value", "array" },` |
|        - |  389 | `	{ "array_pop", "array &$array", "mixed" },` |
|        - |  390 | `	{ "array_product", "array $array", "int\|float" },` |
|        - |  391 | `	{ "array_push", "array &$array, mixed ...$values = ?", "int" },` |
|        - |  392 | `	{ "array_rand", "array $array, int $num = 1", "array\|string\|int" },` |
|        - |  393 | `	{ "array_reduce", "array $array, callable $callback, mixed $initial = NULL", "mixed" },` |
|        - |  394 | `	{ "array_replace", "array $array, array ...$replacements = ?", "array" },` |
|        - |  395 | `	{ "array_reverse", "array $array, bool $preserve_keys = false", "array" },` |
|        - |  396 | `	{ "array_search", "mixed $needle, array $haystack, bool $strict = false", "string\|int\|false" },` |
|        - |  397 | `	{ "array_shift", "array &$array", "mixed" },` |
|        - |  398 | `	{ "array_slice", "array $array, int $offset, ?int $length = NULL, bool $preserve_keys = false", "array" },` |
|        - |  399 | `	{ "array_splice", "array &$array, int $offset, ?int $length = NULL, mixed $replacement = ?", "array" },` |
|        - |  400 | `	{ "array_sum", "array $array", "int\|float" },` |
|        - |  401 | `	{ "array_udiff", "array $array, ...$rest = ?", "array" },` |
|        - |  402 | `	{ "array_uintersect", "array $array, ...$rest = ?", "array" },` |
|        - |  403 | `	{ "array_unique", "array $array, int $flags = 2", "array" },` |
|        - |  404 | `	{ "array_values", "array $array", "array" },` |
|        - |  405 | `	{ "array_walk", "object\|array &$array, callable $callback, mixed $arg = ?", "true" },` |
|        - |  406 | `	{ "array_walk_recursive", "object\|array &$array, callable $callback, mixed $arg = ?", "true" },` |
|        - |  407 | `	{ "arsort", "array &$array, int $flags = 0", "true" },` |
|        - |  408 | `	{ "asin", "float $num", "float" },` |
|        - |  409 | `	{ "asort", "array &$array, int $flags = 0", "true" },` |
|        - |  410 | `	{ "assert", "mixed $assertion, Throwable\|string\|null $description = NULL", "bool" },` |
|        - |  411 | `	{ "atan", "float $num", "float" },` |
|        - |  412 | `	{ "atan2", "float $y, float $x", "float" },` |
|        - |  413 | `	{ "base64_decode", "string $string, bool $strict = false", "string\|false" },` |
|        - |  414 | `	{ "base64_encode", "string $string", "string" },` |
|        - |  415 | `	{ "base_convert", "string $num, int $from_base, int $to_base", "string" },` |
|        - |  416 | `	{ "basename", "string $path, string $suffix = ''", "string" },` |
|        - |  417 | `	{ "bin2hex", "string $string", "string" },` |
|        - |  418 | `	{ "bindec", "string $binary_string", "int\|float" },` |
|        - |  419 | `	{ "boolval", "mixed $value", "bool" },` |
|        - |  420 | `	{ "call_user_func", "callable $callback, mixed ...$args = ?", "mixed" },` |
|        - |  421 | `	{ "call_user_func_array", "callable $callback, array $args", "mixed" },` |
|        - |  422 | `	{ "ceil", "int\|float $num", "float" },` |
|        - |  423 | `	{ "chdir", "string $directory", "bool" },` |
|        - |  424 | `	{ "chgrp", "string $filename, string\|int $group", "bool" },` |
|        - |  425 | `	{ "chmod", "string $filename, int $permissions", "bool" },` |
|        - |  426 | `	{ "chop", "string $string, string $characters = ?", "string" },` |
|        - |  427 | `	{ "chown", "string $filename, string\|int $user", "bool" },` |
|        - |  428 | `	{ "chr", "int $codepoint", "string" },` |
|        - |  429 | `	{ "chunk_split", "string $string, int $length = 76, string $separator = ?", "string" },` |
|        - |  430 | `	{ "class_alias", "string $class, string $alias, bool $autoload = true", "bool" },` |
|        - |  431 | `	{ "class_exists", "string $class, bool $autoload = true", "bool" },` |
|        - |  432 | `	{ "enum_exists", "string $enum, bool $autoload = true", "bool" },` |
|        - |  433 | `	{ "closedir", "$dir_handle = NULL", "void" },` |
|        - |  434 | `	{ "compact", "$var_name, ...$var_names = ?", "array" },` |
|        - |  435 | `	{ "constant", "string $name", "mixed" },` |
|        - |  436 | `	{ "convert_uudecode", "string $string", "string\|false" },` |
|        - |  437 | `	{ "convert_uuencode", "string $string", "string" },` |
|        - |  438 | `	{ "copy", "string $from, string $to, $context = NULL", "bool" },` |
|        - |  439 | `	{ "cos", "float $num", "float" },` |
|        - |  440 | `	{ "cosh", "float $num", "float" },` |
|        - |  441 | `	{ "count", "Countable\|array $value, int $mode = 0", "int" },` |
|        - |  442 | `	{ "crc32", "string $string", "int" },` |
|        - |  443 | `	{ "ctype_alnum", "mixed $text", "bool" },` |
|        - |  444 | `	{ "ctype_alpha", "mixed $text", "bool" },` |
|        - |  445 | `	{ "ctype_cntrl", "mixed $text", "bool" },` |
|        - |  446 | `	{ "ctype_digit", "mixed $text", "bool" },` |
|        - |  447 | `	{ "ctype_graph", "mixed $text", "bool" },` |
|        - |  448 | `	{ "ctype_lower", "mixed $text", "bool" },` |
|        - |  449 | `	{ "ctype_print", "mixed $text", "bool" },` |
|        - |  450 | `	{ "ctype_punct", "mixed $text", "bool" },` |
|        - |  451 | `	{ "ctype_space", "mixed $text", "bool" },` |
|        - |  452 | `	{ "ctype_upper", "mixed $text", "bool" },` |
|        - |  453 | `	{ "ctype_xdigit", "mixed $text", "bool" },` |
|        - |  454 | `	{ "current", "object\|array $array", "mixed" },` |
|        - |  455 | `	{ "date", "string $format, ?int $timestamp = NULL", "string" },` |
|        - |  456 | `	{ "date_default_timezone_get", "", "string" },` |
|        - |  457 | `	{ "date_default_timezone_set", "string $timezoneId", "bool" },` |
|        - |  458 | `	{ "debug_backtrace", "int $options = 1, int $limit = 0", "array" },` |
|        - |  459 | `	{ "debug_print_backtrace", "int $options = 0, int $limit = 0", "void" },` |
|        - |  460 | `	{ "decbin", "int $num", "string" },` |
|        - |  461 | `	{ "dechex", "int $num", "string" },` |
|        - |  462 | `	{ "decoct", "int $num", "string" },` |
|        - |  463 | `	{ "define", "string $constant_name, mixed $value, bool $case_insensitive = false", "bool" },` |
|        - |  464 | `	{ "defined", "string $constant_name", "bool" },` |
|        - |  465 | `	{ "die", "string\|int $status = 0", "never" },` |
|        - |  466 | `	{ "dirname", "string $path, int $levels = 1", "string" },` |
|        - |  467 | `	{ "disk_free_space", "string $directory", "float\|false" },` |
|        - |  468 | `	{ "disk_total_space", "string $directory", "float\|false" },` |
|        - |  469 | `	{ "diskfreespace", "string $directory", "float\|false" },` |
|        - |  470 | `	{ "end", "object\|array &$array", "mixed" },` |
|        - |  471 | `	{ "error_get_last", "", "?array" },` |
|        - |  472 | `	{ "error_clear_last", "", "void" },` |
|        - |  473 | `	{ "error_log", "string $message, int $message_type = 0, ?string $destination = NULL, ?string $additional_headers = NULL", "bool" },` |
|        - |  474 | `	{ "error_reporting", "?int $error_level = NULL", "int" },` |
|        - |  475 | `	{ "exit", "string\|int $status = 0", "never" },` |
|        - |  476 | `	{ "exp", "float $num", "float" },` |
|        - |  477 | `	{ "explode", "string $separator, string $string, int $limit = 9223372036854775807", "array" },` |
|        - |  478 | `	{ "extract", "array &$array, int $flags = 0, string $prefix = ''", "int" },` |
|        - |  479 | `	{ "fclose", "$stream", "bool" },` |
|        - |  480 | `	{ "feof", "$stream", "bool" },` |
|        - |  481 | `	{ "fflush", "$stream", "bool" },` |
|        - |  482 | `	{ "fgetc", "$stream", "string\|false" },` |
|        - |  483 | `	{ "fgetcsv", "$stream, ?int $length = NULL, string $separator = ',', string $enclosure = '\"', string $escape = '\\\\'", "array\|false" },` |
|        - |  484 | `	{ "fgets", "$stream, ?int $length = NULL", "string\|false" },` |
|        - |  485 | `	{ "file", "string $filename, int $flags = 0, $context = NULL", "array\|false" },` |
|        - |  486 | `	{ "file_exists", "string $filename", "bool" },` |
|        - |  487 | `	{ "file_get_contents", "string $filename, bool $use_include_path = false, $context = NULL, int $offset = 0, ?int $length = NULL", "string\|false" },` |
|        - |  488 | `	{ "file_put_contents", "string $filename, mixed $data, int $flags = 0, $context = NULL", "int\|false" },` |
|        - |  489 | `	{ "fileatime", "string $filename", "int\|false" },` |
|        - |  490 | `	{ "filectime", "string $filename", "int\|false" },` |
|        - |  491 | `	{ "filemtime", "string $filename", "int\|false" },` |
|        - |  492 | `	{ "filesize", "string $filename", "int\|false" },` |
|        - |  493 | `	{ "filetype", "string $filename", "string\|false" },` |
|        - |  494 | `	{ "filter_input", "int $type, string $var_name, int $filter = 516, array\|int $options = 0", "mixed" },` |
|        - |  495 | `	{ "filter_var", "mixed $value, int $filter = 516, array\|int $options = 0", "mixed" },` |
|        - |  496 | `	{ "floatval", "mixed $value", "float" },` |
|        - |  497 | `	{ "flock", "$stream, int $operation, &$would_block = NULL", "bool" },` |
|        - |  498 | `	{ "floor", "int\|float $num", "float" },` |
|        - |  499 | `	{ "flush", "", "void" },` |
|        - |  500 | `	{ "fmod", "float $num1, float $num2", "float" },` |
|        - |  501 | `	{ "fnmatch", "string $pattern, string $filename, int $flags = 0", "bool" },` |
|        - |  502 | `	{ "fopen", "string $filename, string $mode, bool $use_include_path = false, $context = NULL", "" },` |
|        - |  503 | `	{ "forward_static_call", "callable $callback, mixed ...$args = ?", "mixed" },` |
|        - |  504 | `	{ "forward_static_call_array", "callable $callback, array $args", "mixed" },` |
|        - |  505 | `	{ "fpassthru", "$stream", "int" },` |
|        - |  506 | `	{ "fprintf", "$stream, string $format, mixed ...$values = ?", "int" },` |
|        - |  507 | `	{ "fputcsv", "$stream, array $fields, string $separator = ',', string $enclosure = '\"', string $escape = '\\\\', string $eol = ?", "int\|false" },` |
|        - |  508 | `	{ "fputs", "$stream, string $data, ?int $length = NULL", "int\|false" },` |
|        - |  509 | `	{ "fread", "$stream, int $length", "string\|false" },` |
|        - |  510 | `	{ "fseek", "$stream, int $offset, int $whence = 0", "int" },` |
|        - |  511 | `	{ "fstat", "$stream", "array\|false" },` |
|        - |  512 | `	{ "ftell", "$stream", "int\|false" },` |
|        - |  513 | `	{ "ftruncate", "$stream, int $size", "bool" },` |
|        - |  514 | `	{ "func_get_arg", "int $position", "mixed" },` |
|        - |  515 | `	{ "func_get_args", "", "array" },` |
|        - |  516 | `	{ "func_num_args", "", "int" },` |
|        - |  517 | `	{ "function_exists", "string $function", "bool" },` |
|        - |  518 | `	{ "fwrite", "$stream, string $data, ?int $length = NULL", "int\|false" },` |
|        - |  519 | `	{ "gc_collect_cycles", "", "int" },` |
|        - |  520 | `	{ "gc_disable", "", "void" },` |
|        - |  521 | `	{ "gc_enable", "", "void" },` |
|        - |  522 | `	{ "gc_enabled", "", "bool" },` |
|        - |  523 | `	{ "gc_mem_caches", "", "int" },` |
|        - |  524 | `	{ "gc_status", "", "array" },` |
|        - |  525 | `	{ "get_called_class", "", "string" },` |
|        - |  526 | `	{ "get_class", "object $object = ?", "string" },` |
|        - |  527 | `	{ "get_class_methods", "object\|string $object_or_class", "array" },` |
|        - |  528 | `	{ "get_class_vars", "string $class", "array" },` |
|        - |  529 | `	{ "get_current_user", "", "string" },` |
|        - |  530 | `	{ "get_declared_classes", "", "array" },` |
|        - |  531 | `	{ "get_declared_interfaces", "", "array" },` |
|        - |  532 | `	{ "get_defined_constants", "bool $categorize = false", "array" },` |
|        - |  533 | `	{ "get_defined_functions", "bool $exclude_disabled = true", "array" },` |
|        - |  534 | `	{ "get_defined_vars", "", "array" },` |
|        - |  535 | `	{ "get_html_translation_table", "int $table = 0, int $flags = 11, string $encoding = 'UTF-8'", "array" },` |
|        - |  536 | `	{ "get_include_path", "", "string\|false" },` |
|        - |  537 | `	{ "get_included_files", "", "array" },` |
|        - |  538 | `	{ "get_object_vars", "object $object", "array" },` |
|        - |  539 | `	{ "get_parent_class", "object\|string $object_or_class = ?", "string\|false" },` |
|        - |  540 | `	{ "get_resource_type", "$resource", "string" },` |
|        - |  541 | `	{ "getcwd", "", "string\|false" },` |
|        - |  542 | `	{ "getdate", "?int $timestamp = NULL", "array" },` |
|        - |  543 | `	{ "getenv", "?string $name = NULL, bool $local_only = false", "array\|string\|false" },` |
|        - |  544 | `	{ "getmygid", "", "int\|false" },` |
|        - |  545 | `	{ "getmypid", "", "int\|false" },` |
|        - |  546 | `	{ "getmyuid", "", "int\|false" },` |
|        - |  547 | `	{ "getopt", "string $short_options, array $long_options = ?, &$rest_index = NULL", "array\|false" },` |
|        - |  548 | `	{ "getrandmax", "", "int" },` |
|        - |  549 | `	{ "gettimeofday", "bool $as_float = false", "array\|float" },` |
|        - |  550 | `	{ "gettype", "mixed $value", "string" },` |
|        - |  551 | `	{ "gmdate", "string $format, ?int $timestamp = NULL", "string" },` |
|        - |  552 | `	{ "gmmktime", "int $hour, ?int $minute = NULL, ?int $second = NULL, ?int $month = NULL, ?int $day = NULL, ?int $year = NULL", "int\|false" },` |
|        - |  553 | `	{ "hash", "string $algo, string $data, bool $binary = false, array $options = ?", "string" },` |
|        - |  554 | `	{ "hash_algos", "", "array" },` |
|        - |  555 | `	{ "hash_equals", "string $known_string, string $user_string", "bool" },` |
|        - |  556 | `	{ "hash_hmac", "string $algo, string $data, string $key, bool $binary = false", "string" },` |
|        - |  557 | `	{ "header", "string $header, bool $replace = true, int $response_code = 0", "void" },` |
|        - |  558 | `	{ "header_remove", "?string $name = NULL", "void" },` |
|        - |  559 | `	{ "headers_list", "", "array" },` |
|        - |  560 | `	{ "headers_sent", "&$filename = NULL, &$line = NULL", "bool" },` |
|        - |  561 | `	{ "hexdec", "string $hex_string", "int\|float" },` |
|        - |  562 | `	{ "html_entity_decode", "string $string, int $flags = 11, ?string $encoding = NULL", "string" },` |
|        - |  563 | `	{ "htmlentities", "string $string, int $flags = 11, ?string $encoding = NULL, bool $double_encode = true", "string" },` |
|        - |  564 | `	{ "htmlspecialchars", "string $string, int $flags = 11, ?string $encoding = NULL, bool $double_encode = true", "string" },` |
|        - |  565 | `	{ "htmlspecialchars_decode", "string $string, int $flags = 11", "string" },` |
|        - |  566 | `	{ "http_response_code", "int $response_code = 0", "int\|bool" },` |
|        - |  567 | `	{ "hypot", "float $x, float $y", "float" },` |
|        - |  568 | `	{ "idate", "string $format, ?int $timestamp = NULL", "int\|false" },` |
|        - |  569 | `	{ "implode", "array\|string $separator, ?array $array = NULL", "string" },` |
|        - |  570 | `	{ "in_array", "mixed $needle, array $haystack, bool $strict = false", "bool" },` |
|        - |  571 | `	{ "intdiv", "int $num1, int $num2", "int" },` |
|        - |  572 | `	{ "interface_exists", "string $interface, bool $autoload = true", "bool" },` |
|        - |  573 | `	{ "trait_exists", "string $trait, bool $autoload = true", "bool" },` |
|        - |  574 | `	{ "intval", "mixed $value, int $base = 10", "int" },` |
|        - |  575 | `	{ "is_a", "mixed $object_or_class, string $class, bool $allow_string = false", "bool" },` |
|        - |  576 | `	{ "is_array", "mixed $value", "bool" },` |
|        - |  577 | `	{ "is_bool", "mixed $value", "bool" },` |
|        - |  578 | `	{ "is_callable", "mixed $value, bool $syntax_only = false, &$callable_name = NULL", "bool" },` |
|        - |  579 | `	{ "is_dir", "string $filename", "bool" },` |
|        - |  580 | `	{ "is_double", "mixed $value", "bool" },` |
|        - |  581 | `	{ "is_executable", "string $filename", "bool" },` |
|        - |  582 | `	{ "is_file", "string $filename", "bool" },` |
|        - |  583 | `	{ "is_float", "mixed $value", "bool" },` |
|        - |  584 | `	{ "is_int", "mixed $value", "bool" },` |
|        - |  585 | `	{ "is_integer", "mixed $value", "bool" },` |
|        - |  586 | `	{ "is_link", "string $filename", "bool" },` |
|        - |  587 | `	{ "is_long", "mixed $value", "bool" },` |
|        - |  588 | `	{ "is_null", "mixed $value", "bool" },` |
|        - |  589 | `	{ "is_numeric", "mixed $value", "bool" },` |
|        - |  590 | `	{ "is_object", "mixed $value", "bool" },` |
|        - |  591 | `	{ "is_readable", "string $filename", "bool" },` |
|        - |  592 | `	{ "is_resource", "mixed $value", "bool" },` |
|        - |  593 | `	{ "is_scalar", "mixed $value", "bool" },` |
|        - |  594 | `	{ "is_string", "mixed $value", "bool" },` |
|        - |  595 | `	{ "is_subclass_of", "mixed $object_or_class, string $class, bool $allow_string = true", "bool" },` |
|        - |  596 | `	{ "is_writable", "string $filename", "bool" },` |
|        - |  597 | `	{ "iterator_apply", "Traversable $iterator, callable $callback, ?array $args = NULL", "int" },` |
|        - |  598 | `	{ "iterator_count", "Traversable\|array $iterator", "int" },` |
|        - |  599 | `	{ "iterator_to_array", "Traversable\|array $iterator, bool $preserve_keys = true", "array" },` |
|        - |  600 | `	{ "join", "array\|string $separator, ?array $array = NULL", "string" },` |
|        - |  601 | `	{ "json_decode", "string $json, ?bool $associative = NULL, int $depth = 512, int $flags = 0", "mixed" },` |
|        - |  602 | `	{ "json_encode", "mixed $value, int $flags = 0, int $depth = 512", "string\|false" },` |
|        - |  603 | `	{ "json_last_error", "", "int" },` |
|        - |  604 | `	{ "json_last_error_msg", "", "string" },` |
|        - |  605 | `	{ "json_validate", "string $json, int $depth = 512, int $flags = 0", "bool" },` |
|        - |  606 | `	{ "key", "object\|array $array", "string\|int\|null" },` |
|        - |  607 | `	{ "krsort", "array &$array, int $flags = 0", "true" },` |
|        - |  608 | `	{ "ksort", "array &$array, int $flags = 0", "true" },` |
|        - |  609 | `	{ "lcfirst", "string $string", "string" },` |
|        - |  610 | `	{ "levenshtein", "string $string1, string $string2, int $insertion_cost = 1, int $replacement_cost = 1, int $deletion_cost = 1", "int" },` |
|        - |  611 | `	{ "link", "string $target, string $link", "bool" },` |
|        - |  612 | `	{ "localtime", "?int $timestamp = NULL, bool $associative = false", "array" },` |
|        - |  613 | `	{ "log", "float $num, float $base = 2.718281828459045", "float" },` |
|        - |  614 | `	{ "log10", "float $num", "float" },` |
|        - |  615 | `	{ "lstat", "string $filename", "array\|false" },` |
|        - |  616 | `	{ "ltrim", "string $string, string $characters = ?", "string" },` |
|        - |  617 | `	{ "max", "mixed $value, mixed ...$values = ?", "mixed" },` |
|        - |  618 | `	{ "mb_chr", "int $codepoint, ?string $encoding = NULL", "string\|false" },` |
|        - |  619 | `	{ "mb_ord", "string $string, ?string $encoding = NULL", "int\|false" },` |
|        - |  620 | `	{ "mb_strtolower", "string $string, ?string $encoding = NULL", "string" },` |
|        - |  621 | `	{ "mb_strtoupper", "string $string, ?string $encoding = NULL", "string" },` |
|        - |  622 | `	{ "md5", "string $string, bool $binary = false", "string" },` |
|        - |  623 | `	{ "md5_file", "string $filename, bool $binary = false", "string\|false" },` |
|        - |  624 | `	{ "method_exists", "$object_or_class, string $method", "bool" },` |
|        - |  625 | `	{ "memory_get_peak_usage", "bool $real_usage = false", "int" },` |
|        - |  626 | `	{ "memory_get_usage", "bool $real_usage = false", "int" },` |
|        - |  627 | `	{ "microtime", "bool $as_float = false", "string\|float" },` |
|        - |  628 | `	{ "min", "mixed $value, mixed ...$values = ?", "mixed" },` |
|        - |  629 | `	{ "mkdir", "string $directory, int $permissions = 511, bool $recursive = false, $context = NULL", "bool" },` |
|        - |  630 | `	{ "mktime", "int $hour, ?int $minute = NULL, ?int $second = NULL, ?int $month = NULL, ?int $day = NULL, ?int $year = NULL", "int\|false" },` |
|        - |  631 | `	{ "mt_getrandmax", "", "int" },` |
|        - |  632 | `	{ "mt_rand", "int $min = ?, int $max = ?", "int" },` |
|        - |  633 | `	{ "mt_srand", "?int $seed = NULL, int $mode = 0", "void" },` |
|        - |  634 | `	{ "next", "object\|array &$array", "mixed" },` |
|        - |  635 | `	{ "nl2br", "string $string, bool $use_xhtml = true", "string" },` |
|        - |  636 | `	{ "ob_clean", "", "bool" },` |
|        - |  637 | `	{ "ob_end_clean", "", "bool" },` |
|        - |  638 | `	{ "ob_end_flush", "", "bool" },` |
|        - |  639 | `	{ "ob_flush", "", "bool" },` |
|        - |  640 | `	{ "ob_get_clean", "", "string\|false" },` |
|        - |  641 | `	{ "ob_get_contents", "", "string\|false" },` |
|        - |  642 | `	{ "ob_get_flush", "", "string\|false" },` |
|        - |  643 | `	{ "ob_get_length", "", "int\|false" },` |
|        - |  644 | `	{ "ob_get_level", "", "int" },` |
|        - |  645 | `	{ "ob_implicit_flush", "bool $enable = true", "void" },` |
|        - |  646 | `	{ "ob_list_handlers", "", "array" },` |
|        - |  647 | `	{ "ob_start", "$callback = NULL, int $chunk_size = 0, int $flags = 112", "bool" },` |
|        - |  648 | `	{ "octdec", "string $octal_string", "int\|float" },` |
|        - |  649 | `	{ "opendir", "string $directory, $context = NULL", "" },` |
|        - |  650 | `	{ "ord", "string $character", "int" },` |
|        - |  651 | `	{ "parse_ini_file", "string $filename, bool $process_sections = false, int $scanner_mode = 0", "array\|false" },` |
|        - |  652 | `	{ "parse_ini_string", "string $ini_string, bool $process_sections = false, int $scanner_mode = 0", "array\|false" },` |
|        - |  653 | `	{ "parse_url", "string $url, int $component = -1", "array\|string\|int\|false\|null" },` |
|        - |  654 | `	{ "password_get_info", "string $hash", "array" },` |
|        - |  655 | `	{ "password_hash", "string $password, string\|int\|null $algo, array $options = ?", "string" },` |
|        - |  656 | `	{ "password_needs_rehash", "string $hash, string\|int\|null $algo, array $options = ?", "bool" },` |
|        - |  657 | `	{ "password_verify", "string $password, string $hash", "bool" },` |
|        - |  658 | `	{ "pathinfo", "string $path, int $flags = 15", "array\|string" },` |
|        - |  659 | `	{ "pclose", "$handle", "int" },` |
|        - |  660 | `	{ "php_sapi_name", "", "string\|false" },` |
|        - |  661 | `	{ "php_uname", "string $mode = 'a'", "string" },` |
|        - |  662 | `	{ "phpinfo", "int $flags = 4294967295", "true" },` |
|        - |  663 | `	{ "phpversion", "?string $extension = NULL", "string\|false" },` |
|        - |  664 | `	{ "pi", "", "float" },` |
|        - |  665 | `	{ "popen", "string $command, string $mode", "" },` |
|        - |  666 | `	{ "pos", "object\|array $array", "mixed" },` |
|        - |  667 | `	{ "pow", "mixed $num, mixed $exponent", "object\|int\|float" },` |
|        - |  668 | `	{ "preg_last_error", "", "int" },` |
|        - |  669 | `	{ "preg_last_error_msg", "", "string" },` |
|        - |  670 | `	{ "fsockopen", "string $hostname, int $port = -1, &$error_code = NULL, &$error_message = NULL, ?float $timeout = NULL", "resource\|false" },` |
|        - |  671 | `	{ "pfsockopen", "string $hostname, int $port = -1, &$error_code = NULL, &$error_message = NULL, ?float $timeout = NULL", "resource\|false" },` |
|        - |  672 | `	{ "stream_socket_client", "string $address, &$error_code = NULL, &$error_message = NULL, ?float $timeout = NULL, int $flags = 4, $context = NULL", "resource\|false" },` |
|        - |  673 | `	{ "preg_match", "string $pattern, string $subject, &$matches = NULL, int $flags = 0, int $offset = 0", "int\|false" },` |
|        - |  674 | `	{ "preg_match_all", "string $pattern, string $subject, &$matches = NULL, int $flags = 0, int $offset = 0", "int\|false" },` |
|        - |  675 | `	{ "preg_quote", "string $str, ?string $delimiter = NULL", "string" },` |
|        - |  676 | `	{ "preg_replace", "array\|string $pattern, array\|string $replacement, array\|string $subject, int $limit = -1, &$count = NULL", "array\|string\|null" },` |
|        - |  677 | `	{ "preg_replace_callback", "array\|string $pattern, callable $callback, array\|string $subject, int $limit = -1, &$count = NULL, int $flags = 0", "array\|string\|null" },` |
|        - |  678 | `	{ "preg_split", "string $pattern, string $subject, int $limit = -1, int $flags = 0", "array\|false" },` |
|        - |  679 | `	{ "prev", "object\|array &$array", "mixed" },` |
|        - |  680 | `	{ "print_r", "mixed $value, bool $return = false", "string\|true" },` |
|        - |  681 | `	{ "printf", "string $format, mixed ...$values = ?", "int" },` |
|        - |  682 | `	{ "property_exists", "$object_or_class, string $property", "bool" },` |
|        - |  683 | `	{ "putenv", "string $assignment", "bool" },` |
|        - |  684 | `	{ "quotemeta", "string $string", "string" },` |
|        - |  685 | `	{ "rand", "int $min = ?, int $max = ?", "int" },` |
|        - |  686 | `	{ "random_bytes", "int $length", "string" },` |
|        - |  687 | `	{ "random_int", "int $min, int $max", "int" },` |
|        - |  688 | `	{ "range", "string\|int\|float $start, string\|int\|float $end, int\|float $step = 1", "array" },` |
|        - |  689 | `	{ "rawurldecode", "string $string", "string" },` |
|        - |  690 | `	{ "rawurlencode", "string $string", "string" },` |
|        - |  691 | `	{ "readdir", "$dir_handle = NULL", "string\|false" },` |
|        - |  692 | `	{ "readfile", "string $filename, bool $use_include_path = false, $context = NULL", "int\|false" },` |
|        - |  693 | `	{ "realpath", "string $path", "string\|false" },` |
|        - |  694 | `	{ "register_shutdown_function", "callable $callback, mixed ...$args = ?", "void" },` |
|        - |  695 | `	{ "rename", "string $from, string $to, $context = NULL", "bool" },` |
|        - |  696 | `	{ "reset", "object\|array &$array", "mixed" },` |
|        - |  697 | `	{ "restore_error_handler", "", "true" },` |
|        - |  698 | `	{ "restore_exception_handler", "", "true" },` |
|        - |  699 | `	{ "rewind", "$stream", "bool" },` |
|        - |  700 | `	{ "rewinddir", "$dir_handle = NULL", "void" },` |
|        - |  701 | `	{ "rmdir", "string $directory, $context = NULL", "bool" },` |
|        - |  702 | `	{ "round", "int\|float $num, int $precision = 0, RoundingMode\|int $mode = ?", "float" },` |
|        - |  703 | `	{ "rsort", "array &$array, int $flags = 0", "true" },` |
|        - |  704 | `	{ "rtrim", "string $string, string $characters = ?", "string" },` |
|        - |  705 | `	{ "serialize", "mixed $value", "string" },` |
|        - |  706 | `	{ "set_error_handler", "?callable $callback, int $error_levels = 30719", "" },` |
|        - |  707 | `	{ "set_exception_handler", "?callable $callback", "" },` |
|        - |  708 | `	{ "get_error_handler", "", "?callable" },` |
|        - |  709 | `	{ "get_exception_handler", "", "?callable" },` |
|        - |  710 | `	{ "hrtime", "bool $as_number = false", "array\|int" },` |
|        - |  711 | `	{ "setcookie", "string $name, string $value = '', array\|int $expires_or_options = 0, string $path = '', string $domain = '', bool $secure = false, bool $httponly = false", "bool" },` |
|        - |  712 | `	{ "setrawcookie", "string $name, string $value = '', array\|int $expires_or_options = 0, string $path = '', string $domain = '', bool $secure = false, bool $httponly = false", "bool" },` |
|        - |  713 | `	{ "sha1", "string $string, bool $binary = false", "string" },` |
|        - |  714 | `	{ "sha1_file", "string $filename, bool $binary = false", "string\|false" },` |
|        - |  715 | `	{ "shuffle", "array &$array", "true" },` |
|        - |  716 | `	{ "similar_text", "string $string1, string $string2, &$percent = NULL", "int" },` |
|        - |  717 | `	{ "sin", "float $num", "float" },` |
|        - |  718 | `	{ "sinh", "float $num", "float" },` |
|        - |  719 | `	{ "sizeof", "Countable\|array $value, int $mode = 0", "int" },` |
|        - |  720 | `	{ "sleep", "int $seconds", "int" },` |
|        - |  721 | `	{ "sort", "array &$array, int $flags = 0", "true" },` |
|        - |  722 | `	{ "soundex", "string $string", "string" },` |
|        - |  723 | `	{ "spl_autoload", "string $class, ?string $file_extensions = NULL", "void" },` |
|        - |  724 | `	{ "spl_autoload_functions", "", "array" },` |
|        - |  725 | `	{ "spl_autoload_register", "?callable $callback = NULL, bool $throw = true, bool $prepend = false", "bool" },` |
|        - |  726 | `	{ "spl_autoload_unregister", "callable $callback", "bool" },` |
|        - |  727 | `	{ "spl_object_hash", "object $object", "string" },` |
|        - |  728 | `	{ "spl_object_id", "object $object", "int" },` |
|        - |  729 | `	{ "sprintf", "string $format, mixed ...$values = ?", "string" },` |
|        - |  730 | `	{ "sqrt", "float $num", "float" },` |
|        - |  731 | `	{ "srand", "?int $seed = NULL, int $mode = 0", "void" },` |
|        - |  732 | `	{ "stat", "string $filename", "array\|false" },` |
|        - |  733 | `	{ "str_contains", "string $haystack, string $needle", "bool" },` |
|        - |  734 | `	{ "str_ends_with", "string $haystack, string $needle", "bool" },` |
|        - |  735 | `	{ "str_getcsv", "string $string, string $separator = ',', string $enclosure = '\"', string $escape = '\\\\'", "array" },` |
|        - |  736 | `	{ "str_ireplace", "array\|string $search, array\|string $replace, array\|string $subject, &$count = NULL", "array\|string" },` |
|        - |  737 | `	{ "str_pad", "string $string, int $length, string $pad_string = ' ', int $pad_type = 1", "string" },` |
|        - |  738 | `	{ "str_repeat", "string $string, int $times", "string" },` |
|        - |  739 | `	{ "str_replace", "array\|string $search, array\|string $replace, array\|string $subject, &$count = NULL", "array\|string" },` |
|        - |  740 | `	{ "str_shuffle", "string $string", "string" },` |
|        - |  741 | `	{ "str_split", "string $string, int $length = 1", "array" },` |
|        - |  742 | `	{ "str_starts_with", "string $haystack, string $needle", "bool" },` |
|        - |  743 | `	{ "str_word_count", "string $string, int $format = 0, ?string $characters = NULL", "array\|int" },` |
|        - |  744 | `	{ "strcasecmp", "string $string1, string $string2", "int" },` |
|        - |  745 | `	{ "strchr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|        - |  746 | `	{ "strcmp", "string $string1, string $string2", "int" },` |
|        - |  747 | `	{ "strnatcasecmp", "string $string1, string $string2", "int" },` |
|        - |  748 | `	{ "strnatcmp", "string $string1, string $string2", "int" },` |
|        - |  749 | `	{ "strcoll", "string $string1, string $string2", "int" },` |
|        - |  750 | `	{ "strcspn", "string $string, string $characters, int $offset = 0, ?int $length = NULL", "int" },` |
|        - |  751 | `	{ "strip_tags", "string $string, array\|string\|null $allowed_tags = NULL", "string" },` |
|        - |  752 | `	{ "stripos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|        - |  753 | `	{ "stripslashes", "string $string", "string" },` |
|        - |  754 | `	{ "stristr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|        - |  755 | `	{ "strlen", "string $string", "int" },` |
|        - |  756 | `	{ "strncasecmp", "string $string1, string $string2, int $length", "int" },` |
|        - |  757 | `	{ "strncmp", "string $string1, string $string2, int $length", "int" },` |
|        - |  758 | `	{ "strpbrk", "string $string, string $characters", "string\|false" },` |
|        - |  759 | `	{ "strpos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|        - |  760 | `	{ "strrchr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|        - |  761 | `	{ "strrev", "string $string", "string" },` |
|        - |  762 | `	{ "strripos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|        - |  763 | `	{ "strrpos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|        - |  764 | `	{ "strspn", "string $string, string $characters, int $offset = 0, ?int $length = NULL", "int" },` |
|        - |  765 | `	{ "strstr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|        - |  766 | `	{ "strtok", "string $string, ?string $token = NULL", "string\|false" },` |
|        - |  767 | `	{ "strtolower", "string $string", "string" },` |
|        - |  768 | `	{ "strtoupper", "string $string", "string" },` |
|        - |  769 | `	{ "strtr", "string $string, array\|string $from, ?string $to = NULL", "string" },` |
|        - |  770 | `	{ "strval", "mixed $value", "string" },` |
|        - |  771 | `	{ "substr", "string $string, int $offset, ?int $length = NULL", "string" },` |
|        - |  772 | `	{ "substr_compare", "string $haystack, string $needle, int $offset, ?int $length = NULL, bool $case_insensitive = false", "int" },` |
|        - |  773 | `	{ "substr_count", "string $haystack, string $needle, int $offset = 0, ?int $length = NULL", "int" },` |
|        - |  774 | `	{ "substr_replace", "array\|string $string, array\|string $replace, array\|int $offset, array\|int\|null $length = NULL", "array\|string" },` |
|        - |  775 | `	{ "symlink", "string $target, string $link", "bool" },` |
|        - |  776 | `	{ "sys_get_temp_dir", "", "string" },` |
|        - |  777 | `	{ "tan", "float $num", "float" },` |
|        - |  778 | `	{ "tanh", "float $num", "float" },` |
|        - |  779 | `	{ "time", "", "int" },` |
|        - |  780 | `	{ "touch", "string $filename, ?int $mtime = NULL, ?int $atime = NULL", "bool" },` |
|        - |  781 | `	{ "trigger_error", "string $message, int $error_level = 1024", "true" },` |
|        - |  782 | `	{ "trim", "string $string, string $characters = ?", "string" },` |
|        - |  783 | `	{ "uasort", "array &$array, callable $callback", "true" },` |
|        - |  784 | `	{ "ucfirst", "string $string", "string" },` |
|        - |  785 | `	{ "ucwords", "string $string, string $separators = ?", "string" },` |
|        - |  786 | `	{ "uksort", "array &$array, callable $callback", "true" },` |
|        - |  787 | `	{ "umask", "?int $mask = NULL", "int" },` |
|        - |  788 | `	{ "uniqid", "string $prefix = '', bool $more_entropy = false", "string" },` |
|        - |  789 | `	{ "unlink", "string $filename, $context = NULL", "bool" },` |
|        - |  790 | `	{ "unserialize", "string $data, array $options = ?", "mixed" },` |
|        - |  791 | `	{ "urldecode", "string $string", "string" },` |
|        - |  792 | `	{ "urlencode", "string $string", "string" },` |
|        - |  793 | `	{ "user_error", "string $message, int $error_level = 1024", "true" },` |
|        - |  794 | `	{ "usleep", "int $microseconds", "void" },` |
|        - |  795 | `	{ "usort", "array &$array, callable $callback", "true" },` |
|        - |  796 | `	{ "utf8_decode", "string $string", "string" },` |
|        - |  797 | `	{ "utf8_encode", "string $string", "string" },` |
|        - |  798 | `	{ "var_dump", "mixed $value, mixed ...$values = ?", "void" },` |
|        - |  799 | `	{ "var_export", "mixed $value, bool $return = false", "?string" },` |
|        - |  800 | `	{ "vfprintf", "$stream, string $format, array $values", "int" },` |
|        - |  801 | `	{ "vprintf", "string $format, array $values", "int" },` |
|        - |  802 | `	{ "vsprintf", "string $format, array $values", "string" },` |
|        - |  803 | `	{ "wordwrap", "string $string, int $width = 75, string $break = ?, bool $cut_long_words = false", "string" },` |
|        - |  804 | `	{ "zip_close", "$zip", "void" },` |
|        - |  805 | `	{ "zip_entry_close", "$zip_entry", "bool" },` |
|        - |  806 | `	{ "zip_entry_compressedsize", "$zip_entry", "int\|false" },` |
|        - |  807 | `	{ "zip_entry_compressionmethod", "$zip_entry", "string\|false" },` |
|        - |  808 | `	{ "zip_entry_filesize", "$zip_entry", "int\|false" },` |
|        - |  809 | `	{ "zip_entry_name", "$zip_entry", "string\|false" },` |
|        - |  810 | `	{ "zip_entry_open", "$zip_dp, $zip_entry, string $mode = 'rb'", "bool" },` |
|        - |  811 | `	{ "zip_entry_read", "$zip_entry, int $len = 1024", "string\|false" },` |
|        - |  812 | `	{ "zip_open", "string $filename", "" },` |
|        - |  813 | `	{ "zip_read", "$zip", "" },` |
|        - |  814 | `};` |
|        - |  815 | `/*` |
|        - |  816 | ` * Stamp the signature strings onto the registered host functions.` |
|        - |  817 | ` * Runs once at PH7_VmMakeReady, after the builtins are registered.` |
|        - |  818 | ` */` |
|        - |  819 | `/*` |
|        - |  820 | ` * Derive the minimum arity from a builtin's declared signature: a parameter is` |
|        - |  821 | ` * optional when it carries a default ("int $length = 76") or is variadic` |
|        - |  822 | ` * ("...$rest"), so the minimum is the count of the parameters that are neither,` |
|        - |  823 | ` * and the ZPP wording is "at least" as soon as one optional/variadic exists.` |
|        - |  824 | ` *` |
|        - |  825 | ` * This makes aBuiltinSig[] the single source of truth for arity — the curated` |
|        - |  826 | ` * aBuiltinArity[] table above stays as an OVERRIDE for the handful of builtins` |
|        - |  827 | ` * php reports differently from their own signature (e.g. strtr(), whose 3-arg` |
|        - |  828 | ` * form still reports "expects exactly 2", and the array_udiff() family, whose` |
|        - |  829 | ` * variadic tail hides a second required argument). Verified against php 8.5.7` |
|        - |  830 | ` * for all 462 signed builtins: 458 derive exactly, 4 are overridden.` |
|        - |  831 | ` */` |
|  1515670 |  832 | `static void VmDeriveArityFromSig(const char *zSig,sxi16 *pnMin,sxu8 *pbAtLeast,sxi16 *pnMax,sxu8 *pbHasMax)` |
|        5 |  833 | `{` |
|  1515675 |  834 | `	const char *zCur = zSig;` |
|  1515675 |  835 | `	int nMin = 0, bAtLeast = 0, bSeen = 0, bOptional = 0;` |
|  1515675 |  836 | `	int nTotal = 0, bVariadic = 0;` |
| 23327694 |  837 | `	for(;;){` |
| 48010981 |  838 | `		if( zCur[0] == '\0' \|\| zCur[0] == ',' ){` |
|  2871263 |  839 | `			if( bSeen ){` |
|  2700963 |  840 | `				nTotal++;` |
|  2700963 |  841 | `				if( bOptional ){` |
|  1062677 |  842 | `					bAtLeast = 1;` |
|   531341 |  843 | `				}else{` |
|  1638291 |  844 | `					nMin++;` |
|        - |  845 | `				}` |
|  1350479 |  846 | `			}` |
|  2871263 |  847 | `			if( zCur[0] == '\0' ){` |
|  1515675 |  848 | `				break;` |
|        - |  849 | `			}` |
|  1355593 |  850 | `			bSeen = bOptional = 0;` |
|  1355593 |  851 | `			zCur++;` |
|  1355593 |  852 | `			continue;` |
|        - |  853 | `		}` |
| 45139723 |  854 | `		if( zCur[0] != ' ' ){` |
| 39189441 |  855 | `			bSeen = 1;` |
| 19594718 |  856 | `		}` |
| 45139723 |  857 | `		if( zCur[0] == '=' \|\| (zCur[0] == '.' && zCur[1] == '.' && zCur[2] == '.') ){` |
|  1134203 |  858 | `			bOptional = 1;` |
|   567099 |  859 | `		}` |
| 45139723 |  860 | `		if( zCur[0] == '.' && zCur[1] == '.' && zCur[2] == '.' ){` |
|    71531 |  861 | `			bVariadic = 1;` |
|    35763 |  862 | `		}` |
| 45139723 |  863 | `		zCur++;` |
|        5 |  864 | `	}` |
|  1515675 |  865 | `	*pnMin = (sxi16)nMin;` |
|  1515675 |  866 | `	*pbAtLeast = (sxu8)bAtLeast;` |
|        - |  867 | `	/* php enforces a MAXIMUM too ("expects at most 1 argument, 2 given"); a` |
|        - |  868 | `	 * variadic tail means there is none. The parameter COUNT is the maximum,` |
|        - |  869 | `	 * whether or not the parameters carry defaults. */` |
|  1515675 |  870 | `	*pnMax = (sxi16)nTotal;` |
|  1515675 |  871 | `	*pbHasMax = (sxu8)(bVariadic ? 0 : 1);` |
|  1515675 |  872 | `}` |
|        - |  873 | `/*` |
|        - |  874 | ` * Does the declared type list (e.g. "array\|string", "?int", "callable") contain` |
|        - |  875 | ` * the given token? Compares against each '\|'-separated alternative, ignoring a` |
|        - |  876 | ` * leading nullable '?'.` |
|        - |  877 | ` */` |
|  1452267 |  878 | `static int VmSigTypeHas(const char *zType,int nType,const char *zTok)` |
|        5 |  879 | `{` |
|  1452272 |  880 | `	int nTok = (int)SyStrlen(zTok);` |
|  1452272 |  881 | `	int i = 0;` |
|  1452272 |  882 | `	if( zType[0] == '?' ){` |
|   275035 |  883 | `		zType++;` |
|   275035 |  884 | `		nType--;` |
|   137515 |  885 | `	}` |
|  2890192 |  886 | `	while( i < nType ){` |
|  1586716 |  887 | `		int j = i;` |
|  9222671 |  888 | `		while( j < nType && zType[j] != '\|' ){` |
|  7635960 |  889 | `			j++;` |
|        5 |  890 | `		}` |
|  1586716 |  891 | `		if( j - i == nTok && SyMemcmp(&zType[i],zTok,(sxu32)nTok) == 0 ){` |
|   148796 |  892 | `			return 1;` |
|        - |  893 | `		}` |
|  1437925 |  894 | `		i = j + 1;` |
|        5 |  895 | `	}` |
|  1303481 |  896 | `	return 0;` |
|   726607 |  897 | `}` |
|        - |  898 | `/*` |
|        - |  899 | ` * Does the declared type list name a CLASS (anything that is not one of php's` |
|        - |  900 | ` * builtin type keywords)? A class-typed parameter accepts an object, so it must` |
|        - |  901 | ` * not be rejected by the array/object/resource screen below.` |
|        - |  902 | ` */` |
|      216 |  903 | `static int VmSigTypeHasClass(const char *zType,int nType)` |
|        5 |  904 | `{` |
|        - |  905 | `	static const char *azBuiltin[] = {` |
|        - |  906 | `		"int","float","string","bool","array","object","callable","iterable",` |
|        - |  907 | `		"mixed","null","void","resource","false","true","never","self","static"` |
|        - |  908 | `	};` |
|      221 |  909 | `	int i = 0;` |
|      221 |  910 | `	if( zType[0] == '?' ){` |
|        7 |  911 | `		zType++;` |
|        7 |  912 | `		nType--;` |
|        3 |  913 | `	}` |
|      341 |  914 | `	while( i < nType ){` |
|      235 |  915 | `		int j = i, k, bKnown = 0;` |
|     1979 |  916 | `		while( j < nType && zType[j] != '\|' ){` |
|     1749 |  917 | `			j++;` |
|        5 |  918 | `		}` |
|     2357 |  919 | `		for( k = 0 ; k < (int)SX_ARRAYSIZE(azBuiltin) ; ++k ){` |
|     2247 |  920 | `			int nB = (int)SyStrlen(azBuiltin[k]);` |
|     2247 |  921 | `			if( j - i == nB && SyMemcmp(&zType[i],azBuiltin[k],(sxu32)nB) == 0 ){` |
|      124 |  922 | `				bKnown = 1;` |
|      124 |  923 | `				break;` |
|        - |  924 | `			}` |
|     1066 |  925 | `		}` |
|      235 |  926 | `		if( !bKnown && j > i ){` |
|      115 |  927 | `			return 1;` |
|        - |  928 | `		}` |
|      124 |  929 | `		i = j + 1;` |
|        4 |  930 | `	}` |
|      110 |  931 | `	return 0;` |
|      113 |  932 | `}` |
|        - |  933 | `/*` |
|        - |  934 | ` * php's ", X given" tail: like ph7_type_name() but an object reports its CLASS,` |
|        - |  935 | ` * which is what php prints in a TypeError.` |
|        - |  936 | ` */` |
|       56 |  937 | `static const char * VmArgTypeName(ph7_value *pVal)` |
|        3 |  938 | `{` |
|       59 |  939 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       59 |  940 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       59 |  941 | `		if( pInst && pInst->pClass ){` |
|       59 |  942 | `			return pInst->pClass->sName.zString;` |
|        - |  943 | `		}` |
|      ! 0 |  944 | `	}` |
|      ! 0 |  945 | `	return ph7_type_name(pVal);` |
|       31 |  946 | `}` |
|        - |  947 | `/*` |
|        - |  948 | ` * PHP-8 ZPP type enforcement for host functions, driven by the aBuiltinSig[]` |
|        - |  949 | ` * declaration (band A #7). Screens only the arguments that php can NEVER coerce` |
|        - |  950 | ` * into a declared scalar parameter — arrays, resources, and objects without a` |
|        - |  951 | ` * __toString() — and throws the catchable TypeError php throws, before the C` |
|        - |  952 | ` * routine runs. Without this an array argument reached the builtin and was` |
|        - |  953 | ` * stringified to the literal "Array" (strrev(['a']) returned "yarrA").` |
|        - |  954 | ` *` |
|        - |  955 | ` * Deliberately narrow: scalar-to-scalar coercion (and its deprecations) stays` |
|        - |  956 | ` * with the per-builtin ZPP helpers. Those emit E_DEPRECATED, which does NOT` |
|        - |  957 | ` * abort the call, so a central copy would double-fire — the trap that sank the` |
|        - |  958 | ` * first central-ZPP attempt. A TypeError aborts, so there is nothing to double.` |
|        - |  959 | ` */` |
|   934397 |  960 | `PH7_PRIVATE sxi32 VmEnforceBuiltinArgTypes(` |
|        - |  961 | `	ph7_context *pCtx,    /* Call context (for the throw) */` |
|        - |  962 | `	ph7_user_func *pFunc, /* Callee */` |
|        - |  963 | `	int nGiven,           /* Argument count */` |
|        - |  964 | `	ph7_value **apArg     /* Arguments */` |
|        - |  965 | `	)` |
|        5 |  966 | `{` |
|        - |  967 | `	/*` |
|        - |  968 | `	 * Builtins whose own argument check is php-exact and VALUE-based rather than` |
|        - |  969 | `	 * type-based must not be pre-empted here, or their message is lost. Same rule` |
|        - |  970 | `	 * the aBuiltinArity[] table follows: a builtin that already says what php says` |
|        - |  971 | `	 * stays off the shared screen. get_class_vars() takes any stringifiable value` |
|        - |  972 | `	 * and reports "must be a valid class name, Array given".` |
|        - |  973 | `	 */` |
|        - |  974 | `	static const char *azSelfChecked[] = { "get_class_vars" };` |
|   934402 |  975 | `	const char *zSig = pFunc->zSig;` |
|        - |  976 | `	const char *zCur, *zEnd;` |
|   934402 |  977 | `	int iArg = 0;` |
|   934402 |  978 | `	if( zSig == 0 ){` |
|   175169 |  979 | `		return SXRET_OK;` |
|        - |  980 | `	}` |
|  1518467 |  981 | `	for( iArg = 0 ; iArg < (int)SX_ARRAYSIZE(azSelfChecked) ; ++iArg ){` |
|  1139215 |  982 | `		if( SyStrncmp(pFunc->sName.zString,azSelfChecked[iArg],` |
|  1139215 |  983 | `			(sxu32)SyStrlen(azSelfChecked[iArg])) == 0` |
|   379989 |  984 | `		 && pFunc->sName.nByte == SyStrlen(azSelfChecked[iArg]) ){` |
|        5 |  985 | `			return SXRET_OK;` |
|        - |  986 | `		}` |
|   379985 |  987 | `	}` |
|   759234 |  988 | `	iArg = 0;` |
|   759234 |  989 | `	zCur = zSig;` |
|   759234 |  990 | `	zEnd = &zSig[SyStrlen(zSig)];` |
|  2167445 |  991 | `	while( zCur < zEnd && iArg < nGiven ){` |
|        - |  992 | `		const char *zType, *zName, *zStop;` |
|        - |  993 | `		int nType, nName;` |
|        - |  994 | `		ph7_value *pArg;` |
|        - |  995 | `		/* Parameter = "<type> $<name>[ = <default>]"; the type is whatever` |
|        - |  996 | `		 * precedes the '$', and an empty type means "untyped" (no screen). */` |
|  2081910 |  997 | `		while( zCur < zEnd && zCur[0] == ' ' ){` |
|   671545 |  998 | `			zCur++;` |
|        5 |  999 | `		}` |
|  1410370 | 1000 | `		zStop = zCur;` |
| 22409823 | 1001 | `		while( zStop < zEnd && zStop[0] != ',' ){` |
| 20999458 | 1002 | `			zStop++;` |
|        5 | 1003 | `		}` |
|  1410370 | 1004 | `		zName = zCur;` |
| 10384372 | 1005 | `		while( zName < zStop && zName[0] != '$' ){` |
|  8974007 | 1006 | `			zName++;` |
|        5 | 1007 | `		}` |
|  1410370 | 1008 | `		if( zName >= zStop ){` |
|        7 | 1009 | `			break; /* malformed / no parameter name — stop screening */` |
|        - | 1010 | `		}` |
|  1410364 | 1011 | `		if( zName >= zCur + 3 && SyMemcmp(zName - 3,"...",3) == 0 ){` |
|     1939 | 1012 | `			break; /* variadic tail: stop (its type applies to the rest) */` |
|        - | 1013 | `		}` |
|  1408430 | 1014 | `		zType = zCur;` |
|  1408430 | 1015 | `		nType = (int)(zName - zCur);` |
|        - | 1016 | `		/* Trim the trailing spaces and the by-ref marker of "array &$array" */` |
|  3490744 | 1017 | `		while( nType > 0 && (zType[nType-1] == ' ' \|\| zType[nType-1] == '&') ){` |
|  1377638 | 1018 | `			nType--;` |
|        5 | 1019 | `		}` |
|  1408430 | 1020 | `		zName++; /* skip '$' */` |
|  1408430 | 1021 | `		nName = 0;` |
| 10197570 | 1022 | `		while( &zName[nName] < zStop && zName[nName] != ' ' && zName[nName] != '=' ){` |
|  8789145 | 1023 | `			nName++;` |
|        5 | 1024 | `		}` |
|  1408430 | 1025 | `		pArg = apArg[iArg];` |
|  1408430 | 1026 | `		if( nType > 0 && !VmSigTypeHas(zType,nType,"mixed") ){` |
|  1301611 | 1027 | `			const char *zGiven = 0;` |
|  1301611 | 1028 | `			if( (pArg->iFlags & MEMOBJ_HASHMAP) != 0 ){` |
|    74066 | 1029 | `				if( !VmSigTypeHas(zType,nType,"array")` |
|    37108 | 1030 | `				 && !VmSigTypeHas(zType,nType,"iterable")` |
|      155 | 1031 | `				 && !VmSigTypeHas(zType,nType,"callable") ){` |
|      119 | 1032 | `					zGiven = "array";` |
|       62 | 1033 | `				}` |
|  1264578 | 1034 | `			}else if( (pArg->iFlags & MEMOBJ_OBJ) != 0 ){` |
|     1560 | 1035 | `				if( !VmSigTypeHas(zType,nType,"object")` |
|     1062 | 1036 | `				 && !VmSigTypeHas(zType,nType,"iterable")` |
|      564 | 1037 | `				 && !VmSigTypeHas(zType,nType,"callable")` |
|      394 | 1038 | `				 && !VmSigTypeHasClass(zType,nType) ){` |
|        - | 1039 | `					/* An object with __toString() still satisfies a string` |
|        - | 1040 | `					 * parameter in weak mode — php coerces it. */` |
|      107 | 1041 | `					ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|      148 | 1042 | `					int bStringable = VmSigTypeHas(zType,nType,"string")` |
|      104 | 1043 | `						&& pInst && PH7_ClassExtractMethod(pInst->pClass,"__toString",` |
|       41 | 1044 | `							sizeof("__toString")-1) != 0;` |
|      107 | 1045 | `					if( !bStringable ){` |
|       59 | 1046 | `						zGiven = VmArgTypeName(pArg);` |
|       28 | 1047 | `					}` |
|       57 | 1048 | `				}` |
|  1226765 | 1049 | `			}else if( (pArg->iFlags & MEMOBJ_NULL) != 0 ){` |
|        - | 1050 | `				/* php only DEPRECATES null for a non-nullable parameter; PHL rejects` |
|        - | 1051 | `				 * it (scope policy). A leading '?' or an explicit "null" arm in a` |
|        - | 1052 | `				 * union declares the parameter nullable. A "callable" parameter is` |
|        - | 1053 | `				 * left to the builtin's own callback check, which words the failure` |
|        - | 1054 | `				 * php's way ("must be a valid callback, no array or string given") —` |
|        - | 1055 | `				 * the same reason get_class_vars() sits on azSelfChecked[]. */` |
|     5006 | 1056 | `				if( zType[0] != '?'` |
|     2529 | 1057 | `				 && !VmSigTypeHas(zType,nType,"null")` |
|       55 | 1058 | `				 && !VmSigTypeHas(zType,nType,"callable") ){` |
|       46 | 1059 | `					zGiven = "null";` |
|       26 | 1060 | `				}` |
|  1223482 | 1061 | `			}else if( (pArg->iFlags & MEMOBJ_RES) != 0 ){` |
|        - | 1062 | `				/* A class-typed parameter also accepts a resource: several handles php 8` |
|        - | 1063 | `				 * models as objects are still resources here (xml_*'s XMLParser is the` |
|        - | 1064 | `				 * one the signatures already declare php-8-style, for reflection). The` |
|        - | 1065 | `				 * screen would otherwise reject the engine's own parser handle. Recorded` |
|        - | 1066 | `				 * as a divergence in NEWPLAN §7 — it goes away when those handles become` |
|        - | 1067 | `				 * real objects. */` |
|        2 | 1068 | `				if( !VmSigTypeHas(zType,nType,"resource")` |
|        3 | 1069 | `				 && !VmSigTypeHasClass(zType,nType) ){` |
|        3 | 1070 | `					zGiven = "resource";` |
|        1 | 1071 | `				}` |
|        1 | 1072 | `			}` |
|  1301611 | 1073 | `			if( zGiven ){` |
|      326 | 1074 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|        - | 1075 | `					"%z(): Argument #%d ($%.*s) must be of type %.*s, %s given",` |
|      107 | 1076 | `					&pFunc->sName,iArg + 1,nName,zName,nType,zType,zGiven);` |
|        - | 1077 | `			}` |
|   651108 | 1078 | `		}` |
|  1408216 | 1079 | `		zCur = (zStop < zEnd) ? zStop + 1 : zEnd;` |
|  1408216 | 1080 | `		iArg++;` |
|        5 | 1081 | `	}` |
|   759020 | 1082 | `	return SXRET_OK;` |
|   467569 | 1083 | `}` |
|        - | 1084 | `/*` |
|        - | 1085 | ` * Builtins whose accepted arity is NOT a contiguous range, so the signature` |
|        - | 1086 | ` * cannot express it and the central too-many-arguments check must stay out of` |
|        - | 1087 | ` * the way: rand()/mt_rand() take 0 OR 2 arguments (never 1), and php words the` |
|        - | 1088 | ` * violation "expects exactly 2 arguments, 3 given" from their own check rather` |
|        - | 1089 | ` * than the ZPP "at most". They validate themselves; leaving bHasMaxArg at 0` |
|        - | 1090 | ` * keeps their message php-faithful.` |
|        - | 1091 | ` */` |
|  1515670 | 1092 | `static int VmBuiltinSelfValidatesArity(const char *zName)` |
|        5 | 1093 | `{` |
|        - | 1094 | `	static const char *const azSelf[] = { "rand", "mt_rand" };` |
|        - | 1095 | `	sxu32 i;` |
|  4536797 | 1096 | `	for( i = 0 ; i < SX_ARRAYSIZE(azSelf) ; i++ ){` |
|  3027939 | 1097 | `		sxu32 nSelf = SyStrlen(azSelf[i]);` |
|  3027939 | 1098 | `		if( SyStrlen(zName) == nSelf && SyStrncmp(zName,azSelf[i],nSelf) == 0 ){` |
|     6817 | 1099 | `			return 1;` |
|        - | 1100 | `		}` |
|  1510566 | 1101 | `	}` |
|  1508863 | 1102 | `	return 0;` |
|   757840 | 1103 | `}` |
|     3406 | 1104 | `PH7_PRIVATE void VmSetBuiltinSignatures(ph7_vm *pVm)` |
|        5 | 1105 | `{` |
|        - | 1106 | `	sxu32 n;` |
|  1559953 | 1107 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinSig) ; n++ ){` |
|  2334818 | 1108 | `		SyHashEntry *pEntry = SyHashGet(&pVm->hHostFunction,` |
|  1556542 | 1109 | `			(const void *)aBuiltinSig[n].zName,(sxu32)SyStrlen(aBuiltinSig[n].zName));` |
|  1556547 | 1110 | `		if( pEntry ){` |
|  1515675 | 1111 | `			ph7_user_func *pFunc = (ph7_user_func *)pEntry->pUserData;` |
|  1515675 | 1112 | `			sxi16 nMin = 0, nMax = 0;` |
|  1515675 | 1113 | `			sxu8 bAtLeast = 0, bHasMax = 0;` |
|  1515675 | 1114 | `			pFunc->zSig = aBuiltinSig[n].zSig;` |
|  1515675 | 1115 | `			pFunc->zRet = aBuiltinSig[n].zRet[0] ? aBuiltinSig[n].zRet : 0;` |
|  1515675 | 1116 | `			VmDeriveArityFromSig(pFunc->zSig,&nMin,&bAtLeast,&nMax,&bHasMax);` |
|        - | 1117 | `			/* The MAXIMUM always comes from the signature: the curated override` |
|        - | 1118 | `			 * table speaks only to the minimum (and its wording). */` |
|  1515675 | 1119 | `			pFunc->nMaxArg = nMax;` |
|  1515675 | 1120 | `			pFunc->bHasMaxArg = (sxu8)(VmBuiltinSelfValidatesArity(aBuiltinSig[n].zName) ? 0 : bHasMax);` |
|  1515675 | 1121 | `			if( pFunc->nMinArg < 1 ){` |
|        - | 1122 | `				/* VmSetBuiltinArity() ran first: a non-zero minimum here means the` |
|        - | 1123 | `				 * curated override already spoke for this builtin, so leave it. */` |
|   636927 | 1124 | `				pFunc->nMinArg = nMin;` |
|   636927 | 1125 | `				pFunc->bAtLeast = bAtLeast;` |
|   318461 | 1126 | `			}` |
|   757835 | 1127 | `		}` |
|   778276 | 1128 | `	}` |
|     3411 | 1129 | `}` |
|        - | 1130 | `/*` |
|        - | 1131 | ` * Signature lookup by function name, for the reflection layer: embedded-PHP` |
|        - | 1132 | ` * builtins (max/min/Exception methods...) are ph7_vm_func instances, not` |
|        - | 1133 | ` * host functions, so they miss the VmSetBuiltinSignatures stamping and pull` |
|        - | 1134 | ` * their row on demand here. Linear scan — reflection-path only.` |
|        - | 1135 | ` */` |
|        4 | 1136 | `PH7_PRIVATE const char * PH7_VmBuiltinSigLookup(const char *zName,sxu32 nLen,const char **pzRet)` |
|        1 | 1137 | `{` |
|        - | 1138 | `	sxu32 n;` |
|        5 | 1139 | `	if( pzRet ){` |
|        5 | 1140 | `		*pzRet = 0;` |
|        2 | 1141 | `	}` |
|     1045 | 1142 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinSig) ; n++ ){` |
|     1044 | 1143 | `		if( SyStrlen(aBuiltinSig[n].zName) == nLen` |
|      541 | 1144 | `		 && SyMemcmp(aBuiltinSig[n].zName,zName,nLen) == 0 ){` |
|        5 | 1145 | `			if( pzRet && aBuiltinSig[n].zRet[0] ){` |
|        5 | 1146 | `				*pzRet = aBuiltinSig[n].zRet;` |
|        2 | 1147 | `			}` |
|        5 | 1148 | `			return aBuiltinSig[n].zSig;` |
|        - | 1149 | `		}` |
|      521 | 1150 | `	}` |
|      ! 0 | 1151 | `	return 0;` |
|        3 | 1152 | `}` |
|        - | 1153 | `/*` |
|        - | 1154 | ` * Write a value back to the caller's variable through a builtin argument's` |
|        - | 1155 | ` * stack-slot nIdx — the shared write-back for builtin by-reference` |
|        - | 1156 | ` * out-parameters (preg_match $matches, preg_replace &$count, similar_text` |
|        - | 1157 | ` * &$percent, ...).` |
|        - | 1158 | ` *` |
|        - | 1159 | ` * For a positional out-param argument the call compiler auto-vivifies known` |
|        - | 1160 | ` * by-reference out-params (see GenStateByRefBuiltinMask in compile.c), so a` |
|        - | 1161 | ` * bare undefined variable, an array subscript, and a declared/untyped` |
|        - | 1162 | ` * property all arrive with a real nIdx and are written back here, matching` |
|        - | 1163 | ` * PHP's reference semantics.` |
|        - | 1164 | ` *` |
|        - | 1165 | ` * nIdx stays SXU32_HIGH and the write-back to the caller is skipped (the` |
|        - | 1166 | ` * value still lands in the local stack slot) when the argument cannot expose` |
|        - | 1167 | ` * a stable memobj slot: a literal, a function-call result, a subscript of a` |
|        - | 1168 | ` * non-lvalue parent (foo()['k']), or any variable in a call that also uses` |
|        - | 1169 | ` * named or spread arguments (compile-time positions no longer map to the` |
|        - | 1170 | ` * runtime arg slots, so the compiler conservatively does not vivify). An` |
|        - | 1171 | ` * uninitialized typed property is also not wired (it throws before the` |
|        - | 1172 | ` * write) -- see the recorded deferrals.` |
|        - | 1173 | ` */` |
|      208 | 1174 | `PH7_PRIVATE void PH7_VmStoreArgByRef(ph7_vm *pVm,ph7_value *pArg,ph7_value *pNewVal)` |
|        5 | 1175 | `{` |
|      213 | 1176 | `	if( pArg->nIdx != SXU32_HIGH ){` |
|      211 | 1177 | `		ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pArg->nIdx);` |
|      211 | 1178 | `		if( pObj ){` |
|      211 | 1179 | `			PH7_MemObjStore(pNewVal,pObj);` |
|      103 | 1180 | `		}` |
|      103 | 1181 | `	}` |
|      213 | 1182 | `	PH7_MemObjStore(pNewVal,pArg);` |
|      213 | 1183 | `}` |
|        - | 1184 | `/*` |
|        - | 1185 | ` * Raise an E_WARNING whose text is used VERBATIM.` |
|        - | 1186 | ` * ph7_context_throw_error_format() prepends "func(): " to whatever it is given, but php's` |
|        - | 1187 | ` * IO warnings put the offending path inside those parens -- "file_get_contents(/nope):` |
|        - | 1188 | ` * Failed to open stream: ..." -- so a caller that needs php's exact shape must build the` |
|        - | 1189 | ` * whole line itself and come through here.` |
|        - | 1190 | ` */` |
|        - | 1191 | `/*` |
|        - | 1192 | ` * Validate a callback argument and throw php's TypeError when it cannot be called.` |
|        - | 1193 | ` *` |
|        - | 1194 | ` * The shared dispatcher (PH7_VmCallUserFunctionWithMap) answers SXRET_OK with a NULL` |
|        - | 1195 | ` * result for an unresolvable callable, so every caller that did not check first failed` |
|        - | 1196 | ` * SILENTLY: call_user_func() returned NULL and usort() left the array sorted by nothing` |
|        - | 1197 | ` * at all. php rejects the ARGUMENT up front, naming exactly why.` |
|        - | 1198 | ` */` |
|      162 | 1199 | `PH7_PRIVATE sxi32 PH7_CheckCallbackArg(` |
|        - | 1200 | `	ph7_context *pCtx,   /* Calling context (names the function in the message) */` |
|        - | 1201 | `	ph7_value *pCb,      /* The callback argument */` |
|        - | 1202 | `	int iArg,            /* Its 1-based position */` |
|        - | 1203 | `	const char *zParam,  /* Its php parameter name, e.g. "callback" */` |
|        - | 1204 | `	int bNullable        /* TRUE when php's text says "or null" */` |
|        - | 1205 | `	)` |
|        3 | 1206 | `{` |
|      165 | 1207 | `	const char *zOrNull = bNullable ? " or null" : "";` |
|      165 | 1208 | `	if( ph7_value_is_callable(pCb) ){` |
|      147 | 1209 | `		return PH7_OK;` |
|        - | 1210 | `	}` |
|       19 | 1211 | `	if( ph7_value_is_string(pCb) ){` |
|        - | 1212 | `		int nLen;` |
|       11 | 1213 | `		const char *zName = ph7_value_to_string(pCb,&nLen);` |
|       16 | 1214 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|        - | 1215 | `			"%s(): Argument #%d ($%s) must be a valid callback%s, function \"%.*s\" not found or invalid function name",` |
|        5 | 1216 | `			ph7_function_name(pCtx),iArg,zParam,zOrNull,nLen,zName);` |
|        - | 1217 | `	}` |
|        9 | 1218 | `	if( ph7_value_is_array(pCb) ){` |
|        7 | 1219 | `		ph7_hashmap *pMap = (ph7_hashmap *)pCb->x.pOther;` |
|        7 | 1220 | `		if( pMap == 0 \|\| pMap->nEntry != 2 ){` |
|        4 | 1221 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|        - | 1222 | `				"%s(): Argument #%d ($%s) must be a valid callback%s, array callback must have exactly two members",` |
|        1 | 1223 | `				ph7_function_name(pCtx),iArg,zParam,zOrNull);` |
|      ! 0 | 1224 | `		}else{` |
|        5 | 1225 | `			ph7_vm *pVm = pCtx->pVm;` |
|        5 | 1226 | `			ph7_value *pCls = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|        5 | 1227 | `			ph7_value *pMeth = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|        5 | 1228 | `			ph7_class *pClass = pCls ? PH7_VmExtractClassFromValue(&(*pVm),pCls) : 0;` |
|        5 | 1229 | `			if( pClass == 0 ){` |
|        5 | 1230 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|        - | 1231 | `					"%s(): Argument #%d ($%s) must be a valid callback%s, class \"%.*s\" not found",` |
|        1 | 1232 | `					ph7_function_name(pCtx),iArg,zParam,zOrNull,` |
|        2 | 1233 | `					pCls ? (int)SyBlobLength(&pCls->sBlob) : 0,` |
|        1 | 1234 | `					pCls ? (const char *)SyBlobData(&pCls->sBlob) : "");` |
|        - | 1235 | `			}` |
|        5 | 1236 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|        - | 1237 | `				"%s(): Argument #%d ($%s) must be a valid callback%s, class %z does not have a method \"%.*s\"",` |
|        1 | 1238 | `				ph7_function_name(pCtx),iArg,zParam,zOrNull,&pClass->sName,` |
|        2 | 1239 | `				pMeth ? (int)SyBlobLength(&pMeth->sBlob) : 0,` |
|        1 | 1240 | `				pMeth ? (const char *)SyBlobData(&pMeth->sBlob) : "");` |
|        - | 1241 | `		}` |
|        - | 1242 | `	}` |
|        4 | 1243 | `	return PH7_VmThrowException(pCtx,"TypeError",` |
|        - | 1244 | `		"%s(): Argument #%d ($%s) must be a valid callback%s, no array or string given",` |
|        1 | 1245 | `		ph7_function_name(pCtx),iArg,zParam,zOrNull);` |
|       84 | 1246 | `}` |
|    19782 | 1247 | `PH7_PRIVATE void PH7_VmThrowWarningFmt(ph7_vm *pVm,const char *zFmt,...)` |
|        5 | 1248 | `{` |
|        - | 1249 | `	va_list ap;` |
|    19787 | 1250 | `	va_start(ap,zFmt);` |
|    19787 | 1251 | `	PH7_VmThrowErrorAp(pVm,0,PH7_CTX_WARNING,zFmt,ap);` |
|    19787 | 1252 | `	va_end(ap);` |
|    19787 | 1253 | `}` |
|        - | 1254 | `/*` |
|        - | 1255 | ` * Emit a formatted E_USER_DEPRECATED diagnostic with no function-name prefix:` |
|        - | 1256 | ` * php reports #[\Deprecated] as USER-deprecated (16384, it is userland-authored),` |
|        - | 1257 | ` * not the engine's E_DEPRECATED (8192) — handler-visible errno matters.` |
|        - | 1258 | ` */` |
|       34 | 1259 | `static void VmThrowUserDeprecatedFmt(ph7_vm *pVm,const char *zFmt,...)` |
|        1 | 1260 | `{` |
|        - | 1261 | `	va_list ap;` |
|       35 | 1262 | `	va_start(ap,zFmt);` |
|       35 | 1263 | `	PH7_VmThrowErrorAp(pVm,0,E_USER_DEPRECATED,zFmt,ap);` |
|       35 | 1264 | `	va_end(ap);` |
|       35 | 1265 | `}` |
|        - | 1266 | `/*` |
|        - | 1267 | ` * php 8.4 #[\Deprecated]: emit the E_USER_DEPRECATED notice for a call to a user` |
|        - | 1268 | ` * function/method carrying the attribute. Message shapes (php-exact):` |
|        - | 1269 | ` *   Function f() is deprecated` |
|        - | 1270 | ` *   Method C::m() is deprecated since 2.0, use g() instead` |
|        - | 1271 | ` * ("since" from the attribute's since: argument; the trailing ", msg" from` |
|        - | 1272 | ` * message:/positional #1.) The attribute arguments are compiled constant` |
|        - | 1273 | ` * expressions — evaluated via VmLocalExec, the reflection thunks' pattern.` |
|        - | 1274 | ` * Called from OP_CALL once per call, only when the callee HAS attributes;` |
|        - | 1275 | ` * pDeclClass names the method's declaring class (0 for plain functions).` |
|        - | 1276 | ` */` |
|        - | 1277 | `/*` |
|        - | 1278 | ` * Expand a global constant's value, emitting the php 8.5 #[\Deprecated]` |
|        - | 1279 | ` * E_USER_DEPRECATED notice first when the constant carries attributes` |
|        - | 1280 | `` * (`Constant GD is deprecated since 1.2` — every access re-warns).`` |
|        - | 1281 | ` */` |
|        - | 1282 | `static void VmDeprecatedAttrNoticeSubject(ph7_vm *pVm,SySet *pAttrs,` |
|        - | 1283 | `	const char *zKind,const SyString *pQual,const SyString *pName);` |
|        - | 1284 | `/*` |
|        - | 1285 | ` * Expand a constant, emitting the php 8.5 #[\Deprecated] E_USER_DEPRECATED notice` |
|        - | 1286 | `` * first when a USERLAND constant carries the attribute (`Constant GD is deprecated`` |
|        - | 1287 | `` * since 1.2` — every access re-warns). Engine constants php merely deprecates are`` |
|        - | 1288 | ` * not mimicked: PHL removes them outright (see the scope policy), so there is no` |
|        - | 1289 | ` * engine-side E_DEPRECATED list here.` |
|        - | 1290 | ` */` |
|    23218 | 1291 | `PH7_PRIVATE void VmExpandConstantWithNotice(ph7_vm *pVm,ph7_constant *pCons,ph7_value *pOut)` |
|        5 | 1292 | `{` |
|    23223 | 1293 | `	if( SySetUsed(&pCons->aAttrs) > 0 ){` |
|        7 | 1294 | `		VmDeprecatedAttrNoticeSubject(pVm,&pCons->aAttrs,"Constant",0,&pCons->sName);` |
|        3 | 1295 | `	}` |
|    23223 | 1296 | `	pCons->xExpand(pOut,pCons->pUserData);` |
|    23223 | 1297 | `}` |
|        - | 1298 | `/*` |
|        - | 1299 | ` * Scan a declared-attribute set for #[\Deprecated]; when found, evaluate its` |
|        - | 1300 | ` * message:/since: arguments (positional #0 = message, #1 = since) into the` |
|        - | 1301 | ` * caller's values and return TRUE. The base subject text ("Function f()",` |
|        - | 1302 | ` * "Constant C::K") is the caller's business.` |
|        - | 1303 | ` */` |
|      130 | 1304 | `static int VmDeprecatedAttrExtract(ph7_vm *pVm,SySet *pAttrs,` |
|        - | 1305 | `	ph7_value *pMsg,int *pbMsg,ph7_value *pSince,int *pbSince)` |
|        5 | 1306 | `{` |
|      135 | 1307 | `	ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|        - | 1308 | `	sxu32 n;` |
|      135 | 1309 | `	*pbMsg = *pbSince = 0;` |
|      235 | 1310 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; ++n ){` |
|      139 | 1311 | `		ph7_attribute *pAttr = &aAttr[n];` |
|        - | 1312 | `		ph7_attr_arg *aArg;` |
|      139 | 1313 | `		sxu32 i,nPos = 0;` |
|      134 | 1314 | `		if( SyStringLength(&pAttr->sName) != sizeof("Deprecated")-1` |
|       89 | 1315 | `		 \|\| SyStrnicmp(SyStringData(&pAttr->sName),"Deprecated",sizeof("Deprecated")-1) != 0 ){` |
|      105 | 1316 | `			continue;` |
|        - | 1317 | `		}` |
|       35 | 1318 | `		aArg = (ph7_attr_arg *)SySetBasePtr(&pAttr->aArgs);` |
|       53 | 1319 | `		for( i = 0 ; i < SySetUsed(&pAttr->aArgs) ; ++i ){` |
|       19 | 1320 | `			ph7_attr_arg *pArg = &aArg[i];` |
|       19 | 1321 | `			int isMsg = 0,isSince = 0;` |
|       19 | 1322 | `			if( SyStringLength(&pArg->sName) == 0 ){` |
|        3 | 1323 | `				isMsg = (nPos == 0);` |
|        3 | 1324 | `				isSince = (nPos == 1);` |
|        3 | 1325 | `				nPos++;` |
|       18 | 1326 | `			}else if( SyStringLength(&pArg->sName) == sizeof("message")-1` |
|       12 | 1327 | `			 && SyMemcmp(SyStringData(&pArg->sName),"message",sizeof("message")-1) == 0 ){` |
|        7 | 1328 | `				isMsg = 1;` |
|       14 | 1329 | `			}else if( SyStringLength(&pArg->sName) == sizeof("since")-1` |
|       11 | 1330 | `			 && SyMemcmp(SyStringData(&pArg->sName),"since",sizeof("since")-1) == 0 ){` |
|       11 | 1331 | `				isSince = 1;` |
|        5 | 1332 | `			}` |
|       19 | 1333 | `			if( isMsg && !*pbMsg && SySetUsed(&pArg->aByteCode) > 0 ){` |
|       13 | 1334 | `				if( VmLocalExec(pVm,&pArg->aByteCode,pMsg,FALSE) == SXRET_OK ){` |
|        9 | 1335 | `					if( (pMsg->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 | 1336 | `						PH7_MemObjToString(pMsg);` |
|      ! 0 | 1337 | `					}` |
|        9 | 1338 | `					*pbMsg = 1;` |
|        5 | 1339 | `				}` |
|       15 | 1340 | `			}else if( isSince && !*pbSince && SySetUsed(&pArg->aByteCode) > 0 ){` |
|       11 | 1341 | `				if( VmLocalExec(pVm,&pArg->aByteCode,pSince,FALSE) == SXRET_OK ){` |
|       11 | 1342 | `					if( (pSince->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 | 1343 | `						PH7_MemObjToString(pSince);` |
|      ! 0 | 1344 | `					}` |
|       11 | 1345 | `					*pbSince = 1;` |
|        5 | 1346 | `				}` |
|        5 | 1347 | `			}` |
|       10 | 1348 | `		}` |
|       35 | 1349 | `		return 1;` |
|      ! 0 | 1350 | `	}` |
|      101 | 1351 | `	return 0;` |
|       70 | 1352 | `}` |
|        - | 1353 | `/*` |
|        - | 1354 | ` * Append php's " since X" / ", message" suffixes to a built base subject and` |
|        - | 1355 | ` * emit the E_USER_DEPRECATED notice.` |
|        - | 1356 | ` */` |
|       34 | 1357 | `static void VmDeprecatedEmit(ph7_vm *pVm,SyBlob *pOut,` |
|        - | 1358 | `	ph7_value *pMsg,int bMsg,ph7_value *pSince,int bSince)` |
|        1 | 1359 | `{` |
|       35 | 1360 | `	if( bSince && SyBlobLength(&pSince->sBlob) > 0 ){` |
|       16 | 1361 | `		SyBlobFormat(pOut," since %.*s",(int)SyBlobLength(&pSince->sBlob),` |
|       10 | 1362 | `			(const char *)SyBlobData(&pSince->sBlob));` |
|        5 | 1363 | `	}` |
|       35 | 1364 | `	if( bMsg && SyBlobLength(&pMsg->sBlob) > 0 ){` |
|       13 | 1365 | `		SyBlobFormat(pOut,", %.*s",(int)SyBlobLength(&pMsg->sBlob),` |
|        8 | 1366 | `			(const char *)SyBlobData(&pMsg->sBlob));` |
|        4 | 1367 | `	}` |
|       35 | 1368 | `	VmThrowUserDeprecatedFmt(pVm,"%.*s",(int)SyBlobLength(pOut),(const char *)SyBlobData(pOut));` |
|       35 | 1369 | `}` |
|        - | 1370 | `/*` |
|        - | 1371 | ` * Generic #[\Deprecated] notice for a named subject:` |
|        - | 1372 | ` * "<Kind> [Qual::]Name is deprecated[ since X][, message]".` |
|        - | 1373 | ` */` |
|       18 | 1374 | `static void VmDeprecatedAttrNoticeSubject(ph7_vm *pVm,SySet *pAttrs,` |
|        - | 1375 | `	const char *zKind,const SyString *pQual,const SyString *pName)` |
|        1 | 1376 | `{` |
|        - | 1377 | `	ph7_value sMsg,sSince;` |
|        - | 1378 | `	SyBlob sOut;` |
|        - | 1379 | `	int bMsg,bSince;` |
|       19 | 1380 | `	PH7_MemObjInit(pVm,&sMsg);` |
|       19 | 1381 | `	PH7_MemObjInit(pVm,&sSince);` |
|       19 | 1382 | `	if( VmDeprecatedAttrExtract(pVm,pAttrs,&sMsg,&bMsg,&sSince,&bSince) ){` |
|       15 | 1383 | `		SyBlobInit(&sOut,&pVm->sAllocator);` |
|       15 | 1384 | `		if( pQual ){` |
|       11 | 1385 | `			SyBlobFormat(&sOut,"%s %z::%z is deprecated",zKind,pQual,pName);` |
|        6 | 1386 | `		}else{` |
|        5 | 1387 | `			SyBlobFormat(&sOut,"%s %z is deprecated",zKind,pName);` |
|        - | 1388 | `		}` |
|       15 | 1389 | `		VmDeprecatedEmit(pVm,&sOut,&sMsg,bMsg,&sSince,bSince);` |
|       15 | 1390 | `		SyBlobRelease(&sOut);` |
|        7 | 1391 | `	}` |
|       19 | 1392 | `	PH7_MemObjRelease(&sMsg);` |
|       19 | 1393 | `	PH7_MemObjRelease(&sSince);` |
|       19 | 1394 | `}` |
|      112 | 1395 | `PH7_PRIVATE void VmDeprecatedAttrNotice(ph7_vm *pVm,ph7_vm_func *pFunc,ph7_class *pDeclClass)` |
|        5 | 1396 | `{` |
|        - | 1397 | `	ph7_value sMsg,sSince;` |
|        - | 1398 | `	SyBlob sOut;` |
|        - | 1399 | `	int bMsg,bSince;` |
|      117 | 1400 | `	PH7_MemObjInit(pVm,&sMsg);` |
|      117 | 1401 | `	PH7_MemObjInit(pVm,&sSince);` |
|      117 | 1402 | `	if( VmDeprecatedAttrExtract(pVm,&pFunc->aAttrs,&sMsg,&bMsg,&sSince,&bSince) ){` |
|       21 | 1403 | `		SyBlobInit(&sOut,&pVm->sAllocator);` |
|       21 | 1404 | `		if( pDeclClass ){` |
|        5 | 1405 | `			SyBlobFormat(&sOut,"Method %z::%z() is deprecated",&pDeclClass->sName,&pFunc->sName);` |
|        3 | 1406 | `		}else{` |
|       17 | 1407 | `			SyBlobFormat(&sOut,"Function %z() is deprecated",&pFunc->sName);` |
|        - | 1408 | `		}` |
|       21 | 1409 | `		VmDeprecatedEmit(pVm,&sOut,&sMsg,bMsg,&sSince,bSince);` |
|       21 | 1410 | `		SyBlobRelease(&sOut);` |
|       10 | 1411 | `	}` |
|      117 | 1412 | `	PH7_MemObjRelease(&sMsg);` |
|      117 | 1413 | `	PH7_MemObjRelease(&sSince);` |
|      117 | 1414 | `}` |
|        - | 1415 | `/*` |
|        - | 1416 | ` * Same notice for a #[\Deprecated] class constant, at its static-access site:` |
|        - | 1417 | `` * php's `Constant C::K is deprecated` (each access re-warns).`` |
|        - | 1418 | ` */` |
|       12 | 1419 | `PH7_PRIVATE void VmDeprecatedConstNotice(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pMember)` |
|        1 | 1420 | `{` |
|       19 | 1421 | `	VmDeprecatedAttrNoticeSubject(pVm,&pMember->aAttrs,` |
|       12 | 1422 | `		(pMember->iFlags & PH7_CLASS_ATTR_ENUMCASE) ? "Enum case" : "Constant",` |
|       12 | 1423 | `		&pClass->sName,&pMember->sName);` |
|       13 | 1424 | `}` |
|        - | 1425 |  |
