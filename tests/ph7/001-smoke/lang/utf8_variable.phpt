--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: UTF-8 in variable names
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$héllo = "world";
echo $héllo . "\n";
?>
--EXPECT--
world
--CLEAN--
<?php
unset($héllo);
