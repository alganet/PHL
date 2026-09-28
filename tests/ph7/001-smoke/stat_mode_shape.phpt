--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A stat answer's `mode` says what the thing IS, on every platform
--DESCRIPTION--
Windows keeps no mode field, so php SYNTHESISES one from the file attributes: a
directory is S_IFDIR with the three execute bits, anything else is S_IFREG, the
read-only attribute is the difference between 0444 and 0666, and a name ending
in .exe/.com/.bat/.cmd adds the execute bits back. PHL answered a flat 0 there,
so `($st['mode'] & 0170000) === 0100000` -- how a portable is-this-a-file is
written against a stat record -- was false for everything and fileperms()
answered 0 on a whole platform. stat() of a DIRECTORY answered false outright,
because the handle it wanted cannot be opened without FILE_FLAG_BACKUP_SEMANTICS,
and that took fileperms(), fileowner(), filegroup(), fileinode() and
filesize() down with it.

What this pins is the SHAPE both platforms share; the exact numbers are the
platform's own.
--FILE--
<?php
$smsDir = sys_get_temp_dir() . '/phl_sms_' . getmypid();
@mkdir($smsDir);
$smsFile = $smsDir . '/plain.txt';
$smsRo   = $smsDir . '/ro.txt';
file_put_contents($smsFile, '0123456789');
file_put_contents($smsRo, 'x');
chmod($smsRo, 0444);

function smsShow($smsLabel, $smsVal) {
    echo str_pad($smsLabel, 30), ' => ', var_export($smsVal, true), "\n";
}

$smsSt = stat($smsFile);
smsShow('file is a stat', is_array($smsSt));
smsShow('file S_IFREG', ($smsSt['mode'] & 0170000) === 0100000);
smsShow('file readable bits', ($smsSt['mode'] & 0444) !== 0);
smsShow('file writable bits', ($smsSt['mode'] & 0200) !== 0);
smsShow('file size', $smsSt['size']);
smsShow('fileperms is that mode', fileperms($smsFile) === $smsSt['mode']);
smsShow('lstat agrees', lstat($smsFile)['mode'] === $smsSt['mode']);

$smsH = fopen($smsFile, 'r');
$smsFs = fstat($smsH);
smsShow('fstat S_IFREG', ($smsFs['mode'] & 0170000) === 0100000);
smsShow('fstat size', $smsFs['size']);
fclose($smsH);

$smsRoSt = stat($smsRo);
smsShow('read-only S_IFREG', ($smsRoSt['mode'] & 0170000) === 0100000);
smsShow('read-only no write bit', ($smsRoSt['mode'] & 0222) === 0);
smsShow('read-only is_writable', is_writable($smsRo));

$smsDirSt = stat($smsDir);
smsShow('dir is a stat', is_array($smsDirSt));
smsShow('dir S_IFDIR', ($smsDirSt['mode'] & 0170000) === 0040000);
smsShow('dir execute bits', ($smsDirSt['mode'] & 0111) !== 0);
smsShow('dir fileperms', fileperms($smsDir) === $smsDirSt['mode']);
/* A directory's SIZE is the platform's own -- 4096 on this filesystem, 0 for a
 * small one on Windows -- so what is pinned is that there IS one and that both
 * doors agree on it. */
smsShow('dir size is an int', is_int(filesize($smsDir)));
smsShow('dir size is the stat one', filesize($smsDir) === $smsDirSt['size']);
smsShow('dir inode is the stat one', fileinode($smsDir) === $smsDirSt['ino']);
smsShow('dir owner is the stat one', fileowner($smsDir) === $smsDirSt['uid']);
smsShow('dir group is the stat one', filegroup($smsDir) === $smsDirSt['gid']);
smsShow('dir mtime is the stat one', filemtime($smsDir) === $smsDirSt['mtime']);

smsShow('is_file agrees', is_file($smsFile) === (($smsSt['mode'] & 0170000) === 0100000));
smsShow('is_dir agrees', is_dir($smsDir) === (($smsDirSt['mode'] & 0170000) === 0040000));

chmod($smsRo, 0666);
@unlink($smsFile);
@unlink($smsRo);
@rmdir($smsDir);
--EXPECT--
file is a stat                 => true
file S_IFREG                   => true
file readable bits             => true
file writable bits             => true
file size                      => 10
fileperms is that mode         => true
lstat agrees                   => true
fstat S_IFREG                  => true
fstat size                     => 10
read-only S_IFREG              => true
read-only no write bit         => true
read-only is_writable          => false
dir is a stat                  => true
dir S_IFDIR                    => true
dir execute bits               => true
dir fileperms                  => true
dir size is an int             => true
dir size is the stat one       => true
dir inode is the stat one      => true
dir owner is the stat one      => true
dir group is the stat one      => true
dir mtime is the stat one      => true
is_file agrees                 => true
is_dir agrees                  => true
