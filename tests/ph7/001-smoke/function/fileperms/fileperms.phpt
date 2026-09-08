--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
fileperms reports the stat mode bits; clearstatcache is a harmless no-op
--SKIPIF--
<?php
// POSIX permission bits (0644/0600) cannot be reproduced on Windows, where chmod
// only toggles the read-only attribute and stat reports 0666/0444 (php behaves
// the same there). This assertion is inherently POSIX-only.
if (DIRECTORY_SEPARATOR === '\\') { echo 'skip POSIX file mode bits are Windows-incompatible'; }
?>
--FILE--
<?php
$t = tempnam(sys_get_temp_dir(), "fp21");
chmod($t, 0644);
echo decoct(fileperms($t) & 0777), "\n";
chmod($t, 0600);
echo decoct(fileperms($t) & 0777), "\n";
echo is_int(fileperms($t)) ? "int\n" : "?\n";
echo (clearstatcache() === null) ? "clear:null\n" : "clear:?\n";
unlink($t);
?>
--EXPECT--
644
600
int
clear:null
--CLEAN--
<?php
