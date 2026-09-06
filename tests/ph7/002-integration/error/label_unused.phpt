--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: unused label warning
--FILE--
<?php
unused_label:
echo "test";
?>
--EXPECTF--
%Atest%A
--CLEAN--
<?php

