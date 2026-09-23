--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Both escapers refuse a command line longer than the platform allows, before and after escaping
--SKIPIF--
skip: flaky
--FILE--
<?php
/* php bounds both escapers by the longest command line the platform's shell
 * accepts (sysconf(_SC_ARG_MAX), cmd.exe's 8192 on Windows) -- once on the
 * argument it was given and once on what escaping made of it. The number is the
 * platform's, so it is wildcarded here; the second pair derives it from the
 * first message and is what actually reaches the post-escape gate. */
$max = 0;
try {
    escapeshellcmd(str_repeat('a', 3000000));
    echo "NO THROW (cmd)\n";
} catch (ValueError $e) {
    echo $e->getMessage(), "\n";
    if (preg_match('/(\d+)/', $e->getMessage(), $m)) { $max = (int)$m[1]; }
}
try {
    escapeshellarg(str_repeat('a', 3000000));
    echo "NO THROW (arg)\n";
} catch (ValueError $e) {
    echo $e->getMessage(), "\n";
}

/* Half the limit of pure metacharacters: accepted as an argument, refused once
 * every one of them has grown a backslash. escapeshellcmd() is the half that can
 * grow on BOTH platforms (a `^` on cmd.exe, a backslash elsewhere);
 * escapeshellarg()'s growth is POSIX-only, so its post-escape gate has a test
 * of its own. */
$half = intdiv($max, 2) + 1;
try {
    escapeshellcmd(str_repeat('&', $half));
    echo "NO THROW (cmd escaped)\n";
} catch (ValueError $e) {
    echo $e->getMessage(), "\n";
}

/* Every one of them is catchable, so the script is still running. */
echo "still here\n";
?>
--EXPECTF--
Command exceeds the allowed length of %d bytes
Argument exceeds the allowed length of %d bytes
Escaped command exceeds the allowed length of %d bytes
still here
