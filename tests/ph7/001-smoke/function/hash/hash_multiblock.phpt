--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
hash() across block boundaries (>64 and >128 bytes), case-insensitive algos, raw output
--FILE--
<?php
$big = str_repeat("a", 200); // spans multiple 64- and 128-byte blocks
echo hash("sha256", $big), "\n";
echo hash("sha512", $big), "\n";
// PHP accepts the algorithm name case-insensitively
echo hash("SHA256", "abc") === hash("sha256", "abc") ? "case-insensitive\n" : "FAIL\n";
// raw binary output lengths
echo strlen(hash("sha256", "x", true)),
     strlen(hash("sha512", "x", true)),
     strlen(hash("sha224", "x", true)),
     strlen(hash("sha384", "x", true)), "\n";
?>
--EXPECT--
c2a908d98f5df987ade41b5fce213067efbcc21ef2240212a41e54b5e7c28ae5
4b11459c33f52a22ee8236782714c150a3b2c60994e9acee17fe68947a3e6789f31e7668394592da7bef827cddca88c4e6f86e4df7ed1ae6cba71f3e98faee9f
case-insensitive
32642848
--CLEAN--
<?php
