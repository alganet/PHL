--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
htmlspecialchars with quotes
--FILE--
<?php
echo htmlspecialchars("John's \"test\" & more", ENT_QUOTES) . "\n";
?>
--EXPECT--
John&#039;s &quot;test&quot; &amp; more