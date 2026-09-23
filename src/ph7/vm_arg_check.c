/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
/*
 * Section:
 *    Builtin-function argument checking: the aBuiltinArity[] min-arity
 *    overrides, the aBuiltinSig[] PHP-8.5 signature table (the single
 *    source of truth for builtin arity/types and Reflection), ZPP type
 *    enforcement for host functions and the deprecation-notice machinery.
 * Status:
 *    Stable.
 */
/*
 * PHP-8 builtin minimum-arity table (band A #5, stage 1).
 *
 * Native builtins carry no formal-parameter signature, so historically each
 * one self-validated its argument count (or, worse, silently degraded to a
 * bogus false/-1/"" return on too few arguments — a PH7-ism that diverges from
 * PHP 8, which throws a catchable ArgumentCountError). This table is the single
 * source of truth for the required minimum: at VM init VmSetBuiltinArity()
 * stamps nMinArg/bAtLeast onto the matching ph7_user_func, and the OP_CALL
 * choke point throws ArgumentCountError before the C routine ever runs.
 *
 * bAtLeast mirrors PHP's ZPP wording: "expects exactly N" when the builtin has
 * no optional/variadic parameters (min == max), "expects at least N" otherwise.
 * Every entry's min count and wording is byte-verified against php 8.5.7.
 *
 * Only functions that currently mis-behave (silent wrong return) are listed;
 * builtins that already self-throw the correct message are intentionally left
 * out so there is no double-check / message drift. The batch-1 block below is
 * the original 31-function seed; the batch-2 block that follows completes the
 * sweep across every remaining silent-degrading builtin (verified against the
 * php 8.5.7 oracle).
 */
static const struct VmBuiltinArity {
	const char *zName;   /* Builtin name (short, unqualified) */
	sxi16 nMin;          /* Minimum required arguments */
	sxu8 bAtLeast;       /* 0 -> "exactly", 1 -> "at least" */
} aBuiltinArity[] = {
	/* String family */
	{ "substr",       2, 1 }, { "substr_count",  2, 1 }, { "str_repeat",     2, 0 },
	{ "str_pad",      2, 1 }, { "strpos",        2, 1 }, { "stripos",        2, 1 },
	{ "strrpos",      2, 1 }, { "strripos",      2, 1 }, { "strstr",         2, 1 },
	{ "stristr",      2, 1 }, { "strrchr",       2, 1 }, { "str_replace",    3, 1 },
	{ "str_ireplace", 3, 1 }, { "strncmp",       3, 0 }, { "strncasecmp",    3, 0 },
	{ "substr_compare",3,1 }, { "strpbrk",       2, 0 }, { "strspn",         2, 1 },
	{ "strcspn",      2, 1 }, { "hexdec",        1, 0 }, { "octdec",         1, 0 },
	{ "bindec",       1, 0 }, { "chunk_split",   1, 1 },
	/* Math family (atan2/intdiv already self-throw the same ArgumentCountError,
	 * so they stay off the table per the disjointness rule above). */
	{ "pow",          2, 0 }, { "fmod",          2, 0 }, { "hypot",          2, 0 },
	{ "log",          1, 1 },
	/* Array family (str_split already self-throws — kept off the table). */
	{ "in_array",     2, 1 }, { "range",         2, 1 },
	{ "implode",      1, 1 }, { "join",          1, 1 },
	/*
	 * Batch 2 (band A #5 continuation) — a systematic sweep of every remaining
	 * builtin that silently degraded on too-few arguments where php 8 throws
	 * ArgumentCountError. Each row's minimum and "exactly"/"at least" wording was
	 * extracted from php 8.5.7's own ArgumentCountError message at the argument
	 * boundary (the message text is byte-identical to the one this table drives).
	 * Functions that already self-throw the correct message are still excluded per
	 * the disjointness rule above.
	 */
	/* String family */
	{ "chop",                      1, 1 },
	{ "explode",                   2, 1 },
	{ "fprintf",                   2, 1 },
	{ "html_entity_decode",        1, 1 },
	{ "htmlentities",              1, 1 },
	{ "htmlspecialchars",          1, 1 },
	{ "htmlspecialchars_decode",   1, 1 },
	{ "lcfirst",                   1, 0 },
	{ "ltrim",                     1, 1 },
	{ "mb_check_encoding",         1, 0 },
	{ "mb_chr",                    1, 1 },
	{ "mb_convert_case",           2, 1 },
	{ "mb_convert_encoding",       2, 1 },
	{ "mb_detect_encoding",        1, 1 },
	{ "mb_ord",                    1, 1 },
	{ "mb_str_split",              1, 1 },
	{ "mb_stripos",                2, 1 },
	{ "mb_strlen",                 1, 1 },
	{ "mb_strpos",                 2, 1 },
	{ "mb_strrpos",                2, 1 },
	{ "mb_strtolower",             1, 1 },
	{ "mb_strtoupper",             1, 1 },
	{ "mb_strwidth",               1, 1 },
	{ "mb_substr",                 2, 1 },
	{ "nl2br",                     1, 1 },
	{ "printf",                    1, 1 },
	{ "quotemeta",                 1, 0 },
	{ "rtrim",                     1, 1 },
	{ "soundex",                   1, 0 },
	{ "sprintf",                   1, 1 },
	{ "str_getcsv",                1, 1 },
	{ "str_shuffle",               1, 0 },
	{ "strcasecmp",                2, 0 },
	{ "strchr",                    2, 1 },
	{ "strcmp",                    2, 0 },
	{ "strnatcasecmp",             2, 0 },
	{ "strnatcmp",                 2, 0 },
	{ "strcoll",                   2, 0 },
	{ "strip_tags",                1, 1 },
	{ "stripslashes",              1, 0 },
	{ "strlen",                    1, 0 },
	{ "strrev",                    1, 0 },
	{ "strtok",                    1, 1 },
	{ "strtolower",                1, 0 },
	{ "strtoupper",                1, 0 },
	{ "strtr",                     2, 0 },
	{ "trim",                      1, 1 },
	{ "ucfirst",                   1, 0 },
	{ "ucwords",                   1, 1 },
	{ "vfprintf",                  3, 0 },
	{ "vprintf",                   2, 0 },
	{ "vsprintf",                  2, 0 },
	{ "wordwrap",                  1, 1 },
	/* Ctype family */
	{ "ctype_alnum",               1, 0 },
	{ "ctype_alpha",               1, 0 },
	{ "ctype_cntrl",               1, 0 },
	{ "ctype_digit",               1, 0 },
	{ "ctype_graph",               1, 0 },
	{ "ctype_lower",               1, 0 },
	{ "ctype_print",               1, 0 },
	{ "ctype_punct",               1, 0 },
	{ "ctype_space",               1, 0 },
	{ "ctype_upper",               1, 0 },
	{ "ctype_xdigit",              1, 0 },
	/* Math family */
	{ "base_convert",              3, 0 },
	{ "cos",                       1, 0 },
	{ "cosh",                      1, 0 },
	{ "crc32",                     1, 0 },
	{ "decbin",                    1, 0 },
	{ "dechex",                    1, 0 },
	{ "decoct",                    1, 0 },
	{ "exp",                       1, 0 },
	{ "log10",                     1, 0 },
	{ "md5",                       1, 1 },
	{ "round",                     1, 1 },
	{ "sha1",                      1, 1 },
	{ "sin",                       1, 0 },
	{ "sinh",                      1, 0 },
	{ "sqrt",                      1, 0 },
	{ "tan",                       1, 0 },
	{ "tanh",                      1, 0 },
	/* Type/var family */
	{ "floatval",                  1, 0 },
	{ "get_resource_id",           1, 0 },
	{ "get_resource_type",         1, 0 },
	{ "gettype",                   1, 0 },
	{ "intval",                    1, 1 },
	{ "is_array",                  1, 0 },
	{ "is_bool",                   1, 0 },
	{ "is_callable",               1, 1 },
	{ "is_double",                 1, 0 },
	{ "is_float",                  1, 0 },
	{ "is_int",                    1, 0 },
	{ "is_integer",                1, 0 },
	{ "is_long",                   1, 0 },
	{ "is_null",                   1, 0 },
	{ "is_numeric",                1, 0 },
	{ "is_object",                 1, 0 },
	{ "is_resource",               1, 0 },
	{ "is_scalar",                 1, 0 },
	{ "is_string",                 1, 0 },
	{ "print_r",                   1, 1 },
	{ "strval",                    1, 0 },
	{ "var_dump",                  1, 1 },
	{ "var_export",                1, 1 },
	/* Array/iterator family */
	{ "array_filter",              1, 1 },
	{ "array_product",             1, 0 },
	{ "array_rand",                1, 1 },
	{ "compact",                   1, 1 },
	{ "current",                   1, 0 },
	{ "end",                       1, 0 },
	{ "extract",                   1, 1 },
	{ "iterator_apply",            2, 1 },
	{ "iterator_count",            1, 0 },
	{ "iterator_to_array",         1, 1 },
	{ "key",                       1, 0 },
	{ "krsort",                    1, 1 },
	{ "ksort",                     1, 1 },
	{ "next",                      1, 0 },
	{ "pos",                       1, 0 },
	{ "prev",                      1, 0 },
	{ "reset",                     1, 0 },
	{ "rsort",                     1, 1 },
	{ "shuffle",                   1, 0 },
	{ "sort",                      1, 1 },
	{ "uasort",                    2, 0 },
	{ "uksort",                    2, 0 },
	{ "usort",                     2, 0 },
	/* Class/reflection family */
	{ "class_alias",               2, 1 },
	{ "class_exists",              1, 1 },
	{ "enum_exists",               1, 1 },
	{ "get_class_methods",         1, 0 },
	{ "get_class_vars",            1, 0 },
	{ "get_object_vars",           1, 0 },
	{ "interface_exists",          1, 1 },
	{ "trait_exists",              1, 1 },
	{ "is_a",                      2, 1 },
	{ "is_subclass_of",            2, 1 },
	{ "method_exists",             2, 0 },
	{ "property_exists",           2, 0 },
	{ "spl_autoload",              1, 1 },
	{ "spl_autoload_unregister",   1, 0 },
	{ "spl_object_hash",           1, 0 },
	{ "spl_object_id",             1, 0 },
	/* Filesystem/IO family */
	{ "basename",                  1, 1 },
	{ "chdir",                     1, 0 },
	{ "chgrp",                     2, 0 },
	{ "dirname",                   1, 1 },
	{ "disk_free_space",           1, 0 },
	{ "disk_total_space",          1, 0 },
	{ "diskfreespace",             1, 0 },
	{ "fclose",                    1, 0 },
	{ "feof",                      1, 0 },
	{ "fflush",                    1, 0 },
	{ "fgetc",                     1, 0 },
	{ "fgetcsv",                   1, 1 },
	{ "file",                      1, 1 },
	{ "file_exists",               1, 0 },
	{ "fileatime",                 1, 0 },
	{ "filectime",                 1, 0 },
	{ "filemtime",                 1, 0 },
	{ "filesize",                  1, 0 },
	{ "filetype",                  1, 0 },
	{ "flock",                     2, 1 },
	{ "fpassthru",                 1, 0 },
	{ "fputcsv",                   2, 1 },
	{ "fputs",                     2, 1 },
	{ "fseek",                     2, 1 },
	{ "fstat",                     1, 0 },
	{ "ftell",                     1, 0 },
	{ "ftruncate",                 2, 0 },
	{ "getopt",                    1, 1 },
	{ "is_dir",                    1, 0 },
	{ "is_executable",             1, 0 },
	{ "is_file",                   1, 0 },
	{ "is_link",                   1, 0 },
	{ "is_readable",               1, 0 },
	{ "is_writable",               1, 0 },
	{ "lstat",                     1, 0 },
	{ "md5_file",                  1, 1 },
	{ "opendir",                   1, 1 },
	{ "pathinfo",                  1, 1 },
	{ "pclose",                    1, 0 },
	{ "realpath",                  1, 0 },
	{ "rewind",                    1, 0 },
	{ "sha1_file",                 1, 1 },
	{ "stat",                      1, 0 },
	/* Date family */
	{ "date",                      1, 1 },
	{ "date_default_timezone_set", 1, 1 },
	{ "gmdate",                    1, 1 },
	{ "gmmktime",                  1, 1 },
	{ "idate",                     1, 1 },
	{ "mktime",                    1, 1 },
	/* Encoding/URL family */
	{ "base64_decode",             1, 1 },
	{ "base64_encode",             1, 0 },
	{ "convert_uudecode",          1, 0 },
	{ "convert_uuencode",          1, 0 },
	{ "parse_ini_file",            1, 1 },
	{ "parse_ini_string",          1, 1 },
	{ "parse_url",                 1, 1 },
	{ "rawurldecode",              1, 0 },
	{ "rawurlencode",              1, 0 },
	{ "urldecode",                 1, 0 },
	{ "urlencode",                 1, 0 },
	/* JSON/serialize family */
	{ "filter_var",                1, 1 },
	{ "json_decode",               1, 1 },
	{ "json_encode",               1, 1 },
	{ "json_validate",             1, 1 },
	{ "serialize",                 1, 0 },
	{ "unserialize",               1, 1 },
	/* PCRE family */
	{ "preg_match",                2, 1 },
	{ "preg_match_all",            2, 1 },
	{ "preg_quote",                1, 1 },
	{ "preg_replace",              3, 1 },
	{ "preg_replace_callback",     3, 1 },
	{ "preg_split",                2, 1 },
	/* XML family */
	/* Constants/misc family */
	{ "call_user_func",            1, 1 },
	{ "call_user_func_array",      2, 0 },
	{ "constant",                  1, 0 },
	{ "define",                    2, 1 },
	{ "defined",                   1, 0 },
	{ "error_log",                 1, 1 },
	{ "fnmatch",                   2, 1 },
	{ "forward_static_call",       1, 1 },
	{ "forward_static_call_array", 2, 0 },
	{ "func_get_arg",              1, 0 },
	{ "function_exists",           1, 0 },
	{ "header",                    1, 1 },
	{ "password_get_info",         1, 0 },
	{ "putenv",                    1, 0 },
	{ "register_shutdown_function", 1, 1 },
	{ "set_error_handler",         1, 1 },
	{ "set_exception_handler",     1, 0 },
	{ "setcookie",                 1, 1 },
	{ "setrawcookie",              1, 1 },
	{ "trigger_error",             1, 1 },
	{ "user_error",                1, 1 },
	/*
	 * Overrides for signatures that under-report their own minimum: the callback
	 * of these three hides inside the variadic tail ("array $array, ...$rest"),
	 * so the derivation reads 1 where php requires 2.
	 */
	{ "array_udiff",               2, 1 },
	{ "array_uintersect",          2, 1 },
	{ "array_diff_uassoc",         2, 1 },
};
/*
 * Stamp the minimum-arity metadata from aBuiltinArity[] onto the already
 * registered host functions. Called once at VM init after every builtin family
 * has been installed into hHostFunction. A name absent from the hash (e.g. a
 * build without a given extension) is simply skipped.
 */
