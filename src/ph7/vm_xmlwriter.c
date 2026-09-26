/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifdef PH7_ENABLE_LIBXML
#include "ph7int.h"
#include <libxml/tree.h>
#include <libxml/xmlerror.h>
#include <libxml/xmlwriter.h>

/*
 * ext/xmlwriter on libxml2: __xw_* native thunks + the XMLWriter class
 * prelude.  Architecture notes live in vm_libxml.c (shared registries,
 * error queue, lifetime model).
 *
 * An XMLWriter object holds a phl_xmlwriter resource {xmlTextWriterPtr,
 * xmlBufferPtr} in $__res.  In-memory writers (openMemory) own an
 * xmlBuffer; outputMemory reads it back.  A writer opened on a URI or
 * attached to a stream carries an io_private instead and pushes its bytes
 * through PH7_StreamWrite, so every wrapper this engine has -- php://,
 * data://, a userland streamWrapper -- is a destination, as in php.
 * Every writer is chained on the per-VM registry (pVm->pXmlWriters) and
 * freed at VM reset/release since PH7 resources have no destructor hook.
 */

typedef struct phl_xmlwriter phl_xmlwriter;
struct phl_xmlwriter {
	xmlTextWriterPtr pWriter;
	xmlBufferPtr pBuf;   /* non-NULL for openMemory writers */
	io_private *pDev;    /* non-NULL for openUri/toUri/toStream writers */
	int bOwnDev;         /* this writer opened pDev, so its close closes it */
	int bIoFailed;       /* the last verb's write to pDev failed (see XwRun) */
	ph7_class_instance *pOwner; /* the object whose slot holds it: the one whose
	                             * death flushes it (see XmlWriterInstanceRelease) */
	phl_xmlwriter *pNext;
};

/*
 * libxml reports a write that failed through the error handler, and the paths
 * that run while an object or the whole VM is going away have no caller to
 * warn: php frees its writers with its request's error handling already gone,
 * so nothing is printed there either. Drop whatever those paths raise.
 */
#if LIBXML_VERSION >= 21200
static void XmlWriterSilentError(void *pUser,const xmlError *pErr)
#else
static void XmlWriterSilentError(void *pUser,xmlErrorPtr pErr)
#endif
{
	SXUNUSED(pUser);
	SXUNUSED(pErr);
}
/*
 * Free one writer (called from the registry sweep in vm_libxml.c via
 * PH7_XmlWriterVmRelease).  The order matters: the text writer must be
 * freed before its backing buffer.
 */
