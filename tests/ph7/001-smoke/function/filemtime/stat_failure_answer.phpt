--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A failed stat answers false and warns, for every member of the family
--FILE--
<?php
/* php's php_stat() raises one E_WARNING per member and answers FALSE. PHL
 * answered the VFS's raw -1 for the three time readers and the STRING "unknown"
 * for filetype() -- both truthy, and both values a successful call can also
 * produce, so `if (filemtime($f))` took the found branch for a file that was not
 * there. The four field readers answered false but said nothing at all. */
$missing = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'phl_stat_missing_xyz';
@unlink($missing);

set_error_handler(function ($no, $msg) use ($missing) {
	echo '[', $no, '] ', str_replace($missing, '<MISSING>', $msg), "\n";
	return true;
});
foreach (['filemtime', 'fileatime', 'filectime', 'filetype', 'filesize',
          'fileperms', 'fileowner', 'filegroup', 'fileinode', 'stat', 'lstat'] as $fn) {
	$r = $fn($missing);
	echo str_pad($fn, 12), var_export($r, true), "\n";
}
restore_error_handler();

/* A path that IS there still answers what it answered before. */
$there = tempnam(sys_get_temp_dir(), 'phl_stat_');
file_put_contents($there, 'abcd');
echo 'filesize   ', var_export(filesize($there), true), "\n";
echo 'filetype   ', var_export(filetype($there), true), "\n";
echo 'filetype d ', var_export(filetype(sys_get_temp_dir()), true), "\n";
echo 'mtime type ', get_debug_type(filemtime($there)), "\n";
echo 'atime type ', get_debug_type(fileatime($there)), "\n";
echo 'ctime type ', get_debug_type(filectime($there)), "\n";
echo 'perms type ', get_debug_type(fileperms($there)), "\n";
echo 'owner type ', get_debug_type(fileowner($there)), "\n";
echo 'group type ', get_debug_type(filegroup($there)), "\n";
echo 'inode type ', get_debug_type(fileinode($there)), "\n";
echo 'perms match ', var_export(fileperms($there) === stat($there)['mode'], true), "\n";
echo 'owner match ', var_export(fileowner($there) === stat($there)['uid'], true), "\n";
@unlink($there);
?>
--EXPECT--
[2] filemtime(): stat failed for <MISSING>
filemtime   false
[2] fileatime(): stat failed for <MISSING>
fileatime   false
[2] filectime(): stat failed for <MISSING>
filectime   false
[2] filetype(): Lstat failed for <MISSING>
filetype    false
[2] filesize(): stat failed for <MISSING>
filesize    false
[2] fileperms(): stat failed for <MISSING>
fileperms   false
[2] fileowner(): stat failed for <MISSING>
fileowner   false
[2] filegroup(): stat failed for <MISSING>
filegroup   false
[2] fileinode(): stat failed for <MISSING>
fileinode   false
[2] stat(): stat failed for <MISSING>
stat        false
[2] lstat(): Lstat failed for <MISSING>
lstat       false
filesize   4
filetype   'file'
filetype d 'dir'
mtime type int
atime type int
ctime type int
perms type int
owner type int
group type int
inode type int
perms match true
owner match true
