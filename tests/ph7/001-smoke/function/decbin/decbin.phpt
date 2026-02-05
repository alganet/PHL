--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
decbin returns binary string
--FILE--
<?php
echo decbin(2) . "\n"; // 10
?>
--EXPECT--
10
--CLEAN--
<?php

