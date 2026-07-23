# The Ghost Region on the First Axis

A solver stencil near a domain boundary reads neighbors that lie outside
the domain.  The SimpleArray family serves that access pattern directly:
the `nghost` property splits the first axis of an array into a ghost
region and a body, so that boundary (halo) data lives in the same
storage as the interior and is addressed with negative indices.  The feature is entirely solvcon-specific; numpy has no
counterpart, so the parity labels of {doc}`the family overview <index>`
do not apply on this page.  Where the ghost region meets an operation
family that numpy also sees, such as slice assignment or the shared
views, the page states the interaction directly.
{doc}`Indexing, Shape, and Layout Manipulation <indexing>` defines the
ghost-free base semantics that this page extends.

## The Partition Model

Every array starts without a ghost region: `nghost` is 0 and
`has_ghost` is `False`.  Assigning a positive `nghost` designates the
first `nghost` positions along the first axis as the ghost region and
the remaining `nbody` positions as the body.  No memory moves and the
shape does not change; only the index origin shifts.  Index 0 becomes
the first body element, and the ghost elements sit at the negative
indices `-nghost` through `-1`:

```python
sarr = solvcon.SimpleArrayFloat64(24)
sarr.ndarray[:] = np.arange(24)
sarr.nghost = 10

assert sarr.has_ghost
assert sarr.nbody == 14
assert sarr.shape == (24,)   # the shape is unchanged
assert sarr[-10] == 0.0      # first ghost element
assert sarr[0] == 10.0       # first body element
assert sarr[13] == 23.0      # last body element
```

Only the first axis carries the partition.  On a multi-dimensional
array the later axes keep the plain index arithmetic of
{doc}`Indexing, Shape, and Layout Manipulation <indexing>`:

```python
sarr = solvcon.SimpleArrayFloat64((4, 3, 2))
sarr.ndarray.flat[:] = range(24)
sarr.nghost = 1

assert sarr.nbody == 3
assert sarr[-1, 0, 0] == 0.0    # the ghost row
assert sarr[0, 0, 0] == 6.0     # the first body row
assert sarr[0, -1, 0] == 10.0   # later axes wrap plainly
```

### Valid Index Range and Wrapping

The valid interval on the first axis is
`[-(shape[0] + nghost), nbody)`.  Indices in `[-nghost, nbody)`
address the partition directly, as above.  An index below `-nghost`
wraps python-style over the storage: the ghost shift makes it
negative relative to the start of storage, and the wrap adds
`shape[0]`, so index `i` resolves to storage position
`i + nghost + shape[0]`.  In particular `sarr[-nghost - 1]` is the
last storage element, mirroring how `sarr[-1]` on a ghost-free array
is the last element:

```python
sarr = solvcon.SimpleArrayInt8(8)
sarr.ndarray[:] = np.arange(8, dtype='int8')
sarr.nghost = 3

assert sarr[-3] == 0     # first ghost element
assert sarr[-4] == 7     # wraps to the last storage element
assert sarr[-11] == 0    # wraps to the first storage element
sarr[-4] = 70
assert sarr.ndarray[7] == 70
```

An index outside the interval raises `IndexError`, and the message
carries the ghost arithmetic.  The one-dimensional form:

```python
sarr = solvcon.SimpleArrayFloat64(24)
sarr.nghost = 10
sarr[14]
# IndexError: SimpleArray: index 14 >= 14 (shape[0]: 24 - nghost: 10)
sarr[-35]
# IndexError: SimpleArray: index -35 < -nghost - shape[0]: -34
```

The multi-dimensional form names the offending dimension:

```python
sarr = solvcon.SimpleArrayFloat64((4, 3, 2))
sarr.nghost = 1
sarr[3, 0, 0]
# IndexError: SimpleArray: dim 0 in [3, 0, 0] >= nbody: 3
# (shape[0]: 4 - nghost: 1)
sarr[-6, 0, 0]
# IndexError: SimpleArray: dim 0 in [-6, 0, 0] < -nghost - shape[0]: -5
```

Both errors apply to reads and writes alike.

## Setting the `nghost` Property

The `nghost` setter accepts any value from 0 through `shape[0]`.
Setting it back to 0 removes the region, and setting it to the full
first-axis extent makes the whole axis ghost:

```python
sarr = solvcon.SimpleArrayInt8(10)
sarr.nghost = 10            # the whole first axis may be ghost
assert sarr.nbody == 0
sarr.nghost = 0             # zero removes the region
assert not sarr.has_ghost
```

Three violations raise `IndexError`.  The value cannot exceed the
first-axis extent, cannot be negative, and cannot be positive on a
zero-dimensional array (which the message calls empty); an array whose
first axis has zero extent falls under the `shape(0)` bound instead:

```python
sarr = solvcon.SimpleArrayInt8(10)
sarr.nghost = 11
# IndexError: SimpleArray: cannot set nghost 11 > shape(0) 10
sarr.nghost = -1
# IndexError: SimpleArray: cannot set negative nghost -1
solvcon.SimpleArrayInt8(()).nghost = 1
# IndexError: SimpleArray: cannot set nghost 1 > 0 to an empty array
```

## The `has_ghost` and `nbody` Properties

