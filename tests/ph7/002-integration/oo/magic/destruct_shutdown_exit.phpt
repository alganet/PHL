--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
exit() inside a shutdown destructor stops the rest and keeps its status
--DESCRIPTION--
The other way php's destructor phase ends early. `exit()` from inside one of
them skips every destructor still owed and carries its status out, the same way
`exit()` inside a register_shutdown_function() callback skips the remaining
callbacks. Reaching the phase at all after a top-level `exit()` is the point of
the first two objects here.
--FILE--
<?php
class Mark
{
    public $n;
    public function __construct($n) { $this->n = $n; }
    public function __destruct()
    {
        echo "D:{$this->n}\n";
        if ($this->n === 'exiter') {
            exit(3);
        }
    }
}

$early  = new Mark('early');
$exiter = new Mark('exiter');
echo "script-end\n";
exit(0);
?>
--EXPECT--
script-end
D:exiter