static void XmlWriterFree(phl_xmlwriter *pXw)
{
	xmlSetStructuredErrorFunc(0,XmlWriterSilentError);
	if( pXw->pWriter ){
		xmlFreeTextWriter(pXw->pWriter);
		pXw->pWriter = 0;
	}
	if( pXw->pBuf ){
		xmlBufferFree(pXw->pBuf);
		pXw->pBuf = 0;
	}
	xmlSetStructuredErrorFunc(0,0);
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

/*
 * A writer's bytes on their way to a stream. libxml calls this from its own
 * output buffer, so everything the engine's stream layer does for fwrite() --
 * the filter chain, the buffered position -- happens here too.
 */
static int XmlWriterIoWrite(void *pUser,const char *zBuf,int nLen)
{
	phl_xmlwriter *pXw = (phl_xmlwriter *)pUser;
	ph7_int64 nWr;
	if( pXw->pDev == 0 || IO_PRIVATE_INVALID(pXw->pDev) ){
		/* The script fclose()'d the handle it handed toStream() */
		pXw->bIoFailed = 1;
		return -1;
	}
	nWr = PH7_StreamWrite(pXw->pDev,(const void *)zBuf,(ph7_int64)nLen);
	if( nWr < 0 ){
		pXw->bIoFailed = 1;
		return -1;
	}
	return (int)nWr;
}
/*
 * The output buffer's close, reached from xmlFreeTextWriter (so: the registry
 * sweep at VM reset). A handle this writer OPENED is closed here, which is
 * where a file written through openUri() gets its last bytes; a borrowed one --
 * toStream()'s -- is left alone, because the script still holds it.
 */
static int XmlWriterIoClose(void *pUser)
{
	phl_xmlwriter *pXw = (phl_xmlwriter *)pUser;
	if( pXw->bOwnDev && pXw->pDev && !IO_PRIVATE_INVALID(pXw->pDev) ){
		PH7_StreamFilterReleaseChains(pXw->pDev);
		PH7_StreamCloseHandle(pXw->pDev->pStream,pXw->pDev->pHandle);
		MarkIOPrivateClosed(pXw->pDev);
	}
	pXw->pDev = 0;
	return 0;
}
/*
 * Flush a writer and let go of its stream: what php does when the writer is
 * freed, which is the point at which a file opened through openUri() is
 * complete. The libxml writer itself stays alive (the registry sweep frees it),
 * so a later call answers false instead of reaching released memory.
 */
static void XmlWriterFlushAndDetach(phl_xmlwriter *pXw)
{
	xmlSetStructuredErrorFunc(0,XmlWriterSilentError);
	if( pXw->pWriter ){
		xmlTextWriterFlush(pXw->pWriter);
	}
	XmlWriterIoClose((void *)pXw);
	xmlSetStructuredErrorFunc(0,0);
	pXw->pOwner = 0;
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
	int iArgBase;          /* 0 for a method, 1 for the function spelling: what a
	                        * ZPP-style message adds to an argument's position */
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
 * The writer behind an `XMLWriter $writer` argument. The declared type has
 * already refused everything that is not one.
 */
static phl_xmlwriter * XmlWriterOfValue(ph7_value *pArg)
{
	ph7_class_instance *pThis;
	SyString sAttr;
	if( pArg == 0 || (pArg->iFlags & MEMOBJ_OBJ) == 0 ){
		return 0;
	}
	pThis = (ph7_class_instance *)pArg->x.pOther;
	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);
	return XmlWriterArg(PH7_ClassInstanceFetchAttr(pThis,&sAttr));
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
	pCall->iArgBase = 0;
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
/*
 * Allocate a writer shell and chain it on the per-VM registry. The shell is
 * reclaimed with the allocator and the libxml writer behind it by the sweep;
 * a URI writer's own handle is flushed and closed earlier, when the object
 * holding it dies (XmlWriterInstanceRelease).
 */
static phl_xmlwriter * XmlWriterNew(ph7_vm *pVm)
{
	phl_xmlwriter *pXw = (phl_xmlwriter *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_xmlwriter));
	if( pXw == 0 ){
		return 0;
	}
	SyZero(pXw,sizeof(phl_xmlwriter));
	pXw->pNext = (phl_xmlwriter *)pVm->pXmlWriters;
	pVm->pXmlWriters = (void *)pXw;
	return pXw;
}
/*
 * Store a writer in an object's hidden slot, flushing the one it replaces.
 * The replaced SHELL is not freed: the slot is reachable from PHP, so another
 * value may still name it, and the registry sweep frees every writer anyway.
 */
static int XmlWriterAttach(ph7_class_instance *pThis,phl_xmlwriter *pXw)
{
	SyString sAttr;
	ph7_value *pRes;
	if( pThis == 0 ){
		return -1;
	}
	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);
	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);
	if( pRes == 0 ){
		return -1;
	}
	{
		/* A second open on the same object replaces its writer, and php frees
		 * the old one there -- which is what flushes a file opened through
		 * openUri(). Flush and close it here for the same reason; the shell
		 * itself stays on the registry, since the slot is reachable from PHP. */
		phl_xmlwriter *pOld = XmlWriterArg(pRes);
		if( pOld && pOld != pXw && pOld->pOwner == pThis ){
			XmlWriterFlushAndDetach(pOld);
		}
	}
	PH7_MemObjRelease(pRes);
	pRes->x.pOther = pXw;
	MemObjSetType(pRes,MEMOBJ_RES);
	pXw->pOwner = pThis;
	return 0;
}
/*
 * php frees the libxml writer with the OBJECT, and for a writer opened on a URI
 * that is when the file gets its last bytes -- `unset($w)` and then reading the
 * file is how a document written to disk is finished. PH7 resources carry no
 * destructor hook, but a native class does: xRelease runs while the instance's
 * slots are still readable, and before the registry sweep.
 *
 * Only the object the writer was ATTACHED to flushes it. The hidden slot is
 * assignable from PHP, so a second object can name the same writer; the shell
 * is freed by the sweep and never here, and a write after the flush answers
 * false rather than reaching released memory.
 */
