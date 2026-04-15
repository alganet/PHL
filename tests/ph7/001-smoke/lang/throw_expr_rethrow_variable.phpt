--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
throw expression: re-throw of a caught exception via a plain variable
--FILE--
<?php
try {
    try {
        throw new Exception('original');
    } catch (Exception $e) {
        echo "inner: ", $e->getMessage(), "\n";
        $last = $e;
        throw $last;
    }
} catch (Exception $e) {
    echo "outer: ", $e->getMessage(), "\n";
}
?>
--EXPECT--
inner: original
outer: original
--CLEAN--
<?php
