--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
isset checks variable existence
--FILE--
<?php
$x = 10;
if (isset($x)) { echo "x_set\n"; } else { echo "x_not_set\n"; }
if (isset($y)) { echo "y_set\n"; } else { echo "y_not_set\n"; }
$z = null;
if (isset($z)) { echo "z_set\n"; } else { echo "z_not_set\n"; }
?>
--EXPECTF--
%Ax_set%Ay_not_set%Az_not_set%A
--CLEAN--
<?php
unset($x, $z);
