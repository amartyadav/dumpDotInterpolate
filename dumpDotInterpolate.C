#include "argList.H"
#include "timeSelector.H"
#include "Time.H"
#include "polyMesh.H"
#include "IOdictionary.H"

using namespace Foam;

int main(int argc, char *argv[])
{
    timeSelector::addOptions();
    #include "addMeshOption.H"
    #include "addRegionOption.H"
    #include "createTime.H"
    #include "setRootCase.H"
    #include "createMesh.H"


    return 0;
}