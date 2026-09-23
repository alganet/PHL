--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Both escapers walk cmd.exe's argument BYTE by byte, because php's C locale there is single-byte
--SKIPIF--
<?php
if (PHP_OS_FAMILY !== 'Windows') {
    echo "skip php reports LC_CTYPE \"C\" on Windows and MSVCRT's C locale is SINGLE-byte, so nothing is ever dropped there; the UTF-8 half is escapeshell_multibyte.phpt";
}
?>
--FILE--
<?php
/* php's walk is php_mblen()'s, and on Windows that answers 1 for EVERY byte:
 * MSVCRT's C locale is single-byte, not ASCII-only the way glibc's is. So
 * nothing is dropped, a well-formed UTF-8 sequence still rides through (each of
 * its bytes is copied in turn, which reassembles it), and `\xFF` -- unreachable
 * under a UTF-8 walk, since it can never lead a sequence -- reaches
 * escapeshellcmd()'s escape table and comes back with a `^` in front of it.
 * Every row below is php 8.5's own answer on this platform. */
$esw_cases = [
    'ascii'         => 'plain',
    'latin'         => "caf\xc3\xa9",
    'cjk'           => "\xe6\x97\xa5\xe6\x9c\xac",
    'emoji'         => "\xf0\x9f\x92\xa9",
    'lone ff'       => "a\xffb",
    'truncated 3'   => "a\xe2\x82b",
    'surrogate'     => "a\xed\xa0\x80b",
    'overlong'      => "a\xc0\xafb",
    'bare cont'     => "a\x80b",
];
foreach ($esw_cases as $esw_name => $esw_s) {
    printf("%-13s cmd=%-20s arg=%s\n", $esw_name,
        bin2hex(escapeshellcmd($esw_s)),
        bin2hex(substr(escapeshellarg($esw_s), 1, -1)));
}
?>
--EXPECT--
ascii         cmd=706c61696e           arg=706c61696e
latin         cmd=636166c3a9           arg=636166c3a9
cjk           cmd=e697a5e69cac         arg=e697a5e69cac
emoji         cmd=f09f92a9             arg=f09f92a9
lone ff       cmd=615eff62             arg=61ff62
truncated 3   cmd=61e28262             arg=61e28262
surrogate     cmd=61eda08062           arg=61eda08062
overlong      cmd=61c0af62             arg=61c0af62
bare cont     cmd=618062               arg=618062
--CLEAN--
<?php
unset($esw_cases, $esw_name, $esw_s);
