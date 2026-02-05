--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
stripslashes removes escape slashes
--FILE--
<?php
echo stripslashes('John\\\'s') . "\n"; // John's
?>
--EXPECT--
John's
--CLEAN--
<?php

