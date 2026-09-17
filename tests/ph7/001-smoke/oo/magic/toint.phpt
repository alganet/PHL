--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
__toInt() is NOT a php magic method: (int)$obj warns and yields 1 (was a bare skip freezing the PH7 extension, which returned the object's own data)
--FILE--
<?php
class FooToInt {
    public $value = 42;
    public function __toInt(){ return (int)$this->value; }   // php never calls this
}
$o = new FooToInt();
var_dump(@(int)$o);
var_dump(@intval($o));

// The warning body is asserted separately so the log/display prefix can float.
$seen = [];
set_error_handler(function ($no, $msg) use (&$seen) { $seen[] = $msg; return true; });
(int)$o;
restore_error_handler();
echo $seen[0], "\n";
?>
--EXPECT--
int(1)
int(1)
Object of class FooToInt could not be converted to int
--CLEAN--
<?php
unset($o, $seen);
