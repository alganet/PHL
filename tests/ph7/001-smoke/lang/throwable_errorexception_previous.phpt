--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Throwable: ErrorException accepts Error as previous
--FILE--
<?php
$inner = new Error("root cause");
$e = new ErrorException("outer", 0, 1, __FILE__, 10, $inner);
echo $e->getMessage(), "\n";
echo $e->getSeverity(), "\n";
$prev = $e->getPrevious();
echo get_class($prev), ":", $prev->getMessage(), "\n";
?>
--EXPECT--
outer
1
Error:root cause
--CLEAN--
<?php
