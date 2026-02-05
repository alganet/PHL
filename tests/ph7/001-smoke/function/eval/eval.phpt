--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
eval basic code evaluation
--SKIPIF--
<?php
if (!function_exists('eval')) { echo 'skip: eval not available'; }
?>
--FILE--
<?php
$result = eval('return 42;');
echo $result . "\n";
$result = eval('$x = 10; return $x * 2;');
echo $result . "\n";
?>
--EXPECT--
42
20
--CLEAN--
<?php
unset($result);
