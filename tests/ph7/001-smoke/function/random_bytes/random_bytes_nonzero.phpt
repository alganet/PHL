--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
random_bytes returns a buffer that was actually written (not all-NUL)
--SKIPIF--
<?php
if (!function_exists('random_bytes')) { echo 'skip: random_bytes not available'; }
?>
--FILE--
<?php
$s = random_bytes(64);
$zero = str_repeat("\0", 64);
echo (is_string($s) && strlen($s) === 64 && $s !== $zero) ? "nonzero_ok\n" : "nonzero_fail\n";
?>
--EXPECT--
nonzero_ok
--CLEAN--
<?php
