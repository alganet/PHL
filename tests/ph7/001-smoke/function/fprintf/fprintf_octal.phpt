--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
fprintf() formats an octal value with %o
--FILE--
<?php
$fname = tempnam(sys_get_temp_dir(), 'ph7_fprintf_octal_');
$fp = fopen($fname, 'w');
if ($fp) {
    fprintf($fp, "Octal: %o", 63);
    fclose($fp);
    echo trim(file_get_contents($fname)) . PHP_EOL;
}
?>
--EXPECT--
Octal: 77
--CLEAN--
<?php
@unlink($fname);
unset($fname, $fp);
