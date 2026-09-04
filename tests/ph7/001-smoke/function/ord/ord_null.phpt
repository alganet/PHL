--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ord(null) returns 0 with deprecation

--FILE--
<?php
echo ord(null) . "\n";
?>
--EXPECTF--
Error [%d]: ord(): Passing null to parameter #1 ($character) of type string is deprecated in %s on line %d
Error [%d]: ord(): Providing an empty string is deprecated in %s on line %d
0
--CLEAN--
<?php

