--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A non-public __destruct reached at shutdown is a warning, not the Error
--DESCRIPTION--
php screens a non-public destructor where the object dies, and answers two
different things. With a php frame under it -- a running program's unset(), a
local going out of scope -- it is a catchable Error naming the scope. With NO
frame under it, which is where the shutdown pass reaches every object a program
left alive, it is an E_WARNING saying the call was IGNORED, after which the
object is simply not destructed and the program is already over.

php has no file and no line to name for that one (`EG(current_execute_data)` is
NULL), so it reports the location as `Unknown` on line `0` -- to the printed
copy and to a user error handler alike.
--FILE--
<?php
set_error_handler(function ($no, $str, $file, $line) {
    echo "handler[$no] $str | file=$file | line=$line\n";
    return true;
});

class Hidden { private function __destruct() { echo "never\n"; } }
class Guarded { protected function __destruct() { echo "never\n"; } }
class Open { public function __destruct() { echo "D:open\n"; } }

function drop()
{
    $o = new Hidden();
    unset($o);
}

try {
    drop();
} catch (Error $e) {
    echo "running: ", $e->getMessage(), "\n";
}

$a = new Hidden();
$b = new Guarded();
$c = new Open();
echo "script-end\n";
?>
--EXPECT--
running: Call to private Hidden::__destruct() from global scope
script-end
D:open
handler[2] Call to protected Guarded::__destruct() from global scope during shutdown ignored | file=Unknown | line=0
handler[2] Call to private Hidden::__destruct() from global scope during shutdown ignored | file=Unknown | line=0
