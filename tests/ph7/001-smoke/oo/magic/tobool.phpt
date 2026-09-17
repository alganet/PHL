--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
__toBool() is NOT a php magic method: an object is ALWAYS truthy, silently (was a bare skip freezing a PH7 extension that could flip an if())
--FILE--
<?php
class FooToBool {
    public $value = false;
    public function __toBool(){ return (bool)$this->value; }   // php never calls this
}
$o = new FooToBool();
var_dump((bool)$o);
var_dump(boolval($o));
echo $o ? "truthy\n" : "falsy\n";
var_dump(!$o);

// Unlike the int/float casts, php emits NO diagnostic here.
$seen = 0;
set_error_handler(function () use (&$seen) { $seen++; return true; });
(bool)$o;
restore_error_handler();
var_dump($seen === 0);
?>
--EXPECT--
bool(true)
bool(true)
truthy
bool(false)
bool(true)
--CLEAN--
<?php
unset($o, $seen);
