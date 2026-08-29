--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
vsprintf with format flags (left justify, blank sign)

--FILE--
<?php
// Left justify flag
echo vsprintf('%-10s', array('left')) . "\n";
// Blank sign flag
echo vsprintf('% 10s', array('blank')) . "\n";
?>
--EXPECT--
left      
     blank
--CLEAN--
<?php

