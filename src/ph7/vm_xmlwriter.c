/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifdef PH7_ENABLE_LIBXML
#include "ph7int.h"
#include <libxml/tree.h>
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
/*
 * One call into a writer verb.
 *
 * php implements the class and the procedural surface with ONE C function per
 * verb: the method spelling reaches the writer through `$this`, the function
 * spelling takes it as argument #1 and shifts every other argument up by one.
 * The two therefore share their diagnostics, and that is visible from PHP --
 * see XmlWriterCheckName for the off-by-one it leaves in the method's own
 * error text. A verb body reads its arguments through this descriptor so both
 * entry points can drive it.
 */
typedef struct xw_call xw_call;
struct xw_call {
	phl_xmlwriter *pXw;    /* the resolved writer; never NULL in a verb body */
	const char *zFn;       /* "XMLWriter::startElement" / "xmlwriter_start_element" */
	const char *zNameArg;  /* this spelling's argument text for the NAME argument */
	int nArg;              /* how many arguments follow the writer */
	ph7_value **apArg;     /* the first argument past the writer */
};
/*
 * php's Z_XMLWRITER_P: an XMLWriter that was never opened has no writer behind
 * it, and every method and every procedural entry refuses it with this Error
 * rather than answering false -- which is what a caller would otherwise store
 * or print as if a document had been written.
 */
static int XmlWriterMissing(ph7_context *pCtx)
{
	return PH7_VmThrowException(pCtx,"Error","Invalid or uninitialized XMLWriter object");
}
/*
 * Build the call descriptor for the METHOD spelling. Returns 0 when the verb
 * may run, or the throw status when the receiver holds no writer.
 */
static int XwCallFromThis(ph7_context *pCtx,xw_call *pCall,const char *zFn,
	const char *zNameArg,int nArg,ph7_value **apArg)
{
	phl_xmlwriter *pXw = XmlWriterOf(pCtx);
	if( pXw == 0 || pXw->pWriter == 0 ){
		return XmlWriterMissing(pCtx);
	}
	pCall->pXw = pXw;
	pCall->zFn = zFn;
	pCall->zNameArg = zNameArg;
	pCall->nArg = nArg;
	pCall->apArg = apArg;
	return 0;
}
/* The n-th argument past the writer as a string, or "" when it is absent. */
static const char * XwStr(xw_call *pCall,int iArg)
{
	if( iArg >= pCall->nArg || pCall->apArg[iArg] == 0 ){
		return "";
	}
	return ph7_value_to_string(pCall->apArg[iArg],0);
}
/* The n-th argument past the writer as a string, or NULL for an absent/null one. */
static const char * XwStrOrNull(xw_call *pCall,int iArg)
{
	if( iArg >= pCall->nArg || pCall->apArg[iArg] == 0
	 || ph7_value_is_null(pCall->apArg[iArg]) ){
		return 0;
	}
	return ph7_value_to_string(pCall->apArg[iArg],0);
}
/* The n-th argument past the writer as a bool. */
static int XwBool(xw_call *pCall,int iArg,int bDefault)
{
	if( iArg >= pCall->nArg || pCall->apArg[iArg] == 0 ){
		return bDefault;
	}
	return ph7_value_to_bool(pCall->apArg[iArg]) ? 1 : 0;
}
/*
 * php's XMLW_NAME_CHK: the name a verb is handed is validated with libxml's own
 * xmlValidateName and refused with a ValueError before anything is written --
 * the alternative is what this engine used to do, which is to hand libxml a name
 * it will not quote and emit a document that is not XML (`<1bad`, `<a x y="v"`).
 *
 * The argument NUMBER php reports is the PROCEDURAL one, hardcoded in the macro,
 * so the method spelling reports its own first argument as "#2" and prints
 * whichever of its OWN parameters sits at that shifted position -- `$content`,
 * `$value`, `$isParam` -- or no name at all when it has none. Reproduced as
 * written: each entry point states its spelling's text.
 */
