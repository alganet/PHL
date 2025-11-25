--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
fgetcsv should parse CSV lines
--FILE--
<?php
$fname = tempnam(sys_get_temp_dir(), 'ph7_getcsv_');
file_put_contents($fname, "a,b,c\n");
$fp = fopen($fname, 'r');
if ($fp) {
    $row = fgetcsv($fp, null, ',', '"', '\\');
    if ($row) {
        echo implode(',', $row) . PHP_EOL;
    }
    fclose($fp);
}
@unlink($fname);
?>
--EXPECT--
a,b,c
--CLEAN--
<?php
@unlink($fname);
?>
