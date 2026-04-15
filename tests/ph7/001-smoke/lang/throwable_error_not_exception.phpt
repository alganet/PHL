--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Throwable: Error is not an Exception (PHP 7+ hierarchy)
--FILE--
<?php
echo (new Error() instanceof Exception) ? "yes\n" : "no\n";
try {
    throw new Error("e");
} catch (Exception $e) {
    echo "wrong:Exception\n";
} catch (Error $e) {
    echo "right:Error:", $e->getMessage(), "\n";
}
?>
--EXPECT--
no
right:Error:e
--CLEAN--
<?php
