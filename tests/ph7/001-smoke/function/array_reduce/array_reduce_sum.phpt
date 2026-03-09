--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_reduce sums numeric values with initial value
--FILE--
<?php
$result = array_reduce(array(1, 2, 3, 4), function($carry, $item) { return $carry + $item; }, 0);
echo $result;
?>
--EXPECT--
10
--CLEAN--
<?php
unset($result);
