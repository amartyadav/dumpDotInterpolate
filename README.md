# dumpDotInterpolate

An OpenFOAM utility that extracts the inputs and reference output of the surface
`dotInterpolate` operation from a CFD case and writes them to a packed binary
file. It is the **data-producer** half of a validation pipeline for an
FPGA-targeted reimplementation of OpenFOAM's face-interpolation kernel.

## What this is

`dumpDotInterpolate` is a standalone OpenFOAM application (built with `wmake`,
run inside a case directory). It loads a mesh and a velocity field, asks OpenFOAM
to compute the face flux `phi = Sf . interpolate(U)` using its own production
code path, and serialises everything needed to independently reproduce and
check that computation:

- the geometric and connectivity data the kernel consumes (face area vectors,
  interpolation weights, owner/neighbour indices),
- the input field (cell-centred velocity `U`),
- and OpenFOAM's own output `phi`, captured as ground truth.

The resulting binary file is consumed by the `dumpio` reader library and the
test harness, neither of which depends on OpenFOAM. This utility is the *only*
point in the validation pipeline that touches OpenFOAM at all.

## The bigger project

This utility is one component of a project to accelerate computational hotspots
of OpenFOAM CFD solvers on FPGA hardware using High-Level Synthesis (HLS). The
target hardware is a Microchip PolarFire FPGA; the reference workload is the
`motorBike` tutorial (an unstructured, snappyHexMesh-generated case
representative of industrial external-aerodynamics CFD).

The first kernel chosen for acceleration is the surface interpolation operation
`dotInterpolate`, which computes the face flux

```
phi(face) = Sf(face) . interpolate(U)(face)
```

where `Sf` is the face area vector, `U` is the cell-centred velocity field, and
`interpolate` is linear (distance-weighted) interpolation from the two adjacent
cell centres to the face.

The end-to-end pipeline:

1. **Profile** the solver to identify hotspots (done).
2. **Isolate** the hotspot into a standalone, HLS-synthesisable C/C++ kernel.
3. **Validate** the kernel on the CPU against OpenFOAM's output before any FPGA
   work. ← *this utility produces the reference data for that step*
4. **Synthesise** the validated kernel with HLS.
5. **Integrate** the generated IP into an FPGA bitstream and offload from the
   host at runtime.

## How it works

Inside the OpenFOAM environment, the utility:

1. Sets up the case (`setRootCase`, `createTime`, `createMesh`) and selects a
   time directory (latest by default).
2. Reads the velocity field `U` from the time directory as a `volVectorField`.
3. Pulls geometric and topological data directly from the mesh:
    - `Sf` — face area vectors (`mesh.Sf()`, a `surfaceVectorField`)
    - `lambda` — linear interpolation weights (`mesh.weights()`)
    - `owner`, `neighbour` — internal-face connectivity (`mesh.owner()`,
      `mesh.neighbour()`)
4. Computes the reference output with OpenFOAM's own operators:
   `phi_ref = fvc::interpolate(U) & mesh.Sf()`.
5. Extracts the contiguous primitive (internal) field from each OpenFOAM
   container, transposing vector fields from OpenFOAM's interleaved
   array-of-structs into **structure-of-arrays** (separate x/y/z streams).
6. Writes a packed binary header followed by all arrays in a fixed order.

The key implementation detail is reaching the raw contiguous buffers underneath
OpenFOAM's templated field types. Every OpenFOAM field ultimately stores its
values in a contiguous array (`UList<T>` — a size and a `T*`). The utility
obtains these via `.primitiveField().cdata()` (for fields) and `.cdata()` (for
the `labelUList` index arrays), writing the underlying bytes directly. No data
is recomputed or reformatted beyond the AoS-to-SoA transposition.

Because `Sf.component(i)` and `U.component(i)` return freshly-allocated
reference-counted temporaries (`tmp<...>`), each extracted component is bound to
a named variable before its buffer pointer is taken, so the buffer stays alive
through the write.

## Output: binary format

A 14-byte packed header followed by the data arrays in a fixed order. All values
little-endian (x86). This format is the contract shared with the `dumpio`
reader; the two must stay in lockstep, and the `formatVersion` field exists to
enforce that.

### Header (14 bytes, packed, no padding)

