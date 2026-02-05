--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
addcslashes escapes characters specified in mask
--FILE--
<?php
echo addcslashes('abc','b') . "\n"; // a\bc
?>
--EXPECT--
a\bc
--CLEAN--
<?php

