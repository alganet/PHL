--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Multi-catch: works with finally block
--FILE--
<?php
class McFinA extends Exception {}

try {
    throw new McFinA("test");
} catch (McFinA | Exception $e) {
    echo "caught: " . $e->getMessage() . "\n";
} finally {
    echo "finally\n";
}
?>
--EXPECT--
caught: test
finally
--CLEAN--
<?php
