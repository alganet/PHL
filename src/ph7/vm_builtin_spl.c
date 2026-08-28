/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#ifndef PH7_DISABLE_BUILTIN_FUNC
/*
 * SPL iterators, slice 1 (NEWPLAN band D): SeekableIterator, ArrayIterator,
 * ArrayObject, plus the natsort()/natcasesort() array functions they need.
 * Embedded-PHP chunk following the Reflection architecture — installed
 * inside the bCompilingBuiltin window, backed by the engine's native array
 * internal-pointer builtins (reset/next/key/current keep their position on a
 * property, so ArrayIterator's cursor IS the backing array's pointer).
 */

/* void __spl_deprecated(string $msg) — E_DEPRECATED with php's exact text
 * (no auto-prepended function name, unlike ph7_context_throw_error) */
static int vm_builtin_spl_deprecated(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zMsg;
	int nMsg;
	if( nArg < 1 ){
		return PH7_OK;
	}
	zMsg = ph7_value_to_string(apArg[0],&nMsg);
	PH7_VmThrowDeprecatedFmt(pCtx->pVm,"%.*s",nMsg,zMsg);
	return PH7_OK;
}

static const char zSplLib[] =
"interface SeekableIterator extends Iterator {"
" public function seek($offset);"
"}"
"trait __SplStoreT {"
" private $__d = [];"
" private $__f = 0;"
" private function __splInitStore($array, $flags, $ctor){"
"  if( is_array($array) ){"
"   $this->__d = $array;"
"  }elseif( is_object($array) ){"
"   __spl_deprecated(get_class($this) . '::' . $ctor . '(): Using an object as a"
" backing array for ' . get_class($this) . ' is deprecated, as it allows violating"
" class constraints and invariants');"
"  $this->__d = get_object_vars($array);"
"  }else{"
"   throw new TypeError(get_class($this) . '::' . $ctor . '(): Argument #1 ($array)"
" must be of type array, ' . get_debug_type($array) . ' given');"
"  }"
"  $this->__f = (int)$flags;"
" }"
" public function offsetExists($key){ return array_key_exists($key, $this->__d); }"
" public function offsetGet($key){ return $this->__d[$key]; }"
" public function offsetSet($key, $value){"
"  if( $key === null ){ $this->__d[] = $value; }"
"  else { $this->__d[$key] = $value; }"
" }"
" public function offsetUnset($key){ unset($this->__d[$key]); }"
" public function append($value){ $this->__d[] = $value; }"
" public function getArrayCopy(){ return $this->__d; }"
" public function count(){ return count($this->__d); }"
" public function getFlags(){ return $this->__f; }"
" public function setFlags($flags){ $this->__f = (int)$flags; }"
" public function asort($flags = 0){ asort($this->__d); return true; }"
" public function ksort($flags = 0){ ksort($this->__d); return true; }"
" public function uasort($callback){ uasort($this->__d, $callback); return true; }"
" public function uksort($callback){ uksort($this->__d, $callback); return true; }"
" public function natsort(){ uasort($this->__d, 'strnatcmp'); return true; }"
" public function natcasesort(){ uasort($this->__d, 'strnatcasecmp'); return true; }"
"}"
"class ArrayIterator implements SeekableIterator, ArrayAccess, Countable {"
" use __SplStoreT;"
" const STD_PROP_LIST = 1;"
" const ARRAY_AS_PROPS = 2;"
" public function __construct($array = [], $flags = 0){"
"  $this->__splInitStore($array, $flags, '__construct');"
"  reset($this->__d);"
" }"
" public function current(){"
"  if( key($this->__d) === null ){ return null; }"
"  return current($this->__d);"
" }"
" public function key(){ return key($this->__d); }"
" public function next(){ next($this->__d); }"
" public function rewind(){ reset($this->__d); }"
" public function valid(){ return key($this->__d) !== null; }"
" public function seek($offset){"
"  $offset = (int)$offset;"
"  if( $offset < 0 || $offset >= count($this->__d) ){"
"   throw new OutOfBoundsException('Seek position ' . $offset . ' is out of range');"
"  }"
"  reset($this->__d);"
"  for( $i = 0; $i < $offset; $i++ ){ next($this->__d); }"
" }"
"}"
"class ArrayObject implements IteratorAggregate, ArrayAccess, Countable {"
" use __SplStoreT;"
" const STD_PROP_LIST = 1;"
" const ARRAY_AS_PROPS = 2;"
" private $__it = 'ArrayIterator';"
" public function __construct($array = [], $flags = 0, $iteratorClass = 'ArrayIterator'){"
"  $this->__splInitStore($array, $flags, '__construct');"
"  if( $iteratorClass !== 'ArrayIterator' ){ $this->setIteratorClass($iteratorClass); }"
" }"
" public function getIterator(){"
"  $c = $this->__it;"
"  return new $c($this->__d);"
" }"
" public function exchangeArray($array){"
"  $old = $this->__d;"
"  $this->__splInitStore($array, $this->__f, 'exchangeArray');"
"  return $old;"
" }"
" public function setIteratorClass($iteratorClass){"
"  $c = (string)$iteratorClass;"
"  if( $c !== 'ArrayIterator'"
"   && (!class_exists($c) || !is_subclass_of($c, 'ArrayIterator')) ){"
"   throw new TypeError('ArrayObject::setIteratorClass(): Argument #1"
" ($iteratorClass) must be a class name derived from ArrayIterator, ' . $c . ' given');"
"  }"
"  $this->__it = $c;"
" }"
" public function getIteratorClass(){ return $this->__it; }"
" public function __get($name){"
"  if( $this->__f & 2 ){ return $this->__d[$name]; }"
"  return null;"
" }"
" public function __set($name, $value){"
"  if( $this->__f & 2 ){ $this->__d[$name] = $value; return; }"
"  $this->{$name} = $value;"
" }"
" public function __isset($name){"
"  if( $this->__f & 2 ){ return isset($this->__d[$name]); }"
"  return false;"
" }"
" public function __unset($name){"
"  if( $this->__f & 2 ){ unset($this->__d[$name]); }"
" }"
"}"
"function natsort(&$array){ return uasort($array, 'strnatcmp'); }"
"function natcasesort(&$array){ return uasort($array, 'strnatcasecmp'); }"
;

PH7_PRIVATE sxi32 PH7_VmInstallSpl(ph7_vm *pVm)
{
	ph7_create_function(&(*pVm),"__spl_deprecated",vm_builtin_spl_deprecated,0);
	return PH7_VmEvalBuiltinChunk(&(*pVm),zSplLib,sizeof(zSplLib)-1);
}

#endif /* PH7_DISABLE_BUILTIN_FUNC */

#ifdef PH7_DISABLE_BUILTIN_FUNC
/* Tiny build: no SPL (builtin layer disabled) */
PH7_PRIVATE sxi32 PH7_VmInstallSpl(ph7_vm *pVm){ (void)pVm; return SXRET_OK; }
#endif
