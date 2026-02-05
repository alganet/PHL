--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
fwrite/fread basic file write and read
--FILE--
<?php
$fname = tempnam(sys_get_temp_dir(), 'ph7_fwrite_');
$fp = fopen($fname, 'w');
if ($fp) {
    fwrite($fp, "Hello World");
    fclose($fp);
    $fp2 = fopen($fname, 'r');
    $data = fread($fp2, 1024);
    fclose($fp2);
    echo "data=" . $data . PHP_EOL;
} else {
    echo "open_failed\n";
}
?>
--EXPECT--
data=Hello World
--CLEAN--
<?php
@unlink($fname);
unset($fname, $fp, $fp2, $data);
