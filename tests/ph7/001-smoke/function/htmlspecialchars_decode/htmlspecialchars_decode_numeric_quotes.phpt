--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
htmlspecialchars_decode: named specials plus special-valued numeric refs
--FILE--
<?php
echo htmlspecialchars_decode("&#39;&#039;&#x27;&#X27;"), "\n";   // all apostrophe forms
echo htmlspecialchars_decode("&#34;&#x22;"), "\n";               // double-quote numerics too
echo htmlspecialchars_decode("&#60;&#62;&#38;&#65;"), "\n";       // special-valued numerics decode, others verbatim
echo htmlspecialchars_decode("&nbsp;&eacute;"), "\n";            // named beyond specials: verbatim
echo htmlspecialchars_decode("&quot;&#34;", ENT_NOQUOTES), "\n"; // gated
?>
--EXPECT--
''''
""
<>&&#65;
&nbsp;&eacute;
&quot;&#34;
--CLEAN--
<?php
