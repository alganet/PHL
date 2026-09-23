--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A Closure presents php's debug shape, and every key of it is conditional
--FILE--
<?php
// php's zend_closure_get_debug_info shows name/file/line for a real closure and a
// single `function` for a FAKE one (a first-class callable over a named function),
// then `static`, `this` and `parameter` only when each applies.
class ClosureShapeHost
{
	public function make($q)
	{
		return function (int $a, string $b = "x", &$c = null) use ($q) { };
	}
	private function priv($z) { }
	public function fcc() { return $this->priv(...); }
	public static function stat() { }
}

// A plain closure: three keys, nothing else.
$plain = function () { };
print_r($plain);
// Captures AND the body's own static, keyed without the `$`, before any call.
$k = 3;
$withState = function () use ($k) { static $seen = 'init'; };
print_r($withState);
// php's required cut runs through the LAST required parameter, so all three here
// are <required> even though the middle one has a default.
$oddCut = function ($a, $b = 1, $c) { };
print_r($oddCut);
// Declared in a method: the implicit $this shows up under `this`, and the by-ref
// parameter is keyed `&$c`.
print_r((new ClosureShapeHost)->make(7));
// A first-class callable is php's FAKE closure: one `function` key, no name/file/line.
print_r((new ClosureShapeHost)->fcc());
print_r(ClosureShapeHost::stat(...));
print_r(strtoupper(...));
// The other surfaces do NOT go through the debug handler: var_export prints
// __set_state, json an empty object, and `(array)` is php's SCALAR wrap.
var_export($plain); echo "\n";
echo json_encode($plain), "\n";
$cast = (array)$plain;
var_dump(count($cast), array_keys($cast), $cast[0] === $plain);
var_dump(get_object_vars($plain));
?>
--EXPECTF--
Closure Object
(
    [name] => {closure:%s:17}
    [file] => %s
    [line] => 17
)
Closure Object
(
    [name] => {closure:%s:21}
    [file] => %s
    [line] => 21
    [static] => Array
        (
            [k] => 3
            [seen] => init
        )

)
Closure Object
(
    [name] => {closure:%s:25}
    [file] => %s
    [line] => 25
    [parameter] => Array
        (
            [$a] => <required>
            [$b] => <required>
            [$c] => <required>
        )

)
Closure Object
(
    [name] => {closure:ClosureShapeHost::make():9}
    [file] => %s
    [line] => 9
    [static] => Array
        (
            [q] => 7
        )

    [this] => ClosureShapeHost Object
        (
        )

    [parameter] => Array
        (
            [$a] => <required>
            [$b] => <optional>
            [&$c] => <optional>
        )

)
Closure Object
(
    [function] => ClosureShapeHost::priv
    [this] => ClosureShapeHost Object
        (
        )

    [parameter] => Array
        (
            [$z] => <required>
        )

)
Closure Object
(
    [function] => ClosureShapeHost::stat
)
Closure Object
(
    [function] => strtoupper
    [parameter] => Array
        (
            [$string] => <required>
        )

)
\Closure::__set_state(array(
))
{}
int(1)
array(1) {
  [0]=>
  int(0)
}
bool(true)
array(0) {
}
--CLEAN--
<?php
