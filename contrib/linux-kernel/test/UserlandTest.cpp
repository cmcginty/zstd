extern "C" {
#include <zstd.h>
}
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <iostream>

using namespace std;

namespace {
struct WorkspaceDeleter {
  void *memory;

  template <typename T> void operator()(T const *) { free(memory); }
};

std::unique_ptr<ZSTD_CCtx, WorkspaceDeleter>
createCCtx(ZSTD_compressionParameters cParams) {
  size_t const workspaceSize = ZSTD_CCtxWorkspaceBound(cParams);
  void *workspace = malloc(workspaceSize);
  std::unique_ptr<ZSTD_CCtx, WorkspaceDeleter> cctx{
      ZSTD_createCCtx(workspace, workspaceSize), WorkspaceDeleter{workspace}};
  if (!cctx) {
    throw std::runtime_error{"Bad cctx"};
  }
  return cctx;
}

std::unique_ptr<ZSTD_CCtx, WorkspaceDeleter>
createCCtx(int level, unsigned long long estimatedSrcSize = 0,
           size_t dictSize = 0) {
  auto const cParams = ZSTD_getCParams(level, estimatedSrcSize, dictSize);
  return createCCtx(cParams);
}

std::unique_ptr<ZSTD_DCtx, WorkspaceDeleter>
createDCtx() {
  size_t const workspaceSize = ZSTD_DCtxWorkspaceBound();
  void *workspace = malloc(workspaceSize);
  std::unique_ptr<ZSTD_DCtx, WorkspaceDeleter> dctx{
      ZSTD_createDCtx(workspace, workspaceSize), WorkspaceDeleter{workspace}};
  if (!dctx) {
    throw std::runtime_error{"Bad dctx"};
  }
  return dctx;
}

std::unique_ptr<ZSTD_CDict, WorkspaceDeleter>
createCDict(std::string const& dict, ZSTD_parameters params) {
  size_t const workspaceSize = ZSTD_CDictWorkspaceBound(params.cParams);
  void *workspace = malloc(workspaceSize);
  std::unique_ptr<ZSTD_CDict, WorkspaceDeleter> cdict{
      ZSTD_createCDict(dict.data(), dict.size(), params, workspace,
                       workspaceSize),
      WorkspaceDeleter{workspace}};
  if (!cdict) {
    throw std::runtime_error{"Bad cdict"};
  }
  return cdict;
}

std::unique_ptr<ZSTD_CDict, WorkspaceDeleter>
createCDict(std::string const& dict, int level) {
  auto const params = ZSTD_getParams(level, 0, dict.size());
  return createCDict(dict, params);
}

std::unique_ptr<ZSTD_DDict, WorkspaceDeleter>
createDDict(std::string const& dict) {
  size_t const workspaceSize = ZSTD_DDictWorkspaceBound();
  void *workspace = malloc(workspaceSize);
  std::unique_ptr<ZSTD_DDict, WorkspaceDeleter> ddict{
      ZSTD_createDDict(dict.data(), dict.size(), workspace, workspaceSize),
      WorkspaceDeleter{workspace}};
  if (!ddict) {
    throw std::runtime_error{"Bad ddict"};
  }
  return ddict;
}

std::unique_ptr<ZSTD_CStream, WorkspaceDeleter>
createCStream(ZSTD_parameters params, unsigned long long pledgedSrcSize = 0) {
  size_t const workspaceSize = ZSTD_CStreamWorkspaceBound(params.cParams);
  void *workspace = malloc(workspaceSize);
  std::unique_ptr<ZSTD_CStream, WorkspaceDeleter> zcs{
      ZSTD_createCStream(params, pledgedSrcSize, workspace, workspaceSize)};
  if (!zcs) {
    throw std::runtime_error{"bad cstream"};
  }
  return zcs;
}

std::unique_ptr<ZSTD_CStream, WorkspaceDeleter>
createCStream(ZSTD_compressionParameters cParams, ZSTD_CDict const &cdict,
              unsigned long long pledgedSrcSize = 0) {
  size_t const workspaceSize = ZSTD_CStreamWorkspaceBound(cParams);
  void *workspace = malloc(workspaceSize);
  std::unique_ptr<ZSTD_CStream, WorkspaceDeleter> zcs{
      ZSTD_createCStream_usingCDict(&cdict, pledgedSrcSize, workspace,
                                    workspaceSize)};
  if (!zcs) {
    throw std::runtime_error{"bad cstream"};
  }
  return zcs;
}

std::unique_ptr<ZSTD_CStream, WorkspaceDeleter>
createCStream(int level, unsigned long long pledgedSrcSize = 0) {
  auto const params = ZSTD_getParams(level, pledgedSrcSize, 0);
  return createCStream(params, pledgedSrcSize);
}

std::unique_ptr<ZSTD_DStream, WorkspaceDeleter>
createDStream(size_t maxWindowSize = (1ULL << ZSTD_WINDOWLOG_MAX),
              ZSTD_DDict const *ddict = nullptr) {
  size_t const workspaceSize = ZSTD_DStreamWorkspaceBound(maxWindowSize);
  void *workspace = malloc(workspaceSize);
  std::unique_ptr<ZSTD_DStream, WorkspaceDeleter> zds{
      ddict == nullptr
          ? ZSTD_createDStream(maxWindowSize, workspace, workspaceSize)
          : ZSTD_createDStream_usingDDict(maxWindowSize, ddict, workspace,
                                          workspaceSize)};
  if (!zds) {
    throw std::runtime_error{"bad dstream"};
  }
  return zds;
}

std::string compress(ZSTD_CCtx &cctx, std::string const &data,
                     ZSTD_parameters params, std::string const &dict = "") {
  std::string compressed;
  compressed.resize(ZSTD_compressBound(data.size()));
  size_t const rc =
      dict.empty()
          ? ZSTD_compressCCtx(&cctx, &compressed[0], compressed.size(),
                              data.data(), data.size(), params)
          : ZSTD_compress_usingDict(&cctx, &compressed[0], compressed.size(),
                                    data.data(), data.size(), dict.data(),
                                    dict.size(), params);
  if (ZSTD_isError(rc)) {
    throw std::runtime_error{"compression error"};
  }
  compressed.resize(rc);
  return compressed;
}

std::string compress(ZSTD_CCtx& cctx, std::string const& data, int level, std::string const& dict = "") {
  auto const params = ZSTD_getParams(level, 0, dict.size());
  return compress(cctx, data, params, dict);
}

std::string decompress(ZSTD_DCtx& dctx, std::string const& compressed, size_t decompressedSize, std::string const& dict = "") {
  std::string decompressed;
  decompressed.resize(decompressedSize);
  size_t const rc =
      dict.empty()
          ? ZSTD_decompressDCtx(&dctx, &decompressed[0], decompressed.size(),
                                compressed.data(), compressed.size())
          : ZSTD_decompress_usingDict(
                &dctx, &decompressed[0], decompressed.size(), compressed.data(),
                compressed.size(), dict.data(), dict.size());
  if (ZSTD_isError(rc)) {
    throw std::runtime_error{"decompression error"};
  }
  decompressed.resize(rc);
  return decompressed;
}

std::string compress(ZSTD_CCtx& cctx, std::string const& data, ZSTD_CDict& cdict) {
  std::string compressed;
  compressed.resize(ZSTD_compressBound(data.size()));
  size_t const rc =
      ZSTD_compress_usingCDict(&cctx, &compressed[0], compressed.size(),
                               data.data(), data.size(), &cdict);
  if (ZSTD_isError(rc)) {
    throw std::runtime_error{"compression error"};
  }
  compressed.resize(rc);
  return compressed;
}

std::string decompress(ZSTD_DCtx& dctx, std::string const& compressed, size_t decompressedSize, ZSTD_DDict& ddict) {
  std::string decompressed;
  decompressed.resize(decompressedSize);
  size_t const rc =
      ZSTD_decompress_usingDDict(&dctx, &decompressed[0], decompressed.size(),
                                 compressed.data(), compressed.size(), &ddict);
  if (ZSTD_isError(rc)) {
    throw std::runtime_error{"decompression error"};
  }
  decompressed.resize(rc);
  return decompressed;
}

std::string compress(ZSTD_CStream& zcs, std::string const& data) {
  std::string compressed;
  compressed.resize(ZSTD_compressBound(data.size()));
  ZSTD_inBuffer in = {data.data(), data.size(), 0};
  ZSTD_outBuffer out = {&compressed[0], compressed.size(), 0};
  while (in.pos != in.size) {
    size_t const rc = ZSTD_compressStream(&zcs, &out, &in);
    if (ZSTD_isError(rc)) {
      throw std::runtime_error{"compress stream failed"};
    }
  }
  size_t const rc = ZSTD_endStream(&zcs, &out);
  if (rc != 0) {
    throw std::runtime_error{"compress end failed"};
  }
  compressed.resize(out.pos);
  return compressed;
}

std::string decompress(ZSTD_DStream &zds, std::string const &compressed,
                       size_t decompressedSize) {
  std::string decompressed;
  decompressed.resize(decompressedSize);
  ZSTD_inBuffer in = {compressed.data(), compressed.size(), 0};
  ZSTD_outBuffer out = {&decompressed[0], decompressed.size(), 0};
  while (in.pos != in.size) {
    size_t const rc = ZSTD_decompressStream(&zds, &out, &in);
    if (ZSTD_isError(rc)) {
      throw std::runtime_error{"decompress stream failed"};
    }
  }
  decompressed.resize(out.pos);
  return decompressed;
}

std::string makeData(size_t size) {
  std::string result;
  result.reserve(size + 20);
  while (result.size() < size) {
    result += "Hello world";
  }
  return result;
}

std::string const kData = "Hello world";
std::string const kPlainDict = makeData(10000);
std::string const kZstdDict{
    "\x37\xA4\x30\xEC\x99\x69\x58\x1C\x21\x10\xD8\x4A\x84\x01\xCC\xF3"
    "\x3C\xCF\x9B\x25\xBB\xC9\x6E\xB2\x9B\xEC\x26\xAD\xCF\xDF\x4E\xCD"
    "\xF3\x2C\x3A\x21\x84\x10\x42\x08\x21\x01\x33\xF1\x78\x3C\x1E\x8F"
    "\xC7\xE3\xF1\x78\x3C\xCF\xF3\xBC\xF7\xD4\x42\x41\x41\x41\x41\x41"
    "\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41"
    "\x41\x41\x41\x41\xA1\x50\x28\x14\x0A\x85\x42\xA1\x50\x28\x14\x0A"
    "\x85\xA2\x28\x8A\xA2\x28\x4A\x29\x7D\x74\xE1\xE1\xE1\xE1\xE1\xE1"
    "\xE1\xE1\xE1\xE1\xE1\xE1\xE1\xE1\xE1\xE1\xE1\xE1\xE1\xF1\x78\x3C"
    "\x1E\x8F\xC7\xE3\xF1\x78\x9E\xE7\x79\xEF\x01\x01\x00\x00\x00\x04"
    "\x00\x00\x00\x08\x00\x00\x00"
    "0123456789",
    161};
}

