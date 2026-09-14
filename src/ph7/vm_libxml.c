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
 * Empty the libxml error queue, releasing the copied message/file strings.
 */
PH7_PRIVATE void PH7_LibxmlClearErrors(ph7_vm *pVm)
{
	phl_libxml_err *aErr = (phl_libxml_err *)SySetBasePtr(&pVm->aLibxmlErr);
	sxu32 n;
	for( n = 0 ; n < SySetUsed(&pVm->aLibxmlErr) ; ++n ){
		if( aErr[n].sMsg.zString ){
			SyMemBackendFree(&pVm->sAllocator,(void *)aErr[n].sMsg.zString);
		}
		if( aErr[n].sFile.zString ){
			SyMemBackendFree(&pVm->sAllocator,(void *)aErr[n].sFile.zString);
		}
	}
	SySetReset(&pVm->aLibxmlErr);
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
/*
 * Install the shared libxml layer: one-time global init plus the per-VM
 * state the DOM/XMLWriter surfaces build on.  Called from PH7_VmInit
 * inside the bCompilingBuiltin window.
 */
PH7_PRIVATE sxi32 PH7_VmInstallLibxml(ph7_vm *pVm)
{
	LibxmlGlobalInit();
	SySetInit(&pVm->aLibxmlErr,&pVm->sAllocator,sizeof(phl_libxml_err));
	pVm->bLibxmlInternalErr = 0;
	pVm->pXmlDocs = 0;
	pVm->pXmlWriters = 0;
	return SXRET_OK;
}

#else
/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */
typedef int vm_libxml_unused;
#endif /* PH7_ENABLE_LIBXML */
