/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
/*
 * Section:
 *    The extension partition: which extension every internal name belongs to,
 *    which extensions this build reports as loaded, and the order each one
 *    lists its own names in. extension_loaded(), get_loaded_extensions() and
 *    get_extension_funcs() live here, and Reflection's four extension surfaces
 *    read the same tables.
 * Status:
 *    Stable.
 */
#include "vm_extension_names.h"
/*
 * Is this extension part of THIS build? The four XML names, the two PDO ones,
 * curl, zlib and openssl are compile-time options, and posix and pcntl are PLATFORM
 * ones -- php builds neither for Windows, so `extension_loaded('posix')` and
 * `extension_loaded('pcntl')` are false there under php and under this.
 * Everything else is always here.
 */
static int VmExtAvailable(int iExt)
{
#ifndef PH7_ENABLE_LIBXML
	if( iExt == PH7_EXT_LIBXML || iExt == PH7_EXT_XML
	 || iExt == PH7_EXT_DOM || iExt == PH7_EXT_XMLWRITER
	 || iExt == PH7_EXT_SIMPLEXML ){
		return 0;
	}
#endif
#ifndef PH7_ENABLE_SQLITE
	if( iExt == PH7_EXT_PDO || iExt == PH7_EXT_PDO_SQLITE || iExt == PH7_EXT_SQLITE3 ){
		return 0;
	}
#endif
#ifndef PH7_ENABLE_CURL
	if( iExt == PH7_EXT_CURL ){
		return 0;
	}
#endif
#ifndef PH7_ENABLE_ZLIB
	/* ext/zip rides the same guard: php's own requires zlib, and so does the
	 * derivation here. */
	if( iExt == PH7_EXT_ZLIB || iExt == PH7_EXT_ZIP ){
		return 0;
	}
#endif
#ifdef PH7_DISABLE_BUILTIN_FUNC
	if( iExt == PH7_EXT_ZIP || iExt == PH7_EXT_PCNTL ){
		return 0;
	}
#endif
#ifndef PH7_ENABLE_OPENSSL
	if( iExt == PH7_EXT_OPENSSL ){
		return 0;
	}
#endif
#ifdef __WINNT__
	if( iExt == PH7_EXT_POSIX || iExt == PH7_EXT_PCNTL ){
		return 0;
	}
#endif
	SXUNUSED(iExt);   /* every option on: nothing above reads it */
	return 1;
}
/*
 * The extension an INTERNAL name belongs to. A function or a class name folds;
 * a constant and an ini directive do not, which is the same case rule the
 * engine's own lookups use for each. A name the table has no row for is the
 * engine's own -- Core -- which is what every one of these answered before the
 * partition existed, so a caller never has to spell that fallback itself.
 */
static int VmExtOfName(const VmExtName *aRow,sxu32 nRow,const char *zName,int nName,int bFold)
{
	sxu32 n;
	if( nName < 1 ){
		return PH7_EXT_CORE;
	}
	for( n = 0 ; n < nRow ; ++n ){
		const char *z = aRow[n].zName;
		if( (int)SyStrlen(z) != nName ){
			continue;
		}
		if( bFold ? (SyStrnicmp(z,zName,(sxu32)nName) == 0)
		          : (SyMemcmp(z,zName,(sxu32)nName) == 0) ){
			return (int)aRow[n].iExt;
		}
	}
	return PH7_EXT_CORE;
}
PH7_PRIVATE int PH7_VmExtOfFunc(const char *zName,int nName)
{
	return VmExtOfName(aExtFunc,SX_ARRAYSIZE(aExtFunc),zName,nName,1);
}
PH7_PRIVATE int PH7_VmExtOfClass(const char *zName,int nName)
{
	return VmExtOfName(aExtClass,SX_ARRAYSIZE(aExtClass),zName,nName,1);
}
PH7_PRIVATE int PH7_VmExtOfConstant(const char *zName,int nName)
{
	return VmExtOfName(aExtConst,SX_ARRAYSIZE(aExtConst),zName,nName,0);
}
PH7_PRIVATE int PH7_VmExtOfIni(const char *zName,int nName)
{
	return VmExtOfName(aExtIni,SX_ARRAYSIZE(aExtIni),zName,nName,0);
}
/*
 * Does the partition carry a ROW for this name? A caller that walks the engine's
 * own table rather than this one needs to tell "belongs to Core" from "the
 * partition has never heard of it", so that a name added to the engine after the
 * table was derived is still reported somewhere instead of silently vanishing.
 */
