--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
fprintf should handle character formatting
--FILE--
<?php
$fname = tempnam(sys_get_temp_dir(), 'ph7_fprintf_char_');
$fp = fopen($fname, 'w');
if ($fp) {
    fprintf($fp, "Char: %c", 65);
    fclose($fp);
    echo trim(file_get_contents($fname)) . PHP_EOL;
}
?>
--EXPECT--
Char: A
--CLEAN--
<?php
@unlink($fname);
unset($fname, $fp);
