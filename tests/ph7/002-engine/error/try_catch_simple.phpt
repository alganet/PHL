--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Simple try-catch block
--FILE--
<?php
try {
    throw new Exception("Test exception");
} catch (Exception $e) {
    echo "Caught: " . $e->getMessage();
}
?>
--EXPECT--
Caught: Test exception

--CLEAN--
<?php
// Nothing to clean
?>