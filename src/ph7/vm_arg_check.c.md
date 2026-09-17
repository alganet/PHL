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
|        - |  153 | `	{ "get_resource_id",           1, 0 },` |
|        - |  154 | `	{ "get_resource_type",         1, 0 },` |
|        - |  155 | `	{ "gettype",                   1, 0 },` |
|        - |  156 | `	{ "intval",                    1, 1 },` |
|        - |  157 | `	{ "is_array",                  1, 0 },` |
|        - |  158 | `	{ "is_bool",                   1, 0 },` |
|        - |  159 | `	{ "is_callable",               1, 1 },` |
|        - |  160 | `	{ "is_double",                 1, 0 },` |
|        - |  161 | `	{ "is_float",                  1, 0 },` |
|        - |  162 | `	{ "is_int",                    1, 0 },` |
|        - |  163 | `	{ "is_integer",                1, 0 },` |
|        - |  164 | `	{ "is_long",                   1, 0 },` |
|        - |  165 | `	{ "is_null",                   1, 0 },` |
|        - |  166 | `	{ "is_numeric",                1, 0 },` |
|        - |  167 | `	{ "is_object",                 1, 0 },` |
|        - |  168 | `	{ "is_resource",               1, 0 },` |
|        - |  169 | `	{ "is_scalar",                 1, 0 },` |
|        - |  170 | `	{ "is_string",                 1, 0 },` |
|        - |  171 | `	{ "print_r",                   1, 1 },` |
|        - |  172 | `	{ "strval",                    1, 0 },` |
|        - |  173 | `	{ "var_dump",                  1, 1 },` |
|        - |  174 | `	{ "var_export",                1, 1 },` |
|        - |  175 | `	/* Array/iterator family */` |
|        - |  176 | `	{ "array_filter",              1, 1 },` |
|        - |  177 | `	{ "array_product",             1, 0 },` |
|        - |  178 | `	{ "array_rand",                1, 1 },` |
|        - |  179 | `	{ "compact",                   1, 1 },` |
|        - |  180 | `	{ "current",                   1, 0 },` |
|        - |  181 | `	{ "end",                       1, 0 },` |
|        - |  182 | `	{ "extract",                   1, 1 },` |
|        - |  183 | `	{ "iterator_apply",            2, 1 },` |
|        - |  184 | `	{ "iterator_count",            1, 0 },` |
|        - |  185 | `	{ "iterator_to_array",         1, 1 },` |
|        - |  186 | `	{ "key",                       1, 0 },` |
|        - |  187 | `	{ "krsort",                    1, 1 },` |
|        - |  188 | `	{ "ksort",                     1, 1 },` |
|        - |  189 | `	{ "next",                      1, 0 },` |
|        - |  190 | `	{ "pos",                       1, 0 },` |
|        - |  191 | `	{ "prev",                      1, 0 },` |
|        - |  192 | `	{ "reset",                     1, 0 },` |
|        - |  193 | `	{ "rsort",                     1, 1 },` |
|        - |  194 | `	{ "shuffle",                   1, 0 },` |
|        - |  195 | `	{ "sort",                      1, 1 },` |
|        - |  196 | `	{ "uasort",                    2, 0 },` |
|        - |  197 | `	{ "uksort",                    2, 0 },` |
|        - |  198 | `	{ "usort",                     2, 0 },` |
|        - |  199 | `	/* Class/reflection family */` |
|        - |  200 | `	{ "class_alias",               2, 1 },` |
|        - |  201 | `	{ "class_exists",              1, 1 },` |
|        - |  202 | `	{ "enum_exists",               1, 1 },` |
|        - |  203 | `	{ "get_class_methods",         1, 0 },` |
|        - |  204 | `	{ "get_class_vars",            1, 0 },` |
|        - |  205 | `	{ "get_object_vars",           1, 0 },` |
|        - |  206 | `	{ "interface_exists",          1, 1 },` |
|        - |  207 | `	{ "trait_exists",              1, 1 },` |
|        - |  208 | `	{ "is_a",                      2, 1 },` |
|        - |  209 | `	{ "is_subclass_of",            2, 1 },` |
|        - |  210 | `	{ "method_exists",             2, 0 },` |
|        - |  211 | `	{ "property_exists",           2, 0 },` |
|        - |  212 | `	{ "spl_autoload",              1, 1 },` |
|        - |  213 | `	{ "spl_autoload_unregister",   1, 0 },` |
|        - |  214 | `	{ "spl_object_hash",           1, 0 },` |
|        - |  215 | `	{ "spl_object_id",             1, 0 },` |
|        - |  216 | `	/* Filesystem/IO family */` |
|        - |  217 | `	{ "basename",                  1, 1 },` |
|        - |  218 | `	{ "chdir",                     1, 0 },` |
|        - |  219 | `	{ "chgrp",                     2, 0 },` |
|        - |  220 | `	{ "dirname",                   1, 1 },` |
|        - |  221 | `	{ "disk_free_space",           1, 0 },` |
|        - |  222 | `	{ "disk_total_space",          1, 0 },` |
|        - |  223 | `	{ "diskfreespace",             1, 0 },` |
|        - |  224 | `	{ "fclose",                    1, 0 },` |
|        - |  225 | `	{ "feof",                      1, 0 },` |
|        - |  226 | `	{ "fflush",                    1, 0 },` |
|        - |  227 | `	{ "fgetc",                     1, 0 },` |
|        - |  228 | `	{ "fgetcsv",                   1, 1 },` |
|        - |  229 | `	{ "file",                      1, 1 },` |
|        - |  230 | `	{ "file_exists",               1, 0 },` |
|        - |  231 | `	{ "fileatime",                 1, 0 },` |
|        - |  232 | `	{ "filectime",                 1, 0 },` |
|        - |  233 | `	{ "filemtime",                 1, 0 },` |
|        - |  234 | `	{ "filesize",                  1, 0 },` |
|        - |  235 | `	{ "filetype",                  1, 0 },` |
|        - |  236 | `	{ "flock",                     2, 1 },` |
|        - |  237 | `	{ "fpassthru",                 1, 0 },` |
|        - |  238 | `	{ "fputcsv",                   2, 1 },` |
|        - |  239 | `	{ "fputs",                     2, 1 },` |
|        - |  240 | `	{ "fseek",                     2, 1 },` |
|        - |  241 | `	{ "fstat",                     1, 0 },` |
|        - |  242 | `	{ "ftell",                     1, 0 },` |
|        - |  243 | `	{ "ftruncate",                 2, 0 },` |
|        - |  244 | `	{ "getopt",                    1, 1 },` |
|        - |  245 | `	{ "is_dir",                    1, 0 },` |
|        - |  246 | `	{ "is_executable",             1, 0 },` |
|        - |  247 | `	{ "is_file",                   1, 0 },` |
|        - |  248 | `	{ "is_link",                   1, 0 },` |
|        - |  249 | `	{ "is_readable",               1, 0 },` |
|        - |  250 | `	{ "is_writable",               1, 0 },` |
|        - |  251 | `	{ "lstat",                     1, 0 },` |
|        - |  252 | `	{ "md5_file",                  1, 1 },` |
|        - |  253 | `	{ "opendir",                   1, 1 },` |
|        - |  254 | `	{ "pathinfo",                  1, 1 },` |
|        - |  255 | `	{ "pclose",                    1, 0 },` |
|        - |  256 | `	{ "realpath",                  1, 0 },` |
|        - |  257 | `	{ "rewind",                    1, 0 },` |
|        - |  258 | `	{ "sha1_file",                 1, 1 },` |
|        - |  259 | `	{ "stat",                      1, 0 },` |
|        - |  260 | `	/* Date family */` |
|        - |  261 | `	{ "date",                      1, 1 },` |
|        - |  262 | `	{ "date_default_timezone_set", 1, 1 },` |
|        - |  263 | `	{ "gmdate",                    1, 1 },` |
|        - |  264 | `	{ "gmmktime",                  1, 1 },` |
|        - |  265 | `	{ "idate",                     1, 1 },` |
|        - |  266 | `	{ "mktime",                    1, 1 },` |
|        - |  267 | `	/* Encoding/URL family */` |
|        - |  268 | `	{ "base64_decode",             1, 1 },` |
|        - |  269 | `	{ "base64_encode",             1, 0 },` |
|        - |  270 | `	{ "convert_uudecode",          1, 0 },` |
|        - |  271 | `	{ "convert_uuencode",          1, 0 },` |
|        - |  272 | `	{ "parse_ini_file",            1, 1 },` |
|        - |  273 | `	{ "parse_ini_string",          1, 1 },` |
|        - |  274 | `	{ "parse_url",                 1, 1 },` |
|        - |  275 | `	{ "rawurldecode",              1, 0 },` |
|        - |  276 | `	{ "rawurlencode",              1, 0 },` |
|        - |  277 | `	{ "urldecode",                 1, 0 },` |
|        - |  278 | `	{ "urlencode",                 1, 0 },` |
|        - |  279 | `	/* JSON/serialize family */` |
|        - |  280 | `	{ "filter_var",                1, 1 },` |
|        - |  281 | `	{ "json_decode",               1, 1 },` |
|        - |  282 | `	{ "json_encode",               1, 1 },` |
|        - |  283 | `	{ "json_validate",             1, 1 },` |
|        - |  284 | `	{ "serialize",                 1, 0 },` |
|        - |  285 | `	{ "unserialize",               1, 1 },` |
|        - |  286 | `	/* PCRE family */` |
|        - |  287 | `	{ "preg_match",                2, 1 },` |
|        - |  288 | `	{ "preg_match_all",            2, 1 },` |
|        - |  289 | `	{ "preg_quote",                1, 1 },` |
|        - |  290 | `	{ "preg_replace",              3, 1 },` |
|        - |  291 | `	{ "preg_replace_callback",     3, 1 },` |
|        - |  292 | `	{ "preg_split",                2, 1 },` |
|        - |  293 | `	/* XML family */` |
|        - |  294 | `	/* Constants/misc family */` |
|        - |  295 | `	{ "call_user_func",            1, 1 },` |
|        - |  296 | `	{ "call_user_func_array",      2, 0 },` |
|        - |  297 | `	{ "constant",                  1, 0 },` |
|        - |  298 | `	{ "define",                    2, 1 },` |
|        - |  299 | `	{ "defined",                   1, 0 },` |
|        - |  300 | `	{ "error_log",                 1, 1 },` |
|        - |  301 | `	{ "fnmatch",                   2, 1 },` |
|        - |  302 | `	{ "forward_static_call",       1, 1 },` |
|        - |  303 | `	{ "forward_static_call_array", 2, 0 },` |
|        - |  304 | `	{ "func_get_arg",              1, 0 },` |
|        - |  305 | `	{ "function_exists",           1, 0 },` |
|        - |  306 | `	{ "header",                    1, 1 },` |
|        - |  307 | `	{ "password_get_info",         1, 0 },` |
|        - |  308 | `	{ "putenv",                    1, 0 },` |
|        - |  309 | `	{ "register_shutdown_function", 1, 1 },` |
|        - |  310 | `	{ "set_error_handler",         1, 1 },` |
|        - |  311 | `	{ "set_exception_handler",     1, 0 },` |
|        - |  312 | `	{ "setcookie",                 1, 1 },` |
|        - |  313 | `	{ "setrawcookie",              1, 1 },` |
|        - |  314 | `	{ "trigger_error",             1, 1 },` |
|        - |  315 | `	{ "user_error",                1, 1 },` |
|        - |  316 | `	/*` |
|        - |  317 | `	 * Overrides for signatures that under-report their own minimum: the callback` |
|        - |  318 | `	 * of these three hides inside the variadic tail ("array $array, ...$rest"),` |
|        - |  319 | `	 * so the derivation reads 1 where php requires 2.` |
|        - |  320 | `	 */` |
|        - |  321 | `	{ "array_udiff",               2, 1 },` |
|        - |  322 | `	{ "array_uintersect",          2, 1 },` |
|        - |  323 | `	{ "array_diff_uassoc",         2, 1 },` |
|        - |  324 | `};` |
|        - |  325 | `/*` |
|        - |  326 | ` * Stamp the minimum-arity metadata from aBuiltinArity[] onto the already` |
|        - |  327 | ` * registered host functions. Called once at VM init after every builtin family` |
|        - |  328 | ` * has been installed into hHostFunction. A name absent from the hash (e.g. a` |
|        - |  329 | ` * build without a given extension) is simply skipped.` |
|        - |  330 | ` */` |
|     3382 |  331 | `PH7_PRIVATE void VmSetBuiltinArity(ph7_vm *pVm)` |
|        5 |  332 | `{` |
|        - |  333 | `	sxu32 n;` |
|   913145 |  334 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinArity) ; ++n ){` |
|   909763 |  335 | `		const struct VmBuiltinArity *p = &aBuiltinArity[n];` |
|  1819521 |  336 | `		SyHashEntry *pEntry = SyHashGet(&pVm->hHostFunction,` |
|   909758 |  337 | `			(const void *)p->zName,SyStrlen(p->zName));` |
|   909763 |  338 | `		if( pEntry ){` |
|   909763 |  339 | `			ph7_user_func *pFunc = (ph7_user_func *)pEntry->pUserData;` |
|   909763 |  340 | `			pFunc->nMinArg  = p->nMin;` |
|   909763 |  341 | `			pFunc->bAtLeast = p->bAtLeast;` |
|   454879 |  342 | `		}` |
|   454884 |  343 | `	}` |
|     3387 |  344 | `}` |
|        - |  345 | `/*` |
|        - |  346 | ` * PHP 8.5 parameter signatures for the C builtins, generated offline from` |
|        - |  347 | ` * a real PHP 8.5 ReflectionFunction dump over PHL's registered function` |
|        - |  348 | ` * list (see the plan's Reflection section). "= ?" marks an optional` |
|        - |  349 | ` * parameter whose default is not representable as a short literal.` |
|        - |  350 | ` * Reflection parses these strings on demand; unlisted builtins degrade to` |
|        - |  351 | ` * the min-arity data.` |
|        - |  352 | ` */` |
|        - |  353 | `static const struct VmBuiltinSig {` |
|        - |  354 | `	const char *zName;` |
|        - |  355 | `	const char *zSig;` |
|        - |  356 | `	const char *zRet;` |
|        - |  357 | `} aBuiltinSig[] = {` |
|        - |  358 | `	{ "abs", "int\|float $num", "int\|float" },` |
|        - |  359 | `	{ "acos", "float $num", "float" },` |
|        - |  360 | `	{ "addcslashes", "string $string, string $characters", "string" },` |
|        - |  361 | `	{ "addslashes", "string $string", "string" },` |
|        - |  362 | `	{ "array_all", "array $array, callable $callback", "bool" },` |
|        - |  363 | `	{ "array_any", "array $array, callable $callback", "bool" },` |
|        - |  364 | `	{ "array_chunk", "array $array, int $length, bool $preserve_keys = false", "array" },` |
|        - |  365 | `	{ "array_column", "array $array, string\|int\|null $column_key, string\|int\|null $index_key = NULL", "array" },` |
|        - |  366 | `	{ "array_combine", "array $keys, array $values", "array" },` |
|        - |  367 | `	{ "array_diff", "array $array, array ...$arrays = ?", "array" },` |
|        - |  368 | `	{ "array_diff_assoc", "array $array, array ...$arrays = ?", "array" },` |
|        - |  369 | `	{ "array_diff_key", "array $array, array ...$arrays = ?", "array" },` |
|        - |  370 | `	{ "array_diff_uassoc", "array $array, ...$rest = ?", "array" },` |
|        - |  371 | `	{ "array_fill", "int $start_index, int $count, mixed $value", "array" },` |
|        - |  372 | `	{ "array_fill_keys", "array $keys, mixed $value", "array" },` |
|        - |  373 | `	{ "array_filter", "array $array, ?callable $callback = NULL, int $mode = 0", "array" },` |
|        - |  374 | `	{ "array_find", "array $array, callable $callback", "mixed" },` |
|        - |  375 | `	{ "array_find_key", "array $array, callable $callback", "mixed" },` |
|        - |  376 | `	{ "array_first", "array $array", "mixed" },` |
|        - |  377 | `	{ "array_flip", "array $array", "array" },` |
|        - |  378 | `	{ "array_intersect", "array $array, array ...$arrays = ?", "array" },` |
|        - |  379 | `	{ "array_intersect_assoc", "array $array, array ...$arrays = ?", "array" },` |
|        - |  380 | `	{ "array_intersect_key", "array $array, array ...$arrays = ?", "array" },` |
|        - |  381 | `	{ "array_is_list", "array $array", "bool" },` |
|        - |  382 | `	{ "array_key_exists", "$key, array $array", "bool" },` |
|        - |  383 | `	{ "array_key_first", "array $array", "string\|int\|null" },` |
|        - |  384 | `	{ "array_key_last", "array $array", "string\|int\|null" },` |
|        - |  385 | `	{ "array_keys", "array $array, mixed $filter_value = ?, bool $strict = false", "array" },` |
|        - |  386 | `	{ "array_last", "array $array", "mixed" },` |
|        - |  387 | `	{ "array_map", "?callable $callback, array $array, array ...$arrays = ?", "array" },` |
|        - |  388 | `	{ "array_merge", "array ...$arrays = ?", "array" },` |
|        - |  389 | `	{ "array_pad", "array $array, int $length, mixed $value", "array" },` |
|        - |  390 | `	{ "array_pop", "array &$array", "mixed" },` |
|        - |  391 | `	{ "array_product", "array $array", "int\|float" },` |
|        - |  392 | `	{ "array_push", "array &$array, mixed ...$values = ?", "int" },` |
|        - |  393 | `	{ "array_rand", "array $array, int $num = 1", "array\|string\|int" },` |
|        - |  394 | `	{ "array_reduce", "array $array, callable $callback, mixed $initial = NULL", "mixed" },` |
|        - |  395 | `	{ "array_replace", "array $array, array ...$replacements = ?", "array" },` |
|        - |  396 | `	{ "array_reverse", "array $array, bool $preserve_keys = false", "array" },` |
|        - |  397 | `	{ "array_search", "mixed $needle, array $haystack, bool $strict = false", "string\|int\|false" },` |
|        - |  398 | `	{ "array_shift", "array &$array", "mixed" },` |
|        - |  399 | `	{ "array_slice", "array $array, int $offset, ?int $length = NULL, bool $preserve_keys = false", "array" },` |
|        - |  400 | `	{ "array_splice", "array &$array, int $offset, ?int $length = NULL, mixed $replacement = ?", "array" },` |
|        - |  401 | `	{ "array_sum", "array $array", "int\|float" },` |
|        - |  402 | `	{ "array_udiff", "array $array, ...$rest = ?", "array" },` |
|        - |  403 | `	{ "array_uintersect", "array $array, ...$rest = ?", "array" },` |
|        - |  404 | `	{ "array_unique", "array $array, int $flags = 2", "array" },` |
|        - |  405 | `	{ "array_values", "array $array", "array" },` |
|        - |  406 | `	{ "array_walk", "object\|array &$array, callable $callback, mixed $arg = ?", "true" },` |
|        - |  407 | `	{ "array_walk_recursive", "object\|array &$array, callable $callback, mixed $arg = ?", "true" },` |
|        - |  408 | `	{ "arsort", "array &$array, int $flags = 0", "true" },` |
|        - |  409 | `	{ "asin", "float $num", "float" },` |
|        - |  410 | `	{ "asort", "array &$array, int $flags = 0", "true" },` |
|        - |  411 | `	{ "assert", "mixed $assertion, Throwable\|string\|null $description = NULL", "bool" },` |
|        - |  412 | `	{ "atan", "float $num", "float" },` |
|        - |  413 | `	{ "atan2", "float $y, float $x", "float" },` |
|        - |  414 | `	{ "base64_decode", "string $string, bool $strict = false", "string\|false" },` |
|        - |  415 | `	{ "base64_encode", "string $string", "string" },` |
|        - |  416 | `	{ "base_convert", "string $num, int $from_base, int $to_base", "string" },` |
|        - |  417 | `	{ "basename", "string $path, string $suffix = ''", "string" },` |
|        - |  418 | `	{ "bin2hex", "string $string", "string" },` |
|        - |  419 | `	{ "bindec", "string $binary_string", "int\|float" },` |
|        - |  420 | `	{ "boolval", "mixed $value", "bool" },` |
|        - |  421 | `	{ "call_user_func", "callable $callback, mixed ...$args = ?", "mixed" },` |
|        - |  422 | `	{ "call_user_func_array", "callable $callback, array $args", "mixed" },` |
|        - |  423 | `	{ "ceil", "int\|float $num", "float" },` |
|        - |  424 | `	{ "chdir", "string $directory", "bool" },` |
|        - |  425 | `	{ "chgrp", "string $filename, string\|int $group", "bool" },` |
|        - |  426 | `	{ "chmod", "string $filename, int $permissions", "bool" },` |
|        - |  427 | `	{ "chop", "string $string, string $characters = ?", "string" },` |
|        - |  428 | `	{ "chown", "string $filename, string\|int $user", "bool" },` |
|        - |  429 | `	{ "chr", "int $codepoint", "string" },` |
|        - |  430 | `	{ "chunk_split", "string $string, int $length = 76, string $separator = ?", "string" },` |
|        - |  431 | `	{ "class_alias", "string $class, string $alias, bool $autoload = true", "bool" },` |
|        - |  432 | `	{ "class_exists", "string $class, bool $autoload = true", "bool" },` |
|        - |  433 | `	{ "enum_exists", "string $enum, bool $autoload = true", "bool" },` |
|        - |  434 | `	{ "closedir", "$dir_handle = NULL", "void" },` |
|        - |  435 | `	{ "compact", "$var_name, ...$var_names = ?", "array" },` |
|        - |  436 | `	{ "constant", "string $name", "mixed" },` |
|        - |  437 | `	{ "convert_uudecode", "string $string", "string\|false" },` |
|        - |  438 | `	{ "convert_uuencode", "string $string", "string" },` |
|        - |  439 | `	{ "copy", "string $from, string $to, $context = NULL", "bool" },` |
|        - |  440 | `	{ "cos", "float $num", "float" },` |
|        - |  441 | `	{ "cosh", "float $num", "float" },` |
|        - |  442 | `	{ "count", "Countable\|array $value, int $mode = 0", "int" },` |
|        - |  443 | `	{ "crc32", "string $string", "int" },` |
|        - |  444 | `	{ "ctype_alnum", "mixed $text", "bool" },` |
|        - |  445 | `	{ "ctype_alpha", "mixed $text", "bool" },` |
|        - |  446 | `	{ "ctype_cntrl", "mixed $text", "bool" },` |
|        - |  447 | `	{ "ctype_digit", "mixed $text", "bool" },` |
|        - |  448 | `	{ "ctype_graph", "mixed $text", "bool" },` |
|        - |  449 | `	{ "ctype_lower", "mixed $text", "bool" },` |
|        - |  450 | `	{ "ctype_print", "mixed $text", "bool" },` |
|        - |  451 | `	{ "ctype_punct", "mixed $text", "bool" },` |
|        - |  452 | `	{ "ctype_space", "mixed $text", "bool" },` |
|        - |  453 | `	{ "ctype_upper", "mixed $text", "bool" },` |
|        - |  454 | `	{ "ctype_xdigit", "mixed $text", "bool" },` |
|        - |  455 | `	{ "current", "object\|array $array", "mixed" },` |
|        - |  456 | `	{ "date", "string $format, ?int $timestamp = NULL", "string" },` |
|        - |  457 | `	{ "date_default_timezone_get", "", "string" },` |
|        - |  458 | `	{ "date_default_timezone_set", "string $timezoneId", "bool" },` |
|        - |  459 | `	{ "debug_backtrace", "int $options = 1, int $limit = 0", "array" },` |
|        - |  460 | `	{ "debug_print_backtrace", "int $options = 0, int $limit = 0", "void" },` |
|        - |  461 | `	{ "decbin", "int $num", "string" },` |
|        - |  462 | `	{ "dechex", "int $num", "string" },` |
|        - |  463 | `	{ "decoct", "int $num", "string" },` |
|        - |  464 | `	{ "define", "string $constant_name, mixed $value, bool $case_insensitive = false", "bool" },` |
|        - |  465 | `	{ "defined", "string $constant_name", "bool" },` |
|        - |  466 | `	{ "die", "string\|int $status = 0", "never" },` |
|        - |  467 | `	{ "dirname", "string $path, int $levels = 1", "string" },` |
|        - |  468 | `	{ "disk_free_space", "string $directory", "float\|false" },` |
|        - |  469 | `	{ "disk_total_space", "string $directory", "float\|false" },` |
|        - |  470 | `	{ "diskfreespace", "string $directory", "float\|false" },` |
|        - |  471 | `	{ "end", "object\|array &$array", "mixed" },` |
|        - |  472 | `	{ "error_get_last", "", "?array" },` |
|        - |  473 | `	{ "error_clear_last", "", "void" },` |
|        - |  474 | `	{ "error_log", "string $message, int $message_type = 0, ?string $destination = NULL, ?string $additional_headers = NULL", "bool" },` |
|        - |  475 | `	{ "error_reporting", "?int $error_level = NULL", "int" },` |
|        - |  476 | `	{ "exit", "string\|int $status = 0", "never" },` |
|        - |  477 | `	{ "exp", "float $num", "float" },` |
|        - |  478 | `	{ "explode", "string $separator, string $string, int $limit = 9223372036854775807", "array" },` |
|        - |  479 | `	{ "extract", "array &$array, int $flags = 0, string $prefix = ''", "int" },` |
|        - |  480 | `	{ "fclose", "$stream", "bool" },` |
|        - |  481 | `	{ "feof", "$stream", "bool" },` |
|        - |  482 | `	{ "fflush", "$stream", "bool" },` |
|        - |  483 | `	{ "fgetc", "$stream", "string\|false" },` |
|        - |  484 | `	{ "fgetcsv", "$stream, ?int $length = NULL, string $separator = ',', string $enclosure = '\"', string $escape = '\\\\'", "array\|false" },` |
|        - |  485 | `	{ "fgets", "$stream, ?int $length = NULL", "string\|false" },` |
|        - |  486 | `	{ "file", "string $filename, int $flags = 0, $context = NULL", "array\|false" },` |
|        - |  487 | `	{ "file_exists", "string $filename", "bool" },` |
|        - |  488 | `	{ "file_get_contents", "string $filename, bool $use_include_path = false, $context = NULL, int $offset = 0, ?int $length = NULL", "string\|false" },` |
|        - |  489 | `	{ "file_put_contents", "string $filename, mixed $data, int $flags = 0, $context = NULL", "int\|false" },` |
|        - |  490 | `	{ "fileatime", "string $filename", "int\|false" },` |
|        - |  491 | `	{ "filectime", "string $filename", "int\|false" },` |
|        - |  492 | `	{ "filemtime", "string $filename", "int\|false" },` |
|        - |  493 | `	{ "filesize", "string $filename", "int\|false" },` |
|        - |  494 | `	{ "filetype", "string $filename", "string\|false" },` |
|        - |  495 | `	{ "filter_input", "int $type, string $var_name, int $filter = 516, array\|int $options = 0", "mixed" },` |
|        - |  496 | `	{ "filter_var", "mixed $value, int $filter = 516, array\|int $options = 0", "mixed" },` |
|        - |  497 | `	{ "floatval", "mixed $value", "float" },` |
|        - |  498 | `	{ "flock", "$stream, int $operation, &$would_block = NULL", "bool" },` |
|        - |  499 | `	{ "floor", "int\|float $num", "float" },` |
|        - |  500 | `	{ "flush", "", "void" },` |
|        - |  501 | `	{ "fmod", "float $num1, float $num2", "float" },` |
|        - |  502 | `	{ "fnmatch", "string $pattern, string $filename, int $flags = 0", "bool" },` |
|        - |  503 | `	{ "fopen", "string $filename, string $mode, bool $use_include_path = false, $context = NULL", "" },` |
|        - |  504 | `	{ "forward_static_call", "callable $callback, mixed ...$args = ?", "mixed" },` |
|        - |  505 | `	{ "forward_static_call_array", "callable $callback, array $args", "mixed" },` |
|        - |  506 | `	{ "fpassthru", "$stream", "int" },` |
|        - |  507 | `	{ "fprintf", "$stream, string $format, mixed ...$values = ?", "int" },` |
|        - |  508 | `	{ "fputcsv", "$stream, array $fields, string $separator = ',', string $enclosure = '\"', string $escape = '\\\\', string $eol = ?", "int\|false" },` |
|        - |  509 | `	{ "fputs", "$stream, string $data, ?int $length = NULL", "int\|false" },` |
|        - |  510 | `	{ "fread", "$stream, int $length", "string\|false" },` |
|        - |  511 | `	{ "fseek", "$stream, int $offset, int $whence = 0", "int" },` |
|        - |  512 | `	{ "fstat", "$stream", "array\|false" },` |
|        - |  513 | `	{ "ftell", "$stream", "int\|false" },` |
|        - |  514 | `	{ "ftruncate", "$stream, int $size", "bool" },` |
|        - |  515 | `	{ "func_get_arg", "int $position", "mixed" },` |
|        - |  516 | `	{ "func_get_args", "", "array" },` |
|        - |  517 | `	{ "func_num_args", "", "int" },` |
|        - |  518 | `	{ "function_exists", "string $function", "bool" },` |
|        - |  519 | `	{ "fwrite", "$stream, string $data, ?int $length = NULL", "int\|false" },` |
|        - |  520 | `	{ "gc_collect_cycles", "", "int" },` |
|        - |  521 | `	{ "gc_disable", "", "void" },` |
|        - |  522 | `	{ "gc_enable", "", "void" },` |
|        - |  523 | `	{ "gc_enabled", "", "bool" },` |
|        - |  524 | `	{ "gc_mem_caches", "", "int" },` |
|        - |  525 | `	{ "gc_status", "", "array" },` |
|        - |  526 | `	{ "get_called_class", "", "string" },` |
|        - |  527 | `	{ "get_class", "object $object = ?", "string" },` |
|        - |  528 | `	{ "get_class_methods", "object\|string $object_or_class", "array" },` |
|        - |  529 | `	{ "get_class_vars", "string $class", "array" },` |
|        - |  530 | `	{ "get_current_user", "", "string" },` |
|        - |  531 | `	{ "get_declared_classes", "", "array" },` |
|        - |  532 | `	{ "get_declared_interfaces", "", "array" },` |
|        - |  533 | `	{ "get_defined_constants", "bool $categorize = false", "array" },` |
|        - |  534 | `	{ "get_defined_functions", "bool $exclude_disabled = true", "array" },` |
|        - |  535 | `	{ "get_defined_vars", "", "array" },` |
|        - |  536 | `	{ "get_html_translation_table", "int $table = 0, int $flags = 11, string $encoding = 'UTF-8'", "array" },` |
|        - |  537 | `	{ "get_include_path", "", "string\|false" },` |
|        - |  538 | `	{ "get_included_files", "", "array" },` |
|        - |  539 | `	{ "get_object_vars", "object $object", "array" },` |
|        - |  540 | `	{ "get_parent_class", "object\|string $object_or_class = ?", "string\|false" },` |
|        - |  541 | `	{ "get_resource_id", "$resource", "int" },` |
|        - |  542 | `	{ "get_resource_type", "$resource", "string" },` |
|        - |  543 | `	{ "getcwd", "", "string\|false" },` |
|        - |  544 | `	{ "getdate", "?int $timestamp = NULL", "array" },` |
|        - |  545 | `	{ "getenv", "?string $name = NULL, bool $local_only = false", "array\|string\|false" },` |
|        - |  546 | `	{ "getmygid", "", "int\|false" },` |
|        - |  547 | `	{ "getmypid", "", "int\|false" },` |
|        - |  548 | `	{ "getmyuid", "", "int\|false" },` |
|        - |  549 | `	{ "getopt", "string $short_options, array $long_options = ?, &$rest_index = NULL", "array\|false" },` |
|        - |  550 | `	{ "getrandmax", "", "int" },` |
|        - |  551 | `	{ "gettimeofday", "bool $as_float = false", "array\|float" },` |
|        - |  552 | `	{ "gettype", "mixed $value", "string" },` |
|        - |  553 | `	{ "gmdate", "string $format, ?int $timestamp = NULL", "string" },` |
|        - |  554 | `	{ "gmmktime", "int $hour, ?int $minute = NULL, ?int $second = NULL, ?int $month = NULL, ?int $day = NULL, ?int $year = NULL", "int\|false" },` |
|        - |  555 | `	{ "hash", "string $algo, string $data, bool $binary = false, array $options = ?", "string" },` |
|        - |  556 | `	{ "hash_algos", "", "array" },` |
|        - |  557 | `	{ "hash_equals", "string $known_string, string $user_string", "bool" },` |
|        - |  558 | `	{ "hash_hmac", "string $algo, string $data, string $key, bool $binary = false", "string" },` |
|        - |  559 | `	{ "header", "string $header, bool $replace = true, int $response_code = 0", "void" },` |
|        - |  560 | `	{ "header_remove", "?string $name = NULL", "void" },` |
|        - |  561 | `	{ "headers_list", "", "array" },` |
|        - |  562 | `	{ "headers_sent", "&$filename = NULL, &$line = NULL", "bool" },` |
|        - |  563 | `	{ "hexdec", "string $hex_string", "int\|float" },` |
|        - |  564 | `	{ "html_entity_decode", "string $string, int $flags = 11, ?string $encoding = NULL", "string" },` |
|        - |  565 | `	{ "htmlentities", "string $string, int $flags = 11, ?string $encoding = NULL, bool $double_encode = true", "string" },` |
|        - |  566 | `	{ "htmlspecialchars", "string $string, int $flags = 11, ?string $encoding = NULL, bool $double_encode = true", "string" },` |
|        - |  567 | `	{ "htmlspecialchars_decode", "string $string, int $flags = 11", "string" },` |
|        - |  568 | `	{ "http_response_code", "int $response_code = 0", "int\|bool" },` |
|        - |  569 | `	{ "hypot", "float $x, float $y", "float" },` |
|        - |  570 | `	{ "idate", "string $format, ?int $timestamp = NULL", "int\|false" },` |
|        - |  571 | `	{ "implode", "array\|string $separator, ?array $array = NULL", "string" },` |
|        - |  572 | `	{ "in_array", "mixed $needle, array $haystack, bool $strict = false", "bool" },` |
|        - |  573 | `	{ "intdiv", "int $num1, int $num2", "int" },` |
|        - |  574 | `	{ "interface_exists", "string $interface, bool $autoload = true", "bool" },` |
|        - |  575 | `	{ "trait_exists", "string $trait, bool $autoload = true", "bool" },` |
|        - |  576 | `	{ "intval", "mixed $value, int $base = 10", "int" },` |
|        - |  577 | `	{ "is_a", "mixed $object_or_class, string $class, bool $allow_string = false", "bool" },` |
|        - |  578 | `	{ "is_array", "mixed $value", "bool" },` |
|        - |  579 | `	{ "is_bool", "mixed $value", "bool" },` |
|        - |  580 | `	{ "is_callable", "mixed $value, bool $syntax_only = false, &$callable_name = NULL", "bool" },` |
|        - |  581 | `	{ "is_dir", "string $filename", "bool" },` |
|        - |  582 | `	{ "is_double", "mixed $value", "bool" },` |
|        - |  583 | `	{ "is_executable", "string $filename", "bool" },` |
|        - |  584 | `	{ "is_file", "string $filename", "bool" },` |
|        - |  585 | `	{ "is_float", "mixed $value", "bool" },` |
|        - |  586 | `	{ "is_int", "mixed $value", "bool" },` |
|        - |  587 | `	{ "is_integer", "mixed $value", "bool" },` |
|        - |  588 | `	{ "is_link", "string $filename", "bool" },` |
|        - |  589 | `	{ "is_long", "mixed $value", "bool" },` |
|        - |  590 | `	{ "is_null", "mixed $value", "bool" },` |
|        - |  591 | `	{ "is_numeric", "mixed $value", "bool" },` |
|        - |  592 | `	{ "is_object", "mixed $value", "bool" },` |
|        - |  593 | `	{ "is_readable", "string $filename", "bool" },` |
|        - |  594 | `	{ "is_resource", "mixed $value", "bool" },` |
|        - |  595 | `	{ "is_scalar", "mixed $value", "bool" },` |
|        - |  596 | `	{ "is_string", "mixed $value", "bool" },` |
|        - |  597 | `	{ "is_subclass_of", "mixed $object_or_class, string $class, bool $allow_string = true", "bool" },` |
|        - |  598 | `	{ "is_writable", "string $filename", "bool" },` |
|        - |  599 | `	{ "iterator_apply", "Traversable $iterator, callable $callback, ?array $args = NULL", "int" },` |
|        - |  600 | `	{ "iterator_count", "Traversable\|array $iterator", "int" },` |
|        - |  601 | `	{ "iterator_to_array", "Traversable\|array $iterator, bool $preserve_keys = true", "array" },` |
|        - |  602 | `	{ "join", "array\|string $separator, ?array $array = NULL", "string" },` |
|        - |  603 | `	{ "json_decode", "string $json, ?bool $associative = NULL, int $depth = 512, int $flags = 0", "mixed" },` |
|        - |  604 | `	{ "json_encode", "mixed $value, int $flags = 0, int $depth = 512", "string\|false" },` |
|        - |  605 | `	{ "json_last_error", "", "int" },` |
|        - |  606 | `	{ "json_last_error_msg", "", "string" },` |
|        - |  607 | `	{ "json_validate", "string $json, int $depth = 512, int $flags = 0", "bool" },` |
|        - |  608 | `	{ "key", "object\|array $array", "string\|int\|null" },` |
|        - |  609 | `	{ "krsort", "array &$array, int $flags = 0", "true" },` |
|        - |  610 | `	{ "ksort", "array &$array, int $flags = 0", "true" },` |
|        - |  611 | `	{ "lcfirst", "string $string", "string" },` |
|        - |  612 | `	{ "levenshtein", "string $string1, string $string2, int $insertion_cost = 1, int $replacement_cost = 1, int $deletion_cost = 1", "int" },` |
|        - |  613 | `	{ "link", "string $target, string $link", "bool" },` |
|        - |  614 | `	{ "localtime", "?int $timestamp = NULL, bool $associative = false", "array" },` |
|        - |  615 | `	{ "log", "float $num, float $base = 2.718281828459045", "float" },` |
|        - |  616 | `	{ "log10", "float $num", "float" },` |
|        - |  617 | `	{ "lstat", "string $filename", "array\|false" },` |
|        - |  618 | `	{ "ltrim", "string $string, string $characters = ?", "string" },` |
|        - |  619 | `	{ "max", "mixed $value, mixed ...$values = ?", "mixed" },` |
|        - |  620 | `	{ "mb_chr", "int $codepoint, ?string $encoding = NULL", "string\|false" },` |
|        - |  621 | `	{ "mb_ord", "string $string, ?string $encoding = NULL", "int\|false" },` |
|        - |  622 | `	{ "mb_strtolower", "string $string, ?string $encoding = NULL", "string" },` |
|        - |  623 | `	{ "mb_strtoupper", "string $string, ?string $encoding = NULL", "string" },` |
|        - |  624 | `	{ "md5", "string $string, bool $binary = false", "string" },` |
|        - |  625 | `	{ "md5_file", "string $filename, bool $binary = false", "string\|false" },` |
|        - |  626 | `	{ "method_exists", "$object_or_class, string $method", "bool" },` |
|        - |  627 | `	{ "memory_get_peak_usage", "bool $real_usage = false", "int" },` |
|        - |  628 | `	{ "memory_get_usage", "bool $real_usage = false", "int" },` |
|        - |  629 | `	{ "microtime", "bool $as_float = false", "string\|float" },` |
|        - |  630 | `	{ "min", "mixed $value, mixed ...$values = ?", "mixed" },` |
|        - |  631 | `	{ "mkdir", "string $directory, int $permissions = 511, bool $recursive = false, $context = NULL", "bool" },` |
|        - |  632 | `	{ "mktime", "int $hour, ?int $minute = NULL, ?int $second = NULL, ?int $month = NULL, ?int $day = NULL, ?int $year = NULL", "int\|false" },` |
|        - |  633 | `	{ "mt_getrandmax", "", "int" },` |
|        - |  634 | `	{ "mt_rand", "int $min = ?, int $max = ?", "int" },` |
|        - |  635 | `	{ "mt_srand", "?int $seed = NULL, int $mode = 0", "void" },` |
|        - |  636 | `	{ "next", "object\|array &$array", "mixed" },` |
|        - |  637 | `	{ "nl2br", "string $string, bool $use_xhtml = true", "string" },` |
|        - |  638 | `	{ "ob_clean", "", "bool" },` |
|        - |  639 | `	{ "ob_end_clean", "", "bool" },` |
|        - |  640 | `	{ "ob_end_flush", "", "bool" },` |
|        - |  641 | `	{ "ob_flush", "", "bool" },` |
|        - |  642 | `	{ "ob_get_clean", "", "string\|false" },` |
|        - |  643 | `	{ "ob_get_contents", "", "string\|false" },` |
|        - |  644 | `	{ "ob_get_flush", "", "string\|false" },` |
|        - |  645 | `	{ "ob_get_length", "", "int\|false" },` |
|        - |  646 | `	{ "ob_get_level", "", "int" },` |
|        - |  647 | `	{ "ob_implicit_flush", "bool $enable = true", "void" },` |
|        - |  648 | `	{ "ob_list_handlers", "", "array" },` |
|        - |  649 | `	{ "ob_start", "$callback = NULL, int $chunk_size = 0, int $flags = 112", "bool" },` |
|        - |  650 | `	{ "octdec", "string $octal_string", "int\|float" },` |
|        - |  651 | `	{ "opendir", "string $directory, $context = NULL", "" },` |
|        - |  652 | `	{ "ord", "string $character", "int" },` |
|        - |  653 | `	{ "parse_ini_file", "string $filename, bool $process_sections = false, int $scanner_mode = 0", "array\|false" },` |
|        - |  654 | `	{ "parse_ini_string", "string $ini_string, bool $process_sections = false, int $scanner_mode = 0", "array\|false" },` |
|        - |  655 | `	{ "parse_url", "string $url, int $component = -1", "array\|string\|int\|false\|null" },` |
|        - |  656 | `	{ "password_get_info", "string $hash", "array" },` |
|        - |  657 | `	{ "password_hash", "string $password, string\|int\|null $algo, array $options = ?", "string" },` |
|        - |  658 | `	{ "password_needs_rehash", "string $hash, string\|int\|null $algo, array $options = ?", "bool" },` |
|        - |  659 | `	{ "password_verify", "string $password, string $hash", "bool" },` |
|        - |  660 | `	{ "pathinfo", "string $path, int $flags = 15", "array\|string" },` |
|        - |  661 | `	{ "pclose", "$handle", "int" },` |
|        - |  662 | `	{ "php_sapi_name", "", "string\|false" },` |
|        - |  663 | `	{ "php_uname", "string $mode = 'a'", "string" },` |
|        - |  664 | `	{ "phpinfo", "int $flags = 4294967295", "true" },` |
|        - |  665 | `	{ "phpversion", "?string $extension = NULL", "string\|false" },` |
|        - |  666 | `	{ "pi", "", "float" },` |
|        - |  667 | `	{ "popen", "string $command, string $mode", "" },` |
|        - |  668 | `	{ "pos", "object\|array $array", "mixed" },` |
|        - |  669 | `	{ "pow", "mixed $num, mixed $exponent", "object\|int\|float" },` |
|        - |  670 | `	{ "preg_last_error", "", "int" },` |
|        - |  671 | `	{ "preg_last_error_msg", "", "string" },` |
|        - |  672 | `	{ "fsockopen", "string $hostname, int $port = -1, &$error_code = NULL, &$error_message = NULL, ?float $timeout = NULL", "resource\|false" },` |
|        - |  673 | `	{ "pfsockopen", "string $hostname, int $port = -1, &$error_code = NULL, &$error_message = NULL, ?float $timeout = NULL", "resource\|false" },` |
|        - |  674 | `	{ "stream_socket_client", "string $address, &$error_code = NULL, &$error_message = NULL, ?float $timeout = NULL, int $flags = 4, $context = NULL", "resource\|false" },` |
|        - |  675 | `	{ "preg_match", "string $pattern, string $subject, &$matches = NULL, int $flags = 0, int $offset = 0", "int\|false" },` |
|        - |  676 | `	{ "preg_match_all", "string $pattern, string $subject, &$matches = NULL, int $flags = 0, int $offset = 0", "int\|false" },` |
|        - |  677 | `	{ "preg_quote", "string $str, ?string $delimiter = NULL", "string" },` |
|        - |  678 | `	{ "preg_replace", "array\|string $pattern, array\|string $replacement, array\|string $subject, int $limit = -1, &$count = NULL", "array\|string\|null" },` |
|        - |  679 | `	{ "preg_replace_callback", "array\|string $pattern, callable $callback, array\|string $subject, int $limit = -1, &$count = NULL, int $flags = 0", "array\|string\|null" },` |
|        - |  680 | `	{ "preg_split", "string $pattern, string $subject, int $limit = -1, int $flags = 0", "array\|false" },` |
|        - |  681 | `	{ "prev", "object\|array &$array", "mixed" },` |
|        - |  682 | `	{ "print_r", "mixed $value, bool $return = false", "string\|true" },` |
|        - |  683 | `	{ "printf", "string $format, mixed ...$values = ?", "int" },` |
|        - |  684 | `	{ "property_exists", "$object_or_class, string $property", "bool" },` |
|        - |  685 | `	{ "putenv", "string $assignment", "bool" },` |
|        - |  686 | `	{ "quotemeta", "string $string", "string" },` |
|        - |  687 | `	{ "rand", "int $min = ?, int $max = ?", "int" },` |
|        - |  688 | `	{ "random_bytes", "int $length", "string" },` |
|        - |  689 | `	{ "random_int", "int $min, int $max", "int" },` |
|        - |  690 | `	{ "range", "string\|int\|float $start, string\|int\|float $end, int\|float $step = 1", "array" },` |
|        - |  691 | `	{ "rawurldecode", "string $string", "string" },` |
|        - |  692 | `	{ "rawurlencode", "string $string", "string" },` |
|        - |  693 | `	{ "readdir", "$dir_handle = NULL", "string\|false" },` |
|        - |  694 | `	{ "readfile", "string $filename, bool $use_include_path = false, $context = NULL", "int\|false" },` |
|        - |  695 | `	{ "realpath", "string $path", "string\|false" },` |
|        - |  696 | `	{ "register_shutdown_function", "callable $callback, mixed ...$args = ?", "void" },` |
|        - |  697 | `	{ "rename", "string $from, string $to, $context = NULL", "bool" },` |
|        - |  698 | `	{ "reset", "object\|array &$array", "mixed" },` |
|        - |  699 | `	{ "restore_error_handler", "", "true" },` |
|        - |  700 | `	{ "restore_exception_handler", "", "true" },` |
|        - |  701 | `	{ "rewind", "$stream", "bool" },` |
|        - |  702 | `	{ "rewinddir", "$dir_handle = NULL", "void" },` |
|        - |  703 | `	{ "rmdir", "string $directory, $context = NULL", "bool" },` |
|        - |  704 | `	{ "round", "int\|float $num, int $precision = 0, RoundingMode\|int $mode = ?", "float" },` |
|        - |  705 | `	{ "rsort", "array &$array, int $flags = 0", "true" },` |
|        - |  706 | `	{ "rtrim", "string $string, string $characters = ?", "string" },` |
|        - |  707 | `	{ "serialize", "mixed $value", "string" },` |
|        - |  708 | `	{ "set_error_handler", "?callable $callback, int $error_levels = 30719", "" },` |
|        - |  709 | `	{ "set_exception_handler", "?callable $callback", "" },` |
|        - |  710 | `	{ "get_error_handler", "", "?callable" },` |
|        - |  711 | `	{ "get_exception_handler", "", "?callable" },` |
|        - |  712 | `	{ "hrtime", "bool $as_number = false", "array\|int" },` |
|        - |  713 | `	{ "setcookie", "string $name, string $value = '', array\|int $expires_or_options = 0, string $path = '', string $domain = '', bool $secure = false, bool $httponly = false", "bool" },` |
|        - |  714 | `	{ "setrawcookie", "string $name, string $value = '', array\|int $expires_or_options = 0, string $path = '', string $domain = '', bool $secure = false, bool $httponly = false", "bool" },` |
|        - |  715 | `	{ "sha1", "string $string, bool $binary = false", "string" },` |
|        - |  716 | `	{ "sha1_file", "string $filename, bool $binary = false", "string\|false" },` |
|        - |  717 | `	{ "shuffle", "array &$array", "true" },` |
|        - |  718 | `	{ "similar_text", "string $string1, string $string2, &$percent = NULL", "int" },` |
|        - |  719 | `	{ "sin", "float $num", "float" },` |
|        - |  720 | `	{ "sinh", "float $num", "float" },` |
|        - |  721 | `	{ "sizeof", "Countable\|array $value, int $mode = 0", "int" },` |
|        - |  722 | `	{ "sleep", "int $seconds", "int" },` |
|        - |  723 | `	{ "sort", "array &$array, int $flags = 0", "true" },` |
|        - |  724 | `	{ "soundex", "string $string", "string" },` |
|        - |  725 | `	{ "spl_autoload", "string $class, ?string $file_extensions = NULL", "void" },` |
|        - |  726 | `	{ "spl_autoload_functions", "", "array" },` |
|        - |  727 | `	{ "spl_autoload_register", "?callable $callback = NULL, bool $throw = true, bool $prepend = false", "bool" },` |
|        - |  728 | `	{ "spl_autoload_unregister", "callable $callback", "bool" },` |
|        - |  729 | `	{ "spl_object_hash", "object $object", "string" },` |
|        - |  730 | `	{ "spl_object_id", "object $object", "int" },` |
|        - |  731 | `	{ "sprintf", "string $format, mixed ...$values = ?", "string" },` |
|        - |  732 | `	{ "sqrt", "float $num", "float" },` |
|        - |  733 | `	{ "srand", "?int $seed = NULL, int $mode = 0", "void" },` |
|        - |  734 | `	{ "stat", "string $filename", "array\|false" },` |
|        - |  735 | `	{ "str_contains", "string $haystack, string $needle", "bool" },` |
|        - |  736 | `	{ "str_ends_with", "string $haystack, string $needle", "bool" },` |
|        - |  737 | `	{ "str_getcsv", "string $string, string $separator = ',', string $enclosure = '\"', string $escape = '\\\\'", "array" },` |
|        - |  738 | `	{ "str_ireplace", "array\|string $search, array\|string $replace, array\|string $subject, &$count = NULL", "array\|string" },` |
|        - |  739 | `	{ "str_pad", "string $string, int $length, string $pad_string = ' ', int $pad_type = 1", "string" },` |
|        - |  740 | `	{ "str_repeat", "string $string, int $times", "string" },` |
|        - |  741 | `	{ "str_replace", "array\|string $search, array\|string $replace, array\|string $subject, &$count = NULL", "array\|string" },` |
|        - |  742 | `	{ "str_shuffle", "string $string", "string" },` |
|        - |  743 | `	{ "str_split", "string $string, int $length = 1", "array" },` |
|        - |  744 | `	{ "str_starts_with", "string $haystack, string $needle", "bool" },` |
|        - |  745 | `	{ "str_word_count", "string $string, int $format = 0, ?string $characters = NULL", "array\|int" },` |
|        - |  746 | `	{ "strcasecmp", "string $string1, string $string2", "int" },` |
|        - |  747 | `	{ "strchr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|        - |  748 | `	{ "strcmp", "string $string1, string $string2", "int" },` |
|        - |  749 | `	{ "strnatcasecmp", "string $string1, string $string2", "int" },` |
|        - |  750 | `	{ "strnatcmp", "string $string1, string $string2", "int" },` |
|        - |  751 | `	{ "strcoll", "string $string1, string $string2", "int" },` |
|        - |  752 | `	{ "strcspn", "string $string, string $characters, int $offset = 0, ?int $length = NULL", "int" },` |
|        - |  753 | `	{ "strip_tags", "string $string, array\|string\|null $allowed_tags = NULL", "string" },` |
|        - |  754 | `	{ "stripos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|        - |  755 | `	{ "stripslashes", "string $string", "string" },` |
|        - |  756 | `	{ "stristr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|        - |  757 | `	{ "strlen", "string $string", "int" },` |
|        - |  758 | `	{ "strncasecmp", "string $string1, string $string2, int $length", "int" },` |
|        - |  759 | `	{ "strncmp", "string $string1, string $string2, int $length", "int" },` |
|        - |  760 | `	{ "strpbrk", "string $string, string $characters", "string\|false" },` |
|        - |  761 | `	{ "strpos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|        - |  762 | `	{ "strrchr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|        - |  763 | `	{ "strrev", "string $string", "string" },` |
|        - |  764 | `	{ "strripos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|        - |  765 | `	{ "strrpos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|        - |  766 | `	{ "strspn", "string $string, string $characters, int $offset = 0, ?int $length = NULL", "int" },` |
|        - |  767 | `	{ "strstr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|        - |  768 | `	{ "strtok", "string $string, ?string $token = NULL", "string\|false" },` |
|        - |  769 | `	{ "strtolower", "string $string", "string" },` |
|        - |  770 | `	{ "strtoupper", "string $string", "string" },` |
|        - |  771 | `	{ "strtr", "string $string, array\|string $from, ?string $to = NULL", "string" },` |
|        - |  772 | `	{ "strval", "mixed $value", "string" },` |
|        - |  773 | `	{ "substr", "string $string, int $offset, ?int $length = NULL", "string" },` |
|        - |  774 | `	{ "substr_compare", "string $haystack, string $needle, int $offset, ?int $length = NULL, bool $case_insensitive = false", "int" },` |
|        - |  775 | `	{ "substr_count", "string $haystack, string $needle, int $offset = 0, ?int $length = NULL", "int" },` |
|        - |  776 | `	{ "substr_replace", "array\|string $string, array\|string $replace, array\|int $offset, array\|int\|null $length = NULL", "array\|string" },` |
|        - |  777 | `	{ "symlink", "string $target, string $link", "bool" },` |
|        - |  778 | `	{ "sys_get_temp_dir", "", "string" },` |
|        - |  779 | `	{ "tan", "float $num", "float" },` |
|        - |  780 | `	{ "tanh", "float $num", "float" },` |
|        - |  781 | `	{ "time", "", "int" },` |
|        - |  782 | `	{ "touch", "string $filename, ?int $mtime = NULL, ?int $atime = NULL", "bool" },` |
|        - |  783 | `	{ "trigger_error", "string $message, int $error_level = 1024", "true" },` |
|        - |  784 | `	{ "trim", "string $string, string $characters = ?", "string" },` |
|        - |  785 | `	{ "uasort", "array &$array, callable $callback", "true" },` |
|        - |  786 | `	{ "ucfirst", "string $string", "string" },` |
|        - |  787 | `	{ "ucwords", "string $string, string $separators = ?", "string" },` |
|        - |  788 | `	{ "uksort", "array &$array, callable $callback", "true" },` |
|        - |  789 | `	{ "umask", "?int $mask = NULL", "int" },` |
|        - |  790 | `	{ "uniqid", "string $prefix = '', bool $more_entropy = false", "string" },` |
|        - |  791 | `	{ "unlink", "string $filename, $context = NULL", "bool" },` |
|        - |  792 | `	{ "unserialize", "string $data, array $options = ?", "mixed" },` |
|        - |  793 | `	{ "urldecode", "string $string", "string" },` |
|        - |  794 | `	{ "urlencode", "string $string", "string" },` |
|        - |  795 | `	{ "user_error", "string $message, int $error_level = 1024", "true" },` |
|        - |  796 | `	{ "usleep", "int $microseconds", "void" },` |
|        - |  797 | `	{ "usort", "array &$array, callable $callback", "true" },` |
|        - |  798 | `	{ "utf8_decode", "string $string", "string" },` |
|        - |  799 | `	{ "utf8_encode", "string $string", "string" },` |
|        - |  800 | `	{ "var_dump", "mixed $value, mixed ...$values = ?", "void" },` |
|        - |  801 | `	{ "var_export", "mixed $value, bool $return = false", "?string" },` |
|        - |  802 | `	{ "vfprintf", "$stream, string $format, array $values", "int" },` |
|        - |  803 | `	{ "vprintf", "string $format, array $values", "int" },` |
|        - |  804 | `	{ "vsprintf", "string $format, array $values", "string" },` |
|        - |  805 | `	{ "wordwrap", "string $string, int $width = 75, string $break = ?, bool $cut_long_words = false", "string" },` |
|        - |  806 | `	{ "zip_close", "$zip", "void" },` |
|        - |  807 | `	{ "zip_entry_close", "$zip_entry", "bool" },` |
|        - |  808 | `	{ "zip_entry_compressedsize", "$zip_entry", "int\|false" },` |
|        - |  809 | `	{ "zip_entry_compressionmethod", "$zip_entry", "string\|false" },` |
|        - |  810 | `	{ "zip_entry_filesize", "$zip_entry", "int\|false" },` |
|        - |  811 | `	{ "zip_entry_name", "$zip_entry", "string\|false" },` |
|        - |  812 | `	{ "zip_entry_open", "$zip_dp, $zip_entry, string $mode = 'rb'", "bool" },` |
|        - |  813 | `	{ "zip_entry_read", "$zip_entry, int $len = 1024", "string\|false" },` |
|        - |  814 | `	{ "zip_open", "string $filename", "" },` |
|        - |  815 | `	{ "zip_read", "$zip", "" },` |
|        - |  816 | `};` |
|        - |  817 | `/*` |
|        - |  818 | ` * Stamp the signature strings onto the registered host functions.` |
|        - |  819 | ` * Runs once at PH7_VmMakeReady, after the builtins are registered.` |
|        - |  820 | ` */` |
|        - |  821 | `/*` |
|        - |  822 | ` * Derive the minimum arity from a builtin's declared signature: a parameter is` |
|        - |  823 | ` * optional when it carries a default ("int $length = 76") or is variadic` |
|        - |  824 | ` * ("...$rest"), so the minimum is the count of the parameters that are neither,` |
|        - |  825 | ` * and the ZPP wording is "at least" as soon as one optional/variadic exists.` |
|        - |  826 | ` *` |
|        - |  827 | ` * This makes aBuiltinSig[] the single source of truth for arity — the curated` |
|        - |  828 | ` * aBuiltinArity[] table above stays as an OVERRIDE for the handful of builtins` |
|        - |  829 | ` * php reports differently from their own signature (e.g. strtr(), whose 3-arg` |
|        - |  830 | ` * form still reports "expects exactly 2", and the array_udiff() family, whose` |
|        - |  831 | ` * variadic tail hides a second required argument). Verified against php 8.5.7` |
|        - |  832 | ` * for all 462 signed builtins: 458 derive exactly, 4 are overridden.` |
|        - |  833 | ` */` |
|  1508372 |  834 | `static void VmDeriveArityFromSig(const char *zSig,sxi16 *pnMin,sxu8 *pbAtLeast,sxi16 *pnMax,sxu8 *pbHasMax)` |
|        5 |  835 | `{` |
|  1508377 |  836 | `	const char *zCur = zSig;` |
|  1508377 |  837 | `	int nMin = 0, bAtLeast = 0, bSeen = 0, bOptional = 0;` |
|  1508377 |  838 | `	int nTotal = 0, bVariadic = 0;` |
| 23180228 |  839 | `	for(;;){` |
| 47706497 |  840 | `		if( zCur[0] == '\0' \|\| zCur[0] == ',' ){` |
|  2854413 |  841 | `			if( bSeen ){` |
|  2685313 |  842 | `				nTotal++;` |
|  2685313 |  843 | `				if( bOptional ){` |
|  1055189 |  844 | `					bAtLeast = 1;` |
|   527597 |  845 | `				}else{` |
|  1630129 |  846 | `					nMin++;` |
|        - |  847 | `				}` |
|  1342654 |  848 | `			}` |
|  2854413 |  849 | `			if( zCur[0] == '\0' ){` |
|  1508377 |  850 | `				break;` |
|        - |  851 | `			}` |
|  1346041 |  852 | `			bSeen = bOptional = 0;` |
|  1346041 |  853 | `			zCur++;` |
|  1346041 |  854 | `			continue;` |
|        - |  855 | `		}` |
| 44852089 |  856 | `		if( zCur[0] != ' ' ){` |
| 38943735 |  857 | `			bSeen = 1;` |
| 19471865 |  858 | `		}` |
| 44852089 |  859 | `		if( zCur[0] == '=' \|\| (zCur[0] == '.' && zCur[1] == '.' && zCur[2] == '.') ){` |
|  1126211 |  860 | `			bOptional = 1;` |
|   563103 |  861 | `		}` |
| 44852089 |  862 | `		if( zCur[0] == '.' && zCur[1] == '.' && zCur[2] == '.' ){` |
|    71027 |  863 | `			bVariadic = 1;` |
|    35511 |  864 | `		}` |
| 44852089 |  865 | `		zCur++;` |
|        5 |  866 | `	}` |
|  1508377 |  867 | `	*pnMin = (sxi16)nMin;` |
|  1508377 |  868 | `	*pbAtLeast = (sxu8)bAtLeast;` |
|        - |  869 | `	/* php enforces a MAXIMUM too ("expects at most 1 argument, 2 given"); a` |
|        - |  870 | `	 * variadic tail means there is none. The parameter COUNT is the maximum,` |
|        - |  871 | `	 * whether or not the parameters carry defaults. */` |
|  1508377 |  872 | `	*pnMax = (sxi16)nTotal;` |
|  1508377 |  873 | `	*pbHasMax = (sxu8)(bVariadic ? 0 : 1);` |
|  1508377 |  874 | `}` |
|        - |  875 | `/*` |
|        - |  876 | ` * Does the declared type list (e.g. "array\|string", "?int", "callable") contain` |
|        - |  877 | ` * the given token? Compares against each '\|'-separated alternative, ignoring a` |
|        - |  878 | ` * leading nullable '?'.` |
|        - |  879 | ` */` |
|  1459364 |  880 | `static int VmSigTypeHas(const char *zType,int nType,const char *zTok)` |
|        5 |  881 | `{` |
|  1459369 |  882 | `	int nTok = (int)SyStrlen(zTok);` |
|  1459369 |  883 | `	int i = 0;` |
|  1459369 |  884 | `	if( zType[0] == '?' ){` |
|   275391 |  885 | `		zType++;` |
|   275391 |  886 | `		nType--;` |
|   137693 |  887 | `	}` |
|  2904215 |  888 | `	while( i < nType ){` |
|  1593931 |  889 | `		int j = i;` |
|  9264222 |  890 | `		while( j < nType && zType[j] != '\|' ){` |
|  7670296 |  891 | `			j++;` |
|        5 |  892 | `		}` |
|  1593931 |  893 | `		if( j - i == nTok && SyMemcmp(&zType[i],zTok,(sxu32)nTok) == 0 ){` |
|   149085 |  894 | `			return 1;` |
|        - |  895 | `		}` |
|  1444851 |  896 | `		i = j + 1;` |
|        5 |  897 | `	}` |
|  1310289 |  898 | `	return 0;` |
|   730166 |  899 | `}` |
|        - |  900 | `/*` |
|        - |  901 | ` * Does the declared type list name a CLASS (anything that is not one of php's` |
|        - |  902 | ` * builtin type keywords)? A class-typed parameter accepts an object, so it must` |
|        - |  903 | ` * not be rejected by the array/object/resource screen below.` |
|        - |  904 | ` */` |
|      216 |  905 | `static int VmSigTypeHasClass(const char *zType,int nType)` |
|        5 |  906 | `{` |
|        - |  907 | `	static const char *azBuiltin[] = {` |
|        - |  908 | `		"int","float","string","bool","array","object","callable","iterable",` |
|        - |  909 | `		"mixed","null","void","resource","false","true","never","self","static"` |
|        - |  910 | `	};` |
|      221 |  911 | `	int i = 0;` |
|      221 |  912 | `	if( zType[0] == '?' ){` |
|        7 |  913 | `		zType++;` |
|        7 |  914 | `		nType--;` |
|        3 |  915 | `	}` |
|      341 |  916 | `	while( i < nType ){` |
|      235 |  917 | `		int j = i, k, bKnown = 0;` |
|     1979 |  918 | `		while( j < nType && zType[j] != '\|' ){` |
|     1749 |  919 | `			j++;` |
|        5 |  920 | `		}` |
|     2357 |  921 | `		for( k = 0 ; k < (int)SX_ARRAYSIZE(azBuiltin) ; ++k ){` |
|     2247 |  922 | `			int nB = (int)SyStrlen(azBuiltin[k]);` |
|     2247 |  923 | `			if( j - i == nB && SyMemcmp(&zType[i],azBuiltin[k],(sxu32)nB) == 0 ){` |
|      125 |  924 | `				bKnown = 1;` |
|      125 |  925 | `				break;` |
|        - |  926 | `			}` |
|     1066 |  927 | `		}` |
|      235 |  928 | `		if( !bKnown && j > i ){` |
|      115 |  929 | `			return 1;` |
|        - |  930 | `		}` |
|      125 |  931 | `		i = j + 1;` |
|        5 |  932 | `	}` |
|      111 |  933 | `	return 0;` |
|      113 |  934 | `}` |
|        - |  935 | `/*` |
|        - |  936 | ` * php's ", X given" tail: like ph7_type_name() but an object reports its CLASS,` |
|        - |  937 | ` * which is what php prints in a TypeError.` |
|        - |  938 | ` */` |
|       56 |  939 | `static const char * VmArgTypeName(ph7_value *pVal)` |
|        4 |  940 | `{` |
|       60 |  941 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       60 |  942 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       60 |  943 | `		if( pInst && pInst->pClass ){` |
|       60 |  944 | `			return pInst->pClass->sName.zString;` |
|        - |  945 | `		}` |
|      ! 0 |  946 | `	}` |
|      ! 0 |  947 | `	return ph7_type_name(pVal);` |
|       32 |  948 | `}` |
|        - |  949 | `/*` |
|        - |  950 | ` * PHP-8 ZPP type enforcement for host functions, driven by the aBuiltinSig[]` |
|        - |  951 | ` * declaration (band A #7). Screens only the arguments that php can NEVER coerce` |
|        - |  952 | ` * into a declared scalar parameter — arrays, resources, and objects without a` |
|        - |  953 | ` * __toString() — and throws the catchable TypeError php throws, before the C` |
|        - |  954 | ` * routine runs. Without this an array argument reached the builtin and was` |
|        - |  955 | ` * stringified to the literal "Array" (strrev(['a']) returned "yarrA").` |
|        - |  956 | ` *` |
|        - |  957 | ` * Deliberately narrow: scalar-to-scalar coercion (and its deprecations) stays` |
|        - |  958 | ` * with the per-builtin ZPP helpers. Those emit E_DEPRECATED, which does NOT` |
|        - |  959 | ` * abort the call, so a central copy would double-fire — the trap that sank the` |
|        - |  960 | ` * first central-ZPP attempt. A TypeError aborts, so there is nothing to double.` |
|        - |  961 | ` */` |
|   938863 |  962 | `PH7_PRIVATE sxi32 VmEnforceBuiltinArgTypes(` |
|        - |  963 | `	ph7_context *pCtx,    /* Call context (for the throw) */` |
|        - |  964 | `	ph7_user_func *pFunc, /* Callee */` |
|        - |  965 | `	int nGiven,           /* Argument count */` |
|        - |  966 | `	ph7_value **apArg     /* Arguments */` |
|        - |  967 | `	)` |
|        5 |  968 | `{` |
|        - |  969 | `	/*` |
|        - |  970 | `	 * Builtins whose own argument check is php-exact and VALUE-based rather than` |
|        - |  971 | `	 * type-based must not be pre-empted here, or their message is lost. Same rule` |
|        - |  972 | `	 * the aBuiltinArity[] table follows: a builtin that already says what php says` |
|        - |  973 | `	 * stays off the shared screen. get_class_vars() takes any stringifiable value` |
|        - |  974 | `	 * and reports "must be a valid class name, Array given".` |
|        - |  975 | `	 */` |
|        - |  976 | `	static const char *azSelfChecked[] = { "get_class_vars" };` |
|   938868 |  977 | `	const char *zSig = pFunc->zSig;` |
|        - |  978 | `	const char *zCur, *zEnd;` |
|   938868 |  979 | `	int iArg = 0;` |
|   938868 |  980 | `	if( zSig == 0 ){` |
|   175181 |  981 | `		return SXRET_OK;` |
|        - |  982 | `	}` |
|  1527375 |  983 | `	for( iArg = 0 ; iArg < (int)SX_ARRAYSIZE(azSelfChecked) ; ++iArg ){` |
|  1145904 |  984 | `		if( SyStrncmp(pFunc->sName.zString,azSelfChecked[iArg],` |
|  1145904 |  985 | `			(sxu32)SyStrlen(azSelfChecked[iArg])) == 0` |
|   382224 |  986 | `		 && pFunc->sName.nByte == SyStrlen(azSelfChecked[iArg]) ){` |
|        5 |  987 | `			return SXRET_OK;` |
|        - |  988 | `		}` |
|   382220 |  989 | `	}` |
|   763688 |  990 | `	iArg = 0;` |
|   763688 |  991 | `	zCur = zSig;` |
|   763688 |  992 | `	zEnd = &zSig[SyStrlen(zSig)];` |
|  2179222 |  993 | `	while( zCur < zEnd && iArg < nGiven ){` |
|        - |  994 | `		const char *zType, *zName, *zStop;` |
|        - |  995 | `		int nType, nName;` |
|        - |  996 | `		ph7_value *pArg;` |
|        - |  997 | `		/* Parameter = "<type> $<name>[ = <default>]"; the type is whatever` |
|        - |  998 | `		 * precedes the '$', and an empty type means "untyped" (no screen). */` |
|  2091938 |  999 | `		while( zCur < zEnd && zCur[0] == ' ' ){` |
|   674232 | 1000 | `			zCur++;` |
|        5 | 1001 | `		}` |
|  1417711 | 1002 | `		zStop = zCur;` |
| 22513968 | 1003 | `		while( zStop < zEnd && zStop[0] != ',' ){` |
| 21096262 | 1004 | `			zStop++;` |
|        5 | 1005 | `		}` |
|  1417711 | 1006 | `		zName = zCur;` |
| 10434070 | 1007 | `		while( zName < zStop && zName[0] != '$' ){` |
|  9016364 | 1008 | `			zName++;` |
|        5 | 1009 | `		}` |
|  1417711 | 1010 | `		if( zName >= zStop ){` |
|        7 | 1011 | `			break; /* malformed / no parameter name — stop screening */` |
|        - | 1012 | `		}` |
|  1417705 | 1013 | `		if( zName >= zCur + 3 && SyMemcmp(zName - 3,"...",3) == 0 ){` |
|     1957 | 1014 | `			break; /* variadic tail: stop (its type applies to the rest) */` |
|        - | 1015 | `		}` |
|  1415753 | 1016 | `		zType = zCur;` |
|  1415753 | 1017 | `		nType = (int)(zName - zCur);` |
|        - | 1018 | `		/* Trim the trailing spaces and the by-ref marker of "array &$array" */` |
|  3508914 | 1019 | `		while( nType > 0 && (zType[nType-1] == ' ' \|\| zType[nType-1] == '&') ){` |
|  1384813 | 1020 | `			nType--;` |
|        5 | 1021 | `		}` |
|  1415753 | 1022 | `		zName++; /* skip '$' */` |
|  1415753 | 1023 | `		nName = 0;` |
| 10246761 | 1024 | `		while( &zName[nName] < zStop && zName[nName] != ' ' && zName[nName] != '=' ){` |
|  8831013 | 1025 | `			nName++;` |
|        5 | 1026 | `		}` |
|  1415753 | 1027 | `		pArg = apArg[iArg];` |
|  1415753 | 1028 | `		if( nType > 0 && !VmSigTypeHas(zType,nType,"mixed") ){` |
|  1308403 | 1029 | `			const char *zGiven = 0;` |
|  1308403 | 1030 | `			if( (pArg->iFlags & MEMOBJ_HASHMAP) != 0 ){` |
|    73946 | 1031 | `				if( !VmSigTypeHas(zType,nType,"array")` |
|    37048 | 1032 | `				 && !VmSigTypeHas(zType,nType,"iterable")` |
|      155 | 1033 | `				 && !VmSigTypeHas(zType,nType,"callable") ){` |
|      119 | 1034 | `					zGiven = "array";` |
|       62 | 1035 | `				}` |
|  1271430 | 1036 | `			}else if( (pArg->iFlags & MEMOBJ_OBJ) != 0 ){` |
|     1586 | 1037 | `				if( !VmSigTypeHas(zType,nType,"object")` |
|     1079 | 1038 | `				 && !VmSigTypeHas(zType,nType,"iterable")` |
|      572 | 1039 | `				 && !VmSigTypeHas(zType,nType,"callable")` |
|      398 | 1040 | `				 && !VmSigTypeHasClass(zType,nType) ){` |
|        - | 1041 | `					/* An object with __toString() still satisfies a string` |
|        - | 1042 | `					 * parameter in weak mode — php coerces it. */` |
|      108 | 1043 | `					ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|      149 | 1044 | `					int bStringable = VmSigTypeHas(zType,nType,"string")` |
|      104 | 1045 | `						&& pInst && PH7_ClassExtractMethod(pInst->pClass,"__toString",` |
|       41 | 1046 | `							sizeof("__toString")-1) != 0;` |
|      108 | 1047 | `					if( !bStringable ){` |
|       60 | 1048 | `						zGiven = VmArgTypeName(pArg);` |
|       28 | 1049 | `					}` |
|       57 | 1050 | `				}` |
|  1233664 | 1051 | `			}else if( (pArg->iFlags & MEMOBJ_NULL) != 0 ){` |
|        - | 1052 | `				/* php only DEPRECATES null for a non-nullable parameter; PHL rejects` |
|        - | 1053 | `				 * it (scope policy). A leading '?' or an explicit "null" arm in a` |
|        - | 1054 | `				 * union declares the parameter nullable. A "callable" parameter is` |
|        - | 1055 | `				 * left to the builtin's own callback check, which words the failure` |
|        - | 1056 | `				 * php's way ("must be a valid callback, no array or string given") —` |
|        - | 1057 | `				 * the same reason get_class_vars() sits on azSelfChecked[]. */` |
|     4898 | 1058 | `				if( zType[0] != '?'` |
|     2475 | 1059 | `				 && !VmSigTypeHas(zType,nType,"null")` |
|       55 | 1060 | `				 && !VmSigTypeHas(zType,nType,"callable") ){` |
|       46 | 1061 | `					zGiven = "null";` |
|       26 | 1062 | `				}` |
|  1230422 | 1063 | `			}else if( (pArg->iFlags & MEMOBJ_RES) != 0 ){` |
|        - | 1064 | `				/* A class-typed parameter also accepts a resource: several handles php 8` |
|        - | 1065 | `				 * models as objects are still resources here (xml_*'s XMLParser is the` |
|        - | 1066 | `				 * one the signatures already declare php-8-style, for reflection). The` |
|        - | 1067 | `				 * screen would otherwise reject the engine's own parser handle. Recorded` |
|        - | 1068 | `				 * as a divergence in NEWPLAN §7 — it goes away when those handles become` |
|        - | 1069 | `				 * real objects. */` |
|        2 | 1070 | `				if( !VmSigTypeHas(zType,nType,"resource")` |
|        3 | 1071 | `				 && !VmSigTypeHasClass(zType,nType) ){` |
|        3 | 1072 | `					zGiven = "resource";` |
|        1 | 1073 | `				}` |
|        1 | 1074 | `			}` |
|  1308403 | 1075 | `			if( zGiven ){` |
|      326 | 1076 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|        - | 1077 | `					"%z(): Argument #%d ($%.*s) must be of type %.*s, %s given",` |
|      107 | 1078 | `					&pFunc->sName,iArg + 1,nName,zName,nType,zType,zGiven);` |
|        - | 1079 | `			}` |
|   654514 | 1080 | `		}` |
|  1415539 | 1081 | `		zCur = (zStop < zEnd) ? zStop + 1 : zEnd;` |
|  1415539 | 1082 | `		iArg++;` |
|        5 | 1083 | `	}` |
|   763474 | 1084 | `	return SXRET_OK;` |
|   469810 | 1085 | `}` |
|        - | 1086 | `/*` |
|        - | 1087 | ` * Builtins whose accepted arity is NOT a contiguous range, so the signature` |
|        - | 1088 | ` * cannot express it and the central too-many-arguments check must stay out of` |
|        - | 1089 | ` * the way: rand()/mt_rand() take 0 OR 2 arguments (never 1), and php words the` |
|        - | 1090 | ` * violation "expects exactly 2 arguments, 3 given" from their own check rather` |
|        - | 1091 | ` * than the ZPP "at most". They validate themselves; leaving bHasMaxArg at 0` |
|        - | 1092 | ` * keeps their message php-faithful.` |
|        - | 1093 | ` */` |
|  1508372 | 1094 | `static int VmBuiltinSelfValidatesArity(const char *zName)` |
|        5 | 1095 | `{` |
|        - | 1096 | `	static const char *const azSelf[] = { "rand", "mt_rand" };` |
|        - | 1097 | `	sxu32 i;` |
|  4514975 | 1098 | `	for( i = 0 ; i < SX_ARRAYSIZE(azSelf) ; i++ ){` |
|  3013367 | 1099 | `		sxu32 nSelf = SyStrlen(azSelf[i]);` |
|  3013367 | 1100 | `		if( SyStrlen(zName) == nSelf && SyStrncmp(zName,azSelf[i],nSelf) == 0 ){` |
|     6769 | 1101 | `			return 1;` |
|        - | 1102 | `		}` |
|  1503304 | 1103 | `	}` |
|  1501613 | 1104 | `	return 0;` |
|   754191 | 1105 | `}` |
|     3382 | 1106 | `PH7_PRIVATE void VmSetBuiltinSignatures(ph7_vm *pVm)` |
|        5 | 1107 | `{` |
|        - | 1108 | `	sxu32 n;` |
|  1552343 | 1109 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinSig) ; n++ ){` |
|  2323439 | 1110 | `		SyHashEntry *pEntry = SyHashGet(&pVm->hHostFunction,` |
|  1548956 | 1111 | `			(const void *)aBuiltinSig[n].zName,(sxu32)SyStrlen(aBuiltinSig[n].zName));` |
|  1548961 | 1112 | `		if( pEntry ){` |
|  1508377 | 1113 | `			ph7_user_func *pFunc = (ph7_user_func *)pEntry->pUserData;` |
|  1508377 | 1114 | `			sxi16 nMin = 0, nMax = 0;` |
|  1508377 | 1115 | `			sxu8 bAtLeast = 0, bHasMax = 0;` |
|  1508377 | 1116 | `			pFunc->zSig = aBuiltinSig[n].zSig;` |
|  1508377 | 1117 | `			pFunc->zRet = aBuiltinSig[n].zRet[0] ? aBuiltinSig[n].zRet : 0;` |
|  1508377 | 1118 | `			VmDeriveArityFromSig(pFunc->zSig,&nMin,&bAtLeast,&nMax,&bHasMax);` |
|        - | 1119 | `			/* The MAXIMUM always comes from the signature: the curated override` |
|        - | 1120 | `			 * table speaks only to the minimum (and its wording). */` |
|  1508377 | 1121 | `			pFunc->nMaxArg = nMax;` |
|  1508377 | 1122 | `			pFunc->bHasMaxArg = (sxu8)(VmBuiltinSelfValidatesArity(aBuiltinSig[n].zName) ? 0 : bHasMax);` |
|  1508377 | 1123 | `			if( pFunc->nMinArg < 1 ){` |
|        - | 1124 | `				/* VmSetBuiltinArity() ran first: a non-zero minimum here means the` |
|        - | 1125 | `				 * curated override already spoke for this builtin, so leave it. */` |
|   632439 | 1126 | `				pFunc->nMinArg = nMin;` |
|   632439 | 1127 | `				pFunc->bAtLeast = bAtLeast;` |
|   316217 | 1128 | `			}` |
|   754186 | 1129 | `		}` |
|   774483 | 1130 | `	}` |
|     3387 | 1131 | `}` |
|        - | 1132 | `/*` |
|        - | 1133 | ` * Signature lookup by function name, for the reflection layer: embedded-PHP` |
|        - | 1134 | ` * builtins (max/min/Exception methods...) are ph7_vm_func instances, not` |
|        - | 1135 | ` * host functions, so they miss the VmSetBuiltinSignatures stamping and pull` |
|        - | 1136 | ` * their row on demand here. Linear scan — reflection-path only.` |
|        - | 1137 | ` */` |
|        4 | 1138 | `PH7_PRIVATE const char * PH7_VmBuiltinSigLookup(const char *zName,sxu32 nLen,const char **pzRet)` |
|        1 | 1139 | `{` |
|        - | 1140 | `	sxu32 n;` |
|        5 | 1141 | `	if( pzRet ){` |
|        5 | 1142 | `		*pzRet = 0;` |
|        2 | 1143 | `	}` |
|     1049 | 1144 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinSig) ; n++ ){` |
|     1048 | 1145 | `		if( SyStrlen(aBuiltinSig[n].zName) == nLen` |
|      543 | 1146 | `		 && SyMemcmp(aBuiltinSig[n].zName,zName,nLen) == 0 ){` |
|        5 | 1147 | `			if( pzRet && aBuiltinSig[n].zRet[0] ){` |
|        5 | 1148 | `				*pzRet = aBuiltinSig[n].zRet;` |
|        2 | 1149 | `			}` |
|        5 | 1150 | `			return aBuiltinSig[n].zSig;` |
|        - | 1151 | `		}` |
|      523 | 1152 | `	}` |
|      ! 0 | 1153 | `	return 0;` |
|        3 | 1154 | `}` |
|        - | 1155 | `/*` |
|        - | 1156 | ` * Write a value back to the caller's variable through a builtin argument's` |
|        - | 1157 | ` * stack-slot nIdx — the shared write-back for builtin by-reference` |
|        - | 1158 | ` * out-parameters (preg_match $matches, preg_replace &$count, similar_text` |
|        - | 1159 | ` * &$percent, ...).` |
|        - | 1160 | ` *` |
|        - | 1161 | ` * For a positional out-param argument the call compiler auto-vivifies known` |
|        - | 1162 | ` * by-reference out-params (see GenStateByRefBuiltinMask in compile.c), so a` |
|        - | 1163 | ` * bare undefined variable, an array subscript, and a declared/untyped` |
|        - | 1164 | ` * property all arrive with a real nIdx and are written back here, matching` |
|        - | 1165 | ` * PHP's reference semantics.` |
|        - | 1166 | ` *` |
|        - | 1167 | ` * nIdx stays SXU32_HIGH and the write-back to the caller is skipped (the` |
|        - | 1168 | ` * value still lands in the local stack slot) when the argument cannot expose` |
|        - | 1169 | ` * a stable memobj slot: a literal, a function-call result, a subscript of a` |
|        - | 1170 | ` * non-lvalue parent (foo()['k']), or any variable in a call that also uses` |
|        - | 1171 | ` * named or spread arguments (compile-time positions no longer map to the` |
|        - | 1172 | ` * runtime arg slots, so the compiler conservatively does not vivify). An` |
|        - | 1173 | ` * uninitialized typed property is also not wired (it throws before the` |
|        - | 1174 | ` * write) -- see the recorded deferrals.` |
|        - | 1175 | ` */` |
|      208 | 1176 | `PH7_PRIVATE void PH7_VmStoreArgByRef(ph7_vm *pVm,ph7_value *pArg,ph7_value *pNewVal)` |
|        5 | 1177 | `{` |
|      213 | 1178 | `	if( pArg->nIdx != SXU32_HIGH ){` |
|      211 | 1179 | `		ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pArg->nIdx);` |
|      211 | 1180 | `		if( pObj ){` |
|      211 | 1181 | `			PH7_MemObjStore(pNewVal,pObj);` |
|      103 | 1182 | `		}` |
|      103 | 1183 | `	}` |
|      213 | 1184 | `	PH7_MemObjStore(pNewVal,pArg);` |
|      213 | 1185 | `}` |
|        - | 1186 | `/*` |
|        - | 1187 | ` * Raise an E_WARNING whose text is used VERBATIM.` |
|        - | 1188 | ` * ph7_context_throw_error_format() prepends "func(): " to whatever it is given, but php's` |
|        - | 1189 | ` * IO warnings put the offending path inside those parens -- "file_get_contents(/nope):` |
|        - | 1190 | ` * Failed to open stream: ..." -- so a caller that needs php's exact shape must build the` |
|        - | 1191 | ` * whole line itself and come through here.` |
|        - | 1192 | ` */` |
|        - | 1193 | `/*` |
|        - | 1194 | ` * Validate a callback argument and throw php's TypeError when it cannot be called.` |
|        - | 1195 | ` *` |
|        - | 1196 | ` * The shared dispatcher (PH7_VmCallUserFunctionWithMap) answers SXRET_OK with a NULL` |
|        - | 1197 | ` * result for an unresolvable callable, so every caller that did not check first failed` |
|        - | 1198 | ` * SILENTLY: call_user_func() returned NULL and usort() left the array sorted by nothing` |
|        - | 1199 | ` * at all. php rejects the ARGUMENT up front, naming exactly why.` |
|        - | 1200 | ` */` |
|      162 | 1201 | `PH7_PRIVATE sxi32 PH7_CheckCallbackArg(` |
|        - | 1202 | `	ph7_context *pCtx,   /* Calling context (names the function in the message) */` |
|        - | 1203 | `	ph7_value *pCb,      /* The callback argument */` |
|        - | 1204 | `	int iArg,            /* Its 1-based position */` |
|        - | 1205 | `	const char *zParam,  /* Its php parameter name, e.g. "callback" */` |
|        - | 1206 | `	int bNullable        /* TRUE when php's text says "or null" */` |
|        - | 1207 | `	)` |
|        3 | 1208 | `{` |
|      165 | 1209 | `	const char *zOrNull = bNullable ? " or null" : "";` |
|      165 | 1210 | `	if( ph7_value_is_callable(pCb) ){` |
|      147 | 1211 | `		return PH7_OK;` |
|        - | 1212 | `	}` |
|       19 | 1213 | `	if( ph7_value_is_string(pCb) ){` |
|        - | 1214 | `		int nLen;` |
|       11 | 1215 | `		const char *zName = ph7_value_to_string(pCb,&nLen);` |
|       16 | 1216 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|        - | 1217 | `			"%s(): Argument #%d ($%s) must be a valid callback%s, function \"%.*s\" not found or invalid function name",` |
|        5 | 1218 | `			ph7_function_name(pCtx),iArg,zParam,zOrNull,nLen,zName);` |
|        - | 1219 | `	}` |
|        9 | 1220 | `	if( ph7_value_is_array(pCb) ){` |
|        7 | 1221 | `		ph7_hashmap *pMap = (ph7_hashmap *)pCb->x.pOther;` |
|        7 | 1222 | `		if( pMap == 0 \|\| pMap->nEntry != 2 ){` |
|        4 | 1223 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|        - | 1224 | `				"%s(): Argument #%d ($%s) must be a valid callback%s, array callback must have exactly two members",` |
|        1 | 1225 | `				ph7_function_name(pCtx),iArg,zParam,zOrNull);` |
|      ! 0 | 1226 | `		}else{` |
|        5 | 1227 | `			ph7_vm *pVm = pCtx->pVm;` |
|        5 | 1228 | `			ph7_value *pCls = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|        5 | 1229 | `			ph7_value *pMeth = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|        5 | 1230 | `			ph7_class *pClass = pCls ? PH7_VmExtractClassFromValue(&(*pVm),pCls) : 0;` |
|        5 | 1231 | `			if( pClass == 0 ){` |
|        5 | 1232 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|        - | 1233 | `					"%s(): Argument #%d ($%s) must be a valid callback%s, class \"%.*s\" not found",` |
|        1 | 1234 | `					ph7_function_name(pCtx),iArg,zParam,zOrNull,` |
|        2 | 1235 | `					pCls ? (int)SyBlobLength(&pCls->sBlob) : 0,` |
|        1 | 1236 | `					pCls ? (const char *)SyBlobData(&pCls->sBlob) : "");` |
|        - | 1237 | `			}` |
|        5 | 1238 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|        - | 1239 | `				"%s(): Argument #%d ($%s) must be a valid callback%s, class %z does not have a method \"%.*s\"",` |
|        1 | 1240 | `				ph7_function_name(pCtx),iArg,zParam,zOrNull,&pClass->sName,` |
|        2 | 1241 | `				pMeth ? (int)SyBlobLength(&pMeth->sBlob) : 0,` |
|        1 | 1242 | `				pMeth ? (const char *)SyBlobData(&pMeth->sBlob) : "");` |
|        - | 1243 | `		}` |
|        - | 1244 | `	}` |
|        4 | 1245 | `	return PH7_VmThrowException(pCtx,"TypeError",` |
|        - | 1246 | `		"%s(): Argument #%d ($%s) must be a valid callback%s, no array or string given",` |
|        1 | 1247 | `		ph7_function_name(pCtx),iArg,zParam,zOrNull);` |
|       84 | 1248 | `}` |
|    19996 | 1249 | `PH7_PRIVATE void PH7_VmThrowWarningFmt(ph7_vm *pVm,const char *zFmt,...)` |
|        5 | 1250 | `{` |
|        - | 1251 | `	va_list ap;` |
|    20001 | 1252 | `	va_start(ap,zFmt);` |
|    20001 | 1253 | `	PH7_VmThrowErrorAp(pVm,0,PH7_CTX_WARNING,zFmt,ap);` |
|    20001 | 1254 | `	va_end(ap);` |
|    20001 | 1255 | `}` |
|        - | 1256 | `/*` |
|        - | 1257 | ` * Emit a formatted E_USER_DEPRECATED diagnostic with no function-name prefix:` |
|        - | 1258 | ` * php reports #[\Deprecated] as USER-deprecated (16384, it is userland-authored),` |
|        - | 1259 | ` * not the engine's E_DEPRECATED (8192) — handler-visible errno matters.` |
|        - | 1260 | ` */` |
|       34 | 1261 | `static void VmThrowUserDeprecatedFmt(ph7_vm *pVm,const char *zFmt,...)` |
|        1 | 1262 | `{` |
|        - | 1263 | `	va_list ap;` |
|       35 | 1264 | `	va_start(ap,zFmt);` |
|       35 | 1265 | `	PH7_VmThrowErrorAp(pVm,0,E_USER_DEPRECATED,zFmt,ap);` |
|       35 | 1266 | `	va_end(ap);` |
|       35 | 1267 | `}` |
|        - | 1268 | `/*` |
|        - | 1269 | ` * php 8.4 #[\Deprecated]: emit the E_USER_DEPRECATED notice for a call to a user` |
|        - | 1270 | ` * function/method carrying the attribute. Message shapes (php-exact):` |
|        - | 1271 | ` *   Function f() is deprecated` |
|        - | 1272 | ` *   Method C::m() is deprecated since 2.0, use g() instead` |
|        - | 1273 | ` * ("since" from the attribute's since: argument; the trailing ", msg" from` |
|        - | 1274 | ` * message:/positional #1.) The attribute arguments are compiled constant` |
|        - | 1275 | ` * expressions — evaluated via VmLocalExec, the reflection thunks' pattern.` |
|        - | 1276 | ` * Called from OP_CALL once per call, only when the callee HAS attributes;` |
|        - | 1277 | ` * pDeclClass names the method's declaring class (0 for plain functions).` |
|        - | 1278 | ` */` |
|        - | 1279 | `/*` |
|        - | 1280 | ` * Expand a global constant's value, emitting the php 8.5 #[\Deprecated]` |
|        - | 1281 | ` * E_USER_DEPRECATED notice first when the constant carries attributes` |
|        - | 1282 | `` * (`Constant GD is deprecated since 1.2` — every access re-warns).`` |
|        - | 1283 | ` */` |
|        - | 1284 | `static void VmDeprecatedAttrNoticeSubject(ph7_vm *pVm,SySet *pAttrs,` |
|        - | 1285 | `	const char *zKind,const SyString *pQual,const SyString *pName);` |
|        - | 1286 | `/*` |
|        - | 1287 | ` * Expand a constant, emitting the php 8.5 #[\Deprecated] E_USER_DEPRECATED notice` |
|        - | 1288 | `` * first when a USERLAND constant carries the attribute (`Constant GD is deprecated`` |
|        - | 1289 | `` * since 1.2` — every access re-warns). Engine constants php merely deprecates are`` |
|        - | 1290 | ` * not mimicked: PHL removes them outright (see the scope policy), so there is no` |
|        - | 1291 | ` * engine-side E_DEPRECATED list here.` |
|        - | 1292 | ` */` |
|    23270 | 1293 | `PH7_PRIVATE void VmExpandConstantWithNotice(ph7_vm *pVm,ph7_constant *pCons,ph7_value *pOut)` |
|        5 | 1294 | `{` |
|    23275 | 1295 | `	if( SySetUsed(&pCons->aAttrs) > 0 ){` |
|        7 | 1296 | `		VmDeprecatedAttrNoticeSubject(pVm,&pCons->aAttrs,"Constant",0,&pCons->sName);` |
|        3 | 1297 | `	}` |
|    23275 | 1298 | `	pCons->xExpand(pOut,pCons->pUserData);` |
|    23275 | 1299 | `}` |
|        - | 1300 | `/*` |
|        - | 1301 | ` * Scan a declared-attribute set for #[\Deprecated]; when found, evaluate its` |
|        - | 1302 | ` * message:/since: arguments (positional #0 = message, #1 = since) into the` |
|        - | 1303 | ` * caller's values and return TRUE. The base subject text ("Function f()",` |
|        - | 1304 | ` * "Constant C::K") is the caller's business.` |
|        - | 1305 | ` */` |
|      130 | 1306 | `static int VmDeprecatedAttrExtract(ph7_vm *pVm,SySet *pAttrs,` |
|        - | 1307 | `	ph7_value *pMsg,int *pbMsg,ph7_value *pSince,int *pbSince)` |
|        5 | 1308 | `{` |
|      135 | 1309 | `	ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|        - | 1310 | `	sxu32 n;` |
|      135 | 1311 | `	*pbMsg = *pbSince = 0;` |
|      235 | 1312 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; ++n ){` |
|      139 | 1313 | `		ph7_attribute *pAttr = &aAttr[n];` |
|        - | 1314 | `		ph7_attr_arg *aArg;` |
|      139 | 1315 | `		sxu32 i,nPos = 0;` |
|      134 | 1316 | `		if( SyStringLength(&pAttr->sName) != sizeof("Deprecated")-1` |
|       89 | 1317 | `		 \|\| SyStrnicmp(SyStringData(&pAttr->sName),"Deprecated",sizeof("Deprecated")-1) != 0 ){` |
|      105 | 1318 | `			continue;` |
|        - | 1319 | `		}` |
|       35 | 1320 | `		aArg = (ph7_attr_arg *)SySetBasePtr(&pAttr->aArgs);` |
|       53 | 1321 | `		for( i = 0 ; i < SySetUsed(&pAttr->aArgs) ; ++i ){` |
|       19 | 1322 | `			ph7_attr_arg *pArg = &aArg[i];` |
|       19 | 1323 | `			int isMsg = 0,isSince = 0;` |
|       19 | 1324 | `			if( SyStringLength(&pArg->sName) == 0 ){` |
|        3 | 1325 | `				isMsg = (nPos == 0);` |
|        3 | 1326 | `				isSince = (nPos == 1);` |
|        3 | 1327 | `				nPos++;` |
|       18 | 1328 | `			}else if( SyStringLength(&pArg->sName) == sizeof("message")-1` |
|       12 | 1329 | `			 && SyMemcmp(SyStringData(&pArg->sName),"message",sizeof("message")-1) == 0 ){` |
|        7 | 1330 | `				isMsg = 1;` |
|       14 | 1331 | `			}else if( SyStringLength(&pArg->sName) == sizeof("since")-1` |
|       11 | 1332 | `			 && SyMemcmp(SyStringData(&pArg->sName),"since",sizeof("since")-1) == 0 ){` |
|       11 | 1333 | `				isSince = 1;` |
|        5 | 1334 | `			}` |
|       19 | 1335 | `			if( isMsg && !*pbMsg && SySetUsed(&pArg->aByteCode) > 0 ){` |
|       13 | 1336 | `				if( VmLocalExec(pVm,&pArg->aByteCode,pMsg,FALSE) == SXRET_OK ){` |
|        9 | 1337 | `					if( (pMsg->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 | 1338 | `						PH7_MemObjToString(pMsg);` |
|      ! 0 | 1339 | `					}` |
|        9 | 1340 | `					*pbMsg = 1;` |
|        5 | 1341 | `				}` |
|       15 | 1342 | `			}else if( isSince && !*pbSince && SySetUsed(&pArg->aByteCode) > 0 ){` |
|       11 | 1343 | `				if( VmLocalExec(pVm,&pArg->aByteCode,pSince,FALSE) == SXRET_OK ){` |
|       11 | 1344 | `					if( (pSince->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 | 1345 | `						PH7_MemObjToString(pSince);` |
|      ! 0 | 1346 | `					}` |
|       11 | 1347 | `					*pbSince = 1;` |
|        5 | 1348 | `				}` |
|        5 | 1349 | `			}` |
|       10 | 1350 | `		}` |
|       35 | 1351 | `		return 1;` |
|      ! 0 | 1352 | `	}` |
|      101 | 1353 | `	return 0;` |
|       70 | 1354 | `}` |
|        - | 1355 | `/*` |
|        - | 1356 | ` * Append php's " since X" / ", message" suffixes to a built base subject and` |
|        - | 1357 | ` * emit the E_USER_DEPRECATED notice.` |
|        - | 1358 | ` */` |
|       34 | 1359 | `static void VmDeprecatedEmit(ph7_vm *pVm,SyBlob *pOut,` |
|        - | 1360 | `	ph7_value *pMsg,int bMsg,ph7_value *pSince,int bSince)` |
|        1 | 1361 | `{` |
|       35 | 1362 | `	if( bSince && SyBlobLength(&pSince->sBlob) > 0 ){` |
|       16 | 1363 | `		SyBlobFormat(pOut," since %.*s",(int)SyBlobLength(&pSince->sBlob),` |
|       10 | 1364 | `			(const char *)SyBlobData(&pSince->sBlob));` |
|        5 | 1365 | `	}` |
|       35 | 1366 | `	if( bMsg && SyBlobLength(&pMsg->sBlob) > 0 ){` |
|       13 | 1367 | `		SyBlobFormat(pOut,", %.*s",(int)SyBlobLength(&pMsg->sBlob),` |
|        8 | 1368 | `			(const char *)SyBlobData(&pMsg->sBlob));` |
|        4 | 1369 | `	}` |
|       35 | 1370 | `	VmThrowUserDeprecatedFmt(pVm,"%.*s",(int)SyBlobLength(pOut),(const char *)SyBlobData(pOut));` |
|       35 | 1371 | `}` |
|        - | 1372 | `/*` |
|        - | 1373 | ` * Generic #[\Deprecated] notice for a named subject:` |
|        - | 1374 | ` * "<Kind> [Qual::]Name is deprecated[ since X][, message]".` |
|        - | 1375 | ` */` |
|       18 | 1376 | `static void VmDeprecatedAttrNoticeSubject(ph7_vm *pVm,SySet *pAttrs,` |
|        - | 1377 | `	const char *zKind,const SyString *pQual,const SyString *pName)` |
|        1 | 1378 | `{` |
|        - | 1379 | `	ph7_value sMsg,sSince;` |
|        - | 1380 | `	SyBlob sOut;` |
|        - | 1381 | `	int bMsg,bSince;` |
|       19 | 1382 | `	PH7_MemObjInit(pVm,&sMsg);` |
|       19 | 1383 | `	PH7_MemObjInit(pVm,&sSince);` |
|       19 | 1384 | `	if( VmDeprecatedAttrExtract(pVm,pAttrs,&sMsg,&bMsg,&sSince,&bSince) ){` |
|       15 | 1385 | `		SyBlobInit(&sOut,&pVm->sAllocator);` |
|       15 | 1386 | `		if( pQual ){` |
|       11 | 1387 | `			SyBlobFormat(&sOut,"%s %z::%z is deprecated",zKind,pQual,pName);` |
|        6 | 1388 | `		}else{` |
|        5 | 1389 | `			SyBlobFormat(&sOut,"%s %z is deprecated",zKind,pName);` |
|        - | 1390 | `		}` |
|       15 | 1391 | `		VmDeprecatedEmit(pVm,&sOut,&sMsg,bMsg,&sSince,bSince);` |
|       15 | 1392 | `		SyBlobRelease(&sOut);` |
|        7 | 1393 | `	}` |
|       19 | 1394 | `	PH7_MemObjRelease(&sMsg);` |
|       19 | 1395 | `	PH7_MemObjRelease(&sSince);` |
|       19 | 1396 | `}` |
|      112 | 1397 | `PH7_PRIVATE void VmDeprecatedAttrNotice(ph7_vm *pVm,ph7_vm_func *pFunc,ph7_class *pDeclClass)` |
|        5 | 1398 | `{` |
|        - | 1399 | `	ph7_value sMsg,sSince;` |
|        - | 1400 | `	SyBlob sOut;` |
|        - | 1401 | `	int bMsg,bSince;` |
|      117 | 1402 | `	PH7_MemObjInit(pVm,&sMsg);` |
|      117 | 1403 | `	PH7_MemObjInit(pVm,&sSince);` |
|      117 | 1404 | `	if( VmDeprecatedAttrExtract(pVm,&pFunc->aAttrs,&sMsg,&bMsg,&sSince,&bSince) ){` |
|       21 | 1405 | `		SyBlobInit(&sOut,&pVm->sAllocator);` |
|       21 | 1406 | `		if( pDeclClass ){` |
|        5 | 1407 | `			SyBlobFormat(&sOut,"Method %z::%z() is deprecated",&pDeclClass->sName,&pFunc->sName);` |
|        3 | 1408 | `		}else{` |
|       17 | 1409 | `			SyBlobFormat(&sOut,"Function %z() is deprecated",&pFunc->sName);` |
|        - | 1410 | `		}` |
|       21 | 1411 | `		VmDeprecatedEmit(pVm,&sOut,&sMsg,bMsg,&sSince,bSince);` |
|       21 | 1412 | `		SyBlobRelease(&sOut);` |
|       10 | 1413 | `	}` |
|      117 | 1414 | `	PH7_MemObjRelease(&sMsg);` |
|      117 | 1415 | `	PH7_MemObjRelease(&sSince);` |
|      117 | 1416 | `}` |
|        - | 1417 | `/*` |
|        - | 1418 | ` * Same notice for a #[\Deprecated] class constant, at its static-access site:` |
|        - | 1419 | `` * php's `Constant C::K is deprecated` (each access re-warns).`` |
|        - | 1420 | ` */` |
|       12 | 1421 | `PH7_PRIVATE void VmDeprecatedConstNotice(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pMember)` |
|        1 | 1422 | `{` |
|       19 | 1423 | `	VmDeprecatedAttrNoticeSubject(pVm,&pMember->aAttrs,` |
|       12 | 1424 | `		(pMember->iFlags & PH7_CLASS_ATTR_ENUMCASE) ? "Enum case" : "Constant",` |
|       12 | 1425 | `		&pClass->sName,&pMember->sName);` |
|       13 | 1426 | `}` |
|        - | 1427 |  |
