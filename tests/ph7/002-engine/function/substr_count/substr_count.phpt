--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
substr_count counts occurrences non-overlapping
--FILE--
<?php
// non overlapping
echo substr_count('abcabc','ab') . "\n";
// overlapping pattern count should still be non-overlapping
echo substr_count('aaaa','aa') . "\n";
?>
--EXPECT--
2
2
