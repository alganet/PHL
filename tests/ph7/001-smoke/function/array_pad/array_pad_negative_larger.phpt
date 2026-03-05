--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_pad with negative pad_size larger than array pads at beginning
--FILE--
<?php
$r = array_pad(array(1, 2), -5, 'x');
echo implode(',', $r) . PHP_EOL;
?>
--EXPECT--
x,x,x,1,2
--CLEAN--
<?php
unset($r);
