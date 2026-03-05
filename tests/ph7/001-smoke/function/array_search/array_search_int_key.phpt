--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_search returns integer key when value is found in indexed array
--FILE--
<?php
$a = array('apple', 'banana', 'cherry');
$r = array_search('banana', $a);
echo $r === 1 ? 'ok' : 'fail';
?>
--EXPECT--
ok
--CLEAN--
<?php
unset($a, $r);
