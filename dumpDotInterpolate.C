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
    if (!os) throw std::runtime_error("Failed to write binary data");
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
    volVectorField vsf
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

    const surfaceVectorField& svf = mesh.Sf();

    // Getting the owner and neighbour from the mesh

    const labelUList& owner = mesh.owner();
    const labelUList& neighbour = mesh.neighbour();

    // Getting the lambda (interpolation weights)

    const surfaceScalarField& lambda = mesh.weights();


    // computing OpenFOAM's reference output
    surfaceScalarField phi_ref = fvc::interpolate(vsf) & svf;

    // IO part

    // Writing headers

    const std::string filename = runTime.path() + "dot_interpolate_dump.bin";
    std::ofstream ostrm(filename, std::ios::binary);

    const uint32_t magicNumber = 0xD07F0A01u;
    const uint8_t formatVersion = 1u;
    const uint8_t elementType = 0u;
    const int32_t nFaces = static_cast<int32_t>(mesh.nFaces());
    const int32_t nCells = static_cast<int32_t>(mesh.nCells());

    writeRaw(ostrm, magicNumber);
    writeRaw(ostrm, formatVersion);
    writeRaw(ostrm, elementType);
    writeRaw(ostrm, nFaces);
    writeRaw(ostrm, nCells);

    // Writing data

    writeRaw(ostrm, svf.component(0)); // sf.X;
    writeRaw(ostrm, svf.component(1)); // sf.Y;
    writeRaw(ostrm, svf.component(2)); // sf.Z;

    writeRaw(ostrm, lambda); // lambda;

    writeRaw(ostrm, vsf.component(0)); // vsf.x;
    writeRaw(ostrm, vsf.component(1)); // vsf.y;
    writeRaw(ostrm, vsf.component(2)); // vsf.z;

    writeRaw(ostrm, owner); // owner
    writeRaw(ostrm, neighbour); // neighbour

    writeRaw(ostrm, phi_ref); // phi_reference (ground truth)


    return 0;
}