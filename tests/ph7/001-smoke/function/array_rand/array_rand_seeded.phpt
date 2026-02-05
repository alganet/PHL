--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_rand should be deterministic when srand is used
--FILE--
<?php
srand(42);
$in = array('a' => 1, 'b' => 2, 'c' => 3);
$k = array_rand($in);
// just check that key exists in input
echo (isset($in[$k]) ? 'ok' : 'fail') . PHP_EOL;
// multi-key
srand(42);
$keys = array_rand($in, 2);
echo is_array($keys) && count($keys) === 2 ? 'ok' : 'fail';
?>
--EXPECT--
ok
ok
--CLEAN--
<?php
unset($in, $k, $keys);
