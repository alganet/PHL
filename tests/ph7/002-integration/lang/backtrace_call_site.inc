<?php
/* Every call below is written HERE; the frames must say so, whichever block the
 * call sits in and however many lines it spans. */
function bcsWhere($tag) {
    $f = debug_backtrace(DEBUG_BACKTRACE_IGNORE_ARGS)[0];
    echo str_pad($tag, 12), basename($f['file']), ':', $f['line'], "\n";
}
class BcsT {
    public function plain()     { bcsWhere('plain'); }
    public function inTry()     { try { bcsWhere('inTry'); } catch (Throwable $e) {} }
    public function inCatch()   { try { throw new RuntimeException('x'); } catch (Throwable $e) { bcsWhere('inCatch'); } }
    public function inFinally() { try { } finally { bcsWhere('inFinally'); } }
    public function nested()    { try { try { bcsWhere('nested'); } finally {} } catch (Throwable $e) {} }
    public function multi()     { bcsWhere(
                                      'multi'
                                  ); }
    public function tryMulti()  { try { bcsWhere(
                                      'tryMulti'
                                  ); } catch (Throwable $e) {} }
}
function bcsGlobal() { try { bcsWhere('global'); } catch (Throwable $e) {} }
