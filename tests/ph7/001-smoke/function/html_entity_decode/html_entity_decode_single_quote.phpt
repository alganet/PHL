--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
html_entity_decode single quotes: default flags include ENT_QUOTES since 8.1
--FILE--
<?php
echo html_entity_decode("&#39;Hi&#39;") . "\n";
echo html_entity_decode("&#39;Hi&#39;", ENT_QUOTES) . "\n";
echo html_entity_decode("&#39;Hi&#39;", ENT_COMPAT) . "\n";
?>
--EXPECT--
'Hi'
'Hi'
&#39;Hi&#39;
--CLEAN--
<?php
