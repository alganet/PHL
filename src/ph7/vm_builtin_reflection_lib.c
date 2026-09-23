/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
/*
 * What is LEFT of the embedded PHP source of the Reflection class library --
 * chunks 6 and 9 of the original nine, compiled at VM init by
 * PH7_VmInstallReflectionLib(), the tail step of PH7_VmInstallReflection()
 * (vm_builtin_reflection.c, which declares everything else from C).
 */
/*
 * Chunk 9: PHP's Reflection export format (__toString on every Reflector).
 * Built entirely from the public reflection API of the target objects.
 */
static const char zReflectLib9[] =
"function __reflect_export_value($v){"
" if($v === null){ return 'NULL'; }"
" if($v === true){ return 'true'; }"
" if($v === false){ return 'false'; }"
" if(is_string($v)){ return chr(39).$v.chr(39); }"
" if(is_array($v)){"
"  $parts = array();"
"  $isList = true;"
"  $next = 0;"
"  foreach($v as $k => $x){"
"   if($k !== $next){ $isList = false; break; }"
"   $next++;"
"  }"
"  foreach($v as $k => $x){"
"   $parts[] = $isList ? __reflect_export_value($x)"
"    : (__reflect_export_value($k).' => '.__reflect_export_value($x));"
"  }"
"  return '['.implode(', ', $parts).']';"
" }"
" return (string)$v;"
"}"
"function __reflect_export_param($p){"
" $s = 'Parameter #'.$p->getPosition().' [ <'.($p->isOptional() ? 'optional' : 'required').'> ';"
" $t = $p->getType();"
" if($t !== null){ $s .= (string)$t.' '; }"
" if($p->isPassedByReference()){ $s .= '&'; }"
" if($p->isVariadic()){ $s .= '...'; }"
" $s .= '$'.$p->getName();"
" if($p->isDefaultValueAvailable()){"
"  try{ $s .= ' = '.__reflect_export_value($p->getDefaultValue()); }"
"  catch(ReflectionException $e){ $s .= ' = <default>'; }"
" }"
" return $s.' ]';"
"}"
"function __reflect_export_prop($p){"
" $s = 'Property [ ';"
" $s .= $p->isPrivate() ? 'private ' : ($p->isProtected() ? 'protected ' : 'public ');"
" if($p->isStatic()){ $s .= 'static '; }"
" if($p->isReadOnly()){ $s .= 'readonly '; }"
" $t = $p->getType();"
" if($t !== null){ $s .= (string)$t.' '; }"
" $s .= '$'.$p->getName();"
" if($p->hasDefaultValue()){ $s .= ' = '.__reflect_export_value($p->getDefaultValue()); }"
" return $s.' ]'.chr(10);"
"}"
"function __reflect_export_cconst($c){"
" $v = $c->getValue();"
" if(is_int($v)){ $t = 'int'; }"
" else if(is_string($v)){ $t = 'string'; }"
" else if(is_float($v)){ $t = 'float'; }"
" else if(is_bool($v)){ $t = 'bool'; }"
" else if(is_array($v)){ $t = 'array'; }"
/* An OBJECT constant — an enum case, overwhelmingly — reports its CLASS as the
 * type and the literal word Object as the value. The (string) cast below is a
 * fatal for one, which is what `echo new ReflectionEnumUnitCase(...)` hit. */
" else if(is_object($v)){ $t = get_class($v); }"
" else{ $t = 'null'; }"
" $vs = is_array($v) ? 'Array'"
"  : (is_object($v) ? 'Object' : (is_bool($v) ? ($v ? '1' : '') : (string)$v));"
" $vis = $c->isPrivate() ? 'private' : ($c->isProtected() ? 'protected' : 'public');"
" return 'Constant [ '.$vis.' '.$t.' '.$c->name.' ] { '.$vs.' }'.chr(10);"
"}"
/* $owner: the class being EXPORTED, when there is one. php's tags are relative
 * to it — a method it did not declare "inherits" from its declaring class —
 * and ReflectionMethod::$class is the DECLARING class, so the reflector alone
 * cannot answer that. __toString() on a lone method passes no owner and gets
 * php's prototype tag only. */
