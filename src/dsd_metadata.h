#pragma once
#include "io.h"
#include <map>
#include <set>
namespace ttp::dsd {
struct Tag {
  std::string key;
  std::wstring value;
};
struct Art {
  Bytes data;
  std::wstring mime, description;
  Picture view{};
};
// The editor never reopens a path. Unknown chunks and audio keep their offsets
// in DSDIFF, including offsets referenced by DSTI and private chunks.
class DsdMetadata {
  struct Slot {
    uint64_t at{}, bytes{};
    std::string key;
    bool id3{};
  };
  IStream *stream_{};
  uint64_t size_{}, tag_at_{}, tag_bytes_{};
  size_t inspected_bytes_{};
  bool dsf_{}, all_{}, pictures_{}, writable_{}, has_id3_{};
  BYTE version_{4};
  std::vector<Slot> slots_;
  std::vector<std::pair<uint64_t, Bytes>> checks_;
  std::vector<Bytes> frames_;
  std::vector<Tag> tags_;
  std::vector<Art> pictures_original_;
  std::map<std::string, std::wstring> changes_;
  std::set<std::string> removed_;
  Bytes make_id3(const std::vector<Art> &) const;
  void parse_id3(const Bytes &);
  void decode_frames();
  void scan(uint64_t begin, uint64_t end, unsigned depth);
  Bytes inspect(uint64_t at, size_t bytes);

public:
  uint64_t raw_start{}, raw_bytes{};
  uint32_t block_samples{4096};
  void Open(IStream *);
  void Set(const char *, const wchar_t *);
  void Pictures() {
    require(writable_, STG_E_ACCESSDENIED);
    pictures_ = true;
  }
  bool Writable() const noexcept { return writable_; }
  bool HasId3() const noexcept { return has_id3_; }
  bool Dirty() const noexcept {
    return all_ || pictures_ || !changes_.empty() || !removed_.empty();
  }
  const std::vector<Tag> &Tags() const noexcept { return tags_; }
  const std::vector<Art> &OriginalPictures() const noexcept {
    return pictures_original_;
  }
  void Save(const std::vector<Art> &);
};
void refresh_art(std::vector<Art> &);
Art copy_art(const Picture *);
std::string metadata_key(std::string);
} // namespace ttp::dsd
