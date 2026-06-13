--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
preg_match_all populates $matches when the variable is undefined (auto-vivify by-ref)
--FILE--
<?php
$r = preg_match_all('/\d/', 'a1b2c3', $m);
echo $r . "\n";
echo implode(',', $m[0]) . "\n";
?>
--EXPECT--
3
1,2,3
--CLEAN--
<?php
