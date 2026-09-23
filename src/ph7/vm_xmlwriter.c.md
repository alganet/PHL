# src/ph7/vm_xmlwriter.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 179/242 lines (73.97%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    4 | ` */` |
|    - |    5 | `#ifdef PH7_ENABLE_LIBXML` |
|    - |    6 | `#include "ph7int.h"` |
|    - |    7 | `#include <libxml/xmlwriter.h>` |
|    - |    8 |  |
|    - |    9 | `/*` |
|    - |   10 | ` * ext/xmlwriter on libxml2: __xw_* native thunks + the XMLWriter class` |
|    - |   11 | ` * prelude.  Architecture notes live in vm_libxml.c (shared registries,` |
|    - |   12 | ` * error queue, lifetime model).` |
|    - |   13 | ` *` |
|    - |   14 | ` * An XMLWriter object holds a phl_xmlwriter resource {xmlTextWriterPtr,` |
|    - |   15 | ` * xmlBufferPtr} in $__res.  In-memory writers (openMemory) own an` |
|    - |   16 | ` * xmlBuffer; outputMemory reads it back.  Every writer is chained on the` |
|    - |   17 | ` * per-VM registry (pVm->pXmlWriters) and freed at VM reset/release since` |
|    - |   18 | ` * PH7 resources have no destructor hook.` |
|    - |   19 | ` */` |
|    - |   20 |  |
|    - |   21 | `typedef struct phl_xmlwriter phl_xmlwriter;` |
|    - |   22 | `struct phl_xmlwriter {` |
|    - |   23 | `	xmlTextWriterPtr pWriter;` |
|    - |   24 | `	xmlBufferPtr pBuf;   /* non-NULL for openMemory writers */` |
|    - |   25 | `	phl_xmlwriter *pNext;` |
|    - |   26 | `};` |
|    - |   27 |  |
|    - |   28 | `/*` |
|    - |   29 | ` * Free one writer (called from the registry sweep in vm_libxml.c via` |
|    - |   30 | ` * PH7_XmlWriterVmRelease).  The order matters: the text writer must be` |
|    - |   31 | ` * freed before its backing buffer.` |
|    - |   32 | ` */` |
|    6 |   33 | `static void XmlWriterFree(phl_xmlwriter *pXw)` |
|    1 |   34 | `{` |
|    7 |   35 | `	if( pXw->pWriter ){` |
|    7 |   36 | `		xmlFreeTextWriter(pXw->pWriter);` |
|    7 |   37 | `		pXw->pWriter = 0;` |
|    3 |   38 | `	}` |
|    7 |   39 | `	if( pXw->pBuf ){` |
|    7 |   40 | `		xmlBufferFree(pXw->pBuf);` |
|    7 |   41 | `		pXw->pBuf = 0;` |
|    3 |   42 | `	}` |
|    7 |   43 | `}` |
|    - |   44 | `/*` |
|    - |   45 | ` * Free every registered writer.  Called from PH7_LibxmlVmReset /` |
|    - |   46 | ` * PH7_LibxmlVmRelease before the allocator that holds the shells is torn` |
|    - |   47 | ` * down.` |
|    - |   48 | ` */` |
| 4080 |   49 | `PH7_PRIVATE void PH7_XmlWriterVmSweep(ph7_vm *pVm)` |
|    5 |   50 | `{` |
| 4085 |   51 | `	phl_xmlwriter *pXw = (phl_xmlwriter *)pVm->pXmlWriters;` |
| 4091 |   52 | `	while( pXw ){` |
|    7 |   53 | `		phl_xmlwriter *pNext = pXw->pNext;` |
|    7 |   54 | `		XmlWriterFree(pXw);` |
|    7 |   55 | `		SyMemBackendFree(&pVm->sAllocator,pXw);` |
|    7 |   56 | `		pXw = pNext;` |
|    1 |   57 | `	}` |
| 4085 |   58 | `	pVm->pXmlWriters = 0;` |
| 4085 |   59 | `}` |
|    - |   60 |  |
|   56 |   61 | `static phl_xmlwriter * XmlWriterArg(ph7_value *pVal)` |
|    1 |   62 | `{` |
|   57 |   63 | `	if( pVal == 0 \|\| !ph7_value_is_resource(pVal) ){` |
|  ! 0 |   64 | `		return 0;` |
|    - |   65 | `	}` |
|   57 |   66 | `	return (phl_xmlwriter *)ph7_value_to_resource(pVal);` |
|   29 |   67 | `}` |
|    - |   68 |  |
|    - |   69 | `/*` |
|    - |   70 | ` * The writer behind $this->__res.` |
|    - |   71 | ` *` |
|    - |   72 | `` * These methods were global `__xw_verb($this->__res, ...)` thunks, so each body`` |
|    - |   73 | ` * used to take the resource as argument #0. As native methods they reach it the` |
|    - |   74 | ` * way php does -- through the receiver -- and their arguments start at #0.` |
|    - |   75 | ` */` |
|   56 |   76 | `static phl_xmlwriter * XmlWriterOf(ph7_context *pCtx)` |
|    1 |   77 | `{` |
|   57 |   78 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    - |   79 | `	SyString sAttr;` |
|    - |   80 | `	ph7_value *pRes;` |
|   57 |   81 | `	if( pThis == 0 ){` |
|  ! 0 |   82 | `		return 0;` |
|    - |   83 | `	}` |
|   57 |   84 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
|   57 |   85 | `	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|   57 |   86 | `	return XmlWriterArg(pRes);` |
|   29 |   87 | `}` |
|    - |   88 | `/* bool XMLWriter::openMemory() */` |
|    6 |   89 | `static int vm_builtin_xw_open_memory(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |   90 | `{` |
|    7 |   91 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |   92 | `	phl_xmlwriter *pXw;` |
|    3 |   93 | `	SXUNUSED(nArg);` |
|    3 |   94 | `	SXUNUSED(apArg);` |
|    7 |   95 | `	pXw = (phl_xmlwriter *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_xmlwriter));` |
|    7 |   96 | `	if( pXw == 0 ){` |
|  ! 0 |   97 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|  ! 0 |   98 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |   99 | `		return PH7_OK;` |
|    - |  100 | `	}` |
|    7 |  101 | `	SyZero(pXw,sizeof(phl_xmlwriter));` |
|    7 |  102 | `	pXw->pBuf = xmlBufferCreate();` |
|    7 |  103 | `	if( pXw->pBuf ){` |
|    7 |  104 | `		pXw->pWriter = xmlNewTextWriterMemory(pXw->pBuf,0);` |
|    3 |  105 | `	}` |
|    7 |  106 | `	if( pXw->pBuf == 0 \|\| pXw->pWriter == 0 ){` |
|  ! 0 |  107 | `		XmlWriterFree(pXw);` |
|  ! 0 |  108 | `		SyMemBackendFree(&pVm->sAllocator,pXw);` |
|  ! 0 |  109 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  110 | `		return PH7_OK;` |
|    - |  111 | `	}` |
|    7 |  112 | `	pXw->pNext = (phl_xmlwriter *)pVm->pXmlWriters;` |
|    7 |  113 | `	pVm->pXmlWriters = (void *)pXw;` |
|    - |  114 | ``	/* The prelude used to do the assignment (`$this->__res = __xw_open_memory()`)`` |
|    - |  115 | `	 * and turn the handle into the bool php returns. Both belong here now: the` |
|    - |  116 | `	 * resource is storage the class owns, and openMemory() answers a bool. */` |
|    - |  117 | `	{` |
|    7 |  118 | `		ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    - |  119 | `		SyString sAttr;` |
|    - |  120 | `		ph7_value *pRes;` |
|    7 |  121 | `		SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
|    7 |  122 | `		pRes = pThis ? PH7_ClassInstanceFetchAttr(pThis,&sAttr) : 0;` |
|    7 |  123 | `		if( pRes == 0 ){` |
|  ! 0 |  124 | `			XmlWriterFree(pXw);` |
|  ! 0 |  125 | `			ph7_result_bool(pCtx,0);` |
|  ! 0 |  126 | `			return PH7_OK;` |
|    - |  127 | `		}` |
|    7 |  128 | `		PH7_MemObjRelease(pRes);` |
|    7 |  129 | `		pRes->x.pOther = pXw;` |
|    7 |  130 | `		MemObjSetType(pRes,MEMOBJ_RES);` |
|    - |  131 | `	}` |
|    7 |  132 | `	ph7_result_bool(pCtx,1);` |
|    7 |  133 | `	return PH7_OK;` |
|    4 |  134 | `}` |
|    - |  135 | `/* bool __xw_set_indent(res,bool) */` |
|    2 |  136 | `static int vm_builtin_xw_set_indent(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  137 | `{` |
|    3 |  138 | `	phl_xmlwriter *pXw = XmlWriterOf(pCtx);` |
|    3 |  139 | `	int bIndent = nArg > 0 ? ph7_value_to_bool(apArg[0]) : 0;` |
|    3 |  140 | `	int rc = -1;` |
|    3 |  141 | `	if( pXw && pXw->pWriter ){` |
|    3 |  142 | `		rc = xmlTextWriterSetIndent(pXw->pWriter,bIndent ? 1 : 0);` |
|    3 |  143 | `		if( rc >= 0 && bIndent ){` |
|    - |  144 | `			/* php's default indent string is a single space */` |
|    3 |  145 | `			xmlTextWriterSetIndentString(pXw->pWriter,(const xmlChar *)" ");` |
|    1 |  146 | `		}` |
|    1 |  147 | `	}` |
|    3 |  148 | `	ph7_result_bool(pCtx,rc >= 0);` |
|    3 |  149 | `	return PH7_OK;` |
|    1 |  150 | `}` |
|    - |  151 | `/* bool __xw_set_indent_string(res,str) */` |
|  ! 0 |  152 | `static int vm_builtin_xw_set_indent_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|  ! 0 |  153 | `{` |
|  ! 0 |  154 | `	phl_xmlwriter *pXw = XmlWriterOf(pCtx);` |
|  ! 0 |  155 | `	const char *zStr = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|  ! 0 |  156 | `	int rc = -1;` |
|  ! 0 |  157 | `	if( pXw && pXw->pWriter ){` |
|  ! 0 |  158 | `		rc = xmlTextWriterSetIndentString(pXw->pWriter,(const xmlChar *)zStr);` |
|  ! 0 |  159 | `	}` |
|  ! 0 |  160 | `	ph7_result_bool(pCtx,rc >= 0);` |
|  ! 0 |  161 | `	return PH7_OK;` |
|  ! 0 |  162 | `}` |
|    - |  163 | `/* bool __xw_start_document(res,?version,?encoding,?standalone) */` |
|    4 |  164 | `static int vm_builtin_xw_start_document(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  165 | `{` |
|    5 |  166 | `	phl_xmlwriter *pXw = XmlWriterOf(pCtx);` |
|    5 |  167 | `	const char *zVer = (nArg > 0 && !ph7_value_is_null(apArg[0])) ? ph7_value_to_string(apArg[0],0) : 0;` |
|    5 |  168 | `	const char *zEnc = (nArg > 1 && !ph7_value_is_null(apArg[1])) ? ph7_value_to_string(apArg[1],0) : 0;` |
|    5 |  169 | `	const char *zStd = (nArg > 2 && !ph7_value_is_null(apArg[2])) ? ph7_value_to_string(apArg[2],0) : 0;` |
|    5 |  170 | `	int rc = -1;` |
|    5 |  171 | `	if( pXw && pXw->pWriter ){` |
|    5 |  172 | `		rc = xmlTextWriterStartDocument(pXw->pWriter,zVer,zEnc,zStd);` |
|    2 |  173 | `	}` |
|    5 |  174 | `	ph7_result_bool(pCtx,rc >= 0);` |
|    5 |  175 | `	return PH7_OK;` |
|    1 |  176 | `}` |
|    - |  177 | `/* bool __xw_end_document(res) */` |
|    2 |  178 | `static int vm_builtin_xw_end_document(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  179 | `{` |
|    1 |  180 | `	SXUNUSED(nArg);` |
|    1 |  181 | `	SXUNUSED(apArg);` |
|    3 |  182 | `	phl_xmlwriter *pXw = XmlWriterOf(pCtx);` |
|    3 |  183 | `	int rc = (pXw && pXw->pWriter) ? xmlTextWriterEndDocument(pXw->pWriter) : -1;` |
|    3 |  184 | `	ph7_result_bool(pCtx,rc >= 0);` |
|    3 |  185 | `	return PH7_OK;` |
|    1 |  186 | `}` |
|    - |  187 | `/* bool __xw_start_element(res,name) */` |
|   10 |  188 | `static int vm_builtin_xw_start_element(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  189 | `{` |
|   11 |  190 | `	phl_xmlwriter *pXw = XmlWriterOf(pCtx);` |
|   11 |  191 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|   11 |  192 | `	int rc = (pXw && pXw->pWriter) ? xmlTextWriterStartElement(pXw->pWriter,(const xmlChar *)zName) : -1;` |
|   11 |  193 | `	ph7_result_bool(pCtx,rc >= 0);` |
|   11 |  194 | `	return PH7_OK;` |
|    1 |  195 | `}` |
|    - |  196 | `/* bool __xw_end_element(res) / __xw_full_end_element(res) */` |
|   10 |  197 | `static int vm_builtin_xw_end_element(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  198 | `{` |
|    5 |  199 | `	SXUNUSED(nArg);` |
|    5 |  200 | `	SXUNUSED(apArg);` |
|   11 |  201 | `	phl_xmlwriter *pXw = XmlWriterOf(pCtx);` |
|   11 |  202 | `	int rc = (pXw && pXw->pWriter) ? xmlTextWriterEndElement(pXw->pWriter) : -1;` |
|   11 |  203 | `	ph7_result_bool(pCtx,rc >= 0);` |
|   11 |  204 | `	return PH7_OK;` |
|    1 |  205 | `}` |
|  ! 0 |  206 | `static int vm_builtin_xw_full_end_element(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|  ! 0 |  207 | `{` |
|  ! 0 |  208 | `	SXUNUSED(nArg);` |
|  ! 0 |  209 | `	SXUNUSED(apArg);` |
|  ! 0 |  210 | `	phl_xmlwriter *pXw = XmlWriterOf(pCtx);` |
|  ! 0 |  211 | `	int rc = (pXw && pXw->pWriter) ? xmlTextWriterFullEndElement(pXw->pWriter) : -1;` |
|  ! 0 |  212 | `	ph7_result_bool(pCtx,rc >= 0);` |
|  ! 0 |  213 | `	return PH7_OK;` |
|  ! 0 |  214 | `}` |
|    - |  215 | `/* bool __xw_write_attribute(res,name,value) */` |
|    8 |  216 | `static int vm_builtin_xw_write_attribute(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  217 | `{` |
|    9 |  218 | `	phl_xmlwriter *pXw = XmlWriterOf(pCtx);` |
|    9 |  219 | `	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[0],0) : "";` |
|    9 |  220 | `	const char *zVal = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|    9 |  221 | `	int rc = (pXw && pXw->pWriter)` |
|   12 |  222 | `		? xmlTextWriterWriteAttribute(pXw->pWriter,(const xmlChar *)zName,(const xmlChar *)zVal) : -1;` |
|    9 |  223 | `	ph7_result_bool(pCtx,rc >= 0);` |
|    9 |  224 | `	return PH7_OK;` |
|    1 |  225 | `}` |
|    - |  226 | `/* bool __xw_write_element(res,name,?content) */` |
|    6 |  227 | `static int vm_builtin_xw_write_element(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  228 | `{` |
|    7 |  229 | `	phl_xmlwriter *pXw = XmlWriterOf(pCtx);` |
|    7 |  230 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|    7 |  231 | `	int rc = -1;` |
|    7 |  232 | `	if( pXw && pXw->pWriter ){` |
|   10 |  233 | `		if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|    7 |  234 | `			const char *zContent = ph7_value_to_string(apArg[1],0);` |
|    7 |  235 | `			rc = xmlTextWriterWriteElement(pXw->pWriter,(const xmlChar *)zName,(const xmlChar *)zContent);` |
|    4 |  236 | `		}else{` |
|    - |  237 | `			/* Empty element: start + end so it serializes as <name/> */` |
|  ! 0 |  238 | `			rc = xmlTextWriterStartElement(pXw->pWriter,(const xmlChar *)zName);` |
|  ! 0 |  239 | `			if( rc >= 0 ){` |
|  ! 0 |  240 | `				rc = xmlTextWriterEndElement(pXw->pWriter);` |
|  ! 0 |  241 | `			}` |
|    - |  242 | `		}` |
|    3 |  243 | `	}` |
|    7 |  244 | `	ph7_result_bool(pCtx,rc >= 0);` |
|    7 |  245 | `	return PH7_OK;` |
|    1 |  246 | `}` |
|    - |  247 | `/* bool __xw_text(res,content) */` |
|    2 |  248 | `static int vm_builtin_xw_text(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  249 | `{` |
|    3 |  250 | `	phl_xmlwriter *pXw = XmlWriterOf(pCtx);` |
|    3 |  251 | `	const char *zText = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|    3 |  252 | `	int rc = (pXw && pXw->pWriter) ? xmlTextWriterWriteString(pXw->pWriter,(const xmlChar *)zText) : -1;` |
|    3 |  253 | `	ph7_result_bool(pCtx,rc >= 0);` |
|    3 |  254 | `	return PH7_OK;` |
|    1 |  255 | `}` |
|    - |  256 | `/* bool __xw_write_raw(res,content) */` |
|  ! 0 |  257 | `static int vm_builtin_xw_write_raw(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|  ! 0 |  258 | `{` |
|  ! 0 |  259 | `	phl_xmlwriter *pXw = XmlWriterOf(pCtx);` |
|  ! 0 |  260 | `	const char *zText = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|  ! 0 |  261 | `	int rc = (pXw && pXw->pWriter) ? xmlTextWriterWriteRaw(pXw->pWriter,(const xmlChar *)zText) : -1;` |
|  ! 0 |  262 | `	ph7_result_bool(pCtx,rc >= 0);` |
|  ! 0 |  263 | `	return PH7_OK;` |
|  ! 0 |  264 | `}` |
|    - |  265 | `/* bool __xw_write_cdata(res,content) */` |
|    2 |  266 | `static int vm_builtin_xw_write_cdata(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  267 | `{` |
|    3 |  268 | `	phl_xmlwriter *pXw = XmlWriterOf(pCtx);` |
|    3 |  269 | `	const char *zText = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|    3 |  270 | `	int rc = (pXw && pXw->pWriter) ? xmlTextWriterWriteCDATA(pXw->pWriter,(const xmlChar *)zText) : -1;` |
|    3 |  271 | `	ph7_result_bool(pCtx,rc >= 0);` |
|    3 |  272 | `	return PH7_OK;` |
|    1 |  273 | `}` |
|    - |  274 | `/* bool __xw_write_comment(res,content) */` |
|    2 |  275 | `static int vm_builtin_xw_write_comment(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  276 | `{` |
|    3 |  277 | `	phl_xmlwriter *pXw = XmlWriterOf(pCtx);` |
|    3 |  278 | `	const char *zText = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|    3 |  279 | `	int rc = (pXw && pXw->pWriter) ? xmlTextWriterWriteComment(pXw->pWriter,(const xmlChar *)zText) : -1;` |
|    3 |  280 | `	ph7_result_bool(pCtx,rc >= 0);` |
|    3 |  281 | `	return PH7_OK;` |
|    1 |  282 | `}` |
|    - |  283 | `/* string __xw_output_memory(res,flush) -- read back the in-memory buffer */` |
|    8 |  284 | `static int vm_builtin_xw_output_memory(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  285 | `{` |
|    9 |  286 | `	phl_xmlwriter *pXw = XmlWriterOf(pCtx);` |
|    9 |  287 | `	int bFlush = nArg > 0 ? ph7_value_to_bool(apArg[0]) : 1;` |
|    9 |  288 | `	if( pXw == 0 \|\| pXw->pBuf == 0 ){` |
|  ! 0 |  289 | `		ph7_result_string(pCtx,"",0);` |
|  ! 0 |  290 | `		return PH7_OK;` |
|    - |  291 | `	}` |
|    - |  292 | `	/* Flush the writer into the buffer before reading (php does this) */` |
|    9 |  293 | `	if( pXw->pWriter ){` |
|    9 |  294 | `		xmlTextWriterFlush(pXw->pWriter);` |
|    4 |  295 | `	}` |
|    9 |  296 | `	ph7_result_string(pCtx,(const char *)xmlBufferContent(pXw->pBuf),(int)xmlBufferLength(pXw->pBuf));` |
|    9 |  297 | `	if( bFlush ){` |
|    7 |  298 | `		xmlBufferEmpty(pXw->pBuf);` |
|    3 |  299 | `	}` |
|    9 |  300 | `	return PH7_OK;` |
|    5 |  301 | `}` |
|    - |  302 | `/* int __xw_flush(res,empty) -- flush; returns bytes written (memory writer) */` |
|  ! 0 |  303 | `static int vm_builtin_xw_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|  ! 0 |  304 | `{` |
|  ! 0 |  305 | `	phl_xmlwriter *pXw = XmlWriterOf(pCtx);` |
|  ! 0 |  306 | `	int bEmpty = nArg > 0 ? ph7_value_to_bool(apArg[0]) : 1;` |
|  ! 0 |  307 | `	int nOut = 0;` |
|  ! 0 |  308 | `	if( pXw && pXw->pWriter ){` |
|  ! 0 |  309 | `		nOut = xmlTextWriterFlush(pXw->pWriter);` |
|  ! 0 |  310 | `	}` |
|  ! 0 |  311 | `	if( pXw && pXw->pBuf ){` |
|    - |  312 | `		/* Memory writer: php returns the buffer as a string from flush() */` |
|  ! 0 |  313 | `		ph7_result_string(pCtx,(const char *)xmlBufferContent(pXw->pBuf),(int)xmlBufferLength(pXw->pBuf));` |
|  ! 0 |  314 | `		if( bEmpty ){` |
|  ! 0 |  315 | `			xmlBufferEmpty(pXw->pBuf);` |
|  ! 0 |  316 | `		}` |
|  ! 0 |  317 | `	}else{` |
|  ! 0 |  318 | `		ph7_result_int(pCtx,nOut);` |
|    - |  319 | `	}` |
|  ! 0 |  320 | `	return PH7_OK;` |
|  ! 0 |  321 | `}` |
|    - |  322 |  |
|    - |  323 | `/*` |
|    - |  324 | ` * The XMLWriter class: a thin prelude over the __xw_* thunks.  Only the` |
|    - |  325 | ` * memory API is exposed (openMemory) -- PHPUnit never writes to a URI/file` |
|    - |  326 | ` * via XMLWriter, and that path is a Milestone-2 addition.` |
|    - |  327 | ` */` |
|    - |  328 | `/* XMLWriter is declared entirely from C by PH7_VmInstallXmlWriter below. It was` |
|    - |  329 | ` * an embedded PHP class whose every method forwarded to a global __xw_ thunk. */` |
|    - |  330 |  |
|    - |  331 | `/*` |
|    - |  332 | ` * Install the XMLWriter library.  Called from PH7_VmInit inside the` |
|    - |  333 | ` * bCompilingBuiltin window, after PH7_VmInstallLibxml.` |
|    - |  334 | ` */` |
| 4670 |  335 | `PH7_PRIVATE sxi32 PH7_VmInstallXmlWriter(ph7_vm *pVm)` |
|    5 |  336 | `{` |
|    - |  337 | `	/* php's own signatures. Declaring them is what gives these methods argument` |
|    - |  338 | `	 * coercion and a too-few/too-many ArgumentCountError; the prelude hand-cast` |
|    - |  339 | `	 * every argument ((string)$name, (bool)$enable) and enforced no arity at all. */` |
|    - |  340 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|    - |  341 | `		{ "openMemory",      PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_open_memory },` |
|    - |  342 | `		{ "setIndent",       PH7_MOD_PUBLIC, "bool $enable", "@bool", vm_builtin_xw_set_indent },` |
|    - |  343 | `		{ "setIndentString", PH7_MOD_PUBLIC, "string $indentation", "@bool", vm_builtin_xw_set_indent_string },` |
|    - |  344 | `		{ "startDocument",   PH7_MOD_PUBLIC,` |
|    - |  345 | `		  "?string $version = null, ?string $encoding = null, ?string $standalone = null",` |
|    - |  346 | `		  "@bool", vm_builtin_xw_start_document },` |
|    - |  347 | `		{ "endDocument",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_end_document },` |
|    - |  348 | `		{ "startElement",    PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_xw_start_element },` |
|    - |  349 | `		{ "endElement",      PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_end_element },` |
|    - |  350 | `		{ "fullEndElement",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_full_end_element },` |
|    - |  351 | `		{ "writeAttribute",  PH7_MOD_PUBLIC, "string $name, string $value", "@bool", vm_builtin_xw_write_attribute },` |
|    - |  352 | `		{ "writeElement",    PH7_MOD_PUBLIC, "string $name, ?string $content = null", "@bool", vm_builtin_xw_write_element },` |
|    - |  353 | `		{ "text",            PH7_MOD_PUBLIC, "string $content", "@bool", vm_builtin_xw_text },` |
|    - |  354 | `		{ "writeRaw",        PH7_MOD_PUBLIC, "string $content", "@bool", vm_builtin_xw_write_raw },` |
|    - |  355 | `		{ "writeCdata",      PH7_MOD_PUBLIC, "string $content", "@bool", vm_builtin_xw_write_cdata },` |
|    - |  356 | `		{ "writeComment",    PH7_MOD_PUBLIC, "string $content", "@bool", vm_builtin_xw_write_comment },` |
|    - |  357 | `		{ "outputMemory",    PH7_MOD_PUBLIC, "bool $flush = true", "@string", vm_builtin_xw_output_memory },` |
|    - |  358 | `		{ "flush",           PH7_MOD_PUBLIC, "bool $empty = true", "@string\|int", vm_builtin_xw_flush },` |
|    - |  359 | `	};` |
|    - |  360 | `	/* The libxml writer handle: storage the class owns, kept public because the` |
|    - |  361 | `	 * prelude declared it so. */` |
|    - |  362 | `	static const PH7_NativePropDef aProp[] = {` |
|    - |  363 | `		{ "__res", PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|    - |  364 | `	};` |
|    - |  365 | `	static const PH7_NativeClassSpec sSpec = {` |
|    - |  366 | `		"XMLWriter", 0, 0, 0,` |
|    - |  367 | `		aMethod, SX_ARRAYSIZE(aMethod),` |
|    - |  368 | `		0, 0,` |
|    - |  369 | `		aProp, SX_ARRAYSIZE(aProp),` |
|    - |  370 | `		0, 0, 0` |
|    - |  371 | `	};` |
| 4675 |  372 | `	return PH7_InstallNativeClasses(&(*pVm),&sSpec,1);` |
|    5 |  373 | `}` |
|    - |  374 |  |
|    - |  375 | `#else` |
|    - |  376 | `/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */` |
|    - |  377 | `typedef int vm_xmlwriter_unused;` |
|    - |  378 | `#endif /* PH7_ENABLE_LIBXML */` |
|    - |  379 |  |
