/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifdef PH7_ENABLE_LIBXML
#include "ph7int.h"
#include <libxml/parser.h>
#include <libxml/xmlerror.h>
#include <libxml/tree.h>

/*
 * Shared libxml2 plumbing for the DOM / XMLWriter / libxml_* surfaces.
 *
 * Memory model: libxml2 stays on its own (system) allocator on purpose.
 * Routing it through SyMemBackend would subject libxml2 internals to the
 * PHL_MAX_ALLOC fault-injection used by the stress tier, and libxml2 does
 * not tolerate mid-parse OOM injection the way the engine's own code does.
 * The cost is that allocation-stress tests do not exercise libxml2 OOM
 * paths.
 *
 * Lifetime model: PH7 resources (MEMOBJ_RES) carry no destructor hook, so
 * every xmlDoc created on behalf of PHP code is owned by a phl_xmldoc entry
 * chained on the per-VM registry (pVm->pXmlDocs) and freed only when the VM
 * is reset (a new request on a reused VM) or released -- never while PHP
 * code could still hold a node wrapper into it.  Nodes unlinked from their
 * tree (removeChild/replaceChild) are parked on the owning phl_xmldoc's
 * aOrphans set and freed with the doc.  Docs therefore accumulate until VM
 * teardown; acceptable for CLI/per-request VMs, revisit with __destruct-
 * driven refcounting if long-lived embeddings ever need early release.
 */

/*
 * One-time process-global libxml2 initialization.  Safe as a plain static
 * flag under PHL's current single-threaded-execution model (see the
 * equivalent note in vm_pcre.c); xmlCleanupParser() is deliberately never
 * called -- it is unsafe with threads and process exit reclaims everything.
 */
static void LibxmlGlobalInit(void)
{
	static int bInit = 0;
	if( !bInit ){
		xmlInitParser();
		bInit = 1;
	}
}
/*
 * Free one registered document: its orphaned subtrees first, then the tree
 * itself.  Registry links and the phl_xmldoc shell live in SyMemBackend and
 * are reclaimed with the VM allocator.
 */
static void LibxmlFreeDoc(phl_xmldoc *pDoc)
{
	xmlNodePtr *apOrphan = (xmlNodePtr *)SySetBasePtr(&pDoc->aOrphans);
	sxu32 n;
	for( n = 0 ; n < SySetUsed(&pDoc->aOrphans) ; ++n ){
		xmlUnlinkNode(apOrphan[n]);
		xmlFreeNode(apOrphan[n]);
	}
	SySetRelease(&pDoc->aOrphans);
	if( pDoc->pDoc ){
		xmlFreeDoc((xmlDocPtr)pDoc->pDoc);
		pDoc->pDoc = 0;
	}
}
/*
 * Reset the per-VM libxml state between executions: drop the accumulated
 * error queue and free every document from the previous request.  Called
 * from PH7_VmMakeReady() (which runs once per exec, including on VM reuse
 * by the -S server) alongside the other per-exec field resets.
 */
PH7_PRIVATE void PH7_LibxmlVmReset(ph7_vm *pVm)
{
	phl_xmldoc *pDoc,*pNext;
	PH7_LibxmlClearErrors(pVm);
	/* A held message fragment is per-request state: a reused VM must not print
	 * the previous request's tail joined to this one's first diagnostic. */
	SyBlobReset(&pVm->sLibxmlPend);
	pVm->bLibxmlInternalErr = 0;
	pDoc = (phl_xmldoc *)pVm->pXmlDocs;
	while( pDoc ){
		pNext = pDoc->pNext;
		LibxmlFreeDoc(pDoc);
		SyMemBackendFree(&pVm->sAllocator,pDoc);
		pDoc = pNext;
	}
	pVm->pXmlDocs = 0;
	/* The ownerless shell rode the chain just freed. */
	pVm->pXmlLimbo = 0;
	/* XMLWriter buffers live outside SyMemBackend too (see vm_xmlwriter.c) */
	PH7_XmlWriterVmSweep(pVm);
	/* ext/xml push parsers: their ctxt/myDoc are libxml allocations and the
	 * handler VALUES hold references that must drop before the allocator goes. */
	PH7_XmlParserVmSweep(pVm);
	/* The entity-loader/streams-context slots hold per-request VALUES (a
	 * closure, a context resource): drop them so a reused VM starts default. */
	PH7_MemObjRelease(&pVm->sXmlEntLoader);
	PH7_MemObjRelease(&pVm->sXmlStreamsCtx);
}
/*
 * Final teardown on VM release.  Must run before SyMemBackendRelease()
 * wipes the allocator that holds the registry shells.
 */
PH7_PRIVATE void PH7_LibxmlVmRelease(ph7_vm *pVm)
{
	PH7_LibxmlVmReset(pVm);
	SySetRelease(&pVm->aLibxmlErr);
	SyBlobRelease(&pVm->sLibxmlPend);
}
/*
 * Release the copied message/file strings of one queue entry.
 */
static void LibxmlFreeErr(ph7_vm *pVm,phl_libxml_err *pErr)
{
	if( pErr->sMsg.zString ){
		SyMemBackendFree(&pVm->sAllocator,(void *)pErr->sMsg.zString);
	}
	if( pErr->sFile.zString ){
		SyMemBackendFree(&pVm->sAllocator,(void *)pErr->sFile.zString);
	}
	SyStringInitFromBuf(&pErr->sMsg,0,0);
	SyStringInitFromBuf(&pErr->sFile,0,0);
}
/*
 * Empty the libxml error queue, releasing the copied strings.  The
 * last-error slot is kept (php parity: use_internal_errors(false) drops
 * the buffer but libxml_get_last_error still reports).
 */
static void LibxmlClearQueue(ph7_vm *pVm)
{
	phl_libxml_err *aErr = (phl_libxml_err *)SySetBasePtr(&pVm->aLibxmlErr);
	sxu32 n;
	for( n = 0 ; n < SySetUsed(&pVm->aLibxmlErr) ; ++n ){
		LibxmlFreeErr(pVm,&aErr[n]);
	}
	SySetReset(&pVm->aLibxmlErr);
}
/*
 * libxml_clear_errors(): drop the queue AND the last-error slot.
 */
