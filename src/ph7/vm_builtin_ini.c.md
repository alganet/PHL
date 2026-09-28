# src/ph7/vm_builtin_ini.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 381/450 lines (84.67%)

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
|      - |   41 | `	const char *zValue;` |
|      - |   42 | `	sxi32 iAccess;` |
|      - |   43 | `} aIniDefault[] = {` |
|      - |   44 | `	{ "allow_url_fopen",          "1",          VM_INI_PERDIR\|VM_INI_SYSTEM },` |
|      - |   45 | `	/* php's default is OFF, and it spells that default as the EMPTY string — which` |
|      - |   46 | `	 * is what ini_get() answers. Including a remote file is the classic RFI, and` |
|      - |   47 | `	 * this is what a STREAM_IS_URL wrapper's include is gated on. */` |
|      - |   48 | `	{ "allow_url_include",        "",           VM_INI_SYSTEM },` |
|      - |   49 | `	{ "arg_separator.input",      "&",          VM_INI_ALL },` |
|      - |   50 | `	{ "arg_separator.output",     "&",          VM_INI_ALL },` |
|      - |   51 | `	{ "auto_detect_line_endings", "",           VM_INI_ALL },` |
|      - |   52 | `	/* ext/bcmath's only directive: the scale every bc* function defaults its` |
|      - |   53 | `	 * $scale argument from, and the one slot bcscale() reads and writes. */` |
|      - |   54 | `	{ "bcmath.scale",             "0",          VM_INI_ALL },` |
|      - |   55 | `	{ "date.timezone",            "UTC",        VM_INI_ALL },` |
|      - |   56 | `	{ "default_charset",          "UTF-8",      VM_INI_ALL },` |
|      - |   57 | `	/* php bounds a socket wait by this rather than waiting forever, and it is` |
|      - |   58 | `	 * where stream_socket_accept() takes its default timeout from. */` |
|      - |   59 | `	{ "default_socket_timeout",   "60",         VM_INI_ALL },` |
|      - |   60 | `	{ "default_mimetype",         "text/html",  VM_INI_ALL },` |
|      - |   61 | `	{ "display_errors",           "",           VM_INI_ALL },` |
|      - |   62 | `	{ "error_log",                "",           VM_INI_ALL },` |
|      - |   63 | `	{ "error_reporting",          "30719",      VM_INI_ALL },` |
|      - |   64 | `	{ "highlight.comment",        "#FF8000",    VM_INI_ALL },` |
|      - |   65 | `	{ "highlight.default",        "#0000BB",    VM_INI_ALL },` |
|      - |   66 | `	{ "highlight.html",           "#000000",    VM_INI_ALL },` |
|      - |   67 | `	{ "highlight.keyword",        "#007700",    VM_INI_ALL },` |
|      - |   68 | `	{ "highlight.string",         "#DD0000",    VM_INI_ALL },` |
|      - |   69 | `	{ "include_path",             ".",          VM_INI_ALL },` |
|      - |   70 | `	{ "log_errors",               "1",          VM_INI_ALL },` |
|      - |   71 | `	{ "max_execution_time",       "0",          VM_INI_ALL },` |
|      - |   72 | `	{ "max_input_nesting_level",  "64",         VM_INI_PERDIR\|VM_INI_SYSTEM },` |
|      - |   73 | `	{ "max_input_vars",           "1000",       VM_INI_PERDIR\|VM_INI_SYSTEM },` |
|      - |   74 | `	{ "memory_limit",             "-1",         VM_INI_ALL },` |
|      - |   75 | `	{ "post_max_size",            "8M",         VM_INI_PERDIR\|VM_INI_SYSTEM },` |
|      - |   76 | `	{ "precision",                "14",         VM_INI_ALL },` |
|      - |   77 | `	{ "serialize_precision",      "-1",         VM_INI_ALL },` |
|      - |   78 | `	/* php's session directives, the whole non-deprecated set: session_start()'s` |
|      - |   79 | `	 * $options array applies its keys THROUGH this table, so a directive missing` |
|      - |   80 | `	 * here is an option php accepts and PHL reports as failed.` |
|      - |   81 | `	 * Six are absent on purpose: php 8.4 DEPRECATES session.sid_length,` |
|      - |   82 | `	 * session.sid_bits_per_character, session.referer_check, session.use_trans_sid,` |
|      - |   83 | `	 * session.trans_sid_tags and session.trans_sid_hosts, and §10 does not carry` |
|      - |   84 | `	 * php's deprecated surface. The session.upload_progress.* family goes with the` |
|      - |   85 | ``	 * file uploads §10 excludes from a CLI-plus-`-S` engine. */`` |
|      - |   86 | `	{ "session.auto_start",       "0",          VM_INI_PERDIR\|VM_INI_SYSTEM },` |
|      - |   87 | `	{ "session.cache_expire",     "180",        VM_INI_ALL },` |
|      - |   88 | `	{ "session.cache_limiter",    "nocache",    VM_INI_ALL },` |
|      - |   89 | `	/* The Set-Cookie the session sends is built out of these seven. */` |
|      - |   90 | `	{ "session.cookie_domain",    "",           VM_INI_ALL },` |
|      - |   91 | `	{ "session.cookie_httponly",  "0",          VM_INI_ALL },` |
|      - |   92 | `	{ "session.cookie_lifetime",  "0",          VM_INI_ALL },` |
|      - |   93 | `	{ "session.cookie_partitioned","0",         VM_INI_ALL },` |
|      - |   94 | `	{ "session.cookie_path",      "/",          VM_INI_ALL },` |
|      - |   95 | `	{ "session.cookie_samesite",  "",           VM_INI_ALL },` |
|      - |   96 | `	{ "session.cookie_secure",    "0",          VM_INI_ALL },` |
|      - |   97 | `	{ "session.gc_divisor",       "100",        VM_INI_ALL },` |
|      - |   98 | `	{ "session.gc_maxlifetime",   "1440",       VM_INI_ALL },` |
|      - |   99 | `	{ "session.gc_probability",   "1",          VM_INI_ALL },` |
|      - |  100 | `	{ "session.lazy_write",       "1",          VM_INI_ALL },` |
|      - |  101 | `	{ "session.name",             "PHPSESSID",  VM_INI_ALL },` |
|      - |  102 | `	{ "session.save_handler",     "files",      VM_INI_ALL },` |
|      - |  103 | `	{ "session.save_path",        "",           VM_INI_ALL },` |
|      - |  104 | ``	/* Which of php's three session serializers writes the store: `php` (the`` |
|      - |  105 | ``	 * `name\|<serialized>` runs a stock php install reads), `php_binary` or`` |
|      - |  106 | ``	 * `php_serialize`. */`` |
|      - |  107 | `	{ "session.serialize_handler","php",        VM_INI_ALL },` |
|      - |  108 | `	/* Whether the session sends and reads its id as a cookie at all. PHL has never` |
|      - |  109 | `	 * read an id from anywhere ELSE, which is what use_only_cookies means. */` |
|      - |  110 | `	{ "session.use_cookies",      "1",          VM_INI_ALL },` |
|      - |  111 | `	{ "session.use_only_cookies", "1",          VM_INI_ALL },` |
|      - |  112 | `	{ "session.use_strict_mode",  "0",          VM_INI_ALL },` |
|      - |  113 | `	{ "short_open_tag",           "",           VM_INI_PERDIR\|VM_INI_SYSTEM },` |
|      - |  114 | `	{ "unserialize_callback_func","",           VM_INI_ALL },` |
|      - |  115 | `	{ "unserialize_max_depth",    "4096",       VM_INI_ALL },` |
|      - |  116 | `	{ "upload_max_filesize",      "2M",         VM_INI_PERDIR\|VM_INI_SYSTEM },` |
|      - |  117 | `	{ "zend.assertions",          "-1",         VM_INI_ALL },` |
|      - |  118 | `};` |
|      - |  119 |  |
|  30214 |  120 | `static int IniNameIs(const VmIniSlot *pSlot,const char *zName)` |
|      5 |  121 | `{` |
|  30219 |  122 | `	sxu32 n = (sxu32)SyStrlen(zName);` |
|  30219 |  123 | `	return pSlot->sName.nByte == n && SyMemcmp(pSlot->sName.zString,zName,n) == 0;` |
|      5 |  124 | `}` |
|      - |  125 | `/*` |
|      - |  126 | ` * zend_ini_parse_bool semantics, matching the C-side VmIniBool the -d/-c path` |
|      - |  127 | ` * uses: on/yes/true, else a non-zero integer parse.` |
|      - |  128 | ` */` |
|    608 |  129 | `static int IniTruthy(const char *zVal,sxu32 nVal)` |
|      5 |  130 | `{` |
|    915 |  131 | `	while( nVal > 0 && (zVal[0] == ' ' \|\| zVal[0] == '\t') ){ zVal++; nVal--; }` |
|    915 |  132 | `	while( nVal > 0 && (zVal[nVal-1] == ' ' \|\| zVal[nVal-1] == '\t') ){ nVal--; }` |
|    613 |  133 | `	if( nVal == 2 && SyStrnicmp(zVal,"on",2) == 0 ){ return 1; }` |
|    613 |  134 | `	if( nVal == 3 && SyStrnicmp(zVal,"yes",3) == 0 ){ return 1; }` |
|    613 |  135 | `	if( nVal == 4 && SyStrnicmp(zVal,"true",4) == 0 ){ return 1; }` |
|      - |  136 | `	{` |
|    613 |  137 | `		sxi32 iVal = 0;` |
|    613 |  138 | `		if( nVal > 0 && SyStrToInt32(zVal,nVal,(void *)&iVal,0) == SXRET_OK ){` |
|    609 |  139 | `			return iVal != 0;` |
|      - |  140 | `		}` |
|      - |  141 | `	}` |
|      5 |  142 | `	return 0;` |
|    309 |  143 | `}` |
|      - |  144 | `/*` |
|      - |  145 | ` * Build the table: the static defaults, then the CLI queue merged over them (an` |
|      - |  146 | ` * unknown CLI name is appended as a new INI_ALL directive, as the chunk did),` |
|      - |  147 | ` * then sorted by name so ini_get_all() can walk it in php's order without a sort.` |
|      - |  148 | ` */` |
|   3692 |  149 | `static sxi32 IniSeed(ph7_vm *pVm)` |
|      5 |  150 | `{` |
|      - |  151 | `	sxu32 i,j;` |
|      - |  152 | `	VmIniEntry *aCli;` |
|      - |  153 | `	VmIniSlot *aSlot;` |
|   3697 |  154 | `	if( pVm->bIniSeeded ){` |
|   3577 |  155 | `		return SXRET_OK;` |
|      - |  156 | `	}` |
|    125 |  157 | `	pVm->bIniSeeded = 1; /* set FIRST: the live-wired writes below re-enter nothing,` |
|      - |  158 | `	                      * but a future one must never recurse into the seed */` |
|   6485 |  159 | `	for( i = 0 ; i < SX_ARRAYSIZE(aIniDefault) ; i++ ){` |
|      - |  160 | `		VmIniSlot sSlot;` |
|   6365 |  161 | `		SyStringInitFromBuf(&sSlot.sName,aIniDefault[i].zName,SyStrlen(aIniDefault[i].zName));` |
|   6365 |  162 | `		sSlot.iAccess = aIniDefault[i].iAccess;` |
|   6365 |  163 | `		SyBlobInit(&sSlot.sGlobal,&pVm->sAllocator);` |
|   6365 |  164 | `		SyBlobInit(&sSlot.sLocal,&pVm->sAllocator);` |
|   6365 |  165 | `		SyBlobAppend(&sSlot.sGlobal,aIniDefault[i].zValue,(sxu32)SyStrlen(aIniDefault[i].zValue));` |
|   6365 |  166 | `		SyBlobAppend(&sSlot.sLocal,aIniDefault[i].zValue,(sxu32)SyStrlen(aIniDefault[i].zValue));` |
|   6365 |  167 | `		if( SySetPut(&pVm->aIniTab,(const void *)&sSlot) != SXRET_OK ){` |
|    ! 0 |  168 | `			return SXERR_MEM;` |
|      - |  169 | `		}` |
|   3185 |  170 | `	}` |
|    125 |  171 | `	aCli = (VmIniEntry *)SySetBasePtr(&pVm->aIniCli);` |
|    151 |  172 | `	for( i = 0 ; i < SySetUsed(&pVm->aIniCli) ; i++ ){` |
|     29 |  173 | `		int bFound = 0;` |
|     29 |  174 | `		aSlot = (VmIniSlot *)SySetBasePtr(&pVm->aIniTab);` |
|    757 |  175 | `		for( j = 0 ; j < SySetUsed(&pVm->aIniTab) ; j++ ){` |
|    752 |  176 | `			if( aSlot[j].sName.nByte == aCli[i].sName.nByte` |
|    413 |  177 | `			 && SyMemcmp(aSlot[j].sName.zString,aCli[i].sName.zString,aCli[i].sName.nByte) == 0 ){` |
|     27 |  178 | `				SyBlobReset(&aSlot[j].sGlobal);` |
|     27 |  179 | `				SyBlobReset(&aSlot[j].sLocal);` |
|     27 |  180 | `				SyBlobAppend(&aSlot[j].sGlobal,aCli[i].sValue.zString,aCli[i].sValue.nByte);` |
|     27 |  181 | `				SyBlobAppend(&aSlot[j].sLocal,aCli[i].sValue.zString,aCli[i].sValue.nByte);` |
|     27 |  182 | `				bFound = 1;` |
|     27 |  183 | `				break;` |
|      - |  184 | `			}` |
|    367 |  185 | `		}` |
|     29 |  186 | `		if( !bFound ){` |
|      - |  187 | `			VmIniSlot sSlot;` |
|      - |  188 | `			/* aIniCli holds VM-lifetime copies already, so the name can be aliased. */` |
|      3 |  189 | `			sSlot.sName = aCli[i].sName;` |
|      3 |  190 | `			sSlot.iAccess = VM_INI_ALL;` |
|      3 |  191 | `			SyBlobInit(&sSlot.sGlobal,&pVm->sAllocator);` |
|      3 |  192 | `			SyBlobInit(&sSlot.sLocal,&pVm->sAllocator);` |
|      3 |  193 | `			SyBlobAppend(&sSlot.sGlobal,aCli[i].sValue.zString,aCli[i].sValue.nByte);` |
|      3 |  194 | `			SyBlobAppend(&sSlot.sLocal,aCli[i].sValue.zString,aCli[i].sValue.nByte);` |
|      3 |  195 | `			if( SySetPut(&pVm->aIniTab,(const void *)&sSlot) != SXRET_OK ){` |
|    ! 0 |  196 | `				return SXERR_MEM;` |
|      - |  197 | `			}` |
|      1 |  198 | `		}` |
|     16 |  199 | `	}` |
|      - |  200 | `	/* Insertion sort by name (the table is ~30 entries and already nearly sorted:` |
|      - |  201 | `	 * only CLI-introduced directives are out of place). */` |
|    125 |  202 | `	aSlot = (VmIniSlot *)SySetBasePtr(&pVm->aIniTab);` |
|   6367 |  203 | `	for( i = 1 ; i < SySetUsed(&pVm->aIniTab) ; i++ ){` |
|   6247 |  204 | `		VmIniSlot sTmp = aSlot[i];` |
|   6247 |  205 | `		j = i;` |
|   6425 |  206 | `		while( j > 0 ){` |
|   6425 |  207 | `			const VmIniSlot *pPrev = &aSlot[j-1];` |
|   6425 |  208 | `			sxu32 nMin = pPrev->sName.nByte < sTmp.sName.nByte ? pPrev->sName.nByte : sTmp.sName.nByte;` |
|   6425 |  209 | `			sxi32 iCmp = SyMemcmp(pPrev->sName.zString,sTmp.sName.zString,nMin);` |
|   6425 |  210 | `			if( iCmp == 0 ){` |
|    ! 0 |  211 | `				iCmp = (sxi32)pPrev->sName.nByte - (sxi32)sTmp.sName.nByte;` |
|    ! 0 |  212 | `			}` |
|   6425 |  213 | `			if( iCmp <= 0 ){` |
|   6247 |  214 | `				break;` |
|      - |  215 | `			}` |
|    183 |  216 | `			aSlot[j] = aSlot[j-1];` |
|    183 |  217 | `			j--;` |
|      5 |  218 | `		}` |
|   6247 |  219 | `		aSlot[j] = sTmp;` |
|   3126 |  220 | `	}` |
|      - |  221 | `	/* Boot-apply the CLI values for the live-wired session knobs. The engine knobs` |
|      - |  222 | `	 * (error_reporting / date.timezone) were already applied C-side by` |
|      - |  223 | `	 * PH7_VM_CONFIG_INI_ENTRY. */` |
|    125 |  224 | `	aSlot = (VmIniSlot *)SySetBasePtr(&pVm->aIniTab);` |
|   6487 |  225 | `	for( i = 0 ; i < SySetUsed(&pVm->aIniTab) ; i++ ){` |
|   6367 |  226 | `		SyBlob *pDst = 0;` |
|   6367 |  227 | `		if( IniNameIs(&aSlot[i],"session.name") ){` |
|    120 |  228 | `			if( SyBlobLength(&aSlot[i].sGlobal) != sizeof("PHPSESSID")-1` |
|    123 |  229 | `			 \|\| SyMemcmp(SyBlobData(&aSlot[i].sGlobal),"PHPSESSID",sizeof("PHPSESSID")-1) != 0 ){` |
|      6 |  230 | `				pDst = &pVm->sSessName;` |
|      8 |  231 | `			}` |
|   6307 |  232 | `		}else if( IniNameIs(&aSlot[i],"session.save_path") ){` |
|    125 |  233 | `			if( SyBlobLength(&aSlot[i].sGlobal) > 0 ){` |
|    ! 0 |  234 | `				pDst = &pVm->sSessPath;` |
|    ! 0 |  235 | `			}` |
|     60 |  236 | `		}` |
|   6367 |  237 | `		if( pDst ){` |
|      6 |  238 | `			sxu32 nLen = SyBlobLength(&aSlot[i].sGlobal);` |
|      6 |  239 | `			const char *zVal = (const char *)SyBlobData(&aSlot[i].sGlobal);` |
|      6 |  240 | `			while( nLen > 0 && zVal[nLen-1] == '/' ){ nLen--; } /* rtrim('/') */` |
|      6 |  241 | `			SyBlobReset(pDst);` |
|      6 |  242 | `			SyBlobAppend(pDst,zVal,nLen);` |
|      3 |  243 | `		}` |
|   3186 |  244 | `	}` |
|    125 |  245 | `	return SXRET_OK;` |
|   1851 |  246 | `}` |
|   3684 |  247 | `static VmIniSlot * IniFind(ph7_vm *pVm,const char *zName,sxu32 nName)` |
|      5 |  248 | `{` |
|   3689 |  249 | `	VmIniSlot *aSlot = (VmIniSlot *)SySetBasePtr(&pVm->aIniTab);` |
|      - |  250 | `	sxu32 n;` |
| 114825 |  251 | `	for( n = 0 ; n < SySetUsed(&pVm->aIniTab) ; n++ ){` |
| 114764 |  252 | `		if( aSlot[n].sName.nByte == nName` |
|  61920 |  253 | `		 && SyMemcmp(aSlot[n].sName.zString,zName,nName) == 0 ){` |
|   3633 |  254 | `			return &aSlot[n];` |
|      - |  255 | `		}` |
|  55573 |  256 | `	}` |
|     59 |  257 | `	return 0;` |
|   1847 |  258 | `}` |
|      - |  259 | `/*` |
|      - |  260 | ` * The EFFECTIVE current value. For a live-wired directive the runtime knob is` |
|      - |  261 | ` * the truth, not the stored local value, so that ini_get() and the knob's own` |
|      - |  262 | ` * accessor can never disagree.` |
|      - |  263 | ` */` |
|   3704 |  264 | `static void IniLiveGet(ph7_vm *pVm,VmIniSlot *pSlot,SyBlob *pOut)` |
|      5 |  265 | `{` |
|   3709 |  266 | `	SyBlobReset(pOut);` |
|   3709 |  267 | `	if( IniNameIs(pSlot,"error_reporting") ){` |
|      - |  268 | `		char zBuf[32];` |
|      3 |  269 | `		int nBuf = SyBufferFormat(zBuf,sizeof(zBuf),"%d",` |
|      2 |  270 | `			pVm->bErrReport ? (int)pVm->iErrMask : 0);` |
|      3 |  271 | `		SyBlobAppend(pOut,zBuf,(sxu32)nBuf);` |
|      3 |  272 | `		return;` |
|      - |  273 | `	}` |
|   3707 |  274 | `	if( IniNameIs(pSlot,"session.name") ){` |
|     40 |  275 | `		SyBlobAppend(pOut,SyBlobData(&pVm->sSessName),SyBlobLength(&pVm->sSessName));` |
|     40 |  276 | `		return;` |
|      - |  277 | `	}` |
|   3669 |  278 | `	if( IniNameIs(pSlot,"session.save_path") && SyBlobLength(&pVm->sSessPath) > 0 ){` |
|     11 |  279 | `		SyBlobAppend(pOut,SyBlobData(&pVm->sSessPath),SyBlobLength(&pVm->sSessPath));` |
|     11 |  280 | `		return;` |
|      - |  281 | `	}` |
|   3659 |  282 | `	if( IniNameIs(pSlot,"include_path") ){` |
|      - |  283 | `		/* The VM's path SET is the store; this directive is a view of it, so` |
|      - |  284 | `		 * ini_get() and get_include_path() can never name different paths. */` |
|     17 |  285 | `		PH7_VmGetIncludePath(pVm,pOut);` |
|     17 |  286 | `		return;` |
|      - |  287 | `	}` |
|   3643 |  288 | `	SyBlobAppend(pOut,SyBlobData(&pSlot->sLocal),SyBlobLength(&pSlot->sLocal));` |
|   1857 |  289 | `}` |
|      - |  290 | `/*` |
|      - |  291 | ` * Push a new value at the runtime knob behind a live-wired directive. The stored` |
|      - |  292 | ` * local value is updated by the caller either way.` |
|      - |  293 | ` */` |
|    252 |  294 | `static void IniLiveSet(ph7_vm *pVm,VmIniSlot *pSlot,const char *zVal,sxu32 nVal)` |
|      5 |  295 | `{` |
|    257 |  296 | `	if( IniNameIs(pSlot,"error_reporting") ){` |
|    ! 0 |  297 | `		sxi32 iVal = 0;` |
|    ! 0 |  298 | `		if( nVal > 0 ){` |
|    ! 0 |  299 | `			SyStrToInt32(zVal,nVal,(void *)&iVal,0);` |
|    ! 0 |  300 | `		}` |
|    ! 0 |  301 | `		pVm->iErrMask = iVal;` |
|    ! 0 |  302 | `		pVm->bErrReport = iVal != 0;` |
|    ! 0 |  303 | `		return;` |
|      - |  304 | `	}` |
|    257 |  305 | `	if( IniNameIs(pSlot,"display_errors") ){` |
|      5 |  306 | `		pVm->bDisplayErrors = IniTruthy(zVal,nVal);` |
|      5 |  307 | `		return;` |
|      - |  308 | `	}` |
|    253 |  309 | `	if( IniNameIs(pSlot,"log_errors") ){` |
|    ! 0 |  310 | `		pVm->bLogErrors = IniTruthy(zVal,nVal);` |
|    ! 0 |  311 | `		return;` |
|      - |  312 | `	}` |
|    253 |  313 | `	if( IniNameIs(pSlot,"session.name") \|\| IniNameIs(pSlot,"session.save_path") ){` |
|     59 |  314 | `		int bPath = IniNameIs(pSlot,"session.save_path");` |
|     59 |  315 | `		SyBlob *pDst = bPath ? &pVm->sSessPath : &pVm->sSessName;` |
|     59 |  316 | `		sxu32 nLen = nVal;` |
|     59 |  317 | `		if( bPath ){` |
|     32 |  318 | `			while( nLen > 0 && zVal[nLen-1] == '/' ){ nLen--; }` |
|     15 |  319 | `		}` |
|     59 |  320 | `		SyBlobReset(pDst);` |
|     59 |  321 | `		SyBlobAppend(pDst,zVal,nLen);` |
|     59 |  322 | `		return;` |
|      - |  323 | `	}` |
|    197 |  324 | `	if( IniNameIs(pSlot,"include_path") ){` |
|      - |  325 | `		/* The one write that moves the include walk. Refused empty by the caller` |
|      - |  326 | `		 * (php's OnUpdateStringUnempty), so nVal is never 0 on the ini_set() path;` |
|      - |  327 | `		 * the boot/-d and ini_restore() paths carry a real value too. */` |
|      5 |  328 | `		PH7_VmSetIncludePath(pVm,zVal,nVal);` |
|      5 |  329 | `		return;` |
|      - |  330 | `	}` |
|    193 |  331 | `	if( IniNameIs(pSlot,"date.timezone") ){` |
|      - |  332 | `		/* Only UTC/GMT exist here (no tz database), matching the engine's own` |
|      - |  333 | `		 * date_default_timezone_set(). */` |
|    ! 0 |  334 | `		if( nVal == 3 && (SyStrnicmp(zVal,"UTC",3) == 0 \|\| SyStrnicmp(zVal,"GMT",3) == 0) ){` |
|    ! 0 |  335 | `			SyMemcpy(zVal,pVm->zDefTz,3);` |
|    ! 0 |  336 | `			pVm->zDefTz[3] = 0;` |
|    ! 0 |  337 | `			pVm->nDefTz = 3;` |
|    ! 0 |  338 | `		}` |
|    ! 0 |  339 | `		return;` |
|      - |  340 | `	}` |
|    131 |  341 | `}` |
|      - |  342 | `/*` |
|      - |  343 | ` * A session directive is settable only while there is no session to disturb: not` |
|      - |  344 | ` * once one is ACTIVE (the store is open and the cookie decided), and not once` |
|      - |  345 | ` * headers have gone out. Answers TRUE (having raised the warning) when the write` |
|      - |  346 | ` * must be refused.` |
|      - |  347 | ` */` |
|    200 |  348 | `static int IniSessionLocked(ph7_context *pCtx,VmIniSlot *pSlot,const char *zFunc)` |
|      5 |  349 | `{` |
|    205 |  350 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  351 | `	char zMsg[160];` |
|      - |  352 | `	const char *zWhy;` |
|    200 |  353 | `	if( pSlot->sName.nByte < sizeof("session.")-1` |
|    205 |  354 | `	 \|\| SyMemcmp(pSlot->sName.zString,"session.",sizeof("session.")-1) != 0 ){` |
|     53 |  355 | `		return 0;` |
|      - |  356 | `	}` |
|    156 |  357 | `	if( pVm->iSessStatus == 2 /* PHP_SESSION_ACTIVE */ ){` |
|    ! 0 |  358 | `		zWhy = "when a session is active";` |
|    156 |  359 | `	}else if( pVm->bHeadersSent ){` |
|    ! 0 |  360 | `		zWhy = "after headers have already been sent";` |
|    ! 0 |  361 | `	}else{` |
|    156 |  362 | `		return 0;` |
|      - |  363 | `	}` |
|    ! 0 |  364 | `	SyBufferFormat(zMsg,sizeof(zMsg),` |
|    ! 0 |  365 | `		"%s(): Session ini settings cannot be changed %s",zFunc,zWhy);` |
|    ! 0 |  366 | `	PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);` |
|    ! 0 |  367 | `	return 1;` |
|    105 |  368 | `}` |
|      - |  369 | `/*` |
|      - |  370 | ` * The per-directive value rules php enforces on every write, and the diagnostic` |
|      - |  371 | ` * each one raises. zWho is the whole prefix php puts on it -- "ini_set()" or, when` |
|      - |  372 | ` * session_start() is applying its $options array, "session_start()" -- because php` |
|      - |  373 | ` * blames the call that made the write, not the API underneath it.` |
|      - |  374 | ` */` |
|    266 |  375 | `static int IniValueAccepted(ph7_vm *pVm,VmIniSlot *pSlot,const char *zVal,sxu32 nVal,` |
|      - |  376 | `	const char *zWho)` |
|      5 |  377 | `{` |
|      - |  378 | `	char zMsg[256];` |
|    271 |  379 | `	if( IniNameIs(pSlot,"include_path") && nVal < 1 ){` |
|      - |  380 | `		/* php registers include_path with OnUpdateStringUnempty: the EMPTY value` |
|      - |  381 | `		 * is refused in silence and the directive keeps what it had. */` |
|      3 |  382 | `		return 0;` |
|      - |  383 | `	}` |
|    269 |  384 | `	if( IniNameIs(pSlot,"bcmath.scale") ){` |
|      - |  385 | `		/* php registers it with a 0..INT_MAX bound and refuses anything outside in` |
|      - |  386 | `` 		 * silence, keeping what the directive had -- `ini_set('bcmath.scale','-1')` `` |
|      - |  387 | `		 * is false there. A NON-numeric value is a different matter and is` |
|      - |  388 | `		 * ACCEPTED (stored verbatim, read back as 0), which falls out of the parse` |
|      - |  389 | `		 * below without a rule of its own. */` |
|     25 |  390 | `		sxi64 iVal = 0;` |
|     25 |  391 | `		SyStrToInt64(zVal,nVal,(void *)&iVal,0);` |
|     25 |  392 | `		if( iVal < 0 \|\| iVal > 2147483647 ){` |
|      5 |  393 | `			return 0;` |
|      - |  394 | `		}` |
|     10 |  395 | `	}` |
|    260 |  396 | `	if( IniNameIs(pSlot,"session.serialize_handler")` |
|    137 |  397 | `	 && !(nVal == 3 && SyMemcmp(zVal,"php",3) == 0)` |
|      8 |  398 | `	 && !(nVal == 10 && SyMemcmp(zVal,"php_binary",10) == 0)` |
|      8 |  399 | `	 && !(nVal == 13 && SyMemcmp(zVal,"php_serialize",13) == 0) ){` |
|      - |  400 | `		/* php looks the name up in its registered serializer list and refuses what` |
|      - |  401 | `		 * it cannot find, keeping the directive where it was. */` |
|      4 |  402 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|      1 |  403 | `			"%s: Serialization handler \"%.*s\" cannot be found",zWho,(int)nVal,zVal);` |
|      3 |  404 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);` |
|      3 |  405 | `		return 0;` |
|      - |  406 | `	}` |
|    259 |  407 | `	if( IniNameIs(pSlot,"session.name") ){` |
|      - |  408 | `		/* The name goes out as a COOKIE name and comes back as one, so php holds it` |
|      - |  409 | `		 * to the cookie alphabet -- and refuses a numeric one, which a browser would` |
|      - |  410 | `		 * hand back as an integer array key. */` |
|      - |  411 | `		static const char zBad[] = "=,;.[ \t\r\n\013\014";` |
|      - |  412 | `		sxu32 i;` |
|     32 |  413 | `		int bBad = nVal < 1;` |
|    222 |  414 | `		for( i = 0 ; !bBad && i < nVal ; i++ ){` |
|    192 |  415 | `			if( zVal[i] == 0 \|\| SyByteFind(zBad,sizeof(zBad)-1,zVal[i],0) == SXRET_OK ){` |
|      3 |  416 | `				bBad = 1;` |
|      1 |  417 | `			}` |
|     97 |  418 | `		}` |
|     32 |  419 | `		if( !bBad ){` |
|     28 |  420 | `			sxi64 iDummy = 0;` |
|     28 |  421 | `			bBad = SyStrToInt64(zVal,nVal,(void *)&iDummy,0) == SXRET_OK;` |
|     13 |  422 | `		}` |
|     32 |  423 | `		if( bBad ){` |
|     10 |  424 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - |  425 | `				"%s: session.name \"%.*s\" must not be numeric, empty, contain null bytes"` |
|      - |  426 | `				" or any of the following characters \"=,;.[ \\t\\r\\n\\013\\014\"",` |
|      3 |  427 | `				zWho,(int)nVal,zVal);` |
|      7 |  428 | `			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);` |
|      7 |  429 | `			return 0;` |
|      - |  430 | `		}` |
|     12 |  431 | `	}` |
|    253 |  432 | `	return 1;` |
|    136 |  433 | `}` |
|      - |  434 | `/*` |
|      - |  435 | ` * Write a directive from C, the way ini_set() writes it. Answers 0 when the write` |
|      - |  436 | ` * was refused (unknown name, not user-settable, or a value the directive's own` |
|      - |  437 | ` * rule rejects) -- which is exactly what session_start()'s $options reports as` |
|      - |  438 | `` * `Setting option "%s" failed`.`` |
|      - |  439 | ` */` |
|     68 |  440 | `PH7_PRIVATE int PH7_VmIniSet(ph7_vm *pVm,const char *zName,sxu32 nName,` |
|      - |  441 | `	const char *zVal,sxu32 nVal,const char *zWho)` |
|      5 |  442 | `{` |
|      - |  443 | `	VmIniSlot *pSlot;` |
|     73 |  444 | `	if( IniSeed(pVm) != SXRET_OK ){` |
|    ! 0 |  445 | `		return 0;` |
|      - |  446 | `	}` |
|     73 |  447 | `	pSlot = IniFind(pVm,zName,nName);` |
|     73 |  448 | `	if( pSlot == 0 \|\| (pSlot->iAccess & VM_INI_USER) == 0 ){` |
|      3 |  449 | `		return 0;` |
|      - |  450 | `	}` |
|     71 |  451 | `	if( !IniValueAccepted(pVm,pSlot,zVal,nVal,zWho) ){` |
|      7 |  452 | `		return 0;` |
|      - |  453 | `	}` |
|     65 |  454 | `	SyBlobReset(&pSlot->sLocal);` |
|     65 |  455 | `	SyBlobAppend(&pSlot->sLocal,zVal,nVal);` |
|     65 |  456 | `	IniLiveSet(pVm,pSlot,zVal,nVal);` |
|     65 |  457 | `	return 1;` |
|     39 |  458 | `}` |
|      - |  459 | `/* string\|false ini_get(string $option) */` |
|    120 |  460 | `static int vm_builtin_ini_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  461 | `{` |
|    125 |  462 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  463 | `	VmIniSlot *pSlot;` |
|      - |  464 | `	const char *zName;` |
|    125 |  465 | `	int nName = 0;` |
|      - |  466 | `	SyBlob sOut;` |
|    125 |  467 | `	if( nArg < 1 \|\| IniSeed(pVm) != SXRET_OK ){` |
|    ! 0 |  468 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  469 | `		return PH7_OK;` |
|      - |  470 | `	}` |
|    125 |  471 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|    125 |  472 | `	pSlot = IniFind(pVm,zName,(sxu32)nName);` |
|    125 |  473 | `	if( pSlot == 0 ){` |
|     13 |  474 | `		ph7_result_bool(pCtx,0);` |
|     13 |  475 | `		return PH7_OK;` |
|      - |  476 | `	}` |
|    115 |  477 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|    115 |  478 | `	IniLiveGet(pVm,pSlot,&sOut);` |
|    115 |  479 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    115 |  480 | `	SyBlobRelease(&sOut);` |
|    115 |  481 | `	return PH7_OK;` |
|     65 |  482 | `}` |
|      - |  483 | `/* string\|false ini_set(string $option, string\|int\|float\|bool\|null $value) */` |
|    200 |  484 | `static int vm_builtin_ini_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  485 | `{` |
|    205 |  486 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  487 | `	VmIniSlot *pSlot;` |
|      - |  488 | `	const char *zName;` |
|      - |  489 | `	const char *zVal;` |
|    205 |  490 | `	int nName = 0, nVal = 0;` |
|      - |  491 | `	SyBlob sOld;` |
|    205 |  492 | `	if( nArg < 2 \|\| IniSeed(pVm) != SXRET_OK ){` |
|    ! 0 |  493 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  494 | `		return PH7_OK;` |
|      - |  495 | `	}` |
|    205 |  496 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|    205 |  497 | `	pSlot = IniFind(pVm,zName,(sxu32)nName);` |
|    205 |  498 | `	if( pSlot == 0 \|\| (pSlot->iAccess & VM_INI_USER) == 0 ){` |
|      5 |  499 | `		ph7_result_bool(pCtx,0);` |
|      5 |  500 | `		return PH7_OK;` |
|      - |  501 | `	}` |
|    201 |  502 | `	if( IniSessionLocked(pCtx,pSlot,"ini_set") ){` |
|    ! 0 |  503 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  504 | `		return PH7_OK;` |
|      - |  505 | `	}` |
|      - |  506 | `	/* php: the -1 (compiled-out) state of zend.assertions is a php.ini-only switch,` |
|      - |  507 | `	 * and so is moving INTO it. Unprefixed warning, exactly as php prints it. */` |
|    201 |  508 | `	if( IniNameIs(pSlot,"zend.assertions") ){` |
|    ! 0 |  509 | `		int bGlobalOff = SyBlobLength(&pSlot->sGlobal) == 2` |
|    ! 0 |  510 | `			&& SyMemcmp(SyBlobData(&pSlot->sGlobal),"-1",2) == 0;` |
|    ! 0 |  511 | `		const char *zNew = ph7_value_to_string(apArg[1],&nVal);` |
|    ! 0 |  512 | `		int bNewOff = nVal == 2 && SyMemcmp(zNew,"-1",2) == 0;` |
|    ! 0 |  513 | `		if( bGlobalOff \|\| bNewOff ){` |
|    ! 0 |  514 | `			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|      - |  515 | `				"zend.assertions may be completely enabled or disabled only in php.ini");` |
|    ! 0 |  516 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 |  517 | `			return PH7_OK;` |
|      - |  518 | `		}` |
|    ! 0 |  519 | `	}` |
|      - |  520 | `	/* The OLD value php answers is the effective one, read before the write. */` |
|    201 |  521 | `	SyBlobInit(&sOld,&pVm->sAllocator);` |
|    201 |  522 | `	IniLiveGet(pVm,pSlot,&sOld);` |
|      - |  523 | `	/* php stringifies the incoming value, with a bool becoming "1"/"" . */` |
|    201 |  524 | `	if( ph7_value_is_bool(apArg[1]) ){` |
|    ! 0 |  525 | `		zVal = ph7_value_to_bool(apArg[1]) ? "1" : "";` |
|    ! 0 |  526 | `		nVal = (int)SyStrlen(zVal);` |
|    ! 0 |  527 | `	}else{` |
|    201 |  528 | `		zVal = ph7_value_to_string(apArg[1],&nVal);` |
|      - |  529 | `	}` |
|    201 |  530 | `	if( !IniValueAccepted(pVm,pSlot,zVal,(sxu32)nVal,"ini_set()") ){` |
|     11 |  531 | `		SyBlobRelease(&sOld);` |
|     11 |  532 | `		ph7_result_bool(pCtx,0);` |
|     11 |  533 | `		return PH7_OK;` |
|      - |  534 | `	}` |
|    193 |  535 | `	SyBlobReset(&pSlot->sLocal);` |
|    193 |  536 | `	SyBlobAppend(&pSlot->sLocal,zVal,(sxu32)nVal);` |
|    193 |  537 | `	IniLiveSet(pVm,pSlot,zVal,(sxu32)nVal);` |
|    193 |  538 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOld),(int)SyBlobLength(&sOld));` |
|    193 |  539 | `	SyBlobRelease(&sOld);` |
|    193 |  540 | `	return PH7_OK;` |
|    105 |  541 | `}` |
|      - |  542 | `/* void ini_restore(string $option) */` |
|      4 |  543 | `static int vm_builtin_ini_restore(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  544 | `{` |
|      5 |  545 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  546 | `	VmIniSlot *pSlot;` |
|      - |  547 | `	const char *zName;` |
|      5 |  548 | `	int nName = 0;` |
|      5 |  549 | `	ph7_result_null(pCtx);` |
|      5 |  550 | `	if( nArg < 1 \|\| IniSeed(pVm) != SXRET_OK ){` |
|    ! 0 |  551 | `		return PH7_OK;` |
|      - |  552 | `	}` |
|      5 |  553 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|      5 |  554 | `	pSlot = IniFind(pVm,zName,(sxu32)nName);` |
|      5 |  555 | `	if( pSlot == 0 \|\| IniSessionLocked(pCtx,pSlot,"ini_restore") ){` |
|    ! 0 |  556 | `		return PH7_OK;` |
|      - |  557 | `	}` |
|      5 |  558 | `	SyBlobReset(&pSlot->sLocal);` |
|      5 |  559 | `	SyBlobAppend(&pSlot->sLocal,SyBlobData(&pSlot->sGlobal),SyBlobLength(&pSlot->sGlobal));` |
|      5 |  560 | `	IniLiveSet(pVm,pSlot,(const char *)SyBlobData(&pSlot->sGlobal),SyBlobLength(&pSlot->sGlobal));` |
|      5 |  561 | `	return PH7_OK;` |
|      3 |  562 | `}` |
|      - |  563 | `/* array\|false ini_get_all(?string $extension = null, bool $details = true) */` |
|      8 |  564 | `static int vm_builtin_ini_get_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  565 | `{` |
|     10 |  566 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  567 | `	VmIniSlot *aSlot;` |
|      - |  568 | `	ph7_value *pOut,*pCur;` |
|     10 |  569 | `	const char *zExt = 0;` |
|     10 |  570 | `	int nExt = 0, bDetails = 1;` |
|      - |  571 | `	sxu32 n;` |
|      - |  572 | `	SyBlob sVal;` |
|     10 |  573 | `	if( IniSeed(pVm) != SXRET_OK ){` |
|    ! 0 |  574 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  575 | `		return PH7_OK;` |
|      - |  576 | `	}` |
|     10 |  577 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|      8 |  578 | `		zExt = ph7_value_to_string(apArg[0],&nExt);` |
|      3 |  579 | `	}` |
|     10 |  580 | `	if( nArg > 1 ){` |
|      3 |  581 | `		bDetails = ph7_value_to_bool(apArg[1]);` |
|      1 |  582 | `	}` |
|     10 |  583 | `	if( zExt ){` |
|      - |  584 | `		/* php reports the extensions it knows; anything else is a warning + false. */` |
|      - |  585 | `		static const char *azKnown[] = { "Core", "session", "date", "standard",` |
|      - |  586 | `			"bcmath" };` |
|      8 |  587 | `		int bKnown = 0;` |
|      - |  588 | `		sxu32 k;` |
|     26 |  589 | `		for( k = 0 ; k < SX_ARRAYSIZE(azKnown) ; k++ ){` |
|     26 |  590 | `			if( (int)SyStrlen(azKnown[k]) == nExt && SyMemcmp(azKnown[k],zExt,(sxu32)nExt) == 0 ){` |
|      8 |  591 | `				bKnown = 1;` |
|      8 |  592 | `				break;` |
|      - |  593 | `			}` |
|     11 |  594 | `		}` |
|      8 |  595 | `		if( !bKnown ){` |
|    ! 0 |  596 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 |  597 | `				"Extension \"%.*s\" cannot be found",nExt,zExt);` |
|    ! 0 |  598 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 |  599 | `			return PH7_OK;` |
|      - |  600 | `		}` |
|      3 |  601 | `	}` |
|     10 |  602 | `	pOut = ph7_context_new_array(pCtx);` |
|     10 |  603 | `	pCur = ph7_context_new_scalar(pCtx);` |
|     10 |  604 | `	if( pOut == 0 \|\| pCur == 0 ){` |
|    ! 0 |  605 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  606 | `	}` |
|     10 |  607 | `	SyBlobInit(&sVal,&pVm->sAllocator);` |
|     10 |  608 | `	aSlot = (VmIniSlot *)SySetBasePtr(&pVm->aIniTab);` |
|      - |  609 | `	/* The table is stored sorted, so this walk is already php's ksort order. */` |
|    434 |  610 | `	for( n = 0 ; n < SySetUsed(&pVm->aIniTab) ; n++ ){` |
|    426 |  611 | `		VmIniSlot *pSlot = &aSlot[n];` |
|      - |  612 | `		char zKey[128];` |
|    426 |  613 | `		if( zExt ){` |
|    320 |  614 | `			int bCore = (nExt == 4 && SyMemcmp(zExt,"Core",4) == 0)` |
|    318 |  615 | `				\|\| (nExt == 8 && SyMemcmp(zExt,"standard",8) == 0);` |
|    320 |  616 | `			if( !bCore ){` |
|      - |  617 | `				/* A named extension keeps only its own "<ext>." prefix. */` |
|    318 |  618 | `				if( pSlot->sName.nByte <= (sxu32)nExt` |
|    318 |  619 | `				 \|\| SyMemcmp(pSlot->sName.zString,zExt,(sxu32)nExt) != 0` |
|    184 |  620 | `				 \|\| pSlot->sName.zString[nExt] != '.' ){` |
|    274 |  621 | `					continue;` |
|      - |  622 | `				}` |
|     25 |  623 | `			}else{` |
|      - |  624 | `				/* Core/standard exclude the directives owned by a named extension. */` |
|    ! 0 |  625 | `				if( (pSlot->sName.nByte > sizeof("session.")-1` |
|    ! 0 |  626 | `				  && SyMemcmp(pSlot->sName.zString,"session.",sizeof("session.")-1) == 0)` |
|    ! 0 |  627 | `				 \|\| (pSlot->sName.nByte > sizeof("date.")-1` |
|    ! 0 |  628 | `				  && SyMemcmp(pSlot->sName.zString,"date.",sizeof("date.")-1) == 0)` |
|    ! 0 |  629 | `				 \|\| (pSlot->sName.nByte > sizeof("bcmath.")-1` |
|    ! 0 |  630 | `				  && SyMemcmp(pSlot->sName.zString,"bcmath.",sizeof("bcmath.")-1) == 0) ){` |
|    ! 0 |  631 | `					continue;` |
|      - |  632 | `				}` |
|      - |  633 | `			}` |
|     23 |  634 | `		}` |
|    154 |  635 | `		if( pSlot->sName.nByte >= sizeof(zKey) ){` |
|    ! 0 |  636 | `			continue;` |
|      - |  637 | `		}` |
|    154 |  638 | `		SyMemcpy(pSlot->sName.zString,zKey,pSlot->sName.nByte);` |
|    154 |  639 | `		zKey[pSlot->sName.nByte] = 0;` |
|    154 |  640 | `		IniLiveGet(pVm,pSlot,&sVal);` |
|    154 |  641 | `		if( bDetails ){` |
|     48 |  642 | `			ph7_value *pRow = ph7_context_new_array(pCtx);` |
|     48 |  643 | `			if( pRow == 0 ){` |
|    ! 0 |  644 | `				break;` |
|      - |  645 | `			}` |
|     71 |  646 | `			ph7_value_string(pCur,(const char *)SyBlobData(&pSlot->sGlobal),` |
|     46 |  647 | `				(int)SyBlobLength(&pSlot->sGlobal));` |
|     48 |  648 | `			ph7_array_add_strkey_elem(pRow,"global_value",pCur);` |
|     48 |  649 | `			ph7_value_reset_string_cursor(pCur);` |
|     48 |  650 | `			ph7_value_string(pCur,(const char *)SyBlobData(&sVal),(int)SyBlobLength(&sVal));` |
|     48 |  651 | `			ph7_array_add_strkey_elem(pRow,"local_value",pCur);` |
|     48 |  652 | `			ph7_value_reset_string_cursor(pCur);` |
|     48 |  653 | `			ph7_value_int(pCur,pSlot->iAccess);` |
|     48 |  654 | `			ph7_array_add_strkey_elem(pRow,"access",pCur);` |
|     48 |  655 | `			ph7_value_reset_string_cursor(pCur);` |
|     48 |  656 | `			ph7_array_add_strkey_elem(pOut,zKey,pRow);` |
|     25 |  657 | `		}else{` |
|    107 |  658 | `			ph7_value_reset_string_cursor(pCur);` |
|    107 |  659 | `			ph7_value_string(pCur,(const char *)SyBlobData(&sVal),(int)SyBlobLength(&sVal));` |
|    107 |  660 | `			ph7_array_add_strkey_elem(pOut,zKey,pCur);` |
|    107 |  661 | `			ph7_value_reset_string_cursor(pCur);` |
|      - |  662 | `		}` |
|     78 |  663 | `	}` |
|     10 |  664 | `	SyBlobRelease(&sVal);` |
|     10 |  665 | `	ph7_result_value(pCtx,pOut);` |
|     10 |  666 | `	return PH7_OK;` |
|      6 |  667 | `}` |
|      - |  668 | `/* string\|false get_cfg_var(string $option) — php answers the GLOBAL value */` |
|      8 |  669 | `static int vm_builtin_get_cfg_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  670 | `{` |
|      9 |  671 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  672 | `	VmIniSlot *pSlot;` |
|      - |  673 | `	const char *zName;` |
|      9 |  674 | `	int nName = 0;` |
|      9 |  675 | `	if( nArg < 1 \|\| IniSeed(pVm) != SXRET_OK ){` |
|    ! 0 |  676 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  677 | `		return PH7_OK;` |
|      - |  678 | `	}` |
|      9 |  679 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|      9 |  680 | `	pSlot = IniFind(pVm,zName,(sxu32)nName);` |
|      9 |  681 | `	if( pSlot == 0 ){` |
|      3 |  682 | `		ph7_result_bool(pCtx,0);` |
|      3 |  683 | `		return PH7_OK;` |
|      - |  684 | `	}` |
|     10 |  685 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&pSlot->sGlobal),` |
|      6 |  686 | `		(int)SyBlobLength(&pSlot->sGlobal));` |
|      7 |  687 | `	return PH7_OK;` |
|      5 |  688 | `}` |
|      - |  689 | `/*` |
|      - |  690 | ` * The effective value of a directive a C builtin needs to obey, as an integer.` |
|      - |  691 | ` * parse_str() reads max_input_vars and max_input_nesting_level through this;` |
|      - |  692 | ` * without it a builtin would have to duplicate php's default and could never` |
|      - |  693 | `` * see an `ini_set()`/`-d` override. Answers iDefault when the directive is`` |
|      - |  694 | ` * absent or unparsable.` |
|      - |  695 | ` */` |
|   1342 |  696 | `PH7_PRIVATE sxi64 PH7_VmIniGetInt(ph7_vm *pVm,const char *zName,sxi64 iDefault)` |
|      5 |  697 | `{` |
|      - |  698 | `	VmIniSlot *pSlot;` |
|      - |  699 | `	SyBlob sVal;` |
|   1347 |  700 | `	sxi64 iVal = iDefault;` |
|   1347 |  701 | `	IniSeed(pVm);` |
|   1347 |  702 | `	pSlot = IniFind(pVm,zName,(sxu32)SyStrlen(zName));` |
|   1347 |  703 | `	if( pSlot == 0 ){` |
|    ! 0 |  704 | `		return iDefault;` |
|      - |  705 | `	}` |
|   1347 |  706 | `	SyBlobInit(&sVal,&pVm->sAllocator);` |
|   1347 |  707 | `	IniLiveGet(pVm,pSlot,&sVal);` |
|   1347 |  708 | `	if( SyBlobLength(&sVal) > 0 ){` |
|   1347 |  709 | `		SyStrToInt64((const char *)SyBlobData(&sVal),SyBlobLength(&sVal),(void *)&iVal,0);` |
|    671 |  710 | `	}` |
|   1347 |  711 | `	SyBlobRelease(&sVal);` |
|   1347 |  712 | `	return iVal;` |
|    676 |  713 | `}` |
|      - |  714 | `/* The same, as a borrowed STRING (arg_separator.input). The bytes live in the` |
|      - |  715 | ` * caller's blob, which it owns. */` |
|   1338 |  716 | `PH7_PRIVATE void PH7_VmIniGetStr(ph7_vm *pVm,const char *zName,SyBlob *pOut)` |
|      5 |  717 | `{` |
|      - |  718 | `	VmIniSlot *pSlot;` |
|   1343 |  719 | `	IniSeed(pVm);` |
|   1343 |  720 | `	SyBlobReset(pOut);` |
|   1343 |  721 | `	pSlot = IniFind(pVm,zName,(sxu32)SyStrlen(zName));` |
|   1343 |  722 | `	if( pSlot ){` |
|   1305 |  723 | `		IniLiveGet(pVm,pSlot,pOut);` |
|    650 |  724 | `	}` |
|   1343 |  725 | `}` |
|      - |  726 | `/*` |
|      - |  727 | ` * Read a BOOLEAN directive the way zend_ini does — "on"/"yes"/"true" as well as` |
|      - |  728 | ` * a non-zero number. Reading one through the integer parser answers 0 for` |
|      - |  729 | `` * `On`, which is the spelling php.ini-production ships, so a directive written`` |
|      - |  730 | ` * that way reads as OFF.` |
|      - |  731 | ` */` |
|    604 |  732 | `PH7_PRIVATE int PH7_VmIniGetBool(ph7_vm *pVm,const char *zName,int bDefault)` |
|      5 |  733 | `{` |
|      - |  734 | `	VmIniSlot *pSlot;` |
|      - |  735 | `	SyBlob sVal;` |
|      - |  736 | `	int bRes;` |
|    609 |  737 | `	IniSeed(pVm);` |
|    609 |  738 | `	pSlot = IniFind(pVm,zName,(sxu32)SyStrlen(zName));` |
|    609 |  739 | `	if( pSlot == 0 ){` |
|    ! 0 |  740 | `		return bDefault;` |
|      - |  741 | `	}` |
|    609 |  742 | `	SyBlobInit(&sVal,&pVm->sAllocator);` |
|    609 |  743 | `	IniLiveGet(pVm,pSlot,&sVal);` |
|    609 |  744 | `	bRes = IniTruthy((const char *)SyBlobData(&sVal),SyBlobLength(&sVal));` |
|    609 |  745 | `	SyBlobRelease(&sVal);` |
|    609 |  746 | `	return bRes;` |
|    307 |  747 | `}` |
|   5740 |  748 | `PH7_PRIVATE sxi32 PH7_VmInstallIni(ph7_vm *pVm)` |
|      5 |  749 | `{` |
|      - |  750 | `	static const struct {` |
|      - |  751 | `		const char *zName;` |
|      - |  752 | `		ProchHostFunction xFunc;` |
|      - |  753 | `	} aFunc[] = {` |
|      - |  754 | `		{ "ini_get",      vm_builtin_ini_get      },` |
|      - |  755 | `		{ "ini_set",      vm_builtin_ini_set      },` |
|      - |  756 | `		{ "ini_restore",  vm_builtin_ini_restore  },` |
|      - |  757 | `		{ "ini_get_all",  vm_builtin_ini_get_all  },` |
|      - |  758 | `		{ "get_cfg_var",  vm_builtin_get_cfg_var  },` |
|      - |  759 | `	};` |
|      - |  760 | `	sxu32 n;` |
|  34445 |  761 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFunc) ; n++ ){` |
|  28705 |  762 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
|  14355 |  763 | `	}` |
|   5745 |  764 | `	return SXRET_OK;` |
|      5 |  765 | `}` |
|      - |  766 | `#else` |
|      - |  767 | `PH7_PRIVATE sxi32 PH7_VmInstallIni(ph7_vm *pVm){ (void)pVm; return SXRET_OK; }` |
|      - |  768 | `PH7_PRIVATE sxi64 PH7_VmIniGetInt(ph7_vm *pVm,const char *zName,sxi64 iDefault){` |
|      - |  769 | `	(void)pVm; (void)zName; return iDefault;` |
|      - |  770 | `}` |
|      - |  771 | `PH7_PRIVATE int PH7_VmIniGetBool(ph7_vm *pVm,const char *zName,int bDefault){` |
|      - |  772 | `	(void)pVm; (void)zName; return bDefault;` |
|      - |  773 | `}` |
|      - |  774 | `PH7_PRIVATE void PH7_VmIniGetStr(ph7_vm *pVm,const char *zName,SyBlob *pOut){` |
|      - |  775 | `	(void)pVm; (void)zName; SyBlobReset(pOut);` |
|      - |  776 | `}` |
|      - |  777 | `PH7_PRIVATE int PH7_VmIniSet(ph7_vm *pVm,const char *zName,sxu32 nName,` |
|      - |  778 | `	const char *zVal,sxu32 nVal,const char *zWho){` |
|      - |  779 | `	(void)pVm; (void)zName; (void)nName; (void)zVal; (void)nVal; (void)zWho; return 0;` |
|      - |  780 | `}` |
|      - |  781 | `#endif` |
|      - |  782 |  |
