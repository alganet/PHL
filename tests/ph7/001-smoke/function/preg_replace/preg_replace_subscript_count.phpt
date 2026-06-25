--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
preg_replace populates the &$count out-param through an array subscript
--FILE--
<?php
$a = [];
$s = preg_replace('/\d/', '#', '1a2b3', -1, $a['c']);
echo $s . "\n";
echo $a['c'] . "\n";
?>
--EXPECT--
#a#b#
3
--CLEAN--
<?php
