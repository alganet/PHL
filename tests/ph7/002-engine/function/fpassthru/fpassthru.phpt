--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
fpassthru should output the file content
--FILE--
<?php
$fname = tempnam(sys_get_temp_dir(), 'ph7_pass_');
file_put_contents($fname, 'PASSTHRU');
$fp = fopen($fname, 'r');
if ($fp) {
    fpassthru($fp);
    fclose($fp);
}
?>
--EXPECT--
PASSTHRU
--CLEAN--
<?php
@unlink($fname);
?>
