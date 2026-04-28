--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
random_int throws TypeError on non-int, non-numeric arguments
--SKIPIF--
<?php
if (!function_exists('random_int')) { echo 'skip: random_int not available'; }
?>
--FILE--
<?php
$arr = array();
try { random_int($arr, 5); echo "n1\n"; } catch (TypeError $e) { echo "te1\n"; }
try { random_int(0, $arr); echo "n2\n"; } catch (TypeError $e) { echo "te2\n"; }
try { random_int("abc", 5); echo "ns\n"; } catch (TypeError $e) { echo "ts\n"; }
?>
--EXPECT--
te1
te2
ts
--CLEAN--
<?php
