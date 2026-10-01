--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A cycle 35 objects long marks the back edge, and only the back edge
--FILE--
<?php
class ObjectDumpRecursionTest {
    public $prop;
}

$root = new ObjectDumpRecursionTest();
$current = $root;

for ($i = 0; $i < 35; $i++) {
    $next = new ObjectDumpRecursionTest();
    $current->prop = $next;
    $current = $next;
}

// Close the cycle back onto the root.
$current->prop = $root;

ob_start();
var_dump($root);
$output = ob_get_clean();

// One marker, at the one edge that points back at a container being walked --
// every link before it is a distinct object and renders in full.
echo 'markers=', substr_count($output, '*RECURSION*'), "\n";
echo 'objects=', substr_count($output, 'object(ObjectDumpRecursionTest)'), "\n";
?>
--EXPECT--
markers=1
objects=36
--CLEAN--
<?php
unset($root, $current, $next, $output);
