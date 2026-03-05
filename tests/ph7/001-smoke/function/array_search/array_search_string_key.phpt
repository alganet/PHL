--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_search returns string key when value is found in associative array
--FILE--
<?php
$a = array('foo' => 'bar', 'baz' => 'qux');
$r = array_search('qux', $a);
echo $r === 'baz' ? 'ok' : 'fail';
?>
--EXPECT--
ok
--CLEAN--
<?php
unset($a, $r);
