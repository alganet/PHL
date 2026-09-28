--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
inet_ntop names the family by the byte count alone
--FILE--
<?php
foreach ([4, 16, 0, 2, 15, 17] as $ntopLen) {
    $ntopS = str_repeat("\x01", $ntopLen);
    var_dump(inet_ntop($ntopS));
}
?>
--EXPECT--
string(7) "1.1.1.1"
string(31) "101:101:101:101:101:101:101:101"
bool(false)
bool(false)
bool(false)
bool(false)
--CLEAN--
<?php
unset($ntopLen, $ntopS);
