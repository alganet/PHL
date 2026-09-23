--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The slot a foreach destructuring or non-name target lands in is not a variable of the scope
--FILE--
<?php
class FeTgtInvisible {
    public $p;
}

function fetgt_locals_list() {
    foreach ([[1, 2]] as [$a, $b]) {
    }
    return array_keys(get_defined_vars());
}

function fetgt_locals_target() {
    $o = new FeTgtInvisible;
    foreach ([1, 2] as $o->p) {
    }
    return array_keys(get_defined_vars());
}

print_r(fetgt_locals_list());
print_r(fetgt_locals_target());
?>
--EXPECT--
Array
(
    [0] => a
    [1] => b
)
Array
(
    [0] => o
)
--CLEAN--
<?php
