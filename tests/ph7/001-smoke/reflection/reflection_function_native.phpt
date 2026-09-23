--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The function reflectors read a compiled body or a declared signature, whichever the target has
--DESCRIPTION--
A reflected function's parameters come from one of two places — a compiled
function's argument list, or the php-style signature string a C builtin or a
native method declares — and every accessor has to answer the same way for both.
This pins the ones that differ per source: a builtin has no file or line, its
defaults are TEXT rather than byte-code, and only a compiled parameter can carry
attributes or a constant default.
--FILE--
<?php
const REFL_FN_K = 41;
/** doc comment */
function reflFnTarget(int $a, string $b = 'x', ?array $c = null, $d = REFL_FN_K, int ...$rest): ?string
{
    static $seen = 7;
    return $b;
}
function &reflFnNativeRef(array &$r) { return $r; }

$r = new ReflectionFunction('reflFnTarget');
echo $r->getName(), ' params=', $r->getNumberOfParameters(),
     ' required=', $r->getNumberOfRequiredParameters(),
     ' variadic=', var_export($r->isVariadic(), true),
     ' ret=', (string)$r->getReturnType(),
     ' internal=', var_export($r->isInternal(), true), "\n";
echo 'doc=', trim($r->getDocComment()), ' statics=', json_encode($r->getStaticVariables()), "\n";
foreach ($r->getParameters() as $p) {
    echo '  ', $p->getPosition(), ' $', $p->getName(),
         ' type=', $p->hasType() ? (string)$p->getType() : '-',
         ' null=', (int)$p->allowsNull(),
         ' opt=', (int)$p->isOptional(),
         ' hasDef=', (int)$p->isDefaultValueAvailable(),
         ' const=', $p->isDefaultValueAvailable() ? (int)$p->isDefaultValueConstant() : '-',
         ' variadic=', (int)$p->isVariadic(), "\n";
}
echo 'constName=', $r->getParameters()[3]->getDefaultValueConstantName(),
     ' value=', var_export($r->getParameters()[3]->getDefaultValue(), true), "\n";
// Both questions presuppose a default: php raises when there is none.
try { $r->getParameters()[0]->isDefaultValueConstant(); }
catch (Throwable $e) { echo 'noDefault: ', get_class($e), ': ', $e->getMessage(), "\n"; }
echo 'byref: ret=', var_export((new ReflectionFunction('reflFnNativeRef'))->returnsReference(), true),
     ' param=', var_export((new ReflectionFunction('reflFnNativeRef'))->getParameters()[0]->isPassedByReference(), true), "\n";

// A C builtin: described by its signature row, so it has parameters but no source.
$b = new ReflectionFunction('str_repeat');
echo 'builtin internal=', var_export($b->isInternal(), true),
     ' file=', var_export($b->getFileName(), true),
     ' line=', var_export($b->getStartLine(), true), "\n";
// PHL has exactly one synthetic extension; strlen is in php's Core too.
echo 'core ext=', var_export((new ReflectionFunction('strlen'))->getExtensionName(), true),
     ' obj=', get_class((new ReflectionFunction('strlen'))->getExtension()), "\n";
foreach ($b->getParameters() as $p) {
    echo '  $', $p->getName(), ' ', (string)$p->getType(), ' opt=', (int)$p->isOptional(), "\n";
}
echo 'invoke=', $b->invoke('ab', 2), ' closure=', ($b->getClosure())('c', 3), "\n";

// A native METHOD declares its parameters the same way a builtin does.
$m = new ReflectionMethod('ReflectionClass', 'getMethods');
echo 'native ', $m->class, '::', $m->getName(),
     ' internal=', var_export($m->isInternal(), true),
     ' params=', $m->getNumberOfParameters(),
     ' required=', $m->getNumberOfRequiredParameters(), "\n";
$p = $m->getParameters()[0];
echo '  $', $p->getName(), ' ', (string)$p->getType(),
     ' opt=', (int)$p->isOptional(), ' default=', var_export($p->getDefaultValue(), true), "\n";

// The deprecated trio php still declares and still answers (with a notice of
// its own, silenced here so the answers are what this pins).
class ReflFnTypes { public function m(ReflFnTypes $o, ?array $a, callable $c, int $i) {} }
$prev = error_reporting(E_ALL & ~E_DEPRECATED);
foreach ((new ReflectionMethod('ReflFnTypes', 'm'))->getParameters() as $p) {
    echo '  $', $p->getName(), ' class=', var_export($p->getClass()?->getName(), true),
         ' isArray=', (int)$p->isArray(), ' isCallable=', (int)$p->isCallable(), "\n";
}
error_reporting($prev);

// php does NOT split "C::m" for ReflectionParameter — only ReflectionMethod does.
try { new ReflectionParameter('ReflFnTypes::m', 0); }
catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
echo (new ReflectionParameter(['ReflFnTypes', 'm'], 0))->getName(), "\n";

// The reflectors php declares uncloneable are.
try { $c = clone $r; echo "cloned\n"; } catch (Throwable $e) { echo get_class($e), "\n"; }
?>
--EXPECT--
reflFnTarget params=5 required=1 variadic=true ret=?string internal=false
doc=/** doc comment */ statics={"seen":7}
  0 $a type=int null=0 opt=0 hasDef=0 const=- variadic=0
  1 $b type=string null=0 opt=1 hasDef=1 const=0 variadic=0
  2 $c type=?array null=1 opt=1 hasDef=1 const=0 variadic=0
  3 $d type=- null=1 opt=1 hasDef=1 const=1 variadic=0
  4 $rest type=int null=0 opt=1 hasDef=0 const=- variadic=1
constName=REFL_FN_K value=41
noDefault: ReflectionException: Internal error: Failed to retrieve the default value
byref: ret=true param=true
builtin internal=true file=false line=false
core ext='Core' obj=ReflectionExtension
  $string string opt=0
  $times int opt=0
invoke=abab closure=ccc
native ReflectionClass::getMethods internal=true params=1 required=0
  $filter ?int opt=1 default=NULL
  $o class='ReflFnTypes' isArray=0 isCallable=0
  $a class=NULL isArray=1 isCallable=0
  $c class=NULL isArray=0 isCallable=1
  $i class=NULL isArray=0 isCallable=0
ReflectionException: Function ReflFnTypes::m() does not exist
o
Error
