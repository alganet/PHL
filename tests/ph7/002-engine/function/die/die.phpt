--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
die with string message
--SKIPIF--
<?php
if (!function_exists('die')) { echo 'skip: die not available'; }
?>
--FILE--
<?php
echo "before_die\n";
die("die_message\n");
echo "after_die\n";
?>
--EXPECT--
before_die
die_message

