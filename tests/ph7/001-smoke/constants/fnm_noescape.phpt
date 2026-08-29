--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
FNM_NOESCAPE constant value (php parity)
--SKIPIF--
skip: needs macos fix
--FILE--
<?php
echo "FNM_NOESCAPE value: " . FNM_NOESCAPE . "\n";
?>
--EXPECT--
FNM_NOESCAPE value: 2
--CLEAN--
<?php

