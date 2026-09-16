--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
usleep() rejects a negative delay with a ValueError
--FILE--
<?php
// The delay must never reach the sleep routine: cast to unsigned it would be a
// multi-thousand-second hang rather than a few hundred microseconds.
try { usleep(-1000); } catch (ValueError $e) { echo $e->getMessage(), "\n"; }
$start = microtime(true);
var_dump(usleep(0));
echo (microtime(true) - $start) < 1 ? "returned promptly\n" : "SLEPT\n";
?>
--EXPECT--
usleep(): Argument #1 ($microseconds) must be greater than or equal to 0
NULL
returned promptly
--CLEAN--
<?php
unset($start);
