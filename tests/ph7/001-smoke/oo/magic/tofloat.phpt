--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
__toFloat() is NOT a php magic method: (float)$obj warns and yields 1.0 (was a bare skip freezing the PH7 extension)
--FILE--
<?php
class FooToFloat {
    public $value = 3.14;
    public function __toFloat(){ return (float)$this->value; }   // php never calls this
}
$o = new FooToFloat();
var_dump(@(float)$o);
var_dump(@floatval($o));

$seen = [];
set_error_handler(function ($no, $msg) use (&$seen) { $seen[] = $msg; return true; });
(float)$o;
restore_error_handler();
echo $seen[0], "\n";
?>
--EXPECT--
float(1)
float(1)
Object of class FooToFloat could not be converted to float
--CLEAN--
<?php
unset($o, $seen);
