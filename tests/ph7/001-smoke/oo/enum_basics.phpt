--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Enums (PHP 8.1): cases, name/value, identity, cases()/from()/tryFrom, interfaces, traits, json, serialize
--FILE--
<?php
interface EnbHasLabel { const PREFIX = "card"; public function label(): string; }
trait EnbNamer { public function tag(): string { return "tag:" . $this->name; } }
enum EnbSuit: string implements EnbHasLabel {
    use EnbNamer;
    const Wild = self::Spades;
    case Hearts = "H";
    case Spades = "S";
    public function label(): string { return self::PREFIX . ":" . $this->value; }
    public static function first(): EnbSuit { return self::cases()[0]; }
}
enum EnbPure { case Alpha; case Beta; }
enum EnbInt: int { const OFF = 10; case Lo = self::OFF + 1; case Hi = 20; }

echo EnbSuit::Hearts->name, " ", EnbSuit::Hearts->value, "\n";
echo EnbSuit::Hearts === EnbSuit::Hearts ? "same" : "diff", "\n";
echo EnbSuit::Hearts === EnbSuit::Spades ? "same" : "diff", "\n";
echo EnbSuit::Hearts instanceof UnitEnum ? "unit" : "-", " ",
     EnbSuit::Hearts instanceof BackedEnum ? "backed" : "-", " ",
     EnbSuit::Hearts instanceof EnbSuit ? "self" : "-", " ",
     EnbSuit::Hearts instanceof EnbHasLabel ? "iface" : "-", "\n";
echo EnbPure::Alpha instanceof BackedEnum ? "backed" : "notbacked", "\n";
echo count(EnbSuit::cases()), " ", implode(",", array_map(fn($c) => $c->name, EnbSuit::cases())), "\n";
echo EnbSuit::from("S")->name, " ", EnbSuit::tryFrom("X") === null ? "null" : "?", "\n";
echo EnbInt::from("11")->name, "\n"; // weak-mode coercion at from()
echo match(EnbSuit::Spades) { EnbSuit::Hearts => "h", EnbSuit::Spades => "s" }, "\n";
echo EnbSuit::Hearts->label(), " ", EnbSuit::Spades->tag(), "\n";
echo EnbSuit::Wild === EnbSuit::Spades ? "wild=spades" : "?", "\n";
echo EnbSuit::first()->name, "\n";
echo EnbInt::Lo->value, " ", EnbInt::Hi->value, "\n";
echo get_class(EnbSuit::Hearts), " ", gettype(EnbSuit::Hearts), "\n";
echo enum_exists("EnbSuit") ? "y" : "n", enum_exists("EnbHasLabel") ? "y" : "n", enum_exists("Missing8") ? "y" : "n", "\n";
echo constant("EnbSuit::Hearts")->value, "\n";
function enb_pick(EnbSuit $s = EnbSuit::Spades): string { return $s->name; }
echo enb_pick(), " ", enb_pick(EnbSuit::Hearts), "\n";
class EnbHolder { const DEF = EnbPure::Beta; }
echo EnbHolder::DEF->name, "\n";
$fcc = EnbSuit::from(...);
echo $fcc("H")->name, "\n";
echo json_encode(EnbSuit::Hearts), " ", json_encode([EnbInt::Lo, EnbInt::Hi]), "\n";
echo json_encode(EnbPure::Alpha) === false ? "jsonfail" : "?", "\n";
echo serialize(EnbSuit::Hearts), "\n";
echo unserialize(serialize(EnbSuit::Hearts)) === EnbSuit::Hearts ? "roundtrip" : "?", "\n";
echo var_export(EnbPure::Alpha, true), "\n";
$o = EnbSuit::Hearts;
echo isset($o->name) ? "n1" : "-", isset($o->value) ? "v1" : "-", isset($o->nope) ? "x1" : "-", "\n";
$vars = get_object_vars($o);
echo $vars["name"], $vars["value"], "\n";
echo property_exists($o, "name") ? "pe" : "-", "\n";
echo in_array(EnbPure::Beta, EnbPure::cases(), true) ? "inarr" : "-", "\n";
?>
--EXPECT--
Hearts H
same
diff
unit backed self iface
notbacked
2 Hearts,Spades
Spades null
Lo
s
card:H tag:Spades
wild=spades
Hearts
11 20
EnbSuit object
ynn
H
Spades Hearts
Beta
Hearts
"H" [11,20]
jsonfail
E:14:"EnbSuit:Hearts";
roundtrip
\EnbPure::Alpha
n1v1-
HeartsH
pe
inarr
--CLEAN--
<?php
unset($o, $vars, $fcc);
