# src/ph7/vm_builtin_class.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 509/587 lines (86.71%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|     - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    5 | ` */` |
|     - |    6 | `#include "ph7int.h"` |
|   568 |    7 | `PH7_PRIVATE int vm_builtin_get_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |    8 | `{` |
|     - |    9 | `	ph7_class *pClass;` |
|     - |   10 | `	SyString *pName;` |
|   573 |   11 | `	if( nArg < 1 ){` |
|     - |   12 | `		/* Check if we are inside a class */` |
|   ! 0 |   13 | `		pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|   ! 0 |   14 | `		if( pClass ){` |
|     - |   15 | `			/* Point to the class name */` |
|   ! 0 |   16 | `			pName = &pClass->sName;` |
|   ! 0 |   17 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|   ! 0 |   18 | `		}else{` |
|     - |   19 | `			/* Not inside class,return FALSE */` |
|   ! 0 |   20 | `			ph7_result_bool(pCtx,0);` |
|     - |   21 | `		}` |
|   ! 0 |   22 | `	}else{` |
|     - |   23 | `		/* Extract the target class */` |
|   573 |   24 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|   573 |   25 | `		if( pClass ){` |
|   571 |   26 | `			pName = &pClass->sName;` |
|     - |   27 | `			/* Return the class name */` |
|   571 |   28 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|   288 |   29 | `		}else{` |
|     - |   30 | `			/* Not a class instance,return FALSE */` |
|     3 |   31 | `			ph7_result_bool(pCtx,0);` |
|     - |   32 | `		}` |
|     - |   33 | `	}` |
|   573 |   34 | `	return PH7_OK;` |
|     5 |   35 | `}` |
|     - |   36 | `/*` |
|     - |   37 | ` * string get_parent_class([object $object = NULL ] )` |
|     - |   38 | ` *   Returns the name of the parent class of an object` |
|     - |   39 | ` * Parameters` |
|     - |   40 | ` *  object` |
|     - |   41 | ` *   The tested object. This parameter may be omitted when inside a class.` |
|     - |   42 | ` * Return` |
|     - |   43 | ` *  The name of the parent class of which object is an instance.` |
|     - |   44 | ` *  Returns FALSE if object is not an object or if the object does` |
|     - |   45 | ` *  not have a parent.` |
|     - |   46 | ` *  If object is omitted when inside a class, the name of that class is returned.` |
|     - |   47 | ` */` |
|    38 |   48 | `PH7_PRIVATE int vm_builtin_get_parent_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |   49 | `{` |
|     - |   50 | `	ph7_class *pClass;` |
|     - |   51 | `	SyString *pName;` |
|    40 |   52 | `	if( nArg < 1 ){` |
|     - |   53 | `		/* Check if we are inside a class [i.e: a method call]*/` |
|     3 |   54 | `		pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|     3 |   55 | `		if( pClass && pClass->pBase ){` |
|     - |   56 | `			/* Point to the class name */` |
|     3 |   57 | `			pName = &pClass->pBase->sName;` |
|     3 |   58 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|     2 |   59 | `		}else{` |
|     - |   60 | `			/* Not inside class,return FALSE */` |
|   ! 0 |   61 | `			ph7_result_bool(pCtx,0);` |
|     - |   62 | `		}` |
|     2 |   63 | `	}else{` |
|     - |   64 | `		/* Extract the target class */` |
|    38 |   65 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|    38 |   66 | `		if( pClass ){` |
|    38 |   67 | `			if( pClass->pBase ){` |
|    36 |   68 | `				pName = &pClass->pBase->sName;` |
|     - |   69 | `				/* Return the parent class name */` |
|    36 |   70 | `				ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|    19 |   71 | `			}else{` |
|     - |   72 | `				/* Object does not have a parent class */` |
|     3 |   73 | `				ph7_result_bool(pCtx,0);` |
|     - |   74 | `			}` |
|    20 |   75 | `		}else{` |
|     - |   76 | `			/* Not a class instance,return FALSE */` |
|   ! 0 |   77 | `			ph7_result_bool(pCtx,0);` |
|     - |   78 | `		}` |
|     - |   79 | `	}` |
|    40 |   80 | `	return PH7_OK;` |
|     2 |   81 | `}` |
|     - |   82 | `/*` |
|     - |   83 | ` * string get_called_class(void)` |
|     - |   84 | ` *   Gets the name of the class the static method is called in.` |
|     - |   85 | ` * Parameters` |
|     - |   86 | ` *  None.` |
|     - |   87 | ` * Return` |
|     - |   88 | ` *  Returns the class name. Returns FALSE if called from outside a class.` |
|     - |   89 | ` */` |
|     4 |   90 | `PH7_PRIVATE int vm_builtin_get_called_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |   91 | `{` |
|     - |   92 | `	ph7_class *pClass;` |
|     - |   93 | `	/* Check if we are inside a class [i.e: a method call] */` |
|     5 |   94 | `	pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|     5 |   95 | `	if( pClass ){` |
|     - |   96 | `		SyString *pName;` |
|     - |   97 | `		/* Point to the class name */` |
|     5 |   98 | `		pName = &pClass->sName;` |
|     5 |   99 | `		ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|     3 |  100 | `	}else{` |
|   ! 0 |  101 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  102 | `		SXUNUSED(apArg);` |
|     - |  103 | `		/* Not inside class,return FALSE */` |
|   ! 0 |  104 | `		ph7_result_bool(pCtx,0);` |
|     - |  105 | `	}` |
|     5 |  106 | `	return PH7_OK;` |
|     1 |  107 | `}` |
|     - |  108 | `/*` |
|     - |  109 | ` * Extract a ph7_class from the given ph7_value.` |
|     - |  110 | ` * The given value must be of type object [i.e: class instance] or` |
|     - |  111 | ` * string which hold the class name.` |
|     - |  112 | ` */` |
|  2192 |  113 | `PH7_PRIVATE ph7_class * PH7_VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg)` |
|     5 |  114 | `{` |
|  2197 |  115 | `	ph7_class *pClass = 0;` |
|  2197 |  116 | `	if( ph7_value_is_object(pArg) ){` |
|     - |  117 | `		/* Class instance already loaded,no need to perform a lookup */` |
|   679 |  118 | `		pClass = ((ph7_class_instance *)pArg->x.pOther)->pClass;` |
|  1858 |  119 | `	}else if( ph7_value_is_string(pArg) ){` |
|     - |  120 | `		const char *zClass;` |
|     - |  121 | `		int nLen;` |
|     - |  122 | `		/* Extract class name */` |
|  1518 |  123 | `		zClass = ph7_value_to_string(pArg,&nLen);` |
|  1518 |  124 | `		if( nLen > 0 ){` |
|     - |  125 | `			SyHashEntry *pEntry;` |
|     - |  126 | `			/* Perform a lookup */` |
|  1518 |  127 | `			pEntry = SyHashGet(&pVm->hClass,(const void *)zClass,(sxu32)nLen);` |
|  1518 |  128 | `			if( pEntry ){` |
|     - |  129 | `				/* Point to the desired class */` |
|  1498 |  130 | `				pClass = (ph7_class *)pEntry->pUserData;` |
|   748 |  131 | `			}` |
|   758 |  132 | `		}` |
|   758 |  133 | `	}` |
|  2197 |  134 | `	return pClass;` |
|     5 |  135 | `}` |
|     - |  136 | `/*` |
|     - |  137 | ` * bool property_exists(mixed $class,string $property)` |
|     - |  138 | ` *   Checks if the object or class has a property.` |
|     - |  139 | ` * Parameters` |
|     - |  140 | ` *  class` |
|     - |  141 | ` *   The class name or an object of the class to test for` |
|     - |  142 | ` * property` |
|     - |  143 | ` *  The name of the property` |
|     - |  144 | ` * Return` |
|     - |  145 | ` *   Returns TRUE if the property exists,FALSE otherwise.` |
|     - |  146 | ` */` |
|    16 |  147 | `PH7_PRIVATE int vm_builtin_property_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  148 | `{` |
|    17 |  149 | `	int res = 0; /* Assume attribute does not exists */` |
|    17 |  150 | `	if( nArg > 1 ){` |
|     - |  151 | `		ph7_class *pClass;` |
|    17 |  152 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|    17 |  153 | `		if( pClass ){` |
|     - |  154 | `			const char *zName;` |
|     - |  155 | `			int nLen;` |
|     - |  156 | `			/* Extract attribute name */` |
|    17 |  157 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|    17 |  158 | `			if( nLen > 0 ){` |
|     - |  159 | `				/* Perform the lookup in the attribute and method table */` |
|    16 |  160 | `				if( SyHashGet(&pClass->hAttr,(const void *)zName,(sxu32)nLen) != 0` |
|    11 |  161 | `					\|\| SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){` |
|     - |  162 | `						/* property exists,flag that */` |
|    13 |  163 | `						res = 1;` |
|     6 |  164 | `				}` |
|     - |  165 | `				/* A DYNAMIC (runtime-added) property lives on the INSTANCE's` |
|     - |  166 | `				 * attribute table, not the class's — php reports those too` |
|     - |  167 | `				 * (band A #3b; pre-fix property_exists() was blind to them). */` |
|    17 |  168 | `				if( res == 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|     3 |  169 | `					ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     3 |  170 | `					if( pThis && SyHashGet(&pThis->hAttr,(const void *)zName,(sxu32)nLen) != 0 ){` |
|   ! 0 |  171 | `						res = 1;` |
|   ! 0 |  172 | `					}` |
|     1 |  173 | `				}` |
|     8 |  174 | `			}` |
|     8 |  175 | `		}` |
|     8 |  176 | `	}` |
|    17 |  177 | `	ph7_result_bool(pCtx,res);` |
|    17 |  178 | `	return PH7_OK;` |
|     1 |  179 | `}` |
|     - |  180 | `/*` |
|     - |  181 | ` * bool method_exists(mixed $class,string $method)` |
|     - |  182 | ` *   Checks if the given method is a class member.` |
|     - |  183 | ` * Parameters` |
|     - |  184 | ` *  class` |
|     - |  185 | ` *   The class name or an object of the class to test for` |
|     - |  186 | ` * property` |
|     - |  187 | ` *  The name of the method` |
|     - |  188 | ` * Return` |
|     - |  189 | ` *   Returns TRUE if the method exists,FALSE otherwise.` |
|     - |  190 | ` */` |
|     4 |  191 | `PH7_PRIVATE int vm_builtin_method_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  192 | `{` |
|     5 |  193 | `	int res = 0; /* Assume method does not exists */` |
|     5 |  194 | `	if( nArg > 1 ){` |
|     - |  195 | `		ph7_class *pClass;` |
|     5 |  196 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|     5 |  197 | `		if( pClass ){` |
|     - |  198 | `			const char *zName;` |
|     - |  199 | `			int nLen;` |
|     - |  200 | `			/* Extract method name */` |
|     5 |  201 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|     5 |  202 | `			if( nLen > 0 ){` |
|     - |  203 | `				/* Perform the lookup in the method table */` |
|     5 |  204 | `				if( SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){` |
|     - |  205 | `					/* method exists,flag that */` |
|     3 |  206 | `					res = 1;` |
|     1 |  207 | `				}` |
|     2 |  208 | `			}` |
|     2 |  209 | `		}` |
|     2 |  210 | `	}` |
|     5 |  211 | `	ph7_result_bool(pCtx,res);` |
|     5 |  212 | `	return PH7_OK;` |
|     1 |  213 | `}` |
|     - |  214 | `/*` |
|     - |  215 | ` * bool class_exists(string $class_name [, bool $autoload = true ] )` |
|     - |  216 | ` *   Checks if the class has been defined.` |
|     - |  217 | ` * Parameters` |
|     - |  218 | ` *  class_name` |
|     - |  219 | ` *   The class name. The name is matched in a case-sensitive manner` |
|     - |  220 | ` *   unlinke the standard PHP engine.` |
|     - |  221 | ` *  autoload` |
|     - |  222 | ` *   Whether or not to call __autoload by default.` |
|     - |  223 | ` * Return` |
|     - |  224 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|     - |  225 | ` */` |
|    66 |  226 | `PH7_PRIVATE int vm_builtin_class_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |  227 | `{` |
|    71 |  228 | `	int res = 0; /* Assume class does not exist */` |
|    71 |  229 | `	if( nArg > 0 ){` |
|    71 |  230 | `		SyHashEntry *pEntry = 0;` |
|     - |  231 | `		const char *zName;` |
|     - |  232 | `		int nLen;` |
|    71 |  233 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|     - |  234 | `		/* Extract given name */` |
|    71 |  235 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|    71 |  236 | `		if( nArg >= 2 ){` |
|     6 |  237 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|     2 |  238 | `		}` |
|    71 |  239 | `		if( nLen > 0 ){` |
|     - |  240 | `			/* Perform a hash lookup first */` |
|    71 |  241 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|    33 |  242 | `		}` |
|    71 |  243 | `		if( pEntry == 0 && nLen > 0 && iAutoload ){` |
|     - |  244 | `			/* Try autoload, then re-check */` |
|    21 |  245 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,(sxu32)nLen,FALSE);` |
|    21 |  246 | `			if( pClass ){` |
|     6 |  247 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|     2 |  248 | `			}` |
|     8 |  249 | `		}` |
|    71 |  250 | `		if( pEntry ){` |
|     - |  251 | `			/* Walk the collision chain: return TRUE only for concrete or abstract classes,` |
|     - |  252 | `			 * not for interfaces or traits (matching PHP behavior). */` |
|    56 |  253 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|    56 |  254 | `			while( pClass ){` |
|    56 |  255 | `				if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0 ){` |
|    56 |  256 | `					res = 1;` |
|    56 |  257 | `					break;` |
|     - |  258 | `				}` |
|   ! 0 |  259 | `				pClass = pClass->pNextName;` |
|   ! 0 |  260 | `			}` |
|    26 |  261 | `		}` |
|    33 |  262 | `	}` |
|    71 |  263 | `	ph7_result_bool(pCtx,res);` |
|    71 |  264 | `	return PH7_OK;` |
|     5 |  265 | `}` |
|     - |  266 | `/*` |
|     - |  267 | ` * bool interface_exists(string $class_name [, bool $autoload = true ] )` |
|     - |  268 | ` *   Checks if the interface has been defined.` |
|     - |  269 | ` * Parameters` |
|     - |  270 | ` *  class_name` |
|     - |  271 | ` *   The class name. The name is matched in a case-sensitive manner` |
|     - |  272 | ` *   unlinke the standard PHP engine.` |
|     - |  273 | ` *  autoload` |
|     - |  274 | ` *   Whether or not to call __autoload by default.` |
|     - |  275 | ` * Return` |
|     - |  276 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|     - |  277 | ` */` |
|    24 |  278 | `PH7_PRIVATE int vm_builtin_interface_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  279 | `{` |
|    25 |  280 | `	int res = 0; /* Assume interface does not exist */` |
|    25 |  281 | `	if( nArg > 0 ){` |
|    25 |  282 | `		SyHashEntry *pEntry = 0;` |
|     - |  283 | `		const char *zName;` |
|     - |  284 | `		int nLen;` |
|    25 |  285 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|     - |  286 | `		/* Extract given name */` |
|    25 |  287 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|    25 |  288 | `		if( nArg >= 2 ){` |
|   ! 0 |  289 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|   ! 0 |  290 | `		}` |
|     - |  291 | `		/* Perform a hash lookup */` |
|    25 |  292 | `		if( nLen > 0 ){` |
|    25 |  293 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|    12 |  294 | `		}` |
|    25 |  295 | `		if( pEntry == 0 && nLen > 0 && iAutoload ){` |
|     - |  296 | `			/* Try autoload — pass iLoadable=FALSE so we get interfaces too */` |
|     3 |  297 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,(sxu32)nLen,FALSE);` |
|     3 |  298 | `			if( pClass ){` |
|   ! 0 |  299 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|   ! 0 |  300 | `			}` |
|     1 |  301 | `		}` |
|    25 |  302 | `		if( pEntry ){` |
|    23 |  303 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|    23 |  304 | `			while( pClass ){` |
|    23 |  305 | `				if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|     - |  306 | `					/* interface is available */` |
|    23 |  307 | `					res = 1;` |
|    23 |  308 | `					break;` |
|     - |  309 | `				}` |
|     - |  310 | `				/* Next with the same name */` |
|   ! 0 |  311 | `				pClass = pClass->pNextName;` |
|   ! 0 |  312 | `			}` |
|    11 |  313 | `		}` |
|    12 |  314 | `	}` |
|    25 |  315 | `	ph7_result_bool(pCtx,res);` |
|    25 |  316 | `	return PH7_OK;` |
|     1 |  317 | `}` |
|     - |  318 | `/*` |
|     - |  319 | ` * bool class_alias([string $original[,string $alias ]])` |
|     - |  320 | ` *   Creates an alias for a class.` |
|     - |  321 | ` * Parameters` |
|     - |  322 | ` *  original` |
|     - |  323 | ` *    The original class.` |
|     - |  324 | ` *  alias` |
|     - |  325 | ` *   The alias name for the class.` |
|     - |  326 | ` * Return` |
|     - |  327 | ` *   Returns TRUE on success or FALSE on failure.` |
|     - |  328 | ` */` |
|     2 |  329 | `PH7_PRIVATE int vm_builtin_class_alias(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  330 | `{` |
|     - |  331 | `	const char *zOld,*zNew;` |
|     - |  332 | `	int nOldLen,nNewLen;` |
|     - |  333 | `	SyHashEntry *pEntry;` |
|     - |  334 | `	ph7_class *pClass;` |
|     - |  335 | `	char *zDup;` |
|     - |  336 | `	sxi32 rc;` |
|     3 |  337 | `	if( nArg < 2 ){` |
|     - |  338 | `		/* Missing arguments,return FALSE */` |
|   ! 0 |  339 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  340 | `		return PH7_OK;` |
|     - |  341 | `	}` |
|     - |  342 | `	/* Extract old class name */` |
|     3 |  343 | `	zOld = ph7_value_to_string(apArg[0],&nOldLen);` |
|     - |  344 | `	/* Extract alias name */` |
|     3 |  345 | `	zNew = ph7_value_to_string(apArg[1],&nNewLen);` |
|     3 |  346 | `	if( nNewLen < 1 ){` |
|     - |  347 | `		/* Invalid alias name,return FALSE */` |
|   ! 0 |  348 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  349 | `		return PH7_OK;` |
|     - |  350 | `	}` |
|     - |  351 | `	/* Perform a hash lookup */` |
|     3 |  352 | `	pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zOld,(sxu32)nOldLen);` |
|     3 |  353 | `	if( pEntry ==  0 ){` |
|     - |  354 | `		/* No such class,return FALSE */` |
|   ! 0 |  355 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  356 | `		return PH7_OK;` |
|     - |  357 | `	}` |
|     - |  358 | `	/* Point to the class */` |
|     3 |  359 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|     - |  360 | `	/* Duplicate alias name */` |
|     3 |  361 | `	zDup = SyMemBackendStrDup(&pCtx->pVm->sAllocator,zNew,(sxu32)nNewLen);` |
|     3 |  362 | `	if( zDup == 0 ){` |
|     - |  363 | `		/* Out of memory,return FALSE */` |
|   ! 0 |  364 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  365 | `		return PH7_OK;` |
|     - |  366 | `	}` |
|     - |  367 | `	/* Create the alias */` |
|     3 |  368 | `	rc = SyHashInsert(&pCtx->pVm->hClass,(const void *)zDup,(sxu32)nNewLen,pClass);` |
|     3 |  369 | `	if( rc != SXRET_OK ){` |
|   ! 0 |  370 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,zDup);` |
|   ! 0 |  371 | `	}` |
|     3 |  372 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|     3 |  373 | `	return PH7_OK;` |
|     2 |  374 | `}` |
|     - |  375 | `/*` |
|     - |  376 | ` * array get_declared_classes(void)` |
|     - |  377 | ` *   Returns an array with the name of the defined classes` |
|     - |  378 | ` * Parameters` |
|     - |  379 | ` *  None` |
|     - |  380 | ` * Return` |
|     - |  381 | ` *   Returns an array of the names of the declared classes` |
|     - |  382 | ` *   in the current script.` |
|     - |  383 | ` * Note:` |
|     - |  384 | ` *   NULL is returned on failure.` |
|     - |  385 | ` */` |
|     2 |  386 | `PH7_PRIVATE int vm_builtin_get_declared_classes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  387 | `{` |
|     - |  388 | `	ph7_value *pName,*pArray;` |
|     - |  389 | `	SyHashEntry *pEntry;` |
|     - |  390 | `	/* Create a new array first */` |
|     3 |  391 | `	pArray = ph7_context_new_array(pCtx);` |
|     3 |  392 | `	pName = ph7_context_new_scalar(pCtx);` |
|     3 |  393 | `	if( pArray == 0 \|\| pName == 0){` |
|   ! 0 |  394 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  395 | `		SXUNUSED(apArg);` |
|     - |  396 | `		/* Out of memory,return NULL */` |
|   ! 0 |  397 | `		ph7_result_null(pCtx);` |
|   ! 0 |  398 | `		return PH7_OK;` |
|     - |  399 | `	}` |
|     - |  400 | `	/* Fill the array with the defined classes */` |
|     3 |  401 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|   194 |  402 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|   191 |  403 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|     - |  404 | `		/* Do not register classes defined as interfaces */` |
|   191 |  405 | `		if( (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|   165 |  406 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|     - |  407 | `			/* insert class name */` |
|   165 |  408 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|     - |  409 | `			/* Reset the cursor */` |
|   165 |  410 | `			ph7_value_reset_string_cursor(pName);` |
|    82 |  411 | `		}` |
|     1 |  412 | `	}` |
|     - |  413 | `	/* Return the created array */` |
|     3 |  414 | `	ph7_result_value(pCtx,pArray);` |
|     3 |  415 | `	return PH7_OK;` |
|     2 |  416 | `}` |
|     - |  417 | `/*` |
|     - |  418 | ` * array get_declared_interfaces(void)` |
|     - |  419 | ` *   Returns an array with the name of the defined interfaces` |
|     - |  420 | ` * Parameters` |
|     - |  421 | ` *  None` |
|     - |  422 | ` * Return` |
|     - |  423 | ` *   Returns an array of the names of the declared interfaces` |
|     - |  424 | ` *   in the current script.` |
|     - |  425 | ` * Note:` |
|     - |  426 | ` *   NULL is returned on failure.` |
|     - |  427 | ` */` |
|     2 |  428 | `PH7_PRIVATE int vm_builtin_get_declared_interfaces(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  429 | `{` |
|     - |  430 | `	ph7_value *pName,*pArray;` |
|     - |  431 | `	SyHashEntry *pEntry;` |
|     - |  432 | `	/* Create a new array first */` |
|     3 |  433 | `	pArray = ph7_context_new_array(pCtx);` |
|     3 |  434 | `	pName = ph7_context_new_scalar(pCtx);` |
|     3 |  435 | `	if( pArray == 0 \|\| pName == 0 ){` |
|   ! 0 |  436 | `		SXUNUSED(nArg); /* cc warning */` |
|   ! 0 |  437 | `		SXUNUSED(apArg);` |
|     - |  438 | `		/* Out of memory,return NULL */` |
|   ! 0 |  439 | `		ph7_result_null(pCtx);` |
|   ! 0 |  440 | `		return PH7_OK;` |
|     - |  441 | `	}` |
|     - |  442 | `	/* Fill the array with the defined classes */` |
|     3 |  443 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|   196 |  444 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|   193 |  445 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|     - |  446 | `		/* Register classes defined as interfaces only */` |
|   193 |  447 | `		if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|    29 |  448 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|     - |  449 | `			/* insert interface name */` |
|    29 |  450 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|     - |  451 | `			/* Reset the cursor */` |
|    29 |  452 | `			ph7_value_reset_string_cursor(pName);` |
|    14 |  453 | `		}` |
|     1 |  454 | `	}` |
|     - |  455 | `	/* Return the created array */` |
|     3 |  456 | `	ph7_result_value(pCtx,pArray);` |
|     3 |  457 | `	return PH7_OK;` |
|     2 |  458 | `}` |
|     - |  459 | `/*` |
|     - |  460 | ` * array get_class_methods(string/object $class_name)` |
|     - |  461 | ` *   Returns an array with the name of the class methods` |
|     - |  462 | ` * Parameters` |
|     - |  463 | ` *  class_name` |
|     - |  464 | ` *  The class name or class instance` |
|     - |  465 | ` * Return` |
|     - |  466 | ` *  Returns an array of method names defined for the class specified by class_name.` |
|     - |  467 | ` *  In case of an error, it returns NULL.` |
|     - |  468 | ` * Note:` |
|     - |  469 | ` *   NULL is returned on failure.` |
|     - |  470 | ` */` |
|     8 |  471 | `PH7_PRIVATE int vm_builtin_get_class_methods(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  472 | `{` |
|     - |  473 | `	ph7_value *pName,*pArray;` |
|     - |  474 | `	SyHashEntry *pEntry;` |
|     - |  475 | `	ph7_class *pClass;` |
|     - |  476 | `	/* Extract the target class first */` |
|     9 |  477 | `	pClass = 0;` |
|     9 |  478 | `	if( nArg > 0 ){` |
|     9 |  479 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|     4 |  480 | `	}` |
|     9 |  481 | `	if( pClass == 0 ){` |
|     - |  482 | `		/* No such class,return NULL */` |
|     3 |  483 | `		ph7_result_null(pCtx);` |
|     3 |  484 | `		return PH7_OK;` |
|     - |  485 | `	}` |
|     - |  486 | `	/* Create a new array  */` |
|     7 |  487 | `	pArray = ph7_context_new_array(pCtx);` |
|     7 |  488 | `	pName = ph7_context_new_scalar(pCtx);` |
|     7 |  489 | `	if( pArray == 0 \|\| pName == 0){` |
|     - |  490 | `		/* Out of memory,return NULL */` |
|   ! 0 |  491 | `		ph7_result_null(pCtx);` |
|   ! 0 |  492 | `		return PH7_OK;` |
|     - |  493 | `	}` |
|     - |  494 | `	/* Fill the array with the defined methods, in php's order: the class's own` |
|     - |  495 | `	 * methods in DECLARATION order, then each ancestor's in ITS declaration` |
|     - |  496 | `	 * order (band A #4 — the raw hash walk returned reverse-insertion/LIFO` |
|     - |  497 | `	 * order). SyHash iterates newest-first, so a reversed walk restores` |
|     - |  498 | `	 * insertion order; grouping by declaring class (sFunc.pUserData, the class` |
|     - |  499 | `	 * a method was compiled into) walks own-then-parent like php. An override` |
|     - |  500 | `	 * lives once in the hash under the subclass, so no dedup is needed. */` |
|     - |  501 | `	{` |
|     - |  502 | `		SySet aTmp;` |
|     - |  503 | `		SyHashEntry **apEntry;` |
|     - |  504 | `		ph7_class *pLevel;` |
|     - |  505 | `		sxu32 n;` |
|     7 |  506 | `		SySetInit(&aTmp,&pCtx->pVm->sAllocator,sizeof(SyHashEntry *));` |
|     7 |  507 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|    27 |  508 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|    21 |  509 | `			SySetPut(&aTmp,(const void *)&pEntry);` |
|     1 |  510 | `		}` |
|     7 |  511 | `		apEntry = (SyHashEntry **)SySetBasePtr(&aTmp);` |
|    15 |  512 | `		for( pLevel = pClass; pLevel; pLevel = pLevel->pBase ){` |
|     - |  513 | `			/* Collect this level's methods, then emit in DECLARATION order` |
|     - |  514 | `			 * (sorted by nLine — same-level methods share a source file; a` |
|     - |  515 | `			 * hash-order fallback covers line-less internal methods). */` |
|     - |  516 | `			SySet aLvl;` |
|     - |  517 | `			ph7_class_method **apLvl;` |
|     - |  518 | `			sxu32 i,j;` |
|     9 |  519 | `			SySetInit(&aLvl,&pCtx->pVm->sAllocator,sizeof(ph7_class_method *));` |
|     - |  520 | `			/* Hash-order fallback for same-line methods: the class's OWN entries` |
|     - |  521 | `			 * come out in declaration order when walked newest-first, while` |
|     - |  522 | `			 * inherited copies (inserted by PH7_ClassInherit's walk of the base` |
|     - |  523 | `			 * hash) come out in declaration order walked oldest-first. */` |
|    37 |  524 | `			for( n = 0; n < SySetUsed(&aTmp); n++ ){` |
|    29 |  525 | `				sxu32 nPick = (pLevel == pClass) ? (SySetUsed(&aTmp) - 1 - n) : n;` |
|    29 |  526 | `				ph7_class_method *pMethod = (ph7_class_method *)apEntry[nPick]->pUserData;` |
|    29 |  527 | `				ph7_class *pDecl = (ph7_class *)pMethod->sFunc.pUserData;` |
|    29 |  528 | `				if( pDecl != pLevel ){` |
|     - |  529 | `					/* A declarer outside the base chain (a used trait, or none)` |
|     - |  530 | `					 * counts as the class's own level, like php. */` |
|     - |  531 | `					ph7_class *pWalk;` |
|     9 |  532 | `					if( pLevel != pClass \|\| pDecl == pClass ){` |
|     7 |  533 | `						continue;` |
|     - |  534 | `					}` |
|     9 |  535 | `					for( pWalk = pClass; pWalk; pWalk = pWalk->pBase ){` |
|     9 |  536 | `						if( pWalk == pDecl ){` |
|     5 |  537 | `							break;` |
|     - |  538 | `						}` |
|     3 |  539 | `					}` |
|     5 |  540 | `					if( pWalk != 0 ){` |
|     5 |  541 | `						continue; /* in-chain: its own level emits it */` |
|     - |  542 | `					}` |
|   ! 0 |  543 | `				}` |
|    21 |  544 | `				SySetPut(&aLvl,(const void *)&pMethod);` |
|    11 |  545 | `			}` |
|     9 |  546 | `			apLvl = (ph7_class_method **)SySetBasePtr(&aLvl);` |
|     - |  547 | `			/* Insertion sort by declaration line (stable) */` |
|    21 |  548 | `			for( i = 1; i < SySetUsed(&aLvl); i++ ){` |
|    13 |  549 | `				ph7_class_method *pKey = apLvl[i];` |
|    13 |  550 | `				for( j = i; j > 0 && apLvl[j-1]->nLine > pKey->nLine; j-- ){` |
|   ! 0 |  551 | `					apLvl[j] = apLvl[j-1];` |
|   ! 0 |  552 | `				}` |
|    13 |  553 | `				apLvl[j] = pKey;` |
|     7 |  554 | `			}` |
|    29 |  555 | `			for( i = 0; i < SySetUsed(&aLvl); i++ ){` |
|     - |  556 | `				/* Insert method name */` |
|    21 |  557 | `				ph7_value_string(pName,SyStringData(&apLvl[i]->sFunc.sName),(int)SyStringLength(&apLvl[i]->sFunc.sName));` |
|    21 |  558 | `				ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|     - |  559 | `				/* Reset the cursor */` |
|    21 |  560 | `				ph7_value_reset_string_cursor(pName);` |
|    11 |  561 | `			}` |
|     9 |  562 | `			SySetRelease(&aLvl);` |
|     5 |  563 | `		}` |
|     7 |  564 | `		SySetRelease(&aTmp);` |
|     - |  565 | `	}` |
|     - |  566 | `	/* Return the created array */` |
|     7 |  567 | `	ph7_result_value(pCtx,pArray);` |
|     - |  568 | `	/*` |
|     - |  569 | `	 * Don't worry about freeing memory here,everything will be relased` |
|     - |  570 | `	 * automatically as soon we return from this foreign function.` |
|     - |  571 | `	 */` |
|     7 |  572 | `	return PH7_OK;` |
|     5 |  573 | `}` |
|     - |  574 | `/*` |
|     - |  575 | ` * This function return TRUE(1) if the given class attribute stored` |
|     - |  576 | ` * in the pAttrName parameter is visible and thus can be extracted` |
|     - |  577 | ` * from the current scope.Otherwise FALSE is returned.` |
|     - |  578 | ` */` |
| 23916 |  579 | `PH7_PRIVATE int PH7_VmClassMemberAccess(` |
|     - |  580 | `	ph7_vm *pVm,               /* Target VM */` |
|     - |  581 | `	ph7_class *pClass,         /* Target Class */` |
|     - |  582 | `	const SyString *pAttrName, /* Attribute name */` |
|     - |  583 | `	sxi32 iProtection,         /* Attribute protection level [i.e: public,protected or private] */` |
|     - |  584 | `	int bLog                   /* TRUE to log forbidden access. */` |
|     - |  585 | `	)` |
|     5 |  586 | `{` |
| 23921 |  587 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
| 18701 |  588 | `		VmFrame *pFrame = pVm->pFrame;` |
|     - |  589 | `		ph7_vm_func *pVmFunc;` |
|     - |  590 | `		ph7_class *pCallerScope;` |
| 18713 |  591 | `		while( pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH) ) ){` |
|     - |  592 | `			/* Safely ignore the exception frame */` |
|    13 |  593 | `			pFrame = pFrame->pParent;` |
|     1 |  594 | `		}` |
| 18701 |  595 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|     - |  596 | `		/* The calling scope is the executing method's declaring class — OR, for a bound closure` |
|     - |  597 | `		 * (Closure::bindTo/call), the explicit scope override carried on the frame (Increment 2). */` |
| 18701 |  598 | `		if( pFrame->pBoundScope ){` |
|    15 |  599 | `			pCallerScope = pFrame->pBoundScope;` |
| 18694 |  600 | `		}else if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
| 18653 |  601 | `			pCallerScope = (ph7_class *)pVmFunc->pUserData;` |
|  9360 |  602 | `		}else if( pVm->pConstEvalClass ){` |
|     - |  603 | `			/* Constant/property initializer bytecode runs without a method` |
|     - |  604 | `			 * frame; its scope is the class being initialized (php: a private` |
|     - |  605 | `			 * constant is reachable from its own class's initializers). */` |
|     3 |  606 | `			pCallerScope = pVm->pConstEvalClass;` |
|     2 |  607 | `		}else{` |
|    34 |  608 | `			goto dis; /* Not in a class scope: access is forbidden */` |
|     - |  609 | `		}` |
| 18669 |  610 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     - |  611 | `			/* Must be the same instance or a trait used by the class */` |
|   597 |  612 | `			ph7_class *pCaller = pCallerScope;` |
|   597 |  613 | `			if( pCaller != pClass ){` |
|     - |  614 | `				/* Check if the caller is a trait used by pClass */` |
|     - |  615 | `				ph7_class **apTrait;` |
|     - |  616 | `				sxu32 nTrait,k;` |
|    12 |  617 | `				int iFound = 0;` |
|    12 |  618 | `				apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|    12 |  619 | `				nTrait = SySetUsed(&pClass->aTrait);` |
|    20 |  620 | `				for(k = 0; k < nTrait; k++){` |
|    17 |  621 | `					if( apTrait[k] == pCaller ){` |
|     9 |  622 | `						iFound = 1;` |
|     9 |  623 | `						break;` |
|     - |  624 | `					}` |
|     5 |  625 | `				}` |
|    12 |  626 | `				if( !iFound ){` |
|     3 |  627 | `					goto dis; /* Access is forbidden */` |
|     - |  628 | `				}` |
|     4 |  629 | `			}` |
|   300 |  630 | `		}else{` |
|     - |  631 | `			/* Protected */` |
| 18077 |  632 | `			ph7_class *pBase = pCallerScope;` |
|     - |  633 | `			/* Must be in the same class hierarchy */` |
| 18077 |  634 | `			if( !PH7_VmInstanceOf(pClass,pBase) && !PH7_VmInstanceOf(pBase,pClass) ){` |
|   ! 0 |  635 | `				goto dis; /* Access is forbidden */` |
|     - |  636 | `			}` |
|     - |  637 | `		}` |
|  9331 |  638 | `	}` |
| 23887 |  639 | `	return 1; /* Access is granted */` |
|    17 |  640 | `dis:` |
|    36 |  641 | `	if( bLog ){` |
|   ! 0 |  642 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|     - |  643 | `			"Access to the class attribute '%z->%z' is forbidden",` |
|   ! 0 |  644 | `			&pClass->sName,pAttrName);` |
|   ! 0 |  645 | `	}` |
|    36 |  646 | `	return 0; /* Access is forbidden */` |
| 11963 |  647 | `}` |
|     - |  648 | `/*` |
|     - |  649 | ` * array get_class_vars(string/object $class_name)` |
|     - |  650 | ` *   Get the default properties of the class` |
|     - |  651 | ` * Parameters` |
|     - |  652 | ` *  class_name` |
|     - |  653 | ` *   The class name or class instance` |
|     - |  654 | ` * Return` |
|     - |  655 | ` *  Returns an associative array of declared properties visible from the current scope` |
|     - |  656 | ` *  with their default value. The resulting array elements are in the form` |
|     - |  657 | ` *  of varname => value.` |
|     - |  658 | ` * Note:` |
|     - |  659 | ` *   NULL is returned on failure.` |
|     - |  660 | ` */` |
|     2 |  661 | `PH7_PRIVATE int vm_builtin_get_class_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  662 | `{` |
|     - |  663 | `	ph7_value *pName,*pArray,sValue;` |
|     - |  664 | `	SyHashEntry *pEntry;` |
|     - |  665 | `	ph7_class *pClass;` |
|     - |  666 | `	/* Extract the target class first */` |
|     3 |  667 | `	pClass = 0;` |
|     3 |  668 | `	if( nArg > 0 ){` |
|     3 |  669 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|     1 |  670 | `	}` |
|     3 |  671 | `	if( pClass == 0 ){` |
|     - |  672 | `		/* No such class,return NULL */` |
|   ! 0 |  673 | `		ph7_result_null(pCtx);` |
|   ! 0 |  674 | `		return PH7_OK;` |
|     - |  675 | `	}` |
|     - |  676 | `	/* Create a new array  */` |
|     3 |  677 | `	pArray = ph7_context_new_array(pCtx);` |
|     3 |  678 | `	pName = ph7_context_new_scalar(pCtx);` |
|     3 |  679 | `	PH7_MemObjInit(pCtx->pVm,&sValue);` |
|     3 |  680 | `	if( pArray == 0 \|\| pName == 0){` |
|     - |  681 | `		/* Out of memory,return NULL */` |
|   ! 0 |  682 | `		ph7_result_null(pCtx);` |
|   ! 0 |  683 | `		return PH7_OK;` |
|     - |  684 | `	}` |
|     - |  685 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|     3 |  686 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     8 |  687 | `	while((pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|     5 |  688 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     - |  689 | `		/* Check if the access is allowed */` |
|     5 |  690 | `		if( PH7_VmClassMemberAccess(pCtx->pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|     5 |  691 | `			SyString *pAttrName = &pAttr->sName;` |
|     5 |  692 | `			ph7_value *pValue = 0;` |
|     5 |  693 | `			if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|     - |  694 | `				/* Static slots are computed at mount; constants lazily */` |
|     5 |  695 | `				PH7_VmMaterializeClassConst(pCtx->pVm,pClass,pAttr);` |
|     5 |  696 | `				pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pAttr->nIdx);` |
|     3 |  697 | `			}else{` |
|   ! 0 |  698 | `				if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|   ! 0 |  699 | `					PH7_MemObjRelease(&sValue);` |
|     - |  700 | `					/* Compute default value (any complex expression) associated with this attribute */` |
|   ! 0 |  701 | `					VmLocalExec(pCtx->pVm,&pAttr->aByteCode,&sValue,FALSE);` |
|   ! 0 |  702 | `					pValue = &sValue;` |
|   ! 0 |  703 | `				}` |
|     - |  704 | `			}` |
|     - |  705 | `			/* Fill in the array */` |
|     5 |  706 | `			ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|     5 |  707 | `			ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|     - |  708 | `			/* Reset the cursor */` |
|     5 |  709 | `			ph7_value_reset_string_cursor(pName);` |
|     2 |  710 | `		}` |
|     1 |  711 | `	}` |
|     3 |  712 | `	PH7_MemObjRelease(&sValue);` |
|     - |  713 | `	/* Return the created array */` |
|     3 |  714 | `	ph7_result_value(pCtx,pArray);` |
|     - |  715 | `	/*` |
|     - |  716 | `	 * Don't worry about freeing memory here,everything will be relased` |
|     - |  717 | `	 * automatically as soon we return from this foreign function.` |
|     - |  718 | `	 */` |
|     3 |  719 | `	return PH7_OK;` |
|     2 |  720 | `}` |
|     - |  721 | `/*` |
|     - |  722 | ` * array get_object_vars(object $this)` |
|     - |  723 | ` *   Gets the properties of the given object` |
|     - |  724 | ` * Parameters` |
|     - |  725 | ` *  this` |
|     - |  726 | ` *   A class instance` |
|     - |  727 | ` * Return` |
|     - |  728 | ` *  Returns an associative array of defined object accessible non-static properties` |
|     - |  729 | ` *  for the specified object in scope. If a property have not been assigned a value` |
|     - |  730 | ` *  it will be returned with a NULL value.` |
|     - |  731 | ` * Note:` |
|     - |  732 | ` *   NULL is returned on failure.` |
|     - |  733 | ` */` |
|    16 |  734 | `PH7_PRIVATE int vm_builtin_get_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  735 | `{` |
|    17 |  736 | `	ph7_class_instance *pThis = 0;` |
|     - |  737 | `	ph7_value *pName,*pArray;` |
|     - |  738 | `	SyHashEntry *pEntry;` |
|    17 |  739 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|     - |  740 | `		/* Extract the target instance */` |
|    17 |  741 | `		pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     8 |  742 | `	}` |
|    17 |  743 | `	if( pThis == 0 ){` |
|     - |  744 | `		/* No such instance,return NULL */` |
|   ! 0 |  745 | `		ph7_result_null(pCtx);` |
|   ! 0 |  746 | `		return PH7_OK;` |
|     - |  747 | `	}` |
|     - |  748 | `	/* Create a new array  */` |
|    17 |  749 | `	pArray = ph7_context_new_array(pCtx);` |
|    17 |  750 | `	pName = ph7_context_new_scalar(pCtx);` |
|    17 |  751 | `	if( pArray == 0 \|\| pName == 0){` |
|     - |  752 | `		/* Out of memory,return NULL */` |
|   ! 0 |  753 | `		ph7_result_null(pCtx);` |
|   ! 0 |  754 | `		return PH7_OK;` |
|     - |  755 | `	}` |
|     - |  756 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|    17 |  757 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|    65 |  758 | `	while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|    49 |  759 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     - |  760 | `		SyString *pAttrName;` |
|    49 |  761 | `		if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|     - |  762 | `			/* Only non-static/constant attributes are extracted */` |
|    11 |  763 | `			continue;` |
|     - |  764 | `		}` |
|    39 |  765 | `		pAttrName = &pVmAttr->pAttr->sName;` |
|     - |  766 | `		/* Check if the access is allowed */` |
|    39 |  767 | `		if( PH7_VmClassMemberAccess(pCtx->pVm,pThis->pClass,pAttrName,pVmAttr->pAttr->iProtection,FALSE) ){` |
|    31 |  768 | `			ph7_value *pValue = 0;` |
|     - |  769 | `			/* Extract attribute */` |
|    31 |  770 | `			pValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|    31 |  771 | `			if( pValue ){` |
|     - |  772 | `				/* Insert attribute name in the array */` |
|    31 |  773 | `				ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|    31 |  774 | `				ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|    15 |  775 | `			}` |
|     - |  776 | `			/* Reset the cursor */` |
|    31 |  777 | `			ph7_value_reset_string_cursor(pName);` |
|    15 |  778 | `		}` |
|     1 |  779 | `	}` |
|     - |  780 | `	/* Return the created array */` |
|    17 |  781 | `	ph7_result_value(pCtx,pArray);` |
|     - |  782 | `	/*` |
|     - |  783 | `	 * Don't worry about freeing memory here,everything will be relased` |
|     - |  784 | `	 * automatically as soon we return from this foreign function.` |
|     - |  785 | `	 */` |
|    17 |  786 | `	return PH7_OK;` |
|     9 |  787 | `}` |
|     - |  788 | ``/* Bound on `extends` chain depth — matches PH7_THROWABLE_WALK_MAX_DEPTH in`` |
|     - |  789 | ` * compile.c. Defends against compiler cycles even though interface cycle` |
|     - |  790 | ` * detection should reject them up front. */` |
|     - |  791 | `#define PH7_INTERFACE_WALK_MAX_DEPTH 64` |
|     - |  792 | `/*` |
|     - |  793 | ` * This function returns TRUE if the given class is an implemented` |
|     - |  794 | ` * interface.Otherwise FALSE is returned.` |
|     - |  795 | ` */` |
| 17596 |  796 | `static int VmQueryInterfaceSet(ph7_class *pClass,SySet *pSet)` |
|     5 |  797 | `{` |
|     - |  798 | `	ph7_class **apInterface;` |
|     - |  799 | `	sxu32 n;` |
| 17601 |  800 | `	if( SySetUsed(pSet) < 1 ){` |
|     - |  801 | `		/* Empty interface container */` |
|   245 |  802 | `		return FALSE;` |
|     - |  803 | `	}` |
|     - |  804 | `	/* Point to the set of implemented interfaces */` |
| 17361 |  805 | `	apInterface = (ph7_class **)SySetBasePtr(pSet);` |
|     - |  806 | `	/* Perform the lookup, walking each interface's parent chain so that` |
|     - |  807 | `	 * Iterator extends Traversable (and similar) is recognized. */` |
| 33349 |  808 | `	for( n = 0 ; n < SySetUsed(pSet) ; n++ ){` |
| 17497 |  809 | `		ph7_class *pIface = apInterface[n];` |
| 17497 |  810 | `		int iDepth = 0;` |
| 33583 |  811 | `		while( pIface && iDepth <= PH7_INTERFACE_WALK_MAX_DEPTH ){` |
| 17595 |  812 | `			if( pIface == pClass ){` |
|  1509 |  813 | `				return TRUE;` |
|     - |  814 | `			}` |
| 16091 |  815 | `			pIface = pIface->pBase;` |
| 16091 |  816 | `			iDepth++;` |
|     5 |  817 | `		}` |
|  7999 |  818 | `	}` |
| 15857 |  819 | `	return FALSE;` |
|  8803 |  820 | `}` |
|     - |  821 | `/*` |
|     - |  822 | ` * This function returns TRUE if the given class (first argument)` |
|     - |  823 | ` * is an instance of the main class (second argument).` |
|     - |  824 | ` * Otherwise FALSE is returned.` |
|     - |  825 | ` */` |
| 21666 |  826 | `PH7_PRIVATE int PH7_VmInstanceOf(ph7_class *pThis,ph7_class *pClass)` |
|     5 |  827 | `{` |
|     - |  828 | `	ph7_class *pParent;` |
|     - |  829 | `	sxi32 rc;` |
| 21671 |  830 | `	if( pThis == pClass ){` |
|     - |  831 | `		/* Instance of the same class */` |
|  8605 |  832 | `		return TRUE;` |
|     - |  833 | `	}` |
|     - |  834 | `	/* Check implemented interfaces */` |
| 13071 |  835 | `	rc = VmQueryInterfaceSet(pClass,&pThis->aInterface);` |
| 13071 |  836 | `	if( rc ){` |
|   983 |  837 | `		return TRUE;` |
|     - |  838 | `	}` |
|     - |  839 | `	/* Check parent classes */` |
| 12093 |  840 | `	pParent = pThis->pBase;` |
| 16089 |  841 | `	while( pParent ){` |
| 15829 |  842 | `		if( pParent == pClass ){` |
|     - |  843 | `			/* Same instance */` |
| 11307 |  844 | `			return TRUE;` |
|     - |  845 | `		}` |
|     - |  846 | `		/* Check the implemented interfaces */` |
|  4527 |  847 | `		rc = VmQueryInterfaceSet(pClass,&pParent->aInterface);` |
|  4527 |  848 | `		if( rc ){` |
|   531 |  849 | `			return TRUE;` |
|     - |  850 | `		}` |
|     - |  851 | `		/* Point to the parent class */` |
|  4001 |  852 | `		pParent = pParent->pBase;` |
|     5 |  853 | `	}` |
|     - |  854 | `	/* Not an instance of the the given class */` |
|   265 |  855 | `	return FALSE;` |
| 10838 |  856 | `}` |
|     - |  857 | `/*` |
|     - |  858 | ` * This function returns TRUE if the given class (first argument)` |
|     - |  859 | ` * is a subclass of the main class (second argument).` |
|     - |  860 | ` * Otherwise FALSE is returned.` |
|     - |  861 | ` */` |
|    12 |  862 | `static int VmSubclassOf(ph7_class *pClass,ph7_class *pBase)` |
|     1 |  863 | `{` |
|    13 |  864 | `	SySet *pInterface = &pClass->aInterface;` |
|     - |  865 | `	SyHashEntry *pEntry;` |
|     - |  866 | `	SyString *pName;` |
|     - |  867 | `	sxi32 rc;` |
|    21 |  868 | `	while( pClass ){` |
|    13 |  869 | `		pName = &pClass->sName;` |
|     - |  870 | `		/* Query the derived hashtable */` |
|    13 |  871 | `		pEntry = SyHashGet(&pBase->hDerived,(const void *)pName->zString,pName->nByte);` |
|    13 |  872 | `		if( pEntry ){` |
|     5 |  873 | `			return TRUE;` |
|     - |  874 | `		}` |
|     9 |  875 | `		pClass = pClass->pBase;` |
|     1 |  876 | `	}` |
|     9 |  877 | `	rc = VmQueryInterfaceSet(pBase,pInterface);` |
|     9 |  878 | `	if( rc ){` |
|   ! 0 |  879 | `		return TRUE;` |
|     - |  880 | `	}` |
|     - |  881 | `	/* Not a subclass */` |
|     9 |  882 | `	return FALSE;` |
|     7 |  883 | `}` |
|     - |  884 | `/*` |
|     - |  885 | ` * bool is_a(object $object,string $class_name)` |
|     - |  886 | ` *   Checks if the object is of this class or has this class as one of its parents.` |
|     - |  887 | ` * Parameters` |
|     - |  888 | ` *  object` |
|     - |  889 | ` *   The tested object` |
|     - |  890 | ` * class_name` |
|     - |  891 | ` *  The class name` |
|     - |  892 | ` * Return` |
|     - |  893 | ` *   Returns TRUE if the object is of this class or has this class as one of its` |
|     - |  894 | ` *   parents, FALSE otherwise.` |
|     - |  895 | ` */` |
|    18 |  896 | `PH7_PRIVATE int vm_builtin_is_a(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  897 | `{` |
|    19 |  898 | `	int res = 0; /* Assume FALSE by default */` |
|    19 |  899 | `	if( nArg > 1 && ph7_value_is_object(apArg[0])  ){` |
|    19 |  900 | `		ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     - |  901 | `		ph7_class *pClass;` |
|     - |  902 | `		/* Extract the given class */` |
|    19 |  903 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|    19 |  904 | `		if( pClass ){` |
|     - |  905 | `			/* Perform the query */` |
|    19 |  906 | `			res = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|     9 |  907 | `		}` |
|     9 |  908 | `	}` |
|     - |  909 | `	/* Query result */` |
|    19 |  910 | `	ph7_result_bool(pCtx,res);` |
|    19 |  911 | `	return PH7_OK;` |
|     1 |  912 | `}` |
|     - |  913 | `/*` |
|     - |  914 | ` * int spl_object_id(object $object)` |
|     - |  915 | ` *  Return the integer object handle (per-instance id) of the given object.` |
|     - |  916 | ` * PHL note: PHP 8 throws a TypeError when passed a non-object; PHL returns NULL` |
|     - |  917 | ` * to stay consistent with the engine's graceful-degradation convention.` |
|     - |  918 | ` */` |
|    18 |  919 | `PH7_PRIVATE int vm_builtin_spl_object_id(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  920 | `{` |
|     - |  921 | `	ph7_class_instance *pThis;` |
|    21 |  922 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|   ! 0 |  923 | `		ph7_result_null(pCtx);` |
|   ! 0 |  924 | `		return PH7_OK;` |
|     - |  925 | `	}` |
|    21 |  926 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    21 |  927 | `	ph7_result_int64(pCtx,(ph7_int64)pThis->nObjId);` |
|    21 |  928 | `	return PH7_OK;` |
|    12 |  929 | `}` |
|     - |  930 | `/*` |
|     - |  931 | ` * string spl_object_hash(object $object)` |
|     - |  932 | ` *  Return a 32-char hex identifier, unique and stable per live object.` |
|     - |  933 | ` * PHL note: PHP derives this from the internal handle plus a per-process key, so` |
|     - |  934 | ` * the exact value is NOT reproducible. PHL returns the zero-padded object id,` |
|     - |  935 | ` * which preserves the only guaranteed properties: unique per live object, stable` |
|     - |  936 | ` * across calls, and distinct objects -> distinct strings. A non-object returns` |
|     - |  937 | ` * NULL (PHP 8 throws a TypeError; see spl_object_id above).` |
|     - |  938 | ` */` |
|    10 |  939 | `PH7_PRIVATE int vm_builtin_spl_object_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  940 | `{` |
|     - |  941 | `	ph7_class_instance *pThis;` |
|    11 |  942 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|   ! 0 |  943 | `		ph7_result_null(pCtx);` |
|   ! 0 |  944 | `		return PH7_OK;` |
|     - |  945 | `	}` |
|    11 |  946 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    11 |  947 | `	ph7_result_string_format(pCtx,"%08x%08x%08x%08x",0,0,0,(unsigned int)pThis->nObjId);` |
|    11 |  948 | `	return PH7_OK;` |
|     6 |  949 | `}` |
|     - |  950 | `/*` |
|     - |  951 | ` * bool is_subclass_of(object/string $object,object/string $class_name)` |
|     - |  952 | ` *   Checks if the object has this class as one of its parents.` |
|     - |  953 | ` * Parameters` |
|     - |  954 | ` *  object` |
|     - |  955 | ` *   The tested object` |
|     - |  956 | ` * class_name` |
|     - |  957 | ` *  The class name` |
|     - |  958 | ` * Return` |
|     - |  959 | ` *  This function returns TRUE if the object , belongs to a class` |
|     - |  960 | ` *  which is a subclass of class_name, FALSE otherwise.` |
|     - |  961 | ` */` |
|    14 |  962 | `PH7_PRIVATE int vm_builtin_is_subclass_of(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  963 | `{` |
|    15 |  964 | `	int res = 0; /* Assume FALSE by default */` |
|    15 |  965 | `	if( nArg > 1 ){` |
|     - |  966 | `		ph7_class *pClass,*pMain;` |
|     - |  967 | `		/* Extract the given classes */` |
|    15 |  968 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|    15 |  969 | `		pMain = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|    15 |  970 | `		if( pClass && pMain ){` |
|     - |  971 | `			/* Perform the query */` |
|    13 |  972 | `			res = VmSubclassOf(pClass,pMain);` |
|     6 |  973 | `		}` |
|     7 |  974 | `	}` |
|     - |  975 | `	/* Query result */` |
|    15 |  976 | `	ph7_result_bool(pCtx,res);` |
|    15 |  977 | `	return PH7_OK;` |
|     1 |  978 | `}` |
|    54 |  979 | `PH7_PRIVATE int vm_builtin_call_user_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  980 | `{` |
|     - |  981 | `	ph7_value sResult; /* Store callback return value here */` |
|     - |  982 | `	sxi32 rc;` |
|    55 |  983 | `	if( nArg < 1 ){` |
|     - |  984 | `		/* Missing arguments,return FALSE */` |
|   ! 0 |  985 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  986 | `		return PH7_OK;` |
|     - |  987 | `	}` |
|    55 |  988 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|    55 |  989 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|     - |  990 | `	/* Try to invoke the callback. If the call_user_func() call site used` |
|     - |  991 | `	 * name: arguments (e.g. call_user_func('f', b: 9)), forward them to the` |
|     - |  992 | `	 * callback. The inner call's argument i is the outer argument i+1 (outer` |
|     - |  993 | `	 * argument 0 is the callback), so the inner name array is simply the outer` |
|     - |  994 | `	 * names shifted by one — no copy needed: VmResolveNamedArgs treats any index` |
|     - |  995 | `	 * >= nTotal as positional, so a shorter map covers the callback's args. */` |
|    64 |  996 | `	if( pCtx->pArgMap && pCtx->pArgMap->bHasNamed && nArg > 1 ){` |
|    19 |  997 | `		VmCallArgMap *pOuter = pCtx->pArgMap;` |
|     - |  998 | `		VmCallArgMap sInner;` |
|    19 |  999 | `		sInner.bHasNamed = 1;` |
|    19 | 1000 | `		sInner.bIsNamespaced = 0;` |
|     - | 1001 | `		/* Named args to call_user_func coerce in WEAK mode even from a` |
|     - | 1002 | `		 * strict_types=1 caller (verified vs php 8.5.7): a name: argument` |
|     - | 1003 | `		 * collected into the variadic and re-spread loses the strict context.` |
|     - | 1004 | `		 * call_user_func_array does NOT share this quirk (it stays strict). */` |
|    19 | 1005 | `		sInner.bStrict = 0;` |
|    19 | 1006 | `		sInner.nTotal = pOuter->nTotal > 1 ? pOuter->nTotal - 1 : 0;` |
|    19 | 1007 | `		sInner.aNames = sInner.nTotal > 0 ? &pOuter->aNames[1] : 0;` |
|    19 | 1008 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult,&sInner);` |
|    10 | 1009 | `	}else{` |
|    37 | 1010 | `		rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult);` |
|     - | 1011 | `	}` |
|    55 | 1012 | `	if( rc == PH7_EXCEPTION ){` |
|     - | 1013 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds` |
|     - | 1014 | `		 * through the nearest try/catch instead of returning FALSE. */` |
|     7 | 1015 | `		PH7_MemObjRelease(&sResult);` |
|     7 | 1016 | `		return PH7_EXCEPTION;` |
|     - | 1017 | `	}` |
|    49 | 1018 | `	if( rc != SXRET_OK ){` |
|     - | 1019 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|   ! 0 | 1020 | `		ph7_result_bool(pCtx,0); /* return false */` |
|   ! 0 | 1021 | `	}else{` |
|     - | 1022 | `		/* Callback result */` |
|    49 | 1023 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|     - | 1024 | `	}` |
|    49 | 1025 | `	PH7_MemObjRelease(&sResult);` |
|    49 | 1026 | `	return PH7_OK;` |
|    28 | 1027 | `}` |
|     - | 1028 | `/*` |
|     - | 1029 | ` * value call_user_func_array(callable $callback,array $param_arr)` |
|     - | 1030 | ` *  Call a callback with an array of parameters.` |
|     - | 1031 | ` * Parameter` |
|     - | 1032 | ` *  $callback` |
|     - | 1033 | ` *   The callable to be called.` |
|     - | 1034 | ` * $param_arr` |
|     - | 1035 | ` *  The parameters to be passed to the callback, as an indexed array.` |
|     - | 1036 | ` * Return` |
|     - | 1037 | ` *  Returns the return value of the callback, or FALSE on error.` |
|     - | 1038 | ` */` |
|    34 | 1039 | `PH7_PRIVATE int vm_builtin_call_user_func_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1040 | `{` |
|     - | 1041 | `	ph7_hashmap_node *pEntry; /* Current hashmap entry */` |
|     - | 1042 | `	ph7_value *pValue,sResult;/* Store callback return value here */` |
|     - | 1043 | `	ph7_hashmap *pMap;        /* Target hashmap */` |
|     - | 1044 | `	SySet aArg;               /* Argument value pointers */` |
|    35 | 1045 | `	SyString *aNames = 0;     /* Name map, lazily allocated when a string key appears */` |
|    35 | 1046 | `	sxu32 nSlot = 0;          /* Number of collected arguments */` |
|     - | 1047 | `	sxi32 rc;` |
|     - | 1048 | `	sxu32 n;` |
|    35 | 1049 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|     - | 1050 | `		/* Missing/Invalid arguments,return FALSE */` |
|   ! 0 | 1051 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1052 | `		return PH7_OK;` |
|     - | 1053 | `	}` |
|    35 | 1054 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|    35 | 1055 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|     - | 1056 | `	/* Initialize the arguments container */` |
|    35 | 1057 | `	SySetInit(&aArg,&pCtx->pVm->sAllocator,sizeof(ph7_value *));` |
|     - | 1058 | `	/* Turn hashmap entries into callback arguments. A string key becomes a` |
|     - | 1059 | `	 * named argument (PHP 8: call_user_func_array($cb, ['b' => 9])), an integer` |
|     - | 1060 | `	 * key stays positional. The name map points straight at each node's key` |
|     - | 1061 | `	 * blob: the source array stays pinned on the operand stack for the whole` |
|     - | 1062 | `	 * call, so the blobs outlive argument binding. A pure list array (no string` |
|     - | 1063 | `	 * keys) never allocates aNames and takes the plain positional path. */` |
|    35 | 1064 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|    35 | 1065 | `	pEntry = pMap->pFirst; /* First inserted entry */` |
|   189 | 1066 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     - | 1067 | `		/* Extract node value */` |
|   155 | 1068 | `		if( (pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx)) != 0 ){` |
|   155 | 1069 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|    23 | 1070 | `				if( aNames == 0 ){` |
|     - | 1071 | `					/* First string key: allocate the whole map, zeroed so every` |
|     - | 1072 | `					 * not-yet-seen slot defaults to positional. */` |
|    13 | 1073 | `					aNames = (SyString *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,pMap->nEntry * sizeof(SyString));` |
|    13 | 1074 | `					if( aNames == 0 ){` |
|   ! 0 | 1075 | `						SySetRelease(&aArg);` |
|   ! 0 | 1076 | `						PH7_MemObjRelease(&sResult);` |
|   ! 0 | 1077 | `						return PH7_ContextMemoryError(pCtx);` |
|     - | 1078 | `					}` |
|    13 | 1079 | `					SyZero(aNames,pMap->nEntry * sizeof(SyString));` |
|     6 | 1080 | `				}` |
|    23 | 1081 | `				SyStringInitFromBuf(&aNames[nSlot],SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    11 | 1082 | `			}` |
|   155 | 1083 | `			SySetPut(&aArg,(const void *)&pValue);` |
|   155 | 1084 | `			nSlot++;` |
|    77 | 1085 | `		}` |
|     - | 1086 | `		/* Point to the next entry */` |
|   155 | 1087 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    78 | 1088 | `	}` |
|     - | 1089 | `	/* Try to invoke the callback */` |
|    35 | 1090 | `	if( aNames ){` |
|     - | 1091 | `		VmCallArgMap sMap;` |
|    13 | 1092 | `		sMap.bHasNamed = 1;` |
|    13 | 1093 | `		sMap.bIsNamespaced = 0;` |
|     - | 1094 | `		/* Coercion strictness follows the caller's file; the OP_CALL dispatcher` |
|     - | 1095 | `		 * forwards the call site's map on pArgMap (0 only at non-OP_CALL sites). */` |
|    13 | 1096 | `		sMap.bStrict = (pCtx->pArgMap ? pCtx->pArgMap->bStrict : 0);` |
|    13 | 1097 | `		sMap.nTotal = nSlot;` |
|    13 | 1098 | `		sMap.aNames = aNames;` |
|    19 | 1099 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],(int)nSlot,` |
|    12 | 1100 | `			(ph7_value **)SySetBasePtr(&aArg),&sResult,&sMap);` |
|    13 | 1101 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,aNames);` |
|     7 | 1102 | `	}else{` |
|    34 | 1103 | `		rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],(int)nSlot,` |
|    22 | 1104 | `			(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|     - | 1105 | `	}` |
|    35 | 1106 | `	if( rc == PH7_EXCEPTION ){` |
|     - | 1107 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds. */` |
|     5 | 1108 | `		PH7_MemObjRelease(&sResult);` |
|     5 | 1109 | `		SySetRelease(&aArg);` |
|     5 | 1110 | `		return PH7_EXCEPTION;` |
|     - | 1111 | `	}` |
|    31 | 1112 | `	if( rc != SXRET_OK ){` |
|     - | 1113 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|   ! 0 | 1114 | `		ph7_result_bool(pCtx,0); /* return false */` |
|   ! 0 | 1115 | `	}else{` |
|     - | 1116 | `		/* Callback result */` |
|    31 | 1117 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|     - | 1118 | `	}` |
|     - | 1119 | `	/* Cleanup the mess left behind */` |
|    31 | 1120 | `	PH7_MemObjRelease(&sResult);` |
|    31 | 1121 | `	SySetRelease(&aArg);` |
|    31 | 1122 | `	return PH7_OK;` |
|    18 | 1123 | `}` |
|     - | 1124 |  |
