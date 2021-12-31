#include "mold.h"

#include <fstream>
#include <iomanip>
#include <ios>
#include <sstream>
#include <unordered_map>
#include <ParallelTools/concurrent_hash_map.hpp>

namespace mold::elf {

template <typename E>
using Map =
  ParallelTools::concurrent_hash_map<InputSection<E> *, std::vector<Symbol<E> *>>;

template <typename E>
static std::unique_ptr<std::ofstream> open_output_file(Context<E> &ctx) {
  std::unique_ptr<std::ofstream> file(new std::ofstream);
  file->open(ctx.arg.Map.c_str());
  if (!file->is_open())
    Fatal(ctx) << "cannot open " << ctx.arg.Map << ": " << errno_string();
  return file;
}

template <typename E>
static Map<E> get_map(Context<E> &ctx) {
  Map<E> map;

  ParallelTools::parallel_for_each(ctx.objs, [&](ObjectFile<E> *file) {
    for (Symbol<E> *sym : file->symbols) {
      if (sym->file == file && sym->input_section &&
          sym->get_type() != STT_SECTION) {
        assert(file == &sym->input_section->file);

        auto p = map.insert(sym->input_section, {});
        p.second->push_back(sym);
      }
    }
  });
  map.for_each([](InputSection<E> * key, std::vector<Symbol<E> *> &value) {sort(value, [](Symbol<E> *a, Symbol<E> *b) { return a->value < b->value; });});
  return map;
}

template <typename E>
void print_map(Context<E> &ctx) {
  std::ostream *out = &std::cout;
  std::unique_ptr<std::ofstream> file;

  if (!ctx.arg.Map.empty()) {
    file = open_output_file(ctx);
    out = file.get();
  }

  // Construct a section-to-symbol map.
  Map<E> map = get_map(ctx);

  // Print a mapfile.
  *out << "             VMA       Size Align Out     In      Symbol\n";

  for (Chunk<E> *osec : ctx.chunks) {
    *out << std::setw(16) << (u64)osec->shdr.sh_addr
         << std::setw(11) << (u64)osec->shdr.sh_size
         << std::setw(6) << (u64)osec->shdr.sh_addralign
         << " " << osec->name << "\n";

    if (osec->kind != Chunk<E>::REGULAR)
      continue;

    std::span<InputSection<E> *> members = ((OutputSection<E> *)osec)->members;
    std::vector<std::string> bufs(members.size());

    ParallelTools::parallel_for((i64)0, (i64)members.size(), [&](i64 i) {
      InputSection<E> *mem = members[i];
      std::ostringstream ss;
      opt_demangle = ctx.arg.demangle;

      ss << std::setw(16) << (osec->shdr.sh_addr + mem->offset)
         << std::setw(11) << (u64)mem->shdr.sh_size
         << std::setw(6) << (u64)mem->shdr.sh_addralign
         << "         " << *mem << "\n";

      for (Symbol<E> *sym : map.value(mem, {}))
        ss << std::setw(16) << sym->get_addr(ctx)
            << "          0     0                 "
            << *sym << "\n";

      bufs[i] = std::move(ss.str());
    });

    for (std::string &str : bufs)
      *out << str;
  }
}

#define INSTANTIATE(E)                          \
  template void print_map(Context<E> &ctx);

INSTANTIATE(X86_64);
INSTANTIATE(I386);
INSTANTIATE(ARM64);

} // namespace mold::elf
