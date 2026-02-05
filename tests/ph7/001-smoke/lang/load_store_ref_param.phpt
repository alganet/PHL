--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Pass-by-reference and load/store ref opcodes
--FILE--
<?php
function set_val(&$x){ $x = 'set'; }
function &pass_get_ref(&$x){ return $x; }

$var = 'orig';
set_val($var);
echo $var . "\n";

$z = &pass_get_ref($var);
$z = 'changed';
echo $var . "\n";

?>
--EXPECT--
set
changed
--CLEAN--
<?php
unset($var, $z);
