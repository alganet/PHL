--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
atan with no arguments throws ArgumentCountError
--FILE--
<?php
try {
    atan();
} catch (ArgumentCountError $e) {
    echo $e->getMessage();
}
?>
--EXPECTF--
atan() expects exactly 1 argument, %d given
--CLEAN--
<?php

