--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
htmlspecialchars with negative flags
--FILE--
<?php
$result = htmlspecialchars("<>&\"", -1);
echo $result . "\n";
?>
--EXPECT--
&lt;&gt;&amp;&quot;
--CLEAN--
<?php
unset($result);
