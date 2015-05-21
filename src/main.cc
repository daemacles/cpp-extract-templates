#include <string>
#include <vector>

#include "myclass.hpp"

#include "extern_templates.hpp"

template <typename T, typename D>
class TemplateClass {
public:
  TemplateClass () : value_(0.0) {}
  TemplateClass (const T& v) : value_(v) {}
  const T& GetValue (void) { return value_; }
  T value_;
};

class RegularClass {
};

template class TemplateClass<int, class jim::MyClass<float>>;

int main(int argc, char **argv) {
  // DELETE THESE.  Used to suppress unused variable warnings.
  (void)argc;
  (void)argv;

  double d = 10;
  double c = add(d, d);
  d = c;

  RegularClass reg;
  (void) reg;

  TemplateClass<int, jim::MyClass<float>> ti;
  ti.GetValue();

  jim::MyClass<char> mc;

  using namespace jim;
  Baseclass *bc = new Subclass1<float>();
  bc->value0_ = 40;

  if (argc > 2) {
    mc.value_ = 40;
  }

  typedef std::vector<int> IVec;
  IVec ivec;
  ivec.push_back(10);

  //std::string msg = "hello";

  return 0;
}
