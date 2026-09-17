# src/ph7/vm_xmlwriter.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 163/220 lines (74.09%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    4 | ` */` |
|     - |    5 | `#ifdef PH7_ENABLE_LIBXML` |
|     - |    6 | `#include "ph7int.h"` |
|     - |    7 | `#include <libxml/xmlwriter.h>` |
|     - |    8 |  |
|     - |    9 | `/*` |
|     - |   10 | ` * ext/xmlwriter on libxml2: __xw_* native thunks + the XMLWriter class` |
|     - |   11 | ` * prelude.  Architecture notes live in vm_libxml.c (shared registries,` |
|     - |   12 | ` * error queue, lifetime model).` |
|     - |   13 | ` *` |
|     - |   14 | ` * An XMLWriter object holds a phl_xmlwriter resource {xmlTextWriterPtr,` |
|     - |   15 | ` * xmlBufferPtr} in $__res.  In-memory writers (openMemory) own an` |
|     - |   16 | ` * xmlBuffer; outputMemory reads it back.  Every writer is chained on the` |
|     - |   17 | ` * per-VM registry (pVm->pXmlWriters) and freed at VM reset/release since` |
|     - |   18 | ` * PH7 resources have no destructor hook.` |
|     - |   19 | ` */` |
|     - |   20 |  |
|     - |   21 | `typedef struct phl_xmlwriter phl_xmlwriter;` |
|     - |   22 | `struct phl_xmlwriter {` |
|     - |   23 | `	xmlTextWriterPtr pWriter;` |
|     - |   24 | `	xmlBufferPtr pBuf;   /* non-NULL for openMemory writers */` |
|     - |   25 | `	phl_xmlwriter *pNext;` |
|     - |   26 | `};` |
|     - |   27 |  |
|     - |   28 | `/*` |
|     - |   29 | ` * Free one writer (called from the registry sweep in vm_libxml.c via` |
|     - |   30 | ` * PH7_XmlWriterVmRelease).  The order matters: the text writer must be` |
|     - |   31 | ` * freed before its backing buffer.` |
|     - |   32 | ` */` |
|     6 |   33 | `static void XmlWriterFree(phl_xmlwriter *pXw)` |
|     1 |   34 | `{` |
|     7 |   35 | `	if( pXw->pWriter ){` |
|     7 |   36 | `		xmlFreeTextWriter(pXw->pWriter);` |
|     7 |   37 | `		pXw->pWriter = 0;` |
|     3 |   38 | `	}` |
|     7 |   39 | `	if( pXw->pBuf ){` |
|     7 |   40 | `		xmlBufferFree(pXw->pBuf);` |
|     7 |   41 | `		pXw->pBuf = 0;` |
|     3 |   42 | `	}` |
|     7 |   43 | `}` |
|     - |   44 | `/*` |
|     - |   45 | ` * Free every registered writer.  Called from PH7_LibxmlVmReset /` |
|     - |   46 | ` * PH7_LibxmlVmRelease before the allocator that holds the shells is torn` |
|     - |   47 | ` * down.` |
|     - |   48 | ` */` |
|  3390 |   49 | `PH7_PRIVATE void PH7_XmlWriterVmSweep(ph7_vm *pVm)` |
|     5 |   50 | `{` |
|  3395 |   51 | `	phl_xmlwriter *pXw = (phl_xmlwriter *)pVm->pXmlWriters;` |
|  3401 |   52 | `	while( pXw ){` |
|     7 |   53 | `		phl_xmlwriter *pNext = pXw->pNext;` |
|     7 |   54 | `		XmlWriterFree(pXw);` |
|     7 |   55 | `		SyMemBackendFree(&pVm->sAllocator,pXw);` |
|     7 |   56 | `		pXw = pNext;` |
|     1 |   57 | `	}` |
|  3395 |   58 | `	pVm->pXmlWriters = 0;` |
|  3395 |   59 | `}` |
|     - |   60 |  |
|    56 |   61 | `static phl_xmlwriter * XmlWriterArg(ph7_value *pVal)` |
|     1 |   62 | `{` |
|    57 |   63 | `	if( pVal == 0 \|\| !ph7_value_is_resource(pVal) ){` |
|   ! 0 |   64 | `		return 0;` |
|     - |   65 | `	}` |
|    57 |   66 | `	return (phl_xmlwriter *)ph7_value_to_resource(pVal);` |
|    29 |   67 | `}` |
|     - |   68 |  |
|     - |   69 | `/* res\|false __xw_open_memory() */` |
|     6 |   70 | `static int vm_builtin_xw_open_memory(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |   71 | `{` |
|     7 |   72 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |   73 | `	phl_xmlwriter *pXw;` |
|     3 |   74 | `	SXUNUSED(nArg);` |
|     3 |   75 | `	SXUNUSED(apArg);` |
|     7 |   76 | `	pXw = (phl_xmlwriter *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_xmlwriter));` |
|     7 |   77 | `	if( pXw == 0 ){` |
|   ! 0 |   78 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|   ! 0 |   79 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |   80 | `		return PH7_OK;` |
|     - |   81 | `	}` |
|     7 |   82 | `	SyZero(pXw,sizeof(phl_xmlwriter));` |
|     7 |   83 | `	pXw->pBuf = xmlBufferCreate();` |
|     7 |   84 | `	if( pXw->pBuf ){` |
|     7 |   85 | `		pXw->pWriter = xmlNewTextWriterMemory(pXw->pBuf,0);` |
|     3 |   86 | `	}` |
|     7 |   87 | `	if( pXw->pBuf == 0 \|\| pXw->pWriter == 0 ){` |
|   ! 0 |   88 | `		XmlWriterFree(pXw);` |
|   ! 0 |   89 | `		SyMemBackendFree(&pVm->sAllocator,pXw);` |
|   ! 0 |   90 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |   91 | `		return PH7_OK;` |
|     - |   92 | `	}` |
|     7 |   93 | `	pXw->pNext = (phl_xmlwriter *)pVm->pXmlWriters;` |
|     7 |   94 | `	pVm->pXmlWriters = (void *)pXw;` |
|     7 |   95 | `	ph7_result_resource(pCtx,pXw);` |
|     7 |   96 | `	return PH7_OK;` |
|     4 |   97 | `}` |
|     - |   98 | `/* bool __xw_set_indent(res,bool) */` |
|     2 |   99 | `static int vm_builtin_xw_set_indent(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  100 | `{` |
|     3 |  101 | `	phl_xmlwriter *pXw = nArg > 1 ? XmlWriterArg(apArg[0]) : 0;` |
|     3 |  102 | `	int bIndent = nArg > 1 ? ph7_value_to_bool(apArg[1]) : 0;` |
|     3 |  103 | `	int rc = -1;` |
|     3 |  104 | `	if( pXw && pXw->pWriter ){` |
|     3 |  105 | `		rc = xmlTextWriterSetIndent(pXw->pWriter,bIndent ? 1 : 0);` |
|     3 |  106 | `		if( rc >= 0 && bIndent ){` |
|     - |  107 | `			/* php's default indent string is a single space */` |
|     3 |  108 | `			xmlTextWriterSetIndentString(pXw->pWriter,(const xmlChar *)" ");` |
|     1 |  109 | `		}` |
|     1 |  110 | `	}` |
|     3 |  111 | `	ph7_result_bool(pCtx,rc >= 0);` |
|     3 |  112 | `	return PH7_OK;` |
|     1 |  113 | `}` |
|     - |  114 | `/* bool __xw_set_indent_string(res,str) */` |
|   ! 0 |  115 | `static int vm_builtin_xw_set_indent_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 |  116 | `{` |
|   ! 0 |  117 | `	phl_xmlwriter *pXw = nArg > 1 ? XmlWriterArg(apArg[0]) : 0;` |
|   ! 0 |  118 | `	const char *zStr = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|   ! 0 |  119 | `	int rc = -1;` |
|   ! 0 |  120 | `	if( pXw && pXw->pWriter ){` |
|   ! 0 |  121 | `		rc = xmlTextWriterSetIndentString(pXw->pWriter,(const xmlChar *)zStr);` |
|   ! 0 |  122 | `	}` |
|   ! 0 |  123 | `	ph7_result_bool(pCtx,rc >= 0);` |
|   ! 0 |  124 | `	return PH7_OK;` |
|   ! 0 |  125 | `}` |
|     - |  126 | `/* bool __xw_start_document(res,?version,?encoding,?standalone) */` |
|     4 |  127 | `static int vm_builtin_xw_start_document(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  128 | `{` |
|     5 |  129 | `	phl_xmlwriter *pXw = nArg > 0 ? XmlWriterArg(apArg[0]) : 0;` |
|     5 |  130 | `	const char *zVer = (nArg > 1 && !ph7_value_is_null(apArg[1])) ? ph7_value_to_string(apArg[1],0) : 0;` |
|     5 |  131 | `	const char *zEnc = (nArg > 2 && !ph7_value_is_null(apArg[2])) ? ph7_value_to_string(apArg[2],0) : 0;` |
|     5 |  132 | `	const char *zStd = (nArg > 3 && !ph7_value_is_null(apArg[3])) ? ph7_value_to_string(apArg[3],0) : 0;` |
|     5 |  133 | `	int rc = -1;` |
|     5 |  134 | `	if( pXw && pXw->pWriter ){` |
|     5 |  135 | `		rc = xmlTextWriterStartDocument(pXw->pWriter,zVer,zEnc,zStd);` |
|     2 |  136 | `	}` |
|     5 |  137 | `	ph7_result_bool(pCtx,rc >= 0);` |
|     5 |  138 | `	return PH7_OK;` |
|     1 |  139 | `}` |
|     - |  140 | `/* bool __xw_end_document(res) */` |
|     2 |  141 | `static int vm_builtin_xw_end_document(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  142 | `{` |
|     3 |  143 | `	phl_xmlwriter *pXw = nArg > 0 ? XmlWriterArg(apArg[0]) : 0;` |
|     3 |  144 | `	int rc = (pXw && pXw->pWriter) ? xmlTextWriterEndDocument(pXw->pWriter) : -1;` |
|     3 |  145 | `	ph7_result_bool(pCtx,rc >= 0);` |
|     3 |  146 | `	return PH7_OK;` |
|     1 |  147 | `}` |
|     - |  148 | `/* bool __xw_start_element(res,name) */` |
|    10 |  149 | `static int vm_builtin_xw_start_element(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  150 | `{` |
|    11 |  151 | `	phl_xmlwriter *pXw = nArg > 1 ? XmlWriterArg(apArg[0]) : 0;` |
|    11 |  152 | `	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|    11 |  153 | `	int rc = (pXw && pXw->pWriter) ? xmlTextWriterStartElement(pXw->pWriter,(const xmlChar *)zName) : -1;` |
|    11 |  154 | `	ph7_result_bool(pCtx,rc >= 0);` |
|    11 |  155 | `	return PH7_OK;` |
|     1 |  156 | `}` |
|     - |  157 | `/* bool __xw_end_element(res) / __xw_full_end_element(res) */` |
|    10 |  158 | `static int vm_builtin_xw_end_element(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  159 | `{` |
|    11 |  160 | `	phl_xmlwriter *pXw = nArg > 0 ? XmlWriterArg(apArg[0]) : 0;` |
|    11 |  161 | `	int rc = (pXw && pXw->pWriter) ? xmlTextWriterEndElement(pXw->pWriter) : -1;` |
|    11 |  162 | `	ph7_result_bool(pCtx,rc >= 0);` |
|    11 |  163 | `	return PH7_OK;` |
|     1 |  164 | `}` |
|   ! 0 |  165 | `static int vm_builtin_xw_full_end_element(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 |  166 | `{` |
|   ! 0 |  167 | `	phl_xmlwriter *pXw = nArg > 0 ? XmlWriterArg(apArg[0]) : 0;` |
|   ! 0 |  168 | `	int rc = (pXw && pXw->pWriter) ? xmlTextWriterFullEndElement(pXw->pWriter) : -1;` |
|   ! 0 |  169 | `	ph7_result_bool(pCtx,rc >= 0);` |
|   ! 0 |  170 | `	return PH7_OK;` |
|   ! 0 |  171 | `}` |
|     - |  172 | `/* bool __xw_write_attribute(res,name,value) */` |
|     8 |  173 | `static int vm_builtin_xw_write_attribute(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  174 | `{` |
|     9 |  175 | `	phl_xmlwriter *pXw = nArg > 2 ? XmlWriterArg(apArg[0]) : 0;` |
|     9 |  176 | `	const char *zName = nArg > 2 ? ph7_value_to_string(apArg[1],0) : "";` |
|     9 |  177 | `	const char *zVal = nArg > 2 ? ph7_value_to_string(apArg[2],0) : "";` |
|     9 |  178 | `	int rc = (pXw && pXw->pWriter)` |
|    12 |  179 | `		? xmlTextWriterWriteAttribute(pXw->pWriter,(const xmlChar *)zName,(const xmlChar *)zVal) : -1;` |
|     9 |  180 | `	ph7_result_bool(pCtx,rc >= 0);` |
|     9 |  181 | `	return PH7_OK;` |
|     1 |  182 | `}` |
|     - |  183 | `/* bool __xw_write_element(res,name,?content) */` |
|     6 |  184 | `static int vm_builtin_xw_write_element(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  185 | `{` |
|     7 |  186 | `	phl_xmlwriter *pXw = nArg > 1 ? XmlWriterArg(apArg[0]) : 0;` |
|     7 |  187 | `	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|     7 |  188 | `	int rc = -1;` |
|     7 |  189 | `	if( pXw && pXw->pWriter ){` |
|    10 |  190 | `		if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|     7 |  191 | `			const char *zContent = ph7_value_to_string(apArg[2],0);` |
|     7 |  192 | `			rc = xmlTextWriterWriteElement(pXw->pWriter,(const xmlChar *)zName,(const xmlChar *)zContent);` |
|     4 |  193 | `		}else{` |
|     - |  194 | `			/* Empty element: start + end so it serializes as <name/> */` |
|   ! 0 |  195 | `			rc = xmlTextWriterStartElement(pXw->pWriter,(const xmlChar *)zName);` |
|   ! 0 |  196 | `			if( rc >= 0 ){` |
|   ! 0 |  197 | `				rc = xmlTextWriterEndElement(pXw->pWriter);` |
|   ! 0 |  198 | `			}` |
|     - |  199 | `		}` |
|     3 |  200 | `	}` |
|     7 |  201 | `	ph7_result_bool(pCtx,rc >= 0);` |
|     7 |  202 | `	return PH7_OK;` |
|     1 |  203 | `}` |
|     - |  204 | `/* bool __xw_text(res,content) */` |
|     2 |  205 | `static int vm_builtin_xw_text(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  206 | `{` |
|     3 |  207 | `	phl_xmlwriter *pXw = nArg > 1 ? XmlWriterArg(apArg[0]) : 0;` |
|     3 |  208 | `	const char *zText = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|     3 |  209 | `	int rc = (pXw && pXw->pWriter) ? xmlTextWriterWriteString(pXw->pWriter,(const xmlChar *)zText) : -1;` |
|     3 |  210 | `	ph7_result_bool(pCtx,rc >= 0);` |
|     3 |  211 | `	return PH7_OK;` |
|     1 |  212 | `}` |
|     - |  213 | `/* bool __xw_write_raw(res,content) */` |
|   ! 0 |  214 | `static int vm_builtin_xw_write_raw(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 |  215 | `{` |
|   ! 0 |  216 | `	phl_xmlwriter *pXw = nArg > 1 ? XmlWriterArg(apArg[0]) : 0;` |
|   ! 0 |  217 | `	const char *zText = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|   ! 0 |  218 | `	int rc = (pXw && pXw->pWriter) ? xmlTextWriterWriteRaw(pXw->pWriter,(const xmlChar *)zText) : -1;` |
|   ! 0 |  219 | `	ph7_result_bool(pCtx,rc >= 0);` |
|   ! 0 |  220 | `	return PH7_OK;` |
|   ! 0 |  221 | `}` |
|     - |  222 | `/* bool __xw_write_cdata(res,content) */` |
|     2 |  223 | `static int vm_builtin_xw_write_cdata(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  224 | `{` |
|     3 |  225 | `	phl_xmlwriter *pXw = nArg > 1 ? XmlWriterArg(apArg[0]) : 0;` |
|     3 |  226 | `	const char *zText = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|     3 |  227 | `	int rc = (pXw && pXw->pWriter) ? xmlTextWriterWriteCDATA(pXw->pWriter,(const xmlChar *)zText) : -1;` |
|     3 |  228 | `	ph7_result_bool(pCtx,rc >= 0);` |
|     3 |  229 | `	return PH7_OK;` |
|     1 |  230 | `}` |
|     - |  231 | `/* bool __xw_write_comment(res,content) */` |
|     2 |  232 | `static int vm_builtin_xw_write_comment(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  233 | `{` |
|     3 |  234 | `	phl_xmlwriter *pXw = nArg > 1 ? XmlWriterArg(apArg[0]) : 0;` |
|     3 |  235 | `	const char *zText = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|     3 |  236 | `	int rc = (pXw && pXw->pWriter) ? xmlTextWriterWriteComment(pXw->pWriter,(const xmlChar *)zText) : -1;` |
|     3 |  237 | `	ph7_result_bool(pCtx,rc >= 0);` |
|     3 |  238 | `	return PH7_OK;` |
|     1 |  239 | `}` |
|     - |  240 | `/* string __xw_output_memory(res,flush) -- read back the in-memory buffer */` |
|     8 |  241 | `static int vm_builtin_xw_output_memory(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  242 | `{` |
|     9 |  243 | `	phl_xmlwriter *pXw = nArg > 0 ? XmlWriterArg(apArg[0]) : 0;` |
|     9 |  244 | `	int bFlush = nArg > 1 ? ph7_value_to_bool(apArg[1]) : 1;` |
|     9 |  245 | `	if( pXw == 0 \|\| pXw->pBuf == 0 ){` |
|   ! 0 |  246 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 |  247 | `		return PH7_OK;` |
|     - |  248 | `	}` |
|     - |  249 | `	/* Flush the writer into the buffer before reading (php does this) */` |
|     9 |  250 | `	if( pXw->pWriter ){` |
|     9 |  251 | `		xmlTextWriterFlush(pXw->pWriter);` |
|     4 |  252 | `	}` |
|     9 |  253 | `	ph7_result_string(pCtx,(const char *)xmlBufferContent(pXw->pBuf),(int)xmlBufferLength(pXw->pBuf));` |
|     9 |  254 | `	if( bFlush ){` |
|     7 |  255 | `		xmlBufferEmpty(pXw->pBuf);` |
|     3 |  256 | `	}` |
|     9 |  257 | `	return PH7_OK;` |
|     5 |  258 | `}` |
|     - |  259 | `/* int __xw_flush(res,empty) -- flush; returns bytes written (memory writer) */` |
|   ! 0 |  260 | `static int vm_builtin_xw_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 |  261 | `{` |
|   ! 0 |  262 | `	phl_xmlwriter *pXw = nArg > 0 ? XmlWriterArg(apArg[0]) : 0;` |
|   ! 0 |  263 | `	int bEmpty = nArg > 1 ? ph7_value_to_bool(apArg[1]) : 1;` |
|   ! 0 |  264 | `	int nOut = 0;` |
|   ! 0 |  265 | `	if( pXw && pXw->pWriter ){` |
|   ! 0 |  266 | `		nOut = xmlTextWriterFlush(pXw->pWriter);` |
|   ! 0 |  267 | `	}` |
|   ! 0 |  268 | `	if( pXw && pXw->pBuf ){` |
|     - |  269 | `		/* Memory writer: php returns the buffer as a string from flush() */` |
|   ! 0 |  270 | `		ph7_result_string(pCtx,(const char *)xmlBufferContent(pXw->pBuf),(int)xmlBufferLength(pXw->pBuf));` |
|   ! 0 |  271 | `		if( bEmpty ){` |
|   ! 0 |  272 | `			xmlBufferEmpty(pXw->pBuf);` |
|   ! 0 |  273 | `		}` |
|   ! 0 |  274 | `	}else{` |
|   ! 0 |  275 | `		ph7_result_int(pCtx,nOut);` |
|     - |  276 | `	}` |
|   ! 0 |  277 | `	return PH7_OK;` |
|   ! 0 |  278 | `}` |
|     - |  279 |  |
|     - |  280 | `/*` |
|     - |  281 | ` * The XMLWriter class: a thin prelude over the __xw_* thunks.  Only the` |
|     - |  282 | ` * memory API is exposed (openMemory) -- PHPUnit never writes to a URI/file` |
|     - |  283 | ` * via XMLWriter, and that path is a Milestone-2 addition.` |
|     - |  284 | ` */` |
|     - |  285 | `static const char zXmlWriterLib[] =` |
|     - |  286 | `	"class XMLWriter"` |
|     - |  287 | `	"{"` |
|     - |  288 | `	"  public $__res;"` |
|     - |  289 | `	"  function openMemory()"` |
|     - |  290 | `	"  {"` |
|     - |  291 | `	"    $this->__res = __xw_open_memory();"` |
|     - |  292 | `	"    return $this->__res !== false;"` |
|     - |  293 | `	"  }"` |
|     - |  294 | `	"  function setIndent($enable){ return __xw_set_indent($this->__res,(bool)$enable); }"` |
|     - |  295 | `	"  function setIndentString($indentString){ return __xw_set_indent_string($this->__res,(string)$indentString); }"` |
|     - |  296 | `	"  function startDocument($version = null,$encoding = null,$standalone = null)"` |
|     - |  297 | `	"  {"` |
|     - |  298 | `	"    return __xw_start_document($this->__res,$version,$encoding,$standalone);"` |
|     - |  299 | `	"  }"` |
|     - |  300 | `	"  function endDocument(){ return __xw_end_document($this->__res); }"` |
|     - |  301 | `	"  function startElement($name){ return __xw_start_element($this->__res,(string)$name); }"` |
|     - |  302 | `	"  function endElement(){ return __xw_end_element($this->__res); }"` |
|     - |  303 | `	"  function fullEndElement(){ return __xw_full_end_element($this->__res); }"` |
|     - |  304 | `	"  function writeAttribute($name,$value)"` |
|     - |  305 | `	"  {"` |
|     - |  306 | `	"    return __xw_write_attribute($this->__res,(string)$name,(string)$value);"` |
|     - |  307 | `	"  }"` |
|     - |  308 | `	"  function writeElement($name,$content = null)"` |
|     - |  309 | `	"  {"` |
|     - |  310 | `	"    return __xw_write_element($this->__res,(string)$name,$content);"` |
|     - |  311 | `	"  }"` |
|     - |  312 | `	"  function text($content){ return __xw_text($this->__res,(string)$content); }"` |
|     - |  313 | `	"  function writeRaw($content){ return __xw_write_raw($this->__res,(string)$content); }"` |
|     - |  314 | `	"  function writeCdata($content){ return __xw_write_cdata($this->__res,(string)$content); }"` |
|     - |  315 | `	"  function writeComment($content){ return __xw_write_comment($this->__res,(string)$content); }"` |
|     - |  316 | `	"  function outputMemory($flush = true){ return __xw_output_memory($this->__res,(bool)$flush); }"` |
|     - |  317 | `	"  function flush($empty = true){ return __xw_flush($this->__res,(bool)$empty); }"` |
|     - |  318 | `	"}";` |
|     - |  319 |  |
|     - |  320 | `/*` |
|     - |  321 | ` * Install the XMLWriter library.  Called from PH7_VmInit inside the` |
|     - |  322 | ` * bCompilingBuiltin window, after PH7_VmInstallLibxml.` |
|     - |  323 | ` */` |
|  3864 |  324 | `PH7_PRIVATE sxi32 PH7_VmInstallXmlWriter(ph7_vm *pVm)` |
|     5 |  325 | `{` |
|     - |  326 | `	static const struct {` |
|     - |  327 | `		const char *zName;` |
|     - |  328 | `		ProchHostFunction xFunc;` |
|     - |  329 | `	} aFunc[] = {` |
|     - |  330 | `		{ "__xw_open_memory",       vm_builtin_xw_open_memory       },` |
|     - |  331 | `		{ "__xw_set_indent",        vm_builtin_xw_set_indent        },` |
|     - |  332 | `		{ "__xw_set_indent_string", vm_builtin_xw_set_indent_string },` |
|     - |  333 | `		{ "__xw_start_document",    vm_builtin_xw_start_document    },` |
|     - |  334 | `		{ "__xw_end_document",      vm_builtin_xw_end_document      },` |
|     - |  335 | `		{ "__xw_start_element",     vm_builtin_xw_start_element     },` |
|     - |  336 | `		{ "__xw_end_element",       vm_builtin_xw_end_element       },` |
|     - |  337 | `		{ "__xw_full_end_element",  vm_builtin_xw_full_end_element  },` |
|     - |  338 | `		{ "__xw_write_attribute",   vm_builtin_xw_write_attribute   },` |
|     - |  339 | `		{ "__xw_write_element",     vm_builtin_xw_write_element     },` |
|     - |  340 | `		{ "__xw_text",              vm_builtin_xw_text              },` |
|     - |  341 | `		{ "__xw_write_raw",         vm_builtin_xw_write_raw         },` |
|     - |  342 | `		{ "__xw_write_cdata",       vm_builtin_xw_write_cdata       },` |
|     - |  343 | `		{ "__xw_write_comment",     vm_builtin_xw_write_comment     },` |
|     - |  344 | `		{ "__xw_output_memory",     vm_builtin_xw_output_memory     },` |
|     - |  345 | `		{ "__xw_flush",             vm_builtin_xw_flush             },` |
|     - |  346 | `	};` |
|     - |  347 | `	sxu32 n;` |
| 65693 |  348 | `	for( n = 0 ; n < sizeof(aFunc)/sizeof(aFunc[0]) ; n++ ){` |
| 61829 |  349 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
| 30917 |  350 | `	}` |
|  3869 |  351 | `	return PH7_VmEvalBuiltinChunk(&(*pVm),zXmlWriterLib,sizeof(zXmlWriterLib)-1);` |
|     5 |  352 | `}` |
|     - |  353 |  |
|     - |  354 | `#else` |
|     - |  355 | `/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */` |
|     - |  356 | `typedef int vm_xmlwriter_unused;` |
|     - |  357 | `#endif /* PH7_ENABLE_LIBXML */` |
|     - |  358 |  |
