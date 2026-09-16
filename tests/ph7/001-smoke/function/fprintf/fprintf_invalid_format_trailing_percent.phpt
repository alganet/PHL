--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
fprintf with a format ending in a dangling '%' raises php's ValueError and writes nothing (was a bare skip hiding PHL's silent truncation)
--FILE--
<?php
$fname = tempnam(sys_get_temp_dir(), 'ph7_fprintf_');
$fp = fopen($fname, 'w');
try {
    fprintf($fp, "test %", 1);
    echo "no throw\n";
} catch (ValueError $e) {
    echo get_class($e), ": ", $e->getMessage(), "\n";
}
fclose($fp);
echo "Content: '", file_get_contents($fname), "'\n";
?>
--EXPECT--
ValueError: Missing format specifier at end of string
Content: ''
--CLEAN--
<?php
@unlink($fname);
unset($fname, $fp, $e);
