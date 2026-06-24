--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Anonymous class: constructor property promotion
--FILE--
<?php
$o = new class(9) {
    function __construct(public int $n) {}
};
echo $o->n, "\n";
?>
--EXPECT--
9
--CLEAN--
<?php
unset($o);