TEST(Block, CCtx) {
  auto cctx = createCCtx(1);
  auto const compressed = compress(*cctx, kData, 1);
  auto dctx = createDCtx();
  auto const decompressed = decompress(*dctx, compressed, kData.size());
  EXPECT_EQ(kData, decompressed);
}

TEST(Block, NoContentSize) {
  auto cctx = createCCtx(1);
  auto const c = compress(*cctx, kData, 1);
  auto const size = ZSTD_findDecompressedSize(c.data(), c.size());
  EXPECT_EQ(ZSTD_CONTENTSIZE_UNKNOWN, size);
}

TEST(Block, ContentSize) {
  auto cctx = createCCtx(1);
  auto params = ZSTD_getParams(1, 0, 0);
  params.fParams.contentSizeFlag = 1;
  auto const c = compress(*cctx, kData, params);
  auto const size = ZSTD_findDecompressedSize(c.data(), c.size());
  EXPECT_EQ(kData.size(), size);
}

TEST(Block, CCtxLevelIncrease) {
  std::string c;
  auto cctx = createCCtx(6);
  auto dctx = createDCtx();
  for (int level = 1; level <= 6; ++level) {
    auto compressed = compress(*cctx, kData, level);
    auto const decompressed = decompress(*dctx, compressed, kData.size());
    EXPECT_EQ(kData, decompressed);
  }
}

