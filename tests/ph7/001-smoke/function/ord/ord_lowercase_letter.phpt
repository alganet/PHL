--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ord('a') returns 97
--FILE--
<?php
echo ord('a') . "\n";
?>
--EXPECT--
97
--CLEAN--
<?php

