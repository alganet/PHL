--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Multi-catch: subclass matches parent listed in multi-catch
--FILE--
<?php
class McInhA extends Exception {}
class McInhB extends McInhA {}

try {
    throw new McInhB("child");
} catch (McInhA | Exception $e) {
    echo $e->getMessage() . "\n";
}
?>
--EXPECT--
child
--CLEAN--
<?php
