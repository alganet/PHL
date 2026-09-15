--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
eval() return values for empty, whitespace and explicit returns
--DESCRIPTION--
php: eval('') compiles nothing and yields FALSE, while a whitespace-only chunk compiles and
yields NULL. Guarded on function_exists('eval') before, which is FALSE in php (eval is a
language construct), so this never ran under the oracle.
--FILE--
<?php
var_dump(eval(''));
var_dump(eval('   '));
var_dump(eval('return null;'));
var_dump(eval('return 42;'));
?>
--EXPECT--
bool(false)
NULL
NULL
int(42)
