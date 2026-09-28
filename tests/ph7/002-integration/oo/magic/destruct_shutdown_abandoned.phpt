--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A throwable a shutdown destructor never catches abandons the rest of them
--DESCRIPTION--
php runs its whole destructor phase under one bailout guard, so the first
uncaught throwable ends it: every object still owed a destructor -- including
the ones the symbol-table half had not reached yet, and everything the object
store holds -- is left undestructed, and the program exits 255.

The trace line is wildcarded: a frame the ENGINE pushed (nothing in the source
called this destructor) is php's `[internal function]`, which PHL does not
report that way yet.
--FILE--
<?php
class Mark
{
    public $n;
    public function __construct($n) { $this->n = $n; }
    public function __destruct()
    {
        echo "D:{$this->n}\n";
        if ($this->n === 'thrower') {
            throw new RuntimeException('from a destructor');
        }
    }
}

class Holder { public static $kept; }

Holder::$kept = new Mark('static-property');
$early   = new Mark('early');
$thrower = new Mark('thrower');
echo "script-end\n";
?>
--EXPECTF--
script-end
D:thrower
--EXPECT_STDERR--
PHP Fatal error:  Uncaught RuntimeException: from a destructor in %s:10
Stack trace:
#0 %A: Mark->__destruct()
#1 {main}
  thrown in %s on line 10
