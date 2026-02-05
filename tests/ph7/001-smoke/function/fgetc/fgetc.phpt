--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
fgetc reads characters one at a time
--FILE--
<?php
$fname = tempnam(sys_get_temp_dir(), 'ph7_fgetc_');
file_put_contents($fname, 'ABC');
$fp = fopen($fname, 'r');
if ($fp) {
    echo "c1=" . fgetc($fp) . PHP_EOL;
    echo "c2=" . fgetc($fp) . PHP_EOL;
    fclose($fp);
}
@unlink($fname);
?>
--EXPECT--
c1=A
c2=B
--CLEAN--
<?php
@unlink($fname);
unset($fname, $fp);
