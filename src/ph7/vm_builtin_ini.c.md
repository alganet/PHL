# src/ph7/vm_builtin_ini.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 293/370 lines (79.19%)

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
|     - |   45 | `	{ "arg_separator.input",      "&",          VM_INI_ALL },` |
|     - |   46 | `	{ "arg_separator.output",     "&",          VM_INI_ALL },` |
|     - |   47 | `	{ "auto_detect_line_endings", "",           VM_INI_ALL },` |
|     - |   48 | `	{ "date.timezone",            "UTC",        VM_INI_ALL },` |
|     - |   49 | `	{ "default_charset",          "UTF-8",      VM_INI_ALL },` |
|     - |   50 | `	{ "default_mimetype",         "text/html",  VM_INI_ALL },` |
|     - |   51 | `	{ "display_errors",           "",           VM_INI_ALL },` |
|     - |   52 | `	{ "error_log",                "",           VM_INI_ALL },` |
|     - |   53 | `	{ "error_reporting",          "30719",      VM_INI_ALL },` |
|     - |   54 | `	{ "highlight.comment",        "#FF8000",    VM_INI_ALL },` |
|     - |   55 | `	{ "highlight.default",        "#0000BB",    VM_INI_ALL },` |
|     - |   56 | `	{ "highlight.html",           "#000000",    VM_INI_ALL },` |
|     - |   57 | `	{ "highlight.keyword",        "#007700",    VM_INI_ALL },` |
|     - |   58 | `	{ "highlight.string",         "#DD0000",    VM_INI_ALL },` |
|     - |   59 | `	{ "include_path",             ".",          VM_INI_ALL },` |
|     - |   60 | `	{ "log_errors",               "1",          VM_INI_ALL },` |
|     - |   61 | `	{ "max_execution_time",       "0",          VM_INI_ALL },` |
|     - |   62 | `	{ "max_input_nesting_level",  "64",         VM_INI_PERDIR\|VM_INI_SYSTEM },` |
|     - |   63 | `	{ "max_input_vars",           "1000",       VM_INI_PERDIR\|VM_INI_SYSTEM },` |
|     - |   64 | `	{ "memory_limit",             "-1",         VM_INI_ALL },` |
|     - |   65 | `	{ "post_max_size",            "8M",         VM_INI_PERDIR\|VM_INI_SYSTEM },` |
|     - |   66 | `	{ "precision",                "14",         VM_INI_ALL },` |
|     - |   67 | `	{ "serialize_precision",      "-1",         VM_INI_ALL },` |
|     - |   68 | `	{ "session.name",             "PHPSESSID",  VM_INI_ALL },` |
|     - |   69 | `	{ "session.save_path",        "",           VM_INI_ALL },` |
|     - |   70 | `	{ "short_open_tag",           "",           VM_INI_PERDIR\|VM_INI_SYSTEM },` |
|     - |   71 | `	{ "unserialize_callback_func","",           VM_INI_ALL },` |
|     - |   72 | `	{ "unserialize_max_depth",    "4096",       VM_INI_ALL },` |
|     - |   73 | `	{ "upload_max_filesize",      "2M",         VM_INI_PERDIR\|VM_INI_SYSTEM },` |
|     - |   74 | `	{ "zend.assertions",          "-1",         VM_INI_ALL },` |
|     - |   75 | `};` |
|     - |   76 |  |
|  4478 |   77 | `static int IniNameIs(const VmIniSlot *pSlot,const char *zName)` |
|     5 |   78 | `{` |
|  4483 |   79 | `	sxu32 n = (sxu32)SyStrlen(zName);` |
|  4483 |   80 | `	return pSlot->sName.nByte == n && SyMemcmp(pSlot->sName.zString,zName,n) == 0;` |
|     5 |   81 | `}` |
|     - |   82 | `/*` |
|     - |   83 | ` * zend_ini_parse_bool semantics, matching the C-side VmIniBool the -d/-c path` |
|     - |   84 | ` * uses: on/yes/true, else a non-zero integer parse.` |
|     - |   85 | ` */` |
|     4 |   86 | `static int IniTruthy(const char *zVal,sxu32 nVal)` |
|     1 |   87 | `{` |
|     7 |   88 | `	while( nVal > 0 && (zVal[0] == ' ' \|\| zVal[0] == '\t') ){ zVal++; nVal--; }` |
|     7 |   89 | `	while( nVal > 0 && (zVal[nVal-1] == ' ' \|\| zVal[nVal-1] == '\t') ){ nVal--; }` |
|     5 |   90 | `	if( nVal == 2 && SyStrnicmp(zVal,"on",2) == 0 ){ return 1; }` |
|     5 |   91 | `	if( nVal == 3 && SyStrnicmp(zVal,"yes",3) == 0 ){ return 1; }` |
|     5 |   92 | `	if( nVal == 4 && SyStrnicmp(zVal,"true",4) == 0 ){ return 1; }` |
|     - |   93 | `	{` |
|     5 |   94 | `		sxi32 iVal = 0;` |
|     5 |   95 | `		if( nVal > 0 && SyStrToInt32(zVal,nVal,(void *)&iVal,0) == SXRET_OK ){` |
|     5 |   96 | `			return iVal != 0;` |
|     - |   97 | `		}` |
|     - |   98 | `	}` |
|   ! 0 |   99 | `	return 0;` |
|     3 |  100 | `}` |
|     - |  101 | `/*` |
|     - |  102 | ` * Build the table: the static defaults, then the CLI queue merged over them (an` |
|     - |  103 | ` * unknown CLI name is appended as a new INI_ALL directive, as the chunk did),` |
|     - |  104 | ` * then sorted by name so ini_get_all() can walk it in php's order without a sort.` |
|     - |  105 | ` */` |
|   820 |  106 | `static sxi32 IniSeed(ph7_vm *pVm)` |
|     5 |  107 | `{` |
|     - |  108 | `	sxu32 i,j;` |
|     - |  109 | `	VmIniEntry *aCli;` |
|     - |  110 | `	VmIniSlot *aSlot;` |
|   825 |  111 | `	if( pVm->bIniSeeded ){` |
|   793 |  112 | `		return SXRET_OK;` |
|     - |  113 | `	}` |
|    37 |  114 | `	pVm->bIniSeeded = 1; /* set FIRST: the live-wired writes below re-enter nothing,` |
|     - |  115 | `	                      * but a future one must never recurse into the seed */` |
|  1029 |  116 | `	for( i = 0 ; i < SX_ARRAYSIZE(aIniDefault) ; i++ ){` |
|     - |  117 | `		VmIniSlot sSlot;` |
|   997 |  118 | `		SyStringInitFromBuf(&sSlot.sName,aIniDefault[i].zName,SyStrlen(aIniDefault[i].zName));` |
|   997 |  119 | `		sSlot.iAccess = aIniDefault[i].iAccess;` |
|   997 |  120 | `		SyBlobInit(&sSlot.sGlobal,&pVm->sAllocator);` |
|   997 |  121 | `		SyBlobInit(&sSlot.sLocal,&pVm->sAllocator);` |
|   997 |  122 | `		SyBlobAppend(&sSlot.sGlobal,aIniDefault[i].zValue,(sxu32)SyStrlen(aIniDefault[i].zValue));` |
|   997 |  123 | `		SyBlobAppend(&sSlot.sLocal,aIniDefault[i].zValue,(sxu32)SyStrlen(aIniDefault[i].zValue));` |
|   997 |  124 | `		if( SySetPut(&pVm->aIniTab,(const void *)&sSlot) != SXRET_OK ){` |
|   ! 0 |  125 | `			return SXERR_MEM;` |
|     - |  126 | `		}` |
|   501 |  127 | `	}` |
|    37 |  128 | `	aCli = (VmIniEntry *)SySetBasePtr(&pVm->aIniCli);` |
|    57 |  129 | `	for( i = 0 ; i < SySetUsed(&pVm->aIniCli) ; i++ ){` |
|    21 |  130 | `		int bFound = 0;` |
|    21 |  131 | `		aSlot = (VmIniSlot *)SySetBasePtr(&pVm->aIniTab);` |
|   415 |  132 | `		for( j = 0 ; j < SySetUsed(&pVm->aIniTab) ; j++ ){` |
|   412 |  133 | `			if( aSlot[j].sName.nByte == aCli[i].sName.nByte` |
|   229 |  134 | `			 && SyMemcmp(aSlot[j].sName.zString,aCli[i].sName.zString,aCli[i].sName.nByte) == 0 ){` |
|    19 |  135 | `				SyBlobReset(&aSlot[j].sGlobal);` |
|    19 |  136 | `				SyBlobReset(&aSlot[j].sLocal);` |
|    19 |  137 | `				SyBlobAppend(&aSlot[j].sGlobal,aCli[i].sValue.zString,aCli[i].sValue.nByte);` |
|    19 |  138 | `				SyBlobAppend(&aSlot[j].sLocal,aCli[i].sValue.zString,aCli[i].sValue.nByte);` |
|    19 |  139 | `				bFound = 1;` |
|    19 |  140 | `				break;` |
|     - |  141 | `			}` |
|   198 |  142 | `		}` |
|    21 |  143 | `		if( !bFound ){` |
|     - |  144 | `			VmIniSlot sSlot;` |
|     - |  145 | `			/* aIniCli holds VM-lifetime copies already, so the name can be aliased. */` |
|     3 |  146 | `			sSlot.sName = aCli[i].sName;` |
|     3 |  147 | `			sSlot.iAccess = VM_INI_ALL;` |
|     3 |  148 | `			SyBlobInit(&sSlot.sGlobal,&pVm->sAllocator);` |
|     3 |  149 | `			SyBlobInit(&sSlot.sLocal,&pVm->sAllocator);` |
|     3 |  150 | `			SyBlobAppend(&sSlot.sGlobal,aCli[i].sValue.zString,aCli[i].sValue.nByte);` |
|     3 |  151 | `			SyBlobAppend(&sSlot.sLocal,aCli[i].sValue.zString,aCli[i].sValue.nByte);` |
|     3 |  152 | `			if( SySetPut(&pVm->aIniTab,(const void *)&sSlot) != SXRET_OK ){` |
|   ! 0 |  153 | `				return SXERR_MEM;` |
|     - |  154 | `			}` |
|     1 |  155 | `		}` |
|    11 |  156 | `	}` |
|     - |  157 | `	/* Insertion sort by name (the table is ~30 entries and already nearly sorted:` |
|     - |  158 | `	 * only CLI-introduced directives are out of place). */` |
|    37 |  159 | `	aSlot = (VmIniSlot *)SySetBasePtr(&pVm->aIniTab);` |
|   999 |  160 | `	for( i = 1 ; i < SySetUsed(&pVm->aIniTab) ; i++ ){` |
|   967 |  161 | `		VmIniSlot sTmp = aSlot[i];` |
|   967 |  162 | `		j = i;` |
|   987 |  163 | `		while( j > 0 ){` |
|   987 |  164 | `			const VmIniSlot *pPrev = &aSlot[j-1];` |
|   987 |  165 | `			sxu32 nMin = pPrev->sName.nByte < sTmp.sName.nByte ? pPrev->sName.nByte : sTmp.sName.nByte;` |
|   987 |  166 | `			sxi32 iCmp = SyMemcmp(pPrev->sName.zString,sTmp.sName.zString,nMin);` |
|   987 |  167 | `			if( iCmp == 0 ){` |
|   ! 0 |  168 | `				iCmp = (sxi32)pPrev->sName.nByte - (sxi32)sTmp.sName.nByte;` |
|   ! 0 |  169 | `			}` |
|   987 |  170 | `			if( iCmp <= 0 ){` |
|   967 |  171 | `				break;` |
|     - |  172 | `			}` |
|    21 |  173 | `			aSlot[j] = aSlot[j-1];` |
|    21 |  174 | `			j--;` |
|     1 |  175 | `		}` |
|   967 |  176 | `		aSlot[j] = sTmp;` |
|   486 |  177 | `	}` |
|     - |  178 | `	/* Boot-apply the CLI values for the live-wired session knobs. The engine knobs` |
|     - |  179 | `	 * (error_reporting / date.timezone) were already applied C-side by` |
|     - |  180 | `	 * PH7_VM_CONFIG_INI_ENTRY. */` |
|    37 |  181 | `	aSlot = (VmIniSlot *)SySetBasePtr(&pVm->aIniTab);` |
|  1031 |  182 | `	for( i = 0 ; i < SySetUsed(&pVm->aIniTab) ; i++ ){` |
|   999 |  183 | `		SyBlob *pDst = 0;` |
|   999 |  184 | `		if( IniNameIs(&aSlot[i],"session.name") ){` |
|    32 |  185 | `			if( SyBlobLength(&aSlot[i].sGlobal) != sizeof("PHPSESSID")-1` |
|    35 |  186 | `			 \|\| SyMemcmp(SyBlobData(&aSlot[i].sGlobal),"PHPSESSID",sizeof("PHPSESSID")-1) != 0 ){` |
|     6 |  187 | `				pDst = &pVm->sSessName;` |
|     8 |  188 | `			}` |
|   983 |  189 | `		}else if( IniNameIs(&aSlot[i],"session.save_path") ){` |
|    37 |  190 | `			if( SyBlobLength(&aSlot[i].sGlobal) > 0 ){` |
|   ! 0 |  191 | `				pDst = &pVm->sSessPath;` |
|   ! 0 |  192 | `			}` |
|    16 |  193 | `		}` |
|   999 |  194 | `		if( pDst ){` |
|     6 |  195 | `			sxu32 nLen = SyBlobLength(&aSlot[i].sGlobal);` |
|     6 |  196 | `			const char *zVal = (const char *)SyBlobData(&aSlot[i].sGlobal);` |
|     6 |  197 | `			while( nLen > 0 && zVal[nLen-1] == '/' ){ nLen--; } /* rtrim('/') */` |
|     6 |  198 | `			SyBlobReset(pDst);` |
|     6 |  199 | `			SyBlobAppend(pDst,zVal,nLen);` |
|     3 |  200 | `		}` |
|   502 |  201 | `	}` |
|    37 |  202 | `	return SXRET_OK;` |
|   415 |  203 | `}` |
|   818 |  204 | `static VmIniSlot * IniFind(ph7_vm *pVm,const char *zName,sxu32 nName)` |
|     5 |  205 | `{` |
|   823 |  206 | `	VmIniSlot *aSlot = (VmIniSlot *)SySetBasePtr(&pVm->aIniTab);` |
|     - |  207 | `	sxu32 n;` |
| 18065 |  208 | `	for( n = 0 ; n < SySetUsed(&pVm->aIniTab) ; n++ ){` |
| 18038 |  209 | `		if( aSlot[n].sName.nByte == nName` |
|  9599 |  210 | `		 && SyMemcmp(aSlot[n].sName.zString,zName,nName) == 0 ){` |
|   801 |  211 | `			return &aSlot[n];` |
|     - |  212 | `		}` |
|  8626 |  213 | `	}` |
|    24 |  214 | `	return 0;` |
|   414 |  215 | `}` |
|     - |  216 | `/*` |
|     - |  217 | ` * The EFFECTIVE current value. For a live-wired directive the runtime knob is` |
|     - |  218 | ` * the truth, not the stored local value, so that ini_get() and the knob's own` |
|     - |  219 | ` * accessor can never disagree.` |
|     - |  220 | ` */` |
|   794 |  221 | `static void IniLiveGet(ph7_vm *pVm,VmIniSlot *pSlot,SyBlob *pOut)` |
|     5 |  222 | `{` |
|   799 |  223 | `	SyBlobReset(pOut);` |
|   799 |  224 | `	if( IniNameIs(pSlot,"error_reporting") ){` |
|     - |  225 | `		char zBuf[32];` |
|   ! 0 |  226 | `		int nBuf = SyBufferFormat(zBuf,sizeof(zBuf),"%d",` |
|   ! 0 |  227 | `			pVm->bErrReport ? (int)pVm->iErrMask : 0);` |
|   ! 0 |  228 | `		SyBlobAppend(pOut,zBuf,(sxu32)nBuf);` |
|   ! 0 |  229 | `		return;` |
|     - |  230 | `	}` |
|   799 |  231 | `	if( IniNameIs(pSlot,"session.name") ){` |
|    19 |  232 | `		SyBlobAppend(pOut,SyBlobData(&pVm->sSessName),SyBlobLength(&pVm->sSessName));` |
|    19 |  233 | `		return;` |
|     - |  234 | `	}` |
|   781 |  235 | `	if( IniNameIs(pSlot,"session.save_path") && SyBlobLength(&pVm->sSessPath) > 0 ){` |
|   ! 0 |  236 | `		SyBlobAppend(pOut,SyBlobData(&pVm->sSessPath),SyBlobLength(&pVm->sSessPath));` |
|   ! 0 |  237 | `		return;` |
|     - |  238 | `	}` |
|   781 |  239 | `	SyBlobAppend(pOut,SyBlobData(&pSlot->sLocal),SyBlobLength(&pSlot->sLocal));` |
|   402 |  240 | `}` |
|     - |  241 | `/*` |
|     - |  242 | ` * Push a new value at the runtime knob behind a live-wired directive. The stored` |
|     - |  243 | ` * local value is updated by the caller either way.` |
|     - |  244 | ` */` |
|    26 |  245 | `static void IniLiveSet(ph7_vm *pVm,VmIniSlot *pSlot,const char *zVal,sxu32 nVal)` |
|     3 |  246 | `{` |
|    29 |  247 | `	if( IniNameIs(pSlot,"error_reporting") ){` |
|   ! 0 |  248 | `		sxi32 iVal = 0;` |
|   ! 0 |  249 | `		if( nVal > 0 ){` |
|   ! 0 |  250 | `			SyStrToInt32(zVal,nVal,(void *)&iVal,0);` |
|   ! 0 |  251 | `		}` |
|   ! 0 |  252 | `		pVm->iErrMask = iVal;` |
|   ! 0 |  253 | `		pVm->bErrReport = iVal != 0;` |
|   ! 0 |  254 | `		return;` |
|     - |  255 | `	}` |
|    29 |  256 | `	if( IniNameIs(pSlot,"display_errors") ){` |
|     5 |  257 | `		pVm->bDisplayErrors = IniTruthy(zVal,nVal);` |
|     5 |  258 | `		return;` |
|     - |  259 | `	}` |
|    24 |  260 | `	if( IniNameIs(pSlot,"log_errors") ){` |
|   ! 0 |  261 | `		pVm->bLogErrors = IniTruthy(zVal,nVal);` |
|   ! 0 |  262 | `		return;` |
|     - |  263 | `	}` |
|    24 |  264 | `	if( IniNameIs(pSlot,"session.name") \|\| IniNameIs(pSlot,"session.save_path") ){` |
|     7 |  265 | `		int bPath = IniNameIs(pSlot,"session.save_path");` |
|     7 |  266 | `		SyBlob *pDst = bPath ? &pVm->sSessPath : &pVm->sSessName;` |
|     7 |  267 | `		sxu32 nLen = nVal;` |
|     7 |  268 | `		if( bPath ){` |
|   ! 0 |  269 | `			while( nLen > 0 && zVal[nLen-1] == '/' ){ nLen--; }` |
|   ! 0 |  270 | `		}` |
|     7 |  271 | `		SyBlobReset(pDst);` |
|     7 |  272 | `		SyBlobAppend(pDst,zVal,nLen);` |
|     7 |  273 | `		return;` |
|     - |  274 | `	}` |
|    18 |  275 | `	if( IniNameIs(pSlot,"date.timezone") ){` |
|     - |  276 | `		/* Only UTC/GMT exist here (no tz database), matching the engine's own` |
|     - |  277 | `		 * date_default_timezone_set(). */` |
|   ! 0 |  278 | `		if( nVal == 3 && (SyStrnicmp(zVal,"UTC",3) == 0 \|\| SyStrnicmp(zVal,"GMT",3) == 0) ){` |
|   ! 0 |  279 | `			SyMemcpy(zVal,pVm->zDefTz,3);` |
|   ! 0 |  280 | `			pVm->zDefTz[3] = 0;` |
|   ! 0 |  281 | `			pVm->nDefTz = 3;` |
|   ! 0 |  282 | `		}` |
|   ! 0 |  283 | `		return;` |
|     - |  284 | `	}` |
|    16 |  285 | `}` |
|     - |  286 | `/*` |
|     - |  287 | ` * The session directives are php.ini-settable only until headers go out.` |
|     - |  288 | ` * Answers TRUE (and has raised the warning) when the write must be refused.` |
|     - |  289 | ` */` |
|    26 |  290 | `static int IniSessionLocked(ph7_context *pCtx,VmIniSlot *pSlot,const char *zFunc)` |
|     3 |  291 | `{` |
|     - |  292 | `	char zMsg[160];` |
|    26 |  293 | `	if( pSlot->sName.nByte < sizeof("session.")-1` |
|    29 |  294 | `	 \|\| SyMemcmp(pSlot->sName.zString,"session.",sizeof("session.")-1) != 0 ){` |
|    23 |  295 | `		return 0;` |
|     - |  296 | `	}` |
|     7 |  297 | `	if( !pCtx->pVm->bHeadersSent ){` |
|     7 |  298 | `		return 0;` |
|     - |  299 | `	}` |
|   ! 0 |  300 | `	SyBufferFormat(zMsg,sizeof(zMsg),` |
|     - |  301 | `		"%s(): Session ini settings cannot be changed after headers have already been sent",` |
|   ! 0 |  302 | `		zFunc);` |
|   ! 0 |  303 | `	PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|   ! 0 |  304 | `	return 1;` |
|    16 |  305 | `}` |
|     - |  306 | `/* string\|false ini_get(string $option) */` |
|    48 |  307 | `static int vm_builtin_ini_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  308 | `{` |
|    51 |  309 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  310 | `	VmIniSlot *pSlot;` |
|     - |  311 | `	const char *zName;` |
|    51 |  312 | `	int nName = 0;` |
|     - |  313 | `	SyBlob sOut;` |
|    51 |  314 | `	if( nArg < 1 \|\| IniSeed(pVm) != SXRET_OK ){` |
|   ! 0 |  315 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  316 | `		return PH7_OK;` |
|     - |  317 | `	}` |
|    51 |  318 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|    51 |  319 | `	pSlot = IniFind(pVm,zName,(sxu32)nName);` |
|    51 |  320 | `	if( pSlot == 0 ){` |
|     6 |  321 | `		ph7_result_bool(pCtx,0);` |
|     6 |  322 | `		return PH7_OK;` |
|     - |  323 | `	}` |
|    47 |  324 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|    47 |  325 | `	IniLiveGet(pVm,pSlot,&sOut);` |
|    47 |  326 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    47 |  327 | `	SyBlobRelease(&sOut);` |
|    47 |  328 | `	return PH7_OK;` |
|    27 |  329 | `}` |
|     - |  330 | `/* string\|false ini_set(string $option, string\|int\|float\|bool\|null $value) */` |
|    28 |  331 | `static int vm_builtin_ini_set(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  332 | `{` |
|    31 |  333 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  334 | `	VmIniSlot *pSlot;` |
|     - |  335 | `	const char *zName;` |
|     - |  336 | `	const char *zVal;` |
|    31 |  337 | `	int nName = 0, nVal = 0;` |
|     - |  338 | `	SyBlob sOld;` |
|    31 |  339 | `	if( nArg < 2 \|\| IniSeed(pVm) != SXRET_OK ){` |
|   ! 0 |  340 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  341 | `		return PH7_OK;` |
|     - |  342 | `	}` |
|    31 |  343 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|    31 |  344 | `	pSlot = IniFind(pVm,zName,(sxu32)nName);` |
|    31 |  345 | `	if( pSlot == 0 \|\| (pSlot->iAccess & VM_INI_USER) == 0 ){` |
|     5 |  346 | `		ph7_result_bool(pCtx,0);` |
|     5 |  347 | `		return PH7_OK;` |
|     - |  348 | `	}` |
|    27 |  349 | `	if( IniSessionLocked(pCtx,pSlot,"ini_set") ){` |
|   ! 0 |  350 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  351 | `		return PH7_OK;` |
|     - |  352 | `	}` |
|     - |  353 | `	/* php: the -1 (compiled-out) state of zend.assertions is a php.ini-only switch,` |
|     - |  354 | `	 * and so is moving INTO it. Unprefixed warning, exactly as php prints it. */` |
|    27 |  355 | `	if( IniNameIs(pSlot,"zend.assertions") ){` |
|   ! 0 |  356 | `		int bGlobalOff = SyBlobLength(&pSlot->sGlobal) == 2` |
|   ! 0 |  357 | `			&& SyMemcmp(SyBlobData(&pSlot->sGlobal),"-1",2) == 0;` |
|   ! 0 |  358 | `		const char *zNew = ph7_value_to_string(apArg[1],&nVal);` |
|   ! 0 |  359 | `		int bNewOff = nVal == 2 && SyMemcmp(zNew,"-1",2) == 0;` |
|   ! 0 |  360 | `		if( bGlobalOff \|\| bNewOff ){` |
|   ! 0 |  361 | `			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|     - |  362 | `				"zend.assertions may be completely enabled or disabled only in php.ini");` |
|   ! 0 |  363 | `			ph7_result_bool(pCtx,0);` |
|   ! 0 |  364 | `			return PH7_OK;` |
|     - |  365 | `		}` |
|   ! 0 |  366 | `	}` |
|     - |  367 | `	/* The OLD value php answers is the effective one, read before the write. */` |
|    27 |  368 | `	SyBlobInit(&sOld,&pVm->sAllocator);` |
|    27 |  369 | `	IniLiveGet(pVm,pSlot,&sOld);` |
|     - |  370 | `	/* php stringifies the incoming value, with a bool becoming "1"/"" . */` |
|    27 |  371 | `	if( ph7_value_is_bool(apArg[1]) ){` |
|   ! 0 |  372 | `		zVal = ph7_value_to_bool(apArg[1]) ? "1" : "";` |
|   ! 0 |  373 | `		nVal = (int)SyStrlen(zVal);` |
|   ! 0 |  374 | `	}else{` |
|    27 |  375 | `		zVal = ph7_value_to_string(apArg[1],&nVal);` |
|     - |  376 | `	}` |
|    27 |  377 | `	SyBlobReset(&pSlot->sLocal);` |
|    27 |  378 | `	SyBlobAppend(&pSlot->sLocal,zVal,(sxu32)nVal);` |
|    27 |  379 | `	IniLiveSet(pVm,pSlot,zVal,(sxu32)nVal);` |
|    27 |  380 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOld),(int)SyBlobLength(&sOld));` |
|    27 |  381 | `	SyBlobRelease(&sOld);` |
|    27 |  382 | `	return PH7_OK;` |
|    17 |  383 | `}` |
|     - |  384 | `/* void ini_restore(string $option) */` |
|     2 |  385 | `static int vm_builtin_ini_restore(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  386 | `{` |
|     3 |  387 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  388 | `	VmIniSlot *pSlot;` |
|     - |  389 | `	const char *zName;` |
|     3 |  390 | `	int nName = 0;` |
|     3 |  391 | `	ph7_result_null(pCtx);` |
|     3 |  392 | `	if( nArg < 1 \|\| IniSeed(pVm) != SXRET_OK ){` |
|   ! 0 |  393 | `		return PH7_OK;` |
|     - |  394 | `	}` |
|     3 |  395 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|     3 |  396 | `	pSlot = IniFind(pVm,zName,(sxu32)nName);` |
|     3 |  397 | `	if( pSlot == 0 \|\| IniSessionLocked(pCtx,pSlot,"ini_restore") ){` |
|   ! 0 |  398 | `		return PH7_OK;` |
|     - |  399 | `	}` |
|     3 |  400 | `	SyBlobReset(&pSlot->sLocal);` |
|     3 |  401 | `	SyBlobAppend(&pSlot->sLocal,SyBlobData(&pSlot->sGlobal),SyBlobLength(&pSlot->sGlobal));` |
|     3 |  402 | `	IniLiveSet(pVm,pSlot,(const char *)SyBlobData(&pSlot->sGlobal),SyBlobLength(&pSlot->sGlobal));` |
|     3 |  403 | `	return PH7_OK;` |
|     2 |  404 | `}` |
|     - |  405 | `/* array\|false ini_get_all(?string $extension = null, bool $details = true) */` |
|     2 |  406 | `static int vm_builtin_ini_get_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  407 | `{` |
|     3 |  408 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  409 | `	VmIniSlot *aSlot;` |
|     - |  410 | `	ph7_value *pOut,*pCur;` |
|     3 |  411 | `	const char *zExt = 0;` |
|     3 |  412 | `	int nExt = 0, bDetails = 1;` |
|     - |  413 | `	sxu32 n;` |
|     - |  414 | `	SyBlob sVal;` |
|     3 |  415 | `	if( IniSeed(pVm) != SXRET_OK ){` |
|   ! 0 |  416 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  417 | `		return PH7_OK;` |
|     - |  418 | `	}` |
|     3 |  419 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|     3 |  420 | `		zExt = ph7_value_to_string(apArg[0],&nExt);` |
|     1 |  421 | `	}` |
|     3 |  422 | `	if( nArg > 1 ){` |
|   ! 0 |  423 | `		bDetails = ph7_value_to_bool(apArg[1]);` |
|   ! 0 |  424 | `	}` |
|     3 |  425 | `	if( zExt ){` |
|     - |  426 | `		/* php reports the extensions it knows; anything else is a warning + false. */` |
|     - |  427 | `		static const char *azKnown[] = { "Core", "session", "date", "standard" };` |
|     3 |  428 | `		int bKnown = 0;` |
|     - |  429 | `		sxu32 k;` |
|     5 |  430 | `		for( k = 0 ; k < SX_ARRAYSIZE(azKnown) ; k++ ){` |
|     5 |  431 | `			if( (int)SyStrlen(azKnown[k]) == nExt && SyMemcmp(azKnown[k],zExt,(sxu32)nExt) == 0 ){` |
|     3 |  432 | `				bKnown = 1;` |
|     3 |  433 | `				break;` |
|     - |  434 | `			}` |
|     2 |  435 | `		}` |
|     3 |  436 | `		if( !bKnown ){` |
|   ! 0 |  437 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|   ! 0 |  438 | `				"Extension \"%.*s\" cannot be found",nExt,zExt);` |
|   ! 0 |  439 | `			ph7_result_bool(pCtx,0);` |
|   ! 0 |  440 | `			return PH7_OK;` |
|     - |  441 | `		}` |
|     1 |  442 | `	}` |
|     3 |  443 | `	pOut = ph7_context_new_array(pCtx);` |
|     3 |  444 | `	pCur = ph7_context_new_scalar(pCtx);` |
|     3 |  445 | `	if( pOut == 0 \|\| pCur == 0 ){` |
|   ! 0 |  446 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  447 | `	}` |
|     3 |  448 | `	SyBlobInit(&sVal,&pVm->sAllocator);` |
|     3 |  449 | `	aSlot = (VmIniSlot *)SySetBasePtr(&pVm->aIniTab);` |
|     - |  450 | `	/* The table is stored sorted, so this walk is already php's ksort order. */` |
|    65 |  451 | `	for( n = 0 ; n < SySetUsed(&pVm->aIniTab) ; n++ ){` |
|    63 |  452 | `		VmIniSlot *pSlot = &aSlot[n];` |
|     - |  453 | `		char zKey[128];` |
|    63 |  454 | `		if( zExt ){` |
|    63 |  455 | `			int bCore = (nExt == 4 && SyMemcmp(zExt,"Core",4) == 0)` |
|    62 |  456 | `				\|\| (nExt == 8 && SyMemcmp(zExt,"standard",8) == 0);` |
|    63 |  457 | `			if( !bCore ){` |
|     - |  458 | `				/* A named extension keeps only its own "<ext>." prefix. */` |
|    62 |  459 | `				if( pSlot->sName.nByte <= (sxu32)nExt` |
|    62 |  460 | `				 \|\| SyMemcmp(pSlot->sName.zString,zExt,(sxu32)nExt) != 0` |
|    34 |  461 | `				 \|\| pSlot->sName.zString[nExt] != '.' ){` |
|    59 |  462 | `					continue;` |
|     - |  463 | `				}` |
|     3 |  464 | `			}else{` |
|     - |  465 | `				/* Core/standard exclude the directives owned by a named extension. */` |
|   ! 0 |  466 | `				if( (pSlot->sName.nByte > sizeof("session.")-1` |
|   ! 0 |  467 | `				  && SyMemcmp(pSlot->sName.zString,"session.",sizeof("session.")-1) == 0)` |
|   ! 0 |  468 | `				 \|\| (pSlot->sName.nByte > sizeof("date.")-1` |
|   ! 0 |  469 | `				  && SyMemcmp(pSlot->sName.zString,"date.",sizeof("date.")-1) == 0) ){` |
|   ! 0 |  470 | `					continue;` |
|     - |  471 | `				}` |
|     - |  472 | `			}` |
|     2 |  473 | `		}` |
|     5 |  474 | `		if( pSlot->sName.nByte >= sizeof(zKey) ){` |
|   ! 0 |  475 | `			continue;` |
|     - |  476 | `		}` |
|     5 |  477 | `		SyMemcpy(pSlot->sName.zString,zKey,pSlot->sName.nByte);` |
|     5 |  478 | `		zKey[pSlot->sName.nByte] = 0;` |
|     5 |  479 | `		IniLiveGet(pVm,pSlot,&sVal);` |
|     5 |  480 | `		if( bDetails ){` |
|     5 |  481 | `			ph7_value *pRow = ph7_context_new_array(pCtx);` |
|     5 |  482 | `			if( pRow == 0 ){` |
|   ! 0 |  483 | `				break;` |
|     - |  484 | `			}` |
|     7 |  485 | `			ph7_value_string(pCur,(const char *)SyBlobData(&pSlot->sGlobal),` |
|     4 |  486 | `				(int)SyBlobLength(&pSlot->sGlobal));` |
|     5 |  487 | `			ph7_array_add_strkey_elem(pRow,"global_value",pCur);` |
|     5 |  488 | `			ph7_value_reset_string_cursor(pCur);` |
|     5 |  489 | `			ph7_value_string(pCur,(const char *)SyBlobData(&sVal),(int)SyBlobLength(&sVal));` |
|     5 |  490 | `			ph7_array_add_strkey_elem(pRow,"local_value",pCur);` |
|     5 |  491 | `			ph7_value_reset_string_cursor(pCur);` |
|     5 |  492 | `			ph7_value_int(pCur,pSlot->iAccess);` |
|     5 |  493 | `			ph7_array_add_strkey_elem(pRow,"access",pCur);` |
|     5 |  494 | `			ph7_value_reset_string_cursor(pCur);` |
|     5 |  495 | `			ph7_array_add_strkey_elem(pOut,zKey,pRow);` |
|     3 |  496 | `		}else{` |
|   ! 0 |  497 | `			ph7_value_reset_string_cursor(pCur);` |
|   ! 0 |  498 | `			ph7_value_string(pCur,(const char *)SyBlobData(&sVal),(int)SyBlobLength(&sVal));` |
|   ! 0 |  499 | `			ph7_array_add_strkey_elem(pOut,zKey,pCur);` |
|   ! 0 |  500 | `			ph7_value_reset_string_cursor(pCur);` |
|     - |  501 | `		}` |
|     3 |  502 | `	}` |
|     3 |  503 | `	SyBlobRelease(&sVal);` |
|     3 |  504 | `	ph7_result_value(pCtx,pOut);` |
|     3 |  505 | `	return PH7_OK;` |
|     2 |  506 | `}` |
|     - |  507 | `/* string\|false get_cfg_var(string $option) — php answers the GLOBAL value */` |
|     6 |  508 | `static int vm_builtin_get_cfg_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  509 | `{` |
|     7 |  510 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  511 | `	VmIniSlot *pSlot;` |
|     - |  512 | `	const char *zName;` |
|     7 |  513 | `	int nName = 0;` |
|     7 |  514 | `	if( nArg < 1 \|\| IniSeed(pVm) != SXRET_OK ){` |
|   ! 0 |  515 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  516 | `		return PH7_OK;` |
|     - |  517 | `	}` |
|     7 |  518 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|     7 |  519 | `	pSlot = IniFind(pVm,zName,(sxu32)nName);` |
|     7 |  520 | `	if( pSlot == 0 ){` |
|     3 |  521 | `		ph7_result_bool(pCtx,0);` |
|     3 |  522 | `		return PH7_OK;` |
|     - |  523 | `	}` |
|     7 |  524 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&pSlot->sGlobal),` |
|     4 |  525 | `		(int)SyBlobLength(&pSlot->sGlobal));` |
|     5 |  526 | `	return PH7_OK;` |
|     4 |  527 | `}` |
|     - |  528 | `/*` |
|     - |  529 | ` * The effective value of a directive a C builtin needs to obey, as an integer.` |
|     - |  530 | ` * parse_str() reads max_input_vars and max_input_nesting_level through this;` |
|     - |  531 | ` * without it a builtin would have to duplicate php's default and could never` |
|     - |  532 | `` * see an `ini_set()`/`-d` override. Answers iDefault when the directive is`` |
|     - |  533 | ` * absent or unparsable.` |
|     - |  534 | ` */` |
|   578 |  535 | `PH7_PRIVATE sxi64 PH7_VmIniGetInt(ph7_vm *pVm,const char *zName,sxi64 iDefault)` |
|     5 |  536 | `{` |
|     - |  537 | `	VmIniSlot *pSlot;` |
|     - |  538 | `	SyBlob sVal;` |
|   583 |  539 | `	sxi64 iVal = iDefault;` |
|   583 |  540 | `	IniSeed(pVm);` |
|   583 |  541 | `	pSlot = IniFind(pVm,zName,(sxu32)SyStrlen(zName));` |
|   583 |  542 | `	if( pSlot == 0 ){` |
|   ! 0 |  543 | `		return iDefault;` |
|     - |  544 | `	}` |
|   583 |  545 | `	SyBlobInit(&sVal,&pVm->sAllocator);` |
|   583 |  546 | `	IniLiveGet(pVm,pSlot,&sVal);` |
|   583 |  547 | `	if( SyBlobLength(&sVal) > 0 ){` |
|   583 |  548 | `		SyStrToInt64((const char *)SyBlobData(&sVal),SyBlobLength(&sVal),(void *)&iVal,0);` |
|   289 |  549 | `	}` |
|   583 |  550 | `	SyBlobRelease(&sVal);` |
|   583 |  551 | `	return iVal;` |
|   294 |  552 | `}` |
|     - |  553 | `/* The same, as a borrowed STRING (arg_separator.input). The bytes live in the` |
|     - |  554 | ` * caller's blob, which it owns. */` |
|   156 |  555 | `PH7_PRIVATE void PH7_VmIniGetStr(ph7_vm *pVm,const char *zName,SyBlob *pOut)` |
|     3 |  556 | `{` |
|     - |  557 | `	VmIniSlot *pSlot;` |
|   159 |  558 | `	IniSeed(pVm);` |
|   159 |  559 | `	SyBlobReset(pOut);` |
|   159 |  560 | `	pSlot = IniFind(pVm,zName,(sxu32)SyStrlen(zName));` |
|   159 |  561 | `	if( pSlot ){` |
|   147 |  562 | `		IniLiveGet(pVm,pSlot,pOut);` |
|    72 |  563 | `	}` |
|   159 |  564 | `}` |
|  4670 |  565 | `PH7_PRIVATE sxi32 PH7_VmInstallIni(ph7_vm *pVm)` |
|     5 |  566 | `{` |
|     - |  567 | `	static const struct {` |
|     - |  568 | `		const char *zName;` |
|     - |  569 | `		ProchHostFunction xFunc;` |
|     - |  570 | `	} aFunc[] = {` |
|     - |  571 | `		{ "ini_get",      vm_builtin_ini_get      },` |
|     - |  572 | `		{ "ini_set",      vm_builtin_ini_set      },` |
|     - |  573 | `		{ "ini_restore",  vm_builtin_ini_restore  },` |
|     - |  574 | `		{ "ini_get_all",  vm_builtin_ini_get_all  },` |
|     - |  575 | `		{ "get_cfg_var",  vm_builtin_get_cfg_var  },` |
|     - |  576 | `	};` |
|     - |  577 | `	sxu32 n;` |
| 28025 |  578 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFunc) ; n++ ){` |
| 23355 |  579 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
| 11680 |  580 | `	}` |
|  4675 |  581 | `	return SXRET_OK;` |
|     5 |  582 | `}` |
|     - |  583 | `#else` |
|     - |  584 | `PH7_PRIVATE sxi32 PH7_VmInstallIni(ph7_vm *pVm){ (void)pVm; return SXRET_OK; }` |
|     - |  585 | `PH7_PRIVATE sxi64 PH7_VmIniGetInt(ph7_vm *pVm,const char *zName,sxi64 iDefault){` |
|     - |  586 | `	(void)pVm; (void)zName; return iDefault;` |
|     - |  587 | `}` |
|     - |  588 | `PH7_PRIVATE void PH7_VmIniGetStr(ph7_vm *pVm,const char *zName,SyBlob *pOut){` |
|     - |  589 | `	(void)pVm; (void)zName; SyBlobReset(pOut);` |
|     - |  590 | `}` |
|     - |  591 | `#endif` |
|     - |  592 |  |
