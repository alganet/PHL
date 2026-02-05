--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strip_tags with UTF-8 tag names
--FILE--
<?php
// Tag name with Latin1 character U+00E1 ("á")
$s = "<á>hélló</á>";
// Allow the same tag
echo strip_tags($s, '<á>') . "\n";
// Without allow list, inner content should remain
echo strip_tags($s) . "\n";
?>
--EXPECT--
<á>hélló</á>
hélló
--CLEAN--
<?php
unset($s);
