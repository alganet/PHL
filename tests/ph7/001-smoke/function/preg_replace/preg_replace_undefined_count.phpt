--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
preg_replace populates the &$count out-param when the variable is undefined
--FILE--
<?php
$s = preg_replace('/\d/', 'x', 'a1b2c3', -1, $count);
echo $s . "\n";
echo $count . "\n";
?>
--EXPECT--
axbxcx
3
--CLEAN--
<?php
