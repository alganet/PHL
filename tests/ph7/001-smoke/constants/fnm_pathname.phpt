--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: FNM_PATHNAME constant
--SKIPIF--
needs macos fix
--FILE--
<?php
echo "FNM_PATHNAME=" . FNM_PATHNAME . "\n";
?>
--EXPECT--
FNM_PATHNAME=1
--CLEAN--
<?php