static void XmlWriterInstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)
{
	SyString sAttr;
	ph7_value *pRes;
	phl_xmlwriter *pXw;
	SXUNUSED(pVm);
	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);
	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);
	pXw = XmlWriterArg(pRes);
	if( pXw == 0 || pXw->pOwner != pThis ){
		return;
	}
	XmlWriterFlushAndDetach(pXw);
}
/* An in-memory writer: its own xmlBuffer, which outputMemory() reads back. */
static phl_xmlwriter * XmlWriterOpenMemory(ph7_vm *pVm)
{
	phl_xmlwriter *pXw = XmlWriterNew(pVm);
	if( pXw == 0 ){
		return 0;
	}
	pXw->pBuf = xmlBufferCreate();
	if( pXw->pBuf ){
		pXw->pWriter = xmlNewTextWriterMemory(pXw->pBuf,0);
	}
	if( pXw->pWriter == 0 ){
		XmlWriterFree(pXw);
		return 0;
	}
	return pXw;
}
/*
 * A writer over an engine stream handle. `bOwn` says whether closing the writer
 * closes the handle -- true for openUri()/toUri(), false for the handle
 * toStream() borrows from the script.
 */
static phl_xmlwriter * XmlWriterOpenDevice(ph7_vm *pVm,io_private *pDev,int bOwn)
{
	xmlOutputBufferPtr pOut;
	phl_xmlwriter *pXw = XmlWriterNew(pVm);
	if( pXw == 0 ){
		return 0;
	}
	pXw->pDev = pDev;
	pXw->bOwnDev = bOwn;
	pOut = xmlOutputBufferCreateIO(XmlWriterIoWrite,XmlWriterIoClose,(void *)pXw,0);
	if( pOut == 0 ){
		pXw->pDev = 0;
		return 0;
	}
	pXw->pWriter = xmlNewTextWriter(pOut);
	if( pXw->pWriter == 0 ){
		/* xmlNewTextWriter does not take the buffer on failure */
		xmlOutputBufferClose(pOut);
		pXw->pDev = 0;
		return 0;
	}
	return pXw;
}
/*
 * php refuses a NUL inside the two arguments it hands to a C interface that
 * would stop at one: the URI, and startDocument()'s encoding NAME. The message
 * is ZPP's, so the argument number is this SPELLING's own -- unlike the
 * hand-written name check, which always reports the procedural position.
 */
static int XmlWriterCheckNul(ph7_context *pCtx,const char *zFn,int iArg,const char *zParam,
	ph7_value *pVal)
{
	int nLen = 0;
	const char *zStr;
	if( pVal == 0 || ph7_value_is_null(pVal) ){
		return 0;
	}
	zStr = ph7_value_to_string(pVal,&nLen);
	if( SyByteFind(zStr,(sxu32)nLen,'\0',0) != SXRET_OK ){
		return 0;
	}
	return PH7_VmThrowException(pCtx,"ValueError",
		"%s(): Argument #%d ($%s) must not contain any null bytes",zFn,iArg,zParam);
}
/*
 * Open a URI for writing through the engine's stream layer and answer the
 * writer, or 0 with php's own diagnostic already raised. `zFn` names the
 * spelling (method or function) every message is reported under.
 */
