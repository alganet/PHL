# src/ph7/vm_arg_check.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 916/979 lines (93.56%)

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
|      4962 |  350 | `PH7_PRIVATE void VmSetBuiltinArity(ph7_vm *pVm)` |
|         5 |  351 | `{` |
|         - |  352 | `	sxu32 n;` |
|   1434023 |  353 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinArity) ; ++n ){` |
|   1429061 |  354 | `		const struct VmBuiltinArity *p = &aBuiltinArity[n];` |
|   2858117 |  355 | `		SyHashEntry *pEntry = SyHashGet(&pVm->hHostFunction,` |
|   1429056 |  356 | `			(const void *)p->zName,SyStrlen(p->zName));` |
|   1429061 |  357 | `		if( pEntry ){` |
|   1429061 |  358 | `			ph7_user_func *pFunc = (ph7_user_func *)pEntry->pUserData;` |
|   1429061 |  359 | `			pFunc->nMinArg  = p->nMin;` |
|   1429061 |  360 | `			pFunc->bAtLeast = p->bAtLeast;` |
|    714528 |  361 | `		}` |
|    714533 |  362 | `	}` |
|      4967 |  363 | `}` |
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
|         - |  391 | `	{ "curl_file_create", "string $filename, ?string $mime_type = null, ?string $posted_filename = null", "CURLFile" },` |
|         - |  392 | `	{ "curl_getinfo", "CurlHandle $handle, ?int $option = null", "mixed" },` |
|         - |  393 | `	{ "curl_init", "?string $url = null", "CurlHandle\|false" },` |
|         - |  394 | `	{ "curl_multi_add_handle", "CurlMultiHandle $multi_handle, CurlHandle $handle", "int" },` |
|         - |  395 | `	{ "curl_multi_close", "CurlMultiHandle $multi_handle", "void" },` |
|         - |  396 | `	{ "curl_multi_errno", "CurlMultiHandle $multi_handle", "int" },` |
|         - |  397 | `	{ "curl_multi_exec", "CurlMultiHandle $multi_handle, &$still_running", "int" },` |
|         - |  398 | `	{ "curl_multi_get_handles", "CurlMultiHandle $multi_handle", "array" },` |
|         - |  399 | `	{ "curl_multi_getcontent", "CurlHandle $handle", "?string" },` |
|         - |  400 | `	{ "curl_multi_info_read", "CurlMultiHandle $multi_handle, &$queued_messages = NULL", "array\|false" },` |
|         - |  401 | `	{ "curl_multi_init", "", "CurlMultiHandle" },` |
|         - |  402 | `	{ "curl_multi_remove_handle", "CurlMultiHandle $multi_handle, CurlHandle $handle", "int" },` |
|         - |  403 | `	{ "curl_multi_select", "CurlMultiHandle $multi_handle, float $timeout = 1.0", "int" },` |
|         - |  404 | `	{ "curl_multi_setopt", "CurlMultiHandle $multi_handle, int $option, mixed $value", "bool" },` |
|         - |  405 | `	{ "curl_multi_strerror", "int $error_code", "?string" },` |
|         - |  406 | `	{ "curl_pause", "CurlHandle $handle, int $flags", "int" },` |
|         - |  407 | `	{ "curl_reset", "CurlHandle $handle", "void" },` |
|         - |  408 | `	{ "curl_setopt", "CurlHandle $handle, int $option, mixed $value", "bool" },` |
|         - |  409 | `	{ "curl_setopt_array", "CurlHandle $handle, array $options", "bool" },` |
|         - |  410 | `	{ "curl_unescape", "CurlHandle $handle, string $string", "string\|false" },` |
|         - |  411 | `	{ "curl_upkeep", "CurlHandle $handle", "bool" },` |
|         - |  412 | `	{ "curl_share_close", "CurlShareHandle $share_handle", "void" },` |
|         - |  413 | `	{ "curl_share_errno", "CurlShareHandle $share_handle", "int" },` |
|         - |  414 | `	{ "curl_share_init", "", "CurlShareHandle" },` |
|         - |  415 | `	{ "curl_share_init_persistent", "array $share_options", "CurlSharePersistentHandle" },` |
|         - |  416 | `	{ "curl_share_setopt", "CurlShareHandle $share_handle, int $option, mixed $value", "bool" },` |
|         - |  417 | `	{ "curl_share_strerror", "int $error_code", "?string" },` |
|         - |  418 | `	{ "curl_strerror", "int $error_code", "?string" },` |
|         - |  419 | `	{ "curl_version", "", "array\|false" },` |
|         - |  420 | `	{ "get_cfg_var", "string $option", "array\|string\|false" },` |
|         - |  421 | `	{ "ini_get", "string $option", "string\|false" },` |
|         - |  422 | `	{ "ini_get_all", "?string $extension = null, bool $details = true", "array\|false" },` |
|         - |  423 | `	{ "ini_restore", "string $option", "void" },` |
|         - |  424 | `	{ "ini_set", "string $option, string\|int\|float\|bool\|null $value", "string\|false" },` |
|         - |  425 | `	{ "libxml_clear_errors", "", "void" },` |
|         - |  426 | `	{ "libxml_get_errors", "", "array" },` |
|         - |  427 | `	{ "libxml_get_external_entity_loader", "", "?callable" },` |
|         - |  428 | `	{ "libxml_get_last_error", "", "LibXMLError\|false" },` |
|         - |  429 | `	{ "libxml_set_external_entity_loader", "?callable $resolver_function", "true" },` |
|         - |  430 | `	{ "libxml_set_streams_context", "$context", "void" },` |
|         - |  431 | `	{ "libxml_use_internal_errors", "?bool $use_errors = null", "bool" },` |
|         - |  432 | `	/* ext/pdo's one function: the procedural spelling of` |
|         - |  433 | `	 * PDO::getAvailableDrivers(). */` |
|         - |  434 | `	{ "pdo_drivers", "", "array" },` |
|         - |  435 | `	{ "xml_error_string", "int $error_code", "?string" },` |
|         - |  436 | `	{ "xml_get_current_byte_index", "XMLParser $parser", "int" },` |
|         - |  437 | `	{ "xml_get_current_column_number", "XMLParser $parser", "int" },` |
|         - |  438 | `	{ "xml_get_current_line_number", "XMLParser $parser", "int" },` |
|         - |  439 | `	{ "xml_get_error_code", "XMLParser $parser", "int" },` |
|         - |  440 | `	{ "xml_parse", "XMLParser $parser, string $data, bool $is_final = false", "int" },` |
|         - |  441 | `	{ "xml_parse_into_struct", "XMLParser $parser, string $data, &$values, &$index = NULL", "int\|false" },` |
|         - |  442 | `	{ "xml_parser_create", "?string $encoding = NULL", "XMLParser" },` |
|         - |  443 | `	{ "xml_parser_create_ns", "?string $encoding = NULL, string $separator = ':'", "XMLParser" },` |
|         - |  444 | `	{ "xml_parser_get_option", "XMLParser $parser, int $option", "string\|int\|bool" },` |
|         - |  445 | `	{ "xml_parser_set_option", "XMLParser $parser, int $option, $value", "bool" },` |
|         - |  446 | `	{ "xml_set_character_data_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  447 | `	{ "xml_set_default_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  448 | `	{ "xml_set_element_handler", "XMLParser $parser, callable\|string\|null $start_handler, callable\|string\|null $end_handler", "true" },` |
|         - |  449 | `	{ "xml_set_end_namespace_decl_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  450 | `	{ "xml_set_external_entity_ref_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  451 | `	{ "xml_set_notation_decl_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  452 | `	{ "xml_set_processing_instruction_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  453 | `	{ "xml_set_start_namespace_decl_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  454 | `	{ "xml_set_unparsed_entity_decl_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  455 | `	/* ext/xmlwriter: php presents every writer verb under a function name as` |
|         - |  456 | `	 * well, with the writer as argument #1 -- which is the numbering its own` |
|         - |  457 | `	 * diagnostics report from BOTH spellings (see vm_xmlwriter.c). */` |
|         - |  458 | `	{ "xmlwriter_open_uri", "string $uri", "XMLWriter\|false" },` |
|         - |  459 | `	{ "xmlwriter_open_memory", "", "XMLWriter\|false" },` |
|         - |  460 | `	{ "xmlwriter_set_indent", "XMLWriter $writer, bool $enable", "bool" },` |
|         - |  461 | `	{ "xmlwriter_set_indent_string", "XMLWriter $writer, string $indentation", "bool" },` |
|         - |  462 | `	{ "xmlwriter_start_comment", "XMLWriter $writer", "bool" },` |
|         - |  463 | `	{ "xmlwriter_end_comment", "XMLWriter $writer", "bool" },` |
|         - |  464 | `	{ "xmlwriter_start_attribute", "XMLWriter $writer, string $name", "bool" },` |
|         - |  465 | `	{ "xmlwriter_end_attribute", "XMLWriter $writer", "bool" },` |
|         - |  466 | `	{ "xmlwriter_write_attribute", "XMLWriter $writer, string $name, string $value", "bool" },` |
|         - |  467 | `	{ "xmlwriter_start_attribute_ns", "XMLWriter $writer, ?string $prefix, string $name, ?string $namespace", "bool" },` |
|         - |  468 | `	{ "xmlwriter_write_attribute_ns", "XMLWriter $writer, ?string $prefix, string $name, ?string $namespace, string $value", "bool" },` |
|         - |  469 | `	{ "xmlwriter_start_element", "XMLWriter $writer, string $name", "bool" },` |
|         - |  470 | `	{ "xmlwriter_end_element", "XMLWriter $writer", "bool" },` |
|         - |  471 | `	{ "xmlwriter_full_end_element", "XMLWriter $writer", "bool" },` |
|         - |  472 | `	{ "xmlwriter_start_element_ns", "XMLWriter $writer, ?string $prefix, string $name, ?string $namespace", "bool" },` |
|         - |  473 | `	{ "xmlwriter_write_element", "XMLWriter $writer, string $name, ?string $content = null", "bool" },` |
|         - |  474 | `	{ "xmlwriter_write_element_ns", "XMLWriter $writer, ?string $prefix, string $name, ?string $namespace, ?string $content = null", "bool" },` |
|         - |  475 | `	{ "xmlwriter_start_pi", "XMLWriter $writer, string $target", "bool" },` |
|         - |  476 | `	{ "xmlwriter_end_pi", "XMLWriter $writer", "bool" },` |
|         - |  477 | `	{ "xmlwriter_write_pi", "XMLWriter $writer, string $target, string $content", "bool" },` |
|         - |  478 | `	{ "xmlwriter_start_cdata", "XMLWriter $writer", "bool" },` |
|         - |  479 | `	{ "xmlwriter_end_cdata", "XMLWriter $writer", "bool" },` |
|         - |  480 | `	{ "xmlwriter_write_cdata", "XMLWriter $writer, string $content", "bool" },` |
|         - |  481 | `	{ "xmlwriter_text", "XMLWriter $writer, string $content", "bool" },` |
|         - |  482 | `	{ "xmlwriter_write_raw", "XMLWriter $writer, string $content", "bool" },` |
|         - |  483 | `	{ "xmlwriter_start_document", "XMLWriter $writer, ?string $version = '1.0', ?string $encoding = null, ?string $standalone = null", "bool" },` |
|         - |  484 | `	{ "xmlwriter_end_document", "XMLWriter $writer", "bool" },` |
|         - |  485 | `	{ "xmlwriter_write_comment", "XMLWriter $writer, string $content", "bool" },` |
|         - |  486 | `	{ "xmlwriter_start_dtd", "XMLWriter $writer, string $qualifiedName, ?string $publicId = null, ?string $systemId = null", "bool" },` |
|         - |  487 | `	{ "xmlwriter_end_dtd", "XMLWriter $writer", "bool" },` |
|         - |  488 | `	{ "xmlwriter_write_dtd", "XMLWriter $writer, string $name, ?string $publicId = null, ?string $systemId = null, ?string $content = null", "bool" },` |
|         - |  489 | `	{ "xmlwriter_start_dtd_element", "XMLWriter $writer, string $qualifiedName", "bool" },` |
|         - |  490 | `	{ "xmlwriter_end_dtd_element", "XMLWriter $writer", "bool" },` |
|         - |  491 | `	{ "xmlwriter_write_dtd_element", "XMLWriter $writer, string $name, string $content", "bool" },` |
|         - |  492 | `	{ "xmlwriter_start_dtd_attlist", "XMLWriter $writer, string $name", "bool" },` |
|         - |  493 | `	{ "xmlwriter_end_dtd_attlist", "XMLWriter $writer", "bool" },` |
|         - |  494 | `	{ "xmlwriter_write_dtd_attlist", "XMLWriter $writer, string $name, string $content", "bool" },` |
|         - |  495 | `	{ "xmlwriter_start_dtd_entity", "XMLWriter $writer, string $name, bool $isParam", "bool" },` |
|         - |  496 | `	{ "xmlwriter_end_dtd_entity", "XMLWriter $writer", "bool" },` |
|         - |  497 | `	{ "xmlwriter_write_dtd_entity", "XMLWriter $writer, string $name, string $content, bool $isParam = false, ?string $publicId = null, ?string $systemId = null, ?string $notationData = null", "bool" },` |
|         - |  498 | `	{ "xmlwriter_output_memory", "XMLWriter $writer, bool $flush = true", "string" },` |
|         - |  499 | `	{ "xmlwriter_flush", "XMLWriter $writer, bool $empty = true", "string\|int" },` |
|         - |  500 | `	{ "session_abort", "", "bool" },` |
|         - |  501 | `	{ "session_cache_expire", "?int $value = null", "int\|false" },` |
|         - |  502 | `	{ "session_cache_limiter", "?string $value = null", "string\|false" },` |
|         - |  503 | `	{ "session_commit", "", "bool" },` |
|         - |  504 | `	{ "session_create_id", "string $prefix = \"\"", "string\|false" },` |
|         - |  505 | `	{ "session_decode", "string $data", "bool" },` |
|         - |  506 | `	{ "session_destroy", "", "bool" },` |
|         - |  507 | `	{ "session_gc", "", "int\|false" },` |
|         - |  508 | `	{ "session_get_cookie_params", "", "array" },` |
|         - |  509 | `	{ "session_set_save_handler", "$sessionhandler, ...$rest = ?", "bool" },` |
|         - |  510 | `	{ "session_set_cookie_params", "array\|int $lifetime_or_options, ?string $path = null, ?string $domain = null, ?bool $secure = null, ?bool $httponly = null", "bool" },` |
|         - |  511 | `	{ "session_encode", "", "string\|false" },` |
|         - |  512 | `	{ "session_id", "?string $id = null", "string\|false" },` |
|         - |  513 | `	{ "session_module_name", "?string $module = null", "string\|false" },` |
|         - |  514 | `	{ "session_name", "?string $name = null", "string\|false" },` |
|         - |  515 | `	{ "session_regenerate_id", "bool $delete_old_session = false", "bool" },` |
|         - |  516 | `	{ "session_register_shutdown", "", "void" },` |
|         - |  517 | `	{ "session_reset", "", "bool" },` |
|         - |  518 | `	{ "session_save_path", "?string $path = null", "string\|false" },` |
|         - |  519 | `	{ "session_start", "array $options = []", "bool" },` |
|         - |  520 | `	{ "session_status", "", "int" },` |
|         - |  521 | `	{ "session_unset", "", "bool" },` |
|         - |  522 | `	{ "session_write_close", "", "bool" },` |
|         - |  523 | `	{ "abs", "int\|float $num", "int\|float" },` |
|         - |  524 | `	{ "acos", "float $num", "float" },` |
|         - |  525 | `	{ "acosh", "float $num", "float" },` |
|         - |  526 | `	{ "addcslashes", "string $string, string $characters", "string" },` |
|         - |  527 | `	{ "addslashes", "string $string", "string" },` |
|         - |  528 | `	{ "array_all", "array $array, callable $callback", "bool" },` |
|         - |  529 | `	{ "array_any", "array $array, callable $callback", "bool" },` |
|         - |  530 | `	{ "array_chunk", "array $array, int $length, bool $preserve_keys = false", "array" },` |
|         - |  531 | `	{ "array_column", "array $array, string\|int\|null $column_key, string\|int\|null $index_key = NULL", "array" },` |
|         - |  532 | `	{ "array_combine", "array $keys, array $values", "array" },` |
|         - |  533 | `	{ "array_diff", "array $array, array ...$arrays = ?", "array" },` |
|         - |  534 | `	{ "array_diff_assoc", "array $array, array ...$arrays = ?", "array" },` |
|         - |  535 | `	{ "array_diff_key", "array $array, array ...$arrays = ?", "array" },` |
|         - |  536 | `	{ "array_diff_uassoc", "array $array, ...$rest = ?", "array" },` |
|         - |  537 | `	{ "array_diff_ukey", "array $array, ...$rest = ?", "array" },` |
|         - |  538 | `	{ "array_fill", "int $start_index, int $count, mixed $value", "array" },` |
|         - |  539 | `	{ "array_fill_keys", "array $keys, mixed $value", "array" },` |
|         - |  540 | `	{ "array_filter", "array $array, ?callable $callback = NULL, int $mode = 0", "array" },` |
|         - |  541 | `	{ "array_find", "array $array, callable $callback", "mixed" },` |
|         - |  542 | `	{ "array_find_key", "array $array, callable $callback", "mixed" },` |
|         - |  543 | `	{ "array_first", "array $array", "mixed" },` |
|         - |  544 | `	{ "array_flip", "array $array", "array" },` |
|         - |  545 | `	{ "array_intersect", "array $array, array ...$arrays = ?", "array" },` |
|         - |  546 | `	{ "array_intersect_assoc", "array $array, array ...$arrays = ?", "array" },` |
|         - |  547 | `	{ "array_intersect_key", "array $array, array ...$arrays = ?", "array" },` |
|         - |  548 | `	{ "array_intersect_uassoc", "array $array, ...$rest = ?", "array" },` |
|         - |  549 | `	{ "array_intersect_ukey", "array $array, ...$rest = ?", "array" },` |
|         - |  550 | `	{ "array_is_list", "array $array", "bool" },` |
|         - |  551 | `	{ "array_key_exists", "$key, array $array", "bool" },` |
|         - |  552 | `	{ "array_key_first", "array $array", "string\|int\|null" },` |
|         - |  553 | `	{ "array_key_last", "array $array", "string\|int\|null" },` |
|         - |  554 | `	{ "array_keys", "array $array, mixed $filter_value = ?, bool $strict = false", "array" },` |
|         - |  555 | `	{ "array_last", "array $array", "mixed" },` |
|         - |  556 | `	{ "array_map", "?callable $callback, array $array, array ...$arrays = ?", "array" },` |
|         - |  557 | `	{ "array_merge", "array ...$arrays = ?", "array" },` |
|         - |  558 | `	{ "array_multisort", "&$array, &...$rest = ?", "true" },` |
|         - |  559 | `	{ "array_merge_recursive", "array ...$arrays = ?", "array" },` |
|         - |  560 | `	{ "array_pad", "array $array, int $length, mixed $value", "array" },` |
|         - |  561 | `	{ "array_pop", "array &$array", "mixed" },` |
|         - |  562 | `	{ "array_product", "array $array", "int\|float" },` |
|         - |  563 | `	{ "array_push", "array &$array, mixed ...$values = ?", "int" },` |
|         - |  564 | `	{ "array_rand", "array $array, int $num = 1", "array\|string\|int" },` |
|         - |  565 | `	{ "array_reduce", "array $array, callable $callback, mixed $initial = NULL", "mixed" },` |
|         - |  566 | `	{ "array_replace", "array $array, array ...$replacements = ?", "array" },` |
|         - |  567 | `	{ "array_reverse", "array $array, bool $preserve_keys = false", "array" },` |
|         - |  568 | `	{ "array_search", "mixed $needle, array $haystack, bool $strict = false", "string\|int\|false" },` |
|         - |  569 | `	{ "array_shift", "array &$array", "mixed" },` |
|         - |  570 | `	{ "array_slice", "array $array, int $offset, ?int $length = NULL, bool $preserve_keys = false", "array" },` |
|         - |  571 | `	{ "array_splice", "array &$array, int $offset, ?int $length = NULL, mixed $replacement = ?", "array" },` |
|         - |  572 | `	{ "array_sum", "array $array", "int\|float" },` |
|         - |  573 | `	{ "array_udiff", "array $array, ...$rest = ?", "array" },` |
|         - |  574 | `	{ "array_udiff_assoc", "array $array, ...$rest = ?", "array" },` |
|         - |  575 | `	{ "array_udiff_uassoc", "array $array, ...$rest = ?", "array" },` |
|         - |  576 | `	{ "array_uintersect", "array $array, ...$rest = ?", "array" },` |
|         - |  577 | `	{ "array_uintersect_assoc", "array $array, ...$rest = ?", "array" },` |
|         - |  578 | `	{ "array_uintersect_uassoc", "array $array, ...$rest = ?", "array" },` |
|         - |  579 | `	{ "array_unique", "array $array, int $flags = 2", "array" },` |
|         - |  580 | `	{ "array_unshift", "array &$array, mixed ...$values = ?", "int" },` |
|         - |  581 | `	{ "array_values", "array $array", "array" },` |
|         - |  582 | `	{ "array_walk", "object\|array &$array, callable $callback, mixed $arg = ?", "true" },` |
|         - |  583 | `	{ "array_walk_recursive", "object\|array &$array, callable $callback, mixed $arg = ?", "true" },` |
|         - |  584 | `	{ "arsort", "array &$array, int $flags = 0", "true" },` |
|         - |  585 | `	{ "asin", "float $num", "float" },` |
|         - |  586 | `	{ "asinh", "float $num", "float" },` |
|         - |  587 | `	{ "asort", "array &$array, int $flags = 0", "true" },` |
|         - |  588 | `	{ "assert", "mixed $assertion, Throwable\|string\|null $description = NULL", "bool" },` |
|         - |  589 | `	{ "atan", "float $num", "float" },` |
|         - |  590 | `	{ "atanh", "float $num", "float" },` |
|         - |  591 | `	{ "atan2", "float $y, float $x", "float" },` |
|         - |  592 | `	{ "base64_decode", "string $string, bool $strict = false", "string\|false" },` |
|         - |  593 | `	{ "base64_encode", "string $string", "string" },` |
|         - |  594 | `	{ "base_convert", "string $num, int $from_base, int $to_base", "string" },` |
|         - |  595 | `	{ "basename", "string $path, string $suffix = ''", "string" },` |
|         - |  596 | `	{ "bcadd", "string $num1, string $num2, ?int $scale = NULL", "string" },` |
|         - |  597 | `	{ "bcceil", "string $num", "string" },` |
|         - |  598 | `	{ "bccomp", "string $num1, string $num2, ?int $scale = NULL", "int" },` |
|         - |  599 | `	{ "bcdiv", "string $num1, string $num2, ?int $scale = NULL", "string" },` |
|         - |  600 | `	{ "bcdivmod", "string $num1, string $num2, ?int $scale = NULL", "array" },` |
|         - |  601 | `	{ "bcmod", "string $num1, string $num2, ?int $scale = NULL", "string" },` |
|         - |  602 | `	{ "bcfloor", "string $num", "string" },` |
|         - |  603 | `	{ "bcmul", "string $num1, string $num2, ?int $scale = NULL", "string" },` |
|         - |  604 | `	{ "bcpow", "string $num, string $exponent, ?int $scale = NULL", "string" },` |
|         - |  605 | `	{ "bcpowmod", "string $num, string $exponent, string $modulus, ?int $scale = NULL", "string" },` |
|         - |  606 | `	{ "bcround", "string $num, int $precision = 0, RoundingMode $mode = ?", "string" },` |
|         - |  607 | `	{ "bcsqrt", "string $num, ?int $scale = NULL", "string" },` |
|         - |  608 | `	{ "bcscale", "?int $scale = NULL", "int" },` |
|         - |  609 | `	{ "bcsub", "string $num1, string $num2, ?int $scale = NULL", "string" },` |
|         - |  610 | `	{ "bin2hex", "string $string", "string" },` |
|         - |  611 | `	{ "bindec", "string $binary_string", "int\|float" },` |
|         - |  612 | `	{ "boolval", "mixed $value", "bool" },` |
|         - |  613 | `	{ "call_user_func", "callable $callback, mixed ...$args = ?", "mixed" },` |
|         - |  614 | `	{ "call_user_func_array", "callable $callback, array $args", "mixed" },` |
|         - |  615 | `	{ "cal_days_in_month", "int $calendar, int $month, int $year", "int" },` |
|         - |  616 | `	{ "cal_from_jd", "int $julian_day, int $calendar", "array" },` |
|         - |  617 | `	{ "cal_info", "int $calendar = -1", "array" },` |
|         - |  618 | `	{ "cal_to_jd", "int $calendar, int $month, int $day, int $year", "int" },` |
|         - |  619 | `	{ "ceil", "int\|float $num", "float" },` |
|         - |  620 | `	{ "chdir", "string $directory", "bool" },` |
|         - |  621 | `	{ "chgrp", "string $filename, string\|int $group", "bool" },` |
|         - |  622 | `	{ "chmod", "string $filename, int $permissions", "bool" },` |
|         - |  623 | `	{ "chop", "string $string, string $characters = ?", "string" },` |
|         - |  624 | `	{ "chown", "string $filename, string\|int $user", "bool" },` |
|         - |  625 | `	{ "chr", "int $codepoint", "string" },` |
|         - |  626 | `	{ "chroot", "string $directory", "bool" },` |
|         - |  627 | `	{ "chunk_split", "string $string, int $length = 76, string $separator = ?", "string" },` |
|         - |  628 | `	{ "class_alias", "string $class, string $alias, bool $autoload = true", "bool" },` |
|         - |  629 | `	{ "class_exists", "string $class, bool $autoload = true", "bool" },` |
|         - |  630 | `	{ "enum_exists", "string $enum, bool $autoload = true", "bool" },` |
|         - |  631 | `	{ "clone", "object $object, array $withProperties = []", "object" },` |
|         - |  632 | `	{ "closedir", "$dir_handle = NULL", "void" },` |
|         - |  633 | `	{ "compact", "$var_name, ...$var_names = ?", "array" },` |
|         - |  634 | `	{ "constant", "string $name", "mixed" },` |
|         - |  635 | `	{ "convert_uudecode", "string $string", "string\|false" },` |
|         - |  636 | `	{ "convert_uuencode", "string $string", "string" },` |
|         - |  637 | `	{ "copy", "string $from, string $to, $context = NULL", "bool" },` |
|         - |  638 | `	{ "cos", "float $num", "float" },` |
|         - |  639 | `	{ "cosh", "float $num", "float" },` |
|         - |  640 | `	{ "count", "Countable\|array $value, int $mode = 0", "int" },` |
|         - |  641 | `	{ "count_chars", "string $string, int $mode = 0", "array\|string" },` |
|         - |  642 | `	{ "crc32", "string $string", "int" },` |
|         - |  643 | `	{ "ctype_alnum", "mixed $text", "bool" },` |
|         - |  644 | `	{ "ctype_alpha", "mixed $text", "bool" },` |
|         - |  645 | `	{ "ctype_cntrl", "mixed $text", "bool" },` |
|         - |  646 | `	{ "ctype_digit", "mixed $text", "bool" },` |
|         - |  647 | `	{ "ctype_graph", "mixed $text", "bool" },` |
|         - |  648 | `	{ "ctype_lower", "mixed $text", "bool" },` |
|         - |  649 | `	{ "ctype_print", "mixed $text", "bool" },` |
|         - |  650 | `	{ "ctype_punct", "mixed $text", "bool" },` |
|         - |  651 | `	{ "ctype_space", "mixed $text", "bool" },` |
|         - |  652 | `	{ "ctype_upper", "mixed $text", "bool" },` |
|         - |  653 | `	{ "ctype_xdigit", "mixed $text", "bool" },` |
|         - |  654 | `	{ "current", "object\|array $array", "mixed" },` |
|         - |  655 | `	{ "date", "string $format, ?int $timestamp = NULL", "string" },` |
|         - |  656 | `	{ "date_add", "DateTime $object, DateInterval $interval", "DateTime" },` |
|         - |  657 | `	{ "date_create", "string $datetime = 'now', ?DateTimeZone $timezone = NULL", "DateTime\|false" },` |
|         - |  658 | `	{ "date_create_from_format", "string $format, string $datetime, ?DateTimeZone $timezone = NULL", "DateTime\|false" },` |
|         - |  659 | `	{ "date_create_immutable", "string $datetime = 'now', ?DateTimeZone $timezone = NULL", "DateTimeImmutable\|false" },` |
|         - |  660 | `	{ "date_create_immutable_from_format", "string $format, string $datetime, ?DateTimeZone $timezone = NULL", "DateTimeImmutable\|false" },` |
|         - |  661 | `	{ "date_date_set", "DateTime $object, int $year, int $month, int $day", "DateTime" },` |
|         - |  662 | `	{ "date_diff", "DateTimeInterface $baseObject, DateTimeInterface $targetObject, bool $absolute = false", "DateInterval" },` |
|         - |  663 | `	{ "date_format", "DateTimeInterface $object, string $format", "string" },` |
|         - |  664 | `	{ "date_get_last_errors", "", "array\|false" },` |
|         - |  665 | `	{ "date_interval_create_from_date_string", "string $datetime", "DateInterval\|false" },` |
|         - |  666 | `	{ "date_interval_format", "DateInterval $object, string $format", "string" },` |
|         - |  667 | `	{ "date_isodate_set", "DateTime $object, int $year, int $week, int $dayOfWeek = 1", "DateTime" },` |
|         - |  668 | `	{ "date_modify", "DateTime $object, string $modifier", "DateTime\|false" },` |
|         - |  669 | `	{ "date_offset_get", "DateTimeInterface $object", "int" },` |
|         - |  670 | `	{ "date_parse", "string $datetime", "array" },` |
|         - |  671 | `	{ "date_parse_from_format", "string $format, string $datetime", "array" },` |
|         - |  672 | `	{ "date_sub", "DateTime $object, DateInterval $interval", "DateTime" },` |
|         - |  673 | `	{ "date_time_set", "DateTime $object, int $hour, int $minute, int $second = 0, int $microsecond = 0", "DateTime" },` |
|         - |  674 | `	{ "date_timestamp_get", "DateTimeInterface $object", "int" },` |
|         - |  675 | `	{ "date_timestamp_set", "DateTime $object, int $timestamp", "DateTime" },` |
|         - |  676 | `	{ "date_timezone_get", "DateTimeInterface $object", "DateTimeZone\|false" },` |
|         - |  677 | `	{ "date_timezone_set", "DateTime $object, DateTimeZone $timezone", "DateTime" },` |
|         - |  678 | `	{ "timezone_name_get", "DateTimeZone $object", "string" },` |
|         - |  679 | `	{ "timezone_offset_get", "DateTimeZone $object, DateTimeInterface $datetime", "int" },` |
|         - |  680 | `	{ "timezone_open", "string $timezone", "DateTimeZone\|false" },` |
|         - |  681 | `	{ "date_default_timezone_get", "", "string" },` |
|         - |  682 | `	{ "date_default_timezone_set", "string $timezoneId", "bool" },` |
|         - |  683 | `	{ "debug_backtrace", "int $options = 1, int $limit = 0", "array" },` |
|         - |  684 | `	{ "debug_print_backtrace", "int $options = 0, int $limit = 0", "void" },` |
|         - |  685 | `	{ "decbin", "int $num", "string" },` |
|         - |  686 | `	{ "dechex", "int $num", "string" },` |
|         - |  687 | `	{ "decoct", "int $num", "string" },` |
|         - |  688 | `	{ "define", "string $constant_name, mixed $value, bool $case_insensitive = false", "bool" },` |
|         - |  689 | `	{ "defined", "string $constant_name", "bool" },` |
|         - |  690 | `	{ "deg2rad", "float $num", "float" },` |
|         - |  691 | `	{ "die", "string\|int $status = 0", "never" },` |
|         - |  692 | `	{ "dir", "string $directory, $context = NULL", "Directory\|false" },` |
|         - |  693 | `	{ "dirname", "string $path, int $levels = 1", "string" },` |
|         - |  694 | `	{ "disk_free_space", "string $directory", "float\|false" },` |
|         - |  695 | `	{ "disk_total_space", "string $directory", "float\|false" },` |
|         - |  696 | `	{ "diskfreespace", "string $directory", "float\|false" },` |
|         - |  697 | `	{ "easter_date", "?int $year = NULL, int $mode = 0", "int" },` |
|         - |  698 | `	{ "easter_days", "?int $year = NULL, int $mode = 0", "int" },` |
|         - |  699 | `	{ "end", "object\|array &$array", "mixed" },` |
|         - |  700 | `	{ "error_get_last", "", "?array" },` |
|         - |  701 | `	{ "error_clear_last", "", "void" },` |
|         - |  702 | `	{ "error_log", "string $message, int $message_type = 0, ?string $destination = NULL, ?string $additional_headers = NULL", "bool" },` |
|         - |  703 | `	{ "error_reporting", "?int $error_level = NULL", "int" },` |
|         - |  704 | `	{ "escapeshellarg", "string $arg", "string" },` |
|         - |  705 | `	{ "escapeshellcmd", "string $command", "string" },` |
|         - |  706 | `	{ "exec", "string $command, &$output = NULL, &$result_code = NULL", "string\|false" },` |
|         - |  707 | `	{ "exit", "string\|int $status = 0", "never" },` |
|         - |  708 | `	{ "exp", "float $num", "float" },` |
|         - |  709 | `	{ "expm1", "float $num", "float" },` |
|         - |  710 | `	{ "explode", "string $separator, string $string, int $limit = 9223372036854775807", "array" },` |
|         - |  711 | `	{ "extension_loaded", "string $extension", "bool" },` |
|         - |  712 | `	{ "extract", "array &$array, int $flags = 0, string $prefix = ''", "int" },` |
|         - |  713 | `	{ "fclose", "$stream", "bool" },` |
|         - |  714 | `	{ "feof", "$stream", "bool" },` |
|         - |  715 | `	{ "fflush", "$stream", "bool" },` |
|         - |  716 | `	{ "fgetc", "$stream", "string\|false" },` |
|         - |  717 | `	{ "fgetcsv", "$stream, ?int $length = NULL, string $separator = ',', string $enclosure = '\"', string $escape = '\\\\'", "array\|false" },` |
|         - |  718 | `	{ "fgets", "$stream, ?int $length = NULL", "string\|false" },` |
|         - |  719 | `	{ "file", "string $filename, int $flags = 0, $context = NULL", "array\|false" },` |
|         - |  720 | `	{ "file_exists", "string $filename", "bool" },` |
|         - |  721 | `	{ "file_get_contents", "string $filename, bool $use_include_path = false, $context = NULL, int $offset = 0, ?int $length = NULL", "string\|false" },` |
|         - |  722 | `	{ "file_put_contents", "string $filename, mixed $data, int $flags = 0, $context = NULL", "int\|false" },` |
|         - |  723 | `	{ "fileatime", "string $filename", "int\|false" },` |
|         - |  724 | `	{ "filectime", "string $filename", "int\|false" },` |
|         - |  725 | `	{ "filegroup", "string $filename", "int\|false" },` |
|         - |  726 | `	{ "fileinode", "string $filename", "int\|false" },` |
|         - |  727 | `	{ "filemtime", "string $filename", "int\|false" },` |
|         - |  728 | `	{ "fileowner", "string $filename", "int\|false" },` |
|         - |  729 | `	{ "fileperms", "string $filename", "int\|false" },` |
|         - |  730 | `	{ "filesize", "string $filename", "int\|false" },` |
|         - |  731 | `	{ "filetype", "string $filename", "string\|false" },` |
|         - |  732 | `	{ "filter_has_var", "int $input_type, string $var_name", "bool" },` |
|         - |  733 | `	{ "filter_id", "string $name", "int\|false" },` |
|         - |  734 | `	{ "filter_input", "int $type, string $var_name, int $filter = 516, array\|int $options = 0", "mixed" },` |
|         - |  735 | `	{ "filter_input_array", "int $type, array\|int $options = 516, bool $add_empty = true", "array\|false\|null" },` |
|         - |  736 | `	{ "filter_list", "", "array" },` |
|         - |  737 | `	{ "filter_var", "mixed $value, int $filter = 516, array\|int $options = 0", "mixed" },` |
|         - |  738 | `	{ "filter_var_array", "array $array, array\|int $options = 516, bool $add_empty = true", "array\|false\|null" },` |
|         - |  739 | `	{ "floatval", "mixed $value", "float" },` |
|         - |  740 | `	{ "flock", "$stream, int $operation, &$would_block = NULL", "bool" },` |
|         - |  741 | `	{ "floor", "int\|float $num", "float" },` |
|         - |  742 | `	{ "flush", "", "void" },` |
|         - |  743 | `	{ "fmod", "float $num1, float $num2", "float" },` |
|         - |  744 | `	{ "fnmatch", "string $pattern, string $filename, int $flags = 0", "bool" },` |
|         - |  745 | `	{ "fopen", "string $filename, string $mode, bool $use_include_path = false, $context = NULL", "" },` |
|         - |  746 | `	{ "forward_static_call", "callable $callback, mixed ...$args = ?", "mixed" },` |
|         - |  747 | `	{ "forward_static_call_array", "callable $callback, array $args", "mixed" },` |
|         - |  748 | `	{ "fpow", "float $num, float $exponent", "float" },` |
|         - |  749 | `	{ "fpassthru", "$stream", "int" },` |
|         - |  750 | `	{ "fprintf", "$stream, string $format, mixed ...$values = ?", "int" },` |
|         - |  751 | `	{ "fputcsv", "$stream, array $fields, string $separator = ',', string $enclosure = '\"', string $escape = '\\\\', string $eol = ?", "int\|false" },` |
|         - |  752 | `	{ "fputs", "$stream, string $data, ?int $length = NULL", "int\|false" },` |
|         - |  753 | `	{ "fread", "$stream, int $length", "string\|false" },` |
|         - |  754 | `	{ "frenchtojd", "int $month, int $day, int $year", "int" },` |
|         - |  755 | `	{ "fseek", "$stream, int $offset, int $whence = 0", "int" },` |
|         - |  756 | `	{ "fstat", "$stream", "array\|false" },` |
|         - |  757 | `	{ "ftell", "$stream", "int\|false" },` |
|         - |  758 | `	{ "ftruncate", "$stream, int $size", "bool" },` |
|         - |  759 | `	{ "func_get_arg", "int $position", "mixed" },` |
|         - |  760 | `	{ "func_get_args", "", "array" },` |
|         - |  761 | `	{ "func_num_args", "", "int" },` |
|         - |  762 | `	{ "function_exists", "string $function", "bool" },` |
|         - |  763 | `	{ "fwrite", "$stream, string $data, ?int $length = NULL", "int\|false" },` |
|         - |  764 | `	{ "gc_collect_cycles", "", "int" },` |
|         - |  765 | `	{ "gc_disable", "", "void" },` |
|         - |  766 | `	{ "gc_enable", "", "void" },` |
|         - |  767 | `	{ "gc_enabled", "", "bool" },` |
|         - |  768 | `	{ "gc_mem_caches", "", "int" },` |
|         - |  769 | `	{ "gc_status", "", "array" },` |
|         - |  770 | `	{ "get_called_class", "", "string" },` |
|         - |  771 | `	{ "get_class", "object $object = ?", "string" },` |
|         - |  772 | `	{ "get_class_methods", "object\|string $object_or_class", "array" },` |
|         - |  773 | `	{ "get_class_vars", "string $class", "array" },` |
|         - |  774 | `	{ "get_current_user", "", "string" },` |
|         - |  775 | `	{ "get_declared_classes", "", "array" },` |
|         - |  776 | `	{ "get_declared_interfaces", "", "array" },` |
|         - |  777 | `	{ "get_declared_traits", "", "array" },` |
|         - |  778 | `	{ "get_defined_constants", "bool $categorize = false", "array" },` |
|         - |  779 | `	{ "get_defined_functions", "bool $exclude_disabled = true", "array" },` |
|         - |  780 | `	{ "get_defined_vars", "", "array" },` |
|         - |  781 | `	{ "get_html_translation_table", "int $table = 0, int $flags = 11, string $encoding = 'UTF-8'", "array" },` |
|         - |  782 | `	{ "get_include_path", "", "string\|false" },` |
|         - |  783 | `	{ "get_included_files", "", "array" },` |
|         - |  784 | `	{ "get_loaded_extensions", "bool $zend_extensions = false", "array" },` |
|         - |  785 | `	{ "get_mangled_object_vars", "object $object", "array" },` |
|         - |  786 | `	{ "get_object_vars", "object $object", "array" },` |
|         - |  787 | `	{ "get_parent_class", "object\|string $object_or_class = ?", "string\|false" },` |
|         - |  788 | `	{ "get_resource_id", "$resource", "int" },` |
|         - |  789 | `	{ "get_resource_type", "$resource", "string" },` |
|         - |  790 | `	{ "getcwd", "", "string\|false" },` |
|         - |  791 | `	{ "getdate", "?int $timestamp = NULL", "array" },` |
|         - |  792 | `	{ "getenv", "?string $name = NULL, bool $local_only = false", "array\|string\|false" },` |
|         - |  793 | `	{ "getmygid", "", "int\|false" },` |
|         - |  794 | `	{ "getmypid", "", "int\|false" },` |
|         - |  795 | `	{ "getmyuid", "", "int\|false" },` |
|         - |  796 | `	{ "getopt", "string $short_options, array $long_options = ?, &$rest_index = NULL", "array\|false" },` |
|         - |  797 | `	{ "getrandmax", "", "int" },` |
|         - |  798 | `	{ "gettimeofday", "bool $as_float = false", "array\|float" },` |
|         - |  799 | `	{ "gettype", "mixed $value", "string" },` |
|         - |  800 | `	{ "get_debug_type", "mixed $value", "string" },` |
|         - |  801 | `	{ "gmdate", "string $format, ?int $timestamp = NULL", "string" },` |
|         - |  802 | `	{ "gmmktime", "int $hour, ?int $minute = NULL, ?int $second = NULL, ?int $month = NULL, ?int $day = NULL, ?int $year = NULL", "int\|false" },` |
|         - |  803 | `	{ "hash", "string $algo, string $data, bool $binary = false, array $options = []", "string" },` |
|         - |  804 | `	{ "hash_algos", "", "array" },` |
|         - |  805 | `	{ "hash_equals", "string $known_string, string $user_string", "bool" },` |
|         - |  806 | `	{ "hash_hmac", "string $algo, string $data, string $key, bool $binary = false", "string" },` |
|         - |  807 | `	{ "hash_hmac_algos", "", "array" },` |
|         - |  808 | `	{ "hash_init", "string $algo, int $flags = 0, string $key = \'\', array $options = []", "HashContext" },` |
|         - |  809 | `	{ "hash_update", "HashContext $context, string $data", "bool" },` |
|         - |  810 | `	{ "hash_final", "HashContext $context, bool $binary = false", "string" },` |
|         - |  811 | `	{ "hash_copy", "HashContext $context", "HashContext" },` |
|         - |  812 | `	{ "hash_file", "string $algo, string $filename, bool $binary = false, array $options = []", "string\|false" },` |
|         - |  813 | `	{ "hash_hkdf", "string $algo, string $key, int $length = 0, string $info = \'\', string $salt = \'\'", "string" },` |
|         - |  814 | `	{ "hash_pbkdf2", "string $algo, string $password, string $salt, int $iterations, int $length = 0, bool $binary = false, array $options = []", "string" },` |
|         - |  815 | `	{ "hash_hmac_file", "string $algo, string $filename, string $key, bool $binary = false", "string\|false" },` |
|         - |  816 | `	{ "hash_update_file", "HashContext $context, string $filename, $stream_context = null", "bool" },` |
|         - |  817 | `	{ "hash_update_stream", "HashContext $context, $stream, int $length = -1", "int" },` |
|         - |  818 | `	{ "gregoriantojd", "int $month, int $day, int $year", "int" },` |
|         - |  819 | `	{ "header", "string $header, bool $replace = true, int $response_code = 0", "void" },` |
|         - |  820 | `	{ "header_remove", "?string $name = NULL", "void" },` |
|         - |  821 | `	{ "headers_list", "", "array" },` |
|         - |  822 | `	{ "headers_sent", "&$filename = NULL, &$line = NULL", "bool" },` |
|         - |  823 | `	{ "hexdec", "string $hex_string", "int\|float" },` |
|         - |  824 | `	{ "html_entity_decode", "string $string, int $flags = 11, ?string $encoding = NULL", "string" },` |
|         - |  825 | `	{ "http_build_query", "object\|array $data, string $numeric_prefix = '', ?string $arg_separator = null, int $encoding_type = 1", "string" },` |
|         - |  826 | `	{ "htmlentities", "string $string, int $flags = 11, ?string $encoding = NULL, bool $double_encode = true", "string" },` |
|         - |  827 | `	{ "htmlspecialchars", "string $string, int $flags = 11, ?string $encoding = NULL, bool $double_encode = true", "string" },` |
|         - |  828 | `	{ "htmlspecialchars_decode", "string $string, int $flags = 11", "string" },` |
|         - |  829 | `	{ "http_response_code", "int $response_code = 0", "int\|bool" },` |
|         - |  830 | `	{ "hypot", "float $x, float $y", "float" },` |
|         - |  831 | `	{ "idate", "string $format, ?int $timestamp = NULL", "int\|false" },` |
|         - |  832 | `	{ "implode", "array\|string $separator, ?array $array = NULL", "string" },` |
|         - |  833 | `	{ "in_array", "mixed $needle, array $haystack, bool $strict = false", "bool" },` |
|         - |  834 | `	{ "intdiv", "int $num1, int $num2", "int" },` |
|         - |  835 | `	{ "interface_exists", "string $interface, bool $autoload = true", "bool" },` |
|         - |  836 | `	{ "trait_exists", "string $trait, bool $autoload = true", "bool" },` |
|         - |  837 | `	{ "intval", "mixed $value, int $base = 10", "int" },` |
|         - |  838 | `	{ "is_a", "mixed $object_or_class, string $class, bool $allow_string = false", "bool" },` |
|         - |  839 | `	{ "is_array", "mixed $value", "bool" },` |
|         - |  840 | `	{ "is_bool", "mixed $value", "bool" },` |
|         - |  841 | `	{ "is_callable", "mixed $value, bool $syntax_only = false, &$callable_name = NULL", "bool" },` |
|         - |  842 | `	{ "is_dir", "string $filename", "bool" },` |
|         - |  843 | `	{ "is_double", "mixed $value", "bool" },` |
|         - |  844 | `	{ "is_executable", "string $filename", "bool" },` |
|         - |  845 | `	{ "is_file", "string $filename", "bool" },` |
|         - |  846 | `	{ "is_float", "mixed $value", "bool" },` |
|         - |  847 | `	{ "is_int", "mixed $value", "bool" },` |
|         - |  848 | `	{ "is_integer", "mixed $value", "bool" },` |
|         - |  849 | `	{ "is_link", "string $filename", "bool" },` |
|         - |  850 | `	{ "is_long", "mixed $value", "bool" },` |
|         - |  851 | `	{ "is_null", "mixed $value", "bool" },` |
|         - |  852 | `	{ "is_numeric", "mixed $value", "bool" },` |
|         - |  853 | `	{ "is_object", "mixed $value", "bool" },` |
|         - |  854 | `	{ "is_readable", "string $filename", "bool" },` |
|         - |  855 | `	{ "is_resource", "mixed $value", "bool" },` |
|         - |  856 | `	{ "is_scalar", "mixed $value", "bool" },` |
|         - |  857 | `	{ "is_string", "mixed $value", "bool" },` |
|         - |  858 | `	{ "is_subclass_of", "mixed $object_or_class, string $class, bool $allow_string = true", "bool" },` |
|         - |  859 | `	{ "is_writable", "string $filename", "bool" },` |
|         - |  860 | `	{ "iterator_apply", "Traversable $iterator, callable $callback, ?array $args = NULL", "int" },` |
|         - |  861 | `	{ "iterator_count", "Traversable\|array $iterator", "int" },` |
|         - |  862 | `	{ "iterator_to_array", "Traversable\|array $iterator, bool $preserve_keys = true", "array" },` |
|         - |  863 | `	{ "jddayofweek", "int $julian_day, int $mode = 0", "int\|string" },` |
|         - |  864 | `	{ "jdmonthname", "int $julian_day, int $mode", "string" },` |
|         - |  865 | `	{ "jdtofrench", "int $julian_day", "string" },` |
|         - |  866 | `	{ "jdtogregorian", "int $julian_day", "string" },` |
|         - |  867 | `	{ "jdtojewish", "int $julian_day, bool $hebrew = false, int $flags = 0", "string" },` |
|         - |  868 | `	{ "jdtojulian", "int $julian_day", "string" },` |
|         - |  869 | `	{ "jdtounix", "int $julian_day", "int" },` |
|         - |  870 | `	{ "jewishtojd", "int $month, int $day, int $year", "int" },` |
|         - |  871 | `	{ "join", "array\|string $separator, ?array $array = NULL", "string" },` |
|         - |  872 | `	{ "juliantojd", "int $month, int $day, int $year", "int" },` |
|         - |  873 | `	{ "json_decode", "string $json, ?bool $associative = NULL, int $depth = 512, int $flags = 0", "mixed" },` |
|         - |  874 | `	{ "json_encode", "mixed $value, int $flags = 0, int $depth = 512", "string\|false" },` |
|         - |  875 | `	{ "json_last_error", "", "int" },` |
|         - |  876 | `	{ "json_last_error_msg", "", "string" },` |
|         - |  877 | `	{ "json_validate", "string $json, int $depth = 512, int $flags = 0", "bool" },` |
|         - |  878 | `	{ "key", "object\|array $array", "string\|int\|null" },` |
|         - |  879 | `	{ "key_exists", "$key, array $array", "bool" },` |
|         - |  880 | `	{ "krsort", "array &$array, int $flags = 0", "true" },` |
|         - |  881 | `	{ "ksort", "array &$array, int $flags = 0", "true" },` |
|         - |  882 | `	{ "lcfirst", "string $string", "string" },` |
|         - |  883 | `	{ "levenshtein", "string $string1, string $string2, int $insertion_cost = 1, int $replacement_cost = 1, int $deletion_cost = 1", "int" },` |
|         - |  884 | `	{ "link", "string $target, string $link", "bool" },` |
|         - |  885 | `	{ "localtime", "?int $timestamp = NULL, bool $associative = false", "array" },` |
|         - |  886 | `	{ "log", "float $num, float $base = 2.718281828459045", "float" },` |
|         - |  887 | `	{ "log10", "float $num", "float" },` |
|         - |  888 | `	{ "log1p", "float $num", "float" },` |
|         - |  889 | `	{ "lstat", "string $filename", "array\|false" },` |
|         - |  890 | `	{ "ltrim", "string $string, string $characters = ?", "string" },` |
|         - |  891 | `	{ "max", "mixed $value, mixed ...$values = ?", "mixed" },` |
|         - |  892 | `	{ "mb_chr", "int $codepoint, ?string $encoding = NULL", "string\|false" },` |
|         - |  893 | `	{ "mb_convert_encoding", "array\|string $string, string $to_encoding, array\|string\|null $from_encoding = NULL", "array\|string\|false" },` |
|         - |  894 | `	{ "mb_ord", "string $string, ?string $encoding = NULL", "int\|false" },` |
|         - |  895 | `	{ "mb_ltrim", "string $string, ?string $characters = null, ?string $encoding = null", "string" },` |
|         - |  896 | `	{ "mb_rtrim", "string $string, ?string $characters = null, ?string $encoding = null", "string" },` |
|         - |  897 | `	{ "mb_lcfirst", "string $string, ?string $encoding = null", "string" },` |
|         - |  898 | `	{ "mb_strtolower", "string $string, ?string $encoding = NULL", "string" },` |
|         - |  899 | `	{ "mb_strtoupper", "string $string, ?string $encoding = NULL", "string" },` |
|         - |  900 | `	{ "mb_trim", "string $string, ?string $characters = null, ?string $encoding = null", "string" },` |
|         - |  901 | `	{ "mb_ucfirst", "string $string, ?string $encoding = null", "string" },` |
|         - |  902 | `	{ "iconv", "string $from_encoding, string $to_encoding, string $string", "string\|false" },` |
|         - |  903 | `	{ "iconv_strlen", "string $string, ?string $encoding = NULL", "int\|false" },` |
|         - |  904 | `	{ "iconv_substr", "string $string, int $offset, ?int $length = NULL, ?string $encoding = NULL", "string\|false" },` |
|         - |  905 | `	{ "iconv_strpos", "string $haystack, string $needle, int $offset = 0, ?string $encoding = NULL", "int\|false" },` |
|         - |  906 | `	{ "iconv_strrpos", "string $haystack, string $needle, ?string $encoding = NULL", "int\|false" },` |
|         - |  907 | `	{ "iconv_get_encoding", "string $type = \"all\"", "array\|string\|false" },` |
|         - |  908 | `	{ "iconv_mime_encode", "string $field_name, string $field_value, array $options = []", "string\|false" },` |
|         - |  909 | `	{ "iconv_mime_decode", "string $string, int $mode = 0, ?string $encoding = NULL", "string\|false" },` |
|         - |  910 | `	{ "iconv_mime_decode_headers", "string $headers, int $mode = 0, ?string $encoding = NULL", "array\|false" },` |
|         - |  911 | `	{ "md5", "string $string, bool $binary = false", "string" },` |
|         - |  912 | `	{ "md5_file", "string $filename, bool $binary = false", "string\|false" },` |
|         - |  913 | `	{ "metaphone", "string $string, int $max_phonemes = 0", "string" },` |
|         - |  914 | `	{ "method_exists", "$object_or_class, string $method", "bool" },` |
|         - |  915 | `	{ "memory_get_peak_usage", "bool $real_usage = false", "int" },` |
|         - |  916 | `	{ "memory_get_usage", "bool $real_usage = false", "int" },` |
|         - |  917 | `	{ "microtime", "bool $as_float = false", "string\|float" },` |
|         - |  918 | `	{ "min", "mixed $value, mixed ...$values = ?", "mixed" },` |
|         - |  919 | `	{ "mkdir", "string $directory, int $permissions = 511, bool $recursive = false, $context = NULL", "bool" },` |
|         - |  920 | `	{ "mktime", "int $hour, ?int $minute = NULL, ?int $second = NULL, ?int $month = NULL, ?int $day = NULL, ?int $year = NULL", "int\|false" },` |
|         - |  921 | `	{ "mt_getrandmax", "", "int" },` |
|         - |  922 | `	{ "mt_rand", "int $min = ?, int $max = ?", "int" },` |
|         - |  923 | `	{ "mt_srand", "?int $seed = NULL, int $mode = 0", "void" },` |
|         - |  924 | `	{ "natcasesort", "array &$array", "true" },` |
|         - |  925 | `	{ "natsort", "array &$array", "true" },` |
|         - |  926 | `	{ "next", "object\|array &$array", "mixed" },` |
|         - |  927 | `	{ "nl2br", "string $string, bool $use_xhtml = true", "string" },` |
|         - |  928 | `	{ "number_format", "float $num, int $decimals = 0, ?string $decimal_separator = '.', ?string $thousands_separator = ','", "string" },` |
|         - |  929 | `	{ "ob_clean", "", "bool" },` |
|         - |  930 | `	{ "ob_end_clean", "", "bool" },` |
|         - |  931 | `	{ "ob_end_flush", "", "bool" },` |
|         - |  932 | `	{ "ob_flush", "", "bool" },` |
|         - |  933 | `	{ "ob_get_clean", "", "string\|false" },` |
|         - |  934 | `	{ "ob_get_contents", "", "string\|false" },` |
|         - |  935 | `	{ "ob_get_flush", "", "string\|false" },` |
|         - |  936 | `	{ "ob_get_length", "", "int\|false" },` |
|         - |  937 | `	{ "ob_get_level", "", "int" },` |
|         - |  938 | `	{ "ob_get_status", "bool $full_status = false", "array" },` |
|         - |  939 | `	{ "ob_implicit_flush", "bool $enable = true", "void" },` |
|         - |  940 | `	{ "ob_list_handlers", "", "array" },` |
|         - |  941 | `	{ "ob_start", "$callback = NULL, int $chunk_size = 0, int $flags = 112", "bool" },` |
|         - |  942 | `	{ "octdec", "string $octal_string", "int\|float" },` |
|         - |  943 | `	{ "opendir", "string $directory, $context = NULL", "" },` |
|         - |  944 | `	{ "ord", "string $character", "int" },` |
|         - |  945 | `	{ "pack", "string $format, mixed ...$values = ?", "string" },` |
|         - |  946 | `	{ "sscanf", "string $string, string $format, mixed &...$vars = ?", "array\|int\|null" },` |
|         - |  947 | `	{ "fscanf", "$stream, string $format, mixed &...$vars = ?", "array\|int\|false\|null" },` |
|         - |  948 | `	{ "unpack", "string $format, string $string, int $offset = 0", "array\|false" },` |
|         - |  949 | `	{ "parse_ini_file", "string $filename, bool $process_sections = false, int $scanner_mode = 0", "array\|false" },` |
|         - |  950 | `	{ "parse_ini_string", "string $ini_string, bool $process_sections = false, int $scanner_mode = 0", "array\|false" },` |
|         - |  951 | `	{ "parse_str", "string $string, &$result", "void" },` |
|         - |  952 | `	{ "parse_url", "string $url, int $component = -1", "array\|string\|int\|false\|null" },` |
|         - |  953 | `	{ "crypt", "string $string, string $salt", "string" },` |
|         - |  954 | `	{ "password_algos", "", "array" },` |
|         - |  955 | `	{ "password_get_info", "string $hash", "array" },` |
|         - |  956 | `	{ "password_hash", "string $password, string\|int\|null $algo, array $options = ?", "string" },` |
|         - |  957 | `	{ "password_needs_rehash", "string $hash, string\|int\|null $algo, array $options = ?", "bool" },` |
|         - |  958 | `	{ "password_verify", "string $password, string $hash", "bool" },` |
|         - |  959 | `	{ "passthru", "string $command, &$result_code = NULL", "?false" },` |
|         - |  960 | `	{ "pathinfo", "string $path, int $flags = 15", "array\|string" },` |
|         - |  961 | `	{ "pclose", "$handle", "int" },` |
|         - |  962 | `	{ "php_sapi_name", "", "string\|false" },` |
|         - |  963 | `	{ "php_uname", "string $mode = 'a'", "string" },` |
|         - |  964 | `	{ "phpinfo", "int $flags = 4294967295", "true" },` |
|         - |  965 | `	{ "phpversion", "?string $extension = NULL", "string\|false" },` |
|         - |  966 | `	{ "pi", "", "float" },` |
|         - |  967 | `	{ "popen", "string $command, string $mode", "" },` |
|         - |  968 | `	{ "pos", "object\|array $array", "mixed" },` |
|         - |  969 | `	{ "pow", "mixed $num, mixed $exponent", "object\|int\|float" },` |
|         - |  970 | `	{ "preg_last_error", "", "int" },` |
|         - |  971 | `	{ "preg_last_error_msg", "", "string" },` |
|         - |  972 | `	{ "fsockopen", "string $hostname, int $port = -1, &$error_code = NULL, &$error_message = NULL, ?float $timeout = NULL", "" },` |
|         - |  973 | `	{ "pfsockopen", "string $hostname, int $port = -1, &$error_code = NULL, &$error_message = NULL, ?float $timeout = NULL", "" },` |
|         - |  974 | `	{ "stream_socket_client", "string $address, &$error_code = NULL, &$error_message = NULL, ?float $timeout = NULL, int $flags = 4, $context = NULL", "" },` |
|         - |  975 | `	{ "stream_socket_server", "string $address, &$error_code = NULL, &$error_message = NULL, int $flags = 12, $context = NULL", "" },` |
|         - |  976 | `	{ "stream_socket_accept", "$socket, ?float $timeout = NULL, &$peer_name = NULL", "" },` |
|         - |  977 | `	{ "stream_socket_get_name", "$socket, bool $remote", "string\|false" },` |
|         - |  978 | `	{ "stream_socket_pair", "int $domain, int $type, int $protocol", "array\|false" },` |
|         - |  979 | `	{ "stream_socket_shutdown", "$stream, int $mode", "bool" },` |
|         - |  980 | `	{ "stream_socket_recvfrom", "$socket, int $length, int $flags = 0, &$address = NULL", "string\|false" },` |
|         - |  981 | `	{ "stream_socket_sendto", "$socket, string $data, int $flags = 0, string $address = ''", "int\|false" },` |
|         - |  982 | `	{ "preg_match", "string $pattern, string $subject, &$matches = NULL, int $flags = 0, int $offset = 0", "int\|false" },` |
|         - |  983 | `	{ "preg_match_all", "string $pattern, string $subject, &$matches = NULL, int $flags = 0, int $offset = 0", "int\|false" },` |
|         - |  984 | `	{ "preg_quote", "string $str, ?string $delimiter = NULL", "string" },` |
|         - |  985 | `	{ "preg_replace", "array\|string $pattern, array\|string $replacement, array\|string $subject, int $limit = -1, &$count = NULL", "array\|string\|null" },` |
|         - |  986 | `	{ "preg_replace_callback", "array\|string $pattern, callable $callback, array\|string $subject, int $limit = -1, &$count = NULL, int $flags = 0", "array\|string\|null" },` |
|         - |  987 | `	{ "preg_split", "string $pattern, string $subject, int $limit = -1, int $flags = 0", "array\|false" },` |
|         - |  988 | `	{ "prev", "object\|array &$array", "mixed" },` |
|         - |  989 | `	{ "print_r", "mixed $value, bool $return = false", "string\|true" },` |
|         - |  990 | `	{ "printf", "string $format, mixed ...$values = ?", "int" },` |
|         - |  991 | `	{ "property_exists", "$object_or_class, string $property", "bool" },` |
|         - |  992 | `	{ "putenv", "string $assignment", "bool" },` |
|         - |  993 | `	{ "quotemeta", "string $string", "string" },` |
|         - |  994 | `	{ "rad2deg", "float $num", "float" },` |
|         - |  995 | `	{ "rand", "int $min = ?, int $max = ?", "int" },` |
|         - |  996 | `	{ "random_bytes", "int $length", "string" },` |
|         - |  997 | `	{ "random_int", "int $min, int $max", "int" },` |
|         - |  998 | `	{ "range", "string\|int\|float $start, string\|int\|float $end, int\|float $step = 1", "array" },` |
|         - |  999 | `	{ "rawurldecode", "string $string", "string" },` |
|         - | 1000 | `	{ "rawurlencode", "string $string", "string" },` |
|         - | 1001 | `	{ "readdir", "$dir_handle = NULL", "string\|false" },` |
|         - | 1002 | `	{ "readfile", "string $filename, bool $use_include_path = false, $context = NULL", "int\|false" },` |
|         - | 1003 | `	{ "readlink", "string $path", "string\|false" },` |
|         - | 1004 | `	{ "realpath", "string $path", "string\|false" },` |
|         - | 1005 | `	{ "stream_resolve_include_path", "string $filename", "string\|false" },` |
|         - | 1006 | `	{ "register_shutdown_function", "callable $callback, mixed ...$args = ?", "void" },` |
|         - | 1007 | `	{ "rename", "string $from, string $to, $context = NULL", "bool" },` |
|         - | 1008 | `	{ "reset", "object\|array &$array", "mixed" },` |
|         - | 1009 | `	{ "restore_error_handler", "", "true" },` |
|         - | 1010 | `	{ "restore_exception_handler", "", "true" },` |
|         - | 1011 | `	{ "rewind", "$stream", "bool" },` |
|         - | 1012 | `	{ "rewinddir", "$dir_handle = NULL", "void" },` |
|         - | 1013 | `	{ "rmdir", "string $directory, $context = NULL", "bool" },` |
|         - | 1014 | `	{ "round", "int\|float $num, int $precision = 0, RoundingMode\|int $mode = ?", "float" },` |
|         - | 1015 | `	{ "rsort", "array &$array, int $flags = 0", "true" },` |
|         - | 1016 | `	{ "rtrim", "string $string, string $characters = ?", "string" },` |
|         - | 1017 | `	{ "serialize", "mixed $value", "string" },` |
|         - | 1018 | `	{ "set_error_handler", "?callable $callback, int $error_levels = 30719", "" },` |
|         - | 1019 | `	{ "set_exception_handler", "?callable $callback", "" },` |
|         - | 1020 | `	{ "get_error_handler", "", "?callable" },` |
|         - | 1021 | `	{ "get_exception_handler", "", "?callable" },` |
|         - | 1022 | `	{ "hrtime", "bool $as_number = false", "array\|int\|float\|false" },` |
|         - | 1023 | `	{ "mb_check_encoding", "array\|string\|null $value = NULL, ?string $encoding = NULL", "bool" },` |
|         - | 1024 | `	{ "mb_convert_case", "string $string, int $mode, ?string $encoding = NULL", "string" },` |
|         - | 1025 | `	{ "mb_detect_encoding", "string $string, array\|string\|null $encodings = NULL, bool $strict = false", "string\|false" },` |
|         - | 1026 | `	{ "mb_internal_encoding", "?string $encoding = NULL", "string\|bool" },` |
|         - | 1027 | `	{ "mb_scrub", "string $string, ?string $encoding = null", "string" },` |
|         - | 1028 | `	{ "mb_substitute_character", "string\|int\|null $substitute_character = null", "string\|int\|bool" },` |
|         - | 1029 | `	{ "mb_str_split", "string $string, int $length = 1, ?string $encoding = NULL", "array" },` |
|         - | 1030 | `	{ "mb_stripos", "string $haystack, string $needle, int $offset = 0, ?string $encoding = NULL", "int\|false" },` |
|         - | 1031 | `	{ "mb_strlen", "string $string, ?string $encoding = NULL", "int" },` |
|         - | 1032 | `	{ "mb_strpos", "string $haystack, string $needle, int $offset = 0, ?string $encoding = NULL", "int\|false" },` |
|         - | 1033 | `	{ "mb_str_pad", "string $string, int $length, string $pad_string = \" \", int $pad_type = 1, ?string $encoding = null", "string" },` |
|         - | 1034 | `	{ "mb_strcut", "string $string, int $start, ?int $length = null, ?string $encoding = null", "string" },` |
|         - | 1035 | `	{ "mb_strimwidth", "string $string, int $start, int $width, string $trim_marker = \"\", ?string $encoding = null", "string" },` |
|         - | 1036 | `	{ "mb_strrchr", "string $haystack, string $needle, bool $before_needle = false, ?string $encoding = null", "string\|false" },` |
|         - | 1037 | `	{ "mb_strrichr", "string $haystack, string $needle, bool $before_needle = false, ?string $encoding = null", "string\|false" },` |
|         - | 1038 | `	{ "mb_strripos", "string $haystack, string $needle, int $offset = 0, ?string $encoding = null", "int\|false" },` |
|         - | 1039 | `	{ "mb_strrpos", "string $haystack, string $needle, int $offset = 0, ?string $encoding = NULL", "int\|false" },` |
|         - | 1040 | `	{ "mb_stristr", "string $haystack, string $needle, bool $before_needle = false, ?string $encoding = null", "string\|false" },` |
|         - | 1041 | `	{ "mb_strstr", "string $haystack, string $needle, bool $before_needle = false, ?string $encoding = null", "string\|false" },` |
|         - | 1042 | `	{ "mb_substr_count", "string $haystack, string $needle, ?string $encoding = null", "int" },` |
|         - | 1043 | `	{ "mb_strwidth", "string $string, ?string $encoding = NULL", "int" },` |
|         - | 1044 | `	{ "mb_substr", "string $string, int $start, ?int $length = NULL, ?string $encoding = NULL", "string" },` |
|         - | 1045 | `	{ "memory_reset_peak_usage", "", "void" },` |
|         - | 1046 | `	{ "proc_close", "$process", "int" },` |
|         - | 1047 | `	{ "proc_get_status", "$process", "array" },` |
|         - | 1048 | `	{ "proc_nice", "int $priority", "bool" },` |
|         - | 1049 | `	{ "proc_open", "array\|string $command, array $descriptor_spec, &$pipes, ?string $cwd = NULL, ?array $env_vars = NULL, ?array $options = NULL", "" },` |
|         - | 1050 | `	{ "proc_terminate", "$process, int $signal = 15", "bool" },` |
|         - | 1051 | `	{ "set_include_path", "string $include_path", "string\|false" },` |
|         - | 1052 | `	{ "setcookie", "string $name, string $value = '', array\|int $expires_or_options = 0, string $path = '', string $domain = '', bool $secure = false, bool $httponly = false", "bool" },` |
|         - | 1053 | `	{ "setrawcookie", "string $name, string $value = '', array\|int $expires_or_options = 0, string $path = '', string $domain = '', bool $secure = false, bool $httponly = false", "bool" },` |
|         - | 1054 | `	{ "settype", "mixed &$var, string $type", "bool" },` |
|         - | 1055 | `	{ "sha1", "string $string, bool $binary = false", "string" },` |
|         - | 1056 | `	{ "sha1_file", "string $filename, bool $binary = false", "string\|false" },` |
|         - | 1057 | `	{ "shell_exec", "string $command", "string\|false\|null" },` |
|         - | 1058 | `	{ "shuffle", "array &$array", "true" },` |
|         - | 1059 | `	{ "similar_text", "string $string1, string $string2, &$percent = NULL", "int" },` |
|         - | 1060 | `	{ "sin", "float $num", "float" },` |
|         - | 1061 | `	{ "sinh", "float $num", "float" },` |
|         - | 1062 | `	{ "sizeof", "Countable\|array $value, int $mode = 0", "int" },` |
|         - | 1063 | `	{ "sleep", "int $seconds", "int" },` |
|         - | 1064 | `	{ "sort", "array &$array, int $flags = 0", "true" },` |
|         - | 1065 | `	{ "soundex", "string $string", "string" },` |
|         - | 1066 | `	{ "spl_autoload", "string $class, ?string $file_extensions = NULL", "void" },` |
|         - | 1067 | `	{ "spl_autoload_functions", "", "array" },` |
|         - | 1068 | `	{ "spl_autoload_register", "?callable $callback = NULL, bool $throw = true, bool $prepend = false", "bool" },` |
|         - | 1069 | `	{ "spl_autoload_unregister", "callable $callback", "bool" },` |
|         - | 1070 | `	{ "spl_object_hash", "object $object", "string" },` |
|         - | 1071 | `	{ "spl_object_id", "object $object", "int" },` |
|         - | 1072 | `	{ "sprintf", "string $format, mixed ...$values = ?", "string" },` |
|         - | 1073 | `	{ "sqrt", "float $num", "float" },` |
|         - | 1074 | `	{ "srand", "?int $seed = NULL, int $mode = 0", "void" },` |
|         - | 1075 | `	{ "stat", "string $filename", "array\|false" },` |
|         - | 1076 | `	{ "str_contains", "string $haystack, string $needle", "bool" },` |
|         - | 1077 | `	{ "str_ends_with", "string $haystack, string $needle", "bool" },` |
|         - | 1078 | `	{ "str_getcsv", "string $string, string $separator = ',', string $enclosure = '\"', string $escape = '\\\\'", "array" },` |
|         - | 1079 | `	{ "str_ireplace", "array\|string $search, array\|string $replace, array\|string $subject, &$count = NULL", "array\|string" },` |
|         - | 1080 | `	{ "str_pad", "string $string, int $length, string $pad_string = ' ', int $pad_type = 1", "string" },` |
|         - | 1081 | `	{ "str_repeat", "string $string, int $times", "string" },` |
|         - | 1082 | `	{ "str_replace", "array\|string $search, array\|string $replace, array\|string $subject, &$count = NULL", "array\|string" },` |
|         - | 1083 | `	{ "str_rot13", "string $string", "string" },` |
|         - | 1084 | `	{ "str_shuffle", "string $string", "string" },` |
|         - | 1085 | `	{ "str_split", "string $string, int $length = 1", "array" },` |
|         - | 1086 | `	{ "str_starts_with", "string $haystack, string $needle", "bool" },` |
|         - | 1087 | `	{ "str_word_count", "string $string, int $format = 0, ?string $characters = NULL", "array\|int" },` |
|         - | 1088 | `	{ "strcasecmp", "string $string1, string $string2", "int" },` |
|         - | 1089 | `	{ "strchr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|         - | 1090 | `	{ "strcmp", "string $string1, string $string2", "int" },` |
|         - | 1091 | `	{ "strnatcasecmp", "string $string1, string $string2", "int" },` |
|         - | 1092 | `	{ "strnatcmp", "string $string1, string $string2", "int" },` |
|         - | 1093 | `	{ "strcoll", "string $string1, string $string2", "int" },` |
|         - | 1094 | `	{ "strcspn", "string $string, string $characters, int $offset = 0, ?int $length = NULL", "int" },` |
|         - | 1095 | `	{ "stream_context_create", "?array $options = NULL, ?array $params = NULL", "" },` |
|         - | 1096 | `	{ "stream_context_get_options", "$stream_or_context", "array" },` |
|         - | 1097 | ``	/* php's argument #2 is `array\|string $wrapper_or_options` and the array form`` |
|         - | 1098 | `	 * — the two-argument spelling — is DEPRECATED in 8.3; §10 refuses what php` |
|         - | 1099 | `	 * deprecates, so this row declares the string and the whole-array form is` |
|         - | 1100 | `	 * spelled stream_context_set_options(). */` |
|         - | 1101 | `	{ "stream_context_set_option", "$context, string $wrapper_name, string $option_name, mixed $value", "bool" },` |
|         - | 1102 | `	{ "stream_context_set_options", "$context, array $options", "bool" },` |
|         - | 1103 | `	{ "stream_context_get_params", "$stream_or_context", "array" },` |
|         - | 1104 | `	{ "stream_context_set_params", "$context, array $params", "bool" },` |
|         - | 1105 | `	{ "stream_context_get_default", "?array $options = NULL", "" },` |
|         - | 1106 | `	{ "stream_context_set_default", "array $options", "" },` |
|         - | 1107 | `	{ "stream_get_contents", "$stream, ?int $length = NULL, int $offset = -1", "string\|false" },` |
|         - | 1108 | `	{ "stream_get_line", "$stream, int $length, string $ending = ''", "string\|false" },` |
|         - | 1109 | `	{ "socket_get_status", "$stream", "array" },` |
|         - | 1110 | `	{ "stream_get_meta_data", "$stream", "array" },` |
|         - | 1111 | `	{ "stream_copy_to_stream", "$from, $to, ?int $length = NULL, int $offset = 0", "int\|false" },` |
|         - | 1112 | `	{ "stream_get_transports", "", "array" },` |
|         - | 1113 | `	{ "stream_is_local", "$stream", "bool" },` |
|         - | 1114 | `	{ "stream_select", "?array &$read, ?array &$write, ?array &$except, ?int $seconds, ?int $microseconds = NULL", "int\|false" },` |
|         - | 1115 | `	{ "stream_set_blocking", "$stream, bool $enable", "bool" },` |
|         - | 1116 | `	{ "socket_set_blocking", "$stream, bool $enable", "bool" },` |
|         - | 1117 | `	{ "stream_set_chunk_size", "$stream, int $size", "int" },` |
|         - | 1118 | `	{ "stream_set_read_buffer", "$stream, int $size", "int" },` |
|         - | 1119 | `	{ "stream_set_timeout", "$stream, int $seconds, int $microseconds = 0", "bool" },` |
|         - | 1120 | `	{ "stream_set_write_buffer", "$stream, int $size", "int" },` |
|         - | 1121 | `	{ "set_file_buffer", "$stream, int $size", "int" },` |
|         - | 1122 | `	{ "stream_supports_lock", "$stream", "bool" },` |
|         - | 1123 | `	{ "stream_get_wrappers", "", "array" },` |
|         - | 1124 | `	{ "stream_get_filters", "", "array" },` |
|         - | 1125 | `	{ "stream_filter_append", "$stream, string $filter_name, int $mode = 0, mixed $params = NULL", "" },` |
|         - | 1126 | `	{ "stream_filter_prepend", "$stream, string $filter_name, int $mode = 0, mixed $params = NULL", "" },` |
|         - | 1127 | `	{ "stream_filter_remove", "$stream_filter", "bool" },` |
|         - | 1128 | `	{ "stream_filter_register", "string $filter_name, string $class", "bool" },` |
|         - | 1129 | `	{ "stream_bucket_make_writeable", "$brigade", "?StreamBucket" },` |
|         - | 1130 | `	{ "stream_bucket_append", "$brigade, StreamBucket $bucket", "void" },` |
|         - | 1131 | `	{ "stream_bucket_prepend", "$brigade, StreamBucket $bucket", "void" },` |
|         - | 1132 | `	{ "stream_bucket_new", "$stream, string $buffer", "StreamBucket" },` |
|         - | 1133 | `	{ "stream_register_wrapper", "string $protocol, string $class, int $flags = 0", "bool" },` |
|         - | 1134 | `	{ "stream_wrapper_register", "string $protocol, string $class, int $flags = 0", "bool" },` |
|         - | 1135 | `	{ "stream_wrapper_unregister", "string $protocol", "bool" },` |
|         - | 1136 | `	{ "stream_wrapper_restore", "string $protocol", "bool" },` |
|         - | 1137 | `	{ "strip_tags", "string $string, array\|string\|null $allowed_tags = NULL", "string" },` |
|         - | 1138 | `	{ "stripos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|         - | 1139 | `	{ "stripslashes", "string $string", "string" },` |
|         - | 1140 | `	{ "stristr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|         - | 1141 | `	{ "strlen", "string $string", "int" },` |
|         - | 1142 | `	{ "strncasecmp", "string $string1, string $string2, int $length", "int" },` |
|         - | 1143 | `	{ "strncmp", "string $string1, string $string2, int $length", "int" },` |
|         - | 1144 | `	{ "strpbrk", "string $string, string $characters", "string\|false" },` |
|         - | 1145 | `	{ "strpos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|         - | 1146 | `	{ "strrchr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|         - | 1147 | `	{ "strrev", "string $string", "string" },` |
|         - | 1148 | `	{ "strripos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|         - | 1149 | `	{ "strrpos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|         - | 1150 | `	{ "strspn", "string $string, string $characters, int $offset = 0, ?int $length = NULL", "int" },` |
|         - | 1151 | `	{ "strstr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|         - | 1152 | `	{ "strtok", "string $string, ?string $token = NULL", "string\|false" },` |
|         - | 1153 | `	{ "strtolower", "string $string", "string" },` |
|         - | 1154 | `	{ "strtotime", "string $datetime, ?int $baseTimestamp = NULL", "int\|false" },` |
|         - | 1155 | `	{ "strtoupper", "string $string", "string" },` |
|         - | 1156 | `	{ "strtr", "string $string, array\|string $from, ?string $to = NULL", "string" },` |
|         - | 1157 | `	{ "strval", "mixed $value", "string" },` |
|         - | 1158 | `	{ "substr", "string $string, int $offset, ?int $length = NULL", "string" },` |
|         - | 1159 | `	{ "substr_compare", "string $haystack, string $needle, int $offset, ?int $length = NULL, bool $case_insensitive = false", "int" },` |
|         - | 1160 | `	{ "substr_count", "string $haystack, string $needle, int $offset = 0, ?int $length = NULL", "int" },` |
|         - | 1161 | `	{ "substr_replace", "array\|string $string, array\|string $replace, array\|int $offset, array\|int\|null $length = NULL", "array\|string" },` |
|         - | 1162 | `	{ "symlink", "string $target, string $link", "bool" },` |
|         - | 1163 | `	{ "sys_get_temp_dir", "", "string" },` |
|         - | 1164 | `	{ "system", "string $command, &$result_code = NULL", "string\|false" },` |
|         - | 1165 | `	{ "tan", "float $num", "float" },` |
|         - | 1166 | `	{ "tanh", "float $num", "float" },` |
|         - | 1167 | `	{ "time", "", "int" },` |
|         - | 1168 | `	{ "token_get_all", "string $code, int $flags = 0", "array" },` |
|         - | 1169 | `	{ "token_name", "int $id", "string" },` |
|         - | 1170 | `	{ "touch", "string $filename, ?int $mtime = NULL, ?int $atime = NULL", "bool" },` |
|         - | 1171 | `	{ "trigger_error", "string $message, int $error_level = 1024", "true" },` |
|         - | 1172 | `	{ "trim", "string $string, string $characters = ?", "string" },` |
|         - | 1173 | `	{ "uasort", "array &$array, callable $callback", "true" },` |
|         - | 1174 | `	{ "ucfirst", "string $string", "string" },` |
|         - | 1175 | `	{ "ucwords", "string $string, string $separators = ?", "string" },` |
|         - | 1176 | `	{ "uksort", "array &$array, callable $callback", "true" },` |
|         - | 1177 | `	{ "umask", "?int $mask = NULL", "int" },` |
|         - | 1178 | `	{ "uniqid", "string $prefix = '', bool $more_entropy = false", "string" },` |
|         - | 1179 | `	{ "unixtojd", "?int $timestamp = NULL", "int\|false" },` |
|         - | 1180 | `	{ "unlink", "string $filename, $context = NULL", "bool" },` |
|         - | 1181 | `	{ "unserialize", "string $data, array $options = ?", "mixed" },` |
|         - | 1182 | `	{ "urldecode", "string $string", "string" },` |
|         - | 1183 | `	{ "urlencode", "string $string", "string" },` |
|         - | 1184 | `	{ "user_error", "string $message, int $error_level = 1024", "true" },` |
|         - | 1185 | `	{ "usleep", "int $microseconds", "void" },` |
|         - | 1186 | `	{ "usort", "array &$array, callable $callback", "true" },` |
|         - | 1187 | `	{ "var_dump", "mixed $value, mixed ...$values = ?", "void" },` |
|         - | 1188 | `	{ "var_export", "mixed $value, bool $return = false", "?string" },` |
|         - | 1189 | `	{ "version_compare", "string $version1, string $version2, ?string $operator = null", "int\|bool" },` |
|         - | 1190 | `	{ "vfprintf", "$stream, string $format, array $values", "int" },` |
|         - | 1191 | `	{ "vprintf", "string $format, array $values", "int" },` |
|         - | 1192 | `	{ "vsprintf", "string $format, array $values", "string" },` |
|         - | 1193 | `	{ "wordwrap", "string $string, int $width = 75, string $break = ?, bool $cut_long_words = false", "string" },` |
|         - | 1194 | `	{ "zip_close", "$zip", "void" },` |
|         - | 1195 | `	{ "zip_entry_close", "$zip_entry", "bool" },` |
|         - | 1196 | `	{ "zip_entry_compressedsize", "$zip_entry", "int\|false" },` |
|         - | 1197 | `	{ "zip_entry_compressionmethod", "$zip_entry", "string\|false" },` |
|         - | 1198 | `	{ "zip_entry_filesize", "$zip_entry", "int\|false" },` |
|         - | 1199 | `	{ "zip_entry_name", "$zip_entry", "string\|false" },` |
|         - | 1200 | `	{ "zip_entry_open", "$zip_dp, $zip_entry, string $mode = 'rb'", "bool" },` |
|         - | 1201 | `	{ "zip_entry_read", "$zip_entry, int $len = 1024", "string\|false" },` |
|         - | 1202 | `	{ "zip_open", "string $filename", "" },` |
|         - | 1203 | `	{ "zip_read", "$zip", "" },` |
|         - | 1204 | `};` |
|         - | 1205 | `/*` |
|         - | 1206 | ` * Stamp the signature strings onto the registered host functions.` |
|         - | 1207 | ` * Runs once at PH7_VmMakeReady, after the builtins are registered.` |
|         - | 1208 | ` */` |
|         - | 1209 | `/*` |
|         - | 1210 | ` * Derive the minimum arity from a builtin's declared signature: a parameter is` |
|         - | 1211 | ` * optional when it carries a default ("int $length = 76") or is variadic` |
|         - | 1212 | ` * ("...$rest"), so the minimum is the count of the parameters that are neither,` |
|         - | 1213 | ` * and the ZPP wording is "at least" as soon as one optional/variadic exists.` |
|         - | 1214 | ` *` |
|         - | 1215 | ` * This makes aBuiltinSig[] the single source of truth for arity — the curated` |
|         - | 1216 | ` * aBuiltinArity[] table above stays as an OVERRIDE for the handful of builtins` |
|         - | 1217 | ` * php reports differently from their own signature (e.g. strtr(), whose 3-arg` |
|         - | 1218 | ` * form still reports "expects exactly 2", and the array_udiff() family, whose` |
|         - | 1219 | ` * variadic tail hides a second required argument). Verified against php 8.5.7` |
|         - | 1220 | ` * for all 462 signed builtins: 458 derive exactly, 4 are overridden.` |
|         - | 1221 | ` */` |
|         - | 1222 | `/*` |
|         - | 1223 | ` * A DEFAULT can contain the parameter separator: php declares` |
|         - | 1224 | `` * `string $separator = ','` and `string $enclosure = '"'`. Every scan of a`` |
|         - | 1225 | ` * signature therefore has to step over a quoted run, or the comma inside one` |
|         - | 1226 | ` * splits the parameter in two — which is how fgetcsv()/fputcsv()/str_getcsv()` |
|         - | 1227 | ` * came to count SIX parameters and accept a fifth argument php refuses.` |
|         - | 1228 | ` * Answers the position of the closing quote (or of the NUL when the run is` |
|         - | 1229 | ` * unterminated); the caller advances past it.` |
|         - | 1230 | ` */` |
|   2271289 | 1231 | `static const char *VmSigSkipQuoted(const char *zCur)` |
|         5 | 1232 | `{` |
|   2271294 | 1233 | `	char c = zCur[0];` |
|   2271294 | 1234 | `	if( c != '\'' && c != '"' ){` |
|       ! 0 | 1235 | `		return zCur;` |
|         - | 1236 | `	}` |
|   3540268 | 1237 | `	for( zCur++ ; zCur[0] ; zCur++ ){` |
|   3540268 | 1238 | `		if( zCur[0] == '\\' && zCur[1] ){` |
|    110445 | 1239 | `			zCur++;` |
|    110445 | 1240 | `			continue;` |
|         - | 1241 | `		}` |
|   3429828 | 1242 | `		if( zCur[0] == c ){` |
|   2271294 | 1243 | `			break;` |
|         - | 1244 | `		}` |
|    579272 | 1245 | `	}` |
|   2271294 | 1246 | `	return zCur;` |
|   1135649 | 1247 | `}` |
|  10415838 | 1248 | `PH7_PRIVATE void VmDeriveArityFromSig(const char *zSig,sxi16 *pnMin,sxu8 *pbAtLeast,sxi16 *pnMax,sxu8 *pbHasMax)` |
|         5 | 1249 | `{` |
|  10415843 | 1250 | `	const char *zCur = zSig;` |
|  10415843 | 1251 | `	int nMin = 0, bAtLeast = 0, bSeen = 0, bOptional = 0;` |
|  10415843 | 1252 | `	int nTotal = 0, bVariadic = 0;` |
| 107545660 | 1253 | `	for(;;){` |
| 221034223 | 1254 | `		if( zCur[0] == '\'' \|\| zCur[0] == '"' ){` |
|    401719 | 1255 | `			bSeen = 1;` |
|    401719 | 1256 | `			zCur = VmSigSkipQuoted(zCur);` |
|    401719 | 1257 | `			if( zCur[0] != '\0' ){` |
|    401719 | 1258 | `				zCur++;` |
|    200857 | 1259 | `			}` |
|    401719 | 1260 | `			continue;` |
|         - | 1261 | `		}` |
| 220632509 | 1262 | `		if( zCur[0] == '\0' \|\| zCur[0] == ',' ){` |
|  15957027 | 1263 | `			if( bSeen ){` |
|  12166835 | 1264 | `				nTotal++;` |
|  12166835 | 1265 | `				if( bOptional ){` |
|   4173147 | 1266 | `					bAtLeast = 1;` |
|   2086576 | 1267 | `				}else{` |
|   7993693 | 1268 | `					nMin++;` |
|         - | 1269 | `				}` |
|   6083415 | 1270 | `			}` |
|  15957027 | 1271 | `			if( zCur[0] == '\0' ){` |
|  10415843 | 1272 | `				break;` |
|         - | 1273 | `			}` |
|   5541189 | 1274 | `			bSeen = bOptional = 0;` |
|   5541189 | 1275 | `			zCur++;` |
|   5541189 | 1276 | `			continue;` |
|         - | 1277 | `		}` |
| 204675487 | 1278 | `		if( zCur[0] != ' ' ){` |
| 179781293 | 1279 | `			bSeen = 1;` |
|  89890644 | 1280 | `		}` |
| 204675487 | 1281 | `		if( zCur[0] == '=' \|\| (zCur[0] == '.' && zCur[1] == '.' && zCur[2] == '.') ){` |
|   4356741 | 1282 | `			bOptional = 1;` |
|   2178368 | 1283 | `		}` |
| 204675487 | 1284 | `		if( zCur[0] == '.' && zCur[1] == '.' && zCur[2] == '.' ){` |
|    321359 | 1285 | `			bVariadic = 1;` |
|    160677 | 1286 | `		}` |
| 204675487 | 1287 | `		zCur++;` |
|         5 | 1288 | `	}` |
|  10415843 | 1289 | `	*pnMin = (sxi16)nMin;` |
|  10415843 | 1290 | `	*pbAtLeast = (sxu8)bAtLeast;` |
|         - | 1291 | `	/* php enforces a MAXIMUM too ("expects at most 1 argument, 2 given"); a` |
|         - | 1292 | `	 * variadic tail means there is none. The parameter COUNT is the maximum,` |
|         - | 1293 | `	 * whether or not the parameters carry defaults. */` |
|  10415843 | 1294 | `	*pnMax = (sxi16)nTotal;` |
|  10415843 | 1295 | `	*pbHasMax = (sxu8)(bVariadic ? 0 : 1);` |
|  10415843 | 1296 | `}` |
|         - | 1297 | `/*` |
|         - | 1298 | ` * Does the declared type list (e.g. "array\|string", "?int", "callable") contain` |
|         - | 1299 | ` * the given token? Compares against each '\|'-separated alternative, ignoring a` |
|         - | 1300 | ` * leading nullable '?'.` |
|         - | 1301 | ` */` |
|  23606520 | 1302 | `static int VmSigTypeHas(const char *zType,int nType,const char *zTok)` |
|         5 | 1303 | `{` |
|  23606525 | 1304 | `	int nTok = (int)SyStrlen(zTok);` |
|  23606525 | 1305 | `	int i = 0;` |
|  23606525 | 1306 | `	if( zType[0] == '?' ){` |
|   2557703 | 1307 | `		zType++;` |
|   2557703 | 1308 | `		nType--;` |
|   1279203 | 1309 | `	}` |
|  46190766 | 1310 | `	while( i < nType ){` |
|  24414857 | 1311 | `		int j = i;` |
| 154923821 | 1312 | `		while( j < nType && zType[j] != '\|' ){` |
| 130508969 | 1313 | `			j++;` |
|         5 | 1314 | `		}` |
|  24414857 | 1315 | `		if( j - i == nTok && SyMemcmp(&zType[i],zTok,(sxu32)nTok) == 0 ){` |
|   1830616 | 1316 | `			return 1;` |
|         - | 1317 | `		}` |
|  22584246 | 1318 | `		i = j + 1;` |
|         5 | 1319 | `	}` |
|  21775914 | 1320 | `	return 0;` |
|  11809194 | 1321 | `}` |
|         - | 1322 | `/*` |
|         - | 1323 | `` * Is EVERY arm of the declared type list `array` (a bare `array`, or `?array`,`` |
|         - | 1324 | `` * or the `array\|null` union that spells the same thing)? Such a parameter has`` |
|         - | 1325 | ` * no arm a scalar can satisfy, and php refuses one outright.` |
|         - | 1326 | ` *` |
|         - | 1327 | `` * The screen used to exempt any type list carrying an `array` arm, union or`` |
|         - | 1328 | `` * not, for a wording reason: php's `array\|object` parameters come from ONE ZPP`` |
|         - | 1329 | ` * macro (Z_PARAM_ARRAY_OR_OBJECT) that names only "array" in the refusal, so` |
|         - | 1330 | ` * the declared type is not the text php prints. That ambiguity does not exist` |
|         - | 1331 | `` * for a parameter typed exactly `array` -- there is one arm and php prints it.`` |
|         - | 1332 | ` */` |
|   6680009 | 1333 | `static int VmSigTypeIsArrayOnly(const char *zType,int nType)` |
|         5 | 1334 | `{` |
|   6680014 | 1335 | `	int i = 0, bArray = 0;` |
|   6680014 | 1336 | `	if( zType[0] == '?' ){` |
|    860817 | 1337 | `		zType++;` |
|    860817 | 1338 | `		nType--;` |
|    430583 | 1339 | `	}` |
|   6874722 | 1340 | `	while( i < nType ){` |
|   6874484 | 1341 | `		int j = i;` |
|  40511332 | 1342 | `		while( j < nType && zType[j] != '\|' ){` |
|  33636853 | 1343 | `			j++;` |
|         5 | 1344 | `		}` |
|   6874484 | 1345 | `		if( j > i ){` |
|   6874479 | 1346 | `			if( j - i == (int)sizeof("array")-1` |
|   3536743 | 1347 | `			 && SyMemcmp(&zType[i],"array",sizeof("array")-1) == 0 ){` |
|    194713 | 1348 | `				bArray = 1;` |
|   6788007 | 1349 | `			}else if( !(j - i == (int)sizeof("null")-1` |
|   3352526 | 1350 | `			         && SyMemcmp(&zType[i],"null",sizeof("null")-1) == 0) ){` |
|   6679776 | 1351 | `				return 0;` |
|         - | 1352 | `			}` |
|     97354 | 1353 | `		}` |
|    194713 | 1354 | `		i = j + 1;` |
|         5 | 1355 | `	}` |
|       243 | 1356 | `	return bArray;` |
|   3341773 | 1357 | `}` |
|         - | 1358 | `/*` |
|         - | 1359 | ` * Does the declared type list name a CLASS (anything that is not one of php's` |
|         - | 1360 | ` * builtin type keywords)? A class-typed parameter accepts an object, so it must` |
|         - | 1361 | ` * not be rejected by the array/object/resource screen below.` |
|         - | 1362 | ` */` |
|         - | 1363 | `/* Is this one arm of a declared type a BUILTIN type name rather than a class? */` |
|   6923053 | 1364 | `static int VmSigArmIsBuiltinType(const char *zArm,int nArm)` |
|         5 | 1365 | `{` |
|         - | 1366 | `	static const char *azBuiltin[] = {` |
|         - | 1367 | `		"int","float","string","bool","array","object","callable","iterable",` |
|         - | 1368 | `		"mixed","null","void","resource","false","true","never","self","static"` |
|         - | 1369 | `	};` |
|         - | 1370 | `	int k;` |
|  17742619 | 1371 | `	for( k = 0 ; k < (int)SX_ARRAYSIZE(azBuiltin) ; ++k ){` |
|  17717981 | 1372 | `		int nB = (int)SyStrlen(azBuiltin[k]);` |
|  17717981 | 1373 | `		if( nArm == nB && SyMemcmp(zArm,azBuiltin[k],(sxu32)nB) == 0 ){` |
|   6898420 | 1374 | `			return 1;` |
|         - | 1375 | `		}` |
|   5412020 | 1376 | `	}` |
|     24643 | 1377 | `	return 0;` |
|   3463307 | 1378 | `}` |
|   6713659 | 1379 | `static int VmSigTypeHasClass(const char *zType,int nType)` |
|         5 | 1380 | `{` |
|   6713664 | 1381 | `	int i = 0;` |
|   6713664 | 1382 | `	if( zType[0] == '?' ){` |
|    866497 | 1383 | `		zType++;` |
|    866497 | 1384 | `		nType--;` |
|    433423 | 1385 | `	}` |
|  13612071 | 1386 | `	while( i < nType ){` |
|   6915090 | 1387 | `		int j = i;` |
|  40862254 | 1388 | `		while( j < nType && zType[j] != '\|' ){` |
|  33947169 | 1389 | `			j++;` |
|         5 | 1390 | `		}` |
|   6915090 | 1391 | `		if( j > i && !VmSigArmIsBuiltinType(&zType[i],j - i) ){` |
|     16683 | 1392 | `			return 1;` |
|         - | 1393 | `		}` |
|   6898412 | 1394 | `		i = j + 1;` |
|         5 | 1395 | `	}` |
|   6696986 | 1396 | `	return 0;` |
|   3358606 | 1397 | `}` |
|         - | 1398 | `/*` |
|         - | 1399 | ` * php's ", X given" tail: like ph7_type_name() but an object reports its CLASS,` |
|         - | 1400 | ` * which is what php prints in a TypeError.` |
|         - | 1401 | ` */` |
|         - | 1402 | `/*` |
|         - | 1403 | ` * Does pObj satisfy any CLASS arm of a declared type?` |
|         - | 1404 | ` *` |
|         - | 1405 | ` * Answers TRUE (unscreened) when an arm names something this VM has not declared:` |
|         - | 1406 | ` * the signatures describe php's surface, parts of which PHL models differently` |
|         - | 1407 | ` * (the resource-backed handles the RES branch below already excuses), and a name` |
|         - | 1408 | ` * that resolves to nothing must not turn into a rejection of a valid argument.` |
|         - | 1409 | ` */` |
|      7958 | 1410 | `static int VmSigObjSatisfiesClass(ph7_vm *pVm,const char *zType,int nType,` |
|         - | 1411 | `	ph7_class_instance *pObj)` |
|         5 | 1412 | `{` |
|      7963 | 1413 | `	int i = 0;` |
|      7963 | 1414 | `	if( pObj == 0 \|\| pObj->pClass == 0 ){` |
|       ! 0 | 1415 | `		return 1;` |
|         - | 1416 | `	}` |
|      7963 | 1417 | `	if( zType[0] == '?' ){` |
|      1892 | 1418 | `		zType++;` |
|      1892 | 1419 | `		nType--;` |
|       944 | 1420 | `	}` |
|      8029 | 1421 | `	while( i < nType ){` |
|      7973 | 1422 | `		int j = i;` |
|     91899 | 1423 | `		while( j < nType && zType[j] != '\|' ){` |
|     83931 | 1424 | `			j++;` |
|         5 | 1425 | `		}` |
|      7973 | 1426 | `		if( j > i && !VmSigArmIsBuiltinType(&zType[i],j - i) ){` |
|      7965 | 1427 | `			ph7_class *pClass = PH7_VmExtractClass(&(*pVm),&zType[i],(sxu32)(j - i),FALSE,0);` |
|      7965 | 1428 | `			if( pClass == 0 ){` |
|         - | 1429 | `				/* Either a builtin type name (already excluded by the caller) or a` |
|         - | 1430 | `				 * class this build does not declare: nothing to judge. */` |
|       ! 0 | 1431 | `				return 1;` |
|         - | 1432 | `			}` |
|      7965 | 1433 | `			if( PH7_VmInstanceOf(pObj->pClass,pClass) ){` |
|      7907 | 1434 | `				return 1;` |
|         - | 1435 | `			}` |
|        29 | 1436 | `		}` |
|        69 | 1437 | `		i = j + 1;` |
|         3 | 1438 | `	}` |
|        59 | 1439 | `	return 0;` |
|      3988 | 1440 | `}` |
|       140 | 1441 | `static const char * VmArgTypeName(ph7_value *pVal)` |
|         5 | 1442 | `{` |
|       145 | 1443 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       145 | 1444 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       145 | 1445 | `		if( pInst && pInst->pClass ){` |
|       145 | 1446 | `			return pInst->pClass->sName.zString;` |
|         - | 1447 | `		}` |
|       ! 0 | 1448 | `	}` |
|       ! 0 | 1449 | `	return ph7_type_name(pVal);` |
|        75 | 1450 | `}` |
|         - | 1451 | `/*` |
|         - | 1452 | `` * Does pArg satisfy a `string` parameter? The rule VmEnforceBuiltinArgTypes()`` |
|         - | 1453 | ` * below applies, factored out so a builtin that words its own overload dispatch` |
|         - | 1454 | ` * (strtr(), whose expected type depends on the ARITY and so cannot be spelled in` |
|         - | 1455 | ` * one signature) decides identically instead of forking the logic. An array never` |
|         - | 1456 | ` * satisfies one; an object does only through __toString(); a resource does not;` |
|         - | 1457 | ` * null does under php, with a deprecation, but not under PHL's §10 null-strictness` |
|         - | 1458 | ` * policy — the screen and this helper both report it as a mismatch.` |
|         - | 1459 | ` */` |
|     46012 | 1460 | `PH7_PRIVATE int PH7_ArgSatisfiesString(ph7_value *pArg)` |
|         5 | 1461 | `{` |
|     46017 | 1462 | `	if( (pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_NULL\|MEMOBJ_RES)) != 0 ){` |
|        35 | 1463 | `		return 0;` |
|         - | 1464 | `	}` |
|     45985 | 1465 | `	if( (pArg->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       142 | 1466 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|       142 | 1467 | `		return pInst && PH7_ClassExtractMethod(pInst->pClass,"__toString",` |
|        69 | 1468 | `			sizeof("__toString")-1) != 0;` |
|         - | 1469 | `	}` |
|     45847 | 1470 | `	return 1;` |
|     23011 | 1471 | `}` |
|         - | 1472 | `/*` |
|         - | 1473 | `` * Is the declared type exactly `int` — the only shape whose float argument the`` |
|         - | 1474 | `` * screen below can decide? A union with a `float`, `string` or `bool` arm has its`` |
|         - | 1475 | ` * own coercion rules per arm (and php words those refusals from the builtin), so` |
|         - | 1476 | ` * only the plain form and its nullable spelling qualify.` |
|         - | 1477 | ` */` |
|      1256 | 1478 | `static int VmSigTypeIsIntOnly(const char *zType,int nType)` |
|         5 | 1479 | `{` |
|      1261 | 1480 | `	if( nType > 0 && zType[0] == '?' ){` |
|       128 | 1481 | `		zType++;` |
|       128 | 1482 | `		nType--;` |
|        62 | 1483 | `	}` |
|      1261 | 1484 | `	if( nType == (int)sizeof("int")-1 && SyMemcmp(zType,"int",3) == 0 ){` |
|       161 | 1485 | `		return 1;` |
|         - | 1486 | `	}` |
|         - | 1487 | ``	/* `int\|null` / `null\|int`, the union spelling of `?int`. */`` |
|      1389 | 1488 | `	return VmSigTypeHas(zType,nType,"int") && VmSigTypeHas(zType,nType,"null")` |
|       285 | 1489 | `	    && !VmSigTypeHas(zType,nType,"float")` |
|         2 | 1490 | `	    && !VmSigTypeHas(zType,nType,"string")` |
|         1 | 1491 | `	    && !VmSigTypeHas(zType,nType,"bool")` |
|       ! 0 | 1492 | `	    && !VmSigTypeHas(zType,nType,"array")` |
|       ! 0 | 1493 | `	    && !VmSigTypeHas(zType,nType,"object")` |
|       ! 0 | 1494 | `	    && !VmSigTypeHas(zType,nType,"iterable")` |
|       ! 0 | 1495 | `	    && !VmSigTypeHas(zType,nType,"callable")` |
|      1384 | 1496 | `	    && !VmSigTypeHasClass(zType,nType);` |
|       635 | 1497 | `}` |
|         - | 1498 | `/*` |
|         - | 1499 | `` * Can this float reach an `int` parameter without losing anything? php's rule is`` |
|         - | 1500 | ` * php_parse_arg_long's: in range, and integral. NaN and the infinities are out by` |
|         - | 1501 | ` * the range test (a NaN compares false against both bounds, which is why the test` |
|         - | 1502 | ` * is written as a pair of accepts rather than a pair of rejects).` |
|         - | 1503 | ` */` |
|       104 | 1504 | `static int VmDoubleFitsInt(double d)` |
|         5 | 1505 | `{` |
|       109 | 1506 | `	if( !PH7_RealFitsInt64(d) ){` |
|        51 | 1507 | `		return 0;` |
|         - | 1508 | `	}` |
|        61 | 1509 | `	return d == (double)(sxi64)d;` |
|        57 | 1510 | `}` |
|         - | 1511 | `/*` |
|         - | 1512 | ` * The same question for a NUMERIC string, which php asks with the same answer:` |
|         - | 1513 | `` * `dechex("1e19")` and `dechex("99999999999999999999")` are both`` |
|         - | 1514 | `` * `must be of type int, string given`. RangeStrToNumber is php's`` |
|         - | 1515 | ` * is_numeric_string grammar and already reclassifies an integer too wide for an` |
|         - | 1516 | ` * sxi64 as a DOUBLE, so the two shapes converge on one test.` |
|         - | 1517 | ` */` |
|        76 | 1518 | `static int VmNumStrFitsInt(ph7_value *pArg)` |
|         4 | 1519 | `{` |
|         - | 1520 | `	const char *zStr;` |
|        80 | 1521 | `	int nLen = 0;` |
|        80 | 1522 | `	sxi64 iVal = 0;` |
|        80 | 1523 | `	double dVal = 0;` |
|        80 | 1524 | `	zStr = ph7_value_to_string(pArg,&nLen);` |
|        80 | 1525 | `	switch( RangeStrToNumber(zStr,(sxu32)nLen,&iVal,&dVal) ){` |
|        55 | 1526 | `	case RANGE_IN_LONG:   return 1;` |
|        27 | 1527 | `	case RANGE_IN_DOUBLE: return VmDoubleFitsInt(dVal);` |
|       ! 0 | 1528 | `	default:              return 0;` |
|         - | 1529 | `	}` |
|        42 | 1530 | `}` |
|         - | 1531 | `/*` |
|         - | 1532 | ` * PHP-8 PATH parameters: which positions carry a filesystem path, a shell` |
|         - | 1533 | ` * command or an include-path list rather than an ordinary string.` |
|         - | 1534 | ` *` |
|         - | 1535 | ` * php spells this in the ZPP macro, not in the declared type: a path parameter` |
|         - | 1536 | `` * is `Z_PARAM_PATH` where an ordinary one is `Z_PARAM_STR`, and both print as`` |
|         - | 1537 | `` * `string` in the stub Reflection reads. The difference is a single rule — a`` |
|         - | 1538 | ` * path may not contain a NUL byte — and php raises a catchable ValueError for` |
|         - | 1539 | ` * one that does, BEFORE the call reaches the filesystem.` |
|         - | 1540 | ` *` |
|         - | 1541 | ` * PHL had no such notion, so every one of these arguments went to the C API as` |
|         - | 1542 | ` * a NUL-terminated string and was silently TRUNCATED at the NUL. That is not a` |
|         - | 1543 | ` * missing diagnostic: the truncated path is a DIFFERENT path, and the builtin` |
|         - | 1544 | `` * then operated on it. `unlink("$dir/x\0.png")` deleted `$dir/x`,`` |
|         - | 1545 | `` * `file_put_contents("$dir/x\0.txt",$d)` wrote it, `touch`/`chmod`/`copy`/`` |
|         - | 1546 | ``  * `rename`/`symlink`/`mkdir` all acted on the prefix, `glob` and `realpath` `` |
|         - | 1547 | `` * answered for it, and `shell_exec("cmd\0; rm -rf /")` ran the prefix as a`` |
|         - | 1548 | ` * command. It is the classic poison-NUL-byte shape php closed engine-wide: a` |
|         - | 1549 | ` * script that concatenates request input into a filename gets a truncation` |
|         - | 1550 | ` * where php gets a refusal, and the extension check the suffix was there to` |
|         - | 1551 | ` * perform never runs.` |
|         - | 1552 | ` *` |
|         - | 1553 | ` * The mask is positional (bit N => parameter N is a path), which is how php` |
|         - | 1554 | ` * carries it too. Only functions PHL actually registers are listed; each row's` |
|         - | 1555 | ` * positions were verified against php 8.5 argument by argument (the answer is` |
|         - | 1556 | `` * NOT derivable from the parameter name — preg_match's `$pattern` is an`` |
|         - | 1557 | ``  * ordinary string, glob's is a path — nor from the type, which is `string` `` |
|         - | 1558 | ` * for both).` |
|         - | 1559 | ` *` |
|         - | 1560 | ` * What is deliberately NOT here: the stat family (file_exists, is_dir, stat,` |
|         - | 1561 | ` * filesize, fileperms, …), which php parses with Z_PARAM_STR and answers` |
|         - | 1562 | `` * `false` for in silence, and the pure PATH-STRING functions (basename,`` |
|         - | 1563 | ` * dirname, pathinfo), which php lets the NUL through untouched because they` |
|         - | 1564 | ` * never touch the filesystem. Both are php-exact here already.` |
|         - | 1565 | ` */` |
|   5818593 | 1566 | `static sxu32 VmBuiltinPathMask(SyString *pName)` |
|         5 | 1567 | `{` |
|         - | 1568 | `	static const struct {` |
|         - | 1569 | `		const char *zName;` |
|         - | 1570 | `		sxu32 nByte;` |
|         - | 1571 | `		sxu32 mask;` |
|         - | 1572 | `	} aPath[] = {` |
|         - | 1573 | `		/* Open / read / write */` |
|         - | 1574 | `		{ "fopen",             5, 1u<<0 },` |
|         - | 1575 | `		{ "file_get_contents", 17, 1u<<0 },` |
|         - | 1576 | `		{ "file_put_contents", 17, 1u<<0 },` |
|         - | 1577 | `		{ "file",              4, 1u<<0 },` |
|         - | 1578 | `		{ "readfile",          8, 1u<<0 },` |
|         - | 1579 | `		{ "parse_ini_file",   14, 1u<<0 },` |
|         - | 1580 | `		{ "md5_file",          8, 1u<<0 },` |
|         - | 1581 | `		{ "sha1_file",         9, 1u<<0 },` |
|         - | 1582 | `		{ "hash_file",         9, 1u<<1 },` |
|         - | 1583 | `		{ "hash_hmac_file",   14, 1u<<1 },` |
|         - | 1584 | `		{ "hash_update_file", 16, 1u<<1 },` |
|         - | 1585 | `		/* Metadata / mutation */` |
|         - | 1586 | `		{ "unlink",            6, 1u<<0 },` |
|         - | 1587 | `		{ "touch",             5, 1u<<0 },` |
|         - | 1588 | `		{ "chmod",             5, 1u<<0 },` |
|         - | 1589 | `		{ "chgrp",             5, 1u<<0 },` |
|         - | 1590 | `		{ "chown",             5, 1u<<0 },` |
|         - | 1591 | `		{ "rename",            6, (1u<<0)\|(1u<<1) },` |
|         - | 1592 | `		{ "copy",              4, (1u<<0)\|(1u<<1) },` |
|         - | 1593 | `		{ "link",              4, (1u<<0)\|(1u<<1) },` |
|         - | 1594 | `		{ "symlink",           7, (1u<<0)\|(1u<<1) },` |
|         - | 1595 | `		{ "readlink",          8, 1u<<0 },` |
|         - | 1596 | `		{ "realpath",          8, 1u<<0 },` |
|         - | 1597 | `		{ "stream_resolve_include_path", 27, 1u<<0 },` |
|         - | 1598 | `		/* Directories */` |
|         - | 1599 | `		{ "mkdir",             5, 1u<<0 },` |
|         - | 1600 | `		{ "rmdir",             5, 1u<<0 },` |
|         - | 1601 | `		{ "opendir",           7, 1u<<0 },` |
|         - | 1602 | `		{ "dir",               3, 1u<<0 },` |
|         - | 1603 | `		{ "scandir",           7, 1u<<0 },` |
|         - | 1604 | `		{ "chdir",             5, 1u<<0 },` |
|         - | 1605 | `		{ "chroot",            6, 1u<<0 },` |
|         - | 1606 | `		{ "glob",              4, 1u<<0 },` |
|         - | 1607 | `		{ "tempnam",           7, (1u<<0)\|(1u<<1) },` |
|         - | 1608 | `		{ "disk_free_space",  15, 1u<<0 },` |
|         - | 1609 | `		{ "disk_total_space", 16, 1u<<0 },` |
|         - | 1610 | `		{ "diskfreespace",    13, 1u<<0 },` |
|         - | 1611 | `		/* Not paths at all, and php screens them exactly as if they were: the` |
|         - | 1612 | `		 * datetime a FORMAT is read against is Z_PARAM_PATH_STR at every door` |
|         - | 1613 | `		 * that takes one, so a NUL inside it is the same catchable ValueError.` |
|         - | 1614 | ``		 * Only these five; `new DateTime($s)`, `date_create()`, `modify()` and`` |
|         - | 1615 | ``		 * `date_parse()` take an ordinary string there and read up to the NUL. */`` |
|         - | 1616 | `		{ "date_parse_from_format",                22, 1u<<1 },` |
|         - | 1617 | `		{ "date_create_from_format",               23, 1u<<1 },` |
|         - | 1618 | `		{ "date_create_immutable_from_format",     33, 1u<<1 },` |
|         - | 1619 | `		{ "DateTime::createFromFormat",            26, 1u<<1 },` |
|         - | 1620 | `		{ "DateTimeImmutable::createFromFormat",   35, 1u<<1 },` |
|         - | 1621 | `		/* Path-shaped settings and the pattern matcher */` |
|         - | 1622 | `		{ "fnmatch",           7, (1u<<0)\|(1u<<1) },` |
|         - | 1623 | `		{ "set_include_path", 16, 1u<<0 },` |
|         - | 1624 | `		{ "session_save_path", 17, 1u<<0 },` |
|         - | 1625 | `		{ "error_log",         9, 1u<<2 },` |
|         - | 1626 | `		/* Commands handed to the shell — and the two escapers, which php screens` |
|         - | 1627 | `		 * the same way even though neither of them runs anything: a NUL in what a` |
|         - | 1628 | `		 * script is about to hand a shell is refused where it is WRITTEN. */` |
|         - | 1629 | `		{ "shell_exec",       10, 1u<<0 },` |
|         - | 1630 | `		{ "popen",             5, 1u<<0 },` |
|         - | 1631 | `		{ "escapeshellarg",   14, 1u<<0 },` |
|         - | 1632 | `		{ "escapeshellcmd",   14, 1u<<0 },` |
|         - | 1633 | `		{ "exec",              4, 1u<<0 },` |
|         - | 1634 | `		{ "system",            6, 1u<<0 },` |
|         - | 1635 | `		{ "passthru",          8, 1u<<0 },` |
|         - | 1636 | `		/* The SPL path constructors, which php screens identically and reports` |
|         - | 1637 | ``		 * under their QUALIFIED name (`SplFileInfo::__construct(): Argument #1`` |
|         - | 1638 | ``		 * ($filename) …`). They are native methods, so their signature reaches this`` |
|         - | 1639 | `		 * screen the same way a builtin's does. */` |
|         - | 1640 | `		{ "SplFileInfo::__construct",                24, 1u<<0 },` |
|         - | 1641 | `		{ "DirectoryIterator::__construct",          30, 1u<<0 },` |
|         - | 1642 | `		{ "FilesystemIterator::__construct",         31, 1u<<0 },` |
|         - | 1643 | `		{ "RecursiveDirectoryIterator::__construct", 39, 1u<<0 },` |
|         - | 1644 | `		{ "GlobIterator::__construct",                25, 1u<<0 },` |
|         - | 1645 | `	};` |
|         - | 1646 | `	sxu32 i;` |
|   5818598 | 1647 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|       ! 0 | 1648 | `		return 0;` |
|         - | 1649 | `	}` |
| 327481340 | 1650 | `	for( i = 0 ; i < SX_ARRAYSIZE(aPath) ; ++i ){` |
| 321762400 | 1651 | `		if( pName->nByte == aPath[i].nByte` |
| 168457527 | 1652 | `		 && SyStrnicmp(pName->zString,aPath[i].zName,pName->nByte) == 0 ){` |
|     99663 | 1653 | `			return aPath[i].mask;` |
|         - | 1654 | `		}` |
| 160917252 | 1655 | `	}` |
|   5718940 | 1656 | `	return 0;` |
|   2910835 | 1657 | `}` |
|         - | 1658 | `/*` |
|         - | 1659 | ` * Does this argument carry a NUL byte? Only a STRING can: every other scalar` |
|         - | 1660 | ` * renders through the number/bool formatters, which emit none. An OBJECT is` |
|         - | 1661 | ` * coerced by the caller before asking (php's ZPP order), so by the time this` |
|         - | 1662 | ` * runs a Stringable is already the string it produced.` |
|         - | 1663 | ` */` |
|     99826 | 1664 | `static int VmArgHasNulByte(ph7_value *pArg)` |
|         5 | 1665 | `{` |
|         - | 1666 | `	const char *zStr;` |
|         - | 1667 | `	sxu32 n, nLen;` |
|     99831 | 1668 | `	if( (pArg->iFlags & MEMOBJ_STRING) == 0 ){` |
|         3 | 1669 | `		return 0;` |
|         - | 1670 | `	}` |
|     99829 | 1671 | `	zStr = (const char *)SyBlobData(&pArg->sBlob);` |
|     99829 | 1672 | `	nLen = SyBlobLength(&pArg->sBlob);` |
|   6398080 | 1673 | `	for( n = 0 ; n < nLen ; ++n ){` |
|   6298366 | 1674 | `		if( zStr[n] == 0 ){` |
|       113 | 1675 | `			return 1;` |
|         - | 1676 | `		}` |
|   3227294 | 1677 | `	}` |
|     99719 | 1678 | `	return 0;` |
|     49918 | 1679 | `}` |
|         - | 1680 | `/*` |
|         - | 1681 | ` * Does php's strict_types rule refuse this argument for the declared type?` |
|         - | 1682 | ` *` |
|         - | 1683 | `` * A `declare(strict_types=1)` file gets NO scalar coercion at an internal call`` |
|         - | 1684 | ` * either — php applies the same rule to a builtin, a native method and a userland` |
|         - | 1685 | `` * function, and the single exception is the int -> float widening. So `trim(5)`,`` |
|         - | 1686 | `` * `sqrt("4")`, `str_repeat("a", 2.0)` and `in_array($n, $a, 1)` are all TypeErrors`` |
|         - | 1687 | ` * there, where the weak-mode screen below (which is the only one PHL had) coerces` |
|         - | 1688 | ` * and computes.` |
|         - | 1689 | ` *` |
|         - | 1690 | ` * Only the arms a scalar could otherwise satisfy are decided here; an array, a` |
|         - | 1691 | ` * resource, a null and a class-typed mismatch are the weak screen's, and its` |
|         - | 1692 | ` * verdicts stand in both modes.` |
|         - | 1693 | ` */` |
|       194 | 1694 | `static int VmStrictArgRefused(ph7_value *pArg,const char *zType,int nType)` |
|         3 | 1695 | `{` |
|         - | 1696 | `	/* Tested in ph7_type_name()'s own order, so the branch taken and the name the` |
|         - | 1697 | `	 * refusal reports can never disagree. FLOAT comes before INT on purpose:` |
|         - | 1698 | `	 * ph7_value_is_int() is deliberately lenient — an integer-valued real caches an` |
|         - | 1699 | ``	 * int and answers TRUE — and `str_repeat("a", 2.0)` is php's TypeError, not an`` |
|         - | 1700 | `	 * accepted int. */` |
|       197 | 1701 | `	if( ph7_value_is_bool(pArg) ){` |
|        12 | 1702 | `		return !VmSigTypeHas(zType,nType,"bool")` |
|         7 | 1703 | `		    && !VmSigTypeHas(zType,nType,"true")` |
|        11 | 1704 | `		    && !VmSigTypeHas(zType,nType,"false");` |
|         - | 1705 | `	}` |
|       189 | 1706 | `	if( ph7_value_is_float(pArg) ){` |
|         7 | 1707 | `		return !VmSigTypeHas(zType,nType,"float");` |
|         - | 1708 | `	}` |
|       183 | 1709 | `	if( ph7_value_is_int(pArg) ){` |
|         - | 1710 | `		/* int -> float is the one widening strict mode keeps. */` |
|        28 | 1711 | `		return !VmSigTypeHas(zType,nType,"int") && !VmSigTypeHas(zType,nType,"float");` |
|         - | 1712 | `	}` |
|       157 | 1713 | `	if( ph7_value_is_string(pArg) ){` |
|         - | 1714 | ``		/* `callable` is not a coercion: a function-name string satisfies it in both`` |
|         - | 1715 | `		 * modes (array_map('strtoupper', …) under strict is php-legal). */` |
|       104 | 1716 | `		return !VmSigTypeHas(zType,nType,"string") && !VmSigTypeHas(zType,nType,"callable");` |
|         - | 1717 | `	}` |
|        54 | 1718 | `	if( ph7_value_is_object(pArg) ){` |
|         - | 1719 | ``		/* An object reaches a `string` parameter only through __toString(), which is`` |
|         - | 1720 | `		 * a coercion strict mode does not perform. Every other arm is the weak` |
|         - | 1721 | `		 * screen's decision. */` |
|        24 | 1722 | `		return VmSigTypeHas(zType,nType,"string")` |
|        12 | 1723 | `		    && !VmSigTypeHas(zType,nType,"object")` |
|         2 | 1724 | `		    && !VmSigTypeHas(zType,nType,"iterable")` |
|         2 | 1725 | `		    && !VmSigTypeHas(zType,nType,"callable")` |
|        23 | 1726 | `		    && !VmSigTypeHasClass(zType,nType);` |
|         - | 1727 | `	}` |
|        32 | 1728 | `	return 0;` |
|       100 | 1729 | `}` |
|         - | 1730 | `/*` |
|         - | 1731 | ` * PHP-8 ZPP type enforcement for host functions, driven by the aBuiltinSig[]` |
|         - | 1732 | ` * declaration (band A #7). Screens only the arguments that php can NEVER coerce` |
|         - | 1733 | ` * into a declared scalar parameter — arrays, resources, and objects without a` |
|         - | 1734 | ` * __toString() — and throws the catchable TypeError php throws, before the C` |
|         - | 1735 | ` * routine runs. Without this an array argument reached the builtin and was` |
|         - | 1736 | ` * stringified to the literal "Array" (strrev(['a']) returned "yarrA").` |
|         - | 1737 | ` *` |
|         - | 1738 | ` * Deliberately narrow: scalar-to-scalar coercion (and its deprecations) stays` |
|         - | 1739 | ` * with the per-builtin ZPP helpers. Those emit E_DEPRECATED, which does NOT` |
|         - | 1740 | ` * abort the call, so a central copy would double-fire — the trap that sank the` |
|         - | 1741 | ` * first central-ZPP attempt. A TypeError aborts, so there is nothing to double.` |
|         - | 1742 | ` */` |
|   6044667 | 1743 | `PH7_PRIVATE sxi32 VmEnforceBuiltinArgTypes(` |
|         - | 1744 | `	ph7_context *pCtx,    /* Call context (for the throw) */` |
|         - | 1745 | `	ph7_user_func *pFunc, /* Callee */` |
|         - | 1746 | `	int nGiven,           /* Argument count */` |
|         - | 1747 | `	ph7_value **apArg     /* Arguments */` |
|         - | 1748 | `	)` |
|         5 | 1749 | `{` |
|         - | 1750 | `	/*` |
|         - | 1751 | `	 * Builtins whose own argument check is php-exact and VALUE-based rather than` |
|         - | 1752 | `	 * type-based must not be pre-empted here, or their message is lost. Same rule` |
|         - | 1753 | `	 * the aBuiltinArity[] table follows: a builtin that already says what php says` |
|         - | 1754 | `	 * stays off the shared screen. get_class_vars() takes any stringifiable value` |
|         - | 1755 | `	 * and reports "must be a valid class name, Array given"; get_class_methods() is` |
|         - | 1756 | `	 * the same shape with php's other wording ("must be an object or a valid class` |
|         - | 1757 | ``	 * name, int given") — the declared `object\|string` never appears in either.`` |
|         - | 1758 | `	 *` |
|         - | 1759 | `	 * strtr() is here for a structural reason: php declares it as two OVERLOADS` |
|         - | 1760 | `	 * dispatched on arity — strtr(string, array) and strtr(string, string, string)` |
|         - | 1761 | `` 	 * — so the expected type of $from is `array` with two arguments and `string` `` |
|         - | 1762 | ``	 * with three. One signature cannot say that (the stub's `array\|string` is the`` |
|         - | 1763 | `	 * union of the two, which is the wording php never uses), so the builtin does` |
|         - | 1764 | `	 * its own dispatch through PH7_ArgSatisfiesString(), the same rule as here.` |
|         - | 1765 | `	 *` |
|         - | 1766 | ``	 * implode() is the same structure: `array\|string $separator` is what the two`` |
|         - | 1767 | `	 * ARITIES accept between them, never what one call can use. Once an $array` |
|         - | 1768 | `	 * argument is present php has resolved the overload and reports` |
|         - | 1769 | ``	 * `must be of type string`, and with the array in position #1 it reports`` |
|         - | 1770 | ``	 * `must be of type string, array given` against #1 rather than a #2 error.`` |
|         - | 1771 | `	 * PH7_builtin_implode words all of that itself.` |
|         - | 1772 | `	 *` |
|         - | 1773 | `	 * Its alias join() is here for the same reason and then some: php 8.5 does not` |
|         - | 1774 | `	 * word the two the same, so the builtin reproduces BOTH orders keyed on the` |
|         - | 1775 | `	 * invoked name (see PH7_builtin_implode's header for the value-for-value` |
|         - | 1776 | `	 * table against 8.5.8). php's own asymmetry between a target and its alias,` |
|         - | 1777 | `	 * reproduced rather than smoothed over — parity is binding (§10).` |
|         - | 1778 | `	 *` |
|         - | 1779 | `	 * number_format() is here because php's DECLARED type and its REFUSAL text` |
|         - | 1780 | ``	 * disagree: the stub says `float $num` (which is what Reflection prints) while`` |
|         - | 1781 | `	 * the ZPP macro behind it is Z_PARAM_NUMBER, whose TypeError says` |
|         - | 1782 | ``	 * `must be of type int\|float`. One row cannot say both, so the row carries the`` |
|         - | 1783 | `	 * declared type for Reflection and the builtin words every refusal itself.` |
|         - | 1784 | `	 *` |
|         - | 1785 | `	 * RecursiveIteratorIterator::__construct() is the first NATIVE METHOD here, and` |
|         - | 1786 | `	 * it is the same disagreement one level up: php's stub declares` |
|         - | 1787 | ``	 * `Traversable $iterator` (what Reflection prints) while its ZPP is a bare "o",`` |
|         - | 1788 | ``	 * whose TypeError says `must be of type object`. A native method's diagnostic`` |
|         - | 1789 | `	 * name is the QUALIFIED one, so the row below matches it and nothing else.` |
|         - | 1790 | `	 *` |
|         - | 1791 | `	 * The array_udiff/array_uintersect u-variant family is here for its ORDER:` |
|         - | 1792 | `	 * php validates the trailing comparison callback(s) before ANY of the` |
|         - | 1793 | `	 * arrays — array_diff_ukey(123,[1],456) names Argument #3, not #1 — and a` |
|         - | 1794 | `	 * positional screen cannot say that. HashmapUVariant performs the whole` |
|         - | 1795 | `	 * php sequence itself (callbacks, then Argument #1, then the middles).` |
|         - | 1796 | `	 */` |
|         - | 1797 | `	static const char *azSelfChecked[] = { "get_class_vars", "get_class_methods", "strtr",` |
|         - | 1798 | `		"implode", "join", "number_format", "RecursiveIteratorIterator::__construct",` |
|         - | 1799 | `		"array_udiff", "array_udiff_assoc", "array_udiff_uassoc",` |
|         - | 1800 | `		"array_uintersect", "array_uintersect_assoc", "array_uintersect_uassoc",` |
|         - | 1801 | `		"array_diff_uassoc", "array_diff_ukey",` |
|         - | 1802 | `		"array_intersect_uassoc", "array_intersect_ukey" };` |
|   6044672 | 1803 | `	const char *zSig = pFunc->zSig;` |
|         - | 1804 | `	const char *zCur, *zEnd;` |
|   6044672 | 1805 | `	int iArg = 0;` |
|         - | 1806 | `	/* The CALL site's file mode, stamped by the compiler onto this call's argument` |
|         - | 1807 | `	 * map (weak when there is no map — a call that carries no compile-time metadata` |
|         - | 1808 | `	 * was written in a weak-mode file, since a strict one always attaches one). */` |
|   6044672 | 1809 | `	int bStrict = (pCtx->pArgMap && pCtx->pArgMap->bStrict) ? 1 : 0;` |
|         - | 1810 | `	sxu32 nPathMask;` |
|   6044672 | 1811 | `	if( zSig == 0 ){` |
|    226079 | 1812 | `		return SXRET_OK;` |
|         - | 1813 | `	}` |
|   5818598 | 1814 | `	nPathMask = VmBuiltinPathMask(&pFunc->sName);` |
| 104089055 | 1815 | `	for( iArg = 0 ; iArg < (int)SX_ARRAYSIZE(azSelfChecked) ; ++iArg ){` |
| 147501133 | 1816 | `		if( SyStrncmp(pFunc->sName.zString,azSelfChecked[iArg],` |
| 147501133 | 1817 | `			(sxu32)SyStrlen(azSelfChecked[iArg])) == 0` |
|  49207576 | 1818 | `		 && pFunc->sName.nByte == SyStrlen(azSelfChecked[iArg]) ){` |
|     46257 | 1819 | `			return SXRET_OK;` |
|         - | 1820 | `		}` |
|  49161303 | 1821 | `	}` |
|   5772346 | 1822 | `	iArg = 0;` |
|   5772346 | 1823 | `	zCur = zSig;` |
|   5772346 | 1824 | `	zEnd = &zSig[SyStrlen(zSig)];` |
|  14373439 | 1825 | `	while( zCur < zEnd && iArg < nGiven ){` |
|         - | 1826 | `		const char *zType, *zName, *zStop;` |
|         - | 1827 | `		int nType, nName, bByRef;` |
|         - | 1828 | `		ph7_value *pArg;` |
|         - | 1829 | `		char zGivenBuf[64];` |
|         - | 1830 | `		/* Parameter = "<type> $<name>[ = <default>]"; the type is whatever` |
|         - | 1831 | `		 * precedes the '$', and an empty type means "untyped" (no screen). */` |
|  11543606 | 1832 | `		while( zCur < zEnd && zCur[0] == ' ' ){` |
|   2925125 | 1833 | `			zCur++;` |
|         5 | 1834 | `		}` |
|   8618486 | 1835 | `		zStop = zCur;` |
| 133163768 | 1836 | `		while( zStop < zEnd && zStop[0] != ',' ){` |
| 124545287 | 1837 | `			if( zStop[0] == '\'' \|\| zStop[0] == '"' ){` |
|   1467848 | 1838 | `				zStop = VmSigSkipQuoted(zStop);` |
|   1467848 | 1839 | `				if( zStop >= zEnd ){` |
|       ! 0 | 1840 | `					break;` |
|         - | 1841 | `				}` |
|    733921 | 1842 | `			}` |
| 124545287 | 1843 | `			zStop++;` |
|         5 | 1844 | `		}` |
|   8618486 | 1845 | `		zName = zCur;` |
|  61378203 | 1846 | `		while( zName < zStop && zName[0] != '$' ){` |
|  52759722 | 1847 | `			zName++;` |
|         5 | 1848 | `		}` |
|   8618486 | 1849 | `		if( zName >= zStop ){` |
|       ! 0 | 1850 | `			break; /* malformed / no parameter name — stop screening */` |
|         - | 1851 | `		}` |
|   8618486 | 1852 | `		if( zName >= zCur + 3 && SyMemcmp(zName - 3,"...",3) == 0 ){` |
|     16245 | 1853 | `			break; /* variadic tail: stop (its type applies to the rest) */` |
|         - | 1854 | `		}` |
|   8602246 | 1855 | `		zType = zCur;` |
|   8602246 | 1856 | `		nType = (int)(zName - zCur);` |
|   8602246 | 1857 | `		if( nType > 0 && zType[0] == '~' ){` |
|         - | 1858 | ``			/* A `~Type $p` row is php's stub-versus-body mismatch: the type php`` |
|         - | 1859 | `			 * DECLARES (which Reflection must report) is looser than the one its C` |
|         - | 1860 | `			 * body asks for, so the screen stands aside and the builtin raises the` |
|         - | 1861 | `			 * TypeError itself. RecursiveCachingIterator::__construct is the first:` |
|         - | 1862 | ``			 * it is declared `Iterator $iterator` and refuses anything that is not a`` |
|         - | 1863 | `			 * RecursiveIterator. */` |
|       657 | 1864 | `			zCur = (zStop < zEnd) ? zStop + 1 : zEnd;` |
|       657 | 1865 | `			iArg++;` |
|       660 | 1866 | `			continue;` |
|         - | 1867 | `		}` |
|         - | 1868 | `		/* Trim the trailing spaces and the by-ref marker of "array &$array" */` |
|   8601592 | 1869 | `		bByRef = 0;` |
|  25651889 | 1870 | `		while( nType > 0 && (zType[nType-1] == ' ' \|\| zType[nType-1] == '&') ){` |
|   8524141 | 1871 | `			if( zType[nType-1] == '&' ){` |
|      4447 | 1872 | `				bByRef = 1;` |
|      2223 | 1873 | `			}` |
|   8524141 | 1874 | `			nType--;` |
|         5 | 1875 | `		}` |
|   8601592 | 1876 | `		zName++; /* skip '$' */` |
|   8601592 | 1877 | `		nName = 0;` |
|  61927196 | 1878 | `		while( &zName[nName] < zStop && zName[nName] != ' ' && zName[nName] != '=' ){` |
|  53325609 | 1879 | `			nName++;` |
|         5 | 1880 | `		}` |
|   8601592 | 1881 | `		pArg = apArg[iArg];` |
|   8601587 | 1882 | `		if( bByRef && pArg->nIdx == SXU32_HIGH` |
|      2265 | 1883 | `		 && !(pCtx->pArgMap && pCtx->pArgMap->bArgShapes && !pCtx->pArgMap->bHasNamed) ){` |
|         - | 1884 | `			/* A by-reference parameter handed something with no slot to write back` |
|         - | 1885 | `			 * through -- a literal, a constant, the result of a call. php settles` |
|         - | 1886 | `			 * that at the CALL, before the callee's ZPP runs, so the type screen` |
|         - | 1887 | ``			 * must not speak first: `array_pop('foo')` is`` |
|         - | 1888 | `			 * "could not be passed by reference" and not "must be of type array,` |
|         - | 1889 | `			 * string given".` |
|         - | 1890 | `			 *` |
|         - | 1891 | `			 * Only when this call site carries no argument SHAPES, though. When it` |
|         - | 1892 | `			 * does, PH7_VmScreenByRefArgShapes has already had its say — it refused` |
|         - | 1893 | `			 * the literal and let the call RESULT through with php's notice — and` |
|         - | 1894 | `			 * standing aside here would swallow the type error php still reports for` |
|         - | 1895 | ``			 * the latter (`sort(new stdClass)` is "must be of type array, stdClass`` |
|         - | 1896 | `			 * given", not a silent false). */` |
|         7 | 1897 | `			zCur = (zStop < zEnd) ? zStop + 1 : zEnd;` |
|         7 | 1898 | `			iArg++;` |
|         7 | 1899 | `			continue;` |
|         - | 1900 | `		}` |
|   8601586 | 1901 | `		if( nType > 0 && !VmSigTypeHas(zType,nType,"mixed") ){` |
|   7161649 | 1902 | `			const char *zGiven = 0;` |
|   7161649 | 1903 | `			if( bStrict && VmStrictArgRefused(pArg,zType,nType) ){` |
|         - | 1904 | ``				/* php names the VALUE for a bool here too (`true given`). */`` |
|        37 | 1905 | `				zGiven = VmValueGivenName(pArg,zGivenBuf,sizeof(zGivenBuf));` |
|   7161631 | 1906 | `			}else if( (pArg->iFlags & MEMOBJ_HASHMAP) != 0 ){` |
|    449223 | 1907 | `				if( !VmSigTypeHas(zType,nType,"array")` |
|    224829 | 1908 | `				 && !VmSigTypeHas(zType,nType,"iterable")` |
|       425 | 1909 | `				 && !VmSigTypeHas(zType,nType,"callable") ){` |
|       203 | 1910 | `					zGiven = "array";` |
|       104 | 1911 | `				}` |
|   6937009 | 1912 | `			}else if( (pArg->iFlags & MEMOBJ_OBJ) != 0 ){` |
|     22460 | 1913 | `				if( !VmSigTypeHas(zType,nType,"object")` |
|     16476 | 1914 | `				 && !VmSigTypeHas(zType,nType,"iterable")` |
|     10492 | 1915 | `				 && !VmSigTypeHas(zType,nType,"callable")` |
|      9419 | 1916 | `				 && !VmSigTypeHasClass(zType,nType) ){` |
|         - | 1917 | `					/* An object with __toString() still satisfies a string` |
|         - | 1918 | `					 * parameter in weak mode — php coerces it. */` |
|       222 | 1919 | `					int bStringable = VmSigTypeHas(zType,nType,"string")` |
|       158 | 1920 | `						&& PH7_ArgSatisfiesString(pArg);` |
|       163 | 1921 | `					if( !bStringable ){` |
|        89 | 1922 | `						zGiven = VmArgTypeName(pArg);` |
|        42 | 1923 | `					}` |
|     22386 | 1924 | `				}else if( VmSigTypeHasClass(zType,nType)` |
|     15161 | 1925 | `				       && !VmSigTypeHas(zType,nType,"object")` |
|      8020 | 1926 | `				       && !VmSigTypeHas(zType,nType,"iterable")` |
|      8020 | 1927 | `				       && !VmSigTypeHas(zType,nType,"callable")` |
|      8025 | 1928 | `				       && !VmSigTypeHas(zType,nType,"string") ){` |
|         - | 1929 | `					/* A class-typed parameter given an object of the WRONG class.` |
|         - | 1930 | `					 * Naming a class used to be enough to let ANY object through, so` |
|         - | 1931 | `` 					 * `date_modify($immutable)` and `timezone_name_get($date)` `` |
|         - | 1932 | `					 * answered silently where php raises. Only decided when every` |
|         - | 1933 | `					 * class arm resolves to a declared class: an arm PHL does not` |
|         - | 1934 | `					 * declare cannot be judged, so the parameter stays unscreened. */` |
|     11946 | 1935 | `					if( !VmSigObjSatisfiesClass(pCtx->pVm,zType,nType,` |
|      7958 | 1936 | `						(ph7_class_instance *)pArg->x.pOther) ){` |
|        59 | 1937 | `						zGiven = VmArgTypeName(pArg);` |
|        28 | 1938 | `					}` |
|      3988 | 1939 | `				}` |
|   6701164 | 1940 | `			}else if( (pArg->iFlags & MEMOBJ_NULL) != 0 ){` |
|         - | 1941 | `				/* php only DEPRECATES null for a non-nullable parameter; PHL rejects` |
|         - | 1942 | `				 * it (scope policy). A leading '?' or an explicit "null" arm in a` |
|         - | 1943 | `				 * union declares the parameter nullable. A "callable" parameter is` |
|         - | 1944 | `				 * left to the builtin's own callback check, which words the failure` |
|         - | 1945 | `				 * php's way ("must be a valid callback, no array or string given") —` |
|         - | 1946 | `				 * the same reason get_class_vars() sits on azSelfChecked[]. */` |
|      7026 | 1947 | `				if( zType[0] != '?'` |
|      3590 | 1948 | `				 && !VmSigTypeHas(zType,nType,"null")` |
|       135 | 1949 | `				 && !VmSigTypeHas(zType,nType,"callable") ){` |
|       103 | 1950 | `					zGiven = "null";` |
|        49 | 1951 | `				}` |
|   6686417 | 1952 | `			}else if( (pArg->iFlags & (MEMOBJ_STRING\|MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL)) != 0` |
|   6682899 | 1953 | `			       && (VmSigTypeHasClass(zType,nType)` |
|   6682570 | 1954 | `			        \|\| VmSigTypeHas(zType,nType,"object")) ){` |
|         - | 1955 | `				/* A SCALAR against a parameter that can only hold an INSTANCE —` |
|         - | 1956 | ``				 * a named class, or the bare `object` keyword. Every other scalar`` |
|         - | 1957 | `				 * pairing is left to weak-mode coercion, which is why nothing` |
|         - | 1958 | `				 * screened scalars here at all — but no coercion produces an` |
|         - | 1959 | `				 * instance, so php rejects this one. Found converting DateTime:` |
|         - | 1960 | ``				 * `$d->diff('x')` and `new DateTime('now','UTC')` ran on with a`` |
|         - | 1961 | ``				 * string where php raises. The `object` half was still blind when`` |
|         - | 1962 | `				 * WeakReference::create() declared the first such parameter, which` |
|         - | 1963 | `				 * also retires the "graceful degradation" NULL that spl_object_id(),` |
|         - | 1964 | `				 * spl_object_hash() and get_object_vars() used to answer. An arm a` |
|         - | 1965 | `				 * scalar CAN satisfy (a union with string/int/float/bool, or` |
|         - | 1966 | `				 * callable, which a string is) keeps the parameter unscreened —` |
|         - | 1967 | ``				 * and so does an `array` arm, whose refusal php words from the`` |
|         - | 1968 | `				 * builtin's own check rather than from the declared type` |
|         - | 1969 | ``				 * (array_walk's `array\|object &$array` says "must be of type`` |
|         - | 1970 | `				 * array", not "of type array\|object"). */` |
|      3813 | 1971 | `				if( !VmSigTypeHas(zType,nType,"string")` |
|      1413 | 1972 | `				 && !VmSigTypeHas(zType,nType,"int")` |
|       198 | 1973 | `				 && !VmSigTypeHas(zType,nType,"float")` |
|       112 | 1974 | `				 && !VmSigTypeHas(zType,nType,"bool")` |
|       112 | 1975 | `				 && !VmSigTypeHas(zType,nType,"true")` |
|       112 | 1976 | `				 && !VmSigTypeHas(zType,nType,"false")` |
|       112 | 1977 | `				 && !VmSigTypeHas(zType,nType,"array")` |
|        98 | 1978 | `				 && !VmSigTypeHas(zType,nType,"callable") ){` |
|         - | 1979 | `					/* php's VALUE name, not the type's: a bool is reported as` |
|         - | 1980 | ``					 * `true`/`false` (the rule Generator::throw()'s own check`` |
|         - | 1981 | `					 * already followed, and which this screen now runs first). */` |
|        77 | 1982 | `					zGiven = VmValueGivenName(pArg,zGivenBuf,sizeof(zGivenBuf));` |
|      2508 | 1983 | `				}else if( (pArg->iFlags & (MEMOBJ_STRING\|MEMOBJ_NULL)) == MEMOBJ_STRING` |
|      2375 | 1984 | `				       && !VmSigTypeHas(zType,nType,"string")` |
|      1155 | 1985 | `				       && !VmSigTypeHas(zType,nType,"bool")` |
|        28 | 1986 | `				       && !VmSigTypeHas(zType,nType,"true")` |
|        28 | 1987 | `				       && !VmSigTypeHas(zType,nType,"false")` |
|        28 | 1988 | `				       && !VmSigTypeHas(zType,nType,"array")` |
|        20 | 1989 | `				       && !VmSigTypeHas(zType,nType,"callable")` |
|        17 | 1990 | `				       && !PH7_MemObjStringIsNumeric(pArg) ){` |
|         - | 1991 | ``					/* The one arm that let this STRING past is `int`/`float`, and it`` |
|         - | 1992 | `					 * only takes a NUMERIC one — no coercion turns a string into an` |
|         - | 1993 | ``					 * instance of the class arm beside it. `round(1.5, 0, "x")` is`` |
|         - | 1994 | ``					 * php's `must be of type RoundingMode\|int, string given`; PHL`` |
|         - | 1995 | `					 * narrowed it to mode 0 and reported the ValueError for an` |
|         - | 1996 | `					 * invalid MODE, which blames the wrong thing. The plain` |
|         - | 1997 | `					 * number-only spelling is screened by the STRING branch below;` |
|         - | 1998 | `					 * a class arm routes the same argument through here instead, so` |
|         - | 1999 | `					 * the rule has to be stated in both places. */` |
|         7 | 2000 | `					zGiven = "string";` |
|         3 | 2001 | `				}` |
|   6681633 | 2002 | `			}else if( (pArg->iFlags & MEMOBJ_REAL) != 0` |
|   3342510 | 2003 | `			       && VmSigTypeIsIntOnly(zType,nType)` |
|       617 | 2004 | `			       && !VmDoubleFitsInt((double)pArg->rVal) ){` |
|         - | 2005 | ``				/* A FLOAT against a parameter typed exactly `int` (or `?int`), and`` |
|         - | 2006 | `				 * one no int can hold: a fraction, a magnitude past the signed` |
|         - | 2007 | `				 * 64-bit range, NaN or an infinity. php refuses every one of them` |
|         - | 2008 | `				 * (zend_parse_arg_long's ZEND_DOUBLE_FITS_LONG / is-integral pair,` |
|         - | 2009 | `				 * the fractional case with a deprecation PHL rejects outright by` |
|         - | 2010 | `				 * §10) and the refusal is this screen's own wording.` |
|         - | 2011 | `				 *` |
|         - | 2012 | `				 * PH7_IntArgResolve has always said exactly this, but only for the` |
|         - | 2013 | `` 				 * builtins that CALL it from their own body — so `dechex(1.5)` `` |
|         - | 2014 | ``				 * answered '1', `array_fill(1.5,1,0)` filled from 1, and`` |
|         - | 2015 | ``				 * `strpos("abc","c",1e19)` took the offset as PHP_INT_MIN and`` |
|         - | 2016 | `				 * reported a ValueError about a range it never had. Seventy-five` |
|         - | 2017 | ``				 * `int` parameters across the signature table were unscreened that`` |
|         - | 2018 | `				 * way, and a NATIVE METHOD has no body to call the helper from at` |
|         - | 2019 | `				 * all. Deciding it from the declared type covers both callee kinds` |
|         - | 2020 | `				 * from one place, and the per-builtin helper still stands for the` |
|         - | 2021 | ``				 * message rows this screen cannot reach (the `azSelfChecked` set). */`` |
|        65 | 2022 | `				zGiven = "float";` |
|   6680332 | 2023 | `			}else if( (pArg->iFlags & (MEMOBJ_STRING\|MEMOBJ_NULL)) == MEMOBJ_STRING` |
|   5307880 | 2024 | `			       && (VmSigTypeHas(zType,nType,"int")` |
|   3933560 | 2025 | `			        \|\| VmSigTypeHas(zType,nType,"float"))` |
|   1968388 | 2026 | `			       && !VmSigTypeHas(zType,nType,"string")` |
|   1968121 | 2027 | `			       && !VmSigTypeHas(zType,nType,"array")` |
|       286 | 2028 | `			       && !VmSigTypeHas(zType,nType,"object")` |
|       278 | 2029 | `			       && !VmSigTypeHas(zType,nType,"iterable")` |
|       278 | 2030 | `			       && !VmSigTypeHas(zType,nType,"callable")` |
|       278 | 2031 | `			       && !VmSigTypeHas(zType,nType,"bool")` |
|       283 | 2032 | `			       && !VmSigTypeHasClass(zType,nType) ){` |
|         - | 2033 | ``				/* A STRING against a NUMBER-only parameter — `int`, `float`, or the`` |
|         - | 2034 | ``				 * `int\|float` union, with no arm a string can satisfy. Weak mode`` |
|         - | 2035 | `				 * coerces a NUMERIC one and php refuses every other — "x", "2abc"` |
|         - | 2036 | ``				 * and "0x2" are all `must be of type int, string given` (rule 41: a`` |
|         - | 2037 | `				 * numeric PREFIX is not enough, which is what SyStrIsNumeric would` |
|         - | 2038 | `				 * have accepted). Every BUILTIN with an int parameter already got` |
|         - | 2039 | `				 * this from PH7_IntArgResolve, called from its own body; a native` |
|         - | 2040 | `` 				 * METHOD has no body to call it from, so `ArrayIterator::seek('x')` `` |
|         - | 2041 | ``				 * seeked to 0, `DateTime::setTimestamp('abc')` set 0 and`` |
|         - | 2042 | ``				 * `DOMNodeList::item('zz')` answered element 0 — wrong ANSWERS,`` |
|         - | 2043 | `				 * not missing errors. Screening the declared type here covers both` |
|         - | 2044 | `				 * callee kinds from one place.` |
|         - | 2045 | `				 *` |
|         - | 2046 | `				 * The FLOAT arm is the same hazard one type over, and it was the` |
|         - | 2047 | `				 * half nothing covered: PH7_IntArgResolve has no float twin, so a` |
|         - | 2048 | ``				 * `float $num` builtin that did not hand-roll its own check simply`` |
|         - | 2049 | `				 * converted the string to 0.0 and COMPUTED with it —` |
|         - | 2050 | ``				 * `cos("nope")` answered `float(1)`, `sqrt("nope")` `float(0)`,`` |
|         - | 2051 | ``				 * `log("nope")` `float(-INF)`. Numbers with nothing wrong-looking`` |
|         - | 2052 | `				 * about them, from input php refuses outright.` |
|         - | 2053 | `				 *` |
|         - | 2054 | `				 * The NULL rule stays where it is: PHL rejects null for a` |
|         - | 2055 | `				 * non-nullable parameter by policy (§10) where php deprecates. */` |
|       422 | 2056 | `				if( !PH7_MemObjStringIsNumeric(pArg) ){` |
|       167 | 2057 | `					zGiven = "string";` |
|       201 | 2058 | `				}else if( VmSigTypeIsIntOnly(zType,nType) && !VmNumStrFitsInt(pArg) ){` |
|         - | 2059 | `					/* A NUMERIC string an int cannot hold — "1.5", "1e19",` |
|         - | 2060 | `					 * "99999999999999999999". php refuses all three (the fractional` |
|         - | 2061 | `					 * one after a deprecation §10 turns into the refusal), and PHL` |
|         - | 2062 | ``					 * narrowed them silently: `dechex("1e19")` answered '1' and`` |
|         - | 2063 | ``					 * `str_repeat("a","99999999999999999999")` took PHP_INT_MAX as`` |
|         - | 2064 | `					 * the count. Same wording, same position as the float arm above,` |
|         - | 2065 | `					 * because php reaches both through one ZPP macro. */` |
|        19 | 2066 | `					zGiven = "string";` |
|         8 | 2067 | `				}` |
|   6680163 | 2068 | `			}else if( (pArg->iFlags & (MEMOBJ_STRING\|MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL)) != 0` |
|   6680019 | 2069 | `			       && VmSigTypeIsArrayOnly(zType,nType) ){` |
|         - | 2070 | ``				/* A SCALAR against a parameter typed exactly `array`. No coercion`` |
|         - | 2071 | `				 * produces one, so php refuses it -- but the screen exempted every` |
|         - | 2072 | ``				 * `array` arm, union or not, and a whole family had no check of its`` |
|         - | 2073 | `				 * own to fall back on: sort/rsort/ksort/krsort/shuffle and` |
|         - | 2074 | ``				 * usort/uasort/uksort each answered `false` for `sort($notAnArray)`,`` |
|         - | 2075 | `				 * which is also what they answer for a sort that genuinely failed.` |
|         - | 2076 | `				 * call_user_func_array('strlen', 'x') answered false too,` |
|         - | 2077 | `				 * iterator_apply RAN the callback, and getopt/hash/password_hash/` |
|         - | 2078 | `				 * password_needs_rehash/unserialize/fputcsv simply carried on with` |
|         - | 2079 | `				 * the string where an options ARRAY was declared.` |
|         - | 2080 | `				 *` |
|         - | 2081 | `				 * The builtins that DO check (array_keys, in_array, asort, ...) word` |
|         - | 2082 | `				 * it identically, so the screen only pre-empts them -- and corrects` |
|         - | 2083 | `				 * one detail on the way: their ph7_type_name() says "bool" where php` |
|         - | 2084 | ``				 * names the VALUE, `true` or `false`. */`` |
|       243 | 2085 | `				zGiven = VmValueGivenName(pArg,zGivenBuf,sizeof(zGivenBuf));` |
|   6679905 | 2086 | `			}else if( (pArg->iFlags & MEMOBJ_RES) != 0 ){` |
|         - | 2087 | `				/* A class-typed parameter also accepts a resource: several handles php 8` |
|         - | 2088 | `				 * models as objects are still resources here (xml_*'s XMLParser is the` |
|         - | 2089 | `				 * one the signatures already declare php-8-style, for reflection). The` |
|         - | 2090 | `				 * screen would otherwise reject the engine's own parser handle. Recorded` |
|         - | 2091 | `				 * as a divergence in NEWPLAN §7 — it goes away when those handles become` |
|         - | 2092 | `				 * real objects. */` |
|        10 | 2093 | `				if( !VmSigTypeHas(zType,nType,"resource")` |
|        12 | 2094 | `				 && !VmSigTypeHasClass(zType,nType) ){` |
|        12 | 2095 | `					zGiven = "resource";` |
|         5 | 2096 | `				}` |
|         5 | 2097 | `			}` |
|   7161649 | 2098 | `			if( zGiven ){` |
|         - | 2099 | ``				/* php's `object\|array` parameters come from ONE ZPP macro`` |
|         - | 2100 | `				 * (Z_PARAM_ARRAY_OR_OBJECT) and it names only "array" in the` |
|         - | 2101 | `				 * refusal — array_walk(null,…), current(null) and` |
|         - | 2102 | `				 * http_build_query(null) all say "must be of type array". The` |
|         - | 2103 | `				 * SCALAR branch above already encodes that rule by declining to` |
|         - | 2104 | `				 * screen at all; the null and resource branches do screen, so the` |
|         - | 2105 | `				 * reported type has to be corrected here instead. */` |
|      1043 | 2106 | `				if( VmSigTypeHas(zType,nType,"array") && VmSigTypeHas(zType,nType,"object") ){` |
|        12 | 2107 | `					zType = "array";` |
|        12 | 2108 | `					nType = (int)sizeof("array")-1;` |
|         5 | 2109 | `				}` |
|      1617 | 2110 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 2111 | `					"%z(): Argument #%d ($%.*s) must be of type %.*s, %s given",` |
|       519 | 2112 | `					&pFunc->sName,iArg + 1,nName,zName,nType,zType,zGiven);` |
|         - | 2113 | `			}` |
|   3582078 | 2114 | `		}` |
|         - | 2115 | ``		/* A NaN reaching a parameter php declares `string`: php's ZPP coerces it`` |
|         - | 2116 | ``		 * (to "NAN") and warns `unexpected NAN value was coerced to string`, the`` |
|         - | 2117 | `		 * same 8.5 diagnostic the cast and the concatenation raise. The builtin` |
|         - | 2118 | `		 * bodies read their argument with ph7_value_to_string, which is the SILENT` |
|         - | 2119 | `		 * conversion by design (the engine builds keys and messages with it), so` |
|         - | 2120 | `		 * the diagnostic belongs here, where the DECLARED type says a coercion is` |
|         - | 2121 | `		 * what is about to happen. A union that also accepts a NUMBER is left` |
|         - | 2122 | `		 * alone -- php keeps the float there and coerces nothing -- but` |
|         - | 2123 | ``		 * `array\|string`, the spelling str_replace()'s subject carries, does`` |
|         - | 2124 | `		 * coerce and does warn. */` |
|   8600543 | 2125 | `		if( (pArg->iFlags & (MEMOBJ_REAL\|MEMOBJ_STRING)) == MEMOBJ_REAL` |
|   4304355 | 2126 | `		 && PH7_IS_NAN((double)pArg->rVal)` |
|      2015 | 2127 | `		 && VmSigTypeHas(zType,nType,"string")` |
|        52 | 2128 | `		 && !VmSigTypeHas(zType,nType,"float")` |
|         5 | 2129 | `		 && !VmSigTypeHas(zType,nType,"int")` |
|         7 | 2130 | `		 && !VmSigTypeHas(zType,nType,"mixed") ){` |
|         3 | 2131 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 2132 | `				"unexpected NAN value was coerced to string");` |
|         1 | 2133 | `		}` |
|         - | 2134 | `		/* A PATH parameter, once its type is settled: php's Z_PARAM_PATH refuses a` |
|         - | 2135 | `		 * NUL byte outright rather than letting the C API truncate at it. Raised` |
|         - | 2136 | `		 * after the type verdict because that is php's order — the coercion runs` |
|         - | 2137 | `		 * first, and only a value that could BE a path is asked whether it is a` |
|         - | 2138 | `		 * legal one. */` |
|   8600548 | 2139 | `		if( iArg < 31 && (nPathMask & (1u<<iArg)) != 0 ){` |
|     99831 | 2140 | `			if( (pArg->iFlags & MEMOBJ_OBJ) != 0 && PH7_ArgSatisfiesString(pArg) ){` |
|         - | 2141 | `				/* A Stringable object: php coerces it and checks the RESULT, so` |
|         - | 2142 | ``				 * `unlink($o)` with a __toString() returning a NUL-bearing name is`` |
|         - | 2143 | `				 * the same ValueError. Converting IN PLACE is what keeps the` |
|         - | 2144 | `				 * accessor running exactly ONCE — the builtin then receives the` |
|         - | 2145 | `				 * string it would have produced itself. The argument a builtin sees` |
|         - | 2146 | `				 * is its own copy on every dispatch route (a direct call, a spread,` |
|         - | 2147 | `				 * both call_user_func forwards), so the caller's object is not` |
|         - | 2148 | `				 * retyped; strict mode never gets here, because a Stringable does` |
|         - | 2149 | ``				 * not satisfy a `string` parameter there and the screen above has`` |
|         - | 2150 | `				 * already refused it. */` |
|         3 | 2151 | `				sxi32 rcConv = PH7_MemObjToStringUV(pArg);` |
|         3 | 2152 | `				if( rcConv != SXRET_OK ){` |
|       ! 0 | 2153 | `					return rcConv; /* __toString() threw: php propagates it too */` |
|         - | 2154 | `				}` |
|         1 | 2155 | `			}` |
|     99831 | 2156 | `			if( VmArgHasNulByte(pArg) ){` |
|       168 | 2157 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 2158 | `					"%z(): Argument #%d ($%.*s) must not contain any null bytes",` |
|        55 | 2159 | `					&pFunc->sName,iArg + 1,nName,zName);` |
|         - | 2160 | `			}` |
|     49858 | 2161 | `		}` |
|   8600438 | 2162 | `		zCur = (zStop < zEnd) ? zStop + 1 : zEnd;` |
|   8600438 | 2163 | `		iArg++;` |
|         5 | 2164 | `	}` |
|   5771198 | 2165 | `	return SXRET_OK;` |
|   3023872 | 2166 | `}` |
|         - | 2167 | `/*` |
|         - | 2168 | ` * Builtins whose accepted arity is NOT a contiguous range, so the signature` |
|         - | 2169 | ` * cannot express it and the central too-many-arguments check must stay out of` |
|         - | 2170 | ` * the way: rand()/mt_rand() take 0 OR 2 arguments (never 1), and php words the` |
|         - | 2171 | ` * violation "expects exactly 2 arguments, 3 given" from their own check rather` |
|         - | 2172 | ` * than the ZPP "at most". They validate themselves; leaving bHasMaxArg at 0` |
|         - | 2173 | ` * keeps their message php-faithful.` |
|         - | 2174 | ` */` |
|   3969600 | 2175 | `static int VmBuiltinSelfValidatesArity(const char *zName)` |
|         5 | 2176 | `{` |
|         - | 2177 | `	static const char *const azSelf[] = { "rand", "mt_rand" };` |
|         - | 2178 | `	sxu32 i;` |
|  11893919 | 2179 | `	for( i = 0 ; i < SX_ARRAYSIZE(azSelf) ; i++ ){` |
|   7934243 | 2180 | `		sxu32 nSelf = SyStrlen(azSelf[i]);` |
|   7934243 | 2181 | `		if( SyStrlen(zName) == nSelf && SyStrncmp(zName,azSelf[i],nSelf) == 0 ){` |
|      9929 | 2182 | `			return 1;` |
|         - | 2183 | `		}` |
|   3962162 | 2184 | `	}` |
|   3959681 | 2185 | `	return 0;` |
|   1984805 | 2186 | `}` |
|         - | 2187 | `/*` |
|         - | 2188 | ` * One parameter of a declared signature, for the named-argument binder below.` |
|         - | 2189 | ` */` |
|         - | 2190 | `typedef struct VmSigParam VmSigParam;` |
|         - | 2191 | `struct VmSigParam` |
|         - | 2192 | `{` |
|         - | 2193 | `	const char *zName; int nName;   /* without the '$' */` |
|         - | 2194 | `	const char *zDef;  int nDef;    /* default TEXT, or 0 when the parameter is required */` |
|         - | 2195 | `	int bVariadic;` |
|         - | 2196 | `};` |
|         - | 2197 | `/*` |
|         - | 2198 | ` * Split a signature into its parameters: the NAME each one binds by and the default` |
|         - | 2199 | ` * TEXT to fall back on. The scan is VmDeriveArityFromSig's, kept apart because that one` |
|         - | 2200 | `` * only counts; a quoted default (`string $separator = ','`) hides a comma, which is why`` |
|         - | 2201 | ` * both go through VmSigSkipQuoted.` |
|         - | 2202 | ` */` |
|     69086 | 2203 | `static int VmSigParams(const char *zSig,VmSigParam *aOut,int nMax)` |
|         5 | 2204 | `{` |
|     69091 | 2205 | `	const char *zCur = zSig;` |
|     69091 | 2206 | `	const char *zStart = zSig;` |
|     69091 | 2207 | `	int n = 0;` |
|   2722050 | 2208 | `	for(;;){` |
|   5649027 | 2209 | `		if( zCur[0] == '\'' \|\| zCur[0] == '"' ){` |
|        19 | 2210 | `			zCur = VmSigSkipQuoted(zCur);` |
|        19 | 2211 | `			if( zCur[0] != '\0' ){` |
|        19 | 2212 | `				zCur++;` |
|         9 | 2213 | `			}` |
|        19 | 2214 | `			continue;` |
|         - | 2215 | `		}` |
|   5649009 | 2216 | `		if( zCur[0] == '\0' \|\| zCur[0] == ',' ){` |
|    273995 | 2217 | `			const char *z = zStart;` |
|    273995 | 2218 | `			const char *zEnd = zCur;` |
|    273995 | 2219 | `			if( n < nMax ){` |
|    273995 | 2220 | `				VmSigParam *p = &aOut[n];` |
|    273995 | 2221 | `				const char *zEq = 0;` |
|    273995 | 2222 | `				const char *zDollar = 0;` |
|    273995 | 2223 | `				p->zName = 0; p->nName = 0; p->zDef = 0; p->nDef = 0; p->bVariadic = 0;` |
|   5649087 | 2224 | `				for( ; z < zEnd ; z++ ){` |
|   5375097 | 2225 | `					if( z[0] == '$' && zDollar == 0 ){` |
|    273995 | 2226 | `						zDollar = z + 1;` |
|   5238102 | 2227 | `					}else if( z[0] == '=' && zEq == 0 ){` |
|     72823 | 2228 | `						zEq = z + 1;` |
|   5064698 | 2229 | `					}else if( z[0] == '.' && z + 2 < zEnd && z[1] == '.' && z[2] == '.' ){` |
|       156 | 2230 | `						p->bVariadic = 1;` |
|        76 | 2231 | `					}` |
|   2687551 | 2232 | `				}` |
|    273995 | 2233 | `				if( zDollar ){` |
|    273995 | 2234 | `					const char *zStop = zEq ? zEq - 1 : zEnd;` |
|    273995 | 2235 | `					const char *zN = zDollar;` |
|   1994125 | 2236 | `					while( zN < zStop && zN[0] != ' ' && zN[0] != '=' ){` |
|   1720135 | 2237 | `						zN++;` |
|         5 | 2238 | `					}` |
|    273995 | 2239 | `					p->zName = zDollar;` |
|    273995 | 2240 | `					p->nName = (int)(zN - zDollar);` |
|    136995 | 2241 | `				}` |
|    273995 | 2242 | `				if( zEq ){` |
|    145641 | 2243 | `					while( zEq < zEnd && zEq[0] == ' ' ){` |
|     72823 | 2244 | `						zEq++;` |
|         5 | 2245 | `					}` |
|     72823 | 2246 | `					p->zDef = zEq;` |
|     72823 | 2247 | `					p->nDef = (int)(zEnd - zEq);` |
|     72823 | 2248 | `					while( p->nDef > 0 && p->zDef[p->nDef-1] == ' ' ){` |
|       ! 0 | 2249 | `						p->nDef--;` |
|       ! 0 | 2250 | `					}` |
|     36409 | 2251 | `				}` |
|    273995 | 2252 | `				if( p->nName > 0 ){` |
|    273995 | 2253 | `					n++;` |
|    136995 | 2254 | `				}` |
|    136995 | 2255 | `			}` |
|    273995 | 2256 | `			if( zCur[0] == '\0' ){` |
|     69091 | 2257 | `				break;` |
|         - | 2258 | `			}` |
|    204909 | 2259 | `			zCur++;` |
|    204909 | 2260 | `			zStart = zCur;` |
|    204909 | 2261 | `			continue;` |
|         - | 2262 | `		}` |
|   5375019 | 2263 | `		zCur++;` |
|         5 | 2264 | `	}` |
|     69091 | 2265 | `	return n;` |
|         5 | 2266 | `}` |
|         - | 2267 | `/*` |
|         - | 2268 | ` * Materialize a signature default's TEXT into pOut. php's own stub values, which is a` |
|         - | 2269 | `` * small set: null, true/false, an integer or float, a quoted string, and `[]`. A default`` |
|         - | 2270 | `` * the table could not state (`= ?`, ~50 rows — §7.4) answers 0, and the caller then reports`` |
|         - | 2271 | ` * the parameter as not passed rather than inventing a value.` |
|         - | 2272 | ` */` |
|         6 | 2273 | `static int VmSigDefaultValue(ph7_vm *pVm,const VmSigParam *pParam,ph7_value *pOut)` |
|         1 | 2274 | `{` |
|         7 | 2275 | `	const char *z = pParam->zDef;` |
|         7 | 2276 | `	int n = pParam->nDef;` |
|         7 | 2277 | `	if( z == 0 \|\| n < 1 \|\| (n == 1 && z[0] == '?') ){` |
|         3 | 2278 | `		return 0;` |
|         - | 2279 | `	}` |
|         5 | 2280 | `	if( n == 4 && (SyStrnicmp(z,"null",4) == 0) ){` |
|       ! 0 | 2281 | `		PH7_MemObjRelease(pOut);` |
|       ! 0 | 2282 | `		return 1; /* a released value IS null */` |
|         - | 2283 | `	}` |
|         5 | 2284 | `	if( n == 4 && SyStrnicmp(z,"true",4) == 0 ){` |
|       ! 0 | 2285 | `		PH7_MemObjInitFromBool(pVm,pOut,1);` |
|       ! 0 | 2286 | `		return 1;` |
|         - | 2287 | `	}` |
|         5 | 2288 | `	if( n == 5 && SyStrnicmp(z,"false",5) == 0 ){` |
|       ! 0 | 2289 | `		PH7_MemObjInitFromBool(pVm,pOut,0);` |
|       ! 0 | 2290 | `		return 1;` |
|         - | 2291 | `	}` |
|         5 | 2292 | `	if( n == 2 && z[0] == '[' && z[1] == ']' ){` |
|         3 | 2293 | `		ph7_hashmap *pMap = PH7_NewHashmap(pVm,0,0);` |
|         3 | 2294 | `		if( pMap == 0 ){` |
|       ! 0 | 2295 | `			return 0;` |
|         - | 2296 | `		}` |
|         3 | 2297 | `		PH7_MemObjRelease(pOut);` |
|         3 | 2298 | `		pOut->x.pOther = pMap;` |
|         3 | 2299 | `		MemObjSetType(pOut,MEMOBJ_HASHMAP);` |
|         3 | 2300 | `		return 1;` |
|         - | 2301 | `	}` |
|         3 | 2302 | `	if( z[0] == '\'' \|\| z[0] == '"' ){` |
|         - | 2303 | `		SyString sStr;` |
|         3 | 2304 | `		SyStringInitFromBuf(&sStr,z + 1,n >= 2 ? n - 2 : 0);` |
|         3 | 2305 | `		PH7_MemObjInitFromString(pVm,pOut,&sStr);` |
|         3 | 2306 | `		return 1;` |
|         - | 2307 | `	}` |
|       ! 0 | 2308 | `	if( z[0] == '-' \|\| z[0] == '+' \|\| (z[0] >= '0' && z[0] <= '9') ){` |
|         - | 2309 | `		SyString sNum;` |
|       ! 0 | 2310 | `		SyStringInitFromBuf(&sNum,z,(sxu32)n);` |
|       ! 0 | 2311 | `		if( PH7_MemObjInitFromString(pVm,pOut,&sNum) != SXRET_OK ){` |
|       ! 0 | 2312 | `			return 0;` |
|         - | 2313 | `		}` |
|       ! 0 | 2314 | `		PH7_MemObjToNumeric(pOut);` |
|       ! 0 | 2315 | `		return 1;` |
|         - | 2316 | `	}` |
|       ! 0 | 2317 | `	return 0; /* a constant expression (M_PI, PHP_ROUND_HALF_UP, …): not evaluated here */` |
|         4 | 2318 | `}` |
|         - | 2319 | `/*` |
|         - | 2320 | ` * Bind a call's NAMED arguments to the callee's declared parameter POSITIONS.` |
|         - | 2321 | ` *` |
|         - | 2322 | ` * A compiled function does this from its parameter records (VmResolveNamedArgs); a host` |
|         - | 2323 | ` * function and a native method have none, so every named argument was simply passed in the` |
|         - | 2324 | `` * order it was WRITTEN. `str_pad(length: 5, string: "x")` reached the builtin as`` |
|         - | 2325 | ` * ("x" at #2, 5 at #1) and reported a TypeError, and — worse, because it is silent —` |
|         - | 2326 | `` * `str_pad("x", 5, pad_type: STR_PAD_LEFT)` bound the flag as $pad_string and answered`` |
|         - | 2327 | ` * "x0000" where php answers "    x". Both spellings are php 8.0 syntax, and the whole` |
|         - | 2328 | ` * ~650-builtin surface plus every native method was affected.` |
|         - | 2329 | ` *` |
|         - | 2330 | ` * The declared signature is the source of names, defaults and positions — the same string` |
|         - | 2331 | ` * Reflection prints. Rewrites *pnArg / apArg in place (the caller's argument vector is` |
|         - | 2332 | ` * scratch it owns) and answers SXRET_OK, or throws php's Error and returns its status.` |
|         - | 2333 | ` * Callees with a VARIADIC tail are left alone: php collects extra named arguments into it` |
|         - | 2334 | ` * by NAME, which the positional vector here cannot express.` |
|         - | 2335 | ` */` |
|        94 | 2336 | `PH7_PRIVATE sxi32 PH7_VmBindNamedArgsToSig(` |
|         - | 2337 | `	ph7_context *pCtx,      /* Call context (for the throws) */` |
|         - | 2338 | `	ph7_user_func *pFunc,   /* Callee: its zSig names the parameters */` |
|         - | 2339 | `	VmCallArgMap *pMap,     /* Call-site map; its aNames[] are per ACTUAL slot */` |
|         - | 2340 | `	int *pnArg,             /* IN/OUT: argument count */` |
|         - | 2341 | `	ph7_value **apArg       /* IN/OUT: argument vector */` |
|         - | 2342 | `	)` |
|         2 | 2343 | `{` |
|         - | 2344 | `	/* php's own stubs top out well under this; a signature with more parameters simply` |
|         - | 2345 | `	 * keeps the positional binding it had. */` |
|         - | 2346 | `#define VM_SIG_MAX_PARAM 32` |
|         - | 2347 | `	VmSigParam aParam[VM_SIG_MAX_PARAM];` |
|         - | 2348 | `	ph7_value *apBound[VM_SIG_MAX_PARAM];` |
|         - | 2349 | `	int nParam,nArg,i,nLast;` |
|        96 | 2350 | `	if( pFunc == 0 \|\| pFunc->zSig == 0 \|\| pMap == 0 \|\| pMap->bHasNamed == 0 ){` |
|        13 | 2351 | `		return SXRET_OK;` |
|         - | 2352 | `	}` |
|        84 | 2353 | `	nArg = *pnArg;` |
|        84 | 2354 | `	if( nArg < 1 \|\| nArg > VM_SIG_MAX_PARAM ){` |
|       ! 0 | 2355 | `		return SXRET_OK;` |
|         - | 2356 | `	}` |
|        84 | 2357 | `	nParam = VmSigParams(pFunc->zSig,aParam,VM_SIG_MAX_PARAM);` |
|        84 | 2358 | `	if( nParam < 1 \|\| aParam[nParam-1].bVariadic ){` |
|        21 | 2359 | `		return SXRET_OK;` |
|         - | 2360 | `	}` |
|       252 | 2361 | `	for( i = 0 ; i < nParam ; ++i ){` |
|       190 | 2362 | `		apBound[i] = 0;` |
|        96 | 2363 | `	}` |
|        64 | 2364 | `	nLast = -1;` |
|       198 | 2365 | `	for( i = 0 ; i < nArg ; ++i ){` |
|       142 | 2366 | `		int p = i;` |
|       202 | 2367 | `		if( i < (int)pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|       128 | 2368 | `			SyString *pName = &pMap->aNames[i];` |
|       246 | 2369 | `			for( p = 0 ; p < nParam ; ++p ){` |
|       240 | 2370 | `				if( (int)pName->nByte == aParam[p].nName` |
|       199 | 2371 | `				 && SyMemcmp(pName->zString,aParam[p].zName,pName->nByte) == 0 ){` |
|       124 | 2372 | `					break;` |
|         - | 2373 | `				}` |
|        60 | 2374 | `			}` |
|       128 | 2375 | `			if( p >= nParam ){` |
|         7 | 2376 | `				return PH7_VmThrowException(pCtx,"Error",` |
|         2 | 2377 | `					"Unknown named parameter $%z",pName);` |
|         - | 2378 | `			}` |
|       124 | 2379 | `			if( apBound[p] ){` |
|         4 | 2380 | `				return PH7_VmThrowException(pCtx,"Error",` |
|         1 | 2381 | `					"Named parameter $%z overwrites previous argument",pName);` |
|         2 | 2382 | `			}` |
|        75 | 2383 | `		}else if( p >= nParam ){` |
|       ! 0 | 2384 | `			return SXRET_OK; /* more positional arguments than the signature knows */` |
|         - | 2385 | `		}` |
|       136 | 2386 | `		apBound[p] = apArg[i];` |
|       136 | 2387 | `		if( p > nLast ){` |
|       114 | 2388 | `			nLast = p;` |
|        56 | 2389 | `		}` |
|        69 | 2390 | `	}` |
|       188 | 2391 | `	for( i = 0 ; i <= nLast ; ++i ){` |
|       134 | 2392 | `		if( apBound[i] == 0 ){` |
|         7 | 2393 | `			ph7_value *pDef = ph7_context_new_scalar(pCtx);` |
|         7 | 2394 | `			if( pDef == 0 \|\| !VmSigDefaultValue(pCtx->pVm,&aParam[i],pDef) ){` |
|         - | 2395 | `				SyString sName;` |
|         3 | 2396 | `				SyStringInitFromBuf(&sName,aParam[i].zName,(sxu32)aParam[i].nName);` |
|         4 | 2397 | `				return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|         1 | 2398 | `					"%z(): Argument #%d ($%z) not passed",&pFunc->sName,i + 1,&sName);` |
|         - | 2399 | `			}` |
|         5 | 2400 | `			apBound[i] = pDef;` |
|         2 | 2401 | `		}` |
|        67 | 2402 | `	}` |
|       184 | 2403 | `	for( i = 0 ; i <= nLast ; ++i ){` |
|       130 | 2404 | `		apArg[i] = apBound[i];` |
|        66 | 2405 | `	}` |
|        56 | 2406 | `	*pnArg = nLast + 1;` |
|        56 | 2407 | `	return SXRET_OK;` |
|        49 | 2408 | `}` |
|         - | 2409 | `/*` |
|         - | 2410 | ` * Name the Nth (0-based) parameter of a declared signature, without the '$'.` |
|         - | 2411 | ` *` |
|         - | 2412 | ` * The signature string is the only place a host function's parameter names live, and` |
|         - | 2413 | `` * php puts them in diagnostics — `sort(): Argument #1 ($array) …`. Answers 0 when the`` |
|         - | 2414 | ` * signature has no such parameter (or none with a name).` |
|         - | 2415 | ` */` |
|        12 | 2416 | `PH7_PRIVATE int PH7_VmSigParamName(const char *zSig,int nPos,SyString *pOut)` |
|         2 | 2417 | `{` |
|         - | 2418 | `	VmSigParam aParam[VM_SIG_MAX_PARAM];` |
|         - | 2419 | `	int nParam;` |
|        14 | 2420 | `	if( zSig == 0 \|\| nPos < 0 \|\| nPos >= VM_SIG_MAX_PARAM ){` |
|       ! 0 | 2421 | `		return 0;` |
|         - | 2422 | `	}` |
|        14 | 2423 | `	nParam = VmSigParams(zSig,aParam,VM_SIG_MAX_PARAM);` |
|        14 | 2424 | `	if( nPos >= nParam \|\| aParam[nPos].nName < 1 ){` |
|       ! 0 | 2425 | `		return 0;` |
|         - | 2426 | `	}` |
|        14 | 2427 | `	if( aParam[nPos].bVariadic ){` |
|         - | 2428 | ``		/* php's get_function_arg_name() answers NULL past `num_args`, which`` |
|         - | 2429 | `		 * counts the non-variadic parameters alone -- so an actual absorbed by` |
|         - | 2430 | ``		 * a `...` tail is named in no diagnostic (`sscanf(): Argument #3 must`` |
|         - | 2431 | ``		 * be passed by reference, value given`, with no ` ($vars)`). */`` |
|       ! 0 | 2432 | `		return 0;` |
|         - | 2433 | `	}` |
|        14 | 2434 | `	SyStringInitFromBuf(pOut,aParam[nPos].zName,(sxu32)aParam[nPos].nName);` |
|        14 | 2435 | `	return 1;` |
|         8 | 2436 | `}` |
|         - | 2437 | `/*` |
|         - | 2438 | `` * A `&` in a builtin's signature is not always php's ZEND_SEND_ARG_BY_REF.`` |
|         - | 2439 | ` *` |
|         - | 2440 | ` * php has a second mode, ZEND_SEND_PREFER_REF: bind by reference when the argument IS a` |
|         - | 2441 | ` * variable, and otherwise take it by value without a word. Reflection prints those` |
|         - | 2442 | ` * parameters as by-reference like any other and PHL's signature string cannot say which` |
|         - | 2443 | `` * mode a `&` means, so the two are told apart here. Probed value-for-value against php`` |
|         - | 2444 | `` * 8.5 over every `&` row PHL declares (41 of them): all but extract() refuse a`` |
|         - | 2445 | `` * non-variable, and extract() answers `int(1)` for `extract(['q' => 1])`.`` |
|         - | 2446 | ` *` |
|         - | 2447 | ` * array_multisort() is listed with it because it is php's other prefer-ref builtin and` |
|         - | 2448 | ` * PHL will need this the day it gains one (it is a MISSING builtin today, §5).` |
|         - | 2449 | ` */` |
|     69154 | 2450 | `PH7_PRIVATE int VmBuiltinPrefersRef(SyString *pName)` |
|         5 | 2451 | `{` |
|         - | 2452 | `	static const char *const azPreferRef[] = { "extract", "array_multisort" };` |
|         - | 2453 | `	sxu32 i;` |
|    207271 | 2454 | `	for( i = 0 ; i < SX_ARRAYSIZE(azPreferRef) ; ++i ){` |
|    138233 | 2455 | `		sxu32 nByte = SyStrlen(azPreferRef[i]);` |
|    138228 | 2456 | `		if( pName->nByte == nByte` |
|     69237 | 2457 | `		 && SyMemcmp(pName->zString,azPreferRef[i],nByte) == 0 ){` |
|       120 | 2458 | `			return 1;` |
|         - | 2459 | `		}` |
|     69061 | 2460 | `	}` |
|     69043 | 2461 | `	return 0;` |
|     34582 | 2462 | `}` |
|         - | 2463 | `/*` |
|         - | 2464 | ` * php refuses a by-reference argument at the CALL, before the callee's ZPP runs, and it` |
|         - | 2465 | ``  * decides from the argument's SHAPE, not from its value: `sort([3,1])`, `usort('x',$cb)` `` |
|         - | 2466 | `` * and `preg_match($p,$s,'lit')` are all`` |
|         - | 2467 | `` * `Error: sort(): Argument #1 ($array) could not be passed by reference`.`` |
|         - | 2468 | ` *` |
|         - | 2469 | ` * The call site's compile-time shape mask (VmCallArgMap.nNonLvalMask) is what says so.` |
|         - | 2470 | ` * Only five builtins raised anything before this, from their own bodies, on the runtime` |
|         - | 2471 | `` * `nIdx == SXU32_HIGH` signal — which cannot tell a literal from the result of a call, a`` |
|         - | 2472 | `` * shape php ACCEPTS with a notice. The thirty other `&` rows answered `true`/`false`/an`` |
|         - | 2473 | ` * int: the same answers they give for work they really did.` |
|         - | 2474 | ` *` |
|         - | 2475 | ` * Skipped when the call site has no shape mask (a spread, an indirect dispatch through` |
|         - | 2476 | ` * call_user_func, an engine-synthesized call) or uses named arguments (which rebind` |
|         - | 2477 | ` * positions the mask is indexed by). The by-ref positions come from the same declared` |
|         - | 2478 | ` * signature everything else here reads.` |
|         - | 2479 | ` */` |
|   6045493 | 2480 | `PH7_PRIVATE sxi32 PH7_VmScreenByRefArgShapes(` |
|         - | 2481 | `	ph7_context *pCtx,     /* Call context (for the throw) */` |
|         - | 2482 | `	ph7_user_func *pFunc,  /* Callee: its zSig names and marks the parameters */` |
|         - | 2483 | `	VmCallArgMap *pMap,    /* Call-site map, or 0 */` |
|         - | 2484 | `	int nGiven,            /* Argument count */` |
|         - | 2485 | `	ph7_value **apArg      /* Arguments */` |
|         - | 2486 | `	)` |
|         5 | 2487 | `{` |
|         - | 2488 | `	VmSigParam aParam[VM_SIG_MAX_PARAM];` |
|         - | 2489 | `	int nParam,n;` |
|         - | 2490 | `	/* The by-ref mask first: it is 0 for all but 41 of the ~650 host functions, so` |
|         - | 2491 | `	 * every other call leaves through one test. */` |
|   6045498 | 2492 | `	if( pFunc == 0 \|\| pFunc->nByRefMask == 0 \|\| pFunc->zSig == 0 \|\| nGiven < 1 ){` |
|   5975098 | 2493 | `		return SXRET_OK;` |
|         - | 2494 | `	}` |
|     70405 | 2495 | `	if( pMap == 0 \|\| !pMap->bArgShapes \|\| pMap->bHasNamed ){` |
|        73 | 2496 | `		return SXRET_OK;` |
|         - | 2497 | `	}` |
|     70335 | 2498 | `	if( (pMap->nNonLvalMask \| pMap->nTempCallMask) == 0 ){` |
|      1231 | 2499 | `		return SXRET_OK;` |
|         - | 2500 | `	}` |
|     69109 | 2501 | `	if( VmBuiltinPrefersRef(&pFunc->sName) ){` |
|       116 | 2502 | `		return SXRET_OK;` |
|         - | 2503 | `	}` |
|     68997 | 2504 | `	nParam = VmSigParams(pFunc->zSig,aParam,VM_SIG_MAX_PARAM);` |
|    273309 | 2505 | `	for( n = 0 ; n < nGiven && n < 31 ; ++n ){` |
|    204369 | 2506 | `		if( (pFunc->nByRefMask & (1u << n)) == 0 ){` |
|    200903 | 2507 | `			continue;` |
|         - | 2508 | `		}` |
|      3471 | 2509 | `		if( (pMap->nNonLvalMask & (1u << n)) == 0 ){` |
|         - | 2510 | `			/* Not a refusal — but a CALL result in this position is php's notice,` |
|         - | 2511 | `			 * and then the builtin operates on the temporary. */` |
|      3419 | 2512 | `			PH7_VmArgTempCallNotice(pCtx->pVm,pMap,(sxu32)n,apArg[n]);` |
|      3419 | 2513 | `			continue;` |
|         - | 2514 | `		}` |
|         - | 2515 | `		/* php names the parameter only when the position is a DECLARED one:` |
|         - | 2516 | ``		 * get_function_arg_name() answers NULL past `num_args`, which counts`` |
|         - | 2517 | `` 		 * the non-variadic parameters alone. So an actual absorbed by a `&...` `` |
|         - | 2518 | ``		 * tail (sscanf's `&...$vars`) is refused without a name. */`` |
|        57 | 2519 | `		if( n < nParam && aParam[n].nName > 0 && !aParam[n].bVariadic ){` |
|        80 | 2520 | `			return PH7_VmThrowException(pCtx,"Error",` |
|         - | 2521 | `				"%z(): Argument #%d ($%.*s) could not be passed by reference",` |
|        25 | 2522 | `				&pFunc->sName,n + 1,aParam[n].nName,aParam[n].zName);` |
|         - | 2523 | `		}` |
|         4 | 2524 | `		return PH7_VmThrowException(pCtx,"Error",` |
|         - | 2525 | `			"%z(): Argument #%d could not be passed by reference",` |
|         1 | 2526 | `			&pFunc->sName,n + 1);` |
|       ! 0 | 2527 | `	}` |
|     68945 | 2528 | `	return SXRET_OK;` |
|   3024285 | 2529 | `}` |
|         - | 2530 | `/*` |
|         - | 2531 | ` * D1: derive a by-reference position bitmask from a php-style signature string.` |
|         - | 2532 | `` * Bit N is set when positional parameter N is declared by-reference (a `&` appears`` |
|         - | 2533 | `` * anywhere in that comma-separated parameter, e.g. `&$matches`, `&...$vars`). Only`` |
|         - | 2534 | ` * the first 31 positions are representable; a by-ref parameter past that is rare` |
|         - | 2535 | ` * for a builtin and simply not tracked here. The scan mirrors VmDeriveArityFromSig.` |
|         - | 2536 | ` */` |
|  10415838 | 2537 | `PH7_PRIVATE sxu32 VmDeriveByRefMaskFromSig(const char *zSig)` |
|         5 | 2538 | `{` |
|  10415843 | 2539 | `	sxu32 mask = 0;` |
|  10415843 | 2540 | `	int n = 0;       /* current parameter index */` |
|  10415843 | 2541 | `	int bSeen = 0;   /* current parameter has non-space content */` |
|  10415843 | 2542 | ``	int bRef = 0;    /* current parameter carries a by-ref `&` */`` |
|  10415843 | 2543 | ``	int bVar = 0;    /* current parameter is a `...` variadic */`` |
|  10415843 | 2544 | `	int bTailRef = 0;/* the LAST parameter was a by-ref variadic */` |
|  10415843 | 2545 | `	const char *zCur = zSig;` |
| 107545660 | 2546 | `	for(;;){` |
| 221034223 | 2547 | `		if( zCur[0] == '\'' \|\| zCur[0] == '"' ){` |
|    401719 | 2548 | `			bSeen = 1;` |
|    401719 | 2549 | `			zCur = VmSigSkipQuoted(zCur);` |
|    401719 | 2550 | `			if( zCur[0] != '\0' ){` |
|    401719 | 2551 | `				zCur++;` |
|    200857 | 2552 | `			}` |
|    401719 | 2553 | `			continue;` |
|         - | 2554 | `		}` |
| 220632509 | 2555 | `		if( zCur[0] == '\0' \|\| zCur[0] == ',' ){` |
|  15957027 | 2556 | `			if( bSeen ){` |
|  12166835 | 2557 | `				if( bRef && n < 31 ){` |
|    346273 | 2558 | `					mask \|= (1u << n);` |
|    173134 | 2559 | `				}` |
|  12166835 | 2560 | `				bTailRef = (bRef && bVar);` |
|  12166835 | 2561 | `				n++;` |
|   6083415 | 2562 | `			}` |
|  15957027 | 2563 | `			if( zCur[0] == '\0' ){` |
|  10415843 | 2564 | `				break;` |
|         - | 2565 | `			}` |
|   5541189 | 2566 | `			bSeen = bRef = bVar = 0;` |
|   5541189 | 2567 | `			zCur++;` |
|   5541189 | 2568 | `			continue;` |
|         - | 2569 | `		}` |
| 204675487 | 2570 | `		if( zCur[0] != ' ' ){` |
| 179781293 | 2571 | `			bSeen = 1;` |
|  89890644 | 2572 | `		}` |
| 204675487 | 2573 | `		if( zCur[0] == '&' ){` |
|    346273 | 2574 | `			bRef = 1;` |
|    173134 | 2575 | `		}` |
| 204675487 | 2576 | `		if( zCur[0] == '.' && zCur[1] == '.' && zCur[2] == '.' ){` |
|         - | 2577 | ``			/* A `...` tail, not a numeric default's decimal point. */`` |
|    321359 | 2578 | `			bVar = 1;` |
|    160677 | 2579 | `		}` |
| 204675487 | 2580 | `		zCur++;` |
|         5 | 2581 | `	}` |
|  10415843 | 2582 | `	if( bTailRef && n > 0 && n <= 31 ){` |
|         - | 2583 | ``		/* A by-ref `&...` tail absorbs every later actual (array_multisort's`` |
|         - | 2584 | ``		 * `&...$rest`): without this, the deferred-argument resolver read the`` |
|         - | 2585 | ``		 * tail positions as by-VALUE and warned `Undefined variable` on an`` |
|         - | 2586 | `		 * undefined actual php binds silently. */` |
|     20631 | 2587 | `		mask \|= ~((1u << (n - 1)) - 1u);` |
|     10313 | 2588 | `	}` |
|  10415843 | 2589 | `	return mask;` |
|         5 | 2590 | `}` |
|      4962 | 2591 | `PH7_PRIVATE void VmSetBuiltinSignatures(ph7_vm *pVm)` |
|         5 | 2592 | `{` |
|         - | 2593 | `	sxu32 n;` |
|   4024187 | 2594 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinSig) ; n++ ){` |
|   6028835 | 2595 | `		SyHashEntry *pEntry = SyHashGet(&pVm->hHostFunction,` |
|   4019220 | 2596 | `			(const void *)aBuiltinSig[n].zName,(sxu32)SyStrlen(aBuiltinSig[n].zName));` |
|   4019225 | 2597 | `		if( pEntry ){` |
|   3969605 | 2598 | `			ph7_user_func *pFunc = (ph7_user_func *)pEntry->pUserData;` |
|   3969605 | 2599 | `			sxi16 nMin = 0, nMax = 0;` |
|   3969605 | 2600 | `			sxu8 bAtLeast = 0, bHasMax = 0;` |
|   3969605 | 2601 | `			pFunc->zSig = aBuiltinSig[n].zSig;` |
|   3969605 | 2602 | `			pFunc->zRet = aBuiltinSig[n].zRet[0] ? aBuiltinSig[n].zRet : 0;` |
|   3969605 | 2603 | `			pFunc->nByRefMask = VmDeriveByRefMaskFromSig(pFunc->zSig);` |
|   3969605 | 2604 | `			VmDeriveArityFromSig(pFunc->zSig,&nMin,&bAtLeast,&nMax,&bHasMax);` |
|         - | 2605 | `			/* The MAXIMUM always comes from the signature: the curated override` |
|         - | 2606 | `			 * table speaks only to the minimum (and its wording). */` |
|   3969605 | 2607 | `			pFunc->nMaxArg = nMax;` |
|   3969605 | 2608 | `			pFunc->bHasMaxArg = (sxu8)(VmBuiltinSelfValidatesArity(aBuiltinSig[n].zName) ? 0 : bHasMax);` |
|   3969605 | 2609 | `			if( pFunc->nMinArg < 1 ){` |
|         - | 2610 | `				/* VmSetBuiltinArity() ran first: a non-zero minimum here means the` |
|         - | 2611 | `				 * curated override already spoke for this builtin, so leave it. */` |
|   2540549 | 2612 | `				pFunc->nMinArg = nMin;` |
|   2540549 | 2613 | `				pFunc->bAtLeast = bAtLeast;` |
|   1270272 | 2614 | `			}` |
|   1984800 | 2615 | `		}` |
|   2009615 | 2616 | `	}` |
|      4967 | 2617 | `}` |
|         - | 2618 | `/*` |
|         - | 2619 | ` * Signature lookup by function name, for the reflection layer: embedded-PHP` |
|         - | 2620 | ` * builtins (max/min/Exception methods...) are ph7_vm_func instances, not` |
|         - | 2621 | ` * host functions, so they miss the VmSetBuiltinSignatures stamping and pull` |
|         - | 2622 | ` * their row on demand here. Linear scan — reflection-path only.` |
|         - | 2623 | ` */` |
|       ! 0 | 2624 | `PH7_PRIVATE const char * PH7_VmBuiltinSigLookup(const char *zName,sxu32 nLen,const char **pzRet)` |
|       ! 0 | 2625 | `{` |
|         - | 2626 | `	sxu32 n;` |
|       ! 0 | 2627 | `	if( pzRet ){` |
|       ! 0 | 2628 | `		*pzRet = 0;` |
|       ! 0 | 2629 | `	}` |
|       ! 0 | 2630 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinSig) ; n++ ){` |
|       ! 0 | 2631 | `		if( SyStrlen(aBuiltinSig[n].zName) == nLen` |
|       ! 0 | 2632 | `		 && SyMemcmp(aBuiltinSig[n].zName,zName,nLen) == 0 ){` |
|       ! 0 | 2633 | `			if( pzRet && aBuiltinSig[n].zRet[0] ){` |
|       ! 0 | 2634 | `				*pzRet = aBuiltinSig[n].zRet;` |
|       ! 0 | 2635 | `			}` |
|       ! 0 | 2636 | `			return aBuiltinSig[n].zSig;` |
|         - | 2637 | `		}` |
|       ! 0 | 2638 | `	}` |
|       ! 0 | 2639 | `	return 0;` |
|       ! 0 | 2640 | `}` |
|         - | 2641 | `/*` |
|         - | 2642 | ` * Write a value back to the caller's variable through a builtin argument's` |
|         - | 2643 | ` * stack-slot nIdx — the shared write-back for builtin by-reference` |
|         - | 2644 | ` * out-parameters (preg_match $matches, preg_replace &$count, similar_text` |
|         - | 2645 | ` * &$percent, ...).` |
|         - | 2646 | ` *` |
|         - | 2647 | ` * For a positional out-param argument the call compiler auto-vivifies known` |
|         - | 2648 | ` * by-reference out-params (see GenStateByRefBuiltinMask in compile.c), so a` |
|         - | 2649 | ` * bare undefined variable, an array subscript, and a declared/untyped` |
|         - | 2650 | ` * property all arrive with a real nIdx and are written back here, matching` |
|         - | 2651 | ` * PHP's reference semantics.` |
|         - | 2652 | ` *` |
|         - | 2653 | ` * nIdx stays SXU32_HIGH and the write-back to the caller is skipped (the` |
|         - | 2654 | ` * value still lands in the local stack slot) when the argument cannot expose` |
|         - | 2655 | ` * a stable memobj slot: a literal, a function-call result, a subscript of a` |
|         - | 2656 | ` * non-lvalue parent (foo()['k']), or any variable in a call that also uses` |
|         - | 2657 | ` * named or spread arguments (compile-time positions no longer map to the` |
|         - | 2658 | ` * runtime arg slots, so the compiler conservatively does not vivify). An` |
|         - | 2659 | ` * uninitialized typed property is also not wired (it throws before the` |
|         - | 2660 | ` * write) -- see the recorded deferrals.` |
|         - | 2661 | ` */` |
|      1216 | 2662 | `PH7_PRIVATE void PH7_VmStoreArgByRef(ph7_vm *pVm,ph7_value *pArg,ph7_value *pNewVal)` |
|         5 | 2663 | `{` |
|      1221 | 2664 | `	if( pArg->nIdx != SXU32_HIGH ){` |
|      1169 | 2665 | `		ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pArg->nIdx);` |
|      1169 | 2666 | `		if( pObj ){` |
|      1169 | 2667 | `			PH7_MemObjStore(pNewVal,pObj);` |
|       584 | 2668 | `		}` |
|       584 | 2669 | `	}` |
|      1221 | 2670 | `	PH7_MemObjStore(pNewVal,pArg);` |
|      1221 | 2671 | `}` |
|         - | 2672 | `/*` |
|         - | 2673 | ` * Raise an E_WARNING whose text is used VERBATIM.` |
|         - | 2674 | ` * ph7_context_throw_error_format() prepends "func(): " to whatever it is given, but php's` |
|         - | 2675 | ` * IO warnings put the offending path inside those parens -- "file_get_contents(/nope):` |
|         - | 2676 | ` * Failed to open stream: ..." -- so a caller that needs php's exact shape must build the` |
|         - | 2677 | ` * whole line itself and come through here.` |
|         - | 2678 | ` */` |
|         - | 2679 | `/*` |
|         - | 2680 | ` * Validate a callback argument and throw php's TypeError when it cannot be called.` |
|         - | 2681 | ` *` |
|         - | 2682 | ` * The shared dispatcher (PH7_VmCallUserFunctionWithMap) answers SXRET_OK with a NULL` |
|         - | 2683 | ` * result for an unresolvable callable, so every caller that did not check first failed` |
|         - | 2684 | ` * SILENTLY: call_user_func() returned NULL and usort() left the array sorted by nothing` |
|         - | 2685 | ` * at all. php rejects the ARGUMENT up front, naming exactly why.` |
|         - | 2686 | ` */` |
|    399516 | 2687 | `PH7_PRIVATE sxi32 PH7_CheckCallbackArg(` |
|         - | 2688 | `	ph7_context *pCtx,   /* Calling context (names the function in the message) */` |
|         - | 2689 | `	ph7_value *pCb,      /* The callback argument */` |
|         - | 2690 | `	int iArg,            /* Its 1-based position */` |
|         - | 2691 | `	const char *zParam,  /* Its php parameter name ("callback"), or 0 for the variadic` |
|         - | 2692 | `	                      * comparators php names by position only (array_udiff …) */` |
|         - | 2693 | `	int bNullable        /* TRUE when php's text says "or null" */` |
|         - | 2694 | `	)` |
|         5 | 2695 | `{` |
|         - | 2696 | `	char zReason[256];` |
|    399521 | 2697 | `	const char *zWhy = PH7_VmCallableReason(pCtx->pVm,pCb,zReason,sizeof(zReason));` |
|    399521 | 2698 | `	if( zWhy == 0 ){` |
|    399293 | 2699 | `		return PH7_OK;` |
|         - | 2700 | `	}` |
|       233 | 2701 | `	if( zParam ){` |
|       266 | 2702 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 2703 | `			"%s(): Argument #%d ($%s) must be a valid callback%s, %s",` |
|        87 | 2704 | `			ph7_function_name(pCtx),iArg,zParam,bNullable ? " or null" : "",zWhy);` |
|         - | 2705 | `	}` |
|        86 | 2706 | `	return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 2707 | `		"%s(): Argument #%d must be a valid callback%s, %s",` |
|        27 | 2708 | `		ph7_function_name(pCtx),iArg,bNullable ? " or null" : "",zWhy);` |
|    199763 | 2709 | `}` |
|     27816 | 2710 | `PH7_PRIVATE void PH7_VmThrowWarningFmt(ph7_vm *pVm,const char *zFmt,...)` |
|         5 | 2711 | `{` |
|         - | 2712 | `	va_list ap;` |
|     27821 | 2713 | `	va_start(ap,zFmt);` |
|     27821 | 2714 | `	PH7_VmThrowErrorAp(pVm,0,PH7_CTX_WARNING,zFmt,ap);` |
|     27821 | 2715 | `	va_end(ap);` |
|     27821 | 2716 | `}` |
|         - | 2717 | `/*` |
|         - | 2718 | ` * Emit a formatted E_USER_WARNING with no function-name prefix: php reports` |
|         - | 2719 | ` * #[\NoDiscard] as a USER warning (512) for the same reason it reports` |
|         - | 2720 | ` * #[\Deprecated] as a USER deprecation — the attribute is userland-authored,` |
|         - | 2721 | ` * and a set_error_handler sees the number.` |
|         - | 2722 | ` */` |
|        56 | 2723 | `static void VmThrowUserWarningFmt(ph7_vm *pVm,const char *zFmt,...)` |
|         1 | 2724 | `{` |
|         - | 2725 | `	va_list ap;` |
|        57 | 2726 | `	va_start(ap,zFmt);` |
|        57 | 2727 | `	PH7_VmThrowErrorAp(pVm,0,E_USER_WARNING,zFmt,ap);` |
|        57 | 2728 | `	va_end(ap);` |
|        57 | 2729 | `}` |
|         - | 2730 | `/*` |
|         - | 2731 | ` * Emit a formatted E_USER_DEPRECATED diagnostic with no function-name prefix:` |
|         - | 2732 | ` * php reports #[\Deprecated] as USER-deprecated (16384, it is userland-authored),` |
|         - | 2733 | ` * not the engine's E_DEPRECATED (8192) — handler-visible errno matters.` |
|         - | 2734 | ` */` |
|        38 | 2735 | `static void VmThrowUserDeprecatedFmt(ph7_vm *pVm,const char *zFmt,...)` |
|         2 | 2736 | `{` |
|         - | 2737 | `	va_list ap;` |
|        40 | 2738 | `	va_start(ap,zFmt);` |
|        40 | 2739 | `	PH7_VmThrowErrorAp(pVm,0,E_USER_DEPRECATED,zFmt,ap);` |
|        40 | 2740 | `	va_end(ap);` |
|        40 | 2741 | `}` |
|         - | 2742 | `/*` |
|         - | 2743 | ` * php 8.4 #[\Deprecated]: emit the E_USER_DEPRECATED notice for a call to a user` |
|         - | 2744 | ` * function/method carrying the attribute. Message shapes (php-exact):` |
|         - | 2745 | ` *   Function f() is deprecated` |
|         - | 2746 | ` *   Method C::m() is deprecated since 2.0, use g() instead` |
|         - | 2747 | ` * ("since" from the attribute's since: argument; the trailing ", msg" from` |
|         - | 2748 | ` * message:/positional #1.) The attribute arguments are compiled constant` |
|         - | 2749 | ` * expressions — evaluated via VmLocalExec, the reflection thunks' pattern.` |
|         - | 2750 | ` * Called from OP_CALL once per call, only when the callee HAS attributes;` |
|         - | 2751 | ` * pDeclClass names the method's declaring class (0 for plain functions).` |
|         - | 2752 | ` */` |
|         - | 2753 | `/*` |
|         - | 2754 | ` * Expand a global constant's value, emitting the php 8.5 #[\Deprecated]` |
|         - | 2755 | ` * E_USER_DEPRECATED notice first when the constant carries attributes` |
|         - | 2756 | `` * (`Constant GD is deprecated since 1.2` — every access re-warns).`` |
|         - | 2757 | ` */` |
|         - | 2758 | `static void VmDeprecatedAttrNoticeSubject(ph7_vm *pVm,SySet *pAttrs,` |
|         - | 2759 | `	const char *zKind,const SyString *pQual,const SyString *pName);` |
|         - | 2760 | `/*` |
|         - | 2761 | ` * Expand a constant, emitting the php 8.5 #[\Deprecated] E_USER_DEPRECATED notice` |
|         - | 2762 | `` * first when a USERLAND constant carries the attribute (`Constant GD is deprecated`` |
|         - | 2763 | `` * since 1.2` — every access re-warns). Engine constants php merely deprecates are`` |
|         - | 2764 | ` * not mimicked: PHL removes them outright (see the scope policy), so there is no` |
|         - | 2765 | ` * engine-side E_DEPRECATED list here.` |
|         - | 2766 | ` *` |
|         - | 2767 | `` * A `const NAME = <expr>;` statement compiles its initializer to a bytecode`` |
|         - | 2768 | ` * program and PH7_VmExpandConstantValue RUNS it — so the value was re-computed` |
|         - | 2769 | ` * on EVERY read. For anything with an identity or a side effect that is a wrong` |
|         - | 2770 | `` * answer, not a slow one: `const C = new Foo();` gave a DIFFERENT object each`` |
|         - | 2771 | `` * time (`C === C` was false, and `Foo::$count` counted one construction per`` |
|         - | 2772 | ` * read) where php evaluates the initializer once and hands the same value out` |
|         - | 2773 | ` * for ever. The first successful expansion is kept, and the constant becomes an` |
|         - | 2774 | ` * ordinary value-backed one — exactly the shape define() registers, so` |
|         - | 2775 | ` * redefinition frees it through the path that already existed.` |
|         - | 2776 | ` *` |
|         - | 2777 | ` * Not cached when the initializer did not complete: a throw, an exit(), or a` |
|         - | 2778 | ` * MUTED evaluation (php has not reached this code, so nothing may be observable)` |
|         - | 2779 | ` * must all be retried rather than frozen into a half-built value.` |
|         - | 2780 | ` */` |
|    227841 | 2781 | `PH7_PRIVATE sxi32 VmExpandConstantOnce(ph7_vm *pVm,ph7_constant *pCons,ph7_value *pOut)` |
|         5 | 2782 | `{` |
|         - | 2783 | `	const void *pResumeBefore,*pInlineBefore;` |
|         - | 2784 | `	sxi32 rc;` |
|    227846 | 2785 | `	if( pCons->xExpand != PH7_VmExpandConstantValue ){` |
|    227678 | 2786 | `		pCons->xExpand(pOut,pCons->pUserData);` |
|    227678 | 2787 | `		return SXRET_OK;` |
|         - | 2788 | `	}` |
|         - | 2789 | `	/* The initializer's own status. PH7_VmExpandConstantValue drops VmLocalExec's` |
|         - | 2790 | `	 * return code (ProcConstant answers void), so the program is driven from here` |
|         - | 2791 | `	 * instead — a caller with no way to see a throw would otherwise cache a` |
|         - | 2792 | `	 * half-built value and keep running past it. */` |
|       173 | 2793 | `	pResumeBefore = (const void *)pVm->pResumeFrame;` |
|       173 | 2794 | `	pInlineBefore = (const void *)pVm->pInlineInstr;` |
|       173 | 2795 | `	rc = VmLocalExec(pVm,(SySet *)pCons->pUserData,pOut,FALSE);` |
|       168 | 2796 | `	if( pVm->nMuteThrow > 0 \|\| rc == PH7_ABORT` |
|       147 | 2797 | `	 \|\| VmLocalExecThrew(pVm,rc,pResumeBefore,pInlineBefore) ){` |
|         - | 2798 | `		/* Did not complete — a throw, an exit(), or a MUTED evaluation (php has` |
|         - | 2799 | `		 * not reached this code, so nothing may be observable). Retry it next` |
|         - | 2800 | `		 * time rather than freezing a value the initializer never produced:` |
|         - | 2801 | ``		 * `const A = LATER; …; define('LATER',5);` must still answer 5. */`` |
|        35 | 2802 | `		return rc == SXRET_OK ? PH7_EXCEPTION : rc;` |
|         - | 2803 | `	}` |
|         - | 2804 | `	{` |
|       141 | 2805 | `		ph7_value *pKeep = (ph7_value *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|       141 | 2806 | `		if( pKeep == 0 ){` |
|       ! 0 | 2807 | `			return SXRET_OK; /* out of memory: stay lazy rather than fail the read */` |
|         - | 2808 | `		}` |
|       141 | 2809 | `		PH7_MemObjInit(pVm,pKeep);` |
|       141 | 2810 | `		PH7_MemObjStore(pOut,pKeep);` |
|       141 | 2811 | `		pCons->xExpand = VmExpandUserConstant;` |
|       141 | 2812 | `		pCons->pUserData = pKeep;` |
|         - | 2813 | `	}` |
|       141 | 2814 | `	return SXRET_OK;` |
|    113927 | 2815 | `}` |
|    144629 | 2816 | `PH7_PRIVATE void VmExpandConstantWithNotice(ph7_vm *pVm,ph7_constant *pCons,ph7_value *pOut)` |
|         5 | 2817 | `{` |
|    144634 | 2818 | `	if( SySetUsed(&pCons->aAttrs) > 0 ){` |
|         7 | 2819 | `		VmDeprecatedAttrNoticeSubject(pVm,&pCons->aAttrs,"Constant",0,&pCons->sName);` |
|         3 | 2820 | `	}` |
|    144634 | 2821 | `	VmExpandConstantOnce(pVm,pCons,pOut);` |
|    144634 | 2822 | `}` |
|         - | 2823 | `/*` |
|         - | 2824 | ` * Query a GLOBAL constant by its exact (case-sensitive) name and expand its` |
|         - | 2825 | ` * value into pOut, which the caller has initialized. Returns 1 when the` |
|         - | 2826 | ` * constant exists. The ini scanner's NORMAL/TYPED value interpretation is the` |
|         - | 2827 | ` * caller: php substitutes a defined constant's value for a bare identifier` |
|         - | 2828 | ` * token inside an unquoted ini value.` |
|         - | 2829 | ` */` |
|        44 | 2830 | `PH7_PRIVATE int PH7_VmQueryConstant(ph7_vm *pVm,const char *zName,sxu32 nName,ph7_value *pOut)` |
|         1 | 2831 | `{` |
|         - | 2832 | `	SyHashEntry *pEntry;` |
|         - | 2833 | `	ph7_constant *pCons;` |
|        45 | 2834 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)zName,nName);` |
|        45 | 2835 | `	if( pEntry == 0 ){` |
|        41 | 2836 | `		return 0;` |
|         - | 2837 | `	}` |
|         5 | 2838 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|         5 | 2839 | `	VmExpandConstantWithNotice(pVm,pCons,pOut);` |
|         5 | 2840 | `	return 1;` |
|        23 | 2841 | `}` |
|         - | 2842 | `/*` |
|         - | 2843 | ` * Scan a declared-attribute set for #[\Deprecated]; when found, evaluate its` |
|         - | 2844 | ` * message:/since: arguments (positional #0 = message, #1 = since) into the` |
|         - | 2845 | ` * caller's values and return TRUE. The base subject text ("Function f()",` |
|         - | 2846 | ` * "Constant C::K") is the caller's business.` |
|         - | 2847 | ` */` |
|       404 | 2848 | `static int VmDeprecatedAttrExtract(ph7_vm *pVm,SySet *pAttrs,` |
|         - | 2849 | `	ph7_value *pMsg,int *pbMsg,ph7_value *pSince,int *pbSince)` |
|         5 | 2850 | `{` |
|       409 | 2851 | `	ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|         - | 2852 | `	sxu32 n;` |
|       409 | 2853 | `	*pbMsg = *pbSince = 0;` |
|       779 | 2854 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; ++n ){` |
|       413 | 2855 | `		ph7_attribute *pAttr = &aAttr[n];` |
|         - | 2856 | `		ph7_attr_arg *aArg;` |
|       413 | 2857 | `		sxu32 i,nPos = 0;` |
|       408 | 2858 | `		if( SyStringLength(&pAttr->sName) != sizeof("Deprecated")-1` |
|       228 | 2859 | `		 \|\| SyStrnicmp(SyStringData(&pAttr->sName),"Deprecated",sizeof("Deprecated")-1) != 0 ){` |
|       375 | 2860 | `			continue;` |
|         - | 2861 | `		}` |
|        40 | 2862 | `		aArg = (ph7_attr_arg *)SySetBasePtr(&pAttr->aArgs);` |
|        58 | 2863 | `		for( i = 0 ; i < SySetUsed(&pAttr->aArgs) ; ++i ){` |
|        19 | 2864 | `			ph7_attr_arg *pArg = &aArg[i];` |
|        19 | 2865 | `			int isMsg = 0,isSince = 0;` |
|        19 | 2866 | `			if( SyStringLength(&pArg->sName) == 0 ){` |
|         3 | 2867 | `				isMsg = (nPos == 0);` |
|         3 | 2868 | `				isSince = (nPos == 1);` |
|         3 | 2869 | `				nPos++;` |
|        18 | 2870 | `			}else if( SyStringLength(&pArg->sName) == sizeof("message")-1` |
|        12 | 2871 | `			 && SyMemcmp(SyStringData(&pArg->sName),"message",sizeof("message")-1) == 0 ){` |
|         7 | 2872 | `				isMsg = 1;` |
|        14 | 2873 | `			}else if( SyStringLength(&pArg->sName) == sizeof("since")-1` |
|        11 | 2874 | `			 && SyMemcmp(SyStringData(&pArg->sName),"since",sizeof("since")-1) == 0 ){` |
|        11 | 2875 | `				isSince = 1;` |
|         5 | 2876 | `			}` |
|        19 | 2877 | `			if( isMsg && !*pbMsg && SySetUsed(&pArg->aByteCode) > 0 ){` |
|        13 | 2878 | `				if( VmLocalExec(pVm,&pArg->aByteCode,pMsg,FALSE) == SXRET_OK ){` |
|         9 | 2879 | `					if( (pMsg->iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2880 | `						PH7_MemObjToString(pMsg);` |
|       ! 0 | 2881 | `					}` |
|         9 | 2882 | `					*pbMsg = 1;` |
|         5 | 2883 | `				}` |
|        15 | 2884 | `			}else if( isSince && !*pbSince && SySetUsed(&pArg->aByteCode) > 0 ){` |
|        11 | 2885 | `				if( VmLocalExec(pVm,&pArg->aByteCode,pSince,FALSE) == SXRET_OK ){` |
|        11 | 2886 | `					if( (pSince->iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2887 | `						PH7_MemObjToString(pSince);` |
|       ! 0 | 2888 | `					}` |
|        11 | 2889 | `					*pbSince = 1;` |
|         5 | 2890 | `				}` |
|         5 | 2891 | `			}` |
|        10 | 2892 | `		}` |
|        40 | 2893 | `		return 1;` |
|       ! 0 | 2894 | `	}` |
|       371 | 2895 | `	return 0;` |
|       207 | 2896 | `}` |
|         - | 2897 | `/*` |
|         - | 2898 | ` * Append php's " since X" / ", message" suffixes to a built base subject and` |
|         - | 2899 | ` * emit the E_USER_DEPRECATED notice.` |
|         - | 2900 | ` */` |
|        38 | 2901 | `static void VmDeprecatedEmit(ph7_vm *pVm,SyBlob *pOut,` |
|         - | 2902 | `	ph7_value *pMsg,int bMsg,ph7_value *pSince,int bSince)` |
|         2 | 2903 | `{` |
|        40 | 2904 | `	if( bSince && SyBlobLength(&pSince->sBlob) > 0 ){` |
|        16 | 2905 | `		SyBlobFormat(pOut," since %.*s",(int)SyBlobLength(&pSince->sBlob),` |
|        10 | 2906 | `			(const char *)SyBlobData(&pSince->sBlob));` |
|         5 | 2907 | `	}` |
|        40 | 2908 | `	if( bMsg && SyBlobLength(&pMsg->sBlob) > 0 ){` |
|        13 | 2909 | `		SyBlobFormat(pOut,", %.*s",(int)SyBlobLength(&pMsg->sBlob),` |
|         8 | 2910 | `			(const char *)SyBlobData(&pMsg->sBlob));` |
|         4 | 2911 | `	}` |
|        40 | 2912 | `	VmThrowUserDeprecatedFmt(pVm,"%.*s",(int)SyBlobLength(pOut),(const char *)SyBlobData(pOut));` |
|        40 | 2913 | `}` |
|         - | 2914 | `/*` |
|         - | 2915 | ` * Generic #[\Deprecated] notice for a named subject:` |
|         - | 2916 | ` * "<Kind> [Qual::]Name is deprecated[ since X][, message]".` |
|         - | 2917 | ` */` |
|        20 | 2918 | `static void VmDeprecatedAttrNoticeSubject(ph7_vm *pVm,SySet *pAttrs,` |
|         - | 2919 | `	const char *zKind,const SyString *pQual,const SyString *pName)` |
|         2 | 2920 | `{` |
|         - | 2921 | `	ph7_value sMsg,sSince;` |
|         - | 2922 | `	SyBlob sOut;` |
|         - | 2923 | `	int bMsg,bSince;` |
|        22 | 2924 | `	PH7_MemObjInit(pVm,&sMsg);` |
|        22 | 2925 | `	PH7_MemObjInit(pVm,&sSince);` |
|        22 | 2926 | `	if( VmDeprecatedAttrExtract(pVm,pAttrs,&sMsg,&bMsg,&sSince,&bSince) ){` |
|        18 | 2927 | `		SyBlobInit(&sOut,&pVm->sAllocator);` |
|        18 | 2928 | `		if( pQual ){` |
|        14 | 2929 | `			SyBlobFormat(&sOut,"%s %z::%z is deprecated",zKind,pQual,pName);` |
|         8 | 2930 | `		}else{` |
|         5 | 2931 | `			SyBlobFormat(&sOut,"%s %z is deprecated",zKind,pName);` |
|         - | 2932 | `		}` |
|        18 | 2933 | `		VmDeprecatedEmit(pVm,&sOut,&sMsg,bMsg,&sSince,bSince);` |
|        18 | 2934 | `		SyBlobRelease(&sOut);` |
|         8 | 2935 | `	}` |
|        22 | 2936 | `	PH7_MemObjRelease(&sMsg);` |
|        22 | 2937 | `	PH7_MemObjRelease(&sSince);` |
|        22 | 2938 | `}` |
|       384 | 2939 | `PH7_PRIVATE void VmDeprecatedAttrNotice(ph7_vm *pVm,ph7_vm_func *pFunc,ph7_class *pDeclClass)` |
|         5 | 2940 | `{` |
|         - | 2941 | `	ph7_value sMsg,sSince;` |
|         - | 2942 | `	SyBlob sOut;` |
|         - | 2943 | `	int bMsg,bSince;` |
|       389 | 2944 | `	PH7_MemObjInit(pVm,&sMsg);` |
|       389 | 2945 | `	PH7_MemObjInit(pVm,&sSince);` |
|       389 | 2946 | `	if( VmDeprecatedAttrExtract(pVm,&pFunc->aAttrs,&sMsg,&bMsg,&sSince,&bSince) ){` |
|        24 | 2947 | `		SyBlobInit(&sOut,&pVm->sAllocator);` |
|        24 | 2948 | `		if( pDeclClass ){` |
|         5 | 2949 | `			SyBlobFormat(&sOut,"Method %z::%z() is deprecated",&pDeclClass->sName,&pFunc->sName);` |
|         3 | 2950 | `		}else{` |
|        20 | 2951 | `			SyBlobFormat(&sOut,"Function %z() is deprecated",&pFunc->sName);` |
|         - | 2952 | `		}` |
|        24 | 2953 | `		VmDeprecatedEmit(pVm,&sOut,&sMsg,bMsg,&sSince,bSince);` |
|        24 | 2954 | `		SyBlobRelease(&sOut);` |
|        11 | 2955 | `	}` |
|       389 | 2956 | `	PH7_MemObjRelease(&sMsg);` |
|       389 | 2957 | `	PH7_MemObjRelease(&sSince);` |
|       389 | 2958 | `}` |
|         - | 2959 | `/*` |
|         - | 2960 | ` * php 8.5's #[\NoDiscard] warning, raised at the CALL, before the body runs, and` |
|         - | 2961 | ` * once per call (a loop warns every time round).` |
|         - | 2962 | ` *` |
|         - | 2963 | ` * The subject is a "function" unless the callee has a class scope, in which case` |
|         - | 2964 | ` * php names the DECLARING class -- an inherited method reports the class that` |
|         - | 2965 | `` * wrote it, and so does `parent::m()`. The tail after php's sentence is the`` |
|         - | 2966 | ` * attribute's own message: a constant EXPRESSION for a compiled declaration` |
|         - | 2967 | ` * (evaluated here, where the constants it may name exist) and a fixed string for` |
|         - | 2968 | ` * an internal member, which is how php words the immutable date mutators.` |
|         - | 2969 | ` */` |
|        56 | 2970 | `PH7_PRIVATE void VmNoDiscardWarn(ph7_vm *pVm,ph7_vm_func *pFunc,ph7_class *pDeclClass)` |
|         1 | 2971 | `{` |
|        57 | 2972 | `	ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(&pFunc->aAttrs);` |
|        57 | 2973 | `	const SyString *pName = &pFunc->sName;` |
|         - | 2974 | `	ph7_value sMsg;` |
|         - | 2975 | `	SyBlob sOut;` |
|        57 | 2976 | `	int bMsg = 0;` |
|         - | 2977 | `	sxu32 n;` |
|         - | 2978 | ``	/* A closure reports php's `{closure:SCOPE:LINE}` spelling, like every other`` |
|         - | 2979 | `	 * diagnostic that names one. */` |
|        57 | 2980 | `	if( SyStringLength(&pFunc->sClosureName) > 0 ){` |
|         7 | 2981 | `		pName = &pFunc->sClosureName;` |
|         3 | 2982 | `	}` |
|        57 | 2983 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|        57 | 2984 | `	if( pDeclClass ){` |
|        23 | 2985 | `		SyBlobFormat(&sOut,"The return value of method %z::%z() should either be used "` |
|        11 | 2986 | `			"or intentionally ignored by casting it as (void)",&pDeclClass->sName,pName);` |
|        12 | 2987 | `	}else{` |
|        35 | 2988 | `		SyBlobFormat(&sOut,"The return value of function %z() should either be used "` |
|        17 | 2989 | `			"or intentionally ignored by casting it as (void)",pName);` |
|         - | 2990 | `	}` |
|        57 | 2991 | `	PH7_MemObjInit(pVm,&sMsg);` |
|        57 | 2992 | `for( n = 0 ; !bMsg && n < SySetUsed(&pFunc->aAttrs) ; ++n ){` |
|         - | 2993 | `		ph7_attr_arg *aArg;` |
|        57 | 2994 | `		sxu32 i,nPos = 0;` |
|        56 | 2995 | `		if( SyStringLength(&aAttr[n].sName) != sizeof("NoDiscard")-1` |
|        57 | 2996 | `		 \|\| SyStrnicmp(SyStringData(&aAttr[n].sName),"NoDiscard",` |
|        28 | 2997 | `				sizeof("NoDiscard")-1) != 0 ){` |
|       ! 0 | 2998 | `			continue;` |
|         - | 2999 | `		}` |
|        57 | 3000 | `		aArg = (ph7_attr_arg *)SySetBasePtr(&aAttr[n].aArgs);` |
|        57 | 3001 | `		for( i = 0 ; i < SySetUsed(&aAttr[n].aArgs) ; ++i ){` |
|        11 | 3002 | `			ph7_attr_arg *pArg = &aArg[i];` |
|         - | 3003 | `			int isMsg;` |
|        11 | 3004 | `			if( SyStringLength(&pArg->sName) == 0 ){` |
|         7 | 3005 | `				isMsg = (nPos == 0);` |
|         7 | 3006 | `				nPos++;` |
|         4 | 3007 | `			}else{` |
|         7 | 3008 | `				isMsg = SyStringLength(&pArg->sName) == sizeof("message")-1` |
|         4 | 3009 | `					&& SyMemcmp(SyStringData(&pArg->sName),"message",` |
|         2 | 3010 | `						sizeof("message")-1) == 0;` |
|         - | 3011 | `			}` |
|        11 | 3012 | `			if( !isMsg ){` |
|       ! 0 | 3013 | `				continue;` |
|         - | 3014 | `			}` |
|         - | 3015 | `			/* A compiled declaration holds the message as a constant` |
|         - | 3016 | `			 * EXPRESSION (evaluated here, where the constants it may name` |
|         - | 3017 | `			 * exist); a native one holds it as a literal, like every other` |
|         - | 3018 | `			 * attribute argument a C-declared class carries. */` |
|        11 | 3019 | `			if( SySetUsed(&pArg->aByteCode) > 0 ){` |
|         9 | 3020 | `				if( VmLocalExec(pVm,&pArg->aByteCode,&sMsg,FALSE) != SXRET_OK ){` |
|       ! 0 | 3021 | `					continue;` |
|         1 | 3022 | `				}` |
|         7 | 3023 | `			}else if( pArg->pNativeValue ){` |
|         3 | 3024 | `				PH7_NativeLiteralValue(pVm,pArg->pNativeValue,&sMsg);` |
|         2 | 3025 | `			}else{` |
|       ! 0 | 3026 | `				continue;` |
|         - | 3027 | `			}` |
|        11 | 3028 | `			if( (sMsg.iFlags & MEMOBJ_STRING) == 0 ){` |
|         3 | 3029 | `				PH7_MemObjToString(&sMsg);` |
|         1 | 3030 | `			}` |
|        11 | 3031 | `			bMsg = 1;` |
|        11 | 3032 | `			break;` |
|       ! 0 | 3033 | `		}` |
|        57 | 3034 | `		break;` |
|       ! 0 | 3035 | `	}` |
|        57 | 3036 | `	if( bMsg && SyBlobLength(&sMsg.sBlob) > 0 ){` |
|        13 | 3037 | `		SyBlobFormat(&sOut,", %.*s",(int)SyBlobLength(&sMsg.sBlob),` |
|         8 | 3038 | `			(const char *)SyBlobData(&sMsg.sBlob));` |
|         4 | 3039 | `	}` |
|        57 | 3040 | `	PH7_MemObjRelease(&sMsg);` |
|         - | 3041 | `	/* php raises it as E_USER_WARNING (512), not the engine's E_WARNING: the` |
|         - | 3042 | `	 * attribute is userland-authored, the same reason #[\Deprecated] is` |
|         - | 3043 | `	 * E_USER_DEPRECATED. A set_error_handler sees the number. */` |
|        85 | 3044 | `	VmThrowUserWarningFmt(pVm,"%.*s",` |
|        56 | 3045 | `		(int)SyBlobLength(&sOut),(const char *)SyBlobData(&sOut));` |
|        57 | 3046 | `	SyBlobRelease(&sOut);` |
|        57 | 3047 | `}` |
|         - | 3048 | `/*` |
|         - | 3049 | ` * Same notice for a #[\Deprecated] class constant, at its static-access site:` |
|         - | 3050 | `` * php's `Constant C::K is deprecated` (each access re-warns).`` |
|         - | 3051 | ` */` |
|        14 | 3052 | `PH7_PRIVATE void VmDeprecatedConstNotice(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pMember)` |
|         2 | 3053 | `{` |
|        23 | 3054 | `	VmDeprecatedAttrNoticeSubject(pVm,&pMember->aAttrs,` |
|        14 | 3055 | `		(pMember->iFlags & PH7_CLASS_ATTR_ENUMCASE) ? "Enum case" : "Constant",` |
|        14 | 3056 | `		&pClass->sName,&pMember->sName);` |
|        16 | 3057 | `}` |
|         - | 3058 | `/*` |
|         - | 3059 | `` * The USER-VISIBLE string coercion a builtin performs on a `mixed` argument or`` |
|         - | 3060 | ` * array element: the builtin-side twin of PH7_MemObjToStringUV's opcode sites.` |
|         - | 3061 | ` * An ARRAY warns "Array to string conversion" and still renders as "Array"; an` |
|         - | 3062 | ` * object whose class has no __toString() -- or one whose __toString() threw --` |
|         - | 3063 | ` * is php's catchable "could not be converted to string" Error, and the builtin` |
|         - | 3064 | ` * must answer that instead of a value.` |
|         - | 3065 | ` *` |
|         - | 3066 | ` * On success pzData and pnLen receive the NUL-terminated bytes (both optional).` |
|         - | 3067 | ` * On a throw they are set to the empty string and the status is returned AND` |
|         - | 3068 | ` * recorded on the call context, so OP_CALL cannot mistake the call for a normal` |
|         - | 3069 | ` * return; a builtin that has already produced output (printf) still keeps it,` |
|         - | 3070 | ` * which is what php does.` |
|         - | 3071 | ` *` |
|         - | 3072 | ` * The public ph7_value_to_string() stays SILENT on purpose: it is the embedder` |
|         - | 3073 | ` * API, and the INTERNAL coercions behind it (array-key canonicalisation, sort` |
|         - | 3074 | ` * comparisons, print_r/var_export/serialize) must not throw -- php's do not` |
|         - | 3075 | ` * either.` |
|         - | 3076 | ` */` |
|    381442 | 3077 | `PH7_PRIVATE sxi32 PH7_ValueToStringUV(ph7_context *pCtx,ph7_value *pValue,const char **pzData,int *pnLen)` |
|         5 | 3078 | `{` |
|    381447 | 3079 | `	sxi32 rc = PH7_MemObjToStringUV(pValue);` |
|    381447 | 3080 | `	if( rc != SXRET_OK ){` |
|        49 | 3081 | `		if( pCtx ){` |
|        49 | 3082 | `			pCtx->nThrowRc = rc;` |
|        23 | 3083 | `		}` |
|        49 | 3084 | `		if( pzData ){` |
|        45 | 3085 | `			*pzData = "";` |
|        21 | 3086 | `		}` |
|        49 | 3087 | `		if( pnLen ){` |
|        45 | 3088 | `			*pnLen = 0;` |
|        21 | 3089 | `		}` |
|        49 | 3090 | `		return rc;` |
|         - | 3091 | `	}` |
|    381401 | 3092 | `	if( pzData \|\| pnLen ){` |
|    381373 | 3093 | `		const char *zData = ph7_value_to_string(pValue,pnLen);` |
|    381373 | 3094 | `		if( pzData ){` |
|    381373 | 3095 | `			*pzData = zData;` |
|    190681 | 3096 | `		}` |
|    190681 | 3097 | `	}` |
|    381401 | 3098 | `	return SXRET_OK;` |
|    190723 | 3099 | `}` |
|         - | 3100 |  |
