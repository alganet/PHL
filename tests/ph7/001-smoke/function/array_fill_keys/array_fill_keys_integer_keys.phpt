--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_fill_keys: integer values become integer keys
--FILE--
<?php
$a = array_fill_keys(array(1, 2, 3), 'val');
echo implode(',', array_keys($a)) . PHP_EOL;
?>
--EXPECT--
1,2,3
--CLEAN--
<?php
unset($a);
