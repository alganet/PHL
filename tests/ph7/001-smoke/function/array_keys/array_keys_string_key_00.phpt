--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_keys preserves '00' as a string key distinct from integer 0
--FILE--
<?php
$a = array('00' => 'str', 0 => 'int');
$k = array_keys($a);
echo $k[0] . ',' . $k[1];
?>
--EXPECT--
00,0
--CLEAN--
<?php
unset($a, $k);
