#include <assert.h>
#include <memory>
#include <optional>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef USE_LIBPROTOBUF_MUTATOR
#include <src/libfuzzer/libfuzzer_macro.h>
#endif
#include <zlib.h>

#include "fuzz_target.pb.h"

static_assert(PB_Z_NO_FLUSH == Z_NO_FLUSH);
static_assert(PB_Z_PARTIAL_FLUSH == Z_PARTIAL_FLUSH);
static_assert(PB_Z_SYNC_FLUSH == Z_SYNC_FLUSH);
static_assert(PB_Z_FULL_FLUSH == Z_FULL_FLUSH);
static_assert(PB_Z_FINISH == Z_FINISH);
static_assert(PB_Z_BLOCK == Z_BLOCK);
static_assert(PB_Z_TREES == Z_TREES);
static_assert(PB_Z_NO_COMPRESSION == Z_NO_COMPRESSION);
static_assert(PB_Z_BEST_SPEED == Z_BEST_SPEED);
static_assert(PB_Z_BEST_COMPRESSION == Z_BEST_COMPRESSION);
static_assert(PB_Z_DEFAULT_COMPRESSION == Z_DEFAULT_COMPRESSION);
static_assert(PB_Z_DEFAULT_STRATEGY == Z_DEFAULT_STRATEGY);
static_assert(PB_Z_FILTERED == Z_FILTERED);
static_assert(PB_Z_HUFFMAN_ONLY == Z_HUFFMAN_ONLY);
static_assert(PB_Z_RLE == Z_RLE);
static_assert(PB_Z_FIXED == Z_FIXED);

static int Debug;

__attribute__((constructor)) static void Init() {
  const char *Env = getenv("DEBUG");
  if (Env && !strcmp(Env, "1"))
    Debug = 1;
}

static void HexDump(FILE *stream, const void *Data, size_t Size) {
  for (size_t i = 0; i < Size; i++)
    fprintf(stream, "\\x%02x", ((const uint8_t *)Data)[i]);
}

static int DeflateSetDictionary(z_stream *Strm, const Bytef *Dict,
                                size_t DictLen) {
  if (Debug) {
    fprintf(stderr, "deflateSetDictionary(&Strm, \"");
    HexDump(stderr, Dict, DictLen);
    fprintf(stderr, "\", %zu) = ", DictLen);
  }
  int Err = deflateSetDictionary(Strm, Dict, DictLen);
  if (Debug)
    fprintf(stderr, "%i;\n", Err);
  return Err;
}

static int Deflate(z_stream *Strm, int Flush) {
  if (Debug)
    fprintf(stderr, "avail_in = %u; avail_out = %u; deflate(&Strm, %i) = ",
            Strm->avail_in, Strm->avail_out, Flush);
  int Err = deflate(Strm, Flush);
  if (Debug)
    fprintf(stderr, "%i;\n", Err);
  return Err;
}

static int InflateSetDictionary(z_stream *Strm, const Bytef *Dict,
                                size_t DictLen) {
  if (Debug) {
    fprintf(stderr, "inflateSetDictionary(&Strm, \"");
    HexDump(stderr, Dict, DictLen);
    fprintf(stderr, "\", %zu) = ", DictLen);
  }
  int Err = inflateSetDictionary(Strm, Dict, DictLen);
  if (Debug)
    fprintf(stderr, "%i;\n", Err);
  return Err;
}

static int Inflate(z_stream *Strm, int Flush) {
  if (Debug)
    fprintf(stderr, "avail_in = %u; avail_out = %u; inflate(&Strm, %i) = ",
            Strm->avail_in, Strm->avail_out, Flush);
  int Err = inflate(Strm, Flush);
  if (Debug)
    fprintf(stderr, "%i;\n", Err);
  return Err;
}

struct Avail {
  z_stream *const Strm;
  const uInt AvailIn0;
  const uInt AvailIn1;
  const uInt AvailOut0;
  const uInt AvailOut1;

