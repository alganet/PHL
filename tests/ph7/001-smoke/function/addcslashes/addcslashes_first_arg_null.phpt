--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
addcslashes(NULL, 'a') should error
--FILE--
<?php
addcslashes(NULL, 'a');
?>
--EXPECTF--
Error [8192]: addcslashes(): Passing null to parameter #1 ($string) of type string is deprecated in %s on line %d
--CLEAN--
<?php

