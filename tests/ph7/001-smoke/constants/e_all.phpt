--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: E_ALL constant value (php 8.4+: 30719 — it was 32767 while E_STRICT still existed)
--FILE--
<?php
echo "E_ALL=" . E_ALL . "\n";
?>
--EXPECT--
E_ALL=30719
--CLEAN--
<?php

