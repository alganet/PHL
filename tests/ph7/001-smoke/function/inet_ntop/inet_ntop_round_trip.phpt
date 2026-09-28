--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
inet_ntop and inet_pton are each other inverse
--FILE--
<?php
$ntopRt = ['7f000001', '00000000000000000000000000000001', '20010db8000000000000ff0000428329', '00000000000000000000ffffc0000280'];
foreach ($ntopRt as $ntopRtHex) {
    var_dump(bin2hex(inet_pton(inet_ntop(hex2bin($ntopRtHex)))) === $ntopRtHex);
}
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
--CLEAN--
<?php
unset($ntopRt, $ntopRtHex);
