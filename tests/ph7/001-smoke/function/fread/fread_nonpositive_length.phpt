--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
fread with a non-positive length throws ValueError
--FILE--
<?php
$fp = fopen('php://memory', 'r+');
fwrite($fp, 'Hello World');
rewind($fp);
foreach ([-5, 0] as $len) {
    try {
        fread($fp, $len);
        echo "NO_ERROR\n";
    } catch (\ValueError $e) {
        echo $e->getMessage(), "\n";
    }
}
fclose($fp);
?>
--EXPECT--
fread(): Argument #2 ($length) must be greater than 0
fread(): Argument #2 ($length) must be greater than 0
--CLEAN--
<?php
