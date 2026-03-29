--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
finally block executes on every loop iteration
--FILE--
<?php
for ($i = 0; $i < 3; $i++) {
    try {
        echo "try $i\n";
    } finally {
        echo "finally $i\n";
    }
}
?>
--EXPECT--
try 0
finally 0
try 1
finally 1
try 2
finally 2
--CLEAN--
<?php
