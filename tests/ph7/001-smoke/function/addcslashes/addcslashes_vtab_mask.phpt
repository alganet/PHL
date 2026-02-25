--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
addcslashes escapes vertical tab when mask contains VTAB
--FILE--
<?php
echo addcslashes("a\vb","\v") . "\n";
?>
--EXPECT--
a\vb
--CLEAN--
<?php

