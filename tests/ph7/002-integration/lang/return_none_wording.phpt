--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
a typed function that returns NO value says "none returned", not "null returned"
--FILE--
<?php
class Oth {}
class Base {}
class P extends Base {
    public function retUnion(): self|false {}
    public function retStatic(): static {}
}
class Q extends P {}

function scalarRet(): int {}
function nullableRet(): ?string {}
function arrayRet(): array {}
function classRet(): Oth {}
function unreachableRet(): string { if (false) { return "x"; } }
// An explicit `return null` is a DIFFERENT program, and php words it differently.
function explicitNull(): int { return null; }

$show = function (callable $fn) {
    try { $fn(); echo "ok\n"; }
    catch (TypeError $e) { echo $e->getMessage(), "\n"; }
};

$show(fn() => scalarRet());
$show(fn() => nullableRet());
$show(fn() => arrayRet());
$show(fn() => classRet());
$show(fn() => unreachableRet());
$show(fn() => explicitNull());

$q = new Q;
$show(fn() => $q->retUnion());
$show(fn() => $q->retStatic());
?>
--EXPECT--
scalarRet(): Return value must be of type int, none returned
nullableRet(): Return value must be of type ?string, none returned
arrayRet(): Return value must be of type array, none returned
classRet(): Return value must be of type Oth, none returned
unreachableRet(): Return value must be of type string, none returned
explicitNull(): Return value must be of type int, null returned
P::retUnion(): Return value must be of type P|false, none returned
P::retStatic(): Return value must be of type Q, none returned
--CLEAN--
<?php
