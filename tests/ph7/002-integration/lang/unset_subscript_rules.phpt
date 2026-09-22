--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
unset() over a subscript destroys only what is there: a string offset is php's "Cannot unset string offsets" Error (PHL freed the whole variable), and a missing key/base is left uncreated
--FILE--
<?php
function attempt($label, callable $fn) {
    echo str_pad($label, 24);
    try {
        $line = 'ok ' . str_replace("\n", '', var_export($fn(), true));
    } catch (Error $e) {
        $line = get_class($e) . ': ' . $e->getMessage();
    }
    echo $line, "\n";
}

// A string offset cannot be unset AT ALL -- php refuses whatever the offset is.
// PHL loaded the character but kept the BASE's slot index on it, so the trailing
// unset() freed the base itself: $s became undefined, and one level in the whole
// $a["k"] element disappeared.
attempt('string int',    function () { $s = "abcdef"; unset($s[1]); return $s; });
attempt('string oor',    function () { $s = "abcdef"; unset($s[99]); return $s; });
attempt('string alpha',  function () { $s = "abcdef"; unset($s["p"]); return $s; });
attempt('string array',  function () { $s = "abcdef"; unset($s[[]]); return $s; });
attempt('string in arr', function () { $a = ["k" => "abcdef"]; unset($a["k"][0]); return $a; });
// unset() over a non-array, non-string base has php's own wording, which is not
// the write-context one.
attempt('int base',      function () { $i = 5; unset($i[0]); return $i; });
attempt('float base',    function () { $f = 1.5; unset($f[0]); return $f; });
attempt('bool base',     function () { $b = true; unset($b[0]); return $b; });
attempt('nested scalar', function () { $a = [1]; unset($a[0][1]); return $a; });
attempt('object base',   function () { $o = new stdClass(); unset($o[0]); return 'reached'; });
// unset() CREATES NOTHING: not the null base, not a missing intermediate, not the
// key it is about to remove.
attempt('null base',     function () { $n = null; unset($n[0]); return $n; });
attempt('missing key',   function () { $a = [1]; unset($a["nope"]); return $a; });
attempt('missing chain', function () { $a = ["x" => [1]]; unset($a["y"]["z"]); return $a; });
attempt('missing deep',  function () { $a = ["x" => [1]]; unset($a["x"]["z"]); return $a; });
// ...and still removes what IS there, through a chain, without touching a copy.
attempt('present key',   function () { $a = [1, 2, 3]; unset($a[1]); return $a; });
attempt('present chain', function () { $a = ["x" => [1, 2]]; unset($a["x"][0]); return $a; });
attempt('copy untouched',function () { $a = ["x" => [1, 2]]; $b = $a; unset($b["x"][0]); return $a; });

// An undefined base is not created either. php reads it (and warns) before it
// finds nothing to unset; the warning is captured so the assertion matches the
// message BODY (php's log copy prefixes "PHP ").
$warn = [];
set_error_handler(function ($no, $msg) use (&$warn) { $warn[] = $msg; return true; });
unset($undef[0]);
restore_error_handler();
echo 'undef created: ', var_export(isset($undef), true), "\n";
echo 'undef warning: ', implode(' | ', $warn), "\n";
?>
--EXPECT--
string int              Error: Cannot unset string offsets
string oor              Error: Cannot unset string offsets
string alpha            Error: Cannot unset string offsets
string array            Error: Cannot unset string offsets
string in arr           Error: Cannot unset string offsets
int base                Error: Cannot unset offset in a non-array variable
float base              Error: Cannot unset offset in a non-array variable
bool base               Error: Cannot unset offset in a non-array variable
nested scalar           Error: Cannot unset offset in a non-array variable
object base             Error: Cannot use object of type stdClass as array
null base               ok NULL
missing key             ok array (  0 => 1,)
missing chain           ok array (  'x' =>   array (    0 => 1,  ),)
missing deep            ok array (  'x' =>   array (    0 => 1,  ),)
present key             ok array (  0 => 1,  2 => 3,)
present chain           ok array (  'x' =>   array (    1 => 2,  ),)
copy untouched          ok array (  'x' =>   array (    0 => 1,    1 => 2,  ),)
undef created: false
undef warning: Undefined variable $undef
