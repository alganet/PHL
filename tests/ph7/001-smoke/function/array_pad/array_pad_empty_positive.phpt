--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_pad with empty array and positive pad_size fills entirely
--FILE--
<?php
$r = array_pad(array(), 3, 'x');
echo implode(',', $r) . PHP_EOL;
?>
--EXPECT--
x,x,x
--CLEAN--
<?php
unset($r);
