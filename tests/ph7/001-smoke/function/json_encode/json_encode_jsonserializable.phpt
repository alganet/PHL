--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
json_encode consults JsonSerializable::jsonSerialize()
--SKIPIF--
<?php if (!function_exists('json_encode') || !interface_exists('JsonSerializable')) { die('skip'); } ?>
--FILE--
<?php
class JsonSerMoney implements JsonSerializable {
    private $amount;
    private $currency;
    public function __construct($a, $c) { $this->amount = $a; $this->currency = $c; }
    public function jsonSerialize(): mixed {
        return ['amount' => $this->amount, 'currency' => $this->currency];
    }
}
class JsonSerScalar implements JsonSerializable {
    public function jsonSerialize(): mixed { return 42; }
}
class JsonSerNested implements JsonSerializable {
    public function jsonSerialize(): mixed {
        return ['m' => new JsonSerMoney(5, 'USD'), 'list' => [1, 2, 3]];
    }
}
class JsonSerPlain {
    public $a = 1;
    public $b = 2;
}

echo json_encode(new JsonSerMoney(100, 'EUR')), "\n"; // assoc array -> JSON object
echo json_encode(new JsonSerScalar()), "\n";          // scalar passthrough
echo json_encode(new JsonSerNested()), "\n";          // nested object + list
echo json_encode(['wrap' => new JsonSerMoney(1, 'GBP')]), "\n"; // nested inside array
echo json_encode(new JsonSerPlain()), "\n";           // non-implementer: public props
echo (new JsonSerMoney(1, 'X') instanceof JsonSerializable) ? "yes\n" : "no\n";
echo interface_exists('JsonSerializable') ? "iface\n" : "no\n";
?>
--EXPECT--
{"amount":100,"currency":"EUR"}
42
{"m":{"amount":5,"currency":"USD"},"list":[1,2,3]}
{"wrap":{"amount":1,"currency":"GBP"}}
{"a":1,"b":2}
yes
iface
--CLEAN--
<?php
