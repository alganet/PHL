--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Unmatched closing bracket should produce syntax error
--FILE--
<?php
// Test unmatched closing bracket
echo "test";
]
?>
--EXPECTF--
%AParse error:%AUnmatched ']'%A
--CLEAN--
<?php

