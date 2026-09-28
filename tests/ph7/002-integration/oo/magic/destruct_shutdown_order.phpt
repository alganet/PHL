--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Every destructor a finished program still owes runs, in php's shutdown order
--DESCRIPTION--
An object still referenced when the script ends used to be torn down with user
destructors suppressed, so a destructor that closes a file, flushes a buffer or
commits a transaction never fired for anything a program left in a global.

php's shutdown has two passes and both are observable here. The first walks the
GLOBAL SYMBOL TABLE backwards and drops every entry holding an object nothing
else refers to -- which is why `$last` goes before `$first`. The second reaches
everything still alive (an array element, an object two names share, a class
static, a function static) in object-CREATION order. Both run after the
register_shutdown_function() callbacks.
--FILE--
<?php
class Mark
{
    public $n;
    public function __construct($n) { $this->n = $n; }
    public function __destruct() { echo "D:{$this->n}\n"; }
}

class Holder { public static $kept; }

function withStatic()
{
    static $s;
    if ($s === null) {
        $s = new Mark('function-static');
    }
}

register_shutdown_function(function () { echo "shutdown-callback\n"; });

$first  = new Mark('first');
$boxed  = [new Mark('in-array')];
$shared = new Mark('shared');
$alias  = $shared;
Holder::$kept = new Mark('static-property');
withStatic();
$last = new Mark('last');
echo "script-end\n";
?>
--EXPECT--
script-end
shutdown-callback
D:last
D:first
D:in-array
D:shared
D:static-property
D:function-static
