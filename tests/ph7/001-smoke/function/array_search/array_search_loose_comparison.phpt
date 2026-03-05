--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_search loose mode matches with type coercion
--FILE--
<?php
$a = array('x', 0, 'y');
$r = array_search(false, $a);
echo $r === 1 ? 'ok' : 'fail';
?>
--EXPECT--
ok
--CLEAN--
<?php
unset($a, $r);
