# src/ph7/vm_arg_check.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 833/891 lines (93.49%)

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
|         - |  260 | `	{ "stream_resolve_include_path",1, 0 },` |
|         - |  261 | `	{ "rewind",                    1, 0 },` |
|         - |  262 | `	{ "sha1_file",                 1, 1 },` |
|         - |  263 | `	{ "stat",                      1, 0 },` |
|         - |  264 | `	/* Date family */` |
|         - |  265 | `	{ "date",                      1, 1 },` |
|         - |  266 | `	{ "date_default_timezone_set", 1, 1 },` |
|         - |  267 | `	{ "gmdate",                    1, 1 },` |
|         - |  268 | `	{ "gmmktime",                  1, 1 },` |
|         - |  269 | `	{ "idate",                     1, 1 },` |
|         - |  270 | `	{ "mktime",                    1, 1 },` |
|         - |  271 | `	/* Encoding/URL family */` |
|         - |  272 | `	{ "base64_decode",             1, 1 },` |
|         - |  273 | `	{ "base64_encode",             1, 0 },` |
|         - |  274 | `	{ "convert_uudecode",          1, 0 },` |
|         - |  275 | `	{ "convert_uuencode",          1, 0 },` |
|         - |  276 | `	{ "parse_ini_file",            1, 1 },` |
|         - |  277 | `	{ "parse_ini_string",          1, 1 },` |
|         - |  278 | `	{ "parse_url",                 1, 1 },` |
|         - |  279 | `	{ "rawurldecode",              1, 0 },` |
|         - |  280 | `	{ "rawurlencode",              1, 0 },` |
|         - |  281 | `	{ "urldecode",                 1, 0 },` |
|         - |  282 | `	{ "urlencode",                 1, 0 },` |
|         - |  283 | `	/* JSON/serialize family */` |
|         - |  284 | `	{ "filter_var",                1, 1 },` |
|         - |  285 | `	{ "json_decode",               1, 1 },` |
|         - |  286 | `	{ "json_encode",               1, 1 },` |
|         - |  287 | `	{ "json_validate",             1, 1 },` |
|         - |  288 | `	{ "serialize",                 1, 0 },` |
|         - |  289 | `	{ "unserialize",               1, 1 },` |
|         - |  290 | `	/* PCRE family */` |
|         - |  291 | `	{ "preg_match",                2, 1 },` |
|         - |  292 | `	{ "preg_match_all",            2, 1 },` |
|         - |  293 | `	{ "preg_quote",                1, 1 },` |
|         - |  294 | `	{ "preg_replace",              3, 1 },` |
|         - |  295 | `	{ "preg_replace_callback",     3, 1 },` |
|         - |  296 | `	{ "preg_split",                2, 1 },` |
|         - |  297 | `	/* XML family */` |
|         - |  298 | `	/* Constants/misc family */` |
|         - |  299 | `	{ "call_user_func",            1, 1 },` |
|         - |  300 | `	{ "call_user_func_array",      2, 0 },` |
|         - |  301 | `	{ "constant",                  1, 0 },` |
|         - |  302 | `	{ "define",                    2, 1 },` |
|         - |  303 | `	{ "defined",                   1, 0 },` |
|         - |  304 | `	{ "error_log",                 1, 1 },` |
|         - |  305 | `	{ "fnmatch",                   2, 1 },` |
|         - |  306 | `	{ "forward_static_call",       1, 1 },` |
|         - |  307 | `	{ "forward_static_call_array", 2, 0 },` |
|         - |  308 | `	{ "func_get_arg",              1, 0 },` |
|         - |  309 | `	{ "function_exists",           1, 0 },` |
|         - |  310 | `	{ "header",                    1, 1 },` |
|         - |  311 | `	{ "password_get_info",         1, 0 },` |
|         - |  312 | `	{ "putenv",                    1, 0 },` |
|         - |  313 | `	{ "register_shutdown_function", 1, 1 },` |
|         - |  314 | `	{ "set_error_handler",         1, 1 },` |
|         - |  315 | `	{ "set_exception_handler",     1, 0 },` |
|         - |  316 | `	{ "setcookie",                 1, 1 },` |
|         - |  317 | `	{ "setrawcookie",              1, 1 },` |
|         - |  318 | `	{ "trigger_error",             1, 1 },` |
|         - |  319 | `	{ "user_error",                1, 1 },` |
|         - |  320 | `	/*` |
|         - |  321 | `	 * Overrides for signatures that under-report their own minimum: the callback` |
|         - |  322 | `	 * of these three hides inside the variadic tail ("array $array, ...$rest"),` |
|         - |  323 | `	 * so the derivation reads 1 where php requires 2.` |
|         - |  324 | `	 */` |
|         - |  325 | `	{ "array_udiff",               2, 1 },` |
|         - |  326 | `	{ "array_uintersect",          2, 1 },` |
|         - |  327 | `	{ "array_diff_uassoc",         2, 1 },` |
|         - |  328 | `	{ "array_diff_ukey",           2, 1 },` |
|         - |  329 | `	{ "array_intersect_ukey",      2, 1 },` |
|         - |  330 | `	{ "array_intersect_uassoc",    2, 1 },` |
|         - |  331 | `	{ "array_udiff_assoc",         2, 1 },` |
|         - |  332 | `	{ "array_uintersect_assoc",    2, 1 },` |
|         - |  333 | `	{ "array_udiff_uassoc",        3, 1 },` |
|         - |  334 | `	{ "array_uintersect_uassoc",   3, 1 },` |
|         - |  335 | `};` |
|         - |  336 | `/*` |
|         - |  337 | ` * Stamp the minimum-arity metadata from aBuiltinArity[] onto the already` |
|         - |  338 | ` * registered host functions. Called once at VM init after every builtin family` |
|         - |  339 | ` * has been installed into hHostFunction. A name absent from the hash (e.g. a` |
|         - |  340 | ` * build without a given extension) is simply skipped.` |
|         - |  341 | ` */` |
|      4552 |  342 | `PH7_PRIVATE void VmSetBuiltinArity(ph7_vm *pVm)` |
|         5 |  343 | `{` |
|         - |  344 | `	sxu32 n;` |
|   1279117 |  345 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinArity) ; ++n ){` |
|   1274565 |  346 | `		const struct VmBuiltinArity *p = &aBuiltinArity[n];` |
|   2549125 |  347 | `		SyHashEntry *pEntry = SyHashGet(&pVm->hHostFunction,` |
|   1274560 |  348 | `			(const void *)p->zName,SyStrlen(p->zName));` |
|   1274565 |  349 | `		if( pEntry ){` |
|   1274565 |  350 | `			ph7_user_func *pFunc = (ph7_user_func *)pEntry->pUserData;` |
|   1274565 |  351 | `			pFunc->nMinArg  = p->nMin;` |
|   1274565 |  352 | `			pFunc->bAtLeast = p->bAtLeast;` |
|    637280 |  353 | `		}` |
|    637285 |  354 | `	}` |
|      4557 |  355 | `}` |
|         - |  356 | `/*` |
|         - |  357 | ` * PHP 8.5 parameter signatures for the C builtins, generated offline from` |
|         - |  358 | ` * a real PHP 8.5 ReflectionFunction dump over PHL's registered function` |
|         - |  359 | ` * list (see the plan's Reflection section). "= ?" marks an optional` |
|         - |  360 | ` * parameter whose default is not representable as a short literal.` |
|         - |  361 | ` * Reflection parses these strings on demand; unlisted builtins degrade to` |
|         - |  362 | ` * the min-arity data.` |
|         - |  363 | ` */` |
|         - |  364 | `static const struct VmBuiltinSig {` |
|         - |  365 | `	const char *zName;` |
|         - |  366 | `	const char *zSig;` |
|         - |  367 | `	const char *zRet;` |
|         - |  368 | `} aBuiltinSig[] = {` |
|         - |  369 | `	/* The subsystems converted from embedded PHP into C (INI, libxml, sessions).` |
|         - |  370 | `	 * A prelude function declared its parameters in PHP and Reflection read them` |
|         - |  371 | `	 * from there; a C builtin has no declaration but this table, so without a row` |
|         - |  372 | `	 * here the same function reports NO parameters -- and loses its arity bounds` |
|         - |  373 | `	 * with them. */` |
|         - |  374 | `	{ "get_cfg_var", "string $option", "array\|string\|false" },` |
|         - |  375 | `	{ "ini_get", "string $option", "string\|false" },` |
|         - |  376 | `	{ "ini_get_all", "?string $extension = null, bool $details = true", "array\|false" },` |
|         - |  377 | `	{ "ini_restore", "string $option", "void" },` |
|         - |  378 | `	{ "ini_set", "string $option, string\|int\|float\|bool\|null $value", "string\|false" },` |
|         - |  379 | `	{ "libxml_clear_errors", "", "void" },` |
|         - |  380 | `	{ "libxml_get_errors", "", "array" },` |
|         - |  381 | `	{ "libxml_get_last_error", "", "LibXMLError\|false" },` |
|         - |  382 | `	{ "libxml_use_internal_errors", "?bool $use_errors = null", "bool" },` |
|         - |  383 | `	{ "session_abort", "", "bool" },` |
|         - |  384 | `	{ "session_cache_expire", "?int $value = null", "int\|false" },` |
|         - |  385 | `	{ "session_cache_limiter", "?string $value = null", "string\|false" },` |
|         - |  386 | `	{ "session_commit", "", "bool" },` |
|         - |  387 | `	{ "session_create_id", "string $prefix = \"\"", "string\|false" },` |
|         - |  388 | `	{ "session_decode", "string $data", "bool" },` |
|         - |  389 | `	{ "session_destroy", "", "bool" },` |
|         - |  390 | `	{ "session_gc", "", "int\|false" },` |
|         - |  391 | `	{ "session_get_cookie_params", "", "array" },` |
|         - |  392 | `	{ "session_set_save_handler", "$sessionhandler, ...$rest = ?", "bool" },` |
|         - |  393 | `	{ "session_set_cookie_params", "array\|int $lifetime_or_options, ?string $path = null, ?string $domain = null, ?bool $secure = null, ?bool $httponly = null", "bool" },` |
|         - |  394 | `	{ "session_encode", "", "string\|false" },` |
|         - |  395 | `	{ "session_id", "?string $id = null", "string\|false" },` |
|         - |  396 | `	{ "session_module_name", "?string $module = null", "string\|false" },` |
|         - |  397 | `	{ "session_name", "?string $name = null", "string\|false" },` |
|         - |  398 | `	{ "session_regenerate_id", "bool $delete_old_session = false", "bool" },` |
|         - |  399 | `	{ "session_register_shutdown", "", "void" },` |
|         - |  400 | `	{ "session_reset", "", "bool" },` |
|         - |  401 | `	{ "session_save_path", "?string $path = null", "string\|false" },` |
|         - |  402 | `	{ "session_start", "array $options = []", "bool" },` |
|         - |  403 | `	{ "session_status", "", "int" },` |
|         - |  404 | `	{ "session_unset", "", "bool" },` |
|         - |  405 | `	{ "session_write_close", "", "bool" },` |
|         - |  406 | `	{ "abs", "int\|float $num", "int\|float" },` |
|         - |  407 | `	{ "acos", "float $num", "float" },` |
|         - |  408 | `	{ "acosh", "float $num", "float" },` |
|         - |  409 | `	{ "addcslashes", "string $string, string $characters", "string" },` |
|         - |  410 | `	{ "addslashes", "string $string", "string" },` |
|         - |  411 | `	{ "array_all", "array $array, callable $callback", "bool" },` |
|         - |  412 | `	{ "array_any", "array $array, callable $callback", "bool" },` |
|         - |  413 | `	{ "array_chunk", "array $array, int $length, bool $preserve_keys = false", "array" },` |
|         - |  414 | `	{ "array_column", "array $array, string\|int\|null $column_key, string\|int\|null $index_key = NULL", "array" },` |
|         - |  415 | `	{ "array_combine", "array $keys, array $values", "array" },` |
|         - |  416 | `	{ "array_diff", "array $array, array ...$arrays = ?", "array" },` |
|         - |  417 | `	{ "array_diff_assoc", "array $array, array ...$arrays = ?", "array" },` |
|         - |  418 | `	{ "array_diff_key", "array $array, array ...$arrays = ?", "array" },` |
|         - |  419 | `	{ "array_diff_uassoc", "array $array, ...$rest = ?", "array" },` |
|         - |  420 | `	{ "array_diff_ukey", "array $array, ...$rest = ?", "array" },` |
|         - |  421 | `	{ "array_fill", "int $start_index, int $count, mixed $value", "array" },` |
|         - |  422 | `	{ "array_fill_keys", "array $keys, mixed $value", "array" },` |
|         - |  423 | `	{ "array_filter", "array $array, ?callable $callback = NULL, int $mode = 0", "array" },` |
|         - |  424 | `	{ "array_find", "array $array, callable $callback", "mixed" },` |
|         - |  425 | `	{ "array_find_key", "array $array, callable $callback", "mixed" },` |
|         - |  426 | `	{ "array_first", "array $array", "mixed" },` |
|         - |  427 | `	{ "array_flip", "array $array", "array" },` |
|         - |  428 | `	{ "array_intersect", "array $array, array ...$arrays = ?", "array" },` |
|         - |  429 | `	{ "array_intersect_assoc", "array $array, array ...$arrays = ?", "array" },` |
|         - |  430 | `	{ "array_intersect_key", "array $array, array ...$arrays = ?", "array" },` |
|         - |  431 | `	{ "array_intersect_uassoc", "array $array, ...$rest = ?", "array" },` |
|         - |  432 | `	{ "array_intersect_ukey", "array $array, ...$rest = ?", "array" },` |
|         - |  433 | `	{ "array_is_list", "array $array", "bool" },` |
|         - |  434 | `	{ "array_key_exists", "$key, array $array", "bool" },` |
|         - |  435 | `	{ "array_key_first", "array $array", "string\|int\|null" },` |
|         - |  436 | `	{ "array_key_last", "array $array", "string\|int\|null" },` |
|         - |  437 | `	{ "array_keys", "array $array, mixed $filter_value = ?, bool $strict = false", "array" },` |
|         - |  438 | `	{ "array_last", "array $array", "mixed" },` |
|         - |  439 | `	{ "array_map", "?callable $callback, array $array, array ...$arrays = ?", "array" },` |
|         - |  440 | `	{ "array_merge", "array ...$arrays = ?", "array" },` |
|         - |  441 | `	{ "array_multisort", "&$array, &...$rest = ?", "true" },` |
|         - |  442 | `	{ "array_merge_recursive", "array ...$arrays = ?", "array" },` |
|         - |  443 | `	{ "array_pad", "array $array, int $length, mixed $value", "array" },` |
|         - |  444 | `	{ "array_pop", "array &$array", "mixed" },` |
|         - |  445 | `	{ "array_product", "array $array", "int\|float" },` |
|         - |  446 | `	{ "array_push", "array &$array, mixed ...$values = ?", "int" },` |
|         - |  447 | `	{ "array_rand", "array $array, int $num = 1", "array\|string\|int" },` |
|         - |  448 | `	{ "array_reduce", "array $array, callable $callback, mixed $initial = NULL", "mixed" },` |
|         - |  449 | `	{ "array_replace", "array $array, array ...$replacements = ?", "array" },` |
|         - |  450 | `	{ "array_reverse", "array $array, bool $preserve_keys = false", "array" },` |
|         - |  451 | `	{ "array_search", "mixed $needle, array $haystack, bool $strict = false", "string\|int\|false" },` |
|         - |  452 | `	{ "array_shift", "array &$array", "mixed" },` |
|         - |  453 | `	{ "array_slice", "array $array, int $offset, ?int $length = NULL, bool $preserve_keys = false", "array" },` |
|         - |  454 | `	{ "array_splice", "array &$array, int $offset, ?int $length = NULL, mixed $replacement = ?", "array" },` |
|         - |  455 | `	{ "array_sum", "array $array", "int\|float" },` |
|         - |  456 | `	{ "array_udiff", "array $array, ...$rest = ?", "array" },` |
|         - |  457 | `	{ "array_udiff_assoc", "array $array, ...$rest = ?", "array" },` |
|         - |  458 | `	{ "array_udiff_uassoc", "array $array, ...$rest = ?", "array" },` |
|         - |  459 | `	{ "array_uintersect", "array $array, ...$rest = ?", "array" },` |
|         - |  460 | `	{ "array_uintersect_assoc", "array $array, ...$rest = ?", "array" },` |
|         - |  461 | `	{ "array_uintersect_uassoc", "array $array, ...$rest = ?", "array" },` |
|         - |  462 | `	{ "array_unique", "array $array, int $flags = 2", "array" },` |
|         - |  463 | `	{ "array_unshift", "array &$array, mixed ...$values = ?", "int" },` |
|         - |  464 | `	{ "array_values", "array $array", "array" },` |
|         - |  465 | `	{ "array_walk", "object\|array &$array, callable $callback, mixed $arg = ?", "true" },` |
|         - |  466 | `	{ "array_walk_recursive", "object\|array &$array, callable $callback, mixed $arg = ?", "true" },` |
|         - |  467 | `	{ "arsort", "array &$array, int $flags = 0", "true" },` |
|         - |  468 | `	{ "asin", "float $num", "float" },` |
|         - |  469 | `	{ "asinh", "float $num", "float" },` |
|         - |  470 | `	{ "asort", "array &$array, int $flags = 0", "true" },` |
|         - |  471 | `	{ "assert", "mixed $assertion, Throwable\|string\|null $description = NULL", "bool" },` |
|         - |  472 | `	{ "atan", "float $num", "float" },` |
|         - |  473 | `	{ "atanh", "float $num", "float" },` |
|         - |  474 | `	{ "atan2", "float $y, float $x", "float" },` |
|         - |  475 | `	{ "base64_decode", "string $string, bool $strict = false", "string\|false" },` |
|         - |  476 | `	{ "base64_encode", "string $string", "string" },` |
|         - |  477 | `	{ "base_convert", "string $num, int $from_base, int $to_base", "string" },` |
|         - |  478 | `	{ "basename", "string $path, string $suffix = ''", "string" },` |
|         - |  479 | `	{ "bin2hex", "string $string", "string" },` |
|         - |  480 | `	{ "bindec", "string $binary_string", "int\|float" },` |
|         - |  481 | `	{ "boolval", "mixed $value", "bool" },` |
|         - |  482 | `	{ "call_user_func", "callable $callback, mixed ...$args = ?", "mixed" },` |
|         - |  483 | `	{ "call_user_func_array", "callable $callback, array $args", "mixed" },` |
|         - |  484 | `	{ "ceil", "int\|float $num", "float" },` |
|         - |  485 | `	{ "chdir", "string $directory", "bool" },` |
|         - |  486 | `	{ "chgrp", "string $filename, string\|int $group", "bool" },` |
|         - |  487 | `	{ "chmod", "string $filename, int $permissions", "bool" },` |
|         - |  488 | `	{ "chop", "string $string, string $characters = ?", "string" },` |
|         - |  489 | `	{ "chown", "string $filename, string\|int $user", "bool" },` |
|         - |  490 | `	{ "chr", "int $codepoint", "string" },` |
|         - |  491 | `	{ "chroot", "string $directory", "bool" },` |
|         - |  492 | `	{ "chunk_split", "string $string, int $length = 76, string $separator = ?", "string" },` |
|         - |  493 | `	{ "class_alias", "string $class, string $alias, bool $autoload = true", "bool" },` |
|         - |  494 | `	{ "class_exists", "string $class, bool $autoload = true", "bool" },` |
|         - |  495 | `	{ "enum_exists", "string $enum, bool $autoload = true", "bool" },` |
|         - |  496 | `	{ "clone", "object $object, array $withProperties = []", "object" },` |
|         - |  497 | `	{ "closedir", "$dir_handle = NULL", "void" },` |
|         - |  498 | `	{ "compact", "$var_name, ...$var_names = ?", "array" },` |
|         - |  499 | `	{ "constant", "string $name", "mixed" },` |
|         - |  500 | `	{ "convert_uudecode", "string $string", "string\|false" },` |
|         - |  501 | `	{ "convert_uuencode", "string $string", "string" },` |
|         - |  502 | `	{ "copy", "string $from, string $to, $context = NULL", "bool" },` |
|         - |  503 | `	{ "cos", "float $num", "float" },` |
|         - |  504 | `	{ "cosh", "float $num", "float" },` |
|         - |  505 | `	{ "count", "Countable\|array $value, int $mode = 0", "int" },` |
|         - |  506 | `	{ "count_chars", "string $string, int $mode = 0", "array\|string" },` |
|         - |  507 | `	{ "crc32", "string $string", "int" },` |
|         - |  508 | `	{ "ctype_alnum", "mixed $text", "bool" },` |
|         - |  509 | `	{ "ctype_alpha", "mixed $text", "bool" },` |
|         - |  510 | `	{ "ctype_cntrl", "mixed $text", "bool" },` |
|         - |  511 | `	{ "ctype_digit", "mixed $text", "bool" },` |
|         - |  512 | `	{ "ctype_graph", "mixed $text", "bool" },` |
|         - |  513 | `	{ "ctype_lower", "mixed $text", "bool" },` |
|         - |  514 | `	{ "ctype_print", "mixed $text", "bool" },` |
|         - |  515 | `	{ "ctype_punct", "mixed $text", "bool" },` |
|         - |  516 | `	{ "ctype_space", "mixed $text", "bool" },` |
|         - |  517 | `	{ "ctype_upper", "mixed $text", "bool" },` |
|         - |  518 | `	{ "ctype_xdigit", "mixed $text", "bool" },` |
|         - |  519 | `	{ "current", "object\|array $array", "mixed" },` |
|         - |  520 | `	{ "date", "string $format, ?int $timestamp = NULL", "string" },` |
|         - |  521 | `	{ "date_add", "DateTime $object, DateInterval $interval", "DateTime" },` |
|         - |  522 | `	{ "date_create", "string $datetime = 'now', ?DateTimeZone $timezone = NULL", "DateTime\|false" },` |
|         - |  523 | `	{ "date_create_from_format", "string $format, string $datetime, ?DateTimeZone $timezone = NULL", "DateTime\|false" },` |
|         - |  524 | `	{ "date_create_immutable", "string $datetime = 'now', ?DateTimeZone $timezone = NULL", "DateTimeImmutable\|false" },` |
|         - |  525 | `	{ "date_create_immutable_from_format", "string $format, string $datetime, ?DateTimeZone $timezone = NULL", "DateTimeImmutable\|false" },` |
|         - |  526 | `	{ "date_date_set", "DateTime $object, int $year, int $month, int $day", "DateTime" },` |
|         - |  527 | `	{ "date_diff", "DateTimeInterface $baseObject, DateTimeInterface $targetObject, bool $absolute = false", "DateInterval" },` |
|         - |  528 | `	{ "date_format", "DateTimeInterface $object, string $format", "string" },` |
|         - |  529 | `	{ "date_get_last_errors", "", "array\|false" },` |
|         - |  530 | `	{ "date_interval_create_from_date_string", "string $datetime", "DateInterval\|false" },` |
|         - |  531 | `	{ "date_interval_format", "DateInterval $object, string $format", "string" },` |
|         - |  532 | `	{ "date_isodate_set", "DateTime $object, int $year, int $week, int $dayOfWeek = 1", "DateTime" },` |
|         - |  533 | `	{ "date_modify", "DateTime $object, string $modifier", "DateTime\|false" },` |
|         - |  534 | `	{ "date_offset_get", "DateTimeInterface $object", "int" },` |
|         - |  535 | `	{ "date_sub", "DateTime $object, DateInterval $interval", "DateTime" },` |
|         - |  536 | `	{ "date_time_set", "DateTime $object, int $hour, int $minute, int $second = 0, int $microsecond = 0", "DateTime" },` |
|         - |  537 | `	{ "date_timestamp_get", "DateTimeInterface $object", "int" },` |
|         - |  538 | `	{ "date_timestamp_set", "DateTime $object, int $timestamp", "DateTime" },` |
|         - |  539 | `	{ "date_timezone_get", "DateTimeInterface $object", "DateTimeZone\|false" },` |
|         - |  540 | `	{ "date_timezone_set", "DateTime $object, DateTimeZone $timezone", "DateTime" },` |
|         - |  541 | `	{ "timezone_name_get", "DateTimeZone $object", "string" },` |
|         - |  542 | `	{ "timezone_offset_get", "DateTimeZone $object, DateTimeInterface $datetime", "int" },` |
|         - |  543 | `	{ "timezone_open", "string $timezone", "DateTimeZone\|false" },` |
|         - |  544 | `	{ "date_default_timezone_get", "", "string" },` |
|         - |  545 | `	{ "date_default_timezone_set", "string $timezoneId", "bool" },` |
|         - |  546 | `	{ "debug_backtrace", "int $options = 1, int $limit = 0", "array" },` |
|         - |  547 | `	{ "debug_print_backtrace", "int $options = 0, int $limit = 0", "void" },` |
|         - |  548 | `	{ "decbin", "int $num", "string" },` |
|         - |  549 | `	{ "dechex", "int $num", "string" },` |
|         - |  550 | `	{ "decoct", "int $num", "string" },` |
|         - |  551 | `	{ "define", "string $constant_name, mixed $value, bool $case_insensitive = false", "bool" },` |
|         - |  552 | `	{ "defined", "string $constant_name", "bool" },` |
|         - |  553 | `	{ "deg2rad", "float $num", "float" },` |
|         - |  554 | `	{ "die", "string\|int $status = 0", "never" },` |
|         - |  555 | `	{ "dir", "string $directory, $context = NULL", "Directory\|false" },` |
|         - |  556 | `	{ "dirname", "string $path, int $levels = 1", "string" },` |
|         - |  557 | `	{ "disk_free_space", "string $directory", "float\|false" },` |
|         - |  558 | `	{ "disk_total_space", "string $directory", "float\|false" },` |
|         - |  559 | `	{ "diskfreespace", "string $directory", "float\|false" },` |
|         - |  560 | `	{ "end", "object\|array &$array", "mixed" },` |
|         - |  561 | `	{ "error_get_last", "", "?array" },` |
|         - |  562 | `	{ "error_clear_last", "", "void" },` |
|         - |  563 | `	{ "error_log", "string $message, int $message_type = 0, ?string $destination = NULL, ?string $additional_headers = NULL", "bool" },` |
|         - |  564 | `	{ "error_reporting", "?int $error_level = NULL", "int" },` |
|         - |  565 | `	{ "escapeshellarg", "string $arg", "string" },` |
|         - |  566 | `	{ "escapeshellcmd", "string $command", "string" },` |
|         - |  567 | `	{ "exec", "string $command, &$output = NULL, &$result_code = NULL", "string\|false" },` |
|         - |  568 | `	{ "exit", "string\|int $status = 0", "never" },` |
|         - |  569 | `	{ "exp", "float $num", "float" },` |
|         - |  570 | `	{ "expm1", "float $num", "float" },` |
|         - |  571 | `	{ "explode", "string $separator, string $string, int $limit = 9223372036854775807", "array" },` |
|         - |  572 | `	{ "extension_loaded", "string $extension", "bool" },` |
|         - |  573 | `	{ "extract", "array &$array, int $flags = 0, string $prefix = ''", "int" },` |
|         - |  574 | `	{ "fclose", "$stream", "bool" },` |
|         - |  575 | `	{ "feof", "$stream", "bool" },` |
|         - |  576 | `	{ "fflush", "$stream", "bool" },` |
|         - |  577 | `	{ "fgetc", "$stream", "string\|false" },` |
|         - |  578 | `	{ "fgetcsv", "$stream, ?int $length = NULL, string $separator = ',', string $enclosure = '\"', string $escape = '\\\\'", "array\|false" },` |
|         - |  579 | `	{ "fgets", "$stream, ?int $length = NULL", "string\|false" },` |
|         - |  580 | `	{ "file", "string $filename, int $flags = 0, $context = NULL", "array\|false" },` |
|         - |  581 | `	{ "file_exists", "string $filename", "bool" },` |
|         - |  582 | `	{ "file_get_contents", "string $filename, bool $use_include_path = false, $context = NULL, int $offset = 0, ?int $length = NULL", "string\|false" },` |
|         - |  583 | `	{ "file_put_contents", "string $filename, mixed $data, int $flags = 0, $context = NULL", "int\|false" },` |
|         - |  584 | `	{ "fileatime", "string $filename", "int\|false" },` |
|         - |  585 | `	{ "filectime", "string $filename", "int\|false" },` |
|         - |  586 | `	{ "filegroup", "string $filename", "int\|false" },` |
|         - |  587 | `	{ "fileinode", "string $filename", "int\|false" },` |
|         - |  588 | `	{ "filemtime", "string $filename", "int\|false" },` |
|         - |  589 | `	{ "fileowner", "string $filename", "int\|false" },` |
|         - |  590 | `	{ "fileperms", "string $filename", "int\|false" },` |
|         - |  591 | `	{ "filesize", "string $filename", "int\|false" },` |
|         - |  592 | `	{ "filetype", "string $filename", "string\|false" },` |
|         - |  593 | `	{ "filter_has_var", "int $input_type, string $var_name", "bool" },` |
|         - |  594 | `	{ "filter_id", "string $name", "int\|false" },` |
|         - |  595 | `	{ "filter_input", "int $type, string $var_name, int $filter = 516, array\|int $options = 0", "mixed" },` |
|         - |  596 | `	{ "filter_input_array", "int $type, array\|int $options = 516, bool $add_empty = true", "array\|false\|null" },` |
|         - |  597 | `	{ "filter_list", "", "array" },` |
|         - |  598 | `	{ "filter_var", "mixed $value, int $filter = 516, array\|int $options = 0", "mixed" },` |
|         - |  599 | `	{ "filter_var_array", "array $array, array\|int $options = 516, bool $add_empty = true", "array\|false\|null" },` |
|         - |  600 | `	{ "floatval", "mixed $value", "float" },` |
|         - |  601 | `	{ "flock", "$stream, int $operation, &$would_block = NULL", "bool" },` |
|         - |  602 | `	{ "floor", "int\|float $num", "float" },` |
|         - |  603 | `	{ "flush", "", "void" },` |
|         - |  604 | `	{ "fmod", "float $num1, float $num2", "float" },` |
|         - |  605 | `	{ "fnmatch", "string $pattern, string $filename, int $flags = 0", "bool" },` |
|         - |  606 | `	{ "fopen", "string $filename, string $mode, bool $use_include_path = false, $context = NULL", "" },` |
|         - |  607 | `	{ "forward_static_call", "callable $callback, mixed ...$args = ?", "mixed" },` |
|         - |  608 | `	{ "forward_static_call_array", "callable $callback, array $args", "mixed" },` |
|         - |  609 | `	{ "fpow", "float $num, float $exponent", "float" },` |
|         - |  610 | `	{ "fpassthru", "$stream", "int" },` |
|         - |  611 | `	{ "fprintf", "$stream, string $format, mixed ...$values = ?", "int" },` |
|         - |  612 | `	{ "fputcsv", "$stream, array $fields, string $separator = ',', string $enclosure = '\"', string $escape = '\\\\', string $eol = ?", "int\|false" },` |
|         - |  613 | `	{ "fputs", "$stream, string $data, ?int $length = NULL", "int\|false" },` |
|         - |  614 | `	{ "fread", "$stream, int $length", "string\|false" },` |
|         - |  615 | `	{ "fseek", "$stream, int $offset, int $whence = 0", "int" },` |
|         - |  616 | `	{ "fstat", "$stream", "array\|false" },` |
|         - |  617 | `	{ "ftell", "$stream", "int\|false" },` |
|         - |  618 | `	{ "ftruncate", "$stream, int $size", "bool" },` |
|         - |  619 | `	{ "func_get_arg", "int $position", "mixed" },` |
|         - |  620 | `	{ "func_get_args", "", "array" },` |
|         - |  621 | `	{ "func_num_args", "", "int" },` |
|         - |  622 | `	{ "function_exists", "string $function", "bool" },` |
|         - |  623 | `	{ "fwrite", "$stream, string $data, ?int $length = NULL", "int\|false" },` |
|         - |  624 | `	{ "gc_collect_cycles", "", "int" },` |
|         - |  625 | `	{ "gc_disable", "", "void" },` |
|         - |  626 | `	{ "gc_enable", "", "void" },` |
|         - |  627 | `	{ "gc_enabled", "", "bool" },` |
|         - |  628 | `	{ "gc_mem_caches", "", "int" },` |
|         - |  629 | `	{ "gc_status", "", "array" },` |
|         - |  630 | `	{ "get_called_class", "", "string" },` |
|         - |  631 | `	{ "get_class", "object $object = ?", "string" },` |
|         - |  632 | `	{ "get_class_methods", "object\|string $object_or_class", "array" },` |
|         - |  633 | `	{ "get_class_vars", "string $class", "array" },` |
|         - |  634 | `	{ "get_current_user", "", "string" },` |
|         - |  635 | `	{ "get_declared_classes", "", "array" },` |
|         - |  636 | `	{ "get_declared_interfaces", "", "array" },` |
|         - |  637 | `	{ "get_declared_traits", "", "array" },` |
|         - |  638 | `	{ "get_defined_constants", "bool $categorize = false", "array" },` |
|         - |  639 | `	{ "get_defined_functions", "bool $exclude_disabled = true", "array" },` |
|         - |  640 | `	{ "get_defined_vars", "", "array" },` |
|         - |  641 | `	{ "get_html_translation_table", "int $table = 0, int $flags = 11, string $encoding = 'UTF-8'", "array" },` |
|         - |  642 | `	{ "get_include_path", "", "string\|false" },` |
|         - |  643 | `	{ "get_included_files", "", "array" },` |
|         - |  644 | `	{ "get_loaded_extensions", "bool $zend_extensions = false", "array" },` |
|         - |  645 | `	{ "get_mangled_object_vars", "object $object", "array" },` |
|         - |  646 | `	{ "get_object_vars", "object $object", "array" },` |
|         - |  647 | `	{ "get_parent_class", "object\|string $object_or_class = ?", "string\|false" },` |
|         - |  648 | `	{ "get_resource_id", "$resource", "int" },` |
|         - |  649 | `	{ "get_resource_type", "$resource", "string" },` |
|         - |  650 | `	{ "getcwd", "", "string\|false" },` |
|         - |  651 | `	{ "getdate", "?int $timestamp = NULL", "array" },` |
|         - |  652 | `	{ "getenv", "?string $name = NULL, bool $local_only = false", "array\|string\|false" },` |
|         - |  653 | `	{ "getmygid", "", "int\|false" },` |
|         - |  654 | `	{ "getmypid", "", "int\|false" },` |
|         - |  655 | `	{ "getmyuid", "", "int\|false" },` |
|         - |  656 | `	{ "getopt", "string $short_options, array $long_options = ?, &$rest_index = NULL", "array\|false" },` |
|         - |  657 | `	{ "getrandmax", "", "int" },` |
|         - |  658 | `	{ "gettimeofday", "bool $as_float = false", "array\|float" },` |
|         - |  659 | `	{ "gettype", "mixed $value", "string" },` |
|         - |  660 | `	{ "get_debug_type", "mixed $value", "string" },` |
|         - |  661 | `	{ "gmdate", "string $format, ?int $timestamp = NULL", "string" },` |
|         - |  662 | `	{ "gmmktime", "int $hour, ?int $minute = NULL, ?int $second = NULL, ?int $month = NULL, ?int $day = NULL, ?int $year = NULL", "int\|false" },` |
|         - |  663 | `	{ "hash", "string $algo, string $data, bool $binary = false, array $options = []", "string" },` |
|         - |  664 | `	{ "hash_algos", "", "array" },` |
|         - |  665 | `	{ "hash_equals", "string $known_string, string $user_string", "bool" },` |
|         - |  666 | `	{ "hash_hmac", "string $algo, string $data, string $key, bool $binary = false", "string" },` |
|         - |  667 | `	{ "hash_hmac_algos", "", "array" },` |
|         - |  668 | `	{ "hash_init", "string $algo, int $flags = 0, string $key = \'\', array $options = []", "HashContext" },` |
|         - |  669 | `	{ "hash_update", "HashContext $context, string $data", "bool" },` |
|         - |  670 | `	{ "hash_final", "HashContext $context, bool $binary = false", "string" },` |
|         - |  671 | `	{ "hash_copy", "HashContext $context", "HashContext" },` |
|         - |  672 | `	{ "hash_file", "string $algo, string $filename, bool $binary = false, array $options = []", "string\|false" },` |
|         - |  673 | `	{ "hash_hkdf", "string $algo, string $key, int $length = 0, string $info = \'\', string $salt = \'\'", "string" },` |
|         - |  674 | `	{ "hash_pbkdf2", "string $algo, string $password, string $salt, int $iterations, int $length = 0, bool $binary = false, array $options = []", "string" },` |
|         - |  675 | `	{ "hash_hmac_file", "string $algo, string $filename, string $key, bool $binary = false", "string\|false" },` |
|         - |  676 | `	{ "hash_update_file", "HashContext $context, string $filename, $stream_context = null", "bool" },` |
|         - |  677 | `	{ "hash_update_stream", "HashContext $context, $stream, int $length = -1", "int" },` |
|         - |  678 | `	{ "header", "string $header, bool $replace = true, int $response_code = 0", "void" },` |
|         - |  679 | `	{ "header_remove", "?string $name = NULL", "void" },` |
|         - |  680 | `	{ "headers_list", "", "array" },` |
|         - |  681 | `	{ "headers_sent", "&$filename = NULL, &$line = NULL", "bool" },` |
|         - |  682 | `	{ "hexdec", "string $hex_string", "int\|float" },` |
|         - |  683 | `	{ "html_entity_decode", "string $string, int $flags = 11, ?string $encoding = NULL", "string" },` |
|         - |  684 | `	{ "http_build_query", "object\|array $data, string $numeric_prefix = '', ?string $arg_separator = null, int $encoding_type = 1", "string" },` |
|         - |  685 | `	{ "htmlentities", "string $string, int $flags = 11, ?string $encoding = NULL, bool $double_encode = true", "string" },` |
|         - |  686 | `	{ "htmlspecialchars", "string $string, int $flags = 11, ?string $encoding = NULL, bool $double_encode = true", "string" },` |
|         - |  687 | `	{ "htmlspecialchars_decode", "string $string, int $flags = 11", "string" },` |
|         - |  688 | `	{ "http_response_code", "int $response_code = 0", "int\|bool" },` |
|         - |  689 | `	{ "hypot", "float $x, float $y", "float" },` |
|         - |  690 | `	{ "idate", "string $format, ?int $timestamp = NULL", "int\|false" },` |
|         - |  691 | `	{ "implode", "array\|string $separator, ?array $array = NULL", "string" },` |
|         - |  692 | `	{ "in_array", "mixed $needle, array $haystack, bool $strict = false", "bool" },` |
|         - |  693 | `	{ "intdiv", "int $num1, int $num2", "int" },` |
|         - |  694 | `	{ "interface_exists", "string $interface, bool $autoload = true", "bool" },` |
|         - |  695 | `	{ "trait_exists", "string $trait, bool $autoload = true", "bool" },` |
|         - |  696 | `	{ "intval", "mixed $value, int $base = 10", "int" },` |
|         - |  697 | `	{ "is_a", "mixed $object_or_class, string $class, bool $allow_string = false", "bool" },` |
|         - |  698 | `	{ "is_array", "mixed $value", "bool" },` |
|         - |  699 | `	{ "is_bool", "mixed $value", "bool" },` |
|         - |  700 | `	{ "is_callable", "mixed $value, bool $syntax_only = false, &$callable_name = NULL", "bool" },` |
|         - |  701 | `	{ "is_dir", "string $filename", "bool" },` |
|         - |  702 | `	{ "is_double", "mixed $value", "bool" },` |
|         - |  703 | `	{ "is_executable", "string $filename", "bool" },` |
|         - |  704 | `	{ "is_file", "string $filename", "bool" },` |
|         - |  705 | `	{ "is_float", "mixed $value", "bool" },` |
|         - |  706 | `	{ "is_int", "mixed $value", "bool" },` |
|         - |  707 | `	{ "is_integer", "mixed $value", "bool" },` |
|         - |  708 | `	{ "is_link", "string $filename", "bool" },` |
|         - |  709 | `	{ "is_long", "mixed $value", "bool" },` |
|         - |  710 | `	{ "is_null", "mixed $value", "bool" },` |
|         - |  711 | `	{ "is_numeric", "mixed $value", "bool" },` |
|         - |  712 | `	{ "is_object", "mixed $value", "bool" },` |
|         - |  713 | `	{ "is_readable", "string $filename", "bool" },` |
|         - |  714 | `	{ "is_resource", "mixed $value", "bool" },` |
|         - |  715 | `	{ "is_scalar", "mixed $value", "bool" },` |
|         - |  716 | `	{ "is_string", "mixed $value", "bool" },` |
|         - |  717 | `	{ "is_subclass_of", "mixed $object_or_class, string $class, bool $allow_string = true", "bool" },` |
|         - |  718 | `	{ "is_writable", "string $filename", "bool" },` |
|         - |  719 | `	{ "iterator_apply", "Traversable $iterator, callable $callback, ?array $args = NULL", "int" },` |
|         - |  720 | `	{ "iterator_count", "Traversable\|array $iterator", "int" },` |
|         - |  721 | `	{ "iterator_to_array", "Traversable\|array $iterator, bool $preserve_keys = true", "array" },` |
|         - |  722 | `	{ "join", "array\|string $separator, ?array $array = NULL", "string" },` |
|         - |  723 | `	{ "json_decode", "string $json, ?bool $associative = NULL, int $depth = 512, int $flags = 0", "mixed" },` |
|         - |  724 | `	{ "json_encode", "mixed $value, int $flags = 0, int $depth = 512", "string\|false" },` |
|         - |  725 | `	{ "json_last_error", "", "int" },` |
|         - |  726 | `	{ "json_last_error_msg", "", "string" },` |
|         - |  727 | `	{ "json_validate", "string $json, int $depth = 512, int $flags = 0", "bool" },` |
|         - |  728 | `	{ "key", "object\|array $array", "string\|int\|null" },` |
|         - |  729 | `	{ "key_exists", "$key, array $array", "bool" },` |
|         - |  730 | `	{ "krsort", "array &$array, int $flags = 0", "true" },` |
|         - |  731 | `	{ "ksort", "array &$array, int $flags = 0", "true" },` |
|         - |  732 | `	{ "lcfirst", "string $string", "string" },` |
|         - |  733 | `	{ "levenshtein", "string $string1, string $string2, int $insertion_cost = 1, int $replacement_cost = 1, int $deletion_cost = 1", "int" },` |
|         - |  734 | `	{ "link", "string $target, string $link", "bool" },` |
|         - |  735 | `	{ "localtime", "?int $timestamp = NULL, bool $associative = false", "array" },` |
|         - |  736 | `	{ "log", "float $num, float $base = 2.718281828459045", "float" },` |
|         - |  737 | `	{ "log10", "float $num", "float" },` |
|         - |  738 | `	{ "log1p", "float $num", "float" },` |
|         - |  739 | `	{ "lstat", "string $filename", "array\|false" },` |
|         - |  740 | `	{ "ltrim", "string $string, string $characters = ?", "string" },` |
|         - |  741 | `	{ "max", "mixed $value, mixed ...$values = ?", "mixed" },` |
|         - |  742 | `	{ "mb_chr", "int $codepoint, ?string $encoding = NULL", "string\|false" },` |
|         - |  743 | `	{ "mb_convert_encoding", "array\|string $string, string $to_encoding, array\|string\|null $from_encoding = NULL", "array\|string\|false" },` |
|         - |  744 | `	{ "mb_ord", "string $string, ?string $encoding = NULL", "int\|false" },` |
|         - |  745 | `	{ "mb_ltrim", "string $string, ?string $characters = null, ?string $encoding = null", "string" },` |
|         - |  746 | `	{ "mb_rtrim", "string $string, ?string $characters = null, ?string $encoding = null", "string" },` |
|         - |  747 | `	{ "mb_lcfirst", "string $string, ?string $encoding = null", "string" },` |
|         - |  748 | `	{ "mb_strtolower", "string $string, ?string $encoding = NULL", "string" },` |
|         - |  749 | `	{ "mb_strtoupper", "string $string, ?string $encoding = NULL", "string" },` |
|         - |  750 | `	{ "mb_trim", "string $string, ?string $characters = null, ?string $encoding = null", "string" },` |
|         - |  751 | `	{ "mb_ucfirst", "string $string, ?string $encoding = null", "string" },` |
|         - |  752 | `	{ "md5", "string $string, bool $binary = false", "string" },` |
|         - |  753 | `	{ "md5_file", "string $filename, bool $binary = false", "string\|false" },` |
|         - |  754 | `	{ "metaphone", "string $string, int $max_phonemes = 0", "string" },` |
|         - |  755 | `	{ "method_exists", "$object_or_class, string $method", "bool" },` |
|         - |  756 | `	{ "memory_get_peak_usage", "bool $real_usage = false", "int" },` |
|         - |  757 | `	{ "memory_get_usage", "bool $real_usage = false", "int" },` |
|         - |  758 | `	{ "microtime", "bool $as_float = false", "string\|float" },` |
|         - |  759 | `	{ "min", "mixed $value, mixed ...$values = ?", "mixed" },` |
|         - |  760 | `	{ "mkdir", "string $directory, int $permissions = 511, bool $recursive = false, $context = NULL", "bool" },` |
|         - |  761 | `	{ "mktime", "int $hour, ?int $minute = NULL, ?int $second = NULL, ?int $month = NULL, ?int $day = NULL, ?int $year = NULL", "int\|false" },` |
|         - |  762 | `	{ "mt_getrandmax", "", "int" },` |
|         - |  763 | `	{ "mt_rand", "int $min = ?, int $max = ?", "int" },` |
|         - |  764 | `	{ "mt_srand", "?int $seed = NULL, int $mode = 0", "void" },` |
|         - |  765 | `	{ "natcasesort", "array &$array", "true" },` |
|         - |  766 | `	{ "natsort", "array &$array", "true" },` |
|         - |  767 | `	{ "next", "object\|array &$array", "mixed" },` |
|         - |  768 | `	{ "nl2br", "string $string, bool $use_xhtml = true", "string" },` |
|         - |  769 | `	{ "number_format", "float $num, int $decimals = 0, ?string $decimal_separator = '.', ?string $thousands_separator = ','", "string" },` |
|         - |  770 | `	{ "ob_clean", "", "bool" },` |
|         - |  771 | `	{ "ob_end_clean", "", "bool" },` |
|         - |  772 | `	{ "ob_end_flush", "", "bool" },` |
|         - |  773 | `	{ "ob_flush", "", "bool" },` |
|         - |  774 | `	{ "ob_get_clean", "", "string\|false" },` |
|         - |  775 | `	{ "ob_get_contents", "", "string\|false" },` |
|         - |  776 | `	{ "ob_get_flush", "", "string\|false" },` |
|         - |  777 | `	{ "ob_get_length", "", "int\|false" },` |
|         - |  778 | `	{ "ob_get_level", "", "int" },` |
|         - |  779 | `	{ "ob_get_status", "bool $full_status = false", "array" },` |
|         - |  780 | `	{ "ob_implicit_flush", "bool $enable = true", "void" },` |
|         - |  781 | `	{ "ob_list_handlers", "", "array" },` |
|         - |  782 | `	{ "ob_start", "$callback = NULL, int $chunk_size = 0, int $flags = 112", "bool" },` |
|         - |  783 | `	{ "octdec", "string $octal_string", "int\|float" },` |
|         - |  784 | `	{ "opendir", "string $directory, $context = NULL", "" },` |
|         - |  785 | `	{ "ord", "string $character", "int" },` |
|         - |  786 | `	{ "pack", "string $format, mixed ...$values = ?", "string" },` |
|         - |  787 | `	{ "unpack", "string $format, string $string, int $offset = 0", "array\|false" },` |
|         - |  788 | `	{ "parse_ini_file", "string $filename, bool $process_sections = false, int $scanner_mode = 0", "array\|false" },` |
|         - |  789 | `	{ "parse_ini_string", "string $ini_string, bool $process_sections = false, int $scanner_mode = 0", "array\|false" },` |
|         - |  790 | `	{ "parse_str", "string $string, &$result", "void" },` |
|         - |  791 | `	{ "parse_url", "string $url, int $component = -1", "array\|string\|int\|false\|null" },` |
|         - |  792 | `	{ "crypt", "string $string, string $salt", "string" },` |
|         - |  793 | `	{ "password_algos", "", "array" },` |
|         - |  794 | `	{ "password_get_info", "string $hash", "array" },` |
|         - |  795 | `	{ "password_hash", "string $password, string\|int\|null $algo, array $options = ?", "string" },` |
|         - |  796 | `	{ "password_needs_rehash", "string $hash, string\|int\|null $algo, array $options = ?", "bool" },` |
|         - |  797 | `	{ "password_verify", "string $password, string $hash", "bool" },` |
|         - |  798 | `	{ "passthru", "string $command, &$result_code = NULL", "?false" },` |
|         - |  799 | `	{ "pathinfo", "string $path, int $flags = 15", "array\|string" },` |
|         - |  800 | `	{ "pclose", "$handle", "int" },` |
|         - |  801 | `	{ "php_sapi_name", "", "string\|false" },` |
|         - |  802 | `	{ "php_uname", "string $mode = 'a'", "string" },` |
|         - |  803 | `	{ "phpinfo", "int $flags = 4294967295", "true" },` |
|         - |  804 | `	{ "phpversion", "?string $extension = NULL", "string\|false" },` |
|         - |  805 | `	{ "pi", "", "float" },` |
|         - |  806 | `	{ "popen", "string $command, string $mode", "" },` |
|         - |  807 | `	{ "pos", "object\|array $array", "mixed" },` |
|         - |  808 | `	{ "pow", "mixed $num, mixed $exponent", "object\|int\|float" },` |
|         - |  809 | `	{ "preg_last_error", "", "int" },` |
|         - |  810 | `	{ "preg_last_error_msg", "", "string" },` |
|         - |  811 | `	{ "fsockopen", "string $hostname, int $port = -1, &$error_code = NULL, &$error_message = NULL, ?float $timeout = NULL", "" },` |
|         - |  812 | `	{ "pfsockopen", "string $hostname, int $port = -1, &$error_code = NULL, &$error_message = NULL, ?float $timeout = NULL", "" },` |
|         - |  813 | `	{ "stream_socket_client", "string $address, &$error_code = NULL, &$error_message = NULL, ?float $timeout = NULL, int $flags = 4, $context = NULL", "" },` |
|         - |  814 | `	{ "stream_socket_server", "string $address, &$error_code = NULL, &$error_message = NULL, int $flags = 12, $context = NULL", "" },` |
|         - |  815 | `	{ "stream_socket_accept", "$socket, ?float $timeout = NULL, &$peer_name = NULL", "" },` |
|         - |  816 | `	{ "stream_socket_get_name", "$socket, bool $remote", "string\|false" },` |
|         - |  817 | `	{ "stream_socket_pair", "int $domain, int $type, int $protocol", "array\|false" },` |
|         - |  818 | `	{ "stream_socket_shutdown", "$stream, int $mode", "bool" },` |
|         - |  819 | `	{ "stream_socket_recvfrom", "$socket, int $length, int $flags = 0, &$address = NULL", "string\|false" },` |
|         - |  820 | `	{ "stream_socket_sendto", "$socket, string $data, int $flags = 0, string $address = ''", "int\|false" },` |
|         - |  821 | `	{ "preg_match", "string $pattern, string $subject, &$matches = NULL, int $flags = 0, int $offset = 0", "int\|false" },` |
|         - |  822 | `	{ "preg_match_all", "string $pattern, string $subject, &$matches = NULL, int $flags = 0, int $offset = 0", "int\|false" },` |
|         - |  823 | `	{ "preg_quote", "string $str, ?string $delimiter = NULL", "string" },` |
|         - |  824 | `	{ "preg_replace", "array\|string $pattern, array\|string $replacement, array\|string $subject, int $limit = -1, &$count = NULL", "array\|string\|null" },` |
|         - |  825 | `	{ "preg_replace_callback", "array\|string $pattern, callable $callback, array\|string $subject, int $limit = -1, &$count = NULL, int $flags = 0", "array\|string\|null" },` |
|         - |  826 | `	{ "preg_split", "string $pattern, string $subject, int $limit = -1, int $flags = 0", "array\|false" },` |
|         - |  827 | `	{ "prev", "object\|array &$array", "mixed" },` |
|         - |  828 | `	{ "print_r", "mixed $value, bool $return = false", "string\|true" },` |
|         - |  829 | `	{ "printf", "string $format, mixed ...$values = ?", "int" },` |
|         - |  830 | `	{ "property_exists", "$object_or_class, string $property", "bool" },` |
|         - |  831 | `	{ "putenv", "string $assignment", "bool" },` |
|         - |  832 | `	{ "quotemeta", "string $string", "string" },` |
|         - |  833 | `	{ "rad2deg", "float $num", "float" },` |
|         - |  834 | `	{ "rand", "int $min = ?, int $max = ?", "int" },` |
|         - |  835 | `	{ "random_bytes", "int $length", "string" },` |
|         - |  836 | `	{ "random_int", "int $min, int $max", "int" },` |
|         - |  837 | `	{ "range", "string\|int\|float $start, string\|int\|float $end, int\|float $step = 1", "array" },` |
|         - |  838 | `	{ "rawurldecode", "string $string", "string" },` |
|         - |  839 | `	{ "rawurlencode", "string $string", "string" },` |
|         - |  840 | `	{ "readdir", "$dir_handle = NULL", "string\|false" },` |
|         - |  841 | `	{ "readfile", "string $filename, bool $use_include_path = false, $context = NULL", "int\|false" },` |
|         - |  842 | `	{ "readlink", "string $path", "string\|false" },` |
|         - |  843 | `	{ "realpath", "string $path", "string\|false" },` |
|         - |  844 | `	{ "stream_resolve_include_path", "string $filename", "string\|false" },` |
|         - |  845 | `	{ "register_shutdown_function", "callable $callback, mixed ...$args = ?", "void" },` |
|         - |  846 | `	{ "rename", "string $from, string $to, $context = NULL", "bool" },` |
|         - |  847 | `	{ "reset", "object\|array &$array", "mixed" },` |
|         - |  848 | `	{ "restore_error_handler", "", "true" },` |
|         - |  849 | `	{ "restore_exception_handler", "", "true" },` |
|         - |  850 | `	{ "rewind", "$stream", "bool" },` |
|         - |  851 | `	{ "rewinddir", "$dir_handle = NULL", "void" },` |
|         - |  852 | `	{ "rmdir", "string $directory, $context = NULL", "bool" },` |
|         - |  853 | `	{ "round", "int\|float $num, int $precision = 0, RoundingMode\|int $mode = ?", "float" },` |
|         - |  854 | `	{ "rsort", "array &$array, int $flags = 0", "true" },` |
|         - |  855 | `	{ "rtrim", "string $string, string $characters = ?", "string" },` |
|         - |  856 | `	{ "serialize", "mixed $value", "string" },` |
|         - |  857 | `	{ "set_error_handler", "?callable $callback, int $error_levels = 30719", "" },` |
|         - |  858 | `	{ "set_exception_handler", "?callable $callback", "" },` |
|         - |  859 | `	{ "get_error_handler", "", "?callable" },` |
|         - |  860 | `	{ "get_exception_handler", "", "?callable" },` |
|         - |  861 | `	{ "hrtime", "bool $as_number = false", "array\|int\|float\|false" },` |
|         - |  862 | `	{ "mb_check_encoding", "array\|string\|null $value = NULL, ?string $encoding = NULL", "bool" },` |
|         - |  863 | `	{ "mb_convert_case", "string $string, int $mode, ?string $encoding = NULL", "string" },` |
|         - |  864 | `	{ "mb_detect_encoding", "string $string, array\|string\|null $encodings = NULL, bool $strict = false", "string\|false" },` |
|         - |  865 | `	{ "mb_internal_encoding", "?string $encoding = NULL", "string\|bool" },` |
|         - |  866 | `	{ "mb_scrub", "string $string, ?string $encoding = null", "string" },` |
|         - |  867 | `	{ "mb_substitute_character", "string\|int\|null $substitute_character = null", "string\|int\|bool" },` |
|         - |  868 | `	{ "mb_str_split", "string $string, int $length = 1, ?string $encoding = NULL", "array" },` |
|         - |  869 | `	{ "mb_stripos", "string $haystack, string $needle, int $offset = 0, ?string $encoding = NULL", "int\|false" },` |
|         - |  870 | `	{ "mb_strlen", "string $string, ?string $encoding = NULL", "int" },` |
|         - |  871 | `	{ "mb_strpos", "string $haystack, string $needle, int $offset = 0, ?string $encoding = NULL", "int\|false" },` |
|         - |  872 | `	{ "mb_str_pad", "string $string, int $length, string $pad_string = \" \", int $pad_type = 1, ?string $encoding = null", "string" },` |
|         - |  873 | `	{ "mb_strcut", "string $string, int $start, ?int $length = null, ?string $encoding = null", "string" },` |
|         - |  874 | `	{ "mb_strimwidth", "string $string, int $start, int $width, string $trim_marker = \"\", ?string $encoding = null", "string" },` |
|         - |  875 | `	{ "mb_strrchr", "string $haystack, string $needle, bool $before_needle = false, ?string $encoding = null", "string\|false" },` |
|         - |  876 | `	{ "mb_strrichr", "string $haystack, string $needle, bool $before_needle = false, ?string $encoding = null", "string\|false" },` |
|         - |  877 | `	{ "mb_strripos", "string $haystack, string $needle, int $offset = 0, ?string $encoding = null", "int\|false" },` |
|         - |  878 | `	{ "mb_strrpos", "string $haystack, string $needle, int $offset = 0, ?string $encoding = NULL", "int\|false" },` |
|         - |  879 | `	{ "mb_stristr", "string $haystack, string $needle, bool $before_needle = false, ?string $encoding = null", "string\|false" },` |
|         - |  880 | `	{ "mb_strstr", "string $haystack, string $needle, bool $before_needle = false, ?string $encoding = null", "string\|false" },` |
|         - |  881 | `	{ "mb_substr_count", "string $haystack, string $needle, ?string $encoding = null", "int" },` |
|         - |  882 | `	{ "mb_strwidth", "string $string, ?string $encoding = NULL", "int" },` |
|         - |  883 | `	{ "mb_substr", "string $string, int $start, ?int $length = NULL, ?string $encoding = NULL", "string" },` |
|         - |  884 | `	{ "memory_reset_peak_usage", "", "void" },` |
|         - |  885 | `	{ "proc_close", "$process", "int" },` |
|         - |  886 | `	{ "proc_get_status", "$process", "array" },` |
|         - |  887 | `	{ "proc_nice", "int $priority", "bool" },` |
|         - |  888 | `	{ "proc_open", "array\|string $command, array $descriptor_spec, &$pipes, ?string $cwd = NULL, ?array $env_vars = NULL, ?array $options = NULL", "" },` |
|         - |  889 | `	{ "proc_terminate", "$process, int $signal = 15", "bool" },` |
|         - |  890 | `	{ "set_include_path", "string $include_path", "string\|false" },` |
|         - |  891 | `	{ "setcookie", "string $name, string $value = '', array\|int $expires_or_options = 0, string $path = '', string $domain = '', bool $secure = false, bool $httponly = false", "bool" },` |
|         - |  892 | `	{ "setrawcookie", "string $name, string $value = '', array\|int $expires_or_options = 0, string $path = '', string $domain = '', bool $secure = false, bool $httponly = false", "bool" },` |
|         - |  893 | `	{ "settype", "mixed &$var, string $type", "bool" },` |
|         - |  894 | `	{ "sha1", "string $string, bool $binary = false", "string" },` |
|         - |  895 | `	{ "sha1_file", "string $filename, bool $binary = false", "string\|false" },` |
|         - |  896 | `	{ "shell_exec", "string $command", "string\|false\|null" },` |
|         - |  897 | `	{ "shuffle", "array &$array", "true" },` |
|         - |  898 | `	{ "similar_text", "string $string1, string $string2, &$percent = NULL", "int" },` |
|         - |  899 | `	{ "sin", "float $num", "float" },` |
|         - |  900 | `	{ "sinh", "float $num", "float" },` |
|         - |  901 | `	{ "sizeof", "Countable\|array $value, int $mode = 0", "int" },` |
|         - |  902 | `	{ "sleep", "int $seconds", "int" },` |
|         - |  903 | `	{ "sort", "array &$array, int $flags = 0", "true" },` |
|         - |  904 | `	{ "soundex", "string $string", "string" },` |
|         - |  905 | `	{ "spl_autoload", "string $class, ?string $file_extensions = NULL", "void" },` |
|         - |  906 | `	{ "spl_autoload_functions", "", "array" },` |
|         - |  907 | `	{ "spl_autoload_register", "?callable $callback = NULL, bool $throw = true, bool $prepend = false", "bool" },` |
|         - |  908 | `	{ "spl_autoload_unregister", "callable $callback", "bool" },` |
|         - |  909 | `	{ "spl_object_hash", "object $object", "string" },` |
|         - |  910 | `	{ "spl_object_id", "object $object", "int" },` |
|         - |  911 | `	{ "sprintf", "string $format, mixed ...$values = ?", "string" },` |
|         - |  912 | `	{ "sqrt", "float $num", "float" },` |
|         - |  913 | `	{ "srand", "?int $seed = NULL, int $mode = 0", "void" },` |
|         - |  914 | `	{ "stat", "string $filename", "array\|false" },` |
|         - |  915 | `	{ "str_contains", "string $haystack, string $needle", "bool" },` |
|         - |  916 | `	{ "str_ends_with", "string $haystack, string $needle", "bool" },` |
|         - |  917 | `	{ "str_getcsv", "string $string, string $separator = ',', string $enclosure = '\"', string $escape = '\\\\'", "array" },` |
|         - |  918 | `	{ "str_ireplace", "array\|string $search, array\|string $replace, array\|string $subject, &$count = NULL", "array\|string" },` |
|         - |  919 | `	{ "str_pad", "string $string, int $length, string $pad_string = ' ', int $pad_type = 1", "string" },` |
|         - |  920 | `	{ "str_repeat", "string $string, int $times", "string" },` |
|         - |  921 | `	{ "str_replace", "array\|string $search, array\|string $replace, array\|string $subject, &$count = NULL", "array\|string" },` |
|         - |  922 | `	{ "str_rot13", "string $string", "string" },` |
|         - |  923 | `	{ "str_shuffle", "string $string", "string" },` |
|         - |  924 | `	{ "str_split", "string $string, int $length = 1", "array" },` |
|         - |  925 | `	{ "str_starts_with", "string $haystack, string $needle", "bool" },` |
|         - |  926 | `	{ "str_word_count", "string $string, int $format = 0, ?string $characters = NULL", "array\|int" },` |
|         - |  927 | `	{ "strcasecmp", "string $string1, string $string2", "int" },` |
|         - |  928 | `	{ "strchr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|         - |  929 | `	{ "strcmp", "string $string1, string $string2", "int" },` |
|         - |  930 | `	{ "strnatcasecmp", "string $string1, string $string2", "int" },` |
|         - |  931 | `	{ "strnatcmp", "string $string1, string $string2", "int" },` |
|         - |  932 | `	{ "strcoll", "string $string1, string $string2", "int" },` |
|         - |  933 | `	{ "strcspn", "string $string, string $characters, int $offset = 0, ?int $length = NULL", "int" },` |
|         - |  934 | `	{ "stream_context_create", "?array $options = NULL, ?array $params = NULL", "" },` |
|         - |  935 | `	{ "stream_context_get_options", "$stream_or_context", "array" },` |
|         - |  936 | ``	/* php's argument #2 is `array\|string $wrapper_or_options` and the array form`` |
|         - |  937 | `	 * — the two-argument spelling — is DEPRECATED in 8.3; §10 refuses what php` |
|         - |  938 | `	 * deprecates, so this row declares the string and the whole-array form is` |
|         - |  939 | `	 * spelled stream_context_set_options(). */` |
|         - |  940 | `	{ "stream_context_set_option", "$context, string $wrapper_name, string $option_name, mixed $value", "bool" },` |
|         - |  941 | `	{ "stream_context_set_options", "$context, array $options", "bool" },` |
|         - |  942 | `	{ "stream_context_get_params", "$stream_or_context", "array" },` |
|         - |  943 | `	{ "stream_context_set_params", "$context, array $params", "bool" },` |
|         - |  944 | `	{ "stream_context_get_default", "?array $options = NULL", "" },` |
|         - |  945 | `	{ "stream_context_set_default", "array $options", "" },` |
|         - |  946 | `	{ "stream_get_contents", "$stream, ?int $length = NULL, int $offset = -1", "string\|false" },` |
|         - |  947 | `	{ "stream_get_line", "$stream, int $length, string $ending = ''", "string\|false" },` |
|         - |  948 | `	{ "socket_get_status", "$stream", "array" },` |
|         - |  949 | `	{ "stream_get_meta_data", "$stream", "array" },` |
|         - |  950 | `	{ "stream_copy_to_stream", "$from, $to, ?int $length = NULL, int $offset = 0", "int\|false" },` |
|         - |  951 | `	{ "stream_get_transports", "", "array" },` |
|         - |  952 | `	{ "stream_is_local", "$stream", "bool" },` |
|         - |  953 | `	{ "stream_select", "?array &$read, ?array &$write, ?array &$except, ?int $seconds, ?int $microseconds = NULL", "int\|false" },` |
|         - |  954 | `	{ "stream_set_blocking", "$stream, bool $enable", "bool" },` |
|         - |  955 | `	{ "socket_set_blocking", "$stream, bool $enable", "bool" },` |
|         - |  956 | `	{ "stream_set_chunk_size", "$stream, int $size", "int" },` |
|         - |  957 | `	{ "stream_set_read_buffer", "$stream, int $size", "int" },` |
|         - |  958 | `	{ "stream_set_timeout", "$stream, int $seconds, int $microseconds = 0", "bool" },` |
|         - |  959 | `	{ "stream_set_write_buffer", "$stream, int $size", "int" },` |
|         - |  960 | `	{ "set_file_buffer", "$stream, int $size", "int" },` |
|         - |  961 | `	{ "stream_supports_lock", "$stream", "bool" },` |
|         - |  962 | `	{ "stream_get_wrappers", "", "array" },` |
|         - |  963 | `	{ "stream_get_filters", "", "array" },` |
|         - |  964 | `	{ "stream_filter_append", "$stream, string $filter_name, int $mode = 0, mixed $params = NULL", "" },` |
|         - |  965 | `	{ "stream_filter_prepend", "$stream, string $filter_name, int $mode = 0, mixed $params = NULL", "" },` |
|         - |  966 | `	{ "stream_filter_remove", "$stream_filter", "bool" },` |
|         - |  967 | `	{ "stream_filter_register", "string $filter_name, string $class", "bool" },` |
|         - |  968 | `	{ "stream_bucket_make_writeable", "$brigade", "?StreamBucket" },` |
|         - |  969 | `	{ "stream_bucket_append", "$brigade, StreamBucket $bucket", "void" },` |
|         - |  970 | `	{ "stream_bucket_prepend", "$brigade, StreamBucket $bucket", "void" },` |
|         - |  971 | `	{ "stream_bucket_new", "$stream, string $buffer", "StreamBucket" },` |
|         - |  972 | `	{ "stream_register_wrapper", "string $protocol, string $class, int $flags = 0", "bool" },` |
|         - |  973 | `	{ "stream_wrapper_register", "string $protocol, string $class, int $flags = 0", "bool" },` |
|         - |  974 | `	{ "stream_wrapper_unregister", "string $protocol", "bool" },` |
|         - |  975 | `	{ "stream_wrapper_restore", "string $protocol", "bool" },` |
|         - |  976 | `	{ "strip_tags", "string $string, array\|string\|null $allowed_tags = NULL", "string" },` |
|         - |  977 | `	{ "stripos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|         - |  978 | `	{ "stripslashes", "string $string", "string" },` |
|         - |  979 | `	{ "stristr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|         - |  980 | `	{ "strlen", "string $string", "int" },` |
|         - |  981 | `	{ "strncasecmp", "string $string1, string $string2, int $length", "int" },` |
|         - |  982 | `	{ "strncmp", "string $string1, string $string2, int $length", "int" },` |
|         - |  983 | `	{ "strpbrk", "string $string, string $characters", "string\|false" },` |
|         - |  984 | `	{ "strpos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|         - |  985 | `	{ "strrchr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|         - |  986 | `	{ "strrev", "string $string", "string" },` |
|         - |  987 | `	{ "strripos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|         - |  988 | `	{ "strrpos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|         - |  989 | `	{ "strspn", "string $string, string $characters, int $offset = 0, ?int $length = NULL", "int" },` |
|         - |  990 | `	{ "strstr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|         - |  991 | `	{ "strtok", "string $string, ?string $token = NULL", "string\|false" },` |
|         - |  992 | `	{ "strtolower", "string $string", "string" },` |
|         - |  993 | `	{ "strtotime", "string $datetime, ?int $baseTimestamp = NULL", "int\|false" },` |
|         - |  994 | `	{ "strtoupper", "string $string", "string" },` |
|         - |  995 | `	{ "strtr", "string $string, array\|string $from, ?string $to = NULL", "string" },` |
|         - |  996 | `	{ "strval", "mixed $value", "string" },` |
|         - |  997 | `	{ "substr", "string $string, int $offset, ?int $length = NULL", "string" },` |
|         - |  998 | `	{ "substr_compare", "string $haystack, string $needle, int $offset, ?int $length = NULL, bool $case_insensitive = false", "int" },` |
|         - |  999 | `	{ "substr_count", "string $haystack, string $needle, int $offset = 0, ?int $length = NULL", "int" },` |
|         - | 1000 | `	{ "substr_replace", "array\|string $string, array\|string $replace, array\|int $offset, array\|int\|null $length = NULL", "array\|string" },` |
|         - | 1001 | `	{ "symlink", "string $target, string $link", "bool" },` |
|         - | 1002 | `	{ "sys_get_temp_dir", "", "string" },` |
|         - | 1003 | `	{ "system", "string $command, &$result_code = NULL", "string\|false" },` |
|         - | 1004 | `	{ "tan", "float $num", "float" },` |
|         - | 1005 | `	{ "tanh", "float $num", "float" },` |
|         - | 1006 | `	{ "time", "", "int" },` |
|         - | 1007 | `	{ "token_get_all", "string $code, int $flags = 0", "array" },` |
|         - | 1008 | `	{ "token_name", "int $id", "string" },` |
|         - | 1009 | `	{ "touch", "string $filename, ?int $mtime = NULL, ?int $atime = NULL", "bool" },` |
|         - | 1010 | `	{ "trigger_error", "string $message, int $error_level = 1024", "true" },` |
|         - | 1011 | `	{ "trim", "string $string, string $characters = ?", "string" },` |
|         - | 1012 | `	{ "uasort", "array &$array, callable $callback", "true" },` |
|         - | 1013 | `	{ "ucfirst", "string $string", "string" },` |
|         - | 1014 | `	{ "ucwords", "string $string, string $separators = ?", "string" },` |
|         - | 1015 | `	{ "uksort", "array &$array, callable $callback", "true" },` |
|         - | 1016 | `	{ "umask", "?int $mask = NULL", "int" },` |
|         - | 1017 | `	{ "uniqid", "string $prefix = '', bool $more_entropy = false", "string" },` |
|         - | 1018 | `	{ "unlink", "string $filename, $context = NULL", "bool" },` |
|         - | 1019 | `	{ "unserialize", "string $data, array $options = ?", "mixed" },` |
|         - | 1020 | `	{ "urldecode", "string $string", "string" },` |
|         - | 1021 | `	{ "urlencode", "string $string", "string" },` |
|         - | 1022 | `	{ "user_error", "string $message, int $error_level = 1024", "true" },` |
|         - | 1023 | `	{ "usleep", "int $microseconds", "void" },` |
|         - | 1024 | `	{ "usort", "array &$array, callable $callback", "true" },` |
|         - | 1025 | `	{ "var_dump", "mixed $value, mixed ...$values = ?", "void" },` |
|         - | 1026 | `	{ "var_export", "mixed $value, bool $return = false", "?string" },` |
|         - | 1027 | `	{ "version_compare", "string $version1, string $version2, ?string $operator = null", "int\|bool" },` |
|         - | 1028 | `	{ "vfprintf", "$stream, string $format, array $values", "int" },` |
|         - | 1029 | `	{ "vprintf", "string $format, array $values", "int" },` |
|         - | 1030 | `	{ "vsprintf", "string $format, array $values", "string" },` |
|         - | 1031 | `	{ "wordwrap", "string $string, int $width = 75, string $break = ?, bool $cut_long_words = false", "string" },` |
|         - | 1032 | `	{ "zip_close", "$zip", "void" },` |
|         - | 1033 | `	{ "zip_entry_close", "$zip_entry", "bool" },` |
|         - | 1034 | `	{ "zip_entry_compressedsize", "$zip_entry", "int\|false" },` |
|         - | 1035 | `	{ "zip_entry_compressionmethod", "$zip_entry", "string\|false" },` |
|         - | 1036 | `	{ "zip_entry_filesize", "$zip_entry", "int\|false" },` |
|         - | 1037 | `	{ "zip_entry_name", "$zip_entry", "string\|false" },` |
|         - | 1038 | `	{ "zip_entry_open", "$zip_dp, $zip_entry, string $mode = 'rb'", "bool" },` |
|         - | 1039 | `	{ "zip_entry_read", "$zip_entry, int $len = 1024", "string\|false" },` |
|         - | 1040 | `	{ "zip_open", "string $filename", "" },` |
|         - | 1041 | `	{ "zip_read", "$zip", "" },` |
|         - | 1042 | `};` |
|         - | 1043 | `/*` |
|         - | 1044 | ` * Stamp the signature strings onto the registered host functions.` |
|         - | 1045 | ` * Runs once at PH7_VmMakeReady, after the builtins are registered.` |
|         - | 1046 | ` */` |
|         - | 1047 | `/*` |
|         - | 1048 | ` * Derive the minimum arity from a builtin's declared signature: a parameter is` |
|         - | 1049 | ` * optional when it carries a default ("int $length = 76") or is variadic` |
|         - | 1050 | ` * ("...$rest"), so the minimum is the count of the parameters that are neither,` |
|         - | 1051 | ` * and the ZPP wording is "at least" as soon as one optional/variadic exists.` |
|         - | 1052 | ` *` |
|         - | 1053 | ` * This makes aBuiltinSig[] the single source of truth for arity — the curated` |
|         - | 1054 | ` * aBuiltinArity[] table above stays as an OVERRIDE for the handful of builtins` |
|         - | 1055 | ` * php reports differently from their own signature (e.g. strtr(), whose 3-arg` |
|         - | 1056 | ` * form still reports "expects exactly 2", and the array_udiff() family, whose` |
|         - | 1057 | ` * variadic tail hides a second required argument). Verified against php 8.5.7` |
|         - | 1058 | ` * for all 462 signed builtins: 458 derive exactly, 4 are overridden.` |
|         - | 1059 | ` */` |
|         - | 1060 | `/*` |
|         - | 1061 | ` * A DEFAULT can contain the parameter separator: php declares` |
|         - | 1062 | `` * `string $separator = ','` and `string $enclosure = '"'`. Every scan of a`` |
|         - | 1063 | ` * signature therefore has to step over a quoted run, or the comma inside one` |
|         - | 1064 | ` * splits the parameter in two — which is how fgetcsv()/fputcsv()/str_getcsv()` |
|         - | 1065 | ` * came to count SIX parameters and accept a fifth argument php refuses.` |
|         - | 1066 | ` * Answers the position of the closing quote (or of the NUL when the run is` |
|         - | 1067 | ` * unterminated); the caller advances past it.` |
|         - | 1068 | ` */` |
|   1900740 | 1069 | `static const char *VmSigSkipQuoted(const char *zCur)` |
|         5 | 1070 | `{` |
|   1900745 | 1071 | `	char c = zCur[0];` |
|   1900745 | 1072 | `	if( c != '\'' && c != '"' ){` |
|       ! 0 | 1073 | `		return zCur;` |
|         - | 1074 | `	}` |
|   2480493 | 1075 | `	for( zCur++ ; zCur[0] ; zCur++ ){` |
|   2480493 | 1076 | `		if( zCur[0] == '\\' && zCur[1] ){` |
|     27585 | 1077 | `			zCur++;` |
|     27585 | 1078 | `			continue;` |
|         - | 1079 | `		}` |
|   2452913 | 1080 | `		if( zCur[0] == c ){` |
|   1900745 | 1081 | `			break;` |
|         - | 1082 | `		}` |
|    276089 | 1083 | `	}` |
|   1900745 | 1084 | `	return zCur;` |
|    950375 | 1085 | `}` |
|   6898460 | 1086 | `PH7_PRIVATE void VmDeriveArityFromSig(const char *zSig,sxi16 *pnMin,sxu8 *pbAtLeast,sxi16 *pnMax,sxu8 *pbHasMax)` |
|         5 | 1087 | `{` |
|   6898465 | 1088 | `	const char *zCur = zSig;` |
|   6898465 | 1089 | `	int nMin = 0, bAtLeast = 0, bSeen = 0, bOptional = 0;` |
|   6898465 | 1090 | `	int nTotal = 0, bVariadic = 0;` |
|  66644242 | 1091 | `	for(;;){` |
| 136941999 | 1092 | `		if( zCur[0] == '\'' \|\| zCur[0] == '"' ){` |
|    221671 | 1093 | `			bSeen = 1;` |
|    221671 | 1094 | `			zCur = VmSigSkipQuoted(zCur);` |
|    221671 | 1095 | `			if( zCur[0] != '\0' ){` |
|    221671 | 1096 | `				zCur++;` |
|    110833 | 1097 | `			}` |
|    221671 | 1098 | `			continue;` |
|         - | 1099 | `		}` |
| 136720333 | 1100 | `		if( zCur[0] == '\0' \|\| zCur[0] == ',' ){` |
|  10330309 | 1101 | `			if( bSeen ){` |
|   7589589 | 1102 | `				nTotal++;` |
|   7589589 | 1103 | `				if( bOptional ){` |
|   2768445 | 1104 | `					bAtLeast = 1;` |
|   1384225 | 1105 | `				}else{` |
|   4821149 | 1106 | `					nMin++;` |
|         - | 1107 | `				}` |
|   3794792 | 1108 | `			}` |
|  10330309 | 1109 | `			if( zCur[0] == '\0' ){` |
|   6898465 | 1110 | `				break;` |
|         - | 1111 | `			}` |
|   3431849 | 1112 | `			bSeen = bOptional = 0;` |
|   3431849 | 1113 | `			zCur++;` |
|   3431849 | 1114 | `			continue;` |
|         - | 1115 | `		}` |
| 126390029 | 1116 | `		if( zCur[0] != ' ' ){` |
| 110555101 | 1117 | `			bSeen = 1;` |
|  55277548 | 1118 | `		}` |
| 126390029 | 1119 | `		if( zCur[0] == '=' \|\| (zCur[0] == '.' && zCur[1] == '.' && zCur[2] == '.') ){` |
|   2927765 | 1120 | `			bOptional = 1;` |
|   1463880 | 1121 | `		}` |
| 126390029 | 1122 | `		if( zCur[0] == '.' && zCur[1] == '.' && zCur[2] == '.' ){` |
|    185055 | 1123 | `			bVariadic = 1;` |
|     92525 | 1124 | `		}` |
| 126390029 | 1125 | `		zCur++;` |
|         5 | 1126 | `	}` |
|   6898465 | 1127 | `	*pnMin = (sxi16)nMin;` |
|   6898465 | 1128 | `	*pbAtLeast = (sxu8)bAtLeast;` |
|         - | 1129 | `	/* php enforces a MAXIMUM too ("expects at most 1 argument, 2 given"); a` |
|         - | 1130 | `	 * variadic tail means there is none. The parameter COUNT is the maximum,` |
|         - | 1131 | `	 * whether or not the parameters carry defaults. */` |
|   6898465 | 1132 | `	*pnMax = (sxi16)nTotal;` |
|   6898465 | 1133 | `	*pbHasMax = (sxu8)(bVariadic ? 0 : 1);` |
|   6898465 | 1134 | `}` |
|         - | 1135 | `/*` |
|         - | 1136 | ` * Does the declared type list (e.g. "array\|string", "?int", "callable") contain` |
|         - | 1137 | ` * the given token? Compares against each '\|'-separated alternative, ignoring a` |
|         - | 1138 | ` * leading nullable '?'.` |
|         - | 1139 | ` */` |
|  11951868 | 1140 | `static int VmSigTypeHas(const char *zType,int nType,const char *zTok)` |
|         5 | 1141 | `{` |
|  11951873 | 1142 | `	int nTok = (int)SyStrlen(zTok);` |
|  11951873 | 1143 | `	int i = 0;` |
|  11951873 | 1144 | `	if( zType[0] == '?' ){` |
|    682237 | 1145 | `		zType++;` |
|    682237 | 1146 | `		nType--;` |
|    341116 | 1147 | `	}` |
|  24409678 | 1148 | `	while( i < nType ){` |
|  12643211 | 1149 | `		int j = i;` |
|  82898997 | 1150 | `		while( j < nType && zType[j] != '\|' ){` |
|  70255791 | 1151 | `			j++;` |
|         5 | 1152 | `		}` |
|  12643211 | 1153 | `		if( j - i == nTok && SyMemcmp(&zType[i],zTok,(sxu32)nTok) == 0 ){` |
|    185406 | 1154 | `			return 1;` |
|         - | 1155 | `		}` |
|  12457810 | 1156 | `		i = j + 1;` |
|         5 | 1157 | `	}` |
|  11766472 | 1158 | `	return 0;` |
|   5979192 | 1159 | `}` |
|         - | 1160 | `/*` |
|         - | 1161 | `` * Is EVERY arm of the declared type list `array` (a bare `array`, or `?array`,`` |
|         - | 1162 | `` * or the `array\|null` union that spells the same thing)? Such a parameter has`` |
|         - | 1163 | ` * no arm a scalar can satisfy, and php refuses one outright.` |
|         - | 1164 | ` *` |
|         - | 1165 | `` * The screen used to exempt any type list carrying an `array` arm, union or`` |
|         - | 1166 | `` * not, for a wording reason: php's `array\|object` parameters come from ONE ZPP`` |
|         - | 1167 | ` * macro (Z_PARAM_ARRAY_OR_OBJECT) that names only "array" in the refusal, so` |
|         - | 1168 | ` * the declared type is not the text php prints. That ambiguity does not exist` |
|         - | 1169 | `` * for a parameter typed exactly `array` -- there is one arm and php prints it.`` |
|         - | 1170 | ` */` |
|   3324309 | 1171 | `static int VmSigTypeIsArrayOnly(const char *zType,int nType)` |
|         5 | 1172 | `{` |
|   3324314 | 1173 | `	int i = 0, bArray = 0;` |
|   3324314 | 1174 | `	if( zType[0] == '?' ){` |
|    323569 | 1175 | `		zType++;` |
|    323569 | 1176 | `		nType--;` |
|    161782 | 1177 | `	}` |
|   3493358 | 1178 | `	while( i < nType ){` |
|   3493132 | 1179 | `		int j = i;` |
|  21875449 | 1180 | `		while( j < nType && zType[j] != '\|' ){` |
|  18382322 | 1181 | `			j++;` |
|         5 | 1182 | `		}` |
|   3493132 | 1183 | `		if( j > i ){` |
|   3493127 | 1184 | `			if( j - i == (int)sizeof("array")-1` |
|   1832180 | 1185 | `			 && SyMemcmp(&zType[i],"array",sizeof("array")-1) == 0 ){` |
|    169049 | 1186 | `				bArray = 1;` |
|   3412513 | 1187 | `			}else if( !(j - i == (int)sizeof("null")-1` |
|   1666827 | 1188 | `			         && SyMemcmp(&zType[i],"null",sizeof("null")-1) == 0) ){` |
|   3324088 | 1189 | `				return 0;` |
|         - | 1190 | `			}` |
|     84522 | 1191 | `		}` |
|    169049 | 1192 | `		i = j + 1;` |
|         5 | 1193 | `	}` |
|       231 | 1194 | `	return bArray;` |
|   1663042 | 1195 | `}` |
|         - | 1196 | `/*` |
|         - | 1197 | ` * Does the declared type list name a CLASS (anything that is not one of php's` |
|         - | 1198 | ` * builtin type keywords)? A class-typed parameter accepts an object, so it must` |
|         - | 1199 | ` * not be rejected by the array/object/resource screen below.` |
|         - | 1200 | ` */` |
|         - | 1201 | `/* Is this one arm of a declared type a BUILTIN type name rather than a class? */` |
|   3512063 | 1202 | `static int VmSigArmIsBuiltinType(const char *zArm,int nArm)` |
|         5 | 1203 | `{` |
|         - | 1204 | `	static const char *azBuiltin[] = {` |
|         - | 1205 | `		"int","float","string","bool","array","object","callable","iterable",` |
|         - | 1206 | `		"mixed","null","void","resource","false","true","never","self","static"` |
|         - | 1207 | `	};` |
|         - | 1208 | `	int k;` |
|   9428762 | 1209 | `	for( k = 0 ; k < (int)SX_ARRAYSIZE(azBuiltin) ; ++k ){` |
|   9421950 | 1210 | `		int nB = (int)SyStrlen(azBuiltin[k]);` |
|   9421950 | 1211 | `		if( nArm == nB && SyMemcmp(zArm,azBuiltin[k],(sxu32)nB) == 0 ){` |
|   3505256 | 1212 | `			return 1;` |
|         - | 1213 | `		}` |
|   2959674 | 1214 | `	}` |
|      6817 | 1215 | `	return 0;` |
|   1756919 | 1216 | `}` |
|   3336981 | 1217 | `static int VmSigTypeHasClass(const char *zType,int nType)` |
|         5 | 1218 | `{` |
|   3336986 | 1219 | `	int i = 0;` |
|   3336986 | 1220 | `	if( zType[0] == '?' ){` |
|    325377 | 1221 | `		zType++;` |
|    325377 | 1222 | `		nType--;` |
|    162686 | 1223 | `	}` |
|   6842235 | 1224 | `	while( i < nType ){` |
|   3509962 | 1225 | `		int j = i;` |
|  22009437 | 1226 | `		while( j < nType && zType[j] != '\|' ){` |
|  18499480 | 1227 | `			j++;` |
|         5 | 1228 | `		}` |
|   3509962 | 1229 | `		if( j > i && !VmSigArmIsBuiltinType(&zType[i],j - i) ){` |
|      4713 | 1230 | `			return 1;` |
|         - | 1231 | `		}` |
|   3505254 | 1232 | `		i = j + 1;` |
|         5 | 1233 | `	}` |
|   3332278 | 1234 | `	return 0;` |
|   1669378 | 1235 | `}` |
|         - | 1236 | `/*` |
|         - | 1237 | ` * php's ", X given" tail: like ph7_type_name() but an object reports its CLASS,` |
|         - | 1238 | ` * which is what php prints in a TypeError.` |
|         - | 1239 | ` */` |
|         - | 1240 | `/*` |
|         - | 1241 | ` * Does pObj satisfy any CLASS arm of a declared type?` |
|         - | 1242 | ` *` |
|         - | 1243 | ` * Answers TRUE (unscreened) when an arm names something this VM has not declared:` |
|         - | 1244 | ` * the signatures describe php's surface, parts of which PHL models differently` |
|         - | 1245 | ` * (the resource-backed handles the RES branch below already excuses), and a name` |
|         - | 1246 | ` * that resolves to nothing must not turn into a rejection of a valid argument.` |
|         - | 1247 | ` */` |
|      2104 | 1248 | `static int VmSigObjSatisfiesClass(ph7_vm *pVm,const char *zType,int nType,` |
|         - | 1249 | `	ph7_class_instance *pObj)` |
|         5 | 1250 | `{` |
|      2109 | 1251 | `	int i = 0;` |
|      2109 | 1252 | `	if( pObj == 0 \|\| pObj->pClass == 0 ){` |
|       ! 0 | 1253 | `		return 1;` |
|         - | 1254 | `	}` |
|      2109 | 1255 | `	if( zType[0] == '?' ){` |
|       217 | 1256 | `		zType++;` |
|       217 | 1257 | `		nType--;` |
|       107 | 1258 | `	}` |
|      2131 | 1259 | `	while( i < nType ){` |
|      2111 | 1260 | `		int j = i;` |
|     24477 | 1261 | `		while( j < nType && zType[j] != '\|' ){` |
|     22371 | 1262 | `			j++;` |
|         5 | 1263 | `		}` |
|      2111 | 1264 | `		if( j > i && !VmSigArmIsBuiltinType(&zType[i],j - i) ){` |
|      2109 | 1265 | `			ph7_class *pClass = PH7_VmExtractClass(&(*pVm),&zType[i],(sxu32)(j - i),FALSE,0);` |
|      2109 | 1266 | `			if( pClass == 0 ){` |
|         - | 1267 | `				/* Either a builtin type name (already excluded by the caller) or a` |
|         - | 1268 | `				 * class this build does not declare: nothing to judge. */` |
|       ! 0 | 1269 | `				return 1;` |
|         - | 1270 | `			}` |
|      2109 | 1271 | `			if( PH7_VmInstanceOf(pObj->pClass,pClass) ){` |
|      2089 | 1272 | `				return 1;` |
|         - | 1273 | `			}` |
|        10 | 1274 | `		}` |
|        25 | 1275 | `		i = j + 1;` |
|         3 | 1276 | `	}` |
|        23 | 1277 | `	return 0;` |
|      1057 | 1278 | `}` |
|       100 | 1279 | `static const char * VmArgTypeName(ph7_value *pVal)` |
|         4 | 1280 | `{` |
|       104 | 1281 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       104 | 1282 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       104 | 1283 | `		if( pInst && pInst->pClass ){` |
|       104 | 1284 | `			return pInst->pClass->sName.zString;` |
|         - | 1285 | `		}` |
|       ! 0 | 1286 | `	}` |
|       ! 0 | 1287 | `	return ph7_type_name(pVal);` |
|        54 | 1288 | `}` |
|         - | 1289 | `/*` |
|         - | 1290 | `` * Does pArg satisfy a `string` parameter? The rule VmEnforceBuiltinArgTypes()`` |
|         - | 1291 | ` * below applies, factored out so a builtin that words its own overload dispatch` |
|         - | 1292 | ` * (strtr(), whose expected type depends on the ARITY and so cannot be spelled in` |
|         - | 1293 | ` * one signature) decides identically instead of forking the logic. An array never` |
|         - | 1294 | ` * satisfies one; an object does only through __toString(); a resource does not;` |
|         - | 1295 | ` * null does under php, with a deprecation, but not under PHL's §10 null-strictness` |
|         - | 1296 | ` * policy — the screen and this helper both report it as a mismatch.` |
|         - | 1297 | ` */` |
|     42076 | 1298 | `PH7_PRIVATE int PH7_ArgSatisfiesString(ph7_value *pArg)` |
|         5 | 1299 | `{` |
|     42081 | 1300 | `	if( (pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_NULL\|MEMOBJ_RES)) != 0 ){` |
|        23 | 1301 | `		return 0;` |
|         - | 1302 | `	}` |
|     42061 | 1303 | `	if( (pArg->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       141 | 1304 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|       141 | 1305 | `		return pInst && PH7_ClassExtractMethod(pInst->pClass,"__toString",` |
|        68 | 1306 | `			sizeof("__toString")-1) != 0;` |
|         - | 1307 | `	}` |
|     41925 | 1308 | `	return 1;` |
|     21043 | 1309 | `}` |
|         - | 1310 | `/*` |
|         - | 1311 | `` * Is the declared type exactly `int` — the only shape whose float argument the`` |
|         - | 1312 | `` * screen below can decide? A union with a `float`, `string` or `bool` arm has its`` |
|         - | 1313 | ` * own coercion rules per arm (and php words those refusals from the builtin), so` |
|         - | 1314 | ` * only the plain form and its nullable spelling qualify.` |
|         - | 1315 | ` */` |
|       654 | 1316 | `static int VmSigTypeIsIntOnly(const char *zType,int nType)` |
|         5 | 1317 | `{` |
|       659 | 1318 | `	if( nType > 0 && zType[0] == '?' ){` |
|        56 | 1319 | `		zType++;` |
|        56 | 1320 | `		nType--;` |
|        26 | 1321 | `	}` |
|       659 | 1322 | `	if( nType == (int)sizeof("int")-1 && SyMemcmp(zType,"int",3) == 0 ){` |
|       154 | 1323 | `		return 1;` |
|         - | 1324 | `	}` |
|         - | 1325 | ``	/* `int\|null` / `null\|int`, the union spelling of `?int`. */`` |
|       676 | 1326 | `	return VmSigTypeHas(zType,nType,"int") && VmSigTypeHas(zType,nType,"null")` |
|       168 | 1327 | `	    && !VmSigTypeHas(zType,nType,"float")` |
|         2 | 1328 | `	    && !VmSigTypeHas(zType,nType,"string")` |
|         1 | 1329 | `	    && !VmSigTypeHas(zType,nType,"bool")` |
|       ! 0 | 1330 | `	    && !VmSigTypeHas(zType,nType,"array")` |
|       ! 0 | 1331 | `	    && !VmSigTypeHas(zType,nType,"object")` |
|       ! 0 | 1332 | `	    && !VmSigTypeHas(zType,nType,"iterable")` |
|       ! 0 | 1333 | `	    && !VmSigTypeHas(zType,nType,"callable")` |
|       671 | 1334 | `	    && !VmSigTypeHasClass(zType,nType);` |
|       332 | 1335 | `}` |
|         - | 1336 | `/*` |
|         - | 1337 | `` * Can this float reach an `int` parameter without losing anything? php's rule is`` |
|         - | 1338 | ` * php_parse_arg_long's: in range, and integral. NaN and the infinities are out by` |
|         - | 1339 | ` * the range test (a NaN compares false against both bounds, which is why the test` |
|         - | 1340 | ` * is written as a pair of accepts rather than a pair of rejects).` |
|         - | 1341 | ` */` |
|        98 | 1342 | `static int VmDoubleFitsInt(double d)` |
|         4 | 1343 | `{` |
|       102 | 1344 | `	if( !(d >= -9223372036854775808.0 && d < 9223372036854775808.0) ){` |
|        50 | 1345 | `		return 0;` |
|         - | 1346 | `	}` |
|        54 | 1347 | `	return d == (double)(sxi64)d;` |
|        53 | 1348 | `}` |
|         - | 1349 | `/*` |
|         - | 1350 | ` * The same question for a NUMERIC string, which php asks with the same answer:` |
|         - | 1351 | `` * `dechex("1e19")` and `dechex("99999999999999999999")` are both`` |
|         - | 1352 | `` * `must be of type int, string given`. RangeStrToNumber is php's`` |
|         - | 1353 | ` * is_numeric_string grammar and already reclassifies an integer too wide for an` |
|         - | 1354 | ` * sxi64 as a DOUBLE, so the two shapes converge on one test.` |
|         - | 1355 | ` */` |
|        76 | 1356 | `static int VmNumStrFitsInt(ph7_value *pArg)` |
|         3 | 1357 | `{` |
|         - | 1358 | `	const char *zStr;` |
|        79 | 1359 | `	int nLen = 0;` |
|        79 | 1360 | `	sxi64 iVal = 0;` |
|        79 | 1361 | `	double dVal = 0;` |
|        79 | 1362 | `	zStr = ph7_value_to_string(pArg,&nLen);` |
|        79 | 1363 | `	switch( RangeStrToNumber(zStr,(sxu32)nLen,&iVal,&dVal) ){` |
|        54 | 1364 | `	case RANGE_IN_LONG:   return 1;` |
|        27 | 1365 | `	case RANGE_IN_DOUBLE: return VmDoubleFitsInt(dVal);` |
|       ! 0 | 1366 | `	default:              return 0;` |
|         - | 1367 | `	}` |
|        41 | 1368 | `}` |
|         - | 1369 | `/*` |
|         - | 1370 | ` * PHP-8 PATH parameters: which positions carry a filesystem path, a shell` |
|         - | 1371 | ` * command or an include-path list rather than an ordinary string.` |
|         - | 1372 | ` *` |
|         - | 1373 | ` * php spells this in the ZPP macro, not in the declared type: a path parameter` |
|         - | 1374 | `` * is `Z_PARAM_PATH` where an ordinary one is `Z_PARAM_STR`, and both print as`` |
|         - | 1375 | `` * `string` in the stub Reflection reads. The difference is a single rule — a`` |
|         - | 1376 | ` * path may not contain a NUL byte — and php raises a catchable ValueError for` |
|         - | 1377 | ` * one that does, BEFORE the call reaches the filesystem.` |
|         - | 1378 | ` *` |
|         - | 1379 | ` * PHL had no such notion, so every one of these arguments went to the C API as` |
|         - | 1380 | ` * a NUL-terminated string and was silently TRUNCATED at the NUL. That is not a` |
|         - | 1381 | ` * missing diagnostic: the truncated path is a DIFFERENT path, and the builtin` |
|         - | 1382 | `` * then operated on it. `unlink("$dir/x\0.png")` deleted `$dir/x`,`` |
|         - | 1383 | `` * `file_put_contents("$dir/x\0.txt",$d)` wrote it, `touch`/`chmod`/`copy`/`` |
|         - | 1384 | ``  * `rename`/`symlink`/`mkdir` all acted on the prefix, `glob` and `realpath` `` |
|         - | 1385 | `` * answered for it, and `shell_exec("cmd\0; rm -rf /")` ran the prefix as a`` |
|         - | 1386 | ` * command. It is the classic poison-NUL-byte shape php closed engine-wide: a` |
|         - | 1387 | ` * script that concatenates request input into a filename gets a truncation` |
|         - | 1388 | ` * where php gets a refusal, and the extension check the suffix was there to` |
|         - | 1389 | ` * perform never runs.` |
|         - | 1390 | ` *` |
|         - | 1391 | ` * The mask is positional (bit N => parameter N is a path), which is how php` |
|         - | 1392 | ` * carries it too. Only functions PHL actually registers are listed; each row's` |
|         - | 1393 | ` * positions were verified against php 8.5 argument by argument (the answer is` |
|         - | 1394 | `` * NOT derivable from the parameter name — preg_match's `$pattern` is an`` |
|         - | 1395 | ``  * ordinary string, glob's is a path — nor from the type, which is `string` `` |
|         - | 1396 | ` * for both).` |
|         - | 1397 | ` *` |
|         - | 1398 | ` * What is deliberately NOT here: the stat family (file_exists, is_dir, stat,` |
|         - | 1399 | ` * filesize, fileperms, …), which php parses with Z_PARAM_STR and answers` |
|         - | 1400 | `` * `false` for in silence, and the pure PATH-STRING functions (basename,`` |
|         - | 1401 | ` * dirname, pathinfo), which php lets the NUL through untouched because they` |
|         - | 1402 | ` * never touch the filesystem. Both are php-exact here already.` |
|         - | 1403 | ` */` |
|   2667806 | 1404 | `static sxu32 VmBuiltinPathMask(SyString *pName)` |
|         5 | 1405 | `{` |
|         - | 1406 | `	static const struct {` |
|         - | 1407 | `		const char *zName;` |
|         - | 1408 | `		sxu32 nByte;` |
|         - | 1409 | `		sxu32 mask;` |
|         - | 1410 | `	} aPath[] = {` |
|         - | 1411 | `		/* Open / read / write */` |
|         - | 1412 | `		{ "fopen",             5, 1u<<0 },` |
|         - | 1413 | `		{ "file_get_contents", 17, 1u<<0 },` |
|         - | 1414 | `		{ "file_put_contents", 17, 1u<<0 },` |
|         - | 1415 | `		{ "file",              4, 1u<<0 },` |
|         - | 1416 | `		{ "readfile",          8, 1u<<0 },` |
|         - | 1417 | `		{ "parse_ini_file",   14, 1u<<0 },` |
|         - | 1418 | `		{ "md5_file",          8, 1u<<0 },` |
|         - | 1419 | `		{ "sha1_file",         9, 1u<<0 },` |
|         - | 1420 | `		{ "hash_file",         9, 1u<<1 },` |
|         - | 1421 | `		{ "hash_hmac_file",   14, 1u<<1 },` |
|         - | 1422 | `		{ "hash_update_file", 16, 1u<<1 },` |
|         - | 1423 | `		/* Metadata / mutation */` |
|         - | 1424 | `		{ "unlink",            6, 1u<<0 },` |
|         - | 1425 | `		{ "touch",             5, 1u<<0 },` |
|         - | 1426 | `		{ "chmod",             5, 1u<<0 },` |
|         - | 1427 | `		{ "chgrp",             5, 1u<<0 },` |
|         - | 1428 | `		{ "chown",             5, 1u<<0 },` |
|         - | 1429 | `		{ "rename",            6, (1u<<0)\|(1u<<1) },` |
|         - | 1430 | `		{ "copy",              4, (1u<<0)\|(1u<<1) },` |
|         - | 1431 | `		{ "link",              4, (1u<<0)\|(1u<<1) },` |
|         - | 1432 | `		{ "symlink",           7, (1u<<0)\|(1u<<1) },` |
|         - | 1433 | `		{ "readlink",          8, 1u<<0 },` |
|         - | 1434 | `		{ "realpath",          8, 1u<<0 },` |
|         - | 1435 | `		{ "stream_resolve_include_path", 27, 1u<<0 },` |
|         - | 1436 | `		/* Directories */` |
|         - | 1437 | `		{ "mkdir",             5, 1u<<0 },` |
|         - | 1438 | `		{ "rmdir",             5, 1u<<0 },` |
|         - | 1439 | `		{ "opendir",           7, 1u<<0 },` |
|         - | 1440 | `		{ "dir",               3, 1u<<0 },` |
|         - | 1441 | `		{ "scandir",           7, 1u<<0 },` |
|         - | 1442 | `		{ "chdir",             5, 1u<<0 },` |
|         - | 1443 | `		{ "chroot",            6, 1u<<0 },` |
|         - | 1444 | `		{ "glob",              4, 1u<<0 },` |
|         - | 1445 | `		{ "tempnam",           7, (1u<<0)\|(1u<<1) },` |
|         - | 1446 | `		{ "disk_free_space",  15, 1u<<0 },` |
|         - | 1447 | `		{ "disk_total_space", 16, 1u<<0 },` |
|         - | 1448 | `		{ "diskfreespace",    13, 1u<<0 },` |
|         - | 1449 | `		/* Path-shaped settings and the pattern matcher */` |
|         - | 1450 | `		{ "fnmatch",           7, (1u<<0)\|(1u<<1) },` |
|         - | 1451 | `		{ "set_include_path", 16, 1u<<0 },` |
|         - | 1452 | `		{ "session_save_path", 17, 1u<<0 },` |
|         - | 1453 | `		{ "error_log",         9, 1u<<2 },` |
|         - | 1454 | `		/* Commands handed to the shell — and the two escapers, which php screens` |
|         - | 1455 | `		 * the same way even though neither of them runs anything: a NUL in what a` |
|         - | 1456 | `		 * script is about to hand a shell is refused where it is WRITTEN. */` |
|         - | 1457 | `		{ "shell_exec",       10, 1u<<0 },` |
|         - | 1458 | `		{ "popen",             5, 1u<<0 },` |
|         - | 1459 | `		{ "escapeshellarg",   14, 1u<<0 },` |
|         - | 1460 | `		{ "escapeshellcmd",   14, 1u<<0 },` |
|         - | 1461 | `		{ "exec",              4, 1u<<0 },` |
|         - | 1462 | `		{ "system",            6, 1u<<0 },` |
|         - | 1463 | `		{ "passthru",          8, 1u<<0 },` |
|         - | 1464 | `		/* The SPL path constructors, which php screens identically and reports` |
|         - | 1465 | ``		 * under their QUALIFIED name (`SplFileInfo::__construct(): Argument #1`` |
|         - | 1466 | ``		 * ($filename) …`). They are native methods, so their signature reaches this`` |
|         - | 1467 | `		 * screen the same way a builtin's does. */` |
|         - | 1468 | `		{ "SplFileInfo::__construct",                24, 1u<<0 },` |
|         - | 1469 | `		{ "DirectoryIterator::__construct",          30, 1u<<0 },` |
|         - | 1470 | `		{ "FilesystemIterator::__construct",         31, 1u<<0 },` |
|         - | 1471 | `		{ "RecursiveDirectoryIterator::__construct", 39, 1u<<0 },` |
|         - | 1472 | `	};` |
|         - | 1473 | `	sxu32 i;` |
|   2667811 | 1474 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|       ! 0 | 1475 | `		return 0;` |
|         - | 1476 | `	}` |
| 132813986 | 1477 | `	for( i = 0 ; i < SX_ARRAYSIZE(aPath) ; ++i ){` |
| 130234820 | 1478 | `		if( pName->nByte == aPath[i].nByte` |
|  67250269 | 1479 | `		 && SyStrnicmp(pName->zString,aPath[i].zName,pName->nByte) == 0 ){` |
|     88650 | 1480 | `			return aPath[i].mask;` |
|         - | 1481 | `		}` |
|  65114355 | 1482 | `	}` |
|   2579166 | 1483 | `	return 0;` |
|   1334733 | 1484 | `}` |
|         - | 1485 | `/*` |
|         - | 1486 | ` * Does this argument carry a NUL byte? Only a STRING can: every other scalar` |
|         - | 1487 | ` * renders through the number/bool formatters, which emit none. An OBJECT is` |
|         - | 1488 | ` * coerced by the caller before asking (php's ZPP order), so by the time this` |
|         - | 1489 | ` * runs a Stringable is already the string it produced.` |
|         - | 1490 | ` */` |
|     88663 | 1491 | `static int VmArgHasNulByte(ph7_value *pArg)` |
|         5 | 1492 | `{` |
|         - | 1493 | `	const char *zStr;` |
|         - | 1494 | `	sxu32 n, nLen;` |
|     88668 | 1495 | `	if( (pArg->iFlags & MEMOBJ_STRING) == 0 ){` |
|         3 | 1496 | `		return 0;` |
|         - | 1497 | `	}` |
|     88666 | 1498 | `	zStr = (const char *)SyBlobData(&pArg->sBlob);` |
|     88666 | 1499 | `	nLen = SyBlobLength(&pArg->sBlob);` |
|   5778257 | 1500 | `	for( n = 0 ; n < nLen ; ++n ){` |
|   5689694 | 1501 | `		if( zStr[n] == 0 ){` |
|       100 | 1502 | `			return 1;` |
|         - | 1503 | `		}` |
|   2885302 | 1504 | `	}` |
|     88568 | 1505 | `	return 0;` |
|     44336 | 1506 | `}` |
|         - | 1507 | `/*` |
|         - | 1508 | ` * Does php's strict_types rule refuse this argument for the declared type?` |
|         - | 1509 | ` *` |
|         - | 1510 | `` * A `declare(strict_types=1)` file gets NO scalar coercion at an internal call`` |
|         - | 1511 | ` * either — php applies the same rule to a builtin, a native method and a userland` |
|         - | 1512 | `` * function, and the single exception is the int -> float widening. So `trim(5)`,`` |
|         - | 1513 | `` * `sqrt("4")`, `str_repeat("a", 2.0)` and `in_array($n, $a, 1)` are all TypeErrors`` |
|         - | 1514 | ` * there, where the weak-mode screen below (which is the only one PHL had) coerces` |
|         - | 1515 | ` * and computes.` |
|         - | 1516 | ` *` |
|         - | 1517 | ` * Only the arms a scalar could otherwise satisfy are decided here; an array, a` |
|         - | 1518 | ` * resource, a null and a class-typed mismatch are the weak screen's, and its` |
|         - | 1519 | ` * verdicts stand in both modes.` |
|         - | 1520 | ` */` |
|       194 | 1521 | `static int VmStrictArgRefused(ph7_value *pArg,const char *zType,int nType)` |
|         3 | 1522 | `{` |
|         - | 1523 | `	/* Tested in ph7_type_name()'s own order, so the branch taken and the name the` |
|         - | 1524 | `	 * refusal reports can never disagree. FLOAT comes before INT on purpose:` |
|         - | 1525 | `	 * ph7_value_is_int() is deliberately lenient — an integer-valued real caches an` |
|         - | 1526 | ``	 * int and answers TRUE — and `str_repeat("a", 2.0)` is php's TypeError, not an`` |
|         - | 1527 | `	 * accepted int. */` |
|       197 | 1528 | `	if( ph7_value_is_bool(pArg) ){` |
|        12 | 1529 | `		return !VmSigTypeHas(zType,nType,"bool")` |
|         7 | 1530 | `		    && !VmSigTypeHas(zType,nType,"true")` |
|        11 | 1531 | `		    && !VmSigTypeHas(zType,nType,"false");` |
|         - | 1532 | `	}` |
|       189 | 1533 | `	if( ph7_value_is_float(pArg) ){` |
|         7 | 1534 | `		return !VmSigTypeHas(zType,nType,"float");` |
|         - | 1535 | `	}` |
|       183 | 1536 | `	if( ph7_value_is_int(pArg) ){` |
|         - | 1537 | `		/* int -> float is the one widening strict mode keeps. */` |
|        28 | 1538 | `		return !VmSigTypeHas(zType,nType,"int") && !VmSigTypeHas(zType,nType,"float");` |
|         - | 1539 | `	}` |
|       157 | 1540 | `	if( ph7_value_is_string(pArg) ){` |
|         - | 1541 | ``		/* `callable` is not a coercion: a function-name string satisfies it in both`` |
|         - | 1542 | `		 * modes (array_map('strtoupper', …) under strict is php-legal). */` |
|       104 | 1543 | `		return !VmSigTypeHas(zType,nType,"string") && !VmSigTypeHas(zType,nType,"callable");` |
|         - | 1544 | `	}` |
|        54 | 1545 | `	if( ph7_value_is_object(pArg) ){` |
|         - | 1546 | ``		/* An object reaches a `string` parameter only through __toString(), which is`` |
|         - | 1547 | `		 * a coercion strict mode does not perform. Every other arm is the weak` |
|         - | 1548 | `		 * screen's decision. */` |
|        24 | 1549 | `		return VmSigTypeHas(zType,nType,"string")` |
|        12 | 1550 | `		    && !VmSigTypeHas(zType,nType,"object")` |
|         2 | 1551 | `		    && !VmSigTypeHas(zType,nType,"iterable")` |
|         2 | 1552 | `		    && !VmSigTypeHas(zType,nType,"callable")` |
|        23 | 1553 | `		    && !VmSigTypeHasClass(zType,nType);` |
|         - | 1554 | `	}` |
|        32 | 1555 | `	return 0;` |
|       100 | 1556 | `}` |
|         - | 1557 | `/*` |
|         - | 1558 | ` * PHP-8 ZPP type enforcement for host functions, driven by the aBuiltinSig[]` |
|         - | 1559 | ` * declaration (band A #7). Screens only the arguments that php can NEVER coerce` |
|         - | 1560 | ` * into a declared scalar parameter — arrays, resources, and objects without a` |
|         - | 1561 | ` * __toString() — and throws the catchable TypeError php throws, before the C` |
|         - | 1562 | ` * routine runs. Without this an array argument reached the builtin and was` |
|         - | 1563 | ` * stringified to the literal "Array" (strrev(['a']) returned "yarrA").` |
|         - | 1564 | ` *` |
|         - | 1565 | ` * Deliberately narrow: scalar-to-scalar coercion (and its deprecations) stays` |
|         - | 1566 | ` * with the per-builtin ZPP helpers. Those emit E_DEPRECATED, which does NOT` |
|         - | 1567 | ` * abort the call, so a central copy would double-fire — the trap that sank the` |
|         - | 1568 | ` * first central-ZPP attempt. A TypeError aborts, so there is nothing to double.` |
|         - | 1569 | ` */` |
|   2865968 | 1570 | `PH7_PRIVATE sxi32 VmEnforceBuiltinArgTypes(` |
|         - | 1571 | `	ph7_context *pCtx,    /* Call context (for the throw) */` |
|         - | 1572 | `	ph7_user_func *pFunc, /* Callee */` |
|         - | 1573 | `	int nGiven,           /* Argument count */` |
|         - | 1574 | `	ph7_value **apArg     /* Arguments */` |
|         - | 1575 | `	)` |
|         5 | 1576 | `{` |
|         - | 1577 | `	/*` |
|         - | 1578 | `	 * Builtins whose own argument check is php-exact and VALUE-based rather than` |
|         - | 1579 | `	 * type-based must not be pre-empted here, or their message is lost. Same rule` |
|         - | 1580 | `	 * the aBuiltinArity[] table follows: a builtin that already says what php says` |
|         - | 1581 | `	 * stays off the shared screen. get_class_vars() takes any stringifiable value` |
|         - | 1582 | `	 * and reports "must be a valid class name, Array given"; get_class_methods() is` |
|         - | 1583 | `	 * the same shape with php's other wording ("must be an object or a valid class` |
|         - | 1584 | ``	 * name, int given") — the declared `object\|string` never appears in either.`` |
|         - | 1585 | `	 *` |
|         - | 1586 | `	 * strtr() is here for a structural reason: php declares it as two OVERLOADS` |
|         - | 1587 | `	 * dispatched on arity — strtr(string, array) and strtr(string, string, string)` |
|         - | 1588 | `` 	 * — so the expected type of $from is `array` with two arguments and `string` `` |
|         - | 1589 | ``	 * with three. One signature cannot say that (the stub's `array\|string` is the`` |
|         - | 1590 | `	 * union of the two, which is the wording php never uses), so the builtin does` |
|         - | 1591 | `	 * its own dispatch through PH7_ArgSatisfiesString(), the same rule as here.` |
|         - | 1592 | `	 *` |
|         - | 1593 | ``	 * implode() is the same structure: `array\|string $separator` is what the two`` |
|         - | 1594 | `	 * ARITIES accept between them, never what one call can use. Once an $array` |
|         - | 1595 | `	 * argument is present php has resolved the overload and reports` |
|         - | 1596 | ``	 * `must be of type string`, and with the array in position #1 it reports`` |
|         - | 1597 | ``	 * `must be of type string, array given` against #1 rather than a #2 error.`` |
|         - | 1598 | `	 * PH7_builtin_implode words all of that itself.` |
|         - | 1599 | `	 *` |
|         - | 1600 | `	 * Its alias join() is here for the same reason and then some: php 8.5 does not` |
|         - | 1601 | `	 * word the two the same, so the builtin reproduces BOTH orders keyed on the` |
|         - | 1602 | `	 * invoked name (see PH7_builtin_implode's header for the value-for-value` |
|         - | 1603 | `	 * table against 8.5.8). php's own asymmetry between a target and its alias,` |
|         - | 1604 | `	 * reproduced rather than smoothed over — parity is binding (§10).` |
|         - | 1605 | `	 *` |
|         - | 1606 | `	 * number_format() is here because php's DECLARED type and its REFUSAL text` |
|         - | 1607 | ``	 * disagree: the stub says `float $num` (which is what Reflection prints) while`` |
|         - | 1608 | `	 * the ZPP macro behind it is Z_PARAM_NUMBER, whose TypeError says` |
|         - | 1609 | ``	 * `must be of type int\|float`. One row cannot say both, so the row carries the`` |
|         - | 1610 | `	 * declared type for Reflection and the builtin words every refusal itself.` |
|         - | 1611 | `	 *` |
|         - | 1612 | `	 * RecursiveIteratorIterator::__construct() is the first NATIVE METHOD here, and` |
|         - | 1613 | `	 * it is the same disagreement one level up: php's stub declares` |
|         - | 1614 | ``	 * `Traversable $iterator` (what Reflection prints) while its ZPP is a bare "o",`` |
|         - | 1615 | ``	 * whose TypeError says `must be of type object`. A native method's diagnostic`` |
|         - | 1616 | `	 * name is the QUALIFIED one, so the row below matches it and nothing else.` |
|         - | 1617 | `	 *` |
|         - | 1618 | `	 * The array_udiff/array_uintersect u-variant family is here for its ORDER:` |
|         - | 1619 | `	 * php validates the trailing comparison callback(s) before ANY of the` |
|         - | 1620 | `	 * arrays — array_diff_ukey(123,[1],456) names Argument #3, not #1 — and a` |
|         - | 1621 | `	 * positional screen cannot say that. HashmapUVariant performs the whole` |
|         - | 1622 | `	 * php sequence itself (callbacks, then Argument #1, then the middles).` |
|         - | 1623 | `	 */` |
|         - | 1624 | `	static const char *azSelfChecked[] = { "get_class_vars", "get_class_methods", "strtr",` |
|         - | 1625 | `		"implode", "join", "number_format", "RecursiveIteratorIterator::__construct",` |
|         - | 1626 | `		"array_udiff", "array_udiff_assoc", "array_udiff_uassoc",` |
|         - | 1627 | `		"array_uintersect", "array_uintersect_assoc", "array_uintersect_uassoc",` |
|         - | 1628 | `		"array_diff_uassoc", "array_diff_ukey",` |
|         - | 1629 | `		"array_intersect_uassoc", "array_intersect_ukey" };` |
|   2865973 | 1630 | `	const char *zSig = pFunc->zSig;` |
|         - | 1631 | `	const char *zCur, *zEnd;` |
|   2865973 | 1632 | `	int iArg = 0;` |
|         - | 1633 | `	/* The CALL site's file mode, stamped by the compiler onto this call's argument` |
|         - | 1634 | `	 * map (weak when there is no map — a call that carries no compile-time metadata` |
|         - | 1635 | `	 * was written in a weak-mode file, since a strict one always attaches one). */` |
|   2865973 | 1636 | `	int bStrict = (pCtx->pArgMap && pCtx->pArgMap->bStrict) ? 1 : 0;` |
|         - | 1637 | `	sxu32 nPathMask;` |
|   2865973 | 1638 | `	if( zSig == 0 ){` |
|    198167 | 1639 | `		return SXRET_OK;` |
|         - | 1640 | `	}` |
|   2667811 | 1641 | `	nPathMask = VmBuiltinPathMask(&pFunc->sName);` |
|  47429673 | 1642 | `	for( iArg = 0 ; iArg < (int)SX_ARRAYSIZE(azSelfChecked) ; ++iArg ){` |
|  67220325 | 1643 | `		if( SyStrncmp(pFunc->sName.zString,azSelfChecked[iArg],` |
|  67220325 | 1644 | `			(sxu32)SyStrlen(azSelfChecked[iArg])) == 0` |
|  22437320 | 1645 | `		 && pFunc->sName.nByte == SyStrlen(azSelfChecked[iArg]) ){` |
|     42343 | 1646 | `			return SXRET_OK;` |
|         - | 1647 | `		}` |
|  22394961 | 1648 | `	}` |
|   2625473 | 1649 | `	iArg = 0;` |
|   2625473 | 1650 | `	zCur = zSig;` |
|   2625473 | 1651 | `	zEnd = &zSig[SyStrlen(zSig)];` |
|   6210794 | 1652 | `	while( zCur < zEnd && iArg < nGiven ){` |
|         - | 1653 | `		const char *zType, *zName, *zStop;` |
|         - | 1654 | `		int nType, nName, bByRef;` |
|         - | 1655 | `		ph7_value *pArg;` |
|         - | 1656 | `		char zGivenBuf[64];` |
|         - | 1657 | `		/* Parameter = "<type> $<name>[ = <default>]"; the type is whatever` |
|         - | 1658 | `		 * precedes the '$', and an empty type means "untyped" (no screen). */` |
|   4609226 | 1659 | `		while( zCur < zEnd && zCur[0] == ' ' ){` |
|   1017673 | 1660 | `			zCur++;` |
|         5 | 1661 | `		}` |
|   3591558 | 1662 | `		zStop = zCur;` |
|  62463497 | 1663 | `		while( zStop < zEnd && zStop[0] != ',' ){` |
|  58871944 | 1664 | `			if( zStop[0] == '\'' \|\| zStop[0] == '"' ){` |
|   1457395 | 1665 | `				zStop = VmSigSkipQuoted(zStop);` |
|   1457395 | 1666 | `				if( zStop >= zEnd ){` |
|       ! 0 | 1667 | `					break;` |
|         - | 1668 | `				}` |
|    728695 | 1669 | `			}` |
|  58871944 | 1670 | `			zStop++;` |
|         5 | 1671 | `		}` |
|   3591558 | 1672 | `		zName = zCur;` |
|  27086147 | 1673 | `		while( zName < zStop && zName[0] != '$' ){` |
|  23494594 | 1674 | `			zName++;` |
|         5 | 1675 | `		}` |
|   3591558 | 1676 | `		if( zName >= zStop ){` |
|       ! 0 | 1677 | `			break; /* malformed / no parameter name — stop screening */` |
|         - | 1678 | `		}` |
|   3591558 | 1679 | `		if( zName >= zCur + 3 && SyMemcmp(zName - 3,"...",3) == 0 ){` |
|      5251 | 1680 | `			break; /* variadic tail: stop (its type applies to the rest) */` |
|         - | 1681 | `		}` |
|   3586312 | 1682 | `		zType = zCur;` |
|   3586312 | 1683 | `		nType = (int)(zName - zCur);` |
|         - | 1684 | `		/* Trim the trailing spaces and the by-ref marker of "array &$array" */` |
|   3586312 | 1685 | `		bByRef = 0;` |
|  10623616 | 1686 | `		while( nType > 0 && (zType[nType-1] == ' ' \|\| zType[nType-1] == '&') ){` |
|   3518440 | 1687 | `			if( zType[nType-1] == '&' ){` |
|      3332 | 1688 | `				bByRef = 1;` |
|      1663 | 1689 | `			}` |
|   3518440 | 1690 | `			nType--;` |
|         5 | 1691 | `		}` |
|   3586312 | 1692 | `		zName++; /* skip '$' */` |
|   3586312 | 1693 | `		nName = 0;` |
|  26922606 | 1694 | `		while( &zName[nName] < zStop && zName[nName] != ' ' && zName[nName] != '=' ){` |
|  23336299 | 1695 | `			nName++;` |
|         5 | 1696 | `		}` |
|   3586312 | 1697 | `		pArg = apArg[iArg];` |
|   3586307 | 1698 | `		if( bByRef && pArg->nIdx == SXU32_HIGH` |
|      1703 | 1699 | `		 && !(pCtx->pArgMap && pCtx->pArgMap->bArgShapes && !pCtx->pArgMap->bHasNamed) ){` |
|         - | 1700 | `			/* A by-reference parameter handed something with no slot to write back` |
|         - | 1701 | `			 * through -- a literal, a constant, the result of a call. php settles` |
|         - | 1702 | `			 * that at the CALL, before the callee's ZPP runs, so the type screen` |
|         - | 1703 | ``			 * must not speak first: `array_pop('foo')` is`` |
|         - | 1704 | `			 * "could not be passed by reference" and not "must be of type array,` |
|         - | 1705 | `			 * string given".` |
|         - | 1706 | `			 *` |
|         - | 1707 | `			 * Only when this call site carries no argument SHAPES, though. When it` |
|         - | 1708 | `			 * does, PH7_VmScreenByRefArgShapes has already had its say — it refused` |
|         - | 1709 | `			 * the literal and let the call RESULT through with php's notice — and` |
|         - | 1710 | `			 * standing aside here would swallow the type error php still reports for` |
|         - | 1711 | ``			 * the latter (`sort(new stdClass)` is "must be of type array, stdClass`` |
|         - | 1712 | `			 * given", not a silent false). */` |
|         7 | 1713 | `			zCur = (zStop < zEnd) ? zStop + 1 : zEnd;` |
|         7 | 1714 | `			iArg++;` |
|         7 | 1715 | `			continue;` |
|         - | 1716 | `		}` |
|   3586306 | 1717 | `		if( nType > 0 && !VmSigTypeHas(zType,nType,"mixed") ){` |
|   3391469 | 1718 | `			const char *zGiven = 0;` |
|   3391469 | 1719 | `			if( bStrict && VmStrictArgRefused(pArg,zType,nType) ){` |
|         - | 1720 | ``				/* php names the VALUE for a bool here too (`true given`). */`` |
|        37 | 1721 | `				zGiven = VmValueGivenName(pArg,zGivenBuf,sizeof(zGivenBuf));` |
|   3391451 | 1722 | `			}else if( (pArg->iFlags & MEMOBJ_HASHMAP) != 0 ){` |
|     51021 | 1723 | `				if( !VmSigTypeHas(zType,nType,"array")` |
|     25711 | 1724 | `				 && !VmSigTypeHas(zType,nType,"iterable")` |
|       407 | 1725 | `				 && !VmSigTypeHas(zType,nType,"callable") ){` |
|       189 | 1726 | `					zGiven = "array";` |
|        97 | 1727 | `				}` |
|   3365922 | 1728 | `			}else if( (pArg->iFlags & MEMOBJ_OBJ) != 0 ){` |
|      8286 | 1729 | `				if( !VmSigTypeHas(zType,nType,"object")` |
|      6117 | 1730 | `				 && !VmSigTypeHas(zType,nType,"iterable")` |
|      3948 | 1731 | `				 && !VmSigTypeHas(zType,nType,"callable")` |
|      3216 | 1732 | `				 && !VmSigTypeHasClass(zType,nType) ){` |
|         - | 1733 | `					/* An object with __toString() still satisfies a string` |
|         - | 1734 | `					 * parameter in weak mode — php coerces it. */` |
|       217 | 1735 | `					int bStringable = VmSigTypeHas(zType,nType,"string")` |
|       154 | 1736 | `						&& PH7_ArgSatisfiesString(pArg);` |
|       159 | 1737 | `					if( !bStringable ){` |
|        84 | 1738 | `						zGiven = VmArgTypeName(pArg);` |
|        40 | 1739 | `					}` |
|      8214 | 1740 | `				}else if( VmSigTypeHasClass(zType,nType)` |
|      5149 | 1741 | `				       && !VmSigTypeHas(zType,nType,"object")` |
|      2166 | 1742 | `				       && !VmSigTypeHas(zType,nType,"iterable")` |
|      2166 | 1743 | `				       && !VmSigTypeHas(zType,nType,"callable")` |
|      2171 | 1744 | `				       && !VmSigTypeHas(zType,nType,"string") ){` |
|         - | 1745 | `					/* A class-typed parameter given an object of the WRONG class.` |
|         - | 1746 | `					 * Naming a class used to be enough to let ANY object through, so` |
|         - | 1747 | `` 					 * `date_modify($immutable)` and `timezone_name_get($date)` `` |
|         - | 1748 | `					 * answered silently where php raises. Only decided when every` |
|         - | 1749 | `					 * class arm resolves to a declared class: an arm PHL does not` |
|         - | 1750 | `					 * declare cannot be judged, so the parameter stays unscreened. */` |
|      3161 | 1751 | `					if( !VmSigObjSatisfiesClass(pCtx->pVm,zType,nType,` |
|      2104 | 1752 | `						(ph7_class_instance *)pArg->x.pOther) ){` |
|        23 | 1753 | `						zGiven = VmArgTypeName(pArg);` |
|        10 | 1754 | `					}` |
|      1057 | 1755 | `				}` |
|   3336269 | 1756 | `			}else if( (pArg->iFlags & MEMOBJ_NULL) != 0 ){` |
|         - | 1757 | `				/* php only DEPRECATES null for a non-nullable parameter; PHL rejects` |
|         - | 1758 | `				 * it (scope policy). A leading '?' or an explicit "null" arm in a` |
|         - | 1759 | `				 * union declares the parameter nullable. A "callable" parameter is` |
|         - | 1760 | `				 * left to the builtin's own callback check, which words the failure` |
|         - | 1761 | `				 * php's way ("must be a valid callback, no array or string given") —` |
|         - | 1762 | `				 * the same reason get_class_vars() sits on azSelfChecked[]. */` |
|      5856 | 1763 | `				if( zType[0] != '?'` |
|      2974 | 1764 | `				 && !VmSigTypeHas(zType,nType,"null")` |
|        89 | 1765 | `				 && !VmSigTypeHas(zType,nType,"callable") ){` |
|        73 | 1766 | `					zGiven = "null";` |
|        34 | 1767 | `				}` |
|   3329198 | 1768 | `			}else if( (pArg->iFlags & (MEMOBJ_STRING\|MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL)) != 0` |
|   3326265 | 1769 | `			       && (VmSigTypeHasClass(zType,nType)` |
|   3326067 | 1770 | `			        \|\| VmSigTypeHas(zType,nType,"object")) ){` |
|         - | 1771 | `				/* A SCALAR against a parameter that can only hold an INSTANCE —` |
|         - | 1772 | ``				 * a named class, or the bare `object` keyword. Every other scalar`` |
|         - | 1773 | `				 * pairing is left to weak-mode coercion, which is why nothing` |
|         - | 1774 | `				 * screened scalars here at all — but no coercion produces an` |
|         - | 1775 | `				 * instance, so php rejects this one. Found converting DateTime:` |
|         - | 1776 | ``				 * `$d->diff('x')` and `new DateTime('now','UTC')` ran on with a`` |
|         - | 1777 | ``				 * string where php raises. The `object` half was still blind when`` |
|         - | 1778 | `				 * WeakReference::create() declared the first such parameter, which` |
|         - | 1779 | `				 * also retires the "graceful degradation" NULL that spl_object_id(),` |
|         - | 1780 | `				 * spl_object_hash() and get_object_vars() used to answer. An arm a` |
|         - | 1781 | `				 * scalar CAN satisfy (a union with string/int/float/bool, or` |
|         - | 1782 | `				 * callable, which a string is) keeps the parameter unscreened —` |
|         - | 1783 | ``				 * and so does an `array` arm, whose refusal php words from the`` |
|         - | 1784 | `				 * builtin's own check rather than from the declared type` |
|         - | 1785 | ``				 * (array_walk's `array\|object &$array` says "must be of type`` |
|         - | 1786 | `				 * array", not "of type array\|object"). */` |
|      1703 | 1787 | `				if( !VmSigTypeHas(zType,nType,"string")` |
|       889 | 1788 | `				 && !VmSigTypeHas(zType,nType,"int")` |
|       114 | 1789 | `				 && !VmSigTypeHas(zType,nType,"float")` |
|        78 | 1790 | `				 && !VmSigTypeHas(zType,nType,"bool")` |
|        78 | 1791 | `				 && !VmSigTypeHas(zType,nType,"true")` |
|        78 | 1792 | `				 && !VmSigTypeHas(zType,nType,"false")` |
|        78 | 1793 | `				 && !VmSigTypeHas(zType,nType,"array")` |
|        64 | 1794 | `				 && !VmSigTypeHas(zType,nType,"callable") ){` |
|         - | 1795 | `					/* php's VALUE name, not the type's: a bool is reported as` |
|         - | 1796 | ``					 * `true`/`false` (the rule Generator::throw()'s own check`` |
|         - | 1797 | `					 * already followed, and which this screen now runs first). */` |
|        45 | 1798 | `					zGiven = VmValueGivenName(pArg,zGivenBuf,sizeof(zGivenBuf));` |
|        20 | 1799 | `				}` |
|   3325456 | 1800 | `			}else if( (pArg->iFlags & MEMOBJ_REAL) != 0` |
|   1663473 | 1801 | `			       && VmSigTypeIsIntOnly(zType,nType)` |
|       314 | 1802 | `			       && !VmDoubleFitsInt((double)pArg->rVal) ){` |
|         - | 1803 | ``				/* A FLOAT against a parameter typed exactly `int` (or `?int`), and`` |
|         - | 1804 | `				 * one no int can hold: a fraction, a magnitude past the signed` |
|         - | 1805 | `				 * 64-bit range, NaN or an infinity. php refuses every one of them` |
|         - | 1806 | `				 * (zend_parse_arg_long's ZEND_DOUBLE_FITS_LONG / is-integral pair,` |
|         - | 1807 | `				 * the fractional case with a deprecation PHL rejects outright by` |
|         - | 1808 | `				 * §10) and the refusal is this screen's own wording.` |
|         - | 1809 | `				 *` |
|         - | 1810 | `				 * PH7_IntArgResolve has always said exactly this, but only for the` |
|         - | 1811 | `` 				 * builtins that CALL it from their own body — so `dechex(1.5)` `` |
|         - | 1812 | ``				 * answered '1', `array_fill(1.5,1,0)` filled from 1, and`` |
|         - | 1813 | ``				 * `strpos("abc","c",1e19)` took the offset as PHP_INT_MIN and`` |
|         - | 1814 | `				 * reported a ValueError about a range it never had. Seventy-five` |
|         - | 1815 | ``				 * `int` parameters across the signature table were unscreened that`` |
|         - | 1816 | `				 * way, and a NATIVE METHOD has no body to call the helper from at` |
|         - | 1817 | `				 * all. Deciding it from the declared type covers both callee kinds` |
|         - | 1818 | `				 * from one place, and the per-builtin helper still stands for the` |
|         - | 1819 | ``				 * message rows this screen cannot reach (the `azSelfChecked` set). */`` |
|        60 | 1820 | `				zGiven = "float";` |
|   3324613 | 1821 | `			}else if( (pArg->iFlags & (MEMOBJ_STRING\|MEMOBJ_NULL)) == MEMOBJ_STRING` |
|   2919525 | 1822 | `			       && (VmSigTypeHas(zType,nType,"int")` |
|   2513695 | 1823 | `			        \|\| VmSigTypeHas(zType,nType,"float"))` |
|   1258035 | 1824 | `			       && !VmSigTypeHas(zType,nType,"string")` |
|   1257813 | 1825 | `			       && !VmSigTypeHas(zType,nType,"array")` |
|       270 | 1826 | `			       && !VmSigTypeHas(zType,nType,"object")` |
|       262 | 1827 | `			       && !VmSigTypeHas(zType,nType,"iterable")` |
|       262 | 1828 | `			       && !VmSigTypeHas(zType,nType,"callable")` |
|       262 | 1829 | `			       && !VmSigTypeHas(zType,nType,"bool")` |
|       267 | 1830 | `			       && !VmSigTypeHasClass(zType,nType) ){` |
|         - | 1831 | ``				/* A STRING against a NUMBER-only parameter — `int`, `float`, or the`` |
|         - | 1832 | ``				 * `int\|float` union, with no arm a string can satisfy. Weak mode`` |
|         - | 1833 | `				 * coerces a NUMERIC one and php refuses every other — "x", "2abc"` |
|         - | 1834 | ``				 * and "0x2" are all `must be of type int, string given` (rule 41: a`` |
|         - | 1835 | `				 * numeric PREFIX is not enough, which is what SyStrIsNumeric would` |
|         - | 1836 | `				 * have accepted). Every BUILTIN with an int parameter already got` |
|         - | 1837 | `				 * this from PH7_IntArgResolve, called from its own body; a native` |
|         - | 1838 | `` 				 * METHOD has no body to call it from, so `ArrayIterator::seek('x')` `` |
|         - | 1839 | ``				 * seeked to 0, `DateTime::setTimestamp('abc')` set 0 and`` |
|         - | 1840 | ``				 * `DOMNodeList::item('zz')` answered element 0 — wrong ANSWERS,`` |
|         - | 1841 | `				 * not missing errors. Screening the declared type here covers both` |
|         - | 1842 | `				 * callee kinds from one place.` |
|         - | 1843 | `				 *` |
|         - | 1844 | `				 * The FLOAT arm is the same hazard one type over, and it was the` |
|         - | 1845 | `				 * half nothing covered: PH7_IntArgResolve has no float twin, so a` |
|         - | 1846 | ``				 * `float $num` builtin that did not hand-roll its own check simply`` |
|         - | 1847 | `				 * converted the string to 0.0 and COMPUTED with it —` |
|         - | 1848 | ``				 * `cos("nope")` answered `float(1)`, `sqrt("nope")` `float(0)`,`` |
|         - | 1849 | ``				 * `log("nope")` `float(-INF)`. Numbers with nothing wrong-looking`` |
|         - | 1850 | `				 * about them, from input php refuses outright.` |
|         - | 1851 | `				 *` |
|         - | 1852 | `				 * The NULL rule stays where it is: PHL rejects null for a` |
|         - | 1853 | `				 * non-nullable parameter by policy (§10) where php deprecates. */` |
|       398 | 1854 | `				if( !PH7_MemObjStringIsNumeric(pArg) ){` |
|       157 | 1855 | `					zGiven = "string";` |
|       189 | 1856 | `				}else if( VmSigTypeIsIntOnly(zType,nType) && !VmNumStrFitsInt(pArg) ){` |
|         - | 1857 | `					/* A NUMERIC string an int cannot hold — "1.5", "1e19",` |
|         - | 1858 | `					 * "99999999999999999999". php refuses all three (the fractional` |
|         - | 1859 | `					 * one after a deprecation §10 turns into the refusal), and PHL` |
|         - | 1860 | ``					 * narrowed them silently: `dechex("1e19")` answered '1' and`` |
|         - | 1861 | ``					 * `str_repeat("a","99999999999999999999")` took PHP_INT_MAX as`` |
|         - | 1862 | `					 * the count. Same wording, same position as the float arm above,` |
|         - | 1863 | `					 * because php reaches both through one ZPP macro. */` |
|        19 | 1864 | `					zGiven = "string";` |
|         8 | 1865 | `				}` |
|   3324455 | 1866 | `			}else if( (pArg->iFlags & (MEMOBJ_STRING\|MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL)) != 0` |
|   3324319 | 1867 | `			       && VmSigTypeIsArrayOnly(zType,nType) ){` |
|         - | 1868 | ``				/* A SCALAR against a parameter typed exactly `array`. No coercion`` |
|         - | 1869 | `				 * produces one, so php refuses it -- but the screen exempted every` |
|         - | 1870 | ``				 * `array` arm, union or not, and a whole family had no check of its`` |
|         - | 1871 | `				 * own to fall back on: sort/rsort/ksort/krsort/shuffle and` |
|         - | 1872 | ``				 * usort/uasort/uksort each answered `false` for `sort($notAnArray)`,`` |
|         - | 1873 | `				 * which is also what they answer for a sort that genuinely failed.` |
|         - | 1874 | `				 * call_user_func_array('strlen', 'x') answered false too,` |
|         - | 1875 | `				 * iterator_apply RAN the callback, and getopt/hash/password_hash/` |
|         - | 1876 | `				 * password_needs_rehash/unserialize/fputcsv simply carried on with` |
|         - | 1877 | `				 * the string where an options ARRAY was declared.` |
|         - | 1878 | `				 *` |
|         - | 1879 | `				 * The builtins that DO check (array_keys, in_array, asort, ...) word` |
|         - | 1880 | `				 * it identically, so the screen only pre-empts them -- and corrects` |
|         - | 1881 | `				 * one detail on the way: their ph7_type_name() says "bool" where php` |
|         - | 1882 | ``				 * names the VALUE, `true` or `false`. */`` |
|       231 | 1883 | `				zGiven = VmValueGivenName(pArg,zGivenBuf,sizeof(zGivenBuf));` |
|   3324211 | 1884 | `			}else if( (pArg->iFlags & MEMOBJ_RES) != 0 ){` |
|         - | 1885 | `				/* A class-typed parameter also accepts a resource: several handles php 8` |
|         - | 1886 | `				 * models as objects are still resources here (xml_*'s XMLParser is the` |
|         - | 1887 | `				 * one the signatures already declare php-8-style, for reflection). The` |
|         - | 1888 | `				 * screen would otherwise reject the engine's own parser handle. Recorded` |
|         - | 1889 | `				 * as a divergence in NEWPLAN §7 — it goes away when those handles become` |
|         - | 1890 | `				 * real objects. */` |
|        10 | 1891 | `				if( !VmSigTypeHas(zType,nType,"resource")` |
|        12 | 1892 | `				 && !VmSigTypeHasClass(zType,nType) ){` |
|        12 | 1893 | `					zGiven = "resource";` |
|         5 | 1894 | `				}` |
|         5 | 1895 | `			}` |
|   3391469 | 1896 | `			if( zGiven ){` |
|         - | 1897 | ``				/* php's `object\|array` parameters come from ONE ZPP macro`` |
|         - | 1898 | `				 * (Z_PARAM_ARRAY_OR_OBJECT) and it names only "array" in the` |
|         - | 1899 | `				 * refusal — array_walk(null,…), current(null) and` |
|         - | 1900 | `				 * http_build_query(null) all say "must be of type array". The` |
|         - | 1901 | `				 * SCALAR branch above already encodes that rule by declining to` |
|         - | 1902 | `				 * screen at all; the null and resource branches do screen, so the` |
|         - | 1903 | `				 * reported type has to be corrected here instead. */` |
|       893 | 1904 | `				if( VmSigTypeHas(zType,nType,"array") && VmSigTypeHas(zType,nType,"object") ){` |
|        12 | 1905 | `					zType = "array";` |
|        12 | 1906 | `					nType = (int)sizeof("array")-1;` |
|         5 | 1907 | `				}` |
|      1386 | 1908 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 1909 | `					"%z(): Argument #%d ($%.*s) must be of type %.*s, %s given",` |
|       444 | 1910 | `					&pFunc->sName,iArg + 1,nName,zName,nType,zType,zGiven);` |
|         - | 1911 | `			}` |
|   1696170 | 1912 | `		}` |
|         - | 1913 | `		/* A PATH parameter, once its type is settled: php's Z_PARAM_PATH refuses a` |
|         - | 1914 | `		 * NUL byte outright rather than letting the C API truncate at it. Raised` |
|         - | 1915 | `		 * after the type verdict because that is php's order — the coercion runs` |
|         - | 1916 | `		 * first, and only a value that could BE a path is asked whether it is a` |
|         - | 1917 | `		 * legal one. */` |
|   3585418 | 1918 | `		if( iArg < 31 && (nPathMask & (1u<<iArg)) != 0 ){` |
|     88668 | 1919 | `			if( (pArg->iFlags & MEMOBJ_OBJ) != 0 && PH7_ArgSatisfiesString(pArg) ){` |
|         - | 1920 | `				/* A Stringable object: php coerces it and checks the RESULT, so` |
|         - | 1921 | ``				 * `unlink($o)` with a __toString() returning a NUL-bearing name is`` |
|         - | 1922 | `				 * the same ValueError. Converting IN PLACE is what keeps the` |
|         - | 1923 | `				 * accessor running exactly ONCE — the builtin then receives the` |
|         - | 1924 | `				 * string it would have produced itself. The argument a builtin sees` |
|         - | 1925 | `				 * is its own copy on every dispatch route (a direct call, a spread,` |
|         - | 1926 | `				 * both call_user_func forwards), so the caller's object is not` |
|         - | 1927 | `				 * retyped; strict mode never gets here, because a Stringable does` |
|         - | 1928 | ``				 * not satisfy a `string` parameter there and the screen above has`` |
|         - | 1929 | `				 * already refused it. */` |
|         3 | 1930 | `				sxi32 rcConv = PH7_MemObjToStringUV(pArg);` |
|         3 | 1931 | `				if( rcConv != SXRET_OK ){` |
|       ! 0 | 1932 | `					return rcConv; /* __toString() threw: php propagates it too */` |
|         - | 1933 | `				}` |
|         1 | 1934 | `			}` |
|     88668 | 1935 | `			if( VmArgHasNulByte(pArg) ){` |
|       149 | 1936 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 1937 | `					"%z(): Argument #%d ($%.*s) must not contain any null bytes",` |
|        49 | 1938 | `					&pFunc->sName,iArg + 1,nName,zName);` |
|         - | 1939 | `			}` |
|     44282 | 1940 | `		}` |
|   3585320 | 1941 | `		zCur = (zStop < zEnd) ? zStop + 1 : zEnd;` |
|   3585320 | 1942 | `		iArg++;` |
|         5 | 1943 | `	}` |
|   2624487 | 1944 | `	return SXRET_OK;` |
|   1433814 | 1945 | `}` |
|         - | 1946 | `/*` |
|         - | 1947 | ` * Builtins whose accepted arity is NOT a contiguous range, so the signature` |
|         - | 1948 | ` * cannot express it and the central too-many-arguments check must stay out of` |
|         - | 1949 | ` * the way: rand()/mt_rand() take 0 OR 2 arguments (never 1), and php words the` |
|         - | 1950 | ` * violation "expects exactly 2 arguments, 3 given" from their own check rather` |
|         - | 1951 | ` * than the ZPP "at most". They validate themselves; leaving bHasMaxArg at 0` |
|         - | 1952 | ` * keeps their message php-faithful.` |
|         - | 1953 | ` */` |
|   2977008 | 1954 | `static int VmBuiltinSelfValidatesArity(const char *zName)` |
|         5 | 1955 | `{` |
|         - | 1956 | `	static const char *const azSelf[] = { "rand", "mt_rand" };` |
|         - | 1957 | `	sxu32 i;` |
|   8917373 | 1958 | `	for( i = 0 ; i < SX_ARRAYSIZE(azSelf) ; i++ ){` |
|   5949469 | 1959 | `		sxu32 nSelf = SyStrlen(azSelf[i]);` |
|   5949469 | 1960 | `		if( SyStrlen(zName) == nSelf && SyStrncmp(zName,azSelf[i],nSelf) == 0 ){` |
|      9109 | 1961 | `			return 1;` |
|         - | 1962 | `		}` |
|   2970185 | 1963 | `	}` |
|   2967909 | 1964 | `	return 0;` |
|   1488509 | 1965 | `}` |
|         - | 1966 | `/*` |
|         - | 1967 | ` * One parameter of a declared signature, for the named-argument binder below.` |
|         - | 1968 | ` */` |
|         - | 1969 | `typedef struct VmSigParam VmSigParam;` |
|         - | 1970 | `struct VmSigParam` |
|         - | 1971 | `{` |
|         - | 1972 | `	const char *zName; int nName;   /* without the '$' */` |
|         - | 1973 | `	const char *zDef;  int nDef;    /* default TEXT, or 0 when the parameter is required */` |
|         - | 1974 | `	int bVariadic;` |
|         - | 1975 | `};` |
|         - | 1976 | `/*` |
|         - | 1977 | ` * Split a signature into its parameters: the NAME each one binds by and the default` |
|         - | 1978 | ` * TEXT to fall back on. The scan is VmDeriveArityFromSig's, kept apart because that one` |
|         - | 1979 | `` * only counts; a quoted default (`string $separator = ','`) hides a comma, which is why`` |
|         - | 1980 | ` * both go through VmSigSkipQuoted.` |
|         - | 1981 | ` */` |
|     57740 | 1982 | `static int VmSigParams(const char *zSig,VmSigParam *aOut,int nMax)` |
|         5 | 1983 | `{` |
|     57745 | 1984 | `	const char *zCur = zSig;` |
|     57745 | 1985 | `	const char *zStart = zSig;` |
|     57745 | 1986 | `	int n = 0;` |
|   2295961 | 1987 | `	for(;;){` |
|   4764617 | 1988 | `		if( zCur[0] == '\'' \|\| zCur[0] == '"' ){` |
|        19 | 1989 | `			zCur = VmSigSkipQuoted(zCur);` |
|        19 | 1990 | `			if( zCur[0] != '\0' ){` |
|        19 | 1991 | `				zCur++;` |
|         9 | 1992 | `			}` |
|        19 | 1993 | `			continue;` |
|         - | 1994 | `		}` |
|   4764599 | 1995 | `		if( zCur[0] == '\0' \|\| zCur[0] == ',' ){` |
|    230417 | 1996 | `			const char *z = zStart;` |
|    230417 | 1997 | `			const char *zEnd = zCur;` |
|    230417 | 1998 | `			if( n < nMax ){` |
|    230417 | 1999 | `				VmSigParam *p = &aOut[n];` |
|    230417 | 2000 | `				const char *zEq = 0;` |
|    230417 | 2001 | `				const char *zDollar = 0;` |
|    230417 | 2002 | `				p->zName = 0; p->nName = 0; p->zDef = 0; p->nDef = 0; p->bVariadic = 0;` |
|   4764677 | 2003 | `				for( ; z < zEnd ; z++ ){` |
|   4534265 | 2004 | `					if( z[0] == '$' && zDollar == 0 ){` |
|    230417 | 2005 | `						zDollar = z + 1;` |
|   4419059 | 2006 | `					}else if( z[0] == '=' && zEq == 0 ){` |
|     59517 | 2007 | `						zEq = z + 1;` |
|   4274097 | 2008 | `					}else if( z[0] == '.' && z + 2 < zEnd && z[1] == '.' && z[2] == '.' ){` |
|        93 | 2009 | `						p->bVariadic = 1;` |
|        44 | 2010 | `					}` |
|   2267135 | 2011 | `				}` |
|    230417 | 2012 | `				if( zDollar ){` |
|    230417 | 2013 | `					const char *zStop = zEq ? zEq - 1 : zEnd;` |
|    230417 | 2014 | `					const char *zN = zDollar;` |
|   1676161 | 2015 | `					while( zN < zStop && zN[0] != ' ' && zN[0] != '=' ){` |
|   1445749 | 2016 | `						zN++;` |
|         5 | 2017 | `					}` |
|    230417 | 2018 | `					p->zName = zDollar;` |
|    230417 | 2019 | `					p->nName = (int)(zN - zDollar);` |
|    115206 | 2020 | `				}` |
|    230417 | 2021 | `				if( zEq ){` |
|    119029 | 2022 | `					while( zEq < zEnd && zEq[0] == ' ' ){` |
|     59517 | 2023 | `						zEq++;` |
|         5 | 2024 | `					}` |
|     59517 | 2025 | `					p->zDef = zEq;` |
|     59517 | 2026 | `					p->nDef = (int)(zEnd - zEq);` |
|     59517 | 2027 | `					while( p->nDef > 0 && p->zDef[p->nDef-1] == ' ' ){` |
|       ! 0 | 2028 | `						p->nDef--;` |
|       ! 0 | 2029 | `					}` |
|     29756 | 2030 | `				}` |
|    230417 | 2031 | `				if( p->nName > 0 ){` |
|    230417 | 2032 | `					n++;` |
|    115206 | 2033 | `				}` |
|    115206 | 2034 | `			}` |
|    230417 | 2035 | `			if( zCur[0] == '\0' ){` |
|     57745 | 2036 | `				break;` |
|         - | 2037 | `			}` |
|    172677 | 2038 | `			zCur++;` |
|    172677 | 2039 | `			zStart = zCur;` |
|    172677 | 2040 | `			continue;` |
|         - | 2041 | `		}` |
|   4534187 | 2042 | `		zCur++;` |
|         5 | 2043 | `	}` |
|     57745 | 2044 | `	return n;` |
|         5 | 2045 | `}` |
|         - | 2046 | `/*` |
|         - | 2047 | ` * Materialize a signature default's TEXT into pOut. php's own stub values, which is a` |
|         - | 2048 | `` * small set: null, true/false, an integer or float, a quoted string, and `[]`. A default`` |
|         - | 2049 | `` * the table could not state (`= ?`, ~50 rows — §7.4) answers 0, and the caller then reports`` |
|         - | 2050 | ` * the parameter as not passed rather than inventing a value.` |
|         - | 2051 | ` */` |
|         6 | 2052 | `static int VmSigDefaultValue(ph7_vm *pVm,const VmSigParam *pParam,ph7_value *pOut)` |
|         1 | 2053 | `{` |
|         7 | 2054 | `	const char *z = pParam->zDef;` |
|         7 | 2055 | `	int n = pParam->nDef;` |
|         7 | 2056 | `	if( z == 0 \|\| n < 1 \|\| (n == 1 && z[0] == '?') ){` |
|         3 | 2057 | `		return 0;` |
|         - | 2058 | `	}` |
|         5 | 2059 | `	if( n == 4 && (SyStrnicmp(z,"null",4) == 0) ){` |
|       ! 0 | 2060 | `		PH7_MemObjRelease(pOut);` |
|       ! 0 | 2061 | `		return 1; /* a released value IS null */` |
|         - | 2062 | `	}` |
|         5 | 2063 | `	if( n == 4 && SyStrnicmp(z,"true",4) == 0 ){` |
|       ! 0 | 2064 | `		PH7_MemObjInitFromBool(pVm,pOut,1);` |
|       ! 0 | 2065 | `		return 1;` |
|         - | 2066 | `	}` |
|         5 | 2067 | `	if( n == 5 && SyStrnicmp(z,"false",5) == 0 ){` |
|       ! 0 | 2068 | `		PH7_MemObjInitFromBool(pVm,pOut,0);` |
|       ! 0 | 2069 | `		return 1;` |
|         - | 2070 | `	}` |
|         5 | 2071 | `	if( n == 2 && z[0] == '[' && z[1] == ']' ){` |
|         3 | 2072 | `		ph7_hashmap *pMap = PH7_NewHashmap(pVm,0,0);` |
|         3 | 2073 | `		if( pMap == 0 ){` |
|       ! 0 | 2074 | `			return 0;` |
|         - | 2075 | `		}` |
|         3 | 2076 | `		PH7_MemObjRelease(pOut);` |
|         3 | 2077 | `		pOut->x.pOther = pMap;` |
|         3 | 2078 | `		MemObjSetType(pOut,MEMOBJ_HASHMAP);` |
|         3 | 2079 | `		return 1;` |
|         - | 2080 | `	}` |
|         3 | 2081 | `	if( z[0] == '\'' \|\| z[0] == '"' ){` |
|         - | 2082 | `		SyString sStr;` |
|         3 | 2083 | `		SyStringInitFromBuf(&sStr,z + 1,n >= 2 ? n - 2 : 0);` |
|         3 | 2084 | `		PH7_MemObjInitFromString(pVm,pOut,&sStr);` |
|         3 | 2085 | `		return 1;` |
|         - | 2086 | `	}` |
|       ! 0 | 2087 | `	if( z[0] == '-' \|\| z[0] == '+' \|\| (z[0] >= '0' && z[0] <= '9') ){` |
|         - | 2088 | `		SyString sNum;` |
|       ! 0 | 2089 | `		SyStringInitFromBuf(&sNum,z,(sxu32)n);` |
|       ! 0 | 2090 | `		if( PH7_MemObjInitFromString(pVm,pOut,&sNum) != SXRET_OK ){` |
|       ! 0 | 2091 | `			return 0;` |
|         - | 2092 | `		}` |
|       ! 0 | 2093 | `		PH7_MemObjToNumeric(pOut);` |
|       ! 0 | 2094 | `		return 1;` |
|         - | 2095 | `	}` |
|       ! 0 | 2096 | `	return 0; /* a constant expression (M_PI, PHP_ROUND_HALF_UP, …): not evaluated here */` |
|         4 | 2097 | `}` |
|         - | 2098 | `/*` |
|         - | 2099 | ` * Bind a call's NAMED arguments to the callee's declared parameter POSITIONS.` |
|         - | 2100 | ` *` |
|         - | 2101 | ` * A compiled function does this from its parameter records (VmResolveNamedArgs); a host` |
|         - | 2102 | ` * function and a native method have none, so every named argument was simply passed in the` |
|         - | 2103 | `` * order it was WRITTEN. `str_pad(length: 5, string: "x")` reached the builtin as`` |
|         - | 2104 | ` * ("x" at #2, 5 at #1) and reported a TypeError, and — worse, because it is silent —` |
|         - | 2105 | `` * `str_pad("x", 5, pad_type: STR_PAD_LEFT)` bound the flag as $pad_string and answered`` |
|         - | 2106 | ` * "x0000" where php answers "    x". Both spellings are php 8.0 syntax, and the whole` |
|         - | 2107 | ` * ~650-builtin surface plus every native method was affected.` |
|         - | 2108 | ` *` |
|         - | 2109 | ` * The declared signature is the source of names, defaults and positions — the same string` |
|         - | 2110 | ` * Reflection prints. Rewrites *pnArg / apArg in place (the caller's argument vector is` |
|         - | 2111 | ` * scratch it owns) and answers SXRET_OK, or throws php's Error and returns its status.` |
|         - | 2112 | ` * Callees with a VARIADIC tail are left alone: php collects extra named arguments into it` |
|         - | 2113 | ` * by NAME, which the positional vector here cannot express.` |
|         - | 2114 | ` */` |
|        94 | 2115 | `PH7_PRIVATE sxi32 PH7_VmBindNamedArgsToSig(` |
|         - | 2116 | `	ph7_context *pCtx,      /* Call context (for the throws) */` |
|         - | 2117 | `	ph7_user_func *pFunc,   /* Callee: its zSig names the parameters */` |
|         - | 2118 | `	VmCallArgMap *pMap,     /* Call-site map; its aNames[] are per ACTUAL slot */` |
|         - | 2119 | `	int *pnArg,             /* IN/OUT: argument count */` |
|         - | 2120 | `	ph7_value **apArg       /* IN/OUT: argument vector */` |
|         - | 2121 | `	)` |
|         2 | 2122 | `{` |
|         - | 2123 | `	/* php's own stubs top out well under this; a signature with more parameters simply` |
|         - | 2124 | `	 * keeps the positional binding it had. */` |
|         - | 2125 | `#define VM_SIG_MAX_PARAM 32` |
|         - | 2126 | `	VmSigParam aParam[VM_SIG_MAX_PARAM];` |
|         - | 2127 | `	ph7_value *apBound[VM_SIG_MAX_PARAM];` |
|         - | 2128 | `	int nParam,nArg,i,nLast;` |
|        96 | 2129 | `	if( pFunc == 0 \|\| pFunc->zSig == 0 \|\| pMap == 0 \|\| pMap->bHasNamed == 0 ){` |
|        13 | 2130 | `		return SXRET_OK;` |
|         - | 2131 | `	}` |
|        84 | 2132 | `	nArg = *pnArg;` |
|        84 | 2133 | `	if( nArg < 1 \|\| nArg > VM_SIG_MAX_PARAM ){` |
|       ! 0 | 2134 | `		return SXRET_OK;` |
|         - | 2135 | `	}` |
|        84 | 2136 | `	nParam = VmSigParams(pFunc->zSig,aParam,VM_SIG_MAX_PARAM);` |
|        84 | 2137 | `	if( nParam < 1 \|\| aParam[nParam-1].bVariadic ){` |
|        21 | 2138 | `		return SXRET_OK;` |
|         - | 2139 | `	}` |
|       252 | 2140 | `	for( i = 0 ; i < nParam ; ++i ){` |
|       190 | 2141 | `		apBound[i] = 0;` |
|        96 | 2142 | `	}` |
|        64 | 2143 | `	nLast = -1;` |
|       198 | 2144 | `	for( i = 0 ; i < nArg ; ++i ){` |
|       142 | 2145 | `		int p = i;` |
|       202 | 2146 | `		if( i < (int)pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|       128 | 2147 | `			SyString *pName = &pMap->aNames[i];` |
|       246 | 2148 | `			for( p = 0 ; p < nParam ; ++p ){` |
|       240 | 2149 | `				if( (int)pName->nByte == aParam[p].nName` |
|       199 | 2150 | `				 && SyMemcmp(pName->zString,aParam[p].zName,pName->nByte) == 0 ){` |
|       124 | 2151 | `					break;` |
|         - | 2152 | `				}` |
|        60 | 2153 | `			}` |
|       128 | 2154 | `			if( p >= nParam ){` |
|         7 | 2155 | `				return PH7_VmThrowException(pCtx,"Error",` |
|         2 | 2156 | `					"Unknown named parameter $%z",pName);` |
|         - | 2157 | `			}` |
|       124 | 2158 | `			if( apBound[p] ){` |
|         4 | 2159 | `				return PH7_VmThrowException(pCtx,"Error",` |
|         1 | 2160 | `					"Named parameter $%z overwrites previous argument",pName);` |
|         2 | 2161 | `			}` |
|        75 | 2162 | `		}else if( p >= nParam ){` |
|       ! 0 | 2163 | `			return SXRET_OK; /* more positional arguments than the signature knows */` |
|         - | 2164 | `		}` |
|       136 | 2165 | `		apBound[p] = apArg[i];` |
|       136 | 2166 | `		if( p > nLast ){` |
|       114 | 2167 | `			nLast = p;` |
|        56 | 2168 | `		}` |
|        69 | 2169 | `	}` |
|       188 | 2170 | `	for( i = 0 ; i <= nLast ; ++i ){` |
|       134 | 2171 | `		if( apBound[i] == 0 ){` |
|         7 | 2172 | `			ph7_value *pDef = ph7_context_new_scalar(pCtx);` |
|         7 | 2173 | `			if( pDef == 0 \|\| !VmSigDefaultValue(pCtx->pVm,&aParam[i],pDef) ){` |
|         - | 2174 | `				SyString sName;` |
|         3 | 2175 | `				SyStringInitFromBuf(&sName,aParam[i].zName,(sxu32)aParam[i].nName);` |
|         4 | 2176 | `				return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|         1 | 2177 | `					"%z(): Argument #%d ($%z) not passed",&pFunc->sName,i + 1,&sName);` |
|         - | 2178 | `			}` |
|         5 | 2179 | `			apBound[i] = pDef;` |
|         2 | 2180 | `		}` |
|        67 | 2181 | `	}` |
|       184 | 2182 | `	for( i = 0 ; i <= nLast ; ++i ){` |
|       130 | 2183 | `		apArg[i] = apBound[i];` |
|        66 | 2184 | `	}` |
|        56 | 2185 | `	*pnArg = nLast + 1;` |
|        56 | 2186 | `	return SXRET_OK;` |
|        49 | 2187 | `}` |
|         - | 2188 | `/*` |
|         - | 2189 | ` * Name the Nth (0-based) parameter of a declared signature, without the '$'.` |
|         - | 2190 | ` *` |
|         - | 2191 | ` * The signature string is the only place a host function's parameter names live, and` |
|         - | 2192 | `` * php puts them in diagnostics — `sort(): Argument #1 ($array) …`. Answers 0 when the`` |
|         - | 2193 | ` * signature has no such parameter (or none with a name).` |
|         - | 2194 | ` */` |
|        12 | 2195 | `PH7_PRIVATE int PH7_VmSigParamName(const char *zSig,int nPos,SyString *pOut)` |
|         2 | 2196 | `{` |
|         - | 2197 | `	VmSigParam aParam[VM_SIG_MAX_PARAM];` |
|         - | 2198 | `	int nParam;` |
|        14 | 2199 | `	if( zSig == 0 \|\| nPos < 0 \|\| nPos >= VM_SIG_MAX_PARAM ){` |
|       ! 0 | 2200 | `		return 0;` |
|         - | 2201 | `	}` |
|        14 | 2202 | `	nParam = VmSigParams(zSig,aParam,VM_SIG_MAX_PARAM);` |
|        14 | 2203 | `	if( nPos >= nParam \|\| aParam[nPos].nName < 1 ){` |
|       ! 0 | 2204 | `		return 0;` |
|         - | 2205 | `	}` |
|        14 | 2206 | `	SyStringInitFromBuf(pOut,aParam[nPos].zName,(sxu32)aParam[nPos].nName);` |
|        14 | 2207 | `	return 1;` |
|         8 | 2208 | `}` |
|         - | 2209 | `/*` |
|         - | 2210 | `` * A `&` in a builtin's signature is not always php's ZEND_SEND_ARG_BY_REF.`` |
|         - | 2211 | ` *` |
|         - | 2212 | ` * php has a second mode, ZEND_SEND_PREFER_REF: bind by reference when the argument IS a` |
|         - | 2213 | ` * variable, and otherwise take it by value without a word. Reflection prints those` |
|         - | 2214 | ` * parameters as by-reference like any other and PHL's signature string cannot say which` |
|         - | 2215 | `` * mode a `&` means, so the two are told apart here. Probed value-for-value against php`` |
|         - | 2216 | `` * 8.5 over every `&` row PHL declares (41 of them): all but extract() refuse a`` |
|         - | 2217 | `` * non-variable, and extract() answers `int(1)` for `extract(['q' => 1])`.`` |
|         - | 2218 | ` *` |
|         - | 2219 | ` * array_multisort() is listed with it because it is php's other prefer-ref builtin and` |
|         - | 2220 | ` * PHL will need this the day it gains one (it is a MISSING builtin today, §5).` |
|         - | 2221 | ` */` |
|     57806 | 2222 | `PH7_PRIVATE int VmBuiltinPrefersRef(SyString *pName)` |
|         5 | 2223 | `{` |
|         - | 2224 | `	static const char *const azPreferRef[] = { "extract", "array_multisort" };` |
|         - | 2225 | `	sxu32 i;` |
|    173227 | 2226 | `	for( i = 0 ; i < SX_ARRAYSIZE(azPreferRef) ; ++i ){` |
|    115537 | 2227 | `		sxu32 nByte = SyStrlen(azPreferRef[i]);` |
|    115532 | 2228 | `		if( pName->nByte == nByte` |
|     57867 | 2229 | `		 && SyMemcmp(pName->zString,azPreferRef[i],nByte) == 0 ){` |
|       120 | 2230 | `			return 1;` |
|         - | 2231 | `		}` |
|     57713 | 2232 | `	}` |
|     57695 | 2233 | `	return 0;` |
|     28908 | 2234 | `}` |
|         - | 2235 | `/*` |
|         - | 2236 | ` * php refuses a by-reference argument at the CALL, before the callee's ZPP runs, and it` |
|         - | 2237 | ``  * decides from the argument's SHAPE, not from its value: `sort([3,1])`, `usort('x',$cb)` `` |
|         - | 2238 | `` * and `preg_match($p,$s,'lit')` are all`` |
|         - | 2239 | `` * `Error: sort(): Argument #1 ($array) could not be passed by reference`.`` |
|         - | 2240 | ` *` |
|         - | 2241 | ` * The call site's compile-time shape mask (VmCallArgMap.nNonLvalMask) is what says so.` |
|         - | 2242 | ` * Only five builtins raised anything before this, from their own bodies, on the runtime` |
|         - | 2243 | `` * `nIdx == SXU32_HIGH` signal — which cannot tell a literal from the result of a call, a`` |
|         - | 2244 | `` * shape php ACCEPTS with a notice. The thirty other `&` rows answered `true`/`false`/an`` |
|         - | 2245 | ` * int: the same answers they give for work they really did.` |
|         - | 2246 | ` *` |
|         - | 2247 | ` * Skipped when the call site has no shape mask (a spread, an indirect dispatch through` |
|         - | 2248 | ` * call_user_func, an engine-synthesized call) or uses named arguments (which rebind` |
|         - | 2249 | ` * positions the mask is indexed by). The by-ref positions come from the same declared` |
|         - | 2250 | ` * signature everything else here reads.` |
|         - | 2251 | ` */` |
|   2866728 | 2252 | `PH7_PRIVATE sxi32 PH7_VmScreenByRefArgShapes(` |
|         - | 2253 | `	ph7_context *pCtx,     /* Call context (for the throw) */` |
|         - | 2254 | `	ph7_user_func *pFunc,  /* Callee: its zSig names and marks the parameters */` |
|         - | 2255 | `	VmCallArgMap *pMap,    /* Call-site map, or 0 */` |
|         - | 2256 | `	int nGiven,            /* Argument count */` |
|         - | 2257 | `	ph7_value **apArg      /* Arguments */` |
|         - | 2258 | `	)` |
|         5 | 2259 | `{` |
|         - | 2260 | `	VmSigParam aParam[VM_SIG_MAX_PARAM];` |
|         - | 2261 | `	int nParam,n;` |
|         - | 2262 | `	/* The by-ref mask first: it is 0 for all but 41 of the ~650 host functions, so` |
|         - | 2263 | `	 * every other call leaves through one test. */` |
|   2866733 | 2264 | `	if( pFunc == 0 \|\| pFunc->nByRefMask == 0 \|\| pFunc->zSig == 0 \|\| nGiven < 1 ){` |
|   2806912 | 2265 | `		return SXRET_OK;` |
|         - | 2266 | `	}` |
|     59826 | 2267 | `	if( pMap == 0 \|\| !pMap->bArgShapes \|\| pMap->bHasNamed ){` |
|        27 | 2268 | `		return SXRET_OK;` |
|         - | 2269 | `	}` |
|     59802 | 2270 | `	if( (pMap->nNonLvalMask \| pMap->nTempCallMask) == 0 ){` |
|      2044 | 2271 | `		return SXRET_OK;` |
|         - | 2272 | `	}` |
|     57763 | 2273 | `	if( VmBuiltinPrefersRef(&pFunc->sName) ){` |
|       116 | 2274 | `		return SXRET_OK;` |
|         - | 2275 | `	}` |
|     57651 | 2276 | `	nParam = VmSigParams(pFunc->zSig,aParam,VM_SIG_MAX_PARAM);` |
|    229591 | 2277 | `	for( n = 0 ; n < nGiven && n < 31 ; ++n ){` |
|    171995 | 2278 | `		if( (pFunc->nByRefMask & (1u << n)) == 0 ){` |
|    170649 | 2279 | `			continue;` |
|         - | 2280 | `		}` |
|      1351 | 2281 | `		if( (pMap->nNonLvalMask & (1u << n)) == 0 ){` |
|         - | 2282 | `			/* Not a refusal — but a CALL result in this position is php's notice,` |
|         - | 2283 | `			 * and then the builtin operates on the temporary. */` |
|      1301 | 2284 | `			PH7_VmArgTempCallNotice(pCtx->pVm,pMap,(sxu32)n,apArg[n]);` |
|      1301 | 2285 | `			continue;` |
|         - | 2286 | `		}` |
|        55 | 2287 | `		if( n < nParam && aParam[n].nName > 0 ){` |
|        80 | 2288 | `			return PH7_VmThrowException(pCtx,"Error",` |
|         - | 2289 | `				"%z(): Argument #%d ($%.*s) could not be passed by reference",` |
|        25 | 2290 | `				&pFunc->sName,n + 1,aParam[n].nName,aParam[n].zName);` |
|         - | 2291 | `		}` |
|       ! 0 | 2292 | `		return PH7_VmThrowException(pCtx,"Error",` |
|         - | 2293 | `			"%z(): Argument #%d could not be passed by reference",` |
|       ! 0 | 2294 | `			&pFunc->sName,n + 1);` |
|       ! 0 | 2295 | `	}` |
|     57601 | 2296 | `	return SXRET_OK;` |
|   1434194 | 2297 | `}` |
|         - | 2298 | `/*` |
|         - | 2299 | ` * D1: derive a by-reference position bitmask from a php-style signature string.` |
|         - | 2300 | `` * Bit N is set when positional parameter N is declared by-reference (a `&` appears`` |
|         - | 2301 | `` * anywhere in that comma-separated parameter, e.g. `&$matches`, `&...$vars`). Only`` |
|         - | 2302 | ` * the first 31 positions are representable; a by-ref parameter past that is rare` |
|         - | 2303 | ` * for a builtin and simply not tracked here. The scan mirrors VmDeriveArityFromSig.` |
|         - | 2304 | ` */` |
|   6898460 | 2305 | `PH7_PRIVATE sxu32 VmDeriveByRefMaskFromSig(const char *zSig)` |
|         5 | 2306 | `{` |
|   6898465 | 2307 | `	sxu32 mask = 0;` |
|   6898465 | 2308 | `	int n = 0;       /* current parameter index */` |
|   6898465 | 2309 | `	int bSeen = 0;   /* current parameter has non-space content */` |
|   6898465 | 2310 | ``	int bRef = 0;    /* current parameter carries a by-ref `&` */`` |
|   6898465 | 2311 | ``	int bVar = 0;    /* current parameter is a `...` variadic */`` |
|   6898465 | 2312 | `	int bTailRef = 0;/* the LAST parameter was a by-ref variadic */` |
|   6898465 | 2313 | `	const char *zCur = zSig;` |
|  66644242 | 2314 | `	for(;;){` |
| 136941999 | 2315 | `		if( zCur[0] == '\'' \|\| zCur[0] == '"' ){` |
|    221671 | 2316 | `			bSeen = 1;` |
|    221671 | 2317 | `			zCur = VmSigSkipQuoted(zCur);` |
|    221671 | 2318 | `			if( zCur[0] != '\0' ){` |
|    221671 | 2319 | `				zCur++;` |
|    110833 | 2320 | `			}` |
|    221671 | 2321 | `			continue;` |
|         - | 2322 | `		}` |
| 136720333 | 2323 | `		if( zCur[0] == '\0' \|\| zCur[0] == ',' ){` |
|  10330309 | 2324 | `			if( bSeen ){` |
|   7589589 | 2325 | `				if( bRef && n < 31 ){` |
|    269167 | 2326 | `					mask \|= (1u << n);` |
|    134581 | 2327 | `				}` |
|   7589589 | 2328 | `				bTailRef = (bRef && bVar);` |
|   7589589 | 2329 | `				n++;` |
|   3794792 | 2330 | `			}` |
|  10330309 | 2331 | `			if( zCur[0] == '\0' ){` |
|   6898465 | 2332 | `				break;` |
|         - | 2333 | `			}` |
|   3431849 | 2334 | `			bSeen = bRef = bVar = 0;` |
|   3431849 | 2335 | `			zCur++;` |
|   3431849 | 2336 | `			continue;` |
|         - | 2337 | `		}` |
| 126390029 | 2338 | `		if( zCur[0] != ' ' ){` |
| 110555101 | 2339 | `			bSeen = 1;` |
|  55277548 | 2340 | `		}` |
| 126390029 | 2341 | `		if( zCur[0] == '&' ){` |
|    269167 | 2342 | `			bRef = 1;` |
|    134581 | 2343 | `		}` |
| 126390029 | 2344 | `		if( zCur[0] == '.' && zCur[1] == '.' && zCur[2] == '.' ){` |
|         - | 2345 | ``			/* A `...` tail, not a numeric default's decimal point. */`` |
|    185055 | 2346 | `			bVar = 1;` |
|     92525 | 2347 | `		}` |
| 126390029 | 2348 | `		zCur++;` |
|         5 | 2349 | `	}` |
|   6898465 | 2350 | `	if( bTailRef && n > 0 && n <= 31 ){` |
|         - | 2351 | ``		/* A by-ref `&...` tail absorbs every later actual (array_multisort's`` |
|         - | 2352 | ``		 * `&...$rest`): without this, the deferred-argument resolver read the`` |
|         - | 2353 | ``		 * tail positions as by-VALUE and warned `Undefined variable` on an`` |
|         - | 2354 | `		 * undefined actual php binds silently. */` |
|      4557 | 2355 | `		mask \|= ~((1u << (n - 1)) - 1u);` |
|      2276 | 2356 | `	}` |
|   6898465 | 2357 | `	return mask;` |
|         5 | 2358 | `}` |
|      4552 | 2359 | `PH7_PRIVATE void VmSetBuiltinSignatures(ph7_vm *pVm)` |
|         5 | 2360 | `{` |
|         - | 2361 | `	sxu32 n;` |
|   3027085 | 2362 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinSig) ; n++ ){` |
|   4533797 | 2363 | `		SyHashEntry *pEntry = SyHashGet(&pVm->hHostFunction,` |
|   3022528 | 2364 | `			(const void *)aBuiltinSig[n].zName,(sxu32)SyStrlen(aBuiltinSig[n].zName));` |
|   3022533 | 2365 | `		if( pEntry ){` |
|   2977013 | 2366 | `			ph7_user_func *pFunc = (ph7_user_func *)pEntry->pUserData;` |
|   2977013 | 2367 | `			sxi16 nMin = 0, nMax = 0;` |
|   2977013 | 2368 | `			sxu8 bAtLeast = 0, bHasMax = 0;` |
|   2977013 | 2369 | `			pFunc->zSig = aBuiltinSig[n].zSig;` |
|   2977013 | 2370 | `			pFunc->zRet = aBuiltinSig[n].zRet[0] ? aBuiltinSig[n].zRet : 0;` |
|   2977013 | 2371 | `			pFunc->nByRefMask = VmDeriveByRefMaskFromSig(pFunc->zSig);` |
|   2977013 | 2372 | `			VmDeriveArityFromSig(pFunc->zSig,&nMin,&bAtLeast,&nMax,&bHasMax);` |
|         - | 2373 | `			/* The MAXIMUM always comes from the signature: the curated override` |
|         - | 2374 | `			 * table speaks only to the minimum (and its wording). */` |
|   2977013 | 2375 | `			pFunc->nMaxArg = nMax;` |
|   2977013 | 2376 | `			pFunc->bHasMaxArg = (sxu8)(VmBuiltinSelfValidatesArity(aBuiltinSig[n].zName) ? 0 : bHasMax);` |
|   2977013 | 2377 | `			if( pFunc->nMinArg < 1 ){` |
|         - | 2378 | `				/* VmSetBuiltinArity() ran first: a non-zero minimum here means the` |
|         - | 2379 | `				 * curated override already spoke for this builtin, so leave it. */` |
|   1702453 | 2380 | `				pFunc->nMinArg = nMin;` |
|   1702453 | 2381 | `				pFunc->bAtLeast = bAtLeast;` |
|    851224 | 2382 | `			}` |
|   1488504 | 2383 | `		}` |
|   1511269 | 2384 | `	}` |
|      4557 | 2385 | `}` |
|         - | 2386 | `/*` |
|         - | 2387 | ` * Signature lookup by function name, for the reflection layer: embedded-PHP` |
|         - | 2388 | ` * builtins (max/min/Exception methods...) are ph7_vm_func instances, not` |
|         - | 2389 | ` * host functions, so they miss the VmSetBuiltinSignatures stamping and pull` |
|         - | 2390 | ` * their row on demand here. Linear scan — reflection-path only.` |
|         - | 2391 | ` */` |
|       ! 0 | 2392 | `PH7_PRIVATE const char * PH7_VmBuiltinSigLookup(const char *zName,sxu32 nLen,const char **pzRet)` |
|       ! 0 | 2393 | `{` |
|         - | 2394 | `	sxu32 n;` |
|       ! 0 | 2395 | `	if( pzRet ){` |
|       ! 0 | 2396 | `		*pzRet = 0;` |
|       ! 0 | 2397 | `	}` |
|       ! 0 | 2398 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinSig) ; n++ ){` |
|       ! 0 | 2399 | `		if( SyStrlen(aBuiltinSig[n].zName) == nLen` |
|       ! 0 | 2400 | `		 && SyMemcmp(aBuiltinSig[n].zName,zName,nLen) == 0 ){` |
|       ! 0 | 2401 | `			if( pzRet && aBuiltinSig[n].zRet[0] ){` |
|       ! 0 | 2402 | `				*pzRet = aBuiltinSig[n].zRet;` |
|       ! 0 | 2403 | `			}` |
|       ! 0 | 2404 | `			return aBuiltinSig[n].zSig;` |
|         - | 2405 | `		}` |
|       ! 0 | 2406 | `	}` |
|       ! 0 | 2407 | `	return 0;` |
|       ! 0 | 2408 | `}` |
|         - | 2409 | `/*` |
|         - | 2410 | ` * Write a value back to the caller's variable through a builtin argument's` |
|         - | 2411 | ` * stack-slot nIdx — the shared write-back for builtin by-reference` |
|         - | 2412 | ` * out-parameters (preg_match $matches, preg_replace &$count, similar_text` |
|         - | 2413 | ` * &$percent, ...).` |
|         - | 2414 | ` *` |
|         - | 2415 | ` * For a positional out-param argument the call compiler auto-vivifies known` |
|         - | 2416 | ` * by-reference out-params (see GenStateByRefBuiltinMask in compile.c), so a` |
|         - | 2417 | ` * bare undefined variable, an array subscript, and a declared/untyped` |
|         - | 2418 | ` * property all arrive with a real nIdx and are written back here, matching` |
|         - | 2419 | ` * PHP's reference semantics.` |
|         - | 2420 | ` *` |
|         - | 2421 | ` * nIdx stays SXU32_HIGH and the write-back to the caller is skipped (the` |
|         - | 2422 | ` * value still lands in the local stack slot) when the argument cannot expose` |
|         - | 2423 | ` * a stable memobj slot: a literal, a function-call result, a subscript of a` |
|         - | 2424 | ` * non-lvalue parent (foo()['k']), or any variable in a call that also uses` |
|         - | 2425 | ` * named or spread arguments (compile-time positions no longer map to the` |
|         - | 2426 | ` * runtime arg slots, so the compiler conservatively does not vivify). An` |
|         - | 2427 | ` * uninitialized typed property is also not wired (it throws before the` |
|         - | 2428 | ` * write) -- see the recorded deferrals.` |
|         - | 2429 | ` */` |
|       860 | 2430 | `PH7_PRIVATE void PH7_VmStoreArgByRef(ph7_vm *pVm,ph7_value *pArg,ph7_value *pNewVal)` |
|         5 | 2431 | `{` |
|       865 | 2432 | `	if( pArg->nIdx != SXU32_HIGH ){` |
|       817 | 2433 | `		ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pArg->nIdx);` |
|       817 | 2434 | `		if( pObj ){` |
|       817 | 2435 | `			PH7_MemObjStore(pNewVal,pObj);` |
|       406 | 2436 | `		}` |
|       406 | 2437 | `	}` |
|       865 | 2438 | `	PH7_MemObjStore(pNewVal,pArg);` |
|       865 | 2439 | `}` |
|         - | 2440 | `/*` |
|         - | 2441 | ` * Raise an E_WARNING whose text is used VERBATIM.` |
|         - | 2442 | ` * ph7_context_throw_error_format() prepends "func(): " to whatever it is given, but php's` |
|         - | 2443 | ` * IO warnings put the offending path inside those parens -- "file_get_contents(/nope):` |
|         - | 2444 | ` * Failed to open stream: ..." -- so a caller that needs php's exact shape must build the` |
|         - | 2445 | ` * whole line itself and come through here.` |
|         - | 2446 | ` */` |
|         - | 2447 | `/*` |
|         - | 2448 | ` * Validate a callback argument and throw php's TypeError when it cannot be called.` |
|         - | 2449 | ` *` |
|         - | 2450 | ` * The shared dispatcher (PH7_VmCallUserFunctionWithMap) answers SXRET_OK with a NULL` |
|         - | 2451 | ` * result for an unresolvable callable, so every caller that did not check first failed` |
|         - | 2452 | ` * SILENTLY: call_user_func() returned NULL and usort() left the array sorted by nothing` |
|         - | 2453 | ` * at all. php rejects the ARGUMENT up front, naming exactly why.` |
|         - | 2454 | ` */` |
|      8062 | 2455 | `PH7_PRIVATE sxi32 PH7_CheckCallbackArg(` |
|         - | 2456 | `	ph7_context *pCtx,   /* Calling context (names the function in the message) */` |
|         - | 2457 | `	ph7_value *pCb,      /* The callback argument */` |
|         - | 2458 | `	int iArg,            /* Its 1-based position */` |
|         - | 2459 | `	const char *zParam,  /* Its php parameter name ("callback"), or 0 for the variadic` |
|         - | 2460 | `	                      * comparators php names by position only (array_udiff …) */` |
|         - | 2461 | `	int bNullable        /* TRUE when php's text says "or null" */` |
|         - | 2462 | `	)` |
|         5 | 2463 | `{` |
|         - | 2464 | `	char zReason[256];` |
|      8067 | 2465 | `	const char *zWhy = PH7_VmCallableReason(pCtx->pVm,pCb,zReason,sizeof(zReason));` |
|      8067 | 2466 | `	if( zWhy == 0 ){` |
|      7849 | 2467 | `		return PH7_OK;` |
|         - | 2468 | `	}` |
|       223 | 2469 | `	if( zParam ){` |
|       251 | 2470 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 2471 | `			"%s(): Argument #%d ($%s) must be a valid callback%s, %s",` |
|        82 | 2472 | `			ph7_function_name(pCtx),iArg,zParam,bNullable ? " or null" : "",zWhy);` |
|         - | 2473 | `	}` |
|        86 | 2474 | `	return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 2475 | `		"%s(): Argument #%d must be a valid callback%s, %s",` |
|        27 | 2476 | `		ph7_function_name(pCtx),iArg,bNullable ? " or null" : "",zWhy);` |
|      4036 | 2477 | `}` |
|     25084 | 2478 | `PH7_PRIVATE void PH7_VmThrowWarningFmt(ph7_vm *pVm,const char *zFmt,...)` |
|         5 | 2479 | `{` |
|         - | 2480 | `	va_list ap;` |
|     25089 | 2481 | `	va_start(ap,zFmt);` |
|     25089 | 2482 | `	PH7_VmThrowErrorAp(pVm,0,PH7_CTX_WARNING,zFmt,ap);` |
|     25089 | 2483 | `	va_end(ap);` |
|     25089 | 2484 | `}` |
|         - | 2485 | `/*` |
|         - | 2486 | ` * Emit a formatted E_USER_DEPRECATED diagnostic with no function-name prefix:` |
|         - | 2487 | ` * php reports #[\Deprecated] as USER-deprecated (16384, it is userland-authored),` |
|         - | 2488 | ` * not the engine's E_DEPRECATED (8192) — handler-visible errno matters.` |
|         - | 2489 | ` */` |
|        34 | 2490 | `static void VmThrowUserDeprecatedFmt(ph7_vm *pVm,const char *zFmt,...)` |
|         1 | 2491 | `{` |
|         - | 2492 | `	va_list ap;` |
|        35 | 2493 | `	va_start(ap,zFmt);` |
|        35 | 2494 | `	PH7_VmThrowErrorAp(pVm,0,E_USER_DEPRECATED,zFmt,ap);` |
|        35 | 2495 | `	va_end(ap);` |
|        35 | 2496 | `}` |
|         - | 2497 | `/*` |
|         - | 2498 | ` * php 8.4 #[\Deprecated]: emit the E_USER_DEPRECATED notice for a call to a user` |
|         - | 2499 | ` * function/method carrying the attribute. Message shapes (php-exact):` |
|         - | 2500 | ` *   Function f() is deprecated` |
|         - | 2501 | ` *   Method C::m() is deprecated since 2.0, use g() instead` |
|         - | 2502 | ` * ("since" from the attribute's since: argument; the trailing ", msg" from` |
|         - | 2503 | ` * message:/positional #1.) The attribute arguments are compiled constant` |
|         - | 2504 | ` * expressions — evaluated via VmLocalExec, the reflection thunks' pattern.` |
|         - | 2505 | ` * Called from OP_CALL once per call, only when the callee HAS attributes;` |
|         - | 2506 | ` * pDeclClass names the method's declaring class (0 for plain functions).` |
|         - | 2507 | ` */` |
|         - | 2508 | `/*` |
|         - | 2509 | ` * Expand a global constant's value, emitting the php 8.5 #[\Deprecated]` |
|         - | 2510 | ` * E_USER_DEPRECATED notice first when the constant carries attributes` |
|         - | 2511 | `` * (`Constant GD is deprecated since 1.2` — every access re-warns).`` |
|         - | 2512 | ` */` |
|         - | 2513 | `static void VmDeprecatedAttrNoticeSubject(ph7_vm *pVm,SySet *pAttrs,` |
|         - | 2514 | `	const char *zKind,const SyString *pQual,const SyString *pName);` |
|         - | 2515 | `/*` |
|         - | 2516 | ` * Expand a constant, emitting the php 8.5 #[\Deprecated] E_USER_DEPRECATED notice` |
|         - | 2517 | `` * first when a USERLAND constant carries the attribute (`Constant GD is deprecated`` |
|         - | 2518 | `` * since 1.2` — every access re-warns). Engine constants php merely deprecates are`` |
|         - | 2519 | ` * not mimicked: PHL removes them outright (see the scope policy), so there is no` |
|         - | 2520 | ` * engine-side E_DEPRECATED list here.` |
|         - | 2521 | ` *` |
|         - | 2522 | `` * A `const NAME = <expr>;` statement compiles its initializer to a bytecode`` |
|         - | 2523 | ` * program and PH7_VmExpandConstantValue RUNS it — so the value was re-computed` |
|         - | 2524 | ` * on EVERY read. For anything with an identity or a side effect that is a wrong` |
|         - | 2525 | `` * answer, not a slow one: `const C = new Foo();` gave a DIFFERENT object each`` |
|         - | 2526 | `` * time (`C === C` was false, and `Foo::$count` counted one construction per`` |
|         - | 2527 | ` * read) where php evaluates the initializer once and hands the same value out` |
|         - | 2528 | ` * for ever. The first successful expansion is kept, and the constant becomes an` |
|         - | 2529 | ` * ordinary value-backed one — exactly the shape define() registers, so` |
|         - | 2530 | ` * redefinition frees it through the path that already existed.` |
|         - | 2531 | ` *` |
|         - | 2532 | ` * Not cached when the initializer did not complete: a throw, an exit(), or a` |
|         - | 2533 | ` * MUTED evaluation (php has not reached this code, so nothing may be observable)` |
|         - | 2534 | ` * must all be retried rather than frozen into a half-built value.` |
|         - | 2535 | ` */` |
|    168873 | 2536 | `PH7_PRIVATE sxi32 VmExpandConstantOnce(ph7_vm *pVm,ph7_constant *pCons,ph7_value *pOut)` |
|         5 | 2537 | `{` |
|         - | 2538 | `	const void *pResumeBefore,*pInlineBefore;` |
|         - | 2539 | `	sxi32 rc;` |
|    168878 | 2540 | `	if( pCons->xExpand != PH7_VmExpandConstantValue ){` |
|    168716 | 2541 | `		pCons->xExpand(pOut,pCons->pUserData);` |
|    168716 | 2542 | `		return SXRET_OK;` |
|         - | 2543 | `	}` |
|         - | 2544 | `	/* The initializer's own status. PH7_VmExpandConstantValue drops VmLocalExec's` |
|         - | 2545 | `	 * return code (ProcConstant answers void), so the program is driven from here` |
|         - | 2546 | `	 * instead — a caller with no way to see a throw would otherwise cache a` |
|         - | 2547 | `	 * half-built value and keep running past it. */` |
|       167 | 2548 | `	pResumeBefore = (const void *)pVm->pResumeFrame;` |
|       167 | 2549 | `	pInlineBefore = (const void *)pVm->pInlineInstr;` |
|       167 | 2550 | `	rc = VmLocalExec(pVm,(SySet *)pCons->pUserData,pOut,FALSE);` |
|       162 | 2551 | `	if( pVm->nMuteThrow > 0 \|\| rc == PH7_ABORT` |
|       141 | 2552 | `	 \|\| VmLocalExecThrew(pVm,rc,pResumeBefore,pInlineBefore) ){` |
|         - | 2553 | `		/* Did not complete — a throw, an exit(), or a MUTED evaluation (php has` |
|         - | 2554 | `		 * not reached this code, so nothing may be observable). Retry it next` |
|         - | 2555 | `		 * time rather than freezing a value the initializer never produced:` |
|         - | 2556 | ``		 * `const A = LATER; …; define('LATER',5);` must still answer 5. */`` |
|        35 | 2557 | `		return rc == SXRET_OK ? PH7_EXCEPTION : rc;` |
|         - | 2558 | `	}` |
|         - | 2559 | `	{` |
|       135 | 2560 | `		ph7_value *pKeep = (ph7_value *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|       135 | 2561 | `		if( pKeep == 0 ){` |
|       ! 0 | 2562 | `			return SXRET_OK; /* out of memory: stay lazy rather than fail the read */` |
|         - | 2563 | `		}` |
|       135 | 2564 | `		PH7_MemObjInit(pVm,pKeep);` |
|       135 | 2565 | `		PH7_MemObjStore(pOut,pKeep);` |
|       135 | 2566 | `		pCons->xExpand = VmExpandUserConstant;` |
|       135 | 2567 | `		pCons->pUserData = pKeep;` |
|         - | 2568 | `	}` |
|       135 | 2569 | `	return SXRET_OK;` |
|     84440 | 2570 | `}` |
|    133093 | 2571 | `PH7_PRIVATE void VmExpandConstantWithNotice(ph7_vm *pVm,ph7_constant *pCons,ph7_value *pOut)` |
|         5 | 2572 | `{` |
|    133098 | 2573 | `	if( SySetUsed(&pCons->aAttrs) > 0 ){` |
|         7 | 2574 | `		VmDeprecatedAttrNoticeSubject(pVm,&pCons->aAttrs,"Constant",0,&pCons->sName);` |
|         3 | 2575 | `	}` |
|    133098 | 2576 | `	VmExpandConstantOnce(pVm,pCons,pOut);` |
|    133098 | 2577 | `}` |
|         - | 2578 | `/*` |
|         - | 2579 | ` * Query a GLOBAL constant by its exact (case-sensitive) name and expand its` |
|         - | 2580 | ` * value into pOut, which the caller has initialized. Returns 1 when the` |
|         - | 2581 | ` * constant exists. The ini scanner's NORMAL/TYPED value interpretation is the` |
|         - | 2582 | ` * caller: php substitutes a defined constant's value for a bare identifier` |
|         - | 2583 | ` * token inside an unquoted ini value.` |
|         - | 2584 | ` */` |
|        44 | 2585 | `PH7_PRIVATE int PH7_VmQueryConstant(ph7_vm *pVm,const char *zName,sxu32 nName,ph7_value *pOut)` |
|         1 | 2586 | `{` |
|         - | 2587 | `	SyHashEntry *pEntry;` |
|         - | 2588 | `	ph7_constant *pCons;` |
|        45 | 2589 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)zName,nName);` |
|        45 | 2590 | `	if( pEntry == 0 ){` |
|        41 | 2591 | `		return 0;` |
|         - | 2592 | `	}` |
|         5 | 2593 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|         5 | 2594 | `	VmExpandConstantWithNotice(pVm,pCons,pOut);` |
|         5 | 2595 | `	return 1;` |
|        23 | 2596 | `}` |
|         - | 2597 | `/*` |
|         - | 2598 | ` * Scan a declared-attribute set for #[\Deprecated]; when found, evaluate its` |
|         - | 2599 | ` * message:/since: arguments (positional #0 = message, #1 = since) into the` |
|         - | 2600 | ` * caller's values and return TRUE. The base subject text ("Function f()",` |
|         - | 2601 | ` * "Constant C::K") is the caller's business.` |
|         - | 2602 | ` */` |
|       204 | 2603 | `static int VmDeprecatedAttrExtract(ph7_vm *pVm,SySet *pAttrs,` |
|         - | 2604 | `	ph7_value *pMsg,int *pbMsg,ph7_value *pSince,int *pbSince)` |
|         5 | 2605 | `{` |
|       209 | 2606 | `	ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|         - | 2607 | `	sxu32 n;` |
|       209 | 2608 | `	*pbMsg = *pbSince = 0;` |
|       383 | 2609 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; ++n ){` |
|       213 | 2610 | `		ph7_attribute *pAttr = &aAttr[n];` |
|         - | 2611 | `		ph7_attr_arg *aArg;` |
|       213 | 2612 | `		sxu32 i,nPos = 0;` |
|       208 | 2613 | `		if( SyStringLength(&pAttr->sName) != sizeof("Deprecated")-1` |
|       126 | 2614 | `		 \|\| SyStrnicmp(SyStringData(&pAttr->sName),"Deprecated",sizeof("Deprecated")-1) != 0 ){` |
|       179 | 2615 | `			continue;` |
|         - | 2616 | `		}` |
|        35 | 2617 | `		aArg = (ph7_attr_arg *)SySetBasePtr(&pAttr->aArgs);` |
|        53 | 2618 | `		for( i = 0 ; i < SySetUsed(&pAttr->aArgs) ; ++i ){` |
|        19 | 2619 | `			ph7_attr_arg *pArg = &aArg[i];` |
|        19 | 2620 | `			int isMsg = 0,isSince = 0;` |
|        19 | 2621 | `			if( SyStringLength(&pArg->sName) == 0 ){` |
|         3 | 2622 | `				isMsg = (nPos == 0);` |
|         3 | 2623 | `				isSince = (nPos == 1);` |
|         3 | 2624 | `				nPos++;` |
|        18 | 2625 | `			}else if( SyStringLength(&pArg->sName) == sizeof("message")-1` |
|        12 | 2626 | `			 && SyMemcmp(SyStringData(&pArg->sName),"message",sizeof("message")-1) == 0 ){` |
|         7 | 2627 | `				isMsg = 1;` |
|        14 | 2628 | `			}else if( SyStringLength(&pArg->sName) == sizeof("since")-1` |
|        11 | 2629 | `			 && SyMemcmp(SyStringData(&pArg->sName),"since",sizeof("since")-1) == 0 ){` |
|        11 | 2630 | `				isSince = 1;` |
|         5 | 2631 | `			}` |
|        19 | 2632 | `			if( isMsg && !*pbMsg && SySetUsed(&pArg->aByteCode) > 0 ){` |
|        13 | 2633 | `				if( VmLocalExec(pVm,&pArg->aByteCode,pMsg,FALSE) == SXRET_OK ){` |
|         9 | 2634 | `					if( (pMsg->iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2635 | `						PH7_MemObjToString(pMsg);` |
|       ! 0 | 2636 | `					}` |
|         9 | 2637 | `					*pbMsg = 1;` |
|         5 | 2638 | `				}` |
|        15 | 2639 | `			}else if( isSince && !*pbSince && SySetUsed(&pArg->aByteCode) > 0 ){` |
|        11 | 2640 | `				if( VmLocalExec(pVm,&pArg->aByteCode,pSince,FALSE) == SXRET_OK ){` |
|        11 | 2641 | `					if( (pSince->iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2642 | `						PH7_MemObjToString(pSince);` |
|       ! 0 | 2643 | `					}` |
|        11 | 2644 | `					*pbSince = 1;` |
|         5 | 2645 | `				}` |
|         5 | 2646 | `			}` |
|        10 | 2647 | `		}` |
|        35 | 2648 | `		return 1;` |
|       ! 0 | 2649 | `	}` |
|       175 | 2650 | `	return 0;` |
|       107 | 2651 | `}` |
|         - | 2652 | `/*` |
|         - | 2653 | ` * Append php's " since X" / ", message" suffixes to a built base subject and` |
|         - | 2654 | ` * emit the E_USER_DEPRECATED notice.` |
|         - | 2655 | ` */` |
|        34 | 2656 | `static void VmDeprecatedEmit(ph7_vm *pVm,SyBlob *pOut,` |
|         - | 2657 | `	ph7_value *pMsg,int bMsg,ph7_value *pSince,int bSince)` |
|         1 | 2658 | `{` |
|        35 | 2659 | `	if( bSince && SyBlobLength(&pSince->sBlob) > 0 ){` |
|        16 | 2660 | `		SyBlobFormat(pOut," since %.*s",(int)SyBlobLength(&pSince->sBlob),` |
|        10 | 2661 | `			(const char *)SyBlobData(&pSince->sBlob));` |
|         5 | 2662 | `	}` |
|        35 | 2663 | `	if( bMsg && SyBlobLength(&pMsg->sBlob) > 0 ){` |
|        13 | 2664 | `		SyBlobFormat(pOut,", %.*s",(int)SyBlobLength(&pMsg->sBlob),` |
|         8 | 2665 | `			(const char *)SyBlobData(&pMsg->sBlob));` |
|         4 | 2666 | `	}` |
|        35 | 2667 | `	VmThrowUserDeprecatedFmt(pVm,"%.*s",(int)SyBlobLength(pOut),(const char *)SyBlobData(pOut));` |
|        35 | 2668 | `}` |
|         - | 2669 | `/*` |
|         - | 2670 | ` * Generic #[\Deprecated] notice for a named subject:` |
|         - | 2671 | ` * "<Kind> [Qual::]Name is deprecated[ since X][, message]".` |
|         - | 2672 | ` */` |
|        18 | 2673 | `static void VmDeprecatedAttrNoticeSubject(ph7_vm *pVm,SySet *pAttrs,` |
|         - | 2674 | `	const char *zKind,const SyString *pQual,const SyString *pName)` |
|         1 | 2675 | `{` |
|         - | 2676 | `	ph7_value sMsg,sSince;` |
|         - | 2677 | `	SyBlob sOut;` |
|         - | 2678 | `	int bMsg,bSince;` |
|        19 | 2679 | `	PH7_MemObjInit(pVm,&sMsg);` |
|        19 | 2680 | `	PH7_MemObjInit(pVm,&sSince);` |
|        19 | 2681 | `	if( VmDeprecatedAttrExtract(pVm,pAttrs,&sMsg,&bMsg,&sSince,&bSince) ){` |
|        15 | 2682 | `		SyBlobInit(&sOut,&pVm->sAllocator);` |
|        15 | 2683 | `		if( pQual ){` |
|        11 | 2684 | `			SyBlobFormat(&sOut,"%s %z::%z is deprecated",zKind,pQual,pName);` |
|         6 | 2685 | `		}else{` |
|         5 | 2686 | `			SyBlobFormat(&sOut,"%s %z is deprecated",zKind,pName);` |
|         - | 2687 | `		}` |
|        15 | 2688 | `		VmDeprecatedEmit(pVm,&sOut,&sMsg,bMsg,&sSince,bSince);` |
|        15 | 2689 | `		SyBlobRelease(&sOut);` |
|         7 | 2690 | `	}` |
|        19 | 2691 | `	PH7_MemObjRelease(&sMsg);` |
|        19 | 2692 | `	PH7_MemObjRelease(&sSince);` |
|        19 | 2693 | `}` |
|       186 | 2694 | `PH7_PRIVATE void VmDeprecatedAttrNotice(ph7_vm *pVm,ph7_vm_func *pFunc,ph7_class *pDeclClass)` |
|         5 | 2695 | `{` |
|         - | 2696 | `	ph7_value sMsg,sSince;` |
|         - | 2697 | `	SyBlob sOut;` |
|         - | 2698 | `	int bMsg,bSince;` |
|       191 | 2699 | `	PH7_MemObjInit(pVm,&sMsg);` |
|       191 | 2700 | `	PH7_MemObjInit(pVm,&sSince);` |
|       191 | 2701 | `	if( VmDeprecatedAttrExtract(pVm,&pFunc->aAttrs,&sMsg,&bMsg,&sSince,&bSince) ){` |
|        21 | 2702 | `		SyBlobInit(&sOut,&pVm->sAllocator);` |
|        21 | 2703 | `		if( pDeclClass ){` |
|         5 | 2704 | `			SyBlobFormat(&sOut,"Method %z::%z() is deprecated",&pDeclClass->sName,&pFunc->sName);` |
|         3 | 2705 | `		}else{` |
|        17 | 2706 | `			SyBlobFormat(&sOut,"Function %z() is deprecated",&pFunc->sName);` |
|         - | 2707 | `		}` |
|        21 | 2708 | `		VmDeprecatedEmit(pVm,&sOut,&sMsg,bMsg,&sSince,bSince);` |
|        21 | 2709 | `		SyBlobRelease(&sOut);` |
|        10 | 2710 | `	}` |
|       191 | 2711 | `	PH7_MemObjRelease(&sMsg);` |
|       191 | 2712 | `	PH7_MemObjRelease(&sSince);` |
|       191 | 2713 | `}` |
|         - | 2714 | `/*` |
|         - | 2715 | ` * Same notice for a #[\Deprecated] class constant, at its static-access site:` |
|         - | 2716 | `` * php's `Constant C::K is deprecated` (each access re-warns).`` |
|         - | 2717 | ` */` |
|        12 | 2718 | `PH7_PRIVATE void VmDeprecatedConstNotice(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pMember)` |
|         1 | 2719 | `{` |
|        19 | 2720 | `	VmDeprecatedAttrNoticeSubject(pVm,&pMember->aAttrs,` |
|        12 | 2721 | `		(pMember->iFlags & PH7_CLASS_ATTR_ENUMCASE) ? "Enum case" : "Constant",` |
|        12 | 2722 | `		&pClass->sName,&pMember->sName);` |
|        13 | 2723 | `}` |
|         - | 2724 | `/*` |
|         - | 2725 | `` * The USER-VISIBLE string coercion a builtin performs on a `mixed` argument or`` |
|         - | 2726 | ` * array element: the builtin-side twin of PH7_MemObjToStringUV's opcode sites.` |
|         - | 2727 | ` * An ARRAY warns "Array to string conversion" and still renders as "Array"; an` |
|         - | 2728 | ` * object whose class has no __toString() -- or one whose __toString() threw --` |
|         - | 2729 | ` * is php's catchable "could not be converted to string" Error, and the builtin` |
|         - | 2730 | ` * must answer that instead of a value.` |
|         - | 2731 | ` *` |
|         - | 2732 | ` * On success pzData and pnLen receive the NUL-terminated bytes (both optional).` |
|         - | 2733 | ` * On a throw they are set to the empty string and the status is returned AND` |
|         - | 2734 | ` * recorded on the call context, so OP_CALL cannot mistake the call for a normal` |
|         - | 2735 | ` * return; a builtin that has already produced output (printf) still keeps it,` |
|         - | 2736 | ` * which is what php does.` |
|         - | 2737 | ` *` |
|         - | 2738 | ` * The public ph7_value_to_string() stays SILENT on purpose: it is the embedder` |
|         - | 2739 | ` * API, and the INTERNAL coercions behind it (array-key canonicalisation, sort` |
|         - | 2740 | ` * comparisons, print_r/var_export/serialize) must not throw -- php's do not` |
|         - | 2741 | ` * either.` |
|         - | 2742 | ` */` |
|    275632 | 2743 | `PH7_PRIVATE sxi32 PH7_ValueToStringUV(ph7_context *pCtx,ph7_value *pValue,const char **pzData,int *pnLen)` |
|         5 | 2744 | `{` |
|    275637 | 2745 | `	sxi32 rc = PH7_MemObjToStringUV(pValue);` |
|    275637 | 2746 | `	if( rc != SXRET_OK ){` |
|        37 | 2747 | `		if( pCtx ){` |
|        37 | 2748 | `			pCtx->nThrowRc = rc;` |
|        17 | 2749 | `		}` |
|        37 | 2750 | `		if( pzData ){` |
|        33 | 2751 | `			*pzData = "";` |
|        15 | 2752 | `		}` |
|        37 | 2753 | `		if( pnLen ){` |
|        33 | 2754 | `			*pnLen = 0;` |
|        15 | 2755 | `		}` |
|        37 | 2756 | `		return rc;` |
|         - | 2757 | `	}` |
|    275603 | 2758 | `	if( pzData \|\| pnLen ){` |
|    275575 | 2759 | `		const char *zData = ph7_value_to_string(pValue,pnLen);` |
|    275575 | 2760 | `		if( pzData ){` |
|    275575 | 2761 | `			*pzData = zData;` |
|    137785 | 2762 | `		}` |
|    137785 | 2763 | `	}` |
|    275603 | 2764 | `	return SXRET_OK;` |
|    137821 | 2765 | `}` |
|         - | 2766 |  |
