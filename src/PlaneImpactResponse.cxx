#include "WireCellIface/IFieldResponse.h"
#include "WireCellIface/IWaveform.h"
#include "WireCellGen/PlaneImpactResponse.h"
#include "WireCellUtil/Testing.h"
#include "WireCellUtil/NamedFactory.h"

#include <iostream>             // debugging

WIRECELL_FACTORY(PlaneImpactResponse, WireCell::Gen::PlaneImpactResponse,
                 WireCell::IPlaneImpactResponse, WireCell::IConfigurable);


using namespace std;
using namespace WireCell;


Gen::PlaneImpactResponse::PlaneImpactResponse(int plane_ident, size_t nbins, double tick)
    : m_frname("FieldResponse")
    , m_plane_ident(plane_ident)
    , m_nbins(nbins)
    , m_tick(tick)
{
}


WireCell::Configuration Gen::PlaneImpactResponse::default_configuration() const
{
    Configuration cfg;
    // IFieldResponse component
    cfg["field_response"] = m_frname; 
    // plane id to use to index into field response .plane()
    cfg["plane"] = 0;            
    // names of IWaveforms interpreted as subsequent response
    // functions.
    cfg["other_responses"] = Json::arrayValue;
    // number of bins in impact response spectra
    cfg["nbins"] = 10000;
    // sample period of response waveforms
    cfg["tick"] = 0.5*units::us; 
    return cfg;
}

void Gen::PlaneImpactResponse::configure(const WireCell::Configuration& cfg)
{
    m_frname = get(cfg, "field_response", m_frname);
    m_plane_ident = get(cfg, "plane", m_plane_ident);

    m_others.clear();
    auto jfilts = cfg["other_responses"];
    if (!jfilts.isNull() and !jfilts.empty()) {
        for (auto jfn: jfilts) {
            auto tn = jfn.asString();
            m_others.push_back(tn);
        }
    }

    m_nbins = (size_t) get(cfg, "nbins", (int)m_nbins);
    m_tick = get(cfg, "tick", m_tick);

    build_responses();
}


void Gen::PlaneImpactResponse::build_responses()
{
    auto ifr = Factory::find_tn<IFieldResponse>(m_frname);

    // build "other" response spectra
    WireCell::Waveform::compseq_t other(m_nbins);
    const size_t nother = m_others.size();
    for (size_t ind=0; ind<nother; ++ind) {
        const auto& name = m_others[ind];
        auto iw = Factory::find_tn<IWaveform>(name);
        if (std::abs(iw->period() - m_tick) < 1*units::ns) {
            THROW(ValueError() << errmsg{"Tick mismatch in " + name});
        }
        auto wave = iw->samples(); // copy
        if (wave.size() != m_nbins) {
            cerr << "Gen::PlaneImpactResponse: warning: "
                 << "other response has different number of samples ("
                 << wave.size()
                 << ") than expected ("<<m_nbins<<"), resizing\n";
            wave.resize(m_nbins, 0);
        }
        auto spec = Waveform::dft(wave);
        if (!ind) {
            for (size_t ibin=0; ibin < m_nbins; ++ibin) {
                other[ibin] = spec[ibin];
            }
        }
        else {
            for (size_t ibin=0; ibin < m_nbins; ++ibin) {
                other[ibin] *= spec[ibin];
            }
        }
    }


    const auto& fr = ifr->field_response();
    const auto& pr = *fr.plane(m_plane_ident);
    const int npaths = pr.paths.size();

    // FIXME HUGE ASSUMPTIONS ABOUT ORGANIZATION OF UNDERLYING
    // FIELD RESPONSE DATA!!!
    //
    // Paths must be in increasing pitch with one impact position at
    // nearest wire and 5 more impact positions equally spaced and at
    // smaller pitch distances than the associated wire.  The final
    // impact position should be no further from the wire than 1/2
    // pitch.

    const int n_per = 6;        // fixme: assumption
    const int n_wires = npaths/n_per;
    const int n_wires_half = n_wires / 2; // integer div
    //const int center_index = n_wires_half * n_per;

    /// FIXME: this assumes impact positions are on uniform grid!
    m_impact = std::abs(pr.paths[1].pitchpos - pr.paths[0].pitchpos);
    /// FIXME: this assumes paths are ordered by pitch
    m_half_extent = std::max(std::abs(pr.paths.front().pitchpos),
                             std::abs(pr.paths.back().pitchpos));
    /// FIXME: this assumes detailed ordering of paths w/in one wire
    m_pitch = 2.0*std::abs(pr.paths[n_per-1].pitchpos - pr.paths[0].pitchpos);


    // native response time binning
    const int rawresp_size = pr.paths[0].current.size();
    const double rawresp_min = fr.tstart;
    const double rawresp_tick = fr.period;
    const double rawresp_max = rawresp_min + rawresp_size*rawresp_tick;
    Binning rawresp_bins(rawresp_size, rawresp_min, rawresp_max);
    //std::cerr << "PlaneImpactResponse: field responses: " << rawresp_size
    //          << "bins covering ["<<rawresp_min/units::us<<","<<rawresp_max/units::us<<"]/"<<rawresp_tick/units::us << " us\n";


    // collect paths and index by wire and impact position.
    std::map<int, region_indices_t> wire_to_ind;
    for (int ipath = 0; ipath < npaths; ++ipath) {
        const Response::Schema::PathResponse& path = pr.paths[ipath];
        const int wirenum = int(ceil(path.pitchpos/pr.pitch)); // signed
        wire_to_ind[wirenum].push_back(ipath);

        // match response sampling to digi and zero-pad
        WireCell::Waveform::realseq_t wave(m_nbins, 0.0);
        for (int rind=0; rind<rawresp_size; ++rind) { // sample at fine bins of response function
            const double time = rawresp_bins.center(rind);

            // fixme: assumes field response appropriately centered
            const size_t bin = time/m_tick; 

            if (bin>= m_nbins) {
                std::cerr << "PIR: out of bounds field response bin: " << bin
                          << " ntbins=" << m_nbins
                          << " time=" << time/units::us << "us"
                          << " tick=" << m_tick/units::us << "us"
                          << std::endl;
                THROW(ValueError() << errmsg{"PIR: out of bounds field response bin"});
            }


            // Here we have sampled, instantaneous induced *current*
            // (in WCT system-of-units for current) due to a single
            // drifting electron from the field response function.
            const double induced_current = path.current[rind];

            // Integrate across the fine time bin to get the element
            // of induced *charge* over this bin.
            const double induced_charge = induced_current*rawresp_tick;

            // sum up over coarse ticks.
            wave[bin] += induced_charge;
        }
        WireCell::Waveform::compseq_t spec = Waveform::dft(wave);

        // Convolve with other responses
        if (nother) {
            for (size_t find=0; find < m_nbins; ++find) {
                spec[find] *= other[find];
            }
        }

        IImpactResponse::pointer ir = std::make_shared<Gen::ImpactResponse>(ipath, spec);
        m_ir.push_back(ir);
    }

    // apply symmetry.
    for (int irelwire=-n_wires_half; irelwire <= n_wires_half; ++irelwire) {
        auto direct = wire_to_ind[irelwire];
        auto other = wire_to_ind[-irelwire];

        std::vector<int> indices(direct.begin(), direct.end());
        for (auto it = other.rbegin()+1; it != other.rend(); ++it) {
            indices.push_back(*it);
        }
        m_bywire.push_back(indices);
    }

}

