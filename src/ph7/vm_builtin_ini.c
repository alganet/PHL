/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#ifndef PH7_DISABLE_BUILTIN_FUNC
/*
 * php.ini subsystem + INI API: a lazily seeded directive table (the static
 * defaults merged with the CLI's -d/-c entries from pVm->aIniCli) behind
 * ini_get / ini_set / ini_restore / ini_get_all / get_cfg_var. Live-wired
 * directives dispatch to the real knobs (error_reporting(), the session state,
 * the default timezone, the diagnostic gates) so the INI view and the engine
 * agree.
 *
 * This was an embedded-PHP chunk over two `__ini_*` C thunks, with the table
 * parked on a private `__IniS` class and five `__ini_*` PHP helpers alongside.
 * All eight of those names are gone: the table is a SySet on the VM and the five
 * functions ARE these C routines. The thunks did not become methods -- there is
 * no class here, only php's global functions -- so, like libxml's, they collapse
 * into the functions they were serving.
 *
 * One thing the move fixes on its own: a diagnostic raised by a prelude function
 * reported the CHUNK's line, so every ini_set()/ini_get_all() warning said
 * "on line 1" regardless of the caller. A C builtin reports the caller's line,
 * which is what php prints.
 */

/* Directive access levels, as php reports them in ini_get_all()['access']. */
#define VM_INI_USER    1
#define VM_INI_PERDIR  2
#define VM_INI_SYSTEM  4
#define VM_INI_ALL     (VM_INI_USER|VM_INI_PERDIR|VM_INI_SYSTEM)

/*
 * The defaults, in the order php's ini_get_all() reports them (sorted by name).
 * Keeping this list sorted is what lets ini_get_all() skip a sort: the seed
 * inserts any CLI-only directive in its sorted position.
 */
static const struct {
	const char *zName;
	const char *zValue;   /* 0 = php's UNSET directive, which reports NULL and
	                       * is not the same thing as "" */
	sxi32 iAccess;
} aIniDefault[] = {
	{ "allow_url_fopen",          "1",          VM_INI_SYSTEM },
	/* php's default is OFF, and it spells that default as the EMPTY string — which
	 * is what ini_get() answers. Including a remote file is the classic RFI, and
	 * this is what a STREAM_IS_URL wrapper's include is gated on. */
	{ "allow_url_include",        "",           VM_INI_SYSTEM },
	{ "arg_separator.input",      "&",          VM_INI_PERDIR|VM_INI_SYSTEM },
	{ "arg_separator.output",     "&",          VM_INI_ALL },
	{ "auto_detect_line_endings", "",           VM_INI_ALL },
	/* ext/bcmath's only directive: the scale every bc* function defaults its
	 * $scale argument from, and the one slot bcscale() reads and writes. */
	{ "bcmath.scale",             "0",          VM_INI_ALL },
	{ "date.timezone",            "UTC",        VM_INI_ALL },
	{ "default_charset",          "UTF-8",      VM_INI_ALL },
	/* php bounds a socket wait by this rather than waiting forever, and it is
	 * where stream_socket_accept() takes its default timeout from. */
	{ "default_socket_timeout",   "60",         VM_INI_ALL },
	{ "default_mimetype",         "text/html",  VM_INI_ALL },
	{ "display_errors",           "",           VM_INI_ALL },
	{ "error_log",                0,            VM_INI_ALL },
	{ "error_reporting",          "30719",      VM_INI_ALL },
	/* The From: header the http:// wrapper writes. php ships it UNSET, and the
	 * difference matters: an unset directive writes no header at all, while an
	 * ini_set() to the EMPTY string writes `From: ` -- which is not how
	 * user_agent below behaves. */
	{ "from",                     0,            VM_INI_ALL },
	{ "highlight.comment",        "#FF8000",    VM_INI_ALL },
	{ "highlight.default",        "#0000BB",    VM_INI_ALL },
	{ "highlight.html",           "#000000",    VM_INI_ALL },
	{ "highlight.keyword",        "#007700",    VM_INI_ALL },
	{ "highlight.string",         "#DD0000",    VM_INI_ALL },
	/* ignore_user_abort(): the directive the function reads and writes. php
	 * spells its default "0" rather than the empty string every other boolean
	 * directive here uses, and ini_get() answers that byte. */
	{ "ignore_user_abort",        "0",          VM_INI_ALL },
	{ "include_path",             ".",          VM_INI_ALL },
	{ "log_errors",               "1",          VM_INI_ALL },
	{ "max_execution_time",       "0",          VM_INI_ALL },
	{ "max_input_nesting_level",  "64",         VM_INI_PERDIR|VM_INI_SYSTEM },
	{ "max_input_vars",           "1000",       VM_INI_PERDIR|VM_INI_SYSTEM },
	{ "memory_limit",             "-1",         VM_INI_ALL },
#ifdef PH7_ENABLE_OPENSSL
	/* ext/openssl's two path directives, php's own defaults (both UNSET, the
	 * empty string) and its access mask (PHP_INI_PERDIR for both). They are
	 * what openssl_get_cert_locations() reports as ini_cafile/ini_capath and
	 * what a TLS peer verification falls back to when a stream context names
	 * no CA of its own. Both ship UNSET rather than empty, which is what php
	 * reports and is a different thing: ini_get_all() answers NULL for an
	 * unset directive and "" for one set to the empty string. php's third, `openssl.libctx`, is not here: it picks
	 * between the default OpenSSL library context and a private one, and this
	 * build has only the default -- registering the name would report a
	 * choice that is not being made. */
	{ "openssl.cafile",           0,            VM_INI_PERDIR },
	{ "openssl.capath",           0,            VM_INI_PERDIR },
#endif
	/* ext/phar's three. `phar.readonly` is php's own default ON: every write
	 * door refuses until an installer turns it off, which is why building an
	 * archive is a `-d phar.readonly=0` job on a stock php too. */
	{ "phar.cache_list",          "",           VM_INI_SYSTEM },
	{ "phar.readonly",            "1",          VM_INI_ALL },
	{ "phar.require_hash",        "1",          VM_INI_ALL },
	{ "post_max_size",            "8M",         VM_INI_PERDIR|VM_INI_SYSTEM },
	{ "precision",                "14",         VM_INI_ALL },
	{ "serialize_precision",      "-1",         VM_INI_ALL },
	/* php's session directives, the whole non-deprecated set: session_start()'s
	 * $options array applies its keys THROUGH this table, so a directive missing
	 * here is an option php accepts and PHL reports as failed.
	 * Six are absent on purpose: php 8.4 DEPRECATES session.sid_length,
	 * session.sid_bits_per_character, session.referer_check, session.use_trans_sid,
	 * session.trans_sid_tags and session.trans_sid_hosts, and §10 does not carry
	 * php's deprecated surface. The session.upload_progress.* family goes with the
	 * file uploads §10 excludes from a CLI-plus-`-S` engine. */
	{ "session.auto_start",       "0",          VM_INI_PERDIR },
	{ "session.cache_expire",     "180",        VM_INI_ALL },
	{ "session.cache_limiter",    "nocache",    VM_INI_ALL },
	/* The Set-Cookie the session sends is built out of these seven. */
	{ "session.cookie_domain",    "",           VM_INI_ALL },
	{ "session.cookie_httponly",  "0",          VM_INI_ALL },
	{ "session.cookie_lifetime",  "0",          VM_INI_ALL },
	{ "session.cookie_partitioned","0",         VM_INI_ALL },
	{ "session.cookie_path",      "/",          VM_INI_ALL },
	{ "session.cookie_samesite",  "",           VM_INI_ALL },
	{ "session.cookie_secure",    "0",          VM_INI_ALL },
	{ "session.gc_divisor",       "100",        VM_INI_ALL },
	{ "session.gc_maxlifetime",   "1440",       VM_INI_ALL },
	{ "session.gc_probability",   "1",          VM_INI_ALL },
	{ "session.lazy_write",       "1",          VM_INI_ALL },
	{ "session.name",             "PHPSESSID",  VM_INI_ALL },
	{ "session.save_handler",     "files",      VM_INI_ALL },
	{ "session.save_path",        "",           VM_INI_ALL },
	/* Which of php's three session serializers writes the store: `php` (the
	 * `name|<serialized>` runs a stock php install reads), `php_binary` or
	 * `php_serialize`. */
	{ "session.serialize_handler","php",        VM_INI_ALL },
	/* Whether the session sends and reads its id as a cookie at all. PHL has never
	 * read an id from anywhere ELSE, which is what use_only_cookies means. */
	{ "session.use_cookies",      "1",          VM_INI_ALL },
	{ "session.use_only_cookies", "1",          VM_INI_ALL },
	{ "session.use_strict_mode",  "0",          VM_INI_ALL },
	{ "short_open_tag",           "",           VM_INI_PERDIR|VM_INI_SYSTEM },
	/* php's three syslog directives, with php's defaults and php's access masks.
	 * `syslog.filter` is the one this engine READS: it decides which bytes
	 * syslog() escapes and is PHP_INI_ALL, so a script can change it. The other
	 * two are what php's own error logger uses when `error_log = syslog`, a
	 * target this build does not have -- they are declared because ini_get() and
	 * ini_get_all() answer them under php and a program can read either. */
	{ "syslog.facility",          "LOG_USER",   VM_INI_SYSTEM },
	{ "syslog.filter",            "no-ctrl",    VM_INI_ALL },
	{ "syslog.ident",             "php",        VM_INI_SYSTEM },
#ifdef PH7_ENABLE_SQLITE
	/* ext/sqlite3's two directives, in this sorted list's own place. `defensive`
	 * is applied to every connection SQLite3 opens (it is what makes an UPDATE of
	 * sqlite_master refuse, even behind `PRAGMA writable_schema=ON`), and
	 * `extension_dir` is the door loadExtension() is shut behind: empty means
	 * "SQLite Extensions are disabled", and php ships it empty. The access masks
	 * are php's own, which do not agree with each other. */
	{ "sqlite3.defensive",        "1",          VM_INI_USER },
	{ "sqlite3.extension_dir",    0,            VM_INI_SYSTEM },
#endif
	{ "unserialize_callback_func","",           VM_INI_ALL },
	{ "unserialize_max_depth",    "4096",       VM_INI_ALL },
	{ "upload_max_filesize",      "2M",         VM_INI_PERDIR|VM_INI_SYSTEM },
	/* The User-Agent the http:// wrapper writes when the request names none.
	 * UNSET like `from` above, and for the same reason -- but the wrapper reads
	 * the two differently: an empty user_agent writes no header at all, while an
	 * empty `from` writes `From: `. */
	{ "user_agent",               0,            VM_INI_ALL },
	{ "zend.assertions",          "-1",         VM_INI_ALL },
#ifdef PH7_ENABLE_ZLIB
	/* ext/zlib's three, php's own defaults and its access mask (all three are
	 * PHP_INI_ALL). They are READ by ob_gzhandler() and zlib_get_coding_type()
	 * and by nothing else: php's output-layer compression is a SAPI feature a
	 * command line never turns on, and neither does this. */
	{ "zlib.output_compression",  "",           VM_INI_ALL },
	{ "zlib.output_compression_level","-1",     VM_INI_ALL },
	{ "zlib.output_handler",      "",           VM_INI_ALL },
#endif
};

