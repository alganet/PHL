--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
readfile should output file contents
--FILE--
<?php
$fname = tempnam(sys_get_temp_dir(), 'ph7_rf_');
file_put_contents($fname, 'READFILE');
readfile($fname);
?>
--EXPECT--
READFILE
--CLEAN--
<?php
@unlink($fname);
unset($fname);
