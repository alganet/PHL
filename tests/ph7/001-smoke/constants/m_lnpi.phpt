--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: M_LNPI constant value
--FILE--
<?php
printf("M_LNPI=%.8f\n", M_LNPI);
?>
--EXPECTF--
M_LNPI=1.1447298%d
--CLEAN--
<?php