PH7_PRIVATE void VmSetBuiltinArity(ph7_vm *pVm)
{
	sxu32 n;
	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinArity) ; ++n ){
		const struct VmBuiltinArity *p = &aBuiltinArity[n];
		SyHashEntry *pEntry = SyHashGet(&pVm->hHostFunction,
			(const void *)p->zName,SyStrlen(p->zName));
		if( pEntry ){
			ph7_user_func *pFunc = (ph7_user_func *)pEntry->pUserData;
			pFunc->nMinArg  = p->nMin;
			pFunc->bAtLeast = p->bAtLeast;
		}
	}
}
/*
 * PHP 8.5 parameter signatures for the C builtins, generated offline from
 * a real PHP 8.5 ReflectionFunction dump over PHL's registered function
 * list (see the plan's Reflection section). "= ?" marks an optional
 * parameter whose default is not representable as a short literal.
 * Reflection parses these strings on demand; unlisted builtins degrade to
 * the min-arity data.
 */
static const struct VmBuiltinSig {
	const char *zName;
	const char *zSig;
	const char *zRet;
} aBuiltinSig[] = {
	/* The subsystems converted from embedded PHP into C (INI, libxml, sessions).
	 * A prelude function declared its parameters in PHP and Reflection read them
	 * from there; a C builtin has no declaration but this table, so without a row
	 * here the same function reports NO parameters -- and loses its arity bounds
	 * with them. */
	{ "get_cfg_var", "string $option", "array|string|false" },
	{ "ini_get", "string $option", "string|false" },
	{ "ini_get_all", "?string $extension = null, bool $details = true", "array|false" },
	{ "ini_restore", "string $option", "void" },
	{ "ini_set", "string $option, string|int|float|bool|null $value", "string|false" },
	{ "libxml_clear_errors", "", "void" },
	{ "libxml_get_errors", "", "array" },
	{ "libxml_get_last_error", "", "LibXMLError|false" },
	{ "libxml_use_internal_errors", "?bool $use_errors = null", "bool" },
	{ "session_abort", "", "bool" },
	{ "session_commit", "", "bool" },
	{ "session_destroy", "", "bool" },
	{ "session_id", "?string $id = null", "string|false" },
	{ "session_name", "?string $name = null", "string|false" },
	{ "session_regenerate_id", "bool $delete_old_session = false", "bool" },
	{ "session_reset", "", "bool" },
	{ "session_save_path", "?string $path = null", "string|false" },
	{ "session_start", "array $options = []", "bool" },
	{ "session_status", "", "int" },
	{ "session_unset", "", "bool" },
	{ "session_write_close", "", "bool" },
	{ "abs", "int|float $num", "int|float" },
	{ "acos", "float $num", "float" },
	{ "addcslashes", "string $string, string $characters", "string" },
	{ "addslashes", "string $string", "string" },
	{ "array_all", "array $array, callable $callback", "bool" },
	{ "array_any", "array $array, callable $callback", "bool" },
	{ "array_chunk", "array $array, int $length, bool $preserve_keys = false", "array" },
	{ "array_column", "array $array, string|int|null $column_key, string|int|null $index_key = NULL", "array" },
	{ "array_combine", "array $keys, array $values", "array" },
	{ "array_diff", "array $array, array ...$arrays = ?", "array" },
	{ "array_diff_assoc", "array $array, array ...$arrays = ?", "array" },
	{ "array_diff_key", "array $array, array ...$arrays = ?", "array" },
	{ "array_diff_uassoc", "array $array, ...$rest = ?", "array" },
	{ "array_fill", "int $start_index, int $count, mixed $value", "array" },
	{ "array_fill_keys", "array $keys, mixed $value", "array" },
	{ "array_filter", "array $array, ?callable $callback = NULL, int $mode = 0", "array" },
	{ "array_find", "array $array, callable $callback", "mixed" },
	{ "array_find_key", "array $array, callable $callback", "mixed" },
	{ "array_first", "array $array", "mixed" },
	{ "array_flip", "array $array", "array" },
	{ "array_intersect", "array $array, array ...$arrays = ?", "array" },
	{ "array_intersect_assoc", "array $array, array ...$arrays = ?", "array" },
	{ "array_intersect_key", "array $array, array ...$arrays = ?", "array" },
	{ "array_is_list", "array $array", "bool" },
	{ "array_key_exists", "$key, array $array", "bool" },
	{ "array_key_first", "array $array", "string|int|null" },
	{ "array_key_last", "array $array", "string|int|null" },
	{ "array_keys", "array $array, mixed $filter_value = ?, bool $strict = false", "array" },
	{ "array_last", "array $array", "mixed" },
	{ "array_map", "?callable $callback, array $array, array ...$arrays = ?", "array" },
	{ "array_merge", "array ...$arrays = ?", "array" },
	{ "array_pad", "array $array, int $length, mixed $value", "array" },
	{ "array_pop", "array &$array", "mixed" },
	{ "array_product", "array $array", "int|float" },
	{ "array_push", "array &$array, mixed ...$values = ?", "int" },
	{ "array_rand", "array $array, int $num = 1", "array|string|int" },
	{ "array_reduce", "array $array, callable $callback, mixed $initial = NULL", "mixed" },
	{ "array_replace", "array $array, array ...$replacements = ?", "array" },
	{ "array_reverse", "array $array, bool $preserve_keys = false", "array" },
	{ "array_search", "mixed $needle, array $haystack, bool $strict = false", "string|int|false" },
	{ "array_shift", "array &$array", "mixed" },
	{ "array_slice", "array $array, int $offset, ?int $length = NULL, bool $preserve_keys = false", "array" },
	{ "array_splice", "array &$array, int $offset, ?int $length = NULL, mixed $replacement = ?", "array" },
	{ "array_sum", "array $array", "int|float" },
	{ "array_udiff", "array $array, ...$rest = ?", "array" },
	{ "array_uintersect", "array $array, ...$rest = ?", "array" },
	{ "array_unique", "array $array, int $flags = 2", "array" },
	{ "array_values", "array $array", "array" },
	{ "array_walk", "object|array &$array, callable $callback, mixed $arg = ?", "true" },
	{ "array_walk_recursive", "object|array &$array, callable $callback, mixed $arg = ?", "true" },
	{ "arsort", "array &$array, int $flags = 0", "true" },
	{ "asin", "float $num", "float" },
	{ "asort", "array &$array, int $flags = 0", "true" },
	{ "assert", "mixed $assertion, Throwable|string|null $description = NULL", "bool" },
	{ "atan", "float $num", "float" },
	{ "atan2", "float $y, float $x", "float" },
	{ "base64_decode", "string $string, bool $strict = false", "string|false" },
	{ "base64_encode", "string $string", "string" },
	{ "base_convert", "string $num, int $from_base, int $to_base", "string" },
	{ "basename", "string $path, string $suffix = ''", "string" },
	{ "bin2hex", "string $string", "string" },
	{ "bindec", "string $binary_string", "int|float" },
	{ "boolval", "mixed $value", "bool" },
	{ "call_user_func", "callable $callback, mixed ...$args = ?", "mixed" },
	{ "call_user_func_array", "callable $callback, array $args", "mixed" },
	{ "ceil", "int|float $num", "float" },
	{ "chdir", "string $directory", "bool" },
	{ "chgrp", "string $filename, string|int $group", "bool" },
	{ "chmod", "string $filename, int $permissions", "bool" },
	{ "chop", "string $string, string $characters = ?", "string" },
	{ "chown", "string $filename, string|int $user", "bool" },
	{ "chr", "int $codepoint", "string" },
	{ "chunk_split", "string $string, int $length = 76, string $separator = ?", "string" },
	{ "class_alias", "string $class, string $alias, bool $autoload = true", "bool" },
	{ "class_exists", "string $class, bool $autoload = true", "bool" },
	{ "enum_exists", "string $enum, bool $autoload = true", "bool" },
	{ "closedir", "$dir_handle = NULL", "void" },
	{ "compact", "$var_name, ...$var_names = ?", "array" },
	{ "constant", "string $name", "mixed" },
	{ "convert_uudecode", "string $string", "string|false" },
	{ "convert_uuencode", "string $string", "string" },
	{ "copy", "string $from, string $to, $context = NULL", "bool" },
	{ "cos", "float $num", "float" },
	{ "cosh", "float $num", "float" },
	{ "count", "Countable|array $value, int $mode = 0", "int" },
	{ "crc32", "string $string", "int" },
	{ "ctype_alnum", "mixed $text", "bool" },
	{ "ctype_alpha", "mixed $text", "bool" },
	{ "ctype_cntrl", "mixed $text", "bool" },
	{ "ctype_digit", "mixed $text", "bool" },
	{ "ctype_graph", "mixed $text", "bool" },
	{ "ctype_lower", "mixed $text", "bool" },
	{ "ctype_print", "mixed $text", "bool" },
	{ "ctype_punct", "mixed $text", "bool" },
	{ "ctype_space", "mixed $text", "bool" },
	{ "ctype_upper", "mixed $text", "bool" },
	{ "ctype_xdigit", "mixed $text", "bool" },
	{ "current", "object|array $array", "mixed" },
	{ "date", "string $format, ?int $timestamp = NULL", "string" },
	{ "date_add", "DateTime $object, DateInterval $interval", "DateTime" },
	{ "date_create", "string $datetime = 'now', ?DateTimeZone $timezone = NULL", "DateTime|false" },
	{ "date_create_from_format", "string $format, string $datetime, ?DateTimeZone $timezone = NULL", "DateTime|false" },
	{ "date_create_immutable", "string $datetime = 'now', ?DateTimeZone $timezone = NULL", "DateTimeImmutable|false" },
	{ "date_create_immutable_from_format", "string $format, string $datetime, ?DateTimeZone $timezone = NULL", "DateTimeImmutable|false" },
	{ "date_date_set", "DateTime $object, int $year, int $month, int $day", "DateTime" },
	{ "date_diff", "DateTimeInterface $baseObject, DateTimeInterface $targetObject, bool $absolute = false", "DateInterval" },
	{ "date_format", "DateTimeInterface $object, string $format", "string" },
	{ "date_get_last_errors", "", "array|false" },
	{ "date_interval_create_from_date_string", "string $datetime", "DateInterval|false" },
	{ "date_interval_format", "DateInterval $object, string $format", "string" },
	{ "date_isodate_set", "DateTime $object, int $year, int $week, int $dayOfWeek = 1", "DateTime" },
	{ "date_modify", "DateTime $object, string $modifier", "DateTime|false" },
	{ "date_offset_get", "DateTimeInterface $object", "int" },
	{ "date_sub", "DateTime $object, DateInterval $interval", "DateTime" },
	{ "date_time_set", "DateTime $object, int $hour, int $minute, int $second = 0, int $microsecond = 0", "DateTime" },
	{ "date_timestamp_get", "DateTimeInterface $object", "int" },
	{ "date_timestamp_set", "DateTime $object, int $timestamp", "DateTime" },
	{ "date_timezone_get", "DateTimeInterface $object", "DateTimeZone|false" },
	{ "date_timezone_set", "DateTime $object, DateTimeZone $timezone", "DateTime" },
	{ "timezone_name_get", "DateTimeZone $object", "string" },
	{ "timezone_offset_get", "DateTimeZone $object, DateTimeInterface $datetime", "int" },
	{ "timezone_open", "string $timezone", "DateTimeZone|false" },
	{ "date_default_timezone_get", "", "string" },
	{ "date_default_timezone_set", "string $timezoneId", "bool" },
	{ "debug_backtrace", "int $options = 1, int $limit = 0", "array" },
	{ "debug_print_backtrace", "int $options = 0, int $limit = 0", "void" },
	{ "decbin", "int $num", "string" },
	{ "dechex", "int $num", "string" },
	{ "decoct", "int $num", "string" },
	{ "define", "string $constant_name, mixed $value, bool $case_insensitive = false", "bool" },
	{ "defined", "string $constant_name", "bool" },
	{ "die", "string|int $status = 0", "never" },
	{ "dirname", "string $path, int $levels = 1", "string" },
	{ "disk_free_space", "string $directory", "float|false" },
	{ "disk_total_space", "string $directory", "float|false" },
	{ "diskfreespace", "string $directory", "float|false" },
	{ "end", "object|array &$array", "mixed" },
	{ "error_get_last", "", "?array" },
	{ "error_clear_last", "", "void" },
	{ "error_log", "string $message, int $message_type = 0, ?string $destination = NULL, ?string $additional_headers = NULL", "bool" },
	{ "error_reporting", "?int $error_level = NULL", "int" },
	{ "exit", "string|int $status = 0", "never" },
	{ "exp", "float $num", "float" },
	{ "explode", "string $separator, string $string, int $limit = 9223372036854775807", "array" },
	{ "extract", "array &$array, int $flags = 0, string $prefix = ''", "int" },
	{ "fclose", "$stream", "bool" },
	{ "feof", "$stream", "bool" },
	{ "fflush", "$stream", "bool" },
	{ "fgetc", "$stream", "string|false" },
	{ "fgetcsv", "$stream, ?int $length = NULL, string $separator = ',', string $enclosure = '\"', string $escape = '\\\\'", "array|false" },
	{ "fgets", "$stream, ?int $length = NULL", "string|false" },
	{ "file", "string $filename, int $flags = 0, $context = NULL", "array|false" },
	{ "file_exists", "string $filename", "bool" },
	{ "file_get_contents", "string $filename, bool $use_include_path = false, $context = NULL, int $offset = 0, ?int $length = NULL", "string|false" },
	{ "file_put_contents", "string $filename, mixed $data, int $flags = 0, $context = NULL", "int|false" },
	{ "fileatime", "string $filename", "int|false" },
	{ "filectime", "string $filename", "int|false" },
	{ "filemtime", "string $filename", "int|false" },
	{ "filesize", "string $filename", "int|false" },
	{ "filetype", "string $filename", "string|false" },
	{ "filter_input", "int $type, string $var_name, int $filter = 516, array|int $options = 0", "mixed" },
	{ "filter_var", "mixed $value, int $filter = 516, array|int $options = 0", "mixed" },
	{ "floatval", "mixed $value", "float" },
	{ "flock", "$stream, int $operation, &$would_block = NULL", "bool" },
	{ "floor", "int|float $num", "float" },
	{ "flush", "", "void" },
	{ "fmod", "float $num1, float $num2", "float" },
	{ "fnmatch", "string $pattern, string $filename, int $flags = 0", "bool" },
	{ "fopen", "string $filename, string $mode, bool $use_include_path = false, $context = NULL", "" },
	{ "forward_static_call", "callable $callback, mixed ...$args = ?", "mixed" },
	{ "forward_static_call_array", "callable $callback, array $args", "mixed" },
	{ "fpassthru", "$stream", "int" },
	{ "fprintf", "$stream, string $format, mixed ...$values = ?", "int" },
	{ "fputcsv", "$stream, array $fields, string $separator = ',', string $enclosure = '\"', string $escape = '\\\\', string $eol = ?", "int|false" },
	{ "fputs", "$stream, string $data, ?int $length = NULL", "int|false" },
	{ "fread", "$stream, int $length", "string|false" },
	{ "fseek", "$stream, int $offset, int $whence = 0", "int" },
	{ "fstat", "$stream", "array|false" },
	{ "ftell", "$stream", "int|false" },
	{ "ftruncate", "$stream, int $size", "bool" },
	{ "func_get_arg", "int $position", "mixed" },
	{ "func_get_args", "", "array" },
	{ "func_num_args", "", "int" },
	{ "function_exists", "string $function", "bool" },
	{ "fwrite", "$stream, string $data, ?int $length = NULL", "int|false" },
	{ "gc_collect_cycles", "", "int" },
	{ "gc_disable", "", "void" },
	{ "gc_enable", "", "void" },
	{ "gc_enabled", "", "bool" },
	{ "gc_mem_caches", "", "int" },
	{ "gc_status", "", "array" },
	{ "get_called_class", "", "string" },
	{ "get_class", "object $object = ?", "string" },
	{ "get_class_methods", "object|string $object_or_class", "array" },
	{ "get_class_vars", "string $class", "array" },
	{ "get_current_user", "", "string" },
	{ "get_declared_classes", "", "array" },
	{ "get_declared_interfaces", "", "array" },
	{ "get_defined_constants", "bool $categorize = false", "array" },
	{ "get_defined_functions", "bool $exclude_disabled = true", "array" },
	{ "get_defined_vars", "", "array" },
	{ "get_html_translation_table", "int $table = 0, int $flags = 11, string $encoding = 'UTF-8'", "array" },
	{ "get_include_path", "", "string|false" },
	{ "get_included_files", "", "array" },
	{ "get_object_vars", "object $object", "array" },
	{ "get_parent_class", "object|string $object_or_class = ?", "string|false" },
	{ "get_resource_id", "$resource", "int" },
	{ "get_resource_type", "$resource", "string" },
	{ "getcwd", "", "string|false" },
	{ "getdate", "?int $timestamp = NULL", "array" },
	{ "getenv", "?string $name = NULL, bool $local_only = false", "array|string|false" },
	{ "getmygid", "", "int|false" },
	{ "getmypid", "", "int|false" },
	{ "getmyuid", "", "int|false" },
	{ "getopt", "string $short_options, array $long_options = ?, &$rest_index = NULL", "array|false" },
	{ "getrandmax", "", "int" },
	{ "gettimeofday", "bool $as_float = false", "array|float" },
	{ "gettype", "mixed $value", "string" },
	{ "gmdate", "string $format, ?int $timestamp = NULL", "string" },
	{ "gmmktime", "int $hour, ?int $minute = NULL, ?int $second = NULL, ?int $month = NULL, ?int $day = NULL, ?int $year = NULL", "int|false" },
	{ "hash", "string $algo, string $data, bool $binary = false, array $options = ?", "string" },
	{ "hash_algos", "", "array" },
	{ "hash_equals", "string $known_string, string $user_string", "bool" },
	{ "hash_hmac", "string $algo, string $data, string $key, bool $binary = false", "string" },
	{ "header", "string $header, bool $replace = true, int $response_code = 0", "void" },
	{ "header_remove", "?string $name = NULL", "void" },
	{ "headers_list", "", "array" },
	{ "headers_sent", "&$filename = NULL, &$line = NULL", "bool" },
	{ "hexdec", "string $hex_string", "int|float" },
	{ "html_entity_decode", "string $string, int $flags = 11, ?string $encoding = NULL", "string" },
	{ "htmlentities", "string $string, int $flags = 11, ?string $encoding = NULL, bool $double_encode = true", "string" },
	{ "htmlspecialchars", "string $string, int $flags = 11, ?string $encoding = NULL, bool $double_encode = true", "string" },
	{ "htmlspecialchars_decode", "string $string, int $flags = 11", "string" },
	{ "http_response_code", "int $response_code = 0", "int|bool" },
	{ "hypot", "float $x, float $y", "float" },
	{ "idate", "string $format, ?int $timestamp = NULL", "int|false" },
	{ "implode", "array|string $separator, ?array $array = NULL", "string" },
	{ "in_array", "mixed $needle, array $haystack, bool $strict = false", "bool" },
	{ "intdiv", "int $num1, int $num2", "int" },
	{ "interface_exists", "string $interface, bool $autoload = true", "bool" },
	{ "trait_exists", "string $trait, bool $autoload = true", "bool" },
	{ "intval", "mixed $value, int $base = 10", "int" },
	{ "is_a", "mixed $object_or_class, string $class, bool $allow_string = false", "bool" },
	{ "is_array", "mixed $value", "bool" },
	{ "is_bool", "mixed $value", "bool" },
	{ "is_callable", "mixed $value, bool $syntax_only = false, &$callable_name = NULL", "bool" },
	{ "is_dir", "string $filename", "bool" },
	{ "is_double", "mixed $value", "bool" },
	{ "is_executable", "string $filename", "bool" },
	{ "is_file", "string $filename", "bool" },
	{ "is_float", "mixed $value", "bool" },
	{ "is_int", "mixed $value", "bool" },
	{ "is_integer", "mixed $value", "bool" },
	{ "is_link", "string $filename", "bool" },
	{ "is_long", "mixed $value", "bool" },
	{ "is_null", "mixed $value", "bool" },
	{ "is_numeric", "mixed $value", "bool" },
	{ "is_object", "mixed $value", "bool" },
	{ "is_readable", "string $filename", "bool" },
	{ "is_resource", "mixed $value", "bool" },
	{ "is_scalar", "mixed $value", "bool" },
	{ "is_string", "mixed $value", "bool" },
	{ "is_subclass_of", "mixed $object_or_class, string $class, bool $allow_string = true", "bool" },
	{ "is_writable", "string $filename", "bool" },
	{ "iterator_apply", "Traversable $iterator, callable $callback, ?array $args = NULL", "int" },
	{ "iterator_count", "Traversable|array $iterator", "int" },
	{ "iterator_to_array", "Traversable|array $iterator, bool $preserve_keys = true", "array" },
	{ "join", "array|string $separator, ?array $array = NULL", "string" },
	{ "json_decode", "string $json, ?bool $associative = NULL, int $depth = 512, int $flags = 0", "mixed" },
	{ "json_encode", "mixed $value, int $flags = 0, int $depth = 512", "string|false" },
	{ "json_last_error", "", "int" },
	{ "json_last_error_msg", "", "string" },
	{ "json_validate", "string $json, int $depth = 512, int $flags = 0", "bool" },
	{ "key", "object|array $array", "string|int|null" },
	{ "key_exists", "$key, array $array", "bool" },
	{ "krsort", "array &$array, int $flags = 0", "true" },
	{ "ksort", "array &$array, int $flags = 0", "true" },
	{ "lcfirst", "string $string", "string" },
	{ "levenshtein", "string $string1, string $string2, int $insertion_cost = 1, int $replacement_cost = 1, int $deletion_cost = 1", "int" },
	{ "link", "string $target, string $link", "bool" },
	{ "localtime", "?int $timestamp = NULL, bool $associative = false", "array" },
	{ "log", "float $num, float $base = 2.718281828459045", "float" },
	{ "log10", "float $num", "float" },
	{ "lstat", "string $filename", "array|false" },
	{ "ltrim", "string $string, string $characters = ?", "string" },
	{ "max", "mixed $value, mixed ...$values = ?", "mixed" },
	{ "mb_chr", "int $codepoint, ?string $encoding = NULL", "string|false" },
	{ "mb_convert_encoding", "array|string $string, string $to_encoding, array|string|null $from_encoding = NULL", "array|string" },
	{ "mb_ord", "string $string, ?string $encoding = NULL", "int|false" },
	{ "mb_strtolower", "string $string, ?string $encoding = NULL", "string" },
	{ "mb_strtoupper", "string $string, ?string $encoding = NULL", "string" },
	{ "md5", "string $string, bool $binary = false", "string" },
	{ "md5_file", "string $filename, bool $binary = false", "string|false" },
	{ "method_exists", "$object_or_class, string $method", "bool" },
	{ "memory_get_peak_usage", "bool $real_usage = false", "int" },
	{ "memory_get_usage", "bool $real_usage = false", "int" },
	{ "microtime", "bool $as_float = false", "string|float" },
	{ "min", "mixed $value, mixed ...$values = ?", "mixed" },
	{ "mkdir", "string $directory, int $permissions = 511, bool $recursive = false, $context = NULL", "bool" },
	{ "mktime", "int $hour, ?int $minute = NULL, ?int $second = NULL, ?int $month = NULL, ?int $day = NULL, ?int $year = NULL", "int|false" },
	{ "mt_getrandmax", "", "int" },
	{ "mt_rand", "int $min = ?, int $max = ?", "int" },
	{ "mt_srand", "?int $seed = NULL, int $mode = 0", "void" },
	{ "natcasesort", "array &$array", "true" },
	{ "natsort", "array &$array", "true" },
	{ "next", "object|array &$array", "mixed" },
	{ "nl2br", "string $string, bool $use_xhtml = true", "string" },
	{ "ob_clean", "", "bool" },
	{ "ob_end_clean", "", "bool" },
	{ "ob_end_flush", "", "bool" },
	{ "ob_flush", "", "bool" },
	{ "ob_get_clean", "", "string|false" },
	{ "ob_get_contents", "", "string|false" },
	{ "ob_get_flush", "", "string|false" },
	{ "ob_get_length", "", "int|false" },
	{ "ob_get_level", "", "int" },
	{ "ob_implicit_flush", "bool $enable = true", "void" },
	{ "ob_list_handlers", "", "array" },
	{ "ob_start", "$callback = NULL, int $chunk_size = 0, int $flags = 112", "bool" },
	{ "octdec", "string $octal_string", "int|float" },
	{ "opendir", "string $directory, $context = NULL", "" },
	{ "ord", "string $character", "int" },
	{ "parse_ini_file", "string $filename, bool $process_sections = false, int $scanner_mode = 0", "array|false" },
	{ "parse_ini_string", "string $ini_string, bool $process_sections = false, int $scanner_mode = 0", "array|false" },
	{ "parse_url", "string $url, int $component = -1", "array|string|int|false|null" },
	{ "password_get_info", "string $hash", "array" },
	{ "password_hash", "string $password, string|int|null $algo, array $options = ?", "string" },
	{ "password_needs_rehash", "string $hash, string|int|null $algo, array $options = ?", "bool" },
	{ "password_verify", "string $password, string $hash", "bool" },
	{ "pathinfo", "string $path, int $flags = 15", "array|string" },
	{ "pclose", "$handle", "int" },
	{ "php_sapi_name", "", "string|false" },
	{ "php_uname", "string $mode = 'a'", "string" },
	{ "phpinfo", "int $flags = 4294967295", "true" },
	{ "phpversion", "?string $extension = NULL", "string|false" },
	{ "pi", "", "float" },
	{ "popen", "string $command, string $mode", "" },
	{ "pos", "object|array $array", "mixed" },
	{ "pow", "mixed $num, mixed $exponent", "object|int|float" },
	{ "preg_last_error", "", "int" },
	{ "preg_last_error_msg", "", "string" },
	{ "fsockopen", "string $hostname, int $port = -1, &$error_code = NULL, &$error_message = NULL, ?float $timeout = NULL", "resource|false" },
	{ "pfsockopen", "string $hostname, int $port = -1, &$error_code = NULL, &$error_message = NULL, ?float $timeout = NULL", "resource|false" },
	{ "stream_socket_client", "string $address, &$error_code = NULL, &$error_message = NULL, ?float $timeout = NULL, int $flags = 4, $context = NULL", "resource|false" },
	{ "preg_match", "string $pattern, string $subject, &$matches = NULL, int $flags = 0, int $offset = 0", "int|false" },
	{ "preg_match_all", "string $pattern, string $subject, &$matches = NULL, int $flags = 0, int $offset = 0", "int|false" },
	{ "preg_quote", "string $str, ?string $delimiter = NULL", "string" },
	{ "preg_replace", "array|string $pattern, array|string $replacement, array|string $subject, int $limit = -1, &$count = NULL", "array|string|null" },
	{ "preg_replace_callback", "array|string $pattern, callable $callback, array|string $subject, int $limit = -1, &$count = NULL, int $flags = 0", "array|string|null" },
	{ "preg_split", "string $pattern, string $subject, int $limit = -1, int $flags = 0", "array|false" },
	{ "prev", "object|array &$array", "mixed" },
	{ "print_r", "mixed $value, bool $return = false", "string|true" },
	{ "printf", "string $format, mixed ...$values = ?", "int" },
	{ "property_exists", "$object_or_class, string $property", "bool" },
	{ "putenv", "string $assignment", "bool" },
	{ "quotemeta", "string $string", "string" },
	{ "rand", "int $min = ?, int $max = ?", "int" },
	{ "random_bytes", "int $length", "string" },
	{ "random_int", "int $min, int $max", "int" },
	{ "range", "string|int|float $start, string|int|float $end, int|float $step = 1", "array" },
	{ "rawurldecode", "string $string", "string" },
	{ "rawurlencode", "string $string", "string" },
	{ "readdir", "$dir_handle = NULL", "string|false" },
	{ "readfile", "string $filename, bool $use_include_path = false, $context = NULL", "int|false" },
	{ "realpath", "string $path", "string|false" },
	{ "register_shutdown_function", "callable $callback, mixed ...$args = ?", "void" },
	{ "rename", "string $from, string $to, $context = NULL", "bool" },
	{ "reset", "object|array &$array", "mixed" },
	{ "restore_error_handler", "", "true" },
	{ "restore_exception_handler", "", "true" },
	{ "rewind", "$stream", "bool" },
	{ "rewinddir", "$dir_handle = NULL", "void" },
	{ "rmdir", "string $directory, $context = NULL", "bool" },
	{ "round", "int|float $num, int $precision = 0, RoundingMode|int $mode = ?", "float" },
	{ "rsort", "array &$array, int $flags = 0", "true" },
	{ "rtrim", "string $string, string $characters = ?", "string" },
	{ "serialize", "mixed $value", "string" },
	{ "set_error_handler", "?callable $callback, int $error_levels = 30719", "" },
	{ "set_exception_handler", "?callable $callback", "" },
	{ "get_error_handler", "", "?callable" },
	{ "get_exception_handler", "", "?callable" },
	{ "hrtime", "bool $as_number = false", "array|int" },
	{ "setcookie", "string $name, string $value = '', array|int $expires_or_options = 0, string $path = '', string $domain = '', bool $secure = false, bool $httponly = false", "bool" },
	{ "setrawcookie", "string $name, string $value = '', array|int $expires_or_options = 0, string $path = '', string $domain = '', bool $secure = false, bool $httponly = false", "bool" },
	{ "settype", "mixed &$var, string $type", "bool" },
	{ "sha1", "string $string, bool $binary = false", "string" },
	{ "sha1_file", "string $filename, bool $binary = false", "string|false" },
	{ "shuffle", "array &$array", "true" },
	{ "similar_text", "string $string1, string $string2, &$percent = NULL", "int" },
	{ "sin", "float $num", "float" },
	{ "sinh", "float $num", "float" },
	{ "sizeof", "Countable|array $value, int $mode = 0", "int" },
	{ "sleep", "int $seconds", "int" },
	{ "sort", "array &$array, int $flags = 0", "true" },
	{ "soundex", "string $string", "string" },
	{ "spl_autoload", "string $class, ?string $file_extensions = NULL", "void" },
	{ "spl_autoload_functions", "", "array" },
	{ "spl_autoload_register", "?callable $callback = NULL, bool $throw = true, bool $prepend = false", "bool" },
	{ "spl_autoload_unregister", "callable $callback", "bool" },
	{ "spl_object_hash", "object $object", "string" },
	{ "spl_object_id", "object $object", "int" },
	{ "sprintf", "string $format, mixed ...$values = ?", "string" },
	{ "sqrt", "float $num", "float" },
	{ "srand", "?int $seed = NULL, int $mode = 0", "void" },
	{ "stat", "string $filename", "array|false" },
	{ "str_contains", "string $haystack, string $needle", "bool" },
	{ "str_ends_with", "string $haystack, string $needle", "bool" },
	{ "str_getcsv", "string $string, string $separator = ',', string $enclosure = '\"', string $escape = '\\\\'", "array" },
	{ "str_ireplace", "array|string $search, array|string $replace, array|string $subject, &$count = NULL", "array|string" },
	{ "str_pad", "string $string, int $length, string $pad_string = ' ', int $pad_type = 1", "string" },
	{ "str_repeat", "string $string, int $times", "string" },
	{ "str_replace", "array|string $search, array|string $replace, array|string $subject, &$count = NULL", "array|string" },
	{ "str_shuffle", "string $string", "string" },
	{ "str_split", "string $string, int $length = 1", "array" },
	{ "str_starts_with", "string $haystack, string $needle", "bool" },
	{ "str_word_count", "string $string, int $format = 0, ?string $characters = NULL", "array|int" },
	{ "strcasecmp", "string $string1, string $string2", "int" },
	{ "strchr", "string $haystack, string $needle, bool $before_needle = false", "string|false" },
	{ "strcmp", "string $string1, string $string2", "int" },
	{ "strnatcasecmp", "string $string1, string $string2", "int" },
	{ "strnatcmp", "string $string1, string $string2", "int" },
	{ "strcoll", "string $string1, string $string2", "int" },
	{ "strcspn", "string $string, string $characters, int $offset = 0, ?int $length = NULL", "int" },
	{ "strip_tags", "string $string, array|string|null $allowed_tags = NULL", "string" },
	{ "stripos", "string $haystack, string $needle, int $offset = 0", "int|false" },
	{ "stripslashes", "string $string", "string" },
	{ "stristr", "string $haystack, string $needle, bool $before_needle = false", "string|false" },
	{ "strlen", "string $string", "int" },
	{ "strncasecmp", "string $string1, string $string2, int $length", "int" },
	{ "strncmp", "string $string1, string $string2, int $length", "int" },
	{ "strpbrk", "string $string, string $characters", "string|false" },
	{ "strpos", "string $haystack, string $needle, int $offset = 0", "int|false" },
	{ "strrchr", "string $haystack, string $needle, bool $before_needle = false", "string|false" },
	{ "strrev", "string $string", "string" },
	{ "strripos", "string $haystack, string $needle, int $offset = 0", "int|false" },
	{ "strrpos", "string $haystack, string $needle, int $offset = 0", "int|false" },
	{ "strspn", "string $string, string $characters, int $offset = 0, ?int $length = NULL", "int" },
	{ "strstr", "string $haystack, string $needle, bool $before_needle = false", "string|false" },
	{ "strtok", "string $string, ?string $token = NULL", "string|false" },
	{ "strtolower", "string $string", "string" },
	{ "strtotime", "string $datetime, ?int $baseTimestamp = NULL", "int|false" },
	{ "strtoupper", "string $string", "string" },
	{ "strtr", "string $string, array|string $from, ?string $to = NULL", "string" },
	{ "strval", "mixed $value", "string" },
	{ "substr", "string $string, int $offset, ?int $length = NULL", "string" },
	{ "substr_compare", "string $haystack, string $needle, int $offset, ?int $length = NULL, bool $case_insensitive = false", "int" },
	{ "substr_count", "string $haystack, string $needle, int $offset = 0, ?int $length = NULL", "int" },
	{ "substr_replace", "array|string $string, array|string $replace, array|int $offset, array|int|null $length = NULL", "array|string" },
	{ "symlink", "string $target, string $link", "bool" },
	{ "sys_get_temp_dir", "", "string" },
	{ "tan", "float $num", "float" },
	{ "tanh", "float $num", "float" },
	{ "time", "", "int" },
	{ "touch", "string $filename, ?int $mtime = NULL, ?int $atime = NULL", "bool" },
	{ "trigger_error", "string $message, int $error_level = 1024", "true" },
	{ "trim", "string $string, string $characters = ?", "string" },
	{ "uasort", "array &$array, callable $callback", "true" },
	{ "ucfirst", "string $string", "string" },
	{ "ucwords", "string $string, string $separators = ?", "string" },
	{ "uksort", "array &$array, callable $callback", "true" },
	{ "umask", "?int $mask = NULL", "int" },
	{ "uniqid", "string $prefix = '', bool $more_entropy = false", "string" },
	{ "unlink", "string $filename, $context = NULL", "bool" },
	{ "unserialize", "string $data, array $options = ?", "mixed" },
	{ "urldecode", "string $string", "string" },
	{ "urlencode", "string $string", "string" },
	{ "user_error", "string $message, int $error_level = 1024", "true" },
	{ "usleep", "int $microseconds", "void" },
	{ "usort", "array &$array, callable $callback", "true" },
	{ "var_dump", "mixed $value, mixed ...$values = ?", "void" },
	{ "var_export", "mixed $value, bool $return = false", "?string" },
	{ "vfprintf", "$stream, string $format, array $values", "int" },
	{ "vprintf", "string $format, array $values", "int" },
	{ "vsprintf", "string $format, array $values", "string" },
	{ "wordwrap", "string $string, int $width = 75, string $break = ?, bool $cut_long_words = false", "string" },
	{ "zip_close", "$zip", "void" },
	{ "zip_entry_close", "$zip_entry", "bool" },
	{ "zip_entry_compressedsize", "$zip_entry", "int|false" },
	{ "zip_entry_compressionmethod", "$zip_entry", "string|false" },
	{ "zip_entry_filesize", "$zip_entry", "int|false" },
	{ "zip_entry_name", "$zip_entry", "string|false" },
	{ "zip_entry_open", "$zip_dp, $zip_entry, string $mode = 'rb'", "bool" },
	{ "zip_entry_read", "$zip_entry, int $len = 1024", "string|false" },
	{ "zip_open", "string $filename", "" },
	{ "zip_read", "$zip", "" },
};
/*
 * Stamp the signature strings onto the registered host functions.
 * Runs once at PH7_VmMakeReady, after the builtins are registered.
 */