"function __reflect_export_fnabs($r, $indent, $owner = null){"
" $tags = $r->isInternal() ? 'internal:Core' : 'user';"
" if($r instanceof ReflectionMethod){"
"  if($r->isConstructor()){ $tags .= ', ctor'; }"
"  else if($r->isDestructor()){ $tags .= ', dtor'; }"
"  $decl = $r->getDeclaringClass()->name;"
"  if($owner !== null && strtolower($decl) !== strtolower($owner)){ $tags .= ', inherits '.$decl; }"
"  else if($r->hasPrototype()){ $tags .= ', prototype '.$r->getPrototype()->class; }"
"  $head = 'Method [ <'.$tags.'> ';"
"  if($r->isAbstract()){ $head .= 'abstract '; }"
"  if($r->isFinal()){ $head .= 'final '; }"
"  if($r->isStatic()){ $head .= 'static '; }"
"  $head .= $r->isPrivate() ? 'private ' : ($r->isProtected() ? 'protected ' : 'public ');"
"  $head .= 'method '.$r->name.' ]';"
" }else{"
"  $kind = $r->isClosure() ? 'Closure' : 'Function';"
"  $head = $kind.' [ <'.$tags.'> function '.$r->name.' ]';"
" }"
" $s = $head.' {'.chr(10);"
" if(!$r->isInternal()){"
"  $s .= '  @@ '.$r->getFileName().' '.$r->getStartLine().' - '.$r->getEndLine().chr(10);"
" }"
" $ps = $r->getParameters();"
" $ret = $r->getReturnType();"
" if(count($ps) > 0 || $ret !== null){"
"  $s .= chr(10).'  - Parameters ['.count($ps).'] {'.chr(10);"
"  foreach($ps as $p){ $s .= '    '.__reflect_export_param($p).chr(10); }"
"  $s .= '  }'.chr(10);"
" }"
" if($ret !== null){ $s .= '  - Return [ '.(string)$ret.' ]'.chr(10); }"
" $s .= '}'.chr(10);"
" if($indent === ''){ return $s; }"
" $lines = explode(chr(10), $s);"
" $out = '';"
" $n = count($lines);"
" for($k = 0; $k < $n; $k++){"
"  if($lines[$k] === '' && $k === $n - 1){ break; }"
"  $out .= ($lines[$k] === '' ? '' : $indent.$lines[$k]).chr(10);"
" }"
" return $out;"
"}"
"function __reflect_export_class($rc){"
" $tags = $rc->isInternal() ? 'internal:Core' : 'user';"
" if($rc->isInterface()){"
"  $head = 'Interface [ <'.$tags.'> interface '.$rc->name.' ]';"
" }else{"
"  $mods = '';"
"  if($rc->isAbstract()){ $mods .= 'abstract '; }"
"  if($rc->isFinal()){ $mods .= 'final '; }"
"  $head = 'Class [ <'.$tags.'> '.$mods.'class '.$rc->name;"
"  $par = $rc->getParentClass();"
"  if($par !== false){ $head .= ' extends '.$par->name; }"
"  $ifs = $rc->getInterfaceNames();"
"  if(count($ifs) > 0){ $head .= ' implements '.implode(', ', $ifs); }"
"  $head .= ' ]';"
" }"
" $s = $head.' {'.chr(10);"
" if(!$rc->isInternal()){"
"  $s .= '  @@ '.$rc->getFileName().' '.$rc->getStartLine().'-'.$rc->getEndLine().chr(10);"
" }"
" $consts = $rc->getReflectionConstants();"
" $s .= chr(10).'  - Constants ['.count($consts).'] {'.chr(10);"
" foreach($consts as $c){ $s .= '    '.__reflect_export_cconst($c); }"
" $s .= '  }'.chr(10);"
" $sp = array();"
" $ip = array();"
" foreach($rc->getProperties() as $p){"
"  if($p->isStatic()){ $sp[] = $p; }else{ $ip[] = $p; }"
" }"
" $sm = array();"
" $im = array();"
" foreach($rc->getMethods() as $m){"
"  if($m->isStatic()){ $sm[] = $m; }else{ $im[] = $m; }"
" }"
" $s .= chr(10).'  - Static properties ['.count($sp).'] {'.chr(10);"
" foreach($sp as $p){ $s .= '    '.__reflect_export_prop($p); }"
" $s .= '  }'.chr(10);"
" $s .= chr(10).'  - Static methods ['.count($sm).'] {'.chr(10);"
" $first = true;"
" foreach($sm as $m){"
"  if(!$first){ $s .= chr(10); }"
"  $first = false;"
"  $s .= __reflect_export_fnabs($m, '    ', $rc->name);"
" }"
" $s .= '  }'.chr(10);"
" $s .= chr(10).'  - Properties ['.count($ip).'] {'.chr(10);"
" foreach($ip as $p){ $s .= '    '.__reflect_export_prop($p); }"
" $s .= '  }'.chr(10);"
" $s .= chr(10).'  - Methods ['.count($im).'] {'.chr(10);"
" $first = true;"
" foreach($im as $m){"
"  if(!$first){ $s .= chr(10); }"
"  $first = false;"
"  $s .= __reflect_export_fnabs($m, '    ', $rc->name);"
" }"
" $s .= '  }'.chr(10);"
" return $s.'}'.chr(10);"
"}"
;
/*
 * Register the __reflect_* thunks and compile the Reflection library.
 * Called from PH7_VmInit while pVm->bCompilingBuiltin is set, right after
 * the core builtin chunks (Exception and friends must exist already).
 */
