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
	const char *zValue;
	sxi32 iAccess;
} aIniDefault[] = {
	{ "allow_url_fopen",          "1",          VM_INI_PERDIR|VM_INI_SYSTEM },
	/* php's default is OFF, and it spells that default as the EMPTY string — which
	 * is what ini_get() answers. Including a remote file is the classic RFI, and
	 * this is what a STREAM_IS_URL wrapper's include is gated on. */
	{ "allow_url_include",        "",           VM_INI_SYSTEM },
	{ "arg_separator.input",      "&",          VM_INI_ALL },
	{ "arg_separator.output",     "&",          VM_INI_ALL },
	{ "auto_detect_line_endings", "",           VM_INI_ALL },
	{ "date.timezone",            "UTC",        VM_INI_ALL },
	{ "default_charset",          "UTF-8",      VM_INI_ALL },
	/* php bounds a socket wait by this rather than waiting forever, and it is
	 * where stream_socket_accept() takes its default timeout from. */
	{ "default_socket_timeout",   "60",         VM_INI_ALL },
	{ "default_mimetype",         "text/html",  VM_INI_ALL },
	{ "display_errors",           "",           VM_INI_ALL },
	{ "error_log",                "",           VM_INI_ALL },
	{ "error_reporting",          "30719",      VM_INI_ALL },
	{ "highlight.comment",        "#FF8000",    VM_INI_ALL },
	{ "highlight.default",        "#0000BB",    VM_INI_ALL },
	{ "highlight.html",           "#000000",    VM_INI_ALL },
	{ "highlight.keyword",        "#007700",    VM_INI_ALL },
	{ "highlight.string",         "#DD0000",    VM_INI_ALL },
	{ "include_path",             ".",          VM_INI_ALL },
	{ "log_errors",               "1",          VM_INI_ALL },
	{ "max_execution_time",       "0",          VM_INI_ALL },
	{ "max_input_nesting_level",  "64",         VM_INI_PERDIR|VM_INI_SYSTEM },
	{ "max_input_vars",           "1000",       VM_INI_PERDIR|VM_INI_SYSTEM },
	{ "memory_limit",             "-1",         VM_INI_ALL },
	{ "post_max_size",            "8M",         VM_INI_PERDIR|VM_INI_SYSTEM },
	{ "precision",                "14",         VM_INI_ALL },
	{ "serialize_precision",      "-1",         VM_INI_ALL },
	{ "session.name",             "PHPSESSID",  VM_INI_ALL },
	{ "session.save_path",        "",           VM_INI_ALL },
	/* Which of php's three session serializers writes the store: `php` (the
	 * `name|<serialized>` runs a stock php install reads), `php_binary` or
	 * `php_serialize`. */
	{ "session.serialize_handler","php",        VM_INI_ALL },
	{ "short_open_tag",           "",           VM_INI_PERDIR|VM_INI_SYSTEM },
	{ "unserialize_callback_func","",           VM_INI_ALL },
	{ "unserialize_max_depth",    "4096",       VM_INI_ALL },
	{ "upload_max_filesize",      "2M",         VM_INI_PERDIR|VM_INI_SYSTEM },
	{ "zend.assertions",          "-1",         VM_INI_ALL },
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
		SyBlobAppend(&sSlot.sGlobal,aIniDefault[i].zValue,(sxu32)SyStrlen(aIniDefault[i].zValue));
		SyBlobAppend(&sSlot.sLocal,aIniDefault[i].zValue,(sxu32)SyStrlen(aIniDefault[i].zValue));
		if( SySetPut(&pVm->aIniTab,(const void *)&sSlot) != SXRET_OK ){
			return SXERR_MEM;
		}
	}
	aCli = (VmIniEntry *)SySetBasePtr(&pVm->aIniCli);
	for( i = 0 ; i < SySetUsed(&pVm->aIniCli) ; i++ ){
		int bFound = 0;
		aSlot = (VmIniSlot *)SySetBasePtr(&pVm->aIniTab);
		for( j = 0 ; j < SySetUsed(&pVm->aIniTab) ; j++ ){
			if( aSlot[j].sName.nByte == aCli[i].sName.nByte
			 && SyMemcmp(aSlot[j].sName.zString,aCli[i].sName.zString,aCli[i].sName.nByte) == 0 ){
				SyBlobReset(&aSlot[j].sGlobal);
				SyBlobReset(&aSlot[j].sLocal);
				SyBlobAppend(&aSlot[j].sGlobal,aCli[i].sValue.zString,aCli[i].sValue.nByte);
				SyBlobAppend(&aSlot[j].sLocal,aCli[i].sValue.zString,aCli[i].sValue.nByte);
				bFound = 1;
				break;
			}
		}
		if( !bFound ){
			VmIniSlot sSlot;
			/* aIniCli holds VM-lifetime copies already, so the name can be aliased. */
			sSlot.sName = aCli[i].sName;
			sSlot.iAccess = VM_INI_ALL;
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
static void IniLiveSet(ph7_vm *pVm,VmIniSlot *pSlot,const char *zVal,sxu32 nVal)
{
	if( IniNameIs(pSlot,"error_reporting") ){
		sxi32 iVal = 0;
		if( nVal > 0 ){
			SyStrToInt32(zVal,nVal,(void *)&iVal,0);
		}
		pVm->iErrMask = iVal;
		pVm->bErrReport = iVal != 0;
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
 * The session directives are php.ini-settable only until headers go out.
 * Answers TRUE (and has raised the warning) when the write must be refused.
 */
static int IniSessionLocked(ph7_context *pCtx,VmIniSlot *pSlot,const char *zFunc)
{
	char zMsg[160];
	if( pSlot->sName.nByte < sizeof("session.")-1
	 || SyMemcmp(pSlot->sName.zString,"session.",sizeof("session.")-1) != 0 ){
		return 0;
	}
	if( !pCtx->pVm->bHeadersSent ){
		return 0;
	}
	SyBufferFormat(zMsg,sizeof(zMsg),
		"%s(): Session ini settings cannot be changed after headers have already been sent",
		zFunc);
	PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);
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
	if( IniSessionLocked(pCtx,pSlot,"ini_set") ){
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
	if( IniNameIs(pSlot,"session.serialize_handler")
	 && !(nVal == 3 && SyMemcmp(zVal,"php",3) == 0)
	 && !(nVal == 10 && SyMemcmp(zVal,"php_binary",10) == 0)
	 && !(nVal == 13 && SyMemcmp(zVal,"php_serialize",13) == 0) ){
		/* php looks the name up in its registered serializer list and refuses what
		 * it cannot find, keeping the directive where it was. */
		char zMsg[160];
		SyBufferFormat(zMsg,sizeof(zMsg),
			"ini_set(): Serialization handler \"%.*s\" cannot be found",nVal,zVal);
		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,zMsg);
		SyBlobRelease(&sOld);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nVal < 1 && IniNameIs(pSlot,"include_path") ){
		/* php registers include_path with OnUpdateStringUnempty: the EMPTY value
		 * is refused, the directive keeps what it had, and the call is FALSE. */
		SyBlobRelease(&sOld);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SyBlobReset(&pSlot->sLocal);
	SyBlobAppend(&pSlot->sLocal,zVal,(sxu32)nVal);
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
	IniLiveSet(pVm,pSlot,(const char *)SyBlobData(&pSlot->sGlobal),SyBlobLength(&pSlot->sGlobal));
	return PH7_OK;
}
/* array|false ini_get_all(?string $extension = null, bool $details = true) */
static int vm_builtin_ini_get_all(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	VmIniSlot *aSlot;
	ph7_value *pOut,*pCur;
	const char *zExt = 0;
	int nExt = 0, bDetails = 1;
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
		/* php reports the extensions it knows; anything else is a warning + false. */
		static const char *azKnown[] = { "Core", "session", "date", "standard" };
		int bKnown = 0;
		sxu32 k;
		for( k = 0 ; k < SX_ARRAYSIZE(azKnown) ; k++ ){
			if( (int)SyStrlen(azKnown[k]) == nExt && SyMemcmp(azKnown[k],zExt,(sxu32)nExt) == 0 ){
				bKnown = 1;
				break;
			}
		}
		if( !bKnown ){
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
		if( zExt ){
			int bCore = (nExt == 4 && SyMemcmp(zExt,"Core",4) == 0)
				|| (nExt == 8 && SyMemcmp(zExt,"standard",8) == 0);
			if( !bCore ){
				/* A named extension keeps only its own "<ext>." prefix. */
				if( pSlot->sName.nByte <= (sxu32)nExt
				 || SyMemcmp(pSlot->sName.zString,zExt,(sxu32)nExt) != 0
				 || pSlot->sName.zString[nExt] != '.' ){
					continue;
				}
			}else{
				/* Core/standard exclude the directives owned by a named extension. */
				if( (pSlot->sName.nByte > sizeof("session.")-1
				  && SyMemcmp(pSlot->sName.zString,"session.",sizeof("session.")-1) == 0)
				 || (pSlot->sName.nByte > sizeof("date.")-1
				  && SyMemcmp(pSlot->sName.zString,"date.",sizeof("date.")-1) == 0) ){
					continue;
				}
			}
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
			ph7_value_string(pCur,(const char *)SyBlobData(&pSlot->sGlobal),
				(int)SyBlobLength(&pSlot->sGlobal));
			ph7_array_add_strkey_elem(pRow,"global_value",pCur);
			ph7_value_reset_string_cursor(pCur);
			ph7_value_string(pCur,(const char *)SyBlobData(&sVal),(int)SyBlobLength(&sVal));
			ph7_array_add_strkey_elem(pRow,"local_value",pCur);
			ph7_value_reset_string_cursor(pCur);
			ph7_value_int(pCur,pSlot->iAccess);
			ph7_array_add_strkey_elem(pRow,"access",pCur);
			ph7_value_reset_string_cursor(pCur);
			ph7_array_add_strkey_elem(pOut,zKey,pRow);
		}else{
			ph7_value_reset_string_cursor(pCur);
			ph7_value_string(pCur,(const char *)SyBlobData(&sVal),(int)SyBlobLength(&sVal));
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
	if( pSlot == 0 ){
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
#endif