/*
 * Derive the minimum arity from a builtin's declared signature: a parameter is
 * optional when it carries a default ("int $length = 76") or is variadic
 * ("...$rest"), so the minimum is the count of the parameters that are neither,
 * and the ZPP wording is "at least" as soon as one optional/variadic exists.
 *
 * This makes aBuiltinSig[] the single source of truth for arity — the curated
 * aBuiltinArity[] table above stays as an OVERRIDE for the handful of builtins
 * php reports differently from their own signature (e.g. strtr(), whose 3-arg
 * form still reports "expects exactly 2", and the array_udiff() family, whose
 * variadic tail hides a second required argument). Verified against php 8.5.7
 * for all 462 signed builtins: 458 derive exactly, 4 are overridden.
 */
PH7_PRIVATE void VmDeriveArityFromSig(const char *zSig,sxi16 *pnMin,sxu8 *pbAtLeast,sxi16 *pnMax,sxu8 *pbHasMax)
{
	const char *zCur = zSig;
	int nMin = 0, bAtLeast = 0, bSeen = 0, bOptional = 0;
	int nTotal = 0, bVariadic = 0;
	for(;;){
		if( zCur[0] == '\0' || zCur[0] == ',' ){
			if( bSeen ){
				nTotal++;
				if( bOptional ){
					bAtLeast = 1;
				}else{
					nMin++;
				}
			}
			if( zCur[0] == '\0' ){
				break;
			}
			bSeen = bOptional = 0;
			zCur++;
			continue;
		}
		if( zCur[0] != ' ' ){
			bSeen = 1;
		}
		if( zCur[0] == '=' || (zCur[0] == '.' && zCur[1] == '.' && zCur[2] == '.') ){
			bOptional = 1;
		}
		if( zCur[0] == '.' && zCur[1] == '.' && zCur[2] == '.' ){
			bVariadic = 1;
		}
		zCur++;
	}
	*pnMin = (sxi16)nMin;
	*pbAtLeast = (sxu8)bAtLeast;
	/* php enforces a MAXIMUM too ("expects at most 1 argument, 2 given"); a
	 * variadic tail means there is none. The parameter COUNT is the maximum,
	 * whether or not the parameters carry defaults. */
	*pnMax = (sxi16)nTotal;
	*pbHasMax = (sxu8)(bVariadic ? 0 : 1);
}
/*
 * Does the declared type list (e.g. "array|string", "?int", "callable") contain
 * the given token? Compares against each '|'-separated alternative, ignoring a
 * leading nullable '?'.
 */