static int IniNameIs(const VmIniSlot *pSlot,const char *zName)
{
	sxu32 n = (sxu32)SyStrlen(zName);
	return pSlot->sName.nByte == n && SyMemcmp(pSlot->sName.zString,zName,n) == 0;
}
/*
 * zend_ini_parse_bool semantics, matching the C-side VmIniBool the -d/-c path
 * uses: on/yes/true, else a non-zero integer parse.
 */
static int IniTruthy(const char *zVal,sxu32 nVal)
{
	while( nVal > 0 && (zVal[0] == ' ' || zVal[0] == '\t') ){ zVal++; nVal--; }
	while( nVal > 0 && (zVal[nVal-1] == ' ' || zVal[nVal-1] == '\t') ){ nVal--; }
	if( nVal == 2 && SyStrnicmp(zVal,"on",2) == 0 ){ return 1; }
	if( nVal == 3 && SyStrnicmp(zVal,"yes",3) == 0 ){ return 1; }
	if( nVal == 4 && SyStrnicmp(zVal,"true",4) == 0 ){ return 1; }
	{
		sxi32 iVal = 0;
		if( nVal > 0 && SyStrToInt32(zVal,nVal,(void *)&iVal,0) == SXRET_OK ){
			return iVal != 0;
		}
	}
	return 0;
}
/*
 * The value rules that apply to a `-d name=value` on the COMMAND LINE, which are
 * not the same set IniValueAccepted enforces on ini_set(). php runs each
 * directive's OnUpdate handler at startup too, but several of those refuse only
 * at RUNTIME -- `-d session.serialize_handler=bogus` is taken by php because the
 * serializer table it would look the name up in is still empty -- so this is a
 * per-directive list rather than a shared screen. A value refused here leaves the
 * directive at its default, which is what php reports.
 */
