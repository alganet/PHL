--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_sum with float warns for nested array values via DoubleSum
--FILE--
<?php
$result = array_sum(array(1.5, array(99), 3.5));
echo abs($result - 5.0) < 0.001 ? 'OK' : 'FAIL';
?>
--EXPECTF--
Error [2]: array_sum(): Addition is not supported on type array in %s on line %s
OK
--CLEAN--
<?php
unset($result);
