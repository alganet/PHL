--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
htmlspecialchars basic encoding
--FILE--
<?php
echo htmlspecialchars('<a>&>') . "\n"; // &lt;a&gt;&amp;&gt;
?>
--EXPECT--
&lt;a&gt;&amp;&gt;
--CLEAN--
<?php

