--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_reduce on empty array without initial returns NULL
--FILE--
<?php
$result = array_reduce(array(), function($carry, $item) { return $carry + $item; });
echo ($result === NULL) ? 'NULL' : 'not null';
?>
--EXPECT--
NULL
--CLEAN--
<?php
unset($result);
