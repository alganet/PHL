--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
hash_equals() constant-time comparison: equal, differing, and different-length
--FILE--
<?php
echo hash_equals("abc", "abc")  ? "eq\n"   : "ne\n";
echo hash_equals("abc", "abd")  ? "eq\n"   : "ne\n";
echo hash_equals("abc", "abcd") ? "eq\n"   : "ne\n";
echo hash_equals("", "")        ? "eq\n"   : "ne\n";
echo hash_equals("x", "")       ? "eq\n"   : "ne\n";
?>
--EXPECT--
eq
ne
ne
eq
ne
--CLEAN--
<?php
