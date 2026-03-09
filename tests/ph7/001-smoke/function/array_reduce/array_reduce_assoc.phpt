--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_reduce works with associative array values
--FILE--
<?php
$result = array_reduce(array('a' => 1, 'b' => 2, 'c' => 3), function($carry, $item) { return $carry + $item; }, 0);
echo $result;
?>
--EXPECT--
6
--CLEAN--
<?php
unset($result);
