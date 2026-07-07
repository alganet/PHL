--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
call_user_func_array named forwarding honors the caller's strict_types
--FILE--
<?php
declare(strict_types=1);
function cufaStrictF(int $x) { echo "x=$x\n"; }
// call_user_func_array binds a name:'d string to a typed param under the
// caller's strict_types, so a string->int mismatch throws (verified vs php).
try {
    call_user_func_array('cufaStrictF', ['x' => '5']);
    echo "cufa: no throw\n";
} catch (\TypeError $e) {
    echo "cufa: TypeError\n";
}
// call_user_func with a NAMED arg is the documented php quirk: it coerces in
// weak mode even here, so '5' becomes 5 rather than throwing.
call_user_func('cufaStrictF', x: '5');
// An exactly-typed value still passes through by name.
call_user_func_array('cufaStrictF', ['x' => 7]);
?>
--EXPECT--
cufa: TypeError
x=5
x=7
--CLEAN--
<?php

