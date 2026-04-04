--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Generator: multiple independent instances of the same generator function
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '5.5.0', '<')) echo 'skip Requires PHP 5.5+'; ?>
--FILE--
<?php
function counter() {
    $i = 0;
    while (true) {
        yield $i;
        $i++;
    }
}

$a = counter();
$b = counter();

$a->rewind();
$b->rewind();

echo "a=" . $a->current() . "\n";
echo "b=" . $b->current() . "\n";

$a->next();
$a->next();
$a->next();

echo "a=" . $a->current() . "\n";
echo "b=" . $b->current() . "\n";

$b->next();

echo "a=" . $a->current() . "\n";
echo "b=" . $b->current() . "\n";
?>
--EXPECT--
a=0
b=0
a=3
b=0
a=3
b=1
--CLEAN--
<?php
