# src/ph7/vm_builtin_spl.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 56/73 lines (76.71%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    4 | ` */` |
|    - |    5 | `#include "ph7int.h"` |
|    - |    6 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|    - |    7 | `/*` |
|    - |    8 | ` * SPL iterators, slice 1 (NEWPLAN band D): SeekableIterator, ArrayIterator,` |
|    - |    9 | ` * ArrayObject, plus the natsort()/natcasesort() array functions they need.` |
|    - |   10 | ` * Embedded-PHP chunk following the Reflection architecture — installed` |
|    - |   11 | ` * inside the bCompilingBuiltin window, backed by the engine's native array` |
|    - |   12 | ` * internal-pointer builtins (reset/next/key/current keep their position on a` |
|    - |   13 | ` * property, so ArrayIterator's cursor IS the backing array's pointer).` |
|    - |   14 | ` */` |
|    - |   15 |  |
|    - |   16 | `/* void __spl_deprecated(string $msg) — E_DEPRECATED with php's exact text` |
|    - |   17 | ` * (no auto-prepended function name, unlike ph7_context_throw_error) */` |
|  ! 0 |   18 | `static int vm_builtin_spl_deprecated(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|  ! 0 |   19 | `{` |
|    - |   20 | `	const char *zMsg;` |
|    - |   21 | `	int nMsg;` |
|  ! 0 |   22 | `	if( nArg < 1 ){` |
|  ! 0 |   23 | `		return PH7_OK;` |
|    - |   24 | `	}` |
|  ! 0 |   25 | `	zMsg = ph7_value_to_string(apArg[0],&nMsg);` |
|  ! 0 |   26 | `	PH7_VmThrowDeprecatedFmt(pCtx->pVm,"%.*s",nMsg,zMsg);` |
|  ! 0 |   27 | `	return PH7_OK;` |
|  ! 0 |   28 | `}` |
|    - |   29 |  |
|    - |   30 | `/* int __weak_create(object $obj) — register/share the weak cell for $obj,` |
|    - |   31 | ` * returning the cell pointer as an opaque int handle */` |
|   14 |   32 | `static int vm_builtin_weak_create(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |   33 | `{` |
|   15 |   34 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |   35 | `	ph7_class_instance *pObj;` |
|   15 |   36 | `	VmWeakCell *pCell = 0;` |
|    - |   37 | `	SyHashEntry *pEntry;` |
|   15 |   38 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|  ! 0 |   39 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |   40 | `		return PH7_OK;` |
|    - |   41 | `	}` |
|   15 |   42 | `	pObj = (ph7_class_instance *)apArg[0]->x.pOther;` |
|   15 |   43 | `	pEntry = SyHashGet(&pVm->hWeakCell,(const void *)&pObj,sizeof(void *));` |
|   15 |   44 | `	if( pEntry ){` |
|    3 |   45 | `		pCell = (VmWeakCell *)pEntry->pUserData;` |
|    3 |   46 | `		pCell->nRef++;` |
|    2 |   47 | `	}else{` |
|   13 |   48 | `		pCell = (VmWeakCell *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmWeakCell));` |
|   13 |   49 | `		if( pCell == 0 ){` |
|  ! 0 |   50 | `			return PH7_ContextMemoryError(pCtx);` |
|    - |   51 | `		}` |
|   13 |   52 | `		pCell->pObj = pObj;` |
|   13 |   53 | `		pCell->nRef = 1;` |
|    - |   54 | `		/* SyHash stores the key POINTER (no copy): key off the cell's own` |
|    - |   55 | `		 * pObj field — heap-stable for the entry's whole lifetime, and it` |
|    - |   56 | `		 * holds the live pointer bytes until the release hook nulls it` |
|    - |   57 | `		 * (which happens only after the entry is deleted). */` |
|   13 |   58 | `		if( SyHashInsert(&pVm->hWeakCell,(const void *)&pCell->pObj,sizeof(void *),pCell) != SXRET_OK ){` |
|  ! 0 |   59 | `			SyMemBackendFree(&pVm->sAllocator,pCell);` |
|  ! 0 |   60 | `			return PH7_ContextMemoryError(pCtx);` |
|    - |   61 | `		}` |
|    - |   62 | `	}` |
|   15 |   63 | `	ph7_result_int64(pCtx,(ph7_int64)(sxu64)(sxuptr)pCell);` |
|   15 |   64 | `	return PH7_OK;` |
|    8 |   65 | `}` |
|    - |   66 | `/* ?object __weak_get(int $handle) — the target instance, or null once dead */` |
|   28 |   67 | `static int vm_builtin_weak_get(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |   68 | `{` |
|    - |   69 | `	VmWeakCell *pCell;` |
|   29 |   70 | `	if( nArg < 1 ){` |
|  ! 0 |   71 | `		ph7_result_null(pCtx);` |
|  ! 0 |   72 | `		return PH7_OK;` |
|    - |   73 | `	}` |
|   29 |   74 | `	pCell = (VmWeakCell *)(sxuptr)(sxu64)ph7_value_to_int64(apArg[0]);` |
|   29 |   75 | `	if( pCell == 0 \|\| pCell->pObj == 0 ){` |
|   11 |   76 | `		ph7_result_null(pCtx);` |
|   11 |   77 | `		return PH7_OK;` |
|    - |   78 | `	}` |
|    - |   79 | `	{` |
|    - |   80 | `		/* Hand the instance back: ph7_result_value's MemObjStore takes the` |
|    - |   81 | `		 * reference, so the temp holds none of its own. */` |
|    - |   82 | `		ph7_value sObj;` |
|   19 |   83 | `		PH7_MemObjInit(pCtx->pVm,&sObj);` |
|   19 |   84 | `		sObj.x.pOther = pCell->pObj;` |
|   19 |   85 | `		MemObjSetType(&sObj,MEMOBJ_OBJ);` |
|   19 |   86 | `		ph7_result_value(pCtx,&sObj);` |
|    - |   87 | `	}` |
|   19 |   88 | `	return PH7_OK;` |
|   15 |   89 | `}` |
|    - |   90 | `/* void __weak_drop(int $handle) — release one PHP-side handle */` |
|   14 |   91 | `static int vm_builtin_weak_drop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |   92 | `{` |
|   15 |   93 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |   94 | `	VmWeakCell *pCell;` |
|   15 |   95 | `	if( nArg < 1 ){` |
|  ! 0 |   96 | `		return PH7_OK;` |
|    - |   97 | `	}` |
|   15 |   98 | `	pCell = (VmWeakCell *)(sxuptr)(sxu64)ph7_value_to_int64(apArg[0]);` |
|   15 |   99 | `	if( pCell == 0 \|\| pCell->nRef == 0 ){` |
|  ! 0 |  100 | `		return PH7_OK;` |
|    - |  101 | `	}` |
|   15 |  102 | `	pCell->nRef--;` |
|   15 |  103 | `	if( pCell->nRef == 0 ){` |
|   13 |  104 | `		if( pCell->pObj ){` |
|    - |  105 | `			/* Still alive: unhook the registry entry before freeing */` |
|    5 |  106 | `			void *pDummy = 0;` |
|    5 |  107 | `			SyHashDeleteEntry(&pVm->hWeakCell,(const void *)&pCell->pObj,sizeof(void *),&pDummy);` |
|    2 |  108 | `		}` |
|   13 |  109 | `		SyMemBackendFree(&pVm->sAllocator,pCell);` |
|    6 |  110 | `	}` |
|   15 |  111 | `	return PH7_OK;` |
|    8 |  112 | `}` |
|    - |  113 |  |
|    - |  114 | `static const char zSplLib[] =` |
|    - |  115 | `"interface SeekableIterator extends Iterator {"` |
|    - |  116 | `" public function seek($offset);"` |
|    - |  117 | `"}"` |
|    - |  118 | `"trait __SplStoreT {"` |
|    - |  119 | `" private $__d = [];"` |
|    - |  120 | `" private $__f = 0;"` |
|    - |  121 | `" private function __splInitStore($array, $flags, $owner){"` |
|    - |  122 | `"  /* $owner is the DECLARING method (php names the declaring class in these"` |
|    - |  123 | `"   * diagnostics, so a RecursiveArrayIterator misuse still says"` |
|    - |  124 | `"   * ArrayIterator::__construct) */"` |
|    - |  125 | `"  if( is_array($array) ){"` |
|    - |  126 | `"   $this->__d = $array;"` |
|    - |  127 | `"  }elseif( is_object($array) ){"` |
|    - |  128 | `"   __spl_deprecated($owner . '(): Using an object as a backing array for '"` |
|    - |  129 | `"    . get_class($this) . ' is deprecated, as it allows violating class"` |
|    - |  130 | `" constraints and invariants');"` |
|    - |  131 | `"  $this->__d = get_object_vars($array);"` |
|    - |  132 | `"  }else{"` |
|    - |  133 | `"   throw new TypeError($owner . '(): Argument #1 ($array) must be of type"` |
|    - |  134 | `" array, ' . get_debug_type($array) . ' given');"` |
|    - |  135 | `"  }"` |
|    - |  136 | `"  $this->__f = (int)$flags;"` |
|    - |  137 | `" }"` |
|    - |  138 | `" public function offsetExists($key){ return array_key_exists($key, $this->__d); }"` |
|    - |  139 | `" public function offsetGet($key){ return $this->__d[$key]; }"` |
|    - |  140 | `" public function offsetSet($key, $value){"` |
|    - |  141 | `"  if( $key === null ){ $this->__d[] = $value; }"` |
|    - |  142 | `"  else { $this->__d[$key] = $value; }"` |
|    - |  143 | `" }"` |
|    - |  144 | `" public function offsetUnset($key){ unset($this->__d[$key]); }"` |
|    - |  145 | `" public function append($value){ $this->__d[] = $value; }"` |
|    - |  146 | `" public function getArrayCopy(){ return $this->__d; }"` |
|    - |  147 | `" public function count(){ return count($this->__d); }"` |
|    - |  148 | `" public function getFlags(){ return $this->__f; }"` |
|    - |  149 | `" public function setFlags($flags){ $this->__f = (int)$flags; }"` |
|    - |  150 | `" public function asort($flags = 0){ asort($this->__d); return true; }"` |
|    - |  151 | `" public function ksort($flags = 0){ ksort($this->__d); return true; }"` |
|    - |  152 | `" public function uasort($callback){ uasort($this->__d, $callback); return true; }"` |
|    - |  153 | `" public function uksort($callback){ uksort($this->__d, $callback); return true; }"` |
|    - |  154 | `" public function natsort(){ uasort($this->__d, 'strnatcmp'); return true; }"` |
|    - |  155 | `" public function natcasesort(){ uasort($this->__d, 'strnatcasecmp'); return true; }"` |
|    - |  156 | `"}"` |
|    - |  157 | `"class ArrayIterator implements SeekableIterator, ArrayAccess, Countable {"` |
|    - |  158 | `" use __SplStoreT;"` |
|    - |  159 | `" const STD_PROP_LIST = 1;"` |
|    - |  160 | `" const ARRAY_AS_PROPS = 2;"` |
|    - |  161 | `" public function __construct($array = [], $flags = 0){"` |
|    - |  162 | `"  $this->__splInitStore($array, $flags, 'ArrayIterator::__construct');"` |
|    - |  163 | `"  reset($this->__d);"` |
|    - |  164 | `" }"` |
|    - |  165 | `" public function current(){"` |
|    - |  166 | `"  if( key($this->__d) === null ){ return null; }"` |
|    - |  167 | `"  return current($this->__d);"` |
|    - |  168 | `" }"` |
|    - |  169 | `" public function key(){ return key($this->__d); }"` |
|    - |  170 | `" public function next(){ next($this->__d); }"` |
|    - |  171 | `" public function rewind(){ reset($this->__d); }"` |
|    - |  172 | `" public function valid(){ return key($this->__d) !== null; }"` |
|    - |  173 | `" public function seek($offset){"` |
|    - |  174 | `"  $offset = (int)$offset;"` |
|    - |  175 | `"  if( $offset < 0 \|\| $offset >= count($this->__d) ){"` |
|    - |  176 | `"   throw new OutOfBoundsException('Seek position ' . $offset . ' is out of range');"` |
|    - |  177 | `"  }"` |
|    - |  178 | `"  reset($this->__d);"` |
|    - |  179 | `"  for( $i = 0; $i < $offset; $i++ ){ next($this->__d); }"` |
|    - |  180 | `" }"` |
|    - |  181 | `"}"` |
|    - |  182 | `"class ArrayObject implements IteratorAggregate, ArrayAccess, Countable {"` |
|    - |  183 | `" use __SplStoreT;"` |
|    - |  184 | `" const STD_PROP_LIST = 1;"` |
|    - |  185 | `" const ARRAY_AS_PROPS = 2;"` |
|    - |  186 | `" private $__it = 'ArrayIterator';"` |
|    - |  187 | `" public function __construct($array = [], $flags = 0, $iteratorClass = 'ArrayIterator'){"` |
|    - |  188 | `"  $this->__splInitStore($array, $flags, 'ArrayObject::__construct');"` |
|    - |  189 | `"  if( $iteratorClass !== 'ArrayIterator' ){ $this->setIteratorClass($iteratorClass); }"` |
|    - |  190 | `" }"` |
|    - |  191 | `" public function getIterator(){"` |
|    - |  192 | `"  $c = $this->__it;"` |
|    - |  193 | `"  return new $c($this->__d);"` |
|    - |  194 | `" }"` |
|    - |  195 | `" public function exchangeArray($array){"` |
|    - |  196 | `"  $old = $this->__d;"` |
|    - |  197 | `"  $this->__splInitStore($array, $this->__f, 'ArrayObject::exchangeArray');"` |
|    - |  198 | `"  return $old;"` |
|    - |  199 | `" }"` |
|    - |  200 | `" public function setIteratorClass($iteratorClass){"` |
|    - |  201 | `"  $c = (string)$iteratorClass;"` |
|    - |  202 | `"  if( $c !== 'ArrayIterator'"` |
|    - |  203 | `"   && (!class_exists($c) \|\| !is_subclass_of($c, 'ArrayIterator')) ){"` |
|    - |  204 | `"   throw new TypeError('ArrayObject::setIteratorClass(): Argument #1"` |
|    - |  205 | `" ($iteratorClass) must be a class name derived from ArrayIterator, ' . $c . ' given');"` |
|    - |  206 | `"  }"` |
|    - |  207 | `"  $this->__it = $c;"` |
|    - |  208 | `" }"` |
|    - |  209 | `" public function getIteratorClass(){ return $this->__it; }"` |
|    - |  210 | `" public function __get($name){"` |
|    - |  211 | `"  if( $this->__f & 2 ){ return $this->__d[$name]; }"` |
|    - |  212 | `"  return null;"` |
|    - |  213 | `" }"` |
|    - |  214 | `" public function __set($name, $value){"` |
|    - |  215 | `"  if( $this->__f & 2 ){ $this->__d[$name] = $value; return; }"` |
|    - |  216 | `"  $this->{$name} = $value;"` |
|    - |  217 | `" }"` |
|    - |  218 | `" public function __isset($name){"` |
|    - |  219 | `"  if( $this->__f & 2 ){ return isset($this->__d[$name]); }"` |
|    - |  220 | `"  return false;"` |
|    - |  221 | `" }"` |
|    - |  222 | `" public function __unset($name){"` |
|    - |  223 | `"  if( $this->__f & 2 ){ unset($this->__d[$name]); }"` |
|    - |  224 | `" }"` |
|    - |  225 | `"}"` |
|    - |  226 | `"function natsort(&$array){ return uasort($array, 'strnatcmp'); }"` |
|    - |  227 | `"function natcasesort(&$array){ return uasort($array, 'strnatcasecmp'); }"` |
|    - |  228 | `"interface OuterIterator extends Iterator {"` |
|    - |  229 | `" public function getInnerIterator();"` |
|    - |  230 | `"}"` |
|    - |  231 | `"class IteratorIterator implements OuterIterator {"` |
|    - |  232 | `" private $__in = null;"` |
|    - |  233 | `" public function __construct($iterator, $class = null){"` |
|    - |  234 | `"  while( $iterator instanceof IteratorAggregate ){ $iterator = $iterator->getIterator(); }"` |
|    - |  235 | `"  if( !($iterator instanceof Iterator) ){"` |
|    - |  236 | `"   throw new TypeError(get_class($this) . '::__construct(): Argument #1 ($iterator)"` |
|    - |  237 | `" must be of type Traversable, ' . get_debug_type($iterator) . ' given');"` |
|    - |  238 | `"  }"` |
|    - |  239 | `"  $this->__in = $iterator;"` |
|    - |  240 | `" }"` |
|    - |  241 | `" public function getInnerIterator(){ return $this->__in; }"` |
|    - |  242 | `" public function current(){ return $this->__in->current(); }"` |
|    - |  243 | `" public function key(){ return $this->__in->key(); }"` |
|    - |  244 | `" public function next(){ $this->__in->next(); }"` |
|    - |  245 | `" public function rewind(){ $this->__in->rewind(); }"` |
|    - |  246 | `" public function valid(){ return $this->__in->valid(); }"` |
|    - |  247 | `"}"` |
|    - |  248 | `"class LimitIterator extends IteratorIterator {"` |
|    - |  249 | `" private $__off = 0;"` |
|    - |  250 | `" private $__lim = -1;"` |
|    - |  251 | `" private $__pos = 0;"` |
|    - |  252 | `" public function __construct($iterator, $offset = 0, $limit = -1){"` |
|    - |  253 | `"  $offset = (int)$offset; $limit = (int)$limit;"` |
|    - |  254 | `"  if( $offset < 0 ){"` |
|    - |  255 | `"   throw new ValueError('LimitIterator::__construct(): Argument #2 ($offset) must be"` |
|    - |  256 | `" greater than or equal to 0');"` |
|    - |  257 | `"  }"` |
|    - |  258 | `"  if( $limit < -1 ){"` |
|    - |  259 | `"   throw new ValueError('LimitIterator::__construct(): Argument #3 ($limit) must be"` |
|    - |  260 | `" greater than or equal to -1');"` |
|    - |  261 | `"  }"` |
|    - |  262 | `"  parent::__construct($iterator);"` |
|    - |  263 | `"  $this->__off = $offset;"` |
|    - |  264 | `"  $this->__lim = $limit;"` |
|    - |  265 | `" }"` |
|    - |  266 | `" public function rewind(){"` |
|    - |  267 | `"  $in = $this->getInnerIterator();"` |
|    - |  268 | `"  $in->rewind();"` |
|    - |  269 | `"  for( $i = 0; $i < $this->__off && $in->valid(); $i++ ){ $in->next(); }"` |
|    - |  270 | `"  $this->__pos = $this->__off;"` |
|    - |  271 | `" }"` |
|    - |  272 | `" public function valid(){"` |
|    - |  273 | `"  if( $this->__lim != -1 && $this->__pos >= $this->__off + $this->__lim ){ return false; }"` |
|    - |  274 | `"  return $this->getInnerIterator()->valid();"` |
|    - |  275 | `" }"` |
|    - |  276 | `" public function next(){ $this->__pos++; $this->getInnerIterator()->next(); }"` |
|    - |  277 | `" public function getPosition(){ return $this->__pos; }"` |
|    - |  278 | `" public function seek($offset){"` |
|    - |  279 | `"  $offset = (int)$offset;"` |
|    - |  280 | `"  if( $offset < $this->__off ){"` |
|    - |  281 | `"   throw new OutOfBoundsException('Cannot seek to ' . $offset . ' which is below the"` |
|    - |  282 | `" offset ' . $this->__off);"` |
|    - |  283 | `"  }"` |
|    - |  284 | `"  if( $this->__lim != -1 && $offset >= $this->__off + $this->__lim ){"` |
|    - |  285 | `"   throw new OutOfBoundsException('Cannot seek to ' . $offset . ' which is behind or"` |
|    - |  286 | `" equal to the limit ' . $this->__lim . ' plus the offset ' . $this->__off);"` |
|    - |  287 | `"  }"` |
|    - |  288 | `"  $in = $this->getInnerIterator();"` |
|    - |  289 | `"  $in->rewind();"` |
|    - |  290 | `"  for( $i = 0; $i < $offset && $in->valid(); $i++ ){ $in->next(); }"` |
|    - |  291 | `"  $this->__pos = $offset;"` |
|    - |  292 | `"  return $this->__pos;"` |
|    - |  293 | `" }"` |
|    - |  294 | `"}"` |
|    - |  295 | `"abstract class FilterIterator extends IteratorIterator {"` |
|    - |  296 | `" abstract public function accept();"` |
|    - |  297 | `" private function __fiFetch(){"` |
|    - |  298 | `"  $in = $this->getInnerIterator();"` |
|    - |  299 | `"  while( $in->valid() && !$this->accept() ){ $in->next(); }"` |
|    - |  300 | `" }"` |
|    - |  301 | `" public function rewind(){ $this->getInnerIterator()->rewind(); $this->__fiFetch(); }"` |
|    - |  302 | `" public function next(){ $this->getInnerIterator()->next(); $this->__fiFetch(); }"` |
|    - |  303 | `"}"` |
|    - |  304 | `"class CallbackFilterIterator extends FilterIterator {"` |
|    - |  305 | `" private $__cb = null;"` |
|    - |  306 | `" public function __construct($iterator, $callback){"` |
|    - |  307 | `"  parent::__construct($iterator);"` |
|    - |  308 | `"  $this->__cb = $callback;"` |
|    - |  309 | `" }"` |
|    - |  310 | `" public function accept(){"` |
|    - |  311 | `"  $in = $this->getInnerIterator();"` |
|    - |  312 | `"  return (bool)call_user_func($this->__cb, $in->current(), $in->key(), $in);"` |
|    - |  313 | `" }"` |
|    - |  314 | `"}"` |
|    - |  315 | `"class RegexIterator extends FilterIterator {"` |
|    - |  316 | `" const USE_KEY = 1;"` |
|    - |  317 | `" const INVERT_MATCH = 2;"` |
|    - |  318 | `" const MATCH = 0;"` |
|    - |  319 | `" const GET_MATCH = 1;"` |
|    - |  320 | `" const ALL_MATCHES = 2;"` |
|    - |  321 | `" const SPLIT = 3;"` |
|    - |  322 | `" const REPLACE = 4;"` |
|    - |  323 | `" public $replacement = null;"` |
|    - |  324 | `" private $__re = '';"` |
|    - |  325 | `" private $__mode = 0;"` |
|    - |  326 | `" private $__rflags = 0;"` |
|    - |  327 | `" private $__pflags = 0;"` |
|    - |  328 | `" private $__cur = null;"` |
|    - |  329 | `" public function __construct($iterator, $pattern, $mode = 0, $flags = 0, $pregFlags = 0){"` |
|    - |  330 | `"  parent::__construct($iterator);"` |
|    - |  331 | `"  $this->__re = (string)$pattern;"` |
|    - |  332 | `"  $this->__mode = (int)$mode;"` |
|    - |  333 | `"  $this->__rflags = (int)$flags;"` |
|    - |  334 | `"  $this->__pflags = (int)$pregFlags;"` |
|    - |  335 | `" }"` |
|    - |  336 | `" public function accept(){"` |
|    - |  337 | `"  $in = $this->getInnerIterator();"` |
|    - |  338 | `"  if( !$in->valid() ){ return false; }"` |
|    - |  339 | `"  $subject = ($this->__rflags & self::USE_KEY) ? $in->key() : $in->current();"` |
|    - |  340 | `"  $subject = (string)$subject;"` |
|    - |  341 | `"  $this->__cur = null;"` |
|    - |  342 | `"  $ok = false;"` |
|    - |  343 | `"  if( $this->__mode === self::MATCH ){"` |
|    - |  344 | `"   $ok = preg_match($this->__re, $subject) > 0;"` |
|    - |  345 | `"  }elseif( $this->__mode === self::GET_MATCH ){"` |
|    - |  346 | `"   $m = null;"` |
|    - |  347 | `"   $ok = preg_match($this->__re, $subject, $m, $this->__pflags) > 0;"` |
|    - |  348 | `"   $this->__cur = $m;"` |
|    - |  349 | `"  }elseif( $this->__mode === self::ALL_MATCHES ){"` |
|    - |  350 | `"   $m = null;"` |
|    - |  351 | `"   $ok = preg_match_all($this->__re, $subject, $m, $this->__pflags) > 0;"` |
|    - |  352 | `"   $this->__cur = $m;"` |
|    - |  353 | `"  }elseif( $this->__mode === self::SPLIT ){"` |
|    - |  354 | `"   $this->__cur = preg_split($this->__re, $subject, -1, $this->__pflags);"` |
|    - |  355 | `"   $ok = is_array($this->__cur) && count($this->__cur) > 1;"` |
|    - |  356 | `"  }elseif( $this->__mode === self::REPLACE ){"` |
|    - |  357 | `"   $n = 0;"` |
|    - |  358 | `"   $this->__cur = preg_replace($this->__re, (string)$this->replacement, $subject, -1, $n);"` |
|    - |  359 | `"   $ok = $n > 0;"` |
|    - |  360 | `"  }"` |
|    - |  361 | `"  if( $this->__rflags & self::INVERT_MATCH ){ $ok = !$ok; }"` |
|    - |  362 | `"  return $ok;"` |
|    - |  363 | `" }"` |
|    - |  364 | `" public function current(){"` |
|    - |  365 | `"  if( $this->__mode === self::MATCH ){ return $this->getInnerIterator()->current(); }"` |
|    - |  366 | `"  return $this->__cur;"` |
|    - |  367 | `" }"` |
|    - |  368 | `" public function getRegex(){ return $this->__re; }"` |
|    - |  369 | `" public function getMode(){ return $this->__mode; }"` |
|    - |  370 | `" public function setMode($mode){ $this->__mode = (int)$mode; }"` |
|    - |  371 | `" public function getFlags(){ return $this->__rflags; }"` |
|    - |  372 | `" public function setFlags($flags){ $this->__rflags = (int)$flags; }"` |
|    - |  373 | `" public function getPregFlags(){ return $this->__pflags; }"` |
|    - |  374 | `" public function setPregFlags($pregFlags){ $this->__pflags = (int)$pregFlags; }"` |
|    - |  375 | `"}"` |
|    - |  376 | `"class AppendIterator implements OuterIterator {"` |
|    - |  377 | `" private $__its = [];"` |
|    - |  378 | `" private $__idx = 0;"` |
|    - |  379 | `" public function __construct(){}"` |
|    - |  380 | `" public function append($iterator){"` |
|    - |  381 | `"  $this->__its[] = $iterator;"` |
|    - |  382 | `"  if( count($this->__its) === 1 ){ $iterator->rewind(); }"` |
|    - |  383 | `" }"` |
|    - |  384 | `" public function getInnerIterator(){ return $this->__its[$this->__idx] ?? null; }"` |
|    - |  385 | `" public function getIteratorIndex(){"` |
|    - |  386 | `"  return isset($this->__its[$this->__idx]) ? $this->__idx : null;"` |
|    - |  387 | `" }"` |
|    - |  388 | `" public function getArrayIterator(){ return new ArrayIterator($this->__its); }"` |
|    - |  389 | `" private function __apAdvance(){"` |
|    - |  390 | `"  while( isset($this->__its[$this->__idx])"` |
|    - |  391 | `"   && !$this->__its[$this->__idx]->valid()"` |
|    - |  392 | `"   && isset($this->__its[$this->__idx + 1]) ){"` |
|    - |  393 | `"   $this->__idx++;"` |
|    - |  394 | `"   $this->__its[$this->__idx]->rewind();"` |
|    - |  395 | `"  }"` |
|    - |  396 | `" }"` |
|    - |  397 | `" public function rewind(){"` |
|    - |  398 | `"  $this->__idx = 0;"` |
|    - |  399 | `"  if( isset($this->__its[0]) ){ $this->__its[0]->rewind(); }"` |
|    - |  400 | `"  $this->__apAdvance();"` |
|    - |  401 | `" }"` |
|    - |  402 | `" public function valid(){"` |
|    - |  403 | `"  $in = $this->getInnerIterator();"` |
|    - |  404 | `"  return $in !== null && $in->valid();"` |
|    - |  405 | `" }"` |
|    - |  406 | `" public function current(){ $in = $this->getInnerIterator(); return $in ? $in->current() : null; }"` |
|    - |  407 | `" public function key(){ $in = $this->getInnerIterator(); return $in ? $in->key() : null; }"` |
|    - |  408 | `" public function next(){"` |
|    - |  409 | `"  $in = $this->getInnerIterator();"` |
|    - |  410 | `"  if( $in ){ $in->next(); }"` |
|    - |  411 | `"  $this->__apAdvance();"` |
|    - |  412 | `" }"` |
|    - |  413 | `"}"` |
|    - |  414 | `"class InfiniteIterator extends IteratorIterator {"` |
|    - |  415 | `" public function next(){"` |
|    - |  416 | `"  $in = $this->getInnerIterator();"` |
|    - |  417 | `"  $in->next();"` |
|    - |  418 | `"  if( !$in->valid() ){ $in->rewind(); }"` |
|    - |  419 | `" }"` |
|    - |  420 | `"}"` |
|    - |  421 | `"class NoRewindIterator extends IteratorIterator {"` |
|    - |  422 | `" public function rewind(){}"` |
|    - |  423 | `"}"` |
|    - |  424 | `"interface RecursiveIterator extends Iterator {"` |
|    - |  425 | `" public function hasChildren();"` |
|    - |  426 | `" public function getChildren();"` |
|    - |  427 | `"}"` |
|    - |  428 | `"class RecursiveArrayIterator extends ArrayIterator implements RecursiveIterator {"` |
|    - |  429 | `" const CHILD_ARRAYS_ONLY = 4;"` |
|    - |  430 | `" public function hasChildren(){"` |
|    - |  431 | `"  $c = $this->current();"` |
|    - |  432 | `"  return is_array($c) \|\| is_object($c);"` |
|    - |  433 | `" }"` |
|    - |  434 | `" public function getChildren(){"` |
|    - |  435 | `"  $c = get_class($this);"` |
|    - |  436 | `"  return new $c($this->current());"` |
|    - |  437 | `" }"` |
|    - |  438 | `"}"` |
|    - |  439 | `"class RecursiveIteratorIterator implements OuterIterator {"` |
|    - |  440 | `" const LEAVES_ONLY = 0;"` |
|    - |  441 | `" const SELF_FIRST = 1;"` |
|    - |  442 | `" const CHILD_FIRST = 2;"` |
|    - |  443 | `" const CATCH_GET_CHILD = 16;"` |
|    - |  444 | `" private $__root = null;"` |
|    - |  445 | `" private $__st = [];"` |
|    - |  446 | `" private $__mode = 0;"` |
|    - |  447 | `" private $__maxDepth = false;"` |
|    - |  448 | `" private $__post = false;"` |
|    - |  449 | `" private $__live = false;"` |
|    - |  450 | `" public function __construct($iterator, $mode = 0, $flags = 0){"` |
|    - |  451 | `"  while( $iterator instanceof IteratorAggregate ){ $iterator = $iterator->getIterator(); }"` |
|    - |  452 | `"  if( !($iterator instanceof RecursiveIterator) ){"` |
|    - |  453 | `"   throw new TypeError('RecursiveIteratorIterator::__construct(): Argument #1"` |
|    - |  454 | `" ($iterator) must be of type RecursiveIterator, ' . get_debug_type($iterator) . ' given');"` |
|    - |  455 | `"  }"` |
|    - |  456 | `"  $this->__root = $iterator;"` |
|    - |  457 | `"  $this->__mode = (int)$mode \| (int)$flags;"` |
|    - |  458 | `" }"` |
|    - |  459 | `" public function getInnerIterator(){ return end($this->__st) ?: $this->__root; }"` |
|    - |  460 | `" public function getSubIterator($level = null){"` |
|    - |  461 | `"  if( $level === null ){ $level = count($this->__st) - 1; }"` |
|    - |  462 | `"  return $this->__st[$level] ?? null;"` |
|    - |  463 | `" }"` |
|    - |  464 | `" public function getDepth(){ return count($this->__st) - 1; }"` |
|    - |  465 | `" public function getMaxDepth(){ return $this->__maxDepth; }"` |
|    - |  466 | `" public function setMaxDepth($maxDepth = -1){"` |
|    - |  467 | `"  $maxDepth = (int)$maxDepth;"` |
|    - |  468 | `"  if( $maxDepth < -1 ){"` |
|    - |  469 | `"   throw new Exception('Parameter max_depth must be >= -1');"` |
|    - |  470 | `"  }"` |
|    - |  471 | `"  $this->__maxDepth = $maxDepth === -1 ? false : $maxDepth;"` |
|    - |  472 | `" }"` |
|    - |  473 | `" public function callHasChildren(){"` |
|    - |  474 | `"  $it = end($this->__st);"` |
|    - |  475 | `"  return $it ? $it->hasChildren() : false;"` |
|    - |  476 | `" }"` |
|    - |  477 | `" public function callGetChildren(){"` |
|    - |  478 | `"  $it = end($this->__st);"` |
|    - |  479 | `"  return $it ? $it->getChildren() : null;"` |
|    - |  480 | `" }"` |
|    - |  481 | `" public function beginIteration(){}"` |
|    - |  482 | `" public function endIteration(){}"` |
|    - |  483 | `" public function beginChildren(){}"` |
|    - |  484 | `" public function endChildren(){}"` |
|    - |  485 | `" public function nextElement(){}"` |
|    - |  486 | `" private function __riDepthOk(){"` |
|    - |  487 | `"  return $this->__maxDepth === false \|\| (count($this->__st) - 1) < $this->__maxDepth;"` |
|    - |  488 | `" }"` |
|    - |  489 | `" private function __riDescend(){"` |
|    - |  490 | `"  /* push the current element's children, positioned at their start */"` |
|    - |  491 | `"  if( $this->__mode & self::CATCH_GET_CHILD ){"` |
|    - |  492 | `"   try { $child = $this->callGetChildren(); }"` |
|    - |  493 | `"   catch (Exception $e) { return false; }"` |
|    - |  494 | `"  }else{"` |
|    - |  495 | `"   $child = $this->callGetChildren();"` |
|    - |  496 | `"  }"` |
|    - |  497 | `"  if( !($child instanceof RecursiveIterator) ){ return false; }"` |
|    - |  498 | `"  $child->rewind();"` |
|    - |  499 | `"  $this->__st[] = $child;"` |
|    - |  500 | `"  $this->beginChildren();"` |
|    - |  501 | `"  return true;"` |
|    - |  502 | `" }"` |
|    - |  503 | `" private function __riFetch(){"` |
|    - |  504 | `"  $m = $this->__mode & 3;"` |
|    - |  505 | `"  for(;;){"` |
|    - |  506 | `"   if( count($this->__st) === 0 ){"` |
|    - |  507 | `"    $this->__live = false;"` |
|    - |  508 | `"    /* php keeps the root level addressable after exhaustion (getDepth 0,"` |
|    - |  509 | `"     * getSubIterator() returns the root) */"` |
|    - |  510 | `"    $this->__st = [$this->__root];"` |
|    - |  511 | `"    $this->endIteration();"` |
|    - |  512 | `"    return;"` |
|    - |  513 | `"   }"` |
|    - |  514 | `"   $it = end($this->__st);"` |
|    - |  515 | `"   if( !$it->valid() ){"` |
|    - |  516 | `"    array_pop($this->__st);"` |
|    - |  517 | `"    $this->endChildren();"` |
|    - |  518 | `"    if( count($this->__st) === 0 ){ continue; }"` |
|    - |  519 | `"    if( $m === self::CHILD_FIRST ){"` |
|    - |  520 | `"     /* the parent node yields now, after its subtree */"` |
|    - |  521 | `"     $this->__post = true;"` |
|    - |  522 | `"     $this->__live = true;"` |
|    - |  523 | `"     return;"` |
|    - |  524 | `"    }"` |
|    - |  525 | `"    end($this->__st)->next();"` |
|    - |  526 | `"    continue;"` |
|    - |  527 | `"   }"` |
|    - |  528 | `"   if( $m === self::LEAVES_ONLY && $it->hasChildren() && $this->__riDepthOk() ){"` |
|    - |  529 | `"    if( $this->__riDescend() ){ continue; }"` |
|    - |  530 | `"   }"` |
|    - |  531 | `"   if( $m === self::CHILD_FIRST && $it->hasChildren() && $this->__riDepthOk() ){"` |
|    - |  532 | `"    if( $this->__riDescend() ){ continue; }"` |
|    - |  533 | `"   }"` |
|    - |  534 | `"   $this->__post = false;"` |
|    - |  535 | `"   $this->__live = true;"` |
|    - |  536 | `"   $this->nextElement();"` |
|    - |  537 | `"   return;"` |
|    - |  538 | `"  }"` |
|    - |  539 | `" }"` |
|    - |  540 | `" public function rewind(){"` |
|    - |  541 | `"  $this->__st = [$this->__root];"` |
|    - |  542 | `"  $this->__root->rewind();"` |
|    - |  543 | `"  $this->__post = false;"` |
|    - |  544 | `"  $this->beginIteration();"` |
|    - |  545 | `"  $this->__riFetch();"` |
|    - |  546 | `" }"` |
|    - |  547 | `" public function valid(){ return $this->__live; }"` |
|    - |  548 | `" public function current(){"` |
|    - |  549 | `"  $it = end($this->__st);"` |
|    - |  550 | `"  return $it ? $it->current() : null;"` |
|    - |  551 | `" }"` |
|    - |  552 | `" public function key(){"` |
|    - |  553 | `"  $it = end($this->__st);"` |
|    - |  554 | `"  return $it ? $it->key() : null;"` |
|    - |  555 | `" }"` |
|    - |  556 | `" public function next(){"` |
|    - |  557 | `"  if( !$this->__live ){ return; }"` |
|    - |  558 | `"  $m = $this->__mode & 3;"` |
|    - |  559 | `"  $it = end($this->__st);"` |
|    - |  560 | `"  if( $this->__post ){"` |
|    - |  561 | `"   /* leaving a CHILD_FIRST post-visit: advance past the node */"` |
|    - |  562 | `"   $this->__post = false;"` |
|    - |  563 | `"   $it->next();"` |
|    - |  564 | `"   $this->__riFetch();"` |
|    - |  565 | `"   return;"` |
|    - |  566 | `"  }"` |
|    - |  567 | `"  if( $m === self::SELF_FIRST && $it->hasChildren() && $this->__riDepthOk() ){"` |
|    - |  568 | `"   if( $this->__riDescend() ){ $this->__riFetch(); return; }"` |
|    - |  569 | `"  }"` |
|    - |  570 | `"  $it->next();"` |
|    - |  571 | `"  $this->__riFetch();"` |
|    - |  572 | `" }"` |
|    - |  573 | `"}"` |
|    - |  574 | `"class WeakReference {"` |
|    - |  575 | `" private $__h = 0;"` |
|    - |  576 | `" private function __construct(){}"` |
|    - |  577 | `" public static function create($object){"` |
|    - |  578 | `"  if( !is_object($object) ){"` |
|    - |  579 | `"   throw new TypeError('WeakReference::create(): Argument #1 ($object) must be"` |
|    - |  580 | `" of type object, ' . get_debug_type($object) . ' given');"` |
|    - |  581 | `"  }"` |
|    - |  582 | `"  $w = new WeakReference();"` |
|    - |  583 | `"  $w->__h = __weak_create($object);"` |
|    - |  584 | `"  return $w;"` |
|    - |  585 | `" }"` |
|    - |  586 | `" public function get(){ return $this->__h ? __weak_get($this->__h) : null; }"` |
|    - |  587 | `" public function __destruct(){"` |
|    - |  588 | `"  if( $this->__h ){ __weak_drop($this->__h); $this->__h = 0; }"` |
|    - |  589 | `" }"` |
|    - |  590 | `"}"` |
|    - |  591 | `"class WeakMap implements ArrayAccess, Countable, IteratorAggregate {"` |
|    - |  592 | `" private $__e = [];"` |
|    - |  593 | `" private function __wmPrune(){"` |
|    - |  594 | `"  foreach( $this->__e as $id => $p ){"` |
|    - |  595 | `"   if( __weak_get($p[0]) === null ){"` |
|    - |  596 | `"    __weak_drop($p[0]);"` |
|    - |  597 | `"    unset($this->__e[$id]);"` |
|    - |  598 | `"   }"` |
|    - |  599 | `"  }"` |
|    - |  600 | `" }"` |
|    - |  601 | `" public function offsetSet($object, $value){"` |
|    - |  602 | `"  if( !is_object($object) ){"` |
|    - |  603 | `"   throw new TypeError('WeakMap key must be an object');"` |
|    - |  604 | `"  }"` |
|    - |  605 | `"  $id = spl_object_id($object);"` |
|    - |  606 | `"  if( isset($this->__e[$id]) && __weak_get($this->__e[$id][0]) !== null ){"` |
|    - |  607 | `"   $this->__e[$id][1] = $value;"` |
|    - |  608 | `"   return;"` |
|    - |  609 | `"  }"` |
|    - |  610 | `"  if( isset($this->__e[$id]) ){ __weak_drop($this->__e[$id][0]); }"` |
|    - |  611 | `"  $this->__e[$id] = [__weak_create($object), $value];"` |
|    - |  612 | `" }"` |
|    - |  613 | `" public function offsetGet($object){"` |
|    - |  614 | `"  if( !is_object($object) ){"` |
|    - |  615 | `"   throw new TypeError('WeakMap key must be an object');"` |
|    - |  616 | `"  }"` |
|    - |  617 | `"  $id = spl_object_id($object);"` |
|    - |  618 | `"  if( isset($this->__e[$id]) && __weak_get($this->__e[$id][0]) === $object ){"` |
|    - |  619 | `"   return $this->__e[$id][1];"` |
|    - |  620 | `"  }"` |
|    - |  621 | `"  throw new Error('Object ' . get_class($object) . '#' . $id . ' not contained"` |
|    - |  622 | `" in WeakMap');"` |
|    - |  623 | `" }"` |
|    - |  624 | `" public function offsetExists($object){"` |
|    - |  625 | `"  if( !is_object($object) ){"` |
|    - |  626 | `"   throw new TypeError('WeakMap key must be an object');"` |
|    - |  627 | `"  }"` |
|    - |  628 | `"  $id = spl_object_id($object);"` |
|    - |  629 | `"  return isset($this->__e[$id]) && __weak_get($this->__e[$id][0]) === $object;"` |
|    - |  630 | `" }"` |
|    - |  631 | `" public function offsetUnset($object){"` |
|    - |  632 | `"  if( !is_object($object) ){"` |
|    - |  633 | `"   throw new TypeError('WeakMap key must be an object');"` |
|    - |  634 | `"  }"` |
|    - |  635 | `"  $id = spl_object_id($object);"` |
|    - |  636 | `"  if( isset($this->__e[$id]) ){"` |
|    - |  637 | `"   __weak_drop($this->__e[$id][0]);"` |
|    - |  638 | `"   unset($this->__e[$id]);"` |
|    - |  639 | `"  }"` |
|    - |  640 | `" }"` |
|    - |  641 | `" public function count(){"` |
|    - |  642 | `"  $this->__wmPrune();"` |
|    - |  643 | `"  return count($this->__e);"` |
|    - |  644 | `" }"` |
|    - |  645 | `" public function getIterator(): Generator {"` |
|    - |  646 | `"  $this->__wmPrune();"` |
|    - |  647 | `"  foreach( $this->__e as $p ){"` |
|    - |  648 | `"   $o = __weak_get($p[0]);"` |
|    - |  649 | `"   if( $o !== null ){ yield $o => $p[1]; }"` |
|    - |  650 | `"  }"` |
|    - |  651 | `" }"` |
|    - |  652 | `" public function __destruct(){"` |
|    - |  653 | `"  foreach( $this->__e as $p ){ __weak_drop($p[0]); }"` |
|    - |  654 | `"  $this->__e = [];"` |
|    - |  655 | `" }"` |
|    - |  656 | `"}"` |
|    - |  657 | `"class EmptyIterator implements Iterator {"` |
|    - |  658 | `" public function current(){"` |
|    - |  659 | `"  throw new BadMethodCallException('Accessing the value of an EmptyIterator');"` |
|    - |  660 | `" }"` |
|    - |  661 | `" public function key(){"` |
|    - |  662 | `"  throw new BadMethodCallException('Accessing the key of an EmptyIterator');"` |
|    - |  663 | `" }"` |
|    - |  664 | `" public function next(){}"` |
|    - |  665 | `" public function rewind(){}"` |
|    - |  666 | `" public function valid(){ return false; }"` |
|    - |  667 | `"}"` |
|    - |  668 | `"class SplDoublyLinkedList implements Iterator, Countable, ArrayAccess {"` |
|    - |  669 | `" const IT_MODE_LIFO = 2;"` |
|    - |  670 | `" const IT_MODE_FIFO = 0;"` |
|    - |  671 | `" const IT_MODE_DELETE = 1;"` |
|    - |  672 | `" const IT_MODE_KEEP = 0;"` |
|    - |  673 | `" private $__q = [];"` |
|    - |  674 | `" private $__mode = 0;"` |
|    - |  675 | `" private $__i = 0;"` |
|    - |  676 | `" public function __construct(){"` |
|    - |  677 | `"  if( $this instanceof SplStack ){ $this->__mode = 2; }"` |
|    - |  678 | `" }"` |
|    - |  679 | `" public function setIteratorMode($mode){"` |
|    - |  680 | `"  $mode = (int)$mode;"` |
|    - |  681 | `"  if( ($this instanceof SplStack \|\| $this instanceof SplQueue)"` |
|    - |  682 | `"   && ($mode & 2) !== ($this->__mode & 2) ){"` |
|    - |  683 | `"   throw new RuntimeException(\"Iterators' LIFO/FIFO modes for SplStack/SplQueue"` |
|    - |  684 | `" objects are frozen\");"` |
|    - |  685 | `"  }"` |
|    - |  686 | `"  $this->__mode = $mode;"` |
|    - |  687 | `" }"` |
|    - |  688 | `" public function getIteratorMode(){ return $this->__mode; }"` |
|    - |  689 | `" public function push($value){ $this->__q[] = $value; }"` |
|    - |  690 | `" public function pop(){"` |
|    - |  691 | `"  if( count($this->__q) === 0 ){"` |
|    - |  692 | `"   throw new RuntimeException(\"Can't pop from an empty datastructure\");"` |
|    - |  693 | `"  }"` |
|    - |  694 | `"  return array_pop($this->__q);"` |
|    - |  695 | `" }"` |
|    - |  696 | `" public function shift(){"` |
|    - |  697 | `"  if( count($this->__q) === 0 ){"` |
|    - |  698 | `"   throw new RuntimeException(\"Can't shift from an empty datastructure\");"` |
|    - |  699 | `"  }"` |
|    - |  700 | `"  return array_shift($this->__q);"` |
|    - |  701 | `" }"` |
|    - |  702 | `" public function unshift($value){ array_unshift($this->__q, $value); }"` |
|    - |  703 | `" public function top(){"` |
|    - |  704 | `"  if( count($this->__q) === 0 ){"` |
|    - |  705 | `"   throw new RuntimeException(\"Can't peek at an empty datastructure\");"` |
|    - |  706 | `"  }"` |
|    - |  707 | `"  return $this->__q[count($this->__q) - 1];"` |
|    - |  708 | `" }"` |
|    - |  709 | `" public function bottom(){"` |
|    - |  710 | `"  if( count($this->__q) === 0 ){"` |
|    - |  711 | `"   throw new RuntimeException(\"Can't peek at an empty datastructure\");"` |
|    - |  712 | `"  }"` |
|    - |  713 | `"  return $this->__q[0];"` |
|    - |  714 | `" }"` |
|    - |  715 | `" public function isEmpty(){ return count($this->__q) === 0; }"` |
|    - |  716 | `" public function count(){ return count($this->__q); }"` |
|    - |  717 | `" public function toArray(){ return $this->__q; }"` |
|    - |  718 | `" public function add($index, $value){"` |
|    - |  719 | `"  $index = (int)$index;"` |
|    - |  720 | `"  if( $index < 0 \|\| $index > count($this->__q) ){"` |
|    - |  721 | `"   throw new OutOfRangeException(get_class($this) === 'SplDoublyLinkedList'"` |
|    - |  722 | `"    ? 'SplDoublyLinkedList::add(): Argument #1 ($index) is out of range'"` |
|    - |  723 | `"    : get_class($this) . '::add(): Argument #1 ($index) is out of range');"` |
|    - |  724 | `"  }"` |
|    - |  725 | `"  array_splice($this->__q, $index, 0, [$value]);"` |
|    - |  726 | `" }"` |
|    - |  727 | `" public function offsetExists($index){"` |
|    - |  728 | `"  return is_int($index) \|\| ctype_digit((string)$index)"` |
|    - |  729 | `"   ? ((int)$index >= 0 && (int)$index < count($this->__q)) : false;"` |
|    - |  730 | `" }"` |
|    - |  731 | `" public function offsetGet($index){"` |
|    - |  732 | `"  $index = (int)$index;"` |
|    - |  733 | `"  if( $index < 0 \|\| $index >= count($this->__q) ){"` |
|    - |  734 | `"   throw new OutOfRangeException('SplDoublyLinkedList::offsetGet(): Argument #1"` |
|    - |  735 | `" ($index) is out of range');"` |
|    - |  736 | `"  }"` |
|    - |  737 | `"  return $this->__q[$index];"` |
|    - |  738 | `" }"` |
|    - |  739 | `" public function offsetSet($index, $value){"` |
|    - |  740 | `"  if( $index === null ){ $this->__q[] = $value; return; }"` |
|    - |  741 | `"  $index = (int)$index;"` |
|    - |  742 | `"  if( $index < 0 \|\| $index >= count($this->__q) ){"` |
|    - |  743 | `"   throw new OutOfRangeException('SplDoublyLinkedList::offsetSet(): Argument #1"` |
|    - |  744 | `" ($index) is out of range');"` |
|    - |  745 | `"  }"` |
|    - |  746 | `"  $this->__q[$index] = $value;"` |
|    - |  747 | `" }"` |
|    - |  748 | `" public function offsetUnset($index){"` |
|    - |  749 | `"  $index = (int)$index;"` |
|    - |  750 | `"  if( $index < 0 \|\| $index >= count($this->__q) ){"` |
|    - |  751 | `"   throw new OutOfRangeException('SplDoublyLinkedList::offsetUnset(): Argument #1"` |
|    - |  752 | `" ($index) is out of range');"` |
|    - |  753 | `"  }"` |
|    - |  754 | `"  array_splice($this->__q, $index, 1);"` |
|    - |  755 | `" }"` |
|    - |  756 | `" public function rewind(){"` |
|    - |  757 | `"  $this->__i = ($this->__mode & 2) ? count($this->__q) - 1 : 0;"` |
|    - |  758 | `" }"` |
|    - |  759 | `" public function valid(){"` |
|    - |  760 | `"  return $this->__i >= 0 && $this->__i < count($this->__q);"` |
|    - |  761 | `" }"` |
|    - |  762 | `" public function current(){ return $this->__q[$this->__i] ?? null; }"` |
|    - |  763 | `" public function key(){ return $this->__i; }"` |
|    - |  764 | `" public function next(){"` |
|    - |  765 | `"  if( $this->__mode & 1 ){"` |
|    - |  766 | `"   /* IT_MODE_DELETE consumes the element just visited */"` |
|    - |  767 | `"   if( $this->__mode & 2 ){ array_pop($this->__q); $this->__i = count($this->__q) - 1; }"` |
|    - |  768 | `"   else { array_shift($this->__q); }"` |
|    - |  769 | `"  }else{"` |
|    - |  770 | `"   $this->__i += ($this->__mode & 2) ? -1 : 1;"` |
|    - |  771 | `"  }"` |
|    - |  772 | `" }"` |
|    - |  773 | `" public function prev(){ $this->__i += ($this->__mode & 2) ? 1 : -1; }"` |
|    - |  774 | `"}"` |
|    - |  775 | `"class SplStack extends SplDoublyLinkedList {}"` |
|    - |  776 | `"class SplQueue extends SplDoublyLinkedList {"` |
|    - |  777 | `" public function enqueue($value){ $this->push($value); }"` |
|    - |  778 | `" public function dequeue(){ return $this->shift(); }"` |
|    - |  779 | `"}"` |
|    - |  780 | `"abstract class SplHeap implements Iterator, Countable {"` |
|    - |  781 | `" private $__h = [];"` |
|    - |  782 | `" abstract protected function compare($value1, $value2);"` |
|    - |  783 | `" private function __hSiftUp($i){"` |
|    - |  784 | `"  while( $i > 0 ){"` |
|    - |  785 | `"   $p = ($i - 1) >> 1;"` |
|    - |  786 | `"   if( $this->compare($this->__h[$i], $this->__h[$p]) <= 0 ){ break; }"` |
|    - |  787 | `"   $t = $this->__h[$p]; $this->__h[$p] = $this->__h[$i]; $this->__h[$i] = $t;"` |
|    - |  788 | `"   $i = $p;"` |
|    - |  789 | `"  }"` |
|    - |  790 | `" }"` |
|    - |  791 | `" private function __hSiftDown($i){"` |
|    - |  792 | `"  $n = count($this->__h);"` |
|    - |  793 | `"  for(;;){"` |
|    - |  794 | `"   $l = 2 * $i + 1; $r = $l + 1; $b = $i;"` |
|    - |  795 | `"   if( $l < $n && $this->compare($this->__h[$l], $this->__h[$b]) > 0 ){ $b = $l; }"` |
|    - |  796 | `"   if( $r < $n && $this->compare($this->__h[$r], $this->__h[$b]) > 0 ){ $b = $r; }"` |
|    - |  797 | `"   if( $b === $i ){ break; }"` |
|    - |  798 | `"   $t = $this->__h[$b]; $this->__h[$b] = $this->__h[$i]; $this->__h[$i] = $t;"` |
|    - |  799 | `"   $i = $b;"` |
|    - |  800 | `"  }"` |
|    - |  801 | `" }"` |
|    - |  802 | `" public function insert($value){"` |
|    - |  803 | `"  $this->__h[] = $value;"` |
|    - |  804 | `"  $this->__hSiftUp(count($this->__h) - 1);"` |
|    - |  805 | `"  return true;"` |
|    - |  806 | `" }"` |
|    - |  807 | `" public function extract(){"` |
|    - |  808 | `"  $n = count($this->__h);"` |
|    - |  809 | `"  if( $n === 0 ){"` |
|    - |  810 | `"   throw new RuntimeException(\"Can't extract from an empty heap\");"` |
|    - |  811 | `"  }"` |
|    - |  812 | `"  $top = $this->__h[0];"` |
|    - |  813 | `"  $last = array_pop($this->__h);"` |
|    - |  814 | `"  if( $n > 1 ){"` |
|    - |  815 | `"   $this->__h[0] = $last;"` |
|    - |  816 | `"   $this->__hSiftDown(0);"` |
|    - |  817 | `"  }"` |
|    - |  818 | `"  return $top;"` |
|    - |  819 | `" }"` |
|    - |  820 | `" public function top(){"` |
|    - |  821 | `"  if( count($this->__h) === 0 ){"` |
|    - |  822 | `"   throw new RuntimeException(\"Can't peek at an empty heap\");"` |
|    - |  823 | `"  }"` |
|    - |  824 | `"  return $this->__h[0];"` |
|    - |  825 | `" }"` |
|    - |  826 | `" public function isEmpty(){ return count($this->__h) === 0; }"` |
|    - |  827 | `" public function count(){ return count($this->__h); }"` |
|    - |  828 | `" public function isCorrupted(){ return false; }"` |
|    - |  829 | `" public function recoverFromCorruption(){ return true; }"` |
|    - |  830 | `" public function rewind(){}"` |
|    - |  831 | `" public function valid(){ return count($this->__h) > 0; }"` |
|    - |  832 | `" public function current(){ return count($this->__h) ? $this->__h[0] : null; }"` |
|    - |  833 | `" public function key(){ return count($this->__h) - 1; }"` |
|    - |  834 | `" public function next(){ if( count($this->__h) ){ $this->extract(); } }"` |
|    - |  835 | `"}"` |
|    - |  836 | `"class SplMinHeap extends SplHeap {"` |
|    - |  837 | `" protected function compare($value1, $value2){ return $value2 <=> $value1; }"` |
|    - |  838 | `"}"` |
|    - |  839 | `"class SplMaxHeap extends SplHeap {"` |
|    - |  840 | `" protected function compare($value1, $value2){ return $value1 <=> $value2; }"` |
|    - |  841 | `"}"` |
|    - |  842 | `"class SplPriorityQueue implements Iterator, Countable {"` |
|    - |  843 | `" const EXTR_DATA = 1;"` |
|    - |  844 | `" const EXTR_PRIORITY = 2;"` |
|    - |  845 | `" const EXTR_BOTH = 3;"` |
|    - |  846 | `" private $__h = [];"` |
|    - |  847 | `" private $__serial = PHP_INT_MAX;"` |
|    - |  848 | `" private $__flags = 1;"` |
|    - |  849 | `" public function compare($priority1, $priority2){ return $priority1 <=> $priority2; }"` |
|    - |  850 | `" private function __pqCmp($a, $b){"` |
|    - |  851 | `"  /* NO tie-break: php's heap swaps only on strictly-greater, which fixes"` |
|    - |  852 | `"   * the (documented-as-undefined) equal-priority order it exhibits */"` |
|    - |  853 | `"  return $this->compare($a[0], $b[0]);"` |
|    - |  854 | `" }"` |
|    - |  855 | `" private function __pqSiftUp($i){"` |
|    - |  856 | `"  while( $i > 0 ){"` |
|    - |  857 | `"   $p = ($i - 1) >> 1;"` |
|    - |  858 | `"   if( $this->__pqCmp($this->__h[$i], $this->__h[$p]) <= 0 ){ break; }"` |
|    - |  859 | `"   $t = $this->__h[$p]; $this->__h[$p] = $this->__h[$i]; $this->__h[$i] = $t;"` |
|    - |  860 | `"   $i = $p;"` |
|    - |  861 | `"  }"` |
|    - |  862 | `" }"` |
|    - |  863 | `" private function __pqSiftDown($i){"` |
|    - |  864 | `"  $n = count($this->__h);"` |
|    - |  865 | `"  for(;;){"` |
|    - |  866 | `"   $l = 2 * $i + 1; $r = $l + 1; $b = $i;"` |
|    - |  867 | `"   if( $l < $n && $this->__pqCmp($this->__h[$l], $this->__h[$b]) > 0 ){ $b = $l; }"` |
|    - |  868 | `"   if( $r < $n && $this->__pqCmp($this->__h[$r], $this->__h[$b]) > 0 ){ $b = $r; }"` |
|    - |  869 | `"   if( $b === $i ){ break; }"` |
|    - |  870 | `"   $t = $this->__h[$b]; $this->__h[$b] = $this->__h[$i]; $this->__h[$i] = $t;"` |
|    - |  871 | `"   $i = $b;"` |
|    - |  872 | `"  }"` |
|    - |  873 | `" }"` |
|    - |  874 | `" public function insert($value, $priority){"` |
|    - |  875 | `"  $this->__h[] = [$priority, $this->__serial--, $value];"` |
|    - |  876 | `"  $this->__pqSiftUp(count($this->__h) - 1);"` |
|    - |  877 | `"  return true;"` |
|    - |  878 | `" }"` |
|    - |  879 | `" private function __pqShape($node){"` |
|    - |  880 | `"  if( $this->__flags === self::EXTR_BOTH ){"` |
|    - |  881 | `"   return ['data' => $node[2], 'priority' => $node[0]];"` |
|    - |  882 | `"  }"` |
|    - |  883 | `"  if( $this->__flags === self::EXTR_PRIORITY ){ return $node[0]; }"` |
|    - |  884 | `"  return $node[2];"` |
|    - |  885 | `" }"` |
|    - |  886 | `" public function extract(){"` |
|    - |  887 | `"  $n = count($this->__h);"` |
|    - |  888 | `"  if( $n === 0 ){"` |
|    - |  889 | `"   throw new RuntimeException(\"Can't extract from an empty heap\");"` |
|    - |  890 | `"  }"` |
|    - |  891 | `"  $top = $this->__h[0];"` |
|    - |  892 | `"  $last = array_pop($this->__h);"` |
|    - |  893 | `"  if( $n > 1 ){"` |
|    - |  894 | `"   $this->__h[0] = $last;"` |
|    - |  895 | `"   $this->__pqSiftDown(0);"` |
|    - |  896 | `"  }"` |
|    - |  897 | `"  return $this->__pqShape($top);"` |
|    - |  898 | `" }"` |
|    - |  899 | `" public function top(){"` |
|    - |  900 | `"  if( count($this->__h) === 0 ){"` |
|    - |  901 | `"   throw new RuntimeException(\"Can't peek at an empty heap\");"` |
|    - |  902 | `"  }"` |
|    - |  903 | `"  return $this->__pqShape($this->__h[0]);"` |
|    - |  904 | `" }"` |
|    - |  905 | `" public function setExtractFlags($flags){ $this->__flags = (int)$flags; }"` |
|    - |  906 | `" public function getExtractFlags(){ return $this->__flags; }"` |
|    - |  907 | `" public function isEmpty(){ return count($this->__h) === 0; }"` |
|    - |  908 | `" public function count(){ return count($this->__h); }"` |
|    - |  909 | `" public function isCorrupted(){ return false; }"` |
|    - |  910 | `" public function recoverFromCorruption(){ return true; }"` |
|    - |  911 | `" public function rewind(){}"` |
|    - |  912 | `" public function valid(){ return count($this->__h) > 0; }"` |
|    - |  913 | `" public function current(){ return count($this->__h) ? $this->__pqShape($this->__h[0]) : null; }"` |
|    - |  914 | `" public function key(){ return count($this->__h) - 1; }"` |
|    - |  915 | `" public function next(){ if( count($this->__h) ){ $this->extract(); } }"` |
|    - |  916 | `"}"` |
|    - |  917 | `"class SplFixedArray implements ArrayAccess, Countable, IteratorAggregate, JsonSerializable {"` |
|    - |  918 | `" private $__a = [];"` |
|    - |  919 | `" private $__n = 0;"` |
|    - |  920 | `" public function __construct($size = 0){"` |
|    - |  921 | `"  $this->setSize((int)$size);"` |
|    - |  922 | `" }"` |
|    - |  923 | `" private function __faIdx($index, $method){"` |
|    - |  924 | `"  if( !is_int($index) ){"` |
|    - |  925 | `"   if( is_string($index) && ctype_digit($index) ){"` |
|    - |  926 | `"    $index = (int)$index;"` |
|    - |  927 | `"   }else{"` |
|    - |  928 | `"    throw new TypeError('Cannot access offset of type ' . get_debug_type($index)"` |
|    - |  929 | `"     . ' on SplFixedArray');"` |
|    - |  930 | `"   }"` |
|    - |  931 | `"  }"` |
|    - |  932 | `"  if( $index < 0 \|\| $index >= $this->__n ){"` |
|    - |  933 | `"   throw new OutOfBoundsException('Index invalid or out of range');"` |
|    - |  934 | `"  }"` |
|    - |  935 | `"  return $index;"` |
|    - |  936 | `" }"` |
|    - |  937 | `" public function offsetExists($index){"` |
|    - |  938 | `"  if( !is_int($index) && !(is_string($index) && ctype_digit($index)) ){ return false; }"` |
|    - |  939 | `"  $index = (int)$index;"` |
|    - |  940 | `"  return $index >= 0 && $index < $this->__n && $this->__a[$index] !== null;"` |
|    - |  941 | `" }"` |
|    - |  942 | `" public function offsetGet($index){ return $this->__a[$this->__faIdx($index, 'offsetGet')]; }"` |
|    - |  943 | `" public function offsetSet($index, $value){ $this->__a[$this->__faIdx($index, 'offsetSet')] = $value; }"` |
|    - |  944 | `" public function offsetUnset($index){ $this->__a[$this->__faIdx($index, 'offsetUnset')] = null; }"` |
|    - |  945 | `" public function getSize(){ return $this->__n; }"` |
|    - |  946 | `" public function setSize($size){"` |
|    - |  947 | `"  $size = (int)$size;"` |
|    - |  948 | `"  if( $size < 0 ){"` |
|    - |  949 | `"   throw new ValueError('SplFixedArray::setSize(): Argument #1 ($size) must be"` |
|    - |  950 | `" greater than or equal to 0');"` |
|    - |  951 | `"  }"` |
|    - |  952 | `"  if( $size < $this->__n ){"` |
|    - |  953 | `"   $this->__a = array_slice($this->__a, 0, $size);"` |
|    - |  954 | `"  }else{"` |
|    - |  955 | `"   for( $i = $this->__n; $i < $size; $i++ ){ $this->__a[$i] = null; }"` |
|    - |  956 | `"  }"` |
|    - |  957 | `"  $this->__n = $size;"` |
|    - |  958 | `"  return true;"` |
|    - |  959 | `" }"` |
|    - |  960 | `" public function count(){ return $this->__n; }"` |
|    - |  961 | `" public function toArray(){ return $this->__a; }"` |
|    - |  962 | `" public static function fromArray($array, $preserveKeys = true){"` |
|    - |  963 | `"  $f = new SplFixedArray(0);"` |
|    - |  964 | `"  if( $preserveKeys ){"` |
|    - |  965 | `"   $max = -1;"` |
|    - |  966 | `"   foreach( $array as $k => $v ){"` |
|    - |  967 | `"    if( !is_int($k) \|\| $k < 0 ){"` |
|    - |  968 | `"     throw new InvalidArgumentException('array must contain only positive integer keys');"` |
|    - |  969 | `"    }"` |
|    - |  970 | `"    if( $k > $max ){ $max = $k; }"` |
|    - |  971 | `"   }"` |
|    - |  972 | `"   $f->setSize($max + 1);"` |
|    - |  973 | `"   foreach( $array as $k => $v ){ $f[$k] = $v; }"` |
|    - |  974 | `"  }else{"` |
|    - |  975 | `"   $vals = array_values($array);"` |
|    - |  976 | `"   $f->setSize(count($vals));"` |
|    - |  977 | `"   foreach( $vals as $k => $v ){ $f[$k] = $v; }"` |
|    - |  978 | `"  }"` |
|    - |  979 | `"  return $f;"` |
|    - |  980 | `" }"` |
|    - |  981 | `" public function getIterator(): Generator {"` |
|    - |  982 | `"  for( $i = 0; $i < $this->__n; $i++ ){ yield $i => $this->__a[$i]; }"` |
|    - |  983 | `" }"` |
|    - |  984 | `" public function jsonSerialize(){ return $this->__a; }"` |
|    - |  985 | `"}"` |
|    - |  986 | `"class SplObjectStorage implements Countable, Iterator, ArrayAccess {"` |
|    - |  987 | `" private $__o = [];"` |
|    - |  988 | `" private $__i = 0;"` |
|    - |  989 | `" public function attach($object, $info = null){"` |
|    - |  990 | `"  __spl_deprecated('Method SplObjectStorage::attach() is deprecated since 8.5, use"` |
|    - |  991 | `" method SplObjectStorage::offsetSet() instead');"` |
|    - |  992 | `"  $this->offsetSet($object, $info);"` |
|    - |  993 | `" }"` |
|    - |  994 | `" public function detach($object){"` |
|    - |  995 | `"  __spl_deprecated('Method SplObjectStorage::detach() is deprecated since 8.5, use"` |
|    - |  996 | `" method SplObjectStorage::offsetUnset() instead');"` |
|    - |  997 | `"  $this->offsetUnset($object);"` |
|    - |  998 | `" }"` |
|    - |  999 | `" public function contains($object){"` |
|    - | 1000 | `"  __spl_deprecated('Method SplObjectStorage::contains() is deprecated since 8.5, use"` |
|    - | 1001 | `" method SplObjectStorage::offsetExists() instead');"` |
|    - | 1002 | `"  return $this->offsetExists($object);"` |
|    - | 1003 | `" }"` |
|    - | 1004 | `" public function offsetSet($object, $info = null){"` |
|    - | 1005 | `"  $this->__o[spl_object_id($object)] = [$object, $info];"` |
|    - | 1006 | `" }"` |
|    - | 1007 | `" public function offsetExists($object){"` |
|    - | 1008 | `"  return isset($this->__o[spl_object_id($object)]);"` |
|    - | 1009 | `" }"` |
|    - | 1010 | `" public function offsetGet($object){"` |
|    - | 1011 | `"  $id = spl_object_id($object);"` |
|    - | 1012 | `"  if( !isset($this->__o[$id]) ){"` |
|    - | 1013 | `"   throw new UnexpectedValueException('Object not found');"` |
|    - | 1014 | `"  }"` |
|    - | 1015 | `"  return $this->__o[$id][1];"` |
|    - | 1016 | `" }"` |
|    - | 1017 | `" public function offsetUnset($object){"` |
|    - | 1018 | `"  unset($this->__o[spl_object_id($object)]);"` |
|    - | 1019 | `" }"` |
|    - | 1020 | `" public function addAll($storage){"` |
|    - | 1021 | `"  foreach( $storage as $obj ){"` |
|    - | 1022 | `"   $this->offsetSet($obj, $storage[$obj]);"` |
|    - | 1023 | `"  }"` |
|    - | 1024 | `"  return $this->count();"` |
|    - | 1025 | `" }"` |
|    - | 1026 | `" public function removeAll($storage){"` |
|    - | 1027 | `"  foreach( $storage as $obj ){ $this->offsetUnset($obj); }"` |
|    - | 1028 | `"  return $this->count();"` |
|    - | 1029 | `" }"` |
|    - | 1030 | `" public function removeAllExcept($storage){"` |
|    - | 1031 | `"  foreach( $this->__o as $id => $pair ){"` |
|    - | 1032 | `"   if( !$storage->offsetExists($pair[0]) ){ unset($this->__o[$id]); }"` |
|    - | 1033 | `"  }"` |
|    - | 1034 | `"  return $this->count();"` |
|    - | 1035 | `" }"` |
|    - | 1036 | `" public function getHash($object){ return spl_object_hash($object); }"` |
|    - | 1037 | `" public function count($mode = 0){ return count($this->__o); }"` |
|    - | 1038 | `" public function getInfo(){"` |
|    - | 1039 | `"  $pair = array_values($this->__o)[$this->__i] ?? null;"` |
|    - | 1040 | `"  return $pair === null ? null : $pair[1];"` |
|    - | 1041 | `" }"` |
|    - | 1042 | `" public function setInfo($info){"` |
|    - | 1043 | `"  $keys = array_keys($this->__o);"` |
|    - | 1044 | `"  if( isset($keys[$this->__i]) ){ $this->__o[$keys[$this->__i]][1] = $info; }"` |
|    - | 1045 | `" }"` |
|    - | 1046 | `" public function rewind(){ $this->__i = 0; }"` |
|    - | 1047 | `" public function valid(){ return $this->__i < count($this->__o); }"` |
|    - | 1048 | `" public function key(){ return $this->__i; }"` |
|    - | 1049 | `" public function current(){"` |
|    - | 1050 | `"  $pair = array_values($this->__o)[$this->__i] ?? null;"` |
|    - | 1051 | `"  return $pair === null ? null : $pair[0];"` |
|    - | 1052 | `" }"` |
|    - | 1053 | `" public function next(){ $this->__i++; }"` |
|    - | 1054 | `"}"` |
|    - | 1055 | `"interface SplObserver {"` |
|    - | 1056 | `" public function update(SplSubject $subject);"` |
|    - | 1057 | `"}"` |
|    - | 1058 | `"interface SplSubject {"` |
|    - | 1059 | `" public function attach(SplObserver $observer);"` |
|    - | 1060 | `" public function detach(SplObserver $observer);"` |
|    - | 1061 | `" public function notify();"` |
|    - | 1062 | `"}"` |
|    - | 1063 | `;` |
|    - | 1064 |  |
| 3946 | 1065 | `PH7_PRIVATE sxi32 PH7_VmInstallSpl(ph7_vm *pVm)` |
|    5 | 1066 | `{` |
| 3951 | 1067 | `	ph7_create_function(&(*pVm),"__spl_deprecated",vm_builtin_spl_deprecated,0);` |
| 3951 | 1068 | `	ph7_create_function(&(*pVm),"__weak_create",vm_builtin_weak_create,0);` |
| 3951 | 1069 | `	ph7_create_function(&(*pVm),"__weak_get",vm_builtin_weak_get,0);` |
| 3951 | 1070 | `	ph7_create_function(&(*pVm),"__weak_drop",vm_builtin_weak_drop,0);` |
| 3951 | 1071 | `	return PH7_VmEvalBuiltinChunk(&(*pVm),zSplLib,sizeof(zSplLib)-1);` |
|    5 | 1072 | `}` |
|    - | 1073 |  |
|    - | 1074 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - | 1075 |  |
|    - | 1076 | `#ifdef PH7_DISABLE_BUILTIN_FUNC` |
|    - | 1077 | `/* Tiny build: no SPL (builtin layer disabled) */` |
|    - | 1078 | `PH7_PRIVATE sxi32 PH7_VmInstallSpl(ph7_vm *pVm){ (void)pVm; return SXRET_OK; }` |
|    - | 1079 | `#endif` |
|    - | 1080 |  |
