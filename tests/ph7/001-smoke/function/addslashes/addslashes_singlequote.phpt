--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
addslashes escapes single quote
--FILE--
<?php
echo addslashes("John's") . "\n"; // John\'s
?>
--EXPECT--
John\'s
--CLEAN--
<?php

