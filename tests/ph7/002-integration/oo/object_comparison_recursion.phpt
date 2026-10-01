--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Comparing two cyclic objects throws a catchable Error
--FILE--
<?php
$a = new stdClass;
$b = new stdClass;
$a->r = $b;
$b->r = $a;
try {
    $a == $b;
    echo "no throw\n";
} catch (Error $e) {
    echo $e->getMessage() . "\n";
}
try {
    $a != $b;
    echo "no throw\n";
} catch (Error $e) {
    echo $e->getMessage() . "\n";
}
?>
--EXPECT--
Nesting level too deep - recursive dependency?
Nesting level too deep - recursive dependency?
--CLEAN--
<?php
unset($a, $b);
