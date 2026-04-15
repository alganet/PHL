--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Throwable: mixed with multi-catch
--FILE--
<?php
function run($throwable) {
    try {
        throw $throwable;
    } catch (TypeError | ValueError $e) {
        echo "specific:", get_class($e), "\n";
    } catch (Throwable $t) {
        echo "throwable:", get_class($t), "\n";
    }
}
run(new TypeError("t"));
run(new ValueError("v"));
run(new Error("e"));
run(new Exception("x"));
?>
--EXPECT--
specific:TypeError
specific:ValueError
throwable:Error
throwable:Exception
--CLEAN--
<?php