PH7_PRIVATE void PH7_LibxmlClearErrors(ph7_vm *pVm)
{
	LibxmlClearQueue(pVm);
	if( pVm->pLibxmlLastErr ){
		LibxmlFreeErr(pVm,(phl_libxml_err *)pVm->pLibxmlLastErr);
		SyMemBackendFree(&pVm->sAllocator,pVm->pLibxmlLastErr);
		pVm->pLibxmlLastErr = 0;
	}
}
/*
 * Push one error onto the per-VM queue and last-error slot, copying the
 * message/file strings.  The typed structured-error callback below and the
 * DOM schema hooks (vm_dom.c) both funnel through this, keeping the
 * queue-building logic in one place and ph7int.h free of libxml types.
 */
PH7_PRIVATE void PH7_LibxmlQueueError(ph7_vm *pVm,int iLevel,int iCode,int iLine,int iColumn,
	const char *zMsg,const char *zFile)
{
	phl_libxml_err sEntry;
	phl_libxml_err *pLast;
	if( pVm == 0 ){
		return;
	}
	SyZero(&sEntry,sizeof(sEntry));
	sEntry.iLevel = iLevel;
	sEntry.iCode = iCode;
	sEntry.iLine = iLine;
	sEntry.iColumn = iColumn;
	if( zMsg ){
		sxu32 nMsg = SyStrlen(zMsg);
		char *zDup = SyMemBackendStrDup(&pVm->sAllocator,zMsg,nMsg);
		if( zDup ){
			/* Trailing newline kept: php's LibXMLError->message preserves it */
			SyStringInitFromBuf(&sEntry.sMsg,zDup,nMsg);
		}
	}
	if( zFile ){
		sxu32 nFile = SyStrlen(zFile);
		char *zDup = SyMemBackendStrDup(&pVm->sAllocator,zFile,nFile);
		if( zDup ){
			SyStringInitFromBuf(&sEntry.sFile,zDup,nFile);
		}
	}
	SySetPut(&pVm->aLibxmlErr,(const void *)&sEntry);
	/* Mirror into the last-error slot (independent string copies so queue
	 * draining cannot invalidate it). */
	pLast = (phl_libxml_err *)pVm->pLibxmlLastErr;
	if( pLast == 0 ){
		pLast = (phl_libxml_err *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_libxml_err));
		if( pLast == 0 ){
			return;
		}
		SyZero(pLast,sizeof(phl_libxml_err));
		pVm->pLibxmlLastErr = (void *)pLast;
	}else{
		LibxmlFreeErr(pVm,pLast);
	}
	pLast->iLevel = iLevel;
	pLast->iCode = iCode;
	pLast->iLine = iLine;
	pLast->iColumn = iColumn;
	if( sEntry.sMsg.zString ){
		char *zDup = SyMemBackendStrDup(&pVm->sAllocator,sEntry.sMsg.zString,sEntry.sMsg.nByte);
		if( zDup ){
			SyStringInitFromBuf(&pLast->sMsg,zDup,sEntry.sMsg.nByte);
		}
	}
	if( sEntry.sFile.zString ){
		char *zDup = SyMemBackendStrDup(&pVm->sAllocator,sEntry.sFile.zString,sEntry.sFile.nByte);
		if( zDup ){
			SyStringInitFromBuf(&pLast->sFile,zDup,sEntry.sFile.nByte);
		}
	}
}
/*
 * Structured-error callback installed while a libxml2 entry point runs
 * between PH7_LibxmlCaptureBegin/End.  Forwards to PH7_LibxmlQueueError;
 * the capture-end decides whether entries stay queued or drain as
 * php-style warnings.
 */
#if LIBXML_VERSION >= 21200
static void LibxmlStructuredErr(void *pUserData,const xmlError *pErr)
#else
static void LibxmlStructuredErr(void *pUserData,xmlErrorPtr pErr)
#endif
{
	if( pErr == 0 ){
		return;
	}
	/* libxml keeps the column in int2 */
	PH7_LibxmlQueueError((ph7_vm *)pUserData,(int)pErr->level,pErr->code,pErr->line,
		pErr->int2,pErr->message,pErr->file);
}
/*
 * ...and libxml's OTHER error channel. A handful of diagnostics never reach
 * the structured handler at all: they are printed with xmlGenericError()
 * directly, whose default writes them to stderr. XPath is where a program
 * meets them -- `zz:nope()` under an unbound prefix says "xmlXPathCompOpEval:
 * function nope bound to undefined prefix zz" through this channel -- and
 * before this handler they went to the terminal, invisible to
 * libxml_get_errors() and to a program's error handler alike, in the middle
 * of whatever the script was writing. php captures them, at libxml's ERROR
 * level under code 1 with no file or line, and this says the same.
 *
 * The signature is printf-style, so the message is formatted here.
 */
static void LibxmlGenericErr(void *pUserData,const char *zFmt,...)
{
	ph7_vm *pVm = (ph7_vm *)pUserData;
	SyBlob sMsg;
	va_list ap;
	if( pVm == 0 || zFmt == 0 ){
		return;
	}
	SyBlobInit(&sMsg,&pVm->sAllocator);
	va_start(ap,zFmt);
	SyBlobFormatAp(&sMsg,zFmt,ap);
	va_end(ap);
	/* php's own generic handler is line-buffered and reports what it flushed
	 * WITHOUT the newline, where a structured message keeps its own -- this
	 * channel's messages arrive whole and newline-terminated. */
	{
		char *zMsg = (char *)SyBlobData(&sMsg);
		sxu32 nMsg = SyBlobLength(&sMsg);
		while( nMsg > 0 && (zMsg[nMsg-1] == '\n' || zMsg[nMsg-1] == '\r') ){
			nMsg--;
		}
		sMsg.nByte = nMsg;
	}
	SyBlobAppend(&sMsg,"",1);   /* the queue copies a C string */
	{
		/* The channel mark goes on the entry the queue just took -- confirmed
		 * by the depth having GROWN, so an insertion that failed cannot leave
		 * the mark on the message before it. */
		sxu32 nBefore = SySetUsed(&pVm->aLibxmlErr);
		PH7_LibxmlQueueError(pVm,XML_ERR_ERROR,1,0,0,(const char *)SyBlobData(&sMsg),"");
		if( SySetUsed(&pVm->aLibxmlErr) > nBefore ){
			phl_libxml_err *aErr = (phl_libxml_err *)SySetBasePtr(&pVm->aLibxmlErr);
			aErr[SySetUsed(&pVm->aLibxmlErr)-1].bWholeLine = 1;
			if( pVm->pLibxmlLastErr ){
				((phl_libxml_err *)pVm->pLibxmlLastErr)->bWholeLine = 1;
			}
		}
	}
	SyBlobRelease(&sMsg);
}
/* xmlSetGenericErrorFunc is deprecated from libxml 2.12 and the MSVC gate
 * refuses a deprecated symbol under /WX. It is still the only door onto that
 * channel, so the deprecation is suppressed at this one call pair. */
