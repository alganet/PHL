--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_rot13 argument contract
--FILE--
<?php
// Scalars coerce like any string parameter; arrays refuse; arity is exact.
var_dump(str_rot13(123));
var_dump(str_rot13(true));
try { str_rot13(); } catch (ArgumentCountError $e) { echo $e->getMessage(), "\n"; }
try { str_rot13("a", "b"); } catch (ArgumentCountError $e) { echo $e->getMessage(), "\n"; }
try { str_rot13([1]); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
?>
--EXPECT--
string(3) "123"
string(1) "1"
str_rot13() expects exactly 1 argument, 0 given
str_rot13() expects exactly 1 argument, 2 given
str_rot13(): Argument #1 ($string) must be of type string, array given