`has_ghost` reports whether `nghost` is nonzero.  `nbody` counts the
body positions along the first axis, `shape[0] - nghost`; it is not an
element count, so a ghost-free `(4, 3, 2)` array reports `nbody == 4`
and the same array with `nghost = 1` reports 3.  A zero-dimensional
array reports 0.  Neither `shape`, `size`, nor `len()` changes with
the partition; they keep describing the full storage as defined in
{doc}`Indexing, Shape, and Layout Manipulation <indexing>`.

## Slice and Ellipsis Assignment

The slice keys of `__setitem__` interpret their explicit bounds on the
first axis in the logical, ghost-shifted coordinates of this page: the
parser adds `nghost` to an explicit start or stop bound and then
applies the ordinary Python slice rules over the full first-axis
extent.  An omitted bound is not shifted; it means the storage edge,
so with a forward step an omitted start begins at the first ghost
element and an omitted stop runs to the end of storage.  The stop bound `0` therefore selects
exactly the ghost region, and the start bound `0` selects the body:

```python
sarr = solvcon.SimpleArrayFloat64(shape=5, value=0)
sarr.nghost = 2

sarr[-2:0] = np.array([10.0, 11.0])        # the ghost region
sarr[0:] = np.array([12.0, 13.0, 14.0])    # the body
assert sarr.ndarray.tolist() == [10, 11, 12, 13, 14]

sarr[:0] = np.array([20.0, 21.0])          # also the ghost region
assert sarr.ndarray.tolist() == [20, 21, 12, 13, 14]
```

Because both bounds default to the storage edges, a bare slice, a
stepped slice, or an ellipsis covers the whole storage including the
ghost region, and a negative step reverses over it:

```python
sarr = solvcon.SimpleArrayFloat64(shape=5, value=0)
sarr.nghost = 2
sarr[::2] = np.array([10.0, 11.0, 12.0])
assert sarr.ndarray.tolist() == [10, 0, 11, 0, 12]

sarr[...] = np.arange(5, dtype='float64')
assert sarr[-2] == 0.0 and sarr[2] == 4.0
```

In a tuple key only the first-axis slice is shifted; slices on the
later axes keep the ghost-free semantics:

```python
sarr = solvcon.SimpleArrayFloat64(shape=(5, 3), value=0)
sarr.nghost = 2
sarr[-2:0, ...] = np.arange(6, dtype='float64').reshape((2, 3))
assert (sarr.ndarray[0:2] == np.arange(6).reshape((2, 3))).all()
```

The accepted right-hand sides, the exact-shape check, and the dtype
conversion rules are those of
{doc}`Indexing, Shape, and Layout Manipulation <indexing>`, unchanged
by the partition.

### Failure Preserves the Partition

A rejected assignment does not disturb the ghost setting.  When the
right-hand side fails the dtype conversion, the array keeps its
`nghost` as before the statement:

```python
sarr = solvcon.SimpleArrayFloat64(shape=(2, 2), value=0)
sarr.nghost = 1
sarr[...] = np.ones((2, 2), dtype='complex128')
# RuntimeError: Cannot convert between complex and non-complex types
assert sarr.nghost == 1
```

## Ghost Regions on Strided Arrays

The partition composes with the strided layouts of
{doc}`Numpy and Buffer-Protocol Interoperation <ndarray>`.  On an
array wrapping a strided view, the ghost indices address the viewed
elements, and a write through a ghost or wrapped index lands in the
viewed region of the original memory:

```python
base = np.arange(12, dtype='float64')
sarr = solvcon.SimpleArrayFloat64(array=base[::2])
sarr.nghost = 2

assert sarr[-2] == 0.0     # first viewed element
assert sarr[-3] == 10.0    # wraps to the last viewed element
sarr[-3] = 200.0
assert base[10] == 200.0
```

## Reductions and Statistics

Reductions and statistics operate over the whole array, ghost region
included: `sum()`, `min()`, `max()`, `median()`, and `average()` on an
array with a ghost region return the same result as on the ghost-free
array, and the axis-wise forms enumerate the first axis from
`-nghost`.  A later page on reductions, statistics, and searching
defines the operations themselves; this page only fixes their scope
with respect to the partition.

## Shared Views Ignore the Partition

The `ndarray` property and the buffer protocol expose the full storage
and index from its start: element `-nghost` of the array is element 0
of the view, as
{doc}`Numpy and Buffer-Protocol Interoperation <ndarray>` defines.
The view's shape spans the full first dimension, so numpy code sees
one plain array and the partition exists only in the array's own
subscript arithmetic:

```python
sarr = solvcon.SimpleArrayFloat64(4, value=0.0)
sarr.nghost = 2
sarr[-2] = 1.0
assert sarr.ndarray[0] == 1.0
assert memoryview(sarr).shape == (4,)
```

## The Dtype-Erased SimpleArray

The dtype-erased `SimpleArray` mirrors the typed ghost surface, per
the binding policy that keeps the two interfaces identical.  The
`nghost`, `has_ghost`, and `nbody` properties carry the semantics and
the error messages of this page, and the subscript operator and the
slice assignment parser apply the same ghost shift on the first axis:

```python
plex = solvcon.SimpleArray(5, dtype='float64')
np.array(plex, copy=False)[:] = np.arange(5)
plex.nghost = 2
assert plex.nbody == 3
assert plex[-2] == 0.0 and plex[0] == 2.0
plex[0:1] = np.array([20.0])
assert plex[0] == 20.0
```

<!-- vim: set ft=markdown ff=unix fenc=utf8 et sw=2 ts=2 sts=2 tw=79: -->
