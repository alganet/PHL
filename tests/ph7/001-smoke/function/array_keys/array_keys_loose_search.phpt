--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_keys loose search matches across types
--FILE--
<?php
$a = array('a' => 0, 'b' => false, 'c' => null, 'd' => 1);
$k = array_keys($a, false);
echo implode(',', $k);
?>
--EXPECT--
a,b,c
--CLEAN--
<?php
unset($a, $k);
