--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
filter_input: ArgumentCountError (too few args) and ValueError (bad type)
--FILE--
<?php
try { filter_input(INPUT_GET); }
catch (\Throwable $e) { echo get_class($e),": ",$e->getMessage(),"\n"; }

try { filter_input(); }
catch (\Throwable $e) { echo get_class($e),": ",$e->getMessage(),"\n"; }

try { filter_input(99, 'x'); }
catch (\Throwable $e) { echo get_class($e),": ",$e->getMessage(),"\n"; }
?>
--EXPECT--
ArgumentCountError: filter_input() expects at least 2 arguments, 1 given
ArgumentCountError: filter_input() expects at least 2 arguments, 0 given
ValueError: filter_input(): Argument #1 ($type) must be an INPUT_* constant
