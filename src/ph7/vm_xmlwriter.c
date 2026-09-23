/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifdef PH7_ENABLE_LIBXML
#include "ph7int.h"
#include <libxml/xmlwriter.h>

/*
 * ext/xmlwriter on libxml2: __xw_* native thunks + the XMLWriter class
 * prelude.  Architecture notes live in vm_libxml.c (shared registries,
 * error queue, lifetime model).
 *
 * An XMLWriter object holds a phl_xmlwriter resource {xmlTextWriterPtr,
 * xmlBufferPtr} in $__res.  In-memory writers (openMemory) own an
 * xmlBuffer; outputMemory reads it back.  Every writer is chained on the
 * per-VM registry (pVm->pXmlWriters) and freed at VM reset/release since
 * PH7 resources have no destructor hook.
 */

typedef struct phl_xmlwriter phl_xmlwriter;
struct phl_xmlwriter {
	xmlTextWriterPtr pWriter;
	xmlBufferPtr pBuf;   /* non-NULL for openMemory writers */
	phl_xmlwriter *pNext;
};

/*
 * Free one writer (called from the registry sweep in vm_libxml.c via
 * PH7_XmlWriterVmRelease).  The order matters: the text writer must be
 * freed before its backing buffer.
 */
static void XmlWriterFree(phl_xmlwriter *pXw)
{
	if( pXw->pWriter ){
		xmlFreeTextWriter(pXw->pWriter);
		pXw->pWriter = 0;
	}
	if( pXw->pBuf ){
		xmlBufferFree(pXw->pBuf);
		pXw->pBuf = 0;
	}
}
/*
 * Free every registered writer.  Called from PH7_LibxmlVmReset /
 * PH7_LibxmlVmRelease before the allocator that holds the shells is torn
 * down.
 */
PH7_PRIVATE void PH7_XmlWriterVmSweep(ph7_vm *pVm)
{
	phl_xmlwriter *pXw = (phl_xmlwriter *)pVm->pXmlWriters;
	while( pXw ){
		phl_xmlwriter *pNext = pXw->pNext;
		XmlWriterFree(pXw);
		SyMemBackendFree(&pVm->sAllocator,pXw);
		pXw = pNext;
	}
	pVm->pXmlWriters = 0;
}

static phl_xmlwriter * XmlWriterArg(ph7_value *pVal)
{
	if( pVal == 0 || !ph7_value_is_resource(pVal) ){
		return 0;
	}
	return (phl_xmlwriter *)ph7_value_to_resource(pVal);
}

/*
 * The writer behind $this->__res.
 *
 * These methods were global `__xw_verb($this->__res, ...)` thunks, so each body
 * used to take the resource as argument #0. As native methods they reach it the
 * way php does -- through the receiver -- and their arguments start at #0.
 */
