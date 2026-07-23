# Reductions, Statistics, Sorting, and Matrices

This page completes the operation catalog of the SimpleArray family with
the members that consume or reorder a whole array: the reductions
`min`, `max`, and `sum`, the statistics `mean`, `average`, `median`,
`var`, and `std`, the sorting group `sort`, `argsort`, and
`take_along_axis`, the searching group `argmin`, `argmax`, and
`argwhere`, and the matrix family around `matmul`.  The parity labels
of {doc}`the family overview <index>` apply throughout.  All of the
operations run over the whole storage, ghost region included, with the
scope that {doc}`The Ghost Region on the First Axis <ghost>` fixes.

## Whole-Array Reductions

`min()`, `max()`, and `sum()` take no argument and reduce the whole
array to one scalar of the element type:

```python
sarr = solvcon.SimpleArrayFloat64(shape=(2, 4), value=1.0)
assert sarr.sum() == 8.0
sarr[1, 0] = 9.2
sarr[0, 3] = -2.3
assert sarr.min() == -2.3
assert sarr.max() == 9.2
```

The scalar result matches the numpy reductions without an axis.  The
numpy `axis` keyword is not accepted: `sarr.sum(axis=0)` raises
`TypeError` from the binding's argument matching.  The statistics
below do take an axis, so the gap is specific to these three; whether
they should grow the same axis form is an open decision.

`sum()` follows the logical indices, so it is verified on strided,
non-contiguous arrays and on both C- and F-contiguous layouts, and it
returns zero on an empty array.  `min()` and `max()` address their
elements through the linear storage; the verified scope is contiguous
arrays, and the tests exercise the integer and floating-point classes.

On `SimpleArrayBool` the sum accumulates with logical or, so
`sum()` answers whether any element is true.  The boolean branch is
explicit in the kernel, so the behavior is deliberate; it diverges
from numpy, where summing a boolean array counts the true elements:

```python
sarr = solvcon.SimpleArrayBool(shape=(3, 2), value=1)
assert sarr.sum() is True     # numpy would count: 6
```

### Reductions Sweep the Ghost Region

Setting a ghost region does not change what the reductions and the
statistics see: they run over the full storage, ghost included, per
the scope that {doc}`The Ghost Region on the First Axis <ghost>`
fixes, and the axis forms enumerate the first axis from `-nghost`:

```python
narr = np.arange(24, dtype='float64').reshape((4, 3, 2))
sarr = solvcon.SimpleArrayFloat64(array=narr)
sarr.nghost = 1
assert sarr.sum() == narr.sum()
assert sarr.mean(axis=0).ndarray.tolist() == \
    np.mean(narr, axis=0).tolist()
```

## Statistics

`mean`, `average`, `median`, `var`, and `std` each come in two forms:
without an axis they reduce the whole array to a scalar, and with an
axis they return an array of the same class with the reduced axes
removed.  The axis accepts a single integer or a list of integers:

```python
narr = np.arange(24, dtype='float64').reshape((2, 3, 4))
sarr = solvcon.SimpleArrayFloat64(array=narr)
assert sarr.mean() == np.mean(narr)
sres = sarr.mean(axis=[0, 2])
assert (sres.ndarray == np.mean(narr, axis=(0, 2))).all()
```

On the floating-point classes every statistic is verified equal to its
numpy counterpart, in both forms, on contiguous and strided arrays and
on arrays with a ghost region.  Three error cases guard the axis form:
an axis outside `[0, ndim)` raises `IndexError`
(`reduce: axis out of range`), so the negative axis spelling of numpy
is rejected instead of counting from the end, and reducing no axis or
every axis raises `RuntimeError`
(`reduce: no axis to reduce or all axes are reduced`), where numpy
would return a scalar.

### The `mean` and `average` Methods

`mean()` is the arithmetic mean, `sum()` over the element count.  An
empty array raises `RuntimeError` (`SimpleArray::mean(): empty
array`), where numpy warns and returns NaN.

`average(weight=None)` without a weight is `mean()`.  The keyword is
named `weight`, not the numpy `weights`, and it takes an array of the
same class.  In the whole-array form the weight must have the
receiver's shape and weights elementwise; in the axis form
`average(axis, weight=None)` the weight supplies one value per element
of each reduced slice:

