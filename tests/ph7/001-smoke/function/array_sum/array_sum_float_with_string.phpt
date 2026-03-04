--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_sum with float first element and numeric string uses DoubleSum string path
--FILE--
<?php
$result = array_sum(array(1.5, "2"));
echo abs($result - 3.5) < 0.001 ? 'OK' : 'FAIL';
?>
--EXPECT--
OK
--CLEAN--
<?php
unset($result);
