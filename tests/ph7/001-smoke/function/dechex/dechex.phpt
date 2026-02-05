--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
dechex returns hex string
--FILE--
<?php
echo dechex(255) . "\n"; // ff
?>
--EXPECT--
ff
--CLEAN--
<?php

