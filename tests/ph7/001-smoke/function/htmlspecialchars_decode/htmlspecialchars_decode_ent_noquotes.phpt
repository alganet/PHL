--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
htmlspecialchars_decode with ENT_NOQUOTES flag
--FILE--
<?php
// Test htmlspecialchars_decode with ENT_NOQUOTES flag
// ENT_NOQUOTES prevents decoding of &quot;
$result = htmlspecialchars_decode('&quot;hello&quot;', ENT_NOQUOTES);
echo $result . "\n";
?>
--EXPECT--
&quot;hello&quot;
--CLEAN--
<?php
unset($result);
