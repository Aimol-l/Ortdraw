#pragma once
#include <QVariantList>
#include <QVariantMap>
#include "utils/DAGraph.hpp"
#include "node/BaseNode.hpp"
#include "utils/Edge.hpp"

inline QVariantList nodeSnapshotsOf(DAGraph& graph) {
    QVariantList out;
    for(BaseNode* n : graph.getAllNodes()){
        QVariantMap m;
        m["uuid"]     = n->uuid().toString();
        m["x"]        = n->x();
        m["y"]        = n->y();
        m["w"]        = n->width();
        m["h"]        = n->height();
        m["category"] = n->category();
        m["selected"] = n->selected();
        out.append(m);
    }
    return out;
}
inline QVariantList edgeSnapshotsOf(DAGraph& graph) {
    QVariantList out;
    for(const Edge& e : graph.getAllEdges()){
        if(!e.start_port || !e.stop_port) continue;
        QVariantMap m;
        m["fromX"]    = e.start_port->position().x();
        m["fromY"]    = e.start_port->position().y();
        m["toX"]      = e.stop_port->position().x();
        m["toY"]      = e.stop_port->position().y();
        m["selected"] = e.seleected;
        out.append(m);
    }
    return out;
}
