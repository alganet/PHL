--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An array literal key is its own, whatever the stack slot last held
--FILE--
<?php
echo "-- a constant key after a keyless literal at the same depth\n";
function k1() { json_encode([1, 2, 3]); return json_encode([E_USER_ERROR => 'x', E_USER_WARNING => 'y']); }
echo k1(), "\n";
function k2() { json_encode([1, 2, 3, 4, 5]); return json_encode([SORT_STRING => 'x', SORT_FLAG_CASE => 'y']); }
echo k2(), "\n";
function k3() { json_encode([1, 2, 3, 4, 5]); return json_encode([E_USER_ERROR => 'x', E_USER_WARNING => 'y', E_USER_NOTICE => 'z']); }
echo k3(), "\n";
function k4() { $t = json_encode([1, 2, 3, 4, 5]); return json_encode([PHP_INT_SIZE => 'x', PHP_FLOAT_DIG => 'y']); }
echo k4(), "\n";
echo "-- the same, one call deeper\n";
function k5() { return json_encode([ENT_QUOTES => 'x', ENT_HTML5 => 'y']); }
function k6() { json_encode([1, 2, 3, 4, 5]); return k5(); }
echo k6(), "\n";
echo "-- a spread source marker must not outlive its literal either\n";
function s1() { $a = [1, 2]; json_encode([...$a]); return json_encode([$a]); }
echo s1(), "\n";
function s2() { $a = ['p' => 1]; json_encode(['q' => 0, ...$a]); return json_encode(['q' => 0, $a]); }
echo s2(), "\n";
echo "-- and an absent key is still absent when it is meant to be\n";
function k7() { json_encode([E_USER_ERROR => 'x']); return json_encode(['a', 'b']); }
echo k7(), "\n";
?>
--EXPECT--
-- a constant key after a keyless literal at the same depth
{"256":"x","512":"y"}
{"2":"x","8":"y"}
{"256":"x","512":"y","1024":"z"}
{"8":"x","15":"y"}
-- the same, one call deeper
{"3":"x","48":"y"}
-- a spread source marker must not outlive its literal either
[[1,2]]
{"q":0,"0":{"p":1}}
-- and an absent key is still absent when it is meant to be
["a","b"]
