--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strtolower converts string to lowercase
--FILE--
<?php
echo strtolower('AbC') . "\n";
?>
--EXPECT--
abc
--CLEAN--
<?php

