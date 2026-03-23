--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
atan with non-numeric string throws TypeError
--FILE--
<?php
try {
    atan("abc");
} catch (TypeError $e) {
    echo $e->getMessage();
}
?>
--EXPECTF--
atan(): Argument #1 ($num) must be of type float, string given
--CLEAN--
<?php

