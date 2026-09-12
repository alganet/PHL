--TEST--
unserialize() restores declared/typed properties (public, private, protected, readonly, nested) and leaves never-set typed properties uninitialized
--FILE--
<?php
final class UtpInner { public int $x; public function __construct(int $x){ $this->x = $x; } }
final class UtpNode {
    public int $id;
    public string $name;
    private array $tags;
    public readonly ?UtpInner $inner;
    protected float $ratio;
    public function __construct(int $id, string $name, array $tags, ?UtpInner $inner, float $ratio) {
        $this->id = $id; $this->name = $name; $this->tags = $tags; $this->inner = $inner; $this->ratio = $ratio;
    }
    public function dump(): string {
        return $this->id . '|' . $this->name . '|' . implode(',', $this->tags) . '|' .
               ($this->inner ? $this->inner->x : 'null') . '|' . $this->ratio;
    }
}
$utpN = new UtpNode(7, "node", ["a", "b"], new UtpInner(99), 3.5);
$utpR = unserialize(serialize($utpN));
echo $utpR->dump(), "\n";
// A typed property that was never written round-trips as still-uninitialized
class UtpU { public int $set; public int $unset; }
$utpU = new UtpU();
$utpU->set = 1;
$utpR2 = unserialize(serialize($utpU));
echo $utpR2->set, "\n";
echo isset($utpR2->unset) ? "set" : "unset", "\n";
--EXPECT--
7|node|a,b|99|3.5
1
unset
