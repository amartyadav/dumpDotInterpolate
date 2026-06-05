#include "argList.H"
#include "timeSelector.H"
#include "Time.H"
#include "fvMesh.H"           // ← add this
#include "volFields.H"
#include "surfaceFields.H"    // ← you'll need this for Sf and phi too
#include "fvc.H"              // ← for fvc::interpolate later
#include <fstream>

using namespace Foam;

template <typename T>
void writeRaw(std::ofstream& os, const T& v)
{
    os.write(reinterpret_cast<const char*>(&v), sizeof(T));
    if (!os) throw std::runtime_error(" === Failed to write binary data === ");
}

template <typename T>
void writeRawArrays(std::ofstream& os, const T* ptr, const int32_t count)
{
    os.write(reinterpret_cast<const char*>(ptr), count * sizeof(T));
    if (!os) throw std::runtime_error(" === Failed to write raw array binary data === ");
}

int main(int argc, char *argv[])
{
    timeSelector::addOptions();
    #include "addMeshOption.H"
    #include "addRegionOption.H"
    #include "setRootCase.H"
    #include "createTime.H"
    #include "createMesh.H"

    // Use the currently selected (usually the latest) time directory
    // pass `runTime.timeName()` as the time and `mesh` so IOobject looks in
    // the case/time/ directory for the field "U".

    // get all the time directories
    const instantList timeDirs = timeSelector::select0(runTime, args);
    instant latestTime;

    // if size of time directories is greater than 0, put the last time directory in latestTime

    if (timeDirs.size() > 0)
    {
        latestTime = timeDirs.last();
    }

    // latestTime is used below
    volVectorField U
    (
        IOobject
        (
            "U",
            runTime.timeName(latestTime.value()),
            mesh,
            IOobject::MUST_READ,
            IOobject::NO_WRITE
        ),
        mesh
    );

    // Getting the geometric data from mesh

    const surfaceVectorField& Sf = mesh.Sf();

    // Getting the owner and neighbour from the mesh

    const labelUList& owner = mesh.owner();
    const labelUList& neighbour = mesh.neighbour();

    // Getting the lambda (interpolation weights)

    const surfaceScalarField& lambda = mesh.weights();


    // computing OpenFOAM's reference output
    surfaceScalarField phi_ref = fvc::interpolate(U) & Sf;

    // IO part

    // Writing headers

    const std::string filename = runTime.path() / "dot_interpolate_dump.bin";
    std::ofstream ostrm(filename, std::ios::binary);
    if (!ostrm) throw std::runtime_error(" === Failed to open/create file === ");

    const uint32_t magicNumber = 0xD07F0A01u;
    const uint8_t formatVersion = 1u;
    const uint8_t elementType = 0u;
    const int32_t nFaces = static_cast<int32_t>(mesh.nInternalFaces());
    const int32_t nCells = static_cast<int32_t>(mesh.nCells());

    writeRaw(ostrm, magicNumber);
    writeRaw(ostrm, formatVersion);
    writeRaw(ostrm, elementType);
    writeRaw(ostrm, nFaces);
    writeRaw(ostrm, nCells);


    // Writing data

    // extracting the cdata to a more permanent structure, as tmp directly like so gets destroyed at the end of the statement, and could be buggy or fragile
    // writeRawArrays(ostrm, Sf.component(0).ref().primitiveField().cdata()); // sf.X;

    tmp<surfaceScalarField> Sfx = Sf.component(0);
    tmp<surfaceScalarField> Sfy = Sf.component(1);
    tmp<surfaceScalarField> Sfz = Sf.component(2);

    const scalarField& SfxField = Sfx.ref().primitiveField();
    const scalarField& SfyField = Sfy.ref().primitiveField();
    const scalarField& SfzField = Sfz.ref().primitiveField();

    writeRawArrays(ostrm, SfxField.cdata(), SfxField.size()); // sf.X;
    writeRawArrays(ostrm, SfyField.cdata(), SfyField.size()); // sf.Y;
    writeRawArrays(ostrm, SfzField.cdata(), SfzField.size()); // sf.Z;

    writeRawArrays(ostrm, lambda.primitiveField().cdata(), lambda.primitiveField().size()); // lambda;

    // same extraction logic as above for U (velocity vector fields)

    tmp<volScalarField> Ux = U.component(0);
    tmp<volScalarField> Uy = U.component(1);
    tmp<volScalarField> Uz = U.component(2);

    const scalarField& UxField = Ux.ref().primitiveField();
    const scalarField& UyField = Uy.ref().primitiveField();
    const scalarField& UzField = Uz.ref().primitiveField();

    writeRawArrays(ostrm, UxField.cdata(), UxField.size()); // vsf.x;
    writeRawArrays(ostrm, UyField.cdata(), UyField.size()); // vsf.y;
    writeRawArrays(ostrm, UzField.cdata(), UzField.size()); // vsf.z;

    writeRawArrays(ostrm, owner.cdata(), owner.size()); // owner
    writeRawArrays(ostrm, neighbour.cdata(), neighbour.size()); // neighbour

    writeRawArrays(ostrm, phi_ref.primitiveField().cdata(), phi_ref.primitiveField().size());
    // phi_reference (ground truth)

    return 0;
}