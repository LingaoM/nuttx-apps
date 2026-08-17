/****************************************************************************
 * apps/examples/loadelf/samples/loadelf_cpp_stress/loadelf_cpp_stress.cxx
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ****************************************************************************/

#include <algorithm>
#include <any>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <exception>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <list>
#include <locale>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <typeinfo>
#include <unordered_map>
#include <variant>
#include <vector>

#include <unistd.h>

namespace
{
bool g_constructed = false;
bool g_destructed = false;

class GlobalProbe
{
public:
  GlobalProbe()
  {
    g_constructed = true;
  }

  ~GlobalProbe()
  {
    g_destructed = true;
    std::printf("loadelf_cpp_stress: global destructor ran\n");
  }
};

class Shape
{
public:
  virtual ~Shape() = default;
  virtual int Area() const = 0;
};

class Rectangle final : public Shape
{
public:
  Rectangle(int width, int height) : m_width(width), m_height(height)
  {
  }

  int Area() const override
  {
    return m_width * m_height;
  }

private:
  int m_width;
  int m_height;
};

class CleanupCounter
{
public:
  explicit CleanupCounter(int &counter) : m_counter(counter)
  {
  }

  ~CleanupCounter()
  {
    m_counter++;
  }

private:
  int &m_counter;
};

GlobalProbe g_global_probe;

int g_failures = 0;

bool Check(bool condition, const char *name)
{
  std::printf("loadelf_cpp_stress: %-28s %s\n",
              name, condition ? "PASS" : "FAIL");
  if (!condition)
    {
      g_failures++;
    }

  return condition;
}

void TestContainers()
{
  std::vector<int> values = {9, 4, 1, 7, 4};
  std::deque<int> queue;
  std::list<std::string> words = {"load", "elf", "cpp"};
  std::set<int> unique;
  std::map<std::string, int> named;
  std::unordered_map<std::string, int> hashed;

  std::sort(values.begin(), values.end());
  queue.assign(values.begin(), values.end());
  unique.insert(values.begin(), values.end());
  named.emplace("front", values.front());
  named.emplace("back", values.back());
  hashed.emplace("sum", std::accumulate(values.begin(), values.end(), 0));

  words.remove("elf");
  words.push_back("stress");

  Check(values == std::vector<int>({1, 4, 4, 7, 9}), "vector/sort");
  Check(queue.front() == 1 && queue.back() == 9, "deque");
  Check(unique.size() == 4 && unique.count(4) == 1, "set");
  Check(named["front"] == 1 && named["back"] == 9, "map");
  Check(hashed["sum"] == 25, "unordered_map");
  Check(words.front() == "load" && words.back() == "stress", "list");
}

void TestModernLibrary()
{
  std::array<int, 4> array = {1, 2, 3, 4};
  std::optional<std::string> optional = std::string("nuttx");
  std::variant<int, std::string> variant = std::string("loadelf");
  std::any any_value = 42;
  std::tuple<int, std::string, double> tuple(7, "cpp", 1.5);
  std::string_view view("loadable");

  auto square = [](int value) { return value * value; };
  std::transform(array.begin(), array.end(), array.begin(), square);

  Check(array[0] == 1 && array[3] == 16, "array/algorithm");
  Check(optional.has_value() && optional->size() == 5, "optional");
  Check(std::get<std::string>(variant) == "loadelf", "variant");
  Check(std::any_cast<int>(any_value) == 42, "any");
  Check(std::get<0>(tuple) == 7 && std::get<1>(tuple) == "cpp", "tuple");
  Check(view.substr(0, 5) == "loada", "string_view");
}

void TestSmartPointersAndRtti()
{
  std::unique_ptr<Shape> unique(new Rectangle(6, 7));
  std::shared_ptr<Shape> shared = std::make_shared<Rectangle>(3, 5);
  std::weak_ptr<Shape> weak = shared;
  Shape *base = unique.get();
  Rectangle *rect = dynamic_cast<Rectangle *>(base);

  Check(unique->Area() == 42, "unique_ptr/virtual");
  Check(shared->Area() == 15 && !weak.expired(), "shared_ptr/weak_ptr");
  Check(rect != nullptr && rect->Area() == 42, "dynamic_cast");
  Check(std::string(typeid(*base).name()).size() > 0, "typeid");
}

void ThrowNested(int &cleanup)
{
  CleanupCounter outer(cleanup);
  {
    CleanupCounter inner(cleanup);
    throw std::runtime_error("nested runtime_error from C++ stress ELF");
  }
}

void TestExceptions()
{
  int cleanup = 0;
  std::exception_ptr captured;
  bool caught_runtime = false;
  bool rethrown_logic = false;

  try
    {
      ThrowNested(cleanup);
    }
  catch (const std::runtime_error &error)
    {
      caught_runtime = std::string(error.what()).find("runtime_error") !=
                       std::string::npos;
      captured = std::current_exception();
    }

  try
    {
      if (captured)
        {
          std::rethrow_exception(captured);
        }
    }
  catch (const std::logic_error &)
    {
      rethrown_logic = false;
    }
  catch (const std::runtime_error &)
    {
      rethrown_logic = true;
    }

  Check(caught_runtime, "throw/catch runtime_error");
  Check(cleanup == 2, "exception stack unwind");
  Check(rethrown_logic, "exception_ptr/rethrow");
}

void TestIostreamLocaleRegex()
{
  std::ostringstream stream;
  std::regex pattern("([a-z]+)-([0-9]+)");
  std::string text("loadelf-2026");
  std::smatch match;
  std::locale loc = std::locale::classic();
  const std::ctype<char> &ctype = std::use_facet<std::ctype<char>>(loc);

  stream << "value=" << 123 << " text=" << std::string("ok");
  std::cout << "loadelf_cpp_stress: cout says " << stream.str() << std::endl;

  bool matched = std::regex_match(text, match, pattern);
  bool locale_ok = ctype.toupper('a') == 'A' &&
                   std::toupper('b', loc) == 'B';

  Check(stream.str() == "value=123 text=ok", "ostringstream");
  Check(matched && match.size() == 3 && match[1] == "loadelf",
        "regex");
  Check(locale_ok, "locale");
}

void TestFilesystemAndFstream()
{
  namespace fs = std::filesystem;

  std::error_code ec;
  fs::path self = fs::path("/system") / "bin" / "loadelf_cpp_stress";
  bool exists = fs::exists(self, ec);
  bool regular = exists && fs::is_regular_file(self, ec);
  std::uintmax_t size = regular ? fs::file_size(self, ec) : 0;

  std::ifstream file(self, std::ios::binary);
  std::array<char, 4> magic = {};
  if (file.good())
    {
      file.read(magic.data(), magic.size());
    }

  Check(exists && !ec, "filesystem exists");
  Check(regular && size > 0, "filesystem file_size");
  Check(magic[0] == '\x7f' && magic[1] == 'E' &&
        magic[2] == 'L' && magic[3] == 'F', "ifstream");
}

void TestChronoAndThreads()
{
  std::mutex mutex;
  std::condition_variable cv;
  std::vector<int> values;
  std::atomic<int> total(0);
  bool ready = false;

  auto start = std::chrono::steady_clock::now();

  std::thread worker([&]()
    {
      std::unique_lock<std::mutex> lock(mutex);
      cv.wait(lock, [&]() { return ready; });
      total.store(std::accumulate(values.begin(), values.end(), 0));
    });

  {
    std::lock_guard<std::mutex> lock(mutex);
    values = {5, 6, 7};
    ready = true;
  }

  cv.notify_one();
  worker.join();

  auto finish = std::chrono::steady_clock::now();
  auto elapsed = finish - start;

  Check(total.load() == 18, "thread/condition_variable");
  Check(elapsed >= std::chrono::steady_clock::duration::zero(), "chrono");
}

void TestFuture()
{
  std::promise<int> promise;
  std::future<int> promised = promise.get_future();
  auto async_value = std::async(std::launch::async, []()
    {
      return 42;
    });

  promise.set_value(11);

  Check(promised.get() == 11, "promise/future");
  Check(async_value.get() == 42, "async");
}
}

extern "C" int main(int argc, char *argv[])
{
  std::printf("loadelf_cpp_stress: loadable C++ stress ELF loaded\n");
  std::printf("loadelf_cpp_stress: pid=%d argc=%d constructed=%s "
              "destructed=%s\n",
              static_cast<int>(getpid()), argc,
              g_constructed ? "yes" : "no",
              g_destructed ? "yes" : "no");

  for (int i = 0; i < argc; i++)
    {
      std::printf("loadelf_cpp_stress: argv[%d]=%s\n", i, argv[i]);
    }

  Check(g_constructed && !g_destructed, "global constructor");

  TestContainers();
  TestModernLibrary();
  TestSmartPointersAndRtti();
  TestExceptions();
  TestIostreamLocaleRegex();
  TestFilesystemAndFstream();
  TestChronoAndThreads();
  TestFuture();

  std::printf("loadelf_cpp_stress: failures=%d\n", g_failures);
  return g_failures == 0 ? 0 : 1;
}
