--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A call argument is read where it is sent, before a later argument runs
--FILE--
<?php
/* php reads a call argument at its own send, so a plain variable, a subscript
 * or a property read in an EARLIER position warns before a LATER argument
 * runs any code -- and a by-reference parameter creates the variable before
 * that code can see it. The calls php compiles into a frameless instruction
 * or an opcode of their own (max(), array_key_exists(), a sprintf() it
 * rewrites) read a plain variable at the call instead, after every argument. */
set_error_handler(function ($no, $msg) { echo "  warning: $msg\n"; return true; });
function aos_s() { echo "  ran s\n"; return 1; }
function aos_f($a, $b, ...$r) { return 1; }
function aos_r(&$a, $b) { $a = 7; }
function aos_seen() { global $aos_g; echo "  seen: ", isset($aos_g) ? 'set' : 'unset', "\n"; return 0; }
class AosK {
    function m($a, $b) {}
    static function sm($a, $b) {}
    function __call($n, $a) { echo "  __call $n\n"; }
    static function __callStatic($n, $a) { echo "  __callStatic $n\n"; }
}
class AosNoCtor {}
class AosCtor { function __construct($a, $b) {} }
$k = new AosK;
$arr = [];
$o = new stdClass;
$cl = function ($a, $b) {};

echo "function\n";               aos_f($u, aos_s());
echo "between two runners\n";    aos_f(aos_s(), $u, aos_s());
echo "several\n";                aos_f(1, $u, aos_s(), $w, aos_s());
echo "subscript\n";              aos_f($arr['k'], aos_s());
echo "property\n";               aos_f($o->p, aos_s());
echo "method\n";                 $k->m($u, aos_s());
echo "static method\n";          AosK::sm($u, aos_s());
echo "__call\n";                 $k->zz($u, aos_s());
echo "__callStatic\n";           AosK::yy($u, aos_s());
echo "closure\n";                $cl($u, aos_s());
echo "new\n";                    new AosCtor($u, aos_s());
echo "new, no constructor\n";    new AosNoCtor($u, aos_s());
echo "builtin\n";                var_export($u, aos_s() > 0);
echo "named after positional\n"; aos_f($u, b: aos_s());
echo "by reference\n";           aos_r($aos_g, aos_seen()); var_dump($aos_g);
echo "by reference, element\n";  aos_r($q['k'], aos_s()); var_dump($q);
echo "frameless max\n";          max($u, aos_s());
echo "frameless in_array\n";     in_array($u, [aos_s()]);
echo "frameless, subscript\n";   max($arr['k'], aos_s());
echo "not frameless at 3\n";     max($u, aos_s(), 3);
echo "rewritten sprintf\n";      sprintf('%s%s', $u, aos_s());
echo "sprintf it keeps\n";       sprintf('%s%x', $u, aos_s());
--EXPECT--
function
  warning: Undefined variable $u
  ran s
between two runners
  ran s
  warning: Undefined variable $u
  ran s
several
  warning: Undefined variable $u
  ran s
  warning: Undefined variable $w
  ran s
subscript
  warning: Undefined array key "k"
  ran s
property
  warning: Undefined property: stdClass::$p
  ran s
method
  warning: Undefined variable $u
  ran s
static method
  warning: Undefined variable $u
  ran s
__call
  warning: Undefined variable $u
  ran s
  __call zz
__callStatic
  warning: Undefined variable $u
  ran s
  __callStatic yy
closure
  warning: Undefined variable $u
  ran s
new
  warning: Undefined variable $u
  ran s
new, no constructor
  warning: Undefined variable $u
  ran s
builtin
  warning: Undefined variable $u
  ran s
named after positional
  warning: Undefined variable $u
  ran s
by reference
  seen: unset
int(7)
by reference, element
  ran s
array(1) {
  ["k"]=>
  int(7)
}
frameless max
  ran s
  warning: Undefined variable $u
frameless in_array
  ran s
  warning: Undefined variable $u
frameless, subscript
  warning: Undefined array key "k"
  ran s
not frameless at 3
  warning: Undefined variable $u
  ran s
rewritten sprintf
  ran s
  warning: Undefined variable $u
sprintf it keeps
  warning: Undefined variable $u
  ran s
