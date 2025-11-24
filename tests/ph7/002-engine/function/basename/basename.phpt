--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: basename should return correct base name and support suffix removal
--FILE--
<?php
echo basename('dir/sub/file.txt') . "\n";
// Remove suffix
echo basename('dir/sub/file.txt', '.txt') . "\n";
?>
--EXPECT--
file.txt
file
