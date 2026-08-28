# src/ph7/vm_builtin_spl.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5/13 lines (38.46%)

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
|    - |   30 | `static const char zSplLib[] =` |
|    - |   31 | `"interface SeekableIterator extends Iterator {"` |
|    - |   32 | `" public function seek($offset);"` |
|    - |   33 | `"}"` |
|    - |   34 | `"trait __SplStoreT {"` |
|    - |   35 | `" private $__d = [];"` |
|    - |   36 | `" private $__f = 0;"` |
|    - |   37 | `" private function __splInitStore($array, $flags, $ctor){"` |
|    - |   38 | `"  if( is_array($array) ){"` |
|    - |   39 | `"   $this->__d = $array;"` |
|    - |   40 | `"  }elseif( is_object($array) ){"` |
|    - |   41 | `"   __spl_deprecated(get_class($this) . '::' . $ctor . '(): Using an object as a"` |
|    - |   42 | `" backing array for ' . get_class($this) . ' is deprecated, as it allows violating"` |
|    - |   43 | `" class constraints and invariants');"` |
|    - |   44 | `"  $this->__d = get_object_vars($array);"` |
|    - |   45 | `"  }else{"` |
|    - |   46 | `"   throw new TypeError(get_class($this) . '::' . $ctor . '(): Argument #1 ($array)"` |
|    - |   47 | `" must be of type array, ' . get_debug_type($array) . ' given');"` |
|    - |   48 | `"  }"` |
|    - |   49 | `"  $this->__f = (int)$flags;"` |
|    - |   50 | `" }"` |
|    - |   51 | `" public function offsetExists($key){ return array_key_exists($key, $this->__d); }"` |
|    - |   52 | `" public function offsetGet($key){ return $this->__d[$key]; }"` |
|    - |   53 | `" public function offsetSet($key, $value){"` |
|    - |   54 | `"  if( $key === null ){ $this->__d[] = $value; }"` |
|    - |   55 | `"  else { $this->__d[$key] = $value; }"` |
|    - |   56 | `" }"` |
|    - |   57 | `" public function offsetUnset($key){ unset($this->__d[$key]); }"` |
|    - |   58 | `" public function append($value){ $this->__d[] = $value; }"` |
|    - |   59 | `" public function getArrayCopy(){ return $this->__d; }"` |
|    - |   60 | `" public function count(){ return count($this->__d); }"` |
|    - |   61 | `" public function getFlags(){ return $this->__f; }"` |
|    - |   62 | `" public function setFlags($flags){ $this->__f = (int)$flags; }"` |
|    - |   63 | `" public function asort($flags = 0){ asort($this->__d); return true; }"` |
|    - |   64 | `" public function ksort($flags = 0){ ksort($this->__d); return true; }"` |
|    - |   65 | `" public function uasort($callback){ uasort($this->__d, $callback); return true; }"` |
|    - |   66 | `" public function uksort($callback){ uksort($this->__d, $callback); return true; }"` |
|    - |   67 | `" public function natsort(){ uasort($this->__d, 'strnatcmp'); return true; }"` |
|    - |   68 | `" public function natcasesort(){ uasort($this->__d, 'strnatcasecmp'); return true; }"` |
|    - |   69 | `"}"` |
|    - |   70 | `"class ArrayIterator implements SeekableIterator, ArrayAccess, Countable {"` |
|    - |   71 | `" use __SplStoreT;"` |
|    - |   72 | `" const STD_PROP_LIST = 1;"` |
|    - |   73 | `" const ARRAY_AS_PROPS = 2;"` |
|    - |   74 | `" public function __construct($array = [], $flags = 0){"` |
|    - |   75 | `"  $this->__splInitStore($array, $flags, '__construct');"` |
|    - |   76 | `"  reset($this->__d);"` |
|    - |   77 | `" }"` |
|    - |   78 | `" public function current(){"` |
|    - |   79 | `"  if( key($this->__d) === null ){ return null; }"` |
|    - |   80 | `"  return current($this->__d);"` |
|    - |   81 | `" }"` |
|    - |   82 | `" public function key(){ return key($this->__d); }"` |
|    - |   83 | `" public function next(){ next($this->__d); }"` |
|    - |   84 | `" public function rewind(){ reset($this->__d); }"` |
|    - |   85 | `" public function valid(){ return key($this->__d) !== null; }"` |
|    - |   86 | `" public function seek($offset){"` |
|    - |   87 | `"  $offset = (int)$offset;"` |
|    - |   88 | `"  if( $offset < 0 \|\| $offset >= count($this->__d) ){"` |
|    - |   89 | `"   throw new OutOfBoundsException('Seek position ' . $offset . ' is out of range');"` |
|    - |   90 | `"  }"` |
|    - |   91 | `"  reset($this->__d);"` |
|    - |   92 | `"  for( $i = 0; $i < $offset; $i++ ){ next($this->__d); }"` |
|    - |   93 | `" }"` |
|    - |   94 | `"}"` |
|    - |   95 | `"class ArrayObject implements IteratorAggregate, ArrayAccess, Countable {"` |
|    - |   96 | `" use __SplStoreT;"` |
|    - |   97 | `" const STD_PROP_LIST = 1;"` |
|    - |   98 | `" const ARRAY_AS_PROPS = 2;"` |
|    - |   99 | `" private $__it = 'ArrayIterator';"` |
|    - |  100 | `" public function __construct($array = [], $flags = 0, $iteratorClass = 'ArrayIterator'){"` |
|    - |  101 | `"  $this->__splInitStore($array, $flags, '__construct');"` |
|    - |  102 | `"  if( $iteratorClass !== 'ArrayIterator' ){ $this->setIteratorClass($iteratorClass); }"` |
|    - |  103 | `" }"` |
|    - |  104 | `" public function getIterator(){"` |
|    - |  105 | `"  $c = $this->__it;"` |
|    - |  106 | `"  return new $c($this->__d);"` |
|    - |  107 | `" }"` |
|    - |  108 | `" public function exchangeArray($array){"` |
|    - |  109 | `"  $old = $this->__d;"` |
|    - |  110 | `"  $this->__splInitStore($array, $this->__f, 'exchangeArray');"` |
|    - |  111 | `"  return $old;"` |
|    - |  112 | `" }"` |
|    - |  113 | `" public function setIteratorClass($iteratorClass){"` |
|    - |  114 | `"  $c = (string)$iteratorClass;"` |
|    - |  115 | `"  if( $c !== 'ArrayIterator'"` |
|    - |  116 | `"   && (!class_exists($c) \|\| !is_subclass_of($c, 'ArrayIterator')) ){"` |
|    - |  117 | `"   throw new TypeError('ArrayObject::setIteratorClass(): Argument #1"` |
|    - |  118 | `" ($iteratorClass) must be a class name derived from ArrayIterator, ' . $c . ' given');"` |
|    - |  119 | `"  }"` |
|    - |  120 | `"  $this->__it = $c;"` |
|    - |  121 | `" }"` |
|    - |  122 | `" public function getIteratorClass(){ return $this->__it; }"` |
|    - |  123 | `" public function __get($name){"` |
|    - |  124 | `"  if( $this->__f & 2 ){ return $this->__d[$name]; }"` |
|    - |  125 | `"  return null;"` |
|    - |  126 | `" }"` |
|    - |  127 | `" public function __set($name, $value){"` |
|    - |  128 | `"  if( $this->__f & 2 ){ $this->__d[$name] = $value; return; }"` |
|    - |  129 | `"  $this->{$name} = $value;"` |
|    - |  130 | `" }"` |
|    - |  131 | `" public function __isset($name){"` |
|    - |  132 | `"  if( $this->__f & 2 ){ return isset($this->__d[$name]); }"` |
|    - |  133 | `"  return false;"` |
|    - |  134 | `" }"` |
|    - |  135 | `" public function __unset($name){"` |
|    - |  136 | `"  if( $this->__f & 2 ){ unset($this->__d[$name]); }"` |
|    - |  137 | `" }"` |
|    - |  138 | `"}"` |
|    - |  139 | `"function natsort(&$array){ return uasort($array, 'strnatcmp'); }"` |
|    - |  140 | `"function natcasesort(&$array){ return uasort($array, 'strnatcasecmp'); }"` |
|    - |  141 | `;` |
|    - |  142 |  |
| 3876 |  143 | `PH7_PRIVATE sxi32 PH7_VmInstallSpl(ph7_vm *pVm)` |
|    5 |  144 | `{` |
| 3881 |  145 | `	ph7_create_function(&(*pVm),"__spl_deprecated",vm_builtin_spl_deprecated,0);` |
| 3881 |  146 | `	return PH7_VmEvalBuiltinChunk(&(*pVm),zSplLib,sizeof(zSplLib)-1);` |
|    5 |  147 | `}` |
|    - |  148 |  |
|    - |  149 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - |  150 |  |
|    - |  151 | `#ifdef PH7_DISABLE_BUILTIN_FUNC` |
|    - |  152 | `/* Tiny build: no SPL (builtin layer disabled) */` |
|    - |  153 | `PH7_PRIVATE sxi32 PH7_VmInstallSpl(ph7_vm *pVm){ (void)pVm; return SXRET_OK; }` |
|    - |  154 | `#endif` |
|    - |  155 |  |