static int VmSigTypeHas(const char *zType,int nType,const char *zTok)
{
	int nTok = (int)SyStrlen(zTok);
	int i = 0;
	if( zType[0] == '?' ){
		zType++;
		nType--;
	}
	while( i < nType ){
		int j = i;
		while( j < nType && zType[j] != '|' ){
			j++;
		}
		if( j - i == nTok && SyMemcmp(&zType[i],zTok,(sxu32)nTok) == 0 ){
			return 1;
		}
		i = j + 1;
	}
	return 0;
}
/*
 * Does the declared type list name a CLASS (anything that is not one of php's
 * builtin type keywords)? A class-typed parameter accepts an object, so it must
 * not be rejected by the array/object/resource screen below.
 */
/* Is this one arm of a declared type a BUILTIN type name rather than a class? */
static int VmSigArmIsBuiltinType(const char *zArm,int nArm)
{
	static const char *azBuiltin[] = {
		"int","float","string","bool","array","object","callable","iterable",
		"mixed","null","void","resource","false","true","never","self","static"
	};
	int k;
	for( k = 0 ; k < (int)SX_ARRAYSIZE(azBuiltin) ; ++k ){
		int nB = (int)SyStrlen(azBuiltin[k]);
		if( nArm == nB && SyMemcmp(zArm,azBuiltin[k],(sxu32)nB) == 0 ){
			return 1;
		}
	}
	return 0;
}
static int VmSigTypeHasClass(const char *zType,int nType)
{
	int i = 0;
	if( zType[0] == '?' ){
		zType++;
		nType--;
	}
	while( i < nType ){
		int j = i;
		while( j < nType && zType[j] != '|' ){
			j++;
		}
		if( j > i && !VmSigArmIsBuiltinType(&zType[i],j - i) ){
			return 1;
		}
		i = j + 1;
	}
	return 0;
}
/*
 * php's ", X given" tail: like ph7_type_name() but an object reports its CLASS,
 * which is what php prints in a TypeError.
 */
