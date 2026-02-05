--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
fprintf with space flag for positive numbers
--FILE--
<?php
$fname = tempnam(sys_get_temp_dir(), 'ph7_fprintf_space_');
$fp = fopen($fname, 'w');
if ($fp) {
    fprintf($fp, "% d", 42);
    fclose($fp);
    echo trim(file_get_contents($fname)) . PHP_EOL;
}
?>
--EXPECT--
 42
--CLEAN--
<?php
@unlink($fname);
unset($fname, $fp);
