--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Finally block executes after catch with exception
--FILE--
<?php
function test() {
    try {
        throw new Exception("err");
    } catch (Exception $e) {
        echo "catch\n";
    } finally {
        echo "finally\n";
    }
    return "done";
}
echo test() . "\n";
?>
--EXPECT--
catch
finally
done
--CLEAN--
<?php
