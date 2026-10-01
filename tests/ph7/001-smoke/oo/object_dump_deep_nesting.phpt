--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nesting with no cycle in it is walked to the leaf
--FILE--
<?php
class ObjectDumpDeepNestingTest {
    public $prop;
}

$root = new ObjectDumpDeepNestingTest();
$current = $root;
for ($i = 0; $i < 35; $i++) {
    $next = new ObjectDumpDeepNestingTest();
    $current->prop = $next;
    $current = $next;
}

ob_start();
var_dump($root);
$output = ob_get_clean();

// A depth counter used to stand in for a cycle guard and cut this off at 31.
echo 'markers=', substr_count($output, '*RECURSION*'), "\n";
echo 'objects=', substr_count($output, 'object(ObjectDumpDeepNestingTest)'), "\n";
echo 'leaf=', substr_count($output, 'NULL'), "\n";
?>
--EXPECT--
markers=0
objects=36
leaf=1
--CLEAN--
<?php
unset($root, $current, $next, $output);
