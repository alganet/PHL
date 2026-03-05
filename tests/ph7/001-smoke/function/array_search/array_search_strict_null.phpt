--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_search strict mode finds null value
--FILE--
<?php
$a = array('a' => null, 'b' => 1);
$r = array_search(null, $a, true);
echo $r === 'a' ? 'ok' : 'fail';
?>
--EXPECT--
ok
--CLEAN--
<?php
unset($a, $r);
