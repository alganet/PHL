--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
preg_replace_callback populates the &$count out-param through an array subscript
--FILE--
<?php
$a = [];
$s = preg_replace_callback('/\d/', fn($m) => '#', '1a2', -1, $a['c']);
echo $s . "\n";
echo $a['c'] . "\n";
?>
--EXPECT--
#a#
2
--CLEAN--
<?php
