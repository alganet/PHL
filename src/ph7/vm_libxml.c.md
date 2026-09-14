# src/ph7/vm_libxml.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 88/308 lines (28.57%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    4 | ` */` |
|      - |    5 | `#ifdef PH7_ENABLE_LIBXML` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `#include <libxml/parser.h>` |
|      - |    8 | `#include <libxml/xmlerror.h>` |
|      - |    9 | `#include <libxml/tree.h>` |
|      - |   10 |  |
|      - |   11 | `/*` |
|      - |   12 | ` * Shared libxml2 plumbing for the DOM / XMLWriter / libxml_* surfaces.` |
|      - |   13 | ` *` |
|      - |   14 | ` * Memory model: libxml2 stays on its own (system) allocator on purpose.` |
|      - |   15 | ` * Routing it through SyMemBackend would subject libxml2 internals to the` |
|      - |   16 | ` * PHL_MAX_ALLOC fault-injection used by the stress tier, and libxml2 does` |
|      - |   17 | ` * not tolerate mid-parse OOM injection the way the engine's own code does.` |
|      - |   18 | ` * The cost is that allocation-stress tests do not exercise libxml2 OOM` |
|      - |   19 | ` * paths.` |
|      - |   20 | ` *` |
|      - |   21 | ` * Lifetime model: PH7 resources (MEMOBJ_RES) carry no destructor hook, so` |
|      - |   22 | ` * every xmlDoc created on behalf of PHP code is owned by a phl_xmldoc entry` |
|      - |   23 | ` * chained on the per-VM registry (pVm->pXmlDocs) and freed only when the VM` |
|      - |   24 | ` * is reset (a new request on a reused VM) or released -- never while PHP` |
|      - |   25 | ` * code could still hold a node wrapper into it.  Nodes unlinked from their` |
|      - |   26 | ` * tree (removeChild/replaceChild) are parked on the owning phl_xmldoc's` |
|      - |   27 | ` * aOrphans set and freed with the doc.  Docs therefore accumulate until VM` |
|      - |   28 | ` * teardown; acceptable for CLI/per-request VMs, revisit with __destruct-` |
|      - |   29 | ` * driven refcounting if long-lived embeddings ever need early release.` |
|      - |   30 | ` */` |
|      - |   31 |  |
|      - |   32 | `/*` |
|      - |   33 | ` * One-time process-global libxml2 initialization.  Safe as a plain static` |
|      - |   34 | ` * flag under PHL's current single-threaded-execution model (see the` |
|      - |   35 | ` * equivalent note in vm_pcre.c); xmlCleanupParser() is deliberately never` |
|      - |   36 | ` * called -- it is unsafe with threads and process exit reclaims everything.` |
|      - |   37 | ` */` |
|   3884 |   38 | `static void LibxmlGlobalInit(void)` |
|      5 |   39 | `{` |
|      - |   40 | `	static int bInit = 0;` |
|   3889 |   41 | `	if( !bInit ){` |
|   3889 |   42 | `		xmlInitParser();` |
|   3889 |   43 | `		bInit = 1;` |
|   1942 |   44 | `	}` |
|   3889 |   45 | `}` |
|      - |   46 | `/*` |
|      - |   47 | ` * Free one registered document: its orphaned subtrees first, then the tree` |
|      - |   48 | ` * itself.  Registry links and the phl_xmldoc shell live in SyMemBackend and` |
|      - |   49 | ` * are reclaimed with the VM allocator.` |
|      - |   50 | ` */` |
|    ! 0 |   51 | `static void LibxmlFreeDoc(phl_xmldoc *pDoc)` |
|    ! 0 |   52 | `{` |
|    ! 0 |   53 | `	xmlNodePtr *apOrphan = (xmlNodePtr *)SySetBasePtr(&pDoc->aOrphans);` |
|      - |   54 | `	sxu32 n;` |
|    ! 0 |   55 | `	for( n = 0 ; n < SySetUsed(&pDoc->aOrphans) ; ++n ){` |
|    ! 0 |   56 | `		xmlUnlinkNode(apOrphan[n]);` |
|    ! 0 |   57 | `		xmlFreeNode(apOrphan[n]);` |
|    ! 0 |   58 | `	}` |
|    ! 0 |   59 | `	SySetRelease(&pDoc->aOrphans);` |
|    ! 0 |   60 | `	if( pDoc->pDoc ){` |
|    ! 0 |   61 | `		xmlFreeDoc((xmlDocPtr)pDoc->pDoc);` |
|    ! 0 |   62 | `		pDoc->pDoc = 0;` |
|    ! 0 |   63 | `	}` |
|    ! 0 |   64 | `}` |
|      - |   65 | `/*` |
|      - |   66 | ` * Reset the per-VM libxml state between executions: drop the accumulated` |
|      - |   67 | ` * error queue and free every document from the previous request.  Called` |
|      - |   68 | ` * from PH7_VmMakeReady() (which runs once per exec, including on VM reuse` |
|      - |   69 | ` * by the -S server) alongside the other per-exec field resets.` |
|      - |   70 | ` */` |
|   3434 |   71 | `PH7_PRIVATE void PH7_LibxmlVmReset(ph7_vm *pVm)` |
|      5 |   72 | `{` |
|      - |   73 | `	phl_xmldoc *pDoc,*pNext;` |
|   3439 |   74 | `	PH7_LibxmlClearErrors(pVm);` |
|   3439 |   75 | `	pVm->bLibxmlInternalErr = 0;` |
|   3439 |   76 | `	pDoc = (phl_xmldoc *)pVm->pXmlDocs;` |
|   3439 |   77 | `	while( pDoc ){` |
|    ! 0 |   78 | `		pNext = pDoc->pNext;` |
|    ! 0 |   79 | `		LibxmlFreeDoc(pDoc);` |
|    ! 0 |   80 | `		SyMemBackendFree(&pVm->sAllocator,pDoc);` |
|    ! 0 |   81 | `		pDoc = pNext;` |
|    ! 0 |   82 | `	}` |
|   3439 |   83 | `	pVm->pXmlDocs = 0;` |
|   3439 |   84 | `}` |
|      - |   85 | `/*` |
|      - |   86 | ` * Final teardown on VM release.  Must run before SyMemBackendRelease()` |
|      - |   87 | ` * wipes the allocator that holds the registry shells.` |
|      - |   88 | ` */` |
|   3426 |   89 | `PH7_PRIVATE void PH7_LibxmlVmRelease(ph7_vm *pVm)` |
|      5 |   90 | `{` |
|   3431 |   91 | `	PH7_LibxmlVmReset(pVm);` |
|   3431 |   92 | `	SySetRelease(&pVm->aLibxmlErr);` |
|   3431 |   93 | `}` |
|      - |   94 | `/*` |
|      - |   95 | ` * Release the copied message/file strings of one queue entry.` |
|      - |   96 | ` */` |
|    ! 0 |   97 | `static void LibxmlFreeErr(ph7_vm *pVm,phl_libxml_err *pErr)` |
|    ! 0 |   98 | `{` |
|    ! 0 |   99 | `	if( pErr->sMsg.zString ){` |
|    ! 0 |  100 | `		SyMemBackendFree(&pVm->sAllocator,(void *)pErr->sMsg.zString);` |
|    ! 0 |  101 | `	}` |
|    ! 0 |  102 | `	if( pErr->sFile.zString ){` |
|    ! 0 |  103 | `		SyMemBackendFree(&pVm->sAllocator,(void *)pErr->sFile.zString);` |
|    ! 0 |  104 | `	}` |
|    ! 0 |  105 | `	SyStringInitFromBuf(&pErr->sMsg,0,0);` |
|    ! 0 |  106 | `	SyStringInitFromBuf(&pErr->sFile,0,0);` |
|    ! 0 |  107 | `}` |
|      - |  108 | `/*` |
|      - |  109 | ` * Empty the libxml error queue (and the last-error slot), releasing the` |
|      - |  110 | ` * copied message/file strings.` |
|      - |  111 | ` */` |
|   3438 |  112 | `PH7_PRIVATE void PH7_LibxmlClearErrors(ph7_vm *pVm)` |
|      5 |  113 | `{` |
|   3443 |  114 | `	phl_libxml_err *aErr = (phl_libxml_err *)SySetBasePtr(&pVm->aLibxmlErr);` |
|      - |  115 | `	sxu32 n;` |
|   3443 |  116 | `	for( n = 0 ; n < SySetUsed(&pVm->aLibxmlErr) ; ++n ){` |
|    ! 0 |  117 | `		LibxmlFreeErr(pVm,&aErr[n]);` |
|    ! 0 |  118 | `	}` |
|   3443 |  119 | `	SySetReset(&pVm->aLibxmlErr);` |
|   3443 |  120 | `	if( pVm->pLibxmlLastErr ){` |
|    ! 0 |  121 | `		LibxmlFreeErr(pVm,(phl_libxml_err *)pVm->pLibxmlLastErr);` |
|    ! 0 |  122 | `		SyMemBackendFree(&pVm->sAllocator,pVm->pLibxmlLastErr);` |
|    ! 0 |  123 | `		pVm->pLibxmlLastErr = 0;` |
|    ! 0 |  124 | `	}` |
|   3443 |  125 | `}` |
|      - |  126 | `/*` |
|      - |  127 | ` * Structured-error callback installed while a libxml2 entry point runs` |
|      - |  128 | ` * between PH7_LibxmlCaptureBegin/End.  Every reported error is queued on` |
|      - |  129 | ` * the per-VM list (the capture-end decides whether it stays there or is` |
|      - |  130 | ` * drained as a PHP warning) and copied into the last-error slot.` |
|      - |  131 | ` */` |
|      - |  132 | `#if LIBXML_VERSION >= 21200` |
|    ! 0 |  133 | `static void LibxmlStructuredErrHandler(void *pUserData,const xmlError *pErr)` |
|      - |  134 | `#else` |
|    ! 0 |  135 | `static void LibxmlStructuredErrHandler(void *pUserData,xmlErrorPtr pErr)` |
|      - |  136 | `#endif` |
|    ! 0 |  137 | `{` |
|    ! 0 |  138 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - |  139 | `	phl_libxml_err sEntry;` |
|      - |  140 | `	phl_libxml_err *pLast;` |
|    ! 0 |  141 | `	if( pErr == 0 \|\| pVm == 0 ){` |
|    ! 0 |  142 | `		return;` |
|      - |  143 | `	}` |
|    ! 0 |  144 | `	SyZero(&sEntry,sizeof(sEntry));` |
|    ! 0 |  145 | `	sEntry.iLevel = (int)pErr->level;` |
|    ! 0 |  146 | `	sEntry.iCode = pErr->code;` |
|    ! 0 |  147 | `	sEntry.iLine = pErr->line;` |
|    ! 0 |  148 | `	sEntry.iColumn = pErr->int2; /* libxml keeps the column in int2 */` |
|    ! 0 |  149 | `	if( pErr->message ){` |
|    ! 0 |  150 | `		sxu32 nMsg = SyStrlen(pErr->message);` |
|    ! 0 |  151 | `		char *zMsg = SyMemBackendStrDup(&pVm->sAllocator,pErr->message,nMsg);` |
|    ! 0 |  152 | `		if( zMsg ){` |
|      - |  153 | `			/* Trailing newline kept: php's LibXMLError->message preserves it */` |
|    ! 0 |  154 | `			SyStringInitFromBuf(&sEntry.sMsg,zMsg,nMsg);` |
|    ! 0 |  155 | `		}` |
|    ! 0 |  156 | `	}` |
|    ! 0 |  157 | `	if( pErr->file ){` |
|    ! 0 |  158 | `		sxu32 nFile = SyStrlen(pErr->file);` |
|    ! 0 |  159 | `		char *zFile = SyMemBackendStrDup(&pVm->sAllocator,pErr->file,nFile);` |
|    ! 0 |  160 | `		if( zFile ){` |
|    ! 0 |  161 | `			SyStringInitFromBuf(&sEntry.sFile,zFile,nFile);` |
|    ! 0 |  162 | `		}` |
|    ! 0 |  163 | `	}` |
|    ! 0 |  164 | `	SySetPut(&pVm->aLibxmlErr,(const void *)&sEntry);` |
|      - |  165 | `	/* Mirror into the last-error slot (its strings are independent copies` |
|      - |  166 | `	 * so queue draining cannot invalidate it). */` |
|    ! 0 |  167 | `	pLast = (phl_libxml_err *)pVm->pLibxmlLastErr;` |
|    ! 0 |  168 | `	if( pLast == 0 ){` |
|    ! 0 |  169 | `		pLast = (phl_libxml_err *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_libxml_err));` |
|    ! 0 |  170 | `		if( pLast == 0 ){` |
|    ! 0 |  171 | `			return;` |
|      - |  172 | `		}` |
|    ! 0 |  173 | `		SyZero(pLast,sizeof(phl_libxml_err));` |
|    ! 0 |  174 | `		pVm->pLibxmlLastErr = (void *)pLast;` |
|    ! 0 |  175 | `	}else{` |
|    ! 0 |  176 | `		LibxmlFreeErr(pVm,pLast);` |
|      - |  177 | `	}` |
|    ! 0 |  178 | `	pLast->iLevel = sEntry.iLevel;` |
|    ! 0 |  179 | `	pLast->iCode = sEntry.iCode;` |
|    ! 0 |  180 | `	pLast->iLine = sEntry.iLine;` |
|    ! 0 |  181 | `	pLast->iColumn = sEntry.iColumn;` |
|    ! 0 |  182 | `	if( sEntry.sMsg.zString ){` |
|    ! 0 |  183 | `		char *zDup = SyMemBackendStrDup(&pVm->sAllocator,sEntry.sMsg.zString,sEntry.sMsg.nByte);` |
|    ! 0 |  184 | `		if( zDup ){` |
|    ! 0 |  185 | `			SyStringInitFromBuf(&pLast->sMsg,zDup,sEntry.sMsg.nByte);` |
|    ! 0 |  186 | `		}` |
|    ! 0 |  187 | `	}` |
|    ! 0 |  188 | `	if( sEntry.sFile.zString ){` |
|    ! 0 |  189 | `		char *zDup = SyMemBackendStrDup(&pVm->sAllocator,sEntry.sFile.zString,sEntry.sFile.nByte);` |
|    ! 0 |  190 | `		if( zDup ){` |
|    ! 0 |  191 | `			SyStringInitFromBuf(&pLast->sFile,zDup,sEntry.sFile.nByte);` |
|    ! 0 |  192 | `		}` |
|    ! 0 |  193 | `	}` |
|    ! 0 |  194 | `}` |
|      - |  195 | `/*` |
|      - |  196 | ` * Bracket a libxml2 entry point.  Begin installs the structured handler` |
|      - |  197 | ` * routed at this VM and returns the current queue depth; End restores the` |
|      - |  198 | ` * default handler and, when libxml_use_internal_errors() is OFF, drains` |
|      - |  199 | ` * every entry recorded since the mark as php-style warnings:` |
|      - |  200 | ` *   funcname(): <message> in <Entity\|file>, line: <n>` |
|      - |  201 | ` * (php's exact wording for the memory-parser case).` |
|      - |  202 | ` */` |
|    ! 0 |  203 | `PH7_PRIVATE sxu32 PH7_LibxmlCaptureBegin(ph7_vm *pVm)` |
|    ! 0 |  204 | `{` |
|    ! 0 |  205 | `	xmlSetStructuredErrorFunc(pVm,LibxmlStructuredErrHandler);` |
|    ! 0 |  206 | `	return SySetUsed(&pVm->aLibxmlErr);` |
|    ! 0 |  207 | `}` |
|    ! 0 |  208 | `PH7_PRIVATE void PH7_LibxmlCaptureEnd(ph7_vm *pVm,sxu32 nMark,const char *zFnName)` |
|    ! 0 |  209 | `{` |
|    ! 0 |  210 | `	xmlSetStructuredErrorFunc(0,0);` |
|    ! 0 |  211 | `	if( pVm->bLibxmlInternalErr ){` |
|      - |  212 | `		/* Internal capture on: entries stay queued for libxml_get_errors() */` |
|    ! 0 |  213 | `		return;` |
|      - |  214 | `	}` |
|    ! 0 |  215 | `	if( SySetUsed(&pVm->aLibxmlErr) > nMark ){` |
|    ! 0 |  216 | `		phl_libxml_err *aErr = (phl_libxml_err *)SySetBasePtr(&pVm->aLibxmlErr);` |
|      - |  217 | `		sxu32 n;` |
|    ! 0 |  218 | `		for( n = nMark ; n < SySetUsed(&pVm->aLibxmlErr) ; ++n ){` |
|      - |  219 | `			SyString sFunc;` |
|      - |  220 | `			SyBlob sMsg;` |
|    ! 0 |  221 | `			sxu32 nTrim = aErr[n].sMsg.nByte;` |
|      - |  222 | `			/* php trims the trailing newline off the warning copy */` |
|    ! 0 |  223 | `			while( nTrim > 0 && (aErr[n].sMsg.zString[nTrim-1] == '\n' \|\| aErr[n].sMsg.zString[nTrim-1] == '\r') ){` |
|    ! 0 |  224 | `				nTrim--;` |
|    ! 0 |  225 | `			}` |
|    ! 0 |  226 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|    ! 0 |  227 | `			SyBlobAppend(&sMsg,aErr[n].sMsg.zString,nTrim);` |
|    ! 0 |  228 | `			if( aErr[n].sFile.nByte > 0 ){` |
|    ! 0 |  229 | `				SyBlobFormat(&sMsg," in %z, line: %d",&aErr[n].sFile,aErr[n].iLine);` |
|    ! 0 |  230 | `			}else{` |
|    ! 0 |  231 | `				SyBlobFormat(&sMsg," in Entity, line: %d",aErr[n].iLine);` |
|      - |  232 | `			}` |
|    ! 0 |  233 | `			SyBlobAppend(&sMsg,"\0",1);` |
|    ! 0 |  234 | `			SyStringInitFromBuf(&sFunc,zFnName,SyStrlen(zFnName));` |
|    ! 0 |  235 | `			PH7_VmThrowError(&(*pVm),zFnName ? &sFunc : 0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|    ! 0 |  236 | `			SyBlobRelease(&sMsg);` |
|    ! 0 |  237 | `			LibxmlFreeErr(pVm,&aErr[n]);` |
|    ! 0 |  238 | `		}` |
|    ! 0 |  239 | `		SySetTruncate(&pVm->aLibxmlErr,nMark);` |
|    ! 0 |  240 | `	}` |
|    ! 0 |  241 | `}` |
|      - |  242 | `/*` |
|      - |  243 | ` * Allocate and register a new document shell on the per-VM registry.` |
|      - |  244 | ` * Returns NULL on allocation failure (the caller reports OOM).` |
|      - |  245 | ` */` |
|    ! 0 |  246 | `PH7_PRIVATE phl_xmldoc * PH7_LibxmlNewDoc(ph7_vm *pVm,void *pXmlDocPtr)` |
|    ! 0 |  247 | `{` |
|      - |  248 | `	phl_xmldoc *pDoc;` |
|    ! 0 |  249 | `	pDoc = (phl_xmldoc *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_xmldoc));` |
|    ! 0 |  250 | `	if( pDoc == 0 ){` |
|    ! 0 |  251 | `		return 0;` |
|      - |  252 | `	}` |
|    ! 0 |  253 | `	SyZero(pDoc,sizeof(phl_xmldoc));` |
|    ! 0 |  254 | `	SySetInit(&pDoc->aOrphans,&pVm->sAllocator,sizeof(void *));` |
|    ! 0 |  255 | `	pDoc->pDoc = pXmlDocPtr;` |
|    ! 0 |  256 | `	pDoc->pVm = &(*pVm);` |
|    ! 0 |  257 | `	pDoc->bPreserveWS = 1;  /* DOMDocument->preserveWhiteSpace default */` |
|    ! 0 |  258 | `	pDoc->bFormatOutput = 0;` |
|    ! 0 |  259 | `	pDoc->pNext = (phl_xmldoc *)pVm->pXmlDocs;` |
|    ! 0 |  260 | `	pVm->pXmlDocs = (void *)pDoc;` |
|    ! 0 |  261 | `	return pDoc;` |
|    ! 0 |  262 | `}` |
|      - |  263 |  |
|      - |  264 | `/* ===== Constants (php ext/libxml + ext/dom node types) ===== */` |
|      - |  265 |  |
|      - |  266 | `#define LIBXML_INT_CONST(FN,VALUE) \` |
|      - |  267 | `	static void FN(ph7_value *pVal,void *pUnused){ \` |
|      - |  268 | `		SXUNUSED(pUnused); \` |
|      - |  269 | `		ph7_value_int64(pVal,(ph7_int64)(VALUE)); \` |
|      - |  270 | `	}` |
|    ! 0 |  271 | `LIBXML_INT_CONST(LibxmlConst_VERSION,        LIBXML_VERSION)` |
|    ! 0 |  272 | `LIBXML_INT_CONST(LibxmlConst_NOENT,          XML_PARSE_NOENT)` |
|    ! 0 |  273 | `LIBXML_INT_CONST(LibxmlConst_DTDLOAD,        XML_PARSE_DTDLOAD)` |
|    ! 0 |  274 | `LIBXML_INT_CONST(LibxmlConst_DTDATTR,        XML_PARSE_DTDATTR)` |
|    ! 0 |  275 | `LIBXML_INT_CONST(LibxmlConst_DTDVALID,       XML_PARSE_DTDVALID)` |
|    ! 0 |  276 | `LIBXML_INT_CONST(LibxmlConst_NOERROR,        XML_PARSE_NOERROR)` |
|    ! 0 |  277 | `LIBXML_INT_CONST(LibxmlConst_NOWARNING,      XML_PARSE_NOWARNING)` |
|    ! 0 |  278 | `LIBXML_INT_CONST(LibxmlConst_NOBLANKS,       XML_PARSE_NOBLANKS)` |
|    ! 0 |  279 | `LIBXML_INT_CONST(LibxmlConst_XINCLUDE,       XML_PARSE_XINCLUDE)` |
|    ! 0 |  280 | `LIBXML_INT_CONST(LibxmlConst_NSCLEAN,        XML_PARSE_NSCLEAN)` |
|    ! 0 |  281 | `LIBXML_INT_CONST(LibxmlConst_NOCDATA,        XML_PARSE_NOCDATA)` |
|    ! 0 |  282 | `LIBXML_INT_CONST(LibxmlConst_NONET,          XML_PARSE_NONET)` |
|    ! 0 |  283 | `LIBXML_INT_CONST(LibxmlConst_PEDANTIC,       XML_PARSE_PEDANTIC)` |
|    ! 0 |  284 | `LIBXML_INT_CONST(LibxmlConst_COMPACT,        XML_PARSE_COMPACT)` |
|    ! 0 |  285 | `LIBXML_INT_CONST(LibxmlConst_PARSEHUGE,      XML_PARSE_HUGE)` |
|    ! 0 |  286 | `LIBXML_INT_CONST(LibxmlConst_BIGLINES,       XML_PARSE_BIG_LINES)` |
|    ! 0 |  287 | `LIBXML_INT_CONST(LibxmlConst_NOXMLDECL,      2)      /* XML_SAVE_NO_DECL */` |
|    ! 0 |  288 | `LIBXML_INT_CONST(LibxmlConst_NOEMPTYTAG,     4)      /* XML_SAVE_NO_EMPTY */` |
|    ! 0 |  289 | `LIBXML_INT_CONST(LibxmlConst_SCHEMA_CREATE,  1)      /* XML_SCHEMA_VAL_VC_I_CREATE */` |
|    ! 0 |  290 | `LIBXML_INT_CONST(LibxmlConst_ERR_NONE,       XML_ERR_NONE)` |
|    ! 0 |  291 | `LIBXML_INT_CONST(LibxmlConst_ERR_WARNING,    XML_ERR_WARNING)` |
|    ! 0 |  292 | `LIBXML_INT_CONST(LibxmlConst_ERR_ERROR,      XML_ERR_ERROR)` |
|      3 |  293 | `LIBXML_INT_CONST(LibxmlConst_ERR_FATAL,      XML_ERR_FATAL)` |
|      - |  294 | `/* ext/dom node-type constants (values fixed by the DOM spec / libxml enums) */` |
|    ! 0 |  295 | `LIBXML_INT_CONST(LibxmlConst_ELEMENT_NODE,        XML_ELEMENT_NODE)` |
|    ! 0 |  296 | `LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_NODE,      XML_ATTRIBUTE_NODE)` |
|    ! 0 |  297 | `LIBXML_INT_CONST(LibxmlConst_TEXT_NODE,           XML_TEXT_NODE)` |
|    ! 0 |  298 | `LIBXML_INT_CONST(LibxmlConst_CDATA_SECTION_NODE,  XML_CDATA_SECTION_NODE)` |
|    ! 0 |  299 | `LIBXML_INT_CONST(LibxmlConst_ENTITY_REF_NODE,     XML_ENTITY_REF_NODE)` |
|    ! 0 |  300 | `LIBXML_INT_CONST(LibxmlConst_ENTITY_NODE,         XML_ENTITY_NODE)` |
|    ! 0 |  301 | `LIBXML_INT_CONST(LibxmlConst_PI_NODE,             XML_PI_NODE)` |
|    ! 0 |  302 | `LIBXML_INT_CONST(LibxmlConst_COMMENT_NODE,        XML_COMMENT_NODE)` |
|    ! 0 |  303 | `LIBXML_INT_CONST(LibxmlConst_DOCUMENT_NODE,       XML_DOCUMENT_NODE)` |
|    ! 0 |  304 | `LIBXML_INT_CONST(LibxmlConst_DOCUMENT_TYPE_NODE,  XML_DOCUMENT_TYPE_NODE)` |
|    ! 0 |  305 | `LIBXML_INT_CONST(LibxmlConst_DOCUMENT_FRAG_NODE,  XML_DOCUMENT_FRAG_NODE)` |
|    ! 0 |  306 | `LIBXML_INT_CONST(LibxmlConst_NOTATION_NODE,       XML_NOTATION_NODE)` |
|    ! 0 |  307 | `LIBXML_INT_CONST(LibxmlConst_HTML_DOCUMENT_NODE,  XML_HTML_DOCUMENT_NODE)` |
|    ! 0 |  308 | `LIBXML_INT_CONST(LibxmlConst_DTD_NODE,            XML_DTD_NODE)` |
|      - |  309 |  |
|    ! 0 |  310 | `static void LibxmlConst_DOTTED_VERSION(ph7_value *pVal,void *pUnused)` |
|    ! 0 |  311 | `{` |
|    ! 0 |  312 | `	SXUNUSED(pUnused);` |
|    ! 0 |  313 | `	ph7_value_string(pVal,LIBXML_DOTTED_VERSION,-1);` |
|    ! 0 |  314 | `}` |
|    ! 0 |  315 | `static void LibxmlConst_LOADED_VERSION(ph7_value *pVal,void *pUnused)` |
|    ! 0 |  316 | `{` |
|    ! 0 |  317 | `	SXUNUSED(pUnused);` |
|      - |  318 | `	/* php exposes the runtime-loaded version string here */` |
|    ! 0 |  319 | `	ph7_value_string(pVal,(const char *)xmlParserVersion,-1);` |
|    ! 0 |  320 | `}` |
|      - |  321 |  |
|   3428 |  322 | `PH7_PRIVATE void PH7_RegisterLibxmlConstants(ph7_vm *pVm)` |
|      5 |  323 | `{` |
|      - |  324 | `	static const struct {` |
|      - |  325 | `		const char *zName;` |
|      - |  326 | `		void (*xExpand)(ph7_value *,void *);` |
|      - |  327 | `	} aConst[] = {` |
|      - |  328 | `		{ "LIBXML_VERSION",        LibxmlConst_VERSION        },` |
|      - |  329 | `		{ "LIBXML_DOTTED_VERSION", LibxmlConst_DOTTED_VERSION },` |
|      - |  330 | `		{ "LIBXML_LOADED_VERSION", LibxmlConst_LOADED_VERSION },` |
|      - |  331 | `		{ "LIBXML_NOENT",          LibxmlConst_NOENT          },` |
|      - |  332 | `		{ "LIBXML_DTDLOAD",        LibxmlConst_DTDLOAD        },` |
|      - |  333 | `		{ "LIBXML_DTDATTR",        LibxmlConst_DTDATTR        },` |
|      - |  334 | `		{ "LIBXML_DTDVALID",       LibxmlConst_DTDVALID       },` |
|      - |  335 | `		{ "LIBXML_NOERROR",        LibxmlConst_NOERROR        },` |
|      - |  336 | `		{ "LIBXML_NOWARNING",      LibxmlConst_NOWARNING      },` |
|      - |  337 | `		{ "LIBXML_NOBLANKS",       LibxmlConst_NOBLANKS       },` |
|      - |  338 | `		{ "LIBXML_XINCLUDE",       LibxmlConst_XINCLUDE       },` |
|      - |  339 | `		{ "LIBXML_NSCLEAN",        LibxmlConst_NSCLEAN        },` |
|      - |  340 | `		{ "LIBXML_NOCDATA",        LibxmlConst_NOCDATA        },` |
|      - |  341 | `		{ "LIBXML_NONET",          LibxmlConst_NONET          },` |
|      - |  342 | `		{ "LIBXML_PEDANTIC",       LibxmlConst_PEDANTIC       },` |
|      - |  343 | `		{ "LIBXML_COMPACT",        LibxmlConst_COMPACT        },` |
|      - |  344 | `		{ "LIBXML_PARSEHUGE",      LibxmlConst_PARSEHUGE      },` |
|      - |  345 | `		{ "LIBXML_BIGLINES",       LibxmlConst_BIGLINES       },` |
|      - |  346 | `		{ "LIBXML_NOXMLDECL",      LibxmlConst_NOXMLDECL      },` |
|      - |  347 | `		{ "LIBXML_NOEMPTYTAG",     LibxmlConst_NOEMPTYTAG     },` |
|      - |  348 | `		{ "LIBXML_SCHEMA_CREATE",  LibxmlConst_SCHEMA_CREATE  },` |
|      - |  349 | `		{ "LIBXML_ERR_NONE",       LibxmlConst_ERR_NONE       },` |
|      - |  350 | `		{ "LIBXML_ERR_WARNING",    LibxmlConst_ERR_WARNING    },` |
|      - |  351 | `		{ "LIBXML_ERR_ERROR",      LibxmlConst_ERR_ERROR      },` |
|      - |  352 | `		{ "LIBXML_ERR_FATAL",      LibxmlConst_ERR_FATAL      },` |
|      - |  353 | `		{ "XML_ELEMENT_NODE",       LibxmlConst_ELEMENT_NODE       },` |
|      - |  354 | `		{ "XML_ATTRIBUTE_NODE",     LibxmlConst_ATTRIBUTE_NODE     },` |
|      - |  355 | `		{ "XML_TEXT_NODE",          LibxmlConst_TEXT_NODE          },` |
|      - |  356 | `		{ "XML_CDATA_SECTION_NODE", LibxmlConst_CDATA_SECTION_NODE },` |
|      - |  357 | `		{ "XML_ENTITY_REF_NODE",    LibxmlConst_ENTITY_REF_NODE    },` |
|      - |  358 | `		{ "XML_ENTITY_NODE",        LibxmlConst_ENTITY_NODE        },` |
|      - |  359 | `		{ "XML_PI_NODE",            LibxmlConst_PI_NODE            },` |
|      - |  360 | `		{ "XML_COMMENT_NODE",       LibxmlConst_COMMENT_NODE       },` |
|      - |  361 | `		{ "XML_DOCUMENT_NODE",      LibxmlConst_DOCUMENT_NODE      },` |
|      - |  362 | `		{ "XML_DOCUMENT_TYPE_NODE", LibxmlConst_DOCUMENT_TYPE_NODE },` |
|      - |  363 | `		{ "XML_DOCUMENT_FRAG_NODE", LibxmlConst_DOCUMENT_FRAG_NODE },` |
|      - |  364 | `		{ "XML_NOTATION_NODE",      LibxmlConst_NOTATION_NODE      },` |
|      - |  365 | `		{ "XML_HTML_DOCUMENT_NODE", LibxmlConst_HTML_DOCUMENT_NODE },` |
|      - |  366 | `		{ "XML_DTD_NODE",           LibxmlConst_DTD_NODE           },` |
|      - |  367 | `	};` |
|      - |  368 | `	sxu32 n;` |
| 137125 |  369 | `	for( n = 0 ; n < sizeof(aConst)/sizeof(aConst[0]) ; n++ ){` |
| 133697 |  370 | `		ph7_create_constant(&(*pVm),aConst[n].zName,aConst[n].xExpand,0);` |
|  66851 |  371 | `	}` |
|   3433 |  372 | `}` |
|      - |  373 | `/* ===== libxml_* native thunks ===== */` |
|      - |  374 |  |
|      - |  375 | `/*` |
|      - |  376 | ` * Serialize one queue entry into a PHP assoc array with the LibXMLError` |
|      - |  377 | ` * property shape; the prelude wraps it into a LibXMLError instance.` |
|      - |  378 | ` */` |
|    ! 0 |  379 | `static void LibxmlErrToArray(ph7_context *pCtx,const phl_libxml_err *pErr,ph7_value *pArray,ph7_value *pWorker)` |
|    ! 0 |  380 | `{` |
|    ! 0 |  381 | `	ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 |  382 | `	ph7_value_int(pWorker,pErr->iLevel);` |
|    ! 0 |  383 | `	ph7_array_add_strkey_elem(pArray,"level",pWorker);` |
|    ! 0 |  384 | `	ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 |  385 | `	ph7_value_int(pWorker,pErr->iCode);` |
|    ! 0 |  386 | `	ph7_array_add_strkey_elem(pArray,"code",pWorker);` |
|    ! 0 |  387 | `	ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 |  388 | `	ph7_value_int(pWorker,pErr->iColumn);` |
|    ! 0 |  389 | `	ph7_array_add_strkey_elem(pArray,"column",pWorker);` |
|    ! 0 |  390 | `	ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 |  391 | `	ph7_value_string(pWorker,pErr->sMsg.zString ? pErr->sMsg.zString : "",(int)pErr->sMsg.nByte);` |
|    ! 0 |  392 | `	ph7_array_add_strkey_elem(pArray,"message",pWorker);` |
|    ! 0 |  393 | `	ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 |  394 | `	ph7_value_string(pWorker,pErr->sFile.zString ? pErr->sFile.zString : "",(int)pErr->sFile.nByte);` |
|    ! 0 |  395 | `	ph7_array_add_strkey_elem(pArray,"file",pWorker);` |
|    ! 0 |  396 | `	ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 |  397 | `	ph7_value_int(pWorker,pErr->iLine);` |
|    ! 0 |  398 | `	ph7_array_add_strkey_elem(pArray,"line",pWorker);` |
|    ! 0 |  399 | `	SXUNUSED(pCtx);` |
|    ! 0 |  400 | `}` |
|      - |  401 | `/*` |
|      - |  402 | ` * bool __libxml_use_internal_errors(?bool $use_errors = null)` |
|      - |  403 | ` *  Flip (or just report, on null/omitted) the internal-capture flag and` |
|      - |  404 | ` *  return the PREVIOUS state -- php semantics.` |
|      - |  405 | ` */` |
|     12 |  406 | `static int vm_builtin_libxml_use_internal_errors(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  407 | `{` |
|     13 |  408 | `	ph7_vm *pVm = pCtx->pVm;` |
|     13 |  409 | `	int bPrev = pVm->bLibxmlInternalErr;` |
|     13 |  410 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|      7 |  411 | `		pVm->bLibxmlInternalErr = ph7_value_to_bool(apArg[0]) ? 1 : 0;` |
|      3 |  412 | `	}` |
|     13 |  413 | `	ph7_result_bool(pCtx,bPrev);` |
|     13 |  414 | `	return PH7_OK;` |
|      1 |  415 | `}` |
|      - |  416 | `/*` |
|      - |  417 | ` * array __libxml_get_errors_raw()` |
|      - |  418 | ` *  The queued errors as raw assoc arrays, oldest first.` |
|      - |  419 | ` */` |
|      2 |  420 | `static int vm_builtin_libxml_get_errors_raw(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  421 | `{` |
|      3 |  422 | `	ph7_vm *pVm = pCtx->pVm;` |
|      3 |  423 | `	phl_libxml_err *aErr = (phl_libxml_err *)SySetBasePtr(&pVm->aLibxmlErr);` |
|      3 |  424 | `	ph7_value *pList = ph7_context_new_array(pCtx);` |
|      3 |  425 | `	ph7_value *pWorker = ph7_context_new_scalar(pCtx);` |
|      - |  426 | `	sxu32 n;` |
|      1 |  427 | `	SXUNUSED(nArg);` |
|      1 |  428 | `	SXUNUSED(apArg);` |
|      3 |  429 | `	if( pList == 0 \|\| pWorker == 0 ){` |
|    ! 0 |  430 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 |  431 | `		ph7_result_null(pCtx);` |
|    ! 0 |  432 | `		return PH7_OK;` |
|      - |  433 | `	}` |
|      3 |  434 | `	for( n = 0 ; n < SySetUsed(&pVm->aLibxmlErr) ; ++n ){` |
|    ! 0 |  435 | `		ph7_value *pEntry = ph7_context_new_array(pCtx);` |
|    ! 0 |  436 | `		if( pEntry == 0 ){` |
|    ! 0 |  437 | `			break;` |
|      - |  438 | `		}` |
|    ! 0 |  439 | `		LibxmlErrToArray(pCtx,&aErr[n],pEntry,pWorker);` |
|    ! 0 |  440 | `		ph7_array_add_elem(pList,0,pEntry);` |
|    ! 0 |  441 | `	}` |
|      3 |  442 | `	ph7_result_value(pCtx,pList);` |
|      3 |  443 | `	return PH7_OK;` |
|      2 |  444 | `}` |
|      - |  445 | `/*` |
|      - |  446 | ` * void __libxml_clear_errors()` |
|      - |  447 | ` */` |
|      4 |  448 | `static int vm_builtin_libxml_clear_errors(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  449 | `{` |
|      2 |  450 | `	SXUNUSED(nArg);` |
|      2 |  451 | `	SXUNUSED(apArg);` |
|      5 |  452 | `	PH7_LibxmlClearErrors(pCtx->pVm);` |
|      5 |  453 | `	ph7_result_null(pCtx);` |
|      5 |  454 | `	return PH7_OK;` |
|      1 |  455 | `}` |
|      - |  456 | `/*` |
|      - |  457 | ` * array\|false __libxml_get_last_error_raw()` |
|      - |  458 | ` */` |
|      2 |  459 | `static int vm_builtin_libxml_get_last_error_raw(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  460 | `{` |
|      3 |  461 | `	ph7_vm *pVm = pCtx->pVm;` |
|      3 |  462 | `	phl_libxml_err *pLast = (phl_libxml_err *)pVm->pLibxmlLastErr;` |
|      - |  463 | `	ph7_value *pEntry;` |
|      - |  464 | `	ph7_value *pWorker;` |
|      1 |  465 | `	SXUNUSED(nArg);` |
|      1 |  466 | `	SXUNUSED(apArg);` |
|      3 |  467 | `	if( pLast == 0 ){` |
|      3 |  468 | `		ph7_result_bool(pCtx,0);` |
|      3 |  469 | `		return PH7_OK;` |
|      - |  470 | `	}` |
|    ! 0 |  471 | `	pEntry = ph7_context_new_array(pCtx);` |
|    ! 0 |  472 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|    ! 0 |  473 | `	if( pEntry == 0 \|\| pWorker == 0 ){` |
|    ! 0 |  474 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 |  475 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  476 | `		return PH7_OK;` |
|      - |  477 | `	}` |
|    ! 0 |  478 | `	LibxmlErrToArray(pCtx,pLast,pEntry,pWorker);` |
|    ! 0 |  479 | `	ph7_result_value(pCtx,pEntry);` |
|    ! 0 |  480 | `	return PH7_OK;` |
|      2 |  481 | `}` |
|      - |  482 |  |
|      - |  483 | `/*` |
|      - |  484 | ` * The libxml PHP-visible surface: the LibXMLError class plus the four` |
|      - |  485 | ` * libxml_* functions, delegating to the __libxml_* thunks above.` |
|      - |  486 | ` */` |
|      - |  487 | `static const char zLibxmlLib[] =` |
|      - |  488 | `	"class LibXMLError"` |
|      - |  489 | `	"{"` |
|      - |  490 | `	"  public $level;"` |
|      - |  491 | `	"  public $code;"` |
|      - |  492 | `	"  public $column;"` |
|      - |  493 | `	"  public $message;"` |
|      - |  494 | `	"  public $file;"` |
|      - |  495 | `	"  public $line;"` |
|      - |  496 | `	"}"` |
|      - |  497 | `	"function __phl_libxml_err_obj($a)"` |
|      - |  498 | `	"{"` |
|      - |  499 | `	"  $e = new LibXMLError;"` |
|      - |  500 | `	"  $e->level = $a['level']; $e->code = $a['code']; $e->column = $a['column'];"` |
|      - |  501 | `	"  $e->message = $a['message']; $e->file = $a['file']; $e->line = $a['line'];"` |
|      - |  502 | `	"  return $e;"` |
|      - |  503 | `	"}"` |
|      - |  504 | `	"function libxml_use_internal_errors($use_errors = null)"` |
|      - |  505 | `	"{"` |
|      - |  506 | `	"  return __libxml_use_internal_errors($use_errors);"` |
|      - |  507 | `	"}"` |
|      - |  508 | `	"function libxml_clear_errors()"` |
|      - |  509 | `	"{"` |
|      - |  510 | `	"  __libxml_clear_errors();"` |
|      - |  511 | `	"}"` |
|      - |  512 | `	"function libxml_get_errors()"` |
|      - |  513 | `	"{"` |
|      - |  514 | `	"  $out = array();"` |
|      - |  515 | `	"  foreach( __libxml_get_errors_raw() as $a ){"` |
|      - |  516 | `	"    $out[] = __phl_libxml_err_obj($a);"` |
|      - |  517 | `	"  }"` |
|      - |  518 | `	"  return $out;"` |
|      - |  519 | `	"}"` |
|      - |  520 | `	"function libxml_get_last_error()"` |
|      - |  521 | `	"{"` |
|      - |  522 | `	"  $a = __libxml_get_last_error_raw();"` |
|      - |  523 | `	"  if( $a === false ){ return false; }"` |
|      - |  524 | `	"  return __phl_libxml_err_obj($a);"` |
|      - |  525 | `	"}";` |
|      - |  526 |  |
|      - |  527 | `/*` |
|      - |  528 | ` * Install the shared libxml layer: one-time global init, the per-VM state` |
|      - |  529 | ` * the DOM/XMLWriter surfaces build on, the __libxml_* thunks and the` |
|      - |  530 | ` * PHP-visible libxml_* functions + LibXMLError class.  Called from` |
|      - |  531 | ` * PH7_VmInit inside the bCompilingBuiltin window.` |
|      - |  532 | ` */` |
|   3884 |  533 | `PH7_PRIVATE sxi32 PH7_VmInstallLibxml(ph7_vm *pVm)` |
|      5 |  534 | `{` |
|      - |  535 | `	static const struct {` |
|      - |  536 | `		const char *zName;` |
|      - |  537 | `		ProchHostFunction xFunc;` |
|      - |  538 | `	} aFunc[] = {` |
|      - |  539 | `		{ "__libxml_use_internal_errors", vm_builtin_libxml_use_internal_errors },` |
|      - |  540 | `		{ "__libxml_get_errors_raw",      vm_builtin_libxml_get_errors_raw      },` |
|      - |  541 | `		{ "__libxml_clear_errors",        vm_builtin_libxml_clear_errors        },` |
|      - |  542 | `		{ "__libxml_get_last_error_raw",  vm_builtin_libxml_get_last_error_raw  },` |
|      - |  543 | `	};` |
|      - |  544 | `	sxu32 n;` |
|   3889 |  545 | `	LibxmlGlobalInit();` |
|   3889 |  546 | `	SySetInit(&pVm->aLibxmlErr,&pVm->sAllocator,sizeof(phl_libxml_err));` |
|   3889 |  547 | `	pVm->bLibxmlInternalErr = 0;` |
|   3889 |  548 | `	pVm->pLibxmlLastErr = 0;` |
|   3889 |  549 | `	pVm->pXmlDocs = 0;` |
|   3889 |  550 | `	pVm->pXmlWriters = 0;` |
|  19425 |  551 | `	for( n = 0 ; n < sizeof(aFunc)/sizeof(aFunc[0]) ; n++ ){` |
|  15541 |  552 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
|   7773 |  553 | `	}` |
|   3889 |  554 | `	return PH7_VmEvalBuiltinChunk(&(*pVm),zLibxmlLib,sizeof(zLibxmlLib)-1);` |
|      5 |  555 | `}` |
|      - |  556 |  |
|      - |  557 | `#else` |
|      - |  558 | `/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */` |
|      - |  559 | `typedef int vm_libxml_unused;` |
|      - |  560 | `#endif /* PH7_ENABLE_LIBXML */` |
|      - |  561 |  |
