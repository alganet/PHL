# src/ph7/vm_builtin_class.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 547/624 lines (87.66%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|     - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    5 | ` */` |
|     - |    6 | `#include "ph7int.h"` |
|   572 |    7 | `PH7_PRIVATE int vm_builtin_get_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 |    8 | `{` |
|     - |    9 | `	ph7_class *pClass;` |
|     - |   10 | `	SyString *pName;` |
|   577 |   11 | `	if( nArg < 1 ){` |
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
|   577 |   24 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|   577 |   25 | `		if( pClass ){` |
|   575 |   26 | `			pName = &pClass->sName;` |
|     - |   27 | `			/* Return the class name */` |
|   575 |   28 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|   290 |   29 | `		}else{` |
|     - |   30 | `			/* Not a class instance,return FALSE */` |
|     3 |   31 | `			ph7_result_bool(pCtx,0);` |
|     - |   32 | `		}` |
|     - |   33 | `	}` |
|   577 |   34 | `	return PH7_OK;` |
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
|  2294 |  113 | `PH7_PRIVATE ph7_class * PH7_VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg)` |
|     5 |  114 | `{` |
|  2299 |  115 | `	ph7_class *pClass = 0;` |
|  2299 |  116 | `	if( ph7_value_is_object(pArg) ){` |
|     - |  117 | `		/* Class instance already loaded,no need to perform a lookup */` |
|   685 |  118 | `		pClass = ((ph7_class_instance *)pArg->x.pOther)->pClass;` |
|  1957 |  119 | `	}else if( ph7_value_is_string(pArg) ){` |
|     - |  120 | `		const char *zClass;` |
|     - |  121 | `		int nLen;` |
|     - |  122 | `		/* Extract class name */` |
|  1614 |  123 | `		zClass = ph7_value_to_string(pArg,&nLen);` |
|  1614 |  124 | `		if( nLen > 0 ){` |
|     - |  125 | `			SyHashEntry *pEntry;` |
|     - |  126 | `			/* Perform a lookup */` |
|  1614 |  127 | `			pEntry = SyHashGet(&pVm->hClass,(const void *)zClass,(sxu32)nLen);` |
|  1614 |  128 | `			if( pEntry ){` |
|     - |  129 | `				/* Point to the desired class */` |
|  1592 |  130 | `				pClass = (ph7_class *)pEntry->pUserData;` |
|   795 |  131 | `			}` |
|   806 |  132 | `		}` |
|   806 |  133 | `	}` |
|  2299 |  134 | `	return pClass;` |
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
|     4 |  227 | `{` |
|    70 |  228 | `	int res = 0; /* Assume class does not exist */` |
|    70 |  229 | `	if( nArg > 0 ){` |
|    70 |  230 | `		SyHashEntry *pEntry = 0;` |
|     - |  231 | `		const char *zName;` |
|     - |  232 | `		int nLen;` |
|    70 |  233 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|     - |  234 | `		/* Extract given name */` |
|    70 |  235 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|    70 |  236 | `		if( nArg >= 2 ){` |
|     6 |  237 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|     2 |  238 | `		}` |
|    70 |  239 | `		if( nLen > 0 ){` |
|     - |  240 | `			/* Perform a hash lookup first */` |
|    70 |  241 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|    33 |  242 | `		}` |
|    70 |  243 | `		if( pEntry == 0 && nLen > 0 && iAutoload ){` |
|     - |  244 | `			/* Try autoload, then re-check */` |
|    20 |  245 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,(sxu32)nLen,FALSE);` |
|    20 |  246 | `			if( pClass ){` |
|     6 |  247 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,(sxu32)nLen);` |
|     2 |  248 | `			}` |
|     8 |  249 | `		}` |
|    70 |  250 | `		if( pEntry ){` |
|     - |  251 | `			/* Walk the collision chain: return TRUE only for concrete or abstract classes,` |
|     - |  252 | `			 * not for interfaces or traits (matching PHP behavior). */` |
|    55 |  253 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|    55 |  254 | `			while( pClass ){` |
|    55 |  255 | `				if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0 ){` |
|    55 |  256 | `					res = 1;` |
|    55 |  257 | `					break;` |
|     - |  258 | `				}` |
|   ! 0 |  259 | `				pClass = pClass->pNextName;` |
|   ! 0 |  260 | `			}` |
|    26 |  261 | `		}` |
|    33 |  262 | `	}` |
|    70 |  263 | `	ph7_result_bool(pCtx,res);` |
|    70 |  264 | `	return PH7_OK;` |
|     4 |  265 | `}` |
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
|   236 |  402 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|   233 |  403 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|     - |  404 | `		/* Do not register classes defined as interfaces */` |
|   233 |  405 | `		if( (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|   205 |  406 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|     - |  407 | `			/* insert class name */` |
|   205 |  408 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|     - |  409 | `			/* Reset the cursor */` |
|   205 |  410 | `			ph7_value_reset_string_cursor(pName);` |
|   102 |  411 | `		}` |
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
|   238 |  444 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|   235 |  445 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|     - |  446 | `		/* Register classes defined as interfaces only */` |
|   235 |  447 | `		if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|    31 |  448 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|     - |  449 | `			/* insert interface name */` |
|    31 |  450 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|     - |  451 | `			/* Reset the cursor */` |
|    31 |  452 | `			ph7_value_reset_string_cursor(pName);` |
|    15 |  453 | `		}` |
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
| 26042 |  579 | `PH7_PRIVATE int PH7_VmClassMemberAccess(` |
|     - |  580 | `	ph7_vm *pVm,               /* Target VM */` |
|     - |  581 | `	ph7_class *pClass,         /* Target Class */` |
|     - |  582 | `	const SyString *pAttrName, /* Attribute name */` |
|     - |  583 | `	sxi32 iProtection,         /* Attribute protection level [i.e: public,protected or private] */` |
|     - |  584 | `	int bLog                   /* TRUE to log forbidden access. */` |
|     - |  585 | `	)` |
|     5 |  586 | `{` |
| 26047 |  587 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
| 20189 |  588 | `		VmFrame *pFrame = pVm->pFrame;` |
|     - |  589 | `		ph7_vm_func *pVmFunc;` |
|     - |  590 | `		ph7_class *pCallerScope;` |
| 20209 |  591 | `		while( pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH) ) ){` |
|     - |  592 | `			/* Safely ignore the exception frame */` |
|    21 |  593 | `			pFrame = pFrame->pParent;` |
|     1 |  594 | `		}` |
| 20189 |  595 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|     - |  596 | `		/* The calling scope is the executing method's declaring class — OR, for a bound closure` |
|     - |  597 | `		 * (Closure::bindTo/call), the explicit scope override carried on the frame (Increment 2). */` |
| 20189 |  598 | `		if( pFrame->pBoundScope ){` |
|    15 |  599 | `			pCallerScope = pFrame->pBoundScope;` |
| 20182 |  600 | `		}else if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
| 20133 |  601 | `			pCallerScope = (ph7_class *)pVmFunc->pUserData;` |
| 10108 |  602 | `		}else if( pVm->pConstEvalClass ){` |
|     - |  603 | `			/* Constant/property initializer bytecode runs without a method` |
|     - |  604 | `			 * frame; its scope is the class being initialized (php: a private` |
|     - |  605 | `			 * constant is reachable from its own class's initializers). */` |
|     3 |  606 | `			pCallerScope = pVm->pConstEvalClass;` |
|     2 |  607 | `		}else{` |
|    42 |  608 | `			goto dis; /* Not in a class scope: access is forbidden */` |
|     - |  609 | `		}` |
| 20149 |  610 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     - |  611 | `			/* php grants private access by DECLARING class: the caller's own` |
|     - |  612 | `			 * class must declare a private attribute of this name (a base` |
|     - |  613 | `			 * method touching its own private on a CHILD instance passes; a` |
|     - |  614 | `			 * child method touching an inherited base-private fails). An attr` |
|     - |  615 | `			 * whose declaring "class" is a TRAIT behaves as if declared by the` |
|     - |  616 | `			 * adopting class. Fallbacks: the caller being a trait used by the` |
|     - |  617 | `			 * instance's class (legacy trait-body scope), or — when the caller` |
|     - |  618 | `			 * class carries no such attr entry at all — the legacy exact-class` |
|     - |  619 | `			 * match (dynamic props and other non-declared shapes). */` |
|  1159 |  620 | `			ph7_class *pCaller = pCallerScope;` |
|  1736 |  621 | `			SyHashEntry *pOwnE = SyHashGet(&pCaller->hAttr,` |
|  1154 |  622 | `				(const void *)pAttrName->zString,pAttrName->nByte);` |
|  1159 |  623 | `			ph7_class_attr *pOwn = pOwnE ? (ph7_class_attr *)pOwnE->pUserData : 0;` |
|  1159 |  624 | `			int bGranted = 0;` |
|  1159 |  625 | `			if( pOwn && pOwn->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|  1024 |  626 | `				if( pOwn->pDeclClass == 0` |
|  1024 |  627 | `				 \|\| pOwn->pDeclClass == pCaller` |
|   536 |  628 | `				 \|\| (pOwn->pDeclClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|  1025 |  629 | `					bGranted = 1;` |
|   515 |  630 | `				}` |
|   644 |  631 | `			}else if( pOwn == 0 && pCaller == pClass ){` |
|   109 |  632 | `				bGranted = 1;` |
|    54 |  633 | `			}` |
|  1159 |  634 | `			if( !bGranted ){` |
|     - |  635 | `				/* Check if the caller is a trait used by pClass */` |
|     - |  636 | `				ph7_class **apTrait;` |
|     - |  637 | `				sxu32 nTrait,k;` |
|    28 |  638 | `				apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|    28 |  639 | `				nTrait = SySetUsed(&pClass->aTrait);` |
|    28 |  640 | `				for(k = 0; k < nTrait; k++){` |
|    21 |  641 | `					if( apTrait[k] == pCaller ){` |
|    21 |  642 | `						bGranted = 1;` |
|    21 |  643 | `						break;` |
|     - |  644 | `					}` |
|   ! 0 |  645 | `				}` |
|    13 |  646 | `			}` |
|  1159 |  647 | `			if( !bGranted ){` |
|     8 |  648 | `				goto dis; /* Access is forbidden */` |
|     - |  649 | `			}` |
|   579 |  650 | `		}else{` |
|     - |  651 | `			/* Protected */` |
| 18995 |  652 | `			ph7_class *pBase = pCallerScope;` |
|     - |  653 | `			/* Must be in the same class hierarchy */` |
| 18995 |  654 | `			if( !PH7_VmInstanceOf(pClass,pBase) && !PH7_VmInstanceOf(pBase,pClass) ){` |
|   ! 0 |  655 | `				goto dis; /* Access is forbidden */` |
|     - |  656 | `			}` |
|     - |  657 | `		}` |
| 10069 |  658 | `	}` |
| 26001 |  659 | `	return 1; /* Access is granted */` |
|    23 |  660 | `dis:` |
|    48 |  661 | `	if( bLog ){` |
|   ! 0 |  662 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|     - |  663 | `			"Access to the class attribute '%z->%z' is forbidden",` |
|   ! 0 |  664 | `			&pClass->sName,pAttrName);` |
|   ! 0 |  665 | `	}` |
|    48 |  666 | `	return 0; /* Access is forbidden */` |
| 13026 |  667 | `}` |
|     - |  668 | `/*` |
|     - |  669 | ` * array get_class_vars(string/object $class_name)` |
|     - |  670 | ` *   Get the default properties of the class` |
|     - |  671 | ` * Parameters` |
|     - |  672 | ` *  class_name` |
|     - |  673 | ` *   The class name or class instance` |
|     - |  674 | ` * Return` |
|     - |  675 | ` *  Returns an associative array of declared properties visible from the current scope` |
|     - |  676 | ` *  with their default value. The resulting array elements are in the form` |
|     - |  677 | ` *  of varname => value.` |
|     - |  678 | ` * Note:` |
|     - |  679 | ` *   NULL is returned on failure.` |
|     - |  680 | ` */` |
|     4 |  681 | `PH7_PRIVATE int vm_builtin_get_class_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  682 | `{` |
|     - |  683 | `	ph7_value *pName,*pArray,sValue;` |
|     - |  684 | `	SyHashEntry *pEntry;` |
|     - |  685 | `	ph7_class *pClass;` |
|     - |  686 | `	/* Extract the target class first */` |
|     5 |  687 | `	pClass = 0;` |
|     5 |  688 | `	if( nArg > 0 ){` |
|     5 |  689 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|     2 |  690 | `	}` |
|     5 |  691 | `	if( pClass == 0 ){` |
|     - |  692 | `		/* No such class,return NULL */` |
|   ! 0 |  693 | `		ph7_result_null(pCtx);` |
|   ! 0 |  694 | `		return PH7_OK;` |
|     - |  695 | `	}` |
|     - |  696 | `	/* Create a new array  */` |
|     5 |  697 | `	pArray = ph7_context_new_array(pCtx);` |
|     5 |  698 | `	pName = ph7_context_new_scalar(pCtx);` |
|     5 |  699 | `	PH7_MemObjInit(pCtx->pVm,&sValue);` |
|     5 |  700 | `	if( pArray == 0 \|\| pName == 0){` |
|     - |  701 | `		/* Out of memory,return NULL */` |
|   ! 0 |  702 | `		ph7_result_null(pCtx);` |
|   ! 0 |  703 | `		return PH7_OK;` |
|     - |  704 | `	}` |
|     - |  705 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|     5 |  706 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|    13 |  707 | `	while((pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|     9 |  708 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     9 |  709 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|     - |  710 | `			/* php 8.4: VIRTUAL hooked properties have no backing store —` |
|     - |  711 | `			 * get_class_vars() excludes them (raw surface) */` |
|     3 |  712 | `			continue;` |
|     - |  713 | `		}` |
|     - |  714 | `		/* Check if the access is allowed */` |
|     7 |  715 | `		if( PH7_VmClassMemberAccess(pCtx->pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|     7 |  716 | `			SyString *pAttrName = &pAttr->sName;` |
|     7 |  717 | `			ph7_value *pValue = 0;` |
|     7 |  718 | `			if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|     - |  719 | `				/* Static slots are computed at mount; constants lazily */` |
|     5 |  720 | `				PH7_VmMaterializeClassConst(pCtx->pVm,pClass,pAttr);` |
|     5 |  721 | `				pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pAttr->nIdx);` |
|     3 |  722 | `			}else{` |
|     3 |  723 | `				if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|     3 |  724 | `					PH7_MemObjRelease(&sValue);` |
|     - |  725 | `					/* Compute default value (any complex expression) associated with this attribute */` |
|     3 |  726 | `					VmLocalExec(pCtx->pVm,&pAttr->aByteCode,&sValue,FALSE);` |
|     3 |  727 | `					pValue = &sValue;` |
|     1 |  728 | `				}` |
|     - |  729 | `			}` |
|     - |  730 | `			/* Fill in the array */` |
|     7 |  731 | `			ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|     7 |  732 | `			ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|     - |  733 | `			/* Reset the cursor */` |
|     7 |  734 | `			ph7_value_reset_string_cursor(pName);` |
|     3 |  735 | `		}` |
|     1 |  736 | `	}` |
|     5 |  737 | `	PH7_MemObjRelease(&sValue);` |
|     - |  738 | `	/* Return the created array */` |
|     5 |  739 | `	ph7_result_value(pCtx,pArray);` |
|     - |  740 | `	/*` |
|     - |  741 | `	 * Don't worry about freeing memory here,everything will be relased` |
|     - |  742 | `	 * automatically as soon we return from this foreign function.` |
|     - |  743 | `	 */` |
|     5 |  744 | `	return PH7_OK;` |
|     3 |  745 | `}` |
|     - |  746 | `/*` |
|     - |  747 | ` * array get_object_vars(object $this)` |
|     - |  748 | ` *   Gets the properties of the given object` |
|     - |  749 | ` * Parameters` |
|     - |  750 | ` *  this` |
|     - |  751 | ` *   A class instance` |
|     - |  752 | ` * Return` |
|     - |  753 | ` *  Returns an associative array of defined object accessible non-static properties` |
|     - |  754 | ` *  for the specified object in scope. If a property have not been assigned a value` |
|     - |  755 | ` *  it will be returned with a NULL value.` |
|     - |  756 | ` * Note:` |
|     - |  757 | ` *   NULL is returned on failure.` |
|     - |  758 | ` */` |
|    24 |  759 | `PH7_PRIVATE int vm_builtin_get_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  760 | `{` |
|    25 |  761 | `	ph7_class_instance *pThis = 0;` |
|     - |  762 | `	ph7_value *pName,*pArray;` |
|     - |  763 | `	SyHashEntry *pEntry;` |
|    25 |  764 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|     - |  765 | `		/* Extract the target instance */` |
|    25 |  766 | `		pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    12 |  767 | `	}` |
|    25 |  768 | `	if( pThis == 0 ){` |
|     - |  769 | `		/* No such instance,return NULL */` |
|   ! 0 |  770 | `		ph7_result_null(pCtx);` |
|   ! 0 |  771 | `		return PH7_OK;` |
|     - |  772 | `	}` |
|     - |  773 | `	/* Create a new array  */` |
|    25 |  774 | `	pArray = ph7_context_new_array(pCtx);` |
|    25 |  775 | `	pName = ph7_context_new_scalar(pCtx);` |
|    25 |  776 | `	if( pArray == 0 \|\| pName == 0){` |
|     - |  777 | `		/* Out of memory,return NULL */` |
|   ! 0 |  778 | `		ph7_result_null(pCtx);` |
|   ! 0 |  779 | `		return PH7_OK;` |
|     - |  780 | `	}` |
|     - |  781 | `	/* Fill the array with the defined attribute visible from the current scope.` |
|     - |  782 | `	 * SNAPSHOT the attribute names first: a PHP 8.4 get hook dispatched mid-walk` |
|     - |  783 | `	 * runs user code that may re-enter an hAttr walk on this instance (resetting` |
|     - |  784 | `	 * the hash's single embedded loop cursor) or unset()/create properties. The` |
|     - |  785 | `	 * names point into CLASS-owned attr storage (they outlive instance mutation);` |
|     - |  786 | `	 * each is re-looked-up before use so an entry unset by an earlier hook is` |
|     - |  787 | `	 * skipped instead of read after free. */` |
|     - |  788 | `	{` |
|     - |  789 | `		SySet sNames;` |
|     - |  790 | `		SyString *aName;` |
|     - |  791 | `		sxu32 iName,nName;` |
|    25 |  792 | `		SySetInit(&sNames,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|    25 |  793 | `		SyHashResetLoopCursor(&pThis->hAttr);` |
|    95 |  794 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|    71 |  795 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    71 |  796 | `			if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|     - |  797 | `				/* Only non-static/constant attributes are extracted */` |
|    11 |  798 | `				continue;` |
|     - |  799 | `			}` |
|    60 |  800 | `			if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|    31 |  801 | `			 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|     3 |  802 | `				continue; /* virtual set-only property: no value to expose (php) */` |
|     - |  803 | `			}` |
|    59 |  804 | `			SySetPut(&sNames,(const void *)&pVmAttr->pAttr->sName);` |
|     1 |  805 | `		}` |
|    25 |  806 | `		aName = (SyString *)SySetBasePtr(&sNames);` |
|    25 |  807 | `		nName = SySetUsed(&sNames);` |
|    83 |  808 | `		for( iName = 0 ; iName < nName ; ++iName ){` |
|    59 |  809 | `			SyString *pAttrName = &aName[iName];` |
|     - |  810 | `			VmClassAttr *pVmAttr;` |
|    59 |  811 | `			pEntry = SyHashGet(&pThis->hAttr,(const void *)pAttrName->zString,pAttrName->nByte);` |
|    59 |  812 | `			if( pEntry == 0 ){` |
|   ! 0 |  813 | `				continue; /* unset by an earlier hook */` |
|     - |  814 | `			}` |
|    59 |  815 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     - |  816 | `			/* Check if the access is allowed */` |
|    59 |  817 | `			if( PH7_VmClassMemberAccess(pCtx->pVm,pThis->pClass,pAttrName,pVmAttr->pAttr->iProtection,FALSE) ){` |
|    47 |  818 | `				ph7_value *pValue = 0;` |
|     - |  819 | `				ph7_value sHookVal;` |
|     - |  820 | `				sxi32 rcHk;` |
|     - |  821 | `				/* PHP 8.4 property hooks: get_object_vars() reads through the get` |
|     - |  822 | `				 * hook (virtual properties included); raw slot otherwise. */` |
|    47 |  823 | `				PH7_MemObjInit(pCtx->pVm,&sHookVal);` |
|    47 |  824 | `				rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|    47 |  825 | `				if( rcHk == SXRET_OK ){` |
|    15 |  826 | `					pValue = &sHookVal;` |
|    40 |  827 | `				}else if( rcHk == SXERR_NOTFOUND ){` |
|     - |  828 | `					/* Extract attribute */` |
|    33 |  829 | `					pValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|    17 |  830 | `				}else{` |
|     - |  831 | `					/* the hook threw — parked on the boundary rail; php aborts the` |
|     - |  832 | `					 * whole builtin at the first throw (the helper's boundary gate` |
|     - |  833 | `					 * keeps LATER hooks from running; raw values it falls back to` |
|     - |  834 | `					 * are discarded when the throw routes) */` |
|   ! 0 |  835 | `					PH7_MemObjRelease(&sHookVal);` |
|   ! 0 |  836 | `					break;` |
|     - |  837 | `				}` |
|    47 |  838 | `				if( pValue ){` |
|     - |  839 | `					/* Insert attribute name in the array */` |
|    47 |  840 | `					ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|    47 |  841 | `					ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|    23 |  842 | `				}` |
|    47 |  843 | `				PH7_MemObjRelease(&sHookVal);` |
|     - |  844 | `				/* Reset the cursor */` |
|    47 |  845 | `				ph7_value_reset_string_cursor(pName);` |
|    23 |  846 | `			}` |
|    30 |  847 | `		}` |
|    25 |  848 | `		SySetRelease(&sNames);` |
|     - |  849 | `	}` |
|     - |  850 | `	/* Return the created array */` |
|    25 |  851 | `	ph7_result_value(pCtx,pArray);` |
|     - |  852 | `	/*` |
|     - |  853 | `	 * Don't worry about freeing memory here,everything will be relased` |
|     - |  854 | `	 * automatically as soon we return from this foreign function.` |
|     - |  855 | `	 */` |
|    25 |  856 | `	return PH7_OK;` |
|    13 |  857 | `}` |
|     - |  858 | ``/* Bound on `extends` chain depth — matches PH7_THROWABLE_WALK_MAX_DEPTH in`` |
|     - |  859 | ` * compile.c. Defends against compiler cycles even though interface cycle` |
|     - |  860 | ` * detection should reject them up front. */` |
|     - |  861 | `#define PH7_INTERFACE_WALK_MAX_DEPTH 64` |
|     - |  862 | `/*` |
|     - |  863 | ` * This function returns TRUE if the given class is an implemented` |
|     - |  864 | ` * interface.Otherwise FALSE is returned.` |
|     - |  865 | ` */` |
| 18092 |  866 | `static int VmQueryInterfaceSet(ph7_class *pClass,SySet *pSet)` |
|     5 |  867 | `{` |
|     - |  868 | `	ph7_class **apInterface;` |
|     - |  869 | `	sxu32 n;` |
| 18097 |  870 | `	if( SySetUsed(pSet) < 1 ){` |
|     - |  871 | `		/* Empty interface container */` |
|   265 |  872 | `		return FALSE;` |
|     - |  873 | `	}` |
|     - |  874 | `	/* Point to the set of implemented interfaces */` |
| 17837 |  875 | `	apInterface = (ph7_class **)SySetBasePtr(pSet);` |
|     - |  876 | `	/* Perform the lookup, walking each interface's parent chain so that` |
|     - |  877 | `	 * Iterator extends Traversable (and similar) is recognized. */` |
| 34259 |  878 | `	for( n = 0 ; n < SySetUsed(pSet) ; n++ ){` |
| 17979 |  879 | `		ph7_class *pIface = apInterface[n];` |
| 17979 |  880 | `		int iDepth = 0;` |
| 34505 |  881 | `		while( pIface && iDepth <= PH7_INTERFACE_WALK_MAX_DEPTH ){` |
| 18083 |  882 | `			if( pIface == pClass ){` |
|  1557 |  883 | `				return TRUE;` |
|     - |  884 | `			}` |
| 16531 |  885 | `			pIface = pIface->pBase;` |
| 16531 |  886 | `			iDepth++;` |
|     5 |  887 | `		}` |
|  8216 |  888 | `	}` |
| 16285 |  889 | `	return FALSE;` |
|  9051 |  890 | `}` |
|     - |  891 | `/*` |
|     - |  892 | ` * This function returns TRUE if the given class (first argument)` |
|     - |  893 | ` * is an instance of the main class (second argument).` |
|     - |  894 | ` * Otherwise FALSE is returned.` |
|     - |  895 | ` */` |
| 22758 |  896 | `PH7_PRIVATE int PH7_VmInstanceOf(ph7_class *pThis,ph7_class *pClass)` |
|     5 |  897 | `{` |
|     - |  898 | `	ph7_class *pParent;` |
|     - |  899 | `	sxi32 rc;` |
| 22763 |  900 | `	if( pThis == pClass ){` |
|     - |  901 | `		/* Instance of the same class */` |
|  9275 |  902 | `		return TRUE;` |
|     - |  903 | `	}` |
|     - |  904 | `	/* Check implemented interfaces */` |
| 13493 |  905 | `	rc = VmQueryInterfaceSet(pClass,&pThis->aInterface);` |
| 13493 |  906 | `	if( rc ){` |
|   989 |  907 | `		return TRUE;` |
|     - |  908 | `	}` |
|     - |  909 | `	/* Check parent classes */` |
| 12509 |  910 | `	pParent = pThis->pBase;` |
| 16537 |  911 | `	while( pParent ){` |
| 16255 |  912 | `		if( pParent == pClass ){` |
|     - |  913 | `			/* Same instance */` |
| 11659 |  914 | `			return TRUE;` |
|     - |  915 | `		}` |
|     - |  916 | `		/* Check the implemented interfaces */` |
|  4601 |  917 | `		rc = VmQueryInterfaceSet(pClass,&pParent->aInterface);` |
|  4601 |  918 | `		if( rc ){` |
|   573 |  919 | `			return TRUE;` |
|     - |  920 | `		}` |
|     - |  921 | `		/* Point to the parent class */` |
|  4033 |  922 | `		pParent = pParent->pBase;` |
|     5 |  923 | `	}` |
|     - |  924 | `	/* Not an instance of the the given class */` |
|   287 |  925 | `	return FALSE;` |
| 11384 |  926 | `}` |
|     - |  927 | `/*` |
|     - |  928 | ` * This function returns TRUE if the given class (first argument)` |
|     - |  929 | ` * is a subclass of the main class (second argument).` |
|     - |  930 | ` * Otherwise FALSE is returned.` |
|     - |  931 | ` */` |
|    12 |  932 | `static int VmSubclassOf(ph7_class *pClass,ph7_class *pBase)` |
|     1 |  933 | `{` |
|    13 |  934 | `	SySet *pInterface = &pClass->aInterface;` |
|     - |  935 | `	SyHashEntry *pEntry;` |
|     - |  936 | `	SyString *pName;` |
|     - |  937 | `	sxi32 rc;` |
|    21 |  938 | `	while( pClass ){` |
|    13 |  939 | `		pName = &pClass->sName;` |
|     - |  940 | `		/* Query the derived hashtable */` |
|    13 |  941 | `		pEntry = SyHashGet(&pBase->hDerived,(const void *)pName->zString,pName->nByte);` |
|    13 |  942 | `		if( pEntry ){` |
|     5 |  943 | `			return TRUE;` |
|     - |  944 | `		}` |
|     9 |  945 | `		pClass = pClass->pBase;` |
|     1 |  946 | `	}` |
|     9 |  947 | `	rc = VmQueryInterfaceSet(pBase,pInterface);` |
|     9 |  948 | `	if( rc ){` |
|   ! 0 |  949 | `		return TRUE;` |
|     - |  950 | `	}` |
|     - |  951 | `	/* Not a subclass */` |
|     9 |  952 | `	return FALSE;` |
|     7 |  953 | `}` |
|     - |  954 | `/*` |
|     - |  955 | ` * bool is_a(object $object,string $class_name)` |
|     - |  956 | ` *   Checks if the object is of this class or has this class as one of its parents.` |
|     - |  957 | ` * Parameters` |
|     - |  958 | ` *  object` |
|     - |  959 | ` *   The tested object` |
|     - |  960 | ` * class_name` |
|     - |  961 | ` *  The class name` |
|     - |  962 | ` * Return` |
|     - |  963 | ` *   Returns TRUE if the object is of this class or has this class as one of its` |
|     - |  964 | ` *   parents, FALSE otherwise.` |
|     - |  965 | ` */` |
|    18 |  966 | `PH7_PRIVATE int vm_builtin_is_a(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  967 | `{` |
|    19 |  968 | `	int res = 0; /* Assume FALSE by default */` |
|    19 |  969 | `	if( nArg > 1 && ph7_value_is_object(apArg[0])  ){` |
|    19 |  970 | `		ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     - |  971 | `		ph7_class *pClass;` |
|     - |  972 | `		/* Extract the given class */` |
|    19 |  973 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|    19 |  974 | `		if( pClass ){` |
|     - |  975 | `			/* Perform the query */` |
|    19 |  976 | `			res = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|     9 |  977 | `		}` |
|     9 |  978 | `	}` |
|     - |  979 | `	/* Query result */` |
|    19 |  980 | `	ph7_result_bool(pCtx,res);` |
|    19 |  981 | `	return PH7_OK;` |
|     1 |  982 | `}` |
|     - |  983 | `/*` |
|     - |  984 | ` * int spl_object_id(object $object)` |
|     - |  985 | ` *  Return the integer object handle (per-instance id) of the given object.` |
|     - |  986 | ` * PHL note: PHP 8 throws a TypeError when passed a non-object; PHL returns NULL` |
|     - |  987 | ` * to stay consistent with the engine's graceful-degradation convention.` |
|     - |  988 | ` */` |
|    18 |  989 | `PH7_PRIVATE int vm_builtin_spl_object_id(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  990 | `{` |
|     - |  991 | `	ph7_class_instance *pThis;` |
|    21 |  992 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|   ! 0 |  993 | `		ph7_result_null(pCtx);` |
|   ! 0 |  994 | `		return PH7_OK;` |
|     - |  995 | `	}` |
|    21 |  996 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    21 |  997 | `	ph7_result_int64(pCtx,(ph7_int64)pThis->nObjId);` |
|    21 |  998 | `	return PH7_OK;` |
|    12 |  999 | `}` |
|     - | 1000 | `/*` |
|     - | 1001 | ` * string spl_object_hash(object $object)` |
|     - | 1002 | ` *  Return a 32-char hex identifier, unique and stable per live object.` |
|     - | 1003 | ` * PHL note: PHP derives this from the internal handle plus a per-process key, so` |
|     - | 1004 | ` * the exact value is NOT reproducible. PHL returns the zero-padded object id,` |
|     - | 1005 | ` * which preserves the only guaranteed properties: unique per live object, stable` |
|     - | 1006 | ` * across calls, and distinct objects -> distinct strings. A non-object returns` |
|     - | 1007 | ` * NULL (PHP 8 throws a TypeError; see spl_object_id above).` |
|     - | 1008 | ` */` |
|    10 | 1009 | `PH7_PRIVATE int vm_builtin_spl_object_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1010 | `{` |
|     - | 1011 | `	ph7_class_instance *pThis;` |
|    11 | 1012 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|   ! 0 | 1013 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1014 | `		return PH7_OK;` |
|     - | 1015 | `	}` |
|    11 | 1016 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    11 | 1017 | `	ph7_result_string_format(pCtx,"%08x%08x%08x%08x",0,0,0,(unsigned int)pThis->nObjId);` |
|    11 | 1018 | `	return PH7_OK;` |
|     6 | 1019 | `}` |
|     - | 1020 | `/*` |
|     - | 1021 | ` * bool is_subclass_of(object/string $object,object/string $class_name)` |
|     - | 1022 | ` *   Checks if the object has this class as one of its parents.` |
|     - | 1023 | ` * Parameters` |
|     - | 1024 | ` *  object` |
|     - | 1025 | ` *   The tested object` |
|     - | 1026 | ` * class_name` |
|     - | 1027 | ` *  The class name` |
|     - | 1028 | ` * Return` |
|     - | 1029 | ` *  This function returns TRUE if the object , belongs to a class` |
|     - | 1030 | ` *  which is a subclass of class_name, FALSE otherwise.` |
|     - | 1031 | ` */` |
|    14 | 1032 | `PH7_PRIVATE int vm_builtin_is_subclass_of(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1033 | `{` |
|    15 | 1034 | `	int res = 0; /* Assume FALSE by default */` |
|    15 | 1035 | `	if( nArg > 1 ){` |
|     - | 1036 | `		ph7_class *pClass,*pMain;` |
|     - | 1037 | `		/* Extract the given classes */` |
|    15 | 1038 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|    15 | 1039 | `		pMain = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|    15 | 1040 | `		if( pClass && pMain ){` |
|     - | 1041 | `			/* Perform the query */` |
|    13 | 1042 | `			res = VmSubclassOf(pClass,pMain);` |
|     6 | 1043 | `		}` |
|     7 | 1044 | `	}` |
|     - | 1045 | `	/* Query result */` |
|    15 | 1046 | `	ph7_result_bool(pCtx,res);` |
|    15 | 1047 | `	return PH7_OK;` |
|     1 | 1048 | `}` |
|    54 | 1049 | `PH7_PRIVATE int vm_builtin_call_user_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1050 | `{` |
|     - | 1051 | `	ph7_value sResult; /* Store callback return value here */` |
|     - | 1052 | `	sxi32 rc;` |
|    55 | 1053 | `	if( nArg < 1 ){` |
|     - | 1054 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 1055 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1056 | `		return PH7_OK;` |
|     - | 1057 | `	}` |
|    55 | 1058 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|    55 | 1059 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|     - | 1060 | `	/* Try to invoke the callback. If the call_user_func() call site used` |
|     - | 1061 | `	 * name: arguments (e.g. call_user_func('f', b: 9)), forward them to the` |
|     - | 1062 | `	 * callback. The inner call's argument i is the outer argument i+1 (outer` |
|     - | 1063 | `	 * argument 0 is the callback), so the inner name array is simply the outer` |
|     - | 1064 | `	 * names shifted by one — no copy needed: VmResolveNamedArgs treats any index` |
|     - | 1065 | `	 * >= nTotal as positional, so a shorter map covers the callback's args. */` |
|    64 | 1066 | `	if( pCtx->pArgMap && pCtx->pArgMap->bHasNamed && nArg > 1 ){` |
|    19 | 1067 | `		VmCallArgMap *pOuter = pCtx->pArgMap;` |
|     - | 1068 | `		VmCallArgMap sInner;` |
|    19 | 1069 | `		sInner.bHasNamed = 1;` |
|    19 | 1070 | `		sInner.bIsNamespaced = 0;` |
|     - | 1071 | `		/* Named args to call_user_func coerce in WEAK mode even from a` |
|     - | 1072 | `		 * strict_types=1 caller (verified vs php 8.5.7): a name: argument` |
|     - | 1073 | `		 * collected into the variadic and re-spread loses the strict context.` |
|     - | 1074 | `		 * call_user_func_array does NOT share this quirk (it stays strict). */` |
|    19 | 1075 | `		sInner.bStrict = 0;` |
|    19 | 1076 | `		sInner.nTotal = pOuter->nTotal > 1 ? pOuter->nTotal - 1 : 0;` |
|    19 | 1077 | `		sInner.aNames = sInner.nTotal > 0 ? &pOuter->aNames[1] : 0;` |
|    19 | 1078 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult,&sInner);` |
|    10 | 1079 | `	}else{` |
|    37 | 1080 | `		rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult);` |
|     - | 1081 | `	}` |
|    55 | 1082 | `	if( rc == PH7_EXCEPTION ){` |
|     - | 1083 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds` |
|     - | 1084 | `		 * through the nearest try/catch instead of returning FALSE. */` |
|     7 | 1085 | `		PH7_MemObjRelease(&sResult);` |
|     7 | 1086 | `		return PH7_EXCEPTION;` |
|     - | 1087 | `	}` |
|    49 | 1088 | `	if( rc != SXRET_OK ){` |
|     - | 1089 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|   ! 0 | 1090 | `		ph7_result_bool(pCtx,0); /* return false */` |
|   ! 0 | 1091 | `	}else{` |
|     - | 1092 | `		/* Callback result */` |
|    49 | 1093 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|     - | 1094 | `	}` |
|    49 | 1095 | `	PH7_MemObjRelease(&sResult);` |
|    49 | 1096 | `	return PH7_OK;` |
|    28 | 1097 | `}` |
|     - | 1098 | `/*` |
|     - | 1099 | ` * value call_user_func_array(callable $callback,array $param_arr)` |
|     - | 1100 | ` *  Call a callback with an array of parameters.` |
|     - | 1101 | ` * Parameter` |
|     - | 1102 | ` *  $callback` |
|     - | 1103 | ` *   The callable to be called.` |
|     - | 1104 | ` * $param_arr` |
|     - | 1105 | ` *  The parameters to be passed to the callback, as an indexed array.` |
|     - | 1106 | ` * Return` |
|     - | 1107 | ` *  Returns the return value of the callback, or FALSE on error.` |
|     - | 1108 | ` */` |
|    34 | 1109 | `PH7_PRIVATE int vm_builtin_call_user_func_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1110 | `{` |
|     - | 1111 | `	ph7_hashmap_node *pEntry; /* Current hashmap entry */` |
|     - | 1112 | `	ph7_value *pValue,sResult;/* Store callback return value here */` |
|     - | 1113 | `	ph7_hashmap *pMap;        /* Target hashmap */` |
|     - | 1114 | `	SySet aArg;               /* Argument value pointers */` |
|    35 | 1115 | `	SyString *aNames = 0;     /* Name map, lazily allocated when a string key appears */` |
|    35 | 1116 | `	sxu32 nSlot = 0;          /* Number of collected arguments */` |
|     - | 1117 | `	sxi32 rc;` |
|     - | 1118 | `	sxu32 n;` |
|    35 | 1119 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|     - | 1120 | `		/* Missing/Invalid arguments,return FALSE */` |
|   ! 0 | 1121 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1122 | `		return PH7_OK;` |
|     - | 1123 | `	}` |
|    35 | 1124 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|    35 | 1125 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|     - | 1126 | `	/* Initialize the arguments container */` |
|    35 | 1127 | `	SySetInit(&aArg,&pCtx->pVm->sAllocator,sizeof(ph7_value *));` |
|     - | 1128 | `	/* Turn hashmap entries into callback arguments. A string key becomes a` |
|     - | 1129 | `	 * named argument (PHP 8: call_user_func_array($cb, ['b' => 9])), an integer` |
|     - | 1130 | `	 * key stays positional. The name map points straight at each node's key` |
|     - | 1131 | `	 * blob: the source array stays pinned on the operand stack for the whole` |
|     - | 1132 | `	 * call, so the blobs outlive argument binding. A pure list array (no string` |
|     - | 1133 | `	 * keys) never allocates aNames and takes the plain positional path. */` |
|    35 | 1134 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|    35 | 1135 | `	pEntry = pMap->pFirst; /* First inserted entry */` |
|   189 | 1136 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     - | 1137 | `		/* Extract node value */` |
|   155 | 1138 | `		if( (pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx)) != 0 ){` |
|   155 | 1139 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|    23 | 1140 | `				if( aNames == 0 ){` |
|     - | 1141 | `					/* First string key: allocate the whole map, zeroed so every` |
|     - | 1142 | `					 * not-yet-seen slot defaults to positional. */` |
|    13 | 1143 | `					aNames = (SyString *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,pMap->nEntry * sizeof(SyString));` |
|    13 | 1144 | `					if( aNames == 0 ){` |
|   ! 0 | 1145 | `						SySetRelease(&aArg);` |
|   ! 0 | 1146 | `						PH7_MemObjRelease(&sResult);` |
|   ! 0 | 1147 | `						return PH7_ContextMemoryError(pCtx);` |
|     - | 1148 | `					}` |
|    13 | 1149 | `					SyZero(aNames,pMap->nEntry * sizeof(SyString));` |
|     6 | 1150 | `				}` |
|    23 | 1151 | `				SyStringInitFromBuf(&aNames[nSlot],SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    11 | 1152 | `			}` |
|   155 | 1153 | `			SySetPut(&aArg,(const void *)&pValue);` |
|   155 | 1154 | `			nSlot++;` |
|    77 | 1155 | `		}` |
|     - | 1156 | `		/* Point to the next entry */` |
|   155 | 1157 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    78 | 1158 | `	}` |
|     - | 1159 | `	/* Try to invoke the callback */` |
|    35 | 1160 | `	if( aNames ){` |
|     - | 1161 | `		VmCallArgMap sMap;` |
|    13 | 1162 | `		sMap.bHasNamed = 1;` |
|    13 | 1163 | `		sMap.bIsNamespaced = 0;` |
|     - | 1164 | `		/* Coercion strictness follows the caller's file; the OP_CALL dispatcher` |
|     - | 1165 | `		 * forwards the call site's map on pArgMap (0 only at non-OP_CALL sites). */` |
|    13 | 1166 | `		sMap.bStrict = (pCtx->pArgMap ? pCtx->pArgMap->bStrict : 0);` |
|    13 | 1167 | `		sMap.nTotal = nSlot;` |
|    13 | 1168 | `		sMap.aNames = aNames;` |
|    19 | 1169 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],(int)nSlot,` |
|    12 | 1170 | `			(ph7_value **)SySetBasePtr(&aArg),&sResult,&sMap);` |
|    13 | 1171 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,aNames);` |
|     7 | 1172 | `	}else{` |
|    34 | 1173 | `		rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],(int)nSlot,` |
|    22 | 1174 | `			(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|     - | 1175 | `	}` |
|    35 | 1176 | `	if( rc == PH7_EXCEPTION ){` |
|     - | 1177 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds. */` |
|     5 | 1178 | `		PH7_MemObjRelease(&sResult);` |
|     5 | 1179 | `		SySetRelease(&aArg);` |
|     5 | 1180 | `		return PH7_EXCEPTION;` |
|     - | 1181 | `	}` |
|    31 | 1182 | `	if( rc != SXRET_OK ){` |
|     - | 1183 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|   ! 0 | 1184 | `		ph7_result_bool(pCtx,0); /* return false */` |
|   ! 0 | 1185 | `	}else{` |
|     - | 1186 | `		/* Callback result */` |
|    31 | 1187 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|     - | 1188 | `	}` |
|     - | 1189 | `	/* Cleanup the mess left behind */` |
|    31 | 1190 | `	PH7_MemObjRelease(&sResult);` |
|    31 | 1191 | `	SySetRelease(&aArg);` |
|    31 | 1192 | `	return PH7_OK;` |
|    18 | 1193 | `}` |
|     - | 1194 |  |
