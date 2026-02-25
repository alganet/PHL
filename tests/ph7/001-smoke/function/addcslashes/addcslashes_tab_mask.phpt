--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
addcslashes escapes tab when mask contains TAB
--FILE--
<?php
echo addcslashes("a\tb","\t") . "\n";
?>
--EXPECT--
a\tb
--CLEAN--
<?php