static int XmlWriterCheckName(ph7_context *pCtx,xw_call *pCall,const char *zName,
	const char *zWhat)
{
	if( xmlValidateName((const xmlChar *)zName,0) == 0 ){
		return 0;
	}
	return PH7_VmThrowException(pCtx,"ValueError",
		"%s(): Argument %s must be a valid %s, \"%s\" given",
		pCall->zFn,pCall->zNameArg,zWhat,zName);
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
/*
 * Run one verb inside a libxml error capture window, the way php's own
 * xmlwriter does: libxml reports a refusal it can explain (a DTD with a public
 * identifier and no system one, say) through the error handler rather than the
 * return value, and php turns each one into a warning naming the caller.
 */
static int XwRun(ph7_context *pCtx,xw_call *pCall,int (*xVerb)(ph7_context *,xw_call *))
{
	sxu32 nMark = PH7_LibxmlCaptureBegin(pCtx->pVm);
	int rc = xVerb(pCtx,pCall);
	PH7_LibxmlCaptureEnd(pCtx->pVm,nMark,pCall->zFn);
	return rc;
}
/* Report a libxml writer call the way php does: its int status as a bool. */
static int XwStatus(ph7_context *pCtx,int rc)
{
	ph7_result_bool(pCtx,rc >= 0);
	return PH7_OK;
}
/*
 * One method entry point. The receiver has to hold a writer (php's Error), and
 * the verb then reads its arguments from #0 up -- the procedural entry, added
 * with the rest of that surface, hands it the same descriptor one slot along.
 */
#define XW_METHOD(cfn,verb,meth,namearg)                                          \
static int cfn(ph7_context *pCtx,int nArg,ph7_value **apArg)                      \
{                                                                                 \
	xw_call sCall;                                                            \
	int rc;                                                                   \
	SyZero(&sCall,sizeof(sCall));                                             \
	rc = XwCallFromThis(pCtx,&sCall,"XMLWriter::" meth,namearg,nArg,apArg);   \
	if( rc != 0 ){                                                            \
		return rc;                                                        \
	}                                                                         \
	return XwRun(pCtx,&sCall,verb);                                           \
}

/* bool XMLWriter::setIndent(bool $enable) */
static int XwSetIndent(ph7_context *pCtx,xw_call *pCall)
{
	/* No indent STRING is set here: libxml's own default is the single space
	 * php answers with, and writing it back made setIndentString() before
	 * setIndent() -- the documented order, and the one php's own examples use --
	 * silently lose the string it had just been given. */
	return XwStatus(pCtx,xmlTextWriterSetIndent(pCall->pXw->pWriter,XwBool(pCall,0,0)));
}
XW_METHOD(vm_builtin_xw_set_indent,XwSetIndent,"setIndent",0)

/* bool XMLWriter::setIndentString(string $indentation) */
static int XwSetIndentString(ph7_context *pCtx,xw_call *pCall)
{
	return XwStatus(pCtx,xmlTextWriterSetIndentString(pCall->pXw->pWriter,
		(const xmlChar *)XwStr(pCall,0)));
}
XW_METHOD(vm_builtin_xw_set_indent_string,XwSetIndentString,"setIndentString",0)

/* bool XMLWriter::startDocument(?string $version, ?string $encoding, ?string $standalone) */
static int XwStartDocument(ph7_context *pCtx,xw_call *pCall)
{
	return XwStatus(pCtx,xmlTextWriterStartDocument(pCall->pXw->pWriter,
		XwStrOrNull(pCall,0),XwStrOrNull(pCall,1),XwStrOrNull(pCall,2)));
}
XW_METHOD(vm_builtin_xw_start_document,XwStartDocument,"startDocument",0)

/* bool XMLWriter::endDocument() */
static int XwEndDocument(ph7_context *pCtx,xw_call *pCall)
{
	return XwStatus(pCtx,xmlTextWriterEndDocument(pCall->pXw->pWriter));
}
XW_METHOD(vm_builtin_xw_end_document,XwEndDocument,"endDocument",0)

/* bool XMLWriter::startComment() */
static int XwStartComment(ph7_context *pCtx,xw_call *pCall)
{
	return XwStatus(pCtx,xmlTextWriterStartComment(pCall->pXw->pWriter));
}
XW_METHOD(vm_builtin_xw_start_comment,XwStartComment,"startComment",0)

/* bool XMLWriter::endComment() */
static int XwEndComment(ph7_context *pCtx,xw_call *pCall)
{
	return XwStatus(pCtx,xmlTextWriterEndComment(pCall->pXw->pWriter));
}
XW_METHOD(vm_builtin_xw_end_comment,XwEndComment,"endComment",0)

/* bool XMLWriter::startAttribute(string $name) */
static int XwStartAttribute(ph7_context *pCtx,xw_call *pCall)
{
	const char *zName = XwStr(pCall,0);
	int rc = XmlWriterCheckName(pCtx,pCall,zName,"attribute name");
	if( rc != 0 ){
		return rc;
	}
	return XwStatus(pCtx,xmlTextWriterStartAttribute(pCall->pXw->pWriter,(const xmlChar *)zName));
}
XW_METHOD(vm_builtin_xw_start_attribute,XwStartAttribute,"startAttribute","#2")

/* bool XMLWriter::endAttribute() */
static int XwEndAttribute(ph7_context *pCtx,xw_call *pCall)
{
	return XwStatus(pCtx,xmlTextWriterEndAttribute(pCall->pXw->pWriter));
}
XW_METHOD(vm_builtin_xw_end_attribute,XwEndAttribute,"endAttribute",0)

/*
 * bool XMLWriter::startAttributeNs(?string $prefix, string $name, ?string $namespace)
 *
 * Only the local NAME is validated -- php checks neither the prefix nor the
 * namespace, so a prefix with a space in it reaches the document (`<x y:e`),
 * which is libxml's answer and therefore php's.
 */
static int XwStartAttributeNs(ph7_context *pCtx,xw_call *pCall)
{
	const char *zName = XwStr(pCall,1);
	int rc = XmlWriterCheckName(pCtx,pCall,zName,"attribute name");
	if( rc != 0 ){
		return rc;
	}
	return XwStatus(pCtx,xmlTextWriterStartAttributeNS(pCall->pXw->pWriter,
		(const xmlChar *)XwStrOrNull(pCall,0),(const xmlChar *)zName,
		(const xmlChar *)XwStrOrNull(pCall,2)));
}
XW_METHOD(vm_builtin_xw_start_attribute_ns,XwStartAttributeNs,"startAttributeNs","#3 ($namespace)")

/* bool XMLWriter::writeAttributeNs(?string $prefix, string $name, ?string $namespace, string $value) */
static int XwWriteAttributeNs(ph7_context *pCtx,xw_call *pCall)
{
	const char *zName = XwStr(pCall,1);
	int rc = XmlWriterCheckName(pCtx,pCall,zName,"attribute name");
	if( rc != 0 ){
		return rc;
	}
	return XwStatus(pCtx,xmlTextWriterWriteAttributeNS(pCall->pXw->pWriter,
		(const xmlChar *)XwStrOrNull(pCall,0),(const xmlChar *)zName,
		(const xmlChar *)XwStrOrNull(pCall,2),(const xmlChar *)XwStr(pCall,3)));
}
XW_METHOD(vm_builtin_xw_write_attribute_ns,XwWriteAttributeNs,"writeAttributeNs","#3 ($namespace)")

/* bool XMLWriter::startElement(string $name) */
static int XwStartElement(ph7_context *pCtx,xw_call *pCall)
{
	const char *zName = XwStr(pCall,0);
	int rc = XmlWriterCheckName(pCtx,pCall,zName,"element name");
	if( rc != 0 ){
		return rc;
	}
	return XwStatus(pCtx,xmlTextWriterStartElement(pCall->pXw->pWriter,(const xmlChar *)zName));
}
XW_METHOD(vm_builtin_xw_start_element,XwStartElement,"startElement","#2")

/* bool XMLWriter::endElement() */
static int XwEndElement(ph7_context *pCtx,xw_call *pCall)
{
	return XwStatus(pCtx,xmlTextWriterEndElement(pCall->pXw->pWriter));
}
XW_METHOD(vm_builtin_xw_end_element,XwEndElement,"endElement",0)

/* bool XMLWriter::fullEndElement() */
static int XwFullEndElement(ph7_context *pCtx,xw_call *pCall)
{
	return XwStatus(pCtx,xmlTextWriterFullEndElement(pCall->pXw->pWriter));
}
XW_METHOD(vm_builtin_xw_full_end_element,XwFullEndElement,"fullEndElement",0)

/* bool XMLWriter::writeAttribute(string $name, string $value) */
static int XwWriteAttribute(ph7_context *pCtx,xw_call *pCall)
{
	const char *zName = XwStr(pCall,0);
	int rc = XmlWriterCheckName(pCtx,pCall,zName,"attribute name");
	if( rc != 0 ){
		return rc;
	}
	return XwStatus(pCtx,xmlTextWriterWriteAttribute(pCall->pXw->pWriter,
		(const xmlChar *)zName,(const xmlChar *)XwStr(pCall,1)));
}
XW_METHOD(vm_builtin_xw_write_attribute,XwWriteAttribute,"writeAttribute","#2 ($value)")

/* bool XMLWriter::writeElement(string $name, ?string $content = null) */
static int XwWriteElement(ph7_context *pCtx,xw_call *pCall)
{
	const char *zName = XwStr(pCall,0);
	const char *zContent = XwStrOrNull(pCall,1);
	int rc = XmlWriterCheckName(pCtx,pCall,zName,"element name");
	if( rc != 0 ){
		return rc;
	}
	if( zContent ){
		rc = xmlTextWriterWriteElement(pCall->pXw->pWriter,(const xmlChar *)zName,
			(const xmlChar *)zContent);
	}else{
		/* Empty element: start + end so it serializes as <name/> */
		rc = xmlTextWriterStartElement(pCall->pXw->pWriter,(const xmlChar *)zName);
		if( rc >= 0 ){
			rc = xmlTextWriterEndElement(pCall->pXw->pWriter);
		}
	}
	return XwStatus(pCtx,rc);
}
XW_METHOD(vm_builtin_xw_write_element,XwWriteElement,"writeElement","#2 ($content)")

/*
 * bool XMLWriter::startElementNs(?string $prefix, string $name, ?string $namespace)
 *
 * libxml declares the namespace on the element it opens, so a null $namespace
 * writes the prefixed name alone -- which is how a document declares a prefix
 * once at the root and uses it below.
 */
static int XwStartElementNs(ph7_context *pCtx,xw_call *pCall)
{
	const char *zName = XwStr(pCall,1);
	int rc = XmlWriterCheckName(pCtx,pCall,zName,"element name");
	if( rc != 0 ){
		return rc;
	}
	return XwStatus(pCtx,xmlTextWriterStartElementNS(pCall->pXw->pWriter,
		(const xmlChar *)XwStrOrNull(pCall,0),(const xmlChar *)zName,
		(const xmlChar *)XwStrOrNull(pCall,2)));
}
XW_METHOD(vm_builtin_xw_start_element_ns,XwStartElementNs,"startElementNs","#3 ($namespace)")

/* bool XMLWriter::writeElementNs(?string $prefix, string $name, ?string $namespace, ?string $content = null) */
static int XwWriteElementNs(ph7_context *pCtx,xw_call *pCall)
{
	const char *zName = XwStr(pCall,1);
	const char *zContent = XwStrOrNull(pCall,3);
	int rc = XmlWriterCheckName(pCtx,pCall,zName,"element name");
	if( rc != 0 ){
		return rc;
	}
	if( zContent ){
		rc = xmlTextWriterWriteElementNS(pCall->pXw->pWriter,
			(const xmlChar *)XwStrOrNull(pCall,0),(const xmlChar *)zName,
			(const xmlChar *)XwStrOrNull(pCall,2),(const xmlChar *)zContent);
	}else{
		/* No content: the empty element php writes, `<p:e xmlns:p="urn"/>` */
		rc = xmlTextWriterStartElementNS(pCall->pXw->pWriter,
			(const xmlChar *)XwStrOrNull(pCall,0),(const xmlChar *)zName,
			(const xmlChar *)XwStrOrNull(pCall,2));
		if( rc >= 0 ){
			rc = xmlTextWriterEndElement(pCall->pXw->pWriter);
		}
	}
	return XwStatus(pCtx,rc);
}
XW_METHOD(vm_builtin_xw_write_element_ns,XwWriteElementNs,"writeElementNs","#3 ($namespace)")

/*
 * bool XMLWriter::startPi(string $target)
 *
 * The target is checked with the same xmlValidateName the element and attribute
 * names go through -- php names it a "PI target" and nothing else changes, so
 * `<?php ... ?>` is spellable and `<?x y ... ?>` is a ValueError.
 */
static int XwStartPi(ph7_context *pCtx,xw_call *pCall)
{
	const char *zTarget = XwStr(pCall,0);
	int rc = XmlWriterCheckName(pCtx,pCall,zTarget,"PI target");
	if( rc != 0 ){
		return rc;
	}
	return XwStatus(pCtx,xmlTextWriterStartPI(pCall->pXw->pWriter,(const xmlChar *)zTarget));
}
XW_METHOD(vm_builtin_xw_start_pi,XwStartPi,"startPi","#2")

/* bool XMLWriter::endPi() */
static int XwEndPi(ph7_context *pCtx,xw_call *pCall)
{
	return XwStatus(pCtx,xmlTextWriterEndPI(pCall->pXw->pWriter));
}
XW_METHOD(vm_builtin_xw_end_pi,XwEndPi,"endPi",0)

/* bool XMLWriter::writePi(string $target, string $content) */
static int XwWritePi(ph7_context *pCtx,xw_call *pCall)
{
	const char *zTarget = XwStr(pCall,0);
	int rc = XmlWriterCheckName(pCtx,pCall,zTarget,"PI target");
	if( rc != 0 ){
		return rc;
	}
	return XwStatus(pCtx,xmlTextWriterWritePI(pCall->pXw->pWriter,
		(const xmlChar *)zTarget,(const xmlChar *)XwStr(pCall,1)));
}
XW_METHOD(vm_builtin_xw_write_pi,XwWritePi,"writePi","#2 ($content)")

/* bool XMLWriter::startCdata() */
static int XwStartCdata(ph7_context *pCtx,xw_call *pCall)
{
	return XwStatus(pCtx,xmlTextWriterStartCDATA(pCall->pXw->pWriter));
}
XW_METHOD(vm_builtin_xw_start_cdata,XwStartCdata,"startCdata",0)

/* bool XMLWriter::endCdata() */
static int XwEndCdata(ph7_context *pCtx,xw_call *pCall)
{
	return XwStatus(pCtx,xmlTextWriterEndCDATA(pCall->pXw->pWriter));
}
XW_METHOD(vm_builtin_xw_end_cdata,XwEndCdata,"endCdata",0)

/* bool XMLWriter::text(string $content) */
static int XwText(ph7_context *pCtx,xw_call *pCall)
{
	return XwStatus(pCtx,xmlTextWriterWriteString(pCall->pXw->pWriter,
		(const xmlChar *)XwStr(pCall,0)));
}
XW_METHOD(vm_builtin_xw_text,XwText,"text",0)

/* bool XMLWriter::writeRaw(string $content) */
static int XwWriteRaw(ph7_context *pCtx,xw_call *pCall)
{
	return XwStatus(pCtx,xmlTextWriterWriteRaw(pCall->pXw->pWriter,
		(const xmlChar *)XwStr(pCall,0)));
}
XW_METHOD(vm_builtin_xw_write_raw,XwWriteRaw,"writeRaw",0)

/* bool XMLWriter::writeCdata(string $content) */
static int XwWriteCdata(ph7_context *pCtx,xw_call *pCall)
{
	return XwStatus(pCtx,xmlTextWriterWriteCDATA(pCall->pXw->pWriter,
		(const xmlChar *)XwStr(pCall,0)));
}
XW_METHOD(vm_builtin_xw_write_cdata,XwWriteCdata,"writeCdata",0)

/* bool XMLWriter::writeComment(string $content) */
static int XwWriteComment(ph7_context *pCtx,xw_call *pCall)
{
	return XwStatus(pCtx,xmlTextWriterWriteComment(pCall->pXw->pWriter,
		(const xmlChar *)XwStr(pCall,0)));
}
XW_METHOD(vm_builtin_xw_write_comment,XwWriteComment,"writeComment",0)

/*
 * The DTD twelve.
 *
 * libxml decides the shape of every one of these, and two of its decisions are
 * only reported through the error handler: a DOCTYPE with a public identifier
 * and no system one ("system identifier needed!"), and a DTD opened once the
 * root element has been written ("DTD allowed only in prolog!"). Both come out
 * as php's warning through the capture window in XwRun.
 *
 * php validates the name of the internal-subset declarations and NOT the
 * DOCTYPE's own qualified name, so `startDtd('x y')` writes `<!DOCTYPE x y`
 * while `startDtdElement('x y')` is a ValueError -- and the ValueError for
 * startDtdEntity says "attribute name" where its neighbours say "element name",
 * because php reaches for a different macro there.
 */
/* bool XMLWriter::startDtd(string $qualifiedName, ?string $publicId, ?string $systemId) */
static int XwStartDtd(ph7_context *pCtx,xw_call *pCall)
{
	return XwStatus(pCtx,xmlTextWriterStartDTD(pCall->pXw->pWriter,
		(const xmlChar *)XwStr(pCall,0),(const xmlChar *)XwStrOrNull(pCall,1),
		(const xmlChar *)XwStrOrNull(pCall,2)));
}
XW_METHOD(vm_builtin_xw_start_dtd,XwStartDtd,"startDtd",0)

/* bool XMLWriter::endDtd() */
static int XwEndDtd(ph7_context *pCtx,xw_call *pCall)
{
	return XwStatus(pCtx,xmlTextWriterEndDTD(pCall->pXw->pWriter));
}
XW_METHOD(vm_builtin_xw_end_dtd,XwEndDtd,"endDtd",0)

/* bool XMLWriter::writeDtd(string $name, ?string $publicId, ?string $systemId, ?string $content) */
static int XwWriteDtd(ph7_context *pCtx,xw_call *pCall)
{
	return XwStatus(pCtx,xmlTextWriterWriteDTD(pCall->pXw->pWriter,
		(const xmlChar *)XwStr(pCall,0),(const xmlChar *)XwStrOrNull(pCall,1),
		(const xmlChar *)XwStrOrNull(pCall,2),(const xmlChar *)XwStrOrNull(pCall,3)));
}
XW_METHOD(vm_builtin_xw_write_dtd,XwWriteDtd,"writeDtd",0)

/* bool XMLWriter::startDtdElement(string $qualifiedName) */
static int XwStartDtdElement(ph7_context *pCtx,xw_call *pCall)
{
	const char *zName = XwStr(pCall,0);
	int rc = XmlWriterCheckName(pCtx,pCall,zName,"element name");
	if( rc != 0 ){
		return rc;
	}
	return XwStatus(pCtx,xmlTextWriterStartDTDElement(pCall->pXw->pWriter,(const xmlChar *)zName));
}
XW_METHOD(vm_builtin_xw_start_dtd_element,XwStartDtdElement,"startDtdElement","#2")

/* bool XMLWriter::endDtdElement() */
static int XwEndDtdElement(ph7_context *pCtx,xw_call *pCall)
{
	return XwStatus(pCtx,xmlTextWriterEndDTDElement(pCall->pXw->pWriter));
}
XW_METHOD(vm_builtin_xw_end_dtd_element,XwEndDtdElement,"endDtdElement",0)

/* bool XMLWriter::writeDtdElement(string $name, string $content) */
static int XwWriteDtdElement(ph7_context *pCtx,xw_call *pCall)
{
	const char *zName = XwStr(pCall,0);
	int rc = XmlWriterCheckName(pCtx,pCall,zName,"element name");
	if( rc != 0 ){
		return rc;
	}
	return XwStatus(pCtx,xmlTextWriterWriteDTDElement(pCall->pXw->pWriter,
		(const xmlChar *)zName,(const xmlChar *)XwStr(pCall,1)));
}
XW_METHOD(vm_builtin_xw_write_dtd_element,XwWriteDtdElement,"writeDtdElement","#2 ($content)")

/* bool XMLWriter::startDtdAttlist(string $name) */
static int XwStartDtdAttlist(ph7_context *pCtx,xw_call *pCall)
{
	const char *zName = XwStr(pCall,0);
	int rc = XmlWriterCheckName(pCtx,pCall,zName,"element name");
	if( rc != 0 ){
		return rc;
	}
	return XwStatus(pCtx,xmlTextWriterStartDTDAttlist(pCall->pXw->pWriter,(const xmlChar *)zName));
}
XW_METHOD(vm_builtin_xw_start_dtd_attlist,XwStartDtdAttlist,"startDtdAttlist","#2")

/* bool XMLWriter::endDtdAttlist() */
static int XwEndDtdAttlist(ph7_context *pCtx,xw_call *pCall)
{
	return XwStatus(pCtx,xmlTextWriterEndDTDAttlist(pCall->pXw->pWriter));
}
XW_METHOD(vm_builtin_xw_end_dtd_attlist,XwEndDtdAttlist,"endDtdAttlist",0)

/* bool XMLWriter::writeDtdAttlist(string $name, string $content) */
static int XwWriteDtdAttlist(ph7_context *pCtx,xw_call *pCall)
{
	const char *zName = XwStr(pCall,0);
	int rc = XmlWriterCheckName(pCtx,pCall,zName,"element name");
	if( rc != 0 ){
		return rc;
	}
	return XwStatus(pCtx,xmlTextWriterWriteDTDAttlist(pCall->pXw->pWriter,
		(const xmlChar *)zName,(const xmlChar *)XwStr(pCall,1)));
}
XW_METHOD(vm_builtin_xw_write_dtd_attlist,XwWriteDtdAttlist,"writeDtdAttlist","#2 ($content)")

/* bool XMLWriter::startDtdEntity(string $name, bool $isParam) */
static int XwStartDtdEntity(ph7_context *pCtx,xw_call *pCall)
{
	const char *zName = XwStr(pCall,0);
	int rc = XmlWriterCheckName(pCtx,pCall,zName,"attribute name");
	if( rc != 0 ){
		return rc;
	}
	return XwStatus(pCtx,xmlTextWriterStartDTDEntity(pCall->pXw->pWriter,
		XwBool(pCall,1,0),(const xmlChar *)zName));
}
XW_METHOD(vm_builtin_xw_start_dtd_entity,XwStartDtdEntity,"startDtdEntity","#2 ($isParam)")

/* bool XMLWriter::endDtdEntity() */
static int XwEndDtdEntity(ph7_context *pCtx,xw_call *pCall)
{
	return XwStatus(pCtx,xmlTextWriterEndDTDEntity(pCall->pXw->pWriter));
}
XW_METHOD(vm_builtin_xw_end_dtd_entity,XwEndDtdEntity,"endDtdEntity",0)

/*
 * bool XMLWriter::writeDtdEntity(string $name, string $content, bool $isParam = false,
 *                                ?string $publicId = null, ?string $systemId = null,
 *                                ?string $notationData = null)
 *
 * php has two calls behind this one name, and picks by whether a public or
 * system identifier is there: with neither it writes the INTERNAL entity (the
 * $content), and with either it writes the EXTERNAL declaration -- where
 * $content is not written at all. $notationData does NOT decide, so a call that
 * names only a notation is still the internal entity and the notation is
 * dropped, which is php's answer and not an oversight of this port.
 */
static int XwWriteDtdEntity(ph7_context *pCtx,xw_call *pCall)
{
	const char *zName = XwStr(pCall,0);
	const char *zPub = XwStrOrNull(pCall,3);
	const char *zSys = XwStrOrNull(pCall,4);
	const char *zNdata = XwStrOrNull(pCall,5);
	int rc = XmlWriterCheckName(pCtx,pCall,zName,"element name");
	if( rc != 0 ){
		return rc;
	}
	if( zPub == 0 && zSys == 0 ){
		rc = xmlTextWriterWriteDTDInternalEntity(pCall->pXw->pWriter,XwBool(pCall,2,0),
			(const xmlChar *)zName,(const xmlChar *)XwStr(pCall,1));
	}else{
		rc = xmlTextWriterWriteDTDExternalEntity(pCall->pXw->pWriter,XwBool(pCall,2,0),
			(const xmlChar *)zName,(const xmlChar *)zPub,(const xmlChar *)zSys,
			(const xmlChar *)zNdata);
	}
	return XwStatus(pCtx,rc);
}
XW_METHOD(vm_builtin_xw_write_dtd_entity,XwWriteDtdEntity,"writeDtdEntity","#2 ($content)")

/* string XMLWriter::outputMemory(bool $flush = true) -- read the buffer back */
static int XwOutputMemory(ph7_context *pCtx,xw_call *pCall)
{
	phl_xmlwriter *pXw = pCall->pXw;
	int bFlush = XwBool(pCall,0,1);
	if( pXw->pBuf == 0 ){
		/* Not an in-memory writer: php answers the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Flush the writer into the buffer before reading (php does this) */
	xmlTextWriterFlush(pXw->pWriter);
	ph7_result_string(pCtx,(const char *)xmlBufferContent(pXw->pBuf),(int)xmlBufferLength(pXw->pBuf));
	if( bFlush ){
		xmlBufferEmpty(pXw->pBuf);
	}
	return PH7_OK;
}
XW_METHOD(vm_builtin_xw_output_memory,XwOutputMemory,"outputMemory",0)

/* string|int XMLWriter::flush(bool $empty = true) */
static int XwFlush(ph7_context *pCtx,xw_call *pCall)
{
	phl_xmlwriter *pXw = pCall->pXw;
	int bEmpty = XwBool(pCall,0,1);
	int nOut = xmlTextWriterFlush(pXw->pWriter);
	if( pXw->pBuf ){
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
XW_METHOD(vm_builtin_xw_flush,XwFlush,"flush",0)

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
		/* Declared in php's own stub order: get_class_methods() and Reflection
		 * both answer declaration order, so the two engines list one surface. */
		{ "openMemory",      PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_open_memory },
		{ "setIndent",       PH7_MOD_PUBLIC, "bool $enable", "@bool", vm_builtin_xw_set_indent },
		{ "setIndentString", PH7_MOD_PUBLIC, "string $indentation", "@bool", vm_builtin_xw_set_indent_string },
		{ "startComment",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_start_comment },
		{ "endComment",      PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_end_comment },
		{ "startAttribute",  PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_xw_start_attribute },
		{ "endAttribute",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_end_attribute },
		{ "writeAttribute",  PH7_MOD_PUBLIC, "string $name, string $value", "@bool", vm_builtin_xw_write_attribute },
		{ "startAttributeNs", PH7_MOD_PUBLIC, "?string $prefix, string $name, ?string $namespace",
		  "@bool", vm_builtin_xw_start_attribute_ns },
		{ "writeAttributeNs", PH7_MOD_PUBLIC,
		  "?string $prefix, string $name, ?string $namespace, string $value",
		  "@bool", vm_builtin_xw_write_attribute_ns },
		{ "startElement",    PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_xw_start_element },
		{ "endElement",      PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_end_element },
		{ "fullEndElement",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_full_end_element },
		{ "startElementNs",  PH7_MOD_PUBLIC, "?string $prefix, string $name, ?string $namespace",
		  "@bool", vm_builtin_xw_start_element_ns },
		{ "writeElement",    PH7_MOD_PUBLIC, "string $name, ?string $content = null", "@bool", vm_builtin_xw_write_element },
		{ "writeElementNs",  PH7_MOD_PUBLIC,
		  "?string $prefix, string $name, ?string $namespace, ?string $content = null",
		  "@bool", vm_builtin_xw_write_element_ns },
		{ "startPi",         PH7_MOD_PUBLIC, "string $target", "@bool", vm_builtin_xw_start_pi },
		{ "endPi",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_end_pi },
		{ "writePi",         PH7_MOD_PUBLIC, "string $target, string $content", "@bool", vm_builtin_xw_write_pi },
		{ "startCdata",      PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_start_cdata },
		{ "endCdata",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_end_cdata },
		{ "writeCdata",      PH7_MOD_PUBLIC, "string $content", "@bool", vm_builtin_xw_write_cdata },
		{ "text",            PH7_MOD_PUBLIC, "string $content", "@bool", vm_builtin_xw_text },
		{ "writeRaw",        PH7_MOD_PUBLIC, "string $content", "@bool", vm_builtin_xw_write_raw },
		{ "startDocument",   PH7_MOD_PUBLIC,
		  "?string $version = null, ?string $encoding = null, ?string $standalone = null",
		  "@bool", vm_builtin_xw_start_document },
		{ "endDocument",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_end_document },
		{ "writeComment",    PH7_MOD_PUBLIC, "string $content", "@bool", vm_builtin_xw_write_comment },
		{ "startDtd",        PH7_MOD_PUBLIC,
		  "string $qualifiedName, ?string $publicId = null, ?string $systemId = null",
		  "@bool", vm_builtin_xw_start_dtd },
		{ "endDtd",          PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_end_dtd },
		{ "writeDtd",        PH7_MOD_PUBLIC,
		  "string $name, ?string $publicId = null, ?string $systemId = null, ?string $content = null",
		  "@bool", vm_builtin_xw_write_dtd },
		{ "startDtdElement", PH7_MOD_PUBLIC, "string $qualifiedName", "@bool", vm_builtin_xw_start_dtd_element },
		{ "endDtdElement",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_end_dtd_element },
		{ "writeDtdElement", PH7_MOD_PUBLIC, "string $name, string $content", "@bool",
		  vm_builtin_xw_write_dtd_element },
		{ "startDtdAttlist", PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_xw_start_dtd_attlist },
		{ "endDtdAttlist",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_end_dtd_attlist },
		{ "writeDtdAttlist", PH7_MOD_PUBLIC, "string $name, string $content", "@bool",
		  vm_builtin_xw_write_dtd_attlist },
		{ "startDtdEntity",  PH7_MOD_PUBLIC, "string $name, bool $isParam", "@bool",
		  vm_builtin_xw_start_dtd_entity },
		{ "endDtdEntity",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_end_dtd_entity },
		{ "writeDtdEntity",  PH7_MOD_PUBLIC,
		  "string $name, string $content, bool $isParam = false, ?string $publicId = null, "
		  "?string $systemId = null, ?string $notationData = null",
		  "@bool", vm_builtin_xw_write_dtd_entity },
		{ "outputMemory",    PH7_MOD_PUBLIC, "bool $flush = true", "@string", vm_builtin_xw_output_memory },
		{ "flush",           PH7_MOD_PUBLIC, "bool $empty = true", "@string|int", vm_builtin_xw_flush },
	};
	/* The libxml writer handle: storage the class owns, kept public because the
	 * prelude declared it so. */
	static const PH7_NativePropDef aProp[] = {
		{ "__res", PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
	};
	/* php's XMLWriter has no clone handler, so `clone $w` is the engine's own
	 * refusal there -- and it has to be one here too: the copy would carry the
	 * SAME libxml writer in its hidden slot, so the two objects would interleave
	 * their output into one document and the copy's own buffer would answer "". */
	static const PH7_NativeClassSpec sSpec = {
		"XMLWriter", 0, 0, PH7_CLASS_NOCLONE,
		aMethod, SX_ARRAYSIZE(aMethod),
		0, 0,
		aProp, SX_ARRAYSIZE(aProp),
		0, 0, 0
	};
	return PH7_InstallNativeClasses(&(*pVm),&sSpec,1);
}

#else
/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */
typedef int vm_xmlwriter_unused;
#endif /* PH7_ENABLE_LIBXML */
