--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
decoct returns octal string
--FILE--
<?php
echo decoct(8) . "\n"; // 10
?>
--EXPECT--
10
--CLEAN--
<?php

