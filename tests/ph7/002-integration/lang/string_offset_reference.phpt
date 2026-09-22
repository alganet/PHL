--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A string OFFSET cannot be either end of a reference: every binding form is php's Error (PHL bound the BASE STRING's slot, so writing through the reference replaced the whole string)
--FILE--
<?php
function attempt($label, callable $fn) {
    echo str_pad($label, 22);
    try {
        $line = 'ok ' . str_replace("\n", '', var_export($fn(), true));
    } catch (Error $e) {
        $line = get_class($e) . ': ' . $e->getMessage();
    }
    echo $line, "\n";
}
class Holder { public $p = 'init'; public static $s = 'init'; }

// The value read out of a string carries the BASE VARIABLE's slot index -- right
// for a plain read, fatal for a binding: `$r = &$s[1]; $r = "Z";` left $s === "Z".
attempt('plain =&',    function () { $s = "abc"; $r = &$s[1]; $r = "Z"; return $s; });
attempt('nested =&',   function () { $a = ["k" => "abc"]; $r = &$a["k"][0]; return $a; });
attempt('array lit',   function () { $s = "abc"; $x = [&$s[1]]; return $s; });
attempt('append =&',   function () { $s = "abc"; $a = []; $a[] = &$s[1]; return $s; });
attempt('keyed =&',    function () { $s = "abc"; $a = []; $a["k"] = &$s[1]; return $s; });
attempt('prop =&',     function () { $s = "abc"; $o = new Holder(); $o->p = &$s[1]; return $o->p; });
attempt('static =&',   function () { $s = "abc"; Holder::$s = &$s[1]; return Holder::$s; });
attempt('byref arg',   function () { $s = "abc"; $f = function (&$q) { $q = "Z"; }; $f($s[1]); return $s; });
// The offset's own rules still run first.
attempt('bad offset',  function () { $s = "abc"; $r = &$s["p"]; return $s; });
// A COPY of a string offset is an ordinary value and references fine...
attempt('copy then =&',function () { $s = "abc"; $c = $s[1]; $r = &$c; $r = "Z"; return [$s, $c]; });
// ...and an ARRAY element is still a real slot.
attempt('array elem',  function () { $a = ["x"]; $r = &$a[0]; $r = "Z"; return $a; });

// Caught, execution resumes and the string is untouched.
$s = "abc";
try { $r = &$s[1]; } catch (Error $e) { echo "caught\n"; }
echo "after: ", $s, "\n";
?>
--EXPECT--
plain =&              Error: Cannot create references to/from string offsets
nested =&             Error: Cannot create references to/from string offsets
array lit             Error: Cannot create references to/from string offsets
append =&             Error: Cannot create references to/from string offsets
keyed =&              Error: Cannot create references to/from string offsets
prop =&               Error: Cannot create references to/from string offsets
static =&             Error: Cannot create references to/from string offsets
byref arg             Error: Cannot create references to/from string offsets
bad offset            TypeError: Cannot access offset of type string on string
copy then =&          ok array (  0 => 'abc',  1 => 'Z',)
array elem            ok array (  0 => 'Z',)
caught
after: abc
