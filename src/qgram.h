#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <iostream>

template <int Q> class QGramIndex {
public:
  QGramIndex() {}

  void addMapping(const std::string &s, uint64_t id) {
    std::string key = std::string(Q - 1, '$') + s + std::string(Q - 1, '$');
    std::transform(key.begin(), key.end(), key.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    for (size_t i = 0; i < key.size() - Q + 1; i++) {
      std::string gram = key.substr(i, Q);
      _document_map[gram].push_back(id);
    }
  }

  std::vector<uint64_t> query(const std::string &s, size_t limit) const {
    std::unordered_map<uint64_t, size_t> counts;
    std::string key = std::string(Q - 1, '$') + s + std::string(Q - 1, '$');
    std::transform(key.begin(), key.end(), key.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    for (size_t i = 0; i < key.size() - Q + 1; i++) {
      std::string gram = key.substr(i, Q);

      std::unordered_map<std::string, std::vector<uint64_t>>::const_iterator
          it = _document_map.find(gram);
      if (it != _document_map.end()) {
        for (uint64_t id : it->second) {
          counts[id]++;
        }
      }
    }
    std::vector<std::pair<uint64_t, size_t>> sorted_counts;
    sorted_counts.reserve(counts.size());
    for (const std::pair<const uint64_t, size_t> &p : counts) {
      sorted_counts.push_back(p);
    }
    std::sort(sorted_counts.begin(), sorted_counts.end(),
              [](const std::pair<uint64_t, size_t> &p1,
                 const std::pair<uint64_t, size_t> &p2) {
                return p1.second > p2.second;
              });
    std::vector<uint64_t> ranked_ids;
    ranked_ids.reserve(std::min(limit, sorted_counts.size()));
    for (size_t i = 0; i < limit && i < sorted_counts.size(); i++) {
      ranked_ids.push_back(sorted_counts[i].first);
    }
    return ranked_ids;
  }

private:
  std::unordered_map<std::string, std::vector<uint64_t>> _document_map;
};
