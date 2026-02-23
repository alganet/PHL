--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
addslashes converts float to string
--FILE--
<?php
echo addslashes(4.5);
?>
--EXPECT--
4.5
--CLEAN--
<?php