  template <typename OpT>
  Avail(z_stream *Strm, const OpT &Op)
      : Strm(Strm), AvailIn0(Strm->avail_in),
        AvailIn1(AvailIn0 < (uInt)Op.avail_in() ? AvailIn0
                                                : (uInt)Op.avail_in()),
        AvailOut0(Strm->avail_out),
        AvailOut1(AvailOut0 < (uInt)Op.avail_out() ? AvailOut0
                                                   : (uInt)Op.avail_out()) {
    Strm->avail_in = AvailIn1;
    Strm->avail_out = AvailOut1;
  }

  ~Avail() {
    uInt ConsumedIn = AvailIn1 - Strm->avail_in;
    Strm->avail_in = AvailIn0 - ConsumedIn;
    uInt ConsumedOut = AvailOut1 - Strm->avail_out;
    Strm->avail_out = AvailOut0 - ConsumedOut;
  }
};

struct OpRunner {
  z_stream *const Strm;

  OpRunner(z_stream *Strm) : Strm(Strm) {}

  int operator()(const class Deflate &Op) const {
    Avail Avail(Strm, Op);
    int Err = Deflate(Strm, Op.flush());
    assert(Err == Z_OK || Err == Z_BUF_ERROR);
    return Err;
  }

  int operator()(const class DeflateParams &Op) const {
    Avail Avail(Strm, Op);
    if (Debug)
      fprintf(stderr,
              "avail_in = %u; avail_out = %u; deflateParams(&Strm, %i, %i) = ",
              Strm->avail_in, Strm->avail_out, Op.level(), Op.strategy());
    int Err = deflateParams(Strm, Op.level(), Op.strategy());
    if (Debug)
      fprintf(stderr, "%i;\n", Err);
    assert(Err == Z_OK || Err == Z_BUF_ERROR);
    return Err;
  }

  int operator()(const class Inflate &Op) const {
    Avail Avail(Strm, Op);
    int Err = Inflate(Strm, Op.flush());
    assert(Err == Z_OK || Err == Z_STREAM_END || Err == Z_NEED_DICT ||
           Err == Z_BUF_ERROR);
    return Err;
  }
};

template <typename V>
static int VisitOp(const DeflateOp &Op, const V &Visitor) {
  if (Op.has_deflate())
    return Visitor(Op.deflate());
  else if (Op.has_deflate_params())
    return Visitor(Op.deflate_params());
  else {
    fprintf(stderr, "Unexpected DeflateOp.op_case() = %i\n", Op.op_case());
    assert(0);
  }
}

template <typename V>
static int VisitMutableOp(DeflateOp &Op, const V &Visitor) {
  if (Op.has_deflate())
    return Visitor(Op.mutable_deflate());
  else if (Op.has_deflate_params())
    return Visitor(Op.mutable_deflate_params());
  else {
    fprintf(stderr, "Unexpected DeflateOp.op_case() = %i\n", Op.op_case());
    assert(0);
  }
}

template <typename V>
static int VisitOp(const InflateOp &Op, const V &Visitor) {
  if (Op.has_inflate())
    return Visitor(Op.inflate());
  else {
    fprintf(stderr, "Unexpected InflateOp.op_case() = %i\n", Op.op_case());
    assert(0);
  }
}

template <typename V>
static int VisitMutableOp(InflateOp &Op, const V &Visitor) {
  if (Op.has_inflate())
    return Visitor(Op.mutable_inflate());
  else {
    fprintf(stderr, "Unexpected InflateOp.op_case() = %i\n", Op.op_case());
    assert(0);
  }
}

template <typename OpsT>
static void NormalizeOps(OpsT *Ops, uInt TotalIn, uInt TotalOut) {
  uInt InDivisor = 0;
  uInt OutDivisor = 0;
  for (typename OpsT::value_type &Op : *Ops) {
    VisitOp(Op, [&InDivisor](auto &Op) {
      InDivisor += Op.avail_in();
      return 0;
    });
    VisitOp(Op, [&OutDivisor](auto &Op) {
      OutDivisor += Op.avail_out();
      return 0;
    });
  }
  if (InDivisor != 0)
    for (typename OpsT::value_type &Op : *Ops)
      VisitMutableOp(Op, [TotalIn, InDivisor](auto *Op) {
        Op->set_avail_in((Op->avail_in() * TotalIn) / InDivisor);
        return 0;
      });
  if (OutDivisor != 0)
    for (typename OpsT::value_type &Op : *Ops)
      VisitMutableOp(Op, [TotalOut, OutDivisor](auto *Op) {
        Op->set_avail_out((Op->avail_out() * TotalOut) / OutDivisor);
        return 0;
      });
}