```python
narr = np.arange(6, dtype='float64').reshape((2, 3))
weights = np.array([0.5, 0.3, 0.2], dtype='float64')
sarr = solvcon.SimpleArrayFloat64(array=narr)
swei = solvcon.SimpleArrayFloat64(array=weights)
sres = sarr.average(axis=1, weight=swei)
assert np.allclose(sres.ndarray, np.average(narr, weights=weights,
                                            axis=1))
```

A weight of the wrong shape and a weight summing to zero each raise
`RuntimeError` (`SimpleArray::average(): weight shape does not match
array shape`, `SimpleArray::average(): total weight is zero`); numpy
raises `ZeroDivisionError` for the zero total.

### The `median` Method

`median()` returns the middle element, averaging the two middle
elements for an even count, equal to `numpy.median` on the
floating-point classes.  The complex classes order lexicographically
by the real part and then the imaginary part, reproducing the numpy
ordering, and the result is verified equal to `numpy.median` on
`complex128` data:

```python
narr = np.array([1 + 10j, 2 + 1j, 3 + 0j, 0 + 3j], dtype='complex128')
sarr = solvcon.SimpleArrayComplex128(array=narr)
med = sarr.median()
assert complex(med.real, med.imag) == np.median(narr)  # 1.5+5.5j
```

The 8-bit and boolean classes compute the median by frequency
counting instead of sorting; the result is verified against numpy
within the element type.

### The `var` and `std` Methods

`var(ddof=0)` and `std(ddof=0)` take the delta degrees of freedom as
numpy does, dividing by `n - ddof`; a `ddof` not smaller than the
element count raises `RuntimeError`.  On the floating-point classes
both match numpy:

```python
narr = np.arange(24, dtype='float64').reshape((2, 3, 4))
sarr = solvcon.SimpleArrayFloat64(array=narr)
assert sarr.var() == np.var(narr)
assert sarr.std(ddof=1) == np.std(narr, ddof=1)
assert (sarr.var(axis=1).ndarray == np.var(narr, axis=1)).all()
```

On the complex classes the variance accumulates the squared magnitude
and the result is real, matching numpy; in the axis form the result
array is the matching real-typed class.

### Integer Statistics Keep the Element Type

On the integer classes the statistics compute in the element type, so
every division truncates, where numpy promotes to `float64`.  The
kernels return `value_type` for `mean`, `average`, and `median`, and
the real-typed `var` and `std` reduce to the element type for the
integer classes:

```python
sarr = solvcon.SimpleArrayInt32(array=np.array([1, 2, 3, 4],
                                               dtype='int32'))
assert sarr.mean() == 2       # numpy: 2.5
assert sarr.var() == 3        # numpy: 1.25, with the truncated mean
```

The tests verify the statistics only on the floating-point and complex
classes, plus the 8-bit median; the integer truncation above is
established from the kernel source and the bound signatures.  Whether
the integer statistics should promote to a floating-point result as
numpy does is an open decision; this page records the truncating
behavior as fact.

## Sorting and Gathering

### The `sort` Method

`sort()` sorts the receiver in place, ascending, and returns `None`,
the in-place counterpart of the numpy `ndarray.sort`.  Only
one-dimensional arrays are supported; any other rank raises
`RuntimeError`:

```python
sarr = solvcon.SimpleArrayFloat64(array=np.array([3.0, 1.0, 2.0]))
sarr.sort()
assert sarr.ndarray.tolist() == [1.0, 2.0, 3.0]
solvcon.SimpleArrayFloat64((2, 3), value=0).sort()
# RuntimeError: SimpleArray::sort(): currently only support 1D array
# but the array is 2 dimension
```

The word "currently" in the message records the intent to lift the
restriction; the N-D semantics are not yet committed, so this page
defines only the one-dimensional behavior.  The sorting group is
verified on ghost-free arrays; its interaction with the ghost
partition is not yet fixed.

### The `argsort` Method

`argsort()` returns the indices that sort the receiver, under the same
one-dimensional restriction and error form as `sort()`.  The ordering
matches `numpy.argsort`, but the return type diverges from numpy: the
result is a `SimpleArrayUint64`, where numpy returns a signed `intp`
array:

```python
sarr = solvcon.SimpleArrayFloat64(array=np.array([3.0, 1.0, 2.0]))
args = sarr.argsort()
assert type(args) is solvcon.SimpleArrayUint64
assert args.ndarray.tolist() == [1, 2, 0]
```

### The `take_along_axis` Method

`take_along_axis(indices)` gathers elements of a one-dimensional
receiver by flat index.  The indices operand is a SimpleArray of any
integer class and any shape, and the result takes the operand's
shape, so composing with `argsort` yields the sorted values without
disturbing the receiver:

