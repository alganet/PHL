--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base_convert zero from_base throws ValueError
--FILE--
<?php
base_convert("10", 0, 10);
?>
--EXPECTF--
%s Fatal error:  Uncaught ValueError: base_convert(): Argument #2 ($from_base) must be between 2 and 36 (inclusive) in %s
--CLEAN--
<?php

