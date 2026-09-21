# src/ph7/vm_arg_check.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 414/423 lines (97.87%)

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
|     3646 |  332 | `PH7_PRIVATE void VmSetBuiltinArity(ph7_vm *pVm)` |
|        5 |  333 | `{` |
|        - |  334 | `	sxu32 n;` |
|   988071 |  335 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinArity) ; ++n ){` |
|   984425 |  336 | `		const struct VmBuiltinArity *p = &aBuiltinArity[n];` |
|  1968845 |  337 | `		SyHashEntry *pEntry = SyHashGet(&pVm->hHostFunction,` |
|   984420 |  338 | `			(const void *)p->zName,SyStrlen(p->zName));` |
|   984425 |  339 | `		if( pEntry ){` |
|   984425 |  340 | `			ph7_user_func *pFunc = (ph7_user_func *)pEntry->pUserData;` |
|   984425 |  341 | `			pFunc->nMinArg  = p->nMin;` |
|   984425 |  342 | `			pFunc->bAtLeast = p->bAtLeast;` |
|   492210 |  343 | `		}` |
|   492215 |  344 | `	}` |
|     3651 |  345 | `}` |
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
|        - |  610 | `	{ "krsort", "array &$array, int $flags = 0", "true" },` |
|        - |  611 | `	{ "ksort", "array &$array, int $flags = 0", "true" },` |
|        - |  612 | `	{ "lcfirst", "string $string", "string" },` |
|        - |  613 | `	{ "levenshtein", "string $string1, string $string2, int $insertion_cost = 1, int $replacement_cost = 1, int $deletion_cost = 1", "int" },` |
|        - |  614 | `	{ "link", "string $target, string $link", "bool" },` |
|        - |  615 | `	{ "localtime", "?int $timestamp = NULL, bool $associative = false", "array" },` |
|        - |  616 | `	{ "log", "float $num, float $base = 2.718281828459045", "float" },` |
|        - |  617 | `	{ "log10", "float $num", "float" },` |
|        - |  618 | `	{ "lstat", "string $filename", "array\|false" },` |
|        - |  619 | `	{ "ltrim", "string $string, string $characters = ?", "string" },` |
|        - |  620 | `	{ "max", "mixed $value, mixed ...$values = ?", "mixed" },` |
|        - |  621 | `	{ "mb_chr", "int $codepoint, ?string $encoding = NULL", "string\|false" },` |
|        - |  622 | `	{ "mb_convert_encoding", "array\|string $string, string $to_encoding, array\|string\|null $from_encoding = NULL", "array\|string" },` |
|        - |  623 | `	{ "mb_ord", "string $string, ?string $encoding = NULL", "int\|false" },` |
|        - |  624 | `	{ "mb_strtolower", "string $string, ?string $encoding = NULL", "string" },` |
|        - |  625 | `	{ "mb_strtoupper", "string $string, ?string $encoding = NULL", "string" },` |
|        - |  626 | `	{ "md5", "string $string, bool $binary = false", "string" },` |
|        - |  627 | `	{ "md5_file", "string $filename, bool $binary = false", "string\|false" },` |
|        - |  628 | `	{ "method_exists", "$object_or_class, string $method", "bool" },` |
|        - |  629 | `	{ "memory_get_peak_usage", "bool $real_usage = false", "int" },` |
|        - |  630 | `	{ "memory_get_usage", "bool $real_usage = false", "int" },` |
|        - |  631 | `	{ "microtime", "bool $as_float = false", "string\|float" },` |
|        - |  632 | `	{ "min", "mixed $value, mixed ...$values = ?", "mixed" },` |
|        - |  633 | `	{ "mkdir", "string $directory, int $permissions = 511, bool $recursive = false, $context = NULL", "bool" },` |
|        - |  634 | `	{ "mktime", "int $hour, ?int $minute = NULL, ?int $second = NULL, ?int $month = NULL, ?int $day = NULL, ?int $year = NULL", "int\|false" },` |
|        - |  635 | `	{ "mt_getrandmax", "", "int" },` |
|        - |  636 | `	{ "mt_rand", "int $min = ?, int $max = ?", "int" },` |
|        - |  637 | `	{ "mt_srand", "?int $seed = NULL, int $mode = 0", "void" },` |
|        - |  638 | `	{ "next", "object\|array &$array", "mixed" },` |
|        - |  639 | `	{ "nl2br", "string $string, bool $use_xhtml = true", "string" },` |
|        - |  640 | `	{ "ob_clean", "", "bool" },` |
|        - |  641 | `	{ "ob_end_clean", "", "bool" },` |
|        - |  642 | `	{ "ob_end_flush", "", "bool" },` |
|        - |  643 | `	{ "ob_flush", "", "bool" },` |
|        - |  644 | `	{ "ob_get_clean", "", "string\|false" },` |
|        - |  645 | `	{ "ob_get_contents", "", "string\|false" },` |
|        - |  646 | `	{ "ob_get_flush", "", "string\|false" },` |
|        - |  647 | `	{ "ob_get_length", "", "int\|false" },` |
|        - |  648 | `	{ "ob_get_level", "", "int" },` |
|        - |  649 | `	{ "ob_implicit_flush", "bool $enable = true", "void" },` |
|        - |  650 | `	{ "ob_list_handlers", "", "array" },` |
|        - |  651 | `	{ "ob_start", "$callback = NULL, int $chunk_size = 0, int $flags = 112", "bool" },` |
|        - |  652 | `	{ "octdec", "string $octal_string", "int\|float" },` |
|        - |  653 | `	{ "opendir", "string $directory, $context = NULL", "" },` |
|        - |  654 | `	{ "ord", "string $character", "int" },` |
|        - |  655 | `	{ "parse_ini_file", "string $filename, bool $process_sections = false, int $scanner_mode = 0", "array\|false" },` |
|        - |  656 | `	{ "parse_ini_string", "string $ini_string, bool $process_sections = false, int $scanner_mode = 0", "array\|false" },` |
|        - |  657 | `	{ "parse_url", "string $url, int $component = -1", "array\|string\|int\|false\|null" },` |
|        - |  658 | `	{ "password_get_info", "string $hash", "array" },` |
|        - |  659 | `	{ "password_hash", "string $password, string\|int\|null $algo, array $options = ?", "string" },` |
|        - |  660 | `	{ "password_needs_rehash", "string $hash, string\|int\|null $algo, array $options = ?", "bool" },` |
|        - |  661 | `	{ "password_verify", "string $password, string $hash", "bool" },` |
|        - |  662 | `	{ "pathinfo", "string $path, int $flags = 15", "array\|string" },` |
|        - |  663 | `	{ "pclose", "$handle", "int" },` |
|        - |  664 | `	{ "php_sapi_name", "", "string\|false" },` |
|        - |  665 | `	{ "php_uname", "string $mode = 'a'", "string" },` |
|        - |  666 | `	{ "phpinfo", "int $flags = 4294967295", "true" },` |
|        - |  667 | `	{ "phpversion", "?string $extension = NULL", "string\|false" },` |
|        - |  668 | `	{ "pi", "", "float" },` |
|        - |  669 | `	{ "popen", "string $command, string $mode", "" },` |
|        - |  670 | `	{ "pos", "object\|array $array", "mixed" },` |
|        - |  671 | `	{ "pow", "mixed $num, mixed $exponent", "object\|int\|float" },` |
|        - |  672 | `	{ "preg_last_error", "", "int" },` |
|        - |  673 | `	{ "preg_last_error_msg", "", "string" },` |
|        - |  674 | `	{ "fsockopen", "string $hostname, int $port = -1, &$error_code = NULL, &$error_message = NULL, ?float $timeout = NULL", "resource\|false" },` |
|        - |  675 | `	{ "pfsockopen", "string $hostname, int $port = -1, &$error_code = NULL, &$error_message = NULL, ?float $timeout = NULL", "resource\|false" },` |
|        - |  676 | `	{ "stream_socket_client", "string $address, &$error_code = NULL, &$error_message = NULL, ?float $timeout = NULL, int $flags = 4, $context = NULL", "resource\|false" },` |
|        - |  677 | `	{ "preg_match", "string $pattern, string $subject, &$matches = NULL, int $flags = 0, int $offset = 0", "int\|false" },` |
|        - |  678 | `	{ "preg_match_all", "string $pattern, string $subject, &$matches = NULL, int $flags = 0, int $offset = 0", "int\|false" },` |
|        - |  679 | `	{ "preg_quote", "string $str, ?string $delimiter = NULL", "string" },` |
|        - |  680 | `	{ "preg_replace", "array\|string $pattern, array\|string $replacement, array\|string $subject, int $limit = -1, &$count = NULL", "array\|string\|null" },` |
|        - |  681 | `	{ "preg_replace_callback", "array\|string $pattern, callable $callback, array\|string $subject, int $limit = -1, &$count = NULL, int $flags = 0", "array\|string\|null" },` |
|        - |  682 | `	{ "preg_split", "string $pattern, string $subject, int $limit = -1, int $flags = 0", "array\|false" },` |
|        - |  683 | `	{ "prev", "object\|array &$array", "mixed" },` |
|        - |  684 | `	{ "print_r", "mixed $value, bool $return = false", "string\|true" },` |
|        - |  685 | `	{ "printf", "string $format, mixed ...$values = ?", "int" },` |
|        - |  686 | `	{ "property_exists", "$object_or_class, string $property", "bool" },` |
|        - |  687 | `	{ "putenv", "string $assignment", "bool" },` |
|        - |  688 | `	{ "quotemeta", "string $string", "string" },` |
|        - |  689 | `	{ "rand", "int $min = ?, int $max = ?", "int" },` |
|        - |  690 | `	{ "random_bytes", "int $length", "string" },` |
|        - |  691 | `	{ "random_int", "int $min, int $max", "int" },` |
|        - |  692 | `	{ "range", "string\|int\|float $start, string\|int\|float $end, int\|float $step = 1", "array" },` |
|        - |  693 | `	{ "rawurldecode", "string $string", "string" },` |
|        - |  694 | `	{ "rawurlencode", "string $string", "string" },` |
|        - |  695 | `	{ "readdir", "$dir_handle = NULL", "string\|false" },` |
|        - |  696 | `	{ "readfile", "string $filename, bool $use_include_path = false, $context = NULL", "int\|false" },` |
|        - |  697 | `	{ "realpath", "string $path", "string\|false" },` |
|        - |  698 | `	{ "register_shutdown_function", "callable $callback, mixed ...$args = ?", "void" },` |
|        - |  699 | `	{ "rename", "string $from, string $to, $context = NULL", "bool" },` |
|        - |  700 | `	{ "reset", "object\|array &$array", "mixed" },` |
|        - |  701 | `	{ "restore_error_handler", "", "true" },` |
|        - |  702 | `	{ "restore_exception_handler", "", "true" },` |
|        - |  703 | `	{ "rewind", "$stream", "bool" },` |
|        - |  704 | `	{ "rewinddir", "$dir_handle = NULL", "void" },` |
|        - |  705 | `	{ "rmdir", "string $directory, $context = NULL", "bool" },` |
|        - |  706 | `	{ "round", "int\|float $num, int $precision = 0, RoundingMode\|int $mode = ?", "float" },` |
|        - |  707 | `	{ "rsort", "array &$array, int $flags = 0", "true" },` |
|        - |  708 | `	{ "rtrim", "string $string, string $characters = ?", "string" },` |
|        - |  709 | `	{ "serialize", "mixed $value", "string" },` |
|        - |  710 | `	{ "set_error_handler", "?callable $callback, int $error_levels = 30719", "" },` |
|        - |  711 | `	{ "set_exception_handler", "?callable $callback", "" },` |
|        - |  712 | `	{ "get_error_handler", "", "?callable" },` |
|        - |  713 | `	{ "get_exception_handler", "", "?callable" },` |
|        - |  714 | `	{ "hrtime", "bool $as_number = false", "array\|int" },` |
|        - |  715 | `	{ "setcookie", "string $name, string $value = '', array\|int $expires_or_options = 0, string $path = '', string $domain = '', bool $secure = false, bool $httponly = false", "bool" },` |
|        - |  716 | `	{ "setrawcookie", "string $name, string $value = '', array\|int $expires_or_options = 0, string $path = '', string $domain = '', bool $secure = false, bool $httponly = false", "bool" },` |
|        - |  717 | `	{ "settype", "mixed &$var, string $type", "bool" },` |
|        - |  718 | `	{ "sha1", "string $string, bool $binary = false", "string" },` |
|        - |  719 | `	{ "sha1_file", "string $filename, bool $binary = false", "string\|false" },` |
|        - |  720 | `	{ "shuffle", "array &$array", "true" },` |
|        - |  721 | `	{ "similar_text", "string $string1, string $string2, &$percent = NULL", "int" },` |
|        - |  722 | `	{ "sin", "float $num", "float" },` |
|        - |  723 | `	{ "sinh", "float $num", "float" },` |
|        - |  724 | `	{ "sizeof", "Countable\|array $value, int $mode = 0", "int" },` |
|        - |  725 | `	{ "sleep", "int $seconds", "int" },` |
|        - |  726 | `	{ "sort", "array &$array, int $flags = 0", "true" },` |
|        - |  727 | `	{ "soundex", "string $string", "string" },` |
|        - |  728 | `	{ "spl_autoload", "string $class, ?string $file_extensions = NULL", "void" },` |
|        - |  729 | `	{ "spl_autoload_functions", "", "array" },` |
|        - |  730 | `	{ "spl_autoload_register", "?callable $callback = NULL, bool $throw = true, bool $prepend = false", "bool" },` |
|        - |  731 | `	{ "spl_autoload_unregister", "callable $callback", "bool" },` |
|        - |  732 | `	{ "spl_object_hash", "object $object", "string" },` |
|        - |  733 | `	{ "spl_object_id", "object $object", "int" },` |
|        - |  734 | `	{ "sprintf", "string $format, mixed ...$values = ?", "string" },` |
|        - |  735 | `	{ "sqrt", "float $num", "float" },` |
|        - |  736 | `	{ "srand", "?int $seed = NULL, int $mode = 0", "void" },` |
|        - |  737 | `	{ "stat", "string $filename", "array\|false" },` |
|        - |  738 | `	{ "str_contains", "string $haystack, string $needle", "bool" },` |
|        - |  739 | `	{ "str_ends_with", "string $haystack, string $needle", "bool" },` |
|        - |  740 | `	{ "str_getcsv", "string $string, string $separator = ',', string $enclosure = '\"', string $escape = '\\\\'", "array" },` |
|        - |  741 | `	{ "str_ireplace", "array\|string $search, array\|string $replace, array\|string $subject, &$count = NULL", "array\|string" },` |
|        - |  742 | `	{ "str_pad", "string $string, int $length, string $pad_string = ' ', int $pad_type = 1", "string" },` |
|        - |  743 | `	{ "str_repeat", "string $string, int $times", "string" },` |
|        - |  744 | `	{ "str_replace", "array\|string $search, array\|string $replace, array\|string $subject, &$count = NULL", "array\|string" },` |
|        - |  745 | `	{ "str_shuffle", "string $string", "string" },` |
|        - |  746 | `	{ "str_split", "string $string, int $length = 1", "array" },` |
|        - |  747 | `	{ "str_starts_with", "string $haystack, string $needle", "bool" },` |
|        - |  748 | `	{ "str_word_count", "string $string, int $format = 0, ?string $characters = NULL", "array\|int" },` |
|        - |  749 | `	{ "strcasecmp", "string $string1, string $string2", "int" },` |
|        - |  750 | `	{ "strchr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|        - |  751 | `	{ "strcmp", "string $string1, string $string2", "int" },` |
|        - |  752 | `	{ "strnatcasecmp", "string $string1, string $string2", "int" },` |
|        - |  753 | `	{ "strnatcmp", "string $string1, string $string2", "int" },` |
|        - |  754 | `	{ "strcoll", "string $string1, string $string2", "int" },` |
|        - |  755 | `	{ "strcspn", "string $string, string $characters, int $offset = 0, ?int $length = NULL", "int" },` |
|        - |  756 | `	{ "strip_tags", "string $string, array\|string\|null $allowed_tags = NULL", "string" },` |
|        - |  757 | `	{ "stripos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|        - |  758 | `	{ "stripslashes", "string $string", "string" },` |
|        - |  759 | `	{ "stristr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|        - |  760 | `	{ "strlen", "string $string", "int" },` |
|        - |  761 | `	{ "strncasecmp", "string $string1, string $string2, int $length", "int" },` |
|        - |  762 | `	{ "strncmp", "string $string1, string $string2, int $length", "int" },` |
|        - |  763 | `	{ "strpbrk", "string $string, string $characters", "string\|false" },` |
|        - |  764 | `	{ "strpos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|        - |  765 | `	{ "strrchr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|        - |  766 | `	{ "strrev", "string $string", "string" },` |
|        - |  767 | `	{ "strripos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|        - |  768 | `	{ "strrpos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|        - |  769 | `	{ "strspn", "string $string, string $characters, int $offset = 0, ?int $length = NULL", "int" },` |
|        - |  770 | `	{ "strstr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|        - |  771 | `	{ "strtok", "string $string, ?string $token = NULL", "string\|false" },` |
|        - |  772 | `	{ "strtolower", "string $string", "string" },` |
|        - |  773 | `	{ "strtoupper", "string $string", "string" },` |
|        - |  774 | `	{ "strtr", "string $string, array\|string $from, ?string $to = NULL", "string" },` |
|        - |  775 | `	{ "strval", "mixed $value", "string" },` |
|        - |  776 | `	{ "substr", "string $string, int $offset, ?int $length = NULL", "string" },` |
|        - |  777 | `	{ "substr_compare", "string $haystack, string $needle, int $offset, ?int $length = NULL, bool $case_insensitive = false", "int" },` |
|        - |  778 | `	{ "substr_count", "string $haystack, string $needle, int $offset = 0, ?int $length = NULL", "int" },` |
|        - |  779 | `	{ "substr_replace", "array\|string $string, array\|string $replace, array\|int $offset, array\|int\|null $length = NULL", "array\|string" },` |
|        - |  780 | `	{ "symlink", "string $target, string $link", "bool" },` |
|        - |  781 | `	{ "sys_get_temp_dir", "", "string" },` |
|        - |  782 | `	{ "tan", "float $num", "float" },` |
|        - |  783 | `	{ "tanh", "float $num", "float" },` |
|        - |  784 | `	{ "time", "", "int" },` |
|        - |  785 | `	{ "touch", "string $filename, ?int $mtime = NULL, ?int $atime = NULL", "bool" },` |
|        - |  786 | `	{ "trigger_error", "string $message, int $error_level = 1024", "true" },` |
|        - |  787 | `	{ "trim", "string $string, string $characters = ?", "string" },` |
|        - |  788 | `	{ "uasort", "array &$array, callable $callback", "true" },` |
|        - |  789 | `	{ "ucfirst", "string $string", "string" },` |
|        - |  790 | `	{ "ucwords", "string $string, string $separators = ?", "string" },` |
|        - |  791 | `	{ "uksort", "array &$array, callable $callback", "true" },` |
|        - |  792 | `	{ "umask", "?int $mask = NULL", "int" },` |
|        - |  793 | `	{ "uniqid", "string $prefix = '', bool $more_entropy = false", "string" },` |
|        - |  794 | `	{ "unlink", "string $filename, $context = NULL", "bool" },` |
|        - |  795 | `	{ "unserialize", "string $data, array $options = ?", "mixed" },` |
|        - |  796 | `	{ "urldecode", "string $string", "string" },` |
|        - |  797 | `	{ "urlencode", "string $string", "string" },` |
|        - |  798 | `	{ "user_error", "string $message, int $error_level = 1024", "true" },` |
|        - |  799 | `	{ "usleep", "int $microseconds", "void" },` |
|        - |  800 | `	{ "usort", "array &$array, callable $callback", "true" },` |
|        - |  801 | `	{ "var_dump", "mixed $value, mixed ...$values = ?", "void" },` |
|        - |  802 | `	{ "var_export", "mixed $value, bool $return = false", "?string" },` |
|        - |  803 | `	{ "vfprintf", "$stream, string $format, array $values", "int" },` |
|        - |  804 | `	{ "vprintf", "string $format, array $values", "int" },` |
|        - |  805 | `	{ "vsprintf", "string $format, array $values", "string" },` |
|        - |  806 | `	{ "wordwrap", "string $string, int $width = 75, string $break = ?, bool $cut_long_words = false", "string" },` |
|        - |  807 | `	{ "zip_close", "$zip", "void" },` |
|        - |  808 | `	{ "zip_entry_close", "$zip_entry", "bool" },` |
|        - |  809 | `	{ "zip_entry_compressedsize", "$zip_entry", "int\|false" },` |
|        - |  810 | `	{ "zip_entry_compressionmethod", "$zip_entry", "string\|false" },` |
|        - |  811 | `	{ "zip_entry_filesize", "$zip_entry", "int\|false" },` |
|        - |  812 | `	{ "zip_entry_name", "$zip_entry", "string\|false" },` |
|        - |  813 | `	{ "zip_entry_open", "$zip_dp, $zip_entry, string $mode = 'rb'", "bool" },` |
|        - |  814 | `	{ "zip_entry_read", "$zip_entry, int $len = 1024", "string\|false" },` |
|        - |  815 | `	{ "zip_open", "string $filename", "" },` |
|        - |  816 | `	{ "zip_read", "$zip", "" },` |
|        - |  817 | `};` |
|        - |  818 | `/*` |
|        - |  819 | ` * Stamp the signature strings onto the registered host functions.` |
|        - |  820 | ` * Runs once at PH7_VmMakeReady, after the builtins are registered.` |
|        - |  821 | ` */` |
|        - |  822 | `/*` |
|        - |  823 | ` * Derive the minimum arity from a builtin's declared signature: a parameter is` |
|        - |  824 | ` * optional when it carries a default ("int $length = 76") or is variadic` |
|        - |  825 | ` * ("...$rest"), so the minimum is the count of the parameters that are neither,` |
|        - |  826 | ` * and the ZPP wording is "at least" as soon as one optional/variadic exists.` |
|        - |  827 | ` *` |
|        - |  828 | ` * This makes aBuiltinSig[] the single source of truth for arity — the curated` |
|        - |  829 | ` * aBuiltinArity[] table above stays as an OVERRIDE for the handful of builtins` |
|        - |  830 | ` * php reports differently from their own signature (e.g. strtr(), whose 3-arg` |
|        - |  831 | ` * form still reports "expects exactly 2", and the array_udiff() family, whose` |
|        - |  832 | ` * variadic tail hides a second required argument). Verified against php 8.5.7` |
|        - |  833 | ` * for all 462 signed builtins: 458 derive exactly, 4 are overridden.` |
|        - |  834 | ` */` |
|  1626116 |  835 | `static void VmDeriveArityFromSig(const char *zSig,sxi16 *pnMin,sxu8 *pbAtLeast,sxi16 *pnMax,sxu8 *pbHasMax)` |
|        5 |  836 | `{` |
|  1626121 |  837 | `	const char *zCur = zSig;` |
|  1626121 |  838 | `	int nMin = 0, bAtLeast = 0, bSeen = 0, bOptional = 0;` |
|  1626121 |  839 | `	int nTotal = 0, bVariadic = 0;` |
| 25128232 |  840 | `	for(;;){` |
| 51718515 |  841 | `		if( zCur[0] == '\0' \|\| zCur[0] == ',' ){` |
|  3088167 |  842 | `			if( bSeen ){` |
|  2905867 |  843 | `				nTotal++;` |
|  2905867 |  844 | `				if( bOptional ){` |
|  1141203 |  845 | `					bAtLeast = 1;` |
|   570604 |  846 | `				}else{` |
|  1764669 |  847 | `					nMin++;` |
|        - |  848 | `				}` |
|  1452931 |  849 | `			}` |
|  3088167 |  850 | `			if( zCur[0] == '\0' ){` |
|  1626121 |  851 | `				break;` |
|        - |  852 | `			}` |
|  1462051 |  853 | `			bSeen = bOptional = 0;` |
|  1462051 |  854 | `			zCur++;` |
|  1462051 |  855 | `			continue;` |
|        - |  856 | `		}` |
| 48630353 |  857 | `		if( zCur[0] != ' ' ){` |
| 42231623 |  858 | `			bSeen = 1;` |
| 21115809 |  859 | `		}` |
| 48630353 |  860 | `		if( zCur[0] == '=' \|\| (zCur[0] == '.' && zCur[1] == '.' && zCur[2] == '.') ){` |
|  1217769 |  861 | `			bOptional = 1;` |
|   608882 |  862 | `		}` |
| 48630353 |  863 | `		if( zCur[0] == '.' && zCur[1] == '.' && zCur[2] == '.' ){` |
|    76571 |  864 | `			bVariadic = 1;` |
|    38283 |  865 | `		}` |
| 48630353 |  866 | `		zCur++;` |
|        5 |  867 | `	}` |
|  1626121 |  868 | `	*pnMin = (sxi16)nMin;` |
|  1626121 |  869 | `	*pbAtLeast = (sxu8)bAtLeast;` |
|        - |  870 | `	/* php enforces a MAXIMUM too ("expects at most 1 argument, 2 given"); a` |
|        - |  871 | `	 * variadic tail means there is none. The parameter COUNT is the maximum,` |
|        - |  872 | `	 * whether or not the parameters carry defaults. */` |
|  1626121 |  873 | `	*pnMax = (sxi16)nTotal;` |
|  1626121 |  874 | `	*pbHasMax = (sxu8)(bVariadic ? 0 : 1);` |
|  1626121 |  875 | `}` |
|        - |  876 | `/*` |
|        - |  877 | ` * Does the declared type list (e.g. "array\|string", "?int", "callable") contain` |
|        - |  878 | ` * the given token? Compares against each '\|'-separated alternative, ignoring a` |
|        - |  879 | ` * leading nullable '?'.` |
|        - |  880 | ` */` |
|  1625738 |  881 | `static int VmSigTypeHas(const char *zType,int nType,const char *zTok)` |
|        5 |  882 | `{` |
|  1625743 |  883 | `	int nTok = (int)SyStrlen(zTok);` |
|  1625743 |  884 | `	int i = 0;` |
|  1625743 |  885 | `	if( zType[0] == '?' ){` |
|   295477 |  886 | `		zType++;` |
|   295477 |  887 | `		nType--;` |
|   147736 |  888 | `	}` |
|  3271065 |  889 | `	while( i < nType ){` |
|  1807679 |  890 | `		int j = i;` |
| 10565232 |  891 | `		while( j < nType && zType[j] != '\|' ){` |
|  8757558 |  892 | `			j++;` |
|        5 |  893 | `		}` |
|  1807679 |  894 | `		if( j - i == nTok && SyMemcmp(&zType[i],zTok,(sxu32)nTok) == 0 ){` |
|   162357 |  895 | `			return 1;` |
|        - |  896 | `		}` |
|  1645327 |  897 | `		i = j + 1;` |
|        5 |  898 | `	}` |
|  1463391 |  899 | `	return 0;` |
|   813639 |  900 | `}` |
|        - |  901 | `/*` |
|        - |  902 | ` * Does the declared type list name a CLASS (anything that is not one of php's` |
|        - |  903 | ` * builtin type keywords)? A class-typed parameter accepts an object, so it must` |
|        - |  904 | ` * not be rejected by the array/object/resource screen below.` |
|        - |  905 | ` */` |
|      216 |  906 | `static int VmSigTypeHasClass(const char *zType,int nType)` |
|        5 |  907 | `{` |
|        - |  908 | `	static const char *azBuiltin[] = {` |
|        - |  909 | `		"int","float","string","bool","array","object","callable","iterable",` |
|        - |  910 | `		"mixed","null","void","resource","false","true","never","self","static"` |
|        - |  911 | `	};` |
|      221 |  912 | `	int i = 0;` |
|      221 |  913 | `	if( zType[0] == '?' ){` |
|        7 |  914 | `		zType++;` |
|        7 |  915 | `		nType--;` |
|        3 |  916 | `	}` |
|      341 |  917 | `	while( i < nType ){` |
|      235 |  918 | `		int j = i, k, bKnown = 0;` |
|     1979 |  919 | `		while( j < nType && zType[j] != '\|' ){` |
|     1749 |  920 | `			j++;` |
|        5 |  921 | `		}` |
|     2357 |  922 | `		for( k = 0 ; k < (int)SX_ARRAYSIZE(azBuiltin) ; ++k ){` |
|     2247 |  923 | `			int nB = (int)SyStrlen(azBuiltin[k]);` |
|     2247 |  924 | `			if( j - i == nB && SyMemcmp(&zType[i],azBuiltin[k],(sxu32)nB) == 0 ){` |
|      124 |  925 | `				bKnown = 1;` |
|      124 |  926 | `				break;` |
|        - |  927 | `			}` |
|     1066 |  928 | `		}` |
|      235 |  929 | `		if( !bKnown && j > i ){` |
|      115 |  930 | `			return 1;` |
|        - |  931 | `		}` |
|      124 |  932 | `		i = j + 1;` |
|        4 |  933 | `	}` |
|      110 |  934 | `	return 0;` |
|      113 |  935 | `}` |
|        - |  936 | `/*` |
|        - |  937 | ` * php's ", X given" tail: like ph7_type_name() but an object reports its CLASS,` |
|        - |  938 | ` * which is what php prints in a TypeError.` |
|        - |  939 | ` */` |
|       56 |  940 | `static const char * VmArgTypeName(ph7_value *pVal)` |
|        3 |  941 | `{` |
|       59 |  942 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       59 |  943 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       59 |  944 | `		if( pInst && pInst->pClass ){` |
|       59 |  945 | `			return pInst->pClass->sName.zString;` |
|        - |  946 | `		}` |
|      ! 0 |  947 | `	}` |
|      ! 0 |  948 | `	return ph7_type_name(pVal);` |
|       31 |  949 | `}` |
|        - |  950 | `/*` |
|        - |  951 | `` * Does pArg satisfy a `string` parameter? The rule VmEnforceBuiltinArgTypes()`` |
|        - |  952 | ` * below applies, factored out so a builtin that words its own overload dispatch` |
|        - |  953 | ` * (strtr(), whose expected type depends on the ARITY and so cannot be spelled in` |
|        - |  954 | ` * one signature) decides identically instead of forking the logic. An array never` |
|        - |  955 | ` * satisfies one; an object does only through __toString(); a resource does not;` |
|        - |  956 | ` * null does under php, with a deprecation, but not under PHL's §10 null-strictness` |
|        - |  957 | ` * policy — the screen and this helper both report it as a mismatch.` |
|        - |  958 | ` */` |
|      294 |  959 | `PH7_PRIVATE int PH7_ArgSatisfiesString(ph7_value *pArg)` |
|        2 |  960 | `{` |
|      296 |  961 | `	if( (pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_NULL\|MEMOBJ_RES)) != 0 ){` |
|       11 |  962 | `		return 0;` |
|        - |  963 | `	}` |
|      286 |  964 | `	if( (pArg->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       88 |  965 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|       88 |  966 | `		return pInst && PH7_ClassExtractMethod(pInst->pClass,"__toString",` |
|       43 |  967 | `			sizeof("__toString")-1) != 0;` |
|        - |  968 | `	}` |
|      199 |  969 | `	return 1;` |
|      149 |  970 | `}` |
|        - |  971 | `/*` |
|        - |  972 | ` * PHP-8 ZPP type enforcement for host functions, driven by the aBuiltinSig[]` |
|        - |  973 | ` * declaration (band A #7). Screens only the arguments that php can NEVER coerce` |
|        - |  974 | ` * into a declared scalar parameter — arrays, resources, and objects without a` |
|        - |  975 | ` * __toString() — and throws the catchable TypeError php throws, before the C` |
|        - |  976 | ` * routine runs. Without this an array argument reached the builtin and was` |
|        - |  977 | ` * stringified to the literal "Array" (strrev(['a']) returned "yarrA").` |
|        - |  978 | ` *` |
|        - |  979 | ` * Deliberately narrow: scalar-to-scalar coercion (and its deprecations) stays` |
|        - |  980 | ` * with the per-builtin ZPP helpers. Those emit E_DEPRECATED, which does NOT` |
|        - |  981 | ` * abort the call, so a central copy would double-fire — the trap that sank the` |
|        - |  982 | ` * first central-ZPP attempt. A TypeError aborts, so there is nothing to double.` |
|        - |  983 | ` */` |
|  3936537 |  984 | `PH7_PRIVATE sxi32 VmEnforceBuiltinArgTypes(` |
|        - |  985 | `	ph7_context *pCtx,    /* Call context (for the throw) */` |
|        - |  986 | `	ph7_user_func *pFunc, /* Callee */` |
|        - |  987 | `	int nGiven,           /* Argument count */` |
|        - |  988 | `	ph7_value **apArg     /* Arguments */` |
|        - |  989 | `	)` |
|        5 |  990 | `{` |
|        - |  991 | `	/*` |
|        - |  992 | `	 * Builtins whose own argument check is php-exact and VALUE-based rather than` |
|        - |  993 | `	 * type-based must not be pre-empted here, or their message is lost. Same rule` |
|        - |  994 | `	 * the aBuiltinArity[] table follows: a builtin that already says what php says` |
|        - |  995 | `	 * stays off the shared screen. get_class_vars() takes any stringifiable value` |
|        - |  996 | `	 * and reports "must be a valid class name, Array given".` |
|        - |  997 | `	 *` |
|        - |  998 | `	 * strtr() is here for a structural reason: php declares it as two OVERLOADS` |
|        - |  999 | `	 * dispatched on arity — strtr(string, array) and strtr(string, string, string)` |
|        - | 1000 | `` 	 * — so the expected type of $from is `array` with two arguments and `string` `` |
|        - | 1001 | ``	 * with three. One signature cannot say that (the stub's `array\|string` is the`` |
|        - | 1002 | `	 * union of the two, which is the wording php never uses), so the builtin does` |
|        - | 1003 | `	 * its own dispatch through PH7_ArgSatisfiesString(), the same rule as here.` |
|        - | 1004 | `	 */` |
|        - | 1005 | `	static const char *azSelfChecked[] = { "get_class_vars", "strtr" };` |
|  3936542 | 1006 | `	const char *zSig = pFunc->zSig;` |
|        - | 1007 | `	const char *zCur, *zEnd;` |
|  3936542 | 1008 | `	int iArg = 0;` |
|  3936542 | 1009 | `	if( zSig == 0 ){` |
|  3082363 | 1010 | `		return SXRET_OK;` |
|        - | 1011 | `	}` |
|  2562436 | 1012 | `	for( iArg = 0 ; iArg < (int)SX_ARRAYSIZE(azSelfChecked) ; ++iArg ){` |
|  2563733 | 1013 | `		if( SyStrncmp(pFunc->sName.zString,azSelfChecked[iArg],` |
|  2563733 | 1014 | `			(sxu32)SyStrlen(azSelfChecked[iArg])) == 0` |
|   855436 | 1015 | `		 && pFunc->sName.nByte == SyStrlen(azSelfChecked[iArg]) ){` |
|      102 | 1016 | `			return SXRET_OK;` |
|        - | 1017 | `		}` |
|   855336 | 1018 | `	}` |
|   854084 | 1019 | `	iArg = 0;` |
|   854084 | 1020 | `	zCur = zSig;` |
|   854084 | 1021 | `	zEnd = &zSig[SyStrlen(zSig)];` |
|  2437300 | 1022 | `	while( zCur < zEnd && iArg < nGiven ){` |
|        - | 1023 | `		const char *zType, *zName, *zStop;` |
|        - | 1024 | `		int nType, nName;` |
|        - | 1025 | `		ph7_value *pArg;` |
|        - | 1026 | `		/* Parameter = "<type> $<name>[ = <default>]"; the type is whatever` |
|        - | 1027 | `		 * precedes the '$', and an empty type means "untyped" (no screen). */` |
|  2337876 | 1028 | `		while( zCur < zEnd && zCur[0] == ' ' ){` |
|   752318 | 1029 | `			zCur++;` |
|        5 | 1030 | `		}` |
|  1585563 | 1031 | `		zStop = zCur;` |
| 25336060 | 1032 | `		while( zStop < zEnd && zStop[0] != ',' ){` |
| 23750502 | 1033 | `			zStop++;` |
|        5 | 1034 | `		}` |
|  1585563 | 1035 | `		zName = zCur;` |
| 11897384 | 1036 | `		while( zName < zStop && zName[0] != '$' ){` |
| 10311826 | 1037 | `			zName++;` |
|        5 | 1038 | `		}` |
|  1585563 | 1039 | `		if( zName >= zStop ){` |
|       45 | 1040 | `			break; /* malformed / no parameter name — stop screening */` |
|        - | 1041 | `		}` |
|  1585519 | 1042 | `		if( zName >= zCur + 3 && SyMemcmp(zName - 3,"...",3) == 0 ){` |
|     2085 | 1043 | `			break; /* variadic tail: stop (its type applies to the rest) */` |
|        - | 1044 | `		}` |
|  1583439 | 1045 | `		zType = zCur;` |
|  1583439 | 1046 | `		nType = (int)(zName - zCur);` |
|        - | 1047 | `		/* Trim the trailing spaces and the by-ref marker of "array &$array" */` |
|  3923525 | 1048 | `		while( nType > 0 && (zType[nType-1] == ' ' \|\| zType[nType-1] == '&') ){` |
|  1547609 | 1049 | `			nType--;` |
|        5 | 1050 | `		}` |
|  1583439 | 1051 | `		zName++; /* skip '$' */` |
|  1583439 | 1052 | `		nName = 0;` |
| 11462601 | 1053 | `		while( &zName[nName] < zStop && zName[nName] != ' ' && zName[nName] != '=' ){` |
|  9879167 | 1054 | `			nName++;` |
|        5 | 1055 | `		}` |
|  1583439 | 1056 | `		pArg = apArg[iArg];` |
|  1583439 | 1057 | `		if( nType > 0 && !VmSigTypeHas(zType,nType,"mixed") ){` |
|  1461417 | 1058 | `			const char *zGiven = 0;` |
|  1461417 | 1059 | `			if( (pArg->iFlags & MEMOBJ_HASHMAP) != 0 ){` |
|    77644 | 1060 | `				if( !VmSigTypeHas(zType,nType,"array")` |
|    38897 | 1061 | `				 && !VmSigTypeHas(zType,nType,"iterable")` |
|      155 | 1062 | `				 && !VmSigTypeHas(zType,nType,"callable") ){` |
|      119 | 1063 | `					zGiven = "array";` |
|       62 | 1064 | `				}` |
|  1422595 | 1065 | `			}else if( (pArg->iFlags & MEMOBJ_OBJ) != 0 ){` |
|     1720 | 1066 | `				if( !VmSigTypeHas(zType,nType,"object")` |
|     1166 | 1067 | `				 && !VmSigTypeHas(zType,nType,"iterable")` |
|      612 | 1068 | `				 && !VmSigTypeHas(zType,nType,"callable")` |
|      418 | 1069 | `				 && !VmSigTypeHasClass(zType,nType) ){` |
|        - | 1070 | `					/* An object with __toString() still satisfies a string` |
|        - | 1071 | `					 * parameter in weak mode — php coerces it. */` |
|      148 | 1072 | `					int bStringable = VmSigTypeHas(zType,nType,"string")` |
|      104 | 1073 | `						&& PH7_ArgSatisfiesString(pArg);` |
|      107 | 1074 | `					if( !bStringable ){` |
|       59 | 1075 | `						zGiven = VmArgTypeName(pArg);` |
|       28 | 1076 | `					}` |
|       57 | 1077 | `				}` |
|  1382913 | 1078 | `			}else if( (pArg->iFlags & MEMOBJ_NULL) != 0 ){` |
|        - | 1079 | `				/* php only DEPRECATES null for a non-nullable parameter; PHL rejects` |
|        - | 1080 | `				 * it (scope policy). A leading '?' or an explicit "null" arm in a` |
|        - | 1081 | `				 * union declares the parameter nullable. A "callable" parameter is` |
|        - | 1082 | `				 * left to the builtin's own callback check, which words the failure` |
|        - | 1083 | `				 * php's way ("must be a valid callback, no array or string given") —` |
|        - | 1084 | `				 * the same reason get_class_vars() sits on azSelfChecked[]. */` |
|     5044 | 1085 | `				if( zType[0] != '?'` |
|     2550 | 1086 | `				 && !VmSigTypeHas(zType,nType,"null")` |
|       58 | 1087 | `				 && !VmSigTypeHas(zType,nType,"callable") ){` |
|       50 | 1088 | `					zGiven = "null";` |
|       27 | 1089 | `				}` |
|  1379531 | 1090 | `			}else if( (pArg->iFlags & MEMOBJ_RES) != 0 ){` |
|        - | 1091 | `				/* A class-typed parameter also accepts a resource: several handles php 8` |
|        - | 1092 | `				 * models as objects are still resources here (xml_*'s XMLParser is the` |
|        - | 1093 | `				 * one the signatures already declare php-8-style, for reflection). The` |
|        - | 1094 | `				 * screen would otherwise reject the engine's own parser handle. Recorded` |
|        - | 1095 | `				 * as a divergence in NEWPLAN §7 — it goes away when those handles become` |
|        - | 1096 | `				 * real objects. */` |
|        2 | 1097 | `				if( !VmSigTypeHas(zType,nType,"resource")` |
|        3 | 1098 | `				 && !VmSigTypeHasClass(zType,nType) ){` |
|        3 | 1099 | `					zGiven = "resource";` |
|        1 | 1100 | `				}` |
|        1 | 1101 | `			}` |
|  1461417 | 1102 | `			if( zGiven ){` |
|      332 | 1103 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|        - | 1104 | `					"%z(): Argument #%d ($%.*s) must be of type %.*s, %s given",` |
|      109 | 1105 | `					&pFunc->sName,iArg + 1,nName,zName,nType,zType,zGiven);` |
|        - | 1106 | `			}` |
|   731247 | 1107 | `		}` |
|  1583221 | 1108 | `		zCur = (zStop < zEnd) ? zStop + 1 : zEnd;` |
|  1583221 | 1109 | `		iArg++;` |
|        5 | 1110 | `	}` |
|   853866 | 1111 | `	return SXRET_OK;` |
|  1968876 | 1112 | `}` |
|        - | 1113 | `/*` |
|        - | 1114 | ` * Builtins whose accepted arity is NOT a contiguous range, so the signature` |
|        - | 1115 | ` * cannot express it and the central too-many-arguments check must stay out of` |
|        - | 1116 | ` * the way: rand()/mt_rand() take 0 OR 2 arguments (never 1), and php words the` |
|        - | 1117 | ` * violation "expects exactly 2 arguments, 3 given" from their own check rather` |
|        - | 1118 | ` * than the ZPP "at most". They validate themselves; leaving bHasMaxArg at 0` |
|        - | 1119 | ` * keeps their message php-faithful.` |
|        - | 1120 | ` */` |
|  1626116 | 1121 | `static int VmBuiltinSelfValidatesArity(const char *zName)` |
|        5 | 1122 | `{` |
|        - | 1123 | `	static const char *const azSelf[] = { "rand", "mt_rand" };` |
|        - | 1124 | `	sxu32 i;` |
|  4867415 | 1125 | `	for( i = 0 ; i < SX_ARRAYSIZE(azSelf) ; i++ ){` |
|  3248591 | 1126 | `		sxu32 nSelf = SyStrlen(azSelf[i]);` |
|  3248591 | 1127 | `		if( SyStrlen(zName) == nSelf && SyStrncmp(zName,azSelf[i],nSelf) == 0 ){` |
|     7297 | 1128 | `			return 1;` |
|        - | 1129 | `		}` |
|  1620652 | 1130 | `	}` |
|  1618829 | 1131 | `	return 0;` |
|   813063 | 1132 | `}` |
|        - | 1133 | `/*` |
|        - | 1134 | ` * D1: derive a by-reference position bitmask from a php-style signature string.` |
|        - | 1135 | `` * Bit N is set when positional parameter N is declared by-reference (a `&` appears`` |
|        - | 1136 | `` * anywhere in that comma-separated parameter, e.g. `&$matches`, `&...$vars`). Only`` |
|        - | 1137 | ` * the first 31 positions are representable; a by-ref parameter past that is rare` |
|        - | 1138 | ` * for a builtin and simply not tracked here. The scan mirrors VmDeriveArityFromSig.` |
|        - | 1139 | ` */` |
|  1626116 | 1140 | `static sxu32 VmDeriveByRefMaskFromSig(const char *zSig)` |
|        5 | 1141 | `{` |
|  1626121 | 1142 | `	sxu32 mask = 0;` |
|  1626121 | 1143 | `	int n = 0;       /* current parameter index */` |
|  1626121 | 1144 | `	int bSeen = 0;   /* current parameter has non-space content */` |
|  1626121 | 1145 | ``	int bRef = 0;    /* current parameter carries a by-ref `&` */`` |
|  1626121 | 1146 | `	const char *zCur = zSig;` |
| 25128232 | 1147 | `	for(;;){` |
| 51718515 | 1148 | `		if( zCur[0] == '\0' \|\| zCur[0] == ',' ){` |
|  3088167 | 1149 | `			if( bSeen ){` |
|  2905867 | 1150 | `				if( bRef && n < 31 ){` |
|   145845 | 1151 | `					mask \|= (1u << n);` |
|    72920 | 1152 | `				}` |
|  2905867 | 1153 | `				n++;` |
|  1452931 | 1154 | `			}` |
|  3088167 | 1155 | `			if( zCur[0] == '\0' ){` |
|  1626121 | 1156 | `				break;` |
|        - | 1157 | `			}` |
|  1462051 | 1158 | `			bSeen = bRef = 0;` |
|  1462051 | 1159 | `			zCur++;` |
|  1462051 | 1160 | `			continue;` |
|        - | 1161 | `		}` |
| 48630353 | 1162 | `		if( zCur[0] != ' ' ){` |
| 42231623 | 1163 | `			bSeen = 1;` |
| 21115809 | 1164 | `		}` |
| 48630353 | 1165 | `		if( zCur[0] == '&' ){` |
|   145845 | 1166 | `			bRef = 1;` |
|    72920 | 1167 | `		}` |
| 48630353 | 1168 | `		zCur++;` |
|        5 | 1169 | `	}` |
|  1626121 | 1170 | `	return mask;` |
|        5 | 1171 | `}` |
|     3646 | 1172 | `PH7_PRIVATE void VmSetBuiltinSignatures(ph7_vm *pVm)` |
|        5 | 1173 | `{` |
|        - | 1174 | `	sxu32 n;` |
|  1673519 | 1175 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinSig) ; n++ ){` |
|  2504807 | 1176 | `		SyHashEntry *pEntry = SyHashGet(&pVm->hHostFunction,` |
|  1669868 | 1177 | `			(const void *)aBuiltinSig[n].zName,(sxu32)SyStrlen(aBuiltinSig[n].zName));` |
|  1669873 | 1178 | `		if( pEntry ){` |
|  1626121 | 1179 | `			ph7_user_func *pFunc = (ph7_user_func *)pEntry->pUserData;` |
|  1626121 | 1180 | `			sxi16 nMin = 0, nMax = 0;` |
|  1626121 | 1181 | `			sxu8 bAtLeast = 0, bHasMax = 0;` |
|  1626121 | 1182 | `			pFunc->zSig = aBuiltinSig[n].zSig;` |
|  1626121 | 1183 | `			pFunc->zRet = aBuiltinSig[n].zRet[0] ? aBuiltinSig[n].zRet : 0;` |
|  1626121 | 1184 | `			pFunc->nByRefMask = VmDeriveByRefMaskFromSig(pFunc->zSig);` |
|  1626121 | 1185 | `			VmDeriveArityFromSig(pFunc->zSig,&nMin,&bAtLeast,&nMax,&bHasMax);` |
|        - | 1186 | `			/* The MAXIMUM always comes from the signature: the curated override` |
|        - | 1187 | `			 * table speaks only to the minimum (and its wording). */` |
|  1626121 | 1188 | `			pFunc->nMaxArg = nMax;` |
|  1626121 | 1189 | `			pFunc->bHasMaxArg = (sxu8)(VmBuiltinSelfValidatesArity(aBuiltinSig[n].zName) ? 0 : bHasMax);` |
|  1626121 | 1190 | `			if( pFunc->nMinArg < 1 ){` |
|        - | 1191 | `				/* VmSetBuiltinArity() ran first: a non-zero minimum here means the` |
|        - | 1192 | `				 * curated override already spoke for this builtin, so leave it. */` |
|   678161 | 1193 | `				pFunc->nMinArg = nMin;` |
|   678161 | 1194 | `				pFunc->bAtLeast = bAtLeast;` |
|   339078 | 1195 | `			}` |
|   813058 | 1196 | `		}` |
|   834939 | 1197 | `	}` |
|     3651 | 1198 | `}` |
|        - | 1199 | `/*` |
|        - | 1200 | ` * Signature lookup by function name, for the reflection layer: embedded-PHP` |
|        - | 1201 | ` * builtins (max/min/Exception methods...) are ph7_vm_func instances, not` |
|        - | 1202 | ` * host functions, so they miss the VmSetBuiltinSignatures stamping and pull` |
|        - | 1203 | ` * their row on demand here. Linear scan — reflection-path only.` |
|        - | 1204 | ` */` |
|        4 | 1205 | `PH7_PRIVATE const char * PH7_VmBuiltinSigLookup(const char *zName,sxu32 nLen,const char **pzRet)` |
|        1 | 1206 | `{` |
|        - | 1207 | `	sxu32 n;` |
|        5 | 1208 | `	if( pzRet ){` |
|        5 | 1209 | `		*pzRet = 0;` |
|        2 | 1210 | `	}` |
|     1049 | 1211 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinSig) ; n++ ){` |
|     1048 | 1212 | `		if( SyStrlen(aBuiltinSig[n].zName) == nLen` |
|      543 | 1213 | `		 && SyMemcmp(aBuiltinSig[n].zName,zName,nLen) == 0 ){` |
|        5 | 1214 | `			if( pzRet && aBuiltinSig[n].zRet[0] ){` |
|        5 | 1215 | `				*pzRet = aBuiltinSig[n].zRet;` |
|        2 | 1216 | `			}` |
|        5 | 1217 | `			return aBuiltinSig[n].zSig;` |
|        - | 1218 | `		}` |
|      523 | 1219 | `	}` |
|      ! 0 | 1220 | `	return 0;` |
|        3 | 1221 | `}` |
|        - | 1222 | `/*` |
|        - | 1223 | ` * Write a value back to the caller's variable through a builtin argument's` |
|        - | 1224 | ` * stack-slot nIdx — the shared write-back for builtin by-reference` |
|        - | 1225 | ` * out-parameters (preg_match $matches, preg_replace &$count, similar_text` |
|        - | 1226 | ` * &$percent, ...).` |
|        - | 1227 | ` *` |
|        - | 1228 | ` * For a positional out-param argument the call compiler auto-vivifies known` |
|        - | 1229 | ` * by-reference out-params (see GenStateByRefBuiltinMask in compile.c), so a` |
|        - | 1230 | ` * bare undefined variable, an array subscript, and a declared/untyped` |
|        - | 1231 | ` * property all arrive with a real nIdx and are written back here, matching` |
|        - | 1232 | ` * PHP's reference semantics.` |
|        - | 1233 | ` *` |
|        - | 1234 | ` * nIdx stays SXU32_HIGH and the write-back to the caller is skipped (the` |
|        - | 1235 | ` * value still lands in the local stack slot) when the argument cannot expose` |
|        - | 1236 | ` * a stable memobj slot: a literal, a function-call result, a subscript of a` |
|        - | 1237 | ` * non-lvalue parent (foo()['k']), or any variable in a call that also uses` |
|        - | 1238 | ` * named or spread arguments (compile-time positions no longer map to the` |
|        - | 1239 | ` * runtime arg slots, so the compiler conservatively does not vivify). An` |
|        - | 1240 | ` * uninitialized typed property is also not wired (it throws before the` |
|        - | 1241 | ` * write) -- see the recorded deferrals.` |
|        - | 1242 | ` */` |
|      356 | 1243 | `PH7_PRIVATE void PH7_VmStoreArgByRef(ph7_vm *pVm,ph7_value *pArg,ph7_value *pNewVal)` |
|        5 | 1244 | `{` |
|      361 | 1245 | `	if( pArg->nIdx != SXU32_HIGH ){` |
|      359 | 1246 | `		ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pArg->nIdx);` |
|      359 | 1247 | `		if( pObj ){` |
|      359 | 1248 | `			PH7_MemObjStore(pNewVal,pObj);` |
|      177 | 1249 | `		}` |
|      177 | 1250 | `	}` |
|      361 | 1251 | `	PH7_MemObjStore(pNewVal,pArg);` |
|      361 | 1252 | `}` |
|        - | 1253 | `/*` |
|        - | 1254 | ` * Raise an E_WARNING whose text is used VERBATIM.` |
|        - | 1255 | ` * ph7_context_throw_error_format() prepends "func(): " to whatever it is given, but php's` |
|        - | 1256 | ` * IO warnings put the offending path inside those parens -- "file_get_contents(/nope):` |
|        - | 1257 | ` * Failed to open stream: ..." -- so a caller that needs php's exact shape must build the` |
|        - | 1258 | ` * whole line itself and come through here.` |
|        - | 1259 | ` */` |
|        - | 1260 | `/*` |
|        - | 1261 | ` * Validate a callback argument and throw php's TypeError when it cannot be called.` |
|        - | 1262 | ` *` |
|        - | 1263 | ` * The shared dispatcher (PH7_VmCallUserFunctionWithMap) answers SXRET_OK with a NULL` |
|        - | 1264 | ` * result for an unresolvable callable, so every caller that did not check first failed` |
|        - | 1265 | ` * SILENTLY: call_user_func() returned NULL and usort() left the array sorted by nothing` |
|        - | 1266 | ` * at all. php rejects the ARGUMENT up front, naming exactly why.` |
|        - | 1267 | ` */` |
|      172 | 1268 | `PH7_PRIVATE sxi32 PH7_CheckCallbackArg(` |
|        - | 1269 | `	ph7_context *pCtx,   /* Calling context (names the function in the message) */` |
|        - | 1270 | `	ph7_value *pCb,      /* The callback argument */` |
|        - | 1271 | `	int iArg,            /* Its 1-based position */` |
|        - | 1272 | `	const char *zParam,  /* Its php parameter name, e.g. "callback" */` |
|        - | 1273 | `	int bNullable        /* TRUE when php's text says "or null" */` |
|        - | 1274 | `	)` |
|        3 | 1275 | `{` |
|      175 | 1276 | `	const char *zOrNull = bNullable ? " or null" : "";` |
|      175 | 1277 | `	if( ph7_value_is_callable(pCb) ){` |
|      157 | 1278 | `		return PH7_OK;` |
|        - | 1279 | `	}` |
|       19 | 1280 | `	if( ph7_value_is_string(pCb) ){` |
|        - | 1281 | `		int nLen;` |
|       11 | 1282 | `		const char *zName = ph7_value_to_string(pCb,&nLen);` |
|       16 | 1283 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|        - | 1284 | `			"%s(): Argument #%d ($%s) must be a valid callback%s, function \"%.*s\" not found or invalid function name",` |
|        5 | 1285 | `			ph7_function_name(pCtx),iArg,zParam,zOrNull,nLen,zName);` |
|        - | 1286 | `	}` |
|        9 | 1287 | `	if( ph7_value_is_array(pCb) ){` |
|        7 | 1288 | `		ph7_hashmap *pMap = (ph7_hashmap *)pCb->x.pOther;` |
|        7 | 1289 | `		if( pMap == 0 \|\| pMap->nEntry != 2 ){` |
|        4 | 1290 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|        - | 1291 | `				"%s(): Argument #%d ($%s) must be a valid callback%s, array callback must have exactly two members",` |
|        1 | 1292 | `				ph7_function_name(pCtx),iArg,zParam,zOrNull);` |
|      ! 0 | 1293 | `		}else{` |
|        5 | 1294 | `			ph7_vm *pVm = pCtx->pVm;` |
|        5 | 1295 | `			ph7_value *pCls = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|        5 | 1296 | `			ph7_value *pMeth = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|        5 | 1297 | `			ph7_class *pClass = pCls ? PH7_VmExtractClassFromValue(&(*pVm),pCls) : 0;` |
|        5 | 1298 | `			if( pClass == 0 ){` |
|        5 | 1299 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|        - | 1300 | `					"%s(): Argument #%d ($%s) must be a valid callback%s, class \"%.*s\" not found",` |
|        1 | 1301 | `					ph7_function_name(pCtx),iArg,zParam,zOrNull,` |
|        2 | 1302 | `					pCls ? (int)SyBlobLength(&pCls->sBlob) : 0,` |
|        1 | 1303 | `					pCls ? (const char *)SyBlobData(&pCls->sBlob) : "");` |
|        - | 1304 | `			}` |
|        5 | 1305 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|        - | 1306 | `				"%s(): Argument #%d ($%s) must be a valid callback%s, class %z does not have a method \"%.*s\"",` |
|        1 | 1307 | `				ph7_function_name(pCtx),iArg,zParam,zOrNull,&pClass->sName,` |
|        2 | 1308 | `				pMeth ? (int)SyBlobLength(&pMeth->sBlob) : 0,` |
|        1 | 1309 | `				pMeth ? (const char *)SyBlobData(&pMeth->sBlob) : "");` |
|        - | 1310 | `		}` |
|        - | 1311 | `	}` |
|        4 | 1312 | `	return PH7_VmThrowException(pCtx,"TypeError",` |
|        - | 1313 | `		"%s(): Argument #%d ($%s) must be a valid callback%s, no array or string given",` |
|        1 | 1314 | `		ph7_function_name(pCtx),iArg,zParam,zOrNull);` |
|       89 | 1315 | `}` |
|    20956 | 1316 | `PH7_PRIVATE void PH7_VmThrowWarningFmt(ph7_vm *pVm,const char *zFmt,...)` |
|        5 | 1317 | `{` |
|        - | 1318 | `	va_list ap;` |
|    20961 | 1319 | `	va_start(ap,zFmt);` |
|    20961 | 1320 | `	PH7_VmThrowErrorAp(pVm,0,PH7_CTX_WARNING,zFmt,ap);` |
|    20961 | 1321 | `	va_end(ap);` |
|    20961 | 1322 | `}` |
|        - | 1323 | `/*` |
|        - | 1324 | ` * Emit a formatted E_USER_DEPRECATED diagnostic with no function-name prefix:` |
|        - | 1325 | ` * php reports #[\Deprecated] as USER-deprecated (16384, it is userland-authored),` |
|        - | 1326 | ` * not the engine's E_DEPRECATED (8192) — handler-visible errno matters.` |
|        - | 1327 | ` */` |
|       34 | 1328 | `static void VmThrowUserDeprecatedFmt(ph7_vm *pVm,const char *zFmt,...)` |
|        1 | 1329 | `{` |
|        - | 1330 | `	va_list ap;` |
|       35 | 1331 | `	va_start(ap,zFmt);` |
|       35 | 1332 | `	PH7_VmThrowErrorAp(pVm,0,E_USER_DEPRECATED,zFmt,ap);` |
|       35 | 1333 | `	va_end(ap);` |
|       35 | 1334 | `}` |
|        - | 1335 | `/*` |
|        - | 1336 | ` * php 8.4 #[\Deprecated]: emit the E_USER_DEPRECATED notice for a call to a user` |
|        - | 1337 | ` * function/method carrying the attribute. Message shapes (php-exact):` |
|        - | 1338 | ` *   Function f() is deprecated` |
|        - | 1339 | ` *   Method C::m() is deprecated since 2.0, use g() instead` |
|        - | 1340 | ` * ("since" from the attribute's since: argument; the trailing ", msg" from` |
|        - | 1341 | ` * message:/positional #1.) The attribute arguments are compiled constant` |
|        - | 1342 | ` * expressions — evaluated via VmLocalExec, the reflection thunks' pattern.` |
|        - | 1343 | ` * Called from OP_CALL once per call, only when the callee HAS attributes;` |
|        - | 1344 | ` * pDeclClass names the method's declaring class (0 for plain functions).` |
|        - | 1345 | ` */` |
|        - | 1346 | `/*` |
|        - | 1347 | ` * Expand a global constant's value, emitting the php 8.5 #[\Deprecated]` |
|        - | 1348 | ` * E_USER_DEPRECATED notice first when the constant carries attributes` |
|        - | 1349 | `` * (`Constant GD is deprecated since 1.2` — every access re-warns).`` |
|        - | 1350 | ` */` |
|        - | 1351 | `static void VmDeprecatedAttrNoticeSubject(ph7_vm *pVm,SySet *pAttrs,` |
|        - | 1352 | `	const char *zKind,const SyString *pQual,const SyString *pName);` |
|        - | 1353 | `/*` |
|        - | 1354 | ` * Expand a constant, emitting the php 8.5 #[\Deprecated] E_USER_DEPRECATED notice` |
|        - | 1355 | `` * first when a USERLAND constant carries the attribute (`Constant GD is deprecated`` |
|        - | 1356 | `` * since 1.2` — every access re-warns). Engine constants php merely deprecates are`` |
|        - | 1357 | ` * not mimicked: PHL removes them outright (see the scope policy), so there is no` |
|        - | 1358 | ` * engine-side E_DEPRECATED list here.` |
|        - | 1359 | ` */` |
|   125020 | 1360 | `PH7_PRIVATE void VmExpandConstantWithNotice(ph7_vm *pVm,ph7_constant *pCons,ph7_value *pOut)` |
|        5 | 1361 | `{` |
|   125025 | 1362 | `	if( SySetUsed(&pCons->aAttrs) > 0 ){` |
|        7 | 1363 | `		VmDeprecatedAttrNoticeSubject(pVm,&pCons->aAttrs,"Constant",0,&pCons->sName);` |
|        3 | 1364 | `	}` |
|   125025 | 1365 | `	pCons->xExpand(pOut,pCons->pUserData);` |
|   125025 | 1366 | `}` |
|        - | 1367 | `/*` |
|        - | 1368 | ` * Scan a declared-attribute set for #[\Deprecated]; when found, evaluate its` |
|        - | 1369 | ` * message:/since: arguments (positional #0 = message, #1 = since) into the` |
|        - | 1370 | ` * caller's values and return TRUE. The base subject text ("Function f()",` |
|        - | 1371 | ` * "Constant C::K") is the caller's business.` |
|        - | 1372 | ` */` |
|      130 | 1373 | `static int VmDeprecatedAttrExtract(ph7_vm *pVm,SySet *pAttrs,` |
|        - | 1374 | `	ph7_value *pMsg,int *pbMsg,ph7_value *pSince,int *pbSince)` |
|        5 | 1375 | `{` |
|      135 | 1376 | `	ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|        - | 1377 | `	sxu32 n;` |
|      135 | 1378 | `	*pbMsg = *pbSince = 0;` |
|      235 | 1379 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; ++n ){` |
|      139 | 1380 | `		ph7_attribute *pAttr = &aAttr[n];` |
|        - | 1381 | `		ph7_attr_arg *aArg;` |
|      139 | 1382 | `		sxu32 i,nPos = 0;` |
|      134 | 1383 | `		if( SyStringLength(&pAttr->sName) != sizeof("Deprecated")-1` |
|       89 | 1384 | `		 \|\| SyStrnicmp(SyStringData(&pAttr->sName),"Deprecated",sizeof("Deprecated")-1) != 0 ){` |
|      105 | 1385 | `			continue;` |
|        - | 1386 | `		}` |
|       35 | 1387 | `		aArg = (ph7_attr_arg *)SySetBasePtr(&pAttr->aArgs);` |
|       53 | 1388 | `		for( i = 0 ; i < SySetUsed(&pAttr->aArgs) ; ++i ){` |
|       19 | 1389 | `			ph7_attr_arg *pArg = &aArg[i];` |
|       19 | 1390 | `			int isMsg = 0,isSince = 0;` |
|       19 | 1391 | `			if( SyStringLength(&pArg->sName) == 0 ){` |
|        3 | 1392 | `				isMsg = (nPos == 0);` |
|        3 | 1393 | `				isSince = (nPos == 1);` |
|        3 | 1394 | `				nPos++;` |
|       18 | 1395 | `			}else if( SyStringLength(&pArg->sName) == sizeof("message")-1` |
|       12 | 1396 | `			 && SyMemcmp(SyStringData(&pArg->sName),"message",sizeof("message")-1) == 0 ){` |
|        7 | 1397 | `				isMsg = 1;` |
|       14 | 1398 | `			}else if( SyStringLength(&pArg->sName) == sizeof("since")-1` |
|       11 | 1399 | `			 && SyMemcmp(SyStringData(&pArg->sName),"since",sizeof("since")-1) == 0 ){` |
|       11 | 1400 | `				isSince = 1;` |
|        5 | 1401 | `			}` |
|       19 | 1402 | `			if( isMsg && !*pbMsg && SySetUsed(&pArg->aByteCode) > 0 ){` |
|       13 | 1403 | `				if( VmLocalExec(pVm,&pArg->aByteCode,pMsg,FALSE) == SXRET_OK ){` |
|        9 | 1404 | `					if( (pMsg->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 | 1405 | `						PH7_MemObjToString(pMsg);` |
|      ! 0 | 1406 | `					}` |
|        9 | 1407 | `					*pbMsg = 1;` |
|        5 | 1408 | `				}` |
|       15 | 1409 | `			}else if( isSince && !*pbSince && SySetUsed(&pArg->aByteCode) > 0 ){` |
|       11 | 1410 | `				if( VmLocalExec(pVm,&pArg->aByteCode,pSince,FALSE) == SXRET_OK ){` |
|       11 | 1411 | `					if( (pSince->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 | 1412 | `						PH7_MemObjToString(pSince);` |
|      ! 0 | 1413 | `					}` |
|       11 | 1414 | `					*pbSince = 1;` |
|        5 | 1415 | `				}` |
|        5 | 1416 | `			}` |
|       10 | 1417 | `		}` |
|       35 | 1418 | `		return 1;` |
|      ! 0 | 1419 | `	}` |
|      101 | 1420 | `	return 0;` |
|       70 | 1421 | `}` |
|        - | 1422 | `/*` |
|        - | 1423 | ` * Append php's " since X" / ", message" suffixes to a built base subject and` |
|        - | 1424 | ` * emit the E_USER_DEPRECATED notice.` |
|        - | 1425 | ` */` |
|       34 | 1426 | `static void VmDeprecatedEmit(ph7_vm *pVm,SyBlob *pOut,` |
|        - | 1427 | `	ph7_value *pMsg,int bMsg,ph7_value *pSince,int bSince)` |
|        1 | 1428 | `{` |
|       35 | 1429 | `	if( bSince && SyBlobLength(&pSince->sBlob) > 0 ){` |
|       16 | 1430 | `		SyBlobFormat(pOut," since %.*s",(int)SyBlobLength(&pSince->sBlob),` |
|       10 | 1431 | `			(const char *)SyBlobData(&pSince->sBlob));` |
|        5 | 1432 | `	}` |
|       35 | 1433 | `	if( bMsg && SyBlobLength(&pMsg->sBlob) > 0 ){` |
|       13 | 1434 | `		SyBlobFormat(pOut,", %.*s",(int)SyBlobLength(&pMsg->sBlob),` |
|        8 | 1435 | `			(const char *)SyBlobData(&pMsg->sBlob));` |
|        4 | 1436 | `	}` |
|       35 | 1437 | `	VmThrowUserDeprecatedFmt(pVm,"%.*s",(int)SyBlobLength(pOut),(const char *)SyBlobData(pOut));` |
|       35 | 1438 | `}` |
|        - | 1439 | `/*` |
|        - | 1440 | ` * Generic #[\Deprecated] notice for a named subject:` |
|        - | 1441 | ` * "<Kind> [Qual::]Name is deprecated[ since X][, message]".` |
|        - | 1442 | ` */` |
|       18 | 1443 | `static void VmDeprecatedAttrNoticeSubject(ph7_vm *pVm,SySet *pAttrs,` |
|        - | 1444 | `	const char *zKind,const SyString *pQual,const SyString *pName)` |
|        1 | 1445 | `{` |
|        - | 1446 | `	ph7_value sMsg,sSince;` |
|        - | 1447 | `	SyBlob sOut;` |
|        - | 1448 | `	int bMsg,bSince;` |
|       19 | 1449 | `	PH7_MemObjInit(pVm,&sMsg);` |
|       19 | 1450 | `	PH7_MemObjInit(pVm,&sSince);` |
|       19 | 1451 | `	if( VmDeprecatedAttrExtract(pVm,pAttrs,&sMsg,&bMsg,&sSince,&bSince) ){` |
|       15 | 1452 | `		SyBlobInit(&sOut,&pVm->sAllocator);` |
|       15 | 1453 | `		if( pQual ){` |
|       11 | 1454 | `			SyBlobFormat(&sOut,"%s %z::%z is deprecated",zKind,pQual,pName);` |
|        6 | 1455 | `		}else{` |
|        5 | 1456 | `			SyBlobFormat(&sOut,"%s %z is deprecated",zKind,pName);` |
|        - | 1457 | `		}` |
|       15 | 1458 | `		VmDeprecatedEmit(pVm,&sOut,&sMsg,bMsg,&sSince,bSince);` |
|       15 | 1459 | `		SyBlobRelease(&sOut);` |
|        7 | 1460 | `	}` |
|       19 | 1461 | `	PH7_MemObjRelease(&sMsg);` |
|       19 | 1462 | `	PH7_MemObjRelease(&sSince);` |
|       19 | 1463 | `}` |
|      112 | 1464 | `PH7_PRIVATE void VmDeprecatedAttrNotice(ph7_vm *pVm,ph7_vm_func *pFunc,ph7_class *pDeclClass)` |
|        5 | 1465 | `{` |
|        - | 1466 | `	ph7_value sMsg,sSince;` |
|        - | 1467 | `	SyBlob sOut;` |
|        - | 1468 | `	int bMsg,bSince;` |
|      117 | 1469 | `	PH7_MemObjInit(pVm,&sMsg);` |
|      117 | 1470 | `	PH7_MemObjInit(pVm,&sSince);` |
|      117 | 1471 | `	if( VmDeprecatedAttrExtract(pVm,&pFunc->aAttrs,&sMsg,&bMsg,&sSince,&bSince) ){` |
|       21 | 1472 | `		SyBlobInit(&sOut,&pVm->sAllocator);` |
|       21 | 1473 | `		if( pDeclClass ){` |
|        5 | 1474 | `			SyBlobFormat(&sOut,"Method %z::%z() is deprecated",&pDeclClass->sName,&pFunc->sName);` |
|        3 | 1475 | `		}else{` |
|       17 | 1476 | `			SyBlobFormat(&sOut,"Function %z() is deprecated",&pFunc->sName);` |
|        - | 1477 | `		}` |
|       21 | 1478 | `		VmDeprecatedEmit(pVm,&sOut,&sMsg,bMsg,&sSince,bSince);` |
|       21 | 1479 | `		SyBlobRelease(&sOut);` |
|       10 | 1480 | `	}` |
|      117 | 1481 | `	PH7_MemObjRelease(&sMsg);` |
|      117 | 1482 | `	PH7_MemObjRelease(&sSince);` |
|      117 | 1483 | `}` |
|        - | 1484 | `/*` |
|        - | 1485 | ` * Same notice for a #[\Deprecated] class constant, at its static-access site:` |
|        - | 1486 | `` * php's `Constant C::K is deprecated` (each access re-warns).`` |
|        - | 1487 | ` */` |
|       12 | 1488 | `PH7_PRIVATE void VmDeprecatedConstNotice(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pMember)` |
|        1 | 1489 | `{` |
|       19 | 1490 | `	VmDeprecatedAttrNoticeSubject(pVm,&pMember->aAttrs,` |
|       12 | 1491 | `		(pMember->iFlags & PH7_CLASS_ATTR_ENUMCASE) ? "Enum case" : "Constant",` |
|       12 | 1492 | `		&pClass->sName,&pMember->sName);` |
|       13 | 1493 | `}` |
|        - | 1494 |  |
