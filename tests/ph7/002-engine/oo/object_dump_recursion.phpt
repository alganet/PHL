--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
object dump recursion limit
--FILE--
<?php
class Test {
    public $prop;
}

$root = new Test();
$current = $root;

for ($i = 0; $i < 35; $i++) {
    $next = new Test();
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