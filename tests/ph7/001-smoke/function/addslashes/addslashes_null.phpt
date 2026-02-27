--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
addslashes(NULL) should error
--FILE--
<?php
addslashes(NULL);
?>
--EXPECTF--
Error [8192]: addslashes(): Passing null to parameter #1 ($string) of type string is deprecated in %s on line %d
--CLEAN--
<?php

