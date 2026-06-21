--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
catch block shares the enclosing variable scope (reads outer locals)
--FILE--
<?php
$x = "VAL";
$n = 41;
try {
    throw new Exception("e");
} catch (Exception $e) {
    echo $x . "\n";
    echo ($n + 1) . "\n";
}
?>
--EXPECT--
VAL
42
--CLEAN--
<?php
