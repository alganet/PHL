/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
/*
 * The embedded PHP source of the Reflection class library (chunks 1-9),
 * compiled at VM init by PH7_VmInstallReflectionLib() — the tail step of
 * PH7_VmInstallReflection() (vm_builtin_reflection.c, which keeps the C
 * host-function thunks the library calls).
 */
/*
 * Chunk 6: the ReflectionEnum family. It is what is LEFT of the long tail --
 * ReflectionConstant, ReflectionExtension, ReflectionZendExtension and
 * ReflectionReference are native (PH7_VmInstallReflectionSmall). These three
 * stay because they extend ReflectionClass and ReflectionClassConstant, which
 * are still prelude PHP; they move when those do.
 */
static const char zReflectLib6[] =
"class ReflectionEnum extends ReflectionClass {"
" public function __construct($objectOrClass){"
"  $info = __phl_rcinfo($objectOrClass);"
"  if($info === null){"
"   throw new ReflectionException('Class \"'.$objectOrClass.'\" does not exist');"
"  }"
"  if(!$info['enum']){"
"   throw new ReflectionException('Class \"'.$info['name'].'\" is not an enum');"
"  }"
"  parent::__construct($objectOrClass);"
" }"
" public function hasCase($name){"
"  $i = __phl_rcinfo($this->name);"
"  return in_array($name, $i['cases'], true);"
" }"
" public function getCase($name){"
"  if(!$this->hasCase($name)){"
"   throw new ReflectionException('Case '.$this->name.'::'.$name.' does not exist');"
"  }"
"  if($this->isBacked()){ return new ReflectionEnumBackedCase($this->name, $name); }"
"  return new ReflectionEnumUnitCase($this->name, $name);"
" }"
" public function getCases(){"
"  $i = __phl_rcinfo($this->name);"
"  $out = array();"
"  foreach($i['cases'] as $c){"
"   $out[] = $this->isBacked()"
"    ? new ReflectionEnumBackedCase($this->name, $c)"
"    : new ReflectionEnumUnitCase($this->name, $c);"
"  }"
"  return $out;"
" }"
" public function isBacked(){ $i = __phl_rcinfo($this->name); return $i['enumbacking'] !== ''; }"
" public function getBackingType(){"
"  $i = __phl_rcinfo($this->name);"
"  if($i['enumbacking'] === ''){ return null; }"
"  return __reflect_make_type($i['enumbacking']);"
" }"
"}"
"class ReflectionEnumUnitCase extends ReflectionClassConstant {"
" public function __construct($class, $constant){"
"  parent::__construct($class, $constant);"
"  $ci = __phl_rcinfo($class);"
"  if(!$ci['enum']){"
"   throw new ReflectionException('Class \"'.$this->class.'\" is not an enum');"
"  }"
"  if(!$this->isEnumCase()){"
"   throw new ReflectionException('Constant '.$this->class.'::'.$constant.' is not a case');"
"  }"
" }"
" public function getEnum(){ return new ReflectionEnum($this->class); }"
"}"
"class ReflectionEnumBackedCase extends ReflectionEnumUnitCase {"
" public function getBackingValue(){ return $this->getValue()->value; }"
"}"
;
/*
 * Chunk 7: ReflectionAttribute and the shared getAttributes() builder.
 * The spec array rides as [kind, target, member, paramIdx]; argument
 * values evaluate lazily through __reflect_attr_args (PHP semantics).
 */
