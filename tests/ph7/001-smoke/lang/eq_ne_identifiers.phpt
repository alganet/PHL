--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
eq and ne are plain identifiers (PH7 operator extension removed)
--FILE--
<?php
// The PH7-inherited 'eq'/'ne' string-comparison operators stole these
// identifiers from valid PHP. They must work as names everywhere PHP allows.
function eq($a, $b) { return $a == $b; }
function ne($a, $b) { return $a != $b; }
echo "fn_eq: ", eq(1, "1") ? "true" : "false", "\n";
echo "fn_ne: ", ne(1, 2) ? "true" : "false", "\n";

const eq = 7;
const ne = 8;
echo "const: ", eq + ne, "\n";

class Cmp {
    const ne = "cne";
    public $eq = "peq";
    function eq() { return "meq"; }
    static function ne() { return "sne"; }
}
$o = new Cmp();
echo "method: ", $o->eq(), " ", Cmp::ne(), "\n";
echo "prop: ", $o->eq, " ", Cmp::ne, "\n";

$eq = "var-eq";
$ne = "var-ne";
echo "vars: $eq $ne\n";
?>
--EXPECT--
fn_eq: true
fn_ne: true
const: 15
method: meq sne
prop: peq cne
vars: var-eq var-ne
--CLEAN--
<?php
unset($o, $eq, $ne);
