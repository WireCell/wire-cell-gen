#ifndef WIRECELLNAV_PARAMWIRES_H
#define WIRECELLNAV_PARAMWIRES_H

#include "WireCellIface/IWireProvider.h"
#include "WireCellIface/IWireGenerator.h"

#include <vector>

namespace WireCell {

    // internal
    class ParamWire;

    /** A provider of wire (segment) geometry as generated from parameters.
     *
     * All wires in one plane are constructed to be parallel to
     * one-another and to be equally spaced between neighbors and
     * perpendicular to the drift direction.
     */

    class ParamWires :
	public IWireGenerator,
	public IWireProvider {
    public:
	ParamWires();
	virtual ~ParamWires();

	void generate(const WireCell::IWireParameters& params);

	/// Access the wires
	wire_iterator wires_begin();
	wire_iterator wires_end();

    private:

	void clear();

	void make_one_plane(WirePlaneType_t plane, const Ray& bounds, const Ray& step);


	typedef std::vector<ParamWire*> ParamWireStore;
	ParamWireStore m_wire_store;

    };


}

#endif