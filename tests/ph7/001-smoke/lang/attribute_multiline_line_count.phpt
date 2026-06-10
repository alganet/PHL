--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Multi-line attribute groups keep the line counter accurate
--FILE--
<?php
#[Config(
    "multi]line",
    [1, 2],
)]
function attr_multiline_f(){ return __LINE__; }
echo attr_multiline_f(), "\n";
echo __LINE__, "\n";
?>
--EXPECT--
6
8
--CLEAN--
<?php
?>