TEST(Block, PlainDict) {
  auto cctx = createCCtx(1);
  auto const compressed = compress(*cctx, kData, 1, kPlainDict);
  auto dctx = createDCtx();
  EXPECT_ANY_THROW(decompress(*dctx, compressed, kData.size()));
  auto const decompressed =
      decompress(*dctx, compressed, kData.size(), kPlainDict);
  EXPECT_EQ(kData, decompressed);
}

TEST(Block, ZstdDict) {
  auto cctx = createCCtx(1);
  auto const compressed = compress(*cctx, kData, 1, kZstdDict);
  auto dctx = createDCtx();
  EXPECT_ANY_THROW(decompress(*dctx, compressed, kData.size()));
  auto const decompressed =
      decompress(*dctx, compressed, kData.size(), kZstdDict);
  EXPECT_EQ(kData, decompressed);
}

TEST(Block, PreprocessedPlainDict) {
  auto cctx = createCCtx(1);
  auto const cdict = createCDict(kPlainDict, 1);
  auto const compressed = compress(*cctx, kData, *cdict);
  auto dctx = createDCtx();
  auto const ddict = createDDict(kPlainDict);
  EXPECT_ANY_THROW(decompress(*dctx, compressed, kData.size()));
  auto const decompressed =
      decompress(*dctx, compressed, kData.size(), *ddict);
  EXPECT_EQ(kData, decompressed);
}

