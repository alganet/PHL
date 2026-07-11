--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
fprintf() rejects the C '#' alternate-form flag with a ValueError, writing nothing (PHP 8)
--FILE--
<?php
$fname = tempnam(sys_get_temp_dir(), 'ph7_fprintf_hash_');
$fp = fopen($fname, 'w');
if ($fp) {
    try {
        fprintf($fp, "Lower: %#x Upper: %#X", 255, 255);
        echo "NO_ERROR\n";
    } catch (\ValueError $e) {
        echo $e->getMessage(), "\n";
    }
    fclose($fp);
    // The throw happens before any output, so the file stays empty.
    echo "written=" . strlen(file_get_contents($fname)) . "\n";
}
?>
--EXPECT--
Unknown format specifier "#"
written=0
--CLEAN--
<?php
@unlink($fname);
unset($fname, $fp);