static void LibxmlGenericSet(ph7_vm *pVm,int bOn)
{
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4996)
#endif
	xmlSetGenericErrorFunc(bOn ? (void *)pVm : 0,bOn ? LibxmlGenericErr : 0);
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
}
/*
 * Bracket a libxml2 entry point.  Begin installs the structured handler
 * routed at this VM and returns the current queue depth; End restores the
 * default handler and, when libxml_use_internal_errors() is OFF, drains
 * every entry recorded since the mark as php-style warnings:
 *   funcname(): <message> in <Entity|file>, line: <n>
 * (php's exact wording for the memory-parser case).
 */
PH7_PRIVATE sxu32 PH7_LibxmlCaptureBegin(ph7_vm *pVm)
{
	xmlSetStructuredErrorFunc(pVm,LibxmlStructuredErr);
	LibxmlGenericSet(pVm,1);
	return SySetUsed(&pVm->aLibxmlErr);
}
/*
 * End a capture window WITHOUT reporting what it caught: for an entry point
 * whose failure php reports through the return value alone (a stream write
 * that could not land under XMLWriter), where libxml's own message would be a
 * warning php never raises. Entries stay queued when internal capture is on --
 * `libxml_get_errors()` is the one place php does show them.
 */
PH7_PRIVATE void PH7_LibxmlDropErrors(ph7_vm *pVm,sxu32 nMark)
{
	xmlSetStructuredErrorFunc(0,0);
	LibxmlGenericSet(pVm,0);
	if( pVm->bLibxmlInternalErr ){
		return;
	}
	if( SySetUsed(&pVm->aLibxmlErr) > nMark ){
		phl_libxml_err *aErr = (phl_libxml_err *)SySetBasePtr(&pVm->aLibxmlErr);
		sxu32 n;
		for( n = nMark ; n < SySetUsed(&pVm->aLibxmlErr) ; ++n ){
			LibxmlFreeErr(pVm,&aErr[n]);
		}
		SySetTruncate(&pVm->aLibxmlErr,nMark);
	}
}
PH7_PRIVATE void PH7_LibxmlCaptureEnd(ph7_vm *pVm,sxu32 nMark,const char *zFnName)
{
	PH7_LibxmlCaptureEndOpts(pVm,nMark,zFnName,0);
}
/*
 * The same drain for a parse that was given OPTIONS, two of which are about
 * these very diagnostics: `LIBXML_NOERROR` silences the errors and the fatals,
 * `LIBXML_NOWARNING` the warnings. php silences them at the PRINT, not at the
 * source -- the queue `libxml_get_errors()` answers still holds them, and so
 * does the slot `libxml_get_last_error()` reads -- so this skips them here,
 * after they have been recorded, and the line buffer never sees them either
 * (php's does not: libxml's message never reaches the printer at all).
 */
PH7_PRIVATE void PH7_LibxmlCaptureEndOpts(ph7_vm *pVm,sxu32 nMark,const char *zFnName,int iOpts)
{
	xmlSetStructuredErrorFunc(0,0);
	LibxmlGenericSet(pVm,0);
	if( pVm->bLibxmlInternalErr ){
		/* Internal capture on: entries stay queued for libxml_get_errors() */
		return;
	}
	if( SySetUsed(&pVm->aLibxmlErr) > nMark ){
		phl_libxml_err *aErr = (phl_libxml_err *)SySetBasePtr(&pVm->aLibxmlErr);
		sxu32 n;
		for( n = nMark ; n < SySetUsed(&pVm->aLibxmlErr) ; ++n ){
			SyString sFunc;
			SyBlob sMsg;
			const char *zPend;
			sxu32 nPend,nTrim;
			if( (aErr[n].iLevel == XML_ERR_WARNING)
			 ? (iOpts & XML_PARSE_NOWARNING) != 0
			 : (iOpts & XML_PARSE_NOERROR) != 0 ){
				LibxmlFreeErr(pVm,&aErr[n]);
				continue;
			}
			/* php's libxml diagnostics are LINE-buffered: each message is
			 * appended to one buffer and the diagnostic is only raised when the
			 * accumulated text ends in a newline. Most of libxml's messages do,
			 * so most of them stand alone -- but the ones that do not (libxml's
			 * "Validation failed: no DTD found !" is one) are held back and
			 * printed JOINED to whatever comes next, even from a later parse of
			 * a different document, and are never printed at all if nothing
			 * else follows. Reproduced rather than tidied up: a program's
			 * output is what it is. */
			SyBlobAppend(&pVm->sLibxmlPend,aErr[n].sMsg.zString,aErr[n].sMsg.nByte);
			zPend = (const char *)SyBlobData(&pVm->sLibxmlPend);
			nPend = SyBlobLength(&pVm->sLibxmlPend);
			if( !aErr[n].bWholeLine && (nPend < 1 || zPend[nPend-1] != '\n') ){
				/* no line yet: hold it for the next message */
				LibxmlFreeErr(pVm,&aErr[n]);
				continue;
			}
			nTrim = nPend;
			/* php trims the trailing newline off the warning copy */
			while( nTrim > 0 && (zPend[nTrim-1] == '\n' || zPend[nTrim-1] == '\r') ){
				nTrim--;
			}
			SyBlobInit(&sMsg,&pVm->sAllocator);
			SyBlobAppend(&sMsg,zPend,nTrim);
			/* php appends the source location only for parser errors that
			 * carry a real line; generic libxml errors print bare. The location
			 * and the LEVEL are the flushing message's, not the held one's. */
			if( aErr[n].iLine > 0 ){
				if( aErr[n].sFile.nByte > 0 ){
					SyBlobFormat(&sMsg," in %z, line: %d",&aErr[n].sFile,aErr[n].iLine);
				}else{
					SyBlobFormat(&sMsg," in Entity, line: %d",aErr[n].iLine);
				}
			}
			SyBlobAppend(&sMsg,"\0",1);
			if( zFnName ){
				SyStringInitFromBuf(&sFunc,zFnName,SyStrlen(zFnName));
			}
			/* php reports libxml's own SEVERITY: a warning (an unsupported XML
			 * version, a DTD the content does not follow) is an E_NOTICE there
			 * and only an error or a fatal is an E_WARNING. The distinction is
			 * visible to any set_error_handler() and to `error_reporting` --
			 * a handler screening on E_WARNING must not see the warnings. */
			PH7_VmThrowError(&(*pVm),zFnName ? &sFunc : 0,
				aErr[n].iLevel == XML_ERR_WARNING ? PH7_CTX_NOTICE : PH7_CTX_WARNING,
				(const char *)SyBlobData(&sMsg));
			SyBlobRelease(&sMsg);
			SyBlobReset(&pVm->sLibxmlPend);
			LibxmlFreeErr(pVm,&aErr[n]);
		}
		SySetTruncate(&pVm->aLibxmlErr,nMark);
	}
}
/*
 * php's OTHER libxml channel.
 *
 * libxml reports an I/O failure through its GENERIC error function rather than
 * the structured one, with the severity spelled INTO the text ("I/O warning :
 * ..."), and php prints that text at E_WARNING whatever the severity says --
 * while the structured copy, which is what `libxml_get_errors()` reports, keeps
 * libxml's own level and carries no such prefix. A caller with a message from
 * that channel hands it here: it goes through the same line buffer as every
 * other diagnostic (so a held fragment is printed in front of it) and takes no
 * source location, because that channel has none.
 */
