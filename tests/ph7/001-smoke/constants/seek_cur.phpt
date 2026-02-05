--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: SEEK_CUR constant
--FILE--
<?php
echo "SEEK_CUR=" . SEEK_CUR . "\n";
?>
--EXPECTF--
SEEK_CUR=%d
--CLEAN--
<?php

