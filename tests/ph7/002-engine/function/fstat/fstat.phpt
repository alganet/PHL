--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
fstat should return an associative array including size
--FILE--
<?php
$fname = tempnam(sys_get_temp_dir(), 'ph7_fstat_');
file_put_contents($fname, 'X');
$fp = fopen($fname, 'r');
if ($fp) {
    $st = fstat($fp);
    echo "size=" . $st['size'] . PHP_EOL;
    fclose($fp);
}
?>
--EXPECT--
size=1
--CLEAN--
<?php
@unlink($fname);
?>
