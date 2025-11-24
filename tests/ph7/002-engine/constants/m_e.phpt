--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: M_E constant value
--FILE--
<?php
printf("M_E=%.8f\n", M_E);
?>
--EXPECTF--
M_E=2.7182818%d
