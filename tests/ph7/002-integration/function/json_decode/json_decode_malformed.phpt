--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
json_decode: php-parity for objects, negatives, and malformed/strict input
--DESCRIPTION--
Regression cluster (was PH7-legacy divergent, now php-identical):
  * a JSON object decodes to a stdClass (no "always an associative array" warning);
    assoc=true still yields an array.
  * negative numbers tokenize (bare, in arrays, as object values, floats).
  * malformed input sets json_last_error() and returns null: empty string,
    garbage, a trailing value, and UNTERMINATED containers ('{', '[1,2').
--FILE--
<?php
// Object vs. associative array.
$obj = json_decode('{"n":-1,"f":-2.5,"child":{"x":1}}');
echo 'is_object=', is_object($obj) ? '1' : '0', "\n";
echo 'n=', $obj->n, ' f=', $obj->f, ' child.x=', $obj->child->x, "\n";
$arr = json_decode('{"a":[-1,-2,-3]}', true);
echo 'assoc=', json_encode($arr), "\n";

// Malformed / strict cases: null result AND json_last_error() set.
foreach (['', 'garbage', '"a":1', '{', '[1,2', '{"a":'] as $bad) {
    $r = json_decode($bad);
    echo ($r === null && json_last_error() !== 0) ? "err_ok\n" : "err_fail\n";
}

// A valid decode clears the error again.
json_decode('{"ok":true}');
echo 'cleared=', json_last_error() === 0 ? '1' : '0', "\n";
?>
--EXPECT--
is_object=1
n=-1 f=-2.5 child.x=1
assoc={"a":[-1,-2,-3]}
err_ok
err_ok
err_ok
err_ok
err_ok
err_ok
cleared=1
--CLEAN--
<?php
