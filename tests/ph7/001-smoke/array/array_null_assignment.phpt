--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array null value assignment
--FILE--
<?php
$array = array('a' => 1, 'b' => 2);
$array['a'] = null;
var_dump($array['a'] === null);
var_dump(count($array));
?>
--EXPECT--
bool(true)
int(2)
--CLEAN--
<?php
unset($array);
