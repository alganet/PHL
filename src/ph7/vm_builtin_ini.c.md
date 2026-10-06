# src/ph7/vm_builtin_ini.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 559/610 lines (91.64%)

[Root index](../../index.md) | [Directory index](index.md)

|      Hits | Line | Source |
| --------: | ---: | :--- |
|         - |    1 | `/**` |
|         - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|         - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|         - |    4 | ` */` |
|         - |    5 | `#include "ph7int.h"` |
|         - |    6 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - |    7 | `/*` |
|         - |    8 | ` * php.ini subsystem + INI API: a lazily seeded directive table (the static` |
|         - |    9 | ` * defaults merged with the CLI's -d/-c entries from pVm->aIniCli) behind` |
|         - |   10 | ` * ini_get / ini_set / ini_restore / ini_get_all / get_cfg_var. Live-wired` |
|         - |   11 | ` * directives dispatch to the real knobs (error_reporting(), the session state,` |
|         - |   12 | ` * the default timezone, the diagnostic gates) so the INI view and the engine` |
|         - |   13 | ` * agree.` |
|         - |   14 | ` *` |
|         - |   15 | `` * This was an embedded-PHP chunk over two `__ini_*` C thunks, with the table`` |
|         - |   16 | `` * parked on a private `__IniS` class and five `__ini_*` PHP helpers alongside.`` |
|         - |   17 | ` * All eight of those names are gone: the table is a SySet on the VM and the five` |
|         - |   18 | ` * functions ARE these C routines. The thunks did not become methods -- there is` |
|         - |   19 | ` * no class here, only php's global functions -- so, like libxml's, they collapse` |
|         - |   20 | ` * into the functions they were serving.` |
|         - |   21 | ` *` |
|         - |   22 | ` * One thing the move fixes on its own: a diagnostic raised by a prelude function` |
|         - |   23 | ` * reported the CHUNK's line, so every ini_set()/ini_get_all() warning said` |
|         - |   24 | ` * "on line 1" regardless of the caller. A C builtin reports the caller's line,` |
|         - |   25 | ` * which is what php prints.` |
|         - |   26 | ` */` |
|         - |   27 |  |
|         - |   28 | `/* Directive access levels, as php reports them in ini_get_all()['access']. */` |
|         - |   29 | `#define VM_INI_USER    1` |
|         - |   30 | `#define VM_INI_PERDIR  2` |
|         - |   31 | `#define VM_INI_SYSTEM  4` |
|         - |   32 | `#define VM_INI_ALL     (VM_INI_USER\|VM_INI_PERDIR\|VM_INI_SYSTEM)` |
|         - |   33 |  |
|         - |   34 | `/*` |
|         - |   35 | ` * The defaults, in the order php's ini_get_all() reports them (sorted by name).` |
|         - |   36 | ` * Keeping this list sorted is what lets ini_get_all() skip a sort: the seed` |
|         - |   37 | ` * inserts any CLI-only directive in its sorted position.` |
|         - |   38 | ` */` |
|         - |   39 | `static const struct {` |
|         - |   40 | `	const char *zName;` |
|         - |   41 | `	const char *zValue;   /* 0 = php's UNSET directive, which reports NULL and` |
|         - |   42 | `	                       * is not the same thing as "" */` |
|         - |   43 | `	sxi32 iAccess;` |
|         - |   44 | `} aIniDefault[] = {` |
|         - |   45 | `	{ "allow_url_fopen",          "1",          VM_INI_SYSTEM },` |
|         - |   46 | `	/* php's default is OFF, and it spells that default as the EMPTY string — which` |
|         - |   47 | `	 * is what ini_get() answers. Including a remote file is the classic RFI, and` |
|         - |   48 | `	 * this is what a STREAM_IS_URL wrapper's include is gated on. */` |
|         - |   49 | `	{ "allow_url_include",        "",           VM_INI_SYSTEM },` |
|         - |   50 | `	{ "arg_separator.input",      "&",          VM_INI_PERDIR\|VM_INI_SYSTEM },` |
|         - |   51 | `	{ "arg_separator.output",     "&",          VM_INI_ALL },` |
|         - |   52 | `	{ "auto_detect_line_endings", "",           VM_INI_ALL },` |
|         - |   53 | `	/* ext/bcmath's only directive: the scale every bc* function defaults its` |
|         - |   54 | `	 * $scale argument from, and the one slot bcscale() reads and writes. */` |
|         - |   55 | `	{ "bcmath.scale",             "0",          VM_INI_ALL },` |
|         - |   56 | `	{ "date.timezone",            "UTC",        VM_INI_ALL },` |
|         - |   57 | `	/* The four the sun trio defaults from. php ships this point (Jerusalem)` |
|         - |   58 | `	 * and this zenith (the sun's centre 50 arcminutes below the horizon,` |
|         - |   59 | `	 * which is refraction plus its own radius) as the stock values. */` |
|         - |   60 | `	{ "date.default_latitude",    "31.7667",    VM_INI_ALL },` |
|         - |   61 | `	{ "date.default_longitude",   "35.2333",    VM_INI_ALL },` |
|         - |   62 | `	{ "date.sunrise_zenith",      "90.833333",  VM_INI_ALL },` |
|         - |   63 | `	{ "date.sunset_zenith",       "90.833333",  VM_INI_ALL },` |
|         - |   64 | `	{ "default_charset",          "UTF-8",      VM_INI_ALL },` |
|         - |   65 | `	/* php bounds a socket wait by this rather than waiting forever, and it is` |
|         - |   66 | `	 * where stream_socket_accept() takes its default timeout from. */` |
|         - |   67 | `	{ "default_socket_timeout",   "60",         VM_INI_ALL },` |
|         - |   68 | `	{ "default_mimetype",         "text/html",  VM_INI_ALL },` |
|         - |   69 | `	{ "display_errors",           "",           VM_INI_ALL },` |
|         - |   70 | `	{ "error_log",                0,            VM_INI_ALL },` |
|         - |   71 | `	{ "error_reporting",          "30719",      VM_INI_ALL },` |
|         - |   72 | `	/* The From: header the http:// wrapper writes. php ships it UNSET, and the` |
|         - |   73 | `	 * difference matters: an unset directive writes no header at all, while an` |
|         - |   74 | ``	 * ini_set() to the EMPTY string writes `From: ` -- which is not how`` |
|         - |   75 | `	 * user_agent below behaves. */` |
|         - |   76 | `	{ "from",                     0,            VM_INI_ALL },` |
|         - |   77 | `	{ "highlight.comment",        "#FF8000",    VM_INI_ALL },` |
|         - |   78 | `	{ "highlight.default",        "#0000BB",    VM_INI_ALL },` |
|         - |   79 | `	{ "highlight.html",           "#000000",    VM_INI_ALL },` |
|         - |   80 | `	{ "highlight.keyword",        "#007700",    VM_INI_ALL },` |
|         - |   81 | `	{ "highlight.string",         "#DD0000",    VM_INI_ALL },` |
|         - |   82 | `	/* ignore_user_abort(): the directive the function reads and writes. php` |
|         - |   83 | `	 * spells its default "0" rather than the empty string every other boolean` |
|         - |   84 | `	 * directive here uses, and ini_get() answers that byte. */` |
|         - |   85 | `	{ "ignore_user_abort",        "0",          VM_INI_ALL },` |
|         - |   86 | `	{ "include_path",             ".",          VM_INI_ALL },` |
|         - |   87 | `	{ "log_errors",               "1",          VM_INI_ALL },` |
|         - |   88 | `	{ "max_execution_time",       "0",          VM_INI_ALL },` |
|         - |   89 | `	{ "max_input_nesting_level",  "64",         VM_INI_PERDIR\|VM_INI_SYSTEM },` |
|         - |   90 | `	{ "max_input_vars",           "1000",       VM_INI_PERDIR\|VM_INI_SYSTEM },` |
|         - |   91 | `	{ "memory_limit",             "-1",         VM_INI_ALL },` |
|         - |   92 | `#ifdef PH7_ENABLE_OPENSSL` |
|         - |   93 | `	/* ext/openssl's two path directives, php's own defaults (both UNSET, the` |
|         - |   94 | `	 * empty string) and its access mask (PHP_INI_PERDIR for both). They are` |
|         - |   95 | `	 * what openssl_get_cert_locations() reports as ini_cafile/ini_capath and` |
|         - |   96 | `	 * what a TLS peer verification falls back to when a stream context names` |
|         - |   97 | `	 * no CA of its own. Both ship UNSET rather than empty, which is what php` |
|         - |   98 | `	 * reports and is a different thing: ini_get_all() answers NULL for an` |
|         - |   99 | ``	 * unset directive and "" for one set to the empty string. php's third, `openssl.libctx`, is not here: it picks`` |
|         - |  100 | `	 * between the default OpenSSL library context and a private one, and this` |
|         - |  101 | `	 * build has only the default -- registering the name would report a` |
|         - |  102 | `	 * choice that is not being made. */` |
|         - |  103 | `	{ "openssl.cafile",           0,            VM_INI_PERDIR },` |
|         - |  104 | `	{ "openssl.capath",           0,            VM_INI_PERDIR },` |
|         - |  105 | `#endif` |
|         - |  106 | ``	/* ext/phar's three. `phar.readonly` is php's own default ON: every write`` |
|         - |  107 | `	 * door refuses until an installer turns it off, which is why building an` |
|         - |  108 | ``	 * archive is a `-d phar.readonly=0` job on a stock php too. */`` |
|         - |  109 | `	{ "phar.cache_list",          "",           VM_INI_SYSTEM },` |
|         - |  110 | `	{ "phar.readonly",            "1",          VM_INI_ALL },` |
|         - |  111 | `	{ "phar.require_hash",        "1",          VM_INI_ALL },` |
|         - |  112 | `	{ "post_max_size",            "8M",         VM_INI_PERDIR\|VM_INI_SYSTEM },` |
|         - |  113 | `	{ "precision",                "14",         VM_INI_ALL },` |
|         - |  114 | `	{ "serialize_precision",      "-1",         VM_INI_ALL },` |
|         - |  115 | `	/* php's session directives, the whole non-deprecated set: session_start()'s` |
|         - |  116 | `	 * $options array applies its keys THROUGH this table, so a directive missing` |
|         - |  117 | `	 * here is an option php accepts and PHL reports as failed.` |
|         - |  118 | `	 * Six are absent on purpose: php 8.4 DEPRECATES session.sid_length,` |
|         - |  119 | `	 * session.sid_bits_per_character, session.referer_check, session.use_trans_sid,` |
|         - |  120 | `	 * session.trans_sid_tags and session.trans_sid_hosts, and the scope policy does not carry` |
|         - |  121 | `	 * php's deprecated surface. The session.upload_progress.* family goes with the` |
|         - |  122 | ``	 * file uploads the scope policy excludes from a CLI-plus-`-S` engine. */`` |
|         - |  123 | `	{ "session.auto_start",       "0",          VM_INI_PERDIR },` |
|         - |  124 | `	{ "session.cache_expire",     "180",        VM_INI_ALL },` |
|         - |  125 | `	{ "session.cache_limiter",    "nocache",    VM_INI_ALL },` |
|         - |  126 | `	/* The Set-Cookie the session sends is built out of these seven. */` |
|         - |  127 | `	{ "session.cookie_domain",    "",           VM_INI_ALL },` |
|         - |  128 | `	{ "session.cookie_httponly",  "0",          VM_INI_ALL },` |
|         - |  129 | `	{ "session.cookie_lifetime",  "0",          VM_INI_ALL },` |
|         - |  130 | `	{ "session.cookie_partitioned","0",         VM_INI_ALL },` |
|         - |  131 | `	{ "session.cookie_path",      "/",          VM_INI_ALL },` |
|         - |  132 | `	{ "session.cookie_samesite",  "",           VM_INI_ALL },` |
|         - |  133 | `	{ "session.cookie_secure",    "0",          VM_INI_ALL },` |
|         - |  134 | `	{ "session.gc_divisor",       "100",        VM_INI_ALL },` |
|         - |  135 | `	{ "session.gc_maxlifetime",   "1440",       VM_INI_ALL },` |
|         - |  136 | `	{ "session.gc_probability",   "1",          VM_INI_ALL },` |
|         - |  137 | `	{ "session.lazy_write",       "1",          VM_INI_ALL },` |
|         - |  138 | `	{ "session.name",             "PHPSESSID",  VM_INI_ALL },` |
|         - |  139 | `	{ "session.save_handler",     "files",      VM_INI_ALL },` |
|         - |  140 | `	{ "session.save_path",        "",           VM_INI_ALL },` |
|         - |  141 | ``	/* Which of php's three session serializers writes the store: `php` (the`` |
|         - |  142 | ``	 * `name\|<serialized>` runs a stock php install reads), `php_binary` or`` |
|         - |  143 | ``	 * `php_serialize`. */`` |
|         - |  144 | `	{ "session.serialize_handler","php",        VM_INI_ALL },` |
|         - |  145 | `	/* Whether the session sends and reads its id as a cookie at all. PHL has never` |
|         - |  146 | `	 * read an id from anywhere ELSE, which is what use_only_cookies means. */` |
|         - |  147 | `	{ "session.use_cookies",      "1",          VM_INI_ALL },` |
|         - |  148 | `	{ "session.use_only_cookies", "1",          VM_INI_ALL },` |
|         - |  149 | `	{ "session.use_strict_mode",  "0",          VM_INI_ALL },` |
|         - |  150 | `	{ "short_open_tag",           "",           VM_INI_PERDIR\|VM_INI_SYSTEM },` |
|         - |  151 | `	/* php's three syslog directives, with php's defaults and php's access masks.` |
|         - |  152 | ``	 * `syslog.filter` is the one this engine READS: it decides which bytes`` |
|         - |  153 | `	 * syslog() escapes and is PHP_INI_ALL, so a script can change it. The other` |
|         - |  154 | ``	 * two are what php's own error logger uses when `error_log = syslog`, a`` |
|         - |  155 | `	 * target this build does not have -- they are declared because ini_get() and` |
|         - |  156 | `	 * ini_get_all() answer them under php and a program can read either. */` |
|         - |  157 | `	{ "syslog.facility",          "LOG_USER",   VM_INI_SYSTEM },` |
|         - |  158 | `	{ "syslog.filter",            "no-ctrl",    VM_INI_ALL },` |
|         - |  159 | `	{ "syslog.ident",             "php",        VM_INI_SYSTEM },` |
|         - |  160 | `#ifdef PH7_ENABLE_SQLITE` |
|         - |  161 | `` 	/* ext/sqlite3's two directives, in this sorted list's own place. `defensive` `` |
|         - |  162 | `	 * is applied to every connection SQLite3 opens (it is what makes an UPDATE of` |
|         - |  163 | ``	 * sqlite_master refuse, even behind `PRAGMA writable_schema=ON`), and`` |
|         - |  164 | ``	 * `extension_dir` is the door loadExtension() is shut behind: empty means`` |
|         - |  165 | `	 * "SQLite Extensions are disabled", and php ships it empty. The access masks` |
|         - |  166 | `	 * are php's own, which do not agree with each other. */` |
|         - |  167 | `	{ "sqlite3.defensive",        "1",          VM_INI_USER },` |
|         - |  168 | `	{ "sqlite3.extension_dir",    0,            VM_INI_SYSTEM },` |
|         - |  169 | `#endif` |
|         - |  170 | `	{ "unserialize_callback_func","",           VM_INI_ALL },` |
|         - |  171 | `	{ "unserialize_max_depth",    "4096",       VM_INI_ALL },` |
|         - |  172 | `	{ "upload_max_filesize",      "2M",         VM_INI_PERDIR\|VM_INI_SYSTEM },` |
|         - |  173 | `	/* The User-Agent the http:// wrapper writes when the request names none.` |
|         - |  174 | ``	 * UNSET like `from` above, and for the same reason -- but the wrapper reads`` |
|         - |  175 | `	 * the two differently: an empty user_agent writes no header at all, while an` |
|         - |  176 | ``	 * empty `from` writes `From: `. */`` |
|         - |  177 | `	{ "user_agent",               0,            VM_INI_ALL },` |
|         - |  178 | `	{ "zend.assertions",          "-1",         VM_INI_ALL },` |
|         - |  179 | `	/* What a trace keeps of a call's arguments. php's built-in defaults are 0 and` |
|         - |  180 | `	 * 15; these are the values php.ini-production ships, which is the shape every` |
|         - |  181 | ``	 * trace here has always had: no `args`, and a string argument (where a frame`` |
|         - |  182 | ``	 * has one) printed as `'...'`. Both are PHP_INI_ALL. */`` |
|         - |  183 | `	{ "zend.exception_ignore_args","1",         VM_INI_ALL },` |
|         - |  184 | `	{ "zend.exception_string_param_max_len","0",VM_INI_ALL },` |
|         - |  185 | `#ifdef PH7_ENABLE_ZLIB` |
|         - |  186 | `	/* ext/zlib's three, php's own defaults and its access mask (all three are` |
|         - |  187 | `	 * PHP_INI_ALL). They are READ by ob_gzhandler() and zlib_get_coding_type()` |
|         - |  188 | `	 * and by nothing else: php's output-layer compression is a SAPI feature a` |
|         - |  189 | `	 * command line never turns on, and neither does this. */` |
|         - |  190 | `	{ "zlib.output_compression",  "",           VM_INI_ALL },` |
|         - |  191 | `	{ "zlib.output_compression_level","-1",     VM_INI_ALL },` |
|         - |  192 | `	{ "zlib.output_handler",      "",           VM_INI_ALL },` |
|         - |  193 | `#endif` |
|         - |  194 | `};` |
|         - |  195 |  |
|   6260136 |  196 | `static int IniNameIs(const VmIniSlot *pSlot,const char *zName)` |
|         5 |  197 | `{` |
|   6260141 |  198 | `	sxu32 n = (sxu32)SyStrlen(zName);` |
|   6260141 |  199 | `	return pSlot->sName.nByte == n && SyMemcmp(pSlot->sName.zString,zName,n) == 0;` |
|         5 |  200 | `}` |
|         - |  201 | `/*` |
|         - |  202 | ` * zend_ini_parse_bool semantics, matching the C-side VmIniBool the -d/-c path` |
|         - |  203 | ` * uses: on/yes/true, else a non-zero integer parse.` |
|         - |  204 | ` */` |
|   1471264 |  205 | `static int IniTruthy(const char *zVal,sxu32 nVal)` |
|         5 |  206 | `{` |
|   2206861 |  207 | `	while( nVal > 0 && (zVal[0] == ' ' \|\| zVal[0] == '\t') ){ zVal++; nVal--; }` |
|   2206861 |  208 | `	while( nVal > 0 && (zVal[nVal-1] == ' ' \|\| zVal[nVal-1] == '\t') ){ nVal--; }` |
|   1471269 |  209 | `	if( nVal == 2 && SyStrnicmp(zVal,"on",2) == 0 ){ return 1; }` |
|   1471269 |  210 | `	if( nVal == 3 && SyStrnicmp(zVal,"yes",3) == 0 ){ return 1; }` |
|   1471269 |  211 | `	if( nVal == 4 && SyStrnicmp(zVal,"true",4) == 0 ){ return 1; }` |
|         - |  212 | `	{` |
|   1471269 |  213 | `		sxi32 iVal = 0;` |
|   1471269 |  214 | `		if( nVal > 0 && SyStrToInt32(zVal,nVal,(void *)&iVal,0) == SXRET_OK ){` |
|   1471263 |  215 | `			return iVal != 0;` |
|         - |  216 | `		}` |
|         - |  217 | `	}` |
|         7 |  218 | `	return 0;` |
|    735600 |  219 | `}` |
|         - |  220 | `/*` |
|         - |  221 | `` * The value rules that apply to a `-d name=value` on the COMMAND LINE, which are`` |
|         - |  222 | ` * not the same set IniValueAccepted enforces on ini_set(). php runs each` |
|         - |  223 | ` * directive's OnUpdate handler at startup too, but several of those refuse only` |
|         - |  224 | `` * at RUNTIME -- `-d session.serialize_handler=bogus` is taken by php because the`` |
|         - |  225 | ` * serializer table it would look the name up in is still empty -- so this is a` |
|         - |  226 | ` * per-directive list rather than a shared screen. A value refused here leaves the` |
|         - |  227 | ` * directive at its default, which is what php reports.` |
|         - |  228 | ` */` |
|       739 |  229 | `static int IniStartupValueAccepted(const SyString *pName,const char *zVal,sxu32 nVal)` |
|         4 |  230 | `{` |
|       739 |  231 | `	if( pName->nByte == sizeof("syslog.filter")-1` |
|       381 |  232 | `	 && SyMemcmp(pName->zString,"syslog.filter",sizeof("syslog.filter")-1) == 0 ){` |
|       ! 0 |  233 | `		return (nVal == 3 && SyMemcmp(zVal,"all",3) == 0)` |
|       ! 0 |  234 | `		    \|\| (nVal == 7 && SyMemcmp(zVal,"no-ctrl",7) == 0)` |
|       ! 0 |  235 | `		    \|\| (nVal == 5 && SyMemcmp(zVal,"ascii",5) == 0)` |
|       ! 0 |  236 | `		    \|\| (nVal == 3 && SyMemcmp(zVal,"raw",3) == 0);` |
|         - |  237 | `	}` |
|       739 |  238 | `	if( pName->nByte == sizeof("zend.exception_string_param_max_len")-1` |
|       375 |  239 | `	 && SyMemcmp(pName->zString,"zend.exception_string_param_max_len",pName->nByte) == 0 ){` |
|         5 |  240 | `		sxi64 iVal = 0;` |
|         5 |  241 | `		SyStrToInt64(zVal,nVal,(void *)&iVal,0);` |
|         5 |  242 | `		return iVal >= 0 && iVal <= 1000000;` |
|         - |  243 | `	}` |
|       739 |  244 | `	return 1;` |
|       373 |  245 | `}` |
|         - |  246 | `/*` |
|         - |  247 | ` * Build the table: the static defaults, then the CLI queue merged over them (an` |
|         - |  248 | ` * unknown CLI name is appended as a new INI_ALL directive, as the chunk did),` |
|         - |  249 | ` * then sorted by name so ini_get_all() can walk it in php's order without a sort.` |
|         - |  250 | ` */` |
|   1478311 |  251 | `static sxi32 IniSeed(ph7_vm *pVm)` |
|         5 |  252 | `{` |
|         - |  253 | `	sxu32 i,j;` |
|         - |  254 | `	VmIniEntry *aCli;` |
|         - |  255 | `	VmIniSlot *aSlot;` |
|   1478316 |  256 | `	if( pVm->bIniSeeded ){` |
|   1476081 |  257 | `		return SXRET_OK;` |
|         - |  258 | `	}` |
|      2240 |  259 | `	pVm->bIniSeeded = 1; /* set FIRST: the live-wired writes below re-enter nothing,` |
|         - |  260 | `	                      * but a future one must never recurse into the seed */` |
|    169865 |  261 | `	for( i = 0 ; i < SX_ARRAYSIZE(aIniDefault) ; i++ ){` |
|         - |  262 | `		VmIniSlot sSlot;` |
|    167630 |  263 | `		SyStringInitFromBuf(&sSlot.sName,aIniDefault[i].zName,SyStrlen(aIniDefault[i].zName));` |
|    167630 |  264 | `		sSlot.iAccess = aIniDefault[i].iAccess;` |
|    167630 |  265 | `		SyBlobInit(&sSlot.sGlobal,&pVm->sAllocator);` |
|    167630 |  266 | `		SyBlobInit(&sSlot.sLocal,&pVm->sAllocator);` |
|         - |  267 | `		/* A row with no value at all is php's UNSET directive, which is not the` |
|         - |  268 | `		 * empty string: it reports NULL everywhere the raw value is shown. */` |
|    167630 |  269 | `		sSlot.bGlobalNull = sSlot.bLocalNull = aIniDefault[i].zValue ? 0 : 1;` |
|    167630 |  270 | `		if( aIniDefault[i].zValue ){` |
|    231086 |  271 | `			SyBlobAppend(&sSlot.sGlobal,aIniDefault[i].zValue,` |
|    154215 |  272 | `				(sxu32)SyStrlen(aIniDefault[i].zValue));` |
|    231086 |  273 | `			SyBlobAppend(&sSlot.sLocal,aIniDefault[i].zValue,` |
|    154215 |  274 | `				(sxu32)SyStrlen(aIniDefault[i].zValue));` |
|     76866 |  275 | `		}` |
|    167630 |  276 | `		if( SySetPut(&pVm->aIniTab,(const void *)&sSlot) != SXRET_OK ){` |
|       ! 0 |  277 | `			return SXERR_MEM;` |
|         - |  278 | `		}` |
|     83555 |  279 | `	}` |
|      2240 |  280 | `	aCli = (VmIniEntry *)SySetBasePtr(&pVm->aIniCli);` |
|      2979 |  281 | `	for( i = 0 ; i < SySetUsed(&pVm->aIniCli) ; i++ ){` |
|       743 |  282 | `		int bFound = 0;` |
|      1112 |  283 | `		if( !IniStartupValueAccepted(&aCli[i].sName,aCli[i].sValue.zString,` |
|       739 |  284 | `			aCli[i].sValue.nByte) ){` |
|       ! 0 |  285 | `			continue;   /* the directive keeps its default, as it does under php */` |
|         - |  286 | `		}` |
|       743 |  287 | `		aSlot = (VmIniSlot *)SySetBasePtr(&pVm->aIniTab);` |
|     30796 |  288 | `		for( j = 0 ; j < SySetUsed(&pVm->aIniTab) ; j++ ){` |
|     30764 |  289 | `			if( aSlot[j].sName.nByte == aCli[i].sName.nByte` |
|     16081 |  290 | `			 && SyMemcmp(aSlot[j].sName.zString,aCli[i].sName.zString,aCli[i].sName.nByte) == 0 ){` |
|       715 |  291 | `				SyBlobReset(&aSlot[j].sGlobal);` |
|       715 |  292 | `				SyBlobReset(&aSlot[j].sLocal);` |
|       715 |  293 | `				SyBlobAppend(&aSlot[j].sGlobal,aCli[i].sValue.zString,aCli[i].sValue.nByte);` |
|       715 |  294 | `				SyBlobAppend(&aSlot[j].sLocal,aCli[i].sValue.zString,aCli[i].sValue.nByte);` |
|         - |  295 | ``				/* `-d name=` names the directive on the command line, so what it`` |
|         - |  296 | `				 * carries is a value -- the empty one, never the unset state. */` |
|       715 |  297 | `				aSlot[j].bGlobalNull = aSlot[j].bLocalNull = 0;` |
|       715 |  298 | `				bFound = 1;` |
|       715 |  299 | `				break;` |
|         - |  300 | `			}` |
|     15014 |  301 | `		}` |
|       743 |  302 | `		if( !bFound ){` |
|         - |  303 | `			VmIniSlot sSlot;` |
|         - |  304 | `			/* aIniCli holds VM-lifetime copies already, so the name can be aliased. */` |
|        29 |  305 | `			sSlot.sName = aCli[i].sName;` |
|        29 |  306 | `			sSlot.iAccess = VM_INI_ALL;` |
|        29 |  307 | `			sSlot.bGlobalNull = sSlot.bLocalNull = 0;` |
|        29 |  308 | `			SyBlobInit(&sSlot.sGlobal,&pVm->sAllocator);` |
|        29 |  309 | `			SyBlobInit(&sSlot.sLocal,&pVm->sAllocator);` |
|        29 |  310 | `			SyBlobAppend(&sSlot.sGlobal,aCli[i].sValue.zString,aCli[i].sValue.nByte);` |
|        29 |  311 | `			SyBlobAppend(&sSlot.sLocal,aCli[i].sValue.zString,aCli[i].sValue.nByte);` |
|        29 |  312 | `			if( SySetPut(&pVm->aIniTab,(const void *)&sSlot) != SXRET_OK ){` |
|       ! 0 |  313 | `				return SXERR_MEM;` |
|         - |  314 | `			}` |
|        14 |  315 | `		}` |
|       373 |  316 | `	}` |
|         - |  317 | `	/* Insertion sort by name (the table is ~30 entries and already nearly sorted:` |
|         - |  318 | `	 * only CLI-introduced directives are out of place). */` |
|      2240 |  319 | `	aSlot = (VmIniSlot *)SySetBasePtr(&pVm->aIniTab);` |
|    167658 |  320 | `	for( i = 1 ; i < SySetUsed(&pVm->aIniTab) ; i++ ){` |
|    165423 |  321 | `		VmIniSlot sTmp = aSlot[i];` |
|    165423 |  322 | `		j = i;` |
|    191016 |  323 | `		while( j > 0 ){` |
|    191014 |  324 | `			const VmIniSlot *pPrev = &aSlot[j-1];` |
|    191014 |  325 | `			sxu32 nMin = pPrev->sName.nByte < sTmp.sName.nByte ? pPrev->sName.nByte : sTmp.sName.nByte;` |
|    191014 |  326 | `			sxi32 iCmp = SyMemcmp(pPrev->sName.zString,sTmp.sName.zString,nMin);` |
|    191014 |  327 | `			if( iCmp == 0 ){` |
|      2240 |  328 | `				iCmp = (sxi32)pPrev->sName.nByte - (sxi32)sTmp.sName.nByte;` |
|      1114 |  329 | `			}` |
|    191014 |  330 | `			if( iCmp <= 0 ){` |
|    165421 |  331 | `				break;` |
|         - |  332 | `			}` |
|     25598 |  333 | `			aSlot[j] = aSlot[j-1];` |
|     25598 |  334 | `			j--;` |
|         5 |  335 | `		}` |
|    165423 |  336 | `		aSlot[j] = sTmp;` |
|     82455 |  337 | `	}` |
|         - |  338 | `	/* Boot-apply the CLI values for the live-wired session knobs. The engine knobs` |
|         - |  339 | `	 * (error_reporting / date.timezone) were already applied C-side by` |
|         - |  340 | `	 * PH7_VM_CONFIG_INI_ENTRY. */` |
|      2240 |  341 | `	aSlot = (VmIniSlot *)SySetBasePtr(&pVm->aIniTab);` |
|    169893 |  342 | `	for( i = 0 ; i < SySetUsed(&pVm->aIniTab) ; i++ ){` |
|    167658 |  343 | `		SyBlob *pDst = 0;` |
|    167658 |  344 | `		if( IniNameIs(&aSlot[i],"session.name") ){` |
|      2235 |  345 | `			if( SyBlobLength(&aSlot[i].sGlobal) != sizeof("PHPSESSID")-1` |
|      2238 |  346 | `			 \|\| SyMemcmp(SyBlobData(&aSlot[i].sGlobal),"PHPSESSID",sizeof("PHPSESSID")-1) != 0 ){` |
|         6 |  347 | `				pDst = &pVm->sSessName;` |
|         8 |  348 | `			}` |
|    166537 |  349 | `		}else if( IniNameIs(&aSlot[i],"session.save_path") ){` |
|      2240 |  350 | `			if( SyBlobLength(&aSlot[i].sGlobal) > 0 ){` |
|       ! 0 |  351 | `				pDst = &pVm->sSessPath;` |
|       ! 0 |  352 | `			}` |
|      1114 |  353 | `		}` |
|    167658 |  354 | `		if( pDst ){` |
|         6 |  355 | `			sxu32 nLen = SyBlobLength(&aSlot[i].sGlobal);` |
|         6 |  356 | `			const char *zVal = (const char *)SyBlobData(&aSlot[i].sGlobal);` |
|         6 |  357 | `			while( nLen > 0 && zVal[nLen-1] == '/' ){ nLen--; } /* rtrim('/') */` |
|         6 |  358 | `			SyBlobReset(pDst);` |
|         6 |  359 | `			SyBlobAppend(pDst,zVal,nLen);` |
|         3 |  360 | `		}` |
|     83569 |  361 | `	}` |
|      2240 |  362 | `	return SXRET_OK;` |
|    739055 |  363 | `}` |
|   1478220 |  364 | `static VmIniSlot * IniFind(ph7_vm *pVm,const char *zName,sxu32 nName)` |
|         5 |  365 | `{` |
|   1478225 |  366 | `	VmIniSlot *aSlot = (VmIniSlot *)SySetBasePtr(&pVm->aIniTab);` |
|         - |  367 | `	sxu32 n;` |
| 104705358 |  368 | `	for( n = 0 ; n < SySetUsed(&pVm->aIniTab) ; n++ ){` |
| 104705251 |  369 | `		if( aSlot[n].sName.nByte == nName` |
|  53827508 |  370 | `		 && SyMemcmp(aSlot[n].sName.zString,zName,nName) == 0 ){` |
|   1478123 |  371 | `			return &aSlot[n];` |
|         - |  372 | `		}` |
|  51608154 |  373 | `	}` |
|       107 |  374 | `	return 0;` |
|    739010 |  375 | `}` |
|         - |  376 | `/*` |
|         - |  377 | ` * The EFFECTIVE current value. For a live-wired directive the runtime knob is` |
|         - |  378 | ` * the truth, not the stored local value, so that ini_get() and the knob's own` |
|         - |  379 | ` * accessor can never disagree.` |
|         - |  380 | ` */` |
|   1480401 |  381 | `static void IniLiveGet(ph7_vm *pVm,VmIniSlot *pSlot,SyBlob *pOut)` |
|         5 |  382 | `{` |
|   1480406 |  383 | `	SyBlobReset(pOut);` |
|   1480406 |  384 | `	if( IniNameIs(pSlot,"error_reporting") ){` |
|         - |  385 | `		char zBuf[32];` |
|        88 |  386 | `		int nBuf = SyBufferFormat(zBuf,sizeof(zBuf),"%d",` |
|        56 |  387 | `			pVm->bErrReport ? (int)pVm->iErrMask : 0);` |
|        61 |  388 | `		SyBlobAppend(pOut,zBuf,(sxu32)nBuf);` |
|        61 |  389 | `		return;` |
|         - |  390 | `	}` |
|   1480350 |  391 | `	if( IniNameIs(pSlot,"session.name") ){` |
|        92 |  392 | `		SyBlobAppend(pOut,SyBlobData(&pVm->sSessName),SyBlobLength(&pVm->sSessName));` |
|        92 |  393 | `		return;` |
|         - |  394 | `	}` |
|   1480263 |  395 | `	if( IniNameIs(pSlot,"session.save_path") && SyBlobLength(&pVm->sSessPath) > 0 ){` |
|        11 |  396 | `		SyBlobAppend(pOut,SyBlobData(&pVm->sSessPath),SyBlobLength(&pVm->sSessPath));` |
|        11 |  397 | `		return;` |
|         - |  398 | `	}` |
|   1480253 |  399 | `	if( IniNameIs(pSlot,"include_path") ){` |
|         - |  400 | `		/* The VM's path SET is the store; this directive is a view of it, so` |
|         - |  401 | `		 * ini_get() and get_include_path() can never name different paths. */` |
|        62 |  402 | `		PH7_VmGetIncludePath(pVm,pOut);` |
|        62 |  403 | `		return;` |
|         - |  404 | `	}` |
|   1480195 |  405 | `	SyBlobAppend(pOut,SyBlobData(&pSlot->sLocal),SyBlobLength(&pSlot->sLocal));` |
|    740081 |  406 | `}` |
|         - |  407 | `/*` |
|         - |  408 | ` * Push a new value at the runtime knob behind a live-wired directive. The stored` |
|         - |  409 | ` * local value is updated by the caller either way.` |
|         - |  410 | ` */` |
|         - |  411 | `/*` |
|         - |  412 | ` * php's byte shorthand: a plain integer, optionally suffixed K, M or G (case` |
|         - |  413 | `` * insensitive, no "B"). `-1` -- and any negative -- means UNLIMITED, which is the`` |
|         - |  414 | ` * CLI default. Anything unparseable reads as 0, which php also treats as` |
|         - |  415 | ` * "allocate nothing", so it is left to say exactly that rather than being` |
|         - |  416 | ` * silently promoted to unlimited.` |
|         - |  417 | ` */` |
|        12 |  418 | `static sxu32 IniParseBytes(const char *zVal,sxu32 nVal,int *pbUnlimited)` |
|         1 |  419 | `{` |
|        13 |  420 | `	sxi64 iVal = 0;` |
|        13 |  421 | `	sxu32 n = 0;` |
|        13 |  422 | `	int bNeg = 0;` |
|        13 |  423 | `	*pbUnlimited = 0;` |
|        19 |  424 | `	while( n < nVal && (zVal[n] == ' ' \|\| zVal[n] == '\t') ){ n++; }` |
|        13 |  425 | `	if( n < nVal && (zVal[n] == '-' \|\| zVal[n] == '+') ){` |
|       ! 0 |  426 | `		bNeg = (zVal[n] == '-');` |
|       ! 0 |  427 | `		n++;` |
|       ! 0 |  428 | `	}` |
|        41 |  429 | `	while( n < nVal && zVal[n] >= '0' && zVal[n] <= '9' ){` |
|        29 |  430 | `		iVal = iVal * 10 + (zVal[n] - '0');` |
|        29 |  431 | `		if( iVal > (sxi64)0x7FFFFFFF ){ iVal = (sxi64)0x7FFFFFFF; } /* clamp: the field is 32-bit */` |
|        29 |  432 | `		n++;` |
|         1 |  433 | `	}` |
|        13 |  434 | `	if( bNeg ){` |
|       ! 0 |  435 | `		*pbUnlimited = 1;   /* php: any negative memory_limit is "no limit" */` |
|       ! 0 |  436 | `		return 0;` |
|         - |  437 | `	}` |
|        13 |  438 | `	if( n < nVal ){` |
|        13 |  439 | `		sxi64 nMul = 0;` |
|        13 |  440 | `		switch( zVal[n] ){` |
|       ! 0 |  441 | `			case 'k': case 'K': nMul = 1024; break;` |
|        13 |  442 | `			case 'm': case 'M': nMul = 1024 * 1024; break;` |
|       ! 0 |  443 | `			case 'g': case 'G': nMul = 1024 * 1024 * 1024; break;` |
|       ! 0 |  444 | `			default: nMul = 0; break;` |
|         - |  445 | `		}` |
|        13 |  446 | `		if( nMul > 0 ){` |
|        13 |  447 | `			iVal = (iVal > (sxi64)0x7FFFFFFF / nMul) ? (sxi64)0x7FFFFFFF : iVal * nMul;` |
|         6 |  448 | `		}` |
|         6 |  449 | `	}` |
|        13 |  450 | `	return (sxu32)iVal;` |
|         7 |  451 | `}` |
|         - |  452 | `/*` |
|         - |  453 | ` * Arm the allocator's total live-byte ceiling from a memory_limit value, and answer` |
|         - |  454 | ` * whether it took. THE one place the directive is interpreted: ini_set() reaches it` |
|         - |  455 | `` * through the validator and `-d name=value` reaches it directly, and a rule that`` |
|         - |  456 | ` * lived in only one of those would hold for one door and not the other.` |
|         - |  457 | ` *` |
|         - |  458 | ` * The one directive that reaches into the ALLOCATOR. php enforces a ceiling and` |
|         - |  459 | ` * kills the script with a fatal when a request would cross it; PHL stored the` |
|         - |  460 | ` * string and enforced nothing, so a runaway allocation -- a reference cycle nothing` |
|         - |  461 | ` * reclaims is the usual way in -- had no ceiling below the kernel's, and` |
|         - |  462 | ` * the OOM killer took the whole process instead of the script. On a shared box that` |
|         - |  463 | ` * is not the script's problem any more: it is everything else's.` |
|         - |  464 | ` *` |
|         - |  465 | ` * Unlimited is the CLI default here as it is in php, so a plain run is unchanged.` |
|         - |  466 | ` */` |
|        12 |  467 | `PH7_PRIVATE int PH7_VmApplyMemoryLimit(ph7_vm *pVm,const char *zVal,sxu32 nVal)` |
|         1 |  468 | `{` |
|        13 |  469 | `	int bUnlimited = 0;` |
|        13 |  470 | `	sxu32 nBytes = IniParseBytes(zVal,nVal,&bUnlimited);` |
|        13 |  471 | `	if( !bUnlimited && nBytes > 0 && nBytes < pVm->sAllocator.nMemUsed ){` |
|         - |  472 | `		/* php REFUSES to lower the ceiling below what is already in use --` |
|         - |  473 | `		 * zend_set_memory_limit answers FAILURE -- and it does so from the` |
|         - |  474 | `		 * directive's own OnUpdate handler, which is why the rule holds at STARTUP` |
|         - |  475 | ``		 * (`-d memory_limit=8K` warns and runs on) exactly as it holds for`` |
|         - |  476 | `		 * ini_set(). Arming a ceiling the interpreter is already past is not a` |
|         - |  477 | `		 * limit, it is a delayed crash: the very next allocation is fatal.` |
|         - |  478 | `		 *` |
|         - |  479 | `		 * Without the rule, a library that PROBES the limit by setting a small one` |
|         - |  480 | `		 * dies instead of learning that it cannot -- monolog's StreamHandler sizes` |
|         - |  481 | `		 * its write chunk exactly that way, and its test walks 1M, 10M, 1024M, 3G` |
|         - |  482 | `		 * in turn, reading the false back and skipping.` |
|         - |  483 | `		 *` |
|         - |  484 | ``		 * php prints this one WITHOUT the `ini_set(): ` prefix its other ini`` |
|         - |  485 | `		 * warnings carry, because it comes from the handler and not from the call. */` |
|         - |  486 | `		char zMsg[160];` |
|       ! 0 |  487 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|         - |  488 | `			"Failed to set memory limit to %u bytes (Current memory usage is %u bytes)",` |
|       ! 0 |  489 | `			nBytes,pVm->sAllocator.nMemUsed);` |
|       ! 0 |  490 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);` |
|       ! 0 |  491 | `		return 0;` |
|         - |  492 | `	}` |
|        13 |  493 | `	pVm->sAllocator.nMemLimit = bUnlimited ? 0 : nBytes;` |
|        13 |  494 | `	pVm->sAllocator.nMemLimitHit = 0;` |
|        13 |  495 | `	pVm->sAllocator.nMemTried = 0;` |
|        13 |  496 | `	return 1;` |
|         7 |  497 | `}` |
|       337 |  498 | `static void IniLiveSet(ph7_vm *pVm,VmIniSlot *pSlot,const char *zVal,sxu32 nVal)` |
|         5 |  499 | `{` |
|       342 |  500 | `	if( IniNameIs(pSlot,"error_reporting") ){` |
|         3 |  501 | `		sxi32 iVal = 0;` |
|         3 |  502 | `		if( nVal > 0 ){` |
|         3 |  503 | `			SyStrToInt32(zVal,nVal,(void *)&iVal,0);` |
|         1 |  504 | `		}` |
|         3 |  505 | `		pVm->iErrMask = iVal;` |
|         3 |  506 | `		pVm->bErrReport = iVal != 0;` |
|         3 |  507 | `		pVm->bErrMaskSet = 1;` |
|         3 |  508 | `		return;` |
|         - |  509 | `	}` |
|       340 |  510 | `	if( IniNameIs(pSlot,"memory_limit") ){` |
|         5 |  511 | `		(void)PH7_VmApplyMemoryLimit(pVm,zVal,nVal);` |
|         5 |  512 | `		return;` |
|         - |  513 | `	}` |
|       336 |  514 | `	if( IniNameIs(pSlot,"display_errors") ){` |
|         5 |  515 | `		pVm->iDisplayErrors = PH7_VmDisplayErrorsMode(zVal,nVal);` |
|         5 |  516 | `		return;` |
|         - |  517 | `	}` |
|       332 |  518 | `	if( IniNameIs(pSlot,"log_errors") ){` |
|       ! 0 |  519 | `		pVm->bLogErrors = IniTruthy(zVal,nVal);` |
|       ! 0 |  520 | `		return;` |
|         - |  521 | `	}` |
|       332 |  522 | `	if( IniNameIs(pSlot,"error_log") ){` |
|         - |  523 | `		/* Where the LOG copy goes. php re-reads the directive per message rather` |
|         - |  524 | `		 * than holding the file open, so a script that moves it mid-run moves the` |
|         - |  525 | `		 * next line -- and clearing it hands the rest of the run back to the` |
|         - |  526 | `		 * error stream. */` |
|        19 |  527 | `		SyBlobReset(&pVm->sErrLogPath);` |
|        19 |  528 | `		if( nVal > 0 ){` |
|        11 |  529 | `			SyBlobAppend(&pVm->sErrLogPath,zVal,nVal);` |
|         4 |  530 | `		}` |
|        19 |  531 | `		SyBlobNullAppend(&pVm->sErrLogPath);` |
|        19 |  532 | `		return;` |
|         - |  533 | `	}` |
|       316 |  534 | `	if( IniNameIs(pSlot,"session.name") \|\| IniNameIs(pSlot,"session.save_path") ){` |
|        58 |  535 | `		int bPath = IniNameIs(pSlot,"session.save_path");` |
|        58 |  536 | `		SyBlob *pDst = bPath ? &pVm->sSessPath : &pVm->sSessName;` |
|        58 |  537 | `		sxu32 nLen = nVal;` |
|        58 |  538 | `		if( bPath ){` |
|        32 |  539 | `			while( nLen > 0 && zVal[nLen-1] == '/' ){ nLen--; }` |
|        15 |  540 | `		}` |
|        58 |  541 | `		SyBlobReset(pDst);` |
|        58 |  542 | `		SyBlobAppend(pDst,zVal,nLen);` |
|        58 |  543 | `		return;` |
|         - |  544 | `	}` |
|       260 |  545 | `	if( IniNameIs(pSlot,"include_path") ){` |
|         - |  546 | `		/* The one write that moves the include walk. Refused empty by the caller` |
|         - |  547 | `		 * (php's OnUpdateStringUnempty), so nVal is never 0 on the ini_set() path;` |
|         - |  548 | `		 * the boot/-d and ini_restore() paths carry a real value too. */` |
|         5 |  549 | `		PH7_VmSetIncludePath(pVm,zVal,nVal);` |
|         5 |  550 | `		return;` |
|         - |  551 | `	}` |
|       256 |  552 | `	if( IniNameIs(pSlot,"date.timezone") ){` |
|         - |  553 | `		/* Validated already (IniValueAccepted), so what arrives here is a name` |
|         - |  554 | `		 * that resolves. It moves the default UNLESS a script has already named` |
|         - |  555 | `		 * one outright: php latches on date_default_timezone_set(), after which` |
|         - |  556 | `		 * the directive still records what it is handed and the default no` |
|         - |  557 | `		 * longer follows it. The stored spelling is the caller's own bytes. */` |
|         9 |  558 | `		if( pVm->bDefTzExplicit ){` |
|         5 |  559 | `			return;` |
|         - |  560 | `		}` |
|         5 |  561 | `		if( nVal > 0 && nVal < sizeof(pVm->zDefTz) ){` |
|         5 |  562 | `			SyMemcpy(zVal,pVm->zDefTz,nVal);` |
|         5 |  563 | `			pVm->zDefTz[nVal] = 0;` |
|         5 |  564 | `			pVm->nDefTz = nVal;` |
|         2 |  565 | `		}` |
|         4 |  566 | `		return;` |
|         - |  567 | `	}` |
|       168 |  568 | `}` |
|         - |  569 | `/*` |
|         - |  570 | ` * A session directive is settable only while there is no session to disturb: not` |
|         - |  571 | ` * once one is ACTIVE (the store is open and the cookie decided), and not once` |
|         - |  572 | ` * headers have gone out. Answers TRUE (having raised the warning) when the write` |
|         - |  573 | ` * must be refused.` |
|         - |  574 | ` */` |
|       302 |  575 | `static int IniSessionLocked(ph7_context *pCtx,VmIniSlot *pSlot,const char *zFunc)` |
|         5 |  576 | `{` |
|       307 |  577 | `	ph7_vm *pVm = pCtx->pVm;` |
|         - |  578 | `	char zMsg[160];` |
|         - |  579 | `	const char *zWhy;` |
|       302 |  580 | `	if( pSlot->sName.nByte < sizeof("session.")-1` |
|       305 |  581 | `	 \|\| SyMemcmp(pSlot->sName.zString,"session.",sizeof("session.")-1) != 0 ){` |
|       143 |  582 | `		return 0;` |
|         - |  583 | `	}` |
|       168 |  584 | `	int bActive = (pVm->iSessStatus == 2 /* PHP_SESSION_ACTIVE */);` |
|       168 |  585 | `	if( bActive ){` |
|         5 |  586 | `		zWhy = "when a session is active";` |
|       166 |  587 | `	}else if( pVm->bHeadersSent ){` |
|         3 |  588 | `		zWhy = "after headers have already been sent";` |
|         2 |  589 | `	}else{` |
|       162 |  590 | `		return 0;` |
|         - |  591 | `	}` |
|        11 |  592 | `	SyBufferFormat(zMsg,sizeof(zMsg),` |
|         3 |  593 | `		"%s(): Session ini settings cannot be changed %s",zFunc,zWhy);` |
|         - |  594 | `	{` |
|         - |  595 | `		/* php names the session_start() or the output behind the refusal. */` |
|         - |  596 | `		SyBlob sMsg;` |
|         8 |  597 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|         8 |  598 | `		SyBlobAppend(&sMsg,zMsg,(sxu32)SyStrlen(zMsg));` |
|         8 |  599 | `		PH7_VmAppendWhere(pVm,&sMsg,bActive);` |
|         8 |  600 | `		SyBlobNullAppend(&sMsg);` |
|         8 |  601 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|         8 |  602 | `		SyBlobRelease(&sMsg);` |
|         - |  603 | `	}` |
|         8 |  604 | `	return 1;` |
|       149 |  605 | `}` |
|         - |  606 | `/*` |
|         - |  607 | ` * The per-directive value rules php enforces on every write, and the diagnostic` |
|         - |  608 | ` * each one raises. zWho is the whole prefix php puts on it -- "ini_set()" or, when` |
|         - |  609 | ` * session_start() is applying its $options array, "session_start()" -- because php` |
|         - |  610 | ` * blames the call that made the write, not the API underneath it.` |
|         - |  611 | ` */` |
|       360 |  612 | `static int IniValueAccepted(ph7_vm *pVm,VmIniSlot *pSlot,const char *zVal,sxu32 nVal,` |
|         - |  613 | `	const char *zWho)` |
|         5 |  614 | `{` |
|         - |  615 | `	char zMsg[256];` |
|       365 |  616 | `	if( IniNameIs(pSlot,"include_path") && nVal < 1 ){` |
|         - |  617 | `		/* php registers include_path with OnUpdateStringUnempty: the EMPTY value` |
|         - |  618 | `		 * is refused in silence and the directive keeps what it had. */` |
|         3 |  619 | `		return 0;` |
|         - |  620 | `	}` |
|       363 |  621 | `	if( IniNameIs(pSlot,"memory_limit") ){` |
|         - |  622 | `		/* One rule, one warning, one place: the applier owns both, so ini_set() and` |
|         - |  623 | ``		 * the `-d` startup path cannot drift apart. Arming from the validator is`` |
|         - |  624 | `		 * idempotent -- an ACCEPTED value is then written and re-applied through` |
|         - |  625 | `		 * IniLiveSet with the same bytes, and a refused one is never written. */` |
|         5 |  626 | `		return PH7_VmApplyMemoryLimit(pVm,zVal,nVal);` |
|         - |  627 | `	}` |
|       359 |  628 | `	if( IniNameIs(pSlot,"date.timezone") ){` |
|         - |  629 | `		/* The directive and date_default_timezone_set() are one rule, and this is` |
|         - |  630 | `		 * the place php enforces it on every write: a name neither table resolves` |
|         - |  631 | `		 * is REFUSED, with a warning that names the value it kept instead, and` |
|         - |  632 | `		 * the directive is left alone. Same shape as memory_limit above -- the` |
|         - |  633 | ``		 * validator owns it so `ini_set()` and the `-d` startup path cannot`` |
|         - |  634 | `		 * drift apart. */` |
|        14 |  635 | `		int bOk = (nVal == 3` |
|        13 |  636 | `		        && (SyStrnicmp(zVal,"UTC",3) == 0 \|\| SyStrnicmp(zVal,"GMT",3) == 0));` |
|         - |  637 | `#ifdef PH7_ENABLE_TZDB` |
|        13 |  638 | `		if( !bOk ){` |
|        13 |  639 | `			bOk = nVal > 0 && nVal < sizeof(pVm->zDefTz) && PH7_TzFind(zVal,(int)nVal) >= 0;` |
|         6 |  640 | `		}` |
|         - |  641 | `#endif` |
|        13 |  642 | `		if( !bOk ){` |
|         7 |  643 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|         - |  644 | `				"%s: Invalid date.timezone value '%.*s', using '%.*s' instead",` |
|         4 |  645 | `				zWho,(int)nVal,zVal,(int)pVm->nDefTz,pVm->zDefTz);` |
|         5 |  646 | `			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);` |
|         5 |  647 | `			return 0;` |
|         - |  648 | `		}` |
|         9 |  649 | `		return 1;` |
|         - |  650 | `	}` |
|       342 |  651 | `	if( IniNameIs(pSlot,"syslog.filter")` |
|       180 |  652 | `	 && !(nVal == 3 && SyMemcmp(zVal,"all",3) == 0)` |
|        12 |  653 | `	 && !(nVal == 7 && SyMemcmp(zVal,"no-ctrl",7) == 0)` |
|         8 |  654 | `	 && !(nVal == 5 && SyMemcmp(zVal,"ascii",5) == 0)` |
|        11 |  655 | `	 && !(nVal == 3 && SyMemcmp(zVal,"raw",3) == 0) ){` |
|         - |  656 | `		/* php names the four modes in its OnUpdate handler and refuses anything` |
|         - |  657 | `		 * else in silence, keeping what the directive had -- so` |
|         - |  658 | ``		 * `ini_set('syslog.filter','bogus')` is false and reads back unchanged.`` |
|         - |  659 | ``		 * The match is CASE-SENSITIVE there: `ASCII` is refused where `ascii` is`` |
|         - |  660 | `		 * taken, which is not what most of php's word-valued directives do. */` |
|         3 |  661 | `		return 0;` |
|         - |  662 | `	}` |
|       344 |  663 | `	if( IniNameIs(pSlot,"bcmath.scale") ){` |
|         - |  664 | `		/* php registers it with a 0..INT_MAX bound and refuses anything outside in` |
|         - |  665 | `` 		 * silence, keeping what the directive had -- `ini_set('bcmath.scale','-1')` `` |
|         - |  666 | `		 * is false there. A NON-numeric value is a different matter and is` |
|         - |  667 | `		 * ACCEPTED (stored verbatim, read back as 0), which falls out of the parse` |
|         - |  668 | `		 * below without a rule of its own. */` |
|        25 |  669 | `		sxi64 iVal = 0;` |
|        25 |  670 | `		SyStrToInt64(zVal,nVal,(void *)&iVal,0);` |
|        25 |  671 | `		if( iVal < 0 \|\| iVal > 2147483647 ){` |
|         5 |  672 | `			return 0;` |
|         - |  673 | `		}` |
|        10 |  674 | `	}` |
|       340 |  675 | `	if( IniNameIs(pSlot,"zend.exception_string_param_max_len") ){` |
|         - |  676 | `		/* php bounds it to 0..1000000 and refuses anything outside in silence; a` |
|         - |  677 | `		 * NON-numeric value is taken, as bcmath.scale's is, and reads as 0. */` |
|        17 |  678 | `		sxi64 iVal = 0;` |
|        17 |  679 | `		SyStrToInt64(zVal,nVal,(void *)&iVal,0);` |
|        17 |  680 | `		if( iVal < 0 \|\| iVal > 1000000 ){` |
|         5 |  681 | `			return 0;` |
|         - |  682 | `		}` |
|         6 |  683 | `	}` |
|       331 |  684 | `	if( IniNameIs(pSlot,"session.serialize_handler")` |
|       168 |  685 | `	 && !(nVal == 3 && SyMemcmp(zVal,"php",3) == 0)` |
|        10 |  686 | `	 && !(nVal == 10 && SyMemcmp(zVal,"php_binary",10) == 0)` |
|        10 |  687 | `	 && !(nVal == 13 && SyMemcmp(zVal,"php_serialize",13) == 0) ){` |
|         - |  688 | `		/* php looks the name up in its registered serializer list and refuses what` |
|         - |  689 | `		 * it cannot find, keeping the directive where it was. */` |
|         8 |  690 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|         2 |  691 | `			"%s: Serialization handler \"%.*s\" cannot be found",zWho,(int)nVal,zVal);` |
|         6 |  692 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);` |
|         6 |  693 | `		return 0;` |
|         - |  694 | `	}` |
|       328 |  695 | `	if( IniNameIs(pSlot,"session.name") ){` |
|         - |  696 | `		/* The name goes out as a COOKIE name and comes back as one, so php holds it` |
|         - |  697 | `		 * to the cookie alphabet -- and refuses a numeric one, which a browser would` |
|         - |  698 | `		 * hand back as an integer array key. */` |
|         - |  699 | `		static const char zBad[] = "=,;.[ \t\r\n\013\014";` |
|         - |  700 | `		sxu32 i;` |
|        36 |  701 | `		int bBad = nVal < 1;` |
|       238 |  702 | `		for( i = 0 ; !bBad && i < nVal ; i++ ){` |
|       204 |  703 | `			if( zVal[i] == 0 \|\| SyByteFind(zBad,sizeof(zBad)-1,zVal[i],0) == SXRET_OK ){` |
|         3 |  704 | `				bBad = 1;` |
|         1 |  705 | `			}` |
|       103 |  706 | `		}` |
|        36 |  707 | `		if( !bBad ){` |
|        32 |  708 | `			sxi64 iDummy = 0;` |
|        32 |  709 | `			bBad = SyStrToInt64(zVal,nVal,(void *)&iDummy,0) == SXRET_OK;` |
|        15 |  710 | `		}` |
|        36 |  711 | `		if( bBad ){` |
|        16 |  712 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|         - |  713 | `				"%s: session.name \"%.*s\" must not be numeric, empty, contain null bytes"` |
|         - |  714 | `				" or any of the following characters \"=,;.[ \\t\\r\\n\\013\\014\"",` |
|         5 |  715 | `				zWho,(int)nVal,zVal);` |
|        11 |  716 | `			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);` |
|        11 |  717 | `			return 0;` |
|         - |  718 | `		}` |
|        12 |  719 | `	}` |
|       318 |  720 | `	return 1;` |
|       176 |  721 | `}` |
|         - |  722 | `/*` |
|         - |  723 | ` * Store one written value.  A write always leaves a local value; what it does to` |
|         - |  724 | ` * the GLOBAL one depends on whether there was ever a global to keep. php saves` |
|         - |  725 | ` * the original at the first modification and reports THAT as global_value -- but` |
|         - |  726 | ` * an UNSET directive has no original to save, so its global_value starts reading` |
|         - |  727 | ` * the written value instead, and only ini_restore() puts the NULL back.` |
|         - |  728 | ` */` |
|       325 |  729 | `static void IniWriteLocal(VmIniSlot *pSlot,const char *zVal,sxu32 nVal)` |
|         5 |  730 | `{` |
|       330 |  731 | `	SyBlobReset(&pSlot->sLocal);` |
|       330 |  732 | `	SyBlobAppend(&pSlot->sLocal,zVal,nVal);` |
|       330 |  733 | `	pSlot->bLocalNull = 0;` |
|       330 |  734 | `}` |
|         - |  735 | `/*` |
|         - |  736 | ` * What global_value reports: the saved original, unless there was none -- in` |
|         - |  737 | ` * which case it is whatever the directive currently holds, NULL included.` |
|         - |  738 | ` */` |
|      2381 |  739 | `static int IniGlobalValue(VmIniSlot *pSlot,const char **pz,sxu32 *pn)` |
|         5 |  740 | `{` |
|      2386 |  741 | `	if( !pSlot->bGlobalNull ){` |
|      2195 |  742 | `		*pz = (const char *)SyBlobData(&pSlot->sGlobal);` |
|      2195 |  743 | `		*pn = SyBlobLength(&pSlot->sGlobal);` |
|      2195 |  744 | `		return 1;` |
|         - |  745 | `	}` |
|       194 |  746 | `	if( pSlot->bLocalNull ){` |
|       190 |  747 | `		return 0;   /* still unset: php reports NULL */` |
|         - |  748 | `	}` |
|         5 |  749 | `	*pz = (const char *)SyBlobData(&pSlot->sLocal);` |
|         5 |  750 | `	*pn = SyBlobLength(&pSlot->sLocal);` |
|         5 |  751 | `	return 1;` |
|      1158 |  752 | `}` |
|         - |  753 | `/*` |
|         - |  754 | ` * Write a directive from C, the way ini_set() writes it. Answers 0 when the write` |
|         - |  755 | ` * was refused (unknown name, not user-settable, or a value the directive's own` |
|         - |  756 | ` * rule rejects) -- which is exactly what session_start()'s $options reports as` |
|         - |  757 | `` * `Setting option "%s" failed`.`` |
|         - |  758 | ` */` |
|        74 |  759 | `PH7_PRIVATE int PH7_VmIniSet(ph7_vm *pVm,const char *zName,sxu32 nName,` |
|         - |  760 | `	const char *zVal,sxu32 nVal,const char *zWho)` |
|         5 |  761 | `{` |
|         - |  762 | `	VmIniSlot *pSlot;` |
|        79 |  763 | `	if( IniSeed(pVm) != SXRET_OK ){` |
|       ! 0 |  764 | `		return 0;` |
|         - |  765 | `	}` |
|        79 |  766 | `	pSlot = IniFind(pVm,zName,nName);` |
|        79 |  767 | `	if( pSlot == 0 \|\| (pSlot->iAccess & VM_INI_USER) == 0 ){` |
|         3 |  768 | `		return 0;` |
|         - |  769 | `	}` |
|        77 |  770 | `	if( !IniValueAccepted(pVm,pSlot,zVal,nVal,zWho) ){` |
|         7 |  771 | `		return 0;` |
|         - |  772 | `	}` |
|        71 |  773 | `	IniWriteLocal(pSlot,zVal,nVal);` |
|        71 |  774 | `	IniLiveSet(pVm,pSlot,zVal,nVal);` |
|        71 |  775 | `	return 1;` |
|        42 |  776 | `}` |
|         - |  777 | `/* string\|false ini_get(string $option) */` |
|      1418 |  778 | `static int vm_builtin_ini_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 |  779 | `{` |
|      1423 |  780 | `	ph7_vm *pVm = pCtx->pVm;` |
|         - |  781 | `	VmIniSlot *pSlot;` |
|         - |  782 | `	const char *zName;` |
|      1423 |  783 | `	int nName = 0;` |
|         - |  784 | `	SyBlob sOut;` |
|      1423 |  785 | `	if( nArg < 1 \|\| IniSeed(pVm) != SXRET_OK ){` |
|       ! 0 |  786 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 |  787 | `		return PH7_OK;` |
|         - |  788 | `	}` |
|      1423 |  789 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|      1423 |  790 | `	pSlot = IniFind(pVm,zName,(sxu32)nName);` |
|      1423 |  791 | `	if( pSlot == 0 ){` |
|        13 |  792 | `		ph7_result_bool(pCtx,0);` |
|        13 |  793 | `		return PH7_OK;` |
|         - |  794 | `	}` |
|      1413 |  795 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      1413 |  796 | `	IniLiveGet(pVm,pSlot,&sOut);` |
|      1413 |  797 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|      1413 |  798 | `	SyBlobRelease(&sOut);` |
|      1413 |  799 | `	return PH7_OK;` |
|       710 |  800 | `}` |
|         - |  801 | `/* string\|false ini_set(string $option, string\|int\|float\|bool\|null $value) */` |
|       300 |  802 | `static int vm_builtin_ini_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 |  803 | `{` |
|       305 |  804 | `	ph7_vm *pVm = pCtx->pVm;` |
|         - |  805 | `	VmIniSlot *pSlot;` |
|         - |  806 | `	const char *zName;` |
|         - |  807 | `	const char *zVal;` |
|       305 |  808 | `	int nName = 0, nVal = 0;` |
|         - |  809 | `	SyBlob sOld;` |
|         - |  810 | `	char zWho[64];` |
|       305 |  811 | `	if( nArg < 2 \|\| IniSeed(pVm) != SXRET_OK ){` |
|       ! 0 |  812 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 |  813 | `		return PH7_OK;` |
|         - |  814 | `	}` |
|       305 |  815 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|       305 |  816 | `	pSlot = IniFind(pVm,zName,(sxu32)nName);` |
|       305 |  817 | `	if( pSlot == 0 \|\| (pSlot->iAccess & VM_INI_USER) == 0 ){` |
|        15 |  818 | `		ph7_result_bool(pCtx,0);` |
|        15 |  819 | `		return PH7_OK;` |
|         - |  820 | `	}` |
|       293 |  821 | `	if( IniSessionLocked(pCtx,pSlot,ph7_function_name(pCtx)) ){` |
|         6 |  822 | `		ph7_result_bool(pCtx,0);` |
|         6 |  823 | `		return PH7_OK;` |
|         - |  824 | `	}` |
|         - |  825 | `	/* php: the -1 (compiled-out) state of zend.assertions is a php.ini-only switch,` |
|         - |  826 | `	 * and so is moving INTO it. Unprefixed warning, exactly as php prints it. */` |
|       289 |  827 | `	if( IniNameIs(pSlot,"zend.assertions") ){` |
|       ! 0 |  828 | `		int bGlobalOff = SyBlobLength(&pSlot->sGlobal) == 2` |
|       ! 0 |  829 | `			&& SyMemcmp(SyBlobData(&pSlot->sGlobal),"-1",2) == 0;` |
|       ! 0 |  830 | `		const char *zNew = ph7_value_to_string(apArg[1],&nVal);` |
|       ! 0 |  831 | `		int bNewOff = nVal == 2 && SyMemcmp(zNew,"-1",2) == 0;` |
|       ! 0 |  832 | `		if( bGlobalOff \|\| bNewOff ){` |
|       ! 0 |  833 | `			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|         - |  834 | `				"zend.assertions may be completely enabled or disabled only in php.ini");` |
|       ! 0 |  835 | `			ph7_result_bool(pCtx,0);` |
|       ! 0 |  836 | `			return PH7_OK;` |
|         - |  837 | `		}` |
|       ! 0 |  838 | `	}` |
|         - |  839 | `	/* The OLD value php answers is the effective one, read before the write. */` |
|       289 |  840 | `	SyBlobInit(&sOld,&pVm->sAllocator);` |
|       289 |  841 | `	IniLiveGet(pVm,pSlot,&sOld);` |
|         - |  842 | `	/* php stringifies the incoming value, with a bool becoming "1"/"" . */` |
|       289 |  843 | `	if( ph7_value_is_bool(apArg[1]) ){` |
|       ! 0 |  844 | `		zVal = ph7_value_to_bool(apArg[1]) ? "1" : "";` |
|       ! 0 |  845 | `		nVal = (int)SyStrlen(zVal);` |
|       ! 0 |  846 | `	}else{` |
|       289 |  847 | `		zVal = ph7_value_to_string(apArg[1],&nVal);` |
|         - |  848 | `	}` |
|       289 |  849 | `	SyBufferFormat(zWho,sizeof(zWho),"%s()",ph7_function_name(pCtx));` |
|       289 |  850 | `	if( !IniValueAccepted(pVm,pSlot,zVal,(sxu32)nVal,zWho) ){` |
|        29 |  851 | `		SyBlobRelease(&sOld);` |
|        29 |  852 | `		ph7_result_bool(pCtx,0);` |
|        29 |  853 | `		return PH7_OK;` |
|         - |  854 | `	}` |
|       264 |  855 | `	IniWriteLocal(pSlot,zVal,(sxu32)nVal);` |
|       264 |  856 | `	IniLiveSet(pVm,pSlot,zVal,(sxu32)nVal);` |
|       264 |  857 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOld),(int)SyBlobLength(&sOld));` |
|       264 |  858 | `	SyBlobRelease(&sOld);` |
|       264 |  859 | `	return PH7_OK;` |
|       147 |  860 | `}` |
|         - |  861 | `/* void ini_restore(string $option) */` |
|        14 |  862 | `static int vm_builtin_ini_restore(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 |  863 | `{` |
|        17 |  864 | `	ph7_vm *pVm = pCtx->pVm;` |
|         - |  865 | `	VmIniSlot *pSlot;` |
|         - |  866 | `	const char *zName;` |
|        17 |  867 | `	int nName = 0;` |
|        17 |  868 | `	ph7_result_null(pCtx);` |
|        17 |  869 | `	if( nArg < 1 \|\| IniSeed(pVm) != SXRET_OK ){` |
|       ! 0 |  870 | `		return PH7_OK;` |
|         - |  871 | `	}` |
|        17 |  872 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|        17 |  873 | `	pSlot = IniFind(pVm,zName,(sxu32)nName);` |
|        17 |  874 | `	if( pSlot == 0 \|\| IniSessionLocked(pCtx,pSlot,"ini_restore") ){` |
|         3 |  875 | `		return PH7_OK;` |
|         - |  876 | `	}` |
|        14 |  877 | `	SyBlobReset(&pSlot->sLocal);` |
|        14 |  878 | `	SyBlobAppend(&pSlot->sLocal,SyBlobData(&pSlot->sGlobal),SyBlobLength(&pSlot->sGlobal));` |
|        14 |  879 | `	pSlot->bLocalNull = pSlot->bGlobalNull;   /* an unset directive goes back to unset */` |
|        14 |  880 | `	IniLiveSet(pVm,pSlot,(const char *)SyBlobData(&pSlot->sGlobal),SyBlobLength(&pSlot->sGlobal));` |
|        14 |  881 | `	return PH7_OK;` |
|        10 |  882 | `}` |
|         - |  883 | `/*` |
|         - |  884 | ` * The extension id php's module registry would find for this name: the key it` |
|         - |  885 | ` * stores is the extension name FOLDED, and the lookup against it is exact.` |
|         - |  886 | ` */` |
|        58 |  887 | `static int VmIniExtensionId(const char *zName,int nName)` |
|         4 |  888 | `{` |
|         - |  889 | `	int iExt,n;` |
|       982 |  890 | `	for( iExt = 0 ; iExt < PH7_VmExtensionCount() ; ++iExt ){` |
|         - |  891 | `		const char *zCanon;` |
|       966 |  892 | `		if( !PH7_VmExtensionAvailable(iExt) ){` |
|         2 |  893 | `			continue;` |
|         - |  894 | `		}` |
|       966 |  895 | `		zCanon = PH7_VmExtensionName(iExt);` |
|       966 |  896 | `		if( (int)SyStrlen(zCanon) != nName ){` |
|       830 |  897 | `			continue;` |
|         - |  898 | `		}` |
|       392 |  899 | `		for( n = 0 ; n < nName ; ++n ){` |
|       350 |  900 | `			if( (char)SyToLower(zCanon[n]) != zName[n] ){` |
|        96 |  901 | `				break;` |
|         - |  902 | `			}` |
|       130 |  903 | `		}` |
|       140 |  904 | `		if( n == nName ){` |
|        46 |  905 | `			return iExt;` |
|         - |  906 | `		}` |
|        49 |  907 | `	}` |
|        17 |  908 | `	return -1;` |
|        33 |  909 | `}` |
|         - |  910 | `/* array\|false ini_get_all(?string $extension = null, bool $details = true) */` |
|        91 |  911 | `static int vm_builtin_ini_get_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 |  912 | `{` |
|        96 |  913 | `	ph7_vm *pVm = pCtx->pVm;` |
|         - |  914 | `	VmIniSlot *aSlot;` |
|         - |  915 | `	ph7_value *pOut,*pCur;` |
|        96 |  916 | `	const char *zExt = 0;` |
|        96 |  917 | `	int nExt = 0, bDetails = 1, iExtSel = -1;` |
|         - |  918 | `	sxu32 n;` |
|         - |  919 | `	SyBlob sVal;` |
|        96 |  920 | `	if( IniSeed(pVm) != SXRET_OK ){` |
|       ! 0 |  921 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 |  922 | `		return PH7_OK;` |
|         - |  923 | `	}` |
|        96 |  924 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|        62 |  925 | `		zExt = ph7_value_to_string(apArg[0],&nExt);` |
|        29 |  926 | `	}` |
|        96 |  927 | `	if( nArg > 1 ){` |
|        30 |  928 | `		bDetails = ph7_value_to_bool(apArg[1]);` |
|        14 |  929 | `	}` |
|        96 |  930 | `	if( zExt ){` |
|         - |  931 | `		/* php looks the name up in the MODULE REGISTRY, whose key is the extension` |
|         - |  932 | `		 * name folded down -- so the match is case-SENSITIVE against that key:` |
|         - |  933 | ``		 * `core` and `spl` are found, `Core` and `SPL` are not, and every other`` |
|         - |  934 | `		 * name is a warning and false. An extension that owns no directive is` |
|         - |  935 | ``		 * still found and answers the EMPTY array; a `phl.stub_extensions` name`` |
|         - |  936 | `		 * is no module and is not found. This engine used to accept five names` |
|         - |  937 | `		 * spelled its own way and refuse the rest. */` |
|        62 |  938 | `		iExtSel = VmIniExtensionId(zExt,nExt);` |
|        62 |  939 | `		if( iExtSel < 0 ){` |
|        25 |  940 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         8 |  941 | `				"Extension \"%.*s\" cannot be found",nExt,zExt);` |
|        17 |  942 | `			ph7_result_bool(pCtx,0);` |
|        17 |  943 | `			return PH7_OK;` |
|         - |  944 | `		}` |
|        21 |  945 | `	}` |
|        80 |  946 | `	pOut = ph7_context_new_array(pCtx);` |
|        80 |  947 | `	pCur = ph7_context_new_scalar(pCtx);` |
|        80 |  948 | `	if( pOut == 0 \|\| pCur == 0 ){` |
|       ! 0 |  949 | `		return PH7_ContextMemoryError(pCtx);` |
|         - |  950 | `	}` |
|        80 |  951 | `	SyBlobInit(&sVal,&pVm->sAllocator);` |
|        80 |  952 | `	aSlot = (VmIniSlot *)SySetBasePtr(&pVm->aIniTab);` |
|         - |  953 | `	/* The table is stored sorted, so this walk is already php's ksort order. */` |
|      5705 |  954 | `	for( n = 0 ; n < SySetUsed(&pVm->aIniTab) ; n++ ){` |
|      5630 |  955 | `		VmIniSlot *pSlot = &aSlot[n];` |
|         - |  956 | `		char zKey[128];` |
|         - |  957 | `		/* A directive belongs where the extension partition puts it rather than` |
|         - |  958 | `` 		 * where its NAME points -- php's `standard` owns `session.trans_sid_tags` `` |
|         - |  959 | ``		 * and its `Core` owns none of the `session.*` ones. `core` is php's own`` |
|         - |  960 | `		 * exception and answers EVERY directive whatever module registered it. */` |
|      5625 |  961 | `		if( iExtSel >= 0 && iExtSel != PH7_EXT_CORE` |
|      2930 |  962 | `		 && PH7_VmExtOfIni(pSlot->sName.zString,(int)pSlot->sName.nByte) != iExtSel ){` |
|      2498 |  963 | `			continue;` |
|         - |  964 | `		}` |
|      3136 |  965 | `		if( pSlot->sName.nByte >= sizeof(zKey) ){` |
|       ! 0 |  966 | `			continue;` |
|         - |  967 | `		}` |
|      3136 |  968 | `		SyMemcpy(pSlot->sName.zString,zKey,pSlot->sName.nByte);` |
|      3136 |  969 | `		zKey[pSlot->sName.nByte] = 0;` |
|      3136 |  970 | `		IniLiveGet(pVm,pSlot,&sVal);` |
|      3136 |  971 | `		if( bDetails ){` |
|      2386 |  972 | `			ph7_value *pRow = ph7_context_new_array(pCtx);` |
|      2386 |  973 | `			if( pRow == 0 ){` |
|       ! 0 |  974 | `				break;` |
|         - |  975 | `			}` |
|         - |  976 | `			{` |
|      2386 |  977 | `				const char *zG = 0;` |
|      2386 |  978 | `				sxu32 nG = 0;` |
|      2386 |  979 | `				if( IniGlobalValue(pSlot,&zG,&nG) ){` |
|      2199 |  980 | `					ph7_value_string(pCur,zG,(int)nG);` |
|      1067 |  981 | `				}else{` |
|       190 |  982 | `					ph7_value_null(pCur);` |
|         - |  983 | `				}` |
|         - |  984 | `			}` |
|      2386 |  985 | `			ph7_array_add_strkey_elem(pRow,"global_value",pCur);` |
|      2386 |  986 | `			ph7_value_reset_string_cursor(pCur);` |
|      2386 |  987 | `			if( pSlot->bLocalNull ){` |
|       190 |  988 | `				ph7_value_null(pCur);` |
|        94 |  989 | `			}else{` |
|      2199 |  990 | `				ph7_value_string(pCur,(const char *)SyBlobData(&sVal),(int)SyBlobLength(&sVal));` |
|         - |  991 | `			}` |
|      2386 |  992 | `			ph7_array_add_strkey_elem(pRow,"local_value",pCur);` |
|      2386 |  993 | `			ph7_value_reset_string_cursor(pCur);` |
|      2386 |  994 | `			ph7_value_int(pCur,pSlot->iAccess);` |
|      2386 |  995 | `			ph7_array_add_strkey_elem(pRow,"access",pCur);` |
|      2386 |  996 | `			ph7_value_reset_string_cursor(pCur);` |
|      2386 |  997 | `			ph7_array_add_strkey_elem(pOut,zKey,pRow);` |
|      1158 |  998 | `		}else{` |
|       752 |  999 | `			ph7_value_reset_string_cursor(pCur);` |
|       752 | 1000 | `			if( pSlot->bLocalNull ){` |
|        62 | 1001 | `				ph7_value_null(pCur);` |
|        32 | 1002 | `			}else{` |
|       692 | 1003 | `				ph7_value_string(pCur,(const char *)SyBlobData(&sVal),(int)SyBlobLength(&sVal));` |
|         - | 1004 | `			}` |
|       752 | 1005 | `			ph7_array_add_strkey_elem(pOut,zKey,pCur);` |
|       752 | 1006 | `			ph7_value_reset_string_cursor(pCur);` |
|         - | 1007 | `		}` |
|      1533 | 1008 | `	}` |
|        80 | 1009 | `	SyBlobRelease(&sVal);` |
|        80 | 1010 | `	ph7_result_value(pCtx,pOut);` |
|        80 | 1011 | `	return PH7_OK;` |
|        50 | 1012 | `}` |
|         - | 1013 | `/* string\|false get_cfg_var(string $option) — php answers the GLOBAL value */` |
|        14 | 1014 | `static int vm_builtin_get_cfg_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 1015 | `{` |
|        16 | 1016 | `	ph7_vm *pVm = pCtx->pVm;` |
|         - | 1017 | `	VmIniSlot *pSlot;` |
|         - | 1018 | `	const char *zName;` |
|        16 | 1019 | `	int nName = 0;` |
|        16 | 1020 | `	if( nArg < 1 \|\| IniSeed(pVm) != SXRET_OK ){` |
|       ! 0 | 1021 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 1022 | `		return PH7_OK;` |
|         - | 1023 | `	}` |
|        16 | 1024 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|        16 | 1025 | `	pSlot = IniFind(pVm,zName,(sxu32)nName);` |
|        16 | 1026 | `	if( pSlot == 0 \|\| pSlot->bGlobalNull ){` |
|         - | 1027 | `		/* php reads this one from the php.ini FILE rather than from the live` |
|         - | 1028 | `		 * directive, so a name the file never mentioned is false -- and a` |
|         - | 1029 | `		 * directive declared with no value is exactly such a name, whatever a` |
|         - | 1030 | `		 * later ini_set() put in it. */` |
|         9 | 1031 | `		ph7_result_bool(pCtx,0);` |
|         9 | 1032 | `		return PH7_OK;` |
|         - | 1033 | `	}` |
|        11 | 1034 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&pSlot->sGlobal),` |
|         6 | 1035 | `		(int)SyBlobLength(&pSlot->sGlobal));` |
|         8 | 1036 | `	return PH7_OK;` |
|         9 | 1037 | `}` |
|         - | 1038 | `/*` |
|         - | 1039 | ` * The effective value of a directive a C builtin needs to obey, as an integer.` |
|         - | 1040 | ` * parse_str() reads max_input_vars and max_input_nesting_level through this;` |
|         - | 1041 | ` * without it a builtin would have to duplicate php's default and could never` |
|         - | 1042 | `` * see an `ini_set()`/`-d` override. Answers iDefault when the directive is`` |
|         - | 1043 | ` * absent or unparsable.` |
|         - | 1044 | ` */` |
|      2679 | 1045 | `PH7_PRIVATE sxi64 PH7_VmIniGetInt(ph7_vm *pVm,const char *zName,sxi64 iDefault)` |
|         5 | 1046 | `{` |
|         - | 1047 | `	VmIniSlot *pSlot;` |
|         - | 1048 | `	SyBlob sVal;` |
|      2684 | 1049 | `	sxi64 iVal = iDefault;` |
|      2684 | 1050 | `	IniSeed(pVm);` |
|      2684 | 1051 | `	pSlot = IniFind(pVm,zName,(sxu32)SyStrlen(zName));` |
|      2684 | 1052 | `	if( pSlot == 0 ){` |
|       ! 0 | 1053 | `		return iDefault;` |
|         - | 1054 | `	}` |
|      2684 | 1055 | `	SyBlobInit(&sVal,&pVm->sAllocator);` |
|      2684 | 1056 | `	IniLiveGet(pVm,pSlot,&sVal);` |
|      2684 | 1057 | `	if( SyBlobLength(&sVal) > 0 ){` |
|      2684 | 1058 | `		SyStrToInt64((const char *)SyBlobData(&sVal),SyBlobLength(&sVal),(void *)&iVal,0);` |
|      1340 | 1059 | `	}` |
|      2684 | 1060 | `	SyBlobRelease(&sVal);` |
|      2684 | 1061 | `	return iVal;` |
|      1345 | 1062 | `}` |
|         - | 1063 | `/* The same, as a borrowed STRING (arg_separator.input). The bytes live in the` |
|         - | 1064 | ` * caller's blob, which it owns. */` |
|         - | 1065 | `/*` |
|         - | 1066 | ` * One directive as the export format needs it: php's access bitmask, the value` |
|         - | 1067 | `` * a script reads now, and the DEFAULT behind it. `pOut`/`pDef` are the`` |
|         - | 1068 | ` * caller's blobs. Answers 0 when the build has no such directive.` |
|         - | 1069 | ` */` |
|        62 | 1070 | `PH7_PRIVATE int PH7_VmIniDescribe(ph7_vm *pVm,const char *zName,sxu32 nName,` |
|         - | 1071 | `	sxi32 *piAccess,SyBlob *pOut,SyBlob *pDef)` |
|         1 | 1072 | `{` |
|         - | 1073 | `	VmIniSlot *pSlot;` |
|        63 | 1074 | `	IniSeed(&(*pVm));` |
|        63 | 1075 | `	pSlot = IniFind(&(*pVm),zName,nName);` |
|        63 | 1076 | `	if( pSlot == 0 ){` |
|       ! 0 | 1077 | `		return 0;` |
|         - | 1078 | `	}` |
|        63 | 1079 | `	*piAccess = pSlot->iAccess;` |
|        63 | 1080 | `	IniLiveGet(&(*pVm),pSlot,pOut);` |
|        63 | 1081 | `	SyBlobReset(pDef);` |
|        63 | 1082 | `	SyBlobAppend(pDef,SyBlobData(&pSlot->sGlobal),SyBlobLength(&pSlot->sGlobal));` |
|        63 | 1083 | `	return 1;` |
|        32 | 1084 | `}` |
|      1655 | 1085 | `PH7_PRIVATE void PH7_VmIniGetStr(ph7_vm *pVm,const char *zName,SyBlob *pOut)` |
|         5 | 1086 | `{` |
|         - | 1087 | `	VmIniSlot *pSlot;` |
|      1660 | 1088 | `	IniSeed(pVm);` |
|      1660 | 1089 | `	SyBlobReset(pOut);` |
|      1660 | 1090 | `	pSlot = IniFind(pVm,zName,(sxu32)SyStrlen(zName));` |
|      1660 | 1091 | `	if( pSlot ){` |
|      1578 | 1092 | `		IniLiveGet(pVm,pSlot,pOut);` |
|       747 | 1093 | `	}` |
|      1660 | 1094 | `}` |
|         - | 1095 | `/*` |
|         - | 1096 | ` * Does this directive currently hold php's UNSET value? Only the surfaces that` |
|         - | 1097 | ` * show a RAW value ask -- ini_get() answers the empty string for one either` |
|         - | 1098 | ` * way, which is why the question has to be put separately.` |
|         - | 1099 | ` */` |
|       740 | 1100 | `PH7_PRIVATE int PH7_VmIniIsUnset(ph7_vm *pVm,const char *zName)` |
|         3 | 1101 | `{` |
|         - | 1102 | `	VmIniSlot *pSlot;` |
|       743 | 1103 | `	IniSeed(pVm);` |
|       743 | 1104 | `	pSlot = IniFind(pVm,zName,(sxu32)SyStrlen(zName));` |
|       743 | 1105 | `	return pSlot != 0 && pSlot->bLocalNull;` |
|         3 | 1106 | `}` |
|         - | 1107 | `/*` |
|         - | 1108 | ` * Read a BOOLEAN directive the way zend_ini does — "on"/"yes"/"true" as well as` |
|         - | 1109 | ` * a non-zero number. Reading one through the integer parser answers 0 for` |
|         - | 1110 | `` * `On`, which is the spelling php.ini-production ships, so a directive written`` |
|         - | 1111 | ` * that way reads as OFF.` |
|         - | 1112 | ` */` |
|   1471264 | 1113 | `PH7_PRIVATE int PH7_VmIniGetBool(ph7_vm *pVm,const char *zName,int bDefault)` |
|         5 | 1114 | `{` |
|         - | 1115 | `	VmIniSlot *pSlot;` |
|         - | 1116 | `	SyBlob sVal;` |
|         - | 1117 | `	int bRes;` |
|   1471269 | 1118 | `	IniSeed(pVm);` |
|   1471269 | 1119 | `	pSlot = IniFind(pVm,zName,(sxu32)SyStrlen(zName));` |
|   1471269 | 1120 | `	if( pSlot == 0 ){` |
|       ! 0 | 1121 | `		return bDefault;` |
|         - | 1122 | `	}` |
|   1471269 | 1123 | `	SyBlobInit(&sVal,&pVm->sAllocator);` |
|   1471269 | 1124 | `	IniLiveGet(pVm,pSlot,&sVal);` |
|   1471269 | 1125 | `	bRes = IniTruthy((const char *)SyBlobData(&sVal),SyBlobLength(&sVal));` |
|   1471269 | 1126 | `	SyBlobRelease(&sVal);` |
|   1471269 | 1127 | `	return bRes;` |
|    735600 | 1128 | `}` |
|      8445 | 1129 | `PH7_PRIVATE sxi32 PH7_VmInstallIni(ph7_vm *pVm)` |
|         5 | 1130 | `{` |
|         - | 1131 | `	static const struct {` |
|         - | 1132 | `		const char *zName;` |
|         - | 1133 | `		ProchHostFunction xFunc;` |
|         - | 1134 | `	} aFunc[] = {` |
|         - | 1135 | `		{ "ini_get",      vm_builtin_ini_get      },` |
|         - | 1136 | `		{ "ini_set",      vm_builtin_ini_set      },` |
|         - | 1137 | `		/* php's alias for the same routine. Both of the diagnostics a write` |
|         - | 1138 | `		 * can raise name the INVOKED function, so they read the context's` |
|         - | 1139 | `		 * name rather than a literal. */` |
|         - | 1140 | `		{ "ini_alter",    vm_builtin_ini_set      },` |
|         - | 1141 | `		{ "ini_restore",  vm_builtin_ini_restore  },` |
|         - | 1142 | `		{ "ini_get_all",  vm_builtin_ini_get_all  },` |
|         - | 1143 | `		{ "get_cfg_var",  vm_builtin_get_cfg_var  },` |
|         - | 1144 | `	};` |
|         - | 1145 | `	sxu32 n;` |
|     59120 | 1146 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFunc) ; n++ ){` |
|     50675 | 1147 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
|     25307 | 1148 | `	}` |
|      8450 | 1149 | `	return SXRET_OK;` |
|         5 | 1150 | `}` |
|         - | 1151 | `#else` |
|         - | 1152 | `PH7_PRIVATE sxi32 PH7_VmInstallIni(ph7_vm *pVm){ (void)pVm; return SXRET_OK; }` |
|         - | 1153 | `PH7_PRIVATE sxi64 PH7_VmIniGetInt(ph7_vm *pVm,const char *zName,sxi64 iDefault){` |
|         - | 1154 | `	(void)pVm; (void)zName; return iDefault;` |
|         - | 1155 | `}` |
|         - | 1156 | `PH7_PRIVATE int PH7_VmIniGetBool(ph7_vm *pVm,const char *zName,int bDefault){` |
|         - | 1157 | `	(void)pVm; (void)zName; return bDefault;` |
|         - | 1158 | `}` |
|         - | 1159 | `PH7_PRIVATE void PH7_VmIniGetStr(ph7_vm *pVm,const char *zName,SyBlob *pOut){` |
|         - | 1160 | `	(void)pVm; (void)zName; SyBlobReset(pOut);` |
|         - | 1161 | `}` |
|         - | 1162 | `PH7_PRIVATE int PH7_VmIniIsUnset(ph7_vm *pVm,const char *zName){` |
|         - | 1163 | `	(void)pVm; (void)zName; return 0;` |
|         - | 1164 | `}` |
|         - | 1165 | `PH7_PRIVATE int PH7_VmIniDescribe(ph7_vm *pVm,const char *zName,sxu32 nName,` |
|         - | 1166 | `	sxi32 *piAccess,SyBlob *pOut,SyBlob *pDef){` |
|         - | 1167 | `	(void)pVm; (void)zName; (void)nName; (void)piAccess;` |
|         - | 1168 | `	SyBlobReset(pOut); SyBlobReset(pDef); return 0;` |
|         - | 1169 | `}` |
|         - | 1170 | `PH7_PRIVATE int PH7_VmIniSet(ph7_vm *pVm,const char *zName,sxu32 nName,` |
|         - | 1171 | `	const char *zVal,sxu32 nVal,const char *zWho){` |
|         - | 1172 | `	(void)pVm; (void)zName; (void)nName; (void)zVal; (void)nVal; (void)zWho; return 0;` |
|         - | 1173 | `}` |
|         - | 1174 | `#endif` |
|         - | 1175 |  |
