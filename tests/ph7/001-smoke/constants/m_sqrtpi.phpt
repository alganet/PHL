--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: M_SQRTPI constant value
--FILE--
<?php
printf("M_SQRTPI=%.8f\n", M_SQRTPI);
?>
--EXPECTF--
M_SQRTPI=1.7724538%d
--CLEAN--
<?php

