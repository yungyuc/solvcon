# Element Collectors

The SimpleCollector classes are growable typed buffers in the spirit of
`std::vector`.  A collector accumulates elements one by one while the
final count is unknown, then hands the result over as a `SimpleArray` for
the typed layer.  It is the element-typed counterpart of the byte-level
`BufferExpander` and is built on top of it.  Numpy has no counterpart:
numpy arrays are fixed-size, so the accumulate-then-convert workflow is
where solvcon code reaches for a collector instead of an array.

The exported roster has 11 classes, one per non-complex element type:
`SimpleCollectorBool`, `SimpleCollectorInt8` through
`SimpleCollectorInt64`, `SimpleCollectorUint8` through
`SimpleCollectorUint64`, `SimpleCollectorFloat32`, and
`SimpleCollectorFloat64`.  The extension module also registers the two
complex collectors, but the top-level `solvcon` package does not export
them.

## Construction

Two forms are supported:

```python
ct = solvcon.SimpleCollectorFloat64()    # size 0, capacity 0
ct = solvcon.SimpleCollectorFloat64(10)  # size 10, capacity 10
```

The sized form allocates storage for the given element count and sets
both the size and the capacity to it; the elements are not initialized.
The sized form also accepts an optional alignment as the second argument
(see below).

## Sizing and Growing

A collector distinguishes its size (the elements currently stored) from
its capacity (the elements allocated), both counted in elements.
`len(ct)` returns the size and the `capacity` property returns the
capacity.  `reserve(cap)` grows the capacity while keeping the size, and
`expand(length)` reserves and then sets the size, mirroring the methods
of `BufferExpander` at element granularity.

`push_back(value)` appends one element, growing the capacity when full:
an empty collector allocates capacity 1, and a full collector doubles its
capacity.  The amortized-doubling growth keeps a long accumulation loop
cheap:

```python
ct = solvcon.SimpleCollectorFloat64()
for it in range(6):
    ct.push_back(it * 1.1)
assert len(ct) == 6
assert ct.capacity == 8   # grew 0, 1, 2, 4, 8
```

`clear()` resets the size to zero and keeps the capacity and alignment,
so the collector can be refilled without reallocating.

## Element Access

Indexing reads and writes single elements of the collector's value type,
so a collector sized up front can also be filled by index instead of by
appending:

```python
ct = solvcon.SimpleCollectorFloat64()
ct.expand(6)
for it in range(6):
    ct[it] = float(it)
```

Bounds are checked against the size, not the capacity, so a
reserved-but-unfilled slot is not addressable:

```python
ct = solvcon.SimpleCollectorFloat64()
ct.reserve(6)
ct[0]  # IndexError: SimpleCollector: index 0 is out of bounds ...
```

## Converting to a SimpleArray

`as_array()` returns a one-dimensional `SimpleArray` of the matching
dtype whose length is the collector's size:

```python
ct = solvcon.SimpleCollectorFloat64()
for it in range(6):
    ct.push_back(float(it))
arr = ct.as_array()
assert list(arr) == [0.0, 1.0, 2.0, 3.0, 4.0, 5.0]
```

The conversion does not copy: it converts the underlying expander in
place, and the returned array shares memory with the collector, so a
write through one side is visible through the other.  The alignment of
the collector carries over to the array.  Growing the collector past its
capacity after the conversion reallocates the storage and detaches the
two objects; the previously returned array keeps the old memory, as does
`clear()`, which only resets the size and leaves the shared content in
place.

## Alignment

Like the arrays, a collector accepts a solvcon-specific alignment with no
numpy analogue, passed as the second constructor argument:

```python
ct = solvcon.SimpleCollectorFloat64(16, 16)
assert ct.alignment == 16
```

Valid values are 0 (the default), 16, 32, and 64 bytes; any other value
raises `ValueError`.  With a non-zero alignment, the byte count of every
allocation (the element count times the item size) must be a multiple of
the alignment, or the operation raises `ValueError`; the check applies to
the sized constructor and to `reserve`:

```python
ct = solvcon.SimpleCollectorFloat64(16, 16)  # 128 bytes, multiple of 16
ct.reserve(33)
# ValueError: BufferExpander::allocate: size ... must be a multiple ...
```

The read-only `alignment` property returns the requested value, and the
array produced by `as_array()` reports the same alignment.

<!-- vim: set ft=markdown ff=unix fenc=utf8 et sw=2 ts=2 sts=2 tw=79: -->
