--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_reduce on empty array with initial returns the initial value
--FILE--
<?php
$result = array_reduce(array(), function($carry, $item) { return $carry + $item; }, 42);
echo $result;
?>
--EXPECT--
42
--CLEAN--
<?php
unset($result);
