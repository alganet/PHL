# src/ph7/vm_arg_check.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1106/1165 lines (94.94%)

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
|         - |   90 | `	{ "mb_encoding_aliases",       1, 0 },` |
|         - |   91 | `	{ "mb_ord",                    1, 1 },` |
|         - |   92 | `	{ "mb_str_split",              1, 1 },` |
|         - |   93 | `	{ "mb_stripos",                2, 1 },` |
|         - |   94 | `	{ "mb_strlen",                 1, 1 },` |
|         - |   95 | `	{ "mb_strpos",                 2, 1 },` |
|         - |   96 | `	{ "mb_strrpos",                2, 1 },` |
|         - |   97 | `	{ "mb_strtolower",             1, 1 },` |
|         - |   98 | `	{ "mb_strtoupper",             1, 1 },` |
|         - |   99 | `	{ "mb_strwidth",               1, 1 },` |
|         - |  100 | `	{ "mb_substr",                 2, 1 },` |
|         - |  101 | `	{ "nl2br",                     1, 1 },` |
|         - |  102 | `	{ "printf",                    1, 1 },` |
|         - |  103 | `	{ "quotemeta",                 1, 0 },` |
|         - |  104 | `	{ "rtrim",                     1, 1 },` |
|         - |  105 | `	{ "soundex",                   1, 0 },` |
|         - |  106 | `	{ "sprintf",                   1, 1 },` |
|         - |  107 | `	{ "str_getcsv",                1, 1 },` |
|         - |  108 | `	{ "str_shuffle",               1, 0 },` |
|         - |  109 | `	{ "strcasecmp",                2, 0 },` |
|         - |  110 | `	{ "strchr",                    2, 1 },` |
|         - |  111 | `	{ "strcmp",                    2, 0 },` |
|         - |  112 | `	{ "strnatcasecmp",             2, 0 },` |
|         - |  113 | `	{ "strnatcmp",                 2, 0 },` |
|         - |  114 | `	{ "strcoll",                   2, 0 },` |
|         - |  115 | `	{ "strip_tags",                1, 1 },` |
|         - |  116 | `	{ "stripslashes",              1, 0 },` |
|         - |  117 | `	{ "strlen",                    1, 0 },` |
|         - |  118 | `	{ "strrev",                    1, 0 },` |
|         - |  119 | `	{ "strtok",                    1, 1 },` |
|         - |  120 | `	{ "strtolower",                1, 0 },` |
|         - |  121 | `	{ "strtoupper",                1, 0 },` |
|         - |  122 | `	{ "strtr",                     2, 0 },` |
|         - |  123 | `	{ "trim",                      1, 1 },` |
|         - |  124 | `	{ "ucfirst",                   1, 0 },` |
|         - |  125 | `	{ "ucwords",                   1, 1 },` |
|         - |  126 | `	{ "vfprintf",                  3, 0 },` |
|         - |  127 | `	{ "vprintf",                   2, 0 },` |
|         - |  128 | `	{ "vsprintf",                  2, 0 },` |
|         - |  129 | `	{ "wordwrap",                  1, 1 },` |
|         - |  130 | `	/* Ctype family */` |
|         - |  131 | `	{ "ctype_alnum",               1, 0 },` |
|         - |  132 | `	{ "ctype_alpha",               1, 0 },` |
|         - |  133 | `	{ "ctype_cntrl",               1, 0 },` |
|         - |  134 | `	{ "ctype_digit",               1, 0 },` |
|         - |  135 | `	{ "ctype_graph",               1, 0 },` |
|         - |  136 | `	{ "ctype_lower",               1, 0 },` |
|         - |  137 | `	{ "ctype_print",               1, 0 },` |
|         - |  138 | `	{ "ctype_punct",               1, 0 },` |
|         - |  139 | `	{ "ctype_space",               1, 0 },` |
|         - |  140 | `	{ "ctype_upper",               1, 0 },` |
|         - |  141 | `	{ "ctype_xdigit",              1, 0 },` |
|         - |  142 | `	/* Math family */` |
|         - |  143 | `	{ "base_convert",              3, 0 },` |
|         - |  144 | `	{ "cos",                       1, 0 },` |
|         - |  145 | `	{ "cosh",                      1, 0 },` |
|         - |  146 | `	{ "crc32",                     1, 0 },` |
|         - |  147 | `	{ "decbin",                    1, 0 },` |
|         - |  148 | `	{ "dechex",                    1, 0 },` |
|         - |  149 | `	{ "decoct",                    1, 0 },` |
|         - |  150 | `	{ "exp",                       1, 0 },` |
|         - |  151 | `	{ "log10",                     1, 0 },` |
|         - |  152 | `	{ "md5",                       1, 1 },` |
|         - |  153 | `	{ "iconv",                     3, 0 },` |
|         - |  154 | `	{ "iconv_strlen",              1, 1 },` |
|         - |  155 | `	{ "iconv_substr",              2, 1 },` |
|         - |  156 | `	{ "iconv_strpos",              2, 1 },` |
|         - |  157 | `	{ "iconv_strrpos",             2, 1 },` |
|         - |  158 | `	{ "iconv_mime_encode",         2, 1 },` |
|         - |  159 | `	{ "iconv_mime_decode",         1, 1 },` |
|         - |  160 | `	{ "iconv_mime_decode_headers", 1, 1 },` |
|         - |  161 | `	{ "round",                     1, 1 },` |
|         - |  162 | `	{ "sha1",                      1, 1 },` |
|         - |  163 | `	{ "sin",                       1, 0 },` |
|         - |  164 | `	{ "sinh",                      1, 0 },` |
|         - |  165 | `	{ "sqrt",                      1, 0 },` |
|         - |  166 | `	{ "tan",                       1, 0 },` |
|         - |  167 | `	{ "tanh",                      1, 0 },` |
|         - |  168 | `	/* Type/var family */` |
|         - |  169 | `	{ "doubleval",                 1, 0 },` |
|         - |  170 | `	{ "floatval",                  1, 0 },` |
|         - |  171 | `	{ "get_resource_id",           1, 0 },` |
|         - |  172 | `	{ "get_resource_type",         1, 0 },` |
|         - |  173 | `	{ "gettype",                   1, 0 },` |
|         - |  174 | `	{ "intval",                    1, 1 },` |
|         - |  175 | `	{ "is_array",                  1, 0 },` |
|         - |  176 | `	{ "is_bool",                   1, 0 },` |
|         - |  177 | `	{ "is_callable",               1, 1 },` |
|         - |  178 | `	{ "is_double",                 1, 0 },` |
|         - |  179 | `	{ "is_float",                  1, 0 },` |
|         - |  180 | `	{ "is_int",                    1, 0 },` |
|         - |  181 | `	{ "is_integer",                1, 0 },` |
|         - |  182 | `	{ "is_long",                   1, 0 },` |
|         - |  183 | `	{ "is_null",                   1, 0 },` |
|         - |  184 | `	{ "is_numeric",                1, 0 },` |
|         - |  185 | `	{ "is_object",                 1, 0 },` |
|         - |  186 | `	{ "is_resource",               1, 0 },` |
|         - |  187 | `	{ "is_scalar",                 1, 0 },` |
|         - |  188 | `	{ "is_string",                 1, 0 },` |
|         - |  189 | `	{ "print_r",                   1, 1 },` |
|         - |  190 | `	{ "strval",                    1, 0 },` |
|         - |  191 | `	{ "var_dump",                  1, 1 },` |
|         - |  192 | `	{ "var_export",                1, 1 },` |
|         - |  193 | `	/* Array/iterator family */` |
|         - |  194 | `	{ "array_filter",              1, 1 },` |
|         - |  195 | `	{ "array_product",             1, 0 },` |
|         - |  196 | `	{ "array_rand",                1, 1 },` |
|         - |  197 | `	{ "compact",                   1, 1 },` |
|         - |  198 | `	{ "current",                   1, 0 },` |
|         - |  199 | `	{ "end",                       1, 0 },` |
|         - |  200 | `	{ "extract",                   1, 1 },` |
|         - |  201 | `	{ "iterator_apply",            2, 1 },` |
|         - |  202 | `	{ "iterator_count",            1, 0 },` |
|         - |  203 | `	{ "iterator_to_array",         1, 1 },` |
|         - |  204 | `	{ "key",                       1, 0 },` |
|         - |  205 | `	{ "krsort",                    1, 1 },` |
|         - |  206 | `	{ "ksort",                     1, 1 },` |
|         - |  207 | `	{ "next",                      1, 0 },` |
|         - |  208 | `	{ "pos",                       1, 0 },` |
|         - |  209 | `	{ "prev",                      1, 0 },` |
|         - |  210 | `	{ "reset",                     1, 0 },` |
|         - |  211 | `	{ "rsort",                     1, 1 },` |
|         - |  212 | `	{ "shuffle",                   1, 0 },` |
|         - |  213 | `	{ "sort",                      1, 1 },` |
|         - |  214 | `	{ "uasort",                    2, 0 },` |
|         - |  215 | `	{ "uksort",                    2, 0 },` |
|         - |  216 | `	{ "usort",                     2, 0 },` |
|         - |  217 | `	/* Class/reflection family */` |
|         - |  218 | `	{ "class_alias",               2, 1 },` |
|         - |  219 | `	{ "class_exists",              1, 1 },` |
|         - |  220 | `	{ "enum_exists",               1, 1 },` |
|         - |  221 | `	{ "get_class_methods",         1, 0 },` |
|         - |  222 | `	{ "get_class_vars",            1, 0 },` |
|         - |  223 | `	{ "get_object_vars",           1, 0 },` |
|         - |  224 | `	{ "interface_exists",          1, 1 },` |
|         - |  225 | `	{ "trait_exists",              1, 1 },` |
|         - |  226 | `	{ "is_a",                      2, 1 },` |
|         - |  227 | `	{ "is_subclass_of",            2, 1 },` |
|         - |  228 | `	{ "method_exists",             2, 0 },` |
|         - |  229 | `	{ "property_exists",           2, 0 },` |
|         - |  230 | `	{ "spl_autoload",              1, 1 },` |
|         - |  231 | `	{ "spl_autoload_unregister",   1, 0 },` |
|         - |  232 | `	{ "spl_object_hash",           1, 0 },` |
|         - |  233 | `	{ "spl_object_id",             1, 0 },` |
|         - |  234 | `	/* Filesystem/IO family */` |
|         - |  235 | `	{ "basename",                  1, 1 },` |
|         - |  236 | `	{ "chdir",                     1, 0 },` |
|         - |  237 | `	{ "chgrp",                     2, 0 },` |
|         - |  238 | `	{ "dir",                       1, 1 },` |
|         - |  239 | `	{ "dirname",                   1, 1 },` |
|         - |  240 | `	{ "disk_free_space",           1, 0 },` |
|         - |  241 | `	{ "disk_total_space",          1, 0 },` |
|         - |  242 | `	{ "diskfreespace",             1, 0 },` |
|         - |  243 | `	{ "fclose",                    1, 0 },` |
|         - |  244 | `	{ "feof",                      1, 0 },` |
|         - |  245 | `	{ "fflush",                    1, 0 },` |
|         - |  246 | `	{ "fgetc",                     1, 0 },` |
|         - |  247 | `	{ "fgetcsv",                   1, 1 },` |
|         - |  248 | `	{ "file",                      1, 1 },` |
|         - |  249 | `	{ "file_exists",               1, 0 },` |
|         - |  250 | `	{ "fileatime",                 1, 0 },` |
|         - |  251 | `	{ "filectime",                 1, 0 },` |
|         - |  252 | `	{ "filemtime",                 1, 0 },` |
|         - |  253 | `	{ "filesize",                  1, 0 },` |
|         - |  254 | `	{ "filetype",                  1, 0 },` |
|         - |  255 | `	{ "flock",                     2, 1 },` |
|         - |  256 | `	{ "fpassthru",                 1, 0 },` |
|         - |  257 | `	{ "fputcsv",                   2, 1 },` |
|         - |  258 | `	{ "fputs",                     2, 1 },` |
|         - |  259 | `	{ "fseek",                     2, 1 },` |
|         - |  260 | `	{ "fstat",                     1, 0 },` |
|         - |  261 | `	/* ext/zlib's aliases of the six above; the alias needs its own row or the` |
|         - |  262 | `	 * ArgumentCountError names the function it is an alias OF. */` |
|         - |  263 | `	{ "gzclose",                   1, 0 },` |
|         - |  264 | `	{ "gzeof",                     1, 0 },` |
|         - |  265 | `	{ "gzgetc",                    1, 0 },` |
|         - |  266 | `	{ "gzpassthru",                1, 0 },` |
|         - |  267 | `	{ "gzputs",                    2, 1 },` |
|         - |  268 | `	{ "gzrewind",                  1, 0 },` |
|         - |  269 | `	{ "gzseek",                    2, 1 },` |
|         - |  270 | `	{ "gztell",                    1, 0 },` |
|         - |  271 | `	{ "gzwrite",                   2, 1 },` |
|         - |  272 | `	{ "ftell",                     1, 0 },` |
|         - |  273 | `	{ "ftruncate",                 2, 0 },` |
|         - |  274 | `	{ "getopt",                    1, 1 },` |
|         - |  275 | `	{ "is_dir",                    1, 0 },` |
|         - |  276 | `	{ "is_executable",             1, 0 },` |
|         - |  277 | `	{ "is_file",                   1, 0 },` |
|         - |  278 | `	{ "is_link",                   1, 0 },` |
|         - |  279 | `	{ "is_readable",               1, 0 },` |
|         - |  280 | `	{ "is_writable",               1, 0 },` |
|         - |  281 | `	{ "lstat",                     1, 0 },` |
|         - |  282 | `	{ "md5_file",                  1, 1 },` |
|         - |  283 | `	{ "opendir",                   1, 1 },` |
|         - |  284 | `	{ "pathinfo",                  1, 1 },` |
|         - |  285 | `	{ "pclose",                    1, 0 },` |
|         - |  286 | `	{ "readlink",                  1, 0 },` |
|         - |  287 | `	{ "realpath",                  1, 0 },` |
|         - |  288 | `	{ "stream_resolve_include_path",1, 0 },` |
|         - |  289 | `	{ "rewind",                    1, 0 },` |
|         - |  290 | `	{ "sha1_file",                 1, 1 },` |
|         - |  291 | `	{ "stat",                      1, 0 },` |
|         - |  292 | `	/* Date family */` |
|         - |  293 | `	{ "date",                      1, 1 },` |
|         - |  294 | `	{ "date_default_timezone_set", 1, 1 },` |
|         - |  295 | `	{ "date_sun_info",             3, 0 },` |
|         - |  296 | `	{ "date_sunrise",              1, 1 },` |
|         - |  297 | `	{ "date_sunset",               1, 1 },` |
|         - |  298 | `	{ "gmdate",                    1, 1 },` |
|         - |  299 | `	{ "gmmktime",                  1, 1 },` |
|         - |  300 | `	{ "idate",                     1, 1 },` |
|         - |  301 | `	{ "mktime",                    1, 1 },` |
|         - |  302 | `	/* Encoding/URL family */` |
|         - |  303 | `	{ "base64_decode",             1, 1 },` |
|         - |  304 | `	{ "base64_encode",             1, 0 },` |
|         - |  305 | `	{ "convert_uudecode",          1, 0 },` |
|         - |  306 | `	{ "convert_uuencode",          1, 0 },` |
|         - |  307 | `	{ "parse_ini_file",            1, 1 },` |
|         - |  308 | `	{ "parse_ini_string",          1, 1 },` |
|         - |  309 | `	{ "parse_url",                 1, 1 },` |
|         - |  310 | `	{ "rawurldecode",              1, 0 },` |
|         - |  311 | `	{ "rawurlencode",              1, 0 },` |
|         - |  312 | `	{ "urldecode",                 1, 0 },` |
|         - |  313 | `	{ "urlencode",                 1, 0 },` |
|         - |  314 | `	/* JSON/serialize family */` |
|         - |  315 | `	{ "filter_var",                1, 1 },` |
|         - |  316 | `	{ "json_decode",               1, 1 },` |
|         - |  317 | `	{ "json_encode",               1, 1 },` |
|         - |  318 | `	{ "json_validate",             1, 1 },` |
|         - |  319 | `	{ "serialize",                 1, 0 },` |
|         - |  320 | `	{ "unserialize",               1, 1 },` |
|         - |  321 | `	/* PCRE family */` |
|         - |  322 | `	{ "preg_match",                2, 1 },` |
|         - |  323 | `	{ "preg_match_all",            2, 1 },` |
|         - |  324 | `	{ "preg_quote",                1, 1 },` |
|         - |  325 | `	{ "preg_replace",              3, 1 },` |
|         - |  326 | `	{ "preg_replace_callback",     3, 1 },` |
|         - |  327 | `	{ "preg_split",                2, 1 },` |
|         - |  328 | `	/* XML family */` |
|         - |  329 | `	/* Constants/misc family */` |
|         - |  330 | `	{ "call_user_func",            1, 1 },` |
|         - |  331 | `	{ "call_user_func_array",      2, 0 },` |
|         - |  332 | `	{ "constant",                  1, 0 },` |
|         - |  333 | `	{ "define",                    2, 1 },` |
|         - |  334 | `	{ "defined",                   1, 0 },` |
|         - |  335 | `	{ "error_log",                 1, 1 },` |
|         - |  336 | `	{ "fnmatch",                   2, 1 },` |
|         - |  337 | `	{ "forward_static_call",       1, 1 },` |
|         - |  338 | `	{ "forward_static_call_array", 2, 0 },` |
|         - |  339 | `	{ "func_get_arg",              1, 0 },` |
|         - |  340 | `	{ "function_exists",           1, 0 },` |
|         - |  341 | `	{ "header",                    1, 1 },` |
|         - |  342 | `	{ "password_get_info",         1, 0 },` |
|         - |  343 | `	{ "putenv",                    1, 0 },` |
|         - |  344 | `	{ "register_shutdown_function", 1, 1 },` |
|         - |  345 | `	{ "set_error_handler",         1, 1 },` |
|         - |  346 | `	{ "set_exception_handler",     1, 0 },` |
|         - |  347 | `	{ "setcookie",                 1, 1 },` |
|         - |  348 | `	{ "setrawcookie",              1, 1 },` |
|         - |  349 | `	{ "trigger_error",             1, 1 },` |
|         - |  350 | `	{ "user_error",                1, 1 },` |
|         - |  351 | `	/*` |
|         - |  352 | `	 * Overrides for signatures that under-report their own minimum: the callback` |
|         - |  353 | `	 * of these three hides inside the variadic tail ("array $array, ...$rest"),` |
|         - |  354 | `	 * so the derivation reads 1 where php requires 2.` |
|         - |  355 | `	 */` |
|         - |  356 | `	{ "array_udiff",               2, 1 },` |
|         - |  357 | `	{ "array_uintersect",          2, 1 },` |
|         - |  358 | `	{ "array_diff_uassoc",         2, 1 },` |
|         - |  359 | `	{ "array_diff_ukey",           2, 1 },` |
|         - |  360 | `	{ "array_intersect_ukey",      2, 1 },` |
|         - |  361 | `	{ "array_intersect_uassoc",    2, 1 },` |
|         - |  362 | `	{ "array_udiff_assoc",         2, 1 },` |
|         - |  363 | `	{ "array_uintersect_assoc",    2, 1 },` |
|         - |  364 | `	{ "array_udiff_uassoc",        3, 1 },` |
|         - |  365 | `	{ "array_uintersect_uassoc",   3, 1 },` |
|         - |  366 | `};` |
|         - |  367 | `/*` |
|         - |  368 | ` * Stamp the minimum-arity metadata from aBuiltinArity[] onto the already` |
|         - |  369 | ` * registered host functions. Called once at VM init after every builtin family` |
|         - |  370 | ` * has been installed into hHostFunction. A name absent from the hash (e.g. a` |
|         - |  371 | ` * build without a given extension) is simply skipped.` |
|         - |  372 | ` */` |
|      6985 |  373 | `PH7_PRIVATE void VmSetBuiltinArity(ph7_vm *pVm)` |
|         5 |  374 | `{` |
|         - |  375 | `	sxu32 n;` |
|   2123445 |  376 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinArity) ; ++n ){` |
|   2116460 |  377 | `		const struct VmBuiltinArity *p = &aBuiltinArity[n];` |
|   4232915 |  378 | `		SyHashEntry *pEntry = SyHashGet(&pVm->hHostFunction,` |
|   2116455 |  379 | `			(const void *)p->zName,SyStrlen(p->zName));` |
|   2116460 |  380 | `		if( pEntry ){` |
|   2116460 |  381 | `			ph7_user_func *pFunc = (ph7_user_func *)pEntry->pUserData;` |
|   2116460 |  382 | `			pFunc->nMinArg  = p->nMin;` |
|   2116460 |  383 | `			pFunc->bAtLeast = p->bAtLeast;` |
|   1056561 |  384 | `		}` |
|   1056566 |  385 | `	}` |
|      6990 |  386 | `}` |
|         - |  387 | `/*` |
|         - |  388 | ` * PHP 8.5 parameter signatures for the C builtins, generated offline from` |
|         - |  389 | ` * a real PHP 8.5 ReflectionFunction dump over PHL's registered function` |
|         - |  390 | ` * list (see the plan's Reflection section). "= ?" marks an optional` |
|         - |  391 | ` * parameter whose default is not representable as a short literal.` |
|         - |  392 | ` * Reflection parses these strings on demand; unlisted builtins degrade to` |
|         - |  393 | ` * the min-arity data.` |
|         - |  394 | ` */` |
|         - |  395 | `static const struct VmBuiltinSig {` |
|         - |  396 | `	const char *zName;` |
|         - |  397 | `	const char *zSig;` |
|         - |  398 | `	const char *zRet;` |
|         - |  399 | `} aBuiltinSig[] = {` |
|         - |  400 | `	/* The subsystems converted from embedded PHP into C (INI, libxml, sessions).` |
|         - |  401 | `	 * A prelude function declared its parameters in PHP and Reflection read them` |
|         - |  402 | `	 * from there; a C builtin has no declaration but this table, so without a row` |
|         - |  403 | `	 * here the same function reports NO parameters -- and loses its arity bounds` |
|         - |  404 | `	 * with them. */` |
|         - |  405 | `	/* ext/curl. Signatures dumped from php 8.5's own ReflectionFunction, which` |
|         - |  406 | `	 * is also where the parameter NAMES come from: a named argument spells the` |
|         - |  407 | `	 * php one, so an invented name breaks valid php. */` |
|         - |  408 | `	{ "curl_close", "CurlHandle $handle", "void" },` |
|         - |  409 | `	{ "curl_copy_handle", "CurlHandle $handle", "CurlHandle\|false" },` |
|         - |  410 | `	{ "curl_errno", "CurlHandle $handle", "int" },` |
|         - |  411 | `	{ "curl_error", "CurlHandle $handle", "string" },` |
|         - |  412 | `	{ "curl_escape", "CurlHandle $handle, string $string", "string\|false" },` |
|         - |  413 | `	{ "curl_exec", "CurlHandle $handle", "string\|bool" },` |
|         - |  414 | `	{ "curl_file_create", "string $filename, ?string $mime_type = null, ?string $posted_filename = null", "CURLFile" },` |
|         - |  415 | `	{ "curl_getinfo", "CurlHandle $handle, ?int $option = null", "mixed" },` |
|         - |  416 | `	{ "curl_init", "?string $url = null", "CurlHandle\|false" },` |
|         - |  417 | `	{ "curl_multi_add_handle", "CurlMultiHandle $multi_handle, CurlHandle $handle", "int" },` |
|         - |  418 | `	{ "curl_multi_close", "CurlMultiHandle $multi_handle", "void" },` |
|         - |  419 | `	{ "curl_multi_errno", "CurlMultiHandle $multi_handle", "int" },` |
|         - |  420 | `	{ "curl_multi_exec", "CurlMultiHandle $multi_handle, &$still_running", "int" },` |
|         - |  421 | `	{ "curl_multi_get_handles", "CurlMultiHandle $multi_handle", "array" },` |
|         - |  422 | `	{ "curl_multi_getcontent", "CurlHandle $handle", "?string" },` |
|         - |  423 | `	{ "curl_multi_info_read", "CurlMultiHandle $multi_handle, &$queued_messages = NULL", "array\|false" },` |
|         - |  424 | `	{ "curl_multi_init", "", "CurlMultiHandle" },` |
|         - |  425 | `	{ "curl_multi_remove_handle", "CurlMultiHandle $multi_handle, CurlHandle $handle", "int" },` |
|         - |  426 | `	{ "curl_multi_select", "CurlMultiHandle $multi_handle, float $timeout = 1.0", "int" },` |
|         - |  427 | `	{ "curl_multi_setopt", "CurlMultiHandle $multi_handle, int $option, mixed $value", "bool" },` |
|         - |  428 | `	{ "curl_multi_strerror", "int $error_code", "?string" },` |
|         - |  429 | `	{ "curl_pause", "CurlHandle $handle, int $flags", "int" },` |
|         - |  430 | `	{ "curl_reset", "CurlHandle $handle", "void" },` |
|         - |  431 | `	{ "curl_setopt", "CurlHandle $handle, int $option, mixed $value", "bool" },` |
|         - |  432 | `	{ "curl_setopt_array", "CurlHandle $handle, array $options", "bool" },` |
|         - |  433 | `	{ "curl_unescape", "CurlHandle $handle, string $string", "string\|false" },` |
|         - |  434 | `	{ "curl_upkeep", "CurlHandle $handle", "bool" },` |
|         - |  435 | `	{ "curl_share_close", "CurlShareHandle $share_handle", "void" },` |
|         - |  436 | `	{ "curl_share_errno", "CurlShareHandle $share_handle", "int" },` |
|         - |  437 | `	{ "curl_share_init", "", "CurlShareHandle" },` |
|         - |  438 | `	{ "curl_share_init_persistent", "array $share_options", "CurlSharePersistentHandle" },` |
|         - |  439 | `	{ "curl_share_setopt", "CurlShareHandle $share_handle, int $option, mixed $value", "bool" },` |
|         - |  440 | `	{ "curl_share_strerror", "int $error_code", "?string" },` |
|         - |  441 | `	{ "curl_strerror", "int $error_code", "?string" },` |
|         - |  442 | `	{ "curl_version", "", "array\|false" },` |
|         - |  443 | `	/* ext/openssl. Signatures dumped from php 8.5's own ReflectionFunction --` |
|         - |  444 | `	 * the parameter NAMES included, since a named argument spells the php one.` |
|         - |  445 | ``	 * The five untyped `$key` / `$certificate` parameters are untyped in php`` |
|         - |  446 | ``	 * too: each takes a handle object, a PEM string, a `file://` path or an`` |
|         - |  447 | `	 * array pair, so php declares no type and screens by hand. */` |
|         - |  448 | `	{ "openssl_x509_export_to_file", "OpenSSLCertificate\|string $certificate, string $output_filename, bool $no_text = true", "bool" },` |
|         - |  449 | `	{ "openssl_x509_export", "OpenSSLCertificate\|string $certificate, &$output, bool $no_text = true", "bool" },` |
|         - |  450 | `	{ "openssl_x509_fingerprint", "OpenSSLCertificate\|string $certificate, string $digest_algo = 'sha1', bool $binary = false", "string\|false" },` |
|         - |  451 | `	{ "openssl_x509_check_private_key", "OpenSSLCertificate\|string $certificate, $private_key", "bool" },` |
|         - |  452 | `	{ "openssl_x509_verify", "OpenSSLCertificate\|string $certificate, $public_key", "int" },` |
|         - |  453 | `	{ "openssl_x509_parse", "OpenSSLCertificate\|string $certificate, bool $short_names = true", "array\|false" },` |
|         - |  454 | `	{ "openssl_x509_checkpurpose", "OpenSSLCertificate\|string $certificate, int $purpose, array $ca_info = [], ?string $untrusted_certificates_file = NULL", "int\|bool" },` |
|         - |  455 | `	{ "openssl_x509_read", "OpenSSLCertificate\|string $certificate", "OpenSSLCertificate\|false" },` |
|         - |  456 | `	{ "openssl_pkcs12_export_to_file", "OpenSSLCertificate\|string $certificate, string $output_filename, $private_key, string $passphrase, array $options = []", "bool" },` |
|         - |  457 | `	{ "openssl_pkcs12_export", "OpenSSLCertificate\|string $certificate, &$output, $private_key, string $passphrase, array $options = []", "bool" },` |
|         - |  458 | `	{ "openssl_pkcs12_read", "string $pkcs12, &$certificates, string $passphrase", "bool" },` |
|         - |  459 | `	{ "openssl_csr_export_to_file", "OpenSSLCertificateSigningRequest\|string $csr, string $output_filename, bool $no_text = true", "bool" },` |
|         - |  460 | `	{ "openssl_csr_export", "OpenSSLCertificateSigningRequest\|string $csr, &$output, bool $no_text = true", "bool" },` |
|         - |  461 | `	{ "openssl_csr_sign", "OpenSSLCertificateSigningRequest\|string $csr, OpenSSLCertificate\|string\|null $ca_certificate, $private_key, int $days, ?array $options = NULL, int $serial = 0, ?string $serial_hex = NULL", "OpenSSLCertificate\|false" },` |
|         - |  462 | `	{ "openssl_csr_new", "array $distinguished_names, &$private_key, ?array $options = NULL, ?array $extra_attributes = NULL", "OpenSSLCertificateSigningRequest\|bool" },` |
|         - |  463 | `	{ "openssl_csr_get_subject", "OpenSSLCertificateSigningRequest\|string $csr, bool $short_names = true", "array\|false" },` |
|         - |  464 | `	{ "openssl_csr_get_public_key", "OpenSSLCertificateSigningRequest\|string $csr, bool $short_names = true", "OpenSSLAsymmetricKey\|false" },` |
|         - |  465 | `	{ "openssl_pkcs7_verify", "string $input_filename, int $flags, ?string $signers_certificates_filename = NULL, array $ca_info = [], ?string $untrusted_certificates_filename = NULL, ?string $content = NULL, ?string $output_filename = NULL", "int\|bool" },` |
|         - |  466 | `	{ "openssl_pkcs7_encrypt", "string $input_filename, string $output_filename, $certificate, ?array $headers, int $flags = 0, int $cipher_algo = OPENSSL_CIPHER_AES_128_CBC", "bool" },` |
|         - |  467 | `	{ "openssl_pkcs7_sign", "string $input_filename, string $output_filename, OpenSSLCertificate\|string $certificate, $private_key, ?array $headers, int $flags = PKCS7_DETACHED, ?string $untrusted_certificates_filename = NULL", "bool" },` |
|         - |  468 | `	{ "openssl_pkcs7_decrypt", "string $input_filename, string $output_filename, $certificate, $private_key = NULL", "bool" },` |
|         - |  469 | `	{ "openssl_pkcs7_read", "string $data, &$certificates", "bool" },` |
|         - |  470 | `	{ "openssl_cms_verify", "string $input_filename, int $flags = 0, ?string $certificates = NULL, array $ca_info = [], ?string $untrusted_certificates_filename = NULL, ?string $content = NULL, ?string $pk7 = NULL, ?string $sigfile = NULL, int $encoding = OPENSSL_ENCODING_SMIME", "bool" },` |
|         - |  471 | `	{ "openssl_cms_encrypt", "string $input_filename, string $output_filename, $certificate, ?array $headers, int $flags = 0, int $encoding = OPENSSL_ENCODING_SMIME, string\|int $cipher_algo = OPENSSL_CIPHER_AES_128_CBC", "bool" },` |
|         - |  472 | `	{ "openssl_cms_sign", "string $input_filename, string $output_filename, OpenSSLCertificate\|string $certificate, $private_key, ?array $headers, int $flags = 0, int $encoding = OPENSSL_ENCODING_SMIME, ?string $untrusted_certificates_filename = NULL", "bool" },` |
|         - |  473 | `	{ "openssl_cms_decrypt", "string $input_filename, string $output_filename, $certificate, $private_key = NULL, int $encoding = OPENSSL_ENCODING_SMIME", "bool" },` |
|         - |  474 | `	{ "openssl_cms_read", "string $input_filename, &$certificates", "bool" },` |
|         - |  475 | `	{ "openssl_pbkdf2", "string $password, string $salt, int $key_length, int $iterations, string $digest_algo = 'sha1'", "string\|false" },` |
|         - |  476 | `	{ "openssl_error_string", "", "string\|false" },` |
|         - |  477 | `	{ "openssl_get_md_methods", "bool $aliases = false", "array" },` |
|         - |  478 | `	{ "openssl_get_cipher_methods", "bool $aliases = false", "array" },` |
|         - |  479 | `	{ "openssl_get_curve_names", "", "array\|false" },` |
|         - |  480 | `	{ "openssl_digest", "string $data, string $digest_algo, bool $binary = false", "string\|false" },` |
|         - |  481 | `	{ "openssl_encrypt", "string $data, string $cipher_algo, string $passphrase, int $options = 0, string $iv = '', &$tag = NULL, string $aad = '', int $tag_length = 16", "string\|false" },` |
|         - |  482 | `	{ "openssl_decrypt", "string $data, string $cipher_algo, string $passphrase, int $options = 0, string $iv = '', ?string $tag = NULL, string $aad = ''", "string\|false" },` |
|         - |  483 | `	{ "openssl_cipher_iv_length", "string $cipher_algo", "int\|false" },` |
|         - |  484 | `	{ "openssl_cipher_key_length", "string $cipher_algo", "int\|false" },` |
|         - |  485 | `	{ "openssl_random_pseudo_bytes", "int $length, &$strong_result = NULL", "string" },` |
|         - |  486 | `	{ "openssl_get_cert_locations", "", "array" },` |
|         - |  487 | `	{ "openssl_pkey_new", "?array $options = NULL", "OpenSSLAsymmetricKey\|false" },` |
|         - |  488 | `	{ "openssl_pkey_export_to_file", "$key, string $output_filename, ?string $passphrase = NULL, ?array $options = NULL", "bool" },` |
|         - |  489 | `	{ "openssl_pkey_export", "$key, &$output, ?string $passphrase = NULL, ?array $options = NULL", "bool" },` |
|         - |  490 | `	{ "openssl_pkey_get_public", "$public_key", "OpenSSLAsymmetricKey\|false" },` |
|         - |  491 | `	{ "openssl_get_publickey", "$public_key", "OpenSSLAsymmetricKey\|false" },` |
|         - |  492 | `	{ "openssl_pkey_get_private", "$private_key, ?string $passphrase = NULL", "OpenSSLAsymmetricKey\|false" },` |
|         - |  493 | `	{ "openssl_get_privatekey", "$private_key, ?string $passphrase = NULL", "OpenSSLAsymmetricKey\|false" },` |
|         - |  494 | `	{ "openssl_pkey_get_details", "OpenSSLAsymmetricKey $key", "array\|false" },` |
|         - |  495 | `	{ "openssl_private_encrypt", "string $data, &$encrypted_data, $private_key, int $padding = OPENSSL_PKCS1_PADDING", "bool" },` |
|         - |  496 | `	{ "openssl_private_decrypt", "string $data, &$decrypted_data, $private_key, int $padding = OPENSSL_PKCS1_PADDING, ?string $digest_algo = NULL", "bool" },` |
|         - |  497 | `	{ "openssl_public_encrypt", "string $data, &$encrypted_data, $public_key, int $padding = OPENSSL_PKCS1_PADDING, ?string $digest_algo = NULL", "bool" },` |
|         - |  498 | `	{ "openssl_public_decrypt", "string $data, &$decrypted_data, $public_key, int $padding = OPENSSL_PKCS1_PADDING", "bool" },` |
|         - |  499 | `	{ "openssl_sign", "string $data, &$signature, $private_key, string\|int $algorithm = OPENSSL_ALGO_SHA1, int $padding = 0", "bool" },` |
|         - |  500 | `	{ "openssl_verify", "string $data, string $signature, $public_key, string\|int $algorithm = OPENSSL_ALGO_SHA1, int $padding = 0", "int\|false" },` |
|         - |  501 | `	{ "openssl_seal", "string $data, &$sealed_data, &$encrypted_keys, array $public_key, string $cipher_algo, &$iv = NULL", "int\|false" },` |
|         - |  502 | `	{ "openssl_open", "string $data, &$output, string $encrypted_key, $private_key, string $cipher_algo, ?string $iv = NULL", "bool" },` |
|         - |  503 | `	{ "openssl_dh_compute_key", "string $public_key, OpenSSLAsymmetricKey $private_key", "string\|false" },` |
|         - |  504 | `	{ "openssl_pkey_derive", "$public_key, $private_key", "string\|false" },` |
|         - |  505 | `	{ "openssl_spki_new", "OpenSSLAsymmetricKey $private_key, string $challenge, int $digest_algo = OPENSSL_ALGO_MD5", "string\|false" },` |
|         - |  506 | `	{ "openssl_spki_verify", "string $spki", "bool" },` |
|         - |  507 | `	{ "openssl_spki_export", "string $spki", "string\|false" },` |
|         - |  508 | `	{ "openssl_spki_export_challenge", "string $spki", "string\|false" },` |
|         - |  509 | `	{ "get_cfg_var", "string $option", "array\|string\|false" },` |
|         - |  510 | `	{ "ini_get", "string $option", "string\|false" },` |
|         - |  511 | `	{ "ini_get_all", "?string $extension = null, bool $details = true", "array\|false" },` |
|         - |  512 | `	{ "ini_restore", "string $option", "void" },` |
|         - |  513 | `	{ "ini_alter", "string $option, string\|int\|float\|bool\|null $value", "string\|false" },` |
|         - |  514 | `	{ "ini_set", "string $option, string\|int\|float\|bool\|null $value", "string\|false" },` |
|         - |  515 | `	{ "libxml_clear_errors", "", "void" },` |
|         - |  516 | `	{ "libxml_get_errors", "", "array" },` |
|         - |  517 | `	{ "libxml_get_external_entity_loader", "", "?callable" },` |
|         - |  518 | `	{ "libxml_get_last_error", "", "LibXMLError\|false" },` |
|         - |  519 | `	{ "libxml_set_external_entity_loader", "?callable $resolver_function", "true" },` |
|         - |  520 | `	{ "libxml_set_streams_context", "$context", "void" },` |
|         - |  521 | `	{ "libxml_use_internal_errors", "?bool $use_errors = null", "bool" },` |
|         - |  522 | ``	/* ext/simplexml's three, and ext/dom's one door into it. `object $node` is`` |
|         - |  523 | `	 * php's own declaration for both directions: the class screen is the body's,` |
|         - |  524 | `	 * so a plain object gets the TypeError the body words and not ZPP's. */` |
|         - |  525 | `	{ "simplexml_load_file",` |
|         - |  526 | `	  "string $filename, ?string $class_name = SimpleXMLElement::class, int $options = 0, "` |
|         - |  527 | `	  "string $namespace_or_prefix = '', bool $is_prefix = false", "SimpleXMLElement\|false" },` |
|         - |  528 | `	{ "simplexml_load_string",` |
|         - |  529 | `	  "string $data, ?string $class_name = SimpleXMLElement::class, int $options = 0, "` |
|         - |  530 | `	  "string $namespace_or_prefix = '', bool $is_prefix = false", "SimpleXMLElement\|false" },` |
|         - |  531 | `	{ "simplexml_import_dom",` |
|         - |  532 | `	  "object $node, ?string $class_name = SimpleXMLElement::class", "?SimpleXMLElement" },` |
|         - |  533 | `	{ "dom_import_simplexml", "object $node", "DOMAttr\|DOMElement" },` |
|         - |  534 | `	{ "doubleval", "mixed $value", "float" },` |
|         - |  535 | `	{ "Dom\\import_simplexml", "object $node", "Dom\\Attr\|Dom\\Element" },` |
|         - |  536 | `	/* ext/pdo's one function: the procedural spelling of` |
|         - |  537 | `	 * PDO::getAvailableDrivers(). */` |
|         - |  538 | `	{ "pdo_drivers", "", "array" },` |
|         - |  539 | `	{ "xml_error_string", "int $error_code", "?string" },` |
|         - |  540 | `	{ "xml_get_current_byte_index", "XMLParser $parser", "int" },` |
|         - |  541 | `	{ "xml_get_current_column_number", "XMLParser $parser", "int" },` |
|         - |  542 | `	{ "xml_get_current_line_number", "XMLParser $parser", "int" },` |
|         - |  543 | `	{ "xml_get_error_code", "XMLParser $parser", "int" },` |
|         - |  544 | `	{ "xml_parse", "XMLParser $parser, string $data, bool $is_final = false", "int" },` |
|         - |  545 | `	{ "xml_parse_into_struct", "XMLParser $parser, string $data, &$values, &$index = NULL", "int\|false" },` |
|         - |  546 | `	{ "xml_parser_create", "?string $encoding = NULL", "XMLParser" },` |
|         - |  547 | `	{ "xml_parser_create_ns", "?string $encoding = NULL, string $separator = ':'", "XMLParser" },` |
|         - |  548 | `	{ "xml_parser_get_option", "XMLParser $parser, int $option", "string\|int\|bool" },` |
|         - |  549 | `	{ "xml_parser_set_option", "XMLParser $parser, int $option, $value", "bool" },` |
|         - |  550 | `	{ "xml_set_character_data_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  551 | `	{ "xml_set_default_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  552 | `	{ "xml_set_element_handler", "XMLParser $parser, callable\|string\|null $start_handler, callable\|string\|null $end_handler", "true" },` |
|         - |  553 | `	{ "xml_set_end_namespace_decl_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  554 | `	{ "xml_set_external_entity_ref_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  555 | `	{ "xml_set_notation_decl_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  556 | `	{ "xml_set_processing_instruction_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  557 | `	{ "xml_set_start_namespace_decl_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  558 | `	{ "xml_set_unparsed_entity_decl_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  559 | `	/* ext/xmlwriter: php presents every writer verb under a function name as` |
|         - |  560 | `	 * well, with the writer as argument #1 -- which is the numbering its own` |
|         - |  561 | `	 * diagnostics report from BOTH spellings (see vm_xmlwriter.c). */` |
|         - |  562 | `	{ "xmlwriter_open_uri", "string $uri", "XMLWriter\|false" },` |
|         - |  563 | `	{ "xmlwriter_open_memory", "", "XMLWriter\|false" },` |
|         - |  564 | `	{ "xmlwriter_set_indent", "XMLWriter $writer, bool $enable", "bool" },` |
|         - |  565 | `	{ "xmlwriter_set_indent_string", "XMLWriter $writer, string $indentation", "bool" },` |
|         - |  566 | `	{ "xmlwriter_start_comment", "XMLWriter $writer", "bool" },` |
|         - |  567 | `	{ "xmlwriter_end_comment", "XMLWriter $writer", "bool" },` |
|         - |  568 | `	{ "xmlwriter_start_attribute", "XMLWriter $writer, string $name", "bool" },` |
|         - |  569 | `	{ "xmlwriter_end_attribute", "XMLWriter $writer", "bool" },` |
|         - |  570 | `	{ "xmlwriter_write_attribute", "XMLWriter $writer, string $name, string $value", "bool" },` |
|         - |  571 | `	{ "xmlwriter_start_attribute_ns", "XMLWriter $writer, ?string $prefix, string $name, ?string $namespace", "bool" },` |
|         - |  572 | `	{ "xmlwriter_write_attribute_ns", "XMLWriter $writer, ?string $prefix, string $name, ?string $namespace, string $value", "bool" },` |
|         - |  573 | `	{ "xmlwriter_start_element", "XMLWriter $writer, string $name", "bool" },` |
|         - |  574 | `	{ "xmlwriter_end_element", "XMLWriter $writer", "bool" },` |
|         - |  575 | `	{ "xmlwriter_full_end_element", "XMLWriter $writer", "bool" },` |
|         - |  576 | `	{ "xmlwriter_start_element_ns", "XMLWriter $writer, ?string $prefix, string $name, ?string $namespace", "bool" },` |
|         - |  577 | `	{ "xmlwriter_write_element", "XMLWriter $writer, string $name, ?string $content = null", "bool" },` |
|         - |  578 | `	{ "xmlwriter_write_element_ns", "XMLWriter $writer, ?string $prefix, string $name, ?string $namespace, ?string $content = null", "bool" },` |
|         - |  579 | `	{ "xmlwriter_start_pi", "XMLWriter $writer, string $target", "bool" },` |
|         - |  580 | `	{ "xmlwriter_end_pi", "XMLWriter $writer", "bool" },` |
|         - |  581 | `	{ "xmlwriter_write_pi", "XMLWriter $writer, string $target, string $content", "bool" },` |
|         - |  582 | `	{ "xmlwriter_start_cdata", "XMLWriter $writer", "bool" },` |
|         - |  583 | `	{ "xmlwriter_end_cdata", "XMLWriter $writer", "bool" },` |
|         - |  584 | `	{ "xmlwriter_write_cdata", "XMLWriter $writer, string $content", "bool" },` |
|         - |  585 | `	{ "xmlwriter_text", "XMLWriter $writer, string $content", "bool" },` |
|         - |  586 | `	{ "xmlwriter_write_raw", "XMLWriter $writer, string $content", "bool" },` |
|         - |  587 | `	{ "xmlwriter_start_document", "XMLWriter $writer, ?string $version = '1.0', ?string $encoding = null, ?string $standalone = null", "bool" },` |
|         - |  588 | `	{ "xmlwriter_end_document", "XMLWriter $writer", "bool" },` |
|         - |  589 | `	{ "xmlwriter_write_comment", "XMLWriter $writer, string $content", "bool" },` |
|         - |  590 | `	{ "xmlwriter_start_dtd", "XMLWriter $writer, string $qualifiedName, ?string $publicId = null, ?string $systemId = null", "bool" },` |
|         - |  591 | `	{ "xmlwriter_end_dtd", "XMLWriter $writer", "bool" },` |
|         - |  592 | `	{ "xmlwriter_write_dtd", "XMLWriter $writer, string $name, ?string $publicId = null, ?string $systemId = null, ?string $content = null", "bool" },` |
|         - |  593 | `	{ "xmlwriter_start_dtd_element", "XMLWriter $writer, string $qualifiedName", "bool" },` |
|         - |  594 | `	{ "xmlwriter_end_dtd_element", "XMLWriter $writer", "bool" },` |
|         - |  595 | `	{ "xmlwriter_write_dtd_element", "XMLWriter $writer, string $name, string $content", "bool" },` |
|         - |  596 | `	{ "xmlwriter_start_dtd_attlist", "XMLWriter $writer, string $name", "bool" },` |
|         - |  597 | `	{ "xmlwriter_end_dtd_attlist", "XMLWriter $writer", "bool" },` |
|         - |  598 | `	{ "xmlwriter_write_dtd_attlist", "XMLWriter $writer, string $name, string $content", "bool" },` |
|         - |  599 | `	{ "xmlwriter_start_dtd_entity", "XMLWriter $writer, string $name, bool $isParam", "bool" },` |
|         - |  600 | `	{ "xmlwriter_end_dtd_entity", "XMLWriter $writer", "bool" },` |
|         - |  601 | `	{ "xmlwriter_write_dtd_entity", "XMLWriter $writer, string $name, string $content, bool $isParam = false, ?string $publicId = null, ?string $systemId = null, ?string $notationData = null", "bool" },` |
|         - |  602 | `	{ "xmlwriter_output_memory", "XMLWriter $writer, bool $flush = true", "string" },` |
|         - |  603 | `	{ "xmlwriter_flush", "XMLWriter $writer, bool $empty = true", "string\|int" },` |
|         - |  604 | `	{ "session_abort", "", "bool" },` |
|         - |  605 | `	{ "session_cache_expire", "?int $value = null", "int\|false" },` |
|         - |  606 | `	{ "session_cache_limiter", "?string $value = null", "string\|false" },` |
|         - |  607 | `	{ "session_commit", "", "bool" },` |
|         - |  608 | `	{ "session_create_id", "string $prefix = \"\"", "string\|false" },` |
|         - |  609 | `	{ "session_decode", "string $data", "bool" },` |
|         - |  610 | `	{ "session_destroy", "", "bool" },` |
|         - |  611 | `	{ "session_gc", "", "int\|false" },` |
|         - |  612 | `	{ "session_get_cookie_params", "", "array" },` |
|         - |  613 | `	{ "session_set_save_handler", "$sessionhandler, ...$rest = ?", "bool" },` |
|         - |  614 | `	{ "session_set_cookie_params", "array\|int $lifetime_or_options, ?string $path = null, ?string $domain = null, ?bool $secure = null, ?bool $httponly = null", "bool" },` |
|         - |  615 | `	{ "session_encode", "", "string\|false" },` |
|         - |  616 | `	{ "session_id", "?string $id = null", "string\|false" },` |
|         - |  617 | `	{ "session_module_name", "?string $module = null", "string\|false" },` |
|         - |  618 | `	{ "session_name", "?string $name = null", "string\|false" },` |
|         - |  619 | `	{ "session_regenerate_id", "bool $delete_old_session = false", "bool" },` |
|         - |  620 | `	{ "session_register_shutdown", "", "void" },` |
|         - |  621 | `	{ "session_reset", "", "bool" },` |
|         - |  622 | `	{ "session_save_path", "?string $path = null", "string\|false" },` |
|         - |  623 | `	{ "session_start", "array $options = []", "bool" },` |
|         - |  624 | `	{ "session_status", "", "int" },` |
|         - |  625 | `	{ "session_unset", "", "bool" },` |
|         - |  626 | `	{ "session_write_close", "", "bool" },` |
|         - |  627 | `	{ "abs", "int\|float $num", "int\|float" },` |
|         - |  628 | `	{ "acos", "float $num", "float" },` |
|         - |  629 | `	{ "acosh", "float $num", "float" },` |
|         - |  630 | `	{ "addcslashes", "string $string, string $characters", "string" },` |
|         - |  631 | `	{ "addslashes", "string $string", "string" },` |
|         - |  632 | `	{ "array_all", "array $array, callable $callback", "bool" },` |
|         - |  633 | `	{ "array_any", "array $array, callable $callback", "bool" },` |
|         - |  634 | `	{ "array_chunk", "array $array, int $length, bool $preserve_keys = false", "array" },` |
|         - |  635 | `	{ "array_column", "array $array, string\|int\|null $column_key, string\|int\|null $index_key = NULL", "array" },` |
|         - |  636 | `	{ "array_combine", "array $keys, array $values", "array" },` |
|         - |  637 | `	{ "array_diff", "array $array, array ...$arrays = ?", "array" },` |
|         - |  638 | `	{ "array_diff_assoc", "array $array, array ...$arrays = ?", "array" },` |
|         - |  639 | `	{ "array_diff_key", "array $array, array ...$arrays = ?", "array" },` |
|         - |  640 | `	{ "array_diff_uassoc", "array $array, ...$rest = ?", "array" },` |
|         - |  641 | `	{ "array_diff_ukey", "array $array, ...$rest = ?", "array" },` |
|         - |  642 | `	{ "array_fill", "int $start_index, int $count, mixed $value", "array" },` |
|         - |  643 | `	{ "array_fill_keys", "array $keys, mixed $value", "array" },` |
|         - |  644 | `	{ "array_filter", "array $array, ?callable $callback = NULL, int $mode = 0", "array" },` |
|         - |  645 | `	{ "array_find", "array $array, callable $callback", "mixed" },` |
|         - |  646 | `	{ "array_find_key", "array $array, callable $callback", "mixed" },` |
|         - |  647 | `	{ "array_first", "array $array", "mixed" },` |
|         - |  648 | `	{ "array_flip", "array $array", "array" },` |
|         - |  649 | `	{ "array_intersect", "array $array, array ...$arrays = ?", "array" },` |
|         - |  650 | `	{ "array_intersect_assoc", "array $array, array ...$arrays = ?", "array" },` |
|         - |  651 | `	{ "array_intersect_key", "array $array, array ...$arrays = ?", "array" },` |
|         - |  652 | `	{ "array_intersect_uassoc", "array $array, ...$rest = ?", "array" },` |
|         - |  653 | `	{ "array_intersect_ukey", "array $array, ...$rest = ?", "array" },` |
|         - |  654 | `	{ "array_is_list", "array $array", "bool" },` |
|         - |  655 | `	{ "array_key_exists", "$key, array $array", "bool" },` |
|         - |  656 | `	{ "array_key_first", "array $array", "string\|int\|null" },` |
|         - |  657 | `	{ "array_key_last", "array $array", "string\|int\|null" },` |
|         - |  658 | `	{ "array_keys", "array $array, mixed $filter_value = ?, bool $strict = false", "array" },` |
|         - |  659 | `	{ "array_last", "array $array", "mixed" },` |
|         - |  660 | `	{ "array_map", "?callable $callback, array $array, array ...$arrays = ?", "array" },` |
|         - |  661 | `	{ "array_merge", "array ...$arrays = ?", "array" },` |
|         - |  662 | `	{ "array_multisort", "&$array, &...$rest = ?", "true" },` |
|         - |  663 | `	{ "array_merge_recursive", "array ...$arrays = ?", "array" },` |
|         - |  664 | `	{ "array_pad", "array $array, int $length, mixed $value", "array" },` |
|         - |  665 | `	{ "array_pop", "array &$array", "mixed" },` |
|         - |  666 | `	{ "array_product", "array $array", "int\|float" },` |
|         - |  667 | `	{ "array_push", "array &$array, mixed ...$values = ?", "int" },` |
|         - |  668 | `	{ "array_rand", "array $array, int $num = 1", "array\|string\|int" },` |
|         - |  669 | `	{ "array_reduce", "array $array, callable $callback, mixed $initial = NULL", "mixed" },` |
|         - |  670 | `	{ "array_replace", "array $array, array ...$replacements = ?", "array" },` |
|         - |  671 | `	{ "array_reverse", "array $array, bool $preserve_keys = false", "array" },` |
|         - |  672 | `	{ "array_search", "mixed $needle, array $haystack, bool $strict = false", "string\|int\|false" },` |
|         - |  673 | `	{ "array_shift", "array &$array", "mixed" },` |
|         - |  674 | `	{ "array_slice", "array $array, int $offset, ?int $length = NULL, bool $preserve_keys = false", "array" },` |
|         - |  675 | `	{ "array_splice", "array &$array, int $offset, ?int $length = NULL, mixed $replacement = []", "array" },` |
|         - |  676 | `	{ "array_sum", "array $array", "int\|float" },` |
|         - |  677 | `	{ "array_udiff", "array $array, ...$rest = ?", "array" },` |
|         - |  678 | `	{ "array_udiff_assoc", "array $array, ...$rest = ?", "array" },` |
|         - |  679 | `	{ "array_udiff_uassoc", "array $array, ...$rest = ?", "array" },` |
|         - |  680 | `	{ "array_uintersect", "array $array, ...$rest = ?", "array" },` |
|         - |  681 | `	{ "array_uintersect_assoc", "array $array, ...$rest = ?", "array" },` |
|         - |  682 | `	{ "array_uintersect_uassoc", "array $array, ...$rest = ?", "array" },` |
|         - |  683 | `	{ "array_unique", "array $array, int $flags = SORT_STRING", "array" },` |
|         - |  684 | `	{ "array_unshift", "array &$array, mixed ...$values = ?", "int" },` |
|         - |  685 | `	{ "array_values", "array $array", "array" },` |
|         - |  686 | `	{ "array_walk", "object\|array &$array, callable $callback, mixed $arg = ?", "true" },` |
|         - |  687 | `	{ "array_walk_recursive", "object\|array &$array, callable $callback, mixed $arg = ?", "true" },` |
|         - |  688 | `	{ "arsort", "array &$array, int $flags = SORT_REGULAR", "true" },` |
|         - |  689 | `	{ "asin", "float $num", "float" },` |
|         - |  690 | `	{ "asinh", "float $num", "float" },` |
|         - |  691 | `	{ "asort", "array &$array, int $flags = SORT_REGULAR", "true" },` |
|         - |  692 | `	{ "assert", "mixed $assertion, Throwable\|string\|null $description = NULL", "bool" },` |
|         - |  693 | `	{ "atan", "float $num", "float" },` |
|         - |  694 | `	{ "atanh", "float $num", "float" },` |
|         - |  695 | `	{ "atan2", "float $y, float $x", "float" },` |
|         - |  696 | `	{ "base64_decode", "string $string, bool $strict = false", "string\|false" },` |
|         - |  697 | `	{ "base64_encode", "string $string", "string" },` |
|         - |  698 | `	{ "base_convert", "string $num, int $from_base, int $to_base", "string" },` |
|         - |  699 | `	{ "basename", "string $path, string $suffix = ''", "string" },` |
|         - |  700 | `	{ "bcadd", "string $num1, string $num2, ?int $scale = NULL", "string" },` |
|         - |  701 | `	{ "bcceil", "string $num", "string" },` |
|         - |  702 | `	{ "bccomp", "string $num1, string $num2, ?int $scale = NULL", "int" },` |
|         - |  703 | `	{ "bcdiv", "string $num1, string $num2, ?int $scale = NULL", "string" },` |
|         - |  704 | `	{ "bcdivmod", "string $num1, string $num2, ?int $scale = NULL", "array" },` |
|         - |  705 | `	{ "bcmod", "string $num1, string $num2, ?int $scale = NULL", "string" },` |
|         - |  706 | `	{ "bcfloor", "string $num", "string" },` |
|         - |  707 | `	{ "bcmul", "string $num1, string $num2, ?int $scale = NULL", "string" },` |
|         - |  708 | `	{ "bcpow", "string $num, string $exponent, ?int $scale = NULL", "string" },` |
|         - |  709 | `	{ "bcpowmod", "string $num, string $exponent, string $modulus, ?int $scale = NULL", "string" },` |
|         - |  710 | `	{ "bcround", "string $num, int $precision = 0, RoundingMode $mode = RoundingMode::HalfAwayFromZero", "string" },` |
|         - |  711 | `	{ "bcsqrt", "string $num, ?int $scale = NULL", "string" },` |
|         - |  712 | `	{ "bcscale", "?int $scale = NULL", "int" },` |
|         - |  713 | `	{ "bcsub", "string $num1, string $num2, ?int $scale = NULL", "string" },` |
|         - |  714 | `	{ "bin2hex", "string $string", "string" },` |
|         - |  715 | `	{ "bindec", "string $binary_string", "int\|float" },` |
|         - |  716 | `	{ "boolval", "mixed $value", "bool" },` |
|         - |  717 | `	{ "call_user_func", "callable $callback, mixed ...$args = ?", "mixed" },` |
|         - |  718 | `	{ "call_user_func_array", "callable $callback, array $args", "mixed" },` |
|         - |  719 | `	{ "cal_days_in_month", "int $calendar, int $month, int $year", "int" },` |
|         - |  720 | `	{ "cal_from_jd", "int $julian_day, int $calendar", "array" },` |
|         - |  721 | `	{ "cal_info", "int $calendar = -1", "array" },` |
|         - |  722 | `	{ "cal_to_jd", "int $calendar, int $month, int $day, int $year", "int" },` |
|         - |  723 | `	{ "ceil", "int\|float $num", "float" },` |
|         - |  724 | `	{ "chdir", "string $directory", "bool" },` |
|         - |  725 | `	{ "chgrp", "string $filename, string\|int $group", "bool" },` |
|         - |  726 | `	{ "chmod", "string $filename, int $permissions", "bool" },` |
|         - |  727 | `	{ "chop", "string $string, string $characters = \" \\n\\r\\t\\v\\x00\"", "string" },` |
|         - |  728 | `	{ "chown", "string $filename, string\|int $user", "bool" },` |
|         - |  729 | `	{ "chr", "int $codepoint", "string" },` |
|         - |  730 | `	{ "chroot", "string $directory", "bool" },` |
|         - |  731 | `	{ "chunk_split", "string $string, int $length = 76, string $separator = \"\\r\\n\"", "string" },` |
|         - |  732 | `	{ "class_alias", "string $class, string $alias, bool $autoload = true", "bool" },` |
|         - |  733 | `	{ "class_exists", "string $class, bool $autoload = true", "bool" },` |
|         - |  734 | ``	/* `$object_or_class` carries NO declared type on purpose: php screens it with`` |
|         - |  735 | `	 * Z_PARAM_OBJ_OR_STR, which refuses in the standard "must be of type` |
|         - |  736 | `	 * object\|string" wording while ReflectionParameter reports no type at all.` |
|         - |  737 | `	 * The builtin raises that refusal itself (vm_builtin_class.c). */` |
|         - |  738 | `	{ "class_implements", "$object_or_class, bool $autoload = true", "array\|false" },` |
|         - |  739 | `	{ "class_parents", "$object_or_class, bool $autoload = true", "array\|false" },` |
|         - |  740 | `	{ "class_uses", "$object_or_class, bool $autoload = true", "array\|false" },` |
|         - |  741 | `	{ "enum_exists", "string $enum, bool $autoload = true", "bool" },` |
|         - |  742 | `	{ "clone", "object $object, array $withProperties = []", "object" },` |
|         - |  743 | `	{ "closedir", "$dir_handle = NULL", "void" },` |
|         - |  744 | `	{ "compact", "$var_name, ...$var_names = ?", "array" },` |
|         - |  745 | `	{ "connection_aborted", "", "int" },` |
|         - |  746 | `	{ "connection_status", "", "int" },` |
|         - |  747 | `	{ "constant", "string $name", "mixed" },` |
|         - |  748 | `	{ "convert_uudecode", "string $string", "string\|false" },` |
|         - |  749 | `	{ "convert_uuencode", "string $string", "string" },` |
|         - |  750 | `	{ "copy", "string $from, string $to, $context = NULL", "bool" },` |
|         - |  751 | `	{ "cos", "float $num", "float" },` |
|         - |  752 | `	{ "cosh", "float $num", "float" },` |
|         - |  753 | `	{ "count", "Countable\|array $value, int $mode = COUNT_NORMAL", "int" },` |
|         - |  754 | `	{ "count_chars", "string $string, int $mode = 0", "array\|string" },` |
|         - |  755 | `	{ "crc32", "string $string", "int" },` |
|         - |  756 | `	{ "ctype_alnum", "mixed $text", "bool" },` |
|         - |  757 | `	{ "ctype_alpha", "mixed $text", "bool" },` |
|         - |  758 | `	{ "ctype_cntrl", "mixed $text", "bool" },` |
|         - |  759 | `	{ "ctype_digit", "mixed $text", "bool" },` |
|         - |  760 | `	{ "ctype_graph", "mixed $text", "bool" },` |
|         - |  761 | `	{ "ctype_lower", "mixed $text", "bool" },` |
|         - |  762 | `	{ "ctype_print", "mixed $text", "bool" },` |
|         - |  763 | `	{ "ctype_punct", "mixed $text", "bool" },` |
|         - |  764 | `	{ "ctype_space", "mixed $text", "bool" },` |
|         - |  765 | `	{ "ctype_upper", "mixed $text", "bool" },` |
|         - |  766 | `	{ "ctype_xdigit", "mixed $text", "bool" },` |
|         - |  767 | `	{ "current", "object\|array $array", "mixed" },` |
|         - |  768 | `	{ "date", "string $format, ?int $timestamp = NULL", "string" },` |
|         - |  769 | `	{ "date_add", "DateTime $object, DateInterval $interval", "DateTime" },` |
|         - |  770 | `	{ "date_create", "string $datetime = 'now', ?DateTimeZone $timezone = NULL", "DateTime\|false" },` |
|         - |  771 | `	{ "date_create_from_format", "string $format, string $datetime, ?DateTimeZone $timezone = NULL", "DateTime\|false" },` |
|         - |  772 | `	{ "date_create_immutable", "string $datetime = 'now', ?DateTimeZone $timezone = NULL", "DateTimeImmutable\|false" },` |
|         - |  773 | `	{ "date_create_immutable_from_format", "string $format, string $datetime, ?DateTimeZone $timezone = NULL", "DateTimeImmutable\|false" },` |
|         - |  774 | `	{ "date_date_set", "DateTime $object, int $year, int $month, int $day", "DateTime" },` |
|         - |  775 | `	{ "date_diff", "DateTimeInterface $baseObject, DateTimeInterface $targetObject, bool $absolute = false", "DateInterval" },` |
|         - |  776 | `	{ "date_format", "DateTimeInterface $object, string $format", "string" },` |
|         - |  777 | `	{ "date_get_last_errors", "", "array\|false" },` |
|         - |  778 | `	{ "date_interval_create_from_date_string", "string $datetime", "DateInterval\|false" },` |
|         - |  779 | `	{ "date_interval_format", "DateInterval $object, string $format", "string" },` |
|         - |  780 | `	{ "date_isodate_set", "DateTime $object, int $year, int $week, int $dayOfWeek = 1", "DateTime" },` |
|         - |  781 | `	{ "date_modify", "DateTime $object, string $modifier", "DateTime\|false" },` |
|         - |  782 | `	{ "date_offset_get", "DateTimeInterface $object", "int" },` |
|         - |  783 | `	{ "date_parse", "string $datetime", "array" },` |
|         - |  784 | `	{ "date_parse_from_format", "string $format, string $datetime", "array" },` |
|         - |  785 | `	{ "date_sub", "DateTime $object, DateInterval $interval", "DateTime" },` |
|         - |  786 | `	{ "date_time_set", "DateTime $object, int $hour, int $minute, int $second = 0, int $microsecond = 0", "DateTime" },` |
|         - |  787 | `	{ "date_timestamp_get", "DateTimeInterface $object", "int" },` |
|         - |  788 | `	{ "date_timestamp_set", "DateTime $object, int $timestamp", "DateTime" },` |
|         - |  789 | `	{ "date_timezone_get", "DateTimeInterface $object", "DateTimeZone\|false" },` |
|         - |  790 | `	{ "date_timezone_set", "DateTime $object, DateTimeZone $timezone", "DateTime" },` |
|         - |  791 | `	{ "timezone_abbreviations_list", "", "array" },` |
|         - |  792 | `	{ "timezone_identifiers_list", "int $timezoneGroup = DateTimeZone::ALL, ?string $countryCode = null", "array" },` |
|         - |  793 | `	{ "timezone_location_get", "DateTimeZone $object", "array\|false" },` |
|         - |  794 | `	{ "timezone_name_from_abbr", "string $abbr, int $utcOffset = -1, int $isDST = -1", "string\|false" },` |
|         - |  795 | `	{ "timezone_name_get", "DateTimeZone $object", "string" },` |
|         - |  796 | `	{ "timezone_offset_get", "DateTimeZone $object, DateTimeInterface $datetime", "int" },` |
|         - |  797 | `	{ "timezone_open", "string $timezone", "DateTimeZone\|false" },` |
|         - |  798 | `	{ "timezone_transitions_get", "DateTimeZone $object, int $timestampBegin = PHP_INT_MIN, int $timestampEnd = 2147483647", "array\|false" },` |
|         - |  799 | `	{ "timezone_version_get", "", "string" },` |
|         - |  800 | `	{ "date_default_timezone_get", "", "string" },` |
|         - |  801 | `	{ "date_default_timezone_set", "string $timezoneId", "bool" },` |
|         - |  802 | `	{ "date_sun_info", "int $timestamp, float $latitude, float $longitude", "array" },` |
|         - |  803 | `	{ "date_sunrise", "int $timestamp, int $returnFormat = SUNFUNCS_RET_STRING, ?float $latitude = null, ?float $longitude = null, ?float $zenith = null, ?float $utcOffset = null", "string\|int\|float\|false" },` |
|         - |  804 | `	{ "date_sunset", "int $timestamp, int $returnFormat = SUNFUNCS_RET_STRING, ?float $latitude = null, ?float $longitude = null, ?float $zenith = null, ?float $utcOffset = null", "string\|int\|float\|false" },` |
|         - |  805 | `	{ "debug_backtrace", "int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT, int $limit = 0", "array" },` |
|         - |  806 | `	{ "debug_print_backtrace", "int $options = 0, int $limit = 0", "void" },` |
|         - |  807 | `	{ "decbin", "int $num", "string" },` |
|         - |  808 | `	{ "dechex", "int $num", "string" },` |
|         - |  809 | `	{ "decoct", "int $num", "string" },` |
|         - |  810 | `	{ "define", "string $constant_name, mixed $value, bool $case_insensitive = false", "bool" },` |
|         - |  811 | `	{ "defined", "string $constant_name", "bool" },` |
|         - |  812 | `	{ "deg2rad", "float $num", "float" },` |
|         - |  813 | `	{ "die", "string\|int $status = 0", "never" },` |
|         - |  814 | `	{ "dir", "string $directory, $context = NULL", "Directory\|false" },` |
|         - |  815 | `	{ "dirname", "string $path, int $levels = 1", "string" },` |
|         - |  816 | `	{ "disk_free_space", "string $directory", "float\|false" },` |
|         - |  817 | `	{ "disk_total_space", "string $directory", "float\|false" },` |
|         - |  818 | `	{ "diskfreespace", "string $directory", "float\|false" },` |
|         - |  819 | `	{ "easter_date", "?int $year = NULL, int $mode = CAL_EASTER_DEFAULT", "int" },` |
|         - |  820 | `	{ "easter_days", "?int $year = NULL, int $mode = CAL_EASTER_DEFAULT", "int" },` |
|         - |  821 | `	{ "end", "object\|array &$array", "mixed" },` |
|         - |  822 | `	{ "error_get_last", "", "?array" },` |
|         - |  823 | `	{ "error_clear_last", "", "void" },` |
|         - |  824 | `	{ "error_log", "string $message, int $message_type = 0, ?string $destination = NULL, ?string $additional_headers = NULL", "bool" },` |
|         - |  825 | `	{ "error_reporting", "?int $error_level = NULL", "int" },` |
|         - |  826 | `	{ "escapeshellarg", "string $arg", "string" },` |
|         - |  827 | `	{ "escapeshellcmd", "string $command", "string" },` |
|         - |  828 | `	{ "exec", "string $command, &$output = NULL, &$result_code = NULL", "string\|false" },` |
|         - |  829 | `	{ "exit", "string\|int $status = 0", "never" },` |
|         - |  830 | `	{ "exp", "float $num", "float" },` |
|         - |  831 | `	{ "expm1", "float $num", "float" },` |
|         - |  832 | `	{ "explode", "string $separator, string $string, int $limit = PHP_INT_MAX", "array" },` |
|         - |  833 | `	{ "extension_loaded", "string $extension", "bool" },` |
|         - |  834 | `	{ "extract", "array &$array, int $flags = EXTR_OVERWRITE, string $prefix = ''", "int" },` |
|         - |  835 | `	{ "fclose", "$stream", "bool" },` |
|         - |  836 | `	{ "feof", "$stream", "bool" },` |
|         - |  837 | `	{ "fflush", "$stream", "bool" },` |
|         - |  838 | `	{ "fgetc", "$stream", "string\|false" },` |
|         - |  839 | `	{ "fgetcsv", "$stream, ?int $length = NULL, string $separator = ',', string $enclosure = '\"', string $escape = '\\\\'", "array\|false" },` |
|         - |  840 | `	{ "fgets", "$stream, ?int $length = NULL", "string\|false" },` |
|         - |  841 | `	{ "file", "string $filename, int $flags = 0, $context = NULL", "array\|false" },` |
|         - |  842 | `	{ "file_exists", "string $filename", "bool" },` |
|         - |  843 | `	{ "file_get_contents", "string $filename, bool $use_include_path = false, $context = NULL, int $offset = 0, ?int $length = NULL", "string\|false" },` |
|         - |  844 | `	{ "file_put_contents", "string $filename, mixed $data, int $flags = 0, $context = NULL", "int\|false" },` |
|         - |  845 | `	{ "fileatime", "string $filename", "int\|false" },` |
|         - |  846 | `	{ "filectime", "string $filename", "int\|false" },` |
|         - |  847 | `	{ "filegroup", "string $filename", "int\|false" },` |
|         - |  848 | `	{ "fileinode", "string $filename", "int\|false" },` |
|         - |  849 | `	{ "filemtime", "string $filename", "int\|false" },` |
|         - |  850 | `	{ "fileowner", "string $filename", "int\|false" },` |
|         - |  851 | `	{ "fileperms", "string $filename", "int\|false" },` |
|         - |  852 | `	{ "filesize", "string $filename", "int\|false" },` |
|         - |  853 | `	{ "filetype", "string $filename", "string\|false" },` |
|         - |  854 | `	{ "filter_has_var", "int $input_type, string $var_name", "bool" },` |
|         - |  855 | `	{ "filter_id", "string $name", "int\|false" },` |
|         - |  856 | `	{ "filter_input", "int $type, string $var_name, int $filter = FILTER_DEFAULT, array\|int $options = 0", "mixed" },` |
|         - |  857 | `	{ "filter_input_array", "int $type, array\|int $options = FILTER_DEFAULT, bool $add_empty = true", "array\|false\|null" },` |
|         - |  858 | `	{ "filter_list", "", "array" },` |
|         - |  859 | `	{ "filter_var", "mixed $value, int $filter = FILTER_DEFAULT, array\|int $options = 0", "mixed" },` |
|         - |  860 | `	{ "filter_var_array", "array $array, array\|int $options = FILTER_DEFAULT, bool $add_empty = true", "array\|false\|null" },` |
|         - |  861 | `	{ "floatval", "mixed $value", "float" },` |
|         - |  862 | `	{ "flock", "$stream, int $operation, &$would_block = NULL", "bool" },` |
|         - |  863 | `	{ "floor", "int\|float $num", "float" },` |
|         - |  864 | `	{ "flush", "", "void" },` |
|         - |  865 | `	{ "fmod", "float $num1, float $num2", "float" },` |
|         - |  866 | `	{ "fnmatch", "string $pattern, string $filename, int $flags = 0", "bool" },` |
|         - |  867 | `	{ "fopen", "string $filename, string $mode, bool $use_include_path = false, $context = NULL", "" },` |
|         - |  868 | `	{ "forward_static_call", "callable $callback, mixed ...$args = ?", "mixed" },` |
|         - |  869 | `	{ "forward_static_call_array", "callable $callback, array $args", "mixed" },` |
|         - |  870 | `	{ "fpow", "float $num, float $exponent", "float" },` |
|         - |  871 | `	{ "fpassthru", "$stream", "int" },` |
|         - |  872 | ``	/* `~string $format`: php resolves the STREAM first and refuses a closed one`` |
|         - |  873 | `	 * before it looks at the format at all, so the central screen stands aside` |
|         - |  874 | `	 * and PH7_FormatCheckFormatArg() in the body raises the same TypeError` |
|         - |  875 | `	 * after the handle has been accepted. */` |
|         - |  876 | `	{ "fprintf", "$stream, ~string $format, mixed ...$values = ?", "int" },` |
|         - |  877 | `	{ "fputcsv", "$stream, array $fields, string $separator = ',', string $enclosure = '\"', string $escape = '\\\\', string $eol = \"\n\"", "int\|false" },` |
|         - |  878 | `	{ "fputs", "$stream, string $data, ?int $length = NULL", "int\|false" },` |
|         - |  879 | `	{ "fread", "$stream, int $length", "string\|false" },` |
|         - |  880 | `	{ "frenchtojd", "int $month, int $day, int $year", "int" },` |
|         - |  881 | `	{ "fseek", "$stream, int $offset, int $whence = SEEK_SET", "int" },` |
|         - |  882 | `	{ "fstat", "$stream", "array\|false" },` |
|         - |  883 | `	{ "ftell", "$stream", "int\|false" },` |
|         - |  884 | `	{ "ftruncate", "$stream, int $size", "bool" },` |
|         - |  885 | `	{ "func_get_arg", "int $position", "mixed" },` |
|         - |  886 | `	{ "func_get_args", "", "array" },` |
|         - |  887 | `	{ "func_num_args", "", "int" },` |
|         - |  888 | `	{ "function_exists", "string $function", "bool" },` |
|         - |  889 | `	{ "fwrite", "$stream, string $data, ?int $length = NULL", "int\|false" },` |
|         - |  890 | `	{ "gc_collect_cycles", "", "int" },` |
|         - |  891 | `	{ "gc_disable", "", "void" },` |
|         - |  892 | `	{ "gc_enable", "", "void" },` |
|         - |  893 | `	{ "gc_enabled", "", "bool" },` |
|         - |  894 | `	{ "gc_mem_caches", "", "int" },` |
|         - |  895 | `	{ "gc_status", "", "array" },` |
|         - |  896 | `	{ "get_called_class", "", "string" },` |
|         - |  897 | `	/* ext/fileinfo */` |
|         - |  898 | `	{ "finfo_open", "int $flags = FILEINFO_NONE, ?string $magic_database = null", "finfo\|false" },` |
|         - |  899 | `	{ "finfo_close", "finfo $finfo", "true" },` |
|         - |  900 | `	{ "finfo_set_flags", "finfo $finfo, int $flags", "true" },` |
|         - |  901 | `	{ "finfo_file", "finfo $finfo, string $filename, int $flags = FILEINFO_NONE, $context = null", "string\|false" },` |
|         - |  902 | `	{ "finfo_buffer", "finfo $finfo, string $string, int $flags = FILEINFO_NONE, $context = null", "string\|false" },` |
|         - |  903 | `	{ "mime_content_type", "$filename", "string\|false" },` |
|         - |  904 | `	/* ext/zlib. The gz* handle verbs are ALIASES of the stream functions above` |
|         - |  905 | `	 * (php registers them that way, and so does this build), but each carries` |
|         - |  906 | `	 * its OWN signature row -- which is what makes gzread()'s ArgumentCountError` |
|         - |  907 | `	 * and its ValueError say "gzread()" rather than "fread()". */` |
|         - |  908 | `	{ "deflate_add", "DeflateContext $context, string $data, int $flush_mode = ZLIB_SYNC_FLUSH", "string\|false" },` |
|         - |  909 | `	{ "deflate_init", "int $encoding, object\|array $options = []", "DeflateContext\|false" },` |
|         - |  910 | `	{ "gzclose", "$stream", "bool" },` |
|         - |  911 | `	{ "gzcompress", "string $data, int $level = -1, int $encoding = ZLIB_ENCODING_DEFLATE", "string\|false" },` |
|         - |  912 | `	{ "gzdecode", "string $data, int $max_length = 0", "string\|false" },` |
|         - |  913 | `	{ "gzdeflate", "string $data, int $level = -1, int $encoding = ZLIB_ENCODING_RAW", "string\|false" },` |
|         - |  914 | `	{ "gzencode", "string $data, int $level = -1, int $encoding = ZLIB_ENCODING_GZIP", "string\|false" },` |
|         - |  915 | `	{ "gzeof", "$stream", "bool" },` |
|         - |  916 | `	{ "gzfile", "string $filename, bool $use_include_path = false", "array\|false" },` |
|         - |  917 | `	{ "gzgetc", "$stream", "string\|false" },` |
|         - |  918 | `	{ "gzgets", "$stream, ?int $length = NULL", "string\|false" },` |
|         - |  919 | `	{ "gzinflate", "string $data, int $max_length = 0", "string\|false" },` |
|         - |  920 | `	{ "gzopen", "string $filename, string $mode, bool $use_include_path = false", "" },` |
|         - |  921 | `	{ "gzpassthru", "$stream", "int" },` |
|         - |  922 | `	{ "gzputs", "$stream, string $data, ?int $length = NULL", "int\|false" },` |
|         - |  923 | `	{ "gzread", "$stream, int $length", "string\|false" },` |
|         - |  924 | `	{ "gzrewind", "$stream", "bool" },` |
|         - |  925 | `	{ "gzseek", "$stream, int $offset, int $whence = SEEK_SET", "int" },` |
|         - |  926 | `	{ "gztell", "$stream", "int\|false" },` |
|         - |  927 | `	{ "gzuncompress", "string $data, int $max_length = 0", "string\|false" },` |
|         - |  928 | `	{ "gzwrite", "$stream, string $data, ?int $length = NULL", "int\|false" },` |
|         - |  929 | `	{ "inflate_add", "InflateContext $context, string $data, int $flush_mode = ZLIB_SYNC_FLUSH", "string\|false" },` |
|         - |  930 | `	{ "inflate_get_read_len", "InflateContext $context", "int" },` |
|         - |  931 | `	{ "inflate_get_status", "InflateContext $context", "int" },` |
|         - |  932 | `	{ "inflate_init", "int $encoding, object\|array $options = []", "InflateContext\|false" },` |
|         - |  933 | `	{ "ob_gzhandler", "string $data, int $flags", "string\|false" },` |
|         - |  934 | `	{ "readgzfile", "string $filename, bool $use_include_path = false", "int\|false" },` |
|         - |  935 | `	{ "zlib_decode", "string $data, int $max_length = 0", "string\|false" },` |
|         - |  936 | `	{ "zlib_encode", "string $data, int $encoding, int $level = -1", "string\|false" },` |
|         - |  937 | `	{ "zlib_get_coding_type", "", "string\|false" },` |
|         - |  938 | `	/* ext/gettext */` |
|         - |  939 | `	{ "_", "string $message", "string" },` |
|         - |  940 | `	{ "bind_textdomain_codeset", "string $domain, ?string $codeset = NULL", "string\|false" },` |
|         - |  941 | `	{ "bindtextdomain", "string $domain, ?string $directory = NULL", "string\|false" },` |
|         - |  942 | `	{ "dcgettext", "string $domain, string $message, int $category", "string" },` |
|         - |  943 | `	{ "dcngettext", "string $domain, string $singular, string $plural, int $count, int $category", "string" },` |
|         - |  944 | `	{ "dgettext", "string $domain, string $message", "string" },` |
|         - |  945 | `	{ "dngettext", "string $domain, string $singular, string $plural, int $count", "string" },` |
|         - |  946 | `	{ "gettext", "string $message", "string" },` |
|         - |  947 | `	{ "ngettext", "string $singular, string $plural, int $count", "string" },` |
|         - |  948 | `	{ "textdomain", "?string $domain = NULL", "string" },` |
|         - |  949 | ``	/* ext/standard's syslog trio. All three answer `true` and nothing else --`` |
|         - |  950 | ``	 * php declares the return type as the literal `true`, not `bool`. */`` |
|         - |  951 | `	{ "openlog", "string $prefix, int $flags, int $facility", "true" },` |
|         - |  952 | `	{ "closelog", "", "true" },` |
|         - |  953 | `	{ "syslog", "int $priority, string $message", "true" },` |
|         - |  954 | `	/* ext/pcntl. Only reachable where the extension is built, but the table is` |
|         - |  955 | `	 * a DECLARATION rather than a registration -- Reflection filters it against` |
|         - |  956 | `	 * the live VM, so the rows cost nothing on Windows. */` |
|         - |  957 | `	{ "pcntl_alarm", "int $seconds", "int" },` |
|         - |  958 | `	{ "pcntl_async_signals", "?bool $enable = NULL", "bool" },` |
|         - |  959 | `	{ "pcntl_errno", "", "int" },` |
|         - |  960 | `	{ "pcntl_exec", "string $path, array $args = [], array $env_vars = []", "false" },` |
|         - |  961 | `	{ "pcntl_fork", "", "int" },` |
|         - |  962 | `	{ "pcntl_get_last_error", "", "int" },` |
|         - |  963 | `	{ "pcntl_getcpu", "", "int" },` |
|         - |  964 | ``	/* ext/sockets. `socket_export_stream()` is the one row with no return type`` |
|         - |  965 | `	 * at all: php declares none for it, so Reflection answers null. */` |
|         - |  966 | `	{ "socket_select", "?array &$read, ?array &$write, ?array &$except, ?int $seconds, int $microseconds = 0", "int\|false" },` |
|         - |  967 | `	/* SOMAXCONN, which is 4096 on Linux and 2147483647 on Windows -- so the` |
|         - |  968 | `	 * default is stated by NAME rather than as the number php prints here. */` |
|         - |  969 | `	{ "socket_create_listen", "int $port, int $backlog = SOMAXCONN", "Socket\|false" },` |
|         - |  970 | `	{ "socket_accept", "Socket $socket", "Socket\|false" },` |
|         - |  971 | `	{ "socket_set_nonblock", "Socket $socket", "bool" },` |
|         - |  972 | `	{ "socket_set_block", "Socket $socket", "bool" },` |
|         - |  973 | `	{ "socket_listen", "Socket $socket, int $backlog = 0", "bool" },` |
|         - |  974 | `	{ "socket_close", "Socket $socket", "void" },` |
|         - |  975 | `	{ "socket_write", "Socket $socket, string $data, ?int $length = null", "int\|false" },` |
|         - |  976 | `	{ "socket_read", "Socket $socket, int $length, int $mode = 2", "string\|false" },` |
|         - |  977 | `	{ "socket_getsockname", "Socket $socket, &$address, &$port = null", "bool" },` |
|         - |  978 | `	{ "socket_getpeername", "Socket $socket, &$address, &$port = null", "bool" },` |
|         - |  979 | `	{ "socket_create", "int $domain, int $type, int $protocol", "Socket\|false" },` |
|         - |  980 | `	{ "socket_connect", "Socket $socket, string $address, ?int $port = null", "bool" },` |
|         - |  981 | `	{ "socket_strerror", "int $error_code", "string" },` |
|         - |  982 | `	{ "socket_bind", "Socket $socket, string $address, int $port = 0", "bool" },` |
|         - |  983 | `	{ "socket_recv", "Socket $socket, &$data, int $length, int $flags", "int\|false" },` |
|         - |  984 | `	{ "socket_send", "Socket $socket, string $data, int $length, int $flags", "int\|false" },` |
|         - |  985 | `	{ "socket_recvfrom", "Socket $socket, &$data, int $length, int $flags, &$address, &$port = null", "int\|false" },` |
|         - |  986 | `	{ "socket_sendto", "Socket $socket, string $data, int $length, int $flags, string $address, ?int $port = null", "int\|false" },` |
|         - |  987 | `	{ "socket_get_option", "Socket $socket, int $level, int $option", "array\|int\|false" },` |
|         - |  988 | `	{ "socket_getopt", "Socket $socket, int $level, int $option", "array\|int\|false" },` |
|         - |  989 | `	{ "socket_set_option", "Socket $socket, int $level, int $option, $value", "bool" },` |
|         - |  990 | `	{ "socket_setopt", "Socket $socket, int $level, int $option, $value", "bool" },` |
|         - |  991 | `	{ "socket_create_pair", "int $domain, int $type, int $protocol, &$pair", "bool" },` |
|         - |  992 | `	{ "socket_shutdown", "Socket $socket, int $mode = 2", "bool" },` |
|         - |  993 | `	{ "socket_atmark", "Socket $socket", "bool" },` |
|         - |  994 | `	{ "socket_last_error", "?Socket $socket = null", "int" },` |
|         - |  995 | `	{ "socket_clear_error", "?Socket $socket = null", "void" },` |
|         - |  996 | `	{ "socket_import_stream", "$stream", "Socket\|false" },` |
|         - |  997 | `	{ "socket_export_stream", "Socket $socket", "" },` |
|         - |  998 | `	{ "socket_sendmsg", "Socket $socket, array $message, int $flags = 0", "int\|false" },` |
|         - |  999 | `	{ "socket_recvmsg", "Socket $socket, array &$message, int $flags = 0", "int\|false" },` |
|         - | 1000 | `	{ "socket_cmsg_space", "int $level, int $type, int $num = 0", "?int" },` |
|         - | 1001 | `	{ "socket_addrinfo_lookup", "string $host, ?string $service = null, array $hints = []", "array\|false" },` |
|         - | 1002 | `	{ "socket_addrinfo_connect", "AddressInfo $address", "Socket\|false" },` |
|         - | 1003 | `	{ "socket_addrinfo_bind", "AddressInfo $address", "Socket\|false" },` |
|         - | 1004 | `	{ "socket_addrinfo_explain", "AddressInfo $address", "array" },` |
|         - | 1005 | `	/* Windows only; the rows are harmless on a build that registers no such` |
|         - | 1006 | `	 * name, since the stamping pass filters against the live VM. */` |
|         - | 1007 | `	{ "socket_wsaprotocol_info_export", "Socket $socket, int $process_id", "string\|false" },` |
|         - | 1008 | `	{ "socket_wsaprotocol_info_import", "string $info_id", "Socket\|false" },` |
|         - | 1009 | `	{ "socket_wsaprotocol_info_release", "string $info_id", "bool" },` |
|         - | 1010 | `	{ "pcntl_getcpuaffinity", "?int $process_id = NULL", "array\|false" },` |
|         - | 1011 | `	{ "pcntl_getpriority", "?int $process_id = NULL, int $mode = PRIO_PROCESS", "int\|false" },` |
|         - | 1012 | `	{ "pcntl_setcpuaffinity", "?int $process_id = NULL, array $cpu_ids = []", "bool" },` |
|         - | 1013 | `	{ "pcntl_setpriority", "int $priority, ?int $process_id = NULL, int $mode = PRIO_PROCESS", "bool" },` |
|         - | 1014 | `	{ "pcntl_signal", "int $signal, $handler, bool $restart_syscalls = true", "bool" },` |
|         - | 1015 | `	{ "pcntl_signal_dispatch", "", "bool" },` |
|         - | 1016 | `	{ "pcntl_signal_get_handler", "int $signal", "" },` |
|         - | 1017 | `	{ "pcntl_sigprocmask", "int $mode, array $signals, &$old_signals = NULL", "bool" },` |
|         - | 1018 | `	{ "pcntl_sigtimedwait", "array $signals, &$info = [], int $seconds = 0, int $nanoseconds = 0", "int\|false" },` |
|         - | 1019 | `	{ "pcntl_sigwaitinfo", "array $signals, &$info = []", "int\|false" },` |
|         - | 1020 | `	{ "pcntl_strerror", "int $error_code", "string" },` |
|         - | 1021 | `	{ "pcntl_unshare", "int $flags", "bool" },` |
|         - | 1022 | `	{ "pcntl_wait", "&$status, int $flags = 0, &$resource_usage = []", "int" },` |
|         - | 1023 | `	{ "pcntl_waitid", "int $idtype = P_ALL, ?int $id = NULL, &$info = [], int $flags = WEXITED, &$resource_usage = []", "bool" },` |
|         - | 1024 | `	{ "pcntl_waitpid", "int $process_id, &$status, int $flags = 0, &$resource_usage = []", "int" },` |
|         - | 1025 | `	{ "pcntl_wexitstatus", "int $status", "int\|false" },` |
|         - | 1026 | `	{ "pcntl_wifcontinued", "int $status", "bool" },` |
|         - | 1027 | `	{ "pcntl_wifexited", "int $status", "bool" },` |
|         - | 1028 | `	{ "pcntl_wifsignaled", "int $status", "bool" },` |
|         - | 1029 | `	{ "pcntl_wifstopped", "int $status", "bool" },` |
|         - | 1030 | `	{ "pcntl_wstopsig", "int $status", "int\|false" },` |
|         - | 1031 | `	{ "pcntl_wtermsig", "int $status", "int\|false" },` |
|         - | 1032 | `	/* ext/posix */` |
|         - | 1033 | `	{ "posix_access", "string $filename, int $flags = 0", "bool" },` |
|         - | 1034 | `	{ "posix_ctermid", "", "string\|false" },` |
|         - | 1035 | `	{ "posix_eaccess", "string $filename, int $flags = 0", "bool" },` |
|         - | 1036 | `	{ "posix_errno", "", "int" },` |
|         - | 1037 | `	{ "posix_fpathconf", "$file_descriptor, int $name", "int\|false" },` |
|         - | 1038 | `	{ "posix_get_last_error", "", "int" },` |
|         - | 1039 | `	{ "posix_getcwd", "", "string\|false" },` |
|         - | 1040 | `	{ "posix_getegid", "", "int" },` |
|         - | 1041 | `	{ "posix_geteuid", "", "int" },` |
|         - | 1042 | `	{ "posix_getgid", "", "int" },` |
|         - | 1043 | `	{ "posix_getgrgid", "int $group_id", "array\|false" },` |
|         - | 1044 | `	{ "posix_getgrnam", "string $name", "array\|false" },` |
|         - | 1045 | `	{ "posix_getgroups", "", "array\|false" },` |
|         - | 1046 | `	{ "posix_getlogin", "", "string\|false" },` |
|         - | 1047 | `	{ "posix_getpgid", "int $process_id", "int\|false" },` |
|         - | 1048 | `	{ "posix_getpgrp", "", "int" },` |
|         - | 1049 | `	{ "posix_getpid", "", "int" },` |
|         - | 1050 | `	{ "posix_getppid", "", "int" },` |
|         - | 1051 | `	{ "posix_getpwnam", "string $username", "array\|false" },` |
|         - | 1052 | `	{ "posix_getpwuid", "int $user_id", "array\|false" },` |
|         - | 1053 | `	{ "posix_getrlimit", "?int $resource = NULL", "array\|false" },` |
|         - | 1054 | `	{ "posix_getsid", "int $process_id", "int\|false" },` |
|         - | 1055 | `	{ "posix_getuid", "", "int" },` |
|         - | 1056 | `	{ "posix_initgroups", "string $username, int $group_id", "bool" },` |
|         - | 1057 | `	{ "posix_isatty", "$file_descriptor", "bool" },` |
|         - | 1058 | `	{ "posix_kill", "int $process_id, int $signal", "bool" },` |
|         - | 1059 | `	{ "posix_mkfifo", "string $filename, int $permissions", "bool" },` |
|         - | 1060 | `	{ "posix_mknod", "string $filename, int $flags, int $major = 0, int $minor = 0", "bool" },` |
|         - | 1061 | `	{ "posix_pathconf", "string $path, int $name", "int\|false" },` |
|         - | 1062 | `	{ "posix_setegid", "int $group_id", "bool" },` |
|         - | 1063 | `	{ "posix_seteuid", "int $user_id", "bool" },` |
|         - | 1064 | `	{ "posix_setgid", "int $group_id", "bool" },` |
|         - | 1065 | `	{ "posix_setpgid", "int $process_id, int $process_group_id", "bool" },` |
|         - | 1066 | `	{ "posix_setrlimit", "int $resource, int $soft_limit, int $hard_limit", "bool" },` |
|         - | 1067 | `	{ "posix_setsid", "", "int" },` |
|         - | 1068 | `	{ "posix_setuid", "int $user_id", "bool" },` |
|         - | 1069 | `	{ "posix_strerror", "int $error_code", "string" },` |
|         - | 1070 | `	{ "posix_sysconf", "int $conf_id", "int" },` |
|         - | 1071 | `	{ "posix_times", "", "array\|false" },` |
|         - | 1072 | `	{ "posix_ttyname", "$file_descriptor", "string\|false" },` |
|         - | 1073 | `	{ "posix_uname", "", "array\|false" },` |
|         - | 1074 | `	{ "get_class", "object $object = ?", "string" },` |
|         - | 1075 | `	{ "get_class_methods", "object\|string $object_or_class", "array" },` |
|         - | 1076 | `	{ "get_class_vars", "string $class", "array" },` |
|         - | 1077 | `	{ "get_current_user", "", "string" },` |
|         - | 1078 | `	{ "get_declared_classes", "", "array" },` |
|         - | 1079 | `	{ "get_declared_interfaces", "", "array" },` |
|         - | 1080 | `	{ "get_declared_traits", "", "array" },` |
|         - | 1081 | `	{ "get_defined_constants", "bool $categorize = false", "array" },` |
|         - | 1082 | `	{ "get_defined_functions", "bool $exclude_disabled = true", "array" },` |
|         - | 1083 | `	{ "get_defined_vars", "", "array" },` |
|         - | 1084 | `	{ "get_headers", "string $url, bool $associative = false, $context = NULL", "array\|false" },` |
|         - | 1085 | `	{ "get_html_translation_table", "int $table = HTML_SPECIALCHARS, int $flags = ENT_QUOTES \| ENT_SUBSTITUTE \| ENT_HTML401, string $encoding = 'UTF-8'", "array" },` |
|         - | 1086 | `	{ "get_include_path", "", "string\|false" },` |
|         - | 1087 | `	{ "get_included_files", "", "array" },` |
|         - | 1088 | `	{ "get_required_files", "", "array" },` |
|         - | 1089 | `	{ "get_extension_funcs", "string $extension", "array\|false" },` |
|         - | 1090 | `	{ "get_loaded_extensions", "bool $zend_extensions = false", "array" },` |
|         - | 1091 | `	{ "get_mangled_object_vars", "object $object", "array" },` |
|         - | 1092 | `	{ "get_object_vars", "object $object", "array" },` |
|         - | 1093 | `	{ "get_parent_class", "~object\|string $object_or_class = ?", "string\|false" },` |
|         - | 1094 | `	{ "get_resource_id", "$resource", "int" },` |
|         - | 1095 | `	{ "get_resource_type", "$resource", "string" },` |
|         - | 1096 | `	{ "getcwd", "", "string\|false" },` |
|         - | 1097 | `	{ "getdate", "?int $timestamp = NULL", "array" },` |
|         - | 1098 | `	{ "getenv", "?string $name = NULL, bool $local_only = false", "array\|string\|false" },` |
|         - | 1099 | `	{ "gethostname", "", "string\|false" },` |
|         - | 1100 | `	{ "getimagesize", "string $filename, &$image_info = NULL", "array\|false" },` |
|         - | 1101 | `	{ "getimagesizefromstring", "string $string, &$image_info = NULL", "array\|false" },` |
|         - | 1102 | `	{ "getmygid", "", "int\|false" },` |
|         - | 1103 | `	{ "getmypid", "", "int\|false" },` |
|         - | 1104 | `	{ "getmyuid", "", "int\|false" },` |
|         - | 1105 | `	{ "getopt", "string $short_options, array $long_options = [], &$rest_index = NULL", "array\|false" },` |
|         - | 1106 | `	{ "getrandmax", "", "int" },` |
|         - | 1107 | `	{ "gettimeofday", "bool $as_float = false", "array\|float" },` |
|         - | 1108 | `	{ "gettype", "mixed $value", "string" },` |
|         - | 1109 | `	{ "get_debug_type", "mixed $value", "string" },` |
|         - | 1110 | `	{ "gmdate", "string $format, ?int $timestamp = NULL", "string" },` |
|         - | 1111 | `	{ "gmmktime", "int $hour, ?int $minute = NULL, ?int $second = NULL, ?int $month = NULL, ?int $day = NULL, ?int $year = NULL", "int\|false" },` |
|         - | 1112 | `	{ "hash", "string $algo, string $data, bool $binary = false, array $options = []", "string" },` |
|         - | 1113 | `	{ "hash_algos", "", "array" },` |
|         - | 1114 | `	{ "hash_equals", "string $known_string, string $user_string", "bool" },` |
|         - | 1115 | `	{ "hash_hmac", "string $algo, string $data, string $key, bool $binary = false", "string" },` |
|         - | 1116 | `	{ "hash_hmac_algos", "", "array" },` |
|         - | 1117 | `	{ "hash_init", "string $algo, int $flags = 0, string $key = \'\', array $options = []", "HashContext" },` |
|         - | 1118 | `	{ "hash_update", "HashContext $context, string $data", "true" },` |
|         - | 1119 | `	{ "hash_final", "HashContext $context, bool $binary = false", "string" },` |
|         - | 1120 | `	{ "hash_copy", "HashContext $context", "HashContext" },` |
|         - | 1121 | `	{ "hash_file", "string $algo, string $filename, bool $binary = false, array $options = []", "string\|false" },` |
|         - | 1122 | `	{ "hash_hkdf", "string $algo, string $key, int $length = 0, string $info = \'\', string $salt = \'\'", "string" },` |
|         - | 1123 | `	{ "hash_pbkdf2", "string $algo, string $password, string $salt, int $iterations, int $length = 0, bool $binary = false, array $options = []", "string" },` |
|         - | 1124 | `	{ "hash_hmac_file", "string $algo, string $filename, string $key, bool $binary = false", "string\|false" },` |
|         - | 1125 | `	{ "hash_update_file", "HashContext $context, string $filename, $stream_context = null", "bool" },` |
|         - | 1126 | `	{ "hash_update_stream", "HashContext $context, $stream, int $length = -1", "int" },` |
|         - | 1127 | `	{ "gregoriantojd", "int $month, int $day, int $year", "int" },` |
|         - | 1128 | `	{ "header", "string $header, bool $replace = true, int $response_code = 0", "void" },` |
|         - | 1129 | `	{ "header_remove", "?string $name = NULL", "void" },` |
|         - | 1130 | `	{ "headers_list", "", "array" },` |
|         - | 1131 | `	{ "headers_sent", "&$filename = NULL, &$line = NULL", "bool" },` |
|         - | 1132 | `	{ "hexdec", "string $hex_string", "int\|float" },` |
|         - | 1133 | `	{ "html_entity_decode", "string $string, int $flags = ENT_QUOTES \| ENT_SUBSTITUTE \| ENT_HTML401, ?string $encoding = NULL", "string" },` |
|         - | 1134 | `	{ "http_build_query", "object\|array $data, string $numeric_prefix = '', ?string $arg_separator = null, int $encoding_type = PHP_QUERY_RFC1738", "string" },` |
|         - | 1135 | `	{ "htmlentities", "string $string, int $flags = ENT_QUOTES \| ENT_SUBSTITUTE \| ENT_HTML401, ?string $encoding = NULL, bool $double_encode = true", "string" },` |
|         - | 1136 | `	{ "htmlspecialchars", "string $string, int $flags = ENT_QUOTES \| ENT_SUBSTITUTE \| ENT_HTML401, ?string $encoding = NULL, bool $double_encode = true", "string" },` |
|         - | 1137 | `	{ "htmlspecialchars_decode", "string $string, int $flags = ENT_QUOTES \| ENT_SUBSTITUTE \| ENT_HTML401", "string" },` |
|         - | 1138 | `	{ "http_clear_last_response_headers", "", "void" },` |
|         - | 1139 | `	{ "http_get_last_response_headers", "", "?array" },` |
|         - | 1140 | `	{ "http_response_code", "int $response_code = 0", "int\|bool" },` |
|         - | 1141 | `	{ "hypot", "float $x, float $y", "float" },` |
|         - | 1142 | `	{ "idate", "string $format, ?int $timestamp = NULL", "int\|false" },` |
|         - | 1143 | `	{ "ignore_user_abort", "?bool $enable = NULL", "int" },` |
|         - | 1144 | `	{ "image_type_to_mime_type", "int $image_type", "string" },` |
|         - | 1145 | `	{ "image_type_to_extension", "int $image_type, bool $include_dot = true", "string\|false" },` |
|         - | 1146 | `	{ "implode", "array\|string $separator, ?array $array = NULL", "string" },` |
|         - | 1147 | `	{ "in_array", "mixed $needle, array $haystack, bool $strict = false", "bool" },` |
|         - | 1148 | `	{ "inet_ntop", "string $ip", "string\|false" },` |
|         - | 1149 | `	{ "inet_pton", "string $ip", "string\|false" },` |
|         - | 1150 | `	{ "intdiv", "int $num1, int $num2", "int" },` |
|         - | 1151 | `	{ "interface_exists", "string $interface, bool $autoload = true", "bool" },` |
|         - | 1152 | `	{ "trait_exists", "string $trait, bool $autoload = true", "bool" },` |
|         - | 1153 | `	{ "intval", "mixed $value, int $base = 10", "int" },` |
|         - | 1154 | `	{ "is_a", "mixed $object_or_class, string $class, bool $allow_string = false", "bool" },` |
|         - | 1155 | `	{ "is_array", "mixed $value", "bool" },` |
|         - | 1156 | `	{ "is_bool", "mixed $value", "bool" },` |
|         - | 1157 | `	{ "is_callable", "mixed $value, bool $syntax_only = false, &$callable_name = NULL", "bool" },` |
|         - | 1158 | `	{ "is_dir", "string $filename", "bool" },` |
|         - | 1159 | `	{ "is_double", "mixed $value", "bool" },` |
|         - | 1160 | `	{ "is_executable", "string $filename", "bool" },` |
|         - | 1161 | `	{ "is_file", "string $filename", "bool" },` |
|         - | 1162 | `	{ "is_float", "mixed $value", "bool" },` |
|         - | 1163 | `	{ "is_int", "mixed $value", "bool" },` |
|         - | 1164 | `	{ "is_integer", "mixed $value", "bool" },` |
|         - | 1165 | `	{ "is_link", "string $filename", "bool" },` |
|         - | 1166 | `	{ "is_long", "mixed $value", "bool" },` |
|         - | 1167 | `	{ "is_null", "mixed $value", "bool" },` |
|         - | 1168 | `	{ "is_numeric", "mixed $value", "bool" },` |
|         - | 1169 | `	{ "is_object", "mixed $value", "bool" },` |
|         - | 1170 | `	{ "is_readable", "string $filename", "bool" },` |
|         - | 1171 | `	{ "is_resource", "mixed $value", "bool" },` |
|         - | 1172 | `	{ "is_scalar", "mixed $value", "bool" },` |
|         - | 1173 | `	{ "is_string", "mixed $value", "bool" },` |
|         - | 1174 | `	{ "is_subclass_of", "mixed $object_or_class, string $class, bool $allow_string = true", "bool" },` |
|         - | 1175 | `	{ "is_writable", "string $filename", "bool" },` |
|         - | 1176 | `	{ "is_writeable", "string $filename", "bool" },` |
|         - | 1177 | `	{ "iterator_apply", "Traversable $iterator, callable $callback, ?array $args = NULL", "int" },` |
|         - | 1178 | `	{ "iterator_count", "Traversable\|array $iterator", "int" },` |
|         - | 1179 | `	{ "iterator_to_array", "Traversable\|array $iterator, bool $preserve_keys = true", "array" },` |
|         - | 1180 | `	{ "jddayofweek", "int $julian_day, int $mode = CAL_DOW_DAYNO", "string\|int" },` |
|         - | 1181 | `	{ "jdmonthname", "int $julian_day, int $mode", "string" },` |
|         - | 1182 | `	{ "jdtofrench", "int $julian_day", "string" },` |
|         - | 1183 | `	{ "jdtogregorian", "int $julian_day", "string" },` |
|         - | 1184 | `	{ "jdtojewish", "int $julian_day, bool $hebrew = false, int $flags = 0", "string" },` |
|         - | 1185 | `	{ "jdtojulian", "int $julian_day", "string" },` |
|         - | 1186 | `	{ "jdtounix", "int $julian_day", "int" },` |
|         - | 1187 | `	{ "jewishtojd", "int $month, int $day, int $year", "int" },` |
|         - | 1188 | `	{ "join", "array\|string $separator, ?array $array = NULL", "string" },` |
|         - | 1189 | `	{ "juliantojd", "int $month, int $day, int $year", "int" },` |
|         - | 1190 | `	{ "json_decode", "string $json, ?bool $associative = NULL, int $depth = 512, int $flags = 0", "mixed" },` |
|         - | 1191 | `	{ "json_encode", "mixed $value, int $flags = 0, int $depth = 512", "string\|false" },` |
|         - | 1192 | `	{ "json_last_error", "", "int" },` |
|         - | 1193 | `	{ "json_last_error_msg", "", "string" },` |
|         - | 1194 | `	{ "json_validate", "string $json, int $depth = 512, int $flags = 0", "bool" },` |
|         - | 1195 | `	{ "key", "object\|array $array", "string\|int\|null" },` |
|         - | 1196 | `	{ "key_exists", "$key, array $array", "bool" },` |
|         - | 1197 | `	{ "krsort", "array &$array, int $flags = SORT_REGULAR", "true" },` |
|         - | 1198 | `	{ "ksort", "array &$array, int $flags = SORT_REGULAR", "true" },` |
|         - | 1199 | `	{ "lcfirst", "string $string", "string" },` |
|         - | 1200 | `	{ "levenshtein", "string $string1, string $string2, int $insertion_cost = 1, int $replacement_cost = 1, int $deletion_cost = 1", "int" },` |
|         - | 1201 | `	{ "link", "string $target, string $link", "bool" },` |
|         - | 1202 | `	{ "localtime", "?int $timestamp = NULL, bool $associative = false", "array" },` |
|         - | 1203 | `	{ "log", "float $num, float $base = M_E", "float" },` |
|         - | 1204 | `	{ "log10", "float $num", "float" },` |
|         - | 1205 | `	{ "log1p", "float $num", "float" },` |
|         - | 1206 | `	{ "lstat", "string $filename", "array\|false" },` |
|         - | 1207 | `	{ "ltrim", "string $string, string $characters = \" \\n\\r\\t\\v\\x00\"", "string" },` |
|         - | 1208 | `	{ "max", "mixed $value, mixed ...$values = ?", "mixed" },` |
|         - | 1209 | `	{ "mb_chr", "int $codepoint, ?string $encoding = NULL", "string\|false" },` |
|         - | 1210 | `	{ "mb_convert_encoding", "array\|string $string, string $to_encoding, array\|string\|null $from_encoding = NULL", "array\|string\|false" },` |
|         - | 1211 | `	{ "mb_ord", "string $string, ?string $encoding = NULL", "int\|false" },` |
|         - | 1212 | `	{ "mb_ltrim", "string $string, ?string $characters = null, ?string $encoding = null", "string" },` |
|         - | 1213 | `	{ "mb_rtrim", "string $string, ?string $characters = null, ?string $encoding = null", "string" },` |
|         - | 1214 | `	{ "mb_lcfirst", "string $string, ?string $encoding = null", "string" },` |
|         - | 1215 | `	{ "mb_strtolower", "string $string, ?string $encoding = NULL", "string" },` |
|         - | 1216 | `	{ "mb_strtoupper", "string $string, ?string $encoding = NULL", "string" },` |
|         - | 1217 | `	{ "mb_trim", "string $string, ?string $characters = null, ?string $encoding = null", "string" },` |
|         - | 1218 | `	{ "mb_ucfirst", "string $string, ?string $encoding = null", "string" },` |
|         - | 1219 | `	{ "iconv", "string $from_encoding, string $to_encoding, string $string", "string\|false" },` |
|         - | 1220 | `	{ "iconv_strlen", "string $string, ?string $encoding = NULL", "int\|false" },` |
|         - | 1221 | `	{ "iconv_substr", "string $string, int $offset, ?int $length = NULL, ?string $encoding = NULL", "string\|false" },` |
|         - | 1222 | `	{ "iconv_strpos", "string $haystack, string $needle, int $offset = 0, ?string $encoding = NULL", "int\|false" },` |
|         - | 1223 | `	{ "iconv_strrpos", "string $haystack, string $needle, ?string $encoding = NULL", "int\|false" },` |
|         - | 1224 | `	{ "iconv_get_encoding", "string $type = \"all\"", "array\|string\|false" },` |
|         - | 1225 | `	{ "iconv_mime_encode", "string $field_name, string $field_value, array $options = []", "string\|false" },` |
|         - | 1226 | `	{ "iconv_mime_decode", "string $string, int $mode = 0, ?string $encoding = NULL", "string\|false" },` |
|         - | 1227 | `	{ "iconv_mime_decode_headers", "string $headers, int $mode = 0, ?string $encoding = NULL", "array\|false" },` |
|         - | 1228 | `	{ "md5", "string $string, bool $binary = false", "string" },` |
|         - | 1229 | `	{ "md5_file", "string $filename, bool $binary = false", "string\|false" },` |
|         - | 1230 | `	{ "metaphone", "string $string, int $max_phonemes = 0", "string" },` |
|         - | 1231 | `	{ "method_exists", "$object_or_class, string $method", "bool" },` |
|         - | 1232 | `	{ "memory_get_peak_usage", "bool $real_usage = false", "int" },` |
|         - | 1233 | `	{ "memory_get_usage", "bool $real_usage = false", "int" },` |
|         - | 1234 | `	{ "microtime", "bool $as_float = false", "string\|float" },` |
|         - | 1235 | `	{ "min", "mixed $value, mixed ...$values = ?", "mixed" },` |
|         - | 1236 | `	{ "mkdir", "string $directory, int $permissions = 0777, bool $recursive = false, $context = NULL", "bool" },` |
|         - | 1237 | `	{ "mktime", "int $hour, ?int $minute = NULL, ?int $second = NULL, ?int $month = NULL, ?int $day = NULL, ?int $year = NULL", "int\|false" },` |
|         - | 1238 | `	{ "mt_getrandmax", "", "int" },` |
|         - | 1239 | `	{ "mt_rand", "int $min = ?, int $max = ?", "int" },` |
|         - | 1240 | `	{ "mt_srand", "?int $seed = NULL, int $mode = MT_RAND_MT19937", "void" },` |
|         - | 1241 | `	{ "natcasesort", "array &$array", "true" },` |
|         - | 1242 | `	{ "natsort", "array &$array", "true" },` |
|         - | 1243 | `	{ "next", "object\|array &$array", "mixed" },` |
|         - | 1244 | `	{ "nl2br", "string $string, bool $use_xhtml = true", "string" },` |
|         - | 1245 | `	{ "number_format", "float $num, int $decimals = 0, ?string $decimal_separator = '.', ?string $thousands_separator = ','", "string" },` |
|         - | 1246 | `	{ "ob_clean", "", "bool" },` |
|         - | 1247 | `	{ "ob_end_clean", "", "bool" },` |
|         - | 1248 | `	{ "ob_end_flush", "", "bool" },` |
|         - | 1249 | `	{ "ob_flush", "", "bool" },` |
|         - | 1250 | `	{ "ob_get_clean", "", "string\|false" },` |
|         - | 1251 | `	{ "ob_get_contents", "", "string\|false" },` |
|         - | 1252 | `	{ "ob_get_flush", "", "string\|false" },` |
|         - | 1253 | `	{ "ob_get_length", "", "int\|false" },` |
|         - | 1254 | `	{ "ob_get_level", "", "int" },` |
|         - | 1255 | `	{ "ob_get_status", "bool $full_status = false", "array" },` |
|         - | 1256 | `	{ "ob_implicit_flush", "bool $enable = true", "void" },` |
|         - | 1257 | `	{ "ob_list_handlers", "", "array" },` |
|         - | 1258 | `	{ "ob_start", "$callback = NULL, int $chunk_size = 0, int $flags = PHP_OUTPUT_HANDLER_STDFLAGS", "bool" },` |
|         - | 1259 | `	{ "octdec", "string $octal_string", "int\|float" },` |
|         - | 1260 | `	{ "opendir", "string $directory, $context = NULL", "" },` |
|         - | 1261 | `	{ "ord", "string $character", "int" },` |
|         - | 1262 | `	{ "pack", "string $format, mixed ...$values = ?", "string" },` |
|         - | 1263 | `	{ "sscanf", "string $string, string $format, mixed &...$vars = ?", "array\|int\|null" },` |
|         - | 1264 | `	{ "fscanf", "$stream, string $format, mixed &...$vars = ?", "array\|int\|false\|null" },` |
|         - | 1265 | `	{ "unpack", "string $format, string $string, int $offset = 0", "array\|false" },` |
|         - | 1266 | `	{ "parse_ini_file", "string $filename, bool $process_sections = false, int $scanner_mode = INI_SCANNER_NORMAL", "array\|false" },` |
|         - | 1267 | `	{ "parse_ini_string", "string $ini_string, bool $process_sections = false, int $scanner_mode = INI_SCANNER_NORMAL", "array\|false" },` |
|         - | 1268 | `	{ "parse_str", "string $string, &$result", "void" },` |
|         - | 1269 | `	{ "parse_url", "string $url, int $component = -1", "array\|string\|int\|false\|null" },` |
|         - | 1270 | `	{ "crypt", "string $string, string $salt", "string" },` |
|         - | 1271 | `	{ "password_algos", "", "array" },` |
|         - | 1272 | `	{ "password_get_info", "string $hash", "array" },` |
|         - | 1273 | `	{ "password_hash", "string $password, string\|int\|null $algo, array $options = []", "string" },` |
|         - | 1274 | `	{ "password_needs_rehash", "string $hash, string\|int\|null $algo, array $options = []", "bool" },` |
|         - | 1275 | `	{ "password_verify", "string $password, string $hash", "bool" },` |
|         - | 1276 | `	{ "passthru", "string $command, &$result_code = NULL", "?false" },` |
|         - | 1277 | `	{ "pathinfo", "string $path, int $flags = PATHINFO_ALL", "array\|string" },` |
|         - | 1278 | `	{ "pclose", "$handle", "int" },` |
|         - | 1279 | `	{ "php_sapi_name", "", "string\|false" },` |
|         - | 1280 | `	{ "php_ini_loaded_file", "", "string\|false" },` |
|         - | 1281 | `	{ "php_ini_scanned_files", "", "string\|false" },` |
|         - | 1282 | `	{ "php_uname", "string $mode = 'a'", "string" },` |
|         - | 1283 | ``	/* php spells this default `INFO_ALL`, and this engine spells the NUMBER: the`` |
|         - | 1284 | `	 * INFO_* family has no consumer here, since phpinfo() ignores $flags and` |
|         - | 1285 | `	 * prints the whole page whatever it is given. The names ship with the` |
|         - | 1286 | `	 * section filter or not at all (recorded). */` |
|         - | 1287 | `	{ "phpinfo", "int $flags = 4294967295", "true" },` |
|         - | 1288 | `	{ "phpversion", "?string $extension = NULL", "string\|false" },` |
|         - | 1289 | `	{ "pi", "", "float" },` |
|         - | 1290 | `	{ "popen", "string $command, string $mode", "" },` |
|         - | 1291 | `	{ "pos", "object\|array $array", "mixed" },` |
|         - | 1292 | `	{ "pow", "mixed $num, mixed $exponent", "object\|int\|float" },` |
|         - | 1293 | `	{ "preg_last_error", "", "int" },` |
|         - | 1294 | `	{ "preg_last_error_msg", "", "string" },` |
|         - | 1295 | `	{ "fsockopen", "string $hostname, int $port = -1, &$error_code = NULL, &$error_message = NULL, ?float $timeout = NULL", "" },` |
|         - | 1296 | `	{ "pfsockopen", "string $hostname, int $port = -1, &$error_code = NULL, &$error_message = NULL, ?float $timeout = NULL", "" },` |
|         - | 1297 | `	{ "stream_socket_client", "string $address, &$error_code = NULL, &$error_message = NULL, ?float $timeout = NULL, int $flags = STREAM_CLIENT_CONNECT, $context = NULL", "" },` |
|         - | 1298 | `	{ "stream_socket_enable_crypto", "$stream, bool $enable, ?int $crypto_method = NULL, $session_stream = NULL", "int\|bool" },` |
|         - | 1299 | `	{ "stream_socket_server", "string $address, &$error_code = NULL, &$error_message = NULL, int $flags = STREAM_SERVER_BIND \| STREAM_SERVER_LISTEN, $context = NULL", "" },` |
|         - | 1300 | `	{ "stream_socket_accept", "$socket, ?float $timeout = NULL, &$peer_name = NULL", "" },` |
|         - | 1301 | `	{ "stream_socket_get_name", "$socket, bool $remote", "string\|false" },` |
|         - | 1302 | `	{ "stream_socket_pair", "int $domain, int $type, int $protocol", "array\|false" },` |
|         - | 1303 | `	{ "stream_isatty", "$stream", "bool" },` |
|         - | 1304 | `	{ "stream_socket_shutdown", "$stream, int $mode", "bool" },` |
|         - | 1305 | `	{ "stream_socket_recvfrom", "$socket, int $length, int $flags = 0, &$address = NULL", "string\|false" },` |
|         - | 1306 | `	{ "stream_socket_sendto", "$socket, string $data, int $flags = 0, string $address = ''", "int\|false" },` |
|         - | 1307 | `	{ "preg_filter", "array\|string $pattern, array\|string $replacement, array\|string $subject, int $limit = -1, &$count = NULL", "array\|string\|null" },` |
|         - | 1308 | `	{ "preg_grep", "string $pattern, array $array, int $flags = 0", "array\|false" },` |
|         - | 1309 | `	{ "preg_match", "string $pattern, string $subject, &$matches = NULL, int $flags = 0, int $offset = 0", "int\|false" },` |
|         - | 1310 | `	{ "preg_match_all", "string $pattern, string $subject, &$matches = NULL, int $flags = 0, int $offset = 0", "int\|false" },` |
|         - | 1311 | `	{ "preg_quote", "string $str, ?string $delimiter = NULL", "string" },` |
|         - | 1312 | `	{ "preg_replace", "array\|string $pattern, array\|string $replacement, array\|string $subject, int $limit = -1, &$count = NULL", "array\|string\|null" },` |
|         - | 1313 | `	{ "preg_replace_callback", "array\|string $pattern, callable $callback, array\|string $subject, int $limit = -1, &$count = NULL, int $flags = 0", "array\|string\|null" },` |
|         - | 1314 | `	{ "preg_replace_callback_array", "array $pattern, array\|string $subject, int $limit = -1, &$count = NULL, int $flags = 0", "array\|string\|null" },` |
|         - | 1315 | `	{ "preg_split", "string $pattern, string $subject, int $limit = -1, int $flags = 0", "array\|false" },` |
|         - | 1316 | `	{ "prev", "object\|array &$array", "mixed" },` |
|         - | 1317 | `	{ "print_r", "mixed $value, bool $return = false", "string\|true" },` |
|         - | 1318 | `	{ "printf", "string $format, mixed ...$values = ?", "int" },` |
|         - | 1319 | `	{ "property_exists", "$object_or_class, string $property", "bool" },` |
|         - | 1320 | `	{ "putenv", "string $assignment", "bool" },` |
|         - | 1321 | `	{ "quoted_printable_decode", "string $string", "string" },` |
|         - | 1322 | `	{ "quoted_printable_encode", "string $string", "string" },` |
|         - | 1323 | `	{ "quotemeta", "string $string", "string" },` |
|         - | 1324 | `	{ "rad2deg", "float $num", "float" },` |
|         - | 1325 | `	{ "rand", "int $min = ?, int $max = ?", "int" },` |
|         - | 1326 | `	{ "random_bytes", "int $length", "string" },` |
|         - | 1327 | `	{ "random_int", "int $min, int $max", "int" },` |
|         - | 1328 | `	{ "range", "string\|int\|float $start, string\|int\|float $end, int\|float $step = 1", "array" },` |
|         - | 1329 | `	{ "rawurldecode", "string $string", "string" },` |
|         - | 1330 | `	{ "rawurlencode", "string $string", "string" },` |
|         - | 1331 | `	{ "readdir", "$dir_handle = NULL", "string\|false" },` |
|         - | 1332 | `	{ "readfile", "string $filename, bool $use_include_path = false, $context = NULL", "int\|false" },` |
|         - | 1333 | `	{ "readlink", "string $path", "string\|false" },` |
|         - | 1334 | `	{ "realpath", "string $path", "string\|false" },` |
|         - | 1335 | `	{ "stream_resolve_include_path", "string $filename", "string\|false" },` |
|         - | 1336 | `	{ "register_shutdown_function", "callable $callback, mixed ...$args = ?", "void" },` |
|         - | 1337 | `	{ "rename", "string $from, string $to, $context = NULL", "bool" },` |
|         - | 1338 | `	{ "reset", "object\|array &$array", "mixed" },` |
|         - | 1339 | `	{ "restore_error_handler", "", "true" },` |
|         - | 1340 | `	{ "restore_exception_handler", "", "true" },` |
|         - | 1341 | `	{ "rewind", "$stream", "bool" },` |
|         - | 1342 | `	{ "rewinddir", "$dir_handle = NULL", "void" },` |
|         - | 1343 | `	{ "rmdir", "string $directory, $context = NULL", "bool" },` |
|         - | 1344 | `	{ "round", "int\|float $num, int $precision = 0, RoundingMode\|int $mode = RoundingMode::HalfAwayFromZero", "float" },` |
|         - | 1345 | `	{ "rsort", "array &$array, int $flags = SORT_REGULAR", "true" },` |
|         - | 1346 | `	{ "rtrim", "string $string, string $characters = \" \\n\\r\\t\\v\\x00\"", "string" },` |
|         - | 1347 | `	{ "serialize", "mixed $value", "string" },` |
|         - | 1348 | `	{ "set_error_handler", "?callable $callback, int $error_levels = E_ALL", "" },` |
|         - | 1349 | `	{ "set_exception_handler", "?callable $callback", "" },` |
|         - | 1350 | `	{ "get_error_handler", "", "?callable" },` |
|         - | 1351 | `	{ "get_exception_handler", "", "?callable" },` |
|         - | 1352 | `	{ "hrtime", "bool $as_number = false", "array\|int\|float\|false" },` |
|         - | 1353 | `	{ "getrusage", "int $mode = 0", "array\|false" },` |
|         - | 1354 | ``	/* php's own row is `int $category, mixed ...$rest` with a MINIMUM of two, so`` |
|         - | 1355 | ``	 * `setlocale(LC_ALL)` is its ArgumentCountError and not a query. */`` |
|         - | 1356 | `	{ "setlocale", "int $category, array\|string $locales, string ...$rest = ?", "string\|false" },` |
|         - | 1357 | `	{ "mb_check_encoding", "array\|string\|null $value = NULL, ?string $encoding = NULL", "bool" },` |
|         - | 1358 | `	{ "mb_convert_case", "string $string, int $mode, ?string $encoding = NULL", "string" },` |
|         - | 1359 | `	{ "mb_detect_encoding", "string $string, array\|string\|null $encodings = NULL, bool $strict = false", "string\|false" },` |
|         - | 1360 | `	{ "mb_encoding_aliases", "string $encoding", "array" },` |
|         - | 1361 | `	{ "mb_internal_encoding", "?string $encoding = NULL", "string\|bool" },` |
|         - | 1362 | `	{ "mb_scrub", "string $string, ?string $encoding = null", "string" },` |
|         - | 1363 | `	{ "mb_substitute_character", "string\|int\|null $substitute_character = null", "string\|int\|bool" },` |
|         - | 1364 | `	{ "mb_str_split", "string $string, int $length = 1, ?string $encoding = NULL", "array" },` |
|         - | 1365 | `	{ "mb_stripos", "string $haystack, string $needle, int $offset = 0, ?string $encoding = NULL", "int\|false" },` |
|         - | 1366 | `	{ "mb_strlen", "string $string, ?string $encoding = NULL", "int" },` |
|         - | 1367 | `	{ "mb_ereg", "string $pattern, string $string, &$matches = NULL", "bool" },` |
|         - | 1368 | `	{ "mb_eregi", "string $pattern, string $string, &$matches = NULL", "bool" },` |
|         - | 1369 | `	{ "mb_ereg_match", "string $pattern, string $string, ?string $options = null", "bool" },` |
|         - | 1370 | `	{ "mb_ereg_replace", "string $pattern, string $replacement, string $string, ?string $options = null", "string\|false\|null" },` |
|         - | 1371 | `	{ "mb_eregi_replace", "string $pattern, string $replacement, string $string, ?string $options = null", "string\|false\|null" },` |
|         - | 1372 | `	{ "mb_ereg_replace_callback", "string $pattern, callable $callback, string $string, ?string $options = null", "string\|false\|null" },` |
|         - | 1373 | `	{ "mb_split", "string $pattern, string $string, int $limit = -1", "array\|false" },` |
|         - | 1374 | `	{ "mb_ereg_search_init", "string $string, ?string $pattern = null, ?string $options = null", "bool" },` |
|         - | 1375 | `	{ "mb_ereg_search", "?string $pattern = null, ?string $options = null", "bool" },` |
|         - | 1376 | `	{ "mb_ereg_search_pos", "?string $pattern = null, ?string $options = null", "array\|false" },` |
|         - | 1377 | `	{ "mb_ereg_search_regs", "?string $pattern = null, ?string $options = null", "array\|false" },` |
|         - | 1378 | `	{ "mb_ereg_search_getregs", "", "array\|false" },` |
|         - | 1379 | `	{ "mb_ereg_search_getpos", "", "int" },` |
|         - | 1380 | `	{ "mb_ereg_search_setpos", "int $offset", "bool" },` |
|         - | 1381 | `	{ "mb_regex_encoding", "?string $encoding = null", "string\|bool" },` |
|         - | 1382 | `	{ "mb_regex_set_options", "?string $options = null", "string" },` |
|         - | 1383 | `	{ "mb_strpos", "string $haystack, string $needle, int $offset = 0, ?string $encoding = NULL", "int\|false" },` |
|         - | 1384 | `	{ "mb_str_pad", "string $string, int $length, string $pad_string = \" \", int $pad_type = STR_PAD_RIGHT, ?string $encoding = null", "string" },` |
|         - | 1385 | `	{ "mb_strcut", "string $string, int $start, ?int $length = null, ?string $encoding = null", "string" },` |
|         - | 1386 | `	{ "mb_strimwidth", "string $string, int $start, int $width, string $trim_marker = \"\", ?string $encoding = null", "string" },` |
|         - | 1387 | `	{ "mb_strrchr", "string $haystack, string $needle, bool $before_needle = false, ?string $encoding = null", "string\|false" },` |
|         - | 1388 | `	{ "mb_strrichr", "string $haystack, string $needle, bool $before_needle = false, ?string $encoding = null", "string\|false" },` |
|         - | 1389 | `	{ "mb_strripos", "string $haystack, string $needle, int $offset = 0, ?string $encoding = null", "int\|false" },` |
|         - | 1390 | `	{ "mb_strrpos", "string $haystack, string $needle, int $offset = 0, ?string $encoding = NULL", "int\|false" },` |
|         - | 1391 | `	{ "mb_stristr", "string $haystack, string $needle, bool $before_needle = false, ?string $encoding = null", "string\|false" },` |
|         - | 1392 | `	{ "mb_strstr", "string $haystack, string $needle, bool $before_needle = false, ?string $encoding = null", "string\|false" },` |
|         - | 1393 | `	{ "mb_substr_count", "string $haystack, string $needle, ?string $encoding = null", "int" },` |
|         - | 1394 | `	{ "mb_strwidth", "string $string, ?string $encoding = NULL", "int" },` |
|         - | 1395 | `	{ "mb_substr", "string $string, int $start, ?int $length = NULL, ?string $encoding = NULL", "string" },` |
|         - | 1396 | `	{ "memory_reset_peak_usage", "", "void" },` |
|         - | 1397 | `	{ "proc_close", "$process", "int" },` |
|         - | 1398 | `	{ "proc_get_status", "$process", "array" },` |
|         - | 1399 | `	{ "proc_nice", "int $priority", "bool" },` |
|         - | 1400 | `	{ "proc_open", "array\|string $command, array $descriptor_spec, &$pipes, ?string $cwd = NULL, ?array $env_vars = NULL, ?array $options = NULL", "" },` |
|         - | 1401 | `	{ "proc_terminate", "$process, int $signal = 15", "bool" },` |
|         - | 1402 | `	{ "set_include_path", "string $include_path", "string\|false" },` |
|         - | 1403 | `	{ "setcookie", "string $name, string $value = '', array\|int $expires_or_options = 0, string $path = '', string $domain = '', bool $secure = false, bool $httponly = false", "bool" },` |
|         - | 1404 | `	{ "setrawcookie", "string $name, string $value = '', array\|int $expires_or_options = 0, string $path = '', string $domain = '', bool $secure = false, bool $httponly = false", "bool" },` |
|         - | 1405 | `	{ "settype", "mixed &$var, string $type", "bool" },` |
|         - | 1406 | `	{ "sha1", "string $string, bool $binary = false", "string" },` |
|         - | 1407 | `	{ "sha1_file", "string $filename, bool $binary = false", "string\|false" },` |
|         - | 1408 | `	{ "shell_exec", "string $command", "string\|false\|null" },` |
|         - | 1409 | `	{ "shuffle", "array &$array", "true" },` |
|         - | 1410 | `	{ "similar_text", "string $string1, string $string2, &$percent = NULL", "int" },` |
|         - | 1411 | `	{ "sin", "float $num", "float" },` |
|         - | 1412 | `	{ "sinh", "float $num", "float" },` |
|         - | 1413 | `	{ "sizeof", "Countable\|array $value, int $mode = COUNT_NORMAL", "int" },` |
|         - | 1414 | `	{ "sleep", "int $seconds", "int" },` |
|         - | 1415 | `	{ "sort", "array &$array, int $flags = SORT_REGULAR", "true" },` |
|         - | 1416 | `	{ "soundex", "string $string", "string" },` |
|         - | 1417 | `	{ "spl_autoload", "string $class, ?string $file_extensions = NULL", "void" },` |
|         - | 1418 | `	{ "spl_autoload_call", "string $class", "void" },` |
|         - | 1419 | `	{ "spl_autoload_extensions", "?string $file_extensions = NULL", "string" },` |
|         - | 1420 | `	{ "spl_autoload_functions", "", "array" },` |
|         - | 1421 | `	{ "spl_autoload_register", "?callable $callback = NULL, bool $throw = true, bool $prepend = false", "bool" },` |
|         - | 1422 | `	{ "spl_autoload_unregister", "callable $callback", "bool" },` |
|         - | 1423 | `	{ "spl_classes", "", "array" },` |
|         - | 1424 | `	{ "spl_object_hash", "object $object", "string" },` |
|         - | 1425 | `	{ "spl_object_id", "object $object", "int" },` |
|         - | 1426 | `	{ "sprintf", "string $format, mixed ...$values = ?", "string" },` |
|         - | 1427 | `	{ "sqrt", "float $num", "float" },` |
|         - | 1428 | `	{ "srand", "?int $seed = NULL, int $mode = MT_RAND_MT19937", "void" },` |
|         - | 1429 | `	{ "stat", "string $filename", "array\|false" },` |
|         - | 1430 | `	{ "str_contains", "string $haystack, string $needle", "bool" },` |
|         - | 1431 | `	{ "str_ends_with", "string $haystack, string $needle", "bool" },` |
|         - | 1432 | `	{ "str_getcsv", "string $string, string $separator = ',', string $enclosure = '\"', string $escape = '\\\\'", "array" },` |
|         - | 1433 | `	{ "str_ireplace", "array\|string $search, array\|string $replace, array\|string $subject, &$count = NULL", "array\|string" },` |
|         - | 1434 | `	{ "str_pad", "string $string, int $length, string $pad_string = ' ', int $pad_type = STR_PAD_RIGHT", "string" },` |
|         - | 1435 | `	{ "str_repeat", "string $string, int $times", "string" },` |
|         - | 1436 | `	{ "str_replace", "array\|string $search, array\|string $replace, array\|string $subject, &$count = NULL", "array\|string" },` |
|         - | 1437 | `	{ "str_rot13", "string $string", "string" },` |
|         - | 1438 | `	{ "str_shuffle", "string $string", "string" },` |
|         - | 1439 | `	{ "str_split", "string $string, int $length = 1", "array" },` |
|         - | 1440 | `	{ "str_starts_with", "string $haystack, string $needle", "bool" },` |
|         - | 1441 | `	{ "str_word_count", "string $string, int $format = 0, ?string $characters = NULL", "array\|int" },` |
|         - | 1442 | `	{ "strcasecmp", "string $string1, string $string2", "int" },` |
|         - | 1443 | `	{ "strchr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|         - | 1444 | `	{ "strcmp", "string $string1, string $string2", "int" },` |
|         - | 1445 | `	{ "strnatcasecmp", "string $string1, string $string2", "int" },` |
|         - | 1446 | `	{ "strnatcmp", "string $string1, string $string2", "int" },` |
|         - | 1447 | `	{ "strcoll", "string $string1, string $string2", "int" },` |
|         - | 1448 | `	{ "strcspn", "string $string, string $characters, int $offset = 0, ?int $length = NULL", "int" },` |
|         - | 1449 | `	{ "stream_context_create", "?array $options = NULL, ?array $params = NULL", "" },` |
|         - | 1450 | `	{ "stream_context_get_options", "$stream_or_context", "array" },` |
|         - | 1451 | ``	/* php's argument #2 is `array\|string $wrapper_or_options` and the array form`` |
|         - | 1452 | `	 * — the two-argument spelling — is DEPRECATED in 8.3; the scope policy refuses what php` |
|         - | 1453 | `	 * deprecates, so this row declares the string and the whole-array form is` |
|         - | 1454 | `	 * spelled stream_context_set_options(). */` |
|         - | 1455 | `	{ "stream_context_set_option", "$context, string $wrapper_name, string $option_name, mixed $value", "true" },` |
|         - | 1456 | `	{ "stream_context_set_options", "$context, array $options", "true" },` |
|         - | 1457 | `	{ "stream_context_get_params", "$context", "array" },` |
|         - | 1458 | `	{ "stream_context_set_params", "$context, array $params", "true" },` |
|         - | 1459 | `	{ "stream_context_get_default", "?array $options = NULL", "" },` |
|         - | 1460 | `	{ "stream_context_set_default", "array $options", "" },` |
|         - | 1461 | `	{ "stream_get_contents", "$stream, ?int $length = NULL, int $offset = -1", "string\|false" },` |
|         - | 1462 | `	{ "stream_get_line", "$stream, int $length, string $ending = ''", "string\|false" },` |
|         - | 1463 | `	{ "socket_get_status", "$stream", "array" },` |
|         - | 1464 | `	{ "stream_get_meta_data", "$stream", "array" },` |
|         - | 1465 | `	{ "stream_copy_to_stream", "$from, $to, ?int $length = NULL, int $offset = 0", "int\|false" },` |
|         - | 1466 | `	{ "stream_get_transports", "", "array" },` |
|         - | 1467 | `	{ "stream_is_local", "$stream", "bool" },` |
|         - | 1468 | `	{ "stream_select", "?array &$read, ?array &$write, ?array &$except, ?int $seconds, ?int $microseconds = NULL", "int\|false" },` |
|         - | 1469 | `	{ "stream_set_blocking", "$stream, bool $enable", "bool" },` |
|         - | 1470 | `	{ "socket_set_blocking", "$stream, bool $enable", "bool" },` |
|         - | 1471 | `	{ "stream_set_chunk_size", "$stream, int $size", "int" },` |
|         - | 1472 | `	{ "stream_set_read_buffer", "$stream, int $size", "int" },` |
|         - | 1473 | `	{ "stream_set_timeout", "$stream, int $seconds, int $microseconds = 0", "bool" },` |
|         - | 1474 | `	{ "stream_set_write_buffer", "$stream, int $size", "int" },` |
|         - | 1475 | `	{ "set_file_buffer", "$stream, int $size", "int" },` |
|         - | 1476 | `	{ "stream_supports_lock", "$stream", "bool" },` |
|         - | 1477 | `	{ "stream_get_wrappers", "", "array" },` |
|         - | 1478 | `	{ "stream_get_filters", "", "array" },` |
|         - | 1479 | `	{ "stream_filter_append", "$stream, string $filter_name, int $mode = 0, mixed $params = ?", "" },` |
|         - | 1480 | `	{ "stream_filter_prepend", "$stream, string $filter_name, int $mode = 0, mixed $params = ?", "" },` |
|         - | 1481 | `	{ "stream_filter_remove", "$stream_filter", "bool" },` |
|         - | 1482 | `	{ "stream_filter_register", "string $filter_name, string $class", "bool" },` |
|         - | 1483 | `	{ "stream_bucket_make_writeable", "$brigade", "?StreamBucket" },` |
|         - | 1484 | `	{ "stream_bucket_append", "$brigade, StreamBucket $bucket", "void" },` |
|         - | 1485 | `	{ "stream_bucket_prepend", "$brigade, StreamBucket $bucket", "void" },` |
|         - | 1486 | `	{ "stream_bucket_new", "$stream, string $buffer", "StreamBucket" },` |
|         - | 1487 | `	{ "stream_register_wrapper", "string $protocol, string $class, int $flags = 0", "bool" },` |
|         - | 1488 | `	{ "stream_wrapper_register", "string $protocol, string $class, int $flags = 0", "bool" },` |
|         - | 1489 | `	{ "stream_wrapper_unregister", "string $protocol", "bool" },` |
|         - | 1490 | `	{ "stream_wrapper_restore", "string $protocol", "bool" },` |
|         - | 1491 | `	{ "strip_tags", "string $string, array\|string\|null $allowed_tags = NULL", "string" },` |
|         - | 1492 | `	{ "stripcslashes", "string $string", "string" },` |
|         - | 1493 | `	{ "stripos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|         - | 1494 | `	{ "stripslashes", "string $string", "string" },` |
|         - | 1495 | `	{ "stristr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|         - | 1496 | `	{ "strlen", "string $string", "int" },` |
|         - | 1497 | `	{ "strncasecmp", "string $string1, string $string2, int $length", "int" },` |
|         - | 1498 | `	{ "strncmp", "string $string1, string $string2, int $length", "int" },` |
|         - | 1499 | `	{ "strpbrk", "string $string, string $characters", "string\|false" },` |
|         - | 1500 | `	{ "strpos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|         - | 1501 | `	{ "strrchr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|         - | 1502 | `	{ "strrev", "string $string", "string" },` |
|         - | 1503 | `	{ "strripos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|         - | 1504 | `	{ "strrpos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|         - | 1505 | `	{ "strspn", "string $string, string $characters, int $offset = 0, ?int $length = NULL", "int" },` |
|         - | 1506 | `	{ "strstr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|         - | 1507 | `	{ "strtok", "string $string, ?string $token = NULL", "string\|false" },` |
|         - | 1508 | `	{ "strtolower", "string $string", "string" },` |
|         - | 1509 | `	{ "strtotime", "string $datetime, ?int $baseTimestamp = NULL", "int\|false" },` |
|         - | 1510 | `	{ "strtoupper", "string $string", "string" },` |
|         - | 1511 | `	{ "strtr", "string $string, array\|string $from, ?string $to = NULL", "string" },` |
|         - | 1512 | `	{ "strval", "mixed $value", "string" },` |
|         - | 1513 | `	{ "substr", "string $string, int $offset, ?int $length = NULL", "string" },` |
|         - | 1514 | `	{ "substr_compare", "string $haystack, string $needle, int $offset, ?int $length = NULL, bool $case_insensitive = false", "int" },` |
|         - | 1515 | `	{ "substr_count", "string $haystack, string $needle, int $offset = 0, ?int $length = NULL", "int" },` |
|         - | 1516 | `	{ "substr_replace", "array\|string $string, array\|string $replace, array\|int $offset, array\|int\|null $length = NULL", "array\|string" },` |
|         - | 1517 | `	{ "symlink", "string $target, string $link", "bool" },` |
|         - | 1518 | `	{ "sys_get_temp_dir", "", "string" },` |
|         - | 1519 | `	{ "sys_getloadavg", "", "array\|false" },` |
|         - | 1520 | `	{ "system", "string $command, &$result_code = NULL", "string\|false" },` |
|         - | 1521 | `	{ "tan", "float $num", "float" },` |
|         - | 1522 | `	{ "tanh", "float $num", "float" },` |
|         - | 1523 | `	{ "time", "", "int" },` |
|         - | 1524 | `	{ "token_get_all", "string $code, int $flags = 0", "array" },` |
|         - | 1525 | `	{ "php_strip_whitespace", "string $filename", "string" },` |
|         - | 1526 | `	{ "token_name", "int $id", "string" },` |
|         - | 1527 | `	{ "touch", "string $filename, ?int $mtime = NULL, ?int $atime = NULL", "bool" },` |
|         - | 1528 | `	{ "trigger_error", "string $message, int $error_level = E_USER_NOTICE", "true" },` |
|         - | 1529 | `	{ "trim", "string $string, string $characters = \" \\n\\r\\t\\v\\x00\"", "string" },` |
|         - | 1530 | `	{ "uasort", "array &$array, callable $callback", "true" },` |
|         - | 1531 | `	{ "ucfirst", "string $string", "string" },` |
|         - | 1532 | `	{ "ucwords", "string $string, string $separators = \" \\t\\r\\n\\f\\v\"", "string" },` |
|         - | 1533 | `	{ "uksort", "array &$array, callable $callback", "true" },` |
|         - | 1534 | `	{ "umask", "?int $mask = NULL", "int" },` |
|         - | 1535 | `	{ "uniqid", "string $prefix = '', bool $more_entropy = false", "string" },` |
|         - | 1536 | `	{ "unixtojd", "?int $timestamp = NULL", "int\|false" },` |
|         - | 1537 | `	{ "unlink", "string $filename, $context = NULL", "bool" },` |
|         - | 1538 | `	{ "unserialize", "string $data, array $options = []", "mixed" },` |
|         - | 1539 | `	{ "urldecode", "string $string", "string" },` |
|         - | 1540 | `	{ "urlencode", "string $string", "string" },` |
|         - | 1541 | `	{ "user_error", "string $message, int $error_level = E_USER_NOTICE", "true" },` |
|         - | 1542 | `	{ "usleep", "int $microseconds", "void" },` |
|         - | 1543 | `	{ "usort", "array &$array, callable $callback", "true" },` |
|         - | 1544 | `	{ "var_dump", "mixed $value, mixed ...$values = ?", "void" },` |
|         - | 1545 | `	{ "var_export", "mixed $value, bool $return = false", "?string" },` |
|         - | 1546 | `	{ "version_compare", "string $version1, string $version2, ?string $operator = null", "int\|bool" },` |
|         - | 1547 | `	/* Both typed parameters stand aside for the same reason as fprintf's: php` |
|         - | 1548 | `	 * refuses $stream before either of them. */` |
|         - | 1549 | `	{ "vfprintf", "$stream, ~string $format, ~array $values", "int" },` |
|         - | 1550 | `	{ "vprintf", "string $format, array $values", "int" },` |
|         - | 1551 | `	{ "vsprintf", "string $format, array $values", "string" },` |
|         - | 1552 | `	{ "wordwrap", "string $string, int $width = 75, string $break = \"\\n\", bool $cut_long_words = false", "string" },` |
|         - | 1553 | `	{ "zip_close", "$zip", "void" },` |
|         - | 1554 | `	{ "zip_entry_close", "$zip_entry", "bool" },` |
|         - | 1555 | `	{ "zip_entry_compressedsize", "$zip_entry", "int\|false" },` |
|         - | 1556 | `	{ "zip_entry_compressionmethod", "$zip_entry", "string\|false" },` |
|         - | 1557 | `	{ "zip_entry_filesize", "$zip_entry", "int\|false" },` |
|         - | 1558 | `	{ "zip_entry_name", "$zip_entry", "string\|false" },` |
|         - | 1559 | `	{ "zip_entry_open", "$zip_dp, $zip_entry, string $mode = 'rb'", "bool" },` |
|         - | 1560 | `	{ "zip_entry_read", "$zip_entry, int $len = 1024", "string\|false" },` |
|         - | 1561 | `	{ "zip_open", "string $filename", "" },` |
|         - | 1562 | `	{ "zip_read", "$zip", "" },` |
|         - | 1563 | `};` |
|         - | 1564 | `/*` |
|         - | 1565 | ` * Stamp the signature strings onto the registered host functions.` |
|         - | 1566 | ` * Runs once at PH7_VmMakeReady, after the builtins are registered.` |
|         - | 1567 | ` */` |
|         - | 1568 | `/*` |
|         - | 1569 | ` * Derive the minimum arity from a builtin's declared signature: a parameter is` |
|         - | 1570 | ` * optional when it carries a default ("int $length = 76") or is variadic` |
|         - | 1571 | ` * ("...$rest"), so the minimum is the count of the parameters that are neither,` |
|         - | 1572 | ` * and the ZPP wording is "at least" as soon as one optional/variadic exists.` |
|         - | 1573 | ` *` |
|         - | 1574 | ` * This makes aBuiltinSig[] the single source of truth for arity — the curated` |
|         - | 1575 | ` * aBuiltinArity[] table above stays as an OVERRIDE for the handful of builtins` |
|         - | 1576 | ` * php reports differently from their own signature (e.g. strtr(), whose 3-arg` |
|         - | 1577 | ` * form still reports "expects exactly 2", and the array_udiff() family, whose` |
|         - | 1578 | ` * variadic tail hides a second required argument). Verified against php 8.5.7` |
|         - | 1579 | ` * for all 462 signed builtins: 458 derive exactly, 4 are overridden.` |
|         - | 1580 | ` */` |
|         - | 1581 | `/*` |
|         - | 1582 | ` * A DEFAULT can contain the parameter separator: php declares` |
|         - | 1583 | `` * `string $separator = ','` and `string $enclosure = '"'`. Every scan of a`` |
|         - | 1584 | ` * signature therefore has to step over a quoted run, or the comma inside one` |
|         - | 1585 | ` * splits the parameter in two — which is how fgetcsv()/fputcsv()/str_getcsv()` |
|         - | 1586 | ` * came to count SIX parameters and accept a fifth argument php refuses.` |
|         - | 1587 | ` * Answers the position of the closing quote (or of the NUL when the run is` |
|         - | 1588 | ` * unterminated); the caller advances past it.` |
|         - | 1589 | ` */` |
|   1587098 | 1590 | `static const char *VmSigSkipQuoted(const char *zCur)` |
|         5 | 1591 | `{` |
|   1587103 | 1592 | `	char c = zCur[0];` |
|   1587103 | 1593 | `	if( c != '\'' && c != '"' ){` |
|       ! 0 | 1594 | `		return zCur;` |
|         - | 1595 | `	}` |
|   4422563 | 1596 | `	for( zCur++ ; zCur[0] ; zCur++ ){` |
|   4422563 | 1597 | `		if( zCur[0] == '\\' && zCur[1] ){` |
|    537589 | 1598 | `			zCur++;` |
|    537589 | 1599 | `			continue;` |
|         - | 1600 | `		}` |
|   3884979 | 1601 | `		if( zCur[0] == c ){` |
|   1587103 | 1602 | `			break;` |
|         - | 1603 | `		}` |
|   1147345 | 1604 | `	}` |
|   1587103 | 1605 | `	return zCur;` |
|    792415 | 1606 | `}` |
|  20386874 | 1607 | `PH7_PRIVATE void VmDeriveArityFromSig(const char *zSig,sxi16 *pnMin,sxu8 *pbAtLeast,sxi16 *pnMax,sxu8 *pbHasMax)` |
|         5 | 1608 | `{` |
|  20386879 | 1609 | `	const char *zCur = zSig;` |
|  20386879 | 1610 | `	int nMin = 0, bAtLeast = 0, bSeen = 0, bOptional = 0;` |
|  20386879 | 1611 | `	int nTotal = 0, bVariadic = 0;` |
| 235468266 | 1612 | `	for(;;){` |
| 485126342 | 1613 | `		if( zCur[0] == '\'' \|\| zCur[0] == '"' ){` |
|    791000 | 1614 | `			bSeen = 1;` |
|    791000 | 1615 | `			zCur = VmSigSkipQuoted(zCur);` |
|    791000 | 1616 | `			if( zCur[0] != '\0' ){` |
|    791000 | 1617 | `				zCur++;` |
|    394931 | 1618 | `			}` |
|    791000 | 1619 | `			continue;` |
|         - | 1620 | `		}` |
| 484335347 | 1621 | `		if( zCur[0] == '\0' \|\| zCur[0] == ',' ){` |
|  32496874 | 1622 | `			if( bSeen ){` |
|  25453480 | 1623 | `				nTotal++;` |
|  25453480 | 1624 | `				if( bOptional ){` |
|   9103111 | 1625 | `					bAtLeast = 1;` |
|   4532693 | 1626 | `				}else{` |
|  16350374 | 1627 | `					nMin++;` |
|         - | 1628 | `				}` |
|  12690685 | 1629 | `			}` |
|  32496874 | 1630 | `			if( zCur[0] == '\0' ){` |
|  20386879 | 1631 | `				break;` |
|         - | 1632 | `			}` |
|  12110000 | 1633 | `			bSeen = bOptional = 0;` |
|  12110000 | 1634 | `			zCur++;` |
|  12110000 | 1635 | `			continue;` |
|         - | 1636 | `		}` |
| 451838478 | 1637 | `		if( zCur[0] != ' ' ){` |
| 398706277 | 1638 | `			bSeen = 1;` |
| 198819592 | 1639 | `		}` |
| 451838478 | 1640 | `		if( zCur[0] == '=' \|\| (zCur[0] == '.' && zCur[1] == '.' && zCur[2] == '.') ){` |
|   9368541 | 1641 | `			bOptional = 1;` |
|   4665194 | 1642 | `		}` |
| 451838478 | 1643 | `		if( zCur[0] == '.' && zCur[1] == '.' && zCur[2] == '.' ){` |
|    637015 | 1644 | `			bVariadic = 1;` |
|    318054 | 1645 | `		}` |
| 451838478 | 1646 | `		zCur++;` |
|         5 | 1647 | `	}` |
|  20386879 | 1648 | `	*pnMin = (sxi16)nMin;` |
|  20386879 | 1649 | `	*pbAtLeast = (sxu8)bAtLeast;` |
|         - | 1650 | `	/* php enforces a MAXIMUM too ("expects at most 1 argument, 2 given"); a` |
|         - | 1651 | `	 * variadic tail means there is none. The parameter COUNT is the maximum,` |
|         - | 1652 | `	 * whether or not the parameters carry defaults. */` |
|  20386879 | 1653 | `	*pnMax = (sxi16)nTotal;` |
|  20386879 | 1654 | `	*pbHasMax = (sxu8)(bVariadic ? 0 : 1);` |
|  20386879 | 1655 | `}` |
|         - | 1656 | `/*` |
|         - | 1657 | ` * Does the declared type list (e.g. "array\|string", "?int", "callable") contain` |
|         - | 1658 | ` * the given token? Compares against each '\|'-separated alternative, ignoring a` |
|         - | 1659 | ` * leading nullable '?'.` |
|         - | 1660 | ` */` |
|   1189730 | 1661 | `static int VmSigTypeHas(const char *zType,int nType,const char *zTok)` |
|         5 | 1662 | `{` |
|   1189735 | 1663 | `	int nTok = (int)SyStrlen(zTok);` |
|   1189735 | 1664 | `	int i = 0;` |
|   1189735 | 1665 | `	if( zType[0] == '?' ){` |
|    144597 | 1666 | `		zType++;` |
|    144597 | 1667 | `		nType--;` |
|     71685 | 1668 | `	}` |
|   2250683 | 1669 | `	while( i < nType ){` |
|   1140737 | 1670 | `		int j = i;` |
|   7223841 | 1671 | `		while( j < nType && zType[j] != '\|' ){` |
|   6083109 | 1672 | `			j++;` |
|         5 | 1673 | `		}` |
|   1140737 | 1674 | `		if( j - i == nTok && SyMemcmp(&zType[i],zTok,(sxu32)nTok) == 0 ){` |
|     79789 | 1675 | `			return 1;` |
|         - | 1676 | `		}` |
|   1060953 | 1677 | `		i = j + 1;` |
|         5 | 1678 | `	}` |
|   1109951 | 1679 | `	return 0;` |
|    588537 | 1680 | `}` |
|         - | 1681 | `/*` |
|         - | 1682 | `` * Is EVERY arm of the declared type list `array` (a bare `array`, or `?array`,`` |
|         - | 1683 | `` * or the `array\|null` union that spells the same thing)? Such a parameter has`` |
|         - | 1684 | ` * no arm a scalar can satisfy, and php refuses one outright.` |
|         - | 1685 | ` *` |
|         - | 1686 | `` * The screen used to exempt any type list carrying an `array` arm, union or`` |
|         - | 1687 | `` * not, for a wording reason: php's `array\|object` parameters come from ONE ZPP`` |
|         - | 1688 | ` * macro (Z_PARAM_ARRAY_OR_OBJECT) that names only "array" in the refusal, so` |
|         - | 1689 | ` * the declared type is not the text php prints. That ambiguity does not exist` |
|         - | 1690 | `` * for a parameter typed exactly `array` -- there is one arm and php prints it.`` |
|         - | 1691 | ` */` |
|     86068 | 1692 | `static int VmSigTypeIsArrayOnly(const char *zType,int nType)` |
|         5 | 1693 | `{` |
|     86073 | 1694 | `	int i = 0, bArray = 0;` |
|     86073 | 1695 | `	if( zType[0] == '?' ){` |
|     11119 | 1696 | `		zType++;` |
|     11119 | 1697 | `		nType--;` |
|      5510 | 1698 | `	}` |
|     93577 | 1699 | `	while( i < nType ){` |
|     79625 | 1700 | `		int j = i;` |
|    502727 | 1701 | `		while( j < nType && zType[j] != '\|' ){` |
|    423107 | 1702 | `			j++;` |
|         5 | 1703 | `		}` |
|     79625 | 1704 | `		if( j > i ){` |
|     79620 | 1705 | `			if( j - i == (int)sizeof("array")-1` |
|     46551 | 1706 | `			 && SyMemcmp(&zType[i],"array",sizeof("array")-1) == 0 ){` |
|      7509 | 1707 | `				bArray = 1;` |
|     78353 | 1708 | `			}else if( !(j - i == (int)sizeof("null")-1` |
|     38212 | 1709 | `			         && SyMemcmp(&zType[i],"null",sizeof("null")-1) == 0) ){` |
|     72121 | 1710 | `				return 0;` |
|         - | 1711 | `			}` |
|      3696 | 1712 | `		}` |
|      7509 | 1713 | `		i = j + 1;` |
|         5 | 1714 | `	}` |
|     13957 | 1715 | `	return bArray;` |
|     42579 | 1716 | `}` |
|         - | 1717 | `/*` |
|         - | 1718 | ` * Does the declared type list name a CLASS (anything that is not one of php's` |
|         - | 1719 | ` * builtin type keywords)? A class-typed parameter accepts an object, so it must` |
|         - | 1720 | ` * not be rejected by the array/object/resource screen below.` |
|         - | 1721 | ` */` |
|         - | 1722 | `/* Is this one arm of a declared type a BUILTIN type name rather than a class? */` |
|    125735 | 1723 | `static int VmSigArmIsBuiltinType(const char *zArm,int nArm)` |
|         5 | 1724 | `{` |
|         - | 1725 | `	static const char *azBuiltin[] = {` |
|         - | 1726 | `		"int","float","string","bool","array","object","callable","iterable",` |
|         - | 1727 | `		"mixed","null","void","resource","false","true","never","self","static"` |
|         - | 1728 | `	};` |
|         - | 1729 | `	int k;` |
|   1162255 | 1730 | `	for( k = 0 ; k < (int)SX_ARRAYSIZE(azBuiltin) ; ++k ){` |
|   1112314 | 1731 | `		int nB = (int)SyStrlen(azBuiltin[k]);` |
|   1112314 | 1732 | `		if( nArm == nB && SyMemcmp(zArm,azBuiltin[k],(sxu32)nB) == 0 ){` |
|     75799 | 1733 | `			return 1;` |
|         - | 1734 | `		}` |
|    517081 | 1735 | `	}` |
|     49946 | 1736 | `	return 0;` |
|     62429 | 1737 | `}` |
|     86072 | 1738 | `static int VmSigTypeHasClass(const char *zType,int nType)` |
|         5 | 1739 | `{` |
|     86077 | 1740 | `	int i = 0;` |
|     86077 | 1741 | `	if( zType[0] == '?' ){` |
|     11119 | 1742 | `		zType++;` |
|     11119 | 1743 | `		nType--;` |
|      5510 | 1744 | `	}` |
|    161861 | 1745 | `	while( i < nType ){` |
|     81759 | 1746 | `		int j = i;` |
|    514587 | 1747 | `		while( j < nType && zType[j] != '\|' ){` |
|    432833 | 1748 | `			j++;` |
|         5 | 1749 | `		}` |
|     81759 | 1750 | `		if( j > i && !VmSigArmIsBuiltinType(&zType[i],j - i) ){` |
|      5975 | 1751 | `			return 1;` |
|         - | 1752 | `		}` |
|     75789 | 1753 | `		i = j + 1;` |
|         5 | 1754 | `	}` |
|     80107 | 1755 | `	return 0;` |
|     42581 | 1756 | `}` |
|         - | 1757 | `/*` |
|         - | 1758 | ` * php's ", X given" tail: like ph7_type_name() but an object reports its CLASS,` |
|         - | 1759 | ` * which is what php prints in a TypeError.` |
|         - | 1760 | ` */` |
|         - | 1761 | `/*` |
|         - | 1762 | ` * Does pObj satisfy any CLASS arm of a declared type?` |
|         - | 1763 | ` *` |
|         - | 1764 | ` * Answers TRUE (unscreened) when an arm names something this VM has not declared:` |
|         - | 1765 | ` * the signatures describe php's surface, parts of which PHL models differently` |
|         - | 1766 | ` * (the resource-backed handles the RES branch below already excuses), and a name` |
|         - | 1767 | ` * that resolves to nothing must not turn into a rejection of a valid argument.` |
|         - | 1768 | ` */` |
|     43969 | 1769 | `static int VmSigObjSatisfiesClass(ph7_vm *pVm,const char *zType,int nType,` |
|         - | 1770 | `	ph7_class_instance *pObj)` |
|         5 | 1771 | `{` |
|     43974 | 1772 | `	int i = 0;` |
|     43974 | 1773 | `	if( pObj == 0 \|\| pObj->pClass == 0 ){` |
|       ! 0 | 1774 | `		return 1;` |
|         - | 1775 | `	}` |
|     43974 | 1776 | `	if( zType[0] == '?' ){` |
|      3521 | 1777 | `		zType++;` |
|      3521 | 1778 | `		nType--;` |
|      1758 | 1779 | `	}` |
|     44060 | 1780 | `	while( i < nType ){` |
|     43986 | 1781 | `		int j = i;` |
|    519469 | 1782 | `		while( j < nType && zType[j] != '\|' ){` |
|    475488 | 1783 | `			j++;` |
|         5 | 1784 | `		}` |
|     43986 | 1785 | `		if( j > i && !VmSigArmIsBuiltinType(&zType[i],j - i) ){` |
|     43976 | 1786 | `			ph7_class *pClass = PH7_VmExtractClass(&(*pVm),&zType[i],(sxu32)(j - i),FALSE,0);` |
|     43976 | 1787 | `			if( pClass == 0 ){` |
|         - | 1788 | `				/* Either a builtin type name (already excluded by the caller) or a` |
|         - | 1789 | `				 * class this build does not declare: nothing to judge. */` |
|       ! 0 | 1790 | `				return 1;` |
|         - | 1791 | `			}` |
|     43976 | 1792 | `			if( PH7_VmInstanceOf(pObj->pClass,pClass) ){` |
|     43900 | 1793 | `				return 1;` |
|         - | 1794 | `			}` |
|        38 | 1795 | `		}` |
|        90 | 1796 | `		i = j + 1;` |
|         4 | 1797 | `	}` |
|        78 | 1798 | `	return 0;` |
|     21993 | 1799 | `}` |
|       182 | 1800 | `static const char * VmArgTypeName(ph7_value *pVal)` |
|         5 | 1801 | `{` |
|       187 | 1802 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       187 | 1803 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       187 | 1804 | `		if( pInst && pInst->pClass ){` |
|       187 | 1805 | `			return pInst->pClass->sName.zString;` |
|         - | 1806 | `		}` |
|       ! 0 | 1807 | `	}` |
|       ! 0 | 1808 | `	return ph7_type_name(pVal);` |
|        96 | 1809 | `}` |
|         - | 1810 | `/*` |
|         - | 1811 | `` * Does pArg satisfy a `string` parameter? The rule VmEnforceBuiltinArgTypes()`` |
|         - | 1812 | ` * below applies, factored out so a builtin that words its own overload dispatch` |
|         - | 1813 | ` * (strtr(), whose expected type depends on the ARITY and so cannot be spelled in` |
|         - | 1814 | ` * one signature) decides identically instead of forking the logic. An array never` |
|         - | 1815 | ` * satisfies one; an object does only through __toString(); a resource does not;` |
|         - | 1816 | ` * null does under php, with a deprecation, but not under PHL's the null-strictness policy` |
|         - | 1817 | ` * policy — the screen and this helper both report it as a mismatch.` |
|         - | 1818 | ` */` |
|     57425 | 1819 | `PH7_PRIVATE int PH7_ArgSatisfiesString(ph7_value *pArg)` |
|         5 | 1820 | `{` |
|     57430 | 1821 | `	if( (pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_NULL\|MEMOBJ_RES)) != 0 ){` |
|        39 | 1822 | `		return 0;` |
|         - | 1823 | `	}` |
|     57396 | 1824 | `	if( (pArg->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       227 | 1825 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|       227 | 1826 | `		return pInst && PH7_ClassExtractMethod(pInst->pClass,"__toString",` |
|       111 | 1827 | `			sizeof("__toString")-1) != 0;` |
|         - | 1828 | `	}` |
|     57174 | 1829 | `	return 1;` |
|     28701 | 1830 | `}` |
|         - | 1831 | `/*` |
|         - | 1832 | `` * Is pArg an OBJECT that a `string` parameter can never take -- one with no`` |
|         - | 1833 | ` * __toString()? The question a builtin screening its own arguments actually has:` |
|         - | 1834 | ` * an object is the one kind whose acceptance depends on the class rather than on` |
|         - | 1835 | `` * the type tag, so a guard written as `ph7_value_is_object()` refuses the`` |
|         - | 1836 | ` * Stringable php converts. str_split(), addslashes(), addcslashes(), bindec(),` |
|         - | 1837 | ` * hexdec(), octdec() and the whole printf family all made that mistake, each` |
|         - | 1838 | ` * turning three one-character strings into a TypeError.` |
|         - | 1839 | ` *` |
|         - | 1840 | ` * Says nothing about arrays, resources or null: those are the caller's, because` |
|         - | 1841 | `` * a `string` parameter's null is refused here and coerced (with a deprecation)`` |
|         - | 1842 | ` * by php, and each builtin words that arm for itself.` |
|         - | 1843 | ` */` |
|     64698 | 1844 | `PH7_PRIVATE int PH7_ArgIsUnstringableObject(ph7_value *pArg)` |
|         5 | 1845 | `{` |
|     64703 | 1846 | `	return (pArg->iFlags & MEMOBJ_OBJ) != 0 && !PH7_ArgSatisfiesString(pArg);` |
|         5 | 1847 | `}` |
|         - | 1848 | `/*` |
|         - | 1849 | `` * Is the declared type exactly `int` — the only shape whose float argument the`` |
|         - | 1850 | `` * screen below can decide? A union with a `float`, `string` or `bool` arm has its`` |
|         - | 1851 | ` * own coercion rules per arm (and php words those refusals from the builtin), so` |
|         - | 1852 | ` * only the plain form and its nullable spelling qualify.` |
|         - | 1853 | ` */` |
|     86068 | 1854 | `static int VmSigTypeIsIntOnly(const char *zType,int nType)` |
|         5 | 1855 | `{` |
|     86073 | 1856 | `	if( nType > 0 && zType[0] == '?' ){` |
|     11119 | 1857 | `		zType++;` |
|     11119 | 1858 | `		nType--;` |
|      5510 | 1859 | `	}` |
|     86073 | 1860 | `	if( nType == (int)sizeof("int")-1 && SyMemcmp(zType,"int",3) == 0 ){` |
|     18331 | 1861 | `		return 1;` |
|         - | 1862 | `	}` |
|         - | 1863 | ``	/* `int\|null` / `null\|int`, the union spelling of `?int`. */`` |
|     68233 | 1864 | `	return VmSigTypeHas(zType,nType,"int") && VmSigTypeHas(zType,nType,"null")` |
|       486 | 1865 | `	    && !VmSigTypeHas(zType,nType,"float")` |
|       166 | 1866 | `	    && !VmSigTypeHas(zType,nType,"string")` |
|        50 | 1867 | `	    && !VmSigTypeHas(zType,nType,"bool")` |
|         4 | 1868 | `	    && !VmSigTypeHas(zType,nType,"array")` |
|         2 | 1869 | `	    && !VmSigTypeHas(zType,nType,"object")` |
|       ! 0 | 1870 | `	    && !VmSigTypeHas(zType,nType,"iterable")` |
|       ! 0 | 1871 | `	    && !VmSigTypeHas(zType,nType,"callable")` |
|     68114 | 1872 | `	    && !VmSigTypeHasClass(zType,nType);` |
|     42579 | 1873 | `}` |
|         - | 1874 | `/*` |
|         - | 1875 | `` * Can this float reach an `int` parameter without losing anything? php's rule is`` |
|         - | 1876 | ` * php_parse_arg_long's: in range, and integral. NaN and the infinities are out by` |
|         - | 1877 | ` * the range test (a NaN compares false against both bounds, which is why the test` |
|         - | 1878 | ` * is written as a pair of accepts rather than a pair of rejects).` |
|         - | 1879 | ` */` |
|       106 | 1880 | `static int VmDoubleFitsInt(double d)` |
|         4 | 1881 | `{` |
|       110 | 1882 | `	if( !PH7_RealFitsInt64(d) ){` |
|        51 | 1883 | `		return 0;` |
|         - | 1884 | `	}` |
|        62 | 1885 | `	return d == (double)(sxi64)d;` |
|        57 | 1886 | `}` |
|         - | 1887 | `/*` |
|         - | 1888 | ` * The same question for a NUMERIC string, which php asks with the same answer:` |
|         - | 1889 | `` * `dechex("1e19")` and `dechex("99999999999999999999")` are both`` |
|         - | 1890 | `` * `must be of type int, string given`. RangeStrToNumber is php's`` |
|         - | 1891 | ` * is_numeric_string grammar and already reclassifies an integer too wide for an` |
|         - | 1892 | ` * sxi64 as a DOUBLE, so the two shapes converge on one test.` |
|         - | 1893 | ` */` |
|        80 | 1894 | `static int VmNumStrFitsInt(ph7_value *pArg)` |
|         4 | 1895 | `{` |
|         - | 1896 | `	const char *zStr;` |
|        84 | 1897 | `	int nLen = 0;` |
|        84 | 1898 | `	sxi64 iVal = 0;` |
|        84 | 1899 | `	double dVal = 0;` |
|        84 | 1900 | `	zStr = ph7_value_to_string(pArg,&nLen);` |
|        84 | 1901 | `	switch( RangeStrToNumber(zStr,(sxu32)nLen,&iVal,&dVal) ){` |
|        60 | 1902 | `	case RANGE_IN_LONG:   return 1;` |
|        27 | 1903 | `	case RANGE_IN_DOUBLE: return VmDoubleFitsInt(dVal);` |
|       ! 0 | 1904 | `	default:              return 0;` |
|         - | 1905 | `	}` |
|        44 | 1906 | `}` |
|         - | 1907 | `/*` |
|         - | 1908 | ` * PHP-8 PATH parameters: which positions carry a filesystem path, a shell` |
|         - | 1909 | ` * command or an include-path list rather than an ordinary string.` |
|         - | 1910 | ` *` |
|         - | 1911 | ` * php spells this in the ZPP macro, not in the declared type: a path parameter` |
|         - | 1912 | `` * is `Z_PARAM_PATH` where an ordinary one is `Z_PARAM_STR`, and both print as`` |
|         - | 1913 | `` * `string` in the stub Reflection reads. The difference is a single rule — a`` |
|         - | 1914 | ` * path may not contain a NUL byte — and php raises a catchable ValueError for` |
|         - | 1915 | ` * one that does, BEFORE the call reaches the filesystem.` |
|         - | 1916 | ` *` |
|         - | 1917 | ` * PHL had no such notion, so every one of these arguments went to the C API as` |
|         - | 1918 | ` * a NUL-terminated string and was silently TRUNCATED at the NUL. That is not a` |
|         - | 1919 | ` * missing diagnostic: the truncated path is a DIFFERENT path, and the builtin` |
|         - | 1920 | `` * then operated on it. `unlink("$dir/x\0.png")` deleted `$dir/x`,`` |
|         - | 1921 | `` * `file_put_contents("$dir/x\0.txt",$d)` wrote it, `touch`/`chmod`/`copy`/`` |
|         - | 1922 | ``  * `rename`/`symlink`/`mkdir` all acted on the prefix, `glob` and `realpath` `` |
|         - | 1923 | `` * answered for it, and `shell_exec("cmd\0; rm -rf /")` ran the prefix as a`` |
|         - | 1924 | ` * command. It is the classic poison-NUL-byte shape php closed engine-wide: a` |
|         - | 1925 | ` * script that concatenates request input into a filename gets a truncation` |
|         - | 1926 | ` * where php gets a refusal, and the extension check the suffix was there to` |
|         - | 1927 | ` * perform never runs.` |
|         - | 1928 | ` *` |
|         - | 1929 | ` * The mask is positional (bit N => parameter N is a path), which is how php` |
|         - | 1930 | ` * carries it too. Only functions PHL actually registers are listed; each row's` |
|         - | 1931 | ` * positions were verified against php 8.5 argument by argument (the answer is` |
|         - | 1932 | `` * NOT derivable from the parameter name — preg_match's `$pattern` is an`` |
|         - | 1933 | ``  * ordinary string, glob's is a path — nor from the type, which is `string` `` |
|         - | 1934 | ` * for both).` |
|         - | 1935 | ` *` |
|         - | 1936 | ` * What is deliberately NOT here: the stat family (file_exists, is_dir, stat,` |
|         - | 1937 | ` * filesize, fileperms, …), which php parses with Z_PARAM_STR and answers` |
|         - | 1938 | `` * `false` for in silence, and the pure PATH-STRING functions (basename,`` |
|         - | 1939 | ` * dirname, pathinfo), which php lets the NUL through untouched because they` |
|         - | 1940 | ` * never touch the filesystem. Both are php-exact here already.` |
|         - | 1941 | ` */` |
|     29632 | 1942 | `static sxu32 VmBuiltinPathMask(SyString *pName)` |
|         5 | 1943 | `{` |
|         - | 1944 | `	static const struct {` |
|         - | 1945 | `		const char *zName;` |
|         - | 1946 | `		sxu32 nByte;` |
|         - | 1947 | `		sxu32 mask;` |
|         - | 1948 | `	} aPath[] = {` |
|         - | 1949 | `		/* Open / read / write */` |
|         - | 1950 | `		{ "fopen",             5, 1u<<0 },` |
|         - | 1951 | `		{ "file_get_contents", 17, 1u<<0 },` |
|         - | 1952 | `		{ "file_put_contents", 17, 1u<<0 },` |
|         - | 1953 | `		{ "file",              4, 1u<<0 },` |
|         - | 1954 | `		{ "readfile",          8, 1u<<0 },` |
|         - | 1955 | `		{ "parse_ini_file",   14, 1u<<0 },` |
|         - | 1956 | `		{ "md5_file",          8, 1u<<0 },` |
|         - | 1957 | `		{ "getimagesize",     12, 1u<<0 },` |
|         - | 1958 | `		{ "sha1_file",         9, 1u<<0 },` |
|         - | 1959 | `		{ "hash_file",         9, 1u<<1 },` |
|         - | 1960 | `		{ "hash_hmac_file",   14, 1u<<1 },` |
|         - | 1961 | `		{ "hash_update_file", 16, 1u<<1 },` |
|         - | 1962 | `		/* Metadata / mutation */` |
|         - | 1963 | `		{ "unlink",            6, 1u<<0 },` |
|         - | 1964 | `		{ "touch",             5, 1u<<0 },` |
|         - | 1965 | `		{ "chmod",             5, 1u<<0 },` |
|         - | 1966 | `		{ "chgrp",             5, 1u<<0 },` |
|         - | 1967 | `		{ "chown",             5, 1u<<0 },` |
|         - | 1968 | `		{ "rename",            6, (1u<<0)\|(1u<<1) },` |
|         - | 1969 | `		{ "copy",              4, (1u<<0)\|(1u<<1) },` |
|         - | 1970 | `		{ "link",              4, (1u<<0)\|(1u<<1) },` |
|         - | 1971 | `		{ "symlink",           7, (1u<<0)\|(1u<<1) },` |
|         - | 1972 | `		{ "readlink",          8, 1u<<0 },` |
|         - | 1973 | `		{ "realpath",          8, 1u<<0 },` |
|         - | 1974 | `		{ "stream_resolve_include_path", 27, 1u<<0 },` |
|         - | 1975 | `		/* Not a path at all: php reads inet_pton()'s $ip with the same` |
|         - | 1976 | `		 * NUL-refusing macro, and answers the same ValueError for a name` |
|         - | 1977 | `		 * that carries one. */` |
|         - | 1978 | `		{ "inet_pton",         9, 1u<<0 },` |
|         - | 1979 | `		/* Directories */` |
|         - | 1980 | `		{ "mkdir",             5, 1u<<0 },` |
|         - | 1981 | `		{ "rmdir",             5, 1u<<0 },` |
|         - | 1982 | `		{ "opendir",           7, 1u<<0 },` |
|         - | 1983 | `		{ "dir",               3, 1u<<0 },` |
|         - | 1984 | `		{ "scandir",           7, 1u<<0 },` |
|         - | 1985 | `		{ "chdir",             5, 1u<<0 },` |
|         - | 1986 | `		{ "chroot",            6, 1u<<0 },` |
|         - | 1987 | `		{ "glob",              4, 1u<<0 },` |
|         - | 1988 | `		{ "tempnam",           7, (1u<<0)\|(1u<<1) },` |
|         - | 1989 | `		{ "disk_free_space",  15, 1u<<0 },` |
|         - | 1990 | `		{ "disk_total_space", 16, 1u<<0 },` |
|         - | 1991 | `		{ "diskfreespace",    13, 1u<<0 },` |
|         - | 1992 | `		/* Not paths at all, and php screens them exactly as if they were: the` |
|         - | 1993 | `		 * datetime a FORMAT is read against is Z_PARAM_PATH_STR at every door` |
|         - | 1994 | `		 * that takes one, so a NUL inside it is the same catchable ValueError.` |
|         - | 1995 | ``		 * Only these five; `new DateTime($s)`, `date_create()`, `modify()` and`` |
|         - | 1996 | ``		 * `date_parse()` take an ordinary string there and read up to the NUL. */`` |
|         - | 1997 | `		{ "date_parse_from_format",                22, 1u<<1 },` |
|         - | 1998 | `		{ "date_create_from_format",               23, 1u<<1 },` |
|         - | 1999 | `		{ "date_create_immutable_from_format",     33, 1u<<1 },` |
|         - | 2000 | `		{ "DateTime::createFromFormat",            26, 1u<<1 },` |
|         - | 2001 | `		{ "DateTimeImmutable::createFromFormat",   35, 1u<<1 },` |
|         - | 2002 | ``		/* ext/sqlite3's two doors onto a database FILE. ext/pdo's `sqlite:` DSN is`` |
|         - | 2003 | `		 * not one of them: php parses a DSN before any of it becomes a path, and` |
|         - | 2004 | `		 * reads it up to the NUL. */` |
|         - | 2005 | `		{ "SQLite3::__construct",                  20, 1u<<0 },` |
|         - | 2006 | `		{ "SQLite3::open",                         13, 1u<<0 },` |
|         - | 2007 | `		/* Not a path either, and php screens it exactly as if it were: BOTH of` |
|         - | 2008 | `		 * bindtextdomain's arguments are Z_PARAM_PATH, so a NUL in the DOMAIN is` |
|         - | 2009 | `		 * the same catchable ValueError the directory gets. The other nine` |
|         - | 2010 | `		 * gettext doors take ordinary strings and read up to the NUL. */` |
|         - | 2011 | `		{ "bindtextdomain",   14, (1u<<0)\|(1u<<1) },` |
|         - | 2012 | `		/* ext/posix's five path doors, which php reads with the same macro. */` |
|         - | 2013 | `		{ "posix_access",     12, 1u<<0 },` |
|         - | 2014 | `		{ "posix_eaccess",    13, 1u<<0 },` |
|         - | 2015 | `		{ "posix_mkfifo",     12, 1u<<0 },` |
|         - | 2016 | `		{ "posix_mknod",      11, 1u<<0 },` |
|         - | 2017 | `		{ "posix_pathconf",   14, 1u<<0 },` |
|         - | 2018 | `		/* ext/fileinfo: the name a type is asked about, and the database the` |
|         - | 2019 | `		 * two openers name, are php Z_PARAM_PATH arguments -- so a NUL in one` |
|         - | 2020 | `		 * is the catchable ValueError rather than a truncated read. */` |
|         - | 2021 | `		{ "finfo_file",            10, 1u<<1 },` |
|         - | 2022 | `		{ "finfo_open",            10, 1u<<1 },` |
|         - | 2023 | `		{ "finfo::file",           11, 1u<<0 },` |
|         - | 2024 | `		{ "finfo::__construct",    18, 1u<<1 },` |
|         - | 2025 | `		{ "mime_content_type",     17, 1u<<0 },` |
|         - | 2026 | `		/* ext/simplexml's file door takes a php path argument too. */` |
|         - | 2027 | `		{ "simplexml_load_file",   19, 1u<<0 },` |
|         - | 2028 | `		/* Path-shaped settings and the pattern matcher */` |
|         - | 2029 | `		{ "fnmatch",           7, (1u<<0)\|(1u<<1) },` |
|         - | 2030 | `		{ "set_include_path", 16, 1u<<0 },` |
|         - | 2031 | `		{ "session_save_path", 17, 1u<<0 },` |
|         - | 2032 | `		{ "error_log",         9, 1u<<2 },` |
|         - | 2033 | `		/* Commands handed to the shell — and the two escapers, which php screens` |
|         - | 2034 | `		 * the same way even though neither of them runs anything: a NUL in what a` |
|         - | 2035 | `		 * script is about to hand a shell is refused where it is WRITTEN. */` |
|         - | 2036 | `		{ "shell_exec",       10, 1u<<0 },` |
|         - | 2037 | `		{ "popen",             5, 1u<<0 },` |
|         - | 2038 | `		{ "escapeshellarg",   14, 1u<<0 },` |
|         - | 2039 | `		{ "escapeshellcmd",   14, 1u<<0 },` |
|         - | 2040 | `		{ "exec",              4, 1u<<0 },` |
|         - | 2041 | `		{ "system",            6, 1u<<0 },` |
|         - | 2042 | `		{ "passthru",          8, 1u<<0 },` |
|         - | 2043 | `		/* The SPL path constructors, which php screens identically and reports` |
|         - | 2044 | ``		 * under their QUALIFIED name (`SplFileInfo::__construct(): Argument #1`` |
|         - | 2045 | ``		 * ($filename) …`). They are native methods, so their signature reaches this`` |
|         - | 2046 | `		 * screen the same way a builtin's does. */` |
|         - | 2047 | `		{ "SplFileInfo::__construct",                24, 1u<<0 },` |
|         - | 2048 | `		{ "DirectoryIterator::__construct",          30, 1u<<0 },` |
|         - | 2049 | `		{ "FilesystemIterator::__construct",         31, 1u<<0 },` |
|         - | 2050 | `		{ "RecursiveDirectoryIterator::__construct", 39, 1u<<0 },` |
|         - | 2051 | `		{ "GlobIterator::__construct",                25, 1u<<0 },` |
|         - | 2052 | `	};` |
|         - | 2053 | `	sxu32 i;` |
|     29637 | 2054 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|       ! 0 | 2055 | `		return 0;` |
|         - | 2056 | `	}` |
|   2057841 | 2057 | `	for( i = 0 ; i < SX_ARRAYSIZE(aPath) ; ++i ){` |
|   2030282 | 2058 | `		if( pName->nByte == aPath[i].nByte` |
|   1050130 | 2059 | `		 && SyStrnicmp(pName->zString,aPath[i].zName,pName->nByte) == 0 ){` |
|      2083 | 2060 | `			return aPath[i].mask;` |
|         - | 2061 | `		}` |
|   1004413 | 2062 | `	}` |
|     27559 | 2063 | `	return 0;` |
|     14677 | 2064 | `}` |
|         - | 2065 | `/*` |
|         - | 2066 | ` * Does this argument carry a NUL byte? Only a STRING can: every other scalar` |
|         - | 2067 | ` * renders through the number/bool formatters, which emit none. An OBJECT is` |
|         - | 2068 | ` * coerced by the caller before asking (php's ZPP order), so by the time this` |
|         - | 2069 | ` * runs a Stringable is already the string it produced.` |
|         - | 2070 | ` */` |
|    124034 | 2071 | `static int VmArgHasNulByte(ph7_value *pArg)` |
|         5 | 2072 | `{` |
|         - | 2073 | `	const char *zStr;` |
|         - | 2074 | `	sxu32 n, nLen;` |
|    124039 | 2075 | `	if( (pArg->iFlags & MEMOBJ_STRING) == 0 ){` |
|        15 | 2076 | `		return 0;` |
|         - | 2077 | `	}` |
|    124027 | 2078 | `	zStr = (const char *)SyBlobData(&pArg->sBlob);` |
|    124027 | 2079 | `	nLen = SyBlobLength(&pArg->sBlob);` |
|   8038388 | 2080 | `	for( n = 0 ; n < nLen ; ++n ){` |
|   7914494 | 2081 | `		if( zStr[n] == 0 ){` |
|       131 | 2082 | `			return 1;` |
|         - | 2083 | `		}` |
|   4103174 | 2084 | `	}` |
|    123899 | 2085 | `	return 0;` |
|     61914 | 2086 | `}` |
|         - | 2087 | `/*` |
|         - | 2088 | ` * Does php's strict_types rule refuse this argument for the declared type?` |
|         - | 2089 | ` *` |
|         - | 2090 | `` * A `declare(strict_types=1)` file gets NO scalar coercion at an internal call`` |
|         - | 2091 | ` * either — php applies the same rule to a builtin, a native method and a userland` |
|         - | 2092 | `` * function, and the single exception is the int -> float widening. So `trim(5)`,`` |
|         - | 2093 | `` * `sqrt("4")`, `str_repeat("a", 2.0)` and `in_array($n, $a, 1)` are all TypeErrors`` |
|         - | 2094 | ` * there, where the weak-mode screen below (which is the only one PHL had) coerces` |
|         - | 2095 | ` * and computes.` |
|         - | 2096 | ` *` |
|         - | 2097 | ` * Only the arms a scalar could otherwise satisfy are decided here; an array, a` |
|         - | 2098 | ` * resource, a null and a class-typed mismatch are the weak screen's, and its` |
|         - | 2099 | ` * verdicts stand in both modes.` |
|         - | 2100 | ` */` |
|      2044 | 2101 | `static int VmStrictArgRefused(ph7_value *pArg,const char *zType,int nType)` |
|         5 | 2102 | `{` |
|         - | 2103 | `	/* Tested in ph7_type_name()'s own order, so the branch taken and the name the` |
|         - | 2104 | `	 * refusal reports can never disagree. FLOAT comes before INT on purpose:` |
|         - | 2105 | `	 * ph7_value_is_int() is deliberately lenient — an integer-valued real caches an` |
|         - | 2106 | ``	 * int and answers TRUE — and `str_repeat("a", 2.0)` is php's TypeError, not an`` |
|         - | 2107 | `	 * accepted int. */` |
|      2049 | 2108 | `	if( ph7_value_is_bool(pArg) ){` |
|        51 | 2109 | `		return !VmSigTypeHas(zType,nType,"bool")` |
|        26 | 2110 | `		    && !VmSigTypeHas(zType,nType,"true")` |
|        48 | 2111 | `		    && !VmSigTypeHas(zType,nType,"false");` |
|         - | 2112 | `	}` |
|      2005 | 2113 | `	if( ph7_value_is_float(pArg) ){` |
|         7 | 2114 | `		return !VmSigTypeHas(zType,nType,"float");` |
|         - | 2115 | `	}` |
|      1999 | 2116 | `	if( ph7_value_is_int(pArg) ){` |
|         - | 2117 | `		/* int -> float is the one widening strict mode keeps. */` |
|       117 | 2118 | `		return !VmSigTypeHas(zType,nType,"int") && !VmSigTypeHas(zType,nType,"float");` |
|         - | 2119 | `	}` |
|      1885 | 2120 | `	if( ph7_value_is_string(pArg) ){` |
|         - | 2121 | ``		/* `callable` is not a coercion: a function-name string satisfies it in both`` |
|         - | 2122 | `		 * modes (array_map('strtoupper', …) under strict is php-legal). */` |
|      1559 | 2123 | `		return !VmSigTypeHas(zType,nType,"string") && !VmSigTypeHas(zType,nType,"callable");` |
|         - | 2124 | `	}` |
|       331 | 2125 | `	if( ph7_value_is_object(pArg) ){` |
|         - | 2126 | ``		/* An object reaches a `string` parameter only through __toString(), which is`` |
|         - | 2127 | `		 * a coercion strict mode does not perform. Every other arm is the weak` |
|         - | 2128 | `		 * screen's decision. */` |
|       173 | 2129 | `		return VmSigTypeHas(zType,nType,"string")` |
|        85 | 2130 | `		    && !VmSigTypeHas(zType,nType,"object")` |
|         4 | 2131 | `		    && !VmSigTypeHas(zType,nType,"iterable")` |
|         4 | 2132 | `		    && !VmSigTypeHas(zType,nType,"callable")` |
|       168 | 2133 | `		    && !VmSigTypeHasClass(zType,nType);` |
|         - | 2134 | `	}` |
|       165 | 2135 | `	return 0;` |
|      1027 | 2136 | `}` |
|         - | 2137 | `/*` |
|         - | 2138 | ` * The next parameter of a signature starting at *pzCur, or 0 when the screen must` |
|         - | 2139 | ` * STOP -- a malformed row with no '$', a variadic tail (whose type applies to every` |
|         - | 2140 | ` * argument after it), or the end of the text. Advances *pzCur past the parameter.` |
|         - | 2141 | ` *` |
|         - | 2142 | ` * This is the walk VmEnforceBuiltinArgTypes used to do inline, moved out unchanged so` |
|         - | 2143 | ` * that it has exactly ONE implementation: VmArgScreenStamp drives it once per builtin` |
|         - | 2144 | ` * to build the cached table, and the screen drives it per call when there is no table.` |
|         - | 2145 | ` * The cached form is therefore correct by construction rather than by inspection.` |
|         - | 2146 | ` */` |
|    139201 | 2147 | `static int VmArgScreenNext(const char **pzCur,const char *zEnd,VmArgScreenParam *pOut)` |
|         5 | 2148 | `{` |
|    139206 | 2149 | `	const char *zCur = *pzCur;` |
|         - | 2150 | `	const char *zType,*zName,*zStop;` |
|         - | 2151 | `	int nType,nName,bByRef;` |
|    139206 | 2152 | `	if( zCur >= zEnd ){` |
|     46916 | 2153 | `		return 0;` |
|         - | 2154 | `	}` |
|         - | 2155 | `	/* Parameter = "<type> $<name>[ = <default>]"; the type is whatever` |
|         - | 2156 | `	 * precedes the '$', and an empty type means "untyped" (no screen). */` |
|    139557 | 2157 | `	while( zCur < zEnd && zCur[0] == ' ' ){` |
|     47267 | 2158 | `		zCur++;` |
|         5 | 2159 | `	}` |
|     92295 | 2160 | `	zStop = zCur;` |
|   1697769 | 2161 | `	while( zStop < zEnd && zStop[0] != ',' ){` |
|   1605479 | 2162 | `		if( zStop[0] == '\'' \|\| zStop[0] == '"' ){` |
|      4925 | 2163 | `			zStop = VmSigSkipQuoted(zStop);` |
|      4925 | 2164 | `			if( zStop >= zEnd ){` |
|       ! 0 | 2165 | `				break;` |
|         - | 2166 | `			}` |
|      2454 | 2167 | `		}` |
|   1605479 | 2168 | `		zStop++;` |
|         5 | 2169 | `	}` |
|     92295 | 2170 | `	zName = zCur;` |
|    682421 | 2171 | `	while( zName < zStop && zName[0] != '$' ){` |
|    590131 | 2172 | `		zName++;` |
|         5 | 2173 | `	}` |
|     92295 | 2174 | `	if( zName >= zStop ){` |
|       ! 0 | 2175 | `		return 0; /* malformed / no parameter name -- stop screening */` |
|         - | 2176 | `	}` |
|     92295 | 2177 | `	if( zName >= zCur + 3 && SyMemcmp(zName - 3,"...",3) == 0 ){` |
|      6227 | 2178 | `		return 0; /* variadic tail: stop (its type applies to the rest) */` |
|         - | 2179 | `	}` |
|     86073 | 2180 | `	zType = zCur;` |
|     86073 | 2181 | `	nType = (int)(zName - zCur);` |
|         - | 2182 | ``	/* A `~Type $p` row is php's stub-versus-body mismatch: the type php DECLARES`` |
|         - | 2183 | `	 * (which Reflection must report) is looser than the one its C body asks for, so` |
|         - | 2184 | `	 * the screen stands aside and the builtin raises the TypeError itself.` |
|         - | 2185 | `	 * RecursiveCachingIterator::__construct is the first: it is declared` |
|         - | 2186 | ``	 * `Iterator $iterator` and refuses anything that is not a RecursiveIterator. */`` |
|     86073 | 2187 | `	pOut->bStub = (sxu8)((nType > 0 && zType[0] == '~') ? 1 : 0);` |
|         - | 2188 | `	/* Trim the trailing spaces and the by-ref marker of "array &$array" */` |
|     86073 | 2189 | `	bByRef = 0;` |
|    243249 | 2190 | `	while( nType > 0 && (zType[nType-1] == ' ' \|\| zType[nType-1] == '&') ){` |
|     79809 | 2191 | `		if( zType[nType-1] == '&' ){` |
|      3177 | 2192 | `			bByRef = 1;` |
|      1568 | 2193 | `		}` |
|     79809 | 2194 | `		nType--;` |
|         5 | 2195 | `	}` |
|     86073 | 2196 | `	zName++; /* skip '$' */` |
|     86073 | 2197 | `	nName = 0;` |
|    676867 | 2198 | `	while( &zName[nName] < zStop && zName[nName] != ' ' && zName[nName] != '=' ){` |
|    590799 | 2199 | `		nName++;` |
|         5 | 2200 | `	}` |
|     86073 | 2201 | `	pOut->zType  = zType;` |
|     86073 | 2202 | `	pOut->nType  = (sxu16)nType;` |
|     86073 | 2203 | `	pOut->zName  = zName;` |
|     86073 | 2204 | `	pOut->nName  = (sxu16)nName;` |
|     86073 | 2205 | `	pOut->bByRef = (sxu8)bByRef;` |
|         - | 2206 | `	/* Which arms this type has, asked ONCE. Every bit is set by calling the function` |
|         - | 2207 | `	 * that used to answer it per argument, so the mask cannot say something the walk` |
|         - | 2208 | `	 * would not -- the same construction the parse above uses, for the same reason:` |
|         - | 2209 | `	 * this screen decides TypeErrors. */` |
|         - | 2210 | `	{` |
|     86073 | 2211 | `		sxu32 m = 0;` |
|     86073 | 2212 | `		if( VmSigTypeHas(zType,nType,"mixed")    ){ m \|= VMSIG_MIXED;    }` |
|     86073 | 2213 | `		if( VmSigTypeHas(zType,nType,"array")    ){ m \|= VMSIG_ARRAY;    }` |
|     86073 | 2214 | `		if( VmSigTypeHas(zType,nType,"iterable") ){ m \|= VMSIG_ITERABLE; }` |
|     86073 | 2215 | `		if( VmSigTypeHas(zType,nType,"callable") ){ m \|= VMSIG_CALLABLE; }` |
|     86073 | 2216 | `		if( VmSigTypeHas(zType,nType,"object")   ){ m \|= VMSIG_OBJECT;   }` |
|     86073 | 2217 | `		if( VmSigTypeHas(zType,nType,"string")   ){ m \|= VMSIG_STRING;   }` |
|     86073 | 2218 | `		if( VmSigTypeHas(zType,nType,"null")     ){ m \|= VMSIG_NULL;     }` |
|     86073 | 2219 | `		if( VmSigTypeHas(zType,nType,"int")      ){ m \|= VMSIG_INT;      }` |
|     86073 | 2220 | `		if( VmSigTypeHas(zType,nType,"float")    ){ m \|= VMSIG_FLOAT;    }` |
|     86073 | 2221 | `		if( VmSigTypeHas(zType,nType,"bool")     ){ m \|= VMSIG_BOOL;     }` |
|     86073 | 2222 | `		if( VmSigTypeHas(zType,nType,"true")     ){ m \|= VMSIG_TRUE;     }` |
|     86073 | 2223 | `		if( VmSigTypeHas(zType,nType,"false")    ){ m \|= VMSIG_FALSE;    }` |
|     86073 | 2224 | `		if( VmSigTypeHas(zType,nType,"resource") ){ m \|= VMSIG_RESOURCE; }` |
|     86073 | 2225 | `		if( VmSigTypeHasClass(zType,nType)       ){ m \|= VMSIG_CLASS;    }` |
|     86073 | 2226 | `		if( VmSigTypeIsIntOnly(zType,nType)      ){ m \|= VMSIG_INTONLY;  }` |
|     86073 | 2227 | `		if( VmSigTypeIsArrayOnly(zType,nType)    ){ m \|= VMSIG_ARRAYONLY;}` |
|     86073 | 2228 | `		pOut->nMask = m;` |
|         - | 2229 | `	}` |
|     86073 | 2230 | `	*pzCur = (zStop < zEnd) ? zStop + 1 : zEnd;` |
|     86073 | 2231 | `	return 1;` |
|     68884 | 2232 | `}` |
|         - | 2233 | `/*` |
|         - | 2234 | ` * Work out this builtin's parameter table once and keep it on its record, beside the` |
|         - | 2235 | ` * two name questions and the signature length. Two passes over the same walk: count,` |
|         - | 2236 | ` * then fill. Leaves aSigParam at 0 when the signature yields no parameters or the` |
|         - | 2237 | ` * allocation fails, and the screen then walks the text per call exactly as before --` |
|         - | 2238 | ` * the fallback is the same code, so there is no second set of answers to keep in step.` |
|         - | 2239 | ` *` |
|         - | 2240 | ` * The table lives in the VM's allocator and is reclaimed with it, the same lifetime as` |
|         - | 2241 | ` * the name strdup beside it in PH7_NewForeignFunction.` |
|         - | 2242 | ` */` |
|     29632 | 2243 | `static void VmArgScreenStamp(ph7_vm *pVm,ph7_user_func *pFunc,const char *zSig,sxu32 nSigLen)` |
|         5 | 2244 | `{` |
|     29637 | 2245 | `	const char *zEnd = &zSig[nSigLen];` |
|         - | 2246 | `	const char *zCur;` |
|         - | 2247 | `	VmArgScreenParam sTmp;` |
|         - | 2248 | `	VmArgScreenParam *aParam;` |
|         - | 2249 | `	sxu32 n;` |
|     72671 | 2250 | `	for( n = 0, zCur = zSig ; VmArgScreenNext(&zCur,zEnd,&sTmp) ; ++n ){` |
|         - | 2251 | `		/* counting only */` |
|     21292 | 2252 | `	}` |
|     29637 | 2253 | `	if( n < 1 ){` |
|      8296 | 2254 | `		return;` |
|         - | 2255 | `	}` |
|     31899 | 2256 | `	aParam = (VmArgScreenParam *)SyMemBackendPoolAlloc(&pVm->sAllocator,` |
|     10553 | 2257 | `		n * (sxu32)sizeof(VmArgScreenParam));` |
|     21346 | 2258 | `	if( aParam == 0 ){` |
|       ! 0 | 2259 | `		return;` |
|         - | 2260 | `	}` |
|     64380 | 2261 | `	for( n = 0, zCur = zSig ; VmArgScreenNext(&zCur,zEnd,&aParam[n]) ; ++n ){` |
|         - | 2262 | `		/* filling */` |
|     21292 | 2263 | `	}` |
|     21346 | 2264 | `	pFunc->aSigParam = aParam;` |
|     21346 | 2265 | `	pFunc->nSigParam = (sxu16)n;` |
|     14677 | 2266 | `}` |
|         - | 2267 | `/*` |
|         - | 2268 | ` * PHP-8 ZPP type enforcement for host functions, driven by the aBuiltinSig[]` |
|         - | 2269 | ` * declaration (band A #7). Screens only the arguments that php can NEVER coerce` |
|         - | 2270 | ` * into a declared scalar parameter — arrays, resources, and objects without a` |
|         - | 2271 | ` * __toString() — and throws the catchable TypeError php throws, before the C` |
|         - | 2272 | ` * routine runs. Without this an array argument reached the builtin and was` |
|         - | 2273 | ` * stringified to the literal "Array" (strrev(['a']) returned "yarrA").` |
|         - | 2274 | ` *` |
|         - | 2275 | ` * Deliberately narrow: scalar-to-scalar coercion (and its deprecations) stays` |
|         - | 2276 | ` * with the per-builtin ZPP helpers. Those emit E_DEPRECATED, which does NOT` |
|         - | 2277 | ` * abort the call, so a central copy would double-fire — the trap that sank the` |
|         - | 2278 | ` * first central-ZPP attempt. A TypeError aborts, so there is nothing to double.` |
|         - | 2279 | ` */` |
|  19220560 | 2280 | `PH7_PRIVATE sxi32 VmEnforceBuiltinArgTypes(` |
|         - | 2281 | `	ph7_context *pCtx,    /* Call context (for the throw) */` |
|         - | 2282 | `	ph7_user_func *pFunc, /* Callee */` |
|         - | 2283 | `	int nGiven,           /* Argument count */` |
|         - | 2284 | `	ph7_value **apArg     /* Arguments */` |
|         - | 2285 | `	)` |
|         5 | 2286 | `{` |
|         - | 2287 | `	/*` |
|         - | 2288 | `	 * Builtins whose own argument check is php-exact and VALUE-based rather than` |
|         - | 2289 | `	 * type-based must not be pre-empted here, or their message is lost. Same rule` |
|         - | 2290 | `	 * the aBuiltinArity[] table follows: a builtin that already says what php says` |
|         - | 2291 | `	 * stays off the shared screen. get_class_vars() takes any stringifiable value` |
|         - | 2292 | `	 * and reports "must be a valid class name, Array given"; get_class_methods() is` |
|         - | 2293 | `	 * the same shape with php's other wording ("must be an object or a valid class` |
|         - | 2294 | ``	 * name, int given") — the declared `object\|string` never appears in either.`` |
|         - | 2295 | `	 *` |
|         - | 2296 | `	 * strtr() is here for a structural reason: php declares it as two OVERLOADS` |
|         - | 2297 | `	 * dispatched on arity — strtr(string, array) and strtr(string, string, string)` |
|         - | 2298 | `` 	 * — so the expected type of $from is `array` with two arguments and `string` `` |
|         - | 2299 | ``	 * with three. One signature cannot say that (the stub's `array\|string` is the`` |
|         - | 2300 | `	 * union of the two, which is the wording php never uses), so the builtin does` |
|         - | 2301 | `	 * its own dispatch through PH7_ArgSatisfiesString(), the same rule as here.` |
|         - | 2302 | `	 *` |
|         - | 2303 | ``	 * implode() is the same structure: `array\|string $separator` is what the two`` |
|         - | 2304 | `	 * ARITIES accept between them, never what one call can use. Once an $array` |
|         - | 2305 | `	 * argument is present php has resolved the overload and reports` |
|         - | 2306 | ``	 * `must be of type string`, and with the array in position #1 it reports`` |
|         - | 2307 | ``	 * `must be of type string, array given` against #1 rather than a #2 error.`` |
|         - | 2308 | `	 * PH7_builtin_implode words all of that itself.` |
|         - | 2309 | `	 *` |
|         - | 2310 | `	 * Its alias join() is here for the same reason and then some: php 8.5 does not` |
|         - | 2311 | `	 * word the two the same, so the builtin reproduces BOTH orders keyed on the` |
|         - | 2312 | `	 * invoked name (see PH7_builtin_implode's header for the value-for-value` |
|         - | 2313 | `	 * table against 8.5.8). php's own asymmetry between a target and its alias,` |
|         - | 2314 | `	 * reproduced rather than smoothed over — parity is binding (the scope policy).` |
|         - | 2315 | `	 *` |
|         - | 2316 | `	 * number_format() is here because php's DECLARED type and its REFUSAL text` |
|         - | 2317 | ``	 * disagree: the stub says `float $num` (which is what Reflection prints) while`` |
|         - | 2318 | `	 * the ZPP macro behind it is Z_PARAM_NUMBER, whose TypeError says` |
|         - | 2319 | ``	 * `must be of type int\|float`. One row cannot say both, so the row carries the`` |
|         - | 2320 | `	 * declared type for Reflection and the builtin words every refusal itself.` |
|         - | 2321 | `	 *` |
|         - | 2322 | `	 * RecursiveIteratorIterator::__construct() is the first NATIVE METHOD here, and` |
|         - | 2323 | `	 * it is the same disagreement one level up: php's stub declares` |
|         - | 2324 | ``	 * `Traversable $iterator` (what Reflection prints) while its ZPP is a bare "o",`` |
|         - | 2325 | ``	 * whose TypeError says `must be of type object`. A native method's diagnostic`` |
|         - | 2326 | `	 * name is the QUALIFIED one, so the row below matches it and nothing else.` |
|         - | 2327 | `	 *` |
|         - | 2328 | `	 * The array_udiff/array_uintersect u-variant family is here for its ORDER:` |
|         - | 2329 | `	 * php validates the trailing comparison callback(s) before ANY of the` |
|         - | 2330 | `	 * arrays — array_diff_ukey(123,[1],456) names Argument #3, not #1 — and a` |
|         - | 2331 | `	 * positional screen cannot say that. HashmapUVariant performs the whole` |
|         - | 2332 | `	 * php sequence itself (callbacks, then Argument #1, then the middles).` |
|         - | 2333 | `	 */` |
|         - | 2334 | `	static const char *azSelfChecked[] = { "get_class_vars", "get_class_methods", "strtr",` |
|         - | 2335 | `		"implode", "join", "number_format", "RecursiveIteratorIterator::__construct",` |
|         - | 2336 | `		"array_udiff", "array_udiff_assoc", "array_udiff_uassoc",` |
|         - | 2337 | `		"array_uintersect", "array_uintersect_assoc", "array_uintersect_uassoc",` |
|         - | 2338 | `		"array_diff_uassoc", "array_diff_ukey",` |
|         - | 2339 | `		"array_intersect_uassoc", "array_intersect_ukey" };` |
|  19220565 | 2340 | `	const char *zSig = pFunc->zSig;` |
|         - | 2341 | `	const char *zCur, *zEnd;` |
|  19220565 | 2342 | `	int iArg = 0;` |
|         - | 2343 | `	/* The CALL site's file mode, stamped by the compiler onto this call's argument` |
|         - | 2344 | `	 * map (weak when there is no map — a call that carries no compile-time metadata` |
|         - | 2345 | `	 * was written in a weak-mode file, since a strict one always attaches one). */` |
|  19220565 | 2346 | `	int bStrict = (pCtx->pArgMap && pCtx->pArgMap->bStrict) ? 1 : 0;` |
|         - | 2347 | `	sxu32 nPathMask;` |
|  19220565 | 2348 | `	if( zSig == 0 ){` |
|    278472 | 2349 | `		return SXRET_OK;` |
|         - | 2350 | `	}` |
|         - | 2351 | `	/* All three of the questions below are about the DECLARATION, which cannot change:` |
|         - | 2352 | `	 * two are about the NAME -- and used to be answered by SCANNING a table on every` |
|         - | 2353 | `	 * builtin call, seventeen names here and about seventy in VmBuiltinPathMask, two` |
|         - | 2354 | `	 * SyStrlen calls per row -- and the third is the length of the signature TEXT,` |
|         - | 2355 | `	 * which zEnd below used to measure on every call. Worked out once and kept on the` |
|         - | 2356 | `	 * function's own record. */` |
|  18942098 | 2357 | `	if( !pFunc->bScreenStamped ){` |
|         - | 2358 | `		int iSelf;` |
|     29637 | 2359 | `		pFunc->nPathMask = VmBuiltinPathMask(&pFunc->sName);` |
|     29637 | 2360 | `		pFunc->nSigLen = (sxu32)SyStrlen(zSig);` |
|     29637 | 2361 | `		VmArgScreenStamp(pCtx->pVm,pFunc,zSig,pFunc->nSigLen);` |
|    527918 | 2362 | `		for( iSelf = 0 ; iSelf < (int)SX_ARRAYSIZE(azSelfChecked) ; ++iSelf ){` |
|    498750 | 2363 | `			const char *zSelf = azSelfChecked[iSelf];` |
|    498745 | 2364 | `			if( SyStrncmp(pFunc->sName.zString,zSelf,pFunc->sName.nByte) == 0` |
|    247776 | 2365 | `			 && SyStrlen(zSelf) == pFunc->sName.nByte ){` |
|       469 | 2366 | `				pFunc->bSelfChecked = 1;` |
|       469 | 2367 | `				break;` |
|         - | 2368 | `			}` |
|    246740 | 2369 | `		}` |
|     29637 | 2370 | `		pFunc->bScreenStamped = 1;` |
|     14672 | 2371 | `	}` |
|  18942098 | 2372 | `	if( pFunc->bSelfChecked ){` |
|     57394 | 2373 | `		return SXRET_OK;` |
|         - | 2374 | `	}` |
|  18884709 | 2375 | `	nPathMask = pFunc->nPathMask;` |
|  18884709 | 2376 | `	zCur = zSig;` |
|  18884709 | 2377 | `	zEnd = &zSig[pFunc->nSigLen];   /* measured once, above -- never per call */` |
|  48314224 | 2378 | `	for( iArg = 0 ; iArg < nGiven ; ++iArg ){` |
|         - | 2379 | `		VmArgScreenParam sParam;` |
|         - | 2380 | `		const char *zType, *zName;` |
|         - | 2381 | `		int nType, nName, bByRef;` |
|         - | 2382 | `		sxu32 nMask;` |
|         - | 2383 | `		ph7_value *pArg;` |
|         - | 2384 | `		char zGivenBuf[64];` |
|         - | 2385 | `		/* The parameter this argument is screened against. Worked out once per` |
|         - | 2386 | `		 * builtin when the table could be built, and by the same walk per call when` |
|         - | 2387 | `		 * it could not; either way the screen stops where the walk stopped. */` |
|  29467956 | 2388 | `		if( pFunc->aSigParam ){` |
|  29465796 | 2389 | `			if( iArg >= (int)pFunc->nSigParam ){` |
|     36023 | 2390 | `				break;` |
|         - | 2391 | `			}` |
|  29430858 | 2392 | `			sParam = pFunc->aSigParam[iArg];` |
|  14718995 | 2393 | `		}else if( !VmArgScreenNext(&zCur,zEnd,&sParam) ){` |
|      2165 | 2394 | `			break;` |
|         - | 2395 | `		}` |
|  29430858 | 2396 | `		if( sParam.bStub ){` |
|      1012 | 2397 | `			continue; /* the builtin raises its own TypeError -- see VmArgScreenNext */` |
|         - | 2398 | `		}` |
|  29429858 | 2399 | `		zType  = sParam.zType;` |
|  29429858 | 2400 | `		nType  = (int)sParam.nType;` |
|  29429858 | 2401 | `		zName  = sParam.zName;` |
|  29429858 | 2402 | `		nName  = (int)sParam.nName;` |
|  29429858 | 2403 | `		bByRef = sParam.bByRef;` |
|  29429858 | 2404 | `		nMask  = sParam.nMask;   /* which arms this type has, worked out once per parameter */` |
|  29429858 | 2405 | `		pArg = apArg[iArg];` |
|  29429853 | 2406 | `		if( bByRef && pArg->nIdx == SXU32_HIGH` |
|     19842 | 2407 | `		 && !(pCtx->pArgMap && pCtx->pArgMap->bArgShapes && !pCtx->pArgMap->bHasNamed) ){` |
|         - | 2408 | `			/* A by-reference parameter handed something with no slot to write back` |
|         - | 2409 | `			 * through -- a literal, a constant, the result of a call. php settles` |
|         - | 2410 | `			 * that at the CALL, before the callee's ZPP runs, so the type screen` |
|         - | 2411 | ``			 * must not speak first: `array_pop('foo')` is`` |
|         - | 2412 | `			 * "could not be passed by reference" and not "must be of type array,` |
|         - | 2413 | `			 * string given".` |
|         - | 2414 | `			 *` |
|         - | 2415 | `			 * Only when this call site carries no argument SHAPES, though. When it` |
|         - | 2416 | `			 * does, PH7_VmScreenByRefArgShapes has already had its say — it refused` |
|         - | 2417 | `			 * the literal and let the call RESULT through with php's notice — and` |
|         - | 2418 | `			 * standing aside here would swallow the type error php still reports for` |
|         - | 2419 | ``			 * the latter (`sort(new stdClass)` is "must be of type array, stdClass`` |
|         - | 2420 | `			 * given", not a silent false). */` |
|        18 | 2421 | `			continue;` |
|         - | 2422 | `		}` |
|  29429844 | 2423 | `		if( nType > 0 && !(nMask & VMSIG_MIXED) ){` |
|  27755204 | 2424 | `			const char *zGiven = 0;` |
|  27755204 | 2425 | `			if( bStrict && VmStrictArgRefused(pArg,zType,nType) ){` |
|         - | 2426 | ``				/* php names the VALUE for a bool here too (`true given`). */`` |
|        44 | 2427 | `				zGiven = VmValueGivenName(pArg,zGivenBuf,sizeof(zGivenBuf));` |
|  27755183 | 2428 | `			}else if( (pArg->iFlags & MEMOBJ_HASHMAP) != 0 ){` |
|    547900 | 2429 | `				if( !(nMask & VMSIG_ARRAY)` |
|    274363 | 2430 | `				 && !(nMask & VMSIG_ITERABLE)` |
|       953 | 2431 | `				 && !(nMask & VMSIG_CALLABLE) ){` |
|       269 | 2432 | `					zGiven = "array";` |
|       137 | 2433 | `				}` |
|  27481151 | 2434 | `			}else if( (pArg->iFlags & MEMOBJ_OBJ) != 0 ){` |
|     67203 | 2435 | `				if( !(nMask & VMSIG_OBJECT)` |
|     58231 | 2436 | `				 && !(nMask & VMSIG_ITERABLE)` |
|     49310 | 2437 | `				 && !(nMask & VMSIG_CALLABLE)` |
|     47060 | 2438 | `				 && !(nMask & VMSIG_CLASS) ){` |
|         - | 2439 | `					/* An object with __toString() still satisfies a string` |
|         - | 2440 | `					 * parameter in weak mode — php coerces it. */` |
|       416 | 2441 | `					int bStringable = (nMask & VMSIG_STRING)` |
|       216 | 2442 | `						&& PH7_ArgSatisfiesString(pArg);` |
|       221 | 2443 | `					if( !bStringable ){` |
|       113 | 2444 | `						zGiven = VmArgTypeName(pArg);` |
|        54 | 2445 | `					}` |
|     67100 | 2446 | `				}else if( (nMask & VMSIG_CLASS)` |
|     55652 | 2447 | `				       && !(nMask & VMSIG_OBJECT)` |
|     44379 | 2448 | `				       && !(nMask & VMSIG_ITERABLE)` |
|     44379 | 2449 | `				       && !(nMask & VMSIG_CALLABLE)` |
|     44384 | 2450 | `				       && !(nMask & VMSIG_STRING) ){` |
|         - | 2451 | `					/* A class-typed parameter given an object of the WRONG class.` |
|         - | 2452 | `					 * Naming a class used to be enough to let ANY object through, so` |
|         - | 2453 | `` 					 * `date_modify($immutable)` and `timezone_name_get($date)` `` |
|         - | 2454 | `					 * answered silently where php raises. Only decided when every` |
|         - | 2455 | `					 * class arm resolves to a declared class: an arm PHL does not` |
|         - | 2456 | `					 * declare cannot be judged, so the parameter stays unscreened. */` |
|     65962 | 2457 | `					if( !VmSigObjSatisfiesClass(pCtx->pVm,zType,nType,` |
|     43969 | 2458 | `						(ph7_class_instance *)pArg->x.pOther) ){` |
|        78 | 2459 | `						zGiven = VmArgTypeName(pArg);` |
|        37 | 2460 | `					}` |
|     21993 | 2461 | `				}` |
|  27173633 | 2462 | `			}else if( (pArg->iFlags & MEMOBJ_NULL) != 0 ){` |
|         - | 2463 | `				/* php only DEPRECATES null for a non-nullable parameter; PHL rejects` |
|         - | 2464 | `				 * it (scope policy). A leading '?' or an explicit "null" arm in a` |
|         - | 2465 | `				 * union declares the parameter nullable. A "callable" parameter is` |
|         - | 2466 | `				 * left to the builtin's own callback check, which words the failure` |
|         - | 2467 | `				 * php's way ("must be a valid callback, no array or string given") —` |
|         - | 2468 | `				 * the same reason get_class_vars() sits on azSelfChecked[]. */` |
|      8583 | 2469 | `				if( zType[0] != '?'` |
|      4393 | 2470 | `				 && !(nMask & VMSIG_NULL)` |
|       173 | 2471 | `				 && !(nMask & VMSIG_CALLABLE) ){` |
|       115 | 2472 | `					zGiven = "null";` |
|        55 | 2473 | `				}` |
|  27135763 | 2474 | `			}else if( (pArg->iFlags & (MEMOBJ_STRING\|MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL)) != 0` |
|  27131470 | 2475 | `			       && ((nMask & VMSIG_CLASS)` |
|  27130802 | 2476 | `			        \|\| (nMask & VMSIG_OBJECT)) ){` |
|         - | 2477 | `				/* A SCALAR against a parameter that can only hold an INSTANCE —` |
|         - | 2478 | ``				 * a named class, or the bare `object` keyword. Every other scalar`` |
|         - | 2479 | `				 * pairing is left to weak-mode coercion, which is why nothing` |
|         - | 2480 | `				 * screened scalars here at all — but no coercion produces an` |
|         - | 2481 | `				 * instance, so php rejects this one. Found converting DateTime:` |
|         - | 2482 | ``				 * `$d->diff('x')` and `new DateTime('now','UTC')` ran on with a`` |
|         - | 2483 | ``				 * string where php raises. The `object` half was still blind when`` |
|         - | 2484 | `				 * WeakReference::create() declared the first such parameter, which` |
|         - | 2485 | `				 * also retires the "graceful degradation" NULL that spl_object_id(),` |
|         - | 2486 | `				 * spl_object_hash() and get_object_vars() used to answer. An arm a` |
|         - | 2487 | `				 * scalar CAN satisfy (a union with string/int/float/bool, or` |
|         - | 2488 | `				 * callable, which a string is) keeps the parameter unscreened —` |
|         - | 2489 | ``				 * and so does an `array` arm, whose refusal php words from the`` |
|         - | 2490 | `				 * builtin's own check rather than from the declared type` |
|         - | 2491 | ``				 * (array_walk's `array\|object &$array` says "must be of type`` |
|         - | 2492 | `				 * array", not "of type array\|object"). */` |
|      5505 | 2493 | `				if( !(nMask & VMSIG_STRING)` |
|      2912 | 2494 | `				 && !(nMask & VMSIG_INT)` |
|       254 | 2495 | `				 && !(nMask & VMSIG_FLOAT)` |
|       168 | 2496 | `				 && !(nMask & VMSIG_BOOL)` |
|       168 | 2497 | `				 && !(nMask & VMSIG_TRUE)` |
|       168 | 2498 | `				 && !(nMask & VMSIG_FALSE)` |
|       168 | 2499 | `				 && !(nMask & VMSIG_ARRAY)` |
|       133 | 2500 | `				 && !(nMask & VMSIG_CALLABLE) ){` |
|         - | 2501 | `					/* php's VALUE name, not the type's: a bool is reported as` |
|         - | 2502 | ``					 * `true`/`false` (the rule Generator::throw()'s own check`` |
|         - | 2503 | `					 * already followed, and which this screen now runs first). */` |
|        92 | 2504 | `					zGiven = VmValueGivenName(pArg,zGivenBuf,sizeof(zGivenBuf));` |
|      5465 | 2505 | `				}else if( (pArg->iFlags & (MEMOBJ_STRING\|MEMOBJ_NULL)) == MEMOBJ_STRING` |
|      5305 | 2506 | `				       && !(nMask & VMSIG_STRING)` |
|      2602 | 2507 | `				       && !(nMask & VMSIG_BOOL)` |
|        32 | 2508 | `				       && !(nMask & VMSIG_TRUE)` |
|        32 | 2509 | `				       && !(nMask & VMSIG_FALSE)` |
|        32 | 2510 | `				       && !(nMask & VMSIG_ARRAY)` |
|        22 | 2511 | `				       && !(nMask & VMSIG_CALLABLE)` |
|        17 | 2512 | `				       && !PH7_MemObjStringIsNumeric(pArg) ){` |
|         - | 2513 | ``					/* The one arm that let this STRING past is `int`/`float`, and it`` |
|         - | 2514 | `					 * only takes a NUMERIC one — no coercion turns a string into an` |
|         - | 2515 | ``					 * instance of the class arm beside it. `round(1.5, 0, "x")` is`` |
|         - | 2516 | ``					 * php's `must be of type RoundingMode\|int, string given`; PHL`` |
|         - | 2517 | `					 * narrowed it to mode 0 and reported the ValueError for an` |
|         - | 2518 | `					 * invalid MODE, which blames the wrong thing. The plain` |
|         - | 2519 | `					 * number-only spelling is screened by the STRING branch below;` |
|         - | 2520 | `					 * a class arm routes the same argument through here instead, so` |
|         - | 2521 | `					 * the rule has to be stated in both places. */` |
|         7 | 2522 | `					zGiven = "string";` |
|         3 | 2523 | `				}` |
|  27128713 | 2524 | `			}else if( (pArg->iFlags & MEMOBJ_REAL) != 0` |
|  13565391 | 2525 | `			       && (nMask & VMSIG_INTONLY)` |
|       855 | 2526 | `			       && !VmDoubleFitsInt((double)pArg->rVal) ){` |
|         - | 2527 | ``				/* A FLOAT against a parameter typed exactly `int` (or `?int`), and`` |
|         - | 2528 | `				 * one no int can hold: a fraction, a magnitude past the signed` |
|         - | 2529 | `				 * 64-bit range, NaN or an infinity. php refuses every one of them` |
|         - | 2530 | `				 * (zend_parse_arg_long's ZEND_DOUBLE_FITS_LONG / is-integral pair,` |
|         - | 2531 | `				 * the fractional case with a deprecation PHL rejects outright by` |
|         - | 2532 | `				 * the scope policy) and the refusal is this screen's own wording.` |
|         - | 2533 | `				 *` |
|         - | 2534 | `				 * PH7_IntArgResolve has always said exactly this, but only for the` |
|         - | 2535 | `` 				 * builtins that CALL it from their own body — so `dechex(1.5)` `` |
|         - | 2536 | ``				 * answered '1', `array_fill(1.5,1,0)` filled from 1, and`` |
|         - | 2537 | ``				 * `strpos("abc","c",1e19)` took the offset as PHP_INT_MIN and`` |
|         - | 2538 | `				 * reported a ValueError about a range it never had. Seventy-five` |
|         - | 2539 | ``				 * `int` parameters across the signature table were unscreened that`` |
|         - | 2540 | `				 * way, and a NATIVE METHOD has no body to call the helper from at` |
|         - | 2541 | `				 * all. Deciding it from the declared type covers both callee kinds` |
|         - | 2542 | `				 * from one place, and the per-builtin helper still stands for the` |
|         - | 2543 | ``				 * message rows this screen cannot reach (the `azSelfChecked` set). */`` |
|        64 | 2544 | `				zGiven = "float";` |
|  27125940 | 2545 | `			}else if( (pArg->iFlags & (MEMOBJ_STRING\|MEMOBJ_NULL)) == MEMOBJ_STRING` |
|  21630801 | 2546 | `			       && ((nMask & VMSIG_INT)` |
|  16133320 | 2547 | `			        \|\| (nMask & VMSIG_FLOAT))` |
|   8068282 | 2548 | `			       && !(nMask & VMSIG_STRING)` |
|   8067819 | 2549 | `			       && !(nMask & VMSIG_ARRAY)` |
|       308 | 2550 | `			       && !(nMask & VMSIG_OBJECT)` |
|       300 | 2551 | `			       && !(nMask & VMSIG_ITERABLE)` |
|       300 | 2552 | `			       && !(nMask & VMSIG_CALLABLE)` |
|       300 | 2553 | `			       && !(nMask & VMSIG_BOOL)` |
|       305 | 2554 | `			       && !(nMask & VMSIG_CLASS) ){` |
|         - | 2555 | ``				/* A STRING against a NUMBER-only parameter — `int`, `float`, or the`` |
|         - | 2556 | ``				 * `int\|float` union, with no arm a string can satisfy. Weak mode`` |
|         - | 2557 | `				 * coerces a NUMERIC one and php refuses every other — "x", "2abc"` |
|         - | 2558 | ``				 * and "0x2" are all `must be of type int, string given` (rule 41: a`` |
|         - | 2559 | `				 * numeric PREFIX is not enough, which is what SyStrIsNumeric would` |
|         - | 2560 | `				 * have accepted). Every BUILTIN with an int parameter already got` |
|         - | 2561 | `				 * this from PH7_IntArgResolve, called from its own body; a native` |
|         - | 2562 | `` 				 * METHOD has no body to call it from, so `ArrayIterator::seek('x')` `` |
|         - | 2563 | ``				 * seeked to 0, `DateTime::setTimestamp('abc')` set 0 and`` |
|         - | 2564 | ``				 * `DOMNodeList::item('zz')` answered element 0 — wrong ANSWERS,`` |
|         - | 2565 | `				 * not missing errors. Screening the declared type here covers both` |
|         - | 2566 | `				 * callee kinds from one place.` |
|         - | 2567 | `				 *` |
|         - | 2568 | `				 * The FLOAT arm is the same hazard one type over, and it was the` |
|         - | 2569 | `				 * half nothing covered: PH7_IntArgResolve has no float twin, so a` |
|         - | 2570 | ``				 * `float $num` builtin that did not hand-roll its own check simply`` |
|         - | 2571 | `				 * converted the string to 0.0 and COMPUTED with it —` |
|         - | 2572 | ``				 * `cos("nope")` answered `float(1)`, `sqrt("nope")` `float(0)`,`` |
|         - | 2573 | ``				 * `log("nope")` `float(-INF)`. Numbers with nothing wrong-looking`` |
|         - | 2574 | `				 * about them, from input php refuses outright.` |
|         - | 2575 | `				 *` |
|         - | 2576 | `				 * The NULL rule stays where it is: PHL rejects null for a` |
|         - | 2577 | `				 * non-nullable parameter by the scope policy, where php deprecates. */` |
|       455 | 2578 | `				if( !PH7_MemObjStringIsNumeric(pArg) ){` |
|       181 | 2579 | `					zGiven = "string";` |
|       216 | 2580 | `				}else if( (nMask & VMSIG_INTONLY) && !VmNumStrFitsInt(pArg) ){` |
|         - | 2581 | `					/* A NUMERIC string an int cannot hold — "1.5", "1e19",` |
|         - | 2582 | `					 * "99999999999999999999". php refuses all three (the fractional` |
|         - | 2583 | `					 * one after a deprecation the scope policy turns into the refusal), and PHL` |
|         - | 2584 | ``					 * narrowed them silently: `dechex("1e19")` answered '1' and`` |
|         - | 2585 | ``					 * `str_repeat("a","99999999999999999999")` took PHP_INT_MAX as`` |
|         - | 2586 | `					 * the count. Same wording, same position as the float arm above,` |
|         - | 2587 | `					 * because php reaches both through one ZPP macro. */` |
|        19 | 2588 | `					zGiven = "string";` |
|         8 | 2589 | `				}` |
|  27125761 | 2590 | `			}else if( (pArg->iFlags & (MEMOBJ_STRING\|MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL)) != 0` |
|  27125605 | 2591 | `			       && (nMask & VMSIG_ARRAYONLY) ){` |
|         - | 2592 | ``				/* A SCALAR against a parameter typed exactly `array`. No coercion`` |
|         - | 2593 | `				 * produces one, so php refuses it -- but the screen exempted every` |
|         - | 2594 | ``				 * `array` arm, union or not, and a whole family had no check of its`` |
|         - | 2595 | `				 * own to fall back on: sort/rsort/ksort/krsort/shuffle and` |
|         - | 2596 | ``				 * usort/uasort/uksort each answered `false` for `sort($notAnArray)`,`` |
|         - | 2597 | `				 * which is also what they answer for a sort that genuinely failed.` |
|         - | 2598 | `				 * call_user_func_array('strlen', 'x') answered false too,` |
|         - | 2599 | `				 * iterator_apply RAN the callback, and getopt/hash/password_hash/` |
|         - | 2600 | `				 * password_needs_rehash/unserialize/fputcsv simply carried on with` |
|         - | 2601 | `				 * the string where an options ARRAY was declared.` |
|         - | 2602 | `				 *` |
|         - | 2603 | `				 * The builtins that DO check (array_keys, in_array, asort, ...) word` |
|         - | 2604 | `				 * it identically, so the screen only pre-empts them -- and corrects` |
|         - | 2605 | `				 * one detail on the way: their ph7_type_name() says "bool" where php` |
|         - | 2606 | ``				 * names the VALUE, `true` or `false`. */`` |
|       259 | 2607 | `				zGiven = VmValueGivenName(pArg,zGivenBuf,sizeof(zGivenBuf));` |
|  27125484 | 2608 | `			}else if( (pArg->iFlags & MEMOBJ_RES) != 0 ){` |
|         - | 2609 | `				/* A class-typed parameter also accepts a resource: several handles php 8` |
|         - | 2610 | `				 * models as objects are still resources here (xml_*'s XMLParser is the` |
|         - | 2611 | `				 * one the signatures already declare php-8-style, for reflection). The` |
|         - | 2612 | `				 * screen would otherwise reject the engine's own parser handle. Recorded` |
|         - | 2613 | `				 * as a recorded divergence — it goes away when those handles become` |
|         - | 2614 | `				 * real objects. */` |
|        12 | 2615 | `				if( !(nMask & VMSIG_RESOURCE)` |
|        14 | 2616 | `				 && !(nMask & VMSIG_CLASS) ){` |
|        14 | 2617 | `					zGiven = "resource";` |
|         6 | 2618 | `				}` |
|         6 | 2619 | `			}` |
|  27755204 | 2620 | `			if( zGiven ){` |
|         - | 2621 | ``				/* php's `object\|array` parameters come from ONE ZPP macro`` |
|         - | 2622 | `				 * (Z_PARAM_ARRAY_OR_OBJECT) and it names only "array" in the` |
|         - | 2623 | `				 * refusal — array_walk(null,…), current(null) and` |
|         - | 2624 | `				 * http_build_query(null) all say "must be of type array". The` |
|         - | 2625 | `				 * SCALAR branch above already encodes that rule by declining to` |
|         - | 2626 | `				 * screen at all; the null and resource branches do screen, so the` |
|         - | 2627 | `				 * reported type has to be corrected here instead. */` |
|      1215 | 2628 | `				if( (nMask & VMSIG_ARRAY) && (nMask & VMSIG_OBJECT) ){` |
|        16 | 2629 | `					zType = "array";` |
|        16 | 2630 | `					nType = (int)sizeof("array")-1;` |
|         7 | 2631 | `				}` |
|      1887 | 2632 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 2633 | `					"%z(): Argument #%d ($%.*s) must be of type %.*s, %s given",` |
|       605 | 2634 | `					&pFunc->sName,iArg + 1,nName,zName,nType,zType,zGiven);` |
|         - | 2635 | `			}` |
|  13878494 | 2636 | `		}` |
|         - | 2637 | ``		/* A NaN reaching a parameter php declares `string`: php's ZPP coerces it`` |
|         - | 2638 | ``		 * (to "NAN") and warns `unexpected NAN value was coerced to string`, the`` |
|         - | 2639 | `		 * same 8.5 diagnostic the cast and the concatenation raise. The builtin` |
|         - | 2640 | `		 * bodies read their argument with ph7_value_to_string, which is the SILENT` |
|         - | 2641 | `		 * conversion by design (the engine builds keys and messages with it), so` |
|         - | 2642 | `		 * the diagnostic belongs here, where the DECLARED type says a coercion is` |
|         - | 2643 | `		 * what is about to happen. A union that also accepts a NUMBER is left` |
|         - | 2644 | `		 * alone -- php keeps the float there and coerces nothing -- but` |
|         - | 2645 | ``		 * `array\|string`, the spelling str_replace()'s subject carries, does`` |
|         - | 2646 | `		 * coerce and does warn. */` |
|  29428629 | 2647 | `		if( (pArg->iFlags & (MEMOBJ_REAL\|MEMOBJ_STRING)) == MEMOBJ_REAL` |
|  14718122 | 2648 | `		 && PH7_IS_NAN((double)pArg->rVal)` |
|      2483 | 2649 | `		 && (nMask & VMSIG_STRING)` |
|        80 | 2650 | `		 && !(nMask & VMSIG_FLOAT)` |
|         5 | 2651 | `		 && !(nMask & VMSIG_INT)` |
|         7 | 2652 | `		 && !(nMask & VMSIG_MIXED) ){` |
|         3 | 2653 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 2654 | `				"unexpected NAN value was coerced to string");` |
|         1 | 2655 | `		}` |
|         - | 2656 | `		/* A PATH parameter, once its type is settled: php's Z_PARAM_PATH refuses a` |
|         - | 2657 | `		 * NUL byte outright rather than letting the C API truncate at it. Raised` |
|         - | 2658 | `		 * after the type verdict because that is php's order — the coercion runs` |
|         - | 2659 | `		 * first, and only a value that could BE a path is asked whether it is a` |
|         - | 2660 | `		 * legal one. */` |
|  29428634 | 2661 | `		if( iArg < 31 && (nPathMask & (1u<<iArg)) != 0 ){` |
|    124039 | 2662 | `			if( (pArg->iFlags & MEMOBJ_OBJ) != 0 && PH7_ArgSatisfiesString(pArg) ){` |
|         - | 2663 | `				/* A Stringable object: php coerces it and checks the RESULT, so` |
|         - | 2664 | ``				 * `unlink($o)` with a __toString() returning a NUL-bearing name is`` |
|         - | 2665 | `				 * the same ValueError. Converting IN PLACE is what keeps the` |
|         - | 2666 | `				 * accessor running exactly ONCE — the builtin then receives the` |
|         - | 2667 | `				 * string it would have produced itself. The argument a builtin sees` |
|         - | 2668 | `				 * is its own copy on every dispatch route (a direct call, a spread,` |
|         - | 2669 | `				 * both call_user_func forwards), so the caller's object is not` |
|         - | 2670 | `				 * retyped; strict mode never gets here, because a Stringable does` |
|         - | 2671 | ``				 * not satisfy a `string` parameter there and the screen above has`` |
|         - | 2672 | `				 * already refused it. */` |
|         3 | 2673 | `				sxi32 rcConv = PH7_MemObjToStringUV(pArg);` |
|         3 | 2674 | `				if( rcConv != SXRET_OK ){` |
|       ! 0 | 2675 | `					return rcConv; /* __toString() threw: php propagates it too */` |
|         - | 2676 | `				}` |
|         1 | 2677 | `			}` |
|    124039 | 2678 | `			if( VmArgHasNulByte(pArg) ){` |
|       192 | 2679 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 2680 | `					"%z(): Argument #%d ($%.*s) must not contain any null bytes",` |
|        61 | 2681 | `					&pFunc->sName,iArg + 1,nName,zName);` |
|         - | 2682 | `			}` |
|     61848 | 2683 | `		}` |
|  14715662 | 2684 | `	}` |
|  18883371 | 2685 | `	return SXRET_OK;` |
|   9611485 | 2686 | `}` |
|         - | 2687 | `/*` |
|         - | 2688 | ` * Builtins whose accepted arity is NOT a contiguous range, so the signature` |
|         - | 2689 | ` * cannot express it and the central too-many-arguments check must stay out of` |
|         - | 2690 | ` * the way: rand()/mt_rand() take 0 OR 2 arguments (never 1), and php words the` |
|         - | 2691 | ` * violation "expects exactly 2 arguments, 3 given" from their own check rather` |
|         - | 2692 | ` * than the ZPP "at most". They validate themselves; leaving bHasMaxArg at 0` |
|         - | 2693 | ` * keeps their message php-faithful.` |
|         - | 2694 | ` */` |
|   7634638 | 2695 | `static int VmBuiltinSelfValidatesArity(const char *zName)` |
|         5 | 2696 | `{` |
|         - | 2697 | `	static const char *const azSelf[] = { "rand", "mt_rand" };` |
|         - | 2698 | `	sxu32 i;` |
|  22882964 | 2699 | `	for( i = 0 ; i < SX_ARRAYSIZE(azSelf) ; i++ ){` |
|  15262296 | 2700 | `		sxu32 nSelf = SyStrlen(azSelf[i]);` |
|  15262296 | 2701 | `		if( SyStrlen(zName) == nSelf && SyStrncmp(zName,azSelf[i],nSelf) == 0 ){` |
|     13975 | 2702 | `			return 1;` |
|         - | 2703 | `		}` |
|   7591204 | 2704 | `	}` |
|   7620673 | 2705 | `	return 0;` |
|   3800835 | 2706 | `}` |
|         - | 2707 | `/*` |
|         - | 2708 | ` * One parameter of a declared signature, for the named-argument binder below.` |
|         - | 2709 | ` */` |
|         - | 2710 | `typedef struct VmSigParam VmSigParam;` |
|         - | 2711 | `struct VmSigParam` |
|         - | 2712 | `{` |
|         - | 2713 | `	const char *zName; int nName;   /* without the '$' */` |
|         - | 2714 | `	const char *zDef;  int nDef;    /* default TEXT, or 0 when the parameter is required */` |
|         - | 2715 | `	int bVariadic;` |
|         - | 2716 | `};` |
|         - | 2717 | `/*` |
|         - | 2718 | ` * Split a signature into its parameters: the NAME each one binds by and the default` |
|         - | 2719 | ` * TEXT to fall back on. The scan is VmDeriveArityFromSig's, kept apart because that one` |
|         - | 2720 | `` * only counts; a quoted default (`string $separator = ','`) hides a comma, which is why`` |
|         - | 2721 | ` * both go through VmSigSkipQuoted.` |
|         - | 2722 | ` */` |
|    122671 | 2723 | `static int VmSigParams(const char *zSig,VmSigParam *aOut,int nMax)` |
|         5 | 2724 | `{` |
|    122676 | 2725 | `	const char *zCur = zSig;` |
|    122676 | 2726 | `	const char *zStart = zSig;` |
|    122676 | 2727 | `	int n = 0;` |
|   4866013 | 2728 | `	for(;;){` |
|  10152093 | 2729 | `		if( zCur[0] == '\'' \|\| zCur[0] == '"' ){` |
|       192 | 2730 | `			zCur = VmSigSkipQuoted(zCur);` |
|       192 | 2731 | `			if( zCur[0] != '\0' ){` |
|       192 | 2732 | `				zCur++;` |
|        94 | 2733 | `			}` |
|       192 | 2734 | `			continue;` |
|         - | 2735 | `		}` |
|  10151905 | 2736 | `		if( zCur[0] == '\0' \|\| zCur[0] == ',' ){` |
|    521541 | 2737 | `			const char *z = zStart;` |
|    521541 | 2738 | `			const char *zEnd = zCur;` |
|    521541 | 2739 | `			if( n < nMax ){` |
|    521541 | 2740 | `				VmSigParam *p = &aOut[n];` |
|    521541 | 2741 | `				const char *zEq = 0;` |
|    521541 | 2742 | `				const char *zDollar = 0;` |
|    521541 | 2743 | `				p->zName = 0; p->nName = 0; p->zDef = 0; p->nDef = 0; p->bVariadic = 0;` |
|  10152373 | 2744 | `				for( ; z < zEnd ; z++ ){` |
|   9630837 | 2745 | `					if( z[0] == '$' && zDollar == 0 ){` |
|    521541 | 2746 | `						zDollar = z + 1;` |
|   9369533 | 2747 | `					}else if( z[0] == '=' && zEq == 0 ){` |
|    196119 | 2748 | `						zEq = z + 1;` |
|   9011100 | 2749 | `					}else if( z[0] == '.' && z + 2 < zEnd && z[1] == '.' && z[2] == '.' ){` |
|       681 | 2750 | `						p->bVariadic = 1;` |
|       338 | 2751 | `					}` |
|   4805056 | 2752 | `				}` |
|    521541 | 2753 | `				if( zDollar ){` |
|    521541 | 2754 | `					const char *zStop = zEq ? zEq - 1 : zEnd;` |
|    521541 | 2755 | `					const char *zN = zDollar;` |
|   3827346 | 2756 | `					while( zN < zStop && zN[0] != ' ' && zN[0] != '=' ){` |
|   3305810 | 2757 | `						zN++;` |
|         5 | 2758 | `					}` |
|    521541 | 2759 | `					p->zName = zDollar;` |
|    521541 | 2760 | `					p->nName = (int)(zN - zDollar);` |
|    260232 | 2761 | `				}` |
|    521541 | 2762 | `				if( zEq ){` |
|    392233 | 2763 | `					while( zEq < zEnd && zEq[0] == ' ' ){` |
|    196119 | 2764 | `						zEq++;` |
|         5 | 2765 | `					}` |
|    196119 | 2766 | `					p->zDef = zEq;` |
|    196119 | 2767 | `					p->nDef = (int)(zEnd - zEq);` |
|    196119 | 2768 | `					while( p->nDef > 0 && p->zDef[p->nDef-1] == ' ' ){` |
|       ! 0 | 2769 | `						p->nDef--;` |
|       ! 0 | 2770 | `					}` |
|     97913 | 2771 | `				}` |
|    521541 | 2772 | `				if( p->nName > 0 ){` |
|    521541 | 2773 | `					n++;` |
|    260232 | 2774 | `				}` |
|    260232 | 2775 | `			}` |
|    521541 | 2776 | `			if( zCur[0] == '\0' ){` |
|    122676 | 2777 | `				break;` |
|         - | 2778 | `			}` |
|    398870 | 2779 | `			zCur++;` |
|    398870 | 2780 | `			zStart = zCur;` |
|    398870 | 2781 | `			continue;` |
|         - | 2782 | `		}` |
|   9630369 | 2783 | `		zCur++;` |
|         5 | 2784 | `	}` |
|    122676 | 2785 | `	return n;` |
|         5 | 2786 | `}` |
|         - | 2787 | `/*` |
|         - | 2788 | ` * Materialize a signature default's TEXT into pOut, for a parameter a NAMED call` |
|         - | 2789 | ` * skipped.` |
|         - | 2790 | ` *` |
|         - | 2791 | ` * The reduction itself is Reflection's (PH7_VmSigDefaultToValue): a default is` |
|         - | 2792 | ` * read off the SAME signature string getDefaultValue() reads, so it has to mean` |
|         - | 2793 | ` * the same thing at both doors, and this one used to carry a smaller reader of` |
|         - | 2794 | ` * its own -- see the note over PH7_VmSigDefaultToValue for what the two` |
|         - | 2795 | `` * disagreed about. Two cases stay here: `[]`, whose value is a hashmap rather`` |
|         - | 2796 | `` * than a scalar, and the `= ?` marker (~50 rows the table cannot state, recorded),`` |
|         - | 2797 | ` * which answers 0 so the caller reports the parameter as not passed rather than` |
|         - | 2798 | ` * inventing a value.` |
|         - | 2799 | ` */` |
|        36 | 2800 | `static int VmSigDefaultValue(ph7_context *pCtx,const VmSigParam *pParam,ph7_value *pOut)` |
|         3 | 2801 | `{` |
|        39 | 2802 | `	const char *z = pParam->zDef;` |
|        39 | 2803 | `	int n = pParam->nDef;` |
|        39 | 2804 | `	if( z == 0 \|\| n < 1 \|\| (n == 1 && z[0] == '?') ){` |
|         6 | 2805 | `		return 0;` |
|         - | 2806 | `	}` |
|        35 | 2807 | `	if( n == 2 && z[0] == '[' && z[1] == ']' ){` |
|         3 | 2808 | `		ph7_hashmap *pMap = PH7_NewHashmap(pCtx->pVm,0,0);` |
|         3 | 2809 | `		if( pMap == 0 ){` |
|       ! 0 | 2810 | `			return 0;` |
|         - | 2811 | `		}` |
|         3 | 2812 | `		PH7_MemObjRelease(pOut);` |
|         3 | 2813 | `		pOut->x.pOther = pMap;` |
|         3 | 2814 | `		MemObjSetType(pOut,MEMOBJ_HASHMAP);` |
|         3 | 2815 | `		return 1;` |
|         - | 2816 | `	}` |
|        33 | 2817 | `	return PH7_VmSigDefaultToValue(pCtx,z,n,pOut);` |
|        21 | 2818 | `}` |
|         - | 2819 | `/*` |
|         - | 2820 | ` * Bind a call's NAMED arguments to the callee's declared parameter POSITIONS.` |
|         - | 2821 | ` *` |
|         - | 2822 | ` * A compiled function does this from its parameter records (VmResolveNamedArgs); a host` |
|         - | 2823 | ` * function and a native method have none, so every named argument was simply passed in the` |
|         - | 2824 | `` * order it was WRITTEN. `str_pad(length: 5, string: "x")` reached the builtin as`` |
|         - | 2825 | ` * ("x" at #2, 5 at #1) and reported a TypeError, and — worse, because it is silent —` |
|         - | 2826 | `` * `str_pad("x", 5, pad_type: STR_PAD_LEFT)` bound the flag as $pad_string and answered`` |
|         - | 2827 | ` * "x0000" where php answers "    x". Both spellings are php 8.0 syntax, and the whole` |
|         - | 2828 | ` * ~650-builtin surface plus every native method was affected.` |
|         - | 2829 | ` *` |
|         - | 2830 | ` * The declared signature is the source of names, defaults and positions — the same string` |
|         - | 2831 | ` * Reflection prints. Rewrites pArgSet (the caller's argument vector is scratch it owns;` |
|         - | 2832 | ` * it is REBUILT rather than written in place, because a skipped default can make the` |
|         - | 2833 | ` * bound vector longer than the one written) and *pnArg, and answers SXRET_OK, or throws` |
|         - | 2834 | ` * php's Error and returns its status.` |
|         - | 2835 | ` *` |
|         - | 2836 | ` * A VARIADIC tail cannot be named: php leaves it out of the parameters a name is looked up` |
|         - | 2837 | ` * in, so a name matching no declared parameter -- the tail's own included -- is an EXTRA,` |
|         - | 2838 | ` * collected by name after the positional actuals. Those land at the end of the vector,` |
|         - | 2839 | ` * *pnExtra counts them, and pTail becomes a copy of pMap naming each slot of the bound` |
|         - | 2840 | ` * vector (its aNames is the caller's to free), so a forwarding builtin can hand them on` |
|         - | 2841 | ` * keyed. Whether the callee TAKES extras at all is its own ZPP's question in php, asked` |
|         - | 2842 | ` * at a moment of its own, so it is the caller's (PH7_VmBuiltinExtraNamedRule).` |
|         - | 2843 | ` */` |
|       408 | 2844 | `PH7_PRIVATE sxi32 PH7_VmBindNamedArgsToSig(` |
|         - | 2845 | `	ph7_context *pCtx,      /* Call context (for the throws) */` |
|         - | 2846 | `	ph7_user_func *pFunc,   /* Callee: its zSig names the parameters */` |
|         - | 2847 | `	VmCallArgMap *pMap,     /* Call-site map; its aNames[] are per ACTUAL slot */` |
|         - | 2848 | `	SySet *pArgSet,         /* IN/OUT: argument vector (ph7_value *) */` |
|         - | 2849 | `	int *pnArg,             /* IN/OUT: argument count */` |
|         - | 2850 | `	int *pnExtra,           /* OUT: trailing actuals that are unknown-name extras */` |
|         - | 2851 | `	VmCallArgMap *pTail     /* OUT: the bound vector's names, when *pnExtra > 0 */` |
|         - | 2852 | `	)` |
|         5 | 2853 | `{` |
|         - | 2854 | `	/* php's own stubs top out well under this; a signature with more parameters simply` |
|         - | 2855 | `	 * keeps the positional binding it had. */` |
|         - | 2856 | `#define VM_SIG_MAX_PARAM 32` |
|         - | 2857 | `	VmSigParam aParam[VM_SIG_MAX_PARAM];` |
|         - | 2858 | `	ph7_value *apBound[VM_SIG_MAX_PARAM];` |
|         - | 2859 | `	ph7_value *apTail[VM_SIG_MAX_PARAM];` |
|         - | 2860 | `	ph7_value *apExtra[VM_SIG_MAX_PARAM];` |
|         - | 2861 | `	SyString aTailName[VM_SIG_MAX_PARAM];` |
|         - | 2862 | `	ph7_value **apArg;` |
|         - | 2863 | `	int nParam,nDecl,nArg,i,nLast,nTail,nExtra,bNamedSeen,nPos,nNamedHigh;` |
|         - | 2864 | `	sxu32 nNamedRun;` |
|       413 | 2865 | `	*pnExtra = 0;` |
|       413 | 2866 | `	if( pFunc == 0 \|\| pFunc->zSig == 0 \|\| pMap == 0 \|\| pMap->bHasNamed == 0 ){` |
|        33 | 2867 | `		return SXRET_OK;` |
|         - | 2868 | `	}` |
|       381 | 2869 | `	nArg = *pnArg;` |
|       381 | 2870 | `	if( nArg < 1 \|\| nArg > VM_SIG_MAX_PARAM ){` |
|       ! 0 | 2871 | `		return SXRET_OK;` |
|         - | 2872 | `	}` |
|       381 | 2873 | `	nParam = VmSigParams(pFunc->zSig,aParam,VM_SIG_MAX_PARAM);` |
|       381 | 2874 | `	if( nParam < 1 ){` |
|       ! 0 | 2875 | `		return SXRET_OK;` |
|         - | 2876 | `	}` |
|       381 | 2877 | `	nDecl = aParam[nParam-1].bVariadic ? nParam - 1 : nParam;` |
|       381 | 2878 | `	apArg = (ph7_value **)SySetBasePtr(pArgSet);` |
|       937 | 2879 | `	for( i = 0 ; i < nDecl ; ++i ){` |
|       561 | 2880 | `		apBound[i] = 0;` |
|       283 | 2881 | `	}` |
|       381 | 2882 | `	nLast = -1;` |
|       381 | 2883 | `	nTail = 0;` |
|       381 | 2884 | `	nExtra = 0;` |
|       381 | 2885 | `	bNamedSeen = 0;` |
|       381 | 2886 | `	nPos = 0;` |
|       381 | 2887 | `	nNamedHigh = 0;` |
|       381 | 2888 | `	nNamedRun = 0;` |
|      1157 | 2889 | `	for( i = 0 ; i < nArg ; ++i ){` |
|         - | 2890 | `		int p;` |
|       919 | 2891 | `		if( i < (int)pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|       547 | 2892 | `			SyString *pName = &pMap->aNames[i];` |
|       547 | 2893 | `			bNamedSeen = 1;` |
|       547 | 2894 | `			nNamedRun = pMap->aRun ? pMap->aRun[i] : 0;` |
|       907 | 2895 | `			for( p = 0 ; p < nDecl ; ++p ){` |
|       628 | 2896 | `				if( (int)pName->nByte == aParam[p].nName` |
|       478 | 2897 | `				 && SyMemcmp(pName->zString,aParam[p].zName,pName->nByte) == 0 ){` |
|       273 | 2898 | `					break;` |
|         - | 2899 | `				}` |
|       185 | 2900 | `			}` |
|       547 | 2901 | `			if( p >= nDecl ){` |
|       279 | 2902 | `				if( nDecl == nParam ){` |
|       ! 0 | 2903 | `					return PH7_VmThrowException(pCtx,"Error",` |
|       ! 0 | 2904 | `						"Unknown named parameter $%z",pName);` |
|         - | 2905 | `				}` |
|         - | 2906 | `				/* php's variadic collects the extras by name AFTER every positional` |
|         - | 2907 | `				 * one, whichever unpack wrote them first. */` |
|       279 | 2908 | `				apExtra[nExtra] = apArg[i];` |
|       279 | 2909 | `				aTailName[nExtra] = *pName;` |
|       279 | 2910 | `				nExtra++;` |
|       279 | 2911 | `				continue;` |
|         - | 2912 | `			}` |
|       273 | 2913 | `			if( apBound[p] ){` |
|       ! 0 | 2914 | `				return PH7_VmThrowException(pCtx,"Error",` |
|       ! 0 | 2915 | `					"Named parameter $%z overwrites previous argument",pName);` |
|         - | 2916 | `			}` |
|       273 | 2917 | `			if( p >= nNamedHigh ){` |
|       231 | 2918 | `				nNamedHigh = p + 1;` |
|       118 | 2919 | `			}` |
|       377 | 2920 | `		}else if( bNamedSeen && (pMap->aRun == 0 \|\| pMap->aRun[i] == nNamedRun) ){` |
|         - | 2921 | `			/* A positional argument after a named one inside ONE unpack (or one` |
|         - | 2922 | `			 * rebuilt array): php's Error, worded by where the list came from. */` |
|         5 | 2923 | `			return PH7_VmThrowException(pCtx,"Error",pMap->bFromUnpack` |
|         - | 2924 | `				? "Cannot use positional argument after named argument during unpacking"` |
|         - | 2925 | `				: "Cannot use positional argument after named argument");` |
|       ! 0 | 2926 | `		}else{` |
|         - | 2927 | `			/* A later unpack's positional argument binds after the highest parameter` |
|         - | 2928 | `			 * a name filled (zend's num_args), not at the place it was written. */` |
|       239 | 2929 | `			p = nPos > nNamedHigh ? nPos : nNamedHigh;` |
|       239 | 2930 | `			nPos = p + 1;` |
|         - | 2931 | `		}` |
|       507 | 2932 | `		if( p >= nDecl ){` |
|         - | 2933 | `			/* More positional arguments than the signature knows (or a variadic's` |
|         - | 2934 | `			 * tail): kept after the bound ones, so a hole a name jumped over is` |
|         - | 2935 | ``			 * still php's `not passed` before the arity screen counts them. */`` |
|        85 | 2936 | `			apTail[nTail] = apArg[i];` |
|        85 | 2937 | `			nTail++;` |
|        85 | 2938 | `			continue;` |
|         - | 2939 | `		}` |
|       427 | 2940 | `		apBound[p] = apArg[i];` |
|       427 | 2941 | `		if( p > nLast ){` |
|       385 | 2942 | `			nLast = p;` |
|       190 | 2943 | `		}` |
|       216 | 2944 | `	}` |
|       821 | 2945 | `	for( i = 0 ; i <= nLast ; ++i ){` |
|       453 | 2946 | `		if( apBound[i] == 0 ){` |
|        39 | 2947 | `			ph7_value *pDef = ph7_context_new_scalar(pCtx);` |
|        39 | 2948 | `			if( pDef == 0 \|\| !VmSigDefaultValue(pCtx,&aParam[i],pDef) ){` |
|         - | 2949 | `				SyString sName;` |
|         6 | 2950 | `				SyStringInitFromBuf(&sName,aParam[i].zName,(sxu32)aParam[i].nName);` |
|         8 | 2951 | `				return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|         2 | 2952 | `					"%z(): Argument #%d ($%z) not passed",&pFunc->sName,i + 1,&sName);` |
|         - | 2953 | `			}` |
|        35 | 2954 | `			apBound[i] = pDef;` |
|        16 | 2955 | `		}` |
|       227 | 2956 | `	}` |
|       373 | 2957 | `	if( nExtra > 0 ){` |
|       231 | 2958 | `		sxu32 nName = (sxu32)(nLast + 1 + nTail + nExtra);` |
|       344 | 2959 | `		SyString *aName = (SyString *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|       113 | 2960 | `			nName * sizeof(SyString));` |
|       231 | 2961 | `		if( aName == 0 ){` |
|       ! 0 | 2962 | `			return SXRET_OK;` |
|         - | 2963 | `		}` |
|       231 | 2964 | `		SyZero(aName,(sxu32)((nLast + 1 + nTail) * sizeof(SyString)));` |
|       505 | 2965 | `		for( i = 0 ; i < nExtra ; ++i ){` |
|       279 | 2966 | `			aName[nLast + 1 + nTail + i] = aTailName[i];` |
|       142 | 2967 | `		}` |
|       231 | 2968 | `		*pTail = *pMap;` |
|       231 | 2969 | `		pTail->bArgShapes = 0;` |
|       231 | 2970 | `		pTail->nNonLvalMask = 0;` |
|       231 | 2971 | `		pTail->nTempCallMask = 0;` |
|       231 | 2972 | `		pTail->nTotal = nName;` |
|       231 | 2973 | `		pTail->aNames = aName;` |
|       231 | 2974 | `		pTail->aRun = 0; /* the bound vector is one run: every extra is after the positionals */` |
|       231 | 2975 | `		*pnExtra = nExtra;` |
|       113 | 2976 | `	}` |
|       373 | 2977 | `	SySetReset(pArgSet);` |
|       815 | 2978 | `	for( i = 0 ; i <= nLast ; ++i ){` |
|       447 | 2979 | `		SySetPut(pArgSet,(const void *)&apBound[i]);` |
|       226 | 2980 | `	}` |
|       453 | 2981 | `	for( i = 0 ; i < nTail ; ++i ){` |
|        85 | 2982 | `		SySetPut(pArgSet,(const void *)&apTail[i]);` |
|        45 | 2983 | `	}` |
|       647 | 2984 | `	for( i = 0 ; i < nExtra ; ++i ){` |
|       279 | 2985 | `		SySetPut(pArgSet,(const void *)&apExtra[i]);` |
|       142 | 2986 | `	}` |
|       373 | 2987 | `	*pnArg = (int)SySetUsed(pArgSet);` |
|       373 | 2988 | `	return SXRET_OK;` |
|       209 | 2989 | `}` |
|         - | 2990 | `/*` |
|         - | 2991 | ` * What a builtin does with the unknown-name EXTRAS the binder above collected past its` |
|         - | 2992 | ` * variadic signature, and WHEN -- php answers from the callee's own parameter parsing, so` |
|         - | 2993 | ` * the moment depends on how its C code parses, not on its stub. Derived from php 8.5 by` |
|         - | 2994 | ` * calling every variadic builtin with one extra, with and without a wrong-typed first` |
|         - | 2995 | ` * argument:` |
|         - | 2996 | ` *` |
|         - | 2997 | ` *   VM_XNAMED_TAKE          the forwards (Z_PARAM_VARIADIC_WITH_NAMED), which hand the` |
|         - | 2998 | ` *                           extras on keyed. forward_static_call() is NOT one. Closure's` |
|         - | 2999 | ` *                           __invoke is, but carries no signature to collect against.` |
|         - | 3000 | `` *   VM_XNAMED_BEFORE_ARITY  the old-style `zend_parse_parameters("+f")` parsers, which`` |
|         - | 3001 | `` *                           refuse while reading their spec: `array_intersect(zz: 1)` is`` |
|         - | 3002 | `` *                           the refusal where `sprintf(zz: 1)` is "expects at least 1".`` |
|         - | 3003 | ` *   VM_XNAMED_BEFORE_TYPES  parsers that read the whole list as the variadic, so the` |
|         - | 3004 | `` *                           stub's declared `array $array` is not screened before it.`` |
|         - | 3005 | ` *   VM_XNAMED_REFUSE        everything else: the declared parameters' screens first.` |
|         - | 3006 | ` */` |
|       248 | 3007 | `PH7_PRIVATE int PH7_VmBuiltinExtraNamedRule(const SyString *pName)` |
|         5 | 3008 | `{` |
|         - | 3009 | `	static const struct { const char *zName; int iRule; } aRule[] = {` |
|         - | 3010 | `		{ "call_user_func",              VM_XNAMED_TAKE },` |
|         - | 3011 | `		{ "Closure::call",               VM_XNAMED_TAKE },` |
|         - | 3012 | `		{ "Fiber::start",                VM_XNAMED_TAKE },` |
|         - | 3013 | `		{ "ReflectionFunction::invoke",  VM_XNAMED_TAKE },` |
|         - | 3014 | `		{ "ReflectionMethod::invoke",    VM_XNAMED_TAKE },` |
|         - | 3015 | `		{ "ReflectionClass::newInstance",VM_XNAMED_TAKE },` |
|         - | 3016 | `		{ "array_diff_assoc",            VM_XNAMED_BEFORE_ARITY },` |
|         - | 3017 | `		{ "array_diff_key",              VM_XNAMED_BEFORE_ARITY },` |
|         - | 3018 | `		{ "array_diff_uassoc",           VM_XNAMED_BEFORE_ARITY },` |
|         - | 3019 | `		{ "array_diff_ukey",             VM_XNAMED_BEFORE_ARITY },` |
|         - | 3020 | `		{ "array_intersect",             VM_XNAMED_BEFORE_ARITY },` |
|         - | 3021 | `		{ "array_intersect_assoc",       VM_XNAMED_BEFORE_ARITY },` |
|         - | 3022 | `		{ "array_intersect_key",         VM_XNAMED_BEFORE_ARITY },` |
|         - | 3023 | `		{ "array_intersect_uassoc",      VM_XNAMED_BEFORE_ARITY },` |
|         - | 3024 | `		{ "array_intersect_ukey",        VM_XNAMED_BEFORE_ARITY },` |
|         - | 3025 | `		{ "array_udiff",                 VM_XNAMED_BEFORE_ARITY },` |
|         - | 3026 | `		{ "array_udiff_assoc",           VM_XNAMED_BEFORE_ARITY },` |
|         - | 3027 | `		{ "array_udiff_uassoc",          VM_XNAMED_BEFORE_ARITY },` |
|         - | 3028 | `		{ "array_uintersect",            VM_XNAMED_BEFORE_ARITY },` |
|         - | 3029 | `		{ "array_uintersect_assoc",      VM_XNAMED_BEFORE_ARITY },` |
|         - | 3030 | `		{ "array_uintersect_uassoc",     VM_XNAMED_BEFORE_ARITY },` |
|         - | 3031 | `		{ "register_shutdown_function",  VM_XNAMED_BEFORE_ARITY },` |
|         - | 3032 | `		{ "register_tick_function",      VM_XNAMED_BEFORE_ARITY },` |
|         - | 3033 | `		{ "array_diff",                  VM_XNAMED_BEFORE_TYPES },` |
|         - | 3034 | `		{ "array_replace",               VM_XNAMED_BEFORE_TYPES },` |
|         - | 3035 | `		{ "array_replace_recursive",     VM_XNAMED_BEFORE_TYPES },` |
|         - | 3036 | `	};` |
|         - | 3037 | `	sxu32 i;` |
|      2331 | 3038 | `	for( i = 0 ; i < SX_ARRAYSIZE(aRule) ; ++i ){` |
|      2293 | 3039 | `		sxu32 nByte = SyStrlen(aRule[i].zName);` |
|      2288 | 3040 | `		if( pName->nByte == nByte` |
|      1274 | 3041 | `		 && SyStrnicmp(pName->zString,aRule[i].zName,nByte) == 0 ){` |
|       215 | 3042 | `			return aRule[i].iRule;` |
|         - | 3043 | `		}` |
|      1043 | 3044 | `	}` |
|        40 | 3045 | `	return VM_XNAMED_REFUSE;` |
|       129 | 3046 | `}` |
|         - | 3047 | `/*` |
|         - | 3048 | ` * php's refusal of the extras, raised from inside the callee: the internal frame is on` |
|         - | 3049 | ` * the trace, and a native method is named with its class.` |
|         - | 3050 | ` */` |
|        40 | 3051 | `PH7_PRIVATE sxi32 PH7_VmRefuseExtraNamed(ph7_context *pCtx,ph7_user_func *pFunc)` |
|         2 | 3052 | `{` |
|        62 | 3053 | `	return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|        20 | 3054 | `		"%z() does not accept unknown named parameters",&pFunc->sName);` |
|         2 | 3055 | `}` |
|         - | 3056 | `/*` |
|         - | 3057 | ` * Name the Nth (0-based) parameter of a declared signature, without the '$'.` |
|         - | 3058 | ` *` |
|         - | 3059 | ` * The signature string is the only place a host function's parameter names live, and` |
|         - | 3060 | `` * php puts them in diagnostics — `sort(): Argument #1 ($array) …`. Answers 0 when the`` |
|         - | 3061 | ` * signature has no such parameter (or none with a name).` |
|         - | 3062 | ` */` |
|        30 | 3063 | `PH7_PRIVATE int PH7_VmSigParamName(const char *zSig,int nPos,SyString *pOut)` |
|         4 | 3064 | `{` |
|         - | 3065 | `	VmSigParam aParam[VM_SIG_MAX_PARAM];` |
|         - | 3066 | `	int nParam;` |
|        34 | 3067 | `	if( zSig == 0 \|\| nPos < 0 \|\| nPos >= VM_SIG_MAX_PARAM ){` |
|       ! 0 | 3068 | `		return 0;` |
|         - | 3069 | `	}` |
|        34 | 3070 | `	nParam = VmSigParams(zSig,aParam,VM_SIG_MAX_PARAM);` |
|        34 | 3071 | `	if( nPos >= nParam \|\| aParam[nPos].nName < 1 ){` |
|       ! 0 | 3072 | `		return 0;` |
|         - | 3073 | `	}` |
|        34 | 3074 | `	if( aParam[nPos].bVariadic ){` |
|         - | 3075 | ``		/* php's get_function_arg_name() answers NULL past `num_args`, which`` |
|         - | 3076 | `		 * counts the non-variadic parameters alone -- so an actual absorbed by` |
|         - | 3077 | ``		 * a `...` tail is named in no diagnostic (`sscanf(): Argument #3 must`` |
|         - | 3078 | ``		 * be passed by reference, value given`, with no ` ($vars)`). */`` |
|       ! 0 | 3079 | `		return 0;` |
|         - | 3080 | `	}` |
|        34 | 3081 | `	SyStringInitFromBuf(pOut,aParam[nPos].zName,(sxu32)aParam[nPos].nName);` |
|        34 | 3082 | `	return 1;` |
|        19 | 3083 | `}` |
|         - | 3084 | `/*` |
|         - | 3085 | ` * Where a NAME binds in a host function's signature, for the screen that refuses it at` |
|         - | 3086 | ` * its send (PH7_OP_NAMED_SEND): the 0-based position of the declared, non-variadic` |
|         - | 3087 | `` * parameter it names, or -1 when it names none. *pbVariadic says whether a `...` tail`` |
|         - | 3088 | ` * would collect an unknown one instead. -2 when the signature cannot be read the way` |
|         - | 3089 | ` * PH7_VmBindNamedArgsToSig reads it, which leaves the name to the call.` |
|         - | 3090 | ` */` |
|       562 | 3091 | `PH7_PRIVATE int PH7_VmSigNamedParam(const char *zSig,const SyString *pName,int *pbVariadic)` |
|         5 | 3092 | `{` |
|         - | 3093 | `	VmSigParam aParam[VM_SIG_MAX_PARAM];` |
|         - | 3094 | `	int nParam,nDecl,p;` |
|       567 | 3095 | `	*pbVariadic = 0;` |
|       567 | 3096 | `	if( zSig == 0 ){` |
|        19 | 3097 | `		return -2;` |
|         - | 3098 | `	}` |
|       549 | 3099 | `	if( zSig[0] == 0 ){` |
|         - | 3100 | ``		/* `time()`: a function declaring no parameter refuses every name at its send,`` |
|         - | 3101 | `		 * where the binder stands down and leaves the call its arity screen. */` |
|        10 | 3102 | `		return -1;` |
|         - | 3103 | `	}` |
|       541 | 3104 | `	nParam = VmSigParams(zSig,aParam,VM_SIG_MAX_PARAM);` |
|       541 | 3105 | `	if( nParam < 1 \|\| nParam > VM_SIG_MAX_PARAM ){` |
|       ! 0 | 3106 | `		return -2;` |
|         - | 3107 | `	}` |
|       541 | 3108 | `	nDecl = aParam[nParam-1].bVariadic ? nParam - 1 : nParam;` |
|       541 | 3109 | `	*pbVariadic = nDecl < nParam;` |
|       941 | 3110 | `	for( p = 0 ; p < nDecl ; ++p ){` |
|       672 | 3111 | `		if( (int)pName->nByte == aParam[p].nName` |
|       502 | 3112 | `		 && SyMemcmp(pName->zString,aParam[p].zName,pName->nByte) == 0 ){` |
|       277 | 3113 | `			return p;` |
|         - | 3114 | `		}` |
|       205 | 3115 | `	}` |
|       269 | 3116 | `	return -1;` |
|       286 | 3117 | `}` |
|         - | 3118 | `/*` |
|         - | 3119 | `` * A `&` in a builtin's signature is not always php's ZEND_SEND_ARG_BY_REF.`` |
|         - | 3120 | ` *` |
|         - | 3121 | ` * php has a second mode, ZEND_SEND_PREFER_REF: bind by reference when the argument IS a` |
|         - | 3122 | ` * variable, and otherwise take it by value without a word. Reflection prints those` |
|         - | 3123 | ` * parameters as by-reference like any other and PHL's signature string cannot say which` |
|         - | 3124 | `` * mode a `&` means, so the two are told apart here. Probed value-for-value against php`` |
|         - | 3125 | `` * 8.5 over every `&` row PHL declares (41 of them): all but extract() refuse a`` |
|         - | 3126 | `` * non-variable, and extract() answers `int(1)` for `extract(['q' => 1])`.`` |
|         - | 3127 | ` *` |
|         - | 3128 | ` * array_multisort() is listed with it because it is php's other prefer-ref builtin and` |
|         - | 3129 | ` * PHL will need this the day it gains one (it is a MISSING builtin today).` |
|         - | 3130 | ` */` |
|    122001 | 3131 | `PH7_PRIVATE int VmBuiltinPrefersRef(SyString *pName)` |
|         5 | 3132 | `{` |
|         - | 3133 | `	static const char *const azPreferRef[] = { "extract", "array_multisort" };` |
|         - | 3134 | `	sxu32 i;` |
|    365768 | 3135 | `	for( i = 0 ; i < SX_ARRAYSIZE(azPreferRef) ; ++i ){` |
|    243905 | 3136 | `		sxu32 nByte = SyStrlen(azPreferRef[i]);` |
|    243900 | 3137 | `		if( pName->nByte == nByte` |
|    121868 | 3138 | `		 && SyMemcmp(pName->zString,azPreferRef[i],nByte) == 0 ){` |
|       143 | 3139 | `			return 1;` |
|         - | 3140 | `		}` |
|    121607 | 3141 | `	}` |
|    121868 | 3142 | `	return 0;` |
|     60866 | 3143 | `}` |
|         - | 3144 | `/*` |
|         - | 3145 | ` * php refuses a by-reference argument at the CALL, before the callee's ZPP runs, and it` |
|         - | 3146 | ``  * decides from the argument's SHAPE, not from its value: `sort([3,1])`, `usort('x',$cb)` `` |
|         - | 3147 | `` * and `preg_match($p,$s,'lit')` are all`` |
|         - | 3148 | `` * `Error: sort(): Argument #1 ($array) could not be passed by reference`.`` |
|         - | 3149 | ` *` |
|         - | 3150 | ` * The call site's compile-time shape mask (VmCallArgMap.nNonLvalMask) is what says so.` |
|         - | 3151 | ` * Only five builtins raised anything before this, from their own bodies, on the runtime` |
|         - | 3152 | `` * `nIdx == SXU32_HIGH` signal — which cannot tell a literal from the result of a call, a`` |
|         - | 3153 | `` * shape php ACCEPTS with a notice. The thirty other `&` rows answered `true`/`false`/an`` |
|         - | 3154 | ` * int: the same answers they give for work they really did.` |
|         - | 3155 | ` *` |
|         - | 3156 | ` * Skipped when the call site has no shape mask (a spread, an indirect dispatch through` |
|         - | 3157 | ` * call_user_func, an engine-synthesized call) or uses named arguments (which rebind` |
|         - | 3158 | ` * positions the mask is indexed by). The by-ref positions come from the same declared` |
|         - | 3159 | ` * signature everything else here reads.` |
|         - | 3160 | ` */` |
|  19221562 | 3161 | `PH7_PRIVATE sxi32 PH7_VmScreenByRefArgShapes(` |
|         - | 3162 | `	ph7_context *pCtx,     /* Call context (for the throw) */` |
|         - | 3163 | `	ph7_user_func *pFunc,  /* Callee: its zSig names and marks the parameters */` |
|         - | 3164 | `	VmCallArgMap *pMap,    /* Call-site map, or 0 */` |
|         - | 3165 | `	int nGiven,            /* Argument count */` |
|         - | 3166 | `	ph7_value **apArg      /* Arguments */` |
|         - | 3167 | `	)` |
|         5 | 3168 | `{` |
|         - | 3169 | `	VmSigParam aParam[VM_SIG_MAX_PARAM];` |
|         - | 3170 | `	int nParam,n;` |
|         - | 3171 | `	/* The by-ref mask first: it is 0 for all but 41 of the ~650 host functions, so` |
|         - | 3172 | `	 * every other call leaves through one test. */` |
|  19221567 | 3173 | `	if( pFunc == 0 \|\| pFunc->nByRefMask == 0 \|\| pFunc->zSig == 0 \|\| nGiven < 1 ){` |
|  19097500 | 3174 | `		return SXRET_OK;` |
|         - | 3175 | `	}` |
|    124072 | 3176 | `	if( pMap == 0 \|\| !pMap->bArgShapes \|\| pMap->bHasNamed ){` |
|       104 | 3177 | `		return SXRET_OK;` |
|         - | 3178 | `	}` |
|    123972 | 3179 | `	if( (pMap->nNonLvalMask \| pMap->nTempCallMask) == 0 ){` |
|      2119 | 3180 | `		return SXRET_OK;` |
|         - | 3181 | `	}` |
|    121858 | 3182 | `	if( VmBuiltinPrefersRef(&pFunc->sName) ){` |
|       129 | 3183 | `		return SXRET_OK;` |
|         - | 3184 | `	}` |
|    121734 | 3185 | `	nParam = VmSigParams(pFunc->zSig,aParam,VM_SIG_MAX_PARAM);` |
|    484273 | 3186 | `	for( n = 0 ; n < nGiven && n < 31 ; ++n ){` |
|    362598 | 3187 | `		if( (pFunc->nByRefMask & (1u << n)) == 0 ){` |
|    324533 | 3188 | `			continue;` |
|         - | 3189 | `		}` |
|     38070 | 3190 | `		if( (pMap->nNonLvalMask & (1u << n)) == 0 ){` |
|         - | 3191 | `			/* Not a refusal — but a CALL result in this position is php's notice,` |
|         - | 3192 | `			 * and then the builtin operates on the temporary. */` |
|     38016 | 3193 | `			PH7_VmArgTempCallNotice(pCtx->pVm,pMap,(sxu32)n,apArg[n]);` |
|     38016 | 3194 | `			continue;` |
|         - | 3195 | `		}` |
|         - | 3196 | `		/* php names the parameter only when the position is a DECLARED one:` |
|         - | 3197 | ``		 * get_function_arg_name() answers NULL past `num_args`, which counts`` |
|         - | 3198 | `` 		 * the non-variadic parameters alone. So an actual absorbed by a `&...` `` |
|         - | 3199 | ``		 * tail (sscanf's `&...$vars`) is refused without a name. */`` |
|        59 | 3200 | `		if( n < nParam && aParam[n].nName > 0 && !aParam[n].bVariadic ){` |
|        83 | 3201 | `			return PH7_VmThrowException(pCtx,"Error",` |
|         - | 3202 | `				"%z(): Argument #%d ($%.*s) could not be passed by reference",` |
|        26 | 3203 | `				&pFunc->sName,n + 1,aParam[n].nName,aParam[n].zName);` |
|         - | 3204 | `		}` |
|         4 | 3205 | `		return PH7_VmThrowException(pCtx,"Error",` |
|         - | 3206 | `			"%z(): Argument #%d could not be passed by reference",` |
|         1 | 3207 | `			&pFunc->sName,n + 1);` |
|       ! 0 | 3208 | `	}` |
|    121680 | 3209 | `	return SXRET_OK;` |
|   9611986 | 3210 | `}` |
|         - | 3211 | `/*` |
|         - | 3212 | ` * D1: derive a by-reference position bitmask from a php-style signature string.` |
|         - | 3213 | `` * Bit N is set when positional parameter N is declared by-reference (a `&` appears`` |
|         - | 3214 | `` * anywhere in that comma-separated parameter, e.g. `&$matches`, `&...$vars`). Only`` |
|         - | 3215 | ` * the first 31 positions are representable; a by-ref parameter past that is rare` |
|         - | 3216 | ` * for a builtin and simply not tracked here. The scan mirrors VmDeriveArityFromSig.` |
|         - | 3217 | ` */` |
|  20386874 | 3218 | `PH7_PRIVATE sxu32 VmDeriveByRefMaskFromSig(const char *zSig)` |
|         5 | 3219 | `{` |
|  20386879 | 3220 | `	sxu32 mask = 0;` |
|  20386879 | 3221 | `	int n = 0;       /* current parameter index */` |
|  20386879 | 3222 | `	int bSeen = 0;   /* current parameter has non-space content */` |
|  20386879 | 3223 | ``	int bRef = 0;    /* current parameter carries a by-ref `&` */`` |
|  20386879 | 3224 | ``	int bVar = 0;    /* current parameter is a `...` variadic */`` |
|  20386879 | 3225 | `	int bTailRef = 0;/* the LAST parameter was a by-ref variadic */` |
|  20386879 | 3226 | `	const char *zCur = zSig;` |
| 235468266 | 3227 | `	for(;;){` |
| 485126342 | 3228 | `		if( zCur[0] == '\'' \|\| zCur[0] == '"' ){` |
|    791000 | 3229 | `			bSeen = 1;` |
|    791000 | 3230 | `			zCur = VmSigSkipQuoted(zCur);` |
|    791000 | 3231 | `			if( zCur[0] != '\0' ){` |
|    791000 | 3232 | `				zCur++;` |
|    394931 | 3233 | `			}` |
|    791000 | 3234 | `			continue;` |
|         - | 3235 | `		}` |
| 484335347 | 3236 | `		if( zCur[0] == '\0' \|\| zCur[0] == ',' ){` |
|  32496874 | 3237 | `			if( bSeen ){` |
|  25453480 | 3238 | `				if( bRef && n < 31 ){` |
|    852816 | 3239 | `					mask \|= (1u << n);` |
|    422253 | 3240 | `				}` |
|  25453480 | 3241 | `				bTailRef = (bRef && bVar);` |
|  25453480 | 3242 | `				n++;` |
|  12690685 | 3243 | `			}` |
|  32496874 | 3244 | `			if( zCur[0] == '\0' ){` |
|  20386879 | 3245 | `				break;` |
|         - | 3246 | `			}` |
|  12110000 | 3247 | `			bSeen = bRef = bVar = 0;` |
|  12110000 | 3248 | `			zCur++;` |
|  12110000 | 3249 | `			continue;` |
|         - | 3250 | `		}` |
| 451838478 | 3251 | `		if( zCur[0] != ' ' ){` |
| 398706277 | 3252 | `			bSeen = 1;` |
| 198819592 | 3253 | `		}` |
| 451838478 | 3254 | `		if( zCur[0] == '&' ){` |
|    852816 | 3255 | `			bRef = 1;` |
|    422253 | 3256 | `		}` |
| 451838478 | 3257 | `		if( zCur[0] == '.' && zCur[1] == '.' && zCur[2] == '.' ){` |
|         - | 3258 | ``			/* A `...` tail, not a numeric default's decimal point. */`` |
|    637015 | 3259 | `			bVar = 1;` |
|    318054 | 3260 | `		}` |
| 451838478 | 3261 | `		zCur++;` |
|         5 | 3262 | `	}` |
|  20386879 | 3263 | `	if( bTailRef && n > 0 && n <= 31 ){` |
|         - | 3264 | ``		/* A by-ref `&...` tail absorbs every later actual (array_multisort's`` |
|         - | 3265 | ``		 * `&...$rest`): without this, the deferred-argument resolver read the`` |
|         - | 3266 | ``		 * tail positions as by-VALUE and warned `Undefined variable` on an`` |
|         - | 3267 | `		 * undefined actual php binds silently. */` |
|     29405 | 3268 | `		mask \|= ~((1u << (n - 1)) - 1u);` |
|     14678 | 3269 | `	}` |
|  20386879 | 3270 | `	return mask;` |
|         5 | 3271 | `}` |
|      6985 | 3272 | `PH7_PRIVATE void VmSetBuiltinSignatures(ph7_vm *pVm)` |
|         5 | 3273 | `{` |
|         - | 3274 | `	sxu32 n;` |
|   7683505 | 3275 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinSig) ; n++ ){` |
|  11508733 | 3276 | `		SyHashEntry *pEntry = SyHashGet(&pVm->hHostFunction,` |
|   7676515 | 3277 | `			(const void *)aBuiltinSig[n].zName,(sxu32)SyStrlen(aBuiltinSig[n].zName));` |
|   7676520 | 3278 | `		if( pEntry ){` |
|   7634643 | 3279 | `			ph7_user_func *pFunc = (ph7_user_func *)pEntry->pUserData;` |
|   7634643 | 3280 | `			sxi16 nMin = 0, nMax = 0;` |
|   7634643 | 3281 | `			sxu8 bAtLeast = 0, bHasMax = 0;` |
|   7634643 | 3282 | `			pFunc->zSig = aBuiltinSig[n].zSig;` |
|   7634643 | 3283 | `			pFunc->zRet = aBuiltinSig[n].zRet[0] ? aBuiltinSig[n].zRet : 0;` |
|   7634643 | 3284 | `			pFunc->nByRefMask = VmDeriveByRefMaskFromSig(pFunc->zSig);` |
|   7634643 | 3285 | `			VmDeriveArityFromSig(pFunc->zSig,&nMin,&bAtLeast,&nMax,&bHasMax);` |
|         - | 3286 | `			/* The MAXIMUM always comes from the signature: the curated override` |
|         - | 3287 | `			 * table speaks only to the minimum (and its wording). */` |
|   7634643 | 3288 | `			pFunc->nMaxArg = nMax;` |
|   7634643 | 3289 | `			pFunc->bHasMaxArg = (sxu8)(VmBuiltinSelfValidatesArity(aBuiltinSig[n].zName) ? 0 : bHasMax);` |
|   7634643 | 3290 | `			if( pFunc->nMinArg < 1 ){` |
|         - | 3291 | `				/* VmSetBuiltinArity() ran first: a non-zero minimum here means the` |
|         - | 3292 | `				 * curated override already spoke for this builtin, so leave it. */` |
|   5518188 | 3293 | `				pFunc->nMinArg = nMin;` |
|   5518188 | 3294 | `				pFunc->bAtLeast = bAtLeast;` |
|   2744269 | 3295 | `			}` |
|   3800830 | 3296 | `		}` |
|   3832218 | 3297 | `	}` |
|      6990 | 3298 | `}` |
|         - | 3299 | `/*` |
|         - | 3300 | ` * Signature lookup by function name, for the reflection layer: embedded-PHP` |
|         - | 3301 | ` * builtins (max/min/Exception methods...) are ph7_vm_func instances, not` |
|         - | 3302 | ` * host functions, so they miss the VmSetBuiltinSignatures stamping and pull` |
|         - | 3303 | ` * their row on demand here. Linear scan — reflection-path only.` |
|         - | 3304 | ` */` |
|       ! 0 | 3305 | `PH7_PRIVATE const char * PH7_VmBuiltinSigLookup(const char *zName,sxu32 nLen,const char **pzRet)` |
|       ! 0 | 3306 | `{` |
|         - | 3307 | `	sxu32 n;` |
|       ! 0 | 3308 | `	if( pzRet ){` |
|       ! 0 | 3309 | `		*pzRet = 0;` |
|       ! 0 | 3310 | `	}` |
|       ! 0 | 3311 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinSig) ; n++ ){` |
|       ! 0 | 3312 | `		if( SyStrlen(aBuiltinSig[n].zName) == nLen` |
|       ! 0 | 3313 | `		 && SyMemcmp(aBuiltinSig[n].zName,zName,nLen) == 0 ){` |
|       ! 0 | 3314 | `			if( pzRet && aBuiltinSig[n].zRet[0] ){` |
|       ! 0 | 3315 | `				*pzRet = aBuiltinSig[n].zRet;` |
|       ! 0 | 3316 | `			}` |
|       ! 0 | 3317 | `			return aBuiltinSig[n].zSig;` |
|         - | 3318 | `		}` |
|       ! 0 | 3319 | `	}` |
|       ! 0 | 3320 | `	return 0;` |
|       ! 0 | 3321 | `}` |
|         - | 3322 | `/*` |
|         - | 3323 | ` * Write a value back to the caller's variable through a builtin argument's` |
|         - | 3324 | ` * stack-slot nIdx — the shared write-back for builtin by-reference` |
|         - | 3325 | ` * out-parameters (preg_match $matches, preg_replace &$count, similar_text` |
|         - | 3326 | ` * &$percent, ...).` |
|         - | 3327 | ` *` |
|         - | 3328 | ` * For a positional out-param argument the call compiler auto-vivifies known` |
|         - | 3329 | ` * by-reference out-params (see GenStateByRefBuiltinMask in compile.c), so a` |
|         - | 3330 | ` * bare undefined variable, an array subscript, and a declared/untyped` |
|         - | 3331 | ` * property all arrive with a real nIdx and are written back here, matching` |
|         - | 3332 | ` * PHP's reference semantics.` |
|         - | 3333 | ` *` |
|         - | 3334 | ` * nIdx stays SXU32_HIGH and the write-back to the caller is skipped (the` |
|         - | 3335 | ` * value still lands in the local stack slot) when the argument cannot expose` |
|         - | 3336 | ` * a stable memobj slot: a literal, a function-call result, a subscript of a` |
|         - | 3337 | ` * non-lvalue parent (foo()['k']), or any variable in a call that also uses` |
|         - | 3338 | ` * named or spread arguments (compile-time positions no longer map to the` |
|         - | 3339 | ` * runtime arg slots, so the compiler conservatively does not vivify). An` |
|         - | 3340 | ` * uninitialized typed property is also not wired (it throws before the` |
|         - | 3341 | ` * write) -- see the recorded deferrals.` |
|         - | 3342 | ` */` |
|     35398 | 3343 | `PH7_PRIVATE void PH7_VmStoreArgByRef(ph7_vm *pVm,ph7_value *pArg,ph7_value *pNewVal)` |
|         5 | 3344 | `{` |
|     35403 | 3345 | `	if( pArg->nIdx != SXU32_HIGH ){` |
|     35309 | 3346 | `		ph7_value *pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pArg->nIdx);` |
|     35309 | 3347 | `		if( pObj ){` |
|     35309 | 3348 | `			PH7_MemObjStore(pNewVal,pObj);` |
|     17649 | 3349 | `		}` |
|     17649 | 3350 | `	}` |
|     35403 | 3351 | `	PH7_MemObjStore(pNewVal,pArg);` |
|     35403 | 3352 | `}` |
|         - | 3353 | `/*` |
|         - | 3354 | ` * Raise an E_WARNING whose text is used VERBATIM.` |
|         - | 3355 | ` * ph7_context_throw_error_format() prepends "func(): " to whatever it is given, but php's` |
|         - | 3356 | ` * IO warnings put the offending path inside those parens -- "file_get_contents(/nope):` |
|         - | 3357 | ` * Failed to open stream: ..." -- so a caller that needs php's exact shape must build the` |
|         - | 3358 | ` * whole line itself and come through here.` |
|         - | 3359 | ` */` |
|         - | 3360 | `/*` |
|         - | 3361 | ` * Validate a callback argument and throw php's TypeError when it cannot be called.` |
|         - | 3362 | ` *` |
|         - | 3363 | ` * The shared dispatcher (PH7_VmCallUserFunctionWithMap) answers SXRET_OK with a NULL` |
|         - | 3364 | ` * result for an unresolvable callable, so every caller that did not check first failed` |
|         - | 3365 | ` * SILENTLY: call_user_func() returned NULL and usort() left the array sorted by nothing` |
|         - | 3366 | ` * at all. php rejects the ARGUMENT up front, naming exactly why.` |
|         - | 3367 | ` */` |
|    403040 | 3368 | `PH7_PRIVATE sxi32 PH7_CheckCallbackArg(` |
|         - | 3369 | `	ph7_context *pCtx,   /* Calling context (names the function in the message) */` |
|         - | 3370 | `	ph7_value *pCb,      /* The callback argument */` |
|         - | 3371 | `	int iArg,            /* Its 1-based position */` |
|         - | 3372 | `	const char *zParam,  /* Its php parameter name ("callback"), or 0 for the variadic` |
|         - | 3373 | `	                      * comparators php names by position only (array_udiff …) */` |
|         - | 3374 | `	int bNullable        /* TRUE when php's text says "or null" */` |
|         - | 3375 | `	)` |
|         5 | 3376 | `{` |
|    403045 | 3377 | `	sxi32 rcDep = PH7_VmCallableDeprecation(pCtx->pVm,pCb);` |
|    403045 | 3378 | `	if( rcDep != PH7_OK ){` |
|        30 | 3379 | `		return rcDep; /* the handler threw on the deprecation: that is the refusal */` |
|         - | 3380 | `	}` |
|    403017 | 3381 | `	return PH7_CheckCallbackReason(pCtx,pCb,iArg,zParam,bNullable);` |
|    201514 | 3382 | `}` |
|         - | 3383 | `/*` |
|         - | 3384 | ` * The same screen, with the scope deprecation already raised by the caller.` |
|         - | 3385 | ` */` |
|    403940 | 3386 | `PH7_PRIVATE sxi32 PH7_CheckCallbackReason(ph7_context *pCtx,ph7_value *pCb,int iArg,` |
|         - | 3387 | `	const char *zParam,int bNullable)` |
|         5 | 3388 | `{` |
|         - | 3389 | `	char zReason[256];` |
|         - | 3390 | `	const char *zWhy;` |
|    403945 | 3391 | `	zWhy = PH7_VmCallableReason(pCtx->pVm,pCb,zReason,sizeof(zReason));` |
|    403945 | 3392 | `	if( zWhy == 0 ){` |
|    403561 | 3393 | `		return PH7_OK;` |
|         - | 3394 | `	}` |
|       389 | 3395 | `	if( zParam ){` |
|       500 | 3396 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3397 | `			"%s(): Argument #%d ($%s) must be a valid callback%s, %s",` |
|       165 | 3398 | `			ph7_function_name(pCtx),iArg,zParam,bNullable ? " or null" : "",zWhy);` |
|         - | 3399 | `	}` |
|        86 | 3400 | `	return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3401 | `		"%s(): Argument #%d must be a valid callback%s, %s",` |
|        27 | 3402 | `		ph7_function_name(pCtx),iArg,bNullable ? " or null" : "",zWhy);` |
|    201964 | 3403 | `}` |
|     34507 | 3404 | `PH7_PRIVATE void PH7_VmThrowWarningFmt(ph7_vm *pVm,const char *zFmt,...)` |
|         5 | 3405 | `{` |
|         - | 3406 | `	va_list ap;` |
|     34512 | 3407 | `	va_start(ap,zFmt);` |
|     34512 | 3408 | `	PH7_VmThrowErrorAp(pVm,0,PH7_CTX_WARNING,zFmt,ap);` |
|     34512 | 3409 | `	va_end(ap);` |
|     34512 | 3410 | `}` |
|         - | 3411 | `/*` |
|         - | 3412 | ` * Emit a formatted E_USER_WARNING with no function-name prefix: php reports` |
|         - | 3413 | ` * #[\NoDiscard] as a USER warning (512) for the same reason it reports` |
|         - | 3414 | ` * #[\Deprecated] as a USER deprecation — the attribute is userland-authored,` |
|         - | 3415 | ` * and a set_error_handler sees the number.` |
|         - | 3416 | ` */` |
|        58 | 3417 | `static void VmThrowUserWarningFmt(ph7_vm *pVm,const char *zFmt,...)` |
|         2 | 3418 | `{` |
|         - | 3419 | `	va_list ap;` |
|        60 | 3420 | `	va_start(ap,zFmt);` |
|        60 | 3421 | `	PH7_VmThrowErrorAp(pVm,0,E_USER_WARNING,zFmt,ap);` |
|        60 | 3422 | `	va_end(ap);` |
|        60 | 3423 | `}` |
|         - | 3424 | `/*` |
|         - | 3425 | ` * Emit a formatted E_USER_DEPRECATED diagnostic with no function-name prefix:` |
|         - | 3426 | ` * php reports #[\Deprecated] as USER-deprecated (16384, it is userland-authored),` |
|         - | 3427 | ` * not the engine's E_DEPRECATED (8192) — handler-visible errno matters.` |
|         - | 3428 | ` */` |
|        38 | 3429 | `static void VmThrowUserDeprecatedFmt(ph7_vm *pVm,const char *zFmt,...)` |
|         2 | 3430 | `{` |
|         - | 3431 | `	va_list ap;` |
|        40 | 3432 | `	va_start(ap,zFmt);` |
|        40 | 3433 | `	PH7_VmThrowErrorAp(pVm,0,E_USER_DEPRECATED,zFmt,ap);` |
|        40 | 3434 | `	va_end(ap);` |
|        40 | 3435 | `}` |
|         - | 3436 | `/*` |
|         - | 3437 | ` * php 8.4 #[\Deprecated]: emit the E_USER_DEPRECATED notice for a call to a user` |
|         - | 3438 | ` * function/method carrying the attribute. Message shapes (php-exact):` |
|         - | 3439 | ` *   Function f() is deprecated` |
|         - | 3440 | ` *   Method C::m() is deprecated since 2.0, use g() instead` |
|         - | 3441 | ` * ("since" from the attribute's since: argument; the trailing ", msg" from` |
|         - | 3442 | ` * message:/positional #1.) The attribute arguments are compiled constant` |
|         - | 3443 | ` * expressions — evaluated via VmLocalExec, the reflection thunks' pattern.` |
|         - | 3444 | ` * Called from OP_CALL once per call, only when the callee HAS attributes;` |
|         - | 3445 | ` * pDeclClass names the method's declaring class (0 for plain functions).` |
|         - | 3446 | ` */` |
|         - | 3447 | `/*` |
|         - | 3448 | ` * Expand a global constant's value, emitting the php 8.5 #[\Deprecated]` |
|         - | 3449 | ` * E_USER_DEPRECATED notice first when the constant carries attributes` |
|         - | 3450 | `` * (`Constant GD is deprecated since 1.2` — every access re-warns).`` |
|         - | 3451 | ` */` |
|         - | 3452 | `static void VmDeprecatedAttrNoticeSubject(ph7_vm *pVm,SySet *pAttrs,` |
|         - | 3453 | `	const char *zKind,const SyString *pQual,const SyString *pName);` |
|         - | 3454 | `/*` |
|         - | 3455 | ` * Expand a constant, emitting the php 8.5 #[\Deprecated] E_USER_DEPRECATED notice` |
|         - | 3456 | `` * first when a USERLAND constant carries the attribute (`Constant GD is deprecated`` |
|         - | 3457 | `` * since 1.2` — every access re-warns). Engine constants php merely deprecates are`` |
|         - | 3458 | ` * not mimicked: PHL removes them outright (see the scope policy), so there is no` |
|         - | 3459 | ` * engine-side E_DEPRECATED list here.` |
|         - | 3460 | ` *` |
|         - | 3461 | `` * Every constant is value-backed by the time it can be read: a `const` statement`` |
|         - | 3462 | ` * evaluates its initializer where it runs, as define() does, so expanding one is a` |
|         - | 3463 | ` * plain copy that cannot throw.` |
|         - | 3464 | ` */` |
|    339252 | 3465 | `PH7_PRIVATE sxi32 VmExpandConstantOnce(ph7_vm *pVm,ph7_constant *pCons,ph7_value *pOut)` |
|         5 | 3466 | `{` |
|    167228 | 3467 | `	SXUNUSED(pVm);` |
|    339257 | 3468 | `	pCons->xExpand(pOut,pCons->pUserData);` |
|    339257 | 3469 | `	return SXRET_OK;` |
|         5 | 3470 | `}` |
|    180780 | 3471 | `PH7_PRIVATE void VmExpandConstantWithNotice(ph7_vm *pVm,ph7_constant *pCons,ph7_value *pOut)` |
|         5 | 3472 | `{` |
|         - | 3473 | `	/* An ENGINE constant php deprecated the symbol of says so when a program` |
|         - | 3474 | `	 * names it -- five of the six were silent here. Listing the table is not` |
|         - | 3475 | `	 * naming one, which is what bConstEnum says. */` |
|    180785 | 3476 | `	if( pCons->zDeprecated && !pVm->bConstEnum ){` |
|        77 | 3477 | `		VmErrorFormat(pVm,8192 /* E_DEPRECATED */,` |
|        25 | 3478 | `			"Constant %z is deprecated since %s",&pCons->sName,pCons->zDeprecated);` |
|        25 | 3479 | `	}` |
|    180785 | 3480 | `	if( SySetUsed(&pCons->aAttrs) > 0 ){` |
|         7 | 3481 | `		VmDeprecatedAttrNoticeSubject(pVm,&pCons->aAttrs,"Constant",0,&pCons->sName);` |
|         3 | 3482 | `	}` |
|    180785 | 3483 | `	VmExpandConstantOnce(pVm,pCons,pOut);` |
|    180785 | 3484 | `}` |
|         - | 3485 | `/*` |
|         - | 3486 | ` * Query a GLOBAL constant by its exact (case-sensitive) name and expand its` |
|         - | 3487 | ` * value into pOut, which the caller has initialized. Returns 1 when the` |
|         - | 3488 | ` * constant exists. The ini scanner's NORMAL/TYPED value interpretation is the` |
|         - | 3489 | ` * caller: php substitutes a defined constant's value for a bare identifier` |
|         - | 3490 | ` * token inside an unquoted ini value.` |
|         - | 3491 | ` */` |
|       610 | 3492 | `PH7_PRIVATE int PH7_VmQueryConstant(ph7_vm *pVm,const char *zName,sxu32 nName,ph7_value *pOut)` |
|         5 | 3493 | `{` |
|         - | 3494 | `	SyHashEntry *pEntry;` |
|         - | 3495 | `	ph7_constant *pCons;` |
|       615 | 3496 | `	pEntry = PH7_VmConstantFetch(pVm,zName,nName,1);` |
|       615 | 3497 | `	if( pEntry == 0 ){` |
|       583 | 3498 | `		return 0;` |
|         - | 3499 | `	}` |
|        36 | 3500 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        36 | 3501 | `	VmExpandConstantWithNotice(pVm,pCons,pOut);` |
|        36 | 3502 | `	return 1;` |
|       310 | 3503 | `}` |
|         - | 3504 | `/*` |
|         - | 3505 | ` * Scan a declared-attribute set for #[\Deprecated]; when found, evaluate its` |
|         - | 3506 | ` * message:/since: arguments (positional #0 = message, #1 = since) into the` |
|         - | 3507 | ` * caller's values and return TRUE. The base subject text ("Function f()",` |
|         - | 3508 | ` * "Constant C::K") is the caller's business.` |
|         - | 3509 | ` */` |
|       410 | 3510 | `static int VmDeprecatedAttrExtract(ph7_vm *pVm,SySet *pAttrs,` |
|         - | 3511 | `	ph7_value *pMsg,int *pbMsg,ph7_value *pSince,int *pbSince)` |
|         5 | 3512 | `{` |
|       415 | 3513 | `	ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|         - | 3514 | `	sxu32 n;` |
|       415 | 3515 | `	*pbMsg = *pbSince = 0;` |
|       791 | 3516 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; ++n ){` |
|       419 | 3517 | `		ph7_attribute *pAttr = &aAttr[n];` |
|         - | 3518 | `		ph7_attr_arg *aArg;` |
|       419 | 3519 | `		sxu32 i,nPos = 0;` |
|       414 | 3520 | `		if( SyStringLength(&pAttr->sName) != sizeof("Deprecated")-1` |
|       231 | 3521 | `		 \|\| SyStrnicmp(SyStringData(&pAttr->sName),"Deprecated",sizeof("Deprecated")-1) != 0 ){` |
|       381 | 3522 | `			continue;` |
|         - | 3523 | `		}` |
|        40 | 3524 | `		aArg = (ph7_attr_arg *)SySetBasePtr(&pAttr->aArgs);` |
|        58 | 3525 | `		for( i = 0 ; i < SySetUsed(&pAttr->aArgs) ; ++i ){` |
|        19 | 3526 | `			ph7_attr_arg *pArg = &aArg[i];` |
|        19 | 3527 | `			int isMsg = 0,isSince = 0;` |
|        19 | 3528 | `			if( SyStringLength(&pArg->sName) == 0 ){` |
|         3 | 3529 | `				isMsg = (nPos == 0);` |
|         3 | 3530 | `				isSince = (nPos == 1);` |
|         3 | 3531 | `				nPos++;` |
|        18 | 3532 | `			}else if( SyStringLength(&pArg->sName) == sizeof("message")-1` |
|        12 | 3533 | `			 && SyMemcmp(SyStringData(&pArg->sName),"message",sizeof("message")-1) == 0 ){` |
|         7 | 3534 | `				isMsg = 1;` |
|        14 | 3535 | `			}else if( SyStringLength(&pArg->sName) == sizeof("since")-1` |
|        11 | 3536 | `			 && SyMemcmp(SyStringData(&pArg->sName),"since",sizeof("since")-1) == 0 ){` |
|        11 | 3537 | `				isSince = 1;` |
|         5 | 3538 | `			}` |
|        19 | 3539 | `			if( isMsg && !*pbMsg && SySetUsed(&pArg->aByteCode) > 0 ){` |
|        13 | 3540 | `				if( VmLocalExec(pVm,&pArg->aByteCode,pMsg,FALSE) == SXRET_OK ){` |
|         9 | 3541 | `					if( (pMsg->iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 3542 | `						PH7_MemObjToString(pMsg);` |
|       ! 0 | 3543 | `					}` |
|         9 | 3544 | `					*pbMsg = 1;` |
|         5 | 3545 | `				}` |
|        15 | 3546 | `			}else if( isSince && !*pbSince && SySetUsed(&pArg->aByteCode) > 0 ){` |
|        11 | 3547 | `				if( VmLocalExec(pVm,&pArg->aByteCode,pSince,FALSE) == SXRET_OK ){` |
|        11 | 3548 | `					if( (pSince->iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 3549 | `						PH7_MemObjToString(pSince);` |
|       ! 0 | 3550 | `					}` |
|        11 | 3551 | `					*pbSince = 1;` |
|         5 | 3552 | `				}` |
|         5 | 3553 | `			}` |
|        10 | 3554 | `		}` |
|        40 | 3555 | `		return 1;` |
|       ! 0 | 3556 | `	}` |
|       377 | 3557 | `	return 0;` |
|       210 | 3558 | `}` |
|         - | 3559 | `/*` |
|         - | 3560 | ` * Append php's " since X" / ", message" suffixes to a built base subject and` |
|         - | 3561 | ` * emit the E_USER_DEPRECATED notice.` |
|         - | 3562 | ` */` |
|        38 | 3563 | `static void VmDeprecatedEmit(ph7_vm *pVm,SyBlob *pOut,` |
|         - | 3564 | `	ph7_value *pMsg,int bMsg,ph7_value *pSince,int bSince)` |
|         2 | 3565 | `{` |
|        40 | 3566 | `	if( bSince && SyBlobLength(&pSince->sBlob) > 0 ){` |
|        16 | 3567 | `		SyBlobFormat(pOut," since %.*s",(int)SyBlobLength(&pSince->sBlob),` |
|        10 | 3568 | `			(const char *)SyBlobData(&pSince->sBlob));` |
|         5 | 3569 | `	}` |
|        40 | 3570 | `	if( bMsg && SyBlobLength(&pMsg->sBlob) > 0 ){` |
|        13 | 3571 | `		SyBlobFormat(pOut,", %.*s",(int)SyBlobLength(&pMsg->sBlob),` |
|         8 | 3572 | `			(const char *)SyBlobData(&pMsg->sBlob));` |
|         4 | 3573 | `	}` |
|        40 | 3574 | `	VmThrowUserDeprecatedFmt(pVm,"%.*s",(int)SyBlobLength(pOut),(const char *)SyBlobData(pOut));` |
|        40 | 3575 | `}` |
|         - | 3576 | `/*` |
|         - | 3577 | ` * Generic #[\Deprecated] notice for a named subject:` |
|         - | 3578 | ` * "<Kind> [Qual::]Name is deprecated[ since X][, message]".` |
|         - | 3579 | ` */` |
|        20 | 3580 | `static void VmDeprecatedAttrNoticeSubject(ph7_vm *pVm,SySet *pAttrs,` |
|         - | 3581 | `	const char *zKind,const SyString *pQual,const SyString *pName)` |
|         2 | 3582 | `{` |
|         - | 3583 | `	ph7_value sMsg,sSince;` |
|         - | 3584 | `	SyBlob sOut;` |
|         - | 3585 | `	int bMsg,bSince;` |
|        22 | 3586 | `	PH7_MemObjInit(pVm,&sMsg);` |
|        22 | 3587 | `	PH7_MemObjInit(pVm,&sSince);` |
|        22 | 3588 | `	if( VmDeprecatedAttrExtract(pVm,pAttrs,&sMsg,&bMsg,&sSince,&bSince) ){` |
|        18 | 3589 | `		SyBlobInit(&sOut,&pVm->sAllocator);` |
|        18 | 3590 | `		if( pQual ){` |
|        14 | 3591 | `			SyBlobFormat(&sOut,"%s %z::%z is deprecated",zKind,pQual,pName);` |
|         8 | 3592 | `		}else{` |
|         5 | 3593 | `			SyBlobFormat(&sOut,"%s %z is deprecated",zKind,pName);` |
|         - | 3594 | `		}` |
|        18 | 3595 | `		VmDeprecatedEmit(pVm,&sOut,&sMsg,bMsg,&sSince,bSince);` |
|        18 | 3596 | `		SyBlobRelease(&sOut);` |
|         8 | 3597 | `	}` |
|        22 | 3598 | `	PH7_MemObjRelease(&sMsg);` |
|        22 | 3599 | `	PH7_MemObjRelease(&sSince);` |
|        22 | 3600 | `}` |
|         - | 3601 | `/*` |
|         - | 3602 | ` * The functions and methods php 8.x deprecated, and the clause each notice ends` |
|         - | 3603 | `` * with. Calling one raises `Function f() is deprecated since <clause>` (or`` |
|         - | 3604 | `` * `Method C::m() ...`) at E_DEPRECATED, BEFORE the callee's arity and type`` |
|         - | 3605 | `` * screens -- `curl_close()` with no argument warns first and throws the`` |
|         - | 3606 | ` * ArgumentCountError second -- and the export format's head reads the same fact` |
|         - | 3607 | `` * as `<internal, deprecated:curl>`.`` |
|         - | 3608 | ` *` |
|         - | 3609 | ` * Marked here rather than raised from each body because the notice belongs to` |
|         - | 3610 | ` * the CALL and not to what the body does (php warns and then runs it), and` |
|         - | 3611 | ` * because the subject is php's own: it names the DECLARING class even for a` |
|         - | 3612 | `` * call through a subclass, so a `MyStore extends SplObjectStorage` still reads`` |
|         - | 3613 | `` * `SplObjectStorage::attach()`. The stamp runs once, after every extension has`` |
|         - | 3614 | ` * installed, so a name a build does not carry is simply skipped.` |
|         - | 3615 | ` *` |
|         - | 3616 | ` * Only names this engine SHIPS are listed; php's own deprecated set is larger` |
|         - | 3617 | ` * (strftime, utf8_encode, the whole mhash family) and every one of those is a` |
|         - | 3618 | ` * name PHL does not have.` |
|         - | 3619 | ` */` |
|         - | 3620 | `static const ph7_deprecated_name aDeprecatedFunc[] = {` |
|         - | 3621 | `	{ "curl_close",        "8.5, as it has no effect since PHP 8.0" },` |
|         - | 3622 | `	/* Deprecated together in 8.1: both are the single-answer face of the one` |
|         - | 3623 | `	 * call that returns all nine. */` |
|         - | 3624 | `	{ "date_sunrise",      "8.1, use date_sun_info() instead" },` |
|         - | 3625 | `	{ "date_sunset",       "8.1, use date_sun_info() instead" },` |
|         - | 3626 | `	/* ext/zip's whole procedural half, deprecated together in 8.0. php names a` |
|         - | 3627 | `	 * replacement for seven of the ten and none for the other three. */` |
|         - | 3628 | `	{ "zip_open",          "8.0, use ZipArchive::open() instead" },` |
|         - | 3629 | `	{ "zip_close",         "8.0, use ZipArchive::close() instead" },` |
|         - | 3630 | `	{ "zip_read",          "8.0, use ZipArchive::statIndex() instead" },` |
|         - | 3631 | `	{ "zip_entry_open",    "8.0" },` |
|         - | 3632 | `	{ "zip_entry_close",   "8.0" },` |
|         - | 3633 | `	{ "zip_entry_read",    "8.0, use ZipArchive::getFromIndex() instead" },` |
|         - | 3634 | `	{ "zip_entry_name",    "8.0, use ZipArchive::statIndex() instead" },` |
|         - | 3635 | `	{ "zip_entry_compressedsize", "8.0, use ZipArchive::statIndex() instead" },` |
|         - | 3636 | `	{ "zip_entry_filesize", "8.0, use ZipArchive::statIndex() instead" },` |
|         - | 3637 | `	{ "zip_entry_compressionmethod", "8.0, use ZipArchive::statIndex() instead" },` |
|         - | 3638 | `	{ "finfo_close",       "8.5, as finfo objects are freed automatically" },` |
|         - | 3639 | `	{ "curl_share_close",  "8.5, as it has no effect since PHP 8.0" },` |
|         - | 3640 | `	{ "DateInterval::__wakeup",` |
|         - | 3641 | `	  "8.5, this method is obsolete, as serialization hooks are provided by "` |
|         - | 3642 | `	  "__unserialize() and __serialize()" },` |
|         - | 3643 | `	{ "DatePeriod::__wakeup",` |
|         - | 3644 | `	  "8.5, this method is obsolete, as serialization hooks are provided by "` |
|         - | 3645 | `	  "__unserialize() and __serialize()" },` |
|         - | 3646 | `	{ "DateTime::__wakeup",` |
|         - | 3647 | `	  "8.5, this method is obsolete, as serialization hooks are provided by "` |
|         - | 3648 | `	  "__unserialize() and __serialize()" },` |
|         - | 3649 | `	{ "DateTimeInterface::__wakeup",` |
|         - | 3650 | `	  "8.5, this method is obsolete, as serialization hooks are provided by "` |
|         - | 3651 | `	  "__unserialize() and __serialize()" },` |
|         - | 3652 | `	{ "DateTimeImmutable::__wakeup",` |
|         - | 3653 | `	  "8.5, this method is obsolete, as serialization hooks are provided by "` |
|         - | 3654 | `	  "__unserialize() and __serialize()" },` |
|         - | 3655 | `	{ "DateTimeZone::__wakeup",` |
|         - | 3656 | `	  "8.5, this method is obsolete, as serialization hooks are provided by "` |
|         - | 3657 | `	  "__unserialize() and __serialize()" },` |
|         - | 3658 | `	{ "SplFixedArray::__wakeup",` |
|         - | 3659 | `	  "8.4, this method is obsolete, as serialization hooks are provided by "` |
|         - | 3660 | `	  "__unserialize() and __serialize()" },` |
|         - | 3661 | `	{ "SplFileInfo::_bad_state_ex", "8.2" },` |
|         - | 3662 | `	{ "SplObjectStorage::attach",` |
|         - | 3663 | `	  "8.5, use method SplObjectStorage::offsetSet() instead" },` |
|         - | 3664 | `	{ "SplObjectStorage::contains",` |
|         - | 3665 | `	  "8.5, use method SplObjectStorage::offsetExists() instead" },` |
|         - | 3666 | `	{ "SplObjectStorage::detach",` |
|         - | 3667 | `	  "8.5, use method SplObjectStorage::offsetUnset() instead" },` |
|         - | 3668 | `	{ "ReflectionFunction::isDisabled",` |
|         - | 3669 | `	  "8.0, as ReflectionFunction can no longer be constructed for disabled functions" },` |
|         - | 3670 | `	{ "ReflectionMethod::setAccessible", "8.5, as it has no effect since PHP 8.1" },` |
|         - | 3671 | `	{ "ReflectionProperty::setAccessible", "8.5, as it has no effect since PHP 8.1" },` |
|         - | 3672 | `	{ "ReflectionParameter::getClass",` |
|         - | 3673 | `	  "8.0, use ReflectionParameter::getType() instead" },` |
|         - | 3674 | `	{ "ReflectionParameter::isArray",` |
|         - | 3675 | `	  "8.0, use ReflectionParameter::getType() instead" },` |
|         - | 3676 | `	{ "ReflectionParameter::isCallable",` |
|         - | 3677 | `	  "8.0, use ReflectionParameter::getType() instead" },` |
|         - | 3678 | `};` |
|         - | 3679 | `/* The ph7_user_func behind one entry: a global builtin, or a native method's` |
|         - | 3680 | ` * own C body reached through its ph7_vm_func. */` |
|    223520 | 3681 | `static ph7_user_func * VmDeprecatedTarget(ph7_vm *pVm,const char *zName)` |
|         5 | 3682 | `{` |
|    223525 | 3683 | `	const char *zSep = 0;` |
|         - | 3684 | `	SyHashEntry *pEntry;` |
|         - | 3685 | `	sxu32 n;` |
|   3492505 | 3686 | `	for( n = 0 ; zName[n] != '\0' ; ++n ){` |
|   3387730 | 3687 | `		if( zName[n] == ':' && zName[n+1] == ':' ){` |
|    118750 | 3688 | `			zSep = &zName[n];` |
|    118750 | 3689 | `			break;` |
|         - | 3690 | `		}` |
|   1631921 | 3691 | `	}` |
|    223525 | 3692 | `	if( zSep == 0 ){` |
|    104780 | 3693 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zName,SyStrlen(zName));` |
|    104780 | 3694 | `		return pEntry ? (ph7_user_func *)pEntry->pUserData : 0;` |
|         - | 3695 | `	}` |
|         - | 3696 | `	{` |
|    118750 | 3697 | `		ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zName,(sxu32)(zSep - zName),FALSE,0);` |
|         - | 3698 | `		ph7_class_method *pMeth;` |
|    118750 | 3699 | `		if( pClass == 0 ){` |
|       ! 0 | 3700 | `			return 0;` |
|         - | 3701 | `		}` |
|    118750 | 3702 | `		pEntry = SyHashGet(&pClass->hMethod,(const void *)(zSep + 2),SyStrlen(zSep + 2));` |
|    118750 | 3703 | `		if( pEntry == 0 ){` |
|         2 | 3704 | `			return 0;` |
|         - | 3705 | `		}` |
|    118748 | 3706 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|    118748 | 3707 | `		return (pMeth->sFunc.iFlags & VM_FUNC_NATIVE) ? pMeth->sFunc.pNative : 0;` |
|         - | 3708 | `	}` |
|    111589 | 3709 | `}` |
|      6985 | 3710 | `PH7_PRIVATE void PH7_MarkDeprecatedFunctions(ph7_vm *pVm)` |
|         5 | 3711 | `{` |
|         - | 3712 | `	sxu32 n;` |
|    230510 | 3713 | `	for( n = 0 ; n < SX_ARRAYSIZE(aDeprecatedFunc) ; ++n ){` |
|    223525 | 3714 | `		ph7_user_func *pTarget = VmDeprecatedTarget(&(*pVm),aDeprecatedFunc[n].zName);` |
|    223525 | 3715 | `		if( pTarget ){` |
|    223523 | 3716 | `			pTarget->pDeprecated = &aDeprecatedFunc[n];` |
|    111583 | 3717 | `		}` |
|    111589 | 3718 | `	}` |
|      6990 | 3719 | `}` |
|         - | 3720 | `/*` |
|         - | 3721 | ` * The notice itself, raised from the one OP_CALL block a builtin and a native` |
|         - | 3722 | ` * method share. php words a qualified subject as a METHOD and a bare one as a` |
|         - | 3723 | ` * FUNCTION, which is exactly what the "::" in the recorded name says.` |
|         - | 3724 | ` */` |
|       272 | 3725 | `PH7_PRIVATE void PH7_VmDeprecatedCallNotice(ph7_vm *pVm,const ph7_deprecated_name *pDep)` |
|         3 | 3726 | `{` |
|       275 | 3727 | `	int bMethod = 0;` |
|         - | 3728 | `	sxu32 n;` |
|      3849 | 3729 | `	for( n = 0 ; pDep->zName[n] != '\0' ; ++n ){` |
|      3625 | 3730 | `		if( pDep->zName[n] == ':' ){` |
|        49 | 3731 | `			bMethod = 1;` |
|        49 | 3732 | `			break;` |
|         - | 3733 | `		}` |
|      1790 | 3734 | `	}` |
|       411 | 3735 | `	VmErrorFormat(&(*pVm),8192 /* E_DEPRECATED */,"%s %s() is deprecated since %s",` |
|       272 | 3736 | `		bMethod ? "Method" : "Function",pDep->zName,pDep->zWhy);` |
|       275 | 3737 | `}` |
|       390 | 3738 | `PH7_PRIVATE void VmDeprecatedAttrNotice(ph7_vm *pVm,ph7_vm_func *pFunc,ph7_class *pDeclClass)` |
|         5 | 3739 | `{` |
|         - | 3740 | `	ph7_value sMsg,sSince;` |
|         - | 3741 | `	SyBlob sOut;` |
|         - | 3742 | `	int bMsg,bSince;` |
|       395 | 3743 | `	PH7_MemObjInit(pVm,&sMsg);` |
|       395 | 3744 | `	PH7_MemObjInit(pVm,&sSince);` |
|       395 | 3745 | `	if( VmDeprecatedAttrExtract(pVm,&pFunc->aAttrs,&sMsg,&bMsg,&sSince,&bSince) ){` |
|        24 | 3746 | `		SyBlobInit(&sOut,&pVm->sAllocator);` |
|        24 | 3747 | `		if( pDeclClass ){` |
|         5 | 3748 | `			SyBlobFormat(&sOut,"Method %z::%z() is deprecated",&pDeclClass->sDisp,&pFunc->sName);` |
|         3 | 3749 | `		}else{` |
|        20 | 3750 | `			SyBlobFormat(&sOut,"Function %z() is deprecated",&pFunc->sName);` |
|         - | 3751 | `		}` |
|        24 | 3752 | `		VmDeprecatedEmit(pVm,&sOut,&sMsg,bMsg,&sSince,bSince);` |
|        24 | 3753 | `		SyBlobRelease(&sOut);` |
|        11 | 3754 | `	}` |
|       395 | 3755 | `	PH7_MemObjRelease(&sMsg);` |
|       395 | 3756 | `	PH7_MemObjRelease(&sSince);` |
|       395 | 3757 | `}` |
|         - | 3758 | `/*` |
|         - | 3759 | ` * php 8.5's #[\NoDiscard] warning, raised at the CALL, before the body runs, and` |
|         - | 3760 | ` * once per call (a loop warns every time round).` |
|         - | 3761 | ` *` |
|         - | 3762 | ` * The subject is a "function" unless the callee has a class scope, in which case` |
|         - | 3763 | ` * php names the DECLARING class -- an inherited method reports the class that` |
|         - | 3764 | `` * wrote it, and so does `parent::m()`. The tail after php's sentence is the`` |
|         - | 3765 | ` * attribute's own message: a constant EXPRESSION for a compiled declaration` |
|         - | 3766 | ` * (evaluated here, where the constants it may name exist) and a fixed string for` |
|         - | 3767 | ` * an internal member, which is how php words the immutable date mutators.` |
|         - | 3768 | ` */` |
|        58 | 3769 | `PH7_PRIVATE void VmNoDiscardWarn(ph7_vm *pVm,ph7_vm_func *pFunc,ph7_class *pDeclClass)` |
|         2 | 3770 | `{` |
|        60 | 3771 | `	ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(&pFunc->aAttrs);` |
|        60 | 3772 | `	const SyString *pName = &pFunc->sName;` |
|         - | 3773 | `	ph7_value sMsg;` |
|         - | 3774 | `	SyBlob sOut;` |
|        60 | 3775 | `	int bMsg = 0;` |
|         - | 3776 | `	sxu32 n;` |
|         - | 3777 | ``	/* A closure reports php's `{closure:SCOPE:LINE}` spelling, like every other`` |
|         - | 3778 | `	 * diagnostic that names one. */` |
|        60 | 3779 | `	if( SyStringLength(&pFunc->sClosureName) > 0 ){` |
|         7 | 3780 | `		pName = &pFunc->sClosureName;` |
|         3 | 3781 | `	}` |
|        60 | 3782 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|        60 | 3783 | `	if( pDeclClass ){` |
|        23 | 3784 | `		SyBlobFormat(&sOut,"The return value of method %z::%z() should either be used "` |
|        11 | 3785 | `			"or intentionally ignored by casting it as (void)",&pDeclClass->sDisp,pName);` |
|        12 | 3786 | `	}else{` |
|        38 | 3787 | `		SyBlobFormat(&sOut,"The return value of function %z() should either be used "` |
|        18 | 3788 | `			"or intentionally ignored by casting it as (void)",pName);` |
|         - | 3789 | `	}` |
|        60 | 3790 | `	PH7_MemObjInit(pVm,&sMsg);` |
|        60 | 3791 | `for( n = 0 ; !bMsg && n < SySetUsed(&pFunc->aAttrs) ; ++n ){` |
|         - | 3792 | `		ph7_attr_arg *aArg;` |
|        60 | 3793 | `		sxu32 i,nPos = 0;` |
|        58 | 3794 | `		if( SyStringLength(&aAttr[n].sName) != sizeof("NoDiscard")-1` |
|        60 | 3795 | `		 \|\| SyStrnicmp(SyStringData(&aAttr[n].sName),"NoDiscard",` |
|        29 | 3796 | `				sizeof("NoDiscard")-1) != 0 ){` |
|       ! 0 | 3797 | `			continue;` |
|         - | 3798 | `		}` |
|        60 | 3799 | `		aArg = (ph7_attr_arg *)SySetBasePtr(&aAttr[n].aArgs);` |
|        60 | 3800 | `		for( i = 0 ; i < SySetUsed(&aAttr[n].aArgs) ; ++i ){` |
|        11 | 3801 | `			ph7_attr_arg *pArg = &aArg[i];` |
|         - | 3802 | `			int isMsg;` |
|        11 | 3803 | `			if( SyStringLength(&pArg->sName) == 0 ){` |
|         7 | 3804 | `				isMsg = (nPos == 0);` |
|         7 | 3805 | `				nPos++;` |
|         4 | 3806 | `			}else{` |
|         7 | 3807 | `				isMsg = SyStringLength(&pArg->sName) == sizeof("message")-1` |
|         4 | 3808 | `					&& SyMemcmp(SyStringData(&pArg->sName),"message",` |
|         2 | 3809 | `						sizeof("message")-1) == 0;` |
|         - | 3810 | `			}` |
|        11 | 3811 | `			if( !isMsg ){` |
|       ! 0 | 3812 | `				continue;` |
|         - | 3813 | `			}` |
|         - | 3814 | `			/* A compiled declaration holds the message as a constant` |
|         - | 3815 | `			 * EXPRESSION (evaluated here, where the constants it may name` |
|         - | 3816 | `			 * exist); a native one holds it as a literal, like every other` |
|         - | 3817 | `			 * attribute argument a C-declared class carries. */` |
|        11 | 3818 | `			if( SySetUsed(&pArg->aByteCode) > 0 ){` |
|         9 | 3819 | `				if( VmLocalExec(pVm,&pArg->aByteCode,&sMsg,FALSE) != SXRET_OK ){` |
|       ! 0 | 3820 | `					continue;` |
|         1 | 3821 | `				}` |
|         7 | 3822 | `			}else if( pArg->pNativeValue ){` |
|         3 | 3823 | `				PH7_NativeLiteralValue(pVm,pArg->pNativeValue,&sMsg);` |
|         2 | 3824 | `			}else{` |
|       ! 0 | 3825 | `				continue;` |
|         - | 3826 | `			}` |
|        11 | 3827 | `			if( (sMsg.iFlags & MEMOBJ_STRING) == 0 ){` |
|         3 | 3828 | `				PH7_MemObjToString(&sMsg);` |
|         1 | 3829 | `			}` |
|        11 | 3830 | `			bMsg = 1;` |
|        11 | 3831 | `			break;` |
|       ! 0 | 3832 | `		}` |
|        60 | 3833 | `		break;` |
|       ! 0 | 3834 | `	}` |
|        60 | 3835 | `	if( bMsg && SyBlobLength(&sMsg.sBlob) > 0 ){` |
|        13 | 3836 | `		SyBlobFormat(&sOut,", %.*s",(int)SyBlobLength(&sMsg.sBlob),` |
|         8 | 3837 | `			(const char *)SyBlobData(&sMsg.sBlob));` |
|         4 | 3838 | `	}` |
|        60 | 3839 | `	PH7_MemObjRelease(&sMsg);` |
|         - | 3840 | `	/* php raises it as E_USER_WARNING (512), not the engine's E_WARNING: the` |
|         - | 3841 | `	 * attribute is userland-authored, the same reason #[\Deprecated] is` |
|         - | 3842 | `	 * E_USER_DEPRECATED. A set_error_handler sees the number. */` |
|        89 | 3843 | `	VmThrowUserWarningFmt(pVm,"%.*s",` |
|        58 | 3844 | `		(int)SyBlobLength(&sOut),(const char *)SyBlobData(&sOut));` |
|        60 | 3845 | `	SyBlobRelease(&sOut);` |
|        60 | 3846 | `}` |
|         - | 3847 | `/*` |
|         - | 3848 | ` * Same notice for a #[\Deprecated] class constant, at its static-access site:` |
|         - | 3849 | `` * php's `Constant C::K is deprecated` (each access re-warns).`` |
|         - | 3850 | ` */` |
|        14 | 3851 | `PH7_PRIVATE void VmDeprecatedConstNotice(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pMember)` |
|         2 | 3852 | `{` |
|        23 | 3853 | `	VmDeprecatedAttrNoticeSubject(pVm,&pMember->aAttrs,` |
|        14 | 3854 | `		(pMember->iFlags & PH7_CLASS_ATTR_ENUMCASE) ? "Enum case" : "Constant",` |
|        14 | 3855 | `		&pClass->sName,&pMember->sName);` |
|        16 | 3856 | `}` |
|         - | 3857 | `/*` |
|         - | 3858 | `` * The USER-VISIBLE string coercion a builtin performs on a `mixed` argument or`` |
|         - | 3859 | ` * array element: the builtin-side twin of PH7_MemObjToStringUV's opcode sites.` |
|         - | 3860 | ` * An ARRAY warns "Array to string conversion" and still renders as "Array"; an` |
|         - | 3861 | ` * object whose class has no __toString() -- or one whose __toString() threw --` |
|         - | 3862 | ` * is php's catchable "could not be converted to string" Error, and the builtin` |
|         - | 3863 | ` * must answer that instead of a value.` |
|         - | 3864 | ` *` |
|         - | 3865 | ` * On success pzData and pnLen receive the NUL-terminated bytes (both optional).` |
|         - | 3866 | ` * On a throw they are set to the empty string and the status is returned AND` |
|         - | 3867 | ` * recorded on the call context, so OP_CALL cannot mistake the call for a normal` |
|         - | 3868 | ` * return; a builtin that has already produced output (printf) still keeps it,` |
|         - | 3869 | ` * which is what php does.` |
|         - | 3870 | ` *` |
|         - | 3871 | ` * The public ph7_value_to_string() stays SILENT on purpose: it is the embedder` |
|         - | 3872 | ` * API, and the INTERNAL coercions behind it (array-key canonicalisation, sort` |
|         - | 3873 | ` * comparisons, print_r/var_export/serialize) must not throw -- php's do not` |
|         - | 3874 | ` * either.` |
|         - | 3875 | ` */` |
|    855359 | 3876 | `PH7_PRIVATE sxi32 PH7_ValueToStringUV(ph7_context *pCtx,ph7_value *pValue,const char **pzData,int *pnLen)` |
|         5 | 3877 | `{` |
|    855364 | 3878 | `	sxi32 rc = PH7_MemObjToStringUV(pValue);` |
|    855364 | 3879 | `	if( rc != SXRET_OK ){` |
|        75 | 3880 | `		if( pCtx ){` |
|        75 | 3881 | `			pCtx->nThrowRc = rc;` |
|        35 | 3882 | `		}` |
|        75 | 3883 | `		if( pzData ){` |
|        71 | 3884 | `			*pzData = "";` |
|        33 | 3885 | `		}` |
|        75 | 3886 | `		if( pnLen ){` |
|        71 | 3887 | `			*pnLen = 0;` |
|        33 | 3888 | `		}` |
|        75 | 3889 | `		return rc;` |
|         - | 3890 | `	}` |
|    855292 | 3891 | `	if( pzData \|\| pnLen ){` |
|    855264 | 3892 | `		const char *zData = ph7_value_to_string(pValue,pnLen);` |
|    855264 | 3893 | `		if( pzData ){` |
|    855264 | 3894 | `			*pzData = zData;` |
|    426866 | 3895 | `		}` |
|    426866 | 3896 | `	}` |
|    855292 | 3897 | `	return SXRET_OK;` |
|    426920 | 3898 | `}` |
|         - | 3899 | `/*` |
|         - | 3900 | ` * The same user-visible coercion for a builtin that php does NOT stop for.` |
|         - | 3901 | ` * zend's zval_get_string leaves the empty string behind when it throws and the` |
|         - | 3902 | `` * C function carries on, so `str_replace()`'s `&$count` still comes back written`` |
|         - | 3903 | ` * from a call that threw. Only the FIRST un-stringable value raises -- a second` |
|         - | 3904 | ` * one would land two Errors for one call -- while every OTHER kind of value is` |
|         - | 3905 | ` * converted normally either way (an array still warns, a scalar still spells` |
|         - | 3906 | ` * itself out), which is what keeps the elements AFTER the failure intact.` |
|         - | 3907 | ` *` |
|         - | 3908 | ` * pzData/pnLen always come back usable, so the caller has nothing to check.` |
|         - | 3909 | ` */` |
|    246421 | 3910 | `PH7_PRIVATE void PH7_ValueToStringUVOnce(ph7_context *pCtx,ph7_value *pValue,` |
|         - | 3911 | `	const char **pzData,int *pnLen)` |
|         5 | 3912 | `{` |
|    246426 | 3913 | `	if( pCtx && pCtx->nThrowRc != 0 && PH7_MemObjIsNotStringable(pValue) ){` |
|       ! 0 | 3914 | `		if( pzData ){ *pzData = ""; }` |
|       ! 0 | 3915 | `		if( pnLen ){ *pnLen = 0; }` |
|       ! 0 | 3916 | `		return;` |
|         - | 3917 | `	}` |
|    246426 | 3918 | `	(void)PH7_ValueToStringUV(pCtx,pValue,pzData,pnLen);` |
|    122855 | 3919 | `}` |
|         - | 3920 | `/*` |
|         - | 3921 | ` * The same coercion again, for a builtin that must FINISH ITS OUTPUT before the` |
|         - | 3922 | ` * Error is raised. php's C functions carry on past the throw and the bytes they` |
|         - | 3923 | ` * write reach the stream BEFORE the exception surfaces:` |
|         - | 3924 | ``  * `file_put_contents($f,['A',$obj,'B'])` leaves "AB" behind and `fputcsv()` `` |
|         - | 3925 | ` * writes its whole line with an empty field. A throw raised from inside a` |
|         - | 3926 | ` * builtin HERE runs the enclosing catch immediately, so raising in place would` |
|         - | 3927 | ` * put the catch's own output in front of the builtin's.` |
|         - | 3928 | ` *` |
|         - | 3929 | ` * Answers the class that could not be converted (and the empty string with it),` |
|         - | 3930 | ` * leaving the caller to raise once its writing is done; 0 when the value` |
|         - | 3931 | ` * converted, which is every other kind -- an array still warns here, in place,` |
|         - | 3932 | ` * exactly as php's does.` |
|         - | 3933 | ` */` |
|     21522 | 3934 | `PH7_PRIVATE ph7_class *PH7_ValueToStringUVDefer(ph7_context *pCtx,ph7_value *pValue,` |
|         - | 3935 | `	const char **pzData,int *pnLen)` |
|         5 | 3936 | `{` |
|     21527 | 3937 | `	if( PH7_MemObjIsNotStringable(pValue) ){` |
|         7 | 3938 | `		ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|         7 | 3939 | `		if( pzData ){ *pzData = ""; }` |
|         7 | 3940 | `		if( pnLen ){ *pnLen = 0; }` |
|         7 | 3941 | `		return pInst ? pInst->pClass : 0;` |
|         - | 3942 | `	}` |
|     21521 | 3943 | `	(void)PH7_ValueToStringUV(pCtx,pValue,pzData,pnLen);` |
|     21521 | 3944 | `	return 0;` |
|     10761 | 3945 | `}` |
|         - | 3946 |  |
