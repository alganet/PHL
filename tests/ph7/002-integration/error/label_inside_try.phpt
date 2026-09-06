--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Label inside try/catch construct error
--FILE--
<?php
// This should trigger a compile error: "Label 'label' inside loop or try/catch block is disallowed"
try {
    label:
    echo "test";
} catch (Exception $e) {
    echo "caught";
}
?>
--EXPECTF--
%Atest%A
--CLEAN--
<?php

