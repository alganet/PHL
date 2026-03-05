--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_search strict mode returns false for type mismatch
--FILE--
<?php
$a = array(0, 1, 2);
$r = array_search('1', $a, true);
echo $r === false ? 'ok' : 'fail';
?>
--EXPECT--
ok
--CLEAN--
<?php
unset($a, $r);
