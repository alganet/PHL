--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
html_entity_decode basic decoding of entities
--FILE--
<?php
// Basic entity decoding tests
echo html_entity_decode("&lt;&gt;&amp;") . "\n";        // <>&
// Double quotes are decoded by default (ENT_COMPAT)
echo html_entity_decode("&quot;Hello&quot;") . "\n";            // "Hello"
// A string containing both encoded and raw characters
echo html_entity_decode("A &lt; B &amp; C") . "\n";        // A < B & C
?>
--EXPECT--
<>&
"Hello"
A < B & C
--CLEAN--
<?php

