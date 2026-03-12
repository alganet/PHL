--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ord("\n") returns 10
--FILE--
<?php
echo ord("\n") . "\n";
?>
--EXPECT--
10
--CLEAN--
<?php

