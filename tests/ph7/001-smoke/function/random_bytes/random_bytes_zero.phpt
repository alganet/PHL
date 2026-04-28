--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
random_bytes throws ValueError on length 0
--SKIPIF--
<?php
if (!function_exists('random_bytes')) { echo 'skip: random_bytes not available'; }
if (!class_exists('ValueError')) { echo 'skip: ValueError not available'; }
?>
--FILE--
<?php
try { random_bytes(0); echo "no_throw\n"; } catch (ValueError $e) { echo "ve\n"; }
?>
--EXPECT--
ve
--CLEAN--
<?php
