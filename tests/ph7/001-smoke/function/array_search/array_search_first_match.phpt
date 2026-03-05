--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_search returns first matching key when duplicates exist
--FILE--
<?php
$a = array('a' => 'x', 'b' => 'x', 'c' => 'x');
$r = array_search('x', $a);
echo $r === 'a' ? 'ok' : 'fail';
?>
--EXPECT--
ok
--CLEAN--
<?php
unset($a, $r);
