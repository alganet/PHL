--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
__FUNCTION__/__METHOD__/__TRAIT__ inside a match() arm resolve lexically (regression: crash)
--FILE--
<?php
// A match() expression compiles its arms inside a synthetic function block that
// carries no ph7_vm_func. Resolving a compile-time magic constant there once
// dereferenced that NULL block and crashed; it must instead see through the
// synthetic block to the enclosing real function/method/trait.
function ff($x) { return match($x) { 1 => __FUNCTION__, default => 'd' }; }

class K {
    public function mm($x) { return match($x) { 1 => __METHOD__, default => 'd' }; }
    public function fn2($x) { return match($x) { 1 => __FUNCTION__, default => 'd' }; }
}

trait Tr {
    public function tm($x) { return match($x) { 1 => __TRAIT__, default => 'd' }; }
}
class UsesTr { use Tr; }

var_dump(ff(1));            // "ff"
var_dump((new K)->mm(1));   // "K::mm"
var_dump((new K)->fn2(1));  // "fn2"
var_dump((new UsesTr)->tm(1)); // "Tr"
?>
--EXPECT--
string(2) "ff"
string(5) "K::mm"
string(3) "fn2"
string(2) "Tr"
--CLEAN--
<?php