/*
 * Does pObj satisfy any CLASS arm of a declared type?
 *
 * Answers TRUE (unscreened) when an arm names something this VM has not declared:
 * the signatures describe php's surface, parts of which PHL models differently
 * (the resource-backed handles the RES branch below already excuses), and a name
 * that resolves to nothing must not turn into a rejection of a valid argument.
 */
static int VmSigObjSatisfiesClass(ph7_vm *pVm,const char *zType,int nType,
	ph7_class_instance *pObj)
{
	int i = 0;
	if( pObj == 0 || pObj->pClass == 0 ){
		return 1;
	}
	if( zType[0] == '?' ){
		zType++;
		nType--;
	}
	while( i < nType ){
		int j = i;
		while( j < nType && zType[j] != '|' ){
			j++;
		}
		if( j > i && !VmSigArmIsBuiltinType(&zType[i],j - i) ){
			ph7_class *pClass = PH7_VmExtractClass(&(*pVm),&zType[i],(sxu32)(j - i),FALSE,0);
			if( pClass == 0 ){
				/* Either a builtin type name (already excluded by the caller) or a
				 * class this build does not declare: nothing to judge. */
				return 1;
			}
			if( PH7_VmInstanceOf(pObj->pClass,pClass) ){
				return 1;
			}
		}
		i = j + 1;
	}
	return 0;
}
static const char * VmArgTypeName(ph7_value *pVal)
{
	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 ){
		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;
		if( pInst && pInst->pClass ){
			return pInst->pClass->sName.zString;
		}
	}
	return ph7_type_name(pVal);
}
/*
 * Does pArg satisfy a `string` parameter? The rule VmEnforceBuiltinArgTypes()
 * below applies, factored out so a builtin that words its own overload dispatch
 * (strtr(), whose expected type depends on the ARITY and so cannot be spelled in
 * one signature) decides identically instead of forking the logic. An array never
 * satisfies one; an object does only through __toString(); a resource does not;
 * null does under php, with a deprecation, but not under PHL's §10 null-strictness
 * policy — the screen and this helper both report it as a mismatch.
 */
