#include "dsd_metadata.h"
#include <cctype>
#include <shlwapi.h>
namespace ttp::dsd {
namespace {
constexpr size_t tag_limit = 64 * 1024 * 1024;
struct Mapping {
  const char *key;
  const char *frame;
  const char *info;
};
constexpr Mapping mappings[] = {
    {"title", "TIT2", "INAM"},       {"artist", "TPE1", "IART"},
    {"album", "TALB", "IPRD"},       {"albumartist", "TPE2", ""},
    {"tracknumber", "TRCK", "ITRK"}, {"date", "TDRC", "ICRD"},
    {"genre", "TCON", "IGNR"},       {"comment", "COMM", "ICMT"},
    {"composer", "TCOM", "IMUS"},    {"copyright", "TCOP", "ICOP"},
    {"lyrics", "USLT", ""},          {"discnumber", "TPOS", ""},
    {"encoder", "TENC", "ISFT"}};
std::string lower(std::string s) {
  for (auto &c : s)
    c = char(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
std::string canonical(std::string s) {
  s = lower(std::move(s));
  if (s == "year")
    s = "date";
  if (s == "album_artist" || s == "album artist")
    s = "albumartist";
  if (s == "track")
    s = "tracknumber";
  if (s == "disc")
    s = "discnumber";
  if (s == "lyric" || s.compare(0, 7, "lyrics-") == 0)
    s = "lyrics";
  return s;
}
uint32_t sync(const BYTE *p) {
  require(!((p[0] | p[1] | p[2] | p[3]) & 128));
  return (p[0] << 21) | (p[1] << 14) | (p[2] << 7) | p[3];
}
void setsync(Bytes &b, size_t at, uint32_t n) {
  require(n < 0x10000000);
  for (unsigned i = 0; i < 4; ++i)
    b[at + i] = BYTE(n >> ((3 - i) * 7) & 127);
}
void setle(Bytes &b, size_t at, uint32_t n) {
  for (unsigned i = 0; i < 4; ++i)
    b[at + i] = BYTE(n >> (i * 8));
}
void setle64(Bytes &b, size_t at, uint64_t n) {
  for (unsigned i = 0; i < 8; ++i)
    b[at + i] = BYTE(n >> (i * 8));
}
std::wstring string_data(const BYTE *p, size_t n, BYTE encoding) {
  if (encoding == 0) {
    std::wstring s;
    for (size_t i = 0; i < n && p[i]; ++i)
      s += wchar_t(p[i]);
    return s;
  }
  if (encoding == 3) {
    size_t end = 0;
    while (end < n && p[end])
      ++end;
    return wide(std::string(reinterpret_cast<const char *>(p), end));
  }
  require(encoding == 1 || encoding == 2, E_NOTIMPL);
  bool big = encoding == 2;
  if (n >= 2 && p[0] == 0xff && p[1] == 0xfe) {
    p += 2;
    n -= 2;
    big = false;
  } else if (n >= 2 && p[0] == 0xfe && p[1] == 0xff) {
    p += 2;
    n -= 2;
    big = true;
  }
  std::wstring s;
  for (size_t i = 0; i + 1 < n; i += 2) {
    auto c = big ? be16(p + i) : le16(p + i);
    if (!c)
      break;
    s += wchar_t(c);
  }
  return s;
}
std::string frame_key(const Bytes &b) {
  std::string id(reinterpret_cast<const char *>(b.data()), 4);
  for (const auto &m : mappings)
    if (id == m.frame || (id == "TYER" && std::string(m.key) == "date"))
      return m.key;
  if (id == "TXXX" && b.size() > 11 && b[8] == 0 && b[9] == 0)
    return canonical(utf8(string_data(b.data() + 11, b.size() - 11, b[10])));
  return {};
}
void append(Bytes &b, const void *p, size_t n) {
  require(n <= tag_limit && b.size() <= tag_limit - n);
  auto first = static_cast<const BYTE *>(p);
  b.insert(b.end(), first, first + n);
}
void string_bytes(Bytes &b, const std::wstring &s, BYTE version,
                  bool terminate) {
  if (version == 4) {
    const auto value = utf8(s);
    append(b, value.data(), value.size());
    if (terminate)
      b.push_back(0);
  } else {
    b.push_back(0xff);
    b.push_back(0xfe);
    for (auto c : s) {
      b.push_back(BYTE(c));
      b.push_back(BYTE(c >> 8));
    }
    if (terminate) {
      b.push_back(0);
      b.push_back(0);
    }
  }
  require(b.size() <= tag_limit);
}
void addframe(Bytes &b, const char *id, const Bytes &payload, BYTE version) {
  Bytes h(10);
  memcpy(h.data(), id, 4);
  if (version == 4)
    setsync(h, 4, DWORD(payload.size()));
  else
    set32(h, 4, DWORD(payload.size()));
  append(b, h.data(), h.size());
  append(b, payload.data(), payload.size());
}
class Temporary {
  wchar_t path_[MAX_PATH]{};
  bool keep_{};

public:
  ComPtr<IStream> stream;
  Temporary() {
    wchar_t dir[MAX_PATH]{};
    auto n = GetTempPathW(MAX_PATH, dir);
    require(n && n < MAX_PATH, HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND));
    require(GetTempFileNameW(dir, L"tda", 0, path_) != 0,
            HRESULT_FROM_WIN32(GetLastError()));
    auto hr = SHCreateStreamOnFileEx(
        path_, STGM_READWRITE | STGM_SHARE_EXCLUSIVE | STGM_CREATE,
        FILE_ATTRIBUTE_TEMPORARY, FALSE, nullptr, stream.put());
    if (FAILED(hr)) {
      DeleteFileW(path_);
      throw Failure{hr};
    }
  }
  void Preserve() {
    keep_ = true;
    OutputDebugStringW(L"ttp_dsd recovery copy: ");
    OutputDebugStringW(path_);
    OutputDebugStringW(L"\n");
  }
  ~Temporary() {
    stream.reset();
    if (!keep_)
      DeleteFileW(path_);
  }
};
void resize(IStream *s, uint64_t size) {
  ULARGE_INTEGER n{};
  n.QuadPart = size;
  check(s->SetSize(n));
}
void copy(IStream *source, uint64_t start, uint64_t length, IStream *target) {
  seek(source, start);
  Bytes buffer(65536);
  while (length) {
    auto n = DWORD(std::min<uint64_t>(length, buffer.size()));
    read_exact(source, buffer.data(), n);
    write_exact(target, buffer.data(), n);
    length -= n;
  }
}
void chunk(IStream *s, const char *id, const Bytes &b) {
  Bytes h(8);
  memcpy(h.data(), id, 4);
  setle(h, 4, DWORD(b.size()));
  write_exact(s, h.data(), 8);
  if (!b.empty())
    write_exact(s, b.data(), DWORD(b.size()));
  if (b.size() & 1) {
    BYTE z = 0;
    write_exact(s, &z, 1);
  }
}
} // namespace
std::string metadata_key(std::string key) { return canonical(std::move(key)); }
void refresh_art(std::vector<Art> &art) {
  for (auto &p : art) {
    auto type = p.view.type;
    p.view = {sizeof(Picture),      p.mime.c_str(), p.description.c_str(),
              DWORD(p.data.size()), p.data.data(),  type};
  }
}
Art copy_art(const Picture *p) {
  require(p && p->size == sizeof(Picture), E_INVALIDARG);
  require(p->data && p->bytes && p->bytes <= 16 * 1024 * 1024 && p->type <= 20,
          E_INVALIDARG);
  require(p->mime, E_INVALIDARG);
  const std::wstring mime = p->mime;
  require(mime == L"image/png" || mime == L"image/jpeg" ||
              mime == L"image/webp",
          E_INVALIDARG);
  Art a{Bytes(p->data, p->data + p->bytes),
        mime,
        p->description ? p->description : L"",
        {}};
  a.view.type = p->type;
  require(a.description.size() <= 65536, E_INVALIDARG);
  return a;
}
void DsdMetadata::Set(const char *key, const wchar_t *value) {
  require(stream_, E_UNEXPECTED);
  require(writable_, STG_E_ACCESSDENIED);
  if (!key) {
    require(!value, E_INVALIDARG);
    all_ = true;
    changes_.clear();
    removed_.clear();
    return;
  }
  auto k = canonical(key);
  require(!k.empty() && k.size() <= 1024, E_INVALIDARG);
  for (auto c : k)
    require(static_cast<unsigned char>(c) >= 32, E_INVALIDARG);
  if (!value) {
    changes_.erase(k);
    removed_.insert(k);
  } else {
    require(wcslen(value) <= 1024 * 1024, E_INVALIDARG);
    changes_[k] = value;
    removed_.erase(k);
  }
}
Bytes DsdMetadata::make_id3(const std::vector<Art> &art) const {
  Bytes b(10);
  memcpy(b.data(), "ID3", 3);
  b[3] = version_;
  for (const auto &f : frames_) {
    const auto k = frame_key(f);
    if ((!k.empty() && (all_ || changes_.count(k) || removed_.count(k))) ||
        (pictures_ && !memcmp(f.data(), "APIC", 4)))
      continue;
    append(b, f.data(), f.size());
  }
  for (const auto &change : changes_) {
    const char *id = "TXXX";
    for (const auto &m : mappings)
      if (change.first == m.key)
        id = m.frame;
    if (version_ == 3 && !strcmp(id, "TDRC"))
      id = "TYER";
    Bytes p{BYTE(version_ == 4 ? 3 : 1)};
    if (!strcmp(id, "TXXX"))
      string_bytes(p, wide(change.first), version_, true);
    if (!strcmp(id, "COMM") || !strcmp(id, "USLT")) {
      append(p, "eng", 3);
      string_bytes(p, L"", version_, true);
    }
    string_bytes(p, change.second, version_, false);
    addframe(b, id, p, version_);
  }
  if (pictures_)
    for (const auto &artItem : art) {
      Bytes p{BYTE(version_ == 4 ? 3 : 1)};
      const auto mime = utf8(artItem.mime);
      append(p, mime.data(), mime.size());
      p.push_back(0);
      p.push_back(BYTE(artItem.view.type));
      string_bytes(p, artItem.description, version_, true);
      append(p, artItem.data.data(), artItem.data.size());
      addframe(b, "APIC", p, version_);
    }
  setsync(b, 6, DWORD(b.size() - 10));
  return b;
}
Bytes DsdMetadata::inspect(uint64_t at, size_t bytes) {
  require(at <= size_ && bytes <= size_ - at);
  require(bytes <= tag_limit && inspected_bytes_ <= 2 * tag_limit - bytes,
          E_NOTIMPL);
  auto b = read_at(stream_, at, bytes);
  inspected_bytes_ += bytes;
  checks_.push_back({at, b});
  return b;
}
void DsdMetadata::parse_id3(const Bytes &b) {
  require(b.size() >= 10 && b.size() <= tag_limit);
  require(!memcmp(b.data(), "ID3", 3) && (b[3] == 3 || b[3] == 4) && !b[4] &&
              !b[5],
          E_NOTIMPL);
  require(!has_id3_ || version_ == b[3], E_NOTIMPL);
  version_ = b[3];
  has_id3_ = true;
  auto length = sync(b.data() + 6);
  require(length <= b.size() - 10);
  size_t at = 10, end = 10 + length, total = 0;
  for (const auto &f : frames_)
    total += f.size();
  while (at < end && b[at]) {
    require(end - at >= 10);
    for (unsigned i = 0; i < 4; ++i)
      require((b[at + i] >= 'A' && b[at + i] <= 'Z') ||
              (b[at + i] >= '0' && b[at + i] <= '9'));
    const auto n =
        version_ == 4 ? sync(b.data() + at + 4) : be32(b.data() + at + 4);
    require(n <= end - at - 10 && total <= tag_limit - 10 &&
            n <= tag_limit - total - 10);
    // Compressed/encrypted/unsynchronised frames require a separate editor.
    // Keep playback's FFmpeg read fallback, but never silently rewrite them.
    require(!b[at + 8] && !b[at + 9], E_NOTIMPL);
    frames_.emplace_back(b.begin() + at, b.begin() + at + 10 + n);
    total += 10 + n;
    at += 10 + n;
  }
  for (; at < end; ++at)
    require(!b[at]);
}
void DsdMetadata::scan(uint64_t at, uint64_t end, unsigned depth) {
  require(depth <= 2);
  while (at < end) {
    require(checks_.size() < 65536 && end - at >= 12);
    const auto h = inspect(at, 12);
    const auto n = be64(h.data() + 4);
    require(n <= end - at - 12 && (n & 1) <= end - at - 12 - n,
            STG_E_READFAULT);
    const auto id = be32(h.data());
    if (id == fourcc('I', 'D', '3', ' ')) {
      require(n <= tag_limit);
      slots_.push_back({at, n, {}, true});
      parse_id3(inspect(at + 12, size_t(n)));
    } else if (id == fourcc('P', 'R', 'O', 'P')) {
      require(n >= 4);
      const auto kind = inspect(at + 12, 4);
      if (!memcmp(kind.data(), "SND ", 4))
        scan(at + 16, at + 12 + n, depth + 1);
    } else if (id == fourcc('D', 'I', 'I', 'N')) {
      scan(at + 12, at + 12 + n, depth + 1);
    } else if (depth && (id == fourcc('D', 'I', 'A', 'R') ||
                         id == fourcc('D', 'I', 'T', 'I'))) {
      require(n >= 4 && n <= tag_limit);
      const auto b = inspect(at + 12, size_t(n));
      const auto length = be32(b.data());
      require(length <= b.size() - 4);
      const std::string key =
          id == fourcc('D', 'I', 'A', 'R') ? "artist" : "title";
      tags_.push_back(
          {key, wide(std::string(reinterpret_cast<const char *>(b.data() + 4),
                                 length))});
      slots_.push_back({at, n, key, false});
    } else if (!depth && id == fourcc('D', 'S', 'D', ' ')) {
      require(!raw_start, E_NOTIMPL);
      raw_start = at + 12;
      raw_bytes = n;
    }
    at += 12 + n + (n & 1);
  }
}
void DsdMetadata::Open(IStream *s) {
  stream_ = s;
  STATSTG st{};
  check(s->Stat(&st, STATFLAG_NONAME));
  size_ = st.cbSize.QuadPart;
  writable_ = (st.grfMode & 3) == STGM_READWRITE;
  const auto h = inspect(0, 28);
  dsf_ = !memcmp(h.data(), "DSD ", 4);
  if (dsf_) {
    const auto little = [](const BYTE *p) {
      return uint64_t(le32(p)) | uint64_t(le32(p + 4)) << 32;
    };
    require(little(h.data() + 4) == 28 && little(h.data() + 12) == size_,
            E_NOTIMPL);
    auto fmt = inspect(28, 64);
    require(!memcmp(fmt.data(), "fmt ", 4) && little(fmt.data() + 4) == 52 &&
            !memcmp(fmt.data() + 52, "data", 4));
    block_samples = le32(fmt.data() + 44);
    raw_start = 92;
    const auto data_size = little(fmt.data() + 56);
    require(data_size >= 12 && data_size - 12 <= size_ - raw_start,
            STG_E_READFAULT);
    raw_bytes = data_size - 12;
    tag_at_ = little(h.data() + 20);
    if (tag_at_) {
      require(tag_at_ >= raw_start + raw_bytes && tag_at_ <= size_ &&
              size_ - tag_at_ >= 10);
      auto tag = inspect(tag_at_, 10);
      tag_bytes_ = 10ULL + sync(tag.data() + 6);
      require(tag_bytes_ <= tag_limit && tag_bytes_ <= size_ - tag_at_);
      parse_id3(inspect(tag_at_, size_t(tag_bytes_)));
    }
  } else {
    require(!memcmp(h.data(), "FRM8", 4) && !memcmp(h.data() + 12, "DSD ", 4),
            E_NOTIMPL);
    require(be64(h.data() + 4) == size_ - 12, E_NOTIMPL);
    scan(16, size_, 0);
  }
  decode_frames();
}
void DsdMetadata::decode_frames() {
  // Native ID3 takes precedence over the optional legacy DIIN text.
  std::vector<Tag> native;
  for (const auto &f : frames_) {
    if (f.size() <= 10)
      continue;
    const BYTE enc = f[10];
    auto p = size_t(11);
    const auto terminated = [&](size_t &pos) {
      const size_t begin = pos, width = (enc == 1 || enc == 2) ? 2 : 1;
      while (pos + width <= f.size() && (f[pos] || (width == 2 && f[pos + 1])))
        pos += width;
      require(pos + width <= f.size());
      auto s = string_data(f.data() + begin, pos - begin, enc);
      pos += width;
      return s;
    };
    if (!memcmp(f.data(), "APIC", 4)) {
      while (p < f.size() && f[p])
        ++p;
      require(p + 2 < f.size() && p - 11 <= 128);
      const auto mime = wide(
          std::string(reinterpret_cast<const char *>(f.data() + 11), p - 11));
      ++p;
      const auto type = f[p++];
      auto description = terminated(p);
      require(type <= 20 && p < f.size() && f.size() - p <= 16 * 1024 * 1024 &&
              pictures_original_.size() < 32);
      Art a{Bytes(f.begin() + p, f.end()), mime, std::move(description), {}};
      a.view.type = type;
      pictures_original_.push_back(std::move(a));
      continue;
    }
    auto key = frame_key(f);
    if (key.empty())
      continue;
    if (!memcmp(f.data(), "TXXX", 4))
      terminated(p);
    else if (!memcmp(f.data(), "COMM", 4) || !memcmp(f.data(), "USLT", 4)) {
      require(f.size() - p >= 3);
      p += 3;
      terminated(p);
    }
    if (version_ == 4 && f[0] == 'T') {
      // The old ABI enumerates repeated names. Do not discard ID3v2.4 values
      // after the first NUL; untouched frames remain byte-for-byte preserved.
      const size_t width = (enc == 1 || enc == 2) ? 2 : 1;
      do {
        const size_t begin = p;
        while (p + width <= f.size() && (f[p] || (width == 2 && f[p + 1])))
          p += width;
        native.push_back({key, string_data(f.data() + begin, p - begin, enc)});
        p += width;
      } while (p < f.size());
    } else {
      auto value = string_data(f.data() + p, f.size() - p, enc);
      native.push_back({key, std::move(value)});
    }
  }
  for (auto &t : tags_)
    if (std::none_of(native.begin(), native.end(),
                     [&](const Tag &n) { return n.key == t.key; }))
      native.push_back(std::move(t));
  tags_ = std::move(native);
  refresh_art(pictures_original_);
}
void DsdMetadata::Save(const std::vector<Art> &art) {
  if (!Dirty())
    return;
  require(writable_, STG_E_ACCESSDENIED);
  require(stream_size(stream_) == size_, STG_E_REVERTED);
  for (const auto &v : checks_)
    require(read_at(stream_, v.first, v.second.size()) == v.second,
            STG_E_REVERTED);
  const auto id3 = make_id3(art);
  Temporary staged, backup;
  auto *target = staged.stream.p;
  uint64_t length{};
  if (dsf_) {
    const auto at = tag_at_ ? tag_at_ : size_;
    copy(stream_, 0, at, target);
    const bool present = id3.size() > 10;
    if (present)
      write_exact(target, id3.data(), DWORD(id3.size()));
    if (tag_at_)
      copy(stream_, tag_at_ + tag_bytes_, size_ - tag_at_ - tag_bytes_, target);
    length = tell(target);
    Bytes h(16);
    setle64(h, 0, length);
    setle64(h, 8, present ? at : 0);
    seek(target, 12);
    write_exact(target, h.data(), DWORD(h.size()));
  } else {
    copy(stream_, 0, size_, target);
    const Slot *chosen = nullptr;
    if (id3.size() > 10)
      for (const auto &slot : slots_)
        if (slot.id3 && slot.bytes >= id3.size()) {
          chosen = &slot;
          break;
        }
    Bytes zero(65536);
    for (const auto &slot : slots_) {
      if (!slot.id3 && !all_ && !changes_.count(slot.key) &&
          !removed_.count(slot.key))
        continue;
      seek(target, slot.at);
      write_exact(target, &slot == chosen ? "ID3 " : "JUNK", 4);
      seek(target, slot.at + 12);
      uint64_t remaining = slot.bytes;
      if (&slot == chosen) {
        write_exact(target, id3.data(), DWORD(id3.size()));
        remaining -= id3.size();
      }
      while (remaining) {
        auto n = DWORD(std::min<uint64_t>(remaining, zero.size()));
        write_exact(target, zero.data(), n);
        remaining -= n;
      }
    }
    length = size_;
    if (!chosen && id3.size() > 10) {
      seek(target, size_);
      Bytes h(12);
      memcpy(h.data(), "ID3 ", 4);
      for (unsigned i = 0; i < 8; ++i)
        h[4 + i] = BYTE(uint64_t(id3.size()) >> ((7 - i) * 8));
      write_exact(target, h.data(), 12);
      write_exact(target, id3.data(), DWORD(id3.size()));
      if (id3.size() & 1)
        write_exact(target, zero.data(), 1);
      length = tell(target);
    }
    Bytes h(8);
    for (unsigned i = 0; i < 8; ++i)
      h[i] = BYTE((length - 12) >> ((7 - i) * 8));
    seek(target, 4);
    write_exact(target, h.data(), 8);
  }
  copy(stream_, 0, size_, backup.stream.p);
  commit_stream(backup.stream.p);
  commit_stream(target);
  const auto result = protect([&]() {
    seek(stream_, 0);
    copy(target, 0, length, stream_);
    resize(stream_, length);
    commit_stream(stream_);
    return S_OK;
  });
  if (FAILED(result)) {
    auto restored = protect([&]() {
      seek(stream_, 0);
      copy(backup.stream.p, 0, size_, stream_);
      resize(stream_, size_);
      commit_stream(stream_);
      return S_OK;
    });
    if (FAILED(restored))
      backup.Preserve();
    check(restored);
    throw Failure{result};
  }
}
} // namespace ttp::dsd
