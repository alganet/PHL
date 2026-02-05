--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
string interpolation with curly syntax ${expr}
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$name = "world";
echo "Hello ${name}!";
?>
--EXPECT--
Hello world!
--CLEAN--
<?php
unset($name);