static int IniStartupValueAccepted(const SyString *pName,const char *zVal,sxu32 nVal)
{
	if( pName->nByte == sizeof("syslog.filter")-1
	 && SyMemcmp(pName->zString,"syslog.filter",sizeof("syslog.filter")-1) == 0 ){
		return (nVal == 3 && SyMemcmp(zVal,"all",3) == 0)
		    || (nVal == 7 && SyMemcmp(zVal,"no-ctrl",7) == 0)
		    || (nVal == 5 && SyMemcmp(zVal,"ascii",5) == 0)
		    || (nVal == 3 && SyMemcmp(zVal,"raw",3) == 0);
	}
	return 1;
}
/*
 * Build the table: the static defaults, then the CLI queue merged over them (an
 * unknown CLI name is appended as a new INI_ALL directive, as the chunk did),
 * then sorted by name so ini_get_all() can walk it in php's order without a sort.
 */
static sxi32 IniSeed(ph7_vm *pVm)
{
	sxu32 i,j;
	VmIniEntry *aCli;
	VmIniSlot *aSlot;
	if( pVm->bIniSeeded ){
		return SXRET_OK;
	}
	pVm->bIniSeeded = 1; /* set FIRST: the live-wired writes below re-enter nothing,
	                      * but a future one must never recurse into the seed */
	for( i = 0 ; i < SX_ARRAYSIZE(aIniDefault) ; i++ ){
		VmIniSlot sSlot;
		SyStringInitFromBuf(&sSlot.sName,aIniDefault[i].zName,SyStrlen(aIniDefault[i].zName));
		sSlot.iAccess = aIniDefault[i].iAccess;
		SyBlobInit(&sSlot.sGlobal,&pVm->sAllocator);
		SyBlobInit(&sSlot.sLocal,&pVm->sAllocator);
		/* A row with no value at all is php's UNSET directive, which is not the
		 * empty string: it reports NULL everywhere the raw value is shown. */
		sSlot.bGlobalNull = sSlot.bLocalNull = aIniDefault[i].zValue ? 0 : 1;
		if( aIniDefault[i].zValue ){
			SyBlobAppend(&sSlot.sGlobal,aIniDefault[i].zValue,
				(sxu32)SyStrlen(aIniDefault[i].zValue));
			SyBlobAppend(&sSlot.sLocal,aIniDefault[i].zValue,
				(sxu32)SyStrlen(aIniDefault[i].zValue));
		}
		if( SySetPut(&pVm->aIniTab,(const void *)&sSlot) != SXRET_OK ){
			return SXERR_MEM;
		}
	}
	aCli = (VmIniEntry *)SySetBasePtr(&pVm->aIniCli);
	for( i = 0 ; i < SySetUsed(&pVm->aIniCli) ; i++ ){
		int bFound = 0;
		if( !IniStartupValueAccepted(&aCli[i].sName,aCli[i].sValue.zString,
			aCli[i].sValue.nByte) ){
			continue;   /* the directive keeps its default, as it does under php */
		}
		aSlot = (VmIniSlot *)SySetBasePtr(&pVm->aIniTab);
		for( j = 0 ; j < SySetUsed(&pVm->aIniTab) ; j++ ){
			if( aSlot[j].sName.nByte == aCli[i].sName.nByte
			 && SyMemcmp(aSlot[j].sName.zString,aCli[i].sName.zString,aCli[i].sName.nByte) == 0 ){
				SyBlobReset(&aSlot[j].sGlobal);
				SyBlobReset(&aSlot[j].sLocal);
				SyBlobAppend(&aSlot[j].sGlobal,aCli[i].sValue.zString,aCli[i].sValue.nByte);
				SyBlobAppend(&aSlot[j].sLocal,aCli[i].sValue.zString,aCli[i].sValue.nByte);
				/* `-d name=` names the directive on the command line, so what it
				 * carries is a value -- the empty one, never the unset state. */
				aSlot[j].bGlobalNull = aSlot[j].bLocalNull = 0;
				bFound = 1;
				break;
			}
		}
		if( !bFound ){
			VmIniSlot sSlot;
			/* aIniCli holds VM-lifetime copies already, so the name can be aliased. */
			sSlot.sName = aCli[i].sName;
			sSlot.iAccess = VM_INI_ALL;
			sSlot.bGlobalNull = sSlot.bLocalNull = 0;
			SyBlobInit(&sSlot.sGlobal,&pVm->sAllocator);
			SyBlobInit(&sSlot.sLocal,&pVm->sAllocator);
			SyBlobAppend(&sSlot.sGlobal,aCli[i].sValue.zString,aCli[i].sValue.nByte);
			SyBlobAppend(&sSlot.sLocal,aCli[i].sValue.zString,aCli[i].sValue.nByte);
			if( SySetPut(&pVm->aIniTab,(const void *)&sSlot) != SXRET_OK ){
				return SXERR_MEM;
			}
		}
	}
	/* Insertion sort by name (the table is ~30 entries and already nearly sorted:
	 * only CLI-introduced directives are out of place). */
	aSlot = (VmIniSlot *)SySetBasePtr(&pVm->aIniTab);
	for( i = 1 ; i < SySetUsed(&pVm->aIniTab) ; i++ ){
		VmIniSlot sTmp = aSlot[i];
		j = i;
		while( j > 0 ){
			const VmIniSlot *pPrev = &aSlot[j-1];
			sxu32 nMin = pPrev->sName.nByte < sTmp.sName.nByte ? pPrev->sName.nByte : sTmp.sName.nByte;
			sxi32 iCmp = SyMemcmp(pPrev->sName.zString,sTmp.sName.zString,nMin);
			if( iCmp == 0 ){
				iCmp = (sxi32)pPrev->sName.nByte - (sxi32)sTmp.sName.nByte;
			}
			if( iCmp <= 0 ){
				break;
			}
			aSlot[j] = aSlot[j-1];
			j--;
		}
		aSlot[j] = sTmp;
	}
	/* Boot-apply the CLI values for the live-wired session knobs. The engine knobs
	 * (error_reporting / date.timezone) were already applied C-side by
	 * PH7_VM_CONFIG_INI_ENTRY. */
	aSlot = (VmIniSlot *)SySetBasePtr(&pVm->aIniTab);
	for( i = 0 ; i < SySetUsed(&pVm->aIniTab) ; i++ ){
		SyBlob *pDst = 0;
		if( IniNameIs(&aSlot[i],"session.name") ){
			if( SyBlobLength(&aSlot[i].sGlobal) != sizeof("PHPSESSID")-1
			 || SyMemcmp(SyBlobData(&aSlot[i].sGlobal),"PHPSESSID",sizeof("PHPSESSID")-1) != 0 ){
				pDst = &pVm->sSessName;
			}
		}else if( IniNameIs(&aSlot[i],"session.save_path") ){
			if( SyBlobLength(&aSlot[i].sGlobal) > 0 ){
				pDst = &pVm->sSessPath;
			}
		}
		if( pDst ){
			sxu32 nLen = SyBlobLength(&aSlot[i].sGlobal);
			const char *zVal = (const char *)SyBlobData(&aSlot[i].sGlobal);
			while( nLen > 0 && zVal[nLen-1] == '/' ){ nLen--; } /* rtrim('/') */
			SyBlobReset(pDst);
			SyBlobAppend(pDst,zVal,nLen);
		}
	}
	return SXRET_OK;
}
static VmIniSlot * IniFind(ph7_vm *pVm,const char *zName,sxu32 nName)
{
	VmIniSlot *aSlot = (VmIniSlot *)SySetBasePtr(&pVm->aIniTab);
	sxu32 n;
	for( n = 0 ; n < SySetUsed(&pVm->aIniTab) ; n++ ){
		if( aSlot[n].sName.nByte == nName
		 && SyMemcmp(aSlot[n].sName.zString,zName,nName) == 0 ){
			return &aSlot[n];
		}
	}
	return 0;
}
/*
 * The EFFECTIVE current value. For a live-wired directive the runtime knob is
 * the truth, not the stored local value, so that ini_get() and the knob's own
 * accessor can never disagree.
 */
