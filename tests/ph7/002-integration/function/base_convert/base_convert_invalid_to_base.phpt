--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base_convert invalid to_base throws ValueError
--FILE--
<?php
base_convert("10", 10, 37);
?>
--EXPECTF--
%s Fatal error:  Uncaught ValueError: base_convert(): Argument #3 ($to_base) must be between 2 and 36 (inclusive) in %s
--CLEAN--
<?php

