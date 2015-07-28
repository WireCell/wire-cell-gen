#include "WireCellIface/IWireSelectors.h"
#include "WireCellNav/ParamWires.h"
#include "WireCellNav/WireParams.h"
#include "WireCellUtil/Testing.h"

#include "TApplication.h"
#include "TCanvas.h"
#include "TH1F.h"
#include "TArrow.h"
#include "TLine.h"

#include <boost/range.hpp>

#include <iostream>
#include <cmath>

using namespace WireCell;
using namespace std;


void test3D(bool interactive)
{
    WireParams params;
    double pitch = 10.0;
    auto cfg = params.default_configuration();
    cfg.put("pitch_mm.u", pitch);
    cfg.put("pitch_mm.v", pitch);
    cfg.put("pitch_mm.w", pitch);
    params.configure(cfg);

    ParamWires pw;
    pw.generate(params);

    const Ray& bbox = params.bounds();

    TApplication* theApp = 0;
    if (interactive) {
	theApp = new TApplication ("test_paramwires",0,0);
    }

    TCanvas c("c","c",500,500);
    TH1F* frame = c.DrawFrame(bbox.first.z(), bbox.first.y(),
			      bbox.second.z(), bbox.second.y());
    frame->SetTitle("red=U, blue=V, +X (-drift) direction into page");
    frame->SetXTitle("Transverse Z direction");
    frame->SetYTitle("Transverse Y (W) direction");

    int colors[3] = {2, 4, 1};

    std::vector<const IWire*> wires(pw.wires_begin(), pw.wires_end());
    Assert(wires.size(), "Got no wires");

    vector<const IWire*> u_wires, v_wires, w_wires;
    copy_if(wires.begin(), wires.end(), back_inserter(u_wires), select_u_wires);
    copy_if(wires.begin(), wires.end(), back_inserter(v_wires), select_v_wires);
    copy_if(wires.begin(), wires.end(), back_inserter(w_wires), select_w_wires);
    size_t n_wires[3] = {
	u_wires.size(),
	v_wires.size(),
	w_wires.size()
    };

    double max_width = 5;
    for (auto wit = wires.begin(); wit != wires.end(); ++wit) {
	const IWire& wire = **wit;
	int iplane = wire.plane();
	int index = wire.index();

	Assert(n_wires[iplane], "Empty plane");
	double width = 1.0+ (((index+1)*max_width)/n_wires[iplane]);

	const Ray ray = wire.ray();

	TArrow* a_wire = new TArrow(ray.first.z(), ray.first.y(),
				    ray.second.z(), ray.second.y(), 0.01, "|>");
	a_wire->SetLineColor(colors[iplane]);
	a_wire->SetLineWidth(width);
	a_wire->Draw();
    }

    if (theApp) {
	theApp->Run();
    }
    else {			// batch
	c.Print("test_paramwires.pdf");
    }

}

int main(int argc, char** argv)
{
    test3D(argc>1);
    return 0;
}