PH7_PRIVATE int PH7_VmExtHasName(int iKind,const char *zName,int nName)
{
	const VmExtName *aRow;
	sxu32 n,nRow;
	int bFold;
	switch( iKind ){
		case PH7_EXT_KIND_CLASS: aRow = aExtClass; nRow = SX_ARRAYSIZE(aExtClass); bFold = 1; break;
		case PH7_EXT_KIND_CONST: aRow = aExtConst; nRow = SX_ARRAYSIZE(aExtConst); bFold = 0; break;
		case PH7_EXT_KIND_INI:   aRow = aExtIni;   nRow = SX_ARRAYSIZE(aExtIni);   bFold = 0; break;
		default:                 aRow = aExtFunc;  nRow = SX_ARRAYSIZE(aExtFunc);  bFold = 1; break;
	}
	if( nName < 1 ){
		return 0;
	}
	for( n = 0 ; n < nRow ; ++n ){
		const char *z = aRow[n].zName;
		if( (int)SyStrlen(z) != nName ){
			continue;
		}
		if( bFold ? (SyStrnicmp(z,zName,(sxu32)nName) == 0)
		          : (SyMemcmp(z,zName,(sxu32)nName) == 0) ){
			return 1;
		}
	}
	return 0;
}
/*
 * The number of extensions this build reports, so a caller can walk them in the
 * order get_loaded_extensions() lists them.
 */
PH7_PRIVATE int PH7_VmExtensionCount(void)
{
	return (int)SX_ARRAYSIZE(azExtName);
}
PH7_PRIVATE int PH7_VmExtensionAvailable(int iExt)
{
	return iExt >= 0 && iExt < (int)SX_ARRAYSIZE(azExtName) && VmExtAvailable(iExt);
}
/*
 * Walk the names ONE extension carries, in php's own registration order --
 * which is the order `get_extension_funcs()` and every ReflectionExtension
 * listing answer in, and is not alphabetical. xVisit stops the walk when it
 * answers non-zero, and that answer is this function's.
 *
 * The table describes a FULL build, so nothing here asserts a name exists:
 * every caller filters against the live VM, which is what makes a build
 * without libxml, sqlite or curl -- and MODE=tiny, which drops most of the
 * library -- answer the shorter list without a second table.
 */
PH7_PRIVATE int PH7_VmExtWalk(int iExt,int iKind,int (*xVisit)(const char *,int,void *),void *pData)
{
	const VmExtName *aRow;
	sxu32 n,nRow;
	switch( iKind ){
		case PH7_EXT_KIND_CLASS: aRow = aExtClass; nRow = SX_ARRAYSIZE(aExtClass); break;
		case PH7_EXT_KIND_CONST: aRow = aExtConst; nRow = SX_ARRAYSIZE(aExtConst); break;
		case PH7_EXT_KIND_INI:   aRow = aExtIni;   nRow = SX_ARRAYSIZE(aExtIni);   break;
		default:                 aRow = aExtFunc;  nRow = SX_ARRAYSIZE(aExtFunc);  break;
	}
	for( n = 0 ; n < nRow ; ++n ){
		if( (int)aRow[n].iExt == iExt ){
			int rc = xVisit(aRow[n].zName,(int)SyStrlen(aRow[n].zName),pData);
			if( rc != 0 ){
				return rc;
			}
		}
	}
	return 0;
}
/*
 * The name of an extension id, and the id of a name. php matches an extension
 * name case-insensitively everywhere it takes one.
 */
PH7_PRIVATE const char * PH7_VmExtensionName(int iExt)
{
	if( iExt < 0 || iExt >= (int)SX_ARRAYSIZE(azExtName) ){
		return "Core";
	}
	return azExtName[iExt];
}
PH7_PRIVATE int PH7_VmExtensionLookup(const char *zName,int nName)
{
	sxu32 n;
	for( n = 0 ; n < SX_ARRAYSIZE(azExtName) ; ++n ){
		if( nName == (int)SyStrlen(azExtName[n])
		 && SyStrnicmp(zName,azExtName[n],(sxu32)nName) == 0 ){
			return VmExtAvailable((int)n) ? (int)n : -1;
		}
	}
	return -1;
}
/*
 * `phl.stub_extensions` is a PHL-only directive: a comma-separated list of
 * extensions PHL does NOT implement but reports as LOADED, so software that
 * only GATES on extension_loaded() (PHPUnit's dom/xmlwriter check) runs
 * unmodified. It synthesizes nothing -- no class, no function, no constant,
 * so a stub name has no id and Reflection never attributes anything to it.
 *
 * Walk it, handing each trimmed name to xVisit until one answers non-zero.
 */
