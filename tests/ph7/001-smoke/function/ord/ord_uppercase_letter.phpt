--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ord('A') returns 65
--FILE--
<?php
echo ord('A') . "\n";
?>
--EXPECT--
65
--CLEAN--
<?php

