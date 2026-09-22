# src/ph7/vm_arg_check.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 403/425 lines (94.82%)

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
|        - |   81 | `	{ "mb_convert_encoding",       2, 1 },` |
|        - |   82 | `	{ "mb_detect_encoding",        1, 1 },` |
|        - |   83 | `	{ "mb_ord",                    1, 1 },` |
|        - |   84 | `	{ "mb_str_split",              1, 1 },` |
|        - |   85 | `	{ "mb_stripos",                2, 1 },` |
|        - |   86 | `	{ "mb_strlen",                 1, 1 },` |
|        - |   87 | `	{ "mb_strpos",                 2, 1 },` |
|        - |   88 | `	{ "mb_strrpos",                2, 1 },` |
|        - |   89 | `	{ "mb_strtolower",             1, 1 },` |
|        - |   90 | `	{ "mb_strtoupper",             1, 1 },` |
|        - |   91 | `	{ "mb_strwidth",               1, 1 },` |
|        - |   92 | `	{ "mb_substr",                 2, 1 },` |
|        - |   93 | `	{ "nl2br",                     1, 1 },` |
|        - |   94 | `	{ "printf",                    1, 1 },` |
|        - |   95 | `	{ "quotemeta",                 1, 0 },` |
|        - |   96 | `	{ "rtrim",                     1, 1 },` |
|        - |   97 | `	{ "soundex",                   1, 0 },` |
|        - |   98 | `	{ "sprintf",                   1, 1 },` |
|        - |   99 | `	{ "str_getcsv",                1, 1 },` |
|        - |  100 | `	{ "str_shuffle",               1, 0 },` |
|        - |  101 | `	{ "strcasecmp",                2, 0 },` |
|        - |  102 | `	{ "strchr",                    2, 1 },` |
|        - |  103 | `	{ "strcmp",                    2, 0 },` |
|        - |  104 | `	{ "strnatcasecmp",             2, 0 },` |
|        - |  105 | `	{ "strnatcmp",                 2, 0 },` |
|        - |  106 | `	{ "strcoll",                   2, 0 },` |
|        - |  107 | `	{ "strip_tags",                1, 1 },` |
|        - |  108 | `	{ "stripslashes",              1, 0 },` |
|        - |  109 | `	{ "strlen",                    1, 0 },` |
|        - |  110 | `	{ "strrev",                    1, 0 },` |
|        - |  111 | `	{ "strtok",                    1, 1 },` |
|        - |  112 | `	{ "strtolower",                1, 0 },` |
|        - |  113 | `	{ "strtoupper",                1, 0 },` |
|        - |  114 | `	{ "strtr",                     2, 0 },` |
|        - |  115 | `	{ "trim",                      1, 1 },` |
|        - |  116 | `	{ "ucfirst",                   1, 0 },` |
|        - |  117 | `	{ "ucwords",                   1, 1 },` |
|        - |  118 | `	{ "vfprintf",                  3, 0 },` |
|        - |  119 | `	{ "vprintf",                   2, 0 },` |
|        - |  120 | `	{ "vsprintf",                  2, 0 },` |
|        - |  121 | `	{ "wordwrap",                  1, 1 },` |
|        - |  122 | `	/* Ctype family */` |
|        - |  123 | `	{ "ctype_alnum",               1, 0 },` |
|        - |  124 | `	{ "ctype_alpha",               1, 0 },` |
|        - |  125 | `	{ "ctype_cntrl",               1, 0 },` |
|        - |  126 | `	{ "ctype_digit",               1, 0 },` |
|        - |  127 | `	{ "ctype_graph",               1, 0 },` |
|        - |  128 | `	{ "ctype_lower",               1, 0 },` |
|        - |  129 | `	{ "ctype_print",               1, 0 },` |
|        - |  130 | `	{ "ctype_punct",               1, 0 },` |
|        - |  131 | `	{ "ctype_space",               1, 0 },` |
|        - |  132 | `	{ "ctype_upper",               1, 0 },` |
|        - |  133 | `	{ "ctype_xdigit",              1, 0 },` |
|        - |  134 | `	/* Math family */` |
|        - |  135 | `	{ "base_convert",              3, 0 },` |
|        - |  136 | `	{ "cos",                       1, 0 },` |
|        - |  137 | `	{ "cosh",                      1, 0 },` |
|        - |  138 | `	{ "crc32",                     1, 0 },` |
|        - |  139 | `	{ "decbin",                    1, 0 },` |
|        - |  140 | `	{ "dechex",                    1, 0 },` |
|        - |  141 | `	{ "decoct",                    1, 0 },` |
|        - |  142 | `	{ "exp",                       1, 0 },` |
|        - |  143 | `	{ "log10",                     1, 0 },` |
|        - |  144 | `	{ "md5",                       1, 1 },` |
|        - |  145 | `	{ "round",                     1, 1 },` |
|        - |  146 | `	{ "sha1",                      1, 1 },` |
|        - |  147 | `	{ "sin",                       1, 0 },` |
|        - |  148 | `	{ "sinh",                      1, 0 },` |
|        - |  149 | `	{ "sqrt",                      1, 0 },` |
|        - |  150 | `	{ "tan",                       1, 0 },` |
|        - |  151 | `	{ "tanh",                      1, 0 },` |
|        - |  152 | `	/* Type/var family */` |
|        - |  153 | `	{ "floatval",                  1, 0 },` |
|        - |  154 | `	{ "get_resource_id",           1, 0 },` |
|        - |  155 | `	{ "get_resource_type",         1, 0 },` |
|        - |  156 | `	{ "gettype",                   1, 0 },` |
|        - |  157 | `	{ "intval",                    1, 1 },` |
|        - |  158 | `	{ "is_array",                  1, 0 },` |
|        - |  159 | `	{ "is_bool",                   1, 0 },` |
|        - |  160 | `	{ "is_callable",               1, 1 },` |
|        - |  161 | `	{ "is_double",                 1, 0 },` |
|        - |  162 | `	{ "is_float",                  1, 0 },` |
|        - |  163 | `	{ "is_int",                    1, 0 },` |
|        - |  164 | `	{ "is_integer",                1, 0 },` |
|        - |  165 | `	{ "is_long",                   1, 0 },` |
|        - |  166 | `	{ "is_null",                   1, 0 },` |
|        - |  167 | `	{ "is_numeric",                1, 0 },` |
|        - |  168 | `	{ "is_object",                 1, 0 },` |
|        - |  169 | `	{ "is_resource",               1, 0 },` |
|        - |  170 | `	{ "is_scalar",                 1, 0 },` |
|        - |  171 | `	{ "is_string",                 1, 0 },` |
|        - |  172 | `	{ "print_r",                   1, 1 },` |
|        - |  173 | `	{ "strval",                    1, 0 },` |
|        - |  174 | `	{ "var_dump",                  1, 1 },` |
|        - |  175 | `	{ "var_export",                1, 1 },` |
|        - |  176 | `	/* Array/iterator family */` |
|        - |  177 | `	{ "array_filter",              1, 1 },` |
|        - |  178 | `	{ "array_product",             1, 0 },` |
|        - |  179 | `	{ "array_rand",                1, 1 },` |
|        - |  180 | `	{ "compact",                   1, 1 },` |
|        - |  181 | `	{ "current",                   1, 0 },` |
|        - |  182 | `	{ "end",                       1, 0 },` |
|        - |  183 | `	{ "extract",                   1, 1 },` |
|        - |  184 | `	{ "iterator_apply",            2, 1 },` |
|        - |  185 | `	{ "iterator_count",            1, 0 },` |
|        - |  186 | `	{ "iterator_to_array",         1, 1 },` |
|        - |  187 | `	{ "key",                       1, 0 },` |
|        - |  188 | `	{ "krsort",                    1, 1 },` |
|        - |  189 | `	{ "ksort",                     1, 1 },` |
|        - |  190 | `	{ "next",                      1, 0 },` |
|        - |  191 | `	{ "pos",                       1, 0 },` |
|        - |  192 | `	{ "prev",                      1, 0 },` |
|        - |  193 | `	{ "reset",                     1, 0 },` |
|        - |  194 | `	{ "rsort",                     1, 1 },` |
|        - |  195 | `	{ "shuffle",                   1, 0 },` |
|        - |  196 | `	{ "sort",                      1, 1 },` |
|        - |  197 | `	{ "uasort",                    2, 0 },` |
|        - |  198 | `	{ "uksort",                    2, 0 },` |
|        - |  199 | `	{ "usort",                     2, 0 },` |
|        - |  200 | `	/* Class/reflection family */` |
|        - |  201 | `	{ "class_alias",               2, 1 },` |
|        - |  202 | `	{ "class_exists",              1, 1 },` |
|        - |  203 | `	{ "enum_exists",               1, 1 },` |
|        - |  204 | `	{ "get_class_methods",         1, 0 },` |
|        - |  205 | `	{ "get_class_vars",            1, 0 },` |
|        - |  206 | `	{ "get_object_vars",           1, 0 },` |
|        - |  207 | `	{ "interface_exists",          1, 1 },` |
|        - |  208 | `	{ "trait_exists",              1, 1 },` |
|        - |  209 | `	{ "is_a",                      2, 1 },` |
|        - |  210 | `	{ "is_subclass_of",            2, 1 },` |
|        - |  211 | `	{ "method_exists",             2, 0 },` |
|        - |  212 | `	{ "property_exists",           2, 0 },` |
|        - |  213 | `	{ "spl_autoload",              1, 1 },` |
|        - |  214 | `	{ "spl_autoload_unregister",   1, 0 },` |
|        - |  215 | `	{ "spl_object_hash",           1, 0 },` |
|        - |  216 | `	{ "spl_object_id",             1, 0 },` |
|        - |  217 | `	/* Filesystem/IO family */` |
|        - |  218 | `	{ "basename",                  1, 1 },` |
|        - |  219 | `	{ "chdir",                     1, 0 },` |
|        - |  220 | `	{ "chgrp",                     2, 0 },` |
|        - |  221 | `	{ "dirname",                   1, 1 },` |
|        - |  222 | `	{ "disk_free_space",           1, 0 },` |
|        - |  223 | `	{ "disk_total_space",          1, 0 },` |
|        - |  224 | `	{ "diskfreespace",             1, 0 },` |
|        - |  225 | `	{ "fclose",                    1, 0 },` |
|        - |  226 | `	{ "feof",                      1, 0 },` |
|        - |  227 | `	{ "fflush",                    1, 0 },` |
|        - |  228 | `	{ "fgetc",                     1, 0 },` |
|        - |  229 | `	{ "fgetcsv",                   1, 1 },` |
|        - |  230 | `	{ "file",                      1, 1 },` |
|        - |  231 | `	{ "file_exists",               1, 0 },` |
|        - |  232 | `	{ "fileatime",                 1, 0 },` |
|        - |  233 | `	{ "filectime",                 1, 0 },` |
|        - |  234 | `	{ "filemtime",                 1, 0 },` |
|        - |  235 | `	{ "filesize",                  1, 0 },` |
|        - |  236 | `	{ "filetype",                  1, 0 },` |
|        - |  237 | `	{ "flock",                     2, 1 },` |
|        - |  238 | `	{ "fpassthru",                 1, 0 },` |
|        - |  239 | `	{ "fputcsv",                   2, 1 },` |
|        - |  240 | `	{ "fputs",                     2, 1 },` |
|        - |  241 | `	{ "fseek",                     2, 1 },` |
|        - |  242 | `	{ "fstat",                     1, 0 },` |
|        - |  243 | `	{ "ftell",                     1, 0 },` |
|        - |  244 | `	{ "ftruncate",                 2, 0 },` |
|        - |  245 | `	{ "getopt",                    1, 1 },` |
|        - |  246 | `	{ "is_dir",                    1, 0 },` |
|        - |  247 | `	{ "is_executable",             1, 0 },` |
|        - |  248 | `	{ "is_file",                   1, 0 },` |
|        - |  249 | `	{ "is_link",                   1, 0 },` |
|        - |  250 | `	{ "is_readable",               1, 0 },` |
|        - |  251 | `	{ "is_writable",               1, 0 },` |
|        - |  252 | `	{ "lstat",                     1, 0 },` |
|        - |  253 | `	{ "md5_file",                  1, 1 },` |
|        - |  254 | `	{ "opendir",                   1, 1 },` |
|        - |  255 | `	{ "pathinfo",                  1, 1 },` |
|        - |  256 | `	{ "pclose",                    1, 0 },` |
|        - |  257 | `	{ "realpath",                  1, 0 },` |
|        - |  258 | `	{ "rewind",                    1, 0 },` |
|        - |  259 | `	{ "sha1_file",                 1, 1 },` |
|        - |  260 | `	{ "stat",                      1, 0 },` |
|        - |  261 | `	/* Date family */` |
|        - |  262 | `	{ "date",                      1, 1 },` |
|        - |  263 | `	{ "date_default_timezone_set", 1, 1 },` |
|        - |  264 | `	{ "gmdate",                    1, 1 },` |
|        - |  265 | `	{ "gmmktime",                  1, 1 },` |
|        - |  266 | `	{ "idate",                     1, 1 },` |
|        - |  267 | `	{ "mktime",                    1, 1 },` |
|        - |  268 | `	/* Encoding/URL family */` |
|        - |  269 | `	{ "base64_decode",             1, 1 },` |
|        - |  270 | `	{ "base64_encode",             1, 0 },` |
|        - |  271 | `	{ "convert_uudecode",          1, 0 },` |
|        - |  272 | `	{ "convert_uuencode",          1, 0 },` |
|        - |  273 | `	{ "parse_ini_file",            1, 1 },` |
|        - |  274 | `	{ "parse_ini_string",          1, 1 },` |
|        - |  275 | `	{ "parse_url",                 1, 1 },` |
|        - |  276 | `	{ "rawurldecode",              1, 0 },` |
|        - |  277 | `	{ "rawurlencode",              1, 0 },` |
|        - |  278 | `	{ "urldecode",                 1, 0 },` |
|        - |  279 | `	{ "urlencode",                 1, 0 },` |
|        - |  280 | `	/* JSON/serialize family */` |
|        - |  281 | `	{ "filter_var",                1, 1 },` |
|        - |  282 | `	{ "json_decode",               1, 1 },` |
|        - |  283 | `	{ "json_encode",               1, 1 },` |
|        - |  284 | `	{ "json_validate",             1, 1 },` |
|        - |  285 | `	{ "serialize",                 1, 0 },` |
|        - |  286 | `	{ "unserialize",               1, 1 },` |
|        - |  287 | `	/* PCRE family */` |
|        - |  288 | `	{ "preg_match",                2, 1 },` |
|        - |  289 | `	{ "preg_match_all",            2, 1 },` |
|        - |  290 | `	{ "preg_quote",                1, 1 },` |
|        - |  291 | `	{ "preg_replace",              3, 1 },` |
|        - |  292 | `	{ "preg_replace_callback",     3, 1 },` |
|        - |  293 | `	{ "preg_split",                2, 1 },` |
|        - |  294 | `	/* XML family */` |
|        - |  295 | `	/* Constants/misc family */` |
|        - |  296 | `	{ "call_user_func",            1, 1 },` |
|        - |  297 | `	{ "call_user_func_array",      2, 0 },` |
|        - |  298 | `	{ "constant",                  1, 0 },` |
|        - |  299 | `	{ "define",                    2, 1 },` |
|        - |  300 | `	{ "defined",                   1, 0 },` |
|        - |  301 | `	{ "error_log",                 1, 1 },` |
|        - |  302 | `	{ "fnmatch",                   2, 1 },` |
|        - |  303 | `	{ "forward_static_call",       1, 1 },` |
|        - |  304 | `	{ "forward_static_call_array", 2, 0 },` |
|        - |  305 | `	{ "func_get_arg",              1, 0 },` |
|        - |  306 | `	{ "function_exists",           1, 0 },` |
|        - |  307 | `	{ "header",                    1, 1 },` |
|        - |  308 | `	{ "password_get_info",         1, 0 },` |
|        - |  309 | `	{ "putenv",                    1, 0 },` |
|        - |  310 | `	{ "register_shutdown_function", 1, 1 },` |
|        - |  311 | `	{ "set_error_handler",         1, 1 },` |
|        - |  312 | `	{ "set_exception_handler",     1, 0 },` |
|        - |  313 | `	{ "setcookie",                 1, 1 },` |
|        - |  314 | `	{ "setrawcookie",              1, 1 },` |
|        - |  315 | `	{ "trigger_error",             1, 1 },` |
|        - |  316 | `	{ "user_error",                1, 1 },` |
|        - |  317 | `	/*` |
|        - |  318 | `	 * Overrides for signatures that under-report their own minimum: the callback` |
|        - |  319 | `	 * of these three hides inside the variadic tail ("array $array, ...$rest"),` |
|        - |  320 | `	 * so the derivation reads 1 where php requires 2.` |
|        - |  321 | `	 */` |
|        - |  322 | `	{ "array_udiff",               2, 1 },` |
|        - |  323 | `	{ "array_uintersect",          2, 1 },` |
|        - |  324 | `	{ "array_diff_uassoc",         2, 1 },` |
|        - |  325 | `};` |
|        - |  326 | `/*` |
|        - |  327 | ` * Stamp the minimum-arity metadata from aBuiltinArity[] onto the already` |
|        - |  328 | ` * registered host functions. Called once at VM init after every builtin family` |
|        - |  329 | ` * has been installed into hHostFunction. A name absent from the hash (e.g. a` |
|        - |  330 | ` * build without a given extension) is simply skipped.` |
|        - |  331 | ` */` |
|     3956 |  332 | `PH7_PRIVATE void VmSetBuiltinArity(ph7_vm *pVm)` |
|        5 |  333 | `{` |
|        - |  334 | `	sxu32 n;` |
|  1072081 |  335 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinArity) ; ++n ){` |
|  1068125 |  336 | `		const struct VmBuiltinArity *p = &aBuiltinArity[n];` |
|  2136245 |  337 | `		SyHashEntry *pEntry = SyHashGet(&pVm->hHostFunction,` |
|  1068120 |  338 | `			(const void *)p->zName,SyStrlen(p->zName));` |
|  1068125 |  339 | `		if( pEntry ){` |
|  1068125 |  340 | `			ph7_user_func *pFunc = (ph7_user_func *)pEntry->pUserData;` |
|  1068125 |  341 | `			pFunc->nMinArg  = p->nMin;` |
|  1068125 |  342 | `			pFunc->bAtLeast = p->bAtLeast;` |
|   534060 |  343 | `		}` |
|   534065 |  344 | `	}` |
|     3961 |  345 | `}` |
|        - |  346 | `/*` |
|        - |  347 | ` * PHP 8.5 parameter signatures for the C builtins, generated offline from` |
|        - |  348 | ` * a real PHP 8.5 ReflectionFunction dump over PHL's registered function` |
|        - |  349 | ` * list (see the plan's Reflection section). "= ?" marks an optional` |
|        - |  350 | ` * parameter whose default is not representable as a short literal.` |
|        - |  351 | ` * Reflection parses these strings on demand; unlisted builtins degrade to` |
|        - |  352 | ` * the min-arity data.` |
|        - |  353 | ` */` |
|        - |  354 | `static const struct VmBuiltinSig {` |
|        - |  355 | `	const char *zName;` |
|        - |  356 | `	const char *zSig;` |
|        - |  357 | `	const char *zRet;` |
|        - |  358 | `} aBuiltinSig[] = {` |
|        - |  359 | `	{ "abs", "int\|float $num", "int\|float" },` |
|        - |  360 | `	{ "acos", "float $num", "float" },` |
|        - |  361 | `	{ "addcslashes", "string $string, string $characters", "string" },` |
|        - |  362 | `	{ "addslashes", "string $string", "string" },` |
|        - |  363 | `	{ "array_all", "array $array, callable $callback", "bool" },` |
|        - |  364 | `	{ "array_any", "array $array, callable $callback", "bool" },` |
|        - |  365 | `	{ "array_chunk", "array $array, int $length, bool $preserve_keys = false", "array" },` |
|        - |  366 | `	{ "array_column", "array $array, string\|int\|null $column_key, string\|int\|null $index_key = NULL", "array" },` |
|        - |  367 | `	{ "array_combine", "array $keys, array $values", "array" },` |
|        - |  368 | `	{ "array_diff", "array $array, array ...$arrays = ?", "array" },` |
|        - |  369 | `	{ "array_diff_assoc", "array $array, array ...$arrays = ?", "array" },` |
|        - |  370 | `	{ "array_diff_key", "array $array, array ...$arrays = ?", "array" },` |
|        - |  371 | `	{ "array_diff_uassoc", "array $array, ...$rest = ?", "array" },` |
|        - |  372 | `	{ "array_fill", "int $start_index, int $count, mixed $value", "array" },` |
|        - |  373 | `	{ "array_fill_keys", "array $keys, mixed $value", "array" },` |
|        - |  374 | `	{ "array_filter", "array $array, ?callable $callback = NULL, int $mode = 0", "array" },` |
|        - |  375 | `	{ "array_find", "array $array, callable $callback", "mixed" },` |
|        - |  376 | `	{ "array_find_key", "array $array, callable $callback", "mixed" },` |
|        - |  377 | `	{ "array_first", "array $array", "mixed" },` |
|        - |  378 | `	{ "array_flip", "array $array", "array" },` |
|        - |  379 | `	{ "array_intersect", "array $array, array ...$arrays = ?", "array" },` |
|        - |  380 | `	{ "array_intersect_assoc", "array $array, array ...$arrays = ?", "array" },` |
|        - |  381 | `	{ "array_intersect_key", "array $array, array ...$arrays = ?", "array" },` |
|        - |  382 | `	{ "array_is_list", "array $array", "bool" },` |
|        - |  383 | `	{ "array_key_exists", "$key, array $array", "bool" },` |
|        - |  384 | `	{ "array_key_first", "array $array", "string\|int\|null" },` |
|        - |  385 | `	{ "array_key_last", "array $array", "string\|int\|null" },` |
|        - |  386 | `	{ "array_keys", "array $array, mixed $filter_value = ?, bool $strict = false", "array" },` |
|        - |  387 | `	{ "array_last", "array $array", "mixed" },` |
|        - |  388 | `	{ "array_map", "?callable $callback, array $array, array ...$arrays = ?", "array" },` |
|        - |  389 | `	{ "array_merge", "array ...$arrays = ?", "array" },` |
|        - |  390 | `	{ "array_pad", "array $array, int $length, mixed $value", "array" },` |
|        - |  391 | `	{ "array_pop", "array &$array", "mixed" },` |
|        - |  392 | `	{ "array_product", "array $array", "int\|float" },` |
|        - |  393 | `	{ "array_push", "array &$array, mixed ...$values = ?", "int" },` |
|        - |  394 | `	{ "array_rand", "array $array, int $num = 1", "array\|string\|int" },` |
|        - |  395 | `	{ "array_reduce", "array $array, callable $callback, mixed $initial = NULL", "mixed" },` |
|        - |  396 | `	{ "array_replace", "array $array, array ...$replacements = ?", "array" },` |
|        - |  397 | `	{ "array_reverse", "array $array, bool $preserve_keys = false", "array" },` |
|        - |  398 | `	{ "array_search", "mixed $needle, array $haystack, bool $strict = false", "string\|int\|false" },` |
|        - |  399 | `	{ "array_shift", "array &$array", "mixed" },` |
|        - |  400 | `	{ "array_slice", "array $array, int $offset, ?int $length = NULL, bool $preserve_keys = false", "array" },` |
|        - |  401 | `	{ "array_splice", "array &$array, int $offset, ?int $length = NULL, mixed $replacement = ?", "array" },` |
|        - |  402 | `	{ "array_sum", "array $array", "int\|float" },` |
|        - |  403 | `	{ "array_udiff", "array $array, ...$rest = ?", "array" },` |
|        - |  404 | `	{ "array_uintersect", "array $array, ...$rest = ?", "array" },` |
|        - |  405 | `	{ "array_unique", "array $array, int $flags = 2", "array" },` |
|        - |  406 | `	{ "array_values", "array $array", "array" },` |
|        - |  407 | `	{ "array_walk", "object\|array &$array, callable $callback, mixed $arg = ?", "true" },` |
|        - |  408 | `	{ "array_walk_recursive", "object\|array &$array, callable $callback, mixed $arg = ?", "true" },` |
|        - |  409 | `	{ "arsort", "array &$array, int $flags = 0", "true" },` |
|        - |  410 | `	{ "asin", "float $num", "float" },` |
|        - |  411 | `	{ "asort", "array &$array, int $flags = 0", "true" },` |
|        - |  412 | `	{ "assert", "mixed $assertion, Throwable\|string\|null $description = NULL", "bool" },` |
|        - |  413 | `	{ "atan", "float $num", "float" },` |
|        - |  414 | `	{ "atan2", "float $y, float $x", "float" },` |
|        - |  415 | `	{ "base64_decode", "string $string, bool $strict = false", "string\|false" },` |
|        - |  416 | `	{ "base64_encode", "string $string", "string" },` |
|        - |  417 | `	{ "base_convert", "string $num, int $from_base, int $to_base", "string" },` |
|        - |  418 | `	{ "basename", "string $path, string $suffix = ''", "string" },` |
|        - |  419 | `	{ "bin2hex", "string $string", "string" },` |
|        - |  420 | `	{ "bindec", "string $binary_string", "int\|float" },` |
|        - |  421 | `	{ "boolval", "mixed $value", "bool" },` |
|        - |  422 | `	{ "call_user_func", "callable $callback, mixed ...$args = ?", "mixed" },` |
|        - |  423 | `	{ "call_user_func_array", "callable $callback, array $args", "mixed" },` |
|        - |  424 | `	{ "ceil", "int\|float $num", "float" },` |
|        - |  425 | `	{ "chdir", "string $directory", "bool" },` |
|        - |  426 | `	{ "chgrp", "string $filename, string\|int $group", "bool" },` |
|        - |  427 | `	{ "chmod", "string $filename, int $permissions", "bool" },` |
|        - |  428 | `	{ "chop", "string $string, string $characters = ?", "string" },` |
|        - |  429 | `	{ "chown", "string $filename, string\|int $user", "bool" },` |
|        - |  430 | `	{ "chr", "int $codepoint", "string" },` |
|        - |  431 | `	{ "chunk_split", "string $string, int $length = 76, string $separator = ?", "string" },` |
|        - |  432 | `	{ "class_alias", "string $class, string $alias, bool $autoload = true", "bool" },` |
|        - |  433 | `	{ "class_exists", "string $class, bool $autoload = true", "bool" },` |
|        - |  434 | `	{ "enum_exists", "string $enum, bool $autoload = true", "bool" },` |
|        - |  435 | `	{ "closedir", "$dir_handle = NULL", "void" },` |
|        - |  436 | `	{ "compact", "$var_name, ...$var_names = ?", "array" },` |
|        - |  437 | `	{ "constant", "string $name", "mixed" },` |
|        - |  438 | `	{ "convert_uudecode", "string $string", "string\|false" },` |
|        - |  439 | `	{ "convert_uuencode", "string $string", "string" },` |
|        - |  440 | `	{ "copy", "string $from, string $to, $context = NULL", "bool" },` |
|        - |  441 | `	{ "cos", "float $num", "float" },` |
|        - |  442 | `	{ "cosh", "float $num", "float" },` |
|        - |  443 | `	{ "count", "Countable\|array $value, int $mode = 0", "int" },` |
|        - |  444 | `	{ "crc32", "string $string", "int" },` |
|        - |  445 | `	{ "ctype_alnum", "mixed $text", "bool" },` |
|        - |  446 | `	{ "ctype_alpha", "mixed $text", "bool" },` |
|        - |  447 | `	{ "ctype_cntrl", "mixed $text", "bool" },` |
|        - |  448 | `	{ "ctype_digit", "mixed $text", "bool" },` |
|        - |  449 | `	{ "ctype_graph", "mixed $text", "bool" },` |
|        - |  450 | `	{ "ctype_lower", "mixed $text", "bool" },` |
|        - |  451 | `	{ "ctype_print", "mixed $text", "bool" },` |
|        - |  452 | `	{ "ctype_punct", "mixed $text", "bool" },` |
|        - |  453 | `	{ "ctype_space", "mixed $text", "bool" },` |
|        - |  454 | `	{ "ctype_upper", "mixed $text", "bool" },` |
|        - |  455 | `	{ "ctype_xdigit", "mixed $text", "bool" },` |
|        - |  456 | `	{ "current", "object\|array $array", "mixed" },` |
|        - |  457 | `	{ "date", "string $format, ?int $timestamp = NULL", "string" },` |
|        - |  458 | `	{ "date_default_timezone_get", "", "string" },` |
|        - |  459 | `	{ "date_default_timezone_set", "string $timezoneId", "bool" },` |
|        - |  460 | `	{ "debug_backtrace", "int $options = 1, int $limit = 0", "array" },` |
|        - |  461 | `	{ "debug_print_backtrace", "int $options = 0, int $limit = 0", "void" },` |
|        - |  462 | `	{ "decbin", "int $num", "string" },` |
|        - |  463 | `	{ "dechex", "int $num", "string" },` |
|        - |  464 | `	{ "decoct", "int $num", "string" },` |
|        - |  465 | `	{ "define", "string $constant_name, mixed $value, bool $case_insensitive = false", "bool" },` |
|        - |  466 | `	{ "defined", "string $constant_name", "bool" },` |
|        - |  467 | `	{ "die", "string\|int $status = 0", "never" },` |
|        - |  468 | `	{ "dirname", "string $path, int $levels = 1", "string" },` |
|        - |  469 | `	{ "disk_free_space", "string $directory", "float\|false" },` |
|        - |  470 | `	{ "disk_total_space", "string $directory", "float\|false" },` |
|        - |  471 | `	{ "diskfreespace", "string $directory", "float\|false" },` |
|        - |  472 | `	{ "end", "object\|array &$array", "mixed" },` |
|        - |  473 | `	{ "error_get_last", "", "?array" },` |
|        - |  474 | `	{ "error_clear_last", "", "void" },` |
|        - |  475 | `	{ "error_log", "string $message, int $message_type = 0, ?string $destination = NULL, ?string $additional_headers = NULL", "bool" },` |
|        - |  476 | `	{ "error_reporting", "?int $error_level = NULL", "int" },` |
|        - |  477 | `	{ "exit", "string\|int $status = 0", "never" },` |
|        - |  478 | `	{ "exp", "float $num", "float" },` |
|        - |  479 | `	{ "explode", "string $separator, string $string, int $limit = 9223372036854775807", "array" },` |
|        - |  480 | `	{ "extract", "array &$array, int $flags = 0, string $prefix = ''", "int" },` |
|        - |  481 | `	{ "fclose", "$stream", "bool" },` |
|        - |  482 | `	{ "feof", "$stream", "bool" },` |
|        - |  483 | `	{ "fflush", "$stream", "bool" },` |
|        - |  484 | `	{ "fgetc", "$stream", "string\|false" },` |
|        - |  485 | `	{ "fgetcsv", "$stream, ?int $length = NULL, string $separator = ',', string $enclosure = '\"', string $escape = '\\\\'", "array\|false" },` |
|        - |  486 | `	{ "fgets", "$stream, ?int $length = NULL", "string\|false" },` |
|        - |  487 | `	{ "file", "string $filename, int $flags = 0, $context = NULL", "array\|false" },` |
|        - |  488 | `	{ "file_exists", "string $filename", "bool" },` |
|        - |  489 | `	{ "file_get_contents", "string $filename, bool $use_include_path = false, $context = NULL, int $offset = 0, ?int $length = NULL", "string\|false" },` |
|        - |  490 | `	{ "file_put_contents", "string $filename, mixed $data, int $flags = 0, $context = NULL", "int\|false" },` |
|        - |  491 | `	{ "fileatime", "string $filename", "int\|false" },` |
|        - |  492 | `	{ "filectime", "string $filename", "int\|false" },` |
|        - |  493 | `	{ "filemtime", "string $filename", "int\|false" },` |
|        - |  494 | `	{ "filesize", "string $filename", "int\|false" },` |
|        - |  495 | `	{ "filetype", "string $filename", "string\|false" },` |
|        - |  496 | `	{ "filter_input", "int $type, string $var_name, int $filter = 516, array\|int $options = 0", "mixed" },` |
|        - |  497 | `	{ "filter_var", "mixed $value, int $filter = 516, array\|int $options = 0", "mixed" },` |
|        - |  498 | `	{ "floatval", "mixed $value", "float" },` |
|        - |  499 | `	{ "flock", "$stream, int $operation, &$would_block = NULL", "bool" },` |
|        - |  500 | `	{ "floor", "int\|float $num", "float" },` |
|        - |  501 | `	{ "flush", "", "void" },` |
|        - |  502 | `	{ "fmod", "float $num1, float $num2", "float" },` |
|        - |  503 | `	{ "fnmatch", "string $pattern, string $filename, int $flags = 0", "bool" },` |
|        - |  504 | `	{ "fopen", "string $filename, string $mode, bool $use_include_path = false, $context = NULL", "" },` |
|        - |  505 | `	{ "forward_static_call", "callable $callback, mixed ...$args = ?", "mixed" },` |
|        - |  506 | `	{ "forward_static_call_array", "callable $callback, array $args", "mixed" },` |
|        - |  507 | `	{ "fpassthru", "$stream", "int" },` |
|        - |  508 | `	{ "fprintf", "$stream, string $format, mixed ...$values = ?", "int" },` |
|        - |  509 | `	{ "fputcsv", "$stream, array $fields, string $separator = ',', string $enclosure = '\"', string $escape = '\\\\', string $eol = ?", "int\|false" },` |
|        - |  510 | `	{ "fputs", "$stream, string $data, ?int $length = NULL", "int\|false" },` |
|        - |  511 | `	{ "fread", "$stream, int $length", "string\|false" },` |
|        - |  512 | `	{ "fseek", "$stream, int $offset, int $whence = 0", "int" },` |
|        - |  513 | `	{ "fstat", "$stream", "array\|false" },` |
|        - |  514 | `	{ "ftell", "$stream", "int\|false" },` |
|        - |  515 | `	{ "ftruncate", "$stream, int $size", "bool" },` |
|        - |  516 | `	{ "func_get_arg", "int $position", "mixed" },` |
|        - |  517 | `	{ "func_get_args", "", "array" },` |
|        - |  518 | `	{ "func_num_args", "", "int" },` |
|        - |  519 | `	{ "function_exists", "string $function", "bool" },` |
|        - |  520 | `	{ "fwrite", "$stream, string $data, ?int $length = NULL", "int\|false" },` |
|        - |  521 | `	{ "gc_collect_cycles", "", "int" },` |
|        - |  522 | `	{ "gc_disable", "", "void" },` |
|        - |  523 | `	{ "gc_enable", "", "void" },` |
|        - |  524 | `	{ "gc_enabled", "", "bool" },` |
|        - |  525 | `	{ "gc_mem_caches", "", "int" },` |
|        - |  526 | `	{ "gc_status", "", "array" },` |
|        - |  527 | `	{ "get_called_class", "", "string" },` |
|        - |  528 | `	{ "get_class", "object $object = ?", "string" },` |
|        - |  529 | `	{ "get_class_methods", "object\|string $object_or_class", "array" },` |
|        - |  530 | `	{ "get_class_vars", "string $class", "array" },` |
|        - |  531 | `	{ "get_current_user", "", "string" },` |
|        - |  532 | `	{ "get_declared_classes", "", "array" },` |
|        - |  533 | `	{ "get_declared_interfaces", "", "array" },` |
|        - |  534 | `	{ "get_defined_constants", "bool $categorize = false", "array" },` |
|        - |  535 | `	{ "get_defined_functions", "bool $exclude_disabled = true", "array" },` |
|        - |  536 | `	{ "get_defined_vars", "", "array" },` |
|        - |  537 | `	{ "get_html_translation_table", "int $table = 0, int $flags = 11, string $encoding = 'UTF-8'", "array" },` |
|        - |  538 | `	{ "get_include_path", "", "string\|false" },` |
|        - |  539 | `	{ "get_included_files", "", "array" },` |
|        - |  540 | `	{ "get_object_vars", "object $object", "array" },` |
|        - |  541 | `	{ "get_parent_class", "object\|string $object_or_class = ?", "string\|false" },` |
|        - |  542 | `	{ "get_resource_id", "$resource", "int" },` |
|        - |  543 | `	{ "get_resource_type", "$resource", "string" },` |
|        - |  544 | `	{ "getcwd", "", "string\|false" },` |
|        - |  545 | `	{ "getdate", "?int $timestamp = NULL", "array" },` |
|        - |  546 | `	{ "getenv", "?string $name = NULL, bool $local_only = false", "array\|string\|false" },` |
|        - |  547 | `	{ "getmygid", "", "int\|false" },` |
|        - |  548 | `	{ "getmypid", "", "int\|false" },` |
|        - |  549 | `	{ "getmyuid", "", "int\|false" },` |
|        - |  550 | `	{ "getopt", "string $short_options, array $long_options = ?, &$rest_index = NULL", "array\|false" },` |
|        - |  551 | `	{ "getrandmax", "", "int" },` |
|        - |  552 | `	{ "gettimeofday", "bool $as_float = false", "array\|float" },` |
|        - |  553 | `	{ "gettype", "mixed $value", "string" },` |
|        - |  554 | `	{ "gmdate", "string $format, ?int $timestamp = NULL", "string" },` |
|        - |  555 | `	{ "gmmktime", "int $hour, ?int $minute = NULL, ?int $second = NULL, ?int $month = NULL, ?int $day = NULL, ?int $year = NULL", "int\|false" },` |
|        - |  556 | `	{ "hash", "string $algo, string $data, bool $binary = false, array $options = ?", "string" },` |
|        - |  557 | `	{ "hash_algos", "", "array" },` |
|        - |  558 | `	{ "hash_equals", "string $known_string, string $user_string", "bool" },` |
|        - |  559 | `	{ "hash_hmac", "string $algo, string $data, string $key, bool $binary = false", "string" },` |
|        - |  560 | `	{ "header", "string $header, bool $replace = true, int $response_code = 0", "void" },` |
|        - |  561 | `	{ "header_remove", "?string $name = NULL", "void" },` |
|        - |  562 | `	{ "headers_list", "", "array" },` |
|        - |  563 | `	{ "headers_sent", "&$filename = NULL, &$line = NULL", "bool" },` |
|        - |  564 | `	{ "hexdec", "string $hex_string", "int\|float" },` |
|        - |  565 | `	{ "html_entity_decode", "string $string, int $flags = 11, ?string $encoding = NULL", "string" },` |
|        - |  566 | `	{ "htmlentities", "string $string, int $flags = 11, ?string $encoding = NULL, bool $double_encode = true", "string" },` |
|        - |  567 | `	{ "htmlspecialchars", "string $string, int $flags = 11, ?string $encoding = NULL, bool $double_encode = true", "string" },` |
|        - |  568 | `	{ "htmlspecialchars_decode", "string $string, int $flags = 11", "string" },` |
|        - |  569 | `	{ "http_response_code", "int $response_code = 0", "int\|bool" },` |
|        - |  570 | `	{ "hypot", "float $x, float $y", "float" },` |
|        - |  571 | `	{ "idate", "string $format, ?int $timestamp = NULL", "int\|false" },` |
|        - |  572 | `	{ "implode", "array\|string $separator, ?array $array = NULL", "string" },` |
|        - |  573 | `	{ "in_array", "mixed $needle, array $haystack, bool $strict = false", "bool" },` |
|        - |  574 | `	{ "intdiv", "int $num1, int $num2", "int" },` |
|        - |  575 | `	{ "interface_exists", "string $interface, bool $autoload = true", "bool" },` |
|        - |  576 | `	{ "trait_exists", "string $trait, bool $autoload = true", "bool" },` |
|        - |  577 | `	{ "intval", "mixed $value, int $base = 10", "int" },` |
|        - |  578 | `	{ "is_a", "mixed $object_or_class, string $class, bool $allow_string = false", "bool" },` |
|        - |  579 | `	{ "is_array", "mixed $value", "bool" },` |
|        - |  580 | `	{ "is_bool", "mixed $value", "bool" },` |
|        - |  581 | `	{ "is_callable", "mixed $value, bool $syntax_only = false, &$callable_name = NULL", "bool" },` |
|        - |  582 | `	{ "is_dir", "string $filename", "bool" },` |
|        - |  583 | `	{ "is_double", "mixed $value", "bool" },` |
|        - |  584 | `	{ "is_executable", "string $filename", "bool" },` |
|        - |  585 | `	{ "is_file", "string $filename", "bool" },` |
|        - |  586 | `	{ "is_float", "mixed $value", "bool" },` |
|        - |  587 | `	{ "is_int", "mixed $value", "bool" },` |
|        - |  588 | `	{ "is_integer", "mixed $value", "bool" },` |
|        - |  589 | `	{ "is_link", "string $filename", "bool" },` |
|        - |  590 | `	{ "is_long", "mixed $value", "bool" },` |
|        - |  591 | `	{ "is_null", "mixed $value", "bool" },` |
|        - |  592 | `	{ "is_numeric", "mixed $value", "bool" },` |
|        - |  593 | `	{ "is_object", "mixed $value", "bool" },` |
|        - |  594 | `	{ "is_readable", "string $filename", "bool" },` |
|        - |  595 | `	{ "is_resource", "mixed $value", "bool" },` |
|        - |  596 | `	{ "is_scalar", "mixed $value", "bool" },` |
|        - |  597 | `	{ "is_string", "mixed $value", "bool" },` |
|        - |  598 | `	{ "is_subclass_of", "mixed $object_or_class, string $class, bool $allow_string = true", "bool" },` |
|        - |  599 | `	{ "is_writable", "string $filename", "bool" },` |
|        - |  600 | `	{ "iterator_apply", "Traversable $iterator, callable $callback, ?array $args = NULL", "int" },` |
|        - |  601 | `	{ "iterator_count", "Traversable\|array $iterator", "int" },` |
|        - |  602 | `	{ "iterator_to_array", "Traversable\|array $iterator, bool $preserve_keys = true", "array" },` |
|        - |  603 | `	{ "join", "array\|string $separator, ?array $array = NULL", "string" },` |
|        - |  604 | `	{ "json_decode", "string $json, ?bool $associative = NULL, int $depth = 512, int $flags = 0", "mixed" },` |
|        - |  605 | `	{ "json_encode", "mixed $value, int $flags = 0, int $depth = 512", "string\|false" },` |
|        - |  606 | `	{ "json_last_error", "", "int" },` |
|        - |  607 | `	{ "json_last_error_msg", "", "string" },` |
|        - |  608 | `	{ "json_validate", "string $json, int $depth = 512, int $flags = 0", "bool" },` |
|        - |  609 | `	{ "key", "object\|array $array", "string\|int\|null" },` |
|        - |  610 | `	{ "key_exists", "$key, array $array", "bool" },` |
|        - |  611 | `	{ "krsort", "array &$array, int $flags = 0", "true" },` |
|        - |  612 | `	{ "ksort", "array &$array, int $flags = 0", "true" },` |
|        - |  613 | `	{ "lcfirst", "string $string", "string" },` |
|        - |  614 | `	{ "levenshtein", "string $string1, string $string2, int $insertion_cost = 1, int $replacement_cost = 1, int $deletion_cost = 1", "int" },` |
|        - |  615 | `	{ "link", "string $target, string $link", "bool" },` |
|        - |  616 | `	{ "localtime", "?int $timestamp = NULL, bool $associative = false", "array" },` |
|        - |  617 | `	{ "log", "float $num, float $base = 2.718281828459045", "float" },` |
|        - |  618 | `	{ "log10", "float $num", "float" },` |
|        - |  619 | `	{ "lstat", "string $filename", "array\|false" },` |
|        - |  620 | `	{ "ltrim", "string $string, string $characters = ?", "string" },` |
|        - |  621 | `	{ "max", "mixed $value, mixed ...$values = ?", "mixed" },` |
|        - |  622 | `	{ "mb_chr", "int $codepoint, ?string $encoding = NULL", "string\|false" },` |
|        - |  623 | `	{ "mb_convert_encoding", "array\|string $string, string $to_encoding, array\|string\|null $from_encoding = NULL", "array\|string" },` |
|        - |  624 | `	{ "mb_ord", "string $string, ?string $encoding = NULL", "int\|false" },` |
|        - |  625 | `	{ "mb_strtolower", "string $string, ?string $encoding = NULL", "string" },` |
|        - |  626 | `	{ "mb_strtoupper", "string $string, ?string $encoding = NULL", "string" },` |
|        - |  627 | `	{ "md5", "string $string, bool $binary = false", "string" },` |
|        - |  628 | `	{ "md5_file", "string $filename, bool $binary = false", "string\|false" },` |
|        - |  629 | `	{ "method_exists", "$object_or_class, string $method", "bool" },` |
|        - |  630 | `	{ "memory_get_peak_usage", "bool $real_usage = false", "int" },` |
|        - |  631 | `	{ "memory_get_usage", "bool $real_usage = false", "int" },` |
|        - |  632 | `	{ "microtime", "bool $as_float = false", "string\|float" },` |
|        - |  633 | `	{ "min", "mixed $value, mixed ...$values = ?", "mixed" },` |
|        - |  634 | `	{ "mkdir", "string $directory, int $permissions = 511, bool $recursive = false, $context = NULL", "bool" },` |
|        - |  635 | `	{ "mktime", "int $hour, ?int $minute = NULL, ?int $second = NULL, ?int $month = NULL, ?int $day = NULL, ?int $year = NULL", "int\|false" },` |
|        - |  636 | `	{ "mt_getrandmax", "", "int" },` |
|        - |  637 | `	{ "mt_rand", "int $min = ?, int $max = ?", "int" },` |
|        - |  638 | `	{ "mt_srand", "?int $seed = NULL, int $mode = 0", "void" },` |
|        - |  639 | `	{ "natcasesort", "array &$array", "true" },` |
|        - |  640 | `	{ "natsort", "array &$array", "true" },` |
|        - |  641 | `	{ "next", "object\|array &$array", "mixed" },` |
|        - |  642 | `	{ "nl2br", "string $string, bool $use_xhtml = true", "string" },` |
|        - |  643 | `	{ "ob_clean", "", "bool" },` |
|        - |  644 | `	{ "ob_end_clean", "", "bool" },` |
|        - |  645 | `	{ "ob_end_flush", "", "bool" },` |
|        - |  646 | `	{ "ob_flush", "", "bool" },` |
|        - |  647 | `	{ "ob_get_clean", "", "string\|false" },` |
|        - |  648 | `	{ "ob_get_contents", "", "string\|false" },` |
|        - |  649 | `	{ "ob_get_flush", "", "string\|false" },` |
|        - |  650 | `	{ "ob_get_length", "", "int\|false" },` |
|        - |  651 | `	{ "ob_get_level", "", "int" },` |
|        - |  652 | `	{ "ob_implicit_flush", "bool $enable = true", "void" },` |
|        - |  653 | `	{ "ob_list_handlers", "", "array" },` |
|        - |  654 | `	{ "ob_start", "$callback = NULL, int $chunk_size = 0, int $flags = 112", "bool" },` |
|        - |  655 | `	{ "octdec", "string $octal_string", "int\|float" },` |
|        - |  656 | `	{ "opendir", "string $directory, $context = NULL", "" },` |
|        - |  657 | `	{ "ord", "string $character", "int" },` |
|        - |  658 | `	{ "parse_ini_file", "string $filename, bool $process_sections = false, int $scanner_mode = 0", "array\|false" },` |
|        - |  659 | `	{ "parse_ini_string", "string $ini_string, bool $process_sections = false, int $scanner_mode = 0", "array\|false" },` |
|        - |  660 | `	{ "parse_url", "string $url, int $component = -1", "array\|string\|int\|false\|null" },` |
|        - |  661 | `	{ "password_get_info", "string $hash", "array" },` |
|        - |  662 | `	{ "password_hash", "string $password, string\|int\|null $algo, array $options = ?", "string" },` |
|        - |  663 | `	{ "password_needs_rehash", "string $hash, string\|int\|null $algo, array $options = ?", "bool" },` |
|        - |  664 | `	{ "password_verify", "string $password, string $hash", "bool" },` |
|        - |  665 | `	{ "pathinfo", "string $path, int $flags = 15", "array\|string" },` |
|        - |  666 | `	{ "pclose", "$handle", "int" },` |
|        - |  667 | `	{ "php_sapi_name", "", "string\|false" },` |
|        - |  668 | `	{ "php_uname", "string $mode = 'a'", "string" },` |
|        - |  669 | `	{ "phpinfo", "int $flags = 4294967295", "true" },` |
|        - |  670 | `	{ "phpversion", "?string $extension = NULL", "string\|false" },` |
|        - |  671 | `	{ "pi", "", "float" },` |
|        - |  672 | `	{ "popen", "string $command, string $mode", "" },` |
|        - |  673 | `	{ "pos", "object\|array $array", "mixed" },` |
|        - |  674 | `	{ "pow", "mixed $num, mixed $exponent", "object\|int\|float" },` |
|        - |  675 | `	{ "preg_last_error", "", "int" },` |
|        - |  676 | `	{ "preg_last_error_msg", "", "string" },` |
|        - |  677 | `	{ "fsockopen", "string $hostname, int $port = -1, &$error_code = NULL, &$error_message = NULL, ?float $timeout = NULL", "resource\|false" },` |
|        - |  678 | `	{ "pfsockopen", "string $hostname, int $port = -1, &$error_code = NULL, &$error_message = NULL, ?float $timeout = NULL", "resource\|false" },` |
|        - |  679 | `	{ "stream_socket_client", "string $address, &$error_code = NULL, &$error_message = NULL, ?float $timeout = NULL, int $flags = 4, $context = NULL", "resource\|false" },` |
|        - |  680 | `	{ "preg_match", "string $pattern, string $subject, &$matches = NULL, int $flags = 0, int $offset = 0", "int\|false" },` |
|        - |  681 | `	{ "preg_match_all", "string $pattern, string $subject, &$matches = NULL, int $flags = 0, int $offset = 0", "int\|false" },` |
|        - |  682 | `	{ "preg_quote", "string $str, ?string $delimiter = NULL", "string" },` |
|        - |  683 | `	{ "preg_replace", "array\|string $pattern, array\|string $replacement, array\|string $subject, int $limit = -1, &$count = NULL", "array\|string\|null" },` |
|        - |  684 | `	{ "preg_replace_callback", "array\|string $pattern, callable $callback, array\|string $subject, int $limit = -1, &$count = NULL, int $flags = 0", "array\|string\|null" },` |
|        - |  685 | `	{ "preg_split", "string $pattern, string $subject, int $limit = -1, int $flags = 0", "array\|false" },` |
|        - |  686 | `	{ "prev", "object\|array &$array", "mixed" },` |
|        - |  687 | `	{ "print_r", "mixed $value, bool $return = false", "string\|true" },` |
|        - |  688 | `	{ "printf", "string $format, mixed ...$values = ?", "int" },` |
|        - |  689 | `	{ "property_exists", "$object_or_class, string $property", "bool" },` |
|        - |  690 | `	{ "putenv", "string $assignment", "bool" },` |
|        - |  691 | `	{ "quotemeta", "string $string", "string" },` |
|        - |  692 | `	{ "rand", "int $min = ?, int $max = ?", "int" },` |
|        - |  693 | `	{ "random_bytes", "int $length", "string" },` |
|        - |  694 | `	{ "random_int", "int $min, int $max", "int" },` |
|        - |  695 | `	{ "range", "string\|int\|float $start, string\|int\|float $end, int\|float $step = 1", "array" },` |
|        - |  696 | `	{ "rawurldecode", "string $string", "string" },` |
|        - |  697 | `	{ "rawurlencode", "string $string", "string" },` |
|        - |  698 | `	{ "readdir", "$dir_handle = NULL", "string\|false" },` |
|        - |  699 | `	{ "readfile", "string $filename, bool $use_include_path = false, $context = NULL", "int\|false" },` |
|        - |  700 | `	{ "realpath", "string $path", "string\|false" },` |
|        - |  701 | `	{ "register_shutdown_function", "callable $callback, mixed ...$args = ?", "void" },` |
|        - |  702 | `	{ "rename", "string $from, string $to, $context = NULL", "bool" },` |
|        - |  703 | `	{ "reset", "object\|array &$array", "mixed" },` |
|        - |  704 | `	{ "restore_error_handler", "", "true" },` |
|        - |  705 | `	{ "restore_exception_handler", "", "true" },` |
|        - |  706 | `	{ "rewind", "$stream", "bool" },` |
|        - |  707 | `	{ "rewinddir", "$dir_handle = NULL", "void" },` |
|        - |  708 | `	{ "rmdir", "string $directory, $context = NULL", "bool" },` |
|        - |  709 | `	{ "round", "int\|float $num, int $precision = 0, RoundingMode\|int $mode = ?", "float" },` |
|        - |  710 | `	{ "rsort", "array &$array, int $flags = 0", "true" },` |
|        - |  711 | `	{ "rtrim", "string $string, string $characters = ?", "string" },` |
|        - |  712 | `	{ "serialize", "mixed $value", "string" },` |
|        - |  713 | `	{ "set_error_handler", "?callable $callback, int $error_levels = 30719", "" },` |
|        - |  714 | `	{ "set_exception_handler", "?callable $callback", "" },` |
|        - |  715 | `	{ "get_error_handler", "", "?callable" },` |
|        - |  716 | `	{ "get_exception_handler", "", "?callable" },` |
|        - |  717 | `	{ "hrtime", "bool $as_number = false", "array\|int" },` |
|        - |  718 | `	{ "setcookie", "string $name, string $value = '', array\|int $expires_or_options = 0, string $path = '', string $domain = '', bool $secure = false, bool $httponly = false", "bool" },` |
|        - |  719 | `	{ "setrawcookie", "string $name, string $value = '', array\|int $expires_or_options = 0, string $path = '', string $domain = '', bool $secure = false, bool $httponly = false", "bool" },` |
|        - |  720 | `	{ "settype", "mixed &$var, string $type", "bool" },` |
|        - |  721 | `	{ "sha1", "string $string, bool $binary = false", "string" },` |
|        - |  722 | `	{ "sha1_file", "string $filename, bool $binary = false", "string\|false" },` |
|        - |  723 | `	{ "shuffle", "array &$array", "true" },` |
|        - |  724 | `	{ "similar_text", "string $string1, string $string2, &$percent = NULL", "int" },` |
|        - |  725 | `	{ "sin", "float $num", "float" },` |
|        - |  726 | `	{ "sinh", "float $num", "float" },` |
|        - |  727 | `	{ "sizeof", "Countable\|array $value, int $mode = 0", "int" },` |
|        - |  728 | `	{ "sleep", "int $seconds", "int" },` |
|        - |  729 | `	{ "sort", "array &$array, int $flags = 0", "true" },` |
|        - |  730 | `	{ "soundex", "string $string", "string" },` |
|        - |  731 | `	{ "spl_autoload", "string $class, ?string $file_extensions = NULL", "void" },` |
|        - |  732 | `	{ "spl_autoload_functions", "", "array" },` |
|        - |  733 | `	{ "spl_autoload_register", "?callable $callback = NULL, bool $throw = true, bool $prepend = false", "bool" },` |
|        - |  734 | `	{ "spl_autoload_unregister", "callable $callback", "bool" },` |
|        - |  735 | `	{ "spl_object_hash", "object $object", "string" },` |
|        - |  736 | `	{ "spl_object_id", "object $object", "int" },` |
|        - |  737 | `	{ "sprintf", "string $format, mixed ...$values = ?", "string" },` |
|        - |  738 | `	{ "sqrt", "float $num", "float" },` |
|        - |  739 | `	{ "srand", "?int $seed = NULL, int $mode = 0", "void" },` |
|        - |  740 | `	{ "stat", "string $filename", "array\|false" },` |
|        - |  741 | `	{ "str_contains", "string $haystack, string $needle", "bool" },` |
|        - |  742 | `	{ "str_ends_with", "string $haystack, string $needle", "bool" },` |
|        - |  743 | `	{ "str_getcsv", "string $string, string $separator = ',', string $enclosure = '\"', string $escape = '\\\\'", "array" },` |
|        - |  744 | `	{ "str_ireplace", "array\|string $search, array\|string $replace, array\|string $subject, &$count = NULL", "array\|string" },` |
|        - |  745 | `	{ "str_pad", "string $string, int $length, string $pad_string = ' ', int $pad_type = 1", "string" },` |
|        - |  746 | `	{ "str_repeat", "string $string, int $times", "string" },` |
|        - |  747 | `	{ "str_replace", "array\|string $search, array\|string $replace, array\|string $subject, &$count = NULL", "array\|string" },` |
|        - |  748 | `	{ "str_shuffle", "string $string", "string" },` |
|        - |  749 | `	{ "str_split", "string $string, int $length = 1", "array" },` |
|        - |  750 | `	{ "str_starts_with", "string $haystack, string $needle", "bool" },` |
|        - |  751 | `	{ "str_word_count", "string $string, int $format = 0, ?string $characters = NULL", "array\|int" },` |
|        - |  752 | `	{ "strcasecmp", "string $string1, string $string2", "int" },` |
|        - |  753 | `	{ "strchr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|        - |  754 | `	{ "strcmp", "string $string1, string $string2", "int" },` |
|        - |  755 | `	{ "strnatcasecmp", "string $string1, string $string2", "int" },` |
|        - |  756 | `	{ "strnatcmp", "string $string1, string $string2", "int" },` |
|        - |  757 | `	{ "strcoll", "string $string1, string $string2", "int" },` |
|        - |  758 | `	{ "strcspn", "string $string, string $characters, int $offset = 0, ?int $length = NULL", "int" },` |
|        - |  759 | `	{ "strip_tags", "string $string, array\|string\|null $allowed_tags = NULL", "string" },` |
|        - |  760 | `	{ "stripos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|        - |  761 | `	{ "stripslashes", "string $string", "string" },` |
|        - |  762 | `	{ "stristr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|        - |  763 | `	{ "strlen", "string $string", "int" },` |
|        - |  764 | `	{ "strncasecmp", "string $string1, string $string2, int $length", "int" },` |
|        - |  765 | `	{ "strncmp", "string $string1, string $string2, int $length", "int" },` |
|        - |  766 | `	{ "strpbrk", "string $string, string $characters", "string\|false" },` |
|        - |  767 | `	{ "strpos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|        - |  768 | `	{ "strrchr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|        - |  769 | `	{ "strrev", "string $string", "string" },` |
|        - |  770 | `	{ "strripos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|        - |  771 | `	{ "strrpos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|        - |  772 | `	{ "strspn", "string $string, string $characters, int $offset = 0, ?int $length = NULL", "int" },` |
|        - |  773 | `	{ "strstr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|        - |  774 | `	{ "strtok", "string $string, ?string $token = NULL", "string\|false" },` |
|        - |  775 | `	{ "strtolower", "string $string", "string" },` |
|        - |  776 | `	{ "strtoupper", "string $string", "string" },` |
|        - |  777 | `	{ "strtr", "string $string, array\|string $from, ?string $to = NULL", "string" },` |
|        - |  778 | `	{ "strval", "mixed $value", "string" },` |
|        - |  779 | `	{ "substr", "string $string, int $offset, ?int $length = NULL", "string" },` |
|        - |  780 | `	{ "substr_compare", "string $haystack, string $needle, int $offset, ?int $length = NULL, bool $case_insensitive = false", "int" },` |
|        - |  781 | `	{ "substr_count", "string $haystack, string $needle, int $offset = 0, ?int $length = NULL", "int" },` |
|        - |  782 | `	{ "substr_replace", "array\|string $string, array\|string $replace, array\|int $offset, array\|int\|null $length = NULL", "array\|string" },` |
|        - |  783 | `	{ "symlink", "string $target, string $link", "bool" },` |
|        - |  784 | `	{ "sys_get_temp_dir", "", "string" },` |
|        - |  785 | `	{ "tan", "float $num", "float" },` |
|        - |  786 | `	{ "tanh", "float $num", "float" },` |
|        - |  787 | `	{ "time", "", "int" },` |
|        - |  788 | `	{ "touch", "string $filename, ?int $mtime = NULL, ?int $atime = NULL", "bool" },` |
|        - |  789 | `	{ "trigger_error", "string $message, int $error_level = 1024", "true" },` |
|        - |  790 | `	{ "trim", "string $string, string $characters = ?", "string" },` |
|        - |  791 | `	{ "uasort", "array &$array, callable $callback", "true" },` |
|        - |  792 | `	{ "ucfirst", "string $string", "string" },` |
|        - |  793 | `	{ "ucwords", "string $string, string $separators = ?", "string" },` |
|        - |  794 | `	{ "uksort", "array &$array, callable $callback", "true" },` |
|        - |  795 | `	{ "umask", "?int $mask = NULL", "int" },` |
|        - |  796 | `	{ "uniqid", "string $prefix = '', bool $more_entropy = false", "string" },` |
|        - |  797 | `	{ "unlink", "string $filename, $context = NULL", "bool" },` |
|        - |  798 | `	{ "unserialize", "string $data, array $options = ?", "mixed" },` |
|        - |  799 | `	{ "urldecode", "string $string", "string" },` |
|        - |  800 | `	{ "urlencode", "string $string", "string" },` |
|        - |  801 | `	{ "user_error", "string $message, int $error_level = 1024", "true" },` |
|        - |  802 | `	{ "usleep", "int $microseconds", "void" },` |
|        - |  803 | `	{ "usort", "array &$array, callable $callback", "true" },` |
|        - |  804 | `	{ "var_dump", "mixed $value, mixed ...$values = ?", "void" },` |
|        - |  805 | `	{ "var_export", "mixed $value, bool $return = false", "?string" },` |
|        - |  806 | `	{ "vfprintf", "$stream, string $format, array $values", "int" },` |
|        - |  807 | `	{ "vprintf", "string $format, array $values", "int" },` |
|        - |  808 | `	{ "vsprintf", "string $format, array $values", "string" },` |
|        - |  809 | `	{ "wordwrap", "string $string, int $width = 75, string $break = ?, bool $cut_long_words = false", "string" },` |
|        - |  810 | `	{ "zip_close", "$zip", "void" },` |
|        - |  811 | `	{ "zip_entry_close", "$zip_entry", "bool" },` |
|        - |  812 | `	{ "zip_entry_compressedsize", "$zip_entry", "int\|false" },` |
|        - |  813 | `	{ "zip_entry_compressionmethod", "$zip_entry", "string\|false" },` |
|        - |  814 | `	{ "zip_entry_filesize", "$zip_entry", "int\|false" },` |
|        - |  815 | `	{ "zip_entry_name", "$zip_entry", "string\|false" },` |
|        - |  816 | `	{ "zip_entry_open", "$zip_dp, $zip_entry, string $mode = 'rb'", "bool" },` |
|        - |  817 | `	{ "zip_entry_read", "$zip_entry, int $len = 1024", "string\|false" },` |
|        - |  818 | `	{ "zip_open", "string $filename", "" },` |
|        - |  819 | `	{ "zip_read", "$zip", "" },` |
|        - |  820 | `};` |
|        - |  821 | `/*` |
|        - |  822 | ` * Stamp the signature strings onto the registered host functions.` |
|        - |  823 | ` * Runs once at PH7_VmMakeReady, after the builtins are registered.` |
|        - |  824 | ` */` |
|        - |  825 | `/*` |
|        - |  826 | ` * Derive the minimum arity from a builtin's declared signature: a parameter is` |
|        - |  827 | ` * optional when it carries a default ("int $length = 76") or is variadic` |
|        - |  828 | ` * ("...$rest"), so the minimum is the count of the parameters that are neither,` |
|        - |  829 | ` * and the ZPP wording is "at least" as soon as one optional/variadic exists.` |
|        - |  830 | ` *` |
|        - |  831 | ` * This makes aBuiltinSig[] the single source of truth for arity — the curated` |
|        - |  832 | ` * aBuiltinArity[] table above stays as an OVERRIDE for the handful of builtins` |
|        - |  833 | ` * php reports differently from their own signature (e.g. strtr(), whose 3-arg` |
|        - |  834 | ` * form still reports "expects exactly 2", and the array_udiff() family, whose` |
|        - |  835 | ` * variadic tail hides a second required argument). Verified against php 8.5.7` |
|        - |  836 | ` * for all 462 signed builtins: 458 derive exactly, 4 are overridden.` |
|        - |  837 | ` */` |
|  1875860 |  838 | `PH7_PRIVATE void VmDeriveArityFromSig(const char *zSig,sxi16 *pnMin,sxu8 *pbAtLeast,sxi16 *pnMax,sxu8 *pbHasMax)` |
|        5 |  839 | `{` |
|  1875865 |  840 | `	const char *zCur = zSig;` |
|  1875865 |  841 | `	int nMin = 0, bAtLeast = 0, bSeen = 0, bOptional = 0;` |
|  1875865 |  842 | `	int nTotal = 0, bVariadic = 0;` |
| 27969284 |  843 | `	for(;;){` |
| 57542469 |  844 | `		if( zCur[0] == '\0' \|\| zCur[0] == ',' ){` |
|  3479761 |  845 | `			if( bSeen ){` |
|  3223097 |  846 | `				nTotal++;` |
|  3223097 |  847 | `				if( bOptional ){` |
|  1260873 |  848 | `					bAtLeast = 1;` |
|   630439 |  849 | `				}else{` |
|  1962229 |  850 | `					nMin++;` |
|        - |  851 | `				}` |
|  1611546 |  852 | `			}` |
|  3479761 |  853 | `			if( zCur[0] == '\0' ){` |
|  1875865 |  854 | `				break;` |
|        - |  855 | `			}` |
|  1603901 |  856 | `			bSeen = bOptional = 0;` |
|  1603901 |  857 | `			zCur++;` |
|  1603901 |  858 | `			continue;` |
|        - |  859 | `		}` |
| 54062713 |  860 | `		if( zCur[0] != ' ' ){` |
| 46999965 |  861 | `			bSeen = 1;` |
| 23499980 |  862 | `		}` |
| 54062713 |  863 | `		if( zCur[0] == '=' \|\| (zCur[0] == '.' && zCur[1] == '.' && zCur[2] == '.') ){` |
|  1343949 |  864 | `			bOptional = 1;` |
|   671972 |  865 | `		}` |
| 54062713 |  866 | `		if( zCur[0] == '.' && zCur[1] == '.' && zCur[2] == '.' ){` |
|    87609 |  867 | `			bVariadic = 1;` |
|    43802 |  868 | `		}` |
| 54062713 |  869 | `		zCur++;` |
|        5 |  870 | `	}` |
|  1875865 |  871 | `	*pnMin = (sxi16)nMin;` |
|  1875865 |  872 | `	*pbAtLeast = (sxu8)bAtLeast;` |
|        - |  873 | `	/* php enforces a MAXIMUM too ("expects at most 1 argument, 2 given"); a` |
|        - |  874 | `	 * variadic tail means there is none. The parameter COUNT is the maximum,` |
|        - |  875 | `	 * whether or not the parameters carry defaults. */` |
|  1875865 |  876 | `	*pnMax = (sxi16)nTotal;` |
|  1875865 |  877 | `	*pbHasMax = (sxu8)(bVariadic ? 0 : 1);` |
|  1875865 |  878 | `}` |
|        - |  879 | `/*` |
|        - |  880 | ` * Does the declared type list (e.g. "array\|string", "?int", "callable") contain` |
|        - |  881 | ` * the given token? Compares against each '\|'-separated alternative, ignoring a` |
|        - |  882 | ` * leading nullable '?'.` |
|        - |  883 | ` */` |
|  1716770 |  884 | `static int VmSigTypeHas(const char *zType,int nType,const char *zTok)` |
|        5 |  885 | `{` |
|  1716775 |  886 | `	int nTok = (int)SyStrlen(zTok);` |
|  1716775 |  887 | `	int i = 0;` |
|  1716775 |  888 | `	if( zType[0] == '?' ){` |
|   250949 |  889 | `		zType++;` |
|   250949 |  890 | `		nType--;` |
|   125472 |  891 | `	}` |
|  3433614 |  892 | `	while( i < nType ){` |
|  1871345 |  893 | `		int j = i;` |
| 10873775 |  894 | `		while( j < nType && zType[j] != '\|' ){` |
|  9002435 |  895 | `			j++;` |
|        5 |  896 | `		}` |
|  1871345 |  897 | `		if( j - i == nTok && SyMemcmp(&zType[i],zTok,(sxu32)nTok) == 0 ){` |
|   154506 |  898 | `			return 1;` |
|        - |  899 | `		}` |
|  1716844 |  900 | `		i = j + 1;` |
|        5 |  901 | `	}` |
|  1562274 |  902 | `	return 0;` |
|   859332 |  903 | `}` |
|        - |  904 | `/*` |
|        - |  905 | ` * Does the declared type list name a CLASS (anything that is not one of php's` |
|        - |  906 | ` * builtin type keywords)? A class-typed parameter accepts an object, so it must` |
|        - |  907 | ` * not be rejected by the array/object/resource screen below.` |
|        - |  908 | ` */` |
|      328 |  909 | `static int VmSigTypeHasClass(const char *zType,int nType)` |
|        5 |  910 | `{` |
|        - |  911 | `	static const char *azBuiltin[] = {` |
|        - |  912 | `		"int","float","string","bool","array","object","callable","iterable",` |
|        - |  913 | `		"mixed","null","void","resource","false","true","never","self","static"` |
|        - |  914 | `	};` |
|      333 |  915 | `	int i = 0;` |
|      333 |  916 | `	if( zType[0] == '?' ){` |
|        7 |  917 | `		zType++;` |
|        7 |  918 | `		nType--;` |
|        3 |  919 | `	}` |
|      479 |  920 | `	while( i < nType ){` |
|      347 |  921 | `		int j = i, k, bKnown = 0;` |
|     2977 |  922 | `		while( j < nType && zType[j] != '\|' ){` |
|     2635 |  923 | `			j++;` |
|        5 |  924 | `		}` |
|     3983 |  925 | `		for( k = 0 ; k < (int)SX_ARRAYSIZE(azBuiltin) ; ++k ){` |
|     3787 |  926 | `			int nB = (int)SyStrlen(azBuiltin[k]);` |
|     3787 |  927 | `			if( j - i == nB && SyMemcmp(&zType[i],azBuiltin[k],(sxu32)nB) == 0 ){` |
|      150 |  928 | `				bKnown = 1;` |
|      150 |  929 | `				break;` |
|        - |  930 | `			}` |
|     1823 |  931 | `		}` |
|      347 |  932 | `		if( !bKnown && j > i ){` |
|      201 |  933 | `			return 1;` |
|        - |  934 | `		}` |
|      150 |  935 | `		i = j + 1;` |
|        4 |  936 | `	}` |
|      136 |  937 | `	return 0;` |
|      169 |  938 | `}` |
|        - |  939 | `/*` |
|        - |  940 | ` * php's ", X given" tail: like ph7_type_name() but an object reports its CLASS,` |
|        - |  941 | ` * which is what php prints in a TypeError.` |
|        - |  942 | ` */` |
|       62 |  943 | `static const char * VmArgTypeName(ph7_value *pVal)` |
|        4 |  944 | `{` |
|       66 |  945 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       66 |  946 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       66 |  947 | `		if( pInst && pInst->pClass ){` |
|       66 |  948 | `			return pInst->pClass->sName.zString;` |
|        - |  949 | `		}` |
|      ! 0 |  950 | `	}` |
|      ! 0 |  951 | `	return ph7_type_name(pVal);` |
|       35 |  952 | `}` |
|        - |  953 | `/*` |
|        - |  954 | `` * Does pArg satisfy a `string` parameter? The rule VmEnforceBuiltinArgTypes()`` |
|        - |  955 | ` * below applies, factored out so a builtin that words its own overload dispatch` |
|        - |  956 | ` * (strtr(), whose expected type depends on the ARITY and so cannot be spelled in` |
|        - |  957 | ` * one signature) decides identically instead of forking the logic. An array never` |
|        - |  958 | ` * satisfies one; an object does only through __toString(); a resource does not;` |
|        - |  959 | ` * null does under php, with a deprecation, but not under PHL's §10 null-strictness` |
|        - |  960 | ` * policy — the screen and this helper both report it as a mismatch.` |
|        - |  961 | ` */` |
|    37124 |  962 | `PH7_PRIVATE int PH7_ArgSatisfiesString(ph7_value *pArg)` |
|        5 |  963 | `{` |
|    37129 |  964 | `	if( (pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_NULL\|MEMOBJ_RES)) != 0 ){` |
|       14 |  965 | `		return 0;` |
|        - |  966 | `	}` |
|    37117 |  967 | `	if( (pArg->iFlags & MEMOBJ_OBJ) != 0 ){` |
|      122 |  968 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|      122 |  969 | `		return pInst && PH7_ClassExtractMethod(pInst->pClass,"__toString",` |
|       59 |  970 | `			sizeof("__toString")-1) != 0;` |
|        - |  971 | `	}` |
|    36999 |  972 | `	return 1;` |
|    18567 |  973 | `}` |
|        - |  974 | `/*` |
|        - |  975 | ` * PHP-8 ZPP type enforcement for host functions, driven by the aBuiltinSig[]` |
|        - |  976 | ` * declaration (band A #7). Screens only the arguments that php can NEVER coerce` |
|        - |  977 | ` * into a declared scalar parameter — arrays, resources, and objects without a` |
|        - |  978 | ` * __toString() — and throws the catchable TypeError php throws, before the C` |
|        - |  979 | ` * routine runs. Without this an array argument reached the builtin and was` |
|        - |  980 | ` * stringified to the literal "Array" (strrev(['a']) returned "yarrA").` |
|        - |  981 | ` *` |
|        - |  982 | ` * Deliberately narrow: scalar-to-scalar coercion (and its deprecations) stays` |
|        - |  983 | ` * with the per-builtin ZPP helpers. Those emit E_DEPRECATED, which does NOT` |
|        - |  984 | ` * abort the call, so a central copy would double-fire — the trap that sank the` |
|        - |  985 | ` * first central-ZPP attempt. A TypeError aborts, so there is nothing to double.` |
|        - |  986 | ` */` |
|  4071029 |  987 | `PH7_PRIVATE sxi32 VmEnforceBuiltinArgTypes(` |
|        - |  988 | `	ph7_context *pCtx,    /* Call context (for the throw) */` |
|        - |  989 | `	ph7_user_func *pFunc, /* Callee */` |
|        - |  990 | `	int nGiven,           /* Argument count */` |
|        - |  991 | `	ph7_value **apArg     /* Arguments */` |
|        - |  992 | `	)` |
|        5 |  993 | `{` |
|        - |  994 | `	/*` |
|        - |  995 | `	 * Builtins whose own argument check is php-exact and VALUE-based rather than` |
|        - |  996 | `	 * type-based must not be pre-empted here, or their message is lost. Same rule` |
|        - |  997 | `	 * the aBuiltinArity[] table follows: a builtin that already says what php says` |
|        - |  998 | `	 * stays off the shared screen. get_class_vars() takes any stringifiable value` |
|        - |  999 | `	 * and reports "must be a valid class name, Array given".` |
|        - | 1000 | `	 *` |
|        - | 1001 | `	 * strtr() is here for a structural reason: php declares it as two OVERLOADS` |
|        - | 1002 | `	 * dispatched on arity — strtr(string, array) and strtr(string, string, string)` |
|        - | 1003 | `` 	 * — so the expected type of $from is `array` with two arguments and `string` `` |
|        - | 1004 | ``	 * with three. One signature cannot say that (the stub's `array\|string` is the`` |
|        - | 1005 | `	 * union of the two, which is the wording php never uses), so the builtin does` |
|        - | 1006 | `	 * its own dispatch through PH7_ArgSatisfiesString(), the same rule as here.` |
|        - | 1007 | `	 *` |
|        - | 1008 | ``	 * implode() is the same structure: `array\|string $separator` is what the two`` |
|        - | 1009 | `	 * ARITIES accept between them, never what one call can use. Once an $array` |
|        - | 1010 | `	 * argument is present php has resolved the overload and reports` |
|        - | 1011 | ``	 * `must be of type string`, and with the array in position #1 it reports`` |
|        - | 1012 | ``	 * `must be of type string, array given` against #1 rather than a #2 error.`` |
|        - | 1013 | `	 * PH7_builtin_implode words all of that itself.` |
|        - | 1014 | `	 *` |
|        - | 1015 | `	 * Its alias join() is here for the same reason and then some: php 8.5 does not` |
|        - | 1016 | `	 * word the two the same, so the builtin reproduces BOTH orders keyed on the` |
|        - | 1017 | `	 * invoked name (see PH7_builtin_implode's header for the value-for-value` |
|        - | 1018 | `	 * table against 8.5.8). php's own asymmetry between a target and its alias,` |
|        - | 1019 | `	 * reproduced rather than smoothed over — parity is binding (§10).` |
|        - | 1020 | `	 */` |
|        - | 1021 | `	static const char *azSelfChecked[] = { "get_class_vars", "strtr", "implode", "join" };` |
|  4071034 | 1022 | `	const char *zSig = pFunc->zSig;` |
|        - | 1023 | `	const char *zCur, *zEnd;` |
|  4071034 | 1024 | `	int iArg = 0;` |
|  4071034 | 1025 | `	if( zSig == 0 ){` |
|  3089949 | 1026 | `		return SXRET_OK;` |
|        - | 1027 | `	}` |
|  4831448 | 1028 | `	for( iArg = 0 ; iArg < (int)SX_ARRAYSIZE(azSelfChecked) ; ++iArg ){` |
|  5833924 | 1029 | `		if( SyStrncmp(pFunc->sName.zString,azSelfChecked[iArg],` |
|  5833924 | 1030 | `			(sxu32)SyStrlen(azSelfChecked[iArg])) == 0` |
|  1965100 | 1031 | `		 && pFunc->sName.nByte == SyStrlen(azSelfChecked[iArg]) ){` |
|    36947 | 1032 | `			return SXRET_OK;` |
|        - | 1033 | `		}` |
|  1928158 | 1034 | `	}` |
|   944148 | 1035 | `	iArg = 0;` |
|   944148 | 1036 | `	zCur = zSig;` |
|   944148 | 1037 | `	zEnd = &zSig[SyStrlen(zSig)];` |
|  2656524 | 1038 | `	while( zCur < zEnd && iArg < nGiven ){` |
|        - | 1039 | `		const char *zType, *zName, *zStop;` |
|        - | 1040 | `		int nType, nName;` |
|        - | 1041 | `		ph7_value *pArg;` |
|        - | 1042 | `		/* Parameter = "<type> $<name>[ = <default>]"; the type is whatever` |
|        - | 1043 | `		 * precedes the '$', and an empty type means "untyped" (no screen). */` |
|  2511708 | 1044 | `		while( zCur < zEnd && zCur[0] == ' ' ){` |
|   796200 | 1045 | `			zCur++;` |
|        5 | 1046 | `		}` |
|  1715513 | 1047 | `		zStop = zCur;` |
| 26730049 | 1048 | `		while( zStop < zEnd && zStop[0] != ',' ){` |
| 25014541 | 1049 | `			zStop++;` |
|        5 | 1050 | `		}` |
|  1715513 | 1051 | `		zName = zCur;` |
| 12509321 | 1052 | `		while( zName < zStop && zName[0] != '$' ){` |
| 10793813 | 1053 | `			zName++;` |
|        5 | 1054 | `		}` |
|  1715513 | 1055 | `		if( zName >= zStop ){` |
|       45 | 1056 | `			break; /* malformed / no parameter name — stop screening */` |
|        - | 1057 | `		}` |
|  1715469 | 1058 | `		if( zName >= zCur + 3 && SyMemcmp(zName - 3,"...",3) == 0 ){` |
|     2867 | 1059 | `			break; /* variadic tail: stop (its type applies to the rest) */` |
|        - | 1060 | `		}` |
|  1712607 | 1061 | `		zType = zCur;` |
|  1712607 | 1062 | `		nType = (int)(zName - zCur);` |
|        - | 1063 | `		/* Trim the trailing spaces and the by-ref marker of "array &$array" */` |
|  4239009 | 1064 | `		while( nType > 0 && (zType[nType-1] == ' ' \|\| zType[nType-1] == '&') ){` |
|  1669165 | 1065 | `			nType--;` |
|        5 | 1066 | `		}` |
|  1712607 | 1067 | `		zName++; /* skip '$' */` |
|  1712607 | 1068 | `		nName = 0;` |
| 12284043 | 1069 | `		while( &zName[nName] < zStop && zName[nName] != ' ' && zName[nName] != '=' ){` |
| 10571441 | 1070 | `			nName++;` |
|        5 | 1071 | `		}` |
|  1712607 | 1072 | `		pArg = apArg[iArg];` |
|  1712607 | 1073 | `		if( nType > 0 && !VmSigTypeHas(zType,nType,"mixed") ){` |
|  1559504 | 1074 | `			const char *zGiven = 0;` |
|  1559504 | 1075 | `			if( (pArg->iFlags & MEMOBJ_HASHMAP) != 0 ){` |
|    45560 | 1076 | `				if( !VmSigTypeHas(zType,nType,"array")` |
|    22892 | 1077 | `				 && !VmSigTypeHas(zType,nType,"iterable")` |
|      229 | 1078 | `				 && !VmSigTypeHas(zType,nType,"callable") ){` |
|      119 | 1079 | `					zGiven = "array";` |
|       62 | 1080 | `				}` |
|  1536724 | 1081 | `			}else if( (pArg->iFlags & MEMOBJ_OBJ) != 0 ){` |
|     2878 | 1082 | `				if( !VmSigTypeHas(zType,nType,"object")` |
|     1878 | 1083 | `				 && !VmSigTypeHas(zType,nType,"iterable")` |
|      878 | 1084 | `				 && !VmSigTypeHas(zType,nType,"callable")` |
|      607 | 1085 | `				 && !VmSigTypeHasClass(zType,nType) ){` |
|        - | 1086 | `					/* An object with __toString() still satisfies a string` |
|        - | 1087 | `					 * parameter in weak mode — php coerces it. */` |
|      188 | 1088 | `					int bStringable = VmSigTypeHas(zType,nType,"string")` |
|      130 | 1089 | `						&& PH7_ArgSatisfiesString(pArg);` |
|      134 | 1090 | `					if( !bStringable ){` |
|       66 | 1091 | `						zGiven = VmArgTypeName(pArg);` |
|       31 | 1092 | `					}` |
|       70 | 1093 | `				}` |
|  1512505 | 1094 | `			}else if( (pArg->iFlags & MEMOBJ_NULL) != 0 ){` |
|        - | 1095 | `				/* php only DEPRECATES null for a non-nullable parameter; PHL rejects` |
|        - | 1096 | `				 * it (scope policy). A leading '?' or an explicit "null" arm in a` |
|        - | 1097 | `				 * union declares the parameter nullable. A "callable" parameter is` |
|        - | 1098 | `				 * left to the builtin's own callback check, which words the failure` |
|        - | 1099 | `				 * php's way ("must be a valid callback, no array or string given") —` |
|        - | 1100 | `				 * the same reason get_class_vars() sits on azSelfChecked[]. */` |
|     5150 | 1101 | `				if( zType[0] != '?'` |
|     2604 | 1102 | `				 && !VmSigTypeHas(zType,nType,"null")` |
|       60 | 1103 | `				 && !VmSigTypeHas(zType,nType,"callable") ){` |
|       52 | 1104 | `					zGiven = "null";` |
|       28 | 1105 | `				}` |
|  1508491 | 1106 | `			}else if( (pArg->iFlags & MEMOBJ_RES) != 0 ){` |
|        - | 1107 | `				/* A class-typed parameter also accepts a resource: several handles php 8` |
|        - | 1108 | `				 * models as objects are still resources here (xml_*'s XMLParser is the` |
|        - | 1109 | `				 * one the signatures already declare php-8-style, for reflection). The` |
|        - | 1110 | `				 * screen would otherwise reject the engine's own parser handle. Recorded` |
|        - | 1111 | `				 * as a divergence in NEWPLAN §7 — it goes away when those handles become` |
|        - | 1112 | `				 * real objects. */` |
|        2 | 1113 | `				if( !VmSigTypeHas(zType,nType,"resource")` |
|        3 | 1114 | `				 && !VmSigTypeHasClass(zType,nType) ){` |
|        3 | 1115 | `					zGiven = "resource";` |
|        1 | 1116 | `				}` |
|        1 | 1117 | `			}` |
|  1559504 | 1118 | `			if( zGiven ){` |
|      344 | 1119 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|        - | 1120 | `					"%z(): Argument #%d ($%.*s) must be of type %.*s, %s given",` |
|      113 | 1121 | `					&pFunc->sName,iArg + 1,nName,zName,nType,zType,zGiven);` |
|        - | 1122 | `			}` |
|   780428 | 1123 | `		}` |
|  1712381 | 1124 | `		zCur = (zStop < zEnd) ? zStop + 1 : zEnd;` |
|  1712381 | 1125 | `		iArg++;` |
|        5 | 1126 | `	}` |
|   943922 | 1127 | `	return SXRET_OK;` |
|  2036263 | 1128 | `}` |
|        - | 1129 | `/*` |
|        - | 1130 | ` * Builtins whose accepted arity is NOT a contiguous range, so the signature` |
|        - | 1131 | ` * cannot express it and the central too-many-arguments check must stay out of` |
|        - | 1132 | ` * the way: rand()/mt_rand() take 0 OR 2 arguments (never 1), and php words the` |
|        - | 1133 | ` * violation "expects exactly 2 arguments, 3 given" from their own check rather` |
|        - | 1134 | ` * than the ZPP "at most". They validate themselves; leaving bHasMaxArg at 0` |
|        - | 1135 | ` * keeps their message php-faithful.` |
|        - | 1136 | ` */` |
|  1776244 | 1137 | `static int VmBuiltinSelfValidatesArity(const char *zName)` |
|        5 | 1138 | `{` |
|        - | 1139 | `	static const char *const azSelf[] = { "rand", "mt_rand" };` |
|        - | 1140 | `	sxu32 i;` |
|  5316869 | 1141 | `	for( i = 0 ; i < SX_ARRAYSIZE(azSelf) ; i++ ){` |
|  3548537 | 1142 | `		sxu32 nSelf = SyStrlen(azSelf[i]);` |
|  3548537 | 1143 | `		if( SyStrlen(zName) == nSelf && SyStrncmp(zName,azSelf[i],nSelf) == 0 ){` |
|     7917 | 1144 | `			return 1;` |
|        - | 1145 | `		}` |
|  1770315 | 1146 | `	}` |
|  1768337 | 1147 | `	return 0;` |
|   888127 | 1148 | `}` |
|        - | 1149 | `/*` |
|        - | 1150 | ` * D1: derive a by-reference position bitmask from a php-style signature string.` |
|        - | 1151 | `` * Bit N is set when positional parameter N is declared by-reference (a `&` appears`` |
|        - | 1152 | `` * anywhere in that comma-separated parameter, e.g. `&$matches`, `&...$vars`). Only`` |
|        - | 1153 | ` * the first 31 positions are representable; a by-ref parameter past that is rare` |
|        - | 1154 | ` * for a builtin and simply not tracked here. The scan mirrors VmDeriveArityFromSig.` |
|        - | 1155 | ` */` |
|  1875860 | 1156 | `PH7_PRIVATE sxu32 VmDeriveByRefMaskFromSig(const char *zSig)` |
|        5 | 1157 | `{` |
|  1875865 | 1158 | `	sxu32 mask = 0;` |
|  1875865 | 1159 | `	int n = 0;       /* current parameter index */` |
|  1875865 | 1160 | `	int bSeen = 0;   /* current parameter has non-space content */` |
|  1875865 | 1161 | ``	int bRef = 0;    /* current parameter carries a by-ref `&` */`` |
|  1875865 | 1162 | `	const char *zCur = zSig;` |
| 27969284 | 1163 | `	for(;;){` |
| 57542469 | 1164 | `		if( zCur[0] == '\0' \|\| zCur[0] == ',' ){` |
|  3479761 | 1165 | `			if( bSeen ){` |
|  3223097 | 1166 | `				if( bRef && n < 31 ){` |
|   166157 | 1167 | `					mask \|= (1u << n);` |
|    83076 | 1168 | `				}` |
|  3223097 | 1169 | `				n++;` |
|  1611546 | 1170 | `			}` |
|  3479761 | 1171 | `			if( zCur[0] == '\0' ){` |
|  1875865 | 1172 | `				break;` |
|        - | 1173 | `			}` |
|  1603901 | 1174 | `			bSeen = bRef = 0;` |
|  1603901 | 1175 | `			zCur++;` |
|  1603901 | 1176 | `			continue;` |
|        - | 1177 | `		}` |
| 54062713 | 1178 | `		if( zCur[0] != ' ' ){` |
| 46999965 | 1179 | `			bSeen = 1;` |
| 23499980 | 1180 | `		}` |
| 54062713 | 1181 | `		if( zCur[0] == '&' ){` |
|   166157 | 1182 | `			bRef = 1;` |
|    83076 | 1183 | `		}` |
| 54062713 | 1184 | `		zCur++;` |
|        5 | 1185 | `	}` |
|  1875865 | 1186 | `	return mask;` |
|        5 | 1187 | `}` |
|     3956 | 1188 | `PH7_PRIVATE void VmSetBuiltinSignatures(ph7_vm *pVm)` |
|        5 | 1189 | `{` |
|        - | 1190 | `	sxu32 n;` |
|  1827677 | 1191 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinSig) ; n++ ){` |
|  2735579 | 1192 | `		SyHashEntry *pEntry = SyHashGet(&pVm->hHostFunction,` |
|  1823716 | 1193 | `			(const void *)aBuiltinSig[n].zName,(sxu32)SyStrlen(aBuiltinSig[n].zName));` |
|  1823721 | 1194 | `		if( pEntry ){` |
|  1776249 | 1195 | `			ph7_user_func *pFunc = (ph7_user_func *)pEntry->pUserData;` |
|  1776249 | 1196 | `			sxi16 nMin = 0, nMax = 0;` |
|  1776249 | 1197 | `			sxu8 bAtLeast = 0, bHasMax = 0;` |
|  1776249 | 1198 | `			pFunc->zSig = aBuiltinSig[n].zSig;` |
|  1776249 | 1199 | `			pFunc->zRet = aBuiltinSig[n].zRet[0] ? aBuiltinSig[n].zRet : 0;` |
|  1776249 | 1200 | `			pFunc->nByRefMask = VmDeriveByRefMaskFromSig(pFunc->zSig);` |
|  1776249 | 1201 | `			VmDeriveArityFromSig(pFunc->zSig,&nMin,&bAtLeast,&nMax,&bHasMax);` |
|        - | 1202 | `			/* The MAXIMUM always comes from the signature: the curated override` |
|        - | 1203 | `			 * table speaks only to the minimum (and its wording). */` |
|  1776249 | 1204 | `			pFunc->nMaxArg = nMax;` |
|  1776249 | 1205 | `			pFunc->bHasMaxArg = (sxu8)(VmBuiltinSelfValidatesArity(aBuiltinSig[n].zName) ? 0 : bHasMax);` |
|  1776249 | 1206 | `			if( pFunc->nMinArg < 1 ){` |
|        - | 1207 | `				/* VmSetBuiltinArity() ran first: a non-zero minimum here means the` |
|        - | 1208 | `				 * curated override already spoke for this builtin, so leave it. */` |
|   747689 | 1209 | `				pFunc->nMinArg = nMin;` |
|   747689 | 1210 | `				pFunc->bAtLeast = bAtLeast;` |
|   373842 | 1211 | `			}` |
|   888122 | 1212 | `		}` |
|   911863 | 1213 | `	}` |
|     3961 | 1214 | `}` |
|        - | 1215 | `/*` |
|        - | 1216 | ` * Signature lookup by function name, for the reflection layer: embedded-PHP` |
|        - | 1217 | ` * builtins (max/min/Exception methods...) are ph7_vm_func instances, not` |
|        - | 1218 | ` * host functions, so they miss the VmSetBuiltinSignatures stamping and pull` |
|        - | 1219 | ` * their row on demand here. Linear scan — reflection-path only.` |
|        - | 1220 | ` */` |
|      ! 0 | 1221 | `PH7_PRIVATE const char * PH7_VmBuiltinSigLookup(const char *zName,sxu32 nLen,const char **pzRet)` |
|      ! 0 | 1222 | `{` |
|        - | 1223 | `	sxu32 n;` |
|      ! 0 | 1224 | `	if( pzRet ){` |
|      ! 0 | 1225 | `		*pzRet = 0;` |
|      ! 0 | 1226 | `	}` |
|      ! 0 | 1227 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinSig) ; n++ ){` |
|      ! 0 | 1228 | `		if( SyStrlen(aBuiltinSig[n].zName) == nLen` |
|      ! 0 | 1229 | `		 && SyMemcmp(aBuiltinSig[n].zName,zName,nLen) == 0 ){` |
|      ! 0 | 1230 | `			if( pzRet && aBuiltinSig[n].zRet[0] ){` |
|      ! 0 | 1231 | `				*pzRet = aBuiltinSig[n].zRet;` |
|      ! 0 | 1232 | `			}` |
|      ! 0 | 1233 | `			return aBuiltinSig[n].zSig;` |
|        - | 1234 | `		}` |
|      ! 0 | 1235 | `	}` |
|      ! 0 | 1236 | `	return 0;` |
|      ! 0 | 1237 | `}` |
|        - | 1238 | `/*` |
|        - | 1239 | ` * Write a value back to the caller's variable through a builtin argument's` |
|        - | 1240 | ` * stack-slot nIdx — the shared write-back for builtin by-reference` |
|        - | 1241 | ` * out-parameters (preg_match $matches, preg_replace &$count, similar_text` |
|        - | 1242 | ` * &$percent, ...).` |
|        - | 1243 | ` *` |
|        - | 1244 | ` * For a positional out-param argument the call compiler auto-vivifies known` |
|        - | 1245 | ` * by-reference out-params (see GenStateByRefBuiltinMask in compile.c), so a` |
|        - | 1246 | ` * bare undefined variable, an array subscript, and a declared/untyped` |
|        - | 1247 | ` * property all arrive with a real nIdx and are written back here, matching` |
|        - | 1248 | ` * PHP's reference semantics.` |
|        - | 1249 | ` *` |
|        - | 1250 | ` * nIdx stays SXU32_HIGH and the write-back to the caller is skipped (the` |
|        - | 1251 | ` * value still lands in the local stack slot) when the argument cannot expose` |
|        - | 1252 | ` * a stable memobj slot: a literal, a function-call result, a subscript of a` |
|        - | 1253 | ` * non-lvalue parent (foo()['k']), or any variable in a call that also uses` |
|        - | 1254 | ` * named or spread arguments (compile-time positions no longer map to the` |
|        - | 1255 | ` * runtime arg slots, so the compiler conservatively does not vivify). An` |
|        - | 1256 | ` * uninitialized typed property is also not wired (it throws before the` |
|        - | 1257 | ` * write) -- see the recorded deferrals.` |
|        - | 1258 | ` */` |
|      430 | 1259 | `PH7_PRIVATE void PH7_VmStoreArgByRef(ph7_vm *pVm,ph7_value *pArg,ph7_value *pNewVal)` |
|        5 | 1260 | `{` |
|      435 | 1261 | `	if( pArg->nIdx != SXU32_HIGH ){` |
|      433 | 1262 | `		ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pArg->nIdx);` |
|      433 | 1263 | `		if( pObj ){` |
|      433 | 1264 | `			PH7_MemObjStore(pNewVal,pObj);` |
|      214 | 1265 | `		}` |
|      214 | 1266 | `	}` |
|      435 | 1267 | `	PH7_MemObjStore(pNewVal,pArg);` |
|      435 | 1268 | `}` |
|        - | 1269 | `/*` |
|        - | 1270 | ` * Raise an E_WARNING whose text is used VERBATIM.` |
|        - | 1271 | ` * ph7_context_throw_error_format() prepends "func(): " to whatever it is given, but php's` |
|        - | 1272 | ` * IO warnings put the offending path inside those parens -- "file_get_contents(/nope):` |
|        - | 1273 | ` * Failed to open stream: ..." -- so a caller that needs php's exact shape must build the` |
|        - | 1274 | ` * whole line itself and come through here.` |
|        - | 1275 | ` */` |
|        - | 1276 | `/*` |
|        - | 1277 | ` * Validate a callback argument and throw php's TypeError when it cannot be called.` |
|        - | 1278 | ` *` |
|        - | 1279 | ` * The shared dispatcher (PH7_VmCallUserFunctionWithMap) answers SXRET_OK with a NULL` |
|        - | 1280 | ` * result for an unresolvable callable, so every caller that did not check first failed` |
|        - | 1281 | ` * SILENTLY: call_user_func() returned NULL and usort() left the array sorted by nothing` |
|        - | 1282 | ` * at all. php rejects the ARGUMENT up front, naming exactly why.` |
|        - | 1283 | ` */` |
|      728 | 1284 | `PH7_PRIVATE sxi32 PH7_CheckCallbackArg(` |
|        - | 1285 | `	ph7_context *pCtx,   /* Calling context (names the function in the message) */` |
|        - | 1286 | `	ph7_value *pCb,      /* The callback argument */` |
|        - | 1287 | `	int iArg,            /* Its 1-based position */` |
|        - | 1288 | `	const char *zParam,  /* Its php parameter name ("callback"), or 0 for the variadic` |
|        - | 1289 | `	                      * comparators php names by position only (array_udiff …) */` |
|        - | 1290 | `	int bNullable        /* TRUE when php's text says "or null" */` |
|        - | 1291 | `	)` |
|        5 | 1292 | `{` |
|        - | 1293 | `	char zReason[256];` |
|      733 | 1294 | `	const char *zWhy = PH7_VmCallableReason(pCtx->pVm,pCb,zReason,sizeof(zReason));` |
|      733 | 1295 | `	if( zWhy == 0 ){` |
|      577 | 1296 | `		return PH7_OK;` |
|        - | 1297 | `	}` |
|      161 | 1298 | `	if( zParam ){` |
|      185 | 1299 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|        - | 1300 | `			"%s(): Argument #%d ($%s) must be a valid callback%s, %s",` |
|       60 | 1301 | `			ph7_function_name(pCtx),iArg,zParam,bNullable ? " or null" : "",zWhy);` |
|        - | 1302 | `	}` |
|       59 | 1303 | `	return PH7_VmThrowException(pCtx,"TypeError",` |
|        - | 1304 | `		"%s(): Argument #%d must be a valid callback%s, %s",` |
|       18 | 1305 | `		ph7_function_name(pCtx),iArg,bNullable ? " or null" : "",zWhy);` |
|      369 | 1306 | `}` |
|    22020 | 1307 | `PH7_PRIVATE void PH7_VmThrowWarningFmt(ph7_vm *pVm,const char *zFmt,...)` |
|        5 | 1308 | `{` |
|        - | 1309 | `	va_list ap;` |
|    22025 | 1310 | `	va_start(ap,zFmt);` |
|    22025 | 1311 | `	PH7_VmThrowErrorAp(pVm,0,PH7_CTX_WARNING,zFmt,ap);` |
|    22025 | 1312 | `	va_end(ap);` |
|    22025 | 1313 | `}` |
|        - | 1314 | `/*` |
|        - | 1315 | ` * Emit a formatted E_USER_DEPRECATED diagnostic with no function-name prefix:` |
|        - | 1316 | ` * php reports #[\Deprecated] as USER-deprecated (16384, it is userland-authored),` |
|        - | 1317 | ` * not the engine's E_DEPRECATED (8192) — handler-visible errno matters.` |
|        - | 1318 | ` */` |
|       34 | 1319 | `static void VmThrowUserDeprecatedFmt(ph7_vm *pVm,const char *zFmt,...)` |
|        1 | 1320 | `{` |
|        - | 1321 | `	va_list ap;` |
|       35 | 1322 | `	va_start(ap,zFmt);` |
|       35 | 1323 | `	PH7_VmThrowErrorAp(pVm,0,E_USER_DEPRECATED,zFmt,ap);` |
|       35 | 1324 | `	va_end(ap);` |
|       35 | 1325 | `}` |
|        - | 1326 | `/*` |
|        - | 1327 | ` * php 8.4 #[\Deprecated]: emit the E_USER_DEPRECATED notice for a call to a user` |
|        - | 1328 | ` * function/method carrying the attribute. Message shapes (php-exact):` |
|        - | 1329 | ` *   Function f() is deprecated` |
|        - | 1330 | ` *   Method C::m() is deprecated since 2.0, use g() instead` |
|        - | 1331 | ` * ("since" from the attribute's since: argument; the trailing ", msg" from` |
|        - | 1332 | ` * message:/positional #1.) The attribute arguments are compiled constant` |
|        - | 1333 | ` * expressions — evaluated via VmLocalExec, the reflection thunks' pattern.` |
|        - | 1334 | ` * Called from OP_CALL once per call, only when the callee HAS attributes;` |
|        - | 1335 | ` * pDeclClass names the method's declaring class (0 for plain functions).` |
|        - | 1336 | ` */` |
|        - | 1337 | `/*` |
|        - | 1338 | ` * Expand a global constant's value, emitting the php 8.5 #[\Deprecated]` |
|        - | 1339 | ` * E_USER_DEPRECATED notice first when the constant carries attributes` |
|        - | 1340 | `` * (`Constant GD is deprecated since 1.2` — every access re-warns).`` |
|        - | 1341 | ` */` |
|        - | 1342 | `static void VmDeprecatedAttrNoticeSubject(ph7_vm *pVm,SySet *pAttrs,` |
|        - | 1343 | `	const char *zKind,const SyString *pQual,const SyString *pName);` |
|        - | 1344 | `/*` |
|        - | 1345 | ` * Expand a constant, emitting the php 8.5 #[\Deprecated] E_USER_DEPRECATED notice` |
|        - | 1346 | `` * first when a USERLAND constant carries the attribute (`Constant GD is deprecated`` |
|        - | 1347 | `` * since 1.2` — every access re-warns). Engine constants php merely deprecates are`` |
|        - | 1348 | ` * not mimicked: PHL removes them outright (see the scope policy), so there is no` |
|        - | 1349 | ` * engine-side E_DEPRECATED list here.` |
|        - | 1350 | ` */` |
|   126538 | 1351 | `PH7_PRIVATE void VmExpandConstantWithNotice(ph7_vm *pVm,ph7_constant *pCons,ph7_value *pOut)` |
|        5 | 1352 | `{` |
|   126543 | 1353 | `	if( SySetUsed(&pCons->aAttrs) > 0 ){` |
|        7 | 1354 | `		VmDeprecatedAttrNoticeSubject(pVm,&pCons->aAttrs,"Constant",0,&pCons->sName);` |
|        3 | 1355 | `	}` |
|   126543 | 1356 | `	pCons->xExpand(pOut,pCons->pUserData);` |
|   126543 | 1357 | `}` |
|        - | 1358 | `/*` |
|        - | 1359 | ` * Scan a declared-attribute set for #[\Deprecated]; when found, evaluate its` |
|        - | 1360 | ` * message:/since: arguments (positional #0 = message, #1 = since) into the` |
|        - | 1361 | ` * caller's values and return TRUE. The base subject text ("Function f()",` |
|        - | 1362 | ` * "Constant C::K") is the caller's business.` |
|        - | 1363 | ` */` |
|      130 | 1364 | `static int VmDeprecatedAttrExtract(ph7_vm *pVm,SySet *pAttrs,` |
|        - | 1365 | `	ph7_value *pMsg,int *pbMsg,ph7_value *pSince,int *pbSince)` |
|        5 | 1366 | `{` |
|      135 | 1367 | `	ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|        - | 1368 | `	sxu32 n;` |
|      135 | 1369 | `	*pbMsg = *pbSince = 0;` |
|      235 | 1370 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; ++n ){` |
|      139 | 1371 | `		ph7_attribute *pAttr = &aAttr[n];` |
|        - | 1372 | `		ph7_attr_arg *aArg;` |
|      139 | 1373 | `		sxu32 i,nPos = 0;` |
|      134 | 1374 | `		if( SyStringLength(&pAttr->sName) != sizeof("Deprecated")-1` |
|       89 | 1375 | `		 \|\| SyStrnicmp(SyStringData(&pAttr->sName),"Deprecated",sizeof("Deprecated")-1) != 0 ){` |
|      105 | 1376 | `			continue;` |
|        - | 1377 | `		}` |
|       35 | 1378 | `		aArg = (ph7_attr_arg *)SySetBasePtr(&pAttr->aArgs);` |
|       53 | 1379 | `		for( i = 0 ; i < SySetUsed(&pAttr->aArgs) ; ++i ){` |
|       19 | 1380 | `			ph7_attr_arg *pArg = &aArg[i];` |
|       19 | 1381 | `			int isMsg = 0,isSince = 0;` |
|       19 | 1382 | `			if( SyStringLength(&pArg->sName) == 0 ){` |
|        3 | 1383 | `				isMsg = (nPos == 0);` |
|        3 | 1384 | `				isSince = (nPos == 1);` |
|        3 | 1385 | `				nPos++;` |
|       18 | 1386 | `			}else if( SyStringLength(&pArg->sName) == sizeof("message")-1` |
|       12 | 1387 | `			 && SyMemcmp(SyStringData(&pArg->sName),"message",sizeof("message")-1) == 0 ){` |
|        7 | 1388 | `				isMsg = 1;` |
|       14 | 1389 | `			}else if( SyStringLength(&pArg->sName) == sizeof("since")-1` |
|       11 | 1390 | `			 && SyMemcmp(SyStringData(&pArg->sName),"since",sizeof("since")-1) == 0 ){` |
|       11 | 1391 | `				isSince = 1;` |
|        5 | 1392 | `			}` |
|       19 | 1393 | `			if( isMsg && !*pbMsg && SySetUsed(&pArg->aByteCode) > 0 ){` |
|       13 | 1394 | `				if( VmLocalExec(pVm,&pArg->aByteCode,pMsg,FALSE) == SXRET_OK ){` |
|        9 | 1395 | `					if( (pMsg->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 | 1396 | `						PH7_MemObjToString(pMsg);` |
|      ! 0 | 1397 | `					}` |
|        9 | 1398 | `					*pbMsg = 1;` |
|        5 | 1399 | `				}` |
|       15 | 1400 | `			}else if( isSince && !*pbSince && SySetUsed(&pArg->aByteCode) > 0 ){` |
|       11 | 1401 | `				if( VmLocalExec(pVm,&pArg->aByteCode,pSince,FALSE) == SXRET_OK ){` |
|       11 | 1402 | `					if( (pSince->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 | 1403 | `						PH7_MemObjToString(pSince);` |
|      ! 0 | 1404 | `					}` |
|       11 | 1405 | `					*pbSince = 1;` |
|        5 | 1406 | `				}` |
|        5 | 1407 | `			}` |
|       10 | 1408 | `		}` |
|       35 | 1409 | `		return 1;` |
|      ! 0 | 1410 | `	}` |
|      101 | 1411 | `	return 0;` |
|       70 | 1412 | `}` |
|        - | 1413 | `/*` |
|        - | 1414 | ` * Append php's " since X" / ", message" suffixes to a built base subject and` |
|        - | 1415 | ` * emit the E_USER_DEPRECATED notice.` |
|        - | 1416 | ` */` |
|       34 | 1417 | `static void VmDeprecatedEmit(ph7_vm *pVm,SyBlob *pOut,` |
|        - | 1418 | `	ph7_value *pMsg,int bMsg,ph7_value *pSince,int bSince)` |
|        1 | 1419 | `{` |
|       35 | 1420 | `	if( bSince && SyBlobLength(&pSince->sBlob) > 0 ){` |
|       16 | 1421 | `		SyBlobFormat(pOut," since %.*s",(int)SyBlobLength(&pSince->sBlob),` |
|       10 | 1422 | `			(const char *)SyBlobData(&pSince->sBlob));` |
|        5 | 1423 | `	}` |
|       35 | 1424 | `	if( bMsg && SyBlobLength(&pMsg->sBlob) > 0 ){` |
|       13 | 1425 | `		SyBlobFormat(pOut,", %.*s",(int)SyBlobLength(&pMsg->sBlob),` |
|        8 | 1426 | `			(const char *)SyBlobData(&pMsg->sBlob));` |
|        4 | 1427 | `	}` |
|       35 | 1428 | `	VmThrowUserDeprecatedFmt(pVm,"%.*s",(int)SyBlobLength(pOut),(const char *)SyBlobData(pOut));` |
|       35 | 1429 | `}` |
|        - | 1430 | `/*` |
|        - | 1431 | ` * Generic #[\Deprecated] notice for a named subject:` |
|        - | 1432 | ` * "<Kind> [Qual::]Name is deprecated[ since X][, message]".` |
|        - | 1433 | ` */` |
|       18 | 1434 | `static void VmDeprecatedAttrNoticeSubject(ph7_vm *pVm,SySet *pAttrs,` |
|        - | 1435 | `	const char *zKind,const SyString *pQual,const SyString *pName)` |
|        1 | 1436 | `{` |
|        - | 1437 | `	ph7_value sMsg,sSince;` |
|        - | 1438 | `	SyBlob sOut;` |
|        - | 1439 | `	int bMsg,bSince;` |
|       19 | 1440 | `	PH7_MemObjInit(pVm,&sMsg);` |
|       19 | 1441 | `	PH7_MemObjInit(pVm,&sSince);` |
|       19 | 1442 | `	if( VmDeprecatedAttrExtract(pVm,pAttrs,&sMsg,&bMsg,&sSince,&bSince) ){` |
|       15 | 1443 | `		SyBlobInit(&sOut,&pVm->sAllocator);` |
|       15 | 1444 | `		if( pQual ){` |
|       11 | 1445 | `			SyBlobFormat(&sOut,"%s %z::%z is deprecated",zKind,pQual,pName);` |
|        6 | 1446 | `		}else{` |
|        5 | 1447 | `			SyBlobFormat(&sOut,"%s %z is deprecated",zKind,pName);` |
|        - | 1448 | `		}` |
|       15 | 1449 | `		VmDeprecatedEmit(pVm,&sOut,&sMsg,bMsg,&sSince,bSince);` |
|       15 | 1450 | `		SyBlobRelease(&sOut);` |
|        7 | 1451 | `	}` |
|       19 | 1452 | `	PH7_MemObjRelease(&sMsg);` |
|       19 | 1453 | `	PH7_MemObjRelease(&sSince);` |
|       19 | 1454 | `}` |
|      112 | 1455 | `PH7_PRIVATE void VmDeprecatedAttrNotice(ph7_vm *pVm,ph7_vm_func *pFunc,ph7_class *pDeclClass)` |
|        5 | 1456 | `{` |
|        - | 1457 | `	ph7_value sMsg,sSince;` |
|        - | 1458 | `	SyBlob sOut;` |
|        - | 1459 | `	int bMsg,bSince;` |
|      117 | 1460 | `	PH7_MemObjInit(pVm,&sMsg);` |
|      117 | 1461 | `	PH7_MemObjInit(pVm,&sSince);` |
|      117 | 1462 | `	if( VmDeprecatedAttrExtract(pVm,&pFunc->aAttrs,&sMsg,&bMsg,&sSince,&bSince) ){` |
|       21 | 1463 | `		SyBlobInit(&sOut,&pVm->sAllocator);` |
|       21 | 1464 | `		if( pDeclClass ){` |
|        5 | 1465 | `			SyBlobFormat(&sOut,"Method %z::%z() is deprecated",&pDeclClass->sName,&pFunc->sName);` |
|        3 | 1466 | `		}else{` |
|       17 | 1467 | `			SyBlobFormat(&sOut,"Function %z() is deprecated",&pFunc->sName);` |
|        - | 1468 | `		}` |
|       21 | 1469 | `		VmDeprecatedEmit(pVm,&sOut,&sMsg,bMsg,&sSince,bSince);` |
|       21 | 1470 | `		SyBlobRelease(&sOut);` |
|       10 | 1471 | `	}` |
|      117 | 1472 | `	PH7_MemObjRelease(&sMsg);` |
|      117 | 1473 | `	PH7_MemObjRelease(&sSince);` |
|      117 | 1474 | `}` |
|        - | 1475 | `/*` |
|        - | 1476 | ` * Same notice for a #[\Deprecated] class constant, at its static-access site:` |
|        - | 1477 | `` * php's `Constant C::K is deprecated` (each access re-warns).`` |
|        - | 1478 | ` */` |
|       12 | 1479 | `PH7_PRIVATE void VmDeprecatedConstNotice(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pMember)` |
|        1 | 1480 | `{` |
|       19 | 1481 | `	VmDeprecatedAttrNoticeSubject(pVm,&pMember->aAttrs,` |
|       12 | 1482 | `		(pMember->iFlags & PH7_CLASS_ATTR_ENUMCASE) ? "Enum case" : "Constant",` |
|       12 | 1483 | `		&pClass->sName,&pMember->sName);` |
|       13 | 1484 | `}` |
|        - | 1485 | `/*` |
|        - | 1486 | `` * The USER-VISIBLE string coercion a builtin performs on a `mixed` argument or`` |
|        - | 1487 | ` * array element: the builtin-side twin of PH7_MemObjToStringUV's opcode sites.` |
|        - | 1488 | ` * An ARRAY warns "Array to string conversion" and still renders as "Array"; an` |
|        - | 1489 | ` * object whose class has no __toString() -- or one whose __toString() threw --` |
|        - | 1490 | ` * is php's catchable "could not be converted to string" Error, and the builtin` |
|        - | 1491 | ` * must answer that instead of a value.` |
|        - | 1492 | ` *` |
|        - | 1493 | ` * On success pzData and pnLen receive the NUL-terminated bytes (both optional).` |
|        - | 1494 | ` * On a throw they are set to the empty string and the status is returned AND` |
|        - | 1495 | ` * recorded on the call context, so OP_CALL cannot mistake the call for a normal` |
|        - | 1496 | ` * return; a builtin that has already produced output (printf) still keeps it,` |
|        - | 1497 | ` * which is what php does.` |
|        - | 1498 | ` *` |
|        - | 1499 | ` * The public ph7_value_to_string() stays SILENT on purpose: it is the embedder` |
|        - | 1500 | ` * API, and the INTERNAL coercions behind it (array-key canonicalisation, sort` |
|        - | 1501 | ` * comparisons, print_r/var_export/serialize) must not throw -- php's do not` |
|        - | 1502 | ` * either.` |
|        - | 1503 | ` */` |
|   203180 | 1504 | `PH7_PRIVATE sxi32 PH7_ValueToStringUV(ph7_context *pCtx,ph7_value *pValue,const char **pzData,int *pnLen)` |
|        5 | 1505 | `{` |
|   203185 | 1506 | `	sxi32 rc = PH7_MemObjToStringUV(pValue);` |
|   203185 | 1507 | `	if( rc != SXRET_OK ){` |
|       37 | 1508 | `		if( pCtx ){` |
|       37 | 1509 | `			pCtx->nThrowRc = rc;` |
|       17 | 1510 | `		}` |
|       37 | 1511 | `		if( pzData ){` |
|       33 | 1512 | `			*pzData = "";` |
|       15 | 1513 | `		}` |
|       37 | 1514 | `		if( pnLen ){` |
|       33 | 1515 | `			*pnLen = 0;` |
|       15 | 1516 | `		}` |
|       37 | 1517 | `		return rc;` |
|        - | 1518 | `	}` |
|   203151 | 1519 | `	if( pzData \|\| pnLen ){` |
|   203123 | 1520 | `		const char *zData = ph7_value_to_string(pValue,pnLen);` |
|   203123 | 1521 | `		if( pzData ){` |
|   203123 | 1522 | `			*pzData = zData;` |
|   101559 | 1523 | `		}` |
|   101559 | 1524 | `	}` |
|   203151 | 1525 | `	return SXRET_OK;` |
|   101595 | 1526 | `}` |
|        - | 1527 |  |
