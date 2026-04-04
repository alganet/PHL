--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Generator: yield as expression with send()
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '5.5.0', '<')) echo 'skip Requires PHP 5.5+'; ?>
--FILE--
<?php
function gen() {
    $received = yield 'first';
    echo "received: $received\n";
    $received = yield 'second';
    echo "received: $received\n";
}

$g = gen();
$g->rewind();
echo $g->current() . "\n";
$g->send('hello');
echo $g->current() . "\n";
$g->send('world');
?>
--EXPECT--
first
received: hello
second
received: world
--CLEAN--
<?php