static void IniLiveGet(ph7_vm *pVm,VmIniSlot *pSlot,SyBlob *pOut)
{
	SyBlobReset(pOut);
	if( IniNameIs(pSlot,"error_reporting") ){
		char zBuf[32];
		int nBuf = SyBufferFormat(zBuf,sizeof(zBuf),"%d",
			pVm->bErrReport ? (int)pVm->iErrMask : 0);
		SyBlobAppend(pOut,zBuf,(sxu32)nBuf);
		return;
	}
	if( IniNameIs(pSlot,"session.name") ){
		SyBlobAppend(pOut,SyBlobData(&pVm->sSessName),SyBlobLength(&pVm->sSessName));
		return;
	}
	if( IniNameIs(pSlot,"session.save_path") && SyBlobLength(&pVm->sSessPath) > 0 ){
		SyBlobAppend(pOut,SyBlobData(&pVm->sSessPath),SyBlobLength(&pVm->sSessPath));
		return;
	}
	if( IniNameIs(pSlot,"include_path") ){
		/* The VM's path SET is the store; this directive is a view of it, so
		 * ini_get() and get_include_path() can never name different paths. */
		PH7_VmGetIncludePath(pVm,pOut);
		return;
	}
	SyBlobAppend(pOut,SyBlobData(&pSlot->sLocal),SyBlobLength(&pSlot->sLocal));
}
/*
 * Push a new value at the runtime knob behind a live-wired directive. The stored
 * local value is updated by the caller either way.
 */
/*
 * php's byte shorthand: a plain integer, optionally suffixed K, M or G (case
 * insensitive, no "B"). `-1` -- and any negative -- means UNLIMITED, which is the
 * CLI default. Anything unparseable reads as 0, which php also treats as
 * "allocate nothing", so it is left to say exactly that rather than being
 * silently promoted to unlimited.
 */
static sxu32 IniParseBytes(const char *zVal,sxu32 nVal,int *pbUnlimited)
{
	sxi64 iVal = 0;
	sxu32 n = 0;
	int bNeg = 0;
	*pbUnlimited = 0;
	while( n < nVal && (zVal[n] == ' ' || zVal[n] == '\t') ){ n++; }
	if( n < nVal && (zVal[n] == '-' || zVal[n] == '+') ){
		bNeg = (zVal[n] == '-');
		n++;
	}
	while( n < nVal && zVal[n] >= '0' && zVal[n] <= '9' ){
		iVal = iVal * 10 + (zVal[n] - '0');
		if( iVal > (sxi64)0x7FFFFFFF ){ iVal = (sxi64)0x7FFFFFFF; } /* clamp: the field is 32-bit */
		n++;
	}
	if( bNeg ){
		*pbUnlimited = 1;   /* php: any negative memory_limit is "no limit" */
		return 0;
	}
	if( n < nVal ){
		sxi64 nMul = 0;
		switch( zVal[n] ){
			case 'k': case 'K': nMul = 1024; break;
			case 'm': case 'M': nMul = 1024 * 1024; break;
			case 'g': case 'G': nMul = 1024 * 1024 * 1024; break;
			default: nMul = 0; break;
		}
		if( nMul > 0 ){
			iVal = (iVal > (sxi64)0x7FFFFFFF / nMul) ? (sxi64)0x7FFFFFFF : iVal * nMul;
		}
	}
	return (sxu32)iVal;
}
/*
 * Arm the allocator's total live-byte ceiling from a memory_limit value, and answer
 * whether it took. THE one place the directive is interpreted: ini_set() reaches it
 * through the validator and `-d name=value` reaches it directly, and a rule that
 * lived in only one of those would hold for one door and not the other.
 *
 * The one directive that reaches into the ALLOCATOR. php enforces a ceiling and
 * kills the script with a fatal when a request would cross it; PHL stored the
 * string and enforced nothing, so a runaway allocation -- a reference cycle nothing
 * reclaims is the usual way in (PLAN.md §5) -- had no ceiling below the kernel's, and
 * the OOM killer took the whole process instead of the script. On a shared box that
 * is not the script's problem any more: it is everything else's.
 *
 * Unlimited is the CLI default here as it is in php, so a plain run is unchanged.
 */