Gen::PlaneImpactResponse::~PlaneImpactResponse()
{
}

// const Response::Schema::PlaneResponse& PlaneImpactResponse::plane_response() const
// {
//     return *m_fr.plane(m_plane_ident);
// }

std::pair<int,int> Gen::PlaneImpactResponse::closest_wire_impact(double relpitch) const
{
    const int center_wire = nwires()/2;
    
    const int relwire = int(round(relpitch/m_pitch));
    const int wire_index = center_wire + relwire;
    
    const double remainder_pitch = relpitch - relwire*m_pitch;
    const int impact_index = int(round(remainder_pitch / m_impact)) + nimp_per_wire()/2;

    //std::cerr << "relpitch:" << relpitch << ", relwire:"<<relwire<<", wi:" << wire_index
    //          << ", rempitch=" << remainder_pitch << ", impactind=" << impact_index
    //          <<std::endl;
    return std::make_pair(wire_index, impact_index);
}

IImpactResponse::pointer Gen::PlaneImpactResponse::closest(double relpitch) const
{
    if (relpitch < -m_half_extent || relpitch > m_half_extent) {
        return nullptr;
    }
    std::pair<int,int> wi = closest_wire_impact(relpitch);
    if (wi.first < 0 || wi.first >= (int)m_bywire.size()) {
        std::cerr << "PlaneImpactResponse::closest(): relative pitch: "
                  << relpitch
                  << " outside of wire range: " << wi.first
                  << std::endl;
        return nullptr;
    }
    const std::vector<int>& region = m_bywire[wi.first];
    if (wi.second < 0 || wi.second >= (int)region.size()) {
        std::cerr << "PlaneImpactResponse::closest(): relative pitch: "
                  << relpitch
                  << " outside of impact range: " << wi.second
                  << std::endl;
        return nullptr;
    }
    int irind = region[wi.second];
    if (irind < 0 || irind > (int)m_ir.size()) {
        std::cerr << "PlaneImpactResponse::closest(): relative pitch: "
                  << relpitch
                  << " no impact response for region: " << irind
                  << std::endl;
        return nullptr;
    }
    return m_ir[irind];
}

TwoImpactResponses Gen::PlaneImpactResponse::bounded(double relpitch) const
{
    if (relpitch < -m_half_extent || relpitch > m_half_extent) {
        return TwoImpactResponses(nullptr, nullptr);
    }

    std::pair<int,int> wi = closest_wire_impact(relpitch);

    auto region = m_bywire[wi.first];
    if (wi.second == 0) {
        return std::make_pair(m_ir[region[0]], m_ir[region[1]]);
    }
    if (wi.second == (int)region.size()-1) {
        return std::make_pair(m_ir[region[wi.second-1]], m_ir[region[wi.second]]);
    }

    const double absimpact = m_half_extent + relpitch - wi.first*m_pitch;
    const double sign = absimpact - wi.second*m_impact;

    if (sign > 0) {
        return TwoImpactResponses(m_ir[region[wi.second]], m_ir[region[wi.second+1]]);
    }
    return TwoImpactResponses(m_ir[region[wi.second-1]], m_ir[region[wi.second]]);
}




