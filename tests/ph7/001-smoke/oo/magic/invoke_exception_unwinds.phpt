--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An exception thrown inside __invoke unwinds the call
--DESCRIPTION--
Regression: an exception raised inside __invoke ran the catch block but then
continued past the failed call with a bogus result. The call must unwind: code
after $obj() must not run when __invoke throws.
--FILE--
<?php
class InvokeUnwind_Boom {
    public function __invoke() {
        throw new Exception("boom");
        echo "after-throw\n"; // unreachable
    }
}
$b = new InvokeUnwind_Boom();
try {
    $r = $b();
    echo "after-call\n"; // must NOT run
} catch (Exception $e) {
    echo "caught: ", $e->getMessage(), "\n";
}
echo "after-try\n";
?>
--EXPECT--
caught: boom
after-try
--CLEAN--
<?php
unset($b);
