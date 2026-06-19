--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
iterable return type accepts array and Traversable, rejects others (PHP 7.4)
--FILE--
<?php
function rtiArr(): iterable { return [1, 2]; }
function rtiGen(): iterable { return (function () { yield 1; })(); }
function rtiBad(): iterable { return 5; }
echo is_array(rtiArr()) ? "arr_ok\n" : "arr_fail\n";
echo (rtiGen() instanceof Traversable) ? "gen_ok\n" : "gen_fail\n";
try { rtiBad(); } catch (TypeError $e) { echo "bad_rejected\n"; }
?>
--EXPECT--
arr_ok
gen_ok
bad_rejected
--CLEAN--
<?php
