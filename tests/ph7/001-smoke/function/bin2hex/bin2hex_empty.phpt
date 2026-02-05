--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
bin2hex handles empty string
--FILE--
<?php
echo bin2hex('') . "\n";
?>
--EXPECT--
--CLEAN--
<?php

