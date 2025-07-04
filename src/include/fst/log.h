// Copyright 2005-2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the 'License');
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an 'AS IS' BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// See www.openfst.org for extensive documentation on this weighted
// finite-state transducer library.
//
// Google-style logging declarations and inline definitions.

#ifndef FST_LOG_H_
#define FST_LOG_H_

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <ostream>
#include <string_view>

#include <fst/flags.h>

class LogMessage;
class LogMessage;

DECLARE_int32(v);

class LogMessage {
 public:
  explicit LogMessage(std::string_view type) : fatal_(type == "FATAL") {
    std::cerr << type << ": ";
  }

  ~LogMessage() {
    std::cerr << std::endl;
    if (fatal_) exit(1);
  }

  std::ostream &stream() { return std::cerr; }

 private:
  bool fatal_;
};

#define LOG(type) LogMessage(#type).stream()
#define VLOG(level) if ((level) <= FST_FLAGS_v) LOG(INFO)

// Checks.
inline void FstCheck(bool x, std::string_view expr, std::string_view file,
                     int line) {
  if (!x) {
    LOG(FATAL) << "Check failed: \"" << expr << "\" file: " << file
               << " line: " << line;
  }
}

#define FST_CHECK(x) FstCheck(static_cast<bool>(x), #x, __FILE__, __LINE__)
#define FST_CHECK_EQ(x, y) FST_CHECK((x) == (y))
#define FST_CHECK_LT(x, y) FST_CHECK((x) < (y))
#define FST_CHECK_GT(x, y) FST_CHECK((x) > (y))
#define FST_CHECK_LE(x, y) FST_CHECK((x) <= (y))
#define FST_CHECK_GE(x, y) FST_CHECK((x) >= (y))
#define FST_CHECK_NE(x, y) FST_CHECK((x) != (y))

// Debug checks.
#define DFST_CHECK(x) assert(x)
#define DFST_CHECK_EQ(x, y) DFST_CHECK((x) == (y))
#define DFST_CHECK_LT(x, y) DFST_CHECK((x) < (y))
#define DFST_CHECK_GT(x, y) DFST_CHECK((x) > (y))
#define DFST_CHECK_LE(x, y) DFST_CHECK((x) <= (y))
#define DFST_CHECK_GE(x, y) DFST_CHECK((x) >= (y))
#define DFST_CHECK_NE(x, y) DFST_CHECK((x) != (y))

#endif  // FST_LOG_H_
