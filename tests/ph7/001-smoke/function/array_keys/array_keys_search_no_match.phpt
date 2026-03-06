--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_keys returns empty array when search value is not found
--FILE--
<?php
$a = array('a' => 1, 'b' => 2);
$k = array_keys($a, 99);
echo count($k) === 0 ? 'ok' : 'fail';
?>
--EXPECT--
ok
--CLEAN--
<?php
unset($a, $k);
