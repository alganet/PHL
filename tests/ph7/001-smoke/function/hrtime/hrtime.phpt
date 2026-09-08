--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
hrtime returns [sec, nsec] or total nanoseconds and is monotonic
--FILE--
<?php
$hrArr = hrtime();
echo (is_array($hrArr) && count($hrArr) === 2 && is_int($hrArr[0]) && is_int($hrArr[1])) ? "array-ok" : "bad", "\n";
echo is_int(hrtime(true)) ? "int-ok" : "bad", "\n";
$hrA = hrtime(true);
$hrB = hrtime(true);
echo ($hrB >= $hrA) ? "monotonic" : "bad", "\n";
?>
--EXPECT--
array-ok
int-ok
monotonic
--CLEAN--
<?php
