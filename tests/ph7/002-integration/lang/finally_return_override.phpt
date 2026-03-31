--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Return in finally block overrides return in try block
--FILE--
<?php
function test() {
    try {
        return "try";
    } finally {
        return "finally";
    }
}
echo test() . "\n";
?>
--EXPECT--
finally
--CLEAN--
<?php
