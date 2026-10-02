# src/ph7/vm_arg_check.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1035/1088 lines (95.13%)

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
|         - |  169 | `	{ "floatval",                  1, 0 },` |
|         - |  170 | `	{ "get_resource_id",           1, 0 },` |
|         - |  171 | `	{ "get_resource_type",         1, 0 },` |
|         - |  172 | `	{ "gettype",                   1, 0 },` |
|         - |  173 | `	{ "intval",                    1, 1 },` |
|         - |  174 | `	{ "is_array",                  1, 0 },` |
|         - |  175 | `	{ "is_bool",                   1, 0 },` |
|         - |  176 | `	{ "is_callable",               1, 1 },` |
|         - |  177 | `	{ "is_double",                 1, 0 },` |
|         - |  178 | `	{ "is_float",                  1, 0 },` |
|         - |  179 | `	{ "is_int",                    1, 0 },` |
|         - |  180 | `	{ "is_integer",                1, 0 },` |
|         - |  181 | `	{ "is_long",                   1, 0 },` |
|         - |  182 | `	{ "is_null",                   1, 0 },` |
|         - |  183 | `	{ "is_numeric",                1, 0 },` |
|         - |  184 | `	{ "is_object",                 1, 0 },` |
|         - |  185 | `	{ "is_resource",               1, 0 },` |
|         - |  186 | `	{ "is_scalar",                 1, 0 },` |
|         - |  187 | `	{ "is_string",                 1, 0 },` |
|         - |  188 | `	{ "print_r",                   1, 1 },` |
|         - |  189 | `	{ "strval",                    1, 0 },` |
|         - |  190 | `	{ "var_dump",                  1, 1 },` |
|         - |  191 | `	{ "var_export",                1, 1 },` |
|         - |  192 | `	/* Array/iterator family */` |
|         - |  193 | `	{ "array_filter",              1, 1 },` |
|         - |  194 | `	{ "array_product",             1, 0 },` |
|         - |  195 | `	{ "array_rand",                1, 1 },` |
|         - |  196 | `	{ "compact",                   1, 1 },` |
|         - |  197 | `	{ "current",                   1, 0 },` |
|         - |  198 | `	{ "end",                       1, 0 },` |
|         - |  199 | `	{ "extract",                   1, 1 },` |
|         - |  200 | `	{ "iterator_apply",            2, 1 },` |
|         - |  201 | `	{ "iterator_count",            1, 0 },` |
|         - |  202 | `	{ "iterator_to_array",         1, 1 },` |
|         - |  203 | `	{ "key",                       1, 0 },` |
|         - |  204 | `	{ "krsort",                    1, 1 },` |
|         - |  205 | `	{ "ksort",                     1, 1 },` |
|         - |  206 | `	{ "next",                      1, 0 },` |
|         - |  207 | `	{ "pos",                       1, 0 },` |
|         - |  208 | `	{ "prev",                      1, 0 },` |
|         - |  209 | `	{ "reset",                     1, 0 },` |
|         - |  210 | `	{ "rsort",                     1, 1 },` |
|         - |  211 | `	{ "shuffle",                   1, 0 },` |
|         - |  212 | `	{ "sort",                      1, 1 },` |
|         - |  213 | `	{ "uasort",                    2, 0 },` |
|         - |  214 | `	{ "uksort",                    2, 0 },` |
|         - |  215 | `	{ "usort",                     2, 0 },` |
|         - |  216 | `	/* Class/reflection family */` |
|         - |  217 | `	{ "class_alias",               2, 1 },` |
|         - |  218 | `	{ "class_exists",              1, 1 },` |
|         - |  219 | `	{ "enum_exists",               1, 1 },` |
|         - |  220 | `	{ "get_class_methods",         1, 0 },` |
|         - |  221 | `	{ "get_class_vars",            1, 0 },` |
|         - |  222 | `	{ "get_object_vars",           1, 0 },` |
|         - |  223 | `	{ "interface_exists",          1, 1 },` |
|         - |  224 | `	{ "trait_exists",              1, 1 },` |
|         - |  225 | `	{ "is_a",                      2, 1 },` |
|         - |  226 | `	{ "is_subclass_of",            2, 1 },` |
|         - |  227 | `	{ "method_exists",             2, 0 },` |
|         - |  228 | `	{ "property_exists",           2, 0 },` |
|         - |  229 | `	{ "spl_autoload",              1, 1 },` |
|         - |  230 | `	{ "spl_autoload_unregister",   1, 0 },` |
|         - |  231 | `	{ "spl_object_hash",           1, 0 },` |
|         - |  232 | `	{ "spl_object_id",             1, 0 },` |
|         - |  233 | `	/* Filesystem/IO family */` |
|         - |  234 | `	{ "basename",                  1, 1 },` |
|         - |  235 | `	{ "chdir",                     1, 0 },` |
|         - |  236 | `	{ "chgrp",                     2, 0 },` |
|         - |  237 | `	{ "dir",                       1, 1 },` |
|         - |  238 | `	{ "dirname",                   1, 1 },` |
|         - |  239 | `	{ "disk_free_space",           1, 0 },` |
|         - |  240 | `	{ "disk_total_space",          1, 0 },` |
|         - |  241 | `	{ "diskfreespace",             1, 0 },` |
|         - |  242 | `	{ "fclose",                    1, 0 },` |
|         - |  243 | `	{ "feof",                      1, 0 },` |
|         - |  244 | `	{ "fflush",                    1, 0 },` |
|         - |  245 | `	{ "fgetc",                     1, 0 },` |
|         - |  246 | `	{ "fgetcsv",                   1, 1 },` |
|         - |  247 | `	{ "file",                      1, 1 },` |
|         - |  248 | `	{ "file_exists",               1, 0 },` |
|         - |  249 | `	{ "fileatime",                 1, 0 },` |
|         - |  250 | `	{ "filectime",                 1, 0 },` |
|         - |  251 | `	{ "filemtime",                 1, 0 },` |
|         - |  252 | `	{ "filesize",                  1, 0 },` |
|         - |  253 | `	{ "filetype",                  1, 0 },` |
|         - |  254 | `	{ "flock",                     2, 1 },` |
|         - |  255 | `	{ "fpassthru",                 1, 0 },` |
|         - |  256 | `	{ "fputcsv",                   2, 1 },` |
|         - |  257 | `	{ "fputs",                     2, 1 },` |
|         - |  258 | `	{ "fseek",                     2, 1 },` |
|         - |  259 | `	{ "fstat",                     1, 0 },` |
|         - |  260 | `	/* ext/zlib's aliases of the six above; the alias needs its own row or the` |
|         - |  261 | `	 * ArgumentCountError names the function it is an alias OF. */` |
|         - |  262 | `	{ "gzclose",                   1, 0 },` |
|         - |  263 | `	{ "gzeof",                     1, 0 },` |
|         - |  264 | `	{ "gzgetc",                    1, 0 },` |
|         - |  265 | `	{ "gzpassthru",                1, 0 },` |
|         - |  266 | `	{ "gzputs",                    2, 1 },` |
|         - |  267 | `	{ "gzrewind",                  1, 0 },` |
|         - |  268 | `	{ "gzseek",                    2, 1 },` |
|         - |  269 | `	{ "gztell",                    1, 0 },` |
|         - |  270 | `	{ "gzwrite",                   2, 1 },` |
|         - |  271 | `	{ "ftell",                     1, 0 },` |
|         - |  272 | `	{ "ftruncate",                 2, 0 },` |
|         - |  273 | `	{ "getopt",                    1, 1 },` |
|         - |  274 | `	{ "is_dir",                    1, 0 },` |
|         - |  275 | `	{ "is_executable",             1, 0 },` |
|         - |  276 | `	{ "is_file",                   1, 0 },` |
|         - |  277 | `	{ "is_link",                   1, 0 },` |
|         - |  278 | `	{ "is_readable",               1, 0 },` |
|         - |  279 | `	{ "is_writable",               1, 0 },` |
|         - |  280 | `	{ "lstat",                     1, 0 },` |
|         - |  281 | `	{ "md5_file",                  1, 1 },` |
|         - |  282 | `	{ "opendir",                   1, 1 },` |
|         - |  283 | `	{ "pathinfo",                  1, 1 },` |
|         - |  284 | `	{ "pclose",                    1, 0 },` |
|         - |  285 | `	{ "readlink",                  1, 0 },` |
|         - |  286 | `	{ "realpath",                  1, 0 },` |
|         - |  287 | `	{ "stream_resolve_include_path",1, 0 },` |
|         - |  288 | `	{ "rewind",                    1, 0 },` |
|         - |  289 | `	{ "sha1_file",                 1, 1 },` |
|         - |  290 | `	{ "stat",                      1, 0 },` |
|         - |  291 | `	/* Date family */` |
|         - |  292 | `	{ "date",                      1, 1 },` |
|         - |  293 | `	{ "date_default_timezone_set", 1, 1 },` |
|         - |  294 | `	{ "date_sun_info",             3, 0 },` |
|         - |  295 | `	{ "date_sunrise",              1, 1 },` |
|         - |  296 | `	{ "date_sunset",               1, 1 },` |
|         - |  297 | `	{ "gmdate",                    1, 1 },` |
|         - |  298 | `	{ "gmmktime",                  1, 1 },` |
|         - |  299 | `	{ "idate",                     1, 1 },` |
|         - |  300 | `	{ "mktime",                    1, 1 },` |
|         - |  301 | `	/* Encoding/URL family */` |
|         - |  302 | `	{ "base64_decode",             1, 1 },` |
|         - |  303 | `	{ "base64_encode",             1, 0 },` |
|         - |  304 | `	{ "convert_uudecode",          1, 0 },` |
|         - |  305 | `	{ "convert_uuencode",          1, 0 },` |
|         - |  306 | `	{ "parse_ini_file",            1, 1 },` |
|         - |  307 | `	{ "parse_ini_string",          1, 1 },` |
|         - |  308 | `	{ "parse_url",                 1, 1 },` |
|         - |  309 | `	{ "rawurldecode",              1, 0 },` |
|         - |  310 | `	{ "rawurlencode",              1, 0 },` |
|         - |  311 | `	{ "urldecode",                 1, 0 },` |
|         - |  312 | `	{ "urlencode",                 1, 0 },` |
|         - |  313 | `	/* JSON/serialize family */` |
|         - |  314 | `	{ "filter_var",                1, 1 },` |
|         - |  315 | `	{ "json_decode",               1, 1 },` |
|         - |  316 | `	{ "json_encode",               1, 1 },` |
|         - |  317 | `	{ "json_validate",             1, 1 },` |
|         - |  318 | `	{ "serialize",                 1, 0 },` |
|         - |  319 | `	{ "unserialize",               1, 1 },` |
|         - |  320 | `	/* PCRE family */` |
|         - |  321 | `	{ "preg_match",                2, 1 },` |
|         - |  322 | `	{ "preg_match_all",            2, 1 },` |
|         - |  323 | `	{ "preg_quote",                1, 1 },` |
|         - |  324 | `	{ "preg_replace",              3, 1 },` |
|         - |  325 | `	{ "preg_replace_callback",     3, 1 },` |
|         - |  326 | `	{ "preg_split",                2, 1 },` |
|         - |  327 | `	/* XML family */` |
|         - |  328 | `	/* Constants/misc family */` |
|         - |  329 | `	{ "call_user_func",            1, 1 },` |
|         - |  330 | `	{ "call_user_func_array",      2, 0 },` |
|         - |  331 | `	{ "constant",                  1, 0 },` |
|         - |  332 | `	{ "define",                    2, 1 },` |
|         - |  333 | `	{ "defined",                   1, 0 },` |
|         - |  334 | `	{ "error_log",                 1, 1 },` |
|         - |  335 | `	{ "fnmatch",                   2, 1 },` |
|         - |  336 | `	{ "forward_static_call",       1, 1 },` |
|         - |  337 | `	{ "forward_static_call_array", 2, 0 },` |
|         - |  338 | `	{ "func_get_arg",              1, 0 },` |
|         - |  339 | `	{ "function_exists",           1, 0 },` |
|         - |  340 | `	{ "header",                    1, 1 },` |
|         - |  341 | `	{ "password_get_info",         1, 0 },` |
|         - |  342 | `	{ "putenv",                    1, 0 },` |
|         - |  343 | `	{ "register_shutdown_function", 1, 1 },` |
|         - |  344 | `	{ "set_error_handler",         1, 1 },` |
|         - |  345 | `	{ "set_exception_handler",     1, 0 },` |
|         - |  346 | `	{ "setcookie",                 1, 1 },` |
|         - |  347 | `	{ "setrawcookie",              1, 1 },` |
|         - |  348 | `	{ "trigger_error",             1, 1 },` |
|         - |  349 | `	{ "user_error",                1, 1 },` |
|         - |  350 | `	/*` |
|         - |  351 | `	 * Overrides for signatures that under-report their own minimum: the callback` |
|         - |  352 | `	 * of these three hides inside the variadic tail ("array $array, ...$rest"),` |
|         - |  353 | `	 * so the derivation reads 1 where php requires 2.` |
|         - |  354 | `	 */` |
|         - |  355 | `	{ "array_udiff",               2, 1 },` |
|         - |  356 | `	{ "array_uintersect",          2, 1 },` |
|         - |  357 | `	{ "array_diff_uassoc",         2, 1 },` |
|         - |  358 | `	{ "array_diff_ukey",           2, 1 },` |
|         - |  359 | `	{ "array_intersect_ukey",      2, 1 },` |
|         - |  360 | `	{ "array_intersect_uassoc",    2, 1 },` |
|         - |  361 | `	{ "array_udiff_assoc",         2, 1 },` |
|         - |  362 | `	{ "array_uintersect_assoc",    2, 1 },` |
|         - |  363 | `	{ "array_udiff_uassoc",        3, 1 },` |
|         - |  364 | `	{ "array_uintersect_uassoc",   3, 1 },` |
|         - |  365 | `};` |
|         - |  366 | `/*` |
|         - |  367 | ` * Stamp the minimum-arity metadata from aBuiltinArity[] onto the already` |
|         - |  368 | ` * registered host functions. Called once at VM init after every builtin family` |
|         - |  369 | ` * has been installed into hHostFunction. A name absent from the hash (e.g. a` |
|         - |  370 | ` * build without a given extension) is simply skipped.` |
|         - |  371 | ` */` |
|      6691 |  372 | `PH7_PRIVATE void VmSetBuiltinArity(ph7_vm *pVm)` |
|         5 |  373 | `{` |
|         - |  374 | `	sxu32 n;` |
|   2027378 |  375 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinArity) ; ++n ){` |
|   2020687 |  376 | `		const struct VmBuiltinArity *p = &aBuiltinArity[n];` |
|   4041369 |  377 | `		SyHashEntry *pEntry = SyHashGet(&pVm->hHostFunction,` |
|   2020682 |  378 | `			(const void *)p->zName,SyStrlen(p->zName));` |
|   2020687 |  379 | `		if( pEntry ){` |
|   2020687 |  380 | `			ph7_user_func *pFunc = (ph7_user_func *)pEntry->pUserData;` |
|   2020687 |  381 | `			pFunc->nMinArg  = p->nMin;` |
|   2020687 |  382 | `			pFunc->bAtLeast = p->bAtLeast;` |
|   1008680 |  383 | `		}` |
|   1008685 |  384 | `	}` |
|      6696 |  385 | `}` |
|         - |  386 | `/*` |
|         - |  387 | ` * PHP 8.5 parameter signatures for the C builtins, generated offline from` |
|         - |  388 | ` * a real PHP 8.5 ReflectionFunction dump over PHL's registered function` |
|         - |  389 | ` * list (see the plan's Reflection section). "= ?" marks an optional` |
|         - |  390 | ` * parameter whose default is not representable as a short literal.` |
|         - |  391 | ` * Reflection parses these strings on demand; unlisted builtins degrade to` |
|         - |  392 | ` * the min-arity data.` |
|         - |  393 | ` */` |
|         - |  394 | `static const struct VmBuiltinSig {` |
|         - |  395 | `	const char *zName;` |
|         - |  396 | `	const char *zSig;` |
|         - |  397 | `	const char *zRet;` |
|         - |  398 | `} aBuiltinSig[] = {` |
|         - |  399 | `	/* The subsystems converted from embedded PHP into C (INI, libxml, sessions).` |
|         - |  400 | `	 * A prelude function declared its parameters in PHP and Reflection read them` |
|         - |  401 | `	 * from there; a C builtin has no declaration but this table, so without a row` |
|         - |  402 | `	 * here the same function reports NO parameters -- and loses its arity bounds` |
|         - |  403 | `	 * with them. */` |
|         - |  404 | `	/* ext/curl. Signatures dumped from php 8.5's own ReflectionFunction, which` |
|         - |  405 | `	 * is also where the parameter NAMES come from: a named argument spells the` |
|         - |  406 | `	 * php one, so an invented name breaks valid php. */` |
|         - |  407 | `	{ "curl_close", "CurlHandle $handle", "void" },` |
|         - |  408 | `	{ "curl_copy_handle", "CurlHandle $handle", "CurlHandle\|false" },` |
|         - |  409 | `	{ "curl_errno", "CurlHandle $handle", "int" },` |
|         - |  410 | `	{ "curl_error", "CurlHandle $handle", "string" },` |
|         - |  411 | `	{ "curl_escape", "CurlHandle $handle, string $string", "string\|false" },` |
|         - |  412 | `	{ "curl_exec", "CurlHandle $handle", "string\|bool" },` |
|         - |  413 | `	{ "curl_file_create", "string $filename, ?string $mime_type = null, ?string $posted_filename = null", "CURLFile" },` |
|         - |  414 | `	{ "curl_getinfo", "CurlHandle $handle, ?int $option = null", "mixed" },` |
|         - |  415 | `	{ "curl_init", "?string $url = null", "CurlHandle\|false" },` |
|         - |  416 | `	{ "curl_multi_add_handle", "CurlMultiHandle $multi_handle, CurlHandle $handle", "int" },` |
|         - |  417 | `	{ "curl_multi_close", "CurlMultiHandle $multi_handle", "void" },` |
|         - |  418 | `	{ "curl_multi_errno", "CurlMultiHandle $multi_handle", "int" },` |
|         - |  419 | `	{ "curl_multi_exec", "CurlMultiHandle $multi_handle, &$still_running", "int" },` |
|         - |  420 | `	{ "curl_multi_get_handles", "CurlMultiHandle $multi_handle", "array" },` |
|         - |  421 | `	{ "curl_multi_getcontent", "CurlHandle $handle", "?string" },` |
|         - |  422 | `	{ "curl_multi_info_read", "CurlMultiHandle $multi_handle, &$queued_messages = NULL", "array\|false" },` |
|         - |  423 | `	{ "curl_multi_init", "", "CurlMultiHandle" },` |
|         - |  424 | `	{ "curl_multi_remove_handle", "CurlMultiHandle $multi_handle, CurlHandle $handle", "int" },` |
|         - |  425 | `	{ "curl_multi_select", "CurlMultiHandle $multi_handle, float $timeout = 1.0", "int" },` |
|         - |  426 | `	{ "curl_multi_setopt", "CurlMultiHandle $multi_handle, int $option, mixed $value", "bool" },` |
|         - |  427 | `	{ "curl_multi_strerror", "int $error_code", "?string" },` |
|         - |  428 | `	{ "curl_pause", "CurlHandle $handle, int $flags", "int" },` |
|         - |  429 | `	{ "curl_reset", "CurlHandle $handle", "void" },` |
|         - |  430 | `	{ "curl_setopt", "CurlHandle $handle, int $option, mixed $value", "bool" },` |
|         - |  431 | `	{ "curl_setopt_array", "CurlHandle $handle, array $options", "bool" },` |
|         - |  432 | `	{ "curl_unescape", "CurlHandle $handle, string $string", "string\|false" },` |
|         - |  433 | `	{ "curl_upkeep", "CurlHandle $handle", "bool" },` |
|         - |  434 | `	{ "curl_share_close", "CurlShareHandle $share_handle", "void" },` |
|         - |  435 | `	{ "curl_share_errno", "CurlShareHandle $share_handle", "int" },` |
|         - |  436 | `	{ "curl_share_init", "", "CurlShareHandle" },` |
|         - |  437 | `	{ "curl_share_init_persistent", "array $share_options", "CurlSharePersistentHandle" },` |
|         - |  438 | `	{ "curl_share_setopt", "CurlShareHandle $share_handle, int $option, mixed $value", "bool" },` |
|         - |  439 | `	{ "curl_share_strerror", "int $error_code", "?string" },` |
|         - |  440 | `	{ "curl_strerror", "int $error_code", "?string" },` |
|         - |  441 | `	{ "curl_version", "", "array\|false" },` |
|         - |  442 | `	/* ext/openssl. Signatures dumped from php 8.5's own ReflectionFunction --` |
|         - |  443 | `	 * the parameter NAMES included, since a named argument spells the php one.` |
|         - |  444 | ``	 * The five untyped `$key` / `$certificate` parameters are untyped in php`` |
|         - |  445 | ``	 * too: each takes a handle object, a PEM string, a `file://` path or an`` |
|         - |  446 | `	 * array pair, so php declares no type and screens by hand. */` |
|         - |  447 | `	{ "openssl_x509_export_to_file", "OpenSSLCertificate\|string $certificate, string $output_filename, bool $no_text = true", "bool" },` |
|         - |  448 | `	{ "openssl_x509_export", "OpenSSLCertificate\|string $certificate, &$output, bool $no_text = true", "bool" },` |
|         - |  449 | `	{ "openssl_x509_fingerprint", "OpenSSLCertificate\|string $certificate, string $digest_algo = 'sha1', bool $binary = false", "string\|false" },` |
|         - |  450 | `	{ "openssl_x509_check_private_key", "OpenSSLCertificate\|string $certificate, $private_key", "bool" },` |
|         - |  451 | `	{ "openssl_x509_verify", "OpenSSLCertificate\|string $certificate, $public_key", "int" },` |
|         - |  452 | `	{ "openssl_x509_parse", "OpenSSLCertificate\|string $certificate, bool $short_names = true", "array\|false" },` |
|         - |  453 | `	{ "openssl_x509_checkpurpose", "OpenSSLCertificate\|string $certificate, int $purpose, array $ca_info = [], ?string $untrusted_certificates_file = NULL", "int\|bool" },` |
|         - |  454 | `	{ "openssl_x509_read", "OpenSSLCertificate\|string $certificate", "OpenSSLCertificate\|false" },` |
|         - |  455 | `	{ "openssl_pkcs12_export_to_file", "OpenSSLCertificate\|string $certificate, string $output_filename, $private_key, string $passphrase, array $options = []", "bool" },` |
|         - |  456 | `	{ "openssl_pkcs12_export", "OpenSSLCertificate\|string $certificate, &$output, $private_key, string $passphrase, array $options = []", "bool" },` |
|         - |  457 | `	{ "openssl_pkcs12_read", "string $pkcs12, &$certificates, string $passphrase", "bool" },` |
|         - |  458 | `	{ "openssl_csr_export_to_file", "OpenSSLCertificateSigningRequest\|string $csr, string $output_filename, bool $no_text = true", "bool" },` |
|         - |  459 | `	{ "openssl_csr_export", "OpenSSLCertificateSigningRequest\|string $csr, &$output, bool $no_text = true", "bool" },` |
|         - |  460 | `	{ "openssl_csr_sign", "OpenSSLCertificateSigningRequest\|string $csr, OpenSSLCertificate\|string\|null $ca_certificate, $private_key, int $days, ?array $options = NULL, int $serial = 0, ?string $serial_hex = NULL", "OpenSSLCertificate\|false" },` |
|         - |  461 | `	{ "openssl_csr_new", "array $distinguished_names, &$private_key, ?array $options = NULL, ?array $extra_attributes = NULL", "OpenSSLCertificateSigningRequest\|bool" },` |
|         - |  462 | `	{ "openssl_csr_get_subject", "OpenSSLCertificateSigningRequest\|string $csr, bool $short_names = true", "array\|false" },` |
|         - |  463 | `	{ "openssl_csr_get_public_key", "OpenSSLCertificateSigningRequest\|string $csr, bool $short_names = true", "OpenSSLAsymmetricKey\|false" },` |
|         - |  464 | `	{ "openssl_pkcs7_verify", "string $input_filename, int $flags, ?string $signers_certificates_filename = NULL, array $ca_info = [], ?string $untrusted_certificates_filename = NULL, ?string $content = NULL, ?string $output_filename = NULL", "int\|bool" },` |
|         - |  465 | `	{ "openssl_pkcs7_encrypt", "string $input_filename, string $output_filename, $certificate, ?array $headers, int $flags = 0, int $cipher_algo = OPENSSL_CIPHER_AES_128_CBC", "bool" },` |
|         - |  466 | `	{ "openssl_pkcs7_sign", "string $input_filename, string $output_filename, OpenSSLCertificate\|string $certificate, $private_key, ?array $headers, int $flags = PKCS7_DETACHED, ?string $untrusted_certificates_filename = NULL", "bool" },` |
|         - |  467 | `	{ "openssl_pkcs7_decrypt", "string $input_filename, string $output_filename, $certificate, $private_key = NULL", "bool" },` |
|         - |  468 | `	{ "openssl_pkcs7_read", "string $data, &$certificates", "bool" },` |
|         - |  469 | `	{ "openssl_cms_verify", "string $input_filename, int $flags = 0, ?string $certificates = NULL, array $ca_info = [], ?string $untrusted_certificates_filename = NULL, ?string $content = NULL, ?string $pk7 = NULL, ?string $sigfile = NULL, int $encoding = OPENSSL_ENCODING_SMIME", "bool" },` |
|         - |  470 | `	{ "openssl_cms_encrypt", "string $input_filename, string $output_filename, $certificate, ?array $headers, int $flags = 0, int $encoding = OPENSSL_ENCODING_SMIME, string\|int $cipher_algo = OPENSSL_CIPHER_AES_128_CBC", "bool" },` |
|         - |  471 | `	{ "openssl_cms_sign", "string $input_filename, string $output_filename, OpenSSLCertificate\|string $certificate, $private_key, ?array $headers, int $flags = 0, int $encoding = OPENSSL_ENCODING_SMIME, ?string $untrusted_certificates_filename = NULL", "bool" },` |
|         - |  472 | `	{ "openssl_cms_decrypt", "string $input_filename, string $output_filename, $certificate, $private_key = NULL, int $encoding = OPENSSL_ENCODING_SMIME", "bool" },` |
|         - |  473 | `	{ "openssl_cms_read", "string $input_filename, &$certificates", "bool" },` |
|         - |  474 | `	{ "openssl_pbkdf2", "string $password, string $salt, int $key_length, int $iterations, string $digest_algo = 'sha1'", "string\|false" },` |
|         - |  475 | `	{ "openssl_error_string", "", "string\|false" },` |
|         - |  476 | `	{ "openssl_get_md_methods", "bool $aliases = false", "array" },` |
|         - |  477 | `	{ "openssl_get_cipher_methods", "bool $aliases = false", "array" },` |
|         - |  478 | `	{ "openssl_get_curve_names", "", "array\|false" },` |
|         - |  479 | `	{ "openssl_digest", "string $data, string $digest_algo, bool $binary = false", "string\|false" },` |
|         - |  480 | `	{ "openssl_encrypt", "string $data, string $cipher_algo, string $passphrase, int $options = 0, string $iv = '', &$tag = NULL, string $aad = '', int $tag_length = 16", "string\|false" },` |
|         - |  481 | `	{ "openssl_decrypt", "string $data, string $cipher_algo, string $passphrase, int $options = 0, string $iv = '', ?string $tag = NULL, string $aad = ''", "string\|false" },` |
|         - |  482 | `	{ "openssl_cipher_iv_length", "string $cipher_algo", "int\|false" },` |
|         - |  483 | `	{ "openssl_cipher_key_length", "string $cipher_algo", "int\|false" },` |
|         - |  484 | `	{ "openssl_random_pseudo_bytes", "int $length, &$strong_result = NULL", "string" },` |
|         - |  485 | `	{ "openssl_get_cert_locations", "", "array" },` |
|         - |  486 | `	{ "openssl_pkey_new", "?array $options = NULL", "OpenSSLAsymmetricKey\|false" },` |
|         - |  487 | `	{ "openssl_pkey_export_to_file", "$key, string $output_filename, ?string $passphrase = NULL, ?array $options = NULL", "bool" },` |
|         - |  488 | `	{ "openssl_pkey_export", "$key, &$output, ?string $passphrase = NULL, ?array $options = NULL", "bool" },` |
|         - |  489 | `	{ "openssl_pkey_get_public", "$public_key", "OpenSSLAsymmetricKey\|false" },` |
|         - |  490 | `	{ "openssl_get_publickey", "$public_key", "OpenSSLAsymmetricKey\|false" },` |
|         - |  491 | `	{ "openssl_pkey_get_private", "$private_key, ?string $passphrase = NULL", "OpenSSLAsymmetricKey\|false" },` |
|         - |  492 | `	{ "openssl_get_privatekey", "$private_key, ?string $passphrase = NULL", "OpenSSLAsymmetricKey\|false" },` |
|         - |  493 | `	{ "openssl_pkey_get_details", "OpenSSLAsymmetricKey $key", "array\|false" },` |
|         - |  494 | `	{ "openssl_private_encrypt", "string $data, &$encrypted_data, $private_key, int $padding = OPENSSL_PKCS1_PADDING", "bool" },` |
|         - |  495 | `	{ "openssl_private_decrypt", "string $data, &$decrypted_data, $private_key, int $padding = OPENSSL_PKCS1_PADDING, ?string $digest_algo = NULL", "bool" },` |
|         - |  496 | `	{ "openssl_public_encrypt", "string $data, &$encrypted_data, $public_key, int $padding = OPENSSL_PKCS1_PADDING, ?string $digest_algo = NULL", "bool" },` |
|         - |  497 | `	{ "openssl_public_decrypt", "string $data, &$decrypted_data, $public_key, int $padding = OPENSSL_PKCS1_PADDING", "bool" },` |
|         - |  498 | `	{ "openssl_sign", "string $data, &$signature, $private_key, string\|int $algorithm = OPENSSL_ALGO_SHA1, int $padding = 0", "bool" },` |
|         - |  499 | `	{ "openssl_verify", "string $data, string $signature, $public_key, string\|int $algorithm = OPENSSL_ALGO_SHA1, int $padding = 0", "int\|false" },` |
|         - |  500 | `	{ "openssl_seal", "string $data, &$sealed_data, &$encrypted_keys, array $public_key, string $cipher_algo, &$iv = NULL", "int\|false" },` |
|         - |  501 | `	{ "openssl_open", "string $data, &$output, string $encrypted_key, $private_key, string $cipher_algo, ?string $iv = NULL", "bool" },` |
|         - |  502 | `	{ "openssl_dh_compute_key", "string $public_key, OpenSSLAsymmetricKey $private_key", "string\|false" },` |
|         - |  503 | `	{ "openssl_pkey_derive", "$public_key, $private_key", "string\|false" },` |
|         - |  504 | `	{ "openssl_spki_new", "OpenSSLAsymmetricKey $private_key, string $challenge, int $digest_algo = OPENSSL_ALGO_MD5", "string\|false" },` |
|         - |  505 | `	{ "openssl_spki_verify", "string $spki", "bool" },` |
|         - |  506 | `	{ "openssl_spki_export", "string $spki", "string\|false" },` |
|         - |  507 | `	{ "openssl_spki_export_challenge", "string $spki", "string\|false" },` |
|         - |  508 | `	{ "get_cfg_var", "string $option", "array\|string\|false" },` |
|         - |  509 | `	{ "ini_get", "string $option", "string\|false" },` |
|         - |  510 | `	{ "ini_get_all", "?string $extension = null, bool $details = true", "array\|false" },` |
|         - |  511 | `	{ "ini_restore", "string $option", "void" },` |
|         - |  512 | `	{ "ini_alter", "string $option, string\|int\|float\|bool\|null $value", "string\|false" },` |
|         - |  513 | `	{ "ini_set", "string $option, string\|int\|float\|bool\|null $value", "string\|false" },` |
|         - |  514 | `	{ "libxml_clear_errors", "", "void" },` |
|         - |  515 | `	{ "libxml_get_errors", "", "array" },` |
|         - |  516 | `	{ "libxml_get_external_entity_loader", "", "?callable" },` |
|         - |  517 | `	{ "libxml_get_last_error", "", "LibXMLError\|false" },` |
|         - |  518 | `	{ "libxml_set_external_entity_loader", "?callable $resolver_function", "true" },` |
|         - |  519 | `	{ "libxml_set_streams_context", "$context", "void" },` |
|         - |  520 | `	{ "libxml_use_internal_errors", "?bool $use_errors = null", "bool" },` |
|         - |  521 | ``	/* ext/simplexml's three, and ext/dom's one door into it. `object $node` is`` |
|         - |  522 | `	 * php's own declaration for both directions: the class screen is the body's,` |
|         - |  523 | `	 * so a plain object gets the TypeError the body words and not ZPP's. */` |
|         - |  524 | `	{ "simplexml_load_file",` |
|         - |  525 | `	  "string $filename, ?string $class_name = SimpleXMLElement::class, int $options = 0, "` |
|         - |  526 | `	  "string $namespace_or_prefix = '', bool $is_prefix = false", "SimpleXMLElement\|false" },` |
|         - |  527 | `	{ "simplexml_load_string",` |
|         - |  528 | `	  "string $data, ?string $class_name = SimpleXMLElement::class, int $options = 0, "` |
|         - |  529 | `	  "string $namespace_or_prefix = '', bool $is_prefix = false", "SimpleXMLElement\|false" },` |
|         - |  530 | `	{ "simplexml_import_dom",` |
|         - |  531 | `	  "object $node, ?string $class_name = SimpleXMLElement::class", "?SimpleXMLElement" },` |
|         - |  532 | `	{ "dom_import_simplexml", "object $node", "DOMAttr\|DOMElement" },` |
|         - |  533 | `	/* ext/pdo's one function: the procedural spelling of` |
|         - |  534 | `	 * PDO::getAvailableDrivers(). */` |
|         - |  535 | `	{ "pdo_drivers", "", "array" },` |
|         - |  536 | `	{ "xml_error_string", "int $error_code", "?string" },` |
|         - |  537 | `	{ "xml_get_current_byte_index", "XMLParser $parser", "int" },` |
|         - |  538 | `	{ "xml_get_current_column_number", "XMLParser $parser", "int" },` |
|         - |  539 | `	{ "xml_get_current_line_number", "XMLParser $parser", "int" },` |
|         - |  540 | `	{ "xml_get_error_code", "XMLParser $parser", "int" },` |
|         - |  541 | `	{ "xml_parse", "XMLParser $parser, string $data, bool $is_final = false", "int" },` |
|         - |  542 | `	{ "xml_parse_into_struct", "XMLParser $parser, string $data, &$values, &$index = NULL", "int\|false" },` |
|         - |  543 | `	{ "xml_parser_create", "?string $encoding = NULL", "XMLParser" },` |
|         - |  544 | `	{ "xml_parser_create_ns", "?string $encoding = NULL, string $separator = ':'", "XMLParser" },` |
|         - |  545 | `	{ "xml_parser_get_option", "XMLParser $parser, int $option", "string\|int\|bool" },` |
|         - |  546 | `	{ "xml_parser_set_option", "XMLParser $parser, int $option, $value", "bool" },` |
|         - |  547 | `	{ "xml_set_character_data_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  548 | `	{ "xml_set_default_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  549 | `	{ "xml_set_element_handler", "XMLParser $parser, callable\|string\|null $start_handler, callable\|string\|null $end_handler", "true" },` |
|         - |  550 | `	{ "xml_set_end_namespace_decl_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  551 | `	{ "xml_set_external_entity_ref_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  552 | `	{ "xml_set_notation_decl_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  553 | `	{ "xml_set_processing_instruction_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  554 | `	{ "xml_set_start_namespace_decl_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  555 | `	{ "xml_set_unparsed_entity_decl_handler", "XMLParser $parser, callable\|string\|null $handler", "true" },` |
|         - |  556 | `	/* ext/xmlwriter: php presents every writer verb under a function name as` |
|         - |  557 | `	 * well, with the writer as argument #1 -- which is the numbering its own` |
|         - |  558 | `	 * diagnostics report from BOTH spellings (see vm_xmlwriter.c). */` |
|         - |  559 | `	{ "xmlwriter_open_uri", "string $uri", "XMLWriter\|false" },` |
|         - |  560 | `	{ "xmlwriter_open_memory", "", "XMLWriter\|false" },` |
|         - |  561 | `	{ "xmlwriter_set_indent", "XMLWriter $writer, bool $enable", "bool" },` |
|         - |  562 | `	{ "xmlwriter_set_indent_string", "XMLWriter $writer, string $indentation", "bool" },` |
|         - |  563 | `	{ "xmlwriter_start_comment", "XMLWriter $writer", "bool" },` |
|         - |  564 | `	{ "xmlwriter_end_comment", "XMLWriter $writer", "bool" },` |
|         - |  565 | `	{ "xmlwriter_start_attribute", "XMLWriter $writer, string $name", "bool" },` |
|         - |  566 | `	{ "xmlwriter_end_attribute", "XMLWriter $writer", "bool" },` |
|         - |  567 | `	{ "xmlwriter_write_attribute", "XMLWriter $writer, string $name, string $value", "bool" },` |
|         - |  568 | `	{ "xmlwriter_start_attribute_ns", "XMLWriter $writer, ?string $prefix, string $name, ?string $namespace", "bool" },` |
|         - |  569 | `	{ "xmlwriter_write_attribute_ns", "XMLWriter $writer, ?string $prefix, string $name, ?string $namespace, string $value", "bool" },` |
|         - |  570 | `	{ "xmlwriter_start_element", "XMLWriter $writer, string $name", "bool" },` |
|         - |  571 | `	{ "xmlwriter_end_element", "XMLWriter $writer", "bool" },` |
|         - |  572 | `	{ "xmlwriter_full_end_element", "XMLWriter $writer", "bool" },` |
|         - |  573 | `	{ "xmlwriter_start_element_ns", "XMLWriter $writer, ?string $prefix, string $name, ?string $namespace", "bool" },` |
|         - |  574 | `	{ "xmlwriter_write_element", "XMLWriter $writer, string $name, ?string $content = null", "bool" },` |
|         - |  575 | `	{ "xmlwriter_write_element_ns", "XMLWriter $writer, ?string $prefix, string $name, ?string $namespace, ?string $content = null", "bool" },` |
|         - |  576 | `	{ "xmlwriter_start_pi", "XMLWriter $writer, string $target", "bool" },` |
|         - |  577 | `	{ "xmlwriter_end_pi", "XMLWriter $writer", "bool" },` |
|         - |  578 | `	{ "xmlwriter_write_pi", "XMLWriter $writer, string $target, string $content", "bool" },` |
|         - |  579 | `	{ "xmlwriter_start_cdata", "XMLWriter $writer", "bool" },` |
|         - |  580 | `	{ "xmlwriter_end_cdata", "XMLWriter $writer", "bool" },` |
|         - |  581 | `	{ "xmlwriter_write_cdata", "XMLWriter $writer, string $content", "bool" },` |
|         - |  582 | `	{ "xmlwriter_text", "XMLWriter $writer, string $content", "bool" },` |
|         - |  583 | `	{ "xmlwriter_write_raw", "XMLWriter $writer, string $content", "bool" },` |
|         - |  584 | `	{ "xmlwriter_start_document", "XMLWriter $writer, ?string $version = '1.0', ?string $encoding = null, ?string $standalone = null", "bool" },` |
|         - |  585 | `	{ "xmlwriter_end_document", "XMLWriter $writer", "bool" },` |
|         - |  586 | `	{ "xmlwriter_write_comment", "XMLWriter $writer, string $content", "bool" },` |
|         - |  587 | `	{ "xmlwriter_start_dtd", "XMLWriter $writer, string $qualifiedName, ?string $publicId = null, ?string $systemId = null", "bool" },` |
|         - |  588 | `	{ "xmlwriter_end_dtd", "XMLWriter $writer", "bool" },` |
|         - |  589 | `	{ "xmlwriter_write_dtd", "XMLWriter $writer, string $name, ?string $publicId = null, ?string $systemId = null, ?string $content = null", "bool" },` |
|         - |  590 | `	{ "xmlwriter_start_dtd_element", "XMLWriter $writer, string $qualifiedName", "bool" },` |
|         - |  591 | `	{ "xmlwriter_end_dtd_element", "XMLWriter $writer", "bool" },` |
|         - |  592 | `	{ "xmlwriter_write_dtd_element", "XMLWriter $writer, string $name, string $content", "bool" },` |
|         - |  593 | `	{ "xmlwriter_start_dtd_attlist", "XMLWriter $writer, string $name", "bool" },` |
|         - |  594 | `	{ "xmlwriter_end_dtd_attlist", "XMLWriter $writer", "bool" },` |
|         - |  595 | `	{ "xmlwriter_write_dtd_attlist", "XMLWriter $writer, string $name, string $content", "bool" },` |
|         - |  596 | `	{ "xmlwriter_start_dtd_entity", "XMLWriter $writer, string $name, bool $isParam", "bool" },` |
|         - |  597 | `	{ "xmlwriter_end_dtd_entity", "XMLWriter $writer", "bool" },` |
|         - |  598 | `	{ "xmlwriter_write_dtd_entity", "XMLWriter $writer, string $name, string $content, bool $isParam = false, ?string $publicId = null, ?string $systemId = null, ?string $notationData = null", "bool" },` |
|         - |  599 | `	{ "xmlwriter_output_memory", "XMLWriter $writer, bool $flush = true", "string" },` |
|         - |  600 | `	{ "xmlwriter_flush", "XMLWriter $writer, bool $empty = true", "string\|int" },` |
|         - |  601 | `	{ "session_abort", "", "bool" },` |
|         - |  602 | `	{ "session_cache_expire", "?int $value = null", "int\|false" },` |
|         - |  603 | `	{ "session_cache_limiter", "?string $value = null", "string\|false" },` |
|         - |  604 | `	{ "session_commit", "", "bool" },` |
|         - |  605 | `	{ "session_create_id", "string $prefix = \"\"", "string\|false" },` |
|         - |  606 | `	{ "session_decode", "string $data", "bool" },` |
|         - |  607 | `	{ "session_destroy", "", "bool" },` |
|         - |  608 | `	{ "session_gc", "", "int\|false" },` |
|         - |  609 | `	{ "session_get_cookie_params", "", "array" },` |
|         - |  610 | `	{ "session_set_save_handler", "$sessionhandler, ...$rest = ?", "bool" },` |
|         - |  611 | `	{ "session_set_cookie_params", "array\|int $lifetime_or_options, ?string $path = null, ?string $domain = null, ?bool $secure = null, ?bool $httponly = null", "bool" },` |
|         - |  612 | `	{ "session_encode", "", "string\|false" },` |
|         - |  613 | `	{ "session_id", "?string $id = null", "string\|false" },` |
|         - |  614 | `	{ "session_module_name", "?string $module = null", "string\|false" },` |
|         - |  615 | `	{ "session_name", "?string $name = null", "string\|false" },` |
|         - |  616 | `	{ "session_regenerate_id", "bool $delete_old_session = false", "bool" },` |
|         - |  617 | `	{ "session_register_shutdown", "", "void" },` |
|         - |  618 | `	{ "session_reset", "", "bool" },` |
|         - |  619 | `	{ "session_save_path", "?string $path = null", "string\|false" },` |
|         - |  620 | `	{ "session_start", "array $options = []", "bool" },` |
|         - |  621 | `	{ "session_status", "", "int" },` |
|         - |  622 | `	{ "session_unset", "", "bool" },` |
|         - |  623 | `	{ "session_write_close", "", "bool" },` |
|         - |  624 | `	{ "abs", "int\|float $num", "int\|float" },` |
|         - |  625 | `	{ "acos", "float $num", "float" },` |
|         - |  626 | `	{ "acosh", "float $num", "float" },` |
|         - |  627 | `	{ "addcslashes", "string $string, string $characters", "string" },` |
|         - |  628 | `	{ "addslashes", "string $string", "string" },` |
|         - |  629 | `	{ "array_all", "array $array, callable $callback", "bool" },` |
|         - |  630 | `	{ "array_any", "array $array, callable $callback", "bool" },` |
|         - |  631 | `	{ "array_chunk", "array $array, int $length, bool $preserve_keys = false", "array" },` |
|         - |  632 | `	{ "array_column", "array $array, string\|int\|null $column_key, string\|int\|null $index_key = NULL", "array" },` |
|         - |  633 | `	{ "array_combine", "array $keys, array $values", "array" },` |
|         - |  634 | `	{ "array_diff", "array $array, array ...$arrays = ?", "array" },` |
|         - |  635 | `	{ "array_diff_assoc", "array $array, array ...$arrays = ?", "array" },` |
|         - |  636 | `	{ "array_diff_key", "array $array, array ...$arrays = ?", "array" },` |
|         - |  637 | `	{ "array_diff_uassoc", "array $array, ...$rest = ?", "array" },` |
|         - |  638 | `	{ "array_diff_ukey", "array $array, ...$rest = ?", "array" },` |
|         - |  639 | `	{ "array_fill", "int $start_index, int $count, mixed $value", "array" },` |
|         - |  640 | `	{ "array_fill_keys", "array $keys, mixed $value", "array" },` |
|         - |  641 | `	{ "array_filter", "array $array, ?callable $callback = NULL, int $mode = 0", "array" },` |
|         - |  642 | `	{ "array_find", "array $array, callable $callback", "mixed" },` |
|         - |  643 | `	{ "array_find_key", "array $array, callable $callback", "mixed" },` |
|         - |  644 | `	{ "array_first", "array $array", "mixed" },` |
|         - |  645 | `	{ "array_flip", "array $array", "array" },` |
|         - |  646 | `	{ "array_intersect", "array $array, array ...$arrays = ?", "array" },` |
|         - |  647 | `	{ "array_intersect_assoc", "array $array, array ...$arrays = ?", "array" },` |
|         - |  648 | `	{ "array_intersect_key", "array $array, array ...$arrays = ?", "array" },` |
|         - |  649 | `	{ "array_intersect_uassoc", "array $array, ...$rest = ?", "array" },` |
|         - |  650 | `	{ "array_intersect_ukey", "array $array, ...$rest = ?", "array" },` |
|         - |  651 | `	{ "array_is_list", "array $array", "bool" },` |
|         - |  652 | `	{ "array_key_exists", "$key, array $array", "bool" },` |
|         - |  653 | `	{ "array_key_first", "array $array", "string\|int\|null" },` |
|         - |  654 | `	{ "array_key_last", "array $array", "string\|int\|null" },` |
|         - |  655 | `	{ "array_keys", "array $array, mixed $filter_value = ?, bool $strict = false", "array" },` |
|         - |  656 | `	{ "array_last", "array $array", "mixed" },` |
|         - |  657 | `	{ "array_map", "?callable $callback, array $array, array ...$arrays = ?", "array" },` |
|         - |  658 | `	{ "array_merge", "array ...$arrays = ?", "array" },` |
|         - |  659 | `	{ "array_multisort", "&$array, &...$rest = ?", "true" },` |
|         - |  660 | `	{ "array_merge_recursive", "array ...$arrays = ?", "array" },` |
|         - |  661 | `	{ "array_pad", "array $array, int $length, mixed $value", "array" },` |
|         - |  662 | `	{ "array_pop", "array &$array", "mixed" },` |
|         - |  663 | `	{ "array_product", "array $array", "int\|float" },` |
|         - |  664 | `	{ "array_push", "array &$array, mixed ...$values = ?", "int" },` |
|         - |  665 | `	{ "array_rand", "array $array, int $num = 1", "array\|string\|int" },` |
|         - |  666 | `	{ "array_reduce", "array $array, callable $callback, mixed $initial = NULL", "mixed" },` |
|         - |  667 | `	{ "array_replace", "array $array, array ...$replacements = ?", "array" },` |
|         - |  668 | `	{ "array_reverse", "array $array, bool $preserve_keys = false", "array" },` |
|         - |  669 | `	{ "array_search", "mixed $needle, array $haystack, bool $strict = false", "string\|int\|false" },` |
|         - |  670 | `	{ "array_shift", "array &$array", "mixed" },` |
|         - |  671 | `	{ "array_slice", "array $array, int $offset, ?int $length = NULL, bool $preserve_keys = false", "array" },` |
|         - |  672 | `	{ "array_splice", "array &$array, int $offset, ?int $length = NULL, mixed $replacement = []", "array" },` |
|         - |  673 | `	{ "array_sum", "array $array", "int\|float" },` |
|         - |  674 | `	{ "array_udiff", "array $array, ...$rest = ?", "array" },` |
|         - |  675 | `	{ "array_udiff_assoc", "array $array, ...$rest = ?", "array" },` |
|         - |  676 | `	{ "array_udiff_uassoc", "array $array, ...$rest = ?", "array" },` |
|         - |  677 | `	{ "array_uintersect", "array $array, ...$rest = ?", "array" },` |
|         - |  678 | `	{ "array_uintersect_assoc", "array $array, ...$rest = ?", "array" },` |
|         - |  679 | `	{ "array_uintersect_uassoc", "array $array, ...$rest = ?", "array" },` |
|         - |  680 | `	{ "array_unique", "array $array, int $flags = SORT_STRING", "array" },` |
|         - |  681 | `	{ "array_unshift", "array &$array, mixed ...$values = ?", "int" },` |
|         - |  682 | `	{ "array_values", "array $array", "array" },` |
|         - |  683 | `	{ "array_walk", "object\|array &$array, callable $callback, mixed $arg = ?", "true" },` |
|         - |  684 | `	{ "array_walk_recursive", "object\|array &$array, callable $callback, mixed $arg = ?", "true" },` |
|         - |  685 | `	{ "arsort", "array &$array, int $flags = SORT_REGULAR", "true" },` |
|         - |  686 | `	{ "asin", "float $num", "float" },` |
|         - |  687 | `	{ "asinh", "float $num", "float" },` |
|         - |  688 | `	{ "asort", "array &$array, int $flags = SORT_REGULAR", "true" },` |
|         - |  689 | `	{ "assert", "mixed $assertion, Throwable\|string\|null $description = NULL", "bool" },` |
|         - |  690 | `	{ "atan", "float $num", "float" },` |
|         - |  691 | `	{ "atanh", "float $num", "float" },` |
|         - |  692 | `	{ "atan2", "float $y, float $x", "float" },` |
|         - |  693 | `	{ "base64_decode", "string $string, bool $strict = false", "string\|false" },` |
|         - |  694 | `	{ "base64_encode", "string $string", "string" },` |
|         - |  695 | `	{ "base_convert", "string $num, int $from_base, int $to_base", "string" },` |
|         - |  696 | `	{ "basename", "string $path, string $suffix = ''", "string" },` |
|         - |  697 | `	{ "bcadd", "string $num1, string $num2, ?int $scale = NULL", "string" },` |
|         - |  698 | `	{ "bcceil", "string $num", "string" },` |
|         - |  699 | `	{ "bccomp", "string $num1, string $num2, ?int $scale = NULL", "int" },` |
|         - |  700 | `	{ "bcdiv", "string $num1, string $num2, ?int $scale = NULL", "string" },` |
|         - |  701 | `	{ "bcdivmod", "string $num1, string $num2, ?int $scale = NULL", "array" },` |
|         - |  702 | `	{ "bcmod", "string $num1, string $num2, ?int $scale = NULL", "string" },` |
|         - |  703 | `	{ "bcfloor", "string $num", "string" },` |
|         - |  704 | `	{ "bcmul", "string $num1, string $num2, ?int $scale = NULL", "string" },` |
|         - |  705 | `	{ "bcpow", "string $num, string $exponent, ?int $scale = NULL", "string" },` |
|         - |  706 | `	{ "bcpowmod", "string $num, string $exponent, string $modulus, ?int $scale = NULL", "string" },` |
|         - |  707 | `	{ "bcround", "string $num, int $precision = 0, RoundingMode $mode = RoundingMode::HalfAwayFromZero", "string" },` |
|         - |  708 | `	{ "bcsqrt", "string $num, ?int $scale = NULL", "string" },` |
|         - |  709 | `	{ "bcscale", "?int $scale = NULL", "int" },` |
|         - |  710 | `	{ "bcsub", "string $num1, string $num2, ?int $scale = NULL", "string" },` |
|         - |  711 | `	{ "bin2hex", "string $string", "string" },` |
|         - |  712 | `	{ "bindec", "string $binary_string", "int\|float" },` |
|         - |  713 | `	{ "boolval", "mixed $value", "bool" },` |
|         - |  714 | `	{ "call_user_func", "callable $callback, mixed ...$args = ?", "mixed" },` |
|         - |  715 | `	{ "call_user_func_array", "callable $callback, array $args", "mixed" },` |
|         - |  716 | `	{ "cal_days_in_month", "int $calendar, int $month, int $year", "int" },` |
|         - |  717 | `	{ "cal_from_jd", "int $julian_day, int $calendar", "array" },` |
|         - |  718 | `	{ "cal_info", "int $calendar = -1", "array" },` |
|         - |  719 | `	{ "cal_to_jd", "int $calendar, int $month, int $day, int $year", "int" },` |
|         - |  720 | `	{ "ceil", "int\|float $num", "float" },` |
|         - |  721 | `	{ "chdir", "string $directory", "bool" },` |
|         - |  722 | `	{ "chgrp", "string $filename, string\|int $group", "bool" },` |
|         - |  723 | `	{ "chmod", "string $filename, int $permissions", "bool" },` |
|         - |  724 | `	{ "chop", "string $string, string $characters = \" \\n\\r\\t\\v\\x00\"", "string" },` |
|         - |  725 | `	{ "chown", "string $filename, string\|int $user", "bool" },` |
|         - |  726 | `	{ "chr", "int $codepoint", "string" },` |
|         - |  727 | `	{ "chroot", "string $directory", "bool" },` |
|         - |  728 | `	{ "chunk_split", "string $string, int $length = 76, string $separator = \"\\r\\n\"", "string" },` |
|         - |  729 | `	{ "class_alias", "string $class, string $alias, bool $autoload = true", "bool" },` |
|         - |  730 | `	{ "class_exists", "string $class, bool $autoload = true", "bool" },` |
|         - |  731 | ``	/* `$object_or_class` carries NO declared type on purpose: php screens it with`` |
|         - |  732 | `	 * Z_PARAM_OBJ_OR_STR, which refuses in the standard "must be of type` |
|         - |  733 | `	 * object\|string" wording while ReflectionParameter reports no type at all.` |
|         - |  734 | `	 * The builtin raises that refusal itself (vm_builtin_class.c). */` |
|         - |  735 | `	{ "class_implements", "$object_or_class, bool $autoload = true", "array\|false" },` |
|         - |  736 | `	{ "class_parents", "$object_or_class, bool $autoload = true", "array\|false" },` |
|         - |  737 | `	{ "class_uses", "$object_or_class, bool $autoload = true", "array\|false" },` |
|         - |  738 | `	{ "enum_exists", "string $enum, bool $autoload = true", "bool" },` |
|         - |  739 | `	{ "clone", "object $object, array $withProperties = []", "object" },` |
|         - |  740 | `	{ "closedir", "$dir_handle = NULL", "void" },` |
|         - |  741 | `	{ "compact", "$var_name, ...$var_names = ?", "array" },` |
|         - |  742 | `	{ "connection_aborted", "", "int" },` |
|         - |  743 | `	{ "connection_status", "", "int" },` |
|         - |  744 | `	{ "constant", "string $name", "mixed" },` |
|         - |  745 | `	{ "convert_uudecode", "string $string", "string\|false" },` |
|         - |  746 | `	{ "convert_uuencode", "string $string", "string" },` |
|         - |  747 | `	{ "copy", "string $from, string $to, $context = NULL", "bool" },` |
|         - |  748 | `	{ "cos", "float $num", "float" },` |
|         - |  749 | `	{ "cosh", "float $num", "float" },` |
|         - |  750 | `	{ "count", "Countable\|array $value, int $mode = COUNT_NORMAL", "int" },` |
|         - |  751 | `	{ "count_chars", "string $string, int $mode = 0", "array\|string" },` |
|         - |  752 | `	{ "crc32", "string $string", "int" },` |
|         - |  753 | `	{ "ctype_alnum", "mixed $text", "bool" },` |
|         - |  754 | `	{ "ctype_alpha", "mixed $text", "bool" },` |
|         - |  755 | `	{ "ctype_cntrl", "mixed $text", "bool" },` |
|         - |  756 | `	{ "ctype_digit", "mixed $text", "bool" },` |
|         - |  757 | `	{ "ctype_graph", "mixed $text", "bool" },` |
|         - |  758 | `	{ "ctype_lower", "mixed $text", "bool" },` |
|         - |  759 | `	{ "ctype_print", "mixed $text", "bool" },` |
|         - |  760 | `	{ "ctype_punct", "mixed $text", "bool" },` |
|         - |  761 | `	{ "ctype_space", "mixed $text", "bool" },` |
|         - |  762 | `	{ "ctype_upper", "mixed $text", "bool" },` |
|         - |  763 | `	{ "ctype_xdigit", "mixed $text", "bool" },` |
|         - |  764 | `	{ "current", "object\|array $array", "mixed" },` |
|         - |  765 | `	{ "date", "string $format, ?int $timestamp = NULL", "string" },` |
|         - |  766 | `	{ "date_add", "DateTime $object, DateInterval $interval", "DateTime" },` |
|         - |  767 | `	{ "date_create", "string $datetime = 'now', ?DateTimeZone $timezone = NULL", "DateTime\|false" },` |
|         - |  768 | `	{ "date_create_from_format", "string $format, string $datetime, ?DateTimeZone $timezone = NULL", "DateTime\|false" },` |
|         - |  769 | `	{ "date_create_immutable", "string $datetime = 'now', ?DateTimeZone $timezone = NULL", "DateTimeImmutable\|false" },` |
|         - |  770 | `	{ "date_create_immutable_from_format", "string $format, string $datetime, ?DateTimeZone $timezone = NULL", "DateTimeImmutable\|false" },` |
|         - |  771 | `	{ "date_date_set", "DateTime $object, int $year, int $month, int $day", "DateTime" },` |
|         - |  772 | `	{ "date_diff", "DateTimeInterface $baseObject, DateTimeInterface $targetObject, bool $absolute = false", "DateInterval" },` |
|         - |  773 | `	{ "date_format", "DateTimeInterface $object, string $format", "string" },` |
|         - |  774 | `	{ "date_get_last_errors", "", "array\|false" },` |
|         - |  775 | `	{ "date_interval_create_from_date_string", "string $datetime", "DateInterval\|false" },` |
|         - |  776 | `	{ "date_interval_format", "DateInterval $object, string $format", "string" },` |
|         - |  777 | `	{ "date_isodate_set", "DateTime $object, int $year, int $week, int $dayOfWeek = 1", "DateTime" },` |
|         - |  778 | `	{ "date_modify", "DateTime $object, string $modifier", "DateTime\|false" },` |
|         - |  779 | `	{ "date_offset_get", "DateTimeInterface $object", "int" },` |
|         - |  780 | `	{ "date_parse", "string $datetime", "array" },` |
|         - |  781 | `	{ "date_parse_from_format", "string $format, string $datetime", "array" },` |
|         - |  782 | `	{ "date_sub", "DateTime $object, DateInterval $interval", "DateTime" },` |
|         - |  783 | `	{ "date_time_set", "DateTime $object, int $hour, int $minute, int $second = 0, int $microsecond = 0", "DateTime" },` |
|         - |  784 | `	{ "date_timestamp_get", "DateTimeInterface $object", "int" },` |
|         - |  785 | `	{ "date_timestamp_set", "DateTime $object, int $timestamp", "DateTime" },` |
|         - |  786 | `	{ "date_timezone_get", "DateTimeInterface $object", "DateTimeZone\|false" },` |
|         - |  787 | `	{ "date_timezone_set", "DateTime $object, DateTimeZone $timezone", "DateTime" },` |
|         - |  788 | `	{ "timezone_abbreviations_list", "", "array" },` |
|         - |  789 | `	{ "timezone_identifiers_list", "int $timezoneGroup = DateTimeZone::ALL, ?string $countryCode = null", "array" },` |
|         - |  790 | `	{ "timezone_location_get", "DateTimeZone $object", "array\|false" },` |
|         - |  791 | `	{ "timezone_name_from_abbr", "string $abbr, int $utcOffset = -1, int $isDST = -1", "string\|false" },` |
|         - |  792 | `	{ "timezone_name_get", "DateTimeZone $object", "string" },` |
|         - |  793 | `	{ "timezone_offset_get", "DateTimeZone $object, DateTimeInterface $datetime", "int" },` |
|         - |  794 | `	{ "timezone_open", "string $timezone", "DateTimeZone\|false" },` |
|         - |  795 | `	{ "timezone_transitions_get", "DateTimeZone $object, int $timestampBegin = PHP_INT_MIN, int $timestampEnd = 2147483647", "array\|false" },` |
|         - |  796 | `	{ "timezone_version_get", "", "string" },` |
|         - |  797 | `	{ "date_default_timezone_get", "", "string" },` |
|         - |  798 | `	{ "date_default_timezone_set", "string $timezoneId", "bool" },` |
|         - |  799 | `	{ "date_sun_info", "int $timestamp, float $latitude, float $longitude", "array" },` |
|         - |  800 | `	{ "date_sunrise", "int $timestamp, int $returnFormat = SUNFUNCS_RET_STRING, ?float $latitude = null, ?float $longitude = null, ?float $zenith = null, ?float $utcOffset = null", "string\|int\|float\|false" },` |
|         - |  801 | `	{ "date_sunset", "int $timestamp, int $returnFormat = SUNFUNCS_RET_STRING, ?float $latitude = null, ?float $longitude = null, ?float $zenith = null, ?float $utcOffset = null", "string\|int\|float\|false" },` |
|         - |  802 | `	{ "debug_backtrace", "int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT, int $limit = 0", "array" },` |
|         - |  803 | `	{ "debug_print_backtrace", "int $options = 0, int $limit = 0", "void" },` |
|         - |  804 | `	{ "decbin", "int $num", "string" },` |
|         - |  805 | `	{ "dechex", "int $num", "string" },` |
|         - |  806 | `	{ "decoct", "int $num", "string" },` |
|         - |  807 | `	{ "define", "string $constant_name, mixed $value, bool $case_insensitive = false", "bool" },` |
|         - |  808 | `	{ "defined", "string $constant_name", "bool" },` |
|         - |  809 | `	{ "deg2rad", "float $num", "float" },` |
|         - |  810 | `	{ "die", "string\|int $status = 0", "never" },` |
|         - |  811 | `	{ "dir", "string $directory, $context = NULL", "Directory\|false" },` |
|         - |  812 | `	{ "dirname", "string $path, int $levels = 1", "string" },` |
|         - |  813 | `	{ "disk_free_space", "string $directory", "float\|false" },` |
|         - |  814 | `	{ "disk_total_space", "string $directory", "float\|false" },` |
|         - |  815 | `	{ "diskfreespace", "string $directory", "float\|false" },` |
|         - |  816 | `	{ "easter_date", "?int $year = NULL, int $mode = CAL_EASTER_DEFAULT", "int" },` |
|         - |  817 | `	{ "easter_days", "?int $year = NULL, int $mode = CAL_EASTER_DEFAULT", "int" },` |
|         - |  818 | `	{ "end", "object\|array &$array", "mixed" },` |
|         - |  819 | `	{ "error_get_last", "", "?array" },` |
|         - |  820 | `	{ "error_clear_last", "", "void" },` |
|         - |  821 | `	{ "error_log", "string $message, int $message_type = 0, ?string $destination = NULL, ?string $additional_headers = NULL", "bool" },` |
|         - |  822 | `	{ "error_reporting", "?int $error_level = NULL", "int" },` |
|         - |  823 | `	{ "escapeshellarg", "string $arg", "string" },` |
|         - |  824 | `	{ "escapeshellcmd", "string $command", "string" },` |
|         - |  825 | `	{ "exec", "string $command, &$output = NULL, &$result_code = NULL", "string\|false" },` |
|         - |  826 | `	{ "exit", "string\|int $status = 0", "never" },` |
|         - |  827 | `	{ "exp", "float $num", "float" },` |
|         - |  828 | `	{ "expm1", "float $num", "float" },` |
|         - |  829 | `	{ "explode", "string $separator, string $string, int $limit = PHP_INT_MAX", "array" },` |
|         - |  830 | `	{ "extension_loaded", "string $extension", "bool" },` |
|         - |  831 | `	{ "extract", "array &$array, int $flags = EXTR_OVERWRITE, string $prefix = ''", "int" },` |
|         - |  832 | `	{ "fclose", "$stream", "bool" },` |
|         - |  833 | `	{ "feof", "$stream", "bool" },` |
|         - |  834 | `	{ "fflush", "$stream", "bool" },` |
|         - |  835 | `	{ "fgetc", "$stream", "string\|false" },` |
|         - |  836 | `	{ "fgetcsv", "$stream, ?int $length = NULL, string $separator = ',', string $enclosure = '\"', string $escape = '\\\\'", "array\|false" },` |
|         - |  837 | `	{ "fgets", "$stream, ?int $length = NULL", "string\|false" },` |
|         - |  838 | `	{ "file", "string $filename, int $flags = 0, $context = NULL", "array\|false" },` |
|         - |  839 | `	{ "file_exists", "string $filename", "bool" },` |
|         - |  840 | `	{ "file_get_contents", "string $filename, bool $use_include_path = false, $context = NULL, int $offset = 0, ?int $length = NULL", "string\|false" },` |
|         - |  841 | `	{ "file_put_contents", "string $filename, mixed $data, int $flags = 0, $context = NULL", "int\|false" },` |
|         - |  842 | `	{ "fileatime", "string $filename", "int\|false" },` |
|         - |  843 | `	{ "filectime", "string $filename", "int\|false" },` |
|         - |  844 | `	{ "filegroup", "string $filename", "int\|false" },` |
|         - |  845 | `	{ "fileinode", "string $filename", "int\|false" },` |
|         - |  846 | `	{ "filemtime", "string $filename", "int\|false" },` |
|         - |  847 | `	{ "fileowner", "string $filename", "int\|false" },` |
|         - |  848 | `	{ "fileperms", "string $filename", "int\|false" },` |
|         - |  849 | `	{ "filesize", "string $filename", "int\|false" },` |
|         - |  850 | `	{ "filetype", "string $filename", "string\|false" },` |
|         - |  851 | `	{ "filter_has_var", "int $input_type, string $var_name", "bool" },` |
|         - |  852 | `	{ "filter_id", "string $name", "int\|false" },` |
|         - |  853 | `	{ "filter_input", "int $type, string $var_name, int $filter = FILTER_DEFAULT, array\|int $options = 0", "mixed" },` |
|         - |  854 | `	{ "filter_input_array", "int $type, array\|int $options = FILTER_DEFAULT, bool $add_empty = true", "array\|false\|null" },` |
|         - |  855 | `	{ "filter_list", "", "array" },` |
|         - |  856 | `	{ "filter_var", "mixed $value, int $filter = FILTER_DEFAULT, array\|int $options = 0", "mixed" },` |
|         - |  857 | `	{ "filter_var_array", "array $array, array\|int $options = FILTER_DEFAULT, bool $add_empty = true", "array\|false\|null" },` |
|         - |  858 | `	{ "floatval", "mixed $value", "float" },` |
|         - |  859 | `	{ "flock", "$stream, int $operation, &$would_block = NULL", "bool" },` |
|         - |  860 | `	{ "floor", "int\|float $num", "float" },` |
|         - |  861 | `	{ "flush", "", "void" },` |
|         - |  862 | `	{ "fmod", "float $num1, float $num2", "float" },` |
|         - |  863 | `	{ "fnmatch", "string $pattern, string $filename, int $flags = 0", "bool" },` |
|         - |  864 | `	{ "fopen", "string $filename, string $mode, bool $use_include_path = false, $context = NULL", "" },` |
|         - |  865 | `	{ "forward_static_call", "callable $callback, mixed ...$args = ?", "mixed" },` |
|         - |  866 | `	{ "forward_static_call_array", "callable $callback, array $args", "mixed" },` |
|         - |  867 | `	{ "fpow", "float $num, float $exponent", "float" },` |
|         - |  868 | `	{ "fpassthru", "$stream", "int" },` |
|         - |  869 | ``	/* `~string $format`: php resolves the STREAM first and refuses a closed one`` |
|         - |  870 | `	 * before it looks at the format at all, so the central screen stands aside` |
|         - |  871 | `	 * and PH7_FormatCheckFormatArg() in the body raises the same TypeError` |
|         - |  872 | `	 * after the handle has been accepted. */` |
|         - |  873 | `	{ "fprintf", "$stream, ~string $format, mixed ...$values = ?", "int" },` |
|         - |  874 | `	{ "fputcsv", "$stream, array $fields, string $separator = ',', string $enclosure = '\"', string $escape = '\\\\', string $eol = \"\n\"", "int\|false" },` |
|         - |  875 | `	{ "fputs", "$stream, string $data, ?int $length = NULL", "int\|false" },` |
|         - |  876 | `	{ "fread", "$stream, int $length", "string\|false" },` |
|         - |  877 | `	{ "frenchtojd", "int $month, int $day, int $year", "int" },` |
|         - |  878 | `	{ "fseek", "$stream, int $offset, int $whence = SEEK_SET", "int" },` |
|         - |  879 | `	{ "fstat", "$stream", "array\|false" },` |
|         - |  880 | `	{ "ftell", "$stream", "int\|false" },` |
|         - |  881 | `	{ "ftruncate", "$stream, int $size", "bool" },` |
|         - |  882 | `	{ "func_get_arg", "int $position", "mixed" },` |
|         - |  883 | `	{ "func_get_args", "", "array" },` |
|         - |  884 | `	{ "func_num_args", "", "int" },` |
|         - |  885 | `	{ "function_exists", "string $function", "bool" },` |
|         - |  886 | `	{ "fwrite", "$stream, string $data, ?int $length = NULL", "int\|false" },` |
|         - |  887 | `	{ "gc_collect_cycles", "", "int" },` |
|         - |  888 | `	{ "gc_disable", "", "void" },` |
|         - |  889 | `	{ "gc_enable", "", "void" },` |
|         - |  890 | `	{ "gc_enabled", "", "bool" },` |
|         - |  891 | `	{ "gc_mem_caches", "", "int" },` |
|         - |  892 | `	{ "gc_status", "", "array" },` |
|         - |  893 | `	{ "get_called_class", "", "string" },` |
|         - |  894 | `	/* ext/fileinfo */` |
|         - |  895 | `	{ "finfo_open", "int $flags = FILEINFO_NONE, ?string $magic_database = null", "finfo\|false" },` |
|         - |  896 | `	{ "finfo_close", "finfo $finfo", "true" },` |
|         - |  897 | `	{ "finfo_set_flags", "finfo $finfo, int $flags", "true" },` |
|         - |  898 | `	{ "finfo_file", "finfo $finfo, string $filename, int $flags = FILEINFO_NONE, $context = null", "string\|false" },` |
|         - |  899 | `	{ "finfo_buffer", "finfo $finfo, string $string, int $flags = FILEINFO_NONE, $context = null", "string\|false" },` |
|         - |  900 | `	{ "mime_content_type", "$filename", "string\|false" },` |
|         - |  901 | `	/* ext/zlib. The gz* handle verbs are ALIASES of the stream functions above` |
|         - |  902 | `	 * (php registers them that way, and so does this build), but each carries` |
|         - |  903 | `	 * its OWN signature row -- which is what makes gzread()'s ArgumentCountError` |
|         - |  904 | `	 * and its ValueError say "gzread()" rather than "fread()". */` |
|         - |  905 | `	{ "deflate_add", "DeflateContext $context, string $data, int $flush_mode = ZLIB_SYNC_FLUSH", "string\|false" },` |
|         - |  906 | `	{ "deflate_init", "int $encoding, object\|array $options = []", "DeflateContext\|false" },` |
|         - |  907 | `	{ "gzclose", "$stream", "bool" },` |
|         - |  908 | `	{ "gzcompress", "string $data, int $level = -1, int $encoding = ZLIB_ENCODING_DEFLATE", "string\|false" },` |
|         - |  909 | `	{ "gzdecode", "string $data, int $max_length = 0", "string\|false" },` |
|         - |  910 | `	{ "gzdeflate", "string $data, int $level = -1, int $encoding = ZLIB_ENCODING_RAW", "string\|false" },` |
|         - |  911 | `	{ "gzencode", "string $data, int $level = -1, int $encoding = ZLIB_ENCODING_GZIP", "string\|false" },` |
|         - |  912 | `	{ "gzeof", "$stream", "bool" },` |
|         - |  913 | `	{ "gzfile", "string $filename, bool $use_include_path = false", "array\|false" },` |
|         - |  914 | `	{ "gzgetc", "$stream", "string\|false" },` |
|         - |  915 | `	{ "gzgets", "$stream, ?int $length = NULL", "string\|false" },` |
|         - |  916 | `	{ "gzinflate", "string $data, int $max_length = 0", "string\|false" },` |
|         - |  917 | `	{ "gzopen", "string $filename, string $mode, bool $use_include_path = false", "" },` |
|         - |  918 | `	{ "gzpassthru", "$stream", "int" },` |
|         - |  919 | `	{ "gzputs", "$stream, string $data, ?int $length = NULL", "int\|false" },` |
|         - |  920 | `	{ "gzread", "$stream, int $length", "string\|false" },` |
|         - |  921 | `	{ "gzrewind", "$stream", "bool" },` |
|         - |  922 | `	{ "gzseek", "$stream, int $offset, int $whence = SEEK_SET", "int" },` |
|         - |  923 | `	{ "gztell", "$stream", "int\|false" },` |
|         - |  924 | `	{ "gzuncompress", "string $data, int $max_length = 0", "string\|false" },` |
|         - |  925 | `	{ "gzwrite", "$stream, string $data, ?int $length = NULL", "int\|false" },` |
|         - |  926 | `	{ "inflate_add", "InflateContext $context, string $data, int $flush_mode = ZLIB_SYNC_FLUSH", "string\|false" },` |
|         - |  927 | `	{ "inflate_get_read_len", "InflateContext $context", "int" },` |
|         - |  928 | `	{ "inflate_get_status", "InflateContext $context", "int" },` |
|         - |  929 | `	{ "inflate_init", "int $encoding, object\|array $options = []", "InflateContext\|false" },` |
|         - |  930 | `	{ "ob_gzhandler", "string $data, int $flags", "string\|false" },` |
|         - |  931 | `	{ "readgzfile", "string $filename, bool $use_include_path = false", "int\|false" },` |
|         - |  932 | `	{ "zlib_decode", "string $data, int $max_length = 0", "string\|false" },` |
|         - |  933 | `	{ "zlib_encode", "string $data, int $encoding, int $level = -1", "string\|false" },` |
|         - |  934 | `	{ "zlib_get_coding_type", "", "string\|false" },` |
|         - |  935 | `	/* ext/gettext */` |
|         - |  936 | `	{ "_", "string $message", "string" },` |
|         - |  937 | `	{ "bind_textdomain_codeset", "string $domain, ?string $codeset = NULL", "string\|false" },` |
|         - |  938 | `	{ "bindtextdomain", "string $domain, ?string $directory = NULL", "string\|false" },` |
|         - |  939 | `	{ "dcgettext", "string $domain, string $message, int $category", "string" },` |
|         - |  940 | `	{ "dcngettext", "string $domain, string $singular, string $plural, int $count, int $category", "string" },` |
|         - |  941 | `	{ "dgettext", "string $domain, string $message", "string" },` |
|         - |  942 | `	{ "dngettext", "string $domain, string $singular, string $plural, int $count", "string" },` |
|         - |  943 | `	{ "gettext", "string $message", "string" },` |
|         - |  944 | `	{ "ngettext", "string $singular, string $plural, int $count", "string" },` |
|         - |  945 | `	{ "textdomain", "?string $domain = NULL", "string" },` |
|         - |  946 | ``	/* ext/standard's syslog trio. All three answer `true` and nothing else --`` |
|         - |  947 | ``	 * php declares the return type as the literal `true`, not `bool`. */`` |
|         - |  948 | `	{ "openlog", "string $prefix, int $flags, int $facility", "true" },` |
|         - |  949 | `	{ "closelog", "", "true" },` |
|         - |  950 | `	{ "syslog", "int $priority, string $message", "true" },` |
|         - |  951 | `	/* ext/pcntl. Only reachable where the extension is built, but the table is` |
|         - |  952 | `	 * a DECLARATION rather than a registration -- Reflection filters it against` |
|         - |  953 | `	 * the live VM, so the rows cost nothing on Windows. */` |
|         - |  954 | `	{ "pcntl_alarm", "int $seconds", "int" },` |
|         - |  955 | `	{ "pcntl_async_signals", "?bool $enable = NULL", "bool" },` |
|         - |  956 | `	{ "pcntl_errno", "", "int" },` |
|         - |  957 | `	{ "pcntl_exec", "string $path, array $args = [], array $env_vars = []", "false" },` |
|         - |  958 | `	{ "pcntl_fork", "", "int" },` |
|         - |  959 | `	{ "pcntl_get_last_error", "", "int" },` |
|         - |  960 | `	{ "pcntl_getcpu", "", "int" },` |
|         - |  961 | ``	/* ext/sockets. `socket_export_stream()` is the one row with no return type`` |
|         - |  962 | `	 * at all: php declares none for it, so Reflection answers null. */` |
|         - |  963 | `	{ "socket_select", "?array &$read, ?array &$write, ?array &$except, ?int $seconds, int $microseconds = 0", "int\|false" },` |
|         - |  964 | `	/* SOMAXCONN, which is 4096 on Linux and 2147483647 on Windows -- so the` |
|         - |  965 | `	 * default is stated by NAME rather than as the number php prints here. */` |
|         - |  966 | `	{ "socket_create_listen", "int $port, int $backlog = SOMAXCONN", "Socket\|false" },` |
|         - |  967 | `	{ "socket_accept", "Socket $socket", "Socket\|false" },` |
|         - |  968 | `	{ "socket_set_nonblock", "Socket $socket", "bool" },` |
|         - |  969 | `	{ "socket_set_block", "Socket $socket", "bool" },` |
|         - |  970 | `	{ "socket_listen", "Socket $socket, int $backlog = 0", "bool" },` |
|         - |  971 | `	{ "socket_close", "Socket $socket", "void" },` |
|         - |  972 | `	{ "socket_write", "Socket $socket, string $data, ?int $length = null", "int\|false" },` |
|         - |  973 | `	{ "socket_read", "Socket $socket, int $length, int $mode = 2", "string\|false" },` |
|         - |  974 | `	{ "socket_getsockname", "Socket $socket, &$address, &$port = null", "bool" },` |
|         - |  975 | `	{ "socket_getpeername", "Socket $socket, &$address, &$port = null", "bool" },` |
|         - |  976 | `	{ "socket_create", "int $domain, int $type, int $protocol", "Socket\|false" },` |
|         - |  977 | `	{ "socket_connect", "Socket $socket, string $address, ?int $port = null", "bool" },` |
|         - |  978 | `	{ "socket_strerror", "int $error_code", "string" },` |
|         - |  979 | `	{ "socket_bind", "Socket $socket, string $address, int $port = 0", "bool" },` |
|         - |  980 | `	{ "socket_recv", "Socket $socket, &$data, int $length, int $flags", "int\|false" },` |
|         - |  981 | `	{ "socket_send", "Socket $socket, string $data, int $length, int $flags", "int\|false" },` |
|         - |  982 | `	{ "socket_recvfrom", "Socket $socket, &$data, int $length, int $flags, &$address, &$port = null", "int\|false" },` |
|         - |  983 | `	{ "socket_sendto", "Socket $socket, string $data, int $length, int $flags, string $address, ?int $port = null", "int\|false" },` |
|         - |  984 | `	{ "socket_get_option", "Socket $socket, int $level, int $option", "array\|int\|false" },` |
|         - |  985 | `	{ "socket_getopt", "Socket $socket, int $level, int $option", "array\|int\|false" },` |
|         - |  986 | `	{ "socket_set_option", "Socket $socket, int $level, int $option, $value", "bool" },` |
|         - |  987 | `	{ "socket_setopt", "Socket $socket, int $level, int $option, $value", "bool" },` |
|         - |  988 | `	{ "socket_create_pair", "int $domain, int $type, int $protocol, &$pair", "bool" },` |
|         - |  989 | `	{ "socket_shutdown", "Socket $socket, int $mode = 2", "bool" },` |
|         - |  990 | `	{ "socket_atmark", "Socket $socket", "bool" },` |
|         - |  991 | `	{ "socket_last_error", "?Socket $socket = null", "int" },` |
|         - |  992 | `	{ "socket_clear_error", "?Socket $socket = null", "void" },` |
|         - |  993 | `	{ "socket_import_stream", "$stream", "Socket\|false" },` |
|         - |  994 | `	{ "socket_export_stream", "Socket $socket", "" },` |
|         - |  995 | `	{ "socket_sendmsg", "Socket $socket, array $message, int $flags = 0", "int\|false" },` |
|         - |  996 | `	{ "socket_recvmsg", "Socket $socket, array &$message, int $flags = 0", "int\|false" },` |
|         - |  997 | `	{ "socket_cmsg_space", "int $level, int $type, int $num = 0", "?int" },` |
|         - |  998 | `	{ "socket_addrinfo_lookup", "string $host, ?string $service = null, array $hints = []", "array\|false" },` |
|         - |  999 | `	{ "socket_addrinfo_connect", "AddressInfo $address", "Socket\|false" },` |
|         - | 1000 | `	{ "socket_addrinfo_bind", "AddressInfo $address", "Socket\|false" },` |
|         - | 1001 | `	{ "socket_addrinfo_explain", "AddressInfo $address", "array" },` |
|         - | 1002 | `	/* Windows only; the rows are harmless on a build that registers no such` |
|         - | 1003 | `	 * name, since the stamping pass filters against the live VM. */` |
|         - | 1004 | `	{ "socket_wsaprotocol_info_export", "Socket $socket, int $process_id", "string\|false" },` |
|         - | 1005 | `	{ "socket_wsaprotocol_info_import", "string $info_id", "Socket\|false" },` |
|         - | 1006 | `	{ "socket_wsaprotocol_info_release", "string $info_id", "bool" },` |
|         - | 1007 | `	{ "pcntl_getcpuaffinity", "?int $process_id = NULL", "array\|false" },` |
|         - | 1008 | `	{ "pcntl_getpriority", "?int $process_id = NULL, int $mode = PRIO_PROCESS", "int\|false" },` |
|         - | 1009 | `	{ "pcntl_setcpuaffinity", "?int $process_id = NULL, array $cpu_ids = []", "bool" },` |
|         - | 1010 | `	{ "pcntl_setpriority", "int $priority, ?int $process_id = NULL, int $mode = PRIO_PROCESS", "bool" },` |
|         - | 1011 | `	{ "pcntl_signal", "int $signal, $handler, bool $restart_syscalls = true", "bool" },` |
|         - | 1012 | `	{ "pcntl_signal_dispatch", "", "bool" },` |
|         - | 1013 | `	{ "pcntl_signal_get_handler", "int $signal", "" },` |
|         - | 1014 | `	{ "pcntl_sigprocmask", "int $mode, array $signals, &$old_signals = NULL", "bool" },` |
|         - | 1015 | `	{ "pcntl_sigtimedwait", "array $signals, &$info = [], int $seconds = 0, int $nanoseconds = 0", "int\|false" },` |
|         - | 1016 | `	{ "pcntl_sigwaitinfo", "array $signals, &$info = []", "int\|false" },` |
|         - | 1017 | `	{ "pcntl_strerror", "int $error_code", "string" },` |
|         - | 1018 | `	{ "pcntl_unshare", "int $flags", "bool" },` |
|         - | 1019 | `	{ "pcntl_wait", "&$status, int $flags = 0, &$resource_usage = []", "int" },` |
|         - | 1020 | `	{ "pcntl_waitid", "int $idtype = P_ALL, ?int $id = NULL, &$info = [], int $flags = WEXITED, &$resource_usage = []", "bool" },` |
|         - | 1021 | `	{ "pcntl_waitpid", "int $process_id, &$status, int $flags = 0, &$resource_usage = []", "int" },` |
|         - | 1022 | `	{ "pcntl_wexitstatus", "int $status", "int\|false" },` |
|         - | 1023 | `	{ "pcntl_wifcontinued", "int $status", "bool" },` |
|         - | 1024 | `	{ "pcntl_wifexited", "int $status", "bool" },` |
|         - | 1025 | `	{ "pcntl_wifsignaled", "int $status", "bool" },` |
|         - | 1026 | `	{ "pcntl_wifstopped", "int $status", "bool" },` |
|         - | 1027 | `	{ "pcntl_wstopsig", "int $status", "int\|false" },` |
|         - | 1028 | `	{ "pcntl_wtermsig", "int $status", "int\|false" },` |
|         - | 1029 | `	/* ext/posix */` |
|         - | 1030 | `	{ "posix_access", "string $filename, int $flags = 0", "bool" },` |
|         - | 1031 | `	{ "posix_ctermid", "", "string\|false" },` |
|         - | 1032 | `	{ "posix_eaccess", "string $filename, int $flags = 0", "bool" },` |
|         - | 1033 | `	{ "posix_errno", "", "int" },` |
|         - | 1034 | `	{ "posix_fpathconf", "$file_descriptor, int $name", "int\|false" },` |
|         - | 1035 | `	{ "posix_get_last_error", "", "int" },` |
|         - | 1036 | `	{ "posix_getcwd", "", "string\|false" },` |
|         - | 1037 | `	{ "posix_getegid", "", "int" },` |
|         - | 1038 | `	{ "posix_geteuid", "", "int" },` |
|         - | 1039 | `	{ "posix_getgid", "", "int" },` |
|         - | 1040 | `	{ "posix_getgrgid", "int $group_id", "array\|false" },` |
|         - | 1041 | `	{ "posix_getgrnam", "string $name", "array\|false" },` |
|         - | 1042 | `	{ "posix_getgroups", "", "array\|false" },` |
|         - | 1043 | `	{ "posix_getlogin", "", "string\|false" },` |
|         - | 1044 | `	{ "posix_getpgid", "int $process_id", "int\|false" },` |
|         - | 1045 | `	{ "posix_getpgrp", "", "int" },` |
|         - | 1046 | `	{ "posix_getpid", "", "int" },` |
|         - | 1047 | `	{ "posix_getppid", "", "int" },` |
|         - | 1048 | `	{ "posix_getpwnam", "string $username", "array\|false" },` |
|         - | 1049 | `	{ "posix_getpwuid", "int $user_id", "array\|false" },` |
|         - | 1050 | `	{ "posix_getrlimit", "?int $resource = NULL", "array\|false" },` |
|         - | 1051 | `	{ "posix_getsid", "int $process_id", "int\|false" },` |
|         - | 1052 | `	{ "posix_getuid", "", "int" },` |
|         - | 1053 | `	{ "posix_initgroups", "string $username, int $group_id", "bool" },` |
|         - | 1054 | `	{ "posix_isatty", "$file_descriptor", "bool" },` |
|         - | 1055 | `	{ "posix_kill", "int $process_id, int $signal", "bool" },` |
|         - | 1056 | `	{ "posix_mkfifo", "string $filename, int $permissions", "bool" },` |
|         - | 1057 | `	{ "posix_mknod", "string $filename, int $flags, int $major = 0, int $minor = 0", "bool" },` |
|         - | 1058 | `	{ "posix_pathconf", "string $path, int $name", "int\|false" },` |
|         - | 1059 | `	{ "posix_setegid", "int $group_id", "bool" },` |
|         - | 1060 | `	{ "posix_seteuid", "int $user_id", "bool" },` |
|         - | 1061 | `	{ "posix_setgid", "int $group_id", "bool" },` |
|         - | 1062 | `	{ "posix_setpgid", "int $process_id, int $process_group_id", "bool" },` |
|         - | 1063 | `	{ "posix_setrlimit", "int $resource, int $soft_limit, int $hard_limit", "bool" },` |
|         - | 1064 | `	{ "posix_setsid", "", "int" },` |
|         - | 1065 | `	{ "posix_setuid", "int $user_id", "bool" },` |
|         - | 1066 | `	{ "posix_strerror", "int $error_code", "string" },` |
|         - | 1067 | `	{ "posix_sysconf", "int $conf_id", "int" },` |
|         - | 1068 | `	{ "posix_times", "", "array\|false" },` |
|         - | 1069 | `	{ "posix_ttyname", "$file_descriptor", "string\|false" },` |
|         - | 1070 | `	{ "posix_uname", "", "array\|false" },` |
|         - | 1071 | `	{ "get_class", "object $object = ?", "string" },` |
|         - | 1072 | `	{ "get_class_methods", "object\|string $object_or_class", "array" },` |
|         - | 1073 | `	{ "get_class_vars", "string $class", "array" },` |
|         - | 1074 | `	{ "get_current_user", "", "string" },` |
|         - | 1075 | `	{ "get_declared_classes", "", "array" },` |
|         - | 1076 | `	{ "get_declared_interfaces", "", "array" },` |
|         - | 1077 | `	{ "get_declared_traits", "", "array" },` |
|         - | 1078 | `	{ "get_defined_constants", "bool $categorize = false", "array" },` |
|         - | 1079 | `	{ "get_defined_functions", "bool $exclude_disabled = true", "array" },` |
|         - | 1080 | `	{ "get_defined_vars", "", "array" },` |
|         - | 1081 | `	{ "get_headers", "string $url, bool $associative = false, $context = NULL", "array\|false" },` |
|         - | 1082 | `	{ "get_html_translation_table", "int $table = HTML_SPECIALCHARS, int $flags = ENT_QUOTES \| ENT_SUBSTITUTE \| ENT_HTML401, string $encoding = 'UTF-8'", "array" },` |
|         - | 1083 | `	{ "get_include_path", "", "string\|false" },` |
|         - | 1084 | `	{ "get_included_files", "", "array" },` |
|         - | 1085 | `	{ "get_required_files", "", "array" },` |
|         - | 1086 | `	{ "get_extension_funcs", "string $extension", "array\|false" },` |
|         - | 1087 | `	{ "get_loaded_extensions", "bool $zend_extensions = false", "array" },` |
|         - | 1088 | `	{ "get_mangled_object_vars", "object $object", "array" },` |
|         - | 1089 | `	{ "get_object_vars", "object $object", "array" },` |
|         - | 1090 | `	{ "get_parent_class", "object\|string $object_or_class = ?", "string\|false" },` |
|         - | 1091 | `	{ "get_resource_id", "$resource", "int" },` |
|         - | 1092 | `	{ "get_resource_type", "$resource", "string" },` |
|         - | 1093 | `	{ "getcwd", "", "string\|false" },` |
|         - | 1094 | `	{ "getdate", "?int $timestamp = NULL", "array" },` |
|         - | 1095 | `	{ "getenv", "?string $name = NULL, bool $local_only = false", "array\|string\|false" },` |
|         - | 1096 | `	{ "gethostname", "", "string\|false" },` |
|         - | 1097 | `	{ "getimagesize", "string $filename, &$image_info = NULL", "array\|false" },` |
|         - | 1098 | `	{ "getimagesizefromstring", "string $string, &$image_info = NULL", "array\|false" },` |
|         - | 1099 | `	{ "getmygid", "", "int\|false" },` |
|         - | 1100 | `	{ "getmypid", "", "int\|false" },` |
|         - | 1101 | `	{ "getmyuid", "", "int\|false" },` |
|         - | 1102 | `	{ "getopt", "string $short_options, array $long_options = [], &$rest_index = NULL", "array\|false" },` |
|         - | 1103 | `	{ "getrandmax", "", "int" },` |
|         - | 1104 | `	{ "gettimeofday", "bool $as_float = false", "array\|float" },` |
|         - | 1105 | `	{ "gettype", "mixed $value", "string" },` |
|         - | 1106 | `	{ "get_debug_type", "mixed $value", "string" },` |
|         - | 1107 | `	{ "gmdate", "string $format, ?int $timestamp = NULL", "string" },` |
|         - | 1108 | `	{ "gmmktime", "int $hour, ?int $minute = NULL, ?int $second = NULL, ?int $month = NULL, ?int $day = NULL, ?int $year = NULL", "int\|false" },` |
|         - | 1109 | `	{ "hash", "string $algo, string $data, bool $binary = false, array $options = []", "string" },` |
|         - | 1110 | `	{ "hash_algos", "", "array" },` |
|         - | 1111 | `	{ "hash_equals", "string $known_string, string $user_string", "bool" },` |
|         - | 1112 | `	{ "hash_hmac", "string $algo, string $data, string $key, bool $binary = false", "string" },` |
|         - | 1113 | `	{ "hash_hmac_algos", "", "array" },` |
|         - | 1114 | `	{ "hash_init", "string $algo, int $flags = 0, string $key = \'\', array $options = []", "HashContext" },` |
|         - | 1115 | `	{ "hash_update", "HashContext $context, string $data", "true" },` |
|         - | 1116 | `	{ "hash_final", "HashContext $context, bool $binary = false", "string" },` |
|         - | 1117 | `	{ "hash_copy", "HashContext $context", "HashContext" },` |
|         - | 1118 | `	{ "hash_file", "string $algo, string $filename, bool $binary = false, array $options = []", "string\|false" },` |
|         - | 1119 | `	{ "hash_hkdf", "string $algo, string $key, int $length = 0, string $info = \'\', string $salt = \'\'", "string" },` |
|         - | 1120 | `	{ "hash_pbkdf2", "string $algo, string $password, string $salt, int $iterations, int $length = 0, bool $binary = false, array $options = []", "string" },` |
|         - | 1121 | `	{ "hash_hmac_file", "string $algo, string $filename, string $key, bool $binary = false", "string\|false" },` |
|         - | 1122 | `	{ "hash_update_file", "HashContext $context, string $filename, $stream_context = null", "bool" },` |
|         - | 1123 | `	{ "hash_update_stream", "HashContext $context, $stream, int $length = -1", "int" },` |
|         - | 1124 | `	{ "gregoriantojd", "int $month, int $day, int $year", "int" },` |
|         - | 1125 | `	{ "header", "string $header, bool $replace = true, int $response_code = 0", "void" },` |
|         - | 1126 | `	{ "header_remove", "?string $name = NULL", "void" },` |
|         - | 1127 | `	{ "headers_list", "", "array" },` |
|         - | 1128 | `	{ "headers_sent", "&$filename = NULL, &$line = NULL", "bool" },` |
|         - | 1129 | `	{ "hexdec", "string $hex_string", "int\|float" },` |
|         - | 1130 | `	{ "html_entity_decode", "string $string, int $flags = ENT_QUOTES \| ENT_SUBSTITUTE \| ENT_HTML401, ?string $encoding = NULL", "string" },` |
|         - | 1131 | `	{ "http_build_query", "object\|array $data, string $numeric_prefix = '', ?string $arg_separator = null, int $encoding_type = PHP_QUERY_RFC1738", "string" },` |
|         - | 1132 | `	{ "htmlentities", "string $string, int $flags = ENT_QUOTES \| ENT_SUBSTITUTE \| ENT_HTML401, ?string $encoding = NULL, bool $double_encode = true", "string" },` |
|         - | 1133 | `	{ "htmlspecialchars", "string $string, int $flags = ENT_QUOTES \| ENT_SUBSTITUTE \| ENT_HTML401, ?string $encoding = NULL, bool $double_encode = true", "string" },` |
|         - | 1134 | `	{ "htmlspecialchars_decode", "string $string, int $flags = ENT_QUOTES \| ENT_SUBSTITUTE \| ENT_HTML401", "string" },` |
|         - | 1135 | `	{ "http_clear_last_response_headers", "", "void" },` |
|         - | 1136 | `	{ "http_get_last_response_headers", "", "?array" },` |
|         - | 1137 | `	{ "http_response_code", "int $response_code = 0", "int\|bool" },` |
|         - | 1138 | `	{ "hypot", "float $x, float $y", "float" },` |
|         - | 1139 | `	{ "idate", "string $format, ?int $timestamp = NULL", "int\|false" },` |
|         - | 1140 | `	{ "ignore_user_abort", "?bool $enable = NULL", "int" },` |
|         - | 1141 | `	{ "image_type_to_mime_type", "int $image_type", "string" },` |
|         - | 1142 | `	{ "image_type_to_extension", "int $image_type, bool $include_dot = true", "string\|false" },` |
|         - | 1143 | `	{ "implode", "array\|string $separator, ?array $array = NULL", "string" },` |
|         - | 1144 | `	{ "in_array", "mixed $needle, array $haystack, bool $strict = false", "bool" },` |
|         - | 1145 | `	{ "inet_ntop", "string $ip", "string\|false" },` |
|         - | 1146 | `	{ "inet_pton", "string $ip", "string\|false" },` |
|         - | 1147 | `	{ "intdiv", "int $num1, int $num2", "int" },` |
|         - | 1148 | `	{ "interface_exists", "string $interface, bool $autoload = true", "bool" },` |
|         - | 1149 | `	{ "trait_exists", "string $trait, bool $autoload = true", "bool" },` |
|         - | 1150 | `	{ "intval", "mixed $value, int $base = 10", "int" },` |
|         - | 1151 | `	{ "is_a", "mixed $object_or_class, string $class, bool $allow_string = false", "bool" },` |
|         - | 1152 | `	{ "is_array", "mixed $value", "bool" },` |
|         - | 1153 | `	{ "is_bool", "mixed $value", "bool" },` |
|         - | 1154 | `	{ "is_callable", "mixed $value, bool $syntax_only = false, &$callable_name = NULL", "bool" },` |
|         - | 1155 | `	{ "is_dir", "string $filename", "bool" },` |
|         - | 1156 | `	{ "is_double", "mixed $value", "bool" },` |
|         - | 1157 | `	{ "is_executable", "string $filename", "bool" },` |
|         - | 1158 | `	{ "is_file", "string $filename", "bool" },` |
|         - | 1159 | `	{ "is_float", "mixed $value", "bool" },` |
|         - | 1160 | `	{ "is_int", "mixed $value", "bool" },` |
|         - | 1161 | `	{ "is_integer", "mixed $value", "bool" },` |
|         - | 1162 | `	{ "is_link", "string $filename", "bool" },` |
|         - | 1163 | `	{ "is_long", "mixed $value", "bool" },` |
|         - | 1164 | `	{ "is_null", "mixed $value", "bool" },` |
|         - | 1165 | `	{ "is_numeric", "mixed $value", "bool" },` |
|         - | 1166 | `	{ "is_object", "mixed $value", "bool" },` |
|         - | 1167 | `	{ "is_readable", "string $filename", "bool" },` |
|         - | 1168 | `	{ "is_resource", "mixed $value", "bool" },` |
|         - | 1169 | `	{ "is_scalar", "mixed $value", "bool" },` |
|         - | 1170 | `	{ "is_string", "mixed $value", "bool" },` |
|         - | 1171 | `	{ "is_subclass_of", "mixed $object_or_class, string $class, bool $allow_string = true", "bool" },` |
|         - | 1172 | `	{ "is_writable", "string $filename", "bool" },` |
|         - | 1173 | `	{ "is_writeable", "string $filename", "bool" },` |
|         - | 1174 | `	{ "iterator_apply", "Traversable $iterator, callable $callback, ?array $args = NULL", "int" },` |
|         - | 1175 | `	{ "iterator_count", "Traversable\|array $iterator", "int" },` |
|         - | 1176 | `	{ "iterator_to_array", "Traversable\|array $iterator, bool $preserve_keys = true", "array" },` |
|         - | 1177 | `	{ "jddayofweek", "int $julian_day, int $mode = CAL_DOW_DAYNO", "string\|int" },` |
|         - | 1178 | `	{ "jdmonthname", "int $julian_day, int $mode", "string" },` |
|         - | 1179 | `	{ "jdtofrench", "int $julian_day", "string" },` |
|         - | 1180 | `	{ "jdtogregorian", "int $julian_day", "string" },` |
|         - | 1181 | `	{ "jdtojewish", "int $julian_day, bool $hebrew = false, int $flags = 0", "string" },` |
|         - | 1182 | `	{ "jdtojulian", "int $julian_day", "string" },` |
|         - | 1183 | `	{ "jdtounix", "int $julian_day", "int" },` |
|         - | 1184 | `	{ "jewishtojd", "int $month, int $day, int $year", "int" },` |
|         - | 1185 | `	{ "join", "array\|string $separator, ?array $array = NULL", "string" },` |
|         - | 1186 | `	{ "juliantojd", "int $month, int $day, int $year", "int" },` |
|         - | 1187 | `	{ "json_decode", "string $json, ?bool $associative = NULL, int $depth = 512, int $flags = 0", "mixed" },` |
|         - | 1188 | `	{ "json_encode", "mixed $value, int $flags = 0, int $depth = 512", "string\|false" },` |
|         - | 1189 | `	{ "json_last_error", "", "int" },` |
|         - | 1190 | `	{ "json_last_error_msg", "", "string" },` |
|         - | 1191 | `	{ "json_validate", "string $json, int $depth = 512, int $flags = 0", "bool" },` |
|         - | 1192 | `	{ "key", "object\|array $array", "string\|int\|null" },` |
|         - | 1193 | `	{ "key_exists", "$key, array $array", "bool" },` |
|         - | 1194 | `	{ "krsort", "array &$array, int $flags = SORT_REGULAR", "true" },` |
|         - | 1195 | `	{ "ksort", "array &$array, int $flags = SORT_REGULAR", "true" },` |
|         - | 1196 | `	{ "lcfirst", "string $string", "string" },` |
|         - | 1197 | `	{ "levenshtein", "string $string1, string $string2, int $insertion_cost = 1, int $replacement_cost = 1, int $deletion_cost = 1", "int" },` |
|         - | 1198 | `	{ "link", "string $target, string $link", "bool" },` |
|         - | 1199 | `	{ "localtime", "?int $timestamp = NULL, bool $associative = false", "array" },` |
|         - | 1200 | `	{ "log", "float $num, float $base = M_E", "float" },` |
|         - | 1201 | `	{ "log10", "float $num", "float" },` |
|         - | 1202 | `	{ "log1p", "float $num", "float" },` |
|         - | 1203 | `	{ "lstat", "string $filename", "array\|false" },` |
|         - | 1204 | `	{ "ltrim", "string $string, string $characters = \" \\n\\r\\t\\v\\x00\"", "string" },` |
|         - | 1205 | `	{ "max", "mixed $value, mixed ...$values = ?", "mixed" },` |
|         - | 1206 | `	{ "mb_chr", "int $codepoint, ?string $encoding = NULL", "string\|false" },` |
|         - | 1207 | `	{ "mb_convert_encoding", "array\|string $string, string $to_encoding, array\|string\|null $from_encoding = NULL", "array\|string\|false" },` |
|         - | 1208 | `	{ "mb_ord", "string $string, ?string $encoding = NULL", "int\|false" },` |
|         - | 1209 | `	{ "mb_ltrim", "string $string, ?string $characters = null, ?string $encoding = null", "string" },` |
|         - | 1210 | `	{ "mb_rtrim", "string $string, ?string $characters = null, ?string $encoding = null", "string" },` |
|         - | 1211 | `	{ "mb_lcfirst", "string $string, ?string $encoding = null", "string" },` |
|         - | 1212 | `	{ "mb_strtolower", "string $string, ?string $encoding = NULL", "string" },` |
|         - | 1213 | `	{ "mb_strtoupper", "string $string, ?string $encoding = NULL", "string" },` |
|         - | 1214 | `	{ "mb_trim", "string $string, ?string $characters = null, ?string $encoding = null", "string" },` |
|         - | 1215 | `	{ "mb_ucfirst", "string $string, ?string $encoding = null", "string" },` |
|         - | 1216 | `	{ "iconv", "string $from_encoding, string $to_encoding, string $string", "string\|false" },` |
|         - | 1217 | `	{ "iconv_strlen", "string $string, ?string $encoding = NULL", "int\|false" },` |
|         - | 1218 | `	{ "iconv_substr", "string $string, int $offset, ?int $length = NULL, ?string $encoding = NULL", "string\|false" },` |
|         - | 1219 | `	{ "iconv_strpos", "string $haystack, string $needle, int $offset = 0, ?string $encoding = NULL", "int\|false" },` |
|         - | 1220 | `	{ "iconv_strrpos", "string $haystack, string $needle, ?string $encoding = NULL", "int\|false" },` |
|         - | 1221 | `	{ "iconv_get_encoding", "string $type = \"all\"", "array\|string\|false" },` |
|         - | 1222 | `	{ "iconv_mime_encode", "string $field_name, string $field_value, array $options = []", "string\|false" },` |
|         - | 1223 | `	{ "iconv_mime_decode", "string $string, int $mode = 0, ?string $encoding = NULL", "string\|false" },` |
|         - | 1224 | `	{ "iconv_mime_decode_headers", "string $headers, int $mode = 0, ?string $encoding = NULL", "array\|false" },` |
|         - | 1225 | `	{ "md5", "string $string, bool $binary = false", "string" },` |
|         - | 1226 | `	{ "md5_file", "string $filename, bool $binary = false", "string\|false" },` |
|         - | 1227 | `	{ "metaphone", "string $string, int $max_phonemes = 0", "string" },` |
|         - | 1228 | `	{ "method_exists", "$object_or_class, string $method", "bool" },` |
|         - | 1229 | `	{ "memory_get_peak_usage", "bool $real_usage = false", "int" },` |
|         - | 1230 | `	{ "memory_get_usage", "bool $real_usage = false", "int" },` |
|         - | 1231 | `	{ "microtime", "bool $as_float = false", "string\|float" },` |
|         - | 1232 | `	{ "min", "mixed $value, mixed ...$values = ?", "mixed" },` |
|         - | 1233 | `	{ "mkdir", "string $directory, int $permissions = 0777, bool $recursive = false, $context = NULL", "bool" },` |
|         - | 1234 | `	{ "mktime", "int $hour, ?int $minute = NULL, ?int $second = NULL, ?int $month = NULL, ?int $day = NULL, ?int $year = NULL", "int\|false" },` |
|         - | 1235 | `	{ "mt_getrandmax", "", "int" },` |
|         - | 1236 | `	{ "mt_rand", "int $min = ?, int $max = ?", "int" },` |
|         - | 1237 | `	{ "mt_srand", "?int $seed = NULL, int $mode = MT_RAND_MT19937", "void" },` |
|         - | 1238 | `	{ "natcasesort", "array &$array", "true" },` |
|         - | 1239 | `	{ "natsort", "array &$array", "true" },` |
|         - | 1240 | `	{ "next", "object\|array &$array", "mixed" },` |
|         - | 1241 | `	{ "nl2br", "string $string, bool $use_xhtml = true", "string" },` |
|         - | 1242 | `	{ "number_format", "float $num, int $decimals = 0, ?string $decimal_separator = '.', ?string $thousands_separator = ','", "string" },` |
|         - | 1243 | `	{ "ob_clean", "", "bool" },` |
|         - | 1244 | `	{ "ob_end_clean", "", "bool" },` |
|         - | 1245 | `	{ "ob_end_flush", "", "bool" },` |
|         - | 1246 | `	{ "ob_flush", "", "bool" },` |
|         - | 1247 | `	{ "ob_get_clean", "", "string\|false" },` |
|         - | 1248 | `	{ "ob_get_contents", "", "string\|false" },` |
|         - | 1249 | `	{ "ob_get_flush", "", "string\|false" },` |
|         - | 1250 | `	{ "ob_get_length", "", "int\|false" },` |
|         - | 1251 | `	{ "ob_get_level", "", "int" },` |
|         - | 1252 | `	{ "ob_get_status", "bool $full_status = false", "array" },` |
|         - | 1253 | `	{ "ob_implicit_flush", "bool $enable = true", "void" },` |
|         - | 1254 | `	{ "ob_list_handlers", "", "array" },` |
|         - | 1255 | `	{ "ob_start", "$callback = NULL, int $chunk_size = 0, int $flags = PHP_OUTPUT_HANDLER_STDFLAGS", "bool" },` |
|         - | 1256 | `	{ "octdec", "string $octal_string", "int\|float" },` |
|         - | 1257 | `	{ "opendir", "string $directory, $context = NULL", "" },` |
|         - | 1258 | `	{ "ord", "string $character", "int" },` |
|         - | 1259 | `	{ "pack", "string $format, mixed ...$values = ?", "string" },` |
|         - | 1260 | `	{ "sscanf", "string $string, string $format, mixed &...$vars = ?", "array\|int\|null" },` |
|         - | 1261 | `	{ "fscanf", "$stream, string $format, mixed &...$vars = ?", "array\|int\|false\|null" },` |
|         - | 1262 | `	{ "unpack", "string $format, string $string, int $offset = 0", "array\|false" },` |
|         - | 1263 | `	{ "parse_ini_file", "string $filename, bool $process_sections = false, int $scanner_mode = INI_SCANNER_NORMAL", "array\|false" },` |
|         - | 1264 | `	{ "parse_ini_string", "string $ini_string, bool $process_sections = false, int $scanner_mode = INI_SCANNER_NORMAL", "array\|false" },` |
|         - | 1265 | `	{ "parse_str", "string $string, &$result", "void" },` |
|         - | 1266 | `	{ "parse_url", "string $url, int $component = -1", "array\|string\|int\|false\|null" },` |
|         - | 1267 | `	{ "crypt", "string $string, string $salt", "string" },` |
|         - | 1268 | `	{ "password_algos", "", "array" },` |
|         - | 1269 | `	{ "password_get_info", "string $hash", "array" },` |
|         - | 1270 | `	{ "password_hash", "string $password, string\|int\|null $algo, array $options = []", "string" },` |
|         - | 1271 | `	{ "password_needs_rehash", "string $hash, string\|int\|null $algo, array $options = []", "bool" },` |
|         - | 1272 | `	{ "password_verify", "string $password, string $hash", "bool" },` |
|         - | 1273 | `	{ "passthru", "string $command, &$result_code = NULL", "?false" },` |
|         - | 1274 | `	{ "pathinfo", "string $path, int $flags = PATHINFO_ALL", "array\|string" },` |
|         - | 1275 | `	{ "pclose", "$handle", "int" },` |
|         - | 1276 | `	{ "php_sapi_name", "", "string\|false" },` |
|         - | 1277 | `	{ "php_ini_loaded_file", "", "string\|false" },` |
|         - | 1278 | `	{ "php_ini_scanned_files", "", "string\|false" },` |
|         - | 1279 | `	{ "php_uname", "string $mode = 'a'", "string" },` |
|         - | 1280 | ``	/* php spells this default `INFO_ALL`, and this engine spells the NUMBER: the`` |
|         - | 1281 | `	 * INFO_* family has no consumer here, since phpinfo() ignores $flags and` |
|         - | 1282 | `	 * prints the whole page whatever it is given. The names ship with the` |
|         - | 1283 | `	 * section filter or not at all (recorded). */` |
|         - | 1284 | `	{ "phpinfo", "int $flags = 4294967295", "true" },` |
|         - | 1285 | `	{ "phpversion", "?string $extension = NULL", "string\|false" },` |
|         - | 1286 | `	{ "pi", "", "float" },` |
|         - | 1287 | `	{ "popen", "string $command, string $mode", "" },` |
|         - | 1288 | `	{ "pos", "object\|array $array", "mixed" },` |
|         - | 1289 | `	{ "pow", "mixed $num, mixed $exponent", "object\|int\|float" },` |
|         - | 1290 | `	{ "preg_last_error", "", "int" },` |
|         - | 1291 | `	{ "preg_last_error_msg", "", "string" },` |
|         - | 1292 | `	{ "fsockopen", "string $hostname, int $port = -1, &$error_code = NULL, &$error_message = NULL, ?float $timeout = NULL", "" },` |
|         - | 1293 | `	{ "pfsockopen", "string $hostname, int $port = -1, &$error_code = NULL, &$error_message = NULL, ?float $timeout = NULL", "" },` |
|         - | 1294 | `	{ "stream_socket_client", "string $address, &$error_code = NULL, &$error_message = NULL, ?float $timeout = NULL, int $flags = STREAM_CLIENT_CONNECT, $context = NULL", "" },` |
|         - | 1295 | `	{ "stream_socket_enable_crypto", "$stream, bool $enable, ?int $crypto_method = NULL, $session_stream = NULL", "int\|bool" },` |
|         - | 1296 | `	{ "stream_socket_server", "string $address, &$error_code = NULL, &$error_message = NULL, int $flags = STREAM_SERVER_BIND \| STREAM_SERVER_LISTEN, $context = NULL", "" },` |
|         - | 1297 | `	{ "stream_socket_accept", "$socket, ?float $timeout = NULL, &$peer_name = NULL", "" },` |
|         - | 1298 | `	{ "stream_socket_get_name", "$socket, bool $remote", "string\|false" },` |
|         - | 1299 | `	{ "stream_socket_pair", "int $domain, int $type, int $protocol", "array\|false" },` |
|         - | 1300 | `	{ "stream_isatty", "$stream", "bool" },` |
|         - | 1301 | `	{ "stream_socket_shutdown", "$stream, int $mode", "bool" },` |
|         - | 1302 | `	{ "stream_socket_recvfrom", "$socket, int $length, int $flags = 0, &$address = NULL", "string\|false" },` |
|         - | 1303 | `	{ "stream_socket_sendto", "$socket, string $data, int $flags = 0, string $address = ''", "int\|false" },` |
|         - | 1304 | `	{ "preg_filter", "array\|string $pattern, array\|string $replacement, array\|string $subject, int $limit = -1, &$count = NULL", "array\|string\|null" },` |
|         - | 1305 | `	{ "preg_grep", "string $pattern, array $array, int $flags = 0", "array\|false" },` |
|         - | 1306 | `	{ "preg_match", "string $pattern, string $subject, &$matches = NULL, int $flags = 0, int $offset = 0", "int\|false" },` |
|         - | 1307 | `	{ "preg_match_all", "string $pattern, string $subject, &$matches = NULL, int $flags = 0, int $offset = 0", "int\|false" },` |
|         - | 1308 | `	{ "preg_quote", "string $str, ?string $delimiter = NULL", "string" },` |
|         - | 1309 | `	{ "preg_replace", "array\|string $pattern, array\|string $replacement, array\|string $subject, int $limit = -1, &$count = NULL", "array\|string\|null" },` |
|         - | 1310 | `	{ "preg_replace_callback", "array\|string $pattern, callable $callback, array\|string $subject, int $limit = -1, &$count = NULL, int $flags = 0", "array\|string\|null" },` |
|         - | 1311 | `	{ "preg_replace_callback_array", "array $pattern, array\|string $subject, int $limit = -1, &$count = NULL, int $flags = 0", "array\|string\|null" },` |
|         - | 1312 | `	{ "preg_split", "string $pattern, string $subject, int $limit = -1, int $flags = 0", "array\|false" },` |
|         - | 1313 | `	{ "prev", "object\|array &$array", "mixed" },` |
|         - | 1314 | `	{ "print_r", "mixed $value, bool $return = false", "string\|true" },` |
|         - | 1315 | `	{ "printf", "string $format, mixed ...$values = ?", "int" },` |
|         - | 1316 | `	{ "property_exists", "$object_or_class, string $property", "bool" },` |
|         - | 1317 | `	{ "putenv", "string $assignment", "bool" },` |
|         - | 1318 | `	{ "quoted_printable_decode", "string $string", "string" },` |
|         - | 1319 | `	{ "quoted_printable_encode", "string $string", "string" },` |
|         - | 1320 | `	{ "quotemeta", "string $string", "string" },` |
|         - | 1321 | `	{ "rad2deg", "float $num", "float" },` |
|         - | 1322 | `	{ "rand", "int $min = ?, int $max = ?", "int" },` |
|         - | 1323 | `	{ "random_bytes", "int $length", "string" },` |
|         - | 1324 | `	{ "random_int", "int $min, int $max", "int" },` |
|         - | 1325 | `	{ "range", "string\|int\|float $start, string\|int\|float $end, int\|float $step = 1", "array" },` |
|         - | 1326 | `	{ "rawurldecode", "string $string", "string" },` |
|         - | 1327 | `	{ "rawurlencode", "string $string", "string" },` |
|         - | 1328 | `	{ "readdir", "$dir_handle = NULL", "string\|false" },` |
|         - | 1329 | `	{ "readfile", "string $filename, bool $use_include_path = false, $context = NULL", "int\|false" },` |
|         - | 1330 | `	{ "readlink", "string $path", "string\|false" },` |
|         - | 1331 | `	{ "realpath", "string $path", "string\|false" },` |
|         - | 1332 | `	{ "stream_resolve_include_path", "string $filename", "string\|false" },` |
|         - | 1333 | `	{ "register_shutdown_function", "callable $callback, mixed ...$args = ?", "void" },` |
|         - | 1334 | `	{ "rename", "string $from, string $to, $context = NULL", "bool" },` |
|         - | 1335 | `	{ "reset", "object\|array &$array", "mixed" },` |
|         - | 1336 | `	{ "restore_error_handler", "", "true" },` |
|         - | 1337 | `	{ "restore_exception_handler", "", "true" },` |
|         - | 1338 | `	{ "rewind", "$stream", "bool" },` |
|         - | 1339 | `	{ "rewinddir", "$dir_handle = NULL", "void" },` |
|         - | 1340 | `	{ "rmdir", "string $directory, $context = NULL", "bool" },` |
|         - | 1341 | `	{ "round", "int\|float $num, int $precision = 0, RoundingMode\|int $mode = RoundingMode::HalfAwayFromZero", "float" },` |
|         - | 1342 | `	{ "rsort", "array &$array, int $flags = SORT_REGULAR", "true" },` |
|         - | 1343 | `	{ "rtrim", "string $string, string $characters = \" \\n\\r\\t\\v\\x00\"", "string" },` |
|         - | 1344 | `	{ "serialize", "mixed $value", "string" },` |
|         - | 1345 | `	{ "set_error_handler", "?callable $callback, int $error_levels = E_ALL", "" },` |
|         - | 1346 | `	{ "set_exception_handler", "?callable $callback", "" },` |
|         - | 1347 | `	{ "get_error_handler", "", "?callable" },` |
|         - | 1348 | `	{ "get_exception_handler", "", "?callable" },` |
|         - | 1349 | `	{ "hrtime", "bool $as_number = false", "array\|int\|float\|false" },` |
|         - | 1350 | `	{ "getrusage", "int $mode = 0", "array\|false" },` |
|         - | 1351 | ``	/* php's own row is `int $category, mixed ...$rest` with a MINIMUM of two, so`` |
|         - | 1352 | ``	 * `setlocale(LC_ALL)` is its ArgumentCountError and not a query. */`` |
|         - | 1353 | `	{ "setlocale", "int $category, array\|string $locales, string ...$rest = ?", "string\|false" },` |
|         - | 1354 | `	{ "mb_check_encoding", "array\|string\|null $value = NULL, ?string $encoding = NULL", "bool" },` |
|         - | 1355 | `	{ "mb_convert_case", "string $string, int $mode, ?string $encoding = NULL", "string" },` |
|         - | 1356 | `	{ "mb_detect_encoding", "string $string, array\|string\|null $encodings = NULL, bool $strict = false", "string\|false" },` |
|         - | 1357 | `	{ "mb_encoding_aliases", "string $encoding", "array" },` |
|         - | 1358 | `	{ "mb_internal_encoding", "?string $encoding = NULL", "string\|bool" },` |
|         - | 1359 | `	{ "mb_scrub", "string $string, ?string $encoding = null", "string" },` |
|         - | 1360 | `	{ "mb_substitute_character", "string\|int\|null $substitute_character = null", "string\|int\|bool" },` |
|         - | 1361 | `	{ "mb_str_split", "string $string, int $length = 1, ?string $encoding = NULL", "array" },` |
|         - | 1362 | `	{ "mb_stripos", "string $haystack, string $needle, int $offset = 0, ?string $encoding = NULL", "int\|false" },` |
|         - | 1363 | `	{ "mb_strlen", "string $string, ?string $encoding = NULL", "int" },` |
|         - | 1364 | `	{ "mb_ereg", "string $pattern, string $string, &$matches = NULL", "bool" },` |
|         - | 1365 | `	{ "mb_eregi", "string $pattern, string $string, &$matches = NULL", "bool" },` |
|         - | 1366 | `	{ "mb_ereg_match", "string $pattern, string $string, ?string $options = null", "bool" },` |
|         - | 1367 | `	{ "mb_ereg_replace", "string $pattern, string $replacement, string $string, ?string $options = null", "string\|false\|null" },` |
|         - | 1368 | `	{ "mb_eregi_replace", "string $pattern, string $replacement, string $string, ?string $options = null", "string\|false\|null" },` |
|         - | 1369 | `	{ "mb_ereg_replace_callback", "string $pattern, callable $callback, string $string, ?string $options = null", "string\|false\|null" },` |
|         - | 1370 | `	{ "mb_split", "string $pattern, string $string, int $limit = -1", "array\|false" },` |
|         - | 1371 | `	{ "mb_ereg_search_init", "string $string, ?string $pattern = null, ?string $options = null", "bool" },` |
|         - | 1372 | `	{ "mb_ereg_search", "?string $pattern = null, ?string $options = null", "bool" },` |
|         - | 1373 | `	{ "mb_ereg_search_pos", "?string $pattern = null, ?string $options = null", "array\|false" },` |
|         - | 1374 | `	{ "mb_ereg_search_regs", "?string $pattern = null, ?string $options = null", "array\|false" },` |
|         - | 1375 | `	{ "mb_ereg_search_getregs", "", "array\|false" },` |
|         - | 1376 | `	{ "mb_ereg_search_getpos", "", "int" },` |
|         - | 1377 | `	{ "mb_ereg_search_setpos", "int $offset", "bool" },` |
|         - | 1378 | `	{ "mb_regex_encoding", "?string $encoding = null", "string\|bool" },` |
|         - | 1379 | `	{ "mb_regex_set_options", "?string $options = null", "string" },` |
|         - | 1380 | `	{ "mb_strpos", "string $haystack, string $needle, int $offset = 0, ?string $encoding = NULL", "int\|false" },` |
|         - | 1381 | `	{ "mb_str_pad", "string $string, int $length, string $pad_string = \" \", int $pad_type = STR_PAD_RIGHT, ?string $encoding = null", "string" },` |
|         - | 1382 | `	{ "mb_strcut", "string $string, int $start, ?int $length = null, ?string $encoding = null", "string" },` |
|         - | 1383 | `	{ "mb_strimwidth", "string $string, int $start, int $width, string $trim_marker = \"\", ?string $encoding = null", "string" },` |
|         - | 1384 | `	{ "mb_strrchr", "string $haystack, string $needle, bool $before_needle = false, ?string $encoding = null", "string\|false" },` |
|         - | 1385 | `	{ "mb_strrichr", "string $haystack, string $needle, bool $before_needle = false, ?string $encoding = null", "string\|false" },` |
|         - | 1386 | `	{ "mb_strripos", "string $haystack, string $needle, int $offset = 0, ?string $encoding = null", "int\|false" },` |
|         - | 1387 | `	{ "mb_strrpos", "string $haystack, string $needle, int $offset = 0, ?string $encoding = NULL", "int\|false" },` |
|         - | 1388 | `	{ "mb_stristr", "string $haystack, string $needle, bool $before_needle = false, ?string $encoding = null", "string\|false" },` |
|         - | 1389 | `	{ "mb_strstr", "string $haystack, string $needle, bool $before_needle = false, ?string $encoding = null", "string\|false" },` |
|         - | 1390 | `	{ "mb_substr_count", "string $haystack, string $needle, ?string $encoding = null", "int" },` |
|         - | 1391 | `	{ "mb_strwidth", "string $string, ?string $encoding = NULL", "int" },` |
|         - | 1392 | `	{ "mb_substr", "string $string, int $start, ?int $length = NULL, ?string $encoding = NULL", "string" },` |
|         - | 1393 | `	{ "memory_reset_peak_usage", "", "void" },` |
|         - | 1394 | `	{ "proc_close", "$process", "int" },` |
|         - | 1395 | `	{ "proc_get_status", "$process", "array" },` |
|         - | 1396 | `	{ "proc_nice", "int $priority", "bool" },` |
|         - | 1397 | `	{ "proc_open", "array\|string $command, array $descriptor_spec, &$pipes, ?string $cwd = NULL, ?array $env_vars = NULL, ?array $options = NULL", "" },` |
|         - | 1398 | `	{ "proc_terminate", "$process, int $signal = 15", "bool" },` |
|         - | 1399 | `	{ "set_include_path", "string $include_path", "string\|false" },` |
|         - | 1400 | `	{ "setcookie", "string $name, string $value = '', array\|int $expires_or_options = 0, string $path = '', string $domain = '', bool $secure = false, bool $httponly = false", "bool" },` |
|         - | 1401 | `	{ "setrawcookie", "string $name, string $value = '', array\|int $expires_or_options = 0, string $path = '', string $domain = '', bool $secure = false, bool $httponly = false", "bool" },` |
|         - | 1402 | `	{ "settype", "mixed &$var, string $type", "bool" },` |
|         - | 1403 | `	{ "sha1", "string $string, bool $binary = false", "string" },` |
|         - | 1404 | `	{ "sha1_file", "string $filename, bool $binary = false", "string\|false" },` |
|         - | 1405 | `	{ "shell_exec", "string $command", "string\|false\|null" },` |
|         - | 1406 | `	{ "shuffle", "array &$array", "true" },` |
|         - | 1407 | `	{ "similar_text", "string $string1, string $string2, &$percent = NULL", "int" },` |
|         - | 1408 | `	{ "sin", "float $num", "float" },` |
|         - | 1409 | `	{ "sinh", "float $num", "float" },` |
|         - | 1410 | `	{ "sizeof", "Countable\|array $value, int $mode = COUNT_NORMAL", "int" },` |
|         - | 1411 | `	{ "sleep", "int $seconds", "int" },` |
|         - | 1412 | `	{ "sort", "array &$array, int $flags = SORT_REGULAR", "true" },` |
|         - | 1413 | `	{ "soundex", "string $string", "string" },` |
|         - | 1414 | `	{ "spl_autoload", "string $class, ?string $file_extensions = NULL", "void" },` |
|         - | 1415 | `	{ "spl_autoload_call", "string $class", "void" },` |
|         - | 1416 | `	{ "spl_autoload_extensions", "?string $file_extensions = NULL", "string" },` |
|         - | 1417 | `	{ "spl_autoload_functions", "", "array" },` |
|         - | 1418 | `	{ "spl_autoload_register", "?callable $callback = NULL, bool $throw = true, bool $prepend = false", "bool" },` |
|         - | 1419 | `	{ "spl_autoload_unregister", "callable $callback", "bool" },` |
|         - | 1420 | `	{ "spl_classes", "", "array" },` |
|         - | 1421 | `	{ "spl_object_hash", "object $object", "string" },` |
|         - | 1422 | `	{ "spl_object_id", "object $object", "int" },` |
|         - | 1423 | `	{ "sprintf", "string $format, mixed ...$values = ?", "string" },` |
|         - | 1424 | `	{ "sqrt", "float $num", "float" },` |
|         - | 1425 | `	{ "srand", "?int $seed = NULL, int $mode = MT_RAND_MT19937", "void" },` |
|         - | 1426 | `	{ "stat", "string $filename", "array\|false" },` |
|         - | 1427 | `	{ "str_contains", "string $haystack, string $needle", "bool" },` |
|         - | 1428 | `	{ "str_ends_with", "string $haystack, string $needle", "bool" },` |
|         - | 1429 | `	{ "str_getcsv", "string $string, string $separator = ',', string $enclosure = '\"', string $escape = '\\\\'", "array" },` |
|         - | 1430 | `	{ "str_ireplace", "array\|string $search, array\|string $replace, array\|string $subject, &$count = NULL", "array\|string" },` |
|         - | 1431 | `	{ "str_pad", "string $string, int $length, string $pad_string = ' ', int $pad_type = STR_PAD_RIGHT", "string" },` |
|         - | 1432 | `	{ "str_repeat", "string $string, int $times", "string" },` |
|         - | 1433 | `	{ "str_replace", "array\|string $search, array\|string $replace, array\|string $subject, &$count = NULL", "array\|string" },` |
|         - | 1434 | `	{ "str_rot13", "string $string", "string" },` |
|         - | 1435 | `	{ "str_shuffle", "string $string", "string" },` |
|         - | 1436 | `	{ "str_split", "string $string, int $length = 1", "array" },` |
|         - | 1437 | `	{ "str_starts_with", "string $haystack, string $needle", "bool" },` |
|         - | 1438 | `	{ "str_word_count", "string $string, int $format = 0, ?string $characters = NULL", "array\|int" },` |
|         - | 1439 | `	{ "strcasecmp", "string $string1, string $string2", "int" },` |
|         - | 1440 | `	{ "strchr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|         - | 1441 | `	{ "strcmp", "string $string1, string $string2", "int" },` |
|         - | 1442 | `	{ "strnatcasecmp", "string $string1, string $string2", "int" },` |
|         - | 1443 | `	{ "strnatcmp", "string $string1, string $string2", "int" },` |
|         - | 1444 | `	{ "strcoll", "string $string1, string $string2", "int" },` |
|         - | 1445 | `	{ "strcspn", "string $string, string $characters, int $offset = 0, ?int $length = NULL", "int" },` |
|         - | 1446 | `	{ "stream_context_create", "?array $options = NULL, ?array $params = NULL", "" },` |
|         - | 1447 | `	{ "stream_context_get_options", "$stream_or_context", "array" },` |
|         - | 1448 | ``	/* php's argument #2 is `array\|string $wrapper_or_options` and the array form`` |
|         - | 1449 | `	 * — the two-argument spelling — is DEPRECATED in 8.3; the scope policy refuses what php` |
|         - | 1450 | `	 * deprecates, so this row declares the string and the whole-array form is` |
|         - | 1451 | `	 * spelled stream_context_set_options(). */` |
|         - | 1452 | `	{ "stream_context_set_option", "$context, string $wrapper_name, string $option_name, mixed $value", "true" },` |
|         - | 1453 | `	{ "stream_context_set_options", "$context, array $options", "true" },` |
|         - | 1454 | `	{ "stream_context_get_params", "$context", "array" },` |
|         - | 1455 | `	{ "stream_context_set_params", "$context, array $params", "true" },` |
|         - | 1456 | `	{ "stream_context_get_default", "?array $options = NULL", "" },` |
|         - | 1457 | `	{ "stream_context_set_default", "array $options", "" },` |
|         - | 1458 | `	{ "stream_get_contents", "$stream, ?int $length = NULL, int $offset = -1", "string\|false" },` |
|         - | 1459 | `	{ "stream_get_line", "$stream, int $length, string $ending = ''", "string\|false" },` |
|         - | 1460 | `	{ "socket_get_status", "$stream", "array" },` |
|         - | 1461 | `	{ "stream_get_meta_data", "$stream", "array" },` |
|         - | 1462 | `	{ "stream_copy_to_stream", "$from, $to, ?int $length = NULL, int $offset = 0", "int\|false" },` |
|         - | 1463 | `	{ "stream_get_transports", "", "array" },` |
|         - | 1464 | `	{ "stream_is_local", "$stream", "bool" },` |
|         - | 1465 | `	{ "stream_select", "?array &$read, ?array &$write, ?array &$except, ?int $seconds, ?int $microseconds = NULL", "int\|false" },` |
|         - | 1466 | `	{ "stream_set_blocking", "$stream, bool $enable", "bool" },` |
|         - | 1467 | `	{ "socket_set_blocking", "$stream, bool $enable", "bool" },` |
|         - | 1468 | `	{ "stream_set_chunk_size", "$stream, int $size", "int" },` |
|         - | 1469 | `	{ "stream_set_read_buffer", "$stream, int $size", "int" },` |
|         - | 1470 | `	{ "stream_set_timeout", "$stream, int $seconds, int $microseconds = 0", "bool" },` |
|         - | 1471 | `	{ "stream_set_write_buffer", "$stream, int $size", "int" },` |
|         - | 1472 | `	{ "set_file_buffer", "$stream, int $size", "int" },` |
|         - | 1473 | `	{ "stream_supports_lock", "$stream", "bool" },` |
|         - | 1474 | `	{ "stream_get_wrappers", "", "array" },` |
|         - | 1475 | `	{ "stream_get_filters", "", "array" },` |
|         - | 1476 | `	{ "stream_filter_append", "$stream, string $filter_name, int $mode = 0, mixed $params = ?", "" },` |
|         - | 1477 | `	{ "stream_filter_prepend", "$stream, string $filter_name, int $mode = 0, mixed $params = ?", "" },` |
|         - | 1478 | `	{ "stream_filter_remove", "$stream_filter", "bool" },` |
|         - | 1479 | `	{ "stream_filter_register", "string $filter_name, string $class", "bool" },` |
|         - | 1480 | `	{ "stream_bucket_make_writeable", "$brigade", "?StreamBucket" },` |
|         - | 1481 | `	{ "stream_bucket_append", "$brigade, StreamBucket $bucket", "void" },` |
|         - | 1482 | `	{ "stream_bucket_prepend", "$brigade, StreamBucket $bucket", "void" },` |
|         - | 1483 | `	{ "stream_bucket_new", "$stream, string $buffer", "StreamBucket" },` |
|         - | 1484 | `	{ "stream_register_wrapper", "string $protocol, string $class, int $flags = 0", "bool" },` |
|         - | 1485 | `	{ "stream_wrapper_register", "string $protocol, string $class, int $flags = 0", "bool" },` |
|         - | 1486 | `	{ "stream_wrapper_unregister", "string $protocol", "bool" },` |
|         - | 1487 | `	{ "stream_wrapper_restore", "string $protocol", "bool" },` |
|         - | 1488 | `	{ "strip_tags", "string $string, array\|string\|null $allowed_tags = NULL", "string" },` |
|         - | 1489 | `	{ "stripcslashes", "string $string", "string" },` |
|         - | 1490 | `	{ "stripos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|         - | 1491 | `	{ "stripslashes", "string $string", "string" },` |
|         - | 1492 | `	{ "stristr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|         - | 1493 | `	{ "strlen", "string $string", "int" },` |
|         - | 1494 | `	{ "strncasecmp", "string $string1, string $string2, int $length", "int" },` |
|         - | 1495 | `	{ "strncmp", "string $string1, string $string2, int $length", "int" },` |
|         - | 1496 | `	{ "strpbrk", "string $string, string $characters", "string\|false" },` |
|         - | 1497 | `	{ "strpos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|         - | 1498 | `	{ "strrchr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|         - | 1499 | `	{ "strrev", "string $string", "string" },` |
|         - | 1500 | `	{ "strripos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|         - | 1501 | `	{ "strrpos", "string $haystack, string $needle, int $offset = 0", "int\|false" },` |
|         - | 1502 | `	{ "strspn", "string $string, string $characters, int $offset = 0, ?int $length = NULL", "int" },` |
|         - | 1503 | `	{ "strstr", "string $haystack, string $needle, bool $before_needle = false", "string\|false" },` |
|         - | 1504 | `	{ "strtok", "string $string, ?string $token = NULL", "string\|false" },` |
|         - | 1505 | `	{ "strtolower", "string $string", "string" },` |
|         - | 1506 | `	{ "strtotime", "string $datetime, ?int $baseTimestamp = NULL", "int\|false" },` |
|         - | 1507 | `	{ "strtoupper", "string $string", "string" },` |
|         - | 1508 | `	{ "strtr", "string $string, array\|string $from, ?string $to = NULL", "string" },` |
|         - | 1509 | `	{ "strval", "mixed $value", "string" },` |
|         - | 1510 | `	{ "substr", "string $string, int $offset, ?int $length = NULL", "string" },` |
|         - | 1511 | `	{ "substr_compare", "string $haystack, string $needle, int $offset, ?int $length = NULL, bool $case_insensitive = false", "int" },` |
|         - | 1512 | `	{ "substr_count", "string $haystack, string $needle, int $offset = 0, ?int $length = NULL", "int" },` |
|         - | 1513 | `	{ "substr_replace", "array\|string $string, array\|string $replace, array\|int $offset, array\|int\|null $length = NULL", "array\|string" },` |
|         - | 1514 | `	{ "symlink", "string $target, string $link", "bool" },` |
|         - | 1515 | `	{ "sys_get_temp_dir", "", "string" },` |
|         - | 1516 | `	{ "sys_getloadavg", "", "array\|false" },` |
|         - | 1517 | `	{ "system", "string $command, &$result_code = NULL", "string\|false" },` |
|         - | 1518 | `	{ "tan", "float $num", "float" },` |
|         - | 1519 | `	{ "tanh", "float $num", "float" },` |
|         - | 1520 | `	{ "time", "", "int" },` |
|         - | 1521 | `	{ "token_get_all", "string $code, int $flags = 0", "array" },` |
|         - | 1522 | `	{ "php_strip_whitespace", "string $filename", "string" },` |
|         - | 1523 | `	{ "token_name", "int $id", "string" },` |
|         - | 1524 | `	{ "touch", "string $filename, ?int $mtime = NULL, ?int $atime = NULL", "bool" },` |
|         - | 1525 | `	{ "trigger_error", "string $message, int $error_level = E_USER_NOTICE", "true" },` |
|         - | 1526 | `	{ "trim", "string $string, string $characters = \" \\n\\r\\t\\v\\x00\"", "string" },` |
|         - | 1527 | `	{ "uasort", "array &$array, callable $callback", "true" },` |
|         - | 1528 | `	{ "ucfirst", "string $string", "string" },` |
|         - | 1529 | `	{ "ucwords", "string $string, string $separators = \" \\t\\r\\n\\f\\v\"", "string" },` |
|         - | 1530 | `	{ "uksort", "array &$array, callable $callback", "true" },` |
|         - | 1531 | `	{ "umask", "?int $mask = NULL", "int" },` |
|         - | 1532 | `	{ "uniqid", "string $prefix = '', bool $more_entropy = false", "string" },` |
|         - | 1533 | `	{ "unixtojd", "?int $timestamp = NULL", "int\|false" },` |
|         - | 1534 | `	{ "unlink", "string $filename, $context = NULL", "bool" },` |
|         - | 1535 | `	{ "unserialize", "string $data, array $options = []", "mixed" },` |
|         - | 1536 | `	{ "urldecode", "string $string", "string" },` |
|         - | 1537 | `	{ "urlencode", "string $string", "string" },` |
|         - | 1538 | `	{ "user_error", "string $message, int $error_level = E_USER_NOTICE", "true" },` |
|         - | 1539 | `	{ "usleep", "int $microseconds", "void" },` |
|         - | 1540 | `	{ "usort", "array &$array, callable $callback", "true" },` |
|         - | 1541 | `	{ "var_dump", "mixed $value, mixed ...$values = ?", "void" },` |
|         - | 1542 | `	{ "var_export", "mixed $value, bool $return = false", "?string" },` |
|         - | 1543 | `	{ "version_compare", "string $version1, string $version2, ?string $operator = null", "int\|bool" },` |
|         - | 1544 | `	/* Both typed parameters stand aside for the same reason as fprintf's: php` |
|         - | 1545 | `	 * refuses $stream before either of them. */` |
|         - | 1546 | `	{ "vfprintf", "$stream, ~string $format, ~array $values", "int" },` |
|         - | 1547 | `	{ "vprintf", "string $format, array $values", "int" },` |
|         - | 1548 | `	{ "vsprintf", "string $format, array $values", "string" },` |
|         - | 1549 | `	{ "wordwrap", "string $string, int $width = 75, string $break = \"\\n\", bool $cut_long_words = false", "string" },` |
|         - | 1550 | `	{ "zip_close", "$zip", "void" },` |
|         - | 1551 | `	{ "zip_entry_close", "$zip_entry", "bool" },` |
|         - | 1552 | `	{ "zip_entry_compressedsize", "$zip_entry", "int\|false" },` |
|         - | 1553 | `	{ "zip_entry_compressionmethod", "$zip_entry", "string\|false" },` |
|         - | 1554 | `	{ "zip_entry_filesize", "$zip_entry", "int\|false" },` |
|         - | 1555 | `	{ "zip_entry_name", "$zip_entry", "string\|false" },` |
|         - | 1556 | `	{ "zip_entry_open", "$zip_dp, $zip_entry, string $mode = 'rb'", "bool" },` |
|         - | 1557 | `	{ "zip_entry_read", "$zip_entry, int $len = 1024", "string\|false" },` |
|         - | 1558 | `	{ "zip_open", "string $filename", "" },` |
|         - | 1559 | `	{ "zip_read", "$zip", "" },` |
|         - | 1560 | `};` |
|         - | 1561 | `/*` |
|         - | 1562 | ` * Stamp the signature strings onto the registered host functions.` |
|         - | 1563 | ` * Runs once at PH7_VmMakeReady, after the builtins are registered.` |
|         - | 1564 | ` */` |
|         - | 1565 | `/*` |
|         - | 1566 | ` * Derive the minimum arity from a builtin's declared signature: a parameter is` |
|         - | 1567 | ` * optional when it carries a default ("int $length = 76") or is variadic` |
|         - | 1568 | ` * ("...$rest"), so the minimum is the count of the parameters that are neither,` |
|         - | 1569 | ` * and the ZPP wording is "at least" as soon as one optional/variadic exists.` |
|         - | 1570 | ` *` |
|         - | 1571 | ` * This makes aBuiltinSig[] the single source of truth for arity — the curated` |
|         - | 1572 | ` * aBuiltinArity[] table above stays as an OVERRIDE for the handful of builtins` |
|         - | 1573 | ` * php reports differently from their own signature (e.g. strtr(), whose 3-arg` |
|         - | 1574 | ` * form still reports "expects exactly 2", and the array_udiff() family, whose` |
|         - | 1575 | ` * variadic tail hides a second required argument). Verified against php 8.5.7` |
|         - | 1576 | ` * for all 462 signed builtins: 458 derive exactly, 4 are overridden.` |
|         - | 1577 | ` */` |
|         - | 1578 | `/*` |
|         - | 1579 | ` * A DEFAULT can contain the parameter separator: php declares` |
|         - | 1580 | `` * `string $separator = ','` and `string $enclosure = '"'`. Every scan of a`` |
|         - | 1581 | ` * signature therefore has to step over a quoted run, or the comma inside one` |
|         - | 1582 | ` * splits the parameter in two — which is how fgetcsv()/fputcsv()/str_getcsv()` |
|         - | 1583 | ` * came to count SIX parameters and accept a fifth argument php refuses.` |
|         - | 1584 | ` * Answers the position of the closing quote (or of the NUL when the run is` |
|         - | 1585 | ` * unterminated); the caller advances past it.` |
|         - | 1586 | ` */` |
|   1456136 | 1587 | `static const char *VmSigSkipQuoted(const char *zCur)` |
|         5 | 1588 | `{` |
|   1456141 | 1589 | `	char c = zCur[0];` |
|   1456141 | 1590 | `	if( c != '\'' && c != '"' ){` |
|       ! 0 | 1591 | `		return zCur;` |
|         - | 1592 | `	}` |
|   3933263 | 1593 | `	for( zCur++ ; zCur[0] ; zCur++ ){` |
|   3933263 | 1594 | `		if( zCur[0] == '\\' && zCur[1] ){` |
|    512861 | 1595 | `			zCur++;` |
|    512861 | 1596 | `			continue;` |
|         - | 1597 | `		}` |
|   3420407 | 1598 | `		if( zCur[0] == c ){` |
|   1456141 | 1599 | `			break;` |
|         - | 1600 | `		}` |
|    980683 | 1601 | `	}` |
|   1456141 | 1602 | `	return zCur;` |
|    726967 | 1603 | `}` |
|  17959301 | 1604 | `PH7_PRIVATE void VmDeriveArityFromSig(const char *zSig,sxi16 *pnMin,sxu8 *pbAtLeast,sxi16 *pnMax,sxu8 *pbHasMax)` |
|         5 | 1605 | `{` |
|  17959306 | 1606 | `	const char *zCur = zSig;` |
|  17959306 | 1607 | `	int nMin = 0, bAtLeast = 0, bSeen = 0, bOptional = 0;` |
|  17959306 | 1608 | `	int nTotal = 0, bVariadic = 0;` |
| 207799107 | 1609 | `	for(;;){` |
| 428508838 | 1610 | `		if( zCur[0] == '\'' \|\| zCur[0] == '"' ){` |
|    725869 | 1611 | `			bSeen = 1;` |
|    725869 | 1612 | `			zCur = VmSigSkipQuoted(zCur);` |
|    725869 | 1613 | `			if( zCur[0] != '\0' ){` |
|    725869 | 1614 | `				zCur++;` |
|    362382 | 1615 | `			}` |
|    725869 | 1616 | `			continue;` |
|         - | 1617 | `		}` |
| 427782974 | 1618 | `		if( zCur[0] == '\0' \|\| zCur[0] == ',' ){` |
|  28924376 | 1619 | `			if( bSeen ){` |
|  22536307 | 1620 | `				nTotal++;` |
|  22536307 | 1621 | `				if( bOptional ){` |
|   8137398 | 1622 | `					bAtLeast = 1;` |
|   4050703 | 1623 | `				}else{` |
|  14398914 | 1624 | `					nMin++;` |
|         - | 1625 | `				}` |
|  11233972 | 1626 | `			}` |
|  28924376 | 1627 | `			if( zCur[0] == '\0' ){` |
|  17959306 | 1628 | `				break;` |
|         - | 1629 | `			}` |
|  10965075 | 1630 | `			bSeen = bOptional = 0;` |
|  10965075 | 1631 | `			zCur++;` |
|  10965075 | 1632 | `			continue;` |
|         - | 1633 | `		}` |
| 398858603 | 1634 | `		if( zCur[0] != ' ' ){` |
| 351270194 | 1635 | `			bSeen = 1;` |
| 175130764 | 1636 | `		}` |
| 398858603 | 1637 | `		if( zCur[0] == '=' \|\| (zCur[0] == '.' && zCur[1] == '.' && zCur[2] == '.') ){` |
|   8391656 | 1638 | `			bOptional = 1;` |
|   4177618 | 1639 | `		}` |
| 398858603 | 1640 | `		if( zCur[0] == '.' && zCur[1] == '.' && zCur[2] == '.' ){` |
|    444463 | 1641 | `			bVariadic = 1;` |
|    221888 | 1642 | `		}` |
| 398858603 | 1643 | `		zCur++;` |
|         5 | 1644 | `	}` |
|  17959306 | 1645 | `	*pnMin = (sxi16)nMin;` |
|  17959306 | 1646 | `	*pbAtLeast = (sxu8)bAtLeast;` |
|         - | 1647 | `	/* php enforces a MAXIMUM too ("expects at most 1 argument, 2 given"); a` |
|         - | 1648 | `	 * variadic tail means there is none. The parameter COUNT is the maximum,` |
|         - | 1649 | `	 * whether or not the parameters carry defaults. */` |
|  17959306 | 1650 | `	*pnMax = (sxi16)nTotal;` |
|  17959306 | 1651 | `	*pbHasMax = (sxu8)(bVariadic ? 0 : 1);` |
|  17959306 | 1652 | `}` |
|         - | 1653 | `/*` |
|         - | 1654 | ` * Does the declared type list (e.g. "array\|string", "?int", "callable") contain` |
|         - | 1655 | ` * the given token? Compares against each '\|'-separated alternative, ignoring a` |
|         - | 1656 | ` * leading nullable '?'.` |
|         - | 1657 | ` */` |
|   1058340 | 1658 | `static int VmSigTypeHas(const char *zType,int nType,const char *zTok)` |
|         5 | 1659 | `{` |
|   1058345 | 1660 | `	int nTok = (int)SyStrlen(zTok);` |
|   1058345 | 1661 | `	int i = 0;` |
|   1058345 | 1662 | `	if( zType[0] == '?' ){` |
|    127123 | 1663 | `		zType++;` |
|    127123 | 1664 | `		nType--;` |
|     62948 | 1665 | `	}` |
|   1990689 | 1666 | `	while( i < nType ){` |
|   1001547 | 1667 | `		int j = i;` |
|   6320133 | 1668 | `		while( j < nType && zType[j] != '\|' ){` |
|   5318591 | 1669 | `			j++;` |
|         5 | 1670 | `		}` |
|   1001547 | 1671 | `		if( j - i == nTok && SyMemcmp(&zType[i],zTok,(sxu32)nTok) == 0 ){` |
|     69203 | 1672 | `			return 1;` |
|         - | 1673 | `		}` |
|    932349 | 1674 | `		i = j + 1;` |
|         5 | 1675 | `	}` |
|    989147 | 1676 | `	return 0;` |
|    522842 | 1677 | `}` |
|         - | 1678 | `/*` |
|         - | 1679 | `` * Is EVERY arm of the declared type list `array` (a bare `array`, or `?array`,`` |
|         - | 1680 | `` * or the `array\|null` union that spells the same thing)? Such a parameter has`` |
|         - | 1681 | ` * no arm a scalar can satisfy, and php refuses one outright.` |
|         - | 1682 | ` *` |
|         - | 1683 | `` * The screen used to exempt any type list carrying an `array` arm, union or`` |
|         - | 1684 | `` * not, for a wording reason: php's `array\|object` parameters come from ONE ZPP`` |
|         - | 1685 | ` * macro (Z_PARAM_ARRAY_OR_OBJECT) that names only "array" in the refusal, so` |
|         - | 1686 | ` * the declared type is not the text php prints. That ambiguity does not exist` |
|         - | 1687 | `` * for a parameter typed exactly `array` -- there is one arm and php prints it.`` |
|         - | 1688 | ` */` |
|     76672 | 1689 | `static int VmSigTypeIsArrayOnly(const char *zType,int nType)` |
|         5 | 1690 | `{` |
|     76677 | 1691 | `	int i = 0, bArray = 0;` |
|     76677 | 1692 | `	if( zType[0] == '?' ){` |
|      9779 | 1693 | `		zType++;` |
|      9779 | 1694 | `		nType--;` |
|      4840 | 1695 | `	}` |
|     83265 | 1696 | `	while( i < nType ){` |
|     70177 | 1697 | `		int j = i;` |
|    441619 | 1698 | `		while( j < nType && zType[j] != '\|' ){` |
|    371447 | 1699 | `			j++;` |
|         5 | 1700 | `		}` |
|     70177 | 1701 | `		if( j > i ){` |
|     70172 | 1702 | `			if( j - i == (int)sizeof("array")-1` |
|     40991 | 1703 | `			 && SyMemcmp(&zType[i],"array",sizeof("array")-1) == 0 ){` |
|      6593 | 1704 | `				bArray = 1;` |
|     69029 | 1705 | `			}else if( !(j - i == (int)sizeof("null")-1` |
|     33612 | 1706 | `			         && SyMemcmp(&zType[i],"null",sizeof("null")-1) == 0) ){` |
|     63589 | 1707 | `				return 0;` |
|         - | 1708 | `			}` |
|      3238 | 1709 | `		}` |
|      6593 | 1710 | `		i = j + 1;` |
|         5 | 1711 | `	}` |
|     13093 | 1712 | `	return bArray;` |
|     37881 | 1713 | `}` |
|         - | 1714 | `/*` |
|         - | 1715 | ` * Does the declared type list name a CLASS (anything that is not one of php's` |
|         - | 1716 | ` * builtin type keywords)? A class-typed parameter accepts an object, so it must` |
|         - | 1717 | ` * not be rejected by the array/object/resource screen below.` |
|         - | 1718 | ` */` |
|         - | 1719 | `/* Is this one arm of a declared type a BUILTIN type name rather than a class? */` |
|    113459 | 1720 | `static int VmSigArmIsBuiltinType(const char *zArm,int nArm)` |
|         5 | 1721 | `{` |
|         - | 1722 | `	static const char *azBuiltin[] = {` |
|         - | 1723 | `		"int","float","string","bool","array","object","callable","iterable",` |
|         - | 1724 | `		"mixed","null","void","resource","false","true","never","self","static"` |
|         - | 1725 | `	};` |
|         - | 1726 | `	int k;` |
|   1068067 | 1727 | `	for( k = 0 ; k < (int)SX_ARRAYSIZE(azBuiltin) ; ++k ){` |
|   1021450 | 1728 | `		int nB = (int)SyStrlen(azBuiltin[k]);` |
|   1021450 | 1729 | `		if( nArm == nB && SyMemcmp(zArm,azBuiltin[k],(sxu32)nB) == 0 ){` |
|     66847 | 1730 | `			return 1;` |
|         - | 1731 | `		}` |
|    476125 | 1732 | `	}` |
|     46622 | 1733 | `	return 0;` |
|     56291 | 1734 | `}` |
|     76674 | 1735 | `static int VmSigTypeHasClass(const char *zType,int nType)` |
|         5 | 1736 | `{` |
|     76679 | 1737 | `	int i = 0;` |
|     76679 | 1738 | `	if( zType[0] == '?' ){` |
|      9779 | 1739 | `		zType++;` |
|      9779 | 1740 | `		nType--;` |
|      4840 | 1741 | `	}` |
|    143511 | 1742 | `	while( i < nType ){` |
|     71965 | 1743 | `		int j = i;` |
|    451363 | 1744 | `		while( j < nType && zType[j] != '\|' ){` |
|    379403 | 1745 | `			j++;` |
|         5 | 1746 | `		}` |
|     71965 | 1747 | `		if( j > i && !VmSigArmIsBuiltinType(&zType[i],j - i) ){` |
|      5133 | 1748 | `			return 1;` |
|         - | 1749 | `		}` |
|     66837 | 1750 | `		i = j + 1;` |
|         5 | 1751 | `	}` |
|     71551 | 1752 | `	return 0;` |
|     37882 | 1753 | `}` |
|         - | 1754 | `/*` |
|         - | 1755 | ` * php's ", X given" tail: like ph7_type_name() but an object reports its CLASS,` |
|         - | 1756 | ` * which is what php prints in a TypeError.` |
|         - | 1757 | ` */` |
|         - | 1758 | `/*` |
|         - | 1759 | ` * Does pObj satisfy any CLASS arm of a declared type?` |
|         - | 1760 | ` *` |
|         - | 1761 | ` * Answers TRUE (unscreened) when an arm names something this VM has not declared:` |
|         - | 1762 | ` * the signatures describe php's surface, parts of which PHL models differently` |
|         - | 1763 | ` * (the resource-backed handles the RES branch below already excuses), and a name` |
|         - | 1764 | ` * that resolves to nothing must not turn into a rejection of a valid argument.` |
|         - | 1765 | ` */` |
|     41487 | 1766 | `static int VmSigObjSatisfiesClass(ph7_vm *pVm,const char *zType,int nType,` |
|         - | 1767 | `	ph7_class_instance *pObj)` |
|         5 | 1768 | `{` |
|     41492 | 1769 | `	int i = 0;` |
|     41492 | 1770 | `	if( pObj == 0 \|\| pObj->pClass == 0 ){` |
|       ! 0 | 1771 | `		return 1;` |
|         - | 1772 | `	}` |
|     41492 | 1773 | `	if( zType[0] == '?' ){` |
|      2087 | 1774 | `		zType++;` |
|      2087 | 1775 | `		nType--;` |
|      1041 | 1776 | `	}` |
|     41562 | 1777 | `	while( i < nType ){` |
|     41504 | 1778 | `		int j = i;` |
|    495797 | 1779 | `		while( j < nType && zType[j] != '\|' ){` |
|    454298 | 1780 | `			j++;` |
|         5 | 1781 | `		}` |
|     41504 | 1782 | `		if( j > i && !VmSigArmIsBuiltinType(&zType[i],j - i) ){` |
|     41494 | 1783 | `			ph7_class *pClass = PH7_VmExtractClass(&(*pVm),&zType[i],(sxu32)(j - i),FALSE,0);` |
|     41494 | 1784 | `			if( pClass == 0 ){` |
|         - | 1785 | `				/* Either a builtin type name (already excluded by the caller) or a` |
|         - | 1786 | `				 * class this build does not declare: nothing to judge. */` |
|       ! 0 | 1787 | `				return 1;` |
|         - | 1788 | `			}` |
|     41494 | 1789 | `			if( PH7_VmInstanceOf(pObj->pClass,pClass) ){` |
|     41434 | 1790 | `				return 1;` |
|         - | 1791 | `			}` |
|        30 | 1792 | `		}` |
|        72 | 1793 | `		i = j + 1;` |
|         2 | 1794 | `	}` |
|        60 | 1795 | `	return 0;` |
|     20752 | 1796 | `}` |
|       166 | 1797 | `static const char * VmArgTypeName(ph7_value *pVal)` |
|         4 | 1798 | `{` |
|       170 | 1799 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       170 | 1800 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|       170 | 1801 | `		if( pInst && pInst->pClass ){` |
|       170 | 1802 | `			return pInst->pClass->sName.zString;` |
|         - | 1803 | `		}` |
|       ! 0 | 1804 | `	}` |
|       ! 0 | 1805 | `	return ph7_type_name(pVal);` |
|        87 | 1806 | `}` |
|         - | 1807 | `/*` |
|         - | 1808 | `` * Does pArg satisfy a `string` parameter? The rule VmEnforceBuiltinArgTypes()`` |
|         - | 1809 | ` * below applies, factored out so a builtin that words its own overload dispatch` |
|         - | 1810 | ` * (strtr(), whose expected type depends on the ARITY and so cannot be spelled in` |
|         - | 1811 | ` * one signature) decides identically instead of forking the logic. An array never` |
|         - | 1812 | ` * satisfies one; an object does only through __toString(); a resource does not;` |
|         - | 1813 | ` * null does under php, with a deprecation, but not under PHL's the null-strictness policy` |
|         - | 1814 | ` * policy — the screen and this helper both report it as a mismatch.` |
|         - | 1815 | ` */` |
|     53527 | 1816 | `PH7_PRIVATE int PH7_ArgSatisfiesString(ph7_value *pArg)` |
|         5 | 1817 | `{` |
|     53532 | 1818 | `	if( (pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_NULL\|MEMOBJ_RES)) != 0 ){` |
|        38 | 1819 | `		return 0;` |
|         - | 1820 | `	}` |
|     53498 | 1821 | `	if( (pArg->iFlags & MEMOBJ_OBJ) != 0 ){` |
|       221 | 1822 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|       221 | 1823 | `		return pInst && PH7_ClassExtractMethod(pInst->pClass,"__toString",` |
|       108 | 1824 | `			sizeof("__toString")-1) != 0;` |
|         - | 1825 | `	}` |
|     53282 | 1826 | `	return 1;` |
|     26752 | 1827 | `}` |
|         - | 1828 | `/*` |
|         - | 1829 | `` * Is pArg an OBJECT that a `string` parameter can never take -- one with no`` |
|         - | 1830 | ` * __toString()? The question a builtin screening its own arguments actually has:` |
|         - | 1831 | ` * an object is the one kind whose acceptance depends on the class rather than on` |
|         - | 1832 | `` * the type tag, so a guard written as `ph7_value_is_object()` refuses the`` |
|         - | 1833 | ` * Stringable php converts. str_split(), addslashes(), addcslashes(), bindec(),` |
|         - | 1834 | ` * hexdec(), octdec() and the whole printf family all made that mistake, each` |
|         - | 1835 | ` * turning three one-character strings into a TypeError.` |
|         - | 1836 | ` *` |
|         - | 1837 | ` * Says nothing about arrays, resources or null: those are the caller's, because` |
|         - | 1838 | `` * a `string` parameter's null is refused here and coerced (with a deprecation)`` |
|         - | 1839 | ` * by php, and each builtin words that arm for itself.` |
|         - | 1840 | ` */` |
|     57924 | 1841 | `PH7_PRIVATE int PH7_ArgIsUnstringableObject(ph7_value *pArg)` |
|         5 | 1842 | `{` |
|     57929 | 1843 | `	return (pArg->iFlags & MEMOBJ_OBJ) != 0 && !PH7_ArgSatisfiesString(pArg);` |
|         5 | 1844 | `}` |
|         - | 1845 | `/*` |
|         - | 1846 | `` * Is the declared type exactly `int` — the only shape whose float argument the`` |
|         - | 1847 | `` * screen below can decide? A union with a `float`, `string` or `bool` arm has its`` |
|         - | 1848 | ` * own coercion rules per arm (and php words those refusals from the builtin), so` |
|         - | 1849 | ` * only the plain form and its nullable spelling qualify.` |
|         - | 1850 | ` */` |
|     76672 | 1851 | `static int VmSigTypeIsIntOnly(const char *zType,int nType)` |
|         5 | 1852 | `{` |
|     76677 | 1853 | `	if( nType > 0 && zType[0] == '?' ){` |
|      9779 | 1854 | `		zType++;` |
|      9779 | 1855 | `		nType--;` |
|      4840 | 1856 | `	}` |
|     76677 | 1857 | `	if( nType == (int)sizeof("int")-1 && SyMemcmp(zType,"int",3) == 0 ){` |
|     16535 | 1858 | `		return 1;` |
|         - | 1859 | `	}` |
|         - | 1860 | ``	/* `int\|null` / `null\|int`, the union spelling of `?int`. */`` |
|     60603 | 1861 | `	return VmSigTypeHas(zType,nType,"int") && VmSigTypeHas(zType,nType,"null")` |
|       456 | 1862 | `	    && !VmSigTypeHas(zType,nType,"float")` |
|       156 | 1863 | `	    && !VmSigTypeHas(zType,nType,"string")` |
|        46 | 1864 | `	    && !VmSigTypeHas(zType,nType,"bool")` |
|         4 | 1865 | `	    && !VmSigTypeHas(zType,nType,"array")` |
|         2 | 1866 | `	    && !VmSigTypeHas(zType,nType,"object")` |
|       ! 0 | 1867 | `	    && !VmSigTypeHas(zType,nType,"iterable")` |
|       ! 0 | 1868 | `	    && !VmSigTypeHas(zType,nType,"callable")` |
|     60490 | 1869 | `	    && !VmSigTypeHasClass(zType,nType);` |
|     37881 | 1870 | `}` |
|         - | 1871 | `/*` |
|         - | 1872 | `` * Can this float reach an `int` parameter without losing anything? php's rule is`` |
|         - | 1873 | ` * php_parse_arg_long's: in range, and integral. NaN and the infinities are out by` |
|         - | 1874 | ` * the range test (a NaN compares false against both bounds, which is why the test` |
|         - | 1875 | ` * is written as a pair of accepts rather than a pair of rejects).` |
|         - | 1876 | ` */` |
|       106 | 1877 | `static int VmDoubleFitsInt(double d)` |
|         5 | 1878 | `{` |
|       111 | 1879 | `	if( !PH7_RealFitsInt64(d) ){` |
|        51 | 1880 | `		return 0;` |
|         - | 1881 | `	}` |
|        62 | 1882 | `	return d == (double)(sxi64)d;` |
|        58 | 1883 | `}` |
|         - | 1884 | `/*` |
|         - | 1885 | ` * The same question for a NUMERIC string, which php asks with the same answer:` |
|         - | 1886 | `` * `dechex("1e19")` and `dechex("99999999999999999999")` are both`` |
|         - | 1887 | `` * `must be of type int, string given`. RangeStrToNumber is php's`` |
|         - | 1888 | ` * is_numeric_string grammar and already reclassifies an integer too wide for an` |
|         - | 1889 | ` * sxi64 as a DOUBLE, so the two shapes converge on one test.` |
|         - | 1890 | ` */` |
|        80 | 1891 | `static int VmNumStrFitsInt(ph7_value *pArg)` |
|         4 | 1892 | `{` |
|         - | 1893 | `	const char *zStr;` |
|        84 | 1894 | `	int nLen = 0;` |
|        84 | 1895 | `	sxi64 iVal = 0;` |
|        84 | 1896 | `	double dVal = 0;` |
|        84 | 1897 | `	zStr = ph7_value_to_string(pArg,&nLen);` |
|        84 | 1898 | `	switch( RangeStrToNumber(zStr,(sxu32)nLen,&iVal,&dVal) ){` |
|        59 | 1899 | `	case RANGE_IN_LONG:   return 1;` |
|        27 | 1900 | `	case RANGE_IN_DOUBLE: return VmDoubleFitsInt(dVal);` |
|       ! 0 | 1901 | `	default:              return 0;` |
|         - | 1902 | `	}` |
|        44 | 1903 | `}` |
|         - | 1904 | `/*` |
|         - | 1905 | ` * PHP-8 PATH parameters: which positions carry a filesystem path, a shell` |
|         - | 1906 | ` * command or an include-path list rather than an ordinary string.` |
|         - | 1907 | ` *` |
|         - | 1908 | ` * php spells this in the ZPP macro, not in the declared type: a path parameter` |
|         - | 1909 | `` * is `Z_PARAM_PATH` where an ordinary one is `Z_PARAM_STR`, and both print as`` |
|         - | 1910 | `` * `string` in the stub Reflection reads. The difference is a single rule — a`` |
|         - | 1911 | ` * path may not contain a NUL byte — and php raises a catchable ValueError for` |
|         - | 1912 | ` * one that does, BEFORE the call reaches the filesystem.` |
|         - | 1913 | ` *` |
|         - | 1914 | ` * PHL had no such notion, so every one of these arguments went to the C API as` |
|         - | 1915 | ` * a NUL-terminated string and was silently TRUNCATED at the NUL. That is not a` |
|         - | 1916 | ` * missing diagnostic: the truncated path is a DIFFERENT path, and the builtin` |
|         - | 1917 | `` * then operated on it. `unlink("$dir/x\0.png")` deleted `$dir/x`,`` |
|         - | 1918 | `` * `file_put_contents("$dir/x\0.txt",$d)` wrote it, `touch`/`chmod`/`copy`/`` |
|         - | 1919 | ``  * `rename`/`symlink`/`mkdir` all acted on the prefix, `glob` and `realpath` `` |
|         - | 1920 | `` * answered for it, and `shell_exec("cmd\0; rm -rf /")` ran the prefix as a`` |
|         - | 1921 | ` * command. It is the classic poison-NUL-byte shape php closed engine-wide: a` |
|         - | 1922 | ` * script that concatenates request input into a filename gets a truncation` |
|         - | 1923 | ` * where php gets a refusal, and the extension check the suffix was there to` |
|         - | 1924 | ` * perform never runs.` |
|         - | 1925 | ` *` |
|         - | 1926 | ` * The mask is positional (bit N => parameter N is a path), which is how php` |
|         - | 1927 | ` * carries it too. Only functions PHL actually registers are listed; each row's` |
|         - | 1928 | ` * positions were verified against php 8.5 argument by argument (the answer is` |
|         - | 1929 | `` * NOT derivable from the parameter name — preg_match's `$pattern` is an`` |
|         - | 1930 | ``  * ordinary string, glob's is a path — nor from the type, which is `string` `` |
|         - | 1931 | ` * for both).` |
|         - | 1932 | ` *` |
|         - | 1933 | ` * What is deliberately NOT here: the stat family (file_exists, is_dir, stat,` |
|         - | 1934 | ` * filesize, fileperms, …), which php parses with Z_PARAM_STR and answers` |
|         - | 1935 | `` * `false` for in silence, and the pure PATH-STRING functions (basename,`` |
|         - | 1936 | ` * dirname, pathinfo), which php lets the NUL through untouched because they` |
|         - | 1937 | ` * never touch the filesystem. Both are php-exact here already.` |
|         - | 1938 | ` */` |
|     26208 | 1939 | `static sxu32 VmBuiltinPathMask(SyString *pName)` |
|         5 | 1940 | `{` |
|         - | 1941 | `	static const struct {` |
|         - | 1942 | `		const char *zName;` |
|         - | 1943 | `		sxu32 nByte;` |
|         - | 1944 | `		sxu32 mask;` |
|         - | 1945 | `	} aPath[] = {` |
|         - | 1946 | `		/* Open / read / write */` |
|         - | 1947 | `		{ "fopen",             5, 1u<<0 },` |
|         - | 1948 | `		{ "file_get_contents", 17, 1u<<0 },` |
|         - | 1949 | `		{ "file_put_contents", 17, 1u<<0 },` |
|         - | 1950 | `		{ "file",              4, 1u<<0 },` |
|         - | 1951 | `		{ "readfile",          8, 1u<<0 },` |
|         - | 1952 | `		{ "parse_ini_file",   14, 1u<<0 },` |
|         - | 1953 | `		{ "md5_file",          8, 1u<<0 },` |
|         - | 1954 | `		{ "getimagesize",     12, 1u<<0 },` |
|         - | 1955 | `		{ "sha1_file",         9, 1u<<0 },` |
|         - | 1956 | `		{ "hash_file",         9, 1u<<1 },` |
|         - | 1957 | `		{ "hash_hmac_file",   14, 1u<<1 },` |
|         - | 1958 | `		{ "hash_update_file", 16, 1u<<1 },` |
|         - | 1959 | `		/* Metadata / mutation */` |
|         - | 1960 | `		{ "unlink",            6, 1u<<0 },` |
|         - | 1961 | `		{ "touch",             5, 1u<<0 },` |
|         - | 1962 | `		{ "chmod",             5, 1u<<0 },` |
|         - | 1963 | `		{ "chgrp",             5, 1u<<0 },` |
|         - | 1964 | `		{ "chown",             5, 1u<<0 },` |
|         - | 1965 | `		{ "rename",            6, (1u<<0)\|(1u<<1) },` |
|         - | 1966 | `		{ "copy",              4, (1u<<0)\|(1u<<1) },` |
|         - | 1967 | `		{ "link",              4, (1u<<0)\|(1u<<1) },` |
|         - | 1968 | `		{ "symlink",           7, (1u<<0)\|(1u<<1) },` |
|         - | 1969 | `		{ "readlink",          8, 1u<<0 },` |
|         - | 1970 | `		{ "realpath",          8, 1u<<0 },` |
|         - | 1971 | `		{ "stream_resolve_include_path", 27, 1u<<0 },` |
|         - | 1972 | `		/* Not a path at all: php reads inet_pton()'s $ip with the same` |
|         - | 1973 | `		 * NUL-refusing macro, and answers the same ValueError for a name` |
|         - | 1974 | `		 * that carries one. */` |
|         - | 1975 | `		{ "inet_pton",         9, 1u<<0 },` |
|         - | 1976 | `		/* Directories */` |
|         - | 1977 | `		{ "mkdir",             5, 1u<<0 },` |
|         - | 1978 | `		{ "rmdir",             5, 1u<<0 },` |
|         - | 1979 | `		{ "opendir",           7, 1u<<0 },` |
|         - | 1980 | `		{ "dir",               3, 1u<<0 },` |
|         - | 1981 | `		{ "scandir",           7, 1u<<0 },` |
|         - | 1982 | `		{ "chdir",             5, 1u<<0 },` |
|         - | 1983 | `		{ "chroot",            6, 1u<<0 },` |
|         - | 1984 | `		{ "glob",              4, 1u<<0 },` |
|         - | 1985 | `		{ "tempnam",           7, (1u<<0)\|(1u<<1) },` |
|         - | 1986 | `		{ "disk_free_space",  15, 1u<<0 },` |
|         - | 1987 | `		{ "disk_total_space", 16, 1u<<0 },` |
|         - | 1988 | `		{ "diskfreespace",    13, 1u<<0 },` |
|         - | 1989 | `		/* Not paths at all, and php screens them exactly as if they were: the` |
|         - | 1990 | `		 * datetime a FORMAT is read against is Z_PARAM_PATH_STR at every door` |
|         - | 1991 | `		 * that takes one, so a NUL inside it is the same catchable ValueError.` |
|         - | 1992 | ``		 * Only these five; `new DateTime($s)`, `date_create()`, `modify()` and`` |
|         - | 1993 | ``		 * `date_parse()` take an ordinary string there and read up to the NUL. */`` |
|         - | 1994 | `		{ "date_parse_from_format",                22, 1u<<1 },` |
|         - | 1995 | `		{ "date_create_from_format",               23, 1u<<1 },` |
|         - | 1996 | `		{ "date_create_immutable_from_format",     33, 1u<<1 },` |
|         - | 1997 | `		{ "DateTime::createFromFormat",            26, 1u<<1 },` |
|         - | 1998 | `		{ "DateTimeImmutable::createFromFormat",   35, 1u<<1 },` |
|         - | 1999 | ``		/* ext/sqlite3's two doors onto a database FILE. ext/pdo's `sqlite:` DSN is`` |
|         - | 2000 | `		 * not one of them: php parses a DSN before any of it becomes a path, and` |
|         - | 2001 | `		 * reads it up to the NUL. */` |
|         - | 2002 | `		{ "SQLite3::__construct",                  20, 1u<<0 },` |
|         - | 2003 | `		{ "SQLite3::open",                         13, 1u<<0 },` |
|         - | 2004 | `		/* Not a path either, and php screens it exactly as if it were: BOTH of` |
|         - | 2005 | `		 * bindtextdomain's arguments are Z_PARAM_PATH, so a NUL in the DOMAIN is` |
|         - | 2006 | `		 * the same catchable ValueError the directory gets. The other nine` |
|         - | 2007 | `		 * gettext doors take ordinary strings and read up to the NUL. */` |
|         - | 2008 | `		{ "bindtextdomain",   14, (1u<<0)\|(1u<<1) },` |
|         - | 2009 | `		/* ext/posix's five path doors, which php reads with the same macro. */` |
|         - | 2010 | `		{ "posix_access",     12, 1u<<0 },` |
|         - | 2011 | `		{ "posix_eaccess",    13, 1u<<0 },` |
|         - | 2012 | `		{ "posix_mkfifo",     12, 1u<<0 },` |
|         - | 2013 | `		{ "posix_mknod",      11, 1u<<0 },` |
|         - | 2014 | `		{ "posix_pathconf",   14, 1u<<0 },` |
|         - | 2015 | `		/* ext/fileinfo: the name a type is asked about, and the database the` |
|         - | 2016 | `		 * two openers name, are php Z_PARAM_PATH arguments -- so a NUL in one` |
|         - | 2017 | `		 * is the catchable ValueError rather than a truncated read. */` |
|         - | 2018 | `		{ "finfo_file",            10, 1u<<1 },` |
|         - | 2019 | `		{ "finfo_open",            10, 1u<<1 },` |
|         - | 2020 | `		{ "finfo::file",           11, 1u<<0 },` |
|         - | 2021 | `		{ "finfo::__construct",    18, 1u<<1 },` |
|         - | 2022 | `		{ "mime_content_type",     17, 1u<<0 },` |
|         - | 2023 | `		/* ext/simplexml's file door takes a php path argument too. */` |
|         - | 2024 | `		{ "simplexml_load_file",   19, 1u<<0 },` |
|         - | 2025 | `		/* Path-shaped settings and the pattern matcher */` |
|         - | 2026 | `		{ "fnmatch",           7, (1u<<0)\|(1u<<1) },` |
|         - | 2027 | `		{ "set_include_path", 16, 1u<<0 },` |
|         - | 2028 | `		{ "session_save_path", 17, 1u<<0 },` |
|         - | 2029 | `		{ "error_log",         9, 1u<<2 },` |
|         - | 2030 | `		/* Commands handed to the shell — and the two escapers, which php screens` |
|         - | 2031 | `		 * the same way even though neither of them runs anything: a NUL in what a` |
|         - | 2032 | `		 * script is about to hand a shell is refused where it is WRITTEN. */` |
|         - | 2033 | `		{ "shell_exec",       10, 1u<<0 },` |
|         - | 2034 | `		{ "popen",             5, 1u<<0 },` |
|         - | 2035 | `		{ "escapeshellarg",   14, 1u<<0 },` |
|         - | 2036 | `		{ "escapeshellcmd",   14, 1u<<0 },` |
|         - | 2037 | `		{ "exec",              4, 1u<<0 },` |
|         - | 2038 | `		{ "system",            6, 1u<<0 },` |
|         - | 2039 | `		{ "passthru",          8, 1u<<0 },` |
|         - | 2040 | `		/* The SPL path constructors, which php screens identically and reports` |
|         - | 2041 | ``		 * under their QUALIFIED name (`SplFileInfo::__construct(): Argument #1`` |
|         - | 2042 | ``		 * ($filename) …`). They are native methods, so their signature reaches this`` |
|         - | 2043 | `		 * screen the same way a builtin's does. */` |
|         - | 2044 | `		{ "SplFileInfo::__construct",                24, 1u<<0 },` |
|         - | 2045 | `		{ "DirectoryIterator::__construct",          30, 1u<<0 },` |
|         - | 2046 | `		{ "FilesystemIterator::__construct",         31, 1u<<0 },` |
|         - | 2047 | `		{ "RecursiveDirectoryIterator::__construct", 39, 1u<<0 },` |
|         - | 2048 | `		{ "GlobIterator::__construct",                25, 1u<<0 },` |
|         - | 2049 | `	};` |
|         - | 2050 | `	sxu32 i;` |
|     26213 | 2051 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|       ! 0 | 2052 | `		return 0;` |
|         - | 2053 | `	}` |
|   1813877 | 2054 | `	for( i = 0 ; i < SX_ARRAYSIZE(aPath) ; ++i ){` |
|   1789626 | 2055 | `		if( pName->nByte == aPath[i].nByte` |
|    925955 | 2056 | `		 && SyStrnicmp(pName->zString,aPath[i].zName,pName->nByte) == 0 ){` |
|      1967 | 2057 | `			return aPath[i].mask;` |
|         - | 2058 | `		}` |
|    884143 | 2059 | `	}` |
|     24251 | 2060 | `	return 0;` |
|     12965 | 2061 | `}` |
|         - | 2062 | `/*` |
|         - | 2063 | ` * Does this argument carry a NUL byte? Only a STRING can: every other scalar` |
|         - | 2064 | ` * renders through the number/bool formatters, which emit none. An OBJECT is` |
|         - | 2065 | ` * coerced by the caller before asking (php's ZPP order), so by the time this` |
|         - | 2066 | ` * runs a Stringable is already the string it produced.` |
|         - | 2067 | ` */` |
|    117486 | 2068 | `static int VmArgHasNulByte(ph7_value *pArg)` |
|         5 | 2069 | `{` |
|         - | 2070 | `	const char *zStr;` |
|         - | 2071 | `	sxu32 n, nLen;` |
|    117491 | 2072 | `	if( (pArg->iFlags & MEMOBJ_STRING) == 0 ){` |
|        15 | 2073 | `		return 0;` |
|         - | 2074 | `	}` |
|    117479 | 2075 | `	zStr = (const char *)SyBlobData(&pArg->sBlob);` |
|    117479 | 2076 | `	nLen = SyBlobLength(&pArg->sBlob);` |
|   7583005 | 2077 | `	for( n = 0 ; n < nLen ; ++n ){` |
|   7465659 | 2078 | `		if( zStr[n] == 0 ){` |
|       132 | 2079 | `			return 1;` |
|         - | 2080 | `		}` |
|   3866084 | 2081 | `	}` |
|    117351 | 2082 | `	return 0;` |
|     58640 | 2083 | `}` |
|         - | 2084 | `/*` |
|         - | 2085 | ` * Does php's strict_types rule refuse this argument for the declared type?` |
|         - | 2086 | ` *` |
|         - | 2087 | `` * A `declare(strict_types=1)` file gets NO scalar coercion at an internal call`` |
|         - | 2088 | ` * either — php applies the same rule to a builtin, a native method and a userland` |
|         - | 2089 | `` * function, and the single exception is the int -> float widening. So `trim(5)`,`` |
|         - | 2090 | `` * `sqrt("4")`, `str_repeat("a", 2.0)` and `in_array($n, $a, 1)` are all TypeErrors`` |
|         - | 2091 | ` * there, where the weak-mode screen below (which is the only one PHL had) coerces` |
|         - | 2092 | ` * and computes.` |
|         - | 2093 | ` *` |
|         - | 2094 | ` * Only the arms a scalar could otherwise satisfy are decided here; an array, a` |
|         - | 2095 | ` * resource, a null and a class-typed mismatch are the weak screen's, and its` |
|         - | 2096 | ` * verdicts stand in both modes.` |
|         - | 2097 | ` */` |
|       432 | 2098 | `static int VmStrictArgRefused(ph7_value *pArg,const char *zType,int nType)` |
|         3 | 2099 | `{` |
|         - | 2100 | `	/* Tested in ph7_type_name()'s own order, so the branch taken and the name the` |
|         - | 2101 | `	 * refusal reports can never disagree. FLOAT comes before INT on purpose:` |
|         - | 2102 | `	 * ph7_value_is_int() is deliberately lenient — an integer-valued real caches an` |
|         - | 2103 | ``	 * int and answers TRUE — and `str_repeat("a", 2.0)` is php's TypeError, not an`` |
|         - | 2104 | `	 * accepted int. */` |
|       435 | 2105 | `	if( ph7_value_is_bool(pArg) ){` |
|        12 | 2106 | `		return !VmSigTypeHas(zType,nType,"bool")` |
|         7 | 2107 | `		    && !VmSigTypeHas(zType,nType,"true")` |
|        11 | 2108 | `		    && !VmSigTypeHas(zType,nType,"false");` |
|         - | 2109 | `	}` |
|       427 | 2110 | `	if( ph7_value_is_float(pArg) ){` |
|         7 | 2111 | `		return !VmSigTypeHas(zType,nType,"float");` |
|         - | 2112 | `	}` |
|       421 | 2113 | `	if( ph7_value_is_int(pArg) ){` |
|         - | 2114 | `		/* int -> float is the one widening strict mode keeps. */` |
|        96 | 2115 | `		return !VmSigTypeHas(zType,nType,"int") && !VmSigTypeHas(zType,nType,"float");` |
|         - | 2116 | `	}` |
|       327 | 2117 | `	if( ph7_value_is_string(pArg) ){` |
|         - | 2118 | ``		/* `callable` is not a coercion: a function-name string satisfies it in both`` |
|         - | 2119 | `		 * modes (array_map('strtoupper', …) under strict is php-legal). */` |
|       214 | 2120 | `		return !VmSigTypeHas(zType,nType,"string") && !VmSigTypeHas(zType,nType,"callable");` |
|         - | 2121 | `	}` |
|       115 | 2122 | `	if( ph7_value_is_object(pArg) ){` |
|         - | 2123 | ``		/* An object reaches a `string` parameter only through __toString(), which is`` |
|         - | 2124 | `		 * a coercion strict mode does not perform. Every other arm is the weak` |
|         - | 2125 | `		 * screen's decision. */` |
|        49 | 2126 | `		return VmSigTypeHas(zType,nType,"string")` |
|        24 | 2127 | `		    && !VmSigTypeHas(zType,nType,"object")` |
|         2 | 2128 | `		    && !VmSigTypeHas(zType,nType,"iterable")` |
|         2 | 2129 | `		    && !VmSigTypeHas(zType,nType,"callable")` |
|        47 | 2130 | `		    && !VmSigTypeHasClass(zType,nType);` |
|         - | 2131 | `	}` |
|        69 | 2132 | `	return 0;` |
|       219 | 2133 | `}` |
|         - | 2134 | `/*` |
|         - | 2135 | ` * The next parameter of a signature starting at *pzCur, or 0 when the screen must` |
|         - | 2136 | ` * STOP -- a malformed row with no '$', a variadic tail (whose type applies to every` |
|         - | 2137 | ` * argument after it), or the end of the text. Advances *pzCur past the parameter.` |
|         - | 2138 | ` *` |
|         - | 2139 | ` * This is the walk VmEnforceBuiltinArgTypes used to do inline, moved out unchanged so` |
|         - | 2140 | ` * that it has exactly ONE implementation: VmArgScreenStamp drives it once per builtin` |
|         - | 2141 | ` * to build the cached table, and the screen drives it per call when there is no table.` |
|         - | 2142 | ` * The cached form is therefore correct by construction rather than by inspection.` |
|         - | 2143 | ` */` |
|    123681 | 2144 | `static int VmArgScreenNext(const char **pzCur,const char *zEnd,VmArgScreenParam *pOut)` |
|         5 | 2145 | `{` |
|    123686 | 2146 | `	const char *zCur = *pzCur;` |
|         - | 2147 | `	const char *zType,*zName,*zStop;` |
|         - | 2148 | `	int nType,nName,bByRef;` |
|    123686 | 2149 | `	if( zCur >= zEnd ){` |
|     41620 | 2150 | `		return 0;` |
|         - | 2151 | `	}` |
|         - | 2152 | `	/* Parameter = "<type> $<name>[ = <default>]"; the type is whatever` |
|         - | 2153 | `	 * precedes the '$', and an empty type means "untyped" (no screen). */` |
|    124301 | 2154 | `	while( zCur < zEnd && zCur[0] == ' ' ){` |
|     42235 | 2155 | `		zCur++;` |
|         5 | 2156 | `	}` |
|     82071 | 2157 | `	zStop = zCur;` |
|   1499527 | 2158 | `	while( zStop < zEnd && zStop[0] != ',' ){` |
|   1417461 | 2159 | `		if( zStop[0] == '\'' \|\| zStop[0] == '"' ){` |
|      4289 | 2160 | `			zStop = VmSigSkipQuoted(zStop);` |
|      4289 | 2161 | `			if( zStop >= zEnd ){` |
|       ! 0 | 2162 | `				break;` |
|         - | 2163 | `			}` |
|      2136 | 2164 | `		}` |
|   1417461 | 2165 | `		zStop++;` |
|         5 | 2166 | `	}` |
|     82071 | 2167 | `	zName = zCur;` |
|    597713 | 2168 | `	while( zName < zStop && zName[0] != '$' ){` |
|    515647 | 2169 | `		zName++;` |
|         5 | 2170 | `	}` |
|     82071 | 2171 | `	if( zName >= zStop ){` |
|       ! 0 | 2172 | `		return 0; /* malformed / no parameter name -- stop screening */` |
|         - | 2173 | `	}` |
|     82071 | 2174 | `	if( zName >= zCur + 3 && SyMemcmp(zName - 3,"...",3) == 0 ){` |
|      5399 | 2175 | `		return 0; /* variadic tail: stop (its type applies to the rest) */` |
|         - | 2176 | `	}` |
|     76677 | 2177 | `	zType = zCur;` |
|     76677 | 2178 | `	nType = (int)(zName - zCur);` |
|         - | 2179 | ``	/* A `~Type $p` row is php's stub-versus-body mismatch: the type php DECLARES`` |
|         - | 2180 | `	 * (which Reflection must report) is looser than the one its C body asks for, so` |
|         - | 2181 | `	 * the screen stands aside and the builtin raises the TypeError itself.` |
|         - | 2182 | `	 * RecursiveCachingIterator::__construct is the first: it is declared` |
|         - | 2183 | ``	 * `Iterator $iterator` and refuses anything that is not a RecursiveIterator. */`` |
|     76677 | 2184 | `	pOut->bStub = (sxu8)((nType > 0 && zType[0] == '~') ? 1 : 0);` |
|         - | 2185 | `	/* Trim the trailing spaces and the by-ref marker of "array &$array" */` |
|     76677 | 2186 | `	bByRef = 0;` |
|    215739 | 2187 | `	while( nType > 0 && (zType[nType-1] == ' ' \|\| zType[nType-1] == '&') ){` |
|     70701 | 2188 | `		if( zType[nType-1] == '&' ){` |
|      2973 | 2189 | `			bByRef = 1;` |
|      1466 | 2190 | `		}` |
|     70701 | 2191 | `		nType--;` |
|         5 | 2192 | `	}` |
|     76677 | 2193 | `	zName++; /* skip '$' */` |
|     76677 | 2194 | `	nName = 0;` |
|    601019 | 2195 | `	while( &zName[nName] < zStop && zName[nName] != ' ' && zName[nName] != '=' ){` |
|    524347 | 2196 | `		nName++;` |
|         5 | 2197 | `	}` |
|     76677 | 2198 | `	pOut->zType  = zType;` |
|     76677 | 2199 | `	pOut->nType  = (sxu16)nType;` |
|     76677 | 2200 | `	pOut->zName  = zName;` |
|     76677 | 2201 | `	pOut->nName  = (sxu16)nName;` |
|     76677 | 2202 | `	pOut->bByRef = (sxu8)bByRef;` |
|         - | 2203 | `	/* Which arms this type has, asked ONCE. Every bit is set by calling the function` |
|         - | 2204 | `	 * that used to answer it per argument, so the mask cannot say something the walk` |
|         - | 2205 | `	 * would not -- the same construction the parse above uses, for the same reason:` |
|         - | 2206 | `	 * this screen decides TypeErrors. */` |
|         - | 2207 | `	{` |
|     76677 | 2208 | `		sxu32 m = 0;` |
|     76677 | 2209 | `		if( VmSigTypeHas(zType,nType,"mixed")    ){ m \|= VMSIG_MIXED;    }` |
|     76677 | 2210 | `		if( VmSigTypeHas(zType,nType,"array")    ){ m \|= VMSIG_ARRAY;    }` |
|     76677 | 2211 | `		if( VmSigTypeHas(zType,nType,"iterable") ){ m \|= VMSIG_ITERABLE; }` |
|     76677 | 2212 | `		if( VmSigTypeHas(zType,nType,"callable") ){ m \|= VMSIG_CALLABLE; }` |
|     76677 | 2213 | `		if( VmSigTypeHas(zType,nType,"object")   ){ m \|= VMSIG_OBJECT;   }` |
|     76677 | 2214 | `		if( VmSigTypeHas(zType,nType,"string")   ){ m \|= VMSIG_STRING;   }` |
|     76677 | 2215 | `		if( VmSigTypeHas(zType,nType,"null")     ){ m \|= VMSIG_NULL;     }` |
|     76677 | 2216 | `		if( VmSigTypeHas(zType,nType,"int")      ){ m \|= VMSIG_INT;      }` |
|     76677 | 2217 | `		if( VmSigTypeHas(zType,nType,"float")    ){ m \|= VMSIG_FLOAT;    }` |
|     76677 | 2218 | `		if( VmSigTypeHas(zType,nType,"bool")     ){ m \|= VMSIG_BOOL;     }` |
|     76677 | 2219 | `		if( VmSigTypeHas(zType,nType,"true")     ){ m \|= VMSIG_TRUE;     }` |
|     76677 | 2220 | `		if( VmSigTypeHas(zType,nType,"false")    ){ m \|= VMSIG_FALSE;    }` |
|     76677 | 2221 | `		if( VmSigTypeHas(zType,nType,"resource") ){ m \|= VMSIG_RESOURCE; }` |
|     76677 | 2222 | `		if( VmSigTypeHasClass(zType,nType)       ){ m \|= VMSIG_CLASS;    }` |
|     76677 | 2223 | `		if( VmSigTypeIsIntOnly(zType,nType)      ){ m \|= VMSIG_INTONLY;  }` |
|     76677 | 2224 | `		if( VmSigTypeIsArrayOnly(zType,nType)    ){ m \|= VMSIG_ARRAYONLY;}` |
|     76677 | 2225 | `		pOut->nMask = m;` |
|         - | 2226 | `	}` |
|     76677 | 2227 | `	*pzCur = (zStop < zEnd) ? zStop + 1 : zEnd;` |
|     76677 | 2228 | `	return 1;` |
|     61124 | 2229 | `}` |
|         - | 2230 | `/*` |
|         - | 2231 | ` * Work out this builtin's parameter table once and keep it on its record, beside the` |
|         - | 2232 | ` * two name questions and the signature length. Two passes over the same walk: count,` |
|         - | 2233 | ` * then fill. Leaves aSigParam at 0 when the signature yields no parameters or the` |
|         - | 2234 | ` * allocation fails, and the screen then walks the text per call exactly as before --` |
|         - | 2235 | ` * the fallback is the same code, so there is no second set of answers to keep in step.` |
|         - | 2236 | ` *` |
|         - | 2237 | ` * The table lives in the VM's allocator and is reclaimed with it, the same lifetime as` |
|         - | 2238 | ` * the name strdup beside it in PH7_NewForeignFunction.` |
|         - | 2239 | ` */` |
|     26208 | 2240 | `static void VmArgScreenStamp(ph7_vm *pVm,ph7_user_func *pFunc,const char *zSig,sxu32 nSigLen)` |
|         5 | 2241 | `{` |
|     26213 | 2242 | `	const char *zEnd = &zSig[nSigLen];` |
|         - | 2243 | `	const char *zCur;` |
|         - | 2244 | `	VmArgScreenParam sTmp;` |
|         - | 2245 | `	VmArgScreenParam *aParam;` |
|         - | 2246 | `	sxu32 n;` |
|     64549 | 2247 | `	for( n = 0, zCur = zSig ; VmArgScreenNext(&zCur,zEnd,&sTmp) ; ++n ){` |
|         - | 2248 | `		/* counting only */` |
|     18943 | 2249 | `	}` |
|     26213 | 2250 | `	if( n < 1 ){` |
|      7298 | 2251 | `		return;` |
|         - | 2252 | `	}` |
|     28260 | 2253 | `	aParam = (VmArgScreenParam *)SyMemBackendPoolAlloc(&pVm->sAllocator,` |
|      9340 | 2254 | `		n * (sxu32)sizeof(VmArgScreenParam));` |
|     18920 | 2255 | `	if( aParam == 0 ){` |
|       ! 0 | 2256 | `		return;` |
|         - | 2257 | `	}` |
|     57256 | 2258 | `	for( n = 0, zCur = zSig ; VmArgScreenNext(&zCur,zEnd,&aParam[n]) ; ++n ){` |
|         - | 2259 | `		/* filling */` |
|     18943 | 2260 | `	}` |
|     18920 | 2261 | `	pFunc->aSigParam = aParam;` |
|     18920 | 2262 | `	pFunc->nSigParam = (sxu16)n;` |
|     12965 | 2263 | `}` |
|         - | 2264 | `/*` |
|         - | 2265 | ` * PHP-8 ZPP type enforcement for host functions, driven by the aBuiltinSig[]` |
|         - | 2266 | ` * declaration (band A #7). Screens only the arguments that php can NEVER coerce` |
|         - | 2267 | ` * into a declared scalar parameter — arrays, resources, and objects without a` |
|         - | 2268 | ` * __toString() — and throws the catchable TypeError php throws, before the C` |
|         - | 2269 | ` * routine runs. Without this an array argument reached the builtin and was` |
|         - | 2270 | ` * stringified to the literal "Array" (strrev(['a']) returned "yarrA").` |
|         - | 2271 | ` *` |
|         - | 2272 | ` * Deliberately narrow: scalar-to-scalar coercion (and its deprecations) stays` |
|         - | 2273 | ` * with the per-builtin ZPP helpers. Those emit E_DEPRECATED, which does NOT` |
|         - | 2274 | ` * abort the call, so a central copy would double-fire — the trap that sank the` |
|         - | 2275 | ` * first central-ZPP attempt. A TypeError aborts, so there is nothing to double.` |
|         - | 2276 | ` */` |
|   6736786 | 2277 | `PH7_PRIVATE sxi32 VmEnforceBuiltinArgTypes(` |
|         - | 2278 | `	ph7_context *pCtx,    /* Call context (for the throw) */` |
|         - | 2279 | `	ph7_user_func *pFunc, /* Callee */` |
|         - | 2280 | `	int nGiven,           /* Argument count */` |
|         - | 2281 | `	ph7_value **apArg     /* Arguments */` |
|         - | 2282 | `	)` |
|         5 | 2283 | `{` |
|         - | 2284 | `	/*` |
|         - | 2285 | `	 * Builtins whose own argument check is php-exact and VALUE-based rather than` |
|         - | 2286 | `	 * type-based must not be pre-empted here, or their message is lost. Same rule` |
|         - | 2287 | `	 * the aBuiltinArity[] table follows: a builtin that already says what php says` |
|         - | 2288 | `	 * stays off the shared screen. get_class_vars() takes any stringifiable value` |
|         - | 2289 | `	 * and reports "must be a valid class name, Array given"; get_class_methods() is` |
|         - | 2290 | `	 * the same shape with php's other wording ("must be an object or a valid class` |
|         - | 2291 | ``	 * name, int given") — the declared `object\|string` never appears in either.`` |
|         - | 2292 | `	 *` |
|         - | 2293 | `	 * strtr() is here for a structural reason: php declares it as two OVERLOADS` |
|         - | 2294 | `	 * dispatched on arity — strtr(string, array) and strtr(string, string, string)` |
|         - | 2295 | `` 	 * — so the expected type of $from is `array` with two arguments and `string` `` |
|         - | 2296 | ``	 * with three. One signature cannot say that (the stub's `array\|string` is the`` |
|         - | 2297 | `	 * union of the two, which is the wording php never uses), so the builtin does` |
|         - | 2298 | `	 * its own dispatch through PH7_ArgSatisfiesString(), the same rule as here.` |
|         - | 2299 | `	 *` |
|         - | 2300 | ``	 * implode() is the same structure: `array\|string $separator` is what the two`` |
|         - | 2301 | `	 * ARITIES accept between them, never what one call can use. Once an $array` |
|         - | 2302 | `	 * argument is present php has resolved the overload and reports` |
|         - | 2303 | ``	 * `must be of type string`, and with the array in position #1 it reports`` |
|         - | 2304 | ``	 * `must be of type string, array given` against #1 rather than a #2 error.`` |
|         - | 2305 | `	 * PH7_builtin_implode words all of that itself.` |
|         - | 2306 | `	 *` |
|         - | 2307 | `	 * Its alias join() is here for the same reason and then some: php 8.5 does not` |
|         - | 2308 | `	 * word the two the same, so the builtin reproduces BOTH orders keyed on the` |
|         - | 2309 | `	 * invoked name (see PH7_builtin_implode's header for the value-for-value` |
|         - | 2310 | `	 * table against 8.5.8). php's own asymmetry between a target and its alias,` |
|         - | 2311 | `	 * reproduced rather than smoothed over — parity is binding (the scope policy).` |
|         - | 2312 | `	 *` |
|         - | 2313 | `	 * number_format() is here because php's DECLARED type and its REFUSAL text` |
|         - | 2314 | ``	 * disagree: the stub says `float $num` (which is what Reflection prints) while`` |
|         - | 2315 | `	 * the ZPP macro behind it is Z_PARAM_NUMBER, whose TypeError says` |
|         - | 2316 | ``	 * `must be of type int\|float`. One row cannot say both, so the row carries the`` |
|         - | 2317 | `	 * declared type for Reflection and the builtin words every refusal itself.` |
|         - | 2318 | `	 *` |
|         - | 2319 | `	 * RecursiveIteratorIterator::__construct() is the first NATIVE METHOD here, and` |
|         - | 2320 | `	 * it is the same disagreement one level up: php's stub declares` |
|         - | 2321 | ``	 * `Traversable $iterator` (what Reflection prints) while its ZPP is a bare "o",`` |
|         - | 2322 | ``	 * whose TypeError says `must be of type object`. A native method's diagnostic`` |
|         - | 2323 | `	 * name is the QUALIFIED one, so the row below matches it and nothing else.` |
|         - | 2324 | `	 *` |
|         - | 2325 | `	 * The array_udiff/array_uintersect u-variant family is here for its ORDER:` |
|         - | 2326 | `	 * php validates the trailing comparison callback(s) before ANY of the` |
|         - | 2327 | `	 * arrays — array_diff_ukey(123,[1],456) names Argument #3, not #1 — and a` |
|         - | 2328 | `	 * positional screen cannot say that. HashmapUVariant performs the whole` |
|         - | 2329 | `	 * php sequence itself (callbacks, then Argument #1, then the middles).` |
|         - | 2330 | `	 */` |
|         - | 2331 | `	static const char *azSelfChecked[] = { "get_class_vars", "get_class_methods", "strtr",` |
|         - | 2332 | `		"implode", "join", "number_format", "RecursiveIteratorIterator::__construct",` |
|         - | 2333 | `		"array_udiff", "array_udiff_assoc", "array_udiff_uassoc",` |
|         - | 2334 | `		"array_uintersect", "array_uintersect_assoc", "array_uintersect_uassoc",` |
|         - | 2335 | `		"array_diff_uassoc", "array_diff_ukey",` |
|         - | 2336 | `		"array_intersect_uassoc", "array_intersect_ukey" };` |
|   6736791 | 2337 | `	const char *zSig = pFunc->zSig;` |
|         - | 2338 | `	const char *zCur, *zEnd;` |
|   6736791 | 2339 | `	int iArg = 0;` |
|         - | 2340 | `	/* The CALL site's file mode, stamped by the compiler onto this call's argument` |
|         - | 2341 | `	 * map (weak when there is no map — a call that carries no compile-time metadata` |
|         - | 2342 | `	 * was written in a weak-mode file, since a strict one always attaches one). */` |
|   6736791 | 2343 | `	int bStrict = (pCtx->pArgMap && pCtx->pArgMap->bStrict) ? 1 : 0;` |
|         - | 2344 | `	sxu32 nPathMask;` |
|   6736791 | 2345 | `	if( zSig == 0 ){` |
|    261830 | 2346 | `		return SXRET_OK;` |
|         - | 2347 | `	}` |
|         - | 2348 | `	/* All three of the questions below are about the DECLARATION, which cannot change:` |
|         - | 2349 | `	 * two are about the NAME -- and used to be answered by SCANNING a table on every` |
|         - | 2350 | `	 * builtin call, seventeen names here and about seventy in VmBuiltinPathMask, two` |
|         - | 2351 | `	 * SyStrlen calls per row -- and the third is the length of the signature TEXT,` |
|         - | 2352 | `	 * which zEnd below used to measure on every call. Worked out once and kept on the` |
|         - | 2353 | `	 * function's own record. */` |
|   6474966 | 2354 | `	if( !pFunc->bScreenStamped ){` |
|         - | 2355 | `		int iSelf;` |
|     26213 | 2356 | `		pFunc->nPathMask = VmBuiltinPathMask(&pFunc->sName);` |
|     26213 | 2357 | `		pFunc->nSigLen = (sxu32)SyStrlen(zSig);` |
|     26213 | 2358 | `		VmArgScreenStamp(pCtx->pVm,pFunc,zSig,pFunc->nSigLen);` |
|    467022 | 2359 | `		for( iSelf = 0 ; iSelf < (int)SX_ARRAYSIZE(azSelfChecked) ; ++iSelf ){` |
|    441226 | 2360 | `			const char *zSelf = azSelfChecked[iSelf];` |
|    441221 | 2361 | `			if( SyStrncmp(pFunc->sName.zString,zSelf,pFunc->sName.nByte) == 0` |
|    218860 | 2362 | `			 && SyStrlen(zSelf) == pFunc->sName.nByte ){` |
|       417 | 2363 | `				pFunc->bSelfChecked = 1;` |
|       417 | 2364 | `				break;` |
|         - | 2365 | `			}` |
|    218004 | 2366 | `		}` |
|     26213 | 2367 | `		pFunc->bScreenStamped = 1;` |
|     12960 | 2368 | `	}` |
|   6474966 | 2369 | `	if( pFunc->bSelfChecked ){` |
|     53468 | 2370 | `		return SXRET_OK;` |
|         - | 2371 | `	}` |
|   6421503 | 2372 | `	nPathMask = pFunc->nPathMask;` |
|   6421503 | 2373 | `	zCur = zSig;` |
|   6421503 | 2374 | `	zEnd = &zSig[pFunc->nSigLen];   /* measured once, above -- never per call */` |
|  16156927 | 2375 | `	for( iArg = 0 ; iArg < nGiven ; ++iArg ){` |
|         - | 2376 | `		VmArgScreenParam sParam;` |
|         - | 2377 | `		const char *zType, *zName;` |
|         - | 2378 | `		int nType, nName, bByRef;` |
|         - | 2379 | `		sxu32 nMask;` |
|         - | 2380 | `		ph7_value *pArg;` |
|         - | 2381 | `		char zGivenBuf[64];` |
|         - | 2382 | `		/* The parameter this argument is screened against. Worked out once per` |
|         - | 2383 | `		 * builtin when the table could be built, and by the same walk per call when` |
|         - | 2384 | `		 * it could not; either way the screen stops where the walk stopped. */` |
|   9765965 | 2385 | `		if( pFunc->aSigParam ){` |
|   9764079 | 2386 | `			if( iArg >= (int)pFunc->nSigParam ){` |
|     28326 | 2387 | `				break;` |
|         - | 2388 | `			}` |
|   9736701 | 2389 | `			sParam = pFunc->aSigParam[iArg];` |
|   4871542 | 2390 | `		}else if( !VmArgScreenNext(&zCur,zEnd,&sParam) ){` |
|      1891 | 2391 | `			break;` |
|         - | 2392 | `		}` |
|   9736701 | 2393 | `		if( sParam.bStub ){` |
|       840 | 2394 | `			continue; /* the builtin raises its own TypeError -- see VmArgScreenNext */` |
|         - | 2395 | `		}` |
|   9735871 | 2396 | `		zType  = sParam.zType;` |
|   9735871 | 2397 | `		nType  = (int)sParam.nType;` |
|   9735871 | 2398 | `		zName  = sParam.zName;` |
|   9735871 | 2399 | `		nName  = (int)sParam.nName;` |
|   9735871 | 2400 | `		bByRef = sParam.bByRef;` |
|   9735871 | 2401 | `		nMask  = sParam.nMask;   /* which arms this type has, worked out once per parameter */` |
|   9735871 | 2402 | `		pArg = apArg[iArg];` |
|   9735866 | 2403 | `		if( bByRef && pArg->nIdx == SXU32_HIGH` |
|     19694 | 2404 | `		 && !(pCtx->pArgMap && pCtx->pArgMap->bArgShapes && !pCtx->pArgMap->bHasNamed) ){` |
|         - | 2405 | `			/* A by-reference parameter handed something with no slot to write back` |
|         - | 2406 | `			 * through -- a literal, a constant, the result of a call. php settles` |
|         - | 2407 | `			 * that at the CALL, before the callee's ZPP runs, so the type screen` |
|         - | 2408 | ``			 * must not speak first: `array_pop('foo')` is`` |
|         - | 2409 | `			 * "could not be passed by reference" and not "must be of type array,` |
|         - | 2410 | `			 * string given".` |
|         - | 2411 | `			 *` |
|         - | 2412 | `			 * Only when this call site carries no argument SHAPES, though. When it` |
|         - | 2413 | `			 * does, PH7_VmScreenByRefArgShapes has already had its say — it refused` |
|         - | 2414 | `			 * the literal and let the call RESULT through with php's notice — and` |
|         - | 2415 | `			 * standing aside here would swallow the type error php still reports for` |
|         - | 2416 | ``			 * the latter (`sort(new stdClass)` is "must be of type array, stdClass`` |
|         - | 2417 | `			 * given", not a silent false). */` |
|        12 | 2418 | `			continue;` |
|         - | 2419 | `		}` |
|   9735861 | 2420 | `		if( nType > 0 && !(nMask & VMSIG_MIXED) ){` |
|   8113641 | 2421 | `			const char *zGiven = 0;` |
|   8113641 | 2422 | `			if( bStrict && VmStrictArgRefused(pArg,zType,nType) ){` |
|         - | 2423 | ``				/* php names the VALUE for a bool here too (`true given`). */`` |
|        37 | 2424 | `				zGiven = VmValueGivenName(pArg,zGivenBuf,sizeof(zGivenBuf));` |
|   8113623 | 2425 | `			}else if( (pArg->iFlags & MEMOBJ_HASHMAP) != 0 ){` |
|    543036 | 2426 | `				if( !(nMask & VMSIG_ARRAY)` |
|    271702 | 2427 | `				 && !(nMask & VMSIG_ITERABLE)` |
|       495 | 2428 | `				 && !(nMask & VMSIG_CALLABLE) ){` |
|       245 | 2429 | `					zGiven = "array";` |
|       125 | 2430 | `				}` |
|   7842026 | 2431 | `			}else if( (pArg->iFlags & MEMOBJ_OBJ) != 0 ){` |
|     60609 | 2432 | `				if( !(nMask & VMSIG_OBJECT)` |
|     53335 | 2433 | `				 && !(nMask & VMSIG_ITERABLE)` |
|     46112 | 2434 | `				 && !(nMask & VMSIG_CALLABLE)` |
|     44093 | 2435 | `				 && !(nMask & VMSIG_CLASS) ){` |
|         - | 2436 | `					/* An object with __toString() still satisfies a string` |
|         - | 2437 | `					 * parameter in weak mode — php coerces it. */` |
|       403 | 2438 | `					int bStringable = (nMask & VMSIG_STRING)` |
|       210 | 2439 | `						&& PH7_ArgSatisfiesString(pArg);` |
|       214 | 2440 | `					if( !bStringable ){` |
|       112 | 2441 | `						zGiven = VmArgTypeName(pArg);` |
|        54 | 2442 | `					}` |
|     60508 | 2443 | `				}else if( (nMask & VMSIG_CLASS)` |
|     50996 | 2444 | `				       && !(nMask & VMSIG_OBJECT)` |
|     41655 | 2445 | `				       && !(nMask & VMSIG_ITERABLE)` |
|     41655 | 2446 | `				       && !(nMask & VMSIG_CALLABLE)` |
|     41660 | 2447 | `				       && !(nMask & VMSIG_STRING) ){` |
|         - | 2448 | `					/* A class-typed parameter given an object of the WRONG class.` |
|         - | 2449 | `					 * Naming a class used to be enough to let ANY object through, so` |
|         - | 2450 | `` 					 * `date_modify($immutable)` and `timezone_name_get($date)` `` |
|         - | 2451 | `					 * answered silently where php raises. Only decided when every` |
|         - | 2452 | `					 * class arm resolves to a declared class: an arm PHL does not` |
|         - | 2453 | `					 * declare cannot be judged, so the parameter stays unscreened. */` |
|     62239 | 2454 | `					if( !VmSigObjSatisfiesClass(pCtx->pVm,zType,nType,` |
|     41487 | 2455 | `						(ph7_class_instance *)pArg->x.pOther) ){` |
|        60 | 2456 | `						zGiven = VmArgTypeName(pArg);` |
|        29 | 2457 | `					}` |
|     20752 | 2458 | `				}` |
|   7540237 | 2459 | `			}else if( (pArg->iFlags & MEMOBJ_NULL) != 0 ){` |
|         - | 2460 | `				/* php only DEPRECATES null for a non-nullable parameter; PHL rejects` |
|         - | 2461 | `				 * it (scope policy). A leading '?' or an explicit "null" arm in a` |
|         - | 2462 | `				 * union declares the parameter nullable. A "callable" parameter is` |
|         - | 2463 | `				 * left to the builtin's own callback check, which words the failure` |
|         - | 2464 | `				 * php's way ("must be a valid callback, no array or string given") —` |
|         - | 2465 | `				 * the same reason get_class_vars() sits on azSelfChecked[]. */` |
|      7701 | 2466 | `				if( zType[0] != '?'` |
|      3936 | 2467 | `				 && !(nMask & VMSIG_NULL)` |
|       152 | 2468 | `				 && !(nMask & VMSIG_CALLABLE) ){` |
|       108 | 2469 | `					zGiven = "null";` |
|        52 | 2470 | `				}` |
|   7506105 | 2471 | `			}else if( (pArg->iFlags & (MEMOBJ_STRING\|MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL)) != 0` |
|   7502253 | 2472 | `			       && ((nMask & VMSIG_CLASS)` |
|   7501629 | 2473 | `			        \|\| (nMask & VMSIG_OBJECT)) ){` |
|         - | 2474 | `				/* A SCALAR against a parameter that can only hold an INSTANCE —` |
|         - | 2475 | ``				 * a named class, or the bare `object` keyword. Every other scalar`` |
|         - | 2476 | `				 * pairing is left to weak-mode coercion, which is why nothing` |
|         - | 2477 | `				 * screened scalars here at all — but no coercion produces an` |
|         - | 2478 | `				 * instance, so php rejects this one. Found converting DateTime:` |
|         - | 2479 | ``				 * `$d->diff('x')` and `new DateTime('now','UTC')` ran on with a`` |
|         - | 2480 | ``				 * string where php raises. The `object` half was still blind when`` |
|         - | 2481 | `				 * WeakReference::create() declared the first such parameter, which` |
|         - | 2482 | `				 * also retires the "graceful degradation" NULL that spl_object_id(),` |
|         - | 2483 | `				 * spl_object_hash() and get_object_vars() used to answer. An arm a` |
|         - | 2484 | `				 * scalar CAN satisfy (a union with string/int/float/bool, or` |
|         - | 2485 | `				 * callable, which a string is) keeps the parameter unscreened —` |
|         - | 2486 | ``				 * and so does an `array` arm, whose refusal php words from the`` |
|         - | 2487 | `				 * builtin's own check rather than from the declared type` |
|         - | 2488 | ``				 * (array_walk's `array\|object &$array` says "must be of type`` |
|         - | 2489 | `				 * array", not "of type array\|object"). */` |
|      3951 | 2490 | `				if( !(nMask & VMSIG_STRING)` |
|      2124 | 2491 | `				 && !(nMask & VMSIG_INT)` |
|       232 | 2492 | `				 && !(nMask & VMSIG_FLOAT)` |
|       146 | 2493 | `				 && !(nMask & VMSIG_BOOL)` |
|       146 | 2494 | `				 && !(nMask & VMSIG_TRUE)` |
|       146 | 2495 | `				 && !(nMask & VMSIG_FALSE)` |
|       146 | 2496 | `				 && !(nMask & VMSIG_ARRAY)` |
|       117 | 2497 | `				 && !(nMask & VMSIG_CALLABLE) ){` |
|         - | 2498 | `					/* php's VALUE name, not the type's: a bool is reported as` |
|         - | 2499 | ``					 * `true`/`false` (the rule Generator::throw()'s own check`` |
|         - | 2500 | `					 * already followed, and which this screen now runs first). */` |
|        82 | 2501 | `					zGiven = VmValueGivenName(pArg,zGivenBuf,sizeof(zGivenBuf));` |
|      3916 | 2502 | `				}else if( (pArg->iFlags & (MEMOBJ_STRING\|MEMOBJ_NULL)) == MEMOBJ_STRING` |
|      3767 | 2503 | `				       && !(nMask & VMSIG_STRING)` |
|      1836 | 2504 | `				       && !(nMask & VMSIG_BOOL)` |
|        32 | 2505 | `				       && !(nMask & VMSIG_TRUE)` |
|        32 | 2506 | `				       && !(nMask & VMSIG_FALSE)` |
|        32 | 2507 | `				       && !(nMask & VMSIG_ARRAY)` |
|        22 | 2508 | `				       && !(nMask & VMSIG_CALLABLE)` |
|        17 | 2509 | `				       && !PH7_MemObjStringIsNumeric(pArg) ){` |
|         - | 2510 | ``					/* The one arm that let this STRING past is `int`/`float`, and it`` |
|         - | 2511 | `					 * only takes a NUMERIC one — no coercion turns a string into an` |
|         - | 2512 | ``					 * instance of the class arm beside it. `round(1.5, 0, "x")` is`` |
|         - | 2513 | ``					 * php's `must be of type RoundingMode\|int, string given`; PHL`` |
|         - | 2514 | `					 * narrowed it to mode 0 and reported the ValueError for an` |
|         - | 2515 | `					 * invalid MODE, which blames the wrong thing. The plain` |
|         - | 2516 | `					 * number-only spelling is screened by the STRING branch below;` |
|         - | 2517 | `					 * a class arm routes the same argument through here instead, so` |
|         - | 2518 | `					 * the rule has to be stated in both places. */` |
|         7 | 2519 | `					zGiven = "string";` |
|         3 | 2520 | `				}` |
|   7500273 | 2521 | `			}else if( (pArg->iFlags & MEMOBJ_REAL) != 0` |
|   3751500 | 2522 | `			       && (nMask & VMSIG_INTONLY)` |
|       858 | 2523 | `			       && !VmDoubleFitsInt((double)pArg->rVal) ){` |
|         - | 2524 | ``				/* A FLOAT against a parameter typed exactly `int` (or `?int`), and`` |
|         - | 2525 | `				 * one no int can hold: a fraction, a magnitude past the signed` |
|         - | 2526 | `				 * 64-bit range, NaN or an infinity. php refuses every one of them` |
|         - | 2527 | `				 * (zend_parse_arg_long's ZEND_DOUBLE_FITS_LONG / is-integral pair,` |
|         - | 2528 | `				 * the fractional case with a deprecation PHL rejects outright by` |
|         - | 2529 | `				 * the scope policy) and the refusal is this screen's own wording.` |
|         - | 2530 | `				 *` |
|         - | 2531 | `				 * PH7_IntArgResolve has always said exactly this, but only for the` |
|         - | 2532 | `` 				 * builtins that CALL it from their own body — so `dechex(1.5)` `` |
|         - | 2533 | ``				 * answered '1', `array_fill(1.5,1,0)` filled from 1, and`` |
|         - | 2534 | ``				 * `strpos("abc","c",1e19)` took the offset as PHP_INT_MIN and`` |
|         - | 2535 | `				 * reported a ValueError about a range it never had. Seventy-five` |
|         - | 2536 | ``				 * `int` parameters across the signature table were unscreened that`` |
|         - | 2537 | `				 * way, and a NATIVE METHOD has no body to call the helper from at` |
|         - | 2538 | `				 * all. Deciding it from the declared type covers both callee kinds` |
|         - | 2539 | `				 * from one place, and the per-builtin helper still stands for the` |
|         - | 2540 | ``				 * message rows this screen cannot reach (the `azSelfChecked` set). */`` |
|        65 | 2541 | `				zGiven = "float";` |
|   7498278 | 2542 | `			}else if( (pArg->iFlags & (MEMOBJ_STRING\|MEMOBJ_NULL)) == MEMOBJ_STRING` |
|   5946365 | 2543 | `			       && ((nMask & VMSIG_INT)` |
|   4392235 | 2544 | `			        \|\| (nMask & VMSIG_FLOAT))` |
|   2197658 | 2545 | `			       && !(nMask & VMSIG_STRING)` |
|   2197236 | 2546 | `			       && !(nMask & VMSIG_ARRAY)` |
|       306 | 2547 | `			       && !(nMask & VMSIG_OBJECT)` |
|       298 | 2548 | `			       && !(nMask & VMSIG_ITERABLE)` |
|       298 | 2549 | `			       && !(nMask & VMSIG_CALLABLE)` |
|       298 | 2550 | `			       && !(nMask & VMSIG_BOOL)` |
|       303 | 2551 | `			       && !(nMask & VMSIG_CLASS) ){` |
|         - | 2552 | ``				/* A STRING against a NUMBER-only parameter — `int`, `float`, or the`` |
|         - | 2553 | ``				 * `int\|float` union, with no arm a string can satisfy. Weak mode`` |
|         - | 2554 | `				 * coerces a NUMERIC one and php refuses every other — "x", "2abc"` |
|         - | 2555 | ``				 * and "0x2" are all `must be of type int, string given` (rule 41: a`` |
|         - | 2556 | `				 * numeric PREFIX is not enough, which is what SyStrIsNumeric would` |
|         - | 2557 | `				 * have accepted). Every BUILTIN with an int parameter already got` |
|         - | 2558 | `				 * this from PH7_IntArgResolve, called from its own body; a native` |
|         - | 2559 | `` 				 * METHOD has no body to call it from, so `ArrayIterator::seek('x')` `` |
|         - | 2560 | ``				 * seeked to 0, `DateTime::setTimestamp('abc')` set 0 and`` |
|         - | 2561 | ``				 * `DOMNodeList::item('zz')` answered element 0 — wrong ANSWERS,`` |
|         - | 2562 | `				 * not missing errors. Screening the declared type here covers both` |
|         - | 2563 | `				 * callee kinds from one place.` |
|         - | 2564 | `				 *` |
|         - | 2565 | `				 * The FLOAT arm is the same hazard one type over, and it was the` |
|         - | 2566 | `				 * half nothing covered: PH7_IntArgResolve has no float twin, so a` |
|         - | 2567 | ``				 * `float $num` builtin that did not hand-roll its own check simply`` |
|         - | 2568 | `				 * converted the string to 0.0 and COMPUTED with it —` |
|         - | 2569 | ``				 * `cos("nope")` answered `float(1)`, `sqrt("nope")` `float(0)`,`` |
|         - | 2570 | ``				 * `log("nope")` `float(-INF)`. Numbers with nothing wrong-looking`` |
|         - | 2571 | `				 * about them, from input php refuses outright.` |
|         - | 2572 | `				 *` |
|         - | 2573 | `				 * The NULL rule stays where it is: PHL rejects null for a` |
|         - | 2574 | `				 * non-nullable parameter by the scope policy, where php deprecates. */` |
|       452 | 2575 | `				if( !PH7_MemObjStringIsNumeric(pArg) ){` |
|       179 | 2576 | `					zGiven = "string";` |
|       215 | 2577 | `				}else if( (nMask & VMSIG_INTONLY) && !VmNumStrFitsInt(pArg) ){` |
|         - | 2578 | `					/* A NUMERIC string an int cannot hold — "1.5", "1e19",` |
|         - | 2579 | `					 * "99999999999999999999". php refuses all three (the fractional` |
|         - | 2580 | `					 * one after a deprecation the scope policy turns into the refusal), and PHL` |
|         - | 2581 | ``					 * narrowed them silently: `dechex("1e19")` answered '1' and`` |
|         - | 2582 | ``					 * `str_repeat("a","99999999999999999999")` took PHP_INT_MAX as`` |
|         - | 2583 | `					 * the count. Same wording, same position as the float arm above,` |
|         - | 2584 | `					 * because php reaches both through one ZPP macro. */` |
|        19 | 2585 | `					zGiven = "string";` |
|         8 | 2586 | `				}` |
|   7498099 | 2587 | `			}else if( (pArg->iFlags & (MEMOBJ_STRING\|MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL)) != 0` |
|   7497944 | 2588 | `			       && (nMask & VMSIG_ARRAYONLY) ){` |
|         - | 2589 | ``				/* A SCALAR against a parameter typed exactly `array`. No coercion`` |
|         - | 2590 | `				 * produces one, so php refuses it -- but the screen exempted every` |
|         - | 2591 | ``				 * `array` arm, union or not, and a whole family had no check of its`` |
|         - | 2592 | `				 * own to fall back on: sort/rsort/ksort/krsort/shuffle and` |
|         - | 2593 | ``				 * usort/uasort/uksort each answered `false` for `sort($notAnArray)`,`` |
|         - | 2594 | `				 * which is also what they answer for a sort that genuinely failed.` |
|         - | 2595 | `				 * call_user_func_array('strlen', 'x') answered false too,` |
|         - | 2596 | `				 * iterator_apply RAN the callback, and getopt/hash/password_hash/` |
|         - | 2597 | `				 * password_needs_rehash/unserialize/fputcsv simply carried on with` |
|         - | 2598 | `				 * the string where an options ARRAY was declared.` |
|         - | 2599 | `				 *` |
|         - | 2600 | `				 * The builtins that DO check (array_keys, in_array, asort, ...) word` |
|         - | 2601 | `				 * it identically, so the screen only pre-empts them -- and corrects` |
|         - | 2602 | `				 * one detail on the way: their ph7_type_name() says "bool" where php` |
|         - | 2603 | ``				 * names the VALUE, `true` or `false`. */`` |
|       257 | 2604 | `				zGiven = VmValueGivenName(pArg,zGivenBuf,sizeof(zGivenBuf));` |
|   7497824 | 2605 | `			}else if( (pArg->iFlags & MEMOBJ_RES) != 0 ){` |
|         - | 2606 | `				/* A class-typed parameter also accepts a resource: several handles php 8` |
|         - | 2607 | `				 * models as objects are still resources here (xml_*'s XMLParser is the` |
|         - | 2608 | `				 * one the signatures already declare php-8-style, for reflection). The` |
|         - | 2609 | `				 * screen would otherwise reject the engine's own parser handle. Recorded` |
|         - | 2610 | `				 * as a recorded divergence — it goes away when those handles become` |
|         - | 2611 | `				 * real objects. */` |
|        12 | 2612 | `				if( !(nMask & VMSIG_RESOURCE)` |
|        15 | 2613 | `				 && !(nMask & VMSIG_CLASS) ){` |
|        15 | 2614 | `					zGiven = "resource";` |
|         6 | 2615 | `				}` |
|         6 | 2616 | `			}` |
|   8113641 | 2617 | `			if( zGiven ){` |
|         - | 2618 | ``				/* php's `object\|array` parameters come from ONE ZPP macro`` |
|         - | 2619 | `				 * (Z_PARAM_ARRAY_OR_OBJECT) and it names only "array" in the` |
|         - | 2620 | `				 * refusal — array_walk(null,…), current(null) and` |
|         - | 2621 | `				 * http_build_query(null) all say "must be of type array". The` |
|         - | 2622 | `				 * SCALAR branch above already encodes that rule by declining to` |
|         - | 2623 | `				 * screen at all; the null and resource branches do screen, so the` |
|         - | 2624 | `				 * reported type has to be corrected here instead. */` |
|      1149 | 2625 | `				if( (nMask & VMSIG_ARRAY) && (nMask & VMSIG_OBJECT) ){` |
|        16 | 2626 | `					zType = "array";` |
|        16 | 2627 | `					nType = (int)sizeof("array")-1;` |
|         7 | 2628 | `				}` |
|      1788 | 2629 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 2630 | `					"%z(): Argument #%d ($%.*s) must be of type %.*s, %s given",` |
|       572 | 2631 | `					&pFunc->sName,iArg + 1,nName,zName,nType,zType,zGiven);` |
|         - | 2632 | `			}` |
|   4057686 | 2633 | `		}` |
|         - | 2634 | ``		/* A NaN reaching a parameter php declares `string`: php's ZPP coerces it`` |
|         - | 2635 | ``		 * (to "NAN") and warns `unexpected NAN value was coerced to string`, the`` |
|         - | 2636 | `		 * same 8.5 diagnostic the cast and the concatenation raise. The builtin` |
|         - | 2637 | `		 * bodies read their argument with ph7_value_to_string, which is the SILENT` |
|         - | 2638 | `		 * conversion by design (the engine builds keys and messages with it), so` |
|         - | 2639 | `		 * the diagnostic belongs here, where the DECLARED type says a coercion is` |
|         - | 2640 | `		 * what is about to happen. A union that also accepts a NUMBER is left` |
|         - | 2641 | `		 * alone -- php keeps the float there and coerces nothing -- but` |
|         - | 2642 | ``		 * `array\|string`, the spelling str_replace()'s subject carries, does`` |
|         - | 2643 | `		 * coerce and does warn. */` |
|   9734712 | 2644 | `		if( (pArg->iFlags & (MEMOBJ_REAL\|MEMOBJ_STRING)) == MEMOBJ_REAL` |
|   4871026 | 2645 | `		 && PH7_IS_NAN((double)pArg->rVal)` |
|      2449 | 2646 | `		 && (nMask & VMSIG_STRING)` |
|        80 | 2647 | `		 && !(nMask & VMSIG_FLOAT)` |
|         5 | 2648 | `		 && !(nMask & VMSIG_INT)` |
|         7 | 2649 | `		 && !(nMask & VMSIG_MIXED) ){` |
|         3 | 2650 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 2651 | `				"unexpected NAN value was coerced to string");` |
|         1 | 2652 | `		}` |
|         - | 2653 | `		/* A PATH parameter, once its type is settled: php's Z_PARAM_PATH refuses a` |
|         - | 2654 | `		 * NUL byte outright rather than letting the C API truncate at it. Raised` |
|         - | 2655 | `		 * after the type verdict because that is php's order — the coercion runs` |
|         - | 2656 | `		 * first, and only a value that could BE a path is asked whether it is a` |
|         - | 2657 | `		 * legal one. */` |
|   9734717 | 2658 | `		if( iArg < 31 && (nPathMask & (1u<<iArg)) != 0 ){` |
|    117491 | 2659 | `			if( (pArg->iFlags & MEMOBJ_OBJ) != 0 && PH7_ArgSatisfiesString(pArg) ){` |
|         - | 2660 | `				/* A Stringable object: php coerces it and checks the RESULT, so` |
|         - | 2661 | ``				 * `unlink($o)` with a __toString() returning a NUL-bearing name is`` |
|         - | 2662 | `				 * the same ValueError. Converting IN PLACE is what keeps the` |
|         - | 2663 | `				 * accessor running exactly ONCE — the builtin then receives the` |
|         - | 2664 | `				 * string it would have produced itself. The argument a builtin sees` |
|         - | 2665 | `				 * is its own copy on every dispatch route (a direct call, a spread,` |
|         - | 2666 | `				 * both call_user_func forwards), so the caller's object is not` |
|         - | 2667 | `				 * retyped; strict mode never gets here, because a Stringable does` |
|         - | 2668 | ``				 * not satisfy a `string` parameter there and the screen above has`` |
|         - | 2669 | `				 * already refused it. */` |
|         3 | 2670 | `				sxi32 rcConv = PH7_MemObjToStringUV(pArg);` |
|         3 | 2671 | `				if( rcConv != SXRET_OK ){` |
|       ! 0 | 2672 | `					return rcConv; /* __toString() threw: php propagates it too */` |
|         - | 2673 | `				}` |
|         1 | 2674 | `			}` |
|    117491 | 2675 | `			if( VmArgHasNulByte(pArg) ){` |
|       193 | 2676 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 2677 | `					"%z(): Argument #%d ($%.*s) must not contain any null bytes",` |
|        61 | 2678 | `					&pFunc->sName,iArg + 1,nName,zName);` |
|         - | 2679 | `			}` |
|     58574 | 2680 | `		}` |
|   4868603 | 2681 | `	}` |
|   6420231 | 2682 | `	return SXRET_OK;` |
|   3369480 | 2683 | `}` |
|         - | 2684 | `/*` |
|         - | 2685 | ` * Builtins whose accepted arity is NOT a contiguous range, so the signature` |
|         - | 2686 | ` * cannot express it and the central too-many-arguments check must stay out of` |
|         - | 2687 | ` * the way: rand()/mt_rand() take 0 OR 2 arguments (never 1), and php words the` |
|         - | 2688 | ` * violation "expects exactly 2 arguments, 3 given" from their own check rather` |
|         - | 2689 | ` * than the ZPP "at most". They validate themselves; leaving bHasMaxArg at 0` |
|         - | 2690 | ` * keeps their message php-faithful.` |
|         - | 2691 | ` */` |
|   7299914 | 2692 | `static int VmBuiltinSelfValidatesArity(const char *zName)` |
|         5 | 2693 | `{` |
|         - | 2694 | `	static const char *const azSelf[] = { "rand", "mt_rand" };` |
|         - | 2695 | `	sxu32 i;` |
|  21879674 | 2696 | `	for( i = 0 ; i < SX_ARRAYSIZE(azSelf) ; i++ ){` |
|  14593142 | 2697 | `		sxu32 nSelf = SyStrlen(azSelf[i]);` |
|  14593142 | 2698 | `		if( SyStrlen(zName) == nSelf && SyStrncmp(zName,azSelf[i],nSelf) == 0 ){` |
|     13387 | 2699 | `			return 1;` |
|         - | 2700 | `		}` |
|   7257825 | 2701 | `	}` |
|   7286537 | 2702 | `	return 0;` |
|   3633925 | 2703 | `}` |
|         - | 2704 | `/*` |
|         - | 2705 | ` * One parameter of a declared signature, for the named-argument binder below.` |
|         - | 2706 | ` */` |
|         - | 2707 | `typedef struct VmSigParam VmSigParam;` |
|         - | 2708 | `struct VmSigParam` |
|         - | 2709 | `{` |
|         - | 2710 | `	const char *zName; int nName;   /* without the '$' */` |
|         - | 2711 | `	const char *zDef;  int nDef;    /* default TEXT, or 0 when the parameter is required */` |
|         - | 2712 | `	int bVariadic;` |
|         - | 2713 | `};` |
|         - | 2714 | `/*` |
|         - | 2715 | ` * Split a signature into its parameters: the NAME each one binds by and the default` |
|         - | 2716 | ` * TEXT to fall back on. The scan is VmDeriveArityFromSig's, kept apart because that one` |
|         - | 2717 | `` * only counts; a quoted default (`string $separator = ','`) hides a comma, which is why`` |
|         - | 2718 | ` * both go through VmSigSkipQuoted.` |
|         - | 2719 | ` */` |
|    115710 | 2720 | `static int VmSigParams(const char *zSig,VmSigParam *aOut,int nMax)` |
|         5 | 2721 | `{` |
|    115715 | 2722 | `	const char *zCur = zSig;` |
|    115715 | 2723 | `	const char *zStart = zSig;` |
|    115715 | 2724 | `	int n = 0;` |
|   4602478 | 2725 | `	for(;;){` |
|   9604971 | 2726 | `		if( zCur[0] == '\'' \|\| zCur[0] == '"' ){` |
|       128 | 2727 | `			zCur = VmSigSkipQuoted(zCur);` |
|       128 | 2728 | `			if( zCur[0] != '\0' ){` |
|       128 | 2729 | `				zCur++;` |
|        62 | 2730 | `			}` |
|       128 | 2731 | `			continue;` |
|         - | 2732 | `		}` |
|   9604847 | 2733 | `		if( zCur[0] == '\0' \|\| zCur[0] == ',' ){` |
|    494886 | 2734 | `			const char *z = zStart;` |
|    494886 | 2735 | `			const char *zEnd = zCur;` |
|    494886 | 2736 | `			if( n < nMax ){` |
|    494886 | 2737 | `				VmSigParam *p = &aOut[n];` |
|    494886 | 2738 | `				const char *zEq = 0;` |
|    494886 | 2739 | `				const char *zDollar = 0;` |
|    494886 | 2740 | `				p->zName = 0; p->nName = 0; p->zDef = 0; p->nDef = 0; p->bVariadic = 0;` |
|   9605123 | 2741 | `				for( ; z < zEnd ; z++ ){` |
|   9110242 | 2742 | `					if( z[0] == '$' && zDollar == 0 ){` |
|    494886 | 2743 | `						zDollar = z + 1;` |
|   8862273 | 2744 | `					}else if( z[0] == '=' && zEq == 0 ){` |
|    188483 | 2745 | `						zEq = z + 1;` |
|   8520984 | 2746 | `					}else if( z[0] == '.' && z + 2 < zEnd && z[1] == '.' && z[2] == '.' ){` |
|       175 | 2747 | `						p->bVariadic = 1;` |
|        85 | 2748 | `					}` |
|   4544904 | 2749 | `				}` |
|    494886 | 2750 | `				if( zDollar ){` |
|    494886 | 2751 | `					const char *zStop = zEq ? zEq - 1 : zEnd;` |
|    494886 | 2752 | `					const char *zN = zDollar;` |
|   3631109 | 2753 | `					while( zN < zStop && zN[0] != ' ' && zN[0] != '=' ){` |
|   3136228 | 2754 | `						zN++;` |
|         5 | 2755 | `					}` |
|    494886 | 2756 | `					p->zName = zDollar;` |
|    494886 | 2757 | `					p->nName = (int)(zN - zDollar);` |
|    246912 | 2758 | `				}` |
|    494886 | 2759 | `				if( zEq ){` |
|    376961 | 2760 | `					while( zEq < zEnd && zEq[0] == ' ' ){` |
|    188483 | 2761 | `						zEq++;` |
|         5 | 2762 | `					}` |
|    188483 | 2763 | `					p->zDef = zEq;` |
|    188483 | 2764 | `					p->nDef = (int)(zEnd - zEq);` |
|    188483 | 2765 | `					while( p->nDef > 0 && p->zDef[p->nDef-1] == ' ' ){` |
|       ! 0 | 2766 | `						p->nDef--;` |
|       ! 0 | 2767 | `					}` |
|     94101 | 2768 | `				}` |
|    494886 | 2769 | `				if( p->nName > 0 ){` |
|    494886 | 2770 | `					n++;` |
|    246912 | 2771 | `				}` |
|    246912 | 2772 | `			}` |
|    494886 | 2773 | `			if( zCur[0] == '\0' ){` |
|    115715 | 2774 | `				break;` |
|         - | 2775 | `			}` |
|    379176 | 2776 | `			zCur++;` |
|    379176 | 2777 | `			zStart = zCur;` |
|    379176 | 2778 | `			continue;` |
|         - | 2779 | `		}` |
|   9109966 | 2780 | `		zCur++;` |
|         5 | 2781 | `	}` |
|    115715 | 2782 | `	return n;` |
|         5 | 2783 | `}` |
|         - | 2784 | `/*` |
|         - | 2785 | ` * Materialize a signature default's TEXT into pOut, for a parameter a NAMED call` |
|         - | 2786 | ` * skipped.` |
|         - | 2787 | ` *` |
|         - | 2788 | ` * The reduction itself is Reflection's (PH7_VmSigDefaultToValue): a default is` |
|         - | 2789 | ` * read off the SAME signature string getDefaultValue() reads, so it has to mean` |
|         - | 2790 | ` * the same thing at both doors, and this one used to carry a smaller reader of` |
|         - | 2791 | ` * its own -- see the note over PH7_VmSigDefaultToValue for what the two` |
|         - | 2792 | `` * disagreed about. Two cases stay here: `[]`, whose value is a hashmap rather`` |
|         - | 2793 | `` * than a scalar, and the `= ?` marker (~50 rows the table cannot state, recorded),`` |
|         - | 2794 | ` * which answers 0 so the caller reports the parameter as not passed rather than` |
|         - | 2795 | ` * inventing a value.` |
|         - | 2796 | ` */` |
|        34 | 2797 | `static int VmSigDefaultValue(ph7_context *pCtx,const VmSigParam *pParam,ph7_value *pOut)` |
|         3 | 2798 | `{` |
|        37 | 2799 | `	const char *z = pParam->zDef;` |
|        37 | 2800 | `	int n = pParam->nDef;` |
|        37 | 2801 | `	if( z == 0 \|\| n < 1 \|\| (n == 1 && z[0] == '?') ){` |
|         3 | 2802 | `		return 0;` |
|         - | 2803 | `	}` |
|        35 | 2804 | `	if( n == 2 && z[0] == '[' && z[1] == ']' ){` |
|         3 | 2805 | `		ph7_hashmap *pMap = PH7_NewHashmap(pCtx->pVm,0,0);` |
|         3 | 2806 | `		if( pMap == 0 ){` |
|       ! 0 | 2807 | `			return 0;` |
|         - | 2808 | `		}` |
|         3 | 2809 | `		PH7_MemObjRelease(pOut);` |
|         3 | 2810 | `		pOut->x.pOther = pMap;` |
|         3 | 2811 | `		MemObjSetType(pOut,MEMOBJ_HASHMAP);` |
|         3 | 2812 | `		return 1;` |
|         - | 2813 | `	}` |
|        33 | 2814 | `	return PH7_VmSigDefaultToValue(pCtx,z,n,pOut);` |
|        20 | 2815 | `}` |
|         - | 2816 | `/*` |
|         - | 2817 | ` * Bind a call's NAMED arguments to the callee's declared parameter POSITIONS.` |
|         - | 2818 | ` *` |
|         - | 2819 | ` * A compiled function does this from its parameter records (VmResolveNamedArgs); a host` |
|         - | 2820 | ` * function and a native method have none, so every named argument was simply passed in the` |
|         - | 2821 | `` * order it was WRITTEN. `str_pad(length: 5, string: "x")` reached the builtin as`` |
|         - | 2822 | ` * ("x" at #2, 5 at #1) and reported a TypeError, and — worse, because it is silent —` |
|         - | 2823 | `` * `str_pad("x", 5, pad_type: STR_PAD_LEFT)` bound the flag as $pad_string and answered`` |
|         - | 2824 | ` * "x0000" where php answers "    x". Both spellings are php 8.0 syntax, and the whole` |
|         - | 2825 | ` * ~650-builtin surface plus every native method was affected.` |
|         - | 2826 | ` *` |
|         - | 2827 | ` * The declared signature is the source of names, defaults and positions — the same string` |
|         - | 2828 | ` * Reflection prints. Rewrites *pnArg / apArg in place (the caller's argument vector is` |
|         - | 2829 | ` * scratch it owns) and answers SXRET_OK, or throws php's Error and returns its status.` |
|         - | 2830 | ` * Callees with a VARIADIC tail are left alone: php collects extra named arguments into it` |
|         - | 2831 | ` * by NAME, which the positional vector here cannot express.` |
|         - | 2832 | ` */` |
|       134 | 2833 | `PH7_PRIVATE sxi32 PH7_VmBindNamedArgsToSig(` |
|         - | 2834 | `	ph7_context *pCtx,      /* Call context (for the throws) */` |
|         - | 2835 | `	ph7_user_func *pFunc,   /* Callee: its zSig names the parameters */` |
|         - | 2836 | `	VmCallArgMap *pMap,     /* Call-site map; its aNames[] are per ACTUAL slot */` |
|         - | 2837 | `	int *pnArg,             /* IN/OUT: argument count */` |
|         - | 2838 | `	ph7_value **apArg       /* IN/OUT: argument vector */` |
|         - | 2839 | `	)` |
|         5 | 2840 | `{` |
|         - | 2841 | `	/* php's own stubs top out well under this; a signature with more parameters simply` |
|         - | 2842 | `	 * keeps the positional binding it had. */` |
|         - | 2843 | `#define VM_SIG_MAX_PARAM 32` |
|         - | 2844 | `	VmSigParam aParam[VM_SIG_MAX_PARAM];` |
|         - | 2845 | `	ph7_value *apBound[VM_SIG_MAX_PARAM];` |
|         - | 2846 | `	int nParam,nArg,i,nLast;` |
|       139 | 2847 | `	if( pFunc == 0 \|\| pFunc->zSig == 0 \|\| pMap == 0 \|\| pMap->bHasNamed == 0 ){` |
|        13 | 2848 | `		return SXRET_OK;` |
|         - | 2849 | `	}` |
|       127 | 2850 | `	nArg = *pnArg;` |
|       127 | 2851 | `	if( nArg < 1 \|\| nArg > VM_SIG_MAX_PARAM ){` |
|       ! 0 | 2852 | `		return SXRET_OK;` |
|         - | 2853 | `	}` |
|       127 | 2854 | `	nParam = VmSigParams(pFunc->zSig,aParam,VM_SIG_MAX_PARAM);` |
|       127 | 2855 | `	if( nParam < 1 \|\| aParam[nParam-1].bVariadic ){` |
|        24 | 2856 | `		return SXRET_OK;` |
|         - | 2857 | `	}` |
|       414 | 2858 | `	for( i = 0 ; i < nParam ; ++i ){` |
|       314 | 2859 | `		apBound[i] = 0;` |
|       159 | 2860 | `	}` |
|       104 | 2861 | `	nLast = -1;` |
|       310 | 2862 | `	for( i = 0 ; i < nArg ; ++i ){` |
|       216 | 2863 | `		int p = i;` |
|       297 | 2864 | `		if( i < (int)pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|       172 | 2865 | `			SyString *pName = &pMap->aNames[i];` |
|       352 | 2866 | `			for( p = 0 ; p < nParam ; ++p ){` |
|       344 | 2867 | `				if( (int)pName->nByte == aParam[p].nName` |
|       278 | 2868 | `				 && SyMemcmp(pName->zString,aParam[p].zName,pName->nByte) == 0 ){` |
|       168 | 2869 | `					break;` |
|         - | 2870 | `				}` |
|        93 | 2871 | `			}` |
|       172 | 2872 | `			if( p >= nParam ){` |
|         7 | 2873 | `				return PH7_VmThrowException(pCtx,"Error",` |
|         2 | 2874 | `					"Unknown named parameter $%z",pName);` |
|         - | 2875 | `			}` |
|       168 | 2876 | `			if( apBound[p] ){` |
|         4 | 2877 | `				return PH7_VmThrowException(pCtx,"Error",` |
|         1 | 2878 | `					"Named parameter $%z overwrites previous argument",pName);` |
|         4 | 2879 | `			}` |
|       128 | 2880 | `		}else if( p >= nParam ){` |
|       ! 0 | 2881 | `			return SXRET_OK; /* more positional arguments than the signature knows */` |
|         - | 2882 | `		}` |
|       210 | 2883 | `		apBound[p] = apArg[i];` |
|       210 | 2884 | `		if( p > nLast ){` |
|       188 | 2885 | `			nLast = p;` |
|        92 | 2886 | `		}` |
|       107 | 2887 | `	}` |
|       328 | 2888 | `	for( i = 0 ; i <= nLast ; ++i ){` |
|       236 | 2889 | `		if( apBound[i] == 0 ){` |
|        37 | 2890 | `			ph7_value *pDef = ph7_context_new_scalar(pCtx);` |
|        37 | 2891 | `			if( pDef == 0 \|\| !VmSigDefaultValue(pCtx,&aParam[i],pDef) ){` |
|         - | 2892 | `				SyString sName;` |
|         3 | 2893 | `				SyStringInitFromBuf(&sName,aParam[i].zName,(sxu32)aParam[i].nName);` |
|         4 | 2894 | `				return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|         1 | 2895 | `					"%z(): Argument #%d ($%z) not passed",&pFunc->sName,i + 1,&sName);` |
|         - | 2896 | `			}` |
|        35 | 2897 | `			apBound[i] = pDef;` |
|        16 | 2898 | `		}` |
|       119 | 2899 | `	}` |
|       324 | 2900 | `	for( i = 0 ; i <= nLast ; ++i ){` |
|       232 | 2901 | `		apArg[i] = apBound[i];` |
|       118 | 2902 | `	}` |
|        96 | 2903 | `	*pnArg = nLast + 1;` |
|        96 | 2904 | `	return SXRET_OK;` |
|        72 | 2905 | `}` |
|         - | 2906 | `/*` |
|         - | 2907 | ` * Name the Nth (0-based) parameter of a declared signature, without the '$'.` |
|         - | 2908 | ` *` |
|         - | 2909 | ` * The signature string is the only place a host function's parameter names live, and` |
|         - | 2910 | `` * php puts them in diagnostics — `sort(): Argument #1 ($array) …`. Answers 0 when the`` |
|         - | 2911 | ` * signature has no such parameter (or none with a name).` |
|         - | 2912 | ` */` |
|        12 | 2913 | `PH7_PRIVATE int PH7_VmSigParamName(const char *zSig,int nPos,SyString *pOut)` |
|         2 | 2914 | `{` |
|         - | 2915 | `	VmSigParam aParam[VM_SIG_MAX_PARAM];` |
|         - | 2916 | `	int nParam;` |
|        14 | 2917 | `	if( zSig == 0 \|\| nPos < 0 \|\| nPos >= VM_SIG_MAX_PARAM ){` |
|       ! 0 | 2918 | `		return 0;` |
|         - | 2919 | `	}` |
|        14 | 2920 | `	nParam = VmSigParams(zSig,aParam,VM_SIG_MAX_PARAM);` |
|        14 | 2921 | `	if( nPos >= nParam \|\| aParam[nPos].nName < 1 ){` |
|       ! 0 | 2922 | `		return 0;` |
|         - | 2923 | `	}` |
|        14 | 2924 | `	if( aParam[nPos].bVariadic ){` |
|         - | 2925 | ``		/* php's get_function_arg_name() answers NULL past `num_args`, which`` |
|         - | 2926 | `		 * counts the non-variadic parameters alone -- so an actual absorbed by` |
|         - | 2927 | ``		 * a `...` tail is named in no diagnostic (`sscanf(): Argument #3 must`` |
|         - | 2928 | ``		 * be passed by reference, value given`, with no ` ($vars)`). */`` |
|       ! 0 | 2929 | `		return 0;` |
|         - | 2930 | `	}` |
|        14 | 2931 | `	SyStringInitFromBuf(pOut,aParam[nPos].zName,(sxu32)aParam[nPos].nName);` |
|        14 | 2932 | `	return 1;` |
|         8 | 2933 | `}` |
|         - | 2934 | `/*` |
|         - | 2935 | `` * A `&` in a builtin's signature is not always php's ZEND_SEND_ARG_BY_REF.`` |
|         - | 2936 | ` *` |
|         - | 2937 | ` * php has a second mode, ZEND_SEND_PREFER_REF: bind by reference when the argument IS a` |
|         - | 2938 | ` * variable, and otherwise take it by value without a word. Reflection prints those` |
|         - | 2939 | ` * parameters as by-reference like any other and PHL's signature string cannot say which` |
|         - | 2940 | `` * mode a `&` means, so the two are told apart here. Probed value-for-value against php`` |
|         - | 2941 | `` * 8.5 over every `&` row PHL declares (41 of them): all but extract() refuse a`` |
|         - | 2942 | `` * non-variable, and extract() answers `int(1)` for `extract(['q' => 1])`.`` |
|         - | 2943 | ` *` |
|         - | 2944 | ` * array_multisort() is listed with it because it is php's other prefer-ref builtin and` |
|         - | 2945 | ` * PHL will need this the day it gains one (it is a MISSING builtin today).` |
|         - | 2946 | ` */` |
|    115776 | 2947 | `PH7_PRIVATE int VmBuiltinPrefersRef(SyString *pName)` |
|         5 | 2948 | `{` |
|         - | 2949 | `	static const char *const azPreferRef[] = { "extract", "array_multisort" };` |
|         - | 2950 | `	sxu32 i;` |
|    347089 | 2951 | `	for( i = 0 ; i < SX_ARRAYSIZE(azPreferRef) ; ++i ){` |
|    231453 | 2952 | `		sxu32 nByte = SyStrlen(azPreferRef[i]);` |
|    231448 | 2953 | `		if( pName->nByte == nByte` |
|    115636 | 2954 | `		 && SyMemcmp(pName->zString,azPreferRef[i],nByte) == 0 ){` |
|       144 | 2955 | `			return 1;` |
|         - | 2956 | `		}` |
|    115383 | 2957 | `	}` |
|    115641 | 2958 | `	return 0;` |
|     57755 | 2959 | `}` |
|         - | 2960 | `/*` |
|         - | 2961 | ` * php refuses a by-reference argument at the CALL, before the callee's ZPP runs, and it` |
|         - | 2962 | ``  * decides from the argument's SHAPE, not from its value: `sort([3,1])`, `usort('x',$cb)` `` |
|         - | 2963 | `` * and `preg_match($p,$s,'lit')` are all`` |
|         - | 2964 | `` * `Error: sort(): Argument #1 ($array) could not be passed by reference`.`` |
|         - | 2965 | ` *` |
|         - | 2966 | ` * The call site's compile-time shape mask (VmCallArgMap.nNonLvalMask) is what says so.` |
|         - | 2967 | ` * Only five builtins raised anything before this, from their own bodies, on the runtime` |
|         - | 2968 | `` * `nIdx == SXU32_HIGH` signal — which cannot tell a literal from the result of a call, a`` |
|         - | 2969 | `` * shape php ACCEPTS with a notice. The thirty other `&` rows answered `true`/`false`/an`` |
|         - | 2970 | ` * int: the same answers they give for work they really did.` |
|         - | 2971 | ` *` |
|         - | 2972 | ` * Skipped when the call site has no shape mask (a spread, an indirect dispatch through` |
|         - | 2973 | ` * call_user_func, an engine-synthesized call) or uses named arguments (which rebind` |
|         - | 2974 | ` * positions the mask is indexed by). The by-ref positions come from the same declared` |
|         - | 2975 | ` * signature everything else here reads.` |
|         - | 2976 | ` */` |
|   6737748 | 2977 | `PH7_PRIVATE sxi32 PH7_VmScreenByRefArgShapes(` |
|         - | 2978 | `	ph7_context *pCtx,     /* Call context (for the throw) */` |
|         - | 2979 | `	ph7_user_func *pFunc,  /* Callee: its zSig names and marks the parameters */` |
|         - | 2980 | `	VmCallArgMap *pMap,    /* Call-site map, or 0 */` |
|         - | 2981 | `	int nGiven,            /* Argument count */` |
|         - | 2982 | `	ph7_value **apArg      /* Arguments */` |
|         - | 2983 | `	)` |
|         5 | 2984 | `{` |
|         - | 2985 | `	VmSigParam aParam[VM_SIG_MAX_PARAM];` |
|         - | 2986 | `	int nParam,n;` |
|         - | 2987 | `	/* The by-ref mask first: it is 0 for all but 41 of the ~650 host functions, so` |
|         - | 2988 | `	 * every other call leaves through one test. */` |
|   6737753 | 2989 | `	if( pFunc == 0 \|\| pFunc->nByRefMask == 0 \|\| pFunc->zSig == 0 \|\| nGiven < 1 ){` |
|   6619933 | 2990 | `		return SXRET_OK;` |
|         - | 2991 | `	}` |
|    117825 | 2992 | `	if( pMap == 0 \|\| !pMap->bArgShapes \|\| pMap->bHasNamed ){` |
|        91 | 2993 | `		return SXRET_OK;` |
|         - | 2994 | `	}` |
|    117737 | 2995 | `	if( (pMap->nNonLvalMask \| pMap->nTempCallMask) == 0 ){` |
|      2037 | 2996 | `		return SXRET_OK;` |
|         - | 2997 | `	}` |
|    115705 | 2998 | `	if( VmBuiltinPrefersRef(&pFunc->sName) ){` |
|       128 | 2999 | `		return SXRET_OK;` |
|         - | 3000 | `	}` |
|    115581 | 3001 | `	nParam = VmSigParams(pFunc->zSig,aParam,VM_SIG_MAX_PARAM);` |
|    459905 | 3002 | `	for( n = 0 ; n < nGiven && n < 31 ; ++n ){` |
|    344383 | 3003 | `		if( (pFunc->nByRefMask & (1u << n)) == 0 ){` |
|    306546 | 3004 | `			continue;` |
|         - | 3005 | `		}` |
|     37842 | 3006 | `		if( (pMap->nNonLvalMask & (1u << n)) == 0 ){` |
|         - | 3007 | `			/* Not a refusal — but a CALL result in this position is php's notice,` |
|         - | 3008 | `			 * and then the builtin operates on the temporary. */` |
|     37788 | 3009 | `			PH7_VmArgTempCallNotice(pCtx->pVm,pMap,(sxu32)n,apArg[n]);` |
|     37788 | 3010 | `			continue;` |
|         - | 3011 | `		}` |
|         - | 3012 | `		/* php names the parameter only when the position is a DECLARED one:` |
|         - | 3013 | ``		 * get_function_arg_name() answers NULL past `num_args`, which counts`` |
|         - | 3014 | `` 		 * the non-variadic parameters alone. So an actual absorbed by a `&...` `` |
|         - | 3015 | ``		 * tail (sscanf's `&...$vars`) is refused without a name. */`` |
|        59 | 3016 | `		if( n < nParam && aParam[n].nName > 0 && !aParam[n].bVariadic ){` |
|        83 | 3017 | `			return PH7_VmThrowException(pCtx,"Error",` |
|         - | 3018 | `				"%z(): Argument #%d ($%.*s) could not be passed by reference",` |
|        26 | 3019 | `				&pFunc->sName,n + 1,aParam[n].nName,aParam[n].zName);` |
|         - | 3020 | `		}` |
|         4 | 3021 | `		return PH7_VmThrowException(pCtx,"Error",` |
|         - | 3022 | `			"%z(): Argument #%d could not be passed by reference",` |
|         1 | 3023 | `			&pFunc->sName,n + 1);` |
|       ! 0 | 3024 | `	}` |
|    115527 | 3025 | `	return SXRET_OK;` |
|   3369961 | 3026 | `}` |
|         - | 3027 | `/*` |
|         - | 3028 | ` * D1: derive a by-reference position bitmask from a php-style signature string.` |
|         - | 3029 | `` * Bit N is set when positional parameter N is declared by-reference (a `&` appears`` |
|         - | 3030 | `` * anywhere in that comma-separated parameter, e.g. `&$matches`, `&...$vars`). Only`` |
|         - | 3031 | ` * the first 31 positions are representable; a by-ref parameter past that is rare` |
|         - | 3032 | ` * for a builtin and simply not tracked here. The scan mirrors VmDeriveArityFromSig.` |
|         - | 3033 | ` */` |
|  17959301 | 3034 | `PH7_PRIVATE sxu32 VmDeriveByRefMaskFromSig(const char *zSig)` |
|         5 | 3035 | `{` |
|  17959306 | 3036 | `	sxu32 mask = 0;` |
|  17959306 | 3037 | `	int n = 0;       /* current parameter index */` |
|  17959306 | 3038 | `	int bSeen = 0;   /* current parameter has non-space content */` |
|  17959306 | 3039 | ``	int bRef = 0;    /* current parameter carries a by-ref `&` */`` |
|  17959306 | 3040 | ``	int bVar = 0;    /* current parameter is a `...` variadic */`` |
|  17959306 | 3041 | `	int bTailRef = 0;/* the LAST parameter was a by-ref variadic */` |
|  17959306 | 3042 | `	const char *zCur = zSig;` |
| 207799107 | 3043 | `	for(;;){` |
| 428508838 | 3044 | `		if( zCur[0] == '\'' \|\| zCur[0] == '"' ){` |
|    725869 | 3045 | `			bSeen = 1;` |
|    725869 | 3046 | `			zCur = VmSigSkipQuoted(zCur);` |
|    725869 | 3047 | `			if( zCur[0] != '\0' ){` |
|    725869 | 3048 | `				zCur++;` |
|    362382 | 3049 | `			}` |
|    725869 | 3050 | `			continue;` |
|         - | 3051 | `		}` |
| 427782974 | 3052 | `		if( zCur[0] == '\0' \|\| zCur[0] == ',' ){` |
|  28924376 | 3053 | `			if( bSeen ){` |
|  22536307 | 3054 | `				if( bRef && n < 31 ){` |
|    815276 | 3055 | `					mask \|= (1u << n);` |
|    403630 | 3056 | `				}` |
|  22536307 | 3057 | `				bTailRef = (bRef && bVar);` |
|  22536307 | 3058 | `				n++;` |
|  11233972 | 3059 | `			}` |
|  28924376 | 3060 | `			if( zCur[0] == '\0' ){` |
|  17959306 | 3061 | `				break;` |
|         - | 3062 | `			}` |
|  10965075 | 3063 | `			bSeen = bRef = bVar = 0;` |
|  10965075 | 3064 | `			zCur++;` |
|  10965075 | 3065 | `			continue;` |
|         - | 3066 | `		}` |
| 398858603 | 3067 | `		if( zCur[0] != ' ' ){` |
| 351270194 | 3068 | `			bSeen = 1;` |
| 175130764 | 3069 | `		}` |
| 398858603 | 3070 | `		if( zCur[0] == '&' ){` |
|    815276 | 3071 | `			bRef = 1;` |
|    403630 | 3072 | `		}` |
| 398858603 | 3073 | `		if( zCur[0] == '.' && zCur[1] == '.' && zCur[2] == '.' ){` |
|         - | 3074 | ``			/* A `...` tail, not a numeric default's decimal point. */`` |
|    444463 | 3075 | `			bVar = 1;` |
|    221888 | 3076 | `		}` |
| 398858603 | 3077 | `		zCur++;` |
|         5 | 3078 | `	}` |
|  17959306 | 3079 | `	if( bTailRef && n > 0 && n <= 31 ){` |
|         - | 3080 | ``		/* A by-ref `&...` tail absorbs every later actual (array_multisort's`` |
|         - | 3081 | ``		 * `&...$rest`): without this, the deferred-argument resolver read the`` |
|         - | 3082 | ``		 * tail positions as by-VALUE and warned `Undefined variable` on an`` |
|         - | 3083 | `		 * undefined actual php binds silently. */` |
|     28003 | 3084 | `		mask \|= ~((1u << (n - 1)) - 1u);` |
|     13977 | 3085 | `	}` |
|  17959306 | 3086 | `	return mask;` |
|         5 | 3087 | `}` |
|      6691 | 3088 | `PH7_PRIVATE void VmSetBuiltinSignatures(ph7_vm *pVm)` |
|         5 | 3089 | `{` |
|         - | 3090 | `	sxu32 n;` |
|   7346723 | 3091 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinSig) ; n++ ){` |
|  11004012 | 3092 | `		SyHashEntry *pEntry = SyHashGet(&pVm->hHostFunction,` |
|   7340027 | 3093 | `			(const void *)aBuiltinSig[n].zName,(sxu32)SyStrlen(aBuiltinSig[n].zName));` |
|   7340032 | 3094 | `		if( pEntry ){` |
|   7299919 | 3095 | `			ph7_user_func *pFunc = (ph7_user_func *)pEntry->pUserData;` |
|   7299919 | 3096 | `			sxi16 nMin = 0, nMax = 0;` |
|   7299919 | 3097 | `			sxu8 bAtLeast = 0, bHasMax = 0;` |
|   7299919 | 3098 | `			pFunc->zSig = aBuiltinSig[n].zSig;` |
|   7299919 | 3099 | `			pFunc->zRet = aBuiltinSig[n].zRet[0] ? aBuiltinSig[n].zRet : 0;` |
|   7299919 | 3100 | `			pFunc->nByRefMask = VmDeriveByRefMaskFromSig(pFunc->zSig);` |
|   7299919 | 3101 | `			VmDeriveArityFromSig(pFunc->zSig,&nMin,&bAtLeast,&nMax,&bHasMax);` |
|         - | 3102 | `			/* The MAXIMUM always comes from the signature: the curated override` |
|         - | 3103 | `			 * table speaks only to the minimum (and its wording). */` |
|   7299919 | 3104 | `			pFunc->nMaxArg = nMax;` |
|   7299919 | 3105 | `			pFunc->bHasMaxArg = (sxu8)(VmBuiltinSelfValidatesArity(aBuiltinSig[n].zName) ? 0 : bHasMax);` |
|   7299919 | 3106 | `			if( pFunc->nMinArg < 1 ){` |
|         - | 3107 | `				/* VmSetBuiltinArity() ran first: a non-zero minimum here means the` |
|         - | 3108 | `				 * curated override already spoke for this builtin, so leave it. */` |
|   5279237 | 3109 | `				pFunc->nMinArg = nMin;` |
|   5279237 | 3110 | `				pFunc->bAtLeast = bAtLeast;` |
|   2625240 | 3111 | `			}` |
|   3633920 | 3112 | `		}` |
|   3663985 | 3113 | `	}` |
|      6696 | 3114 | `}` |
|         - | 3115 | `/*` |
|         - | 3116 | ` * Signature lookup by function name, for the reflection layer: embedded-PHP` |
|         - | 3117 | ` * builtins (max/min/Exception methods...) are ph7_vm_func instances, not` |
|         - | 3118 | ` * host functions, so they miss the VmSetBuiltinSignatures stamping and pull` |
|         - | 3119 | ` * their row on demand here. Linear scan — reflection-path only.` |
|         - | 3120 | ` */` |
|       ! 0 | 3121 | `PH7_PRIVATE const char * PH7_VmBuiltinSigLookup(const char *zName,sxu32 nLen,const char **pzRet)` |
|       ! 0 | 3122 | `{` |
|         - | 3123 | `	sxu32 n;` |
|       ! 0 | 3124 | `	if( pzRet ){` |
|       ! 0 | 3125 | `		*pzRet = 0;` |
|       ! 0 | 3126 | `	}` |
|       ! 0 | 3127 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinSig) ; n++ ){` |
|       ! 0 | 3128 | `		if( SyStrlen(aBuiltinSig[n].zName) == nLen` |
|       ! 0 | 3129 | `		 && SyMemcmp(aBuiltinSig[n].zName,zName,nLen) == 0 ){` |
|       ! 0 | 3130 | `			if( pzRet && aBuiltinSig[n].zRet[0] ){` |
|       ! 0 | 3131 | `				*pzRet = aBuiltinSig[n].zRet;` |
|       ! 0 | 3132 | `			}` |
|       ! 0 | 3133 | `			return aBuiltinSig[n].zSig;` |
|         - | 3134 | `		}` |
|       ! 0 | 3135 | `	}` |
|       ! 0 | 3136 | `	return 0;` |
|       ! 0 | 3137 | `}` |
|         - | 3138 | `/*` |
|         - | 3139 | ` * Write a value back to the caller's variable through a builtin argument's` |
|         - | 3140 | ` * stack-slot nIdx — the shared write-back for builtin by-reference` |
|         - | 3141 | ` * out-parameters (preg_match $matches, preg_replace &$count, similar_text` |
|         - | 3142 | ` * &$percent, ...).` |
|         - | 3143 | ` *` |
|         - | 3144 | ` * For a positional out-param argument the call compiler auto-vivifies known` |
|         - | 3145 | ` * by-reference out-params (see GenStateByRefBuiltinMask in compile.c), so a` |
|         - | 3146 | ` * bare undefined variable, an array subscript, and a declared/untyped` |
|         - | 3147 | ` * property all arrive with a real nIdx and are written back here, matching` |
|         - | 3148 | ` * PHP's reference semantics.` |
|         - | 3149 | ` *` |
|         - | 3150 | ` * nIdx stays SXU32_HIGH and the write-back to the caller is skipped (the` |
|         - | 3151 | ` * value still lands in the local stack slot) when the argument cannot expose` |
|         - | 3152 | ` * a stable memobj slot: a literal, a function-call result, a subscript of a` |
|         - | 3153 | ` * non-lvalue parent (foo()['k']), or any variable in a call that also uses` |
|         - | 3154 | ` * named or spread arguments (compile-time positions no longer map to the` |
|         - | 3155 | ` * runtime arg slots, so the compiler conservatively does not vivify). An` |
|         - | 3156 | ` * uninitialized typed property is also not wired (it throws before the` |
|         - | 3157 | ` * write) -- see the recorded deferrals.` |
|         - | 3158 | ` */` |
|     35266 | 3159 | `PH7_PRIVATE void PH7_VmStoreArgByRef(ph7_vm *pVm,ph7_value *pArg,ph7_value *pNewVal)` |
|         5 | 3160 | `{` |
|     35271 | 3161 | `	if( pArg->nIdx != SXU32_HIGH ){` |
|     35177 | 3162 | `		ph7_value *pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pArg->nIdx);` |
|     35177 | 3163 | `		if( pObj ){` |
|     35177 | 3164 | `			PH7_MemObjStore(pNewVal,pObj);` |
|     17586 | 3165 | `		}` |
|     17586 | 3166 | `	}` |
|     35271 | 3167 | `	PH7_MemObjStore(pNewVal,pArg);` |
|     35271 | 3168 | `}` |
|         - | 3169 | `/*` |
|         - | 3170 | ` * Raise an E_WARNING whose text is used VERBATIM.` |
|         - | 3171 | ` * ph7_context_throw_error_format() prepends "func(): " to whatever it is given, but php's` |
|         - | 3172 | ` * IO warnings put the offending path inside those parens -- "file_get_contents(/nope):` |
|         - | 3173 | ` * Failed to open stream: ..." -- so a caller that needs php's exact shape must build the` |
|         - | 3174 | ` * whole line itself and come through here.` |
|         - | 3175 | ` */` |
|         - | 3176 | `/*` |
|         - | 3177 | ` * Validate a callback argument and throw php's TypeError when it cannot be called.` |
|         - | 3178 | ` *` |
|         - | 3179 | ` * The shared dispatcher (PH7_VmCallUserFunctionWithMap) answers SXRET_OK with a NULL` |
|         - | 3180 | ` * result for an unresolvable callable, so every caller that did not check first failed` |
|         - | 3181 | ` * SILENTLY: call_user_func() returned NULL and usort() left the array sorted by nothing` |
|         - | 3182 | ` * at all. php rejects the ARGUMENT up front, naming exactly why.` |
|         - | 3183 | ` */` |
|    402266 | 3184 | `PH7_PRIVATE sxi32 PH7_CheckCallbackArg(` |
|         - | 3185 | `	ph7_context *pCtx,   /* Calling context (names the function in the message) */` |
|         - | 3186 | `	ph7_value *pCb,      /* The callback argument */` |
|         - | 3187 | `	int iArg,            /* Its 1-based position */` |
|         - | 3188 | `	const char *zParam,  /* Its php parameter name ("callback"), or 0 for the variadic` |
|         - | 3189 | `	                      * comparators php names by position only (array_udiff …) */` |
|         - | 3190 | `	int bNullable        /* TRUE when php's text says "or null" */` |
|         - | 3191 | `	)` |
|         5 | 3192 | `{` |
|         - | 3193 | `	char zReason[256];` |
|    402271 | 3194 | `	const char *zWhy = PH7_VmCallableReason(pCtx->pVm,pCb,zReason,sizeof(zReason));` |
|    402271 | 3195 | `	if( zWhy == 0 ){` |
|    402021 | 3196 | `		return PH7_OK;` |
|         - | 3197 | `	}` |
|       255 | 3198 | `	if( zParam ){` |
|       299 | 3199 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3200 | `			"%s(): Argument #%d ($%s) must be a valid callback%s, %s",` |
|        98 | 3201 | `			ph7_function_name(pCtx),iArg,zParam,bNullable ? " or null" : "",zWhy);` |
|         - | 3202 | `	}` |
|        86 | 3203 | `	return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3204 | `		"%s(): Argument #%d must be a valid callback%s, %s",` |
|        27 | 3205 | `		ph7_function_name(pCtx),iArg,bNullable ? " or null" : "",zWhy);` |
|    201127 | 3206 | `}` |
|     32145 | 3207 | `PH7_PRIVATE void PH7_VmThrowWarningFmt(ph7_vm *pVm,const char *zFmt,...)` |
|         5 | 3208 | `{` |
|         - | 3209 | `	va_list ap;` |
|     32150 | 3210 | `	va_start(ap,zFmt);` |
|     32150 | 3211 | `	PH7_VmThrowErrorAp(pVm,0,PH7_CTX_WARNING,zFmt,ap);` |
|     32150 | 3212 | `	va_end(ap);` |
|     32150 | 3213 | `}` |
|         - | 3214 | `/*` |
|         - | 3215 | ` * Emit a formatted E_USER_WARNING with no function-name prefix: php reports` |
|         - | 3216 | ` * #[\NoDiscard] as a USER warning (512) for the same reason it reports` |
|         - | 3217 | ` * #[\Deprecated] as a USER deprecation — the attribute is userland-authored,` |
|         - | 3218 | ` * and a set_error_handler sees the number.` |
|         - | 3219 | ` */` |
|        56 | 3220 | `static void VmThrowUserWarningFmt(ph7_vm *pVm,const char *zFmt,...)` |
|         1 | 3221 | `{` |
|         - | 3222 | `	va_list ap;` |
|        57 | 3223 | `	va_start(ap,zFmt);` |
|        57 | 3224 | `	PH7_VmThrowErrorAp(pVm,0,E_USER_WARNING,zFmt,ap);` |
|        57 | 3225 | `	va_end(ap);` |
|        57 | 3226 | `}` |
|         - | 3227 | `/*` |
|         - | 3228 | ` * Emit a formatted E_USER_DEPRECATED diagnostic with no function-name prefix:` |
|         - | 3229 | ` * php reports #[\Deprecated] as USER-deprecated (16384, it is userland-authored),` |
|         - | 3230 | ` * not the engine's E_DEPRECATED (8192) — handler-visible errno matters.` |
|         - | 3231 | ` */` |
|        38 | 3232 | `static void VmThrowUserDeprecatedFmt(ph7_vm *pVm,const char *zFmt,...)` |
|         2 | 3233 | `{` |
|         - | 3234 | `	va_list ap;` |
|        40 | 3235 | `	va_start(ap,zFmt);` |
|        40 | 3236 | `	PH7_VmThrowErrorAp(pVm,0,E_USER_DEPRECATED,zFmt,ap);` |
|        40 | 3237 | `	va_end(ap);` |
|        40 | 3238 | `}` |
|         - | 3239 | `/*` |
|         - | 3240 | ` * php 8.4 #[\Deprecated]: emit the E_USER_DEPRECATED notice for a call to a user` |
|         - | 3241 | ` * function/method carrying the attribute. Message shapes (php-exact):` |
|         - | 3242 | ` *   Function f() is deprecated` |
|         - | 3243 | ` *   Method C::m() is deprecated since 2.0, use g() instead` |
|         - | 3244 | ` * ("since" from the attribute's since: argument; the trailing ", msg" from` |
|         - | 3245 | ` * message:/positional #1.) The attribute arguments are compiled constant` |
|         - | 3246 | ` * expressions — evaluated via VmLocalExec, the reflection thunks' pattern.` |
|         - | 3247 | ` * Called from OP_CALL once per call, only when the callee HAS attributes;` |
|         - | 3248 | ` * pDeclClass names the method's declaring class (0 for plain functions).` |
|         - | 3249 | ` */` |
|         - | 3250 | `/*` |
|         - | 3251 | ` * Expand a global constant's value, emitting the php 8.5 #[\Deprecated]` |
|         - | 3252 | ` * E_USER_DEPRECATED notice first when the constant carries attributes` |
|         - | 3253 | `` * (`Constant GD is deprecated since 1.2` — every access re-warns).`` |
|         - | 3254 | ` */` |
|         - | 3255 | `static void VmDeprecatedAttrNoticeSubject(ph7_vm *pVm,SySet *pAttrs,` |
|         - | 3256 | `	const char *zKind,const SyString *pQual,const SyString *pName);` |
|         - | 3257 | `/*` |
|         - | 3258 | ` * Expand a constant, emitting the php 8.5 #[\Deprecated] E_USER_DEPRECATED notice` |
|         - | 3259 | `` * first when a USERLAND constant carries the attribute (`Constant GD is deprecated`` |
|         - | 3260 | `` * since 1.2` — every access re-warns). Engine constants php merely deprecates are`` |
|         - | 3261 | ` * not mimicked: PHL removes them outright (see the scope policy), so there is no` |
|         - | 3262 | ` * engine-side E_DEPRECATED list here.` |
|         - | 3263 | ` *` |
|         - | 3264 | `` * A `const NAME = <expr>;` statement compiles its initializer to a bytecode`` |
|         - | 3265 | ` * program and PH7_VmExpandConstantValue RUNS it — so the value was re-computed` |
|         - | 3266 | ` * on EVERY read. For anything with an identity or a side effect that is a wrong` |
|         - | 3267 | `` * answer, not a slow one: `const C = new Foo();` gave a DIFFERENT object each`` |
|         - | 3268 | `` * time (`C === C` was false, and `Foo::$count` counted one construction per`` |
|         - | 3269 | ` * read) where php evaluates the initializer once and hands the same value out` |
|         - | 3270 | ` * for ever. The first successful expansion is kept, and the constant becomes an` |
|         - | 3271 | ` * ordinary value-backed one — exactly the shape define() registers, so` |
|         - | 3272 | ` * redefinition frees it through the path that already existed.` |
|         - | 3273 | ` *` |
|         - | 3274 | ` * Not cached when the initializer did not complete: a throw, an exit(), or a` |
|         - | 3275 | ` * MUTED evaluation (php has not reached this code, so nothing may be observable)` |
|         - | 3276 | ` * must all be retried rather than frozen into a half-built value.` |
|         - | 3277 | ` */` |
|    301823 | 3278 | `PH7_PRIVATE sxi32 VmExpandConstantOnce(ph7_vm *pVm,ph7_constant *pCons,ph7_value *pOut)` |
|         5 | 3279 | `{` |
|         - | 3280 | `	const void *pResumeBefore,*pInlineBefore;` |
|         - | 3281 | `	sxi32 rc;` |
|    301828 | 3282 | `	if( pCons->xExpand != PH7_VmExpandConstantValue ){` |
|    301646 | 3283 | `		pCons->xExpand(pOut,pCons->pUserData);` |
|    301646 | 3284 | `		return SXRET_OK;` |
|         - | 3285 | `	}` |
|         - | 3286 | `	/* The initializer's own status. PH7_VmExpandConstantValue drops VmLocalExec's` |
|         - | 3287 | `	 * return code (ProcConstant answers void), so the program is driven from here` |
|         - | 3288 | `	 * instead — a caller with no way to see a throw would otherwise cache a` |
|         - | 3289 | `	 * half-built value and keep running past it. */` |
|       187 | 3290 | `	pResumeBefore = (const void *)pVm->pResumeFrame;` |
|       187 | 3291 | `	pInlineBefore = (const void *)pVm->pInlineInstr;` |
|       187 | 3292 | `	rc = VmLocalExec(pVm,(SySet *)pCons->pUserData,pOut,FALSE);` |
|       182 | 3293 | `	if( pVm->nMuteThrow > 0 \|\| rc == PH7_ABORT` |
|       161 | 3294 | `	 \|\| VmLocalExecThrew(pVm,rc,pResumeBefore,pInlineBefore) ){` |
|         - | 3295 | `		/* Did not complete — a throw, an exit(), or a MUTED evaluation (php has` |
|         - | 3296 | `		 * not reached this code, so nothing may be observable). Retry it next` |
|         - | 3297 | `		 * time rather than freezing a value the initializer never produced:` |
|         - | 3298 | ``		 * `const A = LATER; …; define('LATER',5);` must still answer 5. */`` |
|        36 | 3299 | `		return rc == SXRET_OK ? PH7_EXCEPTION : rc;` |
|         - | 3300 | `	}` |
|         - | 3301 | `	{` |
|       155 | 3302 | `		ph7_value *pKeep = (ph7_value *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|       155 | 3303 | `		if( pKeep == 0 ){` |
|       ! 0 | 3304 | `			return SXRET_OK; /* out of memory: stay lazy rather than fail the read */` |
|         - | 3305 | `		}` |
|       155 | 3306 | `		PH7_MemObjInit(pVm,pKeep);` |
|       155 | 3307 | `		PH7_MemObjStore(pOut,pKeep);` |
|       155 | 3308 | `		pCons->xExpand = VmExpandUserConstant;` |
|       155 | 3309 | `		pCons->pUserData = pKeep;` |
|         - | 3310 | `	}` |
|       155 | 3311 | `	return SXRET_OK;` |
|    148801 | 3312 | `}` |
|    164416 | 3313 | `PH7_PRIVATE void VmExpandConstantWithNotice(ph7_vm *pVm,ph7_constant *pCons,ph7_value *pOut)` |
|         5 | 3314 | `{` |
|         - | 3315 | `	/* An ENGINE constant php deprecated the symbol of says so when a program` |
|         - | 3316 | `	 * names it -- five of the six were silent here. Listing the table is not` |
|         - | 3317 | `	 * naming one, which is what bConstEnum says. */` |
|    164421 | 3318 | `	if( pCons->zDeprecated && !pVm->bConstEnum ){` |
|        77 | 3319 | `		VmErrorFormat(pVm,8192 /* E_DEPRECATED */,` |
|        25 | 3320 | `			"Constant %z is deprecated since %s",&pCons->sName,pCons->zDeprecated);` |
|        25 | 3321 | `	}` |
|    164421 | 3322 | `	if( SySetUsed(&pCons->aAttrs) > 0 ){` |
|         7 | 3323 | `		VmDeprecatedAttrNoticeSubject(pVm,&pCons->aAttrs,"Constant",0,&pCons->sName);` |
|         3 | 3324 | `	}` |
|    164421 | 3325 | `	VmExpandConstantOnce(pVm,pCons,pOut);` |
|    164421 | 3326 | `}` |
|         - | 3327 | `/*` |
|         - | 3328 | ` * Query a GLOBAL constant by its exact (case-sensitive) name and expand its` |
|         - | 3329 | ` * value into pOut, which the caller has initialized. Returns 1 when the` |
|         - | 3330 | ` * constant exists. The ini scanner's NORMAL/TYPED value interpretation is the` |
|         - | 3331 | ` * caller: php substitutes a defined constant's value for a bare identifier` |
|         - | 3332 | ` * token inside an unquoted ini value.` |
|         - | 3333 | ` */` |
|       608 | 3334 | `PH7_PRIVATE int PH7_VmQueryConstant(ph7_vm *pVm,const char *zName,sxu32 nName,ph7_value *pOut)` |
|         5 | 3335 | `{` |
|         - | 3336 | `	SyHashEntry *pEntry;` |
|         - | 3337 | `	ph7_constant *pCons;` |
|       613 | 3338 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)zName,nName);` |
|       613 | 3339 | `	if( pEntry == 0 ){` |
|       581 | 3340 | `		return 0;` |
|         - | 3341 | `	}` |
|        36 | 3342 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        36 | 3343 | `	VmExpandConstantWithNotice(pVm,pCons,pOut);` |
|        36 | 3344 | `	return 1;` |
|       309 | 3345 | `}` |
|         - | 3346 | `/*` |
|         - | 3347 | ` * Scan a declared-attribute set for #[\Deprecated]; when found, evaluate its` |
|         - | 3348 | ` * message:/since: arguments (positional #0 = message, #1 = since) into the` |
|         - | 3349 | ` * caller's values and return TRUE. The base subject text ("Function f()",` |
|         - | 3350 | ` * "Constant C::K") is the caller's business.` |
|         - | 3351 | ` */` |
|       404 | 3352 | `static int VmDeprecatedAttrExtract(ph7_vm *pVm,SySet *pAttrs,` |
|         - | 3353 | `	ph7_value *pMsg,int *pbMsg,ph7_value *pSince,int *pbSince)` |
|         5 | 3354 | `{` |
|       409 | 3355 | `	ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|         - | 3356 | `	sxu32 n;` |
|       409 | 3357 | `	*pbMsg = *pbSince = 0;` |
|       779 | 3358 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; ++n ){` |
|       413 | 3359 | `		ph7_attribute *pAttr = &aAttr[n];` |
|         - | 3360 | `		ph7_attr_arg *aArg;` |
|       413 | 3361 | `		sxu32 i,nPos = 0;` |
|       408 | 3362 | `		if( SyStringLength(&pAttr->sName) != sizeof("Deprecated")-1` |
|       228 | 3363 | `		 \|\| SyStrnicmp(SyStringData(&pAttr->sName),"Deprecated",sizeof("Deprecated")-1) != 0 ){` |
|       375 | 3364 | `			continue;` |
|         - | 3365 | `		}` |
|        40 | 3366 | `		aArg = (ph7_attr_arg *)SySetBasePtr(&pAttr->aArgs);` |
|        58 | 3367 | `		for( i = 0 ; i < SySetUsed(&pAttr->aArgs) ; ++i ){` |
|        19 | 3368 | `			ph7_attr_arg *pArg = &aArg[i];` |
|        19 | 3369 | `			int isMsg = 0,isSince = 0;` |
|        19 | 3370 | `			if( SyStringLength(&pArg->sName) == 0 ){` |
|         3 | 3371 | `				isMsg = (nPos == 0);` |
|         3 | 3372 | `				isSince = (nPos == 1);` |
|         3 | 3373 | `				nPos++;` |
|        18 | 3374 | `			}else if( SyStringLength(&pArg->sName) == sizeof("message")-1` |
|        12 | 3375 | `			 && SyMemcmp(SyStringData(&pArg->sName),"message",sizeof("message")-1) == 0 ){` |
|         7 | 3376 | `				isMsg = 1;` |
|        14 | 3377 | `			}else if( SyStringLength(&pArg->sName) == sizeof("since")-1` |
|        11 | 3378 | `			 && SyMemcmp(SyStringData(&pArg->sName),"since",sizeof("since")-1) == 0 ){` |
|        11 | 3379 | `				isSince = 1;` |
|         5 | 3380 | `			}` |
|        19 | 3381 | `			if( isMsg && !*pbMsg && SySetUsed(&pArg->aByteCode) > 0 ){` |
|        13 | 3382 | `				if( VmLocalExec(pVm,&pArg->aByteCode,pMsg,FALSE) == SXRET_OK ){` |
|         9 | 3383 | `					if( (pMsg->iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 3384 | `						PH7_MemObjToString(pMsg);` |
|       ! 0 | 3385 | `					}` |
|         9 | 3386 | `					*pbMsg = 1;` |
|         5 | 3387 | `				}` |
|        15 | 3388 | `			}else if( isSince && !*pbSince && SySetUsed(&pArg->aByteCode) > 0 ){` |
|        11 | 3389 | `				if( VmLocalExec(pVm,&pArg->aByteCode,pSince,FALSE) == SXRET_OK ){` |
|        11 | 3390 | `					if( (pSince->iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 3391 | `						PH7_MemObjToString(pSince);` |
|       ! 0 | 3392 | `					}` |
|        11 | 3393 | `					*pbSince = 1;` |
|         5 | 3394 | `				}` |
|         5 | 3395 | `			}` |
|        10 | 3396 | `		}` |
|        40 | 3397 | `		return 1;` |
|       ! 0 | 3398 | `	}` |
|       371 | 3399 | `	return 0;` |
|       207 | 3400 | `}` |
|         - | 3401 | `/*` |
|         - | 3402 | ` * Append php's " since X" / ", message" suffixes to a built base subject and` |
|         - | 3403 | ` * emit the E_USER_DEPRECATED notice.` |
|         - | 3404 | ` */` |
|        38 | 3405 | `static void VmDeprecatedEmit(ph7_vm *pVm,SyBlob *pOut,` |
|         - | 3406 | `	ph7_value *pMsg,int bMsg,ph7_value *pSince,int bSince)` |
|         2 | 3407 | `{` |
|        40 | 3408 | `	if( bSince && SyBlobLength(&pSince->sBlob) > 0 ){` |
|        16 | 3409 | `		SyBlobFormat(pOut," since %.*s",(int)SyBlobLength(&pSince->sBlob),` |
|        10 | 3410 | `			(const char *)SyBlobData(&pSince->sBlob));` |
|         5 | 3411 | `	}` |
|        40 | 3412 | `	if( bMsg && SyBlobLength(&pMsg->sBlob) > 0 ){` |
|        13 | 3413 | `		SyBlobFormat(pOut,", %.*s",(int)SyBlobLength(&pMsg->sBlob),` |
|         8 | 3414 | `			(const char *)SyBlobData(&pMsg->sBlob));` |
|         4 | 3415 | `	}` |
|        40 | 3416 | `	VmThrowUserDeprecatedFmt(pVm,"%.*s",(int)SyBlobLength(pOut),(const char *)SyBlobData(pOut));` |
|        40 | 3417 | `}` |
|         - | 3418 | `/*` |
|         - | 3419 | ` * Generic #[\Deprecated] notice for a named subject:` |
|         - | 3420 | ` * "<Kind> [Qual::]Name is deprecated[ since X][, message]".` |
|         - | 3421 | ` */` |
|        20 | 3422 | `static void VmDeprecatedAttrNoticeSubject(ph7_vm *pVm,SySet *pAttrs,` |
|         - | 3423 | `	const char *zKind,const SyString *pQual,const SyString *pName)` |
|         2 | 3424 | `{` |
|         - | 3425 | `	ph7_value sMsg,sSince;` |
|         - | 3426 | `	SyBlob sOut;` |
|         - | 3427 | `	int bMsg,bSince;` |
|        22 | 3428 | `	PH7_MemObjInit(pVm,&sMsg);` |
|        22 | 3429 | `	PH7_MemObjInit(pVm,&sSince);` |
|        22 | 3430 | `	if( VmDeprecatedAttrExtract(pVm,pAttrs,&sMsg,&bMsg,&sSince,&bSince) ){` |
|        18 | 3431 | `		SyBlobInit(&sOut,&pVm->sAllocator);` |
|        18 | 3432 | `		if( pQual ){` |
|        14 | 3433 | `			SyBlobFormat(&sOut,"%s %z::%z is deprecated",zKind,pQual,pName);` |
|         8 | 3434 | `		}else{` |
|         5 | 3435 | `			SyBlobFormat(&sOut,"%s %z is deprecated",zKind,pName);` |
|         - | 3436 | `		}` |
|        18 | 3437 | `		VmDeprecatedEmit(pVm,&sOut,&sMsg,bMsg,&sSince,bSince);` |
|        18 | 3438 | `		SyBlobRelease(&sOut);` |
|         8 | 3439 | `	}` |
|        22 | 3440 | `	PH7_MemObjRelease(&sMsg);` |
|        22 | 3441 | `	PH7_MemObjRelease(&sSince);` |
|        22 | 3442 | `}` |
|         - | 3443 | `/*` |
|         - | 3444 | ` * The functions and methods php 8.x deprecated, and the clause each notice ends` |
|         - | 3445 | `` * with. Calling one raises `Function f() is deprecated since <clause>` (or`` |
|         - | 3446 | `` * `Method C::m() ...`) at E_DEPRECATED, BEFORE the callee's arity and type`` |
|         - | 3447 | `` * screens -- `curl_close()` with no argument warns first and throws the`` |
|         - | 3448 | ` * ArgumentCountError second -- and the export format's head reads the same fact` |
|         - | 3449 | `` * as `<internal, deprecated:curl>`.`` |
|         - | 3450 | ` *` |
|         - | 3451 | ` * Marked here rather than raised from each body because the notice belongs to` |
|         - | 3452 | ` * the CALL and not to what the body does (php warns and then runs it), and` |
|         - | 3453 | ` * because the subject is php's own: it names the DECLARING class even for a` |
|         - | 3454 | `` * call through a subclass, so a `MyStore extends SplObjectStorage` still reads`` |
|         - | 3455 | `` * `SplObjectStorage::attach()`. The stamp runs once, after every extension has`` |
|         - | 3456 | ` * installed, so a name a build does not carry is simply skipped.` |
|         - | 3457 | ` *` |
|         - | 3458 | ` * Only names this engine SHIPS are listed; php's own deprecated set is larger` |
|         - | 3459 | ` * (strftime, utf8_encode, the whole mhash family) and every one of those is a` |
|         - | 3460 | ` * name PHL does not have.` |
|         - | 3461 | ` */` |
|         - | 3462 | `static const ph7_deprecated_name aDeprecatedFunc[] = {` |
|         - | 3463 | `	{ "curl_close",        "8.5, as it has no effect since PHP 8.0" },` |
|         - | 3464 | `	/* Deprecated together in 8.1: both are the single-answer face of the one` |
|         - | 3465 | `	 * call that returns all nine. */` |
|         - | 3466 | `	{ "date_sunrise",      "8.1, use date_sun_info() instead" },` |
|         - | 3467 | `	{ "date_sunset",       "8.1, use date_sun_info() instead" },` |
|         - | 3468 | `	/* ext/zip's whole procedural half, deprecated together in 8.0. php names a` |
|         - | 3469 | `	 * replacement for seven of the ten and none for the other three. */` |
|         - | 3470 | `	{ "zip_open",          "8.0, use ZipArchive::open() instead" },` |
|         - | 3471 | `	{ "zip_close",         "8.0, use ZipArchive::close() instead" },` |
|         - | 3472 | `	{ "zip_read",          "8.0, use ZipArchive::statIndex() instead" },` |
|         - | 3473 | `	{ "zip_entry_open",    "8.0" },` |
|         - | 3474 | `	{ "zip_entry_close",   "8.0" },` |
|         - | 3475 | `	{ "zip_entry_read",    "8.0, use ZipArchive::getFromIndex() instead" },` |
|         - | 3476 | `	{ "zip_entry_name",    "8.0, use ZipArchive::statIndex() instead" },` |
|         - | 3477 | `	{ "zip_entry_compressedsize", "8.0, use ZipArchive::statIndex() instead" },` |
|         - | 3478 | `	{ "zip_entry_filesize", "8.0, use ZipArchive::statIndex() instead" },` |
|         - | 3479 | `	{ "zip_entry_compressionmethod", "8.0, use ZipArchive::statIndex() instead" },` |
|         - | 3480 | `	{ "finfo_close",       "8.5, as finfo objects are freed automatically" },` |
|         - | 3481 | `	{ "curl_share_close",  "8.5, as it has no effect since PHP 8.0" },` |
|         - | 3482 | `	{ "DateInterval::__wakeup",` |
|         - | 3483 | `	  "8.5, this method is obsolete, as serialization hooks are provided by "` |
|         - | 3484 | `	  "__unserialize() and __serialize()" },` |
|         - | 3485 | `	{ "DatePeriod::__wakeup",` |
|         - | 3486 | `	  "8.5, this method is obsolete, as serialization hooks are provided by "` |
|         - | 3487 | `	  "__unserialize() and __serialize()" },` |
|         - | 3488 | `	{ "DateTime::__wakeup",` |
|         - | 3489 | `	  "8.5, this method is obsolete, as serialization hooks are provided by "` |
|         - | 3490 | `	  "__unserialize() and __serialize()" },` |
|         - | 3491 | `	{ "DateTimeInterface::__wakeup",` |
|         - | 3492 | `	  "8.5, this method is obsolete, as serialization hooks are provided by "` |
|         - | 3493 | `	  "__unserialize() and __serialize()" },` |
|         - | 3494 | `	{ "DateTimeImmutable::__wakeup",` |
|         - | 3495 | `	  "8.5, this method is obsolete, as serialization hooks are provided by "` |
|         - | 3496 | `	  "__unserialize() and __serialize()" },` |
|         - | 3497 | `	{ "DateTimeZone::__wakeup",` |
|         - | 3498 | `	  "8.5, this method is obsolete, as serialization hooks are provided by "` |
|         - | 3499 | `	  "__unserialize() and __serialize()" },` |
|         - | 3500 | `	{ "SplFixedArray::__wakeup",` |
|         - | 3501 | `	  "8.4, this method is obsolete, as serialization hooks are provided by "` |
|         - | 3502 | `	  "__unserialize() and __serialize()" },` |
|         - | 3503 | `	{ "SplFileInfo::_bad_state_ex", "8.2" },` |
|         - | 3504 | `	{ "SplObjectStorage::attach",` |
|         - | 3505 | `	  "8.5, use method SplObjectStorage::offsetSet() instead" },` |
|         - | 3506 | `	{ "SplObjectStorage::contains",` |
|         - | 3507 | `	  "8.5, use method SplObjectStorage::offsetExists() instead" },` |
|         - | 3508 | `	{ "SplObjectStorage::detach",` |
|         - | 3509 | `	  "8.5, use method SplObjectStorage::offsetUnset() instead" },` |
|         - | 3510 | `	{ "ReflectionFunction::isDisabled",` |
|         - | 3511 | `	  "8.0, as ReflectionFunction can no longer be constructed for disabled functions" },` |
|         - | 3512 | `	{ "ReflectionMethod::setAccessible", "8.5, as it has no effect since PHP 8.1" },` |
|         - | 3513 | `	{ "ReflectionProperty::setAccessible", "8.5, as it has no effect since PHP 8.1" },` |
|         - | 3514 | `	{ "ReflectionParameter::getClass",` |
|         - | 3515 | `	  "8.0, use ReflectionParameter::getType() instead" },` |
|         - | 3516 | `	{ "ReflectionParameter::isArray",` |
|         - | 3517 | `	  "8.0, use ReflectionParameter::getType() instead" },` |
|         - | 3518 | `	{ "ReflectionParameter::isCallable",` |
|         - | 3519 | `	  "8.0, use ReflectionParameter::getType() instead" },` |
|         - | 3520 | `};` |
|         - | 3521 | `/* The ph7_user_func behind one entry: a global builtin, or a native method's` |
|         - | 3522 | ` * own C body reached through its ph7_vm_func. */` |
|    214112 | 3523 | `static ph7_user_func * VmDeprecatedTarget(ph7_vm *pVm,const char *zName)` |
|         5 | 3524 | `{` |
|    214117 | 3525 | `	const char *zSep = 0;` |
|         - | 3526 | `	SyHashEntry *pEntry;` |
|         - | 3527 | `	sxu32 n;` |
|   3345505 | 3528 | `	for( n = 0 ; zName[n] != '\0' ; ++n ){` |
|   3245140 | 3529 | `		if( zName[n] == ':' && zName[n+1] == ':' ){` |
|    113752 | 3530 | `			zSep = &zName[n];` |
|    113752 | 3531 | `			break;` |
|         - | 3532 | `		}` |
|   1563125 | 3533 | `	}` |
|    214117 | 3534 | `	if( zSep == 0 ){` |
|    100370 | 3535 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zName,SyStrlen(zName));` |
|    100370 | 3536 | `		return pEntry ? (ph7_user_func *)pEntry->pUserData : 0;` |
|         - | 3537 | `	}` |
|         - | 3538 | `	{` |
|    113752 | 3539 | `		ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zName,(sxu32)(zSep - zName),FALSE,0);` |
|         - | 3540 | `		ph7_class_method *pMeth;` |
|    113752 | 3541 | `		if( pClass == 0 ){` |
|       ! 0 | 3542 | `			return 0;` |
|         - | 3543 | `		}` |
|    113752 | 3544 | `		pEntry = SyHashGet(&pClass->hMethod,(const void *)(zSep + 2),SyStrlen(zSep + 2));` |
|    113752 | 3545 | `		if( pEntry == 0 ){` |
|         2 | 3546 | `			return 0;` |
|         - | 3547 | `		}` |
|    113750 | 3548 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|    113750 | 3549 | `		return (pMeth->sFunc.iFlags & VM_FUNC_NATIVE) ? pMeth->sFunc.pNative : 0;` |
|         - | 3550 | `	}` |
|    106885 | 3551 | `}` |
|      6691 | 3552 | `PH7_PRIVATE void PH7_MarkDeprecatedFunctions(ph7_vm *pVm)` |
|         5 | 3553 | `{` |
|         - | 3554 | `	sxu32 n;` |
|    220808 | 3555 | `	for( n = 0 ; n < SX_ARRAYSIZE(aDeprecatedFunc) ; ++n ){` |
|    214117 | 3556 | `		ph7_user_func *pTarget = VmDeprecatedTarget(&(*pVm),aDeprecatedFunc[n].zName);` |
|    214117 | 3557 | `		if( pTarget ){` |
|    214115 | 3558 | `			pTarget->pDeprecated = &aDeprecatedFunc[n];` |
|    106879 | 3559 | `		}` |
|    106885 | 3560 | `	}` |
|      6696 | 3561 | `}` |
|         - | 3562 | `/*` |
|         - | 3563 | ` * The notice itself, raised from the one OP_CALL block a builtin and a native` |
|         - | 3564 | ` * method share. php words a qualified subject as a METHOD and a bare one as a` |
|         - | 3565 | ` * FUNCTION, which is exactly what the "::" in the recorded name says.` |
|         - | 3566 | ` */` |
|       272 | 3567 | `PH7_PRIVATE void PH7_VmDeprecatedCallNotice(ph7_vm *pVm,const ph7_deprecated_name *pDep)` |
|         3 | 3568 | `{` |
|       275 | 3569 | `	int bMethod = 0;` |
|         - | 3570 | `	sxu32 n;` |
|      3849 | 3571 | `	for( n = 0 ; pDep->zName[n] != '\0' ; ++n ){` |
|      3625 | 3572 | `		if( pDep->zName[n] == ':' ){` |
|        49 | 3573 | `			bMethod = 1;` |
|        49 | 3574 | `			break;` |
|         - | 3575 | `		}` |
|      1790 | 3576 | `	}` |
|       411 | 3577 | `	VmErrorFormat(&(*pVm),8192 /* E_DEPRECATED */,"%s %s() is deprecated since %s",` |
|       272 | 3578 | `		bMethod ? "Method" : "Function",pDep->zName,pDep->zWhy);` |
|       275 | 3579 | `}` |
|       384 | 3580 | `PH7_PRIVATE void VmDeprecatedAttrNotice(ph7_vm *pVm,ph7_vm_func *pFunc,ph7_class *pDeclClass)` |
|         5 | 3581 | `{` |
|         - | 3582 | `	ph7_value sMsg,sSince;` |
|         - | 3583 | `	SyBlob sOut;` |
|         - | 3584 | `	int bMsg,bSince;` |
|       389 | 3585 | `	PH7_MemObjInit(pVm,&sMsg);` |
|       389 | 3586 | `	PH7_MemObjInit(pVm,&sSince);` |
|       389 | 3587 | `	if( VmDeprecatedAttrExtract(pVm,&pFunc->aAttrs,&sMsg,&bMsg,&sSince,&bSince) ){` |
|        24 | 3588 | `		SyBlobInit(&sOut,&pVm->sAllocator);` |
|        24 | 3589 | `		if( pDeclClass ){` |
|         5 | 3590 | `			SyBlobFormat(&sOut,"Method %z::%z() is deprecated",&pDeclClass->sDisp,&pFunc->sName);` |
|         3 | 3591 | `		}else{` |
|        20 | 3592 | `			SyBlobFormat(&sOut,"Function %z() is deprecated",&pFunc->sName);` |
|         - | 3593 | `		}` |
|        24 | 3594 | `		VmDeprecatedEmit(pVm,&sOut,&sMsg,bMsg,&sSince,bSince);` |
|        24 | 3595 | `		SyBlobRelease(&sOut);` |
|        11 | 3596 | `	}` |
|       389 | 3597 | `	PH7_MemObjRelease(&sMsg);` |
|       389 | 3598 | `	PH7_MemObjRelease(&sSince);` |
|       389 | 3599 | `}` |
|         - | 3600 | `/*` |
|         - | 3601 | ` * php 8.5's #[\NoDiscard] warning, raised at the CALL, before the body runs, and` |
|         - | 3602 | ` * once per call (a loop warns every time round).` |
|         - | 3603 | ` *` |
|         - | 3604 | ` * The subject is a "function" unless the callee has a class scope, in which case` |
|         - | 3605 | ` * php names the DECLARING class -- an inherited method reports the class that` |
|         - | 3606 | `` * wrote it, and so does `parent::m()`. The tail after php's sentence is the`` |
|         - | 3607 | ` * attribute's own message: a constant EXPRESSION for a compiled declaration` |
|         - | 3608 | ` * (evaluated here, where the constants it may name exist) and a fixed string for` |
|         - | 3609 | ` * an internal member, which is how php words the immutable date mutators.` |
|         - | 3610 | ` */` |
|        56 | 3611 | `PH7_PRIVATE void VmNoDiscardWarn(ph7_vm *pVm,ph7_vm_func *pFunc,ph7_class *pDeclClass)` |
|         1 | 3612 | `{` |
|        57 | 3613 | `	ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(&pFunc->aAttrs);` |
|        57 | 3614 | `	const SyString *pName = &pFunc->sName;` |
|         - | 3615 | `	ph7_value sMsg;` |
|         - | 3616 | `	SyBlob sOut;` |
|        57 | 3617 | `	int bMsg = 0;` |
|         - | 3618 | `	sxu32 n;` |
|         - | 3619 | ``	/* A closure reports php's `{closure:SCOPE:LINE}` spelling, like every other`` |
|         - | 3620 | `	 * diagnostic that names one. */` |
|        57 | 3621 | `	if( SyStringLength(&pFunc->sClosureName) > 0 ){` |
|         7 | 3622 | `		pName = &pFunc->sClosureName;` |
|         3 | 3623 | `	}` |
|        57 | 3624 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|        57 | 3625 | `	if( pDeclClass ){` |
|        23 | 3626 | `		SyBlobFormat(&sOut,"The return value of method %z::%z() should either be used "` |
|        11 | 3627 | `			"or intentionally ignored by casting it as (void)",&pDeclClass->sDisp,pName);` |
|        12 | 3628 | `	}else{` |
|        35 | 3629 | `		SyBlobFormat(&sOut,"The return value of function %z() should either be used "` |
|        17 | 3630 | `			"or intentionally ignored by casting it as (void)",pName);` |
|         - | 3631 | `	}` |
|        57 | 3632 | `	PH7_MemObjInit(pVm,&sMsg);` |
|        57 | 3633 | `for( n = 0 ; !bMsg && n < SySetUsed(&pFunc->aAttrs) ; ++n ){` |
|         - | 3634 | `		ph7_attr_arg *aArg;` |
|        57 | 3635 | `		sxu32 i,nPos = 0;` |
|        56 | 3636 | `		if( SyStringLength(&aAttr[n].sName) != sizeof("NoDiscard")-1` |
|        57 | 3637 | `		 \|\| SyStrnicmp(SyStringData(&aAttr[n].sName),"NoDiscard",` |
|        28 | 3638 | `				sizeof("NoDiscard")-1) != 0 ){` |
|       ! 0 | 3639 | `			continue;` |
|         - | 3640 | `		}` |
|        57 | 3641 | `		aArg = (ph7_attr_arg *)SySetBasePtr(&aAttr[n].aArgs);` |
|        57 | 3642 | `		for( i = 0 ; i < SySetUsed(&aAttr[n].aArgs) ; ++i ){` |
|        11 | 3643 | `			ph7_attr_arg *pArg = &aArg[i];` |
|         - | 3644 | `			int isMsg;` |
|        11 | 3645 | `			if( SyStringLength(&pArg->sName) == 0 ){` |
|         7 | 3646 | `				isMsg = (nPos == 0);` |
|         7 | 3647 | `				nPos++;` |
|         4 | 3648 | `			}else{` |
|         7 | 3649 | `				isMsg = SyStringLength(&pArg->sName) == sizeof("message")-1` |
|         4 | 3650 | `					&& SyMemcmp(SyStringData(&pArg->sName),"message",` |
|         2 | 3651 | `						sizeof("message")-1) == 0;` |
|         - | 3652 | `			}` |
|        11 | 3653 | `			if( !isMsg ){` |
|       ! 0 | 3654 | `				continue;` |
|         - | 3655 | `			}` |
|         - | 3656 | `			/* A compiled declaration holds the message as a constant` |
|         - | 3657 | `			 * EXPRESSION (evaluated here, where the constants it may name` |
|         - | 3658 | `			 * exist); a native one holds it as a literal, like every other` |
|         - | 3659 | `			 * attribute argument a C-declared class carries. */` |
|        11 | 3660 | `			if( SySetUsed(&pArg->aByteCode) > 0 ){` |
|         9 | 3661 | `				if( VmLocalExec(pVm,&pArg->aByteCode,&sMsg,FALSE) != SXRET_OK ){` |
|       ! 0 | 3662 | `					continue;` |
|         1 | 3663 | `				}` |
|         7 | 3664 | `			}else if( pArg->pNativeValue ){` |
|         3 | 3665 | `				PH7_NativeLiteralValue(pVm,pArg->pNativeValue,&sMsg);` |
|         2 | 3666 | `			}else{` |
|       ! 0 | 3667 | `				continue;` |
|         - | 3668 | `			}` |
|        11 | 3669 | `			if( (sMsg.iFlags & MEMOBJ_STRING) == 0 ){` |
|         3 | 3670 | `				PH7_MemObjToString(&sMsg);` |
|         1 | 3671 | `			}` |
|        11 | 3672 | `			bMsg = 1;` |
|        11 | 3673 | `			break;` |
|       ! 0 | 3674 | `		}` |
|        57 | 3675 | `		break;` |
|       ! 0 | 3676 | `	}` |
|        57 | 3677 | `	if( bMsg && SyBlobLength(&sMsg.sBlob) > 0 ){` |
|        13 | 3678 | `		SyBlobFormat(&sOut,", %.*s",(int)SyBlobLength(&sMsg.sBlob),` |
|         8 | 3679 | `			(const char *)SyBlobData(&sMsg.sBlob));` |
|         4 | 3680 | `	}` |
|        57 | 3681 | `	PH7_MemObjRelease(&sMsg);` |
|         - | 3682 | `	/* php raises it as E_USER_WARNING (512), not the engine's E_WARNING: the` |
|         - | 3683 | `	 * attribute is userland-authored, the same reason #[\Deprecated] is` |
|         - | 3684 | `	 * E_USER_DEPRECATED. A set_error_handler sees the number. */` |
|        85 | 3685 | `	VmThrowUserWarningFmt(pVm,"%.*s",` |
|        56 | 3686 | `		(int)SyBlobLength(&sOut),(const char *)SyBlobData(&sOut));` |
|        57 | 3687 | `	SyBlobRelease(&sOut);` |
|        57 | 3688 | `}` |
|         - | 3689 | `/*` |
|         - | 3690 | ` * Same notice for a #[\Deprecated] class constant, at its static-access site:` |
|         - | 3691 | `` * php's `Constant C::K is deprecated` (each access re-warns).`` |
|         - | 3692 | ` */` |
|        14 | 3693 | `PH7_PRIVATE void VmDeprecatedConstNotice(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pMember)` |
|         2 | 3694 | `{` |
|        23 | 3695 | `	VmDeprecatedAttrNoticeSubject(pVm,&pMember->aAttrs,` |
|        14 | 3696 | `		(pMember->iFlags & PH7_CLASS_ATTR_ENUMCASE) ? "Enum case" : "Constant",` |
|        14 | 3697 | `		&pClass->sName,&pMember->sName);` |
|        16 | 3698 | `}` |
|         - | 3699 | `/*` |
|         - | 3700 | `` * The USER-VISIBLE string coercion a builtin performs on a `mixed` argument or`` |
|         - | 3701 | ` * array element: the builtin-side twin of PH7_MemObjToStringUV's opcode sites.` |
|         - | 3702 | ` * An ARRAY warns "Array to string conversion" and still renders as "Array"; an` |
|         - | 3703 | ` * object whose class has no __toString() -- or one whose __toString() threw --` |
|         - | 3704 | ` * is php's catchable "could not be converted to string" Error, and the builtin` |
|         - | 3705 | ` * must answer that instead of a value.` |
|         - | 3706 | ` *` |
|         - | 3707 | ` * On success pzData and pnLen receive the NUL-terminated bytes (both optional).` |
|         - | 3708 | ` * On a throw they are set to the empty string and the status is returned AND` |
|         - | 3709 | ` * recorded on the call context, so OP_CALL cannot mistake the call for a normal` |
|         - | 3710 | ` * return; a builtin that has already produced output (printf) still keeps it,` |
|         - | 3711 | ` * which is what php does.` |
|         - | 3712 | ` *` |
|         - | 3713 | ` * The public ph7_value_to_string() stays SILENT on purpose: it is the embedder` |
|         - | 3714 | ` * API, and the INTERNAL coercions behind it (array-key canonicalisation, sort` |
|         - | 3715 | ` * comparisons, print_r/var_export/serialize) must not throw -- php's do not` |
|         - | 3716 | ` * either.` |
|         - | 3717 | ` */` |
|    756945 | 3718 | `PH7_PRIVATE sxi32 PH7_ValueToStringUV(ph7_context *pCtx,ph7_value *pValue,const char **pzData,int *pnLen)` |
|         5 | 3719 | `{` |
|    756950 | 3720 | `	sxi32 rc = PH7_MemObjToStringUV(pValue);` |
|    756950 | 3721 | `	if( rc != SXRET_OK ){` |
|        73 | 3722 | `		if( pCtx ){` |
|        73 | 3723 | `			pCtx->nThrowRc = rc;` |
|        34 | 3724 | `		}` |
|        73 | 3725 | `		if( pzData ){` |
|        69 | 3726 | `			*pzData = "";` |
|        32 | 3727 | `		}` |
|        73 | 3728 | `		if( pnLen ){` |
|        69 | 3729 | `			*pnLen = 0;` |
|        32 | 3730 | `		}` |
|        73 | 3731 | `		return rc;` |
|         - | 3732 | `	}` |
|    756880 | 3733 | `	if( pzData \|\| pnLen ){` |
|    756852 | 3734 | `		const char *zData = ph7_value_to_string(pValue,pnLen);` |
|    756852 | 3735 | `		if( pzData ){` |
|    756852 | 3736 | `			*pzData = zData;` |
|    377660 | 3737 | `		}` |
|    377660 | 3738 | `	}` |
|    756880 | 3739 | `	return SXRET_OK;` |
|    377713 | 3740 | `}` |
|         - | 3741 | `/*` |
|         - | 3742 | ` * The same user-visible coercion for a builtin that php does NOT stop for.` |
|         - | 3743 | ` * zend's zval_get_string leaves the empty string behind when it throws and the` |
|         - | 3744 | `` * C function carries on, so `str_replace()`'s `&$count` still comes back written`` |
|         - | 3745 | ` * from a call that threw. Only the FIRST un-stringable value raises -- a second` |
|         - | 3746 | ` * one would land two Errors for one call -- while every OTHER kind of value is` |
|         - | 3747 | ` * converted normally either way (an array still warns, a scalar still spells` |
|         - | 3748 | ` * itself out), which is what keeps the elements AFTER the failure intact.` |
|         - | 3749 | ` *` |
|         - | 3750 | ` * pzData/pnLen always come back usable, so the caller has nothing to check.` |
|         - | 3751 | ` */` |
|    229445 | 3752 | `PH7_PRIVATE void PH7_ValueToStringUVOnce(ph7_context *pCtx,ph7_value *pValue,` |
|         - | 3753 | `	const char **pzData,int *pnLen)` |
|         5 | 3754 | `{` |
|    229450 | 3755 | `	if( pCtx && pCtx->nThrowRc != 0 && PH7_MemObjIsNotStringable(pValue) ){` |
|       ! 0 | 3756 | `		if( pzData ){ *pzData = ""; }` |
|       ! 0 | 3757 | `		if( pnLen ){ *pnLen = 0; }` |
|       ! 0 | 3758 | `		return;` |
|         - | 3759 | `	}` |
|    229450 | 3760 | `	(void)PH7_ValueToStringUV(pCtx,pValue,pzData,pnLen);` |
|    114367 | 3761 | `}` |
|         - | 3762 | `/*` |
|         - | 3763 | ` * The same coercion again, for a builtin that must FINISH ITS OUTPUT before the` |
|         - | 3764 | ` * Error is raised. php's C functions carry on past the throw and the bytes they` |
|         - | 3765 | ` * write reach the stream BEFORE the exception surfaces:` |
|         - | 3766 | ``  * `file_put_contents($f,['A',$obj,'B'])` leaves "AB" behind and `fputcsv()` `` |
|         - | 3767 | ` * writes its whole line with an empty field. A throw raised from inside a` |
|         - | 3768 | ` * builtin HERE runs the enclosing catch immediately, so raising in place would` |
|         - | 3769 | ` * put the catch's own output in front of the builtin's.` |
|         - | 3770 | ` *` |
|         - | 3771 | ` * Answers the class that could not be converted (and the empty string with it),` |
|         - | 3772 | ` * leaving the caller to raise once its writing is done; 0 when the value` |
|         - | 3773 | ` * converted, which is every other kind -- an array still warns here, in place,` |
|         - | 3774 | ` * exactly as php's does.` |
|         - | 3775 | ` */` |
|     20674 | 3776 | `PH7_PRIVATE ph7_class *PH7_ValueToStringUVDefer(ph7_context *pCtx,ph7_value *pValue,` |
|         - | 3777 | `	const char **pzData,int *pnLen)` |
|         5 | 3778 | `{` |
|     20679 | 3779 | `	if( PH7_MemObjIsNotStringable(pValue) ){` |
|         7 | 3780 | `		ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|         7 | 3781 | `		if( pzData ){ *pzData = ""; }` |
|         7 | 3782 | `		if( pnLen ){ *pnLen = 0; }` |
|         7 | 3783 | `		return pInst ? pInst->pClass : 0;` |
|         - | 3784 | `	}` |
|     20673 | 3785 | `	(void)PH7_ValueToStringUV(pCtx,pValue,pzData,pnLen);` |
|     20673 | 3786 | `	return 0;` |
|     10337 | 3787 | `}` |
|         - | 3788 |  |
