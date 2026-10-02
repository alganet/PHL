# src/ph7/vm_builtin_ini.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 548/599 lines (91.49%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    4 | ` */` |
|      - |    5 | `#include "ph7int.h"` |
|      - |    6 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |    7 | `/*` |
|      - |    8 | ` * php.ini subsystem + INI API: a lazily seeded directive table (the static` |
|      - |    9 | ` * defaults merged with the CLI's -d/-c entries from pVm->aIniCli) behind` |
|      - |   10 | ` * ini_get / ini_set / ini_restore / ini_get_all / get_cfg_var. Live-wired` |
|      - |   11 | ` * directives dispatch to the real knobs (error_reporting(), the session state,` |
|      - |   12 | ` * the default timezone, the diagnostic gates) so the INI view and the engine` |
|      - |   13 | ` * agree.` |
|      - |   14 | ` *` |
|      - |   15 | `` * This was an embedded-PHP chunk over two `__ini_*` C thunks, with the table`` |
|      - |   16 | `` * parked on a private `__IniS` class and five `__ini_*` PHP helpers alongside.`` |
|      - |   17 | ` * All eight of those names are gone: the table is a SySet on the VM and the five` |
|      - |   18 | ` * functions ARE these C routines. The thunks did not become methods -- there is` |
|      - |   19 | ` * no class here, only php's global functions -- so, like libxml's, they collapse` |
|      - |   20 | ` * into the functions they were serving.` |
|      - |   21 | ` *` |
|      - |   22 | ` * One thing the move fixes on its own: a diagnostic raised by a prelude function` |
|      - |   23 | ` * reported the CHUNK's line, so every ini_set()/ini_get_all() warning said` |
|      - |   24 | ` * "on line 1" regardless of the caller. A C builtin reports the caller's line,` |
|      - |   25 | ` * which is what php prints.` |
|      - |   26 | ` */` |
|      - |   27 |  |
|      - |   28 | `/* Directive access levels, as php reports them in ini_get_all()['access']. */` |
|      - |   29 | `#define VM_INI_USER    1` |
|      - |   30 | `#define VM_INI_PERDIR  2` |
|      - |   31 | `#define VM_INI_SYSTEM  4` |
|      - |   32 | `#define VM_INI_ALL     (VM_INI_USER\|VM_INI_PERDIR\|VM_INI_SYSTEM)` |
|      - |   33 |  |
|      - |   34 | `/*` |
|      - |   35 | ` * The defaults, in the order php's ini_get_all() reports them (sorted by name).` |
|      - |   36 | ` * Keeping this list sorted is what lets ini_get_all() skip a sort: the seed` |
|      - |   37 | ` * inserts any CLI-only directive in its sorted position.` |
|      - |   38 | ` */` |
|      - |   39 | `static const struct {` |
|      - |   40 | `	const char *zName;` |
|      - |   41 | `	const char *zValue;   /* 0 = php's UNSET directive, which reports NULL and` |
|      - |   42 | `	                       * is not the same thing as "" */` |
|      - |   43 | `	sxi32 iAccess;` |
|      - |   44 | `} aIniDefault[] = {` |
|      - |   45 | `	{ "allow_url_fopen",          "1",          VM_INI_SYSTEM },` |
|      - |   46 | `	/* php's default is OFF, and it spells that default as the EMPTY string — which` |
|      - |   47 | `	 * is what ini_get() answers. Including a remote file is the classic RFI, and` |
|      - |   48 | `	 * this is what a STREAM_IS_URL wrapper's include is gated on. */` |
|      - |   49 | `	{ "allow_url_include",        "",           VM_INI_SYSTEM },` |
|      - |   50 | `	{ "arg_separator.input",      "&",          VM_INI_PERDIR\|VM_INI_SYSTEM },` |
|      - |   51 | `	{ "arg_separator.output",     "&",          VM_INI_ALL },` |
|      - |   52 | `	{ "auto_detect_line_endings", "",           VM_INI_ALL },` |
|      - |   53 | `	/* ext/bcmath's only directive: the scale every bc* function defaults its` |
|      - |   54 | `	 * $scale argument from, and the one slot bcscale() reads and writes. */` |
|      - |   55 | `	{ "bcmath.scale",             "0",          VM_INI_ALL },` |
|      - |   56 | `	{ "date.timezone",            "UTC",        VM_INI_ALL },` |
|      - |   57 | `	/* The four the sun trio defaults from. php ships this point (Jerusalem)` |
|      - |   58 | `	 * and this zenith (the sun's centre 50 arcminutes below the horizon,` |
|      - |   59 | `	 * which is refraction plus its own radius) as the stock values. */` |
|      - |   60 | `	{ "date.default_latitude",    "31.7667",    VM_INI_ALL },` |
|      - |   61 | `	{ "date.default_longitude",   "35.2333",    VM_INI_ALL },` |
|      - |   62 | `	{ "date.sunrise_zenith",      "90.833333",  VM_INI_ALL },` |
|      - |   63 | `	{ "date.sunset_zenith",       "90.833333",  VM_INI_ALL },` |
|      - |   64 | `	{ "default_charset",          "UTF-8",      VM_INI_ALL },` |
|      - |   65 | `	/* php bounds a socket wait by this rather than waiting forever, and it is` |
|      - |   66 | `	 * where stream_socket_accept() takes its default timeout from. */` |
|      - |   67 | `	{ "default_socket_timeout",   "60",         VM_INI_ALL },` |
|      - |   68 | `	{ "default_mimetype",         "text/html",  VM_INI_ALL },` |
|      - |   69 | `	{ "display_errors",           "",           VM_INI_ALL },` |
|      - |   70 | `	{ "error_log",                0,            VM_INI_ALL },` |
|      - |   71 | `	{ "error_reporting",          "30719",      VM_INI_ALL },` |
|      - |   72 | `	/* The From: header the http:// wrapper writes. php ships it UNSET, and the` |
|      - |   73 | `	 * difference matters: an unset directive writes no header at all, while an` |
|      - |   74 | ``	 * ini_set() to the EMPTY string writes `From: ` -- which is not how`` |
|      - |   75 | `	 * user_agent below behaves. */` |
|      - |   76 | `	{ "from",                     0,            VM_INI_ALL },` |
|      - |   77 | `	{ "highlight.comment",        "#FF8000",    VM_INI_ALL },` |
|      - |   78 | `	{ "highlight.default",        "#0000BB",    VM_INI_ALL },` |
|      - |   79 | `	{ "highlight.html",           "#000000",    VM_INI_ALL },` |
|      - |   80 | `	{ "highlight.keyword",        "#007700",    VM_INI_ALL },` |
|      - |   81 | `	{ "highlight.string",         "#DD0000",    VM_INI_ALL },` |
|      - |   82 | `	/* ignore_user_abort(): the directive the function reads and writes. php` |
|      - |   83 | `	 * spells its default "0" rather than the empty string every other boolean` |
|      - |   84 | `	 * directive here uses, and ini_get() answers that byte. */` |
|      - |   85 | `	{ "ignore_user_abort",        "0",          VM_INI_ALL },` |
|      - |   86 | `	{ "include_path",             ".",          VM_INI_ALL },` |
|      - |   87 | `	{ "log_errors",               "1",          VM_INI_ALL },` |
|      - |   88 | `	{ "max_execution_time",       "0",          VM_INI_ALL },` |
|      - |   89 | `	{ "max_input_nesting_level",  "64",         VM_INI_PERDIR\|VM_INI_SYSTEM },` |
|      - |   90 | `	{ "max_input_vars",           "1000",       VM_INI_PERDIR\|VM_INI_SYSTEM },` |
|      - |   91 | `	{ "memory_limit",             "-1",         VM_INI_ALL },` |
|      - |   92 | `#ifdef PH7_ENABLE_OPENSSL` |
|      - |   93 | `	/* ext/openssl's two path directives, php's own defaults (both UNSET, the` |
|      - |   94 | `	 * empty string) and its access mask (PHP_INI_PERDIR for both). They are` |
|      - |   95 | `	 * what openssl_get_cert_locations() reports as ini_cafile/ini_capath and` |
|      - |   96 | `	 * what a TLS peer verification falls back to when a stream context names` |
|      - |   97 | `	 * no CA of its own. Both ship UNSET rather than empty, which is what php` |
|      - |   98 | `	 * reports and is a different thing: ini_get_all() answers NULL for an` |
|      - |   99 | ``	 * unset directive and "" for one set to the empty string. php's third, `openssl.libctx`, is not here: it picks`` |
|      - |  100 | `	 * between the default OpenSSL library context and a private one, and this` |
|      - |  101 | `	 * build has only the default -- registering the name would report a` |
|      - |  102 | `	 * choice that is not being made. */` |
|      - |  103 | `	{ "openssl.cafile",           0,            VM_INI_PERDIR },` |
|      - |  104 | `	{ "openssl.capath",           0,            VM_INI_PERDIR },` |
|      - |  105 | `#endif` |
|      - |  106 | ``	/* ext/phar's three. `phar.readonly` is php's own default ON: every write`` |
|      - |  107 | `	 * door refuses until an installer turns it off, which is why building an` |
|      - |  108 | ``	 * archive is a `-d phar.readonly=0` job on a stock php too. */`` |
|      - |  109 | `	{ "phar.cache_list",          "",           VM_INI_SYSTEM },` |
|      - |  110 | `	{ "phar.readonly",            "1",          VM_INI_ALL },` |
|      - |  111 | `	{ "phar.require_hash",        "1",          VM_INI_ALL },` |
|      - |  112 | `	{ "post_max_size",            "8M",         VM_INI_PERDIR\|VM_INI_SYSTEM },` |
|      - |  113 | `	{ "precision",                "14",         VM_INI_ALL },` |
|      - |  114 | `	{ "serialize_precision",      "-1",         VM_INI_ALL },` |
|      - |  115 | `	/* php's session directives, the whole non-deprecated set: session_start()'s` |
|      - |  116 | `	 * $options array applies its keys THROUGH this table, so a directive missing` |
|      - |  117 | `	 * here is an option php accepts and PHL reports as failed.` |
|      - |  118 | `	 * Six are absent on purpose: php 8.4 DEPRECATES session.sid_length,` |
|      - |  119 | `	 * session.sid_bits_per_character, session.referer_check, session.use_trans_sid,` |
|      - |  120 | `	 * session.trans_sid_tags and session.trans_sid_hosts, and the scope policy does not carry` |
|      - |  121 | `	 * php's deprecated surface. The session.upload_progress.* family goes with the` |
|      - |  122 | ``	 * file uploads the scope policy excludes from a CLI-plus-`-S` engine. */`` |
|      - |  123 | `	{ "session.auto_start",       "0",          VM_INI_PERDIR },` |
|      - |  124 | `	{ "session.cache_expire",     "180",        VM_INI_ALL },` |
|      - |  125 | `	{ "session.cache_limiter",    "nocache",    VM_INI_ALL },` |
|      - |  126 | `	/* The Set-Cookie the session sends is built out of these seven. */` |
|      - |  127 | `	{ "session.cookie_domain",    "",           VM_INI_ALL },` |
|      - |  128 | `	{ "session.cookie_httponly",  "0",          VM_INI_ALL },` |
|      - |  129 | `	{ "session.cookie_lifetime",  "0",          VM_INI_ALL },` |
|      - |  130 | `	{ "session.cookie_partitioned","0",         VM_INI_ALL },` |
|      - |  131 | `	{ "session.cookie_path",      "/",          VM_INI_ALL },` |
|      - |  132 | `	{ "session.cookie_samesite",  "",           VM_INI_ALL },` |
|      - |  133 | `	{ "session.cookie_secure",    "0",          VM_INI_ALL },` |
|      - |  134 | `	{ "session.gc_divisor",       "100",        VM_INI_ALL },` |
|      - |  135 | `	{ "session.gc_maxlifetime",   "1440",       VM_INI_ALL },` |
|      - |  136 | `	{ "session.gc_probability",   "1",          VM_INI_ALL },` |
|      - |  137 | `	{ "session.lazy_write",       "1",          VM_INI_ALL },` |
|      - |  138 | `	{ "session.name",             "PHPSESSID",  VM_INI_ALL },` |
|      - |  139 | `	{ "session.save_handler",     "files",      VM_INI_ALL },` |
|      - |  140 | `	{ "session.save_path",        "",           VM_INI_ALL },` |
|      - |  141 | ``	/* Which of php's three session serializers writes the store: `php` (the`` |
|      - |  142 | ``	 * `name\|<serialized>` runs a stock php install reads), `php_binary` or`` |
|      - |  143 | ``	 * `php_serialize`. */`` |
|      - |  144 | `	{ "session.serialize_handler","php",        VM_INI_ALL },` |
|      - |  145 | `	/* Whether the session sends and reads its id as a cookie at all. PHL has never` |
|      - |  146 | `	 * read an id from anywhere ELSE, which is what use_only_cookies means. */` |
|      - |  147 | `	{ "session.use_cookies",      "1",          VM_INI_ALL },` |
|      - |  148 | `	{ "session.use_only_cookies", "1",          VM_INI_ALL },` |
|      - |  149 | `	{ "session.use_strict_mode",  "0",          VM_INI_ALL },` |
|      - |  150 | `	{ "short_open_tag",           "",           VM_INI_PERDIR\|VM_INI_SYSTEM },` |
|      - |  151 | `	/* php's three syslog directives, with php's defaults and php's access masks.` |
|      - |  152 | ``	 * `syslog.filter` is the one this engine READS: it decides which bytes`` |
|      - |  153 | `	 * syslog() escapes and is PHP_INI_ALL, so a script can change it. The other` |
|      - |  154 | ``	 * two are what php's own error logger uses when `error_log = syslog`, a`` |
|      - |  155 | `	 * target this build does not have -- they are declared because ini_get() and` |
|      - |  156 | `	 * ini_get_all() answer them under php and a program can read either. */` |
|      - |  157 | `	{ "syslog.facility",          "LOG_USER",   VM_INI_SYSTEM },` |
|      - |  158 | `	{ "syslog.filter",            "no-ctrl",    VM_INI_ALL },` |
|      - |  159 | `	{ "syslog.ident",             "php",        VM_INI_SYSTEM },` |
|      - |  160 | `#ifdef PH7_ENABLE_SQLITE` |
|      - |  161 | `` 	/* ext/sqlite3's two directives, in this sorted list's own place. `defensive` `` |
|      - |  162 | `	 * is applied to every connection SQLite3 opens (it is what makes an UPDATE of` |
|      - |  163 | ``	 * sqlite_master refuse, even behind `PRAGMA writable_schema=ON`), and`` |
|      - |  164 | ``	 * `extension_dir` is the door loadExtension() is shut behind: empty means`` |
|      - |  165 | `	 * "SQLite Extensions are disabled", and php ships it empty. The access masks` |
|      - |  166 | `	 * are php's own, which do not agree with each other. */` |
|      - |  167 | `	{ "sqlite3.defensive",        "1",          VM_INI_USER },` |
|      - |  168 | `	{ "sqlite3.extension_dir",    0,            VM_INI_SYSTEM },` |
|      - |  169 | `#endif` |
|      - |  170 | `	{ "unserialize_callback_func","",           VM_INI_ALL },` |
|      - |  171 | `	{ "unserialize_max_depth",    "4096",       VM_INI_ALL },` |
|      - |  172 | `	{ "upload_max_filesize",      "2M",         VM_INI_PERDIR\|VM_INI_SYSTEM },` |
|      - |  173 | `	/* The User-Agent the http:// wrapper writes when the request names none.` |
|      - |  174 | ``	 * UNSET like `from` above, and for the same reason -- but the wrapper reads`` |
|      - |  175 | `	 * the two differently: an empty user_agent writes no header at all, while an` |
|      - |  176 | ``	 * empty `from` writes `From: `. */`` |
|      - |  177 | `	{ "user_agent",               0,            VM_INI_ALL },` |
|      - |  178 | `	{ "zend.assertions",          "-1",         VM_INI_ALL },` |
|      - |  179 | `#ifdef PH7_ENABLE_ZLIB` |
|      - |  180 | `	/* ext/zlib's three, php's own defaults and its access mask (all three are` |
|      - |  181 | `	 * PHP_INI_ALL). They are READ by ob_gzhandler() and zlib_get_coding_type()` |
|      - |  182 | `	 * and by nothing else: php's output-layer compression is a SAPI feature a` |
|      - |  183 | `	 * command line never turns on, and neither does this. */` |
|      - |  184 | `	{ "zlib.output_compression",  "",           VM_INI_ALL },` |
|      - |  185 | `	{ "zlib.output_compression_level","-1",     VM_INI_ALL },` |
|      - |  186 | `	{ "zlib.output_handler",      "",           VM_INI_ALL },` |
|      - |  187 | `#endif` |
|      - |  188 | `};` |
|      - |  189 |  |
| 153463 |  190 | `static int IniNameIs(const VmIniSlot *pSlot,const char *zName)` |
|      5 |  191 | `{` |
| 153468 |  192 | `	sxu32 n = (sxu32)SyStrlen(zName);` |
| 153468 |  193 | `	return pSlot->sName.nByte == n && SyMemcmp(pSlot->sName.zString,zName,n) == 0;` |
|      5 |  194 | `}` |
|      - |  195 | `/*` |
|      - |  196 | ` * zend_ini_parse_bool semantics, matching the C-side VmIniBool the -d/-c path` |
|      - |  197 | ` * uses: on/yes/true, else a non-zero integer parse.` |
|      - |  198 | ` */` |
|   1297 |  199 | `static int IniTruthy(const char *zVal,sxu32 nVal)` |
|      5 |  200 | `{` |
|   1938 |  201 | `	while( nVal > 0 && (zVal[0] == ' ' \|\| zVal[0] == '\t') ){ zVal++; nVal--; }` |
|   1938 |  202 | `	while( nVal > 0 && (zVal[nVal-1] == ' ' \|\| zVal[nVal-1] == '\t') ){ nVal--; }` |
|   1302 |  203 | `	if( nVal == 2 && SyStrnicmp(zVal,"on",2) == 0 ){ return 1; }` |
|   1302 |  204 | `	if( nVal == 3 && SyStrnicmp(zVal,"yes",3) == 0 ){ return 1; }` |
|   1302 |  205 | `	if( nVal == 4 && SyStrnicmp(zVal,"true",4) == 0 ){ return 1; }` |
|      - |  206 | `	{` |
|   1302 |  207 | `		sxi32 iVal = 0;` |
|   1302 |  208 | `		if( nVal > 0 && SyStrToInt32(zVal,nVal,(void *)&iVal,0) == SXRET_OK ){` |
|   1296 |  209 | `			return iVal != 0;` |
|      - |  210 | `		}` |
|      - |  211 | `	}` |
|      7 |  212 | `	return 0;` |
|    644 |  213 | `}` |
|      - |  214 | `/*` |
|      - |  215 | `` * The value rules that apply to a `-d name=value` on the COMMAND LINE, which are`` |
|      - |  216 | ` * not the same set IniValueAccepted enforces on ini_set(). php runs each` |
|      - |  217 | ` * directive's OnUpdate handler at startup too, but several of those refuse only` |
|      - |  218 | `` * at RUNTIME -- `-d session.serialize_handler=bogus` is taken by php because the`` |
|      - |  219 | ` * serializer table it would look the name up in is still empty -- so this is a` |
|      - |  220 | ` * per-directive list rather than a shared screen. A value refused here leaves the` |
|      - |  221 | ` * directive at its default, which is what php reports.` |
|      - |  222 | ` */` |
|    681 |  223 | `static int IniStartupValueAccepted(const SyString *pName,const char *zVal,sxu32 nVal)` |
|      4 |  224 | `{` |
|    681 |  225 | `	if( pName->nByte == sizeof("syslog.filter")-1` |
|    351 |  226 | `	 && SyMemcmp(pName->zString,"syslog.filter",sizeof("syslog.filter")-1) == 0 ){` |
|    ! 0 |  227 | `		return (nVal == 3 && SyMemcmp(zVal,"all",3) == 0)` |
|    ! 0 |  228 | `		    \|\| (nVal == 7 && SyMemcmp(zVal,"no-ctrl",7) == 0)` |
|    ! 0 |  229 | `		    \|\| (nVal == 5 && SyMemcmp(zVal,"ascii",5) == 0)` |
|    ! 0 |  230 | `		    \|\| (nVal == 3 && SyMemcmp(zVal,"raw",3) == 0);` |
|      - |  231 | `	}` |
|    685 |  232 | `	return 1;` |
|    344 |  233 | `}` |
|      - |  234 | `/*` |
|      - |  235 | ` * Build the table: the static defaults, then the CLI queue merged over them (an` |
|      - |  236 | ` * unknown CLI name is appended as a new INI_ALL directive, as the chunk did),` |
|      - |  237 | ` * then sorted by name so ini_get_all() can walk it in php's order without a sort.` |
|      - |  238 | ` */` |
|   7464 |  239 | `static sxi32 IniSeed(ph7_vm *pVm)` |
|      5 |  240 | `{` |
|      - |  241 | `	sxu32 i,j;` |
|      - |  242 | `	VmIniEntry *aCli;` |
|      - |  243 | `	VmIniSlot *aSlot;` |
|   7469 |  244 | `	if( pVm->bIniSeeded ){` |
|   6706 |  245 | `		return SXRET_OK;` |
|      - |  246 | `	}` |
|    768 |  247 | `	pVm->bIniSeeded = 1; /* set FIRST: the live-wired writes below re-enter nothing,` |
|      - |  248 | `	                      * but a future one must never recurse into the seed */` |
|  56467 |  249 | `	for( i = 0 ; i < SX_ARRAYSIZE(aIniDefault) ; i++ ){` |
|      - |  250 | `		VmIniSlot sSlot;` |
|  55704 |  251 | `		SyStringInitFromBuf(&sSlot.sName,aIniDefault[i].zName,SyStrlen(aIniDefault[i].zName));` |
|  55704 |  252 | `		sSlot.iAccess = aIniDefault[i].iAccess;` |
|  55704 |  253 | `		SyBlobInit(&sSlot.sGlobal,&pVm->sAllocator);` |
|  55704 |  254 | `		SyBlobInit(&sSlot.sLocal,&pVm->sAllocator);` |
|      - |  255 | `		/* A row with no value at all is php's UNSET directive, which is not the` |
|      - |  256 | `		 * empty string: it reports NULL everywhere the raw value is shown. */` |
|  55704 |  257 | `		sSlot.bGlobalNull = sSlot.bLocalNull = aIniDefault[i].zValue ? 0 : 1;` |
|  55704 |  258 | `		if( aIniDefault[i].zValue ){` |
|  76586 |  259 | `			SyBlobAppend(&sSlot.sGlobal,aIniDefault[i].zValue,` |
|  51121 |  260 | `				(sxu32)SyStrlen(aIniDefault[i].zValue));` |
|  76586 |  261 | `			SyBlobAppend(&sSlot.sLocal,aIniDefault[i].zValue,` |
|  51121 |  262 | `				(sxu32)SyStrlen(aIniDefault[i].zValue));` |
|  25460 |  263 | `		}` |
|  55704 |  264 | `		if( SySetPut(&pVm->aIniTab,(const void *)&sSlot) != SXRET_OK ){` |
|    ! 0 |  265 | `			return SXERR_MEM;` |
|      - |  266 | `		}` |
|  27745 |  267 | `	}` |
|    768 |  268 | `	aCli = (VmIniEntry *)SySetBasePtr(&pVm->aIniCli);` |
|   1449 |  269 | `	for( i = 0 ; i < SySetUsed(&pVm->aIniCli) ; i++ ){` |
|    685 |  270 | `		int bFound = 0;` |
|   1025 |  271 | `		if( !IniStartupValueAccepted(&aCli[i].sName,aCli[i].sValue.zString,` |
|    681 |  272 | `			aCli[i].sValue.nByte) ){` |
|    ! 0 |  273 | `			continue;   /* the directive keeps its default, as it does under php */` |
|      - |  274 | `		}` |
|    685 |  275 | `		aSlot = (VmIniSlot *)SySetBasePtr(&pVm->aIniTab);` |
|  28068 |  276 | `		for( j = 0 ; j < SySetUsed(&pVm->aIniTab) ; j++ ){` |
|  28036 |  277 | `			if( aSlot[j].sName.nByte == aCli[i].sName.nByte` |
|  14638 |  278 | `			 && SyMemcmp(aSlot[j].sName.zString,aCli[i].sName.zString,aCli[i].sName.nByte) == 0 ){` |
|    657 |  279 | `				SyBlobReset(&aSlot[j].sGlobal);` |
|    657 |  280 | `				SyBlobReset(&aSlot[j].sLocal);` |
|    657 |  281 | `				SyBlobAppend(&aSlot[j].sGlobal,aCli[i].sValue.zString,aCli[i].sValue.nByte);` |
|    657 |  282 | `				SyBlobAppend(&aSlot[j].sLocal,aCli[i].sValue.zString,aCli[i].sValue.nByte);` |
|      - |  283 | ``				/* `-d name=` names the directive on the command line, so what it`` |
|      - |  284 | `				 * carries is a value -- the empty one, never the unset state. */` |
|    657 |  285 | `				aSlot[j].bGlobalNull = aSlot[j].bLocalNull = 0;` |
|    657 |  286 | `				bFound = 1;` |
|    657 |  287 | `				break;` |
|      - |  288 | `			}` |
|  13679 |  289 | `		}` |
|    685 |  290 | `		if( !bFound ){` |
|      - |  291 | `			VmIniSlot sSlot;` |
|      - |  292 | `			/* aIniCli holds VM-lifetime copies already, so the name can be aliased. */` |
|     29 |  293 | `			sSlot.sName = aCli[i].sName;` |
|     29 |  294 | `			sSlot.iAccess = VM_INI_ALL;` |
|     29 |  295 | `			sSlot.bGlobalNull = sSlot.bLocalNull = 0;` |
|     29 |  296 | `			SyBlobInit(&sSlot.sGlobal,&pVm->sAllocator);` |
|     29 |  297 | `			SyBlobInit(&sSlot.sLocal,&pVm->sAllocator);` |
|     29 |  298 | `			SyBlobAppend(&sSlot.sGlobal,aCli[i].sValue.zString,aCli[i].sValue.nByte);` |
|     29 |  299 | `			SyBlobAppend(&sSlot.sLocal,aCli[i].sValue.zString,aCli[i].sValue.nByte);` |
|     29 |  300 | `			if( SySetPut(&pVm->aIniTab,(const void *)&sSlot) != SXRET_OK ){` |
|    ! 0 |  301 | `				return SXERR_MEM;` |
|      - |  302 | `			}` |
|     14 |  303 | `		}` |
|    344 |  304 | `	}` |
|      - |  305 | `	/* Insertion sort by name (the table is ~30 entries and already nearly sorted:` |
|      - |  306 | `	 * only CLI-introduced directives are out of place). */` |
|    768 |  307 | `	aSlot = (VmIniSlot *)SySetBasePtr(&pVm->aIniTab);` |
|  55732 |  308 | `	for( i = 1 ; i < SySetUsed(&pVm->aIniTab) ; i++ ){` |
|  54969 |  309 | `		VmIniSlot sTmp = aSlot[i];` |
|  54969 |  310 | `		j = i;` |
|  64314 |  311 | `		while( j > 0 ){` |
|  64312 |  312 | `			const VmIniSlot *pPrev = &aSlot[j-1];` |
|  64312 |  313 | `			sxu32 nMin = pPrev->sName.nByte < sTmp.sName.nByte ? pPrev->sName.nByte : sTmp.sName.nByte;` |
|  64312 |  314 | `			sxi32 iCmp = SyMemcmp(pPrev->sName.zString,sTmp.sName.zString,nMin);` |
|  64312 |  315 | `			if( iCmp == 0 ){` |
|    768 |  316 | `				iCmp = (sxi32)pPrev->sName.nByte - (sxi32)sTmp.sName.nByte;` |
|    380 |  317 | `			}` |
|  64312 |  318 | `			if( iCmp <= 0 ){` |
|  54967 |  319 | `				break;` |
|      - |  320 | `			}` |
|   9350 |  321 | `			aSlot[j] = aSlot[j-1];` |
|   9350 |  322 | `			j--;` |
|      5 |  323 | `		}` |
|  54969 |  324 | `		aSlot[j] = sTmp;` |
|  27379 |  325 | `	}` |
|      - |  326 | `	/* Boot-apply the CLI values for the live-wired session knobs. The engine knobs` |
|      - |  327 | `	 * (error_reporting / date.timezone) were already applied C-side by` |
|      - |  328 | `	 * PH7_VM_CONFIG_INI_ENTRY. */` |
|    768 |  329 | `	aSlot = (VmIniSlot *)SySetBasePtr(&pVm->aIniTab);` |
|  56495 |  330 | `	for( i = 0 ; i < SySetUsed(&pVm->aIniTab) ; i++ ){` |
|  55732 |  331 | `		SyBlob *pDst = 0;` |
|  55732 |  332 | `		if( IniNameIs(&aSlot[i],"session.name") ){` |
|    763 |  333 | `			if( SyBlobLength(&aSlot[i].sGlobal) != sizeof("PHPSESSID")-1` |
|    766 |  334 | `			 \|\| SyMemcmp(SyBlobData(&aSlot[i].sGlobal),"PHPSESSID",sizeof("PHPSESSID")-1) != 0 ){` |
|      6 |  335 | `				pDst = &pVm->sSessName;` |
|      8 |  336 | `			}` |
|  55349 |  337 | `		}else if( IniNameIs(&aSlot[i],"session.save_path") ){` |
|    768 |  338 | `			if( SyBlobLength(&aSlot[i].sGlobal) > 0 ){` |
|    ! 0 |  339 | `				pDst = &pVm->sSessPath;` |
|    ! 0 |  340 | `			}` |
|    380 |  341 | `		}` |
|  55732 |  342 | `		if( pDst ){` |
|      6 |  343 | `			sxu32 nLen = SyBlobLength(&aSlot[i].sGlobal);` |
|      6 |  344 | `			const char *zVal = (const char *)SyBlobData(&aSlot[i].sGlobal);` |
|      6 |  345 | `			while( nLen > 0 && zVal[nLen-1] == '/' ){ nLen--; } /* rtrim('/') */` |
|      6 |  346 | `			SyBlobReset(pDst);` |
|      6 |  347 | `			SyBlobAppend(pDst,zVal,nLen);` |
|      3 |  348 | `		}` |
|  27759 |  349 | `	}` |
|    768 |  350 | `	return SXRET_OK;` |
|   3659 |  351 | `}` |
|   7373 |  352 | `static VmIniSlot * IniFind(ph7_vm *pVm,const char *zName,sxu32 nName)` |
|      5 |  353 | `{` |
|   7378 |  354 | `	VmIniSlot *aSlot = (VmIniSlot *)SySetBasePtr(&pVm->aIniTab);` |
|      - |  355 | `	sxu32 n;` |
| 274157 |  356 | `	for( n = 0 ; n < SySetUsed(&pVm->aIniTab) ; n++ ){` |
| 274050 |  357 | `		if( aSlot[n].sName.nByte == nName` |
| 143390 |  358 | `		 && SyMemcmp(aSlot[n].sName.zString,zName,nName) == 0 ){` |
|   7276 |  359 | `			return &aSlot[n];` |
|      - |  360 | `		}` |
| 129902 |  361 | `	}` |
|    107 |  362 | `	return 0;` |
|   3614 |  363 | `}` |
|      - |  364 | `/*` |
|      - |  365 | ` * The EFFECTIVE current value. For a live-wired directive the runtime knob is` |
|      - |  366 | ` * the truth, not the stored local value, so that ini_get() and the knob's own` |
|      - |  367 | ` * accessor can never disagree.` |
|      - |  368 | ` */` |
|   9476 |  369 | `static void IniLiveGet(ph7_vm *pVm,VmIniSlot *pSlot,SyBlob *pOut)` |
|      5 |  370 | `{` |
|   9481 |  371 | `	SyBlobReset(pOut);` |
|   9481 |  372 | `	if( IniNameIs(pSlot,"error_reporting") ){` |
|      - |  373 | `		char zBuf[32];` |
|     87 |  374 | `		int nBuf = SyBufferFormat(zBuf,sizeof(zBuf),"%d",` |
|     56 |  375 | `			pVm->bErrReport ? (int)pVm->iErrMask : 0);` |
|     60 |  376 | `		SyBlobAppend(pOut,zBuf,(sxu32)nBuf);` |
|     60 |  377 | `		return;` |
|      - |  378 | `	}` |
|   9425 |  379 | `	if( IniNameIs(pSlot,"session.name") ){` |
|     92 |  380 | `		SyBlobAppend(pOut,SyBlobData(&pVm->sSessName),SyBlobLength(&pVm->sSessName));` |
|     92 |  381 | `		return;` |
|      - |  382 | `	}` |
|   9338 |  383 | `	if( IniNameIs(pSlot,"session.save_path") && SyBlobLength(&pVm->sSessPath) > 0 ){` |
|     11 |  384 | `		SyBlobAppend(pOut,SyBlobData(&pVm->sSessPath),SyBlobLength(&pVm->sSessPath));` |
|     11 |  385 | `		return;` |
|      - |  386 | `	}` |
|   9328 |  387 | `	if( IniNameIs(pSlot,"include_path") ){` |
|      - |  388 | `		/* The VM's path SET is the store; this directive is a view of it, so` |
|      - |  389 | `		 * ini_get() and get_include_path() can never name different paths. */` |
|     62 |  390 | `		PH7_VmGetIncludePath(pVm,pOut);` |
|     62 |  391 | `		return;` |
|      - |  392 | `	}` |
|   9270 |  393 | `	SyBlobAppend(pOut,SyBlobData(&pSlot->sLocal),SyBlobLength(&pSlot->sLocal));` |
|   4647 |  394 | `}` |
|      - |  395 | `/*` |
|      - |  396 | ` * Push a new value at the runtime knob behind a live-wired directive. The stored` |
|      - |  397 | ` * local value is updated by the caller either way.` |
|      - |  398 | ` */` |
|      - |  399 | `/*` |
|      - |  400 | ` * php's byte shorthand: a plain integer, optionally suffixed K, M or G (case` |
|      - |  401 | `` * insensitive, no "B"). `-1` -- and any negative -- means UNLIMITED, which is the`` |
|      - |  402 | ` * CLI default. Anything unparseable reads as 0, which php also treats as` |
|      - |  403 | ` * "allocate nothing", so it is left to say exactly that rather than being` |
|      - |  404 | ` * silently promoted to unlimited.` |
|      - |  405 | ` */` |
|     12 |  406 | `static sxu32 IniParseBytes(const char *zVal,sxu32 nVal,int *pbUnlimited)` |
|      1 |  407 | `{` |
|     13 |  408 | `	sxi64 iVal = 0;` |
|     13 |  409 | `	sxu32 n = 0;` |
|     13 |  410 | `	int bNeg = 0;` |
|     13 |  411 | `	*pbUnlimited = 0;` |
|     19 |  412 | `	while( n < nVal && (zVal[n] == ' ' \|\| zVal[n] == '\t') ){ n++; }` |
|     13 |  413 | `	if( n < nVal && (zVal[n] == '-' \|\| zVal[n] == '+') ){` |
|    ! 0 |  414 | `		bNeg = (zVal[n] == '-');` |
|    ! 0 |  415 | `		n++;` |
|    ! 0 |  416 | `	}` |
|     41 |  417 | `	while( n < nVal && zVal[n] >= '0' && zVal[n] <= '9' ){` |
|     29 |  418 | `		iVal = iVal * 10 + (zVal[n] - '0');` |
|     29 |  419 | `		if( iVal > (sxi64)0x7FFFFFFF ){ iVal = (sxi64)0x7FFFFFFF; } /* clamp: the field is 32-bit */` |
|     29 |  420 | `		n++;` |
|      1 |  421 | `	}` |
|     13 |  422 | `	if( bNeg ){` |
|    ! 0 |  423 | `		*pbUnlimited = 1;   /* php: any negative memory_limit is "no limit" */` |
|    ! 0 |  424 | `		return 0;` |
|      - |  425 | `	}` |
|     13 |  426 | `	if( n < nVal ){` |
|     13 |  427 | `		sxi64 nMul = 0;` |
|     13 |  428 | `		switch( zVal[n] ){` |
|    ! 0 |  429 | `			case 'k': case 'K': nMul = 1024; break;` |
|     13 |  430 | `			case 'm': case 'M': nMul = 1024 * 1024; break;` |
|    ! 0 |  431 | `			case 'g': case 'G': nMul = 1024 * 1024 * 1024; break;` |
|    ! 0 |  432 | `			default: nMul = 0; break;` |
|      - |  433 | `		}` |
|     13 |  434 | `		if( nMul > 0 ){` |
|     13 |  435 | `			iVal = (iVal > (sxi64)0x7FFFFFFF / nMul) ? (sxi64)0x7FFFFFFF : iVal * nMul;` |
|      6 |  436 | `		}` |
|      6 |  437 | `	}` |
|     13 |  438 | `	return (sxu32)iVal;` |
|      7 |  439 | `}` |
|      - |  440 | `/*` |
|      - |  441 | ` * Arm the allocator's total live-byte ceiling from a memory_limit value, and answer` |
|      - |  442 | ` * whether it took. THE one place the directive is interpreted: ini_set() reaches it` |
|      - |  443 | `` * through the validator and `-d name=value` reaches it directly, and a rule that`` |
|      - |  444 | ` * lived in only one of those would hold for one door and not the other.` |
|      - |  445 | ` *` |
|      - |  446 | ` * The one directive that reaches into the ALLOCATOR. php enforces a ceiling and` |
|      - |  447 | ` * kills the script with a fatal when a request would cross it; PHL stored the` |
|      - |  448 | ` * string and enforced nothing, so a runaway allocation -- a reference cycle nothing` |
|      - |  449 | ` * reclaims is the usual way in -- had no ceiling below the kernel's, and` |
|      - |  450 | ` * the OOM killer took the whole process instead of the script. On a shared box that` |
|      - |  451 | ` * is not the script's problem any more: it is everything else's.` |
|      - |  452 | ` *` |
|      - |  453 | ` * Unlimited is the CLI default here as it is in php, so a plain run is unchanged.` |
|      - |  454 | ` */` |
|     12 |  455 | `PH7_PRIVATE int PH7_VmApplyMemoryLimit(ph7_vm *pVm,const char *zVal,sxu32 nVal)` |
|      1 |  456 | `{` |
|     13 |  457 | `	int bUnlimited = 0;` |
|     13 |  458 | `	sxu32 nBytes = IniParseBytes(zVal,nVal,&bUnlimited);` |
|     13 |  459 | `	if( !bUnlimited && nBytes > 0 && nBytes < pVm->sAllocator.nMemUsed ){` |
|      - |  460 | `		/* php REFUSES to lower the ceiling below what is already in use --` |
|      - |  461 | `		 * zend_set_memory_limit answers FAILURE -- and it does so from the` |
|      - |  462 | `		 * directive's own OnUpdate handler, which is why the rule holds at STARTUP` |
|      - |  463 | ``		 * (`-d memory_limit=8K` warns and runs on) exactly as it holds for`` |
|      - |  464 | `		 * ini_set(). Arming a ceiling the interpreter is already past is not a` |
|      - |  465 | `		 * limit, it is a delayed crash: the very next allocation is fatal.` |
|      - |  466 | `		 *` |
|      - |  467 | `		 * Without the rule, a library that PROBES the limit by setting a small one` |
|      - |  468 | `		 * dies instead of learning that it cannot -- monolog's StreamHandler sizes` |
|      - |  469 | `		 * its write chunk exactly that way, and its test walks 1M, 10M, 1024M, 3G` |
|      - |  470 | `		 * in turn, reading the false back and skipping.` |
|      - |  471 | `		 *` |
|      - |  472 | ``		 * php prints this one WITHOUT the `ini_set(): ` prefix its other ini`` |
|      - |  473 | `		 * warnings carry, because it comes from the handler and not from the call. */` |
|      - |  474 | `		char zMsg[160];` |
|    ! 0 |  475 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - |  476 | `			"Failed to set memory limit to %u bytes (Current memory usage is %u bytes)",` |
|    ! 0 |  477 | `			nBytes,pVm->sAllocator.nMemUsed);` |
|    ! 0 |  478 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);` |
|    ! 0 |  479 | `		return 0;` |
|      - |  480 | `	}` |
|     13 |  481 | `	pVm->sAllocator.nMemLimit = bUnlimited ? 0 : nBytes;` |
|     13 |  482 | `	pVm->sAllocator.nMemLimitHit = 0;` |
|     13 |  483 | `	pVm->sAllocator.nMemTried = 0;` |
|     13 |  484 | `	return 1;` |
|      7 |  485 | `}` |
|    323 |  486 | `static void IniLiveSet(ph7_vm *pVm,VmIniSlot *pSlot,const char *zVal,sxu32 nVal)` |
|      5 |  487 | `{` |
|    328 |  488 | `	if( IniNameIs(pSlot,"error_reporting") ){` |
|      3 |  489 | `		sxi32 iVal = 0;` |
|      3 |  490 | `		if( nVal > 0 ){` |
|      3 |  491 | `			SyStrToInt32(zVal,nVal,(void *)&iVal,0);` |
|      1 |  492 | `		}` |
|      3 |  493 | `		pVm->iErrMask = iVal;` |
|      3 |  494 | `		pVm->bErrReport = iVal != 0;` |
|      3 |  495 | `		pVm->bErrMaskSet = 1;` |
|      3 |  496 | `		return;` |
|      - |  497 | `	}` |
|    326 |  498 | `	if( IniNameIs(pSlot,"memory_limit") ){` |
|      5 |  499 | `		(void)PH7_VmApplyMemoryLimit(pVm,zVal,nVal);` |
|      5 |  500 | `		return;` |
|      - |  501 | `	}` |
|    322 |  502 | `	if( IniNameIs(pSlot,"display_errors") ){` |
|      5 |  503 | `		pVm->iDisplayErrors = PH7_VmDisplayErrorsMode(zVal,nVal);` |
|      5 |  504 | `		return;` |
|      - |  505 | `	}` |
|    318 |  506 | `	if( IniNameIs(pSlot,"log_errors") ){` |
|    ! 0 |  507 | `		pVm->bLogErrors = IniTruthy(zVal,nVal);` |
|    ! 0 |  508 | `		return;` |
|      - |  509 | `	}` |
|    318 |  510 | `	if( IniNameIs(pSlot,"error_log") ){` |
|      - |  511 | `		/* Where the LOG copy goes. php re-reads the directive per message rather` |
|      - |  512 | `		 * than holding the file open, so a script that moves it mid-run moves the` |
|      - |  513 | `		 * next line -- and clearing it hands the rest of the run back to the` |
|      - |  514 | `		 * error stream. */` |
|     17 |  515 | `		SyBlobReset(&pVm->sErrLogPath);` |
|     17 |  516 | `		if( nVal > 0 ){` |
|      9 |  517 | `			SyBlobAppend(&pVm->sErrLogPath,zVal,nVal);` |
|      4 |  518 | `		}` |
|     17 |  519 | `		SyBlobNullAppend(&pVm->sErrLogPath);` |
|     17 |  520 | `		return;` |
|      - |  521 | `	}` |
|    302 |  522 | `	if( IniNameIs(pSlot,"session.name") \|\| IniNameIs(pSlot,"session.save_path") ){` |
|     58 |  523 | `		int bPath = IniNameIs(pSlot,"session.save_path");` |
|     58 |  524 | `		SyBlob *pDst = bPath ? &pVm->sSessPath : &pVm->sSessName;` |
|     58 |  525 | `		sxu32 nLen = nVal;` |
|     58 |  526 | `		if( bPath ){` |
|     32 |  527 | `			while( nLen > 0 && zVal[nLen-1] == '/' ){ nLen--; }` |
|     15 |  528 | `		}` |
|     58 |  529 | `		SyBlobReset(pDst);` |
|     58 |  530 | `		SyBlobAppend(pDst,zVal,nLen);` |
|     58 |  531 | `		return;` |
|      - |  532 | `	}` |
|    246 |  533 | `	if( IniNameIs(pSlot,"include_path") ){` |
|      - |  534 | `		/* The one write that moves the include walk. Refused empty by the caller` |
|      - |  535 | `		 * (php's OnUpdateStringUnempty), so nVal is never 0 on the ini_set() path;` |
|      - |  536 | `		 * the boot/-d and ini_restore() paths carry a real value too. */` |
|      5 |  537 | `		PH7_VmSetIncludePath(pVm,zVal,nVal);` |
|      5 |  538 | `		return;` |
|      - |  539 | `	}` |
|    242 |  540 | `	if( IniNameIs(pSlot,"date.timezone") ){` |
|      - |  541 | `		/* Validated already (IniValueAccepted), so what arrives here is a name` |
|      - |  542 | `		 * that resolves. It moves the default UNLESS a script has already named` |
|      - |  543 | `		 * one outright: php latches on date_default_timezone_set(), after which` |
|      - |  544 | `		 * the directive still records what it is handed and the default no` |
|      - |  545 | `		 * longer follows it. The stored spelling is the caller's own bytes. */` |
|      9 |  546 | `		if( pVm->bDefTzExplicit ){` |
|      5 |  547 | `			return;` |
|      - |  548 | `		}` |
|      5 |  549 | `		if( nVal > 0 && nVal < sizeof(pVm->zDefTz) ){` |
|      5 |  550 | `			SyMemcpy(zVal,pVm->zDefTz,nVal);` |
|      5 |  551 | `			pVm->zDefTz[nVal] = 0;` |
|      5 |  552 | `			pVm->nDefTz = nVal;` |
|      2 |  553 | `		}` |
|      4 |  554 | `		return;` |
|      - |  555 | `	}` |
|    161 |  556 | `}` |
|      - |  557 | `/*` |
|      - |  558 | ` * A session directive is settable only while there is no session to disturb: not` |
|      - |  559 | ` * once one is ACTIVE (the store is open and the cookie decided), and not once` |
|      - |  560 | ` * headers have gone out. Answers TRUE (having raised the warning) when the write` |
|      - |  561 | ` * must be refused.` |
|      - |  562 | ` */` |
|    284 |  563 | `static int IniSessionLocked(ph7_context *pCtx,VmIniSlot *pSlot,const char *zFunc)` |
|      5 |  564 | `{` |
|    289 |  565 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  566 | `	char zMsg[160];` |
|      - |  567 | `	const char *zWhy;` |
|    284 |  568 | `	if( pSlot->sName.nByte < sizeof("session.")-1` |
|    287 |  569 | `	 \|\| SyMemcmp(pSlot->sName.zString,"session.",sizeof("session.")-1) != 0 ){` |
|    125 |  570 | `		return 0;` |
|      - |  571 | `	}` |
|    168 |  572 | `	int bActive = (pVm->iSessStatus == 2 /* PHP_SESSION_ACTIVE */);` |
|    168 |  573 | `	if( bActive ){` |
|      5 |  574 | `		zWhy = "when a session is active";` |
|    166 |  575 | `	}else if( pVm->bHeadersSent ){` |
|      3 |  576 | `		zWhy = "after headers have already been sent";` |
|      2 |  577 | `	}else{` |
|    162 |  578 | `		return 0;` |
|      - |  579 | `	}` |
|     11 |  580 | `	SyBufferFormat(zMsg,sizeof(zMsg),` |
|      3 |  581 | `		"%s(): Session ini settings cannot be changed %s",zFunc,zWhy);` |
|      - |  582 | `	{` |
|      - |  583 | `		/* php names the session_start() or the output behind the refusal. */` |
|      - |  584 | `		SyBlob sMsg;` |
|      8 |  585 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      8 |  586 | `		SyBlobAppend(&sMsg,zMsg,(sxu32)SyStrlen(zMsg));` |
|      8 |  587 | `		PH7_VmAppendWhere(pVm,&sMsg,bActive);` |
|      8 |  588 | `		SyBlobNullAppend(&sMsg);` |
|      8 |  589 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|      8 |  590 | `		SyBlobRelease(&sMsg);` |
|      - |  591 | `	}` |
|      8 |  592 | `	return 1;` |
|    140 |  593 | `}` |
|      - |  594 | `/*` |
|      - |  595 | ` * The per-directive value rules php enforces on every write, and the diagnostic` |
|      - |  596 | ` * each one raises. zWho is the whole prefix php puts on it -- "ini_set()" or, when` |
|      - |  597 | ` * session_start() is applying its $options array, "session_start()" -- because php` |
|      - |  598 | ` * blames the call that made the write, not the API underneath it.` |
|      - |  599 | ` */` |
|    342 |  600 | `static int IniValueAccepted(ph7_vm *pVm,VmIniSlot *pSlot,const char *zVal,sxu32 nVal,` |
|      - |  601 | `	const char *zWho)` |
|      5 |  602 | `{` |
|      - |  603 | `	char zMsg[256];` |
|    347 |  604 | `	if( IniNameIs(pSlot,"include_path") && nVal < 1 ){` |
|      - |  605 | `		/* php registers include_path with OnUpdateStringUnempty: the EMPTY value` |
|      - |  606 | `		 * is refused in silence and the directive keeps what it had. */` |
|      3 |  607 | `		return 0;` |
|      - |  608 | `	}` |
|    345 |  609 | `	if( IniNameIs(pSlot,"memory_limit") ){` |
|      - |  610 | `		/* One rule, one warning, one place: the applier owns both, so ini_set() and` |
|      - |  611 | ``		 * the `-d` startup path cannot drift apart. Arming from the validator is`` |
|      - |  612 | `		 * idempotent -- an ACCEPTED value is then written and re-applied through` |
|      - |  613 | `		 * IniLiveSet with the same bytes, and a refused one is never written. */` |
|      5 |  614 | `		return PH7_VmApplyMemoryLimit(pVm,zVal,nVal);` |
|      - |  615 | `	}` |
|    341 |  616 | `	if( IniNameIs(pSlot,"date.timezone") ){` |
|      - |  617 | `		/* The directive and date_default_timezone_set() are one rule, and this is` |
|      - |  618 | `		 * the place php enforces it on every write: a name neither table resolves` |
|      - |  619 | `		 * is REFUSED, with a warning that names the value it kept instead, and` |
|      - |  620 | `		 * the directive is left alone. Same shape as memory_limit above -- the` |
|      - |  621 | ``		 * validator owns it so `ini_set()` and the `-d` startup path cannot`` |
|      - |  622 | `		 * drift apart. */` |
|     14 |  623 | `		int bOk = (nVal == 3` |
|     13 |  624 | `		        && (SyStrnicmp(zVal,"UTC",3) == 0 \|\| SyStrnicmp(zVal,"GMT",3) == 0));` |
|      - |  625 | `#ifdef PH7_ENABLE_TZDB` |
|     13 |  626 | `		if( !bOk ){` |
|     13 |  627 | `			bOk = nVal > 0 && nVal < sizeof(pVm->zDefTz) && PH7_TzFind(zVal,(int)nVal) >= 0;` |
|      6 |  628 | `		}` |
|      - |  629 | `#endif` |
|     13 |  630 | `		if( !bOk ){` |
|      7 |  631 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - |  632 | `				"%s: Invalid date.timezone value '%.*s', using '%.*s' instead",` |
|      4 |  633 | `				zWho,(int)nVal,zVal,(int)pVm->nDefTz,pVm->zDefTz);` |
|      5 |  634 | `			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);` |
|      5 |  635 | `			return 0;` |
|      - |  636 | `		}` |
|      9 |  637 | `		return 1;` |
|      - |  638 | `	}` |
|    324 |  639 | `	if( IniNameIs(pSlot,"syslog.filter")` |
|    171 |  640 | `	 && !(nVal == 3 && SyMemcmp(zVal,"all",3) == 0)` |
|     12 |  641 | `	 && !(nVal == 7 && SyMemcmp(zVal,"no-ctrl",7) == 0)` |
|      8 |  642 | `	 && !(nVal == 5 && SyMemcmp(zVal,"ascii",5) == 0)` |
|     11 |  643 | `	 && !(nVal == 3 && SyMemcmp(zVal,"raw",3) == 0) ){` |
|      - |  644 | `		/* php names the four modes in its OnUpdate handler and refuses anything` |
|      - |  645 | `		 * else in silence, keeping what the directive had -- so` |
|      - |  646 | ``		 * `ini_set('syslog.filter','bogus')` is false and reads back unchanged.`` |
|      - |  647 | ``		 * The match is CASE-SENSITIVE there: `ASCII` is refused where `ascii` is`` |
|      - |  648 | `		 * taken, which is not what most of php's word-valued directives do. */` |
|      3 |  649 | `		return 0;` |
|      - |  650 | `	}` |
|    326 |  651 | `	if( IniNameIs(pSlot,"bcmath.scale") ){` |
|      - |  652 | `		/* php registers it with a 0..INT_MAX bound and refuses anything outside in` |
|      - |  653 | `` 		 * silence, keeping what the directive had -- `ini_set('bcmath.scale','-1')` `` |
|      - |  654 | `		 * is false there. A NON-numeric value is a different matter and is` |
|      - |  655 | `		 * ACCEPTED (stored verbatim, read back as 0), which falls out of the parse` |
|      - |  656 | `		 * below without a rule of its own. */` |
|     25 |  657 | `		sxi64 iVal = 0;` |
|     25 |  658 | `		SyStrToInt64(zVal,nVal,(void *)&iVal,0);` |
|     25 |  659 | `		if( iVal < 0 \|\| iVal > 2147483647 ){` |
|      5 |  660 | `			return 0;` |
|      - |  661 | `		}` |
|     10 |  662 | `	}` |
|    317 |  663 | `	if( IniNameIs(pSlot,"session.serialize_handler")` |
|    161 |  664 | `	 && !(nVal == 3 && SyMemcmp(zVal,"php",3) == 0)` |
|     10 |  665 | `	 && !(nVal == 10 && SyMemcmp(zVal,"php_binary",10) == 0)` |
|     10 |  666 | `	 && !(nVal == 13 && SyMemcmp(zVal,"php_serialize",13) == 0) ){` |
|      - |  667 | `		/* php looks the name up in its registered serializer list and refuses what` |
|      - |  668 | `		 * it cannot find, keeping the directive where it was. */` |
|      8 |  669 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|      2 |  670 | `			"%s: Serialization handler \"%.*s\" cannot be found",zWho,(int)nVal,zVal);` |
|      6 |  671 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);` |
|      6 |  672 | `		return 0;` |
|      - |  673 | `	}` |
|    314 |  674 | `	if( IniNameIs(pSlot,"session.name") ){` |
|      - |  675 | `		/* The name goes out as a COOKIE name and comes back as one, so php holds it` |
|      - |  676 | `		 * to the cookie alphabet -- and refuses a numeric one, which a browser would` |
|      - |  677 | `		 * hand back as an integer array key. */` |
|      - |  678 | `		static const char zBad[] = "=,;.[ \t\r\n\013\014";` |
|      - |  679 | `		sxu32 i;` |
|     36 |  680 | `		int bBad = nVal < 1;` |
|    238 |  681 | `		for( i = 0 ; !bBad && i < nVal ; i++ ){` |
|    204 |  682 | `			if( zVal[i] == 0 \|\| SyByteFind(zBad,sizeof(zBad)-1,zVal[i],0) == SXRET_OK ){` |
|      3 |  683 | `				bBad = 1;` |
|      1 |  684 | `			}` |
|    103 |  685 | `		}` |
|     36 |  686 | `		if( !bBad ){` |
|     32 |  687 | `			sxi64 iDummy = 0;` |
|     32 |  688 | `			bBad = SyStrToInt64(zVal,nVal,(void *)&iDummy,0) == SXRET_OK;` |
|     15 |  689 | `		}` |
|     36 |  690 | `		if( bBad ){` |
|     17 |  691 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - |  692 | `				"%s: session.name \"%.*s\" must not be numeric, empty, contain null bytes"` |
|      - |  693 | `				" or any of the following characters \"=,;.[ \\t\\r\\n\\013\\014\"",` |
|      5 |  694 | `				zWho,(int)nVal,zVal);` |
|     12 |  695 | `			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);` |
|     12 |  696 | `			return 0;` |
|      - |  697 | `		}` |
|     12 |  698 | `	}` |
|    304 |  699 | `	return 1;` |
|    167 |  700 | `}` |
|      - |  701 | `/*` |
|      - |  702 | ` * Store one written value.  A write always leaves a local value; what it does to` |
|      - |  703 | ` * the GLOBAL one depends on whether there was ever a global to keep. php saves` |
|      - |  704 | ` * the original at the first modification and reports THAT as global_value -- but` |
|      - |  705 | ` * an UNSET directive has no original to save, so its global_value starts reading` |
|      - |  706 | ` * the written value instead, and only ini_restore() puts the NULL back.` |
|      - |  707 | ` */` |
|    311 |  708 | `static void IniWriteLocal(VmIniSlot *pSlot,const char *zVal,sxu32 nVal)` |
|      5 |  709 | `{` |
|    316 |  710 | `	SyBlobReset(&pSlot->sLocal);` |
|    316 |  711 | `	SyBlobAppend(&pSlot->sLocal,zVal,nVal);` |
|    316 |  712 | `	pSlot->bLocalNull = 0;` |
|    316 |  713 | `}` |
|      - |  714 | `/*` |
|      - |  715 | ` * What global_value reports: the saved original, unless there was none -- in` |
|      - |  716 | ` * which case it is whatever the directive currently holds, NULL included.` |
|      - |  717 | ` */` |
|   2323 |  718 | `static int IniGlobalValue(VmIniSlot *pSlot,const char **pz,sxu32 *pn)` |
|      4 |  719 | `{` |
|   2327 |  720 | `	if( !pSlot->bGlobalNull ){` |
|   2137 |  721 | `		*pz = (const char *)SyBlobData(&pSlot->sGlobal);` |
|   2137 |  722 | `		*pn = SyBlobLength(&pSlot->sGlobal);` |
|   2137 |  723 | `		return 1;` |
|      - |  724 | `	}` |
|    193 |  725 | `	if( pSlot->bLocalNull ){` |
|    189 |  726 | `		return 0;   /* still unset: php reports NULL */` |
|      - |  727 | `	}` |
|      5 |  728 | `	*pz = (const char *)SyBlobData(&pSlot->sLocal);` |
|      5 |  729 | `	*pn = SyBlobLength(&pSlot->sLocal);` |
|      5 |  730 | `	return 1;` |
|   1129 |  731 | `}` |
|      - |  732 | `/*` |
|      - |  733 | ` * Write a directive from C, the way ini_set() writes it. Answers 0 when the write` |
|      - |  734 | ` * was refused (unknown name, not user-settable, or a value the directive's own` |
|      - |  735 | ` * rule rejects) -- which is exactly what session_start()'s $options reports as` |
|      - |  736 | `` * `Setting option "%s" failed`.`` |
|      - |  737 | ` */` |
|     74 |  738 | `PH7_PRIVATE int PH7_VmIniSet(ph7_vm *pVm,const char *zName,sxu32 nName,` |
|      - |  739 | `	const char *zVal,sxu32 nVal,const char *zWho)` |
|      5 |  740 | `{` |
|      - |  741 | `	VmIniSlot *pSlot;` |
|     79 |  742 | `	if( IniSeed(pVm) != SXRET_OK ){` |
|    ! 0 |  743 | `		return 0;` |
|      - |  744 | `	}` |
|     79 |  745 | `	pSlot = IniFind(pVm,zName,nName);` |
|     79 |  746 | `	if( pSlot == 0 \|\| (pSlot->iAccess & VM_INI_USER) == 0 ){` |
|      3 |  747 | `		return 0;` |
|      - |  748 | `	}` |
|     77 |  749 | `	if( !IniValueAccepted(pVm,pSlot,zVal,nVal,zWho) ){` |
|      7 |  750 | `		return 0;` |
|      - |  751 | `	}` |
|     71 |  752 | `	IniWriteLocal(pSlot,zVal,nVal);` |
|     71 |  753 | `	IniLiveSet(pVm,pSlot,zVal,nVal);` |
|     71 |  754 | `	return 1;` |
|     42 |  755 | `}` |
|      - |  756 | `/* string\|false ini_get(string $option) */` |
|   1416 |  757 | `static int vm_builtin_ini_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  758 | `{` |
|   1421 |  759 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  760 | `	VmIniSlot *pSlot;` |
|      - |  761 | `	const char *zName;` |
|   1421 |  762 | `	int nName = 0;` |
|      - |  763 | `	SyBlob sOut;` |
|   1421 |  764 | `	if( nArg < 1 \|\| IniSeed(pVm) != SXRET_OK ){` |
|    ! 0 |  765 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  766 | `		return PH7_OK;` |
|      - |  767 | `	}` |
|   1421 |  768 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|   1421 |  769 | `	pSlot = IniFind(pVm,zName,(sxu32)nName);` |
|   1421 |  770 | `	if( pSlot == 0 ){` |
|     13 |  771 | `		ph7_result_bool(pCtx,0);` |
|     13 |  772 | `		return PH7_OK;` |
|      - |  773 | `	}` |
|   1411 |  774 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|   1411 |  775 | `	IniLiveGet(pVm,pSlot,&sOut);` |
|   1411 |  776 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|   1411 |  777 | `	SyBlobRelease(&sOut);` |
|   1411 |  778 | `	return PH7_OK;` |
|    709 |  779 | `}` |
|      - |  780 | `/* string\|false ini_set(string $option, string\|int\|float\|bool\|null $value) */` |
|    282 |  781 | `static int vm_builtin_ini_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  782 | `{` |
|    287 |  783 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  784 | `	VmIniSlot *pSlot;` |
|      - |  785 | `	const char *zName;` |
|      - |  786 | `	const char *zVal;` |
|    287 |  787 | `	int nName = 0, nVal = 0;` |
|      - |  788 | `	SyBlob sOld;` |
|      - |  789 | `	char zWho[64];` |
|    287 |  790 | `	if( nArg < 2 \|\| IniSeed(pVm) != SXRET_OK ){` |
|    ! 0 |  791 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  792 | `		return PH7_OK;` |
|      - |  793 | `	}` |
|    287 |  794 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|    287 |  795 | `	pSlot = IniFind(pVm,zName,(sxu32)nName);` |
|    287 |  796 | `	if( pSlot == 0 \|\| (pSlot->iAccess & VM_INI_USER) == 0 ){` |
|     15 |  797 | `		ph7_result_bool(pCtx,0);` |
|     15 |  798 | `		return PH7_OK;` |
|      - |  799 | `	}` |
|    275 |  800 | `	if( IniSessionLocked(pCtx,pSlot,ph7_function_name(pCtx)) ){` |
|      6 |  801 | `		ph7_result_bool(pCtx,0);` |
|      6 |  802 | `		return PH7_OK;` |
|      - |  803 | `	}` |
|      - |  804 | `	/* php: the -1 (compiled-out) state of zend.assertions is a php.ini-only switch,` |
|      - |  805 | `	 * and so is moving INTO it. Unprefixed warning, exactly as php prints it. */` |
|    271 |  806 | `	if( IniNameIs(pSlot,"zend.assertions") ){` |
|    ! 0 |  807 | `		int bGlobalOff = SyBlobLength(&pSlot->sGlobal) == 2` |
|    ! 0 |  808 | `			&& SyMemcmp(SyBlobData(&pSlot->sGlobal),"-1",2) == 0;` |
|    ! 0 |  809 | `		const char *zNew = ph7_value_to_string(apArg[1],&nVal);` |
|    ! 0 |  810 | `		int bNewOff = nVal == 2 && SyMemcmp(zNew,"-1",2) == 0;` |
|    ! 0 |  811 | `		if( bGlobalOff \|\| bNewOff ){` |
|    ! 0 |  812 | `			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|      - |  813 | `				"zend.assertions may be completely enabled or disabled only in php.ini");` |
|    ! 0 |  814 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 |  815 | `			return PH7_OK;` |
|      - |  816 | `		}` |
|    ! 0 |  817 | `	}` |
|      - |  818 | `	/* The OLD value php answers is the effective one, read before the write. */` |
|    271 |  819 | `	SyBlobInit(&sOld,&pVm->sAllocator);` |
|    271 |  820 | `	IniLiveGet(pVm,pSlot,&sOld);` |
|      - |  821 | `	/* php stringifies the incoming value, with a bool becoming "1"/"" . */` |
|    271 |  822 | `	if( ph7_value_is_bool(apArg[1]) ){` |
|    ! 0 |  823 | `		zVal = ph7_value_to_bool(apArg[1]) ? "1" : "";` |
|    ! 0 |  824 | `		nVal = (int)SyStrlen(zVal);` |
|    ! 0 |  825 | `	}else{` |
|    271 |  826 | `		zVal = ph7_value_to_string(apArg[1],&nVal);` |
|      - |  827 | `	}` |
|    271 |  828 | `	SyBufferFormat(zWho,sizeof(zWho),"%s()",ph7_function_name(pCtx));` |
|    271 |  829 | `	if( !IniValueAccepted(pVm,pSlot,zVal,(sxu32)nVal,zWho) ){` |
|     24 |  830 | `		SyBlobRelease(&sOld);` |
|     24 |  831 | `		ph7_result_bool(pCtx,0);` |
|     24 |  832 | `		return PH7_OK;` |
|      - |  833 | `	}` |
|    250 |  834 | `	IniWriteLocal(pSlot,zVal,(sxu32)nVal);` |
|    250 |  835 | `	IniLiveSet(pVm,pSlot,zVal,(sxu32)nVal);` |
|    250 |  836 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOld),(int)SyBlobLength(&sOld));` |
|    250 |  837 | `	SyBlobRelease(&sOld);` |
|    250 |  838 | `	return PH7_OK;` |
|    138 |  839 | `}` |
|      - |  840 | `/* void ini_restore(string $option) */` |
|     14 |  841 | `static int vm_builtin_ini_restore(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  842 | `{` |
|     17 |  843 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  844 | `	VmIniSlot *pSlot;` |
|      - |  845 | `	const char *zName;` |
|     17 |  846 | `	int nName = 0;` |
|     17 |  847 | `	ph7_result_null(pCtx);` |
|     17 |  848 | `	if( nArg < 1 \|\| IniSeed(pVm) != SXRET_OK ){` |
|    ! 0 |  849 | `		return PH7_OK;` |
|      - |  850 | `	}` |
|     17 |  851 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|     17 |  852 | `	pSlot = IniFind(pVm,zName,(sxu32)nName);` |
|     17 |  853 | `	if( pSlot == 0 \|\| IniSessionLocked(pCtx,pSlot,"ini_restore") ){` |
|      3 |  854 | `		return PH7_OK;` |
|      - |  855 | `	}` |
|     15 |  856 | `	SyBlobReset(&pSlot->sLocal);` |
|     15 |  857 | `	SyBlobAppend(&pSlot->sLocal,SyBlobData(&pSlot->sGlobal),SyBlobLength(&pSlot->sGlobal));` |
|     15 |  858 | `	pSlot->bLocalNull = pSlot->bGlobalNull;   /* an unset directive goes back to unset */` |
|     15 |  859 | `	IniLiveSet(pVm,pSlot,(const char *)SyBlobData(&pSlot->sGlobal),SyBlobLength(&pSlot->sGlobal));` |
|     15 |  860 | `	return PH7_OK;` |
|     10 |  861 | `}` |
|      - |  862 | `/*` |
|      - |  863 | ` * The extension id php's module registry would find for this name: the key it` |
|      - |  864 | ` * stores is the extension name FOLDED, and the lookup against it is exact.` |
|      - |  865 | ` */` |
|     58 |  866 | `static int VmIniExtensionId(const char *zName,int nName)` |
|      3 |  867 | `{` |
|      - |  868 | `	int iExt,n;` |
|    981 |  869 | `	for( iExt = 0 ; iExt < PH7_VmExtensionCount() ; ++iExt ){` |
|      - |  870 | `		const char *zCanon;` |
|    965 |  871 | `		if( !PH7_VmExtensionAvailable(iExt) ){` |
|      1 |  872 | `			continue;` |
|      - |  873 | `		}` |
|    965 |  874 | `		zCanon = PH7_VmExtensionName(iExt);` |
|    965 |  875 | `		if( (int)SyStrlen(zCanon) != nName ){` |
|    829 |  876 | `			continue;` |
|      - |  877 | `		}` |
|    391 |  878 | `		for( n = 0 ; n < nName ; ++n ){` |
|    349 |  879 | `			if( (char)SyToLower(zCanon[n]) != zName[n] ){` |
|     95 |  880 | `				break;` |
|      - |  881 | `			}` |
|    129 |  882 | `		}` |
|    139 |  883 | `		if( n == nName ){` |
|     45 |  884 | `			return iExt;` |
|      - |  885 | `		}` |
|     48 |  886 | `	}` |
|     17 |  887 | `	return -1;` |
|     32 |  888 | `}` |
|      - |  889 | `/* array\|false ini_get_all(?string $extension = null, bool $details = true) */` |
|     91 |  890 | `static int vm_builtin_ini_get_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  891 | `{` |
|     95 |  892 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  893 | `	VmIniSlot *aSlot;` |
|      - |  894 | `	ph7_value *pOut,*pCur;` |
|     95 |  895 | `	const char *zExt = 0;` |
|     95 |  896 | `	int nExt = 0, bDetails = 1, iExtSel = -1;` |
|      - |  897 | `	sxu32 n;` |
|      - |  898 | `	SyBlob sVal;` |
|     95 |  899 | `	if( IniSeed(pVm) != SXRET_OK ){` |
|    ! 0 |  900 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  901 | `		return PH7_OK;` |
|      - |  902 | `	}` |
|     95 |  903 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|     61 |  904 | `		zExt = ph7_value_to_string(apArg[0],&nExt);` |
|     29 |  905 | `	}` |
|     95 |  906 | `	if( nArg > 1 ){` |
|     30 |  907 | `		bDetails = ph7_value_to_bool(apArg[1]);` |
|     14 |  908 | `	}` |
|     95 |  909 | `	if( zExt ){` |
|      - |  910 | `		/* php looks the name up in the MODULE REGISTRY, whose key is the extension` |
|      - |  911 | `		 * name folded down -- so the match is case-SENSITIVE against that key:` |
|      - |  912 | ``		 * `core` and `spl` are found, `Core` and `SPL` are not, and every other`` |
|      - |  913 | `		 * name is a warning and false. An extension that owns no directive is` |
|      - |  914 | ``		 * still found and answers the EMPTY array; a `phl.stub_extensions` name`` |
|      - |  915 | `		 * is no module and is not found. This engine used to accept five names` |
|      - |  916 | `		 * spelled its own way and refuse the rest. */` |
|     61 |  917 | `		iExtSel = VmIniExtensionId(zExt,nExt);` |
|     61 |  918 | `		if( iExtSel < 0 ){` |
|     25 |  919 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      8 |  920 | `				"Extension \"%.*s\" cannot be found",nExt,zExt);` |
|     17 |  921 | `			ph7_result_bool(pCtx,0);` |
|     17 |  922 | `			return PH7_OK;` |
|      - |  923 | `		}` |
|     21 |  924 | `	}` |
|     79 |  925 | `	pOut = ph7_context_new_array(pCtx);` |
|     79 |  926 | `	pCur = ph7_context_new_scalar(pCtx);` |
|     79 |  927 | `	if( pOut == 0 \|\| pCur == 0 ){` |
|    ! 0 |  928 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  929 | `	}` |
|     79 |  930 | `	SyBlobInit(&sVal,&pVm->sAllocator);` |
|     79 |  931 | `	aSlot = (VmIniSlot *)SySetBasePtr(&pVm->aIniTab);` |
|      - |  932 | `	/* The table is stored sorted, so this walk is already php's ksort order. */` |
|   5554 |  933 | `	for( n = 0 ; n < SySetUsed(&pVm->aIniTab) ; n++ ){` |
|   5479 |  934 | `		VmIniSlot *pSlot = &aSlot[n];` |
|      - |  935 | `		char zKey[128];` |
|      - |  936 | `		/* A directive belongs where the extension partition puts it rather than` |
|      - |  937 | `` 		 * where its NAME points -- php's `standard` owns `session.trans_sid_tags` `` |
|      - |  938 | ``		 * and its `Core` owns none of the `session.*` ones. `core` is php's own`` |
|      - |  939 | `		 * exception and answers EVERY directive whatever module registered it. */` |
|   5475 |  940 | `		if( iExtSel >= 0 && iExtSel != PH7_EXT_CORE` |
|   2851 |  941 | `		 && PH7_VmExtOfIni(pSlot->sName.zString,(int)pSlot->sName.nByte) != iExtSel ){` |
|   2425 |  942 | `			continue;` |
|      - |  943 | `		}` |
|   3057 |  944 | `		if( pSlot->sName.nByte >= sizeof(zKey) ){` |
|    ! 0 |  945 | `			continue;` |
|      - |  946 | `		}` |
|   3057 |  947 | `		SyMemcpy(pSlot->sName.zString,zKey,pSlot->sName.nByte);` |
|   3057 |  948 | `		zKey[pSlot->sName.nByte] = 0;` |
|   3057 |  949 | `		IniLiveGet(pVm,pSlot,&sVal);` |
|   3057 |  950 | `		if( bDetails ){` |
|   2327 |  951 | `			ph7_value *pRow = ph7_context_new_array(pCtx);` |
|   2327 |  952 | `			if( pRow == 0 ){` |
|    ! 0 |  953 | `				break;` |
|      - |  954 | `			}` |
|      - |  955 | `			{` |
|   2327 |  956 | `				const char *zG = 0;` |
|   2327 |  957 | `				sxu32 nG = 0;` |
|   2327 |  958 | `				if( IniGlobalValue(pSlot,&zG,&nG) ){` |
|   2141 |  959 | `					ph7_value_string(pCur,zG,(int)nG);` |
|   1039 |  960 | `				}else{` |
|    189 |  961 | `					ph7_value_null(pCur);` |
|      - |  962 | `				}` |
|      - |  963 | `			}` |
|   2327 |  964 | `			ph7_array_add_strkey_elem(pRow,"global_value",pCur);` |
|   2327 |  965 | `			ph7_value_reset_string_cursor(pCur);` |
|   2327 |  966 | `			if( pSlot->bLocalNull ){` |
|    189 |  967 | `				ph7_value_null(pCur);` |
|     93 |  968 | `			}else{` |
|   2141 |  969 | `				ph7_value_string(pCur,(const char *)SyBlobData(&sVal),(int)SyBlobLength(&sVal));` |
|      - |  970 | `			}` |
|   2327 |  971 | `			ph7_array_add_strkey_elem(pRow,"local_value",pCur);` |
|   2327 |  972 | `			ph7_value_reset_string_cursor(pCur);` |
|   2327 |  973 | `			ph7_value_int(pCur,pSlot->iAccess);` |
|   2327 |  974 | `			ph7_array_add_strkey_elem(pRow,"access",pCur);` |
|   2327 |  975 | `			ph7_value_reset_string_cursor(pCur);` |
|   2327 |  976 | `			ph7_array_add_strkey_elem(pOut,zKey,pRow);` |
|   1129 |  977 | `		}else{` |
|    732 |  978 | `			ph7_value_reset_string_cursor(pCur);` |
|    732 |  979 | `			if( pSlot->bLocalNull ){` |
|     62 |  980 | `				ph7_value_null(pCur);` |
|     32 |  981 | `			}else{` |
|    672 |  982 | `				ph7_value_string(pCur,(const char *)SyBlobData(&sVal),(int)SyBlobLength(&sVal));` |
|      - |  983 | `			}` |
|    732 |  984 | `			ph7_array_add_strkey_elem(pOut,zKey,pCur);` |
|    732 |  985 | `			ph7_value_reset_string_cursor(pCur);` |
|      - |  986 | `		}` |
|   1494 |  987 | `	}` |
|     79 |  988 | `	SyBlobRelease(&sVal);` |
|     79 |  989 | `	ph7_result_value(pCtx,pOut);` |
|     79 |  990 | `	return PH7_OK;` |
|     49 |  991 | `}` |
|      - |  992 | `/* string\|false get_cfg_var(string $option) — php answers the GLOBAL value */` |
|     14 |  993 | `static int vm_builtin_get_cfg_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  994 | `{` |
|     16 |  995 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  996 | `	VmIniSlot *pSlot;` |
|      - |  997 | `	const char *zName;` |
|     16 |  998 | `	int nName = 0;` |
|     16 |  999 | `	if( nArg < 1 \|\| IniSeed(pVm) != SXRET_OK ){` |
|    ! 0 | 1000 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1001 | `		return PH7_OK;` |
|      - | 1002 | `	}` |
|     16 | 1003 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|     16 | 1004 | `	pSlot = IniFind(pVm,zName,(sxu32)nName);` |
|     16 | 1005 | `	if( pSlot == 0 \|\| pSlot->bGlobalNull ){` |
|      - | 1006 | `		/* php reads this one from the php.ini FILE rather than from the live` |
|      - | 1007 | `		 * directive, so a name the file never mentioned is false -- and a` |
|      - | 1008 | `		 * directive declared with no value is exactly such a name, whatever a` |
|      - | 1009 | `		 * later ini_set() put in it. */` |
|      9 | 1010 | `		ph7_result_bool(pCtx,0);` |
|      9 | 1011 | `		return PH7_OK;` |
|      - | 1012 | `	}` |
|     11 | 1013 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&pSlot->sGlobal),` |
|      6 | 1014 | `		(int)SyBlobLength(&pSlot->sGlobal));` |
|      8 | 1015 | `	return PH7_OK;` |
|      9 | 1016 | `}` |
|      - | 1017 | `/*` |
|      - | 1018 | ` * The effective value of a directive a C builtin needs to obey, as an integer.` |
|      - | 1019 | ` * parse_str() reads max_input_vars and max_input_nesting_level through this;` |
|      - | 1020 | ` * without it a builtin would have to duplicate php's default and could never` |
|      - | 1021 | `` * see an `ini_set()`/`-d` override. Answers iDefault when the directive is`` |
|      - | 1022 | ` * absent or unparsable.` |
|      - | 1023 | ` */` |
|   1819 | 1024 | `PH7_PRIVATE sxi64 PH7_VmIniGetInt(ph7_vm *pVm,const char *zName,sxi64 iDefault)` |
|      5 | 1025 | `{` |
|      - | 1026 | `	VmIniSlot *pSlot;` |
|      - | 1027 | `	SyBlob sVal;` |
|   1824 | 1028 | `	sxi64 iVal = iDefault;` |
|   1824 | 1029 | `	IniSeed(pVm);` |
|   1824 | 1030 | `	pSlot = IniFind(pVm,zName,(sxu32)SyStrlen(zName));` |
|   1824 | 1031 | `	if( pSlot == 0 ){` |
|    ! 0 | 1032 | `		return iDefault;` |
|      - | 1033 | `	}` |
|   1824 | 1034 | `	SyBlobInit(&sVal,&pVm->sAllocator);` |
|   1824 | 1035 | `	IniLiveGet(pVm,pSlot,&sVal);` |
|   1824 | 1036 | `	if( SyBlobLength(&sVal) > 0 ){` |
|   1824 | 1037 | `		SyStrToInt64((const char *)SyBlobData(&sVal),SyBlobLength(&sVal),(void *)&iVal,0);` |
|    910 | 1038 | `	}` |
|   1824 | 1039 | `	SyBlobRelease(&sVal);` |
|   1824 | 1040 | `	return iVal;` |
|    915 | 1041 | `}` |
|      - | 1042 | `/* The same, as a borrowed STRING (arg_separator.input). The bytes live in the` |
|      - | 1043 | ` * caller's blob, which it owns. */` |
|      - | 1044 | `/*` |
|      - | 1045 | ` * One directive as the export format needs it: php's access bitmask, the value` |
|      - | 1046 | `` * a script reads now, and the DEFAULT behind it. `pOut`/`pDef` are the`` |
|      - | 1047 | ` * caller's blobs. Answers 0 when the build has no such directive.` |
|      - | 1048 | ` */` |
|     62 | 1049 | `PH7_PRIVATE int PH7_VmIniDescribe(ph7_vm *pVm,const char *zName,sxu32 nName,` |
|      - | 1050 | `	sxi32 *piAccess,SyBlob *pOut,SyBlob *pDef)` |
|      1 | 1051 | `{` |
|      - | 1052 | `	VmIniSlot *pSlot;` |
|     63 | 1053 | `	IniSeed(&(*pVm));` |
|     63 | 1054 | `	pSlot = IniFind(&(*pVm),zName,nName);` |
|     63 | 1055 | `	if( pSlot == 0 ){` |
|    ! 0 | 1056 | `		return 0;` |
|      - | 1057 | `	}` |
|     63 | 1058 | `	*piAccess = pSlot->iAccess;` |
|     63 | 1059 | `	IniLiveGet(&(*pVm),pSlot,pOut);` |
|     63 | 1060 | `	SyBlobReset(pDef);` |
|     63 | 1061 | `	SyBlobAppend(pDef,SyBlobData(&pSlot->sGlobal),SyBlobLength(&pSlot->sGlobal));` |
|     63 | 1062 | `	return 1;` |
|     32 | 1063 | `}` |
|   1655 | 1064 | `PH7_PRIVATE void PH7_VmIniGetStr(ph7_vm *pVm,const char *zName,SyBlob *pOut)` |
|      5 | 1065 | `{` |
|      - | 1066 | `	VmIniSlot *pSlot;` |
|   1660 | 1067 | `	IniSeed(pVm);` |
|   1660 | 1068 | `	SyBlobReset(pOut);` |
|   1660 | 1069 | `	pSlot = IniFind(pVm,zName,(sxu32)SyStrlen(zName));` |
|   1660 | 1070 | `	if( pSlot ){` |
|   1578 | 1071 | `		IniLiveGet(pVm,pSlot,pOut);` |
|    747 | 1072 | `	}` |
|   1660 | 1073 | `}` |
|      - | 1074 | `/*` |
|      - | 1075 | ` * Does this directive currently hold php's UNSET value? Only the surfaces that` |
|      - | 1076 | ` * show a RAW value ask -- ini_get() answers the empty string for one either` |
|      - | 1077 | ` * way, which is why the question has to be put separately.` |
|      - | 1078 | ` */` |
|    740 | 1079 | `PH7_PRIVATE int PH7_VmIniIsUnset(ph7_vm *pVm,const char *zName)` |
|      3 | 1080 | `{` |
|      - | 1081 | `	VmIniSlot *pSlot;` |
|    743 | 1082 | `	IniSeed(pVm);` |
|    743 | 1083 | `	pSlot = IniFind(pVm,zName,(sxu32)SyStrlen(zName));` |
|    743 | 1084 | `	return pSlot != 0 && pSlot->bLocalNull;` |
|      3 | 1085 | `}` |
|      - | 1086 | `/*` |
|      - | 1087 | ` * Read a BOOLEAN directive the way zend_ini does — "on"/"yes"/"true" as well as` |
|      - | 1088 | ` * a non-zero number. Reading one through the integer parser answers 0 for` |
|      - | 1089 | `` * `On`, which is the spelling php.ini-production ships, so a directive written`` |
|      - | 1090 | ` * that way reads as OFF.` |
|      - | 1091 | ` */` |
|   1297 | 1092 | `PH7_PRIVATE int PH7_VmIniGetBool(ph7_vm *pVm,const char *zName,int bDefault)` |
|      5 | 1093 | `{` |
|      - | 1094 | `	VmIniSlot *pSlot;` |
|      - | 1095 | `	SyBlob sVal;` |
|      - | 1096 | `	int bRes;` |
|   1302 | 1097 | `	IniSeed(pVm);` |
|   1302 | 1098 | `	pSlot = IniFind(pVm,zName,(sxu32)SyStrlen(zName));` |
|   1302 | 1099 | `	if( pSlot == 0 ){` |
|    ! 0 | 1100 | `		return bDefault;` |
|      - | 1101 | `	}` |
|   1302 | 1102 | `	SyBlobInit(&sVal,&pVm->sAllocator);` |
|   1302 | 1103 | `	IniLiveGet(pVm,pSlot,&sVal);` |
|   1302 | 1104 | `	bRes = IniTruthy((const char *)SyBlobData(&sVal),SyBlobLength(&sVal));` |
|   1302 | 1105 | `	SyBlobRelease(&sVal);` |
|   1302 | 1106 | `	return bRes;` |
|    644 | 1107 | `}` |
|   7925 | 1108 | `PH7_PRIVATE sxi32 PH7_VmInstallIni(ph7_vm *pVm)` |
|      5 | 1109 | `{` |
|      - | 1110 | `	static const struct {` |
|      - | 1111 | `		const char *zName;` |
|      - | 1112 | `		ProchHostFunction xFunc;` |
|      - | 1113 | `	} aFunc[] = {` |
|      - | 1114 | `		{ "ini_get",      vm_builtin_ini_get      },` |
|      - | 1115 | `		{ "ini_set",      vm_builtin_ini_set      },` |
|      - | 1116 | `		/* php's alias for the same routine. Both of the diagnostics a write` |
|      - | 1117 | `		 * can raise name the INVOKED function, so they read the context's` |
|      - | 1118 | `		 * name rather than a literal. */` |
|      - | 1119 | `		{ "ini_alter",    vm_builtin_ini_set      },` |
|      - | 1120 | `		{ "ini_restore",  vm_builtin_ini_restore  },` |
|      - | 1121 | `		{ "ini_get_all",  vm_builtin_ini_get_all  },` |
|      - | 1122 | `		{ "get_cfg_var",  vm_builtin_get_cfg_var  },` |
|      - | 1123 | `	};` |
|      - | 1124 | `	sxu32 n;` |
|  55480 | 1125 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFunc) ; n++ ){` |
|  47555 | 1126 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
|  23747 | 1127 | `	}` |
|   7930 | 1128 | `	return SXRET_OK;` |
|      5 | 1129 | `}` |
|      - | 1130 | `#else` |
|      - | 1131 | `PH7_PRIVATE sxi32 PH7_VmInstallIni(ph7_vm *pVm){ (void)pVm; return SXRET_OK; }` |
|      - | 1132 | `PH7_PRIVATE sxi64 PH7_VmIniGetInt(ph7_vm *pVm,const char *zName,sxi64 iDefault){` |
|      - | 1133 | `	(void)pVm; (void)zName; return iDefault;` |
|      - | 1134 | `}` |
|      - | 1135 | `PH7_PRIVATE int PH7_VmIniGetBool(ph7_vm *pVm,const char *zName,int bDefault){` |
|      - | 1136 | `	(void)pVm; (void)zName; return bDefault;` |
|      - | 1137 | `}` |
|      - | 1138 | `PH7_PRIVATE void PH7_VmIniGetStr(ph7_vm *pVm,const char *zName,SyBlob *pOut){` |
|      - | 1139 | `	(void)pVm; (void)zName; SyBlobReset(pOut);` |
|      - | 1140 | `}` |
|      - | 1141 | `PH7_PRIVATE int PH7_VmIniIsUnset(ph7_vm *pVm,const char *zName){` |
|      - | 1142 | `	(void)pVm; (void)zName; return 0;` |
|      - | 1143 | `}` |
|      - | 1144 | `PH7_PRIVATE int PH7_VmIniDescribe(ph7_vm *pVm,const char *zName,sxu32 nName,` |
|      - | 1145 | `	sxi32 *piAccess,SyBlob *pOut,SyBlob *pDef){` |
|      - | 1146 | `	(void)pVm; (void)zName; (void)nName; (void)piAccess;` |
|      - | 1147 | `	SyBlobReset(pOut); SyBlobReset(pDef); return 0;` |
|      - | 1148 | `}` |
|      - | 1149 | `PH7_PRIVATE int PH7_VmIniSet(ph7_vm *pVm,const char *zName,sxu32 nName,` |
|      - | 1150 | `	const char *zVal,sxu32 nVal,const char *zWho){` |
|      - | 1151 | `	(void)pVm; (void)zName; (void)nName; (void)zVal; (void)nVal; (void)zWho; return 0;` |
|      - | 1152 | `}` |
|      - | 1153 | `#endif` |
|      - | 1154 |  |
