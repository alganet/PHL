--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Complex string interpolation with curly braces
--FILE--
<?php
$var = array('key' => 'value');
echo "Test: {$var['key']}";
?>
--EXPECT--
Test: value
--CLEAN--
<?php
unset($var);