static int VmStubExtWalk(ph7_vm *pVm,int (*xVisit)(const char *,int,void *),void *pData)
{
	SyBlob sList;
	const char *z;
	int nByte,i = 0,rc = 0;
	SyBlobInit(&sList,&pVm->sAllocator);
	PH7_VmIniGetStr(pVm,"phl.stub_extensions",&sList);
	z = (const char *)SyBlobData(&sList);
	nByte = (int)SyBlobLength(&sList);
	while( rc == 0 && i < nByte ){
		int iStart,iEnd;
		while( i < nByte && z[i] == ',' ){ i++; }
		iStart = i;
		while( i < nByte && z[i] != ',' ){ i++; }
		iEnd = i;
		while( iStart < iEnd && (z[iStart] == ' ' || z[iStart] == '\t') ){ iStart++; }
		while( iEnd > iStart && (z[iEnd-1] == ' ' || z[iEnd-1] == '\t') ){ iEnd--; }
		if( iEnd > iStart ){
			rc = xVisit(&z[iStart],iEnd - iStart,pData);
		}
	}
	SyBlobRelease(&sList);
	return rc;
}
typedef struct vm_ext_match vm_ext_match;
struct vm_ext_match {
	const char *zName;
	int nName;
};
static int VmStubExtMatch(const char *zName,int nName,void *pData)
{
	vm_ext_match *p = (vm_ext_match *)pData;
	return nName == p->nName && SyStrnicmp(zName,p->zName,(sxu32)nName) == 0;
}
/*
 * Is this name one of the extensions this engine reports as loaded? Shared by
 * extension_loaded() and phpversion(), which php answers from the same list.
 */
PH7_PRIVATE int PH7_VmExtensionIsLoaded(ph7_vm *pVm,const char *zName,int nName)
{
	vm_ext_match sMatch;
	if( PH7_VmExtensionLookup(zName,nName) >= 0 ){
		return 1;
	}
	sMatch.zName = zName;
	sMatch.nName = nName;
	return VmStubExtWalk(pVm,VmStubExtMatch,&sMatch);
}
/*
 * bool extension_loaded(string $extension)
 *  php matches the name case-insensitively.
 */
PH7_PRIVATE int vm_builtin_extension_loaded(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zName;
	int nName;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zName = ph7_value_to_string(apArg[0],&nName);
	ph7_result_bool(pCtx,PH7_VmExtensionIsLoaded(pCtx->pVm,zName,nName));
	return PH7_OK;
}
static int VmStubExtCollect(const char *zName,int nName,void *pData)
{
	ph7_context *pCtx = (ph7_context *)((void **)pData)[0];
	ph7_value *pArray = (ph7_value *)((void **)pData)[1];
	ph7_value *pVal = ph7_context_new_scalar(pCtx);
	if( pVal ){
		ph7_value_string(pVal,zName,nName);
		ph7_array_add_elem(pArray,0,pVal);
		ph7_context_release_value(pCtx,pVal);
	}
	return 0;
}
/*
 * array get_loaded_extensions(bool $zend_extensions = false)
 *  PHL loads no Zend extension, so the zend list is always empty.
 */
