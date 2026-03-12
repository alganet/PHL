--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ord(' ') returns 32
--FILE--
<?php
echo ord(' ') . "\n";
?>
--EXPECT--
32
--CLEAN--
<?php

