--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_reduce without initial value starts with NULL carry
--FILE--
<?php
$result = array_reduce(array('a', 'b', 'c'), function($carry, $item) { return $carry . '(' . $item . ')'; });
echo $result;
?>
--EXPECT--
(a)(b)(c)
--CLEAN--
<?php
unset($result);