PH7_PRIVATE int vm_builtin_get_loaded_extensions(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pArray = ph7_context_new_array(pCtx);
	void *apData[2];
	sxu32 n;
	if( pArray == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( nArg > 0 && ph7_value_to_bool(apArg[0]) ){
		ph7_result_value(pCtx,pArray);
		return PH7_OK;
	}
	for( n = 0 ; n < SX_ARRAYSIZE(azExtName) ; ++n ){
		ph7_value *pVal;
		if( !VmExtAvailable((int)n) ){
			continue;
		}
		pVal = ph7_context_new_scalar(pCtx);
		if( pVal ){
			ph7_value_string(pVal,azExtName[n],-1);
			ph7_array_add_elem(pArray,0,pVal);
			ph7_context_release_value(pCtx,pVal);
		}
	}
	apData[0] = pCtx;
	apData[1] = pArray;
	VmStubExtWalk(pCtx->pVm,VmStubExtCollect,apData);
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * Does the live VM carry this INTERNAL name? The tables describe a FULL build,
 * so every listing filters through here: a build without libxml, sqlite or curl
 * -- and MODE=tiny, which drops most of the library -- answers the shorter list
 * without a second table. An ini directive is registered by the engine itself
 * and always present, so it is the one kind with nothing to ask.
 */
PH7_PRIVATE int PH7_VmInternalNameExists(ph7_vm *pVm,int iKind,const char *zName,int nName)
{
	if( nName < 1 ){
		return 0;
	}
	switch( iKind ){
		case PH7_EXT_KIND_CLASS:
			return PH7_VmExtractClass(pVm,zName,(sxu32)nName,FALSE,0) != 0;
		case PH7_EXT_KIND_CONST: {
			SyHashEntry *pEntry = SyHashGet(&pVm->hConstant,(const void *)zName,(sxu32)nName);
			return pEntry != 0;
		}
		case PH7_EXT_KIND_INI:
			return 1;
		default:
			break;
	}
	/* A builtin is either a C host function or a prelude body the engine marks
	 * internal -- the same pair function_exists() consults. */
	return PH7_VmGetUserFunction(pVm,(const void *)zName,(sxu32)nName,FALSE) != 0
		|| SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nName) != 0;
}
/*
 * What one extension DECLARES about the others, which is what php's
 * `ReflectionExtension::getDependencies()` answers -- a map from the module
 * registry's own (lower-case) key to "Required", "Optional" or "Conflicts".
 * php's declarations are reproduced here, restricted to the extensions this
 * engine actually has: php's `dom` also names `lexbor` and `domxml` and its
 * `standard` names `uri`, none of which exist in any PHL build, so a program
 * reading the map is told only about names it can go on to check. Every
 * dependency left IS satisfied, since this engine is one binary and its
 * extensions are all loaded together.
 */
static const struct VmExtDep {
	sxu8 iExt;           /* the extension declaring it */
	const char *zOn;     /* the module registry key it names */
	const char *zKind;   /* php's own word for the relationship */
} aExtDep[] = {
	{ PH7_EXT_SPL,        "json",     "Required" },
	{ PH7_EXT_STANDARD,   "session",  "Optional" },
	{ PH7_EXT_SESSION,    "spl",      "Optional" },
	{ PH7_EXT_MBSTRING,   "pcre",     "Required" },
	{ PH7_EXT_LIBXML,     "standard", "Required" },
	{ PH7_EXT_XML,        "libxml",   "Required" },
	{ PH7_EXT_DOM,        "libxml",   "Required" },
	{ PH7_EXT_XMLWRITER,  "libxml",   "Required" },
	{ PH7_EXT_PDO,        "spl",      "Required" },
	{ PH7_EXT_PDO_SQLITE, "pdo",      "Required" },
};
PH7_PRIVATE int PH7_VmExtWalkDep(int iExt,int (*xVisit)(const char *,const char *,void *),void *pData)
{
	sxu32 n;
	for( n = 0 ; n < SX_ARRAYSIZE(aExtDep) ; ++n ){
		if( (int)aExtDep[n].iExt == iExt ){
			int rc = xVisit(aExtDep[n].zOn,aExtDep[n].zKind,pData);
			if( rc != 0 ){
				return rc;
			}
		}
	}
	return 0;
}
/*
 * array|false get_extension_funcs(string $extension)
 *  The functions one extension registers, LOWER-cased, in php's own order.
 * Return
 *  php answers FALSE for a name it does not know AND for one that registers no
 *  function at all -- the same answer for both, so `get_extension_funcs('Reflection')`
 *  is false on php while the extension is plainly loaded. A `phl.stub_extensions`
 *  name synthesizes nothing, so it is false for the second reason.
 */
static int VmExtFuncCollect(const char *zName,int nName,void *pData)
{
	ph7_context *pCtx = (ph7_context *)((void **)pData)[0];
	ph7_value *pArray = (ph7_value *)((void **)pData)[1];
	ph7_value *pVal;
	if( !PH7_VmInternalNameExists(pCtx->pVm,PH7_EXT_KIND_FUNC,zName,nName) ){
		return 0;
	}
	pVal = ph7_context_new_scalar(pCtx);
	if( pVal ){
		/* php hands back the name the way the engine STORES it, which is folded. */
		SyBlob sLower;
		char *zBuf;
		int i;
		SyBlobInit(&sLower,&pCtx->pVm->sAllocator);
		SyBlobAppend(&sLower,(const void *)zName,(sxu32)nName);
		zBuf = (char *)SyBlobData(&sLower);
		for( i = 0 ; i < (int)SyBlobLength(&sLower) ; ++i ){
			zBuf[i] = (char)SyToLower(zBuf[i]);
		}
		ph7_value_string(pVal,zBuf,(int)SyBlobLength(&sLower));
		SyBlobRelease(&sLower);
		ph7_array_add_elem(pArray,0,pVal);
		ph7_context_release_value(pCtx,pVal);
	}
	return 0;
}
PH7_PRIVATE int vm_builtin_get_extension_funcs(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pArray;
	void *apData[2];
	const char *zName;
	int nName,iExt;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zName = ph7_value_to_string(apArg[0],&nName);
	iExt = PH7_VmExtensionLookup(zName,nName);
	if( iExt < 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	apData[0] = pCtx;
	apData[1] = pArray;
	PH7_VmExtWalk(iExt,PH7_EXT_KIND_FUNC,VmExtFuncCollect,apData);
	if( ph7_array_count(pArray) < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
