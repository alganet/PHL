--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
addcslashes escapes form feed when mask contains FORMFEED
--FILE--
<?php
echo addcslashes("a\fb","\f") . "\n";
?>
--EXPECT--
a\fb
--CLEAN--
<?php

