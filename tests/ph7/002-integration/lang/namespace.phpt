--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Namespace declaration
--FILE--
<?php
namespace my\ns;
echo "done\n";
?>
--EXPECT--
done
--CLEAN--
<?php

