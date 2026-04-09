--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Object type hint rejects non-object arguments with TypeError
--FILE--
<?php
function objHintTakesObject(object $o) {
    var_dump($o);
}

function objHintShowTypeError(TypeError $e) {
    $msg = $e->getMessage();
    $pos = strpos($msg, ", called in");
    if ($pos !== false) $msg = substr($msg, 0, $pos);
    echo $msg . "\n";
}

try { objHintTakesObject("hello"); } catch (TypeError $e) { objHintShowTypeError($e); }
try { objHintTakesObject(42); } catch (TypeError $e) { objHintShowTypeError($e); }
try { objHintTakesObject([1,2,3]); } catch (TypeError $e) { objHintShowTypeError($e); }
try { objHintTakesObject(null); } catch (TypeError $e) { objHintShowTypeError($e); }

try {
    objHintTakesObject("skip rest");
    echo "FAIL: should not reach here\n";
} catch (TypeError $e) {
    echo "caught\n";
}
echo "continues after try/catch\n";
?>
--EXPECT--
objHintTakesObject(): Argument #1 ($o) must be of type object, string given
objHintTakesObject(): Argument #1 ($o) must be of type object, int given
objHintTakesObject(): Argument #1 ($o) must be of type object, array given
objHintTakesObject(): Argument #1 ($o) must be of type object, null given
caught
continues after try/catch
--CLEAN--
<?php

