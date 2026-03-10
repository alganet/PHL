--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
count() with invalid mode throws ValueError
--FILE--
<?php
count(array(), 2);
?>
--EXPECTF--
%s Fatal error:  Uncaught ValueError: count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE in %s
--CLEAN--
<?php

