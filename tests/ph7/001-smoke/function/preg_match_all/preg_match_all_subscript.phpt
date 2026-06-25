--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
preg_match_all populates an array-subscript by-ref target ($matches)
--FILE--
<?php
$a = [];
$r = preg_match_all('/\d/', '1 2 3', $a['m']);
echo $r . "\n";
echo implode(',', $a['m'][0]) . "\n";
?>
--EXPECT--
3
1,2,3
--CLEAN--
<?php
