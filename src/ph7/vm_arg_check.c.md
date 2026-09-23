# src/ph7/vm_arg_check.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 812/871 lines (93.23%)

[Root index](../../index.md) | [Directory index](index.md)

|      Hits | Line | Source |
| --------: | ---: | :--- |
|         - |    1 | `/**` |
|         - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|         - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|         - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|         - |    5 | ` */` |
|         - |    6 | `#include "ph7int.h"` |
|         - |    7 | `/*` |
|         - |    8 | ` * Section:` |
|         - |    9 | ` *    Builtin-function argument checking: the aBuiltinArity[] min-arity` |
|         - |   10 | ` *    overrides, the aBuiltinSig[] PHP-8.5 signature table (the single` |
|         - |   11 | ` *    source of truth for builtin arity/types and Reflection), ZPP type` |
|         - |   12 | ` *    enforcement for host functions and the deprecation-notice machinery.` |
|         - |   13 | ` * Status:` |
|         - |   14 | ` *    Stable.` |
|         - |   15 | ` */` |
|         - |   16 | `/*` |
|         - |   17 | ` * PHP-8 builtin minimum-arity table (band A #5, stage 1).` |
|         - |   18 | ` *` |
|         - |   19 | ` * Native builtins carry no formal-parameter signature, so historically each` |
|         - |   20 | ` * one self-validated its argument count (or, worse, silently degraded to a` |
|         - |   21 | ` * bogus false/-1/"" return on too few arguments — a PH7-ism that diverges from` |
|         - |   22 | ` * PHP 8, which throws a catchable ArgumentCountError). This table is the single` |
|         - |   23 | ` * source of truth for the required minimum: at VM init VmSetBuiltinArity()` |
|         - |   24 | ` * stamps nMinArg/bAtLeast onto the matching ph7_user_func, and the OP_CALL` |
|         - |   25 | ` * choke point throws ArgumentCountError before the C routine ever runs.` |
|         - |   26 | ` *` |
|         - |   27 | ` * bAtLeast mirrors PHP's ZPP wording: "expects exactly N" when the builtin has` |
|         - |   28 | ` * no optional/variadic parameters (min == max), "expects at least N" otherwise.` |
|         - |   29 | ` * Every entry's min count and wording is byte-verified against php 8.5.7.` |
|         - |   30 | ` *` |
|         - |   31 | ` * Only functions that currently mis-behave (silent wrong return) are listed;` |
|         - |   32 | ` * builtins that already self-throw the correct message are intentionally left` |
|         - |   33 | ` * out so there is no double-check / message drift. The batch-1 block below is` |
|         - |   34 | ` * the original 31-function seed; the batch-2 block that follows completes the` |
|         - |   35 | ` * sweep across every remaining silent-degrading builtin (verified against the` |
|         - |   36 | ` * php 8.5.7 oracle).` |
|         - |   37 | ` */` |
|         - |   38 | `static const struct VmBuiltinArity {` |
|         - |   39 | `	const char *zName;   /* Builtin name (short, unqualified) */` |
|         - |   40 | `	sxi16 nMin;          /* Minimum required arguments */` |
|         - |   41 | `	sxu8 bAtLeast;       /* 0 -> "exactly", 1 -> "at least" */` |
|         - |   42 | `} aBuiltinArity[] = {` |
|         - |   43 | `	/* String family */` |
|         - |   44 | `	{ "substr",       2, 1 }, { "substr_count",  2, 1 }, { "str_repeat",     2, 0 },` |
|         - |   45 | `	{ "str_pad",      2, 1 }, { "strpos",        2, 1 }, { "stripos",        2, 1 },` |
|         - |   46 | `	{ "strrpos",      2, 1 }, { "strripos",      2, 1 }, { "strstr",         2, 1 },` |
|         - |   47 | `	{ "stristr",      2, 1 }, { "strrchr",       2, 1 }, { "str_replace",    3, 1 },` |
|         - |   48 | `	{ "str_ireplace", 3, 1 }, { "strncmp",       3, 0 }, { "strncasecmp",    3, 0 },` |
|         - |   49 | `	{ "substr_compare",3,1 }, { "strpbrk",       2, 0 }, { "strspn",         2, 1 },` |
|         - |   50 | `	{ "strcspn",      2, 1 }, { "hexdec",        1, 0 }, { "octdec",         1, 0 },` |
|         - |   51 | `	{ "bindec",       1, 0 }, { "chunk_split",   1, 1 },` |
|         - |   52 | `	/* Math family (atan2/intdiv already self-throw the same ArgumentCountError,` |
|         - |   53 | `	 * so they stay off the table per the disjointness rule above). */` |
|         - |   54 | `	{ "pow",          2, 0 }, { "fmod",          2, 0 }, { "hypot",          2, 0 },` |
|         - |   55 | `	{ "log",          1, 1 },` |
|         - |   56 | `	/* Array family (str_split already self-throws — kept off the table). */` |
|         - |   57 | `	{ "in_array",     2, 1 }, { "range",         2, 1 },` |
|         - |   58 | `	{ "implode",      1, 1 }, { "join",          1, 1 },` |
|         - |   59 | `	/*` |
|         - |   60 | `	 * Batch 2 (band A #5 continuation) — a systematic sweep of every remaining` |
|         - |   61 | `	 * builtin that silently degraded on too-few arguments where php 8 throws` |
|         - |   62 | `	 * ArgumentCountError. Each row's minimum and "exactly"/"at least" wording was` |
|         - |   63 | `	 * extracted from php 8.5.7's own ArgumentCountError message at the argument` |
|         - |   64 | `	 * boundary (the message text is byte-identical to the one this table drives).` |
|         - |   65 | `	 * Functions that already self-throw the correct message are still excluded per` |
|         - |   66 | `	 * the disjointness rule above.` |
|         - |   67 | `	 */` |
|         - |   68 | `	/* String family */` |
|         - |   69 | `	{ "chop",                      1, 1 },` |
|         - |   70 | `	{ "explode",                   2, 1 },` |
|         - |   71 | `	{ "fprintf",                   2, 1 },` |
|         - |   72 | `	{ "html_entity_decode",        1, 1 },` |
|         - |   73 | `	{ "htmlentities",              1, 1 },` |
|         - |   74 | `	{ "htmlspecialchars",          1, 1 },` |
|         - |   75 | `	{ "htmlspecialchars_decode",   1, 1 },` |
|         - |   76 | `	{ "lcfirst",                   1, 0 },` |
|         - |   77 | `	{ "ltrim",                     1, 1 },` |
|         - |   78 | `	{ "mb_check_encoding",         1, 0 },` |
|         - |   79 | `	{ "mb_chr",                    1, 1 },` |
|         - |   80 | `	{ "mb_convert_case",           2, 1 },` |
|         - |   81 | `	{ "mb_convert_encoding",       2, 1 },` |
|         - |   82 | `	{ "mb_detect_encoding",        1, 1 },` |
|         - |   83 | `	{ "mb_ord",                    1, 1 },` |
|         - |   84 | `	{ "mb_str_split",              1, 1 },` |
|         - |   85 | `	{ "mb_stripos",                2, 1 },` |
|         - |   86 | `	{ "mb_strlen",                 1, 1 },` |
|         - |   87 | `	{ "mb_strpos",                 2, 1 },` |
|         - |   88 | `	{ "mb_strrpos",                2, 1 },` |
|         - |   89 | `	{ "mb_strtolower",             1, 1 },` |
|         - |   90 | `	{ "mb_strtoupper",             1, 1 },` |
|         - |   91 | `	{ "mb_strwidth",               1, 1 },` |
|         - |   92 | `	{ "mb_substr",                 2, 1 },` |
|         - |   93 | `	{ "nl2br",                     1, 1 },` |
|         - |   94 | `	{ "printf",                    1, 1 },` |
|         - |   95 | `	{ "quotemeta",                 1, 0 },` |
|         - |   96 | `	{ "rtrim",                     1, 1 },` |
|         - |   97 | `	{ "soundex",                   1, 0 },` |
|         - |   98 | `	{ "sprintf",                   1, 1 },` |
|         - |   99 | `	{ "str_getcsv",                1, 1 },` |
|         - |  100 | `	{ "str_shuffle",               1, 0 },` |
|         - |  101 | `	{ "strcasecmp",                2, 0 },` |
|         - |  102 | `	{ "strchr",                    2, 1 },` |
|         - |  103 | `	{ "strcmp",                    2, 0 },` |
|         - |  104 | `	{ "strnatcasecmp",             2, 0 },` |
|         - |  105 | `	{ "strnatcmp",                 2, 0 },` |
|         - |  106 | `	{ "strcoll",                   2, 0 },` |
|         - |  107 | `	{ "strip_tags",                1, 1 },` |
|         - |  108 | `	{ "stripslashes",              1, 0 },` |
|         - |  109 | `	{ "strlen",                    1, 0 },` |
|         - |  110 | `	{ "strrev",                    1, 0 },` |
|         - |  111 | `	{ "strtok",                    1, 1 },` |
|         - |  112 | `	{ "strtolower",                1, 0 },` |
|         - |  113 | `	{ "strtoupper",                1, 0 },` |
|         - |  114 | `	{ "strtr",                     2, 0 },` |
|         - |  115 | `	{ "trim",                      1, 1 },` |
|         - |  116 | `	{ "ucfirst",                   1, 0 },` |
|         - |  117 | `	{ "ucwords",                   1, 1 },` |
|         - |  118 | `	{ "vfprintf",                  3, 0 },` |
|         - |  119 | `	{ "vprintf",                   2, 0 },` |
|         - |  120 | `	{ "vsprintf",                  2, 0 },` |
|         - |  121 | `	{ "wordwrap",                  1, 1 },` |
|         - |  122 | `	/* Ctype family */` |
|         - |  123 | `	{ "ctype_alnum",               1, 0 },` |
|         - |  124 | `	{ "ctype_alpha",               1, 0 },` |
|         - |  125 | `	{ "ctype_cntrl",               1, 0 },` |
|         - |  126 | `	{ "ctype_digit",               1, 0 },` |
|         - |  127 | `	{ "ctype_graph",               1, 0 },` |
|         - |  128 | `	{ "ctype_lower",               1, 0 },` |
|         - |  129 | `	{ "ctype_print",               1, 0 },` |
|         - |  130 | `	{ "ctype_punct",               1, 0 },` |
|         - |  131 | `	{ "ctype_space",               1, 0 },` |
|         - |  132 | `	{ "ctype_upper",               1, 0 },` |
|         - |  133 | `	{ "ctype_xdigit",              1, 0 },` |
|         - |  134 | `	/* Math family */` |
|         - |  135 | `	{ "base_convert",              3, 0 },` |
|         - |  136 | `	{ "cos",                       1, 0 },` |
|         - |  137 | `	{ "cosh",                      1, 0 },` |
|         - |  138 | `	{ "crc32",                     1, 0 },` |
|         - |  139 | `	{ "decbin",                    1, 0 },` |
|         - |  140 | `	{ "dechex",                    1, 0 },` |
|         - |  141 | `	{ "decoct",                    1, 0 },` |
|         - |  142 | `	{ "exp",                       1, 0 },` |
|         - |  143 | `	{ "log10",                     1, 0 },` |
|         - |  144 | `	{ "md5",                       1, 1 },` |
|         - |  145 | `	{ "round",                     1, 1 },` |
|         - |  146 | `	{ "sha1",                      1, 1 },` |
|         - |  147 | `	{ "sin",                       1, 0 },` |
|         - |  148 | `	{ "sinh",                      1, 0 },` |
|         - |  149 | `	{ "sqrt",                      1, 0 },` |
|         - |  150 | `	{ "tan",                       1, 0 },` |
|         - |  151 | `	{ "tanh",                      1, 0 },` |
|         - |  152 | `	/* Type/var family */` |
|         - |  153 | `	{ "floatval",                  1, 0 },` |
|         - |  154 | `	{ "get_resource_id",           1, 0 },` |
|         - |  155 | `	{ "get_resource_type",         1, 0 },` |
|         - |  156 | `	{ "gettype",                   1, 0 },` |
|         - |  157 | `	{ "intval",                    1, 1 },` |
|         - |  158 | `	{ "is_array",                  1, 0 },` |
|         - |  159 | `	{ "is_bool",                   1, 0 },` |
|         - |  160 | `	{ "is_callable",               1, 1 },` |
|         - |  161 | `	{ "is_double",                 1, 0 },` |
|         - |  162 | `	{ "is_float",                  1, 0 },` |
|         - |  163 | `	{ "is_int",                    1, 0 },` |
|         - |  164 | `	{ "is_integer",                1, 0 },` |
|         - |  165 | `	{ "is_long",                   1, 0 },` |
|         - |  166 | `	{ "is_null",                   1, 0 },` |
|         - |  167 | `	{ "is_numeric",                1, 0 },` |
|         - |  168 | `	{ "is_object",                 1, 0 },` |
|         - |  169 | `	{ "is_resource",               1, 0 },` |
|         - |  170 | `	{ "is_scalar",                 1, 0 },` |
|         - |  171 | `	{ "is_string",                 1, 0 },` |
|         - |  172 | `	{ "print_r",                   1, 1 },` |
|         - |  173 | `	{ "strval",                    1, 0 },` |
|         - |  174 | `	{ "var_dump",                  1, 1 },` |
|         - |  175 | `	{ "var_export",                1, 1 },` |
|         - |  176 | `	/* Array/iterator family */` |
|         - |  177 | `	{ "array_filter",              1, 1 },` |
|         - |  178 | `	{ "array_product",             1, 0 },` |
|         - |  179 | `	{ "array_rand",                1, 1 },` |
|         - |  180 | `	{ "compact",                   1, 1 },` |
|         - |  181 | `	{ "current",                   1, 0 },` |
|         - |  182 | `	{ "end",                       1, 0 },` |
|         - |  183 | `	{ "extract",                   1, 1 },` |
|         - |  184 | `	{ "iterator_apply",            2, 1 },` |
|         - |  185 | `	{ "iterator_count",            1, 0 },` |
|         - |  186 | `	{ "iterator_to_array",         1, 1 },` |
|         - |  187 | `	{ "key",                       1, 0 },` |
|         - |  188 | `	{ "krsort",                    1, 1 },` |
|         - |  189 | `	{ "ksort",                     1, 1 },` |
|         - |  190 | `	{ "next",                      1, 0 },` |
|         - |  191 | `	{ "pos",                       1, 0 },` |
|         - |  192 | `	{ "prev",                      1, 0 },` |
|         - |  193 | `	{ "reset",                     1, 0 },` |
|         - |  194 | `	{ "rsort",                     1, 1 },` |
|         - |  195 | `	{ "shuffle",                   1, 0 },` |
|         - |  196 | `	{ "sort",                      1, 1 },` |
|         - |  197 | `	{ "uasort",                    2, 0 },` |
|         - |  198 | `	{ "uksort",                    2, 0 },` |
|         - |  199 | `	{ "usort",                     2, 0 },` |
|         - |  200 | `	/* Class/reflection family */` |
|         - |  201 | `	{ "class_alias",               2, 1 },` |
|         - |  202 | `	{ "class_exists",              1, 1 },` |
|         - |  203 | `	{ "enum_exists",               1, 1 },` |
|         - |  204 | `	{ "get_class_methods",         1, 0 },` |
|         - |  205 | `	{ "get_class_vars",            1, 0 },` |
|         - |  206 | `	{ "get_object_vars",           1, 0 },` |
|         - |  207 | `	{ "interface_exists",          1, 1 },` |
|         - |  208 | `	{ "trait_exists",              1, 1 },` |
|         - |  209 | `	{ "is_a",                      2, 1 },` |
|         - |  210 | `	{ "is_subclass_of",            2, 1 },` |
|         - |  211 | `	{ "method_exists",             2, 0 },` |
|         - |  212 | `	{ "property_exists",           2, 0 },` |
|         - |  213 | `	{ "spl_autoload",              1, 1 },` |
|         - |  214 | `	{ "spl_autoload_unregister",   1, 0 },` |
|         - |  215 | `	{ "spl_object_hash",           1, 0 },` |
|         - |  216 | `	{ "spl_object_id",             1, 0 },` |
|         - |  217 | `	/* Filesystem/IO family */` |
|         - |  218 | `	{ "basename",                  1, 1 },` |
|         - |  219 | `	{ "chdir",                     1, 0 },` |
|         - |  220 | `	{ "chgrp",                     2, 0 },` |
|         - |  221 | `	{ "dir",                       1, 1 },` |
|         - |  222 | `	{ "dirname",                   1, 1 },` |
|         - |  223 | `	{ "disk_free_space",           1, 0 },` |
|         - |  224 | `	{ "disk_total_space",          1, 0 },` |
|         - |  225 | `	{ "diskfreespace",             1, 0 },` |
|         - |  226 | `	{ "fclose",                    1, 0 },` |
|         - |  227 | `	{ "feof",                      1, 0 },` |
|         - |  228 | `	{ "fflush",                    1, 0 },` |
|         - |  229 | `	{ "fgetc",                     1, 0 },` |
|         - |  230 | `	{ "fgetcsv",                   1, 1 },` |
|         - |  231 | `	{ "file",                      1, 1 },` |
|         - |  232 | `	{ "file_exists",               1, 0 },` |
|         - |  233 | `	{ "fileatime",                 1, 0 },` |
|         - |  234 | `	{ "filectime",                 1, 0 },` |
|         - |  235 | `	{ "filemtime",                 1, 0 },` |
|         - |  236 | `	{ "filesize",                  1, 0 },` |
|         - |  237 | `	{ "filetype",                  1, 0 },` |
|         - |  238 | `	{ "flock",                     2, 1 },` |
|         - |  239 | `	{ "fpassthru",                 1, 0 },` |
|         - |  240 | `	{ "fputcsv",                   2, 1 },` |
|         - |  241 | `	{ "fputs",                     2, 1 },` |
|         - |  242 | `	{ "fseek",                     2, 1 },` |
|         - |  243 | `	{ "fstat",                     1, 0 },` |
|         - |  244 | `	{ "ftell",                     1, 0 },` |
|         - |  245 | `	{ "ftruncate",                 2, 0 },` |
|         - |  246 | `	{ "getopt",                    1, 1 },` |
|         - |  247 | `	{ "is_dir",                    1, 0 },` |
|         - |  248 | `	{ "is_executable",             1, 0 },` |
|         - |  249 | `	{ "is_file",                   1, 0 },` |
|         - |  250 | `	{ "is_link",                   1, 0 },` |
|         - |  251 | `	{ "is_readable",               1, 0 },` |
|         - |  252 | `	{ "is_writable",               1, 0 },` |
|         - |  253 | `	{ "lstat",                     1, 0 },` |
|         - |  254 | `	{ "md5_file",                  1, 1 },` |
|         - |  255 | `	{ "opendir",                   1, 1 },` |
|         - |  256 | `	{ "pathinfo",                  1, 1 },` |
|         - |  257 | `	{ "pclose",                    1, 0 },` |
|         - |  258 | `	{ "readlink",                  1, 0 },` |
|         - |  259 | `	{ "realpath",                  1, 0 },` |
|         - |  260 | `	{ "rewind",                    1, 0 },` |
|         - |  261 | `	{ "sha1_file",                 1, 1 },` |
|         - |  262 | `	{ "stat",                      1, 0 },` |
|         - |  263 | `	/* Date family */` |
|         - |  264 | `	{ "date",                      1, 1 },` |
|         - |  265 | `	{ "date_default_timezone_set", 1, 1 },` |
|         - |  266 | `	{ "gmdate",                    1, 1 },` |
|         - |  267 | `	{ "gmmktime",                  1, 1 },` |
|         - |  268 | `	{ "idate",                     1, 1 },` |
|         - |  269 | `	{ "mktime",                    1, 1 },` |
|         - |  270 | `	/* Encoding/URL family */` |
|         - |  271 | `	{ "base64_decode",             1, 1 },` |
|         - |  272 | `	{ "base64_encode",             1, 0 },` |
|         - |  273 | `	{ "convert_uudecode",          1, 0 },` |
|         - |  274 | `	{ "convert_uuencode",          1, 0 },` |
|         - |  275 | `	{ "parse_ini_file",            1, 1 },` |
|         - |  276 | `	{ "parse_ini_string",          1, 1 },` |
|         - |  277 | `	{ "parse_url",                 1, 1 },` |
|         - |  278 | `	{ "rawurldecode",              1, 0 },` |
|         - |  279 | `	{ "rawurlencode",              1, 0 },` |
|         - |  280 | `	{ "urldecode",                 1, 0 },` |
|         - |  281 | `	{ "urlencode",                 1, 0 },` |
|         - |  282 | `	/* JSON/serialize family */` |
|         - |  283 | `	{ "filter_var",                1, 1 },` |
|         - |  284 | `	{ "json_decode",               1, 1 },` |
|         - |  285 | `	{ "json_encode",               1, 1 },` |
|         - |  286 | `	{ "json_validate",             1, 1 },` |
|         - |  287 | `	{ "serialize",                 1, 0 },` |
|         - |  288 | `	{ "unserialize",               1, 1 },` |
|         - |  289 | `	/* PCRE family */` |
|         - |  290 | `	{ "preg_match",                2, 1 },` |
|         - |  291 | `	{ "preg_match_all",            2, 1 },` |
|         - |  292 | `	{ "preg_quote",                1, 1 },` |
|         - |  293 | `	{ "preg_replace",              3, 1 },` |
|         - |  294 | `	{ "preg_replace_callback",     3, 1 },` |
|         - |  295 | `	{ "preg_split",                2, 1 },` |
|         - |  296 | `	/* XML family */` |
|         - |  297 | `	/* Constants/misc family */` |
|         - |  298 | `	{ "call_user_func",            1, 1 },` |
|         - |  299 | `	{ "call_user_func_array",      2, 0 },` |
|         - |  300 | `	{ "constant",                  1, 0 },` |
|         - |  301 | `	{ "define",                    2, 1 },` |
|         - |  302 | `	{ "defined",                   1, 0 },` |
|         - |  303 | `	{ "error_log",                 1, 1 },` |
|         - |  304 | `	{ "fnmatch",                   2, 1 },` |
|         - |  305 | `	{ "forward_static_call",       1, 1 },` |
|         - |  306 | `	{ "forward_static_call_array", 2, 0 },` |
|         - |  307 | `	{ "func_get_arg",              1, 0 },` |
|         - |  308 | `	{ "function_exists",           1, 0 },` |
|         - |  309 | `	{ "header",                    1, 1 },` |
|         - |  310 | `	{ "password_get_info",         1, 0 },` |
|         - |  311 | `	{ "putenv",                    1, 0 },` |
|         - |  312 | `	{ "register_shutdown_function", 1, 1 },` |
|         - |  313 | `	{ "set_error_handler",         1, 1 },` |
|         - |  314 | `	{ "set_exception_handler",     1, 0 },` |
|         - |  315 | `	{ "setcookie",                 1, 1 },` |
|         - |  316 | `	{ "setrawcookie",              1, 1 },` |
|         - |  317 | `	{ "trigger_error",             1, 1 },` |
|         - |  318 | `	{ "user_error",                1, 1 },` |
|         - |  319 | `	/*` |
|         - |  320 | `	 * Overrides for signatures that under-report their own minimum: the callback` |
|         - |  321 | `	 * of these three hides inside the variadic tail ("array $array, ...$rest"),` |
|         - |  322 | `	 * so the derivation reads 1 where php requires 2.` |
|         - |  323 | `	 */` |
|         - |  324 | `	{ "array_udiff",               2, 1 },` |
|         - |  325 | `	{ "array_uintersect",          2, 1 },` |
|         - |  326 | `	{ "array_diff_uassoc",         2, 1 },` |
|         - |  327 | `	{ "array_diff_ukey",           2, 1 },` |
|         - |  328 | `	{ "array_intersect_ukey",      2, 1 },` |
|         - |  329 | `	{ "array_intersect_uassoc",    2, 1 },` |
|         - |  330 | `	{ "array_udiff_assoc",         2, 1 },` |
|         - |  331 | `	{ "array_uintersect_assoc",    2, 1 },` |
|         - |  332 | `	{ "array_udiff_uassoc",        3, 1 },` |
|         - |  333 | `	{ "array_uintersect_uassoc",   3, 1 },` |
|         - |  334 | `};` |
|         - |  335 | `/*` |
|         - |  336 | ` * Stamp the minimum-arity metadata from aBuiltinArity[] onto the already` |
|         - |  337 | ` * registered host functions. Called once at VM init after every builtin family` |
|         - |  338 | ` * has been installed into hHostFunction. A name absent from the hash (e.g. a` |
|         - |  339 | ` * build without a given extension) is simply skipped.` |
|         - |  340 | ` */` |
|      4076 |  341 | `PH7_PRIVATE void VmSetBuiltinArity(ph7_vm *pVm)` |
|         5 |  342 | `{` |
|         - |  343 | `	sxu32 n;` |
|   1141285 |  344 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinArity) ; ++n ){` |
|   1137209 |  345 | `		const struct VmBuiltinArity *p = &aBuiltinArity[n];` |
|   2274413 |  346 | `		SyHashEntry *pEntry = SyHashGet(&pVm->hHostFunction,` |
|   1137204 |  347 | `			(const void *)p->zName,SyStrlen(p->zName));` |
|   1137209 |  348 | `		if( pEntry ){` |
|   1137209 |  349 | `			ph7_user_func *pFunc = (ph7_user_func *)pEntry->pUserData;` |
|   1137209 |  350 | `			pFunc->nMinArg  = p->nMin;` |
|   1137209 |  351 | `			pFunc->bAtLeast = p->bAtLeast;` |
|    568602 |  352 | `		}` |
|    568607 |  353 | `	}` |
|      4081 |  354 | `}` |
|         - |  355 | `/*` |
|         - |  356 | ` * PHP 8.5 parameter signatures for the C builtins, generated offline from` |
|         - |  357 | ` * a real PHP 8.5 ReflectionFunction dump over PHL's registered function` |
|         - |  358 | ` * list (see the plan's Reflection section). "= ?" marks an optional` |
|         - |  359 | ` * parameter whose default is not representable as a short literal.` |
|         - |  360 | ` * Reflection parses these strings on demand; unlisted builtins degrade to` |
|         - |  361 | ` * the min-arity data.` |
|         - |  362 | ` */` |
|         - |  363 | `static const struct VmBuiltinSig {` |
|         - |  364 | `	const char *zName;` |
|         - |  365 | `	const char *zSig;` |
|         - |  366 | `	const char *zRet;` |
|         - |  367 | `} aBuiltinSig[] = {` |
|         - |  368 | `	/* The subsystems converted from embedded PHP into C (INI, libxml, sessions).` |
|         - |  369 | `	 * A prelude function declared its parameters in PHP and Reflection read them` |
|         - |  370 | `	 * from there; a C builtin has no declaration but this table, so without a row` |
|         - |  371 | `	 * here the same function reports NO parameters -- and loses its arity bounds` |
|         - |  372 | `	 * with them. */` |
|         - |  373 | `	{ "get_cfg_var", "string $option", "array\|string\|false" },` |
|         - |  374 | `	{ "ini_get", "string $option", "string\|false" },` |
|         - |  375 | `	{ "ini_get_all", "?string $extension = null, bool $details = true", "array\|false" },` |
|         - |  376 | `	{ "ini_restore", "string $option", "void" },` |
|         - |  377 | `	{ "ini_set", "string $option, string\|int\|float\|bool\|null $value", "string\|false" },` |
|         - |  378 | `	{ "libxml_clear_errors", "", "void" },` |
|         - |  379 | `	{ "libxml_get_errors", "", "array" },` |
|         - |  380 | `	{ "libxml_get_last_error", "", "LibXMLError\|false" },` |
|         - |  381 | `	{ "libxml_use_internal_errors", "?bool $use_errors = null", "bool" },` |
|         - |  382 | `	{ "session_abort", "", "bool" },` |
|         - |  383 | `	{ "session_commit", "", "bool" },` |
|         - |  384 | `	{ "session_destroy", "", "bool" },` |
|         - |  385 | `	{ "session_id", "?string $id = null", "string\|false" },` |
|         - |  386 | `	{ "session_name", "?string $name = null", "string\|false" },` |
|         - |  387 | `	{ "session_regenerate_id", "bool $delete_old_session = false", "bool" },` |
|         - |  388 | `	{ "session_reset", "", "bool" },` |
|         - |  389 | `	{ "session_save_path", "?string $path = null", "string\|false" },` |
|         - |  390 | `	{ "session_start", "array $options = []", "bool" },` |
|         - |  391 | `	{ "session_status", "", "int" },` |
|         - |  392 | `	{ "session_unset", "", "bool" },` |
|         - |  393 | `	{ "session_write_close", "", "bool" },` |
|         - |  394 | `	{ "abs", "int\|float $num", "int\|float" },` |
|         - |  395 | `	{ "acos", "float $num", "float" },` |
|         - |  396 | `	{ "acosh", "float $num", "float" },` |
|         - |  397 | `	{ "addcslashes", "string $string, string $characters", "string" },` |
|         - |  398 | `	{ "addslashes", "string $string", "string" },` |
|         - |  399 | `	{ "array_all", "array $array, callable $callback", "bool" },` |
|         - |  400 | `	{ "array_any", "array $array, callable $callback", "bool" },` |
|         - |  401 | `	{ "array_chunk", "array $array, int $length, bool $preserve_keys = false", "array" },` |
|         - |  402 | `	{ "array_column", "array $array, string\|int\|null $column_key, string\|int\|null $index_key = NULL", "array" },` |
|         - |  403 | `	{ "array_combine", "array $keys, array $values", "array" },` |
|         - |  404 | `	{ "array_diff", "array $array, array ...$arrays = ?", "array" },` |
|         - |  405 | `	{ "array_diff_assoc", "array $array, array ...$arrays = ?", "array" },` |
|         - |  406 | `	{ "array_diff_key", "array $array, array ...$arrays = ?", "array" },` |
|         - |  407 | `	{ "array_diff_uassoc", "array $array, ...$rest = ?", "array" },` |
|         - |  408 | `	{ "array_diff_ukey", "array $array, ...$rest = ?", "array" },` |
|         - |  409 | `	{ "array_fill", "int $start_index, int $count, mixed $value", "array" },` |
|         - |  410 | `	{ "array_fill_keys", "array $keys, mixed $value", "array" },` |
|         - |  411 | `	{ "array_filter", "array $array, ?callable $callback = NULL, int $mode = 0", "array" },` |
|         - |  412 | `	{ "array_find", "array $array, callable $callback", "mixed" },` |
|         - |  413 | `	{ "array_find_key", "array $array, callable $callback", "mixed" },` |
|         - |  414 | `	{ "array_first", "array $array", "mixed" },` |
|         - |  415 | `	{ "array_flip", "array $array", "array" },` |
|         - |  416 | `	{ "array_intersect", "array $array, array ...$arrays = ?", "array" },` |
|         - |  417 | `	{ "array_intersect_assoc", "array $array, array ...$arrays = ?", "array" },` |
|         - |  418 | `	{ "array_intersect_key", "array $array, array ...$arrays = ?", "array" },` |
|         - |  419 | `	{ "array_intersect_uassoc", "array $array, ...$rest = ?", "array" },` |
|         - |  420 | `	{ "array_intersect_ukey", "array $array, ...$rest = ?", "array" },` |
|         - |  421 | `	{ "array_is_list", "array $array", "bool" },` |
|         - |  422 | `	{ "array_key_exists", "$key, array $array", "bool" },` |
|         - |  423 | `	{ "array_key_first", "array $array", "string\|int\|null" },` |
|         - |  424 | `	{ "array_key_last", "array $array", "string\|int\|null" },` |
|         - |  425 | `	{ "array_keys", "array $array, mixed $filter_value = ?, bool $strict = false", "array" },` |
|         - |  426 | `	{ "array_last", "array $array", "mixed" },` |
|         - |  427 | `	{ "array_map", "?callable $callback, array $array, array ...$arrays = ?", "array" },` |
|         - |  428 | `	{ "array_merge", "array ...$arrays = ?", "array" },` |
|         - |  429 | `	{ "array_multisort", "&$array, &...$rest = ?", "true" },` |
|         - |  430 | `	{ "array_merge_recursive", "array ...$arrays = ?", "array" },` |
|         - |  431 | `	{ "array_pad", "array $array, int $length, mixed $value", "array" },` |
|         - |  432 | `	{ "array_pop", "array &$array", "mixed" },` |
|         - |  433 | `	{ "array_product", "array $array", "int\|float" },` |
|         - |  434 | `	{ "array_push", "array &$array, mixed ...$values = ?", "int" },` |
|         - |  435 | `	{ "array_rand", "array $array, int $num = 1", "array\|string\|int" },` |
|         - |  436 | `	{ "array_reduce", "array $array, callable $callback, mixed $initial = NULL", "mixed" },` |
|         - |  437 | `	{ "array_replace", "array $array, array ...$replacements = ?", "array" },` |
|         - |  438 | `	{ "array_reverse", "array $array, bool $preserve_keys = false", "array" },` |
|         - |  439 | `	{ "array_search", "mixed $needle, array $haystack, bool $strict = false", "string\|int\|false" },` |
|         - |  440 | `	{ "array_shift", "array &$array", "mixed" },` |
|         - |  441 | `	{ "array_slice", "array $array, int $offset, ?int $length = NULL, bool $preserve_keys = false", "array" },` |
|         - |  442 | `	{ "array_splice", "array &$array, int $offset, ?int $length = NULL, mixed $replacement = ?", "array" },` |
|         - |  443 | `	{ "array_sum", "array $array", "int\|float" },` |
|         - |  444 | `	{ "array_udiff", "array $array, ...$rest = ?", "array" },` |
|         - |  445 | `	{ "array_udiff_assoc", "array $array, ...$rest = ?", "array" },` |
|         - |  446 | `	{ "array_udiff_uassoc", "array $array, ...$rest = ?", "array" },` |
|         - |  447 | `	{ "array_uintersect", "array $array, ...$rest = ?", "array" },` |
|         - |  448 | `	{ "array_uintersect_assoc", "array $array, ...$rest = ?", "array" },` |
|         - |  449 | `	{ "array_uintersect_uassoc", "array $array, ...$rest = ?", "array" },` |
|         - |  450 | `	{ "array_unique", "array $array, int $flags = 2", "array" },` |
|         - |  451 | `	{ "array_unshift", "array &$array, mixed ...$values = ?", "int" },` |
|         - |  452 | `	{ "array_values", "array $array", "array" },` |
|         - |  453 | `	{ "array_walk", "object\|array &$array, callable $callback, mixed $arg = ?", "true" },` |
|         - |  454 | `	{ "array_walk_recursive", "object\|array &$array, callable $callback, mixed $arg = ?", "true" },` |
|         - |  455 | `	{ "arsort", "array &$array, int $flags = 0", "true" },` |
|         - |  456 | `	{ "asin", "float $num", "float" },` |
|         - |  457 | `	{ "asinh", "float $num", "float" },` |
|         - |  458 | `	{ "asort", "array &$array, int $flags = 0", "true" },` |
|         - |  459 | `	{ "assert", "mixed $assertion, Throwable\|string\|null $description = NULL", "bool" },` |
|         - |  460 | `	{ "atan", "float $num", "float" },` |
|         - |  461 | `	{ "atanh", "float $num", "float" },` |
|         - |  462 | `	{ "atan2", "float $y, float $x", "float" },` |
|         - |  463 | `	{ "base64_decode", "string $string, bool $strict = false", "string\|false" },` |
|         - |  464 | `	{ "base64_encode", "string $string", "string" },` |
|         - |  465 | `	{ "base_convert", "string $num, int $from_base, int $to_base", "string" },` |
|         - |  466 | `	{ "basename", "string $path, string $suffix = ''", "string" },` |
|         - |  467 | `	{ "bin2hex", "string $string", "string" },` |
|         - |  468 | `	{ "bindec", "string $binary_string", "int\|float" },` |
|         - |  469 | `	{ "boolval", "mixed $value", "bool" },` |
|         - |  470 | `	{ "call_user_func", "callable $callback, mixed ...$args = ?", "mixed" },` |
|         - |  471 | `	{ "call_user_func_array", "callable $callback, array $args", "mixed" },` |
|         - |  472 | `	{ "ceil", "int\|float $num", "float" },` |
|         - |  473 | `	{ "chdir", "string $directory", "bool" },` |
|         - |  474 | `	{ "chgrp", "string $filename, string\|int $group", "bool" },` |
|         - |  475 | `	{ "chmod", "string $filename, int $permissions", "bool" },` |
|         - |  476 | `	{ "chop", "string $string, string $characters = ?", "string" },` |
|         - |  477 | `	{ "chown", "string $filename, string\|int $user", "bool" },` |
|         - |  478 | `	{ "chr", "int $codepoint", "string" },` |
|         - |  479 | `	{ "chroot", "string $directory", "bool" },` |
|         - |  480 | `	{ "chunk_split", "string $string, int $length = 76, string $separator = ?", "string" },` |
|         - |  481 | `	{ "class_alias", "string $class, string $alias, bool $autoload = true", "bool" },` |
|         - |  482 | `	{ "class_exists", "string $class, bool $autoload = true", "bool" },` |
|         - |  483 | `	{ "enum_exists", "string $enum, bool $autoload = true", "bool" },` |
|         - |  484 | `	{ "clone", "object $object, array $withProperties = []", "object" },` |
|         - |  485 | `	{ "closedir", "$dir_handle = NULL", "void" },` |
|         - |  486 | `	{ "compact", "$var_name, ...$var_names = ?", "array" },` |
|         - |  487 | `	{ "constant", "string $name", "mixed" },` |
|         - |  488 | `	{ "convert_uudecode", "string $string", "string\|false" },` |
|         - |  489 | `	{ "convert_uuencode", "string $string", "string" },` |
|         - |  490 | `	{ "copy", "string $from, string $to, $context = NULL", "bool" },` |
|         - |  491 | `	{ "cos", "float $num", "float" },` |
|         - |  492 | `	{ "cosh", "float $num", "float" },` |
|         - |  493 | `	{ "count", "Countable\|array $value, int $mode = 0", "int" },` |
|         - |  494 | `	{ "count_chars", "string $string, int $mode = 0", "array\|string" },` |
|         - |  495 | `	{ "crc32", "string $string", "int" },` |
|         - |  496 | `	{ "ctype_alnum", "mixed $text", "bool" },` |
|         - |  497 | `	{ "ctype_alpha", "mixed $text", "bool" },` |
|         - |  498 | `	{ "ctype_cntrl", "mixed $text", "bool" },` |
|         - |  499 | `	{ "ctype_digit", "mixed $text", "bool" },` |
|         - |  500 | `	{ "ctype_graph", "mixed $text", "bool" },` |
|         - |  501 | `	{ "ctype_lower", "mixed $text", "bool" },` |
|         - |  502 | `	{ "ctype_print", "mixed $text", "bool" },` |
|         - |  503 | `	{ "ctype_punct", "mixed $text", "bool" },` |
|         - |  504 | `	{ "ctype_space", "mixed $text", "bool" },` |
|         - |  505 | `	{ "ctype_upper", "mixed $text", "bool" },` |
|         - |  506 | `	{ "ctype_xdigit", "mixed $text", "bool" },` |
|         - |  507 | `	{ "current", "object\|array $array", "mixed" },` |
|         - |  508 | `	{ "date", "string $format, ?int $timestamp = NULL", "string" },` |
|         - |  509 | `	{ "date_add", "DateTime $object, DateInterval $interval", "DateTime" },` |
|         - |  510 | `	{ "date_create", "string $datetime = 'now', ?DateTimeZone $timezone = NULL", "DateTime\|false" },` |
|         - |  511 | `	{ "date_create_from_format", "string $format, string $datetime, ?DateTimeZone $timezone = NULL", "DateTime\|false" },` |
|         - |  512 | `	{ "date_create_immutable", "string $datetime = 'now', ?DateTimeZone $timezone = NULL", "DateTimeImmutable\|false" },` |
|         - |  513 | `	{ "date_create_immutable_from_format", "string $format, string $datetime, ?DateTimeZone $timezone = NULL", "DateTimeImmutable\|false" },` |
|         - |  514 | `	{ "date_date_set", "DateTime $object, int $year, int $month, int $day", "DateTime" },` |
|         - |  515 | `	{ "date_diff", "DateTimeInterface $baseObject, DateTimeInterface $targetObject, bool $absolute = false", "DateInterval" },` |
|         - |  516 | `	{ "date_format", "DateTimeInterface $object, string $format", "string" },` |
|         - |  517 | `	{ "date_get_last_errors", "", "array\|false" },` |
|         - |  518 | `	{ "date_interval_create_from_date_string", "string $datetime", "DateInterval\|false" },` |
|         - |  519 | `	{ "date_interval_format", "DateInterval $object, string $format", "string" },` |
|         - |  520 | `	{ "date_isodate_set", "DateTime $object, int $year, int $week, int $dayOfWeek = 1", "DateTime" },` |
|         - |  521 | `	{ "date_modify", "DateTime $object, string $modifier", "DateTime\|false" },` |
|         - |  522 | `	{ "date_offset_get", "DateTimeInterface $object", "int" },` |
|         - |  523 | `	{ "date_sub", "DateTime $object, DateInterval $interval", "DateTime" },` |
|         - |  524 | `	{ "date_time_set", "DateTime $object, int $hour, int $minute, int $second = 0, int $microsecond = 0", "DateTime" },` |
|         - |  525 | `	{ "date_timestamp_get", "DateTimeInterface $object", "int" },` |
|         - |  526 | `	{ "date_timestamp_set", "DateTime $object, int $timestamp", "DateTime" },` |
|         - |  527 | `	{ "date_timezone_get", "DateTimeInterface $object", "DateTimeZone\|false" },` |
|         - |  528 | `	{ "date_timezone_set", "DateTime $object, DateTimeZone $timezone", "DateTime" },` |
|         - |  529 | `	{ "timezone_name_get", "DateTimeZone $object", "string" },` |
|         - |  530 | `	{ "timezone_offset_get", "DateTimeZone $object, DateTimeInterface $datetime", "int" },` |
|         - |  531 | `	{ "timezone_open", "string $timezone", "DateTimeZone\|false" },` |
|         - |  532 | `	{ "date_default_timezone_get", "", "string" },` |
|         - |  533 | `	{ "date_default_timezone_set", "string $timezoneId", "bool" },` |
|         - |  534 | `	{ "debug_backtrace", "int $options = 1, int $limit = 0", "array" },` |
|         - |  535 | `	{ "debug_print_backtrace", "int $options = 0, int $limit = 0", "void" },` |
|         - |  536 | `	{ "decbin", "int $num", "string" },` |
|         - |  537 | `	{ "dechex", "int $num", "string" },` |
|         - |  538 | `	{ "decoct", "int $num", "string" },` |
|         - |  539 | `	{ "define", "string $constant_name, mixed $value, bool $case_insensitive = false", "bool" },` |
|         - |  540 | `	{ "defined", "string $constant_name", "bool" },` |
|         - |  541 | `	{ "deg2rad", "float $num", "float" },` |
|         - |  542 | `	{ "die", "string\|int $status = 0", "never" },` |
|         - |  543 | `	{ "dir", "string $directory, $context = NULL", "Directory\|false" },` |
|         - |  544 | `	{ "dirname", "string $path, int $levels = 1", "string" },` |
|         - |  545 | `	{ "disk_free_space", "string $directory", "float\|false" },` |
|         - |  546 | `	{ "disk_total_space", "string $directory", "float\|false" },` |
|         - |  547 | `	{ "diskfreespace", "string $directory", "float\|false" },` |
|         - |  548 | `	{ "end", "object\|array &$array", "mixed" },` |
|         - |  549 | `	{ "error_get_last", "", "?array" },` |
|         - |  550 | `	{ "error_clear_last", "", "void" },` |
|         - |  551 | `	{ "error_log", "string $message, int $message_type = 0, ?string $destination = NULL, ?string $additional_headers = NULL", "bool" },` |
|         - |  552 | `	{ "error_reporting", "?int $error_level = NULL", "int" },` |
|         - |  553 | `	{ "escapeshellarg", "string $arg", "string" },` |
|         - |  554 | `	{ "escapeshellcmd", "string $command", "string" },` |
|         - |  555 | `	{ "exec", "string $command, &$output = NULL, &$result_code = NULL", "string\|false" },` |
|         - |  556 | `	{ "exit", "string\|int $status = 0", "never" },` |
|         - |  557 | `	{ "exp", "float $num", "float" },` |
|         - |  558 | `	{ "expm1", "float $num", "float" },` |
|         - |  559 | `	{ "explode", "string $separator, string $string, int $limit = 9223372036854775807", "array" },` |
|         - |  560 | `	{ "extension_loaded", "string $extension", "bool" },` |
|         - |  561 | `	{ "extract", "array &$array, int $flags = 0, string $prefix = ''", "int" },` |
|         - |  562 | `	{ "fclose", "$stream", "bool" },` |
|         - |  563 | `	{ "feof", "$stream", "bool" },` |
|         - |  564 | `	{ "fflush", "$stream", "bool" },` |
|         - |  565 | `	{ "fgetc", "$stream", "string\|false" },` |
|         - |  566 | `	{ "fgetcsv", "$stream, ?int $length = NULL, string $separator = ',', string $enclosure = '\"', string $escape = '\\\\'", "array\|false" },` |
|         - |  567 | `	{ "fgets", "$stream, ?int $length = NULL", "string\|false" },` |
|         - |  568 | `	{ "file", "string $filename, int $flags = 0, $context = NULL", "array\|false" },` |
|         - |  569 | `	{ "file_exists", "string $filename", "bool" },` |
|         - |  570 | `	{ "file_get_contents", "string $filename, bool $use_include_path = false, $context = NULL, int $offset = 0, ?int $length = NULL", "string\|false" },` |
|         - |  571 | `	{ "file_put_contents", "string $filename, mixed $data, int $flags = 0, $context = NULL", "int\|false" },` |
|         - |  572 | `	{ "fileatime", "string $filename", "int\|false" },` |
|         - |  573 | `	{ "filectime", "string $filename", "int\|false" },` |
|         - |  574 | `	{ "filegroup", "string $filename", "int\|false" },` |
|         - |  575 | `	{ "fileinode", "string $filename", "int\|false" },` |
|         - |  576 | `	{ "filemtime", "string $filename", "int\|false" },` |
|         - |  577 | `	{ "fileowner", "string $filename", "int\|false" },` |
|         - |  578 | `	{ "fileperms", "string $filename", "int\|false" },` |
|         - |  579 | `	{ "filesize", "string $filename", "int\|false" },` |
|         - |  580 | `	{ "filetype", "string $filename", "string\|false" },` |
|         - |  581 | `	{ "filter_input", "int $type, string $var_name, int $filter = 516, array\|int $options = 0", "mixed" },` |
|         - |  582 | `	{ "filter_var", "mixed $value, int $filter = 516, array\|int $options = 0", "mixed" },` |
|         - |  583 | `	{ "floatval", "mixed $value", "float" },` |
|         - |  584 | `	{ "flock", "$stream, int $operation, &$would_block = NULL", "bool" },` |
|         - |  585 | `	{ "floor", "int\|float $num", "float" },` |
|         - |  586 | `	{ "flush", "", "void" },` |
|         - |  587 | `	{ "fmod", "float $num1, float $num2", "float" },` |
|         - |  588 | `	{ "fnmatch", "string $pattern, string $filename, int $flags = 0", "bool" },` |
|         - |  589 | `	{ "fopen", "string $filename, string $mode, bool $use_include_path = false, $context = NULL", "" },` |
|         - |  590 | `	{ "forward_static_call", "callable $callback, mixed ...$args = ?", "mixed" },` |
|         - |  591 | `	{ "forward_static_call_array", "callable $callback, array $args", "mixed" },` |
|         - |  592 | `	{ "fpow", "float $num, float $exponent", "float" },` |
|         - |  593 | `	{ "fpassthru", "$stream", "int" },` |
|         - |  594 | `	{ "fprintf", "$stream, string $format, mixed ...$values = ?", "int" },` |
|         - |  595 | `	{ "fputcsv", "$stream, array $fields, string $separator = ',', string $enclosure = '\"', string $escape = '\\\\', string $eol = ?", "int\|false" },` |
|         - |  596 | `	{ "fputs", "$stream, string $data, ?int $length = NULL", "int\|false" },` |
|         - |  597 | `	{ "fread", "$stream, int $length", "string\|false" },` |
|         - |  598 | `	{ "fseek", "$stream, int $offset, int $whence = 0", "int" },` |
|         - |  599 | `	{ "fstat", "$stream", "array\|false" },` |
|         - |  600 | `	{ "ftell", "$stream", "int\|false" },` |
|         - |  601 | `	{ "ftruncate", "$stream, int $size", "bool" },` |
|         - |  602 | `	{ "func_get_arg", "int $position", "mixed" },` |
|         - |  603 | `	{ "func_get_args", "", "array" },` |
|         - |  604 | `	{ "func_num_args", "", "int" },` |
|         - |  605 | `	{ "function_exists", "string $function", "bool" },` |
|         - |  606 | `	{ "fwrite", "$stream, string $data, ?int $length = NULL", "int\|false" },` |
|         - |  607 | `	{ "gc_collect_cycles", "", "int" },` |
|         - |  608 | `	{ "gc_disable", "", "void" },` |
|         - |  609 | `	{ "gc_enable", "", "void" },` |
|         - |  610 | `	{ "gc_enabled", "", "bool" },` |
|         - |  611 | `	{ "gc_mem_caches", "", "int" },` |
|         - |  612 | `	{ "gc_status", "", "array" },` |
|         - |  613 | `	{ "get_called_class", "", "string" },` |
|         - |  614 | `	{ "get_class", "object $object = ?", "string" },` |
|         - |  615 | `	{ "get_class_methods", "object\|string $object_or_class", "array" },` |
|         - |  616 | `	{ "get_class_vars", "string $class", "array" },` |
|         - |  617 | `	{ "get_current_user", "", "string" },` |
|         - |  618 | `	{ "get_declared_classes", "", "array" },` |
|         - |  619 | `	{ "get_declared_interfaces", "", "array" },` |
|         - |  620 | `	{ "get_defined_constants", "bool $categorize = false", "array" },` |
|         - |  621 | `	{ "get_defined_functions", "bool $exclude_disabled = true", "array" },` |
|         - |  622 | `	{ "get_defined_vars", "", "array" },` |
|         - |  623 | `	{ "get_html_translation_table", "int $table = 0, int $flags = 11, string $encoding = 'UTF-8'", "array" },` |
|         - |  624 | `	{ "get_include_path", "", "string\|false" },` |
|         - |  625 | `	{ "get_included_files", "", "array" },` |
|         - |  626 | `	{ "get_loaded_extensions", "bool $zend_extensions = false", "array" },` |
|         - |  627 | `	{ "get_object_vars", "object $object", "array" },` |
|         - |  628 | `	{ "get_parent_class", "object\|string $object_or_class = ?", "string\|false" },` |
|         - |  629 | `	{ "get_resource_id", "$resource", "int" },` |
|         - |  630 | `	{ "get_resource_type", "$resource", "string" },` |
|         - |  631 | `	{ "getcwd", "", "string\|false" },` |
|         - |  632 | `	{ "getdate", "?int $timestamp = NULL", "array" },` |
|         - |  633 | `	{ "getenv", "?string $name = NULL, bool $local_only = false", "array\|string\|false" },` |
|         - |  634 | `	{ "getmygid", "", "int\|false" },` |
|         - |  635 | `	{ "getmypid", "", "int\|false" },` |
|         - |  636 | `	{ "getmyuid", "", "int\|false" },` |
|         - |  637 | `	{ "getopt", "string $short_options, array $long_options = ?, &$rest_index = NULL", "array\|false" },` |
|         - |  638 | `	{ "getrandmax", "", "int" },` |
|         - |  639 | `	{ "gettimeofday", "bool $as_float = false", "array\|float" },` |
|         - |  640 | `	{ "gettype", "mixed $value", "string" },` |
|         - |  641 | `	{ "get_debug_type", "mixed $value", "string" },` |
|         - |  642 | `	{ "gmdate", "string $format, ?int $timestamp = NULL", "string" },` |
|         - |  643 | `	{ "gmmktime", "int $hour, ?int $minute = NULL, ?int $second = NULL, ?int $month = NULL, ?int $day = NULL, ?int $year = NULL", "int\|false" },` |
|         - |  644 | `	{ "hash", "string $algo, string $data, bool $binary = false, array $options = ?", "string" },` |
|         - |  645 | `	{ "hash_algos", "", "array" },` |
|         - |  646 | `	{ "hash_equals", "string $known_string, string $user_string", "bool" },` |
|         - |  647 | `	{ "hash_hmac", "string $algo, string $data, string $key, bool $binary = false", "string" },` |
|         - |  648 | `	{ "header", "string $header, bool $replace = true, int $response_code = 0", "void" },` |
|         - |  649 | `	{ "header_remove", "?string $name = NULL", "void" },` |
|         - |  650 | `	{ "headers_list", "", "array" },` |
|         - |  651 | `	{ "headers_sent", "&$filename = NULL, &$line = NULL", "bool" },` |
|         - |  652 | `	{ "hexdec", "string $hex_string", "int\|float" },` |
|         - |  653 | `	{ "html_entity_decode", "string $string, int $flags = 11, ?string $encoding = NULL", "string" },` |
|         - |  654 | `	{ "http_build_query", "object\|array $data, string $numeric_prefix = '', ?string $arg_separator = null, int $encoding_type = 1", "string" },` |
|         - |  655 | `	{ "htmlentities", "string $string, int $flags = 11, ?string $encoding = NULL, bool $double_encode = true", "string" },` |
|         - |  656 | `	{ "htmlspecialchars", "string $string, int $flags = 11, ?string $encoding = NULL, bool $double_encode = true", "string" },` |
|         - |  657 | `	{ "htmlspecialchars_decode", "string $string, int $flags = 11", "string" },` |
|         - |  658 | `	{ "http_response_code", "int $response_code = 0", "int\|bool" },` |
|         - |  659 | `	{ "hypot", "float $x, float $y", "float" },` |
|         - |  660 | `	{ "idate", "string $format, ?int $timestamp = NULL", "int\|false" },` |
|         - |  661 | `	{ "implode", "array\|string $separator, ?array $array = NULL", "string" },` |
|         - |  662 | `	{ "in_array", "mixed $needle, array $haystack, bool $strict = false", "bool" },` |
|         - |  663 | `	{ "intdiv", "int $num1, int $num2", "int" },` |
|         - |  664 | `	{ "interface_exists", "string $interface, bool $autoload = true", "bool" },` |
|         - |  665 | `	{ "trait_exists", "string $trait, bool $autoload = true", "bool" },` |
|         - |  666 | `	{ "intval", "mixed $value, int $base = 10", "int" },` |
|         - |  667 | `	{ "is_a", "mixed $object_or_class, string $class, bool $allow_string = false", "bool" },` |
|         - |  668 | `	{ "is_array", "mixed $value", "bool" },` |
|         - |  669 | `	{ "is_bool", "mixed $value", "bool" },` |
|         - |  670 | `	{ "is_callable", "mixed $value, bool $syntax_only = false, &$callable_name = NULL", "bool" },` |
|         - |  671 | `	{ "is_dir", "string $filename", "bool" },` |
|         - |  672 | `	{ "is_double", "mixed $value", "bool" },` |
|         - |  673 | `	{ "is_executable", "string $filename", "bool" },` |
|         - |  674 | `	{ "is_file", "string $filename", "bool" },` |
|         - |  675 | `	{ "is_float", "mixed $value", "bool" },` |
|         - |  676 | `	{ "is_int", "mixed $value", "bool" },` |
|         - |  677 | `	{ "is_integer", "mixed $value", "bool" },` |
|         - |  678 | `	{ "is_link", "string $filename", "bool" },` |
|         - |  679 | `	{ "is_long", "mixed $value", "bool" },` |
|         - |  680 | `	{ "is_null", "mixed $value", "bool" },` |
|         - |  681 | `	{ "is_numeric", "mixed $value", "bool" },` |
|         - |  682 | `	{ "is_object", "mixed $value", "bool" },` |
|         - |  683 | `	{ "is_readable", "string $filename", "bool" },` |
|         - |  684 | `	{ "is_resource", "mixed $value", "bool" },` |
|         - |  685 | `	{ "is_scalar", "mixed $value", "bool" },` |
|         - |  686 | `	{ "is_string", "mixed $value", "bool" },` |
|         - |  687 | `	{ "is_subclass_of", "mixed $object_or_class, string $class, bool $allow_string = true", "bool" },` |
|         - |  688 | `	{ "is_writable", "string $filename", "bool" },` |
|         - |  689 | `	{ "iterator_apply", "Traversable $iterator, callable $callback, ?array $args = NULL", "int" },` |
|         - |  690 | `	{ "iterator_count", "Traversable\|array $iterator", "int" },` |
|         - |  691 | `	{ "iterator_to_array", "Traversable\|array $iterator, bool $preserve_keys = true", "array" },` |
|         - |  692 | `	{ "join", "array\|string $separator, ?array $array = NULL", "string" },` |
|         - |  693 | `	{ "json_decode", "string $json, ?bool $associative = NULL, int $depth = 512, int $flags = 0", "mixed" },` |
|         - |  694 | `	{ "json_encode", "mixed $value, int $flags = 0, int $depth = 512", "string\|false" },` |
|         - |  695 | `	{ "json_last_error", "", "int" },` |
|         - |  696 | `	{ "json_last_error_msg", "", "string" },` |
|         - |  697 | `	{ "json_validate", "string $json, int $depth = 512, int $flags = 0", "bool" },` |
|         - |  698 | `	{ "key", "object\|array $array", "string\|int\|null" },` |
|         - |  699 | `	{ "key_exists", "$key, array $array", "bool" },` |
|         - |  700 | `	{ "krsort", "array &$array, int $flags = 0", "true" },` |
|         - |  701 | `	{ "ksort", "array &$array, int $flags = 0", "true" },` |
|         - |  702 | `	{ "lcfirst", "string $string", "string" },` |
|         - |  703 | `	{ "levenshtein", "string $string1, string $string2, int $insertion_cost = 1, int $replacement_cost = 1, int $deletion_cost = 1", "int" },` |
|         - |  704 | `	{ "link", "string $target, string $link", "bool" },` |
|         - |  705 | `	{ "localtime", "?int $timestamp = NULL, bool $associative = false", "array" },` |
|         - |  706 | `	{ "log", "float $num, float $base = 2.718281828459045", "float" },` |
|         - |  707 | `	{ "log10", "float $num", "float" },` |
|         - |  708 | `	{ "log1p", "float $num", "float" },` |
|         - |  709 | `	{ "lstat", "string $filename", "array\|false" },` |
|         - |  710 | `	{ "ltrim", "string $string, string $characters = ?", "string" },` |
|         - |  711 | `	{ "max", "mixed $value, mixed ...$values = ?", "mixed" },` |
|         - |  712 | `	{ "mb_chr", "int $codepoint, ?string $encoding = NULL", "string\|false" },` |
|         - |  713 | `	{ "mb_convert_encoding", "array\|string $string, string $to_encoding, array\|string\|null $from_encoding = NULL", "array\|string\|false" },` |
|         - |  714 | `	{ "mb_ord", "string $string, ?string $encoding = NULL", "int\|false" },` |
|         - |  715 | `	{ "mb_ltrim", "string $string, ?string $characters = null, ?string $encoding = null", "string" },` |
|         - |  716 | `	{ "mb_rtrim", "string $string, ?string $characters = null, ?string $encoding = null", "string" },` |
|         - |  717 | `	{ "mb_strtolower", "string $string, ?string $encoding = NULL", "string" },` |
|         - |  718 | `	{ "mb_strtoupper", "string $string, ?string $encoding = NULL", "string" },` |
|         - |  719 | `	{ "mb_trim", "string $string, ?string $characters = null, ?string $encoding = null", "string" },` |
|         - |  720 | `	{ "md5", "string $string, bool $binary = false", "string" },` |
|         - |  721 | `	{ "md5_file", "string $filename, bool $binary = false", "string\|false" },` |
|         - |  722 | `	{ "metaphone", "string $string, int $max_phonemes = 0", "string" },` |
|         - |  723 | `	{ "method_exists", "$object_or_class, string $method", "bool" },` |
|         - |  724 | `	{ "memory_get_peak_usage", "bool $real_usage = false", "int" },` |
|         - |  725 | `	{ "memory_get_usage", "bool $real_usage = false", "int" },` |
|         - |  726 | `	{ "microtime", "bool $as_float = false", "string\|float" },` |
|         - |  727 | `	{ "min", "mixed $value, mixed ...$values = ?", "mixed" },` |
|         - |  728 | `	{ "mkdir", "string $directory, int $permissions = 511, bool $recursive = false, $context = NULL", "bool" },` |
|         - |  729 | `	{ "mktime", "int $hour, ?int $minute = NULL, ?int $second = NULL, ?int $month = NULL, ?int $day = NULL, ?int $year = NULL", "int\|false" },` |
|         - |  730 | `	{ "mt_getrandmax", "", "int" },` |
|         - |  731 | `	{ "mt_rand", "int $min = ?, int $max = ?", "int" },` |
|         - |  732 | `	{ "mt_srand", "?int $seed = NULL, int $mode = 0", "void" },` |
|         - |  733 | `	{ "natcasesort", "array &$array", "true" },` |
|         - |  734 | `	{ "natsort", "array &$array", "true" },` |
|         - |  735 | `	{ "next", "object\|array &$array", "mixed" },` |
|         - |  736 | `	{ "nl2br", "string $string, bool $use_xhtml = true", "string" },` |
|         - |  737 | `	{ "number_format", "float $num, int $decimals = 0, ?string $decimal_separator = '.', ?string $thousands_separator = ','", "string" },` |
|         - |  738 | `	{ "ob_clean", "", "bool" },` |
|         - |  739 | `	{ "ob_end_clean", "", "bool" },` |
|         - |  740 | `	{ "ob_end_flush", "", "bool" },` |
|         - |  741 | `	{ "ob_flush", "", "bool" },` |
|         - |  742 | `	{ "ob_get_clean", "", "string\|false" },` |
|         - |  743 | `	{ "ob_get_contents", "", "string\|false" },` |
|         - |  744 | `	{ "ob_get_flush", "", "string\|false" },` |
|         - |  745 | `	{ "ob_get_length", "", "int\|false" },` |
|         - |  746 | `	{ "ob_get_level", "", "int" },` |
|         - |  747 | `	{ "ob_implicit_flush", "bool $enable = true", "void" },` |
|         - |  748 | `	{ "ob_list_handlers", "", "array" },` |
|         - |  749 | `	{ "ob_start", "$callback = NULL, int $chunk_size = 0, int $flags = 112", "bool" },` |
|         - |  750 | `	{ "octdec", "string $octal_string", "int\|float" },` |
|         - |  751 | `	{ "opendir", "string $directory, $context = NULL", "" },` |
|         - |  752 | `	{ "ord", "string $character", "int" },` |
|         - |  753 | `	{ "parse_ini_file", "string $filename, bool $process_sections = false, int $scanner_mode = 0", "array\|false" },` |
|         - |  754 | `	{ "parse_ini_string", "string $ini_string, bool $process_sections = false, int $scanner_mode = 0", "array\|false" },` |
|         - |  755 | `	{ "parse_str", "string $string, &$result", "void" },` |
|         - |  756 | `	{ "parse_url", "string $url, int $component = -1", "array\|string\|int\|false\|null" },` |
|         - |  757 | `	{ "password_get_info", "string $hash", "array" },` |
|         - |  758 | `	{ "password_hash", "string $password, string\|int\|null $algo, array $options = ?", "string" },` |
|         - |  759 | `	{ "password_needs_rehash", "string $hash, string\|int\|null $algo, array $options = ?", "bool" },` |
|         - |  760 | `	{ "password_verify", "string $password, string $hash", "bool" },` |
|         - |  761 | `	{ "passthru", "string $command, &$result_code = NULL", "?false" },` |
|         - |  762 | `	{ "pathinfo", "string $path, int $flags = 15", "array\|string" },` |
|         - |  763 | `	{ "pclose", "$handle", "int" },` |
|         - |  764 | `	{ "php_sapi_name", "", "string\|false" },` |
|         - |  765 | `	{ "php_uname", "string $mode = 'a'", "string" },` |
|         - |  766 | `	{ "phpinfo", "int $flags = 4294967295", "true" },` |
|         - |  767 | `	{ "phpversion", "?string $extension = NULL", "string\|false" },` |
|         - |  768 | `	{ "pi", "", "float" },` |
|         - |  769 | `	{ "popen", "string $command, string $mode", "" },` |
|         - |  770 | `	{ "pos", "object\|array $array", "mixed" },` |
|         - |  771 | `	{ "pow", "mixed $num, mixed $exponent", "object\|int\|float" },` |
|         - |  772 | `	{ "preg_last_error", "", "int" },` |
|         - |  773 | `	{ "preg_last_error_msg", "", "string" },` |
|         - |  774 | `	{ "fsockopen", "string $hostname, int $port = -1, &$error_code = NULL, &$error_message = NULL, ?float $timeout = NULL", "" },` |
|         - |  775 | `	{ "pfsockopen", "string $hostname, int $port = -1, &$error_code = NULL, &$error_message = NULL, ?float $timeout = NULL", "" },` |
|         - |  776 | `	{ "stream_socket_client", "string $address, &$error_code = NULL, &$error_message = NULL, ?float $timeout = NULL, int $flags = 4, $context = NULL", "" },` |
|         - |  777 | `	{ "preg_match", "string $pattern, string $subject, &$matches = NULL, int $flags = 0, int $offset = 0", "int\|false" },` |
|         - |  778 | `	{ "preg_match_all", "string $pattern, string $subject, &$matches = NULL, int $flags = 0, int $offset = 0", "int\|false" },` |
|         - |  779 | `	{ "preg_quote", "string $str, ?string $delimiter = NULL", "string" },` |
|         - |  780 | `	{ "preg_replace", "array\|string $pattern, array\|string $replacement, array\|string $subject, int $limit = -1, &$count = NULL", "array\|string\|null" },` |
|         - |  781 | `	{ "preg_replace_callback", "array\|string $pattern, callable $callback, array\|string $subject, int $limit = -1, &$count = NULL, int $flags = 0", "array\|string\|null" },` |
|         - |  782 | `	{ "preg_split", "string $pattern, string $subject, int $limit = -1, int $flags = 0", "array\|false" },` |
|         - |  783 | `	{ "prev", "object\|array &$array", "mixed" },` |
|         - |  784 | `	{ "print_r", "mixed $value, bool $return = false", "string\|true" },` |
|         - |  785 | `	{ "printf", "string $format, mixed ...$values = ?", "int" },` |
|         - |  786 | `	{ "property_exists", "$object_or_class, string $property", "bool" },` |
|         - |  787 | `	{ "putenv", "string $assignment", "bool" },` |
|         - |  788 | `	{ "quotemeta", "string $string", "string" },` |
|         - |  789 | `	{ "rad2deg", "float $num", "float" },` |
|         - |  790 | `	{ "rand", "int $min = ?, int $max = ?", "int" },` |
|         - |  791 | `	{ "random_bytes", "int $length", "string" },` |
|         - |  792 | `	{ "random_int", "int $min, int $max", "int" },` |
|         - |  793 | `	{ "range", "string\|int\|float $start, string\|int\|float $end, int\|float $step = 1", "array" },` |
|         - |  794 | `	{ "rawurldecode", "string $string", "string" },` |
|         - |  795 | `	{ "rawurlencode", "string $string", "string" },` |
|         - |  796 | `	{ "readdir", "$dir_handle = NULL", "string\|false" },` |
|         - |  797 | `	{ "readfile", "string $filename, bool $use_include_path = false, $context = NULL", "int\|false" },` |
|         - |  798 | `	{ "readlink", "string $path", "string\|false" },` |
|         - |  799 | `	{ "realpath", "string $path", "string\|false" },` |
|         - |  800 | `	{ "register_shutdown_function", "callable $callback, mixed ...$args = ?", "void" },` |
|         - |  801 | `	{ "rename", "string $from, string $to, $context = NULL", "bool" },` |
|         - |  802 | `	{ "reset", "object\|array &$array", "mixed" },` |
|         - |  803 | `	{ "restore_error_handler", "", "true" },` |
|         - |  804 | `	{ "restore_exception_handler", "", "true" },` |
|         - |  805 | `	{ "rewind", "$stream", "bool" },` |
|         - |  806 | `	{ "rewinddir", "$dir_handle = NULL", "void" },` |
|         - |  807 | `	{ "rmdir", "string $directory, $context = NULL", "bool" },` |
|         - |  808 | `	{ "round", "int\|float $num, int $precision = 0, RoundingMode\|int $mode = ?", "float" },` |
|         - |  809 | `	{ "rsort", "array &$array, int $flags = 0", "true" },` |
|         - |  810 | `	{ "rtrim", "string $string, string $characters = ?", "string" },` |
|         - |  811 | `	{ "serialize", "mixed $value", "string" },` |
|         - |  812 | `	{ "set_error_handler", "?callable $callback, int $error_levels = 30719", "" },` |
|         - |  813 | `	{ "set_exception_handler", "?callable $callback", "" },` |
|         - |  814 | `	{ "get_error_handler", "", "?callable" },` |
|         - |  815 | `	{ "get_exception_handler", "", "?callable" },` |
|         - |  816 | `	{ "hrtime", "bool $as_number = false", "array\|int\|float\|false" },` |
|         - |  817 | `	{ "mb_check_encoding", "array\|string\|null $value = NULL, ?string $encoding = NULL", "bool" },` |
|         - |  818 | `	{ "mb_convert_case", "string $string, int $mode, ?string $encoding = NULL", "string" },` |
|         - |  819 | `	{ "mb_detect_encoding", "string $string, array\|string\|null $encodings = NULL, bool $strict = false", "string\|false" },` |
|         - |  820 | `	{ "mb_internal_encoding", "?string $encoding = NULL", "string\|bool" },` |
|         - |  821 | `	{ "mb_str_split", "string $string, int $length = 1, ?string $encoding = NULL", "array" },` |
|         - |  822 | `	{ "mb_stripos", "string $haystack, string $needle, int $offset = 0, ?string $encoding = NULL", "int\|false" },` |
|         - |  823 | `	{ "mb_strlen", "string $string, ?string $encoding = NULL", "int" },` |
|         - |  824 | `	{ "mb_strpos", "string $haystack, string $needle, int $offset = 0, ?string $encoding = NULL", "int\|false" },` |
|         - |  825 | `	{ "mb_strrpos", "string $haystack, string $needle, int $offset = 0, ?string $encoding = NULL", "int\|false" },` |
|         - |  826 | `	{ "mb_strwidth", "string $string, ?string $encoding = NULL", "int" },` |
|         - |  827 | `	{ "mb_substr", "string $string, int $start, ?int $length = NULL, ?string $encoding = NULL", "string" },` |
|         - |  828 | `	{ "memory_reset_peak_usage", "", "void" },` |
|         - |  829 | `	{ "proc_close", "$process", "int" },` |
|         - |  830 | `	{ "proc_get_status", "$process", "array" },` |
|         - |  831 | `	{ "proc_nice", "int $priority", "bool" },` |
|         - |  832 | `	{ "proc_open", "array\|string $command, array $descriptor_spec, &$pipes, ?string $cwd = NULL, ?array $env_vars = NULL, ?array $options = NULL", "" },` |
|         - |  833 | `	{ "proc_terminate", "$process, int $signal = 15", "bool" },` |
|         - |  834 | `	{ "set_include_path", "string $include_path", "string\|false" },` |
|         - |  835 | `	{ "setcookie", "string $name, string $value = '', array\|int $expires_or_options = 0, string $path = '', string $domain = '', bool $secure = false, bool $httponly = false", "bool" },` |
|         - |  836 | `	{ "setrawcookie", "string $name, string $value = '', array\|int $expires_or_options = 0, string $path = '', string $domain = '', bool $secure = false, bool $httponly = false", "bool" },` |
|         - |  837 | `	{ "settype", "mixed &$var, string $type", "bool" },` |
|         - |  838 | `	{ "sha1", "string $string, bool $binary = false", "string" },` |
|         - |  839 | `	{ "sha1_file", "string $filename, bool $binary = false", "string\|false" },` |
|         - |  840 | `	{ "shell_exec", "string $command", "string\|false\|null" },` |
|         - |  841 | `	{ "shuffle", "array &$array", "true" },` |
|         - |  842 | `	{ "similar_text", "string $string1, string $string2, &$percent = NULL", "int" },` |
|         - |  843 | `	{ "sin", "float $num", "float" },` |
|         - |  844 | `	{ "sinh", "float $num", "float" },` |
|         - |  845 | `	{ "sizeof", "Countable\|array $value, int $mode = 0", "int" },` |
|         - |  846 | `	{ "sleep", "int $seconds", "int" },` |
|         - |  847 | `	{ "sort", "array &$array, int $flags = 0", "true" },` |
|         - |  848 | `	{ "soundex", "string $string", "string" },` |
|         - |  849 | `	{ "spl_autoload", "string $class, ?string $file_extensions = NULL", "void" },` |
|         - |  850 | `	{ "spl_autoload_functions", "", "array" },` |
|         - |  851 | `	{ "spl_autoload_register", "?callable $callback = NULL, bool $throw = true, bool $prepend = false", "bool" },` |
|         - |  852 | `	{ "spl_autoload_unregister", "callable $callback", "bool" },` |
|         - |  853 | `	{ "spl_object_hash", "object $object", "string" },` |
|         - |  854 | `	{ "spl_object_id", "object $object", "int" },` |
|         - |  855 | `	{ "sprintf", "string $format, mixed ...$values = ?", "string" },` |
|         - |  856 | `	{ "sqrt", "float $num", "float" },` |
|         - |  857 | `	{ "srand", "?int $seed = NULL, int $mode = 0", "void" },` |
|         - |  858 | `	{ "stat", "string $filename", "array\|false" },` |
|         - |  859 | `	{ "str_contains", "string $haystack, string $needle", "bool" },` |
|         - |  860 | `	{ "str_ends_with", "string $haystack, string $needle", "bool" },` |
|         - |  861 | `	{ "str_getcsv", "string $string, string $separator = ',', string $enclosure = '\"', string $escape = '\\\\'", "array" },` |
|         - |  862 | `	{ "str_ireplace", "array\|string $search, array\|string $replace, array\|string $subject, &$count = NULL", "array\|string" },` |
|         - |  863 | `	{ "str_pad", "string $string, int $length, string $pad_string = ' ', int $pad_type = 1", "string" },` |
|         - |  864 | `	{ "str_repeat", "string $string, int $times", "string" },` |
|         - |  865 | `	{ "str_replace", "array\|string $search, array\|string $replace, array\|string $subject, &$count = NULL", "array\|string" },` |
|         - |  866 | `	{ "str_rot13", "string $string", "string" },` |
|         - |  867 | `	{ "str_shuffle", "string $string", "string" },` |
|         - |  868 | `	{ "str_split", "string $string, int $length = 1", "array" },` |
|         - |  869 | `	{ "str_starts_with", "string $haystack, string $needle", "bool" },` |
|         - |  870 | `	{ "str_word_count", "string $string, int $format = 0, ?string $characters = NULL", "array\|int" },` |
|         - |  871 | `	{ "strcasecmp", "string $string1, string $string2", "int" },` |
|         - |  872 | `	{ "strchr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|         - |  873 | `	{ "strcmp", "string $string1, string $string2", "int" },` |
|         - |  874 | `	{ "strnatcasecmp", "string $string1, string $string2", "int" },` |
|         - |  875 | `	{ "strnatcmp", "string $string1, string $string2", "int" },` |
|         - |  876 | `	{ "strcoll", "string $string1, string $string2", "int" },` |
|         - |  877 | `	{ "strcspn", "string $string, string $characters, int $offset = 0, ?int $length = NULL", "int" },` |
|         - |  878 | `	{ "stream_context_create", "?array $options = NULL, ?array $params = NULL", "" },` |
|         - |  879 | `	{ "stream_get_contents", "$stream, ?int $length = NULL, int $offset = -1", "string\|false" },` |
|         - |  880 | `	{ "stream_get_line", "$stream, int $length, string $ending = ''", "string\|false" },` |
|         - |  881 | `	{ "stream_get_meta_data", "$stream", "array" },` |
|         - |  882 | `	{ "stream_get_wrappers", "", "array" },` |
|         - |  883 | `	{ "stream_register_wrapper", "string $protocol, string $class, int $flags = 0", "bool" },` |
|         - |  884 | `	{ "stream_wrapper_register", "string $protocol, string $class, int $flags = 0", "bool" },` |
|         - |  885 | `	{ "stream_wrapper_unregister", "string $protocol", "bool" },` |
|         - |  886 | `	{ "strip_tags", "string $string, array\|string\|null $allowed_tags = NULL", "string" },` |
|         - |  887 | `	{ "stripos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|         - |  888 | `	{ "stripslashes", "string $string", "string" },` |
|         - |  889 | `	{ "stristr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|         - |  890 | `	{ "strlen", "string $string", "int" },` |
|         - |  891 | `	{ "strncasecmp", "string $string1, string $string2, int $length", "int" },` |
|         - |  892 | `	{ "strncmp", "string $string1, string $string2, int $length", "int" },` |
|         - |  893 | `	{ "strpbrk", "string $string, string $characters", "string\|false" },` |
|         - |  894 | `	{ "strpos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|         - |  895 | `	{ "strrchr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|         - |  896 | `	{ "strrev", "string $string", "string" },` |
|         - |  897 | `	{ "strripos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|         - |  898 | `	{ "strrpos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|         - |  899 | `	{ "strspn", "string $string, string $characters, int $offset = 0, ?int $length = NULL", "int" },` |
|         - |  900 | `	{ "strstr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|         - |  901 | `	{ "strtok", "string $string, ?string $token = NULL", "string\|false" },` |
|         - |  902 | `	{ "strtolower", "string $string", "string" },` |
|         - |  903 | `	{ "strtotime", "string $datetime, ?int $baseTimestamp = NULL", "int\|false" },` |
|         - |  904 | `	{ "strtoupper", "string $string", "string" },` |
|         - |  905 | `	{ "strtr", "string $string, array\|string $from, ?string $to = NULL", "string" },` |
|         - |  906 | `	{ "strval", "mixed $value", "string" },` |
|         - |  907 | `	{ "substr", "string $string, int $offset, ?int $length = NULL", "string" },` |
|         - |  908 | `	{ "substr_compare", "string $haystack, string $needle, int $offset, ?int $length = NULL, bool $case_insensitive = false", "int" },` |
|         - |  909 | `	{ "substr_count", "string $haystack, string $needle, int $offset = 0, ?int $length = NULL", "int" },` |
|         - |  910 | `	{ "substr_replace", "array\|string $string, array\|string $replace, array\|int $offset, array\|int\|null $length = NULL", "array\|string" },` |
|         - |  911 | `	{ "symlink", "string $target, string $link", "bool" },` |
|         - |  912 | `	{ "sys_get_temp_dir", "", "string" },` |
|         - |  913 | `	{ "system", "string $command, &$result_code = NULL", "string\|false" },` |
|         - |  914 | `	{ "tan", "float $num", "float" },` |
|         - |  915 | `	{ "tanh", "float $num", "float" },` |
|         - |  916 | `	{ "time", "", "int" },` |
|         - |  917 | `	{ "token_get_all", "string $code, int $flags = 0", "array" },` |
|         - |  918 | `	{ "token_name", "int $id", "string" },` |
|         - |  919 | `	{ "touch", "string $filename, ?int $mtime = NULL, ?int $atime = NULL", "bool" },` |
|         - |  920 | `	{ "trigger_error", "string $message, int $error_level = 1024", "true" },` |
|         - |  921 | `	{ "trim", "string $string, string $characters = ?", "string" },` |
|         - |  922 | `	{ "uasort", "array &$array, callable $callback", "true" },` |
|         - |  923 | `	{ "ucfirst", "string $string", "string" },` |
|         - |  924 | `	{ "ucwords", "string $string, string $separators = ?", "string" },` |
|         - |  925 | `	{ "uksort", "array &$array, callable $callback", "true" },` |
|         - |  926 | `	{ "umask", "?int $mask = NULL", "int" },` |
|         - |  927 | `	{ "uniqid", "string $prefix = '', bool $more_entropy = false", "string" },` |
|         - |  928 | `	{ "unlink", "string $filename, $context = NULL", "bool" },` |
|         - |  929 | `	{ "unserialize", "string $data, array $options = ?", "mixed" },` |
|         - |  930 | `	{ "urldecode", "string $string", "string" },` |
|         - |  931 | `	{ "urlencode", "string $string", "string" },` |
|         - |  932 | `	{ "user_error", "string $message, int $error_level = 1024", "true" },` |
|         - |  933 | `	{ "usleep", "int $microseconds", "void" },` |
|         - |  934 | `	{ "usort", "array &$array, callable $callback", "true" },` |
|         - |  935 | `	{ "var_dump", "mixed $value, mixed ...$values = ?", "void" },` |
|         - |  936 | `	{ "var_export", "mixed $value, bool $return = false", "?string" },` |
|         - |  937 | `	{ "version_compare", "string $version1, string $version2, ?string $operator = null", "int\|bool" },` |
|         - |  938 | `	{ "vfprintf", "$stream, string $format, array $values", "int" },` |
|         - |  939 | `	{ "vprintf", "string $format, array $values", "int" },` |
|         - |  940 | `	{ "vsprintf", "string $format, array $values", "string" },` |
|         - |  941 | `	{ "wordwrap", "string $string, int $width = 75, string $break = ?, bool $cut_long_words = false", "string" },` |
|         - |  942 | `	{ "zip_close", "$zip", "void" },` |
|         - |  943 | `	{ "zip_entry_close", "$zip_entry", "bool" },` |
|         - |  944 | `	{ "zip_entry_compressedsize", "$zip_entry", "int\|false" },` |
|         - |  945 | `	{ "zip_entry_compressionmethod", "$zip_entry", "string\|false" },` |
|         - |  946 | `	{ "zip_entry_filesize", "$zip_entry", "int\|false" },` |
|         - |  947 | `	{ "zip_entry_name", "$zip_entry", "string\|false" },` |
|         - |  948 | `	{ "zip_entry_open", "$zip_dp, $zip_entry, string $mode = 'rb'", "bool" },` |
|         - |  949 | `	{ "zip_entry_read", "$zip_entry, int $len = 1024", "string\|false" },` |
|         - |  950 | `	{ "zip_open", "string $filename", "" },` |
|         - |  951 | `	{ "zip_read", "$zip", "" },` |
|         - |  952 | `};` |
|         - |  953 | `/*` |
|         - |  954 | ` * Stamp the signature strings onto the registered host functions.` |
|         - |  955 | ` * Runs once at PH7_VmMakeReady, after the builtins are registered.` |
|         - |  956 | ` */` |
|         - |  957 | `/*` |
|         - |  958 | ` * Derive the minimum arity from a builtin's declared signature: a parameter is` |
|         - |  959 | ` * optional when it carries a default ("int $length = 76") or is variadic` |
|         - |  960 | ` * ("...$rest"), so the minimum is the count of the parameters that are neither,` |
|         - |  961 | ` * and the ZPP wording is "at least" as soon as one optional/variadic exists.` |
|         - |  962 | ` *` |
|         - |  963 | ` * This makes aBuiltinSig[] the single source of truth for arity — the curated` |
|         - |  964 | ` * aBuiltinArity[] table above stays as an OVERRIDE for the handful of builtins` |
|         - |  965 | ` * php reports differently from their own signature (e.g. strtr(), whose 3-arg` |
|         - |  966 | ` * form still reports "expects exactly 2", and the array_udiff() family, whose` |
|         - |  967 | ` * variadic tail hides a second required argument). Verified against php 8.5.7` |
|         - |  968 | ` * for all 462 signed builtins: 458 derive exactly, 4 are overridden.` |
|         - |  969 | ` */` |
|         - |  970 | `/*` |
|         - |  971 | ` * A DEFAULT can contain the parameter separator: php declares` |
|         - |  972 | `` * `string $separator = ','` and `string $enclosure = '"'`. Every scan of a`` |
|         - |  973 | ` * signature therefore has to step over a quoted run, or the comma inside one` |
|         - |  974 | ` * splits the parameter in two — which is how fgetcsv()/fputcsv()/str_getcsv()` |
|         - |  975 | ` * came to count SIX parameters and accept a fifth argument php refuses.` |
|         - |  976 | ` * Answers the position of the closing quote (or of the NUL when the run is` |
|         - |  977 | ` * unterminated); the caller advances past it.` |
|         - |  978 | ` */` |
|   1797454 |  979 | `static const char *VmSigSkipQuoted(const char *zCur)` |
|         5 |  980 | `{` |
|   1797459 |  981 | `	char c = zCur[0];` |
|   1797459 |  982 | `	if( c != '\'' && c != '"' ){` |
|       ! 0 |  983 | `		return zCur;` |
|         - |  984 | `	}` |
|   2312015 |  985 | `	for( zCur++ ; zCur[0] ; zCur++ ){` |
|   2312015 |  986 | `		if( zCur[0] == '\\' && zCur[1] ){` |
|     24501 |  987 | `			zCur++;` |
|     24501 |  988 | `			continue;` |
|         - |  989 | `		}` |
|   2287519 |  990 | `		if( zCur[0] == c ){` |
|   1797459 |  991 | `			break;` |
|         - |  992 | `		}` |
|    245035 |  993 | `	}` |
|   1797459 |  994 | `	return zCur;` |
|    898732 |  995 | `}` |
|   5812592 |  996 | `PH7_PRIVATE void VmDeriveArityFromSig(const char *zSig,sxi16 *pnMin,sxu8 *pbAtLeast,sxi16 *pnMax,sxu8 *pbHasMax)` |
|         5 |  997 | `{` |
|   5812597 |  998 | `	const char *zCur = zSig;` |
|   5812597 |  999 | `	int nMin = 0, bAtLeast = 0, bSeen = 0, bOptional = 0;` |
|   5812597 | 1000 | `	int nTotal = 0, bVariadic = 0;` |
|  53126497 | 1001 | `	for(;;){` |
| 109008151 | 1002 | `		if( zCur[0] == '\'' \|\| zCur[0] == '"' ){` |
|    170767 | 1003 | `			bSeen = 1;` |
|    170767 | 1004 | `			zCur = VmSigSkipQuoted(zCur);` |
|    170767 | 1005 | `			if( zCur[0] != '\0' ){` |
|    170767 | 1006 | `				zCur++;` |
|     85381 | 1007 | `			}` |
|    170767 | 1008 | `			continue;` |
|         - | 1009 | `		}` |
| 108837389 | 1010 | `		if( zCur[0] == '\0' \|\| zCur[0] == ',' ){` |
|   8396987 | 1011 | `			if( bSeen ){` |
|   5987237 | 1012 | `				nTotal++;` |
|   5987237 | 1013 | `				if( bOptional ){` |
|   2193181 | 1014 | `					bAtLeast = 1;` |
|   1096593 | 1015 | `				}else{` |
|   3794061 | 1016 | `					nMin++;` |
|         - | 1017 | `				}` |
|   2993616 | 1018 | `			}` |
|   8396987 | 1019 | `			if( zCur[0] == '\0' ){` |
|   5812597 | 1020 | `				break;` |
|         - | 1021 | `			}` |
|   2584395 | 1022 | `			bSeen = bOptional = 0;` |
|   2584395 | 1023 | `			zCur++;` |
|   2584395 | 1024 | `			continue;` |
|         - | 1025 | `		}` |
| 100440407 | 1026 | `		if( zCur[0] != ' ' ){` |
|  87963135 | 1027 | `			bSeen = 1;` |
|  43981565 | 1028 | `		}` |
| 100440407 | 1029 | `		if( zCur[0] == '=' \|\| (zCur[0] == '.' && zCur[1] == '.' && zCur[2] == '.') ){` |
|   2327689 | 1030 | `			bOptional = 1;` |
|   1163842 | 1031 | `		}` |
| 100440407 | 1032 | `		if( zCur[0] == '.' && zCur[1] == '.' && zCur[2] == '.' ){` |
|    157863 | 1033 | `			bVariadic = 1;` |
|     78929 | 1034 | `		}` |
| 100440407 | 1035 | `		zCur++;` |
|         5 | 1036 | `	}` |
|   5812597 | 1037 | `	*pnMin = (sxi16)nMin;` |
|   5812597 | 1038 | `	*pbAtLeast = (sxu8)bAtLeast;` |
|         - | 1039 | `	/* php enforces a MAXIMUM too ("expects at most 1 argument, 2 given"); a` |
|         - | 1040 | `	 * variadic tail means there is none. The parameter COUNT is the maximum,` |
|         - | 1041 | `	 * whether or not the parameters carry defaults. */` |
|   5812597 | 1042 | `	*pnMax = (sxi16)nTotal;` |
|   5812597 | 1043 | `	*pbHasMax = (sxu8)(bVariadic ? 0 : 1);` |
|   5812597 | 1044 | `}` |
|         - | 1045 | `/*` |
|         - | 1046 | ` * Does the declared type list (e.g. "array\|string", "?int", "callable") contain` |
|         - | 1047 | ` * the given token? Compares against each '\|'-separated alternative, ignoring a` |
|         - | 1048 | ` * leading nullable '?'.` |
|         - | 1049 | ` */` |
|  11372392 | 1050 | `static int VmSigTypeHas(const char *zType,int nType,const char *zTok)` |
|         5 | 1051 | `{` |
|  11372397 | 1052 | `	int nTok = (int)SyStrlen(zTok);` |
|  11372397 | 1053 | `	int i = 0;` |
|  11372397 | 1054 | `	if( zType[0] == '?' ){` |
|    614469 | 1055 | `		zType++;` |
|    614469 | 1056 | `		nType--;` |
|    307232 | 1057 | `	}` |
|  23227227 | 1058 | `	while( i < nType ){` |
|  12022681 | 1059 | `		int j = i;` |
|  79029999 | 1060 | `		while( j < nType && zType[j] != '\|' ){` |
|  67007323 | 1061 | `			j++;` |
|         5 | 1062 | `		}` |
|  12022681 | 1063 | `		if( j - i == nTok && SyMemcmp(&zType[i],zTok,(sxu32)nTok) == 0 ){` |
|    167851 | 1064 | `			return 1;` |
|         - | 1065 | `		}` |
|  11854835 | 1066 | `		i = j + 1;` |
|         5 | 1067 | `	}` |
|  11204551 | 1068 | `	return 0;` |
|   5689334 | 1069 | `}` |
|         - | 1070 | `/*` |
|         - | 1071 | `` * Is EVERY arm of the declared type list `array` (a bare `array`, or `?array`,`` |
|         - | 1072 | `` * or the `array\|null` union that spells the same thing)? Such a parameter has`` |
|         - | 1073 | ` * no arm a scalar can satisfy, and php refuses one outright.` |
|         - | 1074 | ` *` |
|         - | 1075 | `` * The screen used to exempt any type list carrying an `array` arm, union or`` |
|         - | 1076 | `` * not, for a wording reason: php's `array\|object` parameters come from ONE ZPP`` |
|         - | 1077 | ` * macro (Z_PARAM_ARRAY_OR_OBJECT) that names only "array" in the refusal, so` |
|         - | 1078 | ` * the declared type is not the text php prints. That ambiguity does not exist` |
|         - | 1079 | `` * for a parameter typed exactly `array` -- there is one arm and php prints it.`` |
|         - | 1080 | ` */` |
|   3148974 | 1081 | `static int VmSigTypeIsArrayOnly(const char *zType,int nType)` |
|         5 | 1082 | `{` |
|   3148979 | 1083 | `	int i = 0, bArray = 0;` |
|   3148979 | 1084 | `	if( zType[0] == '?' ){` |
|    291847 | 1085 | `		zType++;` |
|    291847 | 1086 | `		nType--;` |
|    145921 | 1087 | `	}` |
|   3307893 | 1088 | `	while( i < nType ){` |
|   3307671 | 1089 | `		int j = i;` |
|  20818301 | 1090 | `		while( j < nType && zType[j] != '\|' ){` |
|  17510635 | 1091 | `			j++;` |
|         5 | 1092 | `		}` |
|   3307671 | 1093 | `		if( j > i ){` |
|   3307666 | 1094 | `			if( j - i == (int)sizeof("array")-1` |
|   1734311 | 1095 | `			 && SyMemcmp(&zType[i],"array",sizeof("array")-1) == 0 ){` |
|    158919 | 1096 | `				bArray = 1;` |
|   3231089 | 1097 | `			}else if( !(j - i == (int)sizeof("null")-1` |
|   1578099 | 1098 | `			         && SyMemcmp(&zType[i],"null",sizeof("null")-1) == 0) ){` |
|   3148757 | 1099 | `				return 0;` |
|         - | 1100 | `			}` |
|     79457 | 1101 | `		}` |
|    158919 | 1102 | `		i = j + 1;` |
|         5 | 1103 | `	}` |
|       227 | 1104 | `	return bArray;` |
|   1575340 | 1105 | `}` |
|         - | 1106 | `/*` |
|         - | 1107 | ` * Does the declared type list name a CLASS (anything that is not one of php's` |
|         - | 1108 | ` * builtin type keywords)? A class-typed parameter accepts an object, so it must` |
|         - | 1109 | ` * not be rejected by the array/object/resource screen below.` |
|         - | 1110 | ` */` |
|         - | 1111 | `/* Is this one arm of a declared type a BUILTIN type name rather than a class? */` |
|   3321300 | 1112 | `static int VmSigArmIsBuiltinType(const char *zArm,int nArm)` |
|         5 | 1113 | `{` |
|         - | 1114 | `	static const char *azBuiltin[] = {` |
|         - | 1115 | `		"int","float","string","bool","array","object","callable","iterable",` |
|         - | 1116 | `		"mixed","null","void","resource","false","true","never","self","static"` |
|         - | 1117 | `	};` |
|         - | 1118 | `	int k;` |
|   8930849 | 1119 | `	for( k = 0 ; k < (int)SX_ARRAYSIZE(azBuiltin) ; ++k ){` |
|   8927245 | 1120 | `		int nB = (int)SyStrlen(azBuiltin[k]);` |
|   8927245 | 1121 | `		if( nArm == nB && SyMemcmp(zArm,azBuiltin[k],(sxu32)nB) == 0 ){` |
|   3317701 | 1122 | `			return 1;` |
|         - | 1123 | `		}` |
|   2806049 | 1124 | `	}` |
|      3609 | 1125 | `	return 0;` |
|   1661503 | 1126 | `}` |
|   3158244 | 1127 | `static int VmSigTypeHasClass(const char *zType,int nType)` |
|         5 | 1128 | `{` |
|   3158249 | 1129 | `	int i = 0;` |
|   3158249 | 1130 | `	if( zType[0] == '?' ){` |
|    292975 | 1131 | `		zType++;` |
|    292975 | 1132 | `		nType--;` |
|    146485 | 1133 | `	}` |
|   6475943 | 1134 | `	while( i < nType ){` |
|   3320263 | 1135 | `		int j = i;` |
|  20912051 | 1136 | `		while( j < nType && zType[j] != '\|' ){` |
|  17591793 | 1137 | `			j++;` |
|         5 | 1138 | `		}` |
|   3320263 | 1139 | `		if( j > i && !VmSigArmIsBuiltinType(&zType[i],j - i) ){` |
|      2569 | 1140 | `			return 1;` |
|         - | 1141 | `		}` |
|   3317699 | 1142 | `		i = j + 1;` |
|         5 | 1143 | `	}` |
|   3155685 | 1144 | `	return 0;` |
|   1579975 | 1145 | `}` |
|         - | 1146 | `/*` |
|         - | 1147 | ` * php's ", X given" tail: like ph7_type_name() but an object reports its CLASS,` |
|         - | 1148 | ` * which is what php prints in a TypeError.` |
|         - | 1149 | ` */` |
|         - | 1150 | `/*` |
|         - | 1151 | ` * Does pObj satisfy any CLASS arm of a declared type?` |
|         - | 1152 | ` *` |
|         - | 1153 | ` * Answers TRUE (unscreened) when an arm names something this VM has not declared:` |
|         - | 1154 | ` * the signatures describe php's surface, parts of which PHL models differently` |
|         - | 1155 | ` * (the resource-backed handles the RES branch below already excuses), and a name` |
|         - | 1156 | ` * that resolves to nothing must not turn into a rejection of a valid argument.` |
|         - | 1157 | ` */` |
|      1040 | 1158 | `static int VmSigObjSatisfiesClass(ph7_vm *pVm,const char *zType,int nType,` |
|         - | 1159 | `	ph7_class_instance *pObj)` |
|         5 | 1160 | `{` |
|      1045 | 1161 | `	int i = 0;` |
|      1045 | 1162 | `	if( pObj == 0 \|\| pObj->pClass == 0 ){` |
|       ! 0 | 1163 | `		return 1;` |
|         - | 1164 | `	}` |
|      1045 | 1165 | `	if( zType[0] == '?' ){` |
|       217 | 1166 | `		zType++;` |
|       217 | 1167 | `		nType--;` |
|       107 | 1168 | `	}` |
|      1067 | 1169 | `	while( i < nType ){` |
|      1047 | 1170 | `		int j = i;` |
|     11705 | 1171 | `		while( j < nType && zType[j] != '\|' ){` |
|     10663 | 1172 | `			j++;` |
|         5 | 1173 | `		}` |
|      1047 | 1174 | `		if( j > i && !VmSigArmIsBuiltinType(&zType[i],j - i) ){` |
|      1045 | 1175 | `			ph7_class *pClass = PH7_VmExtractClass(&(*pVm),&zType[i],(sxu32)(j - i),FALSE,0);` |
|      1045 | 1176 | `			if( pClass == 0 ){` |
|         - | 1177 | `				/* Either a builtin type name (already excluded by the caller) or a` |
|         - | 1178 | `				 * class this build does not declare: nothing to judge. */` |
|       ! 0 | 1179 | `				return 1;` |
|         - | 1180 | `			}` |
|      1045 | 1181 | `			if( PH7_VmInstanceOf(pObj->pClass,pClass) ){` |
|      1025 | 1182 | `				return 1;` |
|         - | 1183 | `			}` |
|        10 | 1184 | `		}` |
|        24 | 1185 | `		i = j + 1;` |
|         2 | 1186 | `	}` |
|        22 | 1187 | `	return 0;` |
|       525 | 1188 | `}` |
|        98 | 1189 | `static const char * VmArgTypeName(ph7_value *pVal)` |
|         4 | 1190 | `{` |
|       102 | 1191 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       102 | 1192 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       102 | 1193 | `		if( pInst && pInst->pClass ){` |
|       102 | 1194 | `			return pInst->pClass->sName.zString;` |
|         - | 1195 | `		}` |
|       ! 0 | 1196 | `	}` |
|       ! 0 | 1197 | `	return ph7_type_name(pVal);` |
|        53 | 1198 | `}` |
|         - | 1199 | `/*` |
|         - | 1200 | `` * Does pArg satisfy a `string` parameter? The rule VmEnforceBuiltinArgTypes()`` |
|         - | 1201 | ` * below applies, factored out so a builtin that words its own overload dispatch` |
|         - | 1202 | ` * (strtr(), whose expected type depends on the ARITY and so cannot be spelled in` |
|         - | 1203 | ` * one signature) decides identically instead of forking the logic. An array never` |
|         - | 1204 | ` * satisfies one; an object does only through __toString(); a resource does not;` |
|         - | 1205 | ` * null does under php, with a deprecation, but not under PHL's §10 null-strictness` |
|         - | 1206 | ` * policy — the screen and this helper both report it as a mismatch.` |
|         - | 1207 | ` */` |
|     39894 | 1208 | `PH7_PRIVATE int PH7_ArgSatisfiesString(ph7_value *pArg)` |
|         5 | 1209 | `{` |
|     39899 | 1210 | `	if( (pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_NULL\|MEMOBJ_RES)) != 0 ){` |
|        23 | 1211 | `		return 0;` |
|         - | 1212 | `	}` |
|     39879 | 1213 | `	if( (pArg->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       138 | 1214 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|       138 | 1215 | `		return pInst && PH7_ClassExtractMethod(pInst->pClass,"__toString",` |
|        67 | 1216 | `			sizeof("__toString")-1) != 0;` |
|         - | 1217 | `	}` |
|     39745 | 1218 | `	return 1;` |
|     19952 | 1219 | `}` |
|         - | 1220 | `/*` |
|         - | 1221 | `` * Is the declared type exactly `int` — the only shape whose float argument the`` |
|         - | 1222 | `` * screen below can decide? A union with a `float`, `string` or `bool` arm has its`` |
|         - | 1223 | ` * own coercion rules per arm (and php words those refusals from the builtin), so` |
|         - | 1224 | ` * only the plain form and its nullable spelling qualify.` |
|         - | 1225 | ` */` |
|       624 | 1226 | `static int VmSigTypeIsIntOnly(const char *zType,int nType)` |
|         5 | 1227 | `{` |
|       629 | 1228 | `	if( nType > 0 && zType[0] == '?' ){` |
|        39 | 1229 | `		zType++;` |
|        39 | 1230 | `		nType--;` |
|        19 | 1231 | `	}` |
|       629 | 1232 | `	if( nType == (int)sizeof("int")-1 && SyMemcmp(zType,"int",3) == 0 ){` |
|       152 | 1233 | `		return 1;` |
|         - | 1234 | `	}` |
|         - | 1235 | ``	/* `int\|null` / `null\|int`, the union spelling of `?int`. */`` |
|       644 | 1236 | `	return VmSigTypeHas(zType,nType,"int") && VmSigTypeHas(zType,nType,"null")` |
|       165 | 1237 | `	    && !VmSigTypeHas(zType,nType,"float")` |
|       ! 0 | 1238 | `	    && !VmSigTypeHas(zType,nType,"string")` |
|       ! 0 | 1239 | `	    && !VmSigTypeHas(zType,nType,"bool")` |
|       ! 0 | 1240 | `	    && !VmSigTypeHas(zType,nType,"array")` |
|       ! 0 | 1241 | `	    && !VmSigTypeHas(zType,nType,"object")` |
|       ! 0 | 1242 | `	    && !VmSigTypeHas(zType,nType,"iterable")` |
|       ! 0 | 1243 | `	    && !VmSigTypeHas(zType,nType,"callable")` |
|       641 | 1244 | `	    && !VmSigTypeHasClass(zType,nType);` |
|       317 | 1245 | `}` |
|         - | 1246 | `/*` |
|         - | 1247 | `` * Can this float reach an `int` parameter without losing anything? php's rule is`` |
|         - | 1248 | ` * php_parse_arg_long's: in range, and integral. NaN and the infinities are out by` |
|         - | 1249 | ` * the range test (a NaN compares false against both bounds, which is why the test` |
|         - | 1250 | ` * is written as a pair of accepts rather than a pair of rejects).` |
|         - | 1251 | ` */` |
|        96 | 1252 | `static int VmDoubleFitsInt(double d)` |
|         4 | 1253 | `{` |
|       100 | 1254 | `	if( !(d >= -9223372036854775808.0 && d < 9223372036854775808.0) ){` |
|        50 | 1255 | `		return 0;` |
|         - | 1256 | `	}` |
|        52 | 1257 | `	return d == (double)(sxi64)d;` |
|        52 | 1258 | `}` |
|         - | 1259 | `/*` |
|         - | 1260 | ` * The same question for a NUMERIC string, which php asks with the same answer:` |
|         - | 1261 | `` * `dechex("1e19")` and `dechex("99999999999999999999")` are both`` |
|         - | 1262 | `` * `must be of type int, string given`. RangeStrToNumber is php's`` |
|         - | 1263 | ` * is_numeric_string grammar and already reclassifies an integer too wide for an` |
|         - | 1264 | ` * sxi64 as a DOUBLE, so the two shapes converge on one test.` |
|         - | 1265 | ` */` |
|        76 | 1266 | `static int VmNumStrFitsInt(ph7_value *pArg)` |
|         3 | 1267 | `{` |
|         - | 1268 | `	const char *zStr;` |
|        79 | 1269 | `	int nLen = 0;` |
|        79 | 1270 | `	sxi64 iVal = 0;` |
|        79 | 1271 | `	double dVal = 0;` |
|        79 | 1272 | `	zStr = ph7_value_to_string(pArg,&nLen);` |
|        79 | 1273 | `	switch( RangeStrToNumber(zStr,(sxu32)nLen,&iVal,&dVal) ){` |
|        54 | 1274 | `	case RANGE_IN_LONG:   return 1;` |
|        27 | 1275 | `	case RANGE_IN_DOUBLE: return VmDoubleFitsInt(dVal);` |
|       ! 0 | 1276 | `	default:              return 0;` |
|         - | 1277 | `	}` |
|        41 | 1278 | `}` |
|         - | 1279 | `/*` |
|         - | 1280 | ` * PHP-8 PATH parameters: which positions carry a filesystem path, a shell` |
|         - | 1281 | ` * command or an include-path list rather than an ordinary string.` |
|         - | 1282 | ` *` |
|         - | 1283 | ` * php spells this in the ZPP macro, not in the declared type: a path parameter` |
|         - | 1284 | `` * is `Z_PARAM_PATH` where an ordinary one is `Z_PARAM_STR`, and both print as`` |
|         - | 1285 | `` * `string` in the stub Reflection reads. The difference is a single rule — a`` |
|         - | 1286 | ` * path may not contain a NUL byte — and php raises a catchable ValueError for` |
|         - | 1287 | ` * one that does, BEFORE the call reaches the filesystem.` |
|         - | 1288 | ` *` |
|         - | 1289 | ` * PHL had no such notion, so every one of these arguments went to the C API as` |
|         - | 1290 | ` * a NUL-terminated string and was silently TRUNCATED at the NUL. That is not a` |
|         - | 1291 | ` * missing diagnostic: the truncated path is a DIFFERENT path, and the builtin` |
|         - | 1292 | `` * then operated on it. `unlink("$dir/x\0.png")` deleted `$dir/x`,`` |
|         - | 1293 | `` * `file_put_contents("$dir/x\0.txt",$d)` wrote it, `touch`/`chmod`/`copy`/`` |
|         - | 1294 | ``  * `rename`/`symlink`/`mkdir` all acted on the prefix, `glob` and `realpath` `` |
|         - | 1295 | `` * answered for it, and `shell_exec("cmd\0; rm -rf /")` ran the prefix as a`` |
|         - | 1296 | ` * command. It is the classic poison-NUL-byte shape php closed engine-wide: a` |
|         - | 1297 | ` * script that concatenates request input into a filename gets a truncation` |
|         - | 1298 | ` * where php gets a refusal, and the extension check the suffix was there to` |
|         - | 1299 | ` * perform never runs.` |
|         - | 1300 | ` *` |
|         - | 1301 | ` * The mask is positional (bit N => parameter N is a path), which is how php` |
|         - | 1302 | ` * carries it too. Only functions PHL actually registers are listed; each row's` |
|         - | 1303 | ` * positions were verified against php 8.5 argument by argument (the answer is` |
|         - | 1304 | `` * NOT derivable from the parameter name — preg_match's `$pattern` is an`` |
|         - | 1305 | ``  * ordinary string, glob's is a path — nor from the type, which is `string` `` |
|         - | 1306 | ` * for both).` |
|         - | 1307 | ` *` |
|         - | 1308 | ` * What is deliberately NOT here: the stat family (file_exists, is_dir, stat,` |
|         - | 1309 | ` * filesize, fileperms, …), which php parses with Z_PARAM_STR and answers` |
|         - | 1310 | `` * `false` for in silence, and the pure PATH-STRING functions (basename,`` |
|         - | 1311 | ` * dirname, pathinfo), which php lets the NUL through untouched because they` |
|         - | 1312 | ` * never touch the filesystem. Both are php-exact here already.` |
|         - | 1313 | ` */` |
|   2551629 | 1314 | `static sxu32 VmBuiltinPathMask(SyString *pName)` |
|         5 | 1315 | `{` |
|         - | 1316 | `	static const struct {` |
|         - | 1317 | `		const char *zName;` |
|         - | 1318 | `		sxu32 nByte;` |
|         - | 1319 | `		sxu32 mask;` |
|         - | 1320 | `	} aPath[] = {` |
|         - | 1321 | `		/* Open / read / write */` |
|         - | 1322 | `		{ "fopen",             5, 1u<<0 },` |
|         - | 1323 | `		{ "file_get_contents", 17, 1u<<0 },` |
|         - | 1324 | `		{ "file_put_contents", 17, 1u<<0 },` |
|         - | 1325 | `		{ "file",              4, 1u<<0 },` |
|         - | 1326 | `		{ "readfile",          8, 1u<<0 },` |
|         - | 1327 | `		{ "parse_ini_file",   14, 1u<<0 },` |
|         - | 1328 | `		{ "md5_file",          8, 1u<<0 },` |
|         - | 1329 | `		{ "sha1_file",         9, 1u<<0 },` |
|         - | 1330 | `		/* Metadata / mutation */` |
|         - | 1331 | `		{ "unlink",            6, 1u<<0 },` |
|         - | 1332 | `		{ "touch",             5, 1u<<0 },` |
|         - | 1333 | `		{ "chmod",             5, 1u<<0 },` |
|         - | 1334 | `		{ "chgrp",             5, 1u<<0 },` |
|         - | 1335 | `		{ "chown",             5, 1u<<0 },` |
|         - | 1336 | `		{ "rename",            6, (1u<<0)\|(1u<<1) },` |
|         - | 1337 | `		{ "copy",              4, (1u<<0)\|(1u<<1) },` |
|         - | 1338 | `		{ "link",              4, (1u<<0)\|(1u<<1) },` |
|         - | 1339 | `		{ "symlink",           7, (1u<<0)\|(1u<<1) },` |
|         - | 1340 | `		{ "readlink",          8, 1u<<0 },` |
|         - | 1341 | `		{ "realpath",          8, 1u<<0 },` |
|         - | 1342 | `		/* Directories */` |
|         - | 1343 | `		{ "mkdir",             5, 1u<<0 },` |
|         - | 1344 | `		{ "rmdir",             5, 1u<<0 },` |
|         - | 1345 | `		{ "opendir",           7, 1u<<0 },` |
|         - | 1346 | `		{ "dir",               3, 1u<<0 },` |
|         - | 1347 | `		{ "scandir",           7, 1u<<0 },` |
|         - | 1348 | `		{ "chdir",             5, 1u<<0 },` |
|         - | 1349 | `		{ "chroot",            6, 1u<<0 },` |
|         - | 1350 | `		{ "glob",              4, 1u<<0 },` |
|         - | 1351 | `		{ "tempnam",           7, (1u<<0)\|(1u<<1) },` |
|         - | 1352 | `		{ "disk_free_space",  15, 1u<<0 },` |
|         - | 1353 | `		{ "disk_total_space", 16, 1u<<0 },` |
|         - | 1354 | `		{ "diskfreespace",    13, 1u<<0 },` |
|         - | 1355 | `		/* Path-shaped settings and the pattern matcher */` |
|         - | 1356 | `		{ "fnmatch",           7, (1u<<0)\|(1u<<1) },` |
|         - | 1357 | `		{ "set_include_path", 16, 1u<<0 },` |
|         - | 1358 | `		{ "session_save_path", 17, 1u<<0 },` |
|         - | 1359 | `		{ "error_log",         9, 1u<<2 },` |
|         - | 1360 | `		/* Commands handed to the shell — and the two escapers, which php screens` |
|         - | 1361 | `		 * the same way even though neither of them runs anything: a NUL in what a` |
|         - | 1362 | `		 * script is about to hand a shell is refused where it is WRITTEN. */` |
|         - | 1363 | `		{ "shell_exec",       10, 1u<<0 },` |
|         - | 1364 | `		{ "popen",             5, 1u<<0 },` |
|         - | 1365 | `		{ "escapeshellarg",   14, 1u<<0 },` |
|         - | 1366 | `		{ "escapeshellcmd",   14, 1u<<0 },` |
|         - | 1367 | `		{ "exec",              4, 1u<<0 },` |
|         - | 1368 | `		{ "system",            6, 1u<<0 },` |
|         - | 1369 | `		{ "passthru",          8, 1u<<0 },` |
|         - | 1370 | `		/* The SPL path constructors, which php screens identically and reports` |
|         - | 1371 | ``		 * under their QUALIFIED name (`SplFileInfo::__construct(): Argument #1`` |
|         - | 1372 | ``		 * ($filename) …`). They are native methods, so their signature reaches this`` |
|         - | 1373 | `		 * screen the same way a builtin's does. */` |
|         - | 1374 | `		{ "SplFileInfo::__construct",                24, 1u<<0 },` |
|         - | 1375 | `		{ "DirectoryIterator::__construct",          30, 1u<<0 },` |
|         - | 1376 | `		{ "FilesystemIterator::__construct",         31, 1u<<0 },` |
|         - | 1377 | `		{ "RecursiveDirectoryIterator::__construct", 39, 1u<<0 },` |
|         - | 1378 | `	};` |
|         - | 1379 | `	sxu32 i;` |
|   2551634 | 1380 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|       ! 0 | 1381 | `		return 0;` |
|         - | 1382 | `	}` |
| 117018226 | 1383 | `	for( i = 0 ; i < SX_ARRAYSIZE(aPath) ; ++i ){` |
| 114549658 | 1384 | `		if( pName->nByte == aPath[i].nByte` |
|  59184684 | 1385 | `		 && SyStrnicmp(pName->zString,aPath[i].zName,pName->nByte) == 0 ){` |
|     83071 | 1386 | `			return aPath[i].mask;` |
|         - | 1387 | `		}` |
|  57270216 | 1388 | `	}` |
|   2468568 | 1389 | `	return 0;` |
|   1276622 | 1390 | `}` |
|         - | 1391 | `/*` |
|         - | 1392 | ` * Does this argument carry a NUL byte? Only a STRING can: every other scalar` |
|         - | 1393 | ` * renders through the number/bool formatters, which emit none. An OBJECT is` |
|         - | 1394 | ` * coerced by the caller before asking (php's ZPP order), so by the time this` |
|         - | 1395 | ` * runs a Stringable is already the string it produced.` |
|         - | 1396 | ` */` |
|     83080 | 1397 | `static int VmArgHasNulByte(ph7_value *pArg)` |
|         5 | 1398 | `{` |
|         - | 1399 | `	const char *zStr;` |
|         - | 1400 | `	sxu32 n, nLen;` |
|     83085 | 1401 | `	if( (pArg->iFlags & MEMOBJ_STRING) == 0 ){` |
|         3 | 1402 | `		return 0;` |
|         - | 1403 | `	}` |
|     83083 | 1404 | `	zStr = (const char *)SyBlobData(&pArg->sBlob);` |
|     83083 | 1405 | `	nLen = SyBlobLength(&pArg->sBlob);` |
|   5404535 | 1406 | `	for( n = 0 ; n < nLen ; ++n ){` |
|   5321543 | 1407 | `		if( zStr[n] == 0 ){` |
|        87 | 1408 | `			return 1;` |
|         - | 1409 | `		}` |
|   2685044 | 1410 | `	}` |
|     82997 | 1411 | `	return 0;` |
|     41545 | 1412 | `}` |
|         - | 1413 | `/*` |
|         - | 1414 | ` * Does php's strict_types rule refuse this argument for the declared type?` |
|         - | 1415 | ` *` |
|         - | 1416 | `` * A `declare(strict_types=1)` file gets NO scalar coercion at an internal call`` |
|         - | 1417 | ` * either — php applies the same rule to a builtin, a native method and a userland` |
|         - | 1418 | `` * function, and the single exception is the int -> float widening. So `trim(5)`,`` |
|         - | 1419 | `` * `sqrt("4")`, `str_repeat("a", 2.0)` and `in_array($n, $a, 1)` are all TypeErrors`` |
|         - | 1420 | ` * there, where the weak-mode screen below (which is the only one PHL had) coerces` |
|         - | 1421 | ` * and computes.` |
|         - | 1422 | ` *` |
|         - | 1423 | ` * Only the arms a scalar could otherwise satisfy are decided here; an array, a` |
|         - | 1424 | ` * resource, a null and a class-typed mismatch are the weak screen's, and its` |
|         - | 1425 | ` * verdicts stand in both modes.` |
|         - | 1426 | ` */` |
|       194 | 1427 | `static int VmStrictArgRefused(ph7_value *pArg,const char *zType,int nType)` |
|         3 | 1428 | `{` |
|         - | 1429 | `	/* Tested in ph7_type_name()'s own order, so the branch taken and the name the` |
|         - | 1430 | `	 * refusal reports can never disagree. FLOAT comes before INT on purpose:` |
|         - | 1431 | `	 * ph7_value_is_int() is deliberately lenient — an integer-valued real caches an` |
|         - | 1432 | ``	 * int and answers TRUE — and `str_repeat("a", 2.0)` is php's TypeError, not an`` |
|         - | 1433 | `	 * accepted int. */` |
|       197 | 1434 | `	if( ph7_value_is_bool(pArg) ){` |
|        12 | 1435 | `		return !VmSigTypeHas(zType,nType,"bool")` |
|         7 | 1436 | `		    && !VmSigTypeHas(zType,nType,"true")` |
|        11 | 1437 | `		    && !VmSigTypeHas(zType,nType,"false");` |
|         - | 1438 | `	}` |
|       189 | 1439 | `	if( ph7_value_is_float(pArg) ){` |
|         7 | 1440 | `		return !VmSigTypeHas(zType,nType,"float");` |
|         - | 1441 | `	}` |
|       183 | 1442 | `	if( ph7_value_is_int(pArg) ){` |
|         - | 1443 | `		/* int -> float is the one widening strict mode keeps. */` |
|        28 | 1444 | `		return !VmSigTypeHas(zType,nType,"int") && !VmSigTypeHas(zType,nType,"float");` |
|         - | 1445 | `	}` |
|       157 | 1446 | `	if( ph7_value_is_string(pArg) ){` |
|         - | 1447 | ``		/* `callable` is not a coercion: a function-name string satisfies it in both`` |
|         - | 1448 | `		 * modes (array_map('strtoupper', …) under strict is php-legal). */` |
|       104 | 1449 | `		return !VmSigTypeHas(zType,nType,"string") && !VmSigTypeHas(zType,nType,"callable");` |
|         - | 1450 | `	}` |
|        54 | 1451 | `	if( ph7_value_is_object(pArg) ){` |
|         - | 1452 | ``		/* An object reaches a `string` parameter only through __toString(), which is`` |
|         - | 1453 | `		 * a coercion strict mode does not perform. Every other arm is the weak` |
|         - | 1454 | `		 * screen's decision. */` |
|        24 | 1455 | `		return VmSigTypeHas(zType,nType,"string")` |
|        12 | 1456 | `		    && !VmSigTypeHas(zType,nType,"object")` |
|         2 | 1457 | `		    && !VmSigTypeHas(zType,nType,"iterable")` |
|         2 | 1458 | `		    && !VmSigTypeHas(zType,nType,"callable")` |
|        23 | 1459 | `		    && !VmSigTypeHasClass(zType,nType);` |
|         - | 1460 | `	}` |
|        32 | 1461 | `	return 0;` |
|       100 | 1462 | `}` |
|         - | 1463 | `/*` |
|         - | 1464 | ` * PHP-8 ZPP type enforcement for host functions, driven by the aBuiltinSig[]` |
|         - | 1465 | ` * declaration (band A #7). Screens only the arguments that php can NEVER coerce` |
|         - | 1466 | ` * into a declared scalar parameter — arrays, resources, and objects without a` |
|         - | 1467 | ` * __toString() — and throws the catchable TypeError php throws, before the C` |
|         - | 1468 | ` * routine runs. Without this an array argument reached the builtin and was` |
|         - | 1469 | ` * stringified to the literal "Array" (strrev(['a']) returned "yarrA").` |
|         - | 1470 | ` *` |
|         - | 1471 | ` * Deliberately narrow: scalar-to-scalar coercion (and its deprecations) stays` |
|         - | 1472 | ` * with the per-builtin ZPP helpers. Those emit E_DEPRECATED, which does NOT` |
|         - | 1473 | ` * abort the call, so a central copy would double-fire — the trap that sank the` |
|         - | 1474 | ` * first central-ZPP attempt. A TypeError aborts, so there is nothing to double.` |
|         - | 1475 | ` */` |
|   2741735 | 1476 | `PH7_PRIVATE sxi32 VmEnforceBuiltinArgTypes(` |
|         - | 1477 | `	ph7_context *pCtx,    /* Call context (for the throw) */` |
|         - | 1478 | `	ph7_user_func *pFunc, /* Callee */` |
|         - | 1479 | `	int nGiven,           /* Argument count */` |
|         - | 1480 | `	ph7_value **apArg     /* Arguments */` |
|         - | 1481 | `	)` |
|         5 | 1482 | `{` |
|         - | 1483 | `	/*` |
|         - | 1484 | `	 * Builtins whose own argument check is php-exact and VALUE-based rather than` |
|         - | 1485 | `	 * type-based must not be pre-empted here, or their message is lost. Same rule` |
|         - | 1486 | `	 * the aBuiltinArity[] table follows: a builtin that already says what php says` |
|         - | 1487 | `	 * stays off the shared screen. get_class_vars() takes any stringifiable value` |
|         - | 1488 | `	 * and reports "must be a valid class name, Array given"; get_class_methods() is` |
|         - | 1489 | `	 * the same shape with php's other wording ("must be an object or a valid class` |
|         - | 1490 | ``	 * name, int given") — the declared `object\|string` never appears in either.`` |
|         - | 1491 | `	 *` |
|         - | 1492 | `	 * strtr() is here for a structural reason: php declares it as two OVERLOADS` |
|         - | 1493 | `	 * dispatched on arity — strtr(string, array) and strtr(string, string, string)` |
|         - | 1494 | `` 	 * — so the expected type of $from is `array` with two arguments and `string` `` |
|         - | 1495 | ``	 * with three. One signature cannot say that (the stub's `array\|string` is the`` |
|         - | 1496 | `	 * union of the two, which is the wording php never uses), so the builtin does` |
|         - | 1497 | `	 * its own dispatch through PH7_ArgSatisfiesString(), the same rule as here.` |
|         - | 1498 | `	 *` |
|         - | 1499 | ``	 * implode() is the same structure: `array\|string $separator` is what the two`` |
|         - | 1500 | `	 * ARITIES accept between them, never what one call can use. Once an $array` |
|         - | 1501 | `	 * argument is present php has resolved the overload and reports` |
|         - | 1502 | ``	 * `must be of type string`, and with the array in position #1 it reports`` |
|         - | 1503 | ``	 * `must be of type string, array given` against #1 rather than a #2 error.`` |
|         - | 1504 | `	 * PH7_builtin_implode words all of that itself.` |
|         - | 1505 | `	 *` |
|         - | 1506 | `	 * Its alias join() is here for the same reason and then some: php 8.5 does not` |
|         - | 1507 | `	 * word the two the same, so the builtin reproduces BOTH orders keyed on the` |
|         - | 1508 | `	 * invoked name (see PH7_builtin_implode's header for the value-for-value` |
|         - | 1509 | `	 * table against 8.5.8). php's own asymmetry between a target and its alias,` |
|         - | 1510 | `	 * reproduced rather than smoothed over — parity is binding (§10).` |
|         - | 1511 | `	 *` |
|         - | 1512 | `	 * number_format() is here because php's DECLARED type and its REFUSAL text` |
|         - | 1513 | ``	 * disagree: the stub says `float $num` (which is what Reflection prints) while`` |
|         - | 1514 | `	 * the ZPP macro behind it is Z_PARAM_NUMBER, whose TypeError says` |
|         - | 1515 | ``	 * `must be of type int\|float`. One row cannot say both, so the row carries the`` |
|         - | 1516 | `	 * declared type for Reflection and the builtin words every refusal itself.` |
|         - | 1517 | `	 *` |
|         - | 1518 | `	 * RecursiveIteratorIterator::__construct() is the first NATIVE METHOD here, and` |
|         - | 1519 | `	 * it is the same disagreement one level up: php's stub declares` |
|         - | 1520 | ``	 * `Traversable $iterator` (what Reflection prints) while its ZPP is a bare "o",`` |
|         - | 1521 | ``	 * whose TypeError says `must be of type object`. A native method's diagnostic`` |
|         - | 1522 | `	 * name is the QUALIFIED one, so the row below matches it and nothing else.` |
|         - | 1523 | `	 *` |
|         - | 1524 | `	 * The array_udiff/array_uintersect u-variant family is here for its ORDER:` |
|         - | 1525 | `	 * php validates the trailing comparison callback(s) before ANY of the` |
|         - | 1526 | `	 * arrays — array_diff_ukey(123,[1],456) names Argument #3, not #1 — and a` |
|         - | 1527 | `	 * positional screen cannot say that. HashmapUVariant performs the whole` |
|         - | 1528 | `	 * php sequence itself (callbacks, then Argument #1, then the middles).` |
|         - | 1529 | `	 */` |
|         - | 1530 | `	static const char *azSelfChecked[] = { "get_class_vars", "get_class_methods", "strtr",` |
|         - | 1531 | `		"implode", "join", "number_format", "RecursiveIteratorIterator::__construct",` |
|         - | 1532 | `		"array_udiff", "array_udiff_assoc", "array_udiff_uassoc",` |
|         - | 1533 | `		"array_uintersect", "array_uintersect_assoc", "array_uintersect_uassoc",` |
|         - | 1534 | `		"array_diff_uassoc", "array_diff_ukey",` |
|         - | 1535 | `		"array_intersect_uassoc", "array_intersect_ukey" };` |
|   2741740 | 1536 | `	const char *zSig = pFunc->zSig;` |
|         - | 1537 | `	const char *zCur, *zEnd;` |
|   2741740 | 1538 | `	int iArg = 0;` |
|         - | 1539 | `	/* The CALL site's file mode, stamped by the compiler onto this call's argument` |
|         - | 1540 | `	 * map (weak when there is no map — a call that carries no compile-time metadata` |
|         - | 1541 | `	 * was written in a weak-mode file, since a strict one always attaches one). */` |
|   2741740 | 1542 | `	int bStrict = (pCtx->pArgMap && pCtx->pArgMap->bStrict) ? 1 : 0;` |
|         - | 1543 | `	sxu32 nPathMask;` |
|   2741740 | 1544 | `	if( zSig == 0 ){` |
|    190111 | 1545 | `		return SXRET_OK;` |
|         - | 1546 | `	}` |
|   2551634 | 1547 | `	nPathMask = VmBuiltinPathMask(&pFunc->sName);` |
|  45369167 | 1548 | `	for( iArg = 0 ; iArg < (int)SX_ARRAYSIZE(azSelfChecked) ; ++iArg ){` |
|  64300158 | 1549 | `		if( SyStrncmp(pFunc->sName.zString,azSelfChecked[iArg],` |
|  64300158 | 1550 | `			(sxu32)SyStrlen(azSelfChecked[iArg])) == 0` |
|  21462579 | 1551 | `		 && pFunc->sName.nByte == SyStrlen(azSelfChecked[iArg]) ){` |
|     40149 | 1552 | `			return SXRET_OK;` |
|         - | 1553 | `		}` |
|  21422414 | 1554 | `	}` |
|   2511490 | 1555 | `	iArg = 0;` |
|   2511490 | 1556 | `	zCur = zSig;` |
|   2511490 | 1557 | `	zEnd = &zSig[SyStrlen(zSig)];` |
|   5884566 | 1558 | `	while( zCur < zEnd && iArg < nGiven ){` |
|         - | 1559 | `		const char *zType, *zName, *zStop;` |
|         - | 1560 | `		int nType, nName, bByRef;` |
|         - | 1561 | `		ph7_value *pArg;` |
|         - | 1562 | `		char zGivenBuf[64];` |
|         - | 1563 | `		/* Parameter = "<type> $<name>[ = <default>]"; the type is whatever` |
|         - | 1564 | `		 * precedes the '$', and an empty type means "untyped" (no screen). */` |
|   4292307 | 1565 | `		while( zCur < zEnd && zCur[0] == ' ' ){` |
|    914489 | 1566 | `			zCur++;` |
|         5 | 1567 | `		}` |
|   3377823 | 1568 | `		zStop = zCur;` |
|  59187609 | 1569 | `		while( zStop < zEnd && zStop[0] != ',' ){` |
|  55809791 | 1570 | `			if( zStop[0] == '\'' \|\| zStop[0] == '"' ){` |
|   1455917 | 1571 | `				zStop = VmSigSkipQuoted(zStop);` |
|   1455917 | 1572 | `				if( zStop >= zEnd ){` |
|       ! 0 | 1573 | `					break;` |
|         - | 1574 | `				}` |
|    727956 | 1575 | `			}` |
|  55809791 | 1576 | `			zStop++;` |
|         5 | 1577 | `		}` |
|   3377823 | 1578 | `		zName = zCur;` |
|  25639999 | 1579 | `		while( zName < zStop && zName[0] != '$' ){` |
|  22262181 | 1580 | `			zName++;` |
|         5 | 1581 | `		}` |
|   3377823 | 1582 | `		if( zName >= zStop ){` |
|       ! 0 | 1583 | `			break; /* malformed / no parameter name — stop screening */` |
|         - | 1584 | `		}` |
|   3377823 | 1585 | `		if( zName >= zCur + 3 && SyMemcmp(zName - 3,"...",3) == 0 ){` |
|      3785 | 1586 | `			break; /* variadic tail: stop (its type applies to the rest) */` |
|         - | 1587 | `		}` |
|   3374043 | 1588 | `		zType = zCur;` |
|   3374043 | 1589 | `		nType = (int)(zName - zCur);` |
|         - | 1590 | `		/* Trim the trailing spaces and the by-ref marker of "array &$array" */` |
|   3374043 | 1591 | `		bByRef = 0;` |
|  10022927 | 1592 | `		while( nType > 0 && (zType[nType-1] == ' ' \|\| zType[nType-1] == '&') ){` |
|   3324113 | 1593 | `			if( zType[nType-1] == '&' ){` |
|      2721 | 1594 | `				bByRef = 1;` |
|      1358 | 1595 | `			}` |
|   3324113 | 1596 | `			nType--;` |
|         5 | 1597 | `		}` |
|   3374043 | 1598 | `		zName++; /* skip '$' */` |
|   3374043 | 1599 | `		nName = 0;` |
|  25399483 | 1600 | `		while( &zName[nName] < zStop && zName[nName] != ' ' && zName[nName] != '=' ){` |
|  22025445 | 1601 | `			nName++;` |
|         5 | 1602 | `		}` |
|   3374043 | 1603 | `		pArg = apArg[iArg];` |
|   3374038 | 1604 | `		if( bByRef && pArg->nIdx == SXU32_HIGH` |
|      1398 | 1605 | `		 && !(pCtx->pArgMap && pCtx->pArgMap->bArgShapes && !pCtx->pArgMap->bHasNamed) ){` |
|         - | 1606 | `			/* A by-reference parameter handed something with no slot to write back` |
|         - | 1607 | `			 * through -- a literal, a constant, the result of a call. php settles` |
|         - | 1608 | `			 * that at the CALL, before the callee's ZPP runs, so the type screen` |
|         - | 1609 | ``			 * must not speak first: `array_pop('foo')` is`` |
|         - | 1610 | `			 * "could not be passed by reference" and not "must be of type array,` |
|         - | 1611 | `			 * string given".` |
|         - | 1612 | `			 *` |
|         - | 1613 | `			 * Only when this call site carries no argument SHAPES, though. When it` |
|         - | 1614 | `			 * does, PH7_VmScreenByRefArgShapes has already had its say — it refused` |
|         - | 1615 | `			 * the literal and let the call RESULT through with php's notice — and` |
|         - | 1616 | `			 * standing aside here would swallow the type error php still reports for` |
|         - | 1617 | ``			 * the latter (`sort(new stdClass)` is "must be of type array, stdClass`` |
|         - | 1618 | `			 * given", not a silent false). */` |
|         7 | 1619 | `			zCur = (zStop < zEnd) ? zStop + 1 : zEnd;` |
|         7 | 1620 | `			iArg++;` |
|         7 | 1621 | `			continue;` |
|         - | 1622 | `		}` |
|   3374037 | 1623 | `		if( nType > 0 && !VmSigTypeHas(zType,nType,"mixed") ){` |
|   3208655 | 1624 | `			const char *zGiven = 0;` |
|   3208655 | 1625 | `			if( bStrict && VmStrictArgRefused(pArg,zType,nType) ){` |
|         - | 1626 | ``				/* php names the VALUE for a bool here too (`true given`). */`` |
|        37 | 1627 | `				zGiven = VmValueGivenName(pArg,zGivenBuf,sizeof(zGivenBuf));` |
|   3208637 | 1628 | `			}else if( (pArg->iFlags & MEMOBJ_HASHMAP) != 0 ){` |
|     46120 | 1629 | `				if( !VmSigTypeHas(zType,nType,"array")` |
|     23245 | 1630 | `				 && !VmSigTypeHas(zType,nType,"iterable")` |
|       375 | 1631 | `				 && !VmSigTypeHas(zType,nType,"callable") ){` |
|       185 | 1632 | `					zGiven = "array";` |
|        95 | 1633 | `				}` |
|   3185559 | 1634 | `			}else if( (pArg->iFlags & MEMOBJ_OBJ) != 0 ){` |
|      5976 | 1635 | `				if( !VmSigTypeHas(zType,nType,"object")` |
|      4016 | 1636 | `				 && !VmSigTypeHas(zType,nType,"iterable")` |
|      2056 | 1637 | `				 && !VmSigTypeHas(zType,nType,"callable")` |
|      1733 | 1638 | `				 && !VmSigTypeHasClass(zType,nType) ){` |
|         - | 1639 | `					/* An object with __toString() still satisfies a string` |
|         - | 1640 | `					 * parameter in weak mode — php coerces it. */` |
|       212 | 1641 | `					int bStringable = VmSigTypeHas(zType,nType,"string")` |
|       150 | 1642 | `						&& PH7_ArgSatisfiesString(pArg);` |
|       155 | 1643 | `					if( !bStringable ){` |
|        82 | 1644 | `						zGiven = VmArgTypeName(pArg);` |
|        39 | 1645 | `					}` |
|      5906 | 1646 | `				}else if( VmSigTypeHasClass(zType,nType)` |
|      3463 | 1647 | `				       && !VmSigTypeHas(zType,nType,"object")` |
|      1100 | 1648 | `				       && !VmSigTypeHas(zType,nType,"iterable")` |
|      1100 | 1649 | `				       && !VmSigTypeHas(zType,nType,"callable")` |
|      1105 | 1650 | `				       && !VmSigTypeHas(zType,nType,"string") ){` |
|         - | 1651 | `					/* A class-typed parameter given an object of the WRONG class.` |
|         - | 1652 | `					 * Naming a class used to be enough to let ANY object through, so` |
|         - | 1653 | `` 					 * `date_modify($immutable)` and `timezone_name_get($date)` `` |
|         - | 1654 | `					 * answered silently where php raises. Only decided when every` |
|         - | 1655 | `					 * class arm resolves to a declared class: an arm PHL does not` |
|         - | 1656 | `					 * declare cannot be judged, so the parameter stays unscreened. */` |
|      1565 | 1657 | `					if( !VmSigObjSatisfiesClass(pCtx->pVm,zType,nType,` |
|      1040 | 1658 | `						(ph7_class_instance *)pArg->x.pOther) ){` |
|        22 | 1659 | `						zGiven = VmArgTypeName(pArg);` |
|        10 | 1660 | `					}` |
|       525 | 1661 | `				}` |
|   3159511 | 1662 | `			}else if( (pArg->iFlags & MEMOBJ_NULL) != 0 ){` |
|         - | 1663 | `				/* php only DEPRECATES null for a non-nullable parameter; PHL rejects` |
|         - | 1664 | `				 * it (scope policy). A leading '?' or an explicit "null" arm in a` |
|         - | 1665 | `				 * union declares the parameter nullable. A "callable" parameter is` |
|         - | 1666 | `				 * left to the builtin's own callback check, which words the failure` |
|         - | 1667 | `				 * php's way ("must be a valid callback, no array or string given") —` |
|         - | 1668 | `				 * the same reason get_class_vars() sits on azSelfChecked[]. */` |
|      5614 | 1669 | `				if( zType[0] != '?'` |
|      2850 | 1670 | `				 && !VmSigTypeHas(zType,nType,"null")` |
|        85 | 1671 | `				 && !VmSigTypeHas(zType,nType,"callable") ){` |
|        73 | 1672 | `					zGiven = "null";` |
|        34 | 1673 | `				}` |
|   3153716 | 1674 | `			}else if( (pArg->iFlags & (MEMOBJ_STRING\|MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL)) != 0` |
|   3150904 | 1675 | `			       && (VmSigTypeHasClass(zType,nType)` |
|   3150712 | 1676 | `			        \|\| VmSigTypeHas(zType,nType,"object")) ){` |
|         - | 1677 | `				/* A SCALAR against a parameter that can only hold an INSTANCE —` |
|         - | 1678 | ``				 * a named class, or the bare `object` keyword. Every other scalar`` |
|         - | 1679 | `				 * pairing is left to weak-mode coercion, which is why nothing` |
|         - | 1680 | `				 * screened scalars here at all — but no coercion produces an` |
|         - | 1681 | `				 * instance, so php rejects this one. Found converting DateTime:` |
|         - | 1682 | ``				 * `$d->diff('x')` and `new DateTime('now','UTC')` ran on with a`` |
|         - | 1683 | ``				 * string where php raises. The `object` half was still blind when`` |
|         - | 1684 | `				 * WeakReference::create() declared the first such parameter, which` |
|         - | 1685 | `				 * also retires the "graceful degradation" NULL that spl_object_id(),` |
|         - | 1686 | `				 * spl_object_hash() and get_object_vars() used to answer. An arm a` |
|         - | 1687 | `				 * scalar CAN satisfy (a union with string/int/float/bool, or` |
|         - | 1688 | `				 * callable, which a string is) keeps the parameter unscreened —` |
|         - | 1689 | ``				 * and so does an `array` arm, whose refusal php words from the`` |
|         - | 1690 | `				 * builtin's own check rather than from the declared type` |
|         - | 1691 | ``				 * (array_walk's `array\|object &$array` says "must be of type`` |
|         - | 1692 | `				 * array", not "of type array\|object"). */` |
|      1676 | 1693 | `				if( !VmSigTypeHas(zType,nType,"string")` |
|       875 | 1694 | `				 && !VmSigTypeHas(zType,nType,"int")` |
|       112 | 1695 | `				 && !VmSigTypeHas(zType,nType,"float")` |
|        76 | 1696 | `				 && !VmSigTypeHas(zType,nType,"bool")` |
|        76 | 1697 | `				 && !VmSigTypeHas(zType,nType,"true")` |
|        76 | 1698 | `				 && !VmSigTypeHas(zType,nType,"false")` |
|        76 | 1699 | `				 && !VmSigTypeHas(zType,nType,"array")` |
|        62 | 1700 | `				 && !VmSigTypeHas(zType,nType,"callable") ){` |
|         - | 1701 | `					/* php's VALUE name, not the type's: a bool is reported as` |
|         - | 1702 | ``					 * `true`/`false` (the rule Generator::throw()'s own check`` |
|         - | 1703 | `					 * already followed, and which this screen now runs first). */` |
|        41 | 1704 | `					zGiven = VmValueGivenName(pArg,zGivenBuf,sizeof(zGivenBuf));` |
|        19 | 1705 | `				}` |
|   3150108 | 1706 | `			}else if( (pArg->iFlags & MEMOBJ_REAL) != 0` |
|   1575756 | 1707 | `			       && VmSigTypeIsIntOnly(zType,nType)` |
|       298 | 1708 | `			       && !VmDoubleFitsInt((double)pArg->rVal) ){` |
|         - | 1709 | ``				/* A FLOAT against a parameter typed exactly `int` (or `?int`), and`` |
|         - | 1710 | `				 * one no int can hold: a fraction, a magnitude past the signed` |
|         - | 1711 | `				 * 64-bit range, NaN or an infinity. php refuses every one of them` |
|         - | 1712 | `				 * (zend_parse_arg_long's ZEND_DOUBLE_FITS_LONG / is-integral pair,` |
|         - | 1713 | `				 * the fractional case with a deprecation PHL rejects outright by` |
|         - | 1714 | `				 * §10) and the refusal is this screen's own wording.` |
|         - | 1715 | `				 *` |
|         - | 1716 | `				 * PH7_IntArgResolve has always said exactly this, but only for the` |
|         - | 1717 | `` 				 * builtins that CALL it from their own body — so `dechex(1.5)` `` |
|         - | 1718 | ``				 * answered '1', `array_fill(1.5,1,0)` filled from 1, and`` |
|         - | 1719 | ``				 * `strpos("abc","c",1e19)` took the offset as PHP_INT_MIN and`` |
|         - | 1720 | `				 * reported a ValueError about a range it never had. Seventy-five` |
|         - | 1721 | ``				 * `int` parameters across the signature table were unscreened that`` |
|         - | 1722 | `				 * way, and a NATIVE METHOD has no body to call the helper from at` |
|         - | 1723 | `				 * all. Deciding it from the declared type covers both callee kinds` |
|         - | 1724 | `				 * from one place, and the per-builtin helper still stands for the` |
|         - | 1725 | ``				 * message rows this screen cannot reach (the `azSelfChecked` set). */`` |
|        60 | 1726 | `				zGiven = "float";` |
|   3149278 | 1727 | `			}else if( (pArg->iFlags & (MEMOBJ_STRING\|MEMOBJ_NULL)) == MEMOBJ_STRING` |
|   2783105 | 1728 | `			       && (VmSigTypeHas(zType,nType,"int")` |
|   2416323 | 1729 | `			        \|\| VmSigTypeHas(zType,nType,"float"))` |
|   1209153 | 1730 | `			       && !VmSigTypeHas(zType,nType,"string")` |
|   1209039 | 1731 | `			       && !VmSigTypeHas(zType,nType,"array")` |
|       264 | 1732 | `			       && !VmSigTypeHas(zType,nType,"object")` |
|       262 | 1733 | `			       && !VmSigTypeHas(zType,nType,"iterable")` |
|       262 | 1734 | `			       && !VmSigTypeHas(zType,nType,"callable")` |
|       262 | 1735 | `			       && !VmSigTypeHas(zType,nType,"bool")` |
|       267 | 1736 | `			       && !VmSigTypeHasClass(zType,nType) ){` |
|         - | 1737 | ``				/* A STRING against a NUMBER-only parameter — `int`, `float`, or the`` |
|         - | 1738 | ``				 * `int\|float` union, with no arm a string can satisfy. Weak mode`` |
|         - | 1739 | `				 * coerces a NUMERIC one and php refuses every other — "x", "2abc"` |
|         - | 1740 | ``				 * and "0x2" are all `must be of type int, string given` (rule 41: a`` |
|         - | 1741 | `				 * numeric PREFIX is not enough, which is what SyStrIsNumeric would` |
|         - | 1742 | `				 * have accepted). Every BUILTIN with an int parameter already got` |
|         - | 1743 | `				 * this from PH7_IntArgResolve, called from its own body; a native` |
|         - | 1744 | `` 				 * METHOD has no body to call it from, so `ArrayIterator::seek('x')` `` |
|         - | 1745 | ``				 * seeked to 0, `DateTime::setTimestamp('abc')` set 0 and`` |
|         - | 1746 | ``				 * `DOMNodeList::item('zz')` answered element 0 — wrong ANSWERS,`` |
|         - | 1747 | `				 * not missing errors. Screening the declared type here covers both` |
|         - | 1748 | `				 * callee kinds from one place.` |
|         - | 1749 | `				 *` |
|         - | 1750 | `				 * The FLOAT arm is the same hazard one type over, and it was the` |
|         - | 1751 | `				 * half nothing covered: PH7_IntArgResolve has no float twin, so a` |
|         - | 1752 | ``				 * `float $num` builtin that did not hand-roll its own check simply`` |
|         - | 1753 | `				 * converted the string to 0.0 and COMPUTED with it —` |
|         - | 1754 | ``				 * `cos("nope")` answered `float(1)`, `sqrt("nope")` `float(0)`,`` |
|         - | 1755 | ``				 * `log("nope")` `float(-INF)`. Numbers with nothing wrong-looking`` |
|         - | 1756 | `				 * about them, from input php refuses outright.` |
|         - | 1757 | `				 *` |
|         - | 1758 | `				 * The NULL rule stays where it is: PHL rejects null for a` |
|         - | 1759 | `				 * non-nullable parameter by policy (§10) where php deprecates. */` |
|       398 | 1760 | `				if( !PH7_MemObjStringIsNumeric(pArg) ){` |
|       157 | 1761 | `					zGiven = "string";` |
|       189 | 1762 | `				}else if( VmSigTypeIsIntOnly(zType,nType) && !VmNumStrFitsInt(pArg) ){` |
|         - | 1763 | `					/* A NUMERIC string an int cannot hold — "1.5", "1e19",` |
|         - | 1764 | `					 * "99999999999999999999". php refuses all three (the fractional` |
|         - | 1765 | `					 * one after a deprecation §10 turns into the refusal), and PHL` |
|         - | 1766 | ``					 * narrowed them silently: `dechex("1e19")` answered '1' and`` |
|         - | 1767 | ``					 * `str_repeat("a","99999999999999999999")` took PHP_INT_MAX as`` |
|         - | 1768 | `					 * the count. Same wording, same position as the float arm above,` |
|         - | 1769 | `					 * because php reaches both through one ZPP macro. */` |
|        19 | 1770 | `					zGiven = "string";` |
|         8 | 1771 | `				}` |
|   3149120 | 1772 | `			}else if( (pArg->iFlags & (MEMOBJ_STRING\|MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL)) != 0` |
|   3148984 | 1773 | `			       && VmSigTypeIsArrayOnly(zType,nType) ){` |
|         - | 1774 | ``				/* A SCALAR against a parameter typed exactly `array`. No coercion`` |
|         - | 1775 | `				 * produces one, so php refuses it -- but the screen exempted every` |
|         - | 1776 | ``				 * `array` arm, union or not, and a whole family had no check of its`` |
|         - | 1777 | `				 * own to fall back on: sort/rsort/ksort/krsort/shuffle and` |
|         - | 1778 | ``				 * usort/uasort/uksort each answered `false` for `sort($notAnArray)`,`` |
|         - | 1779 | `				 * which is also what they answer for a sort that genuinely failed.` |
|         - | 1780 | `				 * call_user_func_array('strlen', 'x') answered false too,` |
|         - | 1781 | `				 * iterator_apply RAN the callback, and getopt/hash/password_hash/` |
|         - | 1782 | `				 * password_needs_rehash/unserialize/fputcsv simply carried on with` |
|         - | 1783 | `				 * the string where an options ARRAY was declared.` |
|         - | 1784 | `				 *` |
|         - | 1785 | `				 * The builtins that DO check (array_keys, in_array, asort, ...) word` |
|         - | 1786 | `				 * it identically, so the screen only pre-empts them -- and corrects` |
|         - | 1787 | `				 * one detail on the way: their ph7_type_name() says "bool" where php` |
|         - | 1788 | ``				 * names the VALUE, `true` or `false`. */`` |
|       227 | 1789 | `				zGiven = VmValueGivenName(pArg,zGivenBuf,sizeof(zGivenBuf));` |
|   3148878 | 1790 | `			}else if( (pArg->iFlags & MEMOBJ_RES) != 0 ){` |
|         - | 1791 | `				/* A class-typed parameter also accepts a resource: several handles php 8` |
|         - | 1792 | `				 * models as objects are still resources here (xml_*'s XMLParser is the` |
|         - | 1793 | `				 * one the signatures already declare php-8-style, for reflection). The` |
|         - | 1794 | `				 * screen would otherwise reject the engine's own parser handle. Recorded` |
|         - | 1795 | `				 * as a divergence in NEWPLAN §7 — it goes away when those handles become` |
|         - | 1796 | `				 * real objects. */` |
|        10 | 1797 | `				if( !VmSigTypeHas(zType,nType,"resource")` |
|        12 | 1798 | `				 && !VmSigTypeHasClass(zType,nType) ){` |
|        12 | 1799 | `					zGiven = "resource";` |
|         5 | 1800 | `				}` |
|         5 | 1801 | `			}` |
|   3208655 | 1802 | `			if( zGiven ){` |
|         - | 1803 | ``				/* php's `object\|array` parameters come from ONE ZPP macro`` |
|         - | 1804 | `				 * (Z_PARAM_ARRAY_OR_OBJECT) and it names only "array" in the` |
|         - | 1805 | `				 * refusal — array_walk(null,…), current(null) and` |
|         - | 1806 | `				 * http_build_query(null) all say "must be of type array". The` |
|         - | 1807 | `				 * SCALAR branch above already encodes that rule by declining to` |
|         - | 1808 | `				 * screen at all; the null and resource branches do screen, so the` |
|         - | 1809 | `				 * reported type has to be corrected here instead. */` |
|       881 | 1810 | `				if( VmSigTypeHas(zType,nType,"array") && VmSigTypeHas(zType,nType,"object") ){` |
|        12 | 1811 | `					zType = "array";` |
|        12 | 1812 | `					nType = (int)sizeof("array")-1;` |
|         5 | 1813 | `				}` |
|      1362 | 1814 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 1815 | `					"%z(): Argument #%d ($%.*s) must be of type %.*s, %s given",` |
|       438 | 1816 | `					&pFunc->sName,iArg + 1,nName,zName,nType,zType,zGiven);` |
|         - | 1817 | `			}` |
|   1604735 | 1818 | `		}` |
|         - | 1819 | `		/* A PATH parameter, once its type is settled: php's Z_PARAM_PATH refuses a` |
|         - | 1820 | `		 * NUL byte outright rather than letting the C API truncate at it. Raised` |
|         - | 1821 | `		 * after the type verdict because that is php's order — the coercion runs` |
|         - | 1822 | `		 * first, and only a value that could BE a path is asked whether it is a` |
|         - | 1823 | `		 * legal one. */` |
|   3373161 | 1824 | `		if( iArg < 31 && (nPathMask & (1u<<iArg)) != 0 ){` |
|     83085 | 1825 | `			if( (pArg->iFlags & MEMOBJ_OBJ) != 0 && PH7_ArgSatisfiesString(pArg) ){` |
|         - | 1826 | `				/* A Stringable object: php coerces it and checks the RESULT, so` |
|         - | 1827 | ``				 * `unlink($o)` with a __toString() returning a NUL-bearing name is`` |
|         - | 1828 | `				 * the same ValueError. Converting IN PLACE is what keeps the` |
|         - | 1829 | `				 * accessor running exactly ONCE — the builtin then receives the` |
|         - | 1830 | `				 * string it would have produced itself. The argument a builtin sees` |
|         - | 1831 | `				 * is its own copy on every dispatch route (a direct call, a spread,` |
|         - | 1832 | `				 * both call_user_func forwards), so the caller's object is not` |
|         - | 1833 | `				 * retyped; strict mode never gets here, because a Stringable does` |
|         - | 1834 | ``				 * not satisfy a `string` parameter there and the screen above has`` |
|         - | 1835 | `				 * already refused it. */` |
|         3 | 1836 | `				sxi32 rcConv = PH7_MemObjToStringUV(pArg);` |
|         3 | 1837 | `				if( rcConv != SXRET_OK ){` |
|       ! 0 | 1838 | `					return rcConv; /* __toString() threw: php propagates it too */` |
|         - | 1839 | `				}` |
|         1 | 1840 | `			}` |
|     83085 | 1841 | `			if( VmArgHasNulByte(pArg) ){` |
|       130 | 1842 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 1843 | `					"%z(): Argument #%d ($%.*s) must not contain any null bytes",` |
|        43 | 1844 | `					&pFunc->sName,iArg + 1,nName,zName);` |
|         - | 1845 | `			}` |
|     41497 | 1846 | `		}` |
|   3373075 | 1847 | `		zCur = (zStop < zEnd) ? zStop + 1 : zEnd;` |
|   3373075 | 1848 | `		iArg++;` |
|         5 | 1849 | `	}` |
|   2510528 | 1850 | `	return SXRET_OK;` |
|   1371675 | 1851 | `}` |
|         - | 1852 | `/*` |
|         - | 1853 | ` * Builtins whose accepted arity is NOT a contiguous range, so the signature` |
|         - | 1854 | ` * cannot express it and the central too-many-arguments check must stay out of` |
|         - | 1855 | ` * the way: rand()/mt_rand() take 0 OR 2 arguments (never 1), and php words the` |
|         - | 1856 | ` * violation "expects exactly 2 arguments, 3 given" from their own check rather` |
|         - | 1857 | ` * than the ZPP "at most". They validate themselves; leaving bHasMaxArg at 0` |
|         - | 1858 | ` * keeps their message php-faithful.` |
|         - | 1859 | ` */` |
|   2319244 | 1860 | `static int VmBuiltinSelfValidatesArity(const char *zName)` |
|         5 | 1861 | `{` |
|         - | 1862 | `	static const char *const azSelf[] = { "rand", "mt_rand" };` |
|         - | 1863 | `	sxu32 i;` |
|   6945509 | 1864 | `	for( i = 0 ; i < SX_ARRAYSIZE(azSelf) ; i++ ){` |
|   4634417 | 1865 | `		sxu32 nSelf = SyStrlen(azSelf[i]);` |
|   4634417 | 1866 | `		if( SyStrlen(zName) == nSelf && SyStrncmp(zName,azSelf[i],nSelf) == 0 ){` |
|      8157 | 1867 | `			return 1;` |
|         - | 1868 | `		}` |
|   2313135 | 1869 | `	}` |
|   2311097 | 1870 | `	return 0;` |
|   1159627 | 1871 | `}` |
|         - | 1872 | `/*` |
|         - | 1873 | ` * One parameter of a declared signature, for the named-argument binder below.` |
|         - | 1874 | ` */` |
|         - | 1875 | `typedef struct VmSigParam VmSigParam;` |
|         - | 1876 | `struct VmSigParam` |
|         - | 1877 | `{` |
|         - | 1878 | `	const char *zName; int nName;   /* without the '$' */` |
|         - | 1879 | `	const char *zDef;  int nDef;    /* default TEXT, or 0 when the parameter is required */` |
|         - | 1880 | `	int bVariadic;` |
|         - | 1881 | `};` |
|         - | 1882 | `/*` |
|         - | 1883 | ` * Split a signature into its parameters: the NAME each one binds by and the default` |
|         - | 1884 | ` * TEXT to fall back on. The scan is VmDeriveArityFromSig's, kept apart because that one` |
|         - | 1885 | `` * only counts; a quoted default (`string $separator = ','`) hides a comma, which is why`` |
|         - | 1886 | ` * both go through VmSigSkipQuoted.` |
|         - | 1887 | ` */` |
|     54140 | 1888 | `static int VmSigParams(const char *zSig,VmSigParam *aOut,int nMax)` |
|         5 | 1889 | `{` |
|     54145 | 1890 | `	const char *zCur = zSig;` |
|     54145 | 1891 | `	const char *zStart = zSig;` |
|     54145 | 1892 | `	int n = 0;` |
|   2152729 | 1893 | `	for(;;){` |
|   4467099 | 1894 | `		if( zCur[0] == '\'' \|\| zCur[0] == '"' ){` |
|        19 | 1895 | `			zCur = VmSigSkipQuoted(zCur);` |
|        19 | 1896 | `			if( zCur[0] != '\0' ){` |
|        19 | 1897 | `				zCur++;` |
|         9 | 1898 | `			}` |
|        19 | 1899 | `			continue;` |
|         - | 1900 | `		}` |
|   4467081 | 1901 | `		if( zCur[0] == '\0' \|\| zCur[0] == ',' ){` |
|    215763 | 1902 | `			const char *z = zStart;` |
|    215763 | 1903 | `			const char *zEnd = zCur;` |
|    215763 | 1904 | `			if( n < nMax ){` |
|    215763 | 1905 | `				VmSigParam *p = &aOut[n];` |
|    215763 | 1906 | `				const char *zEq = 0;` |
|    215763 | 1907 | `				const char *zDollar = 0;` |
|    215763 | 1908 | `				p->zName = 0; p->nName = 0; p->zDef = 0; p->nDef = 0; p->bVariadic = 0;` |
|   4467159 | 1909 | `				for( ; z < zEnd ; z++ ){` |
|   4251401 | 1910 | `					if( z[0] == '$' && zDollar == 0 ){` |
|    215763 | 1911 | `						zDollar = z + 1;` |
|   4143522 | 1912 | `					}else if( z[0] == '=' && zEq == 0 ){` |
|     55099 | 1913 | `						zEq = z + 1;` |
|   4008096 | 1914 | `					}else if( z[0] == '.' && z + 2 < zEnd && z[1] == '.' && z[2] == '.' ){` |
|        93 | 1915 | `						p->bVariadic = 1;` |
|        44 | 1916 | `					}` |
|   2125703 | 1917 | `				}` |
|    215763 | 1918 | `				if( zDollar ){` |
|    215763 | 1919 | `					const char *zStop = zEq ? zEq - 1 : zEnd;` |
|    215763 | 1920 | `					const char *zN = zDollar;` |
|   1567923 | 1921 | `					while( zN < zStop && zN[0] != ' ' && zN[0] != '=' ){` |
|   1352165 | 1922 | `						zN++;` |
|         5 | 1923 | `					}` |
|    215763 | 1924 | `					p->zName = zDollar;` |
|    215763 | 1925 | `					p->nName = (int)(zN - zDollar);` |
|    107879 | 1926 | `				}` |
|    215763 | 1927 | `				if( zEq ){` |
|    110193 | 1928 | `					while( zEq < zEnd && zEq[0] == ' ' ){` |
|     55099 | 1929 | `						zEq++;` |
|         5 | 1930 | `					}` |
|     55099 | 1931 | `					p->zDef = zEq;` |
|     55099 | 1932 | `					p->nDef = (int)(zEnd - zEq);` |
|     55099 | 1933 | `					while( p->nDef > 0 && p->zDef[p->nDef-1] == ' ' ){` |
|       ! 0 | 1934 | `						p->nDef--;` |
|       ! 0 | 1935 | `					}` |
|     27547 | 1936 | `				}` |
|    215763 | 1937 | `				if( p->nName > 0 ){` |
|    215763 | 1938 | `					n++;` |
|    107879 | 1939 | `				}` |
|    107879 | 1940 | `			}` |
|    215763 | 1941 | `			if( zCur[0] == '\0' ){` |
|     54145 | 1942 | `				break;` |
|         - | 1943 | `			}` |
|    161623 | 1944 | `			zCur++;` |
|    161623 | 1945 | `			zStart = zCur;` |
|    161623 | 1946 | `			continue;` |
|         - | 1947 | `		}` |
|   4251323 | 1948 | `		zCur++;` |
|         5 | 1949 | `	}` |
|     54145 | 1950 | `	return n;` |
|         5 | 1951 | `}` |
|         - | 1952 | `/*` |
|         - | 1953 | ` * Materialize a signature default's TEXT into pOut. php's own stub values, which is a` |
|         - | 1954 | `` * small set: null, true/false, an integer or float, a quoted string, and `[]`. A default`` |
|         - | 1955 | `` * the table could not state (`= ?`, ~50 rows — §7.4) answers 0, and the caller then reports`` |
|         - | 1956 | ` * the parameter as not passed rather than inventing a value.` |
|         - | 1957 | ` */` |
|         6 | 1958 | `static int VmSigDefaultValue(ph7_vm *pVm,const VmSigParam *pParam,ph7_value *pOut)` |
|         1 | 1959 | `{` |
|         7 | 1960 | `	const char *z = pParam->zDef;` |
|         7 | 1961 | `	int n = pParam->nDef;` |
|         7 | 1962 | `	if( z == 0 \|\| n < 1 \|\| (n == 1 && z[0] == '?') ){` |
|         3 | 1963 | `		return 0;` |
|         - | 1964 | `	}` |
|         5 | 1965 | `	if( n == 4 && (SyStrnicmp(z,"null",4) == 0) ){` |
|       ! 0 | 1966 | `		PH7_MemObjRelease(pOut);` |
|       ! 0 | 1967 | `		return 1; /* a released value IS null */` |
|         - | 1968 | `	}` |
|         5 | 1969 | `	if( n == 4 && SyStrnicmp(z,"true",4) == 0 ){` |
|       ! 0 | 1970 | `		PH7_MemObjInitFromBool(pVm,pOut,1);` |
|       ! 0 | 1971 | `		return 1;` |
|         - | 1972 | `	}` |
|         5 | 1973 | `	if( n == 5 && SyStrnicmp(z,"false",5) == 0 ){` |
|       ! 0 | 1974 | `		PH7_MemObjInitFromBool(pVm,pOut,0);` |
|       ! 0 | 1975 | `		return 1;` |
|         - | 1976 | `	}` |
|         5 | 1977 | `	if( n == 2 && z[0] == '[' && z[1] == ']' ){` |
|         3 | 1978 | `		ph7_hashmap *pMap = PH7_NewHashmap(pVm,0,0);` |
|         3 | 1979 | `		if( pMap == 0 ){` |
|       ! 0 | 1980 | `			return 0;` |
|         - | 1981 | `		}` |
|         3 | 1982 | `		PH7_MemObjRelease(pOut);` |
|         3 | 1983 | `		pOut->x.pOther = pMap;` |
|         3 | 1984 | `		MemObjSetType(pOut,MEMOBJ_HASHMAP);` |
|         3 | 1985 | `		return 1;` |
|         - | 1986 | `	}` |
|         3 | 1987 | `	if( z[0] == '\'' \|\| z[0] == '"' ){` |
|         - | 1988 | `		SyString sStr;` |
|         3 | 1989 | `		SyStringInitFromBuf(&sStr,z + 1,n >= 2 ? n - 2 : 0);` |
|         3 | 1990 | `		PH7_MemObjInitFromString(pVm,pOut,&sStr);` |
|         3 | 1991 | `		return 1;` |
|         - | 1992 | `	}` |
|       ! 0 | 1993 | `	if( z[0] == '-' \|\| z[0] == '+' \|\| (z[0] >= '0' && z[0] <= '9') ){` |
|         - | 1994 | `		SyString sNum;` |
|       ! 0 | 1995 | `		SyStringInitFromBuf(&sNum,z,(sxu32)n);` |
|       ! 0 | 1996 | `		if( PH7_MemObjInitFromString(pVm,pOut,&sNum) != SXRET_OK ){` |
|       ! 0 | 1997 | `			return 0;` |
|         - | 1998 | `		}` |
|       ! 0 | 1999 | `		PH7_MemObjToNumeric(pOut);` |
|       ! 0 | 2000 | `		return 1;` |
|         - | 2001 | `	}` |
|       ! 0 | 2002 | `	return 0; /* a constant expression (M_PI, PHP_ROUND_HALF_UP, …): not evaluated here */` |
|         4 | 2003 | `}` |
|         - | 2004 | `/*` |
|         - | 2005 | ` * Bind a call's NAMED arguments to the callee's declared parameter POSITIONS.` |
|         - | 2006 | ` *` |
|         - | 2007 | ` * A compiled function does this from its parameter records (VmResolveNamedArgs); a host` |
|         - | 2008 | ` * function and a native method have none, so every named argument was simply passed in the` |
|         - | 2009 | `` * order it was WRITTEN. `str_pad(length: 5, string: "x")` reached the builtin as`` |
|         - | 2010 | ` * ("x" at #2, 5 at #1) and reported a TypeError, and — worse, because it is silent —` |
|         - | 2011 | `` * `str_pad("x", 5, pad_type: STR_PAD_LEFT)` bound the flag as $pad_string and answered`` |
|         - | 2012 | ` * "x0000" where php answers "    x". Both spellings are php 8.0 syntax, and the whole` |
|         - | 2013 | ` * ~650-builtin surface plus every native method was affected.` |
|         - | 2014 | ` *` |
|         - | 2015 | ` * The declared signature is the source of names, defaults and positions — the same string` |
|         - | 2016 | ` * Reflection prints. Rewrites *pnArg / apArg in place (the caller's argument vector is` |
|         - | 2017 | ` * scratch it owns) and answers SXRET_OK, or throws php's Error and returns its status.` |
|         - | 2018 | ` * Callees with a VARIADIC tail are left alone: php collects extra named arguments into it` |
|         - | 2019 | ` * by NAME, which the positional vector here cannot express.` |
|         - | 2020 | ` */` |
|        94 | 2021 | `PH7_PRIVATE sxi32 PH7_VmBindNamedArgsToSig(` |
|         - | 2022 | `	ph7_context *pCtx,      /* Call context (for the throws) */` |
|         - | 2023 | `	ph7_user_func *pFunc,   /* Callee: its zSig names the parameters */` |
|         - | 2024 | `	VmCallArgMap *pMap,     /* Call-site map; its aNames[] are per ACTUAL slot */` |
|         - | 2025 | `	int *pnArg,             /* IN/OUT: argument count */` |
|         - | 2026 | `	ph7_value **apArg       /* IN/OUT: argument vector */` |
|         - | 2027 | `	)` |
|         2 | 2028 | `{` |
|         - | 2029 | `	/* php's own stubs top out well under this; a signature with more parameters simply` |
|         - | 2030 | `	 * keeps the positional binding it had. */` |
|         - | 2031 | `#define VM_SIG_MAX_PARAM 32` |
|         - | 2032 | `	VmSigParam aParam[VM_SIG_MAX_PARAM];` |
|         - | 2033 | `	ph7_value *apBound[VM_SIG_MAX_PARAM];` |
|         - | 2034 | `	int nParam,nArg,i,nLast;` |
|        96 | 2035 | `	if( pFunc == 0 \|\| pFunc->zSig == 0 \|\| pMap == 0 \|\| pMap->bHasNamed == 0 ){` |
|        13 | 2036 | `		return SXRET_OK;` |
|         - | 2037 | `	}` |
|        84 | 2038 | `	nArg = *pnArg;` |
|        84 | 2039 | `	if( nArg < 1 \|\| nArg > VM_SIG_MAX_PARAM ){` |
|       ! 0 | 2040 | `		return SXRET_OK;` |
|         - | 2041 | `	}` |
|        84 | 2042 | `	nParam = VmSigParams(pFunc->zSig,aParam,VM_SIG_MAX_PARAM);` |
|        84 | 2043 | `	if( nParam < 1 \|\| aParam[nParam-1].bVariadic ){` |
|        21 | 2044 | `		return SXRET_OK;` |
|         - | 2045 | `	}` |
|       252 | 2046 | `	for( i = 0 ; i < nParam ; ++i ){` |
|       190 | 2047 | `		apBound[i] = 0;` |
|        96 | 2048 | `	}` |
|        64 | 2049 | `	nLast = -1;` |
|       198 | 2050 | `	for( i = 0 ; i < nArg ; ++i ){` |
|       142 | 2051 | `		int p = i;` |
|       202 | 2052 | `		if( i < (int)pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|       128 | 2053 | `			SyString *pName = &pMap->aNames[i];` |
|       246 | 2054 | `			for( p = 0 ; p < nParam ; ++p ){` |
|       240 | 2055 | `				if( (int)pName->nByte == aParam[p].nName` |
|       199 | 2056 | `				 && SyMemcmp(pName->zString,aParam[p].zName,pName->nByte) == 0 ){` |
|       124 | 2057 | `					break;` |
|         - | 2058 | `				}` |
|        60 | 2059 | `			}` |
|       128 | 2060 | `			if( p >= nParam ){` |
|         7 | 2061 | `				return PH7_VmThrowException(pCtx,"Error",` |
|         2 | 2062 | `					"Unknown named parameter $%z",pName);` |
|         - | 2063 | `			}` |
|       124 | 2064 | `			if( apBound[p] ){` |
|         4 | 2065 | `				return PH7_VmThrowException(pCtx,"Error",` |
|         1 | 2066 | `					"Named parameter $%z overwrites previous argument",pName);` |
|         2 | 2067 | `			}` |
|        75 | 2068 | `		}else if( p >= nParam ){` |
|       ! 0 | 2069 | `			return SXRET_OK; /* more positional arguments than the signature knows */` |
|         - | 2070 | `		}` |
|       136 | 2071 | `		apBound[p] = apArg[i];` |
|       136 | 2072 | `		if( p > nLast ){` |
|       114 | 2073 | `			nLast = p;` |
|        56 | 2074 | `		}` |
|        69 | 2075 | `	}` |
|       188 | 2076 | `	for( i = 0 ; i <= nLast ; ++i ){` |
|       134 | 2077 | `		if( apBound[i] == 0 ){` |
|         7 | 2078 | `			ph7_value *pDef = ph7_context_new_scalar(pCtx);` |
|         7 | 2079 | `			if( pDef == 0 \|\| !VmSigDefaultValue(pCtx->pVm,&aParam[i],pDef) ){` |
|         - | 2080 | `				SyString sName;` |
|         3 | 2081 | `				SyStringInitFromBuf(&sName,aParam[i].zName,(sxu32)aParam[i].nName);` |
|         4 | 2082 | `				return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|         1 | 2083 | `					"%z(): Argument #%d ($%z) not passed",&pFunc->sName,i + 1,&sName);` |
|         - | 2084 | `			}` |
|         5 | 2085 | `			apBound[i] = pDef;` |
|         2 | 2086 | `		}` |
|        67 | 2087 | `	}` |
|       184 | 2088 | `	for( i = 0 ; i <= nLast ; ++i ){` |
|       130 | 2089 | `		apArg[i] = apBound[i];` |
|        66 | 2090 | `	}` |
|        56 | 2091 | `	*pnArg = nLast + 1;` |
|        56 | 2092 | `	return SXRET_OK;` |
|        49 | 2093 | `}` |
|         - | 2094 | `/*` |
|         - | 2095 | ` * Name the Nth (0-based) parameter of a declared signature, without the '$'.` |
|         - | 2096 | ` *` |
|         - | 2097 | ` * The signature string is the only place a host function's parameter names live, and` |
|         - | 2098 | `` * php puts them in diagnostics — `sort(): Argument #1 ($array) …`. Answers 0 when the`` |
|         - | 2099 | ` * signature has no such parameter (or none with a name).` |
|         - | 2100 | ` */` |
|        12 | 2101 | `PH7_PRIVATE int PH7_VmSigParamName(const char *zSig,int nPos,SyString *pOut)` |
|         2 | 2102 | `{` |
|         - | 2103 | `	VmSigParam aParam[VM_SIG_MAX_PARAM];` |
|         - | 2104 | `	int nParam;` |
|        14 | 2105 | `	if( zSig == 0 \|\| nPos < 0 \|\| nPos >= VM_SIG_MAX_PARAM ){` |
|       ! 0 | 2106 | `		return 0;` |
|         - | 2107 | `	}` |
|        14 | 2108 | `	nParam = VmSigParams(zSig,aParam,VM_SIG_MAX_PARAM);` |
|        14 | 2109 | `	if( nPos >= nParam \|\| aParam[nPos].nName < 1 ){` |
|       ! 0 | 2110 | `		return 0;` |
|         - | 2111 | `	}` |
|        14 | 2112 | `	SyStringInitFromBuf(pOut,aParam[nPos].zName,(sxu32)aParam[nPos].nName);` |
|        14 | 2113 | `	return 1;` |
|         8 | 2114 | `}` |
|         - | 2115 | `/*` |
|         - | 2116 | `` * A `&` in a builtin's signature is not always php's ZEND_SEND_ARG_BY_REF.`` |
|         - | 2117 | ` *` |
|         - | 2118 | ` * php has a second mode, ZEND_SEND_PREFER_REF: bind by reference when the argument IS a` |
|         - | 2119 | ` * variable, and otherwise take it by value without a word. Reflection prints those` |
|         - | 2120 | ` * parameters as by-reference like any other and PHL's signature string cannot say which` |
|         - | 2121 | `` * mode a `&` means, so the two are told apart here. Probed value-for-value against php`` |
|         - | 2122 | `` * 8.5 over every `&` row PHL declares (41 of them): all but extract() refuse a`` |
|         - | 2123 | `` * non-variable, and extract() answers `int(1)` for `extract(['q' => 1])`.`` |
|         - | 2124 | ` *` |
|         - | 2125 | ` * array_multisort() is listed with it because it is php's other prefer-ref builtin and` |
|         - | 2126 | ` * PHL will need this the day it gains one (it is a MISSING builtin today, §5).` |
|         - | 2127 | ` */` |
|     54206 | 2128 | `PH7_PRIVATE int VmBuiltinPrefersRef(SyString *pName)` |
|         5 | 2129 | `{` |
|         - | 2130 | `	static const char *const azPreferRef[] = { "extract", "array_multisort" };` |
|         - | 2131 | `	sxu32 i;` |
|    162427 | 2132 | `	for( i = 0 ; i < SX_ARRAYSIZE(azPreferRef) ; ++i ){` |
|    108337 | 2133 | `		sxu32 nByte = SyStrlen(azPreferRef[i]);` |
|    108332 | 2134 | `		if( pName->nByte == nByte` |
|     54266 | 2135 | `		 && SyMemcmp(pName->zString,azPreferRef[i],nByte) == 0 ){` |
|       121 | 2136 | `			return 1;` |
|         - | 2137 | `		}` |
|     54113 | 2138 | `	}` |
|     54095 | 2139 | `	return 0;` |
|     27108 | 2140 | `}` |
|         - | 2141 | `/*` |
|         - | 2142 | ` * php refuses a by-reference argument at the CALL, before the callee's ZPP runs, and it` |
|         - | 2143 | ``  * decides from the argument's SHAPE, not from its value: `sort([3,1])`, `usort('x',$cb)` `` |
|         - | 2144 | `` * and `preg_match($p,$s,'lit')` are all`` |
|         - | 2145 | `` * `Error: sort(): Argument #1 ($array) could not be passed by reference`.`` |
|         - | 2146 | ` *` |
|         - | 2147 | ` * The call site's compile-time shape mask (VmCallArgMap.nNonLvalMask) is what says so.` |
|         - | 2148 | ` * Only five builtins raised anything before this, from their own bodies, on the runtime` |
|         - | 2149 | `` * `nIdx == SXU32_HIGH` signal — which cannot tell a literal from the result of a call, a`` |
|         - | 2150 | `` * shape php ACCEPTS with a notice. The thirty other `&` rows answered `true`/`false`/an`` |
|         - | 2151 | ` * int: the same answers they give for work they really did.` |
|         - | 2152 | ` *` |
|         - | 2153 | ` * Skipped when the call site has no shape mask (a spread, an indirect dispatch through` |
|         - | 2154 | ` * call_user_func, an engine-synthesized call) or uses named arguments (which rebind` |
|         - | 2155 | ` * positions the mask is indexed by). The by-ref positions come from the same declared` |
|         - | 2156 | ` * signature everything else here reads.` |
|         - | 2157 | ` */` |
|   2742489 | 2158 | `PH7_PRIVATE sxi32 PH7_VmScreenByRefArgShapes(` |
|         - | 2159 | `	ph7_context *pCtx,     /* Call context (for the throw) */` |
|         - | 2160 | `	ph7_user_func *pFunc,  /* Callee: its zSig names and marks the parameters */` |
|         - | 2161 | `	VmCallArgMap *pMap,    /* Call-site map, or 0 */` |
|         - | 2162 | `	int nGiven,            /* Argument count */` |
|         - | 2163 | `	ph7_value **apArg      /* Arguments */` |
|         - | 2164 | `	)` |
|         5 | 2165 | `{` |
|         - | 2166 | `	VmSigParam aParam[VM_SIG_MAX_PARAM];` |
|         - | 2167 | `	int nParam,n;` |
|         - | 2168 | `	/* The by-ref mask first: it is 0 for all but 41 of the ~650 host functions, so` |
|         - | 2169 | `	 * every other call leaves through one test. */` |
|   2742494 | 2170 | `	if( pFunc == 0 \|\| pFunc->nByRefMask == 0 \|\| pFunc->zSig == 0 \|\| nGiven < 1 ){` |
|   2686432 | 2171 | `		return SXRET_OK;` |
|         - | 2172 | `	}` |
|     56067 | 2173 | `	if( pMap == 0 \|\| !pMap->bArgShapes \|\| pMap->bHasNamed ){` |
|        25 | 2174 | `		return SXRET_OK;` |
|         - | 2175 | `	}` |
|     56045 | 2176 | `	if( (pMap->nNonLvalMask \| pMap->nTempCallMask) == 0 ){` |
|      1887 | 2177 | `		return SXRET_OK;` |
|         - | 2178 | `	}` |
|     54163 | 2179 | `	if( VmBuiltinPrefersRef(&pFunc->sName) ){` |
|       117 | 2180 | `		return SXRET_OK;` |
|         - | 2181 | `	}` |
|     54051 | 2182 | `	nParam = VmSigParams(pFunc->zSig,aParam,VM_SIG_MAX_PARAM);` |
|    215331 | 2183 | `	for( n = 0 ; n < nGiven && n < 31 ; ++n ){` |
|    161335 | 2184 | `		if( (pFunc->nByRefMask & (1u << n)) == 0 ){` |
|    160449 | 2185 | `			continue;` |
|         - | 2186 | `		}` |
|       891 | 2187 | `		if( (pMap->nNonLvalMask & (1u << n)) == 0 ){` |
|         - | 2188 | `			/* Not a refusal — but a CALL result in this position is php's notice,` |
|         - | 2189 | `			 * and then the builtin operates on the temporary. */` |
|       841 | 2190 | `			PH7_VmArgTempCallNotice(pCtx->pVm,pMap,(sxu32)n,apArg[n]);` |
|       841 | 2191 | `			continue;` |
|         - | 2192 | `		}` |
|        55 | 2193 | `		if( n < nParam && aParam[n].nName > 0 ){` |
|        80 | 2194 | `			return PH7_VmThrowException(pCtx,"Error",` |
|         - | 2195 | `				"%z(): Argument #%d ($%.*s) could not be passed by reference",` |
|        25 | 2196 | `				&pFunc->sName,n + 1,aParam[n].nName,aParam[n].zName);` |
|         - | 2197 | `		}` |
|       ! 0 | 2198 | `		return PH7_VmThrowException(pCtx,"Error",` |
|         - | 2199 | `			"%z(): Argument #%d could not be passed by reference",` |
|       ! 0 | 2200 | `			&pFunc->sName,n + 1);` |
|       ! 0 | 2201 | `	}` |
|     54001 | 2202 | `	return SXRET_OK;` |
|   1372052 | 2203 | `}` |
|         - | 2204 | `/*` |
|         - | 2205 | ` * D1: derive a by-reference position bitmask from a php-style signature string.` |
|         - | 2206 | `` * Bit N is set when positional parameter N is declared by-reference (a `&` appears`` |
|         - | 2207 | `` * anywhere in that comma-separated parameter, e.g. `&$matches`, `&...$vars`). Only`` |
|         - | 2208 | ` * the first 31 positions are representable; a by-ref parameter past that is rare` |
|         - | 2209 | ` * for a builtin and simply not tracked here. The scan mirrors VmDeriveArityFromSig.` |
|         - | 2210 | ` */` |
|   5812592 | 2211 | `PH7_PRIVATE sxu32 VmDeriveByRefMaskFromSig(const char *zSig)` |
|         5 | 2212 | `{` |
|   5812597 | 2213 | `	sxu32 mask = 0;` |
|   5812597 | 2214 | `	int n = 0;       /* current parameter index */` |
|   5812597 | 2215 | `	int bSeen = 0;   /* current parameter has non-space content */` |
|   5812597 | 2216 | ``	int bRef = 0;    /* current parameter carries a by-ref `&` */`` |
|   5812597 | 2217 | ``	int bVar = 0;    /* current parameter is a `...` variadic */`` |
|   5812597 | 2218 | `	int bTailRef = 0;/* the LAST parameter was a by-ref variadic */` |
|   5812597 | 2219 | `	const char *zCur = zSig;` |
|  53126497 | 2220 | `	for(;;){` |
| 109008151 | 2221 | `		if( zCur[0] == '\'' \|\| zCur[0] == '"' ){` |
|    170767 | 2222 | `			bSeen = 1;` |
|    170767 | 2223 | `			zCur = VmSigSkipQuoted(zCur);` |
|    170767 | 2224 | `			if( zCur[0] != '\0' ){` |
|    170767 | 2225 | `				zCur++;` |
|     85381 | 2226 | `			}` |
|    170767 | 2227 | `			continue;` |
|         - | 2228 | `		}` |
| 108837389 | 2229 | `		if( zCur[0] == '\0' \|\| zCur[0] == ',' ){` |
|   8396987 | 2230 | `			if( bSeen ){` |
|   5987237 | 2231 | `				if( bRef && n < 31 ){` |
|    207881 | 2232 | `					mask \|= (1u << n);` |
|    103938 | 2233 | `				}` |
|   5987237 | 2234 | `				bTailRef = (bRef && bVar);` |
|   5987237 | 2235 | `				n++;` |
|   2993616 | 2236 | `			}` |
|   8396987 | 2237 | `			if( zCur[0] == '\0' ){` |
|   5812597 | 2238 | `				break;` |
|         - | 2239 | `			}` |
|   2584395 | 2240 | `			bSeen = bRef = bVar = 0;` |
|   2584395 | 2241 | `			zCur++;` |
|   2584395 | 2242 | `			continue;` |
|         - | 2243 | `		}` |
| 100440407 | 2244 | `		if( zCur[0] != ' ' ){` |
|  87963135 | 2245 | `			bSeen = 1;` |
|  43981565 | 2246 | `		}` |
| 100440407 | 2247 | `		if( zCur[0] == '&' ){` |
|    207881 | 2248 | `			bRef = 1;` |
|    103938 | 2249 | `		}` |
| 100440407 | 2250 | `		if( zCur[0] == '.' && zCur[1] == '.' && zCur[2] == '.' ){` |
|         - | 2251 | ``			/* A `...` tail, not a numeric default's decimal point. */`` |
|    157863 | 2252 | `			bVar = 1;` |
|     78929 | 2253 | `		}` |
| 100440407 | 2254 | `		zCur++;` |
|         5 | 2255 | `	}` |
|   5812597 | 2256 | `	if( bTailRef && n > 0 && n <= 31 ){` |
|         - | 2257 | ``		/* A by-ref `&...` tail absorbs every later actual (array_multisort's`` |
|         - | 2258 | ``		 * `&...$rest`): without this, the deferred-argument resolver read the`` |
|         - | 2259 | ``		 * tail positions as by-VALUE and warned `Undefined variable` on an`` |
|         - | 2260 | `		 * undefined actual php binds silently. */` |
|      4081 | 2261 | `		mask \|= ~((1u << (n - 1)) - 1u);` |
|      2038 | 2262 | `	}` |
|   5812597 | 2263 | `	return mask;` |
|         5 | 2264 | `}` |
|      4076 | 2265 | `PH7_PRIVATE void VmSetBuiltinSignatures(ph7_vm *pVm)` |
|         5 | 2266 | `{` |
|         - | 2267 | `	sxu32 n;` |
|   2364085 | 2268 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinSig) ; n++ ){` |
|   3540011 | 2269 | `		SyHashEntry *pEntry = SyHashGet(&pVm->hHostFunction,` |
|   2360004 | 2270 | `			(const void *)aBuiltinSig[n].zName,(sxu32)SyStrlen(aBuiltinSig[n].zName));` |
|   2360009 | 2271 | `		if( pEntry ){` |
|   2319249 | 2272 | `			ph7_user_func *pFunc = (ph7_user_func *)pEntry->pUserData;` |
|   2319249 | 2273 | `			sxi16 nMin = 0, nMax = 0;` |
|   2319249 | 2274 | `			sxu8 bAtLeast = 0, bHasMax = 0;` |
|   2319249 | 2275 | `			pFunc->zSig = aBuiltinSig[n].zSig;` |
|   2319249 | 2276 | `			pFunc->zRet = aBuiltinSig[n].zRet[0] ? aBuiltinSig[n].zRet : 0;` |
|   2319249 | 2277 | `			pFunc->nByRefMask = VmDeriveByRefMaskFromSig(pFunc->zSig);` |
|   2319249 | 2278 | `			VmDeriveArityFromSig(pFunc->zSig,&nMin,&bAtLeast,&nMax,&bHasMax);` |
|         - | 2279 | `			/* The MAXIMUM always comes from the signature: the curated override` |
|         - | 2280 | `			 * table speaks only to the minimum (and its wording). */` |
|   2319249 | 2281 | `			pFunc->nMaxArg = nMax;` |
|   2319249 | 2282 | `			pFunc->bHasMaxArg = (sxu8)(VmBuiltinSelfValidatesArity(aBuiltinSig[n].zName) ? 0 : bHasMax);` |
|   2319249 | 2283 | `			if( pFunc->nMinArg < 1 ){` |
|         - | 2284 | `				/* VmSetBuiltinArity() ran first: a non-zero minimum here means the` |
|         - | 2285 | `				 * curated override already spoke for this builtin, so leave it. */` |
|   1182045 | 2286 | `				pFunc->nMinArg = nMin;` |
|   1182045 | 2287 | `				pFunc->bAtLeast = bAtLeast;` |
|    591020 | 2288 | `			}` |
|   1159622 | 2289 | `		}` |
|   1180007 | 2290 | `	}` |
|      4081 | 2291 | `}` |
|         - | 2292 | `/*` |
|         - | 2293 | ` * Signature lookup by function name, for the reflection layer: embedded-PHP` |
|         - | 2294 | ` * builtins (max/min/Exception methods...) are ph7_vm_func instances, not` |
|         - | 2295 | ` * host functions, so they miss the VmSetBuiltinSignatures stamping and pull` |
|         - | 2296 | ` * their row on demand here. Linear scan — reflection-path only.` |
|         - | 2297 | ` */` |
|       ! 0 | 2298 | `PH7_PRIVATE const char * PH7_VmBuiltinSigLookup(const char *zName,sxu32 nLen,const char **pzRet)` |
|       ! 0 | 2299 | `{` |
|         - | 2300 | `	sxu32 n;` |
|       ! 0 | 2301 | `	if( pzRet ){` |
|       ! 0 | 2302 | `		*pzRet = 0;` |
|       ! 0 | 2303 | `	}` |
|       ! 0 | 2304 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinSig) ; n++ ){` |
|       ! 0 | 2305 | `		if( SyStrlen(aBuiltinSig[n].zName) == nLen` |
|       ! 0 | 2306 | `		 && SyMemcmp(aBuiltinSig[n].zName,zName,nLen) == 0 ){` |
|       ! 0 | 2307 | `			if( pzRet && aBuiltinSig[n].zRet[0] ){` |
|       ! 0 | 2308 | `				*pzRet = aBuiltinSig[n].zRet;` |
|       ! 0 | 2309 | `			}` |
|       ! 0 | 2310 | `			return aBuiltinSig[n].zSig;` |
|         - | 2311 | `		}` |
|       ! 0 | 2312 | `	}` |
|       ! 0 | 2313 | `	return 0;` |
|       ! 0 | 2314 | `}` |
|         - | 2315 | `/*` |
|         - | 2316 | ` * Write a value back to the caller's variable through a builtin argument's` |
|         - | 2317 | ` * stack-slot nIdx — the shared write-back for builtin by-reference` |
|         - | 2318 | ` * out-parameters (preg_match $matches, preg_replace &$count, similar_text` |
|         - | 2319 | ` * &$percent, ...).` |
|         - | 2320 | ` *` |
|         - | 2321 | ` * For a positional out-param argument the call compiler auto-vivifies known` |
|         - | 2322 | ` * by-reference out-params (see GenStateByRefBuiltinMask in compile.c), so a` |
|         - | 2323 | ` * bare undefined variable, an array subscript, and a declared/untyped` |
|         - | 2324 | ` * property all arrive with a real nIdx and are written back here, matching` |
|         - | 2325 | ` * PHP's reference semantics.` |
|         - | 2326 | ` *` |
|         - | 2327 | ` * nIdx stays SXU32_HIGH and the write-back to the caller is skipped (the` |
|         - | 2328 | ` * value still lands in the local stack slot) when the argument cannot expose` |
|         - | 2329 | ` * a stable memobj slot: a literal, a function-call result, a subscript of a` |
|         - | 2330 | ` * non-lvalue parent (foo()['k']), or any variable in a call that also uses` |
|         - | 2331 | ` * named or spread arguments (compile-time positions no longer map to the` |
|         - | 2332 | ` * runtime arg slots, so the compiler conservatively does not vivify). An` |
|         - | 2333 | ` * uninitialized typed property is also not wired (it throws before the` |
|         - | 2334 | ` * write) -- see the recorded deferrals.` |
|         - | 2335 | ` */` |
|       622 | 2336 | `PH7_PRIVATE void PH7_VmStoreArgByRef(ph7_vm *pVm,ph7_value *pArg,ph7_value *pNewVal)` |
|         5 | 2337 | `{` |
|       627 | 2338 | `	if( pArg->nIdx != SXU32_HIGH ){` |
|       579 | 2339 | `		ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pArg->nIdx);` |
|       579 | 2340 | `		if( pObj ){` |
|       579 | 2341 | `			PH7_MemObjStore(pNewVal,pObj);` |
|       287 | 2342 | `		}` |
|       287 | 2343 | `	}` |
|       627 | 2344 | `	PH7_MemObjStore(pNewVal,pArg);` |
|       627 | 2345 | `}` |
|         - | 2346 | `/*` |
|         - | 2347 | ` * Raise an E_WARNING whose text is used VERBATIM.` |
|         - | 2348 | ` * ph7_context_throw_error_format() prepends "func(): " to whatever it is given, but php's` |
|         - | 2349 | ` * IO warnings put the offending path inside those parens -- "file_get_contents(/nope):` |
|         - | 2350 | ` * Failed to open stream: ..." -- so a caller that needs php's exact shape must build the` |
|         - | 2351 | ` * whole line itself and come through here.` |
|         - | 2352 | ` */` |
|         - | 2353 | `/*` |
|         - | 2354 | ` * Validate a callback argument and throw php's TypeError when it cannot be called.` |
|         - | 2355 | ` *` |
|         - | 2356 | ` * The shared dispatcher (PH7_VmCallUserFunctionWithMap) answers SXRET_OK with a NULL` |
|         - | 2357 | ` * result for an unresolvable callable, so every caller that did not check first failed` |
|         - | 2358 | ` * SILENTLY: call_user_func() returned NULL and usort() left the array sorted by nothing` |
|         - | 2359 | ` * at all. php rejects the ARGUMENT up front, naming exactly why.` |
|         - | 2360 | ` */` |
|      1046 | 2361 | `PH7_PRIVATE sxi32 PH7_CheckCallbackArg(` |
|         - | 2362 | `	ph7_context *pCtx,   /* Calling context (names the function in the message) */` |
|         - | 2363 | `	ph7_value *pCb,      /* The callback argument */` |
|         - | 2364 | `	int iArg,            /* Its 1-based position */` |
|         - | 2365 | `	const char *zParam,  /* Its php parameter name ("callback"), or 0 for the variadic` |
|         - | 2366 | `	                      * comparators php names by position only (array_udiff …) */` |
|         - | 2367 | `	int bNullable        /* TRUE when php's text says "or null" */` |
|         - | 2368 | `	)` |
|         5 | 2369 | `{` |
|         - | 2370 | `	char zReason[256];` |
|      1051 | 2371 | `	const char *zWhy = PH7_VmCallableReason(pCtx->pVm,pCb,zReason,sizeof(zReason));` |
|      1051 | 2372 | `	if( zWhy == 0 ){` |
|       869 | 2373 | `		return PH7_OK;` |
|         - | 2374 | `	}` |
|       187 | 2375 | `	if( zParam ){` |
|       197 | 2376 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 2377 | `			"%s(): Argument #%d ($%s) must be a valid callback%s, %s",` |
|        64 | 2378 | `			ph7_function_name(pCtx),iArg,zParam,bNullable ? " or null" : "",zWhy);` |
|         - | 2379 | `	}` |
|        86 | 2380 | `	return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 2381 | `		"%s(): Argument #%d must be a valid callback%s, %s",` |
|        27 | 2382 | `		ph7_function_name(pCtx),iArg,bNullable ? " or null" : "",zWhy);` |
|       528 | 2383 | `}` |
|     23840 | 2384 | `PH7_PRIVATE void PH7_VmThrowWarningFmt(ph7_vm *pVm,const char *zFmt,...)` |
|         5 | 2385 | `{` |
|         - | 2386 | `	va_list ap;` |
|     23845 | 2387 | `	va_start(ap,zFmt);` |
|     23845 | 2388 | `	PH7_VmThrowErrorAp(pVm,0,PH7_CTX_WARNING,zFmt,ap);` |
|     23845 | 2389 | `	va_end(ap);` |
|     23845 | 2390 | `}` |
|         - | 2391 | `/*` |
|         - | 2392 | ` * Emit a formatted E_USER_DEPRECATED diagnostic with no function-name prefix:` |
|         - | 2393 | ` * php reports #[\Deprecated] as USER-deprecated (16384, it is userland-authored),` |
|         - | 2394 | ` * not the engine's E_DEPRECATED (8192) — handler-visible errno matters.` |
|         - | 2395 | ` */` |
|        34 | 2396 | `static void VmThrowUserDeprecatedFmt(ph7_vm *pVm,const char *zFmt,...)` |
|         1 | 2397 | `{` |
|         - | 2398 | `	va_list ap;` |
|        35 | 2399 | `	va_start(ap,zFmt);` |
|        35 | 2400 | `	PH7_VmThrowErrorAp(pVm,0,E_USER_DEPRECATED,zFmt,ap);` |
|        35 | 2401 | `	va_end(ap);` |
|        35 | 2402 | `}` |
|         - | 2403 | `/*` |
|         - | 2404 | ` * php 8.4 #[\Deprecated]: emit the E_USER_DEPRECATED notice for a call to a user` |
|         - | 2405 | ` * function/method carrying the attribute. Message shapes (php-exact):` |
|         - | 2406 | ` *   Function f() is deprecated` |
|         - | 2407 | ` *   Method C::m() is deprecated since 2.0, use g() instead` |
|         - | 2408 | ` * ("since" from the attribute's since: argument; the trailing ", msg" from` |
|         - | 2409 | ` * message:/positional #1.) The attribute arguments are compiled constant` |
|         - | 2410 | ` * expressions — evaluated via VmLocalExec, the reflection thunks' pattern.` |
|         - | 2411 | ` * Called from OP_CALL once per call, only when the callee HAS attributes;` |
|         - | 2412 | ` * pDeclClass names the method's declaring class (0 for plain functions).` |
|         - | 2413 | ` */` |
|         - | 2414 | `/*` |
|         - | 2415 | ` * Expand a global constant's value, emitting the php 8.5 #[\Deprecated]` |
|         - | 2416 | ` * E_USER_DEPRECATED notice first when the constant carries attributes` |
|         - | 2417 | `` * (`Constant GD is deprecated since 1.2` — every access re-warns).`` |
|         - | 2418 | ` */` |
|         - | 2419 | `static void VmDeprecatedAttrNoticeSubject(ph7_vm *pVm,SySet *pAttrs,` |
|         - | 2420 | `	const char *zKind,const SyString *pQual,const SyString *pName);` |
|         - | 2421 | `/*` |
|         - | 2422 | ` * Expand a constant, emitting the php 8.5 #[\Deprecated] E_USER_DEPRECATED notice` |
|         - | 2423 | `` * first when a USERLAND constant carries the attribute (`Constant GD is deprecated`` |
|         - | 2424 | `` * since 1.2` — every access re-warns). Engine constants php merely deprecates are`` |
|         - | 2425 | ` * not mimicked: PHL removes them outright (see the scope policy), so there is no` |
|         - | 2426 | ` * engine-side E_DEPRECATED list here.` |
|         - | 2427 | ` */` |
|    127916 | 2428 | `PH7_PRIVATE void VmExpandConstantWithNotice(ph7_vm *pVm,ph7_constant *pCons,ph7_value *pOut)` |
|         5 | 2429 | `{` |
|    127921 | 2430 | `	if( SySetUsed(&pCons->aAttrs) > 0 ){` |
|         7 | 2431 | `		VmDeprecatedAttrNoticeSubject(pVm,&pCons->aAttrs,"Constant",0,&pCons->sName);` |
|         3 | 2432 | `	}` |
|    127921 | 2433 | `	pCons->xExpand(pOut,pCons->pUserData);` |
|    127921 | 2434 | `}` |
|         - | 2435 | `/*` |
|         - | 2436 | ` * Query a GLOBAL constant by its exact (case-sensitive) name and expand its` |
|         - | 2437 | ` * value into pOut, which the caller has initialized. Returns 1 when the` |
|         - | 2438 | ` * constant exists. The ini scanner's NORMAL/TYPED value interpretation is the` |
|         - | 2439 | ` * caller: php substitutes a defined constant's value for a bare identifier` |
|         - | 2440 | ` * token inside an unquoted ini value.` |
|         - | 2441 | ` */` |
|        44 | 2442 | `PH7_PRIVATE int PH7_VmQueryConstant(ph7_vm *pVm,const char *zName,sxu32 nName,ph7_value *pOut)` |
|         1 | 2443 | `{` |
|         - | 2444 | `	SyHashEntry *pEntry;` |
|         - | 2445 | `	ph7_constant *pCons;` |
|        45 | 2446 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)zName,nName);` |
|        45 | 2447 | `	if( pEntry == 0 ){` |
|        41 | 2448 | `		return 0;` |
|         - | 2449 | `	}` |
|         5 | 2450 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|         5 | 2451 | `	VmExpandConstantWithNotice(pVm,pCons,pOut);` |
|         5 | 2452 | `	return 1;` |
|        23 | 2453 | `}` |
|         - | 2454 | `/*` |
|         - | 2455 | ` * Scan a declared-attribute set for #[\Deprecated]; when found, evaluate its` |
|         - | 2456 | ` * message:/since: arguments (positional #0 = message, #1 = since) into the` |
|         - | 2457 | ` * caller's values and return TRUE. The base subject text ("Function f()",` |
|         - | 2458 | ` * "Constant C::K") is the caller's business.` |
|         - | 2459 | ` */` |
|       204 | 2460 | `static int VmDeprecatedAttrExtract(ph7_vm *pVm,SySet *pAttrs,` |
|         - | 2461 | `	ph7_value *pMsg,int *pbMsg,ph7_value *pSince,int *pbSince)` |
|         5 | 2462 | `{` |
|       209 | 2463 | `	ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|         - | 2464 | `	sxu32 n;` |
|       209 | 2465 | `	*pbMsg = *pbSince = 0;` |
|       383 | 2466 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; ++n ){` |
|       213 | 2467 | `		ph7_attribute *pAttr = &aAttr[n];` |
|         - | 2468 | `		ph7_attr_arg *aArg;` |
|       213 | 2469 | `		sxu32 i,nPos = 0;` |
|       208 | 2470 | `		if( SyStringLength(&pAttr->sName) != sizeof("Deprecated")-1` |
|       126 | 2471 | `		 \|\| SyStrnicmp(SyStringData(&pAttr->sName),"Deprecated",sizeof("Deprecated")-1) != 0 ){` |
|       179 | 2472 | `			continue;` |
|         - | 2473 | `		}` |
|        35 | 2474 | `		aArg = (ph7_attr_arg *)SySetBasePtr(&pAttr->aArgs);` |
|        53 | 2475 | `		for( i = 0 ; i < SySetUsed(&pAttr->aArgs) ; ++i ){` |
|        19 | 2476 | `			ph7_attr_arg *pArg = &aArg[i];` |
|        19 | 2477 | `			int isMsg = 0,isSince = 0;` |
|        19 | 2478 | `			if( SyStringLength(&pArg->sName) == 0 ){` |
|         3 | 2479 | `				isMsg = (nPos == 0);` |
|         3 | 2480 | `				isSince = (nPos == 1);` |
|         3 | 2481 | `				nPos++;` |
|        18 | 2482 | `			}else if( SyStringLength(&pArg->sName) == sizeof("message")-1` |
|        12 | 2483 | `			 && SyMemcmp(SyStringData(&pArg->sName),"message",sizeof("message")-1) == 0 ){` |
|         7 | 2484 | `				isMsg = 1;` |
|        14 | 2485 | `			}else if( SyStringLength(&pArg->sName) == sizeof("since")-1` |
|        11 | 2486 | `			 && SyMemcmp(SyStringData(&pArg->sName),"since",sizeof("since")-1) == 0 ){` |
|        11 | 2487 | `				isSince = 1;` |
|         5 | 2488 | `			}` |
|        19 | 2489 | `			if( isMsg && !*pbMsg && SySetUsed(&pArg->aByteCode) > 0 ){` |
|        13 | 2490 | `				if( VmLocalExec(pVm,&pArg->aByteCode,pMsg,FALSE) == SXRET_OK ){` |
|         9 | 2491 | `					if( (pMsg->iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2492 | `						PH7_MemObjToString(pMsg);` |
|       ! 0 | 2493 | `					}` |
|         9 | 2494 | `					*pbMsg = 1;` |
|         5 | 2495 | `				}` |
|        15 | 2496 | `			}else if( isSince && !*pbSince && SySetUsed(&pArg->aByteCode) > 0 ){` |
|        11 | 2497 | `				if( VmLocalExec(pVm,&pArg->aByteCode,pSince,FALSE) == SXRET_OK ){` |
|        11 | 2498 | `					if( (pSince->iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2499 | `						PH7_MemObjToString(pSince);` |
|       ! 0 | 2500 | `					}` |
|        11 | 2501 | `					*pbSince = 1;` |
|         5 | 2502 | `				}` |
|         5 | 2503 | `			}` |
|        10 | 2504 | `		}` |
|        35 | 2505 | `		return 1;` |
|       ! 0 | 2506 | `	}` |
|       175 | 2507 | `	return 0;` |
|       107 | 2508 | `}` |
|         - | 2509 | `/*` |
|         - | 2510 | ` * Append php's " since X" / ", message" suffixes to a built base subject and` |
|         - | 2511 | ` * emit the E_USER_DEPRECATED notice.` |
|         - | 2512 | ` */` |
|        34 | 2513 | `static void VmDeprecatedEmit(ph7_vm *pVm,SyBlob *pOut,` |
|         - | 2514 | `	ph7_value *pMsg,int bMsg,ph7_value *pSince,int bSince)` |
|         1 | 2515 | `{` |
|        35 | 2516 | `	if( bSince && SyBlobLength(&pSince->sBlob) > 0 ){` |
|        16 | 2517 | `		SyBlobFormat(pOut," since %.*s",(int)SyBlobLength(&pSince->sBlob),` |
|        10 | 2518 | `			(const char *)SyBlobData(&pSince->sBlob));` |
|         5 | 2519 | `	}` |
|        35 | 2520 | `	if( bMsg && SyBlobLength(&pMsg->sBlob) > 0 ){` |
|        13 | 2521 | `		SyBlobFormat(pOut,", %.*s",(int)SyBlobLength(&pMsg->sBlob),` |
|         8 | 2522 | `			(const char *)SyBlobData(&pMsg->sBlob));` |
|         4 | 2523 | `	}` |
|        35 | 2524 | `	VmThrowUserDeprecatedFmt(pVm,"%.*s",(int)SyBlobLength(pOut),(const char *)SyBlobData(pOut));` |
|        35 | 2525 | `}` |
|         - | 2526 | `/*` |
|         - | 2527 | ` * Generic #[\Deprecated] notice for a named subject:` |
|         - | 2528 | ` * "<Kind> [Qual::]Name is deprecated[ since X][, message]".` |
|         - | 2529 | ` */` |
|        18 | 2530 | `static void VmDeprecatedAttrNoticeSubject(ph7_vm *pVm,SySet *pAttrs,` |
|         - | 2531 | `	const char *zKind,const SyString *pQual,const SyString *pName)` |
|         1 | 2532 | `{` |
|         - | 2533 | `	ph7_value sMsg,sSince;` |
|         - | 2534 | `	SyBlob sOut;` |
|         - | 2535 | `	int bMsg,bSince;` |
|        19 | 2536 | `	PH7_MemObjInit(pVm,&sMsg);` |
|        19 | 2537 | `	PH7_MemObjInit(pVm,&sSince);` |
|        19 | 2538 | `	if( VmDeprecatedAttrExtract(pVm,pAttrs,&sMsg,&bMsg,&sSince,&bSince) ){` |
|        15 | 2539 | `		SyBlobInit(&sOut,&pVm->sAllocator);` |
|        15 | 2540 | `		if( pQual ){` |
|        11 | 2541 | `			SyBlobFormat(&sOut,"%s %z::%z is deprecated",zKind,pQual,pName);` |
|         6 | 2542 | `		}else{` |
|         5 | 2543 | `			SyBlobFormat(&sOut,"%s %z is deprecated",zKind,pName);` |
|         - | 2544 | `		}` |
|        15 | 2545 | `		VmDeprecatedEmit(pVm,&sOut,&sMsg,bMsg,&sSince,bSince);` |
|        15 | 2546 | `		SyBlobRelease(&sOut);` |
|         7 | 2547 | `	}` |
|        19 | 2548 | `	PH7_MemObjRelease(&sMsg);` |
|        19 | 2549 | `	PH7_MemObjRelease(&sSince);` |
|        19 | 2550 | `}` |
|       186 | 2551 | `PH7_PRIVATE void VmDeprecatedAttrNotice(ph7_vm *pVm,ph7_vm_func *pFunc,ph7_class *pDeclClass)` |
|         5 | 2552 | `{` |
|         - | 2553 | `	ph7_value sMsg,sSince;` |
|         - | 2554 | `	SyBlob sOut;` |
|         - | 2555 | `	int bMsg,bSince;` |
|       191 | 2556 | `	PH7_MemObjInit(pVm,&sMsg);` |
|       191 | 2557 | `	PH7_MemObjInit(pVm,&sSince);` |
|       191 | 2558 | `	if( VmDeprecatedAttrExtract(pVm,&pFunc->aAttrs,&sMsg,&bMsg,&sSince,&bSince) ){` |
|        21 | 2559 | `		SyBlobInit(&sOut,&pVm->sAllocator);` |
|        21 | 2560 | `		if( pDeclClass ){` |
|         5 | 2561 | `			SyBlobFormat(&sOut,"Method %z::%z() is deprecated",&pDeclClass->sName,&pFunc->sName);` |
|         3 | 2562 | `		}else{` |
|        17 | 2563 | `			SyBlobFormat(&sOut,"Function %z() is deprecated",&pFunc->sName);` |
|         - | 2564 | `		}` |
|        21 | 2565 | `		VmDeprecatedEmit(pVm,&sOut,&sMsg,bMsg,&sSince,bSince);` |
|        21 | 2566 | `		SyBlobRelease(&sOut);` |
|        10 | 2567 | `	}` |
|       191 | 2568 | `	PH7_MemObjRelease(&sMsg);` |
|       191 | 2569 | `	PH7_MemObjRelease(&sSince);` |
|       191 | 2570 | `}` |
|         - | 2571 | `/*` |
|         - | 2572 | ` * Same notice for a #[\Deprecated] class constant, at its static-access site:` |
|         - | 2573 | `` * php's `Constant C::K is deprecated` (each access re-warns).`` |
|         - | 2574 | ` */` |
|        12 | 2575 | `PH7_PRIVATE void VmDeprecatedConstNotice(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pMember)` |
|         1 | 2576 | `{` |
|        19 | 2577 | `	VmDeprecatedAttrNoticeSubject(pVm,&pMember->aAttrs,` |
|        12 | 2578 | `		(pMember->iFlags & PH7_CLASS_ATTR_ENUMCASE) ? "Enum case" : "Constant",` |
|        12 | 2579 | `		&pClass->sName,&pMember->sName);` |
|        13 | 2580 | `}` |
|         - | 2581 | `/*` |
|         - | 2582 | `` * The USER-VISIBLE string coercion a builtin performs on a `mixed` argument or`` |
|         - | 2583 | ` * array element: the builtin-side twin of PH7_MemObjToStringUV's opcode sites.` |
|         - | 2584 | ` * An ARRAY warns "Array to string conversion" and still renders as "Array"; an` |
|         - | 2585 | ` * object whose class has no __toString() -- or one whose __toString() threw --` |
|         - | 2586 | ` * is php's catchable "could not be converted to string" Error, and the builtin` |
|         - | 2587 | ` * must answer that instead of a value.` |
|         - | 2588 | ` *` |
|         - | 2589 | ` * On success pzData and pnLen receive the NUL-terminated bytes (both optional).` |
|         - | 2590 | ` * On a throw they are set to the empty string and the status is returned AND` |
|         - | 2591 | ` * recorded on the call context, so OP_CALL cannot mistake the call for a normal` |
|         - | 2592 | ` * return; a builtin that has already produced output (printf) still keeps it,` |
|         - | 2593 | ` * which is what php does.` |
|         - | 2594 | ` *` |
|         - | 2595 | ` * The public ph7_value_to_string() stays SILENT on purpose: it is the embedder` |
|         - | 2596 | ` * API, and the INTERNAL coercions behind it (array-key canonicalisation, sort` |
|         - | 2597 | ` * comparisons, print_r/var_export/serialize) must not throw -- php's do not` |
|         - | 2598 | ` * either.` |
|         - | 2599 | ` */` |
|    244308 | 2600 | `PH7_PRIVATE sxi32 PH7_ValueToStringUV(ph7_context *pCtx,ph7_value *pValue,const char **pzData,int *pnLen)` |
|         5 | 2601 | `{` |
|    244313 | 2602 | `	sxi32 rc = PH7_MemObjToStringUV(pValue);` |
|    244313 | 2603 | `	if( rc != SXRET_OK ){` |
|        37 | 2604 | `		if( pCtx ){` |
|        37 | 2605 | `			pCtx->nThrowRc = rc;` |
|        17 | 2606 | `		}` |
|        37 | 2607 | `		if( pzData ){` |
|        33 | 2608 | `			*pzData = "";` |
|        15 | 2609 | `		}` |
|        37 | 2610 | `		if( pnLen ){` |
|        33 | 2611 | `			*pnLen = 0;` |
|        15 | 2612 | `		}` |
|        37 | 2613 | `		return rc;` |
|         - | 2614 | `	}` |
|    244279 | 2615 | `	if( pzData \|\| pnLen ){` |
|    244251 | 2616 | `		const char *zData = ph7_value_to_string(pValue,pnLen);` |
|    244251 | 2617 | `		if( pzData ){` |
|    244251 | 2618 | `			*pzData = zData;` |
|    122123 | 2619 | `		}` |
|    122123 | 2620 | `	}` |
|    244279 | 2621 | `	return SXRET_OK;` |
|    122159 | 2622 | `}` |
|         - | 2623 |  |
