--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
String escaping in double quotes
--FILE--
<?php
echo "\$dollar\n";
echo "\\backslash\n";
echo "\"quote\n";
?>
--EXPECT--
$dollar
\backslash
"quote
--CLEAN--
<?php

