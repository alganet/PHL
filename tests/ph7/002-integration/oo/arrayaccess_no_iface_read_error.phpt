--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Subscript read on object without ArrayAccess throws Error
--FILE--
<?php
class NoAccess {}
try {
    $o = new NoAccess();
    $x = $o["k"];
} catch (Error $e) {
    echo "caught: ", $e->getMessage(), "\n";
}
?>
--EXPECT--
caught: Cannot use object of type NoAccess as array
--CLEAN--
<?php
