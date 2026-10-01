# src/ph7/vm_builtin_ini.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 513/577 lines (88.91%)

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
|      - |   57 | `	{ "default_charset",          "UTF-8",      VM_INI_ALL },` |
|      - |   58 | `	/* php bounds a socket wait by this rather than waiting forever, and it is` |
|      - |   59 | `	 * where stream_socket_accept() takes its default timeout from. */` |
|      - |   60 | `	{ "default_socket_timeout",   "60",         VM_INI_ALL },` |
|      - |   61 | `	{ "default_mimetype",         "text/html",  VM_INI_ALL },` |
|      - |   62 | `	{ "display_errors",           "",           VM_INI_ALL },` |
|      - |   63 | `	{ "error_log",                0,            VM_INI_ALL },` |
|      - |   64 | `	{ "error_reporting",          "30719",      VM_INI_ALL },` |
|      - |   65 | `	/* The From: header the http:// wrapper writes. php ships it UNSET, and the` |
|      - |   66 | `	 * difference matters: an unset directive writes no header at all, while an` |
|      - |   67 | ``	 * ini_set() to the EMPTY string writes `From: ` -- which is not how`` |
|      - |   68 | `	 * user_agent below behaves. */` |
|      - |   69 | `	{ "from",                     0,            VM_INI_ALL },` |
|      - |   70 | `	{ "highlight.comment",        "#FF8000",    VM_INI_ALL },` |
|      - |   71 | `	{ "highlight.default",        "#0000BB",    VM_INI_ALL },` |
|      - |   72 | `	{ "highlight.html",           "#000000",    VM_INI_ALL },` |
|      - |   73 | `	{ "highlight.keyword",        "#007700",    VM_INI_ALL },` |
|      - |   74 | `	{ "highlight.string",         "#DD0000",    VM_INI_ALL },` |
|      - |   75 | `	/* ignore_user_abort(): the directive the function reads and writes. php` |
|      - |   76 | `	 * spells its default "0" rather than the empty string every other boolean` |
|      - |   77 | `	 * directive here uses, and ini_get() answers that byte. */` |
|      - |   78 | `	{ "ignore_user_abort",        "0",          VM_INI_ALL },` |
|      - |   79 | `	{ "include_path",             ".",          VM_INI_ALL },` |
|      - |   80 | `	{ "log_errors",               "1",          VM_INI_ALL },` |
|      - |   81 | `	{ "max_execution_time",       "0",          VM_INI_ALL },` |
|      - |   82 | `	{ "max_input_nesting_level",  "64",         VM_INI_PERDIR\|VM_INI_SYSTEM },` |
|      - |   83 | `	{ "max_input_vars",           "1000",       VM_INI_PERDIR\|VM_INI_SYSTEM },` |
|      - |   84 | `	{ "memory_limit",             "-1",         VM_INI_ALL },` |
|      - |   85 | `#ifdef PH7_ENABLE_OPENSSL` |
|      - |   86 | `	/* ext/openssl's two path directives, php's own defaults (both UNSET, the` |
|      - |   87 | `	 * empty string) and its access mask (PHP_INI_PERDIR for both). They are` |
|      - |   88 | `	 * what openssl_get_cert_locations() reports as ini_cafile/ini_capath and` |
|      - |   89 | `	 * what a TLS peer verification falls back to when a stream context names` |
|      - |   90 | `	 * no CA of its own. Both ship UNSET rather than empty, which is what php` |
|      - |   91 | `	 * reports and is a different thing: ini_get_all() answers NULL for an` |
|      - |   92 | ``	 * unset directive and "" for one set to the empty string. php's third, `openssl.libctx`, is not here: it picks`` |
|      - |   93 | `	 * between the default OpenSSL library context and a private one, and this` |
|      - |   94 | `	 * build has only the default -- registering the name would report a` |
|      - |   95 | `	 * choice that is not being made. */` |
|      - |   96 | `	{ "openssl.cafile",           0,            VM_INI_PERDIR },` |
|      - |   97 | `	{ "openssl.capath",           0,            VM_INI_PERDIR },` |
|      - |   98 | `#endif` |
|      - |   99 | ``	/* ext/phar's three. `phar.readonly` is php's own default ON: every write`` |
|      - |  100 | `	 * door refuses until an installer turns it off, which is why building an` |
|      - |  101 | ``	 * archive is a `-d phar.readonly=0` job on a stock php too. */`` |
|      - |  102 | `	{ "phar.cache_list",          "",           VM_INI_SYSTEM },` |
|      - |  103 | `	{ "phar.readonly",            "1",          VM_INI_ALL },` |
|      - |  104 | `	{ "phar.require_hash",        "1",          VM_INI_ALL },` |
|      - |  105 | `	{ "post_max_size",            "8M",         VM_INI_PERDIR\|VM_INI_SYSTEM },` |
|      - |  106 | `	{ "precision",                "14",         VM_INI_ALL },` |
|      - |  107 | `	{ "serialize_precision",      "-1",         VM_INI_ALL },` |
|      - |  108 | `	/* php's session directives, the whole non-deprecated set: session_start()'s` |
|      - |  109 | `	 * $options array applies its keys THROUGH this table, so a directive missing` |
|      - |  110 | `	 * here is an option php accepts and PHL reports as failed.` |
|      - |  111 | `	 * Six are absent on purpose: php 8.4 DEPRECATES session.sid_length,` |
|      - |  112 | `	 * session.sid_bits_per_character, session.referer_check, session.use_trans_sid,` |
|      - |  113 | `	 * session.trans_sid_tags and session.trans_sid_hosts, and §10 does not carry` |
|      - |  114 | `	 * php's deprecated surface. The session.upload_progress.* family goes with the` |
|      - |  115 | ``	 * file uploads §10 excludes from a CLI-plus-`-S` engine. */`` |
|      - |  116 | `	{ "session.auto_start",       "0",          VM_INI_PERDIR },` |
|      - |  117 | `	{ "session.cache_expire",     "180",        VM_INI_ALL },` |
|      - |  118 | `	{ "session.cache_limiter",    "nocache",    VM_INI_ALL },` |
|      - |  119 | `	/* The Set-Cookie the session sends is built out of these seven. */` |
|      - |  120 | `	{ "session.cookie_domain",    "",           VM_INI_ALL },` |
|      - |  121 | `	{ "session.cookie_httponly",  "0",          VM_INI_ALL },` |
|      - |  122 | `	{ "session.cookie_lifetime",  "0",          VM_INI_ALL },` |
|      - |  123 | `	{ "session.cookie_partitioned","0",         VM_INI_ALL },` |
|      - |  124 | `	{ "session.cookie_path",      "/",          VM_INI_ALL },` |
|      - |  125 | `	{ "session.cookie_samesite",  "",           VM_INI_ALL },` |
|      - |  126 | `	{ "session.cookie_secure",    "0",          VM_INI_ALL },` |
|      - |  127 | `	{ "session.gc_divisor",       "100",        VM_INI_ALL },` |
|      - |  128 | `	{ "session.gc_maxlifetime",   "1440",       VM_INI_ALL },` |
|      - |  129 | `	{ "session.gc_probability",   "1",          VM_INI_ALL },` |
|      - |  130 | `	{ "session.lazy_write",       "1",          VM_INI_ALL },` |
|      - |  131 | `	{ "session.name",             "PHPSESSID",  VM_INI_ALL },` |
|      - |  132 | `	{ "session.save_handler",     "files",      VM_INI_ALL },` |
|      - |  133 | `	{ "session.save_path",        "",           VM_INI_ALL },` |
|      - |  134 | ``	/* Which of php's three session serializers writes the store: `php` (the`` |
|      - |  135 | ``	 * `name\|<serialized>` runs a stock php install reads), `php_binary` or`` |
|      - |  136 | ``	 * `php_serialize`. */`` |
|      - |  137 | `	{ "session.serialize_handler","php",        VM_INI_ALL },` |
|      - |  138 | `	/* Whether the session sends and reads its id as a cookie at all. PHL has never` |
|      - |  139 | `	 * read an id from anywhere ELSE, which is what use_only_cookies means. */` |
|      - |  140 | `	{ "session.use_cookies",      "1",          VM_INI_ALL },` |
|      - |  141 | `	{ "session.use_only_cookies", "1",          VM_INI_ALL },` |
|      - |  142 | `	{ "session.use_strict_mode",  "0",          VM_INI_ALL },` |
|      - |  143 | `	{ "short_open_tag",           "",           VM_INI_PERDIR\|VM_INI_SYSTEM },` |
|      - |  144 | `	/* php's three syslog directives, with php's defaults and php's access masks.` |
|      - |  145 | ``	 * `syslog.filter` is the one this engine READS: it decides which bytes`` |
|      - |  146 | `	 * syslog() escapes and is PHP_INI_ALL, so a script can change it. The other` |
|      - |  147 | ``	 * two are what php's own error logger uses when `error_log = syslog`, a`` |
|      - |  148 | `	 * target this build does not have -- they are declared because ini_get() and` |
|      - |  149 | `	 * ini_get_all() answer them under php and a program can read either. */` |
|      - |  150 | `	{ "syslog.facility",          "LOG_USER",   VM_INI_SYSTEM },` |
|      - |  151 | `	{ "syslog.filter",            "no-ctrl",    VM_INI_ALL },` |
|      - |  152 | `	{ "syslog.ident",             "php",        VM_INI_SYSTEM },` |
|      - |  153 | `#ifdef PH7_ENABLE_SQLITE` |
|      - |  154 | `` 	/* ext/sqlite3's two directives, in this sorted list's own place. `defensive` `` |
|      - |  155 | `	 * is applied to every connection SQLite3 opens (it is what makes an UPDATE of` |
|      - |  156 | ``	 * sqlite_master refuse, even behind `PRAGMA writable_schema=ON`), and`` |
|      - |  157 | ``	 * `extension_dir` is the door loadExtension() is shut behind: empty means`` |
|      - |  158 | `	 * "SQLite Extensions are disabled", and php ships it empty. The access masks` |
|      - |  159 | `	 * are php's own, which do not agree with each other. */` |
|      - |  160 | `	{ "sqlite3.defensive",        "1",          VM_INI_USER },` |
|      - |  161 | `	{ "sqlite3.extension_dir",    0,            VM_INI_SYSTEM },` |
|      - |  162 | `#endif` |
|      - |  163 | `	{ "unserialize_callback_func","",           VM_INI_ALL },` |
|      - |  164 | `	{ "unserialize_max_depth",    "4096",       VM_INI_ALL },` |
|      - |  165 | `	{ "upload_max_filesize",      "2M",         VM_INI_PERDIR\|VM_INI_SYSTEM },` |
|      - |  166 | `	/* The User-Agent the http:// wrapper writes when the request names none.` |
|      - |  167 | ``	 * UNSET like `from` above, and for the same reason -- but the wrapper reads`` |
|      - |  168 | `	 * the two differently: an empty user_agent writes no header at all, while an` |
|      - |  169 | ``	 * empty `from` writes `From: `. */`` |
|      - |  170 | `	{ "user_agent",               0,            VM_INI_ALL },` |
|      - |  171 | `	{ "zend.assertions",          "-1",         VM_INI_ALL },` |
|      - |  172 | `#ifdef PH7_ENABLE_ZLIB` |
|      - |  173 | `	/* ext/zlib's three, php's own defaults and its access mask (all three are` |
|      - |  174 | `	 * PHP_INI_ALL). They are READ by ob_gzhandler() and zlib_get_coding_type()` |
|      - |  175 | `	 * and by nothing else: php's output-layer compression is a SAPI feature a` |
|      - |  176 | `	 * command line never turns on, and neither does this. */` |
|      - |  177 | `	{ "zlib.output_compression",  "",           VM_INI_ALL },` |
|      - |  178 | `	{ "zlib.output_compression_level","-1",     VM_INI_ALL },` |
|      - |  179 | `	{ "zlib.output_handler",      "",           VM_INI_ALL },` |
|      - |  180 | `#endif` |
|      - |  181 | `};` |
|      - |  182 |  |
|  61692 |  183 | `static int IniNameIs(const VmIniSlot *pSlot,const char *zName)` |
|      5 |  184 | `{` |
|  61697 |  185 | `	sxu32 n = (sxu32)SyStrlen(zName);` |
|  61697 |  186 | `	return pSlot->sName.nByte == n && SyMemcmp(pSlot->sName.zString,zName,n) == 0;` |
|      5 |  187 | `}` |
|      - |  188 | `/*` |
|      - |  189 | ` * zend_ini_parse_bool semantics, matching the C-side VmIniBool the -d/-c path` |
|      - |  190 | ` * uses: on/yes/true, else a non-zero integer parse.` |
|      - |  191 | ` */` |
|   1275 |  192 | `static int IniTruthy(const char *zVal,sxu32 nVal)` |
|      5 |  193 | `{` |
|   1905 |  194 | `	while( nVal > 0 && (zVal[0] == ' ' \|\| zVal[0] == '\t') ){ zVal++; nVal--; }` |
|   1905 |  195 | `	while( nVal > 0 && (zVal[nVal-1] == ' ' \|\| zVal[nVal-1] == '\t') ){ nVal--; }` |
|   1280 |  196 | `	if( nVal == 2 && SyStrnicmp(zVal,"on",2) == 0 ){ return 1; }` |
|   1280 |  197 | `	if( nVal == 3 && SyStrnicmp(zVal,"yes",3) == 0 ){ return 1; }` |
|   1280 |  198 | `	if( nVal == 4 && SyStrnicmp(zVal,"true",4) == 0 ){ return 1; }` |
|      - |  199 | `	{` |
|   1280 |  200 | `		sxi32 iVal = 0;` |
|   1280 |  201 | `		if( nVal > 0 && SyStrToInt32(zVal,nVal,(void *)&iVal,0) == SXRET_OK ){` |
|   1274 |  202 | `			return iVal != 0;` |
|      - |  203 | `		}` |
|      - |  204 | `	}` |
|      7 |  205 | `	return 0;` |
|    633 |  206 | `}` |
|      - |  207 | `/*` |
|      - |  208 | `` * The value rules that apply to a `-d name=value` on the COMMAND LINE, which are`` |
|      - |  209 | ` * not the same set IniValueAccepted enforces on ini_set(). php runs each` |
|      - |  210 | ` * directive's OnUpdate handler at startup too, but several of those refuse only` |
|      - |  211 | `` * at RUNTIME -- `-d session.serialize_handler=bogus` is taken by php because the`` |
|      - |  212 | ` * serializer table it would look the name up in is still empty -- so this is a` |
|      - |  213 | ` * per-directive list rather than a shared screen. A value refused here leaves the` |
|      - |  214 | ` * directive at its default, which is what php reports.` |
|      - |  215 | ` */` |
|     37 |  216 | `static int IniStartupValueAccepted(const SyString *pName,const char *zVal,sxu32 nVal)` |
|      4 |  217 | `{` |
|     37 |  218 | `	if( pName->nByte == sizeof("syslog.filter")-1` |
|     27 |  219 | `	 && SyMemcmp(pName->zString,"syslog.filter",sizeof("syslog.filter")-1) == 0 ){` |
|    ! 0 |  220 | `		return (nVal == 3 && SyMemcmp(zVal,"all",3) == 0)` |
|    ! 0 |  221 | `		    \|\| (nVal == 7 && SyMemcmp(zVal,"no-ctrl",7) == 0)` |
|    ! 0 |  222 | `		    \|\| (nVal == 5 && SyMemcmp(zVal,"ascii",5) == 0)` |
|    ! 0 |  223 | `		    \|\| (nVal == 3 && SyMemcmp(zVal,"raw",3) == 0);` |
|      - |  224 | `	}` |
|     41 |  225 | `	return 1;` |
|     22 |  226 | `}` |
|      - |  227 | `/*` |
|      - |  228 | ` * Build the table: the static defaults, then the CLI queue merged over them (an` |
|      - |  229 | ` * unknown CLI name is appended as a new INI_ALL directive, as the chunk did),` |
|      - |  230 | ` * then sorted by name so ini_get_all() can walk it in php's order without a sort.` |
|      - |  231 | ` */` |
|   5934 |  232 | `static sxi32 IniSeed(ph7_vm *pVm)` |
|      5 |  233 | `{` |
|      - |  234 | `	sxu32 i,j;` |
|      - |  235 | `	VmIniEntry *aCli;` |
|      - |  236 | `	VmIniSlot *aSlot;` |
|   5939 |  237 | `	if( pVm->bIniSeeded ){` |
|   5748 |  238 | `		return SXRET_OK;` |
|      - |  239 | `	}` |
|    196 |  240 | `	pVm->bIniSeeded = 1; /* set FIRST: the live-wired writes below re-enter nothing,` |
|      - |  241 | `	                      * but a future one must never recurse into the seed */` |
|  13375 |  242 | `	for( i = 0 ; i < SX_ARRAYSIZE(aIniDefault) ; i++ ){` |
|      - |  243 | `		VmIniSlot sSlot;` |
|  13184 |  244 | `		SyStringInitFromBuf(&sSlot.sName,aIniDefault[i].zName,SyStrlen(aIniDefault[i].zName));` |
|  13184 |  245 | `		sSlot.iAccess = aIniDefault[i].iAccess;` |
|  13184 |  246 | `		SyBlobInit(&sSlot.sGlobal,&pVm->sAllocator);` |
|  13184 |  247 | `		SyBlobInit(&sSlot.sLocal,&pVm->sAllocator);` |
|      - |  248 | `		/* A row with no value at all is php's UNSET directive, which is not the` |
|      - |  249 | `		 * empty string: it reports NULL everywhere the raw value is shown. */` |
|  13184 |  250 | `		sSlot.bGlobalNull = sSlot.bLocalNull = aIniDefault[i].zValue ? 0 : 1;` |
|  13184 |  251 | `		if( aIniDefault[i].zValue ){` |
|  17960 |  252 | `			SyBlobAppend(&sSlot.sGlobal,aIniDefault[i].zValue,` |
|  12033 |  253 | `				(sxu32)SyStrlen(aIniDefault[i].zValue));` |
|  17960 |  254 | `			SyBlobAppend(&sSlot.sLocal,aIniDefault[i].zValue,` |
|  12033 |  255 | `				(sxu32)SyStrlen(aIniDefault[i].zValue));` |
|   5922 |  256 | `		}` |
|  13184 |  257 | `		if( SySetPut(&pVm->aIniTab,(const void *)&sSlot) != SXRET_OK ){` |
|    ! 0 |  258 | `			return SXERR_MEM;` |
|      - |  259 | `		}` |
|   6491 |  260 | `	}` |
|    196 |  261 | `	aCli = (VmIniEntry *)SySetBasePtr(&pVm->aIniCli);` |
|    233 |  262 | `	for( i = 0 ; i < SySetUsed(&pVm->aIniCli) ; i++ ){` |
|     41 |  263 | `		int bFound = 0;` |
|     59 |  264 | `		if( !IniStartupValueAccepted(&aCli[i].sName,aCli[i].sValue.zString,` |
|     37 |  265 | `			aCli[i].sValue.nByte) ){` |
|    ! 0 |  266 | `			continue;   /* the directive keeps its default, as it does under php */` |
|      - |  267 | `		}` |
|     41 |  268 | `		aSlot = (VmIniSlot *)SySetBasePtr(&pVm->aIniTab);` |
|   1148 |  269 | `		for( j = 0 ; j < SySetUsed(&pVm->aIniTab) ; j++ ){` |
|   1142 |  270 | `			if( aSlot[j].sName.nByte == aCli[i].sName.nByte` |
|    606 |  271 | `			 && SyMemcmp(aSlot[j].sName.zString,aCli[i].sName.zString,aCli[i].sName.nByte) == 0 ){` |
|     39 |  272 | `				SyBlobReset(&aSlot[j].sGlobal);` |
|     39 |  273 | `				SyBlobReset(&aSlot[j].sLocal);` |
|     39 |  274 | `				SyBlobAppend(&aSlot[j].sGlobal,aCli[i].sValue.zString,aCli[i].sValue.nByte);` |
|     39 |  275 | `				SyBlobAppend(&aSlot[j].sLocal,aCli[i].sValue.zString,aCli[i].sValue.nByte);` |
|      - |  276 | ``				/* `-d name=` names the directive on the command line, so what it`` |
|      - |  277 | `				 * carries is a value -- the empty one, never the unset state. */` |
|     39 |  278 | `				aSlot[j].bGlobalNull = aSlot[j].bLocalNull = 0;` |
|     39 |  279 | `				bFound = 1;` |
|     39 |  280 | `				break;` |
|      - |  281 | `			}` |
|    543 |  282 | `		}` |
|     41 |  283 | `		if( !bFound ){` |
|      - |  284 | `			VmIniSlot sSlot;` |
|      - |  285 | `			/* aIniCli holds VM-lifetime copies already, so the name can be aliased. */` |
|      3 |  286 | `			sSlot.sName = aCli[i].sName;` |
|      3 |  287 | `			sSlot.iAccess = VM_INI_ALL;` |
|      3 |  288 | `			sSlot.bGlobalNull = sSlot.bLocalNull = 0;` |
|      3 |  289 | `			SyBlobInit(&sSlot.sGlobal,&pVm->sAllocator);` |
|      3 |  290 | `			SyBlobInit(&sSlot.sLocal,&pVm->sAllocator);` |
|      3 |  291 | `			SyBlobAppend(&sSlot.sGlobal,aCli[i].sValue.zString,aCli[i].sValue.nByte);` |
|      3 |  292 | `			SyBlobAppend(&sSlot.sLocal,aCli[i].sValue.zString,aCli[i].sValue.nByte);` |
|      3 |  293 | `			if( SySetPut(&pVm->aIniTab,(const void *)&sSlot) != SXRET_OK ){` |
|    ! 0 |  294 | `				return SXERR_MEM;` |
|      - |  295 | `			}` |
|      1 |  296 | `		}` |
|     22 |  297 | `	}` |
|      - |  298 | `	/* Insertion sort by name (the table is ~30 entries and already nearly sorted:` |
|      - |  299 | `	 * only CLI-introduced directives are out of place). */` |
|    196 |  300 | `	aSlot = (VmIniSlot *)SySetBasePtr(&pVm->aIniTab);` |
|  13186 |  301 | `	for( i = 1 ; i < SySetUsed(&pVm->aIniTab) ; i++ ){` |
|  12995 |  302 | `		VmIniSlot sTmp = aSlot[i];` |
|  12995 |  303 | `		j = i;` |
|  14408 |  304 | `		while( j > 0 ){` |
|  14408 |  305 | `			const VmIniSlot *pPrev = &aSlot[j-1];` |
|  14408 |  306 | `			sxu32 nMin = pPrev->sName.nByte < sTmp.sName.nByte ? pPrev->sName.nByte : sTmp.sName.nByte;` |
|  14408 |  307 | `			sxi32 iCmp = SyMemcmp(pPrev->sName.zString,sTmp.sName.zString,nMin);` |
|  14408 |  308 | `			if( iCmp == 0 ){` |
|    196 |  309 | `				iCmp = (sxi32)pPrev->sName.nByte - (sxi32)sTmp.sName.nByte;` |
|     94 |  310 | `			}` |
|  14408 |  311 | `			if( iCmp <= 0 ){` |
|  12995 |  312 | `				break;` |
|      - |  313 | `			}` |
|   1418 |  314 | `			aSlot[j] = aSlot[j-1];` |
|   1418 |  315 | `			j--;` |
|      5 |  316 | `		}` |
|  12995 |  317 | `		aSlot[j] = sTmp;` |
|   6398 |  318 | `	}` |
|      - |  319 | `	/* Boot-apply the CLI values for the live-wired session knobs. The engine knobs` |
|      - |  320 | `	 * (error_reporting / date.timezone) were already applied C-side by` |
|      - |  321 | `	 * PH7_VM_CONFIG_INI_ENTRY. */` |
|    196 |  322 | `	aSlot = (VmIniSlot *)SySetBasePtr(&pVm->aIniTab);` |
|  13377 |  323 | `	for( i = 0 ; i < SySetUsed(&pVm->aIniTab) ; i++ ){` |
|  13186 |  324 | `		SyBlob *pDst = 0;` |
|  13186 |  325 | `		if( IniNameIs(&aSlot[i],"session.name") ){` |
|    191 |  326 | `			if( SyBlobLength(&aSlot[i].sGlobal) != sizeof("PHPSESSID")-1` |
|    194 |  327 | `			 \|\| SyMemcmp(SyBlobData(&aSlot[i].sGlobal),"PHPSESSID",sizeof("PHPSESSID")-1) != 0 ){` |
|      6 |  328 | `				pDst = &pVm->sSessName;` |
|      8 |  329 | `			}` |
|  13089 |  330 | `		}else if( IniNameIs(&aSlot[i],"session.save_path") ){` |
|    196 |  331 | `			if( SyBlobLength(&aSlot[i].sGlobal) > 0 ){` |
|    ! 0 |  332 | `				pDst = &pVm->sSessPath;` |
|    ! 0 |  333 | `			}` |
|     94 |  334 | `		}` |
|  13186 |  335 | `		if( pDst ){` |
|      6 |  336 | `			sxu32 nLen = SyBlobLength(&aSlot[i].sGlobal);` |
|      6 |  337 | `			const char *zVal = (const char *)SyBlobData(&aSlot[i].sGlobal);` |
|      6 |  338 | `			while( nLen > 0 && zVal[nLen-1] == '/' ){ nLen--; } /* rtrim('/') */` |
|      6 |  339 | `			SyBlobReset(pDst);` |
|      6 |  340 | `			SyBlobAppend(pDst,zVal,nLen);` |
|      3 |  341 | `		}` |
|   6492 |  342 | `	}` |
|    196 |  343 | `	return SXRET_OK;` |
|   2894 |  344 | `}` |
|   5843 |  345 | `static VmIniSlot * IniFind(ph7_vm *pVm,const char *zName,sxu32 nName)` |
|      5 |  346 | `{` |
|   5848 |  347 | `	VmIniSlot *aSlot = (VmIniSlot *)SySetBasePtr(&pVm->aIniTab);` |
|      - |  348 | `	sxu32 n;` |
| 199721 |  349 | `	for( n = 0 ; n < SySetUsed(&pVm->aIniTab) ; n++ ){` |
| 199642 |  350 | `		if( aSlot[n].sName.nByte == nName` |
| 104187 |  351 | `		 && SyMemcmp(aSlot[n].sName.zString,zName,nName) == 0 ){` |
|   5774 |  352 | `			return &aSlot[n];` |
|      - |  353 | `		}` |
|  93743 |  354 | `	}` |
|     79 |  355 | `	return 0;` |
|   2849 |  356 | `}` |
|      - |  357 | `/*` |
|      - |  358 | ` * The EFFECTIVE current value. For a live-wired directive the runtime knob is` |
|      - |  359 | ` * the truth, not the stored local value, so that ini_get() and the knob's own` |
|      - |  360 | ` * accessor can never disagree.` |
|      - |  361 | ` */` |
|   7870 |  362 | `static void IniLiveGet(ph7_vm *pVm,VmIniSlot *pSlot,SyBlob *pOut)` |
|      5 |  363 | `{` |
|   7875 |  364 | `	SyBlobReset(pOut);` |
|   7875 |  365 | `	if( IniNameIs(pSlot,"error_reporting") ){` |
|      - |  366 | `		char zBuf[32];` |
|     45 |  367 | `		int nBuf = SyBufferFormat(zBuf,sizeof(zBuf),"%d",` |
|     44 |  368 | `			pVm->bErrReport ? (int)pVm->iErrMask : 0);` |
|     47 |  369 | `		SyBlobAppend(pOut,zBuf,(sxu32)nBuf);` |
|     47 |  370 | `		return;` |
|      - |  371 | `	}` |
|   7831 |  372 | `	if( IniNameIs(pSlot,"session.name") ){` |
|     91 |  373 | `		SyBlobAppend(pOut,SyBlobData(&pVm->sSessName),SyBlobLength(&pVm->sSessName));` |
|     91 |  374 | `		return;` |
|      - |  375 | `	}` |
|   7744 |  376 | `	if( IniNameIs(pSlot,"session.save_path") && SyBlobLength(&pVm->sSessPath) > 0 ){` |
|     11 |  377 | `		SyBlobAppend(pOut,SyBlobData(&pVm->sSessPath),SyBlobLength(&pVm->sSessPath));` |
|     11 |  378 | `		return;` |
|      - |  379 | `	}` |
|   7734 |  380 | `	if( IniNameIs(pSlot,"include_path") ){` |
|      - |  381 | `		/* The VM's path SET is the store; this directive is a view of it, so` |
|      - |  382 | `		 * ini_get() and get_include_path() can never name different paths. */` |
|     61 |  383 | `		PH7_VmGetIncludePath(pVm,pOut);` |
|     61 |  384 | `		return;` |
|      - |  385 | `	}` |
|   7676 |  386 | `	SyBlobAppend(pOut,SyBlobData(&pSlot->sLocal),SyBlobLength(&pSlot->sLocal));` |
|   3846 |  387 | `}` |
|      - |  388 | `/*` |
|      - |  389 | ` * Push a new value at the runtime knob behind a live-wired directive. The stored` |
|      - |  390 | ` * local value is updated by the caller either way.` |
|      - |  391 | ` */` |
|      - |  392 | `/*` |
|      - |  393 | ` * php's byte shorthand: a plain integer, optionally suffixed K, M or G (case` |
|      - |  394 | `` * insensitive, no "B"). `-1` -- and any negative -- means UNLIMITED, which is the`` |
|      - |  395 | ` * CLI default. Anything unparseable reads as 0, which php also treats as` |
|      - |  396 | ` * "allocate nothing", so it is left to say exactly that rather than being` |
|      - |  397 | ` * silently promoted to unlimited.` |
|      - |  398 | ` */` |
|     12 |  399 | `static sxu32 IniParseBytes(const char *zVal,sxu32 nVal,int *pbUnlimited)` |
|      1 |  400 | `{` |
|     13 |  401 | `	sxi64 iVal = 0;` |
|     13 |  402 | `	sxu32 n = 0;` |
|     13 |  403 | `	int bNeg = 0;` |
|     13 |  404 | `	*pbUnlimited = 0;` |
|     19 |  405 | `	while( n < nVal && (zVal[n] == ' ' \|\| zVal[n] == '\t') ){ n++; }` |
|     13 |  406 | `	if( n < nVal && (zVal[n] == '-' \|\| zVal[n] == '+') ){` |
|    ! 0 |  407 | `		bNeg = (zVal[n] == '-');` |
|    ! 0 |  408 | `		n++;` |
|    ! 0 |  409 | `	}` |
|     41 |  410 | `	while( n < nVal && zVal[n] >= '0' && zVal[n] <= '9' ){` |
|     29 |  411 | `		iVal = iVal * 10 + (zVal[n] - '0');` |
|     29 |  412 | `		if( iVal > (sxi64)0x7FFFFFFF ){ iVal = (sxi64)0x7FFFFFFF; } /* clamp: the field is 32-bit */` |
|     29 |  413 | `		n++;` |
|      1 |  414 | `	}` |
|     13 |  415 | `	if( bNeg ){` |
|    ! 0 |  416 | `		*pbUnlimited = 1;   /* php: any negative memory_limit is "no limit" */` |
|    ! 0 |  417 | `		return 0;` |
|      - |  418 | `	}` |
|     13 |  419 | `	if( n < nVal ){` |
|     13 |  420 | `		sxi64 nMul = 0;` |
|     13 |  421 | `		switch( zVal[n] ){` |
|    ! 0 |  422 | `			case 'k': case 'K': nMul = 1024; break;` |
|     13 |  423 | `			case 'm': case 'M': nMul = 1024 * 1024; break;` |
|    ! 0 |  424 | `			case 'g': case 'G': nMul = 1024 * 1024 * 1024; break;` |
|    ! 0 |  425 | `			default: nMul = 0; break;` |
|      - |  426 | `		}` |
|     13 |  427 | `		if( nMul > 0 ){` |
|     13 |  428 | `			iVal = (iVal > (sxi64)0x7FFFFFFF / nMul) ? (sxi64)0x7FFFFFFF : iVal * nMul;` |
|      6 |  429 | `		}` |
|      6 |  430 | `	}` |
|     13 |  431 | `	return (sxu32)iVal;` |
|      7 |  432 | `}` |
|      - |  433 | `/*` |
|      - |  434 | ` * Arm the allocator's total live-byte ceiling from a memory_limit value, and answer` |
|      - |  435 | ` * whether it took. THE one place the directive is interpreted: ini_set() reaches it` |
|      - |  436 | `` * through the validator and `-d name=value` reaches it directly, and a rule that`` |
|      - |  437 | ` * lived in only one of those would hold for one door and not the other.` |
|      - |  438 | ` *` |
|      - |  439 | ` * The one directive that reaches into the ALLOCATOR. php enforces a ceiling and` |
|      - |  440 | ` * kills the script with a fatal when a request would cross it; PHL stored the` |
|      - |  441 | ` * string and enforced nothing, so a runaway allocation -- a reference cycle nothing` |
|      - |  442 | ` * reclaims is the usual way in (PLAN.md §5) -- had no ceiling below the kernel's, and` |
|      - |  443 | ` * the OOM killer took the whole process instead of the script. On a shared box that` |
|      - |  444 | ` * is not the script's problem any more: it is everything else's.` |
|      - |  445 | ` *` |
|      - |  446 | ` * Unlimited is the CLI default here as it is in php, so a plain run is unchanged.` |
|      - |  447 | ` */` |
|     12 |  448 | `PH7_PRIVATE int PH7_VmApplyMemoryLimit(ph7_vm *pVm,const char *zVal,sxu32 nVal)` |
|      1 |  449 | `{` |
|     13 |  450 | `	int bUnlimited = 0;` |
|     13 |  451 | `	sxu32 nBytes = IniParseBytes(zVal,nVal,&bUnlimited);` |
|     13 |  452 | `	if( !bUnlimited && nBytes > 0 && nBytes < pVm->sAllocator.nMemUsed ){` |
|      - |  453 | `		/* php REFUSES to lower the ceiling below what is already in use --` |
|      - |  454 | `		 * zend_set_memory_limit answers FAILURE -- and it does so from the` |
|      - |  455 | `		 * directive's own OnUpdate handler, which is why the rule holds at STARTUP` |
|      - |  456 | ``		 * (`-d memory_limit=8K` warns and runs on) exactly as it holds for`` |
|      - |  457 | `		 * ini_set(). Arming a ceiling the interpreter is already past is not a` |
|      - |  458 | `		 * limit, it is a delayed crash: the very next allocation is fatal.` |
|      - |  459 | `		 *` |
|      - |  460 | `		 * Without the rule, a library that PROBES the limit by setting a small one` |
|      - |  461 | `		 * dies instead of learning that it cannot -- monolog's StreamHandler sizes` |
|      - |  462 | `		 * its write chunk exactly that way, and its test walks 1M, 10M, 1024M, 3G` |
|      - |  463 | `		 * in turn, reading the false back and skipping.` |
|      - |  464 | `		 *` |
|      - |  465 | ``		 * php prints this one WITHOUT the `ini_set(): ` prefix its other ini`` |
|      - |  466 | `		 * warnings carry, because it comes from the handler and not from the call. */` |
|      - |  467 | `		char zMsg[160];` |
|    ! 0 |  468 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - |  469 | `			"Failed to set memory limit to %u bytes (Current memory usage is %u bytes)",` |
|    ! 0 |  470 | `			nBytes,pVm->sAllocator.nMemUsed);` |
|    ! 0 |  471 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);` |
|    ! 0 |  472 | `		return 0;` |
|      - |  473 | `	}` |
|     13 |  474 | `	pVm->sAllocator.nMemLimit = bUnlimited ? 0 : nBytes;` |
|     13 |  475 | `	pVm->sAllocator.nMemLimitHit = 0;` |
|     13 |  476 | `	pVm->sAllocator.nMemTried = 0;` |
|     13 |  477 | `	return 1;` |
|      7 |  478 | `}` |
|    301 |  479 | `static void IniLiveSet(ph7_vm *pVm,VmIniSlot *pSlot,const char *zVal,sxu32 nVal)` |
|      5 |  480 | `{` |
|    306 |  481 | `	if( IniNameIs(pSlot,"error_reporting") ){` |
|    ! 0 |  482 | `		sxi32 iVal = 0;` |
|    ! 0 |  483 | `		if( nVal > 0 ){` |
|    ! 0 |  484 | `			SyStrToInt32(zVal,nVal,(void *)&iVal,0);` |
|    ! 0 |  485 | `		}` |
|    ! 0 |  486 | `		pVm->iErrMask = iVal;` |
|    ! 0 |  487 | `		pVm->bErrReport = iVal != 0;` |
|    ! 0 |  488 | `		return;` |
|      - |  489 | `	}` |
|    306 |  490 | `	if( IniNameIs(pSlot,"memory_limit") ){` |
|      5 |  491 | `		(void)PH7_VmApplyMemoryLimit(pVm,zVal,nVal);` |
|      5 |  492 | `		return;` |
|      - |  493 | `	}` |
|    302 |  494 | `	if( IniNameIs(pSlot,"display_errors") ){` |
|      5 |  495 | `		pVm->bDisplayErrors = IniTruthy(zVal,nVal);` |
|      5 |  496 | `		return;` |
|      - |  497 | `	}` |
|    298 |  498 | `	if( IniNameIs(pSlot,"log_errors") ){` |
|    ! 0 |  499 | `		pVm->bLogErrors = IniTruthy(zVal,nVal);` |
|    ! 0 |  500 | `		return;` |
|      - |  501 | `	}` |
|    298 |  502 | `	if( IniNameIs(pSlot,"session.name") \|\| IniNameIs(pSlot,"session.save_path") ){` |
|     59 |  503 | `		int bPath = IniNameIs(pSlot,"session.save_path");` |
|     59 |  504 | `		SyBlob *pDst = bPath ? &pVm->sSessPath : &pVm->sSessName;` |
|     59 |  505 | `		sxu32 nLen = nVal;` |
|     59 |  506 | `		if( bPath ){` |
|     32 |  507 | `			while( nLen > 0 && zVal[nLen-1] == '/' ){ nLen--; }` |
|     15 |  508 | `		}` |
|     59 |  509 | `		SyBlobReset(pDst);` |
|     59 |  510 | `		SyBlobAppend(pDst,zVal,nLen);` |
|     59 |  511 | `		return;` |
|      - |  512 | `	}` |
|    242 |  513 | `	if( IniNameIs(pSlot,"include_path") ){` |
|      - |  514 | `		/* The one write that moves the include walk. Refused empty by the caller` |
|      - |  515 | `		 * (php's OnUpdateStringUnempty), so nVal is never 0 on the ini_set() path;` |
|      - |  516 | `		 * the boot/-d and ini_restore() paths carry a real value too. */` |
|      5 |  517 | `		PH7_VmSetIncludePath(pVm,zVal,nVal);` |
|      5 |  518 | `		return;` |
|      - |  519 | `	}` |
|    238 |  520 | `	if( IniNameIs(pSlot,"date.timezone") ){` |
|      - |  521 | `		/* Only UTC/GMT exist here (no tz database), matching the engine's own` |
|      - |  522 | `		 * date_default_timezone_set(). */` |
|    ! 0 |  523 | `		if( nVal == 3 && (SyStrnicmp(zVal,"UTC",3) == 0 \|\| SyStrnicmp(zVal,"GMT",3) == 0) ){` |
|    ! 0 |  524 | `			SyMemcpy(zVal,pVm->zDefTz,3);` |
|    ! 0 |  525 | `			pVm->zDefTz[3] = 0;` |
|    ! 0 |  526 | `			pVm->nDefTz = 3;` |
|    ! 0 |  527 | `		}` |
|    ! 0 |  528 | `		return;` |
|      - |  529 | `	}` |
|    150 |  530 | `}` |
|      - |  531 | `/*` |
|      - |  532 | ` * A session directive is settable only while there is no session to disturb: not` |
|      - |  533 | ` * once one is ACTIVE (the store is open and the cookie decided), and not once` |
|      - |  534 | ` * headers have gone out. Answers TRUE (having raised the warning) when the write` |
|      - |  535 | ` * must be refused.` |
|      - |  536 | ` */` |
|    258 |  537 | `static int IniSessionLocked(ph7_context *pCtx,VmIniSlot *pSlot,const char *zFunc)` |
|      5 |  538 | `{` |
|    263 |  539 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  540 | `	char zMsg[160];` |
|      - |  541 | `	const char *zWhy;` |
|    258 |  542 | `	if( pSlot->sName.nByte < sizeof("session.")-1` |
|    261 |  543 | `	 \|\| SyMemcmp(pSlot->sName.zString,"session.",sizeof("session.")-1) != 0 ){` |
|     98 |  544 | `		return 0;` |
|      - |  545 | `	}` |
|    168 |  546 | `	int bActive = (pVm->iSessStatus == 2 /* PHP_SESSION_ACTIVE */);` |
|    168 |  547 | `	if( bActive ){` |
|      5 |  548 | `		zWhy = "when a session is active";` |
|    166 |  549 | `	}else if( pVm->bHeadersSent ){` |
|      3 |  550 | `		zWhy = "after headers have already been sent";` |
|      2 |  551 | `	}else{` |
|    162 |  552 | `		return 0;` |
|      - |  553 | `	}` |
|     11 |  554 | `	SyBufferFormat(zMsg,sizeof(zMsg),` |
|      3 |  555 | `		"%s(): Session ini settings cannot be changed %s",zFunc,zWhy);` |
|      - |  556 | `	{` |
|      - |  557 | `		/* php names the session_start() or the output behind the refusal. */` |
|      - |  558 | `		SyBlob sMsg;` |
|      8 |  559 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      8 |  560 | `		SyBlobAppend(&sMsg,zMsg,(sxu32)SyStrlen(zMsg));` |
|      8 |  561 | `		PH7_VmAppendWhere(pVm,&sMsg,bActive);` |
|      8 |  562 | `		SyBlobNullAppend(&sMsg);` |
|      8 |  563 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|      8 |  564 | `		SyBlobRelease(&sMsg);` |
|      - |  565 | `	}` |
|      8 |  566 | `	return 1;` |
|    127 |  567 | `}` |
|      - |  568 | `/*` |
|      - |  569 | ` * The per-directive value rules php enforces on every write, and the diagnostic` |
|      - |  570 | ` * each one raises. zWho is the whole prefix php puts on it -- "ini_set()" or, when` |
|      - |  571 | ` * session_start() is applying its $options array, "session_start()" -- because php` |
|      - |  572 | ` * blames the call that made the write, not the API underneath it.` |
|      - |  573 | ` */` |
|    318 |  574 | `static int IniValueAccepted(ph7_vm *pVm,VmIniSlot *pSlot,const char *zVal,sxu32 nVal,` |
|      - |  575 | `	const char *zWho)` |
|      5 |  576 | `{` |
|      - |  577 | `	char zMsg[256];` |
|    323 |  578 | `	if( IniNameIs(pSlot,"include_path") && nVal < 1 ){` |
|      - |  579 | `		/* php registers include_path with OnUpdateStringUnempty: the EMPTY value` |
|      - |  580 | `		 * is refused in silence and the directive keeps what it had. */` |
|      3 |  581 | `		return 0;` |
|      - |  582 | `	}` |
|    321 |  583 | `	if( IniNameIs(pSlot,"memory_limit") ){` |
|      - |  584 | `		/* One rule, one warning, one place: the applier owns both, so ini_set() and` |
|      - |  585 | ``		 * the `-d` startup path cannot drift apart. Arming from the validator is`` |
|      - |  586 | `		 * idempotent -- an ACCEPTED value is then written and re-applied through` |
|      - |  587 | `		 * IniLiveSet with the same bytes, and a refused one is never written. */` |
|      5 |  588 | `		return PH7_VmApplyMemoryLimit(pVm,zVal,nVal);` |
|      - |  589 | `	}` |
|    312 |  590 | `	if( IniNameIs(pSlot,"syslog.filter")` |
|    165 |  591 | `	 && !(nVal == 3 && SyMemcmp(zVal,"all",3) == 0)` |
|     12 |  592 | `	 && !(nVal == 7 && SyMemcmp(zVal,"no-ctrl",7) == 0)` |
|      8 |  593 | `	 && !(nVal == 5 && SyMemcmp(zVal,"ascii",5) == 0)` |
|     11 |  594 | `	 && !(nVal == 3 && SyMemcmp(zVal,"raw",3) == 0) ){` |
|      - |  595 | `		/* php names the four modes in its OnUpdate handler and refuses anything` |
|      - |  596 | `		 * else in silence, keeping what the directive had -- so` |
|      - |  597 | ``		 * `ini_set('syslog.filter','bogus')` is false and reads back unchanged.`` |
|      - |  598 | ``		 * The match is CASE-SENSITIVE there: `ASCII` is refused where `ascii` is`` |
|      - |  599 | `		 * taken, which is not what most of php's word-valued directives do. */` |
|      3 |  600 | `		return 0;` |
|      - |  601 | `	}` |
|    314 |  602 | `	if( IniNameIs(pSlot,"bcmath.scale") ){` |
|      - |  603 | `		/* php registers it with a 0..INT_MAX bound and refuses anything outside in` |
|      - |  604 | `` 		 * silence, keeping what the directive had -- `ini_set('bcmath.scale','-1')` `` |
|      - |  605 | `		 * is false there. A NON-numeric value is a different matter and is` |
|      - |  606 | `		 * ACCEPTED (stored verbatim, read back as 0), which falls out of the parse` |
|      - |  607 | `		 * below without a rule of its own. */` |
|     25 |  608 | `		sxi64 iVal = 0;` |
|     25 |  609 | `		SyStrToInt64(zVal,nVal,(void *)&iVal,0);` |
|     25 |  610 | `		if( iVal < 0 \|\| iVal > 2147483647 ){` |
|      5 |  611 | `			return 0;` |
|      - |  612 | `		}` |
|     10 |  613 | `	}` |
|    305 |  614 | `	if( IniNameIs(pSlot,"session.serialize_handler")` |
|    155 |  615 | `	 && !(nVal == 3 && SyMemcmp(zVal,"php",3) == 0)` |
|     10 |  616 | `	 && !(nVal == 10 && SyMemcmp(zVal,"php_binary",10) == 0)` |
|     10 |  617 | `	 && !(nVal == 13 && SyMemcmp(zVal,"php_serialize",13) == 0) ){` |
|      - |  618 | `		/* php looks the name up in its registered serializer list and refuses what` |
|      - |  619 | `		 * it cannot find, keeping the directive where it was. */` |
|      8 |  620 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|      2 |  621 | `			"%s: Serialization handler \"%.*s\" cannot be found",zWho,(int)nVal,zVal);` |
|      6 |  622 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);` |
|      6 |  623 | `		return 0;` |
|      - |  624 | `	}` |
|    302 |  625 | `	if( IniNameIs(pSlot,"session.name") ){` |
|      - |  626 | `		/* The name goes out as a COOKIE name and comes back as one, so php holds it` |
|      - |  627 | `		 * to the cookie alphabet -- and refuses a numeric one, which a browser would` |
|      - |  628 | `		 * hand back as an integer array key. */` |
|      - |  629 | `		static const char zBad[] = "=,;.[ \t\r\n\013\014";` |
|      - |  630 | `		sxu32 i;` |
|     36 |  631 | `		int bBad = nVal < 1;` |
|    238 |  632 | `		for( i = 0 ; !bBad && i < nVal ; i++ ){` |
|    204 |  633 | `			if( zVal[i] == 0 \|\| SyByteFind(zBad,sizeof(zBad)-1,zVal[i],0) == SXRET_OK ){` |
|      3 |  634 | `				bBad = 1;` |
|      1 |  635 | `			}` |
|    103 |  636 | `		}` |
|     36 |  637 | `		if( !bBad ){` |
|     32 |  638 | `			sxi64 iDummy = 0;` |
|     32 |  639 | `			bBad = SyStrToInt64(zVal,nVal,(void *)&iDummy,0) == SXRET_OK;` |
|     15 |  640 | `		}` |
|     36 |  641 | `		if( bBad ){` |
|     17 |  642 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - |  643 | `				"%s: session.name \"%.*s\" must not be numeric, empty, contain null bytes"` |
|      - |  644 | `				" or any of the following characters \"=,;.[ \\t\\r\\n\\013\\014\"",` |
|      5 |  645 | `				zWho,(int)nVal,zVal);` |
|     12 |  646 | `			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);` |
|     12 |  647 | `			return 0;` |
|      - |  648 | `		}` |
|     12 |  649 | `	}` |
|    292 |  650 | `	return 1;` |
|    155 |  651 | `}` |
|      - |  652 | `/*` |
|      - |  653 | ` * Store one written value.  A write always leaves a local value; what it does to` |
|      - |  654 | ` * the GLOBAL one depends on whether there was ever a global to keep. php saves` |
|      - |  655 | ` * the original at the first modification and reports THAT as global_value -- but` |
|      - |  656 | ` * an UNSET directive has no original to save, so its global_value starts reading` |
|      - |  657 | ` * the written value instead, and only ini_restore() puts the NULL back.` |
|      - |  658 | ` */` |
|    291 |  659 | `static void IniWriteLocal(VmIniSlot *pSlot,const char *zVal,sxu32 nVal)` |
|      5 |  660 | `{` |
|    296 |  661 | `	SyBlobReset(&pSlot->sLocal);` |
|    296 |  662 | `	SyBlobAppend(&pSlot->sLocal,zVal,nVal);` |
|    296 |  663 | `	pSlot->bLocalNull = 0;` |
|    296 |  664 | `}` |
|      - |  665 | `/*` |
|      - |  666 | ` * What global_value reports: the saved original, unless there was none -- in` |
|      - |  667 | ` * which case it is whatever the directive currently holds, NULL included.` |
|      - |  668 | ` */` |
|   2207 |  669 | `static int IniGlobalValue(VmIniSlot *pSlot,const char **pz,sxu32 *pn)` |
|      4 |  670 | `{` |
|   2211 |  671 | `	if( !pSlot->bGlobalNull ){` |
|   2021 |  672 | `		*pz = (const char *)SyBlobData(&pSlot->sGlobal);` |
|   2021 |  673 | `		*pn = SyBlobLength(&pSlot->sGlobal);` |
|   2021 |  674 | `		return 1;` |
|      - |  675 | `	}` |
|    192 |  676 | `	if( pSlot->bLocalNull ){` |
|    188 |  677 | `		return 0;   /* still unset: php reports NULL */` |
|      - |  678 | `	}` |
|      5 |  679 | `	*pz = (const char *)SyBlobData(&pSlot->sLocal);` |
|      5 |  680 | `	*pn = SyBlobLength(&pSlot->sLocal);` |
|      5 |  681 | `	return 1;` |
|   1073 |  682 | `}` |
|      - |  683 | `/*` |
|      - |  684 | ` * Write a directive from C, the way ini_set() writes it. Answers 0 when the write` |
|      - |  685 | ` * was refused (unknown name, not user-settable, or a value the directive's own` |
|      - |  686 | ` * rule rejects) -- which is exactly what session_start()'s $options reports as` |
|      - |  687 | `` * `Setting option "%s" failed`.`` |
|      - |  688 | ` */` |
|     74 |  689 | `PH7_PRIVATE int PH7_VmIniSet(ph7_vm *pVm,const char *zName,sxu32 nName,` |
|      - |  690 | `	const char *zVal,sxu32 nVal,const char *zWho)` |
|      5 |  691 | `{` |
|      - |  692 | `	VmIniSlot *pSlot;` |
|     79 |  693 | `	if( IniSeed(pVm) != SXRET_OK ){` |
|    ! 0 |  694 | `		return 0;` |
|      - |  695 | `	}` |
|     79 |  696 | `	pSlot = IniFind(pVm,zName,nName);` |
|     79 |  697 | `	if( pSlot == 0 \|\| (pSlot->iAccess & VM_INI_USER) == 0 ){` |
|      3 |  698 | `		return 0;` |
|      - |  699 | `	}` |
|     77 |  700 | `	if( !IniValueAccepted(pVm,pSlot,zVal,nVal,zWho) ){` |
|      7 |  701 | `		return 0;` |
|      - |  702 | `	}` |
|     71 |  703 | `	IniWriteLocal(pSlot,zVal,nVal);` |
|     71 |  704 | `	IniLiveSet(pVm,pSlot,zVal,nVal);` |
|     71 |  705 | `	return 1;` |
|     42 |  706 | `}` |
|      - |  707 | `/* string\|false ini_get(string $option) */` |
|    166 |  708 | `static int vm_builtin_ini_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  709 | `{` |
|    171 |  710 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  711 | `	VmIniSlot *pSlot;` |
|      - |  712 | `	const char *zName;` |
|    171 |  713 | `	int nName = 0;` |
|      - |  714 | `	SyBlob sOut;` |
|    171 |  715 | `	if( nArg < 1 \|\| IniSeed(pVm) != SXRET_OK ){` |
|    ! 0 |  716 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  717 | `		return PH7_OK;` |
|      - |  718 | `	}` |
|    171 |  719 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|    171 |  720 | `	pSlot = IniFind(pVm,zName,(sxu32)nName);` |
|    171 |  721 | `	if( pSlot == 0 ){` |
|     13 |  722 | `		ph7_result_bool(pCtx,0);` |
|     13 |  723 | `		return PH7_OK;` |
|      - |  724 | `	}` |
|    161 |  725 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|    161 |  726 | `	IniLiveGet(pVm,pSlot,&sOut);` |
|    161 |  727 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    161 |  728 | `	SyBlobRelease(&sOut);` |
|    161 |  729 | `	return PH7_OK;` |
|     84 |  730 | `}` |
|      - |  731 | `/* string\|false ini_set(string $option, string\|int\|float\|bool\|null $value) */` |
|    258 |  732 | `static int vm_builtin_ini_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  733 | `{` |
|    263 |  734 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  735 | `	VmIniSlot *pSlot;` |
|      - |  736 | `	const char *zName;` |
|      - |  737 | `	const char *zVal;` |
|    263 |  738 | `	int nName = 0, nVal = 0;` |
|      - |  739 | `	SyBlob sOld;` |
|      - |  740 | `	char zWho[64];` |
|    263 |  741 | `	if( nArg < 2 \|\| IniSeed(pVm) != SXRET_OK ){` |
|    ! 0 |  742 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  743 | `		return PH7_OK;` |
|      - |  744 | `	}` |
|    263 |  745 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|    263 |  746 | `	pSlot = IniFind(pVm,zName,(sxu32)nName);` |
|    263 |  747 | `	if( pSlot == 0 \|\| (pSlot->iAccess & VM_INI_USER) == 0 ){` |
|     16 |  748 | `		ph7_result_bool(pCtx,0);` |
|     16 |  749 | `		return PH7_OK;` |
|      - |  750 | `	}` |
|    251 |  751 | `	if( IniSessionLocked(pCtx,pSlot,ph7_function_name(pCtx)) ){` |
|      6 |  752 | `		ph7_result_bool(pCtx,0);` |
|      6 |  753 | `		return PH7_OK;` |
|      - |  754 | `	}` |
|      - |  755 | `	/* php: the -1 (compiled-out) state of zend.assertions is a php.ini-only switch,` |
|      - |  756 | `	 * and so is moving INTO it. Unprefixed warning, exactly as php prints it. */` |
|    247 |  757 | `	if( IniNameIs(pSlot,"zend.assertions") ){` |
|    ! 0 |  758 | `		int bGlobalOff = SyBlobLength(&pSlot->sGlobal) == 2` |
|    ! 0 |  759 | `			&& SyMemcmp(SyBlobData(&pSlot->sGlobal),"-1",2) == 0;` |
|    ! 0 |  760 | `		const char *zNew = ph7_value_to_string(apArg[1],&nVal);` |
|    ! 0 |  761 | `		int bNewOff = nVal == 2 && SyMemcmp(zNew,"-1",2) == 0;` |
|    ! 0 |  762 | `		if( bGlobalOff \|\| bNewOff ){` |
|    ! 0 |  763 | `			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|      - |  764 | `				"zend.assertions may be completely enabled or disabled only in php.ini");` |
|    ! 0 |  765 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 |  766 | `			return PH7_OK;` |
|      - |  767 | `		}` |
|    ! 0 |  768 | `	}` |
|      - |  769 | `	/* The OLD value php answers is the effective one, read before the write. */` |
|    247 |  770 | `	SyBlobInit(&sOld,&pVm->sAllocator);` |
|    247 |  771 | `	IniLiveGet(pVm,pSlot,&sOld);` |
|      - |  772 | `	/* php stringifies the incoming value, with a bool becoming "1"/"" . */` |
|    247 |  773 | `	if( ph7_value_is_bool(apArg[1]) ){` |
|    ! 0 |  774 | `		zVal = ph7_value_to_bool(apArg[1]) ? "1" : "";` |
|    ! 0 |  775 | `		nVal = (int)SyStrlen(zVal);` |
|    ! 0 |  776 | `	}else{` |
|    247 |  777 | `		zVal = ph7_value_to_string(apArg[1],&nVal);` |
|      - |  778 | `	}` |
|    247 |  779 | `	SyBufferFormat(zWho,sizeof(zWho),"%s()",ph7_function_name(pCtx));` |
|    247 |  780 | `	if( !IniValueAccepted(pVm,pSlot,zVal,(sxu32)nVal,zWho) ){` |
|     21 |  781 | `		SyBlobRelease(&sOld);` |
|     21 |  782 | `		ph7_result_bool(pCtx,0);` |
|     21 |  783 | `		return PH7_OK;` |
|      - |  784 | `	}` |
|    230 |  785 | `	IniWriteLocal(pSlot,zVal,(sxu32)nVal);` |
|    230 |  786 | `	IniLiveSet(pVm,pSlot,zVal,(sxu32)nVal);` |
|    230 |  787 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOld),(int)SyBlobLength(&sOld));` |
|    230 |  788 | `	SyBlobRelease(&sOld);` |
|    230 |  789 | `	return PH7_OK;` |
|    126 |  790 | `}` |
|      - |  791 | `/* void ini_restore(string $option) */` |
|     12 |  792 | `static int vm_builtin_ini_restore(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  793 | `{` |
|     15 |  794 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  795 | `	VmIniSlot *pSlot;` |
|      - |  796 | `	const char *zName;` |
|     15 |  797 | `	int nName = 0;` |
|     15 |  798 | `	ph7_result_null(pCtx);` |
|     15 |  799 | `	if( nArg < 1 \|\| IniSeed(pVm) != SXRET_OK ){` |
|    ! 0 |  800 | `		return PH7_OK;` |
|      - |  801 | `	}` |
|     15 |  802 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|     15 |  803 | `	pSlot = IniFind(pVm,zName,(sxu32)nName);` |
|     15 |  804 | `	if( pSlot == 0 \|\| IniSessionLocked(pCtx,pSlot,"ini_restore") ){` |
|      3 |  805 | `		return PH7_OK;` |
|      - |  806 | `	}` |
|     12 |  807 | `	SyBlobReset(&pSlot->sLocal);` |
|     12 |  808 | `	SyBlobAppend(&pSlot->sLocal,SyBlobData(&pSlot->sGlobal),SyBlobLength(&pSlot->sGlobal));` |
|     12 |  809 | `	pSlot->bLocalNull = pSlot->bGlobalNull;   /* an unset directive goes back to unset */` |
|     12 |  810 | `	IniLiveSet(pVm,pSlot,(const char *)SyBlobData(&pSlot->sGlobal),SyBlobLength(&pSlot->sGlobal));` |
|     12 |  811 | `	return PH7_OK;` |
|      9 |  812 | `}` |
|      - |  813 | `/*` |
|      - |  814 | ` * The extension id php's module registry would find for this name: the key it` |
|      - |  815 | ` * stores is the extension name FOLDED, and the lookup against it is exact.` |
|      - |  816 | ` */` |
|     58 |  817 | `static int VmIniExtensionId(const char *zName,int nName)` |
|      4 |  818 | `{` |
|      - |  819 | `	int iExt,n;` |
|    982 |  820 | `	for( iExt = 0 ; iExt < PH7_VmExtensionCount() ; ++iExt ){` |
|      - |  821 | `		const char *zCanon;` |
|    966 |  822 | `		if( !PH7_VmExtensionAvailable(iExt) ){` |
|      2 |  823 | `			continue;` |
|      - |  824 | `		}` |
|    966 |  825 | `		zCanon = PH7_VmExtensionName(iExt);` |
|    966 |  826 | `		if( (int)SyStrlen(zCanon) != nName ){` |
|    830 |  827 | `			continue;` |
|      - |  828 | `		}` |
|    392 |  829 | `		for( n = 0 ; n < nName ; ++n ){` |
|    350 |  830 | `			if( (char)SyToLower(zCanon[n]) != zName[n] ){` |
|     96 |  831 | `				break;` |
|      - |  832 | `			}` |
|    130 |  833 | `		}` |
|    140 |  834 | `		if( n == nName ){` |
|     46 |  835 | `			return iExt;` |
|      - |  836 | `		}` |
|     49 |  837 | `	}` |
|     17 |  838 | `	return -1;` |
|     33 |  839 | `}` |
|      - |  840 | `/* array\|false ini_get_all(?string $extension = null, bool $details = true) */` |
|     91 |  841 | `static int vm_builtin_ini_get_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  842 | `{` |
|     95 |  843 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  844 | `	VmIniSlot *aSlot;` |
|      - |  845 | `	ph7_value *pOut,*pCur;` |
|     95 |  846 | `	const char *zExt = 0;` |
|     95 |  847 | `	int nExt = 0, bDetails = 1, iExtSel = -1;` |
|      - |  848 | `	sxu32 n;` |
|      - |  849 | `	SyBlob sVal;` |
|     95 |  850 | `	if( IniSeed(pVm) != SXRET_OK ){` |
|    ! 0 |  851 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  852 | `		return PH7_OK;` |
|      - |  853 | `	}` |
|     95 |  854 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|     62 |  855 | `		zExt = ph7_value_to_string(apArg[0],&nExt);` |
|     29 |  856 | `	}` |
|     95 |  857 | `	if( nArg > 1 ){` |
|     29 |  858 | `		bDetails = ph7_value_to_bool(apArg[1]);` |
|     14 |  859 | `	}` |
|     95 |  860 | `	if( zExt ){` |
|      - |  861 | `		/* php looks the name up in the MODULE REGISTRY, whose key is the extension` |
|      - |  862 | `		 * name folded down -- so the match is case-SENSITIVE against that key:` |
|      - |  863 | ``		 * `core` and `spl` are found, `Core` and `SPL` are not, and every other`` |
|      - |  864 | `		 * name is a warning and false. An extension that owns no directive is` |
|      - |  865 | ``		 * still found and answers the EMPTY array; a `phl.stub_extensions` name`` |
|      - |  866 | `		 * is no module and is not found. This engine used to accept five names` |
|      - |  867 | `		 * spelled its own way and refuse the rest. */` |
|     62 |  868 | `		iExtSel = VmIniExtensionId(zExt,nExt);` |
|     62 |  869 | `		if( iExtSel < 0 ){` |
|     25 |  870 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      8 |  871 | `				"Extension \"%.*s\" cannot be found",nExt,zExt);` |
|     17 |  872 | `			ph7_result_bool(pCtx,0);` |
|     17 |  873 | `			return PH7_OK;` |
|      - |  874 | `		}` |
|     21 |  875 | `	}` |
|     79 |  876 | `	pOut = ph7_context_new_array(pCtx);` |
|     79 |  877 | `	pCur = ph7_context_new_scalar(pCtx);` |
|     79 |  878 | `	if( pOut == 0 \|\| pCur == 0 ){` |
|    ! 0 |  879 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  880 | `	}` |
|     79 |  881 | `	SyBlobInit(&sVal,&pVm->sAllocator);` |
|     79 |  882 | `	aSlot = (VmIniSlot *)SySetBasePtr(&pVm->aIniTab);` |
|      - |  883 | `	/* The table is stored sorted, so this walk is already php's ksort order. */` |
|   5254 |  884 | `	for( n = 0 ; n < SySetUsed(&pVm->aIniTab) ; n++ ){` |
|   5179 |  885 | `		VmIniSlot *pSlot = &aSlot[n];` |
|      - |  886 | `		char zKey[128];` |
|      - |  887 | `		/* A directive belongs where the extension partition puts it rather than` |
|      - |  888 | `` 		 * where its NAME points -- php's `standard` owns `session.trans_sid_tags` `` |
|      - |  889 | ``		 * and its `Core` owns none of the `session.*` ones. `core` is php's own`` |
|      - |  890 | `		 * exception and answers EVERY directive whatever module registered it. */` |
|   5175 |  891 | `		if( iExtSel >= 0 && iExtSel != PH7_EXT_CORE` |
|   2695 |  892 | `		 && PH7_VmExtOfIni(pSlot->sName.zString,(int)pSlot->sName.nByte) != iExtSel ){` |
|   2282 |  893 | `			continue;` |
|      - |  894 | `		}` |
|   2901 |  895 | `		if( pSlot->sName.nByte >= sizeof(zKey) ){` |
|    ! 0 |  896 | `			continue;` |
|      - |  897 | `		}` |
|   2901 |  898 | `		SyMemcpy(pSlot->sName.zString,zKey,pSlot->sName.nByte);` |
|   2901 |  899 | `		zKey[pSlot->sName.nByte] = 0;` |
|   2901 |  900 | `		IniLiveGet(pVm,pSlot,&sVal);` |
|   2901 |  901 | `		if( bDetails ){` |
|   2211 |  902 | `			ph7_value *pRow = ph7_context_new_array(pCtx);` |
|   2211 |  903 | `			if( pRow == 0 ){` |
|    ! 0 |  904 | `				break;` |
|      - |  905 | `			}` |
|      - |  906 | `			{` |
|   2211 |  907 | `				const char *zG = 0;` |
|   2211 |  908 | `				sxu32 nG = 0;` |
|   2211 |  909 | `				if( IniGlobalValue(pSlot,&zG,&nG) ){` |
|   2025 |  910 | `					ph7_value_string(pCur,zG,(int)nG);` |
|    983 |  911 | `				}else{` |
|    188 |  912 | `					ph7_value_null(pCur);` |
|      - |  913 | `				}` |
|      - |  914 | `			}` |
|   2211 |  915 | `			ph7_array_add_strkey_elem(pRow,"global_value",pCur);` |
|   2211 |  916 | `			ph7_value_reset_string_cursor(pCur);` |
|   2211 |  917 | `			if( pSlot->bLocalNull ){` |
|    188 |  918 | `				ph7_value_null(pCur);` |
|     92 |  919 | `			}else{` |
|   2025 |  920 | `				ph7_value_string(pCur,(const char *)SyBlobData(&sVal),(int)SyBlobLength(&sVal));` |
|      - |  921 | `			}` |
|   2211 |  922 | `			ph7_array_add_strkey_elem(pRow,"local_value",pCur);` |
|   2211 |  923 | `			ph7_value_reset_string_cursor(pCur);` |
|   2211 |  924 | `			ph7_value_int(pCur,pSlot->iAccess);` |
|   2211 |  925 | `			ph7_array_add_strkey_elem(pRow,"access",pCur);` |
|   2211 |  926 | `			ph7_value_reset_string_cursor(pCur);` |
|   2211 |  927 | `			ph7_array_add_strkey_elem(pOut,zKey,pRow);` |
|   1073 |  928 | `		}else{` |
|    691 |  929 | `			ph7_value_reset_string_cursor(pCur);` |
|    691 |  930 | `			if( pSlot->bLocalNull ){` |
|     61 |  931 | `				ph7_value_null(pCur);` |
|     31 |  932 | `			}else{` |
|    631 |  933 | `				ph7_value_string(pCur,(const char *)SyBlobData(&sVal),(int)SyBlobLength(&sVal));` |
|      - |  934 | `			}` |
|    691 |  935 | `			ph7_array_add_strkey_elem(pOut,zKey,pCur);` |
|    691 |  936 | `			ph7_value_reset_string_cursor(pCur);` |
|      - |  937 | `		}` |
|   1418 |  938 | `	}` |
|     79 |  939 | `	SyBlobRelease(&sVal);` |
|     79 |  940 | `	ph7_result_value(pCtx,pOut);` |
|     79 |  941 | `	return PH7_OK;` |
|     49 |  942 | `}` |
|      - |  943 | `/* string\|false get_cfg_var(string $option) — php answers the GLOBAL value */` |
|     14 |  944 | `static int vm_builtin_get_cfg_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  945 | `{` |
|     16 |  946 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  947 | `	VmIniSlot *pSlot;` |
|      - |  948 | `	const char *zName;` |
|     16 |  949 | `	int nName = 0;` |
|     16 |  950 | `	if( nArg < 1 \|\| IniSeed(pVm) != SXRET_OK ){` |
|    ! 0 |  951 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  952 | `		return PH7_OK;` |
|      - |  953 | `	}` |
|     16 |  954 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|     16 |  955 | `	pSlot = IniFind(pVm,zName,(sxu32)nName);` |
|     16 |  956 | `	if( pSlot == 0 \|\| pSlot->bGlobalNull ){` |
|      - |  957 | `		/* php reads this one from the php.ini FILE rather than from the live` |
|      - |  958 | `		 * directive, so a name the file never mentioned is false -- and a` |
|      - |  959 | `		 * directive declared with no value is exactly such a name, whatever a` |
|      - |  960 | `		 * later ini_set() put in it. */` |
|     10 |  961 | `		ph7_result_bool(pCtx,0);` |
|     10 |  962 | `		return PH7_OK;` |
|      - |  963 | `	}` |
|     11 |  964 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&pSlot->sGlobal),` |
|      6 |  965 | `		(int)SyBlobLength(&pSlot->sGlobal));` |
|      8 |  966 | `	return PH7_OK;` |
|      9 |  967 | `}` |
|      - |  968 | `/*` |
|      - |  969 | ` * The effective value of a directive a C builtin needs to obey, as an integer.` |
|      - |  970 | ` * parse_str() reads max_input_vars and max_input_nesting_level through this;` |
|      - |  971 | ` * without it a builtin would have to duplicate php's default and could never` |
|      - |  972 | `` * see an `ini_set()`/`-d` override. Answers iDefault when the directive is`` |
|      - |  973 | ` * absent or unparsable.` |
|      - |  974 | ` */` |
|   1717 |  975 | `PH7_PRIVATE sxi64 PH7_VmIniGetInt(ph7_vm *pVm,const char *zName,sxi64 iDefault)` |
|      5 |  976 | `{` |
|      - |  977 | `	VmIniSlot *pSlot;` |
|      - |  978 | `	SyBlob sVal;` |
|   1722 |  979 | `	sxi64 iVal = iDefault;` |
|   1722 |  980 | `	IniSeed(pVm);` |
|   1722 |  981 | `	pSlot = IniFind(pVm,zName,(sxu32)SyStrlen(zName));` |
|   1722 |  982 | `	if( pSlot == 0 ){` |
|    ! 0 |  983 | `		return iDefault;` |
|      - |  984 | `	}` |
|   1722 |  985 | `	SyBlobInit(&sVal,&pVm->sAllocator);` |
|   1722 |  986 | `	IniLiveGet(pVm,pSlot,&sVal);` |
|   1722 |  987 | `	if( SyBlobLength(&sVal) > 0 ){` |
|   1722 |  988 | `		SyStrToInt64((const char *)SyBlobData(&sVal),SyBlobLength(&sVal),(void *)&iVal,0);` |
|    859 |  989 | `	}` |
|   1722 |  990 | `	SyBlobRelease(&sVal);` |
|   1722 |  991 | `	return iVal;` |
|    864 |  992 | `}` |
|      - |  993 | `/* The same, as a borrowed STRING (arg_separator.input). The bytes live in the` |
|      - |  994 | ` * caller's blob, which it owns. */` |
|      - |  995 | `/*` |
|      - |  996 | ` * One directive as the export format needs it: php's access bitmask, the value` |
|      - |  997 | `` * a script reads now, and the DEFAULT behind it. `pOut`/`pDef` are the`` |
|      - |  998 | ` * caller's blobs. Answers 0 when the build has no such directive.` |
|      - |  999 | ` */` |
|     62 | 1000 | `PH7_PRIVATE int PH7_VmIniDescribe(ph7_vm *pVm,const char *zName,sxu32 nName,` |
|      - | 1001 | `	sxi32 *piAccess,SyBlob *pOut,SyBlob *pDef)` |
|      1 | 1002 | `{` |
|      - | 1003 | `	VmIniSlot *pSlot;` |
|     63 | 1004 | `	IniSeed(&(*pVm));` |
|     63 | 1005 | `	pSlot = IniFind(&(*pVm),zName,nName);` |
|     63 | 1006 | `	if( pSlot == 0 ){` |
|    ! 0 | 1007 | `		return 0;` |
|      - | 1008 | `	}` |
|     63 | 1009 | `	*piAccess = pSlot->iAccess;` |
|     63 | 1010 | `	IniLiveGet(&(*pVm),pSlot,pOut);` |
|     63 | 1011 | `	SyBlobReset(pDef);` |
|     63 | 1012 | `	SyBlobAppend(pDef,SyBlobData(&pSlot->sGlobal),SyBlobLength(&pSlot->sGlobal));` |
|     63 | 1013 | `	return 1;` |
|     32 | 1014 | `}` |
|   1579 | 1015 | `PH7_PRIVATE void PH7_VmIniGetStr(ph7_vm *pVm,const char *zName,SyBlob *pOut)` |
|      5 | 1016 | `{` |
|      - | 1017 | `	VmIniSlot *pSlot;` |
|   1584 | 1018 | `	IniSeed(pVm);` |
|   1584 | 1019 | `	SyBlobReset(pOut);` |
|   1584 | 1020 | `	pSlot = IniFind(pVm,zName,(sxu32)SyStrlen(zName));` |
|   1584 | 1021 | `	if( pSlot ){` |
|   1530 | 1022 | `		IniLiveGet(pVm,pSlot,pOut);` |
|    723 | 1023 | `	}` |
|   1584 | 1024 | `}` |
|      - | 1025 | `/*` |
|      - | 1026 | ` * Does this directive currently hold php's UNSET value? Only the surfaces that` |
|      - | 1027 | ` * show a RAW value ask -- ini_get() answers the empty string for one either` |
|      - | 1028 | ` * way, which is why the question has to be put separately.` |
|      - | 1029 | ` */` |
|    690 | 1030 | `PH7_PRIVATE int PH7_VmIniIsUnset(ph7_vm *pVm,const char *zName)` |
|      3 | 1031 | `{` |
|      - | 1032 | `	VmIniSlot *pSlot;` |
|    693 | 1033 | `	IniSeed(pVm);` |
|    693 | 1034 | `	pSlot = IniFind(pVm,zName,(sxu32)SyStrlen(zName));` |
|    693 | 1035 | `	return pSlot != 0 && pSlot->bLocalNull;` |
|      3 | 1036 | `}` |
|      - | 1037 | `/*` |
|      - | 1038 | ` * Read a BOOLEAN directive the way zend_ini does — "on"/"yes"/"true" as well as` |
|      - | 1039 | ` * a non-zero number. Reading one through the integer parser answers 0 for` |
|      - | 1040 | `` * `On`, which is the spelling php.ini-production ships, so a directive written`` |
|      - | 1041 | ` * that way reads as OFF.` |
|      - | 1042 | ` */` |
|   1271 | 1043 | `PH7_PRIVATE int PH7_VmIniGetBool(ph7_vm *pVm,const char *zName,int bDefault)` |
|      5 | 1044 | `{` |
|      - | 1045 | `	VmIniSlot *pSlot;` |
|      - | 1046 | `	SyBlob sVal;` |
|      - | 1047 | `	int bRes;` |
|   1276 | 1048 | `	IniSeed(pVm);` |
|   1276 | 1049 | `	pSlot = IniFind(pVm,zName,(sxu32)SyStrlen(zName));` |
|   1276 | 1050 | `	if( pSlot == 0 ){` |
|    ! 0 | 1051 | `		return bDefault;` |
|      - | 1052 | `	}` |
|   1276 | 1053 | `	SyBlobInit(&sVal,&pVm->sAllocator);` |
|   1276 | 1054 | `	IniLiveGet(pVm,pSlot,&sVal);` |
|   1276 | 1055 | `	bRes = IniTruthy((const char *)SyBlobData(&sVal),SyBlobLength(&sVal));` |
|   1276 | 1056 | `	SyBlobRelease(&sVal);` |
|   1276 | 1057 | `	return bRes;` |
|    631 | 1058 | `}` |
|   6721 | 1059 | `PH7_PRIVATE sxi32 PH7_VmInstallIni(ph7_vm *pVm)` |
|      5 | 1060 | `{` |
|      - | 1061 | `	static const struct {` |
|      - | 1062 | `		const char *zName;` |
|      - | 1063 | `		ProchHostFunction xFunc;` |
|      - | 1064 | `	} aFunc[] = {` |
|      - | 1065 | `		{ "ini_get",      vm_builtin_ini_get      },` |
|      - | 1066 | `		{ "ini_set",      vm_builtin_ini_set      },` |
|      - | 1067 | `		/* php's alias for the same routine. Both of the diagnostics a write` |
|      - | 1068 | `		 * can raise name the INVOKED function, so they read the context's` |
|      - | 1069 | `		 * name rather than a literal. */` |
|      - | 1070 | `		{ "ini_alter",    vm_builtin_ini_set      },` |
|      - | 1071 | `		{ "ini_restore",  vm_builtin_ini_restore  },` |
|      - | 1072 | `		{ "ini_get_all",  vm_builtin_ini_get_all  },` |
|      - | 1073 | `		{ "get_cfg_var",  vm_builtin_get_cfg_var  },` |
|      - | 1074 | `	};` |
|      - | 1075 | `	sxu32 n;` |
|  47052 | 1076 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFunc) ; n++ ){` |
|  40331 | 1077 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
|  20141 | 1078 | `	}` |
|   6726 | 1079 | `	return SXRET_OK;` |
|      5 | 1080 | `}` |
|      - | 1081 | `#else` |
|      - | 1082 | `PH7_PRIVATE sxi32 PH7_VmInstallIni(ph7_vm *pVm){ (void)pVm; return SXRET_OK; }` |
|      - | 1083 | `PH7_PRIVATE sxi64 PH7_VmIniGetInt(ph7_vm *pVm,const char *zName,sxi64 iDefault){` |
|      - | 1084 | `	(void)pVm; (void)zName; return iDefault;` |
|      - | 1085 | `}` |
|      - | 1086 | `PH7_PRIVATE int PH7_VmIniGetBool(ph7_vm *pVm,const char *zName,int bDefault){` |
|      - | 1087 | `	(void)pVm; (void)zName; return bDefault;` |
|      - | 1088 | `}` |
|      - | 1089 | `PH7_PRIVATE void PH7_VmIniGetStr(ph7_vm *pVm,const char *zName,SyBlob *pOut){` |
|      - | 1090 | `	(void)pVm; (void)zName; SyBlobReset(pOut);` |
|      - | 1091 | `}` |
|      - | 1092 | `PH7_PRIVATE int PH7_VmIniIsUnset(ph7_vm *pVm,const char *zName){` |
|      - | 1093 | `	(void)pVm; (void)zName; return 0;` |
|      - | 1094 | `}` |
|      - | 1095 | `PH7_PRIVATE int PH7_VmIniDescribe(ph7_vm *pVm,const char *zName,sxu32 nName,` |
|      - | 1096 | `	sxi32 *piAccess,SyBlob *pOut,SyBlob *pDef){` |
|      - | 1097 | `	(void)pVm; (void)zName; (void)nName; (void)piAccess;` |
|      - | 1098 | `	SyBlobReset(pOut); SyBlobReset(pDef); return 0;` |
|      - | 1099 | `}` |
|      - | 1100 | `PH7_PRIVATE int PH7_VmIniSet(ph7_vm *pVm,const char *zName,sxu32 nName,` |
|      - | 1101 | `	const char *zVal,sxu32 nVal,const char *zWho){` |
|      - | 1102 | `	(void)pVm; (void)zName; (void)nName; (void)zVal; (void)nVal; (void)zWho; return 0;` |
|      - | 1103 | `}` |
|      - | 1104 | `#endif` |
|      - | 1105 |  |
