--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Isset and Empty
--FILE--
<?php
$a = 'hello';
echo isset($a) ? 'yes' : 'no';
echo "\n";
echo empty($a) ? 'yes' : 'no';
echo "\n";
echo isset($b) ? 'yes' : 'no';
echo "\n";
echo empty($b) ? 'yes' : 'no';
?>
--EXPECT--
yes
no
no
yes
--CLEAN--
<?php
unset($a);
?>