| Field           | Type   | Size | Notes                                   |
|-----------------|--------|------|-----------------------------------------|
| `magicNumber`   | uint32 | 4    | `0xD07F0A01` — identifies the format    |
| `formatVersion` | uint8  | 1    | currently `1`                           |
| `elementType`   | uint8  | 1    | `0` = FP64 (double), `1` = FP32 (float) |
| `nFaces`        | int32  | 4    | number of **internal** faces            |
| `nCells`        | int32  | 4    | number of cells                         |

### Data section (arrays, in this exact order)

| Array       | Element | Count  | Source                            |
|-------------|---------|--------|-----------------------------------|
| `Sf_x`      | double  | nFaces | `mesh.Sf()` x component           |
| `Sf_y`      | double  | nFaces | `mesh.Sf()` y component           |
| `Sf_z`      | double  | nFaces | `mesh.Sf()` z component           |
| `lambda`    | double  | nFaces | `mesh.weights()` (linear weights) |
| `U_x`       | double  | nCells | `U` x component                   |
| `U_y`       | double  | nCells | `U` y component                   |
| `U_z`       | double  | nCells | `U` z component                   |
| `owner`     | int32   | nFaces | `mesh.owner()`                    |
| `neighbour` | int32   | nFaces | `mesh.neighbour()`                |
| `phi_ref`   | double  | nFaces | `fvc::interpolate(U) & mesh.Sf()` |

Vector fields are stored in structure-of-arrays (SoA) layout — three separate
contiguous component arrays, matching the access pattern the HLS kernel expects.

## Building

`dumpDotInterpolate` is an OpenFOAM application built with `wmake`. It lives
outside the OpenFOAM source tree, in its own directory.

```
dumpDotInterpolate/
├── dumpDotInterpolate.C
└── Make/
    ├── files
    └── options
```

`Make/files`:

```
dumpDotInterpolate.C

EXE = $(FOAM_USER_APPBIN)/dumpDotInterpolate
```

`Make/options` (minimal — the finiteVolume and meshTools modules):

```
EXE_INC = \
    -I$(LIB_SRC)/finiteVolume/lnInclude \
    -I$(LIB_SRC)/meshTools/lnInclude

EXE_LIBS = \
    -lfiniteVolume \
    -lmeshTools
```

To build, with the OpenFOAM environment sourced:

```bash
cd dumpDotInterpolate
wmake
```

The binary is written to `$FOAM_USER_APPBIN` and is on the `PATH`.

### Requirements

- OpenFOAM (developed against OpenFOAM Foundation v13).
- A C++ compiler compatible with the OpenFOAM build. If field headers
  (`volFields.H` and the `DimensionedField`/`GeometricField` template hierarchy)
  fail to compile, confirm the compiler matches the one the OpenFOAM
  installation was built with, and that the source tree is unmodified
  (`git status` in the OpenFOAM directory).

## Usage

Run inside an OpenFOAM case directory that has been run to at least one time
step:

```bash
cd <case>             # e.g. a motorBike case
dumpDotInterpolate
```

By default it selects the latest available time directory, reads `U` from it,
and writes `dot_interpolate_dump.bin` into the case directory.

The produced file is then validated independently:

```bash
test_harness dot_interpolate_dump.bin
```

## Scope and limitations

- **Internal faces only.** Boundary and coupled-patch faces are not included;
  the dump covers the internal-face interpolation loop, which is the kernel's
  scope.
- **Linear interpolation.** The dumped `lambda` weights are the mesh's linear
  weights. For the kernel to reproduce `phi_ref`, the reference must be the
  linear interpolation of `U`. If a case's `fvSchemes` uses a non-linear scheme
  for `interpolate(U)`, the reference will not match a linear kernel — verify
  the `interpolationSchemes` entry before trusting a comparison.
- **FP64 output.** The format reserves an FP32 element type for future use, but
  current dumps are double precision.
- **Little-endian.** No byte-swapping; intended for x86.
- **Serial.** Run on a reconstructed (single-region) case, not on decomposed
  processor directories.

## Relationship to the rest of the project

```
  dumpDotInterpolate  ──writes──►  dot_interpolate_dump.bin
   (OpenFOAM, this)                       │
                                          │ read by
                                          ▼
                              dumpio (reader library)
                                          │ feeds
                                          ▼
                              test harness  ──►  kernel vs phi_ref comparison
```

- **dumpDotInterpolate** (this) — OpenFOAM producer, run rarely (once per test
  case).
- **dumpio** — OpenFOAM-free reader library, parses and validates the dump.
- **test harness** — links `dumpio` and the standalone kernel; runs the
  comparison that validates the kernel against OpenFOAM's output.
