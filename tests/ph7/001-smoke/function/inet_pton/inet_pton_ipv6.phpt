--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
inet_pton reads every IPv6 spelling php reads
--FILE--
<?php
foreach (['::1', '::', 'fe80::1', 'FE80::1', '2001:db8::ff00:42:8329', '::ffff:192.0.2.128', '1:2:3:4:5:6:7:8', '1::', '0:0:0:0:0:0:1.2.3.4',
          '1:2:3:4:5:6:7:8:9', '::1::2', 'fe80::1%eth0', '12345::', '::g', '1:2:3:4:5:6:7:'] as $ptonV6) {
    $ptonV6R = inet_pton($ptonV6);
    /* php hands the string to the C library, and BSD's inet_pton takes a
     * %scope suffix that glibc's refuses -- so that cell is not pinned */
    printf("%-24s => %s\n", $ptonV6,
           str_contains($ptonV6, '%') ? '-' : ($ptonV6R === false ? 'false' : bin2hex($ptonV6R)));
}
?>
--EXPECT--
::1                      => 00000000000000000000000000000001
::                       => 00000000000000000000000000000000
fe80::1                  => fe800000000000000000000000000001
FE80::1                  => fe800000000000000000000000000001
2001:db8::ff00:42:8329   => 20010db8000000000000ff0000428329
::ffff:192.0.2.128       => 00000000000000000000ffffc0000280
1:2:3:4:5:6:7:8          => 00010002000300040005000600070008
1::                      => 00010000000000000000000000000000
0:0:0:0:0:0:1.2.3.4      => 00000000000000000000000001020304
1:2:3:4:5:6:7:8:9        => false
::1::2                   => false
fe80::1%eth0             => -
12345::                  => false
::g                      => false
1:2:3:4:5:6:7:           => false
--CLEAN--
<?php
unset($ptonV6, $ptonV6R);