PH7_PRIVATE void PH7_LibxmlRaiseGeneric(ph7_vm *pVm,const char *zFnName,const char *zMsg)
{
	SyString sFunc;
	SyBlob sOut;
	const char *zPend;
	sxu32 nPend,nTrim;
	SyBlobAppend(&pVm->sLibxmlPend,zMsg,SyStrlen(zMsg));
	zPend = (const char *)SyBlobData(&pVm->sLibxmlPend);
	nPend = SyBlobLength(&pVm->sLibxmlPend);
	if( nPend < 1 || zPend[nPend-1] != '\n' ){
		return;
	}
	nTrim = nPend;
	while( nTrim > 0 && (zPend[nTrim-1] == '\n' || zPend[nTrim-1] == '\r') ){
		nTrim--;
	}
	SyBlobInit(&sOut,&pVm->sAllocator);
	SyBlobAppend(&sOut,zPend,nTrim);
	SyBlobAppend(&sOut,"\0",sizeof(char));
	SyStringInitFromBuf(&sFunc,zFnName,zFnName ? SyStrlen(zFnName) : 0);
	PH7_VmThrowError(&(*pVm),zFnName ? &sFunc : 0,PH7_CTX_WARNING,
		(const char *)SyBlobData(&sOut));
	SyBlobRelease(&sOut);
	SyBlobReset(&pVm->sLibxmlPend);
}
/*
 * Allocate and register a new document shell on the per-VM registry.
 * Returns NULL on allocation failure (the caller reports OOM).
 */
PH7_PRIVATE phl_xmldoc * PH7_LibxmlNewDoc(ph7_vm *pVm,void *pXmlDocPtr)
{
	phl_xmldoc *pDoc;
	pDoc = (phl_xmldoc *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_xmldoc));
	if( pDoc == 0 ){
		return 0;
	}
	SyZero(pDoc,sizeof(phl_xmldoc));
	SySetInit(&pDoc->aOrphans,&pVm->sAllocator,sizeof(void *));
	pDoc->pDoc = pXmlDocPtr;
	pDoc->pVm = &(*pVm);
	pDoc->bPreserveWS = 1;  /* DOMDocument->preserveWhiteSpace default */
	pDoc->bFormatOutput = 0;
	pDoc->pNext = (phl_xmldoc *)pVm->pXmlDocs;
	pVm->pXmlDocs = (void *)pDoc;
	return pDoc;
}

/* ===== Constants (php ext/libxml + ext/dom node types) ===== */

#define LIBXML_INT_CONST(FN,VALUE) \
	static void FN(ph7_value *pVal,void *pUnused){ \
		SXUNUSED(pUnused); \
		ph7_value_int64(pVal,(ph7_int64)(VALUE)); \
	}
LIBXML_INT_CONST(LibxmlConst_VERSION,        LIBXML_VERSION)
LIBXML_INT_CONST(LibxmlConst_RECOVER,        XML_PARSE_RECOVER)
LIBXML_INT_CONST(LibxmlConst_HTML_NOIMPLIED, 8192)   /* HTML_PARSE_NOIMPLIED */
LIBXML_INT_CONST(LibxmlConst_HTML_NODEFDTD,  4)      /* HTML_PARSE_NODEFDTD */
LIBXML_INT_CONST(LibxmlConst_NOENT,          XML_PARSE_NOENT)
LIBXML_INT_CONST(LibxmlConst_DTDLOAD,        XML_PARSE_DTDLOAD)
LIBXML_INT_CONST(LibxmlConst_DTDATTR,        XML_PARSE_DTDATTR)
LIBXML_INT_CONST(LibxmlConst_DTDVALID,       XML_PARSE_DTDVALID)
LIBXML_INT_CONST(LibxmlConst_NOERROR,        XML_PARSE_NOERROR)
LIBXML_INT_CONST(LibxmlConst_NOWARNING,      XML_PARSE_NOWARNING)
LIBXML_INT_CONST(LibxmlConst_NOBLANKS,       XML_PARSE_NOBLANKS)
LIBXML_INT_CONST(LibxmlConst_XINCLUDE,       XML_PARSE_XINCLUDE)
LIBXML_INT_CONST(LibxmlConst_NSCLEAN,        XML_PARSE_NSCLEAN)
LIBXML_INT_CONST(LibxmlConst_NOCDATA,        XML_PARSE_NOCDATA)
LIBXML_INT_CONST(LibxmlConst_NONET,          XML_PARSE_NONET)
LIBXML_INT_CONST(LibxmlConst_PEDANTIC,       XML_PARSE_PEDANTIC)
LIBXML_INT_CONST(LibxmlConst_COMPACT,        XML_PARSE_COMPACT)
LIBXML_INT_CONST(LibxmlConst_PARSEHUGE,      XML_PARSE_HUGE)
LIBXML_INT_CONST(LibxmlConst_BIGLINES,       XML_PARSE_BIG_LINES)
LIBXML_INT_CONST(LibxmlConst_NOXMLDECL,      2)      /* XML_SAVE_NO_DECL */
LIBXML_INT_CONST(LibxmlConst_NOEMPTYTAG,     4)      /* XML_SAVE_NO_EMPTY */
LIBXML_INT_CONST(LibxmlConst_SCHEMA_CREATE,  1)      /* XML_SCHEMA_VAL_VC_I_CREATE */
LIBXML_INT_CONST(LibxmlConst_ERR_NONE,       XML_ERR_NONE)
LIBXML_INT_CONST(LibxmlConst_ERR_WARNING,    XML_ERR_WARNING)
LIBXML_INT_CONST(LibxmlConst_ERR_ERROR,      XML_ERR_ERROR)
LIBXML_INT_CONST(LibxmlConst_ERR_FATAL,      XML_ERR_FATAL)
/* ext/dom node-type constants (values fixed by the DOM spec / libxml enums) */
LIBXML_INT_CONST(LibxmlConst_ELEMENT_NODE,        XML_ELEMENT_NODE)
LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_NODE,      XML_ATTRIBUTE_NODE)
LIBXML_INT_CONST(LibxmlConst_TEXT_NODE,           XML_TEXT_NODE)
LIBXML_INT_CONST(LibxmlConst_CDATA_SECTION_NODE,  XML_CDATA_SECTION_NODE)
LIBXML_INT_CONST(LibxmlConst_ENTITY_REF_NODE,     XML_ENTITY_REF_NODE)
LIBXML_INT_CONST(LibxmlConst_ENTITY_NODE,         XML_ENTITY_NODE)
LIBXML_INT_CONST(LibxmlConst_PI_NODE,             XML_PI_NODE)
LIBXML_INT_CONST(LibxmlConst_COMMENT_NODE,        XML_COMMENT_NODE)
LIBXML_INT_CONST(LibxmlConst_DOCUMENT_NODE,       XML_DOCUMENT_NODE)
LIBXML_INT_CONST(LibxmlConst_DOCUMENT_TYPE_NODE,  XML_DOCUMENT_TYPE_NODE)
LIBXML_INT_CONST(LibxmlConst_DOCUMENT_FRAG_NODE,  XML_DOCUMENT_FRAG_NODE)
LIBXML_INT_CONST(LibxmlConst_NOTATION_NODE,       XML_NOTATION_NODE)
LIBXML_INT_CONST(LibxmlConst_HTML_DOCUMENT_NODE,  XML_HTML_DOCUMENT_NODE)
LIBXML_INT_CONST(LibxmlConst_DTD_NODE,            XML_DTD_NODE)
LIBXML_INT_CONST(LibxmlConst_ELEMENT_DECL_NODE,   XML_ELEMENT_DECL)
LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_DECL_NODE, XML_ATTRIBUTE_DECL)
LIBXML_INT_CONST(LibxmlConst_ENTITY_DECL_NODE,    XML_ENTITY_DECL)
/* php spells libxml's XML_NAMESPACE_DECL twice, under both DOM's name for a
 * namespace node and libxml's own. */