#ifndef USE_LIBPROTOBUF_MUTATOR
static Level ChooseLevel(uint8_t Choice) {
  if (Choice < 128)
    return (Level)((Choice % 11) - 1);
  else
    return PB_Z_BEST_SPEED;
}

static WindowBits ChooseWindowBits(uint8_t Choice) {
  if (Choice < 85)
    return WB_RAW;
  else if (Choice < 170)
    return WB_ZLIB;
  else
    return WB_GZIP;
}

static MemLevel ChooseMemLevel(uint8_t Choice) {
  return (MemLevel)((Choice % 9) + 1);
}

static Strategy ChooseStrategy(uint8_t Choice) {
  if (Choice < 43)
    return PB_Z_FILTERED;
  else if (Choice < 86)
    return PB_Z_HUFFMAN_ONLY;
  else if (Choice < 128)
    return PB_Z_RLE;
  else if (Choice < 196)
    return PB_Z_FIXED;
  else
    return PB_Z_DEFAULT_STRATEGY;
}

static Flush ChooseDeflateFlush(uint8_t Choice) {
  if (Choice < 32)
    return PB_Z_PARTIAL_FLUSH;
  else if (Choice < 64)
    return PB_Z_SYNC_FLUSH;
  else if (Choice < 96)
    return PB_Z_FULL_FLUSH;
  else if (Choice < 128)
    return PB_Z_BLOCK;
  else
    return PB_Z_NO_FLUSH;
}

static bool GeneratePlan(Plan &Plan, const uint8_t *&Data, size_t &Size) {
#define POP(X)                                                                 \
  if (Size == 0)                                                               \
    return false;                                                              \
  (X) = *Data;                                                                 \
  Data++;                                                                      \
  Size--;

  uint8_t InitialLevelChoice;
  POP(InitialLevelChoice);
  Plan.set_level(ChooseLevel(InitialLevelChoice));
  uint8_t WindowBitsChoice;
  POP(WindowBitsChoice);
  Plan.set_window_bits(ChooseWindowBits(WindowBitsChoice));
  uint8_t MemLevelChoice;
  POP(MemLevelChoice);
  Plan.set_mem_level(ChooseMemLevel(MemLevelChoice));
  uint8_t InitialStrategyChoice;
  POP(InitialStrategyChoice);
  Plan.set_strategy(ChooseStrategy(InitialStrategyChoice));

  if (Plan.window_bits() != WB_GZIP) {
    size_t DictLen;
    POP(DictLen);
    if (DictLen > 0 && DictLen < 128) {
      size_t MaxDictLen = Size / 4;
      if (DictLen > MaxDictLen)
        DictLen = MaxDictLen;
      Plan.set_dict(Data, DictLen);
      Data += DictLen;
      Size -= DictLen;
    }
  }

  size_t DeflateOpCount;
  POP(DeflateOpCount);
  size_t MaxDeflateOpCount = Size / 2;
  if (DeflateOpCount > MaxDeflateOpCount)
    DeflateOpCount = MaxDeflateOpCount;
  for (size_t i = 0; i < DeflateOpCount; i++) {
    DeflateOp *Op = Plan.add_deflate_ops();
    uint8_t AvailIn;
    POP(AvailIn);
    AvailIn++;
    uint8_t AvailOut;
    POP(AvailOut);
    AvailOut++;
    uint8_t KindChoice;
    POP(KindChoice);
    if (KindChoice < 32) {
      std::unique_ptr<class DeflateParams> DeflateParams =
          std::make_unique<class DeflateParams>();
      DeflateParams->set_avail_in(AvailIn);
      DeflateParams->set_avail_out(AvailOut);
      uint8_t LevelChoice;
      POP(LevelChoice);
      DeflateParams->set_level(ChooseLevel(LevelChoice));
      uint8_t StrategyChoice;
      POP(StrategyChoice);
      DeflateParams->set_strategy(ChooseStrategy(StrategyChoice));
      Op->set_allocated_deflate_params(DeflateParams.release());
    } else {
      std::unique_ptr<class Deflate> Deflate =
          std::make_unique<class Deflate>();
      Deflate->set_avail_in(AvailIn);
      Deflate->set_avail_out(AvailOut);
      uint8_t FlushChoice;
      POP(FlushChoice);
      Deflate->set_flush(ChooseDeflateFlush(FlushChoice));
      Op->set_allocated_deflate(Deflate.release());
    }
  }

  size_t InflateOpCount;
  POP(InflateOpCount);
  size_t MaxInflateOpCount = MaxDeflateOpCount * 2;
  if (InflateOpCount > MaxInflateOpCount)
    InflateOpCount = MaxInflateOpCount;
  for (size_t i = 0; i < InflateOpCount; i++) {
    InflateOp *Op = Plan.add_inflate_ops();
    uint8_t AvailIn;
    POP(AvailIn);
    AvailIn++;
    uint8_t AvailOut;
    POP(AvailOut);
    AvailOut++;
    std::unique_ptr<class Inflate> Inflate = std::make_unique<class Inflate>();
    Inflate->set_avail_in(AvailIn);
    Inflate->set_avail_out(AvailOut);
    Inflate->set_flush(PB_Z_NO_FLUSH);
    Op->set_allocated_inflate(Inflate.release());
  }

  size_t TailSize;
  POP(TailSize);
  Plan.set_tail_size(TailSize);
#undef POP

  Plan.set_data(Data, Size);

  return true;
}
#endif

