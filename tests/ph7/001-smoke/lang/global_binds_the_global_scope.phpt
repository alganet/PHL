--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
`global $x` names the global scope, and creates the variable when it is not there yet
--FILE--
<?php
$gbind_v = "global";

function gbind_inner() {
    global $gbind_v;
    echo $gbind_v, "\n";
    $gbind_v = "set-by-inner";
}

// The caller has a LOCAL of the same name: it must not be what `global` binds.
function gbind_outer() {
    $gbind_v = "caller-local";
    gbind_inner();
    echo $gbind_v, "\n";
}

gbind_outer();
echo $gbind_v, "\n";

// A global that does not exist yet is created by the declaration, which is what
// makes the ordinary initializer idiom work.
function gbind_make() {
    global $gbind_fresh, $gbind_second;
    $gbind_fresh = "made";
    $gbind_second = "also-made";
}
function gbind_read() {
    global $gbind_fresh;
    return $gbind_fresh;
}
gbind_make();
echo gbind_read(), " ", $gbind_second, "\n";

// The binding is a reference to one slot, so repeated calls accumulate.
function gbind_bump() {
    global $gbind_count, $gbind_list;
    $gbind_count++;
    $gbind_list[] = $gbind_count;
}
gbind_bump();
gbind_bump();
echo $gbind_count, " ", implode(",", $gbind_list), "\n";

// A local declared BEFORE the global statement is replaced by it.
function gbind_shadow() {
    $gbind_v = "local";
    global $gbind_v;
    return $gbind_v;
}
echo gbind_shadow(), "\n";

// A superglobal stays itself.
function gbind_super() {
    global $_SERVER;
    return is_array($_SERVER);
}
var_dump(gbind_super());
?>
--EXPECT--
global
caller-local
set-by-inner
made also-made
2 1,2
set-by-inner
bool(true)
--CLEAN--
<?php
unset($gbind_v, $gbind_fresh, $gbind_second, $gbind_count, $gbind_list);
