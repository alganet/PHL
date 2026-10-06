# src/ph7/vm_serialize.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1038/1099 lines (94.45%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    4 | ` */` |
|     - |    5 | `#include "ph7int.h"` |
|     - |    6 | `#include <stdio.h>  /* snprintf for the shortest-round-trip float repr */` |
|     - |    7 | `#include <stdlib.h> /* strtod / atoi */` |
|     - |    8 | `/*` |
|     - |    9 | ` * Section:` |
|     - |   10 | ` *  PHP serialize()/unserialize() — the real PHP serialization format.` |
|     - |   11 | ` *` |
|     - |   12 | ` *  Format (byte lengths; raw bytes, not escaped):` |
|     - |   13 | ` *    N;  b:0;/b:1;  i:<int>;  d:<shortest>;  s:<bytelen>:"<raw>";` |
|     - |   14 | ` *    a:<count>:{ <key><val> ... }` |
|     - |   15 | ` *    O:<namelen>:"<Class>":<count>:{ <key><val> ... }` |
|     - |   16 | ` *  Object property keys: public -> "name"; protected -> "\0*\0name";` |
|     - |   17 | ` *  private -> "\0<DeclClass>\0name" (the s: length counts the NULs).` |
|     - |   18 | ` *` |
|     - |   19 | ` *  Back-reference graph: every VALUE emitted takes the next number in php's own` |
|     - |   20 | ` *  count (keys take none, and neither does a repeated REFERENCE), so a second` |
|     - |   21 | `` *  sighting of the same object writes `r:<n>;` and a second sighting of the same`` |
|     - |   22 | `` *  reference writes `R:<n>;`. That is what makes a shared object still shared`` |
|     - |   23 | ` *  after a round trip -- and what lets a cyclic value be written at all.` |
|     - |   24 | ` *` |
|     - |   25 | ` *  Documented divergences from PHP 8.5:` |
|     - |   26 | ` *   - the Serializable C: tag is not honored (such a class serializes by the` |
|     - |   27 | ` *     default O: path).` |
|     - |   28 | ` *   - an ALLOWED class's undeclared payload property is skipped (php creates a` |
|     - |   29 | ` *     dynamic property behind a deprecation; PHL's scope policy has no dynamic` |
|     - |   30 | ` *     properties outside stdClass and the __PHP_Incomplete_Class carrier, whose` |
|     - |   31 | ` *     properties are all dynamic and keep their RAW mangled keys).` |
|     - |   32 | ` */` |
|     - |   33 | `#define SERIALIZE_MAX_DEPTH 4096` |
|     - |   34 |  |
|     - |   35 | `/* ----------------------------------------------------------------------------` |
|     - |   36 | ` * Serializer` |
|     - |   37 | ` * ------------------------------------------------------------------------- */` |
|     - |   38 | `/*` |
|     - |   39 | `` * php's `var_hash`: what has already been written, and under which number.`` |
|     - |   40 | ` *` |
|     - |   41 | ` * Two identity spaces share the rule and nothing else, so they get a table each:` |
|     - |   42 | ` * an OBJECT is itself (the instance address), and a REFERENCE is the value slot` |
|     - |   43 | ` * its holders share. Open addressing over a power-of-two ring, because the only` |
|     - |   44 | ` * questions asked of it are "seen?" and "remember", the keys are machine words,` |
|     - |   45 | ` * and a serialize() of a large graph asks them once per value -- SyHash borrows` |
|     - |   46 | ` * its key bytes rather than copying them, which a key that lives in a local` |
|     - |   47 | ` * cannot satisfy. Key 0 marks a free slot, so every key is stored biased by one.` |
|     - |   48 | ` */` |
|     - |   49 | `typedef struct VmSerRefTab VmSerRefTab;` |
|     - |   50 | `struct VmSerRefTab` |
|     - |   51 | `{` |
|     - |   52 | `	SyMemBackend *pAlloc; /* Where the ring comes from */` |
|     - |   53 | `	sxu64 *aKey;          /* Biased key, 0 == free */` |
|     - |   54 | `	sxu32 *aNum;          /* The back-reference number recorded for it */` |
|     - |   55 | `	sxu32 nSlot;          /* Ring size (a power of two), 0 until the first insert */` |
|     - |   56 | `	sxu32 nUsed;          /* Occupied slots */` |
|     - |   57 | `};` |
|     - |   58 | `typedef struct serialize_data serialize_data;` |
|     - |   59 | `struct serialize_data` |
|     - |   60 | `{` |
|     - |   61 | `	ph7_vm *pVm;          /* The underlying VM */` |
|     - |   62 | `	ph7_context *pCtx;    /* Call context (for throwing exceptions) */` |
|     - |   63 | `	SyBlob *pOut;         /* Output accumulator */` |
|     - |   64 | `	int depth;            /* Current nesting level (cycle guard) */` |
|     - |   65 | `	int exc;              /* A magic method threw -> propagate the exception */` |
|     - |   66 | `	int err;              /* Recursion overflow or bad input -> serialize returns false */` |
|     - |   67 | `	sxu32 n;              /* php's counter: how many VALUES have been written */` |
|     - |   68 | `	VmSerRefTab sObj;     /* instance address -> its number */` |
|     - |   69 | `	VmSerRefTab sRef;     /* shared value slot -> its number */` |
|     - |   70 | `};` |
|     - |   71 | `static sxi32 VmSerialize(ph7_value *pIn, serialize_data *pData, int isRef, sxu32 nSlot);` |
|  1784 |   72 | `static void VmSerRefTabInit(VmSerRefTab *pTab, SyMemBackend *pAlloc)` |
|     5 |   73 | `{` |
|  1789 |   74 | `	pTab->pAlloc = pAlloc;` |
|  1789 |   75 | `	pTab->aKey = 0;` |
|  1789 |   76 | `	pTab->aNum = 0;` |
|  1789 |   77 | `	pTab->nSlot = 0;` |
|  1789 |   78 | `	pTab->nUsed = 0;` |
|  1789 |   79 | `}` |
|  1784 |   80 | `static void VmSerRefTabRelease(VmSerRefTab *pTab)` |
|     5 |   81 | `{` |
|  1789 |   82 | `	if( pTab->aKey ){ SyMemBackendFree(pTab->pAlloc,pTab->aKey); }` |
|  1789 |   83 | `	if( pTab->aNum ){ SyMemBackendFree(pTab->pAlloc,pTab->aNum); }` |
|  1789 |   84 | `	pTab->aKey = 0; pTab->aNum = 0; pTab->nSlot = 0; pTab->nUsed = 0;` |
|  1789 |   85 | `}` |
|     - |   86 | `/* The ring position a biased key belongs at: its own slot, or the first free one. */` |
|   716 |   87 | `static sxu32 VmSerRefTabProbe(VmSerRefTab *pTab, sxu64 nKey)` |
|     5 |   88 | `{` |
|   721 |   89 | `	sxu32 i = (sxu32)((nKey ^ (nKey >> 32)) * 2654435761u) & (pTab->nSlot - 1);` |
|   793 |   90 | `	while( pTab->aKey[i] != 0 && pTab->aKey[i] != nKey ){` |
|    73 |   91 | `		i = (i + 1) & (pTab->nSlot - 1);` |
|     1 |   92 | `	}` |
|   721 |   93 | `	return i;` |
|     5 |   94 | `}` |
|   608 |   95 | `static int VmSerRefTabGrow(VmSerRefTab *pTab)` |
|     5 |   96 | `{` |
|   613 |   97 | `	sxu32 nNew = pTab->nSlot ? pTab->nSlot * 2 : 64;` |
|   613 |   98 | `	sxu64 *aKey = (sxu64 *)SyMemBackendAlloc(pTab->pAlloc,nNew * (sxu32)sizeof(sxu64));` |
|   613 |   99 | `	sxu32 *aNum = (sxu32 *)SyMemBackendAlloc(pTab->pAlloc,nNew * (sxu32)sizeof(sxu32));` |
|   613 |  100 | `	sxu64 *aOldKey = pTab->aKey;` |
|   613 |  101 | `	sxu32 *aOldNum = pTab->aNum;` |
|   613 |  102 | `	sxu32 nOld = pTab->nSlot, i;` |
|   613 |  103 | `	if( aKey == 0 \|\| aNum == 0 ){` |
|   ! 0 |  104 | `		if( aKey ){ SyMemBackendFree(pTab->pAlloc,aKey); }` |
|   ! 0 |  105 | `		if( aNum ){ SyMemBackendFree(pTab->pAlloc,aNum); }` |
|   ! 0 |  106 | `		return 0;` |
|     - |  107 | `	}` |
|   613 |  108 | `	SyZero(aKey,nNew * (sxu32)sizeof(sxu64));` |
|   613 |  109 | `	pTab->aKey = aKey; pTab->aNum = aNum; pTab->nSlot = nNew;` |
|   613 |  110 | `	for( i = 0 ; i < nOld ; ++i ){` |
|   ! 0 |  111 | `		if( aOldKey[i] != 0 ){` |
|   ! 0 |  112 | `			sxu32 j = VmSerRefTabProbe(pTab,aOldKey[i]);` |
|   ! 0 |  113 | `			pTab->aKey[j] = aOldKey[i];` |
|   ! 0 |  114 | `			pTab->aNum[j] = aOldNum[i];` |
|   ! 0 |  115 | `		}` |
|   ! 0 |  116 | `	}` |
|   613 |  117 | `	if( aOldKey ){ SyMemBackendFree(pTab->pAlloc,aOldKey); }` |
|   613 |  118 | `	if( aOldNum ){ SyMemBackendFree(pTab->pAlloc,aOldNum); }` |
|   613 |  119 | `	return 1;` |
|   309 |  120 | `}` |
|     - |  121 | `/* Answer the number this key was recorded under, or record the given one. */` |
|   716 |  122 | `static int VmSerRefTabSeen(VmSerRefTab *pTab, sxu64 nRaw, sxu32 nNum, sxu32 *pnSeen)` |
|     5 |  123 | `{` |
|   721 |  124 | `	sxu64 nKey = nRaw + 1;` |
|     - |  125 | `	sxu32 i;` |
|   721 |  126 | `	if( pTab->nSlot == 0 \|\| (pTab->nUsed + 1) * 4 >= pTab->nSlot * 3 ){` |
|   613 |  127 | `		if( !VmSerRefTabGrow(pTab) ){ return 0; }` |
|   304 |  128 | `	}` |
|   721 |  129 | `	i = VmSerRefTabProbe(pTab,nKey);` |
|   721 |  130 | `	if( pTab->aKey[i] == nKey ){` |
|    53 |  131 | `		*pnSeen = pTab->aNum[i];` |
|    53 |  132 | `		return 1;` |
|     - |  133 | `	}` |
|   669 |  134 | `	pTab->aKey[i] = nKey;` |
|   669 |  135 | `	pTab->aNum[i] = nNum;` |
|   669 |  136 | `	pTab->nUsed++;` |
|   669 |  137 | `	return 0;` |
|   363 |  138 | `}` |
|     - |  139 | `/*` |
|     - |  140 | ` * Append the shortest decimal string that round-trips to the given double, in` |
|     - |  141 | ` * PHP's gcvt/serialize style: uppercase 'E' exponent with no leading zeros and a` |
|     - |  142 | ` * "1.0E+20"-style mantissa; INF/-INF/NAN spelled out. PHP switches to the` |
|     - |  143 | ` * exponential form when the leading-digit exponent e satisfies e >= 17 or` |
|     - |  144 | ` * e <= -5 (php_gcvt with ndigit == 17), and to decimal otherwise. Emits just the` |
|     - |  145 | ` * number (no "d:"/";") so var_export can reuse it (see PH7_AppendShortestReal` |
|     - |  146 | ` * decl in ph7int.h).` |
|     - |  147 | ` */` |
|  2440 |  148 | `PH7_PRIVATE void PH7_AppendShortestReal(SyBlob *pOut, double d)` |
|     5 |  149 | `{` |
|     - |  150 | `	char zExp[64];` |
|     - |  151 | `	char zDig[24];   /* significant digits, no sign/point */` |
|     - |  152 | `	const char *p;` |
|     - |  153 | `	int sig, nDig, e, decpt, neg;` |
|  2528 |  154 | `	if( PH7_IS_NAN(d) ){ SyBlobAppend(pOut,"NAN",3); return; }` |
|  2347 |  155 | `	if( PH7_IS_INF(d) ){ SyBlobAppend(pOut, d<0.0?"-INF":"INF", d<0.0?4:3); return; }` |
|     - |  156 | `	/* Find the fewest significant digits that re-parse bit-exactly. */` |
|  9469 |  157 | `	for( sig = 1; sig <= 17; sig++ ){` |
|  9469 |  158 | `		snprintf(zExp,sizeof(zExp),"%.*e",sig-1,d);` |
|  9469 |  159 | `		if( strtod(zExp,0) == d ){ break; }` |
|  3649 |  160 | `	}` |
|  2181 |  161 | `	if( sig > 17 ){ sig = 17; snprintf(zExp,sizeof(zExp),"%.*e",sig-1,d); }` |
|     - |  162 | `	/* Parse "[-]D[.DDD]e[+-]XX": collect digits and the leading-digit exponent. */` |
|  2181 |  163 | `	p = zExp;` |
|  2181 |  164 | `	neg = 0;` |
|  2181 |  165 | `	if( *p == '-' ){ neg = 1; p++; }` |
|  2181 |  166 | `	nDig = 0;` |
| 12573 |  167 | `	while( *p && *p != 'e' && *p != 'E' ){` |
| 10397 |  168 | `		if( *p >= '0' && *p <= '9' && nDig < (int)sizeof(zDig) ){ zDig[nDig++] = *p; }` |
| 10397 |  169 | `		p++;` |
|     5 |  170 | `	}` |
|  2181 |  171 | `	e = (*p) ? atoi(p+1) : 0;` |
|  2181 |  172 | `	while( nDig > 1 && zDig[nDig-1] == '0' ){ nDig--; } /* trim trailing zeros */` |
|  2181 |  173 | `	decpt = e + 1; /* digits to the left of the decimal point */` |
|  2181 |  174 | `	if( neg ){ SyBlobAppend(pOut,"-",1); }` |
|  2181 |  175 | `	if( decpt > 17 \|\| decpt < -3 ){` |
|     - |  176 | `		/* Exponential: <lead>.<rest>E<sign><exp> (mantissa always has a dot). */` |
|   497 |  177 | `		SyBlobAppend(pOut,&zDig[0],1);` |
|   497 |  178 | `		SyBlobAppend(pOut,".",1);` |
|   497 |  179 | `		if( nDig > 1 ){ SyBlobAppend(pOut,&zDig[1],nDig-1); }` |
|   223 |  180 | `		else { SyBlobAppend(pOut,"0",1); }` |
|   497 |  181 | `		SyBlobFormat(pOut,"E%c%d", e<0?'-':'+', e<0?-e:e);` |
|  1934 |  182 | `	}else if( decpt <= 0 ){` |
|     - |  183 | `		/* 0.<zeros><digits> */` |
|     - |  184 | `		int i;` |
|   296 |  185 | `		SyBlobAppend(pOut,"0.",2);` |
|   422 |  186 | `		for( i = 0; i < -decpt; i++ ){ SyBlobAppend(pOut,"0",1); }` |
|   296 |  187 | `		SyBlobAppend(pOut,zDig,nDig);` |
|  1540 |  188 | `	}else if( decpt >= nDig ){` |
|     - |  189 | `		/* <digits><zeros> (integer) */` |
|     - |  190 | `		int i;` |
|   917 |  191 | `		SyBlobAppend(pOut,zDig,nDig);` |
|  1209 |  192 | `		for( i = 0; i < decpt-nDig; i++ ){ SyBlobAppend(pOut,"0",1); }` |
|   461 |  193 | `	}else{` |
|     - |  194 | `		/* <int>.<frac> */` |
|   481 |  195 | `		SyBlobAppend(pOut,zDig,decpt);` |
|   481 |  196 | `		SyBlobAppend(pOut,".",1);` |
|   481 |  197 | `		SyBlobAppend(pOut,&zDig[decpt],nDig-decpt);` |
|     - |  198 | `	}` |
|  1225 |  199 | `}` |
|     - |  200 | `/* Serialize a double as d:<shortest>; */` |
|    80 |  201 | `static void VmSerializeReal(SyBlob *pOut, double d)` |
|     2 |  202 | `{` |
|    82 |  203 | `	SyBlobAppend(pOut,"d:",2);` |
|    82 |  204 | `	PH7_AppendShortestReal(pOut,d);` |
|    82 |  205 | `	SyBlobAppend(pOut,";",1);` |
|    82 |  206 | `}` |
|     - |  207 | `/* Emit s:<bytelen>:"<raw>"; for an arbitrary byte string. */` |
|  5188 |  208 | `static void VmSerializeRawString(SyBlob *pOut, const char *z, int n)` |
|     5 |  209 | `{` |
|  5193 |  210 | `	SyBlobFormat(pOut,"s:%u:\"",(unsigned)n);` |
|  5193 |  211 | `	if( n > 0 ){ SyBlobAppend(pOut,z,(sxu32)n); }` |
|  5193 |  212 | `	SyBlobAppend(pOut,"\";",2);` |
|  5193 |  213 | `}` |
|     - |  214 | `/* An array KEY, straight off the node. php writes it without a value's number:` |
|     - |  215 | ` * a key is never a back-reference target and never advances the counter. */` |
|  5090 |  216 | `static void VmSerializeNodeKey(SyBlob *pOut, ph7_hashmap_node *pNode)` |
|     5 |  217 | `{` |
|  5095 |  218 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|  4263 |  219 | `		SyBlobFormat(pOut,"i:%qd;",pNode->xKey.iKey);` |
|  2134 |  220 | `	}else{` |
|  1251 |  221 | `		VmSerializeRawString(pOut,(const char *)SyBlobData(&pNode->xKey.sKey),` |
|   832 |  222 | `			(int)SyBlobLength(&pNode->xKey.sKey));` |
|     - |  223 | `	}` |
|  5095 |  224 | `}` |
|     - |  225 | `/*` |
|     - |  226 | `` * Every `<key><value>` pair of a map, walked over the NODES rather than through`` |
|     - |  227 | ` * ph7_array_walk: the walker hands out a copy of each value, and a copy has lost` |
|     - |  228 | ` * the two things the back-reference graph is made of -- which slot the element` |
|     - |  229 | `` * shares (php's `Z_ISREF`) and which slot that is.`` |
|     - |  230 | ` *` |
|     - |  231 | `` * `$a[0] = &$a` is the one element whose slot has no other holder to count, so`` |
|     - |  232 | `` * PH7_HashmapNodeIsRef answers no for it once the name `$a` is gone; it is still`` |
|     - |  233 | ` * php's reference, and without the second test serialize() would walk it for` |
|     - |  234 | `` * ever instead of writing `R:`.`` |
|     - |  235 | ` */` |
|   556 |  236 | `static void VmSerializeMapEntries(ph7_hashmap *pMap, serialize_data *pData)` |
|     5 |  237 | `{` |
|   561 |  238 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|   561 |  239 | `	sxu32 n = pMap->nEntry;` |
|  5651 |  240 | `	while( n > 0 && pEntry ){` |
|     - |  241 | `		ph7_value *pVal;` |
|     - |  242 | `		int isRef;` |
|  5097 |  243 | `		if( pData->err \|\| pData->exc ){ return; }` |
|  5095 |  244 | `		VmSerializeNodeKey(pData->pOut,pEntry);` |
|  5095 |  245 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  7619 |  246 | `		isRef = PH7_HashmapNodeIsRef(pEntry)` |
|  5090 |  247 | `			\|\| (pVal && (pVal->iFlags & MEMOBJ_HASHMAP) && (ph7_hashmap *)pVal->x.pOther == pMap);` |
|  5095 |  248 | `		if( pVal ){` |
|  5095 |  249 | `			VmSerialize(pVal,pData,isRef,pEntry->nValIdx);` |
|  2550 |  250 | `		}else{` |
|   ! 0 |  251 | `			pData->n++;` |
|   ! 0 |  252 | `			SyBlobAppend(pData->pOut,"N;",2);` |
|     - |  253 | `		}` |
|  5095 |  254 | `		pEntry = pEntry->pPrev;   /* the map's linear order (see PH7_HashmapWalk) */` |
|  5095 |  255 | `		n--;` |
|     5 |  256 | `	}` |
|   283 |  257 | `}` |
|     - |  258 | `/* Emit an object property key with the proper visibility mangling. pOwner is the class` |
|     - |  259 | ` * being serialized: a private key names the class that OWNS the property, and a trait's` |
|     - |  260 | ` * members are owned by the class that composed them, never by the trait. */` |
|   266 |  261 | `static void VmSerializePropKey(SyBlob *pOut, ph7_class_attr *pAttr, ph7_class *pOwner)` |
|     4 |  262 | `{` |
|   270 |  263 | `	const char *zName = SyStringData(&pAttr->sName);` |
|   270 |  264 | `	int nName = (int)SyStringLength(&pAttr->sName);` |
|   270 |  265 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PUBLIC ){` |
|   201 |  266 | `		VmSerializeRawString(pOut,zName,nName);` |
|   169 |  267 | `	}else if( pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|     - |  268 | `		/* "\0*\0" + name */` |
|    25 |  269 | `		SyBlobFormat(pOut,"s:%u:\"",(unsigned)(nName+3));` |
|    25 |  270 | `		SyBlobAppend(pOut,"\0*\0",3);` |
|    25 |  271 | `		SyBlobAppend(pOut,zName,(sxu32)nName);` |
|    25 |  272 | `		SyBlobAppend(pOut,"\";",2);` |
|    13 |  273 | `	}else{` |
|     - |  274 | `		/* private: "\0<DeclClass>\0" + name */` |
|    46 |  275 | `		ph7_class *pDecl = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pOwner);` |
|    46 |  276 | `		const char *zCls = pDecl ? SyStringData(&pDecl->sName) : "";` |
|    46 |  277 | `		int nCls = pDecl ? (int)SyStringLength(&pDecl->sName) : 0;` |
|    46 |  278 | `		SyBlobFormat(pOut,"s:%u:\"",(unsigned)(nName+nCls+2));` |
|    46 |  279 | `		SyBlobAppend(pOut,"\0",1);` |
|    46 |  280 | `		SyBlobAppend(pOut,zCls,(sxu32)nCls);` |
|    46 |  281 | `		SyBlobAppend(pOut,"\0",1);` |
|    46 |  282 | `		SyBlobAppend(pOut,zName,(sxu32)nName);` |
|    46 |  283 | `		SyBlobAppend(pOut,"\";",2);` |
|     - |  284 | `	}` |
|   270 |  285 | `}` |
|     - |  286 | `/* True if an attribute is a serializable instance property (not static/const). */` |
|   540 |  287 | `static int VmAttrIsProperty(VmClassAttr *pVmAttr)` |
|     4 |  288 | `{` |
|   544 |  289 | `	if( PH7_ClassAttrUninitialized(pVmAttr) ){` |
|     - |  290 | `		/* A typed property never written is not there yet: php's payload has no` |
|     - |  291 | `		 * entry for it and its count is one lower. */` |
|    34 |  292 | `		return 0;` |
|     - |  293 | `	}` |
|     - |  294 | `	/* php 8.4: VIRTUAL hooked properties have no backing store — serialize()` |
|     - |  295 | `	 * excludes them (raw surface; the get hook is NOT consulted).` |
|     - |  296 | `	 *` |
|     - |  297 | `	 * PH7_CLASS_ATTR_HIDDEN is excluded too, which makes serialize() agree with the` |
|     - |  298 | `	 * other eight presentation surfaces at last. It could not be until 3 Aug: a` |
|     - |  299 | `	 * native class's engine slot is the only place its state lives, so hiding it` |
|     - |  300 | ``	 * without php's replacement made `unserialize(serialize($date))` answer an EMPTY`` |
|     - |  301 | ``	 * object. php's replacement is an `__serialize`/`__unserialize` pair, and every`` |
|     - |  302 | `	 * class here that php round-trips now declares one (the date family, the SPL` |
|     - |  303 | `	 * containers, ArrayObject/ArrayIterator). What is left holding a hidden slot is` |
|     - |  304 | `	 * the SPL DECORATOR family, whose state php does not round-trip either — its` |
|     - |  305 | ``	 * payload is `O:16:"IteratorIterator":0:{}`, which is exactly what dropping the`` |
|     - |  306 | `	 * slots produces. */` |
|   524 |  307 | `	return !PH7_ATTR_UNPRESENTED(pVmAttr)` |
|   641 |  308 | `		&& (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL) == 0;` |
|   274 |  309 | `}` |
|     - |  310 | `/* __sleep() walker state: emit each named property in the array's order. */` |
|     - |  311 | `typedef struct sleep_ctx sleep_ctx;` |
|     - |  312 | `struct sleep_ctx` |
|     - |  313 | `{` |
|     - |  314 | `	serialize_data *pData;` |
|     - |  315 | `	ph7_class_instance *pThis;` |
|     - |  316 | `	sxu32 nCount;` |
|     - |  317 | `};` |
|     8 |  318 | `static int VmSleepWalk(ph7_value *pKey, ph7_value *pName, void *pUserData)` |
|     2 |  319 | `{` |
|    10 |  320 | `	sleep_ctx *pS = (sleep_ctx *)pUserData;` |
|    10 |  321 | `	serialize_data *pData = pS->pData;` |
|     - |  322 | `	SyHashEntry *pHE;` |
|     - |  323 | `	VmClassAttr *pVmAttr;` |
|     - |  324 | `	ph7_value *pVal;` |
|     - |  325 | `	const char *zName;` |
|     - |  326 | `	int nName;` |
|    10 |  327 | `	if( pData->err \|\| pData->exc \|\| !ph7_value_is_string(pName) ){ return PH7_OK; }` |
|    10 |  328 | `	zName = ph7_value_to_string(pName,&nName);` |
|    10 |  329 | `	pHE = SyHashGet(&pS->pThis->hAttr,zName,(sxu32)nName);` |
|    10 |  330 | `	if( pHE == 0 ){ return PH7_OK; } /* PHP notices a missing prop; we skip it */` |
|    10 |  331 | `	pVmAttr = (VmClassAttr *)pHE->pUserData;` |
|    10 |  332 | `	if( !VmAttrIsProperty(pVmAttr) ){ return PH7_OK; }` |
|    10 |  333 | `	VmSerializePropKey(pData->pOut,pVmAttr->pAttr,pS->pThis->pClass);` |
|    10 |  334 | `	pVal = PH7_ClassInstanceExtractAttrValue(pS->pThis,pVmAttr);` |
|    10 |  335 | `	if( pVal ){` |
|    10 |  336 | `		VmSerialize(pVal,pData,PH7_ClassAttrIsRef(pS->pThis,pVmAttr),pVmAttr->nIdx);` |
|     6 |  337 | `	}else{` |
|   ! 0 |  338 | `		pData->n++;` |
|   ! 0 |  339 | `		SyBlobAppend(pData->pOut,"N;",2);` |
|     - |  340 | `	}` |
|    10 |  341 | `	pS->nCount++;` |
|     4 |  342 | `	SXUNUSED(pKey);` |
|    10 |  343 | `	return PH7_OK;` |
|     6 |  344 | `}` |
|     - |  345 | `/* Emit "O:<len>:"Class":<count>:{" + body + "}" from a pre-built body blob. */` |
|   460 |  346 | `static void VmSerializeObjectHeader(SyBlob *pOut, SyString *pClassName, sxu32 nCount, SyBlob *pBody)` |
|     5 |  347 | `{` |
|   465 |  348 | `	SyBlobFormat(pOut,"O:%u:\"",(unsigned)pClassName->nByte);` |
|   465 |  349 | `	SyBlobAppend(pOut,pClassName->zString,pClassName->nByte);` |
|   465 |  350 | `	SyBlobFormat(pOut,"\":%u:{",nCount);` |
|   465 |  351 | `	if( SyBlobLength(pBody) > 0 ){ SyBlobAppend(pOut,SyBlobData(pBody),SyBlobLength(pBody)); }` |
|   465 |  352 | `	SyBlobAppend(pOut,"}",1);` |
|   465 |  353 | `}` |
|     - |  354 | `/*` |
|     - |  355 | ` * php refuses to serialize some classes, and it does so in TWO different places.` |
|     - |  356 | ` *` |
|     - |  357 | `` * `ZEND_ACC_NOT_SERIALIZABLE` (PH7_CLASS_NOSERIALIZE) is tested before anything`` |
|     - |  358 | `` * else, so a subclass declaring `__serialize()` is refused all the same; a deny`` |
|     - |  359 | `` * `ce->serialize` HANDLER (PH7_CLASS_NOSERIALIZE_SUBOK) is consulted only after the`` |
|     - |  360 | ` * magic lookup, so there the subclass wins — and php's sentence says which kind you` |
|     - |  361 | ` * hit. Both ride down to every user subclass, which is why this walks pBase: the` |
|     - |  362 | `` * receiver's own iFlags are empty for `class Kid extends SplFileInfo {}`, and PHL`` |
|     - |  363 | ` * happily serialized one where php refuses.` |
|     - |  364 | ` */` |
|  1382 |  365 | `static int VmClassRefusesSerialize(ph7_class *pClass,sxi32 iFlag)` |
|     5 |  366 | `{` |
|  2913 |  367 | `	while( pClass ){` |
|  1691 |  368 | `		if( pClass->iFlags & iFlag ){` |
|   164 |  369 | `			return 1;` |
|     - |  370 | `		}` |
|  1531 |  371 | `		pClass = pClass->pBase;` |
|     5 |  372 | `	}` |
|  1227 |  373 | `	return 0;` |
|   696 |  374 | `}` |
|     - |  375 | `/* Serialize a class instance, honoring __serialize()/__sleep() then the default.` |
|     - |  376 | ` * The object body is built into a temp blob (so the entry count and __sleep's` |
|     - |  377 | ` * array order come out right) before the O: header is written. */` |
|   638 |  378 | `static sxi32 VmSerializeObject(ph7_value *pIn, serialize_data *pData)` |
|     5 |  379 | `{` |
|   643 |  380 | `	ph7_class_instance *pThis = (ph7_class_instance *)pIn->x.pOther;` |
|   643 |  381 | `	ph7_vm *pVm = pData->pVm;` |
|   643 |  382 | `	SyString *pClassName = &pThis->pClass->sName;` |
|     - |  383 | `	ph7_class_method *pMethod;` |
|     - |  384 | `	SyHashEntry *pEntry;` |
|     - |  385 | `	VmClassAttr *pVmAttr;` |
|     - |  386 | `	SyBlob sBody, *pSave;` |
|   643 |  387 | `	sxu32 nCount = 0;` |
|     - |  388 | `	/* Anonymous classes cannot be serialized (PHP throws an Exception). php names` |
|     - |  389 | `	 * the class in the refusal, so an anonymous subclass of Base reports` |
|     - |  390 | ``	 * `Base@anonymous` -- the DISPLAY half of the name, which is also what makes`` |
|     - |  391 | `	 * the test below exact: only a synthesized name is shorter than its own. */` |
|   643 |  392 | `	if( pThis->pClass->sDisp.nByte != pClassName->nByte ){` |
|    10 |  393 | `		PH7_VmThrowException(pData->pCtx,"Exception",` |
|     6 |  394 | `			"Serialization of '%z' is not allowed",&pThis->pClass->sDisp);` |
|     7 |  395 | `		pData->exc = 1;` |
|     7 |  396 | `		return PH7_EXCEPTION;` |
|     - |  397 | `	}` |
|     - |  398 | `	/* Nor can a class holding engine state — Closure, Fiber, Generator, WeakReference,` |
|     - |  399 | `	 * WeakMap. Guard before the generic object path would otherwise emit their private` |
|     - |  400 | `	 * slots, which for the native ones are raw pointers. php names the RECEIVER, so a` |
|     - |  401 | `	 * subclass of one reports its own name. */` |
|   637 |  402 | `	if( VmClassRefusesSerialize(pThis->pClass,PH7_CLASS_NOSERIALIZE) ){` |
|   184 |  403 | `		PH7_VmThrowException(pData->pCtx,"Exception",` |
|    60 |  404 | `			"Serialization of '%z' is not allowed",pClassName);` |
|   124 |  405 | `		pData->exc = 1;` |
|   124 |  406 | `		return PH7_EXCEPTION;` |
|     - |  407 | `	}` |
|     - |  408 | `	/* Enum cases serialize as php 8.1's E: tag — E:<len>:"Class:CASE"; — so` |
|     - |  409 | ``	 * unserialize restores THE case singleton, preserving `===` identity. */`` |
|   517 |  410 | `	if( pThis->pClass->iFlags & PH7_CLASS_ENUM ){` |
|    15 |  411 | `		ph7_value *pName = PH7_EnumCaseNameValue(pThis);` |
|    15 |  412 | `		sxu32 nName = pName ? SyBlobLength(&pName->sBlob) : 0;` |
|    15 |  413 | `		SyBlobFormat(pData->pOut,"E:%u:\"",(unsigned)(pClassName->nByte + 1 + nName));` |
|    15 |  414 | `		SyBlobAppend(pData->pOut,pClassName->zString,pClassName->nByte);` |
|    15 |  415 | `		SyBlobAppend(pData->pOut,":",1);` |
|    15 |  416 | `		if( nName > 0 ){` |
|    15 |  417 | `			SyBlobAppend(pData->pOut,SyBlobData(&pName->sBlob),nName);` |
|     7 |  418 | `		}` |
|    15 |  419 | `		SyBlobAppend(pData->pOut,"\";",2);` |
|    15 |  420 | `		return SXRET_OK;` |
|     - |  421 | `	}` |
|     - |  422 | `	/* An INCOMPLETE object re-serializes as the ORIGINAL class, byte for byte:` |
|     - |  423 | `	 * the class name is the magic member's value (the carrier's own name when a` |
|     - |  424 | `	 * hand-built instance never had one), the magic member itself is dropped,` |
|     - |  425 | `	 * and no magic method is consulted — the carrier has none and php would not` |
|     - |  426 | `	 * ask. The declared COUNT is php's own arithmetic — the property total minus` |
|     - |  427 | `	 * one, floored at zero — and it is decided BEFORE the body, which produces` |
|     - |  428 | `	 * two quirks on a hand-built carrier that never had a name member: a count of` |
|     - |  429 | `	 * zero writes NO body however many properties are there, and any higher count` |
|     - |  430 | `	 * writes them ALL, one more than it declared. Both are php's output. */` |
|   503 |  431 | `	if( PH7_VmIsIncompleteClass(pVm,pThis->pClass) ){` |
|    29 |  432 | `		SyString sOutName = *pClassName;` |
|    29 |  433 | `		SyHashEntry *pMagic = SyHashGet(&pThis->hAttr,` |
|     - |  434 | `			(const void *)PH7_INCOMPLETE_MAGIC_MEMBER,sizeof(PH7_INCOMPLETE_MAGIC_MEMBER)-1);` |
|    29 |  435 | `		sxu32 nTotal = pThis->hAttr.nEntry;` |
|    29 |  436 | `		sxu32 nEmit = nTotal > 0 ? nTotal - 1 : 0;` |
|    29 |  437 | `		if( pMagic && pMagic->pUserData ){` |
|    28 |  438 | `			ph7_value *pNameVal = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,` |
|    18 |  439 | `				((VmClassAttr *)pMagic->pUserData)->nIdx);` |
|    19 |  440 | `			if( pNameVal && (pNameVal->iFlags & MEMOBJ_STRING) && SyBlobLength(&pNameVal->sBlob) > 0 ){` |
|    19 |  441 | `				SyStringInitFromBuf(&sOutName,` |
|     - |  442 | `					(const char *)SyBlobData(&pNameVal->sBlob),SyBlobLength(&pNameVal->sBlob));` |
|     9 |  443 | `			}` |
|     9 |  444 | `		}` |
|    29 |  445 | `		SyBlobInit(&sBody,&pVm->sAllocator);` |
|    29 |  446 | `		pSave = pData->pOut;` |
|    29 |  447 | `		pData->pOut = &sBody;` |
|    29 |  448 | `		pData->depth++;` |
|    29 |  449 | `		if( nEmit > 0 ){` |
|    21 |  450 | `			SyHashResetLoopCursor(&pThis->hAttr);` |
|    69 |  451 | `			while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|     - |  452 | `				ph7_value *pVal;` |
|    49 |  453 | `				pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    48 |  454 | `				if( pEntry->nKeyLen == sizeof(PH7_INCOMPLETE_MAGIC_MEMBER)-1` |
|    33 |  455 | `				 && SyMemcmp(pEntry->pKey,PH7_INCOMPLETE_MAGIC_MEMBER,pEntry->nKeyLen) == 0 ){` |
|    17 |  456 | `					continue; /* the magic member is metadata, not a property */` |
|     - |  457 | `				}` |
|     - |  458 | `				/* The key is stored RAW (mangling bytes included): emit it as-is. */` |
|    33 |  459 | `				VmSerializeRawString(&sBody,(const char *)pEntry->pKey,(int)pEntry->nKeyLen);` |
|    33 |  460 | `				pVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|    33 |  461 | `				if( pVal ){` |
|    33 |  462 | `					VmSerialize(pVal,pData,PH7_ClassAttrIsRef(pThis,pVmAttr),pVmAttr->nIdx);` |
|    17 |  463 | `				}else{` |
|   ! 0 |  464 | `					pData->n++;` |
|   ! 0 |  465 | `					SyBlobAppend(&sBody,"N;",2);` |
|     - |  466 | `				}` |
|     1 |  467 | `			}` |
|    10 |  468 | `		}` |
|    29 |  469 | `		pData->depth--;` |
|    29 |  470 | `		pData->pOut = pSave;` |
|    29 |  471 | `		if( !pData->exc && !pData->err ){` |
|    29 |  472 | `			VmSerializeObjectHeader(pData->pOut,&sOutName,nEmit,&sBody);` |
|    14 |  473 | `		}` |
|    29 |  474 | `		SyBlobRelease(&sBody);` |
|    29 |  475 | `		return pData->exc ? PH7_EXCEPTION : PH7_OK;` |
|     - |  476 | `	}` |
|   475 |  477 | `	SyBlobInit(&sBody,&pVm->sAllocator);` |
|   475 |  478 | `	pSave = pData->pOut;` |
|   475 |  479 | `	pData->pOut = &sBody;     /* recursion appends to the body blob */` |
|   475 |  480 | `	pData->depth++;` |
|     - |  481 | `	/* (1) __serialize(): the returned array's pairs become the body verbatim. */` |
|   475 |  482 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__serialize",sizeof("__serialize")-1);` |
|   475 |  483 | `	if( pMethod ){` |
|     - |  484 | `		ph7_value sRes;` |
|     - |  485 | `		sxi32 rc;` |
|   254 |  486 | `		PH7_MemObjInit(pVm,&sRes);` |
|   254 |  487 | `		rc = PH7_VmCallMagicMethod(pVm,pThis,pMethod,&sRes,0,0);` |
|   254 |  488 | `		if( rc == PH7_EXCEPTION ){ pData->exc = 1; }` |
|   242 |  489 | `		else if( !ph7_value_is_array(&sRes) ){ pData->err = 1; }` |
|     - |  490 | `		else {` |
|   242 |  491 | `			nCount = ph7_array_count(&sRes);` |
|   242 |  492 | `			VmSerializeMapEntries((ph7_hashmap *)sRes.x.pOther,pData);` |
|     - |  493 | `		}` |
|   254 |  494 | `		PH7_MemObjRelease(&sRes);` |
|   254 |  495 | `		goto done;` |
|     - |  496 | `	}` |
|     - |  497 | `	/* (2) __sleep(): emit the named properties in the array's order. */` |
|   224 |  498 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__sleep",sizeof("__sleep")-1);` |
|   224 |  499 | `	if( pMethod ){` |
|     - |  500 | `		ph7_value sRes;` |
|     - |  501 | `		sxi32 rc;` |
|    39 |  502 | `		PH7_MemObjInit(pVm,&sRes);` |
|    39 |  503 | `		rc = PH7_VmCallMagicMethod(pVm,pThis,pMethod,&sRes,0,0);` |
|    39 |  504 | `		if( rc == PH7_EXCEPTION ){ pData->exc = 1; }` |
|    12 |  505 | `		else if( ph7_value_is_array(&sRes) ){` |
|     - |  506 | `			sleep_ctx sleepCtx;` |
|    12 |  507 | `			sleepCtx.pData = pData; sleepCtx.pThis = pThis; sleepCtx.nCount = 0;` |
|    12 |  508 | `			ph7_array_walk(&sRes,VmSleepWalk,&sleepCtx);` |
|    12 |  509 | `			nCount = sleepCtx.nCount;` |
|     5 |  510 | `		}` |
|    39 |  511 | `		PH7_MemObjRelease(&sRes);` |
|    39 |  512 | `		goto done;` |
|     - |  513 | `	}` |
|     - |  514 | `	/* (3) php's deny HANDLER, which sits HERE and not with the flag above: the two` |
|     - |  515 | `	 * magic methods win over it, so a subclass of a DOM node that declares either one` |
|     - |  516 | ``	 * serializes normally and php's sentence names that escape. `__wakeup()` alone is`` |
|     - |  517 | `	 * not one of the two. */` |
|   188 |  518 | `	if( VmClassRefusesSerialize(pThis->pClass,PH7_CLASS_NOSERIALIZE_SUBOK) ){` |
|   ! 0 |  519 | `		PH7_VmThrowException(pData->pCtx,"Exception",` |
|     - |  520 | `			"Serialization of '%z' is not allowed, unless serialization methods "` |
|   ! 0 |  521 | `			"are implemented in a subclass",pClassName);` |
|   ! 0 |  522 | `		pData->exc = 1;` |
|   ! 0 |  523 | `		goto done;` |
|     - |  524 | `	}` |
|     - |  525 | `	/* (4) default: every non-static/const property in declaration order. */` |
|   188 |  526 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|   720 |  527 | `	while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|     - |  528 | `		ph7_value *pVal;` |
|   536 |  529 | `		pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|   536 |  530 | `		if( !VmAttrIsProperty(pVmAttr) ){ continue; }` |
|   262 |  531 | `		VmSerializePropKey(&sBody,pVmAttr->pAttr,pThis->pClass);` |
|   262 |  532 | `		pVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|   262 |  533 | `		if( pVal ){` |
|   262 |  534 | `			VmSerialize(pVal,pData,PH7_ClassAttrIsRef(pThis,pVmAttr),pVmAttr->nIdx);` |
|   133 |  535 | `		}else{` |
|   ! 0 |  536 | `			pData->n++;` |
|   ! 0 |  537 | `			SyBlobAppend(&sBody,"N;",2);` |
|     - |  538 | `		}` |
|   262 |  539 | `		nCount++;` |
|     4 |  540 | `	}` |
|    92 |  541 | `done:` |
|   475 |  542 | `	pData->depth--;` |
|   475 |  543 | `	pData->pOut = pSave;` |
|   475 |  544 | `	if( !pData->exc && !pData->err ){` |
|   437 |  545 | `		VmSerializeObjectHeader(pData->pOut,pClassName,nCount,&sBody);` |
|   216 |  546 | `	}` |
|   475 |  547 | `	SyBlobRelease(&sBody);` |
|   475 |  548 | `	return pData->exc ? PH7_EXCEPTION : PH7_OK;` |
|   324 |  549 | `}` |
|     - |  550 | `/*` |
|     - |  551 | ` * One value, and the number it takes.` |
|     - |  552 | ` *` |
|     - |  553 | ` * isRef says the SLOT this value was reached through is shared -- php's` |
|     - |  554 | `` * `Z_ISREF_P` -- and nSlot names it. php's rule, in the order it applies them:`` |
|     - |  555 | ` * the counter advances for every value; only an object or a reference is` |
|     - |  556 | ` * remembered; a reference whose target is an OBJECT is remembered as that object` |
|     - |  557 | `` * (`['p' => $o, 'q' => &$r]` writes `R:` against the number `$o` already took);`` |
|     - |  558 | ` * and a repeated reference gives its number back, so it costs nothing and the` |
|     - |  559 | `` * count stays in step with the reader's, which pushes nothing for an `R:` either.`` |
|     - |  560 | ` */` |
|  6280 |  561 | `static sxi32 VmSerialize(ph7_value *pIn, serialize_data *pData, int isRef, sxu32 nSlot)` |
|     5 |  562 | `{` |
|  6285 |  563 | `	SyBlob *pOut = pData->pOut;` |
|  6285 |  564 | `	if( pData->err \|\| pData->exc ){ return PH7_OK; }` |
|  6285 |  565 | `	if( pData->depth > SERIALIZE_MAX_DEPTH ){ pData->err = 1; return PH7_OK; }` |
|  6285 |  566 | `	pData->n++;` |
|  6285 |  567 | `	if( isRef \|\| ph7_value_is_object(pIn) ){` |
|   721 |  568 | `		int bObj = ph7_value_is_object(pIn) != 0;` |
|   721 |  569 | `		VmSerRefTab *pTab = bObj ? &pData->sObj : &pData->sRef;` |
|   721 |  570 | `		sxu64 nKey = bObj ? (sxu64)(size_t)pIn->x.pOther : (sxu64)nSlot;` |
|   721 |  571 | `		sxu32 nSeen = 0;` |
|   721 |  572 | `		if( VmSerRefTabSeen(pTab,nKey,pData->n,&nSeen) ){` |
|    53 |  573 | `			if( isRef ){` |
|    25 |  574 | `				pData->n--;` |
|    25 |  575 | `				SyBlobFormat(pOut,"R:%u;",nSeen);` |
|    13 |  576 | `			}else{` |
|    29 |  577 | `				SyBlobFormat(pOut,"r:%u;",nSeen);` |
|     - |  578 | `			}` |
|    53 |  579 | `			return PH7_OK;` |
|     - |  580 | `		}` |
|   332 |  581 | `	}` |
|  6233 |  582 | `	if( ph7_value_is_null(pIn) ){` |
|    86 |  583 | `		SyBlobAppend(pOut,"N;",2);` |
|  6191 |  584 | `	}else if( ph7_value_is_bool(pIn) ){` |
|    78 |  585 | `		SyBlobAppend(pOut, ph7_value_to_bool(pIn) ? "b:1;" : "b:0;", 4);` |
|  6111 |  586 | `	}else if( ph7_value_is_float(pIn) ){` |
|     - |  587 | `		/* Check float (MEMOBJ_REAL) before int: ph7_value_is_int is lenient and` |
|     - |  588 | `		 * also reports true for an integer-valued real (which caches its int). */` |
|    82 |  589 | `		VmSerializeReal(pOut,ph7_value_to_double(pIn));` |
|  6033 |  590 | `	}else if( ph7_value_is_int(pIn) ){` |
|   911 |  591 | `		SyBlobFormat(pOut,"i:%qd;",ph7_value_to_int64(pIn));` |
|  5540 |  592 | `	}else if( ph7_value_is_string(pIn) ){` |
|     - |  593 | `		int nByte;` |
|  4130 |  594 | `		const char *z = ph7_value_to_string(pIn,&nByte);` |
|  4130 |  595 | `		VmSerializeRawString(pOut,z,nByte);` |
|  3024 |  596 | `	}else if( ph7_value_is_array(pIn) ){` |
|   322 |  597 | `		SyBlobFormat(pOut,"a:%u:{",ph7_array_count(pIn));` |
|   322 |  598 | `		pData->depth++;` |
|   322 |  599 | `		VmSerializeMapEntries((ph7_hashmap *)pIn->x.pOther,pData);` |
|   322 |  600 | `		pData->depth--;` |
|   322 |  601 | `		SyBlobAppend(pOut,"}",1);` |
|   802 |  602 | `	}else if( ph7_value_is_object(pIn) ){` |
|   643 |  603 | `		return VmSerializeObject(pIn,pData);` |
|   ! 0 |  604 | `	}else{` |
|     - |  605 | `		/* resource or unknown -> PHP emits i:0; for resources */` |
|   ! 0 |  606 | `		SyBlobAppend(pOut,"i:0;",4);` |
|     - |  607 | `	}` |
|  5595 |  608 | `	return PH7_OK;` |
|  3145 |  609 | `}` |
|     - |  610 | `/*` |
|     - |  611 | ` * The serializer, for an extension that stores a php VALUE in a file of its` |
|     - |  612 | ` * own: a phar's archive-level and per-entry metadata are php-serialized inside` |
|     - |  613 | `` * its manifest, and `getMetadata()` reads them back with the unserializer`` |
|     - |  614 | ` * below. Answers -1 when the value could not be serialized (a resource, a depth` |
|     - |  615 | ` * php refuses), which is what leaves the metadata unset.` |
|     - |  616 | ` */` |
|     4 |  617 | `PH7_PRIVATE int PH7_VmSerializeValue(ph7_context *pCtx,ph7_value *pIn,SyBlob *pOut)` |
|   ! 0 |  618 | `{` |
|     - |  619 | `	serialize_data sData;` |
|     4 |  620 | `	sData.pVm = pCtx->pVm;` |
|     4 |  621 | `	sData.pCtx = pCtx;` |
|     4 |  622 | `	sData.pOut = pOut;` |
|     4 |  623 | `	sData.depth = 0;` |
|     4 |  624 | `	sData.exc = 0;` |
|     4 |  625 | `	sData.err = 0;` |
|     4 |  626 | `	sData.n = 0;` |
|     4 |  627 | `	VmSerRefTabInit(&sData.sObj,&pCtx->pVm->sAllocator);` |
|     4 |  628 | `	VmSerRefTabInit(&sData.sRef,&pCtx->pVm->sAllocator);` |
|     4 |  629 | `	VmSerialize(pIn,&sData,0,SXU32_HIGH);` |
|     4 |  630 | `	VmSerRefTabRelease(&sData.sObj);` |
|     4 |  631 | `	VmSerRefTabRelease(&sData.sRef);` |
|     4 |  632 | `	return (sData.exc \|\| sData.err) ? -1 : 0;` |
|   ! 0 |  633 | `}` |
|     - |  634 | `/*` |
|     - |  635 | ` * string serialize(mixed $value)` |
|     - |  636 | ` *  Returns a storable representation of a value.` |
|     - |  637 | ` */` |
|   888 |  638 | `PH7_PRIVATE int vm_builtin_serialize(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     5 |  639 | `{` |
|     - |  640 | `	serialize_data sData;` |
|     - |  641 | `	SyBlob sOut;` |
|   893 |  642 | `	if( nArg < 1 ){` |
|   ! 0 |  643 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  644 | `		return PH7_OK;` |
|     - |  645 | `	}` |
|   893 |  646 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|   893 |  647 | `	sData.pVm = pCtx->pVm;` |
|   893 |  648 | `	sData.pCtx = pCtx;` |
|   893 |  649 | `	sData.pOut = &sOut;` |
|   893 |  650 | `	sData.depth = 0;` |
|   893 |  651 | `	sData.exc = 0;` |
|   893 |  652 | `	sData.err = 0;` |
|   893 |  653 | `	sData.n = 0;` |
|   893 |  654 | `	VmSerRefTabInit(&sData.sObj,&pCtx->pVm->sAllocator);` |
|   893 |  655 | `	VmSerRefTabInit(&sData.sRef,&pCtx->pVm->sAllocator);` |
|   893 |  656 | `	VmSerialize(apArg[0],&sData,0,SXU32_HIGH);` |
|   893 |  657 | `	VmSerRefTabRelease(&sData.sObj);` |
|   893 |  658 | `	VmSerRefTabRelease(&sData.sRef);` |
|   893 |  659 | `	if( sData.exc ){` |
|   169 |  660 | `		SyBlobRelease(&sOut);` |
|   169 |  661 | `		return PH7_EXCEPTION;` |
|     - |  662 | `	}` |
|   729 |  663 | `	if( sData.err ){` |
|   ! 0 |  664 | `		SyBlobRelease(&sOut);` |
|   ! 0 |  665 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  666 | `		return PH7_OK;` |
|     - |  667 | `	}` |
|   729 |  668 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|   729 |  669 | `	SyBlobRelease(&sOut);` |
|   729 |  670 | `	return PH7_OK;` |
|   449 |  671 | `}` |
|     - |  672 |  |
|     - |  673 | `/* ----------------------------------------------------------------------------` |
|     - |  674 | ` * Unserializer` |
|     - |  675 | ` * ------------------------------------------------------------------------- */` |
|     - |  676 | `typedef struct unserialize_data unserialize_data;` |
|     - |  677 | `struct unserialize_data` |
|     - |  678 | `{` |
|     - |  679 | `	ph7_vm *pVm;` |
|     - |  680 | `	ph7_context *pCtx;` |
|     - |  681 | `	const char *zCur; /* Current parse position */` |
|     - |  682 | `	const char *zEnd; /* End of the input buffer */` |
|     - |  683 | `	int depth;        /* Current nesting level */` |
|     - |  684 | `	int maxDepth;     /* php's max_depth option (default unserialize_max_depth) */` |
|     - |  685 | `	int depthErr;     /* max_depth was exceeded -> report php's extra warning */` |
|     - |  686 | `	const char *zErr; /* Start of the token that failed (php's reported offset) */` |
|     - |  687 | `	int shortErr;     /* A container's declared count outran its contents */` |
|     - |  688 | `	int exc;          /* A __wakeup()/__unserialize() threw -> propagate it */` |
|     - |  689 | `	int allowAll;     /* allowed_classes: TRUE unless the option said false or a list */` |
|     - |  690 | `	ph7_value *pAllowedList; /* ... the list, when one was given (else NULL) */` |
|     - |  691 | `	SySet aRef;       /* VmUnRef, one per value read: php's reading var_hash */` |
|     - |  692 | `};` |
|     - |  693 | `/*` |
|     - |  694 | ` * The reading half of the back-reference graph: what the payload's Nth value` |
|     - |  695 | `` * turned out to be. `r:` wants the OBJECT, which exists as soon as the O: header`` |
|     - |  696 | `` * has been read, so pVal is filled before the body is; `R:` wants the value SLOT`` |
|     - |  697 | ` * the two holders are to share, which is the array node or the property the` |
|     - |  698 | ` * container reserved for it -- reserved BEFORE the value is parsed, because a` |
|     - |  699 | ` * cyclic payload names that number from inside the value it is about to read.` |
|     - |  700 | ` * A number with no slot (the outermost value; nothing owns it yet) can be named` |
|     - |  701 | `` * by `r:` and not by `R:`.`` |
|     - |  702 | ` */` |
|     - |  703 | `typedef struct VmUnRef VmUnRef;` |
|     - |  704 | `struct VmUnRef` |
|     - |  705 | `{` |
|     - |  706 | `	sxu32 nSlot;         /* aMemObj slot this value lives in, or SXU32_HIGH */` |
|     - |  707 | `	ph7_value *pVal;     /* ... and the value itself, once it is known */` |
|     - |  708 | `	VmClassAttr *pAttr;  /* ... and the PROPERTY it is, when it is one */` |
|     - |  709 | `};` |
|     - |  710 | `static ph7_value * VmUnserializeValue(unserialize_data *ud,sxu32 nMe);` |
|     - |  711 | ``/* Take the next number. php pushes nothing for an `R:` token, exactly as the`` |
|     - |  712 | ` * writer's counter steps back when it emits one, so the two stay in step. */` |
|  2820 |  713 | `static sxu32 VmUnRefReserve(unserialize_data *ud)` |
|     5 |  714 | `{` |
|     - |  715 | `	VmUnRef sRec;` |
|  2825 |  716 | `	sRec.nSlot = SXU32_HIGH;` |
|  2825 |  717 | `	sRec.pVal = 0;` |
|  2825 |  718 | `	sRec.pAttr = 0;` |
|  2825 |  719 | `	if( SySetPut(&ud->aRef,(const void *)&sRec) != SXRET_OK ){` |
|   ! 0 |  720 | `		return SXU32_HIGH;` |
|     - |  721 | `	}` |
|  2825 |  722 | `	return SySetUsed(&ud->aRef);   /* php's numbers are 1-based */` |
|  1415 |  723 | `}` |
|  2742 |  724 | `static void VmUnRefBind(unserialize_data *ud,sxu32 nMe,sxu32 nSlot,ph7_value *pVal,` |
|     - |  725 | `	VmClassAttr *pAttr)` |
|     5 |  726 | `{` |
|     - |  727 | `	VmUnRef *pRec;` |
|  2747 |  728 | `	if( nMe == SXU32_HIGH \|\| nMe < 1 \|\| nMe > SySetUsed(&ud->aRef) ){` |
|   ! 0 |  729 | `		return;` |
|     - |  730 | `	}` |
|  2747 |  731 | `	pRec = (VmUnRef *)SySetAt(&ud->aRef,nMe-1);` |
|  2747 |  732 | `	if( pRec == 0 ){ return; }` |
|  2747 |  733 | `	if( nSlot != SXU32_HIGH ){ pRec->nSlot = nSlot; }` |
|  2747 |  734 | `	if( pVal ){ pRec->pVal = pVal; }` |
|  2747 |  735 | `	if( pAttr ){ pRec->pAttr = pAttr; }` |
|  1376 |  736 | `}` |
|    38 |  737 | `static VmUnRef * VmUnRefAt(unserialize_data *ud,sxu32 nId)` |
|     2 |  738 | `{` |
|    40 |  739 | `	if( nId < 1 \|\| nId > SySetUsed(&ud->aRef) ){` |
|     9 |  740 | `		return 0;` |
|     - |  741 | `	}` |
|    32 |  742 | `	return (VmUnRef *)SySetAt(&ud->aRef,nId-1);` |
|    21 |  743 | `}` |
|     - |  744 | `/* Consume the single expected character; 0 on mismatch/EOF. */` |
| 23696 |  745 | `static int VmUnExpect(unserialize_data *ud, char c)` |
|     5 |  746 | `{` |
| 23701 |  747 | `	if( ud->zCur < ud->zEnd && ud->zCur[0] == c ){ ud->zCur++; return 1; }` |
|    35 |  748 | `	return 0;` |
| 11853 |  749 | `}` |
|     - |  750 | `/* Parse an unsigned decimal into *pOut; 0 on no-digit/overflow. */` |
|  3066 |  751 | `static int VmUnParseUInt(unserialize_data *ud, sxu32 *pOut)` |
|     5 |  752 | `{` |
|  3071 |  753 | `	sxu32 v = 0;` |
|  3071 |  754 | `	int n = 0;` |
|  6523 |  755 | `	while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){` |
|  3457 |  756 | `		sxu32 d = (sxu32)(ud->zCur[0] - '0');` |
|  3457 |  757 | `		if( v > (0xFFFFFFFFU - d)/10 ){ return 0; } /* overflow */` |
|  3457 |  758 | `		v = v*10 + d;` |
|  3457 |  759 | `		ud->zCur++; n++;` |
|     5 |  760 | `	}` |
|  3071 |  761 | `	if( n == 0 ){ return 0; }` |
|  3063 |  762 | `	*pOut = v;` |
|  3063 |  763 | `	return 1;` |
|  1538 |  764 | `}` |
|     - |  765 | `/* Parse a signed 64-bit decimal into *pOut; 0 on failure. A digit run PAST the` |
|     - |  766 | ` * int64 range saturates to PHP_INT_MAX/MIN and sets *pOverflow, which is what php` |
|     - |  767 | ` * does (strtol clamping, then its own warning) -- the magnitude used to be` |
|     - |  768 | `` * accumulated with unchecked wraparound, so `i:99999999999999999999;` came back`` |
|     - |  769 | ` * as some unrelated number.` |
|     - |  770 | ` *` |
|     - |  771 | ` * The reader stays local rather than calling SyStrToInt64Ex: that one tolerates` |
|     - |  772 | `` * surrounding whitespace, and the serialization grammar does not (`i: 1;` is a`` |
|     - |  773 | ` * parse failure at offset 0, on both engines). */` |
|  2198 |  774 | `static int VmUnParseInt64(unserialize_data *ud, ph7_int64 *pOut, int *pOverflow)` |
|     5 |  775 | `{` |
|  2203 |  776 | `	int neg = 0, n = 0, ovf = 0;` |
|  2203 |  777 | `	sxu64 v = 0, cutoff;` |
|  2203 |  778 | `	if( ud->zCur < ud->zEnd && (ud->zCur[0]=='-' \|\| ud->zCur[0]=='+') ){` |
|    15 |  779 | `		neg = (ud->zCur[0]=='-'); ud->zCur++;` |
|     7 |  780 | `	}` |
|     - |  781 | `	/* Largest magnitude that fits: PHP_INT_MAX going up, \|PHP_INT_MIN\| going down. */` |
|  2203 |  782 | `	cutoff = neg ? ((sxu64)LARGEST_INT64 + 1) : (sxu64)LARGEST_INT64;` |
|  7237 |  783 | `	while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){` |
|  5039 |  784 | `		sxu64 d = (sxu64)(ud->zCur[0]-'0');` |
|  5039 |  785 | `		if( v > cutoff/10 \|\| (v == cutoff/10 && d > cutoff%10) ){` |
|    37 |  786 | `			ovf = 1;` |
|    19 |  787 | `		}else{` |
|  5003 |  788 | `			v = v*10 + d;` |
|     - |  789 | `		}` |
|  5039 |  790 | `		ud->zCur++; n++;` |
|     5 |  791 | `	}` |
|  2203 |  792 | `	if( n == 0 ){ return 0; }` |
|  2201 |  793 | `	if( ovf ){` |
|    21 |  794 | `		v = cutoff;` |
|    10 |  795 | `	}` |
|  2201 |  796 | `	if( pOverflow ){` |
|  2201 |  797 | `		*pOverflow = ovf;` |
|  1098 |  798 | `	}` |
|     - |  799 | `	/* The negative cap \|PHP_INT_MIN\| has no positive ph7_int64 form, so materialize` |
|     - |  800 | `	 * PHP_INT_MIN directly instead of negating it. */` |
|  1109 |  801 | `	*pOut = neg ? ( v > (sxu64)LARGEST_INT64 ? SMALLEST_INT64 : -(ph7_int64)v )` |
|  2196 |  802 | `	            : (ph7_int64)v;` |
|  2201 |  803 | `	return 1;` |
|  1104 |  804 | `}` |
|     - |  805 | `/* Parse s:<len>:"<len bytes>"; returning the raw view (zStr,nStr). */` |
|  2082 |  806 | `static int VmUnParseString(unserialize_data *ud, const char **pzStr, int *pnStr)` |
|     4 |  807 | `{` |
|     - |  808 | `	const char *zLen;` |
|     - |  809 | `	sxu32 nLen;` |
|  2086 |  810 | `	if( !VmUnExpect(ud,'s') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|  2086 |  811 | `	zLen = ud->zCur;` |
|  2086 |  812 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|  2086 |  813 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|     - |  814 | `	/* Once the DECLARED length has been read, php stops blaming the token as a` |
|     - |  815 | `	 * whole and reports where the declaration turned out to be wrong: the length` |
|     - |  816 | `	 * digits when they overrun the buffer, the byte where the closing quote should` |
|     - |  817 | `	 * have been when they simply disagree with the payload. Length compare (not` |
|     - |  818 | `	 * pointer arithmetic) so a 32-bit pointer cannot wrap. */` |
|  2086 |  819 | `	if( nLen > (sxu32)(ud->zEnd - ud->zCur) ){` |
|     5 |  820 | `		if( ud->zErr == 0 ){ ud->zErr = zLen; }` |
|     5 |  821 | `		return 0;` |
|     - |  822 | `	}` |
|  2082 |  823 | `	*pzStr = ud->zCur;` |
|  2082 |  824 | `	*pnStr = (int)nLen;` |
|  2082 |  825 | `	ud->zCur += nLen;` |
|  2082 |  826 | `	if( !VmUnExpect(ud,'"') \|\| !VmUnExpect(ud,';') ){` |
|     5 |  827 | `		if( ud->zErr == 0 ){ ud->zErr = ud->zCur; }` |
|     5 |  828 | `		return 0;` |
|     - |  829 | `	}` |
|  2078 |  830 | `	return 1;` |
|  1045 |  831 | `}` |
|     - |  832 | `/* Strip object-property key mangling: "\0*\0name" / "\0Class\0name" -> name. */` |
|    60 |  833 | `static void VmUnstripKey(const char *z, int n, const char **pzName, int *pnName)` |
|     2 |  834 | `{` |
|    62 |  835 | `	if( n >= 1 && z[0] == '\0' ){` |
|     - |  836 | `		int i;` |
|   105 |  837 | `		for( i = 1; i < n; i++ ){` |
|   105 |  838 | `			if( z[i] == '\0' ){ *pzName = z+i+1; *pnName = n-i-1; return; }` |
|    43 |  839 | `		}` |
|   ! 0 |  840 | `	}` |
|    42 |  841 | `	*pzName = z; *pnName = n;` |
|    32 |  842 | `}` |
|     - |  843 | `/*` |
|     - |  844 | ` * A container declared N members but its closing brace arrives early. php words` |
|     - |  845 | ` * that one shape "Unexpected end of serialized data" (a genuinely TRUNCATED` |
|     - |  846 | `` * payload -- `a:1:{i:0;` -- gets only the offset warning), and reports the offset`` |
|     - |  847 | ` * of the brace, so pin it here rather than letting the enclosing value latch its` |
|     - |  848 | ` * own start.` |
|     - |  849 | ` */` |
|  2236 |  850 | `static int VmUnserializeShortContainer(unserialize_data *ud)` |
|     4 |  851 | `{` |
|  2240 |  852 | `	if( ud->zCur >= ud->zEnd \|\| ud->zCur[0] != '}' ){` |
|  2234 |  853 | `		return 0;` |
|     - |  854 | `	}` |
|     7 |  855 | `	ud->shortErr = 1;` |
|     7 |  856 | `	if( ud->zErr == 0 ){` |
|     7 |  857 | `		ud->zErr = ud->zCur;` |
|     3 |  858 | `	}` |
|     7 |  859 | `	return 1;` |
|  1122 |  860 | `}` |
|     - |  861 | ``/* Is the next token an `R:` back-reference? Only a container can honour one --`` |
|     - |  862 | ` * it is a bind, not a value -- so every container peeks for it itself. */` |
|  2214 |  863 | `static int VmUnPeekBackRef(unserialize_data *ud)` |
|     4 |  864 | `{` |
|  2218 |  865 | `	return ud->zCur+1 < ud->zEnd && ud->zCur[0] == 'R' && ud->zCur[1] == ':';` |
|     4 |  866 | `}` |
|     - |  867 | `/*` |
|     - |  868 | `` * `r:<digits>;` / `R:<digits>;`, and where php blames a bad one.`` |
|     - |  869 | ` *` |
|     - |  870 | ` * php sets its cursor past the whole token BEFORE it looks at the number, so a` |
|     - |  871 | ` * well-formed token naming a number it cannot use reports where the parser` |
|     - |  872 | `` * stopped -- the byte after the `;` -- and only a token whose SHAPE did not`` |
|     - |  873 | ` * match is blamed at its own start. A digit run past the range saturates` |
|     - |  874 | ` * instead of failing the shape (php's own reader clamps), so` |
|     - |  875 | `` * `R:99999999999999999999;` is a number out of range rather than a syntax error,`` |
|     - |  876 | `` * and a SIGNED one (`r:-1;`) never matches the shape at all.`` |
|     - |  877 | ` */` |
|    48 |  878 | `static int VmUnParseRefToken(unserialize_data *ud,char c,sxu32 *pnId)` |
|     2 |  879 | `{` |
|    50 |  880 | `	const char *zStart = ud->zCur;` |
|    50 |  881 | `	sxu32 nId = 0;` |
|    50 |  882 | `	int nDigit = 0;` |
|    50 |  883 | `	if( VmUnExpect(ud,c) && VmUnExpect(ud,':') ){` |
|   130 |  884 | `		while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){` |
|    82 |  885 | `			nId = nId > (SXU32_HIGH - 9)/10 ? SXU32_HIGH : nId*10 + (sxu32)(ud->zCur[0]-'0');` |
|    82 |  886 | `			ud->zCur++;` |
|    82 |  887 | `			nDigit++;` |
|     2 |  888 | `		}` |
|    50 |  889 | `		if( nDigit > 0 && VmUnExpect(ud,';') ){` |
|    42 |  890 | `			*pnId = nId;` |
|    42 |  891 | `			return 1;` |
|     - |  892 | `		}` |
|     4 |  893 | `	}` |
|     - |  894 | ``	/* A container parses its own `R:` with no VmUnserializeValue wrapper around`` |
|     - |  895 | `	 * it, so the token's own start is latched here rather than by the caller. */` |
|     9 |  896 | `	if( ud->zErr == 0 ){ ud->zErr = zStart; }` |
|     9 |  897 | `	return 0;` |
|    26 |  898 | `}` |
|     - |  899 | `/*` |
|     - |  900 | `` * Parse `R:<n>;` and answer the value slot it names. Fails on a number that was`` |
|     - |  901 | ` * never pushed, on one that has no slot of its own (the outermost value), and on` |
|     - |  902 | ` * one that names nSelf -- the slot the bind is landing in, which php refuses as` |
|     - |  903 | `` * `rval_ref == rval`. A DUPLICATE KEY is how a payload asks for that:`` |
|     - |  904 | `` * `a:2:{s:1:"k";i:1;s:1:"k";R:2;}` would rebind the element to itself. Naming a`` |
|     - |  905 | ` * different slot from a duplicate key is fine and stays so.` |
|     - |  906 | ` */` |
|    30 |  907 | `static int VmUnParseBackRef(unserialize_data *ud,sxu32 nSelf,sxu32 *pnSlot)` |
|     2 |  908 | `{` |
|     - |  909 | `	VmUnRef *pRec;` |
|     - |  910 | `	sxu32 nId;` |
|    32 |  911 | `	if( !VmUnParseRefToken(ud,'R',&nId) ){` |
|     7 |  912 | `		return 0;   /* the shape is wrong: the caller blames the token's start */` |
|     - |  913 | `	}` |
|    26 |  914 | `	pRec = VmUnRefAt(ud,nId);` |
|    26 |  915 | `	if( pRec == 0 \|\| pRec->nSlot == SXU32_HIGH \|\| pRec->nSlot == nSelf ){` |
|    18 |  916 | `		if( ud->zErr == 0 ){ ud->zErr = ud->zCur; }` |
|    18 |  917 | `		return 0;` |
|     - |  918 | `	}` |
|     8 |  919 | `	if( pRec->pAttr` |
|     6 |  920 | `	 && (pRec->pAttr->iState & (VM_CLASS_ATTR_REFBOUND\|VM_CLASS_ATTR_REFSRCPIN)) == 0 ){` |
|     - |  921 | `		/* The SOURCE end of a bind is a reference too, and the table cannot name a` |
|     - |  922 | `		 * property as a holder — so a property being referenced pins its own slot,` |
|     - |  923 | ``		 * exactly as fetching `&$o->p` does. Without it the slot's only recorded`` |
|     - |  924 | `		 * holder is the other end's pin, and neither end reads back as a reference. */` |
|     3 |  925 | `		pRec->pAttr->iState \|= VM_CLASS_ATTR_REFSRCPIN;` |
|     3 |  926 | `		VmPinMemObjSlotCounted(ud->pVm,pRec->nSlot);` |
|     1 |  927 | `	}` |
|     9 |  928 | `	*pnSlot = pRec->nSlot;` |
|     9 |  929 | `	return 1;` |
|    17 |  930 | `}` |
|     - |  931 | `/*` |
|     - |  932 | `` * One `<key><value>` pair into a map.`` |
|     - |  933 | ` *` |
|     - |  934 | ` * The element is created BEFORE its value is read, because the value's own` |
|     - |  935 | ` * number names the element's slot and a cyclic payload asks for that number from` |
|     - |  936 | `` * inside the value it is about to read: `[1, 2, &$self]` comes back as an array`` |
|     - |  937 | `` * whose third element is `R:` at the number the third element itself took.`` |
|     - |  938 | `` * An `R:` value is a bind rather than a store, so it goes in as a reference and`` |
|     - |  939 | ` * takes no number at all.` |
|     - |  940 | ` */` |
|  2036 |  941 | `static int VmUnserializeMapEntry(unserialize_data *ud,ph7_value *pArray)` |
|     4 |  942 | `{` |
|  2040 |  943 | `	ph7_hashmap *pMap = (ph7_hashmap *)pArray->x.pOther;` |
|  2040 |  944 | `	ph7_hashmap_node *pNode = 0;` |
|     - |  945 | `	ph7_value *pKey, *pVal, *pSlot;` |
|     - |  946 | `	ph7_value sNull;` |
|     - |  947 | `	sxu32 nMe, nSlot;` |
|  2040 |  948 | `	pKey = VmUnserializeValue(ud,SXU32_HIGH);  /* a key takes no number */` |
|  2040 |  949 | `	if( pKey == 0 ){ return 0; }` |
|  2034 |  950 | `	if( VmUnPeekBackRef(ud) ){` |
|     - |  951 | `		/* An element already under this key (a duplicate) is the slot the bind` |
|     - |  952 | `		 * would land in, and php refuses one naming itself. */` |
|    28 |  953 | `		ph7_hashmap_node *pOld = 0;` |
|    28 |  954 | `		sxu32 nTarget, nSelf = SXU32_HIGH;` |
|    28 |  955 | `		if( PH7_HashmapLookup(pMap,pKey,&pOld) == SXRET_OK && pOld ){` |
|     5 |  956 | `			nSelf = pOld->nValIdx;` |
|     2 |  957 | `		}` |
|    28 |  958 | `		if( !VmUnParseBackRef(ud,nSelf,&nTarget) ){ return 0; }` |
|     7 |  959 | `		return PH7_HashmapInsertByRef(pMap,pKey,nTarget) == SXRET_OK;` |
|     - |  960 | `	}` |
|  2008 |  961 | `	nMe = VmUnRefReserve(ud);` |
|  2008 |  962 | `	PH7_MemObjInit(ud->pVm,&sNull);` |
|  2008 |  963 | `	if( ph7_array_add_elem(pArray,pKey,&sNull) != PH7_OK ){` |
|   ! 0 |  964 | `		PH7_MemObjRelease(&sNull);` |
|   ! 0 |  965 | `		return 0;` |
|     - |  966 | `	}` |
|  2008 |  967 | `	PH7_MemObjRelease(&sNull);` |
|  2008 |  968 | `	if( PH7_HashmapLookup(pMap,pKey,&pNode) != SXRET_OK \|\| pNode == 0 ){` |
|   ! 0 |  969 | `		return 0;` |
|     - |  970 | `	}` |
|  2008 |  971 | `	nSlot = pNode->nValIdx;` |
|  2008 |  972 | `	VmUnRefBind(ud,nMe,nSlot,(ph7_value *)PH7_MemObjAt(&ud->pVm->aMemObj,nSlot),0);` |
|  2008 |  973 | `	pVal = VmUnserializeValue(ud,nMe);` |
|  2008 |  974 | `	if( pVal == 0 ){ return 0; }` |
|     - |  975 | `	/* Re-read the slot rather than keeping the node: the value just parsed may` |
|     - |  976 | `	 * have bound a reference into this very map. */` |
|  1985 |  977 | `	pSlot = (ph7_value *)PH7_MemObjAt(&ud->pVm->aMemObj,nSlot);` |
|  1985 |  978 | `	if( pSlot ){ PH7_MemObjStore(pVal,pSlot); }` |
|     - |  979 | `	/* The pKey/pVal temporaries are intentionally NOT released per node:` |
|     - |  980 | `	 * ph7_context_release_value() linear-scans the context value set, which` |
|     - |  981 | `	 * would make a large unserialize O(N^2). They are reclaimed in bulk when` |
|     - |  982 | `	 * the call context is torn down. */` |
|  1985 |  983 | `	return 1;` |
|  1022 |  984 | `}` |
|     - |  985 | `/* Parse a:<count>:{ <key><val> ... } into a fresh array value. */` |
|   234 |  986 | `static ph7_value * VmUnserializeArray(unserialize_data *ud,sxu32 nMe)` |
|     4 |  987 | `{` |
|     - |  988 | `	sxu32 count, i;` |
|     - |  989 | `	ph7_value *pArray;` |
|   238 |  990 | `	if( !VmUnExpect(ud,'a') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   238 |  991 | `	if( !VmUnParseUInt(ud,&count) ){ return 0; }` |
|   238 |  992 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'{') ){ return 0; }` |
|   238 |  993 | `	pArray = ph7_context_new_array(ud->pCtx);` |
|   238 |  994 | `	if( pArray == 0 ){ return 0; }` |
|   238 |  995 | `	VmUnRefBind(ud,nMe,SXU32_HIGH,pArray,0);` |
|   238 |  996 | `	ud->depth++;` |
|  1744 |  997 | `	for( i = 0; i < count; i++ ){` |
|  1562 |  998 | `		if( VmUnserializeShortContainer(ud) ){ ud->depth--; return 0; }` |
|  1558 |  999 | `		if( !VmUnserializeMapEntry(ud,pArray) ){ ud->depth--; return 0; }` |
|   756 | 1000 | `	}` |
|   185 | 1001 | `	ud->depth--;` |
|   185 | 1002 | `	if( !VmUnExpect(ud,'}') ){ return 0; }` |
|   185 | 1003 | `	return pArray;` |
|   121 | 1004 | `}` |
|     - | 1005 | `/*` |
|     - | 1006 | ` * Is the class this payload names allowed to instantiate? php's rule: no option` |
|     - | 1007 | `` * or `true` allows everything; `false` allows nothing; a LIST is matched`` |
|     - | 1008 | ` * case-insensitively (php lowercases both sides; the fold is ASCII, like every` |
|     - | 1009 | ` * other name fold here). A non-string list member was already refused by the` |
|     - | 1010 | ` * option screen.` |
|     - | 1011 | ` */` |
|     - | 1012 | `typedef struct allowed_walk_ctx allowed_walk_ctx;` |
|     - | 1013 | `struct allowed_walk_ctx` |
|     - | 1014 | `{` |
|     - | 1015 | `	const char *zClass;` |
|     - | 1016 | `	sxu32 nClass;` |
|     - | 1017 | `	int bFound;` |
|     - | 1018 | `};` |
|    10 | 1019 | `static int VmUnserializeAllowedWalker(ph7_value *pKey, ph7_value *pData, void *pUserData)` |
|     1 | 1020 | `{` |
|    11 | 1021 | `	allowed_walk_ctx *pWalk = (allowed_walk_ctx *)pUserData;` |
|     - | 1022 | `	int nEntry;` |
|    11 | 1023 | `	const char *zEntry = ph7_value_to_string(pData,&nEntry);` |
|     5 | 1024 | `	SXUNUSED(pKey);` |
|    10 | 1025 | `	if( (sxu32)nEntry == pWalk->nClass` |
|    10 | 1026 | `	 && SyStrnicmp(zEntry,pWalk->zClass,pWalk->nClass) == 0 ){` |
|     7 | 1027 | `		pWalk->bFound = 1;` |
|     7 | 1028 | `		return SXERR_ABORT; /* found: stop walking */` |
|     - | 1029 | `	}` |
|     5 | 1030 | `	return PH7_OK;` |
|     6 | 1031 | `}` |
|   350 | 1032 | `static int VmUnserializeClassAllowed(unserialize_data *ud, const char *zClass, sxu32 nClass)` |
|     5 | 1033 | `{` |
|     - | 1034 | `	allowed_walk_ctx sWalk;` |
|   355 | 1035 | `	if( ud->pAllowedList == 0 ){` |
|   343 | 1036 | `		return ud->allowAll;` |
|     - | 1037 | `	}` |
|    13 | 1038 | `	sWalk.zClass = zClass;` |
|    13 | 1039 | `	sWalk.nClass = nClass;` |
|    13 | 1040 | `	sWalk.bFound = 0;` |
|    13 | 1041 | `	ph7_array_walk(ud->pAllowedList,VmUnserializeAllowedWalker,&sWalk);` |
|    13 | 1042 | `	return sWalk.bFound;` |
|   180 | 1043 | `}` |
|     - | 1044 | `/*` |
|     - | 1045 | ` * The instance slot for a property the payload names, creating it as a DYNAMIC` |
|     - | 1046 | ` * one when it is not there yet. A duplicate key overwrites, like any hash store.` |
|     - | 1047 | ` *` |
|     - | 1048 | ``  * An EMPTY name is php's own (`s:0:""` gives a property `''` that `$o->{''}` `` |
|     - | 1049 | ` * reads), and PH7_ClassInstanceAttrEntry is what finds one — SyHashGet refuses a` |
|     - | 1050 | `` * zero-length key engine-wide — so a payload repeating `s:0:""` overwrites`` |
|     - | 1051 | ` * instead of growing one ghost entry per occurrence. Everything that walks hAttr` |
|     - | 1052 | `` * sees such a property normally; only a direct `$o->{''}` cannot, the same`` |
|     - | 1053 | `` * engine-wide empty-name limit the `${''}` lvalue residual records.`` |
|     - | 1054 | ` */` |
|    44 | 1055 | `static ph7_value * VmUnserializePropSlot(unserialize_data *ud,ph7_class_instance *pThis,` |
|     - | 1056 | `	const char *zKey,sxu32 nKey)` |
|     1 | 1057 | `{` |
|    45 | 1058 | `	SyHashEntry *pEntry = PH7_ClassInstanceAttrEntry(pThis,zKey,nKey);` |
|    45 | 1059 | `	if( pEntry ){` |
|   ! 0 | 1060 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|   ! 0 | 1061 | `		return pVmAttr ? (ph7_value *)PH7_MemObjAt(&ud->pVm->aMemObj,pVmAttr->nIdx) : 0;` |
|     - | 1062 | `	}` |
|    45 | 1063 | `	return PH7_VmCreateDynamicAttr(ud->pVm,pThis,zKey,nKey,0);` |
|    23 | 1064 | `}` |
|     - | 1065 | `/*` |
|     - | 1066 | ` * Materialize one parsed property on the __PHP_Incomplete_Class carrier: the key` |
|     - | 1067 | ` * is stored RAW (mangling bytes and all — that is what php keeps, and what lets` |
|     - | 1068 | ` * re-serialization emit the original payload byte for byte).` |
|     - | 1069 | ` */` |
|    44 | 1070 | `static void VmUnserializeIncompleteProp(unserialize_data *ud,ph7_class_instance *pThis,` |
|     - | 1071 | `	const char *zKey,sxu32 nKey,ph7_value *pVal)` |
|     1 | 1072 | `{` |
|    45 | 1073 | `	ph7_value *pSlot = VmUnserializePropSlot(ud,pThis,zKey,nKey);` |
|    45 | 1074 | `	if( pSlot && pVal ){` |
|    45 | 1075 | `		PH7_MemObjStore(pVal,pSlot);` |
|    22 | 1076 | `	}` |
|    45 | 1077 | `}` |
|     - | 1078 | `/* The property a payload key names on the INCOMPLETE carrier, whose keys stay` |
|     - | 1079 | ` * RAW (mangling bytes and all) and whose properties are all dynamic. */` |
|    54 | 1080 | `static VmClassAttr * VmUnserializePropAttrRaw(unserialize_data *ud,ph7_class_instance *pThis,` |
|     - | 1081 | `	const char *zKey,sxu32 nKey)` |
|     1 | 1082 | `{` |
|    55 | 1083 | `	SyHashEntry *pEntry = PH7_ClassInstanceAttrEntry(pThis,zKey,nKey);` |
|    55 | 1084 | `	VmClassAttr *pVmAttr = 0;` |
|    55 | 1085 | `	if( pEntry ){` |
|     3 | 1086 | `		return (VmClassAttr *)pEntry->pUserData;` |
|     - | 1087 | `	}` |
|    53 | 1088 | `	PH7_VmCreateDynamicAttr(ud->pVm,pThis,zKey,nKey,&pVmAttr);` |
|    53 | 1089 | `	return pVmAttr;` |
|    28 | 1090 | `}` |
|     - | 1091 | `/*` |
|     - | 1092 | ` * The property a payload key names.` |
|     - | 1093 | ` *` |
|     - | 1094 | ` * The payload key is php's STORAGE name, so it is tried as WRITTEN first — that` |
|     - | 1095 | ` * is how a base class's private lands in its own slot rather than over the` |
|     - | 1096 | ` * same-named property of the object's own class — and demangled only when the` |
|     - | 1097 | `` * object holds nothing under it (a `\0*\0` protected key, and a private of the`` |
|     - | 1098 | ` * object's OWN class, are both stored plain here).` |
|     - | 1099 | ` *` |
|     - | 1100 | ` * A payload property the class does not DECLARE becomes a dynamic one. PHL used` |
|     - | 1101 | ` * to drop it in silence, which lost the whole body of the commonest payload` |
|     - | 1102 | `` * there is: stdClass declares nothing, so `unserialize(serialize($obj))` on a`` |
|     - | 1103 | `` * `(object)['a'=>1]` or a json_decode() result came back EMPTY.`` |
|     - | 1104 | ` *` |
|     - | 1105 | ` * Where php's own rule and PHL's differ, this is the engine's own dynamic-` |
|     - | 1106 | ` * property decision (VmClassAllowsDynamicProps / #[AllowDynamicProperties]), the` |
|     - | 1107 | `` * one the `$o->n = 1` write path makes: created on stdClass and on a class that`` |
|     - | 1108 | `` * opts in, refused with `Cannot create dynamic property C::$n` otherwise. php`` |
|     - | 1109 | ` * DEPRECATES that last case rather than refusing it (the scope policy rejects` |
|     - | 1110 | ` * php's deprecated surface loudly) and raises this exact Error itself for a` |
|     - | 1111 | ` * readonly class. The Error is a real throw, so it abandons the parse the way a` |
|     - | 1112 | ` * throwing __wakeup() does.` |
|     - | 1113 | ` */` |
|   136 | 1114 | `static VmClassAttr * VmUnserializeTargetAttr(unserialize_data *ud,ph7_class_instance *pThis,` |
|     - | 1115 | `	const char *zKey,int nKey)` |
|     3 | 1116 | `{` |
|   139 | 1117 | `	ph7_vm *pVm = ud->pVm;` |
|   139 | 1118 | `	ph7_class *pClass = pThis->pClass;` |
|   139 | 1119 | `	SyHashEntry *pAttrEntry = PH7_ClassInstanceAttrEntry(pThis,zKey,(sxu32)nKey);` |
|   139 | 1120 | `	VmClassAttr *pVmAttr = 0;` |
|   139 | 1121 | `	if( pAttrEntry == 0 ){` |
|     - | 1122 | `		const char *zName; int nName;` |
|    62 | 1123 | `		VmUnstripKey(zKey,nKey,&zName,&nName);` |
|    62 | 1124 | `		pAttrEntry = PH7_ClassInstanceAttrEntry(pThis,zName,(sxu32)nName);` |
|    30 | 1125 | `	}` |
|   139 | 1126 | `	if( pAttrEntry && pAttrEntry->pUserData ){` |
|    97 | 1127 | `		pVmAttr = (VmClassAttr *)pAttrEntry->pUserData;` |
|    97 | 1128 | `		if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) != 0 ){` |
|   ! 0 | 1129 | `			pVmAttr = 0;` |
|   ! 0 | 1130 | `		}` |
|    47 | 1131 | `	}` |
|   139 | 1132 | `	if( pVmAttr ){` |
|    97 | 1133 | `		return pVmAttr;` |
|     - | 1134 | `	}` |
|    44 | 1135 | `	if( (pClass->iFlags & PH7_CLASS_READONLY) != 0 \|\| !VmClassAllowsDynamicProps(pVm,pClass) ){` |
|    10 | 1136 | `		PH7_VmThrowException(ud->pCtx,"Error",` |
|     3 | 1137 | `			"Cannot create dynamic property %z::$%.*s",&pClass->sDisp,nKey,zKey);` |
|     7 | 1138 | `		ud->exc = 1;` |
|     7 | 1139 | `		return 0;` |
|     - | 1140 | `	}` |
|     - | 1141 | `	/* php creates the dynamic property under the key as WRITTEN, mangling bytes` |
|     - | 1142 | `	 * included — only the declared-property lookup demangles. */` |
|    37 | 1143 | `	PH7_VmCreateDynamicAttr(pVm,pThis,zKey,(sxu32)nKey,&pVmAttr);` |
|    37 | 1144 | `	return pVmAttr;` |
|    71 | 1145 | `}` |
|     - | 1146 | `/* Parse O:<namelen>:"<Class>":<count>:{ ... } into a fresh object value. */` |
|   374 | 1147 | `static ph7_value * VmUnserializeObject(unserialize_data *ud,sxu32 nMe)` |
|     5 | 1148 | `{` |
|     - | 1149 | `	sxu32 nLen, count, i;` |
|     - | 1150 | `	const char *zClass;` |
|   379 | 1151 | `	ph7_class *pClass = 0;` |
|     - | 1152 | `	ph7_class_instance *pThis;` |
|     - | 1153 | `	ph7_class_method *pMethod;` |
|   379 | 1154 | `	ph7_value *pObjVal, *pArrVal = 0;` |
|   379 | 1155 | `	int bIncomplete = 0;   /* build the carrier instead of the named class */` |
|   379 | 1156 | `	int bStampName = 0;    /* ... and remember the payload's name on it */` |
|     - | 1157 | `	const char *zLen;` |
|   379 | 1158 | `	if( !VmUnExpect(ud,'O') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|   379 | 1159 | `	zLen = ud->zCur;` |
|   379 | 1160 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|   375 | 1161 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|     - | 1162 | `	/* Past the DECLARED length php stops blaming the token and reports where the` |
|     - | 1163 | `	 * declaration turned out to be wrong — the s: reader's own two rules, which` |
|     - | 1164 | `	 * this header never had, so every one of these was offset 0. A length that` |
|     - | 1165 | `	 * overruns the buffer (or an EMPTY class name, which php refuses outright)` |
|     - | 1166 | `	 * blames the length DIGITS; a length that merely disagrees with the payload` |
|     - | 1167 | `	 * blames the byte where the closing quote should have been. */` |
|   373 | 1168 | `	if( nLen < 1 \|\| nLen > (sxu32)(ud->zEnd - ud->zCur) ){` |
|     7 | 1169 | `		if( ud->zErr == 0 ){ ud->zErr = zLen; }` |
|     7 | 1170 | `		return 0;` |
|     - | 1171 | `	}` |
|   367 | 1172 | `	zClass = ud->zCur; ud->zCur += nLen;` |
|   367 | 1173 | `	if( !VmUnExpect(ud,'"') ){` |
|     3 | 1174 | `		if( ud->zErr == 0 ){ ud->zErr = ud->zCur; }` |
|     3 | 1175 | `		return 0;` |
|     - | 1176 | `	}` |
|     - | 1177 | `	/* From here on the header is well-formed enough that php reports where the` |
|     - | 1178 | `	 * parser actually stopped rather than the token's start. A NEGATIVE count is` |
|     - | 1179 | `	 * read and then rejected, so the blame falls PAST its digits — php's own` |
|     - | 1180 | `	 * signed reader, which is why the sign is skipped here before the report. */` |
|   360 | 1181 | `	if( !VmUnExpect(ud,':') \|\| !VmUnParseUInt(ud,&count)` |
|   361 | 1182 | `	 \|\| !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'{') ){` |
|    11 | 1183 | `		if( ud->zErr == 0 ){` |
|    11 | 1184 | `			if( ud->zCur < ud->zEnd && (ud->zCur[0] == '-' \|\| ud->zCur[0] == '+') ){` |
|     3 | 1185 | `				ud->zCur++;` |
|     5 | 1186 | `				while( ud->zCur < ud->zEnd && ud->zCur[0] >= '0' && ud->zCur[0] <= '9' ){` |
|     3 | 1187 | `					ud->zCur++;` |
|     1 | 1188 | `				}` |
|     1 | 1189 | `			}` |
|    11 | 1190 | `			ud->zErr = ud->zCur;` |
|     5 | 1191 | `		}` |
|    11 | 1192 | `		return 0;` |
|     - | 1193 | `	}` |
|   355 | 1194 | `	if( !VmUnserializeClassAllowed(ud,zClass,nLen) ){` |
|     - | 1195 | `		/* A class the option refuses becomes __PHP_Incomplete_Class WITHOUT a` |
|     - | 1196 | `		 * class lookup: php never autoloads a name it was told not to build. */` |
|    21 | 1197 | `		bIncomplete = bStampName = 1;` |
|    11 | 1198 | `	}else{` |
|   335 | 1199 | `		pClass = PH7_VmExtractClass(ud->pVm,zClass,nLen,TRUE,0);` |
|   335 | 1200 | `		if( pClass == 0 ){` |
|     - | 1201 | `			/* Unknown even after autoload: php gives the unserialize_callback_func` |
|     - | 1202 | `			 * ini one chance to declare it, then falls back to the carrier and` |
|     - | 1203 | `			 * KEEPS PARSING — an unknown class is not a syntax error. */` |
|     - | 1204 | `			SyBlob sCb;` |
|    29 | 1205 | `			SyBlobInit(&sCb,&ud->pVm->sAllocator);` |
|    29 | 1206 | `			PH7_VmIniGetStr(ud->pVm,"unserialize_callback_func",&sCb);` |
|    29 | 1207 | `			if( SyBlobLength(&sCb) > 0 ){` |
|     - | 1208 | `				ph7_value sCbName, sCbArg, sCbRet;` |
|     - | 1209 | `				sxi32 rcCb;` |
|     7 | 1210 | `				PH7_MemObjInit(ud->pVm,&sCbName);` |
|     7 | 1211 | `				PH7_MemObjInit(ud->pVm,&sCbArg);` |
|     7 | 1212 | `				PH7_MemObjInit(ud->pVm,&sCbRet);` |
|     7 | 1213 | `				PH7_MemObjStringAppend(&sCbName,(const char *)SyBlobData(&sCb),SyBlobLength(&sCb));` |
|     7 | 1214 | `				if( !PH7_VmIsCallable(ud->pVm,&sCbName,FALSE) ){` |
|     - | 1215 | `					/* php throws (uncaught unless the caller catches): the ini named` |
|     - | 1216 | `					 * a function that does not exist. */` |
|     4 | 1217 | `					PH7_VmThrowException(ud->pCtx,"Error",` |
|     - | 1218 | `						"Invalid callback %.*s, function \"%.*s\" not found or invalid function name",` |
|     2 | 1219 | `						(int)SyBlobLength(&sCb),(const char *)SyBlobData(&sCb),` |
|     2 | 1220 | `						(int)SyBlobLength(&sCb),(const char *)SyBlobData(&sCb));` |
|     3 | 1221 | `					PH7_MemObjRelease(&sCbName);` |
|     3 | 1222 | `					SyBlobRelease(&sCb);` |
|     3 | 1223 | `					ud->exc = 1;` |
|     3 | 1224 | `					return 0;` |
|     - | 1225 | `				}` |
|     5 | 1226 | `				PH7_MemObjStringAppend(&sCbArg,zClass,nLen);` |
|     - | 1227 | `				{` |
|     - | 1228 | `					ph7_value *apCbArg[1];` |
|     5 | 1229 | `					apCbArg[0] = &sCbArg;` |
|     5 | 1230 | `					rcCb = PH7_VmCallUserFunction(ud->pVm,&sCbName,1,apCbArg,&sCbRet);` |
|     - | 1231 | `				}` |
|     5 | 1232 | `				PH7_MemObjRelease(&sCbRet);` |
|     5 | 1233 | `				PH7_MemObjRelease(&sCbArg);` |
|     5 | 1234 | `				PH7_MemObjRelease(&sCbName);` |
|     5 | 1235 | `				if( rcCb == PH7_EXCEPTION \|\| ud->pVm->nBoundaryRc != 0 ){` |
|   ! 0 | 1236 | `					ud->exc = 1;` |
|   ! 0 | 1237 | `					SyBlobRelease(&sCb);` |
|   ! 0 | 1238 | `					return 0;` |
|     - | 1239 | `				}` |
|     5 | 1240 | `				pClass = PH7_VmExtractClass(ud->pVm,zClass,nLen,TRUE,0);` |
|     5 | 1241 | `				if( pClass == 0 ){` |
|     4 | 1242 | `					ph7_context_throw_error_format(ud->pCtx,PH7_CTX_WARNING,` |
|     - | 1243 | `						"Function %.*s() hasn't defined the class it was called for",` |
|     2 | 1244 | `						(int)SyBlobLength(&sCb),(const char *)SyBlobData(&sCb));` |
|     1 | 1245 | `				}` |
|     2 | 1246 | `			}` |
|    27 | 1247 | `			SyBlobRelease(&sCb);` |
|    27 | 1248 | `			if( pClass == 0 ){` |
|    25 | 1249 | `				bIncomplete = bStampName = 1;` |
|    13 | 1250 | `			}` |
|   320 | 1251 | `		}else if( PH7_VmIsIncompleteClass(ud->pVm,pClass) ){` |
|     - | 1252 | `			/* A payload naming the carrier ITSELF: carrier semantics (raw dynamic` |
|     - | 1253 | `			 * properties), but php stamps no name member for it. */` |
|    11 | 1254 | `			bIncomplete = 1;` |
|     5 | 1255 | `		}` |
|     - | 1256 | `	}` |
|     - | 1257 | `	/*` |
|     - | 1258 | `	 * The refusal READING one back. It mirrors the writing side's two kinds,` |
|     - | 1259 | `	 * including the second sentence, and the escape from the soft kind is the` |
|     - | 1260 | ``	 * READING magic: `__wakeup()` or `__unserialize()` declared anywhere in the`` |
|     - | 1261 | ``	 * chain, where the writing side wants `__serialize()`/`__sleep()`. A`` |
|     - | 1262 | `	 * subclass of a DOM node with one of those unserializes normally; the same` |
|     - | 1263 | ``	 * subclass with only `__sleep()` does not.`` |
|     - | 1264 | `	 *` |
|     - | 1265 | ``	 * The hard flag admits no escape at all: `class M extends PDO` declaring`` |
|     - | 1266 | ``	 * `__unserialize()` still reports the plain sentence, and so does a`` |
|     - | 1267 | `	 * subclass of SplFileInfo, which is the hard kind despite being SPL.` |
|     - | 1268 | `	 *` |
|     - | 1269 | `	 * Unlike the writing side this cannot sit after a magic lookup, because` |
|     - | 1270 | `	 * the magic runs on an instance and the whole point is not to build one --` |
|     - | 1271 | `	 * so the lookup is explicit here.` |
|     - | 1272 | `	 *` |
|     - | 1273 | `	 * Two more rules, both measured: it fires on the HEADER, before the body is` |
|     - | 1274 | `	 * read (a truncated payload behind a denied class name is still this` |
|     - | 1275 | `	 * exception, not a syntax error), and it is skipped for the incomplete` |
|     - | 1276 | ``	 * carrier, because `allowed_classes: false` never builds the named class --`` |
|     - | 1277 | `	 * php hands back __PHP_Incomplete_Class there without complaint.` |
|     - | 1278 | `	 */` |
|   353 | 1279 | `	if( !bIncomplete && VmClassRefusesSerialize(pClass,PH7_CLASS_NOSERIALIZE) ){` |
|    34 | 1280 | `		PH7_VmThrowException(ud->pCtx,"Exception",` |
|    11 | 1281 | `			"Unserialization of '%z' is not allowed",&pClass->sDisp);` |
|    23 | 1282 | `		ud->exc = 1;` |
|    23 | 1283 | `		return 0;` |
|     - | 1284 | `	}` |
|   326 | 1285 | `	if( !bIncomplete && VmClassRefusesSerialize(pClass,PH7_CLASS_NOSERIALIZE_SUBOK)` |
|   145 | 1286 | `	 && PH7_ClassExtractMethod(pClass,"__wakeup",sizeof("__wakeup")-1) == 0` |
|    14 | 1287 | `	 && PH7_ClassExtractMethod(pClass,"__unserialize",sizeof("__unserialize")-1) == 0 ){` |
|   ! 0 | 1288 | `		PH7_VmThrowException(ud->pCtx,"Exception",` |
|     - | 1289 | `			"Unserialization of '%z' is not allowed, unless unserialization methods "` |
|   ! 0 | 1290 | `			"are implemented in a subclass",&pClass->sDisp);` |
|   ! 0 | 1291 | `		ud->exc = 1;` |
|   ! 0 | 1292 | `		return 0;` |
|     - | 1293 | `	}` |
|   331 | 1294 | `	if( bIncomplete ){` |
|    55 | 1295 | `		pClass = ud->pVm->pIncClass;` |
|    55 | 1296 | `		if( pClass == 0 ){ return 0; } /* defensive: the carrier is always installed */` |
|    27 | 1297 | `	}` |
|   331 | 1298 | `	if( VmClassStaticDeferPending(pClass) ){` |
|     - | 1299 | `		/* Instantiating materializes the class's static table, so a default that` |
|     - | 1300 | `		 * threw at the declaration raises here — before any object exists —` |
|     - | 1301 | ``		 * exactly as `new C` does. Propagated through ud->exc like a throwing`` |
|     - | 1302 | `		 * __wakeup(), not as a parse failure. */` |
|     3 | 1303 | `		sxi32 rcMat = PH7_VmMaterializeClassStatics(ud->pVm,pClass);` |
|     3 | 1304 | `		if( rcMat != SXRET_OK ){ ud->exc = 1; return 0; }` |
|   ! 0 | 1305 | `	}` |
|   329 | 1306 | `	pThis = PH7_NewClassInstance(ud->pVm,pClass);` |
|   329 | 1307 | `	if( pThis == 0 ){ return 0; }` |
|   329 | 1308 | `	pObjVal = ph7_context_new_scalar(ud->pCtx);` |
|   329 | 1309 | `	if( pObjVal == 0 ){ PH7_ClassInstanceUnref(pThis); return 0; }` |
|   329 | 1310 | `	pObjVal->x.pOther = pThis;       /* take the instance's single reference */` |
|   329 | 1311 | `	MemObjSetType(pObjVal,MEMOBJ_OBJ);` |
|     - | 1312 | `	/* Publish it under its number NOW: a payload whose object refers to itself` |
|     - | 1313 | ``	 * (`$o->self = $o`) writes `r:` at this number from inside the body below. */`` |
|   329 | 1314 | `	VmUnRefBind(ud,nMe,SXU32_HIGH,pObjVal,0);` |
|   329 | 1315 | `	if( bStampName ){` |
|     - | 1316 | `		/* The magic member comes first (php's property order), holding the name` |
|     - | 1317 | `		 * the payload spelled — what get_class() lost and re-serialization needs. */` |
|     - | 1318 | `		ph7_value sName;` |
|    45 | 1319 | `		PH7_MemObjInit(ud->pVm,&sName);` |
|    45 | 1320 | `		PH7_MemObjStringAppend(&sName,zClass,nLen);` |
|    45 | 1321 | `		VmUnserializeIncompleteProp(ud,pThis,` |
|     - | 1322 | `			PH7_INCOMPLETE_MAGIC_MEMBER,sizeof(PH7_INCOMPLETE_MAGIC_MEMBER)-1,&sName);` |
|    45 | 1323 | `		PH7_MemObjRelease(&sName);` |
|    22 | 1324 | `	}` |
|     - | 1325 | `	/* Does the class define __unserialize()? Then collect the pairs into an array.` |
|     - | 1326 | `	 * The carrier consults NO magic method: php calls neither __unserialize() nor` |
|     - | 1327 | `	 * __wakeup() for a class it refused to build — that is the option's point. */` |
|   329 | 1328 | `	pMethod = bIncomplete ? 0` |
|   297 | 1329 | `		: PH7_ClassExtractMethod(pClass,"__unserialize",sizeof("__unserialize")-1);` |
|   329 | 1330 | `	if( pMethod ){` |
|   162 | 1331 | `		pArrVal = ph7_context_new_array(ud->pCtx);` |
|   162 | 1332 | `		if( pArrVal == 0 ){ ph7_context_release_value(ud->pCtx,pObjVal); return 0; }` |
|    79 | 1333 | `	}` |
|   329 | 1334 | `	ud->depth++;` |
|   991 | 1335 | `	for( i = 0; i < count; i++ ){` |
|     - | 1336 | `		ph7_value *pKey, *pVal, *pSlot;` |
|     - | 1337 | `		VmClassAttr *pVmAttr;` |
|     - | 1338 | `		sxu32 nPropMe, nSlot;` |
|     - | 1339 | `		int nKey;` |
|     - | 1340 | `		const char *zKey;` |
|   689 | 1341 | `		if( VmUnserializeShortContainer(ud) ){ goto fail; }` |
|   680 | 1342 | `		if( pArrVal ){` |
|     - | 1343 | `			/* __unserialize(): the pairs are an ARRAY's, not properties. */` |
|   485 | 1344 | `			if( !VmUnserializeMapEntry(ud,pArrVal) ){ goto fail; }` |
|   486 | 1345 | `			continue;` |
|     - | 1346 | `		}` |
|   197 | 1347 | `		pKey = VmUnserializeValue(ud,SXU32_HIGH);  /* a key takes no number */` |
|   197 | 1348 | `		if( pKey == 0 ){ goto fail; }` |
|   193 | 1349 | `		zKey = ph7_value_to_string(pKey,&nKey);` |
|     - | 1350 | `		/* The property is resolved — and a dynamic one created — BEFORE the value` |
|     - | 1351 | `		 * is read: its slot is what this value's back-reference number names. */` |
|   125 | 1352 | `		pVmAttr = bIncomplete ? VmUnserializePropAttrRaw(ud,pThis,zKey,(sxu32)nKey)` |
|   163 | 1353 | `		                      : VmUnserializeTargetAttr(ud,pThis,zKey,nKey);` |
|   193 | 1354 | `		if( pVmAttr == 0 \|\| pVmAttr->nIdx == SXU32_HIGH ){ goto fail; }` |
|   187 | 1355 | `		if( VmUnPeekBackRef(ud) ){` |
|     - | 1356 | `			/* Two properties that shared one reference when the payload was` |
|     - | 1357 | `			 * written share one slot again. */` |
|     - | 1358 | `			sxu32 nTarget;` |
|     5 | 1359 | `			if( !VmUnParseBackRef(ud,pVmAttr->nIdx,&nTarget) ){ goto fail; }` |
|     3 | 1360 | `			PH7_VmBindAttrRef(ud->pVm,pVmAttr,nTarget);` |
|     3 | 1361 | `			continue;` |
|     - | 1362 | `		}` |
|   183 | 1363 | `		nSlot = pVmAttr->nIdx;` |
|   183 | 1364 | `		nPropMe = VmUnRefReserve(ud);` |
|   183 | 1365 | `		VmUnRefBind(ud,nPropMe,nSlot,(ph7_value *)PH7_MemObjAt(&ud->pVm->aMemObj,nSlot),pVmAttr);` |
|   183 | 1366 | `		pVal = VmUnserializeValue(ud,nPropMe);` |
|   183 | 1367 | `		if( pVal == 0 ){ goto fail; }` |
|   181 | 1368 | `		pSlot = (ph7_value *)PH7_MemObjAt(&ud->pVm->aMemObj,pVmAttr->nIdx);` |
|   181 | 1369 | `		if( pSlot ){ PH7_MemObjStore(pVal,pSlot); }` |
|     - | 1370 | `		/* php's unserialize() bypasses the property type-check but the value IS` |
|     - | 1371 | `		 * now set, so a typed property must no longer read as "uninitialized" —` |
|     - | 1372 | `		 * clear the per-instance UNINIT latch directly (routing through` |
|     - | 1373 | `		 * VmEnforcePropertyTypeOnStore would wrongly apply readonly/scope checks` |
|     - | 1374 | `		 * from global scope). */` |
|   181 | 1375 | `		pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|     - | 1376 | `		/* The pKey/pVal temporaries are not released per property (bulk-reclaimed` |
|     - | 1377 | `		 * at context teardown) — see the O(N^2) note in VmUnserializeMapEntry. */` |
|    92 | 1378 | `	}` |
|   312 | 1379 | `	ud->depth--;` |
|   312 | 1380 | `	if( !VmUnExpect(ud,'}') ){ ph7_context_release_value(ud->pCtx,pObjVal); return 0; }` |
|     - | 1381 | `	/* Wakeup protocol: __unserialize($array) first, else __wakeup(). */` |
|   312 | 1382 | `	if( pMethod ){` |
|     - | 1383 | `		ph7_value sRes; sxi32 rc;` |
|   162 | 1384 | `		PH7_MemObjInit(ud->pVm,&sRes);` |
|   162 | 1385 | `		rc = PH7_VmCallMagicMethod(ud->pVm,pThis,pMethod,&sRes,1,&pArrVal);` |
|   162 | 1386 | `		PH7_MemObjRelease(&sRes);` |
|   162 | 1387 | `		ph7_context_release_value(ud->pCtx,pArrVal);` |
|   162 | 1388 | `		if( rc == PH7_EXCEPTION ){ ud->exc = 1; return 0; }` |
|    72 | 1389 | `	}else{` |
|   152 | 1390 | `		pMethod = PH7_ClassExtractMethod(pClass,"__wakeup",sizeof("__wakeup")-1);` |
|   152 | 1391 | `		if( pMethod ){` |
|     - | 1392 | `			ph7_value sRes; sxi32 rc;` |
|    28 | 1393 | `			PH7_MemObjInit(ud->pVm,&sRes);` |
|    28 | 1394 | `			rc = PH7_VmCallMagicMethod(ud->pVm,pThis,pMethod,&sRes,0,0);` |
|    28 | 1395 | `			PH7_MemObjRelease(&sRes);` |
|    28 | 1396 | `			if( rc == PH7_EXCEPTION ){ ud->exc = 1; return 0; }` |
|     7 | 1397 | `		}` |
|     - | 1398 | `	}` |
|   278 | 1399 | `	return pObjVal;` |
|     8 | 1400 | `fail:` |
|    18 | 1401 | `	ud->depth--;` |
|    18 | 1402 | `	if( pArrVal ){ ph7_context_release_value(ud->pCtx,pArrVal); }` |
|    18 | 1403 | `	ph7_context_release_value(ud->pCtx,pObjVal);` |
|    18 | 1404 | `	return 0;` |
|   192 | 1405 | `}` |
|     - | 1406 | `/* Parse E:<len>:"Class:CASE"; into the enum case SINGLETON (php 8.1). */` |
|    18 | 1407 | `static ph7_value * VmUnserializeEnumCase(unserialize_data *ud)` |
|     1 | 1408 | `{` |
|     - | 1409 | `	sxu32 nLen, nCls, i;` |
|     - | 1410 | `	const char *zBody;` |
|     - | 1411 | `	ph7_class *pClass;` |
|     - | 1412 | `	ph7_class_attr *pAttr;` |
|     - | 1413 | `	ph7_value *pSlot, *pOut;` |
|     - | 1414 | `	const char *zLen;` |
|    19 | 1415 | `	if( !VmUnExpect(ud,'E') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|    19 | 1416 | `	zLen = ud->zCur;` |
|    19 | 1417 | `	if( !VmUnParseUInt(ud,&nLen) ){ return 0; }` |
|    19 | 1418 | `	if( !VmUnExpect(ud,':') \|\| !VmUnExpect(ud,'"') ){ return 0; }` |
|     - | 1419 | `	/* Same two offset rules as the O: header above. */` |
|    19 | 1420 | `	if( nLen < 1 \|\| nLen > (sxu32)(ud->zEnd - ud->zCur) ){` |
|     5 | 1421 | `		if( ud->zErr == 0 ){ ud->zErr = zLen; }` |
|     5 | 1422 | `		return 0;` |
|     - | 1423 | `	}` |
|    15 | 1424 | `	zBody = ud->zCur; ud->zCur += nLen;` |
|    15 | 1425 | `	if( !VmUnExpect(ud,'"') ){` |
|     5 | 1426 | `		if( ud->zErr == 0 ){ ud->zErr = ud->zCur; }` |
|     5 | 1427 | `		return 0;` |
|     - | 1428 | `	}` |
|    11 | 1429 | `	if( !VmUnExpect(ud,';') ){` |
|     3 | 1430 | `		if( ud->zErr == 0 ){ ud->zErr = ud->zCur; }` |
|     3 | 1431 | `		return 0;` |
|     - | 1432 | `	}` |
|     - | 1433 | `	/* Split "Class:CASE" at the LAST ':' (class names never contain ':') */` |
|     9 | 1434 | `	nCls = 0;` |
|    53 | 1435 | `	for( i = nLen ; i > 0 ; i-- ){` |
|    53 | 1436 | `		if( zBody[i-1] == ':' ){ nCls = i - 1; break; }` |
|    23 | 1437 | `	}` |
|     9 | 1438 | `	if( nCls == 0 \|\| nCls + 1 >= nLen ){ return 0; }` |
|     9 | 1439 | `	pClass = PH7_VmExtractClass(ud->pVm,zBody,nCls,FALSE,0);` |
|     9 | 1440 | `	while( pClass && (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|   ! 0 | 1441 | `		pClass = pClass->pNextName;` |
|   ! 0 | 1442 | `	}` |
|     9 | 1443 | `	if( pClass == 0 ){` |
|     - | 1444 | `		/* php names the class it could not find before the generic offset report.` |
|     - | 1445 | `		 * An enum has no incomplete-object fallback: the case IDENTITY is the whole` |
|     - | 1446 | `		 * point of the E: tag, so there is nothing to stand in for it. */` |
|   ! 0 | 1447 | `		ph7_context_throw_error_format(ud->pCtx,PH7_CTX_WARNING,` |
|   ! 0 | 1448 | `			"Class '%.*s' not found",(int)nCls,zBody);` |
|   ! 0 | 1449 | `		return 0;` |
|     - | 1450 | `	}` |
|     9 | 1451 | `	pAttr = PH7_ClassExtractConstant(pClass,&zBody[nCls+1],nLen - nCls - 1);` |
|     9 | 1452 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) == 0 ){ return 0; }` |
|     9 | 1453 | `	if( PH7_VmMaterializeClassConst(ud->pVm,pClass,pAttr) != SXRET_OK ){` |
|   ! 0 | 1454 | `		ud->exc = 1;` |
|   ! 0 | 1455 | `		return 0;` |
|     - | 1456 | `	}` |
|     9 | 1457 | `	pSlot = (ph7_value *)PH7_MemObjAt(&ud->pVm->aMemObj,pAttr->nIdx);` |
|     9 | 1458 | `	if( pSlot == 0 ){ return 0; }` |
|     9 | 1459 | `	pOut = ph7_context_new_scalar(ud->pCtx);` |
|     9 | 1460 | `	if( pOut ){ PH7_MemObjStore(pSlot,pOut); } /* retains the singleton */` |
|     9 | 1461 | `	return pOut;` |
|    10 | 1462 | `}` |
|     - | 1463 | `/*` |
|     - | 1464 | ` * Parse one value. Wrapped by VmUnserializeValue() so every failure records WHERE` |
|     - | 1465 | ` * it began: php reports the offset of the token it could not match, not the byte` |
|     - | 1466 | `` * its parser happened to stop on (`unserialize("i:1")` is "offset 0", the start of`` |
|     - | 1467 | `` * the unterminated `i:` token, and a short array is the offset of the element it`` |
|     - | 1468 | ` * went looking for). The first — innermost — failure to unwind wins, which is the` |
|     - | 1469 | ` * one php names.` |
|     - | 1470 | ` */` |
|     - | 1471 | `static ph7_value * VmUnserializeValueBody(unserialize_data *ud,sxu32 nMe);` |
|  5050 | 1472 | `static ph7_value * VmUnserializeValue(unserialize_data *ud,sxu32 nMe)` |
|     5 | 1473 | `{` |
|  5055 | 1474 | `	const char *zStart = ud->zCur;` |
|  5055 | 1475 | `	ph7_value *pOut = VmUnserializeValueBody(ud,nMe);` |
|  5055 | 1476 | `	if( pOut == 0 && ud->zErr == 0 && !ud->exc ){` |
|    48 | 1477 | `		ud->zErr = zStart;` |
|    23 | 1478 | `	}` |
|  5055 | 1479 | `	return pOut;` |
|     5 | 1480 | `}` |
|  5062 | 1481 | `static ph7_value * VmUnserializeValueBody(unserialize_data *ud,sxu32 nMe)` |
|     5 | 1482 | `{` |
|     - | 1483 | `	ph7_value *pOut;` |
|     - | 1484 | `	char c;` |
|  5067 | 1485 | `	if( ud->depth > ud->maxDepth ){` |
|     - | 1486 | `		/* php reports the limit once, names the knob, then falls through to the` |
|     - | 1487 | `		 * generic "Error at offset" failure -- so latch it and keep unwinding. */` |
|     7 | 1488 | `		if( !ud->depthErr ){` |
|     7 | 1489 | `			ud->depthErr = 1;` |
|    10 | 1490 | `			ph7_context_throw_error_format(ud->pCtx,PH7_CTX_WARNING,` |
|     - | 1491 | `				"Maximum depth of %d exceeded. The depth limit can be changed using "` |
|     - | 1492 | `				"the max_depth unserialize() option or the unserialize_max_depth ini setting",` |
|     3 | 1493 | `				ud->maxDepth);` |
|     3 | 1494 | `		}` |
|     7 | 1495 | `		return 0;` |
|     - | 1496 | `	}` |
|  5061 | 1497 | `	if( ud->zCur >= ud->zEnd ){ return 0; }` |
|  5053 | 1498 | `	c = ud->zCur[0];` |
|  5053 | 1499 | `	switch( c ){` |
|    19 | 1500 | `	case 'N': /* N; */` |
|    40 | 1501 | `		if( ud->zCur+2 > ud->zEnd \|\| ud->zCur[1] != ';' ){ return 0; }` |
|    38 | 1502 | `		ud->zCur += 2;` |
|    38 | 1503 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|    38 | 1504 | `		if( pOut ){ ph7_value_null(pOut); }` |
|    38 | 1505 | `		return pOut;` |
|    21 | 1506 | `	case 'b': /* b:0; / b:1; */` |
|    54 | 1507 | `		if( ud->zCur+4 > ud->zEnd \|\| ud->zCur[1] != ':'` |
|    55 | 1508 | `		    \|\| (ud->zCur[2] != '0' && ud->zCur[2] != '1') \|\| ud->zCur[3] != ';' ){ return 0; }` |
|    41 | 1509 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|    41 | 1510 | `		if( pOut ){ ph7_value_bool(pOut, ud->zCur[2]=='1'); }` |
|    41 | 1511 | `		ud->zCur += 4;` |
|    41 | 1512 | `		return pOut;` |
|  1099 | 1513 | `	case 'i': { /* i:<int>; */` |
|     - | 1514 | `		ph7_int64 v;` |
|  2203 | 1515 | `		int ovf = 0;` |
|  2203 | 1516 | `		if( !VmUnExpect(ud,'i') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|  2203 | 1517 | `		if( !VmUnParseInt64(ud,&v,&ovf) \|\| !VmUnExpect(ud,';') ){ return 0; }` |
|  2189 | 1518 | `		if( ovf ){` |
|     - | 1519 | `			/* php reports the clamp and keeps the saturated value, once per TOKEN --` |
|     - | 1520 | `			 * so an array of out-of-range integers warns once per element. Reported` |
|     - | 1521 | ``			 * only after the token parses: a malformed one (`i:99...9X`) is php's`` |
|     - | 1522 | `			 * "Error at offset" and nothing else. */` |
|    19 | 1523 | `			ph7_context_throw_error_format(ud->pCtx,PH7_CTX_WARNING,` |
|     - | 1524 | `				"Numerical result out of range");` |
|     9 | 1525 | `		}` |
|  2189 | 1526 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|  2189 | 1527 | `		if( pOut ){ ph7_value_int64(pOut,v); }` |
|  2189 | 1528 | `		return pOut;` |
|     - | 1529 | `	}` |
|    12 | 1530 | `	case 'd': { /* d:<float>; */` |
|     - | 1531 | `		const char *zStart;` |
|    25 | 1532 | `		double d = 0;` |
|    25 | 1533 | `		if( !VmUnExpect(ud,'d') \|\| !VmUnExpect(ud,':') ){ return 0; }` |
|    25 | 1534 | `		zStart = ud->zCur;` |
|   137 | 1535 | `		while( ud->zCur < ud->zEnd && ud->zCur[0] != ';' ){ ud->zCur++; }` |
|    25 | 1536 | `		if( ud->zCur >= ud->zEnd ){ return 0; }` |
|     - | 1537 | `		/* INF / -INF / NAN, else a plain real literal. Parse via libc strtod (the` |
|     - | 1538 | `		 * correctly-rounded inverse of the strtod-verified shortest repr that` |
|     - | 1539 | `		 * VmSerializeReal emits) so unserialize(serialize($f)) is bit-exact.` |
|     - | 1540 | `		 * (SyStrToReal delegates to strtod nowadays; the direct call is kept` |
|     - | 1541 | `		 * because the INF/NAN tags above are already split out here.) */` |
|    25 | 1542 | `		if( (ud->zCur-zStart) == 3 && SyStrnicmp(zStart,"INF",3)==0 ){ d = PH7_INF_VALUE(); }` |
|    25 | 1543 | `		else if( (ud->zCur-zStart)==4 && SyStrnicmp(zStart,"-INF",4)==0 ){ d = -PH7_INF_VALUE(); }` |
|    25 | 1544 | `		else if( (ud->zCur-zStart)==3 && SyStrnicmp(zStart,"NAN",3)==0 ){ d = PH7_NAN_VALUE(); }` |
|     - | 1545 | `		else {` |
|     - | 1546 | `			char zNum[64];` |
|    25 | 1547 | `			int nNum = (int)(ud->zCur - zStart);` |
|    25 | 1548 | `			if( nNum > (int)sizeof(zNum)-1 ){ nNum = (int)sizeof(zNum)-1; }` |
|    25 | 1549 | `			SyMemcpy(zStart,zNum,(sxu32)nNum);` |
|    25 | 1550 | `			zNum[nNum] = '\0';` |
|    25 | 1551 | `			d = strtod(zNum,0);` |
|     - | 1552 | `		}` |
|    25 | 1553 | `		ud->zCur++; /* skip ';' */` |
|    25 | 1554 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|    25 | 1555 | `		if( pOut ){ ph7_value_double(pOut,d); }` |
|    25 | 1556 | `		return pOut;` |
|     - | 1557 | `	}` |
|  1041 | 1558 | `	case 's': { /* s:<len>:"..."; */` |
|     - | 1559 | `		const char *zStr; int nStr;` |
|  2086 | 1560 | `		if( !VmUnParseString(ud,&zStr,&nStr) ){ return 0; }` |
|  2078 | 1561 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|  2078 | 1562 | `		if( pOut ){ ph7_value_string(pOut,zStr,nStr); }` |
|  2078 | 1563 | `		return pOut;` |
|     - | 1564 | `	}` |
|   117 | 1565 | `	case 'a':` |
|   238 | 1566 | `		return VmUnserializeArray(ud,nMe);` |
|   187 | 1567 | `	case 'O':` |
|   379 | 1568 | `		return VmUnserializeObject(ud,nMe);` |
|     9 | 1569 | `	case 'E':` |
|    19 | 1570 | `		return VmUnserializeEnumCase(ud);` |
|     8 | 1571 | `	case 'r': { /* r:<n>; — the object written under that number */` |
|     - | 1572 | `		VmUnRef *pRec;` |
|     - | 1573 | `		sxu32 nId;` |
|    17 | 1574 | `		if( !VmUnParseRefToken(ud,'r',&nId) ){ return 0; }` |
|    15 | 1575 | `		pRec = VmUnRefAt(ud,nId);` |
|    15 | 1576 | `		if( pRec == 0 \|\| pRec->pVal == 0 \|\| !ph7_value_is_object(pRec->pVal) ){` |
|     - | 1577 | ``			/* php refuses an `r:` that does not name an object — including one`` |
|     - | 1578 | ``			 * naming the value being read (`r:1;` on its own). */`` |
|     7 | 1579 | `			if( ud->zErr == 0 ){ ud->zErr = ud->zCur; }` |
|     7 | 1580 | `			return 0;` |
|     - | 1581 | `		}` |
|     9 | 1582 | `		pOut = ph7_context_new_scalar(ud->pCtx);` |
|     9 | 1583 | `		if( pOut ){ PH7_MemObjStore(pRec->pVal,pOut); } /* the SAME instance */` |
|     9 | 1584 | `		return pOut;` |
|     - | 1585 | `	}` |
|     1 | 1586 | `	case 'R': {` |
|     - | 1587 | `		/* A reference back-reference is a BIND, not a value: it needs the slot it` |
|     - | 1588 | `		 * is landing in, so every container parses its own (VmUnPeekBackRef). One` |
|     - | 1589 | `		 * reaching here is the OUTERMOST value, which nothing holds — php's` |
|     - | 1590 | ``		 * `rval_ref == rval` refusal, blamed past the token it did read. */`` |
|     - | 1591 | `		sxu32 nId;` |
|     3 | 1592 | `		if( VmUnParseRefToken(ud,'R',&nId) && ud->zErr == 0 ){` |
|     3 | 1593 | `			ud->zErr = ud->zCur;` |
|     1 | 1594 | `		}` |
|     3 | 1595 | `		return 0;` |
|     - | 1596 | `	}` |
|     4 | 1597 | `	default:` |
|    10 | 1598 | `		return 0;` |
|     - | 1599 | `	}` |
|  2530 | 1600 | `}` |
|     - | 1601 | `/*` |
|     - | 1602 | ` * php's "X given" name for an option value.` |
|     - | 1603 | ` *` |
|     - | 1604 | ` * VmValueGivenName() answers through ph7_type_name(), which reports a WHOLE REAL` |
|     - | 1605 | `` * (2.0) as `int` because PHL caches the integer form in the same slot (the recorded`` |
|     - | 1606 | ` * dual-flag model, and why ph7_value_is_int() is lenient). The option checks need` |
|     - | 1607 | `` * php's DECLARED type, so a real is named `float` whatever it caches — and the`` |
|     - | 1608 | ` * int check below tests MEMOBJ_REAL first for the same reason.` |
|     - | 1609 | ` */` |
|    24 | 1610 | `static const char * VmUnserializeOptionType(ph7_value *pVal, char *zBuf, sxu32 nBuf)` |
|     1 | 1611 | `{` |
|    25 | 1612 | `	if( ph7_value_is_float(pVal) ){` |
|     7 | 1613 | `		return "float";` |
|     - | 1614 | `	}` |
|    19 | 1615 | `	return VmValueGivenName(pVal,zBuf,nBuf);` |
|    13 | 1616 | `}` |
|     - | 1617 | `/* Reject a non-string member of an "allowed_classes" list. */` |
|    14 | 1618 | `static int VmUnserializeClassListWalker(ph7_value *pKey, ph7_value *pData, void *pUserData)` |
|     1 | 1619 | `{` |
|    15 | 1620 | `	ph7_context *pCtx = (ph7_context *)pUserData;` |
|     - | 1621 | `	char zGiven[64];` |
|     7 | 1622 | `	SXUNUSED(pKey);` |
|    15 | 1623 | `	if( ph7_value_is_string(pData) ){` |
|    11 | 1624 | `		return PH7_OK;` |
|     - | 1625 | `	}` |
|     7 | 1626 | `	PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1627 | `		"unserialize(): Option \"allowed_classes\" must be an array of class names, "` |
|     2 | 1628 | `		"%s given",VmUnserializeOptionType(pData,zGiven,sizeof(zGiven)));` |
|     5 | 1629 | `	return SXERR_ABORT;` |
|     8 | 1630 | `}` |
|     - | 1631 | `/*` |
|     - | 1632 | ` * Validate unserialize()'s $options array, php's way.` |
|     - | 1633 | ` *` |
|     - | 1634 | ` * php checks the option map BEFORE parsing a single byte of $data, and the two` |
|     - | 1635 | `` * options it knows have distinct shapes: "allowed_classes" is `array\|bool` and,`` |
|     - | 1636 | ` * when it is an array, every element must be a class-name STRING; "max_depth" is` |
|     - | 1637 | `` * a non-negative `int`. Any other key is ignored, with no diagnostic. PHL used to`` |
|     - | 1638 | ` * ignore the whole array, so a mistyped option silently did nothing at all.` |
|     - | 1639 | ` *` |
|     - | 1640 | ` * On success *piMaxDepth carries the effective depth limit, and the allowed-class` |
|     - | 1641 | ` * spec comes back through *pbAllowAll / *ppAllowedList. The list pointer aliases` |
|     - | 1642 | ` * the $options ARGUMENT's own element rather than a copy: the argument slot holds` |
|     - | 1643 | ` * its own reference for the whole builtin call and no userland name reaches that` |
|     - | 1644 | ` * copy, so a __wakeup() that rewrites (or unsets) the caller's array mid-parse` |
|     - | 1645 | ` * cannot move or free what this walks — php snapshots for the same reason.` |
|     - | 1646 | ` */` |
|    90 | 1647 | `static sxi32 VmUnserializeCheckOptions(` |
|     - | 1648 | `	ph7_context *pCtx,      /* Call context (for the throw) */` |
|     - | 1649 | `	ph7_value *pOptions,    /* The $options array */` |
|     - | 1650 | `	int *piMaxDepth,        /* OUT: effective max_depth */` |
|     - | 1651 | `	int *pbAllowAll,        /* OUT: allowed_classes was absent or true */` |
|     - | 1652 | `	ph7_value **ppAllowedList /* OUT: the allowed_classes LIST, when one was given */` |
|     - | 1653 | `	)` |
|     1 | 1654 | `{` |
|     - | 1655 | `	char zGiven[64];` |
|     - | 1656 | `	ph7_value *pOpt;` |
|    91 | 1657 | `	pOpt = ph7_array_fetch(pOptions,"allowed_classes",sizeof("allowed_classes")-1);` |
|    91 | 1658 | `	if( pOpt ){` |
|    47 | 1659 | `		if( ph7_value_is_array(pOpt) ){` |
|    17 | 1660 | `			if( ph7_array_walk(pOpt,VmUnserializeClassListWalker,pCtx) != PH7_OK ){` |
|     5 | 1661 | `				return PH7_EXCEPTION; /* the walker already threw */` |
|     - | 1662 | `			}` |
|    13 | 1663 | `			*ppAllowedList = pOpt;` |
|    37 | 1664 | `		}else if( !ph7_value_is_bool(pOpt) ){` |
|    16 | 1665 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1666 | `				"unserialize(): Option \"allowed_classes\" must be of type array\|bool, "` |
|     5 | 1667 | `				"%s given",VmUnserializeOptionType(pOpt,zGiven,sizeof(zGiven)));` |
|   ! 0 | 1668 | `		}else{` |
|    21 | 1669 | `			*pbAllowAll = ph7_value_to_bool(pOpt) != 0;` |
|     - | 1670 | `		}` |
|    16 | 1671 | `	}` |
|    77 | 1672 | `	pOpt = ph7_array_fetch(pOptions,"max_depth",sizeof("max_depth")-1);` |
|    77 | 1673 | `	if( pOpt ){` |
|     - | 1674 | `		ph7_int64 iVal;` |
|    41 | 1675 | `		if( ph7_value_is_float(pOpt) \|\| !ph7_value_is_int(pOpt) ){` |
|    16 | 1676 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1677 | `				"unserialize(): Option \"max_depth\" must be of type int, %s given",` |
|     5 | 1678 | `				VmUnserializeOptionType(pOpt,zGiven,sizeof(zGiven)));` |
|     - | 1679 | `		}` |
|    31 | 1680 | `		iVal = ph7_value_to_int64(pOpt);` |
|    31 | 1681 | `		if( iVal < 0 ){` |
|     3 | 1682 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1683 | `				"unserialize(): Option \"max_depth\" must be greater than or equal to 0");` |
|     - | 1684 | `		}` |
|     - | 1685 | `		/* php reads max_depth == 0 as UNLIMITED (the unserialize_max_depth ini` |
|     - | 1686 | `		 * uses the same convention), so it falls back to PHL's own recursion` |
|     - | 1687 | `		 * guard rather than rejecting everything nested — this parser is` |
|     - | 1688 | `		 * recursive-descent and cannot actually run unbounded. Anything above` |
|     - | 1689 | `		 * that guard is likewise capped by it. */` |
|    29 | 1690 | `		if( iVal == 0 \|\| iVal > (ph7_int64)SERIALIZE_MAX_DEPTH ){` |
|    11 | 1691 | `			*piMaxDepth = SERIALIZE_MAX_DEPTH;` |
|     6 | 1692 | `		}else{` |
|    19 | 1693 | `			*piMaxDepth = (int)iVal;` |
|     - | 1694 | `		}` |
|    14 | 1695 | `	}` |
|    65 | 1696 | `	return PH7_OK;` |
|    46 | 1697 | `}` |
|     - | 1698 | `/*` |
|     - | 1699 | ` * Unserialize ONE value from a buffer and report how many bytes it took --` |
|     - | 1700 | ` * php's php_var_unserialize(&p, ...) with the cursor left where the value ended.` |
|     - | 1701 | ` *` |
|     - | 1702 | ` * A legacy Serializable payload is a SEQUENCE of serialized values with one-byte` |
|     - | 1703 | `` * separators between them (SplObjectStorage writes `x:<count>;<obj>,<inf>;…m:<members>`),`` |
|     - | 1704 | ` * and only the parser knows where each value stops -- scanning for the next ';'` |
|     - | 1705 | ` * works for a scalar and cuts an object payload in half. Nothing here writes to` |
|     - | 1706 | ` * pCtx->pRet, so a caller can run it in a loop without rule 54's reset dance.` |
|     - | 1707 | ` *` |
|     - | 1708 | ` * Answers SXRET_OK with *pnRead set, SXERR_SYNTAX on a malformed value, or` |
|     - | 1709 | ` * PH7_EXCEPTION when a __wakeup()/__unserialize() threw. Nothing is reported:` |
|     - | 1710 | ` * the caller words php's own diagnostic. *pnRead is set EITHER WAY -- on failure` |
|     - | 1711 | ` * it is where the parser gave up, which is the offset php's own message carries.` |
|     - | 1712 | ` */` |
|   106 | 1713 | `PH7_PRIVATE sxi32 PH7_VmUnserializeOne(ph7_context *pCtx,const char *zIn,int nByte,int *pnRead,ph7_value *pOut)` |
|     4 | 1714 | `{` |
|     - | 1715 | `	unserialize_data ud;` |
|     - | 1716 | `	ph7_value *pVal;` |
|   110 | 1717 | `	if( pnRead ){` |
|   110 | 1718 | `		*pnRead = 0;` |
|    53 | 1719 | `	}` |
|   110 | 1720 | `	if( nByte < 1 ){` |
|     5 | 1721 | `		return SXERR_SYNTAX;` |
|     - | 1722 | `	}` |
|   106 | 1723 | `	ud.pVm = pCtx->pVm;` |
|   106 | 1724 | `	ud.pCtx = pCtx;` |
|   106 | 1725 | `	ud.zCur = zIn;` |
|   106 | 1726 | `	ud.zEnd = &zIn[nByte];` |
|   106 | 1727 | `	ud.depth = 0;` |
|   106 | 1728 | `	ud.maxDepth = SERIALIZE_MAX_DEPTH;` |
|   106 | 1729 | `	ud.depthErr = 0;` |
|   106 | 1730 | `	ud.zErr = 0;` |
|   106 | 1731 | `	ud.shortErr = 0;` |
|   106 | 1732 | `	ud.exc = 0;` |
|   106 | 1733 | `	ud.allowAll = 1;` |
|   106 | 1734 | `	ud.pAllowedList = 0;` |
|   106 | 1735 | `	SySetInit(&ud.aRef,&pCtx->pVm->sAllocator,sizeof(VmUnRef));` |
|   106 | 1736 | `	pVal = VmUnserializeValue(&ud,VmUnRefReserve(&ud));` |
|   106 | 1737 | `	SySetRelease(&ud.aRef);` |
|   106 | 1738 | `	if( ud.exc ){` |
|   ! 0 | 1739 | `		return PH7_EXCEPTION;` |
|     - | 1740 | `	}` |
|   106 | 1741 | `	if( pnRead ){` |
|   106 | 1742 | `		*pnRead = (int)((pVal == 0 && ud.zErr ? ud.zErr : ud.zCur) - zIn);` |
|    51 | 1743 | `	}` |
|   106 | 1744 | `	if( pVal == 0 ){` |
|     3 | 1745 | `		return SXERR_SYNTAX;` |
|     - | 1746 | `	}` |
|   104 | 1747 | `	if( pOut ){` |
|   104 | 1748 | `		PH7_MemObjStore(pVal,pOut);` |
|    50 | 1749 | `	}` |
|   104 | 1750 | `	ph7_context_release_value(pCtx,pVal);` |
|   104 | 1751 | `	return SXRET_OK;` |
|    57 | 1752 | `}` |
|     - | 1753 | `/*` |
|     - | 1754 | ` * mixed unserialize(string $str)` |
|     - | 1755 | ` *  Create a PHP value from a stored representation. Returns false on failure.` |
|     - | 1756 | ` */` |
|   562 | 1757 | `PH7_PRIVATE int vm_builtin_unserialize(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     5 | 1758 | `{` |
|     - | 1759 | `	unserialize_data ud;` |
|     - | 1760 | `	const char *zIn;` |
|     - | 1761 | `	int nByte;` |
|   567 | 1762 | `	int iMaxDepth = SERIALIZE_MAX_DEPTH;` |
|   567 | 1763 | `	int bAllowAll = 1;` |
|   567 | 1764 | `	ph7_value *pAllowedList = 0;` |
|     - | 1765 | `	ph7_value *pVal;` |
|   567 | 1766 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|   ! 0 | 1767 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1768 | `		return PH7_OK;` |
|     - | 1769 | `	}` |
|     - | 1770 | `	/* No max_depth option: the unserialize_max_depth ini is php's default for it` |
|     - | 1771 | `	 * (0 = unlimited, capped by the recursive parser's own guard either way). */` |
|     - | 1772 | `	{` |
|   567 | 1773 | `		ph7_int64 iIniDepth = PH7_VmIniGetInt(pCtx->pVm,"unserialize_max_depth",` |
|     - | 1774 | `			(sxi64)SERIALIZE_MAX_DEPTH);` |
|   567 | 1775 | `		if( iIniDepth > 0 && iIniDepth < (ph7_int64)SERIALIZE_MAX_DEPTH ){` |
|   ! 0 | 1776 | `			iMaxDepth = (int)iIniDepth;` |
|   ! 0 | 1777 | `		}` |
|     - | 1778 | `	}` |
|     - | 1779 | `	/* php validates $options before touching $data — so a bad option throws even` |
|     - | 1780 | `	 * for input that would not have parsed anyway. */` |
|   567 | 1781 | `	if( nArg > 1 && ph7_value_is_array(apArg[1]) ){` |
|    91 | 1782 | `		sxi32 rc = VmUnserializeCheckOptions(pCtx,apArg[1],&iMaxDepth,&bAllowAll,&pAllowedList);` |
|    91 | 1783 | `		if( rc != PH7_OK ){` |
|    27 | 1784 | `			return rc;` |
|     - | 1785 | `		}` |
|    32 | 1786 | `	}` |
|   541 | 1787 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   541 | 1788 | `	if( nByte < 1 ){` |
|     3 | 1789 | `		ph7_result_bool(pCtx,0);` |
|     3 | 1790 | `		return PH7_OK;` |
|     - | 1791 | `	}` |
|   539 | 1792 | `	ud.pVm = pCtx->pVm;` |
|   539 | 1793 | `	ud.pCtx = pCtx;` |
|   539 | 1794 | `	ud.zCur = zIn;` |
|   539 | 1795 | `	ud.zEnd = &zIn[nByte];` |
|   539 | 1796 | `	ud.depth = 0;` |
|   539 | 1797 | `	ud.maxDepth = iMaxDepth;` |
|   539 | 1798 | `	ud.depthErr = 0;` |
|   539 | 1799 | `	ud.zErr = 0;` |
|   539 | 1800 | `	ud.shortErr = 0;` |
|   539 | 1801 | `	ud.exc = 0;` |
|   539 | 1802 | `	ud.allowAll = bAllowAll;` |
|   539 | 1803 | `	ud.pAllowedList = pAllowedList;` |
|   539 | 1804 | `	SySetInit(&ud.aRef,&pCtx->pVm->sAllocator,sizeof(VmUnRef));` |
|   539 | 1805 | `	pVal = VmUnserializeValue(&ud,VmUnRefReserve(&ud));` |
|   539 | 1806 | `	SySetRelease(&ud.aRef);` |
|   539 | 1807 | `	if( ud.exc ){` |
|     - | 1808 | `		/* A __wakeup()/__unserialize() threw: let the exception unwind. */` |
|    70 | 1809 | `		return PH7_EXCEPTION;` |
|     - | 1810 | `	}` |
|   472 | 1811 | `	if( pVal == 0 ){` |
|     - | 1812 | `		/* php always reports WHERE the parse gave up; PH7 failed silently, so a` |
|     - | 1813 | ``		 * corrupt payload was indistinguishable from a serialized `false`. */`` |
|   121 | 1814 | `		if( ud.shortErr ){` |
|     7 | 1815 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     - | 1816 | `				"Unexpected end of serialized data");` |
|     3 | 1817 | `		}` |
|   239 | 1818 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     - | 1819 | `			"Error at offset %d of %d bytes",` |
|   118 | 1820 | `			(int)((ud.zErr ? ud.zErr : ud.zCur) - zIn),nByte);` |
|   121 | 1821 | `		ph7_result_bool(pCtx,0);` |
|   121 | 1822 | `		return PH7_OK;` |
|     - | 1823 | `	}` |
|   354 | 1824 | `	if( ud.zCur < ud.zEnd ){` |
|     - | 1825 | `		/* php parses the FIRST value and keeps it, but says the rest was ignored. */` |
|     4 | 1826 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     - | 1827 | `			"Extra data starting at offset %d of %d bytes",` |
|     2 | 1828 | `			(int)(ud.zCur - zIn),nByte);` |
|     1 | 1829 | `	}` |
|   354 | 1830 | `	ph7_result_value(pCtx,pVal);` |
|   354 | 1831 | `	ph7_context_release_value(pCtx,pVal);` |
|   354 | 1832 | `	return PH7_OK;` |
|   286 | 1833 | `}` |
|     - | 1834 |  |
