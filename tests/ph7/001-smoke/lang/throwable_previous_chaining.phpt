--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Throwable: Exception accepts an Error as previous
--FILE--
<?php
$inner = new Error("inner");
$outer = new Exception("outer", 0, $inner);
echo $outer->getMessage(), "\n";
$prev = $outer->getPrevious();
echo get_class($prev), ":", $prev->getMessage(), "\n";
echo ($prev instanceof Throwable) ? "throwable\n" : "not\n";
?>
--EXPECT--
outer
Error:inner
throwable
--CLEAN--
<?php
