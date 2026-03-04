--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_sum with int and float-like string promotes to DoubleSum
--FILE--
<?php
$result = array_sum(array(1, "2.5"));
echo abs($result - 3.5) < 0.001 ? 'OK' : 'FAIL';
?>
--EXPECT--
OK
--CLEAN--
<?php
unset($result);
