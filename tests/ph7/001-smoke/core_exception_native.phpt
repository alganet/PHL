--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The Exception/Error family is declared from C, with php's __toString and slots
--DESCRIPTION--
php's `__toString` is a FORMAT, not a concatenation: `Class: message in file:line`
followed by `Stack trace:` and the trace, and the `$previous` chain is part of it
— php builds the string innermost-first and joins the callers after `\n\nNext `,
so the root cause is printed first. The embedded PHP answered
`file line code message` instead, which no php ever produced and which is what
`echo $e` shows. The DECLARATION carried the rest: php's seven slots (including
the private `string` cache PHL simply lacked, so every exception was one property
short on var_dump/print_r/(array)/serialize), their types, `Error::$line` with no
default at all, the FINAL getters, the private `__clone` behind php's uncloneable
refusal, and `__wakeup`.
--FILE--
<?php
function excShow($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '', $out), "\n";
}
/* The trace is normalized rather than printed raw: a frame NUMBER depends on how
   the runner reached this file (php records an `include` frame for the harness
   and PHL does not — an engine-wide backtrace gap, PLAN §7.1), so only the
   frames belonging to this file and the closing {main} are compared. */
function excNorm($s) {
    $out = [];
    foreach (explode("\n", str_replace(__FILE__, 'F', $s)) as $line) {
        if (preg_match('/^#\d+ (.*)$/', $line, $m)) {
            if (str_starts_with($m[1], 'F(')) { $out[] = '# ' . $m[1]; }
            elseif ($m[1] === '{main}') { $out[] = '# {main}'; }
            continue;
        }
        $out[] = $line;
    }
    return implode("\n", $out);
}
function excThrower() { throw new RuntimeException('root cause', 7); }

echo "-- __toString is php's format, and the previous chain is part of it\n";
try { excThrower(); } catch (RuntimeException $inner) {
    $outer = new LogicException('wrapper', 0, $inner);
    echo excNorm((string)$outer), "\n";
}
echo "-- an empty message drops the colon\n";
echo excNorm((string)new Exception()), "\n";

echo "-- getTraceAsString\n";
try { excThrower(); } catch (Throwable $e) { echo excNorm($e->getTraceAsString()), "\n"; }

echo "-- the seven slots php declares, in php's order\n";
foreach ((new ReflectionClass('Exception'))->getProperties() as $p) {
    echo '  ', implode(' ', array_filter([
        implode(' ', Reflection::getModifierNames($p->getModifiers())),
        (string)$p->getType(), '$' . $p->getName(),
        $p->hasDefaultValue() ? '= ' . var_export($p->getDefaultValue(), true) : '(no default)',
    ])), "\n";
}
excShow('Error::$line has no default', fn() => (new ReflectionProperty('Error', 'line'))->hasDefaultValue());
excShow('ErrorException lists severity last', fn() => implode(',', array_map(
    fn($p) => $p->getName(), (new ReflectionClass('ErrorException'))->getProperties())));

echo "-- the getters are final, __clone is private, and clone is refused\n";
excShow('getMessage final', fn() => (new ReflectionMethod('Exception', 'getMessage'))->isFinal());
excShow('__toString not final', fn() => (new ReflectionMethod('Exception', '__toString'))->isFinal());
excShow('__clone private', fn() => (new ReflectionMethod('Exception', '__clone'))->isPrivate());
excShow('clone refused', fn() => clone new Exception('x'));
excShow('method count', fn() => count((new ReflectionClass('Error'))->getMethods()));

echo "-- the constructor writes only what it was given\n";
class ExcSubclass extends Exception { protected $message = 'declared default'; }
excShow('subclass default kept', fn() => (new ExcSubclass())->getMessage());
excShow('empty string overrides it', fn() => (new ExcSubclass(''))->getMessage());
excShow('code coerced', fn() => (new Exception('m', '42'))->getCode());
excShow('message coerced', fn() => (new Exception(42))->getMessage());
excShow('array message refused', fn() => new Exception([]));
excShow('previous must be Throwable', fn() => new Exception('m', 0, 'nope'));

echo "-- ErrorException's own three\n";
excShow('severity default', fn() => (new ErrorException('m'))->getSeverity());
excShow('severity default text', fn() => (string)(new ReflectionParameter(
    ['ErrorException', '__construct'], 'severity'))->getDefaultValue());
excShow('file without line resets it', fn() => (new ErrorException('m', 0, 1, 'given.php'))->getLine());
excShow('file and line', fn() => (new ErrorException('m', 0, 1, 'given.php', 42))->getFile()
    . ':' . (new ErrorException('m', 0, 1, 'given.php', 42))->getLine());
excShow('previous is the SIXTH argument', fn() => get_class(
    (new ErrorException('m', 0, 1, null, null, new Error('deep')))->getPrevious()));

echo "-- the tree still catches\n";
excShow('DivisionByZeroError is arithmetic', fn() => (new DivisionByZeroError) instanceof ArithmeticError);
excShow('intdiv by zero', function () {
    try { intdiv(1, 0); } catch (DivisionByZeroError $e) { return get_class($e) . ': ' . $e->getMessage(); }
});
excShow('every exception is Stringable', fn() => (new BadMethodCallException) instanceof Stringable);
excShow('redeclaring a builtin class', fn() => class_exists('Exception'));
--EXPECT--
-- __toString is php's format, and the previous chain is part of it
RuntimeException: root cause in F:23
Stack trace:
# F(26): excThrower()
# {main}

Next LogicException: wrapper in F:27
Stack trace:
# {main}
-- an empty message drops the colon
Exception in F:31
Stack trace:
# {main}
-- getTraceAsString
# F(34): excThrower()
# {main}
-- the seven slots php declares, in php's order
  protected $message = ''
  private string $string = ''
  protected $code = 0
  protected string $file = ''
  protected int $line = 0
  private array $trace = array (
)
  private ?Throwable $previous = NULL
Error::$line has no default => false
ErrorException lists severity last => 'message,code,file,line,severity'
-- the getters are final, __clone is private, and clone is refused
getMessage final => true
__toString not final => false
__clone private => true
clone refused => Error: Trying to clone an uncloneable object of class Exception
method count => 11
-- the constructor writes only what it was given
subclass default kept => 'declared default'
empty string overrides it => ''
code coerced => 42
message coerced => '42'
array message refused => TypeError: Exception::__construct(): Argument #1 ($message) must be of type string, array given
previous must be Throwable => TypeError: Exception::__construct(): Argument #3 ($previous) must be of type ?Throwable, string given
-- ErrorException's own three
severity default => 1
severity default text => '1'
file without line resets it => 0
file and line => 'given.php:42'
previous is the SIXTH argument => 'Error'
-- the tree still catches
DivisionByZeroError is arithmetic => true
intdiv by zero => 'DivisionByZeroError: Division by zero'
every exception is Stringable => true
redeclaring a builtin class => true
