# src/ph7/vm_arg_check.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1042/1103 lines (94.47%)

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
|         - |   52 | `	/* ext/openssl. One row, and it is php's own inconsistency rather than a` |
|         - |   53 | `	 * shape this table usually carries: openssl_cms_verify() DECLARES` |
|         - |   54 | ``	 * `int $flags = 0` -- ReflectionFunction reports one required parameter --`` |
|         - |   55 | `	 * and then refuses a one-argument call with "expects at least 2` |
|         - |   56 | `	 * arguments". The declaration is what Reflection answers and this row is` |
|         - |   57 | `	 * what a caller hits. */` |
|         - |   58 | `	{ "openssl_cms_verify",        2, 1 },` |
|         - |   59 | `	/* Math family (atan2/intdiv already self-throw the same ArgumentCountError,` |
|         - |   60 | `	 * so they stay off the table per the disjointness rule above). */` |
|         - |   61 | `	{ "pow",          2, 0 }, { "fmod",          2, 0 }, { "hypot",          2, 0 },` |
|         - |   62 | `	{ "log",          1, 1 },` |
|         - |   63 | `	/* Array family (str_split already self-throws — kept off the table). */` |
|         - |   64 | `	{ "in_array",     2, 1 }, { "range",         2, 1 },` |
|         - |   65 | `	{ "implode",      1, 1 }, { "join",          1, 1 },` |
|         - |   66 | `	/*` |
|         - |   67 | `	 * Batch 2 (band A #5 continuation) — a systematic sweep of every remaining` |
|         - |   68 | `	 * builtin that silently degraded on too-few arguments where php 8 throws` |
|         - |   69 | `	 * ArgumentCountError. Each row's minimum and "exactly"/"at least" wording was` |
|         - |   70 | `	 * extracted from php 8.5.7's own ArgumentCountError message at the argument` |
|         - |   71 | `	 * boundary (the message text is byte-identical to the one this table drives).` |
|         - |   72 | `	 * Functions that already self-throw the correct message are still excluded per` |
|         - |   73 | `	 * the disjointness rule above.` |
|         - |   74 | `	 */` |
|         - |   75 | `	/* String family */` |
|         - |   76 | `	{ "chop",                      1, 1 },` |
|         - |   77 | `	{ "explode",                   2, 1 },` |
|         - |   78 | `	{ "fprintf",                   2, 1 },` |
|         - |   79 | `	{ "html_entity_decode",        1, 1 },` |
|         - |   80 | `	{ "htmlentities",              1, 1 },` |
|         - |   81 | `	{ "htmlspecialchars",          1, 1 },` |
|         - |   82 | `	{ "htmlspecialchars_decode",   1, 1 },` |
|         - |   83 | `	{ "lcfirst",                   1, 0 },` |
|         - |   84 | `	{ "ltrim",                     1, 1 },` |
|         - |   85 | `	{ "mb_check_encoding",         1, 0 },` |
|         - |   86 | `	{ "mb_chr",                    1, 1 },` |
|         - |   87 | `	{ "mb_convert_case",           2, 1 },` |
|         - |   88 | `	{ "mb_convert_encoding",       2, 1 },` |
|         - |   89 | `	{ "mb_detect_encoding",        1, 1 },` |
|         - |   90 | `	{ "mb_ord",                    1, 1 },` |
|         - |   91 | `	{ "mb_str_split",              1, 1 },` |
|         - |   92 | `	{ "mb_stripos",                2, 1 },` |
|         - |   93 | `	{ "mb_strlen",                 1, 1 },` |
|         - |   94 | `	{ "mb_strpos",                 2, 1 },` |
|         - |   95 | `	{ "mb_strrpos",                2, 1 },` |
|         - |   96 | `	{ "mb_strtolower",             1, 1 },` |
|         - |   97 | `	{ "mb_strtoupper",             1, 1 },` |
|         - |   98 | `	{ "mb_strwidth",               1, 1 },` |
|         - |   99 | `	{ "mb_substr",                 2, 1 },` |
|         - |  100 | `	{ "nl2br",                     1, 1 },` |
|         - |  101 | `	{ "printf",                    1, 1 },` |
|         - |  102 | `	{ "quotemeta",                 1, 0 },` |
|         - |  103 | `	{ "rtrim",                     1, 1 },` |
|         - |  104 | `	{ "soundex",                   1, 0 },` |
|         - |  105 | `	{ "sprintf",                   1, 1 },` |
|         - |  106 | `	{ "str_getcsv",                1, 1 },` |
|         - |  107 | `	{ "str_shuffle",               1, 0 },` |
|         - |  108 | `	{ "strcasecmp",                2, 0 },` |
|         - |  109 | `	{ "strchr",                    2, 1 },` |
|         - |  110 | `	{ "strcmp",                    2, 0 },` |
|         - |  111 | `	{ "strnatcasecmp",             2, 0 },` |
|         - |  112 | `	{ "strnatcmp",                 2, 0 },` |
|         - |  113 | `	{ "strcoll",                   2, 0 },` |
|         - |  114 | `	{ "strip_tags",                1, 1 },` |
|         - |  115 | `	{ "stripslashes",              1, 0 },` |
|         - |  116 | `	{ "strlen",                    1, 0 },` |
|         - |  117 | `	{ "strrev",                    1, 0 },` |
|         - |  118 | `	{ "strtok",                    1, 1 },` |
|         - |  119 | `	{ "strtolower",                1, 0 },` |
|         - |  120 | `	{ "strtoupper",                1, 0 },` |
|         - |  121 | `	{ "strtr",                     2, 0 },` |
|         - |  122 | `	{ "trim",                      1, 1 },` |
|         - |  123 | `	{ "ucfirst",                   1, 0 },` |
|         - |  124 | `	{ "ucwords",                   1, 1 },` |
|         - |  125 | `	{ "vfprintf",                  3, 0 },` |
|         - |  126 | `	{ "vprintf",                   2, 0 },` |
|         - |  127 | `	{ "vsprintf",                  2, 0 },` |
|         - |  128 | `	{ "wordwrap",                  1, 1 },` |
|         - |  129 | `	/* Ctype family */` |
|         - |  130 | `	{ "ctype_alnum",               1, 0 },` |
|         - |  131 | `	{ "ctype_alpha",               1, 0 },` |
|         - |  132 | `	{ "ctype_cntrl",               1, 0 },` |
|         - |  133 | `	{ "ctype_digit",               1, 0 },` |
|         - |  134 | `	{ "ctype_graph",               1, 0 },` |
|         - |  135 | `	{ "ctype_lower",               1, 0 },` |
|         - |  136 | `	{ "ctype_print",               1, 0 },` |
|         - |  137 | `	{ "ctype_punct",               1, 0 },` |
|         - |  138 | `	{ "ctype_space",               1, 0 },` |
|         - |  139 | `	{ "ctype_upper",               1, 0 },` |
|         - |  140 | `	{ "ctype_xdigit",              1, 0 },` |
|         - |  141 | `	/* Math family */` |
|         - |  142 | `	{ "base_convert",              3, 0 },` |
|         - |  143 | `	{ "cos",                       1, 0 },` |
|         - |  144 | `	{ "cosh",                      1, 0 },` |
|         - |  145 | `	{ "crc32",                     1, 0 },` |
|         - |  146 | `	{ "decbin",                    1, 0 },` |
|         - |  147 | `	{ "dechex",                    1, 0 },` |
|         - |  148 | `	{ "decoct",                    1, 0 },` |
|         - |  149 | `	{ "exp",                       1, 0 },` |
|         - |  150 | `	{ "log10",                     1, 0 },` |
|         - |  151 | `	{ "md5",                       1, 1 },` |
|         - |  152 | `	{ "iconv",                     3, 0 },` |
|         - |  153 | `	{ "iconv_strlen",              1, 1 },` |
|         - |  154 | `	{ "iconv_substr",              2, 1 },` |
|         - |  155 | `	{ "iconv_strpos",              2, 1 },` |
|         - |  156 | `	{ "iconv_strrpos",             2, 1 },` |
|         - |  157 | `	{ "iconv_mime_encode",         2, 1 },` |
|         - |  158 | `	{ "iconv_mime_decode",         1, 1 },` |
|         - |  159 | `	{ "iconv_mime_decode_headers", 1, 1 },` |
|         - |  160 | `	{ "round",                     1, 1 },` |
|         - |  161 | `	{ "sha1",                      1, 1 },` |
|         - |  162 | `	{ "sin",                       1, 0 },` |
|         - |  163 | `	{ "sinh",                      1, 0 },` |
|         - |  164 | `	{ "sqrt",                      1, 0 },` |
|         - |  165 | `	{ "tan",                       1, 0 },` |
|         - |  166 | `	{ "tanh",                      1, 0 },` |
|         - |  167 | `	/* Type/var family */` |
|         - |  168 | `	{ "floatval",                  1, 0 },` |
|         - |  169 | `	{ "get_resource_id",           1, 0 },` |
|         - |  170 | `	{ "get_resource_type",         1, 0 },` |
|         - |  171 | `	{ "gettype",                   1, 0 },` |
|         - |  172 | `	{ "intval",                    1, 1 },` |
|         - |  173 | `	{ "is_array",                  1, 0 },` |
|         - |  174 | `	{ "is_bool",                   1, 0 },` |
|         - |  175 | `	{ "is_callable",               1, 1 },` |
|         - |  176 | `	{ "is_double",                 1, 0 },` |
|         - |  177 | `	{ "is_float",                  1, 0 },` |
|         - |  178 | `	{ "is_int",                    1, 0 },` |
|         - |  179 | `	{ "is_integer",                1, 0 },` |
|         - |  180 | `	{ "is_long",                   1, 0 },` |
|         - |  181 | `	{ "is_null",                   1, 0 },` |
|         - |  182 | `	{ "is_numeric",                1, 0 },` |
|         - |  183 | `	{ "is_object",                 1, 0 },` |
|         - |  184 | `	{ "is_resource",               1, 0 },` |
|         - |  185 | `	{ "is_scalar",                 1, 0 },` |
|         - |  186 | `	{ "is_string",                 1, 0 },` |
|         - |  187 | `	{ "print_r",                   1, 1 },` |
|         - |  188 | `	{ "strval",                    1, 0 },` |
|         - |  189 | `	{ "var_dump",                  1, 1 },` |
|         - |  190 | `	{ "var_export",                1, 1 },` |
|         - |  191 | `	/* Array/iterator family */` |
|         - |  192 | `	{ "array_filter",              1, 1 },` |
|         - |  193 | `	{ "array_product",             1, 0 },` |
|         - |  194 | `	{ "array_rand",                1, 1 },` |
|         - |  195 | `	{ "compact",                   1, 1 },` |
|         - |  196 | `	{ "current",                   1, 0 },` |
|         - |  197 | `	{ "end",                       1, 0 },` |
|         - |  198 | `	{ "extract",                   1, 1 },` |
|         - |  199 | `	{ "iterator_apply",            2, 1 },` |
|         - |  200 | `	{ "iterator_count",            1, 0 },` |
|         - |  201 | `	{ "iterator_to_array",         1, 1 },` |
|         - |  202 | `	{ "key",                       1, 0 },` |
|         - |  203 | `	{ "krsort",                    1, 1 },` |
|         - |  204 | `	{ "ksort",                     1, 1 },` |
|         - |  205 | `	{ "next",                      1, 0 },` |
|         - |  206 | `	{ "pos",                       1, 0 },` |
|         - |  207 | `	{ "prev",                      1, 0 },` |
|         - |  208 | `	{ "reset",                     1, 0 },` |
|         - |  209 | `	{ "rsort",                     1, 1 },` |
|         - |  210 | `	{ "shuffle",                   1, 0 },` |
|         - |  211 | `	{ "sort",                      1, 1 },` |
|         - |  212 | `	{ "uasort",                    2, 0 },` |
|         - |  213 | `	{ "uksort",                    2, 0 },` |
|         - |  214 | `	{ "usort",                     2, 0 },` |
|         - |  215 | `	/* Class/reflection family */` |
|         - |  216 | `	{ "class_alias",               2, 1 },` |
|         - |  217 | `	{ "class_exists",              1, 1 },` |
|         - |  218 | `	{ "enum_exists",               1, 1 },` |
|         - |  219 | `	{ "get_class_methods",         1, 0 },` |
|         - |  220 | `	{ "get_class_vars",            1, 0 },` |
|         - |  221 | `	{ "get_object_vars",           1, 0 },` |
|         - |  222 | `	{ "interface_exists",          1, 1 },` |
|         - |  223 | `	{ "trait_exists",              1, 1 },` |
|         - |  224 | `	{ "is_a",                      2, 1 },` |
|         - |  225 | `	{ "is_subclass_of",            2, 1 },` |
|         - |  226 | `	{ "method_exists",             2, 0 },` |
|         - |  227 | `	{ "property_exists",           2, 0 },` |
|         - |  228 | `	{ "spl_autoload",              1, 1 },` |
|         - |  229 | `	{ "spl_autoload_unregister",   1, 0 },` |
|         - |  230 | `	{ "spl_object_hash",           1, 0 },` |
|         - |  231 | `	{ "spl_object_id",             1, 0 },` |
|         - |  232 | `	/* Filesystem/IO family */` |
|         - |  233 | `	{ "basename",                  1, 1 },` |
|         - |  234 | `	{ "chdir",                     1, 0 },` |
|         - |  235 | `	{ "chgrp",                     2, 0 },` |
|         - |  236 | `	{ "dir",                       1, 1 },` |
|         - |  237 | `	{ "dirname",                   1, 1 },` |
|         - |  238 | `	{ "disk_free_space",           1, 0 },` |
|         - |  239 | `	{ "disk_total_space",          1, 0 },` |
|         - |  240 | `	{ "diskfreespace",             1, 0 },` |
|         - |  241 | `	{ "fclose",                    1, 0 },` |
|         - |  242 | `	{ "feof",                      1, 0 },` |
|         - |  243 | `	{ "fflush",                    1, 0 },` |
|         - |  244 | `	{ "fgetc",                     1, 0 },` |
|         - |  245 | `	{ "fgetcsv",                   1, 1 },` |
|         - |  246 | `	{ "file",                      1, 1 },` |
|         - |  247 | `	{ "file_exists",               1, 0 },` |
|         - |  248 | `	{ "fileatime",                 1, 0 },` |
|         - |  249 | `	{ "filectime",                 1, 0 },` |
|         - |  250 | `	{ "filemtime",                 1, 0 },` |
|         - |  251 | `	{ "filesize",                  1, 0 },` |
|         - |  252 | `	{ "filetype",                  1, 0 },` |
|         - |  253 | `	{ "flock",                     2, 1 },` |
|         - |  254 | `	{ "fpassthru",                 1, 0 },` |
|         - |  255 | `	{ "fputcsv",                   2, 1 },` |
|         - |  256 | `	{ "fputs",                     2, 1 },` |
|         - |  257 | `	{ "fseek",                     2, 1 },` |
|         - |  258 | `	{ "fstat",                     1, 0 },` |
|         - |  259 | `	/* ext/zlib's aliases of the six above; the alias needs its own row or the` |
|         - |  260 | `	 * ArgumentCountError names the function it is an alias OF. */` |
|         - |  261 | `	{ "gzclose",                   1, 0 },` |
|         - |  262 | `	{ "gzeof",                     1, 0 },` |
|         - |  263 | `	{ "gzgetc",                    1, 0 },` |
|         - |  264 | `	{ "gzpassthru",                1, 0 },` |
|         - |  265 | `	{ "gzputs",                    2, 1 },` |
|         - |  266 | `	{ "gzrewind",                  1, 0 },` |
|         - |  267 | `	{ "gzseek",                    2, 1 },` |
|         - |  268 | `	{ "gztell",                    1, 0 },` |
|         - |  269 | `	{ "gzwrite",                   2, 1 },` |
|         - |  270 | `	{ "ftell",                     1, 0 },` |
|         - |  271 | `	{ "ftruncate",                 2, 0 },` |
|         - |  272 | `	{ "getopt",                    1, 1 },` |
|         - |  273 | `	{ "is_dir",                    1, 0 },` |
|         - |  274 | `	{ "is_executable",             1, 0 },` |
|         - |  275 | `	{ "is_file",                   1, 0 },` |
|         - |  276 | `	{ "is_link",                   1, 0 },` |
|         - |  277 | `	{ "is_readable",               1, 0 },` |
|         - |  278 | `	{ "is_writable",               1, 0 },` |
|         - |  279 | `	{ "lstat",                     1, 0 },` |
|         - |  280 | `	{ "md5_file",                  1, 1 },` |
|         - |  281 | `	{ "opendir",                   1, 1 },` |
|         - |  282 | `	{ "pathinfo",                  1, 1 },` |
|         - |  283 | `	{ "pclose",                    1, 0 },` |
|         - |  284 | `	{ "readlink",                  1, 0 },` |
|         - |  285 | `	{ "realpath",                  1, 0 },` |
|         - |  286 | `	{ "stream_resolve_include_path",1, 0 },` |
|         - |  287 | `	{ "rewind",                    1, 0 },` |
|         - |  288 | `	{ "sha1_file",                 1, 1 },` |
|         - |  289 | `	{ "stat",                      1, 0 },` |
|         - |  290 | `	/* Date family */` |
|         - |  291 | `	{ "date",                      1, 1 },` |
|         - |  292 | `	{ "date_default_timezone_set", 1, 1 },` |
|         - |  293 | `	{ "gmdate",                    1, 1 },` |
|         - |  294 | `	{ "gmmktime",                  1, 1 },` |
|         - |  295 | `	{ "idate",                     1, 1 },` |
|         - |  296 | `	{ "mktime",                    1, 1 },` |
|         - |  297 | `	/* Encoding/URL family */` |
|         - |  298 | `	{ "base64_decode",             1, 1 },` |
|         - |  299 | `	{ "base64_encode",             1, 0 },` |
|         - |  300 | `	{ "convert_uudecode",          1, 0 },` |
|         - |  301 | `	{ "convert_uuencode",          1, 0 },` |
|         - |  302 | `	{ "parse_ini_file",            1, 1 },` |
|         - |  303 | `	{ "parse_ini_string",          1, 1 },` |
|         - |  304 | `	{ "parse_url",                 1, 1 },` |
|         - |  305 | `	{ "rawurldecode",              1, 0 },` |
|         - |  306 | `	{ "rawurlencode",              1, 0 },` |
|         - |  307 | `	{ "urldecode",                 1, 0 },` |
|         - |  308 | `	{ "urlencode",                 1, 0 },` |
|         - |  309 | `	/* JSON/serialize family */` |
|         - |  310 | `	{ "filter_var",                1, 1 },` |
|         - |  311 | `	{ "json_decode",               1, 1 },` |
|         - |  312 | `	{ "json_encode",               1, 1 },` |
|         - |  313 | `	{ "json_validate",             1, 1 },` |
|         - |  314 | `	{ "serialize",                 1, 0 },` |
|         - |  315 | `	{ "unserialize",               1, 1 },` |
|         - |  316 | `	/* PCRE family */` |
|         - |  317 | `	{ "preg_match",                2, 1 },` |
|         - |  318 | `	{ "preg_match_all",            2, 1 },` |
|         - |  319 | `	{ "preg_quote",                1, 1 },` |
|         - |  320 | `	{ "preg_replace",              3, 1 },` |
|         - |  321 | `	{ "preg_replace_callback",     3, 1 },` |
|         - |  322 | `	{ "preg_split",                2, 1 },` |
|         - |  323 | `	/* XML family */` |
|         - |  324 | `	/* Constants/misc family */` |
|         - |  325 | `	{ "call_user_func",            1, 1 },` |
|         - |  326 | `	{ "call_user_func_array",      2, 0 },` |
|         - |  327 | `	{ "constant",                  1, 0 },` |
|         - |  328 | `	{ "define",                    2, 1 },` |
|         - |  329 | `	{ "defined",                   1, 0 },` |
|         - |  330 | `	{ "error_log",                 1, 1 },` |
|         - |  331 | `	{ "fnmatch",                   2, 1 },` |
|         - |  332 | `	{ "forward_static_call",       1, 1 },` |
|         - |  333 | `	{ "forward_static_call_array", 2, 0 },` |
|         - |  334 | `	{ "func_get_arg",              1, 0 },` |
|         - |  335 | `	{ "function_exists",           1, 0 },` |
|         - |  336 | `	{ "header",                    1, 1 },` |
|         - |  337 | `	{ "password_get_info",         1, 0 },` |
|         - |  338 | `	{ "putenv",                    1, 0 },` |
|         - |  339 | `	{ "register_shutdown_function", 1, 1 },` |
|         - |  340 | `	{ "set_error_handler",         1, 1 },` |
|         - |  341 | `	{ "set_exception_handler",     1, 0 },` |
|         - |  342 | `	{ "setcookie",                 1, 1 },` |
|         - |  343 | `	{ "setrawcookie",              1, 1 },` |
|         - |  344 | `	{ "trigger_error",             1, 1 },` |
|         - |  345 | `	{ "user_error",                1, 1 },` |
|         - |  346 | `	/*` |
|         - |  347 | `	 * Overrides for signatures that under-report their own minimum: the callback` |
|         - |  348 | `	 * of these three hides inside the variadic tail ("array $array, ...$rest"),` |
|         - |  349 | `	 * so the derivation reads 1 where php requires 2.` |
|         - |  350 | `	 */` |
|         - |  351 | `	{ "array_udiff",               2, 1 },` |
|         - |  352 | `	{ "array_uintersect",          2, 1 },` |
|         - |  353 | `	{ "array_diff_uassoc",         2, 1 },` |
|         - |  354 | `	{ "array_diff_ukey",           2, 1 },` |
|         - |  355 | `	{ "array_intersect_ukey",      2, 1 },` |
|         - |  356 | `	{ "array_intersect_uassoc",    2, 1 },` |
|         - |  357 | `	{ "array_udiff_assoc",         2, 1 },` |
|         - |  358 | `	{ "array_uintersect_assoc",    2, 1 },` |
|         - |  359 | `	{ "array_udiff_uassoc",        3, 1 },` |
|         - |  360 | `	{ "array_uintersect_uassoc",   3, 1 },` |
|         - |  361 | `};` |
|         - |  362 | `/*` |
|         - |  363 | ` * Stamp the minimum-arity metadata from aBuiltinArity[] onto the already` |
|         - |  364 | ` * registered host functions. Called once at VM init after every builtin family` |
|         - |  365 | ` * has been installed into hHostFunction. A name absent from the hash (e.g. a` |
|         - |  366 | ` * build without a given extension) is simply skipped.` |
|         - |  367 | ` */` |
|      5619 |  368 | `PH7_PRIVATE void VmSetBuiltinArity(ph7_vm *pVm)` |
|         5 |  369 | `{` |
|         - |  370 | `	sxu32 n;` |
|   1680086 |  371 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinArity) ; ++n ){` |
|   1674467 |  372 | `		const struct VmBuiltinArity *p = &aBuiltinArity[n];` |
|   3348929 |  373 | `		SyHashEntry *pEntry = SyHashGet(&pVm->hHostFunction,` |
|   1674462 |  374 | `			(const void *)p->zName,SyStrlen(p->zName));` |
|   1674467 |  375 | `		if( pEntry ){` |
|   1674467 |  376 | `			ph7_user_func *pFunc = (ph7_user_func *)pEntry->pUserData;` |
|   1674467 |  377 | `			pFunc->nMinArg  = p->nMin;` |
|   1674467 |  378 | `			pFunc->bAtLeast = p->bAtLeast;` |
|    835890 |  379 | `		}` |
|    835895 |  380 | `	}` |
|      5624 |  381 | `}` |
|         - |  382 | `/*` |
|         - |  383 | ` * PHP 8.5 parameter signatures for the C builtins, generated offline from` |
|         - |  384 | ` * a real PHP 8.5 ReflectionFunction dump over PHL's registered function` |
|         - |  385 | ` * list (see the plan's Reflection section). "= ?" marks an optional` |
|         - |  386 | ` * parameter whose default is not representable as a short literal.` |
|         - |  387 | ` * Reflection parses these strings on demand; unlisted builtins degrade to` |
|         - |  388 | ` * the min-arity data.` |
|         - |  389 | ` */` |
|         - |  390 | `static const struct VmBuiltinSig {` |
|         - |  391 | `	const char *zName;` |
|         - |  392 | `	const char *zSig;` |
|         - |  393 | `	const char *zRet;` |
|         - |  394 | `} aBuiltinSig[] = {` |
|         - |  395 | `	/* The subsystems converted from embedded PHP into C (INI, libxml, sessions).` |
|         - |  396 | `	 * A prelude function declared its parameters in PHP and Reflection read them` |
|         - |  397 | `	 * from there; a C builtin has no declaration but this table, so without a row` |
|         - |  398 | `	 * here the same function reports NO parameters -- and loses its arity bounds` |
|         - |  399 | `	 * with them. */` |
|         - |  400 | `	/* ext/curl. Signatures dumped from php 8.5's own ReflectionFunction, which` |
|         - |  401 | `	 * is also where the parameter NAMES come from: a named argument spells the` |
|         - |  402 | `	 * php one, so an invented name breaks valid php. */` |
|         - |  403 | `	{ "curl_close", "CurlHandle $handle", "void" },` |
|         - |  404 | `	{ "curl_copy_handle", "CurlHandle $handle", "CurlHandle\|false" },` |
|         - |  405 | `	{ "curl_errno", "CurlHandle $handle", "int" },` |
|         - |  406 | `	{ "curl_error", "CurlHandle $handle", "string" },` |
|         - |  407 | `	{ "curl_escape", "CurlHandle $handle, string $string", "string\|false" },` |
|         - |  408 | `	{ "curl_exec", "CurlHandle $handle", "string\|bool" },` |
|         - |  409 | `	{ "curl_file_create", "string $filename, ?string $mime_type = null, ?string $posted_filename = null", "CURLFile" },` |
|         - |  410 | `	{ "curl_getinfo", "CurlHandle $handle, ?int $option = null", "mixed" },` |
|         - |  411 | `	{ "curl_init", "?string $url = null", "CurlHandle\|false" },` |
|         - |  412 | `	{ "curl_multi_add_handle", "CurlMultiHandle $multi_handle, CurlHandle $handle", "int" },` |
|         - |  413 | `	{ "curl_multi_close", "CurlMultiHandle $multi_handle", "void" },` |
|         - |  414 | `	{ "curl_multi_errno", "CurlMultiHandle $multi_handle", "int" },` |
|         - |  415 | `	{ "curl_multi_exec", "CurlMultiHandle $multi_handle, &$still_running", "int" },` |
|         - |  416 | `	{ "curl_multi_get_handles", "CurlMultiHandle $multi_handle", "array" },` |
|         - |  417 | `	{ "curl_multi_getcontent", "CurlHandle $handle", "?string" },` |
|         - |  418 | `	{ "curl_multi_info_read", "CurlMultiHandle $multi_handle, &$queued_messages = NULL", "array\|false" },` |
|         - |  419 | `	{ "curl_multi_init", "", "CurlMultiHandle" },` |
|         - |  420 | `	{ "curl_multi_remove_handle", "CurlMultiHandle $multi_handle, CurlHandle $handle", "int" },` |
|         - |  421 | `	{ "curl_multi_select", "CurlMultiHandle $multi_handle, float $timeout = 1.0", "int" },` |
|         - |  422 | `	{ "curl_multi_setopt", "CurlMultiHandle $multi_handle, int $option, mixed $value", "bool" },` |
|         - |  423 | `	{ "curl_multi_strerror", "int $error_code", "?string" },` |
|         - |  424 | `	{ "curl_pause", "CurlHandle $handle, int $flags", "int" },` |
|         - |  425 | `	{ "curl_reset", "CurlHandle $handle", "void" },` |
|         - |  426 | `	{ "curl_setopt", "CurlHandle $handle, int $option, mixed $value", "bool" },` |
|         - |  427 | `	{ "curl_setopt_array", "CurlHandle $handle, array $options", "bool" },` |
|         - |  428 | `	{ "curl_unescape", "CurlHandle $handle, string $string", "string\|false" },` |
|         - |  429 | `	{ "curl_upkeep", "CurlHandle $handle", "bool" },` |
|         - |  430 | `	{ "curl_share_close", "CurlShareHandle $share_handle", "void" },` |
|         - |  431 | `	{ "curl_share_errno", "CurlShareHandle $share_handle", "int" },` |
|         - |  432 | `	{ "curl_share_init", "", "CurlShareHandle" },` |
|         - |  433 | `	{ "curl_share_init_persistent", "array $share_options", "CurlSharePersistentHandle" },` |
|         - |  434 | `	{ "curl_share_setopt", "CurlShareHandle $share_handle, int $option, mixed $value", "bool" },` |
|         - |  435 | `	{ "curl_share_strerror", "int $error_code", "?string" },` |
|         - |  436 | `	{ "curl_strerror", "int $error_code", "?string" },` |
|         - |  437 | `	{ "curl_version", "", "array\|false" },` |
|         - |  438 | `	/* ext/openssl. Signatures dumped from php 8.5's own ReflectionFunction --` |
|         - |  439 | `	 * the parameter NAMES included, since a named argument spells the php one.` |
|         - |  440 | ``	 * The five untyped `$key` / `$certificate` parameters are untyped in php`` |
|         - |  441 | ``	 * too: each takes a handle object, a PEM string, a `file://` path or an`` |
|         - |  442 | `	 * array pair, so php declares no type and screens by hand. */` |
|         - |  443 | `	{ "openssl_x509_export_to_file", "OpenSSLCertificate\|string $certificate, string $output_filename, bool $no_text = true", "bool" },` |
|         - |  444 | `	{ "openssl_x509_export", "OpenSSLCertificate\|string $certificate, &$output, bool $no_text = true", "bool" },` |
|         - |  445 | `	{ "openssl_x509_fingerprint", "OpenSSLCertificate\|string $certificate, string $digest_algo = 'sha1', bool $binary = false", "string\|false" },` |
|         - |  446 | `	{ "openssl_x509_check_private_key", "OpenSSLCertificate\|string $certificate, $private_key", "bool" },` |
|         - |  447 | `	{ "openssl_x509_verify", "OpenSSLCertificate\|string $certificate, $public_key", "int" },` |
|         - |  448 | `	{ "openssl_x509_parse", "OpenSSLCertificate\|string $certificate, bool $short_names = true", "array\|false" },` |
|         - |  449 | `	{ "openssl_x509_checkpurpose", "OpenSSLCertificate\|string $certificate, int $purpose, array $ca_info = [], ?string $untrusted_certificates_file = NULL", "int\|bool" },` |
|         - |  450 | `	{ "openssl_x509_read", "OpenSSLCertificate\|string $certificate", "OpenSSLCertificate\|false" },` |
|         - |  451 | `	{ "openssl_pkcs12_export_to_file", "OpenSSLCertificate\|string $certificate, string $output_filename, $private_key, string $passphrase, array $options = []", "bool" },` |
|         - |  452 | `	{ "openssl_pkcs12_export", "OpenSSLCertificate\|string $certificate, &$output, $private_key, string $passphrase, array $options = []", "bool" },` |
|         - |  453 | `	{ "openssl_pkcs12_read", "string $pkcs12, &$certificates, string $passphrase", "bool" },` |
|         - |  454 | `	{ "openssl_csr_export_to_file", "OpenSSLCertificateSigningRequest\|string $csr, string $output_filename, bool $no_text = true", "bool" },` |
|         - |  455 | `	{ "openssl_csr_export", "OpenSSLCertificateSigningRequest\|string $csr, &$output, bool $no_text = true", "bool" },` |
|         - |  456 | `	{ "openssl_csr_sign", "OpenSSLCertificateSigningRequest\|string $csr, OpenSSLCertificate\|string\|null $ca_certificate, $private_key, int $days, ?array $options = NULL, int $serial = 0, ?string $serial_hex = NULL", "OpenSSLCertificate\|false" },` |
|         - |  457 | `	{ "openssl_csr_new", "array $distinguished_names, &$private_key, ?array $options = NULL, ?array $extra_attributes = NULL", "OpenSSLCertificateSigningRequest\|bool" },` |
|         - |  458 | `	{ "openssl_csr_get_subject", "OpenSSLCertificateSigningRequest\|string $csr, bool $short_names = true", "array\|false" },` |
|         - |  459 | `	{ "openssl_csr_get_public_key", "OpenSSLCertificateSigningRequest\|string $csr, bool $short_names = true", "OpenSSLAsymmetricKey\|false" },` |
|         - |  460 | `	{ "openssl_pkcs7_verify", "string $input_filename, int $flags, ?string $signers_certificates_filename = NULL, array $ca_info = [], ?string $untrusted_certificates_filename = NULL, ?string $content = NULL, ?string $output_filename = NULL", "int\|bool" },` |
|         - |  461 | `	{ "openssl_pkcs7_encrypt", "string $input_filename, string $output_filename, $certificate, ?array $headers, int $flags = 0, int $cipher_algo = OPENSSL_CIPHER_AES_128_CBC", "bool" },` |
|         - |  462 | `	{ "openssl_pkcs7_sign", "string $input_filename, string $output_filename, OpenSSLCertificate\|string $certificate, $private_key, ?array $headers, int $flags = PKCS7_DETACHED, ?string $untrusted_certificates_filename = NULL", "bool" },` |
|         - |  463 | `	{ "openssl_pkcs7_decrypt", "string $input_filename, string $output_filename, $certificate, $private_key = NULL", "bool" },` |
|         - |  464 | `	{ "openssl_pkcs7_read", "string $data, &$certificates", "bool" },` |
|         - |  465 | `	{ "openssl_cms_verify", "string $input_filename, int $flags = 0, ?string $certificates = NULL, array $ca_info = [], ?string $untrusted_certificates_filename = NULL, ?string $content = NULL, ?string $pk7 = NULL, ?string $sigfile = NULL, int $encoding = OPENSSL_ENCODING_SMIME", "bool" },` |
|         - |  466 | `	{ "openssl_cms_encrypt", "string $input_filename, string $output_filename, $certificate, ?array $headers, int $flags = 0, int $encoding = OPENSSL_ENCODING_SMIME, string\|int $cipher_algo = OPENSSL_CIPHER_AES_128_CBC", "bool" },` |
|         - |  467 | `	{ "openssl_cms_sign", "string $input_filename, string $output_filename, OpenSSLCertificate\|string $certificate, $private_key, ?array $headers, int $flags = 0, int $encoding = OPENSSL_ENCODING_SMIME, ?string $untrusted_certificates_filename = NULL", "bool" },` |
|         - |  468 | `	{ "openssl_cms_decrypt", "string $input_filename, string $output_filename, $certificate, $private_key = NULL, int $encoding = OPENSSL_ENCODING_SMIME", "bool" },` |
|         - |  469 | `	{ "openssl_cms_read", "string $input_filename, &$certificates", "bool" },` |
|         - |  470 | `	{ "openssl_pbkdf2", "string $password, string $salt, int $key_length, int $iterations, string $digest_algo = 'sha1'", "string\|false" },` |
|         - |  471 | `	{ "openssl_error_string", "", "string\|false" },` |
|         - |  472 | `	{ "openssl_get_md_methods", "bool $aliases = false", "array" },` |
|         - |  473 | `	{ "openssl_get_cipher_methods", "bool $aliases = false", "array" },` |
|         - |  474 | `	{ "openssl_get_curve_names", "", "array\|false" },` |
|         - |  475 | `	{ "openssl_digest", "string $data, string $digest_algo, bool $binary = false", "string\|false" },` |
|         - |  476 | `	{ "openssl_encrypt", "string $data, string $cipher_algo, string $passphrase, int $options = 0, string $iv = '', &$tag = NULL, string $aad = '', int $tag_length = 16", "string\|false" },` |
|         - |  477 | `	{ "openssl_decrypt", "string $data, string $cipher_algo, string $passphrase, int $options = 0, string $iv = '', ?string $tag = NULL, string $aad = ''", "string\|false" },` |
|         - |  478 | `	{ "openssl_cipher_iv_length", "string $cipher_algo", "int\|false" },` |
|         - |  479 | `	{ "openssl_cipher_key_length", "string $cipher_algo", "int\|false" },` |
|         - |  480 | `	{ "openssl_random_pseudo_bytes", "int $length, &$strong_result = NULL", "string" },` |
|         - |  481 | `	{ "openssl_get_cert_locations", "", "array" },` |
|         - |  482 | `	{ "openssl_pkey_new", "?array $options = NULL", "OpenSSLAsymmetricKey\|false" },` |
|         - |  483 | `	{ "openssl_pkey_export_to_file", "$key, string $output_filename, ?string $passphrase = NULL, ?array $options = NULL", "bool" },` |
|         - |  484 | `	{ "openssl_pkey_export", "$key, &$output, ?string $passphrase = NULL, ?array $options = NULL", "bool" },` |
|         - |  485 | `	{ "openssl_pkey_get_public", "$public_key", "OpenSSLAsymmetricKey\|false" },` |
|         - |  486 | `	{ "openssl_get_publickey", "$public_key", "OpenSSLAsymmetricKey\|false" },` |
|         - |  487 | `	{ "openssl_pkey_get_private", "$private_key, ?string $passphrase = NULL", "OpenSSLAsymmetricKey\|false" },` |
|         - |  488 | `	{ "openssl_get_privatekey", "$private_key, ?string $passphrase = NULL", "OpenSSLAsymmetricKey\|false" },` |
|         - |  489 | `	{ "openssl_pkey_get_details", "OpenSSLAsymmetricKey $key", "array\|false" },` |
|         - |  490 | `	{ "openssl_private_encrypt", "string $data, &$encrypted_data, $private_key, int $padding = OPENSSL_PKCS1_PADDING", "bool" },` |
|         - |  491 | `	{ "openssl_private_decrypt", "string $data, &$decrypted_data, $private_key, int $padding = OPENSSL_PKCS1_PADDING, ?string $digest_algo = NULL", "bool" },` |
|         - |  492 | `	{ "openssl_public_encrypt", "string $data, &$encrypted_data, $public_key, int $padding = OPENSSL_PKCS1_PADDING, ?string $digest_algo = NULL", "bool" },` |
|         - |  493 | `	{ "openssl_public_decrypt", "string $data, &$decrypted_data, $public_key, int $padding = OPENSSL_PKCS1_PADDING", "bool" },` |
|         - |  494 | `	{ "openssl_sign", "string $data, &$signature, $private_key, string\|int $algorithm = OPENSSL_ALGO_SHA1, int $padding = 0", "bool" },` |
|         - |  495 | `	{ "openssl_verify", "string $data, string $signature, $public_key, string\|int $algorithm = OPENSSL_ALGO_SHA1, int $padding = 0", "int\|false" },` |
|         - |  496 | `	{ "openssl_seal", "string $data, &$sealed_data, &$encrypted_keys, array $public_key, string $cipher_algo, &$iv = NULL", "int\|false" },` |
|         - |  497 | `	{ "openssl_open", "string $data, &$output, string $encrypted_key, $private_key, string $cipher_algo, ?string $iv = NULL", "bool" },` |
|         - |  498 | `	{ "openssl_dh_compute_key", "string $public_key, OpenSSLAsymmetricKey $private_key", "string\|false" },` |
|         - |  499 | `	{ "openssl_pkey_derive", "$public_key, $private_key", "string\|false" },` |
|         - |  500 | `	{ "openssl_spki_new", "OpenSSLAsymmetricKey $private_key, string $challenge, int $digest_algo = OPENSSL_ALGO_MD5", "string\|false" },` |
|         - |  501 | `	{ "openssl_spki_verify", "string $spki", "bool" },` |
|         - |  502 | `	{ "openssl_spki_export", "string $spki", "string\|false" },` |
|         - |  503 | `	{ "openssl_spki_export_challenge", "string $spki", "string\|false" },` |
|         - |  504 | `	{ "get_cfg_var", "string $option", "array\|string\|false" },` |
|         - |  505 | `	{ "ini_get", "string $option", "string\|false" },` |
|         - |  506 | `	{ "ini_get_all", "?string $extension = null, bool $details = true", "array\|false" },` |
|         - |  507 | `	{ "ini_restore", "string $option", "void" },` |
|         - |  508 | `	{ "ini_alter", "string $option, string\|int\|float\|bool\|null $value", "string\|false" },` |
|         - |  509 | `	{ "ini_set", "string $option, string\|int\|float\|bool\|null $value", "string\|false" },` |
|         - |  510 | `	{ "libxml_clear_errors", "", "void" },` |
|         - |  511 | `	{ "libxml_get_errors", "", "array" },` |
|         - |  512 | `	{ "libxml_get_external_entity_loader", "", "?callable" },` |
|         - |  513 | `	{ "libxml_get_last_error", "", "LibXMLError\|false" },` |
|         - |  514 | `	{ "libxml_set_external_entity_loader", "?callable $resolver_function", "true" },` |
|         - |  515 | `	{ "libxml_set_streams_context", "$context", "void" },` |
|         - |  516 | `	{ "libxml_use_internal_errors", "?bool $use_errors = null", "bool" },` |
|         - |  517 | ``	/* ext/simplexml's three, and ext/dom's one door into it. `object $node` is`` |
|         - |  518 | `	 * php's own declaration for both directions: the class screen is the body's,` |
|         - |  519 | `	 * so a plain object gets the TypeError the body words and not ZPP's. */` |
|         - |  520 | `	{ "simplexml_load_file",` |
|         - |  521 | `	  "string $filename, ?string $class_name = SimpleXMLElement::class, int $options = 0, "` |
|         - |  522 | `	  "string $namespace_or_prefix = '', bool $is_prefix = false", "SimpleXMLElement\|false" },` |
|         - |  523 | `	{ "simplexml_load_string",` |
|         - |  524 | `	  "string $data, ?string $class_name = SimpleXMLElement::class, int $options = 0, "` |
|         - |  525 | `	  "string $namespace_or_prefix = '', bool $is_prefix = false", "SimpleXMLElement\|false" },` |
|         - |  526 | `	{ "simplexml_import_dom",` |
|         - |  527 | `	  "object $node, ?string $class_name = SimpleXMLElement::class", "?SimpleXMLElement" },` |
|         - |  528 | `	{ "dom_import_simplexml", "object $node", "DOMAttr\|DOMElement" },` |
|         - |  529 | `	/* ext/pdo's one function: the procedural spelling of` |
|         - |  530 | `	 * PDO::getAvailableDrivers(). */` |
|         - |  531 | `	{ "pdo_drivers", "", "array" },` |
|         - |  532 | `	{ "xml_error_string", "int $error_code", "?string" },` |
|         - |  533 | `	{ "xml_get_current_byte_index", "XMLParser $parser", "int" },` |
|         - |  534 | `	{ "xml_get_current_column_number", "XMLParser $parser", "int" },` |
|         - |  535 | `	{ "xml_get_current_line_number", "XMLParser $parser", "int" },` |
|         - |  536 | `	{ "xml_get_error_code", "XMLParser $parser", "int" },` |
|         - |  537 | `	{ "xml_parse", "XMLParser $parser, string $data, bool $is_final = false", "int" },` |
|         - |  538 | `	{ "xml_parse_into_struct", "XMLParser $parser, string $data, &$values, &$index = NULL", "int\|false" },` |
|         - |  539 | `	{ "xml_parser_create", "?string $encoding = NULL", "XMLParser" },` |
|         - |  540 | `	{ "xml_parser_create_ns", "?string $encoding = NULL, string $separator = ':'", "XMLParser" },` |
|         - |  541 | `	{ "xml_parser_get_option", "XMLParser $parser, int $option", "string\|int\|bool" },` |
|         - |  542 | `	{ "xml_parser_set_option", "XMLParser $parser, int $option, $value", "bool" },` |
|         - |  543 | `	{ "xml_set_character_data_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  544 | `	{ "xml_set_default_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  545 | `	{ "xml_set_element_handler", "XMLParser $parser, callable\|string\|null $start_handler, callable\|string\|null $end_handler", "true" },` |
|         - |  546 | `	{ "xml_set_end_namespace_decl_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  547 | `	{ "xml_set_external_entity_ref_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  548 | `	{ "xml_set_notation_decl_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  549 | `	{ "xml_set_processing_instruction_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  550 | `	{ "xml_set_start_namespace_decl_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  551 | `	{ "xml_set_unparsed_entity_decl_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  552 | `	/* ext/xmlwriter: php presents every writer verb under a function name as` |
|         - |  553 | `	 * well, with the writer as argument #1 -- which is the numbering its own` |
|         - |  554 | `	 * diagnostics report from BOTH spellings (see vm_xmlwriter.c). */` |
|         - |  555 | `	{ "xmlwriter_open_uri", "string $uri", "XMLWriter\|false" },` |
|         - |  556 | `	{ "xmlwriter_open_memory", "", "XMLWriter\|false" },` |
|         - |  557 | `	{ "xmlwriter_set_indent", "XMLWriter $writer, bool $enable", "bool" },` |
|         - |  558 | `	{ "xmlwriter_set_indent_string", "XMLWriter $writer, string $indentation", "bool" },` |
|         - |  559 | `	{ "xmlwriter_start_comment", "XMLWriter $writer", "bool" },` |
|         - |  560 | `	{ "xmlwriter_end_comment", "XMLWriter $writer", "bool" },` |
|         - |  561 | `	{ "xmlwriter_start_attribute", "XMLWriter $writer, string $name", "bool" },` |
|         - |  562 | `	{ "xmlwriter_end_attribute", "XMLWriter $writer", "bool" },` |
|         - |  563 | `	{ "xmlwriter_write_attribute", "XMLWriter $writer, string $name, string $value", "bool" },` |
|         - |  564 | `	{ "xmlwriter_start_attribute_ns", "XMLWriter $writer, ?string $prefix, string $name, ?string $namespace", "bool" },` |
|         - |  565 | `	{ "xmlwriter_write_attribute_ns", "XMLWriter $writer, ?string $prefix, string $name, ?string $namespace, string $value", "bool" },` |
|         - |  566 | `	{ "xmlwriter_start_element", "XMLWriter $writer, string $name", "bool" },` |
|         - |  567 | `	{ "xmlwriter_end_element", "XMLWriter $writer", "bool" },` |
|         - |  568 | `	{ "xmlwriter_full_end_element", "XMLWriter $writer", "bool" },` |
|         - |  569 | `	{ "xmlwriter_start_element_ns", "XMLWriter $writer, ?string $prefix, string $name, ?string $namespace", "bool" },` |
|         - |  570 | `	{ "xmlwriter_write_element", "XMLWriter $writer, string $name, ?string $content = null", "bool" },` |
|         - |  571 | `	{ "xmlwriter_write_element_ns", "XMLWriter $writer, ?string $prefix, string $name, ?string $namespace, ?string $content = null", "bool" },` |
|         - |  572 | `	{ "xmlwriter_start_pi", "XMLWriter $writer, string $target", "bool" },` |
|         - |  573 | `	{ "xmlwriter_end_pi", "XMLWriter $writer", "bool" },` |
|         - |  574 | `	{ "xmlwriter_write_pi", "XMLWriter $writer, string $target, string $content", "bool" },` |
|         - |  575 | `	{ "xmlwriter_start_cdata", "XMLWriter $writer", "bool" },` |
|         - |  576 | `	{ "xmlwriter_end_cdata", "XMLWriter $writer", "bool" },` |
|         - |  577 | `	{ "xmlwriter_write_cdata", "XMLWriter $writer, string $content", "bool" },` |
|         - |  578 | `	{ "xmlwriter_text", "XMLWriter $writer, string $content", "bool" },` |
|         - |  579 | `	{ "xmlwriter_write_raw", "XMLWriter $writer, string $content", "bool" },` |
|         - |  580 | `	{ "xmlwriter_start_document", "XMLWriter $writer, ?string $version = '1.0', ?string $encoding = null, ?string $standalone = null", "bool" },` |
|         - |  581 | `	{ "xmlwriter_end_document", "XMLWriter $writer", "bool" },` |
|         - |  582 | `	{ "xmlwriter_write_comment", "XMLWriter $writer, string $content", "bool" },` |
|         - |  583 | `	{ "xmlwriter_start_dtd", "XMLWriter $writer, string $qualifiedName, ?string $publicId = null, ?string $systemId = null", "bool" },` |
|         - |  584 | `	{ "xmlwriter_end_dtd", "XMLWriter $writer", "bool" },` |
|         - |  585 | `	{ "xmlwriter_write_dtd", "XMLWriter $writer, string $name, ?string $publicId = null, ?string $systemId = null, ?string $content = null", "bool" },` |
|         - |  586 | `	{ "xmlwriter_start_dtd_element", "XMLWriter $writer, string $qualifiedName", "bool" },` |
|         - |  587 | `	{ "xmlwriter_end_dtd_element", "XMLWriter $writer", "bool" },` |
|         - |  588 | `	{ "xmlwriter_write_dtd_element", "XMLWriter $writer, string $name, string $content", "bool" },` |
|         - |  589 | `	{ "xmlwriter_start_dtd_attlist", "XMLWriter $writer, string $name", "bool" },` |
|         - |  590 | `	{ "xmlwriter_end_dtd_attlist", "XMLWriter $writer", "bool" },` |
|         - |  591 | `	{ "xmlwriter_write_dtd_attlist", "XMLWriter $writer, string $name, string $content", "bool" },` |
|         - |  592 | `	{ "xmlwriter_start_dtd_entity", "XMLWriter $writer, string $name, bool $isParam", "bool" },` |
|         - |  593 | `	{ "xmlwriter_end_dtd_entity", "XMLWriter $writer", "bool" },` |
|         - |  594 | `	{ "xmlwriter_write_dtd_entity", "XMLWriter $writer, string $name, string $content, bool $isParam = false, ?string $publicId = null, ?string $systemId = null, ?string $notationData = null", "bool" },` |
|         - |  595 | `	{ "xmlwriter_output_memory", "XMLWriter $writer, bool $flush = true", "string" },` |
|         - |  596 | `	{ "xmlwriter_flush", "XMLWriter $writer, bool $empty = true", "string\|int" },` |
|         - |  597 | `	{ "session_abort", "", "bool" },` |
|         - |  598 | `	{ "session_cache_expire", "?int $value = null", "int\|false" },` |
|         - |  599 | `	{ "session_cache_limiter", "?string $value = null", "string\|false" },` |
|         - |  600 | `	{ "session_commit", "", "bool" },` |
|         - |  601 | `	{ "session_create_id", "string $prefix = \"\"", "string\|false" },` |
|         - |  602 | `	{ "session_decode", "string $data", "bool" },` |
|         - |  603 | `	{ "session_destroy", "", "bool" },` |
|         - |  604 | `	{ "session_gc", "", "int\|false" },` |
|         - |  605 | `	{ "session_get_cookie_params", "", "array" },` |
|         - |  606 | `	{ "session_set_save_handler", "$sessionhandler, ...$rest = ?", "bool" },` |
|         - |  607 | `	{ "session_set_cookie_params", "array\|int $lifetime_or_options, ?string $path = null, ?string $domain = null, ?bool $secure = null, ?bool $httponly = null", "bool" },` |
|         - |  608 | `	{ "session_encode", "", "string\|false" },` |
|         - |  609 | `	{ "session_id", "?string $id = null", "string\|false" },` |
|         - |  610 | `	{ "session_module_name", "?string $module = null", "string\|false" },` |
|         - |  611 | `	{ "session_name", "?string $name = null", "string\|false" },` |
|         - |  612 | `	{ "session_regenerate_id", "bool $delete_old_session = false", "bool" },` |
|         - |  613 | `	{ "session_register_shutdown", "", "void" },` |
|         - |  614 | `	{ "session_reset", "", "bool" },` |
|         - |  615 | `	{ "session_save_path", "?string $path = null", "string\|false" },` |
|         - |  616 | `	{ "session_start", "array $options = []", "bool" },` |
|         - |  617 | `	{ "session_status", "", "int" },` |
|         - |  618 | `	{ "session_unset", "", "bool" },` |
|         - |  619 | `	{ "session_write_close", "", "bool" },` |
|         - |  620 | `	{ "abs", "int\|float $num", "int\|float" },` |
|         - |  621 | `	{ "acos", "float $num", "float" },` |
|         - |  622 | `	{ "acosh", "float $num", "float" },` |
|         - |  623 | `	{ "addcslashes", "string $string, string $characters", "string" },` |
|         - |  624 | `	{ "addslashes", "string $string", "string" },` |
|         - |  625 | `	{ "array_all", "array $array, callable $callback", "bool" },` |
|         - |  626 | `	{ "array_any", "array $array, callable $callback", "bool" },` |
|         - |  627 | `	{ "array_chunk", "array $array, int $length, bool $preserve_keys = false", "array" },` |
|         - |  628 | `	{ "array_column", "array $array, string\|int\|null $column_key, string\|int\|null $index_key = NULL", "array" },` |
|         - |  629 | `	{ "array_combine", "array $keys, array $values", "array" },` |
|         - |  630 | `	{ "array_diff", "array $array, array ...$arrays = ?", "array" },` |
|         - |  631 | `	{ "array_diff_assoc", "array $array, array ...$arrays = ?", "array" },` |
|         - |  632 | `	{ "array_diff_key", "array $array, array ...$arrays = ?", "array" },` |
|         - |  633 | `	{ "array_diff_uassoc", "array $array, ...$rest = ?", "array" },` |
|         - |  634 | `	{ "array_diff_ukey", "array $array, ...$rest = ?", "array" },` |
|         - |  635 | `	{ "array_fill", "int $start_index, int $count, mixed $value", "array" },` |
|         - |  636 | `	{ "array_fill_keys", "array $keys, mixed $value", "array" },` |
|         - |  637 | `	{ "array_filter", "array $array, ?callable $callback = NULL, int $mode = 0", "array" },` |
|         - |  638 | `	{ "array_find", "array $array, callable $callback", "mixed" },` |
|         - |  639 | `	{ "array_find_key", "array $array, callable $callback", "mixed" },` |
|         - |  640 | `	{ "array_first", "array $array", "mixed" },` |
|         - |  641 | `	{ "array_flip", "array $array", "array" },` |
|         - |  642 | `	{ "array_intersect", "array $array, array ...$arrays = ?", "array" },` |
|         - |  643 | `	{ "array_intersect_assoc", "array $array, array ...$arrays = ?", "array" },` |
|         - |  644 | `	{ "array_intersect_key", "array $array, array ...$arrays = ?", "array" },` |
|         - |  645 | `	{ "array_intersect_uassoc", "array $array, ...$rest = ?", "array" },` |
|         - |  646 | `	{ "array_intersect_ukey", "array $array, ...$rest = ?", "array" },` |
|         - |  647 | `	{ "array_is_list", "array $array", "bool" },` |
|         - |  648 | `	{ "array_key_exists", "$key, array $array", "bool" },` |
|         - |  649 | `	{ "array_key_first", "array $array", "string\|int\|null" },` |
|         - |  650 | `	{ "array_key_last", "array $array", "string\|int\|null" },` |
|         - |  651 | `	{ "array_keys", "array $array, mixed $filter_value = ?, bool $strict = false", "array" },` |
|         - |  652 | `	{ "array_last", "array $array", "mixed" },` |
|         - |  653 | `	{ "array_map", "?callable $callback, array $array, array ...$arrays = ?", "array" },` |
|         - |  654 | `	{ "array_merge", "array ...$arrays = ?", "array" },` |
|         - |  655 | `	{ "array_multisort", "&$array, &...$rest = ?", "true" },` |
|         - |  656 | `	{ "array_merge_recursive", "array ...$arrays = ?", "array" },` |
|         - |  657 | `	{ "array_pad", "array $array, int $length, mixed $value", "array" },` |
|         - |  658 | `	{ "array_pop", "array &$array", "mixed" },` |
|         - |  659 | `	{ "array_product", "array $array", "int\|float" },` |
|         - |  660 | `	{ "array_push", "array &$array, mixed ...$values = ?", "int" },` |
|         - |  661 | `	{ "array_rand", "array $array, int $num = 1", "array\|string\|int" },` |
|         - |  662 | `	{ "array_reduce", "array $array, callable $callback, mixed $initial = NULL", "mixed" },` |
|         - |  663 | `	{ "array_replace", "array $array, array ...$replacements = ?", "array" },` |
|         - |  664 | `	{ "array_reverse", "array $array, bool $preserve_keys = false", "array" },` |
|         - |  665 | `	{ "array_search", "mixed $needle, array $haystack, bool $strict = false", "string\|int\|false" },` |
|         - |  666 | `	{ "array_shift", "array &$array", "mixed" },` |
|         - |  667 | `	{ "array_slice", "array $array, int $offset, ?int $length = NULL, bool $preserve_keys = false", "array" },` |
|         - |  668 | `	{ "array_splice", "array &$array, int $offset, ?int $length = NULL, mixed $replacement = []", "array" },` |
|         - |  669 | `	{ "array_sum", "array $array", "int\|float" },` |
|         - |  670 | `	{ "array_udiff", "array $array, ...$rest = ?", "array" },` |
|         - |  671 | `	{ "array_udiff_assoc", "array $array, ...$rest = ?", "array" },` |
|         - |  672 | `	{ "array_udiff_uassoc", "array $array, ...$rest = ?", "array" },` |
|         - |  673 | `	{ "array_uintersect", "array $array, ...$rest = ?", "array" },` |
|         - |  674 | `	{ "array_uintersect_assoc", "array $array, ...$rest = ?", "array" },` |
|         - |  675 | `	{ "array_uintersect_uassoc", "array $array, ...$rest = ?", "array" },` |
|         - |  676 | `	{ "array_unique", "array $array, int $flags = SORT_STRING", "array" },` |
|         - |  677 | `	{ "array_unshift", "array &$array, mixed ...$values = ?", "int" },` |
|         - |  678 | `	{ "array_values", "array $array", "array" },` |
|         - |  679 | `	{ "array_walk", "object\|array &$array, callable $callback, mixed $arg = ?", "true" },` |
|         - |  680 | `	{ "array_walk_recursive", "object\|array &$array, callable $callback, mixed $arg = ?", "true" },` |
|         - |  681 | `	{ "arsort", "array &$array, int $flags = SORT_REGULAR", "true" },` |
|         - |  682 | `	{ "asin", "float $num", "float" },` |
|         - |  683 | `	{ "asinh", "float $num", "float" },` |
|         - |  684 | `	{ "asort", "array &$array, int $flags = SORT_REGULAR", "true" },` |
|         - |  685 | `	{ "assert", "mixed $assertion, Throwable\|string\|null $description = NULL", "bool" },` |
|         - |  686 | `	{ "atan", "float $num", "float" },` |
|         - |  687 | `	{ "atanh", "float $num", "float" },` |
|         - |  688 | `	{ "atan2", "float $y, float $x", "float" },` |
|         - |  689 | `	{ "base64_decode", "string $string, bool $strict = false", "string\|false" },` |
|         - |  690 | `	{ "base64_encode", "string $string", "string" },` |
|         - |  691 | `	{ "base_convert", "string $num, int $from_base, int $to_base", "string" },` |
|         - |  692 | `	{ "basename", "string $path, string $suffix = ''", "string" },` |
|         - |  693 | `	{ "bcadd", "string $num1, string $num2, ?int $scale = NULL", "string" },` |
|         - |  694 | `	{ "bcceil", "string $num", "string" },` |
|         - |  695 | `	{ "bccomp", "string $num1, string $num2, ?int $scale = NULL", "int" },` |
|         - |  696 | `	{ "bcdiv", "string $num1, string $num2, ?int $scale = NULL", "string" },` |
|         - |  697 | `	{ "bcdivmod", "string $num1, string $num2, ?int $scale = NULL", "array" },` |
|         - |  698 | `	{ "bcmod", "string $num1, string $num2, ?int $scale = NULL", "string" },` |
|         - |  699 | `	{ "bcfloor", "string $num", "string" },` |
|         - |  700 | `	{ "bcmul", "string $num1, string $num2, ?int $scale = NULL", "string" },` |
|         - |  701 | `	{ "bcpow", "string $num, string $exponent, ?int $scale = NULL", "string" },` |
|         - |  702 | `	{ "bcpowmod", "string $num, string $exponent, string $modulus, ?int $scale = NULL", "string" },` |
|         - |  703 | `	{ "bcround", "string $num, int $precision = 0, RoundingMode $mode = RoundingMode::HalfAwayFromZero", "string" },` |
|         - |  704 | `	{ "bcsqrt", "string $num, ?int $scale = NULL", "string" },` |
|         - |  705 | `	{ "bcscale", "?int $scale = NULL", "int" },` |
|         - |  706 | `	{ "bcsub", "string $num1, string $num2, ?int $scale = NULL", "string" },` |
|         - |  707 | `	{ "bin2hex", "string $string", "string" },` |
|         - |  708 | `	{ "bindec", "string $binary_string", "int\|float" },` |
|         - |  709 | `	{ "boolval", "mixed $value", "bool" },` |
|         - |  710 | `	{ "call_user_func", "callable $callback, mixed ...$args = ?", "mixed" },` |
|         - |  711 | `	{ "call_user_func_array", "callable $callback, array $args", "mixed" },` |
|         - |  712 | `	{ "cal_days_in_month", "int $calendar, int $month, int $year", "int" },` |
|         - |  713 | `	{ "cal_from_jd", "int $julian_day, int $calendar", "array" },` |
|         - |  714 | `	{ "cal_info", "int $calendar = -1", "array" },` |
|         - |  715 | `	{ "cal_to_jd", "int $calendar, int $month, int $day, int $year", "int" },` |
|         - |  716 | `	{ "ceil", "int\|float $num", "float" },` |
|         - |  717 | `	{ "chdir", "string $directory", "bool" },` |
|         - |  718 | `	{ "chgrp", "string $filename, string\|int $group", "bool" },` |
|         - |  719 | `	{ "chmod", "string $filename, int $permissions", "bool" },` |
|         - |  720 | `	{ "chop", "string $string, string $characters = \" \\n\\r\\t\\v\\x00\"", "string" },` |
|         - |  721 | `	{ "chown", "string $filename, string\|int $user", "bool" },` |
|         - |  722 | `	{ "chr", "int $codepoint", "string" },` |
|         - |  723 | `	{ "chroot", "string $directory", "bool" },` |
|         - |  724 | `	{ "chunk_split", "string $string, int $length = 76, string $separator = \"\\r\\n\"", "string" },` |
|         - |  725 | `	{ "class_alias", "string $class, string $alias, bool $autoload = true", "bool" },` |
|         - |  726 | `	{ "class_exists", "string $class, bool $autoload = true", "bool" },` |
|         - |  727 | ``	/* `$object_or_class` carries NO declared type on purpose: php screens it with`` |
|         - |  728 | `	 * Z_PARAM_OBJ_OR_STR, which refuses in the standard "must be of type` |
|         - |  729 | `	 * object\|string" wording while ReflectionParameter reports no type at all.` |
|         - |  730 | `	 * The builtin raises that refusal itself (vm_builtin_class.c). */` |
|         - |  731 | `	{ "class_implements", "$object_or_class, bool $autoload = true", "array\|false" },` |
|         - |  732 | `	{ "class_parents", "$object_or_class, bool $autoload = true", "array\|false" },` |
|         - |  733 | `	{ "class_uses", "$object_or_class, bool $autoload = true", "array\|false" },` |
|         - |  734 | `	{ "enum_exists", "string $enum, bool $autoload = true", "bool" },` |
|         - |  735 | `	{ "clone", "object $object, array $withProperties = []", "object" },` |
|         - |  736 | `	{ "closedir", "$dir_handle = NULL", "void" },` |
|         - |  737 | `	{ "compact", "$var_name, ...$var_names = ?", "array" },` |
|         - |  738 | `	{ "connection_aborted", "", "int" },` |
|         - |  739 | `	{ "connection_status", "", "int" },` |
|         - |  740 | `	{ "constant", "string $name", "mixed" },` |
|         - |  741 | `	{ "convert_uudecode", "string $string", "string\|false" },` |
|         - |  742 | `	{ "convert_uuencode", "string $string", "string" },` |
|         - |  743 | `	{ "copy", "string $from, string $to, $context = NULL", "bool" },` |
|         - |  744 | `	{ "cos", "float $num", "float" },` |
|         - |  745 | `	{ "cosh", "float $num", "float" },` |
|         - |  746 | `	{ "count", "Countable\|array $value, int $mode = COUNT_NORMAL", "int" },` |
|         - |  747 | `	{ "count_chars", "string $string, int $mode = 0", "array\|string" },` |
|         - |  748 | `	{ "crc32", "string $string", "int" },` |
|         - |  749 | `	{ "ctype_alnum", "mixed $text", "bool" },` |
|         - |  750 | `	{ "ctype_alpha", "mixed $text", "bool" },` |
|         - |  751 | `	{ "ctype_cntrl", "mixed $text", "bool" },` |
|         - |  752 | `	{ "ctype_digit", "mixed $text", "bool" },` |
|         - |  753 | `	{ "ctype_graph", "mixed $text", "bool" },` |
|         - |  754 | `	{ "ctype_lower", "mixed $text", "bool" },` |
|         - |  755 | `	{ "ctype_print", "mixed $text", "bool" },` |
|         - |  756 | `	{ "ctype_punct", "mixed $text", "bool" },` |
|         - |  757 | `	{ "ctype_space", "mixed $text", "bool" },` |
|         - |  758 | `	{ "ctype_upper", "mixed $text", "bool" },` |
|         - |  759 | `	{ "ctype_xdigit", "mixed $text", "bool" },` |
|         - |  760 | `	{ "current", "object\|array $array", "mixed" },` |
|         - |  761 | `	{ "date", "string $format, ?int $timestamp = NULL", "string" },` |
|         - |  762 | `	{ "date_add", "DateTime $object, DateInterval $interval", "DateTime" },` |
|         - |  763 | `	{ "date_create", "string $datetime = 'now', ?DateTimeZone $timezone = NULL", "DateTime\|false" },` |
|         - |  764 | `	{ "date_create_from_format", "string $format, string $datetime, ?DateTimeZone $timezone = NULL", "DateTime\|false" },` |
|         - |  765 | `	{ "date_create_immutable", "string $datetime = 'now', ?DateTimeZone $timezone = NULL", "DateTimeImmutable\|false" },` |
|         - |  766 | `	{ "date_create_immutable_from_format", "string $format, string $datetime, ?DateTimeZone $timezone = NULL", "DateTimeImmutable\|false" },` |
|         - |  767 | `	{ "date_date_set", "DateTime $object, int $year, int $month, int $day", "DateTime" },` |
|         - |  768 | `	{ "date_diff", "DateTimeInterface $baseObject, DateTimeInterface $targetObject, bool $absolute = false", "DateInterval" },` |
|         - |  769 | `	{ "date_format", "DateTimeInterface $object, string $format", "string" },` |
|         - |  770 | `	{ "date_get_last_errors", "", "array\|false" },` |
|         - |  771 | `	{ "date_interval_create_from_date_string", "string $datetime", "DateInterval\|false" },` |
|         - |  772 | `	{ "date_interval_format", "DateInterval $object, string $format", "string" },` |
|         - |  773 | `	{ "date_isodate_set", "DateTime $object, int $year, int $week, int $dayOfWeek = 1", "DateTime" },` |
|         - |  774 | `	{ "date_modify", "DateTime $object, string $modifier", "DateTime\|false" },` |
|         - |  775 | `	{ "date_offset_get", "DateTimeInterface $object", "int" },` |
|         - |  776 | `	{ "date_parse", "string $datetime", "array" },` |
|         - |  777 | `	{ "date_parse_from_format", "string $format, string $datetime", "array" },` |
|         - |  778 | `	{ "date_sub", "DateTime $object, DateInterval $interval", "DateTime" },` |
|         - |  779 | `	{ "date_time_set", "DateTime $object, int $hour, int $minute, int $second = 0, int $microsecond = 0", "DateTime" },` |
|         - |  780 | `	{ "date_timestamp_get", "DateTimeInterface $object", "int" },` |
|         - |  781 | `	{ "date_timestamp_set", "DateTime $object, int $timestamp", "DateTime" },` |
|         - |  782 | `	{ "date_timezone_get", "DateTimeInterface $object", "DateTimeZone\|false" },` |
|         - |  783 | `	{ "date_timezone_set", "DateTime $object, DateTimeZone $timezone", "DateTime" },` |
|         - |  784 | `	{ "timezone_name_get", "DateTimeZone $object", "string" },` |
|         - |  785 | `	{ "timezone_offset_get", "DateTimeZone $object, DateTimeInterface $datetime", "int" },` |
|         - |  786 | `	{ "timezone_open", "string $timezone", "DateTimeZone\|false" },` |
|         - |  787 | `	{ "date_default_timezone_get", "", "string" },` |
|         - |  788 | `	{ "date_default_timezone_set", "string $timezoneId", "bool" },` |
|         - |  789 | `	{ "debug_backtrace", "int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT, int $limit = 0", "array" },` |
|         - |  790 | `	{ "debug_print_backtrace", "int $options = 0, int $limit = 0", "void" },` |
|         - |  791 | `	{ "decbin", "int $num", "string" },` |
|         - |  792 | `	{ "dechex", "int $num", "string" },` |
|         - |  793 | `	{ "decoct", "int $num", "string" },` |
|         - |  794 | `	{ "define", "string $constant_name, mixed $value, bool $case_insensitive = false", "bool" },` |
|         - |  795 | `	{ "defined", "string $constant_name", "bool" },` |
|         - |  796 | `	{ "deg2rad", "float $num", "float" },` |
|         - |  797 | `	{ "die", "string\|int $status = 0", "never" },` |
|         - |  798 | `	{ "dir", "string $directory, $context = NULL", "Directory\|false" },` |
|         - |  799 | `	{ "dirname", "string $path, int $levels = 1", "string" },` |
|         - |  800 | `	{ "disk_free_space", "string $directory", "float\|false" },` |
|         - |  801 | `	{ "disk_total_space", "string $directory", "float\|false" },` |
|         - |  802 | `	{ "diskfreespace", "string $directory", "float\|false" },` |
|         - |  803 | `	{ "easter_date", "?int $year = NULL, int $mode = CAL_EASTER_DEFAULT", "int" },` |
|         - |  804 | `	{ "easter_days", "?int $year = NULL, int $mode = CAL_EASTER_DEFAULT", "int" },` |
|         - |  805 | `	{ "end", "object\|array &$array", "mixed" },` |
|         - |  806 | `	{ "error_get_last", "", "?array" },` |
|         - |  807 | `	{ "error_clear_last", "", "void" },` |
|         - |  808 | `	{ "error_log", "string $message, int $message_type = 0, ?string $destination = NULL, ?string $additional_headers = NULL", "bool" },` |
|         - |  809 | `	{ "error_reporting", "?int $error_level = NULL", "int" },` |
|         - |  810 | `	{ "escapeshellarg", "string $arg", "string" },` |
|         - |  811 | `	{ "escapeshellcmd", "string $command", "string" },` |
|         - |  812 | `	{ "exec", "string $command, &$output = NULL, &$result_code = NULL", "string\|false" },` |
|         - |  813 | `	{ "exit", "string\|int $status = 0", "never" },` |
|         - |  814 | `	{ "exp", "float $num", "float" },` |
|         - |  815 | `	{ "expm1", "float $num", "float" },` |
|         - |  816 | `	{ "explode", "string $separator, string $string, int $limit = PHP_INT_MAX", "array" },` |
|         - |  817 | `	{ "extension_loaded", "string $extension", "bool" },` |
|         - |  818 | `	{ "extract", "array &$array, int $flags = EXTR_OVERWRITE, string $prefix = ''", "int" },` |
|         - |  819 | `	{ "fclose", "$stream", "bool" },` |
|         - |  820 | `	{ "feof", "$stream", "bool" },` |
|         - |  821 | `	{ "fflush", "$stream", "bool" },` |
|         - |  822 | `	{ "fgetc", "$stream", "string\|false" },` |
|         - |  823 | `	{ "fgetcsv", "$stream, ?int $length = NULL, string $separator = ',', string $enclosure = '\"', string $escape = '\\\\'", "array\|false" },` |
|         - |  824 | `	{ "fgets", "$stream, ?int $length = NULL", "string\|false" },` |
|         - |  825 | `	{ "file", "string $filename, int $flags = 0, $context = NULL", "array\|false" },` |
|         - |  826 | `	{ "file_exists", "string $filename", "bool" },` |
|         - |  827 | `	{ "file_get_contents", "string $filename, bool $use_include_path = false, $context = NULL, int $offset = 0, ?int $length = NULL", "string\|false" },` |
|         - |  828 | `	{ "file_put_contents", "string $filename, mixed $data, int $flags = 0, $context = NULL", "int\|false" },` |
|         - |  829 | `	{ "fileatime", "string $filename", "int\|false" },` |
|         - |  830 | `	{ "filectime", "string $filename", "int\|false" },` |
|         - |  831 | `	{ "filegroup", "string $filename", "int\|false" },` |
|         - |  832 | `	{ "fileinode", "string $filename", "int\|false" },` |
|         - |  833 | `	{ "filemtime", "string $filename", "int\|false" },` |
|         - |  834 | `	{ "fileowner", "string $filename", "int\|false" },` |
|         - |  835 | `	{ "fileperms", "string $filename", "int\|false" },` |
|         - |  836 | `	{ "filesize", "string $filename", "int\|false" },` |
|         - |  837 | `	{ "filetype", "string $filename", "string\|false" },` |
|         - |  838 | `	{ "filter_has_var", "int $input_type, string $var_name", "bool" },` |
|         - |  839 | `	{ "filter_id", "string $name", "int\|false" },` |
|         - |  840 | `	{ "filter_input", "int $type, string $var_name, int $filter = FILTER_DEFAULT, array\|int $options = 0", "mixed" },` |
|         - |  841 | `	{ "filter_input_array", "int $type, array\|int $options = FILTER_DEFAULT, bool $add_empty = true", "array\|false\|null" },` |
|         - |  842 | `	{ "filter_list", "", "array" },` |
|         - |  843 | `	{ "filter_var", "mixed $value, int $filter = FILTER_DEFAULT, array\|int $options = 0", "mixed" },` |
|         - |  844 | `	{ "filter_var_array", "array $array, array\|int $options = FILTER_DEFAULT, bool $add_empty = true", "array\|false\|null" },` |
|         - |  845 | `	{ "floatval", "mixed $value", "float" },` |
|         - |  846 | `	{ "flock", "$stream, int $operation, &$would_block = NULL", "bool" },` |
|         - |  847 | `	{ "floor", "int\|float $num", "float" },` |
|         - |  848 | `	{ "flush", "", "void" },` |
|         - |  849 | `	{ "fmod", "float $num1, float $num2", "float" },` |
|         - |  850 | `	{ "fnmatch", "string $pattern, string $filename, int $flags = 0", "bool" },` |
|         - |  851 | `	{ "fopen", "string $filename, string $mode, bool $use_include_path = false, $context = NULL", "" },` |
|         - |  852 | `	{ "forward_static_call", "callable $callback, mixed ...$args = ?", "mixed" },` |
|         - |  853 | `	{ "forward_static_call_array", "callable $callback, array $args", "mixed" },` |
|         - |  854 | `	{ "fpow", "float $num, float $exponent", "float" },` |
|         - |  855 | `	{ "fpassthru", "$stream", "int" },` |
|         - |  856 | ``	/* `~string $format`: php resolves the STREAM first and refuses a closed one`` |
|         - |  857 | `	 * before it looks at the format at all, so the central screen stands aside` |
|         - |  858 | `	 * and PH7_FormatCheckFormatArg() in the body raises the same TypeError` |
|         - |  859 | `	 * after the handle has been accepted. */` |
|         - |  860 | `	{ "fprintf", "$stream, ~string $format, mixed ...$values = ?", "int" },` |
|         - |  861 | `	{ "fputcsv", "$stream, array $fields, string $separator = ',', string $enclosure = '\"', string $escape = '\\\\', string $eol = \"\n\"", "int\|false" },` |
|         - |  862 | `	{ "fputs", "$stream, string $data, ?int $length = NULL", "int\|false" },` |
|         - |  863 | `	{ "fread", "$stream, int $length", "string\|false" },` |
|         - |  864 | `	{ "frenchtojd", "int $month, int $day, int $year", "int" },` |
|         - |  865 | `	{ "fseek", "$stream, int $offset, int $whence = SEEK_SET", "int" },` |
|         - |  866 | `	{ "fstat", "$stream", "array\|false" },` |
|         - |  867 | `	{ "ftell", "$stream", "int\|false" },` |
|         - |  868 | `	{ "ftruncate", "$stream, int $size", "bool" },` |
|         - |  869 | `	{ "func_get_arg", "int $position", "mixed" },` |
|         - |  870 | `	{ "func_get_args", "", "array" },` |
|         - |  871 | `	{ "func_num_args", "", "int" },` |
|         - |  872 | `	{ "function_exists", "string $function", "bool" },` |
|         - |  873 | `	{ "fwrite", "$stream, string $data, ?int $length = NULL", "int\|false" },` |
|         - |  874 | `	{ "gc_collect_cycles", "", "int" },` |
|         - |  875 | `	{ "gc_disable", "", "void" },` |
|         - |  876 | `	{ "gc_enable", "", "void" },` |
|         - |  877 | `	{ "gc_enabled", "", "bool" },` |
|         - |  878 | `	{ "gc_mem_caches", "", "int" },` |
|         - |  879 | `	{ "gc_status", "", "array" },` |
|         - |  880 | `	{ "get_called_class", "", "string" },` |
|         - |  881 | `	/* ext/fileinfo */` |
|         - |  882 | `	{ "finfo_open", "int $flags = FILEINFO_NONE, ?string $magic_database = null", "finfo\|false" },` |
|         - |  883 | `	{ "finfo_close", "finfo $finfo", "true" },` |
|         - |  884 | `	{ "finfo_set_flags", "finfo $finfo, int $flags", "true" },` |
|         - |  885 | `	{ "finfo_file", "finfo $finfo, string $filename, int $flags = FILEINFO_NONE, $context = null", "string\|false" },` |
|         - |  886 | `	{ "finfo_buffer", "finfo $finfo, string $string, int $flags = FILEINFO_NONE, $context = null", "string\|false" },` |
|         - |  887 | `	{ "mime_content_type", "$filename", "string\|false" },` |
|         - |  888 | `	/* ext/zlib. The gz* handle verbs are ALIASES of the stream functions above` |
|         - |  889 | `	 * (php registers them that way, and so does this build), but each carries` |
|         - |  890 | `	 * its OWN signature row -- which is what makes gzread()'s ArgumentCountError` |
|         - |  891 | `	 * and its ValueError say "gzread()" rather than "fread()". */` |
|         - |  892 | `	{ "deflate_add", "DeflateContext $context, string $data, int $flush_mode = ZLIB_SYNC_FLUSH", "string\|false" },` |
|         - |  893 | `	{ "deflate_init", "int $encoding, object\|array $options = []", "DeflateContext\|false" },` |
|         - |  894 | `	{ "gzclose", "$stream", "bool" },` |
|         - |  895 | `	{ "gzcompress", "string $data, int $level = -1, int $encoding = ZLIB_ENCODING_DEFLATE", "string\|false" },` |
|         - |  896 | `	{ "gzdecode", "string $data, int $max_length = 0", "string\|false" },` |
|         - |  897 | `	{ "gzdeflate", "string $data, int $level = -1, int $encoding = ZLIB_ENCODING_RAW", "string\|false" },` |
|         - |  898 | `	{ "gzencode", "string $data, int $level = -1, int $encoding = ZLIB_ENCODING_GZIP", "string\|false" },` |
|         - |  899 | `	{ "gzeof", "$stream", "bool" },` |
|         - |  900 | `	{ "gzfile", "string $filename, bool $use_include_path = false", "array\|false" },` |
|         - |  901 | `	{ "gzgetc", "$stream", "string\|false" },` |
|         - |  902 | `	{ "gzgets", "$stream, ?int $length = NULL", "string\|false" },` |
|         - |  903 | `	{ "gzinflate", "string $data, int $max_length = 0", "string\|false" },` |
|         - |  904 | `	{ "gzopen", "string $filename, string $mode, bool $use_include_path = false", "" },` |
|         - |  905 | `	{ "gzpassthru", "$stream", "int" },` |
|         - |  906 | `	{ "gzputs", "$stream, string $data, ?int $length = NULL", "int\|false" },` |
|         - |  907 | `	{ "gzread", "$stream, int $length", "string\|false" },` |
|         - |  908 | `	{ "gzrewind", "$stream", "bool" },` |
|         - |  909 | `	{ "gzseek", "$stream, int $offset, int $whence = SEEK_SET", "int" },` |
|         - |  910 | `	{ "gztell", "$stream", "int\|false" },` |
|         - |  911 | `	{ "gzuncompress", "string $data, int $max_length = 0", "string\|false" },` |
|         - |  912 | `	{ "gzwrite", "$stream, string $data, ?int $length = NULL", "int\|false" },` |
|         - |  913 | `	{ "inflate_add", "InflateContext $context, string $data, int $flush_mode = ZLIB_SYNC_FLUSH", "string\|false" },` |
|         - |  914 | `	{ "inflate_get_read_len", "InflateContext $context", "int" },` |
|         - |  915 | `	{ "inflate_get_status", "InflateContext $context", "int" },` |
|         - |  916 | `	{ "inflate_init", "int $encoding, object\|array $options = []", "InflateContext\|false" },` |
|         - |  917 | `	{ "ob_gzhandler", "string $data, int $flags", "string\|false" },` |
|         - |  918 | `	{ "readgzfile", "string $filename, bool $use_include_path = false", "int\|false" },` |
|         - |  919 | `	{ "zlib_decode", "string $data, int $max_length = 0", "string\|false" },` |
|         - |  920 | `	{ "zlib_encode", "string $data, int $encoding, int $level = -1", "string\|false" },` |
|         - |  921 | `	{ "zlib_get_coding_type", "", "string\|false" },` |
|         - |  922 | `	/* ext/gettext */` |
|         - |  923 | `	{ "_", "string $message", "string" },` |
|         - |  924 | `	{ "bind_textdomain_codeset", "string $domain, ?string $codeset = NULL", "string\|false" },` |
|         - |  925 | `	{ "bindtextdomain", "string $domain, ?string $directory = NULL", "string\|false" },` |
|         - |  926 | `	{ "dcgettext", "string $domain, string $message, int $category", "string" },` |
|         - |  927 | `	{ "dcngettext", "string $domain, string $singular, string $plural, int $count, int $category", "string" },` |
|         - |  928 | `	{ "dgettext", "string $domain, string $message", "string" },` |
|         - |  929 | `	{ "dngettext", "string $domain, string $singular, string $plural, int $count", "string" },` |
|         - |  930 | `	{ "gettext", "string $message", "string" },` |
|         - |  931 | `	{ "ngettext", "string $singular, string $plural, int $count", "string" },` |
|         - |  932 | `	{ "textdomain", "?string $domain = NULL", "string" },` |
|         - |  933 | ``	/* ext/standard's syslog trio. All three answer `true` and nothing else --`` |
|         - |  934 | ``	 * php declares the return type as the literal `true`, not `bool`. */`` |
|         - |  935 | `	{ "openlog", "string $prefix, int $flags, int $facility", "true" },` |
|         - |  936 | `	{ "closelog", "", "true" },` |
|         - |  937 | `	{ "syslog", "int $priority, string $message", "true" },` |
|         - |  938 | `	/* ext/pcntl. Only reachable where the extension is built, but the table is` |
|         - |  939 | `	 * a DECLARATION rather than a registration -- Reflection filters it against` |
|         - |  940 | `	 * the live VM, so the rows cost nothing on Windows. */` |
|         - |  941 | `	{ "pcntl_alarm", "int $seconds", "int" },` |
|         - |  942 | `	{ "pcntl_async_signals", "?bool $enable = NULL", "bool" },` |
|         - |  943 | `	{ "pcntl_errno", "", "int" },` |
|         - |  944 | `	{ "pcntl_exec", "string $path, array $args = [], array $env_vars = []", "false" },` |
|         - |  945 | `	{ "pcntl_fork", "", "int" },` |
|         - |  946 | `	{ "pcntl_get_last_error", "", "int" },` |
|         - |  947 | `	{ "pcntl_getcpu", "", "int" },` |
|         - |  948 | ``	/* ext/sockets. `socket_export_stream()` is the one row with no return type`` |
|         - |  949 | `	 * at all: php declares none for it, so Reflection answers null. */` |
|         - |  950 | `	{ "socket_select", "?array &$read, ?array &$write, ?array &$except, ?int $seconds, int $microseconds = 0", "int\|false" },` |
|         - |  951 | `	/* SOMAXCONN, which is 4096 on Linux and 2147483647 on Windows -- so the` |
|         - |  952 | `	 * default is stated by NAME rather than as the number php prints here. */` |
|         - |  953 | `	{ "socket_create_listen", "int $port, int $backlog = SOMAXCONN", "Socket\|false" },` |
|         - |  954 | `	{ "socket_accept", "Socket $socket", "Socket\|false" },` |
|         - |  955 | `	{ "socket_set_nonblock", "Socket $socket", "bool" },` |
|         - |  956 | `	{ "socket_set_block", "Socket $socket", "bool" },` |
|         - |  957 | `	{ "socket_listen", "Socket $socket, int $backlog = 0", "bool" },` |
|         - |  958 | `	{ "socket_close", "Socket $socket", "void" },` |
|         - |  959 | `	{ "socket_write", "Socket $socket, string $data, ?int $length = null", "int\|false" },` |
|         - |  960 | `	{ "socket_read", "Socket $socket, int $length, int $mode = 2", "string\|false" },` |
|         - |  961 | `	{ "socket_getsockname", "Socket $socket, &$address, &$port = null", "bool" },` |
|         - |  962 | `	{ "socket_getpeername", "Socket $socket, &$address, &$port = null", "bool" },` |
|         - |  963 | `	{ "socket_create", "int $domain, int $type, int $protocol", "Socket\|false" },` |
|         - |  964 | `	{ "socket_connect", "Socket $socket, string $address, ?int $port = null", "bool" },` |
|         - |  965 | `	{ "socket_strerror", "int $error_code", "string" },` |
|         - |  966 | `	{ "socket_bind", "Socket $socket, string $address, int $port = 0", "bool" },` |
|         - |  967 | `	{ "socket_recv", "Socket $socket, &$data, int $length, int $flags", "int\|false" },` |
|         - |  968 | `	{ "socket_send", "Socket $socket, string $data, int $length, int $flags", "int\|false" },` |
|         - |  969 | `	{ "socket_recvfrom", "Socket $socket, &$data, int $length, int $flags, &$address, &$port = null", "int\|false" },` |
|         - |  970 | `	{ "socket_sendto", "Socket $socket, string $data, int $length, int $flags, string $address, ?int $port = null", "int\|false" },` |
|         - |  971 | `	{ "socket_get_option", "Socket $socket, int $level, int $option", "array\|int\|false" },` |
|         - |  972 | `	{ "socket_getopt", "Socket $socket, int $level, int $option", "array\|int\|false" },` |
|         - |  973 | `	{ "socket_set_option", "Socket $socket, int $level, int $option, $value", "bool" },` |
|         - |  974 | `	{ "socket_setopt", "Socket $socket, int $level, int $option, $value", "bool" },` |
|         - |  975 | `	{ "socket_create_pair", "int $domain, int $type, int $protocol, &$pair", "bool" },` |
|         - |  976 | `	{ "socket_shutdown", "Socket $socket, int $mode = 2", "bool" },` |
|         - |  977 | `	{ "socket_atmark", "Socket $socket", "bool" },` |
|         - |  978 | `	{ "socket_last_error", "?Socket $socket = null", "int" },` |
|         - |  979 | `	{ "socket_clear_error", "?Socket $socket = null", "void" },` |
|         - |  980 | `	{ "socket_import_stream", "$stream", "Socket\|false" },` |
|         - |  981 | `	{ "socket_export_stream", "Socket $socket", "" },` |
|         - |  982 | `	{ "socket_sendmsg", "Socket $socket, array $message, int $flags = 0", "int\|false" },` |
|         - |  983 | `	{ "socket_recvmsg", "Socket $socket, array &$message, int $flags = 0", "int\|false" },` |
|         - |  984 | `	{ "socket_cmsg_space", "int $level, int $type, int $num = 0", "?int" },` |
|         - |  985 | `	{ "socket_addrinfo_lookup", "string $host, ?string $service = null, array $hints = []", "array\|false" },` |
|         - |  986 | `	{ "socket_addrinfo_connect", "AddressInfo $address", "Socket\|false" },` |
|         - |  987 | `	{ "socket_addrinfo_bind", "AddressInfo $address", "Socket\|false" },` |
|         - |  988 | `	{ "socket_addrinfo_explain", "AddressInfo $address", "array" },` |
|         - |  989 | `	/* Windows only; the rows are harmless on a build that registers no such` |
|         - |  990 | `	 * name, since the stamping pass filters against the live VM. */` |
|         - |  991 | `	{ "socket_wsaprotocol_info_export", "Socket $socket, int $process_id", "string\|false" },` |
|         - |  992 | `	{ "socket_wsaprotocol_info_import", "string $info_id", "Socket\|false" },` |
|         - |  993 | `	{ "socket_wsaprotocol_info_release", "string $info_id", "bool" },` |
|         - |  994 | `	{ "pcntl_getcpuaffinity", "?int $process_id = NULL", "array\|false" },` |
|         - |  995 | `	{ "pcntl_getpriority", "?int $process_id = NULL, int $mode = PRIO_PROCESS", "int\|false" },` |
|         - |  996 | `	{ "pcntl_setcpuaffinity", "?int $process_id = NULL, array $cpu_ids = []", "bool" },` |
|         - |  997 | `	{ "pcntl_setpriority", "int $priority, ?int $process_id = NULL, int $mode = PRIO_PROCESS", "bool" },` |
|         - |  998 | `	{ "pcntl_signal", "int $signal, $handler, bool $restart_syscalls = true", "bool" },` |
|         - |  999 | `	{ "pcntl_signal_dispatch", "", "bool" },` |
|         - | 1000 | `	{ "pcntl_signal_get_handler", "int $signal", "" },` |
|         - | 1001 | `	{ "pcntl_sigprocmask", "int $mode, array $signals, &$old_signals = NULL", "bool" },` |
|         - | 1002 | `	{ "pcntl_sigtimedwait", "array $signals, &$info = [], int $seconds = 0, int $nanoseconds = 0", "int\|false" },` |
|         - | 1003 | `	{ "pcntl_sigwaitinfo", "array $signals, &$info = []", "int\|false" },` |
|         - | 1004 | `	{ "pcntl_strerror", "int $error_code", "string" },` |
|         - | 1005 | `	{ "pcntl_unshare", "int $flags", "bool" },` |
|         - | 1006 | `	{ "pcntl_wait", "&$status, int $flags = 0, &$resource_usage = []", "int" },` |
|         - | 1007 | `	{ "pcntl_waitid", "int $idtype = P_ALL, ?int $id = NULL, &$info = [], int $flags = WEXITED, &$resource_usage = []", "bool" },` |
|         - | 1008 | `	{ "pcntl_waitpid", "int $process_id, &$status, int $flags = 0, &$resource_usage = []", "int" },` |
|         - | 1009 | `	{ "pcntl_wexitstatus", "int $status", "int\|false" },` |
|         - | 1010 | `	{ "pcntl_wifcontinued", "int $status", "bool" },` |
|         - | 1011 | `	{ "pcntl_wifexited", "int $status", "bool" },` |
|         - | 1012 | `	{ "pcntl_wifsignaled", "int $status", "bool" },` |
|         - | 1013 | `	{ "pcntl_wifstopped", "int $status", "bool" },` |
|         - | 1014 | `	{ "pcntl_wstopsig", "int $status", "int\|false" },` |
|         - | 1015 | `	{ "pcntl_wtermsig", "int $status", "int\|false" },` |
|         - | 1016 | `	/* ext/posix */` |
|         - | 1017 | `	{ "posix_access", "string $filename, int $flags = 0", "bool" },` |
|         - | 1018 | `	{ "posix_ctermid", "", "string\|false" },` |
|         - | 1019 | `	{ "posix_eaccess", "string $filename, int $flags = 0", "bool" },` |
|         - | 1020 | `	{ "posix_errno", "", "int" },` |
|         - | 1021 | `	{ "posix_fpathconf", "$file_descriptor, int $name", "int\|false" },` |
|         - | 1022 | `	{ "posix_get_last_error", "", "int" },` |
|         - | 1023 | `	{ "posix_getcwd", "", "string\|false" },` |
|         - | 1024 | `	{ "posix_getegid", "", "int" },` |
|         - | 1025 | `	{ "posix_geteuid", "", "int" },` |
|         - | 1026 | `	{ "posix_getgid", "", "int" },` |
|         - | 1027 | `	{ "posix_getgrgid", "int $group_id", "array\|false" },` |
|         - | 1028 | `	{ "posix_getgrnam", "string $name", "array\|false" },` |
|         - | 1029 | `	{ "posix_getgroups", "", "array\|false" },` |
|         - | 1030 | `	{ "posix_getlogin", "", "string\|false" },` |
|         - | 1031 | `	{ "posix_getpgid", "int $process_id", "int\|false" },` |
|         - | 1032 | `	{ "posix_getpgrp", "", "int" },` |
|         - | 1033 | `	{ "posix_getpid", "", "int" },` |
|         - | 1034 | `	{ "posix_getppid", "", "int" },` |
|         - | 1035 | `	{ "posix_getpwnam", "string $username", "array\|false" },` |
|         - | 1036 | `	{ "posix_getpwuid", "int $user_id", "array\|false" },` |
|         - | 1037 | `	{ "posix_getrlimit", "?int $resource = NULL", "array\|false" },` |
|         - | 1038 | `	{ "posix_getsid", "int $process_id", "int\|false" },` |
|         - | 1039 | `	{ "posix_getuid", "", "int" },` |
|         - | 1040 | `	{ "posix_initgroups", "string $username, int $group_id", "bool" },` |
|         - | 1041 | `	{ "posix_isatty", "$file_descriptor", "bool" },` |
|         - | 1042 | `	{ "posix_kill", "int $process_id, int $signal", "bool" },` |
|         - | 1043 | `	{ "posix_mkfifo", "string $filename, int $permissions", "bool" },` |
|         - | 1044 | `	{ "posix_mknod", "string $filename, int $flags, int $major = 0, int $minor = 0", "bool" },` |
|         - | 1045 | `	{ "posix_pathconf", "string $path, int $name", "int\|false" },` |
|         - | 1046 | `	{ "posix_setegid", "int $group_id", "bool" },` |
|         - | 1047 | `	{ "posix_seteuid", "int $user_id", "bool" },` |
|         - | 1048 | `	{ "posix_setgid", "int $group_id", "bool" },` |
|         - | 1049 | `	{ "posix_setpgid", "int $process_id, int $process_group_id", "bool" },` |
|         - | 1050 | `	{ "posix_setrlimit", "int $resource, int $soft_limit, int $hard_limit", "bool" },` |
|         - | 1051 | `	{ "posix_setsid", "", "int" },` |
|         - | 1052 | `	{ "posix_setuid", "int $user_id", "bool" },` |
|         - | 1053 | `	{ "posix_strerror", "int $error_code", "string" },` |
|         - | 1054 | `	{ "posix_sysconf", "int $conf_id", "int" },` |
|         - | 1055 | `	{ "posix_times", "", "array\|false" },` |
|         - | 1056 | `	{ "posix_ttyname", "$file_descriptor", "string\|false" },` |
|         - | 1057 | `	{ "posix_uname", "", "array\|false" },` |
|         - | 1058 | `	{ "get_class", "object $object = ?", "string" },` |
|         - | 1059 | `	{ "get_class_methods", "object\|string $object_or_class", "array" },` |
|         - | 1060 | `	{ "get_class_vars", "string $class", "array" },` |
|         - | 1061 | `	{ "get_current_user", "", "string" },` |
|         - | 1062 | `	{ "get_declared_classes", "", "array" },` |
|         - | 1063 | `	{ "get_declared_interfaces", "", "array" },` |
|         - | 1064 | `	{ "get_declared_traits", "", "array" },` |
|         - | 1065 | `	{ "get_defined_constants", "bool $categorize = false", "array" },` |
|         - | 1066 | `	{ "get_defined_functions", "bool $exclude_disabled = true", "array" },` |
|         - | 1067 | `	{ "get_defined_vars", "", "array" },` |
|         - | 1068 | `	{ "get_headers", "string $url, bool $associative = false, $context = NULL", "array\|false" },` |
|         - | 1069 | `	{ "get_html_translation_table", "int $table = HTML_SPECIALCHARS, int $flags = ENT_QUOTES \| ENT_SUBSTITUTE \| ENT_HTML401, string $encoding = 'UTF-8'", "array" },` |
|         - | 1070 | `	{ "get_include_path", "", "string\|false" },` |
|         - | 1071 | `	{ "get_included_files", "", "array" },` |
|         - | 1072 | `	{ "get_required_files", "", "array" },` |
|         - | 1073 | `	{ "get_extension_funcs", "string $extension", "array\|false" },` |
|         - | 1074 | `	{ "get_loaded_extensions", "bool $zend_extensions = false", "array" },` |
|         - | 1075 | `	{ "get_mangled_object_vars", "object $object", "array" },` |
|         - | 1076 | `	{ "get_object_vars", "object $object", "array" },` |
|         - | 1077 | `	{ "get_parent_class", "object\|string $object_or_class = ?", "string\|false" },` |
|         - | 1078 | `	{ "get_resource_id", "$resource", "int" },` |
|         - | 1079 | `	{ "get_resource_type", "$resource", "string" },` |
|         - | 1080 | `	{ "getcwd", "", "string\|false" },` |
|         - | 1081 | `	{ "getdate", "?int $timestamp = NULL", "array" },` |
|         - | 1082 | `	{ "getenv", "?string $name = NULL, bool $local_only = false", "array\|string\|false" },` |
|         - | 1083 | `	{ "gethostname", "", "string\|false" },` |
|         - | 1084 | `	{ "getimagesize", "string $filename, &$image_info = NULL", "array\|false" },` |
|         - | 1085 | `	{ "getimagesizefromstring", "string $string, &$image_info = NULL", "array\|false" },` |
|         - | 1086 | `	{ "getmygid", "", "int\|false" },` |
|         - | 1087 | `	{ "getmypid", "", "int\|false" },` |
|         - | 1088 | `	{ "getmyuid", "", "int\|false" },` |
|         - | 1089 | `	{ "getopt", "string $short_options, array $long_options = [], &$rest_index = NULL", "array\|false" },` |
|         - | 1090 | `	{ "getrandmax", "", "int" },` |
|         - | 1091 | `	{ "gettimeofday", "bool $as_float = false", "array\|float" },` |
|         - | 1092 | `	{ "gettype", "mixed $value", "string" },` |
|         - | 1093 | `	{ "get_debug_type", "mixed $value", "string" },` |
|         - | 1094 | `	{ "gmdate", "string $format, ?int $timestamp = NULL", "string" },` |
|         - | 1095 | `	{ "gmmktime", "int $hour, ?int $minute = NULL, ?int $second = NULL, ?int $month = NULL, ?int $day = NULL, ?int $year = NULL", "int\|false" },` |
|         - | 1096 | `	{ "hash", "string $algo, string $data, bool $binary = false, array $options = []", "string" },` |
|         - | 1097 | `	{ "hash_algos", "", "array" },` |
|         - | 1098 | `	{ "hash_equals", "string $known_string, string $user_string", "bool" },` |
|         - | 1099 | `	{ "hash_hmac", "string $algo, string $data, string $key, bool $binary = false", "string" },` |
|         - | 1100 | `	{ "hash_hmac_algos", "", "array" },` |
|         - | 1101 | `	{ "hash_init", "string $algo, int $flags = 0, string $key = \'\', array $options = []", "HashContext" },` |
|         - | 1102 | `	{ "hash_update", "HashContext $context, string $data", "true" },` |
|         - | 1103 | `	{ "hash_final", "HashContext $context, bool $binary = false", "string" },` |
|         - | 1104 | `	{ "hash_copy", "HashContext $context", "HashContext" },` |
|         - | 1105 | `	{ "hash_file", "string $algo, string $filename, bool $binary = false, array $options = []", "string\|false" },` |
|         - | 1106 | `	{ "hash_hkdf", "string $algo, string $key, int $length = 0, string $info = \'\', string $salt = \'\'", "string" },` |
|         - | 1107 | `	{ "hash_pbkdf2", "string $algo, string $password, string $salt, int $iterations, int $length = 0, bool $binary = false, array $options = []", "string" },` |
|         - | 1108 | `	{ "hash_hmac_file", "string $algo, string $filename, string $key, bool $binary = false", "string\|false" },` |
|         - | 1109 | `	{ "hash_update_file", "HashContext $context, string $filename, $stream_context = null", "bool" },` |
|         - | 1110 | `	{ "hash_update_stream", "HashContext $context, $stream, int $length = -1", "int" },` |
|         - | 1111 | `	{ "gregoriantojd", "int $month, int $day, int $year", "int" },` |
|         - | 1112 | `	{ "header", "string $header, bool $replace = true, int $response_code = 0", "void" },` |
|         - | 1113 | `	{ "header_remove", "?string $name = NULL", "void" },` |
|         - | 1114 | `	{ "headers_list", "", "array" },` |
|         - | 1115 | `	{ "headers_sent", "&$filename = NULL, &$line = NULL", "bool" },` |
|         - | 1116 | `	{ "hexdec", "string $hex_string", "int\|float" },` |
|         - | 1117 | `	{ "html_entity_decode", "string $string, int $flags = ENT_QUOTES \| ENT_SUBSTITUTE \| ENT_HTML401, ?string $encoding = NULL", "string" },` |
|         - | 1118 | `	{ "http_build_query", "object\|array $data, string $numeric_prefix = '', ?string $arg_separator = null, int $encoding_type = PHP_QUERY_RFC1738", "string" },` |
|         - | 1119 | `	{ "htmlentities", "string $string, int $flags = ENT_QUOTES \| ENT_SUBSTITUTE \| ENT_HTML401, ?string $encoding = NULL, bool $double_encode = true", "string" },` |
|         - | 1120 | `	{ "htmlspecialchars", "string $string, int $flags = ENT_QUOTES \| ENT_SUBSTITUTE \| ENT_HTML401, ?string $encoding = NULL, bool $double_encode = true", "string" },` |
|         - | 1121 | `	{ "htmlspecialchars_decode", "string $string, int $flags = ENT_QUOTES \| ENT_SUBSTITUTE \| ENT_HTML401", "string" },` |
|         - | 1122 | `	{ "http_clear_last_response_headers", "", "void" },` |
|         - | 1123 | `	{ "http_get_last_response_headers", "", "?array" },` |
|         - | 1124 | `	{ "http_response_code", "int $response_code = 0", "int\|bool" },` |
|         - | 1125 | `	{ "hypot", "float $x, float $y", "float" },` |
|         - | 1126 | `	{ "idate", "string $format, ?int $timestamp = NULL", "int\|false" },` |
|         - | 1127 | `	{ "ignore_user_abort", "?bool $enable = NULL", "int" },` |
|         - | 1128 | `	{ "image_type_to_mime_type", "int $image_type", "string" },` |
|         - | 1129 | `	{ "image_type_to_extension", "int $image_type, bool $include_dot = true", "string\|false" },` |
|         - | 1130 | `	{ "implode", "array\|string $separator, ?array $array = NULL", "string" },` |
|         - | 1131 | `	{ "in_array", "mixed $needle, array $haystack, bool $strict = false", "bool" },` |
|         - | 1132 | `	{ "inet_ntop", "string $ip", "string\|false" },` |
|         - | 1133 | `	{ "inet_pton", "string $ip", "string\|false" },` |
|         - | 1134 | `	{ "intdiv", "int $num1, int $num2", "int" },` |
|         - | 1135 | `	{ "interface_exists", "string $interface, bool $autoload = true", "bool" },` |
|         - | 1136 | `	{ "trait_exists", "string $trait, bool $autoload = true", "bool" },` |
|         - | 1137 | `	{ "intval", "mixed $value, int $base = 10", "int" },` |
|         - | 1138 | `	{ "is_a", "mixed $object_or_class, string $class, bool $allow_string = false", "bool" },` |
|         - | 1139 | `	{ "is_array", "mixed $value", "bool" },` |
|         - | 1140 | `	{ "is_bool", "mixed $value", "bool" },` |
|         - | 1141 | `	{ "is_callable", "mixed $value, bool $syntax_only = false, &$callable_name = NULL", "bool" },` |
|         - | 1142 | `	{ "is_dir", "string $filename", "bool" },` |
|         - | 1143 | `	{ "is_double", "mixed $value", "bool" },` |
|         - | 1144 | `	{ "is_executable", "string $filename", "bool" },` |
|         - | 1145 | `	{ "is_file", "string $filename", "bool" },` |
|         - | 1146 | `	{ "is_float", "mixed $value", "bool" },` |
|         - | 1147 | `	{ "is_int", "mixed $value", "bool" },` |
|         - | 1148 | `	{ "is_integer", "mixed $value", "bool" },` |
|         - | 1149 | `	{ "is_link", "string $filename", "bool" },` |
|         - | 1150 | `	{ "is_long", "mixed $value", "bool" },` |
|         - | 1151 | `	{ "is_null", "mixed $value", "bool" },` |
|         - | 1152 | `	{ "is_numeric", "mixed $value", "bool" },` |
|         - | 1153 | `	{ "is_object", "mixed $value", "bool" },` |
|         - | 1154 | `	{ "is_readable", "string $filename", "bool" },` |
|         - | 1155 | `	{ "is_resource", "mixed $value", "bool" },` |
|         - | 1156 | `	{ "is_scalar", "mixed $value", "bool" },` |
|         - | 1157 | `	{ "is_string", "mixed $value", "bool" },` |
|         - | 1158 | `	{ "is_subclass_of", "mixed $object_or_class, string $class, bool $allow_string = true", "bool" },` |
|         - | 1159 | `	{ "is_writable", "string $filename", "bool" },` |
|         - | 1160 | `	{ "is_writeable", "string $filename", "bool" },` |
|         - | 1161 | `	{ "iterator_apply", "Traversable $iterator, callable $callback, ?array $args = NULL", "int" },` |
|         - | 1162 | `	{ "iterator_count", "Traversable\|array $iterator", "int" },` |
|         - | 1163 | `	{ "iterator_to_array", "Traversable\|array $iterator, bool $preserve_keys = true", "array" },` |
|         - | 1164 | `	{ "jddayofweek", "int $julian_day, int $mode = CAL_DOW_DAYNO", "string\|int" },` |
|         - | 1165 | `	{ "jdmonthname", "int $julian_day, int $mode", "string" },` |
|         - | 1166 | `	{ "jdtofrench", "int $julian_day", "string" },` |
|         - | 1167 | `	{ "jdtogregorian", "int $julian_day", "string" },` |
|         - | 1168 | `	{ "jdtojewish", "int $julian_day, bool $hebrew = false, int $flags = 0", "string" },` |
|         - | 1169 | `	{ "jdtojulian", "int $julian_day", "string" },` |
|         - | 1170 | `	{ "jdtounix", "int $julian_day", "int" },` |
|         - | 1171 | `	{ "jewishtojd", "int $month, int $day, int $year", "int" },` |
|         - | 1172 | `	{ "join", "array\|string $separator, ?array $array = NULL", "string" },` |
|         - | 1173 | `	{ "juliantojd", "int $month, int $day, int $year", "int" },` |
|         - | 1174 | `	{ "json_decode", "string $json, ?bool $associative = NULL, int $depth = 512, int $flags = 0", "mixed" },` |
|         - | 1175 | `	{ "json_encode", "mixed $value, int $flags = 0, int $depth = 512", "string\|false" },` |
|         - | 1176 | `	{ "json_last_error", "", "int" },` |
|         - | 1177 | `	{ "json_last_error_msg", "", "string" },` |
|         - | 1178 | `	{ "json_validate", "string $json, int $depth = 512, int $flags = 0", "bool" },` |
|         - | 1179 | `	{ "key", "object\|array $array", "string\|int\|null" },` |
|         - | 1180 | `	{ "key_exists", "$key, array $array", "bool" },` |
|         - | 1181 | `	{ "krsort", "array &$array, int $flags = SORT_REGULAR", "true" },` |
|         - | 1182 | `	{ "ksort", "array &$array, int $flags = SORT_REGULAR", "true" },` |
|         - | 1183 | `	{ "lcfirst", "string $string", "string" },` |
|         - | 1184 | `	{ "levenshtein", "string $string1, string $string2, int $insertion_cost = 1, int $replacement_cost = 1, int $deletion_cost = 1", "int" },` |
|         - | 1185 | `	{ "link", "string $target, string $link", "bool" },` |
|         - | 1186 | `	{ "localtime", "?int $timestamp = NULL, bool $associative = false", "array" },` |
|         - | 1187 | `	{ "log", "float $num, float $base = M_E", "float" },` |
|         - | 1188 | `	{ "log10", "float $num", "float" },` |
|         - | 1189 | `	{ "log1p", "float $num", "float" },` |
|         - | 1190 | `	{ "lstat", "string $filename", "array\|false" },` |
|         - | 1191 | `	{ "ltrim", "string $string, string $characters = \" \\n\\r\\t\\v\\x00\"", "string" },` |
|         - | 1192 | `	{ "max", "mixed $value, mixed ...$values = ?", "mixed" },` |
|         - | 1193 | `	{ "mb_chr", "int $codepoint, ?string $encoding = NULL", "string\|false" },` |
|         - | 1194 | `	{ "mb_convert_encoding", "array\|string $string, string $to_encoding, array\|string\|null $from_encoding = NULL", "array\|string\|false" },` |
|         - | 1195 | `	{ "mb_ord", "string $string, ?string $encoding = NULL", "int\|false" },` |
|         - | 1196 | `	{ "mb_ltrim", "string $string, ?string $characters = null, ?string $encoding = null", "string" },` |
|         - | 1197 | `	{ "mb_rtrim", "string $string, ?string $characters = null, ?string $encoding = null", "string" },` |
|         - | 1198 | `	{ "mb_lcfirst", "string $string, ?string $encoding = null", "string" },` |
|         - | 1199 | `	{ "mb_strtolower", "string $string, ?string $encoding = NULL", "string" },` |
|         - | 1200 | `	{ "mb_strtoupper", "string $string, ?string $encoding = NULL", "string" },` |
|         - | 1201 | `	{ "mb_trim", "string $string, ?string $characters = null, ?string $encoding = null", "string" },` |
|         - | 1202 | `	{ "mb_ucfirst", "string $string, ?string $encoding = null", "string" },` |
|         - | 1203 | `	{ "iconv", "string $from_encoding, string $to_encoding, string $string", "string\|false" },` |
|         - | 1204 | `	{ "iconv_strlen", "string $string, ?string $encoding = NULL", "int\|false" },` |
|         - | 1205 | `	{ "iconv_substr", "string $string, int $offset, ?int $length = NULL, ?string $encoding = NULL", "string\|false" },` |
|         - | 1206 | `	{ "iconv_strpos", "string $haystack, string $needle, int $offset = 0, ?string $encoding = NULL", "int\|false" },` |
|         - | 1207 | `	{ "iconv_strrpos", "string $haystack, string $needle, ?string $encoding = NULL", "int\|false" },` |
|         - | 1208 | `	{ "iconv_get_encoding", "string $type = \"all\"", "array\|string\|false" },` |
|         - | 1209 | `	{ "iconv_mime_encode", "string $field_name, string $field_value, array $options = []", "string\|false" },` |
|         - | 1210 | `	{ "iconv_mime_decode", "string $string, int $mode = 0, ?string $encoding = NULL", "string\|false" },` |
|         - | 1211 | `	{ "iconv_mime_decode_headers", "string $headers, int $mode = 0, ?string $encoding = NULL", "array\|false" },` |
|         - | 1212 | `	{ "md5", "string $string, bool $binary = false", "string" },` |
|         - | 1213 | `	{ "md5_file", "string $filename, bool $binary = false", "string\|false" },` |
|         - | 1214 | `	{ "metaphone", "string $string, int $max_phonemes = 0", "string" },` |
|         - | 1215 | `	{ "method_exists", "$object_or_class, string $method", "bool" },` |
|         - | 1216 | `	{ "memory_get_peak_usage", "bool $real_usage = false", "int" },` |
|         - | 1217 | `	{ "memory_get_usage", "bool $real_usage = false", "int" },` |
|         - | 1218 | `	{ "microtime", "bool $as_float = false", "string\|float" },` |
|         - | 1219 | `	{ "min", "mixed $value, mixed ...$values = ?", "mixed" },` |
|         - | 1220 | `	{ "mkdir", "string $directory, int $permissions = 0777, bool $recursive = false, $context = NULL", "bool" },` |
|         - | 1221 | `	{ "mktime", "int $hour, ?int $minute = NULL, ?int $second = NULL, ?int $month = NULL, ?int $day = NULL, ?int $year = NULL", "int\|false" },` |
|         - | 1222 | `	{ "mt_getrandmax", "", "int" },` |
|         - | 1223 | `	{ "mt_rand", "int $min = ?, int $max = ?", "int" },` |
|         - | 1224 | `	{ "mt_srand", "?int $seed = NULL, int $mode = MT_RAND_MT19937", "void" },` |
|         - | 1225 | `	{ "natcasesort", "array &$array", "true" },` |
|         - | 1226 | `	{ "natsort", "array &$array", "true" },` |
|         - | 1227 | `	{ "next", "object\|array &$array", "mixed" },` |
|         - | 1228 | `	{ "nl2br", "string $string, bool $use_xhtml = true", "string" },` |
|         - | 1229 | `	{ "number_format", "float $num, int $decimals = 0, ?string $decimal_separator = '.', ?string $thousands_separator = ','", "string" },` |
|         - | 1230 | `	{ "ob_clean", "", "bool" },` |
|         - | 1231 | `	{ "ob_end_clean", "", "bool" },` |
|         - | 1232 | `	{ "ob_end_flush", "", "bool" },` |
|         - | 1233 | `	{ "ob_flush", "", "bool" },` |
|         - | 1234 | `	{ "ob_get_clean", "", "string\|false" },` |
|         - | 1235 | `	{ "ob_get_contents", "", "string\|false" },` |
|         - | 1236 | `	{ "ob_get_flush", "", "string\|false" },` |
|         - | 1237 | `	{ "ob_get_length", "", "int\|false" },` |
|         - | 1238 | `	{ "ob_get_level", "", "int" },` |
|         - | 1239 | `	{ "ob_get_status", "bool $full_status = false", "array" },` |
|         - | 1240 | `	{ "ob_implicit_flush", "bool $enable = true", "void" },` |
|         - | 1241 | `	{ "ob_list_handlers", "", "array" },` |
|         - | 1242 | `	{ "ob_start", "$callback = NULL, int $chunk_size = 0, int $flags = PHP_OUTPUT_HANDLER_STDFLAGS", "bool" },` |
|         - | 1243 | `	{ "octdec", "string $octal_string", "int\|float" },` |
|         - | 1244 | `	{ "opendir", "string $directory, $context = NULL", "" },` |
|         - | 1245 | `	{ "ord", "string $character", "int" },` |
|         - | 1246 | `	{ "pack", "string $format, mixed ...$values = ?", "string" },` |
|         - | 1247 | `	{ "sscanf", "string $string, string $format, mixed &...$vars = ?", "array\|int\|null" },` |
|         - | 1248 | `	{ "fscanf", "$stream, string $format, mixed &...$vars = ?", "array\|int\|false\|null" },` |
|         - | 1249 | `	{ "unpack", "string $format, string $string, int $offset = 0", "array\|false" },` |
|         - | 1250 | `	{ "parse_ini_file", "string $filename, bool $process_sections = false, int $scanner_mode = INI_SCANNER_NORMAL", "array\|false" },` |
|         - | 1251 | `	{ "parse_ini_string", "string $ini_string, bool $process_sections = false, int $scanner_mode = INI_SCANNER_NORMAL", "array\|false" },` |
|         - | 1252 | `	{ "parse_str", "string $string, &$result", "void" },` |
|         - | 1253 | `	{ "parse_url", "string $url, int $component = -1", "array\|string\|int\|false\|null" },` |
|         - | 1254 | `	{ "crypt", "string $string, string $salt", "string" },` |
|         - | 1255 | `	{ "password_algos", "", "array" },` |
|         - | 1256 | `	{ "password_get_info", "string $hash", "array" },` |
|         - | 1257 | `	{ "password_hash", "string $password, string\|int\|null $algo, array $options = []", "string" },` |
|         - | 1258 | `	{ "password_needs_rehash", "string $hash, string\|int\|null $algo, array $options = []", "bool" },` |
|         - | 1259 | `	{ "password_verify", "string $password, string $hash", "bool" },` |
|         - | 1260 | `	{ "passthru", "string $command, &$result_code = NULL", "?false" },` |
|         - | 1261 | `	{ "pathinfo", "string $path, int $flags = PATHINFO_ALL", "array\|string" },` |
|         - | 1262 | `	{ "pclose", "$handle", "int" },` |
|         - | 1263 | `	{ "php_sapi_name", "", "string\|false" },` |
|         - | 1264 | `	{ "php_ini_loaded_file", "", "string\|false" },` |
|         - | 1265 | `	{ "php_ini_scanned_files", "", "string\|false" },` |
|         - | 1266 | `	{ "php_uname", "string $mode = 'a'", "string" },` |
|         - | 1267 | ``	/* php spells this default `INFO_ALL`, and this engine spells the NUMBER: the`` |
|         - | 1268 | `	 * INFO_* family has no consumer here, since phpinfo() ignores $flags and` |
|         - | 1269 | `	 * prints the whole page whatever it is given. The names ship with the` |
|         - | 1270 | `	 * section filter or not at all (§7.3). */` |
|         - | 1271 | `	{ "phpinfo", "int $flags = 4294967295", "true" },` |
|         - | 1272 | `	{ "phpversion", "?string $extension = NULL", "string\|false" },` |
|         - | 1273 | `	{ "pi", "", "float" },` |
|         - | 1274 | `	{ "popen", "string $command, string $mode", "" },` |
|         - | 1275 | `	{ "pos", "object\|array $array", "mixed" },` |
|         - | 1276 | `	{ "pow", "mixed $num, mixed $exponent", "object\|int\|float" },` |
|         - | 1277 | `	{ "preg_last_error", "", "int" },` |
|         - | 1278 | `	{ "preg_last_error_msg", "", "string" },` |
|         - | 1279 | `	{ "fsockopen", "string $hostname, int $port = -1, &$error_code = NULL, &$error_message = NULL, ?float $timeout = NULL", "" },` |
|         - | 1280 | `	{ "pfsockopen", "string $hostname, int $port = -1, &$error_code = NULL, &$error_message = NULL, ?float $timeout = NULL", "" },` |
|         - | 1281 | `	{ "stream_socket_client", "string $address, &$error_code = NULL, &$error_message = NULL, ?float $timeout = NULL, int $flags = STREAM_CLIENT_CONNECT, $context = NULL", "" },` |
|         - | 1282 | `	{ "stream_socket_server", "string $address, &$error_code = NULL, &$error_message = NULL, int $flags = STREAM_SERVER_BIND \| STREAM_SERVER_LISTEN, $context = NULL", "" },` |
|         - | 1283 | `	{ "stream_socket_accept", "$socket, ?float $timeout = NULL, &$peer_name = NULL", "" },` |
|         - | 1284 | `	{ "stream_socket_get_name", "$socket, bool $remote", "string\|false" },` |
|         - | 1285 | `	{ "stream_socket_pair", "int $domain, int $type, int $protocol", "array\|false" },` |
|         - | 1286 | `	{ "stream_isatty", "$stream", "bool" },` |
|         - | 1287 | `	{ "stream_socket_shutdown", "$stream, int $mode", "bool" },` |
|         - | 1288 | `	{ "stream_socket_recvfrom", "$socket, int $length, int $flags = 0, &$address = NULL", "string\|false" },` |
|         - | 1289 | `	{ "stream_socket_sendto", "$socket, string $data, int $flags = 0, string $address = ''", "int\|false" },` |
|         - | 1290 | `	{ "preg_filter", "array\|string $pattern, array\|string $replacement, array\|string $subject, int $limit = -1, &$count = NULL", "array\|string\|null" },` |
|         - | 1291 | `	{ "preg_grep", "string $pattern, array $array, int $flags = 0", "array\|false" },` |
|         - | 1292 | `	{ "preg_match", "string $pattern, string $subject, &$matches = NULL, int $flags = 0, int $offset = 0", "int\|false" },` |
|         - | 1293 | `	{ "preg_match_all", "string $pattern, string $subject, &$matches = NULL, int $flags = 0, int $offset = 0", "int\|false" },` |
|         - | 1294 | `	{ "preg_quote", "string $str, ?string $delimiter = NULL", "string" },` |
|         - | 1295 | `	{ "preg_replace", "array\|string $pattern, array\|string $replacement, array\|string $subject, int $limit = -1, &$count = NULL", "array\|string\|null" },` |
|         - | 1296 | `	{ "preg_replace_callback", "array\|string $pattern, callable $callback, array\|string $subject, int $limit = -1, &$count = NULL, int $flags = 0", "array\|string\|null" },` |
|         - | 1297 | `	{ "preg_replace_callback_array", "array $pattern, array\|string $subject, int $limit = -1, &$count = NULL, int $flags = 0", "array\|string\|null" },` |
|         - | 1298 | `	{ "preg_split", "string $pattern, string $subject, int $limit = -1, int $flags = 0", "array\|false" },` |
|         - | 1299 | `	{ "prev", "object\|array &$array", "mixed" },` |
|         - | 1300 | `	{ "print_r", "mixed $value, bool $return = false", "string\|true" },` |
|         - | 1301 | `	{ "printf", "string $format, mixed ...$values = ?", "int" },` |
|         - | 1302 | `	{ "property_exists", "$object_or_class, string $property", "bool" },` |
|         - | 1303 | `	{ "putenv", "string $assignment", "bool" },` |
|         - | 1304 | `	{ "quoted_printable_decode", "string $string", "string" },` |
|         - | 1305 | `	{ "quoted_printable_encode", "string $string", "string" },` |
|         - | 1306 | `	{ "quotemeta", "string $string", "string" },` |
|         - | 1307 | `	{ "rad2deg", "float $num", "float" },` |
|         - | 1308 | `	{ "rand", "int $min = ?, int $max = ?", "int" },` |
|         - | 1309 | `	{ "random_bytes", "int $length", "string" },` |
|         - | 1310 | `	{ "random_int", "int $min, int $max", "int" },` |
|         - | 1311 | `	{ "range", "string\|int\|float $start, string\|int\|float $end, int\|float $step = 1", "array" },` |
|         - | 1312 | `	{ "rawurldecode", "string $string", "string" },` |
|         - | 1313 | `	{ "rawurlencode", "string $string", "string" },` |
|         - | 1314 | `	{ "readdir", "$dir_handle = NULL", "string\|false" },` |
|         - | 1315 | `	{ "readfile", "string $filename, bool $use_include_path = false, $context = NULL", "int\|false" },` |
|         - | 1316 | `	{ "readlink", "string $path", "string\|false" },` |
|         - | 1317 | `	{ "realpath", "string $path", "string\|false" },` |
|         - | 1318 | `	{ "stream_resolve_include_path", "string $filename", "string\|false" },` |
|         - | 1319 | `	{ "register_shutdown_function", "callable $callback, mixed ...$args = ?", "void" },` |
|         - | 1320 | `	{ "rename", "string $from, string $to, $context = NULL", "bool" },` |
|         - | 1321 | `	{ "reset", "object\|array &$array", "mixed" },` |
|         - | 1322 | `	{ "restore_error_handler", "", "true" },` |
|         - | 1323 | `	{ "restore_exception_handler", "", "true" },` |
|         - | 1324 | `	{ "rewind", "$stream", "bool" },` |
|         - | 1325 | `	{ "rewinddir", "$dir_handle = NULL", "void" },` |
|         - | 1326 | `	{ "rmdir", "string $directory, $context = NULL", "bool" },` |
|         - | 1327 | `	{ "round", "int\|float $num, int $precision = 0, RoundingMode\|int $mode = RoundingMode::HalfAwayFromZero", "float" },` |
|         - | 1328 | `	{ "rsort", "array &$array, int $flags = SORT_REGULAR", "true" },` |
|         - | 1329 | `	{ "rtrim", "string $string, string $characters = \" \\n\\r\\t\\v\\x00\"", "string" },` |
|         - | 1330 | `	{ "serialize", "mixed $value", "string" },` |
|         - | 1331 | `	{ "set_error_handler", "?callable $callback, int $error_levels = E_ALL", "" },` |
|         - | 1332 | `	{ "set_exception_handler", "?callable $callback", "" },` |
|         - | 1333 | `	{ "get_error_handler", "", "?callable" },` |
|         - | 1334 | `	{ "get_exception_handler", "", "?callable" },` |
|         - | 1335 | `	{ "hrtime", "bool $as_number = false", "array\|int\|float\|false" },` |
|         - | 1336 | `	{ "getrusage", "int $mode = 0", "array\|false" },` |
|         - | 1337 | ``	/* php's own row is `int $category, mixed ...$rest` with a MINIMUM of two, so`` |
|         - | 1338 | ``	 * `setlocale(LC_ALL)` is its ArgumentCountError and not a query. */`` |
|         - | 1339 | `	{ "setlocale", "int $category, array\|string $locales, string ...$rest = ?", "string\|false" },` |
|         - | 1340 | `	{ "mb_check_encoding", "array\|string\|null $value = NULL, ?string $encoding = NULL", "bool" },` |
|         - | 1341 | `	{ "mb_convert_case", "string $string, int $mode, ?string $encoding = NULL", "string" },` |
|         - | 1342 | `	{ "mb_detect_encoding", "string $string, array\|string\|null $encodings = NULL, bool $strict = false", "string\|false" },` |
|         - | 1343 | `	{ "mb_internal_encoding", "?string $encoding = NULL", "string\|bool" },` |
|         - | 1344 | `	{ "mb_scrub", "string $string, ?string $encoding = null", "string" },` |
|         - | 1345 | `	{ "mb_substitute_character", "string\|int\|null $substitute_character = null", "string\|int\|bool" },` |
|         - | 1346 | `	{ "mb_str_split", "string $string, int $length = 1, ?string $encoding = NULL", "array" },` |
|         - | 1347 | `	{ "mb_stripos", "string $haystack, string $needle, int $offset = 0, ?string $encoding = NULL", "int\|false" },` |
|         - | 1348 | `	{ "mb_strlen", "string $string, ?string $encoding = NULL", "int" },` |
|         - | 1349 | `	{ "mb_strpos", "string $haystack, string $needle, int $offset = 0, ?string $encoding = NULL", "int\|false" },` |
|         - | 1350 | `	{ "mb_str_pad", "string $string, int $length, string $pad_string = \" \", int $pad_type = STR_PAD_RIGHT, ?string $encoding = null", "string" },` |
|         - | 1351 | `	{ "mb_strcut", "string $string, int $start, ?int $length = null, ?string $encoding = null", "string" },` |
|         - | 1352 | `	{ "mb_strimwidth", "string $string, int $start, int $width, string $trim_marker = \"\", ?string $encoding = null", "string" },` |
|         - | 1353 | `	{ "mb_strrchr", "string $haystack, string $needle, bool $before_needle = false, ?string $encoding = null", "string\|false" },` |
|         - | 1354 | `	{ "mb_strrichr", "string $haystack, string $needle, bool $before_needle = false, ?string $encoding = null", "string\|false" },` |
|         - | 1355 | `	{ "mb_strripos", "string $haystack, string $needle, int $offset = 0, ?string $encoding = null", "int\|false" },` |
|         - | 1356 | `	{ "mb_strrpos", "string $haystack, string $needle, int $offset = 0, ?string $encoding = NULL", "int\|false" },` |
|         - | 1357 | `	{ "mb_stristr", "string $haystack, string $needle, bool $before_needle = false, ?string $encoding = null", "string\|false" },` |
|         - | 1358 | `	{ "mb_strstr", "string $haystack, string $needle, bool $before_needle = false, ?string $encoding = null", "string\|false" },` |
|         - | 1359 | `	{ "mb_substr_count", "string $haystack, string $needle, ?string $encoding = null", "int" },` |
|         - | 1360 | `	{ "mb_strwidth", "string $string, ?string $encoding = NULL", "int" },` |
|         - | 1361 | `	{ "mb_substr", "string $string, int $start, ?int $length = NULL, ?string $encoding = NULL", "string" },` |
|         - | 1362 | `	{ "memory_reset_peak_usage", "", "void" },` |
|         - | 1363 | `	{ "proc_close", "$process", "int" },` |
|         - | 1364 | `	{ "proc_get_status", "$process", "array" },` |
|         - | 1365 | `	{ "proc_nice", "int $priority", "bool" },` |
|         - | 1366 | `	{ "proc_open", "array\|string $command, array $descriptor_spec, &$pipes, ?string $cwd = NULL, ?array $env_vars = NULL, ?array $options = NULL", "" },` |
|         - | 1367 | `	{ "proc_terminate", "$process, int $signal = 15", "bool" },` |
|         - | 1368 | `	{ "set_include_path", "string $include_path", "string\|false" },` |
|         - | 1369 | `	{ "setcookie", "string $name, string $value = '', array\|int $expires_or_options = 0, string $path = '', string $domain = '', bool $secure = false, bool $httponly = false", "bool" },` |
|         - | 1370 | `	{ "setrawcookie", "string $name, string $value = '', array\|int $expires_or_options = 0, string $path = '', string $domain = '', bool $secure = false, bool $httponly = false", "bool" },` |
|         - | 1371 | `	{ "settype", "mixed &$var, string $type", "bool" },` |
|         - | 1372 | `	{ "sha1", "string $string, bool $binary = false", "string" },` |
|         - | 1373 | `	{ "sha1_file", "string $filename, bool $binary = false", "string\|false" },` |
|         - | 1374 | `	{ "shell_exec", "string $command", "string\|false\|null" },` |
|         - | 1375 | `	{ "shuffle", "array &$array", "true" },` |
|         - | 1376 | `	{ "similar_text", "string $string1, string $string2, &$percent = NULL", "int" },` |
|         - | 1377 | `	{ "sin", "float $num", "float" },` |
|         - | 1378 | `	{ "sinh", "float $num", "float" },` |
|         - | 1379 | `	{ "sizeof", "Countable\|array $value, int $mode = COUNT_NORMAL", "int" },` |
|         - | 1380 | `	{ "sleep", "int $seconds", "int" },` |
|         - | 1381 | `	{ "sort", "array &$array, int $flags = SORT_REGULAR", "true" },` |
|         - | 1382 | `	{ "soundex", "string $string", "string" },` |
|         - | 1383 | `	{ "spl_autoload", "string $class, ?string $file_extensions = NULL", "void" },` |
|         - | 1384 | `	{ "spl_autoload_call", "string $class", "void" },` |
|         - | 1385 | `	{ "spl_autoload_extensions", "?string $file_extensions = NULL", "string" },` |
|         - | 1386 | `	{ "spl_autoload_functions", "", "array" },` |
|         - | 1387 | `	{ "spl_autoload_register", "?callable $callback = NULL, bool $throw = true, bool $prepend = false", "bool" },` |
|         - | 1388 | `	{ "spl_autoload_unregister", "callable $callback", "bool" },` |
|         - | 1389 | `	{ "spl_classes", "", "array" },` |
|         - | 1390 | `	{ "spl_object_hash", "object $object", "string" },` |
|         - | 1391 | `	{ "spl_object_id", "object $object", "int" },` |
|         - | 1392 | `	{ "sprintf", "string $format, mixed ...$values = ?", "string" },` |
|         - | 1393 | `	{ "sqrt", "float $num", "float" },` |
|         - | 1394 | `	{ "srand", "?int $seed = NULL, int $mode = MT_RAND_MT19937", "void" },` |
|         - | 1395 | `	{ "stat", "string $filename", "array\|false" },` |
|         - | 1396 | `	{ "str_contains", "string $haystack, string $needle", "bool" },` |
|         - | 1397 | `	{ "str_ends_with", "string $haystack, string $needle", "bool" },` |
|         - | 1398 | `	{ "str_getcsv", "string $string, string $separator = ',', string $enclosure = '\"', string $escape = '\\\\'", "array" },` |
|         - | 1399 | `	{ "str_ireplace", "array\|string $search, array\|string $replace, array\|string $subject, &$count = NULL", "array\|string" },` |
|         - | 1400 | `	{ "str_pad", "string $string, int $length, string $pad_string = ' ', int $pad_type = STR_PAD_RIGHT", "string" },` |
|         - | 1401 | `	{ "str_repeat", "string $string, int $times", "string" },` |
|         - | 1402 | `	{ "str_replace", "array\|string $search, array\|string $replace, array\|string $subject, &$count = NULL", "array\|string" },` |
|         - | 1403 | `	{ "str_rot13", "string $string", "string" },` |
|         - | 1404 | `	{ "str_shuffle", "string $string", "string" },` |
|         - | 1405 | `	{ "str_split", "string $string, int $length = 1", "array" },` |
|         - | 1406 | `	{ "str_starts_with", "string $haystack, string $needle", "bool" },` |
|         - | 1407 | `	{ "str_word_count", "string $string, int $format = 0, ?string $characters = NULL", "array\|int" },` |
|         - | 1408 | `	{ "strcasecmp", "string $string1, string $string2", "int" },` |
|         - | 1409 | `	{ "strchr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|         - | 1410 | `	{ "strcmp", "string $string1, string $string2", "int" },` |
|         - | 1411 | `	{ "strnatcasecmp", "string $string1, string $string2", "int" },` |
|         - | 1412 | `	{ "strnatcmp", "string $string1, string $string2", "int" },` |
|         - | 1413 | `	{ "strcoll", "string $string1, string $string2", "int" },` |
|         - | 1414 | `	{ "strcspn", "string $string, string $characters, int $offset = 0, ?int $length = NULL", "int" },` |
|         - | 1415 | `	{ "stream_context_create", "?array $options = NULL, ?array $params = NULL", "" },` |
|         - | 1416 | `	{ "stream_context_get_options", "$stream_or_context", "array" },` |
|         - | 1417 | ``	/* php's argument #2 is `array\|string $wrapper_or_options` and the array form`` |
|         - | 1418 | `	 * — the two-argument spelling — is DEPRECATED in 8.3; §10 refuses what php` |
|         - | 1419 | `	 * deprecates, so this row declares the string and the whole-array form is` |
|         - | 1420 | `	 * spelled stream_context_set_options(). */` |
|         - | 1421 | `	{ "stream_context_set_option", "$context, string $wrapper_name, string $option_name, mixed $value", "true" },` |
|         - | 1422 | `	{ "stream_context_set_options", "$context, array $options", "true" },` |
|         - | 1423 | `	{ "stream_context_get_params", "$context", "array" },` |
|         - | 1424 | `	{ "stream_context_set_params", "$context, array $params", "true" },` |
|         - | 1425 | `	{ "stream_context_get_default", "?array $options = NULL", "" },` |
|         - | 1426 | `	{ "stream_context_set_default", "array $options", "" },` |
|         - | 1427 | `	{ "stream_get_contents", "$stream, ?int $length = NULL, int $offset = -1", "string\|false" },` |
|         - | 1428 | `	{ "stream_get_line", "$stream, int $length, string $ending = ''", "string\|false" },` |
|         - | 1429 | `	{ "socket_get_status", "$stream", "array" },` |
|         - | 1430 | `	{ "stream_get_meta_data", "$stream", "array" },` |
|         - | 1431 | `	{ "stream_copy_to_stream", "$from, $to, ?int $length = NULL, int $offset = 0", "int\|false" },` |
|         - | 1432 | `	{ "stream_get_transports", "", "array" },` |
|         - | 1433 | `	{ "stream_is_local", "$stream", "bool" },` |
|         - | 1434 | `	{ "stream_select", "?array &$read, ?array &$write, ?array &$except, ?int $seconds, ?int $microseconds = NULL", "int\|false" },` |
|         - | 1435 | `	{ "stream_set_blocking", "$stream, bool $enable", "bool" },` |
|         - | 1436 | `	{ "socket_set_blocking", "$stream, bool $enable", "bool" },` |
|         - | 1437 | `	{ "stream_set_chunk_size", "$stream, int $size", "int" },` |
|         - | 1438 | `	{ "stream_set_read_buffer", "$stream, int $size", "int" },` |
|         - | 1439 | `	{ "stream_set_timeout", "$stream, int $seconds, int $microseconds = 0", "bool" },` |
|         - | 1440 | `	{ "stream_set_write_buffer", "$stream, int $size", "int" },` |
|         - | 1441 | `	{ "set_file_buffer", "$stream, int $size", "int" },` |
|         - | 1442 | `	{ "stream_supports_lock", "$stream", "bool" },` |
|         - | 1443 | `	{ "stream_get_wrappers", "", "array" },` |
|         - | 1444 | `	{ "stream_get_filters", "", "array" },` |
|         - | 1445 | `	{ "stream_filter_append", "$stream, string $filter_name, int $mode = 0, mixed $params = ?", "" },` |
|         - | 1446 | `	{ "stream_filter_prepend", "$stream, string $filter_name, int $mode = 0, mixed $params = ?", "" },` |
|         - | 1447 | `	{ "stream_filter_remove", "$stream_filter", "bool" },` |
|         - | 1448 | `	{ "stream_filter_register", "string $filter_name, string $class", "bool" },` |
|         - | 1449 | `	{ "stream_bucket_make_writeable", "$brigade", "?StreamBucket" },` |
|         - | 1450 | `	{ "stream_bucket_append", "$brigade, StreamBucket $bucket", "void" },` |
|         - | 1451 | `	{ "stream_bucket_prepend", "$brigade, StreamBucket $bucket", "void" },` |
|         - | 1452 | `	{ "stream_bucket_new", "$stream, string $buffer", "StreamBucket" },` |
|         - | 1453 | `	{ "stream_register_wrapper", "string $protocol, string $class, int $flags = 0", "bool" },` |
|         - | 1454 | `	{ "stream_wrapper_register", "string $protocol, string $class, int $flags = 0", "bool" },` |
|         - | 1455 | `	{ "stream_wrapper_unregister", "string $protocol", "bool" },` |
|         - | 1456 | `	{ "stream_wrapper_restore", "string $protocol", "bool" },` |
|         - | 1457 | `	{ "strip_tags", "string $string, array\|string\|null $allowed_tags = NULL", "string" },` |
|         - | 1458 | `	{ "stripcslashes", "string $string", "string" },` |
|         - | 1459 | `	{ "stripos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|         - | 1460 | `	{ "stripslashes", "string $string", "string" },` |
|         - | 1461 | `	{ "stristr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|         - | 1462 | `	{ "strlen", "string $string", "int" },` |
|         - | 1463 | `	{ "strncasecmp", "string $string1, string $string2, int $length", "int" },` |
|         - | 1464 | `	{ "strncmp", "string $string1, string $string2, int $length", "int" },` |
|         - | 1465 | `	{ "strpbrk", "string $string, string $characters", "string\|false" },` |
|         - | 1466 | `	{ "strpos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|         - | 1467 | `	{ "strrchr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|         - | 1468 | `	{ "strrev", "string $string", "string" },` |
|         - | 1469 | `	{ "strripos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|         - | 1470 | `	{ "strrpos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|         - | 1471 | `	{ "strspn", "string $string, string $characters, int $offset = 0, ?int $length = NULL", "int" },` |
|         - | 1472 | `	{ "strstr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|         - | 1473 | `	{ "strtok", "string $string, ?string $token = NULL", "string\|false" },` |
|         - | 1474 | `	{ "strtolower", "string $string", "string" },` |
|         - | 1475 | `	{ "strtotime", "string $datetime, ?int $baseTimestamp = NULL", "int\|false" },` |
|         - | 1476 | `	{ "strtoupper", "string $string", "string" },` |
|         - | 1477 | `	{ "strtr", "string $string, array\|string $from, ?string $to = NULL", "string" },` |
|         - | 1478 | `	{ "strval", "mixed $value", "string" },` |
|         - | 1479 | `	{ "substr", "string $string, int $offset, ?int $length = NULL", "string" },` |
|         - | 1480 | `	{ "substr_compare", "string $haystack, string $needle, int $offset, ?int $length = NULL, bool $case_insensitive = false", "int" },` |
|         - | 1481 | `	{ "substr_count", "string $haystack, string $needle, int $offset = 0, ?int $length = NULL", "int" },` |
|         - | 1482 | `	{ "substr_replace", "array\|string $string, array\|string $replace, array\|int $offset, array\|int\|null $length = NULL", "array\|string" },` |
|         - | 1483 | `	{ "symlink", "string $target, string $link", "bool" },` |
|         - | 1484 | `	{ "sys_get_temp_dir", "", "string" },` |
|         - | 1485 | `	{ "sys_getloadavg", "", "array\|false" },` |
|         - | 1486 | `	{ "system", "string $command, &$result_code = NULL", "string\|false" },` |
|         - | 1487 | `	{ "tan", "float $num", "float" },` |
|         - | 1488 | `	{ "tanh", "float $num", "float" },` |
|         - | 1489 | `	{ "time", "", "int" },` |
|         - | 1490 | `	{ "token_get_all", "string $code, int $flags = 0", "array" },` |
|         - | 1491 | `	{ "php_strip_whitespace", "string $filename", "string" },` |
|         - | 1492 | `	{ "token_name", "int $id", "string" },` |
|         - | 1493 | `	{ "touch", "string $filename, ?int $mtime = NULL, ?int $atime = NULL", "bool" },` |
|         - | 1494 | `	{ "trigger_error", "string $message, int $error_level = E_USER_NOTICE", "true" },` |
|         - | 1495 | `	{ "trim", "string $string, string $characters = \" \\n\\r\\t\\v\\x00\"", "string" },` |
|         - | 1496 | `	{ "uasort", "array &$array, callable $callback", "true" },` |
|         - | 1497 | `	{ "ucfirst", "string $string", "string" },` |
|         - | 1498 | `	{ "ucwords", "string $string, string $separators = \" \\t\\r\\n\\f\\v\"", "string" },` |
|         - | 1499 | `	{ "uksort", "array &$array, callable $callback", "true" },` |
|         - | 1500 | `	{ "umask", "?int $mask = NULL", "int" },` |
|         - | 1501 | `	{ "uniqid", "string $prefix = '', bool $more_entropy = false", "string" },` |
|         - | 1502 | `	{ "unixtojd", "?int $timestamp = NULL", "int\|false" },` |
|         - | 1503 | `	{ "unlink", "string $filename, $context = NULL", "bool" },` |
|         - | 1504 | `	{ "unserialize", "string $data, array $options = []", "mixed" },` |
|         - | 1505 | `	{ "urldecode", "string $string", "string" },` |
|         - | 1506 | `	{ "urlencode", "string $string", "string" },` |
|         - | 1507 | `	{ "user_error", "string $message, int $error_level = E_USER_NOTICE", "true" },` |
|         - | 1508 | `	{ "usleep", "int $microseconds", "void" },` |
|         - | 1509 | `	{ "usort", "array &$array, callable $callback", "true" },` |
|         - | 1510 | `	{ "var_dump", "mixed $value, mixed ...$values = ?", "void" },` |
|         - | 1511 | `	{ "var_export", "mixed $value, bool $return = false", "?string" },` |
|         - | 1512 | `	{ "version_compare", "string $version1, string $version2, ?string $operator = null", "int\|bool" },` |
|         - | 1513 | `	/* Both typed parameters stand aside for the same reason as fprintf's: php` |
|         - | 1514 | `	 * refuses $stream before either of them. */` |
|         - | 1515 | `	{ "vfprintf", "$stream, ~string $format, ~array $values", "int" },` |
|         - | 1516 | `	{ "vprintf", "string $format, array $values", "int" },` |
|         - | 1517 | `	{ "vsprintf", "string $format, array $values", "string" },` |
|         - | 1518 | `	{ "wordwrap", "string $string, int $width = 75, string $break = \"\\n\", bool $cut_long_words = false", "string" },` |
|         - | 1519 | `	{ "zip_close", "$zip", "void" },` |
|         - | 1520 | `	{ "zip_entry_close", "$zip_entry", "bool" },` |
|         - | 1521 | `	{ "zip_entry_compressedsize", "$zip_entry", "int\|false" },` |
|         - | 1522 | `	{ "zip_entry_compressionmethod", "$zip_entry", "string\|false" },` |
|         - | 1523 | `	{ "zip_entry_filesize", "$zip_entry", "int\|false" },` |
|         - | 1524 | `	{ "zip_entry_name", "$zip_entry", "string\|false" },` |
|         - | 1525 | `	{ "zip_entry_open", "$zip_dp, $zip_entry, string $mode = 'rb'", "bool" },` |
|         - | 1526 | `	{ "zip_entry_read", "$zip_entry, int $len = 1024", "string\|false" },` |
|         - | 1527 | `	{ "zip_open", "string $filename", "" },` |
|         - | 1528 | `	{ "zip_read", "$zip", "" },` |
|         - | 1529 | `};` |
|         - | 1530 | `/*` |
|         - | 1531 | ` * Stamp the signature strings onto the registered host functions.` |
|         - | 1532 | ` * Runs once at PH7_VmMakeReady, after the builtins are registered.` |
|         - | 1533 | ` */` |
|         - | 1534 | `/*` |
|         - | 1535 | ` * Derive the minimum arity from a builtin's declared signature: a parameter is` |
|         - | 1536 | ` * optional when it carries a default ("int $length = 76") or is variadic` |
|         - | 1537 | ` * ("...$rest"), so the minimum is the count of the parameters that are neither,` |
|         - | 1538 | ` * and the ZPP wording is "at least" as soon as one optional/variadic exists.` |
|         - | 1539 | ` *` |
|         - | 1540 | ` * This makes aBuiltinSig[] the single source of truth for arity — the curated` |
|         - | 1541 | ` * aBuiltinArity[] table above stays as an OVERRIDE for the handful of builtins` |
|         - | 1542 | ` * php reports differently from their own signature (e.g. strtr(), whose 3-arg` |
|         - | 1543 | ` * form still reports "expects exactly 2", and the array_udiff() family, whose` |
|         - | 1544 | ` * variadic tail hides a second required argument). Verified against php 8.5.7` |
|         - | 1545 | ` * for all 462 signed builtins: 458 derive exactly, 4 are overridden.` |
|         - | 1546 | ` */` |
|         - | 1547 | `/*` |
|         - | 1548 | ` * A DEFAULT can contain the parameter separator: php declares` |
|         - | 1549 | `` * `string $separator = ','` and `string $enclosure = '"'`. Every scan of a`` |
|         - | 1550 | ` * signature therefore has to step over a quoted run, or the comma inside one` |
|         - | 1551 | ` * splits the parameter in two — which is how fgetcsv()/fputcsv()/str_getcsv()` |
|         - | 1552 | ` * came to count SIX parameters and accept a fifth argument php refuses.` |
|         - | 1553 | ` * Answers the position of the closing quote (or of the NUL when the run is` |
|         - | 1554 | ` * unterminated); the caller advances past it.` |
|         - | 1555 | ` */` |
|   1229280 | 1556 | `static const char *VmSigSkipQuoted(const char *zCur)` |
|         5 | 1557 | `{` |
|   1229285 | 1558 | `	char c = zCur[0];` |
|   1229285 | 1559 | `	if( c != '\'' && c != '"' ){` |
|       ! 0 | 1560 | `		return zCur;` |
|         - | 1561 | `	}` |
|   3320391 | 1562 | `	for( zCur++ ; zCur[0] ; zCur++ ){` |
|   3320391 | 1563 | `		if( zCur[0] == '\\' && zCur[1] ){` |
|    431259 | 1564 | `			zCur++;` |
|    431259 | 1565 | `			continue;` |
|         - | 1566 | `		}` |
|   2889137 | 1567 | `		if( zCur[0] == c ){` |
|   1229285 | 1568 | `			break;` |
|         - | 1569 | `		}` |
|    828743 | 1570 | `	}` |
|   1229285 | 1571 | `	return zCur;` |
|    613740 | 1572 | `}` |
|  14991760 | 1573 | `PH7_PRIVATE void VmDeriveArityFromSig(const char *zSig,sxi16 *pnMin,sxu8 *pbAtLeast,sxi16 *pnMax,sxu8 *pbHasMax)` |
|         5 | 1574 | `{` |
|  14991765 | 1575 | `	const char *zCur = zSig;` |
|  14991765 | 1576 | `	int nMin = 0, bAtLeast = 0, bSeen = 0, bOptional = 0;` |
|  14991765 | 1577 | `	int nTotal = 0, bVariadic = 0;` |
| 170970482 | 1578 | `	for(;;){` |
| 352537021 | 1579 | `		if( zCur[0] == '\'' \|\| zCur[0] == '"' ){` |
|    612597 | 1580 | `			bSeen = 1;` |
|    612597 | 1581 | `			zCur = VmSigSkipQuoted(zCur);` |
|    612597 | 1582 | `			if( zCur[0] != '\0' ){` |
|    612597 | 1583 | `				zCur++;` |
|    305846 | 1584 | `			}` |
|    612597 | 1585 | `			continue;` |
|         - | 1586 | `		}` |
| 351924429 | 1587 | `		if( zCur[0] == '\0' \|\| zCur[0] == ',' ){` |
|  23978154 | 1588 | `			if( bSeen ){` |
|  18603062 | 1589 | `				nTotal++;` |
|  18603062 | 1590 | `				if( bOptional ){` |
|   6636002 | 1591 | `					bAtLeast = 1;` |
|   3303200 | 1592 | `				}else{` |
|  11967065 | 1593 | `					nMin++;` |
|         - | 1594 | `				}` |
|   9273513 | 1595 | `			}` |
|  23978154 | 1596 | `			if( zCur[0] == '\0' ){` |
|  14991765 | 1597 | `				break;` |
|         - | 1598 | `			}` |
|   8986394 | 1599 | `			bSeen = bOptional = 0;` |
|   8986394 | 1600 | `			zCur++;` |
|   8986394 | 1601 | `			continue;` |
|         - | 1602 | `		}` |
| 327946280 | 1603 | `		if( zCur[0] != ' ' ){` |
| 288905647 | 1604 | `			bSeen = 1;` |
| 144040825 | 1605 | `		}` |
| 327946280 | 1606 | `		if( zCur[0] == '=' \|\| (zCur[0] == '.' && zCur[1] == '.' && zCur[2] == '.') ){` |
|   6849524 | 1607 | `			bOptional = 1;` |
|   3409785 | 1608 | `		}` |
| 327946280 | 1609 | `		if( zCur[0] == '.' && zCur[1] == '.' && zCur[2] == '.' ){` |
|    374831 | 1610 | `			bVariadic = 1;` |
|    187134 | 1611 | `		}` |
| 327946280 | 1612 | `		zCur++;` |
|         5 | 1613 | `	}` |
|  14991765 | 1614 | `	*pnMin = (sxi16)nMin;` |
|  14991765 | 1615 | `	*pbAtLeast = (sxu8)bAtLeast;` |
|         - | 1616 | `	/* php enforces a MAXIMUM too ("expects at most 1 argument, 2 given"); a` |
|         - | 1617 | `	 * variadic tail means there is none. The parameter COUNT is the maximum,` |
|         - | 1618 | `	 * whether or not the parameters carry defaults. */` |
|  14991765 | 1619 | `	*pnMax = (sxi16)nTotal;` |
|  14991765 | 1620 | `	*pbHasMax = (sxu8)(bVariadic ? 0 : 1);` |
|  14991765 | 1621 | `}` |
|         - | 1622 | `/*` |
|         - | 1623 | ` * Does the declared type list (e.g. "array\|string", "?int", "callable") contain` |
|         - | 1624 | ` * the given token? Compares against each '\|'-separated alternative, ignoring a` |
|         - | 1625 | ` * leading nullable '?'.` |
|         - | 1626 | ` */` |
|    888014 | 1627 | `static int VmSigTypeHas(const char *zType,int nType,const char *zTok)` |
|         5 | 1628 | `{` |
|    888019 | 1629 | `	int nTok = (int)SyStrlen(zTok);` |
|    888019 | 1630 | `	int i = 0;` |
|    888019 | 1631 | `	if( zType[0] == '?' ){` |
|    109235 | 1632 | `		zType++;` |
|    109235 | 1633 | `		nType--;` |
|     54004 | 1634 | `	}` |
|   1673541 | 1635 | `	while( i < nType ){` |
|    843417 | 1636 | `		int j = i;` |
|   5316265 | 1637 | `		while( j < nType && zType[j] != '\|' ){` |
|   4472853 | 1638 | `			j++;` |
|         5 | 1639 | `		}` |
|    843417 | 1640 | `		if( j - i == nTok && SyMemcmp(&zType[i],zTok,(sxu32)nTok) == 0 ){` |
|     57895 | 1641 | `			return 1;` |
|         - | 1642 | `		}` |
|    785527 | 1643 | `		i = j + 1;` |
|         5 | 1644 | `	}` |
|    830129 | 1645 | `	return 0;` |
|    438148 | 1646 | `}` |
|         - | 1647 | `/*` |
|         - | 1648 | `` * Is EVERY arm of the declared type list `array` (a bare `array`, or `?array`,`` |
|         - | 1649 | `` * or the `array\|null` union that spells the same thing)? Such a parameter has`` |
|         - | 1650 | ` * no arm a scalar can satisfy, and php refuses one outright.` |
|         - | 1651 | ` *` |
|         - | 1652 | `` * The screen used to exempt any type list carrying an `array` arm, union or`` |
|         - | 1653 | `` * not, for a wording reason: php's `array\|object` parameters come from ONE ZPP`` |
|         - | 1654 | ` * macro (Z_PARAM_ARRAY_OR_OBJECT) that names only "array" in the refusal, so` |
|         - | 1655 | ` * the declared type is not the text php prints. That ambiguity does not exist` |
|         - | 1656 | `` * for a parameter typed exactly `array` -- there is one arm and php prints it.`` |
|         - | 1657 | ` */` |
|     64364 | 1658 | `static int VmSigTypeIsArrayOnly(const char *zType,int nType)` |
|         5 | 1659 | `{` |
|     64369 | 1660 | `	int i = 0, bArray = 0;` |
|     64369 | 1661 | `	if( zType[0] == '?' ){` |
|      8403 | 1662 | `		zType++;` |
|      8403 | 1663 | `		nType--;` |
|      4152 | 1664 | `	}` |
|     69807 | 1665 | `	while( i < nType ){` |
|     59009 | 1666 | `		int j = i;` |
|    370857 | 1667 | `		while( j < nType && zType[j] != '\|' ){` |
|    311853 | 1668 | `			j++;` |
|         5 | 1669 | `		}` |
|     59009 | 1670 | `		if( j > i ){` |
|     59004 | 1671 | `			if( j - i == (int)sizeof("array")-1` |
|     34405 | 1672 | `			 && SyMemcmp(&zType[i],"array",sizeof("array")-1) == 0 ){` |
|      5443 | 1673 | `				bArray = 1;` |
|     58089 | 1674 | `			}else if( !(j - i == (int)sizeof("null")-1` |
|     28280 | 1675 | `			         && SyMemcmp(&zType[i],"null",sizeof("null")-1) == 0) ){` |
|     53571 | 1676 | `				return 0;` |
|         - | 1677 | `			}` |
|      2664 | 1678 | `		}` |
|      5443 | 1679 | `		i = j + 1;` |
|         5 | 1680 | `	}` |
|     10803 | 1681 | `	return bArray;` |
|     31761 | 1682 | `}` |
|         - | 1683 | `/*` |
|         - | 1684 | ` * Does the declared type list name a CLASS (anything that is not one of php's` |
|         - | 1685 | ` * builtin type keywords)? A class-typed parameter accepts an object, so it must` |
|         - | 1686 | ` * not be rejected by the array/object/resource screen below.` |
|         - | 1687 | ` */` |
|         - | 1688 | `/* Is this one arm of a declared type a BUILTIN type name rather than a class? */` |
|    101163 | 1689 | `static int VmSigArmIsBuiltinType(const char *zArm,int nArm)` |
|         5 | 1690 | `{` |
|         - | 1691 | `	static const char *azBuiltin[] = {` |
|         - | 1692 | `		"int","float","string","bool","array","object","callable","iterable",` |
|         - | 1693 | `		"mixed","null","void","resource","false","true","never","self","static"` |
|         - | 1694 | `	};` |
|         - | 1695 | `	int k;` |
|   1007487 | 1696 | `	for( k = 0 ; k < (int)SX_ARRAYSIZE(azBuiltin) ; ++k ){` |
|    962136 | 1697 | `		int nB = (int)SyStrlen(azBuiltin[k]);` |
|    962136 | 1698 | `		if( nArm == nB && SyMemcmp(zArm,azBuiltin[k],(sxu32)nB) == 0 ){` |
|     55817 | 1699 | `			return 1;` |
|         - | 1700 | `		}` |
|    452063 | 1701 | `	}` |
|     45356 | 1702 | `	return 0;` |
|     50169 | 1703 | `}` |
|     64366 | 1704 | `static int VmSigTypeHasClass(const char *zType,int nType)` |
|         5 | 1705 | `{` |
|     64371 | 1706 | `	int i = 0;` |
|     64371 | 1707 | `	if( zType[0] == '?' ){` |
|      8403 | 1708 | `		zType++;` |
|      8403 | 1709 | `		nType--;` |
|      4152 | 1710 | `	}` |
|    120173 | 1711 | `	while( i < nType ){` |
|     60633 | 1712 | `		int j = i;` |
|    379765 | 1713 | `		while( j < nType && zType[j] != '\|' ){` |
|    319137 | 1714 | `			j++;` |
|         5 | 1715 | `		}` |
|     60633 | 1716 | `		if( j > i && !VmSigArmIsBuiltinType(&zType[i],j - i) ){` |
|      4831 | 1717 | `			return 1;` |
|         - | 1718 | `		}` |
|     55807 | 1719 | `		i = j + 1;` |
|         5 | 1720 | `	}` |
|     59545 | 1721 | `	return 0;` |
|     31762 | 1722 | `}` |
|         - | 1723 | `/*` |
|         - | 1724 | ` * php's ", X given" tail: like ph7_type_name() but an object reports its CLASS,` |
|         - | 1725 | ` * which is what php prints in a TypeError.` |
|         - | 1726 | ` */` |
|         - | 1727 | `/*` |
|         - | 1728 | ` * Does pObj satisfy any CLASS arm of a declared type?` |
|         - | 1729 | ` *` |
|         - | 1730 | ` * Answers TRUE (unscreened) when an arm names something this VM has not declared:` |
|         - | 1731 | ` * the signatures describe php's surface, parts of which PHL models differently` |
|         - | 1732 | ` * (the resource-backed handles the RES branch below already excuses), and a name` |
|         - | 1733 | ` * that resolves to nothing must not turn into a rejection of a valid argument.` |
|         - | 1734 | ` */` |
|     40523 | 1735 | `static int VmSigObjSatisfiesClass(ph7_vm *pVm,const char *zType,int nType,` |
|         - | 1736 | `	ph7_class_instance *pObj)` |
|         5 | 1737 | `{` |
|     40528 | 1738 | `	int i = 0;` |
|     40528 | 1739 | `	if( pObj == 0 \|\| pObj->pClass == 0 ){` |
|       ! 0 | 1740 | `		return 1;` |
|         - | 1741 | `	}` |
|     40528 | 1742 | `	if( zType[0] == '?' ){` |
|      1923 | 1743 | `		zType++;` |
|      1923 | 1744 | `		nType--;` |
|       959 | 1745 | `	}` |
|     40598 | 1746 | `	while( i < nType ){` |
|     40540 | 1747 | `		int j = i;` |
|    481269 | 1748 | `		while( j < nType && zType[j] != '\|' ){` |
|    440734 | 1749 | `			j++;` |
|         5 | 1750 | `		}` |
|     40540 | 1751 | `		if( j > i && !VmSigArmIsBuiltinType(&zType[i],j - i) ){` |
|     40530 | 1752 | `			ph7_class *pClass = PH7_VmExtractClass(&(*pVm),&zType[i],(sxu32)(j - i),FALSE,0);` |
|     40530 | 1753 | `			if( pClass == 0 ){` |
|         - | 1754 | `				/* Either a builtin type name (already excluded by the caller) or a` |
|         - | 1755 | `				 * class this build does not declare: nothing to judge. */` |
|       ! 0 | 1756 | `				return 1;` |
|         - | 1757 | `			}` |
|     40530 | 1758 | `			if( PH7_VmInstanceOf(pObj->pClass,pClass) ){` |
|     40470 | 1759 | `				return 1;` |
|         - | 1760 | `			}` |
|        30 | 1761 | `		}` |
|        73 | 1762 | `		i = j + 1;` |
|         3 | 1763 | `	}` |
|        61 | 1764 | `	return 0;` |
|     20270 | 1765 | `}` |
|       148 | 1766 | `static const char * VmArgTypeName(ph7_value *pVal)` |
|         5 | 1767 | `{` |
|       153 | 1768 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       153 | 1769 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       153 | 1770 | `		if( pInst && pInst->pClass ){` |
|       153 | 1771 | `			return pInst->pClass->sName.zString;` |
|         - | 1772 | `		}` |
|       ! 0 | 1773 | `	}` |
|       ! 0 | 1774 | `	return ph7_type_name(pVal);` |
|        79 | 1775 | `}` |
|         - | 1776 | `/*` |
|         - | 1777 | `` * Does pArg satisfy a `string` parameter? The rule VmEnforceBuiltinArgTypes()`` |
|         - | 1778 | ` * below applies, factored out so a builtin that words its own overload dispatch` |
|         - | 1779 | ` * (strtr(), whose expected type depends on the ARITY and so cannot be spelled in` |
|         - | 1780 | ` * one signature) decides identically instead of forking the logic. An array never` |
|         - | 1781 | ` * satisfies one; an object does only through __toString(); a resource does not;` |
|         - | 1782 | ` * null does under php, with a deprecation, but not under PHL's §10 null-strictness` |
|         - | 1783 | ` * policy — the screen and this helper both report it as a mismatch.` |
|         - | 1784 | ` */` |
|     51467 | 1785 | `PH7_PRIVATE int PH7_ArgSatisfiesString(ph7_value *pArg)` |
|         5 | 1786 | `{` |
|     51472 | 1787 | `	if( (pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_NULL\|MEMOBJ_RES)) != 0 ){` |
|        37 | 1788 | `		return 0;` |
|         - | 1789 | `	}` |
|     51438 | 1790 | `	if( (pArg->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       146 | 1791 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|       146 | 1792 | `		return pInst && PH7_ClassExtractMethod(pInst->pClass,"__toString",` |
|        71 | 1793 | `			sizeof("__toString")-1) != 0;` |
|         - | 1794 | `	}` |
|     51296 | 1795 | `	return 1;` |
|     25722 | 1796 | `}` |
|         - | 1797 | `/*` |
|         - | 1798 | `` * Is the declared type exactly `int` — the only shape whose float argument the`` |
|         - | 1799 | `` * screen below can decide? A union with a `float`, `string` or `bool` arm has its`` |
|         - | 1800 | ` * own coercion rules per arm (and php words those refusals from the builtin), so` |
|         - | 1801 | ` * only the plain form and its nullable spelling qualify.` |
|         - | 1802 | ` */` |
|     64364 | 1803 | `static int VmSigTypeIsIntOnly(const char *zType,int nType)` |
|         5 | 1804 | `{` |
|     64369 | 1805 | `	if( nType > 0 && zType[0] == '?' ){` |
|      8403 | 1806 | `		zType++;` |
|      8403 | 1807 | `		nType--;` |
|      4152 | 1808 | `	}` |
|     64369 | 1809 | `	if( nType == (int)sizeof("int")-1 && SyMemcmp(zType,"int",3) == 0 ){` |
|     14445 | 1810 | `		return 1;` |
|         - | 1811 | `	}` |
|         - | 1812 | ``	/* `int\|null` / `null\|int`, the union spelling of `?int`. */`` |
|     50341 | 1813 | `	return VmSigTypeHas(zType,nType,"int") && VmSigTypeHas(zType,nType,"null")` |
|       412 | 1814 | `	    && !VmSigTypeHas(zType,nType,"float")` |
|       132 | 1815 | `	    && !VmSigTypeHas(zType,nType,"string")` |
|        38 | 1816 | `	    && !VmSigTypeHas(zType,nType,"bool")` |
|         4 | 1817 | `	    && !VmSigTypeHas(zType,nType,"array")` |
|         2 | 1818 | `	    && !VmSigTypeHas(zType,nType,"object")` |
|       ! 0 | 1819 | `	    && !VmSigTypeHas(zType,nType,"iterable")` |
|       ! 0 | 1820 | `	    && !VmSigTypeHas(zType,nType,"callable")` |
|     50244 | 1821 | `	    && !VmSigTypeHasClass(zType,nType);` |
|     31761 | 1822 | `}` |
|         - | 1823 | `/*` |
|         - | 1824 | `` * Can this float reach an `int` parameter without losing anything? php's rule is`` |
|         - | 1825 | ` * php_parse_arg_long's: in range, and integral. NaN and the infinities are out by` |
|         - | 1826 | ` * the range test (a NaN compares false against both bounds, which is why the test` |
|         - | 1827 | ` * is written as a pair of accepts rather than a pair of rejects).` |
|         - | 1828 | ` */` |
|       106 | 1829 | `static int VmDoubleFitsInt(double d)` |
|         5 | 1830 | `{` |
|       111 | 1831 | `	if( !PH7_RealFitsInt64(d) ){` |
|        51 | 1832 | `		return 0;` |
|         - | 1833 | `	}` |
|        63 | 1834 | `	return d == (double)(sxi64)d;` |
|        58 | 1835 | `}` |
|         - | 1836 | `/*` |
|         - | 1837 | ` * The same question for a NUMERIC string, which php asks with the same answer:` |
|         - | 1838 | `` * `dechex("1e19")` and `dechex("99999999999999999999")` are both`` |
|         - | 1839 | `` * `must be of type int, string given`. RangeStrToNumber is php's`` |
|         - | 1840 | ` * is_numeric_string grammar and already reclassifies an integer too wide for an` |
|         - | 1841 | ` * sxi64 as a DOUBLE, so the two shapes converge on one test.` |
|         - | 1842 | ` */` |
|        80 | 1843 | `static int VmNumStrFitsInt(ph7_value *pArg)` |
|         5 | 1844 | `{` |
|         - | 1845 | `	const char *zStr;` |
|        85 | 1846 | `	int nLen = 0;` |
|        85 | 1847 | `	sxi64 iVal = 0;` |
|        85 | 1848 | `	double dVal = 0;` |
|        85 | 1849 | `	zStr = ph7_value_to_string(pArg,&nLen);` |
|        85 | 1850 | `	switch( RangeStrToNumber(zStr,(sxu32)nLen,&iVal,&dVal) ){` |
|        60 | 1851 | `	case RANGE_IN_LONG:   return 1;` |
|        27 | 1852 | `	case RANGE_IN_DOUBLE: return VmDoubleFitsInt(dVal);` |
|       ! 0 | 1853 | `	default:              return 0;` |
|         - | 1854 | `	}` |
|        45 | 1855 | `}` |
|         - | 1856 | `/*` |
|         - | 1857 | ` * PHP-8 PATH parameters: which positions carry a filesystem path, a shell` |
|         - | 1858 | ` * command or an include-path list rather than an ordinary string.` |
|         - | 1859 | ` *` |
|         - | 1860 | ` * php spells this in the ZPP macro, not in the declared type: a path parameter` |
|         - | 1861 | `` * is `Z_PARAM_PATH` where an ordinary one is `Z_PARAM_STR`, and both print as`` |
|         - | 1862 | `` * `string` in the stub Reflection reads. The difference is a single rule — a`` |
|         - | 1863 | ` * path may not contain a NUL byte — and php raises a catchable ValueError for` |
|         - | 1864 | ` * one that does, BEFORE the call reaches the filesystem.` |
|         - | 1865 | ` *` |
|         - | 1866 | ` * PHL had no such notion, so every one of these arguments went to the C API as` |
|         - | 1867 | ` * a NUL-terminated string and was silently TRUNCATED at the NUL. That is not a` |
|         - | 1868 | ` * missing diagnostic: the truncated path is a DIFFERENT path, and the builtin` |
|         - | 1869 | `` * then operated on it. `unlink("$dir/x\0.png")` deleted `$dir/x`,`` |
|         - | 1870 | `` * `file_put_contents("$dir/x\0.txt",$d)` wrote it, `touch`/`chmod`/`copy`/`` |
|         - | 1871 | ``  * `rename`/`symlink`/`mkdir` all acted on the prefix, `glob` and `realpath` `` |
|         - | 1872 | `` * answered for it, and `shell_exec("cmd\0; rm -rf /")` ran the prefix as a`` |
|         - | 1873 | ` * command. It is the classic poison-NUL-byte shape php closed engine-wide: a` |
|         - | 1874 | ` * script that concatenates request input into a filename gets a truncation` |
|         - | 1875 | ` * where php gets a refusal, and the extension check the suffix was there to` |
|         - | 1876 | ` * perform never runs.` |
|         - | 1877 | ` *` |
|         - | 1878 | ` * The mask is positional (bit N => parameter N is a path), which is how php` |
|         - | 1879 | ` * carries it too. Only functions PHL actually registers are listed; each row's` |
|         - | 1880 | ` * positions were verified against php 8.5 argument by argument (the answer is` |
|         - | 1881 | `` * NOT derivable from the parameter name — preg_match's `$pattern` is an`` |
|         - | 1882 | ``  * ordinary string, glob's is a path — nor from the type, which is `string` `` |
|         - | 1883 | ` * for both).` |
|         - | 1884 | ` *` |
|         - | 1885 | ` * What is deliberately NOT here: the stat family (file_exists, is_dir, stat,` |
|         - | 1886 | ` * filesize, fileperms, …), which php parses with Z_PARAM_STR and answers` |
|         - | 1887 | `` * `false` for in silence, and the pure PATH-STRING functions (basename,`` |
|         - | 1888 | ` * dirname, pathinfo), which php lets the NUL through untouched because they` |
|         - | 1889 | ` * never touch the filesystem. Both are php-exact here already.` |
|         - | 1890 | ` */` |
|     22462 | 1891 | `static sxu32 VmBuiltinPathMask(SyString *pName)` |
|         5 | 1892 | `{` |
|         - | 1893 | `	static const struct {` |
|         - | 1894 | `		const char *zName;` |
|         - | 1895 | `		sxu32 nByte;` |
|         - | 1896 | `		sxu32 mask;` |
|         - | 1897 | `	} aPath[] = {` |
|         - | 1898 | `		/* Open / read / write */` |
|         - | 1899 | `		{ "fopen",             5, 1u<<0 },` |
|         - | 1900 | `		{ "file_get_contents", 17, 1u<<0 },` |
|         - | 1901 | `		{ "file_put_contents", 17, 1u<<0 },` |
|         - | 1902 | `		{ "file",              4, 1u<<0 },` |
|         - | 1903 | `		{ "readfile",          8, 1u<<0 },` |
|         - | 1904 | `		{ "parse_ini_file",   14, 1u<<0 },` |
|         - | 1905 | `		{ "md5_file",          8, 1u<<0 },` |
|         - | 1906 | `		{ "getimagesize",     12, 1u<<0 },` |
|         - | 1907 | `		{ "sha1_file",         9, 1u<<0 },` |
|         - | 1908 | `		{ "hash_file",         9, 1u<<1 },` |
|         - | 1909 | `		{ "hash_hmac_file",   14, 1u<<1 },` |
|         - | 1910 | `		{ "hash_update_file", 16, 1u<<1 },` |
|         - | 1911 | `		/* Metadata / mutation */` |
|         - | 1912 | `		{ "unlink",            6, 1u<<0 },` |
|         - | 1913 | `		{ "touch",             5, 1u<<0 },` |
|         - | 1914 | `		{ "chmod",             5, 1u<<0 },` |
|         - | 1915 | `		{ "chgrp",             5, 1u<<0 },` |
|         - | 1916 | `		{ "chown",             5, 1u<<0 },` |
|         - | 1917 | `		{ "rename",            6, (1u<<0)\|(1u<<1) },` |
|         - | 1918 | `		{ "copy",              4, (1u<<0)\|(1u<<1) },` |
|         - | 1919 | `		{ "link",              4, (1u<<0)\|(1u<<1) },` |
|         - | 1920 | `		{ "symlink",           7, (1u<<0)\|(1u<<1) },` |
|         - | 1921 | `		{ "readlink",          8, 1u<<0 },` |
|         - | 1922 | `		{ "realpath",          8, 1u<<0 },` |
|         - | 1923 | `		{ "stream_resolve_include_path", 27, 1u<<0 },` |
|         - | 1924 | `		/* Not a path at all: php reads inet_pton()'s $ip with the same` |
|         - | 1925 | `		 * NUL-refusing macro, and answers the same ValueError for a name` |
|         - | 1926 | `		 * that carries one. */` |
|         - | 1927 | `		{ "inet_pton",         9, 1u<<0 },` |
|         - | 1928 | `		/* Directories */` |
|         - | 1929 | `		{ "mkdir",             5, 1u<<0 },` |
|         - | 1930 | `		{ "rmdir",             5, 1u<<0 },` |
|         - | 1931 | `		{ "opendir",           7, 1u<<0 },` |
|         - | 1932 | `		{ "dir",               3, 1u<<0 },` |
|         - | 1933 | `		{ "scandir",           7, 1u<<0 },` |
|         - | 1934 | `		{ "chdir",             5, 1u<<0 },` |
|         - | 1935 | `		{ "chroot",            6, 1u<<0 },` |
|         - | 1936 | `		{ "glob",              4, 1u<<0 },` |
|         - | 1937 | `		{ "tempnam",           7, (1u<<0)\|(1u<<1) },` |
|         - | 1938 | `		{ "disk_free_space",  15, 1u<<0 },` |
|         - | 1939 | `		{ "disk_total_space", 16, 1u<<0 },` |
|         - | 1940 | `		{ "diskfreespace",    13, 1u<<0 },` |
|         - | 1941 | `		/* Not paths at all, and php screens them exactly as if they were: the` |
|         - | 1942 | `		 * datetime a FORMAT is read against is Z_PARAM_PATH_STR at every door` |
|         - | 1943 | `		 * that takes one, so a NUL inside it is the same catchable ValueError.` |
|         - | 1944 | ``		 * Only these five; `new DateTime($s)`, `date_create()`, `modify()` and`` |
|         - | 1945 | ``		 * `date_parse()` take an ordinary string there and read up to the NUL. */`` |
|         - | 1946 | `		{ "date_parse_from_format",                22, 1u<<1 },` |
|         - | 1947 | `		{ "date_create_from_format",               23, 1u<<1 },` |
|         - | 1948 | `		{ "date_create_immutable_from_format",     33, 1u<<1 },` |
|         - | 1949 | `		{ "DateTime::createFromFormat",            26, 1u<<1 },` |
|         - | 1950 | `		{ "DateTimeImmutable::createFromFormat",   35, 1u<<1 },` |
|         - | 1951 | ``		/* ext/sqlite3's two doors onto a database FILE. ext/pdo's `sqlite:` DSN is`` |
|         - | 1952 | `		 * not one of them: php parses a DSN before any of it becomes a path, and` |
|         - | 1953 | `		 * reads it up to the NUL. */` |
|         - | 1954 | `		{ "SQLite3::__construct",                  20, 1u<<0 },` |
|         - | 1955 | `		{ "SQLite3::open",                         13, 1u<<0 },` |
|         - | 1956 | `		/* Not a path either, and php screens it exactly as if it were: BOTH of` |
|         - | 1957 | `		 * bindtextdomain's arguments are Z_PARAM_PATH, so a NUL in the DOMAIN is` |
|         - | 1958 | `		 * the same catchable ValueError the directory gets. The other nine` |
|         - | 1959 | `		 * gettext doors take ordinary strings and read up to the NUL. */` |
|         - | 1960 | `		{ "bindtextdomain",   14, (1u<<0)\|(1u<<1) },` |
|         - | 1961 | `		/* ext/posix's five path doors, which php reads with the same macro. */` |
|         - | 1962 | `		{ "posix_access",     12, 1u<<0 },` |
|         - | 1963 | `		{ "posix_eaccess",    13, 1u<<0 },` |
|         - | 1964 | `		{ "posix_mkfifo",     12, 1u<<0 },` |
|         - | 1965 | `		{ "posix_mknod",      11, 1u<<0 },` |
|         - | 1966 | `		{ "posix_pathconf",   14, 1u<<0 },` |
|         - | 1967 | `		/* ext/fileinfo: the name a type is asked about, and the database the` |
|         - | 1968 | `		 * two openers name, are php Z_PARAM_PATH arguments -- so a NUL in one` |
|         - | 1969 | `		 * is the catchable ValueError rather than a truncated read. */` |
|         - | 1970 | `		{ "finfo_file",            10, 1u<<1 },` |
|         - | 1971 | `		{ "finfo_open",            10, 1u<<1 },` |
|         - | 1972 | `		{ "finfo::file",           11, 1u<<0 },` |
|         - | 1973 | `		{ "finfo::__construct",    18, 1u<<1 },` |
|         - | 1974 | `		{ "mime_content_type",     17, 1u<<0 },` |
|         - | 1975 | `		/* ext/simplexml's file door takes a php path argument too. */` |
|         - | 1976 | `		{ "simplexml_load_file",   19, 1u<<0 },` |
|         - | 1977 | `		/* Path-shaped settings and the pattern matcher */` |
|         - | 1978 | `		{ "fnmatch",           7, (1u<<0)\|(1u<<1) },` |
|         - | 1979 | `		{ "set_include_path", 16, 1u<<0 },` |
|         - | 1980 | `		{ "session_save_path", 17, 1u<<0 },` |
|         - | 1981 | `		{ "error_log",         9, 1u<<2 },` |
|         - | 1982 | `		/* Commands handed to the shell — and the two escapers, which php screens` |
|         - | 1983 | `		 * the same way even though neither of them runs anything: a NUL in what a` |
|         - | 1984 | `		 * script is about to hand a shell is refused where it is WRITTEN. */` |
|         - | 1985 | `		{ "shell_exec",       10, 1u<<0 },` |
|         - | 1986 | `		{ "popen",             5, 1u<<0 },` |
|         - | 1987 | `		{ "escapeshellarg",   14, 1u<<0 },` |
|         - | 1988 | `		{ "escapeshellcmd",   14, 1u<<0 },` |
|         - | 1989 | `		{ "exec",              4, 1u<<0 },` |
|         - | 1990 | `		{ "system",            6, 1u<<0 },` |
|         - | 1991 | `		{ "passthru",          8, 1u<<0 },` |
|         - | 1992 | `		/* The SPL path constructors, which php screens identically and reports` |
|         - | 1993 | ``		 * under their QUALIFIED name (`SplFileInfo::__construct(): Argument #1`` |
|         - | 1994 | ``		 * ($filename) …`). They are native methods, so their signature reaches this`` |
|         - | 1995 | `		 * screen the same way a builtin's does. */` |
|         - | 1996 | `		{ "SplFileInfo::__construct",                24, 1u<<0 },` |
|         - | 1997 | `		{ "DirectoryIterator::__construct",          30, 1u<<0 },` |
|         - | 1998 | `		{ "FilesystemIterator::__construct",         31, 1u<<0 },` |
|         - | 1999 | `		{ "RecursiveDirectoryIterator::__construct", 39, 1u<<0 },` |
|         - | 2000 | `		{ "GlobIterator::__construct",                25, 1u<<0 },` |
|         - | 2001 | `	};` |
|         - | 2002 | `	sxu32 i;` |
|     22467 | 2003 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|       ! 0 | 2004 | `		return 0;` |
|         - | 2005 | `	}` |
|   1562532 | 2006 | `	for( i = 0 ; i < SX_ARRAYSIZE(aPath) ; ++i ){` |
|   1541610 | 2007 | `		if( pName->nByte == aPath[i].nByte` |
|    795784 | 2008 | `		 && SyStrnicmp(pName->zString,aPath[i].zName,pName->nByte) == 0 ){` |
|      1550 | 2009 | `			return aPath[i].mask;` |
|         - | 2010 | `		}` |
|    760911 | 2011 | `	}` |
|     20922 | 2012 | `	return 0;` |
|     11102 | 2013 | `}` |
|         - | 2014 | `/*` |
|         - | 2015 | ` * Does this argument carry a NUL byte? Only a STRING can: every other scalar` |
|         - | 2016 | ` * renders through the number/bool formatters, which emit none. An OBJECT is` |
|         - | 2017 | ` * coerced by the caller before asking (php's ZPP order), so by the time this` |
|         - | 2018 | ` * runs a Stringable is already the string it produced.` |
|         - | 2019 | ` */` |
|    111087 | 2020 | `static int VmArgHasNulByte(ph7_value *pArg)` |
|         5 | 2021 | `{` |
|         - | 2022 | `	const char *zStr;` |
|         - | 2023 | `	sxu32 n, nLen;` |
|    111092 | 2024 | `	if( (pArg->iFlags & MEMOBJ_STRING) == 0 ){` |
|        13 | 2025 | `		return 0;` |
|         - | 2026 | `	}` |
|    111082 | 2027 | `	zStr = (const char *)SyBlobData(&pArg->sBlob);` |
|    111082 | 2028 | `	nLen = SyBlobLength(&pArg->sBlob);` |
|   7110936 | 2029 | `	for( n = 0 ; n < nLen ; ++n ){` |
|   6999987 | 2030 | `		if( zStr[n] == 0 ){` |
|       131 | 2031 | `			return 1;` |
|         - | 2032 | `		}` |
|   3604308 | 2033 | `	}` |
|    110954 | 2034 | `	return 0;` |
|     55483 | 2035 | `}` |
|         - | 2036 | `/*` |
|         - | 2037 | ` * Does php's strict_types rule refuse this argument for the declared type?` |
|         - | 2038 | ` *` |
|         - | 2039 | `` * A `declare(strict_types=1)` file gets NO scalar coercion at an internal call`` |
|         - | 2040 | ` * either — php applies the same rule to a builtin, a native method and a userland` |
|         - | 2041 | `` * function, and the single exception is the int -> float widening. So `trim(5)`,`` |
|         - | 2042 | `` * `sqrt("4")`, `str_repeat("a", 2.0)` and `in_array($n, $a, 1)` are all TypeErrors`` |
|         - | 2043 | ` * there, where the weak-mode screen below (which is the only one PHL had) coerces` |
|         - | 2044 | ` * and computes.` |
|         - | 2045 | ` *` |
|         - | 2046 | ` * Only the arms a scalar could otherwise satisfy are decided here; an array, a` |
|         - | 2047 | ` * resource, a null and a class-typed mismatch are the weak screen's, and its` |
|         - | 2048 | ` * verdicts stand in both modes.` |
|         - | 2049 | ` */` |
|       432 | 2050 | `static int VmStrictArgRefused(ph7_value *pArg,const char *zType,int nType)` |
|         3 | 2051 | `{` |
|         - | 2052 | `	/* Tested in ph7_type_name()'s own order, so the branch taken and the name the` |
|         - | 2053 | `	 * refusal reports can never disagree. FLOAT comes before INT on purpose:` |
|         - | 2054 | `	 * ph7_value_is_int() is deliberately lenient — an integer-valued real caches an` |
|         - | 2055 | ``	 * int and answers TRUE — and `str_repeat("a", 2.0)` is php's TypeError, not an`` |
|         - | 2056 | `	 * accepted int. */` |
|       435 | 2057 | `	if( ph7_value_is_bool(pArg) ){` |
|        12 | 2058 | `		return !VmSigTypeHas(zType,nType,"bool")` |
|         7 | 2059 | `		    && !VmSigTypeHas(zType,nType,"true")` |
|        11 | 2060 | `		    && !VmSigTypeHas(zType,nType,"false");` |
|         - | 2061 | `	}` |
|       427 | 2062 | `	if( ph7_value_is_float(pArg) ){` |
|         7 | 2063 | `		return !VmSigTypeHas(zType,nType,"float");` |
|         - | 2064 | `	}` |
|       421 | 2065 | `	if( ph7_value_is_int(pArg) ){` |
|         - | 2066 | `		/* int -> float is the one widening strict mode keeps. */` |
|        96 | 2067 | `		return !VmSigTypeHas(zType,nType,"int") && !VmSigTypeHas(zType,nType,"float");` |
|         - | 2068 | `	}` |
|       327 | 2069 | `	if( ph7_value_is_string(pArg) ){` |
|         - | 2070 | ``		/* `callable` is not a coercion: a function-name string satisfies it in both`` |
|         - | 2071 | `		 * modes (array_map('strtoupper', …) under strict is php-legal). */` |
|       214 | 2072 | `		return !VmSigTypeHas(zType,nType,"string") && !VmSigTypeHas(zType,nType,"callable");` |
|         - | 2073 | `	}` |
|       115 | 2074 | `	if( ph7_value_is_object(pArg) ){` |
|         - | 2075 | ``		/* An object reaches a `string` parameter only through __toString(), which is`` |
|         - | 2076 | `		 * a coercion strict mode does not perform. Every other arm is the weak` |
|         - | 2077 | `		 * screen's decision. */` |
|        49 | 2078 | `		return VmSigTypeHas(zType,nType,"string")` |
|        24 | 2079 | `		    && !VmSigTypeHas(zType,nType,"object")` |
|         2 | 2080 | `		    && !VmSigTypeHas(zType,nType,"iterable")` |
|         2 | 2081 | `		    && !VmSigTypeHas(zType,nType,"callable")` |
|        47 | 2082 | `		    && !VmSigTypeHasClass(zType,nType);` |
|         - | 2083 | `	}` |
|        69 | 2084 | `	return 0;` |
|       219 | 2085 | `}` |
|         - | 2086 | `/*` |
|         - | 2087 | ` * The next parameter of a signature starting at *pzCur, or 0 when the screen must` |
|         - | 2088 | ` * STOP -- a malformed row with no '$', a variadic tail (whose type applies to every` |
|         - | 2089 | ` * argument after it), or the end of the text. Advances *pzCur past the parameter.` |
|         - | 2090 | ` *` |
|         - | 2091 | ` * This is the walk VmEnforceBuiltinArgTypes used to do inline, moved out unchanged so` |
|         - | 2092 | ` * that it has exactly ONE implementation: VmArgScreenStamp drives it once per builtin` |
|         - | 2093 | ` * to build the cached table, and the screen drives it per call when there is no table.` |
|         - | 2094 | ` * The cached form is therefore correct by construction rather than by inspection.` |
|         - | 2095 | ` */` |
|    104322 | 2096 | `static int VmArgScreenNext(const char **pzCur,const char *zEnd,VmArgScreenParam *pOut)` |
|         5 | 2097 | `{` |
|    104327 | 2098 | `	const char *zCur = *pzCur;` |
|         - | 2099 | `	const char *zType,*zName,*zStop;` |
|         - | 2100 | `	int nType,nName,bByRef;` |
|    104327 | 2101 | `	if( zCur >= zEnd ){` |
|     35665 | 2102 | `		return 0;` |
|         - | 2103 | `	}` |
|         - | 2104 | `	/* Parameter = "<type> $<name>[ = <default>]"; the type is whatever` |
|         - | 2105 | `	 * precedes the '$', and an empty type means "untyped" (no screen). */` |
|    104097 | 2106 | `	while( zCur < zEnd && zCur[0] == ' ' ){` |
|     35435 | 2107 | `		zCur++;` |
|         5 | 2108 | `	}` |
|     68667 | 2109 | `	zStop = zCur;` |
|   1257763 | 2110 | `	while( zStop < zEnd && zStop[0] != ',' ){` |
|   1189101 | 2111 | `		if( zStop[0] == '\'' \|\| zStop[0] == '"' ){` |
|      3983 | 2112 | `			zStop = VmSigSkipQuoted(zStop);` |
|      3983 | 2113 | `			if( zStop >= zEnd ){` |
|       ! 0 | 2114 | `				break;` |
|         - | 2115 | `			}` |
|      1984 | 2116 | `		}` |
|   1189101 | 2117 | `		zStop++;` |
|         5 | 2118 | `	}` |
|     68667 | 2119 | `	zName = zCur;` |
|    500257 | 2120 | `	while( zName < zStop && zName[0] != '$' ){` |
|    431595 | 2121 | `		zName++;` |
|         5 | 2122 | `	}` |
|     68667 | 2123 | `	if( zName >= zStop ){` |
|       ! 0 | 2124 | `		return 0; /* malformed / no parameter name -- stop screening */` |
|         - | 2125 | `	}` |
|     68667 | 2126 | `	if( zName >= zCur + 3 && SyMemcmp(zName - 3,"...",3) == 0 ){` |
|      4303 | 2127 | `		return 0; /* variadic tail: stop (its type applies to the rest) */` |
|         - | 2128 | `	}` |
|     64369 | 2129 | `	zType = zCur;` |
|     64369 | 2130 | `	nType = (int)(zName - zCur);` |
|         - | 2131 | ``	/* A `~Type $p` row is php's stub-versus-body mismatch: the type php DECLARES`` |
|         - | 2132 | `	 * (which Reflection must report) is looser than the one its C body asks for, so` |
|         - | 2133 | `	 * the screen stands aside and the builtin raises the TypeError itself.` |
|         - | 2134 | `	 * RecursiveCachingIterator::__construct is the first: it is declared` |
|         - | 2135 | ``	 * `Iterator $iterator` and refuses anything that is not a RecursiveIterator. */`` |
|     64369 | 2136 | `	pOut->bStub = (sxu8)((nType > 0 && zType[0] == '~') ? 1 : 0);` |
|         - | 2137 | `	/* Trim the trailing spaces and the by-ref marker of "array &$array" */` |
|     64369 | 2138 | `	bByRef = 0;` |
|    181503 | 2139 | `	while( nType > 0 && (zType[nType-1] == ' ' \|\| zType[nType-1] == '&') ){` |
|     59591 | 2140 | `		if( zType[nType-1] == '&' ){` |
|      2495 | 2141 | `			bByRef = 1;` |
|      1228 | 2142 | `		}` |
|     59591 | 2143 | `		nType--;` |
|         5 | 2144 | `	}` |
|     64369 | 2145 | `	zName++; /* skip '$' */` |
|     64369 | 2146 | `	nName = 0;` |
|    502679 | 2147 | `	while( &zName[nName] < zStop && zName[nName] != ' ' && zName[nName] != '=' ){` |
|    438315 | 2148 | `		nName++;` |
|         5 | 2149 | `	}` |
|     64369 | 2150 | `	pOut->zType  = zType;` |
|     64369 | 2151 | `	pOut->nType  = (sxu16)nType;` |
|     64369 | 2152 | `	pOut->zName  = zName;` |
|     64369 | 2153 | `	pOut->nName  = (sxu16)nName;` |
|     64369 | 2154 | `	pOut->bByRef = (sxu8)bByRef;` |
|         - | 2155 | `	/* Which arms this type has, asked ONCE. Every bit is set by calling the function` |
|         - | 2156 | `	 * that used to answer it per argument, so the mask cannot say something the walk` |
|         - | 2157 | `	 * would not -- the same construction the parse above uses, for the same reason:` |
|         - | 2158 | `	 * this screen decides TypeErrors. */` |
|         - | 2159 | `	{` |
|     64369 | 2160 | `		sxu32 m = 0;` |
|     64369 | 2161 | `		if( VmSigTypeHas(zType,nType,"mixed")    ){ m \|= VMSIG_MIXED;    }` |
|     64369 | 2162 | `		if( VmSigTypeHas(zType,nType,"array")    ){ m \|= VMSIG_ARRAY;    }` |
|     64369 | 2163 | `		if( VmSigTypeHas(zType,nType,"iterable") ){ m \|= VMSIG_ITERABLE; }` |
|     64369 | 2164 | `		if( VmSigTypeHas(zType,nType,"callable") ){ m \|= VMSIG_CALLABLE; }` |
|     64369 | 2165 | `		if( VmSigTypeHas(zType,nType,"object")   ){ m \|= VMSIG_OBJECT;   }` |
|     64369 | 2166 | `		if( VmSigTypeHas(zType,nType,"string")   ){ m \|= VMSIG_STRING;   }` |
|     64369 | 2167 | `		if( VmSigTypeHas(zType,nType,"null")     ){ m \|= VMSIG_NULL;     }` |
|     64369 | 2168 | `		if( VmSigTypeHas(zType,nType,"int")      ){ m \|= VMSIG_INT;      }` |
|     64369 | 2169 | `		if( VmSigTypeHas(zType,nType,"float")    ){ m \|= VMSIG_FLOAT;    }` |
|     64369 | 2170 | `		if( VmSigTypeHas(zType,nType,"bool")     ){ m \|= VMSIG_BOOL;     }` |
|     64369 | 2171 | `		if( VmSigTypeHas(zType,nType,"true")     ){ m \|= VMSIG_TRUE;     }` |
|     64369 | 2172 | `		if( VmSigTypeHas(zType,nType,"false")    ){ m \|= VMSIG_FALSE;    }` |
|     64369 | 2173 | `		if( VmSigTypeHas(zType,nType,"resource") ){ m \|= VMSIG_RESOURCE; }` |
|     64369 | 2174 | `		if( VmSigTypeHasClass(zType,nType)       ){ m \|= VMSIG_CLASS;    }` |
|     64369 | 2175 | `		if( VmSigTypeIsIntOnly(zType,nType)      ){ m \|= VMSIG_INTONLY;  }` |
|     64369 | 2176 | `		if( VmSigTypeIsArrayOnly(zType,nType)    ){ m \|= VMSIG_ARRAYONLY;}` |
|     64369 | 2177 | `		pOut->nMask = m;` |
|         - | 2178 | `	}` |
|     64369 | 2179 | `	*pzCur = (zStop < zEnd) ? zStop + 1 : zEnd;` |
|     64369 | 2180 | `	return 1;` |
|     51498 | 2181 | `}` |
|         - | 2182 | `/*` |
|         - | 2183 | ` * Work out this builtin's parameter table once and keep it on its record, beside the` |
|         - | 2184 | ` * two name questions and the signature length. Two passes over the same walk: count,` |
|         - | 2185 | ` * then fill. Leaves aSigParam at 0 when the signature yields no parameters or the` |
|         - | 2186 | ` * allocation fails, and the screen then walks the text per call exactly as before --` |
|         - | 2187 | ` * the fallback is the same code, so there is no second set of answers to keep in step.` |
|         - | 2188 | ` *` |
|         - | 2189 | ` * The table lives in the VM's allocator and is reclaimed with it, the same lifetime as` |
|         - | 2190 | ` * the name strdup beside it in PH7_NewForeignFunction.` |
|         - | 2191 | ` */` |
|     22462 | 2192 | `static void VmArgScreenStamp(ph7_vm *pVm,ph7_user_func *pFunc,const char *zSig,sxu32 nSigLen)` |
|         5 | 2193 | `{` |
|     22467 | 2194 | `	const char *zEnd = &zSig[nSigLen];` |
|         - | 2195 | `	const char *zCur;` |
|         - | 2196 | `	VmArgScreenParam sTmp;` |
|         - | 2197 | `	VmArgScreenParam *aParam;` |
|         - | 2198 | `	sxu32 n;` |
|     54649 | 2199 | `	for( n = 0, zCur = zSig ; VmArgScreenNext(&zCur,zEnd,&sTmp) ; ++n ){` |
|         - | 2200 | `		/* counting only */` |
|     15883 | 2201 | `	}` |
|     22467 | 2202 | `	if( n < 1 ){` |
|      6845 | 2203 | `		return;` |
|         - | 2204 | `	}` |
|     23330 | 2205 | `	aParam = (VmArgScreenParam *)SyMemBackendPoolAlloc(&pVm->sAllocator,` |
|      7703 | 2206 | `		n * (sxu32)sizeof(VmArgScreenParam));` |
|     15627 | 2207 | `	if( aParam == 0 ){` |
|       ! 0 | 2208 | `		return;` |
|         - | 2209 | `	}` |
|     47809 | 2210 | `	for( n = 0, zCur = zSig ; VmArgScreenNext(&zCur,zEnd,&aParam[n]) ; ++n ){` |
|         - | 2211 | `		/* filling */` |
|     15883 | 2212 | `	}` |
|     15627 | 2213 | `	pFunc->aSigParam = aParam;` |
|     15627 | 2214 | `	pFunc->nSigParam = (sxu16)n;` |
|     11102 | 2215 | `}` |
|         - | 2216 | `/*` |
|         - | 2217 | ` * PHP-8 ZPP type enforcement for host functions, driven by the aBuiltinSig[]` |
|         - | 2218 | ` * declaration (band A #7). Screens only the arguments that php can NEVER coerce` |
|         - | 2219 | ` * into a declared scalar parameter — arrays, resources, and objects without a` |
|         - | 2220 | ` * __toString() — and throws the catchable TypeError php throws, before the C` |
|         - | 2221 | ` * routine runs. Without this an array argument reached the builtin and was` |
|         - | 2222 | ` * stringified to the literal "Array" (strrev(['a']) returned "yarrA").` |
|         - | 2223 | ` *` |
|         - | 2224 | ` * Deliberately narrow: scalar-to-scalar coercion (and its deprecations) stays` |
|         - | 2225 | ` * with the per-builtin ZPP helpers. Those emit E_DEPRECATED, which does NOT` |
|         - | 2226 | ` * abort the call, so a central copy would double-fire — the trap that sank the` |
|         - | 2227 | ` * first central-ZPP attempt. A TypeError aborts, so there is nothing to double.` |
|         - | 2228 | ` */` |
|   6460446 | 2229 | `PH7_PRIVATE sxi32 VmEnforceBuiltinArgTypes(` |
|         - | 2230 | `	ph7_context *pCtx,    /* Call context (for the throw) */` |
|         - | 2231 | `	ph7_user_func *pFunc, /* Callee */` |
|         - | 2232 | `	int nGiven,           /* Argument count */` |
|         - | 2233 | `	ph7_value **apArg     /* Arguments */` |
|         - | 2234 | `	)` |
|         5 | 2235 | `{` |
|         - | 2236 | `	/*` |
|         - | 2237 | `	 * Builtins whose own argument check is php-exact and VALUE-based rather than` |
|         - | 2238 | `	 * type-based must not be pre-empted here, or their message is lost. Same rule` |
|         - | 2239 | `	 * the aBuiltinArity[] table follows: a builtin that already says what php says` |
|         - | 2240 | `	 * stays off the shared screen. get_class_vars() takes any stringifiable value` |
|         - | 2241 | `	 * and reports "must be a valid class name, Array given"; get_class_methods() is` |
|         - | 2242 | `	 * the same shape with php's other wording ("must be an object or a valid class` |
|         - | 2243 | ``	 * name, int given") — the declared `object\|string` never appears in either.`` |
|         - | 2244 | `	 *` |
|         - | 2245 | `	 * strtr() is here for a structural reason: php declares it as two OVERLOADS` |
|         - | 2246 | `	 * dispatched on arity — strtr(string, array) and strtr(string, string, string)` |
|         - | 2247 | `` 	 * — so the expected type of $from is `array` with two arguments and `string` `` |
|         - | 2248 | ``	 * with three. One signature cannot say that (the stub's `array\|string` is the`` |
|         - | 2249 | `	 * union of the two, which is the wording php never uses), so the builtin does` |
|         - | 2250 | `	 * its own dispatch through PH7_ArgSatisfiesString(), the same rule as here.` |
|         - | 2251 | `	 *` |
|         - | 2252 | ``	 * implode() is the same structure: `array\|string $separator` is what the two`` |
|         - | 2253 | `	 * ARITIES accept between them, never what one call can use. Once an $array` |
|         - | 2254 | `	 * argument is present php has resolved the overload and reports` |
|         - | 2255 | ``	 * `must be of type string`, and with the array in position #1 it reports`` |
|         - | 2256 | ``	 * `must be of type string, array given` against #1 rather than a #2 error.`` |
|         - | 2257 | `	 * PH7_builtin_implode words all of that itself.` |
|         - | 2258 | `	 *` |
|         - | 2259 | `	 * Its alias join() is here for the same reason and then some: php 8.5 does not` |
|         - | 2260 | `	 * word the two the same, so the builtin reproduces BOTH orders keyed on the` |
|         - | 2261 | `	 * invoked name (see PH7_builtin_implode's header for the value-for-value` |
|         - | 2262 | `	 * table against 8.5.8). php's own asymmetry between a target and its alias,` |
|         - | 2263 | `	 * reproduced rather than smoothed over — parity is binding (§10).` |
|         - | 2264 | `	 *` |
|         - | 2265 | `	 * number_format() is here because php's DECLARED type and its REFUSAL text` |
|         - | 2266 | ``	 * disagree: the stub says `float $num` (which is what Reflection prints) while`` |
|         - | 2267 | `	 * the ZPP macro behind it is Z_PARAM_NUMBER, whose TypeError says` |
|         - | 2268 | ``	 * `must be of type int\|float`. One row cannot say both, so the row carries the`` |
|         - | 2269 | `	 * declared type for Reflection and the builtin words every refusal itself.` |
|         - | 2270 | `	 *` |
|         - | 2271 | `	 * RecursiveIteratorIterator::__construct() is the first NATIVE METHOD here, and` |
|         - | 2272 | `	 * it is the same disagreement one level up: php's stub declares` |
|         - | 2273 | ``	 * `Traversable $iterator` (what Reflection prints) while its ZPP is a bare "o",`` |
|         - | 2274 | ``	 * whose TypeError says `must be of type object`. A native method's diagnostic`` |
|         - | 2275 | `	 * name is the QUALIFIED one, so the row below matches it and nothing else.` |
|         - | 2276 | `	 *` |
|         - | 2277 | `	 * The array_udiff/array_uintersect u-variant family is here for its ORDER:` |
|         - | 2278 | `	 * php validates the trailing comparison callback(s) before ANY of the` |
|         - | 2279 | `	 * arrays — array_diff_ukey(123,[1],456) names Argument #3, not #1 — and a` |
|         - | 2280 | `	 * positional screen cannot say that. HashmapUVariant performs the whole` |
|         - | 2281 | `	 * php sequence itself (callbacks, then Argument #1, then the middles).` |
|         - | 2282 | `	 */` |
|         - | 2283 | `	static const char *azSelfChecked[] = { "get_class_vars", "get_class_methods", "strtr",` |
|         - | 2284 | `		"implode", "join", "number_format", "RecursiveIteratorIterator::__construct",` |
|         - | 2285 | `		"array_udiff", "array_udiff_assoc", "array_udiff_uassoc",` |
|         - | 2286 | `		"array_uintersect", "array_uintersect_assoc", "array_uintersect_uassoc",` |
|         - | 2287 | `		"array_diff_uassoc", "array_diff_ukey",` |
|         - | 2288 | `		"array_intersect_uassoc", "array_intersect_ukey" };` |
|   6460451 | 2289 | `	const char *zSig = pFunc->zSig;` |
|         - | 2290 | `	const char *zCur, *zEnd;` |
|   6460451 | 2291 | `	int iArg = 0;` |
|         - | 2292 | `	/* The CALL site's file mode, stamped by the compiler onto this call's argument` |
|         - | 2293 | `	 * map (weak when there is no map — a call that carries no compile-time metadata` |
|         - | 2294 | `	 * was written in a weak-mode file, since a strict one always attaches one). */` |
|   6460451 | 2295 | `	int bStrict = (pCtx->pArgMap && pCtx->pArgMap->bStrict) ? 1 : 0;` |
|         - | 2296 | `	sxu32 nPathMask;` |
|   6460451 | 2297 | `	if( zSig == 0 ){` |
|    244405 | 2298 | `		return SXRET_OK;` |
|         - | 2299 | `	}` |
|         - | 2300 | `	/* All three of the questions below are about the DECLARATION, which cannot change:` |
|         - | 2301 | `	 * two are about the NAME -- and used to be answered by SCANNING a table on every` |
|         - | 2302 | `	 * builtin call, seventeen names here and about seventy in VmBuiltinPathMask, two` |
|         - | 2303 | `	 * SyStrlen calls per row -- and the third is the length of the signature TEXT,` |
|         - | 2304 | `	 * which zEnd below used to measure on every call. Worked out once and kept on the` |
|         - | 2305 | `	 * function's own record. */` |
|   6216051 | 2306 | `	if( !pFunc->bScreenStamped ){` |
|         - | 2307 | `		int iSelf;` |
|     22467 | 2308 | `		pFunc->nPathMask = VmBuiltinPathMask(&pFunc->sName);` |
|     22467 | 2309 | `		pFunc->nSigLen = (sxu32)SyStrlen(zSig);` |
|     22467 | 2310 | `		VmArgScreenStamp(pCtx->pVm,pFunc,zSig,pFunc->nSigLen);` |
|    400212 | 2311 | `		for( iSelf = 0 ; iSelf < (int)SX_ARRAYSIZE(azSelfChecked) ; ++iSelf ){` |
|    378106 | 2312 | `			const char *zSelf = azSelfChecked[iSelf];` |
|    378101 | 2313 | `			if( SyStrncmp(pFunc->sName.zString,zSelf,pFunc->sName.nByte) == 0` |
|    187416 | 2314 | `			 && SyStrlen(zSelf) == pFunc->sName.nByte ){` |
|       361 | 2315 | `				pFunc->bSelfChecked = 1;` |
|       361 | 2316 | `				break;` |
|         - | 2317 | `			}` |
|    186642 | 2318 | `		}` |
|     22467 | 2319 | `		pFunc->bScreenStamped = 1;` |
|     11097 | 2320 | `	}` |
|   6216051 | 2321 | `	if( pFunc->bSelfChecked ){` |
|     51442 | 2322 | `		return SXRET_OK;` |
|         - | 2323 | `	}` |
|   6164614 | 2324 | `	nPathMask = pFunc->nPathMask;` |
|   6164614 | 2325 | `	zCur = zSig;` |
|   6164614 | 2326 | `	zEnd = &zSig[pFunc->nSigLen];   /* measured once, above -- never per call */` |
|  15527585 | 2327 | `	for( iArg = 0 ; iArg < nGiven ; ++iArg ){` |
|         - | 2328 | `		VmArgScreenParam sParam;` |
|         - | 2329 | `		const char *zType, *zName;` |
|         - | 2330 | `		int nType, nName, bByRef;` |
|         - | 2331 | `		sxu32 nMask;` |
|         - | 2332 | `		ph7_value *pArg;` |
|         - | 2333 | `		char zGivenBuf[64];` |
|         - | 2334 | `		/* The parameter this argument is screened against. Worked out once per` |
|         - | 2335 | `		 * builtin when the table could be built, and by the same walk per call when` |
|         - | 2336 | `		 * it could not; either way the screen stops where the walk stopped. */` |
|   9390749 | 2337 | `		if( pFunc->aSigParam ){` |
|   9388875 | 2338 | `			if( iArg >= (int)pFunc->nSigParam ){` |
|     25615 | 2339 | `				break;` |
|         - | 2340 | `			}` |
|   9364202 | 2341 | `			sParam = pFunc->aSigParam[iArg];` |
|   4683765 | 2342 | `		}else if( !VmArgScreenNext(&zCur,zEnd,&sParam) ){` |
|      1879 | 2343 | `			break;` |
|         - | 2344 | `		}` |
|   9364202 | 2345 | `		if( sParam.bStub ){` |
|       831 | 2346 | `			continue; /* the builtin raises its own TypeError -- see VmArgScreenNext */` |
|         - | 2347 | `		}` |
|   9363378 | 2348 | `		zType  = sParam.zType;` |
|   9363378 | 2349 | `		nType  = (int)sParam.nType;` |
|   9363378 | 2350 | `		zName  = sParam.zName;` |
|   9363378 | 2351 | `		nName  = (int)sParam.nName;` |
|   9363378 | 2352 | `		bByRef = sParam.bByRef;` |
|   9363378 | 2353 | `		nMask  = sParam.nMask;   /* which arms this type has, worked out once per parameter */` |
|   9363378 | 2354 | `		pArg = apArg[iArg];` |
|   9363373 | 2355 | `		if( bByRef && pArg->nIdx == SXU32_HIGH` |
|     18987 | 2356 | `		 && !(pCtx->pArgMap && pCtx->pArgMap->bArgShapes && !pCtx->pArgMap->bHasNamed) ){` |
|         - | 2357 | `			/* A by-reference parameter handed something with no slot to write back` |
|         - | 2358 | `			 * through -- a literal, a constant, the result of a call. php settles` |
|         - | 2359 | `			 * that at the CALL, before the callee's ZPP runs, so the type screen` |
|         - | 2360 | ``			 * must not speak first: `array_pop('foo')` is`` |
|         - | 2361 | `			 * "could not be passed by reference" and not "must be of type array,` |
|         - | 2362 | `			 * string given".` |
|         - | 2363 | `			 *` |
|         - | 2364 | `			 * Only when this call site carries no argument SHAPES, though. When it` |
|         - | 2365 | `			 * does, PH7_VmScreenByRefArgShapes has already had its say — it refused` |
|         - | 2366 | `			 * the literal and let the call RESULT through with php's notice — and` |
|         - | 2367 | `			 * standing aside here would swallow the type error php still reports for` |
|         - | 2368 | ``			 * the latter (`sort(new stdClass)` is "must be of type array, stdClass`` |
|         - | 2369 | `			 * given", not a silent false). */` |
|         7 | 2370 | `			continue;` |
|         - | 2371 | `		}` |
|   9363372 | 2372 | `		if( nType > 0 && !(nMask & VMSIG_MIXED) ){` |
|   7809593 | 2373 | `			const char *zGiven = 0;` |
|   7809593 | 2374 | `			if( bStrict && VmStrictArgRefused(pArg,zType,nType) ){` |
|         - | 2375 | ``				/* php names the VALUE for a bool here too (`true given`). */`` |
|        37 | 2376 | `				zGiven = VmValueGivenName(pArg,zGivenBuf,sizeof(zGivenBuf));` |
|   7809575 | 2377 | `			}else if( (pArg->iFlags & MEMOBJ_HASHMAP) != 0 ){` |
|    459509 | 2378 | `				if( !(nMask & VMSIG_ARRAY)` |
|    229930 | 2379 | `				 && !(nMask & VMSIG_ITERABLE)` |
|       471 | 2380 | `				 && !(nMask & VMSIG_CALLABLE) ){` |
|       231 | 2381 | `					zGiven = "array";` |
|       118 | 2382 | `				}` |
|   7579745 | 2383 | `			}else if( (pArg->iFlags & MEMOBJ_OBJ) != 0 ){` |
|     58849 | 2384 | `				if( !(nMask & VMSIG_OBJECT)` |
|     51819 | 2385 | `				 && !(nMask & VMSIG_ITERABLE)` |
|     44840 | 2386 | `				 && !(nMask & VMSIG_CALLABLE)` |
|     42915 | 2387 | `				 && !(nMask & VMSIG_CLASS) ){` |
|         - | 2388 | `					/* An object with __toString() still satisfies a string` |
|         - | 2389 | `					 * parameter in weak mode — php coerces it. */` |
|       312 | 2390 | `					int bStringable = (nMask & VMSIG_STRING)` |
|       164 | 2391 | `						&& PH7_ArgSatisfiesString(pArg);` |
|       169 | 2392 | `					if( !bStringable ){` |
|        94 | 2393 | `						zGiven = VmArgTypeName(pArg);` |
|        45 | 2394 | `					}` |
|     58772 | 2395 | `				}else if( (nMask & VMSIG_CLASS)` |
|     49643 | 2396 | `				       && !(nMask & VMSIG_OBJECT)` |
|     40663 | 2397 | `				       && !(nMask & VMSIG_ITERABLE)` |
|     40663 | 2398 | `				       && !(nMask & VMSIG_CALLABLE)` |
|     40668 | 2399 | `				       && !(nMask & VMSIG_STRING) ){` |
|         - | 2400 | `					/* A class-typed parameter given an object of the WRONG class.` |
|         - | 2401 | `					 * Naming a class used to be enough to let ANY object through, so` |
|         - | 2402 | `` 					 * `date_modify($immutable)` and `timezone_name_get($date)` `` |
|         - | 2403 | `					 * answered silently where php raises. Only decided when every` |
|         - | 2404 | `					 * class arm resolves to a declared class: an arm PHL does not` |
|         - | 2405 | `					 * declare cannot be judged, so the parameter stays unscreened. */` |
|     60793 | 2406 | `					if( !VmSigObjSatisfiesClass(pCtx->pVm,zType,nType,` |
|     40523 | 2407 | `						(ph7_class_instance *)pArg->x.pOther) ){` |
|        61 | 2408 | `						zGiven = VmArgTypeName(pArg);` |
|        29 | 2409 | `					}` |
|     20270 | 2410 | `				}` |
|   7320596 | 2411 | `			}else if( (pArg->iFlags & MEMOBJ_NULL) != 0 ){` |
|         - | 2412 | `				/* php only DEPRECATES null for a non-nullable parameter; PHL rejects` |
|         - | 2413 | `				 * it (scope policy). A leading '?' or an explicit "null" arm in a` |
|         - | 2414 | `				 * union declares the parameter nullable. A "callable" parameter is` |
|         - | 2415 | `				 * left to the builtin's own callback check, which words the failure` |
|         - | 2416 | `				 * php's way ("must be a valid callback, no array or string given") —` |
|         - | 2417 | `				 * the same reason get_class_vars() sits on azSelfChecked[]. */` |
|      7481 | 2418 | `				if( zType[0] != '?'` |
|      3821 | 2419 | `				 && !(nMask & VMSIG_NULL)` |
|       147 | 2420 | `				 && !(nMask & VMSIG_CALLABLE) ){` |
|       109 | 2421 | `					zGiven = "null";` |
|        52 | 2422 | `				}` |
|   7287454 | 2423 | `			}else if( (pArg->iFlags & (MEMOBJ_STRING\|MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL)) != 0` |
|   7283713 | 2424 | `			       && ((nMask & VMSIG_CLASS)` |
|   7283111 | 2425 | `			        \|\| (nMask & VMSIG_OBJECT)) ){` |
|         - | 2426 | `				/* A SCALAR against a parameter that can only hold an INSTANCE —` |
|         - | 2427 | ``				 * a named class, or the bare `object` keyword. Every other scalar`` |
|         - | 2428 | `				 * pairing is left to weak-mode coercion, which is why nothing` |
|         - | 2429 | `				 * screened scalars here at all — but no coercion produces an` |
|         - | 2430 | `				 * instance, so php rejects this one. Found converting DateTime:` |
|         - | 2431 | ``				 * `$d->diff('x')` and `new DateTime('now','UTC')` ran on with a`` |
|         - | 2432 | ``				 * string where php raises. The `object` half was still blind when`` |
|         - | 2433 | `				 * WeakReference::create() declared the first such parameter, which` |
|         - | 2434 | `				 * also retires the "graceful degradation" NULL that spl_object_id(),` |
|         - | 2435 | `				 * spl_object_hash() and get_object_vars() used to answer. An arm a` |
|         - | 2436 | `				 * scalar CAN satisfy (a union with string/int/float/bool, or` |
|         - | 2437 | `				 * callable, which a string is) keeps the parameter unscreened —` |
|         - | 2438 | ``				 * and so does an `array` arm, whose refusal php words from the`` |
|         - | 2439 | `				 * builtin's own check rather than from the declared type` |
|         - | 2440 | ``				 * (array_walk's `array\|object &$array` says "must be of type`` |
|         - | 2441 | `				 * array", not "of type array\|object"). */` |
|      3955 | 2442 | `				if( !(nMask & VMSIG_STRING)` |
|      2124 | 2443 | `				 && !(nMask & VMSIG_INT)` |
|       228 | 2444 | `				 && !(nMask & VMSIG_FLOAT)` |
|       142 | 2445 | `				 && !(nMask & VMSIG_BOOL)` |
|       142 | 2446 | `				 && !(nMask & VMSIG_TRUE)` |
|       142 | 2447 | `				 && !(nMask & VMSIG_FALSE)` |
|       142 | 2448 | `				 && !(nMask & VMSIG_ARRAY)` |
|       115 | 2449 | `				 && !(nMask & VMSIG_CALLABLE) ){` |
|         - | 2450 | `					/* php's VALUE name, not the type's: a bool is reported as` |
|         - | 2451 | ``					 * `true`/`false` (the rule Generator::throw()'s own check`` |
|         - | 2452 | `					 * already followed, and which this screen now runs first). */` |
|        82 | 2453 | `					zGiven = VmValueGivenName(pArg,zGivenBuf,sizeof(zGivenBuf));` |
|      3920 | 2454 | `				}else if( (pArg->iFlags & (MEMOBJ_STRING\|MEMOBJ_NULL)) == MEMOBJ_STRING` |
|      3773 | 2455 | `				       && !(nMask & VMSIG_STRING)` |
|      1840 | 2456 | `				       && !(nMask & VMSIG_BOOL)` |
|        32 | 2457 | `				       && !(nMask & VMSIG_TRUE)` |
|        32 | 2458 | `				       && !(nMask & VMSIG_FALSE)` |
|        32 | 2459 | `				       && !(nMask & VMSIG_ARRAY)` |
|        22 | 2460 | `				       && !(nMask & VMSIG_CALLABLE)` |
|        17 | 2461 | `				       && !PH7_MemObjStringIsNumeric(pArg) ){` |
|         - | 2462 | ``					/* The one arm that let this STRING past is `int`/`float`, and it`` |
|         - | 2463 | `					 * only takes a NUMERIC one — no coercion turns a string into an` |
|         - | 2464 | ``					 * instance of the class arm beside it. `round(1.5, 0, "x")` is`` |
|         - | 2465 | ``					 * php's `must be of type RoundingMode\|int, string given`; PHL`` |
|         - | 2466 | `					 * narrowed it to mode 0 and reported the ValueError for an` |
|         - | 2467 | `					 * invalid MODE, which blames the wrong thing. The plain` |
|         - | 2468 | `					 * number-only spelling is screened by the STRING branch below;` |
|         - | 2469 | `					 * a class arm routes the same argument through here instead, so` |
|         - | 2470 | `					 * the rule has to be stated in both places. */` |
|         7 | 2471 | `					zGiven = "string";` |
|         3 | 2472 | `				}` |
|   7281730 | 2473 | `			}else if( (pArg->iFlags & MEMOBJ_REAL) != 0` |
|   3640781 | 2474 | `			       && (nMask & VMSIG_INTONLY)` |
|       651 | 2475 | `			       && !VmDoubleFitsInt((double)pArg->rVal) ){` |
|         - | 2476 | ``				/* A FLOAT against a parameter typed exactly `int` (or `?int`), and`` |
|         - | 2477 | `				 * one no int can hold: a fraction, a magnitude past the signed` |
|         - | 2478 | `				 * 64-bit range, NaN or an infinity. php refuses every one of them` |
|         - | 2479 | `				 * (zend_parse_arg_long's ZEND_DOUBLE_FITS_LONG / is-integral pair,` |
|         - | 2480 | `				 * the fractional case with a deprecation PHL rejects outright by` |
|         - | 2481 | `				 * §10) and the refusal is this screen's own wording.` |
|         - | 2482 | `				 *` |
|         - | 2483 | `				 * PH7_IntArgResolve has always said exactly this, but only for the` |
|         - | 2484 | `` 				 * builtins that CALL it from their own body — so `dechex(1.5)` `` |
|         - | 2485 | ``				 * answered '1', `array_fill(1.5,1,0)` filled from 1, and`` |
|         - | 2486 | ``				 * `strpos("abc","c",1e19)` took the offset as PHP_INT_MIN and`` |
|         - | 2487 | `				 * reported a ValueError about a range it never had. Seventy-five` |
|         - | 2488 | ``				 * `int` parameters across the signature table were unscreened that`` |
|         - | 2489 | `				 * way, and a NATIVE METHOD has no body to call the helper from at` |
|         - | 2490 | `				 * all. Deciding it from the declared type covers both callee kinds` |
|         - | 2491 | `				 * from one place, and the per-builtin helper still stands for the` |
|         - | 2492 | ``				 * message rows this screen cannot reach (the `azSelfChecked` set). */`` |
|        65 | 2493 | `				zGiven = "float";` |
|   7279733 | 2494 | `			}else if( (pArg->iFlags & (MEMOBJ_STRING\|MEMOBJ_NULL)) == MEMOBJ_STRING` |
|   5775220 | 2495 | `			       && ((nMask & VMSIG_INT)` |
|   4269176 | 2496 | `			        \|\| (nMask & VMSIG_FLOAT))` |
|   2135112 | 2497 | `			       && !(nMask & VMSIG_STRING)` |
|   2134745 | 2498 | `			       && !(nMask & VMSIG_ARRAY)` |
|       296 | 2499 | `			       && !(nMask & VMSIG_OBJECT)` |
|       288 | 2500 | `			       && !(nMask & VMSIG_ITERABLE)` |
|       288 | 2501 | `			       && !(nMask & VMSIG_CALLABLE)` |
|       288 | 2502 | `			       && !(nMask & VMSIG_BOOL)` |
|       293 | 2503 | `			       && !(nMask & VMSIG_CLASS) ){` |
|         - | 2504 | ``				/* A STRING against a NUMBER-only parameter — `int`, `float`, or the`` |
|         - | 2505 | ``				 * `int\|float` union, with no arm a string can satisfy. Weak mode`` |
|         - | 2506 | `				 * coerces a NUMERIC one and php refuses every other — "x", "2abc"` |
|         - | 2507 | ``				 * and "0x2" are all `must be of type int, string given` (rule 41: a`` |
|         - | 2508 | `				 * numeric PREFIX is not enough, which is what SyStrIsNumeric would` |
|         - | 2509 | `				 * have accepted). Every BUILTIN with an int parameter already got` |
|         - | 2510 | `				 * this from PH7_IntArgResolve, called from its own body; a native` |
|         - | 2511 | `` 				 * METHOD has no body to call it from, so `ArrayIterator::seek('x')` `` |
|         - | 2512 | ``				 * seeked to 0, `DateTime::setTimestamp('abc')` set 0 and`` |
|         - | 2513 | ``				 * `DOMNodeList::item('zz')` answered element 0 — wrong ANSWERS,`` |
|         - | 2514 | `				 * not missing errors. Screening the declared type here covers both` |
|         - | 2515 | `				 * callee kinds from one place.` |
|         - | 2516 | `				 *` |
|         - | 2517 | `				 * The FLOAT arm is the same hazard one type over, and it was the` |
|         - | 2518 | `				 * half nothing covered: PH7_IntArgResolve has no float twin, so a` |
|         - | 2519 | ``				 * `float $num` builtin that did not hand-roll its own check simply`` |
|         - | 2520 | `				 * converted the string to 0.0 and COMPUTED with it —` |
|         - | 2521 | ``				 * `cos("nope")` answered `float(1)`, `sqrt("nope")` `float(0)`,`` |
|         - | 2522 | ``				 * `log("nope")` `float(-INF)`. Numbers with nothing wrong-looking`` |
|         - | 2523 | `				 * about them, from input php refuses outright.` |
|         - | 2524 | `				 *` |
|         - | 2525 | `				 * The NULL rule stays where it is: PHL rejects null for a` |
|         - | 2526 | `				 * non-nullable parameter by policy (§10) where php deprecates. */` |
|       437 | 2527 | `				if( !PH7_MemObjStringIsNumeric(pArg) ){` |
|       173 | 2528 | `					zGiven = "string";` |
|       209 | 2529 | `				}else if( (nMask & VMSIG_INTONLY) && !VmNumStrFitsInt(pArg) ){` |
|         - | 2530 | `					/* A NUMERIC string an int cannot hold — "1.5", "1e19",` |
|         - | 2531 | `					 * "99999999999999999999". php refuses all three (the fractional` |
|         - | 2532 | `					 * one after a deprecation §10 turns into the refusal), and PHL` |
|         - | 2533 | ``					 * narrowed them silently: `dechex("1e19")` answered '1' and`` |
|         - | 2534 | ``					 * `str_repeat("a","99999999999999999999")` took PHP_INT_MAX as`` |
|         - | 2535 | `					 * the count. Same wording, same position as the float arm above,` |
|         - | 2536 | `					 * because php reaches both through one ZPP macro. */` |
|        19 | 2537 | `					zGiven = "string";` |
|         8 | 2538 | `				}` |
|   7279559 | 2539 | `			}else if( (pArg->iFlags & (MEMOBJ_STRING\|MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL)) != 0` |
|   7279410 | 2540 | `			       && (nMask & VMSIG_ARRAYONLY) ){` |
|         - | 2541 | ``				/* A SCALAR against a parameter typed exactly `array`. No coercion`` |
|         - | 2542 | `				 * produces one, so php refuses it -- but the screen exempted every` |
|         - | 2543 | ``				 * `array` arm, union or not, and a whole family had no check of its`` |
|         - | 2544 | `				 * own to fall back on: sort/rsort/ksort/krsort/shuffle and` |
|         - | 2545 | ``				 * usort/uasort/uksort each answered `false` for `sort($notAnArray)`,`` |
|         - | 2546 | `				 * which is also what they answer for a sort that genuinely failed.` |
|         - | 2547 | `				 * call_user_func_array('strlen', 'x') answered false too,` |
|         - | 2548 | `				 * iterator_apply RAN the callback, and getopt/hash/password_hash/` |
|         - | 2549 | `				 * password_needs_rehash/unserialize/fputcsv simply carried on with` |
|         - | 2550 | `				 * the string where an options ARRAY was declared.` |
|         - | 2551 | `				 *` |
|         - | 2552 | `				 * The builtins that DO check (array_keys, in_array, asort, ...) word` |
|         - | 2553 | `				 * it identically, so the screen only pre-empts them -- and corrects` |
|         - | 2554 | `				 * one detail on the way: their ph7_type_name() says "bool" where php` |
|         - | 2555 | ``				 * names the VALUE, `true` or `false`. */`` |
|       251 | 2556 | `				zGiven = VmValueGivenName(pArg,zGivenBuf,sizeof(zGivenBuf));` |
|   7279292 | 2557 | `			}else if( (pArg->iFlags & MEMOBJ_RES) != 0 ){` |
|         - | 2558 | `				/* A class-typed parameter also accepts a resource: several handles php 8` |
|         - | 2559 | `				 * models as objects are still resources here (xml_*'s XMLParser is the` |
|         - | 2560 | `				 * one the signatures already declare php-8-style, for reflection). The` |
|         - | 2561 | `				 * screen would otherwise reject the engine's own parser handle. Recorded` |
|         - | 2562 | `				 * as a divergence in NEWPLAN §7 — it goes away when those handles become` |
|         - | 2563 | `				 * real objects. */` |
|        10 | 2564 | `				if( !(nMask & VMSIG_RESOURCE)` |
|        12 | 2565 | `				 && !(nMask & VMSIG_CLASS) ){` |
|        12 | 2566 | `					zGiven = "resource";` |
|         5 | 2567 | `				}` |
|         5 | 2568 | `			}` |
|   7809593 | 2569 | `			if( zGiven ){` |
|         - | 2570 | ``				/* php's `object\|array` parameters come from ONE ZPP macro`` |
|         - | 2571 | `				 * (Z_PARAM_ARRAY_OR_OBJECT) and it names only "array" in the` |
|         - | 2572 | `				 * refusal — array_walk(null,…), current(null) and` |
|         - | 2573 | `				 * http_build_query(null) all say "must be of type array". The` |
|         - | 2574 | `				 * SCALAR branch above already encodes that rule by declining to` |
|         - | 2575 | `				 * screen at all; the null and resource branches do screen, so the` |
|         - | 2576 | `				 * reported type has to be corrected here instead. */` |
|      1103 | 2577 | `				if( (nMask & VMSIG_ARRAY) && (nMask & VMSIG_OBJECT) ){` |
|        16 | 2578 | `					zType = "array";` |
|        16 | 2579 | `					nType = (int)sizeof("array")-1;` |
|         7 | 2580 | `				}` |
|      1719 | 2581 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 2582 | `					"%z(): Argument #%d ($%.*s) must be of type %.*s, %s given",` |
|       549 | 2583 | `					&pFunc->sName,iArg + 1,nName,zName,nType,zType,zGiven);` |
|         - | 2584 | `			}` |
|   3904446 | 2585 | `		}` |
|         - | 2586 | ``		/* A NaN reaching a parameter php declares `string`: php's ZPP coerces it`` |
|         - | 2587 | ``		 * (to "NAN") and warns `unexpected NAN value was coerced to string`, the`` |
|         - | 2588 | `		 * same 8.5 diagnostic the cast and the concatenation raise. The builtin` |
|         - | 2589 | `		 * bodies read their argument with ph7_value_to_string, which is the SILENT` |
|         - | 2590 | `		 * conversion by design (the engine builds keys and messages with it), so` |
|         - | 2591 | `		 * the diagnostic belongs here, where the DECLARED type says a coercion is` |
|         - | 2592 | `		 * what is about to happen. A union that also accepts a NUMBER is left` |
|         - | 2593 | `		 * alone -- php keeps the float there and coerces nothing -- but` |
|         - | 2594 | ``		 * `array\|string`, the spelling str_replace()'s subject carries, does`` |
|         - | 2595 | `		 * coerce and does warn. */` |
|   9362269 | 2596 | `		if( (pArg->iFlags & (MEMOBJ_REAL\|MEMOBJ_STRING)) == MEMOBJ_REAL` |
|   4682993 | 2597 | `		 && PH7_IS_NAN((double)pArg->rVal)` |
|      2122 | 2598 | `		 && (nMask & VMSIG_STRING)` |
|        52 | 2599 | `		 && !(nMask & VMSIG_FLOAT)` |
|         5 | 2600 | `		 && !(nMask & VMSIG_INT)` |
|         7 | 2601 | `		 && !(nMask & VMSIG_MIXED) ){` |
|         3 | 2602 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 2603 | `				"unexpected NAN value was coerced to string");` |
|         1 | 2604 | `		}` |
|         - | 2605 | `		/* A PATH parameter, once its type is settled: php's Z_PARAM_PATH refuses a` |
|         - | 2606 | `		 * NUL byte outright rather than letting the C API truncate at it. Raised` |
|         - | 2607 | `		 * after the type verdict because that is php's order — the coercion runs` |
|         - | 2608 | `		 * first, and only a value that could BE a path is asked whether it is a` |
|         - | 2609 | `		 * legal one. */` |
|   9362274 | 2610 | `		if( iArg < 31 && (nPathMask & (1u<<iArg)) != 0 ){` |
|    111092 | 2611 | `			if( (pArg->iFlags & MEMOBJ_OBJ) != 0 && PH7_ArgSatisfiesString(pArg) ){` |
|         - | 2612 | `				/* A Stringable object: php coerces it and checks the RESULT, so` |
|         - | 2613 | ``				 * `unlink($o)` with a __toString() returning a NUL-bearing name is`` |
|         - | 2614 | `				 * the same ValueError. Converting IN PLACE is what keeps the` |
|         - | 2615 | `				 * accessor running exactly ONCE — the builtin then receives the` |
|         - | 2616 | `				 * string it would have produced itself. The argument a builtin sees` |
|         - | 2617 | `				 * is its own copy on every dispatch route (a direct call, a spread,` |
|         - | 2618 | `				 * both call_user_func forwards), so the caller's object is not` |
|         - | 2619 | `				 * retyped; strict mode never gets here, because a Stringable does` |
|         - | 2620 | ``				 * not satisfy a `string` parameter there and the screen above has`` |
|         - | 2621 | `				 * already refused it. */` |
|         3 | 2622 | `				sxi32 rcConv = PH7_MemObjToStringUV(pArg);` |
|         3 | 2623 | `				if( rcConv != SXRET_OK ){` |
|       ! 0 | 2624 | `					return rcConv; /* __toString() threw: php propagates it too */` |
|         - | 2625 | `				}` |
|         1 | 2626 | `			}` |
|    111092 | 2627 | `			if( VmArgHasNulByte(pArg) ){` |
|       192 | 2628 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 2629 | `					"%z(): Argument #%d ($%.*s) must not contain any null bytes",` |
|        61 | 2630 | `					&pFunc->sName,iArg + 1,nName,zName);` |
|         - | 2631 | `			}` |
|     55417 | 2632 | `		}` |
|   4680866 | 2633 | `	}` |
|   6163388 | 2634 | `	return SXRET_OK;` |
|   3230121 | 2635 | `}` |
|         - | 2636 | `/*` |
|         - | 2637 | ` * Builtins whose accepted arity is NOT a contiguous range, so the signature` |
|         - | 2638 | ` * cannot express it and the central too-many-arguments check must stay out of` |
|         - | 2639 | ` * the way: rand()/mt_rand() take 0 OR 2 arguments (never 1), and php words the` |
|         - | 2640 | ` * violation "expects exactly 2 arguments, 3 given" from their own check rather` |
|         - | 2641 | ` * than the ZPP "at most". They validate themselves; leaving bHasMaxArg at 0` |
|         - | 2642 | ` * keeps their message php-faithful.` |
|         - | 2643 | ` */` |
|   5978643 | 2644 | `static int VmBuiltinSelfValidatesArity(const char *zName)` |
|         5 | 2645 | `{` |
|         - | 2646 | `	static const char *const azSelf[] = { "rand", "mt_rand" };` |
|         - | 2647 | `	sxu32 i;` |
|  17919077 | 2648 | `	for( i = 0 ; i < SX_ARRAYSIZE(azSelf) ; i++ ){` |
|  11951672 | 2649 | `		sxu32 nSelf = SyStrlen(azSelf[i]);` |
|  11951672 | 2650 | `		if( SyStrlen(zName) == nSelf && SyStrncmp(zName,azSelf[i],nSelf) == 0 ){` |
|     11243 | 2651 | `			return 1;` |
|         - | 2652 | `		}` |
|   5943800 | 2653 | `	}` |
|   5967410 | 2654 | `	return 0;` |
|   2976110 | 2655 | `}` |
|         - | 2656 | `/*` |
|         - | 2657 | ` * One parameter of a declared signature, for the named-argument binder below.` |
|         - | 2658 | ` */` |
|         - | 2659 | `typedef struct VmSigParam VmSigParam;` |
|         - | 2660 | `struct VmSigParam` |
|         - | 2661 | `{` |
|         - | 2662 | `	const char *zName; int nName;   /* without the '$' */` |
|         - | 2663 | `	const char *zDef;  int nDef;    /* default TEXT, or 0 when the parameter is required */` |
|         - | 2664 | `	int bVariadic;` |
|         - | 2665 | `};` |
|         - | 2666 | `/*` |
|         - | 2667 | ` * Split a signature into its parameters: the NAME each one binds by and the default` |
|         - | 2668 | ` * TEXT to fall back on. The scan is VmDeriveArityFromSig's, kept apart because that one` |
|         - | 2669 | `` * only counts; a quoted default (`string $separator = ','`) hides a comma, which is why`` |
|         - | 2670 | ` * both go through VmSigSkipQuoted.` |
|         - | 2671 | ` */` |
|    111298 | 2672 | `static int VmSigParams(const char *zSig,VmSigParam *aOut,int nMax)` |
|         5 | 2673 | `{` |
|    111303 | 2674 | `	const char *zCur = zSig;` |
|    111303 | 2675 | `	const char *zStart = zSig;` |
|    111303 | 2676 | `	int n = 0;` |
|   4419314 | 2677 | `	for(;;){` |
|   9224416 | 2678 | `		if( zCur[0] == '\'' \|\| zCur[0] == '"' ){` |
|       121 | 2679 | `			zCur = VmSigSkipQuoted(zCur);` |
|       121 | 2680 | `			if( zCur[0] != '\0' ){` |
|       121 | 2681 | `				zCur++;` |
|        59 | 2682 | `			}` |
|       121 | 2683 | `			continue;` |
|         - | 2684 | `		}` |
|   9224298 | 2685 | `		if( zCur[0] == '\0' \|\| zCur[0] == ',' ){` |
|    476519 | 2686 | `			const char *z = zStart;` |
|    476519 | 2687 | `			const char *zEnd = zCur;` |
|    476519 | 2688 | `			if( n < nMax ){` |
|    476519 | 2689 | `				VmSigParam *p = &aOut[n];` |
|    476519 | 2690 | `				const char *zEq = 0;` |
|    476519 | 2691 | `				const char *zDollar = 0;` |
|    476519 | 2692 | `				p->zName = 0; p->nName = 0; p->zDef = 0; p->nDef = 0; p->bVariadic = 0;` |
|   9224550 | 2693 | `				for( ; z < zEnd ; z++ ){` |
|   8748036 | 2694 | `					if( z[0] == '$' && zDollar == 0 ){` |
|    476519 | 2695 | `						zDollar = z + 1;` |
|   8509257 | 2696 | `					}else if( z[0] == '=' && zEq == 0 ){` |
|    182340 | 2697 | `						zEq = z + 1;` |
|   8180215 | 2698 | `					}else if( z[0] == '.' && z + 2 < zEnd && z[1] == '.' && z[2] == '.' ){` |
|       173 | 2699 | `						p->bVariadic = 1;` |
|        84 | 2700 | `					}` |
|   4363931 | 2701 | `				}` |
|    476519 | 2702 | `				if( zDollar ){` |
|    476519 | 2703 | `					const char *zStop = zEq ? zEq - 1 : zEnd;` |
|    476519 | 2704 | `					const char *zN = zDollar;` |
|   3492493 | 2705 | `					while( zN < zStop && zN[0] != ' ' && zN[0] != '=' ){` |
|   3015979 | 2706 | `						zN++;` |
|         5 | 2707 | `					}` |
|    476519 | 2708 | `					p->zName = zDollar;` |
|    476519 | 2709 | `					p->nName = (int)(zN - zDollar);` |
|    237735 | 2710 | `				}` |
|    476519 | 2711 | `				if( zEq ){` |
|    364675 | 2712 | `					while( zEq < zEnd && zEq[0] == ' ' ){` |
|    182340 | 2713 | `						zEq++;` |
|         5 | 2714 | `					}` |
|    182340 | 2715 | `					p->zDef = zEq;` |
|    182340 | 2716 | `					p->nDef = (int)(zEnd - zEq);` |
|    182340 | 2717 | `					while( p->nDef > 0 && p->zDef[p->nDef-1] == ' ' ){` |
|       ! 0 | 2718 | `						p->nDef--;` |
|       ! 0 | 2719 | `					}` |
|     91028 | 2720 | `				}` |
|    476519 | 2721 | `				if( p->nName > 0 ){` |
|    476519 | 2722 | `					n++;` |
|    237735 | 2723 | `				}` |
|    237735 | 2724 | `			}` |
|    476519 | 2725 | `			if( zCur[0] == '\0' ){` |
|    111303 | 2726 | `				break;` |
|         - | 2727 | `			}` |
|    365221 | 2728 | `			zCur++;` |
|    365221 | 2729 | `			zStart = zCur;` |
|    365221 | 2730 | `			continue;` |
|         - | 2731 | `		}` |
|   8747784 | 2732 | `		zCur++;` |
|         5 | 2733 | `	}` |
|    111303 | 2734 | `	return n;` |
|         5 | 2735 | `}` |
|         - | 2736 | `/*` |
|         - | 2737 | ` * Materialize a signature default's TEXT into pOut. php's own stub values, which is a` |
|         - | 2738 | `` * small set: null, true/false, an integer or float, a quoted string, and `[]`. A default`` |
|         - | 2739 | `` * the table could not state (`= ?`, ~50 rows — §7.4) answers 0, and the caller then reports`` |
|         - | 2740 | ` * the parameter as not passed rather than inventing a value.` |
|         - | 2741 | ` */` |
|         8 | 2742 | `static int VmSigDefaultValue(ph7_vm *pVm,const VmSigParam *pParam,ph7_value *pOut)` |
|         2 | 2743 | `{` |
|        10 | 2744 | `	const char *z = pParam->zDef;` |
|        10 | 2745 | `	int n = pParam->nDef;` |
|        10 | 2746 | `	if( z == 0 \|\| n < 1 \|\| (n == 1 && z[0] == '?') ){` |
|         3 | 2747 | `		return 0;` |
|         - | 2748 | `	}` |
|         8 | 2749 | `	if( n == 4 && (SyStrnicmp(z,"null",4) == 0) ){` |
|       ! 0 | 2750 | `		PH7_MemObjRelease(pOut);` |
|       ! 0 | 2751 | `		return 1; /* a released value IS null */` |
|         - | 2752 | `	}` |
|         8 | 2753 | `	if( n == 4 && SyStrnicmp(z,"true",4) == 0 ){` |
|       ! 0 | 2754 | `		PH7_MemObjInitFromBool(pVm,pOut,1);` |
|       ! 0 | 2755 | `		return 1;` |
|         - | 2756 | `	}` |
|         8 | 2757 | `	if( n == 5 && SyStrnicmp(z,"false",5) == 0 ){` |
|       ! 0 | 2758 | `		PH7_MemObjInitFromBool(pVm,pOut,0);` |
|       ! 0 | 2759 | `		return 1;` |
|         - | 2760 | `	}` |
|         8 | 2761 | `	if( n == 2 && z[0] == '[' && z[1] == ']' ){` |
|         3 | 2762 | `		ph7_hashmap *pMap = PH7_NewHashmap(pVm,0,0);` |
|         3 | 2763 | `		if( pMap == 0 ){` |
|       ! 0 | 2764 | `			return 0;` |
|         - | 2765 | `		}` |
|         3 | 2766 | `		PH7_MemObjRelease(pOut);` |
|         3 | 2767 | `		pOut->x.pOther = pMap;` |
|         3 | 2768 | `		MemObjSetType(pOut,MEMOBJ_HASHMAP);` |
|         3 | 2769 | `		return 1;` |
|         - | 2770 | `	}` |
|         6 | 2771 | `	if( z[0] == '\'' \|\| z[0] == '"' ){` |
|         - | 2772 | `		SyString sStr;` |
|         3 | 2773 | `		SyStringInitFromBuf(&sStr,z + 1,n >= 2 ? n - 2 : 0);` |
|         3 | 2774 | `		PH7_MemObjInitFromString(pVm,pOut,&sStr);` |
|         3 | 2775 | `		return 1;` |
|         - | 2776 | `	}` |
|         3 | 2777 | `	if( z[0] == '-' \|\| z[0] == '+' \|\| (z[0] >= '0' && z[0] <= '9') ){` |
|         - | 2778 | `		SyString sNum;` |
|         3 | 2779 | `		SyStringInitFromBuf(&sNum,z,(sxu32)n);` |
|         3 | 2780 | `		if( PH7_MemObjInitFromString(pVm,pOut,&sNum) != SXRET_OK ){` |
|       ! 0 | 2781 | `			return 0;` |
|         - | 2782 | `		}` |
|         3 | 2783 | `		PH7_MemObjToNumeric(pOut);` |
|         3 | 2784 | `		return 1;` |
|         - | 2785 | `	}` |
|       ! 0 | 2786 | `	return 0; /* a constant expression (M_PI, PHP_ROUND_HALF_UP, …): not evaluated here */` |
|         6 | 2787 | `}` |
|         - | 2788 | `/*` |
|         - | 2789 | ` * Bind a call's NAMED arguments to the callee's declared parameter POSITIONS.` |
|         - | 2790 | ` *` |
|         - | 2791 | ` * A compiled function does this from its parameter records (VmResolveNamedArgs); a host` |
|         - | 2792 | ` * function and a native method have none, so every named argument was simply passed in the` |
|         - | 2793 | `` * order it was WRITTEN. `str_pad(length: 5, string: "x")` reached the builtin as`` |
|         - | 2794 | ` * ("x" at #2, 5 at #1) and reported a TypeError, and — worse, because it is silent —` |
|         - | 2795 | `` * `str_pad("x", 5, pad_type: STR_PAD_LEFT)` bound the flag as $pad_string and answered`` |
|         - | 2796 | ` * "x0000" where php answers "    x". Both spellings are php 8.0 syntax, and the whole` |
|         - | 2797 | ` * ~650-builtin surface plus every native method was affected.` |
|         - | 2798 | ` *` |
|         - | 2799 | ` * The declared signature is the source of names, defaults and positions — the same string` |
|         - | 2800 | ` * Reflection prints. Rewrites *pnArg / apArg in place (the caller's argument vector is` |
|         - | 2801 | ` * scratch it owns) and answers SXRET_OK, or throws php's Error and returns its status.` |
|         - | 2802 | ` * Callees with a VARIADIC tail are left alone: php collects extra named arguments into it` |
|         - | 2803 | ` * by NAME, which the positional vector here cannot express.` |
|         - | 2804 | ` */` |
|       102 | 2805 | `PH7_PRIVATE sxi32 PH7_VmBindNamedArgsToSig(` |
|         - | 2806 | `	ph7_context *pCtx,      /* Call context (for the throws) */` |
|         - | 2807 | `	ph7_user_func *pFunc,   /* Callee: its zSig names the parameters */` |
|         - | 2808 | `	VmCallArgMap *pMap,     /* Call-site map; its aNames[] are per ACTUAL slot */` |
|         - | 2809 | `	int *pnArg,             /* IN/OUT: argument count */` |
|         - | 2810 | `	ph7_value **apArg       /* IN/OUT: argument vector */` |
|         - | 2811 | `	)` |
|         3 | 2812 | `{` |
|         - | 2813 | `	/* php's own stubs top out well under this; a signature with more parameters simply` |
|         - | 2814 | `	 * keeps the positional binding it had. */` |
|         - | 2815 | `#define VM_SIG_MAX_PARAM 32` |
|         - | 2816 | `	VmSigParam aParam[VM_SIG_MAX_PARAM];` |
|         - | 2817 | `	ph7_value *apBound[VM_SIG_MAX_PARAM];` |
|         - | 2818 | `	int nParam,nArg,i,nLast;` |
|       105 | 2819 | `	if( pFunc == 0 \|\| pFunc->zSig == 0 \|\| pMap == 0 \|\| pMap->bHasNamed == 0 ){` |
|        13 | 2820 | `		return SXRET_OK;` |
|         - | 2821 | `	}` |
|        93 | 2822 | `	nArg = *pnArg;` |
|        93 | 2823 | `	if( nArg < 1 \|\| nArg > VM_SIG_MAX_PARAM ){` |
|       ! 0 | 2824 | `		return SXRET_OK;` |
|         - | 2825 | `	}` |
|        93 | 2826 | `	nParam = VmSigParams(pFunc->zSig,aParam,VM_SIG_MAX_PARAM);` |
|        93 | 2827 | `	if( nParam < 1 \|\| aParam[nParam-1].bVariadic ){` |
|        21 | 2828 | `		return SXRET_OK;` |
|         - | 2829 | `	}` |
|       283 | 2830 | `	for( i = 0 ; i < nParam ; ++i ){` |
|       213 | 2831 | `		apBound[i] = 0;` |
|       108 | 2832 | `	}` |
|        73 | 2833 | `	nLast = -1;` |
|       221 | 2834 | `	for( i = 0 ; i < nArg ; ++i ){` |
|       157 | 2835 | `		int p = i;` |
|       223 | 2836 | `		if( i < (int)pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|       141 | 2837 | `			SyString *pName = &pMap->aNames[i];` |
|       267 | 2838 | `			for( p = 0 ; p < nParam ; ++p ){` |
|       260 | 2839 | `				if( (int)pName->nByte == aParam[p].nName` |
|       217 | 2840 | `				 && SyMemcmp(pName->zString,aParam[p].zName,pName->nByte) == 0 ){` |
|       137 | 2841 | `					break;` |
|         - | 2842 | `				}` |
|        65 | 2843 | `			}` |
|       141 | 2844 | `			if( p >= nParam ){` |
|         7 | 2845 | `				return PH7_VmThrowException(pCtx,"Error",` |
|         2 | 2846 | `					"Unknown named parameter $%z",pName);` |
|         - | 2847 | `			}` |
|       137 | 2848 | `			if( apBound[p] ){` |
|         4 | 2849 | `				return PH7_VmThrowException(pCtx,"Error",` |
|         1 | 2850 | `					"Named parameter $%z overwrites previous argument",pName);` |
|         3 | 2851 | `			}` |
|        84 | 2852 | `		}else if( p >= nParam ){` |
|       ! 0 | 2853 | `			return SXRET_OK; /* more positional arguments than the signature knows */` |
|         - | 2854 | `		}` |
|       151 | 2855 | `		apBound[p] = apArg[i];` |
|       151 | 2856 | `		if( p > nLast ){` |
|       129 | 2857 | `			nLast = p;` |
|        63 | 2858 | `		}` |
|        77 | 2859 | `	}` |
|       213 | 2860 | `	for( i = 0 ; i <= nLast ; ++i ){` |
|       151 | 2861 | `		if( apBound[i] == 0 ){` |
|        10 | 2862 | `			ph7_value *pDef = ph7_context_new_scalar(pCtx);` |
|        10 | 2863 | `			if( pDef == 0 \|\| !VmSigDefaultValue(pCtx->pVm,&aParam[i],pDef) ){` |
|         - | 2864 | `				SyString sName;` |
|         3 | 2865 | `				SyStringInitFromBuf(&sName,aParam[i].zName,(sxu32)aParam[i].nName);` |
|         4 | 2866 | `				return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|         1 | 2867 | `					"%z(): Argument #%d ($%z) not passed",&pFunc->sName,i + 1,&sName);` |
|         - | 2868 | `			}` |
|         8 | 2869 | `			apBound[i] = pDef;` |
|         3 | 2870 | `		}` |
|        76 | 2871 | `	}` |
|       209 | 2872 | `	for( i = 0 ; i <= nLast ; ++i ){` |
|       147 | 2873 | `		apArg[i] = apBound[i];` |
|        75 | 2874 | `	}` |
|        65 | 2875 | `	*pnArg = nLast + 1;` |
|        65 | 2876 | `	return SXRET_OK;` |
|        54 | 2877 | `}` |
|         - | 2878 | `/*` |
|         - | 2879 | ` * Name the Nth (0-based) parameter of a declared signature, without the '$'.` |
|         - | 2880 | ` *` |
|         - | 2881 | ` * The signature string is the only place a host function's parameter names live, and` |
|         - | 2882 | `` * php puts them in diagnostics — `sort(): Argument #1 ($array) …`. Answers 0 when the`` |
|         - | 2883 | ` * signature has no such parameter (or none with a name).` |
|         - | 2884 | ` */` |
|        12 | 2885 | `PH7_PRIVATE int PH7_VmSigParamName(const char *zSig,int nPos,SyString *pOut)` |
|         2 | 2886 | `{` |
|         - | 2887 | `	VmSigParam aParam[VM_SIG_MAX_PARAM];` |
|         - | 2888 | `	int nParam;` |
|        14 | 2889 | `	if( zSig == 0 \|\| nPos < 0 \|\| nPos >= VM_SIG_MAX_PARAM ){` |
|       ! 0 | 2890 | `		return 0;` |
|         - | 2891 | `	}` |
|        14 | 2892 | `	nParam = VmSigParams(zSig,aParam,VM_SIG_MAX_PARAM);` |
|        14 | 2893 | `	if( nPos >= nParam \|\| aParam[nPos].nName < 1 ){` |
|       ! 0 | 2894 | `		return 0;` |
|         - | 2895 | `	}` |
|        14 | 2896 | `	if( aParam[nPos].bVariadic ){` |
|         - | 2897 | ``		/* php's get_function_arg_name() answers NULL past `num_args`, which`` |
|         - | 2898 | `		 * counts the non-variadic parameters alone -- so an actual absorbed by` |
|         - | 2899 | ``		 * a `...` tail is named in no diagnostic (`sscanf(): Argument #3 must`` |
|         - | 2900 | ``		 * be passed by reference, value given`, with no ` ($vars)`). */`` |
|       ! 0 | 2901 | `		return 0;` |
|         - | 2902 | `	}` |
|        14 | 2903 | `	SyStringInitFromBuf(pOut,aParam[nPos].zName,(sxu32)aParam[nPos].nName);` |
|        14 | 2904 | `	return 1;` |
|         8 | 2905 | `}` |
|         - | 2906 | `/*` |
|         - | 2907 | `` * A `&` in a builtin's signature is not always php's ZEND_SEND_ARG_BY_REF.`` |
|         - | 2908 | ` *` |
|         - | 2909 | ` * php has a second mode, ZEND_SEND_PREFER_REF: bind by reference when the argument IS a` |
|         - | 2910 | ` * variable, and otherwise take it by value without a word. Reflection prints those` |
|         - | 2911 | ` * parameters as by-reference like any other and PHL's signature string cannot say which` |
|         - | 2912 | `` * mode a `&` means, so the two are told apart here. Probed value-for-value against php`` |
|         - | 2913 | `` * 8.5 over every `&` row PHL declares (41 of them): all but extract() refuse a`` |
|         - | 2914 | `` * non-variable, and extract() answers `int(1)` for `extract(['q' => 1])`.`` |
|         - | 2915 | ` *` |
|         - | 2916 | ` * array_multisort() is listed with it because it is php's other prefer-ref builtin and` |
|         - | 2917 | ` * PHL will need this the day it gains one (it is a MISSING builtin today, §5).` |
|         - | 2918 | ` */` |
|    111370 | 2919 | `PH7_PRIVATE int VmBuiltinPrefersRef(SyString *pName)` |
|         5 | 2920 | `{` |
|         - | 2921 | `	static const char *const azPreferRef[] = { "extract", "array_multisort" };` |
|         - | 2922 | `	sxu32 i;` |
|    333915 | 2923 | `	for( i = 0 ; i < SX_ARRAYSIZE(azPreferRef) ; ++i ){` |
|    222663 | 2924 | `		sxu32 nByte = SyStrlen(azPreferRef[i]);` |
|    222658 | 2925 | `		if( pName->nByte == nByte` |
|    111212 | 2926 | `		 && SyMemcmp(pName->zString,azPreferRef[i],nByte) == 0 ){` |
|       123 | 2927 | `			return 1;` |
|         - | 2928 | `		}` |
|    111005 | 2929 | `	}` |
|    111257 | 2930 | `	return 0;` |
|     55555 | 2931 | `}` |
|         - | 2932 | `/*` |
|         - | 2933 | ` * php refuses a by-reference argument at the CALL, before the callee's ZPP runs, and it` |
|         - | 2934 | ``  * decides from the argument's SHAPE, not from its value: `sort([3,1])`, `usort('x',$cb)` `` |
|         - | 2935 | `` * and `preg_match($p,$s,'lit')` are all`` |
|         - | 2936 | `` * `Error: sort(): Argument #1 ($array) could not be passed by reference`.`` |
|         - | 2937 | ` *` |
|         - | 2938 | ` * The call site's compile-time shape mask (VmCallArgMap.nNonLvalMask) is what says so.` |
|         - | 2939 | ` * Only five builtins raised anything before this, from their own bodies, on the runtime` |
|         - | 2940 | `` * `nIdx == SXU32_HIGH` signal — which cannot tell a literal from the result of a call, a`` |
|         - | 2941 | `` * shape php ACCEPTS with a notice. The thirty other `&` rows answered `true`/`false`/an`` |
|         - | 2942 | ` * int: the same answers they give for work they really did.` |
|         - | 2943 | ` *` |
|         - | 2944 | ` * Skipped when the call site has no shape mask (a spread, an indirect dispatch through` |
|         - | 2945 | ` * call_user_func, an engine-synthesized call) or uses named arguments (which rebind` |
|         - | 2946 | ` * positions the mask is indexed by). The by-ref positions come from the same declared` |
|         - | 2947 | ` * signature everything else here reads.` |
|         - | 2948 | ` */` |
|   6461354 | 2949 | `PH7_PRIVATE sxi32 PH7_VmScreenByRefArgShapes(` |
|         - | 2950 | `	ph7_context *pCtx,     /* Call context (for the throw) */` |
|         - | 2951 | `	ph7_user_func *pFunc,  /* Callee: its zSig names and marks the parameters */` |
|         - | 2952 | `	VmCallArgMap *pMap,    /* Call-site map, or 0 */` |
|         - | 2953 | `	int nGiven,            /* Argument count */` |
|         - | 2954 | `	ph7_value **apArg      /* Arguments */` |
|         - | 2955 | `	)` |
|         5 | 2956 | `{` |
|         - | 2957 | `	VmSigParam aParam[VM_SIG_MAX_PARAM];` |
|         - | 2958 | `	int nParam,n;` |
|         - | 2959 | `	/* The by-ref mask first: it is 0 for all but 41 of the ~650 host functions, so` |
|         - | 2960 | `	 * every other call leaves through one test. */` |
|   6461359 | 2961 | `	if( pFunc == 0 \|\| pFunc->nByRefMask == 0 \|\| pFunc->zSig == 0 \|\| nGiven < 1 ){` |
|   6348023 | 2962 | `		return SXRET_OK;` |
|         - | 2963 | `	}` |
|    113341 | 2964 | `	if( pMap == 0 \|\| !pMap->bArgShapes \|\| pMap->bHasNamed ){` |
|        79 | 2965 | `		return SXRET_OK;` |
|         - | 2966 | `	}` |
|    113265 | 2967 | `	if( (pMap->nNonLvalMask \| pMap->nTempCallMask) == 0 ){` |
|      1955 | 2968 | `		return SXRET_OK;` |
|         - | 2969 | `	}` |
|    111315 | 2970 | `	if( VmBuiltinPrefersRef(&pFunc->sName) ){` |
|       119 | 2971 | `		return SXRET_OK;` |
|         - | 2972 | `	}` |
|    111201 | 2973 | `	nParam = VmSigParams(pFunc->zSig,aParam,VM_SIG_MAX_PARAM);` |
|    441730 | 2974 | `	for( n = 0 ; n < nGiven && n < 31 ; ++n ){` |
|    330588 | 2975 | `		if( (pFunc->nByRefMask & (1u << n)) == 0 ){` |
|    294066 | 2976 | `			continue;` |
|         - | 2977 | `		}` |
|     36527 | 2978 | `		if( (pMap->nNonLvalMask & (1u << n)) == 0 ){` |
|         - | 2979 | `			/* Not a refusal — but a CALL result in this position is php's notice,` |
|         - | 2980 | `			 * and then the builtin operates on the temporary. */` |
|     36473 | 2981 | `			PH7_VmArgTempCallNotice(pCtx->pVm,pMap,(sxu32)n,apArg[n]);` |
|     36473 | 2982 | `			continue;` |
|         - | 2983 | `		}` |
|         - | 2984 | `		/* php names the parameter only when the position is a DECLARED one:` |
|         - | 2985 | ``		 * get_function_arg_name() answers NULL past `num_args`, which counts`` |
|         - | 2986 | `` 		 * the non-variadic parameters alone. So an actual absorbed by a `&...` `` |
|         - | 2987 | ``		 * tail (sscanf's `&...$vars`) is refused without a name. */`` |
|        59 | 2988 | `		if( n < nParam && aParam[n].nName > 0 && !aParam[n].bVariadic ){` |
|        83 | 2989 | `			return PH7_VmThrowException(pCtx,"Error",` |
|         - | 2990 | `				"%z(): Argument #%d ($%.*s) could not be passed by reference",` |
|        26 | 2991 | `				&pFunc->sName,n + 1,aParam[n].nName,aParam[n].zName);` |
|         - | 2992 | `		}` |
|         4 | 2993 | `		return PH7_VmThrowException(pCtx,"Error",` |
|         - | 2994 | `			"%z(): Argument #%d could not be passed by reference",` |
|         1 | 2995 | `			&pFunc->sName,n + 1);` |
|       ! 0 | 2996 | `	}` |
|    111147 | 2997 | `	return SXRET_OK;` |
|   3230575 | 2998 | `}` |
|         - | 2999 | `/*` |
|         - | 3000 | ` * D1: derive a by-reference position bitmask from a php-style signature string.` |
|         - | 3001 | `` * Bit N is set when positional parameter N is declared by-reference (a `&` appears`` |
|         - | 3002 | `` * anywhere in that comma-separated parameter, e.g. `&$matches`, `&...$vars`). Only`` |
|         - | 3003 | ` * the first 31 positions are representable; a by-ref parameter past that is rare` |
|         - | 3004 | ` * for a builtin and simply not tracked here. The scan mirrors VmDeriveArityFromSig.` |
|         - | 3005 | ` */` |
|  14991760 | 3006 | `PH7_PRIVATE sxu32 VmDeriveByRefMaskFromSig(const char *zSig)` |
|         5 | 3007 | `{` |
|  14991765 | 3008 | `	sxu32 mask = 0;` |
|  14991765 | 3009 | `	int n = 0;       /* current parameter index */` |
|  14991765 | 3010 | `	int bSeen = 0;   /* current parameter has non-space content */` |
|  14991765 | 3011 | ``	int bRef = 0;    /* current parameter carries a by-ref `&` */`` |
|  14991765 | 3012 | ``	int bVar = 0;    /* current parameter is a `...` variadic */`` |
|  14991765 | 3013 | `	int bTailRef = 0;/* the LAST parameter was a by-ref variadic */` |
|  14991765 | 3014 | `	const char *zCur = zSig;` |
| 170970482 | 3015 | `	for(;;){` |
| 352537021 | 3016 | `		if( zCur[0] == '\'' \|\| zCur[0] == '"' ){` |
|    612597 | 3017 | `			bSeen = 1;` |
|    612597 | 3018 | `			zCur = VmSigSkipQuoted(zCur);` |
|    612597 | 3019 | `			if( zCur[0] != '\0' ){` |
|    612597 | 3020 | `				zCur++;` |
|    305846 | 3021 | `			}` |
|    612597 | 3022 | `			continue;` |
|         - | 3023 | `		}` |
| 351924429 | 3024 | `		if( zCur[0] == '\0' \|\| zCur[0] == ',' ){` |
|  23978154 | 3025 | `			if( bSeen ){` |
|  18603062 | 3026 | `				if( bRef && n < 31 ){` |
|    674076 | 3027 | `					mask \|= (1u << n);` |
|    333695 | 3028 | `				}` |
|  18603062 | 3029 | `				bTailRef = (bRef && bVar);` |
|  18603062 | 3030 | `				n++;` |
|   9273513 | 3031 | `			}` |
|  23978154 | 3032 | `			if( zCur[0] == '\0' ){` |
|  14991765 | 3033 | `				break;` |
|         - | 3034 | `			}` |
|   8986394 | 3035 | `			bSeen = bRef = bVar = 0;` |
|   8986394 | 3036 | `			zCur++;` |
|   8986394 | 3037 | `			continue;` |
|         - | 3038 | `		}` |
| 327946280 | 3039 | `		if( zCur[0] != ' ' ){` |
| 288905647 | 3040 | `			bSeen = 1;` |
| 144040825 | 3041 | `		}` |
| 327946280 | 3042 | `		if( zCur[0] == '&' ){` |
|    674076 | 3043 | `			bRef = 1;` |
|    333695 | 3044 | `		}` |
| 327946280 | 3045 | `		if( zCur[0] == '.' && zCur[1] == '.' && zCur[2] == '.' ){` |
|         - | 3046 | ``			/* A `...` tail, not a numeric default's decimal point. */`` |
|    374831 | 3047 | `			bVar = 1;` |
|    187134 | 3048 | `		}` |
| 327946280 | 3049 | `		zCur++;` |
|         5 | 3050 | `	}` |
|  14991765 | 3051 | `	if( bTailRef && n > 0 && n <= 31 ){` |
|         - | 3052 | ``		/* A by-ref `&...` tail absorbs every later actual (array_multisort's`` |
|         - | 3053 | ``		 * `&...$rest`): without this, the deferred-argument resolver read the`` |
|         - | 3054 | ``		 * tail positions as by-VALUE and warned `Undefined variable` on an`` |
|         - | 3055 | `		 * undefined actual php binds silently. */` |
|     23583 | 3056 | `		mask \|= ~((1u << (n - 1)) - 1u);` |
|     11771 | 3057 | `	}` |
|  14991765 | 3058 | `	return mask;` |
|         5 | 3059 | `}` |
|      5619 | 3060 | `PH7_PRIVATE void VmSetBuiltinSignatures(ph7_vm *pVm)` |
|         5 | 3061 | `{` |
|         - | 3062 | `	sxu32 n;` |
|   6017954 | 3063 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinSig) ; n++ ){` |
|   9013685 | 3064 | `		SyHashEntry *pEntry = SyHashGet(&pVm->hHostFunction,` |
|   6012330 | 3065 | `			(const void *)aBuiltinSig[n].zName,(sxu32)SyStrlen(aBuiltinSig[n].zName));` |
|   6012335 | 3066 | `		if( pEntry ){` |
|   5978648 | 3067 | `			ph7_user_func *pFunc = (ph7_user_func *)pEntry->pUserData;` |
|   5978648 | 3068 | `			sxi16 nMin = 0, nMax = 0;` |
|   5978648 | 3069 | `			sxu8 bAtLeast = 0, bHasMax = 0;` |
|   5978648 | 3070 | `			pFunc->zSig = aBuiltinSig[n].zSig;` |
|   5978648 | 3071 | `			pFunc->zRet = aBuiltinSig[n].zRet[0] ? aBuiltinSig[n].zRet : 0;` |
|   5978648 | 3072 | `			pFunc->nByRefMask = VmDeriveByRefMaskFromSig(pFunc->zSig);` |
|   5978648 | 3073 | `			VmDeriveArityFromSig(pFunc->zSig,&nMin,&bAtLeast,&nMax,&bHasMax);` |
|         - | 3074 | `			/* The MAXIMUM always comes from the signature: the curated override` |
|         - | 3075 | `			 * table speaks only to the minimum (and its wording). */` |
|   5978648 | 3076 | `			pFunc->nMaxArg = nMax;` |
|   5978648 | 3077 | `			pFunc->bHasMaxArg = (sxu8)(VmBuiltinSelfValidatesArity(aBuiltinSig[n].zName) ? 0 : bHasMax);` |
|   5978648 | 3078 | `			if( pFunc->nMinArg < 1 ){` |
|         - | 3079 | `				/* VmSetBuiltinArity() ran first: a non-zero minimum here means the` |
|         - | 3080 | `				 * curated override already spoke for this builtin, so leave it. */` |
|   4304186 | 3081 | `				pFunc->nMinArg = nMin;` |
|   4304186 | 3082 | `				pFunc->bAtLeast = bAtLeast;` |
|   2140215 | 3083 | `			}` |
|   2976105 | 3084 | `		}` |
|   3001355 | 3085 | `	}` |
|      5624 | 3086 | `}` |
|         - | 3087 | `/*` |
|         - | 3088 | ` * Signature lookup by function name, for the reflection layer: embedded-PHP` |
|         - | 3089 | ` * builtins (max/min/Exception methods...) are ph7_vm_func instances, not` |
|         - | 3090 | ` * host functions, so they miss the VmSetBuiltinSignatures stamping and pull` |
|         - | 3091 | ` * their row on demand here. Linear scan — reflection-path only.` |
|         - | 3092 | ` */` |
|       ! 0 | 3093 | `PH7_PRIVATE const char * PH7_VmBuiltinSigLookup(const char *zName,sxu32 nLen,const char **pzRet)` |
|       ! 0 | 3094 | `{` |
|         - | 3095 | `	sxu32 n;` |
|       ! 0 | 3096 | `	if( pzRet ){` |
|       ! 0 | 3097 | `		*pzRet = 0;` |
|       ! 0 | 3098 | `	}` |
|       ! 0 | 3099 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinSig) ; n++ ){` |
|       ! 0 | 3100 | `		if( SyStrlen(aBuiltinSig[n].zName) == nLen` |
|       ! 0 | 3101 | `		 && SyMemcmp(aBuiltinSig[n].zName,zName,nLen) == 0 ){` |
|       ! 0 | 3102 | `			if( pzRet && aBuiltinSig[n].zRet[0] ){` |
|       ! 0 | 3103 | `				*pzRet = aBuiltinSig[n].zRet;` |
|       ! 0 | 3104 | `			}` |
|       ! 0 | 3105 | `			return aBuiltinSig[n].zSig;` |
|         - | 3106 | `		}` |
|       ! 0 | 3107 | `	}` |
|       ! 0 | 3108 | `	return 0;` |
|       ! 0 | 3109 | `}` |
|         - | 3110 | `/*` |
|         - | 3111 | ` * Write a value back to the caller's variable through a builtin argument's` |
|         - | 3112 | ` * stack-slot nIdx — the shared write-back for builtin by-reference` |
|         - | 3113 | ` * out-parameters (preg_match $matches, preg_replace &$count, similar_text` |
|         - | 3114 | ` * &$percent, ...).` |
|         - | 3115 | ` *` |
|         - | 3116 | ` * For a positional out-param argument the call compiler auto-vivifies known` |
|         - | 3117 | ` * by-reference out-params (see GenStateByRefBuiltinMask in compile.c), so a` |
|         - | 3118 | ` * bare undefined variable, an array subscript, and a declared/untyped` |
|         - | 3119 | ` * property all arrive with a real nIdx and are written back here, matching` |
|         - | 3120 | ` * PHP's reference semantics.` |
|         - | 3121 | ` *` |
|         - | 3122 | ` * nIdx stays SXU32_HIGH and the write-back to the caller is skipped (the` |
|         - | 3123 | ` * value still lands in the local stack slot) when the argument cannot expose` |
|         - | 3124 | ` * a stable memobj slot: a literal, a function-call result, a subscript of a` |
|         - | 3125 | ` * non-lvalue parent (foo()['k']), or any variable in a call that also uses` |
|         - | 3126 | ` * named or spread arguments (compile-time positions no longer map to the` |
|         - | 3127 | ` * runtime arg slots, so the compiler conservatively does not vivify). An` |
|         - | 3128 | ` * uninitialized typed property is also not wired (it throws before the` |
|         - | 3129 | ` * write) -- see the recorded deferrals.` |
|         - | 3130 | ` */` |
|     34093 | 3131 | `PH7_PRIVATE void PH7_VmStoreArgByRef(ph7_vm *pVm,ph7_value *pArg,ph7_value *pNewVal)` |
|         5 | 3132 | `{` |
|     34098 | 3133 | `	if( pArg->nIdx != SXU32_HIGH ){` |
|     34004 | 3134 | `		ph7_value *pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pArg->nIdx);` |
|     34004 | 3135 | `		if( pObj ){` |
|     34004 | 3136 | `			PH7_MemObjStore(pNewVal,pObj);` |
|     16997 | 3137 | `		}` |
|     16997 | 3138 | `	}` |
|     34098 | 3139 | `	PH7_MemObjStore(pNewVal,pArg);` |
|     34098 | 3140 | `}` |
|         - | 3141 | `/*` |
|         - | 3142 | ` * Raise an E_WARNING whose text is used VERBATIM.` |
|         - | 3143 | ` * ph7_context_throw_error_format() prepends "func(): " to whatever it is given, but php's` |
|         - | 3144 | ` * IO warnings put the offending path inside those parens -- "file_get_contents(/nope):` |
|         - | 3145 | ` * Failed to open stream: ..." -- so a caller that needs php's exact shape must build the` |
|         - | 3146 | ` * whole line itself and come through here.` |
|         - | 3147 | ` */` |
|         - | 3148 | `/*` |
|         - | 3149 | ` * Validate a callback argument and throw php's TypeError when it cannot be called.` |
|         - | 3150 | ` *` |
|         - | 3151 | ` * The shared dispatcher (PH7_VmCallUserFunctionWithMap) answers SXRET_OK with a NULL` |
|         - | 3152 | ` * result for an unresolvable callable, so every caller that did not check first failed` |
|         - | 3153 | ` * SILENTLY: call_user_func() returned NULL and usort() left the array sorted by nothing` |
|         - | 3154 | ` * at all. php rejects the ARGUMENT up front, naming exactly why.` |
|         - | 3155 | ` */` |
|    401868 | 3156 | `PH7_PRIVATE sxi32 PH7_CheckCallbackArg(` |
|         - | 3157 | `	ph7_context *pCtx,   /* Calling context (names the function in the message) */` |
|         - | 3158 | `	ph7_value *pCb,      /* The callback argument */` |
|         - | 3159 | `	int iArg,            /* Its 1-based position */` |
|         - | 3160 | `	const char *zParam,  /* Its php parameter name ("callback"), or 0 for the variadic` |
|         - | 3161 | `	                      * comparators php names by position only (array_udiff …) */` |
|         - | 3162 | `	int bNullable        /* TRUE when php's text says "or null" */` |
|         - | 3163 | `	)` |
|         5 | 3164 | `{` |
|         - | 3165 | `	char zReason[256];` |
|    401873 | 3166 | `	const char *zWhy = PH7_VmCallableReason(pCtx->pVm,pCb,zReason,sizeof(zReason));` |
|    401873 | 3167 | `	if( zWhy == 0 ){` |
|    401627 | 3168 | `		return PH7_OK;` |
|         - | 3169 | `	}` |
|       251 | 3170 | `	if( zParam ){` |
|       293 | 3171 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3172 | `			"%s(): Argument #%d ($%s) must be a valid callback%s, %s",` |
|        96 | 3173 | `			ph7_function_name(pCtx),iArg,zParam,bNullable ? " or null" : "",zWhy);` |
|         - | 3174 | `	}` |
|        86 | 3175 | `	return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3176 | `		"%s(): Argument #%d must be a valid callback%s, %s",` |
|        27 | 3177 | `		ph7_function_name(pCtx),iArg,bNullable ? " or null" : "",zWhy);` |
|    200928 | 3178 | `}` |
|     30879 | 3179 | `PH7_PRIVATE void PH7_VmThrowWarningFmt(ph7_vm *pVm,const char *zFmt,...)` |
|         5 | 3180 | `{` |
|         - | 3181 | `	va_list ap;` |
|     30884 | 3182 | `	va_start(ap,zFmt);` |
|     30884 | 3183 | `	PH7_VmThrowErrorAp(pVm,0,PH7_CTX_WARNING,zFmt,ap);` |
|     30884 | 3184 | `	va_end(ap);` |
|     30884 | 3185 | `}` |
|         - | 3186 | `/*` |
|         - | 3187 | ` * Emit a formatted E_USER_WARNING with no function-name prefix: php reports` |
|         - | 3188 | ` * #[\NoDiscard] as a USER warning (512) for the same reason it reports` |
|         - | 3189 | ` * #[\Deprecated] as a USER deprecation — the attribute is userland-authored,` |
|         - | 3190 | ` * and a set_error_handler sees the number.` |
|         - | 3191 | ` */` |
|        56 | 3192 | `static void VmThrowUserWarningFmt(ph7_vm *pVm,const char *zFmt,...)` |
|         1 | 3193 | `{` |
|         - | 3194 | `	va_list ap;` |
|        57 | 3195 | `	va_start(ap,zFmt);` |
|        57 | 3196 | `	PH7_VmThrowErrorAp(pVm,0,E_USER_WARNING,zFmt,ap);` |
|        57 | 3197 | `	va_end(ap);` |
|        57 | 3198 | `}` |
|         - | 3199 | `/*` |
|         - | 3200 | ` * Emit a formatted E_USER_DEPRECATED diagnostic with no function-name prefix:` |
|         - | 3201 | ` * php reports #[\Deprecated] as USER-deprecated (16384, it is userland-authored),` |
|         - | 3202 | ` * not the engine's E_DEPRECATED (8192) — handler-visible errno matters.` |
|         - | 3203 | ` */` |
|        38 | 3204 | `static void VmThrowUserDeprecatedFmt(ph7_vm *pVm,const char *zFmt,...)` |
|         2 | 3205 | `{` |
|         - | 3206 | `	va_list ap;` |
|        40 | 3207 | `	va_start(ap,zFmt);` |
|        40 | 3208 | `	PH7_VmThrowErrorAp(pVm,0,E_USER_DEPRECATED,zFmt,ap);` |
|        40 | 3209 | `	va_end(ap);` |
|        40 | 3210 | `}` |
|         - | 3211 | `/*` |
|         - | 3212 | ` * php 8.4 #[\Deprecated]: emit the E_USER_DEPRECATED notice for a call to a user` |
|         - | 3213 | ` * function/method carrying the attribute. Message shapes (php-exact):` |
|         - | 3214 | ` *   Function f() is deprecated` |
|         - | 3215 | ` *   Method C::m() is deprecated since 2.0, use g() instead` |
|         - | 3216 | ` * ("since" from the attribute's since: argument; the trailing ", msg" from` |
|         - | 3217 | ` * message:/positional #1.) The attribute arguments are compiled constant` |
|         - | 3218 | ` * expressions — evaluated via VmLocalExec, the reflection thunks' pattern.` |
|         - | 3219 | ` * Called from OP_CALL once per call, only when the callee HAS attributes;` |
|         - | 3220 | ` * pDeclClass names the method's declaring class (0 for plain functions).` |
|         - | 3221 | ` */` |
|         - | 3222 | `/*` |
|         - | 3223 | ` * Expand a global constant's value, emitting the php 8.5 #[\Deprecated]` |
|         - | 3224 | ` * E_USER_DEPRECATED notice first when the constant carries attributes` |
|         - | 3225 | `` * (`Constant GD is deprecated since 1.2` — every access re-warns).`` |
|         - | 3226 | ` */` |
|         - | 3227 | `static void VmDeprecatedAttrNoticeSubject(ph7_vm *pVm,SySet *pAttrs,` |
|         - | 3228 | `	const char *zKind,const SyString *pQual,const SyString *pName);` |
|         - | 3229 | `/*` |
|         - | 3230 | ` * Expand a constant, emitting the php 8.5 #[\Deprecated] E_USER_DEPRECATED notice` |
|         - | 3231 | `` * first when a USERLAND constant carries the attribute (`Constant GD is deprecated`` |
|         - | 3232 | `` * since 1.2` — every access re-warns). Engine constants php merely deprecates are`` |
|         - | 3233 | ` * not mimicked: PHL removes them outright (see the scope policy), so there is no` |
|         - | 3234 | ` * engine-side E_DEPRECATED list here.` |
|         - | 3235 | ` *` |
|         - | 3236 | `` * A `const NAME = <expr>;` statement compiles its initializer to a bytecode`` |
|         - | 3237 | ` * program and PH7_VmExpandConstantValue RUNS it — so the value was re-computed` |
|         - | 3238 | ` * on EVERY read. For anything with an identity or a side effect that is a wrong` |
|         - | 3239 | `` * answer, not a slow one: `const C = new Foo();` gave a DIFFERENT object each`` |
|         - | 3240 | `` * time (`C === C` was false, and `Foo::$count` counted one construction per`` |
|         - | 3241 | ` * read) where php evaluates the initializer once and hands the same value out` |
|         - | 3242 | ` * for ever. The first successful expansion is kept, and the constant becomes an` |
|         - | 3243 | ` * ordinary value-backed one — exactly the shape define() registers, so` |
|         - | 3244 | ` * redefinition frees it through the path that already existed.` |
|         - | 3245 | ` *` |
|         - | 3246 | ` * Not cached when the initializer did not complete: a throw, an exit(), or a` |
|         - | 3247 | ` * MUTED evaluation (php has not reached this code, so nothing may be observable)` |
|         - | 3248 | ` * must all be retried rather than frozen into a half-built value.` |
|         - | 3249 | ` */` |
|    283094 | 3250 | `PH7_PRIVATE sxi32 VmExpandConstantOnce(ph7_vm *pVm,ph7_constant *pCons,ph7_value *pOut)` |
|         5 | 3251 | `{` |
|         - | 3252 | `	const void *pResumeBefore,*pInlineBefore;` |
|         - | 3253 | `	sxi32 rc;` |
|    283099 | 3254 | `	if( pCons->xExpand != PH7_VmExpandConstantValue ){` |
|    282917 | 3255 | `		pCons->xExpand(pOut,pCons->pUserData);` |
|    282917 | 3256 | `		return SXRET_OK;` |
|         - | 3257 | `	}` |
|         - | 3258 | `	/* The initializer's own status. PH7_VmExpandConstantValue drops VmLocalExec's` |
|         - | 3259 | `	 * return code (ProcConstant answers void), so the program is driven from here` |
|         - | 3260 | `	 * instead — a caller with no way to see a throw would otherwise cache a` |
|         - | 3261 | `	 * half-built value and keep running past it. */` |
|       187 | 3262 | `	pResumeBefore = (const void *)pVm->pResumeFrame;` |
|       187 | 3263 | `	pInlineBefore = (const void *)pVm->pInlineInstr;` |
|       187 | 3264 | `	rc = VmLocalExec(pVm,(SySet *)pCons->pUserData,pOut,FALSE);` |
|       182 | 3265 | `	if( pVm->nMuteThrow > 0 \|\| rc == PH7_ABORT` |
|       161 | 3266 | `	 \|\| VmLocalExecThrew(pVm,rc,pResumeBefore,pInlineBefore) ){` |
|         - | 3267 | `		/* Did not complete — a throw, an exit(), or a MUTED evaluation (php has` |
|         - | 3268 | `		 * not reached this code, so nothing may be observable). Retry it next` |
|         - | 3269 | `		 * time rather than freezing a value the initializer never produced:` |
|         - | 3270 | ``		 * `const A = LATER; …; define('LATER',5);` must still answer 5. */`` |
|        35 | 3271 | `		return rc == SXRET_OK ? PH7_EXCEPTION : rc;` |
|         - | 3272 | `	}` |
|         - | 3273 | `	{` |
|       155 | 3274 | `		ph7_value *pKeep = (ph7_value *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|       155 | 3275 | `		if( pKeep == 0 ){` |
|       ! 0 | 3276 | `			return SXRET_OK; /* out of memory: stay lazy rather than fail the read */` |
|         - | 3277 | `		}` |
|       155 | 3278 | `		PH7_MemObjInit(pVm,pKeep);` |
|       155 | 3279 | `		PH7_MemObjStore(pOut,pKeep);` |
|       155 | 3280 | `		pCons->xExpand = VmExpandUserConstant;` |
|       155 | 3281 | `		pCons->pUserData = pKeep;` |
|         - | 3282 | `	}` |
|       155 | 3283 | `	return SXRET_OK;` |
|    139504 | 3284 | `}` |
|    151418 | 3285 | `PH7_PRIVATE void VmExpandConstantWithNotice(ph7_vm *pVm,ph7_constant *pCons,ph7_value *pOut)` |
|         5 | 3286 | `{` |
|         - | 3287 | `	/* An ENGINE constant php deprecated the symbol of says so when a program` |
|         - | 3288 | `	 * names it -- five of the six were silent here. Listing the table is not` |
|         - | 3289 | `	 * naming one, which is what bConstEnum says. */` |
|    151423 | 3290 | `	if( pCons->zDeprecated && !pVm->bConstEnum ){` |
|        65 | 3291 | `		VmErrorFormat(pVm,8192 /* E_DEPRECATED */,` |
|        21 | 3292 | `			"Constant %z is deprecated since %s",&pCons->sName,pCons->zDeprecated);` |
|        21 | 3293 | `	}` |
|    151423 | 3294 | `	if( SySetUsed(&pCons->aAttrs) > 0 ){` |
|         7 | 3295 | `		VmDeprecatedAttrNoticeSubject(pVm,&pCons->aAttrs,"Constant",0,&pCons->sName);` |
|         3 | 3296 | `	}` |
|    151423 | 3297 | `	VmExpandConstantOnce(pVm,pCons,pOut);` |
|    151423 | 3298 | `}` |
|         - | 3299 | `/*` |
|         - | 3300 | ` * Query a GLOBAL constant by its exact (case-sensitive) name and expand its` |
|         - | 3301 | ` * value into pOut, which the caller has initialized. Returns 1 when the` |
|         - | 3302 | ` * constant exists. The ini scanner's NORMAL/TYPED value interpretation is the` |
|         - | 3303 | ` * caller: php substitutes a defined constant's value for a bare identifier` |
|         - | 3304 | ` * token inside an unquoted ini value.` |
|         - | 3305 | ` */` |
|        44 | 3306 | `PH7_PRIVATE int PH7_VmQueryConstant(ph7_vm *pVm,const char *zName,sxu32 nName,ph7_value *pOut)` |
|         1 | 3307 | `{` |
|         - | 3308 | `	SyHashEntry *pEntry;` |
|         - | 3309 | `	ph7_constant *pCons;` |
|        45 | 3310 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)zName,nName);` |
|        45 | 3311 | `	if( pEntry == 0 ){` |
|        41 | 3312 | `		return 0;` |
|         - | 3313 | `	}` |
|         5 | 3314 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|         5 | 3315 | `	VmExpandConstantWithNotice(pVm,pCons,pOut);` |
|         5 | 3316 | `	return 1;` |
|        23 | 3317 | `}` |
|         - | 3318 | `/*` |
|         - | 3319 | ` * Scan a declared-attribute set for #[\Deprecated]; when found, evaluate its` |
|         - | 3320 | ` * message:/since: arguments (positional #0 = message, #1 = since) into the` |
|         - | 3321 | ` * caller's values and return TRUE. The base subject text ("Function f()",` |
|         - | 3322 | ` * "Constant C::K") is the caller's business.` |
|         - | 3323 | ` */` |
|       404 | 3324 | `static int VmDeprecatedAttrExtract(ph7_vm *pVm,SySet *pAttrs,` |
|         - | 3325 | `	ph7_value *pMsg,int *pbMsg,ph7_value *pSince,int *pbSince)` |
|         5 | 3326 | `{` |
|       409 | 3327 | `	ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|         - | 3328 | `	sxu32 n;` |
|       409 | 3329 | `	*pbMsg = *pbSince = 0;` |
|       779 | 3330 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; ++n ){` |
|       413 | 3331 | `		ph7_attribute *pAttr = &aAttr[n];` |
|         - | 3332 | `		ph7_attr_arg *aArg;` |
|       413 | 3333 | `		sxu32 i,nPos = 0;` |
|       408 | 3334 | `		if( SyStringLength(&pAttr->sName) != sizeof("Deprecated")-1` |
|       228 | 3335 | `		 \|\| SyStrnicmp(SyStringData(&pAttr->sName),"Deprecated",sizeof("Deprecated")-1) != 0 ){` |
|       375 | 3336 | `			continue;` |
|         - | 3337 | `		}` |
|        40 | 3338 | `		aArg = (ph7_attr_arg *)SySetBasePtr(&pAttr->aArgs);` |
|        58 | 3339 | `		for( i = 0 ; i < SySetUsed(&pAttr->aArgs) ; ++i ){` |
|        19 | 3340 | `			ph7_attr_arg *pArg = &aArg[i];` |
|        19 | 3341 | `			int isMsg = 0,isSince = 0;` |
|        19 | 3342 | `			if( SyStringLength(&pArg->sName) == 0 ){` |
|         3 | 3343 | `				isMsg = (nPos == 0);` |
|         3 | 3344 | `				isSince = (nPos == 1);` |
|         3 | 3345 | `				nPos++;` |
|        18 | 3346 | `			}else if( SyStringLength(&pArg->sName) == sizeof("message")-1` |
|        12 | 3347 | `			 && SyMemcmp(SyStringData(&pArg->sName),"message",sizeof("message")-1) == 0 ){` |
|         7 | 3348 | `				isMsg = 1;` |
|        14 | 3349 | `			}else if( SyStringLength(&pArg->sName) == sizeof("since")-1` |
|        11 | 3350 | `			 && SyMemcmp(SyStringData(&pArg->sName),"since",sizeof("since")-1) == 0 ){` |
|        11 | 3351 | `				isSince = 1;` |
|         5 | 3352 | `			}` |
|        19 | 3353 | `			if( isMsg && !*pbMsg && SySetUsed(&pArg->aByteCode) > 0 ){` |
|        13 | 3354 | `				if( VmLocalExec(pVm,&pArg->aByteCode,pMsg,FALSE) == SXRET_OK ){` |
|         9 | 3355 | `					if( (pMsg->iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 3356 | `						PH7_MemObjToString(pMsg);` |
|       ! 0 | 3357 | `					}` |
|         9 | 3358 | `					*pbMsg = 1;` |
|         5 | 3359 | `				}` |
|        15 | 3360 | `			}else if( isSince && !*pbSince && SySetUsed(&pArg->aByteCode) > 0 ){` |
|        11 | 3361 | `				if( VmLocalExec(pVm,&pArg->aByteCode,pSince,FALSE) == SXRET_OK ){` |
|        11 | 3362 | `					if( (pSince->iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 3363 | `						PH7_MemObjToString(pSince);` |
|       ! 0 | 3364 | `					}` |
|        11 | 3365 | `					*pbSince = 1;` |
|         5 | 3366 | `				}` |
|         5 | 3367 | `			}` |
|        10 | 3368 | `		}` |
|        40 | 3369 | `		return 1;` |
|       ! 0 | 3370 | `	}` |
|       371 | 3371 | `	return 0;` |
|       207 | 3372 | `}` |
|         - | 3373 | `/*` |
|         - | 3374 | ` * Append php's " since X" / ", message" suffixes to a built base subject and` |
|         - | 3375 | ` * emit the E_USER_DEPRECATED notice.` |
|         - | 3376 | ` */` |
|        38 | 3377 | `static void VmDeprecatedEmit(ph7_vm *pVm,SyBlob *pOut,` |
|         - | 3378 | `	ph7_value *pMsg,int bMsg,ph7_value *pSince,int bSince)` |
|         2 | 3379 | `{` |
|        40 | 3380 | `	if( bSince && SyBlobLength(&pSince->sBlob) > 0 ){` |
|        16 | 3381 | `		SyBlobFormat(pOut," since %.*s",(int)SyBlobLength(&pSince->sBlob),` |
|        10 | 3382 | `			(const char *)SyBlobData(&pSince->sBlob));` |
|         5 | 3383 | `	}` |
|        40 | 3384 | `	if( bMsg && SyBlobLength(&pMsg->sBlob) > 0 ){` |
|        13 | 3385 | `		SyBlobFormat(pOut,", %.*s",(int)SyBlobLength(&pMsg->sBlob),` |
|         8 | 3386 | `			(const char *)SyBlobData(&pMsg->sBlob));` |
|         4 | 3387 | `	}` |
|        40 | 3388 | `	VmThrowUserDeprecatedFmt(pVm,"%.*s",(int)SyBlobLength(pOut),(const char *)SyBlobData(pOut));` |
|        40 | 3389 | `}` |
|         - | 3390 | `/*` |
|         - | 3391 | ` * Generic #[\Deprecated] notice for a named subject:` |
|         - | 3392 | ` * "<Kind> [Qual::]Name is deprecated[ since X][, message]".` |
|         - | 3393 | ` */` |
|        20 | 3394 | `static void VmDeprecatedAttrNoticeSubject(ph7_vm *pVm,SySet *pAttrs,` |
|         - | 3395 | `	const char *zKind,const SyString *pQual,const SyString *pName)` |
|         2 | 3396 | `{` |
|         - | 3397 | `	ph7_value sMsg,sSince;` |
|         - | 3398 | `	SyBlob sOut;` |
|         - | 3399 | `	int bMsg,bSince;` |
|        22 | 3400 | `	PH7_MemObjInit(pVm,&sMsg);` |
|        22 | 3401 | `	PH7_MemObjInit(pVm,&sSince);` |
|        22 | 3402 | `	if( VmDeprecatedAttrExtract(pVm,pAttrs,&sMsg,&bMsg,&sSince,&bSince) ){` |
|        18 | 3403 | `		SyBlobInit(&sOut,&pVm->sAllocator);` |
|        18 | 3404 | `		if( pQual ){` |
|        14 | 3405 | `			SyBlobFormat(&sOut,"%s %z::%z is deprecated",zKind,pQual,pName);` |
|         8 | 3406 | `		}else{` |
|         5 | 3407 | `			SyBlobFormat(&sOut,"%s %z is deprecated",zKind,pName);` |
|         - | 3408 | `		}` |
|        18 | 3409 | `		VmDeprecatedEmit(pVm,&sOut,&sMsg,bMsg,&sSince,bSince);` |
|        18 | 3410 | `		SyBlobRelease(&sOut);` |
|         8 | 3411 | `	}` |
|        22 | 3412 | `	PH7_MemObjRelease(&sMsg);` |
|        22 | 3413 | `	PH7_MemObjRelease(&sSince);` |
|        22 | 3414 | `}` |
|         - | 3415 | `/*` |
|         - | 3416 | ` * The functions and methods php 8.x deprecated, and the clause each notice ends` |
|         - | 3417 | `` * with. Calling one raises `Function f() is deprecated since <clause>` (or`` |
|         - | 3418 | `` * `Method C::m() ...`) at E_DEPRECATED, BEFORE the callee's arity and type`` |
|         - | 3419 | `` * screens -- `curl_close()` with no argument warns first and throws the`` |
|         - | 3420 | ` * ArgumentCountError second -- and the export format's head reads the same fact` |
|         - | 3421 | `` * as `<internal, deprecated:curl>`.`` |
|         - | 3422 | ` *` |
|         - | 3423 | ` * Marked here rather than raised from each body because the notice belongs to` |
|         - | 3424 | ` * the CALL and not to what the body does (php warns and then runs it), and` |
|         - | 3425 | ` * because the subject is php's own: it names the DECLARING class even for a` |
|         - | 3426 | `` * call through a subclass, so a `MyStore extends SplObjectStorage` still reads`` |
|         - | 3427 | `` * `SplObjectStorage::attach()`. The stamp runs once, after every extension has`` |
|         - | 3428 | ` * installed, so a name a build does not carry is simply skipped.` |
|         - | 3429 | ` *` |
|         - | 3430 | ` * Only names this engine SHIPS are listed; php's own deprecated set is larger` |
|         - | 3431 | ` * (strftime, utf8_encode, the whole mhash family) and every one of those is a` |
|         - | 3432 | ` * name PHL does not have.` |
|         - | 3433 | ` */` |
|         - | 3434 | `static const ph7_deprecated_name aDeprecatedFunc[] = {` |
|         - | 3435 | `	{ "curl_close",        "8.5, as it has no effect since PHP 8.0" },` |
|         - | 3436 | `	/* ext/zip's whole procedural half, deprecated together in 8.0. php names a` |
|         - | 3437 | `	 * replacement for seven of the ten and none for the other three. */` |
|         - | 3438 | `	{ "zip_open",          "8.0, use ZipArchive::open() instead" },` |
|         - | 3439 | `	{ "zip_close",         "8.0, use ZipArchive::close() instead" },` |
|         - | 3440 | `	{ "zip_read",          "8.0, use ZipArchive::statIndex() instead" },` |
|         - | 3441 | `	{ "zip_entry_open",    "8.0" },` |
|         - | 3442 | `	{ "zip_entry_close",   "8.0" },` |
|         - | 3443 | `	{ "zip_entry_read",    "8.0, use ZipArchive::getFromIndex() instead" },` |
|         - | 3444 | `	{ "zip_entry_name",    "8.0, use ZipArchive::statIndex() instead" },` |
|         - | 3445 | `	{ "zip_entry_compressedsize", "8.0, use ZipArchive::statIndex() instead" },` |
|         - | 3446 | `	{ "zip_entry_filesize", "8.0, use ZipArchive::statIndex() instead" },` |
|         - | 3447 | `	{ "zip_entry_compressionmethod", "8.0, use ZipArchive::statIndex() instead" },` |
|         - | 3448 | `	{ "finfo_close",       "8.5, as finfo objects are freed automatically" },` |
|         - | 3449 | `	{ "curl_share_close",  "8.5, as it has no effect since PHP 8.0" },` |
|         - | 3450 | `	{ "DateInterval::__wakeup",` |
|         - | 3451 | `	  "8.5, this method is obsolete, as serialization hooks are provided by "` |
|         - | 3452 | `	  "__unserialize() and __serialize()" },` |
|         - | 3453 | `	{ "DatePeriod::__wakeup",` |
|         - | 3454 | `	  "8.5, this method is obsolete, as serialization hooks are provided by "` |
|         - | 3455 | `	  "__unserialize() and __serialize()" },` |
|         - | 3456 | `	{ "DateTime::__wakeup",` |
|         - | 3457 | `	  "8.5, this method is obsolete, as serialization hooks are provided by "` |
|         - | 3458 | `	  "__unserialize() and __serialize()" },` |
|         - | 3459 | `	{ "DateTimeInterface::__wakeup",` |
|         - | 3460 | `	  "8.5, this method is obsolete, as serialization hooks are provided by "` |
|         - | 3461 | `	  "__unserialize() and __serialize()" },` |
|         - | 3462 | `	{ "DateTimeImmutable::__wakeup",` |
|         - | 3463 | `	  "8.5, this method is obsolete, as serialization hooks are provided by "` |
|         - | 3464 | `	  "__unserialize() and __serialize()" },` |
|         - | 3465 | `	{ "DateTimeZone::__wakeup",` |
|         - | 3466 | `	  "8.5, this method is obsolete, as serialization hooks are provided by "` |
|         - | 3467 | `	  "__unserialize() and __serialize()" },` |
|         - | 3468 | `	{ "SplFixedArray::__wakeup",` |
|         - | 3469 | `	  "8.4, this method is obsolete, as serialization hooks are provided by "` |
|         - | 3470 | `	  "__unserialize() and __serialize()" },` |
|         - | 3471 | `	{ "SplFileInfo::_bad_state_ex", "8.2" },` |
|         - | 3472 | `	{ "SplObjectStorage::attach",` |
|         - | 3473 | `	  "8.5, use method SplObjectStorage::offsetSet() instead" },` |
|         - | 3474 | `	{ "SplObjectStorage::contains",` |
|         - | 3475 | `	  "8.5, use method SplObjectStorage::offsetExists() instead" },` |
|         - | 3476 | `	{ "SplObjectStorage::detach",` |
|         - | 3477 | `	  "8.5, use method SplObjectStorage::offsetUnset() instead" },` |
|         - | 3478 | `	{ "ReflectionFunction::isDisabled",` |
|         - | 3479 | `	  "8.0, as ReflectionFunction can no longer be constructed for disabled functions" },` |
|         - | 3480 | `	{ "ReflectionMethod::setAccessible", "8.5, as it has no effect since PHP 8.1" },` |
|         - | 3481 | `	{ "ReflectionProperty::setAccessible", "8.5, as it has no effect since PHP 8.1" },` |
|         - | 3482 | `	{ "ReflectionParameter::getClass",` |
|         - | 3483 | `	  "8.0, use ReflectionParameter::getType() instead" },` |
|         - | 3484 | `	{ "ReflectionParameter::isArray",` |
|         - | 3485 | `	  "8.0, use ReflectionParameter::getType() instead" },` |
|         - | 3486 | `	{ "ReflectionParameter::isCallable",` |
|         - | 3487 | `	  "8.0, use ReflectionParameter::getType() instead" },` |
|         - | 3488 | `};` |
|         - | 3489 | `/* The ph7_user_func behind one entry: a global builtin, or a native method's` |
|         - | 3490 | ` * own C body reached through its ph7_vm_func. */` |
|    168570 | 3491 | `static ph7_user_func * VmDeprecatedTarget(ph7_vm *pVm,const char *zName)` |
|         5 | 3492 | `{` |
|    168575 | 3493 | `	const char *zSep = 0;` |
|         - | 3494 | `	SyHashEntry *pEntry;` |
|         - | 3495 | `	sxu32 n;` |
|   2669030 | 3496 | `	for( n = 0 ; zName[n] != '\0' ; ++n ){` |
|   2595983 | 3497 | `		if( zName[n] == ':' && zName[n+1] == ':' ){` |
|     95528 | 3498 | `			zSep = &zName[n];` |
|     95528 | 3499 | `			break;` |
|         - | 3500 | `		}` |
|   1248230 | 3501 | `	}` |
|    168575 | 3502 | `	if( zSep == 0 ){` |
|     73052 | 3503 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zName,SyStrlen(zName));` |
|     73052 | 3504 | `		return pEntry ? (ph7_user_func *)pEntry->pUserData : 0;` |
|         - | 3505 | `	}` |
|         - | 3506 | `	{` |
|     95528 | 3507 | `		ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zName,(sxu32)(zSep - zName),FALSE,0);` |
|         - | 3508 | `		ph7_class_method *pMeth;` |
|     95528 | 3509 | `		if( pClass == 0 ){` |
|       ! 0 | 3510 | `			return 0;` |
|         - | 3511 | `		}` |
|     95528 | 3512 | `		pEntry = SyHashGet(&pClass->hMethod,(const void *)(zSep + 2),SyStrlen(zSep + 2));` |
|     95528 | 3513 | `		if( pEntry == 0 ){` |
|         2 | 3514 | `			return 0;` |
|         - | 3515 | `		}` |
|     95526 | 3516 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|     95526 | 3517 | `		return (pMeth->sFunc.iFlags & VM_FUNC_NATIVE) ? pMeth->sFunc.pNative : 0;` |
|         - | 3518 | `	}` |
|     84155 | 3519 | `}` |
|      5619 | 3520 | `PH7_PRIVATE void PH7_MarkDeprecatedFunctions(ph7_vm *pVm)` |
|         5 | 3521 | `{` |
|         - | 3522 | `	sxu32 n;` |
|    174194 | 3523 | `	for( n = 0 ; n < SX_ARRAYSIZE(aDeprecatedFunc) ; ++n ){` |
|    168575 | 3524 | `		ph7_user_func *pTarget = VmDeprecatedTarget(&(*pVm),aDeprecatedFunc[n].zName);` |
|    168575 | 3525 | `		if( pTarget ){` |
|    168573 | 3526 | `			pTarget->pDeprecated = &aDeprecatedFunc[n];` |
|     84149 | 3527 | `		}` |
|     84155 | 3528 | `	}` |
|      5624 | 3529 | `}` |
|         - | 3530 | `/*` |
|         - | 3531 | ` * The notice itself, raised from the one OP_CALL block a builtin and a native` |
|         - | 3532 | ` * method share. php words a qualified subject as a METHOD and a bare one as a` |
|         - | 3533 | ` * FUNCTION, which is exactly what the "::" in the recorded name says.` |
|         - | 3534 | ` */` |
|       146 | 3535 | `PH7_PRIVATE void PH7_VmDeprecatedCallNotice(ph7_vm *pVm,const ph7_deprecated_name *pDep)` |
|         2 | 3536 | `{` |
|       148 | 3537 | `	int bMethod = 0;` |
|         - | 3538 | `	sxu32 n;` |
|      2282 | 3539 | `	for( n = 0 ; pDep->zName[n] != '\0' ; ++n ){` |
|      2184 | 3540 | `		if( pDep->zName[n] == ':' ){` |
|        49 | 3541 | `			bMethod = 1;` |
|        49 | 3542 | `			break;` |
|         - | 3543 | `		}` |
|      1069 | 3544 | `	}` |
|       221 | 3545 | `	VmErrorFormat(&(*pVm),8192 /* E_DEPRECATED */,"%s %s() is deprecated since %s",` |
|       146 | 3546 | `		bMethod ? "Method" : "Function",pDep->zName,pDep->zWhy);` |
|       148 | 3547 | `}` |
|       384 | 3548 | `PH7_PRIVATE void VmDeprecatedAttrNotice(ph7_vm *pVm,ph7_vm_func *pFunc,ph7_class *pDeclClass)` |
|         5 | 3549 | `{` |
|         - | 3550 | `	ph7_value sMsg,sSince;` |
|         - | 3551 | `	SyBlob sOut;` |
|         - | 3552 | `	int bMsg,bSince;` |
|       389 | 3553 | `	PH7_MemObjInit(pVm,&sMsg);` |
|       389 | 3554 | `	PH7_MemObjInit(pVm,&sSince);` |
|       389 | 3555 | `	if( VmDeprecatedAttrExtract(pVm,&pFunc->aAttrs,&sMsg,&bMsg,&sSince,&bSince) ){` |
|        24 | 3556 | `		SyBlobInit(&sOut,&pVm->sAllocator);` |
|        24 | 3557 | `		if( pDeclClass ){` |
|         5 | 3558 | `			SyBlobFormat(&sOut,"Method %z::%z() is deprecated",&pDeclClass->sName,&pFunc->sName);` |
|         3 | 3559 | `		}else{` |
|        20 | 3560 | `			SyBlobFormat(&sOut,"Function %z() is deprecated",&pFunc->sName);` |
|         - | 3561 | `		}` |
|        24 | 3562 | `		VmDeprecatedEmit(pVm,&sOut,&sMsg,bMsg,&sSince,bSince);` |
|        24 | 3563 | `		SyBlobRelease(&sOut);` |
|        11 | 3564 | `	}` |
|       389 | 3565 | `	PH7_MemObjRelease(&sMsg);` |
|       389 | 3566 | `	PH7_MemObjRelease(&sSince);` |
|       389 | 3567 | `}` |
|         - | 3568 | `/*` |
|         - | 3569 | ` * php 8.5's #[\NoDiscard] warning, raised at the CALL, before the body runs, and` |
|         - | 3570 | ` * once per call (a loop warns every time round).` |
|         - | 3571 | ` *` |
|         - | 3572 | ` * The subject is a "function" unless the callee has a class scope, in which case` |
|         - | 3573 | ` * php names the DECLARING class -- an inherited method reports the class that` |
|         - | 3574 | `` * wrote it, and so does `parent::m()`. The tail after php's sentence is the`` |
|         - | 3575 | ` * attribute's own message: a constant EXPRESSION for a compiled declaration` |
|         - | 3576 | ` * (evaluated here, where the constants it may name exist) and a fixed string for` |
|         - | 3577 | ` * an internal member, which is how php words the immutable date mutators.` |
|         - | 3578 | ` */` |
|        56 | 3579 | `PH7_PRIVATE void VmNoDiscardWarn(ph7_vm *pVm,ph7_vm_func *pFunc,ph7_class *pDeclClass)` |
|         1 | 3580 | `{` |
|        57 | 3581 | `	ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(&pFunc->aAttrs);` |
|        57 | 3582 | `	const SyString *pName = &pFunc->sName;` |
|         - | 3583 | `	ph7_value sMsg;` |
|         - | 3584 | `	SyBlob sOut;` |
|        57 | 3585 | `	int bMsg = 0;` |
|         - | 3586 | `	sxu32 n;` |
|         - | 3587 | ``	/* A closure reports php's `{closure:SCOPE:LINE}` spelling, like every other`` |
|         - | 3588 | `	 * diagnostic that names one. */` |
|        57 | 3589 | `	if( SyStringLength(&pFunc->sClosureName) > 0 ){` |
|         7 | 3590 | `		pName = &pFunc->sClosureName;` |
|         3 | 3591 | `	}` |
|        57 | 3592 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|        57 | 3593 | `	if( pDeclClass ){` |
|        23 | 3594 | `		SyBlobFormat(&sOut,"The return value of method %z::%z() should either be used "` |
|        11 | 3595 | `			"or intentionally ignored by casting it as (void)",&pDeclClass->sName,pName);` |
|        12 | 3596 | `	}else{` |
|        35 | 3597 | `		SyBlobFormat(&sOut,"The return value of function %z() should either be used "` |
|        17 | 3598 | `			"or intentionally ignored by casting it as (void)",pName);` |
|         - | 3599 | `	}` |
|        57 | 3600 | `	PH7_MemObjInit(pVm,&sMsg);` |
|        57 | 3601 | `for( n = 0 ; !bMsg && n < SySetUsed(&pFunc->aAttrs) ; ++n ){` |
|         - | 3602 | `		ph7_attr_arg *aArg;` |
|        57 | 3603 | `		sxu32 i,nPos = 0;` |
|        56 | 3604 | `		if( SyStringLength(&aAttr[n].sName) != sizeof("NoDiscard")-1` |
|        57 | 3605 | `		 \|\| SyStrnicmp(SyStringData(&aAttr[n].sName),"NoDiscard",` |
|        28 | 3606 | `				sizeof("NoDiscard")-1) != 0 ){` |
|       ! 0 | 3607 | `			continue;` |
|         - | 3608 | `		}` |
|        57 | 3609 | `		aArg = (ph7_attr_arg *)SySetBasePtr(&aAttr[n].aArgs);` |
|        57 | 3610 | `		for( i = 0 ; i < SySetUsed(&aAttr[n].aArgs) ; ++i ){` |
|        11 | 3611 | `			ph7_attr_arg *pArg = &aArg[i];` |
|         - | 3612 | `			int isMsg;` |
|        11 | 3613 | `			if( SyStringLength(&pArg->sName) == 0 ){` |
|         7 | 3614 | `				isMsg = (nPos == 0);` |
|         7 | 3615 | `				nPos++;` |
|         4 | 3616 | `			}else{` |
|         7 | 3617 | `				isMsg = SyStringLength(&pArg->sName) == sizeof("message")-1` |
|         4 | 3618 | `					&& SyMemcmp(SyStringData(&pArg->sName),"message",` |
|         2 | 3619 | `						sizeof("message")-1) == 0;` |
|         - | 3620 | `			}` |
|        11 | 3621 | `			if( !isMsg ){` |
|       ! 0 | 3622 | `				continue;` |
|         - | 3623 | `			}` |
|         - | 3624 | `			/* A compiled declaration holds the message as a constant` |
|         - | 3625 | `			 * EXPRESSION (evaluated here, where the constants it may name` |
|         - | 3626 | `			 * exist); a native one holds it as a literal, like every other` |
|         - | 3627 | `			 * attribute argument a C-declared class carries. */` |
|        11 | 3628 | `			if( SySetUsed(&pArg->aByteCode) > 0 ){` |
|         9 | 3629 | `				if( VmLocalExec(pVm,&pArg->aByteCode,&sMsg,FALSE) != SXRET_OK ){` |
|       ! 0 | 3630 | `					continue;` |
|         1 | 3631 | `				}` |
|         7 | 3632 | `			}else if( pArg->pNativeValue ){` |
|         3 | 3633 | `				PH7_NativeLiteralValue(pVm,pArg->pNativeValue,&sMsg);` |
|         2 | 3634 | `			}else{` |
|       ! 0 | 3635 | `				continue;` |
|         - | 3636 | `			}` |
|        11 | 3637 | `			if( (sMsg.iFlags & MEMOBJ_STRING) == 0 ){` |
|         3 | 3638 | `				PH7_MemObjToString(&sMsg);` |
|         1 | 3639 | `			}` |
|        11 | 3640 | `			bMsg = 1;` |
|        11 | 3641 | `			break;` |
|       ! 0 | 3642 | `		}` |
|        57 | 3643 | `		break;` |
|       ! 0 | 3644 | `	}` |
|        57 | 3645 | `	if( bMsg && SyBlobLength(&sMsg.sBlob) > 0 ){` |
|        13 | 3646 | `		SyBlobFormat(&sOut,", %.*s",(int)SyBlobLength(&sMsg.sBlob),` |
|         8 | 3647 | `			(const char *)SyBlobData(&sMsg.sBlob));` |
|         4 | 3648 | `	}` |
|        57 | 3649 | `	PH7_MemObjRelease(&sMsg);` |
|         - | 3650 | `	/* php raises it as E_USER_WARNING (512), not the engine's E_WARNING: the` |
|         - | 3651 | `	 * attribute is userland-authored, the same reason #[\Deprecated] is` |
|         - | 3652 | `	 * E_USER_DEPRECATED. A set_error_handler sees the number. */` |
|        85 | 3653 | `	VmThrowUserWarningFmt(pVm,"%.*s",` |
|        56 | 3654 | `		(int)SyBlobLength(&sOut),(const char *)SyBlobData(&sOut));` |
|        57 | 3655 | `	SyBlobRelease(&sOut);` |
|        57 | 3656 | `}` |
|         - | 3657 | `/*` |
|         - | 3658 | ` * Same notice for a #[\Deprecated] class constant, at its static-access site:` |
|         - | 3659 | `` * php's `Constant C::K is deprecated` (each access re-warns).`` |
|         - | 3660 | ` */` |
|        14 | 3661 | `PH7_PRIVATE void VmDeprecatedConstNotice(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pMember)` |
|         2 | 3662 | `{` |
|        23 | 3663 | `	VmDeprecatedAttrNoticeSubject(pVm,&pMember->aAttrs,` |
|        14 | 3664 | `		(pMember->iFlags & PH7_CLASS_ATTR_ENUMCASE) ? "Enum case" : "Constant",` |
|        14 | 3665 | `		&pClass->sName,&pMember->sName);` |
|        16 | 3666 | `}` |
|         - | 3667 | `/*` |
|         - | 3668 | `` * The USER-VISIBLE string coercion a builtin performs on a `mixed` argument or`` |
|         - | 3669 | ` * array element: the builtin-side twin of PH7_MemObjToStringUV's opcode sites.` |
|         - | 3670 | ` * An ARRAY warns "Array to string conversion" and still renders as "Array"; an` |
|         - | 3671 | ` * object whose class has no __toString() -- or one whose __toString() threw --` |
|         - | 3672 | ` * is php's catchable "could not be converted to string" Error, and the builtin` |
|         - | 3673 | ` * must answer that instead of a value.` |
|         - | 3674 | ` *` |
|         - | 3675 | ` * On success pzData and pnLen receive the NUL-terminated bytes (both optional).` |
|         - | 3676 | ` * On a throw they are set to the empty string and the status is returned AND` |
|         - | 3677 | ` * recorded on the call context, so OP_CALL cannot mistake the call for a normal` |
|         - | 3678 | ` * return; a builtin that has already produced output (printf) still keeps it,` |
|         - | 3679 | ` * which is what php does.` |
|         - | 3680 | ` *` |
|         - | 3681 | ` * The public ph7_value_to_string() stays SILENT on purpose: it is the embedder` |
|         - | 3682 | ` * API, and the INTERNAL coercions behind it (array-key canonicalisation, sort` |
|         - | 3683 | ` * comparisons, print_r/var_export/serialize) must not throw -- php's do not` |
|         - | 3684 | ` * either.` |
|         - | 3685 | ` */` |
|    709984 | 3686 | `PH7_PRIVATE sxi32 PH7_ValueToStringUV(ph7_context *pCtx,ph7_value *pValue,const char **pzData,int *pnLen)` |
|         5 | 3687 | `{` |
|    709989 | 3688 | `	sxi32 rc = PH7_MemObjToStringUV(pValue);` |
|    709989 | 3689 | `	if( rc != SXRET_OK ){` |
|        73 | 3690 | `		if( pCtx ){` |
|        73 | 3691 | `			pCtx->nThrowRc = rc;` |
|        34 | 3692 | `		}` |
|        73 | 3693 | `		if( pzData ){` |
|        69 | 3694 | `			*pzData = "";` |
|        32 | 3695 | `		}` |
|        73 | 3696 | `		if( pnLen ){` |
|        69 | 3697 | `			*pnLen = 0;` |
|        32 | 3698 | `		}` |
|        73 | 3699 | `		return rc;` |
|         - | 3700 | `	}` |
|    709919 | 3701 | `	if( pzData \|\| pnLen ){` |
|    709891 | 3702 | `		const char *zData = ph7_value_to_string(pValue,pnLen);` |
|    709891 | 3703 | `		if( pzData ){` |
|    709891 | 3704 | `			*pzData = zData;` |
|    354187 | 3705 | `		}` |
|    354187 | 3706 | `	}` |
|    709919 | 3707 | `	return SXRET_OK;` |
|    354240 | 3708 | `}` |
|         - | 3709 | `/*` |
|         - | 3710 | ` * The same user-visible coercion for a builtin that php does NOT stop for.` |
|         - | 3711 | ` * zend's zval_get_string leaves the empty string behind when it throws and the` |
|         - | 3712 | `` * C function carries on, so `str_replace()`'s `&$count` still comes back written`` |
|         - | 3713 | ` * from a call that threw. Only the FIRST un-stringable value raises -- a second` |
|         - | 3714 | ` * one would land two Errors for one call -- while every OTHER kind of value is` |
|         - | 3715 | ` * converted normally either way (an array still warns, a scalar still spells` |
|         - | 3716 | ` * itself out), which is what keeps the elements AFTER the failure intact.` |
|         - | 3717 | ` *` |
|         - | 3718 | ` * pzData/pnLen always come back usable, so the caller has nothing to check.` |
|         - | 3719 | ` */` |
|    219381 | 3720 | `PH7_PRIVATE void PH7_ValueToStringUVOnce(ph7_context *pCtx,ph7_value *pValue,` |
|         - | 3721 | `	const char **pzData,int *pnLen)` |
|         5 | 3722 | `{` |
|    219386 | 3723 | `	if( pCtx && pCtx->nThrowRc != 0 && PH7_MemObjIsNotStringable(pValue) ){` |
|       ! 0 | 3724 | `		if( pzData ){ *pzData = ""; }` |
|       ! 0 | 3725 | `		if( pnLen ){ *pnLen = 0; }` |
|       ! 0 | 3726 | `		return;` |
|         - | 3727 | `	}` |
|    219386 | 3728 | `	(void)PH7_ValueToStringUV(pCtx,pValue,pzData,pnLen);` |
|    109342 | 3729 | `}` |
|         - | 3730 | `/*` |
|         - | 3731 | ` * The same coercion again, for a builtin that must FINISH ITS OUTPUT before the` |
|         - | 3732 | ` * Error is raised. php's C functions carry on past the throw and the bytes they` |
|         - | 3733 | ` * write reach the stream BEFORE the exception surfaces:` |
|         - | 3734 | ``  * `file_put_contents($f,['A',$obj,'B'])` leaves "AB" behind and `fputcsv()` `` |
|         - | 3735 | ` * writes its whole line with an empty field. A throw raised from inside a` |
|         - | 3736 | ` * builtin HERE runs the enclosing catch immediately, so raising in place would` |
|         - | 3737 | ` * put the catch's own output in front of the builtin's.` |
|         - | 3738 | ` *` |
|         - | 3739 | ` * Answers the class that could not be converted (and the empty string with it),` |
|         - | 3740 | ` * leaving the caller to raise once its writing is done; 0 when the value` |
|         - | 3741 | ` * converted, which is every other kind -- an array still warns here, in place,` |
|         - | 3742 | ` * exactly as php's does.` |
|         - | 3743 | ` */` |
|     19457 | 3744 | `PH7_PRIVATE ph7_class *PH7_ValueToStringUVDefer(ph7_context *pCtx,ph7_value *pValue,` |
|         - | 3745 | `	const char **pzData,int *pnLen)` |
|         5 | 3746 | `{` |
|     19462 | 3747 | `	if( PH7_MemObjIsNotStringable(pValue) ){` |
|         7 | 3748 | `		ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|         7 | 3749 | `		if( pzData ){ *pzData = ""; }` |
|         7 | 3750 | `		if( pnLen ){ *pnLen = 0; }` |
|         7 | 3751 | `		return pInst ? pInst->pClass : 0;` |
|         - | 3752 | `	}` |
|     19456 | 3753 | `	(void)PH7_ValueToStringUV(pCtx,pValue,pzData,pnLen);` |
|     19456 | 3754 | `	return 0;` |
|      9729 | 3755 | `}` |
|         - | 3756 |  |
