--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
inet_ntop writes the longest zero run as a double colon
--FILE--
<?php
foreach ([
    '00000000000000000000000000000000',
    '00010000000000000000000000000000',
    '00000000000000000000000000000001',
    '20010db8000000000000ff0000428329',
    '00000000000000000000ffffc0000280',
    '00000000000000000000000001020304',
    '00010000000000010000000000010001',
    'ffffffffffffffffffffffffffffffff',
] as $ntopHex) {
    printf("%-34s => %s\n", $ntopHex, inet_ntop(hex2bin($ntopHex)));
}
?>
--EXPECT--
00000000000000000000000000000000   => ::
00010000000000000000000000000000   => 1::
00000000000000000000000000000001   => ::1
20010db8000000000000ff0000428329   => 2001:db8::ff00:42:8329
00000000000000000000ffffc0000280   => ::ffff:192.0.2.128
00000000000000000000000001020304   => ::1.2.3.4
00010000000000010000000000010001   => 1::1:0:0:1:1
ffffffffffffffffffffffffffffffff   => ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff
--CLEAN--
<?php
unset($ntopHex);
