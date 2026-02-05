--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
atan2 with zero arguments
--FILE--
<?php
$result = atan2(0, 0);
echo "atan2(0,0) = $result\n";
?>
--EXPECT--
atan2(0,0) = 0
--CLEAN--
<?php
unset($result);
