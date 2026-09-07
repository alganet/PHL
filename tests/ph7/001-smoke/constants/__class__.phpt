--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: __CLASS__ constant returns null when used outside class context
--FILE--
<?php
if (__CLASS__ === null) {
    echo "NULL\n";
} else {
    echo "not null\n";
}
?>
--EXPECTF--
%Anot null%A
--CLEAN--
<?php