PH7_PRIVATE int PH7_VmApplyMemoryLimit(ph7_vm *pVm,const char *zVal,sxu32 nVal)
{
	int bUnlimited = 0;
	sxu32 nBytes = IniParseBytes(zVal,nVal,&bUnlimited);
	if( !bUnlimited && nBytes > 0 && nBytes < pVm->sAllocator.nMemUsed ){
		/* php REFUSES to lower the ceiling below what is already in use --
		 * zend_set_memory_limit answers FAILURE -- and it does so from the
		 * directive's own OnUpdate handler, which is why the rule holds at STARTUP
		 * (`-d memory_limit=8K` warns and runs on) exactly as it holds for
		 * ini_set(). Arming a ceiling the interpreter is already past is not a
		 * limit, it is a delayed crash: the very next allocation is fatal.
		 *
		 * Without the rule, a library that PROBES the limit by setting a small one
		 * dies instead of learning that it cannot -- monolog's StreamHandler sizes
		 * its write chunk exactly that way, and its test walks 1M, 10M, 1024M, 3G
		 * in turn, reading the false back and skipping.
		 *
		 * php prints this one WITHOUT the `ini_set(): ` prefix its other ini
		 * warnings carry, because it comes from the handler and not from the call. */
		char zMsg[160];
		SyBufferFormat(zMsg,sizeof(zMsg),
			"Failed to set memory limit to %u bytes (Current memory usage is %u bytes)",
			nBytes,pVm->sAllocator.nMemUsed);
		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);
		return 0;
	}
	pVm->sAllocator.nMemLimit = bUnlimited ? 0 : nBytes;
	pVm->sAllocator.nMemLimitHit = 0;
	pVm->sAllocator.nMemTried = 0;
	return 1;
}
static void IniLiveSet(ph7_vm *pVm,VmIniSlot *pSlot,const char *zVal,sxu32 nVal)
{
	if( IniNameIs(pSlot,"error_reporting") ){
		sxi32 iVal = 0;
		if( nVal > 0 ){
			SyStrToInt32(zVal,nVal,(void *)&iVal,0);
		}
		pVm->iErrMask = iVal;
		pVm->bErrReport = iVal != 0;
		pVm->bErrMaskSet = 1;
		return;
	}
	if( IniNameIs(pSlot,"memory_limit") ){
		(void)PH7_VmApplyMemoryLimit(pVm,zVal,nVal);
		return;
	}
	if( IniNameIs(pSlot,"display_errors") ){
		pVm->bDisplayErrors = IniTruthy(zVal,nVal);
		return;
	}
	if( IniNameIs(pSlot,"log_errors") ){
		pVm->bLogErrors = IniTruthy(zVal,nVal);
		return;
	}
	if( IniNameIs(pSlot,"session.name") || IniNameIs(pSlot,"session.save_path") ){
		int bPath = IniNameIs(pSlot,"session.save_path");
		SyBlob *pDst = bPath ? &pVm->sSessPath : &pVm->sSessName;
		sxu32 nLen = nVal;
		if( bPath ){
			while( nLen > 0 && zVal[nLen-1] == '/' ){ nLen--; }
		}
		SyBlobReset(pDst);
		SyBlobAppend(pDst,zVal,nLen);
		return;
	}
	if( IniNameIs(pSlot,"include_path") ){
		/* The one write that moves the include walk. Refused empty by the caller
		 * (php's OnUpdateStringUnempty), so nVal is never 0 on the ini_set() path;
		 * the boot/-d and ini_restore() paths carry a real value too. */
		PH7_VmSetIncludePath(pVm,zVal,nVal);
		return;
	}
	if( IniNameIs(pSlot,"date.timezone") ){
		/* Only UTC/GMT exist here (no tz database), matching the engine's own
		 * date_default_timezone_set(). */
		if( nVal == 3 && (SyStrnicmp(zVal,"UTC",3) == 0 || SyStrnicmp(zVal,"GMT",3) == 0) ){
			SyMemcpy(zVal,pVm->zDefTz,3);
			pVm->zDefTz[3] = 0;
			pVm->nDefTz = 3;
		}
		return;
	}
}
/*
 * A session directive is settable only while there is no session to disturb: not
 * once one is ACTIVE (the store is open and the cookie decided), and not once
 * headers have gone out. Answers TRUE (having raised the warning) when the write
 * must be refused.
 */
static int IniSessionLocked(ph7_context *pCtx,VmIniSlot *pSlot,const char *zFunc)
{
	ph7_vm *pVm = pCtx->pVm;
	char zMsg[160];
	const char *zWhy;
	if( pSlot->sName.nByte < sizeof("session.")-1
	 || SyMemcmp(pSlot->sName.zString,"session.",sizeof("session.")-1) != 0 ){
		return 0;
	}
	int bActive = (pVm->iSessStatus == 2 /* PHP_SESSION_ACTIVE */);
	if( bActive ){
		zWhy = "when a session is active";
	}else if( pVm->bHeadersSent ){
		zWhy = "after headers have already been sent";
	}else{
		return 0;
	}
	SyBufferFormat(zMsg,sizeof(zMsg),
		"%s(): Session ini settings cannot be changed %s",zFunc,zWhy);
	{
		/* php names the session_start() or the output behind the refusal. */
		SyBlob sMsg;
		SyBlobInit(&sMsg,&pVm->sAllocator);
		SyBlobAppend(&sMsg,zMsg,(sxu32)SyStrlen(zMsg));
		PH7_VmAppendWhere(pVm,&sMsg,bActive);
		SyBlobNullAppend(&sMsg);
		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));
		SyBlobRelease(&sMsg);
	}
	return 1;
}
/*
 * The per-directive value rules php enforces on every write, and the diagnostic
 * each one raises. zWho is the whole prefix php puts on it -- "ini_set()" or, when
 * session_start() is applying its $options array, "session_start()" -- because php
 * blames the call that made the write, not the API underneath it.
 */
static int IniValueAccepted(ph7_vm *pVm,VmIniSlot *pSlot,const char *zVal,sxu32 nVal,
	const char *zWho)
{
	char zMsg[256];
	if( IniNameIs(pSlot,"include_path") && nVal < 1 ){
		/* php registers include_path with OnUpdateStringUnempty: the EMPTY value
		 * is refused in silence and the directive keeps what it had. */
		return 0;
	}
	if( IniNameIs(pSlot,"memory_limit") ){
		/* One rule, one warning, one place: the applier owns both, so ini_set() and
		 * the `-d` startup path cannot drift apart. Arming from the validator is
		 * idempotent -- an ACCEPTED value is then written and re-applied through
		 * IniLiveSet with the same bytes, and a refused one is never written. */
		return PH7_VmApplyMemoryLimit(pVm,zVal,nVal);
	}
	if( IniNameIs(pSlot,"syslog.filter")
	 && !(nVal == 3 && SyMemcmp(zVal,"all",3) == 0)
	 && !(nVal == 7 && SyMemcmp(zVal,"no-ctrl",7) == 0)
	 && !(nVal == 5 && SyMemcmp(zVal,"ascii",5) == 0)
	 && !(nVal == 3 && SyMemcmp(zVal,"raw",3) == 0) ){
		/* php names the four modes in its OnUpdate handler and refuses anything
		 * else in silence, keeping what the directive had -- so
		 * `ini_set('syslog.filter','bogus')` is false and reads back unchanged.
		 * The match is CASE-SENSITIVE there: `ASCII` is refused where `ascii` is
		 * taken, which is not what most of php's word-valued directives do. */
		return 0;
	}
	if( IniNameIs(pSlot,"bcmath.scale") ){
		/* php registers it with a 0..INT_MAX bound and refuses anything outside in
		 * silence, keeping what the directive had -- `ini_set('bcmath.scale','-1')`
		 * is false there. A NON-numeric value is a different matter and is
		 * ACCEPTED (stored verbatim, read back as 0), which falls out of the parse
		 * below without a rule of its own. */
		sxi64 iVal = 0;
		SyStrToInt64(zVal,nVal,(void *)&iVal,0);
		if( iVal < 0 || iVal > 2147483647 ){
			return 0;
		}
	}
	if( IniNameIs(pSlot,"session.serialize_handler")
	 && !(nVal == 3 && SyMemcmp(zVal,"php",3) == 0)
	 && !(nVal == 10 && SyMemcmp(zVal,"php_binary",10) == 0)
	 && !(nVal == 13 && SyMemcmp(zVal,"php_serialize",13) == 0) ){
		/* php looks the name up in its registered serializer list and refuses what
		 * it cannot find, keeping the directive where it was. */
		SyBufferFormat(zMsg,sizeof(zMsg),
			"%s: Serialization handler \"%.*s\" cannot be found",zWho,(int)nVal,zVal);
		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);
		return 0;
	}
	if( IniNameIs(pSlot,"session.name") ){
		/* The name goes out as a COOKIE name and comes back as one, so php holds it
		 * to the cookie alphabet -- and refuses a numeric one, which a browser would
		 * hand back as an integer array key. */
		static const char zBad[] = "=,;.[ \t\r\n\013\014";
		sxu32 i;
		int bBad = nVal < 1;
		for( i = 0 ; !bBad && i < nVal ; i++ ){
			if( zVal[i] == 0 || SyByteFind(zBad,sizeof(zBad)-1,zVal[i],0) == SXRET_OK ){
				bBad = 1;
			}
		}
		if( !bBad ){
			sxi64 iDummy = 0;
			bBad = SyStrToInt64(zVal,nVal,(void *)&iDummy,0) == SXRET_OK;
		}
		if( bBad ){
			SyBufferFormat(zMsg,sizeof(zMsg),
				"%s: session.name \"%.*s\" must not be numeric, empty, contain null bytes"
				" or any of the following characters \"=,;.[ \\t\\r\\n\\013\\014\"",
				zWho,(int)nVal,zVal);
			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);
			return 0;
		}
	}
	return 1;
}
/*
 * Store one written value.  A write always leaves a local value; what it does to
 * the GLOBAL one depends on whether there was ever a global to keep. php saves
 * the original at the first modification and reports THAT as global_value -- but
 * an UNSET directive has no original to save, so its global_value starts reading
 * the written value instead, and only ini_restore() puts the NULL back.
 */
