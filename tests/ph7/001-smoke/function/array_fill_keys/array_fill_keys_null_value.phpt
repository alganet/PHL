--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_fill_keys: null fill value is preserved
--FILE--
<?php
$a = array_fill_keys(array('x'), null);
echo ($a['x'] === null ? 'true' : 'false') . PHP_EOL;
?>
--EXPECT--
true
--CLEAN--
<?php
unset($a);
