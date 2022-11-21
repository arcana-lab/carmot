#pragma once
#include <boost/graph/adjacency_list.hpp>
#include <boost/unordered_map.hpp>
#include <boost/container/map.hpp>
#include <boost/unordered_set.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/directed_graph.hpp>
#include <boost/graph/connected_components.hpp>
#include <boost/graph/topological_sort.hpp>
#include <boost/graph/strong_components.hpp>
#include <boost/unordered/unordered_map_fwd.hpp>
#include <boost/unordered/unordered_set_fwd.hpp>
#include <boost/thread.hpp>
#include <boost/unordered/unordered_set_fwd.hpp>
#include "tbb/concurrent_hash_map.h"
#define TBB_PREVIEW_CONCURRENT_ORDERED_CONTAINERS 1
#include "tbb/concurrent_unordered_set.h"
#include "tbb//concurrent_unordered_map.h"

class Allocation;

struct Vertex { Allocation* entry; };
struct Edge {int weight;};

typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS, Vertex, Edge> graph_t;
typedef boost::graph_traits<graph_t>::vertex_descriptor vertex_t;
typedef boost::graph_traits<graph_t>::edge_descriptor edge_t;


