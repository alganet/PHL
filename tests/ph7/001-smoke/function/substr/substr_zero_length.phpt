--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
substr with zero length
--FILE--
<?php
echo substr("hello", 1, 0) . "\n";
?>
--EXPECT--
--CLEAN--
<?php

