--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_sum uses float arithmetic when a float appears after the first element
--FILE--
<?php
$result = array_sum(array(1, 2.5, 3));
echo abs($result - 6.5) < 0.001 ? 'OK' : 'FAIL';
?>
--EXPECT--
OK
--CLEAN--
<?php
unset($result);
