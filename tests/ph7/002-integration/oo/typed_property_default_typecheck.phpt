--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed instance-property defaults are validated at instantiation (constant rule, catchable)
--FILE--
<?php
// php validates a typed property DEFAULT with the typed-CONSTANT rule — the
// only implicit coercion is int -> float widening; even a numeric string is
// a TypeError ("Cannot assign string to property C::$p of type int") — and
// throws it CATCHABLY when the default materializes at `new`. A class whose
// bad default is never instantiated stays silent.
const TpdStr = "abc";
const TpdNum = "5";
const TpdFrac = 1.5;
const TpdInt = 1;

class TpdBadStr  { public int $p = TpdStr; }
class TpdBadNum  { public int $p = TpdNum; }
class TpdBadFrac { public int $p = TpdFrac; }
class TpdBadBool { public bool $b = TpdInt; }
class TpdNever   { public int $p = TpdStr; } // never instantiated: no error
class TpdOkBase  {}
class TpdOk {
    public float $f = TpdInt;   // int -> float widening allowed
    public ?int $n = null;
    public bool $b = true;
    public float|string $u = TpdFrac;
    public ?TpdOkBase $o = null;
}
class TpdDerived extends TpdOk {}

foreach ([TpdBadStr::class, TpdBadNum::class, TpdBadFrac::class, TpdBadBool::class] as $cls) {
    try {
        new $cls;
        echo "$cls: no error\n";
    } catch (TypeError $e) {
        echo $e->getMessage(), "\n";
    }
}
$o = new TpdDerived;
var_dump($o->f, $o->n, $o->b, $o->u, $o->o);

// Two bad defaults: construction aborts at the FIRST, exactly one throw.
class TpdTwoBad { public int $a = TpdStr; public bool $b = TpdFrac; }
try { new TpdTwoBad; } catch (TypeError $e) { echo $e->getMessage(), "\n"; }

// The constructor must NOT run when a default fails.
class TpdCtorSkip {
    public int $p = TpdStr;
    public function __construct() { echo "ctor ran\n"; }
}
try { new TpdCtorSkip; } catch (TypeError $e) { echo $e->getMessage(), "\n"; }

// A THROWING initializer keeps its own error — no spurious TypeError after it.
class TpdInitThrow { public int $p = TPD_NO_SUCH_CONST; }
try { new TpdInitThrow; } catch (Throwable $e) {
    echo get_class($e), ": ", $e->getMessage(), "\n";
}
echo "ok\n";
?>
--EXPECT--
Cannot assign string to property TpdBadStr::$p of type int
Cannot assign string to property TpdBadNum::$p of type int
Cannot assign float to property TpdBadFrac::$p of type int
Cannot assign int to property TpdBadBool::$b of type bool
float(1)
NULL
bool(true)
float(1.5)
NULL
Cannot assign string to property TpdTwoBad::$a of type int
Cannot assign string to property TpdCtorSkip::$p of type int
Error: Undefined constant "TPD_NO_SUCH_CONST"
ok
--CLEAN--
<?php
