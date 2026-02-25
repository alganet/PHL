--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_chunk called with negative size should throw ValueError
--FILE--
<?php
array_chunk(array(1,2,3), -1);
?>
--EXPECTF--
%s Fatal error:  Uncaught ValueError: array_chunk(): Argument #2 ($length) must be greater than 0 in %s
--CLEAN--
<?php

