--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
scandir() $sorting_order is a two-way split, not a three-value enum
--FILE--
<?php
$sso_dir = sys_get_temp_dir() . '/phl_scandir_sort_' . getmypid();
mkdir($sso_dir);
foreach (['b.txt', 'a.txt', 'c.txt'] as $sso_n) {
    touch($sso_dir . '/' . $sso_n);
}
// Only SCANDIR_SORT_NONE leaves the order alone and only the exact
// SCANDIR_SORT_ASCENDING sorts ascending -- every OTHER value sorts DESCENDING.
// PHL treated anything it did not recognise as SORT_NONE.
foreach ([SCANDIR_SORT_ASCENDING, SCANDIR_SORT_DESCENDING, 3, -1, 99, PHP_INT_MAX] as $sso_o) {
    echo str_pad($sso_o, 20), implode(',', scandir($sso_dir, $sso_o)), "\n";
}
// SORT_NONE returns the same SET, in whatever order the directory yields
$sso_none = scandir($sso_dir, SCANDIR_SORT_NONE);
sort($sso_none);
echo 'none(sorted): ', implode(',', $sso_none), "\n";

foreach (['b.txt', 'a.txt', 'c.txt'] as $sso_n) {
    unlink($sso_dir . '/' . $sso_n);
}
rmdir($sso_dir);
?>
--EXPECT--
0                   .,..,a.txt,b.txt,c.txt
1                   c.txt,b.txt,a.txt,..,.
3                   c.txt,b.txt,a.txt,..,.
-1                  c.txt,b.txt,a.txt,..,.
99                  c.txt,b.txt,a.txt,..,.
9223372036854775807 c.txt,b.txt,a.txt,..,.
none(sorted): .,..,a.txt,b.txt,c.txt
--CLEAN--
<?php
