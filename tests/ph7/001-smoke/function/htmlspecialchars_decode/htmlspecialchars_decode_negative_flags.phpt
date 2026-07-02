--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
htmlspecialchars_decode with negative flags
--FILE--
<?php
$result = htmlspecialchars_decode('&quot;test&quot;', -1);
echo $result . "\n";
?>
--EXPECT--
"test"
--CLEAN--
<?php
unset($result);
