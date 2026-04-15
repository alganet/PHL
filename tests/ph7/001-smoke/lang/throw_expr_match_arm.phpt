--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
throw expression: inside a match arm propagates out of the match
--FILE--
<?php
function check($code) {
    try {
        echo match ($code) {
            200 => "ok: $code\n",
            default => throw new Exception("unexpected: $code"),
        };
    } catch (Exception $e) {
        echo "caught: ", $e->getMessage(), "\n";
    }
}
check(200);
check(500);
echo "done\n";
?>
--EXPECT--
ok: 200
caught: unexpected: 500
done
--CLEAN--
<?php
