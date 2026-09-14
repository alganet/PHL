--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A lossy float / float-string to a builtin int parameter is a TypeError (php deprecates; PHL removes)
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip PHL removes what php only deprecates'; ?>
--FILE--
<?php
// php only DEPRECATES a lossy float->int on a builtin int parameter; PHL rejects it.
try { str_repeat('x', 2.7); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
try { intdiv(7.9, 2); }       catch (TypeError $e) { echo $e->getMessage(), "\n"; }
try { str_repeat('y', '2.5'); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
// an INTEGRAL float loses nothing, so it is accepted silently
echo str_repeat('z', 3.0), "\n";
echo substr('abcdef', 2.0), "\n";
echo intdiv(7, 2), "\n";
?>
--EXPECT--
str_repeat(): Argument #2 ($times) must be of type int, float given
intdiv(): Argument #1 ($num1) must be of type int, float given
str_repeat(): Argument #2 ($times) must be of type int, string given
zzz
cdef
3
