--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
time() should return current Unix timestamp
--FILE--
<?php
$t = time();
echo "is_int=" . (is_int($t) ? 'true' : 'false') . PHP_EOL;
echo "is_numeric=" . (is_numeric($t) ? 'true' : 'false') . PHP_EOL;
echo "gt_zero=" . ($t > 0 ? 'true' : 'false') . PHP_EOL;
?>
--EXPECT--
is_int=true
is_numeric=true
gt_zero=true