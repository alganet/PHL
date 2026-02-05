--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
crc32 computes crc32 checksum for a string
--FILE--
<?php
// Compute the complemented CRC32 of 'abc' to match PH7/PHP output
echo crc32('abc') ^ 0xFFFFFFFF , "\n";
?>
--EXPECT--
3403398717
--CLEAN--
<?php

