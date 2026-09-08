--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
IO failures warn with php's message and return false
--SKIPIF--
skip: needs windows and macos fixes
?>
--FILE--
<?php
// php reports an IO failure as an E_WARNING naming the function, the path and the
// system reason -- and returns false. PH7 raised an E_ERROR (or stayed silent).
set_error_handler(function ($no, $msg) {
    echo "[$no] ", str_replace(sys_get_temp_dir(), '<TMP>', $msg), "\n";
    return true;
});
$ioMissing = '/nonexistent-dir-xyz/f';

echo var_export(file_get_contents($ioMissing), true), "\n";
echo var_export(fopen($ioMissing, 'r'), true), "\n";
echo var_export(readfile($ioMissing), true), "\n";
echo var_export(unlink($ioMissing), true), "\n";
echo var_export(rmdir($ioMissing), true), "\n";
echo var_export(rename($ioMissing, '/tmp/zzz'), true), "\n";
echo var_export(filesize($ioMissing), true), "\n";

// tempnam() must CREATE the file, and the temp dir carries no trailing separator.
$ioTmp = tempnam(sys_get_temp_dir(), 'phl_');
echo 'tempnam created: ', var_export(file_exists($ioTmp), true), "\n";
echo 'no double sep: ', var_export(strpos($ioTmp, '//') === false, true), "\n";
unlink($ioTmp);
echo 'unlinked: ', var_export(file_exists($ioTmp), true), "\n";
restore_error_handler();
?>
--EXPECT--
[2] file_get_contents(/nonexistent-dir-xyz/f): Failed to open stream: No such file or directory
false
[2] fopen(/nonexistent-dir-xyz/f): Failed to open stream: No such file or directory
false
[2] readfile(/nonexistent-dir-xyz/f): Failed to open stream: No such file or directory
false
[2] unlink(/nonexistent-dir-xyz/f): No such file or directory
false
[2] rmdir(/nonexistent-dir-xyz/f): No such file or directory
false
[2] rename(/nonexistent-dir-xyz/f,<TMP>/zzz): No such file or directory
false
[2] filesize(): stat failed for /nonexistent-dir-xyz/f
false
tempnam created: true
no double sep: true
unlinked: false
--CLEAN--
<?php
