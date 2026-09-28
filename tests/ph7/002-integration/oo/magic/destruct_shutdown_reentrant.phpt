--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A shutdown destructor still sees the globals, and an object it makes is destructed too
--DESCRIPTION--
The phase sits before the output buffers are flushed, so a destructor's own echo
still lands inside one a program left open -- which is what makes a destructor
that flushes something work at all. The program's globals are still standing
(the phase runs long before the teardown proper), reachable by name and through
$GLOBALS, and an object a destructor creates is owed a destructor of its own.
--FILE--
<?php
class Late
{
    public $n;
    public function __construct($n) { $this->n = $n; }
    public function __destruct()
    {
        echo "D:{$this->n}\n";
        global $tag;
        echo "  sees-global: ", var_export($tag, true), "\n";
        echo "  sees-GLOBALS: ", var_export($GLOBALS['tag'], true), "\n";
        if ($this->n === 'first') {
            $GLOBALS['born'] = new Late('born-in-a-destructor');
        }
    }
}

ob_start();
$tag   = 'still-here';
$first = new Late('first');
echo "script-end\n";
?>
--EXPECT--
script-end
D:first
  sees-global: 'still-here'
  sees-GLOBALS: 'still-here'
D:born-in-a-destructor
  sees-global: 'still-here'
  sees-GLOBALS: 'still-here'
