--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Match expression: uses strict (===) comparison, not loose (==)
--FILE--
<?php
echo match ("1") { 1 => 'int', "1" => 'string' }, "\n";
echo match (1)   { "1" => 'string', 1 => 'int' }, "\n";
echo match (0)   { false => 'false', null => 'null', 0 => 'int zero' }, "\n";
echo match (null){ 0 => 'int zero', false => 'false', null => 'null' }, "\n";
echo match (true){ 1 => 'one', "true" => 'str', true => 'bool true' }, "\n";
?>
--EXPECT--
string
int
int zero
null
bool true
--CLEAN--
<?php
