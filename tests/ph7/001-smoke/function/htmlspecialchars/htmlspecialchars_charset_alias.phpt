--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
htmlspecialchars UTF-8 charset aliases are accepted silently
--FILE--
<?php
echo htmlspecialchars("<é>", ENT_QUOTES, "UTF-8"), "\n";
echo htmlspecialchars("<é>", ENT_QUOTES, "utf-8"), "\n";
echo htmlspecialchars("<é>", ENT_QUOTES, ""), "\n";
?>
--EXPECT--
&lt;é&gt;
&lt;é&gt;
&lt;é&gt;
--CLEAN--
<?php