PH7_PRIVATE int PH7_ArgSatisfiesString(ph7_value *pArg)
{
	if( (pArg->iFlags & (MEMOBJ_HASHMAP|MEMOBJ_NULL|MEMOBJ_RES)) != 0 ){
		return 0;
	}
	if( (pArg->iFlags & MEMOBJ_OBJ) != 0 ){
		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;
		return pInst && PH7_ClassExtractMethod(pInst->pClass,"__toString",
			sizeof("__toString")-1) != 0;
	}
	return 1;
}
/*
 * PHP-8 ZPP type enforcement for host functions, driven by the aBuiltinSig[]
 * declaration (band A #7). Screens only the arguments that php can NEVER coerce
 * into a declared scalar parameter — arrays, resources, and objects without a
 * __toString() — and throws the catchable TypeError php throws, before the C
 * routine runs. Without this an array argument reached the builtin and was
 * stringified to the literal "Array" (strrev(['a']) returned "yarrA").
 *
 * Deliberately narrow: scalar-to-scalar coercion (and its deprecations) stays
 * with the per-builtin ZPP helpers. Those emit E_DEPRECATED, which does NOT
 * abort the call, so a central copy would double-fire — the trap that sank the
 * first central-ZPP attempt. A TypeError aborts, so there is nothing to double.
 */
PH7_PRIVATE sxi32 VmEnforceBuiltinArgTypes(
	ph7_context *pCtx,    /* Call context (for the throw) */
	ph7_user_func *pFunc, /* Callee */
	int nGiven,           /* Argument count */
	ph7_value **apArg     /* Arguments */
	)
{
	/*
	 * Builtins whose own argument check is php-exact and VALUE-based rather than
	 * type-based must not be pre-empted here, or their message is lost. Same rule
	 * the aBuiltinArity[] table follows: a builtin that already says what php says
	 * stays off the shared screen. get_class_vars() takes any stringifiable value
	 * and reports "must be a valid class name, Array given".
	 *
	 * strtr() is here for a structural reason: php declares it as two OVERLOADS
	 * dispatched on arity — strtr(string, array) and strtr(string, string, string)
	 * — so the expected type of $from is `array` with two arguments and `string`
	 * with three. One signature cannot say that (the stub's `array|string` is the
	 * union of the two, which is the wording php never uses), so the builtin does
	 * its own dispatch through PH7_ArgSatisfiesString(), the same rule as here.
	 *
	 * implode() is the same structure: `array|string $separator` is what the two
	 * ARITIES accept between them, never what one call can use. Once an $array
	 * argument is present php has resolved the overload and reports
	 * `must be of type string`, and with the array in position #1 it reports
	 * `must be of type string, array given` against #1 rather than a #2 error.
	 * PH7_builtin_implode words all of that itself.
	 *
	 * Its alias join() is here for the same reason and then some: php 8.5 does not
	 * word the two the same, so the builtin reproduces BOTH orders keyed on the
	 * invoked name (see PH7_builtin_implode's header for the value-for-value
	 * table against 8.5.8). php's own asymmetry between a target and its alias,
	 * reproduced rather than smoothed over — parity is binding (§10).
	 */
	static const char *azSelfChecked[] = { "get_class_vars", "strtr", "implode", "join" };
	const char *zSig = pFunc->zSig;
	const char *zCur, *zEnd;
	int iArg = 0;
	if( zSig == 0 ){
		return SXRET_OK;
	}
	for( iArg = 0 ; iArg < (int)SX_ARRAYSIZE(azSelfChecked) ; ++iArg ){
		if( SyStrncmp(pFunc->sName.zString,azSelfChecked[iArg],
			(sxu32)SyStrlen(azSelfChecked[iArg])) == 0
		 && pFunc->sName.nByte == SyStrlen(azSelfChecked[iArg]) ){
			return SXRET_OK;
		}
	}
	iArg = 0;
	zCur = zSig;
	zEnd = &zSig[SyStrlen(zSig)];
	while( zCur < zEnd && iArg < nGiven ){
		const char *zType, *zName, *zStop;
		int nType, nName;
		ph7_value *pArg;
		char zGivenBuf[64];
		/* Parameter = "<type> $<name>[ = <default>]"; the type is whatever
		 * precedes the '$', and an empty type means "untyped" (no screen). */
		while( zCur < zEnd && zCur[0] == ' ' ){
			zCur++;
		}
		zStop = zCur;
		while( zStop < zEnd && zStop[0] != ',' ){
			zStop++;
		}
		zName = zCur;
		while( zName < zStop && zName[0] != '$' ){
			zName++;
		}
		if( zName >= zStop ){
			break; /* malformed / no parameter name — stop screening */
		}
		if( zName >= zCur + 3 && SyMemcmp(zName - 3,"...",3) == 0 ){
			break; /* variadic tail: stop (its type applies to the rest) */
		}
		zType = zCur;
		nType = (int)(zName - zCur);
		/* Trim the trailing spaces and the by-ref marker of "array &$array" */
		while( nType > 0 && (zType[nType-1] == ' ' || zType[nType-1] == '&') ){
			nType--;
		}
		zName++; /* skip '$' */
		nName = 0;
		while( &zName[nName] < zStop && zName[nName] != ' ' && zName[nName] != '=' ){
			nName++;
		}
		pArg = apArg[iArg];
		if( nType > 0 && !VmSigTypeHas(zType,nType,"mixed") ){
			const char *zGiven = 0;
			if( (pArg->iFlags & MEMOBJ_HASHMAP) != 0 ){
				if( !VmSigTypeHas(zType,nType,"array")
				 && !VmSigTypeHas(zType,nType,"iterable")
				 && !VmSigTypeHas(zType,nType,"callable") ){
					zGiven = "array";
				}
			}else if( (pArg->iFlags & MEMOBJ_OBJ) != 0 ){
				if( !VmSigTypeHas(zType,nType,"object")
				 && !VmSigTypeHas(zType,nType,"iterable")
				 && !VmSigTypeHas(zType,nType,"callable")
				 && !VmSigTypeHasClass(zType,nType) ){
					/* An object with __toString() still satisfies a string
					 * parameter in weak mode — php coerces it. */
					int bStringable = VmSigTypeHas(zType,nType,"string")
						&& PH7_ArgSatisfiesString(pArg);
					if( !bStringable ){
						zGiven = VmArgTypeName(pArg);
					}
				}else if( VmSigTypeHasClass(zType,nType)
				       && !VmSigTypeHas(zType,nType,"object")
				       && !VmSigTypeHas(zType,nType,"iterable")
				       && !VmSigTypeHas(zType,nType,"callable")
				       && !VmSigTypeHas(zType,nType,"string") ){
					/* A class-typed parameter given an object of the WRONG class.
					 * Naming a class used to be enough to let ANY object through, so
					 * `date_modify($immutable)` and `timezone_name_get($date)`
					 * answered silently where php raises. Only decided when every
					 * class arm resolves to a declared class: an arm PHL does not
					 * declare cannot be judged, so the parameter stays unscreened. */
					if( !VmSigObjSatisfiesClass(pCtx->pVm,zType,nType,
						(ph7_class_instance *)pArg->x.pOther) ){
						zGiven = VmArgTypeName(pArg);
					}
				}
			}else if( (pArg->iFlags & MEMOBJ_NULL) != 0 ){
				/* php only DEPRECATES null for a non-nullable parameter; PHL rejects
				 * it (scope policy). A leading '?' or an explicit "null" arm in a
				 * union declares the parameter nullable. A "callable" parameter is
				 * left to the builtin's own callback check, which words the failure
				 * php's way ("must be a valid callback, no array or string given") —
				 * the same reason get_class_vars() sits on azSelfChecked[]. */
				if( zType[0] != '?'
				 && !VmSigTypeHas(zType,nType,"null")
				 && !VmSigTypeHas(zType,nType,"callable") ){
					zGiven = "null";
				}
			}else if( (pArg->iFlags & (MEMOBJ_STRING|MEMOBJ_INT|MEMOBJ_REAL|MEMOBJ_BOOL)) != 0
			       && VmSigTypeHasClass(zType,nType) ){
				/* A SCALAR against a class-typed parameter. Every other scalar
				 * pairing is left to weak-mode coercion, which is why nothing
				 * screened scalars here at all — but no coercion produces an
				 * instance, so php rejects this one. Found converting DateTime:
				 * `$d->diff('x')` and `new DateTime('now','UTC')` ran on with a
				 * string where php raises. An arm a scalar CAN satisfy (a union
				 * with string/int/float/bool, or callable, which a string is)
				 * keeps the parameter unscreened. */
				if( !VmSigTypeHas(zType,nType,"string")
				 && !VmSigTypeHas(zType,nType,"int")
				 && !VmSigTypeHas(zType,nType,"float")
				 && !VmSigTypeHas(zType,nType,"bool")
				 && !VmSigTypeHas(zType,nType,"true")
				 && !VmSigTypeHas(zType,nType,"false")
				 && !VmSigTypeHas(zType,nType,"callable") ){
					/* php's VALUE name, not the type's: a bool is reported as
					 * `true`/`false` (the rule Generator::throw()'s own check
					 * already followed, and which this screen now runs first). */
					zGiven = VmValueGivenName(pArg,zGivenBuf,sizeof(zGivenBuf));
				}
			}else if( (pArg->iFlags & MEMOBJ_RES) != 0 ){
				/* A class-typed parameter also accepts a resource: several handles php 8
				 * models as objects are still resources here (xml_*'s XMLParser is the
				 * one the signatures already declare php-8-style, for reflection). The
				 * screen would otherwise reject the engine's own parser handle. Recorded
				 * as a divergence in NEWPLAN §7 — it goes away when those handles become
				 * real objects. */
				if( !VmSigTypeHas(zType,nType,"resource")
				 && !VmSigTypeHasClass(zType,nType) ){
					zGiven = "resource";
				}
			}
			if( zGiven ){
				return PH7_VmThrowException(pCtx,"TypeError",
					"%z(): Argument #%d ($%.*s) must be of type %.*s, %s given",
					&pFunc->sName,iArg + 1,nName,zName,nType,zType,zGiven);
			}
		}
		zCur = (zStop < zEnd) ? zStop + 1 : zEnd;
		iArg++;
	}
	return SXRET_OK;
}
/*
 * Builtins whose accepted arity is NOT a contiguous range, so the signature
 * cannot express it and the central too-many-arguments check must stay out of
 * the way: rand()/mt_rand() take 0 OR 2 arguments (never 1), and php words the
 * violation "expects exactly 2 arguments, 3 given" from their own check rather
 * than the ZPP "at most". They validate themselves; leaving bHasMaxArg at 0
 * keeps their message php-faithful.
 */
