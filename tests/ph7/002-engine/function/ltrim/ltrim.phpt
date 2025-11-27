--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: ltrim removes leading whitespace
--FILE--
<?php
$val = "  Hello  ";
echo "ltrim=BEGIN" . ltrim($val) . "END\n";
?>
--EXPECT--
ltrim=BEGINHello  END
