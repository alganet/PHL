--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
fputcsv should write CSV lines to file
--FILE--
<?php
$fname = tempnam(sys_get_temp_dir(), 'ph7_fputcsv_');
$fp = fopen($fname, 'w');
if ($fp) {
    fputcsv($fp, array('a', 'b', 'c'), ',', '"', '\\');
    fclose($fp);
    echo trim(file_get_contents($fname)) . PHP_EOL;
}
?>
--EXPECT--
a,b,c
--CLEAN--
<?php
@unlink($fname);
unset($fname, $fp);
