--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Declare construct emits notice
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
declare(ticks=1);
echo "done\n";
?>
--EXPECTF--
%s Notice:  the declare construct is a no-op in the current release of the PH7(2.1.4) engine %s
--CLEAN--
<?php

