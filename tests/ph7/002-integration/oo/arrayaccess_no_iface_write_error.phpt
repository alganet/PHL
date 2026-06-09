--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Subscript write on object without ArrayAccess throws Error
--FILE--
<?php
class NoAccessW {}
try {
    $o = new NoAccessW();
    $o["k"] = 1;
} catch (Error $e) {
    echo "caught: ", $e->getMessage(), "\n";
}
?>
--EXPECT--
caught: Cannot use object of type NoAccessW as array
--CLEAN--
<?php
