#include "index_factory.h"
#include <faiss/IndexFlat.h>
#include <faiss/IndexIDMap.h>

namespace {
    IndexFactory globalIndexFactory; 
}

IndexFactory* getGlobalIndexFactory() {
    return &globalIndexFactory; 
}

void IndexFactory::init(IndexType type, int dim, MetricType metric) {
    faiss::MetricType faiss_metric = (metric == MetricType::L2) ? faiss::METRIC_L2 : faiss::METRIC_INNER_PRODUCT;

    switch (type) {
        case IndexType::FLAT:
            index_map[type] = new FaissIndex(new faiss::IndexIDMap(new faiss::IndexFlat(dim, faiss_metric)));
            break;
        default:
            break;
    }
}

void* IndexFactory::getIndex(IndexType type) const { 
    auto it = index_map.find(type);
    if (it != index_map.end()) {
        return it->second;
    }
    return nullptr;
}