--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Writing a property of a non-object is php's catchable Error, with its own verb per write kind
--FILE--
<?php
set_error_handler(function ($no, $msg) { echo "Warning: $msg\n"; return true; });

function propnonobj($label, callable $fn) {
    try {
        $fn();
        echo "$label: no error\n";
    } catch (Throwable $e) {
        echo "$label: ", get_class($e), ": ", $e->getMessage(), "\n";
    }
}

propnonobj("assign-null", function () { $u = null; $u->p = 1; });
propnonobj("assign-false", function () { $u = false; $u->p = 1; });
propnonobj("assign-true", function () { $u = true; $u->p = 1; });
propnonobj("assign-int", function () { $u = 5; $u->p = 1; });
propnonobj("assign-float", function () { $u = 1.5; $u->p = 1; });
propnonobj("assign-string", function () { $u = "s"; $u->p = 1; });
propnonobj("assign-array", function () { $u = [1]; $u->p = 1; });
propnonobj("assign-undefined", function () { $undef->p = 1; });
propnonobj("compound", function () { $u = null; $u->p .= "x"; });
propnonobj("coalesce-assign", function () { $u = null; $u->p ??= 1; });
propnonobj("incr", function () { $u = null; $u->p++; });
propnonobj("decr", function () { $u = null; $u->p--; });
propnonobj("subscript", function () { $u = null; $u->p["k"] = 1; });
propnonobj("append", function () { $u = null; $u->p[] = 1; });
propnonobj("reference", function () { $u = null; $x = 1; $u->p =& $x; });
propnonobj("list-target", function () { $u = null; [$u->p] = [1]; });
propnonobj("foreach-target", function () { $u = "s"; foreach ([1] as $u->p) {} });
propnonobj("method", function () { $u = true; $u->m(); });
propnonobj("static-prop", function () { $u = null; $u::$s = 1; });
propnonobj("static-const", function () { $u = [1]; $x = $u::K; });

// A READ is only a warning, and php names a bool by its value there too.
$b = false;
$r = $b->p;
var_dump($r);
$t = true;
$r = $t->p;
var_dump($r);

// The value name reaches the neighbouring diagnostics php spells the same way.
foreach ($b as $ignored) {
}
$r = $b[0];
var_dump($r);
?>
--EXPECT--
assign-null: Error: Attempt to assign property "p" on null
assign-false: Error: Attempt to assign property "p" on false
assign-true: Error: Attempt to assign property "p" on true
assign-int: Error: Attempt to assign property "p" on int
assign-float: Error: Attempt to assign property "p" on float
assign-string: Error: Attempt to assign property "p" on string
assign-array: Error: Attempt to assign property "p" on array
assign-undefined: Error: Attempt to assign property "p" on null
compound: Error: Attempt to assign property "p" on null
coalesce-assign: Error: Attempt to assign property "p" on null
incr: Error: Attempt to increment/decrement property "p" on null
decr: Error: Attempt to increment/decrement property "p" on null
subscript: Error: Attempt to modify property "p" on null
append: Error: Attempt to modify property "p" on null
reference: Error: Attempt to modify property "p" on null
list-target: Error: Attempt to assign property "p" on null
foreach-target: Error: Attempt to assign property "p" on string
method: Error: Call to a member function m() on true
static-prop: Error: Class name must be a valid object or a string
static-const: Error: Class name must be a valid object or a string
Warning: Attempt to read property "p" on false
NULL
Warning: Attempt to read property "p" on true
NULL
Warning: foreach() argument must be of type array|object, false given
Warning: Trying to access array offset on false
NULL
--CLEAN--
<?php
restore_error_handler();
unset($b, $t, $r);
