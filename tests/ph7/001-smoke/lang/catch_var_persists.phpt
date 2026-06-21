--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
the caught exception variable is still set after the catch block
--FILE--
<?php
try {
    throw new Exception("boom");
} catch (Exception $e) {
    echo "in: " . $e->getMessage() . "\n";
}
echo "after: " . $e->getMessage() . "\n";
?>
--EXPECT--
in: boom
after: boom
--CLEAN--
<?php
