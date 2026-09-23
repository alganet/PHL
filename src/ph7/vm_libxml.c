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
	pVm->bLibxmlInternalErr = 0;
	pDoc = (phl_xmldoc *)pVm->pXmlDocs;
	while( pDoc ){
		pNext = pDoc->pNext;
		LibxmlFreeDoc(pDoc);
		SyMemBackendFree(&pVm->sAllocator,pDoc);
		pDoc = pNext;
	}
	pVm->pXmlDocs = 0;
	/* XMLWriter buffers live outside SyMemBackend too (see vm_xmlwriter.c) */
	PH7_XmlWriterVmSweep(pVm);
}
/*
 * Final teardown on VM release.  Must run before SyMemBackendRelease()
 * wipes the allocator that holds the registry shells.
 */
PH7_PRIVATE void PH7_LibxmlVmRelease(ph7_vm *pVm)
{
	PH7_LibxmlVmReset(pVm);
	SySetRelease(&pVm->aLibxmlErr);
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
	return SySetUsed(&pVm->aLibxmlErr);
}
PH7_PRIVATE void PH7_LibxmlCaptureEnd(ph7_vm *pVm,sxu32 nMark,const char *zFnName)
{
	xmlSetStructuredErrorFunc(0,0);
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
			sxu32 nTrim = aErr[n].sMsg.nByte;
			/* php trims the trailing newline off the warning copy */
			while( nTrim > 0 && (aErr[n].sMsg.zString[nTrim-1] == '\n' || aErr[n].sMsg.zString[nTrim-1] == '\r') ){
				nTrim--;
			}
			SyBlobInit(&sMsg,&pVm->sAllocator);
			SyBlobAppend(&sMsg,aErr[n].sMsg.zString,nTrim);
			/* php appends the source location only for parser errors that
			 * carry a real line; generic libxml errors print bare. */
			if( aErr[n].iLine > 0 ){
				if( aErr[n].sFile.nByte > 0 ){
					SyBlobFormat(&sMsg," in %z, line: %d",&aErr[n].sFile,aErr[n].iLine);
				}else{
					SyBlobFormat(&sMsg," in Entity, line: %d",aErr[n].iLine);
				}
			}
			SyBlobAppend(&sMsg,"\0",1);
			SyStringInitFromBuf(&sFunc,zFnName,SyStrlen(zFnName));
			PH7_VmThrowError(&(*pVm),zFnName ? &sFunc : 0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));
			SyBlobRelease(&sMsg);
			LibxmlFreeErr(pVm,&aErr[n]);
		}
		SySetTruncate(&pVm->aLibxmlErr,nMark);
	}
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
 * The libxml PHP-visible surface: the LibXMLError class plus the four
 * libxml_* functions, delegating to the __libxml_* thunks above.
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
	};
	/* Plain data carrier; php declares no methods on it. */
	static const PH7_NativePropDef aProp[] = {
		{ "level",   PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 } },
		{ "code",    PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 } },
		{ "column",  PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 } },
		{ "message", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 } },
		{ "file",    PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 } },
		{ "line",    PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 } },
	};
	static const PH7_NativeClassSpec sSpec = {
		"LibXMLError", 0, 0, 0, 0, 0, 0, 0, aProp, SX_ARRAYSIZE(aProp), 0, 0
	};
	sxu32 n;
	LibxmlGlobalInit();
	SySetInit(&pVm->aLibxmlErr,&pVm->sAllocator,sizeof(phl_libxml_err));
	pVm->bLibxmlInternalErr = 0;
	pVm->pLibxmlLastErr = 0;
	pVm->pXmlDocs = 0;
	pVm->pXmlWriters = 0;
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
