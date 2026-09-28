--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
`f(...$x)` on something that is neither an array nor a Traversable is php's TypeError
--DESCRIPTION--
OP_SPREAD's fallback for a source it could not unpack was to leave the value on the stack as ONE
ordinary argument, so `f(...'str')` bound "str" to the first parameter and `new C(...null)` bound
null -- silently, on source php refuses to run. The array-literal site already raised php's
`Only arrays and Traversables can be unpacked, X given`; the argument site does now too, with the
class php picks there: TypeError for EVERY type, where the array-literal site keeps php's plain
Error for a scalar and TypeError only for an object.
--FILE--
<?php
function t($label, $fn) {
    echo "== $label\n";
    try { var_export($fn()); echo "\n"; } catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
}
function untaH($x = 1, $y = 2) { return "$x/$y"; }
class UntaB { public $got; function __construct($a = 0) { $this->got = $a; } }
class UntaM { public function m($x = 1) { return $x; } public static function s($x = 1) { return $x; } }

foreach (['string' => 'str', 'int' => 5, 'float' => 1.5, 'true' => true, 'null' => null,
          'stdClass' => new stdClass()] as $label => $v) {
    t("call ... $label",   fn() => untaH(...$v));
    t("new ... $label",    fn() => (new UntaB(...$v))->got);
    t("method ... $label", fn() => (new UntaM)->m(...$v));
    t("static ... $label", fn() => UntaM::s(...$v));
    t("array ... $label",  fn() => [...$v]);
}
?>
--EXPECT--
== call ... string
TypeError: Only arrays and Traversables can be unpacked, string given
== new ... string
TypeError: Only arrays and Traversables can be unpacked, string given
== method ... string
TypeError: Only arrays and Traversables can be unpacked, string given
== static ... string
TypeError: Only arrays and Traversables can be unpacked, string given
== array ... string
Error: Only arrays and Traversables can be unpacked, string given
== call ... int
TypeError: Only arrays and Traversables can be unpacked, int given
== new ... int
TypeError: Only arrays and Traversables can be unpacked, int given
== method ... int
TypeError: Only arrays and Traversables can be unpacked, int given
== static ... int
TypeError: Only arrays and Traversables can be unpacked, int given
== array ... int
Error: Only arrays and Traversables can be unpacked, int given
== call ... float
TypeError: Only arrays and Traversables can be unpacked, float given
== new ... float
TypeError: Only arrays and Traversables can be unpacked, float given
== method ... float
TypeError: Only arrays and Traversables can be unpacked, float given
== static ... float
TypeError: Only arrays and Traversables can be unpacked, float given
== array ... float
Error: Only arrays and Traversables can be unpacked, float given
== call ... true
TypeError: Only arrays and Traversables can be unpacked, true given
== new ... true
TypeError: Only arrays and Traversables can be unpacked, true given
== method ... true
TypeError: Only arrays and Traversables can be unpacked, true given
== static ... true
TypeError: Only arrays and Traversables can be unpacked, true given
== array ... true
Error: Only arrays and Traversables can be unpacked, true given
== call ... null
TypeError: Only arrays and Traversables can be unpacked, null given
== new ... null
TypeError: Only arrays and Traversables can be unpacked, null given
== method ... null
TypeError: Only arrays and Traversables can be unpacked, null given
== static ... null
TypeError: Only arrays and Traversables can be unpacked, null given
== array ... null
Error: Only arrays and Traversables can be unpacked, null given
== call ... stdClass
TypeError: Only arrays and Traversables can be unpacked, stdClass given
== new ... stdClass
TypeError: Only arrays and Traversables can be unpacked, stdClass given
== method ... stdClass
TypeError: Only arrays and Traversables can be unpacked, stdClass given
== static ... stdClass
TypeError: Only arrays and Traversables can be unpacked, stdClass given
== array ... stdClass
TypeError: Only arrays and Traversables can be unpacked, stdClass given
