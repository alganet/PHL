--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_defined_vars() returns locals in DECLARATION order (params before body locals; a reassignment keeps the original position)
--DESCRIPTION--
php returns the local symbol table in the order names first appeared (a,b,c), not
reverse. PHL's frame table is head-pushed, so its natural iteration is reverse-insertion;
get_defined_vars() walks it backward to match php. Reassigning a variable reuses its slot
and keeps its original position. A recorded divergence (the hashmap-iteration-order family).
--FILE--
<?php
function locals() {
    $first = 1;
    $second = 2;
    $third = 3;
    $first = 99;
    return implode(',', array_keys(get_defined_vars()));
}
function withparams($p, $q) {
    $r = 3;
    return implode(',', array_keys(get_defined_vars()));
}
echo locals(), "\n";
echo withparams(1, 2), "\n";
?>
--EXPECT--
first,second,third
p,q,r
