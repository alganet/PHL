--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
addslashes converts integer to string
--FILE--
<?php
echo addslashes(123);
?>
--EXPECT--
123
--CLEAN--
<?php

