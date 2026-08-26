# src/ph7/vm_json.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 491/631 lines (77.81%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|    - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    5 | ` */` |
|    - |    6 | `#include "ph7int.h"` |
|    - |    7 | `/*` |
|    - |    8 | ` * Section:` |
|    - |    9 | ` *  JSON encoding/decoding routines.` |
|    - |   10 | ` * Status:` |
|    - |   11 | ` *    Devel.` |
|    - |   12 | ` */` |
|    - |   13 | `/* Forward reference */` |
|    - |   14 | `static int VmJsonArrayEncode(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|    - |   15 | `static int VmJsonObjectEncode(const char *zAttr,ph7_value *pValue,void *pUserData);` |
|    - |   16 | `/*` |
|    - |   17 | ` * JSON encoder state is stored in an instance` |
|    - |   18 | ` * of the following structure.` |
|    - |   19 | ` */` |
|    - |   20 | `typedef struct json_private_data json_private_data;` |
|    - |   21 | `struct json_private_data` |
|    - |   22 | `{` |
|    - |   23 | `	ph7_context *pCtx; /* Call context */` |
|    - |   24 | `	int isFirst;       /* True if first encoded entry */` |
|    - |   25 | `	int isObject;      /* True if the current array level is encoded as a JSON object */` |
|    - |   26 | `	int iFlags;        /* JSON encoding flags */` |
|    - |   27 | `	int nRecCount;     /* Recursion count */` |
|    - |   28 | `	int exc;           /* True if a jsonSerialize() callback threw an exception */` |
|    - |   29 | `	int oom;           /* True if a result append ran out of memory (raises a fatal) */` |
|    - |   30 | `	int fail;          /* True if the value is unencodable (php 8.1: a non-backed` |
|    - |   31 | `	                    * enum case) — json_encode returns FALSE */` |
|    - |   32 | `};` |
|    - |   33 | `/*` |
|    - |   34 | ` * Emit into the JSON result, flagging OOM on the shared data and bailing out` |
|    - |   35 | ` * of the current encode function (which returns PH7_OK; the top-level` |
|    - |   36 | ` * vm_builtin_json_encode checks ->oom and raises a non-catchable fatal). Used` |
|    - |   37 | ` * for every ph7_result_string/ph7_result_string_format append below.` |
|    - |   38 | ` */` |
|    - |   39 | `#define JSON_EMIT(pD, call) do { if( (call) != SXRET_OK ){ (pD)->oom = 1; return PH7_OK; } } while(0)` |
|    - |   40 | `/*` |
|    - |   41 | ` * Emit a float in php's json shape: PH7_AppendShortestReal (the shared` |
|    - |   42 | ` * serialize/var_export shortest-round-trip formatter, php's` |
|    - |   43 | ` * serialize_precision=-1) with the exponent marker lowercased (json prints` |
|    - |   44 | ` * 1.0e+17 where serialize prints 1.0E+17).` |
|    - |   45 | ` */` |
|   20 |   46 | `static sxi32 VmJsonEmitReal(ph7_context *pCtx,double rVal)` |
|    1 |   47 | `{` |
|    - |   48 | `	SyBlob sNum;` |
|    - |   49 | `	char *z;` |
|    - |   50 | `	sxu32 i,n;` |
|    - |   51 | `	sxi32 rc;` |
|   21 |   52 | `	SyBlobInit(&sNum,&pCtx->pVm->sAllocator);` |
|   21 |   53 | `	PH7_AppendShortestReal(&sNum,rVal);` |
|   21 |   54 | `	z = (char *)SyBlobData(&sNum);` |
|   21 |   55 | `	n = SyBlobLength(&sNum);` |
|   21 |   56 | `	if( z == 0 \|\| n < 1 ){` |
|  ! 0 |   57 | `		SyBlobRelease(&sNum);` |
|  ! 0 |   58 | `		return SXERR_MEM; /* treated as OOM by JSON_EMIT */` |
|    - |   59 | `	}` |
|  217 |   60 | `	for( i = 0 ; i < n ; i++ ){` |
|  197 |   61 | `		if( z[i] == 'E' ){` |
|    7 |   62 | `			z[i] = 'e';` |
|    3 |   63 | `		}` |
|   99 |   64 | `	}` |
|   21 |   65 | `	rc = ph7_result_string(pCtx,(const char *)z,(int)n);` |
|   21 |   66 | `	SyBlobRelease(&sNum);` |
|   21 |   67 | `	return rc;` |
|   11 |   68 | `}` |
|    - |   69 | `/*` |
|    - |   70 | ` * Returns the JSON representation of a value.In other word perform a JSON encoding operation.` |
|    - |   71 | ` * According to wikipedia` |
|    - |   72 | ` * JSON's basic types are:` |
|    - |   73 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|    - |   74 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|    - |   75 | ` *   Boolean (true or false)` |
|    - |   76 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|    - |   77 | ` *    do not need to be of the same type)` |
|    - |   78 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|    - |   79 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|    - |   80 | ` *     be distinct from each other)` |
|    - |   81 | ` *   null (empty)` |
|    - |   82 | ` * Non-significant white space may be added freely around the "structural characters"` |
|    - |   83 | ` * (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|    - |   84 | ` */` |
|  846 |   85 | `static sxi32 VmJsonEncode(` |
|    - |   86 | `	ph7_value *pIn,          /* Encode this value */` |
|    - |   87 | `	json_private_data *pData /* Context data */` |
|    2 |   88 | `	){` |
|  848 |   89 | `		ph7_context *pCtx = pData->pCtx;` |
|  848 |   90 | `		int iFlags = pData->iFlags;` |
|    - |   91 | `		int nByte;` |
|  848 |   92 | `		if( ph7_value_is_null(pIn) \|\| ph7_value_is_resource(pIn)){` |
|    - |   93 | `			/* null */` |
|    5 |   94 | `			JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));` |
|  845 |   95 | `		}else if( ph7_value_is_bool(pIn) ){` |
|   15 |   96 | `			int iBool = ph7_value_to_bool(pIn);` |
|    - |   97 | `			int iLen;` |
|    - |   98 | `			/* true/false */` |
|   15 |   99 | `			iLen = iBool ? (int)sizeof("true") : (int)sizeof("false");` |
|   15 |  100 | `			JSON_EMIT(pData,ph7_result_string(pCtx,iBool ? "true" : "false",iLen-1));` |
|  836 |  101 | `		}else if(  ph7_value_is_numeric(pIn) && !ph7_value_is_string(pIn) ){` |
|  430 |  102 | `			if( ph7_value_is_float(pIn) ){` |
|    - |  103 | `				/* php's json float output follows serialize_precision` |
|    - |  104 | `				 * (shortest round-trip, like serialize/var_export), NOT the` |
|    - |  105 | `				 * echo/cast precision of 14 — with a lowercase exponent` |
|    - |  106 | `				 * marker: 1/3 -> 0.3333333333333333, 1e17 -> 1.0e+17,` |
|    - |  107 | `				 * 1.0 -> 1, -0.0 -> -0. */` |
|   17 |  108 | `				JSON_EMIT(pData,VmJsonEmitReal(pCtx,ph7_value_to_double(pIn)));` |
|    9 |  109 | `			}else{` |
|    - |  110 | `				const char *zNum;` |
|    - |  111 | `				/* Get a string representation of the number */` |
|  271 |  112 | `				zNum = ph7_value_to_string(pIn,&nByte);` |
|  271 |  113 | `				JSON_EMIT(pData,ph7_result_string(pCtx,zNum,nByte));` |
|    1 |  114 | `			}` |
|  687 |  115 | `		}else if( ph7_value_is_string(pIn) ){` |
|  260 |  116 | `			if( (iFlags & JSON_NUMERIC_CHECK) &&  ph7_value_is_numeric(pIn) ){` |
|    - |  117 | `				/* Encodes numeric strings as numbers (same float shapes). */` |
|    5 |  118 | `				PH7_MemObjToReal(pIn); /* Force a numeric cast */` |
|    5 |  119 | `				JSON_EMIT(pData,VmJsonEmitReal(pCtx,ph7_value_to_double(pIn)));` |
|    3 |  120 | `			}else{` |
|    - |  121 | `				const char *zIn,*zEnd;` |
|    - |  122 | `				int c;` |
|    - |  123 | `				/* Encode the string */` |
|  256 |  124 | `				zIn = ph7_value_to_string(pIn,&nByte);` |
|  256 |  125 | `				zEnd = &zIn[nByte];` |
|    - |  126 | `				/* Append the double quote */` |
|  256 |  127 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"\"",(int)sizeof(char)));` |
|  479 |  128 | `				for(;;){` |
|  966 |  129 | `					if( zIn >= zEnd ){` |
|    - |  130 | `						/* No more input to process */` |
|  256 |  131 | `						break;` |
|    - |  132 | `					}` |
|  712 |  133 | `					c = zIn[0];` |
|    - |  134 | `					/* Advance the stream cursor */` |
|  712 |  135 | `					zIn++;` |
|  712 |  136 | `					if( (c == '<' \|\| c == '>') && (iFlags & JSON_HEX_TAG) ){` |
|    - |  137 | `						/* All < and > are converted to \u003C and \u003E */` |
|  ! 0 |  138 | `						if( c == '<' ){` |
|  ! 0 |  139 | `							JSON_EMIT(pData,ph7_result_string(pCtx,"\\u003C",(int)sizeof("\\u003C")-1));` |
|  ! 0 |  140 | `						}else{` |
|  ! 0 |  141 | `							JSON_EMIT(pData,ph7_result_string(pCtx,"\\u003E",(int)sizeof("\\u003E")-1));` |
|    - |  142 | `						}` |
|  ! 0 |  143 | `						continue;` |
|  712 |  144 | `					}else if( c == '&' && (iFlags & JSON_HEX_AMP) ){` |
|    - |  145 | `						/* All &s are converted to \u0026.  */` |
|  ! 0 |  146 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\u0026",(int)sizeof("\\u0026")-1));` |
|  ! 0 |  147 | `						continue;` |
|  712 |  148 | `					}else if( c == '\'' && (iFlags & JSON_HEX_APOS) ){` |
|    - |  149 | `						/* All ' are converted to \u0027.   */` |
|  ! 0 |  150 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\u0027",(int)sizeof("\\u0027")-1));` |
|  ! 0 |  151 | `						continue;` |
|  712 |  152 | `					}else if( c == '"' && (iFlags & JSON_HEX_QUOT) ){` |
|    - |  153 | `						/* All " are converted to \u0022. */` |
|  ! 0 |  154 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\u0022",(int)sizeof("\\u0022")-1));` |
|  ! 0 |  155 | `						continue;` |
|    - |  156 | `					}` |
|  712 |  157 | `					if( c == '"' \|\| c == '\\' ){` |
|    - |  158 | `						/* Escape the quote/backslash (php escapes the backslash` |
|    - |  159 | `						 * unconditionally — the old code wrongly tied it to` |
|    - |  160 | `						 * JSON_UNESCAPED_SLASHES, which governs '/' below) */` |
|    3 |  161 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\",(int)sizeof(char)));` |
|  710 |  162 | `					}else if( c == '/' && (iFlags & JSON_UNESCAPED_SLASHES) == 0 ){` |
|    - |  163 | `						/* php escapes forward slashes by default */` |
|    7 |  164 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\",(int)sizeof(char)));` |
|  706 |  165 | `					}else if( (unsigned char)c < 0x20 ){` |
|    - |  166 | `						/* Control characters (band A #4): php emits the short` |
|    - |  167 | `						 * escapes for \b \f \n \r \t and \u00xx for the rest —` |
|    - |  168 | `						 * pre-fix these were emitted RAW (invalid JSON). */` |
|    - |  169 | `						static const char zHex[] = "0123456789abcdef";` |
|    7 |  170 | `						char zEsc[6] = { '\\', 'u', '0', '0', 0, 0 };` |
|    7 |  171 | `						switch(c){` |
|  ! 0 |  172 | `						case '\b': JSON_EMIT(pData,ph7_result_string(pCtx,"\\b",2)); break;` |
|  ! 0 |  173 | `						case '\f': JSON_EMIT(pData,ph7_result_string(pCtx,"\\f",2)); break;` |
|    3 |  174 | `						case '\n': JSON_EMIT(pData,ph7_result_string(pCtx,"\\n",2)); break;` |
|  ! 0 |  175 | `						case '\r': JSON_EMIT(pData,ph7_result_string(pCtx,"\\r",2)); break;` |
|    3 |  176 | `						case '\t': JSON_EMIT(pData,ph7_result_string(pCtx,"\\t",2)); break;` |
|    1 |  177 | `						default:` |
|    3 |  178 | `							zEsc[4] = zHex[(c >> 4) & 0x0F];` |
|    3 |  179 | `							zEsc[5] = zHex[c & 0x0F];` |
|    3 |  180 | `							JSON_EMIT(pData,ph7_result_string(pCtx,zEsc,6));` |
|    2 |  181 | `							break;` |
|    - |  182 | `						}` |
|    7 |  183 | `						continue;` |
|    - |  184 | `					}` |
|    - |  185 | `					/* Append character verbatim */` |
|  706 |  186 | `					JSON_EMIT(pData,ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char)));` |
|    2 |  187 | `				}` |
|    - |  188 | `				/* Append the double quote */` |
|  256 |  189 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"\"",(int)sizeof(char)));` |
|    2 |  190 | `			}` |
|  415 |  191 | `		}else if( ph7_value_is_array(pIn) ){` |
|    - |  192 | `			/* An array encodes as a JSON array iff it is a "list" [consecutive` |
|    - |  193 | `			 * 0-based int keys]; otherwise [or under JSON_FORCE_OBJECT] as an` |
|    - |  194 | `			 * object with stringified keys (PHP semantics). */` |
|  440 |  195 | `			int isObject = (iFlags & JSON_FORCE_OBJECT)` |
|  220 |  196 | `				\|\| !PH7_HashmapIsList((ph7_hashmap *)pIn->x.pOther);` |
|  221 |  197 | `			int savedObject = pData->isObject; /* restore for sibling entries after recursion */` |
|  221 |  198 | `			int c = isObject ? '{' : '[';` |
|  221 |  199 | `			int d = isObject ? '}' : ']';` |
|    - |  200 | `			/* Encode the array */` |
|  221 |  201 | `			pData->isObject = isObject;` |
|  221 |  202 | `			pData->isFirst = 1;` |
|    - |  203 | `			/* Append the square bracket or curly braces */` |
|  221 |  204 | `			JSON_EMIT(pData,ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char)));` |
|    - |  205 | `			/* Iterate throw array entries */` |
|  221 |  206 | `			ph7_array_walk(pIn,VmJsonArrayEncode,pData);` |
|    - |  207 | `			/* Bail if a nested append ran out of memory before the closer */` |
|  221 |  208 | `			if( pData->oom ){` |
|  ! 0 |  209 | `				return PH7_OK;` |
|    - |  210 | `			}` |
|    - |  211 | `			/* Append the closing square bracket or curly braces */` |
|  221 |  212 | `			JSON_EMIT(pData,ph7_result_string(pCtx,(const char *)&d,(int)sizeof(char)));` |
|  221 |  213 | `			pData->isObject = savedObject;` |
|  176 |  214 | `		}else if( ph7_value_is_object(pIn) ){` |
|   66 |  215 | `			ph7_class_instance *pThis = (ph7_class_instance *)pIn->x.pOther;` |
|   66 |  216 | `			ph7_vm *pVm = pIn->pVm;` |
|   66 |  217 | `			ph7_class_method *pMethod = 0;` |
|    - |  218 | `			/* If the object implements JsonSerializable, encode the value` |
|    - |  219 | `			 * returned by jsonSerialize() instead of its public properties.` |
|    - |  220 | `			 * An enum implementing it explicitly also takes this path (php). */` |
|   64 |  221 | `			if( pVm->pJsonSerializableClass` |
|   66 |  222 | `				&& PH7_VmInstanceOf(pThis->pClass,pVm->pJsonSerializableClass) ){` |
|   15 |  223 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"jsonSerialize",sizeof("jsonSerialize")-1);` |
|    7 |  224 | `			}` |
|   66 |  225 | `			if( pMethod == 0 && (pThis->pClass->iFlags & PH7_CLASS_ENUM) != 0 ){` |
|    - |  226 | `				/* php 8.1: a BACKED enum case encodes as its backing value; a` |
|    - |  227 | `				 * pure enum case has no default serialization — json_encode` |
|    - |  228 | `				 * returns false. */` |
|    9 |  229 | `				ph7_value *pBacking = PH7_EnumCaseBackingValueOf(pThis);` |
|    9 |  230 | `				if( pBacking ){` |
|    7 |  231 | `					pData->nRecCount++;` |
|    7 |  232 | `					VmJsonEncode(pBacking,pData);` |
|    7 |  233 | `					pData->nRecCount--;` |
|    4 |  234 | `				}else{` |
|    3 |  235 | `					pData->fail = 1;` |
|    - |  236 | `				}` |
|    9 |  237 | `				return PH7_OK;` |
|    - |  238 | `			}` |
|   58 |  239 | `			if( pMethod ){` |
|    - |  240 | `				ph7_value sResult;` |
|    - |  241 | `				sxi32 rc;` |
|   15 |  242 | `				PH7_MemObjInit(pVm,&sResult);` |
|   15 |  243 | `				rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|   15 |  244 | `				if( rc == PH7_EXCEPTION ){` |
|    - |  245 | `					/* Let jsonSerialize()'s throw propagate */` |
|    5 |  246 | `					PH7_MemObjRelease(&sResult);` |
|    5 |  247 | `					pData->exc = 1;` |
|    5 |  248 | `					return PH7_EXCEPTION;` |
|    - |  249 | `				}` |
|    - |  250 | `				/* Encode the returned value [scalar/array/object] */` |
|   11 |  251 | `				pData->nRecCount++;` |
|   11 |  252 | `				VmJsonEncode(&sResult,pData);` |
|   11 |  253 | `				pData->nRecCount--;` |
|   11 |  254 | `				PH7_MemObjRelease(&sResult);` |
|   11 |  255 | `				if( pData->exc ){` |
|  ! 0 |  256 | `					return PH7_EXCEPTION;` |
|    - |  257 | `				}` |
|   11 |  258 | `				if( pData->oom ){` |
|  ! 0 |  259 | `					return PH7_OK;` |
|    - |  260 | `				}` |
|    6 |  261 | `			}else{` |
|    - |  262 | `				SyHashEntry *pAttrEntry;` |
|    - |  263 | `				SySet sNames;` |
|    - |  264 | `				SyString *aName;` |
|    - |  265 | `				sxu32 iName,nName;` |
|    - |  266 | `				/* Encode the class instance: php serializes only PUBLIC` |
|    - |  267 | `				 * non-static properties, reading through a PHP 8.4 get hook` |
|    - |  268 | `				 * when one is declared (virtual properties included). The` |
|    - |  269 | `				 * names are SNAPSHOTTED first — a hook dispatched mid-walk may` |
|    - |  270 | `				 * re-enter an hAttr walk on this instance (the hash has a` |
|    - |  271 | `				 * single embedded loop cursor) or unset()/create properties;` |
|    - |  272 | `				 * names point into class-owned attr storage and each is` |
|    - |  273 | `				 * re-looked-up before use. */` |
|   44 |  274 | `				pData->isFirst = 1;` |
|    - |  275 | `				/* Append the curly braces */` |
|   44 |  276 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"{",(int)sizeof(char)));` |
|   44 |  277 | `				SySetInit(&sNames,&pVm->sAllocator,sizeof(SyString));` |
|   44 |  278 | `				SyHashResetLoopCursor(&pThis->hAttr);` |
|  114 |  279 | `				while( (pAttrEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|   72 |  280 | `					VmClassAttr *pVmAttr = (VmClassAttr *)pAttrEntry->pUserData;` |
|   70 |  281 | `					if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT))` |
|   72 |  282 | `					 \|\| pVmAttr->pAttr->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|    3 |  283 | `						continue;` |
|    - |  284 | `					}` |
|   70 |  285 | `					SySetPut(&sNames,(const void *)&pVmAttr->pAttr->sName);` |
|    2 |  286 | `				}` |
|   44 |  287 | `				aName = (SyString *)SySetBasePtr(&sNames);` |
|   44 |  288 | `				nName = SySetUsed(&sNames);` |
|  112 |  289 | `				for( iName = 0 ; iName < nName ; ++iName ){` |
|    - |  290 | `					VmClassAttr *pVmAttr;` |
|   70 |  291 | `					ph7_value *pAttrVal = 0;` |
|    - |  292 | `					ph7_value sHookVal;` |
|    - |  293 | `					sxi32 rcHk;` |
|   70 |  294 | `					pAttrEntry = SyHashGet(&pThis->hAttr,(const void *)aName[iName].zString,aName[iName].nByte);` |
|   70 |  295 | `					if( pAttrEntry == 0 ){` |
|  ! 0 |  296 | `						continue; /* unset by an earlier hook */` |
|    - |  297 | `					}` |
|   70 |  298 | `					pVmAttr = (VmClassAttr *)pAttrEntry->pUserData;` |
|   70 |  299 | `					PH7_MemObjInit(pVm,&sHookVal);` |
|   70 |  300 | `					rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|   70 |  301 | `					if( rcHk == SXRET_OK ){` |
|    5 |  302 | `						pAttrVal = &sHookVal;` |
|   68 |  303 | `					}else if( rcHk == SXERR_NOTFOUND ){` |
|    - |  304 | `						/* Encode a COPY: the encoder casts scalars in place` |
|    - |  305 | `						 * (ph7_value_to_string), which must not corrupt the` |
|    - |  306 | `						 * live attribute slot. */` |
|   66 |  307 | `						ph7_value *pRaw = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|   66 |  308 | `						if( pRaw ){` |
|   66 |  309 | `							PH7_MemObjStore(pRaw,&sHookVal);` |
|   66 |  310 | `							pAttrVal = &sHookVal;` |
|   32 |  311 | `						}` |
|   34 |  312 | `					}else{` |
|    - |  313 | `						/* the get hook threw — propagate like jsonSerialize() */` |
|  ! 0 |  314 | `						PH7_MemObjRelease(&sHookVal);` |
|  ! 0 |  315 | `						SySetRelease(&sNames);` |
|  ! 0 |  316 | `						pData->exc = 1;` |
|  ! 0 |  317 | `						return PH7_EXCEPTION;` |
|    - |  318 | `					}` |
|   70 |  319 | `					if( pAttrVal ){` |
|   70 |  320 | `						VmJsonObjectEncode(SyStringData(&pVmAttr->pAttr->sName),pAttrVal,pData);` |
|   34 |  321 | `					}` |
|   70 |  322 | `					PH7_MemObjRelease(&sHookVal);` |
|   70 |  323 | `					if( pData->exc ){` |
|  ! 0 |  324 | `						SySetRelease(&sNames);` |
|  ! 0 |  325 | `						return PH7_EXCEPTION; /* a nested jsonSerialize()/hook threw */` |
|    - |  326 | `					}` |
|   70 |  327 | `					if( pData->oom ){` |
|  ! 0 |  328 | `						SySetRelease(&sNames);` |
|  ! 0 |  329 | `						return PH7_OK;` |
|    - |  330 | `					}` |
|   36 |  331 | `				}` |
|   44 |  332 | `				SySetRelease(&sNames);` |
|    - |  333 | `				/* Append the closing curly braces  */` |
|   44 |  334 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"}",(int)sizeof(char)));` |
|    - |  335 | `			}` |
|   28 |  336 | `		}else{` |
|    - |  337 | `			/* Can't happen */` |
|  ! 0 |  338 | `			JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));` |
|    - |  339 | `		}` |
|    - |  340 | `		/* All done */` |
|  836 |  341 | `		return PH7_OK;` |
|  425 |  342 | `}` |
|    - |  343 | `/*` |
|    - |  344 | ` * The following walker callback is invoked each time we need` |
|    - |  345 | ` * to encode an array to JSON.` |
|    - |  346 | ` */` |
|  506 |  347 | `static int VmJsonArrayEncode(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|    1 |  348 | `{` |
|  507 |  349 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|  507 |  350 | `	if( pJson->nRecCount > 31 \|\| pJson->exc \|\| pJson->oom ){` |
|    - |  351 | `		/* Recursion limit reached, a callback threw, or OOM — return immediately */` |
|  ! 0 |  352 | `		return PH7_OK;` |
|    - |  353 | `	}` |
|  507 |  354 | `	if( !pJson->isFirst ){` |
|    - |  355 | `		/* Append the colon first */` |
|  311 |  356 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,",",(int)sizeof(char)));` |
|  155 |  357 | `	}` |
|  507 |  358 | `	if( pJson->isObject ){` |
|    - |  359 | `		/* Outputs an object rather than an array */` |
|    - |  360 | `		const char *zKey;` |
|    - |  361 | `		int nByte;` |
|    - |  362 | `		/* Extract a string representation of the key */` |
|  265 |  363 | `		zKey = ph7_value_to_string(pKey,&nByte);` |
|    - |  364 | `		/* Append the quoted key and the colon (checked, so an OOM here is caught` |
|    - |  365 | `		 * rather than silently truncating; matches the prior "%.*s" emit byte for` |
|    - |  366 | `		 * byte — keys are not JSON-escaped, a pre-existing behavior). */` |
|  265 |  367 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,"\"",(int)sizeof(char)));` |
|  265 |  368 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,zKey,nByte));` |
|  265 |  369 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,"\":",(int)sizeof("\":")-1));` |
|  132 |  370 | `	}` |
|    - |  371 | `	/* Encode the value */` |
|  507 |  372 | `	pJson->nRecCount++;` |
|  507 |  373 | `	VmJsonEncode(pValue,pJson);` |
|  507 |  374 | `	pJson->nRecCount--;` |
|  507 |  375 | `	pJson->isFirst = 0;` |
|  507 |  376 | `	return PH7_OK;` |
|  254 |  377 | `}` |
|    - |  378 | `/*` |
|    - |  379 | ` * The following walker callback is invoked each time we need to encode` |
|    - |  380 | ` * a class instance [i.e: Object in the PHP jargon] to JSON.` |
|    - |  381 | ` */` |
|   68 |  382 | `static int VmJsonObjectEncode(const char *zAttr,ph7_value *pValue,void *pUserData)` |
|    2 |  383 | `{` |
|   70 |  384 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|   70 |  385 | `	if( pJson->nRecCount > 31 \|\| pJson->exc \|\| pJson->oom ){` |
|    - |  386 | `		/* Recursion limit reached, a callback threw, or OOM — return immediately */` |
|  ! 0 |  387 | `		return PH7_OK;` |
|    - |  388 | `	}` |
|   70 |  389 | `	if( !pJson->isFirst ){` |
|    - |  390 | `		/* Append the colon first */` |
|   29 |  391 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,",",(int)sizeof(char)));` |
|   14 |  392 | `	}` |
|    - |  393 | `	/* Append the quoted attribute name and the colon (checked; matches the prior` |
|    - |  394 | `	 * "%s" emit byte for byte — attribute names are not JSON-escaped). */` |
|   70 |  395 | `	JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,"\"",(int)sizeof(char)));` |
|   70 |  396 | `	JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,zAttr,-1));` |
|   70 |  397 | `	JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,"\":",(int)sizeof("\":")-1));` |
|    - |  398 | `	/* Encode the value */` |
|   70 |  399 | `	pJson->nRecCount++;` |
|   70 |  400 | `	VmJsonEncode(pValue,pJson);` |
|   70 |  401 | `	pJson->nRecCount--;` |
|   70 |  402 | `	pJson->isFirst = 0;` |
|   70 |  403 | `	return PH7_OK;` |
|   36 |  404 | `}` |
|    - |  405 | `/*` |
|    - |  406 | ` * string json_encode(mixed $value [, int $options = 0 ])` |
|    - |  407 | ` *  Returns a string containing the JSON representation of value.` |
|    - |  408 | ` * Parameters` |
|    - |  409 | ` *  $value` |
|    - |  410 | ` *  The value being encoded. Can be any type except a resource.` |
|    - |  411 | ` * $options` |
|    - |  412 | ` *  Bitmask consisting of:` |
|    - |  413 | ` *  JSON_HEX_TAG   All < and > are converted to \u003C and \u003E.` |
|    - |  414 | ` *  JSON_HEX_AMP   All &s are converted to \u0026.` |
|    - |  415 | ` *  JSON_HEX_APOS  All ' are converted to \u0027.` |
|    - |  416 | ` *  JSON_HEX_QUOT  All " are converted to \u0022.` |
|    - |  417 | ` *  JSON_FORCE_OBJECT  Outputs an object rather than an array.` |
|    - |  418 | ` *  JSON_NUMERIC_CHECK Encodes numeric strings as numbers.` |
|    - |  419 | ` *  JSON_BIGINT_AS_STRING   Not used` |
|    - |  420 | ` *  JSON_PRETTY_PRINT       Use whitespace in returned data to format it.` |
|    - |  421 | ` *  JSON_UNESCAPED_SLASHES  Don't escape '/'` |
|    - |  422 | ` *  JSON_UNESCAPED_UNICODE  Not used.` |
|    - |  423 | ` * Return` |
|    - |  424 | ` *  Returns a JSON encoded string on success. FALSE otherwise` |
|    - |  425 | ` */` |
|  256 |  426 | `PH7_PRIVATE int vm_builtin_json_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  427 | `{` |
|    - |  428 | `	json_private_data sJson;` |
|    - |  429 | `	sxi32 rc;` |
|  258 |  430 | `	if( nArg < 1 ){` |
|    - |  431 | `		/* Missing arguments,return FALSE */` |
|  ! 0 |  432 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  433 | `		return PH7_OK;` |
|    - |  434 | `	}` |
|    - |  435 | `	/* Prepare the JSON data */` |
|  258 |  436 | `	sJson.nRecCount = 0;` |
|  258 |  437 | `	sJson.pCtx = pCtx;` |
|  258 |  438 | `	sJson.isFirst = 1;` |
|  258 |  439 | `	sJson.iFlags = 0;` |
|  258 |  440 | `	sJson.exc = 0;` |
|  258 |  441 | `	sJson.oom = 0;` |
|  258 |  442 | `	sJson.fail = 0;` |
|  258 |  443 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|    - |  444 | `		/* Extract option flags */` |
|    7 |  445 | `		sJson.iFlags = ph7_value_to_int(apArg[1]);` |
|    3 |  446 | `	}` |
|  258 |  447 | `	pCtx->pVm->json_rc = JSON_ERROR_NONE;` |
|    - |  448 | `	/* Perform the encoding operation */` |
|  258 |  449 | `	rc = VmJsonEncode(apArg[0],&sJson);` |
|  258 |  450 | `	if( sJson.oom ){` |
|    - |  451 | `		/* A result append ran out of memory: raise a non-catchable fatal,` |
|    - |  452 | `		 * distinct from a JSON-encoding error (json_last_error untouched). */` |
|  ! 0 |  453 | `		return PH7_ContextMemoryError(pCtx);` |
|    - |  454 | `	}` |
|  258 |  455 | `	if( rc == PH7_EXCEPTION \|\| sJson.exc ){` |
|    - |  456 | `		/* A jsonSerialize() callback threw — propagate so the exception unwinds */` |
|    5 |  457 | `		return PH7_EXCEPTION;` |
|    - |  458 | `	}` |
|  254 |  459 | `	if( sJson.fail ){` |
|    - |  460 | `		/* Unencodable value (php 8.1: non-backed enum case): the whole encode` |
|    - |  461 | `		 * fails — discard whatever was emitted and return FALSE. */` |
|    3 |  462 | `		pCtx->pVm->json_rc = JSON_ERROR_NON_BACKED_ENUM;` |
|    3 |  463 | `		ph7_result_bool(pCtx,0);` |
|    3 |  464 | `		return PH7_OK;` |
|    - |  465 | `	}` |
|    - |  466 | `	/* All done */` |
|  252 |  467 | `	return PH7_OK;` |
|  130 |  468 | `}` |
|    - |  469 | `#undef JSON_EMIT` |
|    - |  470 | `/*` |
|    - |  471 | ` * int json_last_error(void)` |
|    - |  472 | ` *  Returns the last error (if any) occurred during the last JSON encoding/decoding.` |
|    - |  473 | ` * Parameters` |
|    - |  474 | ` *  None` |
|    - |  475 | ` * Return` |
|    - |  476 | ` *  Returns an integer, the value can be one of the following constants:` |
|    - |  477 | ` *  JSON_ERROR_NONE            No error has occurred.` |
|    - |  478 | ` *  JSON_ERROR_DEPTH           The maximum stack depth has been exceeded.` |
|    - |  479 | ` *  JSON_ERROR_STATE_MISMATCH  Invalid or malformed JSON.` |
|    - |  480 | ` *  JSON_ERROR_CTRL_CHAR  	   Control character error, possibly incorrectly encoded.` |
|    - |  481 | ` *  JSON_ERROR_SYNTAX          Syntax error.` |
|    - |  482 | ` *  JSON_ERROR_UTF8_CHECK      Malformed UTF-8 characters.` |
|    - |  483 | ` */` |
|   12 |  484 | `PH7_PRIVATE int vm_builtin_json_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  485 | `{` |
|   14 |  486 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  487 | `	/* Return the error code */` |
|   14 |  488 | `	ph7_result_int(pCtx,pVm->json_rc);` |
|    6 |  489 | `	SXUNUSED(nArg); /* cc warning */` |
|    6 |  490 | `	SXUNUSED(apArg);` |
|   14 |  491 | `	return PH7_OK;` |
|    2 |  492 | `}` |
|    - |  493 | `/*` |
|    - |  494 | ` * string json_last_error_msg(void)` |
|    - |  495 | ` *  Returns the error string of the last JSON encoding/decoding operation.` |
|    - |  496 | ` * Parameters` |
|    - |  497 | ` *  None` |
|    - |  498 | ` * Return` |
|    - |  499 | ` *  Returns the human-readable message corresponding to the last json_last_error()` |
|    - |  500 | ` *  code, or "No error" if no error has occurred.` |
|    - |  501 | ` */` |
|    4 |  502 | `PH7_PRIVATE int vm_builtin_json_last_error_msg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  503 | `{` |
|    5 |  504 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  505 | `	const char *zMsg;` |
|    5 |  506 | `	switch( pVm->json_rc ){` |
|    1 |  507 | `	case JSON_ERROR_NONE:` |
|    3 |  508 | `		zMsg = "No error";` |
|    3 |  509 | `		break;` |
|  ! 0 |  510 | `	case JSON_ERROR_DEPTH:` |
|  ! 0 |  511 | `		zMsg = "Maximum stack depth exceeded";` |
|  ! 0 |  512 | `		break;` |
|  ! 0 |  513 | `	case JSON_ERROR_STATE_MISMATCH:` |
|  ! 0 |  514 | `		zMsg = "State mismatch (invalid or malformed JSON)";` |
|  ! 0 |  515 | `		break;` |
|  ! 0 |  516 | `	case JSON_ERROR_CTRL_CHAR:` |
|  ! 0 |  517 | `		zMsg = "Control character error, possibly incorrectly encoded";` |
|  ! 0 |  518 | `		break;` |
|    1 |  519 | `	case JSON_ERROR_SYNTAX:` |
|    3 |  520 | `		zMsg = "Syntax error";` |
|    3 |  521 | `		break;` |
|  ! 0 |  522 | `	case JSON_ERROR_UTF8:` |
|  ! 0 |  523 | `		zMsg = "Malformed UTF-8 characters, possibly incorrectly encoded";` |
|  ! 0 |  524 | `		break;` |
|  ! 0 |  525 | `	case JSON_ERROR_NON_BACKED_ENUM:` |
|  ! 0 |  526 | `		zMsg = "Non-backed enums have no default serialization";` |
|  ! 0 |  527 | `		break;` |
|  ! 0 |  528 | `	default:` |
|  ! 0 |  529 | `		zMsg = "Unknown error";` |
|  ! 0 |  530 | `		break;` |
|    - |  531 | `	}` |
|    5 |  532 | `	ph7_result_string(pCtx,zMsg,-1/* Compute length automatically */);` |
|    2 |  533 | `	SXUNUSED(nArg); /* cc warning */` |
|    2 |  534 | `	SXUNUSED(apArg);` |
|    5 |  535 | `	return PH7_OK;` |
|    1 |  536 | `}` |
|    - |  537 | `/* Possible tokens from the JSON tokenization process */` |
|    - |  538 | `#define JSON_TK_TRUE    0x001 /* Boolean true */` |
|    - |  539 | `#define JSON_TK_FALSE   0x002 /* Boolean false */` |
|    - |  540 | `#define JSON_TK_STR     0x004 /* String enclosed in double quotes */` |
|    - |  541 | `#define JSON_TK_NULL    0x008 /* null */` |
|    - |  542 | `#define JSON_TK_NUM     0x010 /* Numeric */` |
|    - |  543 | `#define JSON_TK_OCB     0x020 /* Open curly braces '{' */` |
|    - |  544 | `#define JSON_TK_CCB     0x040 /* Closing curly braces '}' */` |
|    - |  545 | `#define JSON_TK_OSB     0x080 /* Open square bracke '[' */` |
|    - |  546 | `#define JSON_TK_CSB     0x100 /* Closing square bracket ']' */` |
|    - |  547 | `#define JSON_TK_COLON   0x200 /* Single colon ':' */` |
|    - |  548 | `#define JSON_TK_COMMA   0x400 /* Single comma ',' */` |
|    - |  549 | `#define JSON_TK_INVALID 0x800 /* Unexpected token */` |
|    - |  550 | `/*` |
|    - |  551 | ` * Tokenize an entire JSON input.` |
|    - |  552 | ` * Get a single low-level token from the input file.` |
|    - |  553 | ` * Update the stream pointer so that it points to the first` |
|    - |  554 | ` * character beyond the extracted token.` |
|    - |  555 | ` */` |
|  160 |  556 | `static sxi32 VmJsonTokenize(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|    2 |  557 | `{` |
|  162 |  558 | `	int *pJsonErr = (int *)pUserData;` |
|    - |  559 | `	SyString *pStr;` |
|    - |  560 | `	int c;` |
|    - |  561 | `	/* Ignore leading white spaces */` |
|  166 |  562 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|    - |  563 | `		/* Advance the stream cursor */` |
|    6 |  564 | `		if( pStream->zText[0] == '\n' ){` |
|    - |  565 | `			/* Update line counter */` |
|  ! 0 |  566 | `			pStream->nLine++;` |
|  ! 0 |  567 | `		}` |
|    6 |  568 | `		pStream->zText++;` |
|    2 |  569 | `	}` |
|  162 |  570 | `	if( pStream->zText >= pStream->zEnd ){` |
|    - |  571 | `		/* End of input reached */` |
|  ! 0 |  572 | `		SXUNUSED(pCtxData); /* cc warning */` |
|  ! 0 |  573 | `		return SXERR_EOF;` |
|    - |  574 | `	}` |
|    - |  575 | `	/* Record token starting position and line */` |
|  162 |  576 | `	pToken->nLine = pStream->nLine;` |
|  162 |  577 | `	pToken->pUserData = 0;` |
|  162 |  578 | `	pStr = &pToken->sData;` |
|  162 |  579 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
|  160 |  580 | `	if( pStream->zText[0] == '{' \|\| pStream->zText[0] == '[' \|\| pStream->zText[0] == '}' \|\| pStream->zText[0] == ']'` |
|  110 |  581 | `		\|\| pStream->zText[0] == ':' \|\| pStream->zText[0] == ',' ){` |
|    - |  582 | `			/* Single character */` |
|   94 |  583 | `			c = pStream->zText[0];` |
|    - |  584 | `			/* Set token type */` |
|   94 |  585 | `			switch(c){` |
|   13 |  586 | `			case '[': pToken->nType = JSON_TK_OSB;   break;` |
|   22 |  587 | `			case '{': pToken->nType = JSON_TK_OCB;   break;` |
|   16 |  588 | `			case '}': pToken->nType = JSON_TK_CCB;   break;` |
|   13 |  589 | `			case ']': pToken->nType = JSON_TK_CSB;   break;` |
|   20 |  590 | `			case ':': pToken->nType = JSON_TK_COLON; break;` |
|   17 |  591 | `			case ',': pToken->nType = JSON_TK_COMMA; break;` |
|  ! 0 |  592 | `			default:` |
|  ! 0 |  593 | `				break;` |
|    - |  594 | `			}` |
|    - |  595 | `			/* Advance the stream cursor */` |
|   94 |  596 | `			pStream->zText++;` |
|  116 |  597 | `	}else if( pStream->zText[0] == '"') {` |
|    - |  598 | `		/* JSON string */` |
|   26 |  599 | `		pStream->zText++;` |
|   26 |  600 | `		pStr->zString++;` |
|    - |  601 | `		/* Delimit the string */` |
|   72 |  602 | `		while( pStream->zText < pStream->zEnd ){` |
|   72 |  603 | `			if( pStream->zText[0] == '"' && pStream->zText[-1] != '\\' ){` |
|   26 |  604 | `				break;` |
|    - |  605 | `			}` |
|   48 |  606 | `			if( pStream->zText[0] == '\n' ){` |
|    - |  607 | `				/* Update line counter */` |
|  ! 0 |  608 | `				pStream->nLine++;` |
|  ! 0 |  609 | `			}` |
|   48 |  610 | `			pStream->zText++;` |
|    2 |  611 | `		}` |
|   26 |  612 | `		if( pStream->zText >= pStream->zEnd ){` |
|    - |  613 | `			/* Missing closing '"' */` |
|  ! 0 |  614 | `			pToken->nType = JSON_TK_INVALID;` |
|  ! 0 |  615 | `			*pJsonErr = JSON_ERROR_SYNTAX;` |
|  ! 0 |  616 | `		}else{` |
|   26 |  617 | `			pToken->nType = JSON_TK_STR;` |
|   26 |  618 | `			pStream->zText++; /* Jump the closing double quotes */` |
|    2 |  619 | `		}` |
|   58 |  620 | `	}else if( pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|    - |  621 | `		/* Number */` |
|   31 |  622 | `		pStream->zText++;` |
|   31 |  623 | `		pToken->nType = JSON_TK_NUM;` |
|   31 |  624 | `		while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  625 | `			pStream->zText++;` |
|  ! 0 |  626 | `		}` |
|   31 |  627 | `		if( pStream->zText < pStream->zEnd ){` |
|   31 |  628 | `			c = pStream->zText[0];` |
|   31 |  629 | `			if( c == '.' ){` |
|    - |  630 | `					/* Real number */` |
|  ! 0 |  631 | `					pStream->zText++;` |
|  ! 0 |  632 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  633 | `						pStream->zText++;` |
|  ! 0 |  634 | `					}` |
|  ! 0 |  635 | `					if( pStream->zText < pStream->zEnd ){` |
|  ! 0 |  636 | `						c = pStream->zText[0];` |
|  ! 0 |  637 | `						if( c=='e' \|\| c=='E' ){` |
|  ! 0 |  638 | `							pStream->zText++;` |
|  ! 0 |  639 | `							if( pStream->zText < pStream->zEnd ){` |
|  ! 0 |  640 | `								c = pStream->zText[0];` |
|  ! 0 |  641 | `								if( c =='+' \|\| c=='-' ){` |
|  ! 0 |  642 | `									pStream->zText++;` |
|  ! 0 |  643 | `								}` |
|  ! 0 |  644 | `								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  645 | `									pStream->zText++;` |
|  ! 0 |  646 | `								}` |
|  ! 0 |  647 | `							}` |
|  ! 0 |  648 | `						}` |
|  ! 0 |  649 | `					}` |
|   31 |  650 | `				}else if( c=='e' \|\| c=='E' ){` |
|    - |  651 | `					/* Real number */` |
|  ! 0 |  652 | `					pStream->zText++;` |
|  ! 0 |  653 | `					if( pStream->zText < pStream->zEnd ){` |
|  ! 0 |  654 | `						c = pStream->zText[0];` |
|  ! 0 |  655 | `						if( c =='+' \|\| c=='-' ){` |
|  ! 0 |  656 | `							pStream->zText++;` |
|  ! 0 |  657 | `						}` |
|  ! 0 |  658 | `						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  659 | `							pStream->zText++;` |
|  ! 0 |  660 | `						}` |
|  ! 0 |  661 | `					}` |
|  ! 0 |  662 | `				}` |
|   16 |  663 | `			}` |
|   37 |  664 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("true") -1 &&` |
|   12 |  665 | `		SyStrnicmp((const char *)pStream->zText,"true",sizeof("true")-1) == 0 ){` |
|    - |  666 | `			/* boolean true */` |
|  ! 0 |  667 | `			pToken->nType = JSON_TK_TRUE;` |
|    - |  668 | `			/* Advance the stream cursor */` |
|  ! 0 |  669 | `			pStream->zText += sizeof("true")-1;` |
|   22 |  670 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("false") -1 &&` |
|   12 |  671 | `		SyStrnicmp((const char *)pStream->zText,"false",sizeof("false")-1) == 0 ){` |
|    - |  672 | `			/* boolean false */` |
|  ! 0 |  673 | `			pToken->nType = JSON_TK_FALSE;` |
|    - |  674 | `			/* Advance the stream cursor */` |
|  ! 0 |  675 | `			pStream->zText += sizeof("false")-1;` |
|   22 |  676 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("null") -1 &&` |
|   12 |  677 | `		SyStrnicmp((const char *)pStream->zText,"null",sizeof("null")-1) == 0 ){` |
|    - |  678 | `			/* NULL */` |
|  ! 0 |  679 | `			pToken->nType = JSON_TK_NULL;` |
|    - |  680 | `			/* Advance the stream cursor */` |
|  ! 0 |  681 | `			pStream->zText += sizeof("null")-1;` |
|  ! 0 |  682 | `	}else{` |
|    - |  683 | `		/* Unexpected token */` |
|   16 |  684 | `		pToken->nType = JSON_TK_INVALID;` |
|    - |  685 | `		/* Advance the stream cursor */` |
|   16 |  686 | `		pStream->zText++;` |
|   16 |  687 | `		*pJsonErr = JSON_ERROR_SYNTAX;` |
|    - |  688 | `		/* Abort processing immediatley */` |
|   16 |  689 | `		return SXERR_ABORT;` |
|    - |  690 | `	}` |
|    - |  691 | `	/* record token length */` |
|  148 |  692 | `	pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  148 |  693 | `	if( pToken->nType == JSON_TK_STR ){` |
|   26 |  694 | `		pStr->nByte--;` |
|   12 |  695 | `	}` |
|    - |  696 | `	/* Return to the lexer */` |
|  148 |  697 | `	return SXRET_OK;` |
|   82 |  698 | `}` |
|    - |  699 | `/*` |
|    - |  700 | ` * JSON decoded input consumer callback signature.` |
|    - |  701 | ` */` |
|    - |  702 | `typedef int (*ProcJsonConsumer)(ph7_context *,ph7_value *,ph7_value *,void *);` |
|    - |  703 | `/*` |
|    - |  704 | ` * JSON decoder state is kept in the following structure.` |
|    - |  705 | ` */` |
|    - |  706 | `typedef struct json_decoder json_decoder;` |
|    - |  707 | `struct json_decoder` |
|    - |  708 | `{` |
|    - |  709 | `	ph7_context *pCtx; /* Call context */` |
|    - |  710 | `	ProcJsonConsumer xConsumer; /* Consumer callback */` |
|    - |  711 | `	void *pUserData;   /* Last argument to xConsumer() */` |
|    - |  712 | `	int iFlags;        /* Configuration flags */` |
|    - |  713 | `	SyToken *pIn;      /* Token stream */` |
|    - |  714 | `	SyToken *pEnd;     /* End of the token stream */` |
|    - |  715 | `	int rec_depth;     /* Recursion limit */` |
|    - |  716 | `	int rec_count;     /* Current nesting level */` |
|    - |  717 | `	int *pErr;         /* JSON decoding error if any */` |
|    - |  718 | `};` |
|    - |  719 | `#define JSON_DECODE_ASSOC 0x01 /* Decode a JSON object as an associative array */` |
|    - |  720 | `/* Forward declaration */` |
|    - |  721 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData);` |
|    - |  722 | `/*` |
|    - |  723 | ` * Dequote [i.e: Resolve all backslash escapes ] a JSON string and store` |
|    - |  724 | ` * the result in the given ph7_value.` |
|    - |  725 | ` */` |
|   24 |  726 | `static void VmJsonDequoteString(const SyString *pStr,ph7_value *pWorker)` |
|    2 |  727 | `{` |
|   26 |  728 | `	const char *zIn = pStr->zString;` |
|   26 |  729 | `	const char *zEnd = &pStr->zString[pStr->nByte];` |
|    - |  730 | `	const char *zCur;` |
|    - |  731 | `	int c;` |
|    - |  732 | `	/* Mark the value as a string */` |
|   26 |  733 | `	ph7_value_string(pWorker,"",0); /* Empty string */` |
|   12 |  734 | `	for(;;){` |
|   26 |  735 | `		zCur = zIn;` |
|   72 |  736 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|   48 |  737 | `			zIn++;` |
|    2 |  738 | `		}` |
|   26 |  739 | `		if( zIn > zCur ){` |
|    - |  740 | `			/* Append chunk verbatim */` |
|   26 |  741 | `			ph7_value_string(pWorker,zCur,(int)(zIn-zCur));` |
|   12 |  742 | `		}` |
|   26 |  743 | `		zIn++;` |
|   26 |  744 | `		if( zIn >= zEnd ){` |
|    - |  745 | `			/* End of the input reached */` |
|   26 |  746 | `			break;` |
|    - |  747 | `		}` |
|  ! 0 |  748 | `		c = zIn[0];` |
|    - |  749 | `		/* Unescape the character */` |
|  ! 0 |  750 | `		switch(c){` |
|  ! 0 |  751 | `		case '"':  ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|  ! 0 |  752 | `		case '\\': ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|  ! 0 |  753 | `		case 'n':  ph7_value_string(pWorker,"\n",(int)sizeof(char)); break;` |
|  ! 0 |  754 | `		case 'r':  ph7_value_string(pWorker,"\r",(int)sizeof(char)); break;` |
|  ! 0 |  755 | `		case 't':  ph7_value_string(pWorker,"\t",(int)sizeof(char)); break;` |
|  ! 0 |  756 | `		case 'f':  ph7_value_string(pWorker,"\f",(int)sizeof(char)); break;` |
|  ! 0 |  757 | `		default:` |
|  ! 0 |  758 | `			ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char));` |
|  ! 0 |  759 | `			break;` |
|    - |  760 | `		}` |
|    - |  761 | `		/* Advance the stream cursor */` |
|  ! 0 |  762 | `		zIn++;` |
|  ! 0 |  763 | `	}` |
|   26 |  764 | `}` |
|    - |  765 | `/*` |
|    - |  766 | ` * Returns a ph7_value holding the image of a JSON string. In other word perform a JSON decoding operation.` |
|    - |  767 | ` * According to wikipedia` |
|    - |  768 | ` * JSON's basic types are:` |
|    - |  769 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|    - |  770 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|    - |  771 | ` *   Boolean (true or false)` |
|    - |  772 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|    - |  773 | ` *    do not need to be of the same type)` |
|    - |  774 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|    - |  775 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|    - |  776 | ` *     be distinct from each other)` |
|    - |  777 | ` *   null (empty)` |
|    - |  778 | ` * Non-significant white space may be added freely around the "structural characters" (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|    - |  779 | ` */` |
|   64 |  780 | `static sxi32 VmJsonDecode(` |
|    - |  781 | `	json_decoder *pDecoder, /* JSON decoder */` |
|    - |  782 | `	ph7_value *pArrayKey    /* Key for the decoded array */` |
|    2 |  783 | `	){` |
|    - |  784 | `	ph7_value *pWorker; /* Worker variable */` |
|    - |  785 | `	sxi32 rc;` |
|    - |  786 | `	/* Check if we do not nest to much */` |
|   66 |  787 | `	if( pDecoder->rec_count >= pDecoder->rec_depth ){` |
|    - |  788 | `		/* Nesting limit reached,abort decoding immediately */` |
|  ! 0 |  789 | `		*pDecoder->pErr = JSON_ERROR_DEPTH;` |
|  ! 0 |  790 | `		return SXERR_ABORT;` |
|    - |  791 | `	}` |
|   66 |  792 | `	if( pDecoder->pIn->nType & (JSON_TK_STR\|JSON_TK_TRUE\|JSON_TK_FALSE\|JSON_TK_NULL\|JSON_TK_NUM) ){` |
|    - |  793 | `		/* Scalar value */` |
|   38 |  794 | `		pWorker = ph7_context_new_scalar(pDecoder->pCtx);` |
|   38 |  795 | `		if( pWorker == 0 ){` |
|  ! 0 |  796 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |  797 | `			/* Abort the decoding operation immediately */` |
|  ! 0 |  798 | `			return SXERR_ABORT;` |
|    - |  799 | `		}` |
|    - |  800 | `		/* Reflect the JSON image */` |
|   38 |  801 | `		if( pDecoder->pIn->nType & JSON_TK_NULL ){` |
|    - |  802 | `			/* Nullify the value.*/` |
|  ! 0 |  803 | `			ph7_value_null(pWorker);` |
|   38 |  804 | `		}else if( pDecoder->pIn->nType & (JSON_TK_TRUE\|JSON_TK_FALSE) ){` |
|    - |  805 | `			/* Boolean value */` |
|  ! 0 |  806 | `			ph7_value_bool(pWorker,(pDecoder->pIn->nType & JSON_TK_TRUE) ? 1 : 0 );` |
|   38 |  807 | `		}else if( pDecoder->pIn->nType & JSON_TK_NUM ){` |
|   31 |  808 | `			SyString *pStr = &pDecoder->pIn->sData;` |
|    - |  809 | `			/*` |
|    - |  810 | `			 * Numeric value.` |
|    - |  811 | `			 * Get a string representation first then try to get a numeric` |
|    - |  812 | `			 * value.` |
|    - |  813 | `			 */` |
|   31 |  814 | `			ph7_value_string(pWorker,pStr->zString,(int)pStr->nByte);` |
|    - |  815 | `			/* Obtain a numeric representation */` |
|   31 |  816 | `			PH7_MemObjToNumeric(pWorker);` |
|   16 |  817 | `		}else{` |
|    - |  818 | `			/* Dequote the string */` |
|    8 |  819 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pWorker);` |
|    - |  820 | `		}` |
|    - |  821 | `		/* Invoke the consumer callback */` |
|   38 |  822 | `		rc = pDecoder->xConsumer(pDecoder->pCtx,pArrayKey,pWorker,pDecoder->pUserData);` |
|   38 |  823 | `		if( rc == SXERR_ABORT ){` |
|  ! 0 |  824 | `			return SXERR_ABORT;` |
|    - |  825 | `		}` |
|    - |  826 | `		/* All done,advance the stream cursor */` |
|   38 |  827 | `		pDecoder->pIn++;` |
|   48 |  828 | `	}else if( pDecoder->pIn->nType & JSON_TK_OSB /*'[' */) {` |
|    - |  829 | `		ProcJsonConsumer xOld;` |
|    - |  830 | `		void *pOld;` |
|    - |  831 | `		/* Array representation*/` |
|   13 |  832 | `		pDecoder->pIn++;` |
|    - |  833 | `		/* Create a working array */` |
|   13 |  834 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|   13 |  835 | `		if( pWorker == 0 ){` |
|  ! 0 |  836 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |  837 | `			/* Abort the decoding operation immediately */` |
|  ! 0 |  838 | `			return SXERR_ABORT;` |
|    - |  839 | `		}` |
|    - |  840 | `		/* Save the old consumer */` |
|   13 |  841 | `		xOld = pDecoder->xConsumer;` |
|   13 |  842 | `		pOld = pDecoder->pUserData;` |
|    - |  843 | `		/* Set the new consumer */` |
|   13 |  844 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|   13 |  845 | `		pDecoder->pUserData = pWorker;` |
|    - |  846 | `		/* Decode the array */` |
|   18 |  847 | `		for(;;){` |
|    - |  848 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|    - |  849 | `			 * do this.` |
|    - |  850 | `			 */` |
|   49 |  851 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|   13 |  852 | `				pDecoder->pIn++;` |
|    1 |  853 | `			}` |
|   37 |  854 | `			if( pDecoder->pIn >= pDecoder->pEnd \|\| (pDecoder->pIn->nType & JSON_TK_CSB) /*']'*/ ){` |
|   13 |  855 | `				if( pDecoder->pIn < pDecoder->pEnd ){` |
|   13 |  856 | `					pDecoder->pIn++; /* Jump the trailing ']' */` |
|    6 |  857 | `				}` |
|   13 |  858 | `				break;` |
|    - |  859 | `			}` |
|    - |  860 | `			/* Recurse and decode the entry */` |
|   25 |  861 | `			pDecoder->rec_count++;` |
|   25 |  862 | `			rc = VmJsonDecode(pDecoder,0);` |
|   25 |  863 | `			pDecoder->rec_count--;` |
|   25 |  864 | `			if( rc == SXERR_ABORT ){` |
|    - |  865 | `				/* Abort processing immediately */` |
|  ! 0 |  866 | `				return SXERR_ABORT;` |
|    - |  867 | `			}` |
|    - |  868 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|   25 |  869 | `			if( (pDecoder->pIn < pDecoder->pEnd) &&` |
|   24 |  870 | `				((pDecoder->pIn->nType & (JSON_TK_CSB/*']'*/\|JSON_TK_COMMA/*','*/))==0) ){` |
|    - |  871 | `					/* Unexpected token,abort immediatley */` |
|  ! 0 |  872 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|  ! 0 |  873 | `					return SXERR_ABORT;` |
|    - |  874 | `			}` |
|    1 |  875 | `		}` |
|    - |  876 | `		/* Restore the old consumer */` |
|   13 |  877 | `		pDecoder->xConsumer = xOld;` |
|   13 |  878 | `		pDecoder->pUserData = pOld;` |
|    - |  879 | `		/* Invoke the old consumer on the decoded array */` |
|   13 |  880 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|   24 |  881 | `	}else if( pDecoder->pIn->nType & JSON_TK_OCB /*'{' */) {` |
|    - |  882 | `		ProcJsonConsumer xOld;` |
|    - |  883 | `		ph7_value *pKey;` |
|    - |  884 | `		void *pOld;` |
|    - |  885 | `		/* Object representation*/` |
|   18 |  886 | `		pDecoder->pIn++;` |
|    - |  887 | `		/* Return the object as an associative array */` |
|   18 |  888 | `		if( (pDecoder->iFlags & JSON_DECODE_ASSOC) == 0 ){` |
|    3 |  889 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_WARNING,` |
|    - |  890 | `				"JSON Objects are always returned as an associative array"` |
|    - |  891 | `				);` |
|    1 |  892 | `		}` |
|    - |  893 | `		/* Create a working array */` |
|   18 |  894 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|   18 |  895 | `		pKey = ph7_context_new_scalar(pDecoder->pCtx);` |
|   18 |  896 | `		if( pWorker == 0 \|\| pKey == 0){` |
|  ! 0 |  897 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |  898 | `			/* Abort the decoding operation immediately */` |
|  ! 0 |  899 | `			return SXERR_ABORT;` |
|    - |  900 | `		}` |
|    - |  901 | `		/* Save the old consumer */` |
|   18 |  902 | `		xOld = pDecoder->xConsumer;` |
|   18 |  903 | `		pOld = pDecoder->pUserData;` |
|    - |  904 | `		/* Set the new consumer */` |
|   18 |  905 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|   18 |  906 | `		pDecoder->pUserData = pWorker;` |
|    - |  907 | `		/* Decode the object */` |
|   17 |  908 | `		for(;;){` |
|    - |  909 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|    - |  910 | `			 * do this.` |
|    - |  911 | `			 */` |
|   40 |  912 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|    5 |  913 | `				pDecoder->pIn++;` |
|    1 |  914 | `			}` |
|   36 |  915 | `			if( pDecoder->pIn >= pDecoder->pEnd \|\| (pDecoder->pIn->nType & JSON_TK_CCB) /*'}'*/ ){` |
|   18 |  916 | `				if( pDecoder->pIn < pDecoder->pEnd ){` |
|   16 |  917 | `					pDecoder->pIn++; /* Jump the trailing ']' */` |
|    7 |  918 | `				}` |
|   18 |  919 | `				break;` |
|    - |  920 | `			}` |
|   18 |  921 | `			if( (pDecoder->pIn->nType & JSON_TK_STR) == 0 \|\| &pDecoder->pIn[1] >= pDecoder->pEnd` |
|   20 |  922 | `				\|\| (pDecoder->pIn[1].nType & JSON_TK_COLON) == 0){` |
|    - |  923 | `					/* Syntax error,return immediately */` |
|  ! 0 |  924 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|  ! 0 |  925 | `					return SXERR_ABORT;` |
|    - |  926 | `			}` |
|    - |  927 | `			/* Dequote the key */` |
|   20 |  928 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pKey);` |
|    - |  929 | `			/* Jump the key and the colon */` |
|   20 |  930 | `			pDecoder->pIn += 2;` |
|    - |  931 | `			/* Recurse and decode the value */` |
|   20 |  932 | `			pDecoder->rec_count++;` |
|   20 |  933 | `			rc = VmJsonDecode(pDecoder,pKey);` |
|   20 |  934 | `			pDecoder->rec_count--;` |
|   20 |  935 | `			if( rc == SXERR_ABORT ){` |
|    - |  936 | `				/* Abort processing immediately */` |
|  ! 0 |  937 | `				return SXERR_ABORT;` |
|    - |  938 | `			}` |
|    - |  939 | `			/* Reset the internal buffer of the key */` |
|   20 |  940 | `			ph7_value_reset_string_cursor(pKey);` |
|    - |  941 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|    2 |  942 | `		}` |
|    - |  943 | `		/* Restore the old consumer */` |
|   18 |  944 | `		pDecoder->xConsumer = xOld;` |
|   18 |  945 | `		pDecoder->pUserData = pOld;` |
|    - |  946 | `		/* Invoke the old consumer on the decoded object*/` |
|   18 |  947 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|    - |  948 | `		/* Release the key */` |
|   18 |  949 | `		ph7_context_release_value(pDecoder->pCtx,pKey);` |
|   10 |  950 | `	}else{` |
|    - |  951 | `		/* Unexpected token */` |
|  ! 0 |  952 | `		return SXERR_ABORT; /* Abort immediately */` |
|    - |  953 | `	}` |
|    - |  954 | `	/* Release the worker variable */` |
|   66 |  955 | `	ph7_context_release_value(pDecoder->pCtx,pWorker);` |
|   66 |  956 | `	return SXRET_OK;` |
|   34 |  957 | `}` |
|    - |  958 | `/*` |
|    - |  959 | ` * The following JSON decoder callback is invoked each time` |
|    - |  960 | ` * a JSON array representation [i.e: [15,"hello",FALSE] ]` |
|    - |  961 | ` * is being decoded.` |
|    - |  962 | ` */` |
|   42 |  963 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|    2 |  964 | `{` |
|   44 |  965 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|    - |  966 | `	/* Insert the entry */` |
|   44 |  967 | `	ph7_array_add_elem(pArray,pKey,pWorker); /* Will make it's own copy */` |
|   21 |  968 | `	SXUNUSED(pCtx); /* cc warning */` |
|    - |  969 | `	/* All done */` |
|   44 |  970 | `	return SXRET_OK;` |
|    2 |  971 | `}` |
|    - |  972 | `/*` |
|    - |  973 | ` * Standard JSON decoder callback.` |
|    - |  974 | ` */` |
|   22 |  975 | `static int VmJsonDefaultDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|    2 |  976 | `{` |
|    - |  977 | `	/* Return the value directly */` |
|   24 |  978 | `	ph7_result_value(pCtx,pWorker); /* Will make it's own copy */` |
|   11 |  979 | `	SXUNUSED(pKey); /* cc warning */` |
|   11 |  980 | `	SXUNUSED(pUserData);` |
|    - |  981 | `	/* All done */` |
|   24 |  982 | `	return SXRET_OK;` |
|    2 |  983 | `}` |
|    - |  984 | `/*` |
|    - |  985 | ` * mixed json_decode(string $json[,bool $assoc = false[,int $depth = 32[,int $options = 0 ]]])` |
|    - |  986 | ` *  Takes a JSON encoded string and converts it into a PHP variable.` |
|    - |  987 | ` * Parameters` |
|    - |  988 | ` *  $json` |
|    - |  989 | ` *    The json string being decoded.` |
|    - |  990 | ` * $assoc` |
|    - |  991 | ` *   When TRUE, returned objects will be converted into associative arrays.` |
|    - |  992 | ` * $depth` |
|    - |  993 | ` *   User specified recursion depth.` |
|    - |  994 | ` * $options` |
|    - |  995 | ` *   Bitmask of JSON decode options. Currently only JSON_BIGINT_AS_STRING is supported` |
|    - |  996 | ` * (default is to cast large integers as floats)` |
|    - |  997 | ` * Return` |
|    - |  998 | ` *  The value encoded in json in appropriate PHP type. Values true, false and null (case-insensitive)` |
|    - |  999 | ` *  are returned as TRUE, FALSE and NULL respectively. NULL is returned if the json cannot be decoded` |
|    - | 1000 | ` *  or if the encoded data is deeper than the recursion limit.` |
|    - | 1001 | ` */` |
|    - | 1002 | `/*` |
|    - | 1003 | ` * Tokenize and decode a JSON input. Shared core of json_decode() and json_validate().` |
|    - | 1004 | ` * On success the decoded value is delivered through the default decoder (i.e: it becomes` |
|    - | 1005 | ` * the call-context result, which json_validate's caller then overwrites with a boolean).` |
|    - | 1006 | ` * Returns the resulting JSON error code (pVm->json_rc): JSON_ERROR_NONE on success, a` |
|    - | 1007 | ` * non-zero json_err_code otherwise. A generic decoder abort without a specific code` |
|    - | 1008 | ` * (e.g: out of memory) is reported as JSON_ERROR_SYNTAX so callers can branch on a single` |
|    - | 1009 | ` * value, preserving the original "abort \|\| error => failure" json_decode semantics.` |
|    - | 1010 | ` */` |
|   36 | 1011 | `static int VmJsonDecodeInput(ph7_context *pCtx,const char *zIn,int nByte,int iAssoc,int nDepth)` |
|    2 | 1012 | `{` |
|   38 | 1013 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - | 1014 | `	json_decoder sDecoder;` |
|    - | 1015 | `	SySet sToken;` |
|    - | 1016 | `	SyLex sLex;` |
|    - | 1017 | `	sxi32 rc;` |
|    - | 1018 | `	/* Clear JSON error code */` |
|   38 | 1019 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|    - | 1020 | `	/* Tokenize the input */` |
|   38 | 1021 | `	SySetInit(&sToken,&pVm->sAllocator,sizeof(SyToken));` |
|   38 | 1022 | `	SyLexInit(&sLex,&sToken,VmJsonTokenize,&pVm->json_rc);` |
|   38 | 1023 | `	SyLexTokenizeInput(&sLex,zIn,(sxu32)nByte,0,0,0);` |
|   38 | 1024 | `	if( pVm->json_rc != JSON_ERROR_NONE ){` |
|    - | 1025 | `		/* Something goes wrong while tokenizing input. [i.e: Unexpected token] */` |
|   16 | 1026 | `		SyLexRelease(&sLex);` |
|   16 | 1027 | `		SySetRelease(&sToken);` |
|   16 | 1028 | `		return pVm->json_rc;` |
|    - | 1029 | `	}` |
|    - | 1030 | `	/* Fill the decoder */` |
|   24 | 1031 | `	sDecoder.pCtx = pCtx;` |
|   24 | 1032 | `	sDecoder.pErr = &pVm->json_rc;` |
|   24 | 1033 | `	sDecoder.pIn = (SyToken *)SySetBasePtr(&sToken);` |
|   24 | 1034 | `	sDecoder.pEnd = &sDecoder.pIn[SySetUsed(&sToken)];` |
|   24 | 1035 | `	sDecoder.iFlags = 0;` |
|   24 | 1036 | `	if( iAssoc ){` |
|    - | 1037 | `		/* Returned objects will be converted into associative arrays */` |
|   22 | 1038 | `		sDecoder.iFlags \|= JSON_DECODE_ASSOC;` |
|   10 | 1039 | `	}` |
|   24 | 1040 | `	sDecoder.rec_depth = 32;` |
|   24 | 1041 | `	if( nDepth > 1 && nDepth < 32 ){` |
|    3 | 1042 | `		sDecoder.rec_depth = nDepth;` |
|    1 | 1043 | `	}` |
|   24 | 1044 | `	sDecoder.rec_count = 0;` |
|    - | 1045 | `	/* Set a default consumer */` |
|   24 | 1046 | `	sDecoder.xConsumer = VmJsonDefaultDecoder;` |
|   24 | 1047 | `	sDecoder.pUserData = 0;` |
|    - | 1048 | `	/* Decode the raw JSON input */` |
|   24 | 1049 | `	rc = VmJsonDecode(&sDecoder,0);` |
|   24 | 1050 | `	if( rc == SXERR_ABORT && pVm->json_rc == JSON_ERROR_NONE ){` |
|    - | 1051 | `		/* Generic abort with no specific code: treat as a syntax error */` |
|  ! 0 | 1052 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|  ! 0 | 1053 | `	}` |
|    - | 1054 | `	/* Clean-up the mess left behind */` |
|   24 | 1055 | `	SyLexRelease(&sLex);` |
|   24 | 1056 | `	SySetRelease(&sToken);` |
|   24 | 1057 | `	return pVm->json_rc;` |
|   20 | 1058 | `}` |
|   38 | 1059 | `PH7_PRIVATE int vm_builtin_json_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 1060 | `{` |
|    - | 1061 | `	const char *zIn;` |
|    - | 1062 | `	int nByte;` |
|   40 | 1063 | `	int iAssoc = 0;` |
|   40 | 1064 | `	int nDepth = 32;` |
|   40 | 1065 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - | 1066 | `		/* Missing/Invalid arguments, return NULL */` |
|  ! 0 | 1067 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1068 | `		return PH7_OK;` |
|    - | 1069 | `	}` |
|    - | 1070 | `	/* Extract the JSON string */` |
|   40 | 1071 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   40 | 1072 | `	if( nByte < 1 ){` |
|    - | 1073 | `		/* Empty string,return NULL */` |
|    6 | 1074 | `		ph7_result_null(pCtx);` |
|    6 | 1075 | `		return PH7_OK;` |
|    - | 1076 | `	}` |
|   36 | 1077 | `	if( nArg > 1 && ph7_value_to_bool(apArg[1]) != 0 ){` |
|   24 | 1078 | `		iAssoc = 1;` |
|   11 | 1079 | `	}` |
|   36 | 1080 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|    - | 1081 | `		/* PHP 8: $depth must be in 1 .. INT_MAX (a catchable ValueError otherwise);` |
|    - | 1082 | `		 * read as int64 so a value above INT_MAX is detected, not truncated. */` |
|   13 | 1083 | `		ph7_int64 nWant = ph7_value_to_int64(apArg[2]);` |
|    - | 1084 | `		/* php clears the json error state before validating $depth, so a caught` |
|    - | 1085 | `		 * depth ValueError leaves json_last_error() == JSON_ERROR_NONE (the normal` |
|    - | 1086 | `		 * path resets it again inside VmJsonDecodeInput). */` |
|   13 | 1087 | `		pCtx->pVm->json_rc = JSON_ERROR_NONE;` |
|   13 | 1088 | `		if( nWant <= 0 ){` |
|    9 | 1089 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1090 | `				"json_decode(): Argument #3 ($depth) must be greater than 0");` |
|    - | 1091 | `		}` |
|    5 | 1092 | `		if( nWant > 2147483647 ){` |
|    3 | 1093 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1094 | `				"json_decode(): Argument #3 ($depth) must be less than 2147483647");` |
|    - | 1095 | `		}` |
|    3 | 1096 | `		nDepth = (int)nWant;` |
|    1 | 1097 | `	}` |
|    - | 1098 | `	/* Decode the raw JSON input.The default consumer sets the decoded value as the` |
|    - | 1099 | `	 * call-context result; on failure we replace it with NULL. */` |
|   26 | 1100 | `	if( VmJsonDecodeInput(pCtx,zIn,nByte,iAssoc,nDepth) != JSON_ERROR_NONE ){` |
|    - | 1101 | `		/* Something goes wrong while decoding JSON input.Return NULL. */` |
|   12 | 1102 | `		ph7_result_null(pCtx);` |
|    5 | 1103 | `	}` |
|    - | 1104 | `	/* All done */` |
|   26 | 1105 | `	return PH7_OK;` |
|   21 | 1106 | `}` |
|    - | 1107 | `/*` |
|    - | 1108 | ` * bool json_validate(string $json[,int $depth = 512[,int $flags = 0]])` |
|    - | 1109 | ` *  Validates whether a string is valid JSON without materializing a value.` |
|    - | 1110 | ` * Parameters` |
|    - | 1111 | ` *  $json   The string to validate.` |
|    - | 1112 | ` *  $depth  Maximum nesting depth (clamped to the engine limit of 32).` |
|    - | 1113 | ` *  $flags  Bitmask of decode options (currently none are implemented; accepted/ignored).` |
|    - | 1114 | ` * Return` |
|    - | 1115 | ` *  TRUE if the string is valid JSON, FALSE otherwise. Updates json_last_error().` |
|    - | 1116 | ` */` |
|   20 | 1117 | `PH7_PRIVATE int vm_builtin_json_validate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1118 | `{` |
|   21 | 1119 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - | 1120 | `	const char *zIn;` |
|    - | 1121 | `	int nByte;` |
|   21 | 1122 | `	int nDepth = 32;` |
|   21 | 1123 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - | 1124 | `		/* Missing/Invalid argument: not valid JSON */` |
|  ! 0 | 1125 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|  ! 0 | 1126 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1127 | `		return PH7_OK;` |
|    - | 1128 | `	}` |
|    - | 1129 | `	/* Extract the JSON string */` |
|   21 | 1130 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   21 | 1131 | `	if( nByte < 1 ){` |
|    - | 1132 | `		/* The empty string is not valid JSON (unlike json_decode, which returns NULL` |
|    - | 1133 | `		 * silently, json_validate must record the syntax error) */` |
|    3 | 1134 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|    3 | 1135 | `		ph7_result_bool(pCtx,0);` |
|    3 | 1136 | `		return PH7_OK;` |
|    - | 1137 | `	}` |
|   19 | 1138 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|    - | 1139 | `		/* PHP 8: $depth must be in 1 .. INT_MAX (a catchable ValueError otherwise). */` |
|    9 | 1140 | `		ph7_int64 nWant = ph7_value_to_int64(apArg[1]);` |
|    - | 1141 | `		/* Clear the json error state before validating $depth (php parity), so a` |
|    - | 1142 | `		 * caught depth ValueError leaves json_last_error() == JSON_ERROR_NONE. */` |
|    9 | 1143 | `		pVm->json_rc = JSON_ERROR_NONE;` |
|    9 | 1144 | `		if( nWant <= 0 ){` |
|    5 | 1145 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1146 | `				"json_validate(): Argument #2 ($depth) must be greater than 0");` |
|    - | 1147 | `		}` |
|    5 | 1148 | `		if( nWant > 2147483647 ){` |
|    3 | 1149 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1150 | `				"json_validate(): Argument #2 ($depth) must be less than 2147483647");` |
|    - | 1151 | `		}` |
|    3 | 1152 | `		nDepth = (int)nWant;` |
|    1 | 1153 | `	}` |
|    - | 1154 | `	/* apArg[2] ($flags) is accepted and ignored: no decode flag is implemented.` |
|    - | 1155 | `	 * Decode in associative mode so the "objects are returned as an array" warning is` |
|    - | 1156 | `	 * not raised - the decoded value is discarded, only its validity matters. */` |
|   13 | 1157 | `	ph7_result_bool(pCtx,VmJsonDecodeInput(pCtx,zIn,nByte,1,nDepth) == JSON_ERROR_NONE);` |
|   13 | 1158 | `	return PH7_OK;` |
|   11 | 1159 | `}` |
|    - | 1160 |  |
