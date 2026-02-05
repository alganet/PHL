--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
quotemeta escapes regex meta characters
--FILE--
<?php
echo quotemeta('a+b*c?') . "\n"; // a\+b\*c\?
?>
--EXPECT--
a\+b\*c\?
--CLEAN--
<?php

