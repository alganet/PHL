--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Multi-catch trailing pipe is a parse error
--FILE--
<?php
class McTrailA extends Exception {}
try {
    throw new McTrailA("test");
} catch (McTrailA | $e) {
    echo "should not reach here\n";
}
?>
--EXPECTF--
%s Parse error:  syntax error, unexpected %s %s
--CLEAN--
<?php
