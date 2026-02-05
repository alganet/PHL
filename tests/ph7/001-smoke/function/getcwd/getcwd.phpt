--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: getcwd returns string
--FILE--
<?php
echo "getcwd_type=" . gettype(getcwd()) . "\n";
?>
--EXPECT--
getcwd_type=string
--CLEAN--
<?php

