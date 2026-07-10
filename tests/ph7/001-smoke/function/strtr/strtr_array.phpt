--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strtr with a replace_pairs array (longest-match, left-to-right, no rescan)
--FILE--
<?php
echo strtr("babaca", array("a"=>"x","b"=>"y")) . "\n";
// longest key at each position wins
echo strtr("hello world", array("hello"=>"hi","hello world"=>"HW")) . "\n";
// replacements are not rescanned
echo strtr("ab", array("a"=>"b","b"=>"c")) . "\n";
// overlapping keys: the longer one wins
echo strtr("aaa", array("a"=>"1","aa"=>"2")) . "\n";
?>
--EXPECT--
yxyxcx
HW
bc
21
--CLEAN--
<?php

