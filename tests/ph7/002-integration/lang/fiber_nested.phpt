--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Fiber: nested fibers (fiber A starts fiber B)
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '8.1.0', '<')) echo 'skip Requires PHP 8.1+'; ?>
--FILE--
<?php
function innerFiber() {
    echo "inner start\n";
    Fiber::suspend('from inner');
    echo "inner end\n";
}

function outerFiber() {
    echo "outer start\n";
    $inner = new Fiber('innerFiber');
    $v = $inner->start();
    echo "outer got: $v\n";
    Fiber::suspend('from outer');
    $inner->resume();
    echo "outer end\n";
}

$outer = new Fiber('outerFiber');
$v = $outer->start();
echo "main got: $v\n";
$outer->resume();
echo "main done\n";
?>
--EXPECT--
outer start
inner start
outer got: from inner
main got: from outer
inner end
outer end
main done
--CLEAN--
<?php
