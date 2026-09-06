--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Block mismatch error
--FILE--
<?php
function test() {
}
}
echo "test";
?>
--EXPECTF--
%AParse error:%AUnmatched '}'%A
--CLEAN--
<?php

