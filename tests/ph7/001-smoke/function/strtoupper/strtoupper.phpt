--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strtoupper converts string to uppercase
--FILE--
<?php
echo strtoupper('AbC') . "\n";
?>
--EXPECT--
ABC
--CLEAN--
<?php

