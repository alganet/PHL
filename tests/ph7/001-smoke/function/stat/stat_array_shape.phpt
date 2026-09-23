--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
stat()/lstat()/fstat() answer php's 26 entries: numeric 0..12, then named
--FILE--
<?php
/* php reports each of the thirteen fields TWICE -- once at a numeric index and
 * once under its name, numeric run first. PHL reported the named half only, so
 * the numeric access php's own manual documents ($s[7] is the size) warned
 * `Undefined array key 7` and answered NULL. */
$file = tempnam(sys_get_temp_dir(), 'phl_statshape_');
file_put_contents($file, '0123456789');

$names = ['dev', 'ino', 'mode', 'nlink', 'uid', 'gid', 'rdev', 'size',
          'atime', 'mtime', 'ctime', 'blksize', 'blocks'];

$fh = fopen($file, 'r');
foreach (['stat' => stat($file), 'lstat' => lstat($file), 'fstat' => fstat($fh)] as $what => $s) {
	echo str_pad($what, 6), count($s), ' ', implode(',', array_keys($s)), "\n";
	$paired = true;
	foreach ($names as $i => $n) {
		if ($s[$i] !== $s[$n]) {
			$paired = false;
			echo "  MISMATCH $i/$n\n";
		}
	}
	echo '  paired: ', var_export($paired, true), "  size: ", var_export($s[7], true), "\n";
}
fclose($fh);

/* The numeric half is what a positional read takes. */
[$dev, $ino, $mode] = stat($file);
var_dump($mode === stat($file)['mode']);
@unlink($file);
?>
--EXPECT--
stat  26 0,1,2,3,4,5,6,7,8,9,10,11,12,dev,ino,mode,nlink,uid,gid,rdev,size,atime,mtime,ctime,blksize,blocks
  paired: true  size: 10
lstat 26 0,1,2,3,4,5,6,7,8,9,10,11,12,dev,ino,mode,nlink,uid,gid,rdev,size,atime,mtime,ctime,blksize,blocks
  paired: true  size: 10
fstat 26 0,1,2,3,4,5,6,7,8,9,10,11,12,dev,ino,mode,nlink,uid,gid,rdev,size,atime,mtime,ctime,blksize,blocks
  paired: true  size: 10
bool(true)
