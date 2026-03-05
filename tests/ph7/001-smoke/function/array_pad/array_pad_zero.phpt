--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_pad with pad_size zero returns copy of array
--FILE--
<?php
$r = array_pad(array(1, 2), 0, 'x');
echo implode(',', $r) . PHP_EOL;
?>
--EXPECT--
1,2
--CLEAN--
<?php
unset($r);
