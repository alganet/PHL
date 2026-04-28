--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
random_int throws ValueError when min is greater than max
--SKIPIF--
<?php
if (!function_exists('random_int')) { echo 'skip: random_int not available'; }
if (!class_exists('ValueError')) { echo 'skip: ValueError not available'; }
?>
--FILE--
<?php
try { random_int(10, 5); echo "no_throw\n"; } catch (ValueError $e) { echo "ve\n"; }
?>
--EXPECT--
ve
--CLEAN--
<?php
