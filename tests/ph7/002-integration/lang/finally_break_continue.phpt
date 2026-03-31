--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Finally block executes on continue in loop
--FILE--
<?php
for ($i = 0; $i < 3; $i++) {
    try {
        if ($i == 1) continue;
        echo "body $i\n";
    } finally {
        echo "finally $i\n";
    }
}
?>
--EXPECT--
body 0
finally 0
finally 1
body 2
finally 2
--CLEAN--
<?php
