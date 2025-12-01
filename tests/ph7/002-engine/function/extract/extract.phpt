--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
extract imports variables into the current symbol table
--FILE--
<?php
$arr = array('k' => 'v');
extract($arr);
echo $k . "\n";
?>
--EXPECT--
v
--CLEAN--
<?php
unset($arr, $k);
?>
