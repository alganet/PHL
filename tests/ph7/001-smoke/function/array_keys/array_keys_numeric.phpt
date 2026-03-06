--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_keys returns numeric indices for indexed array
--FILE--
<?php
$a = array('a', 'b', 'c');
$k = array_keys($a);
echo implode(',', $k);
?>
--EXPECT--
0,1,2
--CLEAN--
<?php
unset($a, $k);
