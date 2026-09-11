--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
a parenthesised member access invoked as a call runs the property VALUE, not a method of that name
--FILE--
<?php
/* `($o->p)(...)` / `($o::$p)(...)` invoke the callable VALUE of the property
 * (php's variable-invocation), which is distinct from the method call `$o->p()`.
 * PHL used to compile the parenthesised form as a method call, failing with
 * "Call to undefined method". Regression for the PHPUnit Callback constraint's
 * `($this->callback)($other)`. */
class NpcInvoke {
    public $cb;
    public static $scb;
    public function __construct() {
        $this->cb  = fn ($n) => $n * 2;
        self::$scb = fn ($n) => $n + 100;
    }
    /* A real method of the same name must still win for the UNparenthesised call. */
    public function cb($n) { return "method:$n"; }
    public function callProp($n)   { return ($this->cb)($n); }
    public function callStatic($n) { return (self::$scb)($n); }
    public function callMethod($n) { return $this->cb($n); }
}
$o = new NpcInvoke();
echo $o->callProp(21), "\n";    // property closure -> 42
echo $o->callStatic(1), "\n";   // static property closure -> 101
echo $o->callMethod(5), "\n";   // real method -> method:5
/* Also outside a class: parenthesised element/variable invocation. */
$arr = ['f' => fn () => "elem"];
echo ($arr['f'])(), "\n";
?>
--EXPECT--
42
101
method:5
elem
--CLEAN--
<?php