LIBXML_INT_CONST(LibxmlConst_NAMESPACE_DECL_NODE, XML_NAMESPACE_DECL)
LIBXML_INT_CONST(LibxmlConst_LOCAL_NAMESPACE,     XML_NAMESPACE_DECL)
/*
 * The DTD attribute-TYPE enum. php's numbers are libxml's xmlAttributeType with
 * one deliberate hole: php has no XML_ATTRIBUTE_ENTITIES and gives the name
 * XML_ATTRIBUTE_ENTITY libxml's ENTITIES value (6), so the two disagree about
 * what "entity" means by one.  php's numbering is the contract.
 */
LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_CDATA,       XML_ATTRIBUTE_CDATA)
LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_ID,          XML_ATTRIBUTE_ID)
LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_IDREF,       XML_ATTRIBUTE_IDREF)
LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_IDREFS,      XML_ATTRIBUTE_IDREFS)
LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_ENTITY,      XML_ATTRIBUTE_ENTITIES)
LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_NMTOKEN,     XML_ATTRIBUTE_NMTOKEN)
LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_NMTOKENS,    XML_ATTRIBUTE_NMTOKENS)
LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_ENUMERATION, XML_ATTRIBUTE_ENUMERATION)
LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_NOTATION,    XML_ATTRIBUTE_NOTATION)
/*
 * ext/dom's DOMException codes -- the DOM level-2 numbering an exception's
 * getCode() answers, which is what a catch tests to tell one refusal from
 * another (php's own zero, DOM_PHP_ERR, is the code for everything that is not
 * a DOM error).
 */
LIBXML_INT_CONST(LibxmlConst_PHP_ERR,                  0)
LIBXML_INT_CONST(LibxmlConst_INDEX_SIZE_ERR,           1)
LIBXML_INT_CONST(LibxmlConst_DOMSTRING_SIZE_ERR,       2)
LIBXML_INT_CONST(LibxmlConst_HIERARCHY_REQUEST_ERR,    3)
LIBXML_INT_CONST(LibxmlConst_WRONG_DOCUMENT_ERR,       4)
LIBXML_INT_CONST(LibxmlConst_INVALID_CHARACTER_ERR,    5)
LIBXML_INT_CONST(LibxmlConst_NO_DATA_ALLOWED_ERR,      6)
LIBXML_INT_CONST(LibxmlConst_NO_MODIFICATION_ALLOWED_ERR, 7)
LIBXML_INT_CONST(LibxmlConst_NOT_FOUND_ERR,            8)
LIBXML_INT_CONST(LibxmlConst_NOT_SUPPORTED_ERR,        9)
LIBXML_INT_CONST(LibxmlConst_INUSE_ATTRIBUTE_ERR,     10)
LIBXML_INT_CONST(LibxmlConst_INVALID_STATE_ERR,       11)
LIBXML_INT_CONST(LibxmlConst_SYNTAX_ERR,              12)
LIBXML_INT_CONST(LibxmlConst_INVALID_MODIFICATION_ERR,13)
LIBXML_INT_CONST(LibxmlConst_NAMESPACE_ERR,           14)
LIBXML_INT_CONST(LibxmlConst_INVALID_ACCESS_ERR,      15)
LIBXML_INT_CONST(LibxmlConst_VALIDATION_ERR,          16)

static void LibxmlConst_DOTTED_VERSION(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_string(pVal,LIBXML_DOTTED_VERSION,-1);
}
static void LibxmlConst_LOADED_VERSION(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	/* php exposes the runtime-loaded version string here */
	ph7_value_string(pVal,(const char *)xmlParserVersion,-1);
}

