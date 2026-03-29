--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
try/finally without catch block
--FILE--
<?php
try {
    echo "try\n";
} finally {
    echo "finally\n";
}
echo "after\n";
?>
--EXPECT--
try
finally
after
--CLEAN--
<?php
