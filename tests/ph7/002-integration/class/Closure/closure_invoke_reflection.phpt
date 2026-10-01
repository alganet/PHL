--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A closure IS its own __invoke: method_exists, hasMethod, the call, and a reflector whose IDENTITY is the method's (internal, public, class Closure, no file, no module) while its PARAMETERS are the closure's. php builds it on demand rather than keeping it in the class's function table, which is why get_class_methods() and the class export do not name it and `new ReflectionMethod('Closure','__invoke')` refuses while the object form and every ReflectionClass door answer.
--FILE--
<?php
/* `getCurrent()` is php 8.5's and this engine has not got it yet, so the two method
 * listings are compared with that one name removed -- everything else about them is
 * pinned. */
function names(array $m): array { return array_values(array_diff(array_map(
    static fn($x) => is_string($x) ? $x : $x->getName(), $m), ['getCurrent'])); }

$c = static function (string $a, int $b = 1, ...$rest): string { return $a; };

// A closure IS its own __invoke.
var_dump(method_exists($c, '__invoke'), method_exists('Closure', '__invoke'));
var_dump($c->__invoke('called'));
$ro = new ReflectionObject($c);
var_dump($ro->hasMethod('__invoke'), (new ReflectionClass('Closure'))->hasMethod('__invoke'));

// ...but php does not keep it in the class's function table, so the two doors that
// print THAT do not name it -- and php's order puts `bind` before `bindTo`.
print_r(names(get_class_methods($c)));
print_r(names($ro->getMethods()));
print_r(names((new ReflectionClass('Closure'))->getMethods()));

// Reflected over an INSTANCE it describes the closure; the identity stays the method's.
$m = $ro->getMethod('__invoke');
var_dump($m->getName(), $m->class, $m->getDeclaringClass()->getName());
var_dump($m->isPublic(), $m->isStatic(), $m->getModifiers());
var_dump($m->isInternal(), $m->isUserDefined(), $m->getFileName(), $m->getStartLine());
var_dump($m->getExtensionName(), $m->getExtension());
var_dump($m->getNumberOfParameters(), $m->getNumberOfRequiredParameters(), (string) $m->getReturnType());
foreach ($m->getParameters() as $p) {
    printf("#%d %s \$%s type=%s optional=%d hasDefault=%d variadic=%d byRef=%d decl=%s::%s\n",
        $p->getPosition(), '', $p->getName(), (string) $p->getType(), $p->isOptional(),
        $p->isDefaultValueAvailable(), $p->isVariadic(), $p->isPassedByReference(),
        $p->getDeclaringClass()?->getName() ?? '-', $p->getDeclaringFunction()->getName());
    echo '   ', (string) $p, "\n";
}
echo $m->__toString();
var_dump($m->invoke($c, 'invoked'), get_class($m->getClosure($c)));

// A by-reference and a nullable parameter travel too.
$plain = function (&$ref, ?array $x = null) { };
foreach ((new ReflectionObject($plain))->getMethod('__invoke')->getParameters() as $p) {
    printf("%s type=%s byRef=%d\n", $p->getName(), (string) $p->getType(), $p->isPassedByReference());
}

// Named over the CLASS there is no closure to describe, and the CONSTRUCTOR refuses
// the name outright -- php's own split between its factory and its constructor.
$cm = (new ReflectionClass('Closure'))->getMethod('__invoke');
var_dump($cm->getName(), $cm->getNumberOfParameters(), (string) $cm->getReturnType());
echo $cm->__toString();
try { new ReflectionMethod('Closure', '__invoke'); }
catch (ReflectionException $e) { echo 'refused: ', $e->getMessage(), "\n"; }
// ...while the OBJECT form is what php builds.
var_dump((new ReflectionMethod($c, '__invoke'))->getNumberOfParameters());
?>
--EXPECT--
bool(true)
bool(true)
string(6) "called"
bool(true)
bool(true)
Array
(
    [0] => bind
    [1] => bindTo
    [2] => call
    [3] => fromCallable
)
Array
(
    [0] => __construct
    [1] => bind
    [2] => bindTo
    [3] => call
    [4] => fromCallable
    [5] => __invoke
)
Array
(
    [0] => __construct
    [1] => bind
    [2] => bindTo
    [3] => call
    [4] => fromCallable
    [5] => __invoke
)
string(8) "__invoke"
string(7) "Closure"
string(7) "Closure"
bool(true)
bool(false)
int(1)
bool(true)
bool(false)
bool(false)
bool(false)
bool(false)
NULL
int(3)
int(1)
string(6) "string"
#0  $a type=string optional=0 hasDefault=0 variadic=0 byRef=0 decl=Closure::__invoke
   Parameter #0 [ <required> string $a ]
#1  $b type=int optional=1 hasDefault=0 variadic=0 byRef=0 decl=Closure::__invoke
   Parameter #1 [ <optional> int $b = <default> ]
#2  $rest type= optional=1 hasDefault=0 variadic=1 byRef=0 decl=Closure::__invoke
   Parameter #2 [ <optional> ...$rest ]
Method [ <internal> public method __invoke ] {

  - Parameters [3] {
    Parameter #0 [ <required> string $a ]
    Parameter #1 [ <optional> int $b = <default> ]
    Parameter #2 [ <optional> ...$rest ]
  }
  - Return [ string ]
}
string(7) "invoked"
string(7) "Closure"
ref type= byRef=1
x type=?array byRef=0
string(8) "__invoke"
int(0)
string(0) ""
Method [ <internal> public method __invoke ] {
}
refused: Method Closure::__invoke() does not exist
int(3)
--CLEAN--
<?php
