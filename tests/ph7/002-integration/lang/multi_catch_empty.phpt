--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Empty catch clause is a parse error
--FILE--
<?php
try {
    throw new Exception("test");
} catch () {
    echo "should not reach here\n";
}
?>
--EXPECTF--
%s Parse error:  syntax error, unexpected %s %s
--CLEAN--
<?php
