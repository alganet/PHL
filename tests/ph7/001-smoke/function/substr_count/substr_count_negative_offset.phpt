--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
substr_count with a negative offset counts from the end of the haystack
--FILE--
<?php
// -5 => start at "world"; one 'o'.
echo substr_count("hello world", "o", -5), "\n";
// negative offset combined with a negative length window: window is "abcab" -> one "bc".
echo substr_count("abcabcabc", "bc", -6, -1), "\n";
?>
--EXPECT--
1
1
--CLEAN--
<?php
