# src/ph7/vm_arg_check.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 841/899 lines (93.55%)

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
|         - |  145 | `	{ "iconv",                     3, 0 },` |
|         - |  146 | `	{ "iconv_strlen",              1, 1 },` |
|         - |  147 | `	{ "iconv_substr",              2, 1 },` |
|         - |  148 | `	{ "iconv_strpos",              2, 1 },` |
|         - |  149 | `	{ "iconv_strrpos",             2, 1 },` |
|         - |  150 | `	{ "iconv_mime_encode",         2, 1 },` |
|         - |  151 | `	{ "iconv_mime_decode",         1, 1 },` |
|         - |  152 | `	{ "iconv_mime_decode_headers", 1, 1 },` |
|         - |  153 | `	{ "round",                     1, 1 },` |
|         - |  154 | `	{ "sha1",                      1, 1 },` |
|         - |  155 | `	{ "sin",                       1, 0 },` |
|         - |  156 | `	{ "sinh",                      1, 0 },` |
|         - |  157 | `	{ "sqrt",                      1, 0 },` |
|         - |  158 | `	{ "tan",                       1, 0 },` |
|         - |  159 | `	{ "tanh",                      1, 0 },` |
|         - |  160 | `	/* Type/var family */` |
|         - |  161 | `	{ "floatval",                  1, 0 },` |
|         - |  162 | `	{ "get_resource_id",           1, 0 },` |
|         - |  163 | `	{ "get_resource_type",         1, 0 },` |
|         - |  164 | `	{ "gettype",                   1, 0 },` |
|         - |  165 | `	{ "intval",                    1, 1 },` |
|         - |  166 | `	{ "is_array",                  1, 0 },` |
|         - |  167 | `	{ "is_bool",                   1, 0 },` |
|         - |  168 | `	{ "is_callable",               1, 1 },` |
|         - |  169 | `	{ "is_double",                 1, 0 },` |
|         - |  170 | `	{ "is_float",                  1, 0 },` |
|         - |  171 | `	{ "is_int",                    1, 0 },` |
|         - |  172 | `	{ "is_integer",                1, 0 },` |
|         - |  173 | `	{ "is_long",                   1, 0 },` |
|         - |  174 | `	{ "is_null",                   1, 0 },` |
|         - |  175 | `	{ "is_numeric",                1, 0 },` |
|         - |  176 | `	{ "is_object",                 1, 0 },` |
|         - |  177 | `	{ "is_resource",               1, 0 },` |
|         - |  178 | `	{ "is_scalar",                 1, 0 },` |
|         - |  179 | `	{ "is_string",                 1, 0 },` |
|         - |  180 | `	{ "print_r",                   1, 1 },` |
|         - |  181 | `	{ "strval",                    1, 0 },` |
|         - |  182 | `	{ "var_dump",                  1, 1 },` |
|         - |  183 | `	{ "var_export",                1, 1 },` |
|         - |  184 | `	/* Array/iterator family */` |
|         - |  185 | `	{ "array_filter",              1, 1 },` |
|         - |  186 | `	{ "array_product",             1, 0 },` |
|         - |  187 | `	{ "array_rand",                1, 1 },` |
|         - |  188 | `	{ "compact",                   1, 1 },` |
|         - |  189 | `	{ "current",                   1, 0 },` |
|         - |  190 | `	{ "end",                       1, 0 },` |
|         - |  191 | `	{ "extract",                   1, 1 },` |
|         - |  192 | `	{ "iterator_apply",            2, 1 },` |
|         - |  193 | `	{ "iterator_count",            1, 0 },` |
|         - |  194 | `	{ "iterator_to_array",         1, 1 },` |
|         - |  195 | `	{ "key",                       1, 0 },` |
|         - |  196 | `	{ "krsort",                    1, 1 },` |
|         - |  197 | `	{ "ksort",                     1, 1 },` |
|         - |  198 | `	{ "next",                      1, 0 },` |
|         - |  199 | `	{ "pos",                       1, 0 },` |
|         - |  200 | `	{ "prev",                      1, 0 },` |
|         - |  201 | `	{ "reset",                     1, 0 },` |
|         - |  202 | `	{ "rsort",                     1, 1 },` |
|         - |  203 | `	{ "shuffle",                   1, 0 },` |
|         - |  204 | `	{ "sort",                      1, 1 },` |
|         - |  205 | `	{ "uasort",                    2, 0 },` |
|         - |  206 | `	{ "uksort",                    2, 0 },` |
|         - |  207 | `	{ "usort",                     2, 0 },` |
|         - |  208 | `	/* Class/reflection family */` |
|         - |  209 | `	{ "class_alias",               2, 1 },` |
|         - |  210 | `	{ "class_exists",              1, 1 },` |
|         - |  211 | `	{ "enum_exists",               1, 1 },` |
|         - |  212 | `	{ "get_class_methods",         1, 0 },` |
|         - |  213 | `	{ "get_class_vars",            1, 0 },` |
|         - |  214 | `	{ "get_object_vars",           1, 0 },` |
|         - |  215 | `	{ "interface_exists",          1, 1 },` |
|         - |  216 | `	{ "trait_exists",              1, 1 },` |
|         - |  217 | `	{ "is_a",                      2, 1 },` |
|         - |  218 | `	{ "is_subclass_of",            2, 1 },` |
|         - |  219 | `	{ "method_exists",             2, 0 },` |
|         - |  220 | `	{ "property_exists",           2, 0 },` |
|         - |  221 | `	{ "spl_autoload",              1, 1 },` |
|         - |  222 | `	{ "spl_autoload_unregister",   1, 0 },` |
|         - |  223 | `	{ "spl_object_hash",           1, 0 },` |
|         - |  224 | `	{ "spl_object_id",             1, 0 },` |
|         - |  225 | `	/* Filesystem/IO family */` |
|         - |  226 | `	{ "basename",                  1, 1 },` |
|         - |  227 | `	{ "chdir",                     1, 0 },` |
|         - |  228 | `	{ "chgrp",                     2, 0 },` |
|         - |  229 | `	{ "dir",                       1, 1 },` |
|         - |  230 | `	{ "dirname",                   1, 1 },` |
|         - |  231 | `	{ "disk_free_space",           1, 0 },` |
|         - |  232 | `	{ "disk_total_space",          1, 0 },` |
|         - |  233 | `	{ "diskfreespace",             1, 0 },` |
|         - |  234 | `	{ "fclose",                    1, 0 },` |
|         - |  235 | `	{ "feof",                      1, 0 },` |
|         - |  236 | `	{ "fflush",                    1, 0 },` |
|         - |  237 | `	{ "fgetc",                     1, 0 },` |
|         - |  238 | `	{ "fgetcsv",                   1, 1 },` |
|         - |  239 | `	{ "file",                      1, 1 },` |
|         - |  240 | `	{ "file_exists",               1, 0 },` |
|         - |  241 | `	{ "fileatime",                 1, 0 },` |
|         - |  242 | `	{ "filectime",                 1, 0 },` |
|         - |  243 | `	{ "filemtime",                 1, 0 },` |
|         - |  244 | `	{ "filesize",                  1, 0 },` |
|         - |  245 | `	{ "filetype",                  1, 0 },` |
|         - |  246 | `	{ "flock",                     2, 1 },` |
|         - |  247 | `	{ "fpassthru",                 1, 0 },` |
|         - |  248 | `	{ "fputcsv",                   2, 1 },` |
|         - |  249 | `	{ "fputs",                     2, 1 },` |
|         - |  250 | `	{ "fseek",                     2, 1 },` |
|         - |  251 | `	{ "fstat",                     1, 0 },` |
|         - |  252 | `	{ "ftell",                     1, 0 },` |
|         - |  253 | `	{ "ftruncate",                 2, 0 },` |
|         - |  254 | `	{ "getopt",                    1, 1 },` |
|         - |  255 | `	{ "is_dir",                    1, 0 },` |
|         - |  256 | `	{ "is_executable",             1, 0 },` |
|         - |  257 | `	{ "is_file",                   1, 0 },` |
|         - |  258 | `	{ "is_link",                   1, 0 },` |
|         - |  259 | `	{ "is_readable",               1, 0 },` |
|         - |  260 | `	{ "is_writable",               1, 0 },` |
|         - |  261 | `	{ "lstat",                     1, 0 },` |
|         - |  262 | `	{ "md5_file",                  1, 1 },` |
|         - |  263 | `	{ "opendir",                   1, 1 },` |
|         - |  264 | `	{ "pathinfo",                  1, 1 },` |
|         - |  265 | `	{ "pclose",                    1, 0 },` |
|         - |  266 | `	{ "readlink",                  1, 0 },` |
|         - |  267 | `	{ "realpath",                  1, 0 },` |
|         - |  268 | `	{ "stream_resolve_include_path",1, 0 },` |
|         - |  269 | `	{ "rewind",                    1, 0 },` |
|         - |  270 | `	{ "sha1_file",                 1, 1 },` |
|         - |  271 | `	{ "stat",                      1, 0 },` |
|         - |  272 | `	/* Date family */` |
|         - |  273 | `	{ "date",                      1, 1 },` |
|         - |  274 | `	{ "date_default_timezone_set", 1, 1 },` |
|         - |  275 | `	{ "gmdate",                    1, 1 },` |
|         - |  276 | `	{ "gmmktime",                  1, 1 },` |
|         - |  277 | `	{ "idate",                     1, 1 },` |
|         - |  278 | `	{ "mktime",                    1, 1 },` |
|         - |  279 | `	/* Encoding/URL family */` |
|         - |  280 | `	{ "base64_decode",             1, 1 },` |
|         - |  281 | `	{ "base64_encode",             1, 0 },` |
|         - |  282 | `	{ "convert_uudecode",          1, 0 },` |
|         - |  283 | `	{ "convert_uuencode",          1, 0 },` |
|         - |  284 | `	{ "parse_ini_file",            1, 1 },` |
|         - |  285 | `	{ "parse_ini_string",          1, 1 },` |
|         - |  286 | `	{ "parse_url",                 1, 1 },` |
|         - |  287 | `	{ "rawurldecode",              1, 0 },` |
|         - |  288 | `	{ "rawurlencode",              1, 0 },` |
|         - |  289 | `	{ "urldecode",                 1, 0 },` |
|         - |  290 | `	{ "urlencode",                 1, 0 },` |
|         - |  291 | `	/* JSON/serialize family */` |
|         - |  292 | `	{ "filter_var",                1, 1 },` |
|         - |  293 | `	{ "json_decode",               1, 1 },` |
|         - |  294 | `	{ "json_encode",               1, 1 },` |
|         - |  295 | `	{ "json_validate",             1, 1 },` |
|         - |  296 | `	{ "serialize",                 1, 0 },` |
|         - |  297 | `	{ "unserialize",               1, 1 },` |
|         - |  298 | `	/* PCRE family */` |
|         - |  299 | `	{ "preg_match",                2, 1 },` |
|         - |  300 | `	{ "preg_match_all",            2, 1 },` |
|         - |  301 | `	{ "preg_quote",                1, 1 },` |
|         - |  302 | `	{ "preg_replace",              3, 1 },` |
|         - |  303 | `	{ "preg_replace_callback",     3, 1 },` |
|         - |  304 | `	{ "preg_split",                2, 1 },` |
|         - |  305 | `	/* XML family */` |
|         - |  306 | `	/* Constants/misc family */` |
|         - |  307 | `	{ "call_user_func",            1, 1 },` |
|         - |  308 | `	{ "call_user_func_array",      2, 0 },` |
|         - |  309 | `	{ "constant",                  1, 0 },` |
|         - |  310 | `	{ "define",                    2, 1 },` |
|         - |  311 | `	{ "defined",                   1, 0 },` |
|         - |  312 | `	{ "error_log",                 1, 1 },` |
|         - |  313 | `	{ "fnmatch",                   2, 1 },` |
|         - |  314 | `	{ "forward_static_call",       1, 1 },` |
|         - |  315 | `	{ "forward_static_call_array", 2, 0 },` |
|         - |  316 | `	{ "func_get_arg",              1, 0 },` |
|         - |  317 | `	{ "function_exists",           1, 0 },` |
|         - |  318 | `	{ "header",                    1, 1 },` |
|         - |  319 | `	{ "password_get_info",         1, 0 },` |
|         - |  320 | `	{ "putenv",                    1, 0 },` |
|         - |  321 | `	{ "register_shutdown_function", 1, 1 },` |
|         - |  322 | `	{ "set_error_handler",         1, 1 },` |
|         - |  323 | `	{ "set_exception_handler",     1, 0 },` |
|         - |  324 | `	{ "setcookie",                 1, 1 },` |
|         - |  325 | `	{ "setrawcookie",              1, 1 },` |
|         - |  326 | `	{ "trigger_error",             1, 1 },` |
|         - |  327 | `	{ "user_error",                1, 1 },` |
|         - |  328 | `	/*` |
|         - |  329 | `	 * Overrides for signatures that under-report their own minimum: the callback` |
|         - |  330 | `	 * of these three hides inside the variadic tail ("array $array, ...$rest"),` |
|         - |  331 | `	 * so the derivation reads 1 where php requires 2.` |
|         - |  332 | `	 */` |
|         - |  333 | `	{ "array_udiff",               2, 1 },` |
|         - |  334 | `	{ "array_uintersect",          2, 1 },` |
|         - |  335 | `	{ "array_diff_uassoc",         2, 1 },` |
|         - |  336 | `	{ "array_diff_ukey",           2, 1 },` |
|         - |  337 | `	{ "array_intersect_ukey",      2, 1 },` |
|         - |  338 | `	{ "array_intersect_uassoc",    2, 1 },` |
|         - |  339 | `	{ "array_udiff_assoc",         2, 1 },` |
|         - |  340 | `	{ "array_uintersect_assoc",    2, 1 },` |
|         - |  341 | `	{ "array_udiff_uassoc",        3, 1 },` |
|         - |  342 | `	{ "array_uintersect_uassoc",   3, 1 },` |
|         - |  343 | `};` |
|         - |  344 | `/*` |
|         - |  345 | ` * Stamp the minimum-arity metadata from aBuiltinArity[] onto the already` |
|         - |  346 | ` * registered host functions. Called once at VM init after every builtin family` |
|         - |  347 | ` * has been installed into hHostFunction. A name absent from the hash (e.g. a` |
|         - |  348 | ` * build without a given extension) is simply skipped.` |
|         - |  349 | ` */` |
|      4660 |  350 | `PH7_PRIVATE void VmSetBuiltinArity(ph7_vm *pVm)` |
|         5 |  351 | `{` |
|         - |  352 | `	sxu32 n;` |
|   1346745 |  353 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinArity) ; ++n ){` |
|   1342085 |  354 | `		const struct VmBuiltinArity *p = &aBuiltinArity[n];` |
|   2684165 |  355 | `		SyHashEntry *pEntry = SyHashGet(&pVm->hHostFunction,` |
|   1342080 |  356 | `			(const void *)p->zName,SyStrlen(p->zName));` |
|   1342085 |  357 | `		if( pEntry ){` |
|   1342085 |  358 | `			ph7_user_func *pFunc = (ph7_user_func *)pEntry->pUserData;` |
|   1342085 |  359 | `			pFunc->nMinArg  = p->nMin;` |
|   1342085 |  360 | `			pFunc->bAtLeast = p->bAtLeast;` |
|    671040 |  361 | `		}` |
|    671045 |  362 | `	}` |
|      4665 |  363 | `}` |
|         - |  364 | `/*` |
|         - |  365 | ` * PHP 8.5 parameter signatures for the C builtins, generated offline from` |
|         - |  366 | ` * a real PHP 8.5 ReflectionFunction dump over PHL's registered function` |
|         - |  367 | ` * list (see the plan's Reflection section). "= ?" marks an optional` |
|         - |  368 | ` * parameter whose default is not representable as a short literal.` |
|         - |  369 | ` * Reflection parses these strings on demand; unlisted builtins degrade to` |
|         - |  370 | ` * the min-arity data.` |
|         - |  371 | ` */` |
|         - |  372 | `static const struct VmBuiltinSig {` |
|         - |  373 | `	const char *zName;` |
|         - |  374 | `	const char *zSig;` |
|         - |  375 | `	const char *zRet;` |
|         - |  376 | `} aBuiltinSig[] = {` |
|         - |  377 | `	/* The subsystems converted from embedded PHP into C (INI, libxml, sessions).` |
|         - |  378 | `	 * A prelude function declared its parameters in PHP and Reflection read them` |
|         - |  379 | `	 * from there; a C builtin has no declaration but this table, so without a row` |
|         - |  380 | `	 * here the same function reports NO parameters -- and loses its arity bounds` |
|         - |  381 | `	 * with them. */` |
|         - |  382 | `	/* ext/curl. Signatures dumped from php 8.5's own ReflectionFunction, which` |
|         - |  383 | `	 * is also where the parameter NAMES come from: a named argument spells the` |
|         - |  384 | `	 * php one, so an invented name breaks valid php. */` |
|         - |  385 | `	{ "curl_close", "CurlHandle $handle", "void" },` |
|         - |  386 | `	{ "curl_copy_handle", "CurlHandle $handle", "CurlHandle\|false" },` |
|         - |  387 | `	{ "curl_errno", "CurlHandle $handle", "int" },` |
|         - |  388 | `	{ "curl_error", "CurlHandle $handle", "string" },` |
|         - |  389 | `	{ "curl_escape", "CurlHandle $handle, string $string", "string\|false" },` |
|         - |  390 | `	{ "curl_exec", "CurlHandle $handle", "string\|bool" },` |
|         - |  391 | `	{ "curl_getinfo", "CurlHandle $handle, ?int $option = null", "mixed" },` |
|         - |  392 | `	{ "curl_init", "?string $url = null", "CurlHandle\|false" },` |
|         - |  393 | `	{ "curl_multi_strerror", "int $error_code", "?string" },` |
|         - |  394 | `	{ "curl_pause", "CurlHandle $handle, int $flags", "int" },` |
|         - |  395 | `	{ "curl_reset", "CurlHandle $handle", "void" },` |
|         - |  396 | `	{ "curl_setopt", "CurlHandle $handle, int $option, mixed $value", "bool" },` |
|         - |  397 | `	{ "curl_setopt_array", "CurlHandle $handle, array $options", "bool" },` |
|         - |  398 | `	{ "curl_unescape", "CurlHandle $handle, string $string", "string\|false" },` |
|         - |  399 | `	{ "curl_upkeep", "CurlHandle $handle", "bool" },` |
|         - |  400 | `	{ "curl_share_strerror", "int $error_code", "?string" },` |
|         - |  401 | `	{ "curl_strerror", "int $error_code", "?string" },` |
|         - |  402 | `	{ "curl_version", "", "array\|false" },` |
|         - |  403 | `	{ "get_cfg_var", "string $option", "array\|string\|false" },` |
|         - |  404 | `	{ "ini_get", "string $option", "string\|false" },` |
|         - |  405 | `	{ "ini_get_all", "?string $extension = null, bool $details = true", "array\|false" },` |
|         - |  406 | `	{ "ini_restore", "string $option", "void" },` |
|         - |  407 | `	{ "ini_set", "string $option, string\|int\|float\|bool\|null $value", "string\|false" },` |
|         - |  408 | `	{ "libxml_clear_errors", "", "void" },` |
|         - |  409 | `	{ "libxml_get_errors", "", "array" },` |
|         - |  410 | `	{ "libxml_get_external_entity_loader", "", "?callable" },` |
|         - |  411 | `	{ "libxml_get_last_error", "", "LibXMLError\|false" },` |
|         - |  412 | `	{ "libxml_set_external_entity_loader", "?callable $resolver_function", "true" },` |
|         - |  413 | `	{ "libxml_set_streams_context", "$context", "void" },` |
|         - |  414 | `	{ "libxml_use_internal_errors", "?bool $use_errors = null", "bool" },` |
|         - |  415 | `	{ "xml_error_string", "int $error_code", "?string" },` |
|         - |  416 | `	{ "xml_get_current_byte_index", "XMLParser $parser", "int" },` |
|         - |  417 | `	{ "xml_get_current_column_number", "XMLParser $parser", "int" },` |
|         - |  418 | `	{ "xml_get_current_line_number", "XMLParser $parser", "int" },` |
|         - |  419 | `	{ "xml_get_error_code", "XMLParser $parser", "int" },` |
|         - |  420 | `	{ "xml_parse", "XMLParser $parser, string $data, bool $is_final = false", "int" },` |
|         - |  421 | `	{ "xml_parse_into_struct", "XMLParser $parser, string $data, &$values, &$index = NULL", "int\|false" },` |
|         - |  422 | `	{ "xml_parser_create", "?string $encoding = NULL", "XMLParser" },` |
|         - |  423 | `	{ "xml_parser_create_ns", "?string $encoding = NULL, string $separator = ':'", "XMLParser" },` |
|         - |  424 | `	{ "xml_parser_get_option", "XMLParser $parser, int $option", "string\|int\|bool" },` |
|         - |  425 | `	{ "xml_parser_set_option", "XMLParser $parser, int $option, $value", "bool" },` |
|         - |  426 | `	{ "xml_set_character_data_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  427 | `	{ "xml_set_default_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  428 | `	{ "xml_set_element_handler", "XMLParser $parser, callable\|string\|null $start_handler, callable\|string\|null $end_handler", "true" },` |
|         - |  429 | `	{ "xml_set_end_namespace_decl_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  430 | `	{ "xml_set_external_entity_ref_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  431 | `	{ "xml_set_notation_decl_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  432 | `	{ "xml_set_processing_instruction_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  433 | `	{ "xml_set_start_namespace_decl_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  434 | `	{ "xml_set_unparsed_entity_decl_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  435 | `	/* ext/xmlwriter: php presents every writer verb under a function name as` |
|         - |  436 | `	 * well, with the writer as argument #1 -- which is the numbering its own` |
|         - |  437 | `	 * diagnostics report from BOTH spellings (see vm_xmlwriter.c). */` |
|         - |  438 | `	{ "xmlwriter_open_uri", "string $uri", "XMLWriter\|false" },` |
|         - |  439 | `	{ "xmlwriter_open_memory", "", "XMLWriter\|false" },` |
|         - |  440 | `	{ "xmlwriter_set_indent", "XMLWriter $writer, bool $enable", "bool" },` |
|         - |  441 | `	{ "xmlwriter_set_indent_string", "XMLWriter $writer, string $indentation", "bool" },` |
|         - |  442 | `	{ "xmlwriter_start_comment", "XMLWriter $writer", "bool" },` |
|         - |  443 | `	{ "xmlwriter_end_comment", "XMLWriter $writer", "bool" },` |
|         - |  444 | `	{ "xmlwriter_start_attribute", "XMLWriter $writer, string $name", "bool" },` |
|         - |  445 | `	{ "xmlwriter_end_attribute", "XMLWriter $writer", "bool" },` |
|         - |  446 | `	{ "xmlwriter_write_attribute", "XMLWriter $writer, string $name, string $value", "bool" },` |
|         - |  447 | `	{ "xmlwriter_start_attribute_ns", "XMLWriter $writer, ?string $prefix, string $name, ?string $namespace", "bool" },` |
|         - |  448 | `	{ "xmlwriter_write_attribute_ns", "XMLWriter $writer, ?string $prefix, string $name, ?string $namespace, string $value", "bool" },` |
|         - |  449 | `	{ "xmlwriter_start_element", "XMLWriter $writer, string $name", "bool" },` |
|         - |  450 | `	{ "xmlwriter_end_element", "XMLWriter $writer", "bool" },` |
|         - |  451 | `	{ "xmlwriter_full_end_element", "XMLWriter $writer", "bool" },` |
|         - |  452 | `	{ "xmlwriter_start_element_ns", "XMLWriter $writer, ?string $prefix, string $name, ?string $namespace", "bool" },` |
|         - |  453 | `	{ "xmlwriter_write_element", "XMLWriter $writer, string $name, ?string $content = null", "bool" },` |
|         - |  454 | `	{ "xmlwriter_write_element_ns", "XMLWriter $writer, ?string $prefix, string $name, ?string $namespace, ?string $content = null", "bool" },` |
|         - |  455 | `	{ "xmlwriter_start_pi", "XMLWriter $writer, string $target", "bool" },` |
|         - |  456 | `	{ "xmlwriter_end_pi", "XMLWriter $writer", "bool" },` |
|         - |  457 | `	{ "xmlwriter_write_pi", "XMLWriter $writer, string $target, string $content", "bool" },` |
|         - |  458 | `	{ "xmlwriter_start_cdata", "XMLWriter $writer", "bool" },` |
|         - |  459 | `	{ "xmlwriter_end_cdata", "XMLWriter $writer", "bool" },` |
|         - |  460 | `	{ "xmlwriter_write_cdata", "XMLWriter $writer, string $content", "bool" },` |
|         - |  461 | `	{ "xmlwriter_text", "XMLWriter $writer, string $content", "bool" },` |
|         - |  462 | `	{ "xmlwriter_write_raw", "XMLWriter $writer, string $content", "bool" },` |
|         - |  463 | `	{ "xmlwriter_start_document", "XMLWriter $writer, ?string $version = '1.0', ?string $encoding = null, ?string $standalone = null", "bool" },` |
|         - |  464 | `	{ "xmlwriter_end_document", "XMLWriter $writer", "bool" },` |
|         - |  465 | `	{ "xmlwriter_write_comment", "XMLWriter $writer, string $content", "bool" },` |
|         - |  466 | `	{ "xmlwriter_start_dtd", "XMLWriter $writer, string $qualifiedName, ?string $publicId = null, ?string $systemId = null", "bool" },` |
|         - |  467 | `	{ "xmlwriter_end_dtd", "XMLWriter $writer", "bool" },` |
|         - |  468 | `	{ "xmlwriter_write_dtd", "XMLWriter $writer, string $name, ?string $publicId = null, ?string $systemId = null, ?string $content = null", "bool" },` |
|         - |  469 | `	{ "xmlwriter_start_dtd_element", "XMLWriter $writer, string $qualifiedName", "bool" },` |
|         - |  470 | `	{ "xmlwriter_end_dtd_element", "XMLWriter $writer", "bool" },` |
|         - |  471 | `	{ "xmlwriter_write_dtd_element", "XMLWriter $writer, string $name, string $content", "bool" },` |
|         - |  472 | `	{ "xmlwriter_start_dtd_attlist", "XMLWriter $writer, string $name", "bool" },` |
|         - |  473 | `	{ "xmlwriter_end_dtd_attlist", "XMLWriter $writer", "bool" },` |
|         - |  474 | `	{ "xmlwriter_write_dtd_attlist", "XMLWriter $writer, string $name, string $content", "bool" },` |
|         - |  475 | `	{ "xmlwriter_start_dtd_entity", "XMLWriter $writer, string $name, bool $isParam", "bool" },` |
|         - |  476 | `	{ "xmlwriter_end_dtd_entity", "XMLWriter $writer", "bool" },` |
|         - |  477 | `	{ "xmlwriter_write_dtd_entity", "XMLWriter $writer, string $name, string $content, bool $isParam = false, ?string $publicId = null, ?string $systemId = null, ?string $notationData = null", "bool" },` |
|         - |  478 | `	{ "xmlwriter_output_memory", "XMLWriter $writer, bool $flush = true", "string" },` |
|         - |  479 | `	{ "xmlwriter_flush", "XMLWriter $writer, bool $empty = true", "string\|int" },` |
|         - |  480 | `	{ "session_abort", "", "bool" },` |
|         - |  481 | `	{ "session_cache_expire", "?int $value = null", "int\|false" },` |
|         - |  482 | `	{ "session_cache_limiter", "?string $value = null", "string\|false" },` |
|         - |  483 | `	{ "session_commit", "", "bool" },` |
|         - |  484 | `	{ "session_create_id", "string $prefix = \"\"", "string\|false" },` |
|         - |  485 | `	{ "session_decode", "string $data", "bool" },` |
|         - |  486 | `	{ "session_destroy", "", "bool" },` |
|         - |  487 | `	{ "session_gc", "", "int\|false" },` |
|         - |  488 | `	{ "session_get_cookie_params", "", "array" },` |
|         - |  489 | `	{ "session_set_save_handler", "$sessionhandler, ...$rest = ?", "bool" },` |
|         - |  490 | `	{ "session_set_cookie_params", "array\|int $lifetime_or_options, ?string $path = null, ?string $domain = null, ?bool $secure = null, ?bool $httponly = null", "bool" },` |
|         - |  491 | `	{ "session_encode", "", "string\|false" },` |
|         - |  492 | `	{ "session_id", "?string $id = null", "string\|false" },` |
|         - |  493 | `	{ "session_module_name", "?string $module = null", "string\|false" },` |
|         - |  494 | `	{ "session_name", "?string $name = null", "string\|false" },` |
|         - |  495 | `	{ "session_regenerate_id", "bool $delete_old_session = false", "bool" },` |
|         - |  496 | `	{ "session_register_shutdown", "", "void" },` |
|         - |  497 | `	{ "session_reset", "", "bool" },` |
|         - |  498 | `	{ "session_save_path", "?string $path = null", "string\|false" },` |
|         - |  499 | `	{ "session_start", "array $options = []", "bool" },` |
|         - |  500 | `	{ "session_status", "", "int" },` |
|         - |  501 | `	{ "session_unset", "", "bool" },` |
|         - |  502 | `	{ "session_write_close", "", "bool" },` |
|         - |  503 | `	{ "abs", "int\|float $num", "int\|float" },` |
|         - |  504 | `	{ "acos", "float $num", "float" },` |
|         - |  505 | `	{ "acosh", "float $num", "float" },` |
|         - |  506 | `	{ "addcslashes", "string $string, string $characters", "string" },` |
|         - |  507 | `	{ "addslashes", "string $string", "string" },` |
|         - |  508 | `	{ "array_all", "array $array, callable $callback", "bool" },` |
|         - |  509 | `	{ "array_any", "array $array, callable $callback", "bool" },` |
|         - |  510 | `	{ "array_chunk", "array $array, int $length, bool $preserve_keys = false", "array" },` |
|         - |  511 | `	{ "array_column", "array $array, string\|int\|null $column_key, string\|int\|null $index_key = NULL", "array" },` |
|         - |  512 | `	{ "array_combine", "array $keys, array $values", "array" },` |
|         - |  513 | `	{ "array_diff", "array $array, array ...$arrays = ?", "array" },` |
|         - |  514 | `	{ "array_diff_assoc", "array $array, array ...$arrays = ?", "array" },` |
|         - |  515 | `	{ "array_diff_key", "array $array, array ...$arrays = ?", "array" },` |
|         - |  516 | `	{ "array_diff_uassoc", "array $array, ...$rest = ?", "array" },` |
|         - |  517 | `	{ "array_diff_ukey", "array $array, ...$rest = ?", "array" },` |
|         - |  518 | `	{ "array_fill", "int $start_index, int $count, mixed $value", "array" },` |
|         - |  519 | `	{ "array_fill_keys", "array $keys, mixed $value", "array" },` |
|         - |  520 | `	{ "array_filter", "array $array, ?callable $callback = NULL, int $mode = 0", "array" },` |
|         - |  521 | `	{ "array_find", "array $array, callable $callback", "mixed" },` |
|         - |  522 | `	{ "array_find_key", "array $array, callable $callback", "mixed" },` |
|         - |  523 | `	{ "array_first", "array $array", "mixed" },` |
|         - |  524 | `	{ "array_flip", "array $array", "array" },` |
|         - |  525 | `	{ "array_intersect", "array $array, array ...$arrays = ?", "array" },` |
|         - |  526 | `	{ "array_intersect_assoc", "array $array, array ...$arrays = ?", "array" },` |
|         - |  527 | `	{ "array_intersect_key", "array $array, array ...$arrays = ?", "array" },` |
|         - |  528 | `	{ "array_intersect_uassoc", "array $array, ...$rest = ?", "array" },` |
|         - |  529 | `	{ "array_intersect_ukey", "array $array, ...$rest = ?", "array" },` |
|         - |  530 | `	{ "array_is_list", "array $array", "bool" },` |
|         - |  531 | `	{ "array_key_exists", "$key, array $array", "bool" },` |
|         - |  532 | `	{ "array_key_first", "array $array", "string\|int\|null" },` |
|         - |  533 | `	{ "array_key_last", "array $array", "string\|int\|null" },` |
|         - |  534 | `	{ "array_keys", "array $array, mixed $filter_value = ?, bool $strict = false", "array" },` |
|         - |  535 | `	{ "array_last", "array $array", "mixed" },` |
|         - |  536 | `	{ "array_map", "?callable $callback, array $array, array ...$arrays = ?", "array" },` |
|         - |  537 | `	{ "array_merge", "array ...$arrays = ?", "array" },` |
|         - |  538 | `	{ "array_multisort", "&$array, &...$rest = ?", "true" },` |
|         - |  539 | `	{ "array_merge_recursive", "array ...$arrays = ?", "array" },` |
|         - |  540 | `	{ "array_pad", "array $array, int $length, mixed $value", "array" },` |
|         - |  541 | `	{ "array_pop", "array &$array", "mixed" },` |
|         - |  542 | `	{ "array_product", "array $array", "int\|float" },` |
|         - |  543 | `	{ "array_push", "array &$array, mixed ...$values = ?", "int" },` |
|         - |  544 | `	{ "array_rand", "array $array, int $num = 1", "array\|string\|int" },` |
|         - |  545 | `	{ "array_reduce", "array $array, callable $callback, mixed $initial = NULL", "mixed" },` |
|         - |  546 | `	{ "array_replace", "array $array, array ...$replacements = ?", "array" },` |
|         - |  547 | `	{ "array_reverse", "array $array, bool $preserve_keys = false", "array" },` |
|         - |  548 | `	{ "array_search", "mixed $needle, array $haystack, bool $strict = false", "string\|int\|false" },` |
|         - |  549 | `	{ "array_shift", "array &$array", "mixed" },` |
|         - |  550 | `	{ "array_slice", "array $array, int $offset, ?int $length = NULL, bool $preserve_keys = false", "array" },` |
|         - |  551 | `	{ "array_splice", "array &$array, int $offset, ?int $length = NULL, mixed $replacement = ?", "array" },` |
|         - |  552 | `	{ "array_sum", "array $array", "int\|float" },` |
|         - |  553 | `	{ "array_udiff", "array $array, ...$rest = ?", "array" },` |
|         - |  554 | `	{ "array_udiff_assoc", "array $array, ...$rest = ?", "array" },` |
|         - |  555 | `	{ "array_udiff_uassoc", "array $array, ...$rest = ?", "array" },` |
|         - |  556 | `	{ "array_uintersect", "array $array, ...$rest = ?", "array" },` |
|         - |  557 | `	{ "array_uintersect_assoc", "array $array, ...$rest = ?", "array" },` |
|         - |  558 | `	{ "array_uintersect_uassoc", "array $array, ...$rest = ?", "array" },` |
|         - |  559 | `	{ "array_unique", "array $array, int $flags = 2", "array" },` |
|         - |  560 | `	{ "array_unshift", "array &$array, mixed ...$values = ?", "int" },` |
|         - |  561 | `	{ "array_values", "array $array", "array" },` |
|         - |  562 | `	{ "array_walk", "object\|array &$array, callable $callback, mixed $arg = ?", "true" },` |
|         - |  563 | `	{ "array_walk_recursive", "object\|array &$array, callable $callback, mixed $arg = ?", "true" },` |
|         - |  564 | `	{ "arsort", "array &$array, int $flags = 0", "true" },` |
|         - |  565 | `	{ "asin", "float $num", "float" },` |
|         - |  566 | `	{ "asinh", "float $num", "float" },` |
|         - |  567 | `	{ "asort", "array &$array, int $flags = 0", "true" },` |
|         - |  568 | `	{ "assert", "mixed $assertion, Throwable\|string\|null $description = NULL", "bool" },` |
|         - |  569 | `	{ "atan", "float $num", "float" },` |
|         - |  570 | `	{ "atanh", "float $num", "float" },` |
|         - |  571 | `	{ "atan2", "float $y, float $x", "float" },` |
|         - |  572 | `	{ "base64_decode", "string $string, bool $strict = false", "string\|false" },` |
|         - |  573 | `	{ "base64_encode", "string $string", "string" },` |
|         - |  574 | `	{ "base_convert", "string $num, int $from_base, int $to_base", "string" },` |
|         - |  575 | `	{ "basename", "string $path, string $suffix = ''", "string" },` |
|         - |  576 | `	{ "bin2hex", "string $string", "string" },` |
|         - |  577 | `	{ "bindec", "string $binary_string", "int\|float" },` |
|         - |  578 | `	{ "boolval", "mixed $value", "bool" },` |
|         - |  579 | `	{ "call_user_func", "callable $callback, mixed ...$args = ?", "mixed" },` |
|         - |  580 | `	{ "call_user_func_array", "callable $callback, array $args", "mixed" },` |
|         - |  581 | `	{ "ceil", "int\|float $num", "float" },` |
|         - |  582 | `	{ "chdir", "string $directory", "bool" },` |
|         - |  583 | `	{ "chgrp", "string $filename, string\|int $group", "bool" },` |
|         - |  584 | `	{ "chmod", "string $filename, int $permissions", "bool" },` |
|         - |  585 | `	{ "chop", "string $string, string $characters = ?", "string" },` |
|         - |  586 | `	{ "chown", "string $filename, string\|int $user", "bool" },` |
|         - |  587 | `	{ "chr", "int $codepoint", "string" },` |
|         - |  588 | `	{ "chroot", "string $directory", "bool" },` |
|         - |  589 | `	{ "chunk_split", "string $string, int $length = 76, string $separator = ?", "string" },` |
|         - |  590 | `	{ "class_alias", "string $class, string $alias, bool $autoload = true", "bool" },` |
|         - |  591 | `	{ "class_exists", "string $class, bool $autoload = true", "bool" },` |
|         - |  592 | `	{ "enum_exists", "string $enum, bool $autoload = true", "bool" },` |
|         - |  593 | `	{ "clone", "object $object, array $withProperties = []", "object" },` |
|         - |  594 | `	{ "closedir", "$dir_handle = NULL", "void" },` |
|         - |  595 | `	{ "compact", "$var_name, ...$var_names = ?", "array" },` |
|         - |  596 | `	{ "constant", "string $name", "mixed" },` |
|         - |  597 | `	{ "convert_uudecode", "string $string", "string\|false" },` |
|         - |  598 | `	{ "convert_uuencode", "string $string", "string" },` |
|         - |  599 | `	{ "copy", "string $from, string $to, $context = NULL", "bool" },` |
|         - |  600 | `	{ "cos", "float $num", "float" },` |
|         - |  601 | `	{ "cosh", "float $num", "float" },` |
|         - |  602 | `	{ "count", "Countable\|array $value, int $mode = 0", "int" },` |
|         - |  603 | `	{ "count_chars", "string $string, int $mode = 0", "array\|string" },` |
|         - |  604 | `	{ "crc32", "string $string", "int" },` |
|         - |  605 | `	{ "ctype_alnum", "mixed $text", "bool" },` |
|         - |  606 | `	{ "ctype_alpha", "mixed $text", "bool" },` |
|         - |  607 | `	{ "ctype_cntrl", "mixed $text", "bool" },` |
|         - |  608 | `	{ "ctype_digit", "mixed $text", "bool" },` |
|         - |  609 | `	{ "ctype_graph", "mixed $text", "bool" },` |
|         - |  610 | `	{ "ctype_lower", "mixed $text", "bool" },` |
|         - |  611 | `	{ "ctype_print", "mixed $text", "bool" },` |
|         - |  612 | `	{ "ctype_punct", "mixed $text", "bool" },` |
|         - |  613 | `	{ "ctype_space", "mixed $text", "bool" },` |
|         - |  614 | `	{ "ctype_upper", "mixed $text", "bool" },` |
|         - |  615 | `	{ "ctype_xdigit", "mixed $text", "bool" },` |
|         - |  616 | `	{ "current", "object\|array $array", "mixed" },` |
|         - |  617 | `	{ "date", "string $format, ?int $timestamp = NULL", "string" },` |
|         - |  618 | `	{ "date_add", "DateTime $object, DateInterval $interval", "DateTime" },` |
|         - |  619 | `	{ "date_create", "string $datetime = 'now', ?DateTimeZone $timezone = NULL", "DateTime\|false" },` |
|         - |  620 | `	{ "date_create_from_format", "string $format, string $datetime, ?DateTimeZone $timezone = NULL", "DateTime\|false" },` |
|         - |  621 | `	{ "date_create_immutable", "string $datetime = 'now', ?DateTimeZone $timezone = NULL", "DateTimeImmutable\|false" },` |
|         - |  622 | `	{ "date_create_immutable_from_format", "string $format, string $datetime, ?DateTimeZone $timezone = NULL", "DateTimeImmutable\|false" },` |
|         - |  623 | `	{ "date_date_set", "DateTime $object, int $year, int $month, int $day", "DateTime" },` |
|         - |  624 | `	{ "date_diff", "DateTimeInterface $baseObject, DateTimeInterface $targetObject, bool $absolute = false", "DateInterval" },` |
|         - |  625 | `	{ "date_format", "DateTimeInterface $object, string $format", "string" },` |
|         - |  626 | `	{ "date_get_last_errors", "", "array\|false" },` |
|         - |  627 | `	{ "date_interval_create_from_date_string", "string $datetime", "DateInterval\|false" },` |
|         - |  628 | `	{ "date_interval_format", "DateInterval $object, string $format", "string" },` |
|         - |  629 | `	{ "date_isodate_set", "DateTime $object, int $year, int $week, int $dayOfWeek = 1", "DateTime" },` |
|         - |  630 | `	{ "date_modify", "DateTime $object, string $modifier", "DateTime\|false" },` |
|         - |  631 | `	{ "date_offset_get", "DateTimeInterface $object", "int" },` |
|         - |  632 | `	{ "date_sub", "DateTime $object, DateInterval $interval", "DateTime" },` |
|         - |  633 | `	{ "date_time_set", "DateTime $object, int $hour, int $minute, int $second = 0, int $microsecond = 0", "DateTime" },` |
|         - |  634 | `	{ "date_timestamp_get", "DateTimeInterface $object", "int" },` |
|         - |  635 | `	{ "date_timestamp_set", "DateTime $object, int $timestamp", "DateTime" },` |
|         - |  636 | `	{ "date_timezone_get", "DateTimeInterface $object", "DateTimeZone\|false" },` |
|         - |  637 | `	{ "date_timezone_set", "DateTime $object, DateTimeZone $timezone", "DateTime" },` |
|         - |  638 | `	{ "timezone_name_get", "DateTimeZone $object", "string" },` |
|         - |  639 | `	{ "timezone_offset_get", "DateTimeZone $object, DateTimeInterface $datetime", "int" },` |
|         - |  640 | `	{ "timezone_open", "string $timezone", "DateTimeZone\|false" },` |
|         - |  641 | `	{ "date_default_timezone_get", "", "string" },` |
|         - |  642 | `	{ "date_default_timezone_set", "string $timezoneId", "bool" },` |
|         - |  643 | `	{ "debug_backtrace", "int $options = 1, int $limit = 0", "array" },` |
|         - |  644 | `	{ "debug_print_backtrace", "int $options = 0, int $limit = 0", "void" },` |
|         - |  645 | `	{ "decbin", "int $num", "string" },` |
|         - |  646 | `	{ "dechex", "int $num", "string" },` |
|         - |  647 | `	{ "decoct", "int $num", "string" },` |
|         - |  648 | `	{ "define", "string $constant_name, mixed $value, bool $case_insensitive = false", "bool" },` |
|         - |  649 | `	{ "defined", "string $constant_name", "bool" },` |
|         - |  650 | `	{ "deg2rad", "float $num", "float" },` |
|         - |  651 | `	{ "die", "string\|int $status = 0", "never" },` |
|         - |  652 | `	{ "dir", "string $directory, $context = NULL", "Directory\|false" },` |
|         - |  653 | `	{ "dirname", "string $path, int $levels = 1", "string" },` |
|         - |  654 | `	{ "disk_free_space", "string $directory", "float\|false" },` |
|         - |  655 | `	{ "disk_total_space", "string $directory", "float\|false" },` |
|         - |  656 | `	{ "diskfreespace", "string $directory", "float\|false" },` |
|         - |  657 | `	{ "end", "object\|array &$array", "mixed" },` |
|         - |  658 | `	{ "error_get_last", "", "?array" },` |
|         - |  659 | `	{ "error_clear_last", "", "void" },` |
|         - |  660 | `	{ "error_log", "string $message, int $message_type = 0, ?string $destination = NULL, ?string $additional_headers = NULL", "bool" },` |
|         - |  661 | `	{ "error_reporting", "?int $error_level = NULL", "int" },` |
|         - |  662 | `	{ "escapeshellarg", "string $arg", "string" },` |
|         - |  663 | `	{ "escapeshellcmd", "string $command", "string" },` |
|         - |  664 | `	{ "exec", "string $command, &$output = NULL, &$result_code = NULL", "string\|false" },` |
|         - |  665 | `	{ "exit", "string\|int $status = 0", "never" },` |
|         - |  666 | `	{ "exp", "float $num", "float" },` |
|         - |  667 | `	{ "expm1", "float $num", "float" },` |
|         - |  668 | `	{ "explode", "string $separator, string $string, int $limit = 9223372036854775807", "array" },` |
|         - |  669 | `	{ "extension_loaded", "string $extension", "bool" },` |
|         - |  670 | `	{ "extract", "array &$array, int $flags = 0, string $prefix = ''", "int" },` |
|         - |  671 | `	{ "fclose", "$stream", "bool" },` |
|         - |  672 | `	{ "feof", "$stream", "bool" },` |
|         - |  673 | `	{ "fflush", "$stream", "bool" },` |
|         - |  674 | `	{ "fgetc", "$stream", "string\|false" },` |
|         - |  675 | `	{ "fgetcsv", "$stream, ?int $length = NULL, string $separator = ',', string $enclosure = '\"', string $escape = '\\\\'", "array\|false" },` |
|         - |  676 | `	{ "fgets", "$stream, ?int $length = NULL", "string\|false" },` |
|         - |  677 | `	{ "file", "string $filename, int $flags = 0, $context = NULL", "array\|false" },` |
|         - |  678 | `	{ "file_exists", "string $filename", "bool" },` |
|         - |  679 | `	{ "file_get_contents", "string $filename, bool $use_include_path = false, $context = NULL, int $offset = 0, ?int $length = NULL", "string\|false" },` |
|         - |  680 | `	{ "file_put_contents", "string $filename, mixed $data, int $flags = 0, $context = NULL", "int\|false" },` |
|         - |  681 | `	{ "fileatime", "string $filename", "int\|false" },` |
|         - |  682 | `	{ "filectime", "string $filename", "int\|false" },` |
|         - |  683 | `	{ "filegroup", "string $filename", "int\|false" },` |
|         - |  684 | `	{ "fileinode", "string $filename", "int\|false" },` |
|         - |  685 | `	{ "filemtime", "string $filename", "int\|false" },` |
|         - |  686 | `	{ "fileowner", "string $filename", "int\|false" },` |
|         - |  687 | `	{ "fileperms", "string $filename", "int\|false" },` |
|         - |  688 | `	{ "filesize", "string $filename", "int\|false" },` |
|         - |  689 | `	{ "filetype", "string $filename", "string\|false" },` |
|         - |  690 | `	{ "filter_has_var", "int $input_type, string $var_name", "bool" },` |
|         - |  691 | `	{ "filter_id", "string $name", "int\|false" },` |
|         - |  692 | `	{ "filter_input", "int $type, string $var_name, int $filter = 516, array\|int $options = 0", "mixed" },` |
|         - |  693 | `	{ "filter_input_array", "int $type, array\|int $options = 516, bool $add_empty = true", "array\|false\|null" },` |
|         - |  694 | `	{ "filter_list", "", "array" },` |
|         - |  695 | `	{ "filter_var", "mixed $value, int $filter = 516, array\|int $options = 0", "mixed" },` |
|         - |  696 | `	{ "filter_var_array", "array $array, array\|int $options = 516, bool $add_empty = true", "array\|false\|null" },` |
|         - |  697 | `	{ "floatval", "mixed $value", "float" },` |
|         - |  698 | `	{ "flock", "$stream, int $operation, &$would_block = NULL", "bool" },` |
|         - |  699 | `	{ "floor", "int\|float $num", "float" },` |
|         - |  700 | `	{ "flush", "", "void" },` |
|         - |  701 | `	{ "fmod", "float $num1, float $num2", "float" },` |
|         - |  702 | `	{ "fnmatch", "string $pattern, string $filename, int $flags = 0", "bool" },` |
|         - |  703 | `	{ "fopen", "string $filename, string $mode, bool $use_include_path = false, $context = NULL", "" },` |
|         - |  704 | `	{ "forward_static_call", "callable $callback, mixed ...$args = ?", "mixed" },` |
|         - |  705 | `	{ "forward_static_call_array", "callable $callback, array $args", "mixed" },` |
|         - |  706 | `	{ "fpow", "float $num, float $exponent", "float" },` |
|         - |  707 | `	{ "fpassthru", "$stream", "int" },` |
|         - |  708 | `	{ "fprintf", "$stream, string $format, mixed ...$values = ?", "int" },` |
|         - |  709 | `	{ "fputcsv", "$stream, array $fields, string $separator = ',', string $enclosure = '\"', string $escape = '\\\\', string $eol = ?", "int\|false" },` |
|         - |  710 | `	{ "fputs", "$stream, string $data, ?int $length = NULL", "int\|false" },` |
|         - |  711 | `	{ "fread", "$stream, int $length", "string\|false" },` |
|         - |  712 | `	{ "fseek", "$stream, int $offset, int $whence = 0", "int" },` |
|         - |  713 | `	{ "fstat", "$stream", "array\|false" },` |
|         - |  714 | `	{ "ftell", "$stream", "int\|false" },` |
|         - |  715 | `	{ "ftruncate", "$stream, int $size", "bool" },` |
|         - |  716 | `	{ "func_get_arg", "int $position", "mixed" },` |
|         - |  717 | `	{ "func_get_args", "", "array" },` |
|         - |  718 | `	{ "func_num_args", "", "int" },` |
|         - |  719 | `	{ "function_exists", "string $function", "bool" },` |
|         - |  720 | `	{ "fwrite", "$stream, string $data, ?int $length = NULL", "int\|false" },` |
|         - |  721 | `	{ "gc_collect_cycles", "", "int" },` |
|         - |  722 | `	{ "gc_disable", "", "void" },` |
|         - |  723 | `	{ "gc_enable", "", "void" },` |
|         - |  724 | `	{ "gc_enabled", "", "bool" },` |
|         - |  725 | `	{ "gc_mem_caches", "", "int" },` |
|         - |  726 | `	{ "gc_status", "", "array" },` |
|         - |  727 | `	{ "get_called_class", "", "string" },` |
|         - |  728 | `	{ "get_class", "object $object = ?", "string" },` |
|         - |  729 | `	{ "get_class_methods", "object\|string $object_or_class", "array" },` |
|         - |  730 | `	{ "get_class_vars", "string $class", "array" },` |
|         - |  731 | `	{ "get_current_user", "", "string" },` |
|         - |  732 | `	{ "get_declared_classes", "", "array" },` |
|         - |  733 | `	{ "get_declared_interfaces", "", "array" },` |
|         - |  734 | `	{ "get_declared_traits", "", "array" },` |
|         - |  735 | `	{ "get_defined_constants", "bool $categorize = false", "array" },` |
|         - |  736 | `	{ "get_defined_functions", "bool $exclude_disabled = true", "array" },` |
|         - |  737 | `	{ "get_defined_vars", "", "array" },` |
|         - |  738 | `	{ "get_html_translation_table", "int $table = 0, int $flags = 11, string $encoding = 'UTF-8'", "array" },` |
|         - |  739 | `	{ "get_include_path", "", "string\|false" },` |
|         - |  740 | `	{ "get_included_files", "", "array" },` |
|         - |  741 | `	{ "get_loaded_extensions", "bool $zend_extensions = false", "array" },` |
|         - |  742 | `	{ "get_mangled_object_vars", "object $object", "array" },` |
|         - |  743 | `	{ "get_object_vars", "object $object", "array" },` |
|         - |  744 | `	{ "get_parent_class", "object\|string $object_or_class = ?", "string\|false" },` |
|         - |  745 | `	{ "get_resource_id", "$resource", "int" },` |
|         - |  746 | `	{ "get_resource_type", "$resource", "string" },` |
|         - |  747 | `	{ "getcwd", "", "string\|false" },` |
|         - |  748 | `	{ "getdate", "?int $timestamp = NULL", "array" },` |
|         - |  749 | `	{ "getenv", "?string $name = NULL, bool $local_only = false", "array\|string\|false" },` |
|         - |  750 | `	{ "getmygid", "", "int\|false" },` |
|         - |  751 | `	{ "getmypid", "", "int\|false" },` |
|         - |  752 | `	{ "getmyuid", "", "int\|false" },` |
|         - |  753 | `	{ "getopt", "string $short_options, array $long_options = ?, &$rest_index = NULL", "array\|false" },` |
|         - |  754 | `	{ "getrandmax", "", "int" },` |
|         - |  755 | `	{ "gettimeofday", "bool $as_float = false", "array\|float" },` |
|         - |  756 | `	{ "gettype", "mixed $value", "string" },` |
|         - |  757 | `	{ "get_debug_type", "mixed $value", "string" },` |
|         - |  758 | `	{ "gmdate", "string $format, ?int $timestamp = NULL", "string" },` |
|         - |  759 | `	{ "gmmktime", "int $hour, ?int $minute = NULL, ?int $second = NULL, ?int $month = NULL, ?int $day = NULL, ?int $year = NULL", "int\|false" },` |
|         - |  760 | `	{ "hash", "string $algo, string $data, bool $binary = false, array $options = []", "string" },` |
|         - |  761 | `	{ "hash_algos", "", "array" },` |
|         - |  762 | `	{ "hash_equals", "string $known_string, string $user_string", "bool" },` |
|         - |  763 | `	{ "hash_hmac", "string $algo, string $data, string $key, bool $binary = false", "string" },` |
|         - |  764 | `	{ "hash_hmac_algos", "", "array" },` |
|         - |  765 | `	{ "hash_init", "string $algo, int $flags = 0, string $key = \'\', array $options = []", "HashContext" },` |
|         - |  766 | `	{ "hash_update", "HashContext $context, string $data", "bool" },` |
|         - |  767 | `	{ "hash_final", "HashContext $context, bool $binary = false", "string" },` |
|         - |  768 | `	{ "hash_copy", "HashContext $context", "HashContext" },` |
|         - |  769 | `	{ "hash_file", "string $algo, string $filename, bool $binary = false, array $options = []", "string\|false" },` |
|         - |  770 | `	{ "hash_hkdf", "string $algo, string $key, int $length = 0, string $info = \'\', string $salt = \'\'", "string" },` |
|         - |  771 | `	{ "hash_pbkdf2", "string $algo, string $password, string $salt, int $iterations, int $length = 0, bool $binary = false, array $options = []", "string" },` |
|         - |  772 | `	{ "hash_hmac_file", "string $algo, string $filename, string $key, bool $binary = false", "string\|false" },` |
|         - |  773 | `	{ "hash_update_file", "HashContext $context, string $filename, $stream_context = null", "bool" },` |
|         - |  774 | `	{ "hash_update_stream", "HashContext $context, $stream, int $length = -1", "int" },` |
|         - |  775 | `	{ "header", "string $header, bool $replace = true, int $response_code = 0", "void" },` |
|         - |  776 | `	{ "header_remove", "?string $name = NULL", "void" },` |
|         - |  777 | `	{ "headers_list", "", "array" },` |
|         - |  778 | `	{ "headers_sent", "&$filename = NULL, &$line = NULL", "bool" },` |
|         - |  779 | `	{ "hexdec", "string $hex_string", "int\|float" },` |
|         - |  780 | `	{ "html_entity_decode", "string $string, int $flags = 11, ?string $encoding = NULL", "string" },` |
|         - |  781 | `	{ "http_build_query", "object\|array $data, string $numeric_prefix = '', ?string $arg_separator = null, int $encoding_type = 1", "string" },` |
|         - |  782 | `	{ "htmlentities", "string $string, int $flags = 11, ?string $encoding = NULL, bool $double_encode = true", "string" },` |
|         - |  783 | `	{ "htmlspecialchars", "string $string, int $flags = 11, ?string $encoding = NULL, bool $double_encode = true", "string" },` |
|         - |  784 | `	{ "htmlspecialchars_decode", "string $string, int $flags = 11", "string" },` |
|         - |  785 | `	{ "http_response_code", "int $response_code = 0", "int\|bool" },` |
|         - |  786 | `	{ "hypot", "float $x, float $y", "float" },` |
|         - |  787 | `	{ "idate", "string $format, ?int $timestamp = NULL", "int\|false" },` |
|         - |  788 | `	{ "implode", "array\|string $separator, ?array $array = NULL", "string" },` |
|         - |  789 | `	{ "in_array", "mixed $needle, array $haystack, bool $strict = false", "bool" },` |
|         - |  790 | `	{ "intdiv", "int $num1, int $num2", "int" },` |
|         - |  791 | `	{ "interface_exists", "string $interface, bool $autoload = true", "bool" },` |
|         - |  792 | `	{ "trait_exists", "string $trait, bool $autoload = true", "bool" },` |
|         - |  793 | `	{ "intval", "mixed $value, int $base = 10", "int" },` |
|         - |  794 | `	{ "is_a", "mixed $object_or_class, string $class, bool $allow_string = false", "bool" },` |
|         - |  795 | `	{ "is_array", "mixed $value", "bool" },` |
|         - |  796 | `	{ "is_bool", "mixed $value", "bool" },` |
|         - |  797 | `	{ "is_callable", "mixed $value, bool $syntax_only = false, &$callable_name = NULL", "bool" },` |
|         - |  798 | `	{ "is_dir", "string $filename", "bool" },` |
|         - |  799 | `	{ "is_double", "mixed $value", "bool" },` |
|         - |  800 | `	{ "is_executable", "string $filename", "bool" },` |
|         - |  801 | `	{ "is_file", "string $filename", "bool" },` |
|         - |  802 | `	{ "is_float", "mixed $value", "bool" },` |
|         - |  803 | `	{ "is_int", "mixed $value", "bool" },` |
|         - |  804 | `	{ "is_integer", "mixed $value", "bool" },` |
|         - |  805 | `	{ "is_link", "string $filename", "bool" },` |
|         - |  806 | `	{ "is_long", "mixed $value", "bool" },` |
|         - |  807 | `	{ "is_null", "mixed $value", "bool" },` |
|         - |  808 | `	{ "is_numeric", "mixed $value", "bool" },` |
|         - |  809 | `	{ "is_object", "mixed $value", "bool" },` |
|         - |  810 | `	{ "is_readable", "string $filename", "bool" },` |
|         - |  811 | `	{ "is_resource", "mixed $value", "bool" },` |
|         - |  812 | `	{ "is_scalar", "mixed $value", "bool" },` |
|         - |  813 | `	{ "is_string", "mixed $value", "bool" },` |
|         - |  814 | `	{ "is_subclass_of", "mixed $object_or_class, string $class, bool $allow_string = true", "bool" },` |
|         - |  815 | `	{ "is_writable", "string $filename", "bool" },` |
|         - |  816 | `	{ "iterator_apply", "Traversable $iterator, callable $callback, ?array $args = NULL", "int" },` |
|         - |  817 | `	{ "iterator_count", "Traversable\|array $iterator", "int" },` |
|         - |  818 | `	{ "iterator_to_array", "Traversable\|array $iterator, bool $preserve_keys = true", "array" },` |
|         - |  819 | `	{ "join", "array\|string $separator, ?array $array = NULL", "string" },` |
|         - |  820 | `	{ "json_decode", "string $json, ?bool $associative = NULL, int $depth = 512, int $flags = 0", "mixed" },` |
|         - |  821 | `	{ "json_encode", "mixed $value, int $flags = 0, int $depth = 512", "string\|false" },` |
|         - |  822 | `	{ "json_last_error", "", "int" },` |
|         - |  823 | `	{ "json_last_error_msg", "", "string" },` |
|         - |  824 | `	{ "json_validate", "string $json, int $depth = 512, int $flags = 0", "bool" },` |
|         - |  825 | `	{ "key", "object\|array $array", "string\|int\|null" },` |
|         - |  826 | `	{ "key_exists", "$key, array $array", "bool" },` |
|         - |  827 | `	{ "krsort", "array &$array, int $flags = 0", "true" },` |
|         - |  828 | `	{ "ksort", "array &$array, int $flags = 0", "true" },` |
|         - |  829 | `	{ "lcfirst", "string $string", "string" },` |
|         - |  830 | `	{ "levenshtein", "string $string1, string $string2, int $insertion_cost = 1, int $replacement_cost = 1, int $deletion_cost = 1", "int" },` |
|         - |  831 | `	{ "link", "string $target, string $link", "bool" },` |
|         - |  832 | `	{ "localtime", "?int $timestamp = NULL, bool $associative = false", "array" },` |
|         - |  833 | `	{ "log", "float $num, float $base = 2.718281828459045", "float" },` |
|         - |  834 | `	{ "log10", "float $num", "float" },` |
|         - |  835 | `	{ "log1p", "float $num", "float" },` |
|         - |  836 | `	{ "lstat", "string $filename", "array\|false" },` |
|         - |  837 | `	{ "ltrim", "string $string, string $characters = ?", "string" },` |
|         - |  838 | `	{ "max", "mixed $value, mixed ...$values = ?", "mixed" },` |
|         - |  839 | `	{ "mb_chr", "int $codepoint, ?string $encoding = NULL", "string\|false" },` |
|         - |  840 | `	{ "mb_convert_encoding", "array\|string $string, string $to_encoding, array\|string\|null $from_encoding = NULL", "array\|string\|false" },` |
|         - |  841 | `	{ "mb_ord", "string $string, ?string $encoding = NULL", "int\|false" },` |
|         - |  842 | `	{ "mb_ltrim", "string $string, ?string $characters = null, ?string $encoding = null", "string" },` |
|         - |  843 | `	{ "mb_rtrim", "string $string, ?string $characters = null, ?string $encoding = null", "string" },` |
|         - |  844 | `	{ "mb_lcfirst", "string $string, ?string $encoding = null", "string" },` |
|         - |  845 | `	{ "mb_strtolower", "string $string, ?string $encoding = NULL", "string" },` |
|         - |  846 | `	{ "mb_strtoupper", "string $string, ?string $encoding = NULL", "string" },` |
|         - |  847 | `	{ "mb_trim", "string $string, ?string $characters = null, ?string $encoding = null", "string" },` |
|         - |  848 | `	{ "mb_ucfirst", "string $string, ?string $encoding = null", "string" },` |
|         - |  849 | `	{ "iconv", "string $from_encoding, string $to_encoding, string $string", "string\|false" },` |
|         - |  850 | `	{ "iconv_strlen", "string $string, ?string $encoding = NULL", "int\|false" },` |
|         - |  851 | `	{ "iconv_substr", "string $string, int $offset, ?int $length = NULL, ?string $encoding = NULL", "string\|false" },` |
|         - |  852 | `	{ "iconv_strpos", "string $haystack, string $needle, int $offset = 0, ?string $encoding = NULL", "int\|false" },` |
|         - |  853 | `	{ "iconv_strrpos", "string $haystack, string $needle, ?string $encoding = NULL", "int\|false" },` |
|         - |  854 | `	{ "iconv_get_encoding", "string $type = \"all\"", "array\|string\|false" },` |
|         - |  855 | `	{ "iconv_mime_encode", "string $field_name, string $field_value, array $options = []", "string\|false" },` |
|         - |  856 | `	{ "iconv_mime_decode", "string $string, int $mode = 0, ?string $encoding = NULL", "string\|false" },` |
|         - |  857 | `	{ "iconv_mime_decode_headers", "string $headers, int $mode = 0, ?string $encoding = NULL", "array\|false" },` |
|         - |  858 | `	{ "md5", "string $string, bool $binary = false", "string" },` |
|         - |  859 | `	{ "md5_file", "string $filename, bool $binary = false", "string\|false" },` |
|         - |  860 | `	{ "metaphone", "string $string, int $max_phonemes = 0", "string" },` |
|         - |  861 | `	{ "method_exists", "$object_or_class, string $method", "bool" },` |
|         - |  862 | `	{ "memory_get_peak_usage", "bool $real_usage = false", "int" },` |
|         - |  863 | `	{ "memory_get_usage", "bool $real_usage = false", "int" },` |
|         - |  864 | `	{ "microtime", "bool $as_float = false", "string\|float" },` |
|         - |  865 | `	{ "min", "mixed $value, mixed ...$values = ?", "mixed" },` |
|         - |  866 | `	{ "mkdir", "string $directory, int $permissions = 511, bool $recursive = false, $context = NULL", "bool" },` |
|         - |  867 | `	{ "mktime", "int $hour, ?int $minute = NULL, ?int $second = NULL, ?int $month = NULL, ?int $day = NULL, ?int $year = NULL", "int\|false" },` |
|         - |  868 | `	{ "mt_getrandmax", "", "int" },` |
|         - |  869 | `	{ "mt_rand", "int $min = ?, int $max = ?", "int" },` |
|         - |  870 | `	{ "mt_srand", "?int $seed = NULL, int $mode = 0", "void" },` |
|         - |  871 | `	{ "natcasesort", "array &$array", "true" },` |
|         - |  872 | `	{ "natsort", "array &$array", "true" },` |
|         - |  873 | `	{ "next", "object\|array &$array", "mixed" },` |
|         - |  874 | `	{ "nl2br", "string $string, bool $use_xhtml = true", "string" },` |
|         - |  875 | `	{ "number_format", "float $num, int $decimals = 0, ?string $decimal_separator = '.', ?string $thousands_separator = ','", "string" },` |
|         - |  876 | `	{ "ob_clean", "", "bool" },` |
|         - |  877 | `	{ "ob_end_clean", "", "bool" },` |
|         - |  878 | `	{ "ob_end_flush", "", "bool" },` |
|         - |  879 | `	{ "ob_flush", "", "bool" },` |
|         - |  880 | `	{ "ob_get_clean", "", "string\|false" },` |
|         - |  881 | `	{ "ob_get_contents", "", "string\|false" },` |
|         - |  882 | `	{ "ob_get_flush", "", "string\|false" },` |
|         - |  883 | `	{ "ob_get_length", "", "int\|false" },` |
|         - |  884 | `	{ "ob_get_level", "", "int" },` |
|         - |  885 | `	{ "ob_get_status", "bool $full_status = false", "array" },` |
|         - |  886 | `	{ "ob_implicit_flush", "bool $enable = true", "void" },` |
|         - |  887 | `	{ "ob_list_handlers", "", "array" },` |
|         - |  888 | `	{ "ob_start", "$callback = NULL, int $chunk_size = 0, int $flags = 112", "bool" },` |
|         - |  889 | `	{ "octdec", "string $octal_string", "int\|float" },` |
|         - |  890 | `	{ "opendir", "string $directory, $context = NULL", "" },` |
|         - |  891 | `	{ "ord", "string $character", "int" },` |
|         - |  892 | `	{ "pack", "string $format, mixed ...$values = ?", "string" },` |
|         - |  893 | `	{ "unpack", "string $format, string $string, int $offset = 0", "array\|false" },` |
|         - |  894 | `	{ "parse_ini_file", "string $filename, bool $process_sections = false, int $scanner_mode = 0", "array\|false" },` |
|         - |  895 | `	{ "parse_ini_string", "string $ini_string, bool $process_sections = false, int $scanner_mode = 0", "array\|false" },` |
|         - |  896 | `	{ "parse_str", "string $string, &$result", "void" },` |
|         - |  897 | `	{ "parse_url", "string $url, int $component = -1", "array\|string\|int\|false\|null" },` |
|         - |  898 | `	{ "crypt", "string $string, string $salt", "string" },` |
|         - |  899 | `	{ "password_algos", "", "array" },` |
|         - |  900 | `	{ "password_get_info", "string $hash", "array" },` |
|         - |  901 | `	{ "password_hash", "string $password, string\|int\|null $algo, array $options = ?", "string" },` |
|         - |  902 | `	{ "password_needs_rehash", "string $hash, string\|int\|null $algo, array $options = ?", "bool" },` |
|         - |  903 | `	{ "password_verify", "string $password, string $hash", "bool" },` |
|         - |  904 | `	{ "passthru", "string $command, &$result_code = NULL", "?false" },` |
|         - |  905 | `	{ "pathinfo", "string $path, int $flags = 15", "array\|string" },` |
|         - |  906 | `	{ "pclose", "$handle", "int" },` |
|         - |  907 | `	{ "php_sapi_name", "", "string\|false" },` |
|         - |  908 | `	{ "php_uname", "string $mode = 'a'", "string" },` |
|         - |  909 | `	{ "phpinfo", "int $flags = 4294967295", "true" },` |
|         - |  910 | `	{ "phpversion", "?string $extension = NULL", "string\|false" },` |
|         - |  911 | `	{ "pi", "", "float" },` |
|         - |  912 | `	{ "popen", "string $command, string $mode", "" },` |
|         - |  913 | `	{ "pos", "object\|array $array", "mixed" },` |
|         - |  914 | `	{ "pow", "mixed $num, mixed $exponent", "object\|int\|float" },` |
|         - |  915 | `	{ "preg_last_error", "", "int" },` |
|         - |  916 | `	{ "preg_last_error_msg", "", "string" },` |
|         - |  917 | `	{ "fsockopen", "string $hostname, int $port = -1, &$error_code = NULL, &$error_message = NULL, ?float $timeout = NULL", "" },` |
|         - |  918 | `	{ "pfsockopen", "string $hostname, int $port = -1, &$error_code = NULL, &$error_message = NULL, ?float $timeout = NULL", "" },` |
|         - |  919 | `	{ "stream_socket_client", "string $address, &$error_code = NULL, &$error_message = NULL, ?float $timeout = NULL, int $flags = 4, $context = NULL", "" },` |
|         - |  920 | `	{ "stream_socket_server", "string $address, &$error_code = NULL, &$error_message = NULL, int $flags = 12, $context = NULL", "" },` |
|         - |  921 | `	{ "stream_socket_accept", "$socket, ?float $timeout = NULL, &$peer_name = NULL", "" },` |
|         - |  922 | `	{ "stream_socket_get_name", "$socket, bool $remote", "string\|false" },` |
|         - |  923 | `	{ "stream_socket_pair", "int $domain, int $type, int $protocol", "array\|false" },` |
|         - |  924 | `	{ "stream_socket_shutdown", "$stream, int $mode", "bool" },` |
|         - |  925 | `	{ "stream_socket_recvfrom", "$socket, int $length, int $flags = 0, &$address = NULL", "string\|false" },` |
|         - |  926 | `	{ "stream_socket_sendto", "$socket, string $data, int $flags = 0, string $address = ''", "int\|false" },` |
|         - |  927 | `	{ "preg_match", "string $pattern, string $subject, &$matches = NULL, int $flags = 0, int $offset = 0", "int\|false" },` |
|         - |  928 | `	{ "preg_match_all", "string $pattern, string $subject, &$matches = NULL, int $flags = 0, int $offset = 0", "int\|false" },` |
|         - |  929 | `	{ "preg_quote", "string $str, ?string $delimiter = NULL", "string" },` |
|         - |  930 | `	{ "preg_replace", "array\|string $pattern, array\|string $replacement, array\|string $subject, int $limit = -1, &$count = NULL", "array\|string\|null" },` |
|         - |  931 | `	{ "preg_replace_callback", "array\|string $pattern, callable $callback, array\|string $subject, int $limit = -1, &$count = NULL, int $flags = 0", "array\|string\|null" },` |
|         - |  932 | `	{ "preg_split", "string $pattern, string $subject, int $limit = -1, int $flags = 0", "array\|false" },` |
|         - |  933 | `	{ "prev", "object\|array &$array", "mixed" },` |
|         - |  934 | `	{ "print_r", "mixed $value, bool $return = false", "string\|true" },` |
|         - |  935 | `	{ "printf", "string $format, mixed ...$values = ?", "int" },` |
|         - |  936 | `	{ "property_exists", "$object_or_class, string $property", "bool" },` |
|         - |  937 | `	{ "putenv", "string $assignment", "bool" },` |
|         - |  938 | `	{ "quotemeta", "string $string", "string" },` |
|         - |  939 | `	{ "rad2deg", "float $num", "float" },` |
|         - |  940 | `	{ "rand", "int $min = ?, int $max = ?", "int" },` |
|         - |  941 | `	{ "random_bytes", "int $length", "string" },` |
|         - |  942 | `	{ "random_int", "int $min, int $max", "int" },` |
|         - |  943 | `	{ "range", "string\|int\|float $start, string\|int\|float $end, int\|float $step = 1", "array" },` |
|         - |  944 | `	{ "rawurldecode", "string $string", "string" },` |
|         - |  945 | `	{ "rawurlencode", "string $string", "string" },` |
|         - |  946 | `	{ "readdir", "$dir_handle = NULL", "string\|false" },` |
|         - |  947 | `	{ "readfile", "string $filename, bool $use_include_path = false, $context = NULL", "int\|false" },` |
|         - |  948 | `	{ "readlink", "string $path", "string\|false" },` |
|         - |  949 | `	{ "realpath", "string $path", "string\|false" },` |
|         - |  950 | `	{ "stream_resolve_include_path", "string $filename", "string\|false" },` |
|         - |  951 | `	{ "register_shutdown_function", "callable $callback, mixed ...$args = ?", "void" },` |
|         - |  952 | `	{ "rename", "string $from, string $to, $context = NULL", "bool" },` |
|         - |  953 | `	{ "reset", "object\|array &$array", "mixed" },` |
|         - |  954 | `	{ "restore_error_handler", "", "true" },` |
|         - |  955 | `	{ "restore_exception_handler", "", "true" },` |
|         - |  956 | `	{ "rewind", "$stream", "bool" },` |
|         - |  957 | `	{ "rewinddir", "$dir_handle = NULL", "void" },` |
|         - |  958 | `	{ "rmdir", "string $directory, $context = NULL", "bool" },` |
|         - |  959 | `	{ "round", "int\|float $num, int $precision = 0, RoundingMode\|int $mode = ?", "float" },` |
|         - |  960 | `	{ "rsort", "array &$array, int $flags = 0", "true" },` |
|         - |  961 | `	{ "rtrim", "string $string, string $characters = ?", "string" },` |
|         - |  962 | `	{ "serialize", "mixed $value", "string" },` |
|         - |  963 | `	{ "set_error_handler", "?callable $callback, int $error_levels = 30719", "" },` |
|         - |  964 | `	{ "set_exception_handler", "?callable $callback", "" },` |
|         - |  965 | `	{ "get_error_handler", "", "?callable" },` |
|         - |  966 | `	{ "get_exception_handler", "", "?callable" },` |
|         - |  967 | `	{ "hrtime", "bool $as_number = false", "array\|int\|float\|false" },` |
|         - |  968 | `	{ "mb_check_encoding", "array\|string\|null $value = NULL, ?string $encoding = NULL", "bool" },` |
|         - |  969 | `	{ "mb_convert_case", "string $string, int $mode, ?string $encoding = NULL", "string" },` |
|         - |  970 | `	{ "mb_detect_encoding", "string $string, array\|string\|null $encodings = NULL, bool $strict = false", "string\|false" },` |
|         - |  971 | `	{ "mb_internal_encoding", "?string $encoding = NULL", "string\|bool" },` |
|         - |  972 | `	{ "mb_scrub", "string $string, ?string $encoding = null", "string" },` |
|         - |  973 | `	{ "mb_substitute_character", "string\|int\|null $substitute_character = null", "string\|int\|bool" },` |
|         - |  974 | `	{ "mb_str_split", "string $string, int $length = 1, ?string $encoding = NULL", "array" },` |
|         - |  975 | `	{ "mb_stripos", "string $haystack, string $needle, int $offset = 0, ?string $encoding = NULL", "int\|false" },` |
|         - |  976 | `	{ "mb_strlen", "string $string, ?string $encoding = NULL", "int" },` |
|         - |  977 | `	{ "mb_strpos", "string $haystack, string $needle, int $offset = 0, ?string $encoding = NULL", "int\|false" },` |
|         - |  978 | `	{ "mb_str_pad", "string $string, int $length, string $pad_string = \" \", int $pad_type = 1, ?string $encoding = null", "string" },` |
|         - |  979 | `	{ "mb_strcut", "string $string, int $start, ?int $length = null, ?string $encoding = null", "string" },` |
|         - |  980 | `	{ "mb_strimwidth", "string $string, int $start, int $width, string $trim_marker = \"\", ?string $encoding = null", "string" },` |
|         - |  981 | `	{ "mb_strrchr", "string $haystack, string $needle, bool $before_needle = false, ?string $encoding = null", "string\|false" },` |
|         - |  982 | `	{ "mb_strrichr", "string $haystack, string $needle, bool $before_needle = false, ?string $encoding = null", "string\|false" },` |
|         - |  983 | `	{ "mb_strripos", "string $haystack, string $needle, int $offset = 0, ?string $encoding = null", "int\|false" },` |
|         - |  984 | `	{ "mb_strrpos", "string $haystack, string $needle, int $offset = 0, ?string $encoding = NULL", "int\|false" },` |
|         - |  985 | `	{ "mb_stristr", "string $haystack, string $needle, bool $before_needle = false, ?string $encoding = null", "string\|false" },` |
|         - |  986 | `	{ "mb_strstr", "string $haystack, string $needle, bool $before_needle = false, ?string $encoding = null", "string\|false" },` |
|         - |  987 | `	{ "mb_substr_count", "string $haystack, string $needle, ?string $encoding = null", "int" },` |
|         - |  988 | `	{ "mb_strwidth", "string $string, ?string $encoding = NULL", "int" },` |
|         - |  989 | `	{ "mb_substr", "string $string, int $start, ?int $length = NULL, ?string $encoding = NULL", "string" },` |
|         - |  990 | `	{ "memory_reset_peak_usage", "", "void" },` |
|         - |  991 | `	{ "proc_close", "$process", "int" },` |
|         - |  992 | `	{ "proc_get_status", "$process", "array" },` |
|         - |  993 | `	{ "proc_nice", "int $priority", "bool" },` |
|         - |  994 | `	{ "proc_open", "array\|string $command, array $descriptor_spec, &$pipes, ?string $cwd = NULL, ?array $env_vars = NULL, ?array $options = NULL", "" },` |
|         - |  995 | `	{ "proc_terminate", "$process, int $signal = 15", "bool" },` |
|         - |  996 | `	{ "set_include_path", "string $include_path", "string\|false" },` |
|         - |  997 | `	{ "setcookie", "string $name, string $value = '', array\|int $expires_or_options = 0, string $path = '', string $domain = '', bool $secure = false, bool $httponly = false", "bool" },` |
|         - |  998 | `	{ "setrawcookie", "string $name, string $value = '', array\|int $expires_or_options = 0, string $path = '', string $domain = '', bool $secure = false, bool $httponly = false", "bool" },` |
|         - |  999 | `	{ "settype", "mixed &$var, string $type", "bool" },` |
|         - | 1000 | `	{ "sha1", "string $string, bool $binary = false", "string" },` |
|         - | 1001 | `	{ "sha1_file", "string $filename, bool $binary = false", "string\|false" },` |
|         - | 1002 | `	{ "shell_exec", "string $command", "string\|false\|null" },` |
|         - | 1003 | `	{ "shuffle", "array &$array", "true" },` |
|         - | 1004 | `	{ "similar_text", "string $string1, string $string2, &$percent = NULL", "int" },` |
|         - | 1005 | `	{ "sin", "float $num", "float" },` |
|         - | 1006 | `	{ "sinh", "float $num", "float" },` |
|         - | 1007 | `	{ "sizeof", "Countable\|array $value, int $mode = 0", "int" },` |
|         - | 1008 | `	{ "sleep", "int $seconds", "int" },` |
|         - | 1009 | `	{ "sort", "array &$array, int $flags = 0", "true" },` |
|         - | 1010 | `	{ "soundex", "string $string", "string" },` |
|         - | 1011 | `	{ "spl_autoload", "string $class, ?string $file_extensions = NULL", "void" },` |
|         - | 1012 | `	{ "spl_autoload_functions", "", "array" },` |
|         - | 1013 | `	{ "spl_autoload_register", "?callable $callback = NULL, bool $throw = true, bool $prepend = false", "bool" },` |
|         - | 1014 | `	{ "spl_autoload_unregister", "callable $callback", "bool" },` |
|         - | 1015 | `	{ "spl_object_hash", "object $object", "string" },` |
|         - | 1016 | `	{ "spl_object_id", "object $object", "int" },` |
|         - | 1017 | `	{ "sprintf", "string $format, mixed ...$values = ?", "string" },` |
|         - | 1018 | `	{ "sqrt", "float $num", "float" },` |
|         - | 1019 | `	{ "srand", "?int $seed = NULL, int $mode = 0", "void" },` |
|         - | 1020 | `	{ "stat", "string $filename", "array\|false" },` |
|         - | 1021 | `	{ "str_contains", "string $haystack, string $needle", "bool" },` |
|         - | 1022 | `	{ "str_ends_with", "string $haystack, string $needle", "bool" },` |
|         - | 1023 | `	{ "str_getcsv", "string $string, string $separator = ',', string $enclosure = '\"', string $escape = '\\\\'", "array" },` |
|         - | 1024 | `	{ "str_ireplace", "array\|string $search, array\|string $replace, array\|string $subject, &$count = NULL", "array\|string" },` |
|         - | 1025 | `	{ "str_pad", "string $string, int $length, string $pad_string = ' ', int $pad_type = 1", "string" },` |
|         - | 1026 | `	{ "str_repeat", "string $string, int $times", "string" },` |
|         - | 1027 | `	{ "str_replace", "array\|string $search, array\|string $replace, array\|string $subject, &$count = NULL", "array\|string" },` |
|         - | 1028 | `	{ "str_rot13", "string $string", "string" },` |
|         - | 1029 | `	{ "str_shuffle", "string $string", "string" },` |
|         - | 1030 | `	{ "str_split", "string $string, int $length = 1", "array" },` |
|         - | 1031 | `	{ "str_starts_with", "string $haystack, string $needle", "bool" },` |
|         - | 1032 | `	{ "str_word_count", "string $string, int $format = 0, ?string $characters = NULL", "array\|int" },` |
|         - | 1033 | `	{ "strcasecmp", "string $string1, string $string2", "int" },` |
|         - | 1034 | `	{ "strchr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|         - | 1035 | `	{ "strcmp", "string $string1, string $string2", "int" },` |
|         - | 1036 | `	{ "strnatcasecmp", "string $string1, string $string2", "int" },` |
|         - | 1037 | `	{ "strnatcmp", "string $string1, string $string2", "int" },` |
|         - | 1038 | `	{ "strcoll", "string $string1, string $string2", "int" },` |
|         - | 1039 | `	{ "strcspn", "string $string, string $characters, int $offset = 0, ?int $length = NULL", "int" },` |
|         - | 1040 | `	{ "stream_context_create", "?array $options = NULL, ?array $params = NULL", "" },` |
|         - | 1041 | `	{ "stream_context_get_options", "$stream_or_context", "array" },` |
|         - | 1042 | ``	/* php's argument #2 is `array\|string $wrapper_or_options` and the array form`` |
|         - | 1043 | `	 * — the two-argument spelling — is DEPRECATED in 8.3; §10 refuses what php` |
|         - | 1044 | `	 * deprecates, so this row declares the string and the whole-array form is` |
|         - | 1045 | `	 * spelled stream_context_set_options(). */` |
|         - | 1046 | `	{ "stream_context_set_option", "$context, string $wrapper_name, string $option_name, mixed $value", "bool" },` |
|         - | 1047 | `	{ "stream_context_set_options", "$context, array $options", "bool" },` |
|         - | 1048 | `	{ "stream_context_get_params", "$stream_or_context", "array" },` |
|         - | 1049 | `	{ "stream_context_set_params", "$context, array $params", "bool" },` |
|         - | 1050 | `	{ "stream_context_get_default", "?array $options = NULL", "" },` |
|         - | 1051 | `	{ "stream_context_set_default", "array $options", "" },` |
|         - | 1052 | `	{ "stream_get_contents", "$stream, ?int $length = NULL, int $offset = -1", "string\|false" },` |
|         - | 1053 | `	{ "stream_get_line", "$stream, int $length, string $ending = ''", "string\|false" },` |
|         - | 1054 | `	{ "socket_get_status", "$stream", "array" },` |
|         - | 1055 | `	{ "stream_get_meta_data", "$stream", "array" },` |
|         - | 1056 | `	{ "stream_copy_to_stream", "$from, $to, ?int $length = NULL, int $offset = 0", "int\|false" },` |
|         - | 1057 | `	{ "stream_get_transports", "", "array" },` |
|         - | 1058 | `	{ "stream_is_local", "$stream", "bool" },` |
|         - | 1059 | `	{ "stream_select", "?array &$read, ?array &$write, ?array &$except, ?int $seconds, ?int $microseconds = NULL", "int\|false" },` |
|         - | 1060 | `	{ "stream_set_blocking", "$stream, bool $enable", "bool" },` |
|         - | 1061 | `	{ "socket_set_blocking", "$stream, bool $enable", "bool" },` |
|         - | 1062 | `	{ "stream_set_chunk_size", "$stream, int $size", "int" },` |
|         - | 1063 | `	{ "stream_set_read_buffer", "$stream, int $size", "int" },` |
|         - | 1064 | `	{ "stream_set_timeout", "$stream, int $seconds, int $microseconds = 0", "bool" },` |
|         - | 1065 | `	{ "stream_set_write_buffer", "$stream, int $size", "int" },` |
|         - | 1066 | `	{ "set_file_buffer", "$stream, int $size", "int" },` |
|         - | 1067 | `	{ "stream_supports_lock", "$stream", "bool" },` |
|         - | 1068 | `	{ "stream_get_wrappers", "", "array" },` |
|         - | 1069 | `	{ "stream_get_filters", "", "array" },` |
|         - | 1070 | `	{ "stream_filter_append", "$stream, string $filter_name, int $mode = 0, mixed $params = NULL", "" },` |
|         - | 1071 | `	{ "stream_filter_prepend", "$stream, string $filter_name, int $mode = 0, mixed $params = NULL", "" },` |
|         - | 1072 | `	{ "stream_filter_remove", "$stream_filter", "bool" },` |
|         - | 1073 | `	{ "stream_filter_register", "string $filter_name, string $class", "bool" },` |
|         - | 1074 | `	{ "stream_bucket_make_writeable", "$brigade", "?StreamBucket" },` |
|         - | 1075 | `	{ "stream_bucket_append", "$brigade, StreamBucket $bucket", "void" },` |
|         - | 1076 | `	{ "stream_bucket_prepend", "$brigade, StreamBucket $bucket", "void" },` |
|         - | 1077 | `	{ "stream_bucket_new", "$stream, string $buffer", "StreamBucket" },` |
|         - | 1078 | `	{ "stream_register_wrapper", "string $protocol, string $class, int $flags = 0", "bool" },` |
|         - | 1079 | `	{ "stream_wrapper_register", "string $protocol, string $class, int $flags = 0", "bool" },` |
|         - | 1080 | `	{ "stream_wrapper_unregister", "string $protocol", "bool" },` |
|         - | 1081 | `	{ "stream_wrapper_restore", "string $protocol", "bool" },` |
|         - | 1082 | `	{ "strip_tags", "string $string, array\|string\|null $allowed_tags = NULL", "string" },` |
|         - | 1083 | `	{ "stripos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|         - | 1084 | `	{ "stripslashes", "string $string", "string" },` |
|         - | 1085 | `	{ "stristr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|         - | 1086 | `	{ "strlen", "string $string", "int" },` |
|         - | 1087 | `	{ "strncasecmp", "string $string1, string $string2, int $length", "int" },` |
|         - | 1088 | `	{ "strncmp", "string $string1, string $string2, int $length", "int" },` |
|         - | 1089 | `	{ "strpbrk", "string $string, string $characters", "string\|false" },` |
|         - | 1090 | `	{ "strpos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|         - | 1091 | `	{ "strrchr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|         - | 1092 | `	{ "strrev", "string $string", "string" },` |
|         - | 1093 | `	{ "strripos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|         - | 1094 | `	{ "strrpos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|         - | 1095 | `	{ "strspn", "string $string, string $characters, int $offset = 0, ?int $length = NULL", "int" },` |
|         - | 1096 | `	{ "strstr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|         - | 1097 | `	{ "strtok", "string $string, ?string $token = NULL", "string\|false" },` |
|         - | 1098 | `	{ "strtolower", "string $string", "string" },` |
|         - | 1099 | `	{ "strtotime", "string $datetime, ?int $baseTimestamp = NULL", "int\|false" },` |
|         - | 1100 | `	{ "strtoupper", "string $string", "string" },` |
|         - | 1101 | `	{ "strtr", "string $string, array\|string $from, ?string $to = NULL", "string" },` |
|         - | 1102 | `	{ "strval", "mixed $value", "string" },` |
|         - | 1103 | `	{ "substr", "string $string, int $offset, ?int $length = NULL", "string" },` |
|         - | 1104 | `	{ "substr_compare", "string $haystack, string $needle, int $offset, ?int $length = NULL, bool $case_insensitive = false", "int" },` |
|         - | 1105 | `	{ "substr_count", "string $haystack, string $needle, int $offset = 0, ?int $length = NULL", "int" },` |
|         - | 1106 | `	{ "substr_replace", "array\|string $string, array\|string $replace, array\|int $offset, array\|int\|null $length = NULL", "array\|string" },` |
|         - | 1107 | `	{ "symlink", "string $target, string $link", "bool" },` |
|         - | 1108 | `	{ "sys_get_temp_dir", "", "string" },` |
|         - | 1109 | `	{ "system", "string $command, &$result_code = NULL", "string\|false" },` |
|         - | 1110 | `	{ "tan", "float $num", "float" },` |
|         - | 1111 | `	{ "tanh", "float $num", "float" },` |
|         - | 1112 | `	{ "time", "", "int" },` |
|         - | 1113 | `	{ "token_get_all", "string $code, int $flags = 0", "array" },` |
|         - | 1114 | `	{ "token_name", "int $id", "string" },` |
|         - | 1115 | `	{ "touch", "string $filename, ?int $mtime = NULL, ?int $atime = NULL", "bool" },` |
|         - | 1116 | `	{ "trigger_error", "string $message, int $error_level = 1024", "true" },` |
|         - | 1117 | `	{ "trim", "string $string, string $characters = ?", "string" },` |
|         - | 1118 | `	{ "uasort", "array &$array, callable $callback", "true" },` |
|         - | 1119 | `	{ "ucfirst", "string $string", "string" },` |
|         - | 1120 | `	{ "ucwords", "string $string, string $separators = ?", "string" },` |
|         - | 1121 | `	{ "uksort", "array &$array, callable $callback", "true" },` |
|         - | 1122 | `	{ "umask", "?int $mask = NULL", "int" },` |
|         - | 1123 | `	{ "uniqid", "string $prefix = '', bool $more_entropy = false", "string" },` |
|         - | 1124 | `	{ "unlink", "string $filename, $context = NULL", "bool" },` |
|         - | 1125 | `	{ "unserialize", "string $data, array $options = ?", "mixed" },` |
|         - | 1126 | `	{ "urldecode", "string $string", "string" },` |
|         - | 1127 | `	{ "urlencode", "string $string", "string" },` |
|         - | 1128 | `	{ "user_error", "string $message, int $error_level = 1024", "true" },` |
|         - | 1129 | `	{ "usleep", "int $microseconds", "void" },` |
|         - | 1130 | `	{ "usort", "array &$array, callable $callback", "true" },` |
|         - | 1131 | `	{ "var_dump", "mixed $value, mixed ...$values = ?", "void" },` |
|         - | 1132 | `	{ "var_export", "mixed $value, bool $return = false", "?string" },` |
|         - | 1133 | `	{ "version_compare", "string $version1, string $version2, ?string $operator = null", "int\|bool" },` |
|         - | 1134 | `	{ "vfprintf", "$stream, string $format, array $values", "int" },` |
|         - | 1135 | `	{ "vprintf", "string $format, array $values", "int" },` |
|         - | 1136 | `	{ "vsprintf", "string $format, array $values", "string" },` |
|         - | 1137 | `	{ "wordwrap", "string $string, int $width = 75, string $break = ?, bool $cut_long_words = false", "string" },` |
|         - | 1138 | `	{ "zip_close", "$zip", "void" },` |
|         - | 1139 | `	{ "zip_entry_close", "$zip_entry", "bool" },` |
|         - | 1140 | `	{ "zip_entry_compressedsize", "$zip_entry", "int\|false" },` |
|         - | 1141 | `	{ "zip_entry_compressionmethod", "$zip_entry", "string\|false" },` |
|         - | 1142 | `	{ "zip_entry_filesize", "$zip_entry", "int\|false" },` |
|         - | 1143 | `	{ "zip_entry_name", "$zip_entry", "string\|false" },` |
|         - | 1144 | `	{ "zip_entry_open", "$zip_dp, $zip_entry, string $mode = 'rb'", "bool" },` |
|         - | 1145 | `	{ "zip_entry_read", "$zip_entry, int $len = 1024", "string\|false" },` |
|         - | 1146 | `	{ "zip_open", "string $filename", "" },` |
|         - | 1147 | `	{ "zip_read", "$zip", "" },` |
|         - | 1148 | `};` |
|         - | 1149 | `/*` |
|         - | 1150 | ` * Stamp the signature strings onto the registered host functions.` |
|         - | 1151 | ` * Runs once at PH7_VmMakeReady, after the builtins are registered.` |
|         - | 1152 | ` */` |
|         - | 1153 | `/*` |
|         - | 1154 | ` * Derive the minimum arity from a builtin's declared signature: a parameter is` |
|         - | 1155 | ` * optional when it carries a default ("int $length = 76") or is variadic` |
|         - | 1156 | ` * ("...$rest"), so the minimum is the count of the parameters that are neither,` |
|         - | 1157 | ` * and the ZPP wording is "at least" as soon as one optional/variadic exists.` |
|         - | 1158 | ` *` |
|         - | 1159 | ` * This makes aBuiltinSig[] the single source of truth for arity — the curated` |
|         - | 1160 | ` * aBuiltinArity[] table above stays as an OVERRIDE for the handful of builtins` |
|         - | 1161 | ` * php reports differently from their own signature (e.g. strtr(), whose 3-arg` |
|         - | 1162 | ` * form still reports "expects exactly 2", and the array_udiff() family, whose` |
|         - | 1163 | ` * variadic tail hides a second required argument). Verified against php 8.5.7` |
|         - | 1164 | ` * for all 462 signed builtins: 458 derive exactly, 4 are overridden.` |
|         - | 1165 | ` */` |
|         - | 1166 | `/*` |
|         - | 1167 | ` * A DEFAULT can contain the parameter separator: php declares` |
|         - | 1168 | `` * `string $separator = ','` and `string $enclosure = '"'`. Every scan of a`` |
|         - | 1169 | ` * signature therefore has to step over a quoted run, or the comma inside one` |
|         - | 1170 | ` * splits the parameter in two — which is how fgetcsv()/fputcsv()/str_getcsv()` |
|         - | 1171 | ` * came to count SIX parameters and accept a fifth argument php refuses.` |
|         - | 1172 | ` * Answers the position of the closing quote (or of the NUL when the run is` |
|         - | 1173 | ` * unterminated); the caller advances past it.` |
|         - | 1174 | ` */` |
|   2066545 | 1175 | `static const char *VmSigSkipQuoted(const char *zCur)` |
|         5 | 1176 | `{` |
|   2066550 | 1177 | `	char c = zCur[0];` |
|   2066550 | 1178 | `	if( c != '\'' && c != '"' ){` |
|       ! 0 | 1179 | `		return zCur;` |
|         - | 1180 | `	}` |
|   2850900 | 1181 | `	for( zCur++ ; zCur[0] ; zCur++ ){` |
|   2850900 | 1182 | `		if( zCur[0] == '\\' && zCur[1] ){` |
|     28233 | 1183 | `			zCur++;` |
|     28233 | 1184 | `			continue;` |
|         - | 1185 | `		}` |
|   2822672 | 1186 | `		if( zCur[0] == c ){` |
|   2066550 | 1187 | `			break;` |
|         - | 1188 | `		}` |
|    378066 | 1189 | `	}` |
|   2066550 | 1190 | `	return zCur;` |
|   1033277 | 1191 | `}` |
|   8541416 | 1192 | `PH7_PRIVATE void VmDeriveArityFromSig(const char *zSig,sxi16 *pnMin,sxu8 *pbAtLeast,sxi16 *pnMax,sxu8 *pbHasMax)` |
|         5 | 1193 | `{` |
|   8541421 | 1194 | `	const char *zCur = zSig;` |
|   8541421 | 1195 | `	int nMin = 0, bAtLeast = 0, bSeen = 0, bOptional = 0;` |
|   8541421 | 1196 | `	int nTotal = 0, bVariadic = 0;` |
|  88990225 | 1197 | `	for(;;){` |
| 182903949 | 1198 | `		if( zCur[0] == '\'' \|\| zCur[0] == '"' ){` |
|    303775 | 1199 | `			bSeen = 1;` |
|    303775 | 1200 | `			zCur = VmSigSkipQuoted(zCur);` |
|    303775 | 1201 | `			if( zCur[0] != '\0' ){` |
|    303775 | 1202 | `				zCur++;` |
|    151885 | 1203 | `			}` |
|    303775 | 1204 | `			continue;` |
|         - | 1205 | `		}` |
| 182600179 | 1206 | `		if( zCur[0] == '\0' \|\| zCur[0] == ',' ){` |
|  13161145 | 1207 | `			if( bSeen ){` |
|  10132603 | 1208 | `				nTotal++;` |
|  10132603 | 1209 | `				if( bOptional ){` |
|   3463087 | 1210 | `					bAtLeast = 1;` |
|   1731546 | 1211 | `				}else{` |
|   6669521 | 1212 | `					nMin++;` |
|         - | 1213 | `				}` |
|   5066299 | 1214 | `			}` |
|  13161145 | 1215 | `			if( zCur[0] == '\0' ){` |
|   8541421 | 1216 | `				break;` |
|         - | 1217 | `			}` |
|   4619729 | 1218 | `			bSeen = bOptional = 0;` |
|   4619729 | 1219 | `			zCur++;` |
|   4619729 | 1220 | `			continue;` |
|         - | 1221 | `		}` |
| 169439039 | 1222 | `		if( zCur[0] != ' ' ){` |
| 148787267 | 1223 | `			bSeen = 1;` |
|  74393631 | 1224 | `		}` |
| 169439039 | 1225 | `		if( zCur[0] == '=' \|\| (zCur[0] == '.' && zCur[1] == '.' && zCur[2] == '.') ){` |
|   3626187 | 1226 | `			bOptional = 1;` |
|   1813091 | 1227 | `		}` |
| 169439039 | 1228 | `		if( zCur[0] == '.' && zCur[1] == '.' && zCur[2] == '.' ){` |
|    283947 | 1229 | `			bVariadic = 1;` |
|    141971 | 1230 | `		}` |
| 169439039 | 1231 | `		zCur++;` |
|         5 | 1232 | `	}` |
|   8541421 | 1233 | `	*pnMin = (sxi16)nMin;` |
|   8541421 | 1234 | `	*pbAtLeast = (sxu8)bAtLeast;` |
|         - | 1235 | `	/* php enforces a MAXIMUM too ("expects at most 1 argument, 2 given"); a` |
|         - | 1236 | `	 * variadic tail means there is none. The parameter COUNT is the maximum,` |
|         - | 1237 | `	 * whether or not the parameters carry defaults. */` |
|   8541421 | 1238 | `	*pnMax = (sxi16)nTotal;` |
|   8541421 | 1239 | `	*pbHasMax = (sxu8)(bVariadic ? 0 : 1);` |
|   8541421 | 1240 | `}` |
|         - | 1241 | `/*` |
|         - | 1242 | ` * Does the declared type list (e.g. "array\|string", "?int", "callable") contain` |
|         - | 1243 | ` * the given token? Compares against each '\|'-separated alternative, ignoring a` |
|         - | 1244 | ` * leading nullable '?'.` |
|         - | 1245 | ` */` |
|  12463182 | 1246 | `static int VmSigTypeHas(const char *zType,int nType,const char *zTok)` |
|         5 | 1247 | `{` |
|  12463187 | 1248 | `	int nTok = (int)SyStrlen(zTok);` |
|  12463187 | 1249 | `	int i = 0;` |
|  12463187 | 1250 | `	if( zType[0] == '?' ){` |
|    762673 | 1251 | `		zType++;` |
|    762673 | 1252 | `		nType--;` |
|    381334 | 1253 | `	}` |
|  25449902 | 1254 | `	while( i < nType ){` |
|  13187859 | 1255 | `		int j = i;` |
|  86247971 | 1256 | `		while( j < nType && zType[j] != '\|' ){` |
|  73060117 | 1257 | `			j++;` |
|         5 | 1258 | `		}` |
|  13187859 | 1259 | `		if( j - i == nTok && SyMemcmp(&zType[i],zTok,(sxu32)nTok) == 0 ){` |
|    201144 | 1260 | `			return 1;` |
|         - | 1261 | `		}` |
|  12986720 | 1262 | `		i = j + 1;` |
|         5 | 1263 | `	}` |
|  12262048 | 1264 | `	return 0;` |
|   6234848 | 1265 | `}` |
|         - | 1266 | `/*` |
|         - | 1267 | `` * Is EVERY arm of the declared type list `array` (a bare `array`, or `?array`,`` |
|         - | 1268 | `` * or the `array\|null` union that spells the same thing)? Such a parameter has`` |
|         - | 1269 | ` * no arm a scalar can satisfy, and php refuses one outright.` |
|         - | 1270 | ` *` |
|         - | 1271 | `` * The screen used to exempt any type list carrying an `array` arm, union or`` |
|         - | 1272 | `` * not, for a wording reason: php's `array\|object` parameters come from ONE ZPP`` |
|         - | 1273 | ` * macro (Z_PARAM_ARRAY_OR_OBJECT) that names only "array" in the refusal, so` |
|         - | 1274 | ` * the declared type is not the text php prints. That ambiguity does not exist` |
|         - | 1275 | `` * for a parameter typed exactly `array` -- there is one arm and php prints it.`` |
|         - | 1276 | ` */` |
|   3481388 | 1277 | `static int VmSigTypeIsArrayOnly(const char *zType,int nType)` |
|         5 | 1278 | `{` |
|   3481393 | 1279 | `	int i = 0, bArray = 0;` |
|   3481393 | 1280 | `	if( zType[0] == '?' ){` |
|    358697 | 1281 | `		zType++;` |
|    358697 | 1282 | `		nType--;` |
|    179346 | 1283 | `	}` |
|   3657983 | 1284 | `	while( i < nType ){` |
|   3657755 | 1285 | `		int j = i;` |
|  22788488 | 1286 | `		while( j < nType && zType[j] != '\|' ){` |
|  19130738 | 1287 | `			j++;` |
|         5 | 1288 | `		}` |
|   3657755 | 1289 | `		if( j > i ){` |
|   3657750 | 1290 | `			if( j - i == (int)sizeof("array")-1` |
|   1918263 | 1291 | `			 && SyMemcmp(&zType[i],"array",sizeof("array")-1) == 0 ){` |
|    176595 | 1292 | `				bArray = 1;` |
|   3575828 | 1293 | `			}else if( !(j - i == (int)sizeof("null")-1` |
|   1747829 | 1294 | `			         && SyMemcmp(&zType[i],"null",sizeof("null")-1) == 0) ){` |
|   3481165 | 1295 | `				return 0;` |
|         - | 1296 | `			}` |
|     88295 | 1297 | `		}` |
|    176595 | 1298 | `		i = j + 1;` |
|         5 | 1299 | `	}` |
|       233 | 1300 | `	return bArray;` |
|   1741580 | 1301 | `}` |
|         - | 1302 | `/*` |
|         - | 1303 | ` * Does the declared type list name a CLASS (anything that is not one of php's` |
|         - | 1304 | ` * builtin type keywords)? A class-typed parameter accepts an object, so it must` |
|         - | 1305 | ` * not be rejected by the array/object/resource screen below.` |
|         - | 1306 | ` */` |
|         - | 1307 | `/* Is this one arm of a declared type a BUILTIN type name rather than a class? */` |
|   3686942 | 1308 | `static int VmSigArmIsBuiltinType(const char *zArm,int nArm)` |
|         5 | 1309 | `{` |
|         - | 1310 | `	static const char *azBuiltin[] = {` |
|         - | 1311 | `		"int","float","string","bool","array","object","callable","iterable",` |
|         - | 1312 | `		"mixed","null","void","resource","false","true","never","self","static"` |
|         - | 1313 | `	};` |
|         - | 1314 | `	int k;` |
|   9946160 | 1315 | `	for( k = 0 ; k < (int)SX_ARRAYSIZE(azBuiltin) ; ++k ){` |
|   9931764 | 1316 | `		int nB = (int)SyStrlen(azBuiltin[k]);` |
|   9931764 | 1317 | `		if( nArm == nB && SyMemcmp(zArm,azBuiltin[k],(sxu32)nB) == 0 ){` |
|   3672551 | 1318 | `			return 1;` |
|         - | 1319 | `		}` |
|   3130938 | 1320 | `	}` |
|     14401 | 1321 | `	return 0;` |
|   1844357 | 1322 | `}` |
|   3501094 | 1323 | `static int VmSigTypeHasClass(const char *zType,int nType)` |
|         5 | 1324 | `{` |
|   3501099 | 1325 | `	int i = 0;` |
|   3501099 | 1326 | `	if( zType[0] == '?' ){` |
|    362251 | 1327 | `		zType++;` |
|    362251 | 1328 | `		nType--;` |
|    181123 | 1329 | `	}` |
|   7173643 | 1330 | `	while( i < nType ){` |
|   3682355 | 1331 | `		int j = i;` |
|  22987100 | 1332 | `		while( j < nType && zType[j] != '\|' ){` |
|  19304750 | 1333 | `			j++;` |
|         5 | 1334 | `		}` |
|   3682355 | 1335 | `		if( j > i && !VmSigArmIsBuiltinType(&zType[i],j - i) ){` |
|      9811 | 1336 | `			return 1;` |
|         - | 1337 | `		}` |
|   3672549 | 1338 | `		i = j + 1;` |
|         5 | 1339 | `	}` |
|   3491293 | 1340 | `	return 0;` |
|   1751433 | 1341 | `}` |
|         - | 1342 | `/*` |
|         - | 1343 | ` * php's ", X given" tail: like ph7_type_name() but an object reports its CLASS,` |
|         - | 1344 | ` * which is what php prints in a TypeError.` |
|         - | 1345 | ` */` |
|         - | 1346 | `/*` |
|         - | 1347 | ` * Does pObj satisfy any CLASS arm of a declared type?` |
|         - | 1348 | ` *` |
|         - | 1349 | ` * Answers TRUE (unscreened) when an arm names something this VM has not declared:` |
|         - | 1350 | ` * the signatures describe php's surface, parts of which PHL models differently` |
|         - | 1351 | ` * (the resource-backed handles the RES branch below already excuses), and a name` |
|         - | 1352 | ` * that resolves to nothing must not turn into a rejection of a valid argument.` |
|         - | 1353 | ` */` |
|      4588 | 1354 | `static int VmSigObjSatisfiesClass(ph7_vm *pVm,const char *zType,int nType,` |
|         - | 1355 | `	ph7_class_instance *pObj)` |
|         5 | 1356 | `{` |
|      4593 | 1357 | `	int i = 0;` |
|      4593 | 1358 | `	if( pObj == 0 \|\| pObj->pClass == 0 ){` |
|       ! 0 | 1359 | `		return 1;` |
|         - | 1360 | `	}` |
|      4593 | 1361 | `	if( zType[0] == '?' ){` |
|      1010 | 1362 | `		zType++;` |
|      1010 | 1363 | `		nType--;` |
|       503 | 1364 | `	}` |
|      4623 | 1365 | `	while( i < nType ){` |
|      4597 | 1366 | `		int j = i;` |
|     47377 | 1367 | `		while( j < nType && zType[j] != '\|' ){` |
|     42785 | 1368 | `			j++;` |
|         5 | 1369 | `		}` |
|      4597 | 1370 | `		if( j > i && !VmSigArmIsBuiltinType(&zType[i],j - i) ){` |
|      4595 | 1371 | `			ph7_class *pClass = PH7_VmExtractClass(&(*pVm),&zType[i],(sxu32)(j - i),FALSE,0);` |
|      4595 | 1372 | `			if( pClass == 0 ){` |
|         - | 1373 | `				/* Either a builtin type name (already excluded by the caller) or a` |
|         - | 1374 | `				 * class this build does not declare: nothing to judge. */` |
|       ! 0 | 1375 | `				return 1;` |
|         - | 1376 | `			}` |
|      4595 | 1377 | `			if( PH7_VmInstanceOf(pObj->pClass,pClass) ){` |
|      4567 | 1378 | `				return 1;` |
|         - | 1379 | `			}` |
|        14 | 1380 | `		}` |
|        33 | 1381 | `		i = j + 1;` |
|         3 | 1382 | `	}` |
|        29 | 1383 | `	return 0;` |
|      2299 | 1384 | `}` |
|       106 | 1385 | `static const char * VmArgTypeName(ph7_value *pVal)` |
|         4 | 1386 | `{` |
|       110 | 1387 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       110 | 1388 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       110 | 1389 | `		if( pInst && pInst->pClass ){` |
|       110 | 1390 | `			return pInst->pClass->sName.zString;` |
|         - | 1391 | `		}` |
|       ! 0 | 1392 | `	}` |
|       ! 0 | 1393 | `	return ph7_type_name(pVal);` |
|        57 | 1394 | `}` |
|         - | 1395 | `/*` |
|         - | 1396 | `` * Does pArg satisfy a `string` parameter? The rule VmEnforceBuiltinArgTypes()`` |
|         - | 1397 | ` * below applies, factored out so a builtin that words its own overload dispatch` |
|         - | 1398 | ` * (strtr(), whose expected type depends on the ARITY and so cannot be spelled in` |
|         - | 1399 | ` * one signature) decides identically instead of forking the logic. An array never` |
|         - | 1400 | ` * satisfies one; an object does only through __toString(); a resource does not;` |
|         - | 1401 | ` * null does under php, with a deprecation, but not under PHL's §10 null-strictness` |
|         - | 1402 | ` * policy — the screen and this helper both report it as a mismatch.` |
|         - | 1403 | ` */` |
|     43758 | 1404 | `PH7_PRIVATE int PH7_ArgSatisfiesString(ph7_value *pArg)` |
|         5 | 1405 | `{` |
|     43763 | 1406 | `	if( (pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_NULL\|MEMOBJ_RES)) != 0 ){` |
|        22 | 1407 | `		return 0;` |
|         - | 1408 | `	}` |
|     43743 | 1409 | `	if( (pArg->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       140 | 1410 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|       140 | 1411 | `		return pInst && PH7_ClassExtractMethod(pInst->pClass,"__toString",` |
|        68 | 1412 | `			sizeof("__toString")-1) != 0;` |
|         - | 1413 | `	}` |
|     43607 | 1414 | `	return 1;` |
|     21884 | 1415 | `}` |
|         - | 1416 | `/*` |
|         - | 1417 | `` * Is the declared type exactly `int` — the only shape whose float argument the`` |
|         - | 1418 | `` * screen below can decide? A union with a `float`, `string` or `bool` arm has its`` |
|         - | 1419 | ` * own coercion rules per arm (and php words those refusals from the builtin), so` |
|         - | 1420 | ` * only the plain form and its nullable spelling qualify.` |
|         - | 1421 | ` */` |
|       664 | 1422 | `static int VmSigTypeIsIntOnly(const char *zType,int nType)` |
|         5 | 1423 | `{` |
|       669 | 1424 | `	if( nType > 0 && zType[0] == '?' ){` |
|        56 | 1425 | `		zType++;` |
|        56 | 1426 | `		nType--;` |
|        26 | 1427 | `	}` |
|       669 | 1428 | `	if( nType == (int)sizeof("int")-1 && SyMemcmp(zType,"int",3) == 0 ){` |
|       158 | 1429 | `		return 1;` |
|         - | 1430 | `	}` |
|         - | 1431 | ``	/* `int\|null` / `null\|int`, the union spelling of `?int`. */`` |
|       683 | 1432 | `	return VmSigTypeHas(zType,nType,"int") && VmSigTypeHas(zType,nType,"null")` |
|       168 | 1433 | `	    && !VmSigTypeHas(zType,nType,"float")` |
|         2 | 1434 | `	    && !VmSigTypeHas(zType,nType,"string")` |
|         1 | 1435 | `	    && !VmSigTypeHas(zType,nType,"bool")` |
|       ! 0 | 1436 | `	    && !VmSigTypeHas(zType,nType,"array")` |
|       ! 0 | 1437 | `	    && !VmSigTypeHas(zType,nType,"object")` |
|       ! 0 | 1438 | `	    && !VmSigTypeHas(zType,nType,"iterable")` |
|       ! 0 | 1439 | `	    && !VmSigTypeHas(zType,nType,"callable")` |
|       677 | 1440 | `	    && !VmSigTypeHasClass(zType,nType);` |
|       337 | 1441 | `}` |
|         - | 1442 | `/*` |
|         - | 1443 | `` * Can this float reach an `int` parameter without losing anything? php's rule is`` |
|         - | 1444 | ` * php_parse_arg_long's: in range, and integral. NaN and the infinities are out by` |
|         - | 1445 | ` * the range test (a NaN compares false against both bounds, which is why the test` |
|         - | 1446 | ` * is written as a pair of accepts rather than a pair of rejects).` |
|         - | 1447 | ` */` |
|       102 | 1448 | `static int VmDoubleFitsInt(double d)` |
|         4 | 1449 | `{` |
|       106 | 1450 | `	if( !PH7_RealFitsInt64(d) ){` |
|        50 | 1451 | `		return 0;` |
|         - | 1452 | `	}` |
|        58 | 1453 | `	return d == (double)(sxi64)d;` |
|        55 | 1454 | `}` |
|         - | 1455 | `/*` |
|         - | 1456 | ` * The same question for a NUMERIC string, which php asks with the same answer:` |
|         - | 1457 | `` * `dechex("1e19")` and `dechex("99999999999999999999")` are both`` |
|         - | 1458 | `` * `must be of type int, string given`. RangeStrToNumber is php's`` |
|         - | 1459 | ` * is_numeric_string grammar and already reclassifies an integer too wide for an` |
|         - | 1460 | ` * sxi64 as a DOUBLE, so the two shapes converge on one test.` |
|         - | 1461 | ` */` |
|        76 | 1462 | `static int VmNumStrFitsInt(ph7_value *pArg)` |
|         3 | 1463 | `{` |
|         - | 1464 | `	const char *zStr;` |
|        79 | 1465 | `	int nLen = 0;` |
|        79 | 1466 | `	sxi64 iVal = 0;` |
|        79 | 1467 | `	double dVal = 0;` |
|        79 | 1468 | `	zStr = ph7_value_to_string(pArg,&nLen);` |
|        79 | 1469 | `	switch( RangeStrToNumber(zStr,(sxu32)nLen,&iVal,&dVal) ){` |
|        54 | 1470 | `	case RANGE_IN_LONG:   return 1;` |
|        27 | 1471 | `	case RANGE_IN_DOUBLE: return VmDoubleFitsInt(dVal);` |
|       ! 0 | 1472 | `	default:              return 0;` |
|         - | 1473 | `	}` |
|        41 | 1474 | `}` |
|         - | 1475 | `/*` |
|         - | 1476 | ` * PHP-8 PATH parameters: which positions carry a filesystem path, a shell` |
|         - | 1477 | ` * command or an include-path list rather than an ordinary string.` |
|         - | 1478 | ` *` |
|         - | 1479 | ` * php spells this in the ZPP macro, not in the declared type: a path parameter` |
|         - | 1480 | `` * is `Z_PARAM_PATH` where an ordinary one is `Z_PARAM_STR`, and both print as`` |
|         - | 1481 | `` * `string` in the stub Reflection reads. The difference is a single rule — a`` |
|         - | 1482 | ` * path may not contain a NUL byte — and php raises a catchable ValueError for` |
|         - | 1483 | ` * one that does, BEFORE the call reaches the filesystem.` |
|         - | 1484 | ` *` |
|         - | 1485 | ` * PHL had no such notion, so every one of these arguments went to the C API as` |
|         - | 1486 | ` * a NUL-terminated string and was silently TRUNCATED at the NUL. That is not a` |
|         - | 1487 | ` * missing diagnostic: the truncated path is a DIFFERENT path, and the builtin` |
|         - | 1488 | `` * then operated on it. `unlink("$dir/x\0.png")` deleted `$dir/x`,`` |
|         - | 1489 | `` * `file_put_contents("$dir/x\0.txt",$d)` wrote it, `touch`/`chmod`/`copy`/`` |
|         - | 1490 | ``  * `rename`/`symlink`/`mkdir` all acted on the prefix, `glob` and `realpath` `` |
|         - | 1491 | `` * answered for it, and `shell_exec("cmd\0; rm -rf /")` ran the prefix as a`` |
|         - | 1492 | ` * command. It is the classic poison-NUL-byte shape php closed engine-wide: a` |
|         - | 1493 | ` * script that concatenates request input into a filename gets a truncation` |
|         - | 1494 | ` * where php gets a refusal, and the extension check the suffix was there to` |
|         - | 1495 | ` * perform never runs.` |
|         - | 1496 | ` *` |
|         - | 1497 | ` * The mask is positional (bit N => parameter N is a path), which is how php` |
|         - | 1498 | ` * carries it too. Only functions PHL actually registers are listed; each row's` |
|         - | 1499 | ` * positions were verified against php 8.5 argument by argument (the answer is` |
|         - | 1500 | `` * NOT derivable from the parameter name — preg_match's `$pattern` is an`` |
|         - | 1501 | ``  * ordinary string, glob's is a path — nor from the type, which is `string` `` |
|         - | 1502 | ` * for both).` |
|         - | 1503 | ` *` |
|         - | 1504 | ` * What is deliberately NOT here: the stat family (file_exists, is_dir, stat,` |
|         - | 1505 | ` * filesize, fileperms, …), which php parses with Z_PARAM_STR and answers` |
|         - | 1506 | `` * `false` for in silence, and the pure PATH-STRING functions (basename,`` |
|         - | 1507 | ` * dirname, pathinfo), which php lets the NUL through untouched because they` |
|         - | 1508 | ` * never touch the filesystem. Both are php-exact here already.` |
|         - | 1509 | ` */` |
|   2764214 | 1510 | `static sxu32 VmBuiltinPathMask(SyString *pName)` |
|         5 | 1511 | `{` |
|         - | 1512 | `	static const struct {` |
|         - | 1513 | `		const char *zName;` |
|         - | 1514 | `		sxu32 nByte;` |
|         - | 1515 | `		sxu32 mask;` |
|         - | 1516 | `	} aPath[] = {` |
|         - | 1517 | `		/* Open / read / write */` |
|         - | 1518 | `		{ "fopen",             5, 1u<<0 },` |
|         - | 1519 | `		{ "file_get_contents", 17, 1u<<0 },` |
|         - | 1520 | `		{ "file_put_contents", 17, 1u<<0 },` |
|         - | 1521 | `		{ "file",              4, 1u<<0 },` |
|         - | 1522 | `		{ "readfile",          8, 1u<<0 },` |
|         - | 1523 | `		{ "parse_ini_file",   14, 1u<<0 },` |
|         - | 1524 | `		{ "md5_file",          8, 1u<<0 },` |
|         - | 1525 | `		{ "sha1_file",         9, 1u<<0 },` |
|         - | 1526 | `		{ "hash_file",         9, 1u<<1 },` |
|         - | 1527 | `		{ "hash_hmac_file",   14, 1u<<1 },` |
|         - | 1528 | `		{ "hash_update_file", 16, 1u<<1 },` |
|         - | 1529 | `		/* Metadata / mutation */` |
|         - | 1530 | `		{ "unlink",            6, 1u<<0 },` |
|         - | 1531 | `		{ "touch",             5, 1u<<0 },` |
|         - | 1532 | `		{ "chmod",             5, 1u<<0 },` |
|         - | 1533 | `		{ "chgrp",             5, 1u<<0 },` |
|         - | 1534 | `		{ "chown",             5, 1u<<0 },` |
|         - | 1535 | `		{ "rename",            6, (1u<<0)\|(1u<<1) },` |
|         - | 1536 | `		{ "copy",              4, (1u<<0)\|(1u<<1) },` |
|         - | 1537 | `		{ "link",              4, (1u<<0)\|(1u<<1) },` |
|         - | 1538 | `		{ "symlink",           7, (1u<<0)\|(1u<<1) },` |
|         - | 1539 | `		{ "readlink",          8, 1u<<0 },` |
|         - | 1540 | `		{ "realpath",          8, 1u<<0 },` |
|         - | 1541 | `		{ "stream_resolve_include_path", 27, 1u<<0 },` |
|         - | 1542 | `		/* Directories */` |
|         - | 1543 | `		{ "mkdir",             5, 1u<<0 },` |
|         - | 1544 | `		{ "rmdir",             5, 1u<<0 },` |
|         - | 1545 | `		{ "opendir",           7, 1u<<0 },` |
|         - | 1546 | `		{ "dir",               3, 1u<<0 },` |
|         - | 1547 | `		{ "scandir",           7, 1u<<0 },` |
|         - | 1548 | `		{ "chdir",             5, 1u<<0 },` |
|         - | 1549 | `		{ "chroot",            6, 1u<<0 },` |
|         - | 1550 | `		{ "glob",              4, 1u<<0 },` |
|         - | 1551 | `		{ "tempnam",           7, (1u<<0)\|(1u<<1) },` |
|         - | 1552 | `		{ "disk_free_space",  15, 1u<<0 },` |
|         - | 1553 | `		{ "disk_total_space", 16, 1u<<0 },` |
|         - | 1554 | `		{ "diskfreespace",    13, 1u<<0 },` |
|         - | 1555 | `		/* Path-shaped settings and the pattern matcher */` |
|         - | 1556 | `		{ "fnmatch",           7, (1u<<0)\|(1u<<1) },` |
|         - | 1557 | `		{ "set_include_path", 16, 1u<<0 },` |
|         - | 1558 | `		{ "session_save_path", 17, 1u<<0 },` |
|         - | 1559 | `		{ "error_log",         9, 1u<<2 },` |
|         - | 1560 | `		/* Commands handed to the shell — and the two escapers, which php screens` |
|         - | 1561 | `		 * the same way even though neither of them runs anything: a NUL in what a` |
|         - | 1562 | `		 * script is about to hand a shell is refused where it is WRITTEN. */` |
|         - | 1563 | `		{ "shell_exec",       10, 1u<<0 },` |
|         - | 1564 | `		{ "popen",             5, 1u<<0 },` |
|         - | 1565 | `		{ "escapeshellarg",   14, 1u<<0 },` |
|         - | 1566 | `		{ "escapeshellcmd",   14, 1u<<0 },` |
|         - | 1567 | `		{ "exec",              4, 1u<<0 },` |
|         - | 1568 | `		{ "system",            6, 1u<<0 },` |
|         - | 1569 | `		{ "passthru",          8, 1u<<0 },` |
|         - | 1570 | `		/* The SPL path constructors, which php screens identically and reports` |
|         - | 1571 | ``		 * under their QUALIFIED name (`SplFileInfo::__construct(): Argument #1`` |
|         - | 1572 | ``		 * ($filename) …`). They are native methods, so their signature reaches this`` |
|         - | 1573 | `		 * screen the same way a builtin's does. */` |
|         - | 1574 | `		{ "SplFileInfo::__construct",                24, 1u<<0 },` |
|         - | 1575 | `		{ "DirectoryIterator::__construct",          30, 1u<<0 },` |
|         - | 1576 | `		{ "FilesystemIterator::__construct",         31, 1u<<0 },` |
|         - | 1577 | `		{ "RecursiveDirectoryIterator::__construct", 39, 1u<<0 },` |
|         - | 1578 | `	};` |
|         - | 1579 | `	sxu32 i;` |
|   2764219 | 1580 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|       ! 0 | 1581 | `		return 0;` |
|         - | 1582 | `	}` |
| 137617093 | 1583 | `	for( i = 0 ; i < SX_ARRAYSIZE(aPath) ; ++i ){` |
| 134944512 | 1584 | `		if( pName->nByte == aPath[i].nByte` |
|  69744978 | 1585 | `		 && SyStrnicmp(pName->zString,aPath[i].zName,pName->nByte) == 0 ){` |
|     91643 | 1586 | `			return aPath[i].mask;` |
|         - | 1587 | `		}` |
|  67467417 | 1588 | `	}` |
|   2672581 | 1589 | `	return 0;` |
|   1382931 | 1590 | `}` |
|         - | 1591 | `/*` |
|         - | 1592 | ` * Does this argument carry a NUL byte? Only a STRING can: every other scalar` |
|         - | 1593 | ` * renders through the number/bool formatters, which emit none. An OBJECT is` |
|         - | 1594 | ` * coerced by the caller before asking (php's ZPP order), so by the time this` |
|         - | 1595 | ` * runs a Stringable is already the string it produced.` |
|         - | 1596 | ` */` |
|     91656 | 1597 | `static int VmArgHasNulByte(ph7_value *pArg)` |
|         5 | 1598 | `{` |
|         - | 1599 | `	const char *zStr;` |
|         - | 1600 | `	sxu32 n, nLen;` |
|     91661 | 1601 | `	if( (pArg->iFlags & MEMOBJ_STRING) == 0 ){` |
|         3 | 1602 | `		return 0;` |
|         - | 1603 | `	}` |
|     91659 | 1604 | `	zStr = (const char *)SyBlobData(&pArg->sBlob);` |
|     91659 | 1605 | `	nLen = SyBlobLength(&pArg->sBlob);` |
|   5957597 | 1606 | `	for( n = 0 ; n < nLen ; ++n ){` |
|   5866041 | 1607 | `		if( zStr[n] == 0 ){` |
|       101 | 1608 | `			return 1;` |
|         - | 1609 | `		}` |
|   2975441 | 1610 | `	}` |
|     91561 | 1611 | `	return 0;` |
|     45832 | 1612 | `}` |
|         - | 1613 | `/*` |
|         - | 1614 | ` * Does php's strict_types rule refuse this argument for the declared type?` |
|         - | 1615 | ` *` |
|         - | 1616 | `` * A `declare(strict_types=1)` file gets NO scalar coercion at an internal call`` |
|         - | 1617 | ` * either — php applies the same rule to a builtin, a native method and a userland` |
|         - | 1618 | `` * function, and the single exception is the int -> float widening. So `trim(5)`,`` |
|         - | 1619 | `` * `sqrt("4")`, `str_repeat("a", 2.0)` and `in_array($n, $a, 1)` are all TypeErrors`` |
|         - | 1620 | ` * there, where the weak-mode screen below (which is the only one PHL had) coerces` |
|         - | 1621 | ` * and computes.` |
|         - | 1622 | ` *` |
|         - | 1623 | ` * Only the arms a scalar could otherwise satisfy are decided here; an array, a` |
|         - | 1624 | ` * resource, a null and a class-typed mismatch are the weak screen's, and its` |
|         - | 1625 | ` * verdicts stand in both modes.` |
|         - | 1626 | ` */` |
|       194 | 1627 | `static int VmStrictArgRefused(ph7_value *pArg,const char *zType,int nType)` |
|         3 | 1628 | `{` |
|         - | 1629 | `	/* Tested in ph7_type_name()'s own order, so the branch taken and the name the` |
|         - | 1630 | `	 * refusal reports can never disagree. FLOAT comes before INT on purpose:` |
|         - | 1631 | `	 * ph7_value_is_int() is deliberately lenient — an integer-valued real caches an` |
|         - | 1632 | ``	 * int and answers TRUE — and `str_repeat("a", 2.0)` is php's TypeError, not an`` |
|         - | 1633 | `	 * accepted int. */` |
|       197 | 1634 | `	if( ph7_value_is_bool(pArg) ){` |
|        12 | 1635 | `		return !VmSigTypeHas(zType,nType,"bool")` |
|         7 | 1636 | `		    && !VmSigTypeHas(zType,nType,"true")` |
|        11 | 1637 | `		    && !VmSigTypeHas(zType,nType,"false");` |
|         - | 1638 | `	}` |
|       189 | 1639 | `	if( ph7_value_is_float(pArg) ){` |
|         7 | 1640 | `		return !VmSigTypeHas(zType,nType,"float");` |
|         - | 1641 | `	}` |
|       183 | 1642 | `	if( ph7_value_is_int(pArg) ){` |
|         - | 1643 | `		/* int -> float is the one widening strict mode keeps. */` |
|        28 | 1644 | `		return !VmSigTypeHas(zType,nType,"int") && !VmSigTypeHas(zType,nType,"float");` |
|         - | 1645 | `	}` |
|       157 | 1646 | `	if( ph7_value_is_string(pArg) ){` |
|         - | 1647 | ``		/* `callable` is not a coercion: a function-name string satisfies it in both`` |
|         - | 1648 | `		 * modes (array_map('strtoupper', …) under strict is php-legal). */` |
|       104 | 1649 | `		return !VmSigTypeHas(zType,nType,"string") && !VmSigTypeHas(zType,nType,"callable");` |
|         - | 1650 | `	}` |
|        54 | 1651 | `	if( ph7_value_is_object(pArg) ){` |
|         - | 1652 | ``		/* An object reaches a `string` parameter only through __toString(), which is`` |
|         - | 1653 | `		 * a coercion strict mode does not perform. Every other arm is the weak` |
|         - | 1654 | `		 * screen's decision. */` |
|        24 | 1655 | `		return VmSigTypeHas(zType,nType,"string")` |
|        12 | 1656 | `		    && !VmSigTypeHas(zType,nType,"object")` |
|         2 | 1657 | `		    && !VmSigTypeHas(zType,nType,"iterable")` |
|         2 | 1658 | `		    && !VmSigTypeHas(zType,nType,"callable")` |
|        23 | 1659 | `		    && !VmSigTypeHasClass(zType,nType);` |
|         - | 1660 | `	}` |
|        32 | 1661 | `	return 0;` |
|       100 | 1662 | `}` |
|         - | 1663 | `/*` |
|         - | 1664 | ` * PHP-8 ZPP type enforcement for host functions, driven by the aBuiltinSig[]` |
|         - | 1665 | ` * declaration (band A #7). Screens only the arguments that php can NEVER coerce` |
|         - | 1666 | ` * into a declared scalar parameter — arrays, resources, and objects without a` |
|         - | 1667 | ` * __toString() — and throws the catchable TypeError php throws, before the C` |
|         - | 1668 | ` * routine runs. Without this an array argument reached the builtin and was` |
|         - | 1669 | ` * stringified to the literal "Array" (strrev(['a']) returned "yarrA").` |
|         - | 1670 | ` *` |
|         - | 1671 | ` * Deliberately narrow: scalar-to-scalar coercion (and its deprecations) stays` |
|         - | 1672 | ` * with the per-builtin ZPP helpers. Those emit E_DEPRECATED, which does NOT` |
|         - | 1673 | ` * abort the call, so a central copy would double-fire — the trap that sank the` |
|         - | 1674 | ` * first central-ZPP attempt. A TypeError aborts, so there is nothing to double.` |
|         - | 1675 | ` */` |
|   2969568 | 1676 | `PH7_PRIVATE sxi32 VmEnforceBuiltinArgTypes(` |
|         - | 1677 | `	ph7_context *pCtx,    /* Call context (for the throw) */` |
|         - | 1678 | `	ph7_user_func *pFunc, /* Callee */` |
|         - | 1679 | `	int nGiven,           /* Argument count */` |
|         - | 1680 | `	ph7_value **apArg     /* Arguments */` |
|         - | 1681 | `	)` |
|         5 | 1682 | `{` |
|         - | 1683 | `	/*` |
|         - | 1684 | `	 * Builtins whose own argument check is php-exact and VALUE-based rather than` |
|         - | 1685 | `	 * type-based must not be pre-empted here, or their message is lost. Same rule` |
|         - | 1686 | `	 * the aBuiltinArity[] table follows: a builtin that already says what php says` |
|         - | 1687 | `	 * stays off the shared screen. get_class_vars() takes any stringifiable value` |
|         - | 1688 | `	 * and reports "must be a valid class name, Array given"; get_class_methods() is` |
|         - | 1689 | `	 * the same shape with php's other wording ("must be an object or a valid class` |
|         - | 1690 | ``	 * name, int given") — the declared `object\|string` never appears in either.`` |
|         - | 1691 | `	 *` |
|         - | 1692 | `	 * strtr() is here for a structural reason: php declares it as two OVERLOADS` |
|         - | 1693 | `	 * dispatched on arity — strtr(string, array) and strtr(string, string, string)` |
|         - | 1694 | `` 	 * — so the expected type of $from is `array` with two arguments and `string` `` |
|         - | 1695 | ``	 * with three. One signature cannot say that (the stub's `array\|string` is the`` |
|         - | 1696 | `	 * union of the two, which is the wording php never uses), so the builtin does` |
|         - | 1697 | `	 * its own dispatch through PH7_ArgSatisfiesString(), the same rule as here.` |
|         - | 1698 | `	 *` |
|         - | 1699 | ``	 * implode() is the same structure: `array\|string $separator` is what the two`` |
|         - | 1700 | `	 * ARITIES accept between them, never what one call can use. Once an $array` |
|         - | 1701 | `	 * argument is present php has resolved the overload and reports` |
|         - | 1702 | ``	 * `must be of type string`, and with the array in position #1 it reports`` |
|         - | 1703 | ``	 * `must be of type string, array given` against #1 rather than a #2 error.`` |
|         - | 1704 | `	 * PH7_builtin_implode words all of that itself.` |
|         - | 1705 | `	 *` |
|         - | 1706 | `	 * Its alias join() is here for the same reason and then some: php 8.5 does not` |
|         - | 1707 | `	 * word the two the same, so the builtin reproduces BOTH orders keyed on the` |
|         - | 1708 | `	 * invoked name (see PH7_builtin_implode's header for the value-for-value` |
|         - | 1709 | `	 * table against 8.5.8). php's own asymmetry between a target and its alias,` |
|         - | 1710 | `	 * reproduced rather than smoothed over — parity is binding (§10).` |
|         - | 1711 | `	 *` |
|         - | 1712 | `	 * number_format() is here because php's DECLARED type and its REFUSAL text` |
|         - | 1713 | ``	 * disagree: the stub says `float $num` (which is what Reflection prints) while`` |
|         - | 1714 | `	 * the ZPP macro behind it is Z_PARAM_NUMBER, whose TypeError says` |
|         - | 1715 | ``	 * `must be of type int\|float`. One row cannot say both, so the row carries the`` |
|         - | 1716 | `	 * declared type for Reflection and the builtin words every refusal itself.` |
|         - | 1717 | `	 *` |
|         - | 1718 | `	 * RecursiveIteratorIterator::__construct() is the first NATIVE METHOD here, and` |
|         - | 1719 | `	 * it is the same disagreement one level up: php's stub declares` |
|         - | 1720 | ``	 * `Traversable $iterator` (what Reflection prints) while its ZPP is a bare "o",`` |
|         - | 1721 | ``	 * whose TypeError says `must be of type object`. A native method's diagnostic`` |
|         - | 1722 | `	 * name is the QUALIFIED one, so the row below matches it and nothing else.` |
|         - | 1723 | `	 *` |
|         - | 1724 | `	 * The array_udiff/array_uintersect u-variant family is here for its ORDER:` |
|         - | 1725 | `	 * php validates the trailing comparison callback(s) before ANY of the` |
|         - | 1726 | `	 * arrays — array_diff_ukey(123,[1],456) names Argument #3, not #1 — and a` |
|         - | 1727 | `	 * positional screen cannot say that. HashmapUVariant performs the whole` |
|         - | 1728 | `	 * php sequence itself (callbacks, then Argument #1, then the middles).` |
|         - | 1729 | `	 */` |
|         - | 1730 | `	static const char *azSelfChecked[] = { "get_class_vars", "get_class_methods", "strtr",` |
|         - | 1731 | `		"implode", "join", "number_format", "RecursiveIteratorIterator::__construct",` |
|         - | 1732 | `		"array_udiff", "array_udiff_assoc", "array_udiff_uassoc",` |
|         - | 1733 | `		"array_uintersect", "array_uintersect_assoc", "array_uintersect_uassoc",` |
|         - | 1734 | `		"array_diff_uassoc", "array_diff_ukey",` |
|         - | 1735 | `		"array_intersect_uassoc", "array_intersect_ukey" };` |
|   2969573 | 1736 | `	const char *zSig = pFunc->zSig;` |
|         - | 1737 | `	const char *zCur, *zEnd;` |
|   2969573 | 1738 | `	int iArg = 0;` |
|         - | 1739 | `	/* The CALL site's file mode, stamped by the compiler onto this call's argument` |
|         - | 1740 | `	 * map (weak when there is no map — a call that carries no compile-time metadata` |
|         - | 1741 | `	 * was written in a weak-mode file, since a strict one always attaches one). */` |
|   2969573 | 1742 | `	int bStrict = (pCtx->pArgMap && pCtx->pArgMap->bStrict) ? 1 : 0;` |
|         - | 1743 | `	sxu32 nPathMask;` |
|   2969573 | 1744 | `	if( zSig == 0 ){` |
|    205359 | 1745 | `		return SXRET_OK;` |
|         - | 1746 | `	}` |
|   2764219 | 1747 | `	nPathMask = VmBuiltinPathMask(&pFunc->sName);` |
|  49141435 | 1748 | `	for( iArg = 0 ; iArg < (int)SX_ARRAYSIZE(azSelfChecked) ; ++iArg ){` |
|  69645780 | 1749 | `		if( SyStrncmp(pFunc->sName.zString,azSelfChecked[iArg],` |
|  69645780 | 1750 | `			(sxu32)SyStrlen(azSelfChecked[iArg])) == 0` |
|  23246579 | 1751 | `		 && pFunc->sName.nByte == SyStrlen(azSelfChecked[iArg]) ){` |
|     44027 | 1752 | `			return SXRET_OK;` |
|         - | 1753 | `		}` |
|  23202536 | 1754 | `	}` |
|   2720197 | 1755 | `	iArg = 0;` |
|   2720197 | 1756 | `	zCur = zSig;` |
|   2720197 | 1757 | `	zEnd = &zSig[SyStrlen(zSig)];` |
|   6483764 | 1758 | `	while( zCur < zEnd && iArg < nGiven ){` |
|         - | 1759 | `		const char *zType, *zName, *zStop;` |
|         - | 1760 | `		int nType, nName, bByRef;` |
|         - | 1761 | `		ph7_value *pArg;` |
|         - | 1762 | `		char zGivenBuf[64];` |
|         - | 1763 | `		/* Parameter = "<type> $<name>[ = <default>]"; the type is whatever` |
|         - | 1764 | `		 * precedes the '$', and an empty type means "untyped" (no screen). */` |
|   4885137 | 1765 | `		while( zCur < zEnd && zCur[0] == ' ' ){` |
|   1112544 | 1766 | `			zCur++;` |
|         5 | 1767 | `		}` |
|   3772598 | 1768 | `		zStop = zCur;` |
|  65370569 | 1769 | `		while( zStop < zEnd && zStop[0] != ',' ){` |
|  61597976 | 1770 | `			if( zStop[0] == '\'' \|\| zStop[0] == '"' ){` |
|   1458992 | 1771 | `				zStop = VmSigSkipQuoted(zStop);` |
|   1458992 | 1772 | `				if( zStop >= zEnd ){` |
|       ! 0 | 1773 | `					break;` |
|         - | 1774 | `				}` |
|    729493 | 1775 | `			}` |
|  61597976 | 1776 | `			zStop++;` |
|         5 | 1777 | `		}` |
|   3772598 | 1778 | `		zName = zCur;` |
|  28376521 | 1779 | `		while( zName < zStop && zName[0] != '$' ){` |
|  24603928 | 1780 | `			zName++;` |
|         5 | 1781 | `		}` |
|   3772598 | 1782 | `		if( zName >= zStop ){` |
|       ! 0 | 1783 | `			break; /* malformed / no parameter name — stop screening */` |
|         - | 1784 | `		}` |
|   3772598 | 1785 | `		if( zName >= zCur + 3 && SyMemcmp(zName - 3,"...",3) == 0 ){` |
|      8005 | 1786 | `			break; /* variadic tail: stop (its type applies to the rest) */` |
|         - | 1787 | `		}` |
|   3764598 | 1788 | `		zType = zCur;` |
|   3764598 | 1789 | `		nType = (int)(zName - zCur);` |
|         - | 1790 | `		/* Trim the trailing spaces and the by-ref marker of "array &$array" */` |
|   3764598 | 1791 | `		bByRef = 0;` |
|  11153958 | 1792 | `		while( nType > 0 && (zType[nType-1] == ' ' \|\| zType[nType-1] == '&') ){` |
|   3694490 | 1793 | `			if( zType[nType-1] == '&' ){` |
|      3465 | 1794 | `				bByRef = 1;` |
|      1729 | 1795 | `			}` |
|   3694490 | 1796 | `			nType--;` |
|         5 | 1797 | `		}` |
|   3764598 | 1798 | `		zName++; /* skip '$' */` |
|   3764598 | 1799 | `		nName = 0;` |
|  28196098 | 1800 | `		while( &zName[nName] < zStop && zName[nName] != ' ' && zName[nName] != '=' ){` |
|  24431505 | 1801 | `			nName++;` |
|         5 | 1802 | `		}` |
|   3764598 | 1803 | `		pArg = apArg[iArg];` |
|   3764593 | 1804 | `		if( bByRef && pArg->nIdx == SXU32_HIGH` |
|      1770 | 1805 | `		 && !(pCtx->pArgMap && pCtx->pArgMap->bArgShapes && !pCtx->pArgMap->bHasNamed) ){` |
|         - | 1806 | `			/* A by-reference parameter handed something with no slot to write back` |
|         - | 1807 | `			 * through -- a literal, a constant, the result of a call. php settles` |
|         - | 1808 | `			 * that at the CALL, before the callee's ZPP runs, so the type screen` |
|         - | 1809 | ``			 * must not speak first: `array_pop('foo')` is`` |
|         - | 1810 | `			 * "could not be passed by reference" and not "must be of type array,` |
|         - | 1811 | `			 * string given".` |
|         - | 1812 | `			 *` |
|         - | 1813 | `			 * Only when this call site carries no argument SHAPES, though. When it` |
|         - | 1814 | `			 * does, PH7_VmScreenByRefArgShapes has already had its say — it refused` |
|         - | 1815 | `			 * the literal and let the call RESULT through with php's notice — and` |
|         - | 1816 | `			 * standing aside here would swallow the type error php still reports for` |
|         - | 1817 | ``			 * the latter (`sort(new stdClass)` is "must be of type array, stdClass`` |
|         - | 1818 | `			 * given", not a silent false). */` |
|         7 | 1819 | `			zCur = (zStop < zEnd) ? zStop + 1 : zEnd;` |
|         7 | 1820 | `			iArg++;` |
|         7 | 1821 | `			continue;` |
|         - | 1822 | `		}` |
|   3764592 | 1823 | `		if( nType > 0 && !VmSigTypeHas(zType,nType,"mixed") ){` |
|   3556608 | 1824 | `			const char *zGiven = 0;` |
|   3556608 | 1825 | `			if( bStrict && VmStrictArgRefused(pArg,zType,nType) ){` |
|         - | 1826 | ``				/* php names the VALUE for a bool here too (`true given`). */`` |
|        37 | 1827 | `				zGiven = VmValueGivenName(pArg,zGivenBuf,sizeof(zGivenBuf));` |
|   3556590 | 1828 | `			}else if( (pArg->iFlags & MEMOBJ_HASHMAP) != 0 ){` |
|     53725 | 1829 | `				if( !VmSigTypeHas(zType,nType,"array")` |
|     27077 | 1830 | `				 && !VmSigTypeHas(zType,nType,"iterable")` |
|       421 | 1831 | `				 && !VmSigTypeHas(zType,nType,"callable") ){` |
|       199 | 1832 | `					zGiven = "array";` |
|       102 | 1833 | `				}` |
|   3529716 | 1834 | `			}else if( (pArg->iFlags & MEMOBJ_OBJ) != 0 ){` |
|     12574 | 1835 | `				if( !VmSigTypeHas(zType,nType,"object")` |
|      9642 | 1836 | `				 && !VmSigTypeHas(zType,nType,"iterable")` |
|      6710 | 1837 | `				 && !VmSigTypeHas(zType,nType,"callable")` |
|      5839 | 1838 | `				 && !VmSigTypeHasClass(zType,nType) ){` |
|         - | 1839 | `					/* An object with __toString() still satisfies a string` |
|         - | 1840 | `					 * parameter in weak mode — php coerces it. */` |
|       216 | 1841 | `					int bStringable = VmSigTypeHas(zType,nType,"string")` |
|       154 | 1842 | `						&& PH7_ArgSatisfiesString(pArg);` |
|       158 | 1843 | `					if( !bStringable ){` |
|        84 | 1844 | `						zGiven = VmArgTypeName(pArg);` |
|        40 | 1845 | `					}` |
|     12501 | 1846 | `				}else if( VmSigTypeHasClass(zType,nType)` |
|      8535 | 1847 | `				       && !VmSigTypeHas(zType,nType,"object")` |
|      4650 | 1848 | `				       && !VmSigTypeHas(zType,nType,"iterable")` |
|      4650 | 1849 | `				       && !VmSigTypeHas(zType,nType,"callable")` |
|      4655 | 1850 | `				       && !VmSigTypeHas(zType,nType,"string") ){` |
|         - | 1851 | `					/* A class-typed parameter given an object of the WRONG class.` |
|         - | 1852 | `					 * Naming a class used to be enough to let ANY object through, so` |
|         - | 1853 | `` 					 * `date_modify($immutable)` and `timezone_name_get($date)` `` |
|         - | 1854 | `					 * answered silently where php raises. Only decided when every` |
|         - | 1855 | `					 * class arm resolves to a declared class: an arm PHL does not` |
|         - | 1856 | `					 * declare cannot be judged, so the parameter stays unscreened. */` |
|      6887 | 1857 | `					if( !VmSigObjSatisfiesClass(pCtx->pVm,zType,nType,` |
|      4588 | 1858 | `						(ph7_class_instance *)pArg->x.pOther) ){` |
|        29 | 1859 | `						zGiven = VmArgTypeName(pArg);` |
|        13 | 1860 | `					}` |
|      2299 | 1861 | `				}` |
|   3496560 | 1862 | `			}else if( (pArg->iFlags & MEMOBJ_NULL) != 0 ){` |
|         - | 1863 | `				/* php only DEPRECATES null for a non-nullable parameter; PHL rejects` |
|         - | 1864 | `				 * it (scope policy). A leading '?' or an explicit "null" arm in a` |
|         - | 1865 | `				 * union declares the parameter nullable. A "callable" parameter is` |
|         - | 1866 | `				 * left to the builtin's own callback check, which words the failure` |
|         - | 1867 | `				 * php's way ("must be a valid callback, no array or string given") —` |
|         - | 1868 | `				 * the same reason get_class_vars() sits on azSelfChecked[]. */` |
|      6666 | 1869 | `				if( zType[0] != '?'` |
|      3399 | 1870 | `				 && !VmSigTypeHas(zType,nType,"null")` |
|       113 | 1871 | `				 && !VmSigTypeHas(zType,nType,"callable") ){` |
|        81 | 1872 | `					zGiven = "null";` |
|        38 | 1873 | `				}` |
|   3486940 | 1874 | `			}else if( (pArg->iFlags & (MEMOBJ_STRING\|MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL)) != 0` |
|   3483602 | 1875 | `			       && (VmSigTypeHasClass(zType,nType)` |
|   3483339 | 1876 | `			        \|\| VmSigTypeHas(zType,nType,"object")) ){` |
|         - | 1877 | `				/* A SCALAR against a parameter that can only hold an INSTANCE —` |
|         - | 1878 | ``				 * a named class, or the bare `object` keyword. Every other scalar`` |
|         - | 1879 | `				 * pairing is left to weak-mode coercion, which is why nothing` |
|         - | 1880 | `				 * screened scalars here at all — but no coercion produces an` |
|         - | 1881 | `				 * instance, so php rejects this one. Found converting DateTime:` |
|         - | 1882 | ``				 * `$d->diff('x')` and `new DateTime('now','UTC')` ran on with a`` |
|         - | 1883 | ``				 * string where php raises. The `object` half was still blind when`` |
|         - | 1884 | `				 * WeakReference::create() declared the first such parameter, which` |
|         - | 1885 | `				 * also retires the "graceful degradation" NULL that spl_object_id(),` |
|         - | 1886 | `				 * spl_object_hash() and get_object_vars() used to answer. An arm a` |
|         - | 1887 | `				 * scalar CAN satisfy (a union with string/int/float/bool, or` |
|         - | 1888 | `				 * callable, which a string is) keeps the parameter unscreened —` |
|         - | 1889 | ``				 * and so does an `array` arm, whose refusal php words from the`` |
|         - | 1890 | `				 * builtin's own check rather than from the declared type` |
|         - | 1891 | ``				 * (array_walk's `array\|object &$array` says "must be of type`` |
|         - | 1892 | `				 * array", not "of type array\|object"). */` |
|      1959 | 1893 | `				if( !VmSigTypeHas(zType,nType,"string")` |
|      1019 | 1894 | `				 && !VmSigTypeHas(zType,nType,"int")` |
|       122 | 1895 | `				 && !VmSigTypeHas(zType,nType,"float")` |
|        86 | 1896 | `				 && !VmSigTypeHas(zType,nType,"bool")` |
|        86 | 1897 | `				 && !VmSigTypeHas(zType,nType,"true")` |
|        86 | 1898 | `				 && !VmSigTypeHas(zType,nType,"false")` |
|        86 | 1899 | `				 && !VmSigTypeHas(zType,nType,"array")` |
|        72 | 1900 | `				 && !VmSigTypeHas(zType,nType,"callable") ){` |
|         - | 1901 | `					/* php's VALUE name, not the type's: a bool is reported as` |
|         - | 1902 | ``					 * `true`/`false` (the rule Generator::throw()'s own check`` |
|         - | 1903 | `					 * already followed, and which this screen now runs first). */` |
|        53 | 1904 | `					zGiven = VmValueGivenName(pArg,zGivenBuf,sizeof(zGivenBuf));` |
|        24 | 1905 | `				}` |
|   3482667 | 1906 | `			}else if( (pArg->iFlags & MEMOBJ_REAL) != 0` |
|   1742019 | 1907 | `			       && VmSigTypeIsIntOnly(zType,nType)` |
|       321 | 1908 | `			       && !VmDoubleFitsInt((double)pArg->rVal) ){` |
|         - | 1909 | ``				/* A FLOAT against a parameter typed exactly `int` (or `?int`), and`` |
|         - | 1910 | `				 * one no int can hold: a fraction, a magnitude past the signed` |
|         - | 1911 | `				 * 64-bit range, NaN or an infinity. php refuses every one of them` |
|         - | 1912 | `				 * (zend_parse_arg_long's ZEND_DOUBLE_FITS_LONG / is-integral pair,` |
|         - | 1913 | `				 * the fractional case with a deprecation PHL rejects outright by` |
|         - | 1914 | `				 * §10) and the refusal is this screen's own wording.` |
|         - | 1915 | `				 *` |
|         - | 1916 | `				 * PH7_IntArgResolve has always said exactly this, but only for the` |
|         - | 1917 | `` 				 * builtins that CALL it from their own body — so `dechex(1.5)` `` |
|         - | 1918 | ``				 * answered '1', `array_fill(1.5,1,0)` filled from 1, and`` |
|         - | 1919 | ``				 * `strpos("abc","c",1e19)` took the offset as PHP_INT_MIN and`` |
|         - | 1920 | `				 * reported a ValueError about a range it never had. Seventy-five` |
|         - | 1921 | ``				 * `int` parameters across the signature table were unscreened that`` |
|         - | 1922 | `				 * way, and a NATIVE METHOD has no body to call the helper from at` |
|         - | 1923 | `				 * all. Deciding it from the declared type covers both callee kinds` |
|         - | 1924 | `				 * from one place, and the per-builtin helper still stands for the` |
|         - | 1925 | ``				 * message rows this screen cannot reach (the `azSelfChecked` set). */`` |
|        62 | 1926 | `				zGiven = "float";` |
|   3481697 | 1927 | `			}else if( (pArg->iFlags & (MEMOBJ_STRING\|MEMOBJ_NULL)) == MEMOBJ_STRING` |
|   3036872 | 1928 | `			       && (VmSigTypeHas(zType,nType,"int")` |
|   2591278 | 1929 | `			        \|\| VmSigTypeHas(zType,nType,"float"))` |
|   1296837 | 1930 | `			       && !VmSigTypeHas(zType,nType,"string")` |
|   1296604 | 1931 | `			       && !VmSigTypeHas(zType,nType,"array")` |
|       274 | 1932 | `			       && !VmSigTypeHas(zType,nType,"object")` |
|       266 | 1933 | `			       && !VmSigTypeHas(zType,nType,"iterable")` |
|       266 | 1934 | `			       && !VmSigTypeHas(zType,nType,"callable")` |
|       266 | 1935 | `			       && !VmSigTypeHas(zType,nType,"bool")` |
|       271 | 1936 | `			       && !VmSigTypeHasClass(zType,nType) ){` |
|         - | 1937 | ``				/* A STRING against a NUMBER-only parameter — `int`, `float`, or the`` |
|         - | 1938 | ``				 * `int\|float` union, with no arm a string can satisfy. Weak mode`` |
|         - | 1939 | `				 * coerces a NUMERIC one and php refuses every other — "x", "2abc"` |
|         - | 1940 | ``				 * and "0x2" are all `must be of type int, string given` (rule 41: a`` |
|         - | 1941 | `				 * numeric PREFIX is not enough, which is what SyStrIsNumeric would` |
|         - | 1942 | `				 * have accepted). Every BUILTIN with an int parameter already got` |
|         - | 1943 | `				 * this from PH7_IntArgResolve, called from its own body; a native` |
|         - | 1944 | `` 				 * METHOD has no body to call it from, so `ArrayIterator::seek('x')` `` |
|         - | 1945 | ``				 * seeked to 0, `DateTime::setTimestamp('abc')` set 0 and`` |
|         - | 1946 | ``				 * `DOMNodeList::item('zz')` answered element 0 — wrong ANSWERS,`` |
|         - | 1947 | `				 * not missing errors. Screening the declared type here covers both` |
|         - | 1948 | `				 * callee kinds from one place.` |
|         - | 1949 | `				 *` |
|         - | 1950 | `				 * The FLOAT arm is the same hazard one type over, and it was the` |
|         - | 1951 | `				 * half nothing covered: PH7_IntArgResolve has no float twin, so a` |
|         - | 1952 | ``				 * `float $num` builtin that did not hand-roll its own check simply`` |
|         - | 1953 | `				 * converted the string to 0.0 and COMPUTED with it —` |
|         - | 1954 | ``				 * `cos("nope")` answered `float(1)`, `sqrt("nope")` `float(0)`,`` |
|         - | 1955 | ``				 * `log("nope")` `float(-INF)`. Numbers with nothing wrong-looking`` |
|         - | 1956 | `				 * about them, from input php refuses outright.` |
|         - | 1957 | `				 *` |
|         - | 1958 | `				 * The NULL rule stays where it is: PHL rejects null for a` |
|         - | 1959 | `				 * non-nullable parameter by policy (§10) where php deprecates. */` |
|       404 | 1960 | `				if( !PH7_MemObjStringIsNumeric(pArg) ){` |
|       161 | 1961 | `					zGiven = "string";` |
|       191 | 1962 | `				}else if( VmSigTypeIsIntOnly(zType,nType) && !VmNumStrFitsInt(pArg) ){` |
|         - | 1963 | `					/* A NUMERIC string an int cannot hold — "1.5", "1e19",` |
|         - | 1964 | `					 * "99999999999999999999". php refuses all three (the fractional` |
|         - | 1965 | `					 * one after a deprecation §10 turns into the refusal), and PHL` |
|         - | 1966 | ``					 * narrowed them silently: `dechex("1e19")` answered '1' and`` |
|         - | 1967 | ``					 * `str_repeat("a","99999999999999999999")` took PHP_INT_MAX as`` |
|         - | 1968 | `					 * the count. Same wording, same position as the float arm above,` |
|         - | 1969 | `					 * because php reaches both through one ZPP macro. */` |
|        19 | 1970 | `					zGiven = "string";` |
|         8 | 1971 | `				}` |
|   3481536 | 1972 | `			}else if( (pArg->iFlags & (MEMOBJ_STRING\|MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL)) != 0` |
|   3481398 | 1973 | `			       && VmSigTypeIsArrayOnly(zType,nType) ){` |
|         - | 1974 | ``				/* A SCALAR against a parameter typed exactly `array`. No coercion`` |
|         - | 1975 | `				 * produces one, so php refuses it -- but the screen exempted every` |
|         - | 1976 | ``				 * `array` arm, union or not, and a whole family had no check of its`` |
|         - | 1977 | `				 * own to fall back on: sort/rsort/ksort/krsort/shuffle and` |
|         - | 1978 | ``				 * usort/uasort/uksort each answered `false` for `sort($notAnArray)`,`` |
|         - | 1979 | `				 * which is also what they answer for a sort that genuinely failed.` |
|         - | 1980 | `				 * call_user_func_array('strlen', 'x') answered false too,` |
|         - | 1981 | `				 * iterator_apply RAN the callback, and getopt/hash/password_hash/` |
|         - | 1982 | `				 * password_needs_rehash/unserialize/fputcsv simply carried on with` |
|         - | 1983 | `				 * the string where an options ARRAY was declared.` |
|         - | 1984 | `				 *` |
|         - | 1985 | `				 * The builtins that DO check (array_keys, in_array, asort, ...) word` |
|         - | 1986 | `				 * it identically, so the screen only pre-empts them -- and corrects` |
|         - | 1987 | `				 * one detail on the way: their ph7_type_name() says "bool" where php` |
|         - | 1988 | ``				 * names the VALUE, `true` or `false`. */`` |
|       233 | 1989 | `				zGiven = VmValueGivenName(pArg,zGivenBuf,sizeof(zGivenBuf));` |
|   3481289 | 1990 | `			}else if( (pArg->iFlags & MEMOBJ_RES) != 0 ){` |
|         - | 1991 | `				/* A class-typed parameter also accepts a resource: several handles php 8` |
|         - | 1992 | `				 * models as objects are still resources here (xml_*'s XMLParser is the` |
|         - | 1993 | `				 * one the signatures already declare php-8-style, for reflection). The` |
|         - | 1994 | `				 * screen would otherwise reject the engine's own parser handle. Recorded` |
|         - | 1995 | `				 * as a divergence in NEWPLAN §7 — it goes away when those handles become` |
|         - | 1996 | `				 * real objects. */` |
|        10 | 1997 | `				if( !VmSigTypeHas(zType,nType,"resource")` |
|        12 | 1998 | `				 && !VmSigTypeHasClass(zType,nType) ){` |
|        12 | 1999 | `					zGiven = "resource";` |
|         5 | 2000 | `				}` |
|         5 | 2001 | `			}` |
|   3556608 | 2002 | `			if( zGiven ){` |
|         - | 2003 | ``				/* php's `object\|array` parameters come from ONE ZPP macro`` |
|         - | 2004 | `				 * (Z_PARAM_ARRAY_OR_OBJECT) and it names only "array" in the` |
|         - | 2005 | `				 * refusal — array_walk(null,…), current(null) and` |
|         - | 2006 | `				 * http_build_query(null) all say "must be of type array". The` |
|         - | 2007 | `				 * SCALAR branch above already encodes that rule by declining to` |
|         - | 2008 | `				 * screen at all; the null and resource branches do screen, so the` |
|         - | 2009 | `				 * reported type has to be corrected here instead. */` |
|       933 | 2010 | `				if( VmSigTypeHas(zType,nType,"array") && VmSigTypeHas(zType,nType,"object") ){` |
|        12 | 2011 | `					zType = "array";` |
|        12 | 2012 | `					nType = (int)sizeof("array")-1;` |
|         5 | 2013 | `				}` |
|      1446 | 2014 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 2015 | `					"%z(): Argument #%d ($%.*s) must be of type %.*s, %s given",` |
|       464 | 2016 | `					&pFunc->sName,iArg + 1,nName,zName,nType,zType,zGiven);` |
|         - | 2017 | `			}` |
|   1778725 | 2018 | `		}` |
|         - | 2019 | ``		/* A NaN reaching a parameter php declares `string`: php's ZPP coerces it`` |
|         - | 2020 | ``		 * (to "NAN") and warns `unexpected NAN value was coerced to string`, the`` |
|         - | 2021 | `		 * same 8.5 diagnostic the cast and the concatenation raise. The builtin` |
|         - | 2022 | `		 * bodies read their argument with ph7_value_to_string, which is the SILENT` |
|         - | 2023 | `		 * conversion by design (the engine builds keys and messages with it), so` |
|         - | 2024 | `		 * the diagnostic belongs here, where the DECLARED type says a coercion is` |
|         - | 2025 | `		 * what is about to happen. A union that also accepts a NUMBER is left` |
|         - | 2026 | `		 * alone -- php keeps the float there and coerces nothing -- but` |
|         - | 2027 | ``		 * `array\|string`, the spelling str_replace()'s subject carries, does`` |
|         - | 2028 | `		 * coerce and does warn. */` |
|   3763659 | 2029 | `		if( (pArg->iFlags & (MEMOBJ_REAL\|MEMOBJ_STRING)) == MEMOBJ_REAL` |
|   1883891 | 2030 | `		 && PH7_IS_NAN((double)pArg->rVal)` |
|      1052 | 2031 | `		 && VmSigTypeHas(zType,nType,"string")` |
|        45 | 2032 | `		 && !VmSigTypeHas(zType,nType,"float")` |
|         5 | 2033 | `		 && !VmSigTypeHas(zType,nType,"int")` |
|         7 | 2034 | `		 && !VmSigTypeHas(zType,nType,"mixed") ){` |
|         3 | 2035 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 2036 | `				"unexpected NAN value was coerced to string");` |
|         1 | 2037 | `		}` |
|         - | 2038 | `		/* A PATH parameter, once its type is settled: php's Z_PARAM_PATH refuses a` |
|         - | 2039 | `		 * NUL byte outright rather than letting the C API truncate at it. Raised` |
|         - | 2040 | `		 * after the type verdict because that is php's order — the coercion runs` |
|         - | 2041 | `		 * first, and only a value that could BE a path is asked whether it is a` |
|         - | 2042 | `		 * legal one. */` |
|   3763664 | 2043 | `		if( iArg < 31 && (nPathMask & (1u<<iArg)) != 0 ){` |
|     91661 | 2044 | `			if( (pArg->iFlags & MEMOBJ_OBJ) != 0 && PH7_ArgSatisfiesString(pArg) ){` |
|         - | 2045 | `				/* A Stringable object: php coerces it and checks the RESULT, so` |
|         - | 2046 | ``				 * `unlink($o)` with a __toString() returning a NUL-bearing name is`` |
|         - | 2047 | `				 * the same ValueError. Converting IN PLACE is what keeps the` |
|         - | 2048 | `				 * accessor running exactly ONCE — the builtin then receives the` |
|         - | 2049 | `				 * string it would have produced itself. The argument a builtin sees` |
|         - | 2050 | `				 * is its own copy on every dispatch route (a direct call, a spread,` |
|         - | 2051 | `				 * both call_user_func forwards), so the caller's object is not` |
|         - | 2052 | `				 * retyped; strict mode never gets here, because a Stringable does` |
|         - | 2053 | ``				 * not satisfy a `string` parameter there and the screen above has`` |
|         - | 2054 | `				 * already refused it. */` |
|         3 | 2055 | `				sxi32 rcConv = PH7_MemObjToStringUV(pArg);` |
|         3 | 2056 | `				if( rcConv != SXRET_OK ){` |
|       ! 0 | 2057 | `					return rcConv; /* __toString() threw: php propagates it too */` |
|         - | 2058 | `				}` |
|         1 | 2059 | `			}` |
|     91661 | 2060 | `			if( VmArgHasNulByte(pArg) ){` |
|       150 | 2061 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 2062 | `					"%z(): Argument #%d ($%.*s) must not contain any null bytes",` |
|        49 | 2063 | `					&pFunc->sName,iArg + 1,nName,zName);` |
|         - | 2064 | `			}` |
|     45778 | 2065 | `		}` |
|   3763566 | 2066 | `		zCur = (zStop < zEnd) ? zStop + 1 : zEnd;` |
|   3763566 | 2067 | `		iArg++;` |
|         5 | 2068 | `	}` |
|   2719171 | 2069 | `	return SXRET_OK;` |
|   1485608 | 2070 | `}` |
|         - | 2071 | `/*` |
|         - | 2072 | ` * Builtins whose accepted arity is NOT a contiguous range, so the signature` |
|         - | 2073 | ` * cannot express it and the central too-many-arguments check must stay out of` |
|         - | 2074 | ` * the way: rand()/mt_rand() take 0 OR 2 arguments (never 1), and php words the` |
|         - | 2075 | ` * violation "expects exactly 2 arguments, 3 given" from their own check rather` |
|         - | 2076 | ` * than the ZPP "at most". They validate themselves; leaving bHasMaxArg at 0` |
|         - | 2077 | ` * keeps their message php-faithful.` |
|         - | 2078 | ` */` |
|   3476360 | 2079 | `static int VmBuiltinSelfValidatesArity(const char *zName)` |
|         5 | 2080 | `{` |
|         - | 2081 | `	static const char *const azSelf[] = { "rand", "mt_rand" };` |
|         - | 2082 | `	sxu32 i;` |
|  10415105 | 2083 | `	for( i = 0 ; i < SX_ARRAYSIZE(azSelf) ; i++ ){` |
|   6948065 | 2084 | `		sxu32 nSelf = SyStrlen(azSelf[i]);` |
|   6948065 | 2085 | `		if( SyStrlen(zName) == nSelf && SyStrncmp(zName,azSelf[i],nSelf) == 0 ){` |
|      9325 | 2086 | `			return 1;` |
|         - | 2087 | `		}` |
|   3469375 | 2088 | `	}` |
|   3467045 | 2089 | `	return 0;` |
|   1738185 | 2090 | `}` |
|         - | 2091 | `/*` |
|         - | 2092 | ` * One parameter of a declared signature, for the named-argument binder below.` |
|         - | 2093 | ` */` |
|         - | 2094 | `typedef struct VmSigParam VmSigParam;` |
|         - | 2095 | `struct VmSigParam` |
|         - | 2096 | `{` |
|         - | 2097 | `	const char *zName; int nName;   /* without the '$' */` |
|         - | 2098 | `	const char *zDef;  int nDef;    /* default TEXT, or 0 when the parameter is required */` |
|         - | 2099 | `	int bVariadic;` |
|         - | 2100 | `};` |
|         - | 2101 | `/*` |
|         - | 2102 | ` * Split a signature into its parameters: the NAME each one binds by and the default` |
|         - | 2103 | ` * TEXT to fall back on. The scan is VmDeriveArityFromSig's, kept apart because that one` |
|         - | 2104 | `` * only counts; a quoted default (`string $separator = ','`) hides a comma, which is why`` |
|         - | 2105 | ` * both go through VmSigSkipQuoted.` |
|         - | 2106 | ` */` |
|     60528 | 2107 | `static int VmSigParams(const char *zSig,VmSigParam *aOut,int nMax)` |
|         5 | 2108 | `{` |
|     60533 | 2109 | `	const char *zCur = zSig;` |
|     60533 | 2110 | `	const char *zStart = zSig;` |
|     60533 | 2111 | `	int n = 0;` |
|   2406223 | 2112 | `	for(;;){` |
|   4993425 | 2113 | `		if( zCur[0] == '\'' \|\| zCur[0] == '"' ){` |
|        19 | 2114 | `			zCur = VmSigSkipQuoted(zCur);` |
|        19 | 2115 | `			if( zCur[0] != '\0' ){` |
|        19 | 2116 | `				zCur++;` |
|         9 | 2117 | `			}` |
|        19 | 2118 | `			continue;` |
|         - | 2119 | `		}` |
|   4993407 | 2120 | `		if( zCur[0] == '\0' \|\| zCur[0] == ',' ){` |
|    241489 | 2121 | `			const char *z = zStart;` |
|    241489 | 2122 | `			const char *zEnd = zCur;` |
|    241489 | 2123 | `			if( n < nMax ){` |
|    241489 | 2124 | `				VmSigParam *p = &aOut[n];` |
|    241489 | 2125 | `				const char *zEq = 0;` |
|    241489 | 2126 | `				const char *zDollar = 0;` |
|    241489 | 2127 | `				p->zName = 0; p->nName = 0; p->zDef = 0; p->nDef = 0; p->bVariadic = 0;` |
|   4993485 | 2128 | `				for( ; z < zEnd ; z++ ){` |
|   4752001 | 2129 | `					if( z[0] == '$' && zDollar == 0 ){` |
|    241489 | 2130 | `						zDollar = z + 1;` |
|   4631259 | 2131 | `					}else if( z[0] == '=' && zEq == 0 ){` |
|     62309 | 2132 | `						zEq = z + 1;` |
|   4479365 | 2133 | `					}else if( z[0] == '.' && z + 2 < zEnd && z[1] == '.' && z[2] == '.' ){` |
|       100 | 2134 | `						p->bVariadic = 1;` |
|        48 | 2135 | `					}` |
|   2376003 | 2136 | `				}` |
|    241489 | 2137 | `				if( zDollar ){` |
|    241489 | 2138 | `					const char *zStop = zEq ? zEq - 1 : zEnd;` |
|    241489 | 2139 | `					const char *zN = zDollar;` |
|   1756225 | 2140 | `					while( zN < zStop && zN[0] != ' ' && zN[0] != '=' ){` |
|   1514741 | 2141 | `						zN++;` |
|         5 | 2142 | `					}` |
|    241489 | 2143 | `					p->zName = zDollar;` |
|    241489 | 2144 | `					p->nName = (int)(zN - zDollar);` |
|    120742 | 2145 | `				}` |
|    241489 | 2146 | `				if( zEq ){` |
|    124613 | 2147 | `					while( zEq < zEnd && zEq[0] == ' ' ){` |
|     62309 | 2148 | `						zEq++;` |
|         5 | 2149 | `					}` |
|     62309 | 2150 | `					p->zDef = zEq;` |
|     62309 | 2151 | `					p->nDef = (int)(zEnd - zEq);` |
|     62309 | 2152 | `					while( p->nDef > 0 && p->zDef[p->nDef-1] == ' ' ){` |
|       ! 0 | 2153 | `						p->nDef--;` |
|       ! 0 | 2154 | `					}` |
|     31152 | 2155 | `				}` |
|    241489 | 2156 | `				if( p->nName > 0 ){` |
|    241489 | 2157 | `					n++;` |
|    120742 | 2158 | `				}` |
|    120742 | 2159 | `			}` |
|    241489 | 2160 | `			if( zCur[0] == '\0' ){` |
|     60533 | 2161 | `				break;` |
|         - | 2162 | `			}` |
|    180961 | 2163 | `			zCur++;` |
|    180961 | 2164 | `			zStart = zCur;` |
|    180961 | 2165 | `			continue;` |
|         - | 2166 | `		}` |
|   4751923 | 2167 | `		zCur++;` |
|         5 | 2168 | `	}` |
|     60533 | 2169 | `	return n;` |
|         5 | 2170 | `}` |
|         - | 2171 | `/*` |
|         - | 2172 | ` * Materialize a signature default's TEXT into pOut. php's own stub values, which is a` |
|         - | 2173 | `` * small set: null, true/false, an integer or float, a quoted string, and `[]`. A default`` |
|         - | 2174 | `` * the table could not state (`= ?`, ~50 rows — §7.4) answers 0, and the caller then reports`` |
|         - | 2175 | ` * the parameter as not passed rather than inventing a value.` |
|         - | 2176 | ` */` |
|         6 | 2177 | `static int VmSigDefaultValue(ph7_vm *pVm,const VmSigParam *pParam,ph7_value *pOut)` |
|         1 | 2178 | `{` |
|         7 | 2179 | `	const char *z = pParam->zDef;` |
|         7 | 2180 | `	int n = pParam->nDef;` |
|         7 | 2181 | `	if( z == 0 \|\| n < 1 \|\| (n == 1 && z[0] == '?') ){` |
|         3 | 2182 | `		return 0;` |
|         - | 2183 | `	}` |
|         5 | 2184 | `	if( n == 4 && (SyStrnicmp(z,"null",4) == 0) ){` |
|       ! 0 | 2185 | `		PH7_MemObjRelease(pOut);` |
|       ! 0 | 2186 | `		return 1; /* a released value IS null */` |
|         - | 2187 | `	}` |
|         5 | 2188 | `	if( n == 4 && SyStrnicmp(z,"true",4) == 0 ){` |
|       ! 0 | 2189 | `		PH7_MemObjInitFromBool(pVm,pOut,1);` |
|       ! 0 | 2190 | `		return 1;` |
|         - | 2191 | `	}` |
|         5 | 2192 | `	if( n == 5 && SyStrnicmp(z,"false",5) == 0 ){` |
|       ! 0 | 2193 | `		PH7_MemObjInitFromBool(pVm,pOut,0);` |
|       ! 0 | 2194 | `		return 1;` |
|         - | 2195 | `	}` |
|         5 | 2196 | `	if( n == 2 && z[0] == '[' && z[1] == ']' ){` |
|         3 | 2197 | `		ph7_hashmap *pMap = PH7_NewHashmap(pVm,0,0);` |
|         3 | 2198 | `		if( pMap == 0 ){` |
|       ! 0 | 2199 | `			return 0;` |
|         - | 2200 | `		}` |
|         3 | 2201 | `		PH7_MemObjRelease(pOut);` |
|         3 | 2202 | `		pOut->x.pOther = pMap;` |
|         3 | 2203 | `		MemObjSetType(pOut,MEMOBJ_HASHMAP);` |
|         3 | 2204 | `		return 1;` |
|         - | 2205 | `	}` |
|         3 | 2206 | `	if( z[0] == '\'' \|\| z[0] == '"' ){` |
|         - | 2207 | `		SyString sStr;` |
|         3 | 2208 | `		SyStringInitFromBuf(&sStr,z + 1,n >= 2 ? n - 2 : 0);` |
|         3 | 2209 | `		PH7_MemObjInitFromString(pVm,pOut,&sStr);` |
|         3 | 2210 | `		return 1;` |
|         - | 2211 | `	}` |
|       ! 0 | 2212 | `	if( z[0] == '-' \|\| z[0] == '+' \|\| (z[0] >= '0' && z[0] <= '9') ){` |
|         - | 2213 | `		SyString sNum;` |
|       ! 0 | 2214 | `		SyStringInitFromBuf(&sNum,z,(sxu32)n);` |
|       ! 0 | 2215 | `		if( PH7_MemObjInitFromString(pVm,pOut,&sNum) != SXRET_OK ){` |
|       ! 0 | 2216 | `			return 0;` |
|         - | 2217 | `		}` |
|       ! 0 | 2218 | `		PH7_MemObjToNumeric(pOut);` |
|       ! 0 | 2219 | `		return 1;` |
|         - | 2220 | `	}` |
|       ! 0 | 2221 | `	return 0; /* a constant expression (M_PI, PHP_ROUND_HALF_UP, …): not evaluated here */` |
|         4 | 2222 | `}` |
|         - | 2223 | `/*` |
|         - | 2224 | ` * Bind a call's NAMED arguments to the callee's declared parameter POSITIONS.` |
|         - | 2225 | ` *` |
|         - | 2226 | ` * A compiled function does this from its parameter records (VmResolveNamedArgs); a host` |
|         - | 2227 | ` * function and a native method have none, so every named argument was simply passed in the` |
|         - | 2228 | `` * order it was WRITTEN. `str_pad(length: 5, string: "x")` reached the builtin as`` |
|         - | 2229 | ` * ("x" at #2, 5 at #1) and reported a TypeError, and — worse, because it is silent —` |
|         - | 2230 | `` * `str_pad("x", 5, pad_type: STR_PAD_LEFT)` bound the flag as $pad_string and answered`` |
|         - | 2231 | ` * "x0000" where php answers "    x". Both spellings are php 8.0 syntax, and the whole` |
|         - | 2232 | ` * ~650-builtin surface plus every native method was affected.` |
|         - | 2233 | ` *` |
|         - | 2234 | ` * The declared signature is the source of names, defaults and positions — the same string` |
|         - | 2235 | ` * Reflection prints. Rewrites *pnArg / apArg in place (the caller's argument vector is` |
|         - | 2236 | ` * scratch it owns) and answers SXRET_OK, or throws php's Error and returns its status.` |
|         - | 2237 | ` * Callees with a VARIADIC tail are left alone: php collects extra named arguments into it` |
|         - | 2238 | ` * by NAME, which the positional vector here cannot express.` |
|         - | 2239 | ` */` |
|        94 | 2240 | `PH7_PRIVATE sxi32 PH7_VmBindNamedArgsToSig(` |
|         - | 2241 | `	ph7_context *pCtx,      /* Call context (for the throws) */` |
|         - | 2242 | `	ph7_user_func *pFunc,   /* Callee: its zSig names the parameters */` |
|         - | 2243 | `	VmCallArgMap *pMap,     /* Call-site map; its aNames[] are per ACTUAL slot */` |
|         - | 2244 | `	int *pnArg,             /* IN/OUT: argument count */` |
|         - | 2245 | `	ph7_value **apArg       /* IN/OUT: argument vector */` |
|         - | 2246 | `	)` |
|         2 | 2247 | `{` |
|         - | 2248 | `	/* php's own stubs top out well under this; a signature with more parameters simply` |
|         - | 2249 | `	 * keeps the positional binding it had. */` |
|         - | 2250 | `#define VM_SIG_MAX_PARAM 32` |
|         - | 2251 | `	VmSigParam aParam[VM_SIG_MAX_PARAM];` |
|         - | 2252 | `	ph7_value *apBound[VM_SIG_MAX_PARAM];` |
|         - | 2253 | `	int nParam,nArg,i,nLast;` |
|        96 | 2254 | `	if( pFunc == 0 \|\| pFunc->zSig == 0 \|\| pMap == 0 \|\| pMap->bHasNamed == 0 ){` |
|        13 | 2255 | `		return SXRET_OK;` |
|         - | 2256 | `	}` |
|        84 | 2257 | `	nArg = *pnArg;` |
|        84 | 2258 | `	if( nArg < 1 \|\| nArg > VM_SIG_MAX_PARAM ){` |
|       ! 0 | 2259 | `		return SXRET_OK;` |
|         - | 2260 | `	}` |
|        84 | 2261 | `	nParam = VmSigParams(pFunc->zSig,aParam,VM_SIG_MAX_PARAM);` |
|        84 | 2262 | `	if( nParam < 1 \|\| aParam[nParam-1].bVariadic ){` |
|        21 | 2263 | `		return SXRET_OK;` |
|         - | 2264 | `	}` |
|       252 | 2265 | `	for( i = 0 ; i < nParam ; ++i ){` |
|       190 | 2266 | `		apBound[i] = 0;` |
|        96 | 2267 | `	}` |
|        64 | 2268 | `	nLast = -1;` |
|       198 | 2269 | `	for( i = 0 ; i < nArg ; ++i ){` |
|       142 | 2270 | `		int p = i;` |
|       202 | 2271 | `		if( i < (int)pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|       128 | 2272 | `			SyString *pName = &pMap->aNames[i];` |
|       246 | 2273 | `			for( p = 0 ; p < nParam ; ++p ){` |
|       240 | 2274 | `				if( (int)pName->nByte == aParam[p].nName` |
|       199 | 2275 | `				 && SyMemcmp(pName->zString,aParam[p].zName,pName->nByte) == 0 ){` |
|       124 | 2276 | `					break;` |
|         - | 2277 | `				}` |
|        60 | 2278 | `			}` |
|       128 | 2279 | `			if( p >= nParam ){` |
|         7 | 2280 | `				return PH7_VmThrowException(pCtx,"Error",` |
|         2 | 2281 | `					"Unknown named parameter $%z",pName);` |
|         - | 2282 | `			}` |
|       124 | 2283 | `			if( apBound[p] ){` |
|         4 | 2284 | `				return PH7_VmThrowException(pCtx,"Error",` |
|         1 | 2285 | `					"Named parameter $%z overwrites previous argument",pName);` |
|         2 | 2286 | `			}` |
|        75 | 2287 | `		}else if( p >= nParam ){` |
|       ! 0 | 2288 | `			return SXRET_OK; /* more positional arguments than the signature knows */` |
|         - | 2289 | `		}` |
|       136 | 2290 | `		apBound[p] = apArg[i];` |
|       136 | 2291 | `		if( p > nLast ){` |
|       114 | 2292 | `			nLast = p;` |
|        56 | 2293 | `		}` |
|        69 | 2294 | `	}` |
|       188 | 2295 | `	for( i = 0 ; i <= nLast ; ++i ){` |
|       134 | 2296 | `		if( apBound[i] == 0 ){` |
|         7 | 2297 | `			ph7_value *pDef = ph7_context_new_scalar(pCtx);` |
|         7 | 2298 | `			if( pDef == 0 \|\| !VmSigDefaultValue(pCtx->pVm,&aParam[i],pDef) ){` |
|         - | 2299 | `				SyString sName;` |
|         3 | 2300 | `				SyStringInitFromBuf(&sName,aParam[i].zName,(sxu32)aParam[i].nName);` |
|         4 | 2301 | `				return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|         1 | 2302 | `					"%z(): Argument #%d ($%z) not passed",&pFunc->sName,i + 1,&sName);` |
|         - | 2303 | `			}` |
|         5 | 2304 | `			apBound[i] = pDef;` |
|         2 | 2305 | `		}` |
|        67 | 2306 | `	}` |
|       184 | 2307 | `	for( i = 0 ; i <= nLast ; ++i ){` |
|       130 | 2308 | `		apArg[i] = apBound[i];` |
|        66 | 2309 | `	}` |
|        56 | 2310 | `	*pnArg = nLast + 1;` |
|        56 | 2311 | `	return SXRET_OK;` |
|        49 | 2312 | `}` |
|         - | 2313 | `/*` |
|         - | 2314 | ` * Name the Nth (0-based) parameter of a declared signature, without the '$'.` |
|         - | 2315 | ` *` |
|         - | 2316 | ` * The signature string is the only place a host function's parameter names live, and` |
|         - | 2317 | `` * php puts them in diagnostics — `sort(): Argument #1 ($array) …`. Answers 0 when the`` |
|         - | 2318 | ` * signature has no such parameter (or none with a name).` |
|         - | 2319 | ` */` |
|        12 | 2320 | `PH7_PRIVATE int PH7_VmSigParamName(const char *zSig,int nPos,SyString *pOut)` |
|         2 | 2321 | `{` |
|         - | 2322 | `	VmSigParam aParam[VM_SIG_MAX_PARAM];` |
|         - | 2323 | `	int nParam;` |
|        14 | 2324 | `	if( zSig == 0 \|\| nPos < 0 \|\| nPos >= VM_SIG_MAX_PARAM ){` |
|       ! 0 | 2325 | `		return 0;` |
|         - | 2326 | `	}` |
|        14 | 2327 | `	nParam = VmSigParams(zSig,aParam,VM_SIG_MAX_PARAM);` |
|        14 | 2328 | `	if( nPos >= nParam \|\| aParam[nPos].nName < 1 ){` |
|       ! 0 | 2329 | `		return 0;` |
|         - | 2330 | `	}` |
|        14 | 2331 | `	SyStringInitFromBuf(pOut,aParam[nPos].zName,(sxu32)aParam[nPos].nName);` |
|        14 | 2332 | `	return 1;` |
|         8 | 2333 | `}` |
|         - | 2334 | `/*` |
|         - | 2335 | `` * A `&` in a builtin's signature is not always php's ZEND_SEND_ARG_BY_REF.`` |
|         - | 2336 | ` *` |
|         - | 2337 | ` * php has a second mode, ZEND_SEND_PREFER_REF: bind by reference when the argument IS a` |
|         - | 2338 | ` * variable, and otherwise take it by value without a word. Reflection prints those` |
|         - | 2339 | ` * parameters as by-reference like any other and PHL's signature string cannot say which` |
|         - | 2340 | `` * mode a `&` means, so the two are told apart here. Probed value-for-value against php`` |
|         - | 2341 | `` * 8.5 over every `&` row PHL declares (41 of them): all but extract() refuse a`` |
|         - | 2342 | `` * non-variable, and extract() answers `int(1)` for `extract(['q' => 1])`.`` |
|         - | 2343 | ` *` |
|         - | 2344 | ` * array_multisort() is listed with it because it is php's other prefer-ref builtin and` |
|         - | 2345 | ` * PHL will need this the day it gains one (it is a MISSING builtin today, §5).` |
|         - | 2346 | ` */` |
|     60594 | 2347 | `PH7_PRIVATE int VmBuiltinPrefersRef(SyString *pName)` |
|         5 | 2348 | `{` |
|         - | 2349 | `	static const char *const azPreferRef[] = { "extract", "array_multisort" };` |
|         - | 2350 | `	sxu32 i;` |
|    181591 | 2351 | `	for( i = 0 ; i < SX_ARRAYSIZE(azPreferRef) ; ++i ){` |
|    121113 | 2352 | `		sxu32 nByte = SyStrlen(azPreferRef[i]);` |
|    121108 | 2353 | `		if( pName->nByte == nByte` |
|     60676 | 2354 | `		 && SyMemcmp(pName->zString,azPreferRef[i],nByte) == 0 ){` |
|       120 | 2355 | `			return 1;` |
|         - | 2356 | `		}` |
|     60501 | 2357 | `	}` |
|     60483 | 2358 | `	return 0;` |
|     30302 | 2359 | `}` |
|         - | 2360 | `/*` |
|         - | 2361 | ` * php refuses a by-reference argument at the CALL, before the callee's ZPP runs, and it` |
|         - | 2362 | ``  * decides from the argument's SHAPE, not from its value: `sort([3,1])`, `usort('x',$cb)` `` |
|         - | 2363 | `` * and `preg_match($p,$s,'lit')` are all`` |
|         - | 2364 | `` * `Error: sort(): Argument #1 ($array) could not be passed by reference`.`` |
|         - | 2365 | ` *` |
|         - | 2366 | ` * The call site's compile-time shape mask (VmCallArgMap.nNonLvalMask) is what says so.` |
|         - | 2367 | ` * Only five builtins raised anything before this, from their own bodies, on the runtime` |
|         - | 2368 | `` * `nIdx == SXU32_HIGH` signal — which cannot tell a literal from the result of a call, a`` |
|         - | 2369 | `` * shape php ACCEPTS with a notice. The thirty other `&` rows answered `true`/`false`/an`` |
|         - | 2370 | ` * int: the same answers they give for work they really did.` |
|         - | 2371 | ` *` |
|         - | 2372 | ` * Skipped when the call site has no shape mask (a spread, an indirect dispatch through` |
|         - | 2373 | ` * call_user_func, an engine-synthesized call) or uses named arguments (which rebind` |
|         - | 2374 | ` * positions the mask is indexed by). The by-ref positions come from the same declared` |
|         - | 2375 | ` * signature everything else here reads.` |
|         - | 2376 | ` */` |
|   2970352 | 2377 | `PH7_PRIVATE sxi32 PH7_VmScreenByRefArgShapes(` |
|         - | 2378 | `	ph7_context *pCtx,     /* Call context (for the throw) */` |
|         - | 2379 | `	ph7_user_func *pFunc,  /* Callee: its zSig names and marks the parameters */` |
|         - | 2380 | `	VmCallArgMap *pMap,    /* Call-site map, or 0 */` |
|         - | 2381 | `	int nGiven,            /* Argument count */` |
|         - | 2382 | `	ph7_value **apArg      /* Arguments */` |
|         - | 2383 | `	)` |
|         5 | 2384 | `{` |
|         - | 2385 | `	VmSigParam aParam[VM_SIG_MAX_PARAM];` |
|         - | 2386 | `	int nParam,n;` |
|         - | 2387 | `	/* The by-ref mask first: it is 0 for all but 41 of the ~650 host functions, so` |
|         - | 2388 | `	 * every other call leaves through one test. */` |
|   2970357 | 2389 | `	if( pFunc == 0 \|\| pFunc->nByRefMask == 0 \|\| pFunc->zSig == 0 \|\| nGiven < 1 ){` |
|   2907711 | 2390 | `		return SXRET_OK;` |
|         - | 2391 | `	}` |
|     62651 | 2392 | `	if( pMap == 0 \|\| !pMap->bArgShapes \|\| pMap->bHasNamed ){` |
|        27 | 2393 | `		return SXRET_OK;` |
|         - | 2394 | `	}` |
|     62627 | 2395 | `	if( (pMap->nNonLvalMask \| pMap->nTempCallMask) == 0 ){` |
|      2081 | 2396 | `		return SXRET_OK;` |
|         - | 2397 | `	}` |
|     60551 | 2398 | `	if( VmBuiltinPrefersRef(&pFunc->sName) ){` |
|       116 | 2399 | `		return SXRET_OK;` |
|         - | 2400 | `	}` |
|     60439 | 2401 | `	nParam = VmSigParams(pFunc->zSig,aParam,VM_SIG_MAX_PARAM);` |
|    240677 | 2402 | `	for( n = 0 ; n < nGiven && n < 31 ; ++n ){` |
|    180293 | 2403 | `		if( (pFunc->nByRefMask & (1u << n)) == 0 ){` |
|    178853 | 2404 | `			continue;` |
|         - | 2405 | `		}` |
|      1445 | 2406 | `		if( (pMap->nNonLvalMask & (1u << n)) == 0 ){` |
|         - | 2407 | `			/* Not a refusal — but a CALL result in this position is php's notice,` |
|         - | 2408 | `			 * and then the builtin operates on the temporary. */` |
|      1395 | 2409 | `			PH7_VmArgTempCallNotice(pCtx->pVm,pMap,(sxu32)n,apArg[n]);` |
|      1395 | 2410 | `			continue;` |
|         - | 2411 | `		}` |
|        55 | 2412 | `		if( n < nParam && aParam[n].nName > 0 ){` |
|        80 | 2413 | `			return PH7_VmThrowException(pCtx,"Error",` |
|         - | 2414 | `				"%z(): Argument #%d ($%.*s) could not be passed by reference",` |
|        25 | 2415 | `				&pFunc->sName,n + 1,aParam[n].nName,aParam[n].zName);` |
|         - | 2416 | `		}` |
|       ! 0 | 2417 | `		return PH7_VmThrowException(pCtx,"Error",` |
|         - | 2418 | `			"%z(): Argument #%d could not be passed by reference",` |
|       ! 0 | 2419 | `			&pFunc->sName,n + 1);` |
|       ! 0 | 2420 | `	}` |
|     60389 | 2421 | `	return SXRET_OK;` |
|   1486000 | 2422 | `}` |
|         - | 2423 | `/*` |
|         - | 2424 | ` * D1: derive a by-reference position bitmask from a php-style signature string.` |
|         - | 2425 | `` * Bit N is set when positional parameter N is declared by-reference (a `&` appears`` |
|         - | 2426 | `` * anywhere in that comma-separated parameter, e.g. `&$matches`, `&...$vars`). Only`` |
|         - | 2427 | ` * the first 31 positions are representable; a by-ref parameter past that is rare` |
|         - | 2428 | ` * for a builtin and simply not tracked here. The scan mirrors VmDeriveArityFromSig.` |
|         - | 2429 | ` */` |
|   8541416 | 2430 | `PH7_PRIVATE sxu32 VmDeriveByRefMaskFromSig(const char *zSig)` |
|         5 | 2431 | `{` |
|   8541421 | 2432 | `	sxu32 mask = 0;` |
|   8541421 | 2433 | `	int n = 0;       /* current parameter index */` |
|   8541421 | 2434 | `	int bSeen = 0;   /* current parameter has non-space content */` |
|   8541421 | 2435 | ``	int bRef = 0;    /* current parameter carries a by-ref `&` */`` |
|   8541421 | 2436 | ``	int bVar = 0;    /* current parameter is a `...` variadic */`` |
|   8541421 | 2437 | `	int bTailRef = 0;/* the LAST parameter was a by-ref variadic */` |
|   8541421 | 2438 | `	const char *zCur = zSig;` |
|  88990225 | 2439 | `	for(;;){` |
| 182903949 | 2440 | `		if( zCur[0] == '\'' \|\| zCur[0] == '"' ){` |
|    303775 | 2441 | `			bSeen = 1;` |
|    303775 | 2442 | `			zCur = VmSigSkipQuoted(zCur);` |
|    303775 | 2443 | `			if( zCur[0] != '\0' ){` |
|    303775 | 2444 | `				zCur++;` |
|    151885 | 2445 | `			}` |
|    303775 | 2446 | `			continue;` |
|         - | 2447 | `		}` |
| 182600179 | 2448 | `		if( zCur[0] == '\0' \|\| zCur[0] == ',' ){` |
|  13161145 | 2449 | `			if( bSeen ){` |
|  10132603 | 2450 | `				if( bRef && n < 31 ){` |
|    295367 | 2451 | `					mask \|= (1u << n);` |
|    147681 | 2452 | `				}` |
|  10132603 | 2453 | `				bTailRef = (bRef && bVar);` |
|  10132603 | 2454 | `				n++;` |
|   5066299 | 2455 | `			}` |
|  13161145 | 2456 | `			if( zCur[0] == '\0' ){` |
|   8541421 | 2457 | `				break;` |
|         - | 2458 | `			}` |
|   4619729 | 2459 | `			bSeen = bRef = bVar = 0;` |
|   4619729 | 2460 | `			zCur++;` |
|   4619729 | 2461 | `			continue;` |
|         - | 2462 | `		}` |
| 169439039 | 2463 | `		if( zCur[0] != ' ' ){` |
| 148787267 | 2464 | `			bSeen = 1;` |
|  74393631 | 2465 | `		}` |
| 169439039 | 2466 | `		if( zCur[0] == '&' ){` |
|    295367 | 2467 | `			bRef = 1;` |
|    147681 | 2468 | `		}` |
| 169439039 | 2469 | `		if( zCur[0] == '.' && zCur[1] == '.' && zCur[2] == '.' ){` |
|         - | 2470 | ``			/* A `...` tail, not a numeric default's decimal point. */`` |
|    283947 | 2471 | `			bVar = 1;` |
|    141971 | 2472 | `		}` |
| 169439039 | 2473 | `		zCur++;` |
|         5 | 2474 | `	}` |
|   8541421 | 2475 | `	if( bTailRef && n > 0 && n <= 31 ){` |
|         - | 2476 | ``		/* A by-ref `&...` tail absorbs every later actual (array_multisort's`` |
|         - | 2477 | ``		 * `&...$rest`): without this, the deferred-argument resolver read the`` |
|         - | 2478 | ``		 * tail positions as by-VALUE and warned `Undefined variable` on an`` |
|         - | 2479 | `		 * undefined actual php binds silently. */` |
|      4665 | 2480 | `		mask \|= ~((1u << (n - 1)) - 1u);` |
|      2330 | 2481 | `	}` |
|   8541421 | 2482 | `	return mask;` |
|         5 | 2483 | `}` |
|      4660 | 2484 | `PH7_PRIVATE void VmSetBuiltinSignatures(ph7_vm *pVm)` |
|         5 | 2485 | `{` |
|         - | 2486 | `	sxu32 n;` |
|   3527625 | 2487 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinSig) ; n++ ){` |
|   5284445 | 2488 | `		SyHashEntry *pEntry = SyHashGet(&pVm->hHostFunction,` |
|   3522960 | 2489 | `			(const void *)aBuiltinSig[n].zName,(sxu32)SyStrlen(aBuiltinSig[n].zName));` |
|   3522965 | 2490 | `		if( pEntry ){` |
|   3476365 | 2491 | `			ph7_user_func *pFunc = (ph7_user_func *)pEntry->pUserData;` |
|   3476365 | 2492 | `			sxi16 nMin = 0, nMax = 0;` |
|   3476365 | 2493 | `			sxu8 bAtLeast = 0, bHasMax = 0;` |
|   3476365 | 2494 | `			pFunc->zSig = aBuiltinSig[n].zSig;` |
|   3476365 | 2495 | `			pFunc->zRet = aBuiltinSig[n].zRet[0] ? aBuiltinSig[n].zRet : 0;` |
|   3476365 | 2496 | `			pFunc->nByRefMask = VmDeriveByRefMaskFromSig(pFunc->zSig);` |
|   3476365 | 2497 | `			VmDeriveArityFromSig(pFunc->zSig,&nMin,&bAtLeast,&nMax,&bHasMax);` |
|         - | 2498 | `			/* The MAXIMUM always comes from the signature: the curated override` |
|         - | 2499 | `			 * table speaks only to the minimum (and its wording). */` |
|   3476365 | 2500 | `			pFunc->nMaxArg = nMax;` |
|   3476365 | 2501 | `			pFunc->bHasMaxArg = (sxu8)(VmBuiltinSelfValidatesArity(aBuiltinSig[n].zName) ? 0 : bHasMax);` |
|   3476365 | 2502 | `			if( pFunc->nMinArg < 1 ){` |
|         - | 2503 | `				/* VmSetBuiltinArity() ran first: a non-zero minimum here means the` |
|         - | 2504 | `				 * curated override already spoke for this builtin, so leave it. */` |
|   2134285 | 2505 | `				pFunc->nMinArg = nMin;` |
|   2134285 | 2506 | `				pFunc->bAtLeast = bAtLeast;` |
|   1067140 | 2507 | `			}` |
|   1738180 | 2508 | `		}` |
|   1761485 | 2509 | `	}` |
|      4665 | 2510 | `}` |
|         - | 2511 | `/*` |
|         - | 2512 | ` * Signature lookup by function name, for the reflection layer: embedded-PHP` |
|         - | 2513 | ` * builtins (max/min/Exception methods...) are ph7_vm_func instances, not` |
|         - | 2514 | ` * host functions, so they miss the VmSetBuiltinSignatures stamping and pull` |
|         - | 2515 | ` * their row on demand here. Linear scan — reflection-path only.` |
|         - | 2516 | ` */` |
|       ! 0 | 2517 | `PH7_PRIVATE const char * PH7_VmBuiltinSigLookup(const char *zName,sxu32 nLen,const char **pzRet)` |
|       ! 0 | 2518 | `{` |
|         - | 2519 | `	sxu32 n;` |
|       ! 0 | 2520 | `	if( pzRet ){` |
|       ! 0 | 2521 | `		*pzRet = 0;` |
|       ! 0 | 2522 | `	}` |
|       ! 0 | 2523 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinSig) ; n++ ){` |
|       ! 0 | 2524 | `		if( SyStrlen(aBuiltinSig[n].zName) == nLen` |
|       ! 0 | 2525 | `		 && SyMemcmp(aBuiltinSig[n].zName,zName,nLen) == 0 ){` |
|       ! 0 | 2526 | `			if( pzRet && aBuiltinSig[n].zRet[0] ){` |
|       ! 0 | 2527 | `				*pzRet = aBuiltinSig[n].zRet;` |
|       ! 0 | 2528 | `			}` |
|       ! 0 | 2529 | `			return aBuiltinSig[n].zSig;` |
|         - | 2530 | `		}` |
|       ! 0 | 2531 | `	}` |
|       ! 0 | 2532 | `	return 0;` |
|       ! 0 | 2533 | `}` |
|         - | 2534 | `/*` |
|         - | 2535 | ` * Write a value back to the caller's variable through a builtin argument's` |
|         - | 2536 | ` * stack-slot nIdx — the shared write-back for builtin by-reference` |
|         - | 2537 | ` * out-parameters (preg_match $matches, preg_replace &$count, similar_text` |
|         - | 2538 | ` * &$percent, ...).` |
|         - | 2539 | ` *` |
|         - | 2540 | ` * For a positional out-param argument the call compiler auto-vivifies known` |
|         - | 2541 | ` * by-reference out-params (see GenStateByRefBuiltinMask in compile.c), so a` |
|         - | 2542 | ` * bare undefined variable, an array subscript, and a declared/untyped` |
|         - | 2543 | ` * property all arrive with a real nIdx and are written back here, matching` |
|         - | 2544 | ` * PHP's reference semantics.` |
|         - | 2545 | ` *` |
|         - | 2546 | ` * nIdx stays SXU32_HIGH and the write-back to the caller is skipped (the` |
|         - | 2547 | ` * value still lands in the local stack slot) when the argument cannot expose` |
|         - | 2548 | ` * a stable memobj slot: a literal, a function-call result, a subscript of a` |
|         - | 2549 | ` * non-lvalue parent (foo()['k']), or any variable in a call that also uses` |
|         - | 2550 | ` * named or spread arguments (compile-time positions no longer map to the` |
|         - | 2551 | ` * runtime arg slots, so the compiler conservatively does not vivify). An` |
|         - | 2552 | ` * uninitialized typed property is also not wired (it throws before the` |
|         - | 2553 | ` * write) -- see the recorded deferrals.` |
|         - | 2554 | ` */` |
|       924 | 2555 | `PH7_PRIVATE void PH7_VmStoreArgByRef(ph7_vm *pVm,ph7_value *pArg,ph7_value *pNewVal)` |
|         5 | 2556 | `{` |
|       929 | 2557 | `	if( pArg->nIdx != SXU32_HIGH ){` |
|       879 | 2558 | `		ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pArg->nIdx);` |
|       879 | 2559 | `		if( pObj ){` |
|       879 | 2560 | `			PH7_MemObjStore(pNewVal,pObj);` |
|       437 | 2561 | `		}` |
|       437 | 2562 | `	}` |
|       929 | 2563 | `	PH7_MemObjStore(pNewVal,pArg);` |
|       929 | 2564 | `}` |
|         - | 2565 | `/*` |
|         - | 2566 | ` * Raise an E_WARNING whose text is used VERBATIM.` |
|         - | 2567 | ` * ph7_context_throw_error_format() prepends "func(): " to whatever it is given, but php's` |
|         - | 2568 | ` * IO warnings put the offending path inside those parens -- "file_get_contents(/nope):` |
|         - | 2569 | ` * Failed to open stream: ..." -- so a caller that needs php's exact shape must build the` |
|         - | 2570 | ` * whole line itself and come through here.` |
|         - | 2571 | ` */` |
|         - | 2572 | `/*` |
|         - | 2573 | ` * Validate a callback argument and throw php's TypeError when it cannot be called.` |
|         - | 2574 | ` *` |
|         - | 2575 | ` * The shared dispatcher (PH7_VmCallUserFunctionWithMap) answers SXRET_OK with a NULL` |
|         - | 2576 | ` * result for an unresolvable callable, so every caller that did not check first failed` |
|         - | 2577 | ` * SILENTLY: call_user_func() returned NULL and usort() left the array sorted by nothing` |
|         - | 2578 | ` * at all. php rejects the ARGUMENT up front, naming exactly why.` |
|         - | 2579 | ` */` |
|      8602 | 2580 | `PH7_PRIVATE sxi32 PH7_CheckCallbackArg(` |
|         - | 2581 | `	ph7_context *pCtx,   /* Calling context (names the function in the message) */` |
|         - | 2582 | `	ph7_value *pCb,      /* The callback argument */` |
|         - | 2583 | `	int iArg,            /* Its 1-based position */` |
|         - | 2584 | `	const char *zParam,  /* Its php parameter name ("callback"), or 0 for the variadic` |
|         - | 2585 | `	                      * comparators php names by position only (array_udiff …) */` |
|         - | 2586 | `	int bNullable        /* TRUE when php's text says "or null" */` |
|         - | 2587 | `	)` |
|         5 | 2588 | `{` |
|         - | 2589 | `	char zReason[256];` |
|      8607 | 2590 | `	const char *zWhy = PH7_VmCallableReason(pCtx->pVm,pCb,zReason,sizeof(zReason));` |
|      8607 | 2591 | `	if( zWhy == 0 ){` |
|      8381 | 2592 | `		return PH7_OK;` |
|         - | 2593 | `	}` |
|       231 | 2594 | `	if( zParam ){` |
|       263 | 2595 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 2596 | `			"%s(): Argument #%d ($%s) must be a valid callback%s, %s",` |
|        86 | 2597 | `			ph7_function_name(pCtx),iArg,zParam,bNullable ? " or null" : "",zWhy);` |
|         - | 2598 | `	}` |
|        86 | 2599 | `	return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 2600 | `		"%s(): Argument #%d must be a valid callback%s, %s",` |
|        27 | 2601 | `		ph7_function_name(pCtx),iArg,bNullable ? " or null" : "",zWhy);` |
|      4306 | 2602 | `}` |
|     26174 | 2603 | `PH7_PRIVATE void PH7_VmThrowWarningFmt(ph7_vm *pVm,const char *zFmt,...)` |
|         5 | 2604 | `{` |
|         - | 2605 | `	va_list ap;` |
|     26179 | 2606 | `	va_start(ap,zFmt);` |
|     26179 | 2607 | `	PH7_VmThrowErrorAp(pVm,0,PH7_CTX_WARNING,zFmt,ap);` |
|     26179 | 2608 | `	va_end(ap);` |
|     26179 | 2609 | `}` |
|         - | 2610 | `/*` |
|         - | 2611 | ` * Emit a formatted E_USER_DEPRECATED diagnostic with no function-name prefix:` |
|         - | 2612 | ` * php reports #[\Deprecated] as USER-deprecated (16384, it is userland-authored),` |
|         - | 2613 | ` * not the engine's E_DEPRECATED (8192) — handler-visible errno matters.` |
|         - | 2614 | ` */` |
|        34 | 2615 | `static void VmThrowUserDeprecatedFmt(ph7_vm *pVm,const char *zFmt,...)` |
|         1 | 2616 | `{` |
|         - | 2617 | `	va_list ap;` |
|        35 | 2618 | `	va_start(ap,zFmt);` |
|        35 | 2619 | `	PH7_VmThrowErrorAp(pVm,0,E_USER_DEPRECATED,zFmt,ap);` |
|        35 | 2620 | `	va_end(ap);` |
|        35 | 2621 | `}` |
|         - | 2622 | `/*` |
|         - | 2623 | ` * php 8.4 #[\Deprecated]: emit the E_USER_DEPRECATED notice for a call to a user` |
|         - | 2624 | ` * function/method carrying the attribute. Message shapes (php-exact):` |
|         - | 2625 | ` *   Function f() is deprecated` |
|         - | 2626 | ` *   Method C::m() is deprecated since 2.0, use g() instead` |
|         - | 2627 | ` * ("since" from the attribute's since: argument; the trailing ", msg" from` |
|         - | 2628 | ` * message:/positional #1.) The attribute arguments are compiled constant` |
|         - | 2629 | ` * expressions — evaluated via VmLocalExec, the reflection thunks' pattern.` |
|         - | 2630 | ` * Called from OP_CALL once per call, only when the callee HAS attributes;` |
|         - | 2631 | ` * pDeclClass names the method's declaring class (0 for plain functions).` |
|         - | 2632 | ` */` |
|         - | 2633 | `/*` |
|         - | 2634 | ` * Expand a global constant's value, emitting the php 8.5 #[\Deprecated]` |
|         - | 2635 | ` * E_USER_DEPRECATED notice first when the constant carries attributes` |
|         - | 2636 | `` * (`Constant GD is deprecated since 1.2` — every access re-warns).`` |
|         - | 2637 | ` */` |
|         - | 2638 | `static void VmDeprecatedAttrNoticeSubject(ph7_vm *pVm,SySet *pAttrs,` |
|         - | 2639 | `	const char *zKind,const SyString *pQual,const SyString *pName);` |
|         - | 2640 | `/*` |
|         - | 2641 | ` * Expand a constant, emitting the php 8.5 #[\Deprecated] E_USER_DEPRECATED notice` |
|         - | 2642 | `` * first when a USERLAND constant carries the attribute (`Constant GD is deprecated`` |
|         - | 2643 | `` * since 1.2` — every access re-warns). Engine constants php merely deprecates are`` |
|         - | 2644 | ` * not mimicked: PHL removes them outright (see the scope policy), so there is no` |
|         - | 2645 | ` * engine-side E_DEPRECATED list here.` |
|         - | 2646 | ` *` |
|         - | 2647 | `` * A `const NAME = <expr>;` statement compiles its initializer to a bytecode`` |
|         - | 2648 | ` * program and PH7_VmExpandConstantValue RUNS it — so the value was re-computed` |
|         - | 2649 | ` * on EVERY read. For anything with an identity or a side effect that is a wrong` |
|         - | 2650 | `` * answer, not a slow one: `const C = new Foo();` gave a DIFFERENT object each`` |
|         - | 2651 | `` * time (`C === C` was false, and `Foo::$count` counted one construction per`` |
|         - | 2652 | ` * read) where php evaluates the initializer once and hands the same value out` |
|         - | 2653 | ` * for ever. The first successful expansion is kept, and the constant becomes an` |
|         - | 2654 | ` * ordinary value-backed one — exactly the shape define() registers, so` |
|         - | 2655 | ` * redefinition frees it through the path that already existed.` |
|         - | 2656 | ` *` |
|         - | 2657 | ` * Not cached when the initializer did not complete: a throw, an exit(), or a` |
|         - | 2658 | ` * MUTED evaluation (php has not reached this code, so nothing may be observable)` |
|         - | 2659 | ` * must all be retried rather than frozen into a half-built value.` |
|         - | 2660 | ` */` |
|    217766 | 2661 | `PH7_PRIVATE sxi32 VmExpandConstantOnce(ph7_vm *pVm,ph7_constant *pCons,ph7_value *pOut)` |
|         5 | 2662 | `{` |
|         - | 2663 | `	const void *pResumeBefore,*pInlineBefore;` |
|         - | 2664 | `	sxi32 rc;` |
|    217771 | 2665 | `	if( pCons->xExpand != PH7_VmExpandConstantValue ){` |
|    217609 | 2666 | `		pCons->xExpand(pOut,pCons->pUserData);` |
|    217609 | 2667 | `		return SXRET_OK;` |
|         - | 2668 | `	}` |
|         - | 2669 | `	/* The initializer's own status. PH7_VmExpandConstantValue drops VmLocalExec's` |
|         - | 2670 | `	 * return code (ProcConstant answers void), so the program is driven from here` |
|         - | 2671 | `	 * instead — a caller with no way to see a throw would otherwise cache a` |
|         - | 2672 | `	 * half-built value and keep running past it. */` |
|       167 | 2673 | `	pResumeBefore = (const void *)pVm->pResumeFrame;` |
|       167 | 2674 | `	pInlineBefore = (const void *)pVm->pInlineInstr;` |
|       167 | 2675 | `	rc = VmLocalExec(pVm,(SySet *)pCons->pUserData,pOut,FALSE);` |
|       162 | 2676 | `	if( pVm->nMuteThrow > 0 \|\| rc == PH7_ABORT` |
|       141 | 2677 | `	 \|\| VmLocalExecThrew(pVm,rc,pResumeBefore,pInlineBefore) ){` |
|         - | 2678 | `		/* Did not complete — a throw, an exit(), or a MUTED evaluation (php has` |
|         - | 2679 | `		 * not reached this code, so nothing may be observable). Retry it next` |
|         - | 2680 | `		 * time rather than freezing a value the initializer never produced:` |
|         - | 2681 | ``		 * `const A = LATER; …; define('LATER',5);` must still answer 5. */`` |
|        34 | 2682 | `		return rc == SXRET_OK ? PH7_EXCEPTION : rc;` |
|         - | 2683 | `	}` |
|         - | 2684 | `	{` |
|       135 | 2685 | `		ph7_value *pKeep = (ph7_value *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|       135 | 2686 | `		if( pKeep == 0 ){` |
|       ! 0 | 2687 | `			return SXRET_OK; /* out of memory: stay lazy rather than fail the read */` |
|         - | 2688 | `		}` |
|       135 | 2689 | `		PH7_MemObjInit(pVm,pKeep);` |
|       135 | 2690 | `		PH7_MemObjStore(pOut,pKeep);` |
|       135 | 2691 | `		pCons->xExpand = VmExpandUserConstant;` |
|       135 | 2692 | `		pCons->pUserData = pKeep;` |
|         - | 2693 | `	}` |
|       135 | 2694 | `	return SXRET_OK;` |
|    108885 | 2695 | `}` |
|    135796 | 2696 | `PH7_PRIVATE void VmExpandConstantWithNotice(ph7_vm *pVm,ph7_constant *pCons,ph7_value *pOut)` |
|         5 | 2697 | `{` |
|    135801 | 2698 | `	if( SySetUsed(&pCons->aAttrs) > 0 ){` |
|         7 | 2699 | `		VmDeprecatedAttrNoticeSubject(pVm,&pCons->aAttrs,"Constant",0,&pCons->sName);` |
|         3 | 2700 | `	}` |
|    135801 | 2701 | `	VmExpandConstantOnce(pVm,pCons,pOut);` |
|    135801 | 2702 | `}` |
|         - | 2703 | `/*` |
|         - | 2704 | ` * Query a GLOBAL constant by its exact (case-sensitive) name and expand its` |
|         - | 2705 | ` * value into pOut, which the caller has initialized. Returns 1 when the` |
|         - | 2706 | ` * constant exists. The ini scanner's NORMAL/TYPED value interpretation is the` |
|         - | 2707 | ` * caller: php substitutes a defined constant's value for a bare identifier` |
|         - | 2708 | ` * token inside an unquoted ini value.` |
|         - | 2709 | ` */` |
|        44 | 2710 | `PH7_PRIVATE int PH7_VmQueryConstant(ph7_vm *pVm,const char *zName,sxu32 nName,ph7_value *pOut)` |
|         1 | 2711 | `{` |
|         - | 2712 | `	SyHashEntry *pEntry;` |
|         - | 2713 | `	ph7_constant *pCons;` |
|        45 | 2714 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)zName,nName);` |
|        45 | 2715 | `	if( pEntry == 0 ){` |
|        41 | 2716 | `		return 0;` |
|         - | 2717 | `	}` |
|         5 | 2718 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|         5 | 2719 | `	VmExpandConstantWithNotice(pVm,pCons,pOut);` |
|         5 | 2720 | `	return 1;` |
|        23 | 2721 | `}` |
|         - | 2722 | `/*` |
|         - | 2723 | ` * Scan a declared-attribute set for #[\Deprecated]; when found, evaluate its` |
|         - | 2724 | ` * message:/since: arguments (positional #0 = message, #1 = since) into the` |
|         - | 2725 | ` * caller's values and return TRUE. The base subject text ("Function f()",` |
|         - | 2726 | ` * "Constant C::K") is the caller's business.` |
|         - | 2727 | ` */` |
|       204 | 2728 | `static int VmDeprecatedAttrExtract(ph7_vm *pVm,SySet *pAttrs,` |
|         - | 2729 | `	ph7_value *pMsg,int *pbMsg,ph7_value *pSince,int *pbSince)` |
|         5 | 2730 | `{` |
|       209 | 2731 | `	ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|         - | 2732 | `	sxu32 n;` |
|       209 | 2733 | `	*pbMsg = *pbSince = 0;` |
|       383 | 2734 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; ++n ){` |
|       213 | 2735 | `		ph7_attribute *pAttr = &aAttr[n];` |
|         - | 2736 | `		ph7_attr_arg *aArg;` |
|       213 | 2737 | `		sxu32 i,nPos = 0;` |
|       208 | 2738 | `		if( SyStringLength(&pAttr->sName) != sizeof("Deprecated")-1` |
|       126 | 2739 | `		 \|\| SyStrnicmp(SyStringData(&pAttr->sName),"Deprecated",sizeof("Deprecated")-1) != 0 ){` |
|       179 | 2740 | `			continue;` |
|         - | 2741 | `		}` |
|        35 | 2742 | `		aArg = (ph7_attr_arg *)SySetBasePtr(&pAttr->aArgs);` |
|        53 | 2743 | `		for( i = 0 ; i < SySetUsed(&pAttr->aArgs) ; ++i ){` |
|        19 | 2744 | `			ph7_attr_arg *pArg = &aArg[i];` |
|        19 | 2745 | `			int isMsg = 0,isSince = 0;` |
|        19 | 2746 | `			if( SyStringLength(&pArg->sName) == 0 ){` |
|         3 | 2747 | `				isMsg = (nPos == 0);` |
|         3 | 2748 | `				isSince = (nPos == 1);` |
|         3 | 2749 | `				nPos++;` |
|        18 | 2750 | `			}else if( SyStringLength(&pArg->sName) == sizeof("message")-1` |
|        12 | 2751 | `			 && SyMemcmp(SyStringData(&pArg->sName),"message",sizeof("message")-1) == 0 ){` |
|         7 | 2752 | `				isMsg = 1;` |
|        14 | 2753 | `			}else if( SyStringLength(&pArg->sName) == sizeof("since")-1` |
|        11 | 2754 | `			 && SyMemcmp(SyStringData(&pArg->sName),"since",sizeof("since")-1) == 0 ){` |
|        11 | 2755 | `				isSince = 1;` |
|         5 | 2756 | `			}` |
|        19 | 2757 | `			if( isMsg && !*pbMsg && SySetUsed(&pArg->aByteCode) > 0 ){` |
|        13 | 2758 | `				if( VmLocalExec(pVm,&pArg->aByteCode,pMsg,FALSE) == SXRET_OK ){` |
|         9 | 2759 | `					if( (pMsg->iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2760 | `						PH7_MemObjToString(pMsg);` |
|       ! 0 | 2761 | `					}` |
|         9 | 2762 | `					*pbMsg = 1;` |
|         5 | 2763 | `				}` |
|        15 | 2764 | `			}else if( isSince && !*pbSince && SySetUsed(&pArg->aByteCode) > 0 ){` |
|        11 | 2765 | `				if( VmLocalExec(pVm,&pArg->aByteCode,pSince,FALSE) == SXRET_OK ){` |
|        11 | 2766 | `					if( (pSince->iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2767 | `						PH7_MemObjToString(pSince);` |
|       ! 0 | 2768 | `					}` |
|        11 | 2769 | `					*pbSince = 1;` |
|         5 | 2770 | `				}` |
|         5 | 2771 | `			}` |
|        10 | 2772 | `		}` |
|        35 | 2773 | `		return 1;` |
|       ! 0 | 2774 | `	}` |
|       175 | 2775 | `	return 0;` |
|       107 | 2776 | `}` |
|         - | 2777 | `/*` |
|         - | 2778 | ` * Append php's " since X" / ", message" suffixes to a built base subject and` |
|         - | 2779 | ` * emit the E_USER_DEPRECATED notice.` |
|         - | 2780 | ` */` |
|        34 | 2781 | `static void VmDeprecatedEmit(ph7_vm *pVm,SyBlob *pOut,` |
|         - | 2782 | `	ph7_value *pMsg,int bMsg,ph7_value *pSince,int bSince)` |
|         1 | 2783 | `{` |
|        35 | 2784 | `	if( bSince && SyBlobLength(&pSince->sBlob) > 0 ){` |
|        16 | 2785 | `		SyBlobFormat(pOut," since %.*s",(int)SyBlobLength(&pSince->sBlob),` |
|        10 | 2786 | `			(const char *)SyBlobData(&pSince->sBlob));` |
|         5 | 2787 | `	}` |
|        35 | 2788 | `	if( bMsg && SyBlobLength(&pMsg->sBlob) > 0 ){` |
|        13 | 2789 | `		SyBlobFormat(pOut,", %.*s",(int)SyBlobLength(&pMsg->sBlob),` |
|         8 | 2790 | `			(const char *)SyBlobData(&pMsg->sBlob));` |
|         4 | 2791 | `	}` |
|        35 | 2792 | `	VmThrowUserDeprecatedFmt(pVm,"%.*s",(int)SyBlobLength(pOut),(const char *)SyBlobData(pOut));` |
|        35 | 2793 | `}` |
|         - | 2794 | `/*` |
|         - | 2795 | ` * Generic #[\Deprecated] notice for a named subject:` |
|         - | 2796 | ` * "<Kind> [Qual::]Name is deprecated[ since X][, message]".` |
|         - | 2797 | ` */` |
|        18 | 2798 | `static void VmDeprecatedAttrNoticeSubject(ph7_vm *pVm,SySet *pAttrs,` |
|         - | 2799 | `	const char *zKind,const SyString *pQual,const SyString *pName)` |
|         1 | 2800 | `{` |
|         - | 2801 | `	ph7_value sMsg,sSince;` |
|         - | 2802 | `	SyBlob sOut;` |
|         - | 2803 | `	int bMsg,bSince;` |
|        19 | 2804 | `	PH7_MemObjInit(pVm,&sMsg);` |
|        19 | 2805 | `	PH7_MemObjInit(pVm,&sSince);` |
|        19 | 2806 | `	if( VmDeprecatedAttrExtract(pVm,pAttrs,&sMsg,&bMsg,&sSince,&bSince) ){` |
|        15 | 2807 | `		SyBlobInit(&sOut,&pVm->sAllocator);` |
|        15 | 2808 | `		if( pQual ){` |
|        11 | 2809 | `			SyBlobFormat(&sOut,"%s %z::%z is deprecated",zKind,pQual,pName);` |
|         6 | 2810 | `		}else{` |
|         5 | 2811 | `			SyBlobFormat(&sOut,"%s %z is deprecated",zKind,pName);` |
|         - | 2812 | `		}` |
|        15 | 2813 | `		VmDeprecatedEmit(pVm,&sOut,&sMsg,bMsg,&sSince,bSince);` |
|        15 | 2814 | `		SyBlobRelease(&sOut);` |
|         7 | 2815 | `	}` |
|        19 | 2816 | `	PH7_MemObjRelease(&sMsg);` |
|        19 | 2817 | `	PH7_MemObjRelease(&sSince);` |
|        19 | 2818 | `}` |
|       186 | 2819 | `PH7_PRIVATE void VmDeprecatedAttrNotice(ph7_vm *pVm,ph7_vm_func *pFunc,ph7_class *pDeclClass)` |
|         5 | 2820 | `{` |
|         - | 2821 | `	ph7_value sMsg,sSince;` |
|         - | 2822 | `	SyBlob sOut;` |
|         - | 2823 | `	int bMsg,bSince;` |
|       191 | 2824 | `	PH7_MemObjInit(pVm,&sMsg);` |
|       191 | 2825 | `	PH7_MemObjInit(pVm,&sSince);` |
|       191 | 2826 | `	if( VmDeprecatedAttrExtract(pVm,&pFunc->aAttrs,&sMsg,&bMsg,&sSince,&bSince) ){` |
|        21 | 2827 | `		SyBlobInit(&sOut,&pVm->sAllocator);` |
|        21 | 2828 | `		if( pDeclClass ){` |
|         5 | 2829 | `			SyBlobFormat(&sOut,"Method %z::%z() is deprecated",&pDeclClass->sName,&pFunc->sName);` |
|         3 | 2830 | `		}else{` |
|        17 | 2831 | `			SyBlobFormat(&sOut,"Function %z() is deprecated",&pFunc->sName);` |
|         - | 2832 | `		}` |
|        21 | 2833 | `		VmDeprecatedEmit(pVm,&sOut,&sMsg,bMsg,&sSince,bSince);` |
|        21 | 2834 | `		SyBlobRelease(&sOut);` |
|        10 | 2835 | `	}` |
|       191 | 2836 | `	PH7_MemObjRelease(&sMsg);` |
|       191 | 2837 | `	PH7_MemObjRelease(&sSince);` |
|       191 | 2838 | `}` |
|         - | 2839 | `/*` |
|         - | 2840 | ` * Same notice for a #[\Deprecated] class constant, at its static-access site:` |
|         - | 2841 | `` * php's `Constant C::K is deprecated` (each access re-warns).`` |
|         - | 2842 | ` */` |
|        12 | 2843 | `PH7_PRIVATE void VmDeprecatedConstNotice(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pMember)` |
|         1 | 2844 | `{` |
|        19 | 2845 | `	VmDeprecatedAttrNoticeSubject(pVm,&pMember->aAttrs,` |
|        12 | 2846 | `		(pMember->iFlags & PH7_CLASS_ATTR_ENUMCASE) ? "Enum case" : "Constant",` |
|        12 | 2847 | `		&pClass->sName,&pMember->sName);` |
|        13 | 2848 | `}` |
|         - | 2849 | `/*` |
|         - | 2850 | `` * The USER-VISIBLE string coercion a builtin performs on a `mixed` argument or`` |
|         - | 2851 | ` * array element: the builtin-side twin of PH7_MemObjToStringUV's opcode sites.` |
|         - | 2852 | ` * An ARRAY warns "Array to string conversion" and still renders as "Array"; an` |
|         - | 2853 | ` * object whose class has no __toString() -- or one whose __toString() threw --` |
|         - | 2854 | ` * is php's catchable "could not be converted to string" Error, and the builtin` |
|         - | 2855 | ` * must answer that instead of a value.` |
|         - | 2856 | ` *` |
|         - | 2857 | ` * On success pzData and pnLen receive the NUL-terminated bytes (both optional).` |
|         - | 2858 | ` * On a throw they are set to the empty string and the status is returned AND` |
|         - | 2859 | ` * recorded on the call context, so OP_CALL cannot mistake the call for a normal` |
|         - | 2860 | ` * return; a builtin that has already produced output (printf) still keeps it,` |
|         - | 2861 | ` * which is what php does.` |
|         - | 2862 | ` *` |
|         - | 2863 | ` * The public ph7_value_to_string() stays SILENT on purpose: it is the embedder` |
|         - | 2864 | ` * API, and the INTERNAL coercions behind it (array-key canonicalisation, sort` |
|         - | 2865 | ` * comparisons, print_r/var_export/serialize) must not throw -- php's do not` |
|         - | 2866 | ` * either.` |
|         - | 2867 | ` */` |
|    313036 | 2868 | `PH7_PRIVATE sxi32 PH7_ValueToStringUV(ph7_context *pCtx,ph7_value *pValue,const char **pzData,int *pnLen)` |
|         5 | 2869 | `{` |
|    313041 | 2870 | `	sxi32 rc = PH7_MemObjToStringUV(pValue);` |
|    313041 | 2871 | `	if( rc != SXRET_OK ){` |
|        41 | 2872 | `		if( pCtx ){` |
|        41 | 2873 | `			pCtx->nThrowRc = rc;` |
|        19 | 2874 | `		}` |
|        41 | 2875 | `		if( pzData ){` |
|        37 | 2876 | `			*pzData = "";` |
|        17 | 2877 | `		}` |
|        41 | 2878 | `		if( pnLen ){` |
|        37 | 2879 | `			*pnLen = 0;` |
|        17 | 2880 | `		}` |
|        41 | 2881 | `		return rc;` |
|         - | 2882 | `	}` |
|    313003 | 2883 | `	if( pzData \|\| pnLen ){` |
|    312975 | 2884 | `		const char *zData = ph7_value_to_string(pValue,pnLen);` |
|    312975 | 2885 | `		if( pzData ){` |
|    312975 | 2886 | `			*pzData = zData;` |
|    156482 | 2887 | `		}` |
|    156482 | 2888 | `	}` |
|    313003 | 2889 | `	return SXRET_OK;` |
|    156520 | 2890 | `}` |
|         - | 2891 |  |