static void IniWriteLocal(VmIniSlot *pSlot,const char *zVal,sxu32 nVal)
{
	SyBlobReset(&pSlot->sLocal);
	SyBlobAppend(&pSlot->sLocal,zVal,nVal);
	pSlot->bLocalNull = 0;
}
/*
 * What global_value reports: the saved original, unless there was none -- in
 * which case it is whatever the directive currently holds, NULL included.
 */
static int IniGlobalValue(VmIniSlot *pSlot,const char **pz,sxu32 *pn)
{
	if( !pSlot->bGlobalNull ){
		*pz = (const char *)SyBlobData(&pSlot->sGlobal);
		*pn = SyBlobLength(&pSlot->sGlobal);
		return 1;
	}
	if( pSlot->bLocalNull ){
		return 0;   /* still unset: php reports NULL */
	}
	*pz = (const char *)SyBlobData(&pSlot->sLocal);
	*pn = SyBlobLength(&pSlot->sLocal);
	return 1;
}
/*
 * Write a directive from C, the way ini_set() writes it. Answers 0 when the write
 * was refused (unknown name, not user-settable, or a value the directive's own
 * rule rejects) -- which is exactly what session_start()'s $options reports as
 * `Setting option "%s" failed`.
 */
PH7_PRIVATE int PH7_VmIniSet(ph7_vm *pVm,const char *zName,sxu32 nName,
	const char *zVal,sxu32 nVal,const char *zWho)
{
	VmIniSlot *pSlot;
	if( IniSeed(pVm) != SXRET_OK ){
		return 0;
	}
	pSlot = IniFind(pVm,zName,nName);
	if( pSlot == 0 || (pSlot->iAccess & VM_INI_USER) == 0 ){
		return 0;
	}
	if( !IniValueAccepted(pVm,pSlot,zVal,nVal,zWho) ){
		return 0;
	}
	IniWriteLocal(pSlot,zVal,nVal);
	IniLiveSet(pVm,pSlot,zVal,nVal);
	return 1;
}
/* string|false ini_get(string $option) */
static int vm_builtin_ini_get(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	VmIniSlot *pSlot;
	const char *zName;
	int nName = 0;
	SyBlob sOut;
	if( nArg < 1 || IniSeed(pVm) != SXRET_OK ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zName = ph7_value_to_string(apArg[0],&nName);
	pSlot = IniFind(pVm,zName,(sxu32)nName);
	if( pSlot == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SyBlobInit(&sOut,&pVm->sAllocator);
	IniLiveGet(pVm,pSlot,&sOut);
	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
	SyBlobRelease(&sOut);
	return PH7_OK;
}
/* string|false ini_set(string $option, string|int|float|bool|null $value) */
static int vm_builtin_ini_set(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	VmIniSlot *pSlot;
	const char *zName;
	const char *zVal;
	int nName = 0, nVal = 0;
	SyBlob sOld;
	char zWho[64];
	if( nArg < 2 || IniSeed(pVm) != SXRET_OK ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zName = ph7_value_to_string(apArg[0],&nName);
	pSlot = IniFind(pVm,zName,(sxu32)nName);
	if( pSlot == 0 || (pSlot->iAccess & VM_INI_USER) == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( IniSessionLocked(pCtx,pSlot,ph7_function_name(pCtx)) ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* php: the -1 (compiled-out) state of zend.assertions is a php.ini-only switch,
	 * and so is moving INTO it. Unprefixed warning, exactly as php prints it. */
	if( IniNameIs(pSlot,"zend.assertions") ){
		int bGlobalOff = SyBlobLength(&pSlot->sGlobal) == 2
			&& SyMemcmp(SyBlobData(&pSlot->sGlobal),"-1",2) == 0;
		const char *zNew = ph7_value_to_string(apArg[1],&nVal);
		int bNewOff = nVal == 2 && SyMemcmp(zNew,"-1",2) == 0;
		if( bGlobalOff || bNewOff ){
			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,
				"zend.assertions may be completely enabled or disabled only in php.ini");
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
	}
	/* The OLD value php answers is the effective one, read before the write. */
	SyBlobInit(&sOld,&pVm->sAllocator);
	IniLiveGet(pVm,pSlot,&sOld);
	/* php stringifies the incoming value, with a bool becoming "1"/"" . */
	if( ph7_value_is_bool(apArg[1]) ){
		zVal = ph7_value_to_bool(apArg[1]) ? "1" : "";
		nVal = (int)SyStrlen(zVal);
	}else{
		zVal = ph7_value_to_string(apArg[1],&nVal);
	}
	SyBufferFormat(zWho,sizeof(zWho),"%s()",ph7_function_name(pCtx));
	if( !IniValueAccepted(pVm,pSlot,zVal,(sxu32)nVal,zWho) ){
		SyBlobRelease(&sOld);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	IniWriteLocal(pSlot,zVal,(sxu32)nVal);
	IniLiveSet(pVm,pSlot,zVal,(sxu32)nVal);
	ph7_result_string(pCtx,(const char *)SyBlobData(&sOld),(int)SyBlobLength(&sOld));
	SyBlobRelease(&sOld);
	return PH7_OK;
}
/* void ini_restore(string $option) */
static int vm_builtin_ini_restore(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	VmIniSlot *pSlot;
	const char *zName;
	int nName = 0;
	ph7_result_null(pCtx);
	if( nArg < 1 || IniSeed(pVm) != SXRET_OK ){
		return PH7_OK;
	}
	zName = ph7_value_to_string(apArg[0],&nName);
	pSlot = IniFind(pVm,zName,(sxu32)nName);
	if( pSlot == 0 || IniSessionLocked(pCtx,pSlot,"ini_restore") ){
		return PH7_OK;
	}
	SyBlobReset(&pSlot->sLocal);
	SyBlobAppend(&pSlot->sLocal,SyBlobData(&pSlot->sGlobal),SyBlobLength(&pSlot->sGlobal));
	pSlot->bLocalNull = pSlot->bGlobalNull;   /* an unset directive goes back to unset */
	IniLiveSet(pVm,pSlot,(const char *)SyBlobData(&pSlot->sGlobal),SyBlobLength(&pSlot->sGlobal));
	return PH7_OK;
}
/*
 * The extension id php's module registry would find for this name: the key it
 * stores is the extension name FOLDED, and the lookup against it is exact.
 */
static int VmIniExtensionId(const char *zName,int nName)
{
	int iExt,n;
	for( iExt = 0 ; iExt < PH7_VmExtensionCount() ; ++iExt ){
		const char *zCanon;
		if( !PH7_VmExtensionAvailable(iExt) ){
			continue;
		}
		zCanon = PH7_VmExtensionName(iExt);
		if( (int)SyStrlen(zCanon) != nName ){
			continue;
		}
		for( n = 0 ; n < nName ; ++n ){
			if( (char)SyToLower(zCanon[n]) != zName[n] ){
				break;
			}
		}
		if( n == nName ){
			return iExt;
		}
	}
	return -1;
}
/* array|false ini_get_all(?string $extension = null, bool $details = true) */
static int vm_builtin_ini_get_all(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	VmIniSlot *aSlot;
	ph7_value *pOut,*pCur;
	const char *zExt = 0;
	int nExt = 0, bDetails = 1, iExtSel = -1;
	sxu32 n;
	SyBlob sVal;
	if( IniSeed(pVm) != SXRET_OK ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){
		zExt = ph7_value_to_string(apArg[0],&nExt);
	}
	if( nArg > 1 ){
		bDetails = ph7_value_to_bool(apArg[1]);
	}
	if( zExt ){
		/* php looks the name up in the MODULE REGISTRY, whose key is the extension
		 * name folded down -- so the match is case-SENSITIVE against that key:
		 * `core` and `spl` are found, `Core` and `SPL` are not, and every other
		 * name is a warning and false. An extension that owns no directive is
		 * still found and answers the EMPTY array; a `phl.stub_extensions` name
		 * is no module and is not found. This engine used to accept five names
		 * spelled its own way and refuse the rest. */
		iExtSel = VmIniExtensionId(zExt,nExt);
		if( iExtSel < 0 ){
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
				"Extension \"%.*s\" cannot be found",nExt,zExt);
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
	}
	pOut = ph7_context_new_array(pCtx);
	pCur = ph7_context_new_scalar(pCtx);
	if( pOut == 0 || pCur == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	SyBlobInit(&sVal,&pVm->sAllocator);
	aSlot = (VmIniSlot *)SySetBasePtr(&pVm->aIniTab);
	/* The table is stored sorted, so this walk is already php's ksort order. */
	for( n = 0 ; n < SySetUsed(&pVm->aIniTab) ; n++ ){
		VmIniSlot *pSlot = &aSlot[n];
		char zKey[128];
		/* A directive belongs where the extension partition puts it rather than
		 * where its NAME points -- php's `standard` owns `session.trans_sid_tags`
		 * and its `Core` owns none of the `session.*` ones. `core` is php's own
		 * exception and answers EVERY directive whatever module registered it. */
		if( iExtSel >= 0 && iExtSel != PH7_EXT_CORE
		 && PH7_VmExtOfIni(pSlot->sName.zString,(int)pSlot->sName.nByte) != iExtSel ){
			continue;
		}
		if( pSlot->sName.nByte >= sizeof(zKey) ){
			continue;
		}
		SyMemcpy(pSlot->sName.zString,zKey,pSlot->sName.nByte);
		zKey[pSlot->sName.nByte] = 0;
		IniLiveGet(pVm,pSlot,&sVal);
		if( bDetails ){
			ph7_value *pRow = ph7_context_new_array(pCtx);
			if( pRow == 0 ){
				break;
			}
			{
				const char *zG = 0;
				sxu32 nG = 0;
				if( IniGlobalValue(pSlot,&zG,&nG) ){
					ph7_value_string(pCur,zG,(int)nG);
				}else{
					ph7_value_null(pCur);
				}
			}
			ph7_array_add_strkey_elem(pRow,"global_value",pCur);
			ph7_value_reset_string_cursor(pCur);
			if( pSlot->bLocalNull ){
				ph7_value_null(pCur);
			}else{
				ph7_value_string(pCur,(const char *)SyBlobData(&sVal),(int)SyBlobLength(&sVal));
			}
			ph7_array_add_strkey_elem(pRow,"local_value",pCur);
			ph7_value_reset_string_cursor(pCur);
			ph7_value_int(pCur,pSlot->iAccess);
			ph7_array_add_strkey_elem(pRow,"access",pCur);
			ph7_value_reset_string_cursor(pCur);
			ph7_array_add_strkey_elem(pOut,zKey,pRow);
		}else{
			ph7_value_reset_string_cursor(pCur);
			if( pSlot->bLocalNull ){
				ph7_value_null(pCur);
			}else{
				ph7_value_string(pCur,(const char *)SyBlobData(&sVal),(int)SyBlobLength(&sVal));
			}
			ph7_array_add_strkey_elem(pOut,zKey,pCur);
			ph7_value_reset_string_cursor(pCur);
		}
	}
	SyBlobRelease(&sVal);
	ph7_result_value(pCtx,pOut);
	return PH7_OK;
}
/* string|false get_cfg_var(string $option) — php answers the GLOBAL value */
static int vm_builtin_get_cfg_var(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	VmIniSlot *pSlot;
	const char *zName;
	int nName = 0;
	if( nArg < 1 || IniSeed(pVm) != SXRET_OK ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zName = ph7_value_to_string(apArg[0],&nName);
	pSlot = IniFind(pVm,zName,(sxu32)nName);
	if( pSlot == 0 || pSlot->bGlobalNull ){
		/* php reads this one from the php.ini FILE rather than from the live
		 * directive, so a name the file never mentioned is false -- and a
		 * directive declared with no value is exactly such a name, whatever a
		 * later ini_set() put in it. */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_string(pCtx,(const char *)SyBlobData(&pSlot->sGlobal),
		(int)SyBlobLength(&pSlot->sGlobal));
	return PH7_OK;
}
/*
 * The effective value of a directive a C builtin needs to obey, as an integer.
 * parse_str() reads max_input_vars and max_input_nesting_level through this;
 * without it a builtin would have to duplicate php's default and could never
 * see an `ini_set()`/`-d` override. Answers iDefault when the directive is
 * absent or unparsable.
 */
PH7_PRIVATE sxi64 PH7_VmIniGetInt(ph7_vm *pVm,const char *zName,sxi64 iDefault)
{
	VmIniSlot *pSlot;
	SyBlob sVal;
	sxi64 iVal = iDefault;
	IniSeed(pVm);
	pSlot = IniFind(pVm,zName,(sxu32)SyStrlen(zName));
	if( pSlot == 0 ){
		return iDefault;
	}
	SyBlobInit(&sVal,&pVm->sAllocator);
	IniLiveGet(pVm,pSlot,&sVal);
	if( SyBlobLength(&sVal) > 0 ){
		SyStrToInt64((const char *)SyBlobData(&sVal),SyBlobLength(&sVal),(void *)&iVal,0);
	}
	SyBlobRelease(&sVal);
	return iVal;
}
/* The same, as a borrowed STRING (arg_separator.input). The bytes live in the
 * caller's blob, which it owns. */
/*
 * One directive as the export format needs it: php's access bitmask, the value
 * a script reads now, and the DEFAULT behind it. `pOut`/`pDef` are the
 * caller's blobs. Answers 0 when the build has no such directive.
 */
PH7_PRIVATE int PH7_VmIniDescribe(ph7_vm *pVm,const char *zName,sxu32 nName,
	sxi32 *piAccess,SyBlob *pOut,SyBlob *pDef)
{
	VmIniSlot *pSlot;
	IniSeed(&(*pVm));
	pSlot = IniFind(&(*pVm),zName,nName);
	if( pSlot == 0 ){
		return 0;
	}
	*piAccess = pSlot->iAccess;
	IniLiveGet(&(*pVm),pSlot,pOut);
	SyBlobReset(pDef);
	SyBlobAppend(pDef,SyBlobData(&pSlot->sGlobal),SyBlobLength(&pSlot->sGlobal));
	return 1;
}
PH7_PRIVATE void PH7_VmIniGetStr(ph7_vm *pVm,const char *zName,SyBlob *pOut)
{
	VmIniSlot *pSlot;
	IniSeed(pVm);
	SyBlobReset(pOut);
	pSlot = IniFind(pVm,zName,(sxu32)SyStrlen(zName));
	if( pSlot ){
		IniLiveGet(pVm,pSlot,pOut);
	}
}
/*
 * Does this directive currently hold php's UNSET value? Only the surfaces that
 * show a RAW value ask -- ini_get() answers the empty string for one either
 * way, which is why the question has to be put separately.
 */
PH7_PRIVATE int PH7_VmIniIsUnset(ph7_vm *pVm,const char *zName)
{
	VmIniSlot *pSlot;
	IniSeed(pVm);
	pSlot = IniFind(pVm,zName,(sxu32)SyStrlen(zName));
	return pSlot != 0 && pSlot->bLocalNull;
}
/*
 * Read a BOOLEAN directive the way zend_ini does — "on"/"yes"/"true" as well as
 * a non-zero number. Reading one through the integer parser answers 0 for
 * `On`, which is the spelling php.ini-production ships, so a directive written
 * that way reads as OFF.
 */
PH7_PRIVATE int PH7_VmIniGetBool(ph7_vm *pVm,const char *zName,int bDefault)
{
	VmIniSlot *pSlot;
	SyBlob sVal;
	int bRes;
	IniSeed(pVm);
	pSlot = IniFind(pVm,zName,(sxu32)SyStrlen(zName));
	if( pSlot == 0 ){
		return bDefault;
	}
	SyBlobInit(&sVal,&pVm->sAllocator);
	IniLiveGet(pVm,pSlot,&sVal);
	bRes = IniTruthy((const char *)SyBlobData(&sVal),SyBlobLength(&sVal));
	SyBlobRelease(&sVal);
	return bRes;
}
PH7_PRIVATE sxi32 PH7_VmInstallIni(ph7_vm *pVm)
{
	static const struct {
		const char *zName;
		ProchHostFunction xFunc;
	} aFunc[] = {
		{ "ini_get",      vm_builtin_ini_get      },
		{ "ini_set",      vm_builtin_ini_set      },
		/* php's alias for the same routine. Both of the diagnostics a write
		 * can raise name the INVOKED function, so they read the context's
		 * name rather than a literal. */
		{ "ini_alter",    vm_builtin_ini_set      },
		{ "ini_restore",  vm_builtin_ini_restore  },
		{ "ini_get_all",  vm_builtin_ini_get_all  },
		{ "get_cfg_var",  vm_builtin_get_cfg_var  },
	};
	sxu32 n;
	for( n = 0 ; n < SX_ARRAYSIZE(aFunc) ; n++ ){
		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);
	}
	return SXRET_OK;
}
#else
PH7_PRIVATE sxi32 PH7_VmInstallIni(ph7_vm *pVm){ (void)pVm; return SXRET_OK; }
PH7_PRIVATE sxi64 PH7_VmIniGetInt(ph7_vm *pVm,const char *zName,sxi64 iDefault){
	(void)pVm; (void)zName; return iDefault;
}
PH7_PRIVATE int PH7_VmIniGetBool(ph7_vm *pVm,const char *zName,int bDefault){
	(void)pVm; (void)zName; return bDefault;
}
PH7_PRIVATE void PH7_VmIniGetStr(ph7_vm *pVm,const char *zName,SyBlob *pOut){
	(void)pVm; (void)zName; SyBlobReset(pOut);
}
PH7_PRIVATE int PH7_VmIniIsUnset(ph7_vm *pVm,const char *zName){
	(void)pVm; (void)zName; return 0;
}
PH7_PRIVATE int PH7_VmIniDescribe(ph7_vm *pVm,const char *zName,sxu32 nName,
	sxi32 *piAccess,SyBlob *pOut,SyBlob *pDef){
	(void)pVm; (void)zName; (void)nName; (void)piAccess;
	SyBlobReset(pOut); SyBlobReset(pDef); return 0;
}
PH7_PRIVATE int PH7_VmIniSet(ph7_vm *pVm,const char *zName,sxu32 nName,
	const char *zVal,sxu32 nVal,const char *zWho){
	(void)pVm; (void)zName; (void)nName; (void)zVal; (void)nVal; (void)zWho; return 0;
}
#endif
