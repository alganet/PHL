--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
$GLOBALS by-ref param rejection and snapshot foreach (PHP 8.1)
--FILE--
<?php
// by-ref param rejects $GLOBALS with php's catchable Error
function takes_ref(&$g) { $g["inj"] = 1; }
try { takes_ref($GLOBALS); } catch (\Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
echo isset($GLOBALS["inj"]) ? "leaked" : "no-leak", "\n";
// foreach by value iterates a snapshot: loop-created globals not visited
$seen = 0;
foreach ($GLOBALS as $k => $v) { $seen++; $GLOBALS["gen_$seen"] = 1; }
$snapCount = $seen;
$total = count($GLOBALS);
echo $total > $snapCount ? "snapshot-iter-ok" : "iter-grew", "\n";
--EXPECT--
Error: takes_ref(): Argument #1 ($g) could not be passed by reference
no-leak
snapshot-iter-ok
--CLEAN--
<?php
foreach (array_keys($GLOBALS) as $k) { if (strpos($k, "gen_") === 0) { unset($GLOBALS[$k]); } }
unset($seen, $snapCount, $total, $k, $v);
