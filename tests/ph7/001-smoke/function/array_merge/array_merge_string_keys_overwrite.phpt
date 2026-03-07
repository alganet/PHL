--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_merge overwrites string keys with later values
--FILE--
<?php
$a = array('k' => 'first');
$b = array('k' => 'second');
$c = array_merge($a, $b);
echo $c['k'];
?>
--EXPECT--
second
--CLEAN--
<?php
unset($a, $b, $c);
