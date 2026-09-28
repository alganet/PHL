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
 * Is this extension part of THIS build? The four XML names, the two PDO ones
 * and curl are compile-time options; everything else is always here.
 */
static int VmExtAvailable(int iExt)
{
#ifndef PH7_ENABLE_LIBXML
	if( iExt == PH7_EXT_LIBXML || iExt == PH7_EXT_XML
	 || iExt == PH7_EXT_DOM || iExt == PH7_EXT_XMLWRITER ){
		return 0;
	}
#endif
#ifndef PH7_ENABLE_SQLITE
	/* ext/sqlite3 (the SQLite3 class family) is NOT one of these: §10 scopes
	 * this build to PDO's sqlite DRIVER, so only the two pdo names load. */
	if( iExt == PH7_EXT_PDO || iExt == PH7_EXT_PDO_SQLITE ){
		return 0;
	}
#endif
#ifndef PH7_ENABLE_CURL
	if( iExt == PH7_EXT_CURL ){
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
