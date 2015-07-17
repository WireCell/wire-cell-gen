#include "WireCellIface/IConfigurable.h"

#include "WireCellIface/IWireParameters.h"
#include "WireCellIface/IWireGenerator.h"
#include "WireCellIface/IWireProvider.h"
#include "WireCellIface/IWireDatabase.h"

#include "WireCellIface/ICellGenerator.h"
#include "WireCellIface/ICellProvider.h"
#include "WireCellIface/ICellDatabase.h"

#include "WireCellUtil/Testing.h"

#include "WireCellUtil/NamedFactory.h"

#include <iostream>

using namespace WireCell;
using namespace std;


int main()
{
    // These are here to force the linker to give us the symbols
    WIRECELL_NAMEDFACTORY_USE(WireParams);
    WIRECELL_NAMEDFACTORY_USE(ParamWires);
    WIRECELL_NAMEDFACTORY_USE(WireDatabase);
    WIRECELL_NAMEDFACTORY_USE(BoundCells);
    WIRECELL_NAMEDFACTORY_USE(CellDatabase);

    // fixme: this C++ dance to wire up the interfaces may eventually
    // be done inside a workflow engine.

    // fixme: this needs to be done by a configuration service
    auto wp_cfg = WireCell::Factory::lookup<IConfigurable>("WireParams");
    Assert(wp_cfg, "Failed to get IConfigurable from default WireParams");
    wp_cfg->configure(wp_cfg->default_configuration());
    cout << "Got WireParams IConfigurable interface @ " << wp_cfg << endl;

    auto wp_wps = WireCell::Factory::lookup<IWireParameters>("WireParams");
    Assert(wp_wps, "Failed to get IWireParameters from default WireParams");
    cout << "Got WireParams IWireParameters interface @ " << wp_wps << endl;
    
    auto pw_gen = WireCell::Factory::lookup<IWireGenerator>("ParamWires");
    Assert(pw_gen, "Failed to get IWireGenerator from default ParamWires");
    cout << "Got ParamWires IWireGenerator interface @ " << pw_gen << endl;
    pw_gen->generate(*wp_wps);

    auto pw_pro = WireCell::Factory::lookup<IWireProvider>("ParamWires");
    Assert(pw_pro, "Failed to get IWireProvider from default ParamWires");
    cout << "Got ParamWires IWireProvider interface @ " << pw_pro << endl;

    const WireCell::WireSet& wires = pw_pro->wires();
    int nwires = wires.size();
    cout << "Got " << nwires << " wires" << endl;
    Assert(1103 == nwires);

    auto wdb = WireCell::Factory::lookup<IWireDatabase>("WireDatabase");
    Assert(wdb, "Failed to get IWireDatabase from default WireDatabase");
    cout << "Got WireDatabase IWireDatabase interface @ " << wdb << endl;
    wdb->load(wires);

    auto bc_gen = WireCell::Factory::lookup<ICellGenerator>("BoundCells");
    Assert(bc_gen, "Failed to get ICellGenerator from default BoundCells");
    cout << "Got BoundCells ICellGenerator interface @ " << bc_gen << endl;

    bc_gen->generate(wdb);

    auto bc_pro = WireCell::Factory::lookup<ICellProvider>("BoundCells");
    Assert(bc_pro, "Failed to get ICellProvider from default BoundCells");
    cout << "Got BoundCells ICellProvider interface @ " << bc_pro << endl;

    const WireCell::CellSet& cells = bc_pro->cells();
    int ncells = cells.size();
    cout << "Got " << ncells << " cells" << endl;

    auto cdb = WireCell::Factory::lookup<ICellDatabase>("CellDatabase");
    Assert(cdb, "Failed to get ICellDatabase from default CellDatabase");
    cout << "Got CellDatabase ICellDatabase interface @ " << cdb << endl;
    
    cdb->load(bc_pro->cells());


    return 0;
}

