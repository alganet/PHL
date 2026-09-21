# src/ph7/vm_builtin_class.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 655/742 lines (88.27%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|     976 |    7 | `PH7_PRIVATE int vm_builtin_get_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |    8 | `{` |
|       - |    9 | `	ph7_class *pClass;` |
|       - |   10 | `	SyString *pName;` |
|     981 |   11 | `	if( nArg < 1 ){` |
|       - |   12 | `		/* Check if we are inside a class */` |
|     ! 0 |   13 | `		pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|     ! 0 |   14 | `		if( pClass ){` |
|       - |   15 | `			/* Point to the class name */` |
|     ! 0 |   16 | `			pName = &pClass->sName;` |
|     ! 0 |   17 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|     ! 0 |   18 | `		}else{` |
|       - |   19 | `			/* Not inside class,return FALSE */` |
|     ! 0 |   20 | `			ph7_result_bool(pCtx,0);` |
|       - |   21 | `		}` |
|     ! 0 |   22 | `	}else{` |
|       - |   23 | `		/* Extract the target class */` |
|     981 |   24 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|     981 |   25 | `		if( pClass ){` |
|     981 |   26 | `			pName = &pClass->sName;` |
|       - |   27 | `			/* Return the class name */` |
|     981 |   28 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|     493 |   29 | `		}else{` |
|       - |   30 | `			/* Not a class instance,return FALSE */` |
|     ! 0 |   31 | `			ph7_result_bool(pCtx,0);` |
|       - |   32 | `		}` |
|       - |   33 | `	}` |
|     981 |   34 | `	return PH7_OK;` |
|       5 |   35 | `}` |
|       - |   36 | `/*` |
|       - |   37 | ` * string get_parent_class([object $object = NULL ] )` |
|       - |   38 | ` *   Returns the name of the parent class of an object` |
|       - |   39 | ` * Parameters` |
|       - |   40 | ` *  object` |
|       - |   41 | ` *   The tested object. This parameter may be omitted when inside a class.` |
|       - |   42 | ` * Return` |
|       - |   43 | ` *  The name of the parent class of which object is an instance.` |
|       - |   44 | ` *  Returns FALSE if object is not an object or if the object does` |
|       - |   45 | ` *  not have a parent.` |
|       - |   46 | ` *  If object is omitted when inside a class, the name of that class is returned.` |
|       - |   47 | ` */` |
|      48 |   48 | `PH7_PRIVATE int vm_builtin_get_parent_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |   49 | `{` |
|       - |   50 | `	ph7_class *pClass;` |
|       - |   51 | `	SyString *pName;` |
|      51 |   52 | `	if( nArg < 1 ){` |
|       - |   53 | `		/* Check if we are inside a class [i.e: a method call]*/` |
|       3 |   54 | `		pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|       3 |   55 | `		if( pClass && pClass->pBase ){` |
|       - |   56 | `			/* Point to the class name */` |
|       3 |   57 | `			pName = &pClass->pBase->sName;` |
|       3 |   58 | `			ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|       2 |   59 | `		}else{` |
|       - |   60 | `			/* Not inside class,return FALSE */` |
|     ! 0 |   61 | `			ph7_result_bool(pCtx,0);` |
|       - |   62 | `		}` |
|       2 |   63 | `	}else{` |
|       - |   64 | `		/* Extract the target class */` |
|      49 |   65 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|      49 |   66 | `		if( pClass ){` |
|      49 |   67 | `			if( pClass->pBase ){` |
|      47 |   68 | `				pName = &pClass->pBase->sName;` |
|       - |   69 | `				/* Return the parent class name */` |
|      47 |   70 | `				ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|      25 |   71 | `			}else{` |
|       - |   72 | `				/* Object does not have a parent class */` |
|       3 |   73 | `				ph7_result_bool(pCtx,0);` |
|       - |   74 | `			}` |
|      26 |   75 | `		}else{` |
|       - |   76 | `			/* Not a class instance,return FALSE */` |
|     ! 0 |   77 | `			ph7_result_bool(pCtx,0);` |
|       - |   78 | `		}` |
|       - |   79 | `	}` |
|      51 |   80 | `	return PH7_OK;` |
|       3 |   81 | `}` |
|       - |   82 | `/*` |
|       - |   83 | ` * string get_called_class(void)` |
|       - |   84 | ` *   Gets the name of the class the static method is called in.` |
|       - |   85 | ` * Parameters` |
|       - |   86 | ` *  None.` |
|       - |   87 | ` * Return` |
|       - |   88 | ` *  Returns the class name. Returns FALSE if called from outside a class.` |
|       - |   89 | ` */` |
|       4 |   90 | `PH7_PRIVATE int vm_builtin_get_called_class(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |   91 | `{` |
|       - |   92 | `	ph7_class *pClass;` |
|       - |   93 | `	/* Check if we are inside a class [i.e: a method call] */` |
|       5 |   94 | `	pClass = PH7_VmPeekTopClass(pCtx->pVm);` |
|       5 |   95 | `	if( pClass ){` |
|       - |   96 | `		SyString *pName;` |
|       - |   97 | `		/* Point to the class name */` |
|       5 |   98 | `		pName = &pClass->sName;` |
|       5 |   99 | `		ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|       3 |  100 | `	}else{` |
|     ! 0 |  101 | `		SXUNUSED(nArg); /* cc warning */` |
|     ! 0 |  102 | `		SXUNUSED(apArg);` |
|       - |  103 | `		/* Not inside class,return FALSE */` |
|     ! 0 |  104 | `		ph7_result_bool(pCtx,0);` |
|       - |  105 | `	}` |
|       5 |  106 | `	return PH7_OK;` |
|       1 |  107 | `}` |
|       - |  108 | `/*` |
|       - |  109 | ` * Extract a ph7_class from the given ph7_value.` |
|       - |  110 | ` * The given value must be of type object [i.e: class instance] or` |
|       - |  111 | ` * string which hold the class name.` |
|       - |  112 | ` */` |
|  302216 |  113 | `PH7_PRIVATE ph7_class * PH7_VmExtractClassFromValue(ph7_vm *pVm,ph7_value *pArg)` |
|       5 |  114 | `{` |
|  302221 |  115 | `	ph7_class *pClass = 0;` |
|  302221 |  116 | `	if( ph7_value_is_object(pArg) ){` |
|       - |  117 | `		/* Class instance already loaded,no need to perform a lookup */` |
|  201175 |  118 | `		pClass = ((ph7_class_instance *)pArg->x.pOther)->pClass;` |
|  201636 |  119 | `	}else if( ph7_value_is_string(pArg) ){` |
|       - |  120 | `		const char *zClass;` |
|       - |  121 | `		int nLen;` |
|       - |  122 | `		/* Extract class name */` |
|  101049 |  123 | `		zClass = ph7_value_to_string(pArg,&nLen);` |
|       - |  124 | `		/* A leading '\' (the global-namespace anchor) is stripped by` |
|       - |  125 | `		 * PH7_VmExtractClass itself now (PH7_VmClassNameAnchor), so do not` |
|       - |  126 | `		 * strip here too — a second strip would wrongly resolve "\\Foo". */` |
|  101049 |  127 | `		if( nLen > 0 ){` |
|       - |  128 | `			/* Resolve through PH7_VmExtractClass so a class named by STRING is` |
|       - |  129 | `			 * autoloaded on a miss — php autoloads the class of a [class,method]` |
|       - |  130 | `			 * callable (is_callable/array_map/call_user_func), and of the class` |
|       - |  131 | `			 * argument to method_exists()/property_exists(). iLoadable=FALSE keeps` |
|       - |  132 | `			 * abstract classes and interfaces (a static method on an abstract class` |
|       - |  133 | `			 * is a valid callable). */` |
|  101049 |  134 | `			pClass = PH7_VmExtractClass(pVm,zClass,(sxu32)nLen,FALSE,0);` |
|   50522 |  135 | `		}` |
|   50522 |  136 | `	}` |
|  302221 |  137 | `	return pClass;` |
|       5 |  138 | `}` |
|       - |  139 | `/*` |
|       - |  140 | ` * bool property_exists(mixed $class,string $property)` |
|       - |  141 | ` *   Checks if the object or class has a property.` |
|       - |  142 | ` * Parameters` |
|       - |  143 | ` *  class` |
|       - |  144 | ` *   The class name or an object of the class to test for` |
|       - |  145 | ` * property` |
|       - |  146 | ` *  The name of the property` |
|       - |  147 | ` * Return` |
|       - |  148 | ` *   Returns TRUE if the property exists,FALSE otherwise.` |
|       - |  149 | ` */` |
|      18 |  150 | `PH7_PRIVATE int vm_builtin_property_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  151 | `{` |
|      20 |  152 | `	int res = 0; /* Assume attribute does not exists */` |
|      20 |  153 | `	if( nArg > 1 ){` |
|       - |  154 | `		ph7_class *pClass;` |
|      20 |  155 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|      20 |  156 | `		if( pClass ){` |
|       - |  157 | `			const char *zName;` |
|       - |  158 | `			int nLen;` |
|       - |  159 | `			/* Extract attribute name */` |
|      20 |  160 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|      20 |  161 | `			if( nLen > 0 ){` |
|       - |  162 | `				/* Perform the lookup in the attribute and method table */` |
|      18 |  163 | `				if( SyHashGet(&pClass->hAttr,(const void *)zName,(sxu32)nLen) != 0` |
|      14 |  164 | `					\|\| SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){` |
|       - |  165 | `						/* property exists,flag that */` |
|      13 |  166 | `						res = 1;` |
|       6 |  167 | `				}` |
|       - |  168 | `				/* A DYNAMIC (runtime-added) property lives on the INSTANCE's` |
|       - |  169 | `				 * attribute table, not the class's — php reports those too` |
|       - |  170 | `				 * (band A #3b; pre-fix property_exists() was blind to them). */` |
|      20 |  171 | `				if( res == 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|       3 |  172 | `					ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       3 |  173 | `					if( pThis && SyHashGet(&pThis->hAttr,(const void *)zName,(sxu32)nLen) != 0 ){` |
|     ! 0 |  174 | `						res = 1;` |
|     ! 0 |  175 | `					}` |
|       1 |  176 | `				}` |
|       9 |  177 | `			}` |
|       9 |  178 | `		}` |
|       9 |  179 | `	}` |
|      20 |  180 | `	ph7_result_bool(pCtx,res);` |
|      20 |  181 | `	return PH7_OK;` |
|       2 |  182 | `}` |
|       - |  183 | `/*` |
|       - |  184 | ` * bool method_exists(mixed $class,string $method)` |
|       - |  185 | ` *   Checks if the given method is a class member.` |
|       - |  186 | ` * Parameters` |
|       - |  187 | ` *  class` |
|       - |  188 | ` *   The class name or an object of the class to test for` |
|       - |  189 | ` * property` |
|       - |  190 | ` *  The name of the method` |
|       - |  191 | ` * Return` |
|       - |  192 | ` *   Returns TRUE if the method exists,FALSE otherwise.` |
|       - |  193 | ` */` |
|      14 |  194 | `PH7_PRIVATE int vm_builtin_method_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  195 | `{` |
|      16 |  196 | `	int res = 0; /* Assume method does not exists */` |
|      16 |  197 | `	if( nArg > 1 ){` |
|       - |  198 | `		ph7_class *pClass;` |
|      16 |  199 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|      16 |  200 | `		if( pClass ){` |
|       - |  201 | `			const char *zName;` |
|       - |  202 | `			int nLen;` |
|       - |  203 | `			/* Extract method name */` |
|      12 |  204 | `			zName = ph7_value_to_string(apArg[1],&nLen);` |
|      12 |  205 | `			if( nLen > 0 ){` |
|       - |  206 | `				/* Perform the lookup in the method table */` |
|      12 |  207 | `				if( SyHashGet(&pClass->hMethod,(const void *)zName,(sxu32)nLen) != 0 ){` |
|       - |  208 | `					/* method exists,flag that */` |
|      10 |  209 | `					res = 1;` |
|       4 |  210 | `				}` |
|       5 |  211 | `			}` |
|       5 |  212 | `		}` |
|       7 |  213 | `	}` |
|      16 |  214 | `	ph7_result_bool(pCtx,res);` |
|      16 |  215 | `	return PH7_OK;` |
|       2 |  216 | `}` |
|       - |  217 | `/*` |
|       - |  218 | ` * bool class_exists(string $class_name [, bool $autoload = true ] )` |
|       - |  219 | ` *   Checks if the class has been defined.` |
|       - |  220 | ` * Parameters` |
|       - |  221 | ` *  class_name` |
|       - |  222 | ` *   The class name. The name is matched in a case-sensitive manner` |
|       - |  223 | ` *   unlinke the standard PHP engine.` |
|       - |  224 | ` *  autoload` |
|       - |  225 | ` *   Whether or not to call __autoload by default.` |
|       - |  226 | ` * Return` |
|       - |  227 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|       - |  228 | ` */` |
|      96 |  229 | `PH7_PRIVATE int vm_builtin_class_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  230 | `{` |
|     101 |  231 | `	int res = 0; /* Assume class does not exist */` |
|     101 |  232 | `	if( nArg > 0 ){` |
|     101 |  233 | `		SyHashEntry *pEntry = 0;` |
|       - |  234 | `		const char *zName;` |
|       - |  235 | `		int nLen;` |
|     101 |  236 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|       - |  237 | `		sxu32 nName;` |
|       - |  238 | `		/* Extract given name */` |
|     101 |  239 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|     101 |  240 | `		if( nArg >= 2 ){` |
|       6 |  241 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|       2 |  242 | `		}` |
|       - |  243 | `		/* Strip a leading '\' (global-namespace anchor) — this builtin hashes` |
|       - |  244 | `		 * hClass directly, bypassing PH7_VmExtractClass, so anchor here. */` |
|     101 |  245 | `		nName = (sxu32)nLen;` |
|     101 |  246 | `		PH7_VmClassNameAnchor(&zName,&nName);` |
|     101 |  247 | `		if( nName > 0 ){` |
|       - |  248 | `			/* Perform a hash lookup first */` |
|      97 |  249 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|      46 |  250 | `		}` |
|       - |  251 | `		/* Gate autoload on the ORIGINAL length (nLen), not the stripped nName:` |
|       - |  252 | `		 * php autoloads a lone "\" (with the empty stripped name) but not "". */` |
|     101 |  253 | `		if( pEntry == 0 && nLen > 0 && iAutoload ){` |
|       - |  254 | `			/* Try autoload, then re-check */` |
|      28 |  255 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,nName,FALSE);` |
|      28 |  256 | `			if( pClass ){` |
|       9 |  257 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|       3 |  258 | `			}` |
|      12 |  259 | `		}` |
|     101 |  260 | `		if( pEntry ){` |
|       - |  261 | `			/* Walk the collision chain: return TRUE only for concrete or abstract classes,` |
|       - |  262 | `			 * not for interfaces or traits (matching PHP behavior). */` |
|      79 |  263 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|      81 |  264 | `			while( pClass ){` |
|      79 |  265 | `				if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0 ){` |
|      77 |  266 | `					res = 1;` |
|      77 |  267 | `					break;` |
|       - |  268 | `				}` |
|       3 |  269 | `				pClass = pClass->pNextName;` |
|       1 |  270 | `			}` |
|      37 |  271 | `		}` |
|      48 |  272 | `	}` |
|     101 |  273 | `	ph7_result_bool(pCtx,res);` |
|     101 |  274 | `	return PH7_OK;` |
|       5 |  275 | `}` |
|       - |  276 | `/*` |
|       - |  277 | ` * bool interface_exists(string $class_name [, bool $autoload = true ] )` |
|       - |  278 | ` *   Checks if the interface has been defined.` |
|       - |  279 | ` * Parameters` |
|       - |  280 | ` *  class_name` |
|       - |  281 | ` *   The class name. The name is matched in a case-sensitive manner` |
|       - |  282 | ` *   unlinke the standard PHP engine.` |
|       - |  283 | ` *  autoload` |
|       - |  284 | ` *   Whether or not to call __autoload by default.` |
|       - |  285 | ` * Return` |
|       - |  286 | ` *   TRUE if class_name is a defined class, FALSE otherwise.` |
|       - |  287 | ` */` |
|      32 |  288 | `PH7_PRIVATE int vm_builtin_interface_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  289 | `{` |
|      34 |  290 | `	int res = 0; /* Assume interface does not exist */` |
|      34 |  291 | `	if( nArg > 0 ){` |
|      34 |  292 | `		SyHashEntry *pEntry = 0;` |
|       - |  293 | `		const char *zName;` |
|       - |  294 | `		int nLen;` |
|      34 |  295 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|       - |  296 | `		sxu32 nName;` |
|       - |  297 | `		/* Extract given name */` |
|      34 |  298 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|      34 |  299 | `		if( nArg >= 2 ){` |
|     ! 0 |  300 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|     ! 0 |  301 | `		}` |
|       - |  302 | `		/* Strip a leading '\' (global-namespace anchor); this builtin bypasses` |
|       - |  303 | `		 * PH7_VmExtractClass and hashes hClass directly. */` |
|      34 |  304 | `		nName = (sxu32)nLen;` |
|      34 |  305 | `		PH7_VmClassNameAnchor(&zName,&nName);` |
|       - |  306 | `		/* Perform a hash lookup */` |
|      34 |  307 | `		if( nName > 0 ){` |
|      34 |  308 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|      16 |  309 | `		}` |
|       - |  310 | `		/* Gate autoload on the ORIGINAL length (nLen): php autoloads a lone` |
|       - |  311 | `		 * "\" with the empty stripped name, but not a truly empty "". */` |
|      34 |  312 | `		if( pEntry == 0 && nLen > 0 && iAutoload ){` |
|       - |  313 | `			/* Try autoload — pass iLoadable=FALSE so we get interfaces too */` |
|       3 |  314 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,nName,FALSE);` |
|       3 |  315 | `			if( pClass ){` |
|     ! 0 |  316 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|     ! 0 |  317 | `			}` |
|       1 |  318 | `		}` |
|      34 |  319 | `		if( pEntry ){` |
|      32 |  320 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|      34 |  321 | `			while( pClass ){` |
|      32 |  322 | `				if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|       - |  323 | `					/* interface is available */` |
|      30 |  324 | `					res = 1;` |
|      30 |  325 | `					break;` |
|       - |  326 | `				}` |
|       - |  327 | `				/* Next with the same name */` |
|       3 |  328 | `				pClass = pClass->pNextName;` |
|       1 |  329 | `			}` |
|      15 |  330 | `		}` |
|      16 |  331 | `	}` |
|      34 |  332 | `	ph7_result_bool(pCtx,res);` |
|      34 |  333 | `	return PH7_OK;` |
|       2 |  334 | `}` |
|       - |  335 | `/*` |
|       - |  336 | ` * bool trait_exists(string $trait [, bool $autoload = true ] )` |
|       - |  337 | ` *   Checks if the trait has been defined.` |
|       - |  338 | ` * Parameters` |
|       - |  339 | ` *  trait` |
|       - |  340 | ` *   The trait name (case-sensitive here, unlike the standard PHP engine).` |
|       - |  341 | ` *  autoload` |
|       - |  342 | ` *   Whether to invoke autoloading if the trait is not yet defined.` |
|       - |  343 | ` * Return` |
|       - |  344 | ` *   TRUE if trait is a defined trait, FALSE otherwise.` |
|       - |  345 | ` */` |
|      14 |  346 | `PH7_PRIVATE int vm_builtin_trait_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  347 | `{` |
|      16 |  348 | `	int res = 0; /* Assume trait does not exist */` |
|      16 |  349 | `	if( nArg > 0 ){` |
|      16 |  350 | `		SyHashEntry *pEntry = 0;` |
|       - |  351 | `		const char *zName;` |
|       - |  352 | `		int nLen;` |
|      16 |  353 | `		int iAutoload = 1; /* Default: autoload enabled */` |
|       - |  354 | `		sxu32 nName;` |
|       - |  355 | `		/* Extract given name */` |
|      16 |  356 | `		zName = ph7_value_to_string(apArg[0],&nLen);` |
|      16 |  357 | `		if( nArg >= 2 ){` |
|       3 |  358 | `			iAutoload = ph7_value_to_bool(apArg[1]);` |
|       1 |  359 | `		}` |
|       - |  360 | `		/* Strip a leading '\' (global-namespace anchor); this builtin bypasses` |
|       - |  361 | `		 * PH7_VmExtractClass and hashes hClass directly. */` |
|      16 |  362 | `		nName = (sxu32)nLen;` |
|      16 |  363 | `		PH7_VmClassNameAnchor(&zName,&nName);` |
|       - |  364 | `		/* Perform a hash lookup */` |
|      16 |  365 | `		if( nName > 0 ){` |
|      16 |  366 | `			pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|       7 |  367 | `		}` |
|       - |  368 | `		/* Gate autoload on the ORIGINAL length (nLen): php autoloads a lone` |
|       - |  369 | `		 * "\" with the empty stripped name, but not a truly empty "". */` |
|      16 |  370 | `		if( pEntry == 0 && nLen > 0 && iAutoload ){` |
|       - |  371 | `			/* Try autoload — pass iLoadable=FALSE so we get traits too */` |
|       3 |  372 | `			ph7_class *pClass = PH7_VmTriggerAutoload(pCtx->pVm,zName,nName,FALSE);` |
|       3 |  373 | `			if( pClass ){` |
|     ! 0 |  374 | `				pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|     ! 0 |  375 | `			}` |
|       1 |  376 | `		}` |
|      16 |  377 | `		if( pEntry ){` |
|      12 |  378 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|      18 |  379 | `			while( pClass ){` |
|      12 |  380 | `				if( pClass->iFlags & PH7_CLASS_TRAIT ){` |
|       - |  381 | `					/* trait is available */` |
|       6 |  382 | `					res = 1;` |
|       6 |  383 | `					break;` |
|       - |  384 | `				}` |
|       - |  385 | `				/* Next with the same name */` |
|       7 |  386 | `				pClass = pClass->pNextName;` |
|       1 |  387 | `			}` |
|       5 |  388 | `		}` |
|       7 |  389 | `	}` |
|      16 |  390 | `	ph7_result_bool(pCtx,res);` |
|      16 |  391 | `	return PH7_OK;` |
|       2 |  392 | `}` |
|       - |  393 | `/*` |
|       - |  394 | ` * bool class_alias([string $original[,string $alias ]])` |
|       - |  395 | ` *   Creates an alias for a class.` |
|       - |  396 | ` * Parameters` |
|       - |  397 | ` *  original` |
|       - |  398 | ` *    The original class.` |
|       - |  399 | ` *  alias` |
|       - |  400 | ` *   The alias name for the class.` |
|       - |  401 | ` * Return` |
|       - |  402 | ` *   Returns TRUE on success or FALSE on failure.` |
|       - |  403 | ` */` |
|       4 |  404 | `PH7_PRIVATE int vm_builtin_class_alias(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  405 | `{` |
|       - |  406 | `	const char *zOld,*zNew;` |
|       - |  407 | `	int nOldLen,nNewLen;` |
|       - |  408 | `	sxu32 nOld,nNew;` |
|       - |  409 | `	SyHashEntry *pEntry;` |
|       - |  410 | `	ph7_class *pClass;` |
|       - |  411 | `	char *zDup;` |
|       - |  412 | `	sxi32 rc;` |
|       6 |  413 | `	if( nArg < 2 ){` |
|       - |  414 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  415 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  416 | `		return PH7_OK;` |
|       - |  417 | `	}` |
|       - |  418 | `	/* Extract old class name */` |
|       6 |  419 | `	zOld = ph7_value_to_string(apArg[0],&nOldLen);` |
|       - |  420 | `	/* Extract alias name */` |
|       6 |  421 | `	zNew = ph7_value_to_string(apArg[1],&nNewLen);` |
|       - |  422 | `	/* Strip a leading '\' (global-namespace anchor) from BOTH names: php` |
|       - |  423 | `	 * resolves the target and stores the alias without it, so class_exists()` |
|       - |  424 | `	 * on the plain name then matches. */` |
|       6 |  425 | `	nOld = (sxu32)nOldLen;` |
|       6 |  426 | `	nNew = (sxu32)nNewLen;` |
|       6 |  427 | `	PH7_VmClassNameAnchor(&zOld,&nOld);` |
|       6 |  428 | `	PH7_VmClassNameAnchor(&zNew,&nNew);` |
|       6 |  429 | `	if( nNew < 1 ){` |
|       - |  430 | `		/* Invalid alias name,return FALSE */` |
|     ! 0 |  431 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  432 | `		return PH7_OK;` |
|       - |  433 | `	}` |
|       - |  434 | `	/* Perform a hash lookup */` |
|       6 |  435 | `	pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zOld,nOld);` |
|       6 |  436 | `	if( pEntry ==  0 ){` |
|       - |  437 | `		/* No such class,return FALSE */` |
|     ! 0 |  438 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  439 | `		return PH7_OK;` |
|       - |  440 | `	}` |
|       - |  441 | `	/* Point to the class */` |
|       6 |  442 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|       - |  443 | `	/* Duplicate alias name */` |
|       6 |  444 | `	zDup = SyMemBackendStrDup(&pCtx->pVm->sAllocator,zNew,nNew);` |
|       6 |  445 | `	if( zDup == 0 ){` |
|       - |  446 | `		/* Out of memory,return FALSE */` |
|     ! 0 |  447 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  448 | `		return PH7_OK;` |
|       - |  449 | `	}` |
|       - |  450 | `	/* Create the alias */` |
|       6 |  451 | `	rc = SyHashInsert(&pCtx->pVm->hClass,(const void *)zDup,nNew,pClass);` |
|       6 |  452 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  453 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,zDup);` |
|     ! 0 |  454 | `	}` |
|       6 |  455 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|       6 |  456 | `	return PH7_OK;` |
|       4 |  457 | `}` |
|       - |  458 | `/*` |
|       - |  459 | ` * array get_declared_classes(void)` |
|       - |  460 | ` *   Returns an array with the name of the defined classes` |
|       - |  461 | ` * Parameters` |
|       - |  462 | ` *  None` |
|       - |  463 | ` * Return` |
|       - |  464 | ` *   Returns an array of the names of the declared classes` |
|       - |  465 | ` *   in the current script.` |
|       - |  466 | ` * Note:` |
|       - |  467 | ` *   NULL is returned on failure.` |
|       - |  468 | ` */` |
|       2 |  469 | `PH7_PRIVATE int vm_builtin_get_declared_classes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  470 | `{` |
|       - |  471 | `	ph7_value *pName,*pArray;` |
|       - |  472 | `	SyHashEntry *pEntry;` |
|       - |  473 | `	/* Create a new array first */` |
|       3 |  474 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 |  475 | `	pName = ph7_context_new_scalar(pCtx);` |
|       3 |  476 | `	if( pArray == 0 \|\| pName == 0){` |
|     ! 0 |  477 | `		SXUNUSED(nArg); /* cc warning */` |
|     ! 0 |  478 | `		SXUNUSED(apArg);` |
|       - |  479 | `		/* Out of memory,return NULL */` |
|     ! 0 |  480 | `		ph7_result_null(pCtx);` |
|     ! 0 |  481 | `		return PH7_OK;` |
|       - |  482 | `	}` |
|       - |  483 | `	/* Fill the array with the defined classes */` |
|       3 |  484 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|     386 |  485 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|     383 |  486 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|       - |  487 | `		/* Do not register classes defined as interfaces */` |
|     383 |  488 | `		if( (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     345 |  489 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|       - |  490 | `			/* insert class name */` |
|     345 |  491 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|       - |  492 | `			/* Reset the cursor */` |
|     345 |  493 | `			ph7_value_reset_string_cursor(pName);` |
|     172 |  494 | `		}` |
|       1 |  495 | `	}` |
|       - |  496 | `	/* Return the created array */` |
|       3 |  497 | `	ph7_result_value(pCtx,pArray);` |
|       3 |  498 | `	return PH7_OK;` |
|       2 |  499 | `}` |
|       - |  500 | `/*` |
|       - |  501 | ` * array get_declared_interfaces(void)` |
|       - |  502 | ` *   Returns an array with the name of the defined interfaces` |
|       - |  503 | ` * Parameters` |
|       - |  504 | ` *  None` |
|       - |  505 | ` * Return` |
|       - |  506 | ` *   Returns an array of the names of the declared interfaces` |
|       - |  507 | ` *   in the current script.` |
|       - |  508 | ` * Note:` |
|       - |  509 | ` *   NULL is returned on failure.` |
|       - |  510 | ` */` |
|       2 |  511 | `PH7_PRIVATE int vm_builtin_get_declared_interfaces(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  512 | `{` |
|       - |  513 | `	ph7_value *pName,*pArray;` |
|       - |  514 | `	SyHashEntry *pEntry;` |
|       - |  515 | `	/* Create a new array first */` |
|       3 |  516 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 |  517 | `	pName = ph7_context_new_scalar(pCtx);` |
|       3 |  518 | `	if( pArray == 0 \|\| pName == 0 ){` |
|     ! 0 |  519 | `		SXUNUSED(nArg); /* cc warning */` |
|     ! 0 |  520 | `		SXUNUSED(apArg);` |
|       - |  521 | `		/* Out of memory,return NULL */` |
|     ! 0 |  522 | `		ph7_result_null(pCtx);` |
|     ! 0 |  523 | `		return PH7_OK;` |
|       - |  524 | `	}` |
|       - |  525 | `	/* Fill the array with the defined classes */` |
|       3 |  526 | `	SyHashResetLoopCursor(&pCtx->pVm->hClass);` |
|     388 |  527 | `	while((pEntry = SyHashGetNextEntry(&pCtx->pVm->hClass)) != 0 ){` |
|     385 |  528 | `		ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|       - |  529 | `		/* Register classes defined as interfaces only */` |
|     385 |  530 | `		if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|      41 |  531 | `			ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|       - |  532 | `			/* insert interface name */` |
|      41 |  533 | `			ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|       - |  534 | `			/* Reset the cursor */` |
|      41 |  535 | `			ph7_value_reset_string_cursor(pName);` |
|      20 |  536 | `		}` |
|       1 |  537 | `	}` |
|       - |  538 | `	/* Return the created array */` |
|       3 |  539 | `	ph7_result_value(pCtx,pArray);` |
|       3 |  540 | `	return PH7_OK;` |
|       2 |  541 | `}` |
|       - |  542 | `/*` |
|       - |  543 | ` * array get_class_methods(string/object $class_name)` |
|       - |  544 | ` *   Returns an array with the name of the class methods` |
|       - |  545 | ` * Parameters` |
|       - |  546 | ` *  class_name` |
|       - |  547 | ` *  The class name or class instance` |
|       - |  548 | ` * Return` |
|       - |  549 | ` *  Returns an array of method names defined for the class specified by class_name.` |
|       - |  550 | ` *  In case of an error, it returns NULL.` |
|       - |  551 | ` * Note:` |
|       - |  552 | ` *   NULL is returned on failure.` |
|       - |  553 | ` */` |
|      10 |  554 | `PH7_PRIVATE int vm_builtin_get_class_methods(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  555 | `{` |
|       - |  556 | `	ph7_value *pName,*pArray;` |
|       - |  557 | `	SyHashEntry *pEntry;` |
|       - |  558 | `	ph7_class *pClass;` |
|       - |  559 | `	/* Extract the target class first */` |
|      12 |  560 | `	pClass = 0;` |
|      12 |  561 | `	if( nArg > 0 ){` |
|      12 |  562 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|       5 |  563 | `	}` |
|      12 |  564 | `	if( pClass == 0 ){` |
|       - |  565 | `		/* No such class,return NULL */` |
|       3 |  566 | `		ph7_result_null(pCtx);` |
|       3 |  567 | `		return PH7_OK;` |
|       - |  568 | `	}` |
|       - |  569 | `	/* Create a new array  */` |
|      10 |  570 | `	pArray = ph7_context_new_array(pCtx);` |
|      10 |  571 | `	pName = ph7_context_new_scalar(pCtx);` |
|      10 |  572 | `	if( pArray == 0 \|\| pName == 0){` |
|       - |  573 | `		/* Out of memory,return NULL */` |
|     ! 0 |  574 | `		ph7_result_null(pCtx);` |
|     ! 0 |  575 | `		return PH7_OK;` |
|       - |  576 | `	}` |
|       - |  577 | `	/* Fill the array with the defined methods, in php's order: the class's own` |
|       - |  578 | `	 * methods in DECLARATION order, then each ancestor's in ITS declaration` |
|       - |  579 | `	 * order (band A #4 — the raw hash walk returned reverse-insertion/LIFO` |
|       - |  580 | `	 * order). SyHash iterates newest-first, so a reversed walk restores` |
|       - |  581 | `	 * insertion order; grouping by declaring class (sFunc.pUserData, the class` |
|       - |  582 | `	 * a method was compiled into) walks own-then-parent like php. An override` |
|       - |  583 | `	 * lives once in the hash under the subclass, so no dedup is needed. */` |
|       - |  584 | `	{` |
|       - |  585 | `		SySet aTmp;` |
|       - |  586 | `		SyHashEntry **apEntry;` |
|       - |  587 | `		ph7_class *pLevel;` |
|       - |  588 | `		sxu32 n;` |
|       - |  589 | `		/* Names are emitted from each entry's HASH KEY, not sFunc.sName: a trait` |
|       - |  590 | ``		 * adaptation alias (`hi as bHi`) keeps the original name in its method`` |
|       - |  591 | `		 * struct while the key carries the alias — php lists the alias. */` |
|      10 |  592 | `		SySetInit(&aTmp,&pCtx->pVm->sAllocator,sizeof(SyHashEntry *));` |
|      10 |  593 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      36 |  594 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      28 |  595 | `			SySetPut(&aTmp,(const void *)&pEntry);` |
|       2 |  596 | `		}` |
|      10 |  597 | `		apEntry = (SyHashEntry **)SySetBasePtr(&aTmp);` |
|      20 |  598 | `		for( pLevel = pClass; pLevel; pLevel = pLevel->pBase ){` |
|       - |  599 | `			/* Collect this level's methods, then emit in DECLARATION order` |
|       - |  600 | `			 * (sorted by nLine — same-level methods share a source file; a` |
|       - |  601 | `			 * hash-order fallback covers line-less internal methods). */` |
|       - |  602 | `			SySet aLvl;` |
|       - |  603 | `			SyHashEntry **apLvl;` |
|       - |  604 | `			sxu32 i,j;` |
|      12 |  605 | `			SySetInit(&aLvl,&pCtx->pVm->sAllocator,sizeof(SyHashEntry *));` |
|       - |  606 | `			/* Hash-order fallback for same-line methods: the class's OWN entries` |
|       - |  607 | `			 * come out in declaration order when walked newest-first, while` |
|       - |  608 | `			 * inherited copies (inserted by PH7_ClassInherit's walk of the base` |
|       - |  609 | `			 * hash) come out in declaration order walked oldest-first. */` |
|      46 |  610 | `			for( n = 0; n < SySetUsed(&aTmp); n++ ){` |
|      36 |  611 | `				sxu32 nPick = (pLevel == pClass) ? (SySetUsed(&aTmp) - 1 - n) : n;` |
|      36 |  612 | `				ph7_class_method *pMethod = (ph7_class_method *)apEntry[nPick]->pUserData;` |
|      36 |  613 | `				ph7_class *pDecl = (ph7_class *)pMethod->sFunc.pUserData;` |
|      36 |  614 | `				if( pDecl != pLevel ){` |
|       - |  615 | `					/* A declarer outside the base chain (a used trait, or none)` |
|       - |  616 | `					 * counts as the class's own level, like php. */` |
|       - |  617 | `					ph7_class *pWalk;` |
|      16 |  618 | `					if( pLevel != pClass \|\| pDecl == pClass ){` |
|       5 |  619 | `						continue;` |
|       - |  620 | `					}` |
|      22 |  621 | `					for( pWalk = pClass; pWalk; pWalk = pWalk->pBase ){` |
|      16 |  622 | `						if( pWalk == pDecl ){` |
|       5 |  623 | `							break;` |
|       - |  624 | `						}` |
|       7 |  625 | `					}` |
|      12 |  626 | `					if( pWalk != 0 ){` |
|       5 |  627 | `						continue; /* in-chain: its own level emits it */` |
|       - |  628 | `					}` |
|       3 |  629 | `				}` |
|      28 |  630 | `				SySetPut(&aLvl,(const void *)&apEntry[nPick]);` |
|      15 |  631 | `			}` |
|      12 |  632 | `			apLvl = (SyHashEntry **)SySetBasePtr(&aLvl);` |
|       - |  633 | `			/* Insertion sort by declaration line (stable) */` |
|      28 |  634 | `			for( i = 1; i < SySetUsed(&aLvl); i++ ){` |
|      18 |  635 | `				SyHashEntry *pKey = apLvl[i];` |
|      18 |  636 | `				for( j = i; j > 0 && ((ph7_class_method *)apLvl[j-1]->pUserData)->nLine` |
|      16 |  637 | `						> ((ph7_class_method *)pKey->pUserData)->nLine; j-- ){` |
|     ! 0 |  638 | `					apLvl[j] = apLvl[j-1];` |
|     ! 0 |  639 | `				}` |
|      18 |  640 | `				apLvl[j] = pKey;` |
|      10 |  641 | `			}` |
|      38 |  642 | `			for( i = 0; i < SySetUsed(&aLvl); i++ ){` |
|       - |  643 | `				/* Insert method name (the hash key: alias-aware) */` |
|      28 |  644 | `				ph7_value_string(pName,(const char *)apLvl[i]->pKey,(int)apLvl[i]->nKeyLen);` |
|      28 |  645 | `				ph7_array_add_elem(pArray,0/*Automatic index assign*/,pName); /* Will make it's own copy */` |
|       - |  646 | `				/* Reset the cursor */` |
|      28 |  647 | `				ph7_value_reset_string_cursor(pName);` |
|      15 |  648 | `			}` |
|      12 |  649 | `			SySetRelease(&aLvl);` |
|       7 |  650 | `		}` |
|      10 |  651 | `		SySetRelease(&aTmp);` |
|       - |  652 | `	}` |
|       - |  653 | `	/* Return the created array */` |
|      10 |  654 | `	ph7_result_value(pCtx,pArray);` |
|       - |  655 | `	/*` |
|       - |  656 | `	 * Don't worry about freeing memory here,everything will be relased` |
|       - |  657 | `	 * automatically as soon we return from this foreign function.` |
|       - |  658 | `	 */` |
|      10 |  659 | `	return PH7_OK;` |
|       7 |  660 | `}` |
|       - |  661 | `/*` |
|       - |  662 | ` * This function return TRUE(1) if the given class attribute stored` |
|       - |  663 | ` * in the pAttrName parameter is visible and thus can be extracted` |
|       - |  664 | ` * from the current scope.Otherwise FALSE is returned.` |
|       - |  665 | ` */` |
| 3128354 |  666 | `PH7_PRIVATE int PH7_VmClassMemberAccess(` |
|       - |  667 | `	ph7_vm *pVm,               /* Target VM */` |
|       - |  668 | `	ph7_class *pClass,         /* Target Class */` |
|       - |  669 | `	const SyString *pAttrName, /* Attribute name */` |
|       - |  670 | `	sxi32 iProtection,         /* Attribute protection level [i.e: public,protected or private] */` |
|       - |  671 | `	int bLog                   /* TRUE to log forbidden access. */` |
|       - |  672 | `	)` |
|       5 |  673 | `{` |
| 3128359 |  674 | `	if( iProtection != PH7_CLASS_PROT_PUBLIC ){` |
| 2916939 |  675 | `		VmFrame *pFrame = pVm->pFrame;` |
|       - |  676 | `		ph7_vm_func *pVmFunc;` |
|       - |  677 | `		ph7_class *pCallerScope;` |
| 2916961 |  678 | `		while( pFrame->pParent && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH) ) ){` |
|       - |  679 | `			/* Safely ignore the exception frame */` |
|      25 |  680 | `			pFrame = pFrame->pParent;` |
|       3 |  681 | `		}` |
| 2916939 |  682 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       - |  683 | `		/* The calling scope is the executing method's declaring class — OR, for a bound closure` |
|       - |  684 | `		 * (Closure::bindTo/call), the explicit scope override carried on the frame (Increment 2). */` |
| 2916939 |  685 | `		if( pFrame->pBoundScope ){` |
|      15 |  686 | `			pCallerScope = pFrame->pBoundScope;` |
| 2916932 |  687 | `		}else if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
| 2916813 |  688 | `			pCallerScope = (ph7_class *)pVmFunc->pUserData;` |
| 1458520 |  689 | `		}else if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLOSURE) && pVmFunc->pUserData ){` |
|       - |  690 | `			/* A closure/arrow-fn defined inside a class carries its creation-site` |
|       - |  691 | `			 * class in pUserData (stamped by OP_LOAD_CLOSURE via` |
|       - |  692 | ``			 * PH7_VmPeekDeclaringClass, the same scope `self::`/`parent::` resolve`` |
|       - |  693 | `			 * against inside the body). php binds that class as the closure's scope,` |
|       - |  694 | ``			 * so `$this->privateMethod()` / `self::$private` inside the closure are`` |
|       - |  695 | `			 * allowed — an explicit Closure::bindTo/bind rebind still wins above via` |
|       - |  696 | `			 * pBoundScope. */` |
|      39 |  697 | `			pCallerScope = (ph7_class *)pVmFunc->pUserData;` |
|      97 |  698 | `		}else if( pVm->pConstEvalClass ){` |
|       - |  699 | `			/* Constant/property initializer bytecode runs without a method` |
|       - |  700 | `			 * frame; its scope is the class being initialized (php: a private` |
|       - |  701 | `			 * constant is reachable from its own class's initializers). */` |
|       3 |  702 | `			pCallerScope = pVm->pConstEvalClass;` |
|       2 |  703 | `		}else{` |
|      76 |  704 | `			goto dis; /* Not in a class scope: access is forbidden */` |
|       - |  705 | `		}` |
| 2916867 |  706 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       - |  707 | `			/* php grants private access by DECLARING class: the caller's own` |
|       - |  708 | `			 * class must declare a private attribute of this name (a base` |
|       - |  709 | `			 * method touching its own private on a CHILD instance passes; a` |
|       - |  710 | `			 * child method touching an inherited base-private fails). An attr` |
|       - |  711 | `			 * whose declaring "class" is a TRAIT behaves as if declared by the` |
|       - |  712 | `			 * adopting class. Fallbacks: the caller being a trait used by the` |
|       - |  713 | `			 * instance's class (legacy trait-body scope), or — when the caller` |
|       - |  714 | `			 * class carries no such attr entry at all — the legacy exact-class` |
|       - |  715 | `			 * match (dynamic props and other non-declared shapes). */` |
|   12509 |  716 | `			ph7_class *pCaller = pCallerScope;` |
|   18761 |  717 | `			SyHashEntry *pOwnE = SyHashGet(&pCaller->hAttr,` |
|   12504 |  718 | `				(const void *)pAttrName->zString,pAttrName->nByte);` |
|   12509 |  719 | `			ph7_class_attr *pOwn = pOwnE ? (ph7_class_attr *)pOwnE->pUserData : 0;` |
|   12509 |  720 | `			int bGranted = 0;` |
|   12509 |  721 | `			if( pOwn && pOwn->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|   10646 |  722 | `				if( pOwn->pDeclClass == 0` |
|   10646 |  723 | `				 \|\| pOwn->pDeclClass == pCaller` |
|    6278 |  724 | `				 \|\| (pOwn->pDeclClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|   10629 |  725 | `					bGranted = 1;` |
|    5317 |  726 | `				}` |
|    7186 |  727 | `			}else if( pOwn == 0 && pCaller == pClass ){` |
|     977 |  728 | `				bGranted = 1;` |
|     487 |  729 | `			}` |
|   12509 |  730 | `			if( !bGranted ){` |
|       - |  731 | `				/* Check if the caller is a trait used by pClass */` |
|       - |  732 | `				ph7_class **apTrait;` |
|       - |  733 | `				sxu32 nTrait,k;` |
|     910 |  734 | `				apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|     910 |  735 | `				nTrait = SySetUsed(&pClass->aTrait);` |
|     910 |  736 | `				for(k = 0; k < nTrait; k++){` |
|     ! 0 |  737 | `					if( apTrait[k] == pCaller ){` |
|     ! 0 |  738 | `						bGranted = 1;` |
|     ! 0 |  739 | `						break;` |
|       - |  740 | `					}` |
|     ! 0 |  741 | `				}` |
|     453 |  742 | `			}` |
|   12509 |  743 | `			if( !bGranted && (pClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|       - |  744 | `				/* The target "class" is itself a trait: a trait-copied private` |
|       - |  745 | `				 * member behaves as if declared in the adopting class, so a` |
|       - |  746 | `` 				 * caller that USES the trait gets access (php: `self::s()` `` |
|       - |  747 | `				 * from a using class's static method reaching a trait-private` |
|       - |  748 | `				 * static — the callee resolves via the shared trait VmFunc` |
|       - |  749 | `				 * whose owner is the trait, not the class). */` |
|       - |  750 | `				ph7_class **apTrait;` |
|       - |  751 | `				sxu32 nTrait,k;` |
|     883 |  752 | `				apTrait = (ph7_class **)SySetBasePtr(&pCaller->aTrait);` |
|     883 |  753 | `				nTrait = SySetUsed(&pCaller->aTrait);` |
|     883 |  754 | `				for(k = 0; k < nTrait; k++){` |
|     883 |  755 | `					if( apTrait[k] == pClass ){` |
|     883 |  756 | `						bGranted = 1;` |
|     883 |  757 | `						break;` |
|       - |  758 | `					}` |
|     ! 0 |  759 | `				}` |
|     440 |  760 | `			}` |
|   12509 |  761 | `			if( !bGranted ){` |
|      29 |  762 | `				goto dis; /* Access is forbidden */` |
|       - |  763 | `			}` |
|    6244 |  764 | `		}else{` |
|       - |  765 | `			/* Protected */` |
| 2904363 |  766 | `			ph7_class *pBase = pCallerScope;` |
|       - |  767 | `			/* php checks the hierarchy against the class that INTRODUCES the member,` |
|       - |  768 | `			 * not the one that (re)declares the override we resolved. A protected` |
|       - |  769 | `			 * member declared in a common ancestor B and overridden in a child C is` |
|       - |  770 | `			 * still reachable from a SIBLING scope S (also extending B) — S and C both` |
|       - |  771 | `			 * descend from B. Walk pClass up to the top-most ancestor that genuinely` |
|       - |  772 | `			 * declares a member of this name (a method via sFunc.pUserData, or an attr` |
|       - |  773 | `			 * via pDeclClass — hMethod/hAttr also carry inherited copies, so match on the` |
|       - |  774 | `			 * true declaring class) and test the hierarchy against that introducing` |
|       - |  775 | ``			 * class. `child_only` (declared solely in C) keeps pClass and stays denied`` |
|       - |  776 | `			 * from a sibling, matching php. */` |
| 2904363 |  777 | `			ph7_class *pIntro = pClass;` |
|       - |  778 | `			ph7_class *pAnc;` |
| 6028837 |  779 | `			for( pAnc = pClass ; pAnc ; pAnc = pAnc->pBase ){` |
| 3124479 |  780 | `				ph7_class_method *pAncMeth = PH7_ClassExtractMethod(pAnc,pAttrName->zString,pAttrName->nByte);` |
| 3124479 |  781 | `				SyHashEntry *pAncAttrE = SyHashGet(&pAnc->hAttr,(const void *)pAttrName->zString,pAttrName->nByte);` |
| 3124479 |  782 | `				ph7_class_attr *pAncAttr = pAncAttrE ? (ph7_class_attr *)pAncAttrE->pUserData : 0;` |
| 3124479 |  783 | `				int bHere = 0;` |
| 3124479 |  784 | `				if( pAncMeth && (ph7_class *)pAncMeth->sFunc.pUserData == pAnc ){` |
|    3902 |  785 | `					bHere = 1;` |
|    1949 |  786 | `				}` |
| 3124479 |  787 | `				if( pAncAttr && (pAncAttr->pDeclClass == pAnc \|\| pAncAttr->pDeclClass == 0) ){` |
| 2901007 |  788 | `					bHere = 1;` |
| 1450501 |  789 | `				}` |
| 3124479 |  790 | `				if( bHere ){` |
| 2904905 |  791 | `					pIntro = pAnc; /* keep climbing: the LAST (highest) match wins */` |
| 1452450 |  792 | `				}` |
| 1562242 |  793 | `			}` |
|       - |  794 | `			/* Must be in the same class hierarchy as the introducing class */` |
| 2904363 |  795 | `			if( !PH7_VmInstanceOf(pIntro,pBase) && !PH7_VmInstanceOf(pBase,pIntro) ){` |
|      15 |  796 | `				int bTraitGrant = 0;` |
|      15 |  797 | `				if( (pClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|       - |  798 | `					/* Same trait-target rule as the private branch above */` |
|       - |  799 | `					ph7_class **apTrait;` |
|       - |  800 | `					sxu32 nTrait,k;` |
|      11 |  801 | `					apTrait = (ph7_class **)SySetBasePtr(&pBase->aTrait);` |
|      11 |  802 | `					nTrait = SySetUsed(&pBase->aTrait);` |
|      11 |  803 | `					for(k = 0; k < nTrait; k++){` |
|       9 |  804 | `						if( apTrait[k] == pClass ){` |
|       9 |  805 | `							bTraitGrant = 1;` |
|       9 |  806 | `							break;` |
|       - |  807 | `						}` |
|     ! 0 |  808 | `					}` |
|       4 |  809 | `				}` |
|      15 |  810 | `				if( !bTraitGrant ){` |
|       8 |  811 | `					goto dis; /* Access is forbidden */` |
|       - |  812 | `				}` |
|       3 |  813 | `			}` |
|       - |  814 | `		}` |
| 1458415 |  815 | `	}` |
| 3128255 |  816 | `	return 1; /* Access is granted */` |
|      52 |  817 | `dis:` |
|     108 |  818 | `	if( bLog ){` |
|     ! 0 |  819 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - |  820 | `			"Access to the class attribute '%z->%z' is forbidden",` |
|     ! 0 |  821 | `			&pClass->sName,pAttrName);` |
|     ! 0 |  822 | `	}` |
|     108 |  823 | `	return 0; /* Access is forbidden */` |
| 1564182 |  824 | `}` |
|       - |  825 | `/*` |
|       - |  826 | ` * array get_class_vars(string/object $class_name)` |
|       - |  827 | ` *   Get the default properties of the class` |
|       - |  828 | ` * Parameters` |
|       - |  829 | ` *  class_name` |
|       - |  830 | ` *   The class name or class instance` |
|       - |  831 | ` * Return` |
|       - |  832 | ` *  Returns an associative array of declared properties visible from the current scope` |
|       - |  833 | ` *  with their default value. The resulting array elements are in the form` |
|       - |  834 | ` *  of varname => value.` |
|       - |  835 | ` * Note:` |
|       - |  836 | ` *   NULL is returned on failure.` |
|       - |  837 | ` */` |
|       6 |  838 | `PH7_PRIVATE int vm_builtin_get_class_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  839 | `{` |
|       - |  840 | `	ph7_value *pName,*pArray,sValue;` |
|       - |  841 | `	SyHashEntry *pEntry;` |
|       - |  842 | `	ph7_class *pClass;` |
|       - |  843 | `	/* Extract the target class first */` |
|       8 |  844 | `	pClass = 0;` |
|       8 |  845 | `	if( nArg > 0 ){` |
|       8 |  846 | `		pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|       3 |  847 | `	}` |
|       8 |  848 | `	if( pClass == 0 ){` |
|       - |  849 | `		/* php screens the VALUE, not the type: anything stringifiable is accepted,` |
|       - |  850 | `		 * and a name that does not resolve to a class is a TypeError quoting the` |
|       - |  851 | `		 * stringified argument ("...must be a valid class name, Array given"). This` |
|       - |  852 | `		 * is why get_class_vars() opts out of the shared ZPP type screen in vm.c. */` |
|     ! 0 |  853 | `		int nLen = 0;` |
|     ! 0 |  854 | `		const char *zVal = "";` |
|     ! 0 |  855 | `		if( nArg > 0 ){` |
|     ! 0 |  856 | `			if( (apArg[0]->iFlags & MEMOBJ_HASHMAP) != 0 ){` |
|     ! 0 |  857 | `				zVal = "Array";` |
|     ! 0 |  858 | `				nLen = (int)sizeof("Array") - 1;` |
|     ! 0 |  859 | `			}else{` |
|     ! 0 |  860 | `				zVal = ph7_value_to_string(apArg[0],&nLen);` |
|       - |  861 | `			}` |
|     ! 0 |  862 | `		}` |
|     ! 0 |  863 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  864 | `			"get_class_vars(): Argument #1 ($class) must be a valid class name, %.*s given",` |
|     ! 0 |  865 | `			nLen,zVal);` |
|       - |  866 | `	}` |
|       - |  867 | `	/* Create a new array  */` |
|       8 |  868 | `	pArray = ph7_context_new_array(pCtx);` |
|       8 |  869 | `	pName = ph7_context_new_scalar(pCtx);` |
|       8 |  870 | `	PH7_MemObjInit(pCtx->pVm,&sValue);` |
|       8 |  871 | `	if( pArray == 0 \|\| pName == 0){` |
|       - |  872 | `		/* Out of memory,return NULL */` |
|     ! 0 |  873 | `		ph7_result_null(pCtx);` |
|     ! 0 |  874 | `		return PH7_OK;` |
|       - |  875 | `	}` |
|       - |  876 | `	/* Fill the array with the defined attribute visible from the current scope */` |
|       8 |  877 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|      16 |  878 | `	while((pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|      10 |  879 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      10 |  880 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|       - |  881 | `			/* php 8.4: VIRTUAL hooked properties have no backing store —` |
|       - |  882 | `			 * get_class_vars() excludes them (raw surface) */` |
|       3 |  883 | `			continue;` |
|       - |  884 | `		}` |
|       - |  885 | `		/* Check if the access is allowed */` |
|       8 |  886 | `		if( PH7_VmClassMemberAccess(pCtx->pVm,pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|       8 |  887 | `			SyString *pAttrName = &pAttr->sName;` |
|       8 |  888 | `			ph7_value *pValue = 0;` |
|       8 |  889 | `			if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|       - |  890 | `				/* Static slots are computed at mount; constants lazily */` |
|       3 |  891 | `				PH7_VmMaterializeClassConst(pCtx->pVm,pClass,pAttr);` |
|       3 |  892 | `				pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pAttr->nIdx);` |
|       2 |  893 | `			}else{` |
|       6 |  894 | `				if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|       6 |  895 | `					PH7_MemObjRelease(&sValue);` |
|       - |  896 | `					/* Compute default value (any complex expression) associated with this attribute */` |
|       6 |  897 | `					VmLocalExec(pCtx->pVm,&pAttr->aByteCode,&sValue,FALSE);` |
|       6 |  898 | `					pValue = &sValue;` |
|       2 |  899 | `				}` |
|       - |  900 | `			}` |
|       - |  901 | `			/* Fill in the array */` |
|       8 |  902 | `			ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|       8 |  903 | `			ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|       - |  904 | `			/* Reset the cursor */` |
|       8 |  905 | `			ph7_value_reset_string_cursor(pName);` |
|       3 |  906 | `		}` |
|       2 |  907 | `	}` |
|       8 |  908 | `	PH7_MemObjRelease(&sValue);` |
|       - |  909 | `	/* Return the created array */` |
|       8 |  910 | `	ph7_result_value(pCtx,pArray);` |
|       - |  911 | `	/*` |
|       - |  912 | `	 * Don't worry about freeing memory here,everything will be relased` |
|       - |  913 | `	 * automatically as soon we return from this foreign function.` |
|       - |  914 | `	 */` |
|       8 |  915 | `	return PH7_OK;` |
|       5 |  916 | `}` |
|       - |  917 | `/*` |
|       - |  918 | ` * array get_object_vars(object $this)` |
|       - |  919 | ` *   Gets the properties of the given object` |
|       - |  920 | ` * Parameters` |
|       - |  921 | ` *  this` |
|       - |  922 | ` *   A class instance` |
|       - |  923 | ` * Return` |
|       - |  924 | ` *  Returns an associative array of defined object accessible non-static properties` |
|       - |  925 | ` *  for the specified object in scope. If a property have not been assigned a value` |
|       - |  926 | ` *  it will be returned with a NULL value.` |
|       - |  927 | ` * Note:` |
|       - |  928 | ` *   NULL is returned on failure.` |
|       - |  929 | ` */` |
|      48 |  930 | `PH7_PRIVATE int vm_builtin_get_object_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  931 | `{` |
|      51 |  932 | `	ph7_class_instance *pThis = 0;` |
|       - |  933 | `	ph7_value *pName,*pArray;` |
|       - |  934 | `	SyHashEntry *pEntry;` |
|      51 |  935 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|       - |  936 | `		/* Extract the target instance */` |
|      51 |  937 | `		pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      24 |  938 | `	}` |
|      51 |  939 | `	if( pThis == 0 ){` |
|       - |  940 | `		/* No such instance,return NULL */` |
|     ! 0 |  941 | `		ph7_result_null(pCtx);` |
|     ! 0 |  942 | `		return PH7_OK;` |
|       - |  943 | `	}` |
|       - |  944 | `	/* Create a new array  */` |
|      51 |  945 | `	pArray = ph7_context_new_array(pCtx);` |
|      51 |  946 | `	pName = ph7_context_new_scalar(pCtx);` |
|      51 |  947 | `	if( pArray == 0 \|\| pName == 0){` |
|       - |  948 | `		/* Out of memory,return NULL */` |
|     ! 0 |  949 | `		ph7_result_null(pCtx);` |
|     ! 0 |  950 | `		return PH7_OK;` |
|       - |  951 | `	}` |
|       - |  952 | `	/* Fill the array with the defined attribute visible from the current scope.` |
|       - |  953 | `	 * SNAPSHOT the attribute names first: a PHP 8.4 get hook dispatched mid-walk` |
|       - |  954 | `	 * runs user code that may re-enter an hAttr walk on this instance (resetting` |
|       - |  955 | `	 * the hash's single embedded loop cursor) or unset()/create properties. The` |
|       - |  956 | `	 * names point into CLASS-owned attr storage (they outlive instance mutation);` |
|       - |  957 | `	 * each is re-looked-up before use so an entry unset by an earlier hook is` |
|       - |  958 | `	 * skipped instead of read after free. */` |
|       - |  959 | `	{` |
|       - |  960 | `		SySet sNames;` |
|       - |  961 | `		SyString *aName;` |
|       - |  962 | `		sxu32 iName,nName;` |
|      51 |  963 | `		SySetInit(&sNames,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|      51 |  964 | `		SyHashResetLoopCursor(&pThis->hAttr);` |
|     219 |  965 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|     171 |  966 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     171 |  967 | `			if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|       - |  968 | `				/* Only non-static/constant attributes are extracted */` |
|       3 |  969 | `				continue;` |
|       - |  970 | `			}` |
|     166 |  971 | `			if( (pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|      86 |  972 | `			 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|       3 |  973 | `				continue; /* virtual set-only property: no value to expose (php) */` |
|       - |  974 | `			}` |
|     167 |  975 | `			SySetPut(&sNames,(const void *)&pVmAttr->pAttr->sName);` |
|       3 |  976 | `		}` |
|      51 |  977 | `		aName = (SyString *)SySetBasePtr(&sNames);` |
|      51 |  978 | `		nName = SySetUsed(&sNames);` |
|     215 |  979 | `		for( iName = 0 ; iName < nName ; ++iName ){` |
|     167 |  980 | `			SyString *pAttrName = &aName[iName];` |
|       - |  981 | `			VmClassAttr *pVmAttr;` |
|     167 |  982 | `			pEntry = SyHashGet(&pThis->hAttr,(const void *)pAttrName->zString,pAttrName->nByte);` |
|     167 |  983 | `			if( pEntry == 0 ){` |
|     ! 0 |  984 | `				continue; /* unset by an earlier hook */` |
|       - |  985 | `			}` |
|     167 |  986 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|       - |  987 | `			/* Check if the access is allowed */` |
|     167 |  988 | `			if( PH7_VmClassMemberAccess(pCtx->pVm,pThis->pClass,pAttrName,pVmAttr->pAttr->iProtection,FALSE) ){` |
|     133 |  989 | `				ph7_value *pValue = 0;` |
|       - |  990 | `				ph7_value sHookVal;` |
|       - |  991 | `				sxi32 rcHk;` |
|       - |  992 | `				/* PHP 8.4 property hooks: get_object_vars() reads through the get` |
|       - |  993 | `				 * hook (virtual properties included); raw slot otherwise. */` |
|     133 |  994 | `				PH7_MemObjInit(pCtx->pVm,&sHookVal);` |
|     133 |  995 | `				rcHk = PH7_VmHookGetAttrValue(pThis,pVmAttr,&sHookVal);` |
|     133 |  996 | `				if( rcHk == SXRET_OK ){` |
|      15 |  997 | `					pValue = &sHookVal;` |
|     126 |  998 | `				}else if( rcHk == SXERR_NOTFOUND ){` |
|       - |  999 | `					/* Extract attribute */` |
|     119 | 1000 | `					pValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|      61 | 1001 | `				}else{` |
|       - | 1002 | `					/* the hook threw — parked on the boundary rail; php aborts the` |
|       - | 1003 | `					 * whole builtin at the first throw (the helper's boundary gate` |
|       - | 1004 | `					 * keeps LATER hooks from running; raw values it falls back to` |
|       - | 1005 | `					 * are discarded when the throw routes) */` |
|     ! 0 | 1006 | `					PH7_MemObjRelease(&sHookVal);` |
|     ! 0 | 1007 | `					break;` |
|       - | 1008 | `				}` |
|     133 | 1009 | `				if( pValue ){` |
|       - | 1010 | `					/* Insert attribute name in the array */` |
|     133 | 1011 | `					ph7_value_string(pName,pAttrName->zString,pAttrName->nByte);` |
|     133 | 1012 | `					ph7_array_add_elem(pArray,pName,pValue); /* Will make it's own copy */` |
|      65 | 1013 | `				}` |
|     133 | 1014 | `				PH7_MemObjRelease(&sHookVal);` |
|       - | 1015 | `				/* Reset the cursor */` |
|     133 | 1016 | `				ph7_value_reset_string_cursor(pName);` |
|      65 | 1017 | `			}` |
|      85 | 1018 | `		}` |
|      51 | 1019 | `		SySetRelease(&sNames);` |
|       - | 1020 | `	}` |
|       - | 1021 | `	/* Return the created array */` |
|      51 | 1022 | `	ph7_result_value(pCtx,pArray);` |
|       - | 1023 | `	/*` |
|       - | 1024 | `	 * Don't worry about freeing memory here,everything will be relased` |
|       - | 1025 | `	 * automatically as soon we return from this foreign function.` |
|       - | 1026 | `	 */` |
|      51 | 1027 | `	return PH7_OK;` |
|      27 | 1028 | `}` |
|       - | 1029 | ``/* Bound on `extends` chain depth — matches PH7_THROWABLE_WALK_MAX_DEPTH in`` |
|       - | 1030 | ` * compile.c. Defends against compiler cycles even though interface cycle` |
|       - | 1031 | ` * detection should reject them up front. */` |
|       - | 1032 | `#define PH7_INTERFACE_WALK_MAX_DEPTH 64` |
|       - | 1033 | `/*` |
|       - | 1034 | ` * TRUE if pTarget is reachable from pIface as an ancestor interface: walk the` |
|       - | 1035 | `` * `extends` chain (pBase) AND each additional parent-interface set (aInterface),`` |
|       - | 1036 | `` * so a multiple-interface `interface C extends A, B` is recognized through BOTH`` |
|       - | 1037 | ` * A and B (php allows an interface to extend several interfaces). Recursion is` |
|       - | 1038 | ` * depth-bounded — a malformed cycle cannot run unbounded.` |
|       - | 1039 | ` */` |
| 2564668 | 1040 | `static int VmInterfaceReaches(ph7_class *pIface,ph7_class *pTarget,int iDepth)` |
|       5 | 1041 | `{` |
| 2682211 | 1042 | `	while( pIface && iDepth <= PH7_INTERFACE_WALK_MAX_DEPTH ){` |
|       - | 1043 | `		ph7_class **apParent;` |
|       - | 1044 | `		sxu32 n;` |
| 2567451 | 1045 | `		if( pIface == pTarget ){` |
| 2449911 | 1046 | `			return TRUE;` |
|       - | 1047 | `		}` |
|       - | 1048 | `		/* Additional parent interfaces (interface X extends A, B, …) live in` |
|       - | 1049 | `		 * aInterface; the first parent stays on the pBase chain below. */` |
|  117545 | 1050 | `		apParent = (ph7_class **)SySetBasePtr(&pIface->aInterface);` |
|  117549 | 1051 | `		for( n = 0 ; n < SySetUsed(&pIface->aInterface) ; n++ ){` |
|       7 | 1052 | `			if( VmInterfaceReaches(apParent[n],pTarget,iDepth+1) ){` |
|       3 | 1053 | `				return TRUE;` |
|       - | 1054 | `			}` |
|       3 | 1055 | `		}` |
|  117543 | 1056 | `		pIface = pIface->pBase;` |
|  117543 | 1057 | `		iDepth++;` |
|       5 | 1058 | `	}` |
|  114765 | 1059 | `	return FALSE;` |
| 1282339 | 1060 | `}` |
|       - | 1061 | `/*` |
|       - | 1062 | ` * This function returns TRUE if the given class is an implemented` |
|       - | 1063 | ` * interface.Otherwise FALSE is returned.` |
|       - | 1064 | ` */` |
| 2668488 | 1065 | `static int VmQueryInterfaceSet(ph7_class *pClass,SySet *pSet)` |
|       5 | 1066 | `{` |
|       - | 1067 | `	ph7_class **apInterface;` |
|       - | 1068 | `	sxu32 n;` |
| 2668493 | 1069 | `	if( SySetUsed(pSet) < 1 ){` |
|       - | 1070 | `		/* Empty interface container */` |
|  105953 | 1071 | `		return FALSE;` |
|       - | 1072 | `	}` |
|       - | 1073 | `	/* Point to the set of implemented interfaces */` |
| 2562545 | 1074 | `	apInterface = (ph7_class **)SySetBasePtr(pSet);` |
|       - | 1075 | `	/* Perform the lookup, walking each interface's parent chain so that` |
|       - | 1076 | `	 * Iterator extends Traversable (and similar) is recognized. */` |
| 2677301 | 1077 | `	for( n = 0 ; n < SySetUsed(pSet) ; n++ ){` |
| 2564667 | 1078 | `		if( VmInterfaceReaches(apInterface[n],pClass,0) ){` |
| 2449911 | 1079 | `			return TRUE;` |
|       - | 1080 | `		}` |
|   57383 | 1081 | `	}` |
|  112639 | 1082 | `	return FALSE;` |
| 1334249 | 1083 | `}` |
|       - | 1084 | `/*` |
|       - | 1085 | ` * This function returns TRUE if the given class (first argument)` |
|       - | 1086 | ` * is an instance of the main class (second argument).` |
|       - | 1087 | ` * Otherwise FALSE is returned.` |
|       - | 1088 | ` */` |
| 6889102 | 1089 | `PH7_PRIVATE int PH7_VmInstanceOf(ph7_class *pThis,ph7_class *pClass)` |
|       5 | 1090 | `{` |
|       - | 1091 | `	ph7_class *pParent;` |
|       - | 1092 | `	sxi32 rc;` |
| 6889107 | 1093 | `	if( pThis == pClass ){` |
|       - | 1094 | `		/* Instance of the same class */` |
| 4330295 | 1095 | `		return TRUE;` |
|       - | 1096 | `	}` |
|       - | 1097 | `	/* Check implemented interfaces */` |
| 2558817 | 1098 | `	rc = VmQueryInterfaceSet(pClass,&pThis->aInterface);` |
| 2558817 | 1099 | `	if( rc ){` |
| 2342267 | 1100 | `		return TRUE;` |
|       - | 1101 | `	}` |
|       - | 1102 | `	/* Check parent classes */` |
|  216555 | 1103 | `	pParent = pThis->pBase;` |
|  218553 | 1104 | `	while( pParent ){` |
|  110713 | 1105 | `		if( pParent == pClass ){` |
|       - | 1106 | `			/* Same instance */` |
|    1081 | 1107 | `			return TRUE;` |
|       - | 1108 | `		}` |
|       - | 1109 | `		/* Check the implemented interfaces */` |
|  109637 | 1110 | `		rc = VmQueryInterfaceSet(pClass,&pParent->aInterface);` |
|  109637 | 1111 | `		if( rc ){` |
|  107639 | 1112 | `			return TRUE;` |
|       - | 1113 | `		}` |
|       - | 1114 | `		/* Point to the parent class */` |
|    2003 | 1115 | `		pParent = pParent->pBase;` |
|       5 | 1116 | `	}` |
|       - | 1117 | `	/* Not an instance of the the given class */` |
|  107845 | 1118 | `	return FALSE;` |
| 3444556 | 1119 | `}` |
|       - | 1120 | `/*` |
|       - | 1121 | ` * This function returns TRUE if the given class (first argument)` |
|       - | 1122 | ` * is a subclass of the main class (second argument).` |
|       - | 1123 | ` * Otherwise FALSE is returned.` |
|       - | 1124 | ` */` |
|      46 | 1125 | `static int VmSubclassOf(ph7_class *pClass,ph7_class *pBase)` |
|       3 | 1126 | `{` |
|       - | 1127 | `	SyHashEntry *pEntry;` |
|       - | 1128 | `	SyString *pName;` |
|      83 | 1129 | `	while( pClass ){` |
|      69 | 1130 | `		pName = &pClass->sName;` |
|       - | 1131 | `		/* Query the derived hashtable for a class-hierarchy match */` |
|      69 | 1132 | `		pEntry = SyHashGet(&pBase->hDerived,(const void *)pName->zString,pName->nByte);` |
|      69 | 1133 | `		if( pEntry ){` |
|      25 | 1134 | `			return TRUE;` |
|       - | 1135 | `		}` |
|       - | 1136 | `		/* Query the interfaces implemented by THIS class in the chain — so an` |
|       - | 1137 | `		 * interface implemented by a PARENT (B extends A implements I) is found,` |
|       - | 1138 | `		 * mirroring PH7_VmInstanceOf. The original code queried only the first` |
|       - | 1139 | `		 * class's aInterface, missing inherited interfaces. */` |
|      46 | 1140 | `		if( VmQueryInterfaceSet(pBase,&pClass->aInterface) ){` |
|      11 | 1141 | `			return TRUE;` |
|       - | 1142 | `		}` |
|      36 | 1143 | `		pClass = pClass->pBase;` |
|       2 | 1144 | `	}` |
|       - | 1145 | `	/* Not a subclass */` |
|      16 | 1146 | `	return FALSE;` |
|      26 | 1147 | `}` |
|       - | 1148 | `/*` |
|       - | 1149 | ` * bool is_a(object\|string $object_or_class,string $class,bool $allow_string = false)` |
|       - | 1150 | ` *   Checks if the object/class is of this class or has this class (or interface)` |
|       - | 1151 | ` *   as one of its parents.` |
|       - | 1152 | ` * Parameters` |
|       - | 1153 | ` *  object_or_class` |
|       - | 1154 | ` *   The tested object, or a class name string (only honored when allow_string is TRUE).` |
|       - | 1155 | ` * class` |
|       - | 1156 | ` *  The class or interface name to test against.` |
|       - | 1157 | ` * allow_string` |
|       - | 1158 | ` *  When TRUE, a string first argument is resolved as a class name; php's default` |
|       - | 1159 | ` *  is FALSE, so is_a() returns FALSE for a string first argument without it. The` |
|       - | 1160 | ` *  flag is IGNORED for an object first argument (php).` |
|       - | 1161 | ` * Return` |
|       - | 1162 | ` *   Returns TRUE if object_or_class is of class class or has class as one of its` |
|       - | 1163 | ` *   parents (or implements it as an interface), FALSE otherwise.` |
|       - | 1164 | ` */` |
|      48 | 1165 | `PH7_PRIVATE int vm_builtin_is_a(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1166 | `{` |
|      50 | 1167 | `	int res = 0; /* Assume FALSE by default */` |
|      50 | 1168 | `	if( nArg > 1 ){` |
|      50 | 1169 | `		ph7_class *pThisClass = 0;` |
|      50 | 1170 | `		if( ph7_value_is_object(apArg[0]) ){` |
|       - | 1171 | `			/* An object first argument: allow_string is ignored (php). */` |
|      32 | 1172 | `			pThisClass = ((ph7_class_instance *)apArg[0]->x.pOther)->pClass;` |
|      34 | 1173 | `		}else if( ph7_value_is_string(apArg[0]) && nArg > 2 && ph7_value_to_bool(apArg[2]) ){` |
|       - | 1174 | `			/* A string first argument is resolved to a class ONLY when allow_string` |
|       - | 1175 | `			 * (the 3rd argument) is TRUE — valid php since 5.3.9, and a sibling of` |
|       - | 1176 | `			 * is_subclass_of()'s string form. Autoloads on a miss via` |
|       - | 1177 | `			 * PH7_VmExtractClassFromValue; a leading '\' is anchored there. */` |
|      15 | 1178 | `			pThisClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|       7 | 1179 | `		}` |
|      50 | 1180 | `		if( pThisClass ){` |
|       - | 1181 | `			/* Extract the given class */` |
|      44 | 1182 | `			ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|      44 | 1183 | `			if( pClass ){` |
|       - | 1184 | `				/* Perform the query — instanceof, so the class ITSELF matches` |
|       - | 1185 | `				 * (unlike is_subclass_of, which excludes self). */` |
|      44 | 1186 | `				res = PH7_VmInstanceOf(pThisClass,pClass);` |
|      21 | 1187 | `			}` |
|      21 | 1188 | `		}` |
|      24 | 1189 | `	}` |
|       - | 1190 | `	/* Query result */` |
|      50 | 1191 | `	ph7_result_bool(pCtx,res);` |
|      50 | 1192 | `	return PH7_OK;` |
|       2 | 1193 | `}` |
|       - | 1194 | `/*` |
|       - | 1195 | ` * int spl_object_id(object $object)` |
|       - | 1196 | ` *  Return the integer object handle (per-instance id) of the given object.` |
|       - | 1197 | ` * PHL note: PHP 8 throws a TypeError when passed a non-object; PHL returns NULL` |
|       - | 1198 | ` * to stay consistent with the engine's graceful-degradation convention.` |
|       - | 1199 | ` */` |
|      58 | 1200 | `PH7_PRIVATE int vm_builtin_spl_object_id(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 1201 | `{` |
|       - | 1202 | `	ph7_class_instance *pThis;` |
|      62 | 1203 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|     ! 0 | 1204 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1205 | `		return PH7_OK;` |
|       - | 1206 | `	}` |
|      62 | 1207 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      62 | 1208 | `	ph7_result_int64(pCtx,(ph7_int64)pThis->nObjId);` |
|      62 | 1209 | `	return PH7_OK;` |
|      33 | 1210 | `}` |
|       - | 1211 | `/*` |
|       - | 1212 | ` * string spl_object_hash(object $object)` |
|       - | 1213 | ` *  Return a 32-char hex identifier, unique and stable per live object.` |
|       - | 1214 | ` * PHL note: PHP derives this from the internal handle plus a per-process key, so` |
|       - | 1215 | ` * the exact value is NOT reproducible. PHL returns the zero-padded object id,` |
|       - | 1216 | ` * which preserves the only guaranteed properties: unique per live object, stable` |
|       - | 1217 | ` * across calls, and distinct objects -> distinct strings. A non-object returns` |
|       - | 1218 | ` * NULL (PHP 8 throws a TypeError; see spl_object_id above).` |
|       - | 1219 | ` */` |
|      14 | 1220 | `PH7_PRIVATE int vm_builtin_spl_object_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1221 | `{` |
|       - | 1222 | `	ph7_class_instance *pThis;` |
|      16 | 1223 | `	if( nArg < 1 \|\| !ph7_value_is_object(apArg[0]) ){` |
|     ! 0 | 1224 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1225 | `		return PH7_OK;` |
|       - | 1226 | `	}` |
|      16 | 1227 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      16 | 1228 | `	ph7_result_string_format(pCtx,"%08x%08x%08x%08x",0,0,0,(unsigned int)pThis->nObjId);` |
|      16 | 1229 | `	return PH7_OK;` |
|       9 | 1230 | `}` |
|       - | 1231 | `/*` |
|       - | 1232 | ` * bool is_subclass_of(object\|string $object_or_class,string $class,bool $allow_string = true)` |
|       - | 1233 | ` *   Checks if the object/class has this class (or interface) as one of its` |
|       - | 1234 | ` *   parents — the subclass relation, which EXCLUDES the class itself.` |
|       - | 1235 | ` * Parameters` |
|       - | 1236 | ` *  object_or_class` |
|       - | 1237 | ` *   The tested object, or a class name string (honored unless allow_string is FALSE).` |
|       - | 1238 | ` * class` |
|       - | 1239 | ` *  The class or interface name to test against.` |
|       - | 1240 | ` * allow_string` |
|       - | 1241 | ` *  When FALSE, a string first argument is NOT resolved and the call returns FALSE.` |
|       - | 1242 | ` *  php's default here is TRUE (unlike is_a's FALSE). The flag is IGNORED for an` |
|       - | 1243 | ` *  object first argument (php).` |
|       - | 1244 | ` * Return` |
|       - | 1245 | ` *  Returns TRUE if object_or_class is a proper subclass of class (or implements it` |
|       - | 1246 | ` *  as an interface, directly or via a parent), FALSE otherwise.` |
|       - | 1247 | ` */` |
|      58 | 1248 | `PH7_PRIVATE int vm_builtin_is_subclass_of(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1249 | `{` |
|      61 | 1250 | `	int res = 0; /* Assume FALSE by default */` |
|      61 | 1251 | `	if( nArg > 1 ){` |
|      61 | 1252 | `		ph7_class *pClass = 0;` |
|      61 | 1253 | `		if( ph7_value_is_object(apArg[0]) ){` |
|       - | 1254 | `			/* An object first argument: allow_string is ignored (php). */` |
|      21 | 1255 | `			pClass = ((ph7_class_instance *)apArg[0]->x.pOther)->pClass;` |
|      52 | 1256 | `		}else if( ph7_value_is_string(apArg[0]) && (nArg < 3 \|\| ph7_value_to_bool(apArg[2])) ){` |
|       - | 1257 | `			/* A string first argument is resolved as a class name UNLESS allow_string` |
|       - | 1258 | `			 * (the 3rd argument) is explicitly FALSE — php's default here is TRUE.` |
|       - | 1259 | `			 * Autoloads on a miss via PH7_VmExtractClassFromValue; a leading '\' is` |
|       - | 1260 | `			 * anchored there. Sibling of is_a()'s string form (which defaults OFF). */` |
|      35 | 1261 | `			pClass = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[0]);` |
|      16 | 1262 | `		}` |
|      61 | 1263 | `		if( pClass ){` |
|       - | 1264 | `			/* Extract the target class */` |
|      51 | 1265 | `			ph7_class *pMain = PH7_VmExtractClassFromValue(pCtx->pVm,apArg[1]);` |
|      51 | 1266 | `			if( pMain ){` |
|       - | 1267 | `				/* Perform the query — subclass-only (excludes self, unlike is_a). */` |
|      49 | 1268 | `				res = VmSubclassOf(pClass,pMain);` |
|      23 | 1269 | `			}` |
|      24 | 1270 | `		}` |
|      29 | 1271 | `	}` |
|       - | 1272 | `	/* Query result */` |
|      61 | 1273 | `	ph7_result_bool(pCtx,res);` |
|      61 | 1274 | `	return PH7_OK;` |
|       3 | 1275 | `}` |
|      86 | 1276 | `PH7_PRIVATE int vm_builtin_call_user_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1277 | `{` |
|       - | 1278 | `	ph7_value sResult; /* Store callback return value here */` |
|       - | 1279 | `	sxi32 rc;` |
|      88 | 1280 | `	if( nArg < 1 ){` |
|       - | 1281 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 1282 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1283 | `		return PH7_OK;` |
|       - | 1284 | `	}` |
|       - | 1285 | `	{` |
|       - | 1286 | `		/* php validates the callback BEFORE calling anything; the dispatcher below would` |
|       - | 1287 | `		 * otherwise answer NULL in silence for an unresolvable one. */` |
|      88 | 1288 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",0);` |
|      88 | 1289 | `		if( rcCb != PH7_OK ){` |
|      11 | 1290 | `			return rcCb;` |
|       - | 1291 | `		}` |
|       - | 1292 | `	}` |
|      78 | 1293 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|      78 | 1294 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 1295 | `	/* php passes call_user_func()'s arguments BY VALUE: warn on a by-ref formal and copy */` |
|      78 | 1296 | `	PH7_VmCufDropByRefArgs(pCtx,apArg[0],nArg - 1,&apArg[1]);` |
|       - | 1297 | `	/* Try to invoke the callback. If the call_user_func() call site used` |
|       - | 1298 | `	 * name: arguments (e.g. call_user_func('f', b: 9)), forward them to the` |
|       - | 1299 | `	 * callback. The inner call's argument i is the outer argument i+1 (outer` |
|       - | 1300 | `	 * argument 0 is the callback), so the inner name array is simply the outer` |
|       - | 1301 | `	 * names shifted by one — no copy needed: VmResolveNamedArgs treats any index` |
|       - | 1302 | `	 * >= nTotal as positional, so a shorter map covers the callback's args. */` |
|      87 | 1303 | `	if( pCtx->pArgMap && pCtx->pArgMap->bHasNamed && nArg > 1 ){` |
|      19 | 1304 | `		VmCallArgMap *pOuter = pCtx->pArgMap;` |
|       - | 1305 | `		VmCallArgMap sInner;` |
|       - | 1306 | `		/* Zero first: a field added to the map (sAssertSrc, ...) must read as` |
|       - | 1307 | `		 * unset when forwarded, not as stack garbage. */` |
|      19 | 1308 | `		SyZero(&sInner,sizeof(sInner));` |
|      19 | 1309 | `		sInner.bHasNamed = 1;` |
|      19 | 1310 | `		sInner.bIsNamespaced = 0;` |
|       - | 1311 | `		/* Named args to call_user_func coerce in WEAK mode even from a` |
|       - | 1312 | `		 * strict_types=1 caller (verified vs php 8.5.7): a name: argument` |
|       - | 1313 | `		 * collected into the variadic and re-spread loses the strict context.` |
|       - | 1314 | `		 * call_user_func_array does NOT share this quirk (it stays strict). */` |
|      19 | 1315 | `		sInner.bStrict = 0;` |
|      19 | 1316 | `		sInner.nTotal = pOuter->nTotal > 1 ? pOuter->nTotal - 1 : 0;` |
|      19 | 1317 | `		sInner.aNames = sInner.nTotal > 0 ? &pOuter->aNames[1] : 0;` |
|      19 | 1318 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult,&sInner);` |
|      10 | 1319 | `	}else{` |
|      60 | 1320 | `		rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],nArg - 1,&apArg[1],&sResult);` |
|       - | 1321 | `	}` |
|      78 | 1322 | `	if( rc == PH7_EXCEPTION ){` |
|       - | 1323 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds` |
|       - | 1324 | `		 * through the nearest try/catch instead of returning FALSE. */` |
|      12 | 1325 | `		PH7_MemObjRelease(&sResult);` |
|      12 | 1326 | `		return PH7_EXCEPTION;` |
|       - | 1327 | `	}` |
|      67 | 1328 | `	if( rc != SXRET_OK ){` |
|       - | 1329 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|     ! 0 | 1330 | `		ph7_result_bool(pCtx,0); /* return false */` |
|     ! 0 | 1331 | `	}else{` |
|       - | 1332 | `		/* Callback result */` |
|      67 | 1333 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|       - | 1334 | `	}` |
|      67 | 1335 | `	PH7_MemObjRelease(&sResult);` |
|      67 | 1336 | `	return PH7_OK;` |
|      45 | 1337 | `}` |
|       - | 1338 | `/*` |
|       - | 1339 | ` * value call_user_func_array(callable $callback,array $param_arr)` |
|       - | 1340 | ` *  Call a callback with an array of parameters.` |
|       - | 1341 | ` * Parameter` |
|       - | 1342 | ` *  $callback` |
|       - | 1343 | ` *   The callable to be called.` |
|       - | 1344 | ` * $param_arr` |
|       - | 1345 | ` *  The parameters to be passed to the callback, as an indexed array.` |
|       - | 1346 | ` * Return` |
|       - | 1347 | ` *  Returns the return value of the callback, or FALSE on error.` |
|       - | 1348 | ` */` |
|      38 | 1349 | `PH7_PRIVATE int vm_builtin_call_user_func_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1350 | `{` |
|       - | 1351 | `	ph7_hashmap_node *pEntry; /* Current hashmap entry */` |
|       - | 1352 | `	ph7_value *pValue,sResult;/* Store callback return value here */` |
|       - | 1353 | `	ph7_hashmap *pMap;        /* Target hashmap */` |
|       - | 1354 | `	SySet aArg;               /* Argument value pointers */` |
|      39 | 1355 | `	SyString *aNames = 0;     /* Name map, lazily allocated when a string key appears */` |
|      39 | 1356 | `	sxu32 nSlot = 0;          /* Number of collected arguments */` |
|       - | 1357 | `	sxi32 rc;` |
|       - | 1358 | `	sxu32 n;` |
|      39 | 1359 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 1360 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 1361 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1362 | `		return PH7_OK;` |
|       - | 1363 | `	}` |
|       - | 1364 | `	{` |
|      39 | 1365 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",0);` |
|      39 | 1366 | `		if( rcCb != PH7_OK ){` |
|       3 | 1367 | `			return rcCb;` |
|       - | 1368 | `		}` |
|       - | 1369 | `	}` |
|      37 | 1370 | `	PH7_MemObjInit(pCtx->pVm,&sResult);` |
|      37 | 1371 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 1372 | `	/* Initialize the arguments container */` |
|      37 | 1373 | `	SySetInit(&aArg,&pCtx->pVm->sAllocator,sizeof(ph7_value *));` |
|       - | 1374 | `	/* Turn hashmap entries into callback arguments. A string key becomes a` |
|       - | 1375 | `	 * named argument (PHP 8: call_user_func_array($cb, ['b' => 9])), an integer` |
|       - | 1376 | `	 * key stays positional. The name map points straight at each node's key` |
|       - | 1377 | `	 * blob: the source array stays pinned on the operand stack for the whole` |
|       - | 1378 | `	 * call, so the blobs outlive argument binding. A pure list array (no string` |
|       - | 1379 | `	 * keys) never allocates aNames and takes the plain positional path. */` |
|      37 | 1380 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      37 | 1381 | `	pEntry = pMap->pFirst; /* First inserted entry */` |
|     193 | 1382 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 1383 | `		/* Extract node value */` |
|     157 | 1384 | `		if( (pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx)) != 0 ){` |
|     157 | 1385 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      23 | 1386 | `				if( aNames == 0 ){` |
|       - | 1387 | `					/* First string key: allocate the whole map, zeroed so every` |
|       - | 1388 | `					 * not-yet-seen slot defaults to positional. */` |
|      13 | 1389 | `					aNames = (SyString *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,pMap->nEntry * sizeof(SyString));` |
|      13 | 1390 | `					if( aNames == 0 ){` |
|     ! 0 | 1391 | `						SySetRelease(&aArg);` |
|     ! 0 | 1392 | `						PH7_MemObjRelease(&sResult);` |
|     ! 0 | 1393 | `						return PH7_ContextMemoryError(pCtx);` |
|       - | 1394 | `					}` |
|      13 | 1395 | `					SyZero(aNames,pMap->nEntry * sizeof(SyString));` |
|       6 | 1396 | `				}` |
|      23 | 1397 | `				SyStringInitFromBuf(&aNames[nSlot],SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      11 | 1398 | `			}` |
|     157 | 1399 | `			SySetPut(&aArg,(const void *)&pValue);` |
|     157 | 1400 | `			nSlot++;` |
|      78 | 1401 | `		}` |
|       - | 1402 | `		/* Point to the next entry */` |
|     157 | 1403 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      79 | 1404 | `	}` |
|       - | 1405 | `	/* Try to invoke the callback */` |
|      37 | 1406 | `	if( aNames ){` |
|       - | 1407 | `		VmCallArgMap sMap;` |
|      13 | 1408 | `		SyZero(&sMap,sizeof(sMap)); /* new map fields must read unset, not stack garbage */` |
|      13 | 1409 | `		sMap.bHasNamed = 1;` |
|      13 | 1410 | `		sMap.bIsNamespaced = 0;` |
|       - | 1411 | `		/* Coercion strictness follows the caller's file; the OP_CALL dispatcher` |
|       - | 1412 | `		 * forwards the call site's map on pArgMap (0 only at non-OP_CALL sites). */` |
|      13 | 1413 | `		sMap.bStrict = (pCtx->pArgMap ? pCtx->pArgMap->bStrict : 0);` |
|      13 | 1414 | `		sMap.nTotal = nSlot;` |
|      13 | 1415 | `		sMap.aNames = aNames;` |
|      19 | 1416 | `		rc = PH7_VmCallUserFunctionWithMap(pCtx->pVm,apArg[0],(int)nSlot,` |
|      12 | 1417 | `			(ph7_value **)SySetBasePtr(&aArg),&sResult,&sMap);` |
|      13 | 1418 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,aNames);` |
|       7 | 1419 | `	}else{` |
|      37 | 1420 | `		rc = PH7_VmCallUserFunction(pCtx->pVm,apArg[0],(int)nSlot,` |
|      24 | 1421 | `			(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|       - | 1422 | `	}` |
|      37 | 1423 | `	if( rc == PH7_EXCEPTION ){` |
|       - | 1424 | `		/* The callback raised: propagate so the OP_CALL dispatcher unwinds. */` |
|       5 | 1425 | `		PH7_MemObjRelease(&sResult);` |
|       5 | 1426 | `		SySetRelease(&aArg);` |
|       5 | 1427 | `		return PH7_EXCEPTION;` |
|       - | 1428 | `	}` |
|      33 | 1429 | `	if( rc != SXRET_OK ){` |
|       - | 1430 | `		/* An error occured while invoking the given callback [i.e: not defined] */` |
|     ! 0 | 1431 | `		ph7_result_bool(pCtx,0); /* return false */` |
|     ! 0 | 1432 | `	}else{` |
|       - | 1433 | `		/* Callback result */` |
|      33 | 1434 | `		ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|       - | 1435 | `	}` |
|       - | 1436 | `	/* Cleanup the mess left behind */` |
|      33 | 1437 | `	PH7_MemObjRelease(&sResult);` |
|      33 | 1438 | `	SySetRelease(&aArg);` |
|      33 | 1439 | `	return PH7_OK;` |
|      20 | 1440 | `}` |
|       - | 1441 |  |
