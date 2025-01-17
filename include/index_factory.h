#pragma once

#include "faiss_index.h"
#include <map>

class IndexFactory {
public:
    enum class IndexType {
        FLAT,
        UNKNOWN = -1 
    };

    enum class MetricType {
        L2,
        IP
    };

    void init(IndexType type, int dim, MetricType metric = MetricType::L2);
    void* getIndex(IndexType type) const;

private:
    std::map<IndexType, void*> index_map; 
};

IndexFactory* getGlobalIndexFactory();