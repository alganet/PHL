--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
chr(-1) wraps to chr(255) with deprecation

--FILE--
<?php
echo ord(chr(-1)) . "\n";
?>
--EXPECTF--
Error [%d]: chr(): Providing a value not in-between 0 and 255 is deprecated, this is because a byte value must be in the [0, 255] interval. The value used will be constrained using %% 256 in %s on line %d
255
--CLEAN--
<?php

