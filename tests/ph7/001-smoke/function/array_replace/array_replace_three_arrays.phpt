--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_replace with three arrays where later arrays overwrite earlier ones
--FILE--
<?php
$a = array(0 => 'a');
$b = array(0 => 'b');
$c = array(0 => 'c');
$r = array_replace($a, $b, $c);
echo $r[0];
?>
--EXPECT--
c
--CLEAN--
<?php
unset($a, $b, $c, $r);