TEST(Block, PreprocessedZstdDict) {
  auto cctx = createCCtx(1);
  auto const cdict = createCDict(kZstdDict, 1);
  auto const compressed = compress(*cctx, kData, *cdict);
  auto dctx = createDCtx();
  auto const ddict = createDDict(kZstdDict);
  EXPECT_ANY_THROW(decompress(*dctx, compressed, kData.size()));
  auto const decompressed =
      decompress(*dctx, compressed, kData.size(), *ddict);
  EXPECT_EQ(kData, decompressed);
}

TEST(Block, RecreateCCtx) {
  auto cctx = createCCtx(1);
  {
    auto const compressed = compress(*cctx, kData, 1);
    auto dctx = createDCtx();
    auto const decompressed = decompress(*dctx, compressed, kData.size());
    EXPECT_EQ(kData, decompressed);
  }
  // Create the cctx with the same memory
  auto d = cctx.get_deleter();
  auto raw = cctx.release();
  auto params = ZSTD_getParams(1, 0, 0);
  cctx.reset(
      ZSTD_createCCtx(d.memory, ZSTD_CCtxWorkspaceBound(params.cParams)));
  // Repeat
  {
    auto const compressed = compress(*cctx, kData, 1);
    auto dctx = createDCtx();
    auto const decompressed = decompress(*dctx, compressed, kData.size());
    EXPECT_EQ(kData, decompressed);
  }
}

TEST(Block, RecreateDCtx) {
  auto dctx = createDCtx();
  {
    auto cctx = createCCtx(1);
    auto const compressed = compress(*cctx, kData, 1);
    auto const decompressed = decompress(*dctx, compressed, kData.size());
    EXPECT_EQ(kData, decompressed);
  }
  // Create the cctx with the same memory
  auto d = dctx.get_deleter();
  auto raw = dctx.release();
  dctx.reset(ZSTD_createDCtx(d.memory, ZSTD_DCtxWorkspaceBound()));
  // Repeat
  {
    auto cctx = createCCtx(1);
    auto const compressed = compress(*cctx, kData, 1);
    auto dctx = createDCtx();
    auto const decompressed = decompress(*dctx, compressed, kData.size());
    EXPECT_EQ(kData, decompressed);
  }
}

