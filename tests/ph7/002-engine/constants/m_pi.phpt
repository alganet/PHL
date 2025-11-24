--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: M_PI constant value
--FILE--
<?php
printf("M_PI=%.8f\n", M_PI);
?>
--EXPECTF--
M_PI=3.1415926%d
