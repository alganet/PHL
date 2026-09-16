--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php
// PHL detects a self-referencing object graph with a depth counter and prints
// RECURSION_LIMIT; php marks the back-edge *RECURSION* instead. Different mechanism, so
// the output cannot agree -- see the deep-nesting twin above for the rationale.
if (function_exists('zend_version')) { echo 'skip PHL uses a depth counter where php marks *RECURSION*'; }
?>
--TEST--
object dump recursion limit
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

// Create a cycle
$current->prop = $root;

ob_start();
var_dump($root);
$output = ob_get_clean();
echo strpos($output, 'Nesting limit reached') !== false ? 'RECURSION_LIMIT' : 'NO_LIMIT';
?>
--EXPECT--
RECURSION_LIMIT
--CLEAN--
<?php
unset($root, $current, $next, $output);