PH7_PRIVATE void PH7_RegisterLibxmlConstants(ph7_vm *pVm)
{
	static const struct {
		const char *zName;
		void (*xExpand)(ph7_value *,void *);
	} aConst[] = {
		{ "LIBXML_VERSION",        LibxmlConst_VERSION        },
		{ "LIBXML_DOTTED_VERSION", LibxmlConst_DOTTED_VERSION },
		{ "LIBXML_LOADED_VERSION", LibxmlConst_LOADED_VERSION },
		{ "LIBXML_RECOVER",        LibxmlConst_RECOVER        },
		{ "LIBXML_HTML_NOIMPLIED", LibxmlConst_HTML_NOIMPLIED },
		{ "LIBXML_HTML_NODEFDTD",  LibxmlConst_HTML_NODEFDTD  },
		{ "LIBXML_NOENT",          LibxmlConst_NOENT          },
		{ "LIBXML_DTDLOAD",        LibxmlConst_DTDLOAD        },
		{ "LIBXML_DTDATTR",        LibxmlConst_DTDATTR        },
		{ "LIBXML_DTDVALID",       LibxmlConst_DTDVALID       },
		{ "LIBXML_NOERROR",        LibxmlConst_NOERROR        },
		{ "LIBXML_NOWARNING",      LibxmlConst_NOWARNING      },
		{ "LIBXML_NOBLANKS",       LibxmlConst_NOBLANKS       },
		{ "LIBXML_XINCLUDE",       LibxmlConst_XINCLUDE       },
		{ "LIBXML_NSCLEAN",        LibxmlConst_NSCLEAN        },
		{ "LIBXML_NOCDATA",        LibxmlConst_NOCDATA        },
		{ "LIBXML_NONET",          LibxmlConst_NONET          },
		{ "LIBXML_PEDANTIC",       LibxmlConst_PEDANTIC       },
		{ "LIBXML_COMPACT",        LibxmlConst_COMPACT        },
		{ "LIBXML_PARSEHUGE",      LibxmlConst_PARSEHUGE      },
		{ "LIBXML_BIGLINES",       LibxmlConst_BIGLINES       },
		{ "LIBXML_NOXMLDECL",      LibxmlConst_NOXMLDECL      },
		{ "LIBXML_NOEMPTYTAG",     LibxmlConst_NOEMPTYTAG     },
		{ "LIBXML_SCHEMA_CREATE",  LibxmlConst_SCHEMA_CREATE  },
		{ "LIBXML_ERR_NONE",       LibxmlConst_ERR_NONE       },
		{ "LIBXML_ERR_WARNING",    LibxmlConst_ERR_WARNING    },
		{ "LIBXML_ERR_ERROR",      LibxmlConst_ERR_ERROR      },
		{ "LIBXML_ERR_FATAL",      LibxmlConst_ERR_FATAL      },
		{ "XML_ELEMENT_NODE",       LibxmlConst_ELEMENT_NODE       },
		{ "XML_ATTRIBUTE_NODE",     LibxmlConst_ATTRIBUTE_NODE     },
		{ "XML_TEXT_NODE",          LibxmlConst_TEXT_NODE          },
		{ "XML_CDATA_SECTION_NODE", LibxmlConst_CDATA_SECTION_NODE },
		{ "XML_ENTITY_REF_NODE",    LibxmlConst_ENTITY_REF_NODE    },
		{ "XML_ENTITY_NODE",        LibxmlConst_ENTITY_NODE        },
		{ "XML_PI_NODE",            LibxmlConst_PI_NODE            },
		{ "XML_COMMENT_NODE",       LibxmlConst_COMMENT_NODE       },
		{ "XML_DOCUMENT_NODE",      LibxmlConst_DOCUMENT_NODE      },
		{ "XML_DOCUMENT_TYPE_NODE", LibxmlConst_DOCUMENT_TYPE_NODE },
		{ "XML_DOCUMENT_FRAG_NODE", LibxmlConst_DOCUMENT_FRAG_NODE },
		{ "XML_NOTATION_NODE",      LibxmlConst_NOTATION_NODE      },
		{ "XML_HTML_DOCUMENT_NODE", LibxmlConst_HTML_DOCUMENT_NODE },
		{ "XML_DTD_NODE",           LibxmlConst_DTD_NODE           },
		{ "XML_ELEMENT_DECL_NODE",  LibxmlConst_ELEMENT_DECL_NODE  },
		{ "XML_ATTRIBUTE_DECL_NODE",LibxmlConst_ATTRIBUTE_DECL_NODE},
		{ "XML_ENTITY_DECL_NODE",   LibxmlConst_ENTITY_DECL_NODE   },
		{ "XML_NAMESPACE_DECL_NODE",LibxmlConst_NAMESPACE_DECL_NODE},
		{ "XML_LOCAL_NAMESPACE",    LibxmlConst_LOCAL_NAMESPACE    },
		{ "XML_ATTRIBUTE_CDATA",       LibxmlConst_ATTRIBUTE_CDATA       },
		{ "XML_ATTRIBUTE_ID",          LibxmlConst_ATTRIBUTE_ID          },
		{ "XML_ATTRIBUTE_IDREF",       LibxmlConst_ATTRIBUTE_IDREF       },
		{ "XML_ATTRIBUTE_IDREFS",      LibxmlConst_ATTRIBUTE_IDREFS      },
		{ "XML_ATTRIBUTE_ENTITY",      LibxmlConst_ATTRIBUTE_ENTITY      },
		{ "XML_ATTRIBUTE_NMTOKEN",     LibxmlConst_ATTRIBUTE_NMTOKEN     },
		{ "XML_ATTRIBUTE_NMTOKENS",    LibxmlConst_ATTRIBUTE_NMTOKENS    },
		{ "XML_ATTRIBUTE_ENUMERATION", LibxmlConst_ATTRIBUTE_ENUMERATION },
		{ "XML_ATTRIBUTE_NOTATION",    LibxmlConst_ATTRIBUTE_NOTATION    },
		{ "DOM_PHP_ERR",                    LibxmlConst_PHP_ERR                    },
		{ "DOM_INDEX_SIZE_ERR",             LibxmlConst_INDEX_SIZE_ERR             },
		{ "DOMSTRING_SIZE_ERR",             LibxmlConst_DOMSTRING_SIZE_ERR         },
		{ "DOM_HIERARCHY_REQUEST_ERR",      LibxmlConst_HIERARCHY_REQUEST_ERR      },
		{ "DOM_WRONG_DOCUMENT_ERR",         LibxmlConst_WRONG_DOCUMENT_ERR         },
		{ "DOM_INVALID_CHARACTER_ERR",      LibxmlConst_INVALID_CHARACTER_ERR      },
		{ "DOM_NO_DATA_ALLOWED_ERR",        LibxmlConst_NO_DATA_ALLOWED_ERR        },
		{ "DOM_NO_MODIFICATION_ALLOWED_ERR",LibxmlConst_NO_MODIFICATION_ALLOWED_ERR},
		{ "DOM_NOT_FOUND_ERR",              LibxmlConst_NOT_FOUND_ERR              },
		{ "DOM_NOT_SUPPORTED_ERR",          LibxmlConst_NOT_SUPPORTED_ERR          },
		{ "DOM_INUSE_ATTRIBUTE_ERR",        LibxmlConst_INUSE_ATTRIBUTE_ERR        },
		{ "DOM_INVALID_STATE_ERR",          LibxmlConst_INVALID_STATE_ERR          },
		{ "DOM_SYNTAX_ERR",                 LibxmlConst_SYNTAX_ERR                 },
		{ "DOM_INVALID_MODIFICATION_ERR",   LibxmlConst_INVALID_MODIFICATION_ERR   },
		{ "DOM_NAMESPACE_ERR",              LibxmlConst_NAMESPACE_ERR              },
		{ "DOM_INVALID_ACCESS_ERR",         LibxmlConst_INVALID_ACCESS_ERR         },
		{ "DOM_VALIDATION_ERR",             LibxmlConst_VALIDATION_ERR             },
	};
	sxu32 n;
	for( n = 0 ; n < sizeof(aConst)/sizeof(aConst[0]) ; n++ ){
		ph7_create_constant(&(*pVm),aConst[n].zName,aConst[n].xExpand,0);
	}
}
/* ===== libxml_* native thunks ===== */

