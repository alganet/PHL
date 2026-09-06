--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
unexpected closing curly brace
--FILE--
<?php
if (true) {
    echo 1;
}
echo 2;
}
?>
--EXPECTF--
%AParse error:%AUnmatched '}'%A
--CLEAN--
<?php

