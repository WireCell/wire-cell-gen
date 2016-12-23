#include "WireCellGen/ImpactData.h"

#include <iostream>             // debugging

using namespace WireCell;
using namespace std;

Gen::ImpactData::ImpactData(int impact)
    : m_impact(impact)
{
}
void Gen::ImpactData::add(GaussianDiffusion::pointer diffusion)
{
    m_diffusions.push_back(diffusion);
}

Waveform::realseq_t& Gen::ImpactData::waveform() const
{
    return m_waveform;
}

Waveform::compseq_t& Gen::ImpactData::spectrum() const
{
    return m_spectrum;
}

void Gen::ImpactData::calculate(int nticks) const
{
    if (m_waveform.size() > 0) {
        return;
    }
    m_waveform.resize(nticks, 0.0);

    for (auto diff : m_diffusions) {

	const auto patch = diff->patch();

        const int poffset_bin = diff->poffset_bin();
        const int pbin = m_impact - poffset_bin;
        const int np = patch.rows();
        if (pbin<0 || pbin >= np) {
            continue;
        }

        const int toffset_bin = diff->toffset_bin();
        const int nt = patch.cols();

        for (int tbin=0; tbin<nt; ++tbin) {
            const int absbin = tbin+toffset_bin;
            m_waveform[absbin] += patch(pbin, tbin);
        }
    }

    m_spectrum = Waveform::dft(m_waveform);
}



// std::pair<int,int> Gen::ImpactData::strip() const
// {
//     int imin=-1, imax = -1;
//     for (int ind=0; ind<m_waveform.size(); ++ind) {
//         const double val = m_waveform[ind];
//         if (imin < 0 && val > 0) { imin = ind; }
//         if (val > 0) { imax = ind; }
//     }
//     return std::make_pair(imin, imax+1);
// }

std::pair<double, double> Gen::ImpactData::span(double nsigma) const
{
    int ncount = -1;
    double tmin=0, tmax=0;
    for (auto diff : m_diffusions) {
        ++ncount;
        
        auto td = diff->time_desc();

        const double ltmin = td.center - td.sigma*nsigma;
        const double ltmax = td.center + td.sigma*nsigma;
        if (!ncount) {
            tmin = ltmin;
            tmax = ltmax;
            continue;
        }
        tmin = std::min(tmin, ltmin);
        tmax = std::max(tmax, ltmax);
    }        
    return std::make_pair(tmin,tmax);
}