/*
 * bool __libxml_use_internal_errors(?bool $use_errors = null)
 *  Flip (or just report, on null/omitted) the internal-capture flag and
 *  return the PREVIOUS state -- php semantics.
 */
static int vm_builtin_libxml_use_internal_errors(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	int bPrev = pVm->bLibxmlInternalErr;
	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){
		pVm->bLibxmlInternalErr = ph7_value_to_bool(apArg[0]) ? 1 : 0;
		if( !pVm->bLibxmlInternalErr ){
			/* php frees the accumulated buffer when capture turns OFF */
			LibxmlClearQueue(pVm);
		}
	}
	ph7_result_bool(pCtx,bPrev);
	return PH7_OK;
}
/*
 * array __libxml_get_errors_raw()
 *  The queued errors as raw assoc arrays, oldest first.
 */
/*
 * Build one LibXMLError from a recorded error.
 *
 * The prelude did this in PHP (`__phl_libxml_err_obj()` copying six array keys
 * onto a `new LibXMLError`), over an array these accessors returned purely so
 * that PHP could reshape it. Both the array hop and the helper are gone: the
 * accessors below now answer the objects php answers.
 */
static ph7_class_instance * LibxmlErrObject(ph7_context *pCtx,phl_libxml_err *pErr)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class *pClass = PH7_VmExtractClass(pVm,"LibXMLError",sizeof("LibXMLError")-1,0,0);
	ph7_class_instance *pObj;
	ph7_value sVal;
	SyString sStr;
	if( pClass == 0 ){
		return 0;
	}
	pObj = PH7_NewClassInstance(pVm,pClass);
	if( pObj == 0 ){
		return 0;
	}
	PH7_MemObjInit(pVm,&sVal);
	PH7_MemObjInitFromInt(pVm,&sVal,pErr->iLevel);
	PH7_NativeSetProp(pVm,pObj,"level",sizeof("level")-1,&sVal);
	PH7_MemObjInitFromInt(pVm,&sVal,pErr->iCode);
	PH7_NativeSetProp(pVm,pObj,"code",sizeof("code")-1,&sVal);
	PH7_MemObjInitFromInt(pVm,&sVal,pErr->iColumn);
	PH7_NativeSetProp(pVm,pObj,"column",sizeof("column")-1,&sVal);
	PH7_MemObjInitFromInt(pVm,&sVal,pErr->iLine);
	PH7_NativeSetProp(pVm,pObj,"line",sizeof("line")-1,&sVal);
	SyStringInitFromBuf(&sStr,pErr->sMsg.zString ? pErr->sMsg.zString : "",pErr->sMsg.nByte);
	PH7_MemObjInitFromString(pVm,&sVal,&sStr);
	PH7_NativeSetProp(pVm,pObj,"message",sizeof("message")-1,&sVal);
	SyStringInitFromBuf(&sStr,pErr->sFile.zString ? pErr->sFile.zString : "",pErr->sFile.nByte);
	PH7_MemObjInitFromString(pVm,&sVal,&sStr);
	PH7_NativeSetProp(pVm,pObj,"file",sizeof("file")-1,&sVal);
	PH7_MemObjRelease(&sVal);
	return pObj;
}
/* array libxml_get_errors() */
static int vm_builtin_libxml_get_errors_raw(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	phl_libxml_err *aErr = (phl_libxml_err *)SySetBasePtr(&pVm->aLibxmlErr);
	ph7_value *pList = ph7_context_new_array(pCtx);
	ph7_value *pWorker = ph7_context_new_scalar(pCtx);
	sxu32 n;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pList == 0 || pWorker == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	for( n = 0 ; n < SySetUsed(&pVm->aLibxmlErr) ; ++n ){
		ph7_class_instance *pObj = LibxmlErrObject(pCtx,&aErr[n]);
		ph7_value sEntry;
		if( pObj == 0 ){
			break;
		}
		/* The list takes over the instance's initial iRef=1: ph7_array_add_elem
		 * copies the slot (taking its own reference), so the local one is dropped. */
		PH7_MemObjInit(pVm,&sEntry);
		sEntry.x.pOther = pObj;
		sEntry.iFlags = MEMOBJ_OBJ;
		ph7_array_add_elem(pList,0,&sEntry);
		PH7_ClassInstanceUnref(pObj);
	}
	ph7_result_value(pCtx,pList);
	return PH7_OK;
}
/*
 * void __libxml_clear_errors()
 */
static int vm_builtin_libxml_clear_errors(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	PH7_LibxmlClearErrors(pCtx->pVm);
	ph7_result_null(pCtx);
	return PH7_OK;
}
/*
 * array|false __libxml_get_last_error_raw()
 */
static int vm_builtin_libxml_get_last_error_raw(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	phl_libxml_err *pLast = (phl_libxml_err *)pVm->pLibxmlLastErr;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pLast == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	{
		ph7_class_instance *pObj = LibxmlErrObject(pCtx,pLast);
		ph7_value sRes;
		if( pObj == 0 ){
			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		PH7_MemObjInit(pVm,&sRes);
		sRes.x.pOther = pObj;
		sRes.iFlags = MEMOBJ_OBJ;
		ph7_result_value(pCtx,&sRes);   /* takes its own reference */
		PH7_ClassInstanceUnref(pObj);
	}
	return PH7_OK;
}

/*
 * ?callable libxml_get_external_entity_loader()
 *  The stored resolver VERBATIM (a callable string answers as that string),
 *  or null for the default loader -- php's answer shape.
 */
static int vm_builtin_libxml_get_external_entity_loader(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( (pVm->sXmlEntLoader.iFlags & MEMOBJ_NULL) == 0 ){
		ph7_result_value(pCtx,&pVm->sXmlEntLoader); /* makes its own copy */
	}else{
		ph7_result_null(pCtx);
	}
	return PH7_OK;
}
/*
 * true libxml_set_external_entity_loader(?callable $resolver_function)
 *  Store the resolver (null restores the default). The slot is never
 *  INVOKED here -- no PHL parse path loads an external entity, the same
 *  off-by-default php's sanitized parser options enforce -- so the
 *  round-trip contract is the whole observable surface.
 */
static int vm_builtin_libxml_set_external_entity_loader(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,1);
		return PH7_OK;
	}
	if( !ph7_value_is_null(apArg[0]) ){
		/* php screens through the FCC machinery: a string must resolve as a
		 * CALLABLE, and the refusal names the callback rule. */
		sxi32 rc = PH7_CheckCallbackArg(pCtx,apArg[0],1,"resolver_function",1);
		if( rc != PH7_OK ){
			return rc;
		}
	}
	PH7_MemObjRelease(&pVm->sXmlEntLoader);
	if( !ph7_value_is_null(apArg[0]) ){
		PH7_MemObjStore(apArg[0],&pVm->sXmlEntLoader);
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * void libxml_set_streams_context($context)
 *  Take a stream-context RESOURCE and keep it for the document loaders.
 *  php validates lazily at the next load; PHL has no loader that would
 *  ever read it (the consumer is the http:// wrapper), so
 *  the check runs here, with php's own two messages.
 */
static int vm_builtin_libxml_set_streams_context(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	const io_private *pDev;
	if( nArg < 1 ){
		return PH7_OK;
	}
	if( (apArg[0]->iFlags & MEMOBJ_RES) == 0 ){
		const char *zType = "null";
		if( apArg[0]->iFlags & MEMOBJ_HASHMAP ){ zType = "array"; }
		else if( apArg[0]->iFlags & MEMOBJ_OBJ ){ zType = "object"; }
		else if( apArg[0]->iFlags & MEMOBJ_STRING ){ zType = "string"; }
		else if( apArg[0]->iFlags & MEMOBJ_BOOL ){ zType = "bool"; }
		else if( apArg[0]->iFlags & MEMOBJ_REAL ){ zType = "float"; }
		else if( apArg[0]->iFlags & MEMOBJ_INT ){ zType = "int"; }
		return PH7_VmThrowException(pCtx,"TypeError",
			"libxml_set_streams_context(): Argument #1 ($context) must be of type resource, %s given",
			zType);
	}
	pDev = (const io_private *)apArg[0]->x.pOther;
	if( pDev == 0 || pDev->iMagic != STREAM_CTX_MAGIC ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"libxml_set_streams_context(): supplied resource is not a valid Stream-Context resource");
	}
	PH7_MemObjRelease(&pVm->sXmlStreamsCtx);
	PH7_MemObjStore(apArg[0],&pVm->sXmlStreamsCtx);
	ph7_result_null(pCtx);
	return PH7_OK;
}

/*
 * The libxml PHP-visible surface: the LibXMLError class plus the seven
 * libxml_* functions (php's eighth, libxml_disable_entity_loader(), is
 * E_DEPRECATED since 8.0 and stays removed per the scope policy §10 --
 * twin-paired in 002-integration/function/libxml/).
 */
/* LibXMLError is declared from C below, and the four libxml_* functions ARE the
 * C routines -- they used to be PHP wrappers over __libxml_* thunks, with a PHP
 * helper reshaping an array into the object. */

/*
 * Install the shared libxml layer: one-time global init, the per-VM state
 * the DOM/XMLWriter surfaces build on, the __libxml_* thunks and the
 * PHP-visible libxml_* functions + LibXMLError class.  Called from
 * PH7_VmInit inside the bCompilingBuiltin window.
 */
PH7_PRIVATE sxi32 PH7_VmInstallLibxml(ph7_vm *pVm)
{
	/* Registered under the names php exposes. There is no thunk and no PHP
	 * wrapper any more: these are the functions, so they are internal for
	 * reflection and get the arity enforcement a prelude wrapper never had. */
	static const struct {
		const char *zName;
		ProchHostFunction xFunc;
	} aFunc[] = {
		{ "libxml_use_internal_errors", vm_builtin_libxml_use_internal_errors },
		{ "libxml_get_errors",          vm_builtin_libxml_get_errors_raw      },
		{ "libxml_clear_errors",        vm_builtin_libxml_clear_errors        },
		{ "libxml_get_last_error",      vm_builtin_libxml_get_last_error_raw  },
		{ "libxml_get_external_entity_loader", vm_builtin_libxml_get_external_entity_loader },
		{ "libxml_set_external_entity_loader", vm_builtin_libxml_set_external_entity_loader },
		{ "libxml_set_streams_context", vm_builtin_libxml_set_streams_context },
	};
	/* Plain data carrier; php declares no methods on it. */
	static const PH7_NativePropDef aProp[] = {
		{ "level",   PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },
		{ "code",    PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },
		{ "column",  PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },
		{ "message", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },
		{ "file",    PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },
		{ "line",    PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },
	};
	static const PH7_NativeClassSpec sSpec = {
		"LibXMLError", 0, 0, 0, 0, 0, 0, 0, aProp, SX_ARRAYSIZE(aProp), 0, 0, 0
	};
	sxu32 n;
	LibxmlGlobalInit();
	SySetInit(&pVm->aLibxmlErr,&pVm->sAllocator,sizeof(phl_libxml_err));
	SyBlobInit(&pVm->sLibxmlPend,&pVm->sAllocator);
	pVm->bLibxmlInternalErr = 0;
	pVm->pLibxmlLastErr = 0;
	pVm->pXmlDocs = 0;
	pVm->pXmlWriters = 0;
	PH7_MemObjInit(pVm,&pVm->sXmlEntLoader);
	PH7_MemObjInit(pVm,&pVm->sXmlStreamsCtx);
	for( n = 0 ; n < SX_ARRAYSIZE(aFunc) ; n++ ){
		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);
	}
	/* The class must exist before the accessors can build one. */
	return PH7_InstallNativeClasses(&(*pVm),&sSpec,1);
}

#else
/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */
typedef int vm_libxml_unused;
#endif /* PH7_ENABLE_LIBXML */
