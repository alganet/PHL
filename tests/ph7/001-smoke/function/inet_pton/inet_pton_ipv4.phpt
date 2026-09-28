--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
inet_pton reads a dotted quad and refuses a padded one
--FILE--
<?php
foreach (['127.0.0.1', '0.0.0.0', '255.255.255.255', '1.2.3', '1.2.3.4.5', '256.0.0.1', '01.2.3.4', ' 1.2.3.4', '1.2.3.4 ', '1.2.3.4.', 'abcd', ''] as $ptonV4) {
    $ptonV4R = inet_pton($ptonV4);
    /* php hands the string to the C library, and BSD's inet_pton reads a
     * leading zero that glibc's refuses -- so that cell is not pinned */
    printf("%-18s => %s\n", var_export($ptonV4, true),
           $ptonV4 === '01.2.3.4' ? '-' : ($ptonV4R === false ? 'false' : bin2hex($ptonV4R)));
}
?>
--EXPECT--
'127.0.0.1'        => 7f000001
'0.0.0.0'          => 00000000
'255.255.255.255'  => ffffffff
'1.2.3'            => false
'1.2.3.4.5'        => false
'256.0.0.1'        => false
'01.2.3.4'         => -
' 1.2.3.4'         => false
'1.2.3.4 '         => false
'1.2.3.4.'         => false
'abcd'             => false
''                 => false
--CLEAN--
<?php
unset($ptonV4, $ptonV4R);
