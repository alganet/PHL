# src/ph7/vm_simplexml.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1404/1618 lines (86.77%)

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
|  1722 |   73 | `static phl_domnode * SxeResOf(ph7_class_instance *pObj)` |
|     3 |   74 | `{` |
|  1725 |   75 | `	ph7_value *pVal = pObj ? PH7_NativeAttr(pObj,SXE_RES) : 0;` |
|  1725 |   76 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_RES) == 0 ){` |
|     5 |   77 | `		return 0;` |
|     - |   78 | `	}` |
|  1721 |   79 | `	return (phl_domnode *)pVal->x.pOther;` |
|   864 |   80 | `}` |
|  1322 |   81 | `static xmlNodePtr SxeNodeOf(ph7_class_instance *pObj)` |
|     3 |   82 | `{` |
|  1325 |   83 | `	phl_domnode *pNd = SxeResOf(pObj);` |
|  1325 |   84 | `	return pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     3 |   85 | `}` |
|  1778 |   86 | `static int SxeTypeOf(ph7_class_instance *pObj)` |
|     3 |   87 | `{` |
|  1781 |   88 | `	return pObj ? (int)PH7_NativeAttrInt(pObj,SXE_IT) : SXE_ITER_NONE;` |
|     3 |   89 | `}` |
|     - |   90 | `/*` |
|     - |   91 | ` * A string slot that may be UNSET, which is not the same as empty: php's` |
|     - |   92 | `` * `iter.name` and `iter.nsprefix` are pointers, and a NULL nsprefix means "no`` |
|     - |   93 | ` * filter" where an empty one would mean "the empty namespace".  Answers 0 for` |
|     - |   94 | ` * an unset slot -- the length sentinel PH7_NativeAttrStr cannot give.` |
|     - |   95 | ` */` |
|  1514 |   96 | `static const char * SxeSlotStr(ph7_class_instance *pObj,const char *zSlot,int *pnLen)` |
|     3 |   97 | `{` |
|  1517 |   98 | `	ph7_value *pVal = pObj ? PH7_NativeAttr(pObj,zSlot) : 0;` |
|  1517 |   99 | `	if( pnLen ){` |
|  1517 |  100 | `		*pnLen = 0;` |
|   757 |  101 | `	}` |
|  1517 |  102 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|  1023 |  103 | `		return 0;` |
|     - |  104 | `	}` |
|   497 |  105 | `	if( pnLen ){` |
|   497 |  106 | `		*pnLen = (int)SyBlobLength(&pVal->sBlob);` |
|   247 |  107 | `	}` |
|   497 |  108 | `	return (const char *)SyBlobData(&pVal->sBlob);` |
|   760 |  109 | `}` |
|   414 |  110 | `static const char * SxeIterName(ph7_class_instance *pObj,int *pnLen)` |
|     3 |  111 | `{` |
|   417 |  112 | `	return SxeSlotStr(pObj,SXE_NM,pnLen);` |
|     3 |  113 | `}` |
|  1100 |  114 | `static const char * SxeNsFilter(ph7_class_instance *pObj,int *pnLen)` |
|     3 |  115 | `{` |
|  1103 |  116 | `	return SxeSlotStr(pObj,SXE_NS,pnLen);` |
|     3 |  117 | `}` |
|  1100 |  118 | `static int SxeIsPrefix(ph7_class_instance *pObj)` |
|     3 |  119 | `{` |
|  1103 |  120 | `	return pObj ? (int)PH7_NativeAttrInt(pObj,SXE_ISP) : 0;` |
|     3 |  121 | `}` |
|     - |  122 | `/* The receiver of a native method, when it really is one of ours. */` |
|   908 |  123 | `static ph7_class_instance * SxeThis(ph7_context *pCtx)` |
|     3 |  124 | `{` |
|   911 |  125 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   911 |  126 | `	return pThis && PH7_NativeAttr(pThis,SXE_RES) ? pThis : 0;` |
|     3 |  127 | `}` |
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
|   812 |  140 | `static int SxeMatchNs(xmlNodePtr pNode,const char *zNs,int nNs,int bPrefix)` |
|     3 |  141 | `{` |
|     - |  142 | `	const xmlChar *zHave;` |
|   815 |  143 | `	if( pNode == 0 ){` |
|   ! 0 |  144 | `		return 0;` |
|     - |  145 | `	}` |
|   815 |  146 | `	if( zNs == 0 \|\| nNs < 1 ){` |
|   719 |  147 | `		return pNode->ns == 0 \|\| pNode->ns->prefix == 0;` |
|     - |  148 | `	}` |
|    98 |  149 | `	if( pNode->ns == 0 ){` |
|    18 |  150 | `		return 0;` |
|     - |  151 | `	}` |
|    82 |  152 | `	zHave = bPrefix ? pNode->ns->prefix : pNode->ns->href;` |
|    82 |  153 | `	if( zHave == 0 ){` |
|    23 |  154 | `		return 0;` |
|     - |  155 | `	}` |
|    60 |  156 | `	return (int)xmlStrlen(zHave) == nNs && SyMemcmp(zHave,zNs,(sxu32)nNs) == 0;` |
|   409 |  157 | `}` |
|     - |  158 | `/* ...and the receiver's own filter, applied to one node. */` |
|   812 |  159 | `static int SxeMatch(ph7_class_instance *pObj,xmlNodePtr pNode)` |
|     3 |  160 | `{` |
|   815 |  161 | `	int nNs = 0;` |
|   815 |  162 | `	const char *zNs = SxeNsFilter(pObj,&nNs);` |
|   815 |  163 | `	return SxeMatchNs(pNode,zNs,nNs,SxeIsPrefix(pObj));` |
|     3 |  164 | `}` |
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
|   250 |  297 | `static void SxeNodeText(SyBlob *pOut,xmlNodePtr pNode)` |
|     2 |  298 | `{` |
|     - |  299 | `	xmlNodePtr pWalk;` |
|   252 |  300 | `	if( pNode == 0 ){` |
|     9 |  301 | `		return;` |
|     - |  302 | `	}` |
|   242 |  303 | `	if( pNode->type == XML_COMMENT_NODE \|\| pNode->type == XML_PI_NODE` |
|   241 |  304 | `	 \|\| pNode->type == XML_TEXT_NODE \|\| pNode->type == XML_CDATA_SECTION_NODE ){` |
|     - |  305 | ``		/* These carry their text in `content` and have no child list at all: the`` |
|     - |  306 | ``		 * comment object php's table shows under `comment` stringifies to the`` |
|     - |  307 | `		 * comment's own words. */` |
|     5 |  308 | `		if( pNode->content ){` |
|     7 |  309 | `			SyBlobAppend(pOut,(const char *)pNode->content,` |
|     4 |  310 | `				(sxu32)SyStrlen((const char *)pNode->content));` |
|     2 |  311 | `		}` |
|     5 |  312 | `		return;` |
|     - |  313 | `	}` |
|   504 |  314 | `	for( pWalk = pNode->children ; pWalk ; pWalk = pWalk->next ){` |
|   266 |  315 | `		if( pWalk->type == XML_TEXT_NODE \|\| pWalk->type == XML_CDATA_SECTION_NODE ){` |
|   228 |  316 | `			if( pWalk->content ){` |
|   341 |  317 | `				SyBlobAppend(pOut,(const char *)pWalk->content,` |
|   226 |  318 | `					(sxu32)SyStrlen((const char *)pWalk->content));` |
|   115 |  319 | `			}` |
|   153 |  320 | `		}else if( pWalk->type == XML_ENTITY_REF_NODE ){` |
|     - |  321 | `			/* An unexpanded entity reference contributes its replacement text,` |
|     - |  322 | `			 * which is where libxml keeps it. */` |
|   ! 0 |  323 | `			SxeNodeText(pOut,pWalk);` |
|   ! 0 |  324 | `		}` |
|   134 |  325 | `	}` |
|   127 |  326 | `}` |
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
|   526 |  354 | `static ph7_class_instance * SxeNew(ph7_vm *pVm,ph7_class *pClass,phl_xmldoc *pShell,` |
|     - |  355 | `	xmlNodePtr pNode,int iType,const char *zName,int nName,` |
|     - |  356 | `	const char *zNs,int nNs,int bIsPrefix)` |
|     3 |  357 | `{` |
|     - |  358 | `	ph7_class_instance *pObj;` |
|     - |  359 | `	phl_domnode *pRes;` |
|     - |  360 | `	ph7_value sVal;` |
|   529 |  361 | `	if( pClass == 0 ){` |
|   ! 0 |  362 | `		return 0;` |
|     - |  363 | `	}` |
|   529 |  364 | `	pObj = PH7_NewClassInstance(pVm,pClass);` |
|   529 |  365 | `	if( pObj == 0 ){` |
|   ! 0 |  366 | `		return 0;` |
|     - |  367 | `	}` |
|   529 |  368 | `	pRes = (phl_domnode *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_domnode));` |
|   529 |  369 | `	if( pRes == 0 ){` |
|   ! 0 |  370 | `		PH7_ClassInstanceUnref(pObj);` |
|   ! 0 |  371 | `		return 0;` |
|     - |  372 | `	}` |
|   529 |  373 | `	pRes->pShell = pShell;` |
|   529 |  374 | `	pRes->pNode = pNode;` |
|   529 |  375 | `	PH7_MemObjInit(pVm,&sVal);` |
|   529 |  376 | `	sVal.x.pOther = pRes;` |
|   529 |  377 | `	sVal.iFlags = MEMOBJ_RES;` |
|   529 |  378 | `	PH7_NativeSetProp(pVm,pObj,SXE_RES,sizeof(SXE_RES)-1,&sVal);` |
|   529 |  379 | `	PH7_NativeSetAttrInt(pVm,pObj,SXE_IT,(sxi64)iType);` |
|   529 |  380 | `	if( zName ){` |
|   147 |  381 | `		PH7_NativeSetAttrStr(pVm,pObj,SXE_NM,zName,(sxu32)nName);` |
|    72 |  382 | `	}` |
|   529 |  383 | `	if( zNs && nNs > 0 ){` |
|    52 |  384 | `		PH7_NativeSetAttrStr(pVm,pObj,SXE_NS,zNs,(sxu32)nNs);` |
|    52 |  385 | `		PH7_NativeSetAttrInt(pVm,pObj,SXE_ISP,bIsPrefix ? 1 : 0);` |
|    25 |  386 | `	}` |
|   529 |  387 | `	return pObj;` |
|   266 |  388 | `}` |
|     - |  389 | `/* The same, derived from an existing object: its class, its shell and (unless` |
|     - |  390 | ` * the caller states one) its namespace filter. */` |
|   288 |  391 | `static ph7_class_instance * SxeDerive(ph7_vm *pVm,ph7_class_instance *pSrc,` |
|     - |  392 | `	xmlNodePtr pNode,int iType,const char *zName,int nName)` |
|     3 |  393 | `{` |
|   291 |  394 | `	phl_domnode *pNd = SxeResOf(pSrc);` |
|   291 |  395 | `	int nNs = 0;` |
|   291 |  396 | `	const char *zNs = SxeNsFilter(pSrc,&nNs);` |
|   291 |  397 | `	return SxeNew(pVm,pSrc->pClass,pNd ? pNd->pShell : 0,pNode,iType,zName,nName,` |
|   144 |  398 | `		zNs,nNs,SxeIsPrefix(pSrc));` |
|     3 |  399 | `}` |
|     - |  400 | `/* ...and hand it to PHP. */` |
|   238 |  401 | `static int SxeResultObj(ph7_context *pCtx,ph7_class_instance *pObj)` |
|     3 |  402 | `{` |
|   241 |  403 | `	if( pObj == 0 ){` |
|   ! 0 |  404 | `		ph7_result_null(pCtx);` |
|   ! 0 |  405 | `		return PH7_OK;` |
|     - |  406 | `	}` |
|   241 |  407 | `	PH7_NativeResultObject(pCtx,pObj);` |
|   241 |  408 | `	return PH7_OK;` |
|   122 |  409 | `}` |
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
|   128 |  692 | `static void SxeToText(ph7_vm *pVm,ph7_class_instance *pThis,SyBlob *pOut)` |
|     2 |  693 | `{` |
|   130 |  694 | `	xmlNodePtr pNode = SxeTypeOf(pThis) == SXE_ITER_NONE` |
|   128 |  695 | `		? SxeNodeOf(pThis) : SxeFirstNode(pThis);` |
|    64 |  696 | `	(void)pVm;` |
|   130 |  697 | `	SxeNodeText(pOut,pNode);` |
|   130 |  698 | `}` |
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
|    94 |  794 | `static const char * SxeCopyZ(ph7_vm *pVm,SyBlob *pBuf,const char *z,int n)` |
|     1 |  795 | `{` |
|    95 |  796 | `	SyBlobInit(pBuf,&pVm->sAllocator);` |
|    95 |  797 | `	if( z && n > 0 ){` |
|    95 |  798 | `		SyBlobAppend(pBuf,z,(sxu32)n);` |
|    47 |  799 | `	}` |
|    95 |  800 | `	SyBlobNullAppend(pBuf);` |
|    95 |  801 | `	return (const char *)SyBlobData(pBuf);` |
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
|    56 |  814 | `static int SxeValueText(ph7_vm *pVm,ph7_value *pVal,int bAttr,SyBlob *pOut,` |
|     - |  815 | `	const char **pzClass,char *zMsg,sxu32 nMsg)` |
|     1 |  816 | `{` |
|    57 |  817 | `	SyBlobInit(pOut,&pVm->sAllocator);` |
|    57 |  818 | `	if( pVal == 0 ){` |
|   ! 0 |  819 | `		SyBlobNullAppend(pOut);` |
|   ! 0 |  820 | `		return 1;` |
|     - |  821 | `	}` |
|    57 |  822 | `	if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|     3 |  823 | `		*pzClass = "TypeError";` |
|     4 |  824 | `		SyBufferFormat(zMsg,nMsg,` |
|     - |  825 | `			"It's not possible to assign a complex type to %s, array given",` |
|     1 |  826 | `			bAttr ? "attributes" : "properties");` |
|     3 |  827 | `		return 0;` |
|     - |  828 | `	}` |
|    55 |  829 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
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
|    51 |  845 | `		int nLen = 0;` |
|    51 |  846 | `		const char *zStr = ph7_value_to_string(pVal,&nLen);` |
|    51 |  847 | `		if( nLen > 0 ){` |
|    47 |  848 | `			SyBlobAppend(pOut,zStr,(sxu32)nLen);` |
|    23 |  849 | `		}` |
|     - |  850 | `	}` |
|    51 |  851 | `	SyBlobNullAppend(pOut);` |
|    51 |  852 | `	return 1;` |
|    29 |  853 | `}` |
|     - |  854 | `/*` |
|     - |  855 | ``  * Put text into an element or an attribute the way php's `change_node_zval` `` |
|     - |  856 | `` * does: the string is ENCODED first and then set as content, so a `&` in a`` |
|     - |  857 | ` * value is stored as data and comes back out escaped -- unlike addChild(),` |
|     - |  858 | ` * whose value libxml PARSES.  An empty string leaves the node childless, which` |
|     - |  859 | `` * is what makes `$x->d = null` print `<d/>` and not `<d></d>`.`` |
|     - |  860 | ` */` |
|    46 |  861 | `static void SxeSetText(xmlNodePtr pNode,const char *zText)` |
|     1 |  862 | `{` |
|     - |  863 | `	xmlChar *pEnc;` |
|    47 |  864 | `	if( pNode == 0 ){` |
|   ! 0 |  865 | `		return;` |
|     - |  866 | `	}` |
|    47 |  867 | `	if( zText == 0 \|\| zText[0] == 0 ){` |
|     5 |  868 | `		xmlNodeSetContent(pNode,0);` |
|     5 |  869 | `		return;` |
|     - |  870 | `	}` |
|    43 |  871 | `	pEnc = xmlEncodeEntitiesReentrant(pNode->doc,(const xmlChar *)zText);` |
|    43 |  872 | `	xmlNodeSetContent(pNode,pEnc ? pEnc : (const xmlChar *)zText);` |
|    43 |  873 | `	if( pEnc ){` |
|    43 |  874 | `		xmlFree(pEnc);` |
|    21 |  875 | `	}` |
|    24 |  876 | `}` |
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
|    30 |  902 | `static xmlAttrPtr SxeFindAttr(ph7_class_instance *pThis,xmlNodePtr pNode,` |
|     - |  903 | `	const char *zName,int nName)` |
|     2 |  904 | `{` |
|     - |  905 | `	xmlAttrPtr pAttr;` |
|    32 |  906 | `	if( pNode == 0 \|\| pNode->type != XML_ELEMENT_NODE ){` |
|   ! 0 |  907 | `		return 0;` |
|     - |  908 | `	}` |
|    44 |  909 | `	for( pAttr = pNode->properties ; pAttr ; pAttr = pAttr->next ){` |
|    30 |  910 | `		if( !SxeMatch(pThis,(xmlNodePtr)pAttr) ){` |
|   ! 0 |  911 | `			continue;` |
|     - |  912 | `		}` |
|    28 |  913 | `		if( pAttr->name && (int)xmlStrlen(pAttr->name) == nName` |
|    26 |  914 | `		 && SyMemcmp(pAttr->name,zName,(sxu32)nName) == 0 ){` |
|    18 |  915 | `			return pAttr;` |
|     - |  916 | `		}` |
|     8 |  917 | `	}` |
|    16 |  918 | `	return 0;` |
|    17 |  919 | `}` |
|     - |  920 | `/*` |
|     - |  921 | ` * The node a WRITE lands on or beside: the object's own node for "this node",` |
|     - |  922 | ` * and the first match otherwise -- MATERIALIZED when there is none, so` |
|     - |  923 | `` * `$x->a->b = 'v'` on `<r/>` creates `<a>` before it creates `<b>`.  Answers 0`` |
|     - |  924 | ` * for a question that has nothing to write through (an attribute list, an` |
|     - |  925 | ` * object with no node).` |
|     - |  926 | ` */` |
|    42 |  927 | `static xmlNodePtr SxeWriteBase(ph7_vm *pVm,ph7_class_instance *pThis,int bCreate)` |
|     1 |  928 | `{` |
|    43 |  929 | `	int iType = SxeTypeOf(pThis);` |
|    43 |  930 | `	xmlNodePtr pNode = SxeNodeOf(pThis);` |
|     - |  931 | `	xmlNodePtr pFirst;` |
|    43 |  932 | `	if( pNode == 0 \|\| iType == SXE_ITER_ATTRLIST ){` |
|     3 |  933 | `		return iType == SXE_ITER_ATTRLIST ? pNode : 0;` |
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
|    22 |  945 | `}` |
|     - |  946 |  |
|     - |  947 | `/*` |
|     - |  948 | ` * A warning php raises from a WRITE, which is not a call: there is no accessor` |
|     - |  949 | ` * name to print in front of it, so php attributes it to the caller's scope --` |
|     - |  950 | `` * `main(): Cannot assign to an array of nodes`, `f(): ...` inside a function.`` |
|     - |  951 | ` * Same shape ext/dom's property writes use.` |
|     - |  952 | ` */` |
|     6 |  953 | `static void SxeCallerWarn(ph7_vm *pVm,const char *zFormat,...)` |
|     1 |  954 | `{` |
|     - |  955 | `	SyBlob sFn,sMsg;` |
|     - |  956 | `	SyString sName;` |
|     - |  957 | `	va_list ap;` |
|     7 |  958 | `	SyBlobInit(&sFn,&pVm->sAllocator);` |
|     7 |  959 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     7 |  960 | `	PH7_VmActiveFuncName(pVm,&sFn);` |
|     7 |  961 | `	va_start(ap,zFormat);` |
|     7 |  962 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|     7 |  963 | `	va_end(ap);` |
|     7 |  964 | `	SyBlobNullAppend(&sMsg);` |
|     7 |  965 | `	SyStringInitFromBuf(&sName,SyBlobData(&sFn),SyBlobLength(&sFn));` |
|     7 |  966 | `	PH7_VmThrowError(pVm,&sName,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|     7 |  967 | `	SyBlobRelease(&sMsg);` |
|     7 |  968 | `	SyBlobRelease(&sFn);` |
|     7 |  969 | `}` |
|     - |  970 | `/*` |
|     - |  971 | `` * `$x->name = value` -- php's write_property.`` |
|     - |  972 | ` *` |
|     - |  973 | ` * The write lands on the ONE child element of that name, creates it when there` |
|     - |  974 | ` * is none, and REFUSES when there is more than one: php cannot tell which of a` |
|     - |  975 | ` * set the program meant, and says so in a warning rather than picking.  An` |
|     - |  976 | ` * attribute list has no children to write, and php's handler drops the write` |
|     - |  977 | ` * without a word.` |
|     - |  978 | ` */` |
|    32 |  979 | `static void SxePropStore(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativePropCtx *pCtx)` |
|     1 |  980 | `{` |
|    33 |  981 | `	const SyString *pName = pCtx->pName;` |
|    33 |  982 | `	ph7_value *pValue = pCtx->pResult;` |
|    33 |  983 | `	xmlNodePtr pBase = SxeWriteBase(pVm,pThis,TRUE);` |
|    33 |  984 | `	xmlNodePtr pWalk,pHit = 0;` |
|    33 |  985 | `	int nHit = 0;` |
|     - |  986 | `	SyBlob sText;` |
|    32 |  987 | `	if( SxeTypeOf(pThis) == SXE_ITER_ATTRLIST \|\| pBase == 0` |
|    31 |  988 | `	 \|\| pBase->type != XML_ELEMENT_NODE ){` |
|     6 |  989 | `		return;` |
|     - |  990 | `	}` |
|   111 |  991 | `	for( pWalk = pBase->children ; pWalk ; pWalk = pWalk->next ){` |
|    81 |  992 | `		if( pWalk->type != XML_ELEMENT_NODE \|\| !SxeMatch(pThis,pWalk) ){` |
|   ! 0 |  993 | `			continue;` |
|     - |  994 | `		}` |
|    80 |  995 | `		if( pWalk->name == 0` |
|    80 |  996 | `		 \|\| (sxu32)xmlStrlen(pWalk->name) != SyStringLength(pName)` |
|    78 |  997 | `		 \|\| SyMemcmp(pWalk->name,SyStringData(pName),SyStringLength(pName)) != 0 ){` |
|    53 |  998 | `			continue;` |
|     - |  999 | `		}` |
|    29 | 1000 | `		if( pHit == 0 ){` |
|    27 | 1001 | `			pHit = pWalk;` |
|    13 | 1002 | `		}` |
|    29 | 1003 | `		nHit++;` |
|    15 | 1004 | `	}` |
|    31 | 1005 | `	if( nHit > 1 ){` |
|     3 | 1006 | `		SxeCallerWarn(pVm,` |
|     - | 1007 | `			"Cannot assign to an array of nodes (duplicate subnodes or attr detected)");` |
|     3 | 1008 | `		return;` |
|     - | 1009 | `	}` |
|    43 | 1010 | `	if( !SxeValueText(pVm,pValue,0,&sText,&pCtx->zThrowClass,` |
|    28 | 1011 | `		pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg)) ){` |
|     5 | 1012 | `		return;` |
|     - | 1013 | `	}` |
|    25 | 1014 | `	if( pHit ){` |
|    21 | 1015 | `		SxeSetText(pHit,(const char *)SyBlobData(&sText));` |
|    11 | 1016 | `	}else{` |
|     7 | 1017 | `		SxeNewChild(pVm,pBase,SyStringData(pName),(int)SyStringLength(pName),` |
|     4 | 1018 | `			(const char *)SyBlobData(&sText));` |
|     - | 1019 | `	}` |
|    25 | 1020 | `	SyBlobRelease(&sText);` |
|    17 | 1021 | `}` |
|     - | 1022 | ``/* `unset($x->name)` -- php removes EVERY matching child, not the first. */`` |
|     6 | 1023 | `static void SxePropUnset(ph7_vm *pVm,ph7_class_instance *pThis,const SyString *pName)` |
|     1 | 1024 | `{` |
|     7 | 1025 | `	xmlNodePtr pBase = SxeWriteBase(pVm,pThis,FALSE);` |
|     - | 1026 | `	xmlNodePtr pWalk,pNext;` |
|     6 | 1027 | `	if( SxeTypeOf(pThis) == SXE_ITER_ATTRLIST \|\| pBase == 0` |
|     7 | 1028 | `	 \|\| pBase->type != XML_ELEMENT_NODE ){` |
|   ! 0 | 1029 | `		return;` |
|     - | 1030 | `	}` |
|    25 | 1031 | `	for( pWalk = pBase->children ; pWalk ; pWalk = pNext ){` |
|    19 | 1032 | `		pNext = pWalk->next;` |
|    19 | 1033 | `		if( pWalk->type != XML_ELEMENT_NODE \|\| !SxeMatch(pThis,pWalk) ){` |
|   ! 0 | 1034 | `			continue;` |
|     - | 1035 | `		}` |
|    18 | 1036 | `		if( pWalk->name == 0` |
|    18 | 1037 | `		 \|\| (sxu32)xmlStrlen(pWalk->name) != SyStringLength(pName)` |
|    16 | 1038 | `		 \|\| SyMemcmp(pWalk->name,SyStringData(pName),SyStringLength(pName)) != 0 ){` |
|    13 | 1039 | `			continue;` |
|     - | 1040 | `		}` |
|     7 | 1041 | `		xmlUnlinkNode(pWalk);` |
|     7 | 1042 | `		xmlFreeNode(pWalk);` |
|     4 | 1043 | `	}` |
|     4 | 1044 | `}` |
|     - | 1045 | `/*` |
|     - | 1046 | ` * ph7_class::xProp -- php's read_property / has_property / write_property /` |
|     - | 1047 | ` * unset_property for a class that keeps no property slot at all.` |
|     - | 1048 | ` *` |
|     - | 1049 | ` * Every name is the handler's: SimpleXMLElement declares nothing, so the` |
|     - | 1050 | ` * ordinary path would create a dynamic property where php asks the document.` |
|     - | 1051 | ` * A subclass's OWN declared property is a slot and never reaches here (the` |
|     - | 1052 | ` * engine consults the hook only where the instance has none), which is the` |
|     - | 1053 | ` * one place php and this differ -- php's handler owns those too.` |
|     - | 1054 | ` */` |
|   274 | 1055 | `static void SxePropHook(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativePropCtx *pCtx)` |
|     3 | 1056 | `{` |
|   277 | 1057 | `	if( PH7_NativeAttr(pThis,SXE_RES) == 0 ){` |
|   ! 0 | 1058 | `		return;` |
|     - | 1059 | `	}` |
|   277 | 1060 | `	if( pCtx->iMode == PH7_NATIVE_PROP_OWNS ){` |
|     - | 1061 | `		/* Every name is: the class declares no property, so php's write_property` |
|     - | 1062 | `		 * stands where a dynamic property would be created. */` |
|    65 | 1063 | `		pCtx->bAnswered = 1;` |
|    65 | 1064 | `		return;` |
|     - | 1065 | `	}` |
|   213 | 1066 | `	if( pCtx->iMode == PH7_NATIVE_PROP_WRITE ){` |
|     - | 1067 | `		/* Asked before the value exists, and this handler really STORES -- so it` |
|     - | 1068 | `		 * declines here and takes the write at STORE below, which is the door` |
|     - | 1069 | `		 * every overloaded write shape ends at. Answering here would SWALLOW the` |
|     - | 1070 | `		 * store: the opcode would consume the access and the value would land` |
|     - | 1071 | `		 * nowhere. */` |
|    33 | 1072 | `		return;` |
|     - | 1073 | `	}` |
|   181 | 1074 | `	if( pCtx->iMode == PH7_NATIVE_PROP_STORE ){` |
|    33 | 1075 | `		SxePropStore(pVm,pThis,pCtx);` |
|    33 | 1076 | `		pCtx->bAnswered = 1;` |
|    33 | 1077 | `		return;` |
|     - | 1078 | `	}` |
|   149 | 1079 | `	if( pCtx->iMode == PH7_NATIVE_PROP_UNSET ){` |
|     7 | 1080 | `		SxePropUnset(pVm,pThis,pCtx->pName);` |
|     7 | 1081 | `		pCtx->bAnswered = 1;` |
|     7 | 1082 | `		return;` |
|     - | 1083 | `	}` |
|   143 | 1084 | `	SxePropRead(pVm,pThis,pCtx);` |
|   140 | 1085 | `}` |
|     - | 1086 |  |
|     - | 1087 | `/* ===== Dimensions: php's read_dimension / write_dimension ===== */` |
|     - | 1088 |  |
|     - | 1089 | `/*` |
|     - | 1090 | ` * php's dimension rules turn on the OFFSET'S TYPE, not on its text: a string` |
|     - | 1091 | ` * offset is an ATTRIBUTE name and an integer one is a position in the set.` |
|     - | 1092 | `` * That is why `$x['0']` makes an attribute called `0` and `$x[0]` reaches the`` |
|     - | 1093 | ` * element -- the two spellings are different questions.` |
|     - | 1094 | ` */` |
|    46 | 1095 | `static int SxeDimIsInt(ph7_value *pOffset)` |
|     3 | 1096 | `{` |
|    57 | 1097 | `	return pOffset != 0 && (pOffset->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL)) != 0` |
|    69 | 1098 | `		&& (pOffset->iFlags & MEMOBJ_STRING) == 0;` |
|     3 | 1099 | `}` |
|     - | 1100 | `/* The node at a positional offset in this object's walk, or NULL. A negative` |
|     - | 1101 | ` * offset is php's first node: its loop counts UP to the offset and a negative` |
|     - | 1102 | ` * one never advances it. */` |
|    12 | 1103 | `static xmlNodePtr SxeNodeAtOffset(ph7_class_instance *pThis,sxi64 iOfs,sxi64 *pnCount)` |
|     3 | 1104 | `{` |
|    15 | 1105 | `	xmlNodePtr pNode = SxeFirstNode(pThis);` |
|    15 | 1106 | `	xmlNodePtr pHit = 0;` |
|    15 | 1107 | `	sxi64 n = 0;` |
|    15 | 1108 | `	if( SxeTypeOf(pThis) == SXE_ITER_NONE ){` |
|   ! 0 | 1109 | `		if( pnCount ){` |
|   ! 0 | 1110 | `			*pnCount = pNode ? 1 : 0;` |
|   ! 0 | 1111 | `		}` |
|   ! 0 | 1112 | `		return iOfs <= 0 ? pNode : 0;` |
|     - | 1113 | `	}` |
|    29 | 1114 | `	while( pNode ){` |
|    23 | 1115 | `		if( n == iOfs \|\| (iOfs < 0 && n == 0) ){` |
|    13 | 1116 | `			pHit = pNode;` |
|    13 | 1117 | `			if( pnCount == 0 ){` |
|     9 | 1118 | `				return pHit;` |
|     - | 1119 | `			}` |
|     2 | 1120 | `		}` |
|    16 | 1121 | `		n++;` |
|    16 | 1122 | `		pNode = SxeNextNode(pThis,pNode);` |
|     2 | 1123 | `	}` |
|     7 | 1124 | `	if( pnCount ){` |
|     7 | 1125 | `		*pnCount = n;` |
|     3 | 1126 | `	}` |
|     7 | 1127 | `	return pHit;` |
|     9 | 1128 | `}` |
|     - | 1129 | `/* The element an ATTRIBUTE offset is asked of. */` |
|    26 | 1130 | `static xmlNodePtr SxeAttrBase(ph7_class_instance *pThis)` |
|     2 | 1131 | `{` |
|    28 | 1132 | `	int iType = SxeTypeOf(pThis);` |
|    28 | 1133 | `	if( iType == SXE_ITER_NONE \|\| iType == SXE_ITER_CHILD \|\| iType == SXE_ITER_ATTRLIST ){` |
|    28 | 1134 | `		return SxeNodeOf(pThis);` |
|     - | 1135 | `	}` |
|   ! 0 | 1136 | `	return SxeFirstNode(pThis);` |
|    15 | 1137 | `}` |
|    14 | 1138 | `static void SxeDimRead(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeDimCtx *pCtx)` |
|     3 | 1139 | `{` |
|    17 | 1140 | `	ph7_class_instance *pSub = 0;` |
|    17 | 1141 | `	int bIsset = pCtx->iMode == PH7_NATIVE_DIM_ISSET;` |
|    17 | 1142 | `	int bEmpty = pCtx->iMode == PH7_NATIVE_DIM_NOTEMPTY;` |
|    17 | 1143 | `	xmlNodePtr pHit = 0;` |
|    17 | 1144 | `	if( SxeDimIsInt(pCtx->pOffset) ){` |
|     6 | 1145 | `		pHit = SxeNodeAtOffset(pThis,ph7_value_to_int64(pCtx->pOffset),0);` |
|    14 | 1146 | `	}else if( pCtx->pOffset ){` |
|    12 | 1147 | `		int nName = 0;` |
|    12 | 1148 | `		const char *zName = ph7_value_to_string(pCtx->pOffset,&nName);` |
|    12 | 1149 | `		pHit = (xmlNodePtr)SxeFindAttr(pThis,SxeAttrBase(pThis),zName,nName);` |
|     5 | 1150 | `	}` |
|    17 | 1151 | `	if( pHit == 0 ){` |
|     3 | 1152 | `		if( bIsset \|\| bEmpty ){` |
|   ! 0 | 1153 | `			PH7_MemObjRelease(pCtx->pResult);` |
|   ! 0 | 1154 | `			ph7_value_bool(pCtx->pResult,0);` |
|   ! 0 | 1155 | `		}` |
|     3 | 1156 | `		return;` |
|     - | 1157 | `	}` |
|    15 | 1158 | `	if( bIsset ){` |
|   ! 0 | 1159 | `		PH7_MemObjRelease(pCtx->pResult);` |
|   ! 0 | 1160 | `		ph7_value_bool(pCtx->pResult,1);` |
|   ! 0 | 1161 | `		return;` |
|     - | 1162 | `	}` |
|    15 | 1163 | `	if( bEmpty ){` |
|     - | 1164 | ``		/* php's `check_empty` reads the offset's TEXT and judges THAT, so an`` |
|     - | 1165 | `		 * attribute holding "0" is empty() while the object a read of it hands` |
|     - | 1166 | `		 * back is truthy. */` |
|     - | 1167 | `		SyBlob sText;` |
|     - | 1168 | `		ph7_value sVal;` |
|   ! 0 | 1169 | `		SyBlobInit(&sText,&pVm->sAllocator);` |
|   ! 0 | 1170 | `		SxeNodeText(&sText,pHit);` |
|   ! 0 | 1171 | `		PH7_MemObjInitFromString(pVm,&sVal,0);` |
|   ! 0 | 1172 | `		PH7_MemObjStringAppend(&sVal,(const char *)SyBlobData(&sText),SyBlobLength(&sText));` |
|   ! 0 | 1173 | `		PH7_MemObjRelease(pCtx->pResult);` |
|   ! 0 | 1174 | `		ph7_value_bool(pCtx->pResult,ph7_value_to_bool(&sVal));` |
|   ! 0 | 1175 | `		PH7_MemObjRelease(&sVal);` |
|   ! 0 | 1176 | `		SyBlobRelease(&sText);` |
|   ! 0 | 1177 | `		return;` |
|     - | 1178 | `	}` |
|    15 | 1179 | `	pSub = SxeDerive(pVm,pThis,pHit,SXE_ITER_NONE,0,0);` |
|    15 | 1180 | `	if( pSub == 0 ){` |
|   ! 0 | 1181 | `		return;` |
|     - | 1182 | `	}` |
|    15 | 1183 | `	PH7_MemObjRelease(pCtx->pResult);` |
|    15 | 1184 | `	PH7_MemObjInit(pVm,pCtx->pResult);` |
|    15 | 1185 | `	pCtx->pResult->x.pOther = pSub;` |
|    15 | 1186 | `	pCtx->pResult->iFlags = MEMOBJ_OBJ;` |
|    10 | 1187 | `}` |
|     - | 1188 | `/*` |
|     - | 1189 | `` * `$x[$k] = $v`, `$x[] = $v` and `unset($x[$k])`.`` |
|     - | 1190 | ` *` |
|     - | 1191 | ` * A string offset is an attribute: set where it exists, created where it does` |
|     - | 1192 | ` * not, and on a MISSING element the element is created first, so` |
|     - | 1193 | `` * `$x->kid['a'] = '1'` on `<r/>` yields `<r><kid a="1"/></r>`.`` |
|     - | 1194 | ` *` |
|     - | 1195 | ` * An integer offset is a position, and the write goes where php's does:` |
|     - | 1196 | ` *   * "this node" writes the node's own text, warning when the offset is past` |
|     - | 1197 | ` *     the one node it stands for;` |
|     - | 1198 | ` *   * a NAMED set writes the offset-th match, and past the end warns and then` |
|     - | 1199 | ` *     appends a fresh sibling of the first match;` |
|     - | 1200 | `` *   * `children()` -- the one question with no name -- creates a SIBLING OF ITS`` |
|     - | 1201 | ` *     OWN NODE carrying that node's name, which for a root element means a` |
|     - | 1202 | ` *     second root. php's own answer, degenerate and reproduced.` |
|     - | 1203 | ` */` |
|    30 | 1204 | `static void SxeDimWrite(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeDimCtx *pDim)` |
|     1 | 1205 | `{` |
|    31 | 1206 | `	int iType = SxeTypeOf(pThis);` |
|     - | 1207 | `	SyBlob sText;` |
|    31 | 1208 | `	if( pDim->pOffset && !SxeDimIsInt(pDim->pOffset) ){` |
|    17 | 1209 | `		int nName = 0;` |
|    17 | 1210 | `		const char *zName = ph7_value_to_string(pDim->pOffset,&nName);` |
|    17 | 1211 | `		xmlNodePtr pBase = iType == SXE_ITER_ELEMENT` |
|    16 | 1212 | `			? SxeWriteBase(pVm,pThis,TRUE) : SxeAttrBase(pThis);` |
|     - | 1213 | `		xmlAttrPtr pAttr;` |
|    17 | 1214 | `		if( pBase == 0 \|\| pBase->type != XML_ELEMENT_NODE \|\| nName < 1 ){` |
|   ! 0 | 1215 | `			return;` |
|     - | 1216 | `		}` |
|    25 | 1217 | `		if( !SxeValueText(pVm,pDim->pResult,1,&sText,&pDim->zThrowClass,` |
|    16 | 1218 | `			pDim->zThrowMsg,sizeof(pDim->zThrowMsg)) ){` |
|   ! 0 | 1219 | `			return;` |
|     - | 1220 | `		}` |
|    17 | 1221 | `		pAttr = SxeFindAttr(pThis,pBase,zName,nName);` |
|    17 | 1222 | `		if( pAttr == 0 && iType != SXE_ITER_ATTRLIST ){` |
|     - | 1223 | `			/* An attribute list WRITES the ones it lists and creates none: php's` |
|     - | 1224 | ``			 * `$x->attributes()['zz'] = 'v'` is a silent no-op where`` |
|     - | 1225 | ``			 * `$x['zz'] = 'v'` makes the attribute. */`` |
|     - | 1226 | `			SyBlob sNm;` |
|    13 | 1227 | `			pAttr = xmlNewProp(pBase,` |
|     8 | 1228 | `				(const xmlChar *)SxeCopyZ(pVm,&sNm,zName,nName),(const xmlChar *)"");` |
|     9 | 1229 | `			SyBlobRelease(&sNm);` |
|     4 | 1230 | `		}` |
|    17 | 1231 | `		if( pAttr ){` |
|    15 | 1232 | `			SxeSetText((xmlNodePtr)pAttr,(const char *)SyBlobData(&sText));` |
|     7 | 1233 | `		}` |
|    17 | 1234 | `		SyBlobRelease(&sText);` |
|    17 | 1235 | `		return;` |
|     - | 1236 | `	}` |
|    15 | 1237 | `	if( iType == SXE_ITER_ATTRLIST ){` |
|   ! 0 | 1238 | `		if( pDim->pOffset == 0 ){` |
|   ! 0 | 1239 | `			pDim->zThrowClass = "ValueError";` |
|   ! 0 | 1240 | `			SyBufferFormat(pDim->zThrowMsg,sizeof(pDim->zThrowMsg),` |
|     - | 1241 | `				"Cannot append to an attribute list");` |
|   ! 0 | 1242 | `			return;` |
|     - | 1243 | `		}` |
|     - | 1244 | `		{` |
|   ! 0 | 1245 | `			xmlNodePtr pHit = SxeNodeAtOffset(pThis,ph7_value_to_int64(pDim->pOffset),0);` |
|   ! 0 | 1246 | `			if( pHit && SxeValueText(pVm,pDim->pResult,1,&sText,&pDim->zThrowClass,` |
|   ! 0 | 1247 | `				pDim->zThrowMsg,sizeof(pDim->zThrowMsg)) ){` |
|   ! 0 | 1248 | `				SxeSetText(pHit,(const char *)SyBlobData(&sText));` |
|   ! 0 | 1249 | `				SyBlobRelease(&sText);` |
|   ! 0 | 1250 | `			}` |
|     - | 1251 | `		}` |
|   ! 0 | 1252 | `		return;` |
|     - | 1253 | `	}` |
|    15 | 1254 | `	if( iType == SXE_ITER_NONE ){` |
|     7 | 1255 | `		xmlNodePtr pNode = SxeNodeOf(pThis);` |
|     7 | 1256 | `		sxi64 iOfs = pDim->pOffset ? ph7_value_to_int64(pDim->pOffset) : 0;` |
|     7 | 1257 | `		if( pDim->pOffset == 0 \|\| pNode == 0 ){` |
|     3 | 1258 | `			pDim->zThrowClass = "ValueError";` |
|     3 | 1259 | `			SyBufferFormat(pDim->zThrowMsg,sizeof(pDim->zThrowMsg),` |
|     - | 1260 | `				"Cannot append to an attribute list");` |
|     3 | 1261 | `			return;` |
|     - | 1262 | `		}` |
|     5 | 1263 | `		if( iOfs > 0 ){` |
|     3 | 1264 | `			SxeCallerWarn(pVm,` |
|     - | 1265 | `				"Cannot add element %s number %qd when only 0 such elements exist",` |
|     2 | 1266 | `				pNode->name ? (const char *)pNode->name : "",iOfs);` |
|     1 | 1267 | `		}` |
|     7 | 1268 | `		if( SxeValueText(pVm,pDim->pResult,0,&sText,&pDim->zThrowClass,` |
|     4 | 1269 | `			pDim->zThrowMsg,sizeof(pDim->zThrowMsg)) ){` |
|     5 | 1270 | `			SxeSetText(pNode,(const char *)SyBlobData(&sText));` |
|     5 | 1271 | `			SyBlobRelease(&sText);` |
|     2 | 1272 | `		}` |
|     5 | 1273 | `		return;` |
|     - | 1274 | `	}` |
|     9 | 1275 | `	if( iType == SXE_ITER_ELEMENT ){` |
|     9 | 1276 | `		sxi64 nCount = 0;` |
|     9 | 1277 | `		sxi64 iOfs = pDim->pOffset ? ph7_value_to_int64(pDim->pOffset) : -1;` |
|     9 | 1278 | `		xmlNodePtr pHit = pDim->pOffset ? SxeNodeAtOffset(pThis,iOfs,&nCount) : 0;` |
|     9 | 1279 | `		int nNm = 0;` |
|     9 | 1280 | `		const char *zNm = SxeIterName(pThis,&nNm);` |
|     9 | 1281 | `		if( pHit == 0 && pDim->pOffset && iOfs > nCount ){` |
|     4 | 1282 | `			SxeCallerWarn(pVm,` |
|     - | 1283 | `				"Cannot add element %.*s number %qd when only %qd such elements exist",` |
|     1 | 1284 | `				nNm,zNm ? zNm : "",iOfs,nCount);` |
|     1 | 1285 | `		}` |
|    13 | 1286 | `		if( !SxeValueText(pVm,pDim->pResult,0,&sText,&pDim->zThrowClass,` |
|     8 | 1287 | `			pDim->zThrowMsg,sizeof(pDim->zThrowMsg)) ){` |
|   ! 0 | 1288 | `			return;` |
|     - | 1289 | `		}` |
|     9 | 1290 | `		if( pHit ){` |
|     5 | 1291 | `			SxeSetText(pHit,(const char *)SyBlobData(&sText));` |
|     3 | 1292 | `		}else{` |
|     5 | 1293 | `			xmlNodePtr pFirst = SxeFirstNode(pThis);` |
|     5 | 1294 | `			if( pFirst && pFirst->parent ){` |
|     7 | 1295 | `				SxeNewChild(pVm,pFirst->parent,(const char *)pFirst->name,` |
|     4 | 1296 | `					(int)xmlStrlen(pFirst->name),(const char *)SyBlobData(&sText));` |
|     3 | 1297 | `			}else{` |
|   ! 0 | 1298 | `				SxeNewChild(pVm,SxeNodeOf(pThis),zNm,nNm,(const char *)SyBlobData(&sText));` |
|     - | 1299 | `			}` |
|     - | 1300 | `		}` |
|     9 | 1301 | `		SyBlobRelease(&sText);` |
|     9 | 1302 | `		return;` |
|     - | 1303 | `	}` |
|     - | 1304 | `	/* SXE_ITER_CHILD: php's own degenerate answer, above. */` |
|     - | 1305 | `	{` |
|   ! 0 | 1306 | `		xmlNodePtr pNode = SxeNodeOf(pThis);` |
|   ! 0 | 1307 | `		if( pNode == 0 \|\| pNode->parent == 0 ){` |
|   ! 0 | 1308 | `			return;` |
|     - | 1309 | `		}` |
|   ! 0 | 1310 | `		if( SxeValueText(pVm,pDim->pResult,0,&sText,&pDim->zThrowClass,` |
|   ! 0 | 1311 | `			pDim->zThrowMsg,sizeof(pDim->zThrowMsg)) ){` |
|   ! 0 | 1312 | `			SxeNewChild(pVm,pNode->parent,(const char *)pNode->name,` |
|   ! 0 | 1313 | `				(int)xmlStrlen(pNode->name),(const char *)SyBlobData(&sText));` |
|   ! 0 | 1314 | `			SyBlobRelease(&sText);` |
|   ! 0 | 1315 | `		}` |
|     - | 1316 | `	}` |
|    16 | 1317 | `}` |
|     6 | 1318 | `static void SxeDimUnset(ph7_class_instance *pThis,PH7_NativeDimCtx *pDim)` |
|     1 | 1319 | `{` |
|     7 | 1320 | `	xmlNodePtr pHit = 0;` |
|     7 | 1321 | `	if( SxeDimIsInt(pDim->pOffset) ){` |
|     3 | 1322 | `		pHit = SxeNodeAtOffset(pThis,ph7_value_to_int64(pDim->pOffset),0);` |
|     6 | 1323 | `	}else if( pDim->pOffset ){` |
|     5 | 1324 | `		int nName = 0;` |
|     5 | 1325 | `		const char *zName = ph7_value_to_string(pDim->pOffset,&nName);` |
|     5 | 1326 | `		pHit = (xmlNodePtr)SxeFindAttr(pThis,SxeAttrBase(pThis),zName,nName);` |
|     2 | 1327 | `	}` |
|     7 | 1328 | `	if( pHit == 0 ){` |
|     4 | 1329 | `		return;` |
|     - | 1330 | `	}` |
|     5 | 1331 | `	if( pHit->type == XML_ATTRIBUTE_NODE ){` |
|     3 | 1332 | `		xmlRemoveProp((xmlAttrPtr)pHit);` |
|     3 | 1333 | `		return;` |
|     - | 1334 | `	}` |
|     3 | 1335 | `	xmlUnlinkNode(pHit);` |
|     - | 1336 | `	/* php leaves the node in the document's own ownership rather than freeing` |
|     - | 1337 | `	 * it: an object still standing on it must not read freed memory. The shell` |
|     - | 1338 | `	 * frees it with the tree. */` |
|     3 | 1339 | `	SySetPut(&SxeResOf(pThis)->pShell->aOrphans,(const void *)&pHit);` |
|     4 | 1340 | `}` |
|     - | 1341 | `/* ph7_class::xDim -- one callback for all five modes. */` |
|    50 | 1342 | `static void SxeDimHook(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeDimCtx *pCtx)` |
|     3 | 1343 | `{` |
|    53 | 1344 | `	if( PH7_NativeAttr(pThis,SXE_RES) == 0 ){` |
|   ! 0 | 1345 | `		return;` |
|     - | 1346 | `	}` |
|    50 | 1347 | `	if( pCtx->iMode == PH7_NATIVE_DIM_READ \|\| pCtx->iMode == PH7_NATIVE_DIM_ISSET` |
|    39 | 1348 | `	 \|\| pCtx->iMode == PH7_NATIVE_DIM_NOTEMPTY ){` |
|    17 | 1349 | `		SxeDimRead(pVm,pThis,pCtx);` |
|    17 | 1350 | `		return;` |
|     - | 1351 | `	}` |
|    37 | 1352 | `	if( pCtx->pResult == 0 && pCtx->iMode != PH7_NATIVE_DIM_UNSET ){` |
|     - | 1353 | `		/* The refusal-WORDING probe (PH7_ClassNativeDimRefusal), which carries` |
|     - | 1354 | `		 * neither an offset nor a value. This class has no sentence of its own:` |
|     - | 1355 | `		 * every write it cannot take is php's generic one, so leaving the probe` |
|     - | 1356 | `		 * unanswered is the right answer. */` |
|   ! 0 | 1357 | `		return;` |
|     - | 1358 | `	}` |
|    37 | 1359 | `	if( pCtx->iMode == PH7_NATIVE_DIM_UNSET ){` |
|     7 | 1360 | `		SxeDimUnset(pThis,pCtx);` |
|     7 | 1361 | `		pCtx->bStored = 1;` |
|     7 | 1362 | `		return;` |
|     - | 1363 | `	}` |
|    31 | 1364 | `	SxeDimWrite(pVm,pThis,pCtx);` |
|    31 | 1365 | `	pCtx->bStored = 1;` |
|    28 | 1366 | `}` |
|     - | 1367 |  |
|     - | 1368 | `/* ===== The methods ===== */` |
|     - | 1369 |  |
|     - | 1370 | `/* The node a method that WORKS ON ONE node uses: "this node" for the plain` |
|     - | 1371 | ` * question and the first match otherwise. */` |
|   312 | 1372 | `static xmlNodePtr SxeMethodNode(ph7_class_instance *pThis)` |
|     3 | 1373 | `{` |
|   315 | 1374 | `	return SxeTypeOf(pThis) == SXE_ITER_NONE ? SxeNodeOf(pThis) : SxeFirstNode(pThis);` |
|     3 | 1375 | `}` |
|     - | 1376 | `/* SimpleXMLElement::getName(): the first node's name, and the empty string for` |
|     - | 1377 | ` * a question that finds none -- php answers "" rather than false. */` |
|    46 | 1378 | `SXE_METHOD(vm_builtin_SimpleXMLElement_getName)` |
|     2 | 1379 | `{` |
|    48 | 1380 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|    48 | 1381 | `	xmlNodePtr pNode = pThis ? SxeMethodNode(pThis) : 0;` |
|    23 | 1382 | `	(void)nArg; (void)apArg;` |
|    48 | 1383 | `	if( pNode == 0 \|\| pNode->name == 0 ){` |
|     3 | 1384 | `		ph7_result_string(pCtx,"",0);` |
|     3 | 1385 | `		return PH7_OK;` |
|     - | 1386 | `	}` |
|    46 | 1387 | `	ph7_result_string(pCtx,(const char *)pNode->name,(int)xmlStrlen(pNode->name));` |
|    46 | 1388 | `	return PH7_OK;` |
|    25 | 1389 | `}` |
|     - | 1390 | `/* SimpleXMLElement::__toString(). */` |
|   126 | 1391 | `SXE_METHOD(vm_builtin_SimpleXMLElement_toString)` |
|     2 | 1392 | `{` |
|   128 | 1393 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|     - | 1394 | `	SyBlob sText;` |
|    63 | 1395 | `	(void)nArg; (void)apArg;` |
|   128 | 1396 | `	if( pThis == 0 ){` |
|   ! 0 | 1397 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 | 1398 | `		return PH7_OK;` |
|     - | 1399 | `	}` |
|   128 | 1400 | `	SyBlobInit(&sText,&pCtx->pVm->sAllocator);` |
|   128 | 1401 | `	SxeToText(pCtx->pVm,pThis,&sText);` |
|   128 | 1402 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sText),(int)SyBlobLength(&sText));` |
|   128 | 1403 | `	SyBlobRelease(&sText);` |
|   128 | 1404 | `	return PH7_OK;` |
|    65 | 1405 | `}` |
|     - | 1406 | `/* SimpleXMLElement::count(). */` |
|    16 | 1407 | `SXE_METHOD(vm_builtin_SimpleXMLElement_count)` |
|     1 | 1408 | `{` |
|    17 | 1409 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|     8 | 1410 | `	(void)nArg; (void)apArg;` |
|    17 | 1411 | `	ph7_result_int64(pCtx,pThis ? SxeCount(pThis) : 0);` |
|    17 | 1412 | `	return PH7_OK;` |
|     1 | 1413 | `}` |
|     - | 1414 | `/* SimpleXMLElement::__debugInfo(): php's get_debug_info handler, reachable as a` |
|     - | 1415 | ` * method because php declares it as one. */` |
|     4 | 1416 | `SXE_METHOD(vm_builtin_SimpleXMLElement_debugInfo)` |
|     1 | 1417 | `{` |
|     5 | 1418 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|     5 | 1419 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|     2 | 1420 | `	(void)nArg; (void)apArg;` |
|     5 | 1421 | `	if( pOut == 0 ){` |
|   ! 0 | 1422 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1423 | `		return PH7_OK;` |
|     - | 1424 | `	}` |
|     5 | 1425 | `	if( pThis ){` |
|     5 | 1426 | `		SxeTable(pCtx->pVm,pThis,1,pOut);` |
|     2 | 1427 | `	}` |
|     5 | 1428 | `	ph7_result_value(pCtx,pOut);` |
|     5 | 1429 | `	return PH7_OK;` |
|     3 | 1430 | `}` |
|     - | 1431 | `/*` |
|     - | 1432 | ` * children() and attributes(), which are the same call with a different` |
|     - | 1433 | ` * question: both anchor at the node the receiver's question finds and both` |
|     - | 1434 | ` * take the namespace filter from their ARGUMENTS -- a filter the receiver` |
|     - | 1435 | ` * carried is not inherited through them, only through navigation.` |
|     - | 1436 | ` */` |
|    92 | 1437 | `static int SxeSubQuestion(ph7_context *pCtx,int nArg,ph7_value **apArg,int iType)` |
|     3 | 1438 | `{` |
|    95 | 1439 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|    95 | 1440 | `	xmlNodePtr pNode = pThis ? SxeMethodNode(pThis) : 0;` |
|    95 | 1441 | `	phl_domnode *pNd = pThis ? SxeResOf(pThis) : 0;` |
|    95 | 1442 | `	const char *zNs = 0;` |
|    95 | 1443 | `	int nNs = 0,bPrefix = 0;` |
|    95 | 1444 | `	if( pThis == 0 \|\| pNode == 0 ){` |
|   ! 0 | 1445 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1446 | `		return PH7_OK;` |
|     - | 1447 | `	}` |
|    95 | 1448 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0 ){` |
|    33 | 1449 | `		zNs = ph7_value_to_string(apArg[0],&nNs);` |
|    16 | 1450 | `	}` |
|    95 | 1451 | `	if( nArg > 1 ){` |
|    41 | 1452 | `		bPrefix = ph7_value_to_bool(apArg[1]);` |
|    20 | 1453 | `	}` |
|    95 | 1454 | `	return SxeResultObj(pCtx,SxeNew(pCtx->pVm,pThis->pClass,pNd ? pNd->pShell : 0,` |
|    46 | 1455 | `		pNode,iType,0,0,zNs,nNs,bPrefix));` |
|    49 | 1456 | `}` |
|    56 | 1457 | `SXE_METHOD(vm_builtin_SimpleXMLElement_children)` |
|     3 | 1458 | `{` |
|    59 | 1459 | `	return SxeSubQuestion(pCtx,nArg,apArg,SXE_ITER_CHILD);` |
|     3 | 1460 | `}` |
|    36 | 1461 | `SXE_METHOD(vm_builtin_SimpleXMLElement_attributes)` |
|     3 | 1462 | `{` |
|    39 | 1463 | `	return SxeSubQuestion(pCtx,nArg,apArg,SXE_ITER_ATTRLIST);` |
|     3 | 1464 | `}` |
|     - | 1465 |  |
|     - | 1466 | `/*` |
|     - | 1467 | ` * getNamespaces($recursive) -- the namespaces the node (and, recursively, its` |
|     - | 1468 | ` * subtree) USES: its own, and each of its attributes'.  Keyed by prefix, with` |
|     - | 1469 | ` * a default namespace under the empty key, and the FIRST binding of a prefix` |
|     - | 1470 | ` * wins.` |
|     - | 1471 | ` */` |
|    28 | 1472 | `static void SxeAddNs(ph7_vm *pVm,ph7_value *pOut,xmlNsPtr pNs)` |
|     1 | 1473 | `{` |
|    29 | 1474 | `	const char *zPfx = pNs && pNs->prefix ? (const char *)pNs->prefix : "";` |
|    29 | 1475 | `	int nPfx = (int)SyStrlen(zPfx);` |
|     - | 1476 | `	ph7_value sKey,sVal;` |
|    29 | 1477 | `	if( pNs == 0 \|\| pNs->href == 0 \|\| ph7_array_fetch(pOut,zPfx,nPfx) != 0 ){` |
|    11 | 1478 | `		return;` |
|     - | 1479 | `	}` |
|    19 | 1480 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|    19 | 1481 | `	PH7_MemObjStringAppend(&sKey,zPfx,(sxu32)nPfx);` |
|    19 | 1482 | `	PH7_MemObjInitFromString(pVm,&sVal,0);` |
|    19 | 1483 | `	PH7_MemObjStringAppend(&sVal,(const char *)pNs->href,(sxu32)xmlStrlen(pNs->href));` |
|    19 | 1484 | `	ph7_array_add_elem(pOut,&sKey,&sVal);` |
|    19 | 1485 | `	PH7_MemObjRelease(&sKey);` |
|    19 | 1486 | `	PH7_MemObjRelease(&sVal);` |
|    15 | 1487 | `}` |
|    16 | 1488 | `static void SxeUsedNs(ph7_vm *pVm,xmlNodePtr pNode,int bRecursive,ph7_value *pOut)` |
|     1 | 1489 | `{` |
|     - | 1490 | `	xmlAttrPtr pAttr;` |
|    17 | 1491 | `	if( pNode == 0 ){` |
|   ! 0 | 1492 | `		return;` |
|     - | 1493 | `	}` |
|    17 | 1494 | `	if( pNode->ns ){` |
|    17 | 1495 | `		SxeAddNs(pVm,pOut,pNode->ns);` |
|     8 | 1496 | `	}` |
|    25 | 1497 | `	for( pAttr = pNode->properties ; pAttr ; pAttr = pAttr->next ){` |
|     9 | 1498 | `		if( pAttr->ns ){` |
|     5 | 1499 | `			SxeAddNs(pVm,pOut,pAttr->ns);` |
|     2 | 1500 | `		}` |
|     5 | 1501 | `	}` |
|    17 | 1502 | `	if( bRecursive ){` |
|     - | 1503 | `		xmlNodePtr pWalk;` |
|    29 | 1504 | `		for( pWalk = pNode->children ; pWalk ; pWalk = pWalk->next ){` |
|    17 | 1505 | `			if( pWalk->type == XML_ELEMENT_NODE ){` |
|    11 | 1506 | `				SxeUsedNs(pVm,pWalk,1,pOut);` |
|     5 | 1507 | `			}` |
|     9 | 1508 | `		}` |
|     6 | 1509 | `	}` |
|     9 | 1510 | `}` |
|     6 | 1511 | `SXE_METHOD(vm_builtin_SimpleXMLElement_getNamespaces)` |
|     1 | 1512 | `{` |
|     7 | 1513 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|     7 | 1514 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|     7 | 1515 | `	int bRec = nArg > 0 && ph7_value_to_bool(apArg[0]);` |
|     7 | 1516 | `	if( pOut == 0 ){` |
|   ! 0 | 1517 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1518 | `		return PH7_OK;` |
|     - | 1519 | `	}` |
|     7 | 1520 | `	if( pThis ){` |
|     7 | 1521 | `		SxeUsedNs(pCtx->pVm,SxeMethodNode(pThis),bRec,pOut);` |
|     3 | 1522 | `	}` |
|     7 | 1523 | `	ph7_result_value(pCtx,pOut);` |
|     7 | 1524 | `	return PH7_OK;` |
|     4 | 1525 | `}` |
|     - | 1526 | `/*` |
|     - | 1527 | ` * getDocNamespaces($recursive, $fromRoot) -- the namespaces DECLARED, which is` |
|     - | 1528 | ` * a different list and asked of a different node: this one reads the object's` |
|     - | 1529 | `` * OWN node rather than the node its question finds, so `$x->kid->`` |
|     - | 1530 | `` * getDocNamespaces(false,false)` answers the declarations on `$x`'s node --`` |
|     - | 1531 | ` * the parent -- because that is what an element-set object holds.` |
|     - | 1532 | ` */` |
|     4 | 1533 | `static void SxeDeclaredNs(ph7_vm *pVm,xmlNodePtr pNode,int bRecursive,ph7_value *pOut)` |
|     1 | 1534 | `{` |
|     - | 1535 | `	xmlNsPtr pNs;` |
|     5 | 1536 | `	if( pNode == 0 \|\| pNode->type != XML_ELEMENT_NODE ){` |
|   ! 0 | 1537 | `		return;` |
|     - | 1538 | `	}` |
|    13 | 1539 | `	for( pNs = pNode->nsDef ; pNs ; pNs = pNs->next ){` |
|     9 | 1540 | `		SxeAddNs(pVm,pOut,pNs);` |
|     5 | 1541 | `	}` |
|     5 | 1542 | `	if( bRecursive ){` |
|     - | 1543 | `		xmlNodePtr pWalk;` |
|   ! 0 | 1544 | `		for( pWalk = pNode->children ; pWalk ; pWalk = pWalk->next ){` |
|   ! 0 | 1545 | `			SxeDeclaredNs(pVm,pWalk,1,pOut);` |
|   ! 0 | 1546 | `		}` |
|   ! 0 | 1547 | `	}` |
|     3 | 1548 | `}` |
|     4 | 1549 | `SXE_METHOD(vm_builtin_SimpleXMLElement_getDocNamespaces)` |
|     1 | 1550 | `{` |
|     5 | 1551 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|     5 | 1552 | `	phl_domnode *pNd = pThis ? SxeResOf(pThis) : 0;` |
|     5 | 1553 | `	int bRec = nArg > 0 && ph7_value_to_bool(apArg[0]);` |
|     5 | 1554 | `	int bRoot = nArg > 1 ? ph7_value_to_bool(apArg[1]) : 1;` |
|     5 | 1555 | `	xmlNodePtr pNode = pThis ? SxeNodeOf(pThis) : 0;` |
|     - | 1556 | `	ph7_value *pOut;` |
|     5 | 1557 | `	if( pNode == 0 ){` |
|   ! 0 | 1558 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1559 | `		return PH7_OK;` |
|     - | 1560 | `	}` |
|     5 | 1561 | `	if( bRoot ){` |
|     3 | 1562 | `		xmlDocPtr pDoc = pNd && pNd->pShell ? (xmlDocPtr)pNd->pShell->pDoc : pNode->doc;` |
|     3 | 1563 | `		pNode = pDoc ? xmlDocGetRootElement(pDoc) : 0;` |
|     1 | 1564 | `	}` |
|     5 | 1565 | `	pOut = ph7_context_new_array(pCtx);` |
|     5 | 1566 | `	if( pOut == 0 ){` |
|   ! 0 | 1567 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1568 | `		return PH7_OK;` |
|     - | 1569 | `	}` |
|     5 | 1570 | `	SxeDeclaredNs(pCtx->pVm,pNode,bRec,pOut);` |
|     5 | 1571 | `	ph7_result_value(pCtx,pOut);` |
|     5 | 1572 | `	return PH7_OK;` |
|     3 | 1573 | `}` |
|     - | 1574 |  |
|     - | 1575 | `/*` |
|     - | 1576 | ` * asXML() / saveXML().` |
|     - | 1577 | ` *` |
|     - | 1578 | ` * php dumps the whole DOCUMENT -- declaration, trailing newline and all -- when` |
|     - | 1579 | ` * the node it found is the document's own child, and the NODE alone otherwise.` |
|     - | 1580 | `` * That is one test on the parent, and it is why `$x->asXML()` prints a header`` |
|     - | 1581 | `` * and `$x->kid->asXML()` does not; it is also why the same call on a root that`` |
|     - | 1582 | ` * something UNLINKED prints the bare element.` |
|     - | 1583 | ` */` |
|   108 | 1584 | `static int SxeDumpXml(ph7_class_instance *pThis,SyBlob *pOut)` |
|     1 | 1585 | `{` |
|   109 | 1586 | `	xmlNodePtr pNode = pThis ? SxeMethodNode(pThis) : 0;` |
|   109 | 1587 | `	if( pNode == 0 ){` |
|     3 | 1588 | `		return 0;` |
|     - | 1589 | `	}` |
|   107 | 1590 | `	if( pNode->parent && (pNode->parent->type == XML_DOCUMENT_NODE` |
|    58 | 1591 | `	 \|\| pNode->parent->type == XML_HTML_DOCUMENT_NODE) ){` |
|    97 | 1592 | `		xmlChar *zBuf = 0;` |
|    97 | 1593 | `		int nBuf = 0;` |
|    97 | 1594 | `		xmlDocDumpMemory(pNode->doc,&zBuf,&nBuf);` |
|    97 | 1595 | `		if( zBuf == 0 ){` |
|   ! 0 | 1596 | `			return 0;` |
|     - | 1597 | `		}` |
|    97 | 1598 | `		SyBlobAppend(pOut,(const char *)zBuf,(sxu32)nBuf);` |
|    97 | 1599 | `		xmlFree(zBuf);` |
|    97 | 1600 | `		return 1;` |
|     - | 1601 | `	}` |
|     - | 1602 | `	{` |
|     - | 1603 | `		/* One node, through the same output buffer ext/dom dumps a DTD's children` |
|     - | 1604 | `		 * with -- xmlNodeDump itself is deprecated from libxml 2.12 and the MSVC` |
|     - | 1605 | `		 * gate refuses a deprecated symbol under /WX. */` |
|    11 | 1606 | `		xmlBufferPtr pBuf = xmlBufferCreate();` |
|    11 | 1607 | `		xmlOutputBufferPtr pDump = pBuf ? xmlOutputBufferCreateBuffer(pBuf,0) : 0;` |
|    11 | 1608 | `		if( pDump == 0 ){` |
|   ! 0 | 1609 | `			if( pBuf ){` |
|   ! 0 | 1610 | `				xmlBufferFree(pBuf);` |
|   ! 0 | 1611 | `			}` |
|   ! 0 | 1612 | `			return 0;` |
|     - | 1613 | `		}` |
|    11 | 1614 | `		xmlNodeDumpOutput(pDump,pNode->doc,pNode,0,0,0);` |
|    11 | 1615 | `		xmlOutputBufferFlush(pDump);` |
|    11 | 1616 | `		if( xmlBufferContent(pBuf) ){` |
|    16 | 1617 | `			SyBlobAppend(pOut,(const char *)xmlBufferContent(pBuf),` |
|    10 | 1618 | `				(sxu32)xmlBufferLength(pBuf));` |
|     5 | 1619 | `		}` |
|    11 | 1620 | `		xmlOutputBufferClose(pDump);` |
|    11 | 1621 | `		xmlBufferFree(pBuf);` |
|    11 | 1622 | `		return 1;` |
|     - | 1623 | `	}` |
|    55 | 1624 | `}` |
|   108 | 1625 | `SXE_METHOD(vm_builtin_SimpleXMLElement_asXML)` |
|     1 | 1626 | `{` |
|   109 | 1627 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|     - | 1628 | `	SyBlob sOut;` |
|     - | 1629 | `	int bOk;` |
|   109 | 1630 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|   109 | 1631 | `	bOk = SxeDumpXml(pThis,&sOut);` |
|   109 | 1632 | `	if( !bOk ){` |
|     3 | 1633 | `		SyBlobRelease(&sOut);` |
|     3 | 1634 | `		ph7_result_bool(pCtx,0);` |
|     3 | 1635 | `		return PH7_OK;` |
|     - | 1636 | `	}` |
|   107 | 1637 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0 ){` |
|     - | 1638 | `		/* php writes through its OWN stream layer, so a wrapper and a userland` |
|     - | 1639 | `		 * stream are valid destinations here exactly as they are for` |
|     - | 1640 | `		 * DOMDocument::save(). */` |
|     3 | 1641 | `		int nFile = 0;` |
|     3 | 1642 | `		const char *zFile = ph7_value_to_string(apArg[0],&nFile);` |
|     3 | 1643 | `		const ph7_io_stream *pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nFile);` |
|     3 | 1644 | `		void *pHandle = (pStream && pStream->xWrite)` |
|     3 | 1645 | `			? PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,` |
|     - | 1646 | `				PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC,FALSE,0,FALSE,0,` |
|     2 | 1647 | `				ph7_function_name(pCtx)) : 0;` |
|     3 | 1648 | `		int bWrote = 0;` |
|     3 | 1649 | `		if( pHandle ){` |
|     5 | 1650 | `			bWrote = SyBlobLength(&sOut) == 0` |
|     3 | 1651 | `				\|\| pStream->xWrite(pHandle,(const void *)SyBlobData(&sOut),` |
|     3 | 1652 | `					(ph7_int64)SyBlobLength(&sOut)) >= 0;` |
|     3 | 1653 | `			PH7_StreamCloseHandle(pStream,pHandle);` |
|     1 | 1654 | `		}` |
|     3 | 1655 | `		SyBlobRelease(&sOut);` |
|     3 | 1656 | `		ph7_result_bool(pCtx,bWrote);` |
|     3 | 1657 | `		return PH7_OK;` |
|     - | 1658 | `	}` |
|   105 | 1659 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|   105 | 1660 | `	SyBlobRelease(&sOut);` |
|   105 | 1661 | `	return PH7_OK;` |
|    55 | 1662 | `}` |
|     - | 1663 | `/*` |
|     - | 1664 | ` * xpath(): php anchors the expression at the node the question finds, evaluates` |
|     - | 1665 | ` * it with the prefixes registerXPathNamespace() left on THIS object, and takes` |
|     - | 1666 | ` * only a node set.` |
|     - | 1667 | ` *` |
|     - | 1668 | `` * Anything else -- a `count()`, a `string()`, a comparison -- is a warning and`` |
|     - | 1669 | `` * `false`, named by the type libxml answered with. An attribute list has no`` |
|     - | 1670 | ` * node to anchor at and answers null, and so does a set that found none.` |
|     - | 1671 | ` */` |
|    28 | 1672 | `static int SxeRegisterOne(ph7_value *pKey,ph7_value *pVal,void *pUser)` |
|     1 | 1673 | `{` |
|    29 | 1674 | `	xmlXPathContextPtr pXCtx = (xmlXPathContextPtr)pUser;` |
|    29 | 1675 | `	int nPfx = 0,nUri = 0;` |
|    29 | 1676 | `	const char *zPfx = ph7_value_to_string(pKey,&nPfx);` |
|    29 | 1677 | `	const char *zUri = ph7_value_to_string(pVal,&nUri);` |
|     - | 1678 | `	SyBlob sP,sU;` |
|    29 | 1679 | `	if( nPfx < 1 \|\| pKey->pVm == 0 ){` |
|   ! 0 | 1680 | `		return PH7_OK;` |
|     - | 1681 | `	}` |
|    29 | 1682 | `	SxeCopyZ(pKey->pVm,&sP,zPfx,nPfx);` |
|    29 | 1683 | `	SxeCopyZ(pKey->pVm,&sU,zUri,nUri);` |
|    43 | 1684 | `	xmlXPathRegisterNs(pXCtx,(const xmlChar *)SyBlobData(&sP),` |
|    28 | 1685 | `		(const xmlChar *)SyBlobData(&sU));` |
|    29 | 1686 | `	SyBlobRelease(&sP);` |
|    29 | 1687 | `	SyBlobRelease(&sU);` |
|    29 | 1688 | `	return PH7_OK;` |
|    15 | 1689 | `}` |
|    32 | 1690 | `SXE_METHOD(vm_builtin_SimpleXMLElement_xpath)` |
|     1 | 1691 | `{` |
|    33 | 1692 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|    33 | 1693 | `	xmlNodePtr pNode = pThis ? SxeMethodNode(pThis) : 0;` |
|     - | 1694 | `	xmlXPathContextPtr pXCtx;` |
|     - | 1695 | `	xmlXPathObjectPtr pObj;` |
|     - | 1696 | `	ph7_value *pReg,*pOut;` |
|    33 | 1697 | `	xmlNsPtr *aNs = 0;` |
|    33 | 1698 | `	int nExpr = 0;` |
|    33 | 1699 | `	const char *zExpr = nArg > 0 ? ph7_value_to_string(apArg[0],&nExpr) : "";` |
|     - | 1700 | `	SyBlob sExpr;` |
|     - | 1701 | `	sxu32 nMark;` |
|     - | 1702 | `	int i;` |
|    33 | 1703 | `	if( pThis == 0 \|\| pNode == 0 \|\| SxeTypeOf(pThis) == SXE_ITER_ATTRLIST ){` |
|     5 | 1704 | `		ph7_result_null(pCtx);` |
|     5 | 1705 | `		return PH7_OK;` |
|     - | 1706 | `	}` |
|    29 | 1707 | `	pXCtx = xmlXPathNewContext(pNode->doc);` |
|    29 | 1708 | `	if( pXCtx == 0 ){` |
|   ! 0 | 1709 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1710 | `		return PH7_OK;` |
|     - | 1711 | `	}` |
|    29 | 1712 | `	pXCtx->node = pNode;` |
|    29 | 1713 | `	pReg = PH7_NativeAttr(pThis,SXE_XPNS);` |
|    29 | 1714 | `	if( pReg && (pReg->iFlags & MEMOBJ_HASHMAP) ){` |
|    29 | 1715 | `		ph7_array_walk(pReg,SxeRegisterOne,pXCtx);` |
|    14 | 1716 | `	}` |
|     - | 1717 | `	/* php also puts the CONTEXT NODE's in-scope declarations in front of the` |
|     - | 1718 | ``	 * registered table -- libxml consults `namespaces` before the registry -- so`` |
|     - | 1719 | `	 * a prefix the DOCUMENT declares resolves with no registerXPathNamespace()` |
|     - | 1720 | `	 * at all, and beats one registered under the same prefix. */` |
|    29 | 1721 | `	aNs = xmlGetNsList(pNode->doc,pNode);` |
|    29 | 1722 | `	if( aNs ){` |
|    29 | 1723 | `		int nNs = 0;` |
|    57 | 1724 | `		while( aNs[nNs] ){` |
|    29 | 1725 | `			nNs++;` |
|     1 | 1726 | `		}` |
|    29 | 1727 | `		pXCtx->namespaces = aNs;` |
|    29 | 1728 | `		pXCtx->nsNr = nNs;` |
|    14 | 1729 | `	}` |
|    29 | 1730 | `	SyBlobInit(&sExpr,&pCtx->pVm->sAllocator);` |
|    29 | 1731 | `	if( nExpr > 0 ){` |
|    29 | 1732 | `		SyBlobAppend(&sExpr,zExpr,(sxu32)nExpr);` |
|    14 | 1733 | `	}` |
|    29 | 1734 | `	SyBlobNullAppend(&sExpr);` |
|     - | 1735 | `	/* An expression that does not evaluate is LIBXML's diagnostic, not one php` |
|     - | 1736 | ``	 * writes: `Invalid expression`, `Undefined namespace prefix`. php prints it`` |
|     - | 1737 | `	 * under the method's name, with no source location (an XPath error carries` |
|     - | 1738 | `	 * no line), which is what draining the capture window does here. */` |
|    29 | 1739 | `	nMark = PH7_LibxmlCaptureBegin(pCtx->pVm);` |
|    29 | 1740 | `	pObj = xmlXPathEvalExpression((const xmlChar *)SyBlobData(&sExpr),pXCtx);` |
|    29 | 1741 | `	SyBlobRelease(&sExpr);` |
|    29 | 1742 | `	if( pObj == 0 ){` |
|     3 | 1743 | `		PH7_LibxmlCaptureEnd(pCtx->pVm,nMark,"SimpleXMLElement::xpath");` |
|     3 | 1744 | `		xmlXPathFreeContext(pXCtx);` |
|     3 | 1745 | `		if( aNs ){` |
|     3 | 1746 | `			xmlFree(aNs);` |
|     1 | 1747 | `		}` |
|     3 | 1748 | `		ph7_result_bool(pCtx,0);` |
|     3 | 1749 | `		return PH7_OK;` |
|     - | 1750 | `	}` |
|    27 | 1751 | `	PH7_LibxmlDropErrors(pCtx->pVm,nMark);` |
|    27 | 1752 | `	if( pObj->type != XPATH_NODESET ){` |
|    12 | 1753 | `		const char *zKind = pObj->type == XPATH_NUMBER ? "number"` |
|     6 | 1754 | `			: pObj->type == XPATH_STRING ? "string"` |
|     2 | 1755 | `			: pObj->type == XPATH_BOOLEAN ? "bool" : "unknown";` |
|     7 | 1756 | `		xmlXPathFreeObject(pObj);` |
|     7 | 1757 | `		xmlXPathFreeContext(pXCtx);` |
|     7 | 1758 | `		if( aNs ){` |
|     7 | 1759 | `			xmlFree(aNs);` |
|     3 | 1760 | `		}` |
|    10 | 1761 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     3 | 1762 | `			"XPath expression must return a node set, %s returned",zKind);` |
|     7 | 1763 | `		ph7_result_bool(pCtx,0);` |
|     7 | 1764 | `		return PH7_OK;` |
|     - | 1765 | `	}` |
|    21 | 1766 | `	pOut = ph7_context_new_array(pCtx);` |
|    21 | 1767 | `	if( pOut == 0 ){` |
|   ! 0 | 1768 | `		xmlXPathFreeObject(pObj);` |
|   ! 0 | 1769 | `		xmlXPathFreeContext(pXCtx);` |
|   ! 0 | 1770 | `		if( aNs ){` |
|   ! 0 | 1771 | `			xmlFree(aNs);` |
|   ! 0 | 1772 | `		}` |
|   ! 0 | 1773 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1774 | `		return PH7_OK;` |
|     - | 1775 | `	}` |
|    43 | 1776 | `	for( i = 0 ; pObj->nodesetval && i < pObj->nodesetval->nodeNr ; ++i ){` |
|    23 | 1777 | `		xmlNodePtr pHit = pObj->nodesetval->nodeTab[i];` |
|     - | 1778 | `		ph7_class_instance *pSub;` |
|     - | 1779 | `		ph7_value *pSlot;` |
|    23 | 1780 | `		if( pHit == 0 ){` |
|   ! 0 | 1781 | `			continue;` |
|     - | 1782 | `		}` |
|    23 | 1783 | `		if( pHit->type == XML_ATTRIBUTE_NODE ){` |
|     - | 1784 | `			/* php answers an attribute as the ATTRIBUTE LIST of its owner` |
|     - | 1785 | `			 * narrowed to that name, which is why its table shows an` |
|     - | 1786 | ``			 * `@attributes` row where `$x['a']`'s shows a plain value. The filter`` |
|     - | 1787 | `			 * is the ATTRIBUTE'S OWN namespace, not the receiver's. */` |
|     4 | 1788 | `			const char *zHref = pHit->ns && pHit->ns->href` |
|     4 | 1789 | `				? (const char *)pHit->ns->href : 0;` |
|     5 | 1790 | `			phl_domnode *pNd = SxeResOf(pThis);` |
|    10 | 1791 | `			pSub = SxeNew(pCtx->pVm,pThis->pClass,pNd ? pNd->pShell : 0,pHit->parent,` |
|     4 | 1792 | `				SXE_ITER_ATTRLIST,(const char *)pHit->name,(int)xmlStrlen(pHit->name),` |
|     3 | 1793 | `				zHref,zHref ? (int)SyStrlen(zHref) : 0,0);` |
|    21 | 1794 | `		}else if( pHit->type == XML_TEXT_NODE \|\| pHit->type == XML_CDATA_SECTION_NODE ){` |
|     - | 1795 | `			/* A text node is answered as the element that HOLDS it. */` |
|     7 | 1796 | `			pSub = SxeDerive(pCtx->pVm,pThis,pHit->parent,SXE_ITER_NONE,0,0);` |
|    16 | 1797 | `		}else if( pHit->type == XML_DOCUMENT_NODE \|\| pHit->type == XML_HTML_DOCUMENT_NODE` |
|    11 | 1798 | `		       \|\| pHit->type == XML_NAMESPACE_DECL \|\| pHit->type == XML_DTD_NODE ){` |
|     - | 1799 | `			/* The two a node set can hold that SimpleXML has no object for: the` |
|     - | 1800 | ``			 * document itself (what `/` and a root's `..` select) and a namespace`` |
|     - | 1801 | `			 * declaration. php drops both and answers the shorter array. */` |
|     3 | 1802 | `			continue;` |
|   ! 0 | 1803 | `		}else{` |
|     - | 1804 | `			/* Everything else -- an element, and the comment and processing` |
|     - | 1805 | `			 * instruction php keeps here even though neither is one in its table` |
|     - | 1806 | `			 * of an element's children. */` |
|    11 | 1807 | `			pSub = SxeDerive(pCtx->pVm,pThis,pHit,SXE_ITER_NONE,0,0);` |
|     - | 1808 | `		}` |
|    21 | 1809 | `		if( pSub == 0 ){` |
|   ! 0 | 1810 | `			break;` |
|     - | 1811 | `		}` |
|     - | 1812 | `		/* pSlot CARRIES the constructor's reference (no bump): the array's` |
|     - | 1813 | `		 * insert takes its own, and the context releases pSlot at method end. */` |
|    21 | 1814 | `		pSlot = ph7_context_new_scalar(pCtx);` |
|    21 | 1815 | `		if( pSlot == 0 ){` |
|   ! 0 | 1816 | `			PH7_ClassInstanceUnref(pSub);` |
|   ! 0 | 1817 | `			break;` |
|     - | 1818 | `		}` |
|    21 | 1819 | `		pSlot->x.pOther = pSub;` |
|    21 | 1820 | `		pSlot->iFlags = MEMOBJ_OBJ;` |
|    21 | 1821 | `		ph7_array_add_elem(pOut,0,pSlot);` |
|    11 | 1822 | `	}` |
|    21 | 1823 | `	xmlXPathFreeObject(pObj);` |
|    21 | 1824 | `	xmlXPathFreeContext(pXCtx);` |
|    21 | 1825 | `	if( aNs ){` |
|    21 | 1826 | `		xmlFree(aNs);   /* the xmlNs entries belong to the tree; only the array is ours */` |
|    10 | 1827 | `	}` |
|    21 | 1828 | `	ph7_result_value(pCtx,pOut);` |
|    21 | 1829 | `	return PH7_OK;` |
|    17 | 1830 | `}` |
|     - | 1831 | `/* registerXPathNamespace(): the prefixes THIS object's xpath() knows. They do` |
|     - | 1832 | ` * not travel: a child fetched off this object starts with none, which is php's` |
|     - | 1833 | ` * answer too. */` |
|     4 | 1834 | `SXE_METHOD(vm_builtin_SimpleXMLElement_registerXPathNamespace)` |
|     1 | 1835 | `{` |
|     5 | 1836 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|     - | 1837 | `	ph7_value *pReg;` |
|     5 | 1838 | `	int nPfx = 0,nUri = 0;` |
|     5 | 1839 | `	const char *zPfx = nArg > 0 ? ph7_value_to_string(apArg[0],&nPfx) : "";` |
|     5 | 1840 | `	const char *zUri = nArg > 1 ? ph7_value_to_string(apArg[1],&nUri) : "";` |
|     - | 1841 | `	ph7_value sKey,sVal;` |
|     5 | 1842 | `	if( pThis == 0 ){` |
|   ! 0 | 1843 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1844 | `		return PH7_OK;` |
|     - | 1845 | `	}` |
|     5 | 1846 | `	pReg = PH7_NativeAttr(pThis,SXE_XPNS);` |
|     5 | 1847 | `	if( pReg == 0 ){` |
|   ! 0 | 1848 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1849 | `		return PH7_OK;` |
|     - | 1850 | `	}` |
|     5 | 1851 | `	if( (pReg->iFlags & MEMOBJ_HASHMAP) == 0 && PH7_MemObjToHashmap(pReg) != SXRET_OK ){` |
|   ! 0 | 1852 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1853 | `		return PH7_OK;` |
|     - | 1854 | `	}` |
|     5 | 1855 | `	PH7_MemObjInitFromString(pCtx->pVm,&sKey,0);` |
|     5 | 1856 | `	PH7_MemObjStringAppend(&sKey,zPfx,(sxu32)nPfx);` |
|     5 | 1857 | `	PH7_MemObjInitFromString(pCtx->pVm,&sVal,0);` |
|     5 | 1858 | `	PH7_MemObjStringAppend(&sVal,zUri,(sxu32)nUri);` |
|     5 | 1859 | `	ph7_array_add_elem(pReg,&sKey,&sVal);` |
|     5 | 1860 | `	PH7_MemObjRelease(&sKey);` |
|     5 | 1861 | `	PH7_MemObjRelease(&sVal);` |
|     5 | 1862 | `	ph7_result_bool(pCtx,1);` |
|     5 | 1863 | `	return PH7_OK;` |
|     3 | 1864 | `}` |
|     - | 1865 |  |
|     - | 1866 | `/* ===== addChild / addAttribute ===== */` |
|     - | 1867 |  |
|     - | 1868 | ``/* Split `prefix:local`, php's qualified-name form. */`` |
|    10 | 1869 | `static void SxeSplitQName(const char *zName,int nName,const char **pzPfx,int *pnPfx,` |
|     - | 1870 | `	const char **pzLocal,int *pnLocal)` |
|     1 | 1871 | `{` |
|     - | 1872 | `	int i;` |
|    11 | 1873 | `	*pzPfx = 0;` |
|    11 | 1874 | `	*pnPfx = 0;` |
|    11 | 1875 | `	*pzLocal = zName;` |
|    11 | 1876 | `	*pnLocal = nName;` |
|    21 | 1877 | `	for( i = 0 ; i < nName ; ++i ){` |
|    15 | 1878 | `		if( zName[i] == ':' ){` |
|     5 | 1879 | `			*pzPfx = zName;` |
|     5 | 1880 | `			*pnPfx = i;` |
|     5 | 1881 | `			*pzLocal = zName + i + 1;` |
|     5 | 1882 | `			*pnLocal = nName - i - 1;` |
|     5 | 1883 | `			return;` |
|     - | 1884 | `		}` |
|     6 | 1885 | `	}` |
|     6 | 1886 | `}` |
|     - | 1887 | `/*` |
|     - | 1888 | ` * The namespace a new child or attribute is put in: the one the caller named` |
|     - | 1889 | ` * (declared on the node when the document has no binding for it yet), or the` |
|     - | 1890 | ` * one the qualified name's prefix is already bound to, or none -- in which case` |
|     - | 1891 | ` * libxml gives a child the parent's default binding.` |
|     - | 1892 | ` */` |
|    10 | 1893 | `static xmlNsPtr SxeResolveNs(ph7_vm *pVm,xmlNodePtr pNode,const char *zPfx,int nPfx,` |
|     - | 1894 | `	const char *zHref,int nHref,int bHave)` |
|     1 | 1895 | `{` |
|     - | 1896 | `	SyBlob sP,sH;` |
|    11 | 1897 | `	xmlNsPtr pNs = 0;` |
|    11 | 1898 | `	if( bHave ){` |
|     5 | 1899 | `		SxeCopyZ(pVm,&sH,zHref,nHref);` |
|     5 | 1900 | `		SxeCopyZ(pVm,&sP,zPfx,nPfx);` |
|     5 | 1901 | `		pNs = xmlSearchNsByHref(pNode->doc,pNode,(const xmlChar *)SyBlobData(&sH));` |
|     5 | 1902 | `		if( pNs == 0 ){` |
|     7 | 1903 | `			pNs = xmlNewNs(pNode,(const xmlChar *)SyBlobData(&sH),` |
|     2 | 1904 | `				nPfx > 0 ? (const xmlChar *)SyBlobData(&sP) : 0);` |
|     2 | 1905 | `		}` |
|     5 | 1906 | `		SyBlobRelease(&sH);` |
|     5 | 1907 | `		SyBlobRelease(&sP);` |
|     5 | 1908 | `		return pNs;` |
|     - | 1909 | `	}` |
|     7 | 1910 | `	if( zPfx && nPfx > 0 ){` |
|   ! 0 | 1911 | `		SxeCopyZ(pVm,&sP,zPfx,nPfx);` |
|   ! 0 | 1912 | `		pNs = xmlSearchNs(pNode->doc,pNode,(const xmlChar *)SyBlobData(&sP));` |
|   ! 0 | 1913 | `		SyBlobRelease(&sP);` |
|   ! 0 | 1914 | `	}` |
|     7 | 1915 | `	return pNs;` |
|     6 | 1916 | `}` |
|    10 | 1917 | `SXE_METHOD(vm_builtin_SimpleXMLElement_addChild)` |
|     1 | 1918 | `{` |
|    11 | 1919 | `	ph7_vm *pVm = pCtx->pVm;` |
|    11 | 1920 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|    11 | 1921 | `	xmlNodePtr pNode = pThis ? SxeMethodNode(pThis) : 0;` |
|    11 | 1922 | `	int nQ = 0,nVal = 0,nHref = 0,bHaveNs = 0;` |
|    11 | 1923 | `	const char *zQ = nArg > 0 ? ph7_value_to_string(apArg[0],&nQ) : "";` |
|    11 | 1924 | `	const char *zVal = 0,*zHref = 0,*zPfx,*zLocal;` |
|     - | 1925 | `	int nPfx,nLocal;` |
|     - | 1926 | `	SyBlob sLocal;` |
|     - | 1927 | `	const char *zL;` |
|     - | 1928 | `	xmlNsPtr pNs;` |
|     - | 1929 | `	xmlNodePtr pNew;` |
|     - | 1930 | `	SyBlob sVal;` |
|     - | 1931 | `	sxu32 nMark;` |
|    11 | 1932 | `	if( nQ < 1 ){` |
|     3 | 1933 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1934 | `			"SimpleXMLElement::addChild(): Argument #1 ($qualifiedName) must not be empty");` |
|     - | 1935 | `	}` |
|     9 | 1936 | `	if( pThis && SxeTypeOf(pThis) == SXE_ITER_ATTRLIST ){` |
|     3 | 1937 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Cannot add element to attributes");` |
|     3 | 1938 | `		ph7_result_null(pCtx);` |
|     3 | 1939 | `		return PH7_OK;` |
|     - | 1940 | `	}` |
|     7 | 1941 | `	if( pNode == 0 \|\| pNode->type != XML_ELEMENT_NODE ){` |
|     3 | 1942 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,` |
|     - | 1943 | `			"Cannot add child. Parent is not a permanent member of the XML tree");` |
|     3 | 1944 | `		ph7_result_null(pCtx);` |
|     3 | 1945 | `		return PH7_OK;` |
|     - | 1946 | `	}` |
|     5 | 1947 | `	if( nArg > 1 && (apArg[1]->iFlags & MEMOBJ_NULL) == 0 ){` |
|     5 | 1948 | `		zVal = ph7_value_to_string(apArg[1],&nVal);` |
|     2 | 1949 | `	}` |
|     5 | 1950 | `	if( nArg > 2 && (apArg[2]->iFlags & MEMOBJ_NULL) == 0 ){` |
|     3 | 1951 | `		zHref = ph7_value_to_string(apArg[2],&nHref);` |
|     3 | 1952 | `		bHaveNs = 1;` |
|     1 | 1953 | `	}` |
|     5 | 1954 | `	SxeSplitQName(zQ,nQ,&zPfx,&nPfx,&zLocal,&nLocal);` |
|     5 | 1955 | `	zL = SxeCopyZ(pVm,&sLocal,zLocal,nLocal);` |
|     5 | 1956 | `	SyBlobInit(&sVal,&pVm->sAllocator);` |
|     5 | 1957 | `	if( nVal > 0 ){` |
|     5 | 1958 | `		SyBlobAppend(&sVal,zVal,(sxu32)nVal);` |
|     2 | 1959 | `	}` |
|     5 | 1960 | `	SyBlobNullAppend(&sVal);` |
|     - | 1961 | `	/* php's addChild does NOT escape: libxml PARSES the value for entity` |
|     - | 1962 | ``	 * references, so `'a&b'` is the LIBRARY's answer and not php's -- a`` |
|     - | 1963 | `	 * diagnostic and an empty child under libxml 2.9, the two letters under` |
|     - | 1964 | `	 * 2.13. Both engines answer whatever the libxml they were built against` |
|     - | 1965 | `	 * does, which is why no test pins it. Routed through the per-VM queue like` |
|     - | 1966 | `	 * every other libxml message here, or it prints itself past` |
|     - | 1967 | `	 * error_reporting(). */` |
|     5 | 1968 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     7 | 1969 | `	pNew = xmlNewChild(pNode,0,(const xmlChar *)zL,` |
|     2 | 1970 | `		zVal ? (const xmlChar *)SyBlobData(&sVal) : 0);` |
|     5 | 1971 | `	PH7_LibxmlCaptureEnd(pVm,nMark,"SimpleXMLElement::addChild");` |
|     5 | 1972 | `	SyBlobRelease(&sVal);` |
|     5 | 1973 | `	SyBlobRelease(&sLocal);` |
|     - | 1974 | `	/* The namespace is resolved against the NEW node and declared THERE when the` |
|     - | 1975 | ``	 * document has no binding for it -- `addChild('q:n','v','urn:q')` prints`` |
|     - | 1976 | ``	 * `<q:n xmlns:q="urn:q">`, not a declaration hoisted onto the parent. */`` |
|     5 | 1977 | `	if( pNew ){` |
|     5 | 1978 | `		pNs = SxeResolveNs(pVm,pNew,zPfx,nPfx,zHref,nHref,bHaveNs);` |
|     5 | 1979 | `		if( pNs ){` |
|     3 | 1980 | `			xmlSetNs(pNew,pNs);` |
|     1 | 1981 | `		}` |
|     2 | 1982 | `	}` |
|     5 | 1983 | `	if( pNew == 0 ){` |
|   ! 0 | 1984 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1985 | `		return PH7_OK;` |
|     - | 1986 | `	}` |
|     5 | 1987 | `	return SxeResultObj(pCtx,SxeDerive(pVm,pThis,pNew,SXE_ITER_NONE,0,0));` |
|     6 | 1988 | `}` |
|     8 | 1989 | `SXE_METHOD(vm_builtin_SimpleXMLElement_addAttribute)` |
|     1 | 1990 | `{` |
|     9 | 1991 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|     - | 1992 | `	/* An attribute list writes to the element it lists, not to an attribute:` |
|     - | 1993 | `	 * php reaches its own node here rather than the first match. */` |
|    13 | 1994 | `	xmlNodePtr pNode = pThis` |
|     8 | 1995 | `		? (SxeTypeOf(pThis) == SXE_ITER_ATTRLIST ? SxeNodeOf(pThis) : SxeMethodNode(pThis))` |
|     8 | 1996 | `		: 0;` |
|     9 | 1997 | `	int nQ = 0,nVal = 0,nHref = 0,bHaveNs = 0;` |
|     9 | 1998 | `	const char *zQ = nArg > 0 ? ph7_value_to_string(apArg[0],&nQ) : "";` |
|     9 | 1999 | `	const char *zVal = 0,*zHref = 0,*zPfx,*zLocal;` |
|     - | 2000 | `	int nPfx,nLocal;` |
|     - | 2001 | `	SyBlob sLocal;` |
|     - | 2002 | `	const char *zL;` |
|     - | 2003 | `	xmlNsPtr pNs;` |
|     - | 2004 | `	xmlAttrPtr pAttr;` |
|     - | 2005 | `	SyBlob sVal;` |
|     9 | 2006 | `	if( nQ < 1 ){` |
|   ! 0 | 2007 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 2008 | `			"SimpleXMLElement::addAttribute(): Argument #1 ($qualifiedName) must not be empty");` |
|     - | 2009 | `	}` |
|     9 | 2010 | `	if( pNode == 0 \|\| pNode->type != XML_ELEMENT_NODE ){` |
|     3 | 2011 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Unable to locate parent Element");` |
|     3 | 2012 | `		return PH7_OK;` |
|     - | 2013 | `	}` |
|     7 | 2014 | `	if( nArg > 1 ){` |
|     7 | 2015 | `		zVal = ph7_value_to_string(apArg[1],&nVal);` |
|     3 | 2016 | `	}` |
|     7 | 2017 | `	if( nArg > 2 && (apArg[2]->iFlags & MEMOBJ_NULL) == 0 ){` |
|     3 | 2018 | `		zHref = ph7_value_to_string(apArg[2],&nHref);` |
|     3 | 2019 | `		bHaveNs = 1;` |
|     1 | 2020 | `	}` |
|     7 | 2021 | `	SxeSplitQName(zQ,nQ,&zPfx,&nPfx,&zLocal,&nLocal);` |
|     7 | 2022 | `	zL = SxeCopyZ(pCtx->pVm,&sLocal,zLocal,nLocal);` |
|     7 | 2023 | `	pNs = SxeResolveNs(pCtx->pVm,pNode,zPfx,nPfx,zHref,nHref,bHaveNs);` |
|     7 | 2024 | `	if( xmlHasNsProp(pNode,(const xmlChar *)zL,pNs ? pNs->href : 0) ){` |
|     3 | 2025 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Attribute already exists");` |
|     3 | 2026 | `		SyBlobRelease(&sLocal);` |
|     3 | 2027 | `		return PH7_OK;` |
|     - | 2028 | `	}` |
|     5 | 2029 | `	SyBlobInit(&sVal,&pCtx->pVm->sAllocator);` |
|     5 | 2030 | `	if( nVal > 0 ){` |
|     5 | 2031 | `		SyBlobAppend(&sVal,zVal,(sxu32)nVal);` |
|     2 | 2032 | `	}` |
|     5 | 2033 | `	SyBlobNullAppend(&sVal);` |
|     - | 2034 | ``	/* Unlike addChild, the VALUE is data: php escapes it, so `'a&b'` lands as`` |
|     - | 2035 | ``	 * three characters and comes back out as `a&amp;b`. */`` |
|     4 | 2036 | `	pAttr = pNs ? xmlNewNsProp(pNode,pNs,(const xmlChar *)zL,(const xmlChar *)"")` |
|     3 | 2037 | `		: xmlNewProp(pNode,(const xmlChar *)zL,(const xmlChar *)"");` |
|     5 | 2038 | `	SxeSetText((xmlNodePtr)pAttr,(const char *)SyBlobData(&sVal));` |
|     5 | 2039 | `	SyBlobRelease(&sVal);` |
|     5 | 2040 | `	SyBlobRelease(&sLocal);` |
|     5 | 2041 | `	return PH7_OK;` |
|     5 | 2042 | `}` |
|     - | 2043 |  |
|     - | 2044 | `/* ===== The iterator: php's Iterator and RecursiveIterator, as methods ===== */` |
|     - | 2045 |  |
|     - | 2046 | `/* The object the cursor stands on, or NULL before rewind() and past the end. */` |
|   390 | 2047 | `static ph7_class_instance * SxeCurrent(ph7_class_instance *pThis)` |
|     1 | 2048 | `{` |
|   391 | 2049 | `	return PH7_NativeAttrObj(pThis,SXE_CUR);` |
|     1 | 2050 | `}` |
|   134 | 2051 | `static void SxeSetCurrent(ph7_vm *pVm,ph7_class_instance *pThis,xmlNodePtr pNode)` |
|     1 | 2052 | `{` |
|   135 | 2053 | `	if( pNode == 0 ){` |
|     - | 2054 | `		ph7_value sNull;` |
|    63 | 2055 | `		PH7_MemObjInit(pVm,&sNull);` |
|    63 | 2056 | `		PH7_NativeSetProp(pVm,pThis,SXE_CUR,sizeof(SXE_CUR)-1,&sNull);` |
|    63 | 2057 | `		PH7_MemObjRelease(&sNull);` |
|    63 | 2058 | `		return;` |
|     - | 2059 | `	}` |
|     - | 2060 | `	{` |
|    73 | 2061 | `		ph7_class_instance *pObj = SxeDerive(pVm,pThis,pNode,SXE_ITER_NONE,0,0);` |
|    73 | 2062 | `		if( pObj ){` |
|    73 | 2063 | `			PH7_NativeSetAttrObj(pVm,pThis,SXE_CUR,pObj);` |
|    73 | 2064 | `			PH7_ClassInstanceUnref(pObj);` |
|    36 | 2065 | `		}` |
|     - | 2066 | `	}` |
|    68 | 2067 | `}` |
|    62 | 2068 | `SXE_METHOD(vm_builtin_SimpleXMLElement_rewind)` |
|     1 | 2069 | `{` |
|    63 | 2070 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|    31 | 2071 | `	(void)nArg; (void)apArg;` |
|    63 | 2072 | `	if( pThis ){` |
|    63 | 2073 | `		SxeSetCurrent(pCtx->pVm,pThis,SxeIterFirst(pThis));` |
|    31 | 2074 | `	}` |
|    63 | 2075 | `	return PH7_OK;` |
|     1 | 2076 | `}` |
|   144 | 2077 | `SXE_METHOD(vm_builtin_SimpleXMLElement_valid)` |
|     1 | 2078 | `{` |
|   145 | 2079 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|    72 | 2080 | `	(void)nArg; (void)apArg;` |
|   145 | 2081 | `	ph7_result_bool(pCtx,pThis && SxeCurrent(pThis) != 0);` |
|   145 | 2082 | `	return PH7_OK;` |
|     1 | 2083 | `}` |
|     - | 2084 | `/* php refuses to answer current()/key() with no cursor -- before rewind() and` |
|     - | 2085 | ` * after the walk ran out -- and the refusal is an Error, not a null. */` |
|   146 | 2086 | `static ph7_class_instance * SxeCursorOrThrow(ph7_context *pCtx,ph7_class_instance *pThis,` |
|     - | 2087 | `	sxi32 *pRc)` |
|     1 | 2088 | `{` |
|   147 | 2089 | `	ph7_class_instance *pCur = pThis ? SxeCurrent(pThis) : 0;` |
|   147 | 2090 | `	*pRc = PH7_OK;` |
|   147 | 2091 | `	if( pCur == 0 ){` |
|     3 | 2092 | `		*pRc = PH7_VmThrowException(pCtx,"Error",` |
|     - | 2093 | `			"Iterator not initialized or already consumed");` |
|     1 | 2094 | `	}` |
|   147 | 2095 | `	return pCur;` |
|     1 | 2096 | `}` |
|    74 | 2097 | `SXE_METHOD(vm_builtin_SimpleXMLElement_current)` |
|     1 | 2098 | `{` |
|    75 | 2099 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|     - | 2100 | `	sxi32 rc;` |
|    75 | 2101 | `	ph7_class_instance *pCur = SxeCursorOrThrow(pCtx,pThis,&rc);` |
|    37 | 2102 | `	(void)nArg; (void)apArg;` |
|    75 | 2103 | `	if( pCur == 0 ){` |
|     3 | 2104 | `		return rc;` |
|     - | 2105 | `	}` |
|    73 | 2106 | `	return SxeResultBorrowed(pCtx,pCur);` |
|    38 | 2107 | `}` |
|    72 | 2108 | `SXE_METHOD(vm_builtin_SimpleXMLElement_key)` |
|     1 | 2109 | `{` |
|    73 | 2110 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|     - | 2111 | `	sxi32 rc;` |
|    73 | 2112 | `	ph7_class_instance *pCur = SxeCursorOrThrow(pCtx,pThis,&rc);` |
|     - | 2113 | `	xmlNodePtr pNode;` |
|    36 | 2114 | `	(void)nArg; (void)apArg;` |
|    73 | 2115 | `	if( pCur == 0 ){` |
|   ! 0 | 2116 | `		return rc;` |
|     - | 2117 | `	}` |
|    73 | 2118 | `	pNode = SxeNodeOf(pCur);` |
|    73 | 2119 | `	if( pNode == 0 \|\| pNode->name == 0 ){` |
|   ! 0 | 2120 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 | 2121 | `		return PH7_OK;` |
|     - | 2122 | `	}` |
|    73 | 2123 | `	ph7_result_string(pCtx,(const char *)pNode->name,(int)xmlStrlen(pNode->name));` |
|    73 | 2124 | `	return PH7_OK;` |
|    37 | 2125 | `}` |
|    72 | 2126 | `SXE_METHOD(vm_builtin_SimpleXMLElement_next)` |
|     1 | 2127 | `{` |
|    73 | 2128 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|    73 | 2129 | `	ph7_class_instance *pCur = pThis ? SxeCurrent(pThis) : 0;` |
|    36 | 2130 | `	(void)nArg; (void)apArg;` |
|    73 | 2131 | `	if( pThis == 0 ){` |
|   ! 0 | 2132 | `		return PH7_OK;` |
|     - | 2133 | `	}` |
|    73 | 2134 | `	SxeSetCurrent(pCtx->pVm,pThis,pCur ? SxeIterNext(pThis,SxeNodeOf(pCur)) : 0);` |
|    73 | 2135 | `	return PH7_OK;` |
|    37 | 2136 | `}` |
|     - | 2137 | `/* RecursiveIterator: does the node the cursor stands on have element children,` |
|     - | 2138 | ` * and the node itself as the thing to descend into. An attribute list never` |
|     - | 2139 | ` * has either. */` |
|    14 | 2140 | `SXE_METHOD(vm_builtin_SimpleXMLElement_hasChildren)` |
|     1 | 2141 | `{` |
|    15 | 2142 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|    15 | 2143 | `	ph7_class_instance *pCur = pThis ? SxeCurrent(pThis) : 0;` |
|     - | 2144 | `	xmlNodePtr pWalk;` |
|     7 | 2145 | `	(void)nArg; (void)apArg;` |
|    15 | 2146 | `	if( pCur == 0 \|\| SxeTypeOf(pThis) == SXE_ITER_ATTRLIST ){` |
|   ! 0 | 2147 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2148 | `		return PH7_OK;` |
|     - | 2149 | `	}` |
|    15 | 2150 | `	pWalk = SxeNodeOf(pCur);` |
|    25 | 2151 | `	for( pWalk = pWalk ? pWalk->children : 0 ; pWalk ; pWalk = pWalk->next ){` |
|    15 | 2152 | `		if( pWalk->type == XML_ELEMENT_NODE ){` |
|     5 | 2153 | `			ph7_result_bool(pCtx,1);` |
|     5 | 2154 | `			return PH7_OK;` |
|     - | 2155 | `		}` |
|     6 | 2156 | `	}` |
|    11 | 2157 | `	ph7_result_bool(pCtx,0);` |
|    11 | 2158 | `	return PH7_OK;` |
|     8 | 2159 | `}` |
|    14 | 2160 | `SXE_METHOD(vm_builtin_SimpleXMLElement_getChildren)` |
|     1 | 2161 | `{` |
|    15 | 2162 | `	ph7_class_instance *pThis = SxeThis(pCtx);` |
|    15 | 2163 | `	ph7_class_instance *pCur = pThis ? SxeCurrent(pThis) : 0;` |
|     7 | 2164 | `	(void)nArg; (void)apArg;` |
|    15 | 2165 | `	if( pCur == 0 \|\| SxeTypeOf(pThis) == SXE_ITER_ATTRLIST ){` |
|   ! 0 | 2166 | `		ph7_result_null(pCtx);` |
|   ! 0 | 2167 | `		return PH7_OK;` |
|     - | 2168 | `	}` |
|    15 | 2169 | `	return SxeResultBorrowed(pCtx,pCur);` |
|     8 | 2170 | `}` |
|     - | 2171 |  |
|     - | 2172 | `/* ===== Loading ===== */` |
|     - | 2173 |  |
|     - | 2174 | `/*` |
|     - | 2175 | ` * Parse bytes into a document, with php's error routing: libxml's diagnostics` |
|     - | 2176 | ``  * go through the per-VM queue -- which is what `libxml_use_internal_errors()` `` |
|     - | 2177 | `` * turns off and what `@` and error_reporting() screen -- rather than onto`` |
|     - | 2178 | ` * stderr.  Answers the registered shell, or 0.` |
|     - | 2179 | ` */` |
|   150 | 2180 | `static phl_xmldoc * SxeParse(ph7_context *pCtx,const char *zSrc,int nLen,` |
|     - | 2181 | `	const char *zUrl,int iOpts,const char *zFn)` |
|     3 | 2182 | `{` |
|   153 | 2183 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 2184 | `	xmlDocPtr pDoc;` |
|     - | 2185 | `	phl_xmldoc *pShell;` |
|   153 | 2186 | `	sxu32 nMark = PH7_LibxmlCaptureBeginRaw(pVm);` |
|   153 | 2187 | `	pDoc = xmlReadMemory(zSrc,nLen,zUrl,0,iOpts);` |
|   153 | 2188 | `	PH7_LibxmlCaptureEndOpts(pVm,nMark,zFn,iOpts);` |
|   153 | 2189 | `	if( pDoc == 0 ){` |
|    11 | 2190 | `		return 0;` |
|     - | 2191 | `	}` |
|   143 | 2192 | `	if( xmlDocGetRootElement(pDoc) == 0 ){` |
|     - | 2193 | `		/* php answers false for a document with no element to stand on. */` |
|   ! 0 | 2194 | `		xmlFreeDoc(pDoc);` |
|   ! 0 | 2195 | `		return 0;` |
|     - | 2196 | `	}` |
|   143 | 2197 | `	pShell = PH7_LibxmlNewDoc(pVm,pDoc);` |
|   143 | 2198 | `	if( pShell == 0 ){` |
|   ! 0 | 2199 | `		xmlFreeDoc(pDoc);` |
|   ! 0 | 2200 | `	}` |
|   143 | 2201 | `	return pShell;` |
|    78 | 2202 | `}` |
|     - | 2203 | `/* The class the two loaders build, screened the way php screens it: null is` |
|     - | 2204 | ` * SimpleXMLElement, and anything that is not a SUBCLASS of it is a TypeError` |
|     - | 2205 | ` * naming the argument. */` |
|   154 | 2206 | `static ph7_class * SxeArgClass(ph7_context *pCtx,ph7_value *pVal,const char *zFn,sxi32 *pRc)` |
|     3 | 2207 | `{` |
|   157 | 2208 | `	ph7_vm *pVm = pCtx->pVm;` |
|   157 | 2209 | `	ph7_class *pBase = PH7_VmExtractClass(pVm,"SimpleXMLElement",` |
|     - | 2210 | `		sizeof("SimpleXMLElement")-1,FALSE,0);` |
|     - | 2211 | `	ph7_class *pClass;` |
|   157 | 2212 | `	int nName = 0;` |
|     - | 2213 | `	const char *zName;` |
|   157 | 2214 | `	*pRc = PH7_OK;` |
|   157 | 2215 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|   149 | 2216 | `		return pBase;` |
|     - | 2217 | `	}` |
|    10 | 2218 | `	zName = ph7_value_to_string(pVal,&nName);` |
|    10 | 2219 | `	pClass = PH7_VmExtractClass(pVm,zName,(sxu32)nName,TRUE,0);` |
|    10 | 2220 | `	if( pClass == 0 \|\| pBase == 0 \|\| !PH7_VmInstanceOf(pClass,pBase) ){` |
|     7 | 2221 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 2222 | `			"%s(): Argument #2 ($class_name) must be a class name derived from "` |
|     2 | 2223 | `			"SimpleXMLElement or null, %.*s given",zFn,nName,zName);` |
|     5 | 2224 | `		return 0;` |
|     - | 2225 | `	}` |
|     6 | 2226 | `	return pClass;` |
|    80 | 2227 | `}` |
|     - | 2228 | `/* The root object of a freshly parsed document. */` |
|   136 | 2229 | `static ph7_class_instance * SxeRootObject(ph7_vm *pVm,ph7_class *pClass,phl_xmldoc *pShell,` |
|     - | 2230 | `	const char *zNs,int nNs,int bPrefix)` |
|     3 | 2231 | `{` |
|   139 | 2232 | `	xmlNodePtr pRoot = xmlDocGetRootElement((xmlDocPtr)pShell->pDoc);` |
|   139 | 2233 | `	return SxeNew(pVm,pClass,pShell,pRoot,SXE_ITER_NONE,0,0,zNs,nNs,bPrefix);` |
|     3 | 2234 | `}` |
|     - | 2235 | `/*` |
|     - | 2236 | ` * The shared body of simplexml_load_string() / simplexml_load_file() and of` |
|     - | 2237 | ` * SimpleXMLElement::__construct(): the same five arguments, and the only` |
|     - | 2238 | ` * difference is what a failure IS -- false from the functions, and the` |
|     - | 2239 | `` * constructor's `Exception: String could not be parsed as XML`.`` |
|     - | 2240 | ` */` |
|   150 | 2241 | `static int SxeLoad(ph7_context *pCtx,int nArg,ph7_value **apArg,int bFile,` |
|     - | 2242 | `	const char *zFn,ph7_class *pClass,ph7_class_instance *pInto)` |
|     3 | 2243 | `{` |
|   153 | 2244 | `	ph7_vm *pVm = pCtx->pVm;` |
|   153 | 2245 | `	int nSrc = 0,nNs = 0,bPrefix = 0,iOpts = 0;` |
|   153 | 2246 | `	const char *zSrc = nArg > 0 ? ph7_value_to_string(apArg[0],&nSrc) : "";` |
|   153 | 2247 | `	const char *zNs = 0;` |
|     - | 2248 | `	phl_xmldoc *pShell;` |
|     - | 2249 | `	SyBlob sBody,sPath;` |
|   153 | 2250 | `	int iNsArg = pInto ? 3 : 3;` |
|   153 | 2251 | `	if( nArg > (pInto ? 1 : 2) ){` |
|     3 | 2252 | `		iOpts = ph7_value_to_int(apArg[pInto ? 1 : 2]);` |
|     1 | 2253 | `	}` |
|   153 | 2254 | `	if( nArg > iNsArg ){` |
|   ! 0 | 2255 | `		int n = 0;` |
|   ! 0 | 2256 | `		const char *z = ph7_value_to_string(apArg[iNsArg],&n);` |
|   ! 0 | 2257 | `		if( n > 0 ){` |
|   ! 0 | 2258 | `			zNs = z;` |
|   ! 0 | 2259 | `			nNs = n;` |
|   ! 0 | 2260 | `		}` |
|   ! 0 | 2261 | `	}` |
|   153 | 2262 | `	if( nArg > iNsArg + 1 ){` |
|   ! 0 | 2263 | `		bPrefix = ph7_value_to_bool(apArg[iNsArg + 1]);` |
|   ! 0 | 2264 | `	}` |
|   153 | 2265 | `	if( bFile ){` |
|   ! 0 | 2266 | `		if( !PH7_DomReadFile(pCtx,zSrc,nSrc,zFn,&sBody,&sPath) ){` |
|   ! 0 | 2267 | `			return 0;` |
|     - | 2268 | `		}` |
|   ! 0 | 2269 | `		pShell = SxeParse(pCtx,(const char *)SyBlobData(&sBody),(int)SyBlobLength(&sBody),` |
|   ! 0 | 2270 | `			(const char *)SyBlobData(&sPath),iOpts,zFn);` |
|   ! 0 | 2271 | `		SyBlobRelease(&sBody);` |
|   ! 0 | 2272 | `		SyBlobRelease(&sPath);` |
|   ! 0 | 2273 | `	}else{` |
|   153 | 2274 | `		pShell = SxeParse(pCtx,zSrc,nSrc,0,iOpts,zFn);` |
|     - | 2275 | `	}` |
|   153 | 2276 | `	if( pShell == 0 ){` |
|    11 | 2277 | `		return 0;` |
|     - | 2278 | `	}` |
|   143 | 2279 | `	if( pInto ){` |
|     - | 2280 | `		/* The constructor REPOINTS the object it was called on rather than` |
|     - | 2281 | `		 * making a second one, so a subclass's own constructor may have run` |
|     - | 2282 | `		 * first and its state survives. */` |
|     6 | 2283 | `		xmlNodePtr pRoot = xmlDocGetRootElement((xmlDocPtr)pShell->pDoc);` |
|     6 | 2284 | `		phl_domnode *pRes = (phl_domnode *)SyMemBackendAlloc(&pVm->sAllocator,` |
|     - | 2285 | `			sizeof(phl_domnode));` |
|     - | 2286 | `		ph7_value sVal;` |
|     6 | 2287 | `		if( pRes == 0 ){` |
|   ! 0 | 2288 | `			return 0;` |
|     - | 2289 | `		}` |
|     6 | 2290 | `		pRes->pShell = pShell;` |
|     6 | 2291 | `		pRes->pNode = pRoot;` |
|     6 | 2292 | `		PH7_MemObjInit(pVm,&sVal);` |
|     6 | 2293 | `		sVal.x.pOther = pRes;` |
|     6 | 2294 | `		sVal.iFlags = MEMOBJ_RES;` |
|     6 | 2295 | `		PH7_NativeSetProp(pVm,pInto,SXE_RES,sizeof(SXE_RES)-1,&sVal);` |
|     6 | 2296 | `		PH7_NativeSetAttrInt(pVm,pInto,SXE_IT,SXE_ITER_NONE);` |
|     6 | 2297 | `		if( zNs ){` |
|   ! 0 | 2298 | `			PH7_NativeSetAttrStr(pVm,pInto,SXE_NS,zNs,(sxu32)nNs);` |
|   ! 0 | 2299 | `			PH7_NativeSetAttrInt(pVm,pInto,SXE_ISP,bPrefix ? 1 : 0);` |
|   ! 0 | 2300 | `		}` |
|     6 | 2301 | `		return 1;` |
|     - | 2302 | `	}` |
|     - | 2303 | `	{` |
|   139 | 2304 | `		ph7_class_instance *pObj = SxeRootObject(pVm,pClass,pShell,zNs,nNs,bPrefix);` |
|   139 | 2305 | `		if( pObj == 0 ){` |
|   ! 0 | 2306 | `			return 0;` |
|     - | 2307 | `		}` |
|   139 | 2308 | `		SxeResultObj(pCtx,pObj);` |
|   139 | 2309 | `		return 1;` |
|     - | 2310 | `	}` |
|    78 | 2311 | `}` |
|     8 | 2312 | `SXE_METHOD(vm_builtin_SimpleXMLElement_construct)` |
|     2 | 2313 | `{` |
|    10 | 2314 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    10 | 2315 | `	int bUrl = nArg > 2 && ph7_value_to_bool(apArg[2]);` |
|    10 | 2316 | `	if( pThis == 0 ){` |
|   ! 0 | 2317 | `		return PH7_OK;` |
|     - | 2318 | `	}` |
|    10 | 2319 | `	if( SxeLoad(pCtx,nArg,apArg,bUrl,"SimpleXMLElement::__construct",0,pThis) ){` |
|     6 | 2320 | `		return PH7_OK;` |
|     - | 2321 | `	}` |
|     5 | 2322 | `	return PH7_VmThrowException(pCtx,"Exception","String could not be parsed as XML");` |
|     6 | 2323 | `}` |
|   146 | 2324 | `static int SxeLoadFunc(ph7_context *pCtx,int nArg,ph7_value **apArg,int bFile,const char *zFn)` |
|     3 | 2325 | `{` |
|     - | 2326 | `	sxi32 rc;` |
|   149 | 2327 | `	ph7_class *pClass = SxeArgClass(pCtx,nArg > 1 ? apArg[1] : 0,zFn,&rc);` |
|   149 | 2328 | `	if( pClass == 0 ){` |
|     5 | 2329 | `		return rc;` |
|     - | 2330 | `	}` |
|   145 | 2331 | `	if( !SxeLoad(pCtx,nArg,apArg,bFile,zFn,pClass,0) ){` |
|     7 | 2332 | `		ph7_result_bool(pCtx,0);` |
|     3 | 2333 | `	}` |
|   145 | 2334 | `	return PH7_OK;` |
|    76 | 2335 | `}` |
|   146 | 2336 | `static int vm_builtin_simplexml_load_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 | 2337 | `{` |
|   149 | 2338 | `	return SxeLoadFunc(pCtx,nArg,apArg,0,"simplexml_load_string");` |
|     3 | 2339 | `}` |
|   ! 0 | 2340 | `static int vm_builtin_simplexml_load_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2341 | `{` |
|   ! 0 | 2342 | `	return SxeLoadFunc(pCtx,nArg,apArg,1,"simplexml_load_file");` |
|   ! 0 | 2343 | `}` |
|     - | 2344 |  |
|     - | 2345 | `/* ===== The two doors between ext/simplexml and ext/dom ===== */` |
|     - | 2346 |  |
|     - | 2347 | `/*` |
|     - | 2348 | ` * simplexml_import_dom(object $node, ?string $class_name = 'SimpleXMLElement')` |
|     - | 2349 | ` *` |
|     - | 2350 | ` * The two extensions share one libxml tree, so an import is a second VIEW of it` |
|     - | 2351 | ` * and not a copy: a write through either shows in the other. php takes a` |
|     - | 2352 | ` * document (standing on its root element), an element and an attribute, and` |
|     - | 2353 | ` * warns for anything else.  It caches nothing -- two imports of one document` |
|     - | 2354 | ` * are two objects, unlike dom_import_simplexml's one.` |
|     - | 2355 | ` */` |
|    10 | 2356 | `static int vm_builtin_simplexml_import_dom(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2357 | `{` |
|    11 | 2358 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 2359 | `	ph7_class_instance *pArg;` |
|     - | 2360 | `	ph7_value *pRes;` |
|     - | 2361 | `	phl_domnode *pNd;` |
|     - | 2362 | `	xmlNodePtr pNode;` |
|     - | 2363 | `	ph7_class *pClass;` |
|     - | 2364 | `	sxi32 rc;` |
|    11 | 2365 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 | 2366 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 2367 | `			"simplexml_import_dom(): Argument #1 ($node) must be a valid XML node");` |
|     - | 2368 | `	}` |
|    11 | 2369 | `	pArg = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    11 | 2370 | `	pRes = pArg ? PH7_NativeAttr(pArg,"__res") : 0;` |
|    11 | 2371 | `	if( pRes == 0 \|\| (pRes->iFlags & MEMOBJ_RES) == 0 ){` |
|     3 | 2372 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 2373 | `			"simplexml_import_dom(): Argument #1 ($node) must be a valid XML node");` |
|     - | 2374 | `	}` |
|     9 | 2375 | `	pClass = SxeArgClass(pCtx,nArg > 1 ? apArg[1] : 0,"simplexml_import_dom",&rc);` |
|     9 | 2376 | `	if( pClass == 0 ){` |
|   ! 0 | 2377 | `		return rc;` |
|     - | 2378 | `	}` |
|     9 | 2379 | `	pNd = (phl_domnode *)pRes->x.pOther;` |
|     9 | 2380 | `	pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     9 | 2381 | `	if( pNode && (pNode->type == XML_DOCUMENT_NODE \|\| pNode->type == XML_HTML_DOCUMENT_NODE) ){` |
|     9 | 2382 | `		pNode = xmlDocGetRootElement((xmlDocPtr)pNode);` |
|     4 | 2383 | `	}` |
|     8 | 2384 | `	if( pNode == 0` |
|     8 | 2385 | `	 \|\| (pNode->type != XML_ELEMENT_NODE && pNode->type != XML_ATTRIBUTE_NODE) ){` |
|     3 | 2386 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid Nodetype to import");` |
|     3 | 2387 | `		ph7_result_null(pCtx);` |
|     3 | 2388 | `		return PH7_OK;` |
|     - | 2389 | `	}` |
|     7 | 2390 | `	return SxeResultObj(pCtx,SxeNew(pVm,pClass,pNd->pShell,pNode,SXE_ITER_NONE,0,0,0,0,0));` |
|     6 | 2391 | `}` |
|     - | 2392 | `/*` |
|     - | 2393 | ` * dom_import_simplexml(object $node): DOMAttr\|DOMElement -- ext/dom's half of` |
|     - | 2394 | ` * the same door, declared by that extension and bodied here because it is a` |
|     - | 2395 | ` * SimpleXML object it takes apart.` |
|     - | 2396 | ` *` |
|     - | 2397 | ` * Unlike its opposite this one has an IDENTITY: two imports of the same node` |
|     - | 2398 | ` * are the same DOM object, because ext/dom caches its wrappers per document.` |
|     - | 2399 | ` */` |
|    10 | 2400 | `static int vm_builtin_dom_import_simplexml(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2401 | `{` |
|     - | 2402 | `	ph7_class_instance *pArg;` |
|     - | 2403 | `	phl_domnode *pNd;` |
|     - | 2404 | `	xmlNodePtr pNode;` |
|     - | 2405 | `	ph7_class_instance *pObj;` |
|     - | 2406 | `	ph7_value sRes;` |
|    11 | 2407 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 | 2408 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 2409 | `			"dom_import_simplexml(): Argument #1 ($node) is not a valid node type");` |
|     - | 2410 | `	}` |
|    11 | 2411 | `	pArg = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    11 | 2412 | `	pNd = pArg ? SxeResOf(pArg) : 0;` |
|    11 | 2413 | `	pNode = pArg ? SxeMethodNode(pArg) : 0;` |
|    10 | 2414 | `	if( pNd == 0 \|\| pNode == 0` |
|     9 | 2415 | `	 \|\| (pNode->type != XML_ELEMENT_NODE && pNode->type != XML_ATTRIBUTE_NODE) ){` |
|     3 | 2416 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 2417 | `			"dom_import_simplexml(): Argument #1 ($node) is not a valid node type");` |
|     - | 2418 | `	}` |
|     9 | 2419 | `	pObj = PH7_DomWrapForeign(pCtx->pVm,pNd->pShell,pNode);` |
|     9 | 2420 | `	if( pObj == 0 ){` |
|   ! 0 | 2421 | `		ph7_result_null(pCtx);` |
|   ! 0 | 2422 | `		return PH7_OK;` |
|     - | 2423 | `	}` |
|     9 | 2424 | `	PH7_MemObjInit(pCtx->pVm,&sRes);` |
|     9 | 2425 | `	sRes.x.pOther = pObj;` |
|     9 | 2426 | `	sRes.iFlags = MEMOBJ_OBJ;` |
|     9 | 2427 | `	ph7_result_value(pCtx,&sRes);   /* the cache owns pObj; borrowed */` |
|     9 | 2428 | `	return PH7_OK;` |
|     6 | 2429 | `}` |
|     - | 2430 |  |
|     - | 2431 | `/* ===== Install ===== */` |
|     - | 2432 |  |
|     - | 2433 | `/*` |
|     - | 2434 | `` * `clone $x` on a SimpleXML object copies the DOCUMENT, exactly as ext/dom's`` |
|     - | 2435 | ` * document clone does: php's SimpleXML clone_obj duplicates the tree so a write` |
|     - | 2436 | ` * through the copy does not reach the original.` |
|     - | 2437 | ` */` |
|   ! 0 | 2438 | `static void SxeClone(ph7_vm *pVm,ph7_class_instance *pCopy,ph7_class_instance *pSrc)` |
|   ! 0 | 2439 | `{` |
|   ! 0 | 2440 | `	phl_domnode *pNd = SxeResOf(pSrc);` |
|   ! 0 | 2441 | `	xmlDocPtr pDoc = pNd && pNd->pShell ? (xmlDocPtr)pNd->pShell->pDoc : 0;` |
|   ! 0 | 2442 | `	xmlDocPtr pNew = pDoc ? xmlCopyDoc(pDoc,1) : 0;` |
|   ! 0 | 2443 | `	phl_xmldoc *pShell = pNew ? PH7_LibxmlNewDoc(pVm,pNew) : 0;` |
|     - | 2444 | `	phl_domnode *pRes;` |
|     - | 2445 | `	ph7_value sVal;` |
|   ! 0 | 2446 | `	if( pShell == 0 ){` |
|   ! 0 | 2447 | `		if( pNew ){` |
|   ! 0 | 2448 | `			xmlFreeDoc(pNew);` |
|   ! 0 | 2449 | `		}` |
|   ! 0 | 2450 | `		return;` |
|     - | 2451 | `	}` |
|   ! 0 | 2452 | `	pRes = (phl_domnode *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_domnode));` |
|   ! 0 | 2453 | `	if( pRes == 0 ){` |
|   ! 0 | 2454 | `		return;` |
|     - | 2455 | `	}` |
|   ! 0 | 2456 | `	pRes->pShell = pShell;` |
|     - | 2457 | `	/* Only the ROOT survives a document copy identifiably; php's clone of a` |
|     - | 2458 | `	 * deeper object stands on the copy's root too. */` |
|   ! 0 | 2459 | `	pRes->pNode = xmlDocGetRootElement(pNew);` |
|   ! 0 | 2460 | `	PH7_MemObjInit(pVm,&sVal);` |
|   ! 0 | 2461 | `	sVal.x.pOther = pRes;` |
|   ! 0 | 2462 | `	sVal.iFlags = MEMOBJ_RES;` |
|   ! 0 | 2463 | `	PH7_NativeSetProp(pVm,pCopy,SXE_RES,sizeof(SXE_RES)-1,&sVal);` |
|   ! 0 | 2464 | `}` |
|     - | 2465 | `/*` |
|     - | 2466 | ` * php's compare handler for the class -- and it is NODE IDENTITY, not the` |
|     - | 2467 | ` * property table the rest of its surface shows.` |
|     - | 2468 | ` *` |
|     - | 2469 | `` * So `$x->kid == $x->kid` is true (both stand on the same element and ask the`` |
|     - | 2470 | ` * same question of it) while two documents parsed from the SAME BYTES are never` |
|     - | 2471 | ` * equal, and neither are two objects over different nodes however alike their` |
|     - | 2472 | ` * tables. An object with no node equals another with no node and nothing else.` |
|     - | 2473 | ` */` |
|    12 | 2474 | `static void SxeCmp(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeCmpCtx *pCtx)` |
|     1 | 2475 | `{` |
|     - | 2476 | `	ph7_class_instance *pOther;` |
|     - | 2477 | `	xmlNodePtr pA,pB;` |
|     6 | 2478 | `	(void)pVm;` |
|    13 | 2479 | `	pOther = pCtx->pOther;` |
|    13 | 2480 | `	if( pOther == 0 \|\| PH7_NativeAttr(pOther,SXE_RES) == 0 ){` |
|     7 | 2481 | `		return;` |
|     - | 2482 | `	}` |
|     7 | 2483 | `	pA = SxeNodeOf(pThis);` |
|     7 | 2484 | `	pB = SxeNodeOf(pOther);` |
|     7 | 2485 | `	pCtx->bAnswered = 1;` |
|     7 | 2486 | `	pCtx->iResult = (pA == 0 && pB == 0) ? 0 : (pA == pB ? 0 : 1);` |
|     7 | 2487 | `}` |
|  6721 | 2488 | `PH7_PRIVATE sxi32 PH7_VmInstallSimpleXml(ph7_vm *pVm)` |
|     5 | 2489 | `{` |
|     - | 2490 | `	static const struct {` |
|     - | 2491 | `		const char *zName;` |
|     - | 2492 | `		ProchHostFunction xFunc;` |
|     - | 2493 | `	} aFunc[] = {` |
|     - | 2494 | `		{ "simplexml_load_file",   vm_builtin_simplexml_load_file   },` |
|     - | 2495 | `		{ "simplexml_load_string", vm_builtin_simplexml_load_string },` |
|     - | 2496 | `		{ "simplexml_import_dom",  vm_builtin_simplexml_import_dom  },` |
|     - | 2497 | `		/* ext/dom's own name for the other direction. */` |
|     - | 2498 | `		{ "dom_import_simplexml",  vm_builtin_dom_import_simplexml  },` |
|     - | 2499 | `	};` |
|     - | 2500 | `	/*` |
|     - | 2501 | `	 * The engine slots. php's SimpleXMLElement declares NO property -- Reflection` |
|     - | 2502 | ``	 * lists none and `property_exists()` is false for every name -- so each of`` |
|     - | 2503 | `	 * these is PH7_MOD_HIDDEN and the whole object shows the document instead.` |
|     - | 2504 | `	 */` |
|     - | 2505 | `	static const PH7_NativePropDef aProp[] = {` |
|     - | 2506 | `		{ SXE_RES,  PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 2507 | `		{ SXE_IT,   PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,  0, 0, 0.0 }, 0 },` |
|     - | 2508 | `		{ SXE_NM,   PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 2509 | `		{ SXE_NS,   PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 2510 | `		{ SXE_ISP,  PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,  0, 0, 0.0 }, 0 },` |
|     - | 2511 | `		{ SXE_CUR,  PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 2512 | `		{ SXE_XPNS, PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 2513 | `	};` |
|     - | 2514 | `	/*` |
|     - | 2515 | `	 * php's own list, in php's own order -- which is what Reflection reports and` |
|     - | 2516 | ``	 * what `get_class_methods()` answers. Every return type is TENTATIVE (`@`):`` |
|     - | 2517 | `	 * php's stubs mark the whole class that way, so a subclass may still declare` |
|     - | 2518 | ``	 * `children()` returning something else without a fatal.`` |
|     - | 2519 | `	 */` |
|     - | 2520 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|     - | 2521 | `		{ "xpath", PH7_MOD_PUBLIC, "string $expression", "@array\|false\|null",` |
|     - | 2522 | `		  vm_builtin_SimpleXMLElement_xpath },` |
|     - | 2523 | `		{ "registerXPathNamespace", PH7_MOD_PUBLIC, "string $prefix, string $namespace",` |
|     - | 2524 | `		  "@bool", vm_builtin_SimpleXMLElement_registerXPathNamespace },` |
|     - | 2525 | `		{ "asXML", PH7_MOD_PUBLIC, "?string $filename = null", "@string\|bool",` |
|     - | 2526 | `		  vm_builtin_SimpleXMLElement_asXML },` |
|     - | 2527 | `		{ "saveXML", PH7_MOD_PUBLIC, "?string $filename = null", "@string\|bool",` |
|     - | 2528 | `		  vm_builtin_SimpleXMLElement_asXML },` |
|     - | 2529 | `		{ "getNamespaces", PH7_MOD_PUBLIC, "bool $recursive = false", "@array",` |
|     - | 2530 | `		  vm_builtin_SimpleXMLElement_getNamespaces },` |
|     - | 2531 | `		{ "getDocNamespaces", PH7_MOD_PUBLIC, "bool $recursive = false, bool $fromRoot = true",` |
|     - | 2532 | `		  "@array\|false", vm_builtin_SimpleXMLElement_getDocNamespaces },` |
|     - | 2533 | `		{ "children", PH7_MOD_PUBLIC, "?string $namespaceOrPrefix = null, bool $isPrefix = false",` |
|     - | 2534 | `		  "@?SimpleXMLElement", vm_builtin_SimpleXMLElement_children },` |
|     - | 2535 | `		{ "attributes", PH7_MOD_PUBLIC, "?string $namespaceOrPrefix = null, bool $isPrefix = false",` |
|     - | 2536 | `		  "@?SimpleXMLElement", vm_builtin_SimpleXMLElement_attributes },` |
|     - | 2537 | `		{ "__construct", PH7_MOD_PUBLIC,` |
|     - | 2538 | `		  "string $data, int $options = 0, bool $dataIsURL = false, "` |
|     - | 2539 | `		  "string $namespaceOrPrefix = '', bool $isPrefix = false", "",` |
|     - | 2540 | `		  vm_builtin_SimpleXMLElement_construct },` |
|     - | 2541 | `		{ "addChild", PH7_MOD_PUBLIC,` |
|     - | 2542 | `		  "string $qualifiedName, ?string $value = null, ?string $namespace = null",` |
|     - | 2543 | `		  "@?SimpleXMLElement", vm_builtin_SimpleXMLElement_addChild },` |
|     - | 2544 | `		{ "addAttribute", PH7_MOD_PUBLIC,` |
|     - | 2545 | `		  "string $qualifiedName, string $value, ?string $namespace = null", "@void",` |
|     - | 2546 | `		  vm_builtin_SimpleXMLElement_addAttribute },` |
|     - | 2547 | `		{ "getName", PH7_MOD_PUBLIC, "", "@string", vm_builtin_SimpleXMLElement_getName },` |
|     - | 2548 | `		{ "__toString", PH7_MOD_PUBLIC, "", "string", vm_builtin_SimpleXMLElement_toString },` |
|     - | 2549 | `		{ "__debugInfo", PH7_MOD_PUBLIC, "", "?array", vm_builtin_SimpleXMLElement_debugInfo },` |
|     - | 2550 | `		{ "count", PH7_MOD_PUBLIC, "", "@int", vm_builtin_SimpleXMLElement_count },` |
|     - | 2551 | `		{ "rewind", PH7_MOD_PUBLIC, "", "@void", vm_builtin_SimpleXMLElement_rewind },` |
|     - | 2552 | `		{ "valid", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SimpleXMLElement_valid },` |
|     - | 2553 | `		{ "current", PH7_MOD_PUBLIC, "", "@SimpleXMLElement",` |
|     - | 2554 | `		  vm_builtin_SimpleXMLElement_current },` |
|     - | 2555 | `		{ "key", PH7_MOD_PUBLIC, "", "@string", vm_builtin_SimpleXMLElement_key },` |
|     - | 2556 | `		{ "next", PH7_MOD_PUBLIC, "", "@void", vm_builtin_SimpleXMLElement_next },` |
|     - | 2557 | `		{ "hasChildren", PH7_MOD_PUBLIC, "", "@bool",` |
|     - | 2558 | `		  vm_builtin_SimpleXMLElement_hasChildren },` |
|     - | 2559 | `		{ "getChildren", PH7_MOD_PUBLIC, "", "@?SimpleXMLElement",` |
|     - | 2560 | `		  vm_builtin_SimpleXMLElement_getChildren },` |
|     - | 2561 | `	};` |
|     - | 2562 | `	/*` |
|     - | 2563 | `	 * php's class list. SimpleXMLElement declares Stringable, Countable and` |
|     - | 2564 | ``	 * RecursiveIterator -- and NOT ArrayAccess, which is why `$x['a']` is a`` |
|     - | 2565 | ``	 * dimension handler and `$x instanceof ArrayAccess` is false; and not`` |
|     - | 2566 | `	 * JsonSerializable, because json_encode() reads its property table like any` |
|     - | 2567 | `	 * other object's.` |
|     - | 2568 | `	 *` |
|     - | 2569 | `	 * PH7_CLASS_NUM_AS_STRING is php's cast_object answering IS_LONG/IS_DOUBLE` |
|     - | 2570 | `	 * from the node's text, and PH7_CLASS_VARS_PRESENT is its get_properties` |
|     - | 2571 | `	 * answering the get_object_vars purpose too -- the two places SimpleXML does` |
|     - | 2572 | `	 * not behave like every other native class here.` |
|     - | 2573 | `	 *` |
|     - | 2574 | `	 * SimpleXMLIterator adds nothing: php declares it as an empty subclass, kept` |
|     - | 2575 | `	 * because RecursiveIteratorIterator over one is how the class is used.` |
|     - | 2576 | `	 */` |
|     - | 2577 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - | 2578 | `		{ "SimpleXMLElement", 0, "Stringable,Countable,RecursiveIterator",` |
|     - | 2579 | `		  PH7_CLASS_NOSERIALIZE\|PH7_CLASS_NUM_AS_STRING\|PH7_CLASS_VARS_PRESENT,` |
|     - | 2580 | `		  aMethod, SX_ARRAYSIZE(aMethod), 0, 0, aProp, SX_ARRAYSIZE(aProp),` |
|     - | 2581 | `		  0, 0, SxePresent },` |
|     - | 2582 | `		{ "SimpleXMLIterator", "SimpleXMLElement", 0,` |
|     - | 2583 | `		  PH7_CLASS_NOSERIALIZE\|PH7_CLASS_NUM_AS_STRING\|PH7_CLASS_VARS_PRESENT,` |
|     - | 2584 | `		  0, 0, 0, 0, 0, 0, 0, 0, SxePresent },` |
|     - | 2585 | `	};` |
|     - | 2586 | `	sxi32 rc;` |
|     - | 2587 | `	sxu32 n;` |
| 33610 | 2588 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFunc) ; n++ ){` |
| 26889 | 2589 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
| 13429 | 2590 | `	}` |
|  6726 | 2591 | `	rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|  6726 | 2592 | `	if( rc == SXRET_OK ){` |
|     - | 2593 | `		/* The four handlers php gives the class, assigned on the mounted class` |
|     - | 2594 | `		 * because PH7_NativeClassSpec carries no field for any of them. Stated on` |
|     - | 2595 | `		 * the ROOT only: the engine walks the base chain, which is php's own` |
|     - | 2596 | `		 * handler inheritance, so SimpleXMLIterator and a userland subclass reach` |
|     - | 2597 | `		 * these. */` |
|  6726 | 2598 | `		ph7_class *pClass = PH7_VmExtractClass(&(*pVm),"SimpleXMLElement",` |
|     - | 2599 | `			sizeof("SimpleXMLElement")-1,FALSE,0);` |
|  6726 | 2600 | `		if( pClass ){` |
|  6726 | 2601 | `			pClass->xDim = SxeDimHook;` |
|  6726 | 2602 | `			pClass->xClone = SxeClone;` |
|  6726 | 2603 | `			pClass->xBool = SxeBool;` |
|  6726 | 2604 | `			pClass->xCmp = SxeCmp;` |
|  3356 | 2605 | `		}` |
|  6726 | 2606 | `		PH7_NativeClassInstallPropHook(&(*pVm),"SimpleXMLElement",SxePropHook);` |
|  3356 | 2607 | `	}` |
|  6726 | 2608 | `	return rc;` |
|     5 | 2609 | `}` |
|     - | 2610 |  |
|     - | 2611 | `#else` |
|     - | 2612 | `/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */` |
|     - | 2613 | `typedef int vm_simplexml_unused;` |
|     - | 2614 | `#endif /* PH7_ENABLE_LIBXML */` |
|     - | 2615 |  |
