--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
fgetcsv with a negative length throws ValueError; 0 means "no limit"
--FILE--
<?php
$fp = fopen('php://memory', 'r+');
fwrite($fp, "a,b,c\n");
rewind($fp);
try {
    fgetcsv($fp, -5, ',', '"', '\\');
    echo "NO_ERROR\n";
} catch (\ValueError $e) {
    echo $e->getMessage(), "\n";
}
rewind($fp);
echo implode('|', fgetcsv($fp, 0, ',', '"', '\\')), "\n";
fclose($fp);
?>
--EXPECT--
fgetcsv(): Argument #2 ($length) must be between 0 and 9223372036854775806
a|b|c
--CLEAN--
<?php
