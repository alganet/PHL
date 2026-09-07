--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
abs(NULL) returns 0
--FILE--
<?php
echo abs(NULL) . "\n";
?>
--EXPECTF--
%Aabs(): Passing null to parameter #1 ($num) of type int|float is deprecated%A0%A
--CLEAN--
<?php

