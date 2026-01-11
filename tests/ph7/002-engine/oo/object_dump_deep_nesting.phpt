--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Object dump deep nesting limit
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
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

ob_start();
var_dump($root);
$output = ob_get_clean();
echo strpos($output, 'Nesting limit reached') !== false ? 'NESTING_LIMIT' : 'NO_LIMIT';
?>
--EXPECT--
NESTING_LIMIT