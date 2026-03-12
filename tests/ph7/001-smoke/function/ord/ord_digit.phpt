--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ord('0') returns 48
--FILE--
<?php
echo ord('0') . "\n";
?>
--EXPECT--
48
--CLEAN--
<?php

