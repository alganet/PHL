--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
addcslashes(NULL mask) should error
--FILE--
<?php
addcslashes('abc', null);
?>
--EXPECTF--
Error [8192]: addcslashes(): Passing null to parameter #2 ($characters) of type string is deprecated in %s on line %d
--CLEAN--
<?php

