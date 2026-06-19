--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
true/false literal return types accept the matching boolean (PHP 8.2)
--FILE--
<?php
function rtLitTrue(): true { return true; }
function rtLitFalse(): false { return false; }
echo rtLitTrue() === true ? "true_ok\n" : "true_fail\n";
echo rtLitFalse() === false ? "false_ok\n" : "false_fail\n";
?>
--EXPECT--
true_ok
false_ok
--CLEAN--
<?php
