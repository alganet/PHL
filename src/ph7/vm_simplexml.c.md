# src/ph7/vm_simplexml.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1430/1647 lines (86.82%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    4 | ` */` |
|     - |    5 | `#ifdef PH7_ENABLE_LIBXML` |
|     - |    6 | `#include "ph7int.h"` |
|     - |    7 | `#include <libxml/parser.h>` |
|     - |    8 | `#include <libxml/tree.h>` |
|     - |    9 | `#include <libxml/xpath.h>` |
|     - |   10 | `#include <libxml/xpathInternals.h>` |
|     - |   11 | `#include <libxml/entities.h>` |
|     - |   12 |  |
|     - |   13 | `/*` |
|     - |   14 | ` * ext/simplexml on libxml2: SimpleXMLElement, SimpleXMLIterator and the four` |
|     - |   15 | ` * functions that make and unmake them.` |
|     - |   16 | ` *` |
|     - |   17 | ` * WHAT AN INSTANCE IS.  php's SimpleXMLElement is not a node wrapper -- it is a` |
|     - |   18 | ` * NODE plus a QUESTION about it, and the question is what every one of its` |
|     - |   19 | ` * surfaces answers.  The object carries` |
|     - |   20 | ` *` |
|     - |   21 | ` *   $__res  the {document shell, xmlNode} handle (the same phl_domnode ext/dom` |
|     - |   22 | ` *           uses, so the two extensions share one tree and one lifetime);` |
|     - |   23 | `` *   $__it   which of php's four `iter.type`s this object IS;`` |
|     - |   24 | ` *   $__nm   the element/attribute NAME the question names, when it names one;` |
|     - |   25 | ` *   $__ns   a namespace filter, and $__isp whether it is a PREFIX or a URI.` |
|     - |   26 | ` *` |
|     - |   27 | ` * The four questions, php's SXE_ITER_* :` |
|     - |   28 | ` *` |
|     - |   29 | `` *   NONE      "this node".            `simplexml_load_string()`, `$set[0]`.`` |
|     - |   30 | `` *   ELEMENT   "the children of this node named $__nm".    `$x->kid`.`` |
|     - |   31 | `` *   CHILD     "the children of this node".                `$x->children()`.`` |
|     - |   32 | ` *   ATTRLIST  "the attributes of this node" (named by $__nm when an xpath` |
|     - |   33 | `` *             `//@a` produced it).                        `$x->attributes()`.`` |
|     - |   34 | ` *` |
|     - |   35 | `` * So `$x->kid` does not FIND anything: it hands back an object standing for a`` |
|     - |   36 | `` * SET that may be empty, which is why `$x->nothing` is an object and not null`` |
|     - |   37 | `` * while `isset($x->nothing)` is false, and why `count($x->kid)` is the number`` |
|     - |   38 | ` * of matches.  Every derived object is FRESH -- php caches no identity here,` |
|     - |   39 | `` * and `$x->kid === $x->kid` is false under both engines.`` |
|     - |   40 | ` *` |
|     - |   41 | ` * WHAT IT SHOWS.  var_dump/print_r/(array)/var_export/json_encode/` |
|     - |   42 | ` * get_object_vars/__debugInfo all print php's get_properties table, which is` |
|     - |   43 | ` * NOT the object's slots and is derived in SxeTable() below -- see the rules` |
|     - |   44 | ` * stated there, every one of them read off php 8.5.9 rather than its source.` |
|     - |   45 | ` *` |
|     - |   46 | ` * WHAT IT IS NOT.  SimpleXMLElement declares no property, so every slot above` |
|     - |   47 | ` * is PH7_MOD_HIDDEN; it declares neither ArrayAccess nor JsonSerializable, so` |
|     - |   48 | `` * `$x['a']` is a dimension HANDLER (ph7_class::xDim) and `json_encode()` reads`` |
|     - |   49 | ` * the table; and it is Stringable, Countable and a RecursiveIterator, which it` |
|     - |   50 | ` * satisfies with real declared methods rather than an engine vtable.` |
|     - |   51 | ` */` |
|     - |   52 |  |
|     - |   53 | `/* One native method body, with the receiver's state already reachable. */` |
|     - |   54 | `#define SXE_METHOD(NAME) static int NAME(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     - |   55 |  |
|     - |   56 | `/* The hidden slots. */` |
|     - |   57 | `#define SXE_RES  "__res"` |
|     - |   58 | `#define SXE_IT   "__it"` |
|     - |   59 | `#define SXE_NM   "__nm"` |
|     - |   60 | `#define SXE_NS   "__ns"` |
|     - |   61 | `#define SXE_ISP  "__isp"` |
|     - |   62 | `#define SXE_CUR  "__cur"` |
|     - |   63 | `#define SXE_XPNS "__xpns"` |
|     - |   64 |  |
|     - |   65 | `/* php's iter.type. */` |
|     - |   66 | `#define SXE_ITER_NONE     0` |
|     - |   67 | `#define SXE_ITER_ELEMENT  1` |
|     - |   68 | `#define SXE_ITER_CHILD    2` |
|     - |   69 | `#define SXE_ITER_ATTRLIST 3` |
|     - |   70 |  |
|     - |   71 | `/* ===== The receiver's state ===== */` |
|     - |   72 |  |
|  1866 |   73 | `static phl_domnode * SxeResOf(ph7_class_instance *pObj)` |
|     4 |   74 | `{` |
|  1870 |   75 | `	ph7_value *pVal = pObj ? PH7_NativeAttr(pObj,SXE_RES) : 0;` |
|  1870 |   76 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_RES) == 0 ){` |
|    10 |   77 | `		return 0;` |
|     - |   78 | `	}` |
|  1862 |   79 | `	return (phl_domnode *)pVal->x.pOther;` |
|   937 |   80 | `}` |
|  1412 |   81 | `static xmlNodePtr SxeNodeOf(ph7_class_instance *pObj)` |
|     4 |   82 | `{` |
|  1416 |   83 | `	phl_domnode *pNd = SxeResOf(pObj);` |
|  1416 |   84 | `	return pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     4 |   85 | `}` |
|  1886 |   86 | `static int SxeTypeOf(ph7_class_instance *pObj)` |
|     4 |   87 | `{` |
|  1890 |   88 | `	return pObj ? (int)PH7_NativeAttrInt(pObj,SXE_IT) : SXE_ITER_NONE;` |
|     4 |   89 | `}` |
|     - |   90 | `/*` |
|     - |   91 | ` * A string slot that may be UNSET, which is not the same as empty: php's` |
|     - |   92 | `` * `iter.name` and `iter.nsprefix` are pointers, and a NULL nsprefix means "no`` |
|     - |   93 | ` * filter" where an empty one would mean "the empty namespace".  Answers 0 for` |
|     - |   94 | ` * an unset slot -- the length sentinel PH7_NativeAttrStr cannot give.` |
|     - |   95 | ` */` |
|  1544 |   96 | `static const char * SxeSlotStr(ph7_class_instance *pObj,const char *zSlot,int *pnLen)` |
|     4 |   97 | `{` |
|  1548 |   98 | `	ph7_value *pVal = pObj ? PH7_NativeAttr(pObj,zSlot) : 0;` |
|  1548 |   99 | `	if( pnLen ){` |
|  1548 |  100 | `		*pnLen = 0;` |
|   772 |  101 | `	}` |
|  1548 |  102 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|  1050 |  103 | `		return 0;` |
|     - |  104 | `	}` |
|   501 |  105 | `	if( pnLen ){` |
|   501 |  106 | `		*pnLen = (int)SyBlobLength(&pVal->sBlob);` |
|   249 |  107 | `	}` |
|   501 |  108 | `	return (const char *)SyBlobData(&pVal->sBlob);` |
|   776 |  109 | `}` |
|   414 |  110 | `static const char * SxeIterName(ph7_class_instance *pObj,int *pnLen)` |
|     3 |  111 | `{` |
|   417 |  112 | `	return SxeSlotStr(pObj,SXE_NM,pnLen);` |
|     3 |  113 | `}` |
|  1130 |  114 | `static const char * SxeNsFilter(ph7_class_instance *pObj,int *pnLen)` |
|     4 |  115 | `{` |
|  1134 |  116 | `	return SxeSlotStr(pObj,SXE_NS,pnLen);` |
|     4 |  117 | `}` |
|  1130 |  118 | `static int SxeIsPrefix(ph7_class_instance *pObj)` |
|     4 |  119 | `{` |
|  1134 |  120 | `	return pObj ? (int)PH7_NativeAttrInt(pObj,SXE_ISP) : 0;` |
|     4 |  121 | `}` |
|     - |  122 | `/* The receiver of a native method, when it really is one of ours. */` |
|   942 |  123 | `static ph7_class_instance * SxeThis(ph7_context *pCtx)` |
|     4 |  124 | `{` |
|   946 |  125 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   946 |  126 | `	return pThis && PH7_NativeAttr(pThis,SXE_RES) ? pThis : 0;` |
|     4 |  127 | `}` |
|     - |  128 |  |
|     - |  129 | `/*` |
|     - |  130 | `` * php's `match_ns`, the screen every walk here runs a node past.`` |
|     - |  131 | ` *` |
|     - |  132 | ` * A NULL filter is not "match everything": it is "match what carries no` |
|     - |  133 | `` * PREFIX", which is why `$x->children()` on a document with a default`` |
|     - |  134 | ` * namespace still yields the default-namespace children and skips the prefixed` |
|     - |  135 | ` * ones.  A filter compares against the href, or against the prefix when the` |
|     - |  136 | `` * caller said `$isPrefix`.  An EMPTY filter is php's "none at all" -- the`` |
|     - |  137 | `` * default `''` its two load functions carry, which must not mean "the empty`` |
|     - |  138 | ` * namespace".` |
|     - |  139 | ` */` |
|   836 |  140 | `static int SxeMatchNs(xmlNodePtr pNode,const char *zNs,int nNs,int bPrefix)` |
|     4 |  141 | `{` |
|     - |  142 | `	const xmlChar *zHave;` |
|   840 |  143 | `	if( pNode == 0 ){` |
|   ! 0 |  144 | `		return 0;` |
|     - |  145 | `	}` |
|   840 |  146 | `	if( zNs == 0 \|\| nNs < 1 ){` |
|   740 |  147 | `		return pNode->ns == 0 \|\| pNode->ns->prefix == 0;` |
|     - |  148 | `	}` |
|   102 |  149 | `	if( pNode->ns == 0 ){` |
|    18 |  150 | `		return 0;` |
|     - |  151 | `	}` |
|    86 |  152 | `	zHave = bPrefix ? pNode->ns->prefix : pNode->ns->href;` |
|    86 |  153 | `	if( zHave == 0 ){` |
|    23 |  154 | `		return 0;` |
|     - |  155 | `	}` |
|    64 |  156 | `	return (int)xmlStrlen(zHave) == nNs && SyMemcmp(zHave,zNs,(sxu32)nNs) == 0;` |
|   422 |  157 | `}` |
|     - |  158 | `/* ...and the receiver's own filter, applied to one node. */` |
|   836 |  159 | `static int SxeMatch(ph7_class_instance *pObj,xmlNodePtr pNode)` |
|     4 |  160 | `{` |
|   840 |  161 | `	int nNs = 0;` |
|   840 |  162 | `	const char *zNs = SxeNsFilter(pObj,&nNs);` |
|   840 |  163 | `	return SxeMatchNs(pNode,zNs,nNs,SxeIsPrefix(pObj));` |
|     4 |  164 | `}` |
|     - |  165 | `/* Is this the name the receiver's question names? An unnamed question takes` |
|     - |  166 | ` * every name. */` |
|   402 |  167 | `static int SxeNameMatch(ph7_class_instance *pObj,xmlNodePtr pNode)` |
|     3 |  168 | `{` |
|   405 |  169 | `	int nNm = 0;` |
|   405 |  170 | `	const char *zNm = SxeIterName(pObj,&nNm);` |
|   405 |  171 | `	if( zNm == 0 ){` |
|    42 |  172 | `		return 1;` |
|     - |  173 | `	}` |
|   365 |  174 | `	if( pNode->name == 0 ){` |
|   ! 0 |  175 | `		return 0;` |
|     - |  176 | `	}` |
|   500 |  177 | `	return (int)xmlStrlen(pNode->name) == nNm` |
|   362 |  178 | `		&& SyMemcmp(pNode->name,zNm,(sxu32)nNm) == 0;` |
|   204 |  179 | `}` |
|     - |  180 |  |
|     - |  181 | `/*` |
|     - |  182 | `` * php's `php_sxe_get_first_node`: the node this object's question STARTS at,`` |
|     - |  183 | ` * which for three of the four types is not the node it holds.` |
|     - |  184 | ` *` |
|     - |  185 | ` *   NONE      the node itself (even when it is an attribute).` |
|     - |  186 | ` *   ELEMENT   the first child element with the question's name and namespace.` |
|     - |  187 | ` *   CHILD     the first child ELEMENT the namespace filter takes -- a text` |
|     - |  188 | `` *             node is never one, which is why `(string)$x->children()` on a`` |
|     - |  189 | ` *             text-only element is the empty string.` |
|     - |  190 | ` *   ATTRLIST  the first attribute the filter (and the name, when the question` |
|     - |  191 | ` *             carries one) takes.` |
|     - |  192 | ` */` |
|   374 |  193 | `static xmlNodePtr SxeFirstNode(ph7_class_instance *pObj)` |
|     3 |  194 | `{` |
|   377 |  195 | `	xmlNodePtr pNode = SxeNodeOf(pObj);` |
|   377 |  196 | `	int iType = SxeTypeOf(pObj);` |
|     - |  197 | `	xmlNodePtr pWalk;` |
|   377 |  198 | `	if( pNode == 0 \|\| iType == SXE_ITER_NONE ){` |
|    90 |  199 | `		return pNode;` |
|     - |  200 | `	}` |
|   289 |  201 | `	if( iType == SXE_ITER_ATTRLIST ){` |
|    74 |  202 | `		for( pWalk = (xmlNodePtr)pNode->properties ; pWalk ; pWalk = pWalk->next ){` |
|    66 |  203 | `			if( SxeMatch(pObj,pWalk) && SxeNameMatch(pObj,pWalk) ){` |
|    38 |  204 | `				return pWalk;` |
|     - |  205 | `			}` |
|    16 |  206 | `		}` |
|     9 |  207 | `		return 0;` |
|     - |  208 | `	}` |
|   527 |  209 | `	for( pWalk = pNode->children ; pWalk ; pWalk = pWalk->next ){` |
|   455 |  210 | `		if( pWalk->type != XML_ELEMENT_NODE \|\| !SxeMatch(pObj,pWalk) ){` |
|   106 |  211 | `			continue;` |
|     - |  212 | `		}` |
|   351 |  213 | `		if( iType == SXE_ITER_ELEMENT && !SxeNameMatch(pObj,pWalk) ){` |
|   181 |  214 | `			continue;` |
|     - |  215 | `		}` |
|   173 |  216 | `		return pWalk;` |
|   ! 0 |  217 | `	}` |
|    75 |  218 | `	return 0;` |
|   190 |  219 | `}` |
|     - |  220 | `/* The node AFTER pCur in the same walk -- the iterator's step, and the one` |
|     - |  221 | `` * `use_iter` in the table below takes. */`` |
|   106 |  222 | `static xmlNodePtr SxeNextNode(ph7_class_instance *pObj,xmlNodePtr pCur)` |
|     2 |  223 | `{` |
|   108 |  224 | `	int iType = SxeTypeOf(pObj);` |
|     - |  225 | `	xmlNodePtr pWalk;` |
|   108 |  226 | `	if( pCur == 0 \|\| iType == SXE_ITER_NONE ){` |
|    12 |  227 | `		return 0;` |
|     - |  228 | `	}` |
|   146 |  229 | `	for( pWalk = pCur->next ; pWalk ; pWalk = pWalk->next ){` |
|    90 |  230 | `		if( iType == SXE_ITER_ATTRLIST ){` |
|     9 |  231 | `			if( SxeMatch(pObj,pWalk) && SxeNameMatch(pObj,pWalk) ){` |
|     5 |  232 | `				return pWalk;` |
|     - |  233 | `			}` |
|     5 |  234 | `			continue;` |
|     - |  235 | `		}` |
|    82 |  236 | `		if( pWalk->type != XML_ELEMENT_NODE \|\| !SxeMatch(pObj,pWalk) ){` |
|    31 |  237 | `			continue;` |
|     - |  238 | `		}` |
|    52 |  239 | `		if( iType == SXE_ITER_ELEMENT && !SxeNameMatch(pObj,pWalk) ){` |
|    16 |  240 | `			continue;` |
|     - |  241 | `		}` |
|    38 |  242 | `		return pWalk;` |
|   ! 0 |  243 | `	}` |
|    58 |  244 | `	return 0;` |
|    55 |  245 | `}` |
|     - |  246 |  |
|     - |  247 | `/*` |
|     - |  248 | `` * The ITERATOR's walk, which is not php's `get_first_node` walk for the plain`` |
|     - |  249 | `` * question: `foreach ($x as ...)` over "this node" yields its element CHILDREN,`` |
|     - |  250 | `` * where `(string)$x` and `$x[0]` are about the node itself. php keeps the two`` |
|     - |  251 | ` * apart (php_sxe_reset_iterator starts at node->children for SXE_ITER_NONE,` |
|     - |  252 | ` * php_sxe_get_first_node answers the node), and so does this: everything a` |
|     - |  253 | ` * foreach, count() and the RecursiveIterator pair see comes through here.` |
|     - |  254 | ` */` |
|    78 |  255 | `static xmlNodePtr SxeIterFirst(ph7_class_instance *pObj)` |
|     1 |  256 | `{` |
|     - |  257 | `	xmlNodePtr pNode,pWalk;` |
|    79 |  258 | `	if( SxeTypeOf(pObj) != SXE_ITER_NONE ){` |
|    61 |  259 | `		return SxeFirstNode(pObj);` |
|     - |  260 | `	}` |
|    19 |  261 | `	pNode = SxeNodeOf(pObj);` |
|    19 |  262 | `	if( pNode == 0 \|\| pNode->type == XML_ATTRIBUTE_NODE ){` |
|     5 |  263 | `		return 0;` |
|     - |  264 | `	}` |
|    19 |  265 | `	for( pWalk = pNode->children ; pWalk ; pWalk = pWalk->next ){` |
|    15 |  266 | `		if( pWalk->type == XML_ELEMENT_NODE && SxeMatch(pObj,pWalk) ){` |
|    11 |  267 | `			return pWalk;` |
|     - |  268 | `		}` |
|     3 |  269 | `	}` |
|     5 |  270 | `	return 0;` |
|    40 |  271 | `}` |
|    94 |  272 | `static xmlNodePtr SxeIterNext(ph7_class_instance *pObj,xmlNodePtr pCur)` |
|     1 |  273 | `{` |
|     - |  274 | `	xmlNodePtr pWalk;` |
|    95 |  275 | `	if( pCur == 0 ){` |
|   ! 0 |  276 | `		return 0;` |
|     - |  277 | `	}` |
|    95 |  278 | `	if( SxeTypeOf(pObj) != SXE_ITER_NONE ){` |
|    69 |  279 | `		return SxeNextNode(pObj,pCur);` |
|     - |  280 | `	}` |
|    35 |  281 | `	for( pWalk = pCur->next ; pWalk ; pWalk = pWalk->next ){` |
|    25 |  282 | `		if( pWalk->type == XML_ELEMENT_NODE && SxeMatch(pObj,pWalk) ){` |
|    17 |  283 | `			return pWalk;` |
|     - |  284 | `		}` |
|     5 |  285 | `	}` |
|    11 |  286 | `	return 0;` |
|    48 |  287 | `}` |
|     - |  288 |  |
|     - |  289 | `/* ===== Text ===== */` |
|     - |  290 |  |
|     - |  291 | `/*` |
|     - |  292 | `` * php's `xmlNodeListGetString(doc, node->children, 1)`: the text a node`` |
|     - |  293 | ` * CONTAINS, which walks its child list joining text and CDATA and stepping` |
|     - |  294 | `` * over everything else.  This is `(string)$x` and it is what a child's value`` |
|     - |  295 | ` * is when the table below decides it is a string.` |
|     - |  296 | ` */` |
|   254 |  297 | `static void SxeNodeText(SyBlob *pOut,xmlNodePtr pNode)` |
|     3 |  298 | `{` |
|     - |  299 | `	xmlNodePtr pWalk;` |
|   257 |  300 | `	if( pNode == 0 ){` |
|     9 |  301 | `		return;` |
|     - |  302 | `	}` |
|   246 |  303 | `	if( pNode->type == XML_COMMENT_NODE \|\| pNode->type == XML_PI_NODE` |
|   246 |  304 | `	 \|\| pNode->type == XML_TEXT_NODE \|\| pNode->type == XML_CDATA_SECTION_NODE ){` |
|     - |  305 | ``		/* These carry their text in `content` and have no child list at all: the`` |
|     - |  306 | ``		 * comment object php's table shows under `comment` stringifies to the`` |
|     - |  307 | `		 * comment's own words. */` |
|     5 |  308 | `		if( pNode->content ){` |
|     7 |  309 | `			SyBlobAppend(pOut,(const char *)pNode->content,` |
|     4 |  310 | `				(sxu32)SyStrlen((const char *)pNode->content));` |
|     2 |  311 | `		}` |
|     5 |  312 | `		return;` |
|     - |  313 | `	}` |
|   513 |  314 | `	for( pWalk = pNode->children ; pWalk ; pWalk = pWalk->next ){` |
|   271 |  315 | `		if( pWalk->type == XML_TEXT_NODE \|\| pWalk->type == XML_CDATA_SECTION_NODE ){` |
|   233 |  316 | `			if( pWalk->content ){` |
|   348 |  317 | `				SyBlobAppend(pOut,(const char *)pWalk->content,` |
|   230 |  318 | `					(sxu32)SyStrlen((const char *)pWalk->content));` |
|   118 |  319 | `			}` |
|   155 |  320 | `		}else if( pWalk->type == XML_ENTITY_REF_NODE ){` |
|     - |  321 | `			/* An unexpanded entity reference contributes its replacement text,` |
|     - |  322 | `			 * which is where libxml keeps it. */` |
|   ! 0 |  323 | `			SxeNodeText(pOut,pWalk);` |
|   ! 0 |  324 | `		}` |
|   137 |  325 | `	}` |
|   130 |  326 | `}` |
|     - |  327 | `/* A blank text node in libxml's sense -- whitespace only. php's table skips` |
|     - |  328 | ` * one where it would otherwise show the element's text. */` |
|   118 |  329 | `static int SxeBlankText(xmlNodePtr pNode)` |
|     2 |  330 | `{` |
|     - |  331 | `	const xmlChar *z;` |
|   120 |  332 | `	if( pNode == 0 \|\| pNode->content == 0 ){` |
|   ! 0 |  333 | `		return 1;` |
|     - |  334 | `	}` |
|   144 |  335 | `	for( z = pNode->content ; *z ; ++z ){` |
|   132 |  336 | `		if( *z != ' ' && *z != '\t' && *z != '\n' && *z != '\r' ){` |
|   108 |  337 | `			return 0;` |
|     - |  338 | `		}` |
|    13 |  339 | `	}` |
|    13 |  340 | `	return 1;` |
|    61 |  341 | `}` |
|     - |  342 |  |
|     - |  343 | `/* ===== Making one ===== */` |
|     - |  344 |  |
|     - |  345 | `/*` |
|     - |  346 | ` * A new object over pNode asking iType about it, of the RECEIVER's class -- a` |
|     - |  347 | ` * subclass of SimpleXMLElement propagates through every navigation, which is` |
|     - |  348 | `` * what makes `simplexml_load_string($s,'MySX')->kid` a MySX.`` |
|     - |  349 | ` *` |
|     - |  350 | `` * The namespace filter is INHERITED unless the caller states one: `$x->`` |
|     - |  351 | `` * children('urn:a')->kid` means the `urn:a` kid, because the filter rode the`` |
|     - |  352 | ` * navigation.  BORROWED-free: the caller owns the returned reference.` |
|     - |  353 | ` */` |
|   578 |  354 | `static ph7_class_instance * SxeNew(ph7_vm *pVm,ph7_class *pClass,phl_xmldoc *pShell,` |
|     - |  355 | `	xmlNodePtr pNode,int iType,const char *zName,int nName,` |
|     - |  356 | `	const char *zNs,int nNs,int bIsPrefix)` |
|     4 |  357 | `{` |
|     - |  358 | `	ph7_class_instance *pObj;` |
|     - |  359 | `	phl_domnode *pRes;` |
|     - |  360 | `	ph7_value sVal;` |
|   582 |  361 | `	if( pClass == 0 ){` |
|   ! 0 |  362 | `		return 0;` |
|     - |  363 | `	}` |
|   582 |  364 | `	pObj = PH7_NewClassInstance(pVm,pClass);` |
|   582 |  365 | `	if( pObj == 0 ){` |
|   ! 0 |  366 | `		return 0;` |
|     - |  367 | `	}` |
|   582 |  368 | `	pRes = (phl_domnode *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_domnode));` |
|   582 |  369 | `	if( pRes == 0 ){` |
|   ! 0 |  370 | `		PH7_ClassInstanceUnref(pObj);` |
|   ! 0 |  371 | `		return 0;` |
|     - |  372 | `	}` |
|   582 |  373 | `	pRes->pShell = pShell;` |
|   582 |  374 | `	pRes->pNode = pNode;` |
|   582 |  375 | `	PH7_MemObjInit(pVm,&sVal);` |
|   582 |  376 | `	sVal.x.pOther = pRes;` |
|   582 |  377 | `	sVal.iFlags = MEMOBJ_RES;` |
|   582 |  378 | `	PH7_NativeSetProp(pVm,pObj,SXE_RES,sizeof(SXE_RES)-1,&sVal);` |
|   582 |  379 | `	PH7_NativeSetAttrInt(pVm,pObj,SXE_IT,(sxi64)iType);` |
|   582 |  380 | `	if( zName ){` |
|   147 |  381 | `		PH7_NativeSetAttrStr(pVm,pObj,SXE_NM,zName,(sxu32)nName);` |
|    72 |  382 | `	}` |
|   582 |  383 | `	if( zNs && nNs > 0 ){` |
|    56 |  384 | `		PH7_NativeSetAttrStr(pVm,pObj,SXE_NS,zNs,(sxu32)nNs);` |
|    56 |  385 | `		PH7_NativeSetAttrInt(pVm,pObj,SXE_ISP,bIsPrefix ? 1 : 0);` |
|    27 |  386 | `	}` |
|   582 |  387 | `	return pObj;` |
|   293 |  388 | `}` |
|     - |  389 | `/* The same, derived from an existing object: its class, its shell and (unless` |
|     - |  390 | ` * the caller states one) its namespace filter. */` |
|   294 |  391 | `static ph7_class_instance * SxeDerive(ph7_vm *pVm,ph7_class_instance *pSrc,` |
|     - |  392 | `	xmlNodePtr pNode,int iType,const char *zName,int nName)` |
|     4 |  393 | `{` |
|   298 |  394 | `	phl_domnode *pNd = SxeResOf(pSrc);` |
|   298 |  395 | `	int nNs = 0;` |
|   298 |  396 | `	const char *zNs = SxeNsFilter(pSrc,&nNs);` |
|   298 |  397 | `	return SxeNew(pVm,pSrc->pClass,pNd ? pNd->pShell : 0,pNode,iType,zName,nName,` |
|   147 |  398 | `		zNs,nNs,SxeIsPrefix(pSrc));` |
|     4 |  399 | `}` |
|     - |  400 | `/* ...and hand it to PHP. */` |
|   284 |  401 | `static int SxeResultObj(ph7_context *pCtx,ph7_class_instance *pObj)` |
|     4 |  402 | `{` |
|   288 |  403 | `	if( pObj == 0 ){` |
|   ! 0 |  404 | `		ph7_result_null(pCtx);` |
|   ! 0 |  405 | `		return PH7_OK;` |
|     - |  406 | `	}` |
|   288 |  407 | `	PH7_NativeResultObject(pCtx,pObj);` |
|   288 |  408 | `	return PH7_OK;` |
|   146 |  409 | `}` |
|     - |  410 | `/* ...and hand back one the CALLER still owns -- the cursor object, which the` |
|     - |  411 | ` * receiver's slot keeps alive. ph7_result_value takes its own reference. */` |
|    86 |  412 | `static int SxeResultBorrowed(ph7_context *pCtx,ph7_class_instance *pObj)` |
|     1 |  413 | `{` |
|     - |  414 | `	ph7_value sRes;` |
|    87 |  415 | `	PH7_MemObjInit(pCtx->pVm,&sRes);` |
|    87 |  416 | `	sRes.x.pOther = pObj;` |
|    87 |  417 | `	sRes.iFlags = MEMOBJ_OBJ;` |
|    87 |  418 | `	ph7_result_value(pCtx,&sRes);` |
|    87 |  419 | `	return PH7_OK;` |
|     1 |  420 | `}` |
|     - |  421 |  |
|     - |  422 | `/* ===== php's get_properties table ===== */` |
|     - |  423 |  |
|     - |  424 | `/*` |
|     - |  425 | `` * The value ONE child element contributes, php's `_get_base_node_value`.`` |
|     - |  426 | ` *` |
|     - |  427 | ` * A string when the node's FIRST child is a non-blank text node, and the whole` |
|     - |  428 | `` * of the node's text then -- so `<i>a<j/>b</i>` is the string `ab`, elements`` |
|     - |  429 | ` * stepped over -- and a fresh SimpleXMLElement otherwise.  The three shapes` |
|     - |  430 | ` * that make it an object are: no children at all, a first child that is not` |
|     - |  431 | ` * text (an element, a comment, a CDATA section), and a first child that is` |
|     - |  432 | `` * blank text.  `<l>  </l>` is an object, `<l> x </l>` is the string ` x `.`` |
|     - |  433 | ` */` |
|   132 |  434 | `static void SxeBaseValue(ph7_vm *pVm,ph7_class_instance *pSrc,xmlNodePtr pNode,` |
|     - |  435 | `	ph7_value *pOut)` |
|     2 |  436 | `{` |
|   134 |  437 | `	xmlNodePtr pFirst = pNode ? pNode->children : 0;` |
|   134 |  438 | `	if( pFirst && pFirst->type == XML_TEXT_NODE && !SxeBlankText(pFirst) ){` |
|     - |  439 | `		SyBlob sText;` |
|    90 |  440 | `		SyBlobInit(&sText,&pVm->sAllocator);` |
|    90 |  441 | `		SxeNodeText(&sText,pNode);` |
|    90 |  442 | `		PH7_MemObjRelease(pOut);` |
|    90 |  443 | `		PH7_MemObjInitFromString(pVm,pOut,0);` |
|    90 |  444 | `		PH7_MemObjStringAppend(pOut,(const char *)SyBlobData(&sText),SyBlobLength(&sText));` |
|    90 |  445 | `		SyBlobRelease(&sText);` |
|    90 |  446 | `		return;` |
|     - |  447 | `	}` |
|     - |  448 | `	{` |
|    45 |  449 | `		ph7_class_instance *pObj = SxeDerive(pVm,pSrc,pNode,SXE_ITER_NONE,0,0);` |
|    45 |  450 | `		PH7_MemObjRelease(pOut);` |
|    45 |  451 | `		PH7_MemObjInit(pVm,pOut);` |
|    45 |  452 | `		if( pObj ){` |
|    45 |  453 | `			pOut->x.pOther = pObj;` |
|    45 |  454 | `			pOut->iFlags = MEMOBJ_OBJ;` |
|    22 |  455 | `		}` |
|     - |  456 | `	}` |
|    68 |  457 | `}` |
|     - |  458 | `/*` |
|     - |  459 | `` * php's `sxe_properties_add`: the first value under a name is stored plainly,`` |
|     - |  460 | ` * a second turns the entry into a list and both go in it, and every one after` |
|     - |  461 | `` * appends.  This is why `<r><c>1</c></r>` shows `c => '1'` and`` |
|     - |  462 | `` * `<r><c>1</c><c>2</c></r>` shows `c => ['1','2']`.`` |
|     - |  463 | ` */` |
|   108 |  464 | `static void SxeTableAdd(ph7_vm *pVm,ph7_value *pTable,const char *zName,int nName,` |
|     - |  465 | `	ph7_value *pVal)` |
|     2 |  466 | `{` |
|   110 |  467 | `	ph7_value *pHave = ph7_array_fetch(pTable,zName,nName);` |
|     - |  468 | `	ph7_value sKey;` |
|   110 |  469 | `	if( pHave == 0 ){` |
|    90 |  470 | `		PH7_MemObjInitFromString(pVm,&sKey,0);` |
|    90 |  471 | `		PH7_MemObjStringAppend(&sKey,zName,(sxu32)nName);` |
|    90 |  472 | `		ph7_array_add_elem(pTable,&sKey,pVal);` |
|    90 |  473 | `		PH7_MemObjRelease(&sKey);` |
|   100 |  474 | `		return;` |
|     - |  475 | `	}` |
|    21 |  476 | `	if( (pHave->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     - |  477 | `		/* Promote the single value into php's two-element list. */` |
|     - |  478 | `		ph7_value sList,sFirst;` |
|    21 |  479 | `		PH7_MemObjInit(pVm,&sFirst);` |
|    21 |  480 | `		PH7_MemObjStore(pHave,&sFirst);` |
|    21 |  481 | `		PH7_MemObjInit(pVm,&sList);` |
|    21 |  482 | `		if( PH7_MemObjToHashmap(&sList) != SXRET_OK ){` |
|   ! 0 |  483 | `			PH7_MemObjRelease(&sFirst);` |
|   ! 0 |  484 | `			return;` |
|     - |  485 | `		}` |
|    21 |  486 | `		ph7_array_add_elem(&sList,0,&sFirst);` |
|    21 |  487 | `		ph7_array_add_elem(&sList,0,pVal);` |
|    21 |  488 | `		PH7_MemObjInitFromString(pVm,&sKey,0);` |
|    21 |  489 | `		PH7_MemObjStringAppend(&sKey,zName,(sxu32)nName);` |
|    21 |  490 | `		ph7_array_add_elem(pTable,&sKey,&sList);` |
|    21 |  491 | `		PH7_MemObjRelease(&sKey);` |
|    21 |  492 | `		PH7_MemObjRelease(&sList);` |
|    21 |  493 | `		PH7_MemObjRelease(&sFirst);` |
|    21 |  494 | `		return;` |
|     - |  495 | `	}` |
|   ! 0 |  496 | `	ph7_array_add_elem(pHave,0,pVal);` |
|    56 |  497 | `}` |
|     - |  498 | `/*` |
|     - |  499 | `` * php's `sxe_get_prop_hash` -- the table every presentation surface prints,`` |
|     - |  500 | ` * derived from php 8.5.9 across a matrix of document shapes and accessors.` |
|     - |  501 | ` * The rules, in the order they run:` |
|     - |  502 | ` *` |
|     - |  503 | `` * 1. ATTRIBUTES.  Unless this is a `children()` object being asked by`` |
|     - |  504 | ` *    get_properties (a DEBUG walk asks even then, which is why var_dump of` |
|     - |  505 | `` *    `children()` shows `@attributes` and `(array)` of the same object does`` |
|     - |  506 | ` *    not), every attribute of the START node that the filter takes goes under` |
|     - |  507 | `` *    `@attributes`.  For an ELEMENT question the start node is the first`` |
|     - |  508 | `` *    MATCH, so `$x->kid` shows the first kid's attributes.`` |
|     - |  509 | ` *` |
|     - |  510 | ` * 2. An ATTRIBUTE node has no children question: it shows its own value at the` |
|     - |  511 | `` *    next index, and nothing else.  `$x['a']` is `[0 => '1']`.`` |
|     - |  512 | ` *` |
|     - |  513 | ` * 3. Otherwise the walk starts at the start node's CHILDREN and keys by name` |
|     - |  514 | ` *    -- EXCEPT for the one shape php walks the SET instead: a start node whose` |
|     - |  515 | ` *    only child is a non-blank text node.  There the walk is over the matched` |
|     - |  516 | ` *    siblings themselves and the keys are indices, which is what makes` |
|     - |  517 | `` *    `$x->c` on two text-only `<c>` show `[0=>'one', 1=>'two']` while the same`` |
|     - |  518 | `` *    two EMPTY `<c/>` show nothing at all (their start node has no children,`` |
|     - |  519 | ` *    so the child walk begins at NULL).` |
|     - |  520 | ` *` |
|     - |  521 | ` * 4. In the child walk, text and CDATA nodes are stepped over, an element that` |
|     - |  522 | ` *    fails the namespace filter is stepped over, and everything else -- an` |
|     - |  523 | `` *    element, a comment (`comment`), a processing instruction (its target) --`` |
|     - |  524 | ` *    contributes its base value under its node name.` |
|     - |  525 | ` */` |
|   168 |  526 | `static void SxeTable(ph7_vm *pVm,ph7_class_instance *pThis,int bDebug,ph7_value *pOut)` |
|     2 |  527 | `{` |
|   170 |  528 | `	int iType = SxeTypeOf(pThis);` |
|   170 |  529 | `	xmlNodePtr pNode = SxeNodeOf(pThis);` |
|     - |  530 | `	xmlNodePtr pStart,pWalk;` |
|   170 |  531 | `	int bUseIter = 0;` |
|   170 |  532 | `	if( pNode == 0 ){` |
|   ! 0 |  533 | `		return;` |
|     - |  534 | `	}` |
|   170 |  535 | `	if( bDebug \|\| iType != SXE_ITER_CHILD ){` |
|   142 |  536 | `		xmlNodePtr pAttrOn = iType == SXE_ITER_ELEMENT ? SxeFirstNode(pThis) : pNode;` |
|     - |  537 | `		ph7_value sAttrs;` |
|   142 |  538 | `		int bAny = 0;` |
|   142 |  539 | `		PH7_MemObjInit(pVm,&sAttrs);` |
|   140 |  540 | `		if( pAttrOn && pAttrOn->type != XML_ENTITY_DECL` |
|   124 |  541 | `		 && PH7_MemObjToHashmap(&sAttrs) == SXRET_OK ){` |
|     - |  542 | `			xmlAttrPtr pAttr;` |
|   152 |  543 | `			for( pAttr = pAttrOn->properties ; pAttr ; pAttr = pAttr->next ){` |
|     - |  544 | `				SyBlob sText;` |
|     - |  545 | `				ph7_value sKey,sVal;` |
|    29 |  546 | `				if( !SxeMatch(pThis,(xmlNodePtr)pAttr) ){` |
|   ! 0 |  547 | `					continue;` |
|     - |  548 | `				}` |
|    29 |  549 | `				if( iType == SXE_ITER_ATTRLIST && !SxeNameMatch(pThis,(xmlNodePtr)pAttr) ){` |
|   ! 0 |  550 | `					continue;` |
|     - |  551 | `				}` |
|    29 |  552 | `				SyBlobInit(&sText,&pVm->sAllocator);` |
|    29 |  553 | `				SxeNodeText(&sText,(xmlNodePtr)pAttr);` |
|    29 |  554 | `				PH7_MemObjInitFromString(pVm,&sKey,0);` |
|    43 |  555 | `				PH7_MemObjStringAppend(&sKey,(const char *)pAttr->name,` |
|    28 |  556 | `					(sxu32)xmlStrlen(pAttr->name));` |
|    29 |  557 | `				PH7_MemObjInitFromString(pVm,&sVal,0);` |
|    43 |  558 | `				PH7_MemObjStringAppend(&sVal,(const char *)SyBlobData(&sText),` |
|    14 |  559 | `					SyBlobLength(&sText));` |
|    29 |  560 | `				ph7_array_add_elem(&sAttrs,&sKey,&sVal);` |
|    29 |  561 | `				PH7_MemObjRelease(&sKey);` |
|    29 |  562 | `				PH7_MemObjRelease(&sVal);` |
|    29 |  563 | `				SyBlobRelease(&sText);` |
|    29 |  564 | `				bAny = 1;` |
|    15 |  565 | `			}` |
|    61 |  566 | `		}` |
|   142 |  567 | `		if( bAny ){` |
|     - |  568 | `			ph7_value sKey;` |
|    17 |  569 | `			PH7_MemObjInitFromString(pVm,&sKey,0);` |
|    17 |  570 | `			PH7_MemObjStringAppend(&sKey,"@attributes",sizeof("@attributes")-1);` |
|    17 |  571 | `			ph7_array_add_elem(pOut,&sKey,&sAttrs);` |
|    17 |  572 | `			PH7_MemObjRelease(&sKey);` |
|     8 |  573 | `		}` |
|   142 |  574 | `		PH7_MemObjRelease(&sAttrs);` |
|    70 |  575 | `	}` |
|   170 |  576 | `	if( iType == SXE_ITER_ATTRLIST ){` |
|     5 |  577 | `		return;` |
|     - |  578 | `	}` |
|   166 |  579 | `	if( iType == SXE_ITER_CHILD ){` |
|     - |  580 | ``		/* `children()` walks the node's OWN child list -- php never moves it to`` |
|     - |  581 | `		 * the first match here, which is what makes the table of a children()` |
|     - |  582 | `		 * object the same set of rows the element's own table shows. */` |
|    31 |  583 | `		pStart = SxeNodeOf(pThis);` |
|    31 |  584 | `		pWalk = pStart ? pStart->children : 0;` |
|    16 |  585 | `	}else{` |
|   136 |  586 | `		pStart = SxeFirstNode(pThis);` |
|   136 |  587 | `		if( pStart == 0 ){` |
|    19 |  588 | `			return;` |
|     - |  589 | `		}` |
|   118 |  590 | `		if( pStart->type == XML_ATTRIBUTE_NODE ){` |
|     - |  591 | `			SyBlob sText;` |
|     - |  592 | `			ph7_value sVal;` |
|     7 |  593 | `			SyBlobInit(&sText,&pVm->sAllocator);` |
|     7 |  594 | `			SxeNodeText(&sText,pStart);` |
|     7 |  595 | `			PH7_MemObjInitFromString(pVm,&sVal,0);` |
|     7 |  596 | `			PH7_MemObjStringAppend(&sVal,(const char *)SyBlobData(&sText),SyBlobLength(&sText));` |
|     7 |  597 | `			ph7_array_add_elem(pOut,0,&sVal);` |
|     7 |  598 | `			PH7_MemObjRelease(&sVal);` |
|     7 |  599 | `			SyBlobRelease(&sText);` |
|     7 |  600 | `			return;` |
|     - |  601 | `		}` |
|   110 |  602 | `		if( pStart->children && pStart->children->next == 0` |
|    76 |  603 | `		 && pStart->children->type == XML_TEXT_NODE && !SxeBlankText(pStart->children) ){` |
|    20 |  604 | `			bUseIter = 1;` |
|    20 |  605 | `			pWalk = pStart;` |
|    11 |  606 | `		}else{` |
|    94 |  607 | `			pWalk = pStart->children;` |
|     - |  608 | `		}` |
|     - |  609 | `	}` |
|   322 |  610 | `	while( pWalk ){` |
|     - |  611 | `		ph7_value sVal;` |
|   182 |  612 | `		if( pWalk->type == XML_TEXT_NODE \|\| pWalk->type == XML_CDATA_SECTION_NODE ){` |
|    45 |  613 | `			goto next_node;` |
|     - |  614 | `		}` |
|   138 |  615 | `		if( iType == SXE_ITER_CHILD && pWalk->type != XML_ELEMENT_NODE ){` |
|     - |  616 | ``			/* `children()` walks with php's ITERATOR, which yields elements and`` |
|     - |  617 | `			 * nothing else -- so a comment shows in the table of the element and` |
|     - |  618 | `			 * not in the table of its children(). */` |
|     5 |  619 | `			goto next_node;` |
|     - |  620 | `		}` |
|     - |  621 | `		/* The namespace filter screens CANDIDATE CHILDREN. It is not asked of the` |
|     - |  622 | `		 * set walk: those nodes are the question's own subjects, already matched` |
|     - |  623 | `		 * where the walk found them -- which is why an xpath result standing on a` |
|     - |  624 | `		 * PREFIXED element shows that element's text, filter or no filter. */` |
|   134 |  625 | `		if( !bUseIter && pWalk->type == XML_ELEMENT_NODE && !SxeMatch(pThis,pWalk) ){` |
|   ! 0 |  626 | `			goto next_node;` |
|     - |  627 | `		}` |
|   134 |  628 | `		if( pWalk->name == 0 ){` |
|   ! 0 |  629 | `			goto next_node;` |
|     - |  630 | `		}` |
|   134 |  631 | `		PH7_MemObjInit(pVm,&sVal);` |
|   134 |  632 | `		SxeBaseValue(pVm,pThis,pWalk,&sVal);` |
|   134 |  633 | `		if( bUseIter ){` |
|    26 |  634 | `			ph7_array_add_elem(pOut,0,&sVal);` |
|    14 |  635 | `		}else{` |
|   110 |  636 | `			SxeTableAdd(pVm,pOut,(const char *)pWalk->name,(int)xmlStrlen(pWalk->name),&sVal);` |
|     - |  637 | `		}` |
|   134 |  638 | `		PH7_MemObjRelease(&sVal);` |
|    90 |  639 | `next_node:` |
|   182 |  640 | `		pWalk = bUseIter ? SxeNextNode(pThis,pWalk) : pWalk->next;` |
|     2 |  641 | `	}` |
|    86 |  642 | `}` |
|     - |  643 |  |
|     - |  644 | `/* php's get_properties / get_debug_info / get_object_vars for a SimpleXML` |
|     - |  645 | ` * object: all three are this table, unlike every other native class here (a` |
|     - |  646 | `` * DateTime shows nothing to get_object_vars, a DOM node nothing to `(array)`).`` |
|     - |  647 | ` * That is why the hook answers every purpose. */` |
|   156 |  648 | `static sxi32 SxePresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|     2 |  649 | `{` |
|   158 |  650 | `	if( pThis == 0 \|\| pOut == 0 \|\| (pOut->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 |  651 | `		return SXERR_NOTFOUND;` |
|     - |  652 | `	}` |
|   158 |  653 | `	SxeTable(pVm,pThis,bDebug,pOut);` |
|   158 |  654 | `	return SXRET_OK;` |
|    80 |  655 | `}` |
|     - |  656 | `/*` |
|     - |  657 | ` * php's cast_object for _IS_BOOL, which is the one cast a SimpleXML object does` |
|     - |  658 | `` * not answer with `true`.`` |
|     - |  659 | ` *` |
|     - |  660 | ` * A node the question actually FINDS is truthy; with none, the answer is` |
|     - |  661 | ``  * whether the TABLE has anything in it.  So `(bool)simplexml_load_string('<r/>')` `` |
|     - |  662 | `` * is false and `(bool)simplexml_load_string('<r a="1"/>')` is true -- an object`` |
|     - |  663 | ` * whose question is "this node" never finds a first node, because php's` |
|     - |  664 | `` * `php_sxe_get_first_node` answers NULL for SXE_ITER_NONE when the caller`` |
|     - |  665 | ` * passes no node, and the cast is the one caller that does.` |
|     - |  666 | ` */` |
|    16 |  667 | `static int SxeBool(ph7_vm *pVm,ph7_class_instance *pThis)` |
|     1 |  668 | `{` |
|     - |  669 | `	ph7_value sTable;` |
|     - |  670 | `	int bTruthy;` |
|    17 |  671 | `	if( SxeTypeOf(pThis) != SXE_ITER_NONE && SxeFirstNode(pThis) != 0 ){` |
|     9 |  672 | `		return 1;` |
|     - |  673 | `	}` |
|     9 |  674 | `	PH7_MemObjInit(pVm,&sTable);` |
|     9 |  675 | `	if( PH7_MemObjToHashmap(&sTable) != SXRET_OK ){` |
|   ! 0 |  676 | `		return 1;` |
|     - |  677 | `	}` |
|     - |  678 | `	/* The emptiness question ALWAYS counts attributes -- php's own` |
|     - |  679 | ``	 * `sxe_prop_is_empty` has no `children()` exception where its get_properties`` |
|     - |  680 | ``	 * does, so `(bool)$x->children()` on an element with attributes and no`` |
|     - |  681 | ``	 * element children is TRUE while `(array)` of the same object is empty. */`` |
|     9 |  682 | `	SxeTable(pVm,pThis,1,&sTable);` |
|     9 |  683 | `	bTruthy = ph7_array_count(&sTable) > 0;` |
|     9 |  684 | `	PH7_MemObjRelease(&sTable);` |
|     9 |  685 | `	return bTruthy;` |
|     9 |  686 | `}` |
|     - |  687 |  |
|     - |  688 | `/* ===== The three answers a plain read gives ===== */` |
|     - |  689 |  |
|     - |  690 | ``/* `(string)$x`: the text of the first node the question finds, and an`` |
|     - |  691 | ` * attribute's own value when that is what it found. */` |
|   132 |  692 | `static void SxeToText(ph7_vm *pVm,ph7_class_instance *pThis,SyBlob *pOut)` |
|     3 |  693 | `{` |
|   135 |  694 | `	xmlNodePtr pNode = SxeTypeOf(pThis) == SXE_ITER_NONE` |
|   132 |  695 | `		? SxeNodeOf(pThis) : SxeFirstNode(pThis);` |
|    66 |  696 | `	(void)pVm;` |
|   135 |  697 | `	SxeNodeText(pOut,pNode);` |
|   135 |  698 | `}` |
|     - |  699 | ``/* `count($x)`: how many nodes the question stands for. For "this node" that is`` |
|     - |  700 | ` * its child ELEMENTS -- php counts what a foreach would yield, and a foreach` |
|     - |  701 | ` * over an element yields its children. */` |
|    16 |  702 | `static sxi64 SxeCount(ph7_class_instance *pThis)` |
|     1 |  703 | `{` |
|    17 |  704 | `	xmlNodePtr pNode = SxeIterFirst(pThis);` |
|    17 |  705 | `	sxi64 n = 0;` |
|    39 |  706 | `	while( pNode ){` |
|    23 |  707 | `		n++;` |
|    23 |  708 | `		pNode = SxeIterNext(pThis,pNode);` |
|     1 |  709 | `	}` |
|    17 |  710 | `	return n;` |
|     1 |  711 | `}` |
|     - |  712 |  |
|     - |  713 | `/* ===== Property access: php's read_property / write_property ===== */` |
|     - |  714 |  |
|     - |  715 | `/*` |
|     - |  716 | `` * `$x->name` -- the ELEMENT question, always answered and never a miss: php`` |
|     - |  717 | ` * hands back an object standing for the (possibly empty) set, so a name that` |
|     - |  718 | ` * is not in the document is an object with count 0 and an empty table.  An` |
|     - |  719 | `` * `isset()` is a different question and answers by whether the set has a first`` |
|     - |  720 | ` * node.` |
|     - |  721 | ` */` |
|   140 |  722 | `static void SxePropRead(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativePropCtx *pCtx)` |
|     3 |  723 | `{` |
|   143 |  724 | `	const SyString *pName = pCtx->pName;` |
|   143 |  725 | `	xmlNodePtr pNode = SxeNodeOf(pThis);` |
|     - |  726 | `	ph7_class_instance *pSub;` |
|   143 |  727 | `	if( pNode == 0 ){` |
|   ! 0 |  728 | `		return;` |
|     - |  729 | `	}` |
|   143 |  730 | `	if( SxeTypeOf(pThis) == SXE_ITER_ATTRLIST ){` |
|     - |  731 | `		/* php's handler reaches an attribute list and finds no element to look` |
|     - |  732 | `		 * under: the read answers null and a write is dropped on the floor. */` |
|   ! 0 |  733 | `		pCtx->bAnswered = 1;` |
|   ! 0 |  734 | `		if( pCtx->iMode != PH7_NATIVE_PROP_READ ){` |
|   ! 0 |  735 | `			PH7_MemObjRelease(pCtx->pResult);` |
|   ! 0 |  736 | `			ph7_value_bool(pCtx->pResult,0);` |
|   ! 0 |  737 | `		}` |
|   ! 0 |  738 | `		return;` |
|     - |  739 | `	}` |
|   143 |  740 | `	if( SxeTypeOf(pThis) == SXE_ITER_ELEMENT ){` |
|     - |  741 | `		/* Only a NAMED set collapses to its first match before asking for a` |
|     - |  742 | ``		 * child: `$x->a->b` means the b of the first a. "This node" and`` |
|     - |  743 | ``		 * `children()` keep their own node, which is what makes`` |
|     - |  744 | ``		 * `$x->children('urn:a')->kid` the urn:a KID OF THIS NODE and not a`` |
|     - |  745 | `		 * grandchild. */` |
|   ! 0 |  746 | `		pNode = SxeFirstNode(pThis);` |
|   ! 0 |  747 | `		if( pNode == 0 ){` |
|   ! 0 |  748 | `			pCtx->bAnswered = 1;` |
|   ! 0 |  749 | `			if( pCtx->iMode != PH7_NATIVE_PROP_READ ){` |
|   ! 0 |  750 | `				PH7_MemObjRelease(pCtx->pResult);` |
|   ! 0 |  751 | `				ph7_value_bool(pCtx->pResult,0);` |
|   ! 0 |  752 | `			}` |
|   ! 0 |  753 | `			return;` |
|     - |  754 | `		}` |
|   ! 0 |  755 | `	}` |
|   213 |  756 | `	pSub = SxeDerive(pVm,pThis,pNode,SXE_ITER_ELEMENT,` |
|   140 |  757 | `		SyStringData(pName),(int)SyStringLength(pName));` |
|   143 |  758 | `	if( pSub == 0 ){` |
|   ! 0 |  759 | `		return;` |
|     - |  760 | `	}` |
|   143 |  761 | `	pCtx->bAnswered = 1;` |
|   143 |  762 | `	if( pCtx->iMode == PH7_NATIVE_PROP_READ ){` |
|   137 |  763 | `		PH7_MemObjRelease(pCtx->pResult);` |
|   137 |  764 | `		PH7_MemObjInit(pVm,pCtx->pResult);` |
|   137 |  765 | `		pCtx->pResult->x.pOther = pSub;` |
|   137 |  766 | `		pCtx->pResult->iFlags = MEMOBJ_OBJ;` |
|   137 |  767 | `		return;` |
|     - |  768 | `	}` |
|     - |  769 | `	/* isset() / empty() / property_exists(): php asks the SET, not the object.` |
|     - |  770 | ``	 * `isset()` is "is there a node", and the two emptiness questions are the`` |
|     - |  771 | `	 * object's own truth -- which for a found node with empty text is still` |
|     - |  772 | `	 * true, because the table carries it. */` |
|     - |  773 | `	{` |
|     - |  774 | `		int bAns;` |
|     7 |  775 | `		if( pCtx->iMode == PH7_NATIVE_PROP_ISSET \|\| pCtx->iMode == PH7_NATIVE_PROP_EXISTS ){` |
|     7 |  776 | `			bAns = SxeFirstNode(pSub) != 0;` |
|     4 |  777 | `		}else{` |
|   ! 0 |  778 | `			bAns = SxeBool(pVm,pSub);` |
|     - |  779 | `		}` |
|     7 |  780 | `		PH7_MemObjRelease(pCtx->pResult);` |
|     7 |  781 | `		ph7_value_bool(pCtx->pResult,bAns);` |
|     7 |  782 | `		PH7_ClassInstanceUnref(pSub);` |
|     - |  783 | `	}` |
|    73 |  784 | `}` |
|     - |  785 |  |
|     - |  786 | `/* ===== Writing ===== */` |
|     - |  787 |  |
|     - |  788 | `/*` |
|     - |  789 | ` * A NUL-terminated copy of a slice, for the libxml calls that take one. The` |
|     - |  790 | ` * copy lives in the caller's blob until it releases it -- a fixed buffer would` |
|     - |  791 | ` * have to REFUSE a name longer than itself, and refusing silently is the one` |
|     - |  792 | ` * answer php never gives.` |
|     - |  793 | ` */` |
|   110 |  794 | `static const char * SxeCopyZ(ph7_vm *pVm,SyBlob *pBuf,const char *z,int n)` |
|     1 |  795 | `{` |
|   111 |  796 | `	SyBlobInit(pBuf,&pVm->sAllocator);` |
|   111 |  797 | `	if( z && n > 0 ){` |
|   111 |  798 | `		SyBlobAppend(pBuf,z,(sxu32)n);` |
|    55 |  799 | `	}` |
|   111 |  800 | `	SyBlobNullAppend(pBuf);` |
|   111 |  801 | `	return (const char *)SyBlobData(pBuf);` |
|     1 |  802 | `}` |
|     - |  803 |  |
|     - |  804 | `/*` |
|     - |  805 | ` * The text a value becomes when it lands in the tree.` |
|     - |  806 | ` *` |
|     - |  807 | ` * php takes scalars and NULL through the ordinary string conversion, takes a` |
|     - |  808 | `` * SimpleXMLElement as its own text (so `$a->x = $b->y` copies the TEXT, not`` |
|     - |  809 | ` * the node), and refuses everything else -- an array, and any other object,` |
|     - |  810 | ` * including one that HAS a __toString().  The refusal names the destination:` |
|     - |  811 | `` * `properties` for a property write, `attributes` for an attribute one.`` |
|     - |  812 | ` * Answers 0 when it refused, with the TypeError already worded into pCtx.` |
|     - |  813 | ` */` |
|    72 |  814 | `static int SxeValueText(ph7_vm *pVm,ph7_value *pVal,int bAttr,SyBlob *pOut,` |
|     - |  815 | `	const char **pzClass,char *zMsg,sxu32 nMsg)` |
|     1 |  816 | `{` |
|    73 |  817 | `	SyBlobInit(pOut,&pVm->sAllocator);` |
|    73 |  818 | `	if( pVal == 0 ){` |
|   ! 0 |  819 | `		SyBlobNullAppend(pOut);` |
|   ! 0 |  820 | `		return 1;` |
|     - |  821 | `	}` |
|    73 |  822 | `	if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|     3 |  823 | `		*pzClass = "TypeError";` |
|     4 |  824 | `		SyBufferFormat(zMsg,nMsg,` |
|     - |  825 | `			"It's not possible to assign a complex type to %s, array given",` |
|     1 |  826 | `			bAttr ? "attributes" : "properties");` |
|     3 |  827 | `		return 0;` |
|     - |  828 | `	}` |
|    71 |  829 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|     5 |  830 | `		ph7_class_instance *pObj = (ph7_class_instance *)pVal->x.pOther;` |
|     5 |  831 | `		if( pObj && PH7_NativeAttr(pObj,SXE_RES) ){` |
|     3 |  832 | `			SxeToText(pVm,pObj,pOut);` |
|     3 |  833 | `			SyBlobNullAppend(pOut);` |
|     3 |  834 | `			return 1;` |
|     - |  835 | `		}` |
|     3 |  836 | `		*pzClass = "TypeError";` |
|     6 |  837 | `		SyBufferFormat(zMsg,nMsg,` |
|     - |  838 | `			"It's not possible to assign a complex type to %s, %.*s given",` |
|     1 |  839 | `			bAttr ? "attributes" : "properties",` |
|     2 |  840 | `			pObj ? (int)pObj->pClass->sName.nByte : 6,` |
|     2 |  841 | `			pObj ? pObj->pClass->sName.zString : "object");` |
|     3 |  842 | `		return 0;` |
|     - |  843 | `	}` |
|     - |  844 | `	{` |
|    67 |  845 | `		int nLen = 0;` |
|    67 |  846 | `		const char *zStr = ph7_value_to_string(pVal,&nLen);` |
|    67 |  847 | `		if( nLen > 0 ){` |
|    63 |  848 | `			SyBlobAppend(pOut,zStr,(sxu32)nLen);` |
|    31 |  849 | `		}` |
|     - |  850 | `	}` |
|    67 |  851 | `	SyBlobNullAppend(pOut);` |
|    67 |  852 | `	return 1;` |
|    37 |  853 | `}` |
|     - |  854 | `/*` |
|     - |  855 | ``  * Put text into an element or an attribute the way php's `change_node_zval` `` |
|     - |  856 | `` * does: the string is ENCODED first and then set as content, so a `&` in a`` |
|     - |  857 | ` * value is stored as data and comes back out escaped -- unlike addChild(),` |
|     - |  858 | ` * whose value libxml PARSES.  An empty string leaves the node childless, which` |
|     - |  859 | `` * is what makes `$x->d = null` print `<d/>` and not `<d></d>`.`` |
|     - |  860 | ` */` |
|    64 |  861 | `static void SxeSetText(xmlNodePtr pNode,const char *zText)` |
|     1 |  862 | `{` |
|     - |  863 | `	xmlChar *pEnc;` |
|    65 |  864 | `	if( pNode == 0 ){` |
|   ! 0 |  865 | `		return;` |
|     - |  866 | `	}` |
|    65 |  867 | `	if( zText == 0 \|\| zText[0] == 0 ){` |
|     5 |  868 | `		xmlNodeSetContent(pNode,0);` |
|     5 |  869 | `		return;` |
|     - |  870 | `	}` |
|    61 |  871 | `	pEnc = xmlEncodeEntitiesReentrant(pNode->doc,(const xmlChar *)zText);` |
|    61 |  872 | `	xmlNodeSetContent(pNode,pEnc ? pEnc : (const xmlChar *)zText);` |
|    61 |  873 | `	if( pEnc ){` |
|    61 |  874 | `		xmlFree(pEnc);` |
|    30 |  875 | `	}` |
|    33 |  876 | `}` |
|     - |  877 | `/* A new element under pParent carrying pParent's namespace -- php's` |
|     - |  878 | `` * `xmlNewTextChild(node, node->ns, name, value)`, which is why a child created`` |
|     - |  879 | ` * under a default-namespace root is in that namespace too. */` |
|    12 |  880 | `static xmlNodePtr SxeNewChild(ph7_vm *pVm,xmlNodePtr pParent,const char *zName,int nName,` |
|     - |  881 | `	const char *zText)` |
|     1 |  882 | `{` |
|     - |  883 | `	SyBlob sName;` |
|     - |  884 | `	const char *zZ;` |
|     - |  885 | `	xmlNodePtr pNew;` |
|    13 |  886 | `	if( pParent == 0 \|\| zName == 0 \|\| nName < 1 ){` |
|   ! 0 |  887 | `		return 0;` |
|     - |  888 | `	}` |
|    13 |  889 | `	zZ = SxeCopyZ(pVm,&sName,zName,nName);` |
|     - |  890 | `` 	/* An xmlDoc is not an xmlNode past its first eight fields -- reading `ns` `` |
|     - |  891 | `	 * off one lands on an int. A document parent carries no namespace anyway.` |
|     - |  892 | `	 * The text goes in RAW (escaped at serialize time), and an EMPTY one still` |
|     - |  893 | ``	 * makes a text child -- which is why creating `$x->c = null` prints`` |
|     - |  894 | ``	 * `<c></c>` where setting an existing `<c>` to null prints `<c/>`. */`` |
|    13 |  895 | `	pNew = xmlNewTextChild(pParent,` |
|    12 |  896 | `		pParent->type == XML_ELEMENT_NODE ? pParent->ns : 0,(const xmlChar *)zZ,` |
|     6 |  897 | `		(const xmlChar *)zText);` |
|    13 |  898 | `	SyBlobRelease(&sName);` |
|    13 |  899 | `	return pNew;` |
|     7 |  900 | `}` |
|     - |  901 | `/* The attribute of pNode with this name that the filter takes, or NULL. */` |
|    56 |  902 | `static xmlAttrPtr SxeFindAttr(ph7_class_instance *pThis,xmlNodePtr pNode,` |
|     - |  903 | `	const char *zName,int nName)` |
|     3 |  904 | `{` |
|     - |  905 | `	xmlAttrPtr pAttr;` |
|    59 |  906 | `	if( pNode == 0 \|\| pNode->type != XML_ELEMENT_NODE ){` |
|   ! 0 |  907 | `		return 0;` |
|     - |  908 | `	}` |
|    83 |  909 | `	for( pAttr = pNode->properties ; pAttr ; pAttr = pAttr->next ){` |
|    55 |  910 | `		if( !SxeMatch(pThis,(xmlNodePtr)pAttr) ){` |
|   ! 0 |  911 | `			continue;` |
|     - |  912 | `		}` |
|    52 |  913 | `		if( pAttr->name && (int)xmlStrlen(pAttr->name) == nName` |
|    50 |  914 | `		 && SyMemcmp(pAttr->name,zName,(sxu32)nName) == 0 ){` |
|    31 |  915 | `			return pAttr;` |
|     - |  916 | `		}` |
|    15 |  917 | `	}` |
|    30 |  918 | `	return 0;` |
|    31 |  919 | `}` |
|     - |  920 | `/*` |
|     - |  921 | ` * The node a WRITE lands on or beside: the object's own node for "this node",` |
|     - |  922 | ` * and the first match otherwise -- MATERIALIZED when there is none, so` |
|     - |  923 | `` * `$x->a->b = 'v'` on `<r/>` creates `<a>` before it creates `<b>`.  Answers 0`` |
|     - |  924 | ` * for a question that has nothing to write through (an attribute list, an` |
|     - |  925 | ` * object with no node).` |
|     - |  926 | ` */` |
|    52 |  927 | `static xmlNodePtr SxeWriteBase(ph7_vm *pVm,ph7_class_instance *pThis,int bCreate)` |
|     1 |  928 | `{` |
|    53 |  929 | `	int iType = SxeTypeOf(pThis);` |
|    53 |  930 | `	xmlNodePtr pNode = SxeNodeOf(pThis);` |
|     - |  931 | `	xmlNodePtr pFirst;` |
|    53 |  932 | `	if( pNode == 0 \|\| iType == SXE_ITER_ATTRLIST ){` |
|    13 |  933 | `		return iType == SXE_ITER_ATTRLIST ? pNode : 0;` |
|     - |  934 | `	}` |
|    41 |  935 | `	if( iType == SXE_ITER_NONE \|\| iType == SXE_ITER_CHILD ){` |
|    35 |  936 | `		return pNode;` |
|     - |  937 | `	}` |
|     7 |  938 | `	pFirst = SxeFirstNode(pThis);` |
|     7 |  939 | `	if( pFirst == 0 && bCreate ){` |
|     5 |  940 | `		int nNm = 0;` |
|     5 |  941 | `		const char *zNm = SxeIterName(pThis,&nNm);` |
|     5 |  942 | `		pFirst = SxeNewChild(pVm,pNode,zNm,nNm,0);` |
|     2 |  943 | `	}` |
|     7 |  944 | `	return pFirst;` |
|    27 |  945 | `}` |
|     - |  946 |  |
|     - |  947 | `/*` |
|     - |  948 | ` * A warning php raises from a WRITE, which is not a call: there is no accessor` |
|     - |  949 | ` * name to print in front of it, so php attributes it to the caller's scope --` |
|     - |  950 | `` * `main(): Cannot assign to an array of nodes`, `f(): ...` inside a function.`` |
|     - |  951 | ` * Same shape ext/dom's property writes use.` |
|     - |  952 | ` */` |
|     8 |  953 | `static void SxeCallerWarn(ph7_vm *pVm,const char *zFormat,...)` |
|     1 |  954 | `{` |
|     - |  955 | `	SyBlob sFn,sMsg;` |
|     - |  956 | `	SyString sName;` |
|     - |  957 | `	va_list ap;` |
|     9 |  958 | `	SyBlobInit(&sFn,&pVm->sAllocator);` |
|     9 |  959 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     9 |  960 | `	PH7_VmActiveFuncName(pVm,&sFn);` |
|     9 |  961 | `	va_start(ap,zFormat);` |
|     9 |  962 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|     9 |  963 | `	va_end(ap);` |
|     9 |  964 | `	SyBlobNullAppend(&sMsg);` |
|     9 |  965 | `	SyStringInitFromBuf(&sName,SyBlobData(&sFn),SyBlobLength(&sFn));` |
|     9 |  966 | `	PH7_VmThrowError(pVm,&sName,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|     9 |  967 | `	SyBlobRelease(&sMsg);` |
|     9 |  968 | `	SyBlobRelease(&sFn);` |
|     9 |  969 | `}` |
|     - |  970 | `/*` |
|     - |  971 | ` * An attribute write by name: set where it exists, created where it does not.` |
|     - |  972 | `` * php has one handler for `$x['a'] = v` and `$x->attributes()->a = v`, so both`` |
|     - |  973 | ` * doors land here.  The new attribute carries no namespace even when the` |
|     - |  974 | ` * object is filtered to one -- php's xmlNewProp, reproduced.` |
|     - |  975 | ` */` |
|    36 |  976 | `static void SxeAttrStore(ph7_vm *pVm,ph7_class_instance *pThis,xmlNodePtr pBase,` |
|     - |  977 | `	const char *zName,int nName,ph7_value *pValue,const char **pzClass,char *zMsg,sxu32 nMsg)` |
|     1 |  978 | `{` |
|     - |  979 | `	SyBlob sText;` |
|     - |  980 | `	xmlAttrPtr pAttr;` |
|    37 |  981 | `	if( nName < 1 \|\| !SxeValueText(pVm,pValue,1,&sText,pzClass,zMsg,nMsg) ){` |
|   ! 0 |  982 | `		return;` |
|     - |  983 | `	}` |
|    37 |  984 | `	if( pBase == 0 \|\| pBase->type != XML_ELEMENT_NODE ){` |
|     - |  985 | `		/* php refuses the value before it looks for a node to write it to. */` |
|   ! 0 |  986 | `		SyBlobRelease(&sText);` |
|   ! 0 |  987 | `		return;` |
|     - |  988 | `	}` |
|    37 |  989 | `	pAttr = SxeFindAttr(pThis,pBase,zName,nName);` |
|    37 |  990 | `	if( pAttr == 0 ){` |
|     - |  991 | `		/* An attribute list creates one too since php 8.5.11; 8.5.10 took the` |
|     - |  992 | `		 * list's first attribute for the element and created nothing. */` |
|     - |  993 | `		SyBlob sNm;` |
|    37 |  994 | `		pAttr = xmlNewProp(pBase,` |
|    24 |  995 | `			(const xmlChar *)SxeCopyZ(pVm,&sNm,zName,nName),(const xmlChar *)"");` |
|    25 |  996 | `		SyBlobRelease(&sNm);` |
|    12 |  997 | `	}` |
|    37 |  998 | `	if( pAttr ){` |
|    37 |  999 | `		SxeSetText((xmlNodePtr)pAttr,(const char *)SyBlobData(&sText));` |
|    18 | 1000 | `	}` |
|    37 | 1001 | `	SyBlobRelease(&sText);` |
|    19 | 1002 | `}` |
|     - | 1003 | `/*` |
|     - | 1004 | `` * `$x->name = value` -- php's write_property.`` |
|     - | 1005 | ` *` |
|     - | 1006 | ` * The write lands on the ONE child element of that name, creates it when there` |
|     - | 1007 | ` * is none, and REFUSES when there is more than one: php cannot tell which of a` |
|     - | 1008 | ` * set the program meant, and says so in a warning rather than picking.  On an` |
|     - | 1009 | `` * attribute list the name is an ATTRIBUTE, written as `$x['name']` would be.`` |
|     - | 1010 | ` */` |
|    42 | 1011 | `static void SxePropStore(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativePropCtx *pCtx)` |
|     1 | 1012 | `{` |
|    43 | 1013 | `	const SyString *pName = pCtx->pName;` |
|    43 | 1014 | `	ph7_value *pValue = pCtx->pResult;` |
|    43 | 1015 | `	xmlNodePtr pBase = SxeWriteBase(pVm,pThis,TRUE);` |
|    43 | 1016 | `	xmlNodePtr pWalk,pHit = 0;` |
|    43 | 1017 | `	int nHit = 0;` |
|     - | 1018 | `	SyBlob sText;` |
|    43 | 1019 | `	if( SxeTypeOf(pThis) == SXE_ITER_ATTRLIST ){` |
|    19 | 1020 | `		SxeAttrStore(pVm,pThis,pBase,SyStringData(pName),(int)SyStringLength(pName),` |
|    12 | 1021 | `			pValue,&pCtx->zThrowClass,pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg));` |
|    16 | 1022 | `		return;` |
|     - | 1023 | `	}` |
|    31 | 1024 | `	if( pBase == 0 \|\| pBase->type != XML_ELEMENT_NODE ){` |
|   ! 0 | 1025 | `		return;` |
|     - | 1026 | `	}` |
|   111 | 1027 | `	for( pWalk = pBase->children ; pWalk ; pWalk = pWalk->next ){` |
|    81 | 1028 | `		if( pWalk->type != XML_ELEMENT_NODE \|\| !SxeMatch(pThis,pWalk) ){` |
|   ! 0 | 1029 | `			continue;` |
|     - | 1030 | `		}` |
|    80 | 1031 | `		if( pWalk->name == 0` |
|    80 | 1032 | `		 \|\| (sxu32)xmlStrlen(pWalk->name) != SyStringLength(pName)` |
|    78 | 1033 | `		 \|\| SyMemcmp(pWalk->name,SyStringData(pName),SyStringLength(pName)) != 0 ){` |
|    53 | 1034 | `			continue;` |
|     - | 1035 | `		}` |
|    29 | 1036 | `		if( pHit == 0 ){` |
|    27 | 1037 | `			pHit = pWalk;` |
|    13 | 1038 | `		}` |
|    29 | 1039 | `		nHit++;` |
|    15 | 1040 | `	}` |
|    31 | 1041 | `	if( nHit > 1 ){` |
|     3 | 1042 | `		SxeCallerWarn(pVm,` |
|     - | 1043 | `			"Cannot assign to an array of nodes (duplicate subnodes or attr detected)");` |
|     3 | 1044 | `		return;` |
|     - | 1045 | `	}` |
|    43 | 1046 | `	if( !SxeValueText(pVm,pValue,0,&sText,&pCtx->zThrowClass,` |
|    28 | 1047 | `		pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg)) ){` |
|     5 | 1048 | `		return;` |
|     - | 1049 | `	}` |
|    25 | 1050 | `	if( pHit ){` |
|    21 | 1051 | `		SxeSetText(pHit,(const char *)SyBlobData(&sText));` |
|    11 | 1052 | `	}else{` |
|     7 | 1053 | `		SxeNewChild(pVm,pBase,SyStringData(pName),(int)SyStringLength(pName),` |
|     4 | 1054 | `			(const char *)SyBlobData(&sText));` |
|     - | 1055 | `	}` |
|    25 | 1056 | `	SyBlobRelease(&sText);` |
|    22 | 1057 | `}` |
|     - | 1058 | ``/* `unset($x->name)` -- php removes EVERY matching child, not the first. */`` |
|     6 | 1059 | `static void SxePropUnset(ph7_vm *pVm,ph7_class_instance *pThis,const SyString *pName)` |
|     1 | 1060 | `{` |
|     7 | 1061 | `	xmlNodePtr pBase = SxeWriteBase(pVm,pThis,FALSE);` |
|     - | 1062 | `	xmlNodePtr pWalk,pNext;` |
|     6 | 1063 | `	if( SxeTypeOf(pThis) == SXE_ITER_ATTRLIST \|\| pBase == 0` |
|     7 | 1064 | `	 \|\| pBase->type != XML_ELEMENT_NODE ){` |
|   ! 0 | 1065 | `		return;` |
|     - | 1066 | `	}` |
|    25 | 1067 | `	for( pWalk = pBase->children ; pWalk ; pWalk = pNext ){` |
|    19 | 1068 | `		pNext = pWalk->next;` |
|    19 | 1069 | `		if( pWalk->type != XML_ELEMENT_NODE \|\| !SxeMatch(pThis,pWalk) ){` |
|   ! 0 | 1070 | `			continue;` |
|     - | 1071 | `		}` |
|    18 | 1072 | `		if( pWalk->name == 0` |
|    18 | 1073 | `		 \|\| (sxu32)xmlStrlen(pWalk->name) != SyStringLength(pName)` |
|    16 | 1074 | `		 \|\| SyMemcmp(pWalk->name,SyStringData(pName),SyStringLength(pName)) != 0 ){` |
|    13 | 1075 | `			continue;` |
|     - | 1076 | `		}` |
|     7 | 1077 | `		xmlUnlinkNode(pWalk);` |
|     7 | 1078 | `		xmlFreeNode(pWalk);` |
|     4 | 1079 | `	}` |
|     4 | 1080 | `}` |
|     - | 1081 | `/*` |
|     - | 1082 | ` * ph7_class::xProp -- php's read_property / has_property / write_property /` |
|     - | 1083 | ` * unset_property for a class that keeps no property slot at all.` |
|     - | 1084 | ` *` |
|     - | 1085 | ` * Every name is the handler's: SimpleXMLElement declares nothing, so the` |
|     - | 1086 | ` * ordinary path would create a dynamic property where php asks the document.` |
|     - | 1087 | ` * A subclass's OWN declared property is a slot and never reaches here (the` |
|     - | 1088 | ` * engine consults the hook only where the instance has none), which is the` |
|     - | 1089 | ` * one place php and this differ -- php's handler owns those too.` |
|     - | 1090 | ` */` |
|   314 | 1091 | `static void SxePropHook(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativePropCtx *pCtx)` |
|     3 | 1092 | `{` |
|   317 | 1093 | `	if( PH7_NativeAttr(pThis,SXE_RES) == 0 ){` |
|   ! 0 | 1094 | `		return;` |
|     - | 1095 | `	}` |
|   317 | 1096 | `	if( pCtx->iMode == PH7_NATIVE_PROP_OWNS ){` |
|     - | 1097 | `		/* Every name is: the class declares no property, so php's write_property` |
|     - | 1098 | `		 * stands where a dynamic property would be created. */` |
|    85 | 1099 | `		pCtx->bAnswered = 1;` |
|    85 | 1100 | `		return;` |
|     - | 1101 | `	}` |
|   233 | 1102 | `	if( pCtx->iMode == PH7_NATIVE_PROP_WRITE ){` |
|     - | 1103 | `		/* Asked before the value exists, and this handler really STORES -- so it` |
|     - | 1104 | `		 * declines here and takes the write at STORE below, which is the door` |
|     - | 1105 | `		 * every overloaded write shape ends at. Answering here would SWALLOW the` |
|     - | 1106 | `		 * store: the opcode would consume the access and the value would land` |
|     - | 1107 | `		 * nowhere. */` |
|    43 | 1108 | `		return;` |
|     - | 1109 | `	}` |
|   191 | 1110 | `	if( pCtx->iMode == PH7_NATIVE_PROP_STORE ){` |
|    43 | 1111 | `		SxePropStore(pVm,pThis,pCtx);` |
|    43 | 1112 | `		pCtx->bAnswered = 1;` |
|    43 | 1113 | `		return;` |
|     - | 1114 | `	}` |
|   149 | 1115 | `	if( pCtx->iMode == PH7_NATIVE_PROP_UNSET ){` |
|     7 | 1116 | `		SxePropUnset(pVm,pThis,pCtx->pName);` |
|     7 | 1117 | `		pCtx->bAnswered = 1;` |
|     7 | 1118 | `		return;` |
|     - | 1119 | `	}` |
|   143 | 1120 | `	SxePropRead(pVm,pThis,pCtx);` |
|   160 | 1121 | `}` |
|     - | 1122 |  |
|     - | 1123 | `/* ===== Dimensions: php's read_dimension / write_dimension ===== */` |
|     - | 1124 |  |
|     - | 1125 | `/*` |
|     - | 1126 | ` * php's dimension rules turn on the OFFSET'S TYPE, not on its text: a string` |
|     - | 1127 | ` * offset is an ATTRIBUTE name and an integer one is a position in the set.` |
|     - | 1128 | `` * That is why `$x['0']` makes an attribute called `0` and `$x[0]` reaches the`` |
|     - | 1129 | ` * element -- the two spellings are different questions.` |
|     - | 1130 | ` */` |
|    60 | 1131 | `static int SxeDimIsInt(ph7_value *pOffset)` |
|     4 | 1132 | `{` |
|    72 | 1133 | `	return pOffset != 0 && (pOffset->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL)) != 0` |
|    90 | 1134 | `		&& (pOffset->iFlags & MEMOBJ_STRING) == 0;` |
|     4 | 1135 | `}` |
|     - | 1136 | `/* The node at a positional offset in this object's walk, or NULL. A negative` |
|     - | 1137 | ` * offset reaches NOTHING: a read of it is empty, an unset a no-op and a write` |
|     - | 1138 | ` * the "Cannot add element" warning with no write (php 8.5.10; up to 8.5.9 the` |
|     - | 1139 | ` * loop counted UP to the offset and a negative one stopped on the first` |
|     - | 1140 | ` * node). The count is still taken, for that warning. */` |
|    12 | 1141 | `static xmlNodePtr SxeNodeAtOffset(ph7_class_instance *pThis,sxi64 iOfs,sxi64 *pnCount)` |
|     3 | 1142 | `{` |
|    15 | 1143 | `	xmlNodePtr pNode = SxeFirstNode(pThis);` |
|    15 | 1144 | `	xmlNodePtr pHit = 0;` |
|    15 | 1145 | `	sxi64 n = 0;` |
|    15 | 1146 | `	if( SxeTypeOf(pThis) == SXE_ITER_NONE ){` |
|   ! 0 | 1147 | `		if( pnCount ){` |
|   ! 0 | 1148 | `			*pnCount = pNode ? 1 : 0;` |
|   ! 0 | 1149 | `		}` |
|   ! 0 | 1150 | `		return iOfs == 0 ? pNode : 0;` |
|     - | 1151 | `	}` |
|    29 | 1152 | `	while( pNode ){` |
|    23 | 1153 | `		if( n == iOfs ){` |
|    11 | 1154 | `			pHit = pNode;` |
|    11 | 1155 | `			if( pnCount == 0 ){` |
|     9 | 1156 | `				return pHit;` |
|     - | 1157 | `			}` |
|     1 | 1158 | `		}` |
|    16 | 1159 | `		n++;` |
|    16 | 1160 | `		pNode = SxeNextNode(pThis,pNode);` |
|     2 | 1161 | `	}` |
|     7 | 1162 | `	if( pnCount ){` |
|     7 | 1163 | `		*pnCount = n;` |
|     3 | 1164 | `	}` |
|     7 | 1165 | `	return pHit;` |
|     9 | 1166 | `}` |
|     - | 1167 | `/* The element an ATTRIBUTE offset is asked of. */` |
|    40 | 1168 | `static xmlNodePtr SxeAttrBase(ph7_class_instance *pThis)` |
|     3 | 1169 | `{` |
|    43 | 1170 | `	int iType = SxeTypeOf(pThis);` |
|    43 | 1171 | `	if( iType == SXE_ITER_NONE \|\| iType == SXE_ITER_CHILD \|\| iType == SXE_ITER_ATTRLIST ){` |
|    43 | 1172 | `		return SxeNodeOf(pThis);` |
|     - | 1173 | `	}` |
|   ! 0 | 1174 | `	return SxeFirstNode(pThis);` |
|    23 | 1175 | `}` |
|    20 | 1176 | `static void SxeDimRead(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeDimCtx *pCtx)` |
|     4 | 1177 | `{` |
|    24 | 1178 | `	ph7_class_instance *pSub = 0;` |
|    24 | 1179 | `	int bIsset = pCtx->iMode == PH7_NATIVE_DIM_ISSET;` |
|    24 | 1180 | `	int bEmpty = pCtx->iMode == PH7_NATIVE_DIM_NOTEMPTY;` |
|    24 | 1181 | `	xmlNodePtr pHit = 0;` |
|    24 | 1182 | `	if( SxeDimIsInt(pCtx->pOffset) ){` |
|     6 | 1183 | `		pHit = SxeNodeAtOffset(pThis,ph7_value_to_int64(pCtx->pOffset),0);` |
|    21 | 1184 | `	}else if( pCtx->pOffset ){` |
|    19 | 1185 | `		int nName = 0;` |
|    19 | 1186 | `		const char *zName = ph7_value_to_string(pCtx->pOffset,&nName);` |
|    19 | 1187 | `		pHit = (xmlNodePtr)SxeFindAttr(pThis,SxeAttrBase(pThis),zName,nName);` |
|     8 | 1188 | `	}` |
|    24 | 1189 | `	if( pHit == 0 ){` |
|     3 | 1190 | `		if( bIsset \|\| bEmpty ){` |
|   ! 0 | 1191 | `			PH7_MemObjRelease(pCtx->pResult);` |
|   ! 0 | 1192 | `			ph7_value_bool(pCtx->pResult,0);` |
|   ! 0 | 1193 | `		}` |
|     3 | 1194 | `		return;` |
|     - | 1195 | `	}` |
|    22 | 1196 | `	if( bIsset ){` |
|   ! 0 | 1197 | `		PH7_MemObjRelease(pCtx->pResult);` |
|   ! 0 | 1198 | `		ph7_value_bool(pCtx->pResult,1);` |
|   ! 0 | 1199 | `		return;` |
|     - | 1200 | `	}` |
|    22 | 1201 | `	if( bEmpty ){` |
|     - | 1202 | ``		/* php's `check_empty` reads the offset's TEXT and judges THAT, so an`` |
|     - | 1203 | `		 * attribute holding "0" is empty() while the object a read of it hands` |
|     - | 1204 | `		 * back is truthy. */` |
|     - | 1205 | `		SyBlob sText;` |
|     - | 1206 | `		ph7_value sVal;` |
|   ! 0 | 1207 | `		SyBlobInit(&sText,&pVm->sAllocator);` |
|   ! 0 | 1208 | `		SxeNodeText(&sText,pHit);` |
|   ! 0 | 1209 | `		PH7_MemObjInitFromString(pVm,&sVal,0);` |
|   ! 0 | 1210 | `		PH7_MemObjStringAppend(&sVal,(const char *)SyBlobData(&sText),SyBlobLength(&sText));` |
|   ! 0 | 1211 | `		PH7_MemObjRelease(pCtx->pResult);` |
|   ! 0 | 1212 | `		ph7_value_bool(pCtx->pResult,ph7_value_to_bool(&sVal));` |
|   ! 0 | 1213 | `		PH7_MemObjRelease(&sVal);` |
|   ! 0 | 1214 | `		SyBlobRelease(&sText);` |
|   ! 0 | 1215 | `		return;` |
|     - | 1216 | `	}` |
|    22 | 1217 | `	pSub = SxeDerive(pVm,pThis,pHit,SXE_ITER_NONE,0,0);` |
|    22 | 1218 | `	if( pSub == 0 ){` |
|   ! 0 | 1219 | `		return;` |
|     - | 1220 | `	}` |
|    22 | 1221 | `	PH7_MemObjRelease(pCtx->pResult);` |
|    22 | 1222 | `	PH7_MemObjInit(pVm,pCtx->pResult);` |
|    22 | 1223 | `	pCtx->pResult->x.pOther = pSub;` |
|    22 | 1224 | `	pCtx->pResult->iFlags = MEMOBJ_OBJ;` |
|    14 | 1225 | `}` |
|     - | 1226 | `/*` |
|     - | 1227 | `` * `$x[$k] = $v`, `$x[] = $v` and `unset($x[$k])`.`` |
|     - | 1228 | ` *` |
|     - | 1229 | ` * A string offset is an attribute: set where it exists, created where it does` |
|     - | 1230 | ` * not, and on a MISSING element the element is created first, so` |
|     - | 1231 | `` * `$x->kid['a'] = '1'` on `<r/>` yields `<r><kid a="1"/></r>`.`` |
|     - | 1232 | ` *` |
|     - | 1233 | ` * An integer offset is a position, and the write goes where php's does:` |
|     - | 1234 | ` *   * "this node" writes the node's own text, warning when the offset is past` |
|     - | 1235 | ` *     the one node it stands for;` |
|     - | 1236 | ` *   * a NAMED set writes the offset-th match, and past the end warns and then` |
|     - | 1237 | ` *     appends a fresh sibling of the first match;` |
|     - | 1238 | `` *   * `children()` -- the one question with no name -- creates a SIBLING OF ITS`` |
|     - | 1239 | ` *     OWN NODE carrying that node's name, which for a root element means a` |
|     - | 1240 | ` *     second root. php's own answer, degenerate and reproduced.` |
|     - | 1241 | ` */` |
|    38 | 1242 | `static void SxeDimWrite(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeDimCtx *pDim)` |
|     1 | 1243 | `{` |
|    39 | 1244 | `	int iType = SxeTypeOf(pThis);` |
|     - | 1245 | `	SyBlob sText;` |
|    39 | 1246 | `	if( pDim->pOffset && !SxeDimIsInt(pDim->pOffset) ){` |
|    25 | 1247 | `		int nName = 0;` |
|    25 | 1248 | `		const char *zName = ph7_value_to_string(pDim->pOffset,&nName);` |
|    25 | 1249 | `		xmlNodePtr pBase = iType == SXE_ITER_ELEMENT` |
|    24 | 1250 | `			? SxeWriteBase(pVm,pThis,TRUE) : SxeAttrBase(pThis);` |
|    37 | 1251 | `		SxeAttrStore(pVm,pThis,pBase,zName,nName,pDim->pResult,&pDim->zThrowClass,` |
|    24 | 1252 | `			pDim->zThrowMsg,sizeof(pDim->zThrowMsg));` |
|    25 | 1253 | `		return;` |
|     - | 1254 | `	}` |
|    15 | 1255 | `	if( iType == SXE_ITER_ATTRLIST ){` |
|   ! 0 | 1256 | `		if( pDim->pOffset == 0 ){` |
|   ! 0 | 1257 | `			pDim->zThrowClass = "ValueError";` |
|   ! 0 | 1258 | `			SyBufferFormat(pDim->zThrowMsg,sizeof(pDim->zThrowMsg),` |
|     - | 1259 | `				"Cannot append to an attribute list");` |
|   ! 0 | 1260 | `			return;` |
|     - | 1261 | `		}` |
|     - | 1262 | `		{` |
|   ! 0 | 1263 | `			xmlNodePtr pHit = SxeNodeAtOffset(pThis,ph7_value_to_int64(pDim->pOffset),0);` |
|   ! 0 | 1264 | `			if( pHit && SxeValueText(pVm,pDim->pResult,1,&sText,&pDim->zThrowClass,` |
|   ! 0 | 1265 | `				pDim->zThrowMsg,sizeof(pDim->zThrowMsg)) ){` |
|   ! 0 | 1266 | `				SxeSetText(pHit,(const char *)SyBlobData(&sText));` |
|   ! 0 | 1267 | `				SyBlobRelease(&sText);` |
|   ! 0 | 1268 | `			}` |
|     - | 1269 | `		}` |
|   ! 0 | 1270 | `		return;` |
|     - | 1271 | `	}` |
|    15 | 1272 | `	if( iType == SXE_ITER_NONE ){` |
|     7 | 1273 | `		xmlNodePtr pNode = SxeNodeOf(pThis);` |
|     7 | 1274 | `		sxi64 iOfs = pDim->pOffset ? ph7_value_to_int64(pDim->pOffset) : 0;` |
|     7 | 1275 | `		if( pDim->pOffset == 0 \|\| pNode == 0 ){` |
|     3 | 1276 | `			pDim->zThrowClass = "ValueError";` |
|     3 | 1277 | `			SyBufferFormat(pDim->zThrowMsg,sizeof(pDim->zThrowMsg),` |
|     - | 1278 | `				"Cannot append to an attribute list");` |
|     3 | 1279 | `			return;` |
|     - | 1280 | `		}` |
|     5 | 1281 | `		if( iOfs != 0 ){` |
|     - | 1282 | `			/* Only offset 0 is this node's own text; any other position is the` |
|     - | 1283 | `			 * warning and NO write (8.5.9 wrote the text anyway after warning). */` |
|     3 | 1284 | `			SxeCallerWarn(pVm,` |
|     - | 1285 | `				"Cannot add element %s number %qd when only 0 such elements exist",` |
|     2 | 1286 | `				pNode->name ? (const char *)pNode->name : "",iOfs);` |
|     3 | 1287 | `			return;` |
|     - | 1288 | `		}` |
|     4 | 1289 | `		if( SxeValueText(pVm,pDim->pResult,0,&sText,&pDim->zThrowClass,` |
|     2 | 1290 | `			pDim->zThrowMsg,sizeof(pDim->zThrowMsg)) ){` |
|     3 | 1291 | `			SxeSetText(pNode,(const char *)SyBlobData(&sText));` |
|     3 | 1292 | `			SyBlobRelease(&sText);` |
|     1 | 1293 | `		}` |
|     3 | 1294 | `		return;` |
|     - | 1295 | `	}` |
|     9 | 1296 | `	if( iType == SXE_ITER_ELEMENT ){` |
|     9 | 1297 | `		sxi64 nCount = 0;` |
|     9 | 1298 | `		sxi64 iOfs = pDim->pOffset ? ph7_value_to_int64(pDim->pOffset) : -1;` |
|     9 | 1299 | `		xmlNodePtr pHit = pDim->pOffset ? SxeNodeAtOffset(pThis,iOfs,&nCount) : 0;` |
|     9 | 1300 | `		int nNm = 0;` |
|     9 | 1301 | `		const char *zNm = SxeIterName(pThis,&nNm);` |
|     9 | 1302 | `		if( pHit == 0 && pDim->pOffset && (iOfs > nCount \|\| iOfs < 0) ){` |
|     - | 1303 | `			/* Past the end php warns and APPENDS anyway; a negative position` |
|     - | 1304 | `			 * warns and writes nothing. */` |
|     7 | 1305 | `			SxeCallerWarn(pVm,` |
|     - | 1306 | `				"Cannot add element %.*s number %qd when only %qd such elements exist",` |
|     2 | 1307 | `				nNm,zNm ? zNm : "",iOfs,nCount);` |
|     5 | 1308 | `			if( iOfs < 0 ){` |
|     3 | 1309 | `				return;` |
|     - | 1310 | `			}` |
|     1 | 1311 | `		}` |
|    10 | 1312 | `		if( !SxeValueText(pVm,pDim->pResult,0,&sText,&pDim->zThrowClass,` |
|     6 | 1313 | `			pDim->zThrowMsg,sizeof(pDim->zThrowMsg)) ){` |
|   ! 0 | 1314 | `			return;` |
|     - | 1315 | `		}` |
|     7 | 1316 | `		if( pHit ){` |
|     3 | 1317 | `			SxeSetText(pHit,(const char *)SyBlobData(&sText));` |
|     2 | 1318 | `		}else{` |
|     5 | 1319 | `			xmlNodePtr pFirst = SxeFirstNode(pThis);` |
|     5 | 1320 | `			if( pFirst && pFirst->parent ){` |
|     7 | 1321 | `				SxeNewChild(pVm,pFirst->parent,(const char *)pFirst->name,` |
|     4 | 1322 | `					(int)xmlStrlen(pFirst->name),(const char *)SyBlobData(&sText));` |
|     3 | 1323 | `			}else{` |
|   ! 0 | 1324 | `				SxeNewChild(pVm,SxeNodeOf(pThis),zNm,nNm,(const char *)SyBlobData(&sText));` |
|     - | 1325 | `			}` |
|     - | 1326 | `		}` |
|     7 | 1327 | `		SyBlobRelease(&sText);` |
|     7 | 1328 | `		return;` |
|     - | 1329 | `	}` |
|     - | 1330 | `	/* SXE_ITER_CHILD: php's own degenerate answer, above. */` |
|     - | 1331 | `	{` |
|   ! 0 | 1332 | `		xmlNodePtr pNode = SxeNodeOf(pThis);` |
|   ! 0 | 1333 | `		if( pNode == 0 \|\| pNode->parent == 0 ){` |
|   ! 0 | 1334 | `			return;` |
|     - | 1335 | `		}` |
|   ! 0 | 1336 | `		if( SxeValueText(pVm,pDim->pResult,0,&sText,&pDim->zThrowClass,` |
|   ! 0 | 1337 | `			pDim->zThrowMsg,sizeof(pDim->zThrowMsg)) ){` |
|   ! 0 | 1338 | `			SxeNewChild(pVm,pNode->parent,(const char *)pNode->name,` |
|   ! 0 | 1339 | `				(int)xmlStrlen(pNode->name),(const char *)SyBlobData(&sText));` |
|   ! 0 | 1340 | `			SyBlobRelease(&sText);` |
|   ! 0 | 1341 | `		}` |
|     - | 1342 | `	}` |
|    20 | 1343 | `}` |
|     6 | 1344 | `static void SxeDimUnset(ph7_class_instance *pThis,PH7_NativeDimCtx *pDim)` |
|     1 | 1345 | `{` |
|     7 | 1346 | `	xmlNodePtr pHit = 0;` |
|     7 | 1347 | `	if( SxeDimIsInt(pDim->pOffset) ){` |
|     3 | 1348 | `		pHit = SxeNodeAtOffset(pThis,ph7_value_to_int64(pDim->pOffset),0);` |
|     6 | 1349 | `	}else if( pDim->pOffset ){` |
|     5 | 1350 | `		int nName = 0;` |
|     5 | 1351 | `		const char *zName = ph7_value_to_string(pDim->pOffset,&nName);` |
|     5 | 1352 | `		pHit = (xmlNodePtr)SxeFindAttr(pThis,SxeAttrBase(pThis),zName,nName);` |
|     2 | 1353 | `	}` |
|     7 | 1354 | `	if( pHit == 0 ){` |
|     4 | 1355 | `		return;` |
|     - | 1356 | `	}` |
|     5 | 1357 | `	if( pHit->type == XML_ATTRIBUTE_NODE ){` |
|     3 | 1358 | `		xmlRemoveProp((xmlAttrPtr)pHit);` |
|     3 | 1359 | `		return;` |
|     - | 1360 | `	}` |
|     3 | 1361 | `	xmlUnlinkNode(pHit);` |
|     - | 1362 | `	/* php leaves the node in the document's own ownership rather than freeing` |
|     - | 1363 | `	 * it: an object still standing on it must not read freed memory. The shell` |
|     - | 1364 | `	 * frees it with the tree. */` |
|     3 | 1365 | `	SySetPut(&SxeResOf(pThis)->pShell->aOrphans,(const void *)&pHit);` |
|     4 | 1366 | `}` |
|     - | 1367 | `/* ph7_class::xDim -- one callback for all five modes. */` |
|    64 | 1368 | `static void SxeDimHook(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeDimCtx *pCtx)` |
|     4 | 1369 | `{` |
|    68 | 1370 | `	if( PH7_NativeAttr(pThis,SXE_RES) == 0 ){` |
|   ! 0 | 1371 | `		return;` |
|     - | 1372 | `	}` |
|    64 | 1373 | `	if( pCtx->iMode == PH7_NATIVE_DIM_READ \|\| pCtx->iMode == PH7_NATIVE_DIM_ISSET` |
|    48 | 1374 | `	 \|\| pCtx->iMode == PH7_NATIVE_DIM_NOTEMPTY ){` |
|    24 | 1375 | `		SxeDimRead(pVm,pThis,pCtx);` |
|    24 | 1376 | `		return;` |
|     - | 1377 | `	}` |
|    45 | 1378 | `	if( pCtx->pResult == 0 && pCtx->iMode != PH7_NATIVE_DIM_UNSET ){` |
|     - | 1379 | `		/* The refusal-WORDING probe (PH7_ClassNativeDimRefusal), which carries` |
|     - | 1380 | `		 * neither an offset nor a value. This class has no sentence of its own:` |
|     - | 1381 | `		 * every write it cannot take is php's generic one, so leaving the probe` |
|     - | 1382 | `		 * unanswered is the right answer. */` |
|   ! 0 | 1383 | `		return;` |
|     - | 1384 | `	}` |
|    45 | 1385 | `	if( pCtx->iMode == PH7_NATIVE_DIM_UNSET ){` |
|     7 | 1386 | `		SxeDimUnset(pThis,pCtx);` |
|     7 | 1387 | `		pCtx->bStored = 1;` |
|     7 | 1388 | `		return;` |
|     - | 1389 | `	}` |
|    39 | 1390 | `	SxeDimWrite(pVm,pThis,pCtx);` |
|    39 | 1391 | `	pCtx->bStored = 1;` |
|    36 | 1392 | `}` |
|     - | 1393 |  |
|     - | 1394 | `/* ===== The methods ===== */` |
|     - | 1395 |  |
|     - | 1396 | `/* The node a method that WORKS ON ONE node uses: "this node" for the plain` |
|     - | 1397 | ` * question and the first match otherwise. */` |
|   374 | 1398 | `static xmlNodePtr SxeMethodNode(ph7_class_instance *pThis)` |
|     4 | 1399 | `{` |
|   378 | 1400 | `	return SxeTypeOf(pThis) == SXE_ITER_NONE ? SxeNodeOf(pThis) : SxeFirstNode(pThis);` |
|     4 | 1401 | `}` |
|     - | 1402 | `/* SimpleXMLElement::getName(): the first node's name, and the empty string for` |
|     - | 1403 | ` * a question that finds none -- php answers "" rather than false. */` |
|    46 | 1404 | `SXE_METHOD(vm_builtin_SimpleXMLElement_getName)` |
|     2 | 1405 | `{` |
|    48 | 1406 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|    48 | 1407 | `	xmlNodePtr pNode = pThis ? SxeMethodNode(pThis) : 0;` |
|    23 | 1408 | `	(void)nArg; (void)apArg;` |
|    48 | 1409 | `	if( pNode == 0 \|\| pNode->name == 0 ){` |
|     3 | 1410 | `		ph7_result_string(pCtx,"",0);` |
|     3 | 1411 | `		return PH7_OK;` |
|     - | 1412 | `	}` |
|    46 | 1413 | `	ph7_result_string(pCtx,(const char *)pNode->name,(int)xmlStrlen(pNode->name));` |
|    46 | 1414 | `	return PH7_OK;` |
|    25 | 1415 | `}` |
|     - | 1416 | `/* SimpleXMLElement::__toString(). */` |
|   130 | 1417 | `SXE_METHOD(vm_builtin_SimpleXMLElement_toString)` |
|     3 | 1418 | `{` |
|   133 | 1419 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|     - | 1420 | `	SyBlob sText;` |
|    65 | 1421 | `	(void)nArg; (void)apArg;` |
|   133 | 1422 | `	if( pThis == 0 ){` |
|   ! 0 | 1423 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 | 1424 | `		return PH7_OK;` |
|     - | 1425 | `	}` |
|   133 | 1426 | `	SyBlobInit(&sText,&pCtx->pVm->sAllocator);` |
|   133 | 1427 | `	SxeToText(pCtx->pVm,pThis,&sText);` |
|   133 | 1428 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sText),(int)SyBlobLength(&sText));` |
|   133 | 1429 | `	SyBlobRelease(&sText);` |
|   133 | 1430 | `	return PH7_OK;` |
|    68 | 1431 | `}` |
|     - | 1432 | `/* SimpleXMLElement::count(). */` |
|    16 | 1433 | `SXE_METHOD(vm_builtin_SimpleXMLElement_count)` |
|     1 | 1434 | `{` |
|    17 | 1435 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|     8 | 1436 | `	(void)nArg; (void)apArg;` |
|    17 | 1437 | `	ph7_result_int64(pCtx,pThis ? SxeCount(pThis) : 0);` |
|    17 | 1438 | `	return PH7_OK;` |
|     1 | 1439 | `}` |
|     - | 1440 | `/* SimpleXMLElement::__debugInfo(): php's get_debug_info handler, reachable as a` |
|     - | 1441 | ` * method because php declares it as one. */` |
|     4 | 1442 | `SXE_METHOD(vm_builtin_SimpleXMLElement_debugInfo)` |
|     1 | 1443 | `{` |
|     5 | 1444 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|     5 | 1445 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|     2 | 1446 | `	(void)nArg; (void)apArg;` |
|     5 | 1447 | `	if( pOut == 0 ){` |
|   ! 0 | 1448 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1449 | `		return PH7_OK;` |
|     - | 1450 | `	}` |
|     5 | 1451 | `	if( pThis ){` |
|     5 | 1452 | `		SxeTable(pCtx->pVm,pThis,1,pOut);` |
|     2 | 1453 | `	}` |
|     5 | 1454 | `	ph7_result_value(pCtx,pOut);` |
|     5 | 1455 | `	return PH7_OK;` |
|     3 | 1456 | `}` |
|     - | 1457 | `/*` |
|     - | 1458 | ` * children() and attributes(), which are the same call with a different` |
|     - | 1459 | ` * question: both anchor at the node the receiver's question finds and both` |
|     - | 1460 | ` * take the namespace filter from their ARGUMENTS -- a filter the receiver` |
|     - | 1461 | ` * carried is not inherited through them, only through navigation.` |
|     - | 1462 | ` */` |
|   108 | 1463 | `static int SxeSubQuestion(ph7_context *pCtx,int nArg,ph7_value **apArg,int iType)` |
|     3 | 1464 | `{` |
|   111 | 1465 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|   111 | 1466 | `	xmlNodePtr pNode = pThis ? SxeMethodNode(pThis) : 0;` |
|   111 | 1467 | `	phl_domnode *pNd = pThis ? SxeResOf(pThis) : 0;` |
|   111 | 1468 | `	const char *zNs = 0;` |
|   111 | 1469 | `	int nNs = 0,bPrefix = 0;` |
|   111 | 1470 | `	if( pThis == 0 \|\| pNode == 0 ){` |
|   ! 0 | 1471 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1472 | `		return PH7_OK;` |
|     - | 1473 | `	}` |
|   111 | 1474 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0 ){` |
|    38 | 1475 | `		zNs = ph7_value_to_string(apArg[0],&nNs);` |
|    18 | 1476 | `	}` |
|   111 | 1477 | `	if( nArg > 1 ){` |
|    41 | 1478 | `		bPrefix = ph7_value_to_bool(apArg[1]);` |
|    20 | 1479 | `	}` |
|   111 | 1480 | `	return SxeResultObj(pCtx,SxeNew(pCtx->pVm,pThis->pClass,pNd ? pNd->pShell : 0,` |
|    54 | 1481 | `		pNode,iType,0,0,zNs,nNs,bPrefix));` |
|    57 | 1482 | `}` |
|    56 | 1483 | `SXE_METHOD(vm_builtin_SimpleXMLElement_children)` |
|     3 | 1484 | `{` |
|    59 | 1485 | `	return SxeSubQuestion(pCtx,nArg,apArg,SXE_ITER_CHILD);` |
|     3 | 1486 | `}` |
|    52 | 1487 | `SXE_METHOD(vm_builtin_SimpleXMLElement_attributes)` |
|     3 | 1488 | `{` |
|    55 | 1489 | `	return SxeSubQuestion(pCtx,nArg,apArg,SXE_ITER_ATTRLIST);` |
|     3 | 1490 | `}` |
|     - | 1491 |  |
|     - | 1492 | `/*` |
|     - | 1493 | ` * getNamespaces($recursive) -- the namespaces the node (and, recursively, its` |
|     - | 1494 | ` * subtree) USES: its own, and each of its attributes'.  Keyed by prefix, with` |
|     - | 1495 | ` * a default namespace under the empty key, and the FIRST binding of a prefix` |
|     - | 1496 | ` * wins.` |
|     - | 1497 | ` */` |
|    28 | 1498 | `static void SxeAddNs(ph7_vm *pVm,ph7_value *pOut,xmlNsPtr pNs)` |
|     1 | 1499 | `{` |
|    29 | 1500 | `	const char *zPfx = pNs && pNs->prefix ? (const char *)pNs->prefix : "";` |
|    29 | 1501 | `	int nPfx = (int)SyStrlen(zPfx);` |
|     - | 1502 | `	ph7_value sKey,sVal;` |
|    29 | 1503 | `	if( pNs == 0 \|\| pNs->href == 0 \|\| ph7_array_fetch(pOut,zPfx,nPfx) != 0 ){` |
|    11 | 1504 | `		return;` |
|     - | 1505 | `	}` |
|    19 | 1506 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|    19 | 1507 | `	PH7_MemObjStringAppend(&sKey,zPfx,(sxu32)nPfx);` |
|    19 | 1508 | `	PH7_MemObjInitFromString(pVm,&sVal,0);` |
|    19 | 1509 | `	PH7_MemObjStringAppend(&sVal,(const char *)pNs->href,(sxu32)xmlStrlen(pNs->href));` |
|    19 | 1510 | `	ph7_array_add_elem(pOut,&sKey,&sVal);` |
|    19 | 1511 | `	PH7_MemObjRelease(&sKey);` |
|    19 | 1512 | `	PH7_MemObjRelease(&sVal);` |
|    15 | 1513 | `}` |
|    16 | 1514 | `static void SxeUsedNs(ph7_vm *pVm,xmlNodePtr pNode,int bRecursive,ph7_value *pOut)` |
|     1 | 1515 | `{` |
|     - | 1516 | `	xmlAttrPtr pAttr;` |
|    17 | 1517 | `	if( pNode == 0 ){` |
|   ! 0 | 1518 | `		return;` |
|     - | 1519 | `	}` |
|    17 | 1520 | `	if( pNode->ns ){` |
|    17 | 1521 | `		SxeAddNs(pVm,pOut,pNode->ns);` |
|     8 | 1522 | `	}` |
|    25 | 1523 | `	for( pAttr = pNode->properties ; pAttr ; pAttr = pAttr->next ){` |
|     9 | 1524 | `		if( pAttr->ns ){` |
|     5 | 1525 | `			SxeAddNs(pVm,pOut,pAttr->ns);` |
|     2 | 1526 | `		}` |
|     5 | 1527 | `	}` |
|    17 | 1528 | `	if( bRecursive ){` |
|     - | 1529 | `		xmlNodePtr pWalk;` |
|    29 | 1530 | `		for( pWalk = pNode->children ; pWalk ; pWalk = pWalk->next ){` |
|    17 | 1531 | `			if( pWalk->type == XML_ELEMENT_NODE ){` |
|    11 | 1532 | `				SxeUsedNs(pVm,pWalk,1,pOut);` |
|     5 | 1533 | `			}` |
|     9 | 1534 | `		}` |
|     6 | 1535 | `	}` |
|     9 | 1536 | `}` |
|     6 | 1537 | `SXE_METHOD(vm_builtin_SimpleXMLElement_getNamespaces)` |
|     1 | 1538 | `{` |
|     7 | 1539 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|     7 | 1540 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|     7 | 1541 | `	int bRec = nArg > 0 && ph7_value_to_bool(apArg[0]);` |
|     7 | 1542 | `	if( pOut == 0 ){` |
|   ! 0 | 1543 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1544 | `		return PH7_OK;` |
|     - | 1545 | `	}` |
|     7 | 1546 | `	if( pThis ){` |
|     7 | 1547 | `		SxeUsedNs(pCtx->pVm,SxeMethodNode(pThis),bRec,pOut);` |
|     3 | 1548 | `	}` |
|     7 | 1549 | `	ph7_result_value(pCtx,pOut);` |
|     7 | 1550 | `	return PH7_OK;` |
|     4 | 1551 | `}` |
|     - | 1552 | `/*` |
|     - | 1553 | ` * getDocNamespaces($recursive, $fromRoot) -- the namespaces DECLARED, which is` |
|     - | 1554 | ` * a different list and asked of a different node: this one reads the object's` |
|     - | 1555 | `` * OWN node rather than the node its question finds, so `$x->kid->`` |
|     - | 1556 | `` * getDocNamespaces(false,false)` answers the declarations on `$x`'s node --`` |
|     - | 1557 | ` * the parent -- because that is what an element-set object holds.` |
|     - | 1558 | ` */` |
|     4 | 1559 | `static void SxeDeclaredNs(ph7_vm *pVm,xmlNodePtr pNode,int bRecursive,ph7_value *pOut)` |
|     1 | 1560 | `{` |
|     - | 1561 | `	xmlNsPtr pNs;` |
|     5 | 1562 | `	if( pNode == 0 \|\| pNode->type != XML_ELEMENT_NODE ){` |
|   ! 0 | 1563 | `		return;` |
|     - | 1564 | `	}` |
|    13 | 1565 | `	for( pNs = pNode->nsDef ; pNs ; pNs = pNs->next ){` |
|     9 | 1566 | `		SxeAddNs(pVm,pOut,pNs);` |
|     5 | 1567 | `	}` |
|     5 | 1568 | `	if( bRecursive ){` |
|     - | 1569 | `		xmlNodePtr pWalk;` |
|   ! 0 | 1570 | `		for( pWalk = pNode->children ; pWalk ; pWalk = pWalk->next ){` |
|   ! 0 | 1571 | `			SxeDeclaredNs(pVm,pWalk,1,pOut);` |
|   ! 0 | 1572 | `		}` |
|   ! 0 | 1573 | `	}` |
|     3 | 1574 | `}` |
|     4 | 1575 | `SXE_METHOD(vm_builtin_SimpleXMLElement_getDocNamespaces)` |
|     1 | 1576 | `{` |
|     5 | 1577 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|     5 | 1578 | `	phl_domnode *pNd = pThis ? SxeResOf(pThis) : 0;` |
|     5 | 1579 | `	int bRec = nArg > 0 && ph7_value_to_bool(apArg[0]);` |
|     5 | 1580 | `	int bRoot = nArg > 1 ? ph7_value_to_bool(apArg[1]) : 1;` |
|     5 | 1581 | `	xmlNodePtr pNode = pThis ? SxeNodeOf(pThis) : 0;` |
|     - | 1582 | `	ph7_value *pOut;` |
|     5 | 1583 | `	if( pNode == 0 ){` |
|   ! 0 | 1584 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1585 | `		return PH7_OK;` |
|     - | 1586 | `	}` |
|     5 | 1587 | `	if( bRoot ){` |
|     3 | 1588 | `		xmlDocPtr pDoc = pNd && pNd->pShell ? (xmlDocPtr)pNd->pShell->pDoc : pNode->doc;` |
|     3 | 1589 | `		pNode = pDoc ? xmlDocGetRootElement(pDoc) : 0;` |
|     1 | 1590 | `	}` |
|     5 | 1591 | `	pOut = ph7_context_new_array(pCtx);` |
|     5 | 1592 | `	if( pOut == 0 ){` |
|   ! 0 | 1593 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1594 | `		return PH7_OK;` |
|     - | 1595 | `	}` |
|     5 | 1596 | `	SxeDeclaredNs(pCtx->pVm,pNode,bRec,pOut);` |
|     5 | 1597 | `	ph7_result_value(pCtx,pOut);` |
|     5 | 1598 | `	return PH7_OK;` |
|     3 | 1599 | `}` |
|     - | 1600 |  |
|     - | 1601 | `/*` |
|     - | 1602 | ` * asXML() / saveXML().` |
|     - | 1603 | ` *` |
|     - | 1604 | ` * php dumps the whole DOCUMENT -- declaration, trailing newline and all -- when` |
|     - | 1605 | ` * the node it found is the document's own child, and the NODE alone otherwise.` |
|     - | 1606 | `` * That is one test on the parent, and it is why `$x->asXML()` prints a header`` |
|     - | 1607 | `` * and `$x->kid->asXML()` does not; it is also why the same call on a root that`` |
|     - | 1608 | ` * something UNLINKED prints the bare element.` |
|     - | 1609 | ` */` |
|   122 | 1610 | `static int SxeDumpXml(ph7_class_instance *pThis,SyBlob *pOut)` |
|     1 | 1611 | `{` |
|   123 | 1612 | `	xmlNodePtr pNode = pThis ? SxeMethodNode(pThis) : 0;` |
|   123 | 1613 | `	if( pNode == 0 ){` |
|     3 | 1614 | `		return 0;` |
|     - | 1615 | `	}` |
|   121 | 1616 | `	if( pNode->parent && (pNode->parent->type == XML_DOCUMENT_NODE` |
|    65 | 1617 | `	 \|\| pNode->parent->type == XML_HTML_DOCUMENT_NODE) ){` |
|   111 | 1618 | `		xmlChar *zBuf = 0;` |
|   111 | 1619 | `		int nBuf = 0;` |
|   111 | 1620 | `		xmlDocDumpMemory(pNode->doc,&zBuf,&nBuf);` |
|   111 | 1621 | `		if( zBuf == 0 ){` |
|   ! 0 | 1622 | `			return 0;` |
|     - | 1623 | `		}` |
|   111 | 1624 | `		SyBlobAppend(pOut,(const char *)zBuf,(sxu32)nBuf);` |
|   111 | 1625 | `		xmlFree(zBuf);` |
|   111 | 1626 | `		return 1;` |
|     - | 1627 | `	}` |
|     - | 1628 | `	{` |
|     - | 1629 | `		/* One node, through the same output buffer ext/dom dumps a DTD's children` |
|     - | 1630 | `		 * with -- xmlNodeDump itself is deprecated from libxml 2.12 and the MSVC` |
|     - | 1631 | `		 * gate refuses a deprecated symbol under /WX. */` |
|    11 | 1632 | `		xmlBufferPtr pBuf = xmlBufferCreate();` |
|    11 | 1633 | `		xmlOutputBufferPtr pDump = pBuf ? xmlOutputBufferCreateBuffer(pBuf,0) : 0;` |
|    11 | 1634 | `		if( pDump == 0 ){` |
|   ! 0 | 1635 | `			if( pBuf ){` |
|   ! 0 | 1636 | `				xmlBufferFree(pBuf);` |
|   ! 0 | 1637 | `			}` |
|   ! 0 | 1638 | `			return 0;` |
|     - | 1639 | `		}` |
|    11 | 1640 | `		xmlNodeDumpOutput(pDump,pNode->doc,pNode,0,0,0);` |
|    11 | 1641 | `		xmlOutputBufferFlush(pDump);` |
|    11 | 1642 | `		if( xmlBufferContent(pBuf) ){` |
|    16 | 1643 | `			SyBlobAppend(pOut,(const char *)xmlBufferContent(pBuf),` |
|    10 | 1644 | `				(sxu32)xmlBufferLength(pBuf));` |
|     5 | 1645 | `		}` |
|    11 | 1646 | `		xmlOutputBufferClose(pDump);` |
|    11 | 1647 | `		xmlBufferFree(pBuf);` |
|    11 | 1648 | `		return 1;` |
|     - | 1649 | `	}` |
|    62 | 1650 | `}` |
|   122 | 1651 | `SXE_METHOD(vm_builtin_SimpleXMLElement_asXML)` |
|     1 | 1652 | `{` |
|   123 | 1653 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|     - | 1654 | `	SyBlob sOut;` |
|     - | 1655 | `	int bOk;` |
|   123 | 1656 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|   123 | 1657 | `	bOk = SxeDumpXml(pThis,&sOut);` |
|   123 | 1658 | `	if( !bOk ){` |
|     3 | 1659 | `		SyBlobRelease(&sOut);` |
|     3 | 1660 | `		ph7_result_bool(pCtx,0);` |
|     3 | 1661 | `		return PH7_OK;` |
|     - | 1662 | `	}` |
|   121 | 1663 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0 ){` |
|     - | 1664 | `		/* php writes through its OWN stream layer, so a wrapper and a userland` |
|     - | 1665 | `		 * stream are valid destinations here exactly as they are for` |
|     - | 1666 | `		 * DOMDocument::save(). */` |
|     3 | 1667 | `		int nFile = 0;` |
|     3 | 1668 | `		const char *zFile = ph7_value_to_string(apArg[0],&nFile);` |
|     3 | 1669 | `		const ph7_io_stream *pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nFile);` |
|     3 | 1670 | `		void *pHandle = (pStream && pStream->xWrite)` |
|     3 | 1671 | `			? PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,` |
|     - | 1672 | `				PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC,FALSE,0,FALSE,0,` |
|     2 | 1673 | `				ph7_function_name(pCtx)) : 0;` |
|     3 | 1674 | `		int bWrote = 0;` |
|     3 | 1675 | `		if( pHandle ){` |
|     5 | 1676 | `			bWrote = SyBlobLength(&sOut) == 0` |
|     3 | 1677 | `				\|\| pStream->xWrite(pHandle,(const void *)SyBlobData(&sOut),` |
|     3 | 1678 | `					(ph7_int64)SyBlobLength(&sOut)) >= 0;` |
|     3 | 1679 | `			PH7_StreamCloseHandle(pStream,pHandle);` |
|     1 | 1680 | `		}` |
|     3 | 1681 | `		SyBlobRelease(&sOut);` |
|     3 | 1682 | `		ph7_result_bool(pCtx,bWrote);` |
|     3 | 1683 | `		return PH7_OK;` |
|     - | 1684 | `	}` |
|   119 | 1685 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|   119 | 1686 | `	SyBlobRelease(&sOut);` |
|   119 | 1687 | `	return PH7_OK;` |
|    62 | 1688 | `}` |
|     - | 1689 | `/*` |
|     - | 1690 | ` * xpath(): php anchors the expression at the node the question finds, evaluates` |
|     - | 1691 | ` * it with the prefixes registerXPathNamespace() left on THIS object, and takes` |
|     - | 1692 | ` * only a node set.` |
|     - | 1693 | ` *` |
|     - | 1694 | `` * Anything else -- a `count()`, a `string()`, a comparison -- is a warning and`` |
|     - | 1695 | `` * `false`, named by the type libxml answered with. An attribute list has no`` |
|     - | 1696 | ` * node to anchor at and answers null, and so does a set that found none.` |
|     - | 1697 | ` */` |
|    28 | 1698 | `static int SxeRegisterOne(ph7_value *pKey,ph7_value *pVal,void *pUser)` |
|     1 | 1699 | `{` |
|    29 | 1700 | `	xmlXPathContextPtr pXCtx = (xmlXPathContextPtr)pUser;` |
|    29 | 1701 | `	int nPfx = 0,nUri = 0;` |
|    29 | 1702 | `	const char *zPfx = ph7_value_to_string(pKey,&nPfx);` |
|    29 | 1703 | `	const char *zUri = ph7_value_to_string(pVal,&nUri);` |
|     - | 1704 | `	SyBlob sP,sU;` |
|    29 | 1705 | `	if( nPfx < 1 \|\| pKey->pVm == 0 ){` |
|   ! 0 | 1706 | `		return PH7_OK;` |
|     - | 1707 | `	}` |
|    29 | 1708 | `	SxeCopyZ(pKey->pVm,&sP,zPfx,nPfx);` |
|    29 | 1709 | `	SxeCopyZ(pKey->pVm,&sU,zUri,nUri);` |
|    43 | 1710 | `	xmlXPathRegisterNs(pXCtx,(const xmlChar *)SyBlobData(&sP),` |
|    28 | 1711 | `		(const xmlChar *)SyBlobData(&sU));` |
|    29 | 1712 | `	SyBlobRelease(&sP);` |
|    29 | 1713 | `	SyBlobRelease(&sU);` |
|    29 | 1714 | `	return PH7_OK;` |
|    15 | 1715 | `}` |
|    32 | 1716 | `SXE_METHOD(vm_builtin_SimpleXMLElement_xpath)` |
|     1 | 1717 | `{` |
|    33 | 1718 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|    33 | 1719 | `	xmlNodePtr pNode = pThis ? SxeMethodNode(pThis) : 0;` |
|     - | 1720 | `	xmlXPathContextPtr pXCtx;` |
|     - | 1721 | `	xmlXPathObjectPtr pObj;` |
|     - | 1722 | `	ph7_value *pReg,*pOut;` |
|    33 | 1723 | `	xmlNsPtr *aNs = 0;` |
|    33 | 1724 | `	int nExpr = 0;` |
|    33 | 1725 | `	const char *zExpr = nArg > 0 ? ph7_value_to_string(apArg[0],&nExpr) : "";` |
|     - | 1726 | `	SyBlob sExpr;` |
|     - | 1727 | `	sxu32 nMark;` |
|     - | 1728 | `	int i;` |
|    33 | 1729 | `	if( pThis == 0 \|\| pNode == 0 \|\| SxeTypeOf(pThis) == SXE_ITER_ATTRLIST ){` |
|     5 | 1730 | `		ph7_result_null(pCtx);` |
|     5 | 1731 | `		return PH7_OK;` |
|     - | 1732 | `	}` |
|    29 | 1733 | `	pXCtx = xmlXPathNewContext(pNode->doc);` |
|    29 | 1734 | `	if( pXCtx == 0 ){` |
|   ! 0 | 1735 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1736 | `		return PH7_OK;` |
|     - | 1737 | `	}` |
|    29 | 1738 | `	pXCtx->node = pNode;` |
|    29 | 1739 | `	pReg = PH7_NativeAttr(pThis,SXE_XPNS);` |
|    29 | 1740 | `	if( pReg && (pReg->iFlags & MEMOBJ_HASHMAP) ){` |
|    29 | 1741 | `		ph7_array_walk(pReg,SxeRegisterOne,pXCtx);` |
|    14 | 1742 | `	}` |
|     - | 1743 | `	/* php also puts the CONTEXT NODE's in-scope declarations in front of the` |
|     - | 1744 | ``	 * registered table -- libxml consults `namespaces` before the registry -- so`` |
|     - | 1745 | `	 * a prefix the DOCUMENT declares resolves with no registerXPathNamespace()` |
|     - | 1746 | `	 * at all, and beats one registered under the same prefix. */` |
|    29 | 1747 | `	aNs = xmlGetNsList(pNode->doc,pNode);` |
|    29 | 1748 | `	if( aNs ){` |
|    29 | 1749 | `		int nNs = 0;` |
|    57 | 1750 | `		while( aNs[nNs] ){` |
|    29 | 1751 | `			nNs++;` |
|     1 | 1752 | `		}` |
|    29 | 1753 | `		pXCtx->namespaces = aNs;` |
|    29 | 1754 | `		pXCtx->nsNr = nNs;` |
|    14 | 1755 | `	}` |
|    29 | 1756 | `	SyBlobInit(&sExpr,&pCtx->pVm->sAllocator);` |
|    29 | 1757 | `	if( nExpr > 0 ){` |
|    29 | 1758 | `		SyBlobAppend(&sExpr,zExpr,(sxu32)nExpr);` |
|    14 | 1759 | `	}` |
|    29 | 1760 | `	SyBlobNullAppend(&sExpr);` |
|     - | 1761 | `	/* An expression that does not evaluate is LIBXML's diagnostic, not one php` |
|     - | 1762 | ``	 * writes: `Invalid expression`, `Undefined namespace prefix`. php prints it`` |
|     - | 1763 | `	 * under the method's name, with no source location (an XPath error carries` |
|     - | 1764 | `	 * no line), which is what draining the capture window does here. */` |
|    29 | 1765 | `	nMark = PH7_LibxmlCaptureBegin(pCtx->pVm);` |
|    29 | 1766 | `	pObj = xmlXPathEvalExpression((const xmlChar *)SyBlobData(&sExpr),pXCtx);` |
|    29 | 1767 | `	SyBlobRelease(&sExpr);` |
|    29 | 1768 | `	if( pObj == 0 ){` |
|     3 | 1769 | `		PH7_LibxmlCaptureEnd(pCtx->pVm,nMark,"SimpleXMLElement::xpath");` |
|     3 | 1770 | `		xmlXPathFreeContext(pXCtx);` |
|     3 | 1771 | `		if( aNs ){` |
|     3 | 1772 | `			xmlFree(aNs);` |
|     1 | 1773 | `		}` |
|     3 | 1774 | `		ph7_result_bool(pCtx,0);` |
|     3 | 1775 | `		return PH7_OK;` |
|     - | 1776 | `	}` |
|    27 | 1777 | `	PH7_LibxmlDropErrors(pCtx->pVm,nMark);` |
|    27 | 1778 | `	if( pObj->type != XPATH_NODESET ){` |
|    12 | 1779 | `		const char *zKind = pObj->type == XPATH_NUMBER ? "number"` |
|     6 | 1780 | `			: pObj->type == XPATH_STRING ? "string"` |
|     2 | 1781 | `			: pObj->type == XPATH_BOOLEAN ? "bool" : "unknown";` |
|     7 | 1782 | `		xmlXPathFreeObject(pObj);` |
|     7 | 1783 | `		xmlXPathFreeContext(pXCtx);` |
|     7 | 1784 | `		if( aNs ){` |
|     7 | 1785 | `			xmlFree(aNs);` |
|     3 | 1786 | `		}` |
|    10 | 1787 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     3 | 1788 | `			"XPath expression must return a node set, %s returned",zKind);` |
|     7 | 1789 | `		ph7_result_bool(pCtx,0);` |
|     7 | 1790 | `		return PH7_OK;` |
|     - | 1791 | `	}` |
|    21 | 1792 | `	pOut = ph7_context_new_array(pCtx);` |
|    21 | 1793 | `	if( pOut == 0 ){` |
|   ! 0 | 1794 | `		xmlXPathFreeObject(pObj);` |
|   ! 0 | 1795 | `		xmlXPathFreeContext(pXCtx);` |
|   ! 0 | 1796 | `		if( aNs ){` |
|   ! 0 | 1797 | `			xmlFree(aNs);` |
|   ! 0 | 1798 | `		}` |
|   ! 0 | 1799 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1800 | `		return PH7_OK;` |
|     - | 1801 | `	}` |
|    43 | 1802 | `	for( i = 0 ; pObj->nodesetval && i < pObj->nodesetval->nodeNr ; ++i ){` |
|    23 | 1803 | `		xmlNodePtr pHit = pObj->nodesetval->nodeTab[i];` |
|     - | 1804 | `		ph7_class_instance *pSub;` |
|     - | 1805 | `		ph7_value *pSlot;` |
|    23 | 1806 | `		if( pHit == 0 ){` |
|   ! 0 | 1807 | `			continue;` |
|     - | 1808 | `		}` |
|    23 | 1809 | `		if( pHit->type == XML_ATTRIBUTE_NODE ){` |
|     - | 1810 | `			/* php answers an attribute as the ATTRIBUTE LIST of its owner` |
|     - | 1811 | `			 * narrowed to that name, which is why its table shows an` |
|     - | 1812 | ``			 * `@attributes` row where `$x['a']`'s shows a plain value. The filter`` |
|     - | 1813 | `			 * is the ATTRIBUTE'S OWN namespace, not the receiver's. */` |
|     4 | 1814 | `			const char *zHref = pHit->ns && pHit->ns->href` |
|     4 | 1815 | `				? (const char *)pHit->ns->href : 0;` |
|     5 | 1816 | `			phl_domnode *pNd = SxeResOf(pThis);` |
|    10 | 1817 | `			pSub = SxeNew(pCtx->pVm,pThis->pClass,pNd ? pNd->pShell : 0,pHit->parent,` |
|     4 | 1818 | `				SXE_ITER_ATTRLIST,(const char *)pHit->name,(int)xmlStrlen(pHit->name),` |
|     3 | 1819 | `				zHref,zHref ? (int)SyStrlen(zHref) : 0,0);` |
|    21 | 1820 | `		}else if( pHit->type == XML_TEXT_NODE \|\| pHit->type == XML_CDATA_SECTION_NODE ){` |
|     - | 1821 | `			/* A text node is answered as the element that HOLDS it. */` |
|     7 | 1822 | `			pSub = SxeDerive(pCtx->pVm,pThis,pHit->parent,SXE_ITER_NONE,0,0);` |
|    16 | 1823 | `		}else if( pHit->type == XML_DOCUMENT_NODE \|\| pHit->type == XML_HTML_DOCUMENT_NODE` |
|    11 | 1824 | `		       \|\| pHit->type == XML_NAMESPACE_DECL \|\| pHit->type == XML_DTD_NODE ){` |
|     - | 1825 | `			/* The two a node set can hold that SimpleXML has no object for: the` |
|     - | 1826 | ``			 * document itself (what `/` and a root's `..` select) and a namespace`` |
|     - | 1827 | `			 * declaration. php drops both and answers the shorter array. */` |
|     3 | 1828 | `			continue;` |
|   ! 0 | 1829 | `		}else{` |
|     - | 1830 | `			/* Everything else -- an element, and the comment and processing` |
|     - | 1831 | `			 * instruction php keeps here even though neither is one in its table` |
|     - | 1832 | `			 * of an element's children. */` |
|    11 | 1833 | `			pSub = SxeDerive(pCtx->pVm,pThis,pHit,SXE_ITER_NONE,0,0);` |
|     - | 1834 | `		}` |
|    21 | 1835 | `		if( pSub == 0 ){` |
|   ! 0 | 1836 | `			break;` |
|     - | 1837 | `		}` |
|     - | 1838 | `		/* pSlot CARRIES the constructor's reference (no bump): the array's` |
|     - | 1839 | `		 * insert takes its own, and the context releases pSlot at method end. */` |
|    21 | 1840 | `		pSlot = ph7_context_new_scalar(pCtx);` |
|    21 | 1841 | `		if( pSlot == 0 ){` |
|   ! 0 | 1842 | `			PH7_ClassInstanceUnref(pSub);` |
|   ! 0 | 1843 | `			break;` |
|     - | 1844 | `		}` |
|    21 | 1845 | `		pSlot->x.pOther = pSub;` |
|    21 | 1846 | `		pSlot->iFlags = MEMOBJ_OBJ;` |
|    21 | 1847 | `		ph7_array_add_elem(pOut,0,pSlot);` |
|    11 | 1848 | `	}` |
|    21 | 1849 | `	xmlXPathFreeObject(pObj);` |
|    21 | 1850 | `	xmlXPathFreeContext(pXCtx);` |
|    21 | 1851 | `	if( aNs ){` |
|    21 | 1852 | `		xmlFree(aNs);   /* the xmlNs entries belong to the tree; only the array is ours */` |
|    10 | 1853 | `	}` |
|    21 | 1854 | `	ph7_result_value(pCtx,pOut);` |
|    21 | 1855 | `	return PH7_OK;` |
|    17 | 1856 | `}` |
|     - | 1857 | `/* registerXPathNamespace(): the prefixes THIS object's xpath() knows. They do` |
|     - | 1858 | ` * not travel: a child fetched off this object starts with none, which is php's` |
|     - | 1859 | ` * answer too. */` |
|     4 | 1860 | `SXE_METHOD(vm_builtin_SimpleXMLElement_registerXPathNamespace)` |
|     1 | 1861 | `{` |
|     5 | 1862 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|     - | 1863 | `	ph7_value *pReg;` |
|     5 | 1864 | `	int nPfx = 0,nUri = 0;` |
|     5 | 1865 | `	const char *zPfx = nArg > 0 ? ph7_value_to_string(apArg[0],&nPfx) : "";` |
|     5 | 1866 | `	const char *zUri = nArg > 1 ? ph7_value_to_string(apArg[1],&nUri) : "";` |
|     - | 1867 | `	ph7_value sKey,sVal;` |
|     5 | 1868 | `	if( pThis == 0 ){` |
|   ! 0 | 1869 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1870 | `		return PH7_OK;` |
|     - | 1871 | `	}` |
|     5 | 1872 | `	pReg = PH7_NativeAttr(pThis,SXE_XPNS);` |
|     5 | 1873 | `	if( pReg == 0 ){` |
|   ! 0 | 1874 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1875 | `		return PH7_OK;` |
|     - | 1876 | `	}` |
|     5 | 1877 | `	if( (pReg->iFlags & MEMOBJ_HASHMAP) == 0 && PH7_MemObjToHashmap(pReg) != SXRET_OK ){` |
|   ! 0 | 1878 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1879 | `		return PH7_OK;` |
|     - | 1880 | `	}` |
|     5 | 1881 | `	PH7_MemObjInitFromString(pCtx->pVm,&sKey,0);` |
|     5 | 1882 | `	PH7_MemObjStringAppend(&sKey,zPfx,(sxu32)nPfx);` |
|     5 | 1883 | `	PH7_MemObjInitFromString(pCtx->pVm,&sVal,0);` |
|     5 | 1884 | `	PH7_MemObjStringAppend(&sVal,zUri,(sxu32)nUri);` |
|     5 | 1885 | `	ph7_array_add_elem(pReg,&sKey,&sVal);` |
|     5 | 1886 | `	PH7_MemObjRelease(&sKey);` |
|     5 | 1887 | `	PH7_MemObjRelease(&sVal);` |
|     5 | 1888 | `	ph7_result_bool(pCtx,1);` |
|     5 | 1889 | `	return PH7_OK;` |
|     3 | 1890 | `}` |
|     - | 1891 |  |
|     - | 1892 | `/* ===== addChild / addAttribute ===== */` |
|     - | 1893 |  |
|     - | 1894 | ``/* Split `prefix:local`, php's qualified-name form. */`` |
|    10 | 1895 | `static void SxeSplitQName(const char *zName,int nName,const char **pzPfx,int *pnPfx,` |
|     - | 1896 | `	const char **pzLocal,int *pnLocal)` |
|     1 | 1897 | `{` |
|     - | 1898 | `	int i;` |
|    11 | 1899 | `	*pzPfx = 0;` |
|    11 | 1900 | `	*pnPfx = 0;` |
|    11 | 1901 | `	*pzLocal = zName;` |
|    11 | 1902 | `	*pnLocal = nName;` |
|    21 | 1903 | `	for( i = 0 ; i < nName ; ++i ){` |
|    15 | 1904 | `		if( zName[i] == ':' ){` |
|     5 | 1905 | `			*pzPfx = zName;` |
|     5 | 1906 | `			*pnPfx = i;` |
|     5 | 1907 | `			*pzLocal = zName + i + 1;` |
|     5 | 1908 | `			*pnLocal = nName - i - 1;` |
|     5 | 1909 | `			return;` |
|     - | 1910 | `		}` |
|     6 | 1911 | `	}` |
|     6 | 1912 | `}` |
|     - | 1913 | `/*` |
|     - | 1914 | ` * The namespace a new child or attribute is put in: the one the caller named` |
|     - | 1915 | ` * (declared on the node when the document has no binding for it yet), or the` |
|     - | 1916 | ` * one the qualified name's prefix is already bound to, or none -- in which case` |
|     - | 1917 | ` * libxml gives a child the parent's default binding.` |
|     - | 1918 | ` */` |
|    10 | 1919 | `static xmlNsPtr SxeResolveNs(ph7_vm *pVm,xmlNodePtr pNode,const char *zPfx,int nPfx,` |
|     - | 1920 | `	const char *zHref,int nHref,int bHave)` |
|     1 | 1921 | `{` |
|     - | 1922 | `	SyBlob sP,sH;` |
|    11 | 1923 | `	xmlNsPtr pNs = 0;` |
|    11 | 1924 | `	if( bHave ){` |
|     5 | 1925 | `		SxeCopyZ(pVm,&sH,zHref,nHref);` |
|     5 | 1926 | `		SxeCopyZ(pVm,&sP,zPfx,nPfx);` |
|     5 | 1927 | `		pNs = xmlSearchNsByHref(pNode->doc,pNode,(const xmlChar *)SyBlobData(&sH));` |
|     5 | 1928 | `		if( pNs == 0 ){` |
|     7 | 1929 | `			pNs = xmlNewNs(pNode,(const xmlChar *)SyBlobData(&sH),` |
|     2 | 1930 | `				nPfx > 0 ? (const xmlChar *)SyBlobData(&sP) : 0);` |
|     2 | 1931 | `		}` |
|     5 | 1932 | `		SyBlobRelease(&sH);` |
|     5 | 1933 | `		SyBlobRelease(&sP);` |
|     5 | 1934 | `		return pNs;` |
|     - | 1935 | `	}` |
|     7 | 1936 | `	if( zPfx && nPfx > 0 ){` |
|   ! 0 | 1937 | `		SxeCopyZ(pVm,&sP,zPfx,nPfx);` |
|   ! 0 | 1938 | `		pNs = xmlSearchNs(pNode->doc,pNode,(const xmlChar *)SyBlobData(&sP));` |
|   ! 0 | 1939 | `		SyBlobRelease(&sP);` |
|   ! 0 | 1940 | `	}` |
|     7 | 1941 | `	return pNs;` |
|     6 | 1942 | `}` |
|    10 | 1943 | `SXE_METHOD(vm_builtin_SimpleXMLElement_addChild)` |
|     1 | 1944 | `{` |
|    11 | 1945 | `	ph7_vm *pVm = pCtx->pVm;` |
|    11 | 1946 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|    11 | 1947 | `	xmlNodePtr pNode = pThis ? SxeMethodNode(pThis) : 0;` |
|    11 | 1948 | `	int nQ = 0,nVal = 0,nHref = 0,bHaveNs = 0;` |
|    11 | 1949 | `	const char *zQ = nArg > 0 ? ph7_value_to_string(apArg[0],&nQ) : "";` |
|    11 | 1950 | `	const char *zVal = 0,*zHref = 0,*zPfx,*zLocal;` |
|     - | 1951 | `	int nPfx,nLocal;` |
|     - | 1952 | `	SyBlob sLocal;` |
|     - | 1953 | `	const char *zL;` |
|     - | 1954 | `	xmlNsPtr pNs;` |
|     - | 1955 | `	xmlNodePtr pNew;` |
|     - | 1956 | `	SyBlob sVal;` |
|     - | 1957 | `	sxu32 nMark;` |
|    11 | 1958 | `	if( nQ < 1 ){` |
|     3 | 1959 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1960 | `			"SimpleXMLElement::addChild(): Argument #1 ($qualifiedName) must not be empty");` |
|     - | 1961 | `	}` |
|     9 | 1962 | `	if( pThis && SxeTypeOf(pThis) == SXE_ITER_ATTRLIST ){` |
|     3 | 1963 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Cannot add element to attributes");` |
|     3 | 1964 | `		ph7_result_null(pCtx);` |
|     3 | 1965 | `		return PH7_OK;` |
|     - | 1966 | `	}` |
|     7 | 1967 | `	if( pNode == 0 \|\| pNode->type != XML_ELEMENT_NODE ){` |
|     3 | 1968 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,` |
|     - | 1969 | `			"Cannot add child. Parent is not a permanent member of the XML tree");` |
|     3 | 1970 | `		ph7_result_null(pCtx);` |
|     3 | 1971 | `		return PH7_OK;` |
|     - | 1972 | `	}` |
|     5 | 1973 | `	if( nArg > 1 && (apArg[1]->iFlags & MEMOBJ_NULL) == 0 ){` |
|     5 | 1974 | `		zVal = ph7_value_to_string(apArg[1],&nVal);` |
|     2 | 1975 | `	}` |
|     5 | 1976 | `	if( nArg > 2 && (apArg[2]->iFlags & MEMOBJ_NULL) == 0 ){` |
|     3 | 1977 | `		zHref = ph7_value_to_string(apArg[2],&nHref);` |
|     3 | 1978 | `		bHaveNs = 1;` |
|     1 | 1979 | `	}` |
|     5 | 1980 | `	SxeSplitQName(zQ,nQ,&zPfx,&nPfx,&zLocal,&nLocal);` |
|     5 | 1981 | `	zL = SxeCopyZ(pVm,&sLocal,zLocal,nLocal);` |
|     5 | 1982 | `	SyBlobInit(&sVal,&pVm->sAllocator);` |
|     5 | 1983 | `	if( nVal > 0 ){` |
|     5 | 1984 | `		SyBlobAppend(&sVal,zVal,(sxu32)nVal);` |
|     2 | 1985 | `	}` |
|     5 | 1986 | `	SyBlobNullAppend(&sVal);` |
|     - | 1987 | `	/* php's addChild does NOT escape: libxml PARSES the value for entity` |
|     - | 1988 | ``	 * references, so `'a&b'` is the LIBRARY's answer and not php's -- a`` |
|     - | 1989 | `	 * diagnostic and an empty child under libxml 2.9, the two letters under` |
|     - | 1990 | `	 * 2.13. Both engines answer whatever the libxml they were built against` |
|     - | 1991 | `	 * does, which is why no test pins it. Routed through the per-VM queue like` |
|     - | 1992 | `	 * every other libxml message here, or it prints itself past` |
|     - | 1993 | `	 * error_reporting(). */` |
|     5 | 1994 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     7 | 1995 | `	pNew = xmlNewChild(pNode,0,(const xmlChar *)zL,` |
|     2 | 1996 | `		zVal ? (const xmlChar *)SyBlobData(&sVal) : 0);` |
|     5 | 1997 | `	PH7_LibxmlCaptureEnd(pVm,nMark,"SimpleXMLElement::addChild");` |
|     5 | 1998 | `	SyBlobRelease(&sVal);` |
|     5 | 1999 | `	SyBlobRelease(&sLocal);` |
|     - | 2000 | `	/* The namespace is resolved against the NEW node and declared THERE when the` |
|     - | 2001 | ``	 * document has no binding for it -- `addChild('q:n','v','urn:q')` prints`` |
|     - | 2002 | ``	 * `<q:n xmlns:q="urn:q">`, not a declaration hoisted onto the parent. */`` |
|     5 | 2003 | `	if( pNew ){` |
|     5 | 2004 | `		pNs = SxeResolveNs(pVm,pNew,zPfx,nPfx,zHref,nHref,bHaveNs);` |
|     5 | 2005 | `		if( pNs ){` |
|     3 | 2006 | `			xmlSetNs(pNew,pNs);` |
|     1 | 2007 | `		}` |
|     2 | 2008 | `	}` |
|     5 | 2009 | `	if( pNew == 0 ){` |
|   ! 0 | 2010 | `		ph7_result_null(pCtx);` |
|   ! 0 | 2011 | `		return PH7_OK;` |
|     - | 2012 | `	}` |
|     5 | 2013 | `	return SxeResultObj(pCtx,SxeDerive(pVm,pThis,pNew,SXE_ITER_NONE,0,0));` |
|     6 | 2014 | `}` |
|     8 | 2015 | `SXE_METHOD(vm_builtin_SimpleXMLElement_addAttribute)` |
|     1 | 2016 | `{` |
|     9 | 2017 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|     - | 2018 | `	/* An attribute list writes to the element it lists, not to an attribute:` |
|     - | 2019 | `	 * php reaches its own node here rather than the first match. */` |
|    13 | 2020 | `	xmlNodePtr pNode = pThis` |
|     8 | 2021 | `		? (SxeTypeOf(pThis) == SXE_ITER_ATTRLIST ? SxeNodeOf(pThis) : SxeMethodNode(pThis))` |
|     8 | 2022 | `		: 0;` |
|     9 | 2023 | `	int nQ = 0,nVal = 0,nHref = 0,bHaveNs = 0;` |
|     9 | 2024 | `	const char *zQ = nArg > 0 ? ph7_value_to_string(apArg[0],&nQ) : "";` |
|     9 | 2025 | `	const char *zVal = 0,*zHref = 0,*zPfx,*zLocal;` |
|     - | 2026 | `	int nPfx,nLocal;` |
|     - | 2027 | `	SyBlob sLocal;` |
|     - | 2028 | `	const char *zL;` |
|     - | 2029 | `	xmlNsPtr pNs;` |
|     - | 2030 | `	xmlAttrPtr pAttr;` |
|     - | 2031 | `	SyBlob sVal;` |
|     9 | 2032 | `	if( nQ < 1 ){` |
|   ! 0 | 2033 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 2034 | `			"SimpleXMLElement::addAttribute(): Argument #1 ($qualifiedName) must not be empty");` |
|     - | 2035 | `	}` |
|     9 | 2036 | `	if( pNode == 0 \|\| pNode->type != XML_ELEMENT_NODE ){` |
|     3 | 2037 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Unable to locate parent Element");` |
|     3 | 2038 | `		return PH7_OK;` |
|     - | 2039 | `	}` |
|     7 | 2040 | `	if( nArg > 1 ){` |
|     7 | 2041 | `		zVal = ph7_value_to_string(apArg[1],&nVal);` |
|     3 | 2042 | `	}` |
|     7 | 2043 | `	if( nArg > 2 && (apArg[2]->iFlags & MEMOBJ_NULL) == 0 ){` |
|     3 | 2044 | `		zHref = ph7_value_to_string(apArg[2],&nHref);` |
|     3 | 2045 | `		bHaveNs = 1;` |
|     1 | 2046 | `	}` |
|     7 | 2047 | `	SxeSplitQName(zQ,nQ,&zPfx,&nPfx,&zLocal,&nLocal);` |
|     7 | 2048 | `	zL = SxeCopyZ(pCtx->pVm,&sLocal,zLocal,nLocal);` |
|     7 | 2049 | `	pNs = SxeResolveNs(pCtx->pVm,pNode,zPfx,nPfx,zHref,nHref,bHaveNs);` |
|     7 | 2050 | `	if( xmlHasNsProp(pNode,(const xmlChar *)zL,pNs ? pNs->href : 0) ){` |
|     3 | 2051 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Attribute already exists");` |
|     3 | 2052 | `		SyBlobRelease(&sLocal);` |
|     3 | 2053 | `		return PH7_OK;` |
|     - | 2054 | `	}` |
|     5 | 2055 | `	SyBlobInit(&sVal,&pCtx->pVm->sAllocator);` |
|     5 | 2056 | `	if( nVal > 0 ){` |
|     5 | 2057 | `		SyBlobAppend(&sVal,zVal,(sxu32)nVal);` |
|     2 | 2058 | `	}` |
|     5 | 2059 | `	SyBlobNullAppend(&sVal);` |
|     - | 2060 | ``	/* Unlike addChild, the VALUE is data: php escapes it, so `'a&b'` lands as`` |
|     - | 2061 | ``	 * three characters and comes back out as `a&amp;b`. */`` |
|     4 | 2062 | `	pAttr = pNs ? xmlNewNsProp(pNode,pNs,(const xmlChar *)zL,(const xmlChar *)"")` |
|     3 | 2063 | `		: xmlNewProp(pNode,(const xmlChar *)zL,(const xmlChar *)"");` |
|     5 | 2064 | `	SxeSetText((xmlNodePtr)pAttr,(const char *)SyBlobData(&sVal));` |
|     5 | 2065 | `	SyBlobRelease(&sVal);` |
|     5 | 2066 | `	SyBlobRelease(&sLocal);` |
|     5 | 2067 | `	return PH7_OK;` |
|     5 | 2068 | `}` |
|     - | 2069 |  |
|     - | 2070 | `/* ===== The iterator: php's Iterator and RecursiveIterator, as methods ===== */` |
|     - | 2071 |  |
|     - | 2072 | `/* The object the cursor stands on, or NULL before rewind() and past the end. */` |
|   390 | 2073 | `static ph7_class_instance * SxeCurrent(ph7_class_instance *pThis)` |
|     1 | 2074 | `{` |
|   391 | 2075 | `	return PH7_NativeAttrObj(pThis,SXE_CUR);` |
|     1 | 2076 | `}` |
|   134 | 2077 | `static void SxeSetCurrent(ph7_vm *pVm,ph7_class_instance *pThis,xmlNodePtr pNode)` |
|     1 | 2078 | `{` |
|   135 | 2079 | `	if( pNode == 0 ){` |
|     - | 2080 | `		ph7_value sNull;` |
|    63 | 2081 | `		PH7_MemObjInit(pVm,&sNull);` |
|    63 | 2082 | `		PH7_NativeSetProp(pVm,pThis,SXE_CUR,sizeof(SXE_CUR)-1,&sNull);` |
|    63 | 2083 | `		PH7_MemObjRelease(&sNull);` |
|    63 | 2084 | `		return;` |
|     - | 2085 | `	}` |
|     - | 2086 | `	{` |
|    73 | 2087 | `		ph7_class_instance *pObj = SxeDerive(pVm,pThis,pNode,SXE_ITER_NONE,0,0);` |
|    73 | 2088 | `		if( pObj ){` |
|    73 | 2089 | `			PH7_NativeSetAttrObj(pVm,pThis,SXE_CUR,pObj);` |
|    73 | 2090 | `			PH7_ClassInstanceUnref(pObj);` |
|    36 | 2091 | `		}` |
|     - | 2092 | `	}` |
|    68 | 2093 | `}` |
|    62 | 2094 | `SXE_METHOD(vm_builtin_SimpleXMLElement_rewind)` |
|     1 | 2095 | `{` |
|    63 | 2096 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|    31 | 2097 | `	(void)nArg; (void)apArg;` |
|    63 | 2098 | `	if( pThis ){` |
|    63 | 2099 | `		SxeSetCurrent(pCtx->pVm,pThis,SxeIterFirst(pThis));` |
|    31 | 2100 | `	}` |
|    63 | 2101 | `	return PH7_OK;` |
|     1 | 2102 | `}` |
|   144 | 2103 | `SXE_METHOD(vm_builtin_SimpleXMLElement_valid)` |
|     1 | 2104 | `{` |
|   145 | 2105 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|    72 | 2106 | `	(void)nArg; (void)apArg;` |
|   145 | 2107 | `	ph7_result_bool(pCtx,pThis && SxeCurrent(pThis) != 0);` |
|   145 | 2108 | `	return PH7_OK;` |
|     1 | 2109 | `}` |
|     - | 2110 | `/* php refuses to answer current()/key() with no cursor -- before rewind() and` |
|     - | 2111 | ` * after the walk ran out -- and the refusal is an Error, not a null. */` |
|   146 | 2112 | `static ph7_class_instance * SxeCursorOrThrow(ph7_context *pCtx,ph7_class_instance *pThis,` |
|     - | 2113 | `	sxi32 *pRc)` |
|     1 | 2114 | `{` |
|   147 | 2115 | `	ph7_class_instance *pCur = pThis ? SxeCurrent(pThis) : 0;` |
|   147 | 2116 | `	*pRc = PH7_OK;` |
|   147 | 2117 | `	if( pCur == 0 ){` |
|     3 | 2118 | `		*pRc = PH7_VmThrowException(pCtx,"Error",` |
|     - | 2119 | `			"Iterator not initialized or already consumed");` |
|     1 | 2120 | `	}` |
|   147 | 2121 | `	return pCur;` |
|     1 | 2122 | `}` |
|    74 | 2123 | `SXE_METHOD(vm_builtin_SimpleXMLElement_current)` |
|     1 | 2124 | `{` |
|    75 | 2125 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|     - | 2126 | `	sxi32 rc;` |
|    75 | 2127 | `	ph7_class_instance *pCur = SxeCursorOrThrow(pCtx,pThis,&rc);` |
|    37 | 2128 | `	(void)nArg; (void)apArg;` |
|    75 | 2129 | `	if( pCur == 0 ){` |
|     3 | 2130 | `		return rc;` |
|     - | 2131 | `	}` |
|    73 | 2132 | `	return SxeResultBorrowed(pCtx,pCur);` |
|    38 | 2133 | `}` |
|    72 | 2134 | `SXE_METHOD(vm_builtin_SimpleXMLElement_key)` |
|     1 | 2135 | `{` |
|    73 | 2136 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|     - | 2137 | `	sxi32 rc;` |
|    73 | 2138 | `	ph7_class_instance *pCur = SxeCursorOrThrow(pCtx,pThis,&rc);` |
|     - | 2139 | `	xmlNodePtr pNode;` |
|    36 | 2140 | `	(void)nArg; (void)apArg;` |
|    73 | 2141 | `	if( pCur == 0 ){` |
|   ! 0 | 2142 | `		return rc;` |
|     - | 2143 | `	}` |
|    73 | 2144 | `	pNode = SxeNodeOf(pCur);` |
|    73 | 2145 | `	if( pNode == 0 \|\| pNode->name == 0 ){` |
|   ! 0 | 2146 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 | 2147 | `		return PH7_OK;` |
|     - | 2148 | `	}` |
|    73 | 2149 | `	ph7_result_string(pCtx,(const char *)pNode->name,(int)xmlStrlen(pNode->name));` |
|    73 | 2150 | `	return PH7_OK;` |
|    37 | 2151 | `}` |
|    72 | 2152 | `SXE_METHOD(vm_builtin_SimpleXMLElement_next)` |
|     1 | 2153 | `{` |
|    73 | 2154 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|    73 | 2155 | `	ph7_class_instance *pCur = pThis ? SxeCurrent(pThis) : 0;` |
|    36 | 2156 | `	(void)nArg; (void)apArg;` |
|    73 | 2157 | `	if( pThis == 0 ){` |
|   ! 0 | 2158 | `		return PH7_OK;` |
|     - | 2159 | `	}` |
|    73 | 2160 | `	SxeSetCurrent(pCtx->pVm,pThis,pCur ? SxeIterNext(pThis,SxeNodeOf(pCur)) : 0);` |
|    73 | 2161 | `	return PH7_OK;` |
|    37 | 2162 | `}` |
|     - | 2163 | `/* RecursiveIterator: does the node the cursor stands on have element children,` |
|     - | 2164 | ` * and the node itself as the thing to descend into. An attribute list never` |
|     - | 2165 | ` * has either. */` |
|    14 | 2166 | `SXE_METHOD(vm_builtin_SimpleXMLElement_hasChildren)` |
|     1 | 2167 | `{` |
|    15 | 2168 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|    15 | 2169 | `	ph7_class_instance *pCur = pThis ? SxeCurrent(pThis) : 0;` |
|     - | 2170 | `	xmlNodePtr pWalk;` |
|     7 | 2171 | `	(void)nArg; (void)apArg;` |
|    15 | 2172 | `	if( pCur == 0 \|\| SxeTypeOf(pThis) == SXE_ITER_ATTRLIST ){` |
|   ! 0 | 2173 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2174 | `		return PH7_OK;` |
|     - | 2175 | `	}` |
|    15 | 2176 | `	pWalk = SxeNodeOf(pCur);` |
|    25 | 2177 | `	for( pWalk = pWalk ? pWalk->children : 0 ; pWalk ; pWalk = pWalk->next ){` |
|    15 | 2178 | `		if( pWalk->type == XML_ELEMENT_NODE ){` |
|     5 | 2179 | `			ph7_result_bool(pCtx,1);` |
|     5 | 2180 | `			return PH7_OK;` |
|     - | 2181 | `		}` |
|     6 | 2182 | `	}` |
|    11 | 2183 | `	ph7_result_bool(pCtx,0);` |
|    11 | 2184 | `	return PH7_OK;` |
|     8 | 2185 | `}` |
|    14 | 2186 | `SXE_METHOD(vm_builtin_SimpleXMLElement_getChildren)` |
|     1 | 2187 | `{` |
|    15 | 2188 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|    15 | 2189 | `	ph7_class_instance *pCur = pThis ? SxeCurrent(pThis) : 0;` |
|     7 | 2190 | `	(void)nArg; (void)apArg;` |
|    15 | 2191 | `	if( pCur == 0 \|\| SxeTypeOf(pThis) == SXE_ITER_ATTRLIST ){` |
|   ! 0 | 2192 | `		ph7_result_null(pCtx);` |
|   ! 0 | 2193 | `		return PH7_OK;` |
|     - | 2194 | `	}` |
|    15 | 2195 | `	return SxeResultBorrowed(pCtx,pCur);` |
|     8 | 2196 | `}` |
|     - | 2197 |  |
|     - | 2198 | `/* ===== Loading ===== */` |
|     - | 2199 |  |
|     - | 2200 | `/*` |
|     - | 2201 | ` * Parse bytes into a document, with php's error routing: libxml's diagnostics` |
|     - | 2202 | ``  * go through the per-VM queue -- which is what `libxml_use_internal_errors()` `` |
|     - | 2203 | `` * turns off and what `@` and error_reporting() screen -- rather than onto`` |
|     - | 2204 | ` * stderr.  Answers the registered shell, or 0.` |
|     - | 2205 | ` */` |
|   174 | 2206 | `static phl_xmldoc * SxeParse(ph7_context *pCtx,const char *zSrc,int nLen,` |
|     - | 2207 | `	const char *zUrl,int iOpts,const char *zFn)` |
|     4 | 2208 | `{` |
|   178 | 2209 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 2210 | `	xmlDocPtr pDoc;` |
|     - | 2211 | `	phl_xmldoc *pShell;` |
|   178 | 2212 | `	sxu32 nMark = PH7_LibxmlCaptureBeginRaw(pVm);` |
|   178 | 2213 | `	pDoc = xmlReadMemory(zSrc,nLen,zUrl,0,iOpts);` |
|   178 | 2214 | `	PH7_LibxmlCaptureEndOpts(pVm,nMark,zFn,iOpts);` |
|   178 | 2215 | `	if( pDoc == 0 ){` |
|    11 | 2216 | `		return 0;` |
|     - | 2217 | `	}` |
|   168 | 2218 | `	if( xmlDocGetRootElement(pDoc) == 0 ){` |
|     - | 2219 | `		/* php answers false for a document with no element to stand on. */` |
|   ! 0 | 2220 | `		xmlFreeDoc(pDoc);` |
|   ! 0 | 2221 | `		return 0;` |
|     - | 2222 | `	}` |
|   168 | 2223 | `	pShell = PH7_LibxmlNewDoc(pVm,pDoc);` |
|   168 | 2224 | `	if( pShell == 0 ){` |
|   ! 0 | 2225 | `		xmlFreeDoc(pDoc);` |
|   ! 0 | 2226 | `	}` |
|   168 | 2227 | `	return pShell;` |
|    91 | 2228 | `}` |
|     - | 2229 | `/* The class the two loaders build, screened the way php screens it: null is` |
|     - | 2230 | ` * SimpleXMLElement, and anything that is not a SUBCLASS of it is a TypeError` |
|     - | 2231 | ` * naming the argument. */` |
|   184 | 2232 | `static ph7_class * SxeArgClass(ph7_context *pCtx,ph7_value *pVal,const char *zFn,sxi32 *pRc)` |
|     4 | 2233 | `{` |
|   188 | 2234 | `	ph7_vm *pVm = pCtx->pVm;` |
|   188 | 2235 | `	ph7_class *pBase = PH7_VmExtractClass(pVm,"SimpleXMLElement",` |
|     - | 2236 | `		sizeof("SimpleXMLElement")-1,FALSE,0);` |
|     - | 2237 | `	ph7_class *pClass;` |
|   188 | 2238 | `	int nName = 0;` |
|     - | 2239 | `	const char *zName;` |
|   188 | 2240 | `	*pRc = PH7_OK;` |
|   188 | 2241 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|   178 | 2242 | `		return pBase;` |
|     - | 2243 | `	}` |
|    13 | 2244 | `	zName = ph7_value_to_string(pVal,&nName);` |
|    13 | 2245 | `	pClass = PH7_VmExtractClass(pVm,zName,(sxu32)nName,TRUE,0);` |
|    13 | 2246 | `	if( pClass == 0 \|\| pBase == 0 \|\| !PH7_VmInstanceOf(pClass,pBase) ){` |
|     7 | 2247 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 2248 | `			"%s(): Argument #2 ($class_name) must be a class name derived from "` |
|     2 | 2249 | `			"SimpleXMLElement or null, %.*s given",zFn,nName,zName);` |
|     5 | 2250 | `		return 0;` |
|     - | 2251 | `	}` |
|     9 | 2252 | `	return pClass;` |
|    96 | 2253 | `}` |
|     - | 2254 | `/* The root object of a freshly parsed document. */` |
|   160 | 2255 | `static ph7_class_instance * SxeRootObject(ph7_vm *pVm,ph7_class *pClass,phl_xmldoc *pShell,` |
|     - | 2256 | `	const char *zNs,int nNs,int bPrefix)` |
|     4 | 2257 | `{` |
|   164 | 2258 | `	xmlNodePtr pRoot = xmlDocGetRootElement((xmlDocPtr)pShell->pDoc);` |
|   164 | 2259 | `	return SxeNew(pVm,pClass,pShell,pRoot,SXE_ITER_NONE,0,0,zNs,nNs,bPrefix);` |
|     4 | 2260 | `}` |
|     - | 2261 | `/*` |
|     - | 2262 | ` * The shared body of simplexml_load_string() / simplexml_load_file() and of` |
|     - | 2263 | ` * SimpleXMLElement::__construct(): the same five arguments, and the only` |
|     - | 2264 | ` * difference is what a failure IS -- false from the functions, and the` |
|     - | 2265 | `` * constructor's `Exception: String could not be parsed as XML`.`` |
|     - | 2266 | ` */` |
|   174 | 2267 | `static int SxeLoad(ph7_context *pCtx,int nArg,ph7_value **apArg,int bFile,` |
|     - | 2268 | `	const char *zFn,ph7_class *pClass,ph7_class_instance *pInto)` |
|     4 | 2269 | `{` |
|   178 | 2270 | `	ph7_vm *pVm = pCtx->pVm;` |
|   178 | 2271 | `	int nSrc = 0,nNs = 0,bPrefix = 0,iOpts = 0;` |
|   178 | 2272 | `	const char *zSrc = nArg > 0 ? ph7_value_to_string(apArg[0],&nSrc) : "";` |
|   178 | 2273 | `	const char *zNs = 0;` |
|     - | 2274 | `	phl_xmldoc *pShell;` |
|     - | 2275 | `	SyBlob sBody,sPath;` |
|   178 | 2276 | `	int iNsArg = pInto ? 3 : 3;` |
|   178 | 2277 | `	if( nArg > (pInto ? 1 : 2) ){` |
|     6 | 2278 | `		iOpts = ph7_value_to_int(apArg[pInto ? 1 : 2]);` |
|     2 | 2279 | `	}` |
|   178 | 2280 | `	if( nArg > iNsArg ){` |
|   ! 0 | 2281 | `		int n = 0;` |
|   ! 0 | 2282 | `		const char *z = ph7_value_to_string(apArg[iNsArg],&n);` |
|   ! 0 | 2283 | `		if( n > 0 ){` |
|   ! 0 | 2284 | `			zNs = z;` |
|   ! 0 | 2285 | `			nNs = n;` |
|   ! 0 | 2286 | `		}` |
|   ! 0 | 2287 | `	}` |
|   178 | 2288 | `	if( nArg > iNsArg + 1 ){` |
|   ! 0 | 2289 | `		bPrefix = ph7_value_to_bool(apArg[iNsArg + 1]);` |
|   ! 0 | 2290 | `	}` |
|   178 | 2291 | `	if( bFile ){` |
|   ! 0 | 2292 | `		if( !PH7_DomReadFile(pCtx,zSrc,nSrc,zFn,&sBody,&sPath) ){` |
|   ! 0 | 2293 | `			return 0;` |
|     - | 2294 | `		}` |
|   ! 0 | 2295 | `		pShell = SxeParse(pCtx,(const char *)SyBlobData(&sBody),(int)SyBlobLength(&sBody),` |
|   ! 0 | 2296 | `			(const char *)SyBlobData(&sPath),iOpts,zFn);` |
|   ! 0 | 2297 | `		SyBlobRelease(&sBody);` |
|   ! 0 | 2298 | `		SyBlobRelease(&sPath);` |
|   ! 0 | 2299 | `	}else{` |
|   178 | 2300 | `		pShell = SxeParse(pCtx,zSrc,nSrc,0,iOpts,zFn);` |
|     - | 2301 | `	}` |
|   178 | 2302 | `	if( pShell == 0 ){` |
|    11 | 2303 | `		return 0;` |
|     - | 2304 | `	}` |
|   168 | 2305 | `	if( pInto ){` |
|     - | 2306 | `		/* The constructor REPOINTS the object it was called on rather than` |
|     - | 2307 | `		 * making a second one, so a subclass's own constructor may have run` |
|     - | 2308 | `		 * first and its state survives. */` |
|     6 | 2309 | `		xmlNodePtr pRoot = xmlDocGetRootElement((xmlDocPtr)pShell->pDoc);` |
|     6 | 2310 | `		phl_domnode *pRes = (phl_domnode *)SyMemBackendAlloc(&pVm->sAllocator,` |
|     - | 2311 | `			sizeof(phl_domnode));` |
|     - | 2312 | `		ph7_value sVal;` |
|     6 | 2313 | `		if( pRes == 0 ){` |
|   ! 0 | 2314 | `			return 0;` |
|     - | 2315 | `		}` |
|     6 | 2316 | `		pRes->pShell = pShell;` |
|     6 | 2317 | `		pRes->pNode = pRoot;` |
|     6 | 2318 | `		PH7_MemObjInit(pVm,&sVal);` |
|     6 | 2319 | `		sVal.x.pOther = pRes;` |
|     6 | 2320 | `		sVal.iFlags = MEMOBJ_RES;` |
|     6 | 2321 | `		PH7_NativeSetProp(pVm,pInto,SXE_RES,sizeof(SXE_RES)-1,&sVal);` |
|     6 | 2322 | `		PH7_NativeSetAttrInt(pVm,pInto,SXE_IT,SXE_ITER_NONE);` |
|     6 | 2323 | `		if( zNs ){` |
|   ! 0 | 2324 | `			PH7_NativeSetAttrStr(pVm,pInto,SXE_NS,zNs,(sxu32)nNs);` |
|   ! 0 | 2325 | `			PH7_NativeSetAttrInt(pVm,pInto,SXE_ISP,bPrefix ? 1 : 0);` |
|   ! 0 | 2326 | `		}` |
|     6 | 2327 | `		return 1;` |
|     - | 2328 | `	}` |
|     - | 2329 | `	{` |
|   164 | 2330 | `		ph7_class_instance *pObj = SxeRootObject(pVm,pClass,pShell,zNs,nNs,bPrefix);` |
|   164 | 2331 | `		if( pObj == 0 ){` |
|   ! 0 | 2332 | `			return 0;` |
|     - | 2333 | `		}` |
|   164 | 2334 | `		SxeResultObj(pCtx,pObj);` |
|   164 | 2335 | `		return 1;` |
|     - | 2336 | `	}` |
|    91 | 2337 | `}` |
|     8 | 2338 | `SXE_METHOD(vm_builtin_SimpleXMLElement_construct)` |
|     2 | 2339 | `{` |
|    10 | 2340 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    10 | 2341 | `	int bUrl = nArg > 2 && ph7_value_to_bool(apArg[2]);` |
|    10 | 2342 | `	if( pThis == 0 ){` |
|   ! 0 | 2343 | `		return PH7_OK;` |
|     - | 2344 | `	}` |
|    10 | 2345 | `	if( SxeLoad(pCtx,nArg,apArg,bUrl,"SimpleXMLElement::__construct",0,pThis) ){` |
|     6 | 2346 | `		return PH7_OK;` |
|     - | 2347 | `	}` |
|     5 | 2348 | `	return PH7_VmThrowException(pCtx,"Exception","String could not be parsed as XML");` |
|     6 | 2349 | `}` |
|   170 | 2350 | `static int SxeLoadFunc(ph7_context *pCtx,int nArg,ph7_value **apArg,int bFile,const char *zFn)` |
|     4 | 2351 | `{` |
|     - | 2352 | `	sxi32 rc;` |
|   174 | 2353 | `	ph7_class *pClass = SxeArgClass(pCtx,nArg > 1 ? apArg[1] : 0,zFn,&rc);` |
|   174 | 2354 | `	if( pClass == 0 ){` |
|     5 | 2355 | `		return rc;` |
|     - | 2356 | `	}` |
|   170 | 2357 | `	if( !SxeLoad(pCtx,nArg,apArg,bFile,zFn,pClass,0) ){` |
|     7 | 2358 | `		ph7_result_bool(pCtx,0);` |
|     3 | 2359 | `	}` |
|   170 | 2360 | `	return PH7_OK;` |
|    89 | 2361 | `}` |
|   170 | 2362 | `static int vm_builtin_simplexml_load_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 | 2363 | `{` |
|   174 | 2364 | `	return SxeLoadFunc(pCtx,nArg,apArg,0,"simplexml_load_string");` |
|     4 | 2365 | `}` |
|   ! 0 | 2366 | `static int vm_builtin_simplexml_load_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2367 | `{` |
|   ! 0 | 2368 | `	return SxeLoadFunc(pCtx,nArg,apArg,1,"simplexml_load_file");` |
|   ! 0 | 2369 | `}` |
|     - | 2370 |  |
|     - | 2371 | `/* ===== The two doors between ext/simplexml and ext/dom ===== */` |
|     - | 2372 |  |
|     - | 2373 | `/*` |
|     - | 2374 | ` * simplexml_import_dom(object $node, ?string $class_name = 'SimpleXMLElement')` |
|     - | 2375 | ` *` |
|     - | 2376 | ` * The two extensions share one libxml tree, so an import is a second VIEW of it` |
|     - | 2377 | ` * and not a copy: a write through either shows in the other. php takes a` |
|     - | 2378 | ` * document (standing on its root element), an element and an attribute, and` |
|     - | 2379 | ` * warns for anything else.  It caches nothing -- two imports of one document` |
|     - | 2380 | ` * are two objects, unlike dom_import_simplexml's one.` |
|     - | 2381 | ` */` |
|    16 | 2382 | `static int vm_builtin_simplexml_import_dom(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 2383 | `{` |
|    18 | 2384 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 2385 | `	ph7_class_instance *pArg;` |
|     - | 2386 | `	ph7_value *pRes;` |
|     - | 2387 | `	phl_domnode *pNd;` |
|     - | 2388 | `	xmlNodePtr pNode;` |
|     - | 2389 | `	ph7_class *pClass;` |
|     - | 2390 | `	sxi32 rc;` |
|    18 | 2391 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 | 2392 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 2393 | `			"simplexml_import_dom(): Argument #1 ($node) must be a valid XML node");` |
|     - | 2394 | `	}` |
|    18 | 2395 | `	pArg = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    18 | 2396 | `	pRes = pArg ? PH7_NativeAttr(pArg,"__res") : 0;` |
|    18 | 2397 | `	if( pRes == 0 \|\| (pRes->iFlags & MEMOBJ_RES) == 0 ){` |
|     3 | 2398 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 2399 | `			"simplexml_import_dom(): Argument #1 ($node) must be a valid XML node");` |
|     - | 2400 | `	}` |
|    16 | 2401 | `	pClass = SxeArgClass(pCtx,nArg > 1 ? apArg[1] : 0,"simplexml_import_dom",&rc);` |
|    16 | 2402 | `	if( pClass == 0 ){` |
|   ! 0 | 2403 | `		return rc;` |
|     - | 2404 | `	}` |
|    16 | 2405 | `	pNd = (phl_domnode *)pRes->x.pOther;` |
|    16 | 2406 | `	pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    16 | 2407 | `	if( pNode && (pNode->type == XML_DOCUMENT_NODE \|\| pNode->type == XML_HTML_DOCUMENT_NODE) ){` |
|    16 | 2408 | `		pNode = xmlDocGetRootElement((xmlDocPtr)pNode);` |
|     7 | 2409 | `	}` |
|    14 | 2410 | `	if( pNode == 0` |
|    15 | 2411 | `	 \|\| (pNode->type != XML_ELEMENT_NODE && pNode->type != XML_ATTRIBUTE_NODE) ){` |
|     3 | 2412 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid Nodetype to import");` |
|     3 | 2413 | `		ph7_result_null(pCtx);` |
|     3 | 2414 | `		return PH7_OK;` |
|     - | 2415 | `	}` |
|    14 | 2416 | `	return SxeResultObj(pCtx,SxeNew(pVm,pClass,pNd->pShell,pNode,SXE_ITER_NONE,0,0,0,0,0));` |
|    10 | 2417 | `}` |
|     - | 2418 | `/*` |
|     - | 2419 | ` * dom_import_simplexml(object $node): DOMAttr\|DOMElement   -- the 2004 tree` |
|     - | 2420 | ` * Dom\import_simplexml(object $node): Dom\Attr\|Dom\Element -- php 8.4's` |
|     - | 2421 | ` *` |
|     - | 2422 | ` * ext/dom's half of the same door, declared by that extension and bodied here` |
|     - | 2423 | ` * because it is a SimpleXML object it takes apart. php 8.4 put a second class` |
|     - | 2424 | ` * tree over the same libxml nodes and gave it its own door rather than a flag,` |
|     - | 2425 | ` * so this is one body under two names.` |
|     - | 2426 | ` *` |
|     - | 2427 | ` * Unlike its opposite this one has an IDENTITY: two imports of the same node` |
|     - | 2428 | ` * are the same DOM object, because ext/dom caches its wrappers per document.` |
|     - | 2429 | ` *` |
|     - | 2430 | ` * And the two doors do not share a tree. The FIRST one to run over a document` |
|     - | 2431 | ` * latches it, and the other then refuses that whole document -- every node of` |
|     - | 2432 | `` * it, not the one asked about -- with `must not be already imported as a ...`.`` |
|     - | 2433 | ``  * A document made by php 8.4's producers arrives latched; a `new DOMDocument` `` |
|     - | 2434 | ` * does not, so the modern door still answers there, and what it answers is a` |
|     - | 2435 | `` * `Dom\Element` whose `ownerDocument` is that DOMDocument. The wrapper class is`` |
|     - | 2436 | ` * the door's choice and the owner is the cache's, and they really can disagree.` |
|     - | 2437 | ` */` |
|    42 | 2438 | `static int SxeDomImport(ph7_context *pCtx,int nArg,ph7_value **apArg,int bModern)` |
|     2 | 2439 | `{` |
|    44 | 2440 | `	const char *zFn = bModern ? "Dom\\import_simplexml" : "dom_import_simplexml";` |
|     - | 2441 | `	ph7_class_instance *pArg;` |
|     - | 2442 | `	phl_domnode *pNd;` |
|     - | 2443 | `	xmlNodePtr pNode;` |
|     - | 2444 | `	ph7_class_instance *pObj;` |
|     - | 2445 | `	ph7_value sRes;` |
|    44 | 2446 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 | 2447 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|   ! 0 | 2448 | `			"%s(): Argument #1 ($node) is not a valid node type",zFn);` |
|     - | 2449 | `	}` |
|    44 | 2450 | `	pArg = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    44 | 2451 | `	pNd = pArg ? SxeResOf(pArg) : 0;` |
|    44 | 2452 | `	pNode = pArg ? SxeMethodNode(pArg) : 0;` |
|    42 | 2453 | `	if( pNd == 0 \|\| pNode == 0` |
|    40 | 2454 | `	 \|\| (pNode->type != XML_ELEMENT_NODE && pNode->type != XML_ATTRIBUTE_NODE) ){` |
|     8 | 2455 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     2 | 2456 | `			"%s(): Argument #1 ($node) is not a valid node type",zFn);` |
|     - | 2457 | `	}` |
|    40 | 2458 | `	if( pNd->pShell ){` |
|    40 | 2459 | `		int iWant = bModern ? 2 : 1;` |
|    40 | 2460 | `		if( pNd->pShell->iSxFamily != 0 && pNd->pShell->iSxFamily != iWant ){` |
|    19 | 2461 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 2462 | `				"%s(): Argument #1 ($node) must not be already imported as a %s",` |
|     6 | 2463 | `				zFn,bModern ? "DOMNode" : "Dom\\Node");` |
|     - | 2464 | `		}` |
|     - | 2465 | `		/* The latch is taken even when the cache answers with the OTHER tree's` |
|     - | 2466 | `		 * object: php sets it before it looks, so a modern import that hands` |
|     - | 2467 | `		 * back a DOMElement still shuts the 2004 door behind it. */` |
|    28 | 2468 | `		pNd->pShell->iSxFamily = iWant;` |
|    13 | 2469 | `	}` |
|    28 | 2470 | `	pObj = PH7_DomWrapForeign(pCtx->pVm,pNd->pShell,pNode,bModern);` |
|    28 | 2471 | `	if( pObj == 0 ){` |
|   ! 0 | 2472 | `		ph7_result_null(pCtx);` |
|   ! 0 | 2473 | `		return PH7_OK;` |
|     - | 2474 | `	}` |
|    28 | 2475 | `	PH7_MemObjInit(pCtx->pVm,&sRes);` |
|    28 | 2476 | `	sRes.x.pOther = pObj;` |
|    28 | 2477 | `	sRes.iFlags = MEMOBJ_OBJ;` |
|    28 | 2478 | `	ph7_result_value(pCtx,&sRes);   /* takes its own reference... */` |
|    28 | 2479 | `	PH7_ClassInstanceUnref(pObj);   /* ...and the wrap's goes back */` |
|    28 | 2480 | `	return PH7_OK;` |
|    23 | 2481 | `}` |
|    22 | 2482 | `static int vm_builtin_dom_import_simplexml(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 2483 | `{` |
|    24 | 2484 | `	return SxeDomImport(pCtx,nArg,apArg,0);` |
|     2 | 2485 | `}` |
|    20 | 2486 | `static int vm_builtin_Dom_import_simplexml(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2487 | `{` |
|    21 | 2488 | `	return SxeDomImport(pCtx,nArg,apArg,1);` |
|     1 | 2489 | `}` |
|     - | 2490 |  |
|     - | 2491 | `/* ===== Install ===== */` |
|     - | 2492 |  |
|     - | 2493 | `/*` |
|     - | 2494 | `` * `clone $x` on a SimpleXML object copies the DOCUMENT, exactly as ext/dom's`` |
|     - | 2495 | ` * document clone does: php's SimpleXML clone_obj duplicates the tree so a write` |
|     - | 2496 | ` * through the copy does not reach the original.` |
|     - | 2497 | ` */` |
|   ! 0 | 2498 | `static void SxeClone(ph7_vm *pVm,ph7_class_instance *pCopy,ph7_class_instance *pSrc)` |
|   ! 0 | 2499 | `{` |
|   ! 0 | 2500 | `	phl_domnode *pNd = SxeResOf(pSrc);` |
|   ! 0 | 2501 | `	xmlDocPtr pDoc = pNd && pNd->pShell ? (xmlDocPtr)pNd->pShell->pDoc : 0;` |
|   ! 0 | 2502 | `	xmlDocPtr pNew = pDoc ? xmlCopyDoc(pDoc,1) : 0;` |
|   ! 0 | 2503 | `	phl_xmldoc *pShell = pNew ? PH7_LibxmlNewDoc(pVm,pNew) : 0;` |
|     - | 2504 | `	phl_domnode *pRes;` |
|     - | 2505 | `	ph7_value sVal;` |
|   ! 0 | 2506 | `	if( pShell == 0 ){` |
|   ! 0 | 2507 | `		if( pNew ){` |
|   ! 0 | 2508 | `			xmlFreeDoc(pNew);` |
|   ! 0 | 2509 | `		}` |
|   ! 0 | 2510 | `		return;` |
|     - | 2511 | `	}` |
|   ! 0 | 2512 | `	pRes = (phl_domnode *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_domnode));` |
|   ! 0 | 2513 | `	if( pRes == 0 ){` |
|   ! 0 | 2514 | `		return;` |
|     - | 2515 | `	}` |
|   ! 0 | 2516 | `	pRes->pShell = pShell;` |
|     - | 2517 | `	/* Only the ROOT survives a document copy identifiably; php's clone of a` |
|     - | 2518 | `	 * deeper object stands on the copy's root too. */` |
|   ! 0 | 2519 | `	pRes->pNode = xmlDocGetRootElement(pNew);` |
|   ! 0 | 2520 | `	PH7_MemObjInit(pVm,&sVal);` |
|   ! 0 | 2521 | `	sVal.x.pOther = pRes;` |
|   ! 0 | 2522 | `	sVal.iFlags = MEMOBJ_RES;` |
|   ! 0 | 2523 | `	PH7_NativeSetProp(pVm,pCopy,SXE_RES,sizeof(SXE_RES)-1,&sVal);` |
|   ! 0 | 2524 | `}` |
|     - | 2525 | `/*` |
|     - | 2526 | ` * php's compare handler for the class -- and it is NODE IDENTITY, not the` |
|     - | 2527 | ` * property table the rest of its surface shows.` |
|     - | 2528 | ` *` |
|     - | 2529 | `` * So `$x->kid == $x->kid` is true (both stand on the same element and ask the`` |
|     - | 2530 | ` * same question of it) while two documents parsed from the SAME BYTES are never` |
|     - | 2531 | ` * equal, and neither are two objects over different nodes however alike their` |
|     - | 2532 | ` * tables. An object with no node equals another with no node and nothing else.` |
|     - | 2533 | ` */` |
|    12 | 2534 | `static void SxeCmp(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeCmpCtx *pCtx)` |
|     1 | 2535 | `{` |
|     - | 2536 | `	ph7_class_instance *pOther;` |
|     - | 2537 | `	xmlNodePtr pA,pB;` |
|     6 | 2538 | `	(void)pVm;` |
|    13 | 2539 | `	pOther = pCtx->pOther;` |
|    13 | 2540 | `	if( pOther == 0 \|\| PH7_NativeAttr(pOther,SXE_RES) == 0 ){` |
|     7 | 2541 | `		return;` |
|     - | 2542 | `	}` |
|     7 | 2543 | `	pA = SxeNodeOf(pThis);` |
|     7 | 2544 | `	pB = SxeNodeOf(pOther);` |
|     7 | 2545 | `	pCtx->bAnswered = 1;` |
|     7 | 2546 | `	pCtx->iResult = (pA == 0 && pB == 0) ? 0 : (pA == pB ? 0 : 1);` |
|     7 | 2547 | `}` |
|  8445 | 2548 | `PH7_PRIVATE sxi32 PH7_VmInstallSimpleXml(ph7_vm *pVm)` |
|     5 | 2549 | `{` |
|     - | 2550 | `	static const struct {` |
|     - | 2551 | `		const char *zName;` |
|     - | 2552 | `		ProchHostFunction xFunc;` |
|     - | 2553 | `	} aFunc[] = {` |
|     - | 2554 | `		{ "simplexml_load_file",   vm_builtin_simplexml_load_file   },` |
|     - | 2555 | `		{ "simplexml_load_string", vm_builtin_simplexml_load_string },` |
|     - | 2556 | `		{ "simplexml_import_dom",  vm_builtin_simplexml_import_dom  },` |
|     - | 2557 | `		/* ext/dom's own names for the other direction: one per class tree. */` |
|     - | 2558 | `		{ "dom_import_simplexml",  vm_builtin_dom_import_simplexml  },` |
|     - | 2559 | `		{ "Dom\\import_simplexml", vm_builtin_Dom_import_simplexml  },` |
|     - | 2560 | `	};` |
|     - | 2561 | `	/*` |
|     - | 2562 | `	 * The engine slots. php's SimpleXMLElement declares NO property -- Reflection` |
|     - | 2563 | ``	 * lists none and `property_exists()` is false for every name -- so each of`` |
|     - | 2564 | `	 * these is PH7_MOD_HIDDEN and the whole object shows the document instead.` |
|     - | 2565 | `	 */` |
|     - | 2566 | `	static const PH7_NativePropDef aProp[] = {` |
|     - | 2567 | `		{ SXE_RES,  PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 2568 | `		{ SXE_IT,   PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,  0, 0, 0.0 }, 0 },` |
|     - | 2569 | `		{ SXE_NM,   PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 2570 | `		{ SXE_NS,   PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 2571 | `		{ SXE_ISP,  PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,  0, 0, 0.0 }, 0 },` |
|     - | 2572 | `		{ SXE_CUR,  PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 2573 | `		{ SXE_XPNS, PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 2574 | `	};` |
|     - | 2575 | `	/*` |
|     - | 2576 | `	 * php's own list, in php's own order -- which is what Reflection reports and` |
|     - | 2577 | ``	 * what `get_class_methods()` answers. Every return type is TENTATIVE (`@`):`` |
|     - | 2578 | `	 * php's stubs mark the whole class that way, so a subclass may still declare` |
|     - | 2579 | ``	 * `children()` returning something else without a fatal.`` |
|     - | 2580 | `	 */` |
|     - | 2581 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|     - | 2582 | `		{ "xpath", PH7_MOD_PUBLIC, "string $expression", "@array\|false\|null",` |
|     - | 2583 | `		  vm_builtin_SimpleXMLElement_xpath },` |
|     - | 2584 | `		{ "registerXPathNamespace", PH7_MOD_PUBLIC, "string $prefix, string $namespace",` |
|     - | 2585 | `		  "@bool", vm_builtin_SimpleXMLElement_registerXPathNamespace },` |
|     - | 2586 | `		{ "asXML", PH7_MOD_PUBLIC, "?string $filename = null", "@string\|bool",` |
|     - | 2587 | `		  vm_builtin_SimpleXMLElement_asXML },` |
|     - | 2588 | `		{ "saveXML", PH7_MOD_PUBLIC, "?string $filename = null", "@string\|bool",` |
|     - | 2589 | `		  vm_builtin_SimpleXMLElement_asXML },` |
|     - | 2590 | `		{ "getNamespaces", PH7_MOD_PUBLIC, "bool $recursive = false", "@array",` |
|     - | 2591 | `		  vm_builtin_SimpleXMLElement_getNamespaces },` |
|     - | 2592 | `		{ "getDocNamespaces", PH7_MOD_PUBLIC, "bool $recursive = false, bool $fromRoot = true",` |
|     - | 2593 | `		  "@array\|false", vm_builtin_SimpleXMLElement_getDocNamespaces },` |
|     - | 2594 | `		{ "children", PH7_MOD_PUBLIC, "?string $namespaceOrPrefix = null, bool $isPrefix = false",` |
|     - | 2595 | `		  "@?SimpleXMLElement", vm_builtin_SimpleXMLElement_children },` |
|     - | 2596 | `		{ "attributes", PH7_MOD_PUBLIC, "?string $namespaceOrPrefix = null, bool $isPrefix = false",` |
|     - | 2597 | `		  "@?SimpleXMLElement", vm_builtin_SimpleXMLElement_attributes },` |
|     - | 2598 | `		{ "__construct", PH7_MOD_PUBLIC,` |
|     - | 2599 | `		  "string $data, int $options = 0, bool $dataIsURL = false, "` |
|     - | 2600 | `		  "string $namespaceOrPrefix = '', bool $isPrefix = false", "",` |
|     - | 2601 | `		  vm_builtin_SimpleXMLElement_construct },` |
|     - | 2602 | `		{ "addChild", PH7_MOD_PUBLIC,` |
|     - | 2603 | `		  "string $qualifiedName, ?string $value = null, ?string $namespace = null",` |
|     - | 2604 | `		  "@?SimpleXMLElement", vm_builtin_SimpleXMLElement_addChild },` |
|     - | 2605 | `		{ "addAttribute", PH7_MOD_PUBLIC,` |
|     - | 2606 | `		  "string $qualifiedName, string $value, ?string $namespace = null", "@void",` |
|     - | 2607 | `		  vm_builtin_SimpleXMLElement_addAttribute },` |
|     - | 2608 | `		{ "getName", PH7_MOD_PUBLIC, "", "@string", vm_builtin_SimpleXMLElement_getName },` |
|     - | 2609 | `		{ "__toString", PH7_MOD_PUBLIC, "", "string", vm_builtin_SimpleXMLElement_toString },` |
|     - | 2610 | `		{ "__debugInfo", PH7_MOD_PUBLIC, "", "?array", vm_builtin_SimpleXMLElement_debugInfo },` |
|     - | 2611 | `		{ "count", PH7_MOD_PUBLIC, "", "@int", vm_builtin_SimpleXMLElement_count },` |
|     - | 2612 | `		{ "rewind", PH7_MOD_PUBLIC, "", "@void", vm_builtin_SimpleXMLElement_rewind },` |
|     - | 2613 | `		{ "valid", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SimpleXMLElement_valid },` |
|     - | 2614 | `		{ "current", PH7_MOD_PUBLIC, "", "@SimpleXMLElement",` |
|     - | 2615 | `		  vm_builtin_SimpleXMLElement_current },` |
|     - | 2616 | `		{ "key", PH7_MOD_PUBLIC, "", "@string", vm_builtin_SimpleXMLElement_key },` |
|     - | 2617 | `		{ "next", PH7_MOD_PUBLIC, "", "@void", vm_builtin_SimpleXMLElement_next },` |
|     - | 2618 | `		{ "hasChildren", PH7_MOD_PUBLIC, "", "@bool",` |
|     - | 2619 | `		  vm_builtin_SimpleXMLElement_hasChildren },` |
|     - | 2620 | `		{ "getChildren", PH7_MOD_PUBLIC, "", "@?SimpleXMLElement",` |
|     - | 2621 | `		  vm_builtin_SimpleXMLElement_getChildren },` |
|     - | 2622 | `	};` |
|     - | 2623 | `	/*` |
|     - | 2624 | `	 * php's class list. SimpleXMLElement declares Stringable, Countable and` |
|     - | 2625 | ``	 * RecursiveIterator -- and NOT ArrayAccess, which is why `$x['a']` is a`` |
|     - | 2626 | ``	 * dimension handler and `$x instanceof ArrayAccess` is false; and not`` |
|     - | 2627 | `	 * JsonSerializable, because json_encode() reads its property table like any` |
|     - | 2628 | `	 * other object's.` |
|     - | 2629 | `	 *` |
|     - | 2630 | `	 * PH7_CLASS_NUM_AS_STRING is php's cast_object answering IS_LONG/IS_DOUBLE` |
|     - | 2631 | `	 * from the node's text, and PH7_CLASS_VARS_PRESENT is its get_properties` |
|     - | 2632 | `	 * answering the get_object_vars purpose too -- the two places SimpleXML does` |
|     - | 2633 | `	 * not behave like every other native class here.` |
|     - | 2634 | `	 *` |
|     - | 2635 | `	 * SimpleXMLIterator adds nothing: php declares it as an empty subclass, kept` |
|     - | 2636 | `	 * because RecursiveIteratorIterator over one is how the class is used.` |
|     - | 2637 | `	 */` |
|     - | 2638 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - | 2639 | `		{ "SimpleXMLElement", 0, "Stringable,Countable,RecursiveIterator",` |
|     - | 2640 | `		  PH7_CLASS_NOSERIALIZE\|PH7_CLASS_NUM_AS_STRING\|PH7_CLASS_VARS_PRESENT,` |
|     - | 2641 | `		  aMethod, SX_ARRAYSIZE(aMethod), 0, 0, aProp, SX_ARRAYSIZE(aProp),` |
|     - | 2642 | `		  0, 0, SxePresent },` |
|     - | 2643 | `		{ "SimpleXMLIterator", "SimpleXMLElement", 0,` |
|     - | 2644 | `		  PH7_CLASS_NOSERIALIZE\|PH7_CLASS_NUM_AS_STRING\|PH7_CLASS_VARS_PRESENT,` |
|     - | 2645 | `		  0, 0, 0, 0, 0, 0, 0, 0, SxePresent },` |
|     - | 2646 | `	};` |
|     - | 2647 | `	sxi32 rc;` |
|     - | 2648 | `	sxu32 n;` |
| 50675 | 2649 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFunc) ; n++ ){` |
| 42230 | 2650 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
| 21090 | 2651 | `	}` |
|  8450 | 2652 | `	rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|  8450 | 2653 | `	if( rc == SXRET_OK ){` |
|     - | 2654 | `		/* The four handlers php gives the class, assigned on the mounted class` |
|     - | 2655 | `		 * because PH7_NativeClassSpec carries no field for any of them. Stated on` |
|     - | 2656 | `		 * the ROOT only: the engine walks the base chain, which is php's own` |
|     - | 2657 | `		 * handler inheritance, so SimpleXMLIterator and a userland subclass reach` |
|     - | 2658 | `		 * these. */` |
|  8450 | 2659 | `		ph7_class *pClass = PH7_VmExtractClass(&(*pVm),"SimpleXMLElement",` |
|     - | 2660 | `			sizeof("SimpleXMLElement")-1,FALSE,0);` |
|  8450 | 2661 | `		if( pClass ){` |
|  8450 | 2662 | `			pClass->xDim = SxeDimHook;` |
|  8450 | 2663 | `			pClass->xClone = SxeClone;` |
|  8450 | 2664 | `			pClass->xBool = SxeBool;` |
|  8450 | 2665 | `			pClass->xCmp = SxeCmp;` |
|  4217 | 2666 | `		}` |
|  8450 | 2667 | `		PH7_NativeClassInstallPropHook(&(*pVm),"SimpleXMLElement",SxePropHook);` |
|  4217 | 2668 | `	}` |
|  8450 | 2669 | `	return rc;` |
|     5 | 2670 | `}` |
|     - | 2671 |  |
|     - | 2672 | `#else` |
|     - | 2673 | `/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */` |
|     - | 2674 | `typedef int vm_simplexml_unused;` |
|     - | 2675 | `#endif /* PH7_ENABLE_LIBXML */` |
|     - | 2676 |  |
