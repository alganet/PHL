--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Closure cannot be instantiated with new
--FILE--
<?php
echo var_export(class_exists("Closure"), true), "\n";
try { $c = new Closure(); } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
?>
--EXPECT--
true
Error: Instantiation of class Closure is not allowed
--CLEAN--
<?php
