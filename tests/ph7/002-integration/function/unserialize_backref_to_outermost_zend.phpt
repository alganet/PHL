--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php: an R: naming the OUTERMOST value makes it refer to itself (zend half of the twin pair)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
}
?>
--FILE--
<?php
$payloads = [
    'the whole payload'  => 'a:1:{i:0;R:1;}',
    'one level down'     => 'a:2:{i:0;a:1:{i:0;R:1;}i:1;i:9;}',
];
foreach ($payloads as $label => $payload) {
    echo $label, ': ';
    $v = @unserialize($payload);
    var_dump($v === false ? false : count($v));
}
?>
--EXPECT--
the whole payload: int(1)
one level down: int(2)
--CLEAN--
<?php
