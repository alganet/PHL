--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
getcwd() rejects extra arguments
--DESCRIPTION--
php enforces a MAXIMUM argument count for internal functions; PHL only did where a
builtin hand-rolled the check, so getcwd("unexpected") silently ignored the extra
argument. Asserted the permissive behavior from behind a bare skip before.
--FILE--
<?php
$cwd = getcwd();
try { getcwd("unexpected"); } catch (\ArgumentCountError $e) { echo $e->getMessage(), "\n"; }
echo "cwd_type=", gettype($cwd), "\n";
echo "cwd_eq=", ($cwd === getcwd() ? 'same' : 'diff'), "\n";
?>
--EXPECT--
getcwd() expects exactly 0 arguments, 1 given
cwd_type=string
cwd_eq=same
