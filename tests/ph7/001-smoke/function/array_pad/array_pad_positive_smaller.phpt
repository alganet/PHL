--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_pad with positive pad_size smaller than array returns copy
--FILE--
<?php
$r = array_pad(array(1, 2), 1, 'x');
echo implode(',', $r) . PHP_EOL;
?>
--EXPECT--
1,2
--CLEAN--
<?php
unset($r);
