<p align="center">
  <img height="100" src="img/logo.png"/>  
</p>

`cppgit2` is a `libgit2` wrapper library for use in modern C++ `( >= C++11)`. See the [Build and Integration](#build-and-integration) section for details on how to build and integrate `cppgit2` in your projects.

<p align="center">
  <img src="img/init_add_commit.png"/>
</p>

## Build and Integration

Run the following commands to build `cppgit2`. 

**NOTE**: This also builds `libgit2` from source. `libgit2` is a submodule in the `ext/` directory that points to a stable release commit, e.g., [v0.99.0](https://github.com/libgit2/libgit2/releases/tag/v0.99.0).

```bash
git clone --recurse-submodules -j8 https://github.com/p-ranav/cppgit2
cd cppgit2
mkdir build && cd build
cmake .. && make
```

The build output is in four directories: `include`, `lib`, `samples` and `test`:

```
include/
├── cppgit2/
├── git2/
└── git2.h
lib/
├── libcppgit2.so -> libcppgit2.so.1
├── libcppgit2.so.0.1.0
├── libcppgit2.so.1 -> libcppgit2.so.0.1.0
├── libcppgit2.static.a
├── libgit2_clar
├── libgit2.pc
├── libgit2.so -> libgit2.so.99
├── libgit2.so.0.99.0
├── libgit2.so.99 -> libgit2.so.0.99.0
└── ...
samples/
test/
```

For integration in your projects,

* Add `build/include` to your `include_directories`
* Add `build/lib` to your `link_directories`
* Build your application, linking with `cppgit2`
* Add `build/lib` to your `LD_LIBRARY_PATH` to load the shared libraries at runtime. 

Here's an example using `g++`:

```bash
g++ -std=c++11 -Ibuild/include -Lbuild/lib -o my_sample my_sample.cpp -lcppgit2
export LD_LIBRARY_PATH=build/lib:$LD_LIBRARY_PATH
./my_sample
```

and the same example with `CMake`:

```cmake
PROJECT(my_sample)
CMAKE_MINIMUM_REQUIRED(VERSION 3.8)

INCLUDE_DIRECTORIES("build/include")
ADD_EXECUTABLE(my_sample my_sample.cpp)
find_library(CPPGIT2_LIBRARY cppgit2 HINTS ./build/lib)
TARGET_LINK_LIBRARIES(my_sample ${CPPGIT2_LIBRARY})
SET_PROPERTY(TARGET my_sample PROPERTY CXX_STANDARD 11)
```

## Sample Programs

### Initialize a new repository

To initialize a new repository, simply call `repository::init`. If you want to create a bare repository, set the second argument to `true`. 

```cpp
#include <cppgit2/repository.hpp>
using namespace cppgit2;

int main() {
  auto repo = repository::init("hello_world", false);
}
```

### Clone a repository and checkout specific branch

Let's say you want to clone a repository and checkout a specific branch. Construct an `options` object using `clone::options`, set the checkout branch name, and then use `repository::clone` to clone the repository. 

```cpp
// clone_and_checkout_branch.cpp
#include <cppgit2/repository.hpp>
using namespace cppgit2;

int main() {
  auto url = "https://github.com/fffaraz/awesome-cpp";
  auto branch_name = "gh-pages";
  auto path = "awesome_cpp";

  // Prepare clone options
  clone::options options;
  options.set_checkout_branch_name(branch_name);

  // Clone repository
  auto repo = repository::clone(url, path, options);
}
```

### Open an existing repository

You can open an existing repository with `repository::open`.

```cpp
#include <cppgit2/repository.hpp>
using namespace cppgit2;

int main() {
  auto path = "~/dev/foo/bar";          // bar must contain a .git directory
  auto repo = repository::open(path);
}
```

Use `repository::open_bare` to open a bare repository.

## Design Notes

### Interoperability with `libgit2`

Most `cppgit2` data structures can be constructed using a `libgit2` C pointer. 

```cpp
// Construct libgit2 signature
git_signature sig;
sig.name = (char *)"Foo Bar";
sig.email = (char *)"foo.bar@baz.com";

// Construct cppgit2 wrapper
cppgit2::signature sig2(&sig);

REQUIRE(sig2.name() == std::string(sig.name));
REQUIRE(sig2.email() == std::string(sig.email));
```

Similarly, a `libgit2` C pointer can be extracted from its wrapping `cppgit2` data structure using the `.c_ptr()` method.

```cpp
// Construct cppgit2 OID object
oid oid1("f9de917ac729414151fdce077d4098cfec9a45a5");

// Access libgit2 C ptr
const git_oid *oid1_cptr = oid1.c_ptr();

// Use the libgit2 C API to format
size_t n = 8;
char * oid1_formatted = (char *)malloc(sizeof(char) * n);
git_oid_tostr(oid1_formatted, n + 1, oid1_cptr);

// Results are the same
REQUIRE(oid1.to_hex_string(8) == std::string(oid1_formatted)); // f9de917
```

### Ownership and Memory Management

`libgit2` sometimes allocates memory and returns pointers to data structures that are owned by the user (required to be free'd by the user), and at other times returns a pointer to memory that is managed by the `libgit2` layer. 

To properly cleanup memory that is owned by the user, use the `ownership` enum to explicitly specify the ownership when wrapping.

```cpp
cppgit2::tree tree1(&tree_cptr, ownership::user);
```

If the pointer being wrapped is owned by the user, the object's destructor will call `git_<type>_free` on the pointer and clean up properly. If you specify the ownership as `ownership::libgit2`, the pointer is left alone. 

```cpp
tree::tree(git_tree *c_ptr, ownership owner = ownership::libgit2) 
  : c_ptr_(c_ptr), owner_(owner) {}

tree::~tree() {
  if (c_ptr_ && owner_ == ownership::user)
    git_tree_free(c_ptr_);
}
```

### Error Handling

At the moment, `cppgit2` throws a custom `git_exception` anytime the return value from `libgit2` indicates that an error has occurred. Typically `libgit2` functions respond with a return code (0 = good, anything else = error) and `git_error_last` provides the most recent error message. `cppgit2` uses this message when constructing the `git_exception` exception.

Here's a typical example of a wrapped function:

```cpp
void repository::delete_reflog(const std::string &name) {
  if (git_reflog_delete(c_ptr_, name.c_str()))
    throw git_exception();
}
```

where `git_exception` initializes its `what()` message like so:

```cpp
git_exception() {
  auto error = git_error_last();
  message_ = error ? error->message : "unknown error";
}

virtual const char *what() const throw() { return message_; }
```

## Contributions

`cppgit2` is in active development. `libgit2` is constantly evolving with over 750 functions in its public API. A good portion of this is covered in `cppgit2` (See [API Coverage](#api-coverage)). However, there is much room for improvement - in terms of code quality, tests, and samples. 

Contributions are welcome, have a look at the [CONTRIBUTING.md](CONTRIBUTING.md) document for more information. If you notice any bugs while using/reviewing `cppgit2`, please report them. Suggestions w.r.t improving the code quality are also welcome.

## License
The project is available under the [MIT](https://opensource.org/licenses/MIT) license.

## API Coverage

### annotated

| libgit2 | cppgit2:: |
| --- | --- |
| `git_annotated_commit_free` | `annotated_commit::~annotated_commit` |
| `git_annotated_commit_from_fetchhead` | `repository::create_annotated_commit` |
| `git_annotated_commit_from_ref` | `repository::create_annotated_commit` |
| `git_annotated_commit_from_revspec` | `epository::create_annotated_commit` |
| `git_annotated_commit_id` | `annotated_commit::id` |
| `git_annotated_commit_lookup` | `repository::lookup_annotated_commit` |
| `git_annotated_commit_ref` | `annotated_commit::refname` |


### apply

| libgit2 | cppgit2:: |
| --- | --- |
| `git_apply` | `repository::apply_diff` |
| `git_apply_to_tree` | `repository::apply_diff` |


### attr

| libgit2 | cppgit2:: |
| --- | --- |
| `git_attr_add_macro` | `repository::add_attribute_macro` |
| `git_attr_cache_flush` | `repository::flush_attrobutes_cache` |
| `git_attr_foreach` | `repository::for_each_attribute` |
| `git_attr_get` | `repository::lookup_attribute` |
| `git_attr_get_many` | `repository::lookup_multiple_attributes` |
| `git_attr_value` | `attribute::value` |


### blame

| libgit2 | cppgit2:: |
| --- | --- |
| `git_blame_buffer` | `blame::get_blame_for_buffer` |
| `git_blame_file` | `repository::blame_file` |
| `git_blame_free` | `blame::~blame` |
| `git_blame_get_hunk_byindex` | `blame::hunk_by_index` |
| `git_blame_get_hunk_byline` |`blame::hunk_by_line` |
| `git_blame_get_hunk_count` | `blame::hunk_count` |
| `git_blame_init_options` | `blame::options::options` |
| `git_blame_options_init` | `blame::options::options` |


### blob

| libgit2 | cppgit2:: |
| --- | --- |
| `git_blob_create_from_buffer` | `repository::create_blob_from_buffer` |
| `git_blob_create_from_disk` | `repository::create_blob_from_disk` |
`git_blob_create_from_stream` | |
`git_blob_create_from_stream_commit` | |
| `git_blob_create_from_workdir` | `repository::create_blobf=_from_workdir` |
| `git_blob_create_fromworkdir` | `repository::create_blobf=_from_workdir` |
| `git_blob_dup` | `blob::copy` |
| `git_blob_filter` | **Not implemented** |
| `git_blob_filtered_content` | **Not implemented** |
| `git_blob_free` | `blob::~blob` |
| `git_blob_id` | `blob::id` |
| `git_blob_is_binary` | `blob::is_binary` |
| `git_blob_lookup` | `repository::lookup_blob` |
| `git_blob_lookup_prefix` | `repository::lookup_blob` |
| `git_blob_owner` | `blob::owner` |
| `git_blob_rawcontent` | `blob::raw_content` |
| `git_blob_rawsize` | `blob::raw_size` |


### branch

| libgit2 | cppgit2:: |
| --- | --- |
| `git_branch_create` | `repository::create_branch` |
| `git_branch_create_from_annotated` | `repository::create_branch` |
| `git_branch_delete` | `repository::delete_branch` |
| `git_branch_is_checked_out` | `repository::is_branched_checked_out` |
| `git_branch_is_head` | `repository::is_head_pointing_to_branch` |
| `git_branch_iterator_free` | `repository::for_each_branch` |
| `git_branch_iterator_new` | `repository::for_each_branch` |
| `git_branch_lookup` | `repository::lookup_branch` |
| `git_branch_move` | `repository::rename_branch` |
| `git_branch_name` | `repository::branch_name` |
| `git_branch_next` | `repository::for_each_branch` |
| `git_branch_remote_name` | `repository::branch_remote_name` |
| `git_branch_set_upstream` | `repository::set_branch_upstream` |
| `git_branch_upstream` | `repository::branch_upstream` |
| `git_branch_upstream_name` | `repository::branch_upstream_name` |
| `git_branch_upstream_remote` | `repository::branch_upstream_remote` |


### buf

| libgit2 | cppgit2:: |
| --- | --- |
| `git_buf_contains_nul` |  `data_buffer::contains_nul` |
| `git_buf_dispose` | `data_buffer::~data_buffer` |
| `git_buf_free` | `data_buffer::~data_buffer`  |
| `git_buf_grow` | `data_buffer::grow_to_size` |
| `git_buf_is_binary` | `data_buffer::is_binary` |
| `git_buf_set` | `data_buffer::set_buffer` |

### checkout

| libgit2 | cppgit2:: |
| --- | --- |
| `git_checkout_head` | `repository::checkout_head` |
| `git_checkout_index` | `repository::checkout_index` |
| `git_checkout_options_init` | `repository::checkout::options::options` |
| `git_checkout_tree` | `repository::checkout_tree` |


### cherrypick

| libgit2 | cppgit2:: |
| --- | --- |
`git_cherrypick` | `repository::cherrypick_commit` |
`git_cherrypick_commit` | `repository::cherrypick_commit` |
`git_cherrypick_options_init` | `cherrypick::options::options` |


### clone

| libgit2 | cppgit2:: |
| --- | --- |
`git_clone` | `repository::clone` |
`git_clone_options_init` | `clone::options::options` |


### commit

| libgit2 | cppgit2:: |
| --- | --- |
| `git_commit_amend` | `commit::amend` |
| `git_commit_author` | `commit::author` |
`git_commit_author_with_mailmap` | |
| `git_commit_body` | `commit::body` |
| `git_commit_committer` | `commit::committer` |
`git_commit_committer_with_mailmap` | |
| `git_commit_create` | `repository::create_commit` |
| `git_commit_create_buffer` | `repository::create_commit` |
| `git_commit_create_v` | **Not implemented** |
| `git_commit_create_with_signature` | `repository::create_commit` |
| `git_commit_dup` | `commit::copy` |
| `git_commit_extract_signature` | `repository::extract_signature_from_commit` |
| `git_commit_free` | `commit::~commit` |
| `git_commit_header_field` | `commit::operator[]` |
| `git_commit_id` | `commit::id` |
| `git_commit_lookup` | `repository::lookup_commit` |
| `git_commit_lookup_prefix` | `repository::lookup_commit` |
| `git_commit_message` | `commit::message` |
| `git_commit_message_encoding` | `commit::message_encoding` |
| `git_commit_message_raw` | `commit::message_raw` |
| `git_commit_nth_gen_ancestor` | `commit::ancestor` |
| `git_commit_owner` | `commit::owner` |
| `git_commit_parent` | `commit::parent` |
| `git_commit_parent_id` | `commit::parent_id` |
| `git_commit_parentcount` | `commit::parent_count` |
| `git_commit_raw_header` | `commit::raw_header` |
| `git_commit_summary` | `commit::summary` |
| `git_commit_time` | `commit::time` |
| `git_commit_time_offset` | `commit::time_offset` |
| `git_commit_tree` | `commit::tree` |
| `git_commit_tree_id` | `commit::tree_id` |


### config

| libgit2 | cppgit2:: |
| --- | --- |
| `git_config_add_file_ondisk` | `repository::add_ondisk_config_file` |
| `git_config_backend_foreach_match` | **Not implemented** |
| `git_config_delete_entry` | `config::delete_entry` |
| `git_config_delete_multivar` | `config::delete_entry` |
| `git_config_entry_free` | `config::entry::~entry` |
| `git_config_find_global` | `config::locate_global_config` |
| `git_config_find_programdata` | `config::locate_global_config_in_programdata` |
| `git_config_find_system` | `config::locate_global_system_config` |
| `git_config_find_xdg` | `config::locate_global_xdg_compatible_config`  |
| `git_config_foreach` | `config::for_each` |
| `git_config_foreach_match` | `config::for_each` |
| `git_config_free` | `config::~config` |
| `git_config_get_bool` | `config::value_as_bool` |
| `git_config_get_entry` | `config::operator[]` |
| `git_config_get_int32` | `config::value_as_int32` |
| `git_config_get_int64` | `config::value_as_int64` |
| `git_config_get_mapped` | **Not implemented** |
| `git_config_get_multivar_foreach` | **Not implemented** |
| `git_config_get_path` | `config::path` |
| `git_config_get_string` | `config::value_as_string` |
| `git_config_get_string_buf` | `config::value_as_data_buffer` |
| `git_config_iterator_free` | `config::for_each_entry` |
| `git_config_iterator_glob_new` | **Not implemented** |
| `git_config_iterator_new` | `config::for_each_entry` |
| `git_config_lock` | `config::lock` |
| `git_config_lookup_map_value` | **Not implemented** |
| `git_config_multivar_iterator_new` | **Not implemented** |
| `git_config_new` | `config::new_config` |
| `git_config_next` | `config::for_each_entry` |
| `git_config_open_default` | `config::open_default_config`  |
| `git_config_open_global` | `config::open_global_config` |
| `git_config_open_level` | `config::open_config_at_level` |
| `git_config_open_ondisk` | **Not implemented** |
| `git_config_parse_bool` | `config::parse_as_bool` |
| `git_config_parse_int32` | `config::parse_as_int32` |
| `git_config_parse_int64` | `config::parse_as_int64` |
| `git_config_parse_path` | `config::parse_path` |
| `git_config_set_bool` | `config::insert_entry` |
| `git_config_set_int32` | `config::insert_entry` |
| `git_config_set_int64` | `config::insert_entry` |
| `git_config_set_multivar` | `config::insert_entry` |
| `git_config_set_string` | `config::insert_entry` |
| `git_config_snapshot` | `config::snapshot` |

### cred

| libgit2 | cppgit2:: |
| --- | --- |
`git_cred_default_new` | |
`git_cred_free` | |
`git_cred_get_username` | |
`git_cred_has_username` | |
`git_cred_ssh_custom_new` | |
`git_cred_ssh_interactive_new` | |
`git_cred_ssh_key_from_agent` | |
`git_cred_ssh_key_memory_new` | |
`git_cred_ssh_key_new` | |
`git_cred_username_new` | |
`git_cred_userpass` | |
`git_cred_userpass_plaintext_new` | |

### describe

| libgit2 | cppgit2:: |
| --- | --- |
`git_describe_commit` | |
`git_describe_format` | |
`git_describe_format_options_init` | |
`git_describe_options_init` | |
`git_describe_result_free` | |
`git_describe_workdir` | |

### diff

| libgit2 | cppgit2:: |
| --- | --- |
`git_diff_blob_to_buffer` | |
| `git_diff_blobs` | `diff::compare_files` |
`git_diff_buffers` | |
`git_diff_commit_as_email` | |
`git_diff_find_options_init` | |
`git_diff_find_similar` | |
`git_diff_foreach` | |
`git_diff_format_email` | |
`git_diff_format_email_options_init` | |
| `git_diff_free` | `diff::~diff` |
`git_diff_from_buffer` | |
| `git_diff_get_delta` | `diff::operator[]` |
`git_diff_get_stats` | |
`git_diff_index_to_index` | |
`git_diff_index_to_workdir` | `repository::create_diff_index_to_workdir` |
| `git_diff_is_sorted_icase` | `diff::is_sorted_case_sensitive` |
| `git_diff_merge` | `diff::merge` |
| `git_diff_num_deltas` | `diff::size` |
| `git_diff_num_deltas_of_type` | `diff::size` |
| `git_diff_options_init` | `diff::options::options` |
`git_diff_patchid` | |
`git_diff_patchid_options_init` | |
`git_diff_print` | |
`git_diff_stats_deletions` | |
`git_diff_stats_files_changed` | |
`git_diff_stats_free` | |
`git_diff_stats_insertions` | |
`git_diff_stats_to_buf` | |
| `git_diff_status_char` | `diff::status_char` |
| `git_diff_to_buf` | `diff::to_string` |
`git_diff_tree_to_index` | `repository::create_diff_tree_to_index` |
`git_diff_tree_to_tree` | `repository::create_diff_tree_to_tree` |
`git_diff_tree_to_workdir` | `repository::create_diff_tree_to_workdir` |
`git_diff_tree_to_workdir_with_index` | `create_diff_tree_to_workdir_with_index` |

### error

| libgit2 | cppgit2:: |
| --- | --- |
`git_error_clear` | `git_exception::clear` |
`git_error_last` | `git_exception::git_exception` |
`git_error_set_oom` | **Not Implemented** |
`git_error_set_str` | **Not Implemented** |

### fetch

| libgit2 | cppgit2:: |
| --- | --- |
`git_fetch_options_init` | `fetch::options::options` |

### graph

| libgit2 | cppgit2:: |
| --- | --- |
`git_graph_ahead_behind` | |
`git_graph_descendant_of` | |

### ignore

| libgit2 | cppgit2:: |
| --- | --- |
`git_ignore_add_rule` | `repository::add_ignore_rules` |
`git_ignore_clear_internal_rules` | `repository::clear_ignore_rules` |
`git_ignore_path_is_ignored` | `repository::is_path_ignored` |

### index

| libgit2 | cppgit2:: |
| --- | --- |
| `git_index_add` | `index::add_entry` |
| `git_index_add_all` | `index::add_entries_that_match` |
| `git_index_add_bypath` | `index::add_entry_by_path` |
| `git_index_add_from_buffer` | `index::add_entry_from_buffer` |
| `git_index_caps` | `index::capability_flags` |
| `git_index_checksum` | `index::checksum` |
| `git_index_clear` | `index::clear` |
| `git_index_conflict_add` | `index::add_conflict_entry` |
| `git_index_conflict_cleanup` | `index::remove_all_conflicts` |
| `git_index_conflict_get` | **Not Implemented** |
| `git_index_conflict_iterator_free` | `index::for_each_conflict` |
| `git_index_conflict_iterator_new` | `index::for_each_conflict` |
| `git_index_conflict_next` | `index::for_each_conflict` |
| `git_index_conflict_remove` | `index::remove_conflict_entries` |
| `git_index_entry_is_conflict` | `index::entry::is_conflict` |
| `git_index_entry_stage` | `index::entry::entry_stage` |
| `git_index_entrycount` | `index::size` |
| `git_index_find` | `index::find_first` |
| `git_index_find_prefix` | `index::find_first_matching_prefix` |
| `git_index_free` | `index::~index` |
| `git_index_get_byindex` | `index::operator[]` |
| `git_index_get_bypath` | `index::entry_in_path` |
| `git_index_has_conflicts` | `index::has_conflicts` |
| `git_index_iterator_free` | `index::for_each` |
| `git_index_iterator_new` | `index::for_each` |
| `git_index_iterator_next` | `index::for_each` |
| `git_index_new` | `index::index` |
| `git_index_open` | `index::open` |
| `git_index_owner` | `index::owner` |
| `git_index_path` | `index::path` |
| `git_index_read` | `index::read` |
| `git_index_read_tree` | `index::read_tree` |
| `git_index_remove` | `index::remove_entry` |
| `git_index_remove_all` | `index::remove_entries_that_match` |
| `git_index_remove_bypath` | `index::remove_entry_by_path` |
| `git_index_remove_directory` | `index::remove_entries_in_directory` |
| `git_index_set_caps` | `index::set_index_capabilities` |
| `git_index_set_version` | `index::set_version` |
| `git_index_update_all` | `index::update_entries_that_match` |
| `git_index_version` | `index::version` |
| `git_index_write` | `index::write` |
| `git_index_write_tree` | `index::write_tree` |
| `git_index_write_tree_to` | `index::write_tree_to` |


### indexer

| libgit2 | cppgit2:: |
| --- | --- |
`git_indexer_append` | `indexer::append` |
`git_indexer_commit` | `indexer::commit` |
`git_indexer_free` | `indexer::~indexer` |
`git_indexer_hash` | `indexer::hash` |
`git_indexer_new` | `indexer::indexer` |
`git_indexer_options_init` | `indexer::options::options` |


### libgit2

| libgit2 | cppgit2:: |
| --- | --- |
`git_libgit2_features` | |
| `git_libgit2_init` | `libgit2_api::libgit2_api` |
`git_libgit2_opts` | |
| `git_libgit2_shutdown` | `libgit2_api::~libgit2_api` |
| `git_libgit2_version` | `libgit2_api::version` |


### merge

| libgit2 | cppgit2:: |
| --- | --- |
`git_merge` | |
`git_merge_analysis` | |
`git_merge_analysis_for_ref` | |
`git_merge_base` | |
`git_merge_base_many` | |
`git_merge_base_octopus` | |
`git_merge_bases` | |
`git_merge_bases_many` | |
`git_merge_commits` | |
`git_merge_file` | |
`git_merge_file_from_index` | |
`git_merge_file_input_init` | |
`git_merge_file_options_init` | |
`git_merge_file_result_free` | |
`git_merge_options_init` | `merge::options::options` |
`git_merge_trees` | |

### note

| libgit2 | cppgit2:: |
| --- | --- |
| `git_note_author` | `note::author` |
| `git_note_commit_create` | `repository::create_note` |
| `git_note_commit_iterator_new` | **Not Implemented** |
| `git_note_commit_read` | `repository::read_note` |
| `git_note_commit_remove` | `repository::remove_note` |
| `git_note_committer` | `note::committer` |
| `git_note_create` | `repository::create_note` |
| `git_note_default_ref` | `repository::default_notes_reference` |
| `git_note_foreach` | `repository::for_each_note` |
| `git_note_free` | `note::~note` |
| `git_note_id` | `note::id` |
| `git_note_iterator_free` | **Not Implemented** |
| `git_note_iterator_new` | **Not Implemented** |
| `git_note_message` | `note::message` |
| `git_note_next` | **Not Implemented** |
| `git_note_read` | `repository::read_note` |
| `git_note_remove` | `repository::remove_note` |


### object

| libgit2 | cppgit2:: |
| --- | --- |
| `git_object__size` | **Not implemented** |
| `git_object_dup` | `object::copy` |
| `git_object_free` | `object::~object` |
| `git_object_id` | `object::id` |
| `git_object_lookup` | `repository::lookup_object` |
| `git_object_lookup_bypath` | `repository::lookup_object` |
| `git_object_lookup_prefix` | `repository::lookup_object` |
| `git_object_owner` | `object::owner` |
| `git_object_peel` | `object::peel_until` |
| `git_object_short_id` | `object::short_id` |
| `git_object_string2type` | `object::type_from_string` |
| `git_object_type` | `object::type` |
| `git_object_type2string` | `object::string_from_type` |
| `git_object_typeisloose` | `object::is_type_loose` |

### odb

| libgit2 | cppgit2:: |
| --- | --- |
`git_odb_add_alternate` | |
`git_odb_add_backend` | |
`git_odb_add_disk_alternate` | |
| `git_odb_backend_loose` | `odb::create_backend_for_loose_objects` |
| `git_odb_backend_one_pack` | `odb::create_backend_for_one_packfile` |
| `git_odb_backend_pack` | `odb::create_backend_for_packfiles` |
| `git_odb_exists` | `odb::exists` |
| `git_odb_exists_prefix` | `odb::exists` |
`git_odb_expand_ids` | |
`git_odb_foreach` | |
| `git_odb_free` | `odb::~odb` |
| `git_odb_get_backend` | `odb::operator[]` |
`git_odb_hash` | |
`git_odb_hashfile` | |
| `git_odb_new` | `odb::odb` |
| `git_odb_num_backends` | `odb::size` |
`git_odb_object_data` | |
`git_odb_object_dup` | |
`git_odb_object_free` | |
`git_odb_object_id` | |
`git_odb_object_size` | |
`git_odb_object_type` | |
`git_odb_open` | |
`git_odb_open_rstream` | |
`git_odb_open_wstream` | |
`git_odb_read` | |
`git_odb_read_header` | |
`git_odb_read_prefix` | |
`git_odb_refresh` | |
`git_odb_stream_finalize_write` | |
`git_odb_stream_free` | |
`git_odb_stream_read` | |
`git_odb_stream_write` | |
`git_odb_write` | |
`git_odb_write_pack` | |


### oid

| libgit2 | cppgit2:: |
| --- | --- |
| `git_oid_cmp` | `oid::compare` |
| `git_oid_cpy` | `oid::copy` |
| `git_oid_equal` | `oid::operator==` |
| `git_oid_fmt` | **Not implemented** |
| `git_oid_fromraw` | `oid::oid` |
| `git_oid_fromstr` | `oid::oid` |
| `git_oid_fromstrn` | `oid::oid` |
| `git_oid_fromstrp` | **Not implemented** |
| `git_oid_is_zero` | `oid::is_zero` |
| `git_oid_iszero` | `oid::is_zero` |
| `git_oid_ncmp` | `oid::compare` |
| `git_oid_nfmt` | **Not implemented** |
| `git_oid_pathfmt` | `oid::to_path_string` |
| `git_oid_shorten_add` | `oid::shorten::add` |
| `git_oid_shorten_free` | `oid::shorten::~shorten` |
| `git_oid_shorten_new` | `oid::shorten::shorten` |
| `git_oid_strcmp` | `oid::compare` |
| `git_oid_streq` | `oid::operator==` |
| `git_oid_tostr` | `oid::to_hex_string` |
| `git_oid_tostr_s` | `oid::to_hex_string` |


### oidarray

| libgit2 | cppgit2:: |
| --- | --- |
`git_oidarray_free` | **Not Implemented** |


### packbuilder

| libgit2 | cppgit2:: |
| --- | --- |
`git_packbuilder_foreach` | |
| `git_packbuilder_free` | `pack_builder::~pack_builder` |
| `git_packbuilder_hash` | |
`git_packbuilder_insert` | |
| `git_packbuilder_insert_commit` | `pack_builder::insert_commit` |
| `git_packbuilder_insert_recur` | `pack_builder::insert_object_recursively` |
| `git_packbuilder_insert_tree` | `pack_builder::insert_tree` |
`git_packbuilder_insert_walk` | |
`git_packbuilder_new` | |
| `git_packbuilder_object_count` | `pack_builder::size` |
| `git_packbuilder_set_callbacks` | `pack_builder::set_progress_callback` |
| `git_packbuilder_set_threads` | `pack_builder::set_threads` |
`git_packbuilder_write` | |
`git_packbuilder_write_buf` | |
| `git_packbuilder_written` | `pack_builder::written` |


### patch

| libgit2 | cppgit2:: |
| --- | --- |
`git_patch_free` | `patch::~patch` |
`git_patch_from_blob_and_buffer` | `patch::patch` |
`git_patch_from_blobs` | `patch::patch` |
`git_patch_from_buffers` | `patch::patch` |
`git_patch_from_diff` | `patch::patch` |
`git_patch_get_delta` | `patch::delta` |
`git_patch_get_hunk` | |
`git_patch_get_line_in_hunk` | |
`git_patch_line_stats` | `patch::line_stats` |
`git_patch_num_hunks` | `patch::num_hunks` |
`git_patch_num_lines_in_hunk` | `patch::num_lines_in_hunk` |
`git_patch_print` | |
`git_patch_size` | `patch::size` |
`git_patch_to_buf` | `patch::to_buffer` |


### pathspec

| libgit2 | cppgit2:: |
| --- | --- |
`git_pathspec_free` | `pathspec::~pathspec` |
`git_pathspec_match_diff` | `pathspec::match_diff` |
`git_pathspec_match_index` | `pathspec::match_index` |
`git_pathspec_match_list_diff_entry` | `pathspec::match_list::diff_entry` |
`git_pathspec_match_list_entry` | `pathspec::match_list::entry` |
`git_pathspec_match_list_entrycount` | `pathspec::match_list::size` |
`git_pathspec_match_list_failed_entry` | `pathspec::match_list::failed_entry` |
`git_pathspec_match_list_failed_entrycount` | `pathspec::match_list::failed_entrycount` |
`git_pathspec_match_list_free` | `pathspec::match_list::~match_list` |
`git_pathspec_match_tree` | `pathspec::match_free` |
`git_pathspec_match_workdir` | `pathspec::match_workdir` |
`git_pathspec_matches_path` | `pathspec::matches_path` |
`git_pathspec_new` | `pathspec::compile` |


### proxy

| libgit2 | cppgit2:: |
| --- | --- |
| `git_proxy_options_init` | `proxy::options::options` |

### push

| libgit2 | cppgit2:: |
| --- | --- |
| `git_push_options_init` | `push::options::options` |


### rebase

| libgit2 | cppgit2:: |
| --- | --- |
`git_rebase_abort` | `rebase::abort` |
`git_rebase_commit` | `rebase::commit` |
`git_rebase_finish` | `rebase::finish` |
`git_rebase_free` | `rebase::~rebase` |
`git_rebase_init` | `repository::init_rebase` |
`git_rebase_inmemory_index` | `rebase::index` |
`git_rebase_next` | `rebase::next` |
`git_rebase_onto_id` | `rebase::onto_id` |
`git_rebase_onto_name` | `rebase::onto_name` |
`git_rebase_open` | `repository::open_rebase` |
`git_rebase_operation_byindex` | `rebase::operator[]` |
`git_rebase_operation_current` | `rebase::current_operation` |
`git_rebase_operation_entrycount` | `rebase::size` |
`git_rebase_options_init` | `rebase::options::options` |
`git_rebase_orig_head_id` | `rebase::original_head_id` |
`git_rebase_orig_head_name` | `rebase::original_head_name` |


### refdb

| libgit2 | cppgit2:: |
| --- | --- |
`git_refdb_compress` | `refdb::compress` |
`git_refdb_free` | `refdb::~refdb` |
`git_refdb_new` | `repository::create_reference_database` |
`git_refdb_open` | `repository::open_reference_database` |


### reference

| libgit2 | cppgit2:: |
| --- | --- |
| `git_reference_cmp` | `reference::compare` |
| `git_reference_create` | `repository::create_reference` |
| `git_reference_create_matching` | `repository::create_reference` |
| `git_reference_delete` | `reference::delete_reference` |
| `git_reference_dup` | `reference::copy` |
| `git_reference_dwim` | `repository::lookup_reference_by_dwim` |
| `git_reference_ensure_log` | `repository::ensure_reflog_for_reference` |
| `git_reference_foreach` | `repository::for_each_reference` |
| `git_reference_foreach_glob` | `repository::for_each_reference_glob` |
| `git_reference_foreach_name` | `repository::for_each_reference_name` |
| `git_reference_free` | `reference::~reference` |
| `git_reference_has_log` | `repository::reference_has_reflog` |
| `git_reference_is_branch` | `reference::is_branch` |
| `git_reference_is_note` | `reference::is_note` |
| `git_reference_is_remote` | `reference::is_remote` |
| `git_reference_is_tag` | `reference::is_tag` |
| `git_reference_is_valid_name` | `reference::is_valid_name` |
| `git_reference_iterator_free` | `repository::for_each_reference` |
| `git_reference_iterator_glob_new` | `repository::for_each_reference_glob` |
| `git_reference_iterator_new` | `repository::for_each_reference` |
| `git_reference_list` | `repository::reference_list` |
| `git_reference_lookup` | `repository::lookup_reference` |
| `git_reference_name` | `reference::name` |
| `git_reference_name_to_id` | `repository::reference_name_to_id` |
| `git_reference_next` | `repository::for_each_reference` |
| `git_reference_next_name` | `repository::for_each_reference_name` |
| `git_reference_normalize_name` | `reference::normalize_name` |
| `git_reference_owner` | `reference::owner` |
| `git_reference_peel` | reference::peel_until` |
| `git_reference_remove` | `repository::delete_reference` |
| `git_reference_rename` | `reference::rename` |
| `git_reference_resolve` | `reference::resolve` |
| `git_reference_set_target` | `reference::set_target` |
| `git_reference_shorthand` | `reference::shorthand_name` |
| `git_reference_symbolic_create` | `repository::create_symbolic_reference` |
| `git_reference_symbolic_create_matching` | `repository::create_symbolic_reference` |
| `git_reference_symbolic_set_target` | `reference::set_symbolic_target` |
| `git_reference_symbolic_target` | `reference::symbolic_target` |
| `git_reference_target` | `reference::target` |
| `git_reference_target_peel` | `reference::peeled_target` |
| `git_reference_type` | `reference::type` |


### reflog

| libgit2 | cppgit2:: |
| --- | --- |
| `git_reflog_append` | `reflog::append` |
| `git_reflog_delete` | `repository::delete_reflog` |
| `git_reflog_drop` | `reflog::remove` |
| `git_reflog_entry_byindex` | `reflog::operator[]` |
| `git_reflog_entry_committer` | `reflog::entry::committer` |
| `git_reflog_entry_id_new` | `reflog::entry::new_id` |
| `git_reflog_entry_id_old` | `reflog::entry::old_id` |
| `git_reflog_entry_message` | `reflog::entry::message` |
| `git_reflog_entrycount` | `reflog::size` |
| `git_reflog_free` | `reflog::~reflog` |
| `git_reflog_read` | `repository::read_reflog` |
| `git_reflog_rename` | `repository::rename_reflog` |
| `git_reflog_write` | `repository::write_to_disk` |


### refspec

| libgit2 | cppgit2:: |
| --- | --- |
`git_refspec_direction` | `refspec::direction` |
`git_refspec_dst` | `refspec::destination` |
`git_refspec_dst_matches` | `refspec::destination_matches_reference` |
`git_refspec_force` | `refspec::is_force_update_enabled` |
`git_refspec_free` | `refspec::~refspec` |
`git_refspec_parse` | `refspec::parse` |
`git_refspec_rtransform` | `refspec::transform_target_to_source_reference` |
`git_refspec_src` | `refspec::source` |
`git_refspec_src_matches` | `refspec::source_matches_reference` |
`git_refspec_string` | `refspec::to_string` |
`git_refspec_transform` | `refspec::transform_reference` |


### remote

| libgit2 | cppgit2:: |
| --- | --- |
`git_remote_add_fetch` | `repository::add_fetch_refspec_to_remote` |
`git_remote_add_push` | `repository::add_push_refspec_to_remote` |
`git_remote_autotag` | |
`git_remote_connect` | |
`git_remote_connected` | `remote::is_connected` |
`git_remote_create` | `repository::create_remote` |
`git_remote_create_anonymous` | `repository::create_anonymous_remote` |
`git_remote_create_detached` | `remote::create_detached_remote` |
`git_remote_create_options_init` | |
`git_remote_create_with_fetchspec` | `repository::create_remote` |
`git_remote_create_with_opts` | |
`git_remote_default_branch` | `remote::default_branch` |
`git_remote_delete` | `repository::delete_remote` |
`git_remote_disconnect` | `remote::disconnect` |
`git_remote_download` | |
`git_remote_dup` | `remote::copy` |
`git_remote_fetch` | |
`git_remote_free` | `remote::~remote` |
`git_remote_get_fetch_refspecs` | `remote::fetch_refspec` |
`git_remote_get_push_refspecs` | `remote::push_refspec` |
`git_remote_get_refspec` | |
`git_remote_init_callbacks` | |
`git_remote_is_valid_name` | `remote::is_valid_name` |
`git_remote_list` | `repository::remote_list` |
`git_remote_lookup` | `repository::lookup_remote` |
`git_remote_ls` | |
`git_remote_name` | `remote::name` |
`git_remote_owner` | `remote::owner` |
`git_remote_prune` | |
`git_remote_prune_refs` | |
`git_remote_push` | `remote::push` |
`git_remote_pushurl` | `remote::push_url` |
`git_remote_refspec_count` | `remote::size` |
`git_remote_rename` | `repository::rename_remote` |
`git_remote_set_autotag` | |
`git_remote_set_pushurl` | `repository::set_remote_push_url` |
`git_remote_set_url` | `repository::set_remote_url` |
`git_remote_stats` | |
`git_remote_stop` | `remote::stop` |
`git_remote_update_tips` | |
`git_remote_upload` | `remote::upload` |
`git_remote_url` | `remote::url` |


### repository

| libgit2 | cppgit2:: |
| --- | --- |
| `git_repository_commondir` | `repository::commondir` |
| `git_repository_config` | `repository::config` |
| `git_repository_config_snapshot` | `repository::config_snapshot` |
| `git_repository_detach_head` | `repository::detach_head` |
| `git_repository_discover` | `repository::discover_path` |
`git_repository_fetchhead_foreach` | |
| `git_repository_free` | `repository::~repository` |
| `git_repository_get_namespace` | `repository::namespace_` |
| `git_repository_hashfile` | `repository::hashfile` |
| `git_repository_head` | `repository::head` |
| `git_repository_head_detached` | `repository::is_head_detached` |
| `git_repository_head_detached_for_worktree` | `repository::is_head_detached_for_worktree` |
| `git_repository_head_for_worktree` | `repository::head_for_worktree` |
| `git_repository_head_unborn` | `repository::is_head_unborn` |
| `git_repository_ident` | `repository::identity` |
| `git_repository_index` | `repository::index` |
| `git_repository_init` | `repository::init` |
`git_repository_init_ext` | |
`git_repository_init_options_init` | |
| `git_repository_is_bare` | `repository::is_bare` |
| `git_repository_is_empty` | `repository::is_empty` |
| `git_repository_is_shallow` | `repository::is_shallow` |
| `git_repository_is_worktree` | `repository::is_worktree` |
| `git_repository_item_path` | `repository::path` |
`git_repository_mergehead_foreach` | |
| `git_repository_message` | `repository::message` |
| `git_repository_message_remove` | `repository::remove_message` |
`git_repository_odb` | |
| `git_repository_open` | `repository::open` |
| `git_repository_open_bare` | `repository::open_bare` |
`git_repository_open_ext` | |
`git_repository_open_from_worktree` | |
| `git_repository_path` | `repository::path` |
`git_repository_refdb` | |
| `git_repository_set_head` | `repository::set_head` |
| `git_repository_set_head_detached` | `repository::set_head_detached` |
`git_repository_set_head_detached_from_annotated` | |
| `git_repository_set_ident` | `repository::set_identity` |
| `git_repository_set_namespace` | `repository::set_namespace` |
| `git_repository_set_workdir` | `repository::set_workdir` |
| `git_repository_state` | `repository::state` |
| `git_repository_state_cleanup` | `repository::cleanup_state` |
| `git_repository_workdir` | `repository::workdir` |
`git_repository_wrap_odb` | |


### reset

| libgit2 | cppgit2:: |
| --- | --- |
| `git_reset` | `repository::reset` |
| `git_reset_default` | `repository::reset_default` |
| `git_reset_from_annotated` | `repository::reset` |


### revert

| libgit2 | cppgit2:: |
| --- | --- |
`git_revert` | `repository::revert_commit` |
`git_revert_commit` | `repository::revert_commit` |
`git_revert_options_init` | `revert::options::options` |


### revparse

| libgit2 | cppgit2:: |
| --- | --- |
`git_revparse` | `repository::revparse` |
`git_revparse_ext` | `repository::revparse_to_object_and_reference` |
`git_revparse_single` | `repository::revparse_to_object` |


### revwalk

| libgit2 | cppgit2:: |
| --- | --- |
`git_revwalk_add_hide_cb` | `revwalk::add_hide_callback` |
`git_revwalk_free` | `revwalk::~revwalk` |
`git_revwalk_hide` | `revwalk::hide` |
`git_revwalk_hide_glob` | `revwalk::hide_glob` |
`git_revwalk_hide_head` | `revwalk::hide_head` |
`git_revwalk_hide_ref` | `revwalk::hide_reference` |
`git_revwalk_new` | `repository::create_revwalk` |
`git_revwalk_next` | `revwalk::next` |
`git_revwalk_push` | `revwalk::push` |
`git_revwalk_push_glob` | `revwalk::push_glob` |
`git_revwalk_push_head` | `revwalk::push_head` |
`git_revwalk_push_range` | `revwalk::push_range` |
`git_revwalk_push_ref` | `revwalk::push_reference` |
`git_revwalk_repository` | `revwalk::repository` |
`git_revwalk_reset` | `revwalk::reset` |
`git_revwalk_simplify_first_parent` | `revwalk::simplify_first_parent` |
`git_revwalk_sorting` | `revwalk::set_sorting_mode` |

### signature

| libgit2 | cppgit2:: |
| --- | --- |
| `git_signature_default` | `repository::default_signature` |
| `git_signature_dup` | `signature::copy` |
| `git_signature_free` | `signature::~signature` |
| `git_signature_from_buffer` | `signature::signature` |
| `git_signature_new` | `signature::signature` |
| `git_signature_now` | `signature::signature` |


### stash

| libgit2 | cppgit2:: |
| --- | --- |
| `git_stash_apply` | `repository::apply_stash` |
| `git_stash_apply_options_init` | `stash::options::options` |
| `git_stash_drop` | `repository::drop_stash` |
| `git_stash_foreach` | `repository::for_each_stash` |
| `git_stash_pop` | `repository::pop_stash` |
| `git_stash_save` | `repository::save_stash` |


### status

| libgit2 | cppgit2:: |
| --- | --- |
| `git_status_byindex` | `status::list::operator[]` |
| `git_status_file` | `repository::status_file` |
| `git_status_foreach` | `repository::for_each_status` |
| `git_status_foreach_ext` | `repository::for_each_status` |
| `git_status_list_entrycount` | `status::list::size` |
| `git_status_list_free` | `status::list::~list` |
| `git_status_list_new` | `repository::status_list` |
| `git_status_options_init` | `status::options::options` |
| `git_status_should_ignore` | `repository::should_ignore` |


### strarray

| libgit2 | cppgit2:: |
| --- | --- |
| `git_strarray_copy` | `strarray::copy` |
| `git_strarray_free` | `strarray::~strarray` |


### submodule

| libgit2 | cppgit2:: |
| --- | --- |
| `git_submodule_add_finalize` | `submodule::resolve_setup` |
| `git_submodule_add_setup` | `repository::setup_submodule` |
| `git_submodule_add_to_index` | `submodule::add_to_index` |
| `git_submodule_branch` | `submodule::branch_name` |
`git_submodule_clone` | |
| `git_submodule_fetch_recurse_submodules` | `submodule::recuse_submodules_option` |
| `git_submodule_foreach` | `repository::for_each_submodule` |
| `git_submodule_free` | `submodule::~submodule` |
| `git_submodule_head_id` | `submodule::head_id` |
| `git_submodule_ignore` | `submodule::ignore_option` |
| `git_submodule_index_id` | `submodule::index_id` |
| `git_submodule_init` | `submodule::init` |
`git_submodule_location` | |
| `git_submodule_lookup` | `submodule::lookup_submodule` |
| `git_submodule_name` | `submodule::name` |
| `git_submodule_open` | `submodule::open_repository` |
| `git_submodule_owner` | `submodule::owner` |
| `git_submodule_path` | `submodule::path` |
| `git_submodule_reload` | `submodule::reload` |
`git_submodule_repo_init` | |
| `git_submodule_resolve_url` | `repository::resolve_submodule_url` |
| `git_submodule_set_branch` | `repository::set_submodule_branch` |
| `git_submodule_set_fetch_recurse_submodules` | `repository::set_submodule_fetch_recurse_option` |
| `git_submodule_set_ignore` | `repository::set_submodule_ignore_option` |
| `git_submodule_set_update` | `repository::set_submodule_update_option` |
| `git_submodule_set_url` | `repository::set_submodule_url` |
| `git_submodule_status` | `repository::submodule_status` |
| `git_submodule_sync` | `submodule::sync` |
`git_submodule_update` | |
`git_submodule_update_options_init` | |
| `git_submodule_update_strategy` | `submodule::update_strategy` |
| `git_submodule_url` | `submodule::url` |
| `git_submodule_wd_id` | **Not implemented** |

### tag

| libgit2 | cppgit2:: |
| --- | --- |
| `git_tag_annotation_create` | `repository::create_tag_annotation` |
| `git_tag_create` | `repository::create_tag` |
| `git_tag_create_from_buffer` | `repository::create_tag` |
| `git_tag_create_lightweight` | `repository::create_lightweight_tag` |
| `git_tag_delete` | `repository::delete_tag` |
| `git_tag_dup` | `tag::copy` |
| `git_tag_foreach` | `repository::for_each_tag` |
| `git_tag_free` | `tag::~tag` |
| `git_tag_id` | `tag::id` |
| `git_tag_list` | `repository::tags` |
| `git_tag_list_match` | `repository::tags_that_match` |
| `git_tag_lookup` | `repository::lookup_tag` |
| `git_tag_lookup_prefix` | `repository::lookup_tag` |
| `git_tag_message` | `tag::message` |
| `git_tag_name` | `tag::name` |
| `git_tag_owner` | `tag::owner` |
| `git_tag_peel` | `tag::peel` |
| `git_tag_tagger` | `tag::tagger` |
| `git_tag_target` | `tag::target` |
| `git_tag_target_id` | `tag::target_id` |
| `git_tag_target_type` | `tag::target_type` |

### trace

| libgit2 | cppgit2:: |
| --- | --- |
| `git_trace_set` | **Not implemented** |

### transaction

| libgit2 | cppgit2:: |
| --- | --- |
| `git_transaction_commit` | `transaction::commit` |
| `git_transaction_free` | `transaction::~transaction` |
| `git_transaction_lock_ref` | `transaction::lock_reference` |
| `git_transaction_new` | `repository::create_transaction` |
| `git_transaction_remove` | `transaction::remove_reference` |
| `git_transaction_set_reflog` | `transaction::set_reflog` |
| `git_transaction_set_symbolic_target` | `transaction::set_symbolic_target` |
| `git_transaction_set_target` | `transaction::set_target` |

### tree

| libgit2 | cppgit2:: |
| --- | --- |
| `git_tree_create_updated` | `repository::create_updated_tree` |
| `git_tree_dup` | `tree::copy` |
| `git_tree_entry_byid` | `tree::lookup_entry_by_id` |
| `git_tree_entry_byindex` | `tree::lookup_entry_by_index` |
| `git_tree_entry_byname` | `tree::lookup_entry_by_name` |
| `git_tree_entry_bypath` | `tree::lookup_entry_by_path` |
| `git_tree_entry_cmp` | `tree::entry::compare` |
| `git_tree_entry_dup` | `tree::entry::copy` |
| `git_tree_entry_filemode` | `tree::entry::filemode` |
| `git_tree_entry_filemode_raw` | `tree::entry::raw_filemode` |
| `git_tree_entry_free` | `tree::entry::~entry` |
| `git_tree_entry_id` | `tree::entry::id` |
| `git_tree_entry_name` | `tree::entry::filename` |
| `git_tree_entry_to_object` | `repository::tree_entry_to_object` |
| `git_tree_entry_type` | `tree::entry::type` |
| `git_tree_entrycount` | `tree::size` |
| `git_tree_free` | `tree::~tree` |
| `git_tree_id` | `tree::id` |
| `git_tree_lookup` | `repository::lookup_tree` |
| `git_tree_lookup_prefix` | `repository::lookup_tree` |
| `git_tree_owner` | `tree::owner` |
| `git_tree_walk` | `tree::walk` |


### treebuilder

| libgit2 | cppgit2:: |
| --- | --- |
| `git_treebuilder_clear` | `tree_builder::clear` |
| `git_treebuilder_entrycount` | `tree_builder::size` |
| `git_treebuilder_filter` | `tree_builder::filter` |
| `git_treebuilder_free` | `tree_builder::~tree_builder` |
| `git_treebuilder_get` | `tree_builder::operator[]` |
| `git_treebuilder_insert` | `tree_builder::insert` |
| `git_treebuilder_new` | `tree_builder::tree_builder` |
| `git_treebuilder_remove` | `tree_builder::remove` |
| `git_treebuilder_write` | `tree_builder::write` |
| `git_treebuilder_write_with_buffer` | `tree_builder::write` |

### worktree

| libgit2 | cppgit2:: |
| --- | --- |
| `git_worktree_add` | `repository::add_worktree` |
| `git_worktree_add_options_init` | `worktree::add_options::add_options` |
| `git_worktree_free` | `worktree::~worktree` |
| `git_worktree_is_locked` | `worktree::is_prunable` |
| `git_worktree_is_prunable` | `worktree::is_prunable` |
| `git_worktree_list` | `repository::list_worktrees` |
| `git_worktree_lock` | `worktree::lock` |
| `git_worktree_lookup` | `repository::lookup_worktree` |
| `git_worktree_name` | `worktree::name` |
| `git_worktree_open_from_repository` | `repository::open_worktree` |
| `git_worktree_path` | `worktree::path` |
| `git_worktree_prune` | `worktree::prune` |
| `git_worktree_prune_options_init` | `worktree::prune_options::prune_options` |
| `git_worktree_unlock` | `worktree::unlock` |
| `git_worktree_validate` | `worktree::validate` |
