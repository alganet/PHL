--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_diff_key basic test
--FILE--
<?php
$array1 = array('a' => 1, 'b' => 2, 'c' => 3);
$array2 = array('a' => 1, 'b' => 2);
$result = array_diff_key($array1, $array2);
echo isset($result['c']) && $result['c'] === 3 && count($result) === 1 ? 'OK' : 'FAIL';
?>
--EXPECT--
OK