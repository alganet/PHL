--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
the bare-return compile check leaves every shape php still accepts alone
--FILE--
<?php
// Untyped: `return;` has always meant `return null;`.
function untyped() { return; }
var_dump(untyped());

// `void` is what a bare return MEANS, so it is the one declaration exempt.
function isVoid(): void { return; }
var_dump(isVoid());

class C {
    public function m(): void { return; }
    // A generator's declared type describes the Generator the CALL produces,
    // never the value `return;` hands to getReturn().
    public function gen(): Generator { yield 1; return; }
}
$c = new C;
var_dump($c->m());
$g = $c->gen();
foreach ($g as $v) { echo "yield $v\n"; }
var_dump($g->getReturn());

function genFn(): iterable { if (true) { return; } yield 2; }
foreach (genFn() as $v) { echo "never $v\n"; }
echo "gen ok\n";

// The check looks at the INNERMOST function: an untyped closure inside a
// typed one keeps its bare return.
function outer(): int {
    $inner = function () { return; };
    var_dump($inner());
    return 7;
}
var_dump(outer());

// A bare return inside a catch/finally of an untyped function still compiles.
function guarded() {
    try { throw new Exception('x'); }
    catch (Exception $e) { return; }
    finally { echo "finally\n"; }
}
var_dump(guarded());
?>
--EXPECT--
NULL
NULL
NULL
yield 1
NULL
gen ok
NULL
int(7)
finally
NULL
--CLEAN--
<?php
