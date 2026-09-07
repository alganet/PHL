--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Diagnostics, exceptions and traces report the source line
--FILE--
<?php
set_error_handler(function ($no, $msg, $file, $line) {
    echo "warn(line $line): $msg\n";
    return true;
});

$ltA = "5x" + 1;

$ltB = "6y"
     + 1;

function ltThrower() {
    throw new RuntimeException("boom");
}

try {
    ltThrower();
} catch (RuntimeException $e) {
    echo "caught at line ", $e->getLine(), "\n";
    echo $e->getTraceAsString(), "\n";
}

$ltE = new LogicException("here");
echo "created at line ", $ltE->getLine(), "\n";
echo "file matches: ", var_export($ltE->getFile() === __FILE__, true), "\n";

class LtCustom extends Exception
{
    public function __construct()
    {
        // No parent::__construct() — php still records the creation site.
    }
}
$ltC = new LtCustom();
echo "subclass without parent ctor: line ", $ltC->getLine(), "\n";
restore_error_handler();
?>
--EXPECTF--
warn(line 7): A non-numeric value encountered
warn(line 10): A non-numeric value encountered
caught at line 13
#0 %A(17): ltThrower()
#1 {main}
created at line 23
file matches: true
subclass without parent ctor: line 34
--CLEAN--
<?php
