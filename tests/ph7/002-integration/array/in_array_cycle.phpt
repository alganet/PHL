--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
in_array() with a cyclic needle against a different cyclic element throws
--FILE--
<?php
$a = [];
$a[] = &$a;
$b = [];
$b[] = &$b;

// Same reference: in_array returns true (pointer identity)
var_dump(in_array($a, [$a]));

// Different cyclic arrays: comparison recurses and throws
try {
    var_dump(in_array($a, [$b]));
} catch (Error $e) {
    echo $e->getMessage() . "\n";
}

// Non-cyclic haystack: the needle's cycle doesn't hurt since
// the first comparison is array vs non-array (type mismatch wins)
var_dump(in_array($a, [[1,2,3]]));
?>
--EXPECT--
bool(true)
Nesting level too deep - recursive dependency?
bool(false)
--CLEAN--
<?php
unset($a, $b);
