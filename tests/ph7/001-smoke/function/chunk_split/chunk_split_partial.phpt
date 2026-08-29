--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
chunk_split with partial chunk at end

--FILE--
<?php
// Test case where chunk length exceeds remaining string length
// This should cover the adjustment logic in chunk_split
echo chunk_split('abcdefghijk', 5, '-') . "\n";
?>
--EXPECT--
abcde-fghij-k-
--CLEAN--
<?php

