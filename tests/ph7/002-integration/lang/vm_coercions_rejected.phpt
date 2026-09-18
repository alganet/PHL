--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Deprecated VM coercions (float offset, false->array, dynamic prop, string ++/--) are rejected
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip PHL removes what php only deprecates'; ?>
--DESCRIPTION--
php only DEPRECATES these; PHL targets php's non-deprecated surface and rejects them. A NULL
offset is the exception — it deprecates + coerces to "" on both engines now, so it moved to the
cross-engine null_array_offset.phpt and is no longer part of this PHL-only "rejected" set.
--FILE--
<?php
function tc(callable $c) { try { $c(); echo "NO-THROW\n"; } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; } }

// float array offset (read / write / literal); isset() stays lenient
tc(function () { $a = [1, 2]; return $a[1.5]; });
tc(function () { $a = []; $a[1.5] = 1; });
tc(function () { $a = [1.5 => "v"]; });
// false auto-converted to array (null still auto-vivifies)
tc(function () { $x = false; $x[] = 1; });
// dynamic property on a plain class
tc(function () { class Zc {} $o = new Zc; $o->w = 1; });
// non-numeric string ++ / --
tc(function () { $s = "abc"; $s++; });
tc(function () { $s = "abc"; $s--; });

// still valid: integral float offset, null auto-vivify, isset leniency, stdClass, numeric ++
$a = [0, 1, 2, 3];
echo $a[2.0], "\n";                    // integral float offset ok
$n = null; $n[] = 5; echo $n[0], "\n"; // null auto-vivifies
var_dump(isset($a[1.5]));              // isset stays lenient (no throw)
$o = new stdClass; $o->x = 9; echo $o->x, "\n";
$s = "5"; $s++; echo $s, "\n";
?>
--EXPECT--
TypeError: Cannot access offset of type float on array
TypeError: Cannot access offset of type float on array
TypeError: Cannot access offset of type float on array
Error: Cannot use a scalar value as an array
Error: Cannot create dynamic property Zc::$w
TypeError: Increment on a non-numeric string is not supported, use str_increment() instead
TypeError: Decrement on a non-numeric string is not supported
2
5
bool(true)
9
6
