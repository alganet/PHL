--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Both escapers walk their argument as UTF-8 where php does: a well-formed sequence rides through, an ill-formed byte is dropped
--SKIPIF--
<?php
/* php walks the argument with php_mblen(), i.e. in the process's LC_CTYPE
 * encoding, which it picks up from the environment at startup. On POSIX that is
 * a UTF-8 one in every ordinary environment, and PHL -- UTF-8-only
 * and with no setlocale at all -- answers the same. Two runs are not that, and
 * each has its own half of the corpus: Windows, where MSVCRT's C locale is
 * SINGLE-byte and nothing is ever dropped (escapeshell_multibyte_windows.phpt),
 * and php under a non-UTF-8 LC_CTYPE, where glibc's C locale drops every byte
 * >= 0x80 instead. */
if (PHP_OS_FAMILY === 'Windows') {
    echo "skip php's walk is SINGLE-byte on Windows (MSVCRT's C locale answers 1 for every byte); that half is escapeshell_multibyte_windows.phpt";
} elseif (function_exists('setlocale') && defined('LC_CTYPE')
    && stripos((string)setlocale(LC_CTYPE, 0), 'utf') === false) {
    echo "skip php's multibyte walk is LC_CTYPE-dependent and this run's LC_CTYPE is not a UTF-8 one";
}
?>
--FILE--
<?php
/* Both escapers copy a well-formed multibyte sequence through UNTOUCHED -- a
 * metacharacter's byte value inside one is not a metacharacter -- and DROP a
 * byte that cannot start a character. Reported as hex, and for escapeshellarg()
 * with the platform's own quoting stripped, so the expectation holds on cmd.exe
 * as much as on a POSIX shell. */
$esm_cases = [
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
foreach ($esm_cases as $esm_name => $esm_s) {
    printf("%-13s cmd=%-20s arg=%s\n", $esm_name,
        bin2hex(escapeshellcmd($esm_s)),
        bin2hex(substr(escapeshellarg($esm_s), 1, -1)));
}
?>
--EXPECT--
ascii         cmd=706c61696e           arg=706c61696e
latin         cmd=636166c3a9           arg=636166c3a9
cjk           cmd=e697a5e69cac         arg=e697a5e69cac
emoji         cmd=f09f92a9             arg=f09f92a9
lone ff       cmd=6162                 arg=6162
truncated 3   cmd=6162                 arg=6162
surrogate     cmd=6162                 arg=6162
overlong      cmd=6162                 arg=6162
bare cont     cmd=6162                 arg=6162
--CLEAN--
<?php
unset($esm_cases, $esm_name, $esm_s);
