--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A throwing comparator abandons the u-variant builtin with no partial result
--FILE--
<?php
// A throwing comparison callback abandons the whole builtin: the enclosing
// catch runs, no partial result is produced, and the statements after the
// catch still execute.
try {
    $r = array_udiff_assoc(["a" => 1, "b" => 2], ["a" => 1], function ($x, $y) { throw new RuntimeException("boom"); });
    var_dump($r);
} catch (RuntimeException $e) { echo "caught ", $e->getMessage(), "\n"; }
try {
    $r = array_intersect_ukey(["a" => 1], ["a" => 2], function ($x, $y) { throw new LogicException("kaboom"); });
    var_dump($r);
} catch (LogicException $e) { echo "caught ", $e->getMessage(), "\n"; }
try {
    $r = array_uintersect_uassoc(["a" => 1], ["a" => 1], function ($x, $y) { return 0; }, function ($x, $y) { throw new DomainException("halt"); });
    var_dump($r);
} catch (DomainException $e) { echo "caught ", $e->getMessage(), "\n"; }
echo "after-all\n";
?>
--EXPECT--
caught boom
caught kaboom
caught halt
after-all
