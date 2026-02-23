--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
addslashes escapes double quotes
--FILE--
<?php
// string contains double quotes
echo addslashes("She said \"hi\"");
?>
--EXPECT--
She said \"hi\"
--CLEAN--
<?php

