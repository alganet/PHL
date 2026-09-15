--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
String offset assignment writes in place; [] on a string is an Error
--DESCRIPTION--
php raises "[] operator not supported for strings" and leaves the string untouched. PHL
silently APPENDED, so this test asserted "AZ" from behind a bare skip -- a wrong answer
that never ran under the oracle. Offset assignment ($s[1] = 'X') is unaffected.
--FILE--
<?php
$s = "abc";
$s[1] = 'X';
echo $s . "\n";
$s2 = "A";
try {
    $s2[] = 'Z';
} catch (\Error $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
echo $s2 . "\n";
?>
--EXPECT--
aXc
Error: [] operator not supported for strings
A
--CLEAN--
<?php
unset($s, $s2);