```python
sarr = solvcon.SimpleArrayFloat64(array=np.array([3.0, 1.0, 2.0]))
sres = sarr.take_along_axis(sarr.argsort())
assert sres.ndarray.tolist() == [1.0, 2.0, 3.0]
assert sarr.ndarray.tolist() == [3.0, 1.0, 2.0]
```

Despite the name, the semantics are those of `numpy.take` on a flat
array; the numpy `take_along_axis`, which gathers along one axis of a
same-rank index array, does not apply to the one-dimensional receiver.
The naming diverges from numpy and is recorded here.

An out-of-range index raises `IndexError` naming the offending
position in the indices operand:

```python
data = solvcon.SimpleArrayInt32(array=np.arange(10, dtype='int32'))
idx = solvcon.SimpleArrayUint64(
    array=np.array([[0, 1], [2, 3], [4, 20]], dtype='uint64'))
data.take_along_axis(idx)
# IndexError: SimpleArray::take_along_axis(): indices[2, 1] is 20,
# which is out of range of the array size 10
```

An operand that is not an integer-classed SimpleArray is not
rejected: the binding falls through without gathering and hands back
the receiver's data, silently ignoring the operand.  The explicit
list of accepted classes in the binding makes the intent clear, so
raising `TypeError` for other operands is target behavior; do not
rely on the fall-through.

`take_along_axis_simd(indices)` is the performance-explicit variant
with identical desired semantics, validating the indices up front and
gathering through the SIMD path; its out-of-range message carries the
`_simd` name.

## Searching

### The `argmin` and `argmax` Methods

`argmin()` and `argmax()` return the flat index of the smallest and
largest element as a Python `int`, matching the numpy methods without
an axis; ties resolve to the first occurrence, as in numpy:

```python
narr = np.array([[1, 3, 5, 7, 9],
                 [2, 4, 6, 8, 10],
                 [1, 10, 1, 10, 1]], dtype='float64')
sarr = solvcon.SimpleArrayFloat64(array=narr)
assert sarr.argmin() == narr.argmin() == 0
assert sarr.argmax() == narr.argmax() == 9
```

### The `argwhere` Method

`argwhere()` maps the nonzero elements to their coordinates as a
`SimpleArrayUint64` of shape `(count, ndim)`, one row per selected
element in row-major order.  The values equal `numpy.argwhere`; the
dtype diverges, unsigned against the numpy signed `intp`, the same
divergence as `argsort`.  The method is bound on every typed class,
and the boolean array of a comparison is its intended condition form;
{doc}`Elementwise Arithmetic, Comparison, and Selection <elementwise>`
fixes that spelling and the parity of the selection family:

```python
narr = np.array([[1, 3, 5], [10, 4, 10]], dtype='float64')
sarr = solvcon.SimpleArrayFloat64(array=narr)
ret = sarr.eq(10).argwhere()
assert (ret.ndarray == np.argwhere(narr == 10)).all()
```

Like the reductions, `argmin`, `argmax`, and `argwhere` address their
elements through the linear storage; the verified scope is
C-contiguous arrays.

## Matrix Operations

### The `matmul` Family

`matmul(other)` multiplies one- and two-dimensional operands of the
same class: matrix-matrix, matrix-vector, vector-matrix, and
vector-vector, with the operand shapes chained as in numpy.  The
`__matmul__` protocol is bound to it, so the `@` operator is the
equivalent spelling.  The results are verified equal to
`numpy.matmul` with one divergence: the vector-vector product returns
a one-element array of shape `(1,)` where numpy returns a scalar:

```python
a = solvcon.SimpleArrayFloat64(array=np.array([[1., 2.], [3., 4.]]))
b = solvcon.SimpleArrayFloat64(array=np.array([[5., 6.], [7., 8.]]))
assert ((a @ b).ndarray == np.array([[19., 22.], [43., 50.]])).all()

v = solvcon.SimpleArrayFloat64(array=np.array([1., 2.]))
w = solvcon.SimpleArrayFloat64(array=np.array([3., 4.]))
assert v.matmul(w).shape == (1,) and v.matmul(w)[0] == 11.0
```

A mismatched inner dimension and an operand of more than two
dimensions each raise `IndexError`; the shape text uses no spaces:

