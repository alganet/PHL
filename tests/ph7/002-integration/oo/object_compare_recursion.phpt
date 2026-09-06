--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Object comparison with deep recursion
--FILE--
<?php
class Test {
    public $self;
}

function create_deep($depth) {
    $obj = new Test();
    if ($depth > 0) {
        $obj->self = create_deep($depth - 1);
    }
    return $obj;
}

$a = create_deep(5);
$b = create_deep(5);

if ($a == $b) {
    echo "equal\n";
} else {
    echo "not equal\n";
}
?>
--EXPECTF--
%Aequal%A
--CLEAN--
<?php
unset($obj, $a, $b);
