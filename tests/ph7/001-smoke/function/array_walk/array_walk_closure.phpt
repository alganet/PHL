--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_walk works with closure callback
--FILE--
<?php
$a = array('x' => 10, 'y' => 20);
array_walk($a, function($v, $k) {
    echo $k . '=' . $v . "\n";
});
?>
--EXPECT--
x=10
y=20
--CLEAN--
<?php
unset($a);