```python
a.matmul(solvcon.SimpleArrayFloat64((3, 3), value=0.0))
# IndexError: SimpleArray::matmul(): shape mismatch: this=(2,2)
# other=(3,3)
c = solvcon.SimpleArrayFloat64((2, 2, 2), value=0.0)
c.matmul(c)
# IndexError: SimpleArray::matmul(): unsupported dimensions:
# this=(2,2,2) other=(2,2,2). SimpleArray must be 1D or 2D.
```

`matmul_blas(other)` routes through the vendor BLAS when available,
and `matmul_fast(other, tile_x=16, tile_y=16, tile_z=16)` uses a tiled
kernel whose tile sizes must be positive; a non-positive tile raises
`IndexError` (`SimpleArray::matmul_fast(): tile sizes must be
positive: tile_x=0 tile_y=16 tile_z=16`).  Both are verified to
produce the `matmul` results and shapes, and both reject the same
shape errors.

### In-Place Matrix Multiplication

`imatmul`, `imatmul_blas`, and `imatmul_fast` compute the product and
replace the receiver's content, reshaping it to the result.  Like the
in-place arithmetic of
{doc}`Elementwise Arithmetic, Comparison, and Selection <elementwise>`
they return `None`, under the same open decision on returning the
receiver.  The `__imatmul__` protocol does return the receiver, as the
Python data model requires, so the `a @= b` statement works and
rebinds `a` to the updated array.

### Constructors and Transforms

`eye(n)` and `scaled_eye(n, scale)` are static methods constructing an
`n` by `n` identity and scaled identity; `n` must be positive or
`ValueError` is raised (`SimpleArray::eye(): size must be greater than
0, but got 0`).  `eye` matches `numpy.eye(n)` in the class dtype;
`scaled_eye` has no direct numpy spelling.

`pow(n)` raises a square matrix to a non-negative integer power by
squaring, with `pow(0)` the identity, matching
`numpy.linalg.matrix_power` on that domain.  A negative exponent
raises `ValueError` instead of computing the numpy matrix inverse:

```python
m = solvcon.SimpleArrayFloat64(array=np.array([[1., 2.], [3., 4.]]))
assert (m.pow(2).ndarray == np.linalg.matrix_power(m.ndarray,
                                                   2)).all()
m.pow(-1)
# ValueError: SimpleArray::pow(): exponent must be non-negative, but
# got -1
```

`hermitian()` returns the conjugate transpose as a copy of a
two-dimensional array, equal to `narr.conj().T`; on the non-complex
classes it is the transpose copy.  `symmetrize()` averages a square
matrix with its (conjugate) transpose.  `trace()` sums the diagonal of
a square matrix into a scalar; numpy's `trace` also accepts non-square
input, which these methods reject.  A wrong rank or a non-square shape
raises `RuntimeError` naming the requirement:

```python
solvcon.SimpleArrayFloat64(5).trace()
# RuntimeError: SimpleArray::trace(): operation requires 2D
# SimpleArray, but got 1D SimpleArray
solvcon.SimpleArrayFloat64((3, 4), value=0.0).symmetrize()
# RuntimeError: SimpleArray::symmetrize(): operation requires square
# SimpleArray, but got 3x4 shape
```

The bindings register the whole matrix family on every typed class.
The tests exercise `matmul`, `pow`, `eye`, and `scaled_eye` on the
floating-point classes, `hermitian` and `symmetrize` on `complex128`,
and `trace` on the floating-point, integer, and complex classes; the
other class-operation combinations follow the same kernels but are
unverified.

## The Dtype-Erased SimpleArray

From the families of this page the dtype-erased `SimpleArray` binds
only `min()`, `max()`, and `sum()`, with the whole-array semantics
above.  The statistics, the sorting and searching groups, and the
matrix family are not bound, and neither are the `@` and `@=`
operators.  The binding policy keeps the erased and typed interfaces
identical, so providing the missing members with the typed semantics
is target behavior; until then, the `typed` bridge of
{doc}`Construction and Data Types <construct>` reaches them on a copy.

## Closing the Catalog

This page ends the operation catalog that began with
{doc}`Memory Buffers <memory>`.  Across all pages one norm repeats:
the typed classes carry the full interface, the erased `SimpleArray`
converges to the same surface, and every gap between the two is
recorded as target behavior.  The parity policy of
{doc}`the family overview <index>` governs how each page relates that
surface to numpy.

<!-- vim: set ft=markdown ff=unix fenc=utf8 et sw=2 ts=2 sts=2 tw=79: -->
