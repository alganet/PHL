--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A call's result may be written THROUGH, php's shape
--FILE--
<?php
// php lets a call be the BASE of a write chain: the write lands on the value the
// call answered and is discarded, and for an object receiver it really mutates
// the object -- which is why `getConfig()->debug = true` is an ordinary idiom.
// PHL refused every subscript whose base was not a variable, at PARSE time, with
// a message naming the operator ("'=': Left operand must be a modifiable
// l-value"), so these programs did not run at all.

function wtcrArray() { return [1, 2, 3]; }
function wtcrObject() { static $o; if (!$o) { $o = new WtcrHolder; } return $o; }

class WtcrHolder { public $p = 0; public $q = 'a'; }
class WtcrMaker {
    public $h;
    function m() { if (!$this->h) { $this->h = new WtcrHolder; } return $this->h; }
    function a() { return [10, 20]; }
    static function s() { return [10, 20]; }
}

// A userland call's array result: the write goes to the temporary.
wtcrArray()[0] = 5;
echo "sub-assign ok\n";

// A builtin php does NOT specialize is writable through just the same.
str_split("ab")[0] = "z";
strtoupper("a")[0] = "z";
array_values([1])[0] = 2;
get_object_vars(new WtcrHolder)["p"] = 2;
echo "builtin-base ok\n";

// A method call, a static call and a closure call are all bases too.
$mk = new WtcrMaker;
$mk->a()[0] = 5;
WtcrMaker::s()[0] = 5;
$f = function () { return [1, 2]; };
$f()[0] = 5;
echo "call-kinds ok\n";

// An object receiver is really mutated -- the object outlives the statement.
wtcrObject()->p = 7;
echo "object write: ", wtcrObject()->p, "\n";
wtcrObject()->q .= 'b';
echo "object append: ", wtcrObject()->q, "\n";
wtcrObject()->p++;
echo "object incr: ", wtcrObject()->p, "\n";
$mk->m()->p = 3;
echo "method receiver: ", $mk->m()->p, "\n";

// Through a reference, and as an unset()/foreach target.
$r =& wtcrObject()->p;
$r = 9;
echo "by ref: ", wtcrObject()->p, "\n";
unset(wtcrObject()->p);
var_dump(isset(wtcrObject()->p));
foreach ([1] as wtcrArray()[0]) {
}
echo "foreach target ok\n";

// An ArrayAccess receiver answered by a call.
$ao = new ArrayObject([1, 2, 3]);
function wtcrAo() { static $a; if (!$a) { $a = new ArrayObject([1, 2, 3]); } return $a; }
wtcrAo()[0] = 9;
echo "arrayaccess: ", wtcrAo()[0], "\n";
?>
--EXPECT--
sub-assign ok
builtin-base ok
call-kinds ok
object write: 7
object append: ab
object incr: 8
method receiver: 3
by ref: 9
bool(false)
foreach target ok
arrayaccess: 9
