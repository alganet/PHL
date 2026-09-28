<?php
/* A refusal php makes at RUN time: it cannot early-bind a name it already holds,
 * so DECLARE_CLASS reports it and the require IS on the trace. */
class CfstDup {}
class CfstDup {}
