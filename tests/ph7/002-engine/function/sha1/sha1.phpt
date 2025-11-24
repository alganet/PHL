--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
sha1 computes sha1 hash for a string
--FILE--
<?php
echo sha1('abc') . "\n";
?>
--EXPECT--
a9993e364706816aba3e25717850c26c9cd0d89d
