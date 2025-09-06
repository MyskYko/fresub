#include "simulation.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <unordered_map>
#include "aig_utils.hpp"

namespace fresub {
  
  // Use lit helpers from aig_utils.hpp
  
  std::vector<std::vector<uint64_t>> compute_truth_tables_for_window(aigman const& aig, Window const& window, bool verbose) {

    static const unsigned long long basepats[] = {0xaaaaaaaaaaaaaaaaull,
	0xccccccccccccccccull,
	0xf0f0f0f0f0f0f0f0ull,
	0xff00ff00ff00ff00ull,
	0xffff0000ffff0000ull,
	0xffffffff00000000ull};

    if (verbose) {
      std::cout << "\n--- COMPUTING TRUTH TABLES FOR WINDOW ---\n";
      std::cout << "Target: " << window.target_node << "\n";
      std::cout << "Window inputs: [";
      for (size_t i = 0; i < window.inputs.size(); i++) {
	if (i > 0) std::cout << ", ";
	std::cout << window.inputs[i];
      }
      std::cout << "]\nWindow nodes: [";
      for (size_t i = 0; i < window.nodes.size(); i++) {
	if (i > 0) std::cout << ", ";
	std::cout << window.nodes[i];
      }
      std::cout << "]\nDivisors: [";
      for (size_t i = 0; i < window.divisors.size(); i++) {
	if (i > 0) std::cout << ", ";
	std::cout << window.divisors[i];
      }
      std::cout << "]\n";
    }
    
    int num_inputs = window.inputs.size();
    assert(num_inputs <= 20);
    int num_patterns = 1 << num_inputs;
    int num_words = (num_patterns + 63) / 64;
    if (verbose) {
      std::cout << "Truth table size: " << num_patterns << " patterns = " << num_words << " words of 64 bits\n";
    }
    
    std::unordered_map<int, std::vector<uint64_t>> node_tt;

    if (verbose) std::cout << "Initializing primary input truth tables:\n";
    for(int i = 0; i < num_inputs; i++) {
      int wi = window.inputs[i];
      node_tt[wi].resize(num_words);
      if(i < 6) {
	for(int j = 0; j < num_words; j++) {
	  node_tt[wi][j] = basepats[i];
	}
      } else {
	for(int j = 0; j < num_words; j++) {
	  node_tt[wi][j] = (j >> (i - 6)) & 1? 0xffffffffffffffffull: 0ull;
	}
      }
      if (verbose) {
	std::cout << "  Input " << wi << " (position " << i << "): ";
	if (num_patterns <= 64) {
	  for (int b = num_patterns - 1; b >= 0; b--) {
	    std::cout << ((node_tt[wi][0] >> b) & 1);
	  }
	  std::cout << " (0x" << std::hex << node_tt[wi][0] << std::dec << ")";
	} else {
	  std::cout << "[" << num_words << " words, " << num_patterns << " patterns]";
	}
	std::cout << "\n";
      }
    }
      
    if (verbose) std::cout << "\nProcessing window nodes:\n";
    for (int current_node : window.nodes) {
      if (std::find(window.inputs.begin(), window.inputs.end(), current_node) != window.inputs.end()) {
	continue; // Skip inputs, already processed
      }
      int fanin0 = lit2var(aig.vObjs[current_node * 2]);
      int fanin1 = lit2var(aig.vObjs[current_node * 2 + 1]);
      bool comp0 = is_complemented(aig.vObjs[current_node * 2]);
      bool comp1 = is_complemented(aig.vObjs[current_node * 2 + 1]);
      node_tt[current_node].resize(num_words);
      for (int w = 0; w < num_words; w++) {
	uint64_t val0 = comp0 ? ~node_tt[fanin0][w] : node_tt[fanin0][w];
	uint64_t val1 = comp1 ? ~node_tt[fanin1][w] : node_tt[fanin1][w];
	node_tt[current_node][w] = val0 & val1;
      }
      if (verbose) {
	std::cout << "  Node " << current_node << " = AND(";
	std::cout << fanin0 << (comp0 ? "'" : "") << ", ";
	std::cout << fanin1 << (comp1 ? "'" : "") << "):\n";
	if (num_patterns <= 64) {
	  std::cout << "    ";
	  for (int b = num_patterns - 1; b >= 0; b--) {
	    std::cout << ((node_tt[current_node][0] >> b) & 1);
	  }
	  std::cout << " (0x" << std::hex << node_tt[current_node][0] << std::dec << ")";
	} else {
	  std::cout << "    [" << num_words << " words computed]";
	}
	std::cout << "\n";
      }
    }
    
    // Extract results as vector<vector<word>>
    // results[0..n-1] = divisors[0..n-1], results[n] = target
    std::vector<std::vector<uint64_t>> results;
    for (int divisor : window.divisors) {
      assert(node_tt.find(divisor) != node_tt.end());
      results.push_back(std::move(node_tt[divisor]));
    }
    assert(node_tt.find(window.target_node) != node_tt.end());
    results.push_back(std::move(node_tt[window.target_node]));
    if (verbose) {
      std::cout << "\nExtracted truth tables as vector<vector<word>>:\n";
      for (size_t i = 0; i < window.divisors.size(); i++) {
	std::cout << "  results[" << i << "] = divisor " << window.divisors[i] 
		  << " (" << results[i].size() << " words)\n";
      }
      std::cout << "  results[" << window.divisors.size() << "] = target " << window.target_node 
		<< " (" << results[window.divisors.size()].size() << " words)\n";
      std::cout << "  Total: " << results.size() << " truth tables\n";
    }
    
    return results;
  }

} // namespace fresub
