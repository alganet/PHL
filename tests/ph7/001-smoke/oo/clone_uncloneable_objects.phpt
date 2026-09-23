--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Cloning a Generator, a Fiber or an enum case is one catchable Error, both spellings
--FILE--
<?php
function cloneUncloneableGen() { yield 1; }
enum CloneUncloneableSuit { case Hearts; }

function cloneUncloneableTry(callable $f) {
    try { $o = $f(); echo "no-throw: ", get_class($o), "\n"; }
    catch (\Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
}

$gen = cloneUncloneableGen();
$fiber = new Fiber(function () {});
$case = CloneUncloneableSuit::Hearts;

// the operator and the call form answer identically
cloneUncloneableTry(fn() => clone $gen);
cloneUncloneableTry(fn() => clone($gen));
cloneUncloneableTry(fn() => clone $fiber);
cloneUncloneableTry(fn() => clone($fiber));
cloneUncloneableTry(fn() => clone $case);
cloneUncloneableTry(fn() => clone($case));

// the catch really is a catch: execution continues, and the originals still work
echo $gen->current(), "\n";
var_dump($case === CloneUncloneableSuit::Hearts);
echo "end\n";
?>
--EXPECT--
Error: Trying to clone an uncloneable object of class Generator
Error: Trying to clone an uncloneable object of class Generator
Error: Trying to clone an uncloneable object of class Fiber
Error: Trying to clone an uncloneable object of class Fiber
Error: Trying to clone an uncloneable object of class CloneUncloneableSuit
Error: Trying to clone an uncloneable object of class CloneUncloneableSuit
1
bool(true)
end