static void FixupPlan(Plan *Plan) {
  if (Plan->data().size() == 0)
    Plan->set_data("!");
}

static void RunPlan(Plan &Plan) {
  size_t CompressedSize =
      Plan.data().size() * 2 + (Plan.deflate_ops_size() + 1) * 128;
  NormalizeOps(Plan.mutable_deflate_ops(), Plan.data().size(), CompressedSize);
  if (Debug)
    fprintf(stderr, "n_deflate_ops = %i;\n", Plan.deflate_ops_size());

  std::unique_ptr<uint8_t[]> Compressed(new uint8_t[CompressedSize]);
  z_stream Strm;
  memset(&Strm, 0, sizeof(Strm));
  int Err = deflateInit2(&Strm, Plan.level(), Z_DEFLATED, Plan.window_bits(),
                         Plan.mem_level(), Plan.strategy());
  if (Debug)
    fprintf(stderr, "deflateInit2(&Strm, %i, Z_DEFLATED, %i, %i, %i) = %i;\n",
            Plan.level(), Plan.window_bits(), Plan.mem_level(), Plan.strategy(),
            Err);
  assert(Err == Z_OK);
  if (Plan.dict().size() > 0) {
    Err = DeflateSetDictionary(&Strm, (const Bytef *)Plan.dict().c_str(),
                               Plan.dict().size());
    assert(Err == Z_OK);
  }
  Strm.next_in = (const Bytef *)Plan.data().c_str();
  Strm.avail_in = Plan.data().size();
  Strm.next_out = Compressed.get();
  Strm.avail_out = CompressedSize;
  if (Debug) {
    fprintf(stderr, "char next_in[%zu] = \"", Plan.data().size());
    HexDump(stderr, Plan.data().c_str(), Plan.data().size());
    fprintf(stderr, "\";\nchar next_out[%zu];\n", CompressedSize);
  }
  for (int i = 0; i < Plan.deflate_ops_size(); i++)
    VisitOp(Plan.deflate_ops(i), OpRunner(&Strm));
  Err = Deflate(&Strm, Z_FINISH);
  assert(Err == Z_STREAM_END);
  assert(Strm.avail_in == 0);
  uInt ActualCompressedSize = CompressedSize - Strm.avail_out;
  assert(ActualCompressedSize == Strm.total_out);
  if (Debug)
    fprintf(stderr, "total_out = %i;\n", ActualCompressedSize);
  Err = deflateEnd(&Strm);
  assert(Err == Z_OK);

  NormalizeOps(Plan.mutable_inflate_ops(), ActualCompressedSize,
               Plan.data().size());
  if (Debug)
    fprintf(stderr, "n_inflate_ops = %i;\n", Plan.inflate_ops_size());

  std::unique_ptr<uint8_t[]> Uncompressed(new uint8_t[Plan.data().size()]);
  Err = inflateInit2(&Strm, Plan.window_bits());
  if (Debug)
    fprintf(stderr, "inflateInit2(&Strm, %i) = %i;\n", Plan.window_bits(), Err);
  assert(Err == Z_OK);
  if (Plan.dict().size() > 0 && Plan.window_bits() == WB_RAW) {
    Err = InflateSetDictionary(&Strm, (const Bytef *)Plan.dict().c_str(),
                               Plan.dict().size());
    assert(Err == Z_OK);
  }
  Strm.next_in = Compressed.get();
  Strm.avail_in = ActualCompressedSize;
  Strm.next_out = Uncompressed.get();
  Strm.avail_out = Plan.data().size() + Plan.tail_size();
  for (int i = 0; i < Plan.inflate_ops_size(); i++) {
    Err = VisitOp(Plan.inflate_ops(i), OpRunner(&Strm));
    if (Err == Z_NEED_DICT) {
      assert(Plan.dict().size() > 0 && Plan.window_bits() == WB_ZLIB);
      Err = InflateSetDictionary(&Strm, (const Bytef *)Plan.dict().c_str(),
                                 Plan.dict().size());
      assert(Err == Z_OK);
    }
  }
  if (Err != Z_STREAM_END) {
    Err = Inflate(&Strm, Z_NO_FLUSH);
    if (Err == Z_NEED_DICT) {
      assert(Plan.dict().size() > 0 && Plan.window_bits() == WB_ZLIB);
      Err = InflateSetDictionary(&Strm, (const Bytef *)Plan.dict().c_str(),
                                 Plan.dict().size());
      assert(Err == Z_OK);
      Err = Inflate(&Strm, Z_NO_FLUSH);
    }
  }
  assert(Err == Z_STREAM_END);
  assert(Strm.avail_in == 0);
  assert(Strm.avail_out == (uInt)Plan.tail_size());
  assert(memcmp(Uncompressed.get(), Plan.data().c_str(), Plan.data().size()) ==
         0);
  Err = inflateEnd(&Strm);
  assert(Err == Z_OK);
}

