--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The constant-expression forms php still accepts keep compiling once calls are rejected (guards the call scanner against false positives)
--FILE--
<?php
class Ok {
    const PLAIN  = 1 + 2;
    const ARR    = [1, 2];
    const REF    = self::PLAIN * 2;
    const FCC    = strlen(...);           // first-class callable: not a call
    const CLOSED = static function () { return strlen('deferred'); }; // returns 8
}
const G_CONCAT  = 'a' . 'b';
const G_TERNARY = true ? 'yes' : 'no';
const G_NEW     = 1 > 0;

echo Ok::PLAIN, Ok::ARR[1], Ok::REF, "\n";
echo (Ok::FCC)('abcd'), "\n";
echo (Ok::CLOSED)(), "\n";
echo G_CONCAT, ' ', G_TERNARY, ' ', var_export(G_NEW, true), "\n";

class Defaults {
    public $plain = [1, 2];
    public $ref   = Ok::PLAIN;
}
$d = new Defaults();
echo $d->plain[0], $d->ref, "\n";

function withDefaults($a = 5, $b = Ok::PLAIN, $c = [1, 2]) { return $a + $b + $c[1]; }
echo withDefaults(), "\n";
?>
--EXPECT--
326
4
8
ab yes true
13
10
--CLEAN--
<?php
