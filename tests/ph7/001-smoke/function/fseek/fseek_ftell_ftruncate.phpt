--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
fseek/ftell/ftruncate basic behavior
--FILE--
<?php
$fname = tempnam(sys_get_temp_dir(), 'ph7_fseek_');
file_put_contents($fname, str_repeat('X', 10));
$fp = fopen($fname, 'r+');
if ($fp) {
    // Seek to position 5
    fseek($fp, 5);
    echo "pos=" . ftell($fp) . PHP_EOL;
    // Truncate to 3
    ftruncate($fp, 3);
    fflush($fp);
    fclose($fp);
    echo "size=" . filesize($fname) . PHP_EOL;
}
?>
--EXPECT--
pos=5
size=3
--CLEAN--
<?php
@unlink($fname);
unset($fname, $fp);
