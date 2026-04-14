--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nullsafe on a non-null non-object LHS does not short-circuit: the underlying `->` raises an error (PHP Warning / PHL Error) and execution continues with null. Nullsafe only suppresses the NULL case.
--FILE--
<?php
$nsfNonObj_fired = 0;
function nsfNonObj_handler($errno, $errstr) {
    global $nsfNonObj_fired;
    $nsfNonObj_fired = 1;
    return true;
}
set_error_handler('nsfNonObj_handler');
$nsfNonObj_a = 5;
$nsfNonObj_r = $nsfNonObj_a?->foo;
echo "error-fired:", ($nsfNonObj_fired ? "yes" : "no"), "\n";
echo "r-is-null:", ($nsfNonObj_r === null ? "yes" : "no"), "\n";
echo "done\n";
restore_error_handler();
?>
--EXPECT--
error-fired:yes
r-is-null:yes
done
--CLEAN--
<?php
unset($nsfNonObj_a, $nsfNonObj_r, $nsfNonObj_fired);
