# src/ph7/vm_json.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 494/634 lines (77.92%)

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
|  882 |   85 | `static sxi32 VmJsonEncode(` |
|    - |   86 | `	ph7_value *pIn,          /* Encode this value */` |
|    - |   87 | `	json_private_data *pData /* Context data */` |
|    2 |   88 | `	){` |
|  884 |   89 | `		ph7_context *pCtx = pData->pCtx;` |
|  884 |   90 | `		int iFlags = pData->iFlags;` |
|    - |   91 | `		int nByte;` |
|  884 |   92 | `		if( ph7_value_is_null(pIn) \|\| ph7_value_is_resource(pIn)){` |
|    - |   93 | `			/* null */` |
|    5 |   94 | `			JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));` |
|  881 |   95 | `		}else if( ph7_value_is_bool(pIn) ){` |
|   15 |   96 | `			int iBool = ph7_value_to_bool(pIn);` |
|    - |   97 | `			int iLen;` |
|    - |   98 | `			/* true/false */` |
|   15 |   99 | `			iLen = iBool ? (int)sizeof("true") : (int)sizeof("false");` |
|   15 |  100 | `			JSON_EMIT(pData,ph7_result_string(pCtx,iBool ? "true" : "false",iLen-1));` |
|  872 |  101 | `		}else if(  ph7_value_is_numeric(pIn) && !ph7_value_is_string(pIn) ){` |
|  454 |  102 | `			if( ph7_value_is_float(pIn) ){` |
|    - |  103 | `				/* php's json float output follows serialize_precision` |
|    - |  104 | `				 * (shortest round-trip, like serialize/var_export), NOT the` |
|    - |  105 | `				 * echo/cast precision of 14 — with a lowercase exponent` |
|    - |  106 | `				 * marker: 1/3 -> 0.3333333333333333, 1e17 -> 1.0e+17,` |
|    - |  107 | `				 * 1.0 -> 1, -0.0 -> -0. */` |
|   17 |  108 | `				JSON_EMIT(pData,VmJsonEmitReal(pCtx,ph7_value_to_double(pIn)));` |
|    9 |  109 | `			}else{` |
|    - |  110 | `				const char *zNum;` |
|    - |  111 | `				/* Get a string representation of the number */` |
|  287 |  112 | `				zNum = ph7_value_to_string(pIn,&nByte);` |
|  287 |  113 | `				JSON_EMIT(pData,ph7_result_string(pCtx,zNum,nByte));` |
|    1 |  114 | `			}` |
|  715 |  115 | `		}else if( ph7_value_is_string(pIn) ){` |
|  266 |  116 | `			if( (iFlags & JSON_NUMERIC_CHECK) &&  ph7_value_is_numeric(pIn) ){` |
|    - |  117 | `				/* Encodes numeric strings as numbers (same float shapes). */` |
|    5 |  118 | `				PH7_MemObjToReal(pIn); /* Force a numeric cast */` |
|    5 |  119 | `				JSON_EMIT(pData,VmJsonEmitReal(pCtx,ph7_value_to_double(pIn)));` |
|    3 |  120 | `			}else{` |
|    - |  121 | `				const char *zIn,*zEnd;` |
|    - |  122 | `				int c;` |
|    - |  123 | `				/* Encode the string */` |
|  262 |  124 | `				zIn = ph7_value_to_string(pIn,&nByte);` |
|  262 |  125 | `				zEnd = &zIn[nByte];` |
|    - |  126 | `				/* Append the double quote */` |
|  262 |  127 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"\"",(int)sizeof(char)));` |
|  491 |  128 | `				for(;;){` |
|  990 |  129 | `					if( zIn >= zEnd ){` |
|    - |  130 | `						/* No more input to process */` |
|  262 |  131 | `						break;` |
|    - |  132 | `					}` |
|  730 |  133 | `					c = zIn[0];` |
|    - |  134 | `					/* Advance the stream cursor */` |
|  730 |  135 | `					zIn++;` |
|  730 |  136 | `					if( (c == '<' \|\| c == '>') && (iFlags & JSON_HEX_TAG) ){` |
|    - |  137 | `						/* All < and > are converted to \u003C and \u003E */` |
|  ! 0 |  138 | `						if( c == '<' ){` |
|  ! 0 |  139 | `							JSON_EMIT(pData,ph7_result_string(pCtx,"\\u003C",(int)sizeof("\\u003C")-1));` |
|  ! 0 |  140 | `						}else{` |
|  ! 0 |  141 | `							JSON_EMIT(pData,ph7_result_string(pCtx,"\\u003E",(int)sizeof("\\u003E")-1));` |
|    - |  142 | `						}` |
|  ! 0 |  143 | `						continue;` |
|  730 |  144 | `					}else if( c == '&' && (iFlags & JSON_HEX_AMP) ){` |
|    - |  145 | `						/* All &s are converted to \u0026.  */` |
|  ! 0 |  146 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\u0026",(int)sizeof("\\u0026")-1));` |
|  ! 0 |  147 | `						continue;` |
|  730 |  148 | `					}else if( c == '\'' && (iFlags & JSON_HEX_APOS) ){` |
|    - |  149 | `						/* All ' are converted to \u0027.   */` |
|  ! 0 |  150 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\u0027",(int)sizeof("\\u0027")-1));` |
|  ! 0 |  151 | `						continue;` |
|  730 |  152 | `					}else if( c == '"' && (iFlags & JSON_HEX_QUOT) ){` |
|    - |  153 | `						/* All " are converted to \u0022. */` |
|  ! 0 |  154 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\u0022",(int)sizeof("\\u0022")-1));` |
|  ! 0 |  155 | `						continue;` |
|    - |  156 | `					}` |
|  730 |  157 | `					if( c == '"' \|\| c == '\\' ){` |
|    - |  158 | `						/* Escape the quote/backslash (php escapes the backslash` |
|    - |  159 | `						 * unconditionally — the old code wrongly tied it to` |
|    - |  160 | `						 * JSON_UNESCAPED_SLASHES, which governs '/' below) */` |
|    3 |  161 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\",(int)sizeof(char)));` |
|  728 |  162 | `					}else if( c == '/' && (iFlags & JSON_UNESCAPED_SLASHES) == 0 ){` |
|    - |  163 | `						/* php escapes forward slashes by default */` |
|    7 |  164 | `						JSON_EMIT(pData,ph7_result_string(pCtx,"\\",(int)sizeof(char)));` |
|  724 |  165 | `					}else if( (unsigned char)c < 0x20 ){` |
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
|  724 |  186 | `					JSON_EMIT(pData,ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char)));` |
|    2 |  187 | `				}` |
|    - |  188 | `				/* Append the double quote */` |
|  262 |  189 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"\"",(int)sizeof(char)));` |
|    2 |  190 | `			}` |
|  432 |  191 | `		}else if( ph7_value_is_array(pIn) ){` |
|    - |  192 | `			/* An array encodes as a JSON array iff it is a "list" [consecutive` |
|    - |  193 | `			 * 0-based int keys]; otherwise [or under JSON_FORCE_OBJECT] as an` |
|    - |  194 | `			 * object with stringified keys (PHP semantics). */` |
|  464 |  195 | `			int isObject = (iFlags & JSON_FORCE_OBJECT)` |
|  232 |  196 | `				\|\| !PH7_HashmapIsList((ph7_hashmap *)pIn->x.pOther);` |
|  233 |  197 | `			int savedObject = pData->isObject; /* restore for sibling entries after recursion */` |
|  233 |  198 | `			int c = isObject ? '{' : '[';` |
|  233 |  199 | `			int d = isObject ? '}' : ']';` |
|    - |  200 | `			/* Encode the array */` |
|  233 |  201 | `			pData->isObject = isObject;` |
|  233 |  202 | `			pData->isFirst = 1;` |
|    - |  203 | `			/* Append the square bracket or curly braces */` |
|  233 |  204 | `			JSON_EMIT(pData,ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char)));` |
|    - |  205 | `			/* Iterate throw array entries */` |
|  233 |  206 | `			ph7_array_walk(pIn,VmJsonArrayEncode,pData);` |
|    - |  207 | `			/* Bail if a nested append ran out of memory before the closer */` |
|  233 |  208 | `			if( pData->oom ){` |
|  ! 0 |  209 | `				return PH7_OK;` |
|    - |  210 | `			}` |
|    - |  211 | `			/* Append the closing square bracket or curly braces */` |
|  233 |  212 | `			JSON_EMIT(pData,ph7_result_string(pCtx,(const char *)&d,(int)sizeof(char)));` |
|  233 |  213 | `			pData->isObject = savedObject;` |
|  184 |  214 | `		}else if( ph7_value_is_object(pIn) ){` |
|   68 |  215 | `			ph7_class_instance *pThis = (ph7_class_instance *)pIn->x.pOther;` |
|   68 |  216 | `			ph7_vm *pVm = pIn->pVm;` |
|   68 |  217 | `			ph7_class_method *pMethod = 0;` |
|    - |  218 | `			/* If the object implements JsonSerializable, encode the value` |
|    - |  219 | `			 * returned by jsonSerialize() instead of its public properties.` |
|    - |  220 | `			 * An enum implementing it explicitly also takes this path (php). */` |
|   66 |  221 | `			if( pVm->pJsonSerializableClass` |
|   68 |  222 | `				&& PH7_VmInstanceOf(pThis->pClass,pVm->pJsonSerializableClass) ){` |
|   15 |  223 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"jsonSerialize",sizeof("jsonSerialize")-1);` |
|    7 |  224 | `			}` |
|   68 |  225 | `			if( pMethod == 0 && (pThis->pClass->iFlags & PH7_CLASS_ENUM) != 0 ){` |
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
|   60 |  239 | `			if( pMethod ){` |
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
|   46 |  274 | `				pData->isFirst = 1;` |
|    - |  275 | `				/* Append the curly braces */` |
|   46 |  276 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"{",(int)sizeof(char)));` |
|   46 |  277 | `				SySetInit(&sNames,&pVm->sAllocator,sizeof(SyString));` |
|   46 |  278 | `				SyHashResetLoopCursor(&pThis->hAttr);` |
|  124 |  279 | `				while( (pAttrEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|   80 |  280 | `					VmClassAttr *pVmAttr = (VmClassAttr *)pAttrEntry->pUserData;` |
|   78 |  281 | `					if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT))` |
|   80 |  282 | `					 \|\| pVmAttr->pAttr->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|    3 |  283 | `						continue;` |
|    - |  284 | `					}` |
|   76 |  285 | `					if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|   40 |  286 | `					 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|    3 |  287 | `						continue; /* virtual set-only property: no value to encode (php) */` |
|    - |  288 | `					}` |
|   76 |  289 | `					SySetPut(&sNames,(const void *)&pVmAttr->pAttr->sName);` |
|    2 |  290 | `				}` |
|   46 |  291 | `				aName = (SyString *)SySetBasePtr(&sNames);` |
|   46 |  292 | `				nName = SySetUsed(&sNames);` |
|  120 |  293 | `				for( iName = 0 ; iName < nName ; ++iName ){` |
|    - |  294 | `					VmClassAttr *pVmAttr;` |
|   76 |  295 | `					ph7_value *pAttrVal = 0;` |
|    - |  296 | `					ph7_value sHookVal;` |
|    - |  297 | `					sxi32 rcHk;` |
|   76 |  298 | `					pAttrEntry = SyHashGet(&pThis->hAttr,(const void *)aName[iName].zString,aName[iName].nByte);` |
|   76 |  299 | `					if( pAttrEntry == 0 ){` |
|  ! 0 |  300 | `						continue; /* unset by an earlier hook */` |
|    - |  301 | `					}` |
|   76 |  302 | `					pVmAttr = (VmClassAttr *)pAttrEntry->pUserData;` |
|   76 |  303 | `					PH7_MemObjInit(pVm,&sHookVal);` |
|   76 |  304 | `					rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|   76 |  305 | `					if( rcHk == SXRET_OK ){` |
|   11 |  306 | `						pAttrVal = &sHookVal;` |
|   71 |  307 | `					}else if( rcHk == SXERR_NOTFOUND ){` |
|    - |  308 | `						/* Encode a COPY: the encoder casts scalars in place` |
|    - |  309 | `						 * (ph7_value_to_string), which must not corrupt the` |
|    - |  310 | `						 * live attribute slot. */` |
|   66 |  311 | `						ph7_value *pRaw = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|   66 |  312 | `						if( pRaw ){` |
|   66 |  313 | `							PH7_MemObjStore(pRaw,&sHookVal);` |
|   66 |  314 | `							pAttrVal = &sHookVal;` |
|   32 |  315 | `						}` |
|   34 |  316 | `					}else{` |
|    - |  317 | `						/* the get hook threw — propagate like jsonSerialize() */` |
|  ! 0 |  318 | `						PH7_MemObjRelease(&sHookVal);` |
|  ! 0 |  319 | `						SySetRelease(&sNames);` |
|  ! 0 |  320 | `						pData->exc = 1;` |
|  ! 0 |  321 | `						return PH7_EXCEPTION;` |
|    - |  322 | `					}` |
|   76 |  323 | `					if( pAttrVal ){` |
|   76 |  324 | `						VmJsonObjectEncode(SyStringData(&pVmAttr->pAttr->sName),pAttrVal,pData);` |
|   37 |  325 | `					}` |
|   76 |  326 | `					PH7_MemObjRelease(&sHookVal);` |
|   76 |  327 | `					if( pData->exc ){` |
|  ! 0 |  328 | `						SySetRelease(&sNames);` |
|  ! 0 |  329 | `						return PH7_EXCEPTION; /* a nested jsonSerialize()/hook threw */` |
|    - |  330 | `					}` |
|   76 |  331 | `					if( pData->oom ){` |
|  ! 0 |  332 | `						SySetRelease(&sNames);` |
|  ! 0 |  333 | `						return PH7_OK;` |
|    - |  334 | `					}` |
|   39 |  335 | `				}` |
|   46 |  336 | `				SySetRelease(&sNames);` |
|    - |  337 | `				/* Append the closing curly braces  */` |
|   46 |  338 | `				JSON_EMIT(pData,ph7_result_string(pCtx,"}",(int)sizeof(char)));` |
|    - |  339 | `			}` |
|   29 |  340 | `		}else{` |
|    - |  341 | `			/* Can't happen */` |
|  ! 0 |  342 | `			JSON_EMIT(pData,ph7_result_string(pCtx,"null",(int)sizeof("null")-1));` |
|    - |  343 | `		}` |
|    - |  344 | `		/* All done */` |
|  872 |  345 | `		return PH7_OK;` |
|  443 |  346 | `}` |
|    - |  347 | `/*` |
|    - |  348 | ` * The following walker callback is invoked each time we need` |
|    - |  349 | ` * to encode an array to JSON.` |
|    - |  350 | ` */` |
|  522 |  351 | `static int VmJsonArrayEncode(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|    1 |  352 | `{` |
|  523 |  353 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|  523 |  354 | `	if( pJson->nRecCount > 31 \|\| pJson->exc \|\| pJson->oom ){` |
|    - |  355 | `		/* Recursion limit reached, a callback threw, or OOM — return immediately */` |
|  ! 0 |  356 | `		return PH7_OK;` |
|    - |  357 | `	}` |
|  523 |  358 | `	if( !pJson->isFirst ){` |
|    - |  359 | `		/* Append the colon first */` |
|  317 |  360 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,",",(int)sizeof(char)));` |
|  158 |  361 | `	}` |
|  523 |  362 | `	if( pJson->isObject ){` |
|    - |  363 | `		/* Outputs an object rather than an array */` |
|    - |  364 | `		const char *zKey;` |
|    - |  365 | `		int nByte;` |
|    - |  366 | `		/* Extract a string representation of the key */` |
|  275 |  367 | `		zKey = ph7_value_to_string(pKey,&nByte);` |
|    - |  368 | `		/* Append the quoted key and the colon (checked, so an OOM here is caught` |
|    - |  369 | `		 * rather than silently truncating; matches the prior "%.*s" emit byte for` |
|    - |  370 | `		 * byte — keys are not JSON-escaped, a pre-existing behavior). */` |
|  275 |  371 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,"\"",(int)sizeof(char)));` |
|  275 |  372 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,zKey,nByte));` |
|  275 |  373 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,"\":",(int)sizeof("\":")-1));` |
|  137 |  374 | `	}` |
|    - |  375 | `	/* Encode the value */` |
|  523 |  376 | `	pJson->nRecCount++;` |
|  523 |  377 | `	VmJsonEncode(pValue,pJson);` |
|  523 |  378 | `	pJson->nRecCount--;` |
|  523 |  379 | `	pJson->isFirst = 0;` |
|  523 |  380 | `	return PH7_OK;` |
|  262 |  381 | `}` |
|    - |  382 | `/*` |
|    - |  383 | ` * The following walker callback is invoked each time we need to encode` |
|    - |  384 | ` * a class instance [i.e: Object in the PHP jargon] to JSON.` |
|    - |  385 | ` */` |
|   74 |  386 | `static int VmJsonObjectEncode(const char *zAttr,ph7_value *pValue,void *pUserData)` |
|    2 |  387 | `{` |
|   76 |  388 | `	json_private_data *pJson = (json_private_data *)pUserData;` |
|   76 |  389 | `	if( pJson->nRecCount > 31 \|\| pJson->exc \|\| pJson->oom ){` |
|    - |  390 | `		/* Recursion limit reached, a callback threw, or OOM — return immediately */` |
|  ! 0 |  391 | `		return PH7_OK;` |
|    - |  392 | `	}` |
|   76 |  393 | `	if( !pJson->isFirst ){` |
|    - |  394 | `		/* Append the colon first */` |
|   33 |  395 | `		JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,",",(int)sizeof(char)));` |
|   16 |  396 | `	}` |
|    - |  397 | `	/* Append the quoted attribute name and the colon (checked; matches the prior` |
|    - |  398 | `	 * "%s" emit byte for byte — attribute names are not JSON-escaped). */` |
|   76 |  399 | `	JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,"\"",(int)sizeof(char)));` |
|   76 |  400 | `	JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,zAttr,-1));` |
|   76 |  401 | `	JSON_EMIT(pJson,ph7_result_string(pJson->pCtx,"\":",(int)sizeof("\":")-1));` |
|    - |  402 | `	/* Encode the value */` |
|   76 |  403 | `	pJson->nRecCount++;` |
|   76 |  404 | `	VmJsonEncode(pValue,pJson);` |
|   76 |  405 | `	pJson->nRecCount--;` |
|   76 |  406 | `	pJson->isFirst = 0;` |
|   76 |  407 | `	return PH7_OK;` |
|   39 |  408 | `}` |
|    - |  409 | `/*` |
|    - |  410 | ` * string json_encode(mixed $value [, int $options = 0 ])` |
|    - |  411 | ` *  Returns a string containing the JSON representation of value.` |
|    - |  412 | ` * Parameters` |
|    - |  413 | ` *  $value` |
|    - |  414 | ` *  The value being encoded. Can be any type except a resource.` |
|    - |  415 | ` * $options` |
|    - |  416 | ` *  Bitmask consisting of:` |
|    - |  417 | ` *  JSON_HEX_TAG   All < and > are converted to \u003C and \u003E.` |
|    - |  418 | ` *  JSON_HEX_AMP   All &s are converted to \u0026.` |
|    - |  419 | ` *  JSON_HEX_APOS  All ' are converted to \u0027.` |
|    - |  420 | ` *  JSON_HEX_QUOT  All " are converted to \u0022.` |
|    - |  421 | ` *  JSON_FORCE_OBJECT  Outputs an object rather than an array.` |
|    - |  422 | ` *  JSON_NUMERIC_CHECK Encodes numeric strings as numbers.` |
|    - |  423 | ` *  JSON_BIGINT_AS_STRING   Not used` |
|    - |  424 | ` *  JSON_PRETTY_PRINT       Use whitespace in returned data to format it.` |
|    - |  425 | ` *  JSON_UNESCAPED_SLASHES  Don't escape '/'` |
|    - |  426 | ` *  JSON_UNESCAPED_UNICODE  Not used.` |
|    - |  427 | ` * Return` |
|    - |  428 | ` *  Returns a JSON encoded string on success. FALSE otherwise` |
|    - |  429 | ` */` |
|  270 |  430 | `PH7_PRIVATE int vm_builtin_json_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  431 | `{` |
|    - |  432 | `	json_private_data sJson;` |
|    - |  433 | `	sxi32 rc;` |
|  272 |  434 | `	if( nArg < 1 ){` |
|    - |  435 | `		/* Missing arguments,return FALSE */` |
|  ! 0 |  436 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  437 | `		return PH7_OK;` |
|    - |  438 | `	}` |
|    - |  439 | `	/* Prepare the JSON data */` |
|  272 |  440 | `	sJson.nRecCount = 0;` |
|  272 |  441 | `	sJson.pCtx = pCtx;` |
|  272 |  442 | `	sJson.isFirst = 1;` |
|  272 |  443 | `	sJson.iFlags = 0;` |
|  272 |  444 | `	sJson.exc = 0;` |
|  272 |  445 | `	sJson.oom = 0;` |
|  272 |  446 | `	sJson.fail = 0;` |
|  272 |  447 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|    - |  448 | `		/* Extract option flags */` |
|    7 |  449 | `		sJson.iFlags = ph7_value_to_int(apArg[1]);` |
|    3 |  450 | `	}` |
|  272 |  451 | `	pCtx->pVm->json_rc = JSON_ERROR_NONE;` |
|    - |  452 | `	/* Perform the encoding operation */` |
|  272 |  453 | `	rc = VmJsonEncode(apArg[0],&sJson);` |
|  272 |  454 | `	if( sJson.oom ){` |
|    - |  455 | `		/* A result append ran out of memory: raise a non-catchable fatal,` |
|    - |  456 | `		 * distinct from a JSON-encoding error (json_last_error untouched). */` |
|  ! 0 |  457 | `		return PH7_ContextMemoryError(pCtx);` |
|    - |  458 | `	}` |
|  272 |  459 | `	if( rc == PH7_EXCEPTION \|\| sJson.exc ){` |
|    - |  460 | `		/* A jsonSerialize() callback threw — propagate so the exception unwinds */` |
|    5 |  461 | `		return PH7_EXCEPTION;` |
|    - |  462 | `	}` |
|  268 |  463 | `	if( sJson.fail ){` |
|    - |  464 | `		/* Unencodable value (php 8.1: non-backed enum case): the whole encode` |
|    - |  465 | `		 * fails — discard whatever was emitted and return FALSE. */` |
|    3 |  466 | `		pCtx->pVm->json_rc = JSON_ERROR_NON_BACKED_ENUM;` |
|    3 |  467 | `		ph7_result_bool(pCtx,0);` |
|    3 |  468 | `		return PH7_OK;` |
|    - |  469 | `	}` |
|    - |  470 | `	/* All done */` |
|  266 |  471 | `	return PH7_OK;` |
|  137 |  472 | `}` |
|    - |  473 | `#undef JSON_EMIT` |
|    - |  474 | `/*` |
|    - |  475 | ` * int json_last_error(void)` |
|    - |  476 | ` *  Returns the last error (if any) occurred during the last JSON encoding/decoding.` |
|    - |  477 | ` * Parameters` |
|    - |  478 | ` *  None` |
|    - |  479 | ` * Return` |
|    - |  480 | ` *  Returns an integer, the value can be one of the following constants:` |
|    - |  481 | ` *  JSON_ERROR_NONE            No error has occurred.` |
|    - |  482 | ` *  JSON_ERROR_DEPTH           The maximum stack depth has been exceeded.` |
|    - |  483 | ` *  JSON_ERROR_STATE_MISMATCH  Invalid or malformed JSON.` |
|    - |  484 | ` *  JSON_ERROR_CTRL_CHAR  	   Control character error, possibly incorrectly encoded.` |
|    - |  485 | ` *  JSON_ERROR_SYNTAX          Syntax error.` |
|    - |  486 | ` *  JSON_ERROR_UTF8_CHECK      Malformed UTF-8 characters.` |
|    - |  487 | ` */` |
|   12 |  488 | `PH7_PRIVATE int vm_builtin_json_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  489 | `{` |
|   14 |  490 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  491 | `	/* Return the error code */` |
|   14 |  492 | `	ph7_result_int(pCtx,pVm->json_rc);` |
|    6 |  493 | `	SXUNUSED(nArg); /* cc warning */` |
|    6 |  494 | `	SXUNUSED(apArg);` |
|   14 |  495 | `	return PH7_OK;` |
|    2 |  496 | `}` |
|    - |  497 | `/*` |
|    - |  498 | ` * string json_last_error_msg(void)` |
|    - |  499 | ` *  Returns the error string of the last JSON encoding/decoding operation.` |
|    - |  500 | ` * Parameters` |
|    - |  501 | ` *  None` |
|    - |  502 | ` * Return` |
|    - |  503 | ` *  Returns the human-readable message corresponding to the last json_last_error()` |
|    - |  504 | ` *  code, or "No error" if no error has occurred.` |
|    - |  505 | ` */` |
|    4 |  506 | `PH7_PRIVATE int vm_builtin_json_last_error_msg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  507 | `{` |
|    5 |  508 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  509 | `	const char *zMsg;` |
|    5 |  510 | `	switch( pVm->json_rc ){` |
|    1 |  511 | `	case JSON_ERROR_NONE:` |
|    3 |  512 | `		zMsg = "No error";` |
|    3 |  513 | `		break;` |
|  ! 0 |  514 | `	case JSON_ERROR_DEPTH:` |
|  ! 0 |  515 | `		zMsg = "Maximum stack depth exceeded";` |
|  ! 0 |  516 | `		break;` |
|  ! 0 |  517 | `	case JSON_ERROR_STATE_MISMATCH:` |
|  ! 0 |  518 | `		zMsg = "State mismatch (invalid or malformed JSON)";` |
|  ! 0 |  519 | `		break;` |
|  ! 0 |  520 | `	case JSON_ERROR_CTRL_CHAR:` |
|  ! 0 |  521 | `		zMsg = "Control character error, possibly incorrectly encoded";` |
|  ! 0 |  522 | `		break;` |
|    1 |  523 | `	case JSON_ERROR_SYNTAX:` |
|    3 |  524 | `		zMsg = "Syntax error";` |
|    3 |  525 | `		break;` |
|  ! 0 |  526 | `	case JSON_ERROR_UTF8:` |
|  ! 0 |  527 | `		zMsg = "Malformed UTF-8 characters, possibly incorrectly encoded";` |
|  ! 0 |  528 | `		break;` |
|  ! 0 |  529 | `	case JSON_ERROR_NON_BACKED_ENUM:` |
|  ! 0 |  530 | `		zMsg = "Non-backed enums have no default serialization";` |
|  ! 0 |  531 | `		break;` |
|  ! 0 |  532 | `	default:` |
|  ! 0 |  533 | `		zMsg = "Unknown error";` |
|  ! 0 |  534 | `		break;` |
|    - |  535 | `	}` |
|    5 |  536 | `	ph7_result_string(pCtx,zMsg,-1/* Compute length automatically */);` |
|    2 |  537 | `	SXUNUSED(nArg); /* cc warning */` |
|    2 |  538 | `	SXUNUSED(apArg);` |
|    5 |  539 | `	return PH7_OK;` |
|    1 |  540 | `}` |
|    - |  541 | `/* Possible tokens from the JSON tokenization process */` |
|    - |  542 | `#define JSON_TK_TRUE    0x001 /* Boolean true */` |
|    - |  543 | `#define JSON_TK_FALSE   0x002 /* Boolean false */` |
|    - |  544 | `#define JSON_TK_STR     0x004 /* String enclosed in double quotes */` |
|    - |  545 | `#define JSON_TK_NULL    0x008 /* null */` |
|    - |  546 | `#define JSON_TK_NUM     0x010 /* Numeric */` |
|    - |  547 | `#define JSON_TK_OCB     0x020 /* Open curly braces '{' */` |
|    - |  548 | `#define JSON_TK_CCB     0x040 /* Closing curly braces '}' */` |
|    - |  549 | `#define JSON_TK_OSB     0x080 /* Open square bracke '[' */` |
|    - |  550 | `#define JSON_TK_CSB     0x100 /* Closing square bracket ']' */` |
|    - |  551 | `#define JSON_TK_COLON   0x200 /* Single colon ':' */` |
|    - |  552 | `#define JSON_TK_COMMA   0x400 /* Single comma ',' */` |
|    - |  553 | `#define JSON_TK_INVALID 0x800 /* Unexpected token */` |
|    - |  554 | `/*` |
|    - |  555 | ` * Tokenize an entire JSON input.` |
|    - |  556 | ` * Get a single low-level token from the input file.` |
|    - |  557 | ` * Update the stream pointer so that it points to the first` |
|    - |  558 | ` * character beyond the extracted token.` |
|    - |  559 | ` */` |
|  160 |  560 | `static sxi32 VmJsonTokenize(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)` |
|    2 |  561 | `{` |
|  162 |  562 | `	int *pJsonErr = (int *)pUserData;` |
|    - |  563 | `	SyString *pStr;` |
|    - |  564 | `	int c;` |
|    - |  565 | `	/* Ignore leading white spaces */` |
|  166 |  566 | `	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){` |
|    - |  567 | `		/* Advance the stream cursor */` |
|    6 |  568 | `		if( pStream->zText[0] == '\n' ){` |
|    - |  569 | `			/* Update line counter */` |
|  ! 0 |  570 | `			pStream->nLine++;` |
|  ! 0 |  571 | `		}` |
|    6 |  572 | `		pStream->zText++;` |
|    2 |  573 | `	}` |
|  162 |  574 | `	if( pStream->zText >= pStream->zEnd ){` |
|    - |  575 | `		/* End of input reached */` |
|  ! 0 |  576 | `		SXUNUSED(pCtxData); /* cc warning */` |
|  ! 0 |  577 | `		return SXERR_EOF;` |
|    - |  578 | `	}` |
|    - |  579 | `	/* Record token starting position and line */` |
|  162 |  580 | `	pToken->nLine = pStream->nLine;` |
|  162 |  581 | `	pToken->pUserData = 0;` |
|  162 |  582 | `	pStr = &pToken->sData;` |
|  162 |  583 | `	SyStringInitFromBuf(pStr,pStream->zText,0);` |
|  160 |  584 | `	if( pStream->zText[0] == '{' \|\| pStream->zText[0] == '[' \|\| pStream->zText[0] == '}' \|\| pStream->zText[0] == ']'` |
|  110 |  585 | `		\|\| pStream->zText[0] == ':' \|\| pStream->zText[0] == ',' ){` |
|    - |  586 | `			/* Single character */` |
|   94 |  587 | `			c = pStream->zText[0];` |
|    - |  588 | `			/* Set token type */` |
|   94 |  589 | `			switch(c){` |
|   13 |  590 | `			case '[': pToken->nType = JSON_TK_OSB;   break;` |
|   22 |  591 | `			case '{': pToken->nType = JSON_TK_OCB;   break;` |
|   16 |  592 | `			case '}': pToken->nType = JSON_TK_CCB;   break;` |
|   13 |  593 | `			case ']': pToken->nType = JSON_TK_CSB;   break;` |
|   20 |  594 | `			case ':': pToken->nType = JSON_TK_COLON; break;` |
|   17 |  595 | `			case ',': pToken->nType = JSON_TK_COMMA; break;` |
|  ! 0 |  596 | `			default:` |
|  ! 0 |  597 | `				break;` |
|    - |  598 | `			}` |
|    - |  599 | `			/* Advance the stream cursor */` |
|   94 |  600 | `			pStream->zText++;` |
|  116 |  601 | `	}else if( pStream->zText[0] == '"') {` |
|    - |  602 | `		/* JSON string */` |
|   26 |  603 | `		pStream->zText++;` |
|   26 |  604 | `		pStr->zString++;` |
|    - |  605 | `		/* Delimit the string */` |
|   72 |  606 | `		while( pStream->zText < pStream->zEnd ){` |
|   72 |  607 | `			if( pStream->zText[0] == '"' && pStream->zText[-1] != '\\' ){` |
|   26 |  608 | `				break;` |
|    - |  609 | `			}` |
|   48 |  610 | `			if( pStream->zText[0] == '\n' ){` |
|    - |  611 | `				/* Update line counter */` |
|  ! 0 |  612 | `				pStream->nLine++;` |
|  ! 0 |  613 | `			}` |
|   48 |  614 | `			pStream->zText++;` |
|    2 |  615 | `		}` |
|   26 |  616 | `		if( pStream->zText >= pStream->zEnd ){` |
|    - |  617 | `			/* Missing closing '"' */` |
|  ! 0 |  618 | `			pToken->nType = JSON_TK_INVALID;` |
|  ! 0 |  619 | `			*pJsonErr = JSON_ERROR_SYNTAX;` |
|  ! 0 |  620 | `		}else{` |
|   26 |  621 | `			pToken->nType = JSON_TK_STR;` |
|   26 |  622 | `			pStream->zText++; /* Jump the closing double quotes */` |
|    2 |  623 | `		}` |
|   58 |  624 | `	}else if( pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|    - |  625 | `		/* Number */` |
|   31 |  626 | `		pStream->zText++;` |
|   31 |  627 | `		pToken->nType = JSON_TK_NUM;` |
|   31 |  628 | `		while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  629 | `			pStream->zText++;` |
|  ! 0 |  630 | `		}` |
|   31 |  631 | `		if( pStream->zText < pStream->zEnd ){` |
|   31 |  632 | `			c = pStream->zText[0];` |
|   31 |  633 | `			if( c == '.' ){` |
|    - |  634 | `					/* Real number */` |
|  ! 0 |  635 | `					pStream->zText++;` |
|  ! 0 |  636 | `					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  637 | `						pStream->zText++;` |
|  ! 0 |  638 | `					}` |
|  ! 0 |  639 | `					if( pStream->zText < pStream->zEnd ){` |
|  ! 0 |  640 | `						c = pStream->zText[0];` |
|  ! 0 |  641 | `						if( c=='e' \|\| c=='E' ){` |
|  ! 0 |  642 | `							pStream->zText++;` |
|  ! 0 |  643 | `							if( pStream->zText < pStream->zEnd ){` |
|  ! 0 |  644 | `								c = pStream->zText[0];` |
|  ! 0 |  645 | `								if( c =='+' \|\| c=='-' ){` |
|  ! 0 |  646 | `									pStream->zText++;` |
|  ! 0 |  647 | `								}` |
|  ! 0 |  648 | `								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  649 | `									pStream->zText++;` |
|  ! 0 |  650 | `								}` |
|  ! 0 |  651 | `							}` |
|  ! 0 |  652 | `						}` |
|  ! 0 |  653 | `					}` |
|   31 |  654 | `				}else if( c=='e' \|\| c=='E' ){` |
|    - |  655 | `					/* Real number */` |
|  ! 0 |  656 | `					pStream->zText++;` |
|  ! 0 |  657 | `					if( pStream->zText < pStream->zEnd ){` |
|  ! 0 |  658 | `						c = pStream->zText[0];` |
|  ! 0 |  659 | `						if( c =='+' \|\| c=='-' ){` |
|  ! 0 |  660 | `							pStream->zText++;` |
|  ! 0 |  661 | `						}` |
|  ! 0 |  662 | `						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){` |
|  ! 0 |  663 | `							pStream->zText++;` |
|  ! 0 |  664 | `						}` |
|  ! 0 |  665 | `					}` |
|  ! 0 |  666 | `				}` |
|   16 |  667 | `			}` |
|   37 |  668 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("true") -1 &&` |
|   12 |  669 | `		SyStrnicmp((const char *)pStream->zText,"true",sizeof("true")-1) == 0 ){` |
|    - |  670 | `			/* boolean true */` |
|  ! 0 |  671 | `			pToken->nType = JSON_TK_TRUE;` |
|    - |  672 | `			/* Advance the stream cursor */` |
|  ! 0 |  673 | `			pStream->zText += sizeof("true")-1;` |
|   22 |  674 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("false") -1 &&` |
|   12 |  675 | `		SyStrnicmp((const char *)pStream->zText,"false",sizeof("false")-1) == 0 ){` |
|    - |  676 | `			/* boolean false */` |
|  ! 0 |  677 | `			pToken->nType = JSON_TK_FALSE;` |
|    - |  678 | `			/* Advance the stream cursor */` |
|  ! 0 |  679 | `			pStream->zText += sizeof("false")-1;` |
|   22 |  680 | `	}else if( XLEX_IN_LEN(pStream) >= sizeof("null") -1 &&` |
|   12 |  681 | `		SyStrnicmp((const char *)pStream->zText,"null",sizeof("null")-1) == 0 ){` |
|    - |  682 | `			/* NULL */` |
|  ! 0 |  683 | `			pToken->nType = JSON_TK_NULL;` |
|    - |  684 | `			/* Advance the stream cursor */` |
|  ! 0 |  685 | `			pStream->zText += sizeof("null")-1;` |
|  ! 0 |  686 | `	}else{` |
|    - |  687 | `		/* Unexpected token */` |
|   16 |  688 | `		pToken->nType = JSON_TK_INVALID;` |
|    - |  689 | `		/* Advance the stream cursor */` |
|   16 |  690 | `		pStream->zText++;` |
|   16 |  691 | `		*pJsonErr = JSON_ERROR_SYNTAX;` |
|    - |  692 | `		/* Abort processing immediatley */` |
|   16 |  693 | `		return SXERR_ABORT;` |
|    - |  694 | `	}` |
|    - |  695 | `	/* record token length */` |
|  148 |  696 | `	pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);` |
|  148 |  697 | `	if( pToken->nType == JSON_TK_STR ){` |
|   26 |  698 | `		pStr->nByte--;` |
|   12 |  699 | `	}` |
|    - |  700 | `	/* Return to the lexer */` |
|  148 |  701 | `	return SXRET_OK;` |
|   82 |  702 | `}` |
|    - |  703 | `/*` |
|    - |  704 | ` * JSON decoded input consumer callback signature.` |
|    - |  705 | ` */` |
|    - |  706 | `typedef int (*ProcJsonConsumer)(ph7_context *,ph7_value *,ph7_value *,void *);` |
|    - |  707 | `/*` |
|    - |  708 | ` * JSON decoder state is kept in the following structure.` |
|    - |  709 | ` */` |
|    - |  710 | `typedef struct json_decoder json_decoder;` |
|    - |  711 | `struct json_decoder` |
|    - |  712 | `{` |
|    - |  713 | `	ph7_context *pCtx; /* Call context */` |
|    - |  714 | `	ProcJsonConsumer xConsumer; /* Consumer callback */` |
|    - |  715 | `	void *pUserData;   /* Last argument to xConsumer() */` |
|    - |  716 | `	int iFlags;        /* Configuration flags */` |
|    - |  717 | `	SyToken *pIn;      /* Token stream */` |
|    - |  718 | `	SyToken *pEnd;     /* End of the token stream */` |
|    - |  719 | `	int rec_depth;     /* Recursion limit */` |
|    - |  720 | `	int rec_count;     /* Current nesting level */` |
|    - |  721 | `	int *pErr;         /* JSON decoding error if any */` |
|    - |  722 | `};` |
|    - |  723 | `#define JSON_DECODE_ASSOC 0x01 /* Decode a JSON object as an associative array */` |
|    - |  724 | `/* Forward declaration */` |
|    - |  725 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData);` |
|    - |  726 | `/*` |
|    - |  727 | ` * Dequote [i.e: Resolve all backslash escapes ] a JSON string and store` |
|    - |  728 | ` * the result in the given ph7_value.` |
|    - |  729 | ` */` |
|   24 |  730 | `static void VmJsonDequoteString(const SyString *pStr,ph7_value *pWorker)` |
|    2 |  731 | `{` |
|   26 |  732 | `	const char *zIn = pStr->zString;` |
|   26 |  733 | `	const char *zEnd = &pStr->zString[pStr->nByte];` |
|    - |  734 | `	const char *zCur;` |
|    - |  735 | `	int c;` |
|    - |  736 | `	/* Mark the value as a string */` |
|   26 |  737 | `	ph7_value_string(pWorker,"",0); /* Empty string */` |
|   12 |  738 | `	for(;;){` |
|   26 |  739 | `		zCur = zIn;` |
|   72 |  740 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|   48 |  741 | `			zIn++;` |
|    2 |  742 | `		}` |
|   26 |  743 | `		if( zIn > zCur ){` |
|    - |  744 | `			/* Append chunk verbatim */` |
|   26 |  745 | `			ph7_value_string(pWorker,zCur,(int)(zIn-zCur));` |
|   12 |  746 | `		}` |
|   26 |  747 | `		zIn++;` |
|   26 |  748 | `		if( zIn >= zEnd ){` |
|    - |  749 | `			/* End of the input reached */` |
|   26 |  750 | `			break;` |
|    - |  751 | `		}` |
|  ! 0 |  752 | `		c = zIn[0];` |
|    - |  753 | `		/* Unescape the character */` |
|  ! 0 |  754 | `		switch(c){` |
|  ! 0 |  755 | `		case '"':  ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|  ! 0 |  756 | `		case '\\': ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char)); break;` |
|  ! 0 |  757 | `		case 'n':  ph7_value_string(pWorker,"\n",(int)sizeof(char)); break;` |
|  ! 0 |  758 | `		case 'r':  ph7_value_string(pWorker,"\r",(int)sizeof(char)); break;` |
|  ! 0 |  759 | `		case 't':  ph7_value_string(pWorker,"\t",(int)sizeof(char)); break;` |
|  ! 0 |  760 | `		case 'f':  ph7_value_string(pWorker,"\f",(int)sizeof(char)); break;` |
|  ! 0 |  761 | `		default:` |
|  ! 0 |  762 | `			ph7_value_string(pWorker,(const char *)&c,(int)sizeof(char));` |
|  ! 0 |  763 | `			break;` |
|    - |  764 | `		}` |
|    - |  765 | `		/* Advance the stream cursor */` |
|  ! 0 |  766 | `		zIn++;` |
|  ! 0 |  767 | `	}` |
|   26 |  768 | `}` |
|    - |  769 | `/*` |
|    - |  770 | ` * Returns a ph7_value holding the image of a JSON string. In other word perform a JSON decoding operation.` |
|    - |  771 | ` * According to wikipedia` |
|    - |  772 | ` * JSON's basic types are:` |
|    - |  773 | ` *   Number (double precision floating-point format in JavaScript, generally depends on implementation)` |
|    - |  774 | ` *   String (double-quoted Unicode, with backslash escaping)` |
|    - |  775 | ` *   Boolean (true or false)` |
|    - |  776 | ` *   Array (an ordered sequence of values, comma-separated and enclosed in square brackets; the values` |
|    - |  777 | ` *    do not need to be of the same type)` |
|    - |  778 | ` *   Object (an unordered collection of key:value pairs with the ':' character separating the key` |
|    - |  779 | ` *     and the value, comma-separated and enclosed in curly braces; the keys must be strings and should` |
|    - |  780 | ` *     be distinct from each other)` |
|    - |  781 | ` *   null (empty)` |
|    - |  782 | ` * Non-significant white space may be added freely around the "structural characters" (i.e. the brackets "[{]}", colon ":" and comma ",").` |
|    - |  783 | ` */` |
|   64 |  784 | `static sxi32 VmJsonDecode(` |
|    - |  785 | `	json_decoder *pDecoder, /* JSON decoder */` |
|    - |  786 | `	ph7_value *pArrayKey    /* Key for the decoded array */` |
|    2 |  787 | `	){` |
|    - |  788 | `	ph7_value *pWorker; /* Worker variable */` |
|    - |  789 | `	sxi32 rc;` |
|    - |  790 | `	/* Check if we do not nest to much */` |
|   66 |  791 | `	if( pDecoder->rec_count >= pDecoder->rec_depth ){` |
|    - |  792 | `		/* Nesting limit reached,abort decoding immediately */` |
|  ! 0 |  793 | `		*pDecoder->pErr = JSON_ERROR_DEPTH;` |
|  ! 0 |  794 | `		return SXERR_ABORT;` |
|    - |  795 | `	}` |
|   66 |  796 | `	if( pDecoder->pIn->nType & (JSON_TK_STR\|JSON_TK_TRUE\|JSON_TK_FALSE\|JSON_TK_NULL\|JSON_TK_NUM) ){` |
|    - |  797 | `		/* Scalar value */` |
|   38 |  798 | `		pWorker = ph7_context_new_scalar(pDecoder->pCtx);` |
|   38 |  799 | `		if( pWorker == 0 ){` |
|  ! 0 |  800 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |  801 | `			/* Abort the decoding operation immediately */` |
|  ! 0 |  802 | `			return SXERR_ABORT;` |
|    - |  803 | `		}` |
|    - |  804 | `		/* Reflect the JSON image */` |
|   38 |  805 | `		if( pDecoder->pIn->nType & JSON_TK_NULL ){` |
|    - |  806 | `			/* Nullify the value.*/` |
|  ! 0 |  807 | `			ph7_value_null(pWorker);` |
|   38 |  808 | `		}else if( pDecoder->pIn->nType & (JSON_TK_TRUE\|JSON_TK_FALSE) ){` |
|    - |  809 | `			/* Boolean value */` |
|  ! 0 |  810 | `			ph7_value_bool(pWorker,(pDecoder->pIn->nType & JSON_TK_TRUE) ? 1 : 0 );` |
|   38 |  811 | `		}else if( pDecoder->pIn->nType & JSON_TK_NUM ){` |
|   31 |  812 | `			SyString *pStr = &pDecoder->pIn->sData;` |
|    - |  813 | `			/*` |
|    - |  814 | `			 * Numeric value.` |
|    - |  815 | `			 * Get a string representation first then try to get a numeric` |
|    - |  816 | `			 * value.` |
|    - |  817 | `			 */` |
|   31 |  818 | `			ph7_value_string(pWorker,pStr->zString,(int)pStr->nByte);` |
|    - |  819 | `			/* Obtain a numeric representation */` |
|   31 |  820 | `			PH7_MemObjToNumeric(pWorker);` |
|   16 |  821 | `		}else{` |
|    - |  822 | `			/* Dequote the string */` |
|    8 |  823 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pWorker);` |
|    - |  824 | `		}` |
|    - |  825 | `		/* Invoke the consumer callback */` |
|   38 |  826 | `		rc = pDecoder->xConsumer(pDecoder->pCtx,pArrayKey,pWorker,pDecoder->pUserData);` |
|   38 |  827 | `		if( rc == SXERR_ABORT ){` |
|  ! 0 |  828 | `			return SXERR_ABORT;` |
|    - |  829 | `		}` |
|    - |  830 | `		/* All done,advance the stream cursor */` |
|   38 |  831 | `		pDecoder->pIn++;` |
|   48 |  832 | `	}else if( pDecoder->pIn->nType & JSON_TK_OSB /*'[' */) {` |
|    - |  833 | `		ProcJsonConsumer xOld;` |
|    - |  834 | `		void *pOld;` |
|    - |  835 | `		/* Array representation*/` |
|   13 |  836 | `		pDecoder->pIn++;` |
|    - |  837 | `		/* Create a working array */` |
|   13 |  838 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|   13 |  839 | `		if( pWorker == 0 ){` |
|  ! 0 |  840 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |  841 | `			/* Abort the decoding operation immediately */` |
|  ! 0 |  842 | `			return SXERR_ABORT;` |
|    - |  843 | `		}` |
|    - |  844 | `		/* Save the old consumer */` |
|   13 |  845 | `		xOld = pDecoder->xConsumer;` |
|   13 |  846 | `		pOld = pDecoder->pUserData;` |
|    - |  847 | `		/* Set the new consumer */` |
|   13 |  848 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|   13 |  849 | `		pDecoder->pUserData = pWorker;` |
|    - |  850 | `		/* Decode the array */` |
|   18 |  851 | `		for(;;){` |
|    - |  852 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|    - |  853 | `			 * do this.` |
|    - |  854 | `			 */` |
|   49 |  855 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|   13 |  856 | `				pDecoder->pIn++;` |
|    1 |  857 | `			}` |
|   37 |  858 | `			if( pDecoder->pIn >= pDecoder->pEnd \|\| (pDecoder->pIn->nType & JSON_TK_CSB) /*']'*/ ){` |
|   13 |  859 | `				if( pDecoder->pIn < pDecoder->pEnd ){` |
|   13 |  860 | `					pDecoder->pIn++; /* Jump the trailing ']' */` |
|    6 |  861 | `				}` |
|   13 |  862 | `				break;` |
|    - |  863 | `			}` |
|    - |  864 | `			/* Recurse and decode the entry */` |
|   25 |  865 | `			pDecoder->rec_count++;` |
|   25 |  866 | `			rc = VmJsonDecode(pDecoder,0);` |
|   25 |  867 | `			pDecoder->rec_count--;` |
|   25 |  868 | `			if( rc == SXERR_ABORT ){` |
|    - |  869 | `				/* Abort processing immediately */` |
|  ! 0 |  870 | `				return SXERR_ABORT;` |
|    - |  871 | `			}` |
|    - |  872 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|   25 |  873 | `			if( (pDecoder->pIn < pDecoder->pEnd) &&` |
|   24 |  874 | `				((pDecoder->pIn->nType & (JSON_TK_CSB/*']'*/\|JSON_TK_COMMA/*','*/))==0) ){` |
|    - |  875 | `					/* Unexpected token,abort immediatley */` |
|  ! 0 |  876 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|  ! 0 |  877 | `					return SXERR_ABORT;` |
|    - |  878 | `			}` |
|    1 |  879 | `		}` |
|    - |  880 | `		/* Restore the old consumer */` |
|   13 |  881 | `		pDecoder->xConsumer = xOld;` |
|   13 |  882 | `		pDecoder->pUserData = pOld;` |
|    - |  883 | `		/* Invoke the old consumer on the decoded array */` |
|   13 |  884 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|   24 |  885 | `	}else if( pDecoder->pIn->nType & JSON_TK_OCB /*'{' */) {` |
|    - |  886 | `		ProcJsonConsumer xOld;` |
|    - |  887 | `		ph7_value *pKey;` |
|    - |  888 | `		void *pOld;` |
|    - |  889 | `		/* Object representation*/` |
|   18 |  890 | `		pDecoder->pIn++;` |
|    - |  891 | `		/* Return the object as an associative array */` |
|   18 |  892 | `		if( (pDecoder->iFlags & JSON_DECODE_ASSOC) == 0 ){` |
|    3 |  893 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_WARNING,` |
|    - |  894 | `				"JSON Objects are always returned as an associative array"` |
|    - |  895 | `				);` |
|    1 |  896 | `		}` |
|    - |  897 | `		/* Create a working array */` |
|   18 |  898 | `		pWorker = ph7_context_new_array(pDecoder->pCtx);` |
|   18 |  899 | `		pKey = ph7_context_new_scalar(pDecoder->pCtx);` |
|   18 |  900 | `		if( pWorker == 0 \|\| pKey == 0){` |
|  ! 0 |  901 | `			ph7_context_throw_error(pDecoder->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    - |  902 | `			/* Abort the decoding operation immediately */` |
|  ! 0 |  903 | `			return SXERR_ABORT;` |
|    - |  904 | `		}` |
|    - |  905 | `		/* Save the old consumer */` |
|   18 |  906 | `		xOld = pDecoder->xConsumer;` |
|   18 |  907 | `		pOld = pDecoder->pUserData;` |
|    - |  908 | `		/* Set the new consumer */` |
|   18 |  909 | `		pDecoder->xConsumer = VmJsonArrayDecoder;` |
|   18 |  910 | `		pDecoder->pUserData = pWorker;` |
|    - |  911 | `		/* Decode the object */` |
|   17 |  912 | `		for(;;){` |
|    - |  913 | `			/* Jump trailing comma. Note that the standard PHP engine will not let you` |
|    - |  914 | `			 * do this.` |
|    - |  915 | `			 */` |
|   40 |  916 | `			while( (pDecoder->pIn < pDecoder->pEnd) && (pDecoder->pIn->nType & JSON_TK_COMMA) ){` |
|    5 |  917 | `				pDecoder->pIn++;` |
|    1 |  918 | `			}` |
|   36 |  919 | `			if( pDecoder->pIn >= pDecoder->pEnd \|\| (pDecoder->pIn->nType & JSON_TK_CCB) /*'}'*/ ){` |
|   18 |  920 | `				if( pDecoder->pIn < pDecoder->pEnd ){` |
|   16 |  921 | `					pDecoder->pIn++; /* Jump the trailing ']' */` |
|    7 |  922 | `				}` |
|   18 |  923 | `				break;` |
|    - |  924 | `			}` |
|   18 |  925 | `			if( (pDecoder->pIn->nType & JSON_TK_STR) == 0 \|\| &pDecoder->pIn[1] >= pDecoder->pEnd` |
|   20 |  926 | `				\|\| (pDecoder->pIn[1].nType & JSON_TK_COLON) == 0){` |
|    - |  927 | `					/* Syntax error,return immediately */` |
|  ! 0 |  928 | `					*pDecoder->pErr = JSON_ERROR_SYNTAX;` |
|  ! 0 |  929 | `					return SXERR_ABORT;` |
|    - |  930 | `			}` |
|    - |  931 | `			/* Dequote the key */` |
|   20 |  932 | `			VmJsonDequoteString(&pDecoder->pIn->sData,pKey);` |
|    - |  933 | `			/* Jump the key and the colon */` |
|   20 |  934 | `			pDecoder->pIn += 2;` |
|    - |  935 | `			/* Recurse and decode the value */` |
|   20 |  936 | `			pDecoder->rec_count++;` |
|   20 |  937 | `			rc = VmJsonDecode(pDecoder,pKey);` |
|   20 |  938 | `			pDecoder->rec_count--;` |
|   20 |  939 | `			if( rc == SXERR_ABORT ){` |
|    - |  940 | `				/* Abort processing immediately */` |
|  ! 0 |  941 | `				return SXERR_ABORT;` |
|    - |  942 | `			}` |
|    - |  943 | `			/* Reset the internal buffer of the key */` |
|   20 |  944 | `			ph7_value_reset_string_cursor(pKey);` |
|    - |  945 | `			/*The cursor is automatically advanced by the VmJsonDecode() function */` |
|    2 |  946 | `		}` |
|    - |  947 | `		/* Restore the old consumer */` |
|   18 |  948 | `		pDecoder->xConsumer = xOld;` |
|   18 |  949 | `		pDecoder->pUserData = pOld;` |
|    - |  950 | `		/* Invoke the old consumer on the decoded object*/` |
|   18 |  951 | `		xOld(pDecoder->pCtx,pArrayKey,pWorker,pOld);` |
|    - |  952 | `		/* Release the key */` |
|   18 |  953 | `		ph7_context_release_value(pDecoder->pCtx,pKey);` |
|   10 |  954 | `	}else{` |
|    - |  955 | `		/* Unexpected token */` |
|  ! 0 |  956 | `		return SXERR_ABORT; /* Abort immediately */` |
|    - |  957 | `	}` |
|    - |  958 | `	/* Release the worker variable */` |
|   66 |  959 | `	ph7_context_release_value(pDecoder->pCtx,pWorker);` |
|   66 |  960 | `	return SXRET_OK;` |
|   34 |  961 | `}` |
|    - |  962 | `/*` |
|    - |  963 | ` * The following JSON decoder callback is invoked each time` |
|    - |  964 | ` * a JSON array representation [i.e: [15,"hello",FALSE] ]` |
|    - |  965 | ` * is being decoded.` |
|    - |  966 | ` */` |
|   42 |  967 | `static int VmJsonArrayDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|    2 |  968 | `{` |
|   44 |  969 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|    - |  970 | `	/* Insert the entry */` |
|   44 |  971 | `	ph7_array_add_elem(pArray,pKey,pWorker); /* Will make it's own copy */` |
|   21 |  972 | `	SXUNUSED(pCtx); /* cc warning */` |
|    - |  973 | `	/* All done */` |
|   44 |  974 | `	return SXRET_OK;` |
|    2 |  975 | `}` |
|    - |  976 | `/*` |
|    - |  977 | ` * Standard JSON decoder callback.` |
|    - |  978 | ` */` |
|   22 |  979 | `static int VmJsonDefaultDecoder(ph7_context *pCtx,ph7_value *pKey,ph7_value *pWorker,void *pUserData)` |
|    2 |  980 | `{` |
|    - |  981 | `	/* Return the value directly */` |
|   24 |  982 | `	ph7_result_value(pCtx,pWorker); /* Will make it's own copy */` |
|   11 |  983 | `	SXUNUSED(pKey); /* cc warning */` |
|   11 |  984 | `	SXUNUSED(pUserData);` |
|    - |  985 | `	/* All done */` |
|   24 |  986 | `	return SXRET_OK;` |
|    2 |  987 | `}` |
|    - |  988 | `/*` |
|    - |  989 | ` * mixed json_decode(string $json[,bool $assoc = false[,int $depth = 32[,int $options = 0 ]]])` |
|    - |  990 | ` *  Takes a JSON encoded string and converts it into a PHP variable.` |
|    - |  991 | ` * Parameters` |
|    - |  992 | ` *  $json` |
|    - |  993 | ` *    The json string being decoded.` |
|    - |  994 | ` * $assoc` |
|    - |  995 | ` *   When TRUE, returned objects will be converted into associative arrays.` |
|    - |  996 | ` * $depth` |
|    - |  997 | ` *   User specified recursion depth.` |
|    - |  998 | ` * $options` |
|    - |  999 | ` *   Bitmask of JSON decode options. Currently only JSON_BIGINT_AS_STRING is supported` |
|    - | 1000 | ` * (default is to cast large integers as floats)` |
|    - | 1001 | ` * Return` |
|    - | 1002 | ` *  The value encoded in json in appropriate PHP type. Values true, false and null (case-insensitive)` |
|    - | 1003 | ` *  are returned as TRUE, FALSE and NULL respectively. NULL is returned if the json cannot be decoded` |
|    - | 1004 | ` *  or if the encoded data is deeper than the recursion limit.` |
|    - | 1005 | ` */` |
|    - | 1006 | `/*` |
|    - | 1007 | ` * Tokenize and decode a JSON input. Shared core of json_decode() and json_validate().` |
|    - | 1008 | ` * On success the decoded value is delivered through the default decoder (i.e: it becomes` |
|    - | 1009 | ` * the call-context result, which json_validate's caller then overwrites with a boolean).` |
|    - | 1010 | ` * Returns the resulting JSON error code (pVm->json_rc): JSON_ERROR_NONE on success, a` |
|    - | 1011 | ` * non-zero json_err_code otherwise. A generic decoder abort without a specific code` |
|    - | 1012 | ` * (e.g: out of memory) is reported as JSON_ERROR_SYNTAX so callers can branch on a single` |
|    - | 1013 | ` * value, preserving the original "abort \|\| error => failure" json_decode semantics.` |
|    - | 1014 | ` */` |
|   36 | 1015 | `static int VmJsonDecodeInput(ph7_context *pCtx,const char *zIn,int nByte,int iAssoc,int nDepth)` |
|    2 | 1016 | `{` |
|   38 | 1017 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - | 1018 | `	json_decoder sDecoder;` |
|    - | 1019 | `	SySet sToken;` |
|    - | 1020 | `	SyLex sLex;` |
|    - | 1021 | `	sxi32 rc;` |
|    - | 1022 | `	/* Clear JSON error code */` |
|   38 | 1023 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|    - | 1024 | `	/* Tokenize the input */` |
|   38 | 1025 | `	SySetInit(&sToken,&pVm->sAllocator,sizeof(SyToken));` |
|   38 | 1026 | `	SyLexInit(&sLex,&sToken,VmJsonTokenize,&pVm->json_rc);` |
|   38 | 1027 | `	SyLexTokenizeInput(&sLex,zIn,(sxu32)nByte,0,0,0);` |
|   38 | 1028 | `	if( pVm->json_rc != JSON_ERROR_NONE ){` |
|    - | 1029 | `		/* Something goes wrong while tokenizing input. [i.e: Unexpected token] */` |
|   16 | 1030 | `		SyLexRelease(&sLex);` |
|   16 | 1031 | `		SySetRelease(&sToken);` |
|   16 | 1032 | `		return pVm->json_rc;` |
|    - | 1033 | `	}` |
|    - | 1034 | `	/* Fill the decoder */` |
|   24 | 1035 | `	sDecoder.pCtx = pCtx;` |
|   24 | 1036 | `	sDecoder.pErr = &pVm->json_rc;` |
|   24 | 1037 | `	sDecoder.pIn = (SyToken *)SySetBasePtr(&sToken);` |
|   24 | 1038 | `	sDecoder.pEnd = &sDecoder.pIn[SySetUsed(&sToken)];` |
|   24 | 1039 | `	sDecoder.iFlags = 0;` |
|   24 | 1040 | `	if( iAssoc ){` |
|    - | 1041 | `		/* Returned objects will be converted into associative arrays */` |
|   22 | 1042 | `		sDecoder.iFlags \|= JSON_DECODE_ASSOC;` |
|   10 | 1043 | `	}` |
|   24 | 1044 | `	sDecoder.rec_depth = 32;` |
|   24 | 1045 | `	if( nDepth > 1 && nDepth < 32 ){` |
|    3 | 1046 | `		sDecoder.rec_depth = nDepth;` |
|    1 | 1047 | `	}` |
|   24 | 1048 | `	sDecoder.rec_count = 0;` |
|    - | 1049 | `	/* Set a default consumer */` |
|   24 | 1050 | `	sDecoder.xConsumer = VmJsonDefaultDecoder;` |
|   24 | 1051 | `	sDecoder.pUserData = 0;` |
|    - | 1052 | `	/* Decode the raw JSON input */` |
|   24 | 1053 | `	rc = VmJsonDecode(&sDecoder,0);` |
|   24 | 1054 | `	if( rc == SXERR_ABORT && pVm->json_rc == JSON_ERROR_NONE ){` |
|    - | 1055 | `		/* Generic abort with no specific code: treat as a syntax error */` |
|  ! 0 | 1056 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|  ! 0 | 1057 | `	}` |
|    - | 1058 | `	/* Clean-up the mess left behind */` |
|   24 | 1059 | `	SyLexRelease(&sLex);` |
|   24 | 1060 | `	SySetRelease(&sToken);` |
|   24 | 1061 | `	return pVm->json_rc;` |
|   20 | 1062 | `}` |
|   38 | 1063 | `PH7_PRIVATE int vm_builtin_json_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 1064 | `{` |
|    - | 1065 | `	const char *zIn;` |
|    - | 1066 | `	int nByte;` |
|   40 | 1067 | `	int iAssoc = 0;` |
|   40 | 1068 | `	int nDepth = 32;` |
|   40 | 1069 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - | 1070 | `		/* Missing/Invalid arguments, return NULL */` |
|  ! 0 | 1071 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1072 | `		return PH7_OK;` |
|    - | 1073 | `	}` |
|    - | 1074 | `	/* Extract the JSON string */` |
|   40 | 1075 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   40 | 1076 | `	if( nByte < 1 ){` |
|    - | 1077 | `		/* Empty string,return NULL */` |
|    6 | 1078 | `		ph7_result_null(pCtx);` |
|    6 | 1079 | `		return PH7_OK;` |
|    - | 1080 | `	}` |
|   36 | 1081 | `	if( nArg > 1 && ph7_value_to_bool(apArg[1]) != 0 ){` |
|   24 | 1082 | `		iAssoc = 1;` |
|   11 | 1083 | `	}` |
|   36 | 1084 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|    - | 1085 | `		/* PHP 8: $depth must be in 1 .. INT_MAX (a catchable ValueError otherwise);` |
|    - | 1086 | `		 * read as int64 so a value above INT_MAX is detected, not truncated. */` |
|   13 | 1087 | `		ph7_int64 nWant = ph7_value_to_int64(apArg[2]);` |
|    - | 1088 | `		/* php clears the json error state before validating $depth, so a caught` |
|    - | 1089 | `		 * depth ValueError leaves json_last_error() == JSON_ERROR_NONE (the normal` |
|    - | 1090 | `		 * path resets it again inside VmJsonDecodeInput). */` |
|   13 | 1091 | `		pCtx->pVm->json_rc = JSON_ERROR_NONE;` |
|   13 | 1092 | `		if( nWant <= 0 ){` |
|    9 | 1093 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1094 | `				"json_decode(): Argument #3 ($depth) must be greater than 0");` |
|    - | 1095 | `		}` |
|    5 | 1096 | `		if( nWant > 2147483647 ){` |
|    3 | 1097 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1098 | `				"json_decode(): Argument #3 ($depth) must be less than 2147483647");` |
|    - | 1099 | `		}` |
|    3 | 1100 | `		nDepth = (int)nWant;` |
|    1 | 1101 | `	}` |
|    - | 1102 | `	/* Decode the raw JSON input.The default consumer sets the decoded value as the` |
|    - | 1103 | `	 * call-context result; on failure we replace it with NULL. */` |
|   26 | 1104 | `	if( VmJsonDecodeInput(pCtx,zIn,nByte,iAssoc,nDepth) != JSON_ERROR_NONE ){` |
|    - | 1105 | `		/* Something goes wrong while decoding JSON input.Return NULL. */` |
|   12 | 1106 | `		ph7_result_null(pCtx);` |
|    5 | 1107 | `	}` |
|    - | 1108 | `	/* All done */` |
|   26 | 1109 | `	return PH7_OK;` |
|   21 | 1110 | `}` |
|    - | 1111 | `/*` |
|    - | 1112 | ` * bool json_validate(string $json[,int $depth = 512[,int $flags = 0]])` |
|    - | 1113 | ` *  Validates whether a string is valid JSON without materializing a value.` |
|    - | 1114 | ` * Parameters` |
|    - | 1115 | ` *  $json   The string to validate.` |
|    - | 1116 | ` *  $depth  Maximum nesting depth (clamped to the engine limit of 32).` |
|    - | 1117 | ` *  $flags  Bitmask of decode options (currently none are implemented; accepted/ignored).` |
|    - | 1118 | ` * Return` |
|    - | 1119 | ` *  TRUE if the string is valid JSON, FALSE otherwise. Updates json_last_error().` |
|    - | 1120 | ` */` |
|   20 | 1121 | `PH7_PRIVATE int vm_builtin_json_validate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1122 | `{` |
|   21 | 1123 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - | 1124 | `	const char *zIn;` |
|    - | 1125 | `	int nByte;` |
|   21 | 1126 | `	int nDepth = 32;` |
|   21 | 1127 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - | 1128 | `		/* Missing/Invalid argument: not valid JSON */` |
|  ! 0 | 1129 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|  ! 0 | 1130 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1131 | `		return PH7_OK;` |
|    - | 1132 | `	}` |
|    - | 1133 | `	/* Extract the JSON string */` |
|   21 | 1134 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   21 | 1135 | `	if( nByte < 1 ){` |
|    - | 1136 | `		/* The empty string is not valid JSON (unlike json_decode, which returns NULL` |
|    - | 1137 | `		 * silently, json_validate must record the syntax error) */` |
|    3 | 1138 | `		pVm->json_rc = JSON_ERROR_SYNTAX;` |
|    3 | 1139 | `		ph7_result_bool(pCtx,0);` |
|    3 | 1140 | `		return PH7_OK;` |
|    - | 1141 | `	}` |
|   19 | 1142 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|    - | 1143 | `		/* PHP 8: $depth must be in 1 .. INT_MAX (a catchable ValueError otherwise). */` |
|    9 | 1144 | `		ph7_int64 nWant = ph7_value_to_int64(apArg[1]);` |
|    - | 1145 | `		/* Clear the json error state before validating $depth (php parity), so a` |
|    - | 1146 | `		 * caught depth ValueError leaves json_last_error() == JSON_ERROR_NONE. */` |
|    9 | 1147 | `		pVm->json_rc = JSON_ERROR_NONE;` |
|    9 | 1148 | `		if( nWant <= 0 ){` |
|    5 | 1149 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1150 | `				"json_validate(): Argument #2 ($depth) must be greater than 0");` |
|    - | 1151 | `		}` |
|    5 | 1152 | `		if( nWant > 2147483647 ){` |
|    3 | 1153 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1154 | `				"json_validate(): Argument #2 ($depth) must be less than 2147483647");` |
|    - | 1155 | `		}` |
|    3 | 1156 | `		nDepth = (int)nWant;` |
|    1 | 1157 | `	}` |
|    - | 1158 | `	/* apArg[2] ($flags) is accepted and ignored: no decode flag is implemented.` |
|    - | 1159 | `	 * Decode in associative mode so the "objects are returned as an array" warning is` |
|    - | 1160 | `	 * not raised - the decoded value is discarded, only its validity matters. */` |
|   13 | 1161 | `	ph7_result_bool(pCtx,VmJsonDecodeInput(pCtx,zIn,nByte,1,nDepth) == JSON_ERROR_NONE);` |
|   13 | 1162 | `	return PH7_OK;` |
|   11 | 1163 | `}` |
|    - | 1164 |  |
