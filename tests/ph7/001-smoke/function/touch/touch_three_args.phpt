--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
touch() with three arguments
--FILE--
<?php
$fname = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'ph7_touch3_' . uniqid() . '.txt';
file_put_contents($fname, 'X');
$time = time();
$ok = touch($fname, $time, $time);
echo "touched=" . ($ok ? 'true' : 'false') . PHP_EOL;
// The Windows skip this test used to carry ("POSIX-only behavior") was hiding a real
// Windows bug, not a platform difference: WinVfs_Touch stored the raw unix seconds in
// the FILETIME and passed the mtime as SetFileTime's CREATION argument, so neither
// stamp landed. Both stamps are checked here now, on every platform (27 Jul 2026).
echo "mtime=" . (filemtime($fname) === $time ? 'set' : 'WRONG') . PHP_EOL;
echo "atime=" . (fileatime($fname) === $time ? 'set' : 'WRONG') . PHP_EOL;
?>
--EXPECT--
touched=true
mtime=set
atime=set
--CLEAN--
<?php
@unlink($fname);
unset($fname, $time, $ok);
