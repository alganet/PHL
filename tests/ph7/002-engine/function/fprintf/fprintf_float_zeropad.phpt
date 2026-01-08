--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
fprintf should handle zero-padding for floating point numbers
--FILE--
<?php
$fname = tempnam(sys_get_temp_dir(), 'ph7_fprintf_zeropad_');
$fp = fopen($fname, 'w');
if ($fp) {
    // Test zero-padding with floating point numbers
    fprintf($fp, "%05.2f", 3.14);
    fclose($fp);
    echo trim(file_get_contents($fname)) . PHP_EOL;
}
?>
--EXPECT--
03.14
--CLEAN--
<?php
@unlink($fname);
?>