--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
String literal reuse
--FILE--
<?php
// Use the same string literal multiple times to test literal caching
$s1 = 'hello world';
$s2 = 'hello world';
$s3 = 'hello world';
if ($s1 === $s2 && $s2 === $s3) echo "PASS\n";
?>
--EXPECT--
PASS
--CLEAN--
<?php
unset($s1, $s2, $s3);
