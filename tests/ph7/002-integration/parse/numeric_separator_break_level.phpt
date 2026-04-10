--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Malformed numeric separator in 'break' level is a parse error
--FILE--
<?php
for ($i = 0; $i < 3; $i++) {
    for ($j = 0; $j < 3; $j++) {
        break 1_;
    }
}
?>
--EXPECTF--
%s Parse error:  syntax error, unexpected identifier "_"
--CLEAN--
<?php
