--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
chunk_split splits string into chunks
--FILE--
<?php
echo chunk_split('abcdef', 2, ':') . "\n"; // ab:cd:ef:
?>
--EXPECT--
ab:cd:ef:
--CLEAN--
<?php

