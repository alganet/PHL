--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
nl2br() with is_xhtml=false

--FILE--
<?php
echo nl2br("a\nb", false);
?>
--EXPECT--
a<br>
b
--CLEAN--
<?php

