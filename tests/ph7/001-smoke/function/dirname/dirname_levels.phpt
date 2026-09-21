--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
dirname() honours $levels and rejects a level below 1
--FILE--
<?php
// $levels (php 7.0) used to be accepted and IGNORED, so every call answered the
// one-level parent. Each level re-runs the dirname walk and stops at the fixed
// point ("/" and "." are their own parents).
// The filesystem ROOT prints as the platform's own separator in BOTH engines ("\" on
// Windows, php's DEFAULT_SLASH), so normalise before printing.
foreach (["/a/b/c/d", "a/b/c", "file.txt", "a"] as $dnl_p) {
    $dnl_row = [];
    foreach ([1, 2, 3, 4, 99] as $dnl_lv) {
        $dnl_row[] = $dnl_lv . ':' . strtr(dirname($dnl_p, $dnl_lv), "\\", "/");
    }
    echo str_pad($dnl_p, 10), implode(' ', $dnl_row), "\n";
}
// the default is one level
var_dump(dirname("/a/b/c") === dirname("/a/b/c", 1));
// PHP_INT_MAX levels terminates at the root instead of walking forever
var_dump(strtr(dirname("/a/b/c", PHP_INT_MAX), "\\", "/"));
foreach ([0, -1, PHP_INT_MIN] as $dnl_bad) {
    try {
        dirname("/a/b", $dnl_bad);
        echo "NO_THROW\n";
    } catch (\ValueError $e) {
        echo $e->getMessage(), "\n";
    }
}
?>
--EXPECT--
/a/b/c/d  1:/a/b/c 2:/a/b 3:/a 4:/ 99:/
a/b/c     1:a/b 2:a 3:. 4:. 99:.
file.txt  1:. 2:. 3:. 4:. 99:.
a         1:. 2:. 3:. 4:. 99:.
bool(true)
string(1) "/"
dirname(): Argument #2 ($levels) must be greater than or equal to 1
dirname(): Argument #2 ($levels) must be greater than or equal to 1
dirname(): Argument #2 ($levels) must be greater than or equal to 1
--CLEAN--
<?php
unset($e);
