--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Curly brace array subscript (legacy syntax)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
$a = array('x' => 42);
echo $a{'x'} . "\n"; // should print 42
?>
--EXPECT--
42
--CLEAN--
<?php
unset($a);
?>
