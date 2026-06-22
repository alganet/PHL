--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
++/-- on an initialized readonly property is a (catchable) Error; the value is unchanged
--FILE--
<?php
class RoIncr {
    public function __construct(public readonly int $x) {}
}
$o = new RoIncr(5);
try { $o->x++; } catch (\Error $e) { echo $e->getMessage(), "\n"; }
try { $o->x--; } catch (\Error $e) { echo $e->getMessage(), "\n"; }
try { ++$o->x; } catch (\Error $e) { echo $e->getMessage(), "\n"; }
try { --$o->x; } catch (\Error $e) { echo $e->getMessage(), "\n"; }
echo $o->x, "\n";
?>
--EXPECT--
Cannot modify readonly property RoIncr::$x
Cannot modify readonly property RoIncr::$x
Cannot modify readonly property RoIncr::$x
Cannot modify readonly property RoIncr::$x
5
--CLEAN--
<?php
