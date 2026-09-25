# src/ph7/vm_builtin_ini.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 375/442 lines (84.84%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    4 | ` */` |
|     - |    5 | `#include "ph7int.h"` |
|     - |    6 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|     - |    7 | `/*` |
|     - |    8 | ` * php.ini subsystem + INI API: a lazily seeded directive table (the static` |
|     - |    9 | ` * defaults merged with the CLI's -d/-c entries from pVm->aIniCli) behind` |
|     - |   10 | ` * ini_get / ini_set / ini_restore / ini_get_all / get_cfg_var. Live-wired` |
|     - |   11 | ` * directives dispatch to the real knobs (error_reporting(), the session state,` |
|     - |   12 | ` * the default timezone, the diagnostic gates) so the INI view and the engine` |
|     - |   13 | ` * agree.` |
|     - |   14 | ` *` |
|     - |   15 | `` * This was an embedded-PHP chunk over two `__ini_*` C thunks, with the table`` |
|     - |   16 | `` * parked on a private `__IniS` class and five `__ini_*` PHP helpers alongside.`` |
|     - |   17 | ` * All eight of those names are gone: the table is a SySet on the VM and the five` |
|     - |   18 | ` * functions ARE these C routines. The thunks did not become methods -- there is` |
|     - |   19 | ` * no class here, only php's global functions -- so, like libxml's, they collapse` |
|     - |   20 | ` * into the functions they were serving.` |
|     - |   21 | ` *` |
|     - |   22 | ` * One thing the move fixes on its own: a diagnostic raised by a prelude function` |
|     - |   23 | ` * reported the CHUNK's line, so every ini_set()/ini_get_all() warning said` |
|     - |   24 | ` * "on line 1" regardless of the caller. A C builtin reports the caller's line,` |
|     - |   25 | ` * which is what php prints.` |
|     - |   26 | ` */` |
|     - |   27 |  |
|     - |   28 | `/* Directive access levels, as php reports them in ini_get_all()['access']. */` |
|     - |   29 | `#define VM_INI_USER    1` |
|     - |   30 | `#define VM_INI_PERDIR  2` |
|     - |   31 | `#define VM_INI_SYSTEM  4` |
|     - |   32 | `#define VM_INI_ALL     (VM_INI_USER\|VM_INI_PERDIR\|VM_INI_SYSTEM)` |
|     - |   33 |  |
|     - |   34 | `/*` |
|     - |   35 | ` * The defaults, in the order php's ini_get_all() reports them (sorted by name).` |
|     - |   36 | ` * Keeping this list sorted is what lets ini_get_all() skip a sort: the seed` |
|     - |   37 | ` * inserts any CLI-only directive in its sorted position.` |
|     - |   38 | ` */` |
|     - |   39 | `static const struct {` |
|     - |   40 | `	const char *zName;` |
|     - |   41 | `	const char *zValue;` |
|     - |   42 | `	sxi32 iAccess;` |
|     - |   43 | `} aIniDefault[] = {` |
|     - |   44 | `	{ "allow_url_fopen",          "1",          VM_INI_PERDIR\|VM_INI_SYSTEM },` |
|     - |   45 | `	/* php's default is OFF, and it spells that default as the EMPTY string — which` |
|     - |   46 | `	 * is what ini_get() answers. Including a remote file is the classic RFI, and` |
|     - |   47 | `	 * this is what a STREAM_IS_URL wrapper's include is gated on. */` |
|     - |   48 | `	{ "allow_url_include",        "",           VM_INI_SYSTEM },` |
|     - |   49 | `	{ "arg_separator.input",      "&",          VM_INI_ALL },` |
|     - |   50 | `	{ "arg_separator.output",     "&",          VM_INI_ALL },` |
|     - |   51 | `	{ "auto_detect_line_endings", "",           VM_INI_ALL },` |
|     - |   52 | `	{ "date.timezone",            "UTC",        VM_INI_ALL },` |
|     - |   53 | `	{ "default_charset",          "UTF-8",      VM_INI_ALL },` |
|     - |   54 | `	/* php bounds a socket wait by this rather than waiting forever, and it is` |
|     - |   55 | `	 * where stream_socket_accept() takes its default timeout from. */` |
|     - |   56 | `	{ "default_socket_timeout",   "60",         VM_INI_ALL },` |
|     - |   57 | `	{ "default_mimetype",         "text/html",  VM_INI_ALL },` |
|     - |   58 | `	{ "display_errors",           "",           VM_INI_ALL },` |
|     - |   59 | `	{ "error_log",                "",           VM_INI_ALL },` |
|     - |   60 | `	{ "error_reporting",          "30719",      VM_INI_ALL },` |
|     - |   61 | `	{ "highlight.comment",        "#FF8000",    VM_INI_ALL },` |
|     - |   62 | `	{ "highlight.default",        "#0000BB",    VM_INI_ALL },` |
|     - |   63 | `	{ "highlight.html",           "#000000",    VM_INI_ALL },` |
|     - |   64 | `	{ "highlight.keyword",        "#007700",    VM_INI_ALL },` |
|     - |   65 | `	{ "highlight.string",         "#DD0000",    VM_INI_ALL },` |
|     - |   66 | `	{ "include_path",             ".",          VM_INI_ALL },` |
|     - |   67 | `	{ "log_errors",               "1",          VM_INI_ALL },` |
|     - |   68 | `	{ "max_execution_time",       "0",          VM_INI_ALL },` |
|     - |   69 | `	{ "max_input_nesting_level",  "64",         VM_INI_PERDIR\|VM_INI_SYSTEM },` |
|     - |   70 | `	{ "max_input_vars",           "1000",       VM_INI_PERDIR\|VM_INI_SYSTEM },` |
|     - |   71 | `	{ "memory_limit",             "-1",         VM_INI_ALL },` |
|     - |   72 | `	{ "post_max_size",            "8M",         VM_INI_PERDIR\|VM_INI_SYSTEM },` |
|     - |   73 | `	{ "precision",                "14",         VM_INI_ALL },` |
|     - |   74 | `	{ "serialize_precision",      "-1",         VM_INI_ALL },` |
|     - |   75 | `	/* php's session directives, the whole non-deprecated set: session_start()'s` |
|     - |   76 | `	 * $options array applies its keys THROUGH this table, so a directive missing` |
|     - |   77 | `	 * here is an option php accepts and PHL reports as failed.` |
|     - |   78 | `	 * Six are absent on purpose: php 8.4 DEPRECATES session.sid_length,` |
|     - |   79 | `	 * session.sid_bits_per_character, session.referer_check, session.use_trans_sid,` |
|     - |   80 | `	 * session.trans_sid_tags and session.trans_sid_hosts, and §10 does not carry` |
|     - |   81 | `	 * php's deprecated surface. The session.upload_progress.* family goes with the` |
|     - |   82 | ``	 * file uploads §10 excludes from a CLI-plus-`-S` engine. */`` |
|     - |   83 | `	{ "session.auto_start",       "0",          VM_INI_PERDIR\|VM_INI_SYSTEM },` |
|     - |   84 | `	{ "session.cache_expire",     "180",        VM_INI_ALL },` |
|     - |   85 | `	{ "session.cache_limiter",    "nocache",    VM_INI_ALL },` |
|     - |   86 | `	/* The Set-Cookie the session sends is built out of these seven. */` |
|     - |   87 | `	{ "session.cookie_domain",    "",           VM_INI_ALL },` |
|     - |   88 | `	{ "session.cookie_httponly",  "0",          VM_INI_ALL },` |
|     - |   89 | `	{ "session.cookie_lifetime",  "0",          VM_INI_ALL },` |
|     - |   90 | `	{ "session.cookie_partitioned","0",         VM_INI_ALL },` |
|     - |   91 | `	{ "session.cookie_path",      "/",          VM_INI_ALL },` |
|     - |   92 | `	{ "session.cookie_samesite",  "",           VM_INI_ALL },` |
|     - |   93 | `	{ "session.cookie_secure",    "0",          VM_INI_ALL },` |
|     - |   94 | `	{ "session.gc_divisor",       "100",        VM_INI_ALL },` |
|     - |   95 | `	{ "session.gc_maxlifetime",   "1440",       VM_INI_ALL },` |
|     - |   96 | `	{ "session.gc_probability",   "1",          VM_INI_ALL },` |
|     - |   97 | `	{ "session.lazy_write",       "1",          VM_INI_ALL },` |
|     - |   98 | `	{ "session.name",             "PHPSESSID",  VM_INI_ALL },` |
|     - |   99 | `	{ "session.save_handler",     "files",      VM_INI_ALL },` |
|     - |  100 | `	{ "session.save_path",        "",           VM_INI_ALL },` |
|     - |  101 | ``	/* Which of php's three session serializers writes the store: `php` (the`` |
|     - |  102 | ``	 * `name\|<serialized>` runs a stock php install reads), `php_binary` or`` |
|     - |  103 | ``	 * `php_serialize`. */`` |
|     - |  104 | `	{ "session.serialize_handler","php",        VM_INI_ALL },` |
|     - |  105 | `	/* Whether the session sends and reads its id as a cookie at all. PHL has never` |
|     - |  106 | `	 * read an id from anywhere ELSE, which is what use_only_cookies means. */` |
|     - |  107 | `	{ "session.use_cookies",      "1",          VM_INI_ALL },` |
|     - |  108 | `	{ "session.use_only_cookies", "1",          VM_INI_ALL },` |
|     - |  109 | `	{ "session.use_strict_mode",  "0",          VM_INI_ALL },` |
|     - |  110 | `	{ "short_open_tag",           "",           VM_INI_PERDIR\|VM_INI_SYSTEM },` |
|     - |  111 | `	{ "unserialize_callback_func","",           VM_INI_ALL },` |
|     - |  112 | `	{ "unserialize_max_depth",    "4096",       VM_INI_ALL },` |
|     - |  113 | `	{ "upload_max_filesize",      "2M",         VM_INI_PERDIR\|VM_INI_SYSTEM },` |
|     - |  114 | `	{ "zend.assertions",          "-1",         VM_INI_ALL },` |
|     - |  115 | `};` |
|     - |  116 |  |
| 22948 |  117 | `static int IniNameIs(const VmIniSlot *pSlot,const char *zName)` |
|     5 |  118 | `{` |
| 22953 |  119 | `	sxu32 n = (sxu32)SyStrlen(zName);` |
| 22953 |  120 | `	return pSlot->sName.nByte == n && SyMemcmp(pSlot->sName.zString,zName,n) == 0;` |
|     5 |  121 | `}` |
|     - |  122 | `/*` |
|     - |  123 | ` * zend_ini_parse_bool semantics, matching the C-side VmIniBool the -d/-c path` |
|     - |  124 | ` * uses: on/yes/true, else a non-zero integer parse.` |
|     - |  125 | ` */` |
|   598 |  126 | `static int IniTruthy(const char *zVal,sxu32 nVal)` |
|     5 |  127 | `{` |
|   900 |  128 | `	while( nVal > 0 && (zVal[0] == ' ' \|\| zVal[0] == '\t') ){ zVal++; nVal--; }` |
|   900 |  129 | `	while( nVal > 0 && (zVal[nVal-1] == ' ' \|\| zVal[nVal-1] == '\t') ){ nVal--; }` |
|   603 |  130 | `	if( nVal == 2 && SyStrnicmp(zVal,"on",2) == 0 ){ return 1; }` |
|   603 |  131 | `	if( nVal == 3 && SyStrnicmp(zVal,"yes",3) == 0 ){ return 1; }` |
|   603 |  132 | `	if( nVal == 4 && SyStrnicmp(zVal,"true",4) == 0 ){ return 1; }` |
|     - |  133 | `	{` |
|   603 |  134 | `		sxi32 iVal = 0;` |
|   603 |  135 | `		if( nVal > 0 && SyStrToInt32(zVal,nVal,(void *)&iVal,0) == SXRET_OK ){` |
|   599 |  136 | `			return iVal != 0;` |
|     - |  137 | `		}` |
|     - |  138 | `	}` |
|     5 |  139 | `	return 0;` |
|   304 |  140 | `}` |
|     - |  141 | `/*` |
|     - |  142 | ` * Build the table: the static defaults, then the CLI queue merged over them (an` |
|     - |  143 | ` * unknown CLI name is appended as a new INI_ALL directive, as the chunk did),` |
|     - |  144 | ` * then sorted by name so ini_get_all() can walk it in php's order without a sort.` |
|     - |  145 | ` */` |
|  2833 |  146 | `static sxi32 IniSeed(ph7_vm *pVm)` |
|     5 |  147 | `{` |
|     - |  148 | `	sxu32 i,j;` |
|     - |  149 | `	VmIniEntry *aCli;` |
|     - |  150 | `	VmIniSlot *aSlot;` |
|  2838 |  151 | `	if( pVm->bIniSeeded ){` |
|  2748 |  152 | `		return SXRET_OK;` |
|     - |  153 | `	}` |
|    95 |  154 | `	pVm->bIniSeeded = 1; /* set FIRST: the live-wired writes below re-enter nothing,` |
|     - |  155 | `	                      * but a future one must never recurse into the seed */` |
|  4775 |  156 | `	for( i = 0 ; i < SX_ARRAYSIZE(aIniDefault) ; i++ ){` |
|     - |  157 | `		VmIniSlot sSlot;` |
|  4685 |  158 | `		SyStringInitFromBuf(&sSlot.sName,aIniDefault[i].zName,SyStrlen(aIniDefault[i].zName));` |
|  4685 |  159 | `		sSlot.iAccess = aIniDefault[i].iAccess;` |
|  4685 |  160 | `		SyBlobInit(&sSlot.sGlobal,&pVm->sAllocator);` |
|  4685 |  161 | `		SyBlobInit(&sSlot.sLocal,&pVm->sAllocator);` |
|  4685 |  162 | `		SyBlobAppend(&sSlot.sGlobal,aIniDefault[i].zValue,(sxu32)SyStrlen(aIniDefault[i].zValue));` |
|  4685 |  163 | `		SyBlobAppend(&sSlot.sLocal,aIniDefault[i].zValue,(sxu32)SyStrlen(aIniDefault[i].zValue));` |
|  4685 |  164 | `		if( SySetPut(&pVm->aIniTab,(const void *)&sSlot) != SXRET_OK ){` |
|   ! 0 |  165 | `			return SXERR_MEM;` |
|     - |  166 | `		}` |
|  2345 |  167 | `	}` |
|    95 |  168 | `	aCli = (VmIniEntry *)SySetBasePtr(&pVm->aIniCli);` |
|   121 |  169 | `	for( i = 0 ; i < SySetUsed(&pVm->aIniCli) ; i++ ){` |
|    28 |  170 | `		int bFound = 0;` |
|    28 |  171 | `		aSlot = (VmIniSlot *)SySetBasePtr(&pVm->aIniTab);` |
|   732 |  172 | `		for( j = 0 ; j < SySetUsed(&pVm->aIniTab) ; j++ ){` |
|   728 |  173 | `			if( aSlot[j].sName.nByte == aCli[i].sName.nByte` |
|   393 |  174 | `			 && SyMemcmp(aSlot[j].sName.zString,aCli[i].sName.zString,aCli[i].sName.nByte) == 0 ){` |
|    26 |  175 | `				SyBlobReset(&aSlot[j].sGlobal);` |
|    26 |  176 | `				SyBlobReset(&aSlot[j].sLocal);` |
|    26 |  177 | `				SyBlobAppend(&aSlot[j].sGlobal,aCli[i].sValue.zString,aCli[i].sValue.nByte);` |
|    26 |  178 | `				SyBlobAppend(&aSlot[j].sLocal,aCli[i].sValue.zString,aCli[i].sValue.nByte);` |
|    26 |  179 | `				bFound = 1;` |
|    26 |  180 | `				break;` |
|     - |  181 | `			}` |
|   354 |  182 | `		}` |
|    28 |  183 | `		if( !bFound ){` |
|     - |  184 | `			VmIniSlot sSlot;` |
|     - |  185 | `			/* aIniCli holds VM-lifetime copies already, so the name can be aliased. */` |
|     3 |  186 | `			sSlot.sName = aCli[i].sName;` |
|     3 |  187 | `			sSlot.iAccess = VM_INI_ALL;` |
|     3 |  188 | `			SyBlobInit(&sSlot.sGlobal,&pVm->sAllocator);` |
|     3 |  189 | `			SyBlobInit(&sSlot.sLocal,&pVm->sAllocator);` |
|     3 |  190 | `			SyBlobAppend(&sSlot.sGlobal,aCli[i].sValue.zString,aCli[i].sValue.nByte);` |
|     3 |  191 | `			SyBlobAppend(&sSlot.sLocal,aCli[i].sValue.zString,aCli[i].sValue.nByte);` |
|     3 |  192 | `			if( SySetPut(&pVm->aIniTab,(const void *)&sSlot) != SXRET_OK ){` |
|   ! 0 |  193 | `				return SXERR_MEM;` |
|     - |  194 | `			}` |
|     1 |  195 | `		}` |
|    15 |  196 | `	}` |
|     - |  197 | `	/* Insertion sort by name (the table is ~30 entries and already nearly sorted:` |
|     - |  198 | `	 * only CLI-introduced directives are out of place). */` |
|    95 |  199 | `	aSlot = (VmIniSlot *)SySetBasePtr(&pVm->aIniTab);` |
|  4687 |  200 | `	for( i = 1 ; i < SySetUsed(&pVm->aIniTab) ; i++ ){` |
|  4597 |  201 | `		VmIniSlot sTmp = aSlot[i];` |
|  4597 |  202 | `		j = i;` |
|  4745 |  203 | `		while( j > 0 ){` |
|  4745 |  204 | `			const VmIniSlot *pPrev = &aSlot[j-1];` |
|  4745 |  205 | `			sxu32 nMin = pPrev->sName.nByte < sTmp.sName.nByte ? pPrev->sName.nByte : sTmp.sName.nByte;` |
|  4745 |  206 | `			sxi32 iCmp = SyMemcmp(pPrev->sName.zString,sTmp.sName.zString,nMin);` |
|  4745 |  207 | `			if( iCmp == 0 ){` |
|   ! 0 |  208 | `				iCmp = (sxi32)pPrev->sName.nByte - (sxi32)sTmp.sName.nByte;` |
|   ! 0 |  209 | `			}` |
|  4745 |  210 | `			if( iCmp <= 0 ){` |
|  4597 |  211 | `				break;` |
|     - |  212 | `			}` |
|   153 |  213 | `			aSlot[j] = aSlot[j-1];` |
|   153 |  214 | `			j--;` |
|     5 |  215 | `		}` |
|  4597 |  216 | `		aSlot[j] = sTmp;` |
|  2301 |  217 | `	}` |
|     - |  218 | `	/* Boot-apply the CLI values for the live-wired session knobs. The engine knobs` |
|     - |  219 | `	 * (error_reporting / date.timezone) were already applied C-side by` |
|     - |  220 | `	 * PH7_VM_CONFIG_INI_ENTRY. */` |
|    95 |  221 | `	aSlot = (VmIniSlot *)SySetBasePtr(&pVm->aIniTab);` |
|  4777 |  222 | `	for( i = 0 ; i < SySetUsed(&pVm->aIniTab) ; i++ ){` |
|  4687 |  223 | `		SyBlob *pDst = 0;` |
|  4687 |  224 | `		if( IniNameIs(&aSlot[i],"session.name") ){` |
|    90 |  225 | `			if( SyBlobLength(&aSlot[i].sGlobal) != sizeof("PHPSESSID")-1` |
|    93 |  226 | `			 \|\| SyMemcmp(SyBlobData(&aSlot[i].sGlobal),"PHPSESSID",sizeof("PHPSESSID")-1) != 0 ){` |
|     6 |  227 | `				pDst = &pVm->sSessName;` |
|     8 |  228 | `			}` |
|  4642 |  229 | `		}else if( IniNameIs(&aSlot[i],"session.save_path") ){` |
|    95 |  230 | `			if( SyBlobLength(&aSlot[i].sGlobal) > 0 ){` |
|   ! 0 |  231 | `				pDst = &pVm->sSessPath;` |
|   ! 0 |  232 | `			}` |
|    45 |  233 | `		}` |
|  4687 |  234 | `		if( pDst ){` |
|     6 |  235 | `			sxu32 nLen = SyBlobLength(&aSlot[i].sGlobal);` |
|     6 |  236 | `			const char *zVal = (const char *)SyBlobData(&aSlot[i].sGlobal);` |
|     6 |  237 | `			while( nLen > 0 && zVal[nLen-1] == '/' ){ nLen--; } /* rtrim('/') */` |
|     6 |  238 | `			SyBlobReset(pDst);` |
|     6 |  239 | `			SyBlobAppend(pDst,zVal,nLen);` |
|     3 |  240 | `		}` |
|  2346 |  241 | `	}` |
|    95 |  242 | `	return SXRET_OK;` |
|  1421 |  243 | `}` |
|  2829 |  244 | `static VmIniSlot * IniFind(ph7_vm *pVm,const char *zName,sxu32 nName)` |
|     5 |  245 | `{` |
|  2834 |  246 | `	VmIniSlot *aSlot = (VmIniSlot *)SySetBasePtr(&pVm->aIniTab);` |
|     - |  247 | `	sxu32 n;` |
| 99481 |  248 | `	for( n = 0 ; n < SySetUsed(&pVm->aIniTab) ; n++ ){` |
| 99432 |  249 | `		if( aSlot[n].sName.nByte == nName` |
| 53325 |  250 | `		 && SyMemcmp(aSlot[n].sName.zString,zName,nName) == 0 ){` |
|  2790 |  251 | `			return &aSlot[n];` |
|     - |  252 | `		}` |
| 48310 |  253 | `	}` |
|    47 |  254 | `	return 0;` |
|  1419 |  255 | `}` |
|     - |  256 | `/*` |
|     - |  257 | ` * The EFFECTIVE current value. For a live-wired directive the runtime knob is` |
|     - |  258 | ` * the truth, not the stored local value, so that ini_get() and the knob's own` |
|     - |  259 | ` * accessor can never disagree.` |
|     - |  260 | ` */` |
|  2871 |  261 | `static void IniLiveGet(ph7_vm *pVm,VmIniSlot *pSlot,SyBlob *pOut)` |
|     5 |  262 | `{` |
|  2876 |  263 | `	SyBlobReset(pOut);` |
|  2876 |  264 | `	if( IniNameIs(pSlot,"error_reporting") ){` |
|     - |  265 | `		char zBuf[32];` |
|     3 |  266 | `		int nBuf = SyBufferFormat(zBuf,sizeof(zBuf),"%d",` |
|     2 |  267 | `			pVm->bErrReport ? (int)pVm->iErrMask : 0);` |
|     3 |  268 | `		SyBlobAppend(pOut,zBuf,(sxu32)nBuf);` |
|     3 |  269 | `		return;` |
|     - |  270 | `	}` |
|  2874 |  271 | `	if( IniNameIs(pSlot,"session.name") ){` |
|    40 |  272 | `		SyBlobAppend(pOut,SyBlobData(&pVm->sSessName),SyBlobLength(&pVm->sSessName));` |
|    40 |  273 | `		return;` |
|     - |  274 | `	}` |
|  2836 |  275 | `	if( IniNameIs(pSlot,"session.save_path") && SyBlobLength(&pVm->sSessPath) > 0 ){` |
|    11 |  276 | `		SyBlobAppend(pOut,SyBlobData(&pVm->sSessPath),SyBlobLength(&pVm->sSessPath));` |
|    11 |  277 | `		return;` |
|     - |  278 | `	}` |
|  2826 |  279 | `	if( IniNameIs(pSlot,"include_path") ){` |
|     - |  280 | `		/* The VM's path SET is the store; this directive is a view of it, so` |
|     - |  281 | `		 * ini_get() and get_include_path() can never name different paths. */` |
|    17 |  282 | `		PH7_VmGetIncludePath(pVm,pOut);` |
|    17 |  283 | `		return;` |
|     - |  284 | `	}` |
|  2810 |  285 | `	SyBlobAppend(pOut,SyBlobData(&pSlot->sLocal),SyBlobLength(&pSlot->sLocal));` |
|  1440 |  286 | `}` |
|     - |  287 | `/*` |
|     - |  288 | ` * Push a new value at the runtime knob behind a live-wired directive. The stored` |
|     - |  289 | ` * local value is updated by the caller either way.` |
|     - |  290 | ` */` |
|   220 |  291 | `static void IniLiveSet(ph7_vm *pVm,VmIniSlot *pSlot,const char *zVal,sxu32 nVal)` |
|     5 |  292 | `{` |
|   225 |  293 | `	if( IniNameIs(pSlot,"error_reporting") ){` |
|   ! 0 |  294 | `		sxi32 iVal = 0;` |
|   ! 0 |  295 | `		if( nVal > 0 ){` |
|   ! 0 |  296 | `			SyStrToInt32(zVal,nVal,(void *)&iVal,0);` |
|   ! 0 |  297 | `		}` |
|   ! 0 |  298 | `		pVm->iErrMask = iVal;` |
|   ! 0 |  299 | `		pVm->bErrReport = iVal != 0;` |
|   ! 0 |  300 | `		return;` |
|     - |  301 | `	}` |
|   225 |  302 | `	if( IniNameIs(pSlot,"display_errors") ){` |
|     5 |  303 | `		pVm->bDisplayErrors = IniTruthy(zVal,nVal);` |
|     5 |  304 | `		return;` |
|     - |  305 | `	}` |
|   221 |  306 | `	if( IniNameIs(pSlot,"log_errors") ){` |
|   ! 0 |  307 | `		pVm->bLogErrors = IniTruthy(zVal,nVal);` |
|   ! 0 |  308 | `		return;` |
|     - |  309 | `	}` |
|   221 |  310 | `	if( IniNameIs(pSlot,"session.name") \|\| IniNameIs(pSlot,"session.save_path") ){` |
|    58 |  311 | `		int bPath = IniNameIs(pSlot,"session.save_path");` |
|    58 |  312 | `		SyBlob *pDst = bPath ? &pVm->sSessPath : &pVm->sSessName;` |
|    58 |  313 | `		sxu32 nLen = nVal;` |
|    58 |  314 | `		if( bPath ){` |
|    32 |  315 | `			while( nLen > 0 && zVal[nLen-1] == '/' ){ nLen--; }` |
|    15 |  316 | `		}` |
|    58 |  317 | `		SyBlobReset(pDst);` |
|    58 |  318 | `		SyBlobAppend(pDst,zVal,nLen);` |
|    58 |  319 | `		return;` |
|     - |  320 | `	}` |
|   165 |  321 | `	if( IniNameIs(pSlot,"include_path") ){` |
|     - |  322 | `		/* The one write that moves the include walk. Refused empty by the caller` |
|     - |  323 | `		 * (php's OnUpdateStringUnempty), so nVal is never 0 on the ini_set() path;` |
|     - |  324 | `		 * the boot/-d and ini_restore() paths carry a real value too. */` |
|     5 |  325 | `		PH7_VmSetIncludePath(pVm,zVal,nVal);` |
|     5 |  326 | `		return;` |
|     - |  327 | `	}` |
|   161 |  328 | `	if( IniNameIs(pSlot,"date.timezone") ){` |
|     - |  329 | `		/* Only UTC/GMT exist here (no tz database), matching the engine's own` |
|     - |  330 | `		 * date_default_timezone_set(). */` |
|   ! 0 |  331 | `		if( nVal == 3 && (SyStrnicmp(zVal,"UTC",3) == 0 \|\| SyStrnicmp(zVal,"GMT",3) == 0) ){` |
|   ! 0 |  332 | `			SyMemcpy(zVal,pVm->zDefTz,3);` |
|   ! 0 |  333 | `			pVm->zDefTz[3] = 0;` |
|   ! 0 |  334 | `			pVm->nDefTz = 3;` |
|   ! 0 |  335 | `		}` |
|   ! 0 |  336 | `		return;` |
|     - |  337 | `	}` |
|   115 |  338 | `}` |
|     - |  339 | `/*` |
|     - |  340 | ` * A session directive is settable only while there is no session to disturb: not` |
|     - |  341 | ` * once one is ACTIVE (the store is open and the cookie decided), and not once` |
|     - |  342 | ` * headers have gone out. Answers TRUE (having raised the warning) when the write` |
|     - |  343 | ` * must be refused.` |
|     - |  344 | ` */` |
|   180 |  345 | `static int IniSessionLocked(ph7_context *pCtx,VmIniSlot *pSlot,const char *zFunc)` |
|     4 |  346 | `{` |
|   184 |  347 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  348 | `	char zMsg[160];` |
|     - |  349 | `	const char *zWhy;` |
|   180 |  350 | `	if( pSlot->sName.nByte < sizeof("session.")-1` |
|   184 |  351 | `	 \|\| SyMemcmp(pSlot->sName.zString,"session.",sizeof("session.")-1) != 0 ){` |
|    31 |  352 | `		return 0;` |
|     - |  353 | `	}` |
|   155 |  354 | `	if( pVm->iSessStatus == 2 /* PHP_SESSION_ACTIVE */ ){` |
|   ! 0 |  355 | `		zWhy = "when a session is active";` |
|   155 |  356 | `	}else if( pVm->bHeadersSent ){` |
|   ! 0 |  357 | `		zWhy = "after headers have already been sent";` |
|   ! 0 |  358 | `	}else{` |
|   155 |  359 | `		return 0;` |
|     - |  360 | `	}` |
|   ! 0 |  361 | `	SyBufferFormat(zMsg,sizeof(zMsg),` |
|   ! 0 |  362 | `		"%s(): Session ini settings cannot be changed %s",zFunc,zWhy);` |
|   ! 0 |  363 | `	PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);` |
|   ! 0 |  364 | `	return 1;` |
|    94 |  365 | `}` |
|     - |  366 | `/*` |
|     - |  367 | ` * The per-directive value rules php enforces on every write, and the diagnostic` |
|     - |  368 | ` * each one raises. zWho is the whole prefix php puts on it -- "ini_set()" or, when` |
|     - |  369 | ` * session_start() is applying its $options array, "session_start()" -- because php` |
|     - |  370 | ` * blames the call that made the write, not the API underneath it.` |
|     - |  371 | ` */` |
|   230 |  372 | `static int IniValueAccepted(ph7_vm *pVm,VmIniSlot *pSlot,const char *zVal,sxu32 nVal,` |
|     - |  373 | `	const char *zWho)` |
|     5 |  374 | `{` |
|     - |  375 | `	char zMsg[256];` |
|   235 |  376 | `	if( IniNameIs(pSlot,"include_path") && nVal < 1 ){` |
|     - |  377 | `		/* php registers include_path with OnUpdateStringUnempty: the EMPTY value` |
|     - |  378 | `		 * is refused in silence and the directive keeps what it had. */` |
|     3 |  379 | `		return 0;` |
|     - |  380 | `	}` |
|   228 |  381 | `	if( IniNameIs(pSlot,"session.serialize_handler")` |
|   121 |  382 | `	 && !(nVal == 3 && SyMemcmp(zVal,"php",3) == 0)` |
|     8 |  383 | `	 && !(nVal == 10 && SyMemcmp(zVal,"php_binary",10) == 0)` |
|     8 |  384 | `	 && !(nVal == 13 && SyMemcmp(zVal,"php_serialize",13) == 0) ){` |
|     - |  385 | `		/* php looks the name up in its registered serializer list and refuses what` |
|     - |  386 | `		 * it cannot find, keeping the directive where it was. */` |
|     4 |  387 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|     1 |  388 | `			"%s: Serialization handler \"%.*s\" cannot be found",zWho,(int)nVal,zVal);` |
|     3 |  389 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);` |
|     3 |  390 | `		return 0;` |
|     - |  391 | `	}` |
|   227 |  392 | `	if( IniNameIs(pSlot,"session.name") ){` |
|     - |  393 | `		/* The name goes out as a COOKIE name and comes back as one, so php holds it` |
|     - |  394 | `		 * to the cookie alphabet -- and refuses a numeric one, which a browser would` |
|     - |  395 | `		 * hand back as an integer array key. */` |
|     - |  396 | `		static const char zBad[] = "=,;.[ \t\r\n\013\014";` |
|     - |  397 | `		sxu32 i;` |
|    32 |  398 | `		int bBad = nVal < 1;` |
|   222 |  399 | `		for( i = 0 ; !bBad && i < nVal ; i++ ){` |
|   192 |  400 | `			if( zVal[i] == 0 \|\| SyByteFind(zBad,sizeof(zBad)-1,zVal[i],0) == SXRET_OK ){` |
|     3 |  401 | `				bBad = 1;` |
|     1 |  402 | `			}` |
|    97 |  403 | `		}` |
|    32 |  404 | `		if( !bBad ){` |
|    28 |  405 | `			sxi64 iDummy = 0;` |
|    28 |  406 | `			bBad = SyStrToInt64(zVal,nVal,(void *)&iDummy,0) == SXRET_OK;` |
|    13 |  407 | `		}` |
|    32 |  408 | `		if( bBad ){` |
|    10 |  409 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|     - |  410 | `				"%s: session.name \"%.*s\" must not be numeric, empty, contain null bytes"` |
|     - |  411 | `				" or any of the following characters \"=,;.[ \\t\\r\\n\\013\\014\"",` |
|     3 |  412 | `				zWho,(int)nVal,zVal);` |
|     7 |  413 | `			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);` |
|     7 |  414 | `			return 0;` |
|     - |  415 | `		}` |
|    12 |  416 | `	}` |
|   221 |  417 | `	return 1;` |
|   118 |  418 | `}` |
|     - |  419 | `/*` |
|     - |  420 | ` * Write a directive from C, the way ini_set() writes it. Answers 0 when the write` |
|     - |  421 | ` * was refused (unknown name, not user-settable, or a value the directive's own` |
|     - |  422 | ` * rule rejects) -- which is exactly what session_start()'s $options reports as` |
|     - |  423 | `` * `Setting option "%s" failed`.`` |
|     - |  424 | ` */` |
|    52 |  425 | `PH7_PRIVATE int PH7_VmIniSet(ph7_vm *pVm,const char *zName,sxu32 nName,` |
|     - |  426 | `	const char *zVal,sxu32 nVal,const char *zWho)` |
|     4 |  427 | `{` |
|     - |  428 | `	VmIniSlot *pSlot;` |
|    56 |  429 | `	if( IniSeed(pVm) != SXRET_OK ){` |
|   ! 0 |  430 | `		return 0;` |
|     - |  431 | `	}` |
|    56 |  432 | `	pSlot = IniFind(pVm,zName,nName);` |
|    56 |  433 | `	if( pSlot == 0 \|\| (pSlot->iAccess & VM_INI_USER) == 0 ){` |
|     3 |  434 | `		return 0;` |
|     - |  435 | `	}` |
|    54 |  436 | `	if( !IniValueAccepted(pVm,pSlot,zVal,nVal,zWho) ){` |
|     7 |  437 | `		return 0;` |
|     - |  438 | `	}` |
|    48 |  439 | `	SyBlobReset(&pSlot->sLocal);` |
|    48 |  440 | `	SyBlobAppend(&pSlot->sLocal,zVal,nVal);` |
|    48 |  441 | `	IniLiveSet(pVm,pSlot,zVal,nVal);` |
|    48 |  442 | `	return 1;` |
|    30 |  443 | `}` |
|     - |  444 | `/* string\|false ini_get(string $option) */` |
|   106 |  445 | `static int vm_builtin_ini_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  446 | `{` |
|   111 |  447 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  448 | `	VmIniSlot *pSlot;` |
|     - |  449 | `	const char *zName;` |
|   111 |  450 | `	int nName = 0;` |
|     - |  451 | `	SyBlob sOut;` |
|   111 |  452 | `	if( nArg < 1 \|\| IniSeed(pVm) != SXRET_OK ){` |
|   ! 0 |  453 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  454 | `		return PH7_OK;` |
|     - |  455 | `	}` |
|   111 |  456 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|   111 |  457 | `	pSlot = IniFind(pVm,zName,(sxu32)nName);` |
|   111 |  458 | `	if( pSlot == 0 ){` |
|     6 |  459 | `		ph7_result_bool(pCtx,0);` |
|     6 |  460 | `		return PH7_OK;` |
|     - |  461 | `	}` |
|   107 |  462 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|   107 |  463 | `	IniLiveGet(pVm,pSlot,&sOut);` |
|   107 |  464 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|   107 |  465 | `	SyBlobRelease(&sOut);` |
|   107 |  466 | `	return PH7_OK;` |
|    58 |  467 | `}` |
|     - |  468 | `/* string\|false ini_set(string $option, string\|int\|float\|bool\|null $value) */` |
|   180 |  469 | `static int vm_builtin_ini_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 |  470 | `{` |
|   184 |  471 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  472 | `	VmIniSlot *pSlot;` |
|     - |  473 | `	const char *zName;` |
|     - |  474 | `	const char *zVal;` |
|   184 |  475 | `	int nName = 0, nVal = 0;` |
|     - |  476 | `	SyBlob sOld;` |
|   184 |  477 | `	if( nArg < 2 \|\| IniSeed(pVm) != SXRET_OK ){` |
|   ! 0 |  478 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  479 | `		return PH7_OK;` |
|     - |  480 | `	}` |
|   184 |  481 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|   184 |  482 | `	pSlot = IniFind(pVm,zName,(sxu32)nName);` |
|   184 |  483 | `	if( pSlot == 0 \|\| (pSlot->iAccess & VM_INI_USER) == 0 ){` |
|     5 |  484 | `		ph7_result_bool(pCtx,0);` |
|     5 |  485 | `		return PH7_OK;` |
|     - |  486 | `	}` |
|   180 |  487 | `	if( IniSessionLocked(pCtx,pSlot,"ini_set") ){` |
|   ! 0 |  488 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  489 | `		return PH7_OK;` |
|     - |  490 | `	}` |
|     - |  491 | `	/* php: the -1 (compiled-out) state of zend.assertions is a php.ini-only switch,` |
|     - |  492 | `	 * and so is moving INTO it. Unprefixed warning, exactly as php prints it. */` |
|   180 |  493 | `	if( IniNameIs(pSlot,"zend.assertions") ){` |
|   ! 0 |  494 | `		int bGlobalOff = SyBlobLength(&pSlot->sGlobal) == 2` |
|   ! 0 |  495 | `			&& SyMemcmp(SyBlobData(&pSlot->sGlobal),"-1",2) == 0;` |
|   ! 0 |  496 | `		const char *zNew = ph7_value_to_string(apArg[1],&nVal);` |
|   ! 0 |  497 | `		int bNewOff = nVal == 2 && SyMemcmp(zNew,"-1",2) == 0;` |
|   ! 0 |  498 | `		if( bGlobalOff \|\| bNewOff ){` |
|   ! 0 |  499 | `			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|     - |  500 | `				"zend.assertions may be completely enabled or disabled only in php.ini");` |
|   ! 0 |  501 | `			ph7_result_bool(pCtx,0);` |
|   ! 0 |  502 | `			return PH7_OK;` |
|     - |  503 | `		}` |
|   ! 0 |  504 | `	}` |
|     - |  505 | `	/* The OLD value php answers is the effective one, read before the write. */` |
|   180 |  506 | `	SyBlobInit(&sOld,&pVm->sAllocator);` |
|   180 |  507 | `	IniLiveGet(pVm,pSlot,&sOld);` |
|     - |  508 | `	/* php stringifies the incoming value, with a bool becoming "1"/"" . */` |
|   180 |  509 | `	if( ph7_value_is_bool(apArg[1]) ){` |
|   ! 0 |  510 | `		zVal = ph7_value_to_bool(apArg[1]) ? "1" : "";` |
|   ! 0 |  511 | `		nVal = (int)SyStrlen(zVal);` |
|   ! 0 |  512 | `	}else{` |
|   180 |  513 | `		zVal = ph7_value_to_string(apArg[1],&nVal);` |
|     - |  514 | `	}` |
|   180 |  515 | `	if( !IniValueAccepted(pVm,pSlot,zVal,(sxu32)nVal,"ini_set()") ){` |
|     6 |  516 | `		SyBlobRelease(&sOld);` |
|     6 |  517 | `		ph7_result_bool(pCtx,0);` |
|     6 |  518 | `		return PH7_OK;` |
|     - |  519 | `	}` |
|   176 |  520 | `	SyBlobReset(&pSlot->sLocal);` |
|   176 |  521 | `	SyBlobAppend(&pSlot->sLocal,zVal,(sxu32)nVal);` |
|   176 |  522 | `	IniLiveSet(pVm,pSlot,zVal,(sxu32)nVal);` |
|   176 |  523 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOld),(int)SyBlobLength(&sOld));` |
|   176 |  524 | `	SyBlobRelease(&sOld);` |
|   176 |  525 | `	return PH7_OK;` |
|    94 |  526 | `}` |
|     - |  527 | `/* void ini_restore(string $option) */` |
|     4 |  528 | `static int vm_builtin_ini_restore(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  529 | `{` |
|     5 |  530 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  531 | `	VmIniSlot *pSlot;` |
|     - |  532 | `	const char *zName;` |
|     5 |  533 | `	int nName = 0;` |
|     5 |  534 | `	ph7_result_null(pCtx);` |
|     5 |  535 | `	if( nArg < 1 \|\| IniSeed(pVm) != SXRET_OK ){` |
|   ! 0 |  536 | `		return PH7_OK;` |
|     - |  537 | `	}` |
|     5 |  538 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|     5 |  539 | `	pSlot = IniFind(pVm,zName,(sxu32)nName);` |
|     5 |  540 | `	if( pSlot == 0 \|\| IniSessionLocked(pCtx,pSlot,"ini_restore") ){` |
|   ! 0 |  541 | `		return PH7_OK;` |
|     - |  542 | `	}` |
|     5 |  543 | `	SyBlobReset(&pSlot->sLocal);` |
|     5 |  544 | `	SyBlobAppend(&pSlot->sLocal,SyBlobData(&pSlot->sGlobal),SyBlobLength(&pSlot->sGlobal));` |
|     5 |  545 | `	IniLiveSet(pVm,pSlot,(const char *)SyBlobData(&pSlot->sGlobal),SyBlobLength(&pSlot->sGlobal));` |
|     5 |  546 | `	return PH7_OK;` |
|     3 |  547 | `}` |
|     - |  548 | `/* array\|false ini_get_all(?string $extension = null, bool $details = true) */` |
|     4 |  549 | `static int vm_builtin_ini_get_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  550 | `{` |
|     5 |  551 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  552 | `	VmIniSlot *aSlot;` |
|     - |  553 | `	ph7_value *pOut,*pCur;` |
|     5 |  554 | `	const char *zExt = 0;` |
|     5 |  555 | `	int nExt = 0, bDetails = 1;` |
|     - |  556 | `	sxu32 n;` |
|     - |  557 | `	SyBlob sVal;` |
|     5 |  558 | `	if( IniSeed(pVm) != SXRET_OK ){` |
|   ! 0 |  559 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  560 | `		return PH7_OK;` |
|     - |  561 | `	}` |
|     5 |  562 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|     3 |  563 | `		zExt = ph7_value_to_string(apArg[0],&nExt);` |
|     1 |  564 | `	}` |
|     5 |  565 | `	if( nArg > 1 ){` |
|     3 |  566 | `		bDetails = ph7_value_to_bool(apArg[1]);` |
|     1 |  567 | `	}` |
|     5 |  568 | `	if( zExt ){` |
|     - |  569 | `		/* php reports the extensions it knows; anything else is a warning + false. */` |
|     - |  570 | `		static const char *azKnown[] = { "Core", "session", "date", "standard" };` |
|     3 |  571 | `		int bKnown = 0;` |
|     - |  572 | `		sxu32 k;` |
|     5 |  573 | `		for( k = 0 ; k < SX_ARRAYSIZE(azKnown) ; k++ ){` |
|     5 |  574 | `			if( (int)SyStrlen(azKnown[k]) == nExt && SyMemcmp(azKnown[k],zExt,(sxu32)nExt) == 0 ){` |
|     3 |  575 | `				bKnown = 1;` |
|     3 |  576 | `				break;` |
|     - |  577 | `			}` |
|     2 |  578 | `		}` |
|     3 |  579 | `		if( !bKnown ){` |
|   ! 0 |  580 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|   ! 0 |  581 | `				"Extension \"%.*s\" cannot be found",nExt,zExt);` |
|   ! 0 |  582 | `			ph7_result_bool(pCtx,0);` |
|   ! 0 |  583 | `			return PH7_OK;` |
|     - |  584 | `		}` |
|     1 |  585 | `	}` |
|     5 |  586 | `	pOut = ph7_context_new_array(pCtx);` |
|     5 |  587 | `	pCur = ph7_context_new_scalar(pCtx);` |
|     5 |  588 | `	if( pOut == 0 \|\| pCur == 0 ){` |
|   ! 0 |  589 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  590 | `	}` |
|     5 |  591 | `	SyBlobInit(&sVal,&pVm->sAllocator);` |
|     5 |  592 | `	aSlot = (VmIniSlot *)SySetBasePtr(&pVm->aIniTab);` |
|     - |  593 | `	/* The table is stored sorted, so this walk is already php's ksort order. */` |
|   213 |  594 | `	for( n = 0 ; n < SySetUsed(&pVm->aIniTab) ; n++ ){` |
|   209 |  595 | `		VmIniSlot *pSlot = &aSlot[n];` |
|     - |  596 | `		char zKey[128];` |
|   209 |  597 | `		if( zExt ){` |
|   105 |  598 | `			int bCore = (nExt == 4 && SyMemcmp(zExt,"Core",4) == 0)` |
|   104 |  599 | `				\|\| (nExt == 8 && SyMemcmp(zExt,"standard",8) == 0);` |
|   105 |  600 | `			if( !bCore ){` |
|     - |  601 | `				/* A named extension keeps only its own "<ext>." prefix. */` |
|   104 |  602 | `				if( pSlot->sName.nByte <= (sxu32)nExt` |
|   104 |  603 | `				 \|\| SyMemcmp(pSlot->sName.zString,zExt,(sxu32)nExt) != 0` |
|    74 |  604 | `				 \|\| pSlot->sName.zString[nExt] != '.' ){` |
|    63 |  605 | `					continue;` |
|     - |  606 | `				}` |
|    22 |  607 | `			}else{` |
|     - |  608 | `				/* Core/standard exclude the directives owned by a named extension. */` |
|   ! 0 |  609 | `				if( (pSlot->sName.nByte > sizeof("session.")-1` |
|   ! 0 |  610 | `				  && SyMemcmp(pSlot->sName.zString,"session.",sizeof("session.")-1) == 0)` |
|   ! 0 |  611 | `				 \|\| (pSlot->sName.nByte > sizeof("date.")-1` |
|   ! 0 |  612 | `				  && SyMemcmp(pSlot->sName.zString,"date.",sizeof("date.")-1) == 0) ){` |
|   ! 0 |  613 | `					continue;` |
|     - |  614 | `				}` |
|     - |  615 | `			}` |
|    21 |  616 | `		}` |
|   147 |  617 | `		if( pSlot->sName.nByte >= sizeof(zKey) ){` |
|   ! 0 |  618 | `			continue;` |
|     - |  619 | `		}` |
|   147 |  620 | `		SyMemcpy(pSlot->sName.zString,zKey,pSlot->sName.nByte);` |
|   147 |  621 | `		zKey[pSlot->sName.nByte] = 0;` |
|   147 |  622 | `		IniLiveGet(pVm,pSlot,&sVal);` |
|   147 |  623 | `		if( bDetails ){` |
|    43 |  624 | `			ph7_value *pRow = ph7_context_new_array(pCtx);` |
|    43 |  625 | `			if( pRow == 0 ){` |
|   ! 0 |  626 | `				break;` |
|     - |  627 | `			}` |
|    64 |  628 | `			ph7_value_string(pCur,(const char *)SyBlobData(&pSlot->sGlobal),` |
|    42 |  629 | `				(int)SyBlobLength(&pSlot->sGlobal));` |
|    43 |  630 | `			ph7_array_add_strkey_elem(pRow,"global_value",pCur);` |
|    43 |  631 | `			ph7_value_reset_string_cursor(pCur);` |
|    43 |  632 | `			ph7_value_string(pCur,(const char *)SyBlobData(&sVal),(int)SyBlobLength(&sVal));` |
|    43 |  633 | `			ph7_array_add_strkey_elem(pRow,"local_value",pCur);` |
|    43 |  634 | `			ph7_value_reset_string_cursor(pCur);` |
|    43 |  635 | `			ph7_value_int(pCur,pSlot->iAccess);` |
|    43 |  636 | `			ph7_array_add_strkey_elem(pRow,"access",pCur);` |
|    43 |  637 | `			ph7_value_reset_string_cursor(pCur);` |
|    43 |  638 | `			ph7_array_add_strkey_elem(pOut,zKey,pRow);` |
|    22 |  639 | `		}else{` |
|   105 |  640 | `			ph7_value_reset_string_cursor(pCur);` |
|   105 |  641 | `			ph7_value_string(pCur,(const char *)SyBlobData(&sVal),(int)SyBlobLength(&sVal));` |
|   105 |  642 | `			ph7_array_add_strkey_elem(pOut,zKey,pCur);` |
|   105 |  643 | `			ph7_value_reset_string_cursor(pCur);` |
|     - |  644 | `		}` |
|    74 |  645 | `	}` |
|     5 |  646 | `	SyBlobRelease(&sVal);` |
|     5 |  647 | `	ph7_result_value(pCtx,pOut);` |
|     5 |  648 | `	return PH7_OK;` |
|     3 |  649 | `}` |
|     - |  650 | `/* string\|false get_cfg_var(string $option) — php answers the GLOBAL value */` |
|     8 |  651 | `static int vm_builtin_get_cfg_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  652 | `{` |
|     9 |  653 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  654 | `	VmIniSlot *pSlot;` |
|     - |  655 | `	const char *zName;` |
|     9 |  656 | `	int nName = 0;` |
|     9 |  657 | `	if( nArg < 1 \|\| IniSeed(pVm) != SXRET_OK ){` |
|   ! 0 |  658 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  659 | `		return PH7_OK;` |
|     - |  660 | `	}` |
|     9 |  661 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|     9 |  662 | `	pSlot = IniFind(pVm,zName,(sxu32)nName);` |
|     9 |  663 | `	if( pSlot == 0 ){` |
|     3 |  664 | `		ph7_result_bool(pCtx,0);` |
|     3 |  665 | `		return PH7_OK;` |
|     - |  666 | `	}` |
|    10 |  667 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&pSlot->sGlobal),` |
|     6 |  668 | `		(int)SyBlobLength(&pSlot->sGlobal));` |
|     7 |  669 | `	return PH7_OK;` |
|     5 |  670 | `}` |
|     - |  671 | `/*` |
|     - |  672 | ` * The effective value of a directive a C builtin needs to obey, as an integer.` |
|     - |  673 | ` * parse_str() reads max_input_vars and max_input_nesting_level through this;` |
|     - |  674 | ` * without it a builtin would have to duplicate php's default and could never` |
|     - |  675 | `` * see an `ini_set()`/`-d` override. Answers iDefault when the directive is`` |
|     - |  676 | ` * absent or unparsable.` |
|     - |  677 | ` */` |
|   993 |  678 | `PH7_PRIVATE sxi64 PH7_VmIniGetInt(ph7_vm *pVm,const char *zName,sxi64 iDefault)` |
|     5 |  679 | `{` |
|     - |  680 | `	VmIniSlot *pSlot;` |
|     - |  681 | `	SyBlob sVal;` |
|   998 |  682 | `	sxi64 iVal = iDefault;` |
|   998 |  683 | `	IniSeed(pVm);` |
|   998 |  684 | `	pSlot = IniFind(pVm,zName,(sxu32)SyStrlen(zName));` |
|   998 |  685 | `	if( pSlot == 0 ){` |
|   ! 0 |  686 | `		return iDefault;` |
|     - |  687 | `	}` |
|   998 |  688 | `	SyBlobInit(&sVal,&pVm->sAllocator);` |
|   998 |  689 | `	IniLiveGet(pVm,pSlot,&sVal);` |
|   998 |  690 | `	if( SyBlobLength(&sVal) > 0 ){` |
|   998 |  691 | `		SyStrToInt64((const char *)SyBlobData(&sVal),SyBlobLength(&sVal),(void *)&iVal,0);` |
|   496 |  692 | `	}` |
|   998 |  693 | `	SyBlobRelease(&sVal);` |
|   998 |  694 | `	return iVal;` |
|   501 |  695 | `}` |
|     - |  696 | `/* The same, as a borrowed STRING (arg_separator.input). The bytes live in the` |
|     - |  697 | ` * caller's blob, which it owns. */` |
|   892 |  698 | `PH7_PRIVATE void PH7_VmIniGetStr(ph7_vm *pVm,const char *zName,SyBlob *pOut)` |
|     5 |  699 | `{` |
|     - |  700 | `	VmIniSlot *pSlot;` |
|   897 |  701 | `	IniSeed(pVm);` |
|   897 |  702 | `	SyBlobReset(pOut);` |
|   897 |  703 | `	pSlot = IniFind(pVm,zName,(sxu32)SyStrlen(zName));` |
|   897 |  704 | `	if( pSlot ){` |
|   865 |  705 | `		IniLiveGet(pVm,pSlot,pOut);` |
|   430 |  706 | `	}` |
|   897 |  707 | `}` |
|     - |  708 | `/*` |
|     - |  709 | ` * Read a BOOLEAN directive the way zend_ini does — "on"/"yes"/"true" as well as` |
|     - |  710 | ` * a non-zero number. Reading one through the integer parser answers 0 for` |
|     - |  711 | `` * `On`, which is the spelling php.ini-production ships, so a directive written`` |
|     - |  712 | ` * that way reads as OFF.` |
|     - |  713 | ` */` |
|   594 |  714 | `PH7_PRIVATE int PH7_VmIniGetBool(ph7_vm *pVm,const char *zName,int bDefault)` |
|     5 |  715 | `{` |
|     - |  716 | `	VmIniSlot *pSlot;` |
|     - |  717 | `	SyBlob sVal;` |
|     - |  718 | `	int bRes;` |
|   599 |  719 | `	IniSeed(pVm);` |
|   599 |  720 | `	pSlot = IniFind(pVm,zName,(sxu32)SyStrlen(zName));` |
|   599 |  721 | `	if( pSlot == 0 ){` |
|   ! 0 |  722 | `		return bDefault;` |
|     - |  723 | `	}` |
|   599 |  724 | `	SyBlobInit(&sVal,&pVm->sAllocator);` |
|   599 |  725 | `	IniLiveGet(pVm,pSlot,&sVal);` |
|   599 |  726 | `	bRes = IniTruthy((const char *)SyBlobData(&sVal),SyBlobLength(&sVal));` |
|   599 |  727 | `	SyBlobRelease(&sVal);` |
|   599 |  728 | `	return bRes;` |
|   302 |  729 | `}` |
|  5146 |  730 | `PH7_PRIVATE sxi32 PH7_VmInstallIni(ph7_vm *pVm)` |
|     5 |  731 | `{` |
|     - |  732 | `	static const struct {` |
|     - |  733 | `		const char *zName;` |
|     - |  734 | `		ProchHostFunction xFunc;` |
|     - |  735 | `	} aFunc[] = {` |
|     - |  736 | `		{ "ini_get",      vm_builtin_ini_get      },` |
|     - |  737 | `		{ "ini_set",      vm_builtin_ini_set      },` |
|     - |  738 | `		{ "ini_restore",  vm_builtin_ini_restore  },` |
|     - |  739 | `		{ "ini_get_all",  vm_builtin_ini_get_all  },` |
|     - |  740 | `		{ "get_cfg_var",  vm_builtin_get_cfg_var  },` |
|     - |  741 | `	};` |
|     - |  742 | `	sxu32 n;` |
| 30881 |  743 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFunc) ; n++ ){` |
| 25735 |  744 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
| 12870 |  745 | `	}` |
|  5151 |  746 | `	return SXRET_OK;` |
|     5 |  747 | `}` |
|     - |  748 | `#else` |
|     - |  749 | `PH7_PRIVATE sxi32 PH7_VmInstallIni(ph7_vm *pVm){ (void)pVm; return SXRET_OK; }` |
|     - |  750 | `PH7_PRIVATE sxi64 PH7_VmIniGetInt(ph7_vm *pVm,const char *zName,sxi64 iDefault){` |
|     - |  751 | `	(void)pVm; (void)zName; return iDefault;` |
|     - |  752 | `}` |
|     - |  753 | `PH7_PRIVATE int PH7_VmIniGetBool(ph7_vm *pVm,const char *zName,int bDefault){` |
|     - |  754 | `	(void)pVm; (void)zName; return bDefault;` |
|     - |  755 | `}` |
|     - |  756 | `PH7_PRIVATE void PH7_VmIniGetStr(ph7_vm *pVm,const char *zName,SyBlob *pOut){` |
|     - |  757 | `	(void)pVm; (void)zName; SyBlobReset(pOut);` |
|     - |  758 | `}` |
|     - |  759 | `PH7_PRIVATE int PH7_VmIniSet(ph7_vm *pVm,const char *zName,sxu32 nName,` |
|     - |  760 | `	const char *zVal,sxu32 nVal,const char *zWho){` |
|     - |  761 | `	(void)pVm; (void)zName; (void)nName; (void)zVal; (void)nVal; (void)zWho; return 0;` |
|     - |  762 | `}` |
|     - |  763 | `#endif` |
|     - |  764 |  |
