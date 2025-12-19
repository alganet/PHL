--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_sum with floats
--FILE--
<?php
$result = array_sum(array(1.5, 2.5, 3.0));
echo abs($result - 7.0) < 0.001 ? 'OK' : 'FAIL';
?>
--EXPECT--
OK