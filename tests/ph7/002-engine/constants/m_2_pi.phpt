--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
M_2_PI constant value
--FILE--
<?php
if (M_2_PI > 0.636 && M_2_PI < 0.637) {
    echo "M_2_PI is correct\n";
} else {
    echo "M_2_PI is incorrect\n";
}
?>
--EXPECT--
M_2_PI is correct