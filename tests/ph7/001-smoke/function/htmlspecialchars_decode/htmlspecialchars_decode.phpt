--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
htmlspecialchars_decode basic decoding
--FILE--
<?php
echo htmlspecialchars_decode('&lt;a&gt;&amp;') . "\n"; // <a>&
?>
--EXPECT--
<a>&
--CLEAN--
<?php

