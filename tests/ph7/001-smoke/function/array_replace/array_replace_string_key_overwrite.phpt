--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_replace overwrites values at matching string keys
--FILE--
<?php
$a = array('k' => 'va');
$b = array('k' => 'vb');
$r = array_replace($a, $b);
echo $r['k'];
?>
--EXPECT--
vb
--CLEAN--
<?php
unset($a, $b, $r);
