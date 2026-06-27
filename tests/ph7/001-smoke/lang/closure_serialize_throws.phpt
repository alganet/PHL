--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Closures cannot be serialized (Exception, no internals leaked)
--FILE--
<?php
try { serialize(fn() => 1); } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { serialize(function(){}); } catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
?>
--EXPECT--
Exception: Serialization of 'Closure' is not allowed
Exception: Serialization of 'Closure' is not allowed
--CLEAN--
<?php