static phl_xmlwriter * XmlWriterOpenUri(ph7_context *pCtx,ph7_value *pArg,const char *zFn,
	int bStatic,int *pRc)
{
	ph7_vm *pVm = pCtx->pVm;
	const ph7_io_stream *pStream;
	io_private *pDev;
	phl_xmlwriter *pXw;
	const char *zUri;
	int nUri = 0;
	*pRc = PH7_OK;
	zUri = pArg ? ph7_value_to_string(pArg,&nUri) : "";
	if( nUri < 1 ){
		*pRc = PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #1 ($uri) must not be empty",zFn);
		return 0;
	}
	*pRc = XmlWriterCheckNul(pCtx,zFn,1,"uri",pArg);
	if( *pRc != PH7_OK ){
		return 0;
	}
	pStream = PH7_VmGetStreamDevice(pVm,&zUri,nUri);
	pDev = pStream ? (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE) : 0;
	if( pDev ){
		InitIOPrivate(pVm,pStream,pDev);
		/* php opens the destination "wb" */
		pDev->pHandle = PH7_StreamOpenHandle(pVm,pStream,zUri,
			PH7_IO_OPEN_WRONLY|PH7_IO_OPEN_CREATE|PH7_IO_OPEN_TRUNC,FALSE,pArg,FALSE,0,zFn);
		if( pDev->pHandle ){
			int nOrig = 0;
			const char *zOrig = ph7_value_to_string(pArg,&nOrig);
			SetIOPrivateOpenedAs(pDev,zOrig,nOrig,"wb",2);
		}else{
			/* Nothing reached PHP, so the shell goes back */
			PH7_StreamReleaseUnopened(pCtx,pDev);
			pDev = 0;
		}
	}
	pXw = pDev ? XmlWriterOpenDevice(pVm,pDev,TRUE) : 0;
	if( pXw == 0 ){
		/* php's two refusals for the same failure: the factory raises, the
		 * opener warns and answers false. */
		if( bStatic ){
			*pRc = PH7_VmThrowException(pCtx,"ValueError",
				"%s(): Argument #1 ($uri) must resolve to a valid file path",zFn);
		}else{
			SyString sFn;
			SyStringInitFromBuf(&sFn,zFn,SyStrlen(zFn));
			PH7_VmThrowError(pVm,&sFn,PH7_CTX_WARNING,"Unable to resolve file path");
		}
		return 0;
	}
	return pXw;
}
/* bool XMLWriter::openMemory() */
static int vm_builtin_xw_open_memory(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_xmlwriter *pXw;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	pXw = XmlWriterOpenMemory(pCtx->pVm);
	if( pXw == 0 || XmlWriterAttach(PH7_ContextThis(pCtx),pXw) != 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* bool XMLWriter::openUri(string $uri) */
static int vm_builtin_xw_open_uri(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_xmlwriter *pXw;
	int rc = PH7_OK;
	pXw = XmlWriterOpenUri(pCtx,nArg > 0 ? apArg[0] : 0,"XMLWriter::openUri",FALSE,&rc);
	if( pXw == 0 ){
		if( rc != PH7_OK ){
			return rc;
		}
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( XmlWriterAttach(PH7_ContextThis(pCtx),pXw) != 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * The php 8.5 factories. Each answers a NEW writer rather than configuring the
 * receiver, and each builds the LATE STATIC class, so a subclass of XMLWriter
 * gets one of its own.
 */
static int XmlWriterFactory(ph7_context *pCtx,phl_xmlwriter *pXw)
{
	ph7_class *pClass = PH7_ContextCalledClass(pCtx);
	ph7_class_instance *pObj;
	if( pClass == 0 ){
		pClass = PH7_VmExtractClass(pCtx->pVm,"XMLWriter",sizeof("XMLWriter")-1,FALSE,0);
	}
	pObj = pClass ? PH7_NewClassInstance(pCtx->pVm,pClass) : 0;
	if( pObj == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( XmlWriterAttach(pObj,pXw) != 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	PH7_NativeResultObject(pCtx,pObj);
	return PH7_OK;
}
/* static XMLWriter::toMemory(): static */
static int vm_builtin_xw_to_memory(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_xmlwriter *pXw;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	pXw = XmlWriterOpenMemory(pCtx->pVm);
	if( pXw == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	return XmlWriterFactory(pCtx,pXw);
}
/* static XMLWriter::toUri(string $uri): static */
static int vm_builtin_xw_to_uri(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int rc = PH7_OK;
	phl_xmlwriter *pXw = XmlWriterOpenUri(pCtx,nArg > 0 ? apArg[0] : 0,"XMLWriter::toUri",TRUE,&rc);
	if( pXw == 0 ){
		return rc;
	}
	return XmlWriterFactory(pCtx,pXw);
}
/*
 * static XMLWriter::toStream(mixed $stream): static
 *
 * The handle stays the script's: the writer only pushes bytes at it, and
 * closing the writer does not close it. A handle the script fcloses afterwards
 * makes every later write fail, which is the answer php's own stream reference
 * gives once the resource is gone.
 */
static int vm_builtin_xw_to_stream(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	io_private *pDev;
	phl_xmlwriter *pXw;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"XMLWriter::toStream(): Argument #1 ($stream) must be of type resource, %s given",
			nArg > 0 ? ph7_type_name(apArg[0]) : "null");
	}
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	if( IO_PRIVATE_INVALID(pDev) ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"XMLWriter::toStream(): supplied resource is not a valid stream resource");
	}
	pXw = XmlWriterOpenDevice(pCtx->pVm,pDev,FALSE);
	if( pXw == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	return XmlWriterFactory(pCtx,pXw);
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
	int rc;
	pCall->pXw->bIoFailed = 0;
	rc = xVerb(pCtx,pCall);
	if( pCall->pXw->bIoFailed ){
		/* A write that could not land is reported by the RETURN value alone:
		 * php says nothing when the stream behind a writer has gone away, and
		 * libxml's own "I/O error" would be a warning php never raises. */
		pCall->pXw->bIoFailed = 0;
		PH7_LibxmlDropErrors(pCtx->pVm,nMark);
		return rc;
	}
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

/*
 * Build the call descriptor for the FUNCTION spelling: the writer is argument
 * #1 and everything else shifts one slot, which is where php's diagnostics get
 * their numbering from.
 */
static int XwCallFromArg(ph7_context *pCtx,xw_call *pCall,const char *zFn,
	const char *zNameArg,int nArg,ph7_value **apArg)
{
	phl_xmlwriter *pXw = nArg > 0 ? XmlWriterOfValue(apArg[0]) : 0;
	if( pXw == 0 || pXw->pWriter == 0 ){
		return XmlWriterMissing(pCtx);
	}
	pCall->pXw = pXw;
	pCall->zFn = zFn;
	pCall->zNameArg = zNameArg;
	pCall->iArgBase = 1;
	pCall->nArg = nArg - 1;
	pCall->apArg = apArg + 1;
	return 0;
}
/* One procedural entry point: the same verb, one argument along. */
#define XW_FUNCTION(cfn,verb,fname,namearg)                                       \
static int cfn(ph7_context *pCtx,int nArg,ph7_value **apArg)                      \
{                                                                                 \
	xw_call sCall;                                                            \
	int rc;                                                                   \
	SyZero(&sCall,sizeof(sCall));                                             \
	rc = XwCallFromArg(pCtx,&sCall,fname,namearg,nArg,apArg);                 \
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

/*
 * bool XMLWriter::startDocument(?string $version, ?string $encoding, ?string $standalone)
 *
 * The ENCODING is the one argument here php refuses a NUL in: it names a
 * character set for libxml to look up, and that lookup stops at the first NUL.
 * The version and the standalone flag are written out verbatim and take one.
 */
static int XwStartDocument(ph7_context *pCtx,xw_call *pCall)
{
	int rc = XmlWriterCheckNul(pCtx,pCall->zFn,pCall->iArgBase + 2,"encoding",
		pCall->nArg > 1 ? pCall->apArg[1] : 0);
	if( rc != PH7_OK ){
		return rc;
	}
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

/*
 * The procedural surface. php's ext/xmlwriter presents every verb twice, and
 * the function spelling is the ORIGINAL one -- the class arrived in 5.1.2 --
 * so a program written against it is not using an alias for the method but the
 * name the extension was documented under. Each entry drives the same verb one
 * argument along, and states its OWN name-argument text: procedural numbering
 * is what php's macro reports, so here it names the real parameter.
 */
XW_FUNCTION(vm_builtin_xmlwriter_set_indent,XwSetIndent,"xmlwriter_set_indent",0)
XW_FUNCTION(vm_builtin_xmlwriter_set_indent_string,XwSetIndentString,"xmlwriter_set_indent_string",0)
XW_FUNCTION(vm_builtin_xmlwriter_start_comment,XwStartComment,"xmlwriter_start_comment",0)
XW_FUNCTION(vm_builtin_xmlwriter_end_comment,XwEndComment,"xmlwriter_end_comment",0)
XW_FUNCTION(vm_builtin_xmlwriter_start_attribute,XwStartAttribute,"xmlwriter_start_attribute","#2 ($name)")
XW_FUNCTION(vm_builtin_xmlwriter_end_attribute,XwEndAttribute,"xmlwriter_end_attribute",0)
XW_FUNCTION(vm_builtin_xmlwriter_write_attribute,XwWriteAttribute,"xmlwriter_write_attribute","#2 ($name)")
XW_FUNCTION(vm_builtin_xmlwriter_start_attribute_ns,XwStartAttributeNs,"xmlwriter_start_attribute_ns","#3 ($name)")
XW_FUNCTION(vm_builtin_xmlwriter_write_attribute_ns,XwWriteAttributeNs,"xmlwriter_write_attribute_ns","#3 ($name)")
XW_FUNCTION(vm_builtin_xmlwriter_start_element,XwStartElement,"xmlwriter_start_element","#2 ($name)")
XW_FUNCTION(vm_builtin_xmlwriter_end_element,XwEndElement,"xmlwriter_end_element",0)
XW_FUNCTION(vm_builtin_xmlwriter_full_end_element,XwFullEndElement,"xmlwriter_full_end_element",0)
XW_FUNCTION(vm_builtin_xmlwriter_start_element_ns,XwStartElementNs,"xmlwriter_start_element_ns","#3 ($name)")
XW_FUNCTION(vm_builtin_xmlwriter_write_element,XwWriteElement,"xmlwriter_write_element","#2 ($name)")
XW_FUNCTION(vm_builtin_xmlwriter_write_element_ns,XwWriteElementNs,"xmlwriter_write_element_ns","#3 ($name)")
XW_FUNCTION(vm_builtin_xmlwriter_start_pi,XwStartPi,"xmlwriter_start_pi","#2 ($target)")
XW_FUNCTION(vm_builtin_xmlwriter_end_pi,XwEndPi,"xmlwriter_end_pi",0)
XW_FUNCTION(vm_builtin_xmlwriter_write_pi,XwWritePi,"xmlwriter_write_pi","#2 ($target)")
XW_FUNCTION(vm_builtin_xmlwriter_start_cdata,XwStartCdata,"xmlwriter_start_cdata",0)
XW_FUNCTION(vm_builtin_xmlwriter_end_cdata,XwEndCdata,"xmlwriter_end_cdata",0)
XW_FUNCTION(vm_builtin_xmlwriter_write_cdata,XwWriteCdata,"xmlwriter_write_cdata",0)
XW_FUNCTION(vm_builtin_xmlwriter_text,XwText,"xmlwriter_text",0)
XW_FUNCTION(vm_builtin_xmlwriter_write_raw,XwWriteRaw,"xmlwriter_write_raw",0)
XW_FUNCTION(vm_builtin_xmlwriter_start_document,XwStartDocument,"xmlwriter_start_document",0)
XW_FUNCTION(vm_builtin_xmlwriter_end_document,XwEndDocument,"xmlwriter_end_document",0)
XW_FUNCTION(vm_builtin_xmlwriter_write_comment,XwWriteComment,"xmlwriter_write_comment",0)
XW_FUNCTION(vm_builtin_xmlwriter_start_dtd,XwStartDtd,"xmlwriter_start_dtd",0)
XW_FUNCTION(vm_builtin_xmlwriter_end_dtd,XwEndDtd,"xmlwriter_end_dtd",0)
XW_FUNCTION(vm_builtin_xmlwriter_write_dtd,XwWriteDtd,"xmlwriter_write_dtd",0)
XW_FUNCTION(vm_builtin_xmlwriter_start_dtd_element,XwStartDtdElement,"xmlwriter_start_dtd_element","#2 ($qualifiedName)")
XW_FUNCTION(vm_builtin_xmlwriter_end_dtd_element,XwEndDtdElement,"xmlwriter_end_dtd_element",0)
XW_FUNCTION(vm_builtin_xmlwriter_write_dtd_element,XwWriteDtdElement,"xmlwriter_write_dtd_element","#2 ($name)")
XW_FUNCTION(vm_builtin_xmlwriter_start_dtd_attlist,XwStartDtdAttlist,"xmlwriter_start_dtd_attlist","#2 ($name)")
XW_FUNCTION(vm_builtin_xmlwriter_end_dtd_attlist,XwEndDtdAttlist,"xmlwriter_end_dtd_attlist",0)
XW_FUNCTION(vm_builtin_xmlwriter_write_dtd_attlist,XwWriteDtdAttlist,"xmlwriter_write_dtd_attlist","#2 ($name)")
XW_FUNCTION(vm_builtin_xmlwriter_start_dtd_entity,XwStartDtdEntity,"xmlwriter_start_dtd_entity","#2 ($name)")
XW_FUNCTION(vm_builtin_xmlwriter_end_dtd_entity,XwEndDtdEntity,"xmlwriter_end_dtd_entity",0)
XW_FUNCTION(vm_builtin_xmlwriter_write_dtd_entity,XwWriteDtdEntity,"xmlwriter_write_dtd_entity","#2 ($name)")
XW_FUNCTION(vm_builtin_xmlwriter_output_memory,XwOutputMemory,"xmlwriter_output_memory",0)
XW_FUNCTION(vm_builtin_xmlwriter_flush,XwFlush,"xmlwriter_flush",0)

/* XMLWriter|false xmlwriter_open_memory() */
static int vm_builtin_xmlwriter_open_memory(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_xmlwriter *pXw;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	pXw = XmlWriterOpenMemory(pCtx->pVm);
	if( pXw == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	return XmlWriterFactory(pCtx,pXw);
}
/* XMLWriter|false xmlwriter_open_uri(string $uri) */
static int vm_builtin_xmlwriter_open_uri(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int rc = PH7_OK;
	phl_xmlwriter *pXw = XmlWriterOpenUri(pCtx,nArg > 0 ? apArg[0] : 0,"xmlwriter_open_uri",FALSE,&rc);
	if( pXw == 0 ){
		if( rc != PH7_OK ){
			return rc;
		}
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	return XmlWriterFactory(pCtx,pXw);
}

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
		{ "openUri",         PH7_MOD_PUBLIC, "string $uri", "@bool", vm_builtin_xw_open_uri },
		{ "toUri",           PH7_MOD_PUBLIC|PH7_MOD_STATIC, "string $uri", "static",
		  vm_builtin_xw_to_uri },
		{ "openMemory",      PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_open_memory },
		{ "toMemory",        PH7_MOD_PUBLIC|PH7_MOD_STATIC, "", "static", vm_builtin_xw_to_memory },
		{ "toStream",        PH7_MOD_PUBLIC|PH7_MOD_STATIC, "mixed $stream", "static",
		  vm_builtin_xw_to_stream },
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
	/* php's ext/xmlwriter presents every verb under a function name too; each
	 * one drives the very same body through XwCallFromArg. */
	static const struct {
		const char *zName;
		ProchHostFunction xFunc;
	} aFunc[] = {
		{ "xmlwriter_open_uri",    vm_builtin_xmlwriter_open_uri },
		{ "xmlwriter_open_memory", vm_builtin_xmlwriter_open_memory },
		{ "xmlwriter_set_indent", vm_builtin_xmlwriter_set_indent },
		{ "xmlwriter_set_indent_string", vm_builtin_xmlwriter_set_indent_string },
		{ "xmlwriter_start_comment", vm_builtin_xmlwriter_start_comment },
		{ "xmlwriter_end_comment", vm_builtin_xmlwriter_end_comment },
		{ "xmlwriter_start_attribute", vm_builtin_xmlwriter_start_attribute },
		{ "xmlwriter_end_attribute", vm_builtin_xmlwriter_end_attribute },
		{ "xmlwriter_write_attribute", vm_builtin_xmlwriter_write_attribute },
		{ "xmlwriter_start_attribute_ns", vm_builtin_xmlwriter_start_attribute_ns },
		{ "xmlwriter_write_attribute_ns", vm_builtin_xmlwriter_write_attribute_ns },
		{ "xmlwriter_start_element", vm_builtin_xmlwriter_start_element },
		{ "xmlwriter_end_element", vm_builtin_xmlwriter_end_element },
		{ "xmlwriter_full_end_element", vm_builtin_xmlwriter_full_end_element },
		{ "xmlwriter_start_element_ns", vm_builtin_xmlwriter_start_element_ns },
		{ "xmlwriter_write_element", vm_builtin_xmlwriter_write_element },
		{ "xmlwriter_write_element_ns", vm_builtin_xmlwriter_write_element_ns },
		{ "xmlwriter_start_pi", vm_builtin_xmlwriter_start_pi },
		{ "xmlwriter_end_pi", vm_builtin_xmlwriter_end_pi },
		{ "xmlwriter_write_pi", vm_builtin_xmlwriter_write_pi },
		{ "xmlwriter_start_cdata", vm_builtin_xmlwriter_start_cdata },
		{ "xmlwriter_end_cdata", vm_builtin_xmlwriter_end_cdata },
		{ "xmlwriter_write_cdata", vm_builtin_xmlwriter_write_cdata },
		{ "xmlwriter_text", vm_builtin_xmlwriter_text },
		{ "xmlwriter_write_raw", vm_builtin_xmlwriter_write_raw },
		{ "xmlwriter_start_document", vm_builtin_xmlwriter_start_document },
		{ "xmlwriter_end_document", vm_builtin_xmlwriter_end_document },
		{ "xmlwriter_write_comment", vm_builtin_xmlwriter_write_comment },
		{ "xmlwriter_start_dtd", vm_builtin_xmlwriter_start_dtd },
		{ "xmlwriter_end_dtd", vm_builtin_xmlwriter_end_dtd },
		{ "xmlwriter_write_dtd", vm_builtin_xmlwriter_write_dtd },
		{ "xmlwriter_start_dtd_element", vm_builtin_xmlwriter_start_dtd_element },
		{ "xmlwriter_end_dtd_element", vm_builtin_xmlwriter_end_dtd_element },
		{ "xmlwriter_write_dtd_element", vm_builtin_xmlwriter_write_dtd_element },
		{ "xmlwriter_start_dtd_attlist", vm_builtin_xmlwriter_start_dtd_attlist },
		{ "xmlwriter_end_dtd_attlist", vm_builtin_xmlwriter_end_dtd_attlist },
		{ "xmlwriter_write_dtd_attlist", vm_builtin_xmlwriter_write_dtd_attlist },
		{ "xmlwriter_start_dtd_entity", vm_builtin_xmlwriter_start_dtd_entity },
		{ "xmlwriter_end_dtd_entity", vm_builtin_xmlwriter_end_dtd_entity },
		{ "xmlwriter_write_dtd_entity", vm_builtin_xmlwriter_write_dtd_entity },
		{ "xmlwriter_output_memory", vm_builtin_xmlwriter_output_memory },
		{ "xmlwriter_flush", vm_builtin_xmlwriter_flush },
	};
	sxu32 n;
	static const PH7_NativeClassSpec sSpec = {
		"XMLWriter", 0, 0, PH7_CLASS_NOCLONE,
		aMethod, SX_ARRAYSIZE(aMethod),
		0, 0,
		aProp, SX_ARRAYSIZE(aProp),
		XmlWriterInstanceRelease, 0, 0
	};
	for( n = 0 ; n < SX_ARRAYSIZE(aFunc) ; n++ ){
		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);
	}
	return PH7_InstallNativeClasses(&(*pVm),&sSpec,1);
}

#else
/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */
typedef int vm_xmlwriter_unused;
#endif /* PH7_ENABLE_LIBXML */
