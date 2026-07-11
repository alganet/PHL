--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
date()/gmdate(): $timestamp follows php 8 ?int weak ZPP (coerce or TypeError)
--FILE--
<?php
// A non-numeric string, an array, or an object throws a catchable TypeError;
// the value name follows php's convention (string/array/class-name).
foreach (["12abc", "0x1A", "", "  "] as $s) {
    try { date("Y", $s); } catch (\TypeError $e) { echo $e->getMessage(), "\n"; }
}
try { date("Y", []); }          catch (\TypeError $e) { echo $e->getMessage(), "\n"; }
try { date("Y", new stdClass); } catch (\TypeError $e) { echo $e->getMessage(), "\n"; }
try { gmdate("Y", "nope"); }    catch (\TypeError $e) { echo $e->getMessage(), "\n"; }
try { gmdate("Y", []); }        catch (\TypeError $e) { echo $e->getMessage(), "\n"; }

// Coercible values (int / numeric string / exponent / signed / bool) resolve to
// a Unix timestamp. Assert via gmdate (UTC) so the value is timezone-independent
// and byte-identical to php; whole values only (no float->int deprecation).
echo gmdate("Y-m-d H:i:s", 100), "\n";
echo gmdate("Y-m-d H:i:s", 0), "\n";
echo gmdate("Y-m-d H:i:s", -5), "\n";
echo gmdate("Y-m-d H:i:s", "100"), "\n";
echo gmdate("Y-m-d H:i:s", " 100 "), "\n";
echo gmdate("Y-m-d H:i:s", "1e3"), "\n";
echo gmdate("Y-m-d H:i:s", "+5"), "\n";
echo gmdate("Y-m-d H:i:s", true), "\n";
echo gmdate("Y-m-d H:i:s", false), "\n";

// null is accepted (nullable) and means "current time", like an omitted arg.
echo gmdate("Y-m-d", null) === gmdate("Y-m-d") ? "null-is-now\n" : "null-bad\n";
?>
--EXPECT--
date(): Argument #2 ($timestamp) must be of type ?int, string given
date(): Argument #2 ($timestamp) must be of type ?int, string given
date(): Argument #2 ($timestamp) must be of type ?int, string given
date(): Argument #2 ($timestamp) must be of type ?int, string given
date(): Argument #2 ($timestamp) must be of type ?int, array given
date(): Argument #2 ($timestamp) must be of type ?int, stdClass given
gmdate(): Argument #2 ($timestamp) must be of type ?int, string given
gmdate(): Argument #2 ($timestamp) must be of type ?int, array given
1970-01-01 00:01:40
1970-01-01 00:00:00
1969-12-31 23:59:55
1970-01-01 00:01:40
1970-01-01 00:01:40
1970-01-01 00:16:40
1970-01-01 00:00:05
1970-01-01 00:00:01
1970-01-01 00:00:00
null-is-now
--CLEAN--
<?php
