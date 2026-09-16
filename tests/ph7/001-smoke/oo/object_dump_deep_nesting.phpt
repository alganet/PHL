--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Object dump deep nesting limit
--SKIPIF--
<?php
// PHL bounds var_dump()/print_r() nesting depth and prints NESTING_LIMIT; php has no such
// limit and walks the whole structure. An engine-design difference, not a fidelity gap:
// the depth is an embedder knob (PH7_VM_CONFIG_RECURSION_DEPTH), and php's own failure
// mode for unbounded nesting is resource exhaustion measured in BYTES, not a depth.
if (function_exists('zend_version')) { echo 'skip PHL bounds var_dump nesting (embedder knob); php has no depth limit'; }
?>
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
echo strpos($output, 'Nesting limit reached') !== false ? 'NESTING_LIMIT' : 'NO_LIMIT';
?>
--EXPECT--
NESTING_LIMIT
--CLEAN--
<?php
unset($root, $current, $next, $output);
