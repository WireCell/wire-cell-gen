#include "WireCellGen/BinnedDiffusion.h"
#include "WireCellIface/SimpleDepo.h"
#include "WireCellUtil/ExecMon.h"
#include "WireCellUtil/Testing.h"
#include "WireCellUtil/Point.h"
#include "WireCellUtil/Units.h"

#include "TApplication.h"
#include "TCanvas.h"
#include "TStyle.h"
#include "TFile.h"
#include "TH2F.h"
#include "TPolyMarker.h"

#include <iostream>

using namespace WireCell;
using namespace std;

struct Meta {
    //TApplication* theApp = 0;

    TCanvas* canvas;
    ExecMon em;
    const char* name;

    Meta(const char* name)
    //: theApp(new TApplication (name,0,0))
	: canvas(new TCanvas("canvas","canvas", 500,500))
	, em(name)
	, name(name) {
	print("[");
    }

    void print(const char* extra = "") {
	string fname = Form("%s.pdf%s", name, extra);
	//cerr << "Printing: " << fname << endl;
	canvas->Print(fname.c_str(), "pdf");
    }
};

const int nticks = 9600;
const double tick = 0.5*units::us;
const double drift_speed = 1.0*units::mm/units::us;
const int nwires = 1000;
const int npmwires = 10;	// effective induction range in # of wire pitches
const double wire_pitch = 3*units::mm;
const int nimpacts_per_wire_pitch = 10;
const double impact_pitch = wire_pitch/nimpacts_per_wire_pitch;


void test_track(Meta& meta, double charge, double t0, double track_time, const Ray& track_ray, double stepsize, bool fluctuate)
{
    const int nimpacts = nwires*npmwires;
    const double z_half_width = wire_pitch*0.5*nwires;

    const Point w_origin(-3*units::mm, 0.0, -z_half_width);
    const Vector w_pitchdir(0.0, 0.0, 1.0);


    const double min_time = t0;
    const double max_time = min_time + nticks*tick;
    const int ndiffision_sigma = 3.0;
    
    Gen::BinnedDiffusion bd(w_origin, w_pitchdir,
                            nimpacts, -z_half_width, z_half_width,
                            nticks, t0, t0 + nticks*tick,
                            ndiffision_sigma, fluctuate);

    auto track_start = track_ray.first;
    auto track_dir = ray_unit(track_ray);
    auto track_length = ray_length(track_ray);

    const double DL=5.3*units::centimeter2/units::second;
    const double DT=12.8*units::centimeter2/units::second;

    meta.em("begin adding depos");
    for (double dist=0.0; dist < track_length; dist += stepsize) {
	auto pt = track_start + dist*track_dir;
	double drift_time = pt.x()/drift_speed;
	pt.x(0);		// insta-drift

	const double tmpcm2 = 2*DL*drift_time/units::centimeter2;
	const double sigmaL = sqrt(tmpcm2)*units::centimeter / drift_speed;
	const double sigmaT = sqrt(2*DT*drift_time/units::centimeter2)*units::centimeter2;
	
	auto depo = std::make_shared<SimpleDepo>(t0+drift_time, pt, charge);
	bd.add(depo, sigmaL, sigmaT);
	cerr << "dist: " <<dist/units::mm << "mm, drift: " << drift_time/units::us << "us depo:" << depo->pos() << " @ " << depo->time()/units::us << "us\n";
    }

    meta.em("begin swiping wires");

    for (int iwire = 0; iwire < nwires; ++iwire) {

	const int lo_wire = std::max(iwire-npmwires, 0);
	const int hi_wire = std::min(iwire+npmwires, nwires-1);
	const int lo_impact = int(round((lo_wire - 0.5) * nimpacts_per_wire_pitch));
	const int hi_impact = int(round((hi_wire + 0.5) * nimpacts_per_wire_pitch));

	std::vector<Gen::ImpactData::pointer> collect;

	for (int impact_number = lo_impact; impact_number <= hi_impact; ++impact_number) {
	    auto impact_data = bd.impact_data(impact_number);
	    if (impact_data) {
		collect.push_back(impact_data);
	    }
	}

	if (collect.empty()) {
	    continue;
	}
	
	bd.erase(0, lo_impact);

	if (false) {		
	    continue;
	}

	auto one = collect.front();

	// find nonzero bounds int the awkward way possible.
	double min_pitch = 0.0;
	double max_pitch = 0.0;
	int min_tick = 0;
	int max_tick = 0;
	for (int ind=0; ind < collect.size(); ++ind ){
	    auto idptr = collect[ind];
	    auto mm = idptr->strip();
	    double pitch = -z_half_width + idptr->impact_number()*impact_pitch;
	    if (!ind) {
		min_pitch = max_pitch = pitch;
		min_tick = mm.first;
		max_tick = mm.second;
		continue;
	    }
	    min_tick = std::min(min_tick, mm.first);
	    max_tick = std::max(max_tick, mm.second-1);
	    min_pitch = std::min(min_pitch, pitch);
	    max_pitch = std::max(max_pitch, pitch);
	}

	double min_pitch_mm = min_pitch/units::mm;
	double max_pitch_mm = max_pitch/units::mm;
	double min_time_us = (min_tick-0.5)*tick/units::us;
	double max_time_us = (max_tick+0.5)*tick/units::us;
	double num_ticks = 1+max_tick - min_tick;

	cerr << "Tick range: [" << min_tick << "," << max_tick << "]\n";
	cerr << "Histogram: t=[" << min_time_us << "," << max_time_us << "]x" << num_ticks << " "
	     << "p=[" << min_pitch_mm << "," << max_pitch_mm << "]x" << collect.size() << "\n";

	TH2F hist("h","h", num_ticks, min_time_us, max_time_us, collect.size(), min_pitch_mm, max_pitch_mm);
	hist.SetTitle(Form("Diffused charge for wire %d", iwire));
	hist.SetXTitle("time (us)");
	hist.SetYTitle("pitch (mm)");

	for (auto idptr : collect) {
	    auto wave = idptr->waveform();
            double pitch_distance_mm = (-z_half_width + idptr->impact_number()*impact_pitch ) / units::mm;
	    //cerr << "\t" << idptr->impact_number() << "@" << pitch_distance_mm << " x " << wave.size() <<  endl;
	    Assert (wave.size() == nticks);
	    auto mm = idptr->strip();
	    for (int itick=mm.first; itick<mm.second; ++itick) {
		const double time_us = (itick * tick)/units::us;
		hist.Fill(time_us, pitch_distance_mm, wave[itick]);
	    }
	}
	hist.Draw("colz");
	meta.print();
    }
    meta.em("done");
}

int main(int argc, char* argv[])
{
    const char* me = argv[0];

    Meta meta(me);
    gStyle->SetOptStat(0);

    const double t0 = 1.0*units::s;
    const double track_time = t0+10*units::ns;
    const double delta = 100*units::mm;
    Ray track_ray(Point(1*units::m-delta, 0, -delta),
		  Point(1*units::m+delta, 0, +delta));
    const double stepsize = 1*units::mm;
    const double charge = 1e5;
    test_track(meta, charge, t0, track_time, track_ray, stepsize, true);

    meta.print("]");

    cerr << meta.em.summary() << endl;
    return 0;
}