static int VmBuiltinSelfValidatesArity(const char *zName)
{
	static const char *const azSelf[] = { "rand", "mt_rand" };
	sxu32 i;
	for( i = 0 ; i < SX_ARRAYSIZE(azSelf) ; i++ ){
		sxu32 nSelf = SyStrlen(azSelf[i]);
		if( SyStrlen(zName) == nSelf && SyStrncmp(zName,azSelf[i],nSelf) == 0 ){
			return 1;
		}
	}
	return 0;
}
/*
 * D1: derive a by-reference position bitmask from a php-style signature string.
 * Bit N is set when positional parameter N is declared by-reference (a `&` appears
 * anywhere in that comma-separated parameter, e.g. `&$matches`, `&...$vars`). Only
 * the first 31 positions are representable; a by-ref parameter past that is rare
 * for a builtin and simply not tracked here. The scan mirrors VmDeriveArityFromSig.
 */
PH7_PRIVATE sxu32 VmDeriveByRefMaskFromSig(const char *zSig)
{
	sxu32 mask = 0;
	int n = 0;       /* current parameter index */
	int bSeen = 0;   /* current parameter has non-space content */
	int bRef = 0;    /* current parameter carries a by-ref `&` */
	const char *zCur = zSig;
	for(;;){
		if( zCur[0] == '\0' || zCur[0] == ',' ){
			if( bSeen ){
				if( bRef && n < 31 ){
					mask |= (1u << n);
				}
				n++;
			}
			if( zCur[0] == '\0' ){
				break;
			}
			bSeen = bRef = 0;
			zCur++;
			continue;
		}
		if( zCur[0] != ' ' ){
			bSeen = 1;
		}
		if( zCur[0] == '&' ){
			bRef = 1;
		}
		zCur++;
	}
	return mask;
}
PH7_PRIVATE void VmSetBuiltinSignatures(ph7_vm *pVm)
{
	sxu32 n;
	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinSig) ; n++ ){
		SyHashEntry *pEntry = SyHashGet(&pVm->hHostFunction,
			(const void *)aBuiltinSig[n].zName,(sxu32)SyStrlen(aBuiltinSig[n].zName));
		if( pEntry ){
			ph7_user_func *pFunc = (ph7_user_func *)pEntry->pUserData;
			sxi16 nMin = 0, nMax = 0;
			sxu8 bAtLeast = 0, bHasMax = 0;
			pFunc->zSig = aBuiltinSig[n].zSig;
			pFunc->zRet = aBuiltinSig[n].zRet[0] ? aBuiltinSig[n].zRet : 0;
			pFunc->nByRefMask = VmDeriveByRefMaskFromSig(pFunc->zSig);
			VmDeriveArityFromSig(pFunc->zSig,&nMin,&bAtLeast,&nMax,&bHasMax);
			/* The MAXIMUM always comes from the signature: the curated override
			 * table speaks only to the minimum (and its wording). */
			pFunc->nMaxArg = nMax;
			pFunc->bHasMaxArg = (sxu8)(VmBuiltinSelfValidatesArity(aBuiltinSig[n].zName) ? 0 : bHasMax);
			if( pFunc->nMinArg < 1 ){
				/* VmSetBuiltinArity() ran first: a non-zero minimum here means the
				 * curated override already spoke for this builtin, so leave it. */
				pFunc->nMinArg = nMin;
				pFunc->bAtLeast = bAtLeast;
			}
		}
	}
}
/*
 * Signature lookup by function name, for the reflection layer: embedded-PHP
 * builtins (max/min/Exception methods...) are ph7_vm_func instances, not
 * host functions, so they miss the VmSetBuiltinSignatures stamping and pull
 * their row on demand here. Linear scan — reflection-path only.
 */
PH7_PRIVATE const char * PH7_VmBuiltinSigLookup(const char *zName,sxu32 nLen,const char **pzRet)
{
	sxu32 n;
	if( pzRet ){
		*pzRet = 0;
	}
	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinSig) ; n++ ){
		if( SyStrlen(aBuiltinSig[n].zName) == nLen
		 && SyMemcmp(aBuiltinSig[n].zName,zName,nLen) == 0 ){
			if( pzRet && aBuiltinSig[n].zRet[0] ){
				*pzRet = aBuiltinSig[n].zRet;
			}
			return aBuiltinSig[n].zSig;
		}
	}
	return 0;
}
/*
 * Write a value back to the caller's variable through a builtin argument's
 * stack-slot nIdx — the shared write-back for builtin by-reference
 * out-parameters (preg_match $matches, preg_replace &$count, similar_text
 * &$percent, ...).
 *
 * For a positional out-param argument the call compiler auto-vivifies known
 * by-reference out-params (see GenStateByRefBuiltinMask in compile.c), so a
 * bare undefined variable, an array subscript, and a declared/untyped
 * property all arrive with a real nIdx and are written back here, matching
 * PHP's reference semantics.
 *
 * nIdx stays SXU32_HIGH and the write-back to the caller is skipped (the
 * value still lands in the local stack slot) when the argument cannot expose
 * a stable memobj slot: a literal, a function-call result, a subscript of a
 * non-lvalue parent (foo()['k']), or any variable in a call that also uses
 * named or spread arguments (compile-time positions no longer map to the
 * runtime arg slots, so the compiler conservatively does not vivify). An
 * uninitialized typed property is also not wired (it throws before the
 * write) -- see the recorded deferrals.
 */
PH7_PRIVATE void PH7_VmStoreArgByRef(ph7_vm *pVm,ph7_value *pArg,ph7_value *pNewVal)
{
	if( pArg->nIdx != SXU32_HIGH ){
		ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pArg->nIdx);
		if( pObj ){
			PH7_MemObjStore(pNewVal,pObj);
		}
	}
	PH7_MemObjStore(pNewVal,pArg);
}
/*
 * Raise an E_WARNING whose text is used VERBATIM.
 * ph7_context_throw_error_format() prepends "func(): " to whatever it is given, but php's
 * IO warnings put the offending path inside those parens -- "file_get_contents(/nope):
 * Failed to open stream: ..." -- so a caller that needs php's exact shape must build the
 * whole line itself and come through here.
 */
/*
 * Validate a callback argument and throw php's TypeError when it cannot be called.
 *
 * The shared dispatcher (PH7_VmCallUserFunctionWithMap) answers SXRET_OK with a NULL
 * result for an unresolvable callable, so every caller that did not check first failed
 * SILENTLY: call_user_func() returned NULL and usort() left the array sorted by nothing
 * at all. php rejects the ARGUMENT up front, naming exactly why.
 */
PH7_PRIVATE sxi32 PH7_CheckCallbackArg(
	ph7_context *pCtx,   /* Calling context (names the function in the message) */
	ph7_value *pCb,      /* The callback argument */
	int iArg,            /* Its 1-based position */
	const char *zParam,  /* Its php parameter name ("callback"), or 0 for the variadic
	                      * comparators php names by position only (array_udiff …) */
	int bNullable        /* TRUE when php's text says "or null" */
	)
{
	char zReason[256];
	const char *zWhy = PH7_VmCallableReason(pCtx->pVm,pCb,zReason,sizeof(zReason));
	if( zWhy == 0 ){
		return PH7_OK;
	}
	if( zParam ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"%s(): Argument #%d ($%s) must be a valid callback%s, %s",
			ph7_function_name(pCtx),iArg,zParam,bNullable ? " or null" : "",zWhy);
	}
	return PH7_VmThrowException(pCtx,"TypeError",
		"%s(): Argument #%d must be a valid callback%s, %s",
		ph7_function_name(pCtx),iArg,bNullable ? " or null" : "",zWhy);
}
PH7_PRIVATE void PH7_VmThrowWarningFmt(ph7_vm *pVm,const char *zFmt,...)
{
	va_list ap;
	va_start(ap,zFmt);
	PH7_VmThrowErrorAp(pVm,0,PH7_CTX_WARNING,zFmt,ap);
	va_end(ap);
}
/*
 * Emit a formatted E_USER_DEPRECATED diagnostic with no function-name prefix:
 * php reports #[\Deprecated] as USER-deprecated (16384, it is userland-authored),
 * not the engine's E_DEPRECATED (8192) — handler-visible errno matters.
 */
