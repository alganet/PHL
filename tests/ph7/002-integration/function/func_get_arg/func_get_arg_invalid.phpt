--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
func_get_arg/func_num_args/func_get_args outside a function throw, as php does
--DESCRIPTION--
php raises catchable Errors here. PHL warned and returned FALSE -- and func_num_args()
returned int(-1), a perfectly usable number, so comparisons and arithmetic on it silently
took the wrong branch. An out-of-range position is php's ValueError. Asserted from behind
a bare skip before, so none of it ran under the oracle.
--FILE--
<?php
try { func_get_arg(0); } catch (\Error $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { func_num_args(); } catch (\Error $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { func_get_args(); } catch (\Error $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

function fgai() { return func_get_arg(0); }
try { fgai(); } catch (\Error $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

function fgaok($a, $b) { return func_num_args() . '/' . implode(',', func_get_args()) . '/' . func_get_arg(1); }
echo fgaok(7, 8), "\n";
?>
--EXPECT--
Error: func_get_arg() cannot be called from the global scope
Error: func_num_args() must be called from a function context
Error: func_get_args() cannot be called from the global scope
ValueError: func_get_arg(): Argument #1 ($position) must be less than the number of the arguments passed to the currently executed function
2/7,8/8
