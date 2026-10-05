--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Property redeclarations php accepts
--DESCRIPTION--
Every shape here is the same type spelled differently, or a variance a hooked
parent allows, or a parent php does not check at all (a private one). The
native classes' own declarations count too: Exception's $file and $line are
typed and $message untyped, and php_user_filter's $params is mixed.
--FILE--
<?php
class PrdOkA {}
class PrdOkParent {
    public int|string $u;
    public iterable $it;
    public ?PrdOkParent $n;
    public self $s;
    public static ?int $st;
    private int $priv;
    public PrdOkA $q;
}
class PrdOkKid extends PrdOkParent {
    public string|int $u;
    public array|Traversable $it;
    public PrdOkParent|null $n;
    public PrdOkParent $s;
    public static int|null $st;
    public static string $priv;
    public \PrdOkA $q;
}
abstract class PrdOkGet { abstract public ?int $g { get; } }
class PrdOkGetKid extends PrdOkGet { public int $g = 1; }
abstract class PrdOkSet { abstract public int $w { set; } }
class PrdOkSetKid extends PrdOkSet { public ?int $w; }
class PrdOkPromoted extends PrdOkParent {
    public function __construct(public int $i = 0, public string|int $u = 1) {}
}
class PrdOkError extends Exception {
    public $message = "m";
    protected $code = 3;
    protected string $file = "";
    protected int $line = 0;
}
class PrdOkFilter extends php_user_filter {
    public mixed $params;
    public string $filtername = "";
    public $stream;
}
echo (new ReflectionProperty("php_user_filter", "params"))->getType(), "\n";
echo "ok\n";
?>
--EXPECTF--
mixed
ok