#ifdef USE_LIBPROTOBUF_MUTATOR
static void FixupOp(DeflateOp *Op) {
  if (Op->has_deflate() && (Op->deflate().flush() == PB_Z_FINISH ||
                            Op->deflate().flush() == PB_Z_TREES))
    Op->mutable_deflate()->set_flush(PB_Z_NO_FLUSH);
}

static void FixupOp(InflateOp *Op) {
  if (Op->has_inflate())
    Op->mutable_inflate()->set_flush(PB_Z_NO_FLUSH);
}

template <typename OpsT> static void FixupOps(OpsT *Ops) {
  int Pos = 0;
  for (int i = 0, size = Ops->size(); i < size; i++) {
    typename OpsT::value_type &Op = (*Ops)[i];
    if (Op.op_case() == 0)
      continue;
    FixupOp(&Op);
    Ops->SwapElements(Pos, i);
    Pos++;
  }
  Ops->DeleteSubrange(Pos, Ops->size() - Pos);
}

static protobuf_mutator::libfuzzer::PostProcessorRegistration<Plan> reg = {
    [](Plan *Plan, unsigned int /* Seed */) {
      FixupPlan(Plan);
      if (Plan->window_bits() == WB_DEFAULT)
        Plan->set_window_bits(WB_ZLIB);
      if (Plan->mem_level() == MEM_LEVEL_DEFAULT)
        Plan->set_mem_level(MEM_LEVEL8);
      FixupOps(Plan->mutable_deflate_ops());
      FixupOps(Plan->mutable_inflate_ops());
      if (Plan->window_bits() == WB_GZIP)
        Plan->clear_dict();
      Plan->set_tail_size(Plan->tail_size() & 0xff);
    }};

DEFINE_PROTO_FUZZER(const Plan &Plan) {
  class Plan PlanCopy = Plan;
  RunPlan(PlanCopy);
}
#else
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  Plan Plan;
  if (GeneratePlan(Plan, Data, Size)) {
    FixupPlan(&Plan);
    RunPlan(Plan);
  }
  return 0;
}
#endif
