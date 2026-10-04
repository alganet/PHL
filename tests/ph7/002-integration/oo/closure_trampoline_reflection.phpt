--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Reflection describes a call trampoline closure as the internal function php builds
--FILE--
<?php
class CtrBase {
    public function __call($n, $a) { return "call $n"; }
    public static function __callStatic($n, $a) { return "callStatic $n"; }
}
class CtrKid extends CtrBase {}

$o = new CtrKid;
$cases = [
    'syntax, __call'             => $o->gone(...),
    'syntax, __callStatic'       => CtrKid::gone(...),
    'fromCallable, __call'       => Closure::fromCallable([$o, 'gone']),
    'fromCallable, __callStatic' => Closure::fromCallable('CtrKid::gone'),
];
foreach ($cases as $label => $c) {
    echo "-- $label\n";
    $r = new ReflectionFunction($c);
    var_dump($r->getName(), $r->getNumberOfParameters(), $r->getNumberOfRequiredParameters());
    var_dump($r->isVariadic(), $r->isStatic(), $r->isInternal(), $r->isUserDefined());
    var_dump($r->getExtensionName(), $r->getFileName(), $r->getStartLine());
    foreach ($r->getParameters() as $p) {
        var_dump($p->getName(), $p->getPosition(), $p->isVariadic(), $p->isOptional());
        var_dump($p->hasType(), (string) $p->getType(), $p->allowsNull());
        var_dump($p->isDefaultValueAvailable(), $p->isPassedByReference());
        var_dump($p->getDeclaringClass()->name, $p->getDeclaringFunction()->name);
        echo $p, "\n";
    }
    echo $r;
}

echo "-- ReflectionParameter over the trampoline itself\n";
$p = new ReflectionParameter($o->gone(...), 'arguments');
var_dump($p->getPosition(), $p->getDeclaringClass()->name);
$p = new ReflectionParameter(CtrKid::gone(...), 0);
var_dump($p->getName(), $p->isVariadic());
try {
    new ReflectionParameter(Closure::fromCallable([$o, 'gone']), 0);
} catch (ReflectionException $e) {
    echo get_class($e), ": ", $e->getMessage(), "\n";
}
--EXPECT--
-- syntax, __call
string(4) "gone"
int(1)
int(0)
bool(true)
bool(false)
bool(true)
bool(false)
bool(false)
bool(false)
bool(false)
string(9) "arguments"
int(0)
bool(true)
bool(true)
bool(true)
string(5) "mixed"
bool(true)
bool(false)
bool(false)
string(7) "CtrBase"
string(4) "gone"
Parameter #0 [ <optional> mixed ...$arguments ]
Closure [ <internal> public method gone ] {

  - Parameters [1] {
    Parameter #0 [ <optional> mixed ...$arguments ]
  }
}
-- syntax, __callStatic
string(4) "gone"
int(1)
int(0)
bool(true)
bool(true)
bool(true)
bool(false)
bool(false)
bool(false)
bool(false)
string(9) "arguments"
int(0)
bool(true)
bool(true)
bool(true)
string(5) "mixed"
bool(true)
bool(false)
bool(false)
string(7) "CtrBase"
string(4) "gone"
Parameter #0 [ <optional> mixed ...$arguments ]
Closure [ <internal> static public method gone ] {

  - Parameters [1] {
    Parameter #0 [ <optional> mixed ...$arguments ]
  }
}
-- fromCallable, __call
string(4) "gone"
int(0)
int(0)
bool(false)
bool(false)
bool(true)
bool(false)
bool(false)
bool(false)
bool(false)
Closure [ <internal> public method gone ] {
}
-- fromCallable, __callStatic
string(4) "gone"
int(0)
int(0)
bool(false)
bool(true)
bool(true)
bool(false)
bool(false)
bool(false)
bool(false)
Closure [ <internal> static public method gone ] {
}
-- ReflectionParameter over the trampoline itself
int(0)
string(7) "CtrBase"
string(9) "arguments"
bool(true)
ReflectionException: The parameter specified by its offset could not be found
