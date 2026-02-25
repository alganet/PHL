--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
addcslashes with empty mask leaves string untouched
--FILE--
<?php
echo addcslashes('hello','') . "\n";
?>
--EXPECT--
hello
--CLEAN--
<?php