/*
 * Compile the nine reflection chunks, in order. Split from
 * PH7_VmInstallReflection so the chunk strings and their sizeof stay in
 * one translation unit.
 */
PH7_PRIVATE sxi32 PH7_VmInstallReflectionLib(ph7_vm *pVm)
{
	sxi32 rc;
	/* Where chunk 1 was: Reflector, Reflection, ReflectionException,
	 * ReflectionClass and ReflectionObject are native now. Chunks 2 and 3 name
	 * `Reflector` in their own `implements` clauses, so this has to run first. */
	rc = PH7_VmInstallReflectionClass(&(*pVm));
	if( rc != SXRET_OK ){
		return rc;
	}
	/* Where chunk 2 was: ReflectionFunctionAbstract, ReflectionFunction,
	 * ReflectionMethod and ReflectionParameter are native now. */
	rc = PH7_VmInstallReflectionFunc(&(*pVm));
	if( rc != SXRET_OK ){
		return rc;
	}
	rc = PH7_VmInstallReflectionHookType(&(*pVm));
	if( rc != SXRET_OK ){
		return rc;
	}
	/* Where chunk 3 was: ReflectionProperty and ReflectionClassConstant are
	 * native now, and PropertyHookType is a native ENUM. */
	rc = PH7_VmInstallReflectionMember(&(*pVm));
	if( rc != SXRET_OK ){
		return rc;
	}
	/* Where chunk 4 was: the four type classes are native now (see
	 * PH7_VmInstallReflectionTypes). Stringable exists by this point. */
	rc = PH7_VmInstallReflectionTypes(&(*pVm));
	if( rc != SXRET_OK ){
		return rc;
	}
	/* Where chunk 5 was: ReflectionGenerator/Fiber and the four standalone
	 * classes chunk 6 used to hold are native now. Reflector (chunk 1) exists. */
	rc = PH7_VmInstallReflectionSmall(&(*pVm));
	if( rc != SXRET_OK ){
		return rc;
	}
	/* Where chunk 6 was: the three ReflectionEnum classes are native now, and
	 * the class DESCRIPTOR they read (__phl_rcinfo) has no callers left. */
	rc = PH7_VmInstallReflectionEnum(&(*pVm));
	if( rc != SXRET_OK ){
		return rc;
	}
	/* Where chunk 7 was: ReflectionAttribute is native now, and with it the
	 * builder (__reflect_build_attrs) and the two helpers it needed. */
	rc = PH7_VmInstallReflectionAttribute(&(*pVm));
	if( rc != SXRET_OK ){
		return rc;
	}
	return PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib9, sizeof(zReflectLib9)-1);
}
