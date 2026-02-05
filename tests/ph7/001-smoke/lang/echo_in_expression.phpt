--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Echo in expression context (or/and short-circuit)
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test echo in expression context, triggering LOADC for boolean true
// Covers PH7_CompileLangConstruct echo handling in expression (line ~720)

$x = false;
$x or echo "false or executed\n";

$y = true;
$y and echo "true and executed\n";

$z = false;
$z or echo "second false or\n";

echo "Done\n";
?>
--EXPECT--
false or executed
true and executed
second false or
Done
--CLEAN--
<?php
unset($x, $y, $z);
