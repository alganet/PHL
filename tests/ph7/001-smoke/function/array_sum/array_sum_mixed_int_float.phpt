--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_sum with mixed integer and float values
--FILE--
<?php
$result = array_sum(array(1, 2, 3.5, 4));
echo abs($result - 10.5) < 0.001 ? 'OK' : 'FAIL';
?>
--EXPECT--
OK
--CLEAN--
<?php
unset($result);
