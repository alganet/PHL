--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
addslashes escapes backslashes
--FILE--
<?php
// path with one backslash
echo addslashes('C:\\path');
?>
--EXPECT--
C:\\path
--CLEAN--
<?php

