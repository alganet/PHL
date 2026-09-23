--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php's dereferencable scalars are accepted left of "->" and run
--FILE--
<?php
set_error_handler(function ($no, $msg) { echo "Warning: $msg\n"; return true; });

const DEREF_SCALAR_K = 5;
class DerefScalarHolder { const C = 7; }

$a = "x";

var_dump("s"->p);
var_dump('s'->p);
var_dump("$a"->p);
var_dump([1]->p);
var_dump(array(1)->p);
var_dump(DEREF_SCALAR_K->p);
var_dump(DerefScalarHolder::C->p);
var_dump(true->p);
var_dump(null->p);
var_dump(null?->p);

try {
    "s"->m();
} catch (Throwable $e) {
    echo get_class($e), ": ", $e->getMessage(), "\n";
}
?>
--EXPECT--
Warning: Attempt to read property "p" on string
NULL
Warning: Attempt to read property "p" on string
NULL
Warning: Attempt to read property "p" on string
NULL
Warning: Attempt to read property "p" on array
NULL
Warning: Attempt to read property "p" on array
NULL
Warning: Attempt to read property "p" on int
NULL
Warning: Attempt to read property "p" on int
NULL
Warning: Attempt to read property "p" on true
NULL
Warning: Attempt to read property "p" on null
NULL
NULL
Error: Call to a member function m() on string
--CLEAN--
<?php
restore_error_handler();
unset($a);
