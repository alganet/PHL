--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: array_values returns numeric indexed array with values
--FILE--
<?php
$a = array('x' => 'a', 'y' => 'b');
echo implode(',', array_values($a)) . "\n";
?>
--EXPECT--
a,b
--CLEAN--
<?php
unset($a);
?>
