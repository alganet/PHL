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
|    - |  211 | `"  /* ?? null: a missing key must not raise php's undefined-array-key warning"` |
|    - |  212 | `"   * from INSIDE the wrapper (php's ArrayObject warns about the PROPERTY, and"` |
|    - |  213 | `"   * PHL's own magic-read path already diagnoses that) */"` |
|    - |  214 | `"  if( $this->__f & 2 ){ return $this->__d[$name] ?? null; }"` |
|    - |  215 | `"  return null;"` |
|    - |  216 | `" }"` |
|    - |  217 | `" public function __set($name, $value){"` |
|    - |  218 | `"  if( $this->__f & 2 ){ $this->__d[$name] = $value; return; }"` |
|    - |  219 | `"  $this->{$name} = $value;"` |
|    - |  220 | `" }"` |
|    - |  221 | `" public function __isset($name){"` |
|    - |  222 | `"  if( $this->__f & 2 ){ return isset($this->__d[$name]); }"` |
|    - |  223 | `"  return false;"` |
|    - |  224 | `" }"` |
|    - |  225 | `" public function __unset($name){"` |
|    - |  226 | `"  if( $this->__f & 2 ){ unset($this->__d[$name]); }"` |
|    - |  227 | `" }"` |
|    - |  228 | `"}"` |
|    - |  229 | `"function natsort(&$array){ return uasort($array, 'strnatcmp'); }"` |
|    - |  230 | `"function natcasesort(&$array){ return uasort($array, 'strnatcasecmp'); }"` |
|    - |  231 | `"interface OuterIterator extends Iterator {"` |
|    - |  232 | `" public function getInnerIterator();"` |
|    - |  233 | `"}"` |
|    - |  234 | `"class IteratorIterator implements OuterIterator {"` |
|    - |  235 | `" private $__in = null;"` |
|    - |  236 | `" public function __construct($iterator, $class = null){"` |
|    - |  237 | `"  while( $iterator instanceof IteratorAggregate ){ $iterator = $iterator->getIterator(); }"` |
|    - |  238 | `"  if( !($iterator instanceof Iterator) ){"` |
|    - |  239 | `"   throw new TypeError(get_class($this) . '::__construct(): Argument #1 ($iterator)"` |
|    - |  240 | `" must be of type Traversable, ' . get_debug_type($iterator) . ' given');"` |
|    - |  241 | `"  }"` |
|    - |  242 | `"  $this->__in = $iterator;"` |
|    - |  243 | `" }"` |
|    - |  244 | `" public function getInnerIterator(){ return $this->__in; }"` |
|    - |  245 | `" public function current(){ return $this->__in->current(); }"` |
|    - |  246 | `" public function key(){ return $this->__in->key(); }"` |
|    - |  247 | `" public function next(){ $this->__in->next(); }"` |
|    - |  248 | `" public function rewind(){ $this->__in->rewind(); }"` |
|    - |  249 | `" public function valid(){ return $this->__in->valid(); }"` |
|    - |  250 | `"}"` |
|    - |  251 | `"class LimitIterator extends IteratorIterator {"` |
|    - |  252 | `" private $__off = 0;"` |
|    - |  253 | `" private $__lim = -1;"` |
|    - |  254 | `" private $__pos = 0;"` |
|    - |  255 | `" public function __construct($iterator, $offset = 0, $limit = -1){"` |
|    - |  256 | `"  $offset = (int)$offset; $limit = (int)$limit;"` |
|    - |  257 | `"  if( $offset < 0 ){"` |
|    - |  258 | `"   throw new ValueError('LimitIterator::__construct(): Argument #2 ($offset) must be"` |
|    - |  259 | `" greater than or equal to 0');"` |
|    - |  260 | `"  }"` |
|    - |  261 | `"  if( $limit < -1 ){"` |
|    - |  262 | `"   throw new ValueError('LimitIterator::__construct(): Argument #3 ($limit) must be"` |
|    - |  263 | `" greater than or equal to -1');"` |
|    - |  264 | `"  }"` |
|    - |  265 | `"  parent::__construct($iterator);"` |
|    - |  266 | `"  $this->__off = $offset;"` |
|    - |  267 | `"  $this->__lim = $limit;"` |
|    - |  268 | `" }"` |
|    - |  269 | `" public function rewind(){"` |
|    - |  270 | `"  $in = $this->getInnerIterator();"` |
|    - |  271 | `"  $in->rewind();"` |
|    - |  272 | `"  for( $i = 0; $i < $this->__off && $in->valid(); $i++ ){ $in->next(); }"` |
|    - |  273 | `"  $this->__pos = $this->__off;"` |
|    - |  274 | `" }"` |
|    - |  275 | `" public function valid(){"` |
|    - |  276 | `"  if( $this->__lim != -1 && $this->__pos >= $this->__off + $this->__lim ){ return false; }"` |
|    - |  277 | `"  return $this->getInnerIterator()->valid();"` |
|    - |  278 | `" }"` |
|    - |  279 | `" public function next(){ $this->__pos++; $this->getInnerIterator()->next(); }"` |
|    - |  280 | `" public function getPosition(){ return $this->__pos; }"` |
|    - |  281 | `" public function seek($offset){"` |
|    - |  282 | `"  $offset = (int)$offset;"` |
|    - |  283 | `"  if( $offset < $this->__off ){"` |
|    - |  284 | `"   throw new OutOfBoundsException('Cannot seek to ' . $offset . ' which is below the"` |
|    - |  285 | `" offset ' . $this->__off);"` |
|    - |  286 | `"  }"` |
|    - |  287 | `"  if( $this->__lim != -1 && $offset >= $this->__off + $this->__lim ){"` |
|    - |  288 | `"   throw new OutOfBoundsException('Cannot seek to ' . $offset . ' which is behind or"` |
|    - |  289 | `" equal to the limit ' . $this->__lim . ' plus the offset ' . $this->__off);"` |
|    - |  290 | `"  }"` |
|    - |  291 | `"  $in = $this->getInnerIterator();"` |
|    - |  292 | `"  $in->rewind();"` |
|    - |  293 | `"  for( $i = 0; $i < $offset && $in->valid(); $i++ ){ $in->next(); }"` |
|    - |  294 | `"  $this->__pos = $offset;"` |
|    - |  295 | `"  return $this->__pos;"` |
|    - |  296 | `" }"` |
|    - |  297 | `"}"` |
|    - |  298 | `"abstract class FilterIterator extends IteratorIterator {"` |
|    - |  299 | `" abstract public function accept();"` |
|    - |  300 | `" private function __fiFetch(){"` |
|    - |  301 | `"  $in = $this->getInnerIterator();"` |
|    - |  302 | `"  while( $in->valid() && !$this->accept() ){ $in->next(); }"` |
|    - |  303 | `" }"` |
|    - |  304 | `" public function rewind(){ $this->getInnerIterator()->rewind(); $this->__fiFetch(); }"` |
|    - |  305 | `" public function next(){ $this->getInnerIterator()->next(); $this->__fiFetch(); }"` |
|    - |  306 | `"}"` |
|    - |  307 | `"class CallbackFilterIterator extends FilterIterator {"` |
|    - |  308 | `" private $__cb = null;"` |
|    - |  309 | `" public function __construct($iterator, $callback){"` |
|    - |  310 | `"  parent::__construct($iterator);"` |
|    - |  311 | `"  $this->__cb = $callback;"` |
|    - |  312 | `" }"` |
|    - |  313 | `" public function accept(){"` |
|    - |  314 | `"  $in = $this->getInnerIterator();"` |
|    - |  315 | `"  return (bool)call_user_func($this->__cb, $in->current(), $in->key(), $in);"` |
|    - |  316 | `" }"` |
|    - |  317 | `"}"` |
|    - |  318 | `"class RegexIterator extends FilterIterator {"` |
|    - |  319 | `" const USE_KEY = 1;"` |
|    - |  320 | `" const INVERT_MATCH = 2;"` |
|    - |  321 | `" const MATCH = 0;"` |
|    - |  322 | `" const GET_MATCH = 1;"` |
|    - |  323 | `" const ALL_MATCHES = 2;"` |
|    - |  324 | `" const SPLIT = 3;"` |
|    - |  325 | `" const REPLACE = 4;"` |
|    - |  326 | `" public $replacement = null;"` |
|    - |  327 | `" private $__re = '';"` |
|    - |  328 | `" private $__mode = 0;"` |
|    - |  329 | `" private $__rflags = 0;"` |
|    - |  330 | `" private $__pflags = 0;"` |
|    - |  331 | `" private $__cur = null;"` |
|    - |  332 | `" public function __construct($iterator, $pattern, $mode = 0, $flags = 0, $pregFlags = 0){"` |
|    - |  333 | `"  parent::__construct($iterator);"` |
|    - |  334 | `"  $this->__re = (string)$pattern;"` |
|    - |  335 | `"  $this->__mode = (int)$mode;"` |
|    - |  336 | `"  $this->__rflags = (int)$flags;"` |
|    - |  337 | `"  $this->__pflags = (int)$pregFlags;"` |
|    - |  338 | `" }"` |
|    - |  339 | `" public function accept(){"` |
|    - |  340 | `"  $in = $this->getInnerIterator();"` |
|    - |  341 | `"  if( !$in->valid() ){ return false; }"` |
|    - |  342 | `"  $subject = ($this->__rflags & self::USE_KEY) ? $in->key() : $in->current();"` |
|    - |  343 | `"  $subject = (string)$subject;"` |
|    - |  344 | `"  $this->__cur = null;"` |
|    - |  345 | `"  $ok = false;"` |
|    - |  346 | `"  if( $this->__mode === self::MATCH ){"` |
|    - |  347 | `"   $ok = preg_match($this->__re, $subject) > 0;"` |
|    - |  348 | `"  }elseif( $this->__mode === self::GET_MATCH ){"` |
|    - |  349 | `"   $m = null;"` |
|    - |  350 | `"   $ok = preg_match($this->__re, $subject, $m, $this->__pflags) > 0;"` |
|    - |  351 | `"   $this->__cur = $m;"` |
|    - |  352 | `"  }elseif( $this->__mode === self::ALL_MATCHES ){"` |
|    - |  353 | `"   $m = null;"` |
|    - |  354 | `"   $ok = preg_match_all($this->__re, $subject, $m, $this->__pflags) > 0;"` |
|    - |  355 | `"   $this->__cur = $m;"` |
|    - |  356 | `"  }elseif( $this->__mode === self::SPLIT ){"` |
|    - |  357 | `"   $this->__cur = preg_split($this->__re, $subject, -1, $this->__pflags);"` |
|    - |  358 | `"   $ok = is_array($this->__cur) && count($this->__cur) > 1;"` |
|    - |  359 | `"  }elseif( $this->__mode === self::REPLACE ){"` |
|    - |  360 | `"   $n = 0;"` |
|    - |  361 | `"   $this->__cur = preg_replace($this->__re, (string)$this->replacement, $subject, -1, $n);"` |
|    - |  362 | `"   $ok = $n > 0;"` |
|    - |  363 | `"  }"` |
|    - |  364 | `"  if( $this->__rflags & self::INVERT_MATCH ){ $ok = !$ok; }"` |
|    - |  365 | `"  return $ok;"` |
|    - |  366 | `" }"` |
|    - |  367 | `" public function current(){"` |
|    - |  368 | `"  if( $this->__mode === self::MATCH ){ return $this->getInnerIterator()->current(); }"` |
|    - |  369 | `"  return $this->__cur;"` |
|    - |  370 | `" }"` |
|    - |  371 | `" public function getRegex(){ return $this->__re; }"` |
|    - |  372 | `" public function getMode(){ return $this->__mode; }"` |
|    - |  373 | `" public function setMode($mode){ $this->__mode = (int)$mode; }"` |
|    - |  374 | `" public function getFlags(){ return $this->__rflags; }"` |
|    - |  375 | `" public function setFlags($flags){ $this->__rflags = (int)$flags; }"` |
|    - |  376 | `" public function getPregFlags(){ return $this->__pflags; }"` |
|    - |  377 | `" public function setPregFlags($pregFlags){ $this->__pflags = (int)$pregFlags; }"` |
|    - |  378 | `"}"` |
|    - |  379 | `"class AppendIterator implements OuterIterator {"` |
|    - |  380 | `" private $__its = [];"` |
|    - |  381 | `" private $__idx = 0;"` |
|    - |  382 | `" public function __construct(){}"` |
|    - |  383 | `" public function append($iterator){"` |
|    - |  384 | `"  $this->__its[] = $iterator;"` |
|    - |  385 | `"  if( count($this->__its) === 1 ){ $iterator->rewind(); }"` |
|    - |  386 | `" }"` |
|    - |  387 | `" public function getInnerIterator(){ return $this->__its[$this->__idx] ?? null; }"` |
|    - |  388 | `" public function getIteratorIndex(){"` |
|    - |  389 | `"  return isset($this->__its[$this->__idx]) ? $this->__idx : null;"` |
|    - |  390 | `" }"` |
|    - |  391 | `" public function getArrayIterator(){ return new ArrayIterator($this->__its); }"` |
|    - |  392 | `" private function __apAdvance(){"` |
|    - |  393 | `"  while( isset($this->__its[$this->__idx])"` |
|    - |  394 | `"   && !$this->__its[$this->__idx]->valid()"` |
|    - |  395 | `"   && isset($this->__its[$this->__idx + 1]) ){"` |
|    - |  396 | `"   $this->__idx++;"` |
|    - |  397 | `"   $this->__its[$this->__idx]->rewind();"` |
|    - |  398 | `"  }"` |
|    - |  399 | `" }"` |
|    - |  400 | `" public function rewind(){"` |
|    - |  401 | `"  $this->__idx = 0;"` |
|    - |  402 | `"  if( isset($this->__its[0]) ){ $this->__its[0]->rewind(); }"` |
|    - |  403 | `"  $this->__apAdvance();"` |
|    - |  404 | `" }"` |
|    - |  405 | `" public function valid(){"` |
|    - |  406 | `"  $in = $this->getInnerIterator();"` |
|    - |  407 | `"  return $in !== null && $in->valid();"` |
|    - |  408 | `" }"` |
|    - |  409 | `" public function current(){ $in = $this->getInnerIterator(); return $in ? $in->current() : null; }"` |
|    - |  410 | `" public function key(){ $in = $this->getInnerIterator(); return $in ? $in->key() : null; }"` |
|    - |  411 | `" public function next(){"` |
|    - |  412 | `"  $in = $this->getInnerIterator();"` |
|    - |  413 | `"  if( $in ){ $in->next(); }"` |
|    - |  414 | `"  $this->__apAdvance();"` |
|    - |  415 | `" }"` |
|    - |  416 | `"}"` |
|    - |  417 | `"class InfiniteIterator extends IteratorIterator {"` |
|    - |  418 | `" public function next(){"` |
|    - |  419 | `"  $in = $this->getInnerIterator();"` |
|    - |  420 | `"  $in->next();"` |
|    - |  421 | `"  if( !$in->valid() ){ $in->rewind(); }"` |
|    - |  422 | `" }"` |
|    - |  423 | `"}"` |
|    - |  424 | `"class NoRewindIterator extends IteratorIterator {"` |
|    - |  425 | `" public function rewind(){}"` |
|    - |  426 | `"}"` |
|    - |  427 | `"interface RecursiveIterator extends Iterator {"` |
|    - |  428 | `" public function hasChildren();"` |
|    - |  429 | `" public function getChildren();"` |
|    - |  430 | `"}"` |
|    - |  431 | `"class RecursiveArrayIterator extends ArrayIterator implements RecursiveIterator {"` |
|    - |  432 | `" const CHILD_ARRAYS_ONLY = 4;"` |
|    - |  433 | `" public function hasChildren(){"` |
|    - |  434 | `"  $c = $this->current();"` |
|    - |  435 | `"  return is_array($c) \|\| is_object($c);"` |
|    - |  436 | `" }"` |
|    - |  437 | `" public function getChildren(){"` |
|    - |  438 | `"  $c = get_class($this);"` |
|    - |  439 | `"  return new $c($this->current());"` |
|    - |  440 | `" }"` |
|    - |  441 | `"}"` |
|    - |  442 | `"abstract class RecursiveFilterIterator extends FilterIterator implements RecursiveIterator {"` |
|    - |  443 | `" public function __construct(RecursiveIterator $iterator){"` |
|    - |  444 | `"  parent::__construct($iterator);"` |
|    - |  445 | `" }"` |
|    - |  446 | `" public function hasChildren(){"` |
|    - |  447 | `"  return $this->getInnerIterator()->hasChildren();"` |
|    - |  448 | `" }"` |
|    - |  449 | `" public function getChildren(){"` |
|    - |  450 | `"  return new static($this->getInnerIterator()->getChildren());"` |
|    - |  451 | `" }"` |
|    - |  452 | `"}"` |
|    - |  453 | `"class RecursiveIteratorIterator implements OuterIterator {"` |
|    - |  454 | `" const LEAVES_ONLY = 0;"` |
|    - |  455 | `" const SELF_FIRST = 1;"` |
|    - |  456 | `" const CHILD_FIRST = 2;"` |
|    - |  457 | `" const CATCH_GET_CHILD = 16;"` |
|    - |  458 | `" private $__root = null;"` |
|    - |  459 | `" private $__st = [];"` |
|    - |  460 | `" private $__mode = 0;"` |
|    - |  461 | `" private $__maxDepth = false;"` |
|    - |  462 | `" private $__post = false;"` |
|    - |  463 | `" private $__live = false;"` |
|    - |  464 | `" public function __construct($iterator, $mode = 0, $flags = 0){"` |
|    - |  465 | `"  while( $iterator instanceof IteratorAggregate ){ $iterator = $iterator->getIterator(); }"` |
|    - |  466 | `"  if( !($iterator instanceof RecursiveIterator) ){"` |
|    - |  467 | `"   throw new TypeError('RecursiveIteratorIterator::__construct(): Argument #1"` |
|    - |  468 | `" ($iterator) must be of type RecursiveIterator, ' . get_debug_type($iterator) . ' given');"` |
|    - |  469 | `"  }"` |
|    - |  470 | `"  $this->__root = $iterator;"` |
|    - |  471 | `"  $this->__mode = (int)$mode \| (int)$flags;"` |
|    - |  472 | `" }"` |
|    - |  473 | `" public function getInnerIterator(){ return end($this->__st) ?: $this->__root; }"` |
|    - |  474 | `" public function getSubIterator($level = null){"` |
|    - |  475 | `"  if( $level === null ){ $level = count($this->__st) - 1; }"` |
|    - |  476 | `"  return $this->__st[$level] ?? null;"` |
|    - |  477 | `" }"` |
|    - |  478 | `" public function getDepth(){ return count($this->__st) - 1; }"` |
|    - |  479 | `" public function getMaxDepth(){ return $this->__maxDepth; }"` |
|    - |  480 | `" public function setMaxDepth($maxDepth = -1){"` |
|    - |  481 | `"  $maxDepth = (int)$maxDepth;"` |
|    - |  482 | `"  if( $maxDepth < -1 ){"` |
|    - |  483 | `"   throw new Exception('Parameter max_depth must be >= -1');"` |
|    - |  484 | `"  }"` |
|    - |  485 | `"  $this->__maxDepth = $maxDepth === -1 ? false : $maxDepth;"` |
|    - |  486 | `" }"` |
|    - |  487 | `" public function callHasChildren(){"` |
|    - |  488 | `"  $it = end($this->__st);"` |
|    - |  489 | `"  return $it ? $it->hasChildren() : false;"` |
|    - |  490 | `" }"` |
|    - |  491 | `" public function callGetChildren(){"` |
|    - |  492 | `"  $it = end($this->__st);"` |
|    - |  493 | `"  return $it ? $it->getChildren() : null;"` |
|    - |  494 | `" }"` |
|    - |  495 | `" public function beginIteration(){}"` |
|    - |  496 | `" public function endIteration(){}"` |
|    - |  497 | `" public function beginChildren(){}"` |
|    - |  498 | `" public function endChildren(){}"` |
|    - |  499 | `" public function nextElement(){}"` |
|    - |  500 | `" private function __riDepthOk(){"` |
|    - |  501 | `"  return $this->__maxDepth === false \|\| (count($this->__st) - 1) < $this->__maxDepth;"` |
|    - |  502 | `" }"` |
|    - |  503 | `" private function __riDescend(){"` |
|    - |  504 | `"  /* push the current element's children, positioned at their start */"` |
|    - |  505 | `"  if( $this->__mode & self::CATCH_GET_CHILD ){"` |
|    - |  506 | `"   try { $child = $this->callGetChildren(); }"` |
|    - |  507 | `"   catch (Exception $e) { return false; }"` |
|    - |  508 | `"  }else{"` |
|    - |  509 | `"   $child = $this->callGetChildren();"` |
|    - |  510 | `"  }"` |
|    - |  511 | `"  if( !($child instanceof RecursiveIterator) ){ return false; }"` |
|    - |  512 | `"  $child->rewind();"` |
|    - |  513 | `"  $this->__st[] = $child;"` |
|    - |  514 | `"  $this->beginChildren();"` |
|    - |  515 | `"  return true;"` |
|    - |  516 | `" }"` |
|    - |  517 | `" private function __riFetch(){"` |
|    - |  518 | `"  $m = $this->__mode & 3;"` |
|    - |  519 | `"  for(;;){"` |
|    - |  520 | `"   if( count($this->__st) === 0 ){"` |
|    - |  521 | `"    $this->__live = false;"` |
|    - |  522 | `"    /* php keeps the root level addressable after exhaustion (getDepth 0,"` |
|    - |  523 | `"     * getSubIterator() returns the root) */"` |
|    - |  524 | `"    $this->__st = [$this->__root];"` |
|    - |  525 | `"    $this->endIteration();"` |
|    - |  526 | `"    return;"` |
|    - |  527 | `"   }"` |
|    - |  528 | `"   $it = end($this->__st);"` |
|    - |  529 | `"   if( !$it->valid() ){"` |
|    - |  530 | `"    array_pop($this->__st);"` |
|    - |  531 | `"    $this->endChildren();"` |
|    - |  532 | `"    if( count($this->__st) === 0 ){ continue; }"` |
|    - |  533 | `"    if( $m === self::CHILD_FIRST ){"` |
|    - |  534 | `"     /* the parent node yields now, after its subtree */"` |
|    - |  535 | `"     $this->__post = true;"` |
|    - |  536 | `"     $this->__live = true;"` |
|    - |  537 | `"     return;"` |
|    - |  538 | `"    }"` |
|    - |  539 | `"    end($this->__st)->next();"` |
|    - |  540 | `"    continue;"` |
|    - |  541 | `"   }"` |
|    - |  542 | `"   if( $m === self::LEAVES_ONLY && $it->hasChildren() && $this->__riDepthOk() ){"` |
|    - |  543 | `"    if( $this->__riDescend() ){ continue; }"` |
|    - |  544 | `"   }"` |
|    - |  545 | `"   if( $m === self::CHILD_FIRST && $it->hasChildren() && $this->__riDepthOk() ){"` |
|    - |  546 | `"    if( $this->__riDescend() ){ continue; }"` |
|    - |  547 | `"   }"` |
|    - |  548 | `"   $this->__post = false;"` |
|    - |  549 | `"   $this->__live = true;"` |
|    - |  550 | `"   $this->nextElement();"` |
|    - |  551 | `"   return;"` |
|    - |  552 | `"  }"` |
|    - |  553 | `" }"` |
|    - |  554 | `" public function rewind(){"` |
|    - |  555 | `"  $this->__st = [$this->__root];"` |
|    - |  556 | `"  $this->__root->rewind();"` |
|    - |  557 | `"  $this->__post = false;"` |
|    - |  558 | `"  $this->beginIteration();"` |
|    - |  559 | `"  $this->__riFetch();"` |
|    - |  560 | `" }"` |
|    - |  561 | `" public function valid(){ return $this->__live; }"` |
|    - |  562 | `" public function current(){"` |
|    - |  563 | `"  $it = end($this->__st);"` |
|    - |  564 | `"  return $it ? $it->current() : null;"` |
|    - |  565 | `" }"` |
|    - |  566 | `" public function key(){"` |
|    - |  567 | `"  $it = end($this->__st);"` |
|    - |  568 | `"  return $it ? $it->key() : null;"` |
|    - |  569 | `" }"` |
|    - |  570 | `" public function next(){"` |
|    - |  571 | `"  if( !$this->__live ){ return; }"` |
|    - |  572 | `"  $m = $this->__mode & 3;"` |
|    - |  573 | `"  $it = end($this->__st);"` |
|    - |  574 | `"  if( $this->__post ){"` |
|    - |  575 | `"   /* leaving a CHILD_FIRST post-visit: advance past the node */"` |
|    - |  576 | `"   $this->__post = false;"` |
|    - |  577 | `"   $it->next();"` |
|    - |  578 | `"   $this->__riFetch();"` |
|    - |  579 | `"   return;"` |
|    - |  580 | `"  }"` |
|    - |  581 | `"  if( $m === self::SELF_FIRST && $it->hasChildren() && $this->__riDepthOk() ){"` |
|    - |  582 | `"   if( $this->__riDescend() ){ $this->__riFetch(); return; }"` |
|    - |  583 | `"  }"` |
|    - |  584 | `"  $it->next();"` |
|    - |  585 | `"  $this->__riFetch();"` |
|    - |  586 | `" }"` |
|    - |  587 | `"}"` |
|    - |  588 | `"class WeakReference {"` |
|    - |  589 | `" private $__h = 0;"` |
|    - |  590 | `" private function __construct(){}"` |
|    - |  591 | `" public static function create($object){"` |
|    - |  592 | `"  if( !is_object($object) ){"` |
|    - |  593 | `"   throw new TypeError('WeakReference::create(): Argument #1 ($object) must be"` |
|    - |  594 | `" of type object, ' . get_debug_type($object) . ' given');"` |
|    - |  595 | `"  }"` |
|    - |  596 | `"  $w = new WeakReference();"` |
|    - |  597 | `"  $w->__h = __weak_create($object);"` |
|    - |  598 | `"  return $w;"` |
|    - |  599 | `" }"` |
|    - |  600 | `" public function get(){ return $this->__h ? __weak_get($this->__h) : null; }"` |
|    - |  601 | `" public function __destruct(){"` |
|    - |  602 | `"  if( $this->__h ){ __weak_drop($this->__h); $this->__h = 0; }"` |
|    - |  603 | `" }"` |
|    - |  604 | `"}"` |
|    - |  605 | `"class WeakMap implements ArrayAccess, Countable, IteratorAggregate {"` |
|    - |  606 | `" private $__e = [];"` |
|    - |  607 | `" private function __wmPrune(){"` |
|    - |  608 | `"  foreach( $this->__e as $id => $p ){"` |
|    - |  609 | `"   if( __weak_get($p[0]) === null ){"` |
|    - |  610 | `"    __weak_drop($p[0]);"` |
|    - |  611 | `"    unset($this->__e[$id]);"` |
|    - |  612 | `"   }"` |
|    - |  613 | `"  }"` |
|    - |  614 | `" }"` |
|    - |  615 | `" public function offsetSet($object, $value){"` |
|    - |  616 | `"  if( !is_object($object) ){"` |
|    - |  617 | `"   throw new TypeError('WeakMap key must be an object');"` |
|    - |  618 | `"  }"` |
|    - |  619 | `"  $id = spl_object_id($object);"` |
|    - |  620 | `"  if( isset($this->__e[$id]) && __weak_get($this->__e[$id][0]) !== null ){"` |
|    - |  621 | `"   $this->__e[$id][1] = $value;"` |
|    - |  622 | `"   return;"` |
|    - |  623 | `"  }"` |
|    - |  624 | `"  if( isset($this->__e[$id]) ){ __weak_drop($this->__e[$id][0]); }"` |
|    - |  625 | `"  $this->__e[$id] = [__weak_create($object), $value];"` |
|    - |  626 | `" }"` |
|    - |  627 | `" public function offsetGet($object){"` |
|    - |  628 | `"  if( !is_object($object) ){"` |
|    - |  629 | `"   throw new TypeError('WeakMap key must be an object');"` |
|    - |  630 | `"  }"` |
|    - |  631 | `"  $id = spl_object_id($object);"` |
|    - |  632 | `"  if( isset($this->__e[$id]) && __weak_get($this->__e[$id][0]) === $object ){"` |
|    - |  633 | `"   return $this->__e[$id][1];"` |
|    - |  634 | `"  }"` |
|    - |  635 | `"  throw new Error('Object ' . get_class($object) . '#' . $id . ' not contained"` |
|    - |  636 | `" in WeakMap');"` |
|    - |  637 | `" }"` |
|    - |  638 | `" public function offsetExists($object){"` |
|    - |  639 | `"  if( !is_object($object) ){"` |
|    - |  640 | `"   throw new TypeError('WeakMap key must be an object');"` |
|    - |  641 | `"  }"` |
|    - |  642 | `"  $id = spl_object_id($object);"` |
|    - |  643 | `"  return isset($this->__e[$id]) && __weak_get($this->__e[$id][0]) === $object;"` |
|    - |  644 | `" }"` |
|    - |  645 | `" public function offsetUnset($object){"` |
|    - |  646 | `"  if( !is_object($object) ){"` |
|    - |  647 | `"   throw new TypeError('WeakMap key must be an object');"` |
|    - |  648 | `"  }"` |
|    - |  649 | `"  $id = spl_object_id($object);"` |
|    - |  650 | `"  if( isset($this->__e[$id]) ){"` |
|    - |  651 | `"   __weak_drop($this->__e[$id][0]);"` |
|    - |  652 | `"   unset($this->__e[$id]);"` |
|    - |  653 | `"  }"` |
|    - |  654 | `" }"` |
|    - |  655 | `" public function count(){"` |
|    - |  656 | `"  $this->__wmPrune();"` |
|    - |  657 | `"  return count($this->__e);"` |
|    - |  658 | `" }"` |
|    - |  659 | `" public function getIterator(): Generator {"` |
|    - |  660 | `"  $this->__wmPrune();"` |
|    - |  661 | `"  foreach( $this->__e as $p ){"` |
|    - |  662 | `"   $o = __weak_get($p[0]);"` |
|    - |  663 | `"   if( $o !== null ){ yield $o => $p[1]; }"` |
|    - |  664 | `"  }"` |
|    - |  665 | `" }"` |
|    - |  666 | `" public function __destruct(){"` |
|    - |  667 | `"  foreach( $this->__e as $p ){ __weak_drop($p[0]); }"` |
|    - |  668 | `"  $this->__e = [];"` |
|    - |  669 | `" }"` |
|    - |  670 | `"}"` |
|    - |  671 | `"class EmptyIterator implements Iterator {"` |
|    - |  672 | `" public function current(){"` |
|    - |  673 | `"  throw new BadMethodCallException('Accessing the value of an EmptyIterator');"` |
|    - |  674 | `" }"` |
|    - |  675 | `" public function key(){"` |
|    - |  676 | `"  throw new BadMethodCallException('Accessing the key of an EmptyIterator');"` |
|    - |  677 | `" }"` |
|    - |  678 | `" public function next(){}"` |
|    - |  679 | `" public function rewind(){}"` |
|    - |  680 | `" public function valid(){ return false; }"` |
|    - |  681 | `"}"` |
|    - |  682 | `"class SplDoublyLinkedList implements Iterator, Countable, ArrayAccess {"` |
|    - |  683 | `" const IT_MODE_LIFO = 2;"` |
|    - |  684 | `" const IT_MODE_FIFO = 0;"` |
|    - |  685 | `" const IT_MODE_DELETE = 1;"` |
|    - |  686 | `" const IT_MODE_KEEP = 0;"` |
|    - |  687 | `" private $__q = [];"` |
|    - |  688 | `" private $__mode = 0;"` |
|    - |  689 | `" private $__i = 0;"` |
|    - |  690 | `" public function __construct(){"` |
|    - |  691 | `"  if( $this instanceof SplStack ){ $this->__mode = 2; }"` |
|    - |  692 | `" }"` |
|    - |  693 | `" public function setIteratorMode($mode){"` |
|    - |  694 | `"  $mode = (int)$mode;"` |
|    - |  695 | `"  if( ($this instanceof SplStack \|\| $this instanceof SplQueue)"` |
|    - |  696 | `"   && ($mode & 2) !== ($this->__mode & 2) ){"` |
|    - |  697 | `"   throw new RuntimeException(\"Iterators' LIFO/FIFO modes for SplStack/SplQueue"` |
|    - |  698 | `" objects are frozen\");"` |
|    - |  699 | `"  }"` |
|    - |  700 | `"  $this->__mode = $mode;"` |
|    - |  701 | `" }"` |
|    - |  702 | `" public function getIteratorMode(){ return $this->__mode; }"` |
|    - |  703 | `" public function push($value){ $this->__q[] = $value; }"` |
|    - |  704 | `" public function pop(){"` |
|    - |  705 | `"  if( count($this->__q) === 0 ){"` |
|    - |  706 | `"   throw new RuntimeException(\"Can't pop from an empty datastructure\");"` |
|    - |  707 | `"  }"` |
|    - |  708 | `"  return array_pop($this->__q);"` |
|    - |  709 | `" }"` |
|    - |  710 | `" public function shift(){"` |
|    - |  711 | `"  if( count($this->__q) === 0 ){"` |
|    - |  712 | `"   throw new RuntimeException(\"Can't shift from an empty datastructure\");"` |
|    - |  713 | `"  }"` |
|    - |  714 | `"  return array_shift($this->__q);"` |
|    - |  715 | `" }"` |
|    - |  716 | `" public function unshift($value){ array_unshift($this->__q, $value); }"` |
|    - |  717 | `" public function top(){"` |
|    - |  718 | `"  if( count($this->__q) === 0 ){"` |
|    - |  719 | `"   throw new RuntimeException(\"Can't peek at an empty datastructure\");"` |
|    - |  720 | `"  }"` |
|    - |  721 | `"  return $this->__q[count($this->__q) - 1];"` |
|    - |  722 | `" }"` |
|    - |  723 | `" public function bottom(){"` |
|    - |  724 | `"  if( count($this->__q) === 0 ){"` |
|    - |  725 | `"   throw new RuntimeException(\"Can't peek at an empty datastructure\");"` |
|    - |  726 | `"  }"` |
|    - |  727 | `"  return $this->__q[0];"` |
|    - |  728 | `" }"` |
|    - |  729 | `" public function isEmpty(){ return count($this->__q) === 0; }"` |
|    - |  730 | `" public function count(){ return count($this->__q); }"` |
|    - |  731 | `" public function toArray(){ return $this->__q; }"` |
|    - |  732 | `" public function add($index, $value){"` |
|    - |  733 | `"  $index = (int)$index;"` |
|    - |  734 | `"  if( $index < 0 \|\| $index > count($this->__q) ){"` |
|    - |  735 | `"   throw new OutOfRangeException(get_class($this) === 'SplDoublyLinkedList'"` |
|    - |  736 | `"    ? 'SplDoublyLinkedList::add(): Argument #1 ($index) is out of range'"` |
|    - |  737 | `"    : get_class($this) . '::add(): Argument #1 ($index) is out of range');"` |
|    - |  738 | `"  }"` |
|    - |  739 | `"  array_splice($this->__q, $index, 0, [$value]);"` |
|    - |  740 | `" }"` |
|    - |  741 | `" public function offsetExists($index){"` |
|    - |  742 | `"  return is_int($index) \|\| ctype_digit((string)$index)"` |
|    - |  743 | `"   ? ((int)$index >= 0 && (int)$index < count($this->__q)) : false;"` |
|    - |  744 | `" }"` |
|    - |  745 | `" public function offsetGet($index){"` |
|    - |  746 | `"  $index = (int)$index;"` |
|    - |  747 | `"  if( $index < 0 \|\| $index >= count($this->__q) ){"` |
|    - |  748 | `"   throw new OutOfRangeException('SplDoublyLinkedList::offsetGet(): Argument #1"` |
|    - |  749 | `" ($index) is out of range');"` |
|    - |  750 | `"  }"` |
|    - |  751 | `"  return $this->__q[$index];"` |
|    - |  752 | `" }"` |
|    - |  753 | `" public function offsetSet($index, $value){"` |
|    - |  754 | `"  if( $index === null ){ $this->__q[] = $value; return; }"` |
|    - |  755 | `"  $index = (int)$index;"` |
|    - |  756 | `"  if( $index < 0 \|\| $index >= count($this->__q) ){"` |
|    - |  757 | `"   throw new OutOfRangeException('SplDoublyLinkedList::offsetSet(): Argument #1"` |
|    - |  758 | `" ($index) is out of range');"` |
|    - |  759 | `"  }"` |
|    - |  760 | `"  $this->__q[$index] = $value;"` |
|    - |  761 | `" }"` |
|    - |  762 | `" public function offsetUnset($index){"` |
|    - |  763 | `"  $index = (int)$index;"` |
|    - |  764 | `"  if( $index < 0 \|\| $index >= count($this->__q) ){"` |
|    - |  765 | `"   throw new OutOfRangeException('SplDoublyLinkedList::offsetUnset(): Argument #1"` |
|    - |  766 | `" ($index) is out of range');"` |
|    - |  767 | `"  }"` |
|    - |  768 | `"  array_splice($this->__q, $index, 1);"` |
|    - |  769 | `" }"` |
|    - |  770 | `" public function rewind(){"` |
|    - |  771 | `"  $this->__i = ($this->__mode & 2) ? count($this->__q) - 1 : 0;"` |
|    - |  772 | `" }"` |
|    - |  773 | `" public function valid(){"` |
|    - |  774 | `"  return $this->__i >= 0 && $this->__i < count($this->__q);"` |
|    - |  775 | `" }"` |
|    - |  776 | `" public function current(){ return $this->__q[$this->__i] ?? null; }"` |
|    - |  777 | `" public function key(){ return $this->__i; }"` |
|    - |  778 | `" public function next(){"` |
|    - |  779 | `"  if( $this->__mode & 1 ){"` |
|    - |  780 | `"   /* IT_MODE_DELETE consumes the element just visited */"` |
|    - |  781 | `"   if( $this->__mode & 2 ){ array_pop($this->__q); $this->__i = count($this->__q) - 1; }"` |
|    - |  782 | `"   else { array_shift($this->__q); }"` |
|    - |  783 | `"  }else{"` |
|    - |  784 | `"   $this->__i += ($this->__mode & 2) ? -1 : 1;"` |
|    - |  785 | `"  }"` |
|    - |  786 | `" }"` |
|    - |  787 | `" public function prev(){ $this->__i += ($this->__mode & 2) ? 1 : -1; }"` |
|    - |  788 | `"}"` |
|    - |  789 | `"class SplStack extends SplDoublyLinkedList {}"` |
|    - |  790 | `"class SplQueue extends SplDoublyLinkedList {"` |
|    - |  791 | `" public function enqueue($value){ $this->push($value); }"` |
|    - |  792 | `" public function dequeue(){ return $this->shift(); }"` |
|    - |  793 | `"}"` |
|    - |  794 | `"abstract class SplHeap implements Iterator, Countable {"` |
|    - |  795 | `" private $__h = [];"` |
|    - |  796 | `" abstract protected function compare($value1, $value2);"` |
|    - |  797 | `" private function __hSiftUp($i){"` |
|    - |  798 | `"  while( $i > 0 ){"` |
|    - |  799 | `"   $p = ($i - 1) >> 1;"` |
|    - |  800 | `"   if( $this->compare($this->__h[$i], $this->__h[$p]) <= 0 ){ break; }"` |
|    - |  801 | `"   $t = $this->__h[$p]; $this->__h[$p] = $this->__h[$i]; $this->__h[$i] = $t;"` |
|    - |  802 | `"   $i = $p;"` |
|    - |  803 | `"  }"` |
|    - |  804 | `" }"` |
|    - |  805 | `" private function __hSiftDown($i){"` |
|    - |  806 | `"  $n = count($this->__h);"` |
|    - |  807 | `"  for(;;){"` |
|    - |  808 | `"   $l = 2 * $i + 1; $r = $l + 1; $b = $i;"` |
|    - |  809 | `"   if( $l < $n && $this->compare($this->__h[$l], $this->__h[$b]) > 0 ){ $b = $l; }"` |
|    - |  810 | `"   if( $r < $n && $this->compare($this->__h[$r], $this->__h[$b]) > 0 ){ $b = $r; }"` |
|    - |  811 | `"   if( $b === $i ){ break; }"` |
|    - |  812 | `"   $t = $this->__h[$b]; $this->__h[$b] = $this->__h[$i]; $this->__h[$i] = $t;"` |
|    - |  813 | `"   $i = $b;"` |
|    - |  814 | `"  }"` |
|    - |  815 | `" }"` |
|    - |  816 | `" public function insert($value){"` |
|    - |  817 | `"  $this->__h[] = $value;"` |
|    - |  818 | `"  $this->__hSiftUp(count($this->__h) - 1);"` |
|    - |  819 | `"  return true;"` |
|    - |  820 | `" }"` |
|    - |  821 | `" public function extract(){"` |
|    - |  822 | `"  $n = count($this->__h);"` |
|    - |  823 | `"  if( $n === 0 ){"` |
|    - |  824 | `"   throw new RuntimeException(\"Can't extract from an empty heap\");"` |
|    - |  825 | `"  }"` |
|    - |  826 | `"  $top = $this->__h[0];"` |
|    - |  827 | `"  $last = array_pop($this->__h);"` |
|    - |  828 | `"  if( $n > 1 ){"` |
|    - |  829 | `"   $this->__h[0] = $last;"` |
|    - |  830 | `"   $this->__hSiftDown(0);"` |
|    - |  831 | `"  }"` |
|    - |  832 | `"  return $top;"` |
|    - |  833 | `" }"` |
|    - |  834 | `" public function top(){"` |
|    - |  835 | `"  if( count($this->__h) === 0 ){"` |
|    - |  836 | `"   throw new RuntimeException(\"Can't peek at an empty heap\");"` |
|    - |  837 | `"  }"` |
|    - |  838 | `"  return $this->__h[0];"` |
|    - |  839 | `" }"` |
|    - |  840 | `" public function isEmpty(){ return count($this->__h) === 0; }"` |
|    - |  841 | `" public function count(){ return count($this->__h); }"` |
|    - |  842 | `" public function isCorrupted(){ return false; }"` |
|    - |  843 | `" public function recoverFromCorruption(){ return true; }"` |
|    - |  844 | `" public function rewind(){}"` |
|    - |  845 | `" public function valid(){ return count($this->__h) > 0; }"` |
|    - |  846 | `" public function current(){ return count($this->__h) ? $this->__h[0] : null; }"` |
|    - |  847 | `" public function key(){ return count($this->__h) - 1; }"` |
|    - |  848 | `" public function next(){ if( count($this->__h) ){ $this->extract(); } }"` |
|    - |  849 | `"}"` |
|    - |  850 | `"class SplMinHeap extends SplHeap {"` |
|    - |  851 | `" protected function compare($value1, $value2){ return $value2 <=> $value1; }"` |
|    - |  852 | `"}"` |
|    - |  853 | `"class SplMaxHeap extends SplHeap {"` |
|    - |  854 | `" protected function compare($value1, $value2){ return $value1 <=> $value2; }"` |
|    - |  855 | `"}"` |
|    - |  856 | `"class SplPriorityQueue implements Iterator, Countable {"` |
|    - |  857 | `" const EXTR_DATA = 1;"` |
|    - |  858 | `" const EXTR_PRIORITY = 2;"` |
|    - |  859 | `" const EXTR_BOTH = 3;"` |
|    - |  860 | `" private $__h = [];"` |
|    - |  861 | `" private $__serial = PHP_INT_MAX;"` |
|    - |  862 | `" private $__flags = 1;"` |
|    - |  863 | `" public function compare($priority1, $priority2){ return $priority1 <=> $priority2; }"` |
|    - |  864 | `" private function __pqCmp($a, $b){"` |
|    - |  865 | `"  /* NO tie-break: php's heap swaps only on strictly-greater, which fixes"` |
|    - |  866 | `"   * the (documented-as-undefined) equal-priority order it exhibits */"` |
|    - |  867 | `"  return $this->compare($a[0], $b[0]);"` |
|    - |  868 | `" }"` |
|    - |  869 | `" private function __pqSiftUp($i){"` |
|    - |  870 | `"  while( $i > 0 ){"` |
|    - |  871 | `"   $p = ($i - 1) >> 1;"` |
|    - |  872 | `"   if( $this->__pqCmp($this->__h[$i], $this->__h[$p]) <= 0 ){ break; }"` |
|    - |  873 | `"   $t = $this->__h[$p]; $this->__h[$p] = $this->__h[$i]; $this->__h[$i] = $t;"` |
|    - |  874 | `"   $i = $p;"` |
|    - |  875 | `"  }"` |
|    - |  876 | `" }"` |
|    - |  877 | `" private function __pqSiftDown($i){"` |
|    - |  878 | `"  $n = count($this->__h);"` |
|    - |  879 | `"  for(;;){"` |
|    - |  880 | `"   $l = 2 * $i + 1; $r = $l + 1; $b = $i;"` |
|    - |  881 | `"   if( $l < $n && $this->__pqCmp($this->__h[$l], $this->__h[$b]) > 0 ){ $b = $l; }"` |
|    - |  882 | `"   if( $r < $n && $this->__pqCmp($this->__h[$r], $this->__h[$b]) > 0 ){ $b = $r; }"` |
|    - |  883 | `"   if( $b === $i ){ break; }"` |
|    - |  884 | `"   $t = $this->__h[$b]; $this->__h[$b] = $this->__h[$i]; $this->__h[$i] = $t;"` |
|    - |  885 | `"   $i = $b;"` |
|    - |  886 | `"  }"` |
|    - |  887 | `" }"` |
|    - |  888 | `" public function insert($value, $priority){"` |
|    - |  889 | `"  $this->__h[] = [$priority, $this->__serial--, $value];"` |
|    - |  890 | `"  $this->__pqSiftUp(count($this->__h) - 1);"` |
|    - |  891 | `"  return true;"` |
|    - |  892 | `" }"` |
|    - |  893 | `" private function __pqShape($node){"` |
|    - |  894 | `"  if( $this->__flags === self::EXTR_BOTH ){"` |
|    - |  895 | `"   return ['data' => $node[2], 'priority' => $node[0]];"` |
|    - |  896 | `"  }"` |
|    - |  897 | `"  if( $this->__flags === self::EXTR_PRIORITY ){ return $node[0]; }"` |
|    - |  898 | `"  return $node[2];"` |
|    - |  899 | `" }"` |
|    - |  900 | `" public function extract(){"` |
|    - |  901 | `"  $n = count($this->__h);"` |
|    - |  902 | `"  if( $n === 0 ){"` |
|    - |  903 | `"   throw new RuntimeException(\"Can't extract from an empty heap\");"` |
|    - |  904 | `"  }"` |
|    - |  905 | `"  $top = $this->__h[0];"` |
|    - |  906 | `"  $last = array_pop($this->__h);"` |
|    - |  907 | `"  if( $n > 1 ){"` |
|    - |  908 | `"   $this->__h[0] = $last;"` |
|    - |  909 | `"   $this->__pqSiftDown(0);"` |
|    - |  910 | `"  }"` |
|    - |  911 | `"  return $this->__pqShape($top);"` |
|    - |  912 | `" }"` |
|    - |  913 | `" public function top(){"` |
|    - |  914 | `"  if( count($this->__h) === 0 ){"` |
|    - |  915 | `"   throw new RuntimeException(\"Can't peek at an empty heap\");"` |
|    - |  916 | `"  }"` |
|    - |  917 | `"  return $this->__pqShape($this->__h[0]);"` |
|    - |  918 | `" }"` |
|    - |  919 | `" public function setExtractFlags($flags){ $this->__flags = (int)$flags; }"` |
|    - |  920 | `" public function getExtractFlags(){ return $this->__flags; }"` |
|    - |  921 | `" public function isEmpty(){ return count($this->__h) === 0; }"` |
|    - |  922 | `" public function count(){ return count($this->__h); }"` |
|    - |  923 | `" public function isCorrupted(){ return false; }"` |
|    - |  924 | `" public function recoverFromCorruption(){ return true; }"` |
|    - |  925 | `" public function rewind(){}"` |
|    - |  926 | `" public function valid(){ return count($this->__h) > 0; }"` |
|    - |  927 | `" public function current(){ return count($this->__h) ? $this->__pqShape($this->__h[0]) : null; }"` |
|    - |  928 | `" public function key(){ return count($this->__h) - 1; }"` |
|    - |  929 | `" public function next(){ if( count($this->__h) ){ $this->extract(); } }"` |
|    - |  930 | `"}"` |
|    - |  931 | `"class SplFixedArray implements ArrayAccess, Countable, IteratorAggregate, JsonSerializable {"` |
|    - |  932 | `" private $__a = [];"` |
|    - |  933 | `" private $__n = 0;"` |
|    - |  934 | `" public function __construct($size = 0){"` |
|    - |  935 | `"  $this->setSize((int)$size);"` |
|    - |  936 | `" }"` |
|    - |  937 | `" private function __faIdx($index, $method){"` |
|    - |  938 | `"  if( !is_int($index) ){"` |
|    - |  939 | `"   if( is_string($index) && ctype_digit($index) ){"` |
|    - |  940 | `"    $index = (int)$index;"` |
|    - |  941 | `"   }else{"` |
|    - |  942 | `"    throw new TypeError('Cannot access offset of type ' . get_debug_type($index)"` |
|    - |  943 | `"     . ' on SplFixedArray');"` |
|    - |  944 | `"   }"` |
|    - |  945 | `"  }"` |
|    - |  946 | `"  if( $index < 0 \|\| $index >= $this->__n ){"` |
|    - |  947 | `"   throw new OutOfBoundsException('Index invalid or out of range');"` |
|    - |  948 | `"  }"` |
|    - |  949 | `"  return $index;"` |
|    - |  950 | `" }"` |
|    - |  951 | `" public function offsetExists($index){"` |
|    - |  952 | `"  if( !is_int($index) && !(is_string($index) && ctype_digit($index)) ){ return false; }"` |
|    - |  953 | `"  $index = (int)$index;"` |
|    - |  954 | `"  return $index >= 0 && $index < $this->__n && $this->__a[$index] !== null;"` |
|    - |  955 | `" }"` |
|    - |  956 | `" public function offsetGet($index){ return $this->__a[$this->__faIdx($index, 'offsetGet')]; }"` |
|    - |  957 | `" public function offsetSet($index, $value){ $this->__a[$this->__faIdx($index, 'offsetSet')] = $value; }"` |
|    - |  958 | `" public function offsetUnset($index){ $this->__a[$this->__faIdx($index, 'offsetUnset')] = null; }"` |
|    - |  959 | `" public function getSize(){ return $this->__n; }"` |
|    - |  960 | `" public function setSize($size){"` |
|    - |  961 | `"  $size = (int)$size;"` |
|    - |  962 | `"  if( $size < 0 ){"` |
|    - |  963 | `"   throw new ValueError('SplFixedArray::setSize(): Argument #1 ($size) must be"` |
|    - |  964 | `" greater than or equal to 0');"` |
|    - |  965 | `"  }"` |
|    - |  966 | `"  if( $size < $this->__n ){"` |
|    - |  967 | `"   $this->__a = array_slice($this->__a, 0, $size);"` |
|    - |  968 | `"  }else{"` |
|    - |  969 | `"   for( $i = $this->__n; $i < $size; $i++ ){ $this->__a[$i] = null; }"` |
|    - |  970 | `"  }"` |
|    - |  971 | `"  $this->__n = $size;"` |
|    - |  972 | `"  return true;"` |
|    - |  973 | `" }"` |
|    - |  974 | `" public function count(){ return $this->__n; }"` |
|    - |  975 | `" public function toArray(){ return $this->__a; }"` |
|    - |  976 | `" public static function fromArray($array, $preserveKeys = true){"` |
|    - |  977 | `"  $f = new SplFixedArray(0);"` |
|    - |  978 | `"  if( $preserveKeys ){"` |
|    - |  979 | `"   $max = -1;"` |
|    - |  980 | `"   foreach( $array as $k => $v ){"` |
|    - |  981 | `"    if( !is_int($k) \|\| $k < 0 ){"` |
|    - |  982 | `"     throw new InvalidArgumentException('array must contain only positive integer keys');"` |
|    - |  983 | `"    }"` |
|    - |  984 | `"    if( $k > $max ){ $max = $k; }"` |
|    - |  985 | `"   }"` |
|    - |  986 | `"   $f->setSize($max + 1);"` |
|    - |  987 | `"   foreach( $array as $k => $v ){ $f[$k] = $v; }"` |
|    - |  988 | `"  }else{"` |
|    - |  989 | `"   $vals = array_values($array);"` |
|    - |  990 | `"   $f->setSize(count($vals));"` |
|    - |  991 | `"   foreach( $vals as $k => $v ){ $f[$k] = $v; }"` |
|    - |  992 | `"  }"` |
|    - |  993 | `"  return $f;"` |
|    - |  994 | `" }"` |
|    - |  995 | `" public function getIterator(): Generator {"` |
|    - |  996 | `"  for( $i = 0; $i < $this->__n; $i++ ){ yield $i => $this->__a[$i]; }"` |
|    - |  997 | `" }"` |
|    - |  998 | `" public function jsonSerialize(){ return $this->__a; }"` |
|    - |  999 | `"}"` |
|    - | 1000 | `"class SplObjectStorage implements Countable, Iterator, ArrayAccess {"` |
|    - | 1001 | `" private $__o = [];"` |
|    - | 1002 | `" private $__i = 0;"` |
|    - | 1003 | `" public function attach($object, $info = null){"` |
|    - | 1004 | `"  __spl_deprecated('Method SplObjectStorage::attach() is deprecated since 8.5, use"` |
|    - | 1005 | `" method SplObjectStorage::offsetSet() instead');"` |
|    - | 1006 | `"  $this->offsetSet($object, $info);"` |
|    - | 1007 | `" }"` |
|    - | 1008 | `" public function detach($object){"` |
|    - | 1009 | `"  __spl_deprecated('Method SplObjectStorage::detach() is deprecated since 8.5, use"` |
|    - | 1010 | `" method SplObjectStorage::offsetUnset() instead');"` |
|    - | 1011 | `"  $this->offsetUnset($object);"` |
|    - | 1012 | `" }"` |
|    - | 1013 | `" public function contains($object){"` |
|    - | 1014 | `"  __spl_deprecated('Method SplObjectStorage::contains() is deprecated since 8.5, use"` |
|    - | 1015 | `" method SplObjectStorage::offsetExists() instead');"` |
|    - | 1016 | `"  return $this->offsetExists($object);"` |
|    - | 1017 | `" }"` |
|    - | 1018 | `" public function offsetSet($object, $info = null){"` |
|    - | 1019 | `"  $this->__o[spl_object_id($object)] = [$object, $info];"` |
|    - | 1020 | `" }"` |
|    - | 1021 | `" public function offsetExists($object){"` |
|    - | 1022 | `"  return isset($this->__o[spl_object_id($object)]);"` |
|    - | 1023 | `" }"` |
|    - | 1024 | `" public function offsetGet($object){"` |
|    - | 1025 | `"  $id = spl_object_id($object);"` |
|    - | 1026 | `"  if( !isset($this->__o[$id]) ){"` |
|    - | 1027 | `"   throw new UnexpectedValueException('Object not found');"` |
|    - | 1028 | `"  }"` |
|    - | 1029 | `"  return $this->__o[$id][1];"` |
|    - | 1030 | `" }"` |
|    - | 1031 | `" public function offsetUnset($object){"` |
|    - | 1032 | `"  unset($this->__o[spl_object_id($object)]);"` |
|    - | 1033 | `" }"` |
|    - | 1034 | `" public function addAll($storage){"` |
|    - | 1035 | `"  foreach( $storage as $obj ){"` |
|    - | 1036 | `"   $this->offsetSet($obj, $storage[$obj]);"` |
|    - | 1037 | `"  }"` |
|    - | 1038 | `"  return $this->count();"` |
|    - | 1039 | `" }"` |
|    - | 1040 | `" public function removeAll($storage){"` |
|    - | 1041 | `"  foreach( $storage as $obj ){ $this->offsetUnset($obj); }"` |
|    - | 1042 | `"  return $this->count();"` |
|    - | 1043 | `" }"` |
|    - | 1044 | `" public function removeAllExcept($storage){"` |
|    - | 1045 | `"  foreach( $this->__o as $id => $pair ){"` |
|    - | 1046 | `"   if( !$storage->offsetExists($pair[0]) ){ unset($this->__o[$id]); }"` |
|    - | 1047 | `"  }"` |
|    - | 1048 | `"  return $this->count();"` |
|    - | 1049 | `" }"` |
|    - | 1050 | `" public function getHash($object){ return spl_object_hash($object); }"` |
|    - | 1051 | `" public function count($mode = 0){ return count($this->__o); }"` |
|    - | 1052 | `" public function getInfo(){"` |
|    - | 1053 | `"  $pair = array_values($this->__o)[$this->__i] ?? null;"` |
|    - | 1054 | `"  return $pair === null ? null : $pair[1];"` |
|    - | 1055 | `" }"` |
|    - | 1056 | `" public function setInfo($info){"` |
|    - | 1057 | `"  $keys = array_keys($this->__o);"` |
|    - | 1058 | `"  if( isset($keys[$this->__i]) ){ $this->__o[$keys[$this->__i]][1] = $info; }"` |
|    - | 1059 | `" }"` |
|    - | 1060 | `" public function rewind(){ $this->__i = 0; }"` |
|    - | 1061 | `" public function valid(){ return $this->__i < count($this->__o); }"` |
|    - | 1062 | `" public function key(){ return $this->__i; }"` |
|    - | 1063 | `" public function current(){"` |
|    - | 1064 | `"  $pair = array_values($this->__o)[$this->__i] ?? null;"` |
|    - | 1065 | `"  return $pair === null ? null : $pair[0];"` |
|    - | 1066 | `" }"` |
|    - | 1067 | `" public function next(){ $this->__i++; }"` |
|    - | 1068 | `"}"` |
|    - | 1069 | `"interface SplObserver {"` |
|    - | 1070 | `" public function update(SplSubject $subject);"` |
|    - | 1071 | `"}"` |
|    - | 1072 | `"interface SplSubject {"` |
|    - | 1073 | `" public function attach(SplObserver $observer);"` |
|    - | 1074 | `" public function detach(SplObserver $observer);"` |
|    - | 1075 | `" public function notify();"` |
|    - | 1076 | `"}"` |
|    - | 1077 | `"class SplFileInfo implements Stringable {"` |
|    - | 1078 | `" protected $__pathName = '';"` |
|    - | 1079 | `" protected $__fileName = '';"` |
|    - | 1080 | `" public function __construct($path){"` |
|    - | 1081 | `"  $this->__pathName = (string)$path;"` |
|    - | 1082 | `"  $this->__fileName = basename($this->__pathName);"` |
|    - | 1083 | `" }"` |
|    - | 1084 | `" public function getPathname(){ return $this->__pathName; }"` |
|    - | 1085 | `" public function getFilename(){ return $this->__fileName; }"` |
|    - | 1086 | `" public function getPath(){ return dirname($this->__pathName); }"` |
|    - | 1087 | `" public function getBasename($suffix = ''){"` |
|    - | 1088 | `"  $b = basename($this->__pathName);"` |
|    - | 1089 | `"  if( $suffix !== '' && strlen($suffix) < strlen($b) && substr($b, -strlen($suffix)) === $suffix ){"` |
|    - | 1090 | `"   $b = substr($b, 0, -strlen($suffix));"` |
|    - | 1091 | `"  }"` |
|    - | 1092 | `"  return $b;"` |
|    - | 1093 | `" }"` |
|    - | 1094 | `" public function getExtension(){ return pathinfo($this->__pathName, PATHINFO_EXTENSION); }"` |
|    - | 1095 | `" public function getRealPath(){ return realpath($this->__pathName); }"` |
|    - | 1096 | `" public function isDir(){ return is_dir($this->__pathName); }"` |
|    - | 1097 | `" public function isFile(){ return is_file($this->__pathName); }"` |
|    - | 1098 | `" public function isLink(){ return is_link($this->__pathName); }"` |
|    - | 1099 | `" public function isReadable(){ return is_readable($this->__pathName); }"` |
|    - | 1100 | `" public function isWritable(){ return is_writable($this->__pathName); }"` |
|    - | 1101 | `" public function getSize(){ return filesize($this->__pathName); }"` |
|    - | 1102 | `" public function getMTime(){ return filemtime($this->__pathName); }"` |
|    - | 1103 | `" public function getATime(){ return fileatime($this->__pathName); }"` |
|    - | 1104 | `" public function getCTime(){ return filectime($this->__pathName); }"` |
|    - | 1105 | `" public function getType(){ return filetype($this->__pathName); }"` |
|    - | 1106 | `" public function getFileInfo(){ return new SplFileInfo($this->__pathName); }"` |
|    - | 1107 | `" public function getPathInfo(){ return new SplFileInfo(dirname($this->__pathName)); }"` |
|    - | 1108 | `" public function __toString(){ return $this->__pathName; }"` |
|    - | 1109 | `"}"` |
|    - | 1110 | `"class DirectoryIterator extends SplFileInfo implements SeekableIterator {"` |
|    - | 1111 | `" protected $__dir = '';"` |
|    - | 1112 | `" protected $__entries = array();"` |
|    - | 1113 | `" protected $__pos = 0;"` |
|    - | 1114 | `" public function __construct($path){"` |
|    - | 1115 | `"  $this->__dir = (string)$path;"` |
|    - | 1116 | `"  parent::__construct($this->__dir);"` |
|    - | 1117 | `"  $this->__load();"` |
|    - | 1118 | `" }"` |
|    - | 1119 | `" protected function __load(){"` |
|    - | 1120 | `"  $this->__entries = array();"` |
|    - | 1121 | `"  $h = @opendir($this->__dir);"` |
|    - | 1122 | `"  if( $h !== false ){"` |
|    - | 1123 | `"   while( ($e = readdir($h)) !== false ){ $this->__entries[] = $e; }"` |
|    - | 1124 | `"   closedir($h);"` |
|    - | 1125 | `"  }"` |
|    - | 1126 | `"  $this->__pos = 0;"` |
|    - | 1127 | `"  $this->__sync();"` |
|    - | 1128 | `" }"` |
|    - | 1129 | `" protected function __join($name){"` |
|    - | 1130 | `"  $d = $this->__dir;"` |
|    - | 1131 | `"  $last = substr($d, -1);"` |
|    - | 1132 | `"  $sep = ($last === '/' \|\| $last === '\\\\' \|\| $d === '') ? '' : '/';"` |
|    - | 1133 | `"  return $d . $sep . $name;"` |
|    - | 1134 | `" }"` |
|    - | 1135 | `" protected function __sync(){"` |
|    - | 1136 | `"  if( $this->__pos >= 0 && $this->__pos < count($this->__entries) ){"` |
|    - | 1137 | `"   $name = $this->__entries[$this->__pos];"` |
|    - | 1138 | `"   $this->__fileName = $name;"` |
|    - | 1139 | `"   $this->__pathName = $this->__join($name);"` |
|    - | 1140 | `"  }"` |
|    - | 1141 | `" }"` |
|    - | 1142 | `" public function isDot(){ $n = $this->__fileName; return $n === '.' \|\| $n === '..'; }"` |
|    - | 1143 | `" public function getFilename(){ return $this->__fileName; }"` |
|    - | 1144 | `" public function current(){ return $this; }"` |
|    - | 1145 | `" public function key(){ return $this->__pos; }"` |
|    - | 1146 | `" public function next(){ $this->__pos++; $this->__sync(); }"` |
|    - | 1147 | `" public function rewind(){ $this->__pos = 0; $this->__sync(); }"` |
|    - | 1148 | `" public function valid(){ return $this->__pos < count($this->__entries); }"` |
|    - | 1149 | `" public function seek($position){ $this->__pos = (int)$position; $this->__sync(); }"` |
|    - | 1150 | `" public function getFlags(){ return 0; }"` |
|    - | 1151 | `"}"` |
|    - | 1152 | `"class FilesystemIterator extends DirectoryIterator {"` |
|    - | 1153 | `" const CURRENT_AS_PATHNAME = 32;"` |
|    - | 1154 | `" const CURRENT_AS_FILEINFO = 0;"` |
|    - | 1155 | `" const CURRENT_AS_SELF = 16;"` |
|    - | 1156 | `" const CURRENT_MODE_MASK = 240;"` |
|    - | 1157 | `" const KEY_AS_PATHNAME = 0;"` |
|    - | 1158 | `" const KEY_AS_FILENAME = 256;"` |
|    - | 1159 | `" const FOLLOW_SYMLINKS = 512;"` |
|    - | 1160 | `" const KEY_MODE_MASK = 3840;"` |
|    - | 1161 | `" const NEW_CURRENT_AND_KEY = 256;"` |
|    - | 1162 | `" const OTHER_MODE_MASK = 12288;"` |
|    - | 1163 | `" const SKIP_DOTS = 4096;"` |
|    - | 1164 | `" const UNIX_PATHS = 8192;"` |
|    - | 1165 | `" protected $__flags = 4096;"` |
|    - | 1166 | `" public function __construct($path, $flags = 4096){"` |
|    - | 1167 | `"  $this->__flags = (int)$flags;"` |
|    - | 1168 | `"  parent::__construct($path);"` |
|    - | 1169 | `" }"` |
|    - | 1170 | `" protected function __skipDots(){"` |
|    - | 1171 | `"  if( $this->__flags & self::SKIP_DOTS ){"` |
|    - | 1172 | `"   while( ($this->__pos < count($this->__entries)) && $this->isDot() ){ $this->__pos++; $this->__sync(); }"` |
|    - | 1173 | `"  }"` |
|    - | 1174 | `" }"` |
|    - | 1175 | `" public function rewind(){ $this->__pos = 0; $this->__sync(); $this->__skipDots(); }"` |
|    - | 1176 | `" public function next(){ $this->__pos++; $this->__sync(); $this->__skipDots(); }"` |
|    - | 1177 | `" public function current(){"` |
|    - | 1178 | `"  $mode = $this->__flags & self::CURRENT_MODE_MASK;"` |
|    - | 1179 | `"  if( $mode === self::CURRENT_AS_PATHNAME ){ return $this->getPathname(); }"` |
|    - | 1180 | `"  if( $mode === self::CURRENT_AS_SELF ){ return $this; }"` |
|    - | 1181 | `"  return new SplFileInfo($this->getPathname());"` |
|    - | 1182 | `" }"` |
|    - | 1183 | `" public function key(){"` |
|    - | 1184 | `"  if( $this->__flags & self::KEY_AS_FILENAME ){ return $this->getFilename(); }"` |
|    - | 1185 | `"  return $this->getPathname();"` |
|    - | 1186 | `" }"` |
|    - | 1187 | `" public function getFlags(){ return $this->__flags; }"` |
|    - | 1188 | `" public function setFlags($flags){ $this->__flags = (int)$flags; }"` |
|    - | 1189 | `"}"` |
|    - | 1190 | `"class RecursiveDirectoryIterator extends FilesystemIterator implements RecursiveIterator {"` |
|    - | 1191 | `" public function hasChildren(){"` |
|    - | 1192 | `"  if( $this->isDot() ){ return false; }"` |
|    - | 1193 | `"  return $this->isDir();"` |
|    - | 1194 | `" }"` |
|    - | 1195 | `" public function getChildren(){"` |
|    - | 1196 | `"  return new RecursiveDirectoryIterator($this->getPathname(), $this->__flags);"` |
|    - | 1197 | `" }"` |
|    - | 1198 | `" public function getSubPath(){ return ''; }"` |
|    - | 1199 | `" public function getSubPathname(){ return $this->getFilename(); }"` |
|    - | 1200 | `"}"` |
|    - | 1201 | `;` |
|    - | 1202 |  |
| 3876 | 1203 | `PH7_PRIVATE sxi32 PH7_VmInstallSpl(ph7_vm *pVm)` |
|    5 | 1204 | `{` |
| 3881 | 1205 | `	ph7_create_function(&(*pVm),"__spl_deprecated",vm_builtin_spl_deprecated,0);` |
| 3881 | 1206 | `	ph7_create_function(&(*pVm),"__weak_create",vm_builtin_weak_create,0);` |
| 3881 | 1207 | `	ph7_create_function(&(*pVm),"__weak_get",vm_builtin_weak_get,0);` |
| 3881 | 1208 | `	ph7_create_function(&(*pVm),"__weak_drop",vm_builtin_weak_drop,0);` |
| 3881 | 1209 | `	return PH7_VmEvalBuiltinChunk(&(*pVm),zSplLib,sizeof(zSplLib)-1);` |
|    5 | 1210 | `}` |
|    - | 1211 |  |
|    - | 1212 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - | 1213 |  |
|    - | 1214 | `#ifdef PH7_DISABLE_BUILTIN_FUNC` |
|    - | 1215 | `/* Tiny build: no SPL (builtin layer disabled) */` |
|    - | 1216 | `PH7_PRIVATE sxi32 PH7_VmInstallSpl(ph7_vm *pVm){ (void)pVm; return SXRET_OK; }` |
|    - | 1217 | `#endif` |
|    - | 1218 |  |
