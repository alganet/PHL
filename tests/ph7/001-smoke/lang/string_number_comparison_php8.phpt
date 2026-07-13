--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHP 8 saner string-to-number comparisons
--FILE--
<?php
// A number compared with a NON-numeric string is compared as strings (the number
// cast to its string form). Only a numeric string triggers a numeric comparison.
// Bools printed via (int) as 1/0 to dodge the var_dump bool-casing divergence.

// number vs non-numeric string -> string comparison
echo (int)(0 == "abc"), "\n";     // 0  ("0" != "abc")
echo (int)(0 == ""), "\n";        // 0  ("0" != "")   (PHP 8: was true in PHP 7)
echo (int)("abc" < 10), "\n";     // 0  ("abc" > "10")
echo (int)(10 < "abc"), "\n";     // 1
echo ("abc" <=> 10), "\n";        // 1

// number vs numeric string -> numeric comparison (unchanged)
echo (int)(0 == "0"), "\n";       // 1
echo (int)("10" == 10), "\n";     // 1
echo (int)(1.5 < "2"), "\n";      // 1

// reduction / sort use the same comparator
echo max("abc", 10), "\n";    // abc
echo min("abc", 10), "\n";    // 10
$a = [10, "abc", 2];
sort($a);
echo implode(",", $a), "\n";  // 2,10,abc
?>
--EXPECT--
0
0
0
1
1
1
1
1
abc
10
2,10,abc
--CLEAN--
<?php
