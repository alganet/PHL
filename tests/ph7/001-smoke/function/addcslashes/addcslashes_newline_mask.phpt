--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
addcslashes escapes newline when mask contains newline
--FILE--
<?php
echo addcslashes("a\nb","\n") . "\n";
?>
--EXPECT--
a\nb
--CLEAN--
<?php