TEST(Stream, Basic) {
  auto zcs = createCStream(1);
  auto const compressed = compress(*zcs, kData);
  auto zds = createDStream();
  auto const decompressed = decompress(*zds, compressed, kData.size());
  EXPECT_EQ(kData, decompressed);
}

TEST(Stream, PlainDict) {
  auto params = ZSTD_getParams(1, kData.size(), kPlainDict.size());
  params.cParams.windowLog = 17;
  auto cdict = createCDict(kPlainDict, params);
  auto zcs = createCStream(params.cParams, *cdict, kData.size());
  auto const compressed = compress(*zcs, kData);
  auto const contentSize =
      ZSTD_findDecompressedSize(compressed.data(), compressed.size());
  EXPECT_ANY_THROW(decompress(*createDStream(), compressed, kData.size()));
  auto ddict = createDDict(kPlainDict);
  auto zds = createDStream(1 << 17, ddict.get());
  auto const decompressed = decompress(*zds, compressed, kData.size());
  EXPECT_EQ(kData, decompressed);
}

TEST(Stream, ZstdDict) {
  auto params = ZSTD_getParams(1, 0, 0);
  params.cParams.windowLog = 17;
  auto cdict = createCDict(kZstdDict, 1);
  auto zcs = createCStream(params.cParams, *cdict);
  auto const compressed = compress(*zcs, kData);
  EXPECT_ANY_THROW(decompress(*createDStream(), compressed, kData.size()));
  auto ddict = createDDict(kZstdDict);
  auto zds = createDStream(1 << 17, ddict.get());
  auto const decompressed = decompress(*zds, compressed, kData.size());
  EXPECT_EQ(kData, decompressed);
}

TEST(Stream, ResetCStream) {
  auto zcs = createCStream(1);
  auto zds = createDStream();
  {
    auto const compressed = compress(*zcs, kData);
    auto const decompressed = decompress(*zds, compressed, kData.size());
    EXPECT_EQ(kData, decompressed);
  }
  {
    ZSTD_resetCStream(zcs.get(), 0);
    auto const compressed = compress(*zcs, kData);
    auto const decompressed = decompress(*zds, compressed, kData.size());
    EXPECT_EQ(kData, decompressed);
  }
}

TEST(Stream, ResetDStream) {
  auto zcs = createCStream(1);
  auto zds = createDStream();
  auto const compressed = compress(*zcs, kData);
  EXPECT_ANY_THROW(decompress(*zds, kData, kData.size()));
  EXPECT_ANY_THROW(decompress(*zds, compressed, kData.size()));
  ZSTD_resetDStream(zds.get());
  auto const decompressed = decompress(*zds, compressed, kData.size());
  EXPECT_EQ(kData, decompressed);
}

TEST(Stream, Flush) {
  auto zcs = createCStream(1);
  auto zds = createDStream();
  std::string compressed;
  {
    compressed.resize(ZSTD_compressBound(kData.size()));
    ZSTD_inBuffer in = {kData.data(), kData.size(), 0};
    ZSTD_outBuffer out = {&compressed[0], compressed.size(), 0};
    while (in.pos != in.size) {
      size_t const rc = ZSTD_compressStream(zcs.get(), &out, &in);
      if (ZSTD_isError(rc)) {
        throw std::runtime_error{"compress stream failed"};
      }
    }
    EXPECT_EQ(0, out.pos);
    size_t const rc = ZSTD_flushStream(zcs.get(), &out);
    if (rc != 0) {
      throw std::runtime_error{"compress end failed"};
    }
    compressed.resize(out.pos);
    EXPECT_LT(0, out.pos);
  }
  std::string decompressed;
  {
    decompressed.resize(kData.size());
    ZSTD_inBuffer in = {compressed.data(), compressed.size(), 0};
    ZSTD_outBuffer out = {&decompressed[0], decompressed.size(), 0};
    while (in.pos != in.size) {
      size_t const rc = ZSTD_decompressStream(zds.get(), &out, &in);
      if (ZSTD_isError(rc)) {
        throw std::runtime_error{"decompress stream failed"};
      }
    }
  }
  EXPECT_EQ(kData, decompressed);
}