static phl_xmlwriter * XmlWriterOf(ph7_context *pCtx)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	SyString sAttr;
	ph7_value *pRes;
	if( pThis == 0 ){
		return 0;
	}
	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);
	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);
	return XmlWriterArg(pRes);
}
/* bool XMLWriter::openMemory() */
static int vm_builtin_xw_open_memory(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	phl_xmlwriter *pXw;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	pXw = (phl_xmlwriter *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_xmlwriter));
	if( pXw == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SyZero(pXw,sizeof(phl_xmlwriter));
	pXw->pBuf = xmlBufferCreate();
	if( pXw->pBuf ){
		pXw->pWriter = xmlNewTextWriterMemory(pXw->pBuf,0);
	}
	if( pXw->pBuf == 0 || pXw->pWriter == 0 ){
		XmlWriterFree(pXw);
		SyMemBackendFree(&pVm->sAllocator,pXw);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pXw->pNext = (phl_xmlwriter *)pVm->pXmlWriters;
	pVm->pXmlWriters = (void *)pXw;
	/* The prelude used to do the assignment (`$this->__res = __xw_open_memory()`)
	 * and turn the handle into the bool php returns. Both belong here now: the
	 * resource is storage the class owns, and openMemory() answers a bool. */
	{
		ph7_class_instance *pThis = PH7_ContextThis(pCtx);
		SyString sAttr;
		ph7_value *pRes;
		SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);
		pRes = pThis ? PH7_ClassInstanceFetchAttr(pThis,&sAttr) : 0;
		if( pRes == 0 ){
			XmlWriterFree(pXw);
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		PH7_MemObjRelease(pRes);
		pRes->x.pOther = pXw;
		MemObjSetType(pRes,MEMOBJ_RES);
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* bool __xw_set_indent(res,bool) */
static int vm_builtin_xw_set_indent(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_xmlwriter *pXw = XmlWriterOf(pCtx);
	int bIndent = nArg > 0 ? ph7_value_to_bool(apArg[0]) : 0;
	int rc = -1;
	if( pXw && pXw->pWriter ){
		rc = xmlTextWriterSetIndent(pXw->pWriter,bIndent ? 1 : 0);
		if( rc >= 0 && bIndent ){
			/* php's default indent string is a single space */
			xmlTextWriterSetIndentString(pXw->pWriter,(const xmlChar *)" ");
		}
	}
	ph7_result_bool(pCtx,rc >= 0);
	return PH7_OK;
}
/* bool __xw_set_indent_string(res,str) */
static int vm_builtin_xw_set_indent_string(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_xmlwriter *pXw = XmlWriterOf(pCtx);
	const char *zStr = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
	int rc = -1;
	if( pXw && pXw->pWriter ){
		rc = xmlTextWriterSetIndentString(pXw->pWriter,(const xmlChar *)zStr);
	}
	ph7_result_bool(pCtx,rc >= 0);
	return PH7_OK;
}
/* bool __xw_start_document(res,?version,?encoding,?standalone) */
static int vm_builtin_xw_start_document(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_xmlwriter *pXw = XmlWriterOf(pCtx);
	const char *zVer = (nArg > 0 && !ph7_value_is_null(apArg[0])) ? ph7_value_to_string(apArg[0],0) : 0;
	const char *zEnc = (nArg > 1 && !ph7_value_is_null(apArg[1])) ? ph7_value_to_string(apArg[1],0) : 0;
	const char *zStd = (nArg > 2 && !ph7_value_is_null(apArg[2])) ? ph7_value_to_string(apArg[2],0) : 0;
	int rc = -1;
	if( pXw && pXw->pWriter ){
		rc = xmlTextWriterStartDocument(pXw->pWriter,zVer,zEnc,zStd);
	}
	ph7_result_bool(pCtx,rc >= 0);
	return PH7_OK;
}
/* bool __xw_end_document(res) */
static int vm_builtin_xw_end_document(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	phl_xmlwriter *pXw = XmlWriterOf(pCtx);
	int rc = (pXw && pXw->pWriter) ? xmlTextWriterEndDocument(pXw->pWriter) : -1;
	ph7_result_bool(pCtx,rc >= 0);
	return PH7_OK;
}
/* bool __xw_start_element(res,name) */
static int vm_builtin_xw_start_element(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_xmlwriter *pXw = XmlWriterOf(pCtx);
	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
	int rc = (pXw && pXw->pWriter) ? xmlTextWriterStartElement(pXw->pWriter,(const xmlChar *)zName) : -1;
	ph7_result_bool(pCtx,rc >= 0);
	return PH7_OK;
}
/* bool __xw_end_element(res) / __xw_full_end_element(res) */
static int vm_builtin_xw_end_element(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	phl_xmlwriter *pXw = XmlWriterOf(pCtx);
	int rc = (pXw && pXw->pWriter) ? xmlTextWriterEndElement(pXw->pWriter) : -1;
	ph7_result_bool(pCtx,rc >= 0);
	return PH7_OK;
}
static int vm_builtin_xw_full_end_element(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	phl_xmlwriter *pXw = XmlWriterOf(pCtx);
	int rc = (pXw && pXw->pWriter) ? xmlTextWriterFullEndElement(pXw->pWriter) : -1;
	ph7_result_bool(pCtx,rc >= 0);
	return PH7_OK;
}
/* bool __xw_write_attribute(res,name,value) */
static int vm_builtin_xw_write_attribute(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_xmlwriter *pXw = XmlWriterOf(pCtx);
	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[0],0) : "";
	const char *zVal = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";
	int rc = (pXw && pXw->pWriter)
		? xmlTextWriterWriteAttribute(pXw->pWriter,(const xmlChar *)zName,(const xmlChar *)zVal) : -1;
	ph7_result_bool(pCtx,rc >= 0);
	return PH7_OK;
}
/* bool __xw_write_element(res,name,?content) */
static int vm_builtin_xw_write_element(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_xmlwriter *pXw = XmlWriterOf(pCtx);
	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
	int rc = -1;
	if( pXw && pXw->pWriter ){
		if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){
			const char *zContent = ph7_value_to_string(apArg[1],0);
			rc = xmlTextWriterWriteElement(pXw->pWriter,(const xmlChar *)zName,(const xmlChar *)zContent);
		}else{
			/* Empty element: start + end so it serializes as <name/> */
			rc = xmlTextWriterStartElement(pXw->pWriter,(const xmlChar *)zName);
			if( rc >= 0 ){
				rc = xmlTextWriterEndElement(pXw->pWriter);
			}
		}
	}
	ph7_result_bool(pCtx,rc >= 0);
	return PH7_OK;
}
/* bool __xw_text(res,content) */
static int vm_builtin_xw_text(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_xmlwriter *pXw = XmlWriterOf(pCtx);
	const char *zText = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
	int rc = (pXw && pXw->pWriter) ? xmlTextWriterWriteString(pXw->pWriter,(const xmlChar *)zText) : -1;
	ph7_result_bool(pCtx,rc >= 0);
	return PH7_OK;
}
/* bool __xw_write_raw(res,content) */
static int vm_builtin_xw_write_raw(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_xmlwriter *pXw = XmlWriterOf(pCtx);
	const char *zText = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
	int rc = (pXw && pXw->pWriter) ? xmlTextWriterWriteRaw(pXw->pWriter,(const xmlChar *)zText) : -1;
	ph7_result_bool(pCtx,rc >= 0);
	return PH7_OK;
}
/* bool __xw_write_cdata(res,content) */
static int vm_builtin_xw_write_cdata(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_xmlwriter *pXw = XmlWriterOf(pCtx);
	const char *zText = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
	int rc = (pXw && pXw->pWriter) ? xmlTextWriterWriteCDATA(pXw->pWriter,(const xmlChar *)zText) : -1;
	ph7_result_bool(pCtx,rc >= 0);
	return PH7_OK;
}
/* bool __xw_write_comment(res,content) */
static int vm_builtin_xw_write_comment(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_xmlwriter *pXw = XmlWriterOf(pCtx);
	const char *zText = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
	int rc = (pXw && pXw->pWriter) ? xmlTextWriterWriteComment(pXw->pWriter,(const xmlChar *)zText) : -1;
	ph7_result_bool(pCtx,rc >= 0);
	return PH7_OK;
}
/* string __xw_output_memory(res,flush) -- read back the in-memory buffer */
static int vm_builtin_xw_output_memory(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_xmlwriter *pXw = XmlWriterOf(pCtx);
	int bFlush = nArg > 0 ? ph7_value_to_bool(apArg[0]) : 1;
	if( pXw == 0 || pXw->pBuf == 0 ){
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Flush the writer into the buffer before reading (php does this) */
	if( pXw->pWriter ){
		xmlTextWriterFlush(pXw->pWriter);
	}
	ph7_result_string(pCtx,(const char *)xmlBufferContent(pXw->pBuf),(int)xmlBufferLength(pXw->pBuf));
	if( bFlush ){
		xmlBufferEmpty(pXw->pBuf);
	}
	return PH7_OK;
}
/* int __xw_flush(res,empty) -- flush; returns bytes written (memory writer) */
static int vm_builtin_xw_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_xmlwriter *pXw = XmlWriterOf(pCtx);
	int bEmpty = nArg > 0 ? ph7_value_to_bool(apArg[0]) : 1;
	int nOut = 0;
	if( pXw && pXw->pWriter ){
		nOut = xmlTextWriterFlush(pXw->pWriter);
	}
	if( pXw && pXw->pBuf ){
		/* Memory writer: php returns the buffer as a string from flush() */
		ph7_result_string(pCtx,(const char *)xmlBufferContent(pXw->pBuf),(int)xmlBufferLength(pXw->pBuf));
		if( bEmpty ){
			xmlBufferEmpty(pXw->pBuf);
		}
	}else{
		ph7_result_int(pCtx,nOut);
	}
	return PH7_OK;
}

/*
 * The XMLWriter class: a thin prelude over the __xw_* thunks.  Only the
 * memory API is exposed (openMemory) -- PHPUnit never writes to a URI/file
 * via XMLWriter, and that path is a Milestone-2 addition.
 */
/* XMLWriter is declared entirely from C by PH7_VmInstallXmlWriter below. It was
 * an embedded PHP class whose every method forwarded to a global __xw_ thunk. */

/*
 * Install the XMLWriter library.  Called from PH7_VmInit inside the
 * bCompilingBuiltin window, after PH7_VmInstallLibxml.
 */
PH7_PRIVATE sxi32 PH7_VmInstallXmlWriter(ph7_vm *pVm)
{
	/* php's own signatures. Declaring them is what gives these methods argument
	 * coercion and a too-few/too-many ArgumentCountError; the prelude hand-cast
	 * every argument ((string)$name, (bool)$enable) and enforced no arity at all. */
	static const PH7_NativeMethodDef aMethod[] = {
		{ "openMemory",      PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_open_memory },
		{ "setIndent",       PH7_MOD_PUBLIC, "bool $enable", "@bool", vm_builtin_xw_set_indent },
		{ "setIndentString", PH7_MOD_PUBLIC, "string $indentation", "@bool", vm_builtin_xw_set_indent_string },
		{ "startDocument",   PH7_MOD_PUBLIC,
		  "?string $version = null, ?string $encoding = null, ?string $standalone = null",
		  "@bool", vm_builtin_xw_start_document },
		{ "endDocument",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_end_document },
		{ "startElement",    PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_xw_start_element },
		{ "endElement",      PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_end_element },
		{ "fullEndElement",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_full_end_element },
		{ "writeAttribute",  PH7_MOD_PUBLIC, "string $name, string $value", "@bool", vm_builtin_xw_write_attribute },
		{ "writeElement",    PH7_MOD_PUBLIC, "string $name, ?string $content = null", "@bool", vm_builtin_xw_write_element },
		{ "text",            PH7_MOD_PUBLIC, "string $content", "@bool", vm_builtin_xw_text },
		{ "writeRaw",        PH7_MOD_PUBLIC, "string $content", "@bool", vm_builtin_xw_write_raw },
		{ "writeCdata",      PH7_MOD_PUBLIC, "string $content", "@bool", vm_builtin_xw_write_cdata },
		{ "writeComment",    PH7_MOD_PUBLIC, "string $content", "@bool", vm_builtin_xw_write_comment },
		{ "outputMemory",    PH7_MOD_PUBLIC, "bool $flush = true", "@string", vm_builtin_xw_output_memory },
		{ "flush",           PH7_MOD_PUBLIC, "bool $empty = true", "@string|int", vm_builtin_xw_flush },
	};
	/* The libxml writer handle: storage the class owns, kept public because the
	 * prelude declared it so. */
	static const PH7_NativePropDef aProp[] = {
		{ "__res", PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
	};
	static const PH7_NativeClassSpec sSpec = {
		"XMLWriter", 0, 0, 0,
		aMethod, SX_ARRAYSIZE(aMethod),
		0, 0,
		aProp, SX_ARRAYSIZE(aProp),
		0, 0
	};
	return PH7_InstallNativeClasses(&(*pVm),&sSpec,1);
}

#else
/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */
typedef int vm_xmlwriter_unused;
#endif /* PH7_ENABLE_LIBXML */
