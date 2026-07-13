--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
similar_text match counts and by-ref percent
--FILE--
<?php
echo similar_text("World", "Word"), "\n";
echo similar_text("", ""), "\n";
echo similar_text("abc", ""), "\n";
echo similar_text("Hello World", "Hello PHP World"), "\n";
echo similar_text("abcdef", "fedcba"), "\n";
echo similar_text("test", "text"), "\n";
// percent lands in an undefined variable through the by-ref out-param
similar_text("World", "Word", $simPct1);
echo round($simPct1, 6), "\n";
similar_text("", "", $simPct2);
echo round($simPct2, 6), "\n";
similar_text("aaa", "aaa", $simPct3);
echo round($simPct3, 6), "\n";
?>
--EXPECT--
4
0
0
11
1
3
88.888889
0
100
--CLEAN--
<?php
