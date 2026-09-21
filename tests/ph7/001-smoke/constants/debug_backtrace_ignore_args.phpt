--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: DEBUG_BACKTRACE_IGNORE_ARGS constant
--FILE--
<?php
echo "DEBUG_BACKTRACE_IGNORE_ARGS=" . DEBUG_BACKTRACE_IGNORE_ARGS . "\n";
?>
--EXPECT--
DEBUG_BACKTRACE_IGNORE_ARGS=2
--CLEAN--
<?php

