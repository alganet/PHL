--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
htmlentities encodes basic html
--FILE--
<?php
echo htmlentities('<a>') . "\n"; // &lt;a&gt;
?>
--EXPECT--
&lt;a&gt;
--CLEAN--
<?php

