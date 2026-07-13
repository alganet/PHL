--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
levenshtein distances, empty strings, case sensitivity, custom costs
--FILE--
<?php
echo levenshtein("kitten", "sitting"), "\n";
echo levenshtein("", "abc"), "\n";
echo levenshtein("a", ""), "\n";
echo levenshtein("abc", "abc"), "\n";
echo levenshtein("Kitten", "kitten"), "\n";
// custom insertion/replacement/deletion costs
echo levenshtein("flaw", "lawn", 2, 3, 4), "\n";
echo levenshtein("flaw", "lawn", 2), "\n";
echo levenshtein("gumbo", "gambol", 2, 1, 3), "\n";
echo levenshtein("frog", "fog", 100, 100, 100), "\n";
// no 255-byte limit in php 8
echo levenshtein(str_repeat("ab", 150), str_repeat("ba", 150)), "\n";
// numeric-string cost is accepted in weak mode
echo levenshtein("a", "b", "7"), "\n";
?>
--EXPECT--
3
3
1
0
1
6
3
3
100
2
1
--CLEAN--
<?php
