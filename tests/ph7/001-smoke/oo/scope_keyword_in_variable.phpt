--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
self, static and parent held in a variable are class names, not keywords
--FILE--
<?php
// The three resolve only where they are WRITTEN. Reaching `new`, `::`, a
// first-class callable, `instanceof` or a closure's bound scope through a
// variable, the string is looked up as a class, and no class can be called that.
set_error_handler(function ($n, $s) { echo "E$n: $s\n"; return true; });
function skv_try(callable $f) {
    try { $r = $f(); echo is_object($r) ? get_class($r) : var_export($r, true), "\n"; }
    catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
}
class SkvP { const K = 'PK'; public static $p = 'Pp'; static function s() { return 'P::s'; } }
trait SkvT {
    function inTrait() { $c = 'self'; return [$this instanceof self, $this instanceof $c]; }
}
class SkvA extends SkvP {
    use SkvT;
    const K = 'AK';
    public static $p = 'Ap';
    function __construct($a = null) {}
    static function s() { return 'A::s'; }
    function run() {
        foreach (['self', 'static', 'parent'] as $c) {
            echo "-- $c\n";
            skv_try(fn() => new $c);
            skv_try(fn() => new $c(1));
            skv_try(fn() => $c::s());
            skv_try(fn() => $c::K);
            skv_try(fn() => $c::$p);
            skv_try(fn() => ($c::s(...))());
            skv_try(fn() => $this instanceof $c);
            skv_try(fn() => (function () { return 1; })->bindTo($this, $c));
            skv_try(fn() => Closure::bind(function () { return 1; }, null, $c));
        }
        echo "-- written\n";
        $m = 's';
        var_dump(new self instanceof self, new static(1) instanceof parent, new parent instanceof SkvP);
        var_dump(self::s(), static::s(), parent::s(), self::$m(), parent::$m());
        var_dump(self::K, parent::K, static::$p, (self::s(...))(), (parent::s(...))());
        var_dump($this instanceof self, $this instanceof static, $this instanceof parent);
        var_dump($this->inTrait());
    }
}
(new SkvA)->run();
--EXPECT--
-- self
Error: Class "self" not found
Error: Class "self" not found
Error: Class "self" not found
Error: Class "self" not found
Error: Class "self" not found
Error: Class "self" not found
false
E2: Class "self" not found
NULL
E2: Class "self" not found
NULL
-- static
Error: Class "static" not found
Error: Class "static" not found
Error: Class "static" not found
Error: Class "static" not found
Error: Class "static" not found
Error: Class "static" not found
false
Closure
Closure
-- parent
Error: Class "parent" not found
Error: Class "parent" not found
Error: Class "parent" not found
Error: Class "parent" not found
Error: Class "parent" not found
Error: Class "parent" not found
false
E2: Class "parent" not found
NULL
E2: Class "parent" not found
NULL
-- written
bool(true)
bool(true)
bool(true)
string(4) "A::s"
string(4) "A::s"
string(4) "P::s"
string(4) "A::s"
string(4) "P::s"
string(2) "AK"
string(2) "PK"
string(2) "Ap"
string(4) "A::s"
string(4) "P::s"
bool(true)
bool(true)
bool(true)
array(2) {
  [0]=>
  bool(true)
  [1]=>
  bool(false)
}
