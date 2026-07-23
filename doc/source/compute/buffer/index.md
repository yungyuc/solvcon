# Buffers and Arrays

The SimpleArray family provides typed multi-dimensional arrays whose memory
is owned by C++ and shared with Python without copying.  The classes are
implemented under `cpp/solvcon/buffer/` and exposed through the `_solvcon`
extension module, so the same contiguous memory backs both the C++ solvers
and numpy-based scripting.  The arrays are the data backbone of the solvers:
mesh connectivity, solution variables, and working storage all live in them.

This document defines the desired behavior of the Python API of the family.
Each subpage covers one group of operations and states the semantics the
implementation targets.  Behavior that is intended but not yet implemented
is explicitly marked as target behavior.

## Class Roster

The family consists of four groups of classes, all importable from the
top-level `solvcon` package.

Raw memory buffers manage untyped bytes:

- `ConcreteBuffer`: an untyped, fixed-size byte buffer.
- `BufferExpander`: an untyped, growable staging buffer that can hand its
  content over as a `ConcreteBuffer`.

Typed array classes provide multi-dimensional access on top of a
`ConcreteBuffer`.  There are 13 of them, one per element type:

- `SimpleArrayBool`
- `SimpleArrayInt8`, `SimpleArrayInt16`, `SimpleArrayInt32`,
  `SimpleArrayInt64`
- `SimpleArrayUint8`, `SimpleArrayUint16`, `SimpleArrayUint32`,
  `SimpleArrayUint64`
- `SimpleArrayFloat32`, `SimpleArrayFloat64`
- `SimpleArrayComplex64`, `SimpleArrayComplex128`

The dtype-erased class `SimpleArray` wraps any of the typed classes behind a
single Python type.  It is constructed with a shape and a dtype string (or
from a numpy `ndarray`) and dispatches operations to the concrete typed
class it holds.  The `typed` property returns a typed copy of the wrapped
array, and the `plex` property on a typed array returns an erased copy;
neither bridge shares memory with the original.

Growable typed buffers collect elements one by one, in the spirit of
`std::vector`:

- `SimpleCollectorBool`
- `SimpleCollectorInt8`, `SimpleCollectorInt16`, `SimpleCollectorInt32`,
  `SimpleCollectorInt64`
- `SimpleCollectorUint8`, `SimpleCollectorUint16`, `SimpleCollectorUint32`,
  `SimpleCollectorUint64`
- `SimpleCollectorFloat32`, `SimpleCollectorFloat64`

The extension module also registers `SimpleCollectorComplex64` and
`SimpleCollectorComplex128`, but the top-level `solvcon` package does not
export them; this document covers the exported set.

## Relation to Numpy

The Python API of the SimpleArray family is designed against numpy, but not
as a clone of it.  This document is normative: it defines the desired
behavior, and the implementation converges to it over time.  With respect to
numpy the desired behavior falls into three categories:

1. Some behavior deliberately matches numpy.  Element indexing, the buffer
   protocol, and dtype naming follow numpy so that arrays move between the
   two worlds without surprises.
2. Some behavior deliberately diverges from numpy.  The arrays serve the
   solvers first: ghost regions extend an array below index zero for
   solver halo data, alignment is an explicit constructor argument for SIMD
   kernels, and buffer ownership stays explicit instead of numpy's implicit
   base-object chain.
3. Some behavior is converging toward numpy.  Comparison and selection
   operations are being brought to numpy semantics incrementally; until the
   convergence completes, the pages note where the current behavior still
   differs from the target.

Throughout the subpages, each operation family with a numpy counterpart is
tagged with one of three parity labels:

- "matches numpy": the desired behavior is the numpy behavior; any observed
  difference is a defect.
- "diverges from numpy": the desired behavior intentionally differs; the
  tag is followed by a short statement of the target semantics.
- "converging to numpy": the numpy behavior is the target, but the current
  implementation is not there yet; the tag is followed by what still
  differs.

## Contents

```{toctree}
:maxdepth: 2

memory
construct
collector
ndarray
```

<!-- vim: set ft=markdown ff=unix fenc=utf8 et sw=2 ts=2 sts=2 tw=79: -->
