--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_sum with float-like strings uses DoubleSum
--FILE--
<?php
$result = array_sum(array("2.5", "3.5"));
echo abs($result - 6.0) < 0.001 ? 'OK' : 'FAIL';
?>
--EXPECT--
OK
--CLEAN--
<?php
unset($result);
