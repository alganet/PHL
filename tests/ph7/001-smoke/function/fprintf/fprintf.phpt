--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
fprintf should write formatted content to a file
--FILE--
<?php
$fname = tempnam(sys_get_temp_dir(), 'ph7_fprintf_');
$fp = fopen($fname, 'w');
if ($fp) {
    fprintf($fp, "%s %d", 'Value', 42);
    fclose($fp);
    echo trim(file_get_contents($fname)) . PHP_EOL;
}
?>
--EXPECT--
Value 42
--CLEAN--
<?php
@unlink($fname);
unset($fname, $fp);
