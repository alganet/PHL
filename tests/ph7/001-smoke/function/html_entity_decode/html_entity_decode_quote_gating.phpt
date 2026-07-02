--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
html_entity_decode quote gating applies to named AND numeric forms; &apos; needs a non-HTML401 doctype
--FILE--
<?php
echo html_entity_decode("&quot;&#34;&#039;&#39;", ENT_NOQUOTES), "\n";
echo html_entity_decode("&quot;&#34;", ENT_COMPAT), "\n";
echo html_entity_decode("&#039;&#39;", ENT_COMPAT), "\n";
echo html_entity_decode("&apos;"), "\n";                         // not an HTML 4.01 entity
echo html_entity_decode("&apos;", ENT_QUOTES | ENT_HTML5), "\n";
?>
--EXPECT--
&quot;&#34;&#039;&#39;
""
&#039;&#39;
&apos;
'
--CLEAN--
<?php