static void VmThrowUserDeprecatedFmt(ph7_vm *pVm,const char *zFmt,...)
{
	va_list ap;
	va_start(ap,zFmt);
	PH7_VmThrowErrorAp(pVm,0,E_USER_DEPRECATED,zFmt,ap);
	va_end(ap);
}
/*
 * php 8.4 #[\Deprecated]: emit the E_USER_DEPRECATED notice for a call to a user
 * function/method carrying the attribute. Message shapes (php-exact):
 *   Function f() is deprecated
 *   Method C::m() is deprecated since 2.0, use g() instead
 * ("since" from the attribute's since: argument; the trailing ", msg" from
 * message:/positional #1.) The attribute arguments are compiled constant
 * expressions — evaluated via VmLocalExec, the reflection thunks' pattern.
 * Called from OP_CALL once per call, only when the callee HAS attributes;
 * pDeclClass names the method's declaring class (0 for plain functions).
 */
/*
 * Expand a global constant's value, emitting the php 8.5 #[\Deprecated]
 * E_USER_DEPRECATED notice first when the constant carries attributes
 * (`Constant GD is deprecated since 1.2` — every access re-warns).
 */
static void VmDeprecatedAttrNoticeSubject(ph7_vm *pVm,SySet *pAttrs,
	const char *zKind,const SyString *pQual,const SyString *pName);
/*
 * Expand a constant, emitting the php 8.5 #[\Deprecated] E_USER_DEPRECATED notice
 * first when a USERLAND constant carries the attribute (`Constant GD is deprecated
 * since 1.2` — every access re-warns). Engine constants php merely deprecates are
 * not mimicked: PHL removes them outright (see the scope policy), so there is no
 * engine-side E_DEPRECATED list here.
 */
PH7_PRIVATE void VmExpandConstantWithNotice(ph7_vm *pVm,ph7_constant *pCons,ph7_value *pOut)
{
	if( SySetUsed(&pCons->aAttrs) > 0 ){
		VmDeprecatedAttrNoticeSubject(pVm,&pCons->aAttrs,"Constant",0,&pCons->sName);
	}
	pCons->xExpand(pOut,pCons->pUserData);
}
/*
 * Scan a declared-attribute set for #[\Deprecated]; when found, evaluate its
 * message:/since: arguments (positional #0 = message, #1 = since) into the
 * caller's values and return TRUE. The base subject text ("Function f()",
 * "Constant C::K") is the caller's business.
 */
static int VmDeprecatedAttrExtract(ph7_vm *pVm,SySet *pAttrs,
	ph7_value *pMsg,int *pbMsg,ph7_value *pSince,int *pbSince)
{
	ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(pAttrs);
	sxu32 n;
	*pbMsg = *pbSince = 0;
	for( n = 0 ; n < SySetUsed(pAttrs) ; ++n ){
		ph7_attribute *pAttr = &aAttr[n];
		ph7_attr_arg *aArg;
		sxu32 i,nPos = 0;
		if( SyStringLength(&pAttr->sName) != sizeof("Deprecated")-1
		 || SyStrnicmp(SyStringData(&pAttr->sName),"Deprecated",sizeof("Deprecated")-1) != 0 ){
			continue;
		}
		aArg = (ph7_attr_arg *)SySetBasePtr(&pAttr->aArgs);
		for( i = 0 ; i < SySetUsed(&pAttr->aArgs) ; ++i ){
			ph7_attr_arg *pArg = &aArg[i];
			int isMsg = 0,isSince = 0;
			if( SyStringLength(&pArg->sName) == 0 ){
				isMsg = (nPos == 0);
				isSince = (nPos == 1);
				nPos++;
			}else if( SyStringLength(&pArg->sName) == sizeof("message")-1
			 && SyMemcmp(SyStringData(&pArg->sName),"message",sizeof("message")-1) == 0 ){
				isMsg = 1;
			}else if( SyStringLength(&pArg->sName) == sizeof("since")-1
			 && SyMemcmp(SyStringData(&pArg->sName),"since",sizeof("since")-1) == 0 ){
				isSince = 1;
			}
			if( isMsg && !*pbMsg && SySetUsed(&pArg->aByteCode) > 0 ){
				if( VmLocalExec(pVm,&pArg->aByteCode,pMsg,FALSE) == SXRET_OK ){
					if( (pMsg->iFlags & MEMOBJ_STRING) == 0 ){
						PH7_MemObjToString(pMsg);
					}
					*pbMsg = 1;
				}
			}else if( isSince && !*pbSince && SySetUsed(&pArg->aByteCode) > 0 ){
				if( VmLocalExec(pVm,&pArg->aByteCode,pSince,FALSE) == SXRET_OK ){
					if( (pSince->iFlags & MEMOBJ_STRING) == 0 ){
						PH7_MemObjToString(pSince);
					}
					*pbSince = 1;
				}
			}
		}
		return 1;
	}
	return 0;
}
/*
 * Append php's " since X" / ", message" suffixes to a built base subject and
 * emit the E_USER_DEPRECATED notice.
 */
static void VmDeprecatedEmit(ph7_vm *pVm,SyBlob *pOut,
	ph7_value *pMsg,int bMsg,ph7_value *pSince,int bSince)
{
	if( bSince && SyBlobLength(&pSince->sBlob) > 0 ){
		SyBlobFormat(pOut," since %.*s",(int)SyBlobLength(&pSince->sBlob),
			(const char *)SyBlobData(&pSince->sBlob));
	}
	if( bMsg && SyBlobLength(&pMsg->sBlob) > 0 ){
		SyBlobFormat(pOut,", %.*s",(int)SyBlobLength(&pMsg->sBlob),
			(const char *)SyBlobData(&pMsg->sBlob));
	}
	VmThrowUserDeprecatedFmt(pVm,"%.*s",(int)SyBlobLength(pOut),(const char *)SyBlobData(pOut));
}
/*
 * Generic #[\Deprecated] notice for a named subject:
 * "<Kind> [Qual::]Name is deprecated[ since X][, message]".
 */
static void VmDeprecatedAttrNoticeSubject(ph7_vm *pVm,SySet *pAttrs,
	const char *zKind,const SyString *pQual,const SyString *pName)
{
	ph7_value sMsg,sSince;
	SyBlob sOut;
	int bMsg,bSince;
	PH7_MemObjInit(pVm,&sMsg);
	PH7_MemObjInit(pVm,&sSince);
	if( VmDeprecatedAttrExtract(pVm,pAttrs,&sMsg,&bMsg,&sSince,&bSince) ){
		SyBlobInit(&sOut,&pVm->sAllocator);
		if( pQual ){
			SyBlobFormat(&sOut,"%s %z::%z is deprecated",zKind,pQual,pName);
		}else{
			SyBlobFormat(&sOut,"%s %z is deprecated",zKind,pName);
		}
		VmDeprecatedEmit(pVm,&sOut,&sMsg,bMsg,&sSince,bSince);
		SyBlobRelease(&sOut);
	}
	PH7_MemObjRelease(&sMsg);
	PH7_MemObjRelease(&sSince);
}
PH7_PRIVATE void VmDeprecatedAttrNotice(ph7_vm *pVm,ph7_vm_func *pFunc,ph7_class *pDeclClass)
{
	ph7_value sMsg,sSince;
	SyBlob sOut;
	int bMsg,bSince;
	PH7_MemObjInit(pVm,&sMsg);
	PH7_MemObjInit(pVm,&sSince);
	if( VmDeprecatedAttrExtract(pVm,&pFunc->aAttrs,&sMsg,&bMsg,&sSince,&bSince) ){
		SyBlobInit(&sOut,&pVm->sAllocator);
		if( pDeclClass ){
			SyBlobFormat(&sOut,"Method %z::%z() is deprecated",&pDeclClass->sName,&pFunc->sName);
		}else{
			SyBlobFormat(&sOut,"Function %z() is deprecated",&pFunc->sName);
		}
		VmDeprecatedEmit(pVm,&sOut,&sMsg,bMsg,&sSince,bSince);
		SyBlobRelease(&sOut);
	}
	PH7_MemObjRelease(&sMsg);
	PH7_MemObjRelease(&sSince);
}
/*
 * Same notice for a #[\Deprecated] class constant, at its static-access site:
 * php's `Constant C::K is deprecated` (each access re-warns).
 */
PH7_PRIVATE void VmDeprecatedConstNotice(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pMember)
{
	VmDeprecatedAttrNoticeSubject(pVm,&pMember->aAttrs,
		(pMember->iFlags & PH7_CLASS_ATTR_ENUMCASE) ? "Enum case" : "Constant",
		&pClass->sName,&pMember->sName);
}
/*
 * The USER-VISIBLE string coercion a builtin performs on a `mixed` argument or
 * array element: the builtin-side twin of PH7_MemObjToStringUV's opcode sites.
 * An ARRAY warns "Array to string conversion" and still renders as "Array"; an
 * object whose class has no __toString() -- or one whose __toString() threw --
 * is php's catchable "could not be converted to string" Error, and the builtin
 * must answer that instead of a value.
 *
 * On success pzData and pnLen receive the NUL-terminated bytes (both optional).
 * On a throw they are set to the empty string and the status is returned AND
 * recorded on the call context, so OP_CALL cannot mistake the call for a normal
 * return; a builtin that has already produced output (printf) still keeps it,
 * which is what php does.
 *
 * The public ph7_value_to_string() stays SILENT on purpose: it is the embedder
 * API, and the INTERNAL coercions behind it (array-key canonicalisation, sort
 * comparisons, print_r/var_export/serialize) must not throw -- php's do not
 * either.
 */
PH7_PRIVATE sxi32 PH7_ValueToStringUV(ph7_context *pCtx,ph7_value *pValue,const char **pzData,int *pnLen)
{
	sxi32 rc = PH7_MemObjToStringUV(pValue);
	if( rc != SXRET_OK ){
		if( pCtx ){
			pCtx->nThrowRc = rc;
		}
		if( pzData ){
			*pzData = "";
		}
		if( pnLen ){
			*pnLen = 0;
		}
		return rc;
	}
	if( pzData || pnLen ){
		const char *zData = ph7_value_to_string(pValue,pnLen);
		if( pzData ){
			*pzData = zData;
		}
	}
	return SXRET_OK;
}
