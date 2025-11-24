--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: file_exists and filesize for temp files
--FILE--
<?php
$fname = tempnam(sys_get_temp_dir(), 'ph7_fexist_');
file_put_contents($fname, "abcd");
echo "file_exists=" . (file_exists($fname) ? 'true' : 'false') . "\n";
echo "filesize=" . filesize($fname) . "\n";
?>
--EXPECT--
file_exists=true
filesize=4
--CLEAN--
<?php
if (isset($fname) && file_exists($fname)) unlink($fname);
unset($fname);
?>