static const char zReflectLib7[] =
"function __reflect_has_deprecated($meta){"
" foreach($meta as $a){"
"  if(strtolower($a['name']) === 'deprecated'){ return true; }"
" }"
" return false;"
"}"
"function __reflect_target_names($mask){"
" $parts = array();"
" foreach(array('class' => 1, 'function' => 2, 'method' => 4, 'property' => 8,"
"  'class constant' => 16, 'parameter' => 32, 'constant' => 64) as $nm => $bit){"
"  if($mask & $bit){ $parts[] = $nm; }"
" }"
" return implode(', ', $parts);"
"}"
"function __reflect_build_attrs($meta, $spec, $target, $name, $flags){"
" $out = array();"
" $counts = array();"
" foreach($meta as $a){"
"  $k = strtolower($a['name']);"
"  $counts[$k] = isset($counts[$k]) ? $counts[$k] + 1 : 1;"
" }"
" $idx = 0;"
" foreach($meta as $a){"
"  $keep = true;"
"  if($name !== null){"
"   $keep = strtolower($a['name']) === strtolower($name);"
"   if(!$keep && ($flags & 2)){"
"    $keep = is_subclass_of($a['name'], $name);"
"   }"
"  }"
"  if($keep){"
"   $r = __reflect_new_no_ctor('ReflectionAttribute');"
"   $r->__init($a['name'], $spec, $idx, $target, $counts[strtolower($a['name'])] > 1);"
"   $out[] = $r;"
"  }"
"  $idx++;"
" }"
" return $out;"
"}"
"final class ReflectionAttribute {"
" const IS_INSTANCEOF = 2;"
" protected $__name = '';"
" protected $__spec = null;"
" protected $__idx = 0;"
" protected $__target = 0;"
" protected $__rep = false;"
" public function __construct(){"
"  throw new Error('Call to private ReflectionAttribute::__construct() from global scope');"
" }"
" public function __init($name, $spec, $idx, $target, $rep){"
"  $this->__name = $name;"
"  $this->__spec = $spec;"
"  $this->__idx = $idx;"
"  $this->__target = $target;"
"  $this->__rep = $rep;"
" }"
" public function getName(){ return $this->__name; }"
" public function getTarget(){ return $this->__target; }"
" public function isRepeated(){ return $this->__rep; }"
" public function getArguments(){"
"  $a = __reflect_attr_args($this->__spec[0], $this->__spec[1], $this->__spec[2], $this->__spec[3], $this->__idx);"
"  return $a === null ? array() : $a;"
" }"
" public function newInstance(){"
"  $name = $this->__name;"
"  $ci = __phl_rcinfo($name);"
"  if($ci === null){"
"   throw new Error('Attribute class \"'.$name.'\" not found');"
"  }"
"  $name = $ci['name'];"
"  $decl = null;"
"  $didx = 0;"
"  foreach($ci['attrs'] as $a){"
"   if(strtolower($a['name']) === 'attribute'){ $decl = $didx; break; }"
"   $didx++;"
"  }"
"  if($decl === null){"
"   throw new Error('Attempting to use non-attribute class \"'.$name.'\" as attribute');"
"  }"
"  $dargs = __reflect_attr_args('class', $name, null, 0, $decl);"
"  $flags = 127;"
"  if(is_array($dargs)){"
"   if(isset($dargs[0])){ $flags = $dargs[0]; }"
"   else if(isset($dargs['flags'])){ $flags = $dargs['flags']; }"
"  }"
"  if(($flags & $this->__target) === 0){"
"   $tnames = array(1 => 'class', 2 => 'function', 4 => 'method', 8 => 'property',"
"    16 => 'class constant', 32 => 'parameter', 64 => 'constant');"
"   throw new Error('Attribute \"'.$name.'\" cannot target '.$tnames[$this->__target]"
"    .' (allowed targets: '.__reflect_target_names($flags).')');"
"  }"
"  if($this->__rep && ($flags & 128) === 0){"
"   throw new Error('Attribute \"'.$name.'\" must not be repeated');"
"  }"
"  return __reflect_new_instance($name, $this->getArguments());"
" }"
" public function __toString(){"
"  return 'Attribute [ '.$this->__name.' ]';"
" }"
"}"
;
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
" else{ $t = 'null'; }"
" $vs = is_array($v) ? 'Array' : (is_bool($v) ? ($v ? '1' : '') : (string)$v);"
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
	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib6, sizeof(zReflectLib6)-1);
	if( rc != SXRET_OK ){
		return rc;
	}
	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib7, sizeof(zReflectLib7)-1);
	if( rc != SXRET_OK ){
		return rc;
	}
	return PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib9, sizeof(zReflectLib9)-1);
}
