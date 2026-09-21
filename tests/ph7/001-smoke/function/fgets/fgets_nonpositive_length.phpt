--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
fgets with a non-positive length throws ValueError; NULL reads the whole line
--FILE--
<?php
$fp = fopen('php://memory', 'r+');
fwrite($fp, "Hello World\n");
rewind($fp);
foreach ([0, -1] as $len) {
    try {
        fgets($fp, $len);
        echo "NO_ERROR\n";
    } catch (\ValueError $e) {
        echo $e->getMessage(), "\n";
    }
}
rewind($fp);
echo trim(fgets($fp, null)), "\n";
fclose($fp);
?>
--EXPECT--
fgets(): Argument #2 ($length) must be greater than 0
fgets(): Argument #2 ($length) must be greater than 0
Hello World
--CLEAN--
<?php
