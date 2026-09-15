--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
throw with a non-object operand is a catchable Error at runtime, not a compile error
--FILE--
<?php
foreach ([1, "str", null, 3.5, [1, 2], true] as $v) {
    try { throw $v; } catch (\Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
}
try { throw 5; } catch (\Error $e) { echo "literal: ", $e->getMessage(), "\n"; }
try { throw new stdClass(); } catch (\Error $e) { echo "non-throwable: ", $e->getMessage(), "\n"; }
try { throw new Exception("real"); } catch (\Exception $e) { echo "real: ", $e->getMessage(), "\n"; }
echo "alive\n";
?>
--EXPECT--
Error: Can only throw objects
Error: Can only throw objects
Error: Can only throw objects
Error: Can only throw objects
Error: Can only throw objects
Error: Can only throw objects
literal: Can only throw objects
non-throwable: Cannot throw objects that do not implement Throwable
real: real
alive
