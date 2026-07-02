--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
html_entity_decode with negative flags
--FILE--
<?php
$result = html_entity_decode("<>", -1);
echo $result . "\n";
?>
--EXPECT--
<>
--CLEAN--
<?php
unset($result);
