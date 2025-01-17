#include <cppgit2/strarray.hpp>
#include <cstdlib>
#include <cstring>

#include <git2/sys/alloc.h>

extern "C" git_allocator git__allocator;

#define git__malloc(len)                      git__allocator.gmalloc(len, __FILE__, __LINE__)

namespace cppgit2 {


strarray::strarray() {
  c_struct_.count = 0;
  c_struct_.strings = nullptr;
}

strarray::strarray(const std::vector<std::string> &strings) {
  auto size = strings.size();
  c_struct_.count = size;
  c_struct_.strings = (char **)git__malloc(size * sizeof(char *));
  for (size_t i = 0; i < size; ++i) {
    auto length = strings[i].size() + 1;
    c_struct_.strings[i] = (char *)git__malloc(length * sizeof(char));
    strncpy(c_struct_.strings[i], strings[i].c_str(), length);
    c_struct_.strings[i][length - 1] = '\0';
  }
}

strarray::strarray(const git_strarray *c_ptr) {
  c_struct_.count = c_ptr->count;
  c_struct_.strings = (char **)git__malloc(c_ptr->count * sizeof(char *));
  for (size_t i = 0; i < c_ptr->count; ++i) {
    auto length = strlen(c_ptr->strings[i]) + 1;
    c_struct_.strings[i] = (char *)git__malloc(length * sizeof(char));
    strncpy(c_struct_.strings[i], c_ptr->strings[i], length);
    c_struct_.strings[i][length - 1] = '\0';
  }
}

strarray::strarray(const strarray& other)
{
  *this = other;
}

strarray& strarray::operator = (const strarray& other) {
  if (git_strarray_copy(&c_struct_, &other.c_struct_))
    throw git_exception();
}


strarray::~strarray() {
  if (c_struct_.count)
    git_strarray_free(&c_struct_);
}

strarray strarray::copy() const {
  strarray result;
  if (git_strarray_copy(&result.c_struct_, &c_struct_))
    throw git_exception();
  return result;
}

std::vector<std::string> strarray::to_vector() const {
  std::vector<std::string> result{};
  for (auto tag : *this)
    result.push_back(tag);
  return result;
}

const git_strarray *strarray::c_ptr() const { return &c_struct_; }

} // namespace cppgit2
