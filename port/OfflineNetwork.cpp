#include "network/NetConfigManager.h"
#include "network/FileCompressor.h"
#include <cstdlib>
void NetConfigManager::load() {}
void NetConfigManager::save() {}
CompressedData::CompressedData(uint8_t* p, size_t n): data(p), size(n) {}
CompressedData::~CompressedData() { free(data); }
bool CompressedData::is_valid() { return false; }
CompressedData FileCompressor::compress(const std::string&, size_t) { return {nullptr,0}; }
bool FileCompressor::decompress(const uint8_t*, const std::string&) { return false; }
