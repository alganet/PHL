--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_reverse on a single-element array returns same element
--FILE--
<?php
$r = array_reverse(array(42));
echo $r[0] . PHP_EOL;
?>
--EXPECT--
42
--CLEAN--
<?php
unset($r);
