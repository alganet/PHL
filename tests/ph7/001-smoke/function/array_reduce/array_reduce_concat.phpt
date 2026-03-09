--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_reduce concatenates strings with initial value
--FILE--
<?php
$result = array_reduce(array('a', 'b', 'c'), function($carry, $item) { return $carry . $item; }, '');
echo $result;
?>
--EXPECT--
abc
--CLEAN--
<?php
unset($result);
