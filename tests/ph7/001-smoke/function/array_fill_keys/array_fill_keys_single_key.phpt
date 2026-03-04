--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_fill_keys: single element array
--FILE--
<?php
$a = array_fill_keys(array('only'), 42);
echo $a['only'] . PHP_EOL;
?>
--EXPECT--
42
--CLEAN--
<?php
unset($a);
