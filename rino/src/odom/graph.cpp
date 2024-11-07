/**
 * Copyright 2020, Massachusetts Institute of Technology,
 * Cambridge, MA 02139
 * All Rights Reserved
 * Authors: Jingnan Shi, et al. (see THANKS for the full author list)
 * See LICENSE for the license information
 */

#include "odom/graph.hpp"
#include "pmc/pmc.h"


vector<int> MaxCliqueSolver::findMaxClique(Graph graph) {

    // Handle deprecated field
    if (!params_.solve_exactly) {
        params_.solver_mode = CLIQUE_SOLVER_MODE::PMC_HEU;
    }

    // Create a PMC graph from the TEASER graph
    vector<int>       edges;
    vector<long long> vertices;
    vertices.push_back(edges.size());

    const auto all_vertices = graph.getVertices();
    for (const auto &i: all_vertices) {
        const auto &c_edges = graph.getEdges(i);
        edges.insert(edges.end(), c_edges.begin(), c_edges.end());
        vertices.push_back(edges.size());
    }

    // Use PMC to calculate
    pmc::pmc_graph G(vertices, edges);

    // Prepare PMC input
    // TODO: Incorporate this to the constructor

    pmc::input in;
    in.algorithm           = 0;
    in.threads             = 12; // Originally, threads are 12
    in.experiment          = 0;
    in.lb                  = 0;
    in.ub                  = 0;
    in.param_ub            = 0;
    in.adj_limit           = 20000;
    in.time_limit          = params_.time_limit;
    in.remove_time         = 4;
    in.graph_stats         = false;
    in.verbose             = false;
    in.help                = false;
    in.MCE                 = false;
    in.decreasing_order    = false;
    in.heu_strat           = "kcore";
    in.vertex_search_order = "deg";

    // vector to represent max clique
    vector<int> C;

    // upper-bound of max clique
    G.compute_cores();
    auto max_core = G.get_max_core();

    std::cout << "\033[1;34mMax core number: \033[0m" << max_core << std::endl;
    std::cout << "\033[1;34mNum vertices: \033[0m" << vertices.size() << std::endl;

    // check for k-core heuristic threshold
    // check whether threshold equals 1 to short circuit the comparison
    if (params_.solver_mode == CLIQUE_SOLVER_MODE::KCORE_HEU &&
        params_.kcore_heuristic_threshold != 1 &&
        max_core > static_cast<int>(params_.kcore_heuristic_threshold *
                                    static_cast<double>(all_vertices.size()))) {
        std::cout << "\033[1;34mUsing K-core heuristic finder.\033[0m" << std::endl;
        // remove all nodes with core number less than max core number
        // k_cores is a vector saving the core number of each vertex
        auto     k_cores = G.get_kcores();
        for (int i       = 1; i < k_cores->size(); ++i) {
            // Note: k_core has size equals to num vertices + 1
            if ((*k_cores)[i] >= max_core) {
                C.push_back(i - 1);
            }
        }
        return C;
    }

    if (in.ub == 0) {
        in.ub = max_core + 1;
    }

    // lower-bound of max clique
    if (in.lb == 0 && in.heu_strat != "0") { // skip if given as input
        pmc::pmc_heu maxclique(G, in);
        in.lb = maxclique.search(G, C);
    }

    assert(in.lb != 0);
    if (in.lb == 0) {
        // This means that max clique has a size of one
        std::cout << "\033[1;31mMax clique lower bound equals to zero. Abort.Max clique lower bound equals to zero. Abort.\033[0m" << std::endl;
        return C;
    }

    if (in.lb == in.ub) {
        return C;
    }

    // Optional exact max clique finding
    if (params_.solver_mode == CLIQUE_SOLVER_MODE::PMC_EXACT) {
        std::cout << "\033[1;34mUsing exact finder.\033[0m" << std::endl;
        // The following methods are used:
        // 1. k-core pruning
        // 2. neigh-core pruning/ordering
        // 3. dynamic coloring bounds/sort
        // see the original PMC paper and implementation for details:
        // R. A. Rossi, D. F. Gleich, and A. H. Gebremedhin, “Parallel Maximum Clique Algorithms with
        // Applications to Network Analysis,” SIAM J. Sci. Comput., vol. 37, no. 5, pp. C589–C616, Jan.
        // 2015.
        if (G.num_vertices() < in.adj_limit) {
            std::cout << "\033[1;34mUsing exact search_dense finder.\033[0m" << std::endl;
            G.create_adj();
            pmc::pmcx_maxclique finder(G, in);
            // finder.search_dense(G, C);
            finder.search_dense(G, C);
        } 
        else {
            std::cout << "\033[1;34mUsing exact search finder.\033[0m" << std::endl;
            pmc::pmcx_maxclique finder(G, in);
            finder.search(G, C);
        }
    }

    return C;
}
