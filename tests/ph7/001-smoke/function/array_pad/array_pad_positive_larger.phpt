--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_pad with positive pad_size larger than array pads at end
--FILE--
<?php
$r = array_pad(array(1, 2), 5, 'x');
echo implode(',', $r) . PHP_EOL;
?>
--EXPECT--
1,2,x,x,x
--CLEAN--
<?php
unset($